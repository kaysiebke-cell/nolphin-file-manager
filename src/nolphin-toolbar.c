/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nolphin
 *
 * Copyright (C) 2011, Red Hat, Inc.
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
 * Author: Cosimo Cecchi <cosimoc@redhat.com>
 *
 */

#include <config.h>

#include "nolphin-toolbar.h"

#include "nolphin-location-bar.h"
#include "nolphin-pathbar.h"
#include "nolphin-window-private.h"
#include "nolphin-actions.h"
#include "nolphin-file-utilities.h"
#include <glib/gi18n.h>
#include <libnolphin-private/nolphin-global-preferences.h>
#include <libnolphin-private/nolphin-ui-utilities.h>

struct _NolphinToolbarPriv {
	GtkWidget *toolbar;

	GtkActionGroup *action_group;
	GtkUIManager *ui_manager;

    GtkWidget *previous_button;
    GtkWidget *next_button;
    GtkWidget *up_button;
    GtkWidget *refresh_button;
    GtkWidget *home_button;
    GtkWidget *computer_button;
    GtkWidget *toggle_location_button;
    GtkWidget *open_terminal_button;
    GtkWidget *new_folder_button;
    GtkWidget *search_button;
    GtkWidget *icon_view_button;
    GtkWidget *list_view_button;
    GtkWidget *compact_view_button;
    GtkWidget *show_thumbnails_button;
    GtkWidget *show_extra_pane_button;
    GtkWidget *split_layout_button;

	GtkWidget *path_bar;
	GtkWidget *location_bar;
    GtkWidget *root_bar;
    GtkWidget *stack;

	gboolean show_main_bar;
	gboolean show_location_entry;
    gboolean show_root_bar;
};

enum {
	PROP_ACTION_GROUP = 1,
	PROP_SHOW_LOCATION_ENTRY,
	PROP_SHOW_MAIN_BAR,
	NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

enum {
    CHECK_ADMIN_LOCATION,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (NolphinToolbar, nolphin_toolbar, GTK_TYPE_BOX);

static void
nolphin_toolbar_update_root_state (NolphinToolbar *self)
{
    gboolean is_admin_uri;

    g_signal_emit (self, signals[CHECK_ADMIN_LOCATION], 0, &is_admin_uri);

    if ((is_admin_uri ||
         (nolphin_user_is_root () && !nolphin_treating_root_as_normal())) &&
         g_settings_get_boolean (nolphin_preferences, NOLPHIN_PREFERENCES_SHOW_ROOT_WARNING)) {
        if (self->priv->show_root_bar != TRUE) {
            self->priv->show_root_bar = TRUE;
        }
    } else {
        self->priv->show_root_bar = FALSE;
    }
}

static void
toolbar_update_appearance (NolphinToolbar *self)
{
	GtkWidget *widgetitem;
	gboolean icon_toolbar;
	gboolean show_location_entry;

    nolphin_toolbar_update_root_state (self);

	show_location_entry = self->priv->show_location_entry;

	gtk_widget_set_visible (GTK_WIDGET(self->priv->toolbar),
				self->priv->show_main_bar);

    if (show_location_entry) {
        gtk_stack_set_visible_child_name (GTK_STACK (self->priv->stack), "location_bar");
    } else {
        gtk_stack_set_visible_child_name (GTK_STACK (self->priv->stack), "path_bar");
    }

    gtk_widget_set_visible (self->priv->root_bar,
                self->priv->show_root_bar);

        /* Please refer to the element name, not the action name after the forward slash, otherwise the prefs will not work*/

    widgetitem = self->priv->previous_button;
    icon_toolbar = g_settings_get_boolean (nolphin_preferences, NOLPHIN_PREFERENCES_SHOW_PREVIOUS_ICON_TOOLBAR);
    if ( icon_toolbar == FALSE ) { gtk_widget_hide (widgetitem); }
    else {gtk_widget_show (GTK_WIDGET(widgetitem));}

    widgetitem = self->priv->next_button;
    icon_toolbar = g_settings_get_boolean (nolphin_preferences, NOLPHIN_PREFERENCES_SHOW_NEXT_ICON_TOOLBAR);
    if ( icon_toolbar == FALSE ) { gtk_widget_hide (widgetitem); }
    else {gtk_widget_show (GTK_WIDGET(widgetitem));}

    widgetitem = self->priv->up_button;
    icon_toolbar = g_settings_get_boolean (nolphin_preferences, NOLPHIN_PREFERENCES_SHOW_UP_ICON_TOOLBAR);
    if ( icon_toolbar == FALSE ) { gtk_widget_hide (widgetitem); }
    else {gtk_widget_show (GTK_WIDGET(widgetitem));}

    widgetitem = self->priv->refresh_button;
    icon_toolbar = g_settings_get_boolean (nolphin_preferences, NOLPHIN_PREFERENCES_SHOW_RELOAD_ICON_TOOLBAR);
    if ( icon_toolbar == FALSE ) { gtk_widget_hide (widgetitem); }
    else {gtk_widget_show (GTK_WIDGET(widgetitem));}

    widgetitem = self->priv->home_button;
    icon_toolbar = g_settings_get_boolean (nolphin_preferences, NOLPHIN_PREFERENCES_SHOW_HOME_ICON_TOOLBAR);
    if ( icon_toolbar == FALSE ) { gtk_widget_hide (widgetitem); }
    else {gtk_widget_show (GTK_WIDGET(widgetitem));}

    widgetitem = self->priv->computer_button;
    icon_toolbar = g_settings_get_boolean (nolphin_preferences, NOLPHIN_PREFERENCES_SHOW_COMPUTER_ICON_TOOLBAR);
    if ( icon_toolbar == FALSE ) { gtk_widget_hide (widgetitem); }
    else {gtk_widget_show (GTK_WIDGET(widgetitem));}

    widgetitem = self->priv->search_button;
    icon_toolbar = g_settings_get_boolean (nolphin_preferences, NOLPHIN_PREFERENCES_SHOW_SEARCH_ICON_TOOLBAR);
    if ( icon_toolbar == FALSE ) { gtk_widget_hide (widgetitem); }
    else {gtk_widget_show (GTK_WIDGET(widgetitem));}

    widgetitem = self->priv->new_folder_button;
    icon_toolbar = g_settings_get_boolean (nolphin_preferences, NOLPHIN_PREFERENCES_SHOW_NEW_FOLDER_ICON_TOOLBAR);
    if ( icon_toolbar == FALSE ) { gtk_widget_hide (widgetitem); }
    else {gtk_widget_show (GTK_WIDGET(widgetitem));}

    widgetitem = self->priv->open_terminal_button;
    icon_toolbar = g_settings_get_boolean (nolphin_preferences, NOLPHIN_PREFERENCES_SHOW_OPEN_IN_TERMINAL_TOOLBAR);
    if (icon_toolbar == FALSE ) {gtk_widget_hide (widgetitem); }
    else {gtk_widget_show (GTK_WIDGET(widgetitem));}

    widgetitem = self->priv->toggle_location_button;
    icon_toolbar = g_settings_get_boolean (nolphin_preferences, NOLPHIN_PREFERENCES_SHOW_EDIT_ICON_TOOLBAR);
    if ( icon_toolbar == FALSE ) { gtk_widget_hide (widgetitem); }
    else {gtk_widget_show (GTK_WIDGET(widgetitem));}

    widgetitem = self->priv->icon_view_button;
    icon_toolbar = g_settings_get_boolean (nolphin_preferences, NOLPHIN_PREFERENCES_SHOW_ICON_VIEW_ICON_TOOLBAR);
    if ( icon_toolbar == FALSE ) { gtk_widget_hide (widgetitem); }
    else {gtk_widget_show (GTK_WIDGET(widgetitem));}

    widgetitem = self->priv->list_view_button;
    icon_toolbar = g_settings_get_boolean (nolphin_preferences, NOLPHIN_PREFERENCES_SHOW_LIST_VIEW_ICON_TOOLBAR);
    if ( icon_toolbar == FALSE ) { gtk_widget_hide (widgetitem); }
    else {gtk_widget_show (GTK_WIDGET(widgetitem));}

    widgetitem = self->priv->compact_view_button;
    icon_toolbar = g_settings_get_boolean (nolphin_preferences, NOLPHIN_PREFERENCES_SHOW_COMPACT_VIEW_ICON_TOOLBAR);
    if ( icon_toolbar == FALSE ) { gtk_widget_hide (widgetitem); }
    else {gtk_widget_show (GTK_WIDGET(widgetitem));}

    widgetitem = self->priv->show_thumbnails_button;
    icon_toolbar = g_settings_get_boolean (nolphin_preferences, NOLPHIN_PREFERENCES_SHOW_SHOW_THUMBNAILS_TOOLBAR);
    if ( icon_toolbar == FALSE ) { gtk_widget_hide (widgetitem); }
    else {gtk_widget_show (GTK_WIDGET(widgetitem));}

    widgetitem = self->priv->show_extra_pane_button;
    icon_toolbar = g_settings_get_boolean (nolphin_preferences, NOLPHIN_PREFERENCES_SHOW_TOGGLE_EXTRA_PANE_TOOLBAR);
    if ( icon_toolbar == FALSE ) { gtk_widget_hide (widgetitem); }
    else {gtk_widget_show (GTK_WIDGET(widgetitem));}

    widgetitem = self->priv->split_layout_button;
    if ( icon_toolbar == FALSE ) { gtk_widget_hide (widgetitem); }
    else {gtk_widget_show (GTK_WIDGET(widgetitem));}
}

static void
setup_root_info_bar (NolphinToolbar *self) {

    GtkWidget *root_bar = gtk_info_bar_new ();
    gtk_info_bar_set_message_type (GTK_INFO_BAR (root_bar), GTK_MESSAGE_ERROR);
    GtkWidget *content_area = gtk_info_bar_get_content_area (GTK_INFO_BAR (root_bar));

    GtkWidget *label = gtk_label_new (_("Erhöhte Berechtigungen"));
    gtk_widget_show (label);
    gtk_container_add (GTK_CONTAINER (content_area), label);

    self->priv->root_bar = root_bar;
    gtk_box_pack_start (GTK_BOX (self), self->priv->root_bar, TRUE, TRUE, 0);
}

static GtkWidget *
toolbar_create_toolbutton (NolphinToolbar *self,
                gboolean create_toggle,
                const gchar *name)
{
    GtkWidget *button;
    GtkWidget *image;
    GtkAction *action;

    if (create_toggle)
    {
        button = gtk_toggle_button_new ();
    } else {
        button = gtk_button_new ();
    }

    image = gtk_image_new ();

    gtk_button_set_image (GTK_BUTTON (button), image);
    action = gtk_action_group_get_action (self->priv->action_group, name);
    gtk_activatable_set_related_action (GTK_ACTIVATABLE (button), action);
    gtk_button_set_label (GTK_BUTTON (button), NULL);
    gtk_widget_set_tooltip_text (button, gtk_action_get_tooltip (action));
    gtk_widget_set_can_focus (button, FALSE);
    gtk_style_context_add_class (gtk_widget_get_style_context (button), GTK_STYLE_CLASS_FLAT);

    return button;
}

/* Kachel-Symbol für die Layout-Auswahl: zeichnet die Zonen in Theme-Farben. */
static gboolean
layout_tile_draw (GtkWidget *widget, cairo_t *cr, gpointer user_data)
{
    static const double zones[][5][4] = {
        { {0, 0, .5, 1}, {.5, 0, .5, 1}, {0} },
        { {0, 0, 1/3., 1}, {1/3., 0, 1/3., 1}, {2/3., 0, 1/3., 1}, {0} },
        { {0, 0, .5, .5}, {.5, 0, .5, .5}, {0, .5, .5, .5}, {.5, .5, .5, .5}, {0} },
        { {0, 0, 2/3., 1}, {2/3., 0, 1/3., .5}, {2/3., .5, 1/3., .5}, {0} },
        { {0, 0, 1, .5}, {0, .5, 1, .5}, {0} },
    };
    GtkStyleContext *context = gtk_widget_get_style_context (widget);
    GtkStateFlags state = gtk_widget_get_state_flags (widget);
    GdkRGBA color;
    gint w = gtk_widget_get_allocated_width (widget);
    gint h = gtk_widget_get_allocated_height (widget);
    gint i;

    gtk_style_context_get_color (context, state, &color);
    gdk_cairo_set_source_rgba (cr, &color);
    cairo_set_line_width (cr, 1.0);
    for (i = 0; zones[GPOINTER_TO_INT (user_data)][i][2] > 0; i++) {
        const double *z = zones[GPOINTER_TO_INT (user_data)][i];

        cairo_rectangle (cr, z[0] * w + 1.5, z[1] * h + 1.5, z[2] * w - 3, z[3] * h - 3);
        cairo_stroke (cr);
    }
    return FALSE;
}

static void
layout_tile_clicked (GtkButton *button, gpointer user_data)
{
    const gchar *name = g_object_get_data (G_OBJECT (button), "nolphin-layout-action");
    GtkWidget *window = gtk_widget_get_toplevel (GTK_WIDGET (user_data));
    GtkAction *action = NULL;

    gtk_popover_popdown (GTK_POPOVER (gtk_widget_get_ancestor (GTK_WIDGET (button), GTK_TYPE_POPOVER)));
    /* Die Layout-Aktionen liegen in der Hauptaktionsgruppe des Fensters
     * (wie im Menü), nicht in der Gruppe dieser Werkzeugleiste. */
    if (NOLPHIN_IS_WINDOW (window)) {
        action = gtk_action_group_get_action (nolphin_window_get_main_action_group (NOLPHIN_WINDOW (window)), name);
    }
    if (action != NULL) {
        gtk_action_activate (action);
    }
}

static GtkWidget *
toolbar_create_split_layout_button (NolphinToolbar *self)
{
    static const struct {
        const char *action;
        const char *label;
        const char *tooltip;
    } tiles[] = {
        { NOLPHIN_ACTION_SPLIT_LAYOUT_TWO_COLUMNS, N_("2 Spalten"), N_("Die Ansicht in zwei Spalten teilen") },
        { NOLPHIN_ACTION_SPLIT_LAYOUT_THREE_COLUMNS, N_("3 Spalten"), N_("Die Ansicht in drei Spalten teilen") },
        { NOLPHIN_ACTION_SPLIT_LAYOUT_GRID, N_("2×2"), N_("Die Ansicht in vier Bereiche als Raster teilen") },
        { NOLPHIN_ACTION_SPLIT_LAYOUT_BIG_PLUS_TWO, N_("1 groß + 2"), N_("Links ein großer Bereich, rechts zwei kleine untereinander") },
        { NOLPHIN_ACTION_SPLIT_LAYOUT_TWO_ROWS, N_("2 Zeilen"), N_("Die Ansicht in zwei Zeilen teilen") },
    };
    GtkWidget *button = gtk_menu_button_new ();
    GtkWidget *popover = gtk_popover_new (button);
    GtkWidget *title = gtk_label_new (NULL);
    GtkWidget *vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
    gchar *markup = g_markup_printf_escaped ("<b>%s</b>", _("Layout wählen"));
    guint i;

    gtk_label_set_markup (GTK_LABEL (title), markup);
    g_free (markup);
    gtk_widget_set_halign (title, GTK_ALIGN_START);
    gtk_box_pack_start (GTK_BOX (vbox), title, FALSE, FALSE, 0);

    for (i = 0; i < G_N_ELEMENTS (tiles); i++) {
        GtkWidget *tile = gtk_button_new ();
        GtkWidget *tbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
        GtkWidget *area = gtk_drawing_area_new ();
        GtkWidget *label = gtk_label_new (_(tiles[i].label));

        gtk_widget_set_size_request (area, 64, 44);
        g_signal_connect (area, "draw", G_CALLBACK (layout_tile_draw), GINT_TO_POINTER ((gint) i));
        gtk_box_pack_start (GTK_BOX (tbox), area, FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (tbox), label, FALSE, FALSE, 0);
        gtk_container_add (GTK_CONTAINER (tile), tbox);
        gtk_button_set_relief (GTK_BUTTON (tile), GTK_RELIEF_NONE);
        g_object_set_data (G_OBJECT (tile), "nolphin-layout-action", (gpointer) tiles[i].action);
        gtk_widget_set_tooltip_text (tile, _(tiles[i].tooltip));
        g_signal_connect (tile, "clicked", G_CALLBACK (layout_tile_clicked), button);
        gtk_box_pack_start (GTK_BOX (row), tile, FALSE, FALSE, 0);
    }
    gtk_box_pack_start (GTK_BOX (vbox), row, FALSE, FALSE, 0);
    gtk_widget_set_margin_start (vbox, 10);
    gtk_widget_set_margin_end (vbox, 10);
    gtk_widget_set_margin_top (vbox, 10);
    gtk_widget_set_margin_bottom (vbox, 10);
    gtk_container_add (GTK_CONTAINER (popover), vbox);
    gtk_widget_show_all (vbox);

    gtk_menu_button_set_popover (GTK_MENU_BUTTON (button), popover);
    gtk_button_set_image (GTK_BUTTON (button), gtk_image_new_from_icon_name ("pan-down-symbolic", GTK_ICON_SIZE_MENU));
    gtk_widget_set_tooltip_text (button, _("Layout der geteilten Ansicht wählen"));
    gtk_widget_set_can_focus (button, FALSE);
    gtk_style_context_add_class (gtk_widget_get_style_context (button), GTK_STYLE_CLASS_FLAT);
    return button;
}

static void
nolphin_toolbar_constructed (GObject *obj)
{
	NolphinToolbar *self = NOLPHIN_TOOLBAR (obj);
	GtkWidget *toolbar;
    GtkWidget *hbox;
    GtkToolItem *tool_box;
    GtkWidget *box;
	GtkStyleContext *context;

	G_OBJECT_CLASS (nolphin_toolbar_parent_class)->constructed (obj);

	gtk_style_context_set_junction_sides (gtk_widget_get_style_context (GTK_WIDGET (self)),
					      GTK_JUNCTION_BOTTOM);

    self->priv->show_location_entry = g_settings_get_boolean (nolphin_preferences, NOLPHIN_PREFERENCES_SHOW_LOCATION_ENTRY);

	/* add the UI */
	self->priv->ui_manager = gtk_ui_manager_new ();
	gtk_ui_manager_insert_action_group (self->priv->ui_manager, self->priv->action_group, 0);

	toolbar = gtk_toolbar_new ();
	self->priv->toolbar = toolbar;
    gtk_box_pack_start (GTK_BOX (self), self->priv->toolbar, TRUE, TRUE, 0);

	context = gtk_widget_get_style_context (GTK_WIDGET(toolbar));
	gtk_style_context_add_class (context, GTK_STYLE_CLASS_PRIMARY_TOOLBAR);

    /* Left side of the toolbar */
    tool_box = gtk_tool_item_new ();
    box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);

    self->priv->previous_button = toolbar_create_toolbutton (self, FALSE, NOLPHIN_ACTION_BACK);
    gtk_container_add (GTK_CONTAINER (box), self->priv->previous_button);

    self->priv->next_button = toolbar_create_toolbutton (self, FALSE, NOLPHIN_ACTION_FORWARD);
    gtk_container_add (GTK_CONTAINER (box), self->priv->next_button);

    self->priv->up_button = toolbar_create_toolbutton (self, FALSE, NOLPHIN_ACTION_UP);
    gtk_container_add (GTK_CONTAINER (box), self->priv->up_button);

    self->priv->refresh_button = toolbar_create_toolbutton (self, FALSE, NOLPHIN_ACTION_RELOAD);
    gtk_container_add (GTK_CONTAINER (box), self->priv->refresh_button);

    self->priv->home_button = toolbar_create_toolbutton (self, FALSE, NOLPHIN_ACTION_HOME);
    gtk_container_add (GTK_CONTAINER (box), self->priv->home_button);

    self->priv->computer_button = toolbar_create_toolbutton (self, FALSE, NOLPHIN_ACTION_COMPUTER);
    gtk_container_add (GTK_CONTAINER (box), self->priv->computer_button);

    gtk_container_add (GTK_CONTAINER (tool_box), GTK_WIDGET (box));
    gtk_container_add (GTK_CONTAINER (self->priv->toolbar), GTK_WIDGET (tool_box));

    gtk_widget_show_all (GTK_WIDGET (tool_box));
    gtk_widget_set_margin_right (GTK_WIDGET (tool_box), 6);

    /* Container to hold the location and pathbars */
    self->priv->stack = gtk_stack_new();
    gtk_stack_set_transition_type (GTK_STACK (self->priv->stack), GTK_STACK_TRANSITION_TYPE_CROSSFADE);
    gtk_stack_set_transition_duration (GTK_STACK (self->priv->stack), 150);

    /* Regular Path Bar */
    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (self->priv->stack), TRUE, TRUE, 0);

    self->priv->path_bar = g_object_new (NOLPHIN_TYPE_PATH_BAR, NULL);
    gtk_stack_add_named(GTK_STACK (self->priv->stack), GTK_WIDGET (self->priv->path_bar), "path_bar");

    /* Entry-Like Location Bar */
    self->priv->location_bar = nolphin_location_bar_new ();
    gtk_stack_add_named(GTK_STACK (self->priv->stack), GTK_WIDGET (self->priv->location_bar), "location_bar");
    gtk_widget_show_all (hbox);

    tool_box = gtk_tool_item_new ();
    gtk_tool_item_set_expand (tool_box, TRUE);
    gtk_container_add (GTK_CONTAINER (tool_box), hbox);
    gtk_container_add (GTK_CONTAINER (self->priv->toolbar), GTK_WIDGET (tool_box));
    gtk_widget_show (GTK_WIDGET (tool_box));

    /* Right Side of the toolbar */
    tool_box = gtk_tool_item_new ();
    box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);

    self->priv->toggle_location_button = toolbar_create_toolbutton (self, FALSE, NOLPHIN_ACTION_TOGGLE_LOCATION);
    gtk_container_add (GTK_CONTAINER (box), self->priv->toggle_location_button);

    self->priv->open_terminal_button = toolbar_create_toolbutton (self, FALSE, NOLPHIN_ACTION_OPEN_IN_TERMINAL);
    gtk_container_add (GTK_CONTAINER (box), self->priv->open_terminal_button);

    self->priv->new_folder_button = toolbar_create_toolbutton (self, FALSE, NOLPHIN_ACTION_NEW_FOLDER);
    gtk_container_add (GTK_CONTAINER (box), self->priv->new_folder_button);

    self->priv->search_button = toolbar_create_toolbutton (self, TRUE, NOLPHIN_ACTION_SEARCH);
    gtk_container_add (GTK_CONTAINER (box), self->priv->search_button);

    self->priv->show_thumbnails_button = toolbar_create_toolbutton (self, TRUE, NOLPHIN_ACTION_SHOW_THUMBNAILS);
    gtk_container_add (GTK_CONTAINER (box), self->priv->show_thumbnails_button);

    setup_root_info_bar (self);

    self->priv->show_extra_pane_button = toolbar_create_toolbutton (self, TRUE, NOLPHIN_ACTION_SHOW_HIDE_EXTRA_PANE);
    gtk_container_add (GTK_CONTAINER (box), self->priv->show_extra_pane_button);

    self->priv->split_layout_button = toolbar_create_split_layout_button (self);
    gtk_container_add (GTK_CONTAINER (box), self->priv->split_layout_button);

    self->priv->icon_view_button = toolbar_create_toolbutton (self, TRUE, NOLPHIN_ACTION_ICON_VIEW);
    gtk_container_add (GTK_CONTAINER (box), self->priv->icon_view_button);

    self->priv->list_view_button = toolbar_create_toolbutton (self, TRUE, NOLPHIN_ACTION_LIST_VIEW);
    gtk_container_add (GTK_CONTAINER (box), self->priv->list_view_button);

    self->priv->compact_view_button = toolbar_create_toolbutton (self, TRUE, NOLPHIN_ACTION_COMPACT_VIEW);
    gtk_container_add (GTK_CONTAINER (box), self->priv->compact_view_button);

    gtk_container_add (GTK_CONTAINER (tool_box), GTK_WIDGET (box));
    gtk_container_add (GTK_CONTAINER (self->priv->toolbar), GTK_WIDGET (tool_box));

    gtk_widget_show_all (GTK_WIDGET (tool_box));
    gtk_widget_set_margin_left (GTK_WIDGET (tool_box), 6);

    g_signal_connect_swapped (nolphin_preferences,
                  "changed",
                  G_CALLBACK (toolbar_update_appearance), self);

	toolbar_update_appearance (self);
}

static void
nolphin_toolbar_init (NolphinToolbar *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, NOLPHIN_TYPE_TOOLBAR,
						  NolphinToolbarPriv);
	self->priv->show_main_bar = TRUE;	
}

static void
nolphin_toolbar_get_property (GObject *object,
			       guint property_id,
			       GValue *value,
			       GParamSpec *pspec)
{
	NolphinToolbar *self = NOLPHIN_TOOLBAR (object);

	switch (property_id) {
	case PROP_SHOW_LOCATION_ENTRY:
		g_value_set_boolean (value, self->priv->show_location_entry);
		break;
	case PROP_SHOW_MAIN_BAR:
		g_value_set_boolean (value, self->priv->show_main_bar);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
nolphin_toolbar_set_property (GObject *object,
			       guint property_id,
			       const GValue *value,
			       GParamSpec *pspec)
{
	NolphinToolbar *self = NOLPHIN_TOOLBAR (object);

	switch (property_id) {
	case PROP_ACTION_GROUP:
		self->priv->action_group = g_value_dup_object (value);
		break;
	case PROP_SHOW_LOCATION_ENTRY:
		nolphin_toolbar_set_show_location_entry (self, g_value_get_boolean (value));
		break;
	case PROP_SHOW_MAIN_BAR:
		nolphin_toolbar_set_show_main_bar (self, g_value_get_boolean (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
nolphin_toolbar_dispose (GObject *obj)
{
	NolphinToolbar *self = NOLPHIN_TOOLBAR (obj);

	g_clear_object (&self->priv->action_group);

	g_signal_handlers_disconnect_by_func (nolphin_preferences,
					      toolbar_update_appearance, self);

	G_OBJECT_CLASS (nolphin_toolbar_parent_class)->dispose (obj);
}

static void
nolphin_toolbar_class_init (NolphinToolbarClass *klass)
{
	GObjectClass *oclass;

	oclass = G_OBJECT_CLASS (klass);
	oclass->get_property = nolphin_toolbar_get_property;
	oclass->set_property = nolphin_toolbar_set_property;
	oclass->constructed = nolphin_toolbar_constructed;
	oclass->dispose = nolphin_toolbar_dispose;

	properties[PROP_ACTION_GROUP] =
		g_param_spec_object ("action-group",
				     "The action group",
				     "The action group to get actions from",
				     GTK_TYPE_ACTION_GROUP,
				     G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY |
				     G_PARAM_STATIC_STRINGS);
	properties[PROP_SHOW_LOCATION_ENTRY] =
		g_param_spec_boolean ("show-location-entry",
				      "Whether to show the location entry",
				      "Whether to show the location entry instead of the pathbar",
				      FALSE,
				      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
	properties[PROP_SHOW_MAIN_BAR] =
		g_param_spec_boolean ("show-main-bar",
				      "Whether to show the main bar",
				      "Whether to show the main toolbar",
				      TRUE,
				      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
	
    signals[CHECK_ADMIN_LOCATION] =
        g_signal_new ("check-admin-location",
                      G_TYPE_FROM_CLASS (klass),
                      G_SIGNAL_RUN_LAST,
                      0, NULL, NULL, NULL,
                      G_TYPE_BOOLEAN, 0);

	g_type_class_add_private (klass, sizeof (NolphinToolbarClass));
	g_object_class_install_properties (oclass, NUM_PROPERTIES, properties);
}

GtkWidget *
nolphin_toolbar_new (GtkActionGroup *action_group)
{
	return g_object_new (NOLPHIN_TYPE_TOOLBAR,
			     "action-group", action_group,
			     "orientation", GTK_ORIENTATION_VERTICAL,
			     NULL);
}

GtkWidget *
nolphin_toolbar_get_path_bar (NolphinToolbar *self)
{
	return self->priv->path_bar;
}

GtkWidget *
nolphin_toolbar_get_location_bar (NolphinToolbar *self)
{
	return self->priv->location_bar;
}

gboolean
nolphin_toolbar_get_show_location_entry (NolphinToolbar *self)
{
	return self->priv->show_location_entry;
}

void
nolphin_toolbar_set_show_main_bar (NolphinToolbar *self,
				    gboolean show_main_bar)
{
	if (show_main_bar != self->priv->show_main_bar) {
		self->priv->show_main_bar = show_main_bar;
		toolbar_update_appearance (self);

		g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SHOW_MAIN_BAR]);
	}
}

void
nolphin_toolbar_set_show_location_entry (NolphinToolbar *self,
					  gboolean show_location_entry)
{
	if (show_location_entry != self->priv->show_location_entry) {
		self->priv->show_location_entry = show_location_entry;
		toolbar_update_appearance (self);

		g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SHOW_LOCATION_ENTRY]);
	}
}

void
nolphin_toolbar_update_for_location (NolphinToolbar *self)
{
    toolbar_update_appearance (self);
}