/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* fm-list-model.h - a GtkTreeModel for file lists.

   Copyright (C) 2001, 2002 Anders Carlsson
   Copyright (C) 2003, Soeren Sandmann
   Copyright (C) 2004, Novell, Inc.

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

   Authors: Anders Carlsson <andersca@gnu.org>, Soeren Sandmann (sandmann@daimi.au.dk), Dave Camp <dave@ximian.com>
*/

#include <config.h>

#include "nolphin-list-model.h"

#include <string.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <cairo-gobject.h>

#include <libegg/eggtreemultidnd.h>
#include <eel/eel-graphic-effects.h>
#include <libnolphin-private/nolphin-dnd.h>
#include <libnolphin-private/nolphin-file-utilities.h>

enum {
	SUBDIRECTORY_UNLOADED,
    GET_ICON_SCALE,
    GET_FILTER_MATCH,
	LAST_SIGNAL
};

static GQuark attribute_name_q,
	attribute_modification_date_q,
	attribute_date_modified_q,
    attribute_search_result_count_q;

/* msec delay after Loading... dummy row turns into (empty) */
#define LOADING_TO_EMPTY_DELAY 100

static guint list_model_signals[LAST_SIGNAL] = { 0 };

static int nolphin_list_model_file_entry_compare_func (gconstpointer a,
							gconstpointer b,
							gpointer      user_data);
static void nolphin_list_model_tree_model_init (GtkTreeModelIface *iface);
static void nolphin_list_model_sortable_init (GtkTreeSortableIface *iface);
static void nolphin_list_model_multi_drag_source_init (EggTreeMultiDragSourceIface *iface);

struct NolphinListModelDetails {
	GSequence *files;
	GHashTable *directory_reverse_map; /* map from directory to GSequenceIter's */
	GHashTable *top_reverse_map;	   /* map from files in top dir to GSequenceIter's */

    NolphinDirectory *view_dir;

	int stamp;

	GQuark sort_attribute;
	GtkSortType order;

	gboolean sort_directories_first;
	gboolean sort_favorites_first;

	GtkTreeView *drag_view;
	int drag_begin_x;
	int drag_begin_y;

	GPtrArray *columns;

	GList *highlight_files;
    gboolean temp_unsorted;
    gboolean expansion_enabled;

    gboolean filter_active;
};

typedef struct {
	NolphinListModel *model;

	GList *path_list;
} DragDataGetInfo;

typedef struct FileEntry FileEntry;

struct FileEntry {
	NolphinFile *file;
	GHashTable *reverse_map;	/* map from files to GSequenceIter's */
	NolphinDirectory *subdirectory;
	FileEntry *parent;
	GSequence *files;
	GSequenceIter *ptr;
	guint loaded : 1;
    guint expanding : 1;
    guint ok_to_show_thumb : 1;
};

G_DEFINE_TYPE_WITH_CODE (NolphinListModel, nolphin_list_model, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_MODEL,
						nolphin_list_model_tree_model_init)
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_SORTABLE,
						nolphin_list_model_sortable_init)
			 G_IMPLEMENT_INTERFACE (EGG_TYPE_TREE_MULTI_DRAG_SOURCE,
						nolphin_list_model_multi_drag_source_init));

static const GtkTargetEntry drag_types [] = {
	{ (char *)NOLPHIN_ICON_DND_GNOME_ICON_LIST_TYPE, 0, NOLPHIN_ICON_DND_GNOME_ICON_LIST },
	{ (char *)NOLPHIN_ICON_DND_URI_LIST_TYPE, 0, NOLPHIN_ICON_DND_URI_LIST },
};

static GtkTargetList *drag_target_list = NULL;

static void
file_entry_free (FileEntry *file_entry)
{
	nolphin_file_unref (file_entry->file);
	if (file_entry->reverse_map) {
		g_hash_table_destroy (file_entry->reverse_map);
		file_entry->reverse_map = NULL;
	}
	if (file_entry->subdirectory != NULL) {
		nolphin_directory_unref (file_entry->subdirectory);
	}
	if (file_entry->files != NULL) {
		g_sequence_free (file_entry->files);
	}
	g_free (file_entry);
}

static GtkTreeModelFlags
nolphin_list_model_get_flags (GtkTreeModel *tree_model)
{
	return GTK_TREE_MODEL_ITERS_PERSIST;
}

static int
nolphin_list_model_get_n_columns (GtkTreeModel *tree_model)
{
	return NOLPHIN_LIST_MODEL_NUM_COLUMNS + NOLPHIN_LIST_MODEL (tree_model)->details->columns->len;
}

static GType
nolphin_list_model_get_column_type (GtkTreeModel *tree_model, int index)
{
	switch (index) {
	case NOLPHIN_LIST_MODEL_FILE_COLUMN:
		return NOLPHIN_TYPE_FILE;
	case NOLPHIN_LIST_MODEL_SUBDIRECTORY_COLUMN:
		return NOLPHIN_TYPE_DIRECTORY;
	case NOLPHIN_LIST_MODEL_SMALLEST_ICON_COLUMN:
	case NOLPHIN_LIST_MODEL_SMALLER_ICON_COLUMN:
	case NOLPHIN_LIST_MODEL_SMALL_ICON_COLUMN:
	case NOLPHIN_LIST_MODEL_STANDARD_ICON_COLUMN:
	case NOLPHIN_LIST_MODEL_LARGE_ICON_COLUMN:
	case NOLPHIN_LIST_MODEL_LARGER_ICON_COLUMN:
	case NOLPHIN_LIST_MODEL_LARGEST_ICON_COLUMN:
		return CAIRO_GOBJECT_TYPE_SURFACE;
    case NOLPHIN_LIST_MODEL_FILE_NAME_IS_EDITABLE_COLUMN:
		return G_TYPE_BOOLEAN;
    case NOLPHIN_LIST_MODEL_TEXT_WEIGHT_COLUMN:
        return G_TYPE_INT;
	default:
		if (index < (int)(NOLPHIN_LIST_MODEL_NUM_COLUMNS + NOLPHIN_LIST_MODEL (tree_model)->details->columns->len)) {
			return G_TYPE_STRING;
		} else {
			return G_TYPE_INVALID;
		}
	}
}

static void
nolphin_list_model_ptr_to_iter (NolphinListModel *model, GSequenceIter *ptr, GtkTreeIter *iter)
{
	g_assert (!g_sequence_iter_is_end (ptr));
	if (iter != NULL) {
		iter->stamp = model->details->stamp;
		iter->user_data = ptr;
	}
}

static gboolean
nolphin_list_model_get_iter (GtkTreeModel *tree_model, GtkTreeIter *iter, GtkTreePath *path)
{
	NolphinListModel *model;
	GSequence *files;
	GSequenceIter *ptr;
	FileEntry *file_entry;
	int i, d;

	model = (NolphinListModel *)tree_model;
	ptr = NULL;

	files = model->details->files;
	for (d = 0; d < gtk_tree_path_get_depth (path); d++) {
		i = gtk_tree_path_get_indices (path)[d];

		if (files == NULL || i >= g_sequence_get_length (files)) {
			return FALSE;
		}

		ptr = g_sequence_get_iter_at_pos (files, i);
		file_entry = g_sequence_get (ptr);
		files = file_entry->files;
	}

	nolphin_list_model_ptr_to_iter (model, ptr, iter);

	return TRUE;
}

static GtkTreePath *
nolphin_list_model_get_path (GtkTreeModel *tree_model, GtkTreeIter *iter)
{
	GtkTreePath *path;
	NolphinListModel *model;
	GSequenceIter *ptr;
	FileEntry *file_entry;


	model = (NolphinListModel *)tree_model;

	g_return_val_if_fail (iter->stamp == model->details->stamp, NULL);

	if (g_sequence_iter_is_end (iter->user_data)) {
		/* FIXME is this right? */
		return NULL;
	}

	path = gtk_tree_path_new ();
	ptr = iter->user_data;
	while (ptr != NULL) {
		gtk_tree_path_prepend_index (path, g_sequence_iter_get_position (ptr));
		file_entry = g_sequence_get (ptr);
		if (file_entry->parent != NULL) {
			ptr = file_entry->parent->ptr;
		} else {
			ptr = NULL;
		}
	}

	return path;
}

static gint
nolphin_list_model_get_filter_match (NolphinListModel *model, NolphinFile *file)
{
    gint retval = -1;

    g_signal_emit (model, list_model_signals[GET_FILTER_MATCH], 0,
                   file, &retval);

    return retval;
}

static gint
nolphin_list_model_get_icon_scale (NolphinListModel *model)
{
   gint retval = -1;

   g_signal_emit (model, list_model_signals[GET_ICON_SCALE], 0,
              &retval);

   if (retval == -1) {
       retval = gdk_screen_get_monitor_scale_factor (gdk_screen_get_default (), 0);
   }

   return retval;
}

static void
nolphin_list_model_get_value (GtkTreeModel *tree_model, GtkTreeIter *iter, int column, GValue *value)
{
	NolphinListModel *model;
    NolphinFile *parent_file;
	FileEntry *file_entry;
	NolphinFile *file;
	char *str;

	model = (NolphinListModel *)tree_model;

	g_return_if_fail (model->details->stamp == iter->stamp);
	g_return_if_fail (!g_sequence_iter_is_end (iter->user_data));

	file_entry = g_sequence_get (iter->user_data);
	file = file_entry->file;
    parent_file = file_entry->parent ? file_entry->parent->file : NULL;

	switch (column) {
	case NOLPHIN_LIST_MODEL_FILE_COLUMN:
		g_value_init (value, NOLPHIN_TYPE_FILE);

		g_value_set_object (value, file);
		break;
	case NOLPHIN_LIST_MODEL_SUBDIRECTORY_COLUMN:
		g_value_init (value, NOLPHIN_TYPE_DIRECTORY);

		g_value_set_object (value, file_entry->subdirectory);
		break;
	case NOLPHIN_LIST_MODEL_SMALLEST_ICON_COLUMN:
	case NOLPHIN_LIST_MODEL_SMALLER_ICON_COLUMN:
	case NOLPHIN_LIST_MODEL_SMALL_ICON_COLUMN:
	case NOLPHIN_LIST_MODEL_STANDARD_ICON_COLUMN:
	case NOLPHIN_LIST_MODEL_LARGE_ICON_COLUMN:
	case NOLPHIN_LIST_MODEL_LARGER_ICON_COLUMN:
	case NOLPHIN_LIST_MODEL_LARGEST_ICON_COLUMN:
		g_value_init (value, CAIRO_GOBJECT_TYPE_SURFACE);

		if (file != NULL) {
            NolphinFileIconFlags flags;
            cairo_surface_t *surface;
            int icon_size, icon_scale;
            NolphinZoomLevel zoom_level;
            GdkPixbuf *icon, *rendered_icon;
            NolphinIconInfo *icon_info;
            GList *emblem_icons, *l;

			zoom_level = nolphin_list_model_get_zoom_level_from_column_id (column);
			icon_size = nolphin_get_list_icon_size_for_zoom_level (zoom_level);
            icon_scale = nolphin_list_model_get_icon_scale (model);

			flags = NOLPHIN_FILE_ICON_FLAGS_FORCE_THUMBNAIL_SIZE |
				NOLPHIN_FILE_ICON_FLAGS_USE_MOUNT_ICON_AS_EMBLEM;

            if (file_entry->ok_to_show_thumb) {
                flags |= NOLPHIN_FILE_ICON_FLAGS_USE_THUMBNAILS;
            }

			if (model->details->drag_view != NULL) {
				GtkTreePath *path_a, *path_b;

				gtk_tree_view_get_drag_dest_row (model->details->drag_view,
								 &path_a,
								 NULL);
				if (path_a != NULL) {
					path_b = gtk_tree_model_get_path (tree_model, iter);

					if (gtk_tree_path_compare (path_a, path_b) == 0) {
						flags |= NOLPHIN_FILE_ICON_FLAGS_FOR_DRAG_ACCEPT;
					}

					gtk_tree_path_free (path_a);
					gtk_tree_path_free (path_b);
				}
			}

            icon_info = nolphin_file_get_icon (file, icon_size, 0, icon_scale, flags);
            emblem_icons = nolphin_file_get_emblem_icons (file, parent_file);

            if (emblem_icons) {
                GdkPixbuf *initial_pixbuf;
                GIcon *gicon, *emblemed_icon, *emblem_icon;
                GEmblem *emblem;
                gint w, h, s;
                gboolean bad_ratio;

                initial_pixbuf = nolphin_icon_info_get_pixbuf_at_size (icon_info, icon_size * icon_scale);

                w = gdk_pixbuf_get_width (initial_pixbuf);
                h = gdk_pixbuf_get_height (initial_pixbuf);

                s = MAX (w, h);
                if (s < icon_size)
                    icon_size = s;

                bad_ratio = (int)(nolphin_icon_get_emblem_size_for_icon_size (icon_size) * icon_scale) > (int)(w * 0.75) ||
                            (int)(nolphin_icon_get_emblem_size_for_icon_size (icon_size) * icon_scale) > (int)(h * 0.75);

                gicon = G_ICON (initial_pixbuf);

                /* pick only the first emblem we can render for the list view */
                for (l = emblem_icons; !bad_ratio && l != NULL; l = l->next) {
                    emblem_icon = l->data;
                    emblem = g_emblem_new (emblem_icon);
                    emblemed_icon = g_emblemed_icon_new (gicon, emblem);

                    g_object_unref (gicon);
                    g_object_unref (emblem);
                    gicon = emblemed_icon;

                    break;
                }

                nolphin_icon_info_clear (&icon_info);
                icon_info = nolphin_icon_info_lookup (gicon, icon_size, icon_scale);

                g_list_free_full (emblem_icons, g_object_unref);
                g_object_unref (gicon);
            }

			icon = nolphin_icon_info_get_pixbuf_at_size (icon_info, icon_size * icon_scale);

			nolphin_icon_info_unref (icon_info);

			if (model->details->highlight_files != NULL &&
			    g_list_find_custom (model->details->highlight_files,
			                        file, (GCompareFunc) nolphin_file_compare_location))
			{
				rendered_icon = eel_create_spotlight_pixbuf (icon);

				if (rendered_icon != NULL) {
					g_object_unref (icon);
					icon = rendered_icon;
				}
			}

            surface = gdk_cairo_surface_create_from_pixbuf (icon, icon_scale, NULL);
            g_value_take_boxed (value, surface);
			g_object_unref (icon);
		}
		break;
	case NOLPHIN_LIST_MODEL_FILE_NAME_IS_EDITABLE_COLUMN:
		g_value_init (value, G_TYPE_BOOLEAN);

                g_value_set_boolean (value, file != NULL && nolphin_file_can_rename (file));
                break;
    case NOLPHIN_LIST_MODEL_TEXT_WEIGHT_COLUMN:
        g_value_init (value, G_TYPE_INT);

        if (file != NULL) {
            if (nolphin_file_is_unavailable_favorite (file)) {
                g_value_set_int (value, UNAVAILABLE_TEXT_WEIGHT);
            }
            else
            if (nolphin_file_get_pinning (file) && !nolphin_file_is_in_favorites (file)) {
                g_value_set_int (value, PINNED_TEXT_WEIGHT);
            }
            else
            {
                g_value_set_int (value, NORMAL_TEXT_WEIGHT);
            }
        }
        break;
    case NOLPHIN_LIST_MODEL_ICON_SHOWN:
        g_value_init (value, G_TYPE_BOOLEAN);

        g_value_set_boolean (value, file_entry->ok_to_show_thumb);
        file_entry->ok_to_show_thumb = TRUE;
        break;
 	default:
 		if (column >= NOLPHIN_LIST_MODEL_NUM_COLUMNS || column < (int)(NOLPHIN_LIST_MODEL_NUM_COLUMNS + model->details->columns->len)) {
			NolphinColumn *nolphin_column;
			GQuark attribute;
			nolphin_column = model->details->columns->pdata[column - NOLPHIN_LIST_MODEL_NUM_COLUMNS];

			g_value_init (value, G_TYPE_STRING);
			g_object_get (nolphin_column,
				      "attribute_q", &attribute,
				      NULL);
			if (file != NULL) {
                if (attribute == attribute_search_result_count_q) {
                    str = nolphin_file_get_search_result_count_as_string (file, (gpointer) model->details->view_dir);
                } else {
                    str = nolphin_file_get_string_attribute_with_default_q (file, attribute);
                }
				g_value_take_string (value, str);
			} else if (attribute == attribute_name_q) {
				if (file_entry->parent->loaded) {
					g_value_set_string (value, _("(Leer)"));
				} else {
					g_value_set_string (value, _("Ladevorgang …"));
				}
			}
		} else {
			g_assert_not_reached ();
		}
	}
}

static gboolean
nolphin_list_model_iter_next (GtkTreeModel *tree_model, GtkTreeIter *iter)
{
	NolphinListModel *model;

	model = (NolphinListModel *)tree_model;

	g_return_val_if_fail (model->details->stamp == iter->stamp, FALSE);

	iter->user_data = g_sequence_iter_next (iter->user_data);

	return !g_sequence_iter_is_end (iter->user_data);
}

static gboolean
nolphin_list_model_iter_children (GtkTreeModel *tree_model, GtkTreeIter *iter, GtkTreeIter *parent)
{
	NolphinListModel *model;
	GSequence *files;
	FileEntry *file_entry;

	model = (NolphinListModel *)tree_model;

	if (parent == NULL) {
		files = model->details->files;
	} else {
		file_entry = g_sequence_get (parent->user_data);
		files = file_entry->files;
	}

	if (files == NULL || g_sequence_get_length (files) == 0) {
		return FALSE;
	}

	iter->stamp = model->details->stamp;
	iter->user_data = g_sequence_get_begin_iter (files);

	return TRUE;
}

static gboolean
nolphin_list_model_iter_has_child (GtkTreeModel *tree_model, GtkTreeIter *iter)
{
	FileEntry *file_entry;

	if (iter == NULL) {
		return !nolphin_list_model_is_empty (NOLPHIN_LIST_MODEL (tree_model));
	}

	file_entry = g_sequence_get (iter->user_data);

	return (file_entry->files != NULL && g_sequence_get_length (file_entry->files) > 0);
}

static int
nolphin_list_model_iter_n_children (GtkTreeModel *tree_model, GtkTreeIter *iter)
{
	NolphinListModel *model;
	GSequence *files;
	FileEntry *file_entry;

	model = (NolphinListModel *)tree_model;

	if (iter == NULL) {
		files = model->details->files;
	} else {
		file_entry = g_sequence_get (iter->user_data);
		files = file_entry->files;
	}

	return g_sequence_get_length (files);
}

static gboolean
nolphin_list_model_iter_nth_child (GtkTreeModel *tree_model, GtkTreeIter *iter, GtkTreeIter *parent, int n)
{
	NolphinListModel *model;
	GSequenceIter *child;
	GSequence *files;
	FileEntry *file_entry;

	model = (NolphinListModel *)tree_model;

	if (parent != NULL) {
		file_entry = g_sequence_get (parent->user_data);
		files = file_entry->files;
	} else {
		files = model->details->files;
	}

	child = g_sequence_get_iter_at_pos (files, n);

	if (g_sequence_iter_is_end (child)) {
		return FALSE;
	}

	iter->stamp = model->details->stamp;
	iter->user_data = child;

	return TRUE;
}

static gboolean
nolphin_list_model_iter_parent (GtkTreeModel *tree_model, GtkTreeIter *iter, GtkTreeIter *child)
{
	NolphinListModel *model;
	FileEntry *file_entry;

	model = (NolphinListModel *)tree_model;

	file_entry = g_sequence_get (child->user_data);

	if (file_entry->parent == NULL) {
		return FALSE;
	}

	iter->stamp = model->details->stamp;
	iter->user_data = file_entry->parent->ptr;

	return TRUE;
}

static GSequenceIter *
lookup_file (NolphinListModel *model, NolphinFile *file,
	     NolphinDirectory *directory)
{
	FileEntry *file_entry;
	GSequenceIter *ptr, *parent_ptr;

	parent_ptr = NULL;
	if (directory) {
		parent_ptr = g_hash_table_lookup (model->details->directory_reverse_map,
						  directory);
	}

	if (parent_ptr) {
		file_entry = g_sequence_get (parent_ptr);
		ptr = g_hash_table_lookup (file_entry->reverse_map, file);
	} else {
		ptr = g_hash_table_lookup (model->details->top_reverse_map, file);
	}

	if (ptr) {
		g_assert (((FileEntry *)g_sequence_get (ptr))->file == file);
	}

	return ptr;
}


struct GetIters {
	NolphinListModel *model;
	NolphinFile *file;
	GList *iters;
};

static void
dir_to_iters (struct GetIters *data,
	      GHashTable *reverse_map)
{
	GSequenceIter *ptr;

	ptr = g_hash_table_lookup (reverse_map, data->file);
	if (ptr) {
		GtkTreeIter *iter;
		iter = g_new0 (GtkTreeIter, 1);
		nolphin_list_model_ptr_to_iter (data->model, ptr, iter);
		data->iters = g_list_prepend (data->iters, iter);
	}
}

static void
file_to_iter_cb (gpointer  key,
		 gpointer  value,
		 gpointer  user_data)
{
	struct GetIters *data;
	FileEntry *dir_file_entry;

	data = user_data;
	dir_file_entry = g_sequence_get ((GSequenceIter *)value);
	dir_to_iters (data, dir_file_entry->reverse_map);
}

GList *
nolphin_list_model_get_all_iters_for_file (NolphinListModel *model, NolphinFile *file)
{
	struct GetIters data;

	data.file = file;
	data.model = model;
	data.iters = NULL;

	dir_to_iters (&data, model->details->top_reverse_map);
	g_hash_table_foreach (model->details->directory_reverse_map,
			      file_to_iter_cb, &data);

	return g_list_reverse (data.iters);
}

gboolean
nolphin_list_model_get_first_iter_for_file (NolphinListModel          *model,
					     NolphinFile         *file,
					     GtkTreeIter          *iter)
{
	GList *list;
	gboolean res;

	res = FALSE;

	list = nolphin_list_model_get_all_iters_for_file (model, file);
	if (list != NULL) {
		res = TRUE;
		*iter = *(GtkTreeIter *)list->data;
	}
	g_list_free_full (list, g_free);

	return res;
}


gboolean
nolphin_list_model_get_tree_iter_from_file (NolphinListModel *model, NolphinFile *file,
					     NolphinDirectory *directory,
					     GtkTreeIter *iter)
{
	GSequenceIter *ptr;

	ptr = lookup_file (model, file, directory);
	if (!ptr) {
		return FALSE;
	}

	nolphin_list_model_ptr_to_iter (model, ptr, iter);

	return TRUE;
}

static int
nolphin_list_model_file_entry_compare_func (gconstpointer a,
					     gconstpointer b,
					     gpointer      user_data)
{
	FileEntry *file_entry1;
	FileEntry *file_entry2;
	NolphinListModel *model;
	int result;

	model = (NolphinListModel *)user_data;

	file_entry1 = (FileEntry *)a;
	file_entry2 = (FileEntry *)b;

	if (file_entry1->file != NULL && file_entry2->file != NULL) {
		if (model->details->filter_active) {
			gint match1 = nolphin_list_model_get_filter_match (model, file_entry1->file);
			gint match2 = nolphin_list_model_get_filter_match (model, file_entry2->file);

			if (match1 != match2) {
				return (match1 < match2) ? -1 : 1;
			}
		}

		result = nolphin_file_compare_for_sort_by_attribute_q (file_entry1->file, file_entry2->file,
									model->details->sort_attribute,
									model->details->sort_directories_first,
									model->details->sort_favorites_first,
									(model->details->order == GTK_SORT_DESCENDING),
                                    model->details->view_dir);
	} else if (file_entry1->file == NULL) {
		return -1;
	} else {
		return 1;
	}

	return result;
}

int
nolphin_list_model_compare_func (NolphinListModel *model,
				  NolphinFile *file1,
				  NolphinFile *file2)
{
	int result;

	result = nolphin_file_compare_for_sort_by_attribute_q (file1, file2,
								model->details->sort_attribute,
								model->details->sort_directories_first,
								model->details->sort_favorites_first,
								(model->details->order == GTK_SORT_DESCENDING),
                                model->details->view_dir);

	return result;
}

static void
nolphin_list_model_sort_file_entries (NolphinListModel *model, GSequence *files, GtkTreePath *path)
{
	GSequenceIter **old_order;
	GtkTreeIter iter;
	int *new_order;
	int length;
	int i;
	FileEntry *file_entry;
	gboolean has_iter;

	length = g_sequence_get_length (files);

	if (length <= 1) {
		return;
	}

	/* generate old order of GSequenceIter's */
	old_order = g_new (GSequenceIter *, length);
	for (i = 0; i < length; ++i) {
		GSequenceIter *ptr = g_sequence_get_iter_at_pos (files, i);

		file_entry = g_sequence_get (ptr);
		if (file_entry->files != NULL) {
			gtk_tree_path_append_index (path, i);
			nolphin_list_model_sort_file_entries (model, file_entry->files, path);
			gtk_tree_path_up (path);
		}

		old_order[i] = ptr;
	}

	/* sort */
	g_sequence_sort (files, nolphin_list_model_file_entry_compare_func, model);

	/* generate new order */
	new_order = g_new (int, length);
	/* Note: new_order[newpos] = oldpos */
	for (i = 0; i < length; ++i) {
		new_order[g_sequence_iter_get_position (old_order[i])] = i;
	}

	/* Let the world know about our new order */

	g_assert (new_order != NULL);

	has_iter = FALSE;
	if (gtk_tree_path_get_depth (path) != 0) {
		gboolean get_iter_result;
		has_iter = TRUE;
		get_iter_result = gtk_tree_model_get_iter (GTK_TREE_MODEL (model), &iter, path);
		g_assert (get_iter_result);
	}

	gtk_tree_model_rows_reordered (GTK_TREE_MODEL (model),
				       path, has_iter ? &iter : NULL, new_order);

	g_free (old_order);
	g_free (new_order);
}

static void
nolphin_list_model_sort (NolphinListModel *model)
{
	GtkTreePath *path;

	path = gtk_tree_path_new ();

	nolphin_list_model_sort_file_entries (model, model->details->files, path);

	gtk_tree_path_free (path);
}

static gboolean
nolphin_list_model_get_sort_column_id (GtkTreeSortable *sortable,
					gint            *sort_column_id,
					GtkSortType     *order)
{
	NolphinListModel *model;
	int id;

	model = (NolphinListModel *)sortable;

	id = nolphin_list_model_get_sort_column_id_from_attribute
		(model, model->details->sort_attribute);

	if (id == -1) {
		return FALSE;
	}

	if (sort_column_id != NULL) {
		*sort_column_id = id;
	}

	if (order != NULL) {
		*order = model->details->order;
	}

	return TRUE;
}

static void
nolphin_list_model_set_sort_column_id (GtkTreeSortable *sortable, gint sort_column_id, GtkSortType order)
{
	NolphinListModel *model;

	model = (NolphinListModel *)sortable;

	model->details->sort_attribute = nolphin_list_model_get_attribute_from_sort_column_id (model, sort_column_id);

	model->details->order = order;

	nolphin_list_model_sort (model);
	gtk_tree_sortable_sort_column_changed (sortable);
}

static gboolean
nolphin_list_model_has_default_sort_func (GtkTreeSortable *sortable)
{
	return FALSE;
}

static gboolean
nolphin_list_model_multi_row_draggable (EggTreeMultiDragSource *drag_source, GList *path_list)
{
	return TRUE;
}

static void
each_path_get_data_binder (NolphinDragEachSelectedItemDataGet data_get,
			   gpointer context,
			   gpointer data)
{
	DragDataGetInfo *info;
	GList *l;
	NolphinFile *file;
	GtkTreeRowReference *row;
	GtkTreePath *path;
	char *uri;
	char *path_str;
	GdkRectangle cell_area;
	GtkTreeViewColumn *column;

	info = context;

	g_return_if_fail (info->model->details->drag_view);

	column = gtk_tree_view_get_column (info->model->details->drag_view, 0);

	for (l = info->path_list; l != NULL; l = l->next) {
		row = l->data;

		path = gtk_tree_row_reference_get_path (row);
		file = nolphin_list_model_file_for_path (info->model, path);
		if (file) {
			gtk_tree_view_get_cell_area
				(info->model->details->drag_view,
				 path,
				 column,
				 &cell_area);

			uri = nolphin_file_get_local_uri (file);
			path_str = nolphin_file_get_path (file);

			(*data_get) (uri,
					 path_str,
				     0,
				     cell_area.y - info->model->details->drag_begin_y,
				     cell_area.width, cell_area.height,
				     data);

			g_free (uri);
			g_free (path_str);

			nolphin_file_unref (file);
		}

		gtk_tree_path_free (path);
	}
}

static gboolean
nolphin_list_model_multi_drag_data_get (EggTreeMultiDragSource *drag_source,
					 GList *path_list,
					 GtkSelectionData *selection_data)
{
	NolphinListModel *model;
	DragDataGetInfo context;
	guint target_info;

	model = NOLPHIN_LIST_MODEL (drag_source);

	context.model = model;
	context.path_list = path_list;

	if (!drag_target_list) {
		drag_target_list = nolphin_list_model_get_drag_target_list ();
	}

	if (gtk_target_list_find (drag_target_list,
				  gtk_selection_data_get_target (selection_data),
				  &target_info)) {
		nolphin_drag_drag_data_get (NULL,
					     NULL,
					     selection_data,
					     target_info,
					     GDK_CURRENT_TIME,
					     &context,
					     each_path_get_data_binder);
		return TRUE;
	} else {
		return FALSE;
	}
}

static gboolean
nolphin_list_model_multi_drag_data_delete (EggTreeMultiDragSource *drag_source, GList *path_list)
{
	return TRUE;
}

static void
add_dummy_row (NolphinListModel *model, FileEntry *parent_entry)
{
	FileEntry *dummy_file_entry;
	GtkTreeIter iter;
	GtkTreePath *path;

	dummy_file_entry = g_new0 (FileEntry, 1);
	dummy_file_entry->parent = parent_entry;
	dummy_file_entry->ptr = g_sequence_insert_sorted (parent_entry->files, dummy_file_entry,
							  nolphin_list_model_file_entry_compare_func, model);
	iter.stamp = model->details->stamp;
	iter.user_data = dummy_file_entry->ptr;

	path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &iter);
	gtk_tree_model_row_inserted (GTK_TREE_MODEL (model), path, &iter);
	gtk_tree_path_free (path);
}

gboolean
nolphin_list_model_add_file (NolphinListModel *model, NolphinFile *file,
			      NolphinDirectory *directory)
{
	GtkTreeIter iter;
	GtkTreePath *path;
	FileEntry *file_entry;
	GSequenceIter *ptr, *parent_ptr;
	GSequence *files;
	gboolean replace_dummy;
	GHashTable *parent_hash;

	parent_ptr = g_hash_table_lookup (model->details->directory_reverse_map,
					  directory);
	if (parent_ptr) {
		file_entry = g_sequence_get (parent_ptr);
		ptr = g_hash_table_lookup (file_entry->reverse_map, file);
	} else {
		file_entry = NULL;
		ptr = g_hash_table_lookup (model->details->top_reverse_map, file);
	}

	if (ptr != NULL) {
		if (!model->details->filter_active) {
			g_warning ("file already in tree (parent_ptr: %p)!!!", parent_ptr);
		}
		return FALSE;
	}

	file_entry = g_new0 (FileEntry, 1);
	file_entry->file = nolphin_file_ref (file);
	file_entry->parent = NULL;
	file_entry->subdirectory = NULL;
	file_entry->files = NULL;
    file_entry->ok_to_show_thumb =
        nolphin_file_get_load_deferred_attrs (file) == NOLPHIN_FILE_LOAD_DEFERRED_ATTRS_PRELOAD;
	files = model->details->files;
	parent_hash = model->details->top_reverse_map;

	replace_dummy = FALSE;

	if (parent_ptr != NULL) {
		file_entry->parent = g_sequence_get (parent_ptr);
		/* At this point we set loaded. Either we saw
		 * "done" and ignored it waiting for this, or we do this
		 * earlier, but then we replace the dummy row anyway,
		 * so it doesn't matter */
		file_entry->parent->loaded = 1;
		parent_hash = file_entry->parent->reverse_map;
		files = file_entry->parent->files;
		if (g_sequence_get_length (files) == 1) {
			GSequenceIter *dummy_ptr = g_sequence_get_iter_at_pos (files, 0);
			FileEntry *dummy_entry = g_sequence_get (dummy_ptr);
			if (dummy_entry->file == NULL) {
				/* replace the dummy loading entry */
				model->details->stamp++;
				g_sequence_remove (dummy_ptr);

				replace_dummy = TRUE;
			}
		}
	}

	if (model->details->temp_unsorted)
                file_entry->ptr = g_sequence_append (files, file_entry);
        else
                file_entry->ptr = g_sequence_insert_sorted (files, file_entry,
                                                            nolphin_list_model_file_entry_compare_func, model);

	g_hash_table_insert (parent_hash, file, file_entry->ptr);

	iter.stamp = model->details->stamp;
	iter.user_data = file_entry->ptr;

	path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &iter);
	if (replace_dummy) {
		gtk_tree_model_row_changed (GTK_TREE_MODEL (model), path, &iter);
	} else {
		gtk_tree_model_row_inserted (GTK_TREE_MODEL (model), path, &iter);
	}

    if (nolphin_file_is_directory (file) && model->details->expansion_enabled) {
        guint count;
        gboolean got_count, unreadable;

        file_entry->files = g_sequence_new ((GDestroyNotify)file_entry_free);

        got_count = nolphin_file_get_directory_item_count (file, &count, &unreadable);

        if ((!got_count && !unreadable) || count > 0) {
            add_dummy_row (model, file_entry);
            gtk_tree_model_row_has_child_toggled (GTK_TREE_MODEL (model),
                                                  path, &iter);
        }
    }

    gtk_tree_path_free (path);

	return TRUE;
}

static gboolean
update_dummy_row (NolphinListModel *model,
                  NolphinFile      *file,
                  FileEntry     *file_entry)
{
    GSequence *files;
    gboolean changed;
    guint count;
    gboolean got_count, unreadable;

    changed = FALSE;

    got_count = nolphin_file_get_directory_item_count (file, &count, &unreadable);

    if ((got_count && count == 0) || (!got_count && unreadable)) {
        files = file_entry->files;
        if (g_sequence_get_length (files) == 1) {
            GSequenceIter *dummy_ptr = g_sequence_get_iter_at_pos (files, 0);
            FileEntry *dummy_entry = g_sequence_get (dummy_ptr);

            if (dummy_entry->file == NULL) {
                if (!file_entry->expanding) {
                    GtkTreePath *path;
                    GtkTreeIter iter;

                    iter.stamp = model->details->stamp;
                    iter.user_data = dummy_ptr;
                    path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &iter);

                    g_sequence_remove (dummy_ptr);
                    model->details->stamp++;
                    gtk_tree_model_row_deleted (GTK_TREE_MODEL (model), path);
                    gtk_tree_path_free (path);
                    changed = TRUE;
                }
            }
        }
    }

    file_entry->expanding = FALSE;

    return changed;
}

void
nolphin_list_model_file_changed (NolphinListModel *model, NolphinFile *file,
				  NolphinDirectory *directory)
{
	FileEntry *parent_file_entry;
	GtkTreeIter iter;
	GtkTreePath *path, *parent_path;
	GSequenceIter *ptr;
	int pos_before, pos_after, length, i, old;
	int *new_order;
	gboolean has_iter;
	GSequence *files;

	ptr = lookup_file (model, file, directory);
	if (!ptr) {
		return;
	}

	pos_before = g_sequence_iter_get_position (ptr);

        if (!model->details->temp_unsorted)
                g_sequence_sort_changed (ptr, nolphin_list_model_file_entry_compare_func, model);

	pos_after = g_sequence_iter_get_position (ptr);

	if (pos_before != pos_after) {
		/* The file moved, we need to send rows_reordered */

		parent_file_entry = ((FileEntry *)g_sequence_get (ptr))->parent;

		if (parent_file_entry == NULL) {
			has_iter = FALSE;
			parent_path = gtk_tree_path_new ();
			files = model->details->files;
		} else {
			has_iter = TRUE;
			nolphin_list_model_ptr_to_iter (model, parent_file_entry->ptr, &iter);
			parent_path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &iter);
			files = parent_file_entry->files;
		}

		length = g_sequence_get_length (files);
		new_order = g_new (int, length);
		/* Note: new_order[newpos] = oldpos */
		for (i = 0, old = 0; i < length; ++i) {
			if (i == pos_after) {
				new_order[i] = pos_before;
			} else {
				if (old == pos_before)
					old++;
				new_order[i] = old++;
			}
		}

		gtk_tree_model_rows_reordered (GTK_TREE_MODEL (model),
					       parent_path, has_iter ? &iter : NULL, new_order);

		gtk_tree_path_free (parent_path);
		g_free (new_order);
	}

	nolphin_list_model_ptr_to_iter (model, ptr, &iter);
	path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &iter);

    gtk_tree_model_row_changed (GTK_TREE_MODEL (model), path, &iter);

    if (nolphin_file_is_directory (file) && model->details->expansion_enabled) {
        if (update_dummy_row (model, file, g_sequence_get (ptr))) {
            gtk_tree_model_row_has_child_toggled (GTK_TREE_MODEL (model),
                                                  path, &iter);
        }
    }

    gtk_tree_path_free (path);
}

gboolean
nolphin_list_model_is_empty (NolphinListModel *model)
{
	return (g_sequence_get_length (model->details->files) == 0);
}

guint
nolphin_list_model_get_length (NolphinListModel *model)
{
	return g_sequence_get_length (model->details->files);
}

static void
nolphin_list_model_remove (NolphinListModel *model, GtkTreeIter *iter)
{
	GSequenceIter *ptr, *child_ptr;
	FileEntry *file_entry, *child_file_entry, *parent_file_entry;
	GtkTreePath *path;
	GtkTreeIter parent_iter;

	ptr = iter->user_data;
	file_entry = g_sequence_get (ptr);

	if (file_entry->files != NULL) {
		while (g_sequence_get_length (file_entry->files) > 0) {
			child_ptr = g_sequence_get_begin_iter (file_entry->files);
			child_file_entry = g_sequence_get (child_ptr);
			if (child_file_entry->file != NULL) {
				nolphin_list_model_remove_file (model,
							   child_file_entry->file,
							   file_entry->subdirectory);
			} else {
				path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), iter);
				gtk_tree_path_append_index (path, 0);
				model->details->stamp++;
				g_sequence_remove (child_ptr);
				gtk_tree_model_row_deleted (GTK_TREE_MODEL (model), path);
				gtk_tree_path_free (path);
			}

			/* the parent iter didn't actually change */
			iter->stamp = model->details->stamp;
		}

	}

	if (file_entry->file != NULL) { /* Don't try to remove dummy row */
		if (file_entry->parent != NULL) {
			g_hash_table_remove (file_entry->parent->reverse_map, file_entry->file);
		} else {
			g_hash_table_remove (model->details->top_reverse_map, file_entry->file);
		}
	}

	parent_file_entry = file_entry->parent;
	if (parent_file_entry && g_sequence_get_length (parent_file_entry->files) == 1 &&
	    file_entry->file != NULL) {
		/* this is the last non-dummy child, add a dummy node */
		/* We need to do this before removing the last file to avoid
		 * collapsing the row.
		 */

        guint count;
        gboolean got_count, unreadable;

        got_count = nolphin_file_get_directory_item_count (parent_file_entry->file, &count, &unreadable);

        if ((!got_count && !unreadable) || count > 0) {
            add_dummy_row (model, parent_file_entry);
        }
    }

	if (file_entry->subdirectory != NULL) {
		g_signal_emit (model,
			       list_model_signals[SUBDIRECTORY_UNLOADED], 0,
			       file_entry->subdirectory);
		g_hash_table_remove (model->details->directory_reverse_map,
				     file_entry->subdirectory);
	}

	path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), iter);

	g_sequence_remove (ptr);
	model->details->stamp++;
	gtk_tree_model_row_deleted (GTK_TREE_MODEL (model), path);

	gtk_tree_path_free (path);

	if (parent_file_entry && g_sequence_get_length (parent_file_entry->files) == 0) {
		parent_iter.stamp = model->details->stamp;
		parent_iter.user_data = parent_file_entry->ptr;
		path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &parent_iter);
		gtk_tree_model_row_has_child_toggled (GTK_TREE_MODEL (model),
						      path, &parent_iter);
		gtk_tree_path_free (path);
	}
}

void
nolphin_list_model_remove_file (NolphinListModel *model, NolphinFile *file,
			   NolphinDirectory *directory)
{
	GtkTreeIter iter;

	if (nolphin_list_model_get_tree_iter_from_file (model, file, directory, &iter)) {
		nolphin_list_model_remove (model, &iter);
	}
}

static void
nolphin_list_model_clear_directory (NolphinListModel *model, GSequence *files)
{
	GtkTreeIter iter;
	FileEntry *file_entry;

	while (g_sequence_get_length (files) > 0) {
		iter.user_data = g_sequence_get_begin_iter (files);

		file_entry = g_sequence_get (iter.user_data);
		if (file_entry->files != NULL) {
			nolphin_list_model_clear_directory (model, file_entry->files);
		}

		iter.stamp = model->details->stamp;
		nolphin_list_model_remove (model, &iter);
	}
}

void
nolphin_list_model_clear (NolphinListModel *model)
{
	g_return_if_fail (model != NULL);

	nolphin_list_model_clear_directory (model, model->details->files);
}

NolphinFile *
nolphin_list_model_file_for_path (NolphinListModel *model, GtkTreePath *path)
{
	NolphinFile *file;
	GtkTreeIter iter;

	file = NULL;
	if (gtk_tree_model_get_iter (GTK_TREE_MODEL (model),
				     &iter, path)) {
		gtk_tree_model_get (GTK_TREE_MODEL (model),
				    &iter,
				    NOLPHIN_LIST_MODEL_FILE_COLUMN, &file,
				    -1);
	}
	return file;
}

gboolean
nolphin_list_model_load_subdirectory (NolphinListModel *model, GtkTreePath *path, NolphinDirectory **directory)
{
	GtkTreeIter iter;
	FileEntry *file_entry;
	NolphinDirectory *subdirectory;

	if (!gtk_tree_model_get_iter (GTK_TREE_MODEL (model), &iter, path)) {
		return FALSE;
	}

	file_entry = g_sequence_get (iter.user_data);
	if (file_entry->file == NULL ||
	    file_entry->subdirectory != NULL) {
		return FALSE;
	}

	subdirectory = nolphin_directory_get_for_file (file_entry->file);

	if (g_hash_table_lookup (model->details->directory_reverse_map,
				 subdirectory) != NULL) {
		nolphin_directory_unref (subdirectory);
		g_warning ("Already in directory_reverse_map, failing\n");
		return FALSE;
	}

	file_entry->subdirectory = subdirectory,
	g_hash_table_insert (model->details->directory_reverse_map,
			     subdirectory, file_entry->ptr);
	file_entry->reverse_map = g_hash_table_new (g_direct_hash, g_direct_equal);

	/* Return a ref too */
	nolphin_directory_ref (subdirectory);
	*directory = subdirectory;

	return TRUE;
}

/* removes all children of the subfolder and unloads the subdirectory */
void
nolphin_list_model_unload_subdirectory (NolphinListModel *model, GtkTreeIter *iter)
{
	GSequenceIter *child_ptr;
	FileEntry *file_entry, *child_file_entry;
	GtkTreeIter child_iter;

	file_entry = g_sequence_get (iter->user_data);
	if (file_entry->file == NULL ||
	    file_entry->subdirectory == NULL) {
		return;
	}

	file_entry->loaded = 0;

	/* Remove all children */
	while (g_sequence_get_length (file_entry->files) > 0) {
		child_ptr = g_sequence_get_begin_iter (file_entry->files);
		child_file_entry = g_sequence_get (child_ptr);
		if (child_file_entry->file == NULL) {
			/* Don't delete the dummy node */
			break;
		} else {
			nolphin_list_model_ptr_to_iter (model, child_ptr, &child_iter);
			nolphin_list_model_remove (model, &child_iter);
		}
	}

	/* Emit unload signal */
	g_signal_emit (model,
		       list_model_signals[SUBDIRECTORY_UNLOADED], 0,
		       file_entry->subdirectory);

	/* actually unload */
	g_hash_table_remove (model->details->directory_reverse_map,
			     file_entry->subdirectory);
	nolphin_directory_unref (file_entry->subdirectory);
	file_entry->subdirectory = NULL;

	g_assert (g_hash_table_size (file_entry->reverse_map) == 0);
	g_hash_table_destroy (file_entry->reverse_map);
	file_entry->reverse_map = NULL;
}



void
nolphin_list_model_set_should_sort_directories_first (NolphinListModel *model, gboolean sort_directories_first)
{
	if (model->details->sort_directories_first == sort_directories_first) {
		return;
	}

	model->details->sort_directories_first = sort_directories_first;
	nolphin_list_model_sort (model);
}

void
nolphin_list_model_set_should_sort_favorites_first (NolphinListModel *model, gboolean sort_favorites_first)
{
	if (model->details->sort_favorites_first == sort_favorites_first) {
		return;
	}

	model->details->sort_favorites_first = sort_favorites_first;
	nolphin_list_model_sort (model);
}

int
nolphin_list_model_get_sort_column_id_from_attribute (NolphinListModel *model,
						       GQuark attribute)
{
	guint i;

	if (attribute == 0) {
		return -1;
	}

	/* Hack - the preferences dialog sets modification_date for some
	 * rather than date_modified for some reason.  Make sure that
	 * works. */
	if (attribute == attribute_modification_date_q) {
		attribute = attribute_date_modified_q;
	}

	for (i = 0; i < model->details->columns->len; i++) {
		NolphinColumn *column;
		GQuark column_attribute;

		column =
			NOLPHIN_COLUMN (model->details->columns->pdata[i]);
		g_object_get (G_OBJECT (column),
			      "attribute_q", &column_attribute,
			      NULL);
		if (column_attribute == attribute) {
			return NOLPHIN_LIST_MODEL_NUM_COLUMNS + i;
		}
	}

	return -1;
}

GQuark
nolphin_list_model_get_attribute_from_sort_column_id (NolphinListModel *model,
						       int sort_column_id)
{
	NolphinColumn *column;
	int index;
	GQuark attribute;

	index = sort_column_id - NOLPHIN_LIST_MODEL_NUM_COLUMNS;

	if (index < 0 || index >= (int)model->details->columns->len) {
		g_warning ("unknown sort column id: %d", sort_column_id);
		return 0;
	}

	column = NOLPHIN_COLUMN (model->details->columns->pdata[index]);
	g_object_get (G_OBJECT (column), "attribute_q", &attribute, NULL);

	return attribute;
}

NolphinZoomLevel
nolphin_list_model_get_zoom_level_from_column_id (int column)
{
	switch (column) {
	case NOLPHIN_LIST_MODEL_SMALLEST_ICON_COLUMN:
		return NOLPHIN_ZOOM_LEVEL_SMALLEST;
	case NOLPHIN_LIST_MODEL_SMALLER_ICON_COLUMN:
		return NOLPHIN_ZOOM_LEVEL_SMALLER;
	case NOLPHIN_LIST_MODEL_SMALL_ICON_COLUMN:
		return NOLPHIN_ZOOM_LEVEL_SMALL;
	case NOLPHIN_LIST_MODEL_STANDARD_ICON_COLUMN:
		return NOLPHIN_ZOOM_LEVEL_STANDARD;
	case NOLPHIN_LIST_MODEL_LARGE_ICON_COLUMN:
		return NOLPHIN_ZOOM_LEVEL_LARGE;
	case NOLPHIN_LIST_MODEL_LARGER_ICON_COLUMN:
		return NOLPHIN_ZOOM_LEVEL_LARGER;
	case NOLPHIN_LIST_MODEL_LARGEST_ICON_COLUMN:
		return NOLPHIN_ZOOM_LEVEL_LARGEST;
        default:
                break;
	}

	g_return_val_if_reached (NOLPHIN_ZOOM_LEVEL_STANDARD);
}

int
nolphin_list_model_get_column_id_from_zoom_level (NolphinZoomLevel zoom_level)
{
	switch (zoom_level) {
	case NOLPHIN_ZOOM_LEVEL_SMALLEST:
		return NOLPHIN_LIST_MODEL_SMALLEST_ICON_COLUMN;
	case NOLPHIN_ZOOM_LEVEL_SMALLER:
		return NOLPHIN_LIST_MODEL_SMALLER_ICON_COLUMN;
	case NOLPHIN_ZOOM_LEVEL_SMALL:
		return NOLPHIN_LIST_MODEL_SMALL_ICON_COLUMN;
	case NOLPHIN_ZOOM_LEVEL_STANDARD:
		return NOLPHIN_LIST_MODEL_STANDARD_ICON_COLUMN;
	case NOLPHIN_ZOOM_LEVEL_LARGE:
		return NOLPHIN_LIST_MODEL_LARGE_ICON_COLUMN;
	case NOLPHIN_ZOOM_LEVEL_LARGER:
		return NOLPHIN_LIST_MODEL_LARGER_ICON_COLUMN;
	case NOLPHIN_ZOOM_LEVEL_LARGEST:
		return NOLPHIN_LIST_MODEL_LARGEST_ICON_COLUMN;
        case NOLPHIN_ZOOM_LEVEL_NULL:
    default:
        g_return_val_if_reached (NOLPHIN_LIST_MODEL_STANDARD_ICON_COLUMN);
	}
}

void
nolphin_list_model_set_drag_view (NolphinListModel *model,
				   GtkTreeView *view,
				   int drag_begin_x,
				   int drag_begin_y)
{
	g_return_if_fail (model != NULL);
	g_return_if_fail (NOLPHIN_IS_LIST_MODEL (model));
	g_return_if_fail (!view || GTK_IS_TREE_VIEW (view));

	model->details->drag_view = view;
	model->details->drag_begin_x = drag_begin_x;
	model->details->drag_begin_y = drag_begin_y;
}

GtkTargetList *
nolphin_list_model_get_drag_target_list (void)
{
	GtkTargetList *target_list;

	target_list = gtk_target_list_new (drag_types, G_N_ELEMENTS (drag_types));
	gtk_target_list_add_text_targets (target_list, NOLPHIN_ICON_DND_TEXT);

	return target_list;
}

int
nolphin_list_model_add_column (NolphinListModel *model,
				NolphinColumn *column)
{
	g_ptr_array_add (model->details->columns, column);
	g_object_ref (column);

	return NOLPHIN_LIST_MODEL_NUM_COLUMNS + (model->details->columns->len - 1);
}

int
nolphin_list_model_get_column_number (NolphinListModel *model,
				       const char *column_name)
{
	guint i;

	for (i = 0; i < model->details->columns->len; i++) {
		NolphinColumn *column;
		char *name;

		column = model->details->columns->pdata[i];

		g_object_get (G_OBJECT (column), "name", &name, NULL);

		if (!strcmp (name, column_name)) {
			g_free (name);
			return NOLPHIN_LIST_MODEL_NUM_COLUMNS + i;
		}
		g_free (name);
	}

	return -1;
}

static void
nolphin_list_model_dispose (GObject *object)
{
	NolphinListModel *model;
	guint i;

	model = NOLPHIN_LIST_MODEL (object);

	if (model->details->columns) {
		for (i = 0; i < model->details->columns->len; i++) {
			g_object_unref (model->details->columns->pdata[i]);
		}
		g_ptr_array_free (model->details->columns, TRUE);
		model->details->columns = NULL;
	}

	if (model->details->files) {
		g_sequence_free (model->details->files);
		model->details->files = NULL;
	}

	if (model->details->top_reverse_map) {
		g_hash_table_destroy (model->details->top_reverse_map);
		model->details->top_reverse_map = NULL;
	}
	if (model->details->directory_reverse_map) {
		g_hash_table_destroy (model->details->directory_reverse_map);
		model->details->directory_reverse_map = NULL;
	}

	G_OBJECT_CLASS (nolphin_list_model_parent_class)->dispose (object);
}

static void
nolphin_list_model_finalize (GObject *object)
{
	NolphinListModel *model;

	model = NOLPHIN_LIST_MODEL (object);

	if (model->details->highlight_files != NULL) {
		nolphin_file_list_free (model->details->highlight_files);
		model->details->highlight_files = NULL;
	}

	g_free (model->details);

	G_OBJECT_CLASS (nolphin_list_model_parent_class)->finalize (object);
}

void
nolphin_list_model_set_filter_active (NolphinListModel *model,
                                   gboolean       active)
{
	g_return_if_fail (NOLPHIN_IS_LIST_MODEL (model));

	model->details->filter_active = active;

	nolphin_list_model_sort (model);
}

gboolean
nolphin_list_model_get_filter_active (NolphinListModel *model)
{
	g_return_val_if_fail (NOLPHIN_IS_LIST_MODEL (model), FALSE);
	return model->details->filter_active;
}

static void
nolphin_list_model_init (NolphinListModel *model)
{
	model->details = g_new0 (NolphinListModelDetails, 1);
	model->details->files = g_sequence_new ((GDestroyNotify)file_entry_free);
	model->details->top_reverse_map = g_hash_table_new (g_direct_hash, g_direct_equal);
	model->details->directory_reverse_map = g_hash_table_new (g_direct_hash, g_direct_equal);
	model->details->stamp = g_random_int ();
	model->details->sort_attribute = 0;
	model->details->columns = g_ptr_array_new ();
}

static void
nolphin_list_model_class_init (NolphinListModelClass *klass)
{
	GObjectClass *object_class;

	attribute_name_q = g_quark_from_static_string ("name");
	attribute_modification_date_q = g_quark_from_static_string ("modification_date");
	attribute_date_modified_q = g_quark_from_static_string ("date_modified");
    attribute_search_result_count_q = g_quark_from_string ("search_result_count");

	object_class = (GObjectClass *)klass;
	object_class->finalize = nolphin_list_model_finalize;
	object_class->dispose = nolphin_list_model_dispose;

      list_model_signals[SUBDIRECTORY_UNLOADED] =
        g_signal_new ("subdirectory_unloaded",
                      NOLPHIN_TYPE_LIST_MODEL,
                      G_SIGNAL_RUN_FIRST,
                      G_STRUCT_OFFSET (NolphinListModelClass, subdirectory_unloaded),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__OBJECT,
                      G_TYPE_NONE, 1,
                      NOLPHIN_TYPE_DIRECTORY);

      list_model_signals[GET_ICON_SCALE] =
        g_signal_new ("get-icon-scale",
                      NOLPHIN_TYPE_LIST_MODEL,
                      G_SIGNAL_RUN_LAST,
                      0, NULL, NULL,
                      NULL,
                      G_TYPE_INT, 0);

     list_model_signals[GET_FILTER_MATCH] =
        g_signal_new ("get-filter-match",
                      NOLPHIN_TYPE_LIST_MODEL,
                      G_SIGNAL_RUN_LAST,
                      0, NULL, NULL,
                      NULL,
                      G_TYPE_INT, 1,
                      NOLPHIN_TYPE_FILE);
}

static void
nolphin_list_model_tree_model_init (GtkTreeModelIface *iface)
{
	iface->get_flags = nolphin_list_model_get_flags;
	iface->get_n_columns = nolphin_list_model_get_n_columns;
	iface->get_column_type = nolphin_list_model_get_column_type;
	iface->get_iter = nolphin_list_model_get_iter;
	iface->get_path = nolphin_list_model_get_path;
	iface->get_value = nolphin_list_model_get_value;
	iface->iter_next = nolphin_list_model_iter_next;
	iface->iter_children = nolphin_list_model_iter_children;
	iface->iter_has_child = nolphin_list_model_iter_has_child;
	iface->iter_n_children = nolphin_list_model_iter_n_children;
	iface->iter_nth_child = nolphin_list_model_iter_nth_child;
	iface->iter_parent = nolphin_list_model_iter_parent;
}

static void
nolphin_list_model_sortable_init (GtkTreeSortableIface *iface)
{
	iface->get_sort_column_id = nolphin_list_model_get_sort_column_id;
	iface->set_sort_column_id = nolphin_list_model_set_sort_column_id;
	iface->has_default_sort_func = nolphin_list_model_has_default_sort_func;
}

static void
nolphin_list_model_multi_drag_source_init (EggTreeMultiDragSourceIface *iface)
{
	iface->row_draggable = nolphin_list_model_multi_row_draggable;
	iface->drag_data_get = nolphin_list_model_multi_drag_data_get;
	iface->drag_data_delete = nolphin_list_model_multi_drag_data_delete;
}

void
nolphin_list_model_subdirectory_done_loading (NolphinListModel *model, NolphinDirectory *directory)
{
	GtkTreeIter iter;
	GtkTreePath *path;
	FileEntry *file_entry, *dummy_entry;
	GSequenceIter *parent_ptr, *dummy_ptr;
	GSequence *files;

	if (model == NULL || model->details->directory_reverse_map == NULL) {
		return;
	}
	parent_ptr = g_hash_table_lookup (model->details->directory_reverse_map,
					  directory);
	if (parent_ptr == NULL) {
		return;
	}



	file_entry = g_sequence_get (parent_ptr);
	files = file_entry->files;

	/* Only swap loading -> empty if we saw no files yet at "done",
	 * otherwise, toggle loading at first added file to the model.
	 */
	if (!nolphin_directory_is_not_empty (directory) &&
	    g_sequence_get_length (files) == 1) {
		dummy_ptr = g_sequence_get_iter_at_pos (file_entry->files, 0);
		dummy_entry = g_sequence_get (dummy_ptr);
		if (dummy_entry->file == NULL) {
			/* was the dummy file */
			file_entry->loaded = 1;

			iter.stamp = model->details->stamp;
			iter.user_data = dummy_ptr;

			path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &iter);
			gtk_tree_model_row_changed (GTK_TREE_MODEL (model), path, &iter);
			gtk_tree_path_free (path);
		}
	}
}

static void
refresh_row (gpointer data,
             gpointer user_data)
{
	NolphinFile *file;
	NolphinListModel *model;
	GList *iters, *l;
	GtkTreePath *path;

	model = user_data;
	file = data;

	iters = nolphin_list_model_get_all_iters_for_file (model, file);
	for (l = iters; l != NULL; l = l->next) {
		path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), l->data);
		gtk_tree_model_row_changed (GTK_TREE_MODEL (model), path, l->data);

		gtk_tree_path_free (path);
	}

	g_list_free_full (iters, g_free);
}

void
nolphin_list_model_set_highlight_for_files (NolphinListModel *model,
					     GList *files)
{
	if (model->details->highlight_files != NULL) {
		g_list_foreach (model->details->highlight_files,
		                refresh_row, model);
		nolphin_file_list_free (model->details->highlight_files);
		model->details->highlight_files = NULL;
	}

	if (files != NULL) {
		model->details->highlight_files = nolphin_file_list_copy (files);
		g_list_foreach (model->details->highlight_files,
		                refresh_row, model);

	}
}

void
nolphin_list_model_set_temporarily_disable_sort (NolphinListModel *model, gboolean disable)
{
    model->details->temp_unsorted = disable;

    if (!disable)
        nolphin_list_model_sort (model);
}

gboolean
nolphin_list_model_get_temporarily_disable_sort (NolphinListModel *model)
{
    return model->details->temp_unsorted;
}

void
nolphin_list_model_set_expanding (NolphinListModel *model, NolphinDirectory *directory)
{
    FileEntry *entry;
    GSequenceIter *ptr;

    ptr = NULL;

    if (directory) {
        ptr = g_hash_table_lookup (model->details->directory_reverse_map,
                                   directory);
    }

    if (!ptr) {
        g_warning ("nolphin_list_model_set_expanding: No pointer found, something's wrong?");
        return;
    }

    entry = g_sequence_get (ptr);
    entry->expanding = TRUE;
}

void
nolphin_list_model_set_view_directory (NolphinListModel *model, NolphinDirectory *dir)
{
    model->details->view_dir = dir;
}

void
nolphin_list_model_set_expansion_enabled (NolphinListModel *model, gboolean enabled)
{
    model->details->expansion_enabled = enabled;
}

