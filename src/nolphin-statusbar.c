/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* nolphin-statusbar.c
 * 
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
 * Boston, MA 02110-1335, USA.
 */

#include "nolphin-statusbar.h"

#include "nolphin-actions.h"

#include <config.h>
#include <glib/gi18n.h>

enum {
        LAST_SIGNAL
};

enum {
    PROP_WINDOW = 1,
    NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

G_DEFINE_TYPE (NolphinStatusBar, nolphin_status_bar, GTK_TYPE_BOX);

static void
nolphin_status_bar_init (NolphinStatusBar *bar)
{
    bar->window = NULL;
}

static void
nolphin_status_bar_set_property (GObject        *object,
                              guint           arg_id,
                              const GValue   *value,
                              GParamSpec     *pspec)
{
    NolphinStatusBar *self = NOLPHIN_STATUS_BAR (object);

    switch (arg_id) {
        case PROP_WINDOW:
            self->window = g_value_get_object (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, arg_id, pspec);
            break;
    }
}

static void
nolphin_status_bar_get_property (GObject      *object,
                              guint         arg_id,
                              GValue       *value,
                              GParamSpec   *pspec)
{
    NolphinStatusBar *self = NOLPHIN_STATUS_BAR (object);

    switch (arg_id) {
        case PROP_WINDOW:
            g_value_set_object (value, self->window);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, arg_id, pspec);
            break;
    }
}

static void
nolphin_status_bar_dispose (GObject *object)
{
    NolphinStatusBar *bar = NOLPHIN_STATUS_BAR (object);

    bar->window = NULL;

    G_OBJECT_CLASS (nolphin_status_bar_parent_class)->dispose (object);
}

static void
action_places_toggle_callback (GtkButton *button, NolphinStatusBar *bar)
{
    nolphin_window_set_sidebar_id (NOLPHIN_WINDOW (bar->window), NOLPHIN_WINDOW_SIDEBAR_PLACES);

    nolphin_status_bar_sync_button_states (bar);
}

static void
action_treeview_toggle_callback (GtkButton *button, NolphinStatusBar *bar)
{
    nolphin_window_set_sidebar_id (NOLPHIN_WINDOW (bar->window), NOLPHIN_WINDOW_SIDEBAR_TREE);

    nolphin_status_bar_sync_button_states (bar);
}

static void
action_show_sidebar_callback (GtkButton *button, NolphinStatusBar *bar)
{
    nolphin_window_show_sidebar (bar->window);
}

static void
action_hide_sidebar_callback (GtkButton *button, NolphinStatusBar *bar)
{
    nolphin_window_hide_sidebar (bar->window);
}

static void
sidebar_state_changed_cb (gpointer pointer, gboolean state, gpointer user_data)
{
    nolphin_status_bar_sync_button_states (NOLPHIN_STATUS_BAR (user_data));
}

static void
sidebar_type_changed_cb (gpointer pointer, const gchar *sidebar_id, gpointer user_data)
{
    nolphin_status_bar_sync_button_states (NOLPHIN_STATUS_BAR (user_data));
}

static void
on_slider_changed_cb (GtkWidget *zoom_slider, gpointer user_data)
{
    NolphinStatusBar *bar = NOLPHIN_STATUS_BAR (user_data);
    gdouble val = gtk_range_get_value (GTK_RANGE (zoom_slider));

    NolphinWindowSlot *slot = nolphin_window_get_active_slot (bar->window);

    if (!NOLPHIN_IS_WINDOW_SLOT (slot))
        return;

    NolphinView *view = slot->content_view;

    if (!NOLPHIN_IS_VIEW (view))
        return;

    nolphin_view_zoom_to_level (view, (int) val);
}

#define SLIDER_WIDTH 100

static void
nolphin_status_bar_constructed (GObject *object)
{
    NolphinStatusBar *bar = NOLPHIN_STATUS_BAR (object);
    G_OBJECT_CLASS (nolphin_status_bar_parent_class)->constructed (object);

    GtkWidget *statusbar = gtk_statusbar_new ();
    GtkStyleContext *context;

    bar->real_statusbar = statusbar;

    GtkIconSize size = gtk_icon_size_from_name (NOLPHIN_STATUSBAR_ICON_SIZE_NAME);

    context = gtk_widget_get_style_context (GTK_WIDGET (bar));
    gtk_style_context_add_class (context, GTK_STYLE_CLASS_TOOLBAR);
    gtk_container_set_border_width (GTK_CONTAINER (bar), 2);

    GtkWidget *button, *icon;

    button = gtk_toggle_button_new ();
    icon = gtk_image_new_from_icon_name ("nolphin-sidebar-places-symbolic", size);
    gtk_button_set_image (GTK_BUTTON (button), icon);
    gtk_widget_set_tooltip_text (GTK_WIDGET (button), _("Orte anzeigen"));
    bar->places_button = button;
    gtk_box_pack_start (GTK_BOX (bar), button, FALSE, FALSE, 2);
    g_signal_connect (GTK_BUTTON (button), "clicked",
                      G_CALLBACK (action_places_toggle_callback), bar);

    button = gtk_toggle_button_new ();
    icon = gtk_image_new_from_icon_name ("nolphin-sidebar-tree-symbolic", size);
    gtk_button_set_image (GTK_BUTTON (button), icon);
    gtk_widget_set_tooltip_text (GTK_WIDGET (button), _("Baumansicht anzeigen"));
    bar->tree_button = button;
    gtk_box_pack_start (GTK_BOX (bar), button, FALSE, FALSE, 2);
    g_signal_connect (GTK_BUTTON (button), "clicked",
                      G_CALLBACK (action_treeview_toggle_callback), bar);

    GtkWidget *sep = gtk_separator_new (GTK_ORIENTATION_VERTICAL);
    gtk_box_pack_start (GTK_BOX (bar), sep, FALSE, FALSE, 6);
    gtk_widget_show (sep);
    bar->separator = sep;

    button = gtk_button_new ();
    icon = gtk_image_new_from_icon_name ("nolphin-sidebar-hide-symbolic", size);
    gtk_button_set_image (GTK_BUTTON (button), icon);
    gtk_widget_set_tooltip_text (GTK_WIDGET (button), _("Seitenleiste verstecken (F9)"));
    bar->hide_button = button;
    gtk_box_pack_start (GTK_BOX (bar), button, FALSE, FALSE, 2);
    g_signal_connect (GTK_BUTTON (button), "clicked",
                      G_CALLBACK (action_hide_sidebar_callback), bar);

    button = gtk_button_new ();
    icon = gtk_image_new_from_icon_name ("nolphin-sidebar-show-symbolic", size);
    gtk_button_set_image (GTK_BUTTON (button), icon);
    gtk_widget_set_tooltip_text (GTK_WIDGET (button), _("Seitenleiste anzeigen (F9)"));
    bar->show_button = button;
    gtk_box_pack_start (GTK_BOX (bar), button, FALSE, FALSE, 2);
    g_signal_connect (GTK_BUTTON (button), "clicked",
                      G_CALLBACK (action_show_sidebar_callback), bar);

    gtk_box_pack_start (GTK_BOX (bar), statusbar, TRUE, TRUE, 10);
    gtk_widget_set_margin_top (GTK_WIDGET (statusbar), 0);
    gtk_widget_set_margin_bottom (GTK_WIDGET (statusbar), 0);

    GtkWidget *zoom_slider = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL,
                                                       (gdouble) NOLPHIN_ZOOM_LEVEL_SMALLEST,
                                                       (gdouble) NOLPHIN_ZOOM_LEVEL_LARGEST,
                                                       1.0);
    gtk_widget_set_tooltip_text (GTK_WIDGET (zoom_slider), _("Vergrößerungsstufe einstellen"));
    bar->zoom_slider = zoom_slider;

    gtk_box_pack_start (GTK_BOX (bar), zoom_slider, FALSE, FALSE, 2);

    gtk_widget_set_size_request (GTK_WIDGET (zoom_slider), SLIDER_WIDTH, 0);
    gtk_scale_set_draw_value (GTK_SCALE (zoom_slider), FALSE);
    gtk_range_set_increments (GTK_RANGE (zoom_slider), 1.0, 1.0);
    gtk_range_set_round_digits (GTK_RANGE (zoom_slider), 0);

    gtk_widget_show_all (GTK_WIDGET (bar));

    g_signal_connect_object (NOLPHIN_WINDOW (bar->window), "notify::show-sidebar",
                             G_CALLBACK (sidebar_state_changed_cb), bar, G_CONNECT_AFTER);

    g_signal_connect_object (NOLPHIN_WINDOW (bar->window), "notify::sidebar-view-id",
                           G_CALLBACK (sidebar_type_changed_cb), bar, G_CONNECT_AFTER);

    g_signal_connect (GTK_RANGE (zoom_slider), "value-changed",
                      G_CALLBACK (on_slider_changed_cb), bar);

    GtkWidget *cont = gtk_statusbar_get_message_area (GTK_STATUSBAR (statusbar));

    GList *children = gtk_container_get_children (GTK_CONTAINER (cont));

    gtk_box_set_child_packing (GTK_BOX (cont),
                               GTK_WIDGET (children->data),
                               TRUE, FALSE, 10, GTK_PACK_START);

    g_list_free (children);

    nolphin_status_bar_sync_button_states (bar);
}


static void
nolphin_status_bar_class_init (NolphinStatusBarClass *status_bar_class)
{
    GObjectClass *oclass;

    oclass = G_OBJECT_CLASS (status_bar_class);

    oclass->set_property = nolphin_status_bar_set_property;
    oclass->get_property = nolphin_status_bar_get_property;

    oclass->dispose = nolphin_status_bar_dispose;
    oclass->constructed = nolphin_status_bar_constructed;

    properties[PROP_WINDOW] = g_param_spec_object ("window",
                                                   "The NolphinWindow",
                                                   "The parent NolphinWindow",
                                                   NOLPHIN_TYPE_WINDOW,
                                                   G_PARAM_READWRITE |
                                                   G_PARAM_CONSTRUCT_ONLY |
                                                   G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (oclass, NUM_PROPERTIES, properties);
}

GtkWidget *
nolphin_status_bar_new (NolphinWindow *window)
{
    return g_object_new (NOLPHIN_TYPE_STATUS_BAR,
                         "orientation", GTK_ORIENTATION_HORIZONTAL,
                         "spacing", 0,
                         "window", window,
                         NULL);
}

GtkWidget *
nolphin_status_bar_get_real_statusbar (NolphinStatusBar *bar)
{
    return bar->real_statusbar;
}

void
nolphin_status_bar_sync_button_states (NolphinStatusBar *bar)
{
    const gchar *sidebar_id = nolphin_window_get_sidebar_id (NOLPHIN_WINDOW (bar->window));

    gboolean sidebar_visible = nolphin_window_get_show_sidebar (NOLPHIN_WINDOW (bar->window));

    if (sidebar_visible) {
        gtk_widget_show (bar->tree_button);
        gtk_widget_show (bar->places_button);
        gtk_widget_show (bar->separator);
        gtk_widget_show (bar->hide_button);
        gtk_widget_hide (bar->show_button);
    } else {
        gtk_widget_hide (bar->tree_button);
        gtk_widget_hide (bar->places_button);
        gtk_widget_hide (bar->hide_button);
        gtk_widget_hide (bar->separator);
        gtk_widget_show (bar->show_button);
    }

    g_signal_handlers_block_by_func (GTK_BUTTON (bar->tree_button), action_treeview_toggle_callback, bar);
    if (g_strcmp0 (sidebar_id, NOLPHIN_WINDOW_SIDEBAR_TREE) == 0) {
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (bar->tree_button), TRUE);
    } else {
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (bar->tree_button), FALSE);
    }
    g_signal_handlers_unblock_by_func (GTK_BUTTON (bar->tree_button), action_treeview_toggle_callback, bar);


    g_signal_handlers_block_by_func (GTK_BUTTON (bar->places_button), action_places_toggle_callback, bar);

    if (g_strcmp0 (sidebar_id, NOLPHIN_WINDOW_SIDEBAR_PLACES) == 0) {
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (bar->places_button), TRUE);
    } else {
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (bar->places_button), FALSE);
    }
    g_signal_handlers_unblock_by_func (GTK_BUTTON (bar->places_button), action_places_toggle_callback, bar);
}

void
nolphin_status_bar_sync_zoom_widgets (NolphinStatusBar *bar)
{

    NolphinWindowSlot *slot = nolphin_window_get_active_slot (bar->window);

    if (!NOLPHIN_IS_WINDOW_SLOT (slot))
        return;

    NolphinView *view = slot->content_view;

    if (!NOLPHIN_IS_VIEW (view))
        return;

    NolphinZoomLevel zoom_level = nolphin_view_get_zoom_level (NOLPHIN_VIEW (view));

    g_signal_handlers_block_by_func (GTK_RANGE (bar->zoom_slider), on_slider_changed_cb, bar);

    gtk_range_set_value (GTK_RANGE (bar->zoom_slider), (double) zoom_level);

    g_signal_handlers_unblock_by_func (GTK_RANGE (bar->zoom_slider), on_slider_changed_cb, bar);
}
