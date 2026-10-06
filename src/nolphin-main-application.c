/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nolphin-main-application: Nolphin application subclass for standard windows
 *
 * Copyright (C) 1999, 2000 Red Hat, Inc.
 * Copyright (C) 2000, 2001 Eazel, Inc.
 * Copyright (C) 2010, Cosimo Cecchi <cosimoc@gnome.org>
 *
 * Nolphin is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nolphin is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Suite 500, MA 02110-1335, USA.
 *
 */

#include <config.h>

#include "nolphin-main-application.h"

#if ENABLE_EMPTY_VIEW
#include "nolphin-empty-view.h"
#endif /* ENABLE_EMPTY_VIEW */

#include "nolphin-freedesktop-dbus.h"
#include "nolphin-icon-view.h"
#include "nolphin-image-properties-page.h"
#include "nolphin-list-view.h"
#include "nolphin-previewer.h"
#include "nolphin-progress-ui-handler.h"
#include "nolphin-self-check-functions.h"
#include "nolphin-window.h"
#include "nolphin-window-bookmarks.h"
#include "nolphin-window-manage-views.h"
#include "nolphin-window-private.h"
#include "nolphin-window-slot.h"
#include "nolphin-statusbar.h"
#include "nolphin-notebook.h"

#include <libnolphin-private/nolphin-dbus-manager.h>
#include <libnolphin-private/nolphin-directory-private.h>
#include <libnolphin-private/nolphin-file-utilities.h>
#include <libnolphin-private/nolphin-file-operations.h>
#include <libnolphin-private/nolphin-global-preferences.h>
#include <libnolphin-private/nolphin-lib-self-check-functions.h>
#include <libnolphin-private/nolphin-module.h>
#include <libnolphin-private/nolphin-signaller.h>
#include <libnolphin-private/nolphin-ui-utilities.h>
#include <libnolphin-private/nolphin-undo-manager.h>
#include <libnolphin-private/nolphin-thumbnails.h>
#include <libnolphin-private/nolphin-search-engine-advanced.h>
#include <libnolphin-extension/nolphin-menu-provider.h>

#define DEBUG_FLAG NOLPHIN_DEBUG_APPLICATION
#include <libnolphin-private/nolphin-debug.h>
#include "nolphin-diagnostics.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <fcntl.h>
#include <errno.h>
#include <gdk/gdkx.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-stock-dialogs.h>

#define GNOME_DESKTOP_USE_UNSTABLE_API

#include <libcinnamon-desktop/gnome-desktop-thumbnail.h>

/* Keep window from shrinking down ridiculously small; numbers are somewhat arbitrary */
#define APPLICATION_WINDOW_MIN_WIDTH	300
#define APPLICATION_WINDOW_MIN_HEIGHT	100

#define START_STATE_CONFIG "start-state"

#define NOLPHIN_ACCEL_MAP_SAVE_DELAY 30

/* Disable the self-check functionality */
#define NOLPHIN_OMIT_SELF_CHECK "omit"

#define NOLPHIN_NOTIFICATION_UNMOUNT_ICON_NAME  "media-removable"
#define NOLPHIN_NOTIFICATION_UNMOUNT_ID_PENDING "unmount-pending"
#define NOLPHIN_NOTIFICATION_UNMOUNT_ID_DONE    "unmount-done"

static void     mount_removed_callback            (GVolumeMonitor            *monitor,
						   GMount                    *mount,
						   NolphinMainApplication       *application);
static void     mount_added_callback              (GVolumeMonitor            *monitor,
						   GMount                    *mount,
						   NolphinMainApplication       *application);

G_DEFINE_TYPE (NolphinMainApplication, nolphin_main_application, NOLPHIN_TYPE_APPLICATION);

struct _NolphinMainApplicationPriv {
	GVolumeMonitor *volume_monitor;

	NolphinDBusManager *dbus_manager;
	NolphinFreedesktopDBus *fdb_manager;

	gchar *geometry;
};

static void
nolphin_main_application_send_notification (NolphinApplication *application,
                                         const gchar *title,
                                         const gchar *body,
                                         const gchar *icon_name,
                                         const gchar *notification_id,
                                         const GNotificationPriority prio)
{
	NolphinMainApplication *app = NOLPHIN_MAIN_APPLICATION (application);
	GNotification *notification;
	GIcon *icon;

	icon = g_themed_icon_new (icon_name);
	notification = g_notification_new (title);
	g_notification_set_body (notification, body);
	g_notification_set_icon (notification, icon);
	g_notification_set_priority (notification, prio);

	g_application_send_notification (G_APPLICATION (app), notification_id, notification);

	g_object_unref (notification);
	g_object_unref (icon);
}

static void
nolphin_main_application_notify_unmount_done (NolphinApplication *application,
                                           const gchar     *message)
{
	NolphinMainApplication *app = NOLPHIN_MAIN_APPLICATION (application);
	gchar **strings;

	// remove notification for pending unmount state
	g_application_withdraw_notification (G_APPLICATION (app), NOLPHIN_NOTIFICATION_UNMOUNT_ID_PENDING);

	g_return_if_fail (message != NULL);
	strings = g_strsplit (message, "\n", 2);

	nolphin_main_application_send_notification (application, strings[0], strings[1],
	                                         NOLPHIN_NOTIFICATION_UNMOUNT_ICON_NAME,
	                                         NOLPHIN_NOTIFICATION_UNMOUNT_ID_DONE,
	                                         G_NOTIFICATION_PRIORITY_NORMAL);
	
	g_strfreev (strings);
}

static void
nolphin_main_application_notify_unmount_show (NolphinApplication *application,
                                           const gchar     *message)
{
	gchar **strings;

	g_return_if_fail (message != NULL);
	strings = g_strsplit (message, "\n", 2);

	nolphin_main_application_send_notification (application, strings[0], strings[1],
	                                         NOLPHIN_NOTIFICATION_UNMOUNT_ICON_NAME,
	                                         NOLPHIN_NOTIFICATION_UNMOUNT_ID_PENDING,
	                                         G_NOTIFICATION_PRIORITY_URGENT);
	g_strfreev (strings);
}

static void
nolphin_main_application_close_all_windows (NolphinApplication *self)
{
	GList *list_copy;
	GList *l;
	
	list_copy = g_list_copy (gtk_application_get_windows (GTK_APPLICATION (self)));
	for (l = list_copy; l != NULL; l = l->next) {
		if (NOLPHIN_IS_WINDOW (l->data)) {
			NolphinWindow *window;

			window = NOLPHIN_WINDOW (l->data);
			nolphin_window_close (window);
		}
	}
	g_list_free (list_copy);
}

static NolphinWindow *
nolphin_main_application_create_window (NolphinApplication *application,
                                     GdkScreen       *screen)
{
	NolphinWindow *window;
	char *geometry_string;
	gboolean maximized;

	g_return_val_if_fail (NOLPHIN_IS_APPLICATION (application), NULL);

    window = nolphin_window_new (GTK_APPLICATION (application), screen);

	maximized = g_settings_get_boolean
		(nolphin_window_state, NOLPHIN_WINDOW_STATE_MAXIMIZED);
	if (maximized) {
		gtk_window_maximize (GTK_WINDOW (window));
	} else {
		gtk_window_unmaximize (GTK_WINDOW (window));
	}

    geometry_string = g_settings_get_string (nolphin_window_state, NOLPHIN_WINDOW_STATE_GEOMETRY);

    if (NOLPHIN_MAIN_APPLICATION (application)->priv->geometry == NULL && 
        geometry_string != NULL &&
        geometry_string[0] != 0) {
		/* Ignore saved window position if a window with the same
		 * location is already showing. That way the two windows
		 * wont appear at the exact same location on the screen.
		 */
		eel_gtk_window_set_initial_geometry_from_string 
			(GTK_WINDOW (window), 
			 geometry_string,
			 NOLPHIN_WINDOW_MIN_WIDTH,
			 NOLPHIN_WINDOW_MIN_HEIGHT,
			 TRUE);
	}

	g_free (geometry_string);

    nolphin_undo_manager_attach (application->undo_manager, G_OBJECT (window));

	DEBUG ("Creating a new navigation window");
	
	return window;
}

static void
mount_added_callback (GVolumeMonitor *monitor,
		      GMount *mount,
		      NolphinMainApplication *application)
{
	NolphinDirectory *directory;
	GFile *root;
	gchar *uri;
		
	root = g_mount_get_root (mount);
	uri = g_file_get_uri (root);

	DEBUG ("Added mount at uri %s", uri);
	g_free (uri);
	
	directory = nolphin_directory_get_existing (root);
	g_object_unref (root);
	if (directory != NULL) {
		nolphin_directory_force_reload (directory);
		nolphin_directory_unref (directory);
	}
}

/* Called whenever a mount is unmounted. Check and see if there are
 * any windows open displaying contents on the mount. If there are,
 * close them.  It would also be cool to save open window and position
 * info.
 */
static void
mount_removed_callback (GVolumeMonitor *monitor,
			GMount *mount,
			NolphinMainApplication *application)
{
	GList *window_list, *node, *close_list;
	NolphinWindow *window;
	NolphinWindowSlot *slot;
	NolphinWindowSlot *force_no_close_slot;
	GFile *root, *computer;
	gchar *uri;
	guint n_slots;

	close_list = NULL;
	force_no_close_slot = NULL;
	n_slots = 0;

	/* Check and see if any of the open windows are displaying contents from the unmounted mount */
	window_list = gtk_application_get_windows (GTK_APPLICATION (application));

	root = g_mount_get_root (mount);
	uri = g_file_get_uri (root);

	DEBUG ("Removed mount at uri %s", uri);
	g_free (uri);

	/* Construct a list of windows to be closed. Do not add the non-closable windows to the list. */
	for (node = window_list; node != NULL; node = node->next) {
		window = NOLPHIN_WINDOW (node->data);
		if (window != NULL) {
			GList *l;
			GList *lp;
			GFile *saved_location;

			/* Clear the stored secondary pane location if it was on the removed mount */
			saved_location = window->details->secondary_pane_last_location;
			if (saved_location != NULL &&
			    (g_file_has_prefix (saved_location, root) || g_file_equal (saved_location, root))) {
				nolphin_window_clear_secondary_pane_location (window);
			}

			for (lp = window->details->panes; lp != NULL; lp = lp->next) {
				NolphinWindowPane *pane;
				pane = (NolphinWindowPane*) lp->data;
				for (l = pane->slots; l != NULL; l = l->next) {
					slot = l->data;
					n_slots++;
					if (nolphin_window_slot_should_close_with_mount (slot, mount)) {
						close_list = g_list_prepend (close_list, slot);
					}
				} /* for all slots */
			} /* for all panes */
		}
	}

	g_object_unref (root);

	/* Handle the windows in the close list. */
	for (node = close_list; node != NULL; node = node->next) {
		slot = node->data;

		if (slot != force_no_close_slot) {
            if (g_settings_get_boolean (nolphin_preferences, NOLPHIN_PREFERENCES_CLOSE_DEVICE_VIEW_ON_EJECT))
                nolphin_window_pane_close_slot (slot->pane, slot);
            else
                nolphin_window_slot_go_home (slot, FALSE);
		} else {
			computer = g_file_new_for_path (g_get_home_dir ());
			nolphin_window_slot_open_location (slot, computer, 0);
			g_object_unref(computer);
		}
	}

	g_list_free (close_list);
}

static void
open_window (NolphinMainApplication *application,
	     GFile *location, GdkScreen *screen, const char *geometry)
{
	NolphinWindow *window;
	gchar *uri;
	gboolean have_geometry;

	uri = g_file_get_uri (location);
	DEBUG ("Opening new window at uri %s", uri);

	window = nolphin_main_application_create_window (NOLPHIN_APPLICATION (application),
						     screen);
	nolphin_window_go_to (window, location);

	have_geometry = geometry != NULL && strcmp(geometry, "") != 0;

	if (have_geometry && !gtk_widget_get_visible (GTK_WIDGET (window))) {
		/* never maximize windows opened from shell if a
		 * custom geometry has been requested.
		 */
		gtk_window_unmaximize (GTK_WINDOW (window));
		eel_gtk_window_set_initial_geometry_from_string (GTK_WINDOW (window),
								 geometry,
								 APPLICATION_WINDOW_MIN_WIDTH,
								 APPLICATION_WINDOW_MIN_HEIGHT,
								 FALSE);
	}

	g_free (uri);
}

static void
open_tabs (NolphinMainApplication *application,
         GFile **locations,
         guint n_files,
         GdkScreen *screen,
         const char *geometry)
{
    NolphinWindow *window;
    gchar *uri;
    gboolean have_geometry;

    window = nolphin_main_application_create_window (NOLPHIN_APPLICATION (application),
                             screen);

    /* open all locations */
    uri = g_file_get_uri (locations[0]);
    g_debug ("Opening new tab at uri %s\n", uri);
    nolphin_window_go_to (window, locations[0]);
    g_free (uri);
    for (int i = 1; i < n_files; i++) {
        /* open tabs in reverse order because each
         * tab is opened before the previous one */
        guint tab = n_files-i;
        uri = g_file_get_uri (locations[tab]);
        g_debug ("Opening new tab at uri %s\n", uri);
        nolphin_window_go_to_tab (window, locations[tab]);
        g_free (uri);
    }

    have_geometry = geometry != NULL && strcmp(geometry, "") != 0;

    if (have_geometry && !gtk_widget_get_visible (GTK_WIDGET (window))) {
        /* never maximize windows opened from shell if a
         * custom geometry has been requested.
         */
        gtk_window_unmaximize (GTK_WINDOW (window));
        eel_gtk_window_set_initial_geometry_from_string (GTK_WINDOW (window),
                                 geometry,
                                 APPLICATION_WINDOW_MIN_WIDTH,
                                 APPLICATION_WINDOW_MIN_HEIGHT,
                                 FALSE);
    }
}

static void
open_tabs_in_existing_window (NolphinMainApplication *application,
                              GFile **locations,
                              guint n_files,
                              GdkScreen *screen,
                              const char *geometry)
{
    gchar *uri;

    GList *list_copy;
    GList *l;

    list_copy = g_list_copy (gtk_application_get_windows (GTK_APPLICATION (&application->parent)));
    for (l = list_copy; l != NULL; l = l->next) {
        if (NOLPHIN_IS_WINDOW (l->data)) {
            NolphinWindow *window;

            window = NOLPHIN_WINDOW (l->data);

            /* open all locations */
            for (int i = 1; i <= n_files; i++) {
                /* open tabs in reverse order because each
                 * tab is opened before the previous one */
                guint tab = n_files - i;
                uri = g_file_get_uri (locations[tab]);
                g_debug ("Opening new tab at uri %s\n", uri);
                nolphin_window_go_to_tab (window, locations[tab]);
                g_free(uri);
            }

            /* go to the last tab we opened */
            NolphinWindowPane *pane;

            pane = nolphin_window_get_active_pane (window);
            nolphin_notebook_set_current_page_relative (NOLPHIN_NOTEBOOK (pane->notebook), n_files);

            /* Don't use `gtk_window_present()`, as the window manager will ignore this window's focus request and try
             * to just mark it urgent instead (flashing in the window list for example). */
            if (eel_check_is_wayland ()) {
                gtk_window_present (GTK_WINDOW (window));
            } else {
                gtk_window_present_with_time (GTK_WINDOW (window),
                                              gdk_x11_get_server_time (gtk_widget_get_window (GTK_WIDGET (window))));
            }

          break;
        }
    }
    if (l == NULL) {
        /* no existing window was found, so open a new window */
        open_tabs (application, locations, n_files, screen, geometry);
    }
    g_list_free (list_copy);
}

static void
open_windows (NolphinMainApplication *application,
	      GFile **files,
	      gint n_files,
	      GdkScreen *screen,
	      const char *geometry,
	      gboolean open_in_tabs,
	      gboolean open_in_existing_window)
{
	gint i;

	if (files == NULL || files[0] == NULL) {
		/* No explicit locations requested: try restoring the last session. */
		NolphinWindow *window;
		gboolean have_geometry;
		gboolean do_restore;

		window = nolphin_main_application_create_window (NOLPHIN_APPLICATION (application), screen);

		have_geometry = geometry != NULL && strcmp (geometry, "") != 0;
		if (have_geometry && !gtk_widget_get_visible (GTK_WIDGET (window))) {
			/* never maximize windows opened from shell if a
			 * custom geometry has been requested.
			 */
			gtk_window_unmaximize (GTK_WINDOW (window));
			eel_gtk_window_set_initial_geometry_from_string (GTK_WINDOW (window),
									 geometry,
									 APPLICATION_WINDOW_MIN_WIDTH,
									 APPLICATION_WINDOW_MIN_HEIGHT,
									 FALSE);
		}

		do_restore = g_settings_get_boolean (nolphin_preferences, NOLPHIN_PREFERENCES_RESTORE_TABS_ON_STARTUP);

		if (!do_restore || !nolphin_window_restore_saved_tabs (window)) {
			/* Fall back to a safe default location */
			GFile *home = g_file_new_for_path (g_get_home_dir ());
			nolphin_window_go_to (window, home);
			g_object_unref (home);
		}
	} else {
		if (open_in_existing_window) {
			/* Open one tab at each requested location in an existing window */
			open_tabs_in_existing_window (application, files, n_files, screen, geometry);
		} else if (open_in_tabs) {
			/* Open one window with one tab at each requested location */
			open_tabs (application, files, n_files, screen, geometry);
		} else {
			/* Open windows at each requested location. */
			for (i = 0; i < n_files; i++) {
				open_window (application, files[i], screen, geometry);
			}
		}
	}
}

static void
nolphin_main_application_open_location (NolphinApplication     *application,
                                     GFile               *location,
                                     GFile               *selection,
                                     const char          *startup_id,
                                     const gboolean      open_in_tabs)
{
	NolphinWindow *window;
	GList *sel_list = NULL;

	window = nolphin_main_application_create_window (application, gdk_screen_get_default ());
	gtk_window_set_startup_id (GTK_WINDOW (window), startup_id);

	if (selection != NULL) {
		sel_list = g_list_prepend (sel_list, nolphin_file_get (selection));
	}

	if(open_in_tabs){
		nolphin_window_slot_open_location_full (nolphin_window_get_active_slot (window), location,
						 NOLPHIN_WINDOW_OPEN_FLAG_NEW_TAB, sel_list, NULL, NULL);
	} else {
		nolphin_window_slot_open_location_full (nolphin_window_get_active_slot (window), location,
						 0, sel_list, NULL, NULL);
	}

	if (sel_list != NULL) {
		nolphin_file_list_free (sel_list);
	}
}

static NolphinWindowSlot *
find_existing_slot_for_location (NolphinApplication *application,
                                 GFile           *location)
{
	GList *windows;
	GList *l;

	windows = gtk_application_get_windows (GTK_APPLICATION (application));
	for (l = windows; l != NULL; l = l->next) {
		NolphinWindow *window;
		GList *p;

		if (!NOLPHIN_IS_WINDOW (l->data)) {
			continue;
		}

		window = NOLPHIN_WINDOW (l->data);

		for (p = window->details->panes; p != NULL; p = p->next) {
			NolphinWindowPane *pane = p->data;
			GList *s;

			for (s = pane->slots; s != NULL; s = s->next) {
				NolphinWindowSlot *slot = s->data;
				GFile *slot_location;
				gboolean match;

				slot_location = nolphin_window_slot_get_location (slot);
				if (slot_location == NULL) {
					continue;
				}

				match = g_file_equal (slot_location, location);
				g_object_unref (slot_location);

				if (match) {
					return slot;
				}
			}
		}
	}

	return NULL;
}

static void
present_window (NolphinWindow *window)
{
	if (eel_check_is_wayland ()) {
		gtk_window_present (GTK_WINDOW (window));
	} else {
		gtk_window_present_with_time (GTK_WINDOW (window),
		                              gdk_x11_get_server_time (gtk_widget_get_window (GTK_WIDGET (window))));
	}
}

static void
nolphin_main_application_show_items (NolphinApplication *application,
                                  GFile          **uris,
                                  gint             n_uris,
                                  const char      *startup_id)
{
	GHashTable *groups;
	GHashTableIter iter;
	gpointer key, value;
	gint i;

	if (uris == NULL || n_uris == 0) {
		return;
	}

	/* Group URIs by their parent location so URIs sharing a parent open in
	 * a single window with all of them selected, instead of N windows of
	 * the same folder.  Keyed by parent URI string (owned by hash). */
	groups = g_hash_table_new_full (g_str_hash, g_str_equal,
	                                g_free, (GDestroyNotify) g_ptr_array_unref);

	for (i = 0; i < n_uris; i++) {
		GFile *parent;
		gchar *key_uri;
		GPtrArray *group;

		parent = g_file_get_parent (uris[i]);
		key_uri = (parent != NULL) ? g_file_get_uri (parent)
		                           : g_file_get_uri (uris[i]);

		group = g_hash_table_lookup (groups, key_uri);
		if (group == NULL) {
			group = g_ptr_array_new ();
			g_hash_table_insert (groups, key_uri, group);
		} else {
			g_free (key_uri);
		}
		g_ptr_array_add (group, uris[i]);

		g_clear_object (&parent);
	}

	g_hash_table_iter_init (&iter, groups);
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		GPtrArray *group = (GPtrArray *) value;
		GFile *first = g_ptr_array_index (group, 0);
		GFile *parent;
		GFile *target;
		NolphinWindowSlot *existing_slot;
		GList *sel_list = NULL;
		guint j;

		parent = g_file_get_parent (first);
		target = (parent != NULL) ? parent : first;

		if (parent != NULL) {
			for (j = 0; j < group->len; j++) {
				sel_list = g_list_prepend (sel_list,
				                           nolphin_file_get ((GFile *) g_ptr_array_index (group, j)));
			}
			sel_list = g_list_reverse (sel_list);
		}

		existing_slot = find_existing_slot_for_location (application, target);

		if (existing_slot != NULL) {
			NolphinWindow *window = nolphin_window_slot_get_window (existing_slot);
			NolphinView *view;

			nolphin_window_slot_make_hosting_pane_active (existing_slot);

			if (sel_list != NULL) {
				view = nolphin_window_slot_get_current_view (existing_slot);
				if (view != NULL) {
					nolphin_view_set_selection (view, sel_list);
				}
			}

			present_window (window);
		} else {
			NolphinWindow *window;

			window = nolphin_main_application_create_window (application, gdk_screen_get_default ());
			if (startup_id != NULL) {
				gtk_window_set_startup_id (GTK_WINDOW (window), startup_id);
			}

			nolphin_window_slot_open_location_full (nolphin_window_get_active_slot (window), target,
			                                     0, sel_list, NULL, NULL);
		}

		if (sel_list != NULL) {
			nolphin_file_list_free (sel_list);
		}
		g_clear_object (&parent);
	}

	g_hash_table_destroy (groups);
}

static void
nolphin_main_application_open (GApplication *app,
                            GFile       **files,
                            gint          n_files,
                            const gchar  *options)
{
	NolphinMainApplication *self = NOLPHIN_MAIN_APPLICATION (app);

	gboolean open_in_tabs = FALSE;
	gchar *geometry = NULL;
	gboolean open_in_existing_window = strcmp (options, "EXISTING_WINDOW") == 0;
	gboolean default_no_args = FALSE;
	gboolean select_mode = FALSE;
	const char splitter = '=';

	g_debug ("Open called on the GApplication instance; %d files", n_files);

	if (!open_in_existing_window) {
		/* Check if local command line passed --geometry, --tabs or --select */
		if (strlen (options) > 0) {
			gchar** split_options = g_strsplit (options, &splitter, 2);
			if (strcmp (split_options[0], "SELECT") == 0) {
				select_mode = TRUE;
			} else if (g_str_has_prefix (split_options[0], "DEFAULT")) {
				default_no_args = TRUE;
				if (g_str_has_prefix (split_options[0], "DEFAULT+")) {
					geometry = g_strdup (split_options[0] + strlen ("DEFAULT+"));
				}
			} else if (strcmp (split_options[0], "NULL") != 0) {
				geometry = g_strdup (split_options[0]);
			}
			sscanf (split_options[1], "%d", &open_in_tabs);
			g_strfreev (split_options);
		}
	}

	DEBUG ("Open called on the GApplication instance; %d files, open in tabs: %s, geometry: '%s',"
           "open in existing window: %s, select mode: %s",
           n_files,
           open_in_tabs ? "yes" : "no",
           geometry ? geometry : "none",
           open_in_existing_window ? "yes" : "no",
           select_mode ? "yes" : "no");

	if (select_mode) {
		nolphin_application_show_items (NOLPHIN_APPLICATION (app), files, n_files, NULL);
	} else if (default_no_args) {
		/* Treat this as a no-arg launch; open_windows() will attempt session restore
		 * and fall back to Home if restore isn't possible. */
		open_windows (self, NULL, 0, gdk_screen_get_default (), geometry, open_in_tabs, open_in_existing_window);
	} else {
		open_windows (self, files, n_files, gdk_screen_get_default (), geometry, open_in_tabs, open_in_existing_window);
	}

    g_clear_pointer (&geometry, g_free);
}

static void
nolphin_main_application_init (NolphinMainApplication *application)
{
    application->priv = G_TYPE_INSTANCE_GET_PRIVATE (application,
                                                     NOLPHIN_TYPE_MAIN_APPLICATION,
                                                     NolphinMainApplicationPriv);
}

static void
nolphin_main_application_finalize (GObject *object)
{
    NolphinMainApplication *application;

    application = NOLPHIN_MAIN_APPLICATION (object);

    nolphin_bookmarks_exiting ();

    g_clear_object (&application->priv->volume_monitor);
    g_free (application->priv->geometry);

    g_clear_object (&application->priv->dbus_manager);
    g_clear_object (&application->priv->fdb_manager);

    free_search_helpers ();

    G_OBJECT_CLASS (nolphin_main_application_parent_class)->finalize (object);
}

static gboolean
do_cmdline_sanity_checks (NolphinMainApplication *self,
			  gboolean perform_self_check,
			  gboolean version,
			  gboolean kill_shell,
			  gboolean open_in_tabs,
			  gchar **remaining)
{
	gboolean retval = FALSE;

	if (perform_self_check && (remaining != NULL || kill_shell)) {
		g_printerr ("%s\n",
			    _("--check kann nicht zusammen mit anderen Optionen verwendet werden."));
		goto out;
	}

	if (kill_shell && remaining != NULL) {
		g_printerr ("%s\n",
			    _("--quit kann nicht mit Adressen benutzt werden."));
		goto out;
	}

	if (self->priv->geometry != NULL &&
	    !open_in_tabs &&
	    remaining != NULL && remaining[0] != NULL && remaining[1] != NULL) {
		g_printerr ("%s\n",
			    _("--geometry kann nicht mit mehr als einer Adresse benutzt werden."));
		goto out;
	}

	retval = TRUE;

 out:
	return retval;
}

static void
do_perform_self_checks (gint *exit_status)
{
#ifndef NOLPHIN_OMIT_SELF_CHECK
	/* Run the checks (each twice) for nolphin and libnolphin-private. */

	nolphin_run_self_checks ();
	nolphin_run_lib_self_checks ();
	eel_exit_if_self_checks_failed ();

	nolphin_run_self_checks ();
	nolphin_run_lib_self_checks ();
	eel_exit_if_self_checks_failed ();
#endif

	*exit_status = EXIT_SUCCESS;
}

static gboolean
nolphin_main_application_local_command_line (GApplication *application,
					 gchar ***arguments,
					 gint *exit_status)
{
	gboolean perform_self_check = FALSE;
	gboolean version = FALSE;
	gboolean browser = FALSE;
	gboolean open_in_tabs = FALSE;
	gboolean open_in_existing_window = FALSE;
	gboolean kill_shell = FALSE;
	gboolean no_default_window = FALSE;
    gboolean no_desktop_ignored = FALSE;
	gboolean fix_cache = FALSE;
    gboolean debug = FALSE;
	gboolean select = FALSE;
	gchar **remaining = NULL;
    GApplicationFlags init_flags;
	NolphinMainApplication *self = NOLPHIN_MAIN_APPLICATION (application);

	const GOptionEntry options[] = {
#ifndef NOLPHIN_OMIT_SELF_CHECK
		{ "check", 'c', 0, G_OPTION_ARG_NONE, &perform_self_check, 
		  N_("Eine Reihe schneller Selbsttests durchführen."), NULL },
#endif
		/* dummy, only for compatibility reasons */
		{ "browser", '\0', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE, &browser,
		  NULL, NULL },
		{ "version", '\0', 0, G_OPTION_ARG_NONE, &version,
		  N_("Die Programmversion anzeigen"), NULL },
		{ "geometry", 'g', 0, G_OPTION_ARG_STRING, &self->priv->geometry,
		  N_("Das Anfangsfenster mit der angegebenen Geometrie erstellen. Beispiele: nolphin --geometry=+100+100, nolphin --geometry=600x400, nolphin --geometry=600x400+100+100."), N_("GEOMETRIE") },
		{ "no-default-window", 'n', 0, G_OPTION_ARG_NONE, &no_default_window,
		  N_("Nur für ausdrücklich angegebene Adressen die Fenster erstellen."), NULL },
        { "no-desktop", '\0', 0, G_OPTION_ARG_NONE, &no_desktop_ignored,
          N_("Ignorierter Ausdruck – wird nur zur Kompatibilität zugelassen."), NULL },
		{ "tabs", 't', 0, G_OPTION_ARG_NONE, &open_in_tabs,
		  N_("Adressen in Reitern öffnen."), NULL },
		{ "existing-window", 0, 0, G_OPTION_ARG_NONE, &open_in_existing_window,
		  N_("Adressen in einem bestehenden Fenster öffnen."), NULL },
		{ "select", 's', 0, G_OPTION_ARG_NONE, &select,
		  N_("Den übergeordneten Ordner der Adresse anzeigen und die Adresse auswählen."), NULL },
		{ "fix-cache", '\0', 0, G_OPTION_ARG_NONE, &fix_cache,
		  N_("Bitte den Benutzervorschaubildpuffer reparieren - das kann nützlich sein, wenn Sie Probleme mit den Vorschaubildern haben. Muss als Systemverwalter ausgeführt werden."), NULL },
        { "debug", 0, 0, G_OPTION_ARG_NONE, &debug,
          "Enable debugging code.  Example usage: 'NOLPHIN_DEBUG=Actions,Window nolphin --debug'.  Use NOLPHIN_DEBUG=help for more topics.", NULL },
		{ "quit", 'q', 0, G_OPTION_ARG_NONE, &kill_shell, 
		  N_("Nolphin beenden."), NULL },
		{ G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, &remaining, NULL,  N_("[Adresse …]") },

		{ NULL }
	};
	GOptionContext *context;
	GError *error = NULL;
	gint argc = 0;
	gchar **argv = NULL;

	*exit_status = EXIT_SUCCESS;

	context = g_option_context_new (_("\n\nDas Dateisystem mit Hilfe der Dateiverwaltung durchsuchen"));
	g_option_context_add_main_entries (context, options, NULL);
	g_option_context_add_group (context, gtk_get_option_group (TRUE));

	argv = *arguments;
	argc = g_strv_length (argv);

	if (!g_option_context_parse (context, &argc, &argv, &error)) {
		g_printerr ("Could not parse arguments: %s\n", error->message);
		g_error_free (error);

		*exit_status = EXIT_FAILURE;
		goto out;
	}

	if (version) {
		g_print ("nolphin " VERSION "\n");
		goto out;
	}

    if (debug) {
#if (GLIB_CHECK_VERSION(2,80,0))
        const gchar* const domains[] = { "Nolphin", NULL };
        g_log_writer_default_set_debug_domains (domains);
#else
        g_setenv ("G_MESSAGES_DEBUG", "all", TRUE);
#endif
    }

	if (!do_cmdline_sanity_checks (self, perform_self_check,
				       version, kill_shell, open_in_tabs, remaining)) {
		*exit_status = EXIT_FAILURE;
		goto out;
	}

	if (perform_self_check) {
		do_perform_self_checks (exit_status);
		goto out;
	}

    if (fix_cache) {
        if (nolphin_user_is_root () && nolphin_treating_root_as_normal ()) {
            g_printerr ("Ignoring --fix-cache as root-is-normal setting is true and this "
                        "check is probably unnecessary.\n");
        } else
        if (!nolphin_user_is_root ()) {
            g_printerr ("The --fix-cache option must be run with sudo or as the root user.\n");
        } else
        {
            gnome_desktop_thumbnail_cache_fix_permissions ();
            g_print ("User thumbnail cache successfully repaired.\n");
        }

        goto out;
    }

	DEBUG ("Parsing local command line, no_default_window %d, quit %d, "
	       "self checks %d",
	       no_default_window, kill_shell, perform_self_check);

    /* Keep our original flags handy */
    init_flags = g_application_get_flags (application);

    /* First try to register as a service (this allows our dbus activation to succeed
     * if we're not already running */
    g_application_set_flags (application, init_flags | G_APPLICATION_IS_SERVICE);
    g_application_register (application, NULL, &error);

	if (error != NULL) {
        g_debug ("Could not register nolphin as a service, trying as a remote: %s", error->message);
        g_clear_error (&error);
    } else {
        goto post_registration;
    }

    /* If service registration failed, try to connect to the existing instance */
    g_application_set_flags (application, init_flags | G_APPLICATION_IS_LAUNCHER);
    g_application_register (application, NULL, &error);

    if (error != NULL) {
        g_printerr ("Could not register nolphin as a remote: %s\n", error->message);
        g_clear_error (&error);

        *exit_status = EXIT_FAILURE;
        goto out;
    }

post_registration:

	if (kill_shell) {
		DEBUG ("Killing application, as requested");
		g_action_group_activate_action (G_ACTION_GROUP (application),
						"quit", NULL);
		goto out;
	}

	GFile **files;
	gint idx, len;
	gboolean used_default_location;

	len = 0;
	files = NULL;
	used_default_location = FALSE;

	/* Convert args to GFiles */
	if (remaining != NULL) {
		GFile *file;
		GPtrArray *file_array;

		file_array = g_ptr_array_new ();

		for (idx = 0; remaining[idx] != NULL; idx++) {
			file = g_file_new_for_commandline_arg (remaining[idx]);
			if (file != NULL) {
				g_ptr_array_add (file_array, file);
			}
		}

		len = file_array->len;
		files = (GFile **) g_ptr_array_free (file_array, FALSE);
		g_strfreev (remaining);
	}

	if (select && len == 0) {
		g_printerr ("%s\n", _("--select muss mit mindestens einer Adresse verwendet werden."));
		*exit_status = EXIT_FAILURE;
		goto out;
	}

	if (files == NULL && !no_default_window) {
		/* Original behavior: default to Home when no URIs are provided. */
		files = g_malloc0 (2 * sizeof (GFile *));
		len = 1;

		files[0] = g_file_new_for_path (g_get_home_dir ());
		files[1] = NULL;

		/* Mark that this was a no-arg launch, not an explicit URI. */
		used_default_location = TRUE;
	}

	/* Invoke "Open" to open in existing window or create new windows.
	 */
	if (len > 0) {
		gchar* concatOptions = g_malloc0(64);
		if (open_in_existing_window) {
			g_stpcpy (concatOptions, "EXISTING_WINDOW");
		} else if (select) {
			g_snprintf (concatOptions, 64, "SELECT=%d", open_in_tabs);
		} else {
			if (self->priv->geometry == NULL) {
				/* If Home was synthesized because no URIs were passed, signal that
				 * to the primary instance so it can attempt session restore. */
				if (used_default_location) {
					g_snprintf (concatOptions, 64, "DEFAULT=%d", open_in_tabs);
				} else {
					g_snprintf (concatOptions, 64, "NULL=%d", open_in_tabs);
				}
			} else {
				if (used_default_location) {
					g_snprintf (concatOptions, 64, "DEFAULT+%s=%d", self->priv->geometry, open_in_tabs);
				} else {
					g_snprintf (concatOptions, 64, "%s=%d", self->priv->geometry, open_in_tabs);
				}
			}
		}
		g_application_open (application, files, len, concatOptions);
		g_free (concatOptions);
	}

	if (files != NULL) {
		for (idx = 0; idx < len; idx++) {
			g_object_unref (files[idx]);
		}
		g_free (files);
	}

 out:
	g_option_context_free (context);

	return TRUE;	
}

static void
menu_state_changed_callback (NolphinMainApplication *self)
{
    if (!g_settings_get_boolean (nolphin_window_state, NOLPHIN_WINDOW_STATE_START_WITH_MENU_BAR) &&
        !g_settings_get_boolean (nolphin_preferences, NOLPHIN_PREFERENCES_DISABLE_MENU_WARNING)) {

        GtkWidget *dialog;
        GtkWidget *msg_area;
        GtkWidget *checkbox;

        dialog = gtk_message_dialog_new (NULL,
                                         GTK_DIALOG_MODAL,
                                         GTK_MESSAGE_INFO,
                                         GTK_BUTTONS_OK,
                                         _("Nolphins Hauptmenü ist nun ausgeblendet"));

        gchar *secondary;
        secondary = g_strdup_printf (_("Sie haben sich entschieden, das Hauptmenü auszublenden. Sie können es vorübergehend wieder einblenden, indem Sie:\n\n- die Taste <Alt> drücken\n- durch Rechtsklick auf einen leeren Bereich der Hauptwerkzeugleiste\n- durch Rechtsklick auf einen leeren Bereich der Statusleiste.\n\nSie können das Hauptmenü dauerhaft wieder einblenden, indem Sie diese Option im Ansicht-Menü auswählen."));
        g_object_set (dialog,
                      "secondary-text", secondary,
                      NULL);
        g_free (secondary);

        msg_area = gtk_message_dialog_get_message_area (GTK_MESSAGE_DIALOG (dialog));
        checkbox = gtk_check_button_new_with_label (_("Diesen Hinweis nicht wieder anzeigen."));
        gtk_box_pack_start (GTK_BOX (msg_area), checkbox, TRUE, TRUE, 2);

        g_settings_bind (nolphin_preferences,
                         NOLPHIN_PREFERENCES_DISABLE_MENU_WARNING,
                         checkbox,
                         "active",
                         G_SETTINGS_BIND_DEFAULT);

        gtk_widget_show_all (dialog);

        g_signal_connect (dialog, "response",
                          G_CALLBACK (gtk_widget_destroy), NULL);
    }

}

static void
nolphin_main_application_continue_startup (NolphinApplication *app)
{
	NolphinMainApplication *self = NOLPHIN_MAIN_APPLICATION (app);

	/* create DBus manager */
	self->priv->dbus_manager = nolphin_dbus_manager_new ();
	self->priv->fdb_manager = nolphin_freedesktop_dbus_new ();

    /* Check the user's ~/.config/nolphin directory and post warnings
     * if there are problems.
     */

    nolphin_application_check_required_directory (app, nolphin_get_user_directory ());

	/* register views */
	nolphin_icon_view_register ();
	nolphin_list_view_register ();
	nolphin_icon_view_compact_register ();
	nolphin_icon_view_gallery_register ();
#if defined(ENABLE_EMPTY_VIEW) && ENABLE_EMPTY_VIEW
	nolphin_empty_view_register ();
#endif

	/* Watch for unmounts so we can close open windows */
	/* TODO-gio: This should be using the UNMOUNTED feature of GFileMonitor instead */
	self->priv->volume_monitor = g_volume_monitor_get ();
	g_signal_connect_object (self->priv->volume_monitor, "mount_removed",
				 G_CALLBACK (mount_removed_callback), self, 0);
	g_signal_connect_object (self->priv->volume_monitor, "mount_added",
				 G_CALLBACK (mount_added_callback), self, 0);

    g_signal_connect_swapped (nolphin_window_state, "changed::" NOLPHIN_WINDOW_STATE_START_WITH_MENU_BAR,
                              G_CALLBACK (menu_state_changed_callback), self);
}

static void
nolphin_desktop_application_continue_quit (NolphinApplication *app)
{
}

static void
nolphin_main_application_quit_mainloop (GApplication *app)
{
    nolphin_main_application_notify_unmount_done (NOLPHIN_APPLICATION (app), NULL);

    G_APPLICATION_CLASS (nolphin_main_application_parent_class)->quit_mainloop (app);
}

static void
nolphin_main_application_class_init (NolphinMainApplicationClass *class)
{
    GObjectClass *object_class;
    GApplicationClass *application_class;
    NolphinApplicationClass *nolphin_app_class;

    object_class = G_OBJECT_CLASS (class);
    object_class->finalize = nolphin_main_application_finalize;

    application_class = G_APPLICATION_CLASS (class);
    application_class->open = nolphin_main_application_open;
    application_class->local_command_line = nolphin_main_application_local_command_line;
    application_class->quit_mainloop = nolphin_main_application_quit_mainloop;

    nolphin_app_class = NOLPHIN_APPLICATION_CLASS (class);
    nolphin_app_class->open_location = nolphin_main_application_open_location;
    nolphin_app_class->show_items = nolphin_main_application_show_items;
    nolphin_app_class->create_window = nolphin_main_application_create_window;
    nolphin_app_class->notify_unmount_show = nolphin_main_application_notify_unmount_show;
    nolphin_app_class->notify_unmount_done = nolphin_main_application_notify_unmount_done;
    nolphin_app_class->close_all_windows = nolphin_main_application_close_all_windows;
    nolphin_app_class->continue_startup = nolphin_main_application_continue_startup;
    nolphin_app_class->continue_quit = nolphin_desktop_application_continue_quit;

    g_type_class_add_private (class, sizeof (NolphinMainApplicationPriv));
}

NolphinApplication *
nolphin_main_application_get_singleton (void)
{
    /* Ein Start aus dem Build-Ordner bekommt eine eigene ID, damit er neben der
     * installierten Version läuft und nicht in deren Instanz landet. */
    return nolphin_application_initialize_singleton (NOLPHIN_TYPE_MAIN_APPLICATION,
                                                  "application-id", nolphin_is_development_build (NULL) ? "org.Nolphin.Dev" : "org.Nolphin",
                                                  "flags", G_APPLICATION_HANDLES_OPEN,
                                                  "register-session", TRUE,
                                                  NULL);
}
