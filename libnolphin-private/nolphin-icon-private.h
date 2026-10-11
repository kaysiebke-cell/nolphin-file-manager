/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* gnome-icon-container-private.h

   Copyright (C) 1999, 2000 Free Software Foundation
   Copyright (C) 2000 Eazel, Inc.

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

   Author: Ettore Perazzoli <ettore@gnu.org>
*/

#ifndef NOLPHIN_ICON_CONTAINER_PRIVATE_H
#define NOLPHIN_ICON_CONTAINER_PRIVATE_H

#include <eel/eel-glib-extensions.h>
#include <libnolphin-private/nolphin-icon-canvas-item.h>
#include <libnolphin-private/nolphin-icon-container.h>
#include <libnolphin-private/nolphin-icon-dnd.h>
#include <libnolphin-private/nolphin-icon.h>

/* Private NolphinIconContainer members. */

typedef struct {
	double start_x, start_y;

	EelCanvasItem *selection_rectangle;
	EelDRect prev_rect;
	guint timer_id;

	guint prev_x, prev_y;

	int last_adj_x;
	int last_adj_y;
	gboolean active;
} NolphinIconRubberbandInfo;

typedef enum {
	DRAG_STATE_INITIAL,
	DRAG_STATE_MOVE_OR_COPY,
	DRAG_STATE_STRETCH
} DragState;

typedef struct {
	/* Pointer position in canvas coordinates. */
	int pointer_x, pointer_y;

	/* Icon top, left, and size in canvas coordinates. */
	int icon_x, icon_y;
	guint icon_size;
} StretchState;

typedef enum {
	AXIS_NONE,
	AXIS_HORIZONTAL,
	AXIS_VERTICAL
} Axis;

enum {
	LABEL_COLOR,
	LABEL_COLOR_HIGHLIGHT,
	LABEL_COLOR_ACTIVE,
	LABEL_COLOR_PRELIGHT,
	LABEL_INFO_COLOR,
	LABEL_INFO_COLOR_HIGHLIGHT,
	LABEL_INFO_COLOR_ACTIVE,
	LAST_LABEL_COLOR
};

typedef struct {
    gint icon_pad_left;
    gint icon_pad_right;
    gint icon_pad_top;
    gint icon_pad_bottom;
    gint container_pad_left;
    gint container_pad_right;
    gint container_pad_top;
    gint container_pad_bottom;
    gint standard_icon_grid_width;
    gint text_beside_icon_grid_width;
    gint desktop_pad_horizontal;
    gint desktop_pad_vertical;
    gint snap_size_x;
    gint snap_size_y;
    gint max_text_width_standard;
    gint max_text_width_beside;
    gint max_text_width_beside_top_to_bottom;
    gint icon_vertical_adjust;
} NolphinViewLayoutConstants;

typedef struct {
    NolphinIconContainer *container;
    GtkBorder *borders;
    int **icon_grid;
    int *grid_memory;
    int num_rows;
    int num_columns;
    int icon_size;
    int real_snap_x;
    int real_snap_y;
    gboolean horizontal;
} NolphinCenteredPlacementGrid;

typedef struct {
    NolphinIconContainer *container;
    int **icon_grid;
    int *grid_memory;
    int num_rows;
    int num_columns;
    gboolean tight;
} NolphinPlacementGrid;

struct NolphinIconContainerDetails {
	/* List of icons. */
	GList *icons;
	GList *new_icons;
	GHashTable *icon_set;

	/* Current icon for keyboard navigation. */
	NolphinIcon *keyboard_focus;
	NolphinIcon *keyboard_rubberband_start;

	/* Rubberbanding status. */
        NolphinIconRubberbandInfo rubberband_info;

        NolphinViewLayoutConstants *view_constants;
        /* Last highlighted drop target. */
	NolphinIcon *drop_target;

	/* Current icon with stretch handles, so we have only one. */
	NolphinIcon *stretch_icon;
	double stretch_initial_x, stretch_initial_y;
		/* The position we are scaling to on stretch */
	double world_x;
	double world_y;
	
	guint stretch_initial_size;

	/* Timeout used to make a selected icon fully visible after a short
	 * period of time. (The timeout is needed to make sure
	 * double-clicking still works.)
	 */
	guint keyboard_icon_reveal_timer_id;
	NolphinIcon *keyboard_icon_to_reveal;
	
	/* If a request is made to reveal an unpositioned icon we remember
	 * it and reveal it once it gets positioned (in relayout).
	 */
	NolphinIcon *pending_icon_to_reveal;

	/* If a request is made to rename an unpositioned icon we remember
	 * it and start renaming it once it gets positioned (in relayout).
	 */
	NolphinIcon *pending_icon_to_rename;

	/* Remembered information about the start of the current event. */
	guint32 button_down_time;
	
	/* Drag state. Valid only if drag_button is non-zero. */
	guint drag_button;
	NolphinIcon *drag_icon;
	int drag_x, drag_y;
	DragState drag_state;
	gboolean drag_started;
	StretchState stretch_start;
	gboolean drag_allow_moves;

	gboolean icon_selected_on_button_down;
	guint double_click_button[2];
	gboolean skip_rename_on_release;
	NolphinIcon *double_click_icon[2]; /* Both clicks in a double click need to be on the same icon */

	NolphinIcon *range_selection_base_icon;
	/* DnD info. */
	NolphinIconDndInfo *dnd_info;
	
	/* Renaming Details */

	GtkWidget *rename_widget;	/* Editable text item */
	char *original_text;			/* Copy of editable text for later compare */

	char *font; 	/* specific fonts used to draw labels */
        gboolean renaming;

	/* Idle ID. */
	guint idle_id;

	/* Idle handler for stretch code */
	guint stretch_idle_id;

	/* Align idle id */
	guint align_idle_id;

	/* Used to coalesce selection changed signals in some cases */
	guint selection_changed_id;

	/* zoom level */
	int zoom_level;
	
	/* font sizes used to draw labels */
	int font_size_table[NOLPHIN_ZOOM_LEVEL_LARGEST + 1];

	/* State used so arrow keys don't wander if icons aren't lined up.
	 */
	int arrow_key_start_x;
	int arrow_key_start_y;
	GtkDirectionType arrow_key_direction;

	/* Mode settings. */
	gboolean single_click_mode;
	gboolean auto_layout;
        gboolean stored_auto_layout;
        gboolean click_to_rename;

	/* Whether for the vertical layout, all columns are supposed to
	 * have the same width. */
	gboolean all_columns_same_width;
	
	/* Layout mode */
	NolphinIconLayoutMode layout_mode;

	/* Label position */
	NolphinIconLabelPosition label_position;

	/* Forced icon size, iff greater than 0 */
	int forced_icon_size;

	/* Should the container keep icons aligned to a grid */
	gboolean keep_aligned;

        /* Set to TRUE after first allocation has been done */
	gboolean has_been_allocated;

	int size_allocation_count;
	guint size_allocation_count_id;
    int renaming_allocation_count;
	
	/* Is the container fixed or resizable */
	gboolean is_fixed_size;
	
	/* Is the container for a desktop window */
	gboolean is_desktop;

    /* Used by desktop grid container only */
    gboolean horizontal;
    gint h_adjust;
    gint v_adjust;

    gboolean show_desktop_tooltips;
    gboolean show_icon_view_tooltips;

    gchar *filter_highlight_text;

	/* Ignore the visible area the next time the scroll region is recomputed */
	gboolean reset_scroll_region_trigger;
	
        gint tooltip_flags; /* Really a NolphinFileTooltipFlags */

	/* margins to follow, used for the desktop panel avoidance */
	int left_margin;
	int right_margin;
	int top_margin;
	int bottom_margin;

	/* Whether we should use drop shadows for the icon labels or not */
	gboolean use_drop_shadows;
	gboolean drop_shadows_requested;
	/* interactive search */
	gboolean imcontext_changed;
	/* a11y items used by canvas items */
	guint a11y_item_action_idle_handler;

        time_t layout_timestamp;

	GtkWidget *search_window;
	GtkWidget *search_entry;
	NolphinCenteredPlacementGrid *dnd_grid;
	GQueue* a11y_item_action_queue;
        int selected_iter;
	guint search_entry_changed_id;
	guint typeselect_flush_timeout;

        gint current_dnd_x;
        gint current_dnd_y;
        gboolean insert_dnd_mode;

	eel_boolean_bit is_loading : 1;
	eel_boolean_bit needs_resort : 1;

	eel_boolean_bit store_layout_timestamps : 1;
	eel_boolean_bit store_layout_timestamps_when_finishing_new_icons : 1;

    gint ok_to_load_deferred_attrs;
    guint update_visible_icons_id;

    GQueue *lazy_icon_load_queue;
    guint lazy_icon_load_id;

    GList *current_selection;
    gint current_selection_count;
    gint fixed_text_height;
};

typedef struct {
    double width;
    double height;
    double x_offset;
    double y_offset;
} NolphinCanvasRects;

#define GET_VIEW_CONSTANT(c,name) (NOLPHIN_ICON_CONTAINER (c)->details->view_constants->name)

#define SNAP_HORIZONTAL(func,x) ((func ((double)((x) - GET_VIEW_CONSTANT (container, desktop_pad_horizontal)) / GET_VIEW_CONSTANT (container, snap_size_x)) * GET_VIEW_CONSTANT (container, snap_size_x)) + GET_VIEW_CONSTANT (container, desktop_pad_horizontal))
#define SNAP_VERTICAL(func, y) ((func ((double)((y) - GET_VIEW_CONSTANT (container, desktop_pad_vertical)) / GET_VIEW_CONSTANT (container, snap_size_y)) * GET_VIEW_CONSTANT (container, snap_size_y)) + GET_VIEW_CONSTANT (container, desktop_pad_vertical))

#define SNAP_NEAREST_HORIZONTAL(x) SNAP_HORIZONTAL (floor, x + .5)
#define SNAP_NEAREST_VERTICAL(y) SNAP_VERTICAL (floor, y + .5)

#define SNAP_CEIL_HORIZONTAL(x) SNAP_HORIZONTAL (ceil, x)
#define SNAP_CEIL_VERTICAL(y) SNAP_VERTICAL (ceil, y)

/* Private functions shared by mutiple files. */
NolphinIcon *nolphin_icon_container_get_icon_by_uri             (NolphinIconContainer *container,
								   const char            *uri);
void          nolphin_icon_container_select_list_unselect_others (NolphinIconContainer *container,
								   GList                 *icons);
char *        nolphin_icon_container_get_icon_uri                (NolphinIconContainer *container,
								   NolphinIcon          *icon);
char *        nolphin_icon_container_get_icon_drop_target_uri    (NolphinIconContainer *container,
								   NolphinIcon          *icon);
void          nolphin_icon_container_update_icon                 (NolphinIconContainer *container,
								   NolphinIcon          *icon);
gboolean      nolphin_icon_container_scroll                      (NolphinIconContainer *container,
								   int                    delta_x,
								   int                    delta_y);
void          nolphin_icon_container_update_scroll_region        (NolphinIconContainer *container);
gint              nolphin_icon_container_get_canvas_height (NolphinIconContainer *container,
                                                         GtkAllocation      allocation);
gint              nolphin_icon_container_get_canvas_width (NolphinIconContainer *container,
                                                        GtkAllocation      allocation);
double        nolphin_icon_container_get_mirror_x_position (NolphinIconContainer *container, NolphinIcon *icon, double x);
void          nolphin_icon_container_set_rtl_positions (NolphinIconContainer *container);
void          nolphin_icon_container_end_renaming_mode (NolphinIconContainer *container, gboolean commit);
NolphinIcon     *nolphin_icon_container_get_icon_being_renamed (NolphinIconContainer *container);

void              nolphin_icon_container_icon_set_position (NolphinIconContainer *container,
                                                         NolphinIcon          *icon,
                                                         gdouble            x,
                                                         gdouble            y);
void              nolphin_icon_container_icon_get_bounding_box (NolphinIconContainer *container, NolphinIcon *icon,
                                                             int *x1_return, int *y1_return,
                                                             int *x2_return, int *y2_return,
                                                             NolphinIconCanvasItemBoundsUsage usage);

void              nolphin_icon_container_move_icon                     (NolphinIconContainer *container,
                                                                     NolphinIcon *icon,
                                                                     int x, int y,
                                                                     double scale,
                                                                     gboolean raise,
                                                                     gboolean snap,
                                                                     gboolean update_position);

void          nolphin_icon_container_icon_raise                  (NolphinIconContainer *container,
                                                               NolphinIcon *icon);
void          nolphin_icon_container_finish_adding_icon                (NolphinIconContainer *container,
                                                                     NolphinIcon           *icon);
gboolean      nolphin_icon_container_icon_is_positioned (const NolphinIcon *icon);
void          nolphin_icon_container_sort_icons (NolphinIconContainer *container,
                                              GList            **icons);
void          nolphin_icon_container_resort (NolphinIconContainer *container);
void          nolphin_icon_container_get_all_icon_bounds (NolphinIconContainer *container,
                                                       double *x1, double *y1,
                                                       double *x2, double *y2,
                                                       NolphinIconCanvasItemBoundsUsage usage);
void          nolphin_icon_container_store_layout_timestamps_now (NolphinIconContainer *container);
void          nolphin_icon_container_redo_layout (NolphinIconContainer *container);

NolphinIconInfo *nolphin_icon_container_get_icon_images (NolphinIconContainer *container,
                                                   NolphinIconData      *data,
                                                   int                    size,
                                                   gboolean               for_drag_accept,
                                                   gboolean              *has_open_window,
                                                   gboolean               visible);
void          nolphin_icon_container_get_icon_text (NolphinIconContainer *container,
                                                 NolphinIconData      *data,
                                                 char                 **editable_text,
                                                 char                 **additional_text,
                                                 gboolean              *pinned,
                                                 gboolean              *fav_unavailable,
                                                 gboolean               include_invisible);

/* nolphin-centered-placement-grid api
 *
 * used by nolphin-icon-view-grid-container.c, nolphin-icon-dnd.h
 */

NolphinCenteredPlacementGrid *nolphin_centered_placement_grid_new               (NolphinIconContainer *container, gboolean horizontal);
void               nolphin_centered_placement_grid_free              (NolphinCenteredPlacementGrid *grid);

void               nolphin_centered_placement_grid_nominal_to_icon_position (NolphinCenteredPlacementGrid *grid,
                                                                          NolphinIcon                  *icon,
                                                                          gint                       x_nominal,
                                                                          gint                       y_nominal,
                                                                          gint                      *x_adjusted,
                                                                          gint                      *y_adjusted);
void               nolphin_centered_placement_grid_icon_position_to_nominal (NolphinCenteredPlacementGrid *grid,
                                                                          NolphinIcon                  *icon,
                                                                          gint                       x_adjusted,
                                                                          gint                       y_adjusted,
                                                                          gint                      *x_nominal,
                                                                          gint                      *y_nominal);
void               nolphin_centered_placement_grid_mark_icon         (NolphinCenteredPlacementGrid *grid, NolphinIcon *icon);
void               nolphin_centered_placement_grid_unmark_icon       (NolphinCenteredPlacementGrid *grid, NolphinIcon *icon);
void               nolphin_centered_placement_grid_mark_position (NolphinCenteredPlacementGrid *grid,
                                                               gint                       x,
                                                               gint                       y);
void               nolphin_centered_placement_grid_unmark_position (NolphinCenteredPlacementGrid *grid,
                                                                 gint                       x,
                                                                 gint                       y);
void               nolphin_centered_placement_grid_pre_populate        (NolphinCenteredPlacementGrid *grid,
                                                                     GList                     *icons,
                                                                     gboolean                   ignore_lazy);
void               nolphin_centered_placement_grid_get_next_position_rect (NolphinCenteredPlacementGrid *grid,
                                                                        GdkRectangle              *in_rect,
                                                                        GdkRectangle              *out_rect,
                                                                        gboolean                  *is_free);
void               nolphin_centered_placement_grid_get_current_position_rect (NolphinCenteredPlacementGrid *grid,
                                                                           gint                       x,
                                                                           gint                       y,
                                                                           GdkRectangle              *rect,
                                                                           gboolean                  *is_free);
NolphinIcon *         nolphin_centered_placement_grid_get_icon_at_position (NolphinCenteredPlacementGrid *grid,
                                                                      gint                       x,
                                                                      gint                       y);
GList *            nolphin_centered_placement_grid_clear_grid_for_selection (NolphinCenteredPlacementGrid *grid,
                                                                          gint                       start_x,
                                                                          gint                       start_y,
                                                                          GList                     *drag_sel_list);
/* nolphin-placement-grid api
 *
 * used by nolphin-icon-view-container.c
 */

NolphinPlacementGrid *nolphin_placement_grid_new               (NolphinIconContainer *container, gboolean tight);
void               nolphin_placement_grid_free              (NolphinPlacementGrid *grid);
gboolean           nolphin_placement_grid_position_is_free  (NolphinPlacementGrid *grid, EelIRect pos);
void               nolphin_placement_grid_mark              (NolphinPlacementGrid *grid, EelIRect pos);
void               nolphin_placement_grid_canvas_position_to_grid_position (NolphinPlacementGrid *grid, EelIRect canvas_position, EelIRect *grid_position);
void               nolphin_placement_grid_mark_icon         (NolphinPlacementGrid *grid, NolphinIcon *icon);

#endif /* NOLPHIN_ICON_CONTAINER_PRIVATE_H */
