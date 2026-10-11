/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nolphin
 *
 * Copyright (C) 2000 Eazel, Inc.
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
 * Authors: Darin Adler <darin@bentspoon.com>
 */

#include <config.h>
#include "nolphin-desktop-window.h"
#include "nolphin-window-private.h"
#include "nolphin-actions.h"
#include "nolphin-desktop-manager.h"
#include "nolphin-icon-view.h"
#include "nolphin-desktop-icon-grid-view.h"

#include <X11/Xatom.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <gio/gio.h>
#include <glib/gi18n.h>

#ifdef HAVE_GTK_LAYER_SHELL
#include <gtk-layer-shell/gtk-layer-shell.h>
#endif

#include <eel/eel-vfs-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <libnolphin-private/nolphin-file-utilities.h>
#include <libnolphin-private/nolphin-icon-names.h>
#include <libnolphin-private/nolphin-global-preferences.h>
#include <libnolphin-private/nolphin-desktop-utils.h>

#define DEBUG_FLAG NOLPHIN_DEBUG_DESKTOP
#include <libnolphin-private/nolphin-debug.h>

enum {
    PROP_MONITOR = 1,
    NUM_PROPERTIES
};

struct NolphinDesktopWindowDetails {
    gint monitor;
	gboolean loaded;
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

G_DEFINE_TYPE (NolphinDesktopWindow, nolphin_desktop_window, 
	       NOLPHIN_TYPE_WINDOW);

static void
nolphin_desktop_window_update_directory (NolphinDesktopWindow *window)
{
	GFile *location;

	g_assert (NOLPHIN_IS_DESKTOP_WINDOW (window));

	window->details->loaded = FALSE;

    location = g_file_new_for_uri (EEL_DESKTOP_URI);

	nolphin_window_go_to (NOLPHIN_WINDOW (window), location);
	window->details->loaded = TRUE;

	g_object_unref (location);
}

static void
nolphin_desktop_window_dispose (GObject *obj)
{
	NolphinDesktopWindow *window = NOLPHIN_DESKTOP_WINDOW (obj);

	g_signal_handlers_disconnect_by_func (nolphin_preferences,
					      nolphin_desktop_window_update_directory,
					      window);

	G_OBJECT_CLASS (nolphin_desktop_window_parent_class)->dispose (obj);
}

static void
nolphin_desktop_window_constructed (GObject *obj)
{
	GtkActionGroup *action_group;
	GtkAction *action;
	AtkObject *accessible;
	NolphinDesktopWindow *window = NOLPHIN_DESKTOP_WINDOW (obj);
	NolphinWindow *nwindow = NOLPHIN_WINDOW (obj);

	G_OBJECT_CLASS (nolphin_desktop_window_parent_class)->constructed (obj);

    g_object_set_data (G_OBJECT (window), "monitor_number",
                       GINT_TO_POINTER (window->details->monitor));

	gtk_widget_hide (nwindow->details->statusbar);
	gtk_widget_hide (nwindow->details->menubar);
	gtk_widget_hide (nwindow->details->nolphin_status_bar);
	gtk_widget_hide (nwindow->details->statusbar_separator);

	action_group = nolphin_window_get_main_action_group (nwindow);

	/* Don't allow close action on desktop */
	action = gtk_action_group_get_action (action_group,
					      NOLPHIN_ACTION_CLOSE);
	gtk_action_set_sensitive (action, FALSE);

	/* Don't allow new tab on desktop */
	action = gtk_action_group_get_action (action_group,
					      NOLPHIN_ACTION_NEW_TAB);
	gtk_action_set_sensitive (action, FALSE);

	/* Set the accessible name so that it doesn't inherit the cryptic desktop URI. */
	accessible = gtk_widget_get_accessible (GTK_WIDGET (window));

	if (accessible) {
		atk_object_set_name (accessible, _("Schreibtisch"));
	}

    nolphin_desktop_window_update_geometry (window);

    gtk_window_set_resizable (GTK_WINDOW (window),
                  FALSE);
    gtk_window_set_decorated (GTK_WINDOW (window),
                  FALSE);

    gtk_widget_show (GTK_WIDGET (window));
}

static void
nolphin_desktop_window_get_property (GObject *object,
                                     guint property_id,
                                   GValue *value,
                               GParamSpec *pspec)
{
    NolphinDesktopWindow *window = NOLPHIN_DESKTOP_WINDOW (object);

    switch (property_id) {
    case PROP_MONITOR:
        g_value_set_int (value, window->details->monitor);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
nolphin_desktop_window_set_property (GObject *object,
                                     guint property_id,
                             const GValue *value,
                               GParamSpec *pspec)
{
    NolphinDesktopWindow *window = NOLPHIN_DESKTOP_WINDOW (object);

    switch (property_id) {
    case PROP_MONITOR:
        window->details->monitor = g_value_get_int (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
nolphin_desktop_window_init (NolphinDesktopWindow *window)
{
	window->details = G_TYPE_INSTANCE_GET_PRIVATE (window, NOLPHIN_TYPE_DESKTOP_WINDOW,
						       NolphinDesktopWindowDetails);

	g_object_set_data (G_OBJECT (window), "is_desktop_window",
			   GINT_TO_POINTER (1));

	/* Make it easier for themes authors to style the desktop window separately */
	gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (window)), "nolphin-desktop-window");

    if (eel_check_is_wayland ()) {
#ifdef HAVE_GTK_LAYER_SHELL
        gtk_layer_init_for_window (GTK_WINDOW (window));
#endif
    } else {
        gtk_window_set_type_hint (GTK_WINDOW (window), GDK_WINDOW_TYPE_HINT_DESKTOP);
    }
}

NolphinDesktopWindow *
nolphin_desktop_window_new (gint monitor)
{
	GApplication *application;
	NolphinDesktopWindow *window;

	application = g_application_get_default ();

    window = g_object_new (NOLPHIN_TYPE_DESKTOP_WINDOW,
			               "application", application,                           
                           "disable-chrome", TRUE,
                           "monitor", monitor,
                           NULL);

    /* Stop wrong desktop window size in GTK 3.20*/
    /* We don't want to set a default size, which the parent does, since this */
    /* will cause the desktop window to open at the wrong size in gtk 3.20 */
#if GTK_CHECK_VERSION (3, 19, 0) 
    gtk_window_set_default_size (GTK_WINDOW (window), -1, -1);
#endif

    GdkRGBA transparent = {0, 0, 0, 0};
    gtk_widget_override_background_color (GTK_WIDGET (window), 0, &transparent);

	/* Point window at the desktop folder.
	 * Note that nolphin_desktop_window_init is too early to do this.
	 */
	nolphin_desktop_window_update_directory (window);

	return window;
}

static gboolean
nolphin_desktop_window_delete_event (GtkWidget *widget,
                                  GdkEventAny *event)
{
	/* Returning true tells GTK+ not to delete the window. */
	return TRUE;
}

static void
map (GtkWidget *widget)
{
	/* Chain up to realize our children */
	GTK_WIDGET_CLASS (nolphin_desktop_window_parent_class)->map (widget);

    if (!eel_check_is_wayland ()) {
        gdk_window_lower (gtk_widget_get_window (widget));
    }

    GdkWindow *window;
    GdkRGBA transparent = { 0, 0, 0, 0 };

    window = gtk_widget_get_window (widget);
    gdk_window_set_background_rgba (window, &transparent);
}

static void
unrealize (GtkWidget *widget)
{
	GTK_WIDGET_CLASS (nolphin_desktop_window_parent_class)->unrealize (widget);
}

static void
realize (GtkWidget *widget)
{
    GdkVisual *visual;

	/* Make sure we get keyboard events */
	gtk_widget_set_events (widget, gtk_widget_get_events (widget) 
			      | GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK);

    visual = gdk_screen_get_rgba_visual (gtk_widget_get_screen (widget));
    if (visual) {
        gtk_widget_set_visual (widget, visual);
    }

	/* Do the work of realizing. */
	GTK_WIDGET_CLASS (nolphin_desktop_window_parent_class)->realize (widget);
}

static NolphinIconInfo *
real_get_icon (NolphinWindow *window,
	       NolphinWindowSlot *slot)
{
	return nolphin_icon_info_lookup_from_name (NOLPHIN_ICON_DESKTOP, 48,
                                            gtk_widget_get_scale_factor (GTK_WIDGET (window)));
}

static void
real_sync_title (NolphinWindow *window,
		 NolphinWindowSlot *slot)
{
	/* hardcode "Schreibtisch" */
	gtk_window_set_title (GTK_WINDOW (window), _("Desktop"));
}

static void
real_window_close (NolphinWindow *window)
{
	/* stub, does nothing */
	return;
}

static void
nolphin_desktop_window_class_init (NolphinDesktopWindowClass *klass)
{
	GtkWidgetClass *wclass = GTK_WIDGET_CLASS (klass);
	NolphinWindowClass *nclass = NOLPHIN_WINDOW_CLASS (klass);
	GObjectClass *oclass = G_OBJECT_CLASS (klass);

	oclass->constructed = nolphin_desktop_window_constructed;
	oclass->dispose = nolphin_desktop_window_dispose;
    oclass->set_property = nolphin_desktop_window_set_property;
    oclass->get_property = nolphin_desktop_window_get_property;

	wclass->realize = realize;
	wclass->unrealize = unrealize;
	wclass->map = map;
	wclass->delete_event = nolphin_desktop_window_delete_event;

	nclass->sync_title = real_sync_title;
	nclass->get_icon = real_get_icon;
	nclass->close = real_window_close;

    properties[PROP_MONITOR] =
        g_param_spec_int ("monitor",
                          "Monitor number",
                          "The monitor number this window is assigned to",
                          G_MININT, G_MAXINT, 0,
                          G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

    g_type_class_add_private (klass, sizeof (NolphinDesktopWindowDetails));
    g_object_class_install_properties (oclass, NUM_PROPERTIES, properties);
}

gboolean
nolphin_desktop_window_loaded (NolphinDesktopWindow *window)
{
	return window->details->loaded;
}

gint
nolphin_desktop_window_get_monitor (NolphinDesktopWindow *window)
{
    return window->details->monitor;
}

void
nolphin_desktop_window_update_geometry (NolphinDesktopWindow *window)
{
    GdkRectangle rect;

    if (nolphin_desktop_utils_configure_layer_shell (GTK_WINDOW (window),
                                                    window->details->monitor,
                                                    TRUE)) {
        DEBUG ("NolphinDesktopWindow: using layer-shell for monitor %d",
               window->details->monitor);
        return;
    }

    nolphin_desktop_manager_get_window_rect_for_monitor (nolphin_desktop_manager_get (),
                                                      window->details->monitor,
                                                      &rect);

    DEBUG ("NolphinDesktopWindow monitor:%d: x:%d, y:%d, w:%d, h:%d",
           window->details->monitor,
           rect.x, rect.y,
           rect.width, rect.height);

    gtk_window_move (GTK_WINDOW (window), rect.x, rect.y);
    gtk_widget_set_size_request (GTK_WIDGET (window), rect.width, rect.height);
}

gboolean
nolphin_desktop_window_get_grid_adjusts (NolphinDesktopWindow *window,
                                      gint              *h_adjust,
                                      gint              *v_adjust)
{
    NolphinView *view;
    NolphinWindowSlot *slot;
    NolphinFile *file;

    slot = nolphin_window_get_active_slot (NOLPHIN_WINDOW (window));
    view = nolphin_window_slot_get_current_view (slot);

    g_return_val_if_fail (NOLPHIN_IS_VIEW (view), FALSE);

    file = nolphin_view_get_directory_as_file (view);

    nolphin_icon_view_get_directory_grid_adjusts (NOLPHIN_ICON_VIEW (view),
                                               file,
                                               h_adjust,
                                               v_adjust);

    return TRUE;
}

gboolean
nolphin_desktop_window_set_grid_adjusts (NolphinDesktopWindow *window,
                                      gint               h_adjust,
                                      gint               v_adjust)
{
    NolphinView *view;
    NolphinWindowSlot *slot;

    slot = nolphin_window_get_active_slot (NOLPHIN_WINDOW (window));
    view = nolphin_window_slot_get_current_view (slot);

    g_return_val_if_fail (NOLPHIN_IS_DESKTOP_ICON_GRID_VIEW (view), FALSE);

    nolphin_desktop_icon_grid_view_set_grid_adjusts (NOLPHIN_DESKTOP_ICON_GRID_VIEW (view),
                                                  h_adjust,
                                                  v_adjust);

    return TRUE;
}

GtkActionGroup *
nolphin_desktop_window_get_action_group (NolphinDesktopWindow *window)
{
    NolphinView *view;
    NolphinWindowSlot *slot;

    slot = nolphin_window_get_active_slot (NOLPHIN_WINDOW (window));
    view = nolphin_window_slot_get_current_view (slot);

    g_return_val_if_fail (NOLPHIN_IS_DESKTOP_ICON_GRID_VIEW (view), NULL);

    return nolphin_desktop_icon_grid_view_get_action_group (NOLPHIN_DESKTOP_ICON_GRID_VIEW (view));
}