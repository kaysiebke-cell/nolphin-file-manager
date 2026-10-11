/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* Nolphin - Icon canvas item class for icon container.
 *
 * Copyright (C) 2000 Eazel, Inc.
 *
 * Author: Andy Hertzfeld <andy@eazel.com>
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

#ifndef NOLPHIN_ICON_CANVAS_ITEM_H
#define NOLPHIN_ICON_CANVAS_ITEM_H

#include <eel/eel-canvas.h>
#include <eel/eel-art-extensions.h>

G_BEGIN_DECLS

#define NOLPHIN_TYPE_ICON_CANVAS_ITEM nolphin_icon_canvas_item_get_type()
#define NOLPHIN_ICON_CANVAS_ITEM(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NOLPHIN_TYPE_ICON_CANVAS_ITEM, NolphinIconCanvasItem))
#define NOLPHIN_ICON_CANVAS_ITEM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NOLPHIN_TYPE_ICON_CANVAS_ITEM, NolphinIconCanvasItemClass))
#define NOLPHIN_IS_ICON_CANVAS_ITEM(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NOLPHIN_TYPE_ICON_CANVAS_ITEM))
#define NOLPHIN_IS_ICON_CANVAS_ITEM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NOLPHIN_TYPE_ICON_CANVAS_ITEM))
#define NOLPHIN_ICON_CANVAS_ITEM_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NOLPHIN_TYPE_ICON_CANVAS_ITEM, NolphinIconCanvasItemClass))

typedef struct NolphinIconCanvasItem NolphinIconCanvasItem;
typedef struct NolphinIconCanvasItemClass NolphinIconCanvasItemClass;
typedef struct NolphinIconCanvasItemDetails NolphinIconCanvasItemDetails;

struct NolphinIconCanvasItem {
	EelCanvasItem item;
	NolphinIconCanvasItemDetails *details;
	gpointer user_data;
};

struct NolphinIconCanvasItemClass {
	EelCanvasItemClass parent_class;
};

/* not namespaced due to their length */
typedef enum {
	BOUNDS_USAGE_FOR_LAYOUT,
	BOUNDS_USAGE_FOR_ENTIRE_ITEM,
	BOUNDS_USAGE_FOR_DISPLAY
} NolphinIconCanvasItemBoundsUsage;

/* GObject */
GType       nolphin_icon_canvas_item_get_type                 (void);

/* attributes */
void        nolphin_icon_canvas_item_set_image                (NolphinIconCanvasItem       *item,
								GdkPixbuf                    *image);
cairo_surface_t* nolphin_icon_canvas_item_get_drag_surface    (NolphinIconCanvasItem       *item);
void        nolphin_icon_canvas_item_set_emblems              (NolphinIconCanvasItem       *item,
								GList                        *emblem_pixbufs);
void        nolphin_icon_canvas_item_set_show_stretch_handles (NolphinIconCanvasItem       *item,
								gboolean                      show_stretch_handles);
double      nolphin_icon_canvas_item_get_max_text_width       (NolphinIconCanvasItem       *item);
const char *nolphin_icon_canvas_item_get_editable_text        (NolphinIconCanvasItem       *icon_item);
void        nolphin_icon_canvas_item_set_renaming             (NolphinIconCanvasItem       *icon_item,
								gboolean                      state);

/* geometry and hit testing */
gboolean    nolphin_icon_canvas_item_hit_test_rectangle       (NolphinIconCanvasItem       *item,
								EelIRect                      canvas_rect);
gboolean    nolphin_icon_canvas_item_hit_test_stretch_handles (NolphinIconCanvasItem       *item,
								gdouble                       world_x,
								gdouble                       world_y,
								GtkCornerType                *corner);
void        nolphin_icon_canvas_item_invalidate_label         (NolphinIconCanvasItem       *item);
void        nolphin_icon_canvas_item_invalidate_label_size    (NolphinIconCanvasItem       *item);
EelDRect    nolphin_icon_canvas_item_get_icon_rectangle       (const NolphinIconCanvasItem *item);
EelDRect    nolphin_icon_canvas_item_get_text_rectangle       (NolphinIconCanvasItem       *item,
								gboolean                      for_layout);
void        nolphin_icon_canvas_item_get_icon_canvas_rectangle (NolphinIconCanvasItem *item,
                                                             EelIRect *rect);
void        nolphin_icon_canvas_item_get_bounds_for_layout    (NolphinIconCanvasItem       *item,
								double *x1, double *y1, double *x2, double *y2);
void        nolphin_icon_canvas_item_get_bounds_for_entire_item (NolphinIconCanvasItem       *item,
								  double *x1, double *y1, double *x2, double *y2);
void        nolphin_icon_canvas_item_update_bounds            (NolphinIconCanvasItem       *item,
								double i2w_dx, double i2w_dy);
void        nolphin_icon_canvas_item_set_is_visible           (NolphinIconCanvasItem       *item,
								gboolean                      visible);
/* whether the entire label text must be visible at all times */
void        nolphin_icon_canvas_item_set_entire_text          (NolphinIconCanvasItem       *icon_item,
								gboolean                      entire_text);
gint        nolphin_icon_canvas_item_get_fixed_text_height_for_layout (NolphinIconCanvasItem *item);
G_END_DECLS

#endif /* NOLPHIN_ICON_CANVAS_ITEM_H */
