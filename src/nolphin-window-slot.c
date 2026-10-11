/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nolphin-window-slot.c: Nolphin window slot

   Copyright (C) 2008 Free Software Foundation, Inc.

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

   Author: Christian Neumair <cneumair@gnome.org>
*/

#include "config.h"

#include "nolphin-window-slot.h"

#include "nolphin-actions.h"
#include "nolphin-desktop-window.h"
#include "nolphin-filter-bar.h"
#include "nolphin-floating-bar.h"
#include "nolphin-window-private.h"
#include "nolphin-window-manage-views.h"
#include "nolphin-window-types.h"
#include "nolphin-window-slot-dnd.h"
#include "nolphin-workspace-panel.h"

#include <glib/gi18n.h>

#include <libnolphin-private/nolphin-file.h>
#include <libnolphin-private/nolphin-file-utilities.h>
#include <libnolphin-private/nolphin-global-preferences.h>

#define DEBUG_FLAG NOLPHIN_DEBUG_WINDOW
#include <libnolphin-private/nolphin-debug.h>

#include <eel/eel-string.h>

G_DEFINE_TYPE (NolphinWindowSlot, nolphin_window_slot, GTK_TYPE_BOX);

enum {
	ACTIVE,
	INACTIVE,
	CHANGED_PANE,
	LOCATION_CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static void
sync_search_directory (NolphinWindowSlot *slot)
{
	NolphinDirectory *directory;
	NolphinQuery *query;

	g_assert (NOLPHIN_IS_FILE (slot->viewed_file));

	directory = nolphin_directory_get_for_file (slot->viewed_file);
	g_assert (NOLPHIN_IS_SEARCH_DIRECTORY (directory));

	query = nolphin_query_editor_get_query (slot->query_editor);

    if (query) {
        nolphin_query_set_show_hidden (query,
                                    g_settings_get_boolean (nolphin_preferences,
                                                            NOLPHIN_PREFERENCES_SHOW_HIDDEN_FILES));
    }

	nolphin_search_directory_set_query (NOLPHIN_SEARCH_DIRECTORY (directory),
					     query);

    g_clear_object (&query);

	nolphin_window_slot_force_reload (slot);

	nolphin_directory_unref (directory);
}

static void
sync_search_location_cb (NolphinWindow *window,
			 GError *error,
			 gpointer user_data)
{
	NolphinWindowSlot *slot = user_data;

	sync_search_directory (slot);
}

static void
create_new_search (NolphinWindowSlot *slot)
{
	char *uri;
	NolphinDirectory *directory;
	GFile *location;

	uri = nolphin_search_directory_generate_new_uri ();
	location = g_file_new_for_uri (uri);

	directory = nolphin_directory_get (location);
	g_assert (NOLPHIN_IS_SEARCH_DIRECTORY (directory));

	nolphin_window_slot_open_location_full (slot, location, NOLPHIN_WINDOW_OPEN_FLAG_SEARCH, NULL, sync_search_location_cb, slot);

	nolphin_directory_unref (directory);
	g_object_unref (location);
	g_free (uri);
}

/* Öffnet eine gespeicherte Suche (.nsearch): Suchleiste einblenden,
 * Muster aus der Datei einsetzen und die Suche starten. */
gboolean
nolphin_window_slot_open_saved_search (NolphinWindowSlot *slot, const char *path)
{
	NolphinQuery *query;
	GtkAction *search;

	query = nolphin_query_load ((char *) path);
	if (query == NULL) {
		return FALSE;
	}

	search = gtk_action_group_get_action (slot->pane->toolbar_action_group, NOLPHIN_ACTION_SEARCH);
	if (search != NULL && !gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (search))) {
		gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (search), TRUE);
	} else {
		nolphin_window_slot_set_query_editor_visible (slot, TRUE);
	}

	nolphin_query_editor_set_query (slot->query_editor, query);
	g_object_unref (query);

	create_new_search (slot);
	return TRUE;
}

static void
query_editor_cancel_callback (NolphinQueryEditor *editor,
			      NolphinWindowSlot *slot)
{
	GtkAction *search;

	search = gtk_action_group_get_action (slot->pane->toolbar_action_group,
					      NOLPHIN_ACTION_SEARCH);

	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (search), FALSE);
}

static void
query_editor_changed_callback (NolphinQueryEditor *editor,
			       NolphinQuery *query,
			       gboolean reload,
			       NolphinWindowSlot *slot)
{
	NolphinDirectory *directory;

	g_assert (NOLPHIN_IS_FILE (slot->viewed_file));

    gtk_widget_hide (slot->no_search_results_box);
	directory = nolphin_directory_get_for_file (slot->viewed_file);
	if (!NOLPHIN_IS_SEARCH_DIRECTORY (directory)) {
		/* this is the first change from the query editor. we
		   ask for a location change to the search directory,
		   indicate the directory needs to be sync'd with the
		   current query. */
		create_new_search (slot);
		/* Focus is now on the new slot, move it back to query_editor */
		gtk_widget_grab_focus (GTK_WIDGET (slot->query_editor));
	} else {
		sync_search_directory (slot);
	}

	nolphin_directory_unref (directory);
}

static void
update_query_editor (NolphinWindowSlot *slot)
{
	NolphinDirectory *directory;
	NolphinSearchDirectory *search_directory;

	directory = nolphin_directory_get (slot->location);

	if (NOLPHIN_IS_SEARCH_DIRECTORY (directory)) {
		NolphinQuery *query;
		search_directory = NOLPHIN_SEARCH_DIRECTORY (directory);
		query = nolphin_search_directory_get_query (search_directory);
		if (query != NULL) {
			nolphin_query_editor_set_query (slot->query_editor,
							 query);
			g_object_unref (query);
		}
	} else {
		nolphin_query_editor_set_location (slot->query_editor, slot->location);
	}

	nolphin_directory_unref (directory);
}

/* Die Suchleiste wohnt während einer Suche in der rechten Arbeitsleiste
 * (Seite "Suche"), sonst im Tab. */
static void
slot_return_query_editor (NolphinWindowSlot *slot)
{
	GtkWidget *editor = GTK_WIDGET (slot->query_editor);
	GtkWidget *parent = gtk_widget_get_parent (editor);

	if (parent == NULL || parent == slot->extra_location_widgets) {
		return;
	}

	g_object_ref (editor);
	gtk_container_remove (GTK_CONTAINER (parent), editor);
	gtk_box_pack_start (GTK_BOX (slot->extra_location_widgets), editor, TRUE, TRUE, 0);
	g_object_unref (editor);
}

static GtkWidget *
slot_workspace_panel (NolphinWindowSlot *slot)
{
	NolphinWindow *window = (slot->pane != NULL) ? slot->pane->window : NULL;

	return (window != NULL) ? nolphin_window_get_workspace_panel (window) : NULL;
}

void
nolphin_window_slot_move_query_editor_to_panel (NolphinWindowSlot *slot)
{
	GtkWidget *panel = slot_workspace_panel (slot);
	GtkWidget *host = (panel != NULL) ? nolphin_workspace_panel_get_search_host (panel) : NULL;
	GtkWidget *editor = GTK_WIDGET (slot->query_editor);
	GtkWidget *parent;
	GList *children, *l;

	if (host == NULL) {
		return;
	}

	/* Eine Suchleiste eines anderen Tabs zuerst in ihren Tab zurückgeben. */
	children = gtk_container_get_children (GTK_CONTAINER (host));
	for (l = children; l != NULL; l = l->next) {
		NolphinWindowSlot *owner = g_object_get_data (G_OBJECT (l->data), "owner-slot");

		if (l->data != editor && owner != NULL) {
			slot_return_query_editor (owner);
		}
	}
	g_list_free (children);

	parent = gtk_widget_get_parent (editor);
	if (parent != host) {
		g_object_ref (editor);
		if (parent != NULL) {
			gtk_container_remove (GTK_CONTAINER (parent), editor);
		}
		/* gtk_container_add statt pack_start: nur so meldet der Behälter das Hinzufügen
		 * (die Such-Seite blendet dann ihren Hinweistext aus). */
		gtk_container_add (GTK_CONTAINER (host), editor);
		g_object_unref (editor);
	}
	g_object_set_data (G_OBJECT (editor), "owner-slot", slot);

	nolphin_workspace_panel_show_search (panel, slot->pane->window);
	gtk_widget_grab_focus (editor);
}

/* Beim Tabwechsel: die Suchleiste des jetzt aktiven Tabs in die Leiste
 * holen, sofern dort gesucht wird; sonst ihre Suchleiste im Tab lassen. */
void
nolphin_window_slot_sync_query_editor_host (NolphinWindowSlot *slot)
{
	GtkWidget *panel = slot_workspace_panel (slot);
	GtkWidget *host = (panel != NULL) ? nolphin_workspace_panel_get_search_host (panel) : NULL;
	GList *children, *l;

	if (host == NULL) {
		return;
	}

	children = gtk_container_get_children (GTK_CONTAINER (host));
	for (l = children; l != NULL; l = l->next) {
		NolphinWindowSlot *owner = g_object_get_data (G_OBJECT (l->data), "owner-slot");

		if (owner != NULL && owner != slot) {
			slot_return_query_editor (owner);
		}
	}
	g_list_free (children);

	if (nolphin_query_editor_get_active (NOLPHIN_QUERY_EDITOR (slot->query_editor))) {
		nolphin_window_slot_move_query_editor_to_panel (slot);
	}
}

static void
ensure_query_editor (NolphinWindowSlot *slot)
{
	g_assert (slot->query_editor != NULL);

	update_query_editor (slot);

    nolphin_query_editor_set_active (NOLPHIN_QUERY_EDITOR (slot->query_editor),
                                  nolphin_window_slot_get_location_uri (slot),
                                  TRUE);

	gtk_widget_grab_focus (GTK_WIDGET (slot->query_editor));
}

void
nolphin_window_slot_set_query_editor_visible (NolphinWindowSlot *slot,
					       gboolean            visible)
{
    gtk_widget_hide (slot->no_search_results_box);
    nolphin_window_slot_hide_filter_bar (slot);

	if (visible) {
		ensure_query_editor (slot);
		nolphin_window_slot_move_query_editor_to_panel (slot);

		if (slot->qe_changed_id == 0)
			slot->qe_changed_id = g_signal_connect (slot->query_editor, "changed",
								G_CALLBACK (query_editor_changed_callback), slot);
		if (slot->qe_cancel_id == 0)
			slot->qe_cancel_id = g_signal_connect (slot->query_editor, "cancel",
							       G_CALLBACK (query_editor_cancel_callback), slot);

	} else {
        nolphin_query_editor_set_active (NOLPHIN_QUERY_EDITOR (slot->query_editor), NULL, FALSE);

        {
            GtkWidget *panel = slot_workspace_panel (slot);
            gboolean was_in_panel = (gtk_widget_get_parent (GTK_WIDGET (slot->query_editor)) != slot->extra_location_widgets);

            slot_return_query_editor (slot);
            if (was_in_panel && panel != NULL) {
                nolphin_workspace_panel_show_preview (panel);
            }
        }

        if (slot->qe_changed_id > 0) {
            g_signal_handler_disconnect (slot->query_editor, slot->qe_changed_id);
            slot->qe_changed_id = 0;
        }

        if (slot->qe_cancel_id > 0) {
            g_signal_handler_disconnect (slot->query_editor, slot->qe_cancel_id);
            slot->qe_cancel_id = 0;
        }

        nolphin_query_editor_set_query (slot->query_editor, NULL);

        if (slot->content_view != NULL) {
            nolphin_view_grab_focus (slot->content_view);
        }
	}
}

static void
real_active (NolphinWindowSlot *slot)
{
	NolphinWindow *window;
	NolphinWindowPane *pane;
	int page_num;

	window = nolphin_window_slot_get_window (slot);
	pane = slot->pane;
	page_num = gtk_notebook_page_num (GTK_NOTEBOOK (pane->notebook),
					  GTK_WIDGET (slot));
	g_assert (page_num >= 0);

	gtk_notebook_set_current_page (GTK_NOTEBOOK (pane->notebook), page_num);

	/* sync window to new slot */
	nolphin_window_push_status (window, slot->status_text);
	nolphin_window_sync_allow_stop (window, slot);
	nolphin_window_sync_title (window, slot);
	nolphin_window_sync_zoom_widgets (window);
    nolphin_window_sync_bookmark_action (window);
	nolphin_window_pane_sync_location_widgets (slot->pane);
	nolphin_window_pane_sync_search_widgets (slot->pane);
	nolphin_window_sync_thumbnail_action(window);

	if (slot->viewed_file != NULL) {
		nolphin_window_sync_view_type (window);
		nolphin_window_load_extension_menus (window);
	}
}

static void
real_inactive (NolphinWindowSlot *slot)
{
	NolphinWindow *window;

	window = nolphin_window_slot_get_window (slot);
	g_assert (slot == nolphin_window_get_active_slot (window));
}

static void
floating_bar_action_cb (NolphinFloatingBar *floating_bar,
			gint action,
			NolphinWindowSlot *slot)
{
	if (action == NOLPHIN_FLOATING_BAR_ACTION_ID_STOP) {
		nolphin_window_slot_stop_loading (slot);
	}
}

static void
create_nsr_box (NolphinWindowSlot *slot)
{
    GtkWidget *box;
    GtkWidget *widget;
    PangoAttrList *attrs;

    box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);

    widget = gtk_image_new_from_icon_name ("xsi-search-symbolic", GTK_ICON_SIZE_DIALOG);
    gtk_box_pack_start (GTK_BOX (box), widget, FALSE, FALSE, 0);

    widget = gtk_label_new ("");
    attrs = pango_attr_list_new ();
    pango_attr_list_insert (attrs, pango_attr_size_new (20 * PANGO_SCALE));
    gtk_label_set_attributes (GTK_LABEL (widget), attrs);
    pango_attr_list_unref (attrs);
    gtk_box_pack_start (GTK_BOX (box), widget, FALSE, FALSE, 0);

    gtk_widget_set_halign (box, GTK_ALIGN_CENTER);
    gtk_widget_set_valign (box, GTK_ALIGN_CENTER);

    gtk_widget_show_all (box);
    gtk_widget_set_no_show_all (box, TRUE);
    gtk_widget_hide (box);

    slot->no_search_results_box = box;
    slot->no_results_label = widget;
}

static void
view_begin_loading_cb (NolphinView       *view,
                       NolphinWindowSlot *slot)
{
    if (gtk_revealer_get_reveal_child (GTK_REVEALER (slot->filter_bar_revealer))) {
        gtk_revealer_set_reveal_child (GTK_REVEALER (slot->filter_bar_revealer), FALSE);
        nolphin_filter_bar_set_text (NOLPHIN_FILTER_BAR (slot->filter_bar), "");
    }

    nolphin_view_grab_focus (view);
}

static void
filter_bar_cancel_cb (NolphinFilterBar *bar,
                      NolphinWindowSlot *slot)
{
    if (slot->content_view != NULL) {
        nolphin_view_clear_filter (slot->content_view);
    }
}

static void
view_activate_filter_cb (NolphinView   *view,
                         const char *filter_text,
                         NolphinWindowSlot *slot)
{
    if (filter_text != NULL && filter_text[0] != '\0') {
        gtk_revealer_set_reveal_child (GTK_REVEALER (slot->filter_bar_revealer), TRUE);
        nolphin_filter_bar_set_text (NOLPHIN_FILTER_BAR (slot->filter_bar), filter_text);

        if (NOLPHIN_VIEW_CLASS (G_OBJECT_GET_CLASS (view))->is_empty (view)) {
            gtk_label_set_text (GTK_LABEL (slot->no_results_label), _("Keine passenden Dateien"));
            gtk_widget_show (slot->no_search_results_box);
        } else {
            gtk_widget_hide (slot->no_search_results_box);
        }
    } else {
        gtk_widget_hide (slot->no_search_results_box);
        nolphin_window_slot_hide_filter_bar (slot);
    }
}

void
nolphin_window_slot_hide_filter_bar (NolphinWindowSlot *slot)
{
    g_return_if_fail (NOLPHIN_IS_WINDOW_SLOT (slot));

    if (!gtk_revealer_get_reveal_child (GTK_REVEALER (slot->filter_bar_revealer))) {
        return;
    }

    gtk_revealer_set_reveal_child (GTK_REVEALER (slot->filter_bar_revealer), FALSE);
    nolphin_filter_bar_set_text (NOLPHIN_FILTER_BAR (slot->filter_bar), "");

    if (slot->content_view != NULL) {
        nolphin_view_clear_filter (slot->content_view);
        nolphin_view_grab_focus (slot->content_view);
    }
}

static void
nolphin_window_slot_init (NolphinWindowSlot *slot)
{
	GtkWidget *extras_vbox;

	gtk_orientable_set_orientation (GTK_ORIENTABLE (slot),
					GTK_ORIENTATION_VERTICAL);
	gtk_widget_show (GTK_WIDGET (slot));

	extras_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	slot->extra_location_widgets = extras_vbox;
	gtk_box_pack_start (GTK_BOX (slot), extras_vbox, FALSE, FALSE, 0);
	gtk_widget_show (extras_vbox);

	slot->query_editor = NOLPHIN_QUERY_EDITOR (nolphin_query_editor_new ());

	nolphin_window_slot_add_extra_location_widget (slot, GTK_WIDGET (slot->query_editor));

    slot->filter_bar = nolphin_filter_bar_new ();
    slot->filter_bar_revealer = gtk_revealer_new ();
    gtk_revealer_set_transition_type (GTK_REVEALER (slot->filter_bar_revealer),
                                      GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN);
    gtk_revealer_set_transition_duration (GTK_REVEALER (slot->filter_bar_revealer), 150);
    gtk_container_add (GTK_CONTAINER (slot->filter_bar_revealer), slot->filter_bar);
    gtk_box_pack_start (GTK_BOX (slot), slot->filter_bar_revealer, FALSE, FALSE, 0);
    gtk_widget_show_all (slot->filter_bar_revealer);

    g_signal_connect (slot->filter_bar, "cancel",
                      G_CALLBACK (filter_bar_cancel_cb), slot);

	slot->view_overlay = gtk_overlay_new ();
	gtk_widget_add_events (slot->view_overlay,
			       GDK_ENTER_NOTIFY_MASK |
			       GDK_LEAVE_NOTIFY_MASK);
	gtk_box_pack_start (GTK_BOX (slot), slot->view_overlay, TRUE, TRUE, 0);
	gtk_widget_show (slot->view_overlay);

	slot->floating_bar = nolphin_floating_bar_new ("", FALSE);
	gtk_widget_set_halign (slot->floating_bar, GTK_ALIGN_END);
	gtk_widget_set_valign (slot->floating_bar, GTK_ALIGN_END);
	gtk_overlay_add_overlay (GTK_OVERLAY (slot->view_overlay),
				 slot->floating_bar);

    create_nsr_box (slot);
    gtk_overlay_add_overlay (GTK_OVERLAY (slot->view_overlay),
                             slot->no_search_results_box);

	g_signal_connect (slot->floating_bar, "action",
			  G_CALLBACK (floating_bar_action_cb), slot);

    slot->cache_bar = NULL;

	slot->title = g_strdup (_("Ladevorgang …"));
}

static void
view_end_loading_cb (NolphinView       *view,
		     		 gboolean        all_files_seen,
		     		 NolphinWindowSlot *slot)
{
	if (slot->needs_reload) {
		nolphin_window_slot_queue_reload (slot, FALSE);
		slot->needs_reload = FALSE;
	} else if (all_files_seen) {
        NolphinDirectory *directory;

        directory = nolphin_directory_get_for_file (slot->viewed_file);

        if (NOLPHIN_IS_SEARCH_DIRECTORY (directory) &&
            !nolphin_directory_is_not_empty (directory)) {
            gtk_label_set_text (GTK_LABEL (slot->no_results_label), _("Keine Dateien gefunden"));
            gtk_widget_show (slot->no_search_results_box);
        } else {
            gtk_widget_hide (slot->no_search_results_box);
        }

        nolphin_directory_unref (directory);
    }
}

static void
nolphin_window_slot_dispose (GObject *object)
{
	NolphinWindowSlot *slot;
	GtkWidget *widget;

	slot = NOLPHIN_WINDOW_SLOT (object);

	nolphin_window_slot_clear_forward_list (slot);
	nolphin_window_slot_clear_back_list (slot);

	/* Steckt die Suchleiste gerade in der rechten Leiste, mit dem Tab entfernen. */
	if (slot->query_editor != NULL &&
	    gtk_widget_get_parent (GTK_WIDGET (slot->query_editor)) != NULL &&
	    gtk_widget_get_parent (GTK_WIDGET (slot->query_editor)) != slot->extra_location_widgets) {
		gtk_widget_destroy (GTK_WIDGET (slot->query_editor));
	}
    nolphin_window_slot_remove_extra_location_widgets (slot);

	if (slot->content_view) {
		widget = GTK_WIDGET (slot->content_view);
		gtk_widget_destroy (widget);
		g_object_unref (slot->content_view);
		slot->content_view = NULL;
	}

	if (slot->new_content_view) {
		widget = GTK_WIDGET (slot->new_content_view);
		gtk_widget_destroy (widget);
		g_object_unref (slot->new_content_view);
		slot->new_content_view = NULL;
	}

	if (slot->set_status_timeout_id != 0) {
		g_source_remove (slot->set_status_timeout_id);
		slot->set_status_timeout_id = 0;
	}

	if (slot->loading_timeout_id != 0) {
		g_source_remove (slot->loading_timeout_id);
		slot->loading_timeout_id = 0;
	}

	nolphin_window_slot_set_viewed_file (slot, NULL);
	/* TODO? why do we unref here? the file is NULL.
	 * It was already here before the slot move, though */
	nolphin_file_unref (slot->viewed_file);

	if (slot->location) {
		/* TODO? why do we ref here, instead of unreffing?
		 * It was already here before the slot migration, though */
		g_object_ref (slot->location);
	}

	g_list_free_full (slot->pending_selection, g_object_unref);
	slot->pending_selection = NULL;

	g_clear_object (&slot->current_location_bookmark);
	g_clear_object (&slot->last_location_bookmark);

	if (slot->find_mount_cancellable != NULL) {
		g_cancellable_cancel (slot->find_mount_cancellable);
		slot->find_mount_cancellable = NULL;
	}

	slot->pane = NULL;

	g_free (slot->title);
	slot->title = NULL;

	g_free (slot->custom_title);
	slot->custom_title = NULL;

	g_free (slot->status_text);
	slot->status_text = NULL;

	G_OBJECT_CLASS (nolphin_window_slot_parent_class)->dispose (object);
}

static void
nolphin_window_slot_class_init (NolphinWindowSlotClass *klass)
{
	GObjectClass *oclass = G_OBJECT_CLASS (klass);

	klass->active = real_active;
	klass->inactive = real_inactive;

	oclass->dispose = nolphin_window_slot_dispose;

	signals[ACTIVE] =
		g_signal_new ("active",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (NolphinWindowSlotClass, active),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	signals[INACTIVE] =
		g_signal_new ("inactive",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (NolphinWindowSlotClass, inactive),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	signals[CHANGED_PANE] =
		g_signal_new ("changed-pane",
			G_TYPE_FROM_CLASS (klass),
			G_SIGNAL_RUN_LAST,
			G_STRUCT_OFFSET (NolphinWindowSlotClass, changed_pane),
			NULL, NULL,
			g_cclosure_marshal_VOID__VOID,
			G_TYPE_NONE, 0);

	signals[LOCATION_CHANGED] =
		g_signal_new ("location-changed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL,
			      g_cclosure_marshal_generic,
			      G_TYPE_NONE, 2,
			      G_TYPE_STRING,
			      G_TYPE_STRING);
}

GFile *
nolphin_window_slot_get_location (NolphinWindowSlot *slot)
{
	g_assert (slot != NULL);

	if (slot->location != NULL) {
		return g_object_ref (slot->location);
	}
	return NULL;
}

char *
nolphin_window_slot_get_location_uri (NolphinWindowSlot *slot)
{
	g_assert (NOLPHIN_IS_WINDOW_SLOT (slot));

	if (slot->location) {
		return g_file_get_uri (slot->location);
	}
	return NULL;
}

void
nolphin_window_slot_make_hosting_pane_active (NolphinWindowSlot *slot)
{
	g_assert (NOLPHIN_IS_WINDOW_PANE (slot->pane));

	nolphin_window_set_active_slot (nolphin_window_slot_get_window (slot),
					 slot);
}

NolphinWindow *
nolphin_window_slot_get_window (NolphinWindowSlot *slot)
{
	g_assert (NOLPHIN_IS_WINDOW_SLOT (slot));
	return slot->pane->window;
}

/* nolphin_window_slot_update_title:
 *
 * Re-calculate the slot title.
 * Called when the location or view has changed.
 * @slot: The NolphinWindowSlot in question.
 *
 */
void
nolphin_window_slot_update_title (NolphinWindowSlot *slot)
{
	NolphinWindow *window;
	char *title;
	gboolean do_sync = FALSE;

	title = nolphin_compute_title_for_location (slot->location);
	window = nolphin_window_slot_get_window (slot);

	if (g_strcmp0 (title, slot->title) != 0) {
		do_sync = TRUE;

		g_free (slot->title);
		slot->title = title;
		title = NULL;
	}

	if (strlen (slot->title) > 0 &&
	    slot->current_location_bookmark != NULL) {
		do_sync = TRUE;
	}

	if (do_sync) {
		nolphin_window_sync_title (window, slot);
	}

	if (title != NULL) {
		g_free (title);
	}
}

/* nolphin_window_slot_update_icon:
 *
 * Re-calculate the slot icon
 * Called when the location or view or icon set has changed.
 * @slot: The NolphinWindowSlot in question.
 */
void
nolphin_window_slot_update_icon (NolphinWindowSlot *slot)
{
	NolphinWindow *window;
	NolphinIconInfo *info;
	const char *icon_name;
	GdkPixbuf *pixbuf;

	window = nolphin_window_slot_get_window (slot);
	info = NOLPHIN_WINDOW_CLASS (G_OBJECT_GET_CLASS (window))->get_icon (window, slot);

	icon_name = NULL;
	if (info) {
		icon_name = nolphin_icon_info_get_used_name (info);
		if (icon_name != NULL) {
			/* Gtk+ doesn't short circuit this (yet), so avoid lots of work
			 * if we're setting to the same icon. This happens a lot e.g. when
			 * the trash directory changes due to the file count changing.
			 */
			if (g_strcmp0 (icon_name, gtk_window_get_icon_name (GTK_WINDOW (window))) != 0) {
				gtk_window_set_icon_name (GTK_WINDOW (window), icon_name);
			}
		} else {
			pixbuf = nolphin_icon_info_get_pixbuf_nodefault (info);

			if (pixbuf) {
				gtk_window_set_icon (GTK_WINDOW (window), pixbuf);
				g_object_unref (pixbuf);
			}
		}

        nolphin_icon_info_unref (info);
	}
}

void
nolphin_window_slot_set_show_thumbnails (NolphinWindowSlot *slot,
                                      gboolean show_thumbnails)
{
  NolphinDirectory *directory;

  directory = nolphin_directory_get (slot->location);
  nolphin_directory_set_show_thumbnails(directory, show_thumbnails);
  nolphin_directory_unref (directory);
}

void
nolphin_window_slot_set_content_view_widget (NolphinWindowSlot *slot,
					      NolphinView *new_view)
{
	NolphinWindow *window;
	GtkWidget *widget;

	window = nolphin_window_slot_get_window (slot);

	if (slot->content_view != NULL) {
		/* disconnect old view */
        g_signal_handlers_disconnect_by_func (slot->content_view, G_CALLBACK (view_end_loading_cb), slot);
        g_signal_handlers_disconnect_by_func (slot->content_view, G_CALLBACK (view_begin_loading_cb), slot);

        if (slot->filter_activate_handler_id > 0) {
            g_signal_handler_disconnect (slot->content_view, slot->filter_activate_handler_id);
            slot->filter_activate_handler_id = 0;
        }

		nolphin_window_disconnect_content_view (window, slot->content_view);

		widget = GTK_WIDGET (slot->content_view);
		gtk_widget_destroy (widget);
		g_object_unref (slot->content_view);
		slot->content_view = NULL;
	}

    /* Hide filter bar when switching views */
    nolphin_window_slot_hide_filter_bar (slot);

	if (new_view != NULL) {
		widget = GTK_WIDGET (new_view);
		gtk_container_add (GTK_CONTAINER (slot->view_overlay), widget);
		gtk_widget_show (widget);

		slot->content_view = new_view;
		g_object_ref (slot->content_view);

		g_signal_connect (new_view, "end_loading", G_CALLBACK (view_end_loading_cb), slot);
		g_signal_connect (new_view, "begin_loading", G_CALLBACK (view_begin_loading_cb), slot);

        slot->filter_activate_handler_id =
            g_signal_connect (new_view, "activate-filter",
                              G_CALLBACK (view_activate_filter_cb), slot);

		/* connect new view */
		nolphin_window_connect_content_view (window, new_view);
	}
}

void
nolphin_window_slot_set_allow_stop (NolphinWindowSlot *slot,
				     gboolean allow)
{
	NolphinWindow *window;

	g_assert (NOLPHIN_IS_WINDOW_SLOT (slot));

	slot->allow_stop = allow;

	window = nolphin_window_slot_get_window (slot);
	nolphin_window_sync_allow_stop (window, slot);
}

static void
real_slot_set_short_status (NolphinWindowSlot *slot,
			    const gchar *status)
{

	gboolean show_statusbar;
	gboolean disable_chrome;

	nolphin_floating_bar_cleanup_actions (NOLPHIN_FLOATING_BAR (slot->floating_bar));
	nolphin_floating_bar_set_show_spinner (NOLPHIN_FLOATING_BAR (slot->floating_bar),
						FALSE);

	show_statusbar = g_settings_get_boolean (nolphin_window_state,
						 NOLPHIN_WINDOW_STATE_START_WITH_STATUS_BAR);

	g_object_get (nolphin_window_slot_get_window (slot),
		      "disable-chrome", &disable_chrome,
		      NULL);

	if (status == NULL || show_statusbar || disable_chrome) {
		gtk_widget_hide (slot->floating_bar);
		return;
	}

	nolphin_floating_bar_set_label (NOLPHIN_FLOATING_BAR (slot->floating_bar), status);
	gtk_widget_show (slot->floating_bar);
}

typedef struct {
	gchar *status;
	NolphinWindowSlot *slot;
} SetStatusData;

static void
set_status_data_free (gpointer data)
{
	SetStatusData *status_data = data;

	g_free (status_data->status);

	g_free (data);
}

static gboolean
set_status_timeout_cb (gpointer data)
{
	SetStatusData *status_data = data;

	status_data->slot->set_status_timeout_id = 0;
	real_slot_set_short_status (status_data->slot, status_data->status);

	return FALSE;
}

static void
set_floating_bar_status (NolphinWindowSlot *slot,
			 const gchar *status)
{
	GtkSettings *settings;
	gint double_click_time;
	SetStatusData *status_data;

	if (slot->set_status_timeout_id != 0) {
		g_source_remove (slot->set_status_timeout_id);
		slot->set_status_timeout_id = 0;
	}

	settings = gtk_settings_get_for_screen (gtk_widget_get_screen (GTK_WIDGET (slot->content_view)));
	g_object_get (settings,
		      "gtk-double-click-time", &double_click_time,
		      NULL);

	status_data = g_new0 (SetStatusData, 1);
	status_data->status = g_strdup (status);
	status_data->slot = slot;

	/* waiting for half of the double-click-time before setting
	 * the status seems to be a good approximation of not setting it
	 * too often and not delaying the statusbar too much.
	 */
	slot->set_status_timeout_id =
		g_timeout_add_full (G_PRIORITY_DEFAULT,
				    (guint) (double_click_time / 2),
				    set_status_timeout_cb,
				    status_data,
				    set_status_data_free);
}

void
nolphin_window_slot_set_status (NolphinWindowSlot *slot,
                             const char *status,
                             const char *short_status,
                             gboolean    location_loading)
{
	NolphinWindow *window;

	g_assert (NOLPHIN_IS_WINDOW_SLOT (slot));

	g_free (slot->status_text);
	slot->status_text = g_strdup (status);

	if (slot->content_view != NULL && !location_loading) {
		set_floating_bar_status (slot, short_status);
	}

	window = nolphin_window_slot_get_window (slot);
	if (slot == nolphin_window_get_active_slot (window)) {
		nolphin_window_push_status (window, slot->status_text);
	}
}

static void
remove_all_extra_location_widgets (GtkWidget *widget,
				   gpointer data)
{
	NolphinWindowSlot *slot = data;
	NolphinDirectory *directory;

	directory = nolphin_directory_get (slot->location);
	if (widget != GTK_WIDGET (slot->query_editor)) {
		gtk_container_remove (GTK_CONTAINER (slot->extra_location_widgets), widget);
	}

	nolphin_directory_unref (directory);
}

void
nolphin_window_slot_remove_extra_location_widgets (NolphinWindowSlot *slot)
{
    gtk_container_foreach (GTK_CONTAINER (slot->extra_location_widgets),
                           remove_all_extra_location_widgets,
                           slot);
}

void
nolphin_window_slot_add_extra_location_widget (NolphinWindowSlot *slot,
						GtkWidget *widget)
{
	gtk_box_pack_start (GTK_BOX (slot->extra_location_widgets),
			    widget, TRUE, TRUE, 0);
	gtk_widget_show (slot->extra_location_widgets);
}

/* returns either the pending or the actual current uri */
char *
nolphin_window_slot_get_current_uri (NolphinWindowSlot *slot)
{
	if (slot->pending_location != NULL) {
		return g_file_get_uri (slot->pending_location);
	}

	if (slot->location != NULL) {
		return g_file_get_uri (slot->location);
	}

	g_assert_not_reached ();
	return NULL;
}

NolphinView *
nolphin_window_slot_get_current_view (NolphinWindowSlot *slot)
{
	if (slot->content_view != NULL) {
		return slot->content_view;
	} else if (slot->new_content_view) {
		return slot->new_content_view;
	}

	return NULL;
}

void
nolphin_window_slot_go_home (NolphinWindowSlot *slot,
			      NolphinWindowOpenFlags flags)
{
	GFile *home;

	g_return_if_fail (NOLPHIN_IS_WINDOW_SLOT (slot));

	home = g_file_new_for_path (g_get_home_dir ());
	nolphin_window_slot_open_location (slot, home, flags);
	g_object_unref (home);
}

void
nolphin_window_slot_go_up (NolphinWindowSlot *slot,
			    NolphinWindowOpenFlags flags)
{
	GFile *parent;
	char * uri;

	if (slot->location == NULL) {
		return;
	}

	parent = g_file_get_parent (slot->location);
	if (parent == NULL) {
		if (g_file_has_uri_scheme (slot->location, "smb")) {
			uri = g_file_get_uri (slot->location);

            DEBUG ("Starting samba URI for navigation: %s", uri);

			if (g_strcmp0 ("smb:///", uri) == 0) {
				parent = g_file_new_for_uri ("network:///");
			}
			else {
                GString *gstr;
                char * temp;

                gstr = g_string_new (uri);

				// Remove last /
                if (g_str_has_suffix (gstr->str, "/")) {
                    gstr = g_string_set_size (gstr, gstr->len - 1);
                }

				// Remove last part of string after last remaining /
				temp = g_strrstr (gstr->str, "/") + 1;
				if (temp != NULL) {
                    gstr = g_string_set_size (gstr, temp - gstr->str);
				}

                // if we're going to end up with smb://, redirect it to network instead.
                if (g_strcmp0 ("smb://", gstr->str) == 0) {
                    gstr = g_string_assign (gstr, "network:///");
                }

                uri = g_string_free (gstr, FALSE);

				parent = g_file_new_for_uri (uri);

                DEBUG ("Ending samba URI for navigation: %s", uri);
			}
			g_free (uri);
		}
		else {
			return;
		}
	}

	nolphin_window_slot_open_location (slot, parent, flags);
	g_object_unref (parent);
}

void
nolphin_window_slot_clear_forward_list (NolphinWindowSlot *slot)
{
	g_assert (NOLPHIN_IS_WINDOW_SLOT (slot));

	g_list_free_full (slot->forward_list, g_object_unref);
	slot->forward_list = NULL;
}

void
nolphin_window_slot_clear_back_list (NolphinWindowSlot *slot)
{
	g_assert (NOLPHIN_IS_WINDOW_SLOT (slot));

	g_list_free_full (slot->back_list, g_object_unref);
	slot->back_list = NULL;
}

gboolean
nolphin_window_slot_should_close_with_mount (NolphinWindowSlot *slot,
					      GMount *mount)
{
	GFile *mount_location;
	gboolean close_with_mount;

	mount_location = g_mount_get_root (mount);
	close_with_mount =
		g_file_has_prefix (NOLPHIN_WINDOW_SLOT (slot)->location, mount_location) ||
		g_file_equal (NOLPHIN_WINDOW_SLOT (slot)->location, mount_location);

	g_object_unref (mount_location);

	return close_with_mount;
}

NolphinWindowSlot *
nolphin_window_slot_new (NolphinWindowPane *pane)
{
	NolphinWindowSlot *slot;

	slot = g_object_new (NOLPHIN_TYPE_WINDOW_SLOT, NULL);
	slot->pane = pane;

	return slot;
}
