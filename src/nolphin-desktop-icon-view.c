/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* fm-desktop-icon-view.c - implementation of icon view for managing the desktop.

   Copyright (C) 2000, 2001 Eazel, Inc.mou

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
   Boston, MA 02110-1335, USA.

   Authors: Mike Engber <engber@eazel.com>
   	    Gene Z. Ragan <gzr@eazel.com>
	    Miguel de Icaza <miguel@ximian.com>
*/

#include <config.h>

#include "nolphin-desktop-icon-view.h"

#include "nolphin-actions.h"
#include "nolphin-application.h"
#include "nolphin-desktop-manager.h"
#include "nolphin-desktop-window.h"
#include "nolphin-icon-view-container.h"
#include "nolphin-view-factory.h"
#include "nolphin-view.h"

#include <X11/Xatom.h>
#include <gtk/gtk.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-vfs-extensions.h>
#include <fcntl.h>
#include <gdk/gdkx.h>
#include <glib/gi18n.h>
#include <libnolphin-private/nolphin-desktop-icon-file.h>
#include <libnolphin-private/nolphin-directory-notify.h>
#include <libnolphin-private/nolphin-file-changes-queue.h>
#include <libnolphin-private/nolphin-file-operations.h>
#include <libnolphin-private/nolphin-file-utilities.h>
#include <libnolphin-private/nolphin-ui-utilities.h>
#include <libnolphin-private/nolphin-global-preferences.h>
#include <libnolphin-private/nolphin-link.h>
#include <libnolphin-private/nolphin-metadata.h>
#include <libnolphin-private/nolphin-monitor.h>
#include <libnolphin-private/nolphin-program-choosing.h>
#include <libnolphin-private/nolphin-trash-monitor.h>
#include <libnolphin-private/nolphin-desktop-utils.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/* Timeout to check the desktop directory for updates */
#define RESCAN_TIMEOUT 4

struct NolphinDesktopIconViewDetails
{
	GdkWindow *root_window;
	GtkActionGroup *desktop_action_group;
	guint desktop_merge_id;

	/* For the desktop rescanning
	 */
	gulong delayed_init_signal;
	guint reload_desktop_timeout;
	gboolean pending_rescan;
};

static void     default_zoom_level_changed                        (gpointer                user_data);
static void     real_merge_menus                                  (NolphinView        *view);
static void     real_update_menus                                 (NolphinView        *view);
static void     nolphin_desktop_icon_view_update_icon_container_fonts  (NolphinDesktopIconView      *view);
static void     font_changed_callback                             (gpointer                callback_data);
static void     nolphin_desktop_icon_view_constructed (GObject *object);

G_DEFINE_TYPE (NolphinDesktopIconView, nolphin_desktop_icon_view, NOLPHIN_TYPE_ICON_VIEW)

static char *desktop_directory;
static time_t desktop_dir_modify_time;

#define get_icon_container(w) nolphin_icon_view_get_icon_container(NOLPHIN_ICON_VIEW (w))

static void
desktop_directory_changed_callback (gpointer callback_data)
{
	g_free (desktop_directory);
	desktop_directory = nolphin_get_desktop_directory ();
}

static void
update_margins (NolphinDesktopIconView *icon_view)
{
    NolphinIconContainer *icon_container;
    gint current_monitor;
    gint l, r, t, b;

    icon_container = get_icon_container (icon_view);

    g_object_get (NOLPHIN_DESKTOP_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (icon_view))),
                  "monitor", &current_monitor,
                  NULL);

    nolphin_desktop_manager_get_margins (nolphin_desktop_manager_get (), current_monitor, &l, &r, &t, &b);

    nolphin_icon_container_set_margins (icon_container, l, r, t, b);
}

static GdkFilterReturn
gdk_filter_func (GdkXEvent *gdk_xevent,
                 GdkEvent  *event,
                 gpointer   data)
{
    XEvent *xevent = gdk_xevent;
    NolphinDesktopIconView *icon_view;

    icon_view = NOLPHIN_DESKTOP_ICON_VIEW (data);

    switch (xevent->type) {
        case PropertyNotify:
            if (xevent->xproperty.atom == gdk_x11_get_xatom_by_name ("_NET_WORKAREA")) {
                update_margins (icon_view);
            }
            break;
        default:
            break;
    }

    return GDK_FILTER_CONTINUE;
}

static const char *
real_get_id (NolphinView *view)
{
	return NOLPHIN_DESKTOP_ICON_VIEW_ID;
}

static gboolean
should_show_file_on_current_monitor (NolphinView *view, NolphinFile *file)
{
    gint current_monitor = nolphin_desktop_utils_get_monitor_for_widget (GTK_WIDGET (view));
    gint file_monitor = nolphin_file_get_monitor_number (file);

    NolphinDesktopManager *dm = nolphin_desktop_manager_get ();

    if (current_monitor == file_monitor) {
        return TRUE;
    }

    if (nolphin_desktop_manager_get_primary_only (dm)) {
        return TRUE;
    }

    if (file_monitor > -1 &&
        !g_settings_get_boolean (nolphin_desktop_preferences, NOLPHIN_PREFERENCES_SHOW_ORPHANED_DESKTOP_ICONS)) {
        return FALSE;
    }

    if (file_monitor == -1) {
        /* New file, no previous metadata - this should go on the primary monitor */
        return nolphin_desktop_manager_get_monitor_is_primary (dm, current_monitor);
    }

    if (!nolphin_desktop_manager_get_monitor_is_active (dm, file_monitor)) {
        nolphin_file_set_is_desktop_orphan (file, TRUE);
        if (nolphin_desktop_manager_get_monitor_is_primary (dm, current_monitor)) {
            return TRUE;
        }
    }

    return FALSE;
}

static void
real_add_file (NolphinView *view, NolphinFile *file, NolphinDirectory *directory)
{
    NolphinIconView *icon_view;
    NolphinIconContainer *icon_container;

    g_assert (directory == nolphin_view_get_model (view));

    if (!should_show_file_on_current_monitor (view, file)) {
        return;
    }

    icon_view = NOLPHIN_ICON_VIEW (view);
    icon_container = get_icon_container (icon_view);

    if (nolphin_icon_container_add (icon_container, NOLPHIN_ICON_CONTAINER_ICON_DATA (file))) {
        nolphin_file_ref (file);
    }
}

static void
unrealized_callback (GtkWidget *widget, NolphinDesktopIconView *desktop_icon_view)
{
  g_return_if_fail (desktop_icon_view->details->root_window != NULL);

  gdk_window_remove_filter (desktop_icon_view->details->root_window,
                            gdk_filter_func,
                            desktop_icon_view);

  desktop_icon_view->details->root_window = NULL;
}

static void
realized_callback (GtkWidget *widget, NolphinDesktopIconView *desktop_icon_view)
{
  GdkWindow *root_window;
  GdkScreen *screen;

  g_return_if_fail (desktop_icon_view->details->root_window == NULL);

  screen = gtk_widget_get_screen (widget);

  root_window = gdk_screen_get_root_window (screen);

  desktop_icon_view->details->root_window = root_window;

  update_margins (desktop_icon_view);

  /* Setup the property filter */
  gdk_window_set_events (root_window, GDK_PROPERTY_CHANGE_MASK);
  gdk_window_add_filter (root_window,
                         gdk_filter_func,
                         desktop_icon_view);
}

static void
nolphin_desktop_icon_view_dispose (GObject *object)
{
	NolphinDesktopIconView *icon_view;
	GtkUIManager *ui_manager;

	icon_view = NOLPHIN_DESKTOP_ICON_VIEW (object);

	/* Remove desktop rescan timeout. */
	if (icon_view->details->reload_desktop_timeout != 0) {
		g_source_remove (icon_view->details->reload_desktop_timeout);
		icon_view->details->reload_desktop_timeout = 0;
	}

	ui_manager = nolphin_view_get_ui_manager (NOLPHIN_VIEW (icon_view));
	if (ui_manager != NULL) {
		nolphin_ui_unmerge_ui (ui_manager,
					&icon_view->details->desktop_merge_id,
					&icon_view->details->desktop_action_group);
	}

	g_signal_handlers_disconnect_by_func (nolphin_icon_view_preferences,
					      default_zoom_level_changed,
					      icon_view);
	g_signal_handlers_disconnect_by_func (nolphin_desktop_preferences,
					      font_changed_callback,
					      icon_view);

	g_signal_handlers_disconnect_by_func (nolphin_preferences,
					      desktop_directory_changed_callback,
					      NULL);

	g_signal_handlers_disconnect_by_func (gnome_lockdown_preferences,
					      nolphin_view_update_menus,
					      icon_view);

	G_OBJECT_CLASS (nolphin_desktop_icon_view_parent_class)->dispose (object);
}

static void
nolphin_desktop_icon_view_class_init (NolphinDesktopIconViewClass *class)
{
	NolphinViewClass *vclass;

	vclass = NOLPHIN_VIEW_CLASS (class);

    G_OBJECT_CLASS (class)->dispose = nolphin_desktop_icon_view_dispose;
	G_OBJECT_CLASS (class)->constructed = nolphin_desktop_icon_view_constructed;

    NOLPHIN_ICON_VIEW_CLASS (class)->use_grid_container = FALSE;

	vclass->merge_menus = real_merge_menus;
	vclass->update_menus = real_update_menus;
	vclass->get_view_id = real_get_id;
    vclass->add_file = real_add_file;

#if GTK_CHECK_VERSION(3, 21, 0)
	GtkWidgetClass *wclass = GTK_WIDGET_CLASS (class);
	gtk_widget_class_set_css_name (wclass, "nolphin-desktop-icon-view");
#endif
	g_type_class_add_private (class, sizeof (NolphinDesktopIconViewDetails));
}

static void
nolphin_desktop_icon_view_handle_middle_click (NolphinIconContainer *icon_container,
						GdkEventButton *event,
						NolphinDesktopIconView *desktop_icon_view)
{
	XButtonEvent x_event;
	GdkDevice *keyboard = NULL, *pointer = NULL, *cur;
	GdkDeviceManager *manager;
	GList *list, *l;

	manager = gdk_display_get_device_manager (gtk_widget_get_display (GTK_WIDGET (icon_container)));
	list = gdk_device_manager_list_devices (manager, GDK_DEVICE_TYPE_MASTER);

	for (l = list; l != NULL; l = l->next) {
		cur = l->data;

		if (pointer == NULL && (gdk_device_get_source (cur) == GDK_SOURCE_MOUSE)) {
			pointer = cur;
		}

		if (keyboard == NULL && (gdk_device_get_source (cur) == GDK_SOURCE_KEYBOARD)) {
			keyboard = cur;
		}

		if (pointer != NULL && keyboard != NULL) {
			break;
		}
	}

	g_list_free (list);

	/* During a mouse click we have the pointer and keyboard grab.
	 * We will send a fake event to the root window which will cause it
	 * to try to get the grab so we need to let go ourselves.
	 */

	if (pointer != NULL) {
		gdk_device_ungrab (pointer, GDK_CURRENT_TIME);
	}


	if (keyboard != NULL) {
		gdk_device_ungrab (keyboard, GDK_CURRENT_TIME);
	}

	/* Stop the event because we don't want anyone else dealing with it. */
	gdk_flush ();
	g_signal_stop_emission_by_name (icon_container, "middle_click");

	/* build an X event to represent the middle click. */
	x_event.type = ButtonPress;
	x_event.send_event = True;
	x_event.display = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
	x_event.window = GDK_ROOT_WINDOW ();
	x_event.root = GDK_ROOT_WINDOW ();
	x_event.subwindow = 0;
	x_event.time = event->time;
	x_event.x = event->x;
	x_event.y = event->y;
	x_event.x_root = event->x_root;
	x_event.y_root = event->y_root;
	x_event.state = event->state;
	x_event.button = event->button;
	x_event.same_screen = True;

	/* Send it to the root window, the window manager will handle it. */
	XSendEvent (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), GDK_ROOT_WINDOW (), True,
		    ButtonPressMask, (XEvent *) &x_event);
}

static void
desktop_icon_container_realize (GtkWidget *widget,
                                NolphinDesktopIconView *desktop_icon_view)
{
    GdkWindow *bin_window;
    GdkRGBA transparent = { 0, 0, 0, 0 };

    bin_window = gtk_layout_get_bin_window (GTK_LAYOUT (widget));
    gdk_window_set_background_rgba (bin_window, &transparent);
}

static NolphinZoomLevel
get_default_zoom_level (void)
{
	NolphinZoomLevel default_zoom_level;

	default_zoom_level = g_settings_get_enum (nolphin_icon_view_preferences,
						  NOLPHIN_PREFERENCES_ICON_VIEW_DEFAULT_ZOOM_LEVEL);

	return CLAMP (default_zoom_level, NOLPHIN_ZOOM_LEVEL_SMALLEST, NOLPHIN_ZOOM_LEVEL_LARGEST);
}

static void
default_zoom_level_changed (gpointer user_data)
{
	NolphinZoomLevel new_level;
	NolphinDesktopIconView *desktop_icon_view;

	desktop_icon_view = NOLPHIN_DESKTOP_ICON_VIEW (user_data);
	new_level = get_default_zoom_level ();

	nolphin_icon_container_set_zoom_level (get_icon_container (desktop_icon_view),
						new_level);
}

static gboolean
do_desktop_rescan (gpointer data)
{
	NolphinDesktopIconView *desktop_icon_view;
	struct stat buf;

	desktop_icon_view = NOLPHIN_DESKTOP_ICON_VIEW (data);
	if (desktop_icon_view->details->pending_rescan) {
		return TRUE;
	}

	if (stat (desktop_directory, &buf) == -1) {
		return TRUE;
	}

	if (buf.st_ctime == desktop_dir_modify_time) {
		return TRUE;
	}

	desktop_icon_view->details->pending_rescan = TRUE;

	nolphin_directory_force_reload
		(nolphin_view_get_model (NOLPHIN_VIEW (desktop_icon_view)));

	return TRUE;
}

static void
done_loading (NolphinDirectory *model,
	      NolphinDesktopIconView *desktop_icon_view)
{
	struct stat buf;

	desktop_icon_view->details->pending_rescan = FALSE;
	if (stat (desktop_directory, &buf) == -1) {
		return;
	}

	desktop_dir_modify_time = buf.st_ctime;
}

/* This function is used because the NolphinDirectory model does not
 * exist always in the desktop_icon_view, so we wait until it has been
 * instantiated.
 */
static void
delayed_init (NolphinDesktopIconView *desktop_icon_view)
{
	/* Keep track of the load time. */
	g_signal_connect_object (nolphin_view_get_model (NOLPHIN_VIEW (desktop_icon_view)),
				 "done_loading",
				 G_CALLBACK (done_loading), desktop_icon_view, 0);

	/* Monitor desktop directory. */
	desktop_icon_view->details->reload_desktop_timeout =
		g_timeout_add_seconds (RESCAN_TIMEOUT, do_desktop_rescan, desktop_icon_view);

	g_signal_handler_disconnect (desktop_icon_view,
				     desktop_icon_view->details->delayed_init_signal);

	desktop_icon_view->details->delayed_init_signal = 0;
}

static void
font_changed_callback (gpointer callback_data)
{
 	g_return_if_fail (NOLPHIN_IS_DESKTOP_ICON_VIEW (callback_data));

	nolphin_desktop_icon_view_update_icon_container_fonts (NOLPHIN_DESKTOP_ICON_VIEW (callback_data));
}

static void
nolphin_desktop_icon_view_update_icon_container_fonts (NolphinDesktopIconView *icon_view)
{
	NolphinIconContainer *icon_container;
	char *font;

	icon_container = get_icon_container (icon_view);
	g_assert (icon_container != NULL);

	font = g_settings_get_string (nolphin_desktop_preferences,
				      NOLPHIN_PREFERENCES_DESKTOP_FONT);

	nolphin_icon_container_set_font (icon_container, font);

	g_free (font);
}

static void
nolphin_desktop_icon_view_init (NolphinDesktopIconView *desktop_icon_view)
{
    desktop_icon_view->details = G_TYPE_INSTANCE_GET_PRIVATE (desktop_icon_view,
                                                              NOLPHIN_TYPE_DESKTOP_ICON_VIEW,
                                                              NolphinDesktopIconViewDetails);
}

static void
nolphin_desktop_icon_view_constructed (GObject *object)
{
    NolphinDesktopIconView *desktop_icon_view;
    NolphinIconContainer *icon_container;
    GtkAllocation allocation;
    GtkAdjustment *hadj, *vadj;

    desktop_icon_view = NOLPHIN_DESKTOP_ICON_VIEW (object);

    G_OBJECT_CLASS (nolphin_desktop_icon_view_parent_class)->constructed (G_OBJECT (desktop_icon_view));

    if (desktop_directory == NULL) {
        g_signal_connect_swapped (nolphin_preferences, "changed::" NOLPHIN_PREFERENCES_DESKTOP_IS_HOME_DIR,
                      G_CALLBACK(desktop_directory_changed_callback),
                      NULL);
        desktop_directory_changed_callback (NULL);
    }

    icon_container = get_icon_container (desktop_icon_view);
    nolphin_icon_container_set_use_drop_shadows (icon_container, TRUE);
    nolphin_icon_view_container_set_sort_desktop (NOLPHIN_ICON_VIEW_CONTAINER (icon_container), TRUE);

    /* Do a reload on the desktop if we don't have FAM, a smarter
     * way to keep track of the items on the desktop.
     */
    if (!nolphin_monitor_active ()) {
        desktop_icon_view->details->delayed_init_signal = g_signal_connect_object
            (desktop_icon_view, "begin_loading",
             G_CALLBACK (delayed_init), desktop_icon_view, 0);
    }

    nolphin_icon_container_set_is_fixed_size (icon_container, TRUE);
    nolphin_icon_container_set_is_desktop (icon_container, TRUE);

    nolphin_icon_container_set_store_layout_timestamps (icon_container, TRUE);

    /* Set allocation to be at 0, 0 */
    gtk_widget_get_allocation (GTK_WIDGET (icon_container), &allocation);
    allocation.x = 0;
    allocation.y = 0;
    gtk_widget_set_allocation (GTK_WIDGET (icon_container), &allocation);

    gtk_widget_queue_resize (GTK_WIDGET (icon_container));

    hadj = gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (icon_container));
    vadj = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (icon_container));

    gtk_adjustment_set_value (hadj, 0);
    gtk_adjustment_set_value (vadj, 0);

    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (desktop_icon_view),
                         GTK_SHADOW_NONE);

    nolphin_view_ignore_hidden_file_preferences
        (NOLPHIN_VIEW (desktop_icon_view));

    nolphin_view_set_show_foreign (NOLPHIN_VIEW (desktop_icon_view),
                    FALSE);

    /* Set our default layout mode */
    nolphin_icon_container_set_layout_mode (icon_container,
                         gtk_widget_get_direction (GTK_WIDGET(icon_container)) == GTK_TEXT_DIR_RTL ?
                         NOLPHIN_ICON_LAYOUT_T_B_R_L :
                         NOLPHIN_ICON_LAYOUT_T_B_L_R);

    g_signal_connect_object (icon_container, "middle_click",
                 G_CALLBACK (nolphin_desktop_icon_view_handle_middle_click), desktop_icon_view, 0);
    g_signal_connect_object (icon_container, "realize",
                 G_CALLBACK (desktop_icon_container_realize), desktop_icon_view, 0);

    g_signal_connect_swapped (nolphin_icon_view_preferences,
                  "changed::" NOLPHIN_PREFERENCES_ICON_VIEW_DEFAULT_ZOOM_LEVEL,
                  G_CALLBACK (default_zoom_level_changed),
                  desktop_icon_view);

    g_signal_connect_object (desktop_icon_view, "realize",
                             G_CALLBACK (realized_callback), desktop_icon_view, 0);
    g_signal_connect_object (desktop_icon_view, "unrealize",
                             G_CALLBACK (unrealized_callback), desktop_icon_view, 0);

    g_signal_connect_swapped (nolphin_desktop_preferences,
                  "changed::" NOLPHIN_PREFERENCES_DESKTOP_FONT,
                  G_CALLBACK (font_changed_callback),
                  desktop_icon_view);

    default_zoom_level_changed (desktop_icon_view);
    nolphin_desktop_icon_view_update_icon_container_fonts (desktop_icon_view);

    g_signal_connect_swapped (gnome_lockdown_preferences,
                  "changed::" NOLPHIN_PREFERENCES_LOCKDOWN_COMMAND_LINE,
                  G_CALLBACK (nolphin_view_update_menus),
                  desktop_icon_view);
}

static void
action_stretch_callback (GtkAction *action,
                         gpointer callback_data)
{
    g_assert (NOLPHIN_IS_ICON_VIEW (callback_data));

    nolphin_icon_container_show_stretch_handles (get_icon_container (callback_data));
}

static void
action_unstretch_callback (GtkAction *action,
                           gpointer callback_data)
{
    g_assert (NOLPHIN_IS_ICON_VIEW (callback_data));

    nolphin_icon_container_unstretch (get_icon_container (callback_data));
}

static void
action_empty_trash_conditional_callback (GtkAction *action,
					 gpointer data)
{
        g_assert (NOLPHIN_IS_VIEW (data));

	nolphin_file_operations_empty_trash (GTK_WIDGET (data));
}

static gboolean
trash_link_is_selection (NolphinView *view)
{
	GList *selection;
	NolphinDesktopLink *link;
	gboolean result;

	result = FALSE;

	selection = nolphin_view_get_selection (view);

	if ((g_list_length (selection) == 1) &&
	    NOLPHIN_IS_DESKTOP_ICON_FILE (selection->data)) {
		link = nolphin_desktop_icon_file_get_link (NOLPHIN_DESKTOP_ICON_FILE (selection->data));
		/* link may be NULL if the link was recently removed (unmounted) */
		if (link != NULL &&
		    nolphin_desktop_link_get_link_type (link) == NOLPHIN_DESKTOP_LINK_TRASH) {
			result = TRUE;
		}
		if (link) {
			g_object_unref (link);
		}
	}

	nolphin_file_list_free (selection);

	return result;
}

static void
real_update_menus (NolphinView *view)
{
    NolphinDesktopIconView *desktop_view;
    NolphinIconContainer *icon_container;
    char *label;
    gint selection_count;

	gboolean include_empty_trash;
	GtkAction *action;

	g_assert (NOLPHIN_IS_DESKTOP_ICON_VIEW (view));

	NOLPHIN_VIEW_CLASS (nolphin_desktop_icon_view_parent_class)->update_menus (view);

	desktop_view = NOLPHIN_DESKTOP_ICON_VIEW (view);

	/* Empty Trash */
	include_empty_trash = trash_link_is_selection (view);
	action = gtk_action_group_get_action (desktop_view->details->desktop_action_group,
					      NOLPHIN_ACTION_EMPTY_TRASH_CONDITIONAL);
	gtk_action_set_visible (action,
				include_empty_trash);
	if (include_empty_trash) {
		label = g_strdup (_("Papierkorb _leeren"));
		g_object_set (action , "label", label, NULL);
		gtk_action_set_sensitive (action,
					  !nolphin_trash_monitor_is_empty ());
		g_free (label);
	}

    selection_count = nolphin_view_get_selection_count (view);
    icon_container = get_icon_container (desktop_view);

    action = gtk_action_group_get_action (desktop_view->details->desktop_action_group,
                                          NOLPHIN_ACTION_STRETCH);
    gtk_action_set_sensitive (action,
                              selection_count == 1 &&
                              icon_container != NULL &&
                              !nolphin_icon_container_has_stretch_handles (icon_container));
    gtk_action_set_visible (action, TRUE);

    action = gtk_action_group_get_action (desktop_view->details->desktop_action_group,
                                          NOLPHIN_ACTION_UNSTRETCH);
    g_object_set (action, "label",
                  (selection_count > 1) ?
                      _("_Originalgröße der Symbole wiederherstellen")
                    : _("Symbolgröße _wiederherstellen"),
                  NULL);
    gtk_action_set_sensitive (action,
                              icon_container != NULL &&
                              nolphin_icon_container_is_stretched (icon_container));
    gtk_action_set_visible (action, TRUE);
}

static const GtkActionEntry desktop_view_entries[] = {
    /* name, stock id */         { "Stretch", NULL,
    /* label, accelerator */       N_("Symbolgröße ändern …"), NULL,
    /* tooltip */                  N_("Das gewählte Symbol Größenveränderbar machen"),
                                 G_CALLBACK (action_stretch_callback) },
    /* name, stock id */         { "Unstretch", NULL,
    /* label, accelerator */       N_("_Originalgröße der Symbole wiederherstellen"), NULL,
    /* tooltip */                  N_("Alle gewählten Symbole auf Originalgröße zurücksetzen"),
                                 G_CALLBACK (action_unstretch_callback) },
    /* name, stock id */         { "Empty Trash Conditional", NULL,
    /* label, accelerator */       N_("Papierkorb leeren"), NULL,
    /* tooltip */                  N_("Alle Objekte im Papierkorb löschen"),
                                 G_CALLBACK (action_empty_trash_conditional_callback) }
};

static void
real_merge_menus (NolphinView *view)
{
	NolphinDesktopIconView *desktop_view;
	GtkUIManager *ui_manager;
	GtkActionGroup *action_group;

	NOLPHIN_VIEW_CLASS (nolphin_desktop_icon_view_parent_class)->merge_menus (view);

	desktop_view = NOLPHIN_DESKTOP_ICON_VIEW (view);

	ui_manager = nolphin_view_get_ui_manager (view);

	action_group = gtk_action_group_new ("DesktopViewActions");
	gtk_action_group_set_translation_domain (action_group, GETTEXT_PACKAGE);
	desktop_view->details->desktop_action_group = action_group;
	gtk_action_group_add_actions (action_group,
				      desktop_view_entries, G_N_ELEMENTS (desktop_view_entries),
				      view);

	gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);
	g_object_unref (action_group); /* owned by ui manager */

	desktop_view->details->desktop_merge_id =
		gtk_ui_manager_add_ui_from_resource (ui_manager, "/org/nolphin/nolphin-desktop-icon-view-ui.xml", NULL);
}

static NolphinView *
nolphin_desktop_icon_view_create (NolphinWindowSlot *slot)
{
	NolphinIconView *view;

	view = g_object_new (NOLPHIN_TYPE_DESKTOP_ICON_VIEW,
			     "window-slot", slot,
			     "supports-zooming", FALSE,
			     "supports-auto-layout", FALSE,
			     "is-desktop", TRUE,
			     "supports-keep-aligned", TRUE,
			     "supports-labels-beside-icons", FALSE,
			     NULL);
	return NOLPHIN_VIEW (view);
}

static gboolean
nolphin_desktop_icon_view_supports_uri (const char *uri,
				   GFileType file_type,
				   const char *mime_type)
{
	if (g_str_has_prefix (uri, EEL_DESKTOP_URI)) {
		return TRUE;
	}

	return FALSE;
}

static NolphinViewInfo nolphin_desktop_icon_view = {
	(char *)NOLPHIN_DESKTOP_ICON_VIEW_ID,
	(char *)"Desktop View",
	(char *)"_Desktop",
	(char *)N_("Die Schreibtischansicht stieß auf einen Fehler."),
	(char *)N_("Die Schreibtischansicht stieß beim Starten auf einen Fehler."),
	(char *)"Display this location with the desktop view.",
	nolphin_desktop_icon_view_create,
	nolphin_desktop_icon_view_supports_uri
};

void
nolphin_desktop_icon_view_register (void)
{
	nolphin_desktop_icon_view.error_label = _(nolphin_desktop_icon_view.error_label);
	nolphin_desktop_icon_view.startup_error_label = _(nolphin_desktop_icon_view.startup_error_label);

	nolphin_view_factory_register (&nolphin_desktop_icon_view);
}

