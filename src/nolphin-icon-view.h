/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nolphin-icon-view.h - interface for icon view of directory.
 *
 * Copyright (C) 2000 Eazel, Inc.
 *
 * The Gnome Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * The Gnome Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with the Gnome Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
 * Boston, MA 02110-1335, USA.
 *
 * Authors: John Sullivan <sullivan@eazel.com>
 *
 */

#ifndef NOLPHIN_ICON_VIEW_H
#define NOLPHIN_ICON_VIEW_H

#include "nolphin-view.h"

typedef struct NolphinIconView NolphinIconView;
typedef struct NolphinIconViewClass NolphinIconViewClass;

#define NOLPHIN_TYPE_ICON_VIEW nolphin_icon_view_get_type()
#define NOLPHIN_ICON_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NOLPHIN_TYPE_ICON_VIEW, NolphinIconView))
#define NOLPHIN_ICON_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NOLPHIN_TYPE_ICON_VIEW, NolphinIconViewClass))
#define NOLPHIN_IS_ICON_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NOLPHIN_TYPE_ICON_VIEW))
#define NOLPHIN_IS_ICON_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NOLPHIN_TYPE_ICON_VIEW))
#define NOLPHIN_ICON_VIEW_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NOLPHIN_TYPE_ICON_VIEW, NolphinIconViewClass))

#define NOLPHIN_ICON_VIEW_ID "OAFIID:Nolphin_File_Manager_Icon_View"
#define FM_COMPACT_VIEW_ID "OAFIID:Nolphin_File_Manager_Compact_View"
#define NOLPHIN_GALLERY_VIEW_ID "OAFIID:Nolphin_File_Manager_Gallery_View"

typedef struct NolphinIconViewDetails NolphinIconViewDetails;

struct NolphinIconView {
	NolphinView parent;
	NolphinIconViewDetails *details;
};

struct NolphinIconViewClass {
	NolphinViewClass parent_class;

    gboolean use_grid_container;
};

/* GObject support */
GType   nolphin_icon_view_get_type      (void);
int     nolphin_icon_view_compare_files (NolphinIconView   *icon_view,
					  NolphinFile *a,
					  NolphinFile *b);
gboolean nolphin_icon_view_is_compact   (NolphinIconView *icon_view);
gboolean nolphin_icon_view_is_gallery   (NolphinIconView *icon_view);

void    nolphin_icon_view_register         (void);
void    nolphin_icon_view_compact_register (void);
void    nolphin_icon_view_gallery_register (void);

NolphinIconContainer * nolphin_icon_view_get_icon_container (NolphinIconView *view);

void    nolphin_icon_view_set_sort_criterion_by_sort_type (NolphinIconView     *icon_view,
                                                        NolphinFileSortType  sort_type);
void    nolphin_icon_view_set_directory_keep_aligned (NolphinIconView *icon_view,
                                                   NolphinFile *file,
                                                   gboolean keep_aligned);
gchar  *nolphin_icon_view_get_directory_sort_by      (NolphinIconView *icon_view, NolphinFile *file);
gboolean nolphin_icon_view_get_directory_sort_reversed (NolphinIconView *icon_view, NolphinFile *file);
void    nolphin_icon_view_flip_sort_reversed (NolphinIconView *icon_view);
gboolean nolphin_icon_view_set_sort_reversed (NolphinIconView *icon_view,
                                          gboolean      new_value,
                                          gboolean      set_metadata);
void   nolphin_icon_view_set_directory_horizontal_layout (NolphinIconView *icon_view,
                                                       NolphinFile     *file,
                                                       gboolean      horizontal);
gboolean nolphin_icon_view_get_directory_horizontal_layout (NolphinIconView *icon_view,
                                                         NolphinFile     *file);

void nolphin_icon_view_set_directory_grid_adjusts (NolphinIconView *icon_view,
                                                NolphinFile     *file,
                                                gint          horizontal,
                                                gint          vertical);
void nolphin_icon_view_get_directory_grid_adjusts (NolphinIconView *icon_view,
                                                NolphinFile     *file,
                                                gint         *horizontal,
                                                gint         *vertical);
#endif /* NOLPHIN_ICON_VIEW_H */
