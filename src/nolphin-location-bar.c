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
 * You should have received a copy of the GNU General Public
 * License along with this program; see the file COPYING.  If not,
 * write to the Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
 * Boston, MA 02110-1335, USA.
 *
 * Author: Maciej Stachowiak <mjs@eazel.com>
 *         Ettore Perazzoli <ettore@gnu.org>
 *         Michael Meeks <michael@nuclecu.unam.mx>
 *	   Andy Hertzfeld <andy@eazel.com>
 *
 */

/* nolphin-location-bar.c - Location bar for Nolphin
 */

#include <config.h>
#include "nolphin-location-bar.h"

#include "nolphin-application.h"
#include "nolphin-location-entry.h"
#include "nolphin-window.h"
#include <eel/eel-accessibility.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-string.h>
#include <eel/eel-vfs-extensions.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <libnolphin-private/nolphin-icon-dnd.h>
#include <libnolphin-private/nolphin-clipboard.h>
#include <stdio.h>
#include <string.h>

#define NOLPHIN_DND_URI_LIST_TYPE 	  (char *)"text/uri-list"
#define NOLPHIN_DND_TEXT_PLAIN_TYPE  (char *)"text/plain"

struct NolphinLocationBarDetails {
	NolphinEntry *entry;

	char *last_location;

	guint idle_id;
};

enum {
	NOLPHIN_DND_URI_LIST,
	NOLPHIN_DND_TEXT_PLAIN,
	NOLPHIN_DND_NTARGETS
};

enum {
	CANCEL,
	LOCATION_CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static const GtkTargetEntry drag_types [] = {
	{ NOLPHIN_DND_URI_LIST_TYPE,   0, NOLPHIN_DND_URI_LIST },
	{ NOLPHIN_DND_TEXT_PLAIN_TYPE, 0, NOLPHIN_DND_TEXT_PLAIN },
};

static const GtkTargetEntry drop_types [] = {
	{ NOLPHIN_DND_URI_LIST_TYPE,   0, NOLPHIN_DND_URI_LIST },
	{ NOLPHIN_DND_TEXT_PLAIN_TYPE, 0, NOLPHIN_DND_TEXT_PLAIN },
};

G_DEFINE_TYPE (NolphinLocationBar, nolphin_location_bar,
	       GTK_TYPE_BOX);

static NolphinWindow *
nolphin_location_bar_get_window (GtkWidget *bar)
{
	return NOLPHIN_WINDOW (gtk_widget_get_ancestor (bar, NOLPHIN_TYPE_WINDOW));
}

/**
 * nolphin_location_bar_get_location
 *
 * Get the GFile represented by the text in the location bar.
 **/
static GFile *
nolphin_location_bar_get_location (NolphinLocationBar *bar)
{
	char *user_location;
	GFile *location;

	user_location = gtk_editable_get_chars (GTK_EDITABLE (bar->details->entry), 0, -1);
	location = g_file_parse_name (user_location);
	g_free (user_location);

	return location;
}

static void
emit_location_changed (NolphinLocationBar *bar)
{
	GFile *location;

	location = nolphin_location_bar_get_location (bar);
	g_signal_emit (bar, signals[LOCATION_CHANGED], 0, location);
	g_object_unref (location);
}

static void
drag_data_received_callback (GtkWidget *widget,
		       	     GdkDragContext *context,
		       	     int x,
		       	     int y,
		       	     GtkSelectionData *data,
		             guint info,
		             guint32 time,
			     gpointer callback_data)
{
	char **names;
	NolphinApplication *application;
	int name_count;
	NolphinWindow *new_window, *window;
	GdkScreen      *screen;
	gboolean new_windows_for_extras;
	char *prompt;
	char *detail;
	GFile *location;
	NolphinLocationBar *self = NOLPHIN_LOCATION_BAR (widget);

	g_assert (data != NULL);
	g_assert (callback_data == NULL);

	names = g_uri_list_extract_uris ((const gchar *) gtk_selection_data_get_data (data));

	if (names == NULL || *names == NULL) {
		g_warning ("No D&D URI's");
		g_strfreev (names);
		gtk_drag_finish (context, FALSE, FALSE, time);
		return;
	}

	window = nolphin_location_bar_get_window (widget);
	new_windows_for_extras = FALSE;
	/* Ask user if they really want to open multiple windows
	 * for multiple dropped URIs. This is likely to have been
	 * a mistake.
	 */
	name_count = g_strv_length (names);
	if (name_count > 1) {
		prompt = g_strdup_printf (ngettext("Wollen Sie %d Ort anzeigen?",
						   "Wollen Sie %d Orte anzeigen?",
						   name_count),
					  name_count);
		detail = g_strdup_printf (ngettext("Dies würde %d Einzelfenster öffnen.",
						   "Dies würde %d Einzelfenster öffnen.",
						   name_count),
					  name_count);
		/* eel_run_simple_dialog should really take in pairs
		 * like gtk_dialog_new_with_buttons() does. */
		new_windows_for_extras = eel_run_simple_dialog
			(GTK_WIDGET (window),
			 TRUE,
			 GTK_MESSAGE_QUESTION,
			 prompt,
			 detail,
			 GTK_STOCK_CANCEL, GTK_STOCK_OK,
			 NULL) != 0 /* GNOME_OK */;

		g_free (prompt);
		g_free (detail);

		if (!new_windows_for_extras) {
			g_strfreev (names);
			gtk_drag_finish (context, FALSE, FALSE, time);
			return;
		}
	}

	nolphin_location_bar_set_location (self, names[0]);
	emit_location_changed (self);

	if (new_windows_for_extras) {
		int i;

		application = nolphin_application_get_singleton ();
		screen = gtk_window_get_screen (GTK_WINDOW (window));

		for (i = 1; names[i] != NULL; ++i) {
			new_window = nolphin_application_create_window (application, screen);
			location = g_file_new_for_uri (names[i]);
			nolphin_window_go_to (new_window, location);
			g_object_unref (location);
		}
	}

	g_strfreev (names);

	gtk_drag_finish (context, TRUE, FALSE, time);
}

static void
drag_data_get_callback (GtkWidget *widget,
		  	GdkDragContext *context,
		  	GtkSelectionData *selection_data,
		  	guint info,
		 	guint32 time,
			gpointer callback_data)
{
	NolphinLocationBar *self;
	GFile *location;
	gchar *uri;

	g_assert (selection_data != NULL);
	self = callback_data;

	location = nolphin_location_bar_get_location (self);
	uri = g_file_get_uri (location);

	switch (info) {
	case NOLPHIN_DND_URI_LIST:
	case NOLPHIN_DND_TEXT_PLAIN:
		gtk_selection_data_set (selection_data,
					gtk_selection_data_get_target (selection_data),
					8, (guchar *) uri,
					strlen (uri));
		break;
	default:
		g_assert_not_reached ();
	}
	g_free (uri);
	g_object_unref (location);
}

static gboolean
button_pressed_callback (GtkWidget             *widget,
			       GdkEventButton        *event)
{
	NolphinWindow *window;
	NolphinWindowSlot *slot;
	NolphinView *view;
	NolphinLocationEntry *entry;

	if (event->button != 3) {
		return FALSE;
	}

	window = nolphin_location_bar_get_window (gtk_widget_get_parent (widget));
	slot = nolphin_window_get_active_slot (window);
	view = slot->content_view;
	entry = NOLPHIN_LOCATION_ENTRY (gtk_bin_get_child (GTK_BIN (widget)));

    if (view == NULL ||
        nolphin_location_entry_get_secondary_action (entry) == NOLPHIN_LOCATION_ENTRY_ACTION_GOTO) {
        return FALSE;
    }

	nolphin_view_pop_up_location_context_menu (view, event, NULL);

	return FALSE;
}

/**
 * nolphin_location_bar_update_icon
 *
 * if the text in the entry matches the uri, set the label to "location", otherwise use "goto"
 *
 **/
static void
nolphin_location_bar_update_icon (NolphinLocationBar *bar)
{
       const char *current_text;
       GFile *location;
       GFile *last_location;

       if (bar->details->last_location == NULL){
               nolphin_location_entry_set_secondary_action (NOLPHIN_LOCATION_ENTRY (bar->details->entry),
                                                             NOLPHIN_LOCATION_ENTRY_ACTION_GOTO);
               return;
       }

       current_text = gtk_entry_get_text (GTK_ENTRY (bar->details->entry));
       location = g_file_parse_name (current_text);
       last_location = g_file_parse_name (bar->details->last_location);

       if (g_file_equal (last_location, location)) {
               nolphin_location_entry_set_secondary_action (NOLPHIN_LOCATION_ENTRY (bar->details->entry),
                                                             NOLPHIN_LOCATION_ENTRY_ACTION_CLEAR);
       } else {
               nolphin_location_entry_set_secondary_action (NOLPHIN_LOCATION_ENTRY (bar->details->entry),
                                                             NOLPHIN_LOCATION_ENTRY_ACTION_GOTO);
       }

       g_object_unref (location);
       g_object_unref (last_location);
}

static void
editable_changed_callback (GtkEntry *entry,
                          gpointer user_data)
{
       nolphin_location_bar_update_icon (NOLPHIN_LOCATION_BAR (user_data));
}

static void
nolphin_location_bar_cancel (NolphinLocationBar *bar)
{
    nolphin_location_bar_set_location (bar, bar->details->last_location);
    nolphin_entry_select_all (bar->details->entry);
}

static void
editable_activate_callback (GtkEntry *entry,
			    gpointer user_data)
{
    nolphin_location_bar_update_icon (NOLPHIN_LOCATION_BAR (user_data));
	NolphinLocationBar *self = user_data;
	const char *entry_text;

	entry_text = gtk_entry_get_text (entry);
	if (entry_text != NULL && *entry_text != '\0') {
		emit_location_changed (self);
	} else {
        nolphin_location_bar_cancel(self);
    }
}

void
nolphin_location_bar_activate (NolphinLocationBar *bar)
{
	/* Put the keyboard focus in the text field when switching to this mode,
	 * and select all text for easy overtyping
	 */
	gtk_widget_grab_focus (GTK_WIDGET (bar->details->entry));
	nolphin_entry_select_all (bar->details->entry);
}

static void
finalize (GObject *object)
{
	NolphinLocationBar *bar;

	bar = NOLPHIN_LOCATION_BAR (object);

	/* cancel the pending idle call, if any */
	if (bar->details->idle_id != 0) {
		g_source_remove (bar->details->idle_id);
		bar->details->idle_id = 0;
	}

	g_free (bar->details->last_location);
	bar->details->last_location = NULL;

	G_OBJECT_CLASS (nolphin_location_bar_parent_class)->finalize (object);
}

static void
nolphin_location_bar_class_init (NolphinLocationBarClass *klass)
{
	GObjectClass *gobject_class;
	GtkBindingSet *binding_set;

	gobject_class = G_OBJECT_CLASS (klass);
	gobject_class->finalize = finalize;

	klass->cancel = nolphin_location_bar_cancel;

	signals[CANCEL] = g_signal_new
		("cancel",
		 G_TYPE_FROM_CLASS (klass),
		 G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		 G_STRUCT_OFFSET (NolphinLocationBarClass,
				  cancel),
		 NULL, NULL,
		 g_cclosure_marshal_VOID__VOID,
		 G_TYPE_NONE, 0);

	signals[LOCATION_CHANGED] = g_signal_new
		("location-changed",
		 G_TYPE_FROM_CLASS (klass),
		 G_SIGNAL_RUN_LAST, 0,
		 NULL, NULL,
		 g_cclosure_marshal_generic,
		 G_TYPE_NONE, 1, G_TYPE_OBJECT);

	binding_set = gtk_binding_set_by_class (klass);
	gtk_binding_entry_add_signal (binding_set, GDK_KEY_Escape, 0, "cancel", 0);

	g_type_class_add_private (klass, sizeof (NolphinLocationBarDetails));
}

static void
nolphin_location_bar_init (NolphinLocationBar *bar)
{
	GtkWidget *entry;
	GtkWidget *event_box;

	bar->details = G_TYPE_INSTANCE_GET_PRIVATE (bar, NOLPHIN_TYPE_LOCATION_BAR,
						    NolphinLocationBarDetails);

	gtk_orientable_set_orientation (GTK_ORIENTABLE (bar),
					GTK_ORIENTATION_HORIZONTAL);


    event_box = gtk_event_box_new ();
    gtk_event_box_set_visible_window (GTK_EVENT_BOX (event_box), FALSE);
    gtk_container_set_border_width (GTK_CONTAINER (event_box), 2);

	entry = nolphin_location_entry_new ();

	g_signal_connect_object (entry, "activate",
				 G_CALLBACK (editable_activate_callback), bar, G_CONNECT_AFTER);
    g_signal_connect_object (entry, "changed",
                             G_CALLBACK (editable_changed_callback), bar, 0);

    gtk_container_add (GTK_CONTAINER (event_box), entry);

	gtk_box_pack_start (GTK_BOX (bar), event_box, TRUE, TRUE, 4);

	/* Label context menu */
	g_signal_connect (event_box, "button-press-event",
			  G_CALLBACK (button_pressed_callback), NULL);

	/* Drag source */
	gtk_drag_source_set (GTK_WIDGET (event_box),
			     GDK_BUTTON1_MASK | GDK_BUTTON3_MASK,
			     drag_types, G_N_ELEMENTS (drag_types),
			     GDK_ACTION_COPY | GDK_ACTION_LINK);
	g_signal_connect_object (event_box, "drag_data_get",
				 G_CALLBACK (drag_data_get_callback), bar, 0);

	/* Drag dest. */
	gtk_drag_dest_set (GTK_WIDGET (bar),
			   GTK_DEST_DEFAULT_ALL,
			   drop_types, G_N_ELEMENTS (drop_types),
			   GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK);
	g_signal_connect (bar, "drag_data_received",
			  G_CALLBACK (drag_data_received_callback), NULL);

	bar->details->entry = NOLPHIN_ENTRY (entry);

	gtk_widget_show_all (GTK_WIDGET (bar));
}

GtkWidget *
nolphin_location_bar_new (void)
{
	GtkWidget *bar;

	bar = gtk_widget_new (NOLPHIN_TYPE_LOCATION_BAR, NULL);

	return bar;
}

void
nolphin_location_bar_set_location (NolphinLocationBar *bar,
				    const char *location)
{
	char *formatted_location;
	GFile *file;
      char *unescaped_string;

	g_assert (location != NULL);

	/* Note: This is called in reaction to external changes, and
	 * thus should not emit the LOCATION_CHANGED signal. */

	if (eel_uri_is_search (location)) {
		nolphin_location_entry_set_special_text (NOLPHIN_LOCATION_ENTRY (bar->details->entry),
							  "");
	} else {
		file = g_file_new_for_uri (location);
		formatted_location = g_file_get_parse_name (file);
		g_object_unref (file);

              if (eel_uri_is_network (formatted_location)) {
                  unescaped_string = g_uri_unescape_string (formatted_location, "%20");
                  nolphin_location_entry_update_current_location (NOLPHIN_LOCATION_ENTRY (bar->details->entry),
                                                                   unescaped_string);
                  g_free (unescaped_string);
              } else {
                  nolphin_location_entry_update_current_location (NOLPHIN_LOCATION_ENTRY (bar->details->entry),
                                                                   formatted_location);
              }

		g_free (formatted_location);
	}

	/* remember the original location for later comparison */

	if (bar->details->last_location != location) {
		g_free (bar->details->last_location);
		bar->details->last_location = g_strdup (location);
	}

    nolphin_location_bar_update_icon (bar);
}

NolphinEntry *
nolphin_location_bar_get_entry (NolphinLocationBar *location_bar)
{
	return location_bar->details->entry;
}

gboolean
nolphin_location_bar_has_focus (NolphinLocationBar *location_bar)
{
    return gtk_widget_has_focus (GTK_WIDGET (location_bar->details->entry));
}
