/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nolphin-desktop-link-monitor.c: singleton that manages the links

   Copyright (C) 2003 Red Hat, Inc.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
   Boston, MA 02110-1335, USA.

   Author: Alexander Larsson <alexl@redhat.com>
*/

#include <config.h>
#include "nolphin-desktop-link-monitor.h"
#include "nolphin-desktop-metadata.h"
#include "nolphin-desktop-link.h"
#include "nolphin-desktop-icon-file.h"
#include "nolphin-directory.h"
#include "nolphin-desktop-directory.h"
#include "nolphin-global-preferences.h"

#include <eel/eel-debug.h>
#include <eel/eel-vfs-extensions.h>
#include <eel/eel-stock-dialogs.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <libnolphin-private/nolphin-trash-monitor.h>
#include <string.h>

struct NolphinDesktopLinkMonitorDetails {
	GVolumeMonitor *volume_monitor;
	NolphinDirectory *desktop_dir;

	NolphinDesktopLink *home_link;
	NolphinDesktopLink *computer_link;
	NolphinDesktopLink *trash_link;
	NolphinDesktopLink *network_link;

	GList *mount_links;
};

G_DEFINE_TYPE (NolphinDesktopLinkMonitor, nolphin_desktop_link_monitor, G_TYPE_OBJECT);

static NolphinDesktopLinkMonitor *the_link_monitor = NULL;

static void
destroy_desktop_link_monitor (void)
{
	if (the_link_monitor != NULL) {
		g_object_unref (the_link_monitor);
	}
}

NolphinDesktopLinkMonitor *
nolphin_desktop_link_monitor_get (void)
{
	if (the_link_monitor == NULL) {
		g_object_new (NOLPHIN_TYPE_DESKTOP_LINK_MONITOR, NULL);
		eel_debug_call_at_shutdown (destroy_desktop_link_monitor);
	}
	return the_link_monitor;
}

static void
volume_delete_dialog (GtkWidget *parent_view,
                      NolphinDesktopLink *link)
{
	GMount *mount;
	char *dialog_str;
	char *display_name;

	mount = nolphin_desktop_link_get_mount (link);

	if (mount != NULL) {
		display_name = nolphin_desktop_link_get_display_name (link);
		dialog_str = g_strdup_printf (_("Sie können den Datenträger »%s« nicht in den Papierkorb verschieben."),
					      display_name);
		g_free (display_name);

		if (g_mount_can_eject (mount)) {
			eel_run_simple_dialog
				(parent_view,
				 FALSE,
				 GTK_MESSAGE_ERROR,
				 dialog_str,
				 _("Wenn Sie den Datenträger auswerfen möchten, verwenden Sie bitte »Auswerfen« im Kontextmenü des Datenträgers."),
				 GTK_STOCK_OK, NULL);
		} else {
			eel_run_simple_dialog
				(parent_view,
				 FALSE,
				 GTK_MESSAGE_ERROR,
				 dialog_str,
				 _("Zum Aushängen des Datenträgers bitte »Aushängen« im Kontextmenü des Datenträgers verwenden."),
				 GTK_STOCK_OK, NULL);
		}

		g_object_unref (mount);
		g_free (dialog_str);
	}
}

void
nolphin_desktop_link_monitor_delete_link (NolphinDesktopLinkMonitor *monitor,
					   NolphinDesktopLink *link,
					   GtkWidget *parent_view)
{
	switch (nolphin_desktop_link_get_link_type (link)) {
	case NOLPHIN_DESKTOP_LINK_HOME:
	case NOLPHIN_DESKTOP_LINK_COMPUTER:
	case NOLPHIN_DESKTOP_LINK_TRASH:
	case NOLPHIN_DESKTOP_LINK_NETWORK:
		/* just ignore. We don't allow you to delete these */
		break;
        case NOLPHIN_DESKTOP_LINK_MOUNT:
	default:
		volume_delete_dialog (parent_view, link);
		break;
	}
}

static gboolean
volume_file_name_used (NolphinDesktopLinkMonitor *monitor,
		       const char *name)
{
	GList *l;
	char *other_name;
	gboolean same;

	for (l = monitor->details->mount_links; l != NULL; l = l->next) {
		other_name = nolphin_desktop_link_get_file_name (l->data);
		same = strcmp (name, other_name) == 0;
		g_free (other_name);

		if (same) {
			return TRUE;
		}
	}

	return FALSE;
}

char *
nolphin_desktop_link_monitor_make_filename_unique (NolphinDesktopLinkMonitor *monitor,
						    const char *filename)
{
	char *unique_name;
	int i;

	i = 2;
	unique_name = g_strdup (filename);
	while (volume_file_name_used (monitor, unique_name)) {
		g_free (unique_name);
		unique_name = g_strdup_printf ("%s.%d", filename, i++);
	}
	return unique_name;
}

static gboolean
has_mount (NolphinDesktopLinkMonitor *monitor,
	   GMount                     *mount)
{
	gboolean ret;
	GMount *other_mount;
	GList *l;

	ret = FALSE;

	for (l = monitor->details->mount_links; l != NULL; l = l->next) {
		other_mount = nolphin_desktop_link_get_mount (l->data);
		if (mount == other_mount) {
			g_object_unref (other_mount);
			ret = TRUE;
			break;
		}
		g_object_unref (other_mount);
	}

	return ret;
}

static void
create_mount_link (NolphinDesktopLinkMonitor *monitor,
		   GMount *mount)
{
	NolphinDesktopLink *link;

	if (has_mount (monitor, mount))
		return;

	if ((!g_mount_is_shadowed (mount)) &&
	    g_settings_get_boolean (nolphin_desktop_preferences,
				    NOLPHIN_PREFERENCES_DESKTOP_VOLUMES_VISIBLE)) {
		link = nolphin_desktop_link_new_from_mount (mount);
		monitor->details->mount_links = g_list_prepend (monitor->details->mount_links, link);
	}
}

static void
remove_mount_link (NolphinDesktopLinkMonitor *monitor,
		   GMount *mount)
{
	GList *l;
	NolphinDesktopLink *link;
	GMount *other_mount;

	link = NULL;
	for (l = monitor->details->mount_links; l != NULL; l = l->next) {
		other_mount = nolphin_desktop_link_get_mount (l->data);
		if (mount == other_mount) {
			g_object_unref (other_mount);
			link = l->data;
			break;
		}
		g_object_unref (other_mount);
	}

	if (link) {
		monitor->details->mount_links = g_list_remove (monitor->details->mount_links, link);
		g_object_unref (link);
	}
}



static void
mount_added_callback (GVolumeMonitor *volume_monitor,
		      GMount *mount,
		      NolphinDesktopLinkMonitor *monitor)
{
	create_mount_link (monitor, mount);
}


static void
mount_removed_callback (GVolumeMonitor *volume_monitor,
			GMount *mount,
			NolphinDesktopLinkMonitor *monitor)
{
	remove_mount_link (monitor, mount);
}

static void
mount_changed_callback (GVolumeMonitor *volume_monitor,
			GMount *mount,
			NolphinDesktopLinkMonitor *monitor)
{
	/* TODO: update the mount with other details */

	/* remove a mount if it goes into the shadows */
	if (g_mount_is_shadowed (mount) && has_mount (monitor, mount)) {
		remove_mount_link (monitor, mount);
	}}

static void
update_link_visibility (NolphinDesktopLinkMonitor *monitor,
			NolphinDesktopLink       **link_ref,
			NolphinDesktopLinkType     link_type,
			const char                 *preference_key)
{
	if (g_settings_get_boolean (nolphin_desktop_preferences, preference_key)) {
		if (*link_ref == NULL) {
			*link_ref = nolphin_desktop_link_new (link_type);
		}
	} else {
		if (*link_ref != NULL) {
            /* If this were a real file, removing (deleting or moving) it would
             * also remove its metadata, though for a different reason, and
             * unmanaged by us.  We have to simulate that when removing a fake
             * 'desktop' file, so if it gets added again later, it behaves like a
             * 'new' file.
             */
            nolphin_desktop_clear_metadata (nolphin_desktop_link_get_file (*link_ref));

			g_object_unref (*link_ref);
			*link_ref = NULL;
		}
	}
}

static void
desktop_home_visible_changed (gpointer callback_data)
{
	NolphinDesktopLinkMonitor *monitor;

	monitor = NOLPHIN_DESKTOP_LINK_MONITOR (callback_data);

	update_link_visibility (NOLPHIN_DESKTOP_LINK_MONITOR (monitor),
				&monitor->details->home_link,
				NOLPHIN_DESKTOP_LINK_HOME,
				NOLPHIN_PREFERENCES_DESKTOP_HOME_VISIBLE);
}

static void
desktop_computer_visible_changed (gpointer callback_data)
{
	NolphinDesktopLinkMonitor *monitor;

	monitor = NOLPHIN_DESKTOP_LINK_MONITOR (callback_data);

	update_link_visibility (NOLPHIN_DESKTOP_LINK_MONITOR (callback_data),
				&monitor->details->computer_link,
				NOLPHIN_DESKTOP_LINK_COMPUTER,
				NOLPHIN_PREFERENCES_DESKTOP_COMPUTER_VISIBLE);
}

static void
desktop_trash_visible_changed (gpointer callback_data)
{
	NolphinDesktopLinkMonitor *monitor;

	monitor = NOLPHIN_DESKTOP_LINK_MONITOR (callback_data);

	update_link_visibility (NOLPHIN_DESKTOP_LINK_MONITOR (callback_data),
				&monitor->details->trash_link,
				NOLPHIN_DESKTOP_LINK_TRASH,
				NOLPHIN_PREFERENCES_DESKTOP_TRASH_VISIBLE);
}

static void
desktop_network_visible_changed (gpointer callback_data)
{
	NolphinDesktopLinkMonitor *monitor;

	monitor = NOLPHIN_DESKTOP_LINK_MONITOR (callback_data);

	update_link_visibility (NOLPHIN_DESKTOP_LINK_MONITOR (callback_data),
				&monitor->details->network_link,
				NOLPHIN_DESKTOP_LINK_NETWORK,
				NOLPHIN_PREFERENCES_DESKTOP_NETWORK_VISIBLE);
}

static void
desktop_volumes_visible_changed (gpointer callback_data)
{
	NolphinDesktopLinkMonitor *monitor;
	GList *l, *mounts;

	monitor = NOLPHIN_DESKTOP_LINK_MONITOR (callback_data);

	if (g_settings_get_boolean (nolphin_desktop_preferences,
				    NOLPHIN_PREFERENCES_DESKTOP_VOLUMES_VISIBLE)) {
		if (monitor->details->mount_links == NULL) {
			mounts = g_volume_monitor_get_mounts (monitor->details->volume_monitor);
			for (l = mounts; l != NULL; l = l->next) {
				create_mount_link (monitor, l->data);
				g_object_unref (l->data);
			}
			g_list_free (mounts);
		}
	} else {
		g_list_foreach (monitor->details->mount_links, (GFunc)g_object_unref, NULL);
		g_list_free (monitor->details->mount_links);
		monitor->details->mount_links = NULL;
	}
}

static void
create_link_and_add_preference (NolphinDesktopLink   **link_ref,
				NolphinDesktopLinkType link_type,
				const char             *preference_key,
				GCallback               callback,
				gpointer                callback_data)
{
	char *detailed_signal;

	if (g_settings_get_boolean (nolphin_desktop_preferences, preference_key)) {
		*link_ref = nolphin_desktop_link_new (link_type);
	}

	detailed_signal = g_strconcat ("changed::", preference_key, NULL);
	g_signal_connect_swapped (nolphin_desktop_preferences,
				  detailed_signal,
				  callback, callback_data);

	g_free (detailed_signal);
}

static void
nolphin_desktop_link_monitor_init (NolphinDesktopLinkMonitor *monitor)
{
	GList *l, *mounts;
	GMount *mount;

	monitor->details = G_TYPE_INSTANCE_GET_PRIVATE (monitor, NOLPHIN_TYPE_DESKTOP_LINK_MONITOR,
							NolphinDesktopLinkMonitorDetails);

	the_link_monitor = monitor;
	monitor->details->volume_monitor = g_volume_monitor_get ();

	/* We keep around a ref to the desktop dir */
	monitor->details->desktop_dir = nolphin_directory_get_by_uri (EEL_DESKTOP_URI);

	/* Default links */

	create_link_and_add_preference (&monitor->details->home_link,
					NOLPHIN_DESKTOP_LINK_HOME,
					NOLPHIN_PREFERENCES_DESKTOP_HOME_VISIBLE,
					G_CALLBACK (desktop_home_visible_changed),
					monitor);

	create_link_and_add_preference (&monitor->details->computer_link,
					NOLPHIN_DESKTOP_LINK_COMPUTER,
					NOLPHIN_PREFERENCES_DESKTOP_COMPUTER_VISIBLE,
					G_CALLBACK (desktop_computer_visible_changed),
					monitor);

	create_link_and_add_preference (&monitor->details->trash_link,
					NOLPHIN_DESKTOP_LINK_TRASH,
					NOLPHIN_PREFERENCES_DESKTOP_TRASH_VISIBLE,
					G_CALLBACK (desktop_trash_visible_changed),
					monitor);

	create_link_and_add_preference (&monitor->details->network_link,
					NOLPHIN_DESKTOP_LINK_NETWORK,
					NOLPHIN_PREFERENCES_DESKTOP_NETWORK_VISIBLE,
					G_CALLBACK (desktop_network_visible_changed),
					monitor);

	/* Mount links */

	mounts = g_volume_monitor_get_mounts (monitor->details->volume_monitor);
	for (l = mounts; l != NULL; l = l->next) {
		mount = l->data;
		create_mount_link (monitor, mount);
		g_object_unref (mount);
	}
	g_list_free (mounts);

	g_signal_connect_swapped (nolphin_desktop_preferences,
				  "changed::" NOLPHIN_PREFERENCES_DESKTOP_VOLUMES_VISIBLE,
				  G_CALLBACK (desktop_volumes_visible_changed),
				  monitor);

	g_signal_connect_object (monitor->details->volume_monitor, "mount_added",
				 G_CALLBACK (mount_added_callback), monitor, 0);
	g_signal_connect_object (monitor->details->volume_monitor, "mount_removed",
				 G_CALLBACK (mount_removed_callback), monitor, 0);
	g_signal_connect_object (monitor->details->volume_monitor, "mount_changed",
				 G_CALLBACK (mount_changed_callback), monitor, 0);

}

static void
remove_link_and_preference (NolphinDesktopLink   **link_ref,
			    const char             *preference_key,
			    GCallback               callback,
			    gpointer                callback_data)
{
	if (*link_ref != NULL) {
		g_object_unref (*link_ref);
		*link_ref = NULL;
	}

	g_signal_handlers_disconnect_by_func (nolphin_desktop_preferences,
					      callback, callback_data);
}

static void
desktop_link_monitor_finalize (GObject *object)
{
	NolphinDesktopLinkMonitor *monitor;

	monitor = NOLPHIN_DESKTOP_LINK_MONITOR (object);

	g_object_unref (monitor->details->volume_monitor);

	/* Default links */

	remove_link_and_preference (&monitor->details->home_link,
				    NOLPHIN_PREFERENCES_DESKTOP_HOME_VISIBLE,
				    G_CALLBACK (desktop_home_visible_changed),
				    monitor);

	remove_link_and_preference (&monitor->details->computer_link,
				    NOLPHIN_PREFERENCES_DESKTOP_COMPUTER_VISIBLE,
				    G_CALLBACK (desktop_computer_visible_changed),
				    monitor);

	remove_link_and_preference (&monitor->details->trash_link,
				    NOLPHIN_PREFERENCES_DESKTOP_TRASH_VISIBLE,
				    G_CALLBACK (desktop_trash_visible_changed),
				    monitor);

	remove_link_and_preference (&monitor->details->network_link,
				    NOLPHIN_PREFERENCES_DESKTOP_NETWORK_VISIBLE,
				    G_CALLBACK (desktop_network_visible_changed),
				    monitor);

	/* Mounts */

	g_list_foreach (monitor->details->mount_links, (GFunc)g_object_unref, NULL);
	g_list_free (monitor->details->mount_links);
	monitor->details->mount_links = NULL;

	nolphin_directory_unref (monitor->details->desktop_dir);
	monitor->details->desktop_dir = NULL;

	g_signal_handlers_disconnect_by_func (nolphin_desktop_preferences,
					      desktop_volumes_visible_changed,
					      monitor);

	G_OBJECT_CLASS (nolphin_desktop_link_monitor_parent_class)->finalize (object);
}

static void
nolphin_desktop_link_monitor_class_init (NolphinDesktopLinkMonitorClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = desktop_link_monitor_finalize;

	g_type_class_add_private (klass, sizeof (NolphinDesktopLinkMonitorDetails));
}
