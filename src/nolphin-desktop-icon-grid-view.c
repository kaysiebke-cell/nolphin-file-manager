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

#include "nolphin-desktop-icon-grid-view.h"

#include "nolphin-actions.h"
#include "nolphin-application.h"
#include "nolphin-desktop-manager.h"
#include "nolphin-desktop-window.h"
#include "nolphin-desktop-overlay.h"
#include "nolphin-icon-view-grid-container.h"
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

struct NolphinDesktopIconGridViewDetails
{
	GdkWindow *root_window;
	GtkActionGroup *desktop_action_group;
	guint desktop_merge_id;

	/* For the desktop rescanning
	 */
	gulong delayed_init_signal;
	guint reload_desktop_timeout;
	gboolean pending_rescan;
    gboolean updating_menus;
};

typedef struct {
    const char *action;
    const char *metadata_text;
    const NolphinFileSortType sort_type;
} DesktopSortCriterion;

static const DesktopSortCriterion sort_criteria[] = {
    {
        "Desktop Sort by Name",
        "name",
        NOLPHIN_FILE_SORT_BY_DISPLAY_NAME
    },
    {
        "Desktop Sort by Size",
        "size",
        NOLPHIN_FILE_SORT_BY_SIZE
    },
    {
        "Desktop Sort by Type",
        "detailed_type",
        NOLPHIN_FILE_SORT_BY_DETAILED_TYPE
    },
    {
        "Desktop Sort by Date",
        "modification date",
        NOLPHIN_FILE_SORT_BY_MTIME
    },
    {
        "Desktop Sort by Extension",
        "extension",
        NOLPHIN_FILE_SORT_BY_EXTENSION
    }
};

static void     real_merge_menus                                  (NolphinView        *view);
static void     real_update_menus                                 (NolphinView        *view);
static void     nolphin_desktop_icon_grid_view_update_icon_container_fonts  (NolphinDesktopIconGridView      *view);
static void     font_changed_callback                             (gpointer                callback_data);
static void     nolphin_desktop_icon_grid_view_constructed (GObject *object);

G_DEFINE_TYPE (NolphinDesktopIconGridView, nolphin_desktop_icon_grid_view, NOLPHIN_TYPE_ICON_VIEW)

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
update_margins (NolphinDesktopIconGridView *icon_view)
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
    NolphinDesktopIconGridView *icon_view;

    icon_view = NOLPHIN_DESKTOP_ICON_GRID_VIEW (data);

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
	return NOLPHIN_DESKTOP_ICON_GRID_VIEW_IID;
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
nolphin_desktop_icon_grid_view_remove_file (NolphinView *view, NolphinFile *file, NolphinDirectory *directory)
{
    /* This used to assert that 'directory == nolphin_view_get_model (view)', but that
     * resulted in a lot of crash reports (bug #352592). I don't see how that trace happens.
     * It seems that somehow we get a files_changed event sent to the view from a directory
     * that isn't the model, but the code disables the monitor and signal callback handlers when
     * changing directories. Maybe we can get some more information when this happens.
     * Further discussion in bug #368178.
     */
    if (directory != nolphin_view_get_model (view)) {
        char *file_uri, *dir_uri, *model_uri;
        file_uri = nolphin_file_get_uri (file);
        dir_uri = nolphin_directory_get_uri (directory);
        model_uri = nolphin_directory_get_uri (nolphin_view_get_model (view));
        g_warning ("nolphin_icon_view_remove_file() - directory not icon view model, shouldn't happen.\n"
               "file: %p:%s, dir: %p:%s, model: %p:%s, view loading: %d\n"
               "If you see this, please add this info to http://bugzilla.gnome.org/show_bug.cgi?id=368178",
               file, file_uri, directory, dir_uri, nolphin_view_get_model (view), model_uri, nolphin_view_get_loading (view));
        g_free (file_uri);
        g_free (dir_uri);
        g_free (model_uri);
    }

    if (nolphin_icon_container_remove (get_icon_container (view), NOLPHIN_ICON_CONTAINER_ICON_DATA (file))) {
        nolphin_file_unref (file);
    }
}

static void
nolphin_desktop_icon_grid_view_add_file (NolphinView *view, NolphinFile *file, NolphinDirectory *directory)
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
nolphin_desktop_icon_grid_view_file_changed (NolphinView *view, NolphinFile *file, NolphinDirectory *directory)
{
    NolphinIconView *icon_view;

    g_assert (directory == nolphin_view_get_model (view));

    g_return_if_fail (view != NULL);
    icon_view = NOLPHIN_ICON_VIEW (view);


    if (!should_show_file_on_current_monitor (view, file)) {
        nolphin_desktop_icon_grid_view_remove_file (view, file, directory);
    } else {
        nolphin_icon_container_request_update (get_icon_container (icon_view),
                                            NOLPHIN_ICON_CONTAINER_ICON_DATA (file));
    }
}

static void
unrealized_callback (GtkWidget *widget, NolphinDesktopIconGridView *desktop_icon_grid_view)
{
  g_return_if_fail (desktop_icon_grid_view->details->root_window != NULL);

  gdk_window_remove_filter (desktop_icon_grid_view->details->root_window,
                            gdk_filter_func,
                            desktop_icon_grid_view);

  desktop_icon_grid_view->details->root_window = NULL;
}

static void
realized_callback (GtkWidget *widget, NolphinDesktopIconGridView *desktop_icon_grid_view)
{
  GdkWindow *root_window;
  GdkScreen *screen;

  g_return_if_fail (desktop_icon_grid_view->details->root_window == NULL);

  screen = gtk_widget_get_screen (widget);

  root_window = gdk_screen_get_root_window (screen);

  desktop_icon_grid_view->details->root_window = root_window;

  update_margins (desktop_icon_grid_view);

  /* Setup the property filter */
  gdk_window_set_events (root_window, GDK_PROPERTY_CHANGE_MASK);
  gdk_window_add_filter (root_window,
                         gdk_filter_func,
                         desktop_icon_grid_view);
}

static void
nolphin_desktop_icon_grid_view_dispose (GObject *object)
{
	NolphinDesktopIconGridView *icon_view;
	GtkUIManager *ui_manager;

	icon_view = NOLPHIN_DESKTOP_ICON_GRID_VIEW (object);

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

	g_signal_handlers_disconnect_by_func (nolphin_desktop_preferences,
					      font_changed_callback,
					      icon_view);

	g_signal_handlers_disconnect_by_func (nolphin_preferences,
					      desktop_directory_changed_callback,
					      NULL);

	g_signal_handlers_disconnect_by_func (gnome_lockdown_preferences,
					      nolphin_view_update_menus,
					      icon_view);

	G_OBJECT_CLASS (nolphin_desktop_icon_grid_view_parent_class)->dispose (object);
}

static void
nolphin_desktop_icon_grid_view_class_init (NolphinDesktopIconGridViewClass *class)
{
	NolphinViewClass *vclass;

	vclass = NOLPHIN_VIEW_CLASS (class);

    G_OBJECT_CLASS (class)->dispose = nolphin_desktop_icon_grid_view_dispose;
	G_OBJECT_CLASS (class)->constructed = nolphin_desktop_icon_grid_view_constructed;

    NOLPHIN_ICON_VIEW_CLASS (class)->use_grid_container = TRUE;

	vclass->merge_menus = real_merge_menus;
	vclass->update_menus = real_update_menus;
	vclass->get_view_id = real_get_id;
    vclass->add_file = nolphin_desktop_icon_grid_view_add_file;
    vclass->file_changed = nolphin_desktop_icon_grid_view_file_changed;
    vclass->remove_file = nolphin_desktop_icon_grid_view_remove_file;

#if GTK_CHECK_VERSION(3, 21, 0)
	GtkWidgetClass *wclass = GTK_WIDGET_CLASS (class);
	gtk_widget_class_set_css_name (wclass, "nolphin-desktop-icon-view");
#endif
	g_type_class_add_private (class, sizeof (NolphinDesktopIconGridViewDetails));
}

static void
nolphin_desktop_icon_grid_view_handle_middle_click (NolphinIconContainer *icon_container,
						GdkEventButton *event,
						NolphinDesktopIconGridView *desktop_icon_grid_view)
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
                                NolphinDesktopIconGridView *desktop_icon_grid_view)
{
    GdkWindow *bin_window;
    GdkRGBA transparent = { 0, 0, 0, 0 };

    bin_window = gtk_layout_get_bin_window (GTK_LAYOUT (widget));
    gdk_window_set_background_rgba (bin_window, &transparent);
}

static gboolean
do_desktop_rescan (gpointer data)
{
	NolphinDesktopIconGridView *desktop_icon_grid_view;
	struct stat buf;

	desktop_icon_grid_view = NOLPHIN_DESKTOP_ICON_GRID_VIEW (data);
	if (desktop_icon_grid_view->details->pending_rescan) {
		return TRUE;
	}

	if (stat (desktop_directory, &buf) == -1) {
		return TRUE;
	}

	if (buf.st_ctime == desktop_dir_modify_time) {
		return TRUE;
	}

	desktop_icon_grid_view->details->pending_rescan = TRUE;

	nolphin_directory_force_reload
		(nolphin_view_get_model (NOLPHIN_VIEW (desktop_icon_grid_view)));

	return TRUE;
}

static void
done_loading (NolphinDirectory *model,
	      NolphinDesktopIconGridView *desktop_icon_grid_view)
{
	struct stat buf;

	desktop_icon_grid_view->details->pending_rescan = FALSE;
	if (stat (desktop_directory, &buf) == -1) {
		return;
	}

	desktop_dir_modify_time = buf.st_ctime;
}

/* This function is used because the NolphinDirectory model does not
 * exist always in the desktop_icon_grid_view, so we wait until it has been
 * instantiated.
 */
static void
delayed_init (NolphinDesktopIconGridView *desktop_icon_grid_view)
{
	/* Keep track of the load time. */
	g_signal_connect_object (nolphin_view_get_model (NOLPHIN_VIEW (desktop_icon_grid_view)),
				 "done_loading",
				 G_CALLBACK (done_loading), desktop_icon_grid_view, 0);

	/* Monitor desktop directory. */
	desktop_icon_grid_view->details->reload_desktop_timeout =
		g_timeout_add_seconds (RESCAN_TIMEOUT, do_desktop_rescan, desktop_icon_grid_view);

	g_signal_handler_disconnect (desktop_icon_grid_view,
				     desktop_icon_grid_view->details->delayed_init_signal);

	desktop_icon_grid_view->details->delayed_init_signal = 0;
    desktop_icon_grid_view->details->updating_menus = FALSE;
}

static void
font_changed_callback (gpointer callback_data)
{
 	g_return_if_fail (NOLPHIN_IS_DESKTOP_ICON_GRID_VIEW (callback_data));

	nolphin_desktop_icon_grid_view_update_icon_container_fonts (NOLPHIN_DESKTOP_ICON_GRID_VIEW (callback_data));
}

static void
nolphin_desktop_icon_grid_view_update_icon_container_fonts (NolphinDesktopIconGridView *icon_view)
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
nolphin_desktop_icon_grid_view_init (NolphinDesktopIconGridView *desktop_icon_grid_view)
{
    desktop_icon_grid_view->details = G_TYPE_INSTANCE_GET_PRIVATE (desktop_icon_grid_view,
                                                                   NOLPHIN_TYPE_DESKTOP_ICON_GRID_VIEW,
                                                                   NolphinDesktopIconGridViewDetails);
}

static void
nolphin_desktop_icon_grid_view_constructed (GObject *object)
{
    NolphinDesktopIconGridView *desktop_icon_grid_view;
    NolphinIconContainer *icon_container;
    GtkAllocation allocation;
    GtkAdjustment *hadj, *vadj;

    desktop_icon_grid_view = NOLPHIN_DESKTOP_ICON_GRID_VIEW (object);

    G_OBJECT_CLASS (nolphin_desktop_icon_grid_view_parent_class)->constructed (G_OBJECT (desktop_icon_grid_view));

    if (desktop_directory == NULL) {
        g_signal_connect_swapped (nolphin_preferences, "changed::" NOLPHIN_PREFERENCES_DESKTOP_IS_HOME_DIR,
                      G_CALLBACK(desktop_directory_changed_callback),
                      NULL);
        desktop_directory_changed_callback (NULL);
    }

    icon_container = get_icon_container (desktop_icon_grid_view);
    nolphin_icon_container_set_use_drop_shadows (icon_container, TRUE);
    nolphin_icon_view_grid_container_set_sort_desktop (NOLPHIN_ICON_VIEW_GRID_CONTAINER (icon_container), TRUE);

    /* Do a reload on the desktop if we don't have FAM, a smarter
     * way to keep track of the items on the desktop.
     */
    if (!nolphin_monitor_active ()) {
        desktop_icon_grid_view->details->delayed_init_signal = g_signal_connect_object
            (desktop_icon_grid_view, "begin_loading",
             G_CALLBACK (delayed_init), desktop_icon_grid_view, 0);
    }

    nolphin_icon_container_set_is_fixed_size (icon_container, TRUE);
    nolphin_icon_container_set_is_desktop (icon_container, TRUE);
    nolphin_icon_container_set_keep_aligned (icon_container, TRUE);

    NOLPHIN_ICON_VIEW_GRID_CONTAINER (icon_container)->horizontal = FALSE;

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

    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (desktop_icon_grid_view),
                         GTK_SHADOW_NONE);

    nolphin_view_ignore_hidden_file_preferences
        (NOLPHIN_VIEW (desktop_icon_grid_view));

    nolphin_view_set_show_foreign (NOLPHIN_VIEW (desktop_icon_grid_view),
                    FALSE);

    g_signal_connect_object (icon_container, "middle_click",
                 G_CALLBACK (nolphin_desktop_icon_grid_view_handle_middle_click), desktop_icon_grid_view, 0);
    g_signal_connect_object (icon_container, "realize",
                 G_CALLBACK (desktop_icon_container_realize), desktop_icon_grid_view, 0);

    g_signal_connect_object (desktop_icon_grid_view, "realize",
                             G_CALLBACK (realized_callback), desktop_icon_grid_view, 0);
    g_signal_connect_object (desktop_icon_grid_view, "unrealize",
                             G_CALLBACK (unrealized_callback), desktop_icon_grid_view, 0);

    g_signal_connect_swapped (nolphin_desktop_preferences,
                  "changed::" NOLPHIN_PREFERENCES_DESKTOP_FONT,
                  G_CALLBACK (font_changed_callback),
                  desktop_icon_grid_view);

    nolphin_desktop_icon_grid_view_update_icon_container_fonts (desktop_icon_grid_view);

    g_signal_connect_swapped (gnome_lockdown_preferences,
                  "changed::" NOLPHIN_PREFERENCES_LOCKDOWN_COMMAND_LINE,
                  G_CALLBACK (nolphin_view_update_menus),
                  desktop_icon_grid_view);
}

static void
action_empty_trash_conditional_callback (GtkAction *action,
                                         gpointer   data)
{
    g_assert (NOLPHIN_IS_VIEW (data));

    nolphin_file_operations_empty_trash (GTK_WIDGET (data));
}

static void
clear_orphan_states (NolphinDesktopIconGridView *view)
{
    GList *icons;

    for (icons = get_icon_container (view)->details->icons; icons != NULL; icons = icons->next) {
        NolphinFile *file;
        NolphinIcon *icon;

        icon = icons->data;

        file = NOLPHIN_FILE (icon->data);
        nolphin_file_set_is_desktop_orphan (file, FALSE);
    }
}

static void
action_auto_arrange_callback (GtkAction *action,
                              NolphinDesktopIconGridView *view)
{
    gboolean new;

    g_assert (NOLPHIN_IS_VIEW (view));

    if (view->details->updating_menus) {
        return;
    }

    new = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));

    clear_orphan_states (view);
    nolphin_icon_view_set_sort_reversed (NOLPHIN_ICON_VIEW (view), FALSE, TRUE);

    nolphin_icon_container_set_auto_layout (get_icon_container (view), new);

    nolphin_icon_container_store_layout_timestamps_now (get_icon_container (view));
}

static void
action_reverse_sort_callback (GtkAction               *action,
                              NolphinDesktopIconGridView *view)
{
    NolphinIconContainer *container;
    gboolean new;

    g_assert (NOLPHIN_IS_VIEW (view));

    if (view->details->updating_menus) {
        return;
    }

    container = get_icon_container (view);
    new = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));

    clear_orphan_states (view);

    container->details->needs_resort = TRUE;

    nolphin_icon_view_set_sort_reversed (NOLPHIN_ICON_VIEW (view), new, TRUE);
    nolphin_icon_container_sort (get_icon_container (view));
    nolphin_icon_container_redo_layout (container);

    nolphin_view_update_menus (NOLPHIN_VIEW (view));

    nolphin_icon_container_store_layout_timestamps_now (get_icon_container (view));
}

static void
set_sort_type (NolphinDesktopIconGridView *view,
               GtkAction               *action,
               NolphinFileSortType         type)
{
    if (view->details->updating_menus) {
        return;
    }

    clear_orphan_states (view);

    nolphin_icon_view_set_sort_reversed (NOLPHIN_ICON_VIEW (view), FALSE, TRUE);
    nolphin_icon_view_set_sort_criterion_by_sort_type (NOLPHIN_ICON_VIEW (view), type);

    nolphin_icon_container_redo_layout (get_icon_container (view));

    nolphin_view_update_menus (NOLPHIN_VIEW (view));

    nolphin_icon_container_store_layout_timestamps_now (get_icon_container (view));
}

static void
action_show_overlay (GtkAction               *action,
                     NolphinDesktopIconGridView *view)
{
    nolphin_desktop_manager_show_desktop_overlay (nolphin_desktop_manager_get (),
                                               nolphin_desktop_utils_get_monitor_for_widget (GTK_WIDGET (view)));
}

static void
set_direction (NolphinDesktopIconGridView *view,
               gboolean                 horizontal)
{
    NolphinFile *file;
    NolphinIconContainer *container;

    if (view->details->updating_menus) {
        return;
    }

    clear_orphan_states (view);

    container = get_icon_container (view);
    file = nolphin_view_get_directory_as_file (NOLPHIN_VIEW (view));

    nolphin_icon_container_set_horizontal_layout (container, horizontal);
    container->details->needs_resort = TRUE;

    nolphin_icon_view_set_sort_reversed (NOLPHIN_ICON_VIEW (view), FALSE, TRUE);
    nolphin_icon_container_sort (get_icon_container (view));
    nolphin_icon_container_redo_layout (get_icon_container (view));

    nolphin_icon_view_set_directory_horizontal_layout (NOLPHIN_ICON_VIEW (view), file, horizontal);

    nolphin_view_update_menus (NOLPHIN_VIEW (view));

    nolphin_icon_container_store_layout_timestamps_now (get_icon_container (view));
}

static void
action_layout_direction_callback (GtkAction               *action,
                                  GtkRadioAction          *current,
                                  NolphinDesktopIconGridView *view)
{
    NolphinDesktopLayoutDirection direction;

    direction = gtk_radio_action_get_current_value (current);

    switch (direction) {
        case DESKTOP_ARRANGE_HORIZONTAL:
            set_direction (view, TRUE);
            break;
        case DESKTOP_ARRANGE_VERTICAL:
            set_direction (view, FALSE);
            break;
        default:
            break;
    }
}

static void
action_desktop_size_callback (GtkAction               *action,
                              GtkRadioAction          *current,
                              NolphinDesktopIconGridView *view)
{
    NolphinZoomLevel level;
    NolphinIconContainer *container;

    if (view->details->updating_menus) {
        return;
    }

    container = get_icon_container (view);

    level = gtk_radio_action_get_current_value (current);

    nolphin_view_zoom_to_level (NOLPHIN_VIEW (view), level);

    clear_orphan_states (view);

    /* TODO: Instead of switching back to defaults, re-align the existing icons
     * into the new slots.  This is complicated, due to how the redo_layout_internal
     * function works. */

    container->details->needs_resort = TRUE;
    container->details->auto_layout = TRUE;

    nolphin_icon_container_redo_layout (container);

    nolphin_view_update_menus (NOLPHIN_VIEW (view));

    nolphin_icon_container_store_layout_timestamps_now (get_icon_container (view));
}

static void
action_sort_order_callback (GtkAction               *action,
                            GtkRadioAction          *current,
                            NolphinDesktopIconGridView *view)
{
    NolphinFileSortType type;

    g_assert (NOLPHIN_IS_VIEW (view));

    if (view->details->updating_menus) {
        return;
    }

    type = gtk_radio_action_get_current_value (current);

    set_sort_type (view, action, type);
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
	NolphinDesktopIconGridView *desktop_view;
    NolphinIconContainer *container;
    NolphinFile *file;
	char *label;
	gboolean include_empty_trash;
    gboolean horizontal_layout;
    gboolean auto_arrange, reversed;
    gint i;

    GtkUIManager *ui_manager;
	GtkAction *action;
    GList *groups, *l;

	g_assert (NOLPHIN_IS_DESKTOP_ICON_GRID_VIEW (view));

    container = get_icon_container (view);

    desktop_view = NOLPHIN_DESKTOP_ICON_GRID_VIEW (view);
    desktop_view->details->updating_menus = TRUE;

	NOLPHIN_VIEW_CLASS (nolphin_desktop_icon_grid_view_parent_class)->update_menus (view);

    /* Empty Trash */
    include_empty_trash = trash_link_is_selection (view);
    action = gtk_action_group_get_action (desktop_view->details->desktop_action_group,
                                          NOLPHIN_ACTION_EMPTY_TRASH_CONDITIONAL);
    gtk_action_set_visible (action, include_empty_trash);

    if (include_empty_trash) {
        label = g_strdup (_("Papierkorb _leeren"));
        g_object_set (action , "label", label, NULL);
        gtk_action_set_sensitive (action, !nolphin_trash_monitor_is_empty ());
        g_free (label);
    }

    file = nolphin_view_get_directory_as_file (NOLPHIN_VIEW (desktop_view));

    horizontal_layout = nolphin_icon_view_get_directory_horizontal_layout (NOLPHIN_ICON_VIEW (desktop_view), file);

    action = gtk_action_group_get_action (desktop_view->details->desktop_action_group,
                                          "Horizontal Layout");
    gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), horizontal_layout);

    action = gtk_action_group_get_action (desktop_view->details->desktop_action_group,
                                          "Vertical Layout");
    gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), !horizontal_layout);

    auto_arrange = nolphin_icon_container_is_auto_layout (container);

    action = gtk_action_group_get_action (desktop_view->details->desktop_action_group,
                                          "Desktop Autoarrange");
    gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), auto_arrange);

    reversed = nolphin_icon_view_get_directory_sort_reversed (NOLPHIN_ICON_VIEW (desktop_view), file);

    action = gtk_action_group_get_action (desktop_view->details->desktop_action_group,
                                          "Desktop Reverse Sort");

    gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), reversed);

    gchar *order;

    order = nolphin_icon_view_get_directory_sort_by (NOLPHIN_ICON_VIEW (desktop_view), file);

    for (i = 0; i < G_N_ELEMENTS (sort_criteria); i++) {
        action = gtk_action_group_get_action (desktop_view->details->desktop_action_group,
                                              sort_criteria[i].action);

        if (g_strcmp0 (order, sort_criteria[i].metadata_text) == 0) {
            gtk_radio_action_set_current_value (GTK_RADIO_ACTION (action),
                                                sort_criteria[i].sort_type);
            break;
        }
    }

    g_free (order);

    /* Update zoom radio */

    switch (nolphin_view_get_zoom_level (NOLPHIN_VIEW (desktop_view))) {
        case NOLPHIN_ZOOM_LEVEL_SMALLER:
            action = gtk_action_group_get_action (desktop_view->details->desktop_action_group,
                                                  "Desktop Smaller");
            break;
        case NOLPHIN_ZOOM_LEVEL_SMALL:
            action = gtk_action_group_get_action (desktop_view->details->desktop_action_group,
                                                  "Desktop Small");
            break;
        case NOLPHIN_ZOOM_LEVEL_LARGE:
            action = gtk_action_group_get_action (desktop_view->details->desktop_action_group,
                                                  "Desktop Large");
            break;
        case NOLPHIN_ZOOM_LEVEL_LARGER:
            action = gtk_action_group_get_action (desktop_view->details->desktop_action_group,
                                                  "Desktop Larger");
            break;
        case NOLPHIN_ZOOM_LEVEL_STANDARD:
        case NOLPHIN_ZOOM_LEVEL_NULL:
        case NOLPHIN_ZOOM_LEVEL_SMALLEST:
        case NOLPHIN_ZOOM_LEVEL_LARGEST:
        default:
            action = gtk_action_group_get_action (desktop_view->details->desktop_action_group,
                                                  "Desktop Normal");
            break;
    }

    gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), TRUE);

    desktop_view->details->updating_menus = FALSE;

    ui_manager = nolphin_view_get_ui_manager (view);
    groups = gtk_ui_manager_get_action_groups (ui_manager);

    /* These actions are set up in NolphinIconView, but we want to replace it all with the
     * desktop submenu.  This could be avoided by creating an additional subclass of NolphinIconView
     * to implement these things, but this is simpler for now.
     */
    for (l = groups; l != NULL; l = l->next) {
        GtkActionGroup *group;

        group = GTK_ACTION_GROUP (l->data);

        action = gtk_action_group_get_action (group, NOLPHIN_ACTION_CLEAN_UP);
        if (action != NULL) {
            gtk_action_set_visible (action, FALSE);
        }
        action = gtk_action_group_get_action (group, NOLPHIN_ACTION_KEEP_ALIGNED);
        if (action != NULL) {
            gtk_action_set_visible (action, FALSE);
        }
        action = gtk_action_group_get_action (group, NOLPHIN_ACTION_ARRANGE_ITEMS);
        if (action != NULL) {
            gtk_action_set_visible (action, FALSE);
        }
    }
}

static const GtkToggleActionEntry desktop_grid_toggle_entries[] = {
  /* name, stock id */      { "Desktop Autoarrange", NULL,
  /* label, accelerator */    N_("Automatisch anordnen"), NULL,
  /* tooltip */               NULL,
                              G_CALLBACK (action_auto_arrange_callback),
                              0 },
  /* name, stock id */      { "Desktop Reverse Sort", NULL,
  /* label, accelerator */    N_("Sortierung umkehren"), NULL,
  /* tooltip */               NULL,
                              G_CALLBACK (action_reverse_sort_callback),
                              0 }
};

static const GtkRadioActionEntry desktop_size_radio_entries[] = {
  { "Desktop Smaller", NULL,
    N_("Kleiner"), NULL,
    NULL,
    NOLPHIN_ZOOM_LEVEL_SMALLER },
  { "Desktop Small", NULL,
    N_("Klein"), NULL,
    NULL,
    NOLPHIN_ZOOM_LEVEL_SMALL },
  { "Desktop Normal", NULL,
    N_("Normal"), NULL,
    NULL,
    NOLPHIN_ZOOM_LEVEL_STANDARD },
  { "Desktop Large", NULL,
    N_("Groß"), NULL,
    NULL,
    NOLPHIN_ZOOM_LEVEL_LARGE },
  { "Desktop Larger", NULL,
    N_("Größer"), NULL,
    NULL,
    NOLPHIN_ZOOM_LEVEL_LARGER },
};

static const GtkRadioActionEntry desktop_direction_radio_entries[] = {
    { "Vertical Layout", NULL,
      N_("_Senkrecht"), NULL,
      NULL,
      DESKTOP_ARRANGE_VERTICAL },
    { "Horizontal Layout", NULL,
      N_("_Horizontal"), NULL,
      NULL,
      DESKTOP_ARRANGE_HORIZONTAL }
};

static const GtkRadioActionEntry desktop_sort_radio_entries[] = {
    { "Desktop Sort by Name", NULL,
      N_("Name"), NULL,
      NULL,
      NOLPHIN_FILE_SORT_BY_DISPLAY_NAME },
    { "Desktop Sort by Size", NULL,
      N_("Größe"), NULL,
      NULL,
      NOLPHIN_FILE_SORT_BY_SIZE },
    { "Desktop Sort by Type", NULL,
      N_("Dateityp"), NULL,
      NULL,
      NOLPHIN_FILE_SORT_BY_DETAILED_TYPE },
    { "Desktop Sort by Date", NULL,
      N_("Datum"), NULL,
      NULL,
      NOLPHIN_FILE_SORT_BY_MTIME },
    { "Desktop Sort by Extension", NULL,
      N_("Erweiterung"), NULL,
      NULL,
      NOLPHIN_FILE_SORT_BY_EXTENSION }
};

static const GtkActionEntry desktop_grid_entries[] = {
    /* name, stock id, label */  { "Desktop Submenu", NULL, N_("_Sortieren") },
    /* name, stock id, label */  { "Desktop Zoom", NULL, N_("Symbol_größe") },
    /* name, stock id */
    { "Empty Trash Conditional", NULL,
      /* label, accelerator */
      N_("Papierkorb leeren"), NULL,
      /* tooltip */
      N_("Alle Objekte im Papierkorb löschen"),
      G_CALLBACK (action_empty_trash_conditional_callback) },

    { "Show Desktop Overlay", NULL,
      N_("_Anpassen"), NULL,
      N_("Schreibtischanordnung für diesen Bildschirm anpassen"),
      G_CALLBACK (action_show_overlay) },
};

static void
real_merge_menus (NolphinView *view)
{
	NolphinDesktopIconGridView *desktop_view;
    GtkUIManager *ui_manager;
	GtkActionGroup *action_group;

	NOLPHIN_VIEW_CLASS (nolphin_desktop_icon_grid_view_parent_class)->merge_menus (view);

	desktop_view = NOLPHIN_DESKTOP_ICON_GRID_VIEW (view);

	ui_manager = nolphin_view_get_ui_manager (view);

	action_group = gtk_action_group_new ("DesktopViewActions");
	gtk_action_group_set_translation_domain (action_group, GETTEXT_PACKAGE);
	desktop_view->details->desktop_action_group = action_group;

    gtk_action_group_add_actions (action_group,
                                  desktop_grid_entries,
                                  G_N_ELEMENTS (desktop_grid_entries),
                                  view);

    gtk_action_group_add_toggle_actions (action_group,
                                         desktop_grid_toggle_entries,
                                         G_N_ELEMENTS (desktop_grid_toggle_entries),
                                         view);

    gtk_action_group_add_radio_actions (action_group,
                                        desktop_size_radio_entries,
                                        G_N_ELEMENTS (desktop_size_radio_entries),
                                        -1,
                                        G_CALLBACK (action_desktop_size_callback),
                                        view);

    gtk_action_group_add_radio_actions (action_group,
                                        desktop_direction_radio_entries,
                                        G_N_ELEMENTS (desktop_direction_radio_entries),
                                        -1,
                                        G_CALLBACK (action_layout_direction_callback),
                                        view);

    gtk_action_group_add_radio_actions (action_group,
                                        desktop_sort_radio_entries,
                                        G_N_ELEMENTS (desktop_sort_radio_entries),
                                        -1,
                                        G_CALLBACK (action_sort_order_callback),
                                        view);

    gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);
    g_object_unref (action_group); /* owned by ui manager */

	desktop_view->details->desktop_merge_id =
		gtk_ui_manager_add_ui_from_resource (ui_manager, "/org/nolphin/nolphin-desktop-icon-grid-view-ui.xml", NULL);
}

static NolphinView *
nolphin_desktop_icon_grid_view_create (NolphinWindowSlot *slot)
{
	NolphinIconView *view;

	view = g_object_new (NOLPHIN_TYPE_DESKTOP_ICON_GRID_VIEW,
			     "window-slot", slot,
			     "supports-zooming", TRUE,
			     "supports-auto-layout", TRUE,
			     "is-desktop", TRUE,
			     "supports-keep-aligned", TRUE,
			     "supports-labels-beside-icons", FALSE,
			     NULL);
	return NOLPHIN_VIEW (view);
}

static gboolean
nolphin_desktop_icon_grid_view_supports_uri (const char *uri,
				   GFileType file_type,
				   const char *mime_type)
{
	if (g_str_has_prefix (uri, EEL_DESKTOP_URI)) {
		return TRUE;
	}

	return FALSE;
}

static NolphinViewInfo nolphin_desktop_icon_grid_view = {
	(char *)NOLPHIN_DESKTOP_ICON_GRID_VIEW_IID,
	(char *)"Desktop Grid View",
	(char *)"_Desktop",
	(char *)N_("Die Schreibtischansicht stieß auf einen Fehler."),
	(char *)N_("Die Schreibtischansicht stieß beim Starten auf einen Fehler."),
	(char *)"Display this location with the desktop grid view.",
	nolphin_desktop_icon_grid_view_create,
	nolphin_desktop_icon_grid_view_supports_uri
};

void
nolphin_desktop_icon_grid_view_register (void)
{
	nolphin_desktop_icon_grid_view.error_label = _(nolphin_desktop_icon_grid_view.error_label);
	nolphin_desktop_icon_grid_view.startup_error_label = _(nolphin_desktop_icon_grid_view.startup_error_label);

	nolphin_view_factory_register (&nolphin_desktop_icon_grid_view);
}

/* CHEATING: These should be part of NolphinIconView and our subclasses should implement it but only
 * one subclass will ever support it and we'll eventually get rid of the other */
GtkActionGroup *
nolphin_desktop_icon_grid_view_get_action_group (NolphinDesktopIconGridView *view)
{
    return view->details->desktop_action_group;
}

void
nolphin_desktop_icon_grid_view_set_grid_adjusts (NolphinDesktopIconGridView *view,
                                              gint                     h_adjust,
                                              gint                     v_adjust)
{
    NolphinIconContainer *container;
    NolphinFile *file;

    if (view->details->updating_menus) {
        return;
    }

    container = get_icon_container (view);
    clear_orphan_states (view);

    file = nolphin_view_get_directory_as_file (NOLPHIN_VIEW (view));

    nolphin_icon_container_set_grid_adjusts (container, h_adjust, v_adjust);

    container->details->needs_resort = TRUE;
    container->details->auto_layout = TRUE;

    nolphin_icon_view_set_directory_grid_adjusts (NOLPHIN_ICON_VIEW (view),
                                               file,
                                               h_adjust,
                                               v_adjust);

    nolphin_icon_container_redo_layout (container);

    nolphin_view_update_menus (NOLPHIN_VIEW (view));

    nolphin_icon_container_store_layout_timestamps_now (get_icon_container (view));

}
