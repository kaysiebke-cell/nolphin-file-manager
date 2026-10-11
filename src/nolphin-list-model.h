/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* fm-list-model.h - a GtkTreeModel for file lists. 

   Copyright (C) 2001, 2002 Anders Carlsson

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

   Authors: Anders Carlsson <andersca@gnu.org>
*/

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <libnolphin-private/nolphin-file.h>
#include <libnolphin-private/nolphin-directory.h>
#include <libnolphin-extension/nolphin-column.h>

#ifndef NOLPHIN_LIST_MODEL_H
#define NOLPHIN_LIST_MODEL_H

#define NOLPHIN_TYPE_LIST_MODEL nolphin_list_model_get_type()
#define NOLPHIN_LIST_MODEL(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NOLPHIN_TYPE_LIST_MODEL, NolphinListModel))
#define NOLPHIN_LIST_MODEL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NOLPHIN_TYPE_LIST_MODEL, NolphinListModelClass))
#define NOLPHIN_IS_LIST_MODEL(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NOLPHIN_TYPE_LIST_MODEL))
#define NOLPHIN_IS_LIST_MODEL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NOLPHIN_TYPE_LIST_MODEL))
#define NOLPHIN_LIST_MODEL_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NOLPHIN_TYPE_LIST_MODEL, NolphinListModelClass))

enum {
	NOLPHIN_LIST_MODEL_FILE_COLUMN,
	NOLPHIN_LIST_MODEL_SUBDIRECTORY_COLUMN,
	NOLPHIN_LIST_MODEL_SMALLEST_ICON_COLUMN,
	NOLPHIN_LIST_MODEL_SMALLER_ICON_COLUMN,
	NOLPHIN_LIST_MODEL_SMALL_ICON_COLUMN,
	NOLPHIN_LIST_MODEL_STANDARD_ICON_COLUMN,
	NOLPHIN_LIST_MODEL_LARGE_ICON_COLUMN,
	NOLPHIN_LIST_MODEL_LARGER_ICON_COLUMN,
	NOLPHIN_LIST_MODEL_LARGEST_ICON_COLUMN,
	NOLPHIN_LIST_MODEL_FILE_NAME_IS_EDITABLE_COLUMN,
    NOLPHIN_LIST_MODEL_TEXT_WEIGHT_COLUMN,
    NOLPHIN_LIST_MODEL_ICON_SHOWN,
	NOLPHIN_LIST_MODEL_NUM_COLUMNS
};

typedef struct NolphinListModelDetails NolphinListModelDetails;

typedef struct NolphinListModel {
	GObject parent_instance;
	NolphinListModelDetails *details;
} NolphinListModel;

typedef struct {
	GObjectClass parent_class;

	void (* subdirectory_unloaded)(NolphinListModel *model,
				       NolphinDirectory *subdirectory);
} NolphinListModelClass;

GType    nolphin_list_model_get_type                          (void);
gboolean nolphin_list_model_add_file                          (NolphinListModel          *model,
								NolphinFile         *file,
								NolphinDirectory    *directory);
void     nolphin_list_model_file_changed                      (NolphinListModel          *model,
								NolphinFile         *file,
								NolphinDirectory    *directory);
gboolean nolphin_list_model_is_empty                          (NolphinListModel          *model);
guint    nolphin_list_model_get_length                        (NolphinListModel          *model);
void     nolphin_list_model_remove_file                       (NolphinListModel          *model,
								NolphinFile         *file,
								NolphinDirectory    *directory);
void     nolphin_list_model_clear                             (NolphinListModel          *model);
gboolean nolphin_list_model_get_tree_iter_from_file           (NolphinListModel          *model,
								NolphinFile         *file,
								NolphinDirectory    *directory,
								GtkTreeIter          *iter);
GList *  nolphin_list_model_get_all_iters_for_file            (NolphinListModel          *model,
								NolphinFile         *file);
gboolean nolphin_list_model_get_first_iter_for_file           (NolphinListModel          *model,
								NolphinFile         *file,
								GtkTreeIter          *iter);
void     nolphin_list_model_set_should_sort_directories_first (NolphinListModel          *model,
								gboolean              sort_directories_first);
void     nolphin_list_model_set_should_sort_favorites_first (NolphinListModel          *model,
								gboolean              sort_favorites_first);
int      nolphin_list_model_get_sort_column_id_from_attribute (NolphinListModel *model,
								GQuark       attribute);
GQuark   nolphin_list_model_get_attribute_from_sort_column_id (NolphinListModel *model,
								int sort_column_id);
void     nolphin_list_model_sort_files                        (NolphinListModel *model,
								GList **files);

NolphinZoomLevel nolphin_list_model_get_zoom_level_from_column_id (int               column);
int               nolphin_list_model_get_column_id_from_zoom_level (NolphinZoomLevel zoom_level);

NolphinFile *    nolphin_list_model_file_for_path (NolphinListModel *model, GtkTreePath *path);
gboolean          nolphin_list_model_load_subdirectory (NolphinListModel *model, GtkTreePath *path, NolphinDirectory **directory);
void              nolphin_list_model_unload_subdirectory (NolphinListModel *model, GtkTreeIter *iter);

void              nolphin_list_model_set_drag_view (NolphinListModel *model,
						     GtkTreeView *view,
						     int begin_x, 
						     int begin_y);

GtkTargetList *   nolphin_list_model_get_drag_target_list (void);

int               nolphin_list_model_compare_func (NolphinListModel *model,
						    NolphinFile *file1,
						    NolphinFile *file2);


int               nolphin_list_model_add_column (NolphinListModel *model,
						  NolphinColumn *column);
int               nolphin_list_model_get_column_number (NolphinListModel *model,
							 const char *column_name);

void              nolphin_list_model_subdirectory_done_loading (NolphinListModel       *model,
								 NolphinDirectory *directory);

void              nolphin_list_model_set_highlight_for_files (NolphinListModel *model,
							       GList *files);

void              nolphin_list_model_set_temporarily_disable_sort (NolphinListModel *model, gboolean disable);
gboolean          nolphin_list_model_get_temporarily_disable_sort (NolphinListModel *model);
void              nolphin_list_model_set_expanding                (NolphinListModel *model, NolphinDirectory *directory);
void              nolphin_list_model_set_view_directory           (NolphinListModel *model, NolphinDirectory *dir);
void              nolphin_list_model_set_expansion_enabled        (NolphinListModel *model, gboolean enabled);

void              nolphin_list_model_set_filter_active           (NolphinListModel *model,
                                                               gboolean       active);
gboolean          nolphin_list_model_get_filter_active           (NolphinListModel *model);

#endif /* NOLPHIN_LIST_MODEL_H */
