/* -*- Mode: C; indent-tabs-mode: f; c-basic-offset: 4; tab-width: 4 -*- */

/* fm-icon-container.h - the container widget for file manager icons

   Copyright (C) 2002 Sun Microsystems, Inc.

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

   Author: Michael Meeks <michael@ximian.com>
*/

#ifndef NOLPHIN_ICON_VIEW_GRID_CONTAINER_H
#define NOLPHIN_ICON_VIEW_GRID_CONTAINER_H

#include "nolphin-icon-view.h"

#include <libnolphin-private/nolphin-icon-private.h>

typedef struct NolphinIconViewGridContainer NolphinIconViewGridContainer;
typedef struct NolphinIconViewGridContainerClass NolphinIconViewGridContainerClass;

#define NOLPHIN_TYPE_ICON_VIEW_GRID_CONTAINER nolphin_icon_view_grid_container_get_type()
#define NOLPHIN_ICON_VIEW_GRID_CONTAINER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NOLPHIN_TYPE_ICON_VIEW_GRID_CONTAINER, NolphinIconViewGridContainer))
#define NOLPHIN_ICON_VIEW_GRID_CONTAINER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NOLPHIN_TYPE_ICON_VIEW_GRID_CONTAINER, NolphinIconViewGridContainerClass))
#define NOLPHIN_IS_ICON_VIEW_GRID_CONTAINER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NOLPHIN_TYPE_ICON_VIEW_GRID_CONTAINER))
#define NOLPHIN_IS_ICON_VIEW_GRID_CONTAINER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NOLPHIN_TYPE_ICON_VIEW_GRID_CONTAINER))
#define NOLPHIN_ICON_VIEW_GRID_CONTAINER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NOLPHIN_TYPE_ICON_VIEW_GRID_CONTAINER, NolphinIconViewGridContainerClass))

typedef struct NolphinIconViewGridContainerDetails NolphinIconViewGridContainerDetails;

struct NolphinIconViewGridContainer {
	NolphinIconContainer parent;

	NolphinIconView *view;
	gboolean      sort_for_desktop;
    gboolean      horizontal;
    gboolean      manual_sort_dirty;
    gint          text_ellipsis_limit;

    GQuark       *attributes;
};

struct NolphinIconViewGridContainerClass {
	NolphinIconContainerClass parent_class;
};

GType                  nolphin_icon_view_grid_container_get_type         (void);
NolphinIconContainer *nolphin_icon_view_grid_container_construct        (NolphinIconViewGridContainer *icon_container,
                                                                   NolphinIconView              *view,
                                                                   gboolean                   is_desktop);
NolphinIconContainer *nolphin_icon_view_grid_container_new              (NolphinIconView              *view,
                                                                   gboolean                   is_desktop);
void                   nolphin_icon_view_grid_container_set_sort_desktop (NolphinIconViewGridContainer *container,
                                                                       gboolean                   desktop);

#endif /* NOLPHIN_ICON_VIEW_GRID_CONTAINER_H */
