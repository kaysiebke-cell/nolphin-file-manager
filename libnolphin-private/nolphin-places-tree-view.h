/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-
 
   Copyright (C) 2007 Martin Wehner
  
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
*/

#ifndef NOLPHIN_PLACES_TREE_VIEW_H
#define NOLPHIN_PLACES_TREE_VIEW_H

#include <gtk/gtk.h>

#define NOLPHIN_TYPE_PLACES_TREE_VIEW nolphin_places_tree_view_get_type()
#define NOLPHIN_PLACES_TREE_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NOLPHIN_TYPE_PLACES_TREE_VIEW, NolphinPlacesTreeView))
#define NOLPHIN_PLACES_TREE_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NOLPHIN_TYPE_PLACES_TREE_VIEW, NolphinPlacesTreeViewClass))
#define NOLPHIN_IS_PLACES_TREE_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NOLPHIN_TYPE_PLACES_TREE_VIEW))
#define NOLPHIN_IS_PLACES_TREE_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NOLPHIN_TYPE_PLACES_TREE_VIEW))
#define NOLPHIN_PLACES_TREE_VIEW_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NOLPHIN_TYPE_PLACES_TREE_VIEW, NolphinPlacesTreeViewClass))

typedef struct _NolphinPlacesTreeView NolphinPlacesTreeView;
typedef struct _NolphinPlacesTreeViewClass NolphinPlacesTreeViewClass;

struct _NolphinPlacesTreeView {
	GtkTreeView parent;
};

struct _NolphinPlacesTreeViewClass {
	GtkTreeViewClass parent_class;
};

GType        nolphin_places_tree_view_get_type (void);
GtkWidget   *nolphin_places_tree_view_new      (void);

#endif /* NOLPHIN_PLACES_TREE_VIEW_H */
