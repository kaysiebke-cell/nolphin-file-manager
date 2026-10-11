/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nolphin
 *
 * Copyright (C) 2002 Sun Microsystems, Inc.
 *
 * Nolphin is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nolphin is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
 * Boston, MA 02110-1335, USA.
 * 
 * Author: Dave Camp <dave@ximian.com>
 */

/* nolphin-tree-view-drag-dest.h: Handles drag and drop for treeviews which 
 *                                 contain a hierarchy of files
 */

#ifndef NOLPHIN_TREE_VIEW_DRAG_DEST_H
#define NOLPHIN_TREE_VIEW_DRAG_DEST_H

#include <gtk/gtk.h>

#include "nolphin-file.h"

G_BEGIN_DECLS

#define NOLPHIN_TYPE_TREE_VIEW_DRAG_DEST	(nolphin_tree_view_drag_dest_get_type ())
#define NOLPHIN_TREE_VIEW_DRAG_DEST(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), NOLPHIN_TYPE_TREE_VIEW_DRAG_DEST, NolphinTreeViewDragDest))
#define NOLPHIN_TREE_VIEW_DRAG_DEST_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), NOLPHIN_TYPE_TREE_VIEW_DRAG_DEST, NolphinTreeViewDragDestClass))
#define NOLPHIN_IS_TREE_VIEW_DRAG_DEST(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), NOLPHIN_TYPE_TREE_VIEW_DRAG_DEST))
#define NOLPHIN_IS_TREE_VIEW_DRAG_DEST_CLASS(klass)	(G_TYPE_CLASS_CHECK_CLASS_TYPE ((klass), NOLPHIN_TYPE_TREE_VIEW_DRAG_DEST))

typedef struct _NolphinTreeViewDragDest        NolphinTreeViewDragDest;
typedef struct _NolphinTreeViewDragDestClass   NolphinTreeViewDragDestClass;
typedef struct _NolphinTreeViewDragDestDetails NolphinTreeViewDragDestDetails;

struct _NolphinTreeViewDragDest {
	GObject parent;
	
	NolphinTreeViewDragDestDetails *details;
};

struct _NolphinTreeViewDragDestClass {
	GObjectClass parent;
	
	char *(*get_root_uri) (NolphinTreeViewDragDest *dest);
	NolphinFile *(*get_file_for_path) (NolphinTreeViewDragDest *dest,
					    GtkTreePath *path);
	void (*move_copy_items) (NolphinTreeViewDragDest *dest,
				 const GList *item_uris,
				 const char *target_uri,
				 GdkDragAction action,
				 int x,
				 int y);
	void (* handle_netscape_url) (NolphinTreeViewDragDest *dest,
				 const char *url,
				 const char *target_uri,
				 GdkDragAction action,
				 int x,
				 int y);
	void (* handle_uri_list) (NolphinTreeViewDragDest *dest,
				  const char *uri_list,
				  const char *target_uri,
				  GdkDragAction action,
				  int x,
				  int y);
	void (* handle_text)    (NolphinTreeViewDragDest *dest,
				  const char *text,
				  const char *target_uri,
				  GdkDragAction action,
				  int x,
				  int y);
	void (* handle_raw)    (NolphinTreeViewDragDest *dest,
				  char *raw_data,
				  int length,
				  const char *target_uri,
				  const char *direct_save_uri,
				  GdkDragAction action,
				  int x,
				  int y);
};

GType                     nolphin_tree_view_drag_dest_get_type (void);
NolphinTreeViewDragDest *nolphin_tree_view_drag_dest_new      (GtkTreeView *tree_view, gboolean strict_drop);

G_END_DECLS

#endif
