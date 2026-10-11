/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nolphin-metadata.h: #defines and other metadata-related info
 
   Copyright (C) 2000 Eazel, Inc.
  
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
  
   Author: John Sullivan <sullivan@eazel.com>
*/

#ifndef NOLPHIN_METADATA_H
#define NOLPHIN_METADATA_H

/* Keys for getting/setting Nolphin metadata. All metadata used in Nolphin
 * should define its key here, so we can keep track of the whole set easily.
 * Any updates here needs to be added in nolphin-metadata.c too.
 */

#include <glib.h>

/* Per-file */

#define NOLPHIN_METADATA_KEY_DEFAULT_VIEW		 	"nolphin-default-view"

#define NOLPHIN_METADATA_KEY_LOCATION_BACKGROUND_COLOR 	"folder-background-color"
#define NOLPHIN_METADATA_KEY_LOCATION_BACKGROUND_IMAGE 	"folder-background-image"

#define NOLPHIN_METADATA_KEY_ICON_VIEW_ZOOM_LEVEL       	"nolphin-icon-view-zoom-level"
#define NOLPHIN_METADATA_KEY_ICON_VIEW_AUTO_LAYOUT      	"nolphin-icon-view-auto-layout"
#define NOLPHIN_METADATA_KEY_ICON_VIEW_SORT_BY          	"nolphin-icon-view-sort-by"
#define NOLPHIN_METADATA_KEY_ICON_VIEW_SORT_REVERSED    	"nolphin-icon-view-sort-reversed"
#define NOLPHIN_METADATA_KEY_ICON_VIEW_KEEP_ALIGNED            "nolphin-icon-view-keep-aligned"
#define NOLPHIN_METADATA_KEY_ICON_VIEW_LAYOUT_TIMESTAMP	"nolphin-icon-view-layout-timestamp"

#define NOLPHIN_METADATA_KEY_LIST_VIEW_ZOOM_LEVEL       	"nolphin-list-view-zoom-level"
#define NOLPHIN_METADATA_KEY_LIST_VIEW_SORT_COLUMN      	"nolphin-list-view-sort-column"
#define NOLPHIN_METADATA_KEY_LIST_VIEW_SORT_REVERSED    	"nolphin-list-view-sort-reversed"
#define NOLPHIN_METADATA_KEY_LIST_VIEW_VISIBLE_COLUMNS    	"nolphin-list-view-visible-columns"
#define NOLPHIN_METADATA_KEY_LIST_VIEW_COLUMN_ORDER    	"nolphin-list-view-column-order"

#define NOLPHIN_METADATA_KEY_COMPACT_VIEW_ZOOM_LEVEL		"nolphin-compact-view-zoom-level"

#define NOLPHIN_METADATA_KEY_WINDOW_GEOMETRY			"nolphin-window-geometry"
#define NOLPHIN_METADATA_KEY_WINDOW_SCROLL_POSITION		"nolphin-window-scroll-position"
#define NOLPHIN_METADATA_KEY_WINDOW_SHOW_HIDDEN_FILES		"nolphin-window-show-hidden-files"
#define NOLPHIN_METADATA_KEY_WINDOW_MAXIMIZED			"nolphin-window-maximized"
#define NOLPHIN_METADATA_KEY_WINDOW_STICKY			"nolphin-window-sticky"
#define NOLPHIN_METADATA_KEY_WINDOW_KEEP_ABOVE			"nolphin-window-keep-above"

#define NOLPHIN_METADATA_KEY_SIDEBAR_BACKGROUND_COLOR   	"nolphin-sidebar-background-color"
#define NOLPHIN_METADATA_KEY_SIDEBAR_BACKGROUND_IMAGE   	"nolphin-sidebar-background-image"
#define NOLPHIN_METADATA_KEY_SIDEBAR_BUTTONS			"nolphin-sidebar-buttons"

#define NOLPHIN_METADATA_KEY_ANNOTATION                    "annotation"
#define NOLPHIN_METADATA_KEY_RATING                        "nolphin-rating"

#define NOLPHIN_METADATA_KEY_ICON_POSITION              	"nolphin-icon-position"
#define NOLPHIN_METADATA_KEY_ICON_POSITION_TIMESTAMP		"nolphin-icon-position-timestamp"
#define NOLPHIN_METADATA_KEY_ICON_SCALE                 	"icon-scale"
#define NOLPHIN_METADATA_KEY_CUSTOM_ICON                	"custom-icon"
#define NOLPHIN_METADATA_KEY_CUSTOM_ICON_NAME                	"custom-icon-name"
#define NOLPHIN_METADATA_KEY_EMBLEMS				"emblems"

#define NOLPHIN_METADATA_KEY_MONITOR               "monitor"
#define NOLPHIN_METADATA_KEY_DESKTOP_GRID_HORIZONTAL  "desktop-horizontal"
#define NOLPHIN_METADATA_KEY_SHOW_THUMBNAILS "show-thumbnails"
#define NOLPHIN_METADATA_KEY_DESKTOP_GRID_ADJUST      "desktop-grid-adjust"

#define NOLPHIN_METADATA_KEY_PINNED                   "pinned-to-top"
#define NOLPHIN_METADATA_KEY_FAVORITE                 "xapp-favorite"
#define NOLPHIN_METADATA_KEY_FAVORITE_AVAILABLE     "xapp-favorite-available"

guint nolphin_metadata_get_id (const char *metadata);

#endif /* NOLPHIN_METADATA_H */
