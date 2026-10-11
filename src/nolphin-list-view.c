/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* fm-list-view.c - implementation of list view of directory.

   Copyright (C) 2000 Eazel, Inc.
   Copyright (C) 2001, 2002 Anders Carlsson <andersca@gnu.org>

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

   Authors: John Sullivan <sullivan@eazel.com>
            Anders Carlsson <andersca@gnu.org>
	    David Emory Watson <dwatson@cs.ucr.edu>
*/

#include <config.h>
#include "nolphin-list-view.h"

#include "nolphin-application.h"
#include "nolphin-list-model.h"
#include <libnolphin-private/nolphin-fzy-utils.h>
#include "nolphin-error-reporting.h"
#include "nolphin-view-dnd.h"
#include "nolphin-view-factory.h"
#include "nolphin-window.h"
#include "nolphin-window-slot.h"

#include <string.h>
#include <eel/eel-vfs-extensions.h>
#include <eel/eel-gdk-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-glib-extensions.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <libegg/eggtreemultidnd.h>
#include <glib/gi18n.h>
#include <glib-object.h>
#include <libnolphin-extension/nolphin-column-provider.h>
#include <libnolphin-private/nolphin-clipboard-monitor.h>
#include <libnolphin-private/nolphin-column-chooser.h>
#include <libnolphin-private/nolphin-column-utilities.h>
#include <libnolphin-private/nolphin-dnd.h>
#include <libnolphin-private/nolphin-file-dnd.h>
#include <libnolphin-private/nolphin-file-utilities.h>
#include <libnolphin-private/nolphin-ui-utilities.h>
#include <libnolphin-private/nolphin-global-preferences.h>
#include <libnolphin-private/nolphin-icon-dnd.h>
#include <libnolphin-private/nolphin-metadata.h>
#include <libnolphin-private/nolphin-module.h>
#include <libnolphin-private/nolphin-thumbnails.h>
#include <libnolphin-private/nolphin-tree-view-drag-dest.h>
#include <libnolphin-private/nolphin-clipboard.h>

#define DEBUG_FLAG NOLPHIN_DEBUG_LIST_VIEW
#include <libnolphin-private/nolphin-debug.h>

struct NolphinListViewDetails {
	GtkTreeView *tree_view;
	NolphinListModel *model;
	GtkActionGroup *list_action_group;
	guint list_merge_id;

	GtkTreeViewColumn   *file_name_column;
	int file_name_column_num;

	GtkCellRendererPixbuf *pixbuf_cell;
	GtkCellRendererText   *file_name_cell;
	GList *cells;
	GtkCellEditable *editable_widget;

	NolphinZoomLevel zoom_level;

	NolphinTreeViewDragDest *drag_dest;

	GtkTreePath *double_click_path[2]; /* Both clicks in a double click need to be on the same row */

	GtkTreePath *new_selection_path;   /* Path of the new selection after removing a file */

	GtkTreePath *hover_path;

	guint drag_button;
	int drag_x;
	int drag_y;

    gint ok_to_load_deferred_attrs;
    guint update_visible_icons_id;

    gboolean rename_on_release;
	gboolean drag_started;
	gboolean ignore_button_release;
	gboolean row_selected_on_button_down;
	gboolean menus_ready;
	gboolean active;

    gboolean rubber_banding;

	GHashTable *columns;
	GtkWidget *column_editor;

	char *original_name;

	NolphinFile *renaming_file;
	gboolean rename_done;
	guint renaming_file_activate_timeout;

	gulong clipboard_handler_id;

	GQuark last_sort_attr;

    gboolean tooltip_flags;
    gboolean show_tooltips;

    gboolean click_to_rename;

    GList *current_selection;
    gint current_selection_count;

    gboolean overlay_scrolling;
};

struct SelectionForeachData {
	GList *list;
	GtkTreeSelection *selection;
};

/*
 * The row height should be large enough to not clip emblems.
 * Computing this would be costly, so we just choose a number
 * that works well with the set of emblems we've designed.
 */
#define LIST_VIEW_MINIMUM_ROW_HEIGHT	28

/* We wait two seconds after row is collapsed to unload the subdirectory */
#define COLLAPSE_TO_UNLOAD_DELAY 2

/* Wait for the rename to end when activating a file being renamed */
#define WAIT_FOR_RENAME_ON_ACTIVATE 200

#define INITIAL_UPDATE_VISIBLE_DELAY 300
#define NORMAL_UPDATE_VISIBLE_DELAY 50

static GdkCursor *              hand_cursor = NULL;

static GtkTargetList *          source_target_list = NULL;

static GList *nolphin_list_view_get_selection                   (NolphinView   *view);
static void   nolphin_list_view_update_selection                (NolphinView *view);
static GList *nolphin_list_view_get_selection_for_file_transfer (NolphinView   *view);
static void   nolphin_list_view_set_zoom_level                  (NolphinListView        *view,
								  NolphinZoomLevel  new_level,
								  gboolean           always_set_level);
static void   nolphin_list_view_scale_font_size                 (NolphinListView        *view,
								  NolphinZoomLevel  new_level);
static void   nolphin_list_view_scroll_to_file                  (NolphinListView        *view,
								  NolphinFile      *file);
static void   nolphin_list_view_rename_callback                 (NolphinFile      *file,
								  GFile             *result_location,
								  GError            *error,
								  gpointer           callback_data);

static void nolphin_list_view_start_renaming_file               (NolphinView *view,
                                                              NolphinFile *file,
                                                              gboolean  select_all);

static void   apply_columns_settings                             (NolphinListView *list_view,
                                                                  char **column_order,
                                                                  char **visible_columns);
static char **get_visible_columns                                (NolphinListView *list_view);
static char **get_default_visible_columns                        (NolphinListView *list_view);
static char **get_column_order                                   (NolphinListView *list_view);
static char **get_default_column_order                           (NolphinListView *list_view);

static void   set_columns_settings_from_metadata_and_preferences (NolphinListView *list_view);
static void   queue_update_visible_icons (NolphinListView *view, gint delay);
static NolphinZoomLevel nolphin_list_view_get_zoom_level (NolphinView *view);
static void   prioritize_visible_files (NolphinListView *view);

G_DEFINE_TYPE (NolphinListView, nolphin_list_view, NOLPHIN_TYPE_VIEW);

static gint click_policy = NOLPHIN_CLICK_POLICY_SINGLE;

static const char * default_trash_visible_columns[] = {
	"name", "size", "type", "trashed_on", "trash_orig_path", NULL
};

static const char * default_trash_columns_order[] = {
	"name", "size", "type", "trashed_on", "trash_orig_path", NULL
};

static const char * default_recent_visible_columns[] = {
    "name", "size", "type", "date_accessed", NULL
};

static const char * default_recent_columns_order[] = {
    "name", "size", "type", "date_accessed", NULL
};

static const char * default_favorites_visible_columns[] = {
    "name", "size", "date_modified", NULL
};

static const char * default_favorites_columns_order[] = {
    "name", "size", "date_modified", NULL
};

static const char * default_search_columns[] = {
    "name", "where", "date_modified", NULL
};

static gchar **
string_array_from_string_glist (GList *list)
{
    GPtrArray *res;
    GList *l;
    gchar **ret;

    res = g_ptr_array_new ();

    for (l = list; l != NULL; l = l->next) {
        g_ptr_array_add (res, g_strdup (l->data));
    }

    g_ptr_array_add (res, NULL);

    ret = (char **) g_ptr_array_free (res, FALSE);

    return ret;
}

static const gchar*
get_default_sort_order (NolphinFile *file, gboolean *reversed)
{
	NolphinFileSortType default_sort_order;
	gboolean default_sort_reversed;
	const gchar *retval;
	const char *attributes[] = {
		"name", /* is really "manually" which doesn't apply to lists */
		"name",
		"size",
		"type",
		"detailed_type",
		"date_modified",
		"date_accessed",
		"trashed_on",
		NULL
	};

	retval = nolphin_file_get_default_sort_attribute (file, reversed);

	if (retval == NULL) {
		default_sort_order = g_settings_get_enum (nolphin_preferences,
							  NOLPHIN_PREFERENCES_DEFAULT_SORT_ORDER);
		default_sort_reversed = g_settings_get_boolean (nolphin_preferences,
								NOLPHIN_PREFERENCES_DEFAULT_SORT_IN_REVERSE_ORDER);

		retval = attributes[default_sort_order];
		*reversed = default_sort_reversed;
	}

	return retval;
}

static void nolphin_list_view_update_filter_text (NolphinView *view, const char *filter_text);
static void nolphin_list_view_select_first        (NolphinView *view);

static void
tooltip_prefs_changed_callback (NolphinListView *view)
{
    view->details->show_tooltips = g_settings_get_boolean (nolphin_preferences,
                                                           NOLPHIN_PREFERENCES_TOOLTIPS_LIST_VIEW);

    view->details->tooltip_flags = nolphin_global_preferences_get_tooltip_flags ();
}

static void
expanders_enabled_changed_cb (NolphinListView *view)
{
    gboolean enabled;
    NolphinWindowSlot *slot;

    g_return_if_fail (NOLPHIN_IS_LIST_VIEW (view));
    g_return_if_fail (GTK_IS_TREE_VIEW (view->details->tree_view) && view->details->tree_view != NULL);

    enabled = g_settings_get_boolean (nolphin_list_view_preferences,
                                      NOLPHIN_PREFERENCES_LIST_VIEW_ENABLE_EXPANSION);

    gtk_tree_view_collapse_all (view->details->tree_view);
    gtk_tree_view_set_show_expanders (view->details->tree_view, enabled);
    nolphin_list_model_set_expansion_enabled (view->details->model, enabled);

    slot = nolphin_view_get_nolphin_window_slot (NOLPHIN_VIEW (view));
    if (slot != NULL) {
        nolphin_window_slot_queue_reload (slot, FALSE);
    }
}

static void
list_selection_changed_callback (GtkTreeSelection *selection, gpointer user_data)
{
	NolphinView *view;

	view = NOLPHIN_VIEW (user_data);

	nolphin_view_notify_selection_changed (view);
}

/* Move these to eel? */

static void
tree_selection_foreach_set_boolean (GtkTreeModel *model,
				    GtkTreePath *path,
				    GtkTreeIter *iter,
				    gpointer callback_data)
{
	* (gboolean *) callback_data = TRUE;
}

static gboolean
tree_selection_not_empty (GtkTreeSelection *selection)
{
	gboolean not_empty;

	not_empty = FALSE;
	gtk_tree_selection_selected_foreach (selection,
					     tree_selection_foreach_set_boolean,
					     &not_empty);
	return not_empty;
}

static gboolean
tree_view_has_selection (GtkTreeView *view)
{
	return tree_selection_not_empty (gtk_tree_view_get_selection (view));
}

static void
preview_selected_items (NolphinListView *view)
{
	GList *file_list;

	file_list = nolphin_list_view_get_selection (NOLPHIN_VIEW (view));

	if (file_list != NULL) {
		nolphin_view_preview_files (NOLPHIN_VIEW (view),
					     file_list, NULL);
		nolphin_file_list_free (file_list);
	}
}

static void
activate_selected_items (NolphinListView *view)
{
	GList *file_list;

	file_list = nolphin_list_view_get_selection (NOLPHIN_VIEW (view));


	if (view->details->renaming_file) {
		/* We're currently renaming a file, wait until the rename is
		   finished, or the activation uri will be wrong */
		if (view->details->renaming_file_activate_timeout == 0) {
			view->details->renaming_file_activate_timeout =
				g_timeout_add (WAIT_FOR_RENAME_ON_ACTIVATE, (GSourceFunc) activate_selected_items, view);
		}
		return;
	}

	if (view->details->renaming_file_activate_timeout != 0) {
		g_source_remove (view->details->renaming_file_activate_timeout);
		view->details->renaming_file_activate_timeout = 0;
	}

	nolphin_view_activate_files (NOLPHIN_VIEW (view),
				      file_list,
				      0, TRUE);
	nolphin_file_list_free (file_list);

}

static void
activate_selected_items_alternate (NolphinListView *view,
				   NolphinFile *file,
				   gboolean open_in_tab)
{
	GList *file_list;
	NolphinWindowOpenFlags flags;

	flags = 0;

	if (g_settings_get_boolean (nolphin_preferences,
				    NOLPHIN_PREFERENCES_ALWAYS_USE_BROWSER)) {
		if (open_in_tab) {
			flags |= NOLPHIN_WINDOW_OPEN_FLAG_NEW_TAB;
		} else {
			flags |= NOLPHIN_WINDOW_OPEN_FLAG_NEW_WINDOW;
		}
	} else {
		flags |= NOLPHIN_WINDOW_OPEN_FLAG_CLOSE_BEHIND;
	}

	if (file != NULL) {
		nolphin_file_ref (file);
		file_list = g_list_prepend (NULL, file);
	} else {
		file_list = nolphin_list_view_get_selection (NOLPHIN_VIEW (view));
	}
	nolphin_view_activate_files (NOLPHIN_VIEW (view),
				      file_list,
				      flags,
				      TRUE);
	nolphin_file_list_free (file_list);

}

static gboolean
button_event_modifies_selection (GdkEventButton *event)
{
	return (event->state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) != 0;
}

static void
nolphin_list_view_did_not_drag (NolphinListView *view,
				 GdkEventButton *event)
{
	GtkTreeView *tree_view;
	GtkTreeSelection *selection;
	GtkTreePath *path;

	tree_view = view->details->tree_view;
	selection = gtk_tree_view_get_selection (tree_view);

	if (gtk_tree_view_get_path_at_pos (tree_view, event->x, event->y,
					   &path, NULL, NULL, NULL)) {
		if ((event->button == 1 || event->button == 2)
		    && ((event->state & GDK_CONTROL_MASK) != 0 ||
			(event->state & GDK_SHIFT_MASK) == 0)
		    && view->details->row_selected_on_button_down) {
			if (!button_event_modifies_selection (event)) {
				gtk_tree_selection_unselect_all (selection);
				gtk_tree_selection_select_path (selection, path);
			} else {
				gtk_tree_selection_unselect_path (selection, path);
			}
		}

		if ((click_policy == NOLPHIN_CLICK_POLICY_SINGLE)
		    && !button_event_modifies_selection(event)) {
			if (event->button == 1) {
				activate_selected_items (view);
			} else if (event->button == 2) {
				activate_selected_items_alternate (view, NULL, TRUE);
			}
		}

        if (view->details->rename_on_release) {
            NolphinFile *file = nolphin_list_model_file_for_path (view->details->model, path);
            nolphin_list_view_start_renaming_file (NOLPHIN_VIEW (view),
                                                file,
                                                nolphin_file_is_directory (file));
            nolphin_file_unref (file);
            view->details->rename_on_release = FALSE;
        }

		gtk_tree_path_free (path);
	}

}

static void
drag_data_get_callback (GtkWidget *widget,
			GdkDragContext *context,
			GtkSelectionData *selection_data,
			guint info,
			guint time)
{
	GtkTreeView *tree_view;
	GtkTreeModel *model;
	GList *ref_list;

	tree_view = GTK_TREE_VIEW (widget);

	model = gtk_tree_view_get_model (tree_view);

	if (model == NULL) {
		return;
	}

	ref_list = g_object_get_data (G_OBJECT (context), "drag-info");

	if (ref_list == NULL) {
		return;
	}

	if (EGG_IS_TREE_MULTI_DRAG_SOURCE (model)) {
		egg_tree_multi_drag_source_drag_data_get (EGG_TREE_MULTI_DRAG_SOURCE (model),
							  ref_list,
							  selection_data);
	}
}

static void
filtered_selection_foreach (GtkTreeModel *model,
			    GtkTreePath *path,
			    GtkTreeIter *iter,
			    gpointer data)
{
	struct SelectionForeachData *selection_data;
	GtkTreeIter parent;
	GtkTreeIter child;

	selection_data = data;

	/* If the parent folder is also selected, don't include this file in the
	 * file operation, since that would copy it to the toplevel target instead
	 * of keeping it as a child of the copied folder
	 */
	child = *iter;
	while (gtk_tree_model_iter_parent (model, &parent, &child)) {
		if (gtk_tree_selection_iter_is_selected (selection_data->selection,
							 &parent)) {
			return;
		}
		child = parent;
	}

	selection_data->list = g_list_prepend (selection_data->list,
					       gtk_tree_row_reference_new (model, path));
}

static GList *
get_filtered_selection_refs (GtkTreeView *tree_view)
{
	struct SelectionForeachData selection_data;

	selection_data.list = NULL;
	selection_data.selection = gtk_tree_view_get_selection (tree_view);

	gtk_tree_selection_selected_foreach (selection_data.selection,
					     filtered_selection_foreach,
					     &selection_data);
	return g_list_reverse (selection_data.list);
}

static void
ref_list_free (GList *ref_list)
{
	g_list_foreach (ref_list, (GFunc) gtk_tree_row_reference_free, NULL);
	g_list_free (ref_list);
}

static void
stop_drag_check (NolphinListView *view)
{
	view->details->drag_button = 0;
}

static cairo_surface_t *
get_drag_surface (NolphinListView *view)
{
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter;
	cairo_surface_t *ret;
	GdkRectangle cell_area;

	ret = NULL;

	if (gtk_tree_view_get_path_at_pos (view->details->tree_view,
					   view->details->drag_x,
					   view->details->drag_y,
					   &path, NULL, NULL, NULL)) {
		model = gtk_tree_view_get_model (view->details->tree_view);
		gtk_tree_model_get_iter (model, &iter, path);
		gtk_tree_model_get (model, &iter,
				    nolphin_list_model_get_column_id_from_zoom_level (view->details->zoom_level),
				    &ret,
				    -1);

		gtk_tree_view_get_cell_area (view->details->tree_view,
					     path,
					     view->details->file_name_column,
					     &cell_area);

		gtk_tree_path_free (path);
	}

	return ret;
}

static void
drag_begin_callback (GtkWidget *widget,
		     GdkDragContext *context,
		     NolphinListView *view)
{
	GList *ref_list;

    cairo_surface_t *surface;

    surface = get_drag_surface (view);
    if (surface) {
        gtk_drag_set_icon_surface (context, surface);
        cairo_surface_destroy (surface);
	} else {
		gtk_drag_set_icon_default (context);
	}

	stop_drag_check (view);
	view->details->drag_started = TRUE;

	ref_list = get_filtered_selection_refs (GTK_TREE_VIEW (widget));
	g_object_set_data_full (G_OBJECT (context),
				"drag-info",
				ref_list,
				(GDestroyNotify)ref_list_free);
}

static void
drag_end_callback (GtkWidget *widget,
             GdkDragContext *context,
             NolphinListView *view)
{
    view->details->drag_started = FALSE;
}

static gboolean
motion_notify_callback (GtkWidget *widget,
			GdkEventMotion *event,
			gpointer callback_data)
{
	NolphinListView *view;

	view = NOLPHIN_LIST_VIEW (callback_data);

    if (event->window != gtk_tree_view_get_bin_window (GTK_TREE_VIEW (widget))) {
        return GDK_EVENT_PROPAGATE;
    }

	if (click_policy == NOLPHIN_CLICK_POLICY_SINGLE) {
		GtkTreePath *old_hover_path;

		old_hover_path = view->details->hover_path;
		gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (widget),
					       event->x, event->y,
					       &view->details->hover_path,
					       NULL, NULL, NULL);

		if ((old_hover_path != NULL) != (view->details->hover_path != NULL)) {
			if (view->details->hover_path != NULL) {
				gdk_window_set_cursor (gtk_widget_get_window (widget), hand_cursor);
			} else {
				gdk_window_set_cursor (gtk_widget_get_window (widget), NULL);
			}
		}

		if (old_hover_path != NULL) {
			gtk_tree_path_free (old_hover_path);
		}
	}

    /* If we're already rubber-banding, we can skip all of this logic and just let the parent
     * class continue to handle selection */
    if (view->details->drag_button != 0 && !view->details->rubber_banding) {
        GtkTreePath *path;
        GtkTreeSelection *selection;
        gboolean is_new_self_selection;

        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));

        gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (widget),
                                       view->details->drag_x,
                                       view->details->drag_y,
                                       &path,
                                       NULL, NULL, NULL);

        /* This looks complicated but it's just verbose:  We'll only consider allowing rubber-banding
         * to begin if the following are TRUE: a) The current row is the only row currently selected,
         * and  b) This is the first click that's been made on this row - meaning, the button-press-event
         * that preceded this motion-event was the one that caused this row to be selected. */
        is_new_self_selection = gtk_tree_selection_count_selected_rows (selection) == 1 &&
                                gtk_tree_selection_path_is_selected (selection, path) &&
                                (!view->details->double_click_path[1] ||
                                (view->details->double_click_path[1] &&
                                gtk_tree_path_compare (view->details->double_click_path[0],
                                                       view->details->double_click_path[1]) != 0));

        gtk_tree_path_free (path);

        /* We also want to further restrict rubber-banding to be initiated only in blank areas of the row.
         * This allows DnD to operate on a new selection like before, when the motion begins over text or
         * icons */
        if (is_new_self_selection && gtk_tree_view_is_blank_at_pos (GTK_TREE_VIEW (widget),
                                                                    view->details->drag_x,
                                                                    view->details->drag_y,
                                                                    NULL, NULL, NULL, NULL)) {
            /* If this is a candidate for rubber-banding, track that state in the view, and allow the event
             * to continue into Gtk (which handles rubber-band selection for us) */
            view->details->rubber_banding = TRUE;

            return GDK_EVENT_PROPAGATE;
        }

        /* All other cases, allow DnD to potentially begin */
        if (!source_target_list) {
            source_target_list = nolphin_list_model_get_drag_target_list ();
        }

        if (gtk_drag_check_threshold (widget,
                                      view->details->drag_x,
                                      view->details->drag_y,
                                      event->x,
                                      event->y)) {
            gtk_drag_begin (widget,
                            source_target_list,
                            GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_LINK | GDK_ACTION_ASK,
                            view->details->drag_button,
                            (GdkEvent*) event);
        }

        /* The event is handled by the DnD begin, don't propagate further */
        return GDK_EVENT_STOP;
    }

    return GDK_EVENT_PROPAGATE;
}

static gboolean
query_tooltip_callback (GtkWidget *widget,
                        gint x,
                        gint y,
                        gboolean kb_mode,
                        GtkTooltip *tooltip,
                        gpointer user_data)
{
    NolphinListView *list_view;
    gboolean ret;

    ret = FALSE;

    list_view = NOLPHIN_LIST_VIEW (user_data);

    if (list_view->details->show_tooltips) {
        GtkTreeIter iter;
        NolphinFile *file;
        GtkTreePath *path = NULL;
        GtkTreeModel *model = GTK_TREE_MODEL (list_view->details->model);

        if (gtk_tree_view_get_tooltip_context (GTK_TREE_VIEW (widget), &x, &y,
                                               kb_mode,
                                               &model, &path, &iter)) {

            if (!gtk_tree_view_is_blank_at_pos (GTK_TREE_VIEW (widget), x, y, NULL, NULL, NULL, NULL)) {
                gtk_tree_model_get (GTK_TREE_MODEL (list_view->details->model),
                                    &iter,
                                    NOLPHIN_LIST_MODEL_FILE_COLUMN, &file,
                                    -1);
                if (file) {
                    gchar *tooltip_text;

                    tooltip_text = nolphin_file_construct_tooltip (file,
                                                                list_view->details->tooltip_flags,
                                                                nolphin_view_get_model (NOLPHIN_VIEW (list_view)));
                    gtk_tooltip_set_markup (tooltip, tooltip_text);
                    gtk_tree_view_set_tooltip_cell (GTK_TREE_VIEW (widget), tooltip, path, NULL, NULL);
                    g_free (tooltip_text);

                    ret = TRUE;
                }

                nolphin_file_unref (file);
            }
        }
        gtk_tree_path_free (path);
    }

    return ret;
}

static gboolean
leave_notify_callback (GtkWidget *widget,
		       GdkEventCrossing *event,
		       gpointer callback_data)
{
	NolphinListView *view;

	view = NOLPHIN_LIST_VIEW (callback_data);

	if (click_policy == NOLPHIN_CLICK_POLICY_SINGLE &&
	    view->details->hover_path != NULL) {
		gtk_tree_path_free (view->details->hover_path);
		view->details->hover_path = NULL;
	}

	return FALSE;
}

static gboolean
enter_notify_callback (GtkWidget *widget,
		       GdkEventCrossing *event,
		       gpointer callback_data)
{
	NolphinListView *view;

	view = NOLPHIN_LIST_VIEW (callback_data);

	if (click_policy == NOLPHIN_CLICK_POLICY_SINGLE) {
		if (view->details->hover_path != NULL) {
			gtk_tree_path_free (view->details->hover_path);
		}

		gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (widget),
					       event->x, event->y,
					       &view->details->hover_path,
					       NULL, NULL, NULL);

		if (view->details->hover_path != NULL) {
			gdk_window_set_cursor (gtk_widget_get_window (widget), hand_cursor);
		}
	}

	return FALSE;
}

static void
do_popup_menu (GtkWidget *widget, NolphinListView *view, GdkEventButton *event)
{
 	if (tree_view_has_selection (GTK_TREE_VIEW (widget))) {
		nolphin_view_pop_up_selection_context_menu (NOLPHIN_VIEW (view), event);
	} else {
                nolphin_view_pop_up_background_context_menu (NOLPHIN_VIEW (view), event);
	}
}

static void
row_activated_callback (GtkTreeView *treeview, GtkTreePath *path,
			GtkTreeViewColumn *column, NolphinListView *view)
{
	activate_selected_items (view);
}

static void
columns_reordered_callback (AtkObject *atk,
                            gpointer user_data)
{
    NolphinListView *view = NOLPHIN_LIST_VIEW (user_data);

    gchar **columns;
    GList *vis_columns = NULL;
    int i;
    NolphinFile *file = nolphin_view_get_directory_as_file (NOLPHIN_VIEW (view));

    columns = get_visible_columns (view);

    for (i = 0; columns[i] != NULL; ++i) {
        vis_columns = g_list_prepend (vis_columns, columns[i]);
    }

    vis_columns = g_list_reverse (vis_columns);

    GList *tv_list, *iter, *l;
    GList *list = NULL;

    tv_list = gtk_tree_view_get_columns (view->details->tree_view);

    for (iter = tv_list; iter != NULL; iter = iter->next) {
        for (l = vis_columns; l != NULL; l = l->next) {
            if (iter->data == g_hash_table_lookup (view->details->columns, l->data))
                list = g_list_prepend (list, (gchar *)l->data);
        }
    }

    list = g_list_reverse (list);

    if (nolphin_global_preferences_get_ignore_view_metadata ()) {
        nolphin_window_set_ignore_meta_column_order (nolphin_view_get_nolphin_window (NOLPHIN_VIEW (view)), list);
    } else if (nolphin_file_is_in_search (file)) {
        gchar **column_array = string_array_from_string_glist (list);

        g_settings_set_strv (nolphin_search_preferences,
                             NOLPHIN_PREFERENCES_SEARCH_VISIBLE_COLUMNS,
                             (const gchar **) column_array);
    } else {
        nolphin_file_set_metadata_list (file,
                                     NOLPHIN_METADATA_KEY_LIST_VIEW_COLUMN_ORDER,
                                     list);
    }
    g_list_free_full (list, g_free);
    g_free (columns);
    g_list_free (vis_columns);
    g_list_free (tv_list);
}

static gboolean
clicked_on_text_in_name_cell (NolphinListView *view, GtkTreePath *path, GdkEventButton *event)
{
    gboolean ret = FALSE;

    NolphinListViewDetails *details = view->details;
    int x_col_offset, x_cell_offset, width, expander_size, horizontal_separator, expansion_offset;

    x_col_offset = gtk_tree_view_column_get_x_offset (details->file_name_column);

    gtk_tree_view_column_cell_get_position (details->file_name_column,
                                            GTK_CELL_RENDERER (details->file_name_cell),
                                            &x_cell_offset, &width);

    if (g_settings_get_boolean (nolphin_list_view_preferences, NOLPHIN_PREFERENCES_LIST_VIEW_ENABLE_EXPANSION)) {
        gtk_widget_style_get (GTK_WIDGET (details->tree_view),
                                          "expander-size", &expander_size,
                                          "horizontal-separator", &horizontal_separator,
                                          NULL);

        expander_size += 4;
        expansion_offset = ((horizontal_separator / 2) + gtk_tree_path_get_depth (path) * expander_size);
    } else {
        expansion_offset = 0;
    }

    ret = (event->x > (expansion_offset + x_col_offset + x_cell_offset) &&
           event->x < (x_col_offset + x_cell_offset + width)) &&
           !gtk_tree_view_is_blank_at_pos (GTK_TREE_VIEW (view->details->tree_view),
                                                          event->x, event->y,
                                                          NULL, NULL, NULL, NULL);

    return ret;
}

static gboolean
clicked_within_double_click_interval (NolphinListView *view)
{
    static gint64 last_click_time = 0;
    static int click_count = 0;

    gint64 current_time;
    gint interval;

    /* fetch system double-click time */
    g_object_get (G_OBJECT (gtk_widget_get_settings (GTK_WIDGET (view))),
              "gtk-double-click-time", &interval,
              NULL);

    current_time = g_get_monotonic_time ();
    if (current_time - last_click_time < interval * 1000) {
        click_count++;
    } else {
        click_count = 0;
    }

    /* Stash time for next compare */
    last_click_time = current_time;

    /* Only allow double click */
    if (click_count == 1) {
        click_count = 0;
        last_click_time = 0;
        return TRUE;
    } else {
        return FALSE;
    }
}

static gboolean
clicked_within_slow_click_interval_on_text (NolphinListView *view, GtkTreePath *path, GdkEventButton *event)
{
    static gint64 last_slow_click_time = 0;
    static gint slow_click_count = 0;
    gint64 current_time;
    gint interval;
    gint double_click_interval;

    /* fetch system double-click time */
    g_object_get (G_OBJECT (gtk_widget_get_settings (GTK_WIDGET (view))),
                  "gtk-double-click-time", &double_click_interval,
                  NULL);

    /* slow click interval is always 800ms longer than the system
     * double-click interval. */

    interval = double_click_interval + 800;

    current_time = g_get_monotonic_time ();
    if (current_time - last_slow_click_time < interval * 1000) {
        slow_click_count = 1;
    } else {
        slow_click_count = 0;
    }

    /* Stash time for next compare */
    last_slow_click_time = current_time;

    GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view->details->tree_view));

    GList *selected = gtk_tree_selection_get_selected_rows (selection, NULL);
    gint selected_count = g_list_length (selected);

    g_list_free_full (selected, (GDestroyNotify) gtk_tree_path_free);

    if (selected_count != 1)
        return FALSE;

    /* Only allow second click on text to trigger this */
    if (slow_click_count == 1 && view->details->double_click_path[1] &&
        gtk_tree_path_compare (view->details->double_click_path[0], view->details->double_click_path[1]) == 0 &&
        clicked_on_text_in_name_cell (view, path, event)) {
        slow_click_count = 0;

        return TRUE;
    } else {
        return FALSE;
    }
}

static gboolean
handle_icon_double_click (NolphinListView *view, GtkTreePath *path, GdkEventButton *event, gboolean on_expander)
{
    /* Ignore double click if we are in single click mode */
    if (click_policy == NOLPHIN_CLICK_POLICY_SINGLE) {
        return FALSE;
    }

    if (event->button == GDK_BUTTON_SECONDARY) {
        return FALSE;
    }

    if (clicked_within_double_click_interval (view) &&
        view->details->double_click_path[1] &&
        gtk_tree_path_compare (view->details->double_click_path[0], view->details->double_click_path[1]) == 0 &&
        !on_expander) {
        /* NOTE: Activation can actually destroy the view if we're switching */
        if (!button_event_modifies_selection (event)) {
            if (event->button == 1) {
                activate_selected_items (view);
            } else if (event->button == 2) {
                activate_selected_items_alternate (view, NULL, TRUE);
            }

            return TRUE;
        } else if (event->button == 1 &&
               (event->state & GDK_SHIFT_MASK) != 0) {
            NolphinFile *file;
            file = nolphin_list_model_file_for_path (view->details->model, path);
            if (file != NULL) {
                activate_selected_items_alternate (view, file, TRUE);
                nolphin_file_unref (file);
            }

            return TRUE;
        }
    }

    return FALSE;
}

static gboolean
handle_icon_slow_two_click (NolphinListView *view, GtkTreePath *path, GdkEventButton *event)
{
    NolphinListViewDetails *details;
    NolphinFile *file;
    gboolean can_rename;

    details = view->details;

    if (!details->click_to_rename)
        return FALSE;

    file = nolphin_list_model_file_for_path (view->details->model, path);
    can_rename = nolphin_file_can_rename (file);
    nolphin_file_unref (file);

    if (!can_rename)
        return FALSE;

    if (clicked_within_slow_click_interval_on_text (view, path, event) && !button_event_modifies_selection (event)) {
        return TRUE;
    }

    return FALSE;
}

static gboolean
button_press_callback (GtkWidget *widget, GdkEventButton *event, gpointer callback_data)
{
	NolphinListView *view;
	GtkTreeView *tree_view;
	GtkTreePath *path;
	gboolean call_parent;
	GtkTreeSelection *selection;
	GtkWidgetClass *tree_view_class;

	int expander_size, horizontal_separator;
	gboolean on_expander;
	gboolean blank_click;

	view = NOLPHIN_LIST_VIEW (callback_data);
	tree_view = GTK_TREE_VIEW (widget);
	tree_view_class = GTK_WIDGET_GET_CLASS (tree_view);
	selection = gtk_tree_view_get_selection (tree_view);
	blank_click = FALSE;

	/* Don't handle extra mouse buttons here */
	if (event->button > 5) {
		return GDK_EVENT_PROPAGATE;
	}

    if (event->type == GDK_2BUTTON_PRESS || event->type == GDK_3BUTTON_PRESS) {
        if (g_settings_get_boolean (nolphin_preferences, NOLPHIN_PREFERENCES_CLICK_DOUBLE_PARENT_FOLDER) &&
                                    (event->button == 1)) {
            /* double left click on blank will go to parent folder */
            if (!gtk_tree_view_get_path_at_pos (tree_view, event->x, event->y,
                                                NULL, NULL, NULL, NULL)) {
                NolphinWindowSlot *slot = nolphin_view_get_nolphin_window_slot (NOLPHIN_VIEW (view));
                nolphin_window_slot_go_up (slot, 0);
            }
        }

        return GDK_EVENT_STOP;
    }

	if (event->window != gtk_tree_view_get_bin_window (tree_view)) {
		return GDK_EVENT_PROPAGATE;
	}

    if (!nolphin_view_get_active (NOLPHIN_VIEW (view)) && gtk_tree_selection_count_selected_rows (selection) > 0) {
        NolphinWindowSlot *slot = nolphin_view_get_nolphin_window_slot (NOLPHIN_VIEW (view));
        nolphin_window_slot_make_hosting_pane_active (slot);
        return GDK_EVENT_STOP;
    }

	nolphin_list_model_set_drag_view
		(NOLPHIN_LIST_MODEL (gtk_tree_view_get_model (tree_view)),
		 tree_view,
		 event->x, event->y);

	view->details->ignore_button_release = FALSE;

	call_parent = TRUE;
	if (gtk_tree_view_get_path_at_pos (tree_view, event->x, event->y,
					   &path, NULL, NULL, NULL)) {
        if (g_settings_get_boolean (nolphin_list_view_preferences,
                                      NOLPHIN_PREFERENCES_LIST_VIEW_ENABLE_EXPANSION)) {
    		gtk_widget_style_get (widget,
    				      "expander-size", &expander_size,
    				      "horizontal-separator", &horizontal_separator,
    				      NULL);
    		/* TODO we should not hardcode this extra padding. It is
    		 * EXPANDER_EXTRA_PADDING from GtkTreeView.
    		 */
    		expander_size += 4;
    		on_expander = (event->x <= horizontal_separator / 2 +
    			       gtk_tree_path_get_depth (path) * expander_size);
        } else {
            on_expander = FALSE;
        }
		/* Keep track of path of last click so double clicks only happen
		 * on the same item */
		if ((event->button == 1 || event->button == 2)  &&
		    event->type == GDK_BUTTON_PRESS) {
			if (view->details->double_click_path[1]) {
				gtk_tree_path_free (view->details->double_click_path[1]);
			}
			view->details->double_click_path[1] = view->details->double_click_path[0];
			view->details->double_click_path[0] = gtk_tree_path_copy (path);
		}

		if (handle_icon_double_click (view, path, event, on_expander)) {
			/* Double clicking does not trigger a D&D action. */
			view->details->drag_button = 0;

		} else {
            /* queue up renaming if we've clicked within the slow-click timeframe.  Don't actually
               do it, however, until there's a button release (this allows dragging to occur on
               single items, without triggering rename) */
            view->details->rename_on_release = handle_icon_slow_two_click (view, path, event);

			/* We're going to filter out some situations where
			 * we can't let the default code run because all
			 * but one row would be would be deselected. We don't
			 * want that; we want the right click menu or single
			 * click to apply to everything that's currently selected. */

			if (event->button == 3) {
				blank_click =
					(!gtk_tree_selection_path_is_selected (selection, path) &&
					 gtk_tree_view_is_blank_at_pos (tree_view, event->x, event->y, NULL, NULL, NULL, NULL));
			}

			if (event->button == 3 &&
			    (blank_click || gtk_tree_selection_path_is_selected (selection, path))) {
				call_parent = FALSE;
			}

			if ((event->button == 1 || event->button == 2) &&
			    ((event->state & GDK_CONTROL_MASK) != 0 ||
			     (event->state & GDK_SHIFT_MASK) == 0)) {
				view->details->row_selected_on_button_down = gtk_tree_selection_path_is_selected (selection, path);
				if (view->details->row_selected_on_button_down) {
					call_parent = on_expander;
					view->details->ignore_button_release = call_parent;
				} else if ((event->state & GDK_CONTROL_MASK) != 0) {
					GList *selected_rows;
					GList *l;

					call_parent = FALSE;
					if ((event->state & GDK_SHIFT_MASK) != 0) {
						GtkTreePath *cursor;
						gtk_tree_view_get_cursor (tree_view, &cursor, NULL);
						if (cursor != NULL) {
							gtk_tree_selection_select_range (selection, cursor, path);
						} else {
							gtk_tree_selection_select_path (selection, path);
						}
					} else {
						gtk_tree_selection_select_path (selection, path);
					}
					selected_rows = gtk_tree_selection_get_selected_rows (selection, NULL);

					/* This unselects everything */
					gtk_tree_view_set_cursor (tree_view, path, NULL, FALSE);

					/* So select it again */
					l = selected_rows;
					while (l != NULL) {
						GtkTreePath *p = l->data;
						l = l->next;
						gtk_tree_selection_select_path (selection, p);
						gtk_tree_path_free (p);
					}
					g_list_free (selected_rows);
				} else {
					view->details->ignore_button_release = on_expander;
				}
			}

			if (call_parent) {
				g_signal_handlers_block_by_func (tree_view,
								 row_activated_callback,
								 view);

				tree_view_class->button_press_event (widget, event);

				g_signal_handlers_unblock_by_func (tree_view,
								   row_activated_callback,
								   view);
			} else if (gtk_tree_selection_path_is_selected (selection, path)) {
				gtk_widget_grab_focus (widget);
			}

			if ((event->button == 1 || event->button == 2) &&
			    event->type == GDK_BUTTON_PRESS) {
				view->details->drag_started = FALSE;
				view->details->drag_button = event->button;
				view->details->drag_x = event->x;
				view->details->drag_y = event->y;
			}

			if (event->button == 3) {
				if (blank_click) {
					gtk_tree_selection_unselect_all (selection);
				}
				do_popup_menu (widget, view, event);
			}
		}

		gtk_tree_path_free (path);
	} else {
		if ((event->button == 1 || event->button == 2)  &&
		    event->type == GDK_BUTTON_PRESS) {
			if (view->details->double_click_path[1]) {
				gtk_tree_path_free (view->details->double_click_path[1]);
			}
			view->details->double_click_path[1] = view->details->double_click_path[0];
			view->details->double_click_path[0] = NULL;
		}
		/* Deselect if people click outside any row. It's OK to
		   let default code run; it won't reselect anything. */
		gtk_tree_selection_unselect_all (gtk_tree_view_get_selection (tree_view));
		tree_view_class->button_press_event (widget, event);

		if (event->button == 3) {
			do_popup_menu (widget, view, event);
		}
	}

	/* We chained to the default handler in this method, so never
	 * let the default handler run */
	return GDK_EVENT_STOP;
}

static gboolean
button_release_callback (GtkWidget *widget,
			 GdkEventButton *event,
			 gpointer callback_data)
{
	NolphinListView *view;

	view = NOLPHIN_LIST_VIEW (callback_data);

    view->details->rubber_banding = FALSE;

	if (event->button == view->details->drag_button) {
		stop_drag_check (view);
		if (!view->details->drag_started &&
		    !view->details->ignore_button_release) {
			nolphin_list_view_did_not_drag (view, event);
		}
	}
	return FALSE;
}

static gboolean
popup_menu_callback (GtkWidget *widget, gpointer callback_data)
{
 	NolphinListView *view;

	view = NOLPHIN_LIST_VIEW (callback_data);

	do_popup_menu (widget, view, NULL);

	return TRUE;
}

static void
subdirectory_done_loading_callback (NolphinDirectory *directory, NolphinListView *view)
{
	nolphin_list_model_subdirectory_done_loading (view->details->model, directory);

    queue_update_visible_icons (view, INITIAL_UPDATE_VISIBLE_DELAY);
}

static void
row_expanded_callback (GtkTreeView *treeview, GtkTreeIter *iter, GtkTreePath *path, gpointer callback_data)
{
 	NolphinListView *view;
 	NolphinDirectory *directory;

	view = NOLPHIN_LIST_VIEW (callback_data);

	if (nolphin_list_model_load_subdirectory (view->details->model, path, &directory)) {
		char *uri;

		uri = nolphin_directory_get_uri (directory);
		DEBUG ("Row expaded callback for uri %s", uri);
		g_free (uri);

		nolphin_view_add_subdirectory (NOLPHIN_VIEW (view), directory);
        nolphin_list_model_set_expanding (view->details->model, directory);

		if (nolphin_directory_are_all_files_seen (directory)) {
			nolphin_list_model_subdirectory_done_loading (view->details->model,
								 directory);
		} else {
			g_signal_connect_object (directory, "done_loading",
						 G_CALLBACK (subdirectory_done_loading_callback),
						 view, 0);
		}

		nolphin_directory_unref (directory);
	}
}

struct UnloadDelayData {
	NolphinFile *file;
	NolphinDirectory *directory;
	NolphinListView *view;
};

static gboolean
unload_file_timeout (gpointer data)
{
	struct UnloadDelayData *unload_data = data;
	GtkTreeIter iter;
	NolphinListModel *model;
	GtkTreePath *path;

	if (unload_data->view != NULL) {
		model = unload_data->view->details->model;
		if (nolphin_list_model_get_tree_iter_from_file (model,
							   unload_data->file,
							   unload_data->directory,
							   &iter)) {
			path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &iter);
			if (!gtk_tree_view_row_expanded (unload_data->view->details->tree_view,
							 path)) {
				nolphin_list_model_unload_subdirectory (model, &iter);
			}
			gtk_tree_path_free (path);
		}

		g_object_remove_weak_pointer (G_OBJECT (unload_data->view),
					      (gpointer *) &unload_data->view);
	}

	if (unload_data->directory) {
		nolphin_directory_unref (unload_data->directory);
	}
	nolphin_file_unref (unload_data->file);
	g_free (unload_data);
	return FALSE;
}

static void
row_collapsed_callback (GtkTreeView *treeview, GtkTreeIter *iter, GtkTreePath *path, gpointer callback_data)
{
 	NolphinListView *view;
 	NolphinFile *file;
	NolphinDirectory *directory;
	GtkTreeIter parent;
	struct UnloadDelayData *unload_data;
	GtkTreeModel *model;
	char *uri;

	view = NOLPHIN_LIST_VIEW (callback_data);
	model = GTK_TREE_MODEL (view->details->model);

	gtk_tree_model_get (model, iter,
			    NOLPHIN_LIST_MODEL_FILE_COLUMN, &file,
			    -1);

	directory = NULL;
	if (gtk_tree_model_iter_parent (model, &parent, iter)) {
		gtk_tree_model_get (model, &parent,
				    NOLPHIN_LIST_MODEL_SUBDIRECTORY_COLUMN, &directory,
				    -1);
	}


	uri = nolphin_file_get_uri (file);
	DEBUG ("Row collapsed callback for uri %s", uri);
	g_free (uri);

	unload_data = g_new (struct UnloadDelayData, 1);
	unload_data->view = view;
	unload_data->file = file;
	unload_data->directory = directory;

	g_object_add_weak_pointer (G_OBJECT (unload_data->view),
				   (gpointer *) &unload_data->view);

	g_timeout_add_seconds (COLLAPSE_TO_UNLOAD_DELAY,
			       unload_file_timeout,
			       unload_data);
}

static void
subdirectory_unloaded_callback (NolphinListModel *model,
				NolphinDirectory *directory,
				gpointer callback_data)
{
	NolphinListView *view;

	g_return_if_fail (NOLPHIN_IS_LIST_MODEL (model));
	g_return_if_fail (NOLPHIN_IS_DIRECTORY (directory));

	view = NOLPHIN_LIST_VIEW(callback_data);

	g_signal_handlers_disconnect_by_func (directory,
					      G_CALLBACK (subdirectory_done_loading_callback),
					      view);
	nolphin_view_remove_subdirectory (NOLPHIN_VIEW (view), directory);
}

static gboolean
key_press_callback (GtkWidget *widget, GdkEventKey *event, gpointer callback_data)
{
	NolphinView *view;
	GdkEventButton button_event = { 0 };
	gboolean handled;
	GtkTreeView *tree_view;
	GtkTreePath *path;

	tree_view = GTK_TREE_VIEW (widget);

	view = NOLPHIN_VIEW (callback_data);
	handled = FALSE;

    if (event->keyval == GDK_KEY_slash ||
        event->keyval == GDK_KEY_KP_Divide ||
        event->keyval == GDK_KEY_asciitilde) {
        if (gtk_bindings_activate_event (G_OBJECT (nolphin_view_get_nolphin_window (view)), event)) {
            return GDK_EVENT_STOP;
        }
    }

	switch (event->keyval) {
	case GDK_KEY_F10:
		if (event->state & GDK_CONTROL_MASK) {
			nolphin_view_pop_up_background_context_menu (view, &button_event);
			handled = TRUE;
		}
		break;
	case GDK_KEY_Right:
        if (!g_settings_get_boolean (nolphin_list_view_preferences, NOLPHIN_PREFERENCES_LIST_VIEW_ENABLE_EXPANSION))
            break;

		gtk_tree_view_get_cursor (tree_view, &path, NULL);
		if (path) {
			gtk_tree_view_expand_row (tree_view, path, FALSE);
			gtk_tree_path_free (path);
		}
		handled = TRUE;
		break;
	case GDK_KEY_Left:
        if (!g_settings_get_boolean (nolphin_list_view_preferences, NOLPHIN_PREFERENCES_LIST_VIEW_ENABLE_EXPANSION))
            break;

		gtk_tree_view_get_cursor (tree_view, &path, NULL);
		if (path) {
			if (!gtk_tree_view_collapse_row (tree_view, path)) {
				/* if the row is already collapsed or doesn't have any children,
				 * jump to the parent row instead.
				 */
				if ((gtk_tree_path_get_depth (path) > 1) && gtk_tree_path_up (path)) {
					gtk_tree_view_set_cursor (tree_view, path, NULL, FALSE);
				}
			}

			gtk_tree_path_free (path);
		}
		handled = TRUE;
		break;
	case GDK_KEY_space:
		if (nolphin_view_activate_filter (view, event)) {
			handled = TRUE;
			break;
		}
		if (event->state & GDK_CONTROL_MASK) {
			handled = FALSE;
			break;
		}
		if (!gtk_widget_has_focus (GTK_WIDGET (NOLPHIN_LIST_VIEW (view)->details->tree_view))) {
			handled = FALSE;
			break;
		}
		if ((event->state & GDK_SHIFT_MASK) != 0) {
			activate_selected_items_alternate (NOLPHIN_LIST_VIEW (view), NULL, TRUE);
		} else {
			preview_selected_items (NOLPHIN_LIST_VIEW (view));
		}
		handled = TRUE;
		break;
	case GDK_KEY_Return:
	case GDK_KEY_KP_Enter:
		if ((event->state & GDK_SHIFT_MASK) != 0) {
			activate_selected_items_alternate (NOLPHIN_LIST_VIEW (view), NULL, TRUE);
		} else {
			activate_selected_items (NOLPHIN_LIST_VIEW (view));
		}
		handled = TRUE;
		break;
	case GDK_KEY_v:
		/* Eat Control + v to not enable type ahead */
		if ((event->state & GDK_CONTROL_MASK) != 0) {
			handled = TRUE;
		}
		break;

	default:
		handled = FALSE;
	}

	if (!handled) {
		handled = nolphin_view_activate_filter (view, event);
	}

	return handled;
}

static void
set_ok_to_load_deferred_attrs (NolphinListView  *list_view,
                       gboolean       ok)
{
    list_view->details->ok_to_load_deferred_attrs = ok;

    if (ok) {
        queue_update_visible_icons (list_view, INITIAL_UPDATE_VISIBLE_DELAY);
    }
}

static void
nolphin_list_view_reveal_selection (NolphinView *view)
{
	GList *selection;

	g_return_if_fail (NOLPHIN_IS_LIST_VIEW (view));

        selection = nolphin_view_get_selection (view);

	/* Make sure at least one of the selected items is scrolled into view */
	if (selection != NULL) {
		NolphinListView *list_view;
		NolphinFile *file;
		GtkTreeIter iter;
		GtkTreePath *path;

		list_view = NOLPHIN_LIST_VIEW (view);
		file = selection->data;
		if (nolphin_list_model_get_first_iter_for_file (list_view->details->model, file, &iter)) {
			path = gtk_tree_model_get_path (GTK_TREE_MODEL (list_view->details->model), &iter);

			gtk_tree_view_scroll_to_cell (list_view->details->tree_view, path, NULL, FALSE, 0.0, 0.0);

			gtk_tree_path_free (path);
		}
	}

        nolphin_file_list_free (selection);
}

static gboolean
sort_criterion_changes_due_to_user (GtkTreeView *tree_view)
{
	GList *columns, *p;
	GtkTreeViewColumn *column;
	GSignalInvocationHint *ihint;
	gboolean ret;

	ret = FALSE;

	columns = gtk_tree_view_get_columns (tree_view);
	for (p = columns; p != NULL; p = p->next) {
		column = p->data;
		ihint = g_signal_get_invocation_hint (column);
		if (ihint != NULL) {
			ret = TRUE;
			break;
		}
	}
	g_list_free (columns);

	return ret;
}

static void
sort_column_changed_callback (GtkTreeSortable *sortable,
			      NolphinListView *view)
{
	NolphinFile *file;
	gint sort_column_id, default_sort_column_id;
	GtkSortType reversed;
	GQuark sort_attr, default_sort_attr;
	char *reversed_attr, *default_reversed_attr;
	gboolean default_sort_reversed;

	file = nolphin_view_get_directory_as_file (NOLPHIN_VIEW (view));

	gtk_tree_sortable_get_sort_column_id (sortable, &sort_column_id, &reversed);
	sort_attr = nolphin_list_model_get_attribute_from_sort_column_id (view->details->model, sort_column_id);

	default_sort_column_id = nolphin_list_model_get_sort_column_id_from_attribute (view->details->model,
										  g_quark_from_string (get_default_sort_order (file, &default_sort_reversed)));
	default_sort_attr = nolphin_list_model_get_attribute_from_sort_column_id (view->details->model, default_sort_column_id);

        if (nolphin_global_preferences_get_ignore_view_metadata ())
                nolphin_window_set_ignore_meta_sort_column (nolphin_view_get_nolphin_window (NOLPHIN_VIEW (view)),
                                                         g_quark_to_string (sort_attr));
        else if (nolphin_file_is_in_search (file)) {
            g_settings_set_string (nolphin_search_preferences, NOLPHIN_PREFERENCES_SEARCH_SORT_COLUMN, g_quark_to_string (sort_attr));
        } else {
            nolphin_file_set_metadata (file, NOLPHIN_METADATA_KEY_LIST_VIEW_SORT_COLUMN,
                                    g_quark_to_string (default_sort_attr), g_quark_to_string (sort_attr));
        }

	default_reversed_attr = (default_sort_reversed ? (char *)"true" : (char *)"false");

	if (view->details->last_sort_attr != sort_attr &&
	    sort_criterion_changes_due_to_user (view->details->tree_view)) {
		/* at this point, the sort order is always GTK_SORT_ASCENDING, if the sort column ID
		 * switched. Invert the sort order, if it's the default criterion with a reversed preference,
		 * or if it makes sense for the attribute (i.e. date). */
		if (sort_attr == default_sort_attr) {
			/* use value from preferences */
			reversed = g_settings_get_boolean (nolphin_preferences,
							   NOLPHIN_PREFERENCES_DEFAULT_SORT_IN_REVERSE_ORDER);
		} else {
			reversed = nolphin_file_is_date_sort_attribute_q (sort_attr);
		}

		if (reversed) {
			g_signal_handlers_block_by_func (sortable, sort_column_changed_callback, view);
			gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (view->details->model),
							      sort_column_id,
							      GTK_SORT_DESCENDING);
			g_signal_handlers_unblock_by_func (sortable, sort_column_changed_callback, view);
		}
	}

    if (nolphin_global_preferences_get_ignore_view_metadata ()) {
        nolphin_window_set_ignore_meta_sort_direction (nolphin_view_get_nolphin_window (NOLPHIN_VIEW (view)),
                                                    reversed ? SORT_DESCENDING : SORT_ASCENDING);
    } else if (nolphin_file_is_in_search (file)) {
        g_settings_set_boolean (nolphin_search_preferences, NOLPHIN_PREFERENCES_SEARCH_REVERSE_SORT, reversed);
    } else {
        reversed_attr = (reversed ? (char *)"true" : (char *)"false");
        nolphin_file_set_metadata (file, NOLPHIN_METADATA_KEY_LIST_VIEW_SORT_REVERSED,
                                default_reversed_attr, reversed_attr);
    }

	/* Make sure selected item(s) is visible after sort */
	nolphin_list_view_reveal_selection (NOLPHIN_VIEW (view));

	view->details->last_sort_attr = sort_attr;
}

static void
cell_renderer_editing_started_cb (GtkCellRenderer *renderer,
				  GtkCellEditable *editable,
				  const gchar *path_str,
				  NolphinListView *list_view)
{
	GtkEntry *entry;

	entry = GTK_ENTRY (editable);
	list_view->details->editable_widget = editable;

	/* Free a previously allocated original_name */
	g_free (list_view->details->original_name);

	list_view->details->original_name = g_strdup (gtk_entry_get_text (entry));

    g_signal_handlers_block_matched (entry,
                                     G_SIGNAL_MATCH_ID | G_SIGNAL_MATCH_DATA,
                                     g_signal_lookup ("focus-out-event", GTK_TYPE_WIDGET),
                                     0, NULL, NULL,
                                     renderer);

	nolphin_clipboard_set_up_editable
		(GTK_EDITABLE (entry),
		 nolphin_view_get_ui_manager (NOLPHIN_VIEW (list_view)),
		 FALSE);
}

static void
cell_renderer_editing_canceled (GtkCellRendererText *cell,
				NolphinListView    *view)
{
    view->details->editable_widget = NULL;
	nolphin_view_set_is_renaming (NOLPHIN_VIEW (view), FALSE);
	nolphin_view_unfreeze_updates (NOLPHIN_VIEW (view));
}

static void
cell_renderer_edited (GtkCellRendererText *cell,
		      const char          *path_str,
		      const char          *new_text,
		      NolphinListView    *view)
{
	GtkTreePath *path;
	NolphinFile *file;
	GtkTreeIter iter;

	view->details->editable_widget = NULL;
	nolphin_view_set_is_renaming (NOLPHIN_VIEW (view), FALSE);

	/* Don't allow a rename with an empty string. Revert to original
	 * without notifying the user.
	 */
	if (new_text[0] == '\0') {
		g_object_set (G_OBJECT (view->details->file_name_cell),
			      "editable", FALSE,
			      NULL);
		nolphin_view_unfreeze_updates (NOLPHIN_VIEW (view));
		return;
	}

	path = gtk_tree_path_new_from_string (path_str);

	gtk_tree_model_get_iter (GTK_TREE_MODEL (view->details->model),
				 &iter, path);

	gtk_tree_path_free (path);

	gtk_tree_model_get (GTK_TREE_MODEL (view->details->model),
			    &iter,
			    NOLPHIN_LIST_MODEL_FILE_COLUMN, &file,
			    -1);

	/* Only rename if name actually changed */
	if (strcmp (new_text, view->details->original_name) != 0) {
		view->details->renaming_file = nolphin_file_ref (file);
		view->details->rename_done = FALSE;
		nolphin_rename_file (file, new_text, nolphin_list_view_rename_callback, g_object_ref (view));
		g_free (view->details->original_name);
		view->details->original_name = g_strdup (new_text);
	}

	nolphin_file_unref (file);

	/*We're done editing - make the filename-cells readonly again.*/
	g_object_set (G_OBJECT (view->details->file_name_cell),
		      "editable", FALSE,
		      NULL);

	nolphin_view_unfreeze_updates (NOLPHIN_VIEW (view));
}

static char *
get_root_uri_callback (NolphinTreeViewDragDest *dest,
		       gpointer user_data)
{
	NolphinListView *view;

	view = NOLPHIN_LIST_VIEW (user_data);

	return nolphin_view_get_uri (NOLPHIN_VIEW (view));
}

// this is confusing... so rename them.
#define ALLOW_EXPAND FALSE
#define PREVENT_EXPAND TRUE

static gboolean
test_expand_row_callback (GtkTreeView *treeview,
                          GtkTreeIter *iter,
                          GtkTreePath *path,
                          gpointer     user_data)
{
    NolphinListView *view = NOLPHIN_LIST_VIEW (user_data);

    if (!view->details->drag_started) {
        return ALLOW_EXPAND;
    }

    if (eel_gtk_get_treeview_row_text_is_under_pointer (view->details->tree_view)) {
        return ALLOW_EXPAND;
    }

    return PREVENT_EXPAND;
}

static NolphinFile *
get_file_for_path_callback (NolphinTreeViewDragDest *dest,
			    GtkTreePath *path,
			    gpointer user_data)
{
    NolphinListView *view;

    view = NOLPHIN_LIST_VIEW (user_data);

    if (!eel_gtk_get_treeview_row_text_is_under_pointer (view->details->tree_view)) {
        return NULL;
    }

    return nolphin_list_model_file_for_path (view->details->model, path);
}

/* Handles an URL received from Mozilla */
static void
list_view_handle_netscape_url (NolphinTreeViewDragDest *dest, const char *encoded_url,
			       const char *target_uri, GdkDragAction action, int x, int y, NolphinListView *view)
{
	nolphin_view_handle_netscape_url_drop (NOLPHIN_VIEW (view),
						encoded_url, target_uri, action, x, y);
}

static void
list_view_handle_uri_list (NolphinTreeViewDragDest *dest, const char *item_uris,
			   const char *target_uri,
			   GdkDragAction action, int x, int y, NolphinListView *view)
{
	nolphin_view_handle_uri_list_drop (NOLPHIN_VIEW (view),
					    item_uris, target_uri, action, x, y);
}

static void
list_view_handle_text (NolphinTreeViewDragDest *dest, const char *text,
		       const char *target_uri,
		       GdkDragAction action, int x, int y, NolphinListView *view)
{
	nolphin_view_handle_text_drop (NOLPHIN_VIEW (view),
					text, target_uri, action, x, y);
}

static void
list_view_handle_raw (NolphinTreeViewDragDest *dest, const char *raw_data,
		      int length, const char *target_uri, const char *direct_save_uri,
		      GdkDragAction action, int x, int y, NolphinListView *view)
{
	nolphin_view_handle_raw_drop (NOLPHIN_VIEW (view),
				       raw_data, length, target_uri, direct_save_uri,
				       action, x, y);
}

static void
move_copy_items_callback (NolphinTreeViewDragDest *dest,
			  const GList *item_uris,
			  const char *target_uri,
			  guint action,
			  int x,
			  int y,
			  gpointer user_data)

{
	NolphinView *view = user_data;

	nolphin_clipboard_clear_if_colliding_uris (GTK_WIDGET (view),
						    item_uris,
						    nolphin_view_get_copied_files_atom (view));
	nolphin_view_move_copy_items (view,
				       item_uris,
				       NULL,
				       target_uri,
				       action,
				       x, y);
}

static void
column_header_menu_toggled (GtkCheckMenuItem *menu_item,
                            NolphinListView *list_view)
{
	NolphinFile *file;
    char **visible_columns;
    const char *menu_item_column_id;
	GList *list = NULL;
    GList *l, *current_view_columns;
	int i;

    file = nolphin_view_get_directory_as_file (NOLPHIN_VIEW (list_view));
    menu_item_column_id = g_object_get_data (G_OBJECT (menu_item), "column-name");

    current_view_columns = gtk_tree_view_get_columns (list_view->details->tree_view);

    for (l = current_view_columns; l != NULL; l = l->next) {
        GtkTreeViewColumn *c = GTK_TREE_VIEW_COLUMN (l->data);

        const char *current_id = g_object_get_data (G_OBJECT (c), "column-id");

        if (g_strcmp0 (current_id, menu_item_column_id) == 0) {
            if (gtk_check_menu_item_get_active (menu_item)) {
                list = g_list_prepend (list, g_strdup (current_id));
            }
        } else {
            if (gtk_tree_view_column_get_visible (c))
                list = g_list_prepend (list, g_strdup (current_id));
        }
    }

    list = g_list_reverse (list);

    if (nolphin_global_preferences_get_ignore_view_metadata ()) {
        nolphin_window_set_ignore_meta_visible_columns (nolphin_view_get_nolphin_window (NOLPHIN_VIEW (list_view)), list);
    } else if (nolphin_file_is_in_search (file)) {
        gchar **column_array = string_array_from_string_glist (list);

        g_settings_set_strv (nolphin_search_preferences,
                             NOLPHIN_PREFERENCES_SEARCH_VISIBLE_COLUMNS,
                             (const gchar **) column_array);

        g_strfreev (column_array);
    } else
        nolphin_file_set_metadata_list (file,
                                     NOLPHIN_METADATA_KEY_LIST_VIEW_VISIBLE_COLUMNS,
                                     list);

    visible_columns = g_new0 (char *, g_list_length (list) + 1);
    for (i = 0, l = list; l != NULL; ++i, l = l->next) {
		visible_columns[i] = l->data;
    }

	/* set view values ourselves, as new metadata could not have been
	 * updated yet.
	 */
    apply_columns_settings (list_view, visible_columns, visible_columns);

    g_list_free (list);
    g_list_free (current_view_columns);
    g_strfreev (visible_columns);
}

static void
column_header_menu_use_default (GtkMenuItem *menu_item,
                                NolphinListView *list_view)
{
	NolphinFile *file;
	char **default_columns;
	char **default_order;

	file = nolphin_view_get_directory_as_file (NOLPHIN_VIEW (list_view));

    g_signal_handlers_block_by_func (list_view->details->tree_view,
                                     columns_reordered_callback,
                                     list_view);

    if (nolphin_global_preferences_get_ignore_view_metadata ()) {
        NolphinWindow *window = nolphin_view_get_nolphin_window (NOLPHIN_VIEW (list_view));
        nolphin_window_set_ignore_meta_visible_columns (window, NULL);
        nolphin_window_set_ignore_meta_column_order (window, NULL);
    } else {
        nolphin_file_set_metadata_list (file, NOLPHIN_METADATA_KEY_LIST_VIEW_COLUMN_ORDER, NULL);
        nolphin_file_set_metadata_list (file, NOLPHIN_METADATA_KEY_LIST_VIEW_VISIBLE_COLUMNS, NULL);
    }

    if (nolphin_file_is_in_search (file)) {
        g_settings_reset (nolphin_search_preferences, NOLPHIN_PREFERENCES_SEARCH_VISIBLE_COLUMNS);
    }

    default_columns = get_default_visible_columns (list_view);

    default_order = get_default_column_order (list_view);

	/* set view values ourselves, as new metadata could not have been
	 * updated yet.
	 */
	apply_columns_settings (list_view, default_order, default_columns);

    g_signal_handlers_unblock_by_func (list_view->details->tree_view,
                                       columns_reordered_callback,
                                       list_view);

	g_strfreev (default_columns);
	g_strfreev (default_order);
}

static void
column_header_menu_disable_sort (GtkMenuItem *menu_item,
                                 NolphinListView *list_view)
{
    gboolean active = gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (menu_item));

    nolphin_list_model_set_temporarily_disable_sort (list_view->details->model, active);
}

static gboolean
column_header_clicked (GtkWidget *column_button,
                       GdkEventButton *event,
                       NolphinListView *list_view)
{
    GList *valid_columns, *l;
    gchar **visible_columns;
    NolphinFile *file;
    GtkWidget *menu;
    GtkWidget *menu_item;

    if (event->button != GDK_BUTTON_SECONDARY) {
        return FALSE;
    }

    file = nolphin_view_get_directory_as_file (NOLPHIN_VIEW (list_view));

    menu = gtk_menu_new ();

    visible_columns = get_visible_columns (list_view);
    valid_columns = nolphin_get_columns_for_file (file);

    for (l = valid_columns; l != NULL; l = l->next) {
        gchar *name;
        gchar *label;

        g_object_get (G_OBJECT (l->data),
                      "name", &name,
                      "label", &label,
                      NULL);

        menu_item = gtk_check_menu_item_new_with_label (label);
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);

        g_object_set_data_full (G_OBJECT (menu_item),
                                "column-name", g_strdup (name), g_free);

        gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menu_item),
                                        g_strv_contains ((const gchar * const *) visible_columns, name));

        /* Don't allow hiding the filename */
        if (g_strcmp0 (name, "name") == 0) {
            gtk_widget_set_sensitive (GTK_WIDGET (menu_item), FALSE);
        }

        g_signal_connect (menu_item,
                          "toggled",
                          G_CALLBACK (column_header_menu_toggled),
                          list_view);

        g_free (name);
        g_free (label);
    }

    g_strfreev (visible_columns);
    nolphin_column_list_free (valid_columns);

	menu_item = gtk_separator_menu_item_new ();
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);

	menu_item = gtk_menu_item_new_with_label (_("Vorgabe verwenden"));
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);

	g_signal_connect (menu_item,
	                  "activate",
	                  G_CALLBACK (column_header_menu_use_default),
	                  list_view);

    menu_item = gtk_separator_menu_item_new ();
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);

    menu_item = gtk_check_menu_item_new_with_label (_("Automatische Sortierung vorübergehend deaktivieren"));
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);

    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menu_item),
                                    nolphin_list_model_get_temporarily_disable_sort (list_view->details->model));

    g_signal_connect (menu_item,
                      "activate",
                      G_CALLBACK (column_header_menu_disable_sort),
                      list_view);

    gtk_widget_show_all (menu);
    eel_pop_up_context_menu (GTK_MENU (menu), (GdkEvent *) event, column_button);

    return TRUE;
}

static void
apply_columns_settings (NolphinListView *list_view,
			char **column_order,
			char **visible_columns)
{
	GList *all_columns;
	NolphinFile *file;
	GList *old_view_columns, *view_columns;
	GHashTable *visible_columns_hash;
	GList *l;
    gint i;

	file = nolphin_view_get_directory_as_file (NOLPHIN_VIEW (list_view));

	/* prepare ordered list of view columns using column_order and visible_columns */
	view_columns = NULL;

	all_columns = nolphin_get_columns_for_file (file);
	all_columns = nolphin_sort_columns (all_columns, column_order);

	/* hash table to lookup if a given column should be visible */
	visible_columns_hash = g_hash_table_new_full (g_str_hash,
						      g_str_equal,
						      (GDestroyNotify) g_free,
						      (GDestroyNotify) g_free);
	for (i = 0; visible_columns[i] != NULL; ++i) {
		g_hash_table_insert (visible_columns_hash,
				     g_ascii_strdown (visible_columns[i], -1),
				     g_ascii_strdown (visible_columns[i], -1));
	}

	for (l = all_columns; l != NULL; l = l->next) {
		char *name;
		char *lowercase;

		g_object_get (G_OBJECT (l->data), "name", &name, NULL);
		lowercase = g_ascii_strdown (name, -1);

		if (g_hash_table_lookup (visible_columns_hash, lowercase) != NULL) {
			GtkTreeViewColumn *view_column;

			view_column = g_hash_table_lookup (list_view->details->columns, name);
			if (view_column != NULL) {
				view_columns = g_list_prepend (view_columns, view_column);
			}
		}

		g_free (name);
		g_free (lowercase);
	}

	g_hash_table_destroy (visible_columns_hash);
	nolphin_column_list_free (all_columns);

	view_columns = g_list_reverse (view_columns);

	/* hide columns that are not present in the configuration */
	old_view_columns = gtk_tree_view_get_columns (list_view->details->tree_view);
	for (l = old_view_columns; l != NULL; l = l->next) {
		if (g_list_find (view_columns, l->data) == NULL) {
			gtk_tree_view_column_set_visible (l->data, FALSE);
		}
	}
	g_list_free (old_view_columns);

    /* see bug: https://github.com/GNOME/gtk/commit/497e877755f1fa1
     * Explanation for branching - move_column_after generates useless logfile spam,
     * and to avoid it, simply removing and adding columns in a different order works
     * just as well.  The problem is, gtk versions < 3.22.25 lack the patch referenced
     * in the above bug report.  An additional problem is that different pre-3.22.25
     * versions behave differently depending on other code changes in GtkTreeViewColumn.
     * Mint 18 (gtk 3.18.9) using the add/remove column method would make a new button
     * widget upon reparenting, losing existing signal handlers.  In 3.22.11, however,
     * (debian stretch, LMDE3,) we get a nice segfault.
     *
     * This may seem a long way to go for a clean log, but the warnings can accumulate
     * quickly...
     */

    if (gtk_check_version (3, 22, 25) == NULL) {
        gint prev_view_column;

        prev_view_column = 0;
        for (l = view_columns; l != NULL; l = l->next) {
            g_signal_handlers_disconnect_by_func (gtk_tree_view_column_get_button (l->data),
                                                  column_header_clicked, list_view);

            gtk_tree_view_remove_column (list_view->details->tree_view, g_object_ref (l->data));
            gtk_tree_view_insert_column (list_view->details->tree_view, l->data, prev_view_column ++);

            g_signal_connect (gtk_tree_view_column_get_button (l->data),
                              "button-press-event",
                              G_CALLBACK (column_header_clicked),
                              list_view);

            gtk_tree_view_column_set_visible (l->data, TRUE);
            g_object_unref (l->data);
        }
    } else {
        GtkTreeViewColumn *prev_view_column;

        /* show new columns from the configuration */
        for (l = view_columns; l != NULL; l = l->next) {
            gtk_tree_view_column_set_visible (l->data, TRUE);
        }

        /* place columns in the correct order */
        prev_view_column = NULL;
        for (l = view_columns; l != NULL; l = l->next) {
            gtk_tree_view_move_column_after (list_view->details->tree_view, l->data, prev_view_column);
            prev_view_column = l->data;
        }
    }

    g_list_free (view_columns);
}

static void
filename_cell_data_func (GtkTreeViewColumn *column,
			 GtkCellRenderer   *renderer,
			 GtkTreeModel      *model,
			 GtkTreeIter       *iter,
			 NolphinListView        *view)
{
	char *text;
	GtkTreePath *path;
	PangoUnderline underline;
    gint weight;

	gtk_tree_model_get (model, iter,
			    view->details->file_name_column_num, &text,
                NOLPHIN_LIST_MODEL_TEXT_WEIGHT_COLUMN, &weight,
			    -1);

	if (click_policy == NOLPHIN_CLICK_POLICY_SINGLE) {
		path = gtk_tree_model_get_path (model, iter);

		if (view->details->hover_path == NULL ||
		    gtk_tree_path_compare (path, view->details->hover_path)) {
			underline = PANGO_UNDERLINE_NONE;
		} else {
			underline = PANGO_UNDERLINE_SINGLE;
		}

		gtk_tree_path_free (path);
	} else {
		underline = PANGO_UNDERLINE_NONE;
	}

	if (text != NULL && nolphin_list_model_get_filter_active (view->details->model)) {
		const char *filter_text = nolphin_view_get_filter_text (NOLPHIN_VIEW (view));
		PangoAttrList *match_attrs = nolphin_fzy_match_attrs (filter_text, text);

		if (match_attrs != NULL) {
			PangoAttrList *attrs = pango_attr_list_new ();
			PangoAttribute *base = pango_attr_weight_new (weight);
			PangoAttrIterator *iter;

			pango_attr_list_insert (attrs, base);

			iter = pango_attr_list_get_iterator (match_attrs);
			do {
				PangoAttribute *a = pango_attr_iterator_get (iter, PANGO_ATTR_WEIGHT);
				if (a != NULL) {
					pango_attr_list_change (attrs, pango_attribute_copy (a));
				}
			} while (pango_attr_iterator_next (iter));
			pango_attr_iterator_destroy (iter);
			pango_attr_list_unref (match_attrs);

			g_object_set (G_OBJECT (renderer),
			              "text", text,
			              "underline", underline,
			              "weight-set", FALSE,
			              "attributes", attrs,
			              NULL);
			pango_attr_list_unref (attrs);
		} else {
			g_object_set (G_OBJECT (renderer),
			              "text", text,
			              "underline", underline,
			              "weight", weight,
			              "attributes", NULL,
			              NULL);
		}
	} else {
		g_object_set (G_OBJECT (renderer),
		              "text", text,
		              "underline", underline,
		              "weight", weight,
		              "attributes", NULL,
		              NULL);
	}

    g_free (text);
}

static gboolean
focus_in_event_callback (GtkWidget *widget, GdkEventFocus *event, gpointer user_data)
{
	NolphinWindowSlot *slot;
	NolphinListView *list_view = NOLPHIN_LIST_VIEW (user_data);

	/* make the corresponding slot (and the pane that contains it) active */
	slot = nolphin_view_get_nolphin_window_slot (NOLPHIN_VIEW (list_view));
	nolphin_window_slot_make_hosting_pane_active (slot);

	return FALSE;
}

static void
prioritize_visible_files (NolphinListView *view)
{
    NolphinFile *last_file;
    // GList *queue_list, *l;
    GdkRectangle vrect;
    GtkTreeIter iter;
    GtkTreePath *path;
    gint icon_size, cy, start_y, end_y, stepdown;
    gint bin_y;

    gtk_tree_view_get_visible_rect (view->details->tree_view,
                                    &vrect);
    icon_size = nolphin_get_list_icon_size_for_zoom_level (nolphin_list_view_get_zoom_level (NOLPHIN_VIEW (view)));

    gtk_tree_view_convert_tree_to_bin_window_coords(view->details->tree_view,
                                                    1, vrect.y,
                                                    NULL, &bin_y);

    stepdown = icon_size * .75;

    start_y = bin_y - (vrect.height / 2);
    end_y = bin_y + vrect.height + (vrect.height / 2);

    last_file = NULL;
    cy = end_y;

    // Images that start out un-thumbnailed end up resolving in reverse
    // order, so work bottom-up here.
    while (cy > start_y) {
        if (gtk_tree_view_get_path_at_pos (view->details->tree_view,
                                           1, cy,
                                           &path, NULL, NULL, NULL)) {
            NolphinFile *file;
            gboolean shown;

            gtk_tree_model_get_iter (GTK_TREE_MODEL (view->details->model),
                                     &iter, path);

            gtk_tree_path_free (path);
            gtk_tree_model_get (GTK_TREE_MODEL (view->details->model),
                                &iter,
                                NOLPHIN_LIST_MODEL_ICON_SHOWN, &shown,
                                NOLPHIN_LIST_MODEL_FILE_COLUMN, &file, -1);

            /* We'll catch some files twice, so filter them out */
            if (file != NULL && file != last_file) {
                last_file = file;

                if (nolphin_file_get_load_deferred_attrs (file) == NOLPHIN_FILE_LOAD_DEFERRED_ATTRS_NO) {
                    nolphin_file_set_load_deferred_attrs (file, NOLPHIN_FILE_LOAD_DEFERRED_ATTRS_YES);
                }

                if (nolphin_file_is_thumbnailing (file)) {
                    gchar *uri = nolphin_file_get_uri (file);

                    nolphin_thumbnail_prioritize (uri);
                    g_free (uri);
                } else {
                    nolphin_file_invalidate_attributes (file, NOLPHIN_FILE_DEFERRED_ATTRIBUTES);
                }
            }

            nolphin_file_unref (file);
        }

        cy -= stepdown;
    }
}

static gboolean
update_visible_icons_cb (NolphinListView *view)
{
    prioritize_visible_files (view);

    view->details->update_visible_icons_id = 0;
    return G_SOURCE_REMOVE;
}

static void
queue_update_visible_icons(NolphinListView *view,
                           gint          delay)
{
    if (view->details->update_visible_icons_id > 0) {
        g_source_remove (view->details->update_visible_icons_id);
    }

    view->details->update_visible_icons_id = g_timeout_add (delay, (GSourceFunc) update_visible_icons_cb, view);
}

static void
handle_vadjustment_changed (GtkAdjustment *adjustment,
                            NolphinListView  *view)
{
    queue_update_visible_icons (view, NORMAL_UPDATE_VISIBLE_DELAY);
}

static gint
get_icon_scale_callback (NolphinListModel *model,
                         NolphinListView  *view)
{
   return gtk_widget_get_scale_factor (GTK_WIDGET (view->details->tree_view));
}

static gint
get_filter_match_callback (NolphinListModel *model,
                           NolphinFile      *file,
                           NolphinListView  *view)
{
    return nolphin_view_get_filter_match (NOLPHIN_VIEW (view), file);
}

static void
on_treeview_realized (GtkWidget *widget,
                      gpointer   user_data)
{
    NolphinListView *view = NOLPHIN_LIST_VIEW (user_data);
    GtkAdjustment *adjust;

    adjust = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (view->details->tree_view));
    g_signal_connect (adjust,
                      "value-changed",
                      G_CALLBACK (handle_vadjustment_changed),
                      view);
}

static void
on_size_allocation_changed (GtkWidget    *widget,
                            GdkRectangle *allocation,
                            gpointer      user_data)
{
    NolphinListView *view = NOLPHIN_LIST_VIEW (user_data);
    GtkAdjustment *adjustment;
    gdouble page_size, upper;

    adjustment = gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (view->details->tree_view));
    g_object_get (adjustment, "page-size", &page_size, "upper", &upper, NULL);

    if (view->details->overlay_scrolling && page_size < upper) {
        GtkWidget *hscrollbar = gtk_scrolled_window_get_hscrollbar (GTK_SCROLLED_WINDOW (view));
        gint nat_height;

        gtk_widget_get_preferred_height (hscrollbar, NULL, &nat_height);
        gtk_widget_set_margin_bottom (GTK_WIDGET (view->details->tree_view), nat_height + 2);
    }
    else {
        gtk_widget_set_margin_bottom (GTK_WIDGET (view->details->tree_view), 0);
    }

    gtk_widget_queue_allocate (widget);
}

static void
update_date_fonts (NolphinListView *view)
{
    g_return_if_fail (NOLPHIN_IS_LIST_VIEW (view));
    NolphinDateFontChoice mono_pref;
    gchar *font_name;
    PangoStyle date_style;
    gchar *date_name = NULL;
    gchar *date_family = NULL;

    GtkSettings *settings = gtk_settings_get_default ();
    g_object_get (settings, "gtk-font-name", &font_name, NULL);

    mono_pref = g_settings_get_enum (nolphin_preferences, NOLPHIN_PREFERENCES_DATE_FONT_CHOICE);

    if (g_settings_get_enum (nolphin_preferences, NOLPHIN_PREFERENCES_DATE_FORMAT) == NOLPHIN_DATE_FORMAT_INFORMAL ||
        mono_pref == NOLPHIN_DATE_FONT_CHOICE_NONE ||
        g_strstr_len (font_name, -1, "Mono")) {
        date_name = g_strdup (font_name);
    } else {
        if (mono_pref == NOLPHIN_DATE_FONT_CHOICE_AUTO) {
            PangoFontDescription *font_desc = pango_font_description_from_string (font_name);
            const gchar *current_font_family = pango_font_description_get_family (font_desc);

            if (current_font_family != NULL) {
                date_family = nolphin_global_preferences_get_mono_font_family_match (current_font_family);
            } else {
                g_warning ("No font family name set, not using monospace for date columns");
                date_family = NULL;
            }

            date_style = pango_font_description_get_style (font_desc);

            pango_font_description_free (font_desc);
        } else {
            date_name = nolphin_global_preferences_get_mono_system_font ();
        }
    }

    GList *combined = g_list_copy (view->details->cells);
    combined = g_list_prepend (combined, view->details->file_name_cell);
    GList *l;

    for (l = combined; l != NULL; l = l->next) {
        GtkCellRenderer *cell = GTK_CELL_RENDERER (l->data);
        const gchar *column_id = g_object_get_data (G_OBJECT (cell), "column-id");

        if (g_str_has_prefix (column_id, "date_")) {
            if (date_family) {
                g_object_set (GTK_CELL_RENDERER_TEXT (cell),
                              "family", date_family,
                              "style", date_style,
                              NULL);
            } else {
                g_object_set (GTK_CELL_RENDERER_TEXT (cell),
                              "font", date_name,
                              NULL);
            }
        }
        else {
            g_object_set (GTK_CELL_RENDERER_TEXT (cell),
                          "font", font_name,
                          NULL);
        }
    }

    gtk_widget_queue_draw (GTK_WIDGET (view->details->tree_view));

    g_list_free (combined);
    g_free (font_name);
    g_free (date_family);
    g_free (date_name);
}

static void
create_and_set_up_tree_view (NolphinListView *view)
{
	GtkCellRenderer *cell;
	GtkTreeViewColumn *column;
	GtkBindingSet *binding_set;
	AtkObject *atk_obj;
	GList *nolphin_columns;
	GList *l;
	gchar **default_column_order, **default_visible_columns;

	view->details->tree_view = GTK_TREE_VIEW (gtk_tree_view_new ());

    gtk_tree_view_set_rubber_banding (GTK_TREE_VIEW (view->details->tree_view), TRUE);

    gtk_tree_view_set_show_expanders (view->details->tree_view,
                                      g_settings_get_boolean (nolphin_list_view_preferences,
                                                              NOLPHIN_PREFERENCES_LIST_VIEW_ENABLE_EXPANSION));
    g_signal_connect_swapped (nolphin_list_view_preferences,
                              "changed::" NOLPHIN_PREFERENCES_LIST_VIEW_ENABLE_EXPANSION,
                              G_CALLBACK (expanders_enabled_changed_cb),
                              view);

	view->details->columns = g_hash_table_new_full (g_str_hash,
							g_str_equal,
							(GDestroyNotify) g_free,
							NULL);

	gtk_tree_view_set_enable_search (view->details->tree_view, FALSE);

	/* Don't handle backspace key. It's used to open the parent folder. */
	binding_set = gtk_binding_set_by_class (GTK_WIDGET_GET_CLASS (view->details->tree_view));
	gtk_binding_entry_remove (binding_set, GDK_KEY_BackSpace, 0);

	view->details->drag_dest =
		nolphin_tree_view_drag_dest_new (view->details->tree_view, TRUE);

	g_signal_connect_object (view->details->drag_dest,
				 "get_root_uri",
				 G_CALLBACK (get_root_uri_callback),
				 view, 0);
	g_signal_connect_object (view->details->drag_dest,
				 "get_file_for_path",
				 G_CALLBACK (get_file_for_path_callback),
				 view, 0);
	g_signal_connect_object (view->details->drag_dest,
				 "move_copy_items",
				 G_CALLBACK (move_copy_items_callback),
				 view, 0);
	g_signal_connect_object (view->details->drag_dest, "handle_netscape_url",
				 G_CALLBACK (list_view_handle_netscape_url), view, 0);
	g_signal_connect_object (view->details->drag_dest, "handle_uri_list",
				 G_CALLBACK (list_view_handle_uri_list), view, 0);
	g_signal_connect_object (view->details->drag_dest, "handle_text",
				 G_CALLBACK (list_view_handle_text), view, 0);
	g_signal_connect_object (view->details->drag_dest, "handle_raw",
				 G_CALLBACK (list_view_handle_raw), view, 0);

	g_signal_connect_object (gtk_tree_view_get_selection (view->details->tree_view),
				 "changed",
				 G_CALLBACK (list_selection_changed_callback), view, 0);

    g_signal_connect_object (GTK_WIDGET (view->details->tree_view), "query-tooltip",
                             G_CALLBACK (query_tooltip_callback), view, 0);

    g_signal_connect_object (view->details->tree_view, "drag_begin",
                 G_CALLBACK (drag_begin_callback), view, 0);
    g_signal_connect_object (view->details->tree_view, "drag-end",
                 G_CALLBACK (drag_end_callback), view, 0);
	g_signal_connect_object (view->details->tree_view, "drag_data_get",
				 G_CALLBACK (drag_data_get_callback), view, 0);
	g_signal_connect_object (view->details->tree_view, "motion_notify_event",
				 G_CALLBACK (motion_notify_callback), view, 0);
	g_signal_connect_object (view->details->tree_view, "enter_notify_event",
				 G_CALLBACK (enter_notify_callback), view, 0);
	g_signal_connect_object (view->details->tree_view, "leave_notify_event",
				 G_CALLBACK (leave_notify_callback), view, 0);
	g_signal_connect_object (view->details->tree_view, "button_press_event",
				 G_CALLBACK (button_press_callback), view, 0);
	g_signal_connect_object (view->details->tree_view, "button_release_event",
				 G_CALLBACK (button_release_callback), view, 0);
	g_signal_connect_object (view->details->tree_view, "key_press_event",
				 G_CALLBACK (key_press_callback), view, 0);
	g_signal_connect_object (view->details->tree_view, "popup_menu",
                                 G_CALLBACK (popup_menu_callback), view, 0);
	g_signal_connect_object (view->details->tree_view, "row_expanded",
                                 G_CALLBACK (row_expanded_callback), view, 0);
	g_signal_connect_object (view->details->tree_view, "row_collapsed",
                                 G_CALLBACK (row_collapsed_callback), view, 0);
	g_signal_connect_object (view->details->tree_view, "row-activated",
                                 G_CALLBACK (row_activated_callback), view, 0);
    g_signal_connect_object (view->details->tree_view, "test-expand-row",
                                 G_CALLBACK (test_expand_row_callback), view, 0);

    	g_signal_connect_object (view->details->tree_view, "focus_in_event",
				 G_CALLBACK(focus_in_event_callback), view, 0);

    g_signal_connect (view->details->tree_view, "realize", G_CALLBACK (on_treeview_realized), view);
    g_signal_connect (view->details->tree_view, "size-allocate", G_CALLBACK (on_size_allocation_changed), view);

	view->details->model = g_object_new (NOLPHIN_TYPE_LIST_MODEL, NULL);
    nolphin_list_model_set_expansion_enabled (view->details->model,
                                           g_settings_get_boolean (nolphin_list_view_preferences,
                                                                   NOLPHIN_PREFERENCES_LIST_VIEW_ENABLE_EXPANSION));

	gtk_tree_view_set_model (view->details->tree_view, GTK_TREE_MODEL (view->details->model));
	/* Need the model for the dnd drop icon "accept" change */
	nolphin_list_model_set_drag_view (NOLPHIN_LIST_MODEL (view->details->model),
				     view->details->tree_view,  0, 0);

	g_signal_connect_object (view->details->model, "sort_column_changed",
				 G_CALLBACK (sort_column_changed_callback), view, 0);

	g_signal_connect_object (view->details->model, "subdirectory_unloaded",
				 G_CALLBACK (subdirectory_unloaded_callback), view, 0);

    g_signal_connect_object (view->details->model, "get-icon-scale",
                 G_CALLBACK (get_icon_scale_callback), view, 0);
    g_signal_connect_object (view->details->model, "get-filter-match",
                 G_CALLBACK (get_filter_match_callback), view, 0);

	gtk_tree_selection_set_mode (gtk_tree_view_get_selection (view->details->tree_view), GTK_SELECTION_MULTIPLE);
	gtk_tree_view_set_rules_hint (view->details->tree_view, TRUE);

	nolphin_columns = nolphin_get_all_columns ();

	for (l = nolphin_columns; l != NULL; l = l->next) {
		NolphinColumn *nolphin_column;
		int column_num;
		char *name;
		char *label;
		float xalign;
        gint width_chars;
        gboolean ellipsize;


		nolphin_column = NOLPHIN_COLUMN (l->data);

		g_object_get (nolphin_column,
			      "name", &name,
			      "label", &label,
			      "xalign", &xalign,
                  "width-chars", &width_chars,
                  "ellipsize", &ellipsize, NULL);

		column_num = nolphin_list_model_add_column (view->details->model,
						       nolphin_column);

		/* Created the name column specially, because it
		 * has the icon in it.*/
		if (!strcmp (name, "name")) {
			/* Create the file name column */
			cell = gtk_cell_renderer_pixbuf_new ();
			view->details->pixbuf_cell = (GtkCellRendererPixbuf *)cell;

			view->details->file_name_column = gtk_tree_view_column_new ();
            gtk_tree_view_append_column (view->details->tree_view,
                                         view->details->file_name_column);

            g_object_set_data_full (G_OBJECT (view->details->file_name_column),
                                    "column-id", g_strdup (name),
                                    g_free);

			view->details->file_name_column_num = column_num;

			g_hash_table_insert (view->details->columns,
					     g_strdup ("name"),
					     view->details->file_name_column);

            g_signal_connect (gtk_tree_view_column_get_button (view->details->file_name_column),
                              "button-press-event",
                              G_CALLBACK (column_header_clicked),
                              view);

			gtk_tree_view_set_search_column (view->details->tree_view, column_num);

			gtk_tree_view_column_set_sort_column_id (view->details->file_name_column, column_num);
			gtk_tree_view_column_set_title (view->details->file_name_column, _("Name"));
			gtk_tree_view_column_set_resizable (view->details->file_name_column, TRUE);
            gtk_tree_view_column_set_min_width (view->details->file_name_column, 100);
            gtk_tree_view_column_set_reorderable (view->details->file_name_column, TRUE);
            gtk_tree_view_column_set_expand (view->details->file_name_column, TRUE);

			gtk_tree_view_column_pack_start (view->details->file_name_column, cell, FALSE);
			gtk_tree_view_column_set_attributes (view->details->file_name_column,
							     cell,
							     "surface", NOLPHIN_LIST_MODEL_SMALLEST_ICON_COLUMN,
							     NULL);

			cell = gtk_cell_renderer_text_new ();
			view->details->file_name_cell = (GtkCellRendererText *)cell;
            g_object_set (cell,
                          "xpad", 5,
                          "ellipsize", PANGO_ELLIPSIZE_END,
                          "width-chars", 40,
                          NULL);

            g_object_set_data_full (G_OBJECT (cell),
                                    "column-id", g_strdup ("filename"),
                                    g_free);

			g_signal_connect (cell, "edited", G_CALLBACK (cell_renderer_edited), view);
			g_signal_connect (cell, "editing-canceled", G_CALLBACK (cell_renderer_editing_canceled), view);
			g_signal_connect (cell, "editing-started", G_CALLBACK (cell_renderer_editing_started_cb), view);

			gtk_tree_view_column_pack_start (view->details->file_name_column, cell, TRUE);
			gtk_tree_view_column_set_cell_data_func (view->details->file_name_column, cell,
								 (GtkTreeCellDataFunc) filename_cell_data_func,
								 view, NULL);
		} else {
			cell = gtk_cell_renderer_text_new ();
            g_object_set (cell,
                          "xalign", xalign,
                          "xpad", 5,
                          "width-chars", width_chars,
                          "ellipsize", ellipsize,
                          NULL);

			view->details->cells = g_list_append (view->details->cells,
							      cell);
            g_object_set_data_full (G_OBJECT (cell),
                                    "column-id", g_strdup (name),
                                    g_free);

            column = gtk_tree_view_column_new ();
            g_object_set_data_full (G_OBJECT (column),
                                    "column-id", g_strdup (name),
                                    g_free);

            gtk_tree_view_column_set_title (column, label);
            gtk_tree_view_column_pack_start (column, cell, TRUE);
            gtk_tree_view_column_set_attributes (column, cell,
                                                 "text", column_num,
                                                 "weight", NOLPHIN_LIST_MODEL_TEXT_WEIGHT_COLUMN,
                                                 NULL);

            gtk_tree_view_append_column (view->details->tree_view, column);
            gtk_tree_view_column_set_min_width (column, 30);
			gtk_tree_view_column_set_sort_column_id (column, column_num);

            g_hash_table_insert (view->details->columns,
                                 g_strdup (name),
                                 column);

            g_signal_connect (gtk_tree_view_column_get_button (column),
                              "button-press-event",
                              G_CALLBACK (column_header_clicked),
                              view);

			gtk_tree_view_column_set_resizable (column, TRUE);
            gtk_tree_view_column_set_reorderable (column, TRUE);
		}
		g_free (name);
		g_free (label);
	}

    update_date_fonts (view);
    GtkSettings *gtk_settings = gtk_settings_get_default ();
    g_signal_connect_swapped (gtk_settings, "notify::gtk-font-name", G_CALLBACK (update_date_fonts), view);
    g_signal_connect_swapped (nolphin_preferences, "changed::" NOLPHIN_PREFERENCES_DATE_FONT_CHOICE, G_CALLBACK (update_date_fonts), view);
    g_signal_connect_swapped (gnome_interface_preferences, "changed::" NOLPHIN_PREFERENCES_MONO_FONT_NAME, G_CALLBACK (update_date_fonts), view);
	nolphin_column_list_free (nolphin_columns);

	default_visible_columns = g_settings_get_strv (nolphin_list_view_preferences,
						       NOLPHIN_PREFERENCES_LIST_VIEW_DEFAULT_VISIBLE_COLUMNS);
	default_column_order = g_settings_get_strv (nolphin_list_view_preferences,
						    NOLPHIN_PREFERENCES_LIST_VIEW_DEFAULT_COLUMN_ORDER);

	/* Apply the default column order and visible columns, to get it
	 * right most of the time. The metadata will be checked when a
	 * folder is loaded */
	apply_columns_settings (view,
				default_column_order,
				default_visible_columns);

	gtk_widget_show (GTK_WIDGET (view->details->tree_view));
	gtk_container_add (GTK_CONTAINER (view), GTK_WIDGET (view->details->tree_view));

        atk_obj = gtk_widget_get_accessible (GTK_WIDGET (view->details->tree_view));
        atk_object_set_name (atk_obj, _("Listenansicht"));

    gtk_widget_set_has_tooltip (GTK_WIDGET (view->details->tree_view), TRUE);

	g_strfreev (default_visible_columns);
	g_strfreev (default_column_order);
}

static void
nolphin_list_view_add_file (NolphinView *view, NolphinFile *file, NolphinDirectory *directory)
{
	NolphinListModel *model;

    if (nolphin_file_has_thumbnail_access_problem (file)) {
        nolphin_application_set_cache_flag (nolphin_application_get_singleton ());
        nolphin_window_slot_check_bad_cache_bar (nolphin_view_get_nolphin_window_slot (view));
    }

	model = NOLPHIN_LIST_VIEW (view)->details->model;
	nolphin_list_model_add_file (model, file, directory);
    queue_update_visible_icons (NOLPHIN_LIST_VIEW (view), INITIAL_UPDATE_VISIBLE_DELAY);
}

static char **
get_default_visible_columns (NolphinListView *list_view)
{
    NolphinFile *file;

    file = nolphin_view_get_directory_as_file (NOLPHIN_VIEW (list_view));

    if (nolphin_file_is_in_trash (file)) {
        return g_strdupv ((gchar **) default_trash_visible_columns);
    }

    if (nolphin_file_is_in_recent (file)) {
        return g_strdupv ((gchar **) default_recent_visible_columns);
    }

    if (nolphin_file_is_in_favorites (file)) {
        return g_strdupv ((gchar **) default_favorites_visible_columns);
    }

    if (nolphin_file_is_in_search (file)) {
        return g_strdupv ((gchar **) default_search_columns);
    }

    return g_settings_get_strv (nolphin_list_view_preferences,
                                NOLPHIN_PREFERENCES_LIST_VIEW_DEFAULT_VISIBLE_COLUMNS);
}

static char **
get_visible_columns (NolphinListView *list_view)
{
	NolphinFile *file;
	GList *visible_columns;
	char **ret;

	ret = NULL;
    visible_columns = NULL;

	file = nolphin_view_get_directory_as_file (NOLPHIN_VIEW (list_view));

    if (nolphin_global_preferences_get_ignore_view_metadata ()) {
        visible_columns = nolphin_window_get_ignore_meta_visible_columns (nolphin_view_get_nolphin_window (NOLPHIN_VIEW (list_view)));
    } else {
        if (nolphin_file_is_in_search (file)) {
            gchar **modified_cols;

            modified_cols = g_settings_get_strv (nolphin_search_preferences, NOLPHIN_PREFERENCES_SEARCH_VISIBLE_COLUMNS);

            if (g_strv_length (modified_cols) > 0) {
                return modified_cols;
            } else {
                g_strfreev (modified_cols);
            }
        } else {
            visible_columns = nolphin_file_get_metadata_list (file, NOLPHIN_METADATA_KEY_LIST_VIEW_VISIBLE_COLUMNS);
        }
    }

	if (visible_columns) {
        ret = string_array_from_string_glist (visible_columns);
        g_list_free_full (visible_columns, g_free);
	}

	if (ret != NULL) {
		return ret;
	}

	return get_default_visible_columns (list_view);
}

static char **
get_default_column_order (NolphinListView *list_view)
{
    NolphinFile *file;

    file = nolphin_view_get_directory_as_file (NOLPHIN_VIEW (list_view));

    if (nolphin_file_is_in_trash (file)) {
        return g_strdupv ((gchar **) default_trash_columns_order);
    }

    if (nolphin_file_is_in_recent (file)) {
        return g_strdupv ((gchar **) default_recent_columns_order);
    }

    if (nolphin_file_is_in_favorites (file)) {
        return g_strdupv ((gchar **) default_favorites_columns_order);
    }

    if (nolphin_file_is_in_search (file)) {
        return g_strdupv ((gchar **) default_search_columns);
    }

    return g_settings_get_strv (nolphin_list_view_preferences,
                                NOLPHIN_PREFERENCES_LIST_VIEW_DEFAULT_COLUMN_ORDER);
}

static char **
get_column_order (NolphinListView *list_view)
{
	NolphinFile *file;
	GList *column_order;
	char **ret;

    column_order = NULL;
	ret = NULL;

	file = nolphin_view_get_directory_as_file (NOLPHIN_VIEW (list_view));

    if (nolphin_global_preferences_get_ignore_view_metadata ()) {
        column_order = nolphin_window_get_ignore_meta_column_order (nolphin_view_get_nolphin_window (NOLPHIN_VIEW (list_view)));
    } else {
        if (nolphin_file_is_in_search (file)) {
            gchar **modified_cols;
            modified_cols = g_settings_get_strv (nolphin_search_preferences, NOLPHIN_PREFERENCES_SEARCH_VISIBLE_COLUMNS);

            if (g_strv_length (modified_cols) > 0) {
                return modified_cols;
            } else {
                g_strfreev (modified_cols);
            }
        } else {
            column_order = nolphin_file_get_metadata_list (file, NOLPHIN_METADATA_KEY_LIST_VIEW_COLUMN_ORDER);
        }
    }

    if (column_order) {
        ret = string_array_from_string_glist (column_order);
        g_list_free_full (column_order, g_free);
    }

	if (ret != NULL) {
		return ret;
	}

	return get_default_column_order (list_view);
}

static void
set_columns_settings_from_metadata_and_preferences (NolphinListView *list_view)
{
	char **column_order;
	char **visible_columns;

	column_order = get_column_order (list_view);
	visible_columns = get_visible_columns (list_view);

	apply_columns_settings (list_view, column_order, visible_columns);

	g_strfreev (column_order);
	g_strfreev (visible_columns);
}

static void
set_sort_order_from_metadata_and_preferences (NolphinListView *list_view)
{
	char *sort_attribute;
	int sort_column_id;
	NolphinFile *file;
	gboolean sort_reversed, default_sort_reversed;
	const gchar *default_sort_order;

	file = nolphin_view_get_directory_as_file (NOLPHIN_VIEW (list_view));

        if (nolphin_global_preferences_get_ignore_view_metadata ())
                sort_attribute = g_strdup (nolphin_window_get_ignore_meta_sort_column (nolphin_view_get_nolphin_window (NOLPHIN_VIEW (list_view))));
        else if (nolphin_file_is_in_search (file)) {
            sort_attribute = g_settings_get_string (nolphin_search_preferences, NOLPHIN_PREFERENCES_SEARCH_SORT_COLUMN);
        } else {
            sort_attribute = nolphin_file_get_metadata (file,
                                                     NOLPHIN_METADATA_KEY_LIST_VIEW_SORT_COLUMN,
                                                     NULL);
        }
	sort_column_id = nolphin_list_model_get_sort_column_id_from_attribute (list_view->details->model,
									  g_quark_from_string (sort_attribute));
	g_free (sort_attribute);

	default_sort_order = get_default_sort_order (file, &default_sort_reversed);

	if (sort_column_id == -1) {
		sort_column_id =
			nolphin_list_model_get_sort_column_id_from_attribute (list_view->details->model,
									 g_quark_from_string (default_sort_order));
	}

    if (nolphin_global_preferences_get_ignore_view_metadata ()) {
        gint dir = nolphin_window_get_ignore_meta_sort_direction (nolphin_view_get_nolphin_window (NOLPHIN_VIEW (list_view)));
        sort_reversed = dir > SORT_NULL ? dir == SORT_DESCENDING : default_sort_reversed;
    } else if (nolphin_file_is_in_search (file)) {
        sort_reversed = g_settings_get_boolean (nolphin_search_preferences, NOLPHIN_PREFERENCES_SEARCH_REVERSE_SORT);
    } else {
        sort_reversed = nolphin_file_get_boolean_metadata (file,
                                                        NOLPHIN_METADATA_KEY_LIST_VIEW_SORT_REVERSED,
                                                        default_sort_reversed);
    }
    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (list_view->details->model),
                                                             sort_column_id,
                                                             sort_reversed ? GTK_SORT_DESCENDING : GTK_SORT_ASCENDING);
}

static gboolean
list_view_changed_foreach (GtkTreeModel *model,
              		   GtkTreePath  *path,
			   GtkTreeIter  *iter,
			   gpointer      data)
{
	gtk_tree_model_row_changed (model, path, iter);
	return FALSE;
}

static NolphinZoomLevel
get_default_zoom_level (void) {
	NolphinZoomLevel default_zoom_level;

	default_zoom_level = g_settings_get_enum (nolphin_list_view_preferences,
						  NOLPHIN_PREFERENCES_LIST_VIEW_DEFAULT_ZOOM_LEVEL);

	if (default_zoom_level <  NOLPHIN_ZOOM_LEVEL_SMALLEST
	    || NOLPHIN_ZOOM_LEVEL_LARGEST < default_zoom_level) {
		default_zoom_level = NOLPHIN_ZOOM_LEVEL_SMALL;
	}

	return default_zoom_level;
}

static void
set_zoom_level_from_metadata_and_preferences (NolphinListView *list_view)
{
	NolphinFile *file;
	int level;

	if (nolphin_view_supports_zooming (NOLPHIN_VIEW (list_view))) {
		file = nolphin_view_get_directory_as_file (NOLPHIN_VIEW (list_view));
        if (nolphin_global_preferences_get_ignore_view_metadata ()) {
            gchar *uri;

            uri = nolphin_file_get_uri (file);

            if (eel_uri_is_search (uri)) {
                level = get_default_zoom_level ();
            } else {
                gint ignore_level;
                ignore_level = nolphin_window_get_ignore_meta_zoom_level (nolphin_view_get_nolphin_window (NOLPHIN_VIEW (list_view)));

                level = ignore_level > -1 ? ignore_level : get_default_zoom_level ();
            }

            g_free (uri);
        } else {
            level = nolphin_file_get_integer_metadata (file,
							    NOLPHIN_METADATA_KEY_LIST_VIEW_ZOOM_LEVEL,
							    get_default_zoom_level ());
        }
		nolphin_list_view_set_zoom_level (list_view, level, TRUE);

		/* updated the rows after updating the font size */
		gtk_tree_model_foreach (GTK_TREE_MODEL (list_view->details->model),
					list_view_changed_foreach, NULL);
	}
}

static void
nolphin_list_view_begin_loading (NolphinView *view)
{
	NolphinListView *list_view;

	list_view = NOLPHIN_LIST_VIEW (view);

	set_sort_order_from_metadata_and_preferences (list_view);
	set_zoom_level_from_metadata_and_preferences (list_view);
	set_columns_settings_from_metadata_and_preferences (list_view);

    gtk_widget_set_margin_bottom (GTK_WIDGET (list_view->details->tree_view), 0);

    set_ok_to_load_deferred_attrs (list_view, FALSE);

    nolphin_list_model_set_view_directory (list_view->details->model, nolphin_view_get_model (view));

    AtkObject *atk = gtk_widget_get_accessible (GTK_WIDGET (NOLPHIN_LIST_VIEW (view)->details->tree_view));

    g_signal_connect_object (atk, "column-reordered",
                             G_CALLBACK (columns_reordered_callback), view, 0);
}

static void
stop_cell_editing (NolphinListView *list_view)
{
	GtkTreeViewColumn *column;

	/* Stop an ongoing rename to commit the name changes when the user
	 * changes directories without exiting cell edit mode. It also prevents
	 * the edited handler from being called on the cleared list model.
	 */
	column = list_view->details->file_name_column;
	if (column != NULL && list_view->details->editable_widget != NULL &&
	    GTK_IS_CELL_EDITABLE (list_view->details->editable_widget)) {
		gtk_cell_editable_editing_done (list_view->details->editable_widget);
	}
}

static void
nolphin_list_view_clear (NolphinView *view)
{
	NolphinListView *list_view;
    GtkTreeSelection *tree_selection;

	list_view = NOLPHIN_LIST_VIEW (view);

    list_view->details->ok_to_load_deferred_attrs = FALSE;

    if (list_view->details->update_visible_icons_id > 0) {
        g_source_remove (list_view->details->update_visible_icons_id);
        list_view->details->update_visible_icons_id = 0;
    }

    tree_selection = gtk_tree_view_get_selection (list_view->details->tree_view);

    g_signal_handlers_block_by_func (tree_selection, list_selection_changed_callback, view);

	if (list_view->details->model != NULL) {
		stop_cell_editing (list_view);
		nolphin_list_model_clear (list_view->details->model);
	}

    g_signal_handlers_unblock_by_func (tree_selection, list_selection_changed_callback, view);
}

static void
nolphin_list_view_rename_callback (NolphinFile *file,
				    GFile *result_location,
				    GError *error,
				    gpointer callback_data)
{
	NolphinListView *view;

	view = NOLPHIN_LIST_VIEW (callback_data);

	if (view->details->renaming_file) {
		view->details->rename_done = TRUE;

		if (error != NULL) {
			/* If the rename failed (or was cancelled), kill renaming_file.
			 * We won't get a change event for the rename, so otherwise
			 * it would stay around forever.
			 */
			nolphin_file_unref (view->details->renaming_file);
			view->details->renaming_file = NULL;
		}
	}

	g_object_unref (view);
}


static void
nolphin_list_view_file_changed (NolphinView *view, NolphinFile *file, NolphinDirectory *directory)
{
	NolphinListView *listview;
	GtkTreeIter iter;
	GtkTreePath *file_path;

	listview = NOLPHIN_LIST_VIEW (view);

	nolphin_list_model_file_changed (listview->details->model, file, directory);

	if (listview->details->renaming_file != NULL &&
	    file == listview->details->renaming_file &&
	    listview->details->rename_done) {
		/* This is (probably) the result of the rename operation, and
		 * the tree-view changes above could have resorted the list, so
		 * scroll to the new position
		 */
		if (nolphin_list_model_get_tree_iter_from_file (listview->details->model, file, directory, &iter)) {
			file_path = gtk_tree_model_get_path (GTK_TREE_MODEL (listview->details->model), &iter);
			gtk_tree_view_scroll_to_cell (listview->details->tree_view,
						      file_path, NULL,
						      FALSE, 0.0, 0.0);
			gtk_tree_path_free (file_path);
		}

		nolphin_file_unref (listview->details->renaming_file);
		listview->details->renaming_file = NULL;
	}
}

typedef struct {
	GtkTreePath *path;
	gboolean is_common;
	gboolean is_root;
} HasCommonParentData;

static void
tree_selection_has_common_parent_foreach_func (GtkTreeModel *model,
						GtkTreePath *path,
						GtkTreeIter *iter,
						gpointer user_data)
{
	HasCommonParentData *data;
	GtkTreePath *parent_path;
	gboolean has_parent;

	data = (HasCommonParentData *) user_data;

	parent_path = gtk_tree_path_copy (path);
	gtk_tree_path_up (parent_path);

	has_parent = (gtk_tree_path_get_depth (parent_path) > 0) ? TRUE : FALSE;

	if (!has_parent) {
		data->is_root = TRUE;
	}

	if (data->is_common && !data->is_root) {
		if (data->path == NULL) {
			data->path = gtk_tree_path_copy (parent_path);
		} else if (gtk_tree_path_compare (data->path, parent_path) != 0) {
			data->is_common = FALSE;
		}
	}

	gtk_tree_path_free (parent_path);
}

static void
tree_selection_has_common_parent (GtkTreeSelection *selection,
				  gboolean *is_common,
				  gboolean *is_root)
{
	HasCommonParentData data;

	g_assert (is_common != NULL);
	g_assert (is_root != NULL);

	data.path = NULL;
	data.is_common = *is_common = TRUE;
	data.is_root = *is_root = FALSE;

	gtk_tree_selection_selected_foreach (selection,
					     tree_selection_has_common_parent_foreach_func,
					     &data);

	*is_common = data.is_common;
	*is_root = data.is_root;

	if (data.path != NULL) {
		gtk_tree_path_free (data.path);
	}
}

static char *
nolphin_list_view_get_backing_uri (NolphinView *view)
{
	NolphinListView *list_view;
	NolphinListModel *list_model;
	NolphinFile *file;
	GtkTreeView *tree_view;
	GtkTreeSelection *selection;
	GtkTreePath *path;
	GList *paths;
	guint length;
	char *uri;

	g_return_val_if_fail (NOLPHIN_IS_LIST_VIEW (view), NULL);

	list_view = NOLPHIN_LIST_VIEW (view);
	list_model = list_view->details->model;
	tree_view = list_view->details->tree_view;

	g_assert (list_model);

	/* We currently handle three common cases here:
	 * (a) if the selection contains non-filesystem items (i.e., the
	 *     "(Empty)" label), we return the uri of the parent.
	 * (b) if the selection consists of exactly one _expanded_ directory, we
	 *     return its URI.
	 * (c) if the selection consists of either exactly one item which is not
	 *     an expanded directory) or multiple items in the same directory,
	 *     we return the URI of the common parent.
	 */

	uri = NULL;

	selection = gtk_tree_view_get_selection (tree_view);
	length = gtk_tree_selection_count_selected_rows (selection);

	if (length == 1) {

		paths = gtk_tree_selection_get_selected_rows (selection, NULL);
		path = (GtkTreePath *) paths->data;

		file = nolphin_list_model_file_for_path (list_model, path);
		if (file == NULL) {
			/* The selected item is a label, not a file */
			gtk_tree_path_up (path);
			file = nolphin_list_model_file_for_path (list_model, path);
		}

		if (file != NULL) {
			if (nolphin_file_is_directory (file) &&
			    gtk_tree_view_row_expanded (tree_view, path)) {
				uri = nolphin_file_get_uri (file);
			}
			nolphin_file_unref (file);
		}

		gtk_tree_path_free (path);
		g_list_free (paths);
	}

	if (uri == NULL && length > 0) {

		gboolean is_common, is_root;

		/* Check that all the selected items belong to the same
		 * directory and that directory is not the root directory (which
		 * is handled by NolphinView::get_backing_directory.) */

		tree_selection_has_common_parent (selection, &is_common, &is_root);

		if (is_common && !is_root) {

			paths = gtk_tree_selection_get_selected_rows (selection, NULL);
			path = (GtkTreePath *) paths->data;

			file = nolphin_list_model_file_for_path (list_model, path);
			g_assert (file != NULL);
			uri = nolphin_file_get_parent_uri (file);
			nolphin_file_unref (file);

			g_list_foreach (paths, (GFunc) gtk_tree_path_free, NULL);
			g_list_free (paths);
		}
	}

	if (uri != NULL) {
		return uri;
	}

	return NOLPHIN_VIEW_CLASS (nolphin_list_view_parent_class)->get_backing_uri (view);
}

static void
nolphin_list_view_get_selection_foreach_func (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
	GList **list;
	NolphinFile *file;

	list = data;

	gtk_tree_model_get (model, iter,
			    NOLPHIN_LIST_MODEL_FILE_COLUMN, &file,
			    -1);

	if (file != NULL) {
		(* list) = g_list_prepend ((* list), file);
	}
}

static GList *
nolphin_list_view_get_selection (NolphinView *view)
{
	GList *list;

	list = NULL;

	gtk_tree_selection_selected_foreach (gtk_tree_view_get_selection (NOLPHIN_LIST_VIEW (view)->details->tree_view),
					     nolphin_list_view_get_selection_foreach_func, &list);

	return g_list_reverse (list);
}

static GList *
nolphin_list_view_peek_selection (NolphinView *view)
{
    NolphinListView *list_view = NOLPHIN_LIST_VIEW (view);

    if (list_view->details->current_selection_count == -1) {
        nolphin_list_view_update_selection (NOLPHIN_VIEW (list_view));
    }

    return list_view->details->current_selection;
}

static gint
nolphin_list_view_get_selection_count (NolphinView *view)
{
    NolphinListView *list_view = NOLPHIN_LIST_VIEW (view);

    if (list_view->details->current_selection_count == -1) {
        nolphin_list_view_update_selection (NOLPHIN_VIEW (list_view));
    }

    return list_view->details->current_selection_count;
}

static void
nolphin_list_view_update_selection (NolphinView *view)
{
    NolphinListView *list_view = NOLPHIN_LIST_VIEW (view);

    if (list_view->details->current_selection != NULL) {
        g_list_free (list_view->details->current_selection);

        list_view->details->current_selection = NULL;
        list_view->details->current_selection_count = 0;
    }

    list_view->details->current_selection = nolphin_list_view_get_selection (view);
    list_view->details->current_selection_count = g_list_length (list_view->details->current_selection);
}

static void
nolphin_list_view_get_selection_for_file_transfer_foreach_func (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
	NolphinFile *file;
	struct SelectionForeachData *selection_data;
	GtkTreeIter parent, child;

	selection_data = data;

	gtk_tree_model_get (model, iter,
			    NOLPHIN_LIST_MODEL_FILE_COLUMN, &file,
			    -1);

	if (file != NULL) {
		/* If the parent folder is also selected, don't include this file in the
		 * file operation, since that would copy it to the toplevel target instead
		 * of keeping it as a child of the copied folder
		 */
		child = *iter;
		while (gtk_tree_model_iter_parent (model, &parent, &child)) {
			if (gtk_tree_selection_iter_is_selected (selection_data->selection,
								 &parent)) {
				return;
			}
			child = parent;
		}

		nolphin_file_ref (file);
		selection_data->list = g_list_prepend (selection_data->list, file);
	}
}


static GList *
nolphin_list_view_get_selection_for_file_transfer (NolphinView *view)
{
	struct SelectionForeachData selection_data;

	selection_data.list = NULL;
	selection_data.selection = gtk_tree_view_get_selection (NOLPHIN_LIST_VIEW (view)->details->tree_view);

	gtk_tree_selection_selected_foreach (selection_data.selection,
					     nolphin_list_view_get_selection_for_file_transfer_foreach_func, &selection_data);

	return g_list_reverse (selection_data.list);
}




static guint
nolphin_list_view_get_item_count (NolphinView *view)
{
	g_return_val_if_fail (NOLPHIN_IS_LIST_VIEW (view), 0);

	return nolphin_list_model_get_length (NOLPHIN_LIST_VIEW (view)->details->model);
}

static gboolean
nolphin_list_view_is_empty (NolphinView *view)
{
	return nolphin_list_model_is_empty (NOLPHIN_LIST_VIEW (view)->details->model);
}

static void
nolphin_list_view_end_file_changes (NolphinView *view)
{
	NolphinListView *list_view;

	list_view = NOLPHIN_LIST_VIEW (view);

	if (list_view->details->new_selection_path) {
		gtk_tree_view_set_cursor (list_view->details->tree_view,
					  list_view->details->new_selection_path,
					  NULL, FALSE);
		gtk_tree_path_free (list_view->details->new_selection_path);
		list_view->details->new_selection_path = NULL;
	}
}

static void
nolphin_list_view_remove_file (NolphinView *view, NolphinFile *file, NolphinDirectory *directory)
{
	GtkTreePath *path;
	GtkTreePath *file_path;
	GtkTreeIter iter;
	GtkTreeIter temp_iter;
	GtkTreeRowReference* row_reference;
	NolphinListView *list_view;
	GtkTreeModel* tree_model;
	GtkTreeSelection *selection;

	path = NULL;
	row_reference = NULL;
	list_view = NOLPHIN_LIST_VIEW (view);
	tree_model = GTK_TREE_MODEL(list_view->details->model);

	if (nolphin_list_model_get_tree_iter_from_file (list_view->details->model, file, directory, &iter)) {
		selection = gtk_tree_view_get_selection (list_view->details->tree_view);
		file_path = gtk_tree_model_get_path (tree_model, &iter);

		if (gtk_tree_selection_path_is_selected (selection, file_path)) {
			/* get reference for next element in the list view. If the element to be deleted is the
			 * last one, get reference to previous element. If there is only one element in view
			 * no need to select anything.
			 */
			temp_iter = iter;

			if (gtk_tree_model_iter_next (tree_model, &iter)) {
				path = gtk_tree_model_get_path (tree_model, &iter);
				row_reference = gtk_tree_row_reference_new (tree_model, path);
			} else {
				path = gtk_tree_model_get_path (tree_model, &temp_iter);
				if (gtk_tree_path_prev (path)) {
					row_reference = gtk_tree_row_reference_new (tree_model, path);
				}
			}
			gtk_tree_path_free (path);
		}

		gtk_tree_path_free (file_path);

		nolphin_list_model_remove_file (list_view->details->model, file, directory);

		if (gtk_tree_row_reference_valid (row_reference)) {
			if (list_view->details->new_selection_path) {
				gtk_tree_path_free (list_view->details->new_selection_path);
			}
			list_view->details->new_selection_path = gtk_tree_row_reference_get_path (row_reference);
		}

		if (row_reference) {
			gtk_tree_row_reference_free (row_reference);
		}
	}


}

static void
nolphin_list_view_set_selection (NolphinView *view, GList *selection)
{
	NolphinListView *list_view;
	GtkTreeSelection *tree_selection;
	GList *node;
	GList *iters, *l;
	NolphinFile *file;

	list_view = NOLPHIN_LIST_VIEW (view);
	tree_selection = gtk_tree_view_get_selection (list_view->details->tree_view);

	g_signal_handlers_block_by_func (tree_selection, list_selection_changed_callback, view);

	gtk_tree_selection_unselect_all (tree_selection);
	for (node = selection; node != NULL; node = node->next) {
		file = node->data;
		iters = nolphin_list_model_get_all_iters_for_file (list_view->details->model, file);

		for (l = iters; l != NULL; l = l->next) {
			gtk_tree_selection_select_iter (tree_selection,
							(GtkTreeIter *)l->data);
		}
		g_list_free_full (iters, g_free);
	}

	g_signal_handlers_unblock_by_func (tree_selection, list_selection_changed_callback, view);
	nolphin_view_notify_selection_changed (view);
}

static void
nolphin_list_view_invert_selection (NolphinView *view)
{
	NolphinListView *list_view;
	GtkTreeSelection *tree_selection;
	GList *node;
	GList *iters, *l;
	NolphinFile *file;
	GList *selection = NULL;

	list_view = NOLPHIN_LIST_VIEW (view);
	tree_selection = gtk_tree_view_get_selection (list_view->details->tree_view);

	g_signal_handlers_block_by_func (tree_selection, list_selection_changed_callback, view);

	gtk_tree_selection_selected_foreach (tree_selection,
					     nolphin_list_view_get_selection_foreach_func, &selection);

	gtk_tree_selection_select_all (tree_selection);

	for (node = selection; node != NULL; node = node->next) {
		file = node->data;
		iters = nolphin_list_model_get_all_iters_for_file (list_view->details->model, file);

		for (l = iters; l != NULL; l = l->next) {
			gtk_tree_selection_unselect_iter (tree_selection,
							  (GtkTreeIter *)l->data);
		}
		g_list_free_full (iters, g_free);
	}

	g_list_free (selection);

	g_signal_handlers_unblock_by_func (tree_selection, list_selection_changed_callback, view);
	nolphin_view_notify_selection_changed (view);
}

static void
nolphin_list_view_select_all (NolphinView *view)
{
	gtk_tree_selection_select_all (gtk_tree_view_get_selection (NOLPHIN_LIST_VIEW (view)->details->tree_view));
}

static void
nolphin_list_view_merge_menus (NolphinView *view)
{
  NolphinListView *list_view;
  GtkUIManager *ui_manager;
  GtkActionGroup *action_group;

  list_view = NOLPHIN_LIST_VIEW (view);

  NOLPHIN_VIEW_CLASS (nolphin_list_view_parent_class)->merge_menus (view);

  ui_manager = nolphin_view_get_ui_manager (view);

  action_group = gtk_action_group_new ("ListViewActions");
  gtk_action_group_set_translation_domain (action_group, GETTEXT_PACKAGE);
  list_view->details->list_action_group = action_group;

  gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);
  g_object_unref (action_group); /* owned by ui manager */

  list_view->details->list_merge_id =
    gtk_ui_manager_add_ui_from_resource (ui_manager, "/org/nolphin/nolphin-list-view-ui.xml", NULL);

  list_view->details->menus_ready = TRUE;
}

static void
nolphin_list_view_unmerge_menus (NolphinView *view)
{
  NolphinListView *list_view;
  GtkUIManager *ui_manager;

  list_view = NOLPHIN_LIST_VIEW (view);

  NOLPHIN_VIEW_CLASS (nolphin_list_view_parent_class)->unmerge_menus (view);

  ui_manager = nolphin_view_get_ui_manager (view);
  if (ui_manager != NULL) {
    nolphin_ui_unmerge_ui (ui_manager,
          &list_view->details->list_merge_id,
          &list_view->details->list_action_group);
  }
}

static void
nolphin_list_view_update_menus (NolphinView *view)
{
	NolphinListView *list_view;

        list_view = NOLPHIN_LIST_VIEW (view);

	/* don't update if the menus aren't ready */
	if (!list_view->details->menus_ready) {
		return;
	}

	NOLPHIN_VIEW_CLASS (nolphin_list_view_parent_class)->update_menus (view);
}

/* Reset sort criteria and zoom level to match defaults */
static void
nolphin_list_view_reset_to_defaults (NolphinView *view)
{
	NolphinFile *file;

	file = nolphin_view_get_directory_as_file (view);

    g_signal_handlers_block_by_func (NOLPHIN_LIST_VIEW (view)->details->tree_view,
                                     columns_reordered_callback,
                                     NOLPHIN_LIST_VIEW (view));

    if (nolphin_global_preferences_get_ignore_view_metadata ()) {
        NolphinWindow *window = nolphin_view_get_nolphin_window (NOLPHIN_VIEW (view));
        nolphin_window_set_ignore_meta_sort_column (window, NULL);
        nolphin_window_set_ignore_meta_sort_direction (window, SORT_NULL);
        nolphin_window_set_ignore_meta_zoom_level (window, NOLPHIN_ZOOM_LEVEL_NULL);
        nolphin_window_set_ignore_meta_column_order (window, NULL);
        nolphin_window_set_ignore_meta_visible_columns (window, NULL);
    } else if (nolphin_file_is_in_search (file)) {
        g_settings_reset (nolphin_search_preferences, NOLPHIN_PREFERENCES_SEARCH_VISIBLE_COLUMNS);
        g_settings_reset (nolphin_search_preferences, NOLPHIN_PREFERENCES_SEARCH_SORT_COLUMN);
        g_settings_reset (nolphin_search_preferences, NOLPHIN_PREFERENCES_SEARCH_REVERSE_SORT);
    } else {
        nolphin_file_set_metadata (file, NOLPHIN_METADATA_KEY_LIST_VIEW_SORT_COLUMN, NULL, NULL);
        nolphin_file_set_metadata (file, NOLPHIN_METADATA_KEY_LIST_VIEW_SORT_REVERSED, NULL, NULL);
        nolphin_file_set_metadata (file, NOLPHIN_METADATA_KEY_LIST_VIEW_ZOOM_LEVEL, NULL, NULL);
        nolphin_file_set_metadata_list (file, NOLPHIN_METADATA_KEY_LIST_VIEW_COLUMN_ORDER, NULL);
        nolphin_file_set_metadata_list (file, NOLPHIN_METADATA_KEY_LIST_VIEW_VISIBLE_COLUMNS, NULL);
    }


    char **default_columns, **default_order;

    default_columns = get_default_visible_columns (NOLPHIN_LIST_VIEW (view));
    default_order = get_default_column_order (NOLPHIN_LIST_VIEW (view));
    apply_columns_settings (NOLPHIN_LIST_VIEW (view), default_order, default_columns);

    g_signal_handlers_unblock_by_func (NOLPHIN_LIST_VIEW (view)->details->tree_view,
                                       columns_reordered_callback,
                                       NOLPHIN_LIST_VIEW (view));
}

static void
nolphin_list_view_scale_font_size (NolphinListView *view,
				    NolphinZoomLevel new_level)
{
	GList *l;
	static gboolean first_time = TRUE;
	static double pango_scale[7];
	int medium;
	int i;

	g_return_if_fail (new_level >= NOLPHIN_ZOOM_LEVEL_SMALLEST &&
			  new_level <= NOLPHIN_ZOOM_LEVEL_LARGEST);

	if (first_time) {
		first_time = FALSE;
		medium = NOLPHIN_ZOOM_LEVEL_SMALLER;
		pango_scale[medium] = PANGO_SCALE_MEDIUM;
		for (i = medium; i > NOLPHIN_ZOOM_LEVEL_SMALLEST; i--) {
			pango_scale[i - 1] = (1 / 1.2) * pango_scale[i];
		}
		for (i = medium; i < NOLPHIN_ZOOM_LEVEL_LARGEST; i++) {
			pango_scale[i + 1] = 1.2 * pango_scale[i];
		}
	}

	g_object_set (G_OBJECT (view->details->file_name_cell),
		      "scale", pango_scale[new_level],
		      NULL);
	for (l = view->details->cells; l != NULL; l = l->next) {
		g_object_set (G_OBJECT (l->data),
			      "scale", pango_scale[new_level],
			      NULL);
	}
}

static void
nolphin_list_view_set_zoom_level (NolphinListView *view,
				   NolphinZoomLevel new_level,
				   gboolean always_emit)
{
    NolphinFile *file;
	int icon_size;
	int column;

	g_return_if_fail (NOLPHIN_IS_LIST_VIEW (view));
	g_return_if_fail (new_level >= NOLPHIN_ZOOM_LEVEL_SMALLEST &&
			  new_level <= NOLPHIN_ZOOM_LEVEL_LARGEST);

	if (view->details->zoom_level == new_level) {
		if (always_emit) {
			g_signal_emit_by_name (NOLPHIN_VIEW(view), "zoom_level_changed");
		}
		return;
	}

	view->details->zoom_level = new_level;
	g_signal_emit_by_name (NOLPHIN_VIEW(view), "zoom_level_changed");

    file = nolphin_view_get_directory_as_file (NOLPHIN_VIEW (view));

    if (nolphin_global_preferences_get_ignore_view_metadata ()) {
        gchar *uri;

        uri = nolphin_file_get_uri (file);

        if (!eel_uri_is_search (uri)) {
            nolphin_window_set_ignore_meta_zoom_level (nolphin_view_get_nolphin_window (NOLPHIN_VIEW (view)), new_level);
        }

        g_free (uri);
    } else {
        nolphin_file_set_integer_metadata (file,
                                        NOLPHIN_METADATA_KEY_LIST_VIEW_ZOOM_LEVEL,
                                        get_default_zoom_level (),
                                        new_level);
    }

	/* Select correctly scaled icons. */
	column = nolphin_list_model_get_column_id_from_zoom_level (new_level);
	gtk_tree_view_column_set_attributes (view->details->file_name_column,
					     GTK_CELL_RENDERER (view->details->pixbuf_cell),
					     "surface", column,
					     NULL);

	/* Scale text. */
	nolphin_list_view_scale_font_size (view, new_level);

	/* Make all rows the same size. */
	icon_size = nolphin_get_list_icon_size_for_zoom_level (new_level);
	gtk_cell_renderer_set_fixed_size (GTK_CELL_RENDERER (view->details->pixbuf_cell),
					  -1, icon_size);

	nolphin_view_update_menus (NOLPHIN_VIEW (view));

	/* FIXME: https://bugzilla.gnome.org/show_bug.cgi?id=641518 */
	gtk_tree_view_columns_autosize (view->details->tree_view);
}

static void
nolphin_list_view_bump_zoom_level (NolphinView *view, int zoom_increment)
{
	NolphinListView *list_view;
	gint new_level;

	g_return_if_fail (NOLPHIN_IS_LIST_VIEW (view));

	list_view = NOLPHIN_LIST_VIEW (view);
	new_level = list_view->details->zoom_level + zoom_increment;

	if (new_level >= NOLPHIN_ZOOM_LEVEL_SMALLEST &&
	    new_level <= NOLPHIN_ZOOM_LEVEL_LARGEST) {
		nolphin_list_view_set_zoom_level (list_view, new_level, FALSE);
	}
}

static NolphinZoomLevel
nolphin_list_view_get_zoom_level (NolphinView *view)
{
	NolphinListView *list_view;

	g_return_val_if_fail (NOLPHIN_IS_LIST_VIEW (view), NOLPHIN_ZOOM_LEVEL_STANDARD);

	list_view = NOLPHIN_LIST_VIEW (view);

	return list_view->details->zoom_level;
}

static void
nolphin_list_view_zoom_to_level (NolphinView *view,
				  NolphinZoomLevel zoom_level)
{
	NolphinListView *list_view;

	g_return_if_fail (NOLPHIN_IS_LIST_VIEW (view));

	list_view = NOLPHIN_LIST_VIEW (view);

	nolphin_list_view_set_zoom_level (list_view, zoom_level, FALSE);
}

static void
nolphin_list_view_restore_default_zoom_level (NolphinView *view)
{
	NolphinListView *list_view;

	g_return_if_fail (NOLPHIN_IS_LIST_VIEW (view));

	list_view = NOLPHIN_LIST_VIEW (view);

	nolphin_list_view_set_zoom_level (list_view, get_default_zoom_level (), FALSE);
}

static NolphinZoomLevel
nolphin_list_view_get_default_zoom_level (NolphinView *view)
{
    g_return_val_if_fail (NOLPHIN_IS_LIST_VIEW (view), NOLPHIN_ZOOM_LEVEL_NULL);

    return get_default_zoom_level();
}

static gboolean
nolphin_list_view_can_zoom_in (NolphinView *view)
{
	g_return_val_if_fail (NOLPHIN_IS_LIST_VIEW (view), FALSE);

	return NOLPHIN_LIST_VIEW (view)->details->zoom_level	< NOLPHIN_ZOOM_LEVEL_LARGEST;
}

static gboolean
nolphin_list_view_can_zoom_out (NolphinView *view)
{
	g_return_val_if_fail (NOLPHIN_IS_LIST_VIEW (view), FALSE);

	return NOLPHIN_LIST_VIEW (view)->details->zoom_level > NOLPHIN_ZOOM_LEVEL_SMALLEST;
}

static void
nolphin_list_view_start_renaming_file (NolphinView *view,
					NolphinFile *file,
					gboolean select_all)
{
	NolphinListView *list_view;
	GtkTreeIter iter;
	GtkTreePath *path;

	list_view = NOLPHIN_LIST_VIEW (view);

	/* Select all if we are in renaming mode already */
	if (list_view->details->file_name_column && list_view->details->editable_widget) {
		gtk_editable_select_region (GTK_EDITABLE (list_view->details->editable_widget),
					    0,
					    -1);
		return;
	}

	if (!nolphin_list_model_get_first_iter_for_file (list_view->details->model, file, &iter)) {
		return;
	}

	/* call parent class to make sure the right icon is selected */
	NOLPHIN_VIEW_CLASS (nolphin_list_view_parent_class)->start_renaming_file (view, file, select_all);

	/* Freeze updates to the view to prevent losing rename focus when the tree view updates */
	nolphin_view_freeze_updates (NOLPHIN_VIEW (view));

	path = gtk_tree_model_get_path (GTK_TREE_MODEL (list_view->details->model), &iter);

	/* Make filename-cells editable. */
	g_object_set (G_OBJECT (list_view->details->file_name_cell),
		      "editable", TRUE,
		      NULL);

	gtk_tree_view_scroll_to_cell (list_view->details->tree_view,
				      NULL,
				      list_view->details->file_name_column,
				      TRUE, 0.0, 0.0);
	gtk_tree_view_set_cursor_on_cell (list_view->details->tree_view,
					  path,
					  list_view->details->file_name_column,
					  GTK_CELL_RENDERER (list_view->details->file_name_cell),
					  TRUE);

	/* set cursor also triggers editing-started, where we save the editable widget */
	if (list_view->details->editable_widget != NULL) {
        int start_offset = 0;
        int end_offset = -1;

        if (!select_all) {
            eel_filename_get_rename_region (list_view->details->original_name,
                           &start_offset, &end_offset);
        }

		gtk_editable_select_region (GTK_EDITABLE (list_view->details->editable_widget),
					    start_offset, end_offset);
	}

	gtk_tree_path_free (path);
}

static void
nolphin_list_view_click_to_rename_mode_changed (NolphinView *directory_view)
{
    NolphinListView *view;

    g_assert (NOLPHIN_IS_LIST_VIEW (directory_view));

    view = NOLPHIN_LIST_VIEW (directory_view);

    view->details->click_to_rename = g_settings_get_boolean (nolphin_preferences,
                                                                  NOLPHIN_PREFERENCES_CLICK_TO_RENAME);
}

static void
nolphin_list_view_click_policy_changed (NolphinView *directory_view)
{
	GdkWindow *win;
	GdkDisplay *display;
	NolphinListView *view;
	GtkTreeIter iter;
	GtkTreeView *tree;

	view = NOLPHIN_LIST_VIEW (directory_view);

    click_policy = g_settings_get_enum (nolphin_preferences,
                                        NOLPHIN_PREFERENCES_CLICK_POLICY);

	/* ensure that we unset the hand cursor and refresh underlined rows */
	if (click_policy == NOLPHIN_CLICK_POLICY_DOUBLE) {
		if (view->details->hover_path != NULL) {
			if (gtk_tree_model_get_iter (GTK_TREE_MODEL (view->details->model),
						     &iter, view->details->hover_path)) {
				gtk_tree_model_row_changed (GTK_TREE_MODEL (view->details->model),
							    view->details->hover_path, &iter);
			}

			gtk_tree_path_free (view->details->hover_path);
			view->details->hover_path = NULL;
		}

		tree = view->details->tree_view;
		if (gtk_widget_get_realized (GTK_WIDGET (tree))) {
			win = gtk_widget_get_window (GTK_WIDGET (tree));
			gdk_window_set_cursor (win, NULL);

			display = gtk_widget_get_display (GTK_WIDGET (view));
			if (display != NULL) {
				gdk_display_flush (display);
			}
		}

		g_clear_object (&hand_cursor);
	} else if (click_policy == NOLPHIN_CLICK_POLICY_SINGLE) {
		if (hand_cursor == NULL) {
			hand_cursor = gdk_cursor_new(GDK_HAND2);
		}
	}
}

static void
default_sort_order_changed_callback (gpointer callback_data)
{
	NolphinListView *list_view;

	list_view = NOLPHIN_LIST_VIEW (callback_data);

	set_sort_order_from_metadata_and_preferences (list_view);
}

static void
default_zoom_level_changed_callback (gpointer callback_data)
{
	NolphinListView *list_view;

	list_view = NOLPHIN_LIST_VIEW (callback_data);

	set_zoom_level_from_metadata_and_preferences (list_view);
}

static void
default_visible_columns_changed_callback (gpointer callback_data)
{
	NolphinListView *list_view;

	list_view = NOLPHIN_LIST_VIEW (callback_data);

	set_columns_settings_from_metadata_and_preferences (list_view);
}

static void
default_column_order_changed_callback (gpointer callback_data)
{
	NolphinListView *list_view;

	list_view = NOLPHIN_LIST_VIEW (callback_data);

	set_columns_settings_from_metadata_and_preferences (list_view);
}

static void
nolphin_list_view_sort_directories_first_changed (NolphinView *view)
{
	NolphinListView *list_view;

	list_view = NOLPHIN_LIST_VIEW (view);

	nolphin_list_model_set_should_sort_directories_first (list_view->details->model,
							 nolphin_view_should_sort_directories_first (view));
}

static void
nolphin_list_view_sort_favorites_first_changed (NolphinView *view)
{
	NolphinListView *list_view;

	list_view = NOLPHIN_LIST_VIEW (view);

	nolphin_list_model_set_should_sort_favorites_first (list_view->details->model,
							 nolphin_view_should_sort_favorites_first (view));
}

static int
nolphin_list_view_compare_files (NolphinView *view, NolphinFile *file1, NolphinFile *file2)
{
	NolphinListView *list_view;

	list_view = NOLPHIN_LIST_VIEW (view);
	return nolphin_list_model_compare_func (list_view->details->model, file1, file2);
}

static gboolean
nolphin_list_view_using_manual_layout (NolphinView *view)
{
	g_return_val_if_fail (NOLPHIN_IS_LIST_VIEW (view), FALSE);

	return FALSE;
}

static void
nolphin_list_view_dispose (GObject *object)
{
	NolphinListView *list_view;

	list_view = NOLPHIN_LIST_VIEW (object);

    g_signal_handlers_disconnect_by_func (gtk_settings_get_default (), update_date_fonts, list_view);
    g_signal_handlers_disconnect_by_func (nolphin_preferences, update_date_fonts, list_view);

	if (list_view->details->model) {
		stop_cell_editing (list_view);
		g_object_unref (list_view->details->model);
		list_view->details->model = NULL;
	}

	if (list_view->details->drag_dest) {
		g_object_unref (list_view->details->drag_dest);
		list_view->details->drag_dest = NULL;
	}

	if (list_view->details->renaming_file_activate_timeout != 0) {
		g_source_remove (list_view->details->renaming_file_activate_timeout);
		list_view->details->renaming_file_activate_timeout = 0;
	}

    if (list_view->details->update_visible_icons_id > 0) {
        g_source_remove (list_view->details->update_visible_icons_id);
        list_view->details->update_visible_icons_id = 0;
    }

	if (list_view->details->clipboard_handler_id != 0) {
		g_signal_handler_disconnect (nolphin_clipboard_monitor_get (),
		                             list_view->details->clipboard_handler_id);
		list_view->details->clipboard_handler_id = 0;
	}

    G_OBJECT_CLASS (nolphin_list_view_parent_class)->dispose (object);
}

static void
nolphin_list_view_finalize (GObject *object)
{
	NolphinListView *list_view;

	list_view = NOLPHIN_LIST_VIEW (object);

	g_free (list_view->details->original_name);
	list_view->details->original_name = NULL;

	if (list_view->details->double_click_path[0]) {
		gtk_tree_path_free (list_view->details->double_click_path[0]);
	}
	if (list_view->details->double_click_path[1]) {
		gtk_tree_path_free (list_view->details->double_click_path[1]);
	}
	if (list_view->details->new_selection_path) {
		gtk_tree_path_free (list_view->details->new_selection_path);
	}

	g_list_free (list_view->details->cells);
	g_hash_table_destroy (list_view->details->columns);

	if (list_view->details->hover_path != NULL) {
		gtk_tree_path_free (list_view->details->hover_path);
	}

	if (list_view->details->column_editor != NULL) {
		gtk_widget_destroy (list_view->details->column_editor);
	}

	g_free (list_view->details);

	g_signal_handlers_disconnect_by_func (nolphin_preferences,
					      default_sort_order_changed_callback,
					      list_view);
	g_signal_handlers_disconnect_by_func (nolphin_list_view_preferences,
					      default_zoom_level_changed_callback,
					      list_view);
	g_signal_handlers_disconnect_by_func (nolphin_list_view_preferences,
					      default_visible_columns_changed_callback,
					      list_view);
	g_signal_handlers_disconnect_by_func (nolphin_list_view_preferences,
					      default_column_order_changed_callback,
					      list_view);
    g_signal_handlers_disconnect_by_func (nolphin_list_view_preferences,
                                          expanders_enabled_changed_cb,
                                          list_view);
    g_signal_handlers_disconnect_by_func (nolphin_preferences,
                                          tooltip_prefs_changed_callback,
                                          list_view);

	G_OBJECT_CLASS (nolphin_list_view_parent_class)->finalize (object);
}

static char *
nolphin_list_view_get_first_visible_file (NolphinView *view)
{
	NolphinFile *file;
	GtkTreePath *path;
	GtkTreeIter iter;
	NolphinListView *list_view;

	list_view = NOLPHIN_LIST_VIEW (view);

	if (gtk_tree_view_get_path_at_pos (list_view->details->tree_view,
					   0, 0,
					   &path, NULL, NULL, NULL)) {
		gtk_tree_model_get_iter (GTK_TREE_MODEL (list_view->details->model),
					 &iter, path);

		gtk_tree_path_free (path);

		gtk_tree_model_get (GTK_TREE_MODEL (list_view->details->model),
				    &iter,
				    NOLPHIN_LIST_MODEL_FILE_COLUMN, &file,
				    -1);
		if (file) {
			char *uri;

			uri = nolphin_file_get_uri (file);

			nolphin_file_unref (file);

			return uri;
		}
	}

	return NULL;
}

static void
nolphin_list_view_scroll_to_file (NolphinListView *view,
				   NolphinFile *file)
{
	GtkTreePath *path;
	GtkTreeIter iter;

	if (!nolphin_list_model_get_first_iter_for_file (view->details->model, file, &iter)) {
		return;
	}

	path = gtk_tree_model_get_path (GTK_TREE_MODEL (view->details->model), &iter);

	gtk_tree_view_scroll_to_cell (view->details->tree_view,
				      path, NULL,
				      TRUE, 0.0, 0.0);

	gtk_tree_path_free (path);
}

static void
list_view_scroll_to_file (NolphinView *view,
			  const char *uri)
{
	NolphinFile *file;

	if (uri != NULL) {
		/* Only if existing, since we don't want to add the file to
		   the directory if it has been removed since then */
		file = nolphin_file_get_existing_by_uri (uri);
		if (file != NULL) {
			nolphin_list_view_scroll_to_file (NOLPHIN_LIST_VIEW (view), file);
			nolphin_file_unref (file);
		}
	}
}

static void
list_view_notify_clipboard_info (NolphinClipboardMonitor *monitor,
                                 NolphinClipboardInfo *info,
                                 NolphinListView *view)
{
	/* this could be called as a result of _end_loading() being
	 * called after _dispose(), where the model is cleared.
	 */
	if (view->details->model == NULL) {
		return;
	}

	if (info != NULL && info->cut) {
		nolphin_list_model_set_highlight_for_files (view->details->model, info->files);
	} else {
		nolphin_list_model_set_highlight_for_files (view->details->model, NULL);
	}
}

static void
nolphin_list_view_end_loading (NolphinView *view,
				gboolean all_files_seen)
{
	NolphinClipboardMonitor *monitor;
	NolphinClipboardInfo *info;

    set_ok_to_load_deferred_attrs (NOLPHIN_LIST_VIEW (view), TRUE);

	monitor = nolphin_clipboard_monitor_get ();
	info = nolphin_clipboard_monitor_get_clipboard_info (monitor);

	list_view_notify_clipboard_info (monitor, info, NOLPHIN_LIST_VIEW (view));
}

static const char *
nolphin_list_view_get_id (NolphinView *view)
{
	return NOLPHIN_LIST_VIEW_ID;
}

static const gchar *
nolphin_list_view_get_sort_attribute (NolphinView *view)
{
	NolphinListView *list_view = NOLPHIN_LIST_VIEW (view);
	gint sort_column_id;
	GtkSortType order;
	GQuark attribute;

	if (!gtk_tree_sortable_get_sort_column_id (
			GTK_TREE_SORTABLE (list_view->details->model),
			&sort_column_id, &order)) {
		return NULL;
	}

	attribute = nolphin_list_model_get_attribute_from_sort_column_id (
		list_view->details->model, sort_column_id);

	return attribute != 0 ? g_quark_to_string (attribute) : NULL;
}

static void
nolphin_list_view_class_init (NolphinListViewClass *class)
{
	NolphinViewClass *nolphin_view_class;

	nolphin_view_class = NOLPHIN_VIEW_CLASS (class);

	G_OBJECT_CLASS (class)->dispose = nolphin_list_view_dispose;
	G_OBJECT_CLASS (class)->finalize = nolphin_list_view_finalize;

	nolphin_view_class->add_file = nolphin_list_view_add_file;
	nolphin_view_class->begin_loading = nolphin_list_view_begin_loading;
	nolphin_view_class->end_loading = nolphin_list_view_end_loading;
	nolphin_view_class->bump_zoom_level = nolphin_list_view_bump_zoom_level;
	nolphin_view_class->can_zoom_in = nolphin_list_view_can_zoom_in;
	nolphin_view_class->can_zoom_out = nolphin_list_view_can_zoom_out;
        nolphin_view_class->click_policy_changed = nolphin_list_view_click_policy_changed;
	nolphin_view_class->clear = nolphin_list_view_clear;
	nolphin_view_class->file_changed = nolphin_list_view_file_changed;
	nolphin_view_class->get_backing_uri = nolphin_list_view_get_backing_uri;
	nolphin_view_class->get_selection = nolphin_list_view_get_selection;
    nolphin_view_class->peek_selection = nolphin_list_view_peek_selection;
    nolphin_view_class->get_selection_count = nolphin_list_view_get_selection_count;
	nolphin_view_class->get_selection_for_file_transfer = nolphin_list_view_get_selection_for_file_transfer;
	nolphin_view_class->get_item_count = nolphin_list_view_get_item_count;
	nolphin_view_class->is_empty = nolphin_list_view_is_empty;
	nolphin_view_class->remove_file = nolphin_list_view_remove_file;
    nolphin_view_class->merge_menus = nolphin_list_view_merge_menus;
    nolphin_view_class->unmerge_menus = nolphin_list_view_unmerge_menus;
	nolphin_view_class->update_menus = nolphin_list_view_update_menus;
	nolphin_view_class->reset_to_defaults = nolphin_list_view_reset_to_defaults;
	nolphin_view_class->restore_default_zoom_level = nolphin_list_view_restore_default_zoom_level;
    nolphin_view_class->get_default_zoom_level = nolphin_list_view_get_default_zoom_level;
	nolphin_view_class->reveal_selection = nolphin_list_view_reveal_selection;
	nolphin_view_class->select_all = nolphin_list_view_select_all;
	nolphin_view_class->set_selection = nolphin_list_view_set_selection;
	nolphin_view_class->invert_selection = nolphin_list_view_invert_selection;
	nolphin_view_class->compare_files = nolphin_list_view_compare_files;
	nolphin_view_class->update_filter_text = nolphin_list_view_update_filter_text;
	nolphin_view_class->select_first = nolphin_list_view_select_first;
	nolphin_view_class->sort_directories_first_changed = nolphin_list_view_sort_directories_first_changed;
	nolphin_view_class->sort_favorites_first_changed = nolphin_list_view_sort_favorites_first_changed;
	nolphin_view_class->start_renaming_file = nolphin_list_view_start_renaming_file;
	nolphin_view_class->get_zoom_level = nolphin_list_view_get_zoom_level;
	nolphin_view_class->zoom_to_level = nolphin_list_view_zoom_to_level;
	nolphin_view_class->end_file_changes = nolphin_list_view_end_file_changes;
	nolphin_view_class->using_manual_layout = nolphin_list_view_using_manual_layout;
	nolphin_view_class->get_view_id = nolphin_list_view_get_id;
	nolphin_view_class->get_first_visible_file = nolphin_list_view_get_first_visible_file;
	nolphin_view_class->scroll_to_file = list_view_scroll_to_file;
    nolphin_view_class->click_to_rename_mode_changed = nolphin_list_view_click_to_rename_mode_changed;
	nolphin_view_class->get_sort_attribute = nolphin_list_view_get_sort_attribute;
}

static void
nolphin_list_view_init (NolphinListView *list_view)
{
	list_view->details = g_new0 (NolphinListViewDetails, 1);

    GtkStyleContext *context = gtk_widget_get_style_context (GTK_WIDGET (list_view));
    gtk_style_context_add_class (context, "view");

	create_and_set_up_tree_view (list_view);

	g_signal_connect_swapped (nolphin_preferences,
				  "changed::" NOLPHIN_PREFERENCES_DEFAULT_SORT_ORDER,
				  G_CALLBACK (default_sort_order_changed_callback),
				  list_view);
	g_signal_connect_swapped (nolphin_preferences,
				  "changed::" NOLPHIN_PREFERENCES_DEFAULT_SORT_IN_REVERSE_ORDER,
				  G_CALLBACK (default_sort_order_changed_callback),
				  list_view);
	g_signal_connect_swapped (nolphin_list_view_preferences,
				  "changed::" NOLPHIN_PREFERENCES_LIST_VIEW_DEFAULT_ZOOM_LEVEL,
				  G_CALLBACK (default_zoom_level_changed_callback),
				  list_view);
	g_signal_connect_swapped (nolphin_list_view_preferences,
				  "changed::" NOLPHIN_PREFERENCES_LIST_VIEW_DEFAULT_VISIBLE_COLUMNS,
				  G_CALLBACK (default_visible_columns_changed_callback),
				  list_view);
	g_signal_connect_swapped (nolphin_list_view_preferences,
				  "changed::" NOLPHIN_PREFERENCES_LIST_VIEW_DEFAULT_COLUMN_ORDER,
				  G_CALLBACK (default_column_order_changed_callback),
				  list_view);

    g_signal_connect_swapped (nolphin_preferences,
                              "changed::" NOLPHIN_PREFERENCES_TOOLTIPS_LIST_VIEW,
                              G_CALLBACK (tooltip_prefs_changed_callback),
                              list_view);

    g_signal_connect_swapped (nolphin_preferences,
                              "changed::" NOLPHIN_PREFERENCES_TOOLTIP_FILE_TYPE,
                              G_CALLBACK (tooltip_prefs_changed_callback),
                              list_view);

    g_signal_connect_swapped (nolphin_preferences,
                              "changed::" NOLPHIN_PREFERENCES_TOOLTIP_MOD_DATE,
                              G_CALLBACK (tooltip_prefs_changed_callback),
                              list_view);

    g_signal_connect_swapped (nolphin_preferences,
                              "changed::" NOLPHIN_PREFERENCES_TOOLTIP_ACCESS_DATE,
                              G_CALLBACK (tooltip_prefs_changed_callback),
                              list_view);

    g_signal_connect_swapped (nolphin_preferences,
                              "changed::" NOLPHIN_PREFERENCES_TOOLTIP_FULL_PATH,
                              G_CALLBACK (tooltip_prefs_changed_callback),
                              list_view);

    tooltip_prefs_changed_callback (list_view);

	nolphin_list_view_click_policy_changed (NOLPHIN_VIEW (list_view));
    nolphin_list_view_click_to_rename_mode_changed (NOLPHIN_VIEW (list_view));

	nolphin_list_view_sort_directories_first_changed (NOLPHIN_VIEW (list_view));
	nolphin_list_view_sort_favorites_first_changed (NOLPHIN_VIEW (list_view));

    list_view->details->current_selection_count = -1;

	/* ensure that the zoom level is always set in begin_loading */
	list_view->details->zoom_level = NOLPHIN_ZOOM_LEVEL_SMALLEST - 1;

	list_view->details->hover_path = NULL;
	list_view->details->clipboard_handler_id =
		g_signal_connect (nolphin_clipboard_monitor_get (),
		                  "clipboard_info",
		                  G_CALLBACK (list_view_notify_clipboard_info), list_view);

    GtkSettings *gtksettings = gtk_settings_get_default ();
    g_object_get (gtksettings,
                  "gtk-overlay-scrolling", &list_view->details->overlay_scrolling,
                  NULL);
}

static NolphinView *
nolphin_list_view_create (NolphinWindowSlot *slot)
{
	NolphinListView *view;

	view = g_object_new (NOLPHIN_TYPE_LIST_VIEW,
			     "window-slot", slot,
			     NULL);
	return NOLPHIN_VIEW (view);
}

static gboolean
nolphin_list_view_supports_uri (const char *uri,
				 GFileType file_type,
				 const char *mime_type)
{
	if (file_type == G_FILE_TYPE_DIRECTORY) {
		return TRUE;
	}
	if (g_str_has_prefix (uri, "trash:")) {
		return TRUE;
	}
    if (g_str_has_prefix (uri, "recent:")) {
        return TRUE;
    }
    if (g_str_has_prefix (uri, "favorites:")) {
        return TRUE;
    }
	if (g_str_has_prefix (uri, EEL_SEARCH_URI)) {
		return TRUE;
	}

	return FALSE;
}

static NolphinViewInfo nolphin_list_view = {
	(char *)NOLPHIN_LIST_VIEW_ID,
	/* translators: this is used in the view selection dropdown
	 * of navigation windows and in the preferences dialog */
	(char *)N_("Listenansicht"),
	/* translators: this is used in the view menu */
	(char *)N_("_Liste"),
	(char *)N_("Fehler in der Listenansicht."),
	(char *)N_("Die Listenansicht stieß beim Starten auf einen Fehler."),
	(char *)N_("Diesen Ort mit der Listenansicht anzeigen"),
	nolphin_list_view_create,
	nolphin_list_view_supports_uri
};

void
nolphin_list_view_register (void)
{
	nolphin_list_view.view_combo_label = _(nolphin_list_view.view_combo_label);
	nolphin_list_view.view_menu_label_with_mnemonic = _(nolphin_list_view.view_menu_label_with_mnemonic);
	nolphin_list_view.error_label = _(nolphin_list_view.error_label);
	nolphin_list_view.startup_error_label = _(nolphin_list_view.startup_error_label);
	nolphin_list_view.display_location_label = _(nolphin_list_view.display_location_label);

	nolphin_view_factory_register (&nolphin_list_view);
}

GtkTreeView*
nolphin_list_view_get_tree_view (NolphinListView *list_view)
{
	return list_view->details->tree_view;
}

static void
nolphin_list_view_update_filter_text (NolphinView   *view,
                                   const char *filter_text)
{
    NolphinListView *list_view = NOLPHIN_LIST_VIEW (view);

    nolphin_list_model_set_filter_active (list_view->details->model,
                                       filter_text != NULL && filter_text[0] != '\0');
}

static void
nolphin_list_view_select_first (NolphinView *view)
{
    NolphinListView *list_view = NOLPHIN_LIST_VIEW (view);
    GtkTreeModel *model = GTK_TREE_MODEL (list_view->details->model);
    GtkTreeIter iter;
    GtkTreePath *path;

    if (!gtk_tree_model_get_iter_first (model, &iter)) {
        return;
    }

    path = gtk_tree_model_get_path (model, &iter);
    gtk_tree_view_set_cursor (list_view->details->tree_view, path, NULL, FALSE);
    gtk_tree_path_free (path);
}
