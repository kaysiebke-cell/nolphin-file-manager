/* -*- Mode: C; indent-tabs-mode: f; c-basic-offset: 4; tab-width: 4 -*- */

/* nolphin-icon-container.c - Icon container widget.

   Copyright (C) 1999, 2000 Free Software Foundation
   Copyright (C) 2000, 2001 Eazel, Inc.
   Copyright (C) 2002, 2003 Red Hat, Inc.

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

   Authors: Ettore Perazzoli <ettore@gnu.org>,
   Darin Adler <darin@bentspoon.com>
*/

#include <config.h>
#include <math.h>
#include "nolphin-icon-container.h"

#include "nolphin-file.h"
#include "nolphin-directory.h"
#include "nolphin-desktop-icon-file.h"
#include "nolphin-global-preferences.h"
#include "nolphin-icon-private.h"
#include "nolphin-lib-self-check-functions.h"
#include "nolphin-selection-canvas-item.h"
#include "nolphin-desktop-utils.h"
#include "nolphin-thumbnails.h"
#include <atk/atkaction.h>
#include <eel/eel-accessibility.h>
#include <eel/eel-vfs-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-art-extensions.h>
#include <eel/eel-editable-label.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <glib/gi18n.h>
#include <stdio.h>
#include <string.h>

#define DEBUG_FLAG NOLPHIN_DEBUG_ICON_CONTAINER
#include "nolphin-debug.h"

#define TAB_NAVIGATION_DISABLED

/* Interval for updating the rubberband selection, in milliseconds.  */
#define RUBBERBAND_TIMEOUT_INTERVAL 10
#define RUBBERBAND_SCROLL_THRESHOLD 5

/* Timeout for making the icon currently selected for keyboard operation visible.
 * If this is 0, you can get into trouble with extra scrolling after holding
 * down the arrow key for awhile when there are many items.
 */
#define KEYBOARD_ICON_REVEAL_TIMEOUT 10

/* Maximum amount of milliseconds the mouse button is allowed to stay down
 * and still be considered a click.
 */
#define MAX_CLICK_TIME 1500

#define INITIAL_UPDATE_VISIBLE_DELAY 300
#define NORMAL_UPDATE_VISIBLE_DELAY 50

/* Button assignments. */
#define DRAG_BUTTON 1
#define RUBBERBAND_BUTTON 1
#define MIDDLE_BUTTON 2
#define CONTEXTUAL_MENU_BUTTON 3
#define DRAG_MENU_BUTTON 2

/* Copied from NolphinIconContainer */
#define NOLPHIN_ICON_CONTAINER_SEARCH_DIALOG_TIMEOUT 5

/* Copied from NolphinFile */
#define UNDEFINED_TIME ((time_t) (-1))

enum {
	ACTION_ACTIVATE,
	ACTION_MENU,
	LAST_ACTION
};

typedef struct {
	GList *selection;
	char *action_descriptions[LAST_ACTION];
} NolphinIconContainerAccessiblePrivate;

static GType         nolphin_icon_container_accessible_get_type (void);

static void          preview_selected_items                         (NolphinIconContainer *container);
static void          activate_selected_items                        (NolphinIconContainer *container);
static void          activate_selected_items_alternate              (NolphinIconContainer *container,
								     NolphinIcon          *icon);
static void          compute_stretch                                (StretchState          *start,
								     StretchState          *current);
static NolphinIcon *get_first_selected_icon                        (NolphinIconContainer *container);
static NolphinIcon *get_nth_selected_icon                          (NolphinIconContainer *container,
								     int                    index);
static gboolean      has_multiple_selection                         (NolphinIconContainer *container);
static gboolean      all_selected                                   (NolphinIconContainer *container);
static gboolean      has_selection                                  (NolphinIconContainer *container);
static void          icon_destroy                                   (NolphinIconContainer *container,
								     NolphinIcon          *icon);

static gboolean      is_renaming                                    (NolphinIconContainer *container);
static gboolean      is_renaming_pending                            (NolphinIconContainer *container);
static void          process_pending_icon_to_rename                 (NolphinIconContainer *container);

static void          handle_hadjustment_changed                     (GtkAdjustment         *adjustment,
								     NolphinIconContainer *container);
static void          handle_vadjustment_changed                     (GtkAdjustment         *adjustment,
								     NolphinIconContainer *container);
static GList *       nolphin_icon_container_get_selected_icons (NolphinIconContainer *container);
static void          queue_update_visible_icons                 (NolphinIconContainer *container, gint delay);
static void          reveal_icon                                    (NolphinIconContainer *container,
								     NolphinIcon *icon);

static void         text_ellipsis_limit_changed_container_callback  (gpointer callback_data);

static int compare_icons_horizontal (NolphinIconContainer *container,
				     NolphinIcon *icon_a,
				     NolphinIcon *icon_b);

static int compare_icons_vertical (NolphinIconContainer *container,
				   NolphinIcon *icon_a,
				   NolphinIcon *icon_b);

static void remove_search_entry_timeout (NolphinIconContainer *container);

static gboolean handle_icon_slow_two_click (NolphinIconContainer *container,
                                            NolphinIcon *icon,
                                            GdkEventButton *event);

static void schedule_align_icons (NolphinIconContainer *container);

static gpointer accessible_parent_class;

static GQuark accessible_private_data_quark = 0;

static const char *nolphin_icon_container_accessible_action_names[] = {
	"activate",
	"menu",
	NULL
};

static const char *nolphin_icon_container_accessible_action_descriptions[] = {
	"Activate selected items",
	"Popup context menu",
	NULL
};

G_DEFINE_TYPE (NolphinIconContainer, nolphin_icon_container, EEL_TYPE_CANVAS);

/* The NolphinIconContainer signals.  */
enum {
	ACTIVATE,
	ACTIVATE_ALTERNATE,
	ACTIVATE_PREVIEWER,
	BAND_SELECT_STARTED,
	BAND_SELECT_ENDED,
	BUTTON_PRESS,
	CAN_ACCEPT_ITEM,
	CONTEXT_CLICK_BACKGROUND,
	CONTEXT_CLICK_SELECTION,
	MIDDLE_CLICK,
	GET_CONTAINER_URI,
	GET_ICON_URI,
	GET_ICON_DROP_TARGET_URI,
	ICON_POSITION_CHANGED,
	GET_STORED_LAYOUT_TIMESTAMP,
	STORE_LAYOUT_TIMESTAMP,
	ICON_RENAME_STARTED,
	ICON_RENAME_ENDED,
	ICON_STRETCH_STARTED,
	ICON_STRETCH_ENDED,
	LAYOUT_CHANGED,
	MOVE_COPY_ITEMS,
	HANDLE_NETSCAPE_URL,
	HANDLE_URI_LIST,
	HANDLE_TEXT,
	HANDLE_RAW,
	SELECTION_CHANGED,
	ICON_ADDED,
	ICON_REMOVED,
	CLEARED,
    GET_TOOLTIP_TEXT,
    CHECK_FILTER_EVENT,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static void
tooltip_prefs_changed_callback (NolphinIconContainer *container)
{
    container->details->show_desktop_tooltips = g_settings_get_boolean (nolphin_preferences,
                                                                        NOLPHIN_PREFERENCES_TOOLTIPS_DESKTOP);
    container->details->show_icon_view_tooltips = g_settings_get_boolean (nolphin_preferences,
                                                                          NOLPHIN_PREFERENCES_TOOLTIPS_ICON_VIEW);

    container->details->tooltip_flags = nolphin_global_preferences_get_tooltip_flags ();

    nolphin_icon_container_request_update_all (container);
}

/* Functions dealing with NolphinIcons.  */

static gboolean
clicked_on_text (NolphinIconContainer *container,
                          NolphinIcon *icon,
                    GdkEventButton *event)
{
    if (icon == NULL)
        return FALSE;

    double eventX, eventY;
    EelDRect icon_rect;

    icon_rect = nolphin_icon_canvas_item_get_text_rectangle (icon->item, TRUE);
    eel_canvas_window_to_world (EEL_CANVAS (container), event->x, event->y, &eventX, &eventY);

    gboolean ret =  (eventX > icon_rect.x0) &&
           (eventX < icon_rect.x1) &&
           (eventY > icon_rect.y0) &&
           (eventY < icon_rect.y1);

    return ret;
}

static gboolean
clicked_on_icon (NolphinIconContainer *container,
                          NolphinIcon *icon,
                    GdkEventButton *event)
{
    if (icon == NULL)
        return FALSE;

    double eventX, eventY;
    EelDRect icon_rect;

    icon_rect = nolphin_icon_canvas_item_get_icon_rectangle (icon->item);
    eel_canvas_window_to_world (EEL_CANVAS (container), event->x, event->y, &eventX, &eventY);

    gboolean ret =  (eventX > icon_rect.x0) &&
           (eventX < icon_rect.x1) &&
           (eventY > icon_rect.y0) &&
           (eventY < icon_rect.y1);

    return ret;
}

static void
icon_free (NolphinIcon *icon)
{
	/* Destroy this canvas item; the parent will unref it. */
	eel_canvas_item_destroy (EEL_CANVAS_ITEM (icon->item));
	g_free (icon);
}

gboolean
nolphin_icon_container_icon_is_positioned (const NolphinIcon *icon)
{
	return icon->x != ICON_UNPOSITIONED_VALUE && icon->y != ICON_UNPOSITIONED_VALUE;
}

static void
icon_get_size (NolphinIconContainer *container,
	       NolphinIcon *icon,
	       guint *size)
{
	if (size != NULL) {
		*size = MAX (nolphin_get_icon_size_for_zoom_level (container->details->zoom_level)
			       * icon->scale, NOLPHIN_ICON_SIZE_SMALLEST);
	}
}

/* The icon_set_size function is used by the stretching user
 * interface, which currently stretches in a way that keeps the aspect
 * ratio. Later we might have a stretching interface that stretches Y
 * separate from X and we will change this around.
 */
static void
icon_set_size (NolphinIconContainer *container,
	       NolphinIcon *icon,
	       guint icon_size,
	       gboolean snap,
	       gboolean update_position)
{
	guint old_size;
	double scale;

	icon_get_size (container, icon, &old_size);
	if (icon_size == old_size) {
		return;
	}

	scale = (double) icon_size /
		nolphin_get_icon_size_for_zoom_level
		(container->details->zoom_level);
	nolphin_icon_container_move_icon (container, icon,
					   icon->x, icon->y,
					   scale, FALSE,
					   snap, update_position);
}

static void
emit_stretch_started (NolphinIconContainer *container, NolphinIcon *icon)
{
	g_signal_emit (container,
			 signals[ICON_STRETCH_STARTED], 0,
			 icon->data);
}

static void
emit_stretch_ended (NolphinIconContainer *container, NolphinIcon *icon)
{
	g_signal_emit (container,
			 signals[ICON_STRETCH_ENDED], 0,
			 icon->data);
}

static void
icon_toggle_selected (NolphinIconContainer *container,
		      NolphinIcon *icon)
{
	nolphin_icon_container_end_renaming_mode (container, TRUE);

	icon->is_selected = !icon->is_selected;
	eel_canvas_item_set (EEL_CANVAS_ITEM (icon->item),
			     "highlighted_for_selection", (gboolean) icon->is_selected,
			     NULL);

	/* If the icon is deselected, then get rid of the stretch handles.
	 * No harm in doing the same if the item is newly selected.
	 */
	if (icon == container->details->stretch_icon) {
		container->details->stretch_icon = NULL;
		nolphin_icon_canvas_item_set_show_stretch_handles (icon->item, FALSE);
		/* snap the icon if necessary */
		if (container->details->keep_aligned) {
			nolphin_icon_container_move_icon (container,
							   icon,
							   icon->x, icon->y,
							   icon->scale,
							   FALSE, TRUE, TRUE);
		}

		emit_stretch_ended (container, icon);
	}

	/* Raise each newly-selected icon to the front as it is selected. */
	if (icon->is_selected) {
        nolphin_icon_container_icon_raise (container, icon);
	}
}

/* Select an icon. Return TRUE if selection has changed. */
static gboolean
icon_set_selected (NolphinIconContainer *container,
		   NolphinIcon *icon,
		   gboolean select)
{
	g_assert (select == FALSE || select == TRUE);
	g_assert (icon->is_selected == FALSE || icon->is_selected == TRUE);

	if (select == icon->is_selected) {
		return FALSE;
	}

	icon_toggle_selected (container, icon);
	g_assert (select == icon->is_selected);
	return TRUE;
}

/* Utility functions for NolphinIconContainer.  */

gboolean
nolphin_icon_container_scroll (NolphinIconContainer *container,
				int delta_x, int delta_y)
{
	GtkAdjustment *hadj, *vadj;
	int old_h_value, old_v_value;

	hadj = gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (container));
	vadj = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (container));

	/* Store the old ajustment values so we can tell if we
	 * ended up actually scrolling. We may not have in a case
	 * where the resulting value got pinned to the adjustment
	 * min or max.
	 */
	old_h_value = gtk_adjustment_get_value (hadj);
	old_v_value = gtk_adjustment_get_value (vadj);

	gtk_adjustment_set_value (hadj, gtk_adjustment_get_value (hadj) + delta_x);
	gtk_adjustment_set_value (vadj, gtk_adjustment_get_value (vadj) + delta_y);

	/* return TRUE if we did scroll */
	return gtk_adjustment_get_value (hadj) != old_h_value || gtk_adjustment_get_value (vadj) != old_v_value;
}

static void
pending_icon_to_reveal_destroy_callback (NolphinIconCanvasItem *item,
					 NolphinIconContainer *container)
{
	g_assert (NOLPHIN_IS_ICON_CONTAINER (container));
	g_assert (container->details->pending_icon_to_reveal != NULL);
	g_assert (container->details->pending_icon_to_reveal->item == item);

	container->details->pending_icon_to_reveal = NULL;
}

static NolphinIcon*
get_pending_icon_to_reveal (NolphinIconContainer *container)
{
	return container->details->pending_icon_to_reveal;
}

static void
set_pending_icon_to_reveal (NolphinIconContainer *container, NolphinIcon *icon)
{
	NolphinIcon *old_icon;

	old_icon = container->details->pending_icon_to_reveal;

	if (icon == old_icon) {
		return;
	}

	if (old_icon != NULL) {
		g_signal_handlers_disconnect_by_func
			(old_icon->item,
			 G_CALLBACK (pending_icon_to_reveal_destroy_callback),
			 container);
	}

	if (icon != NULL) {
		g_signal_connect (icon->item, "destroy",
				  G_CALLBACK (pending_icon_to_reveal_destroy_callback),
				  container);
	}

	container->details->pending_icon_to_reveal = icon;
}

static void
item_get_canvas_bounds (NolphinIconContainer *container,
                        EelCanvasItem     *item,
                        EelIRect          *bounds,
                        gboolean           safety_pad)
{
	EelDRect world_rect;

	eel_canvas_item_get_bounds (item,
				    &world_rect.x0,
				    &world_rect.y0,
				    &world_rect.x1,
				    &world_rect.y1);
	eel_canvas_item_i2w (item->parent,
			     &world_rect.x0,
			     &world_rect.y0);
	eel_canvas_item_i2w (item->parent,
			     &world_rect.x1,
			     &world_rect.y1);
	if (safety_pad) {
		world_rect.x0 -= GET_VIEW_CONSTANT (container, icon_pad_left) + GET_VIEW_CONSTANT (container, icon_pad_right);
		world_rect.x1 += GET_VIEW_CONSTANT (container, icon_pad_left) + GET_VIEW_CONSTANT (container, icon_pad_right);

		world_rect.y0 -= GET_VIEW_CONSTANT (container, icon_pad_top) + GET_VIEW_CONSTANT (container, icon_pad_bottom);
		world_rect.y1 += GET_VIEW_CONSTANT (container, icon_pad_top) + GET_VIEW_CONSTANT (container, icon_pad_bottom);
	}

	eel_canvas_w2c (item->canvas,
			world_rect.x0,
			world_rect.y0,
			&bounds->x0,
			&bounds->y0);
	eel_canvas_w2c (item->canvas,
			world_rect.x1,
			world_rect.y1,
			&bounds->x1,
			&bounds->y1);
}

static void
icon_get_row_and_column_bounds (NolphinIconContainer *container,
				NolphinIcon *icon,
				EelIRect *bounds,
				gboolean safety_pad)
{
	GList *p;
	NolphinIcon *one_icon;
	EelIRect one_bounds;

	item_get_canvas_bounds (container, EEL_CANVAS_ITEM (icon->item), bounds, safety_pad);

	for (p = container->details->icons; p != NULL; p = p->next) {
		one_icon = p->data;

		if (icon == one_icon) {
			continue;
		}

		if (compare_icons_horizontal (container, icon, one_icon) == 0) {
			item_get_canvas_bounds (container, EEL_CANVAS_ITEM (one_icon->item), &one_bounds, safety_pad);
			bounds->x0 = MIN (bounds->x0, one_bounds.x0);
			bounds->x1 = MAX (bounds->x1, one_bounds.x1);
		}

		if (compare_icons_vertical (container, icon, one_icon) == 0) {
			item_get_canvas_bounds (container, EEL_CANVAS_ITEM (one_icon->item), &one_bounds, safety_pad);
			bounds->y0 = MIN (bounds->y0, one_bounds.y0);
			bounds->y1 = MAX (bounds->y1, one_bounds.y1);
		}
	}


}

static void
reveal_icon (NolphinIconContainer *container,
	     NolphinIcon *icon)
{
	GtkAllocation allocation;
	GtkAdjustment *hadj, *vadj;
	EelIRect bounds;

	if (!nolphin_icon_container_icon_is_positioned (icon)) {
		set_pending_icon_to_reveal (container, icon);
		return;
	}

	set_pending_icon_to_reveal (container, NULL);

	gtk_widget_get_allocation (GTK_WIDGET (container), &allocation);

	hadj = gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (container));
	vadj = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (container));

	if (nolphin_icon_container_is_auto_layout (container)) {
		/* ensure that we reveal the entire row/column */
		icon_get_row_and_column_bounds (container, icon, &bounds, TRUE);
	} else {
		item_get_canvas_bounds (container, EEL_CANVAS_ITEM (icon->item), &bounds, TRUE);
	}
	if (bounds.y0 < gtk_adjustment_get_value (vadj)) {
		gtk_adjustment_set_value (vadj, bounds.y0);
	} else if (bounds.y1 > gtk_adjustment_get_value (vadj) + allocation.height) {
		gtk_adjustment_set_value
			(vadj, bounds.y1 - allocation.height);
	}

	if (bounds.x0 < gtk_adjustment_get_value (hadj)) {
		gtk_adjustment_set_value (hadj, bounds.x0);
	} else if (bounds.x1 > gtk_adjustment_get_value (hadj) + allocation.width) {
        if (bounds.x1 - allocation.width > bounds.x0) {
            gtk_adjustment_set_value
                (hadj, bounds.x0);
        } else {
            gtk_adjustment_set_value
                (hadj, bounds.x1 - allocation.width);
        }
	}
}

static void
process_pending_icon_to_reveal (NolphinIconContainer *container)
{
	NolphinIcon *pending_icon_to_reveal;

	pending_icon_to_reveal = get_pending_icon_to_reveal (container);

	if (pending_icon_to_reveal != NULL) {
		reveal_icon (container, pending_icon_to_reveal);
	}
}

static gboolean
keyboard_icon_reveal_timeout_callback (gpointer data)
{
	NolphinIconContainer *container;
	NolphinIcon *icon;

	container = NOLPHIN_ICON_CONTAINER (data);
	icon = container->details->keyboard_icon_to_reveal;

	g_assert (icon != NULL);

	/* Only reveal the icon if it's still the keyboard focus or if
	 * it's still selected. Someone originally thought we should
	 * cancel this reveal if the user manages to sneak a direct
	 * scroll in before the timeout fires, but we later realized
	 * this wouldn't actually be an improvement
	 * (see bugzilla.gnome.org 40612).
	 */
	if (icon == container->details->keyboard_focus
	    || icon->is_selected) {
		reveal_icon (container, icon);
	}
	container->details->keyboard_icon_reveal_timer_id = 0;

	return FALSE;
}

static void
unschedule_keyboard_icon_reveal (NolphinIconContainer *container)
{
	NolphinIconContainerDetails *details;

	details = container->details;

	if (details->keyboard_icon_reveal_timer_id != 0) {
		g_source_remove (details->keyboard_icon_reveal_timer_id);
	}
}

static void
schedule_keyboard_icon_reveal (NolphinIconContainer *container,
			       NolphinIcon *icon)
{
	NolphinIconContainerDetails *details;

	details = container->details;

	unschedule_keyboard_icon_reveal (container);

	details->keyboard_icon_to_reveal = icon;
	details->keyboard_icon_reveal_timer_id
		= g_timeout_add (KEYBOARD_ICON_REVEAL_TIMEOUT,
				 keyboard_icon_reveal_timeout_callback,
				 container);
}

static void
clear_keyboard_focus (NolphinIconContainer *container)
{
        if (container->details->keyboard_focus != NULL) {
		eel_canvas_item_set (EEL_CANVAS_ITEM (container->details->keyboard_focus->item),
				       "highlighted_as_keyboard_focus", 0,
				       NULL);
	}

	container->details->keyboard_focus = NULL;
}

inline static void
emit_atk_focus_tracker_notify (NolphinIcon *icon)
{
	AtkObject *atk_object = atk_gobject_accessible_for_object (G_OBJECT (icon->item));
	atk_focus_tracker_notify (atk_object);
}

/* Set @icon as the icon currently selected for keyboard operations. */
static void
set_keyboard_focus (NolphinIconContainer *container,
		    NolphinIcon *icon)
{
	g_assert (icon != NULL);

	if (icon == container->details->keyboard_focus) {
		return;
	}

	clear_keyboard_focus (container);

	container->details->keyboard_focus = icon;

	eel_canvas_item_set (EEL_CANVAS_ITEM (container->details->keyboard_focus->item),
			       "highlighted_as_keyboard_focus", 1,
			       NULL);

	emit_atk_focus_tracker_notify (icon);
}

static void
set_keyboard_rubberband_start (NolphinIconContainer *container,
			       NolphinIcon *icon)
{
	container->details->keyboard_rubberband_start = icon;
}

static void
clear_keyboard_rubberband_start (NolphinIconContainer *container)
{
	container->details->keyboard_rubberband_start = NULL;
}

/* carbon-copy of eel_canvas_group_bounds(), but
 * for NolphinIconContainerItems it returns the
 * bounds for the “entire item”.
 */
static void
get_icon_bounds_for_canvas_bounds (EelCanvasGroup *group,
				   double *x1, double *y1,
				   double *x2, double *y2,
				   NolphinIconCanvasItemBoundsUsage usage)
{
	EelCanvasItem *child;
	GList *list;
	double tx1, ty1, tx2, ty2;
	double minx, miny, maxx, maxy;
	int set;

	/* Get the bounds of the first visible item */

	child = NULL; /* Unnecessary but eliminates a warning. */

	set = FALSE;

	for (list = group->item_list; list; list = list->next) {
		child = list->data;

		if (!NOLPHIN_IS_ICON_CANVAS_ITEM (child)) {
			continue;
		}

		if (child->flags & EEL_CANVAS_ITEM_VISIBLE) {
			set = TRUE;
			if (!NOLPHIN_IS_ICON_CANVAS_ITEM (child) ||
			    usage == BOUNDS_USAGE_FOR_DISPLAY) {
				eel_canvas_item_get_bounds (child, &minx, &miny, &maxx, &maxy);
			} else if (usage == BOUNDS_USAGE_FOR_LAYOUT) {
				nolphin_icon_canvas_item_get_bounds_for_layout (NOLPHIN_ICON_CANVAS_ITEM (child),
										 &minx, &miny, &maxx, &maxy);
			} else if (usage == BOUNDS_USAGE_FOR_ENTIRE_ITEM) {
				nolphin_icon_canvas_item_get_bounds_for_entire_item (NOLPHIN_ICON_CANVAS_ITEM (child),
										      &minx, &miny, &maxx, &maxy);
			} else {
				g_assert_not_reached ();
			}
			break;
		}
	}

	/* If there were no visible items, return an empty bounding box */

	if (!set) {
		*x1 = *y1 = *x2 = *y2 = 0.0;
		return;
	}

	/* Now we can grow the bounds using the rest of the items */

	list = list->next;

	for (; list; list = list->next) {
		child = list->data;

		if (!NOLPHIN_IS_ICON_CANVAS_ITEM (child)) {
			continue;
		}

		if (!(child->flags & EEL_CANVAS_ITEM_VISIBLE))
			continue;

		if (!NOLPHIN_IS_ICON_CANVAS_ITEM (child) ||
		    usage == BOUNDS_USAGE_FOR_DISPLAY) {
			eel_canvas_item_get_bounds (child, &tx1, &ty1, &tx2, &ty2);
		} else if (usage == BOUNDS_USAGE_FOR_LAYOUT) {
			nolphin_icon_canvas_item_get_bounds_for_layout (NOLPHIN_ICON_CANVAS_ITEM (child),
									 &tx1, &ty1, &tx2, &ty2);
		} else if (usage == BOUNDS_USAGE_FOR_ENTIRE_ITEM) {
			nolphin_icon_canvas_item_get_bounds_for_entire_item (NOLPHIN_ICON_CANVAS_ITEM (child),
									      &tx1, &ty1, &tx2, &ty2);
		} else {
			g_assert_not_reached ();
		}

		if (tx1 < minx)
			minx = tx1;

		if (ty1 < miny)
			miny = ty1;

		if (tx2 > maxx)
			maxx = tx2;

		if (ty2 > maxy)
			maxy = ty2;
	}

	/* Make the bounds be relative to our parent's coordinate system */

	if (EEL_CANVAS_ITEM (group)->parent) {
		minx += group->xpos;
		miny += group->ypos;
		maxx += group->xpos;
		maxy += group->ypos;
	}

	if (x1 != NULL) {
		*x1 = minx;
	}

	if (y1 != NULL) {
		*y1 = miny;
	}

	if (x2 != NULL) {
		*x2 = maxx;
	}

	if (y2 != NULL) {
		*y2 = maxy;
	}
}

void
nolphin_icon_container_get_all_icon_bounds (NolphinIconContainer *container,
		     double *x1, double *y1,
		     double *x2, double *y2,
		     NolphinIconCanvasItemBoundsUsage usage)
{
	/* FIXME bugzilla.gnome.org 42477: Do we have to do something about the rubberband
	 * here? Any other non-icon items?
	 */
	get_icon_bounds_for_canvas_bounds (EEL_CANVAS_GROUP (EEL_CANVAS (container)->root),
					   x1, y1, x2, y2, usage);
}

/* Don't preserve visible white space the next time the scroll region
 * is recomputed when the container is not empty. */
void
nolphin_icon_container_reset_scroll_region (NolphinIconContainer *container)
{
	container->details->reset_scroll_region_trigger = TRUE;
}

/* Set a new scroll region without eliminating any of the currently-visible area. */
static void
canvas_set_scroll_region_include_visible_area (EelCanvas *canvas,
					       double x1, double y1,
					       double x2, double y2)
{
	double old_x1, old_y1, old_x2, old_y2;
	double old_scroll_x, old_scroll_y;
	double height, width;
	GtkAllocation allocation;

	eel_canvas_get_scroll_region (canvas, &old_x1, &old_y1, &old_x2, &old_y2);
	gtk_widget_get_allocation (GTK_WIDGET (canvas), &allocation);

	width = (allocation.width) / canvas->pixels_per_unit;
	height = (allocation.height) / canvas->pixels_per_unit;

	old_scroll_x = gtk_adjustment_get_value (gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (canvas)));
	old_scroll_y = gtk_adjustment_get_value (gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (canvas)));

	x1 = MIN (x1, old_x1 + old_scroll_x);
	y1 = MIN (y1, old_y1 + old_scroll_y);
	x2 = MAX (x2, old_x1 + old_scroll_x + width);
	y2 = MAX (y2, old_y1 + old_scroll_y + height);

	eel_canvas_set_scroll_region
		(canvas, x1, y1, x2, y2);
}

void
nolphin_icon_container_update_scroll_region (NolphinIconContainer *container)
{
	double x1, y1, x2, y2;
	double pixels_per_unit;
	GtkAdjustment *hadj, *vadj;
	float step_increment;
	gboolean reset_scroll_region;
	GtkAllocation allocation;

	pixels_per_unit = EEL_CANVAS (container)->pixels_per_unit;

	if (nolphin_icon_container_get_is_fixed_size (container)) {
		/* Set the scroll region to the size of the container allocation */
		gtk_widget_get_allocation (GTK_WIDGET (container), &allocation);
		eel_canvas_set_scroll_region
			(EEL_CANVAS (container),
			 (double) - container->details->left_margin / pixels_per_unit,
			 (double) - container->details->top_margin / pixels_per_unit,
			 ((double) (allocation.width - 1)
			 - container->details->left_margin
			 - container->details->right_margin)
			 / pixels_per_unit,
			 ((double) (allocation.height - 1)
			 - container->details->top_margin
			 - container->details->bottom_margin)
			 / pixels_per_unit);
		return;
	}

	reset_scroll_region = container->details->reset_scroll_region_trigger
		|| nolphin_icon_container_is_empty (container)
		|| nolphin_icon_container_is_auto_layout (container);

	/* The trigger is only cleared when container is non-empty, so
	 * callers can reliably reset the scroll region when an item
	 * is added even if extraneous relayouts are called when the
	 * window is still empty.
	 */
	if (!nolphin_icon_container_is_empty (container)) {
		container->details->reset_scroll_region_trigger = FALSE;
	}

	nolphin_icon_container_get_all_icon_bounds (container, &x1, &y1, &x2, &y2, BOUNDS_USAGE_FOR_ENTIRE_ITEM);

	/* Add border at the "end"of the layout (i.e. after the icons), to
	 * ensure we get some space when scrolled to the end.
	 * For horizontal layouts, we add a bottom border.
	 * Vertical layout is used by the compact view so the end
	 * depends on the RTL setting.
	 */
	if (nolphin_icon_container_is_layout_vertical (container)) {
		if (nolphin_icon_container_is_layout_rtl (container)) {
			x1 -= GET_VIEW_CONSTANT (container, icon_pad_left) + GET_VIEW_CONSTANT (container, container_pad_left);
		} else {
			x2 += GET_VIEW_CONSTANT (container, icon_pad_right) + GET_VIEW_CONSTANT (container, container_pad_right);
		}
	} else {
		y2 += GET_VIEW_CONSTANT (container, icon_pad_bottom) + GET_VIEW_CONSTANT (container, container_pad_bottom);
	}

	/* Auto-layout assumes a 0, 0 scroll origin and at least allocation->width.
	 * Then we lay out to the right or to the left, so
	 * x can be < 0 and > allocation */
	if (nolphin_icon_container_is_auto_layout (container)) {
		gtk_widget_get_allocation (GTK_WIDGET (container), &allocation);
		x1 = MIN (x1, 0);
		x2 = MAX (x2, allocation.width / pixels_per_unit);
		y1 = 0;
	} else {
		/* Otherwise we add the padding that is at the start of the
		   layout */
		if (nolphin_icon_container_is_layout_rtl (container)) {
			x2 += GET_VIEW_CONSTANT (container, icon_pad_right) + GET_VIEW_CONSTANT (container, container_pad_right);
		} else {
			x1 -= GET_VIEW_CONSTANT (container, icon_pad_left) + GET_VIEW_CONSTANT (container, container_pad_left);
		}
		y1 -= GET_VIEW_CONSTANT (container, icon_pad_top) + GET_VIEW_CONSTANT (container, container_pad_top);
	}

	x2 -= 1;
	x2 = MAX(x1, x2);

	y2 -= 1;
	y2 = MAX(y1, y2);

	if (reset_scroll_region) {
		eel_canvas_set_scroll_region
			(EEL_CANVAS (container),
			 x1, y1, x2, y2);
	} else {
		canvas_set_scroll_region_include_visible_area
			(EEL_CANVAS (container),
			 x1, y1, x2, y2);
	}

	hadj = gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (container));
	vadj = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (container));

	/* Scroll by 1/4 icon each time you click. */
	step_increment = nolphin_get_icon_size_for_zoom_level
		(container->details->zoom_level) / 4;
	if (gtk_adjustment_get_step_increment (hadj) != step_increment) {
		gtk_adjustment_set_step_increment (hadj, step_increment);
	}
	if (gtk_adjustment_get_step_increment (vadj) != step_increment) {
		gtk_adjustment_set_step_increment (vadj, step_increment);
	}
}

static int
compare_icons (gconstpointer a, gconstpointer b, gpointer icon_container)
{
	NolphinIconContainerClass *klass;
	const NolphinIcon *icon_a, *icon_b;

	icon_a = a;
	icon_b = b;
	klass  = NOLPHIN_ICON_CONTAINER_GET_CLASS (icon_container);

	return klass->compare_icons (icon_container, icon_a->data, icon_b->data);
}

static void
align_icons (NolphinIconContainer *container)
{
    NOLPHIN_ICON_CONTAINER_GET_CLASS (container)->align_icons (container);
}

static void
redo_layout_internal (NolphinIconContainer *container)
{
    container->details->fixed_text_height = -1;

    if (NOLPHIN_ICON_CONTAINER_GET_CLASS (container)->finish_adding_new_icons != NULL) {
        NOLPHIN_ICON_CONTAINER_GET_CLASS (container)->finish_adding_new_icons (container);
    }

	/* Don't do any re-laying-out during stretching. Later we
	 * might add smart logic that does this and leaves room for
	 * the stretched icon, but if we do it we want it to be fast
	 * and only re-lay-out when it's really needed.
	 */

    if (container->details->auto_layout && container->details->drag_state != DRAG_STATE_STRETCH) {
        if (container->details->needs_resort) {
            nolphin_icon_container_resort (container);
            container->details->needs_resort = FALSE;
        }

        NOLPHIN_ICON_CONTAINER_GET_CLASS (container)->lay_down_icons (container, container->details->icons, 0);
	}

	if (nolphin_icon_container_is_layout_rtl (container)) {
		nolphin_icon_container_set_rtl_positions (container);
	}

	nolphin_icon_container_update_scroll_region (container);
    queue_update_visible_icons (container, INITIAL_UPDATE_VISIBLE_DELAY);

	process_pending_icon_to_reveal (container);
	process_pending_icon_to_rename (container);
}

static gboolean
redo_layout_callback (gpointer callback_data)
{
	NolphinIconContainer *container;

	container = NOLPHIN_ICON_CONTAINER (callback_data);
	redo_layout_internal (container);
	container->details->idle_id = 0;

	return FALSE;
}

static void
unschedule_redo_layout (NolphinIconContainer *container)
{
        if (container->details->idle_id != 0) {
		g_source_remove (container->details->idle_id);
		container->details->idle_id = 0;
	}
}

static void
schedule_redo_layout (NolphinIconContainer *container)
{
	if (container->details->idle_id == 0
	    && container->details->has_been_allocated) {
		container->details->idle_id = g_idle_add
			(redo_layout_callback, container);
	}
}

void
nolphin_icon_container_redo_layout (NolphinIconContainer *container)
{
	unschedule_redo_layout (container);
	redo_layout_internal (container);
}

static void
reload_icon_positions (NolphinIconContainer *container)
{
    NOLPHIN_ICON_CONTAINER_GET_CLASS (container)->reload_icon_positions (container);
}

/* Container-level icon handling functions.  */

static gboolean
button_event_modifies_selection (GdkEventButton *event)
{
	return (event->state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) != 0;
}

/* invalidate the cached label sizes for all the icons */
static void
invalidate_label_sizes (NolphinIconContainer *container)
{
	GList *p;
	NolphinIcon *icon;

	for (p = container->details->icons; p != NULL; p = p->next) {
		icon = p->data;

		nolphin_icon_canvas_item_invalidate_label_size (icon->item);
	}
}

static void
update_icons (NolphinIconContainer *container)
{
    GList *p;
    NolphinIcon *icon;

    for (p = container->details->icons; p != NULL; p = p->next) {
        icon = p->data;

        nolphin_icon_container_update_icon (container, icon);
    }
}

static gboolean
select_range (NolphinIconContainer *container,
	      NolphinIcon *icon1,
	      NolphinIcon *icon2,
	      gboolean unselect_outside_range)
{
	gboolean selection_changed;
	GList *p;
	NolphinIcon *icon;
	NolphinIcon *unmatched_icon;
	gboolean select;

	selection_changed = FALSE;

	unmatched_icon = NULL;
	select = FALSE;
	for (p = container->details->icons; p != NULL; p = p->next) {
		icon = p->data;

		if (unmatched_icon == NULL) {
			if (icon == icon1) {
				unmatched_icon = icon2;
				select = TRUE;
			} else if (icon == icon2) {
				unmatched_icon = icon1;
				select = TRUE;
			}
		}

		if (select || unselect_outside_range) {
			selection_changed |= icon_set_selected
				(container, icon, select);
		}

		if (unmatched_icon != NULL && icon == unmatched_icon) {
			select = FALSE;
		}

	}

	if (selection_changed && icon2 != NULL) {
		emit_atk_focus_tracker_notify (icon2);
	}
	return selection_changed;
}


static gboolean
select_one_unselect_others (NolphinIconContainer *container,
			    NolphinIcon *icon_to_select)
{
	gboolean selection_changed;
	GList *p;
	NolphinIcon *icon;

	selection_changed = FALSE;

	for (p = container->details->icons; p != NULL; p = p->next) {
		icon = p->data;

		selection_changed |= icon_set_selected
			(container, icon, icon == icon_to_select);
	}

	if (selection_changed && icon_to_select != NULL) {
		emit_atk_focus_tracker_notify (icon_to_select);
		reveal_icon (container, icon_to_select);
	}
	return selection_changed;
}

static gboolean
unselect_all (NolphinIconContainer *container)
{
	return select_one_unselect_others (container, NULL);
}

/* Implementation of rubberband selection.  */
static void
rubberband_select (NolphinIconContainer *container,
		   const EelDRect *previous_rect,
		   const EelDRect *current_rect)
{
	GList *p;
	gboolean selection_changed, is_in, canvas_rect_calculated;
	NolphinIcon *icon;
	EelIRect canvas_rect;
	EelCanvas *canvas;

	selection_changed = FALSE;
	canvas_rect_calculated = FALSE;

	for (p = container->details->icons; p != NULL; p = p->next) {
		icon = p->data;

		if (!canvas_rect_calculated) {
			/* Only do this calculation once, since all the canvas items
			 * we are interating are in the same coordinate space
			 */
			canvas = EEL_CANVAS_ITEM (icon->item)->canvas;
			eel_canvas_w2c (canvas,
					current_rect->x0,
					current_rect->y0,
					&canvas_rect.x0,
					&canvas_rect.y0);
			eel_canvas_w2c (canvas,
					current_rect->x1,
					current_rect->y1,
					&canvas_rect.x1,
					&canvas_rect.y1);
			canvas_rect_calculated = TRUE;
		}

		is_in = nolphin_icon_canvas_item_hit_test_rectangle (icon->item, canvas_rect);

		selection_changed |= icon_set_selected
			(container, icon,
			 is_in ^ icon->was_selected_before_rubberband);
	}

	if (selection_changed) {
		g_signal_emit (container,
				 signals[SELECTION_CHANGED], 0);
	}
}

static int
rubberband_timeout_callback (gpointer data)
{
	NolphinIconContainer *container;
	GtkWidget *widget;
	NolphinIconRubberbandInfo *band_info;
	int x, y;
	double x1, y1, x2, y2;
	double world_x, world_y;
	int x_scroll, y_scroll;
	int adj_x, adj_y;
	gboolean adj_changed;
	GtkAllocation allocation;

	EelDRect selection_rect;

	widget = GTK_WIDGET (data);
	container = NOLPHIN_ICON_CONTAINER (data);
	band_info = &container->details->rubberband_info;

	g_assert (band_info->timer_id != 0);

	adj_changed = FALSE;
	gtk_widget_get_allocation (widget, &allocation);

	adj_x = gtk_adjustment_get_value (gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (container)));
	if (adj_x != band_info->last_adj_x) {
		band_info->last_adj_x = adj_x;
		adj_changed = TRUE;
	}

	adj_y = gtk_adjustment_get_value (gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (container)));
	if (adj_y != band_info->last_adj_y) {
		band_info->last_adj_y = adj_y;
		adj_changed = TRUE;
	}

	gdk_window_get_device_position (gtk_widget_get_window (widget),
					gdk_device_manager_get_client_pointer (
						gdk_display_get_device_manager (
							gtk_widget_get_display (widget))),
					&x, &y, NULL);

	if (x < RUBBERBAND_SCROLL_THRESHOLD) {
		x_scroll = x - RUBBERBAND_SCROLL_THRESHOLD;
		x = 0;
	} else if (x >= allocation.width - RUBBERBAND_SCROLL_THRESHOLD) {
		x_scroll = x - allocation.width + RUBBERBAND_SCROLL_THRESHOLD + 1;
		x = allocation.width - 1;
	} else {
		x_scroll = 0;
	}

	if (y < RUBBERBAND_SCROLL_THRESHOLD) {
		y_scroll = y - RUBBERBAND_SCROLL_THRESHOLD;
		y = 0;
	} else if (y >= allocation.height - RUBBERBAND_SCROLL_THRESHOLD) {
		y_scroll = y - allocation.height + RUBBERBAND_SCROLL_THRESHOLD + 1;
		y = allocation.height - 1;
	} else {
		y_scroll = 0;
	}

	if (y_scroll == 0 && x_scroll == 0
	    && (int) band_info->prev_x == x && (int) band_info->prev_y == y && !adj_changed) {
		return TRUE;
	}

	nolphin_icon_container_scroll (container, x_scroll, y_scroll);

	/* Remember to convert from widget to scrolled window coords */
	eel_canvas_window_to_world (EEL_CANVAS (container),
				    x + gtk_adjustment_get_value (gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (container))),
				    y + gtk_adjustment_get_value (gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (container))),
				    &world_x, &world_y);

	if (world_x < band_info->start_x) {
		x1 = world_x;
		x2 = band_info->start_x;
	} else {
		x1 = band_info->start_x;
		x2 = world_x;
	}

	if (world_y < band_info->start_y) {
		y1 = world_y;
		y2 = band_info->start_y;
	} else {
		y1 = band_info->start_y;
		y2 = world_y;
	}

	/* Don't let the area of the selection rectangle be empty.
	 * Aside from the fact that it would be funny when the rectangle disappears,
	 * this also works around a crash in libart that happens sometimes when a
	 * zero height rectangle is passed.
	 */
	x2 = MAX (x1 + 1, x2);
	y2 = MAX (y1 + 1, y2);

	eel_canvas_item_set
		(band_info->selection_rectangle,
		 "x1", x1, "y1", y1,
		 "x2", x2, "y2", y2,
		 NULL);

	selection_rect.x0 = x1;
	selection_rect.y0 = y1;
	selection_rect.x1 = x2;
	selection_rect.y1 = y2;

	rubberband_select (container,
			   &band_info->prev_rect,
			   &selection_rect);

	band_info->prev_x = x;
	band_info->prev_y = y;

	band_info->prev_rect = selection_rect;

	return TRUE;
}

static void
start_rubberbanding (NolphinIconContainer *container,
		     GdkEventButton *event)
{
	AtkObject *accessible;
	NolphinIconContainerDetails *details;
	NolphinIconRubberbandInfo *band_info;
	GdkRGBA bg_color, border_color;
	GList *p;
	NolphinIcon *icon;
	GtkStyleContext *context;

	details = container->details;
	band_info = &details->rubberband_info;

	g_signal_emit (container,
		       signals[BAND_SELECT_STARTED], 0);

	for (p = details->icons; p != NULL; p = p->next) {
		icon = p->data;
		icon->was_selected_before_rubberband = icon->is_selected;
	}

	eel_canvas_window_to_world
		(EEL_CANVAS (container), event->x, event->y,
		 &band_info->start_x, &band_info->start_y);

	context = gtk_widget_get_style_context (GTK_WIDGET (container));
	gtk_style_context_save (context);
	gtk_style_context_add_class (context, GTK_STYLE_CLASS_RUBBERBAND);

	gtk_style_context_get_background_color (context, GTK_STATE_FLAG_NORMAL, &bg_color);
	gtk_style_context_get_border_color (context, GTK_STATE_FLAG_NORMAL, &border_color);

	gtk_style_context_restore (context);

	band_info->selection_rectangle = eel_canvas_item_new
		(eel_canvas_root
		 (EEL_CANVAS (container)),
		 NOLPHIN_TYPE_SELECTION_CANVAS_ITEM,
		 "x1", band_info->start_x,
		 "y1", band_info->start_y,
		 "x2", band_info->start_x,
		 "y2", band_info->start_y,
		 "fill_color_rgba", &bg_color,
		 "outline_color_rgba", &border_color,
		 "width_pixels", 1,
		 NULL);

	accessible = atk_gobject_accessible_for_object
		(G_OBJECT (band_info->selection_rectangle));
	atk_object_set_name (accessible, "selection");
	atk_object_set_description (accessible, _("Das Auswahlrechteck"));

	band_info->prev_x = event->x - gtk_adjustment_get_value (gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (container)));
	band_info->prev_y = event->y - gtk_adjustment_get_value (gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (container)));

	band_info->active = TRUE;

	if (band_info->timer_id == 0) {
		band_info->timer_id = g_timeout_add
			(RUBBERBAND_TIMEOUT_INTERVAL,
			 rubberband_timeout_callback,
			 container);
	}

	eel_canvas_item_grab (band_info->selection_rectangle,
				(GDK_POINTER_MOTION_MASK
				 | GDK_BUTTON_RELEASE_MASK
				 | GDK_SCROLL_MASK),
				NULL, event->time);
}

static void
stop_rubberbanding (NolphinIconContainer *container,
		    guint32 time)
{
	NolphinIconRubberbandInfo *band_info;
	GList *icons;
	gboolean enable_animation;

	band_info = &container->details->rubberband_info;

	g_assert (band_info->timer_id != 0);
	g_source_remove (band_info->timer_id);
	band_info->timer_id = 0;

	band_info->active = FALSE;

	g_object_get (gtk_settings_get_default (), "gtk-enable-animations", &enable_animation, NULL);

	/* Destroy this canvas item; the parent will unref it. */
	eel_canvas_item_ungrab (band_info->selection_rectangle, time);
	eel_canvas_item_lower_to_bottom (band_info->selection_rectangle);
	if (enable_animation) {
		nolphin_selection_canvas_item_fade_out (NOLPHIN_SELECTION_CANVAS_ITEM (band_info->selection_rectangle), 150);
	} else {
		eel_canvas_item_destroy (band_info->selection_rectangle);
	}
	band_info->selection_rectangle = NULL;

	/* if only one item has been selected, use it as range
	 * selection base (cf. handle_icon_button_press) */
	icons = nolphin_icon_container_get_selected_icons (container);
	if (g_list_length (icons) == 1) {
		container->details->range_selection_base_icon = icons->data;
	}
	g_list_free (icons);

	g_signal_emit (container,
			 signals[BAND_SELECT_ENDED], 0);
}

/* Keyboard navigation.  */

typedef gboolean (* IsBetterIconFunction) (NolphinIconContainer *container,
					   NolphinIcon *start_icon,
					   NolphinIcon *best_so_far,
					   NolphinIcon *candidate,
					   void *data);

static NolphinIcon *
find_best_icon (NolphinIconContainer *container,
		NolphinIcon *start_icon,
		IsBetterIconFunction function,
		void *data)
{
	GList *p;
	NolphinIcon *best, *candidate;

	best = NULL;
	for (p = container->details->icons; p != NULL; p = p->next) {
		candidate = p->data;

		if (candidate != start_icon) {
			if ((* function) (container, start_icon, best, candidate, data)) {
				best = candidate;
			}
		}
	}
	return best;
}

static NolphinIcon *
find_best_selected_icon (NolphinIconContainer *container,
			 NolphinIcon *start_icon,
			 IsBetterIconFunction function,
			 void *data)
{
	GList *p;
	NolphinIcon *best, *candidate;

	best = NULL;
	for (p = container->details->icons; p != NULL; p = p->next) {
		candidate = p->data;

		if (candidate != start_icon && candidate->is_selected) {
			if ((* function) (container, start_icon, best, candidate, data)) {
				best = candidate;
			}
		}
	}
	return best;
}

static int
compare_icons_by_uri (NolphinIconContainer *container,
		      NolphinIcon *icon_a,
		      NolphinIcon *icon_b)
{
	char *uri_a, *uri_b;
	int result;

	g_assert (NOLPHIN_IS_ICON_CONTAINER (container));
	g_assert (icon_a != NULL);
	g_assert (icon_b != NULL);
	g_assert (icon_a != icon_b);

	uri_a = nolphin_icon_container_get_icon_uri (container, icon_a);
	uri_b = nolphin_icon_container_get_icon_uri (container, icon_b);
	result = strcmp (uri_a, uri_b);
	g_assert (result != 0);
	g_free (uri_a);
	g_free (uri_b);

	return result;
}

static int
get_cmp_point_x (NolphinIconContainer *container,
		 EelDRect icon_rect)
{
	if (container->details->label_position == NOLPHIN_ICON_LABEL_POSITION_BESIDE) {
		if (gtk_widget_get_direction (GTK_WIDGET (container)) == GTK_TEXT_DIR_RTL) {
			return icon_rect.x0;
		} else {
			return icon_rect.x1;
		}
	} else {
		return (icon_rect.x0 + icon_rect.x1) / 2;
	}
}

static int
get_cmp_point_y (NolphinIconContainer *container,
		 EelDRect icon_rect)
{
	if (container->details->label_position == NOLPHIN_ICON_LABEL_POSITION_BESIDE) {
		return (icon_rect.y0 + icon_rect.y1)/2;
	} else {
		return icon_rect.y1;
	}
}


static int
compare_icons_horizontal (NolphinIconContainer *container,
			  NolphinIcon *icon_a,
			  NolphinIcon *icon_b)
{
	EelDRect world_rect;
	int ax, bx;

	world_rect = nolphin_icon_canvas_item_get_icon_rectangle (icon_a->item);
	eel_canvas_w2c
		(EEL_CANVAS (container),
		 get_cmp_point_x (container, world_rect),
		 get_cmp_point_y (container, world_rect),
		 &ax,
		 NULL);
	world_rect = nolphin_icon_canvas_item_get_icon_rectangle (icon_b->item);
	eel_canvas_w2c
		(EEL_CANVAS (container),
		 get_cmp_point_x (container, world_rect),
		 get_cmp_point_y (container, world_rect),
		 &bx,
		 NULL);

	if (ax < bx) {
		return -1;
	}
	if (ax > bx) {
		return +1;
	}
	return 0;
}

static int
compare_icons_vertical (NolphinIconContainer *container,
			NolphinIcon *icon_a,
			NolphinIcon *icon_b)
{
	EelDRect world_rect;
	int ay, by;

	world_rect = nolphin_icon_canvas_item_get_icon_rectangle (icon_a->item);
	eel_canvas_w2c
		(EEL_CANVAS (container),
		 get_cmp_point_x (container, world_rect),
		 get_cmp_point_y (container, world_rect),
		 NULL,
		 &ay);
	world_rect = nolphin_icon_canvas_item_get_icon_rectangle (icon_b->item);
	eel_canvas_w2c
		(EEL_CANVAS (container),
		 get_cmp_point_x (container, world_rect),
		 get_cmp_point_y (container, world_rect),
		 NULL,
		 &by);

	if (ay < by) {
		return -1;
	}
	if (ay > by) {
		return +1;
	}
	return 0;
}

static int
compare_icons_horizontal_first (NolphinIconContainer *container,
				NolphinIcon *icon_a,
				NolphinIcon *icon_b)
{
	EelDRect world_rect;
	int ax, ay, bx, by;

	world_rect = nolphin_icon_canvas_item_get_icon_rectangle (icon_a->item);
	eel_canvas_w2c
		(EEL_CANVAS (container),
		 get_cmp_point_x (container, world_rect),
		 get_cmp_point_y (container, world_rect),
		 &ax,
		 &ay);
	world_rect = nolphin_icon_canvas_item_get_icon_rectangle (icon_b->item);
	eel_canvas_w2c
		(EEL_CANVAS (container),
		 get_cmp_point_x (container, world_rect),
		 get_cmp_point_y (container, world_rect),
		 &bx,
		 &by);

	if (ax < bx) {
		return -1;
	}
	if (ax > bx) {
		return +1;
	}
	if (ay < by) {
		return -1;
	}
	if (ay > by) {
		return +1;
	}
	return compare_icons_by_uri (container, icon_a, icon_b);
}

static int
compare_icons_vertical_first (NolphinIconContainer *container,
			      NolphinIcon *icon_a,
			      NolphinIcon *icon_b)
{
	EelDRect world_rect;
	int ax, ay, bx, by;

	world_rect = nolphin_icon_canvas_item_get_icon_rectangle (icon_a->item);
	eel_canvas_w2c
		(EEL_CANVAS (container),
		 get_cmp_point_x (container, world_rect),
		 get_cmp_point_y (container, world_rect),
		 &ax,
		 &ay);
	world_rect = nolphin_icon_canvas_item_get_icon_rectangle (icon_b->item);
	eel_canvas_w2c
		(EEL_CANVAS (container),
		 get_cmp_point_x (container, world_rect),
		 get_cmp_point_y (container, world_rect),
		 &bx,
		 &by);

	if (ay < by) {
		return -1;
	}
	if (ay > by) {
		return +1;
	}
	if (ax < bx) {
		return -1;
	}
	if (ax > bx) {
		return +1;
	}
	return compare_icons_by_uri (container, icon_a, icon_b);
}

static gboolean
leftmost_in_top_row (NolphinIconContainer *container,
		     NolphinIcon *start_icon,
		     NolphinIcon *best_so_far,
		     NolphinIcon *candidate,
		     void *data)
{
	if (best_so_far == NULL) {
		return TRUE;
	}
	return compare_icons_vertical_first (container, best_so_far, candidate) > 0;
}

static gboolean
rightmost_in_top_row (NolphinIconContainer *container,
		      NolphinIcon *start_icon,
		      NolphinIcon *best_so_far,
		      NolphinIcon *candidate,
		      void *data)
{
	if (best_so_far == NULL) {
		return TRUE;
	}
	return compare_icons_vertical (container, best_so_far, candidate) > 0;
	return compare_icons_horizontal (container, best_so_far, candidate) < 0;
}

static gboolean
rightmost_in_bottom_row (NolphinIconContainer *container,
			 NolphinIcon *start_icon,
			 NolphinIcon *best_so_far,
			 NolphinIcon *candidate,
			 void *data)
{
	if (best_so_far == NULL) {
		return TRUE;
	}
	return compare_icons_vertical_first (container, best_so_far, candidate) < 0;
}

static int
compare_with_start_row (NolphinIconContainer *container,
			NolphinIcon *icon)
{
	EelCanvasItem *item;

	item = EEL_CANVAS_ITEM (icon->item);

	if (container->details->arrow_key_start_y < item->y1) {
		return -1;
	}
	if (container->details->arrow_key_start_y > item->y2) {
		return +1;
	}
	return 0;
}

static int
compare_with_start_column (NolphinIconContainer *container,
			   NolphinIcon *icon)
{
	EelCanvasItem *item;

	item = EEL_CANVAS_ITEM (icon->item);

	if (container->details->arrow_key_start_x < item->x1) {
		return -1;
	}
	if (container->details->arrow_key_start_x > item->x2) {
		return +1;
	}
	return 0;
}

static gboolean
same_row_right_side_leftmost (NolphinIconContainer *container,
			      NolphinIcon *start_icon,
			      NolphinIcon *best_so_far,
			      NolphinIcon *candidate,
			      void *data)
{
	/* Candidates not on the start row do not qualify. */
	if (compare_with_start_row (container, candidate) != 0) {
		return FALSE;
	}

	/* Candidates that are farther right lose out. */
	if (best_so_far != NULL) {
		if (compare_icons_horizontal_first (container,
						    best_so_far,
						    candidate) < 0) {
			return FALSE;
		}
	}

	/* Candidate to the left of the start do not qualify. */
	if (compare_icons_horizontal_first (container,
					    candidate,
					    start_icon) <= 0) {
		return FALSE;
	}

	return TRUE;
}

static gboolean
same_row_left_side_rightmost (NolphinIconContainer *container,
			      NolphinIcon *start_icon,
			      NolphinIcon *best_so_far,
			      NolphinIcon *candidate,
			      void *data)
{
	/* Candidates not on the start row do not qualify. */
	if (compare_with_start_row (container, candidate) != 0) {
		return FALSE;
	}

	/* Candidates that are farther left lose out. */
	if (best_so_far != NULL) {
		if (compare_icons_horizontal_first (container,
						    best_so_far,
						    candidate) > 0) {
			return FALSE;
		}
	}

	/* Candidate to the right of the start do not qualify. */
	if (compare_icons_horizontal_first (container,
					    candidate,
					    start_icon) >= 0) {
		return FALSE;
	}

	return TRUE;
}

static gboolean
next_row_leftmost (NolphinIconContainer *container,
		   NolphinIcon *start_icon,
	           NolphinIcon *best_so_far,
		   NolphinIcon *candidate,
		   void *data)
{
	/* sort out icons that are not below the current row */
	if (compare_with_start_row (container, candidate) >= 0) {
		return FALSE;
	}

	if (best_so_far != NULL) {
		if (compare_icons_vertical_first (container,
						  best_so_far,
						  candidate) > 0) {
			/* candidate is above best choice, but below the current row */
			return TRUE;
		}

		if (compare_icons_horizontal_first (container,
						    best_so_far,
						    candidate) > 0) {
			return TRUE;
		}
	}

	return best_so_far == NULL;
}

static gboolean
next_row_rightmost (NolphinIconContainer *container,
		    NolphinIcon *start_icon,
		    NolphinIcon *best_so_far,
		    NolphinIcon *candidate,
		    void *data)
{
	/* sort out icons that are not below the current row */
	if (compare_with_start_row (container, candidate) >= 0) {
		return FALSE;
	}

	if (best_so_far != NULL) {
		if (compare_icons_vertical_first (container,
						  best_so_far,
						  candidate) > 0) {
			/* candidate is above best choice, but below the current row */
			return TRUE;
		}

		if (compare_icons_horizontal_first (container,
						    best_so_far,
						    candidate) < 0) {
			return TRUE;
		}
	}

	return best_so_far == NULL;
}

static gboolean
next_column_bottommost (NolphinIconContainer *container,
			NolphinIcon *start_icon,
			NolphinIcon *best_so_far,
			NolphinIcon *candidate,
			void *data)
{
	/* sort out icons that are not on the right of the current column */
	if (compare_with_start_column (container, candidate) >= 0) {
		return FALSE;
	}

	if (best_so_far != NULL) {
		if (compare_icons_horizontal_first (container,
						  best_so_far,
						  candidate) > 0) {
			/* candidate is above best choice, but below the current row */
			return TRUE;
		}

		if (compare_icons_vertical_first (container,
						  best_so_far,
						  candidate) < 0) {
			return TRUE;
		}
	}

	return best_so_far == NULL;
}

static gboolean
previous_row_rightmost (NolphinIconContainer *container,
		        NolphinIcon *start_icon,
			NolphinIcon *best_so_far,
			NolphinIcon *candidate,
			void *data)
{
	/* sort out icons that are not above the current row */
	if (compare_with_start_row (container, candidate) <= 0) {
		return FALSE;
	}

	if (best_so_far != NULL) {
		if (compare_icons_vertical_first (container,
						  best_so_far,
						  candidate) < 0) {
			/* candidate is below the best choice, but above the current row */
			return TRUE;
		}

		if (compare_icons_horizontal_first (container,
						    best_so_far,
						    candidate) < 0) {
			return TRUE;
		}
	}

	return best_so_far == NULL;
}

static gboolean
same_column_above_lowest (NolphinIconContainer *container,
			  NolphinIcon *start_icon,
			  NolphinIcon *best_so_far,
			  NolphinIcon *candidate,
			  void *data)
{
	/* Candidates not on the start column do not qualify. */
	if (compare_with_start_column (container, candidate) != 0) {
		return FALSE;
	}

	/* Candidates that are higher lose out. */
	if (best_so_far != NULL) {
		if (compare_icons_vertical_first (container,
						  best_so_far,
						  candidate) > 0) {
			return FALSE;
		}
	}

	/* Candidates below the start do not qualify. */
	if (compare_icons_vertical_first (container,
					  candidate,
					  start_icon) >= 0) {
		return FALSE;
	}

	return TRUE;
}

static gboolean
same_column_below_highest (NolphinIconContainer *container,
			   NolphinIcon *start_icon,
			   NolphinIcon *best_so_far,
			   NolphinIcon *candidate,
			   void *data)
{
	/* Candidates not on the start column do not qualify. */
	if (compare_with_start_column (container, candidate) != 0) {
		return FALSE;
	}

	/* Candidates that are lower lose out. */
	if (best_so_far != NULL) {
		if (compare_icons_vertical_first (container,
						  best_so_far,
						  candidate) < 0) {
			return FALSE;
		}
	}

	/* Candidates above the start do not qualify. */
	if (compare_icons_vertical_first (container,
					  candidate,
					  start_icon) <= 0) {
		return FALSE;
	}

	return TRUE;
}

static gboolean
previous_column_highest (NolphinIconContainer *container,
			 NolphinIcon *start_icon,
			 NolphinIcon *best_so_far,
			 NolphinIcon *candidate,
			 void *data)
{
	/* sort out icons that are not before the current column */
	if (compare_with_start_column (container, candidate) <= 0) {
		return FALSE;
	}

	if (best_so_far != NULL) {
		if (compare_icons_horizontal (container,
					      best_so_far,
					      candidate) < 0) {
			/* candidate is right of the best choice, but left of the current column */
			return TRUE;
		}

		if (compare_icons_vertical (container,
					    best_so_far,
					    candidate) > 0) {
			return TRUE;
		}
	}

	return best_so_far == NULL;
}


static gboolean
next_column_highest (NolphinIconContainer *container,
		     NolphinIcon *start_icon,
		     NolphinIcon *best_so_far,
		     NolphinIcon *candidate,
		     void *data)
{
	/* sort out icons that are not after the current column */
	if (compare_with_start_column (container, candidate) >= 0) {
		return FALSE;
	}

	if (best_so_far != NULL) {
		if (compare_icons_horizontal_first (container,
						    best_so_far,
						    candidate) > 0) {
			/* candidate is left of the best choice, but right of the current column */
			return TRUE;
		}

		if (compare_icons_vertical_first (container,
						  best_so_far,
						  candidate) > 0) {
			return TRUE;
		}
	}

	return best_so_far == NULL;
}

static gboolean
previous_column_lowest (NolphinIconContainer *container,
		        NolphinIcon *start_icon,
			NolphinIcon *best_so_far,
			NolphinIcon *candidate,
			void *data)
{
	/* sort out icons that are not before the current column */
	if (compare_with_start_column (container, candidate) <= 0) {
		return FALSE;
	}

	if (best_so_far != NULL) {
		if (compare_icons_horizontal_first (container,
						    best_so_far,
						    candidate) < 0) {
			/* candidate is right of the best choice, but left of the current column */
			return TRUE;
		}

		if (compare_icons_vertical_first (container,
						  best_so_far,
						  candidate) < 0) {
			return TRUE;
		}
	}

	return best_so_far == NULL;
}

static gboolean
last_column_lowest (NolphinIconContainer *container,
		    NolphinIcon *start_icon,
		    NolphinIcon *best_so_far,
		    NolphinIcon *candidate,
		    void *data)
{
	if (best_so_far == NULL) {
		return TRUE;
	}
	return compare_icons_horizontal_first (container, best_so_far, candidate) < 0;
}

static gboolean
closest_in_90_degrees (NolphinIconContainer *container,
		       NolphinIcon *start_icon,
		       NolphinIcon *best_so_far,
		       NolphinIcon *candidate,
		       void *data)
{
	EelDRect world_rect;
	int x, y;
	int dx, dy;
	int dist;
	int *best_dist;


	world_rect = nolphin_icon_canvas_item_get_icon_rectangle (candidate->item);
	eel_canvas_w2c
		(EEL_CANVAS (container),
		 get_cmp_point_x (container, world_rect),
		 get_cmp_point_y (container, world_rect),
		 &x,
		 &y);

	dx = x - container->details->arrow_key_start_x;
	dy = y - container->details->arrow_key_start_y;

	switch (container->details->arrow_key_direction) {
	case GTK_DIR_UP:
		if (dy > 0 ||
		    ABS(dx) > ABS(dy)) {
			return FALSE;
		}
		break;
	case GTK_DIR_DOWN:
		if (dy < 0 ||
		    ABS(dx) > ABS(dy)) {
			return FALSE;
		}
		break;
	case GTK_DIR_LEFT:
		if (dx > 0 ||
		    ABS(dy) > ABS(dx)) {
			return FALSE;
		}
		break;
	case GTK_DIR_RIGHT:
		if (dx < 0 ||
		    ABS(dy) > ABS(dx)) {
			return FALSE;
		}
		break;
    case GTK_DIR_TAB_FORWARD:
    case GTK_DIR_TAB_BACKWARD:
	default:
		g_assert_not_reached();
	}

	dist = dx*dx + dy*dy;
	best_dist = data;

	if (best_so_far == NULL) {
		*best_dist = dist;
		return TRUE;
	}

	if (dist < *best_dist) {
		*best_dist = dist;
		return TRUE;
	}

	return FALSE;
}

static EelDRect
get_rubberband (NolphinIcon *icon1,
		NolphinIcon *icon2)
{
	EelDRect rect1;
	EelDRect rect2;
	EelDRect ret;

	eel_canvas_item_get_bounds (EEL_CANVAS_ITEM (icon1->item),
				    &rect1.x0, &rect1.y0,
				    &rect1.x1, &rect1.y1);
	eel_canvas_item_get_bounds (EEL_CANVAS_ITEM (icon2->item),
				    &rect2.x0, &rect2.y0,
				    &rect2.x1, &rect2.y1);

	eel_drect_union (&ret, &rect1, &rect2);

	return ret;
}

static void
keyboard_move_to (NolphinIconContainer *container,
		  NolphinIcon *icon,
		  NolphinIcon *from,
		  GdkEventKey *event)
{
	if (icon == NULL) {
		return;
	}

	if (event != NULL &&
	    (event->state & GDK_CONTROL_MASK) != 0 &&
	    (event->state & GDK_SHIFT_MASK) == 0) {
		/* Move the keyboard focus. Use Control modifier
		 * rather than Alt to avoid Sawfish conflict.
		 */
		set_keyboard_focus (container, icon);
		container->details->keyboard_rubberband_start = NULL;
	} else if (event != NULL &&
		   ((event->state & GDK_CONTROL_MASK) != 0 ||
		    !container->details->auto_layout) &&
		   (event->state & GDK_SHIFT_MASK) != 0) {
		/* Do rubberband selection */
		EelDRect rect;

		if (from && !container->details->keyboard_rubberband_start) {
			set_keyboard_rubberband_start (container, from);
		}

		set_keyboard_focus (container, icon);

		if (icon && container->details->keyboard_rubberband_start) {
			rect = get_rubberband (container->details->keyboard_rubberband_start,
					       icon);
			rubberband_select (container, NULL, &rect);
		}
	} else if (event != NULL &&
		   (event->state & GDK_CONTROL_MASK) == 0 &&
		   (event->state & GDK_SHIFT_MASK) != 0) {
		/* Select range */
		NolphinIcon *start_icon;

		start_icon = container->details->range_selection_base_icon;
		if (start_icon == NULL || !start_icon->is_selected) {
			start_icon = icon;
			container->details->range_selection_base_icon = icon;
		}

		set_keyboard_focus (container, icon);

		if (select_range (container, start_icon, icon, TRUE)) {
			g_signal_emit (container,
				       signals[SELECTION_CHANGED], 0);
		}
	} else {
		/* Select icons and get rid of the special keyboard focus. */
		clear_keyboard_focus (container);
		clear_keyboard_rubberband_start (container);

		container->details->range_selection_base_icon = icon;
		if (select_one_unselect_others (container, icon)) {
			g_signal_emit (container,
					 signals[SELECTION_CHANGED], 0);
		}
	}
	schedule_keyboard_icon_reveal (container, icon);
}

static void
keyboard_home (NolphinIconContainer *container,
	       GdkEventKey *event)
{
	NolphinIcon *from;
	NolphinIcon *to;

	/* Home selects the first icon.
	 * Control-Home sets the keyboard focus to the first icon.
	 */

	from = find_best_selected_icon (container, NULL,
					rightmost_in_bottom_row,
					NULL);
	to = find_best_icon (container, NULL, leftmost_in_top_row, NULL);

	keyboard_move_to (container, to, from, event);
}

static void
keyboard_end (NolphinIconContainer *container,
	      GdkEventKey *event)
{
	NolphinIcon *to;
	NolphinIcon *from;

	/* End selects the last icon.
	 * Control-End sets the keyboard focus to the last icon.
	 */
	from = find_best_selected_icon (container, NULL,
					leftmost_in_top_row,
					NULL);
	to = find_best_icon (container, NULL,
			     nolphin_icon_container_is_layout_vertical (container) ?
			     last_column_lowest :
			     rightmost_in_bottom_row,
			     NULL);

	keyboard_move_to (container, to, from, event);
}

static void
record_arrow_key_start (NolphinIconContainer *container,
			NolphinIcon *icon,
			GtkDirectionType direction)
{
	EelDRect world_rect;

	world_rect = nolphin_icon_canvas_item_get_icon_rectangle (icon->item);
	eel_canvas_w2c
		(EEL_CANVAS (container),
		 get_cmp_point_x (container, world_rect),
		 get_cmp_point_y (container, world_rect),
		 &container->details->arrow_key_start_x,
		 &container->details->arrow_key_start_y);
	container->details->arrow_key_direction = direction;
}

static void
keyboard_arrow_key (NolphinIconContainer *container,
		    GdkEventKey *event,
		    GtkDirectionType direction,
		    IsBetterIconFunction better_start,
		    IsBetterIconFunction empty_start,
		    IsBetterIconFunction better_destination,
		    IsBetterIconFunction better_destination_fallback,
		    IsBetterIconFunction better_destination_fallback_fallback,
		    IsBetterIconFunction better_destination_manual)
{
	NolphinIcon *from;
	NolphinIcon *to;
	int data;

	/* Chose the icon to start with.
	 * If we have a keyboard focus, start with it.
	 * Otherwise, use the single selected icon.
	 * If there's multiple selection, use the icon farthest toward the end.
	 */

	from = container->details->keyboard_focus;

	if (from == NULL) {
		if (has_multiple_selection (container)) {
			if (all_selected (container)) {
				from = find_best_selected_icon
					(container, NULL,
					 empty_start, NULL);
			} else {
				from = find_best_selected_icon
					(container, NULL,
					 better_start, NULL);
			}
		} else {
			from = get_first_selected_icon (container);
		}
	}

	/* If there's no icon, select the icon farthest toward the end.
	 * If there is an icon, select the next icon based on the arrow direction.
	 */
	if (from == NULL) {
		to = from = find_best_icon
			(container, NULL,
			 empty_start, NULL);
	} else {
		record_arrow_key_start (container, from, direction);

		to = find_best_icon
			(container, from,
			 container->details->auto_layout ? better_destination : better_destination_manual,
			 &data);

		/* Wrap around to next/previous row/column */
		if (to == NULL &&
		    better_destination_fallback != NULL) {
			to = find_best_icon
				(container, from,
				 better_destination_fallback,
				 &data);
		}

		/* With a layout like
		 * 1 2 3
		 * 4
		 * (horizontal layout)
		 *
		 * or
		 *
		 * 1 4
		 * 2
		 * 3
		 * (vertical layout)
		 *
		 * * pressing down for any of 1,2,3 (horizontal layout)
		 * * pressing right for any of 1,2,3 (vertical layout)
		 *
		 * Should select 4.
		 */
		if (to == NULL &&
		    container->details->auto_layout &&
		    better_destination_fallback_fallback != NULL) {
			to = find_best_icon
				(container, from,
				 better_destination_fallback_fallback,
				 &data);
		}

		if (to == NULL) {
			to = from;
		}

	}

	keyboard_move_to (container, to, from, event);
}

static gboolean
is_rectangle_selection_event (GdkEventKey *event)
{
	return (event->state & GDK_CONTROL_MASK) != 0 &&
	       (event->state & GDK_SHIFT_MASK) != 0;
}

static void
keyboard_right (NolphinIconContainer *container,
		GdkEventKey *event)
{
	IsBetterIconFunction fallback;
	IsBetterIconFunction next_column_fallback;

	fallback = NULL;
	if (container->details->auto_layout &&
	    !nolphin_icon_container_is_layout_vertical (container) &&
	    !is_rectangle_selection_event (event)) {
		fallback = next_row_leftmost;
	}

	next_column_fallback = NULL;
	if (nolphin_icon_container_is_layout_vertical (container) &&
	    gtk_widget_get_direction (GTK_WIDGET (container)) != GTK_TEXT_DIR_RTL) {
		next_column_fallback = next_column_bottommost;
	}

	/* Right selects the next icon in the same row.
	 * Control-Right sets the keyboard focus to the next icon in the same row.
	 */
	keyboard_arrow_key (container,
			    event,
			    GTK_DIR_RIGHT,
			    rightmost_in_bottom_row,
			    nolphin_icon_container_is_layout_rtl (container) ?
			    rightmost_in_top_row : leftmost_in_top_row,
			    same_row_right_side_leftmost,
			    fallback,
			    next_column_fallback,
			    closest_in_90_degrees);
}

static void
keyboard_left (NolphinIconContainer *container,
	       GdkEventKey *event)
{
	IsBetterIconFunction fallback;
	IsBetterIconFunction previous_column_fallback;

	fallback = NULL;
	if (container->details->auto_layout &&
	    !nolphin_icon_container_is_layout_vertical (container) &&
	    !is_rectangle_selection_event (event)) {
		fallback = previous_row_rightmost;
	}

	previous_column_fallback = NULL;
	if (nolphin_icon_container_is_layout_vertical (container) &&
	    gtk_widget_get_direction (GTK_WIDGET (container)) == GTK_TEXT_DIR_RTL) {
		previous_column_fallback = previous_column_lowest;
	}

	/* Left selects the next icon in the same row.
	 * Control-Left sets the keyboard focus to the next icon in the same row.
	 */
	keyboard_arrow_key (container,
			    event,
			    GTK_DIR_LEFT,
			    rightmost_in_bottom_row,
			    nolphin_icon_container_is_layout_rtl (container) ?
			    rightmost_in_top_row : leftmost_in_top_row,
			    same_row_left_side_rightmost,
			    fallback,
			    previous_column_fallback,
			    closest_in_90_degrees);
}

static void
keyboard_down (NolphinIconContainer *container,
	       GdkEventKey *event)
{
	IsBetterIconFunction fallback;
	IsBetterIconFunction next_row_fallback;

	fallback = NULL;
	if (container->details->auto_layout &&
	    nolphin_icon_container_is_layout_vertical (container) &&
	    !is_rectangle_selection_event (event)) {
		if (gtk_widget_get_direction (GTK_WIDGET (container)) == GTK_TEXT_DIR_RTL) {
			fallback = previous_column_highest;
		} else {
			fallback = next_column_highest;
		}
	}

	next_row_fallback = NULL;
	if (!nolphin_icon_container_is_layout_vertical (container)) {
		if (gtk_widget_get_direction (GTK_WIDGET (container)) == GTK_TEXT_DIR_RTL) {
			next_row_fallback = next_row_leftmost;
		} else {
			next_row_fallback = next_row_rightmost;
		}
	}

	/* Down selects the next icon in the same column.
	 * Control-Down sets the keyboard focus to the next icon in the same column.
	 */
	keyboard_arrow_key (container,
			    event,
			    GTK_DIR_DOWN,
			    rightmost_in_bottom_row,
			    nolphin_icon_container_is_layout_rtl (container) ?
			    rightmost_in_top_row : leftmost_in_top_row,
			    same_column_below_highest,
			    fallback,
			    next_row_fallback,
			    closest_in_90_degrees);
}

static void
keyboard_up (NolphinIconContainer *container,
	     GdkEventKey *event)
{
	IsBetterIconFunction fallback;

	fallback = NULL;
	if (container->details->auto_layout &&
	    nolphin_icon_container_is_layout_vertical (container) &&
	    !is_rectangle_selection_event (event)) {
		if (gtk_widget_get_direction (GTK_WIDGET (container)) == GTK_TEXT_DIR_RTL) {
			fallback = next_column_bottommost;
		} else {
			fallback = previous_column_lowest;
		}
	}

	/* Up selects the next icon in the same column.
	 * Control-Up sets the keyboard focus to the next icon in the same column.
	 */
	keyboard_arrow_key (container,
			    event,
			    GTK_DIR_UP,
			    rightmost_in_bottom_row,
			    nolphin_icon_container_is_layout_rtl (container) ?
			    rightmost_in_top_row : leftmost_in_top_row,
			    same_column_above_lowest,
			    fallback,
			    NULL,
			    closest_in_90_degrees);
}

static void
keyboard_space (NolphinIconContainer *container,
		GdkEventKey *event)
{
	NolphinIcon *icon;

	if (!has_selection (container) &&
	    container->details->keyboard_focus != NULL) {
		keyboard_move_to (container,
				  container->details->keyboard_focus,
				  NULL, NULL);
	} else if ((event->state & GDK_CONTROL_MASK) != 0 &&
		   (event->state & GDK_SHIFT_MASK) == 0) {
		/* Control-space toggles the selection state of the current icon. */
		if (container->details->keyboard_focus != NULL) {
			icon_toggle_selected (container, container->details->keyboard_focus);
			g_signal_emit (container, signals[SELECTION_CHANGED], 0);
			if  (container->details->keyboard_focus->is_selected) {
				container->details->range_selection_base_icon = container->details->keyboard_focus;
			}
		} else {
			icon = find_best_selected_icon (container,
							NULL,
							leftmost_in_top_row,
							NULL);
			if (icon == NULL) {
				icon = find_best_icon (container,
						       NULL,
						       leftmost_in_top_row,
						       NULL);
			}
			if (icon != NULL) {
				set_keyboard_focus (container, icon);
			}
		}
	} else if ((event->state & GDK_SHIFT_MASK) != 0) {
		activate_selected_items_alternate (container, NULL);
	} else {
		preview_selected_items (container);
	}
}

/* look for the first icon that matches the longest part of a given
 * search pattern
 */
typedef struct {
	gunichar *name;
	int last_match_length;
} BestNameMatch;

#ifndef TAB_NAVIGATION_DISABLED
static void
select_previous_or_next_icon (NolphinIconContainer *container,
			      gboolean next,
			      GdkEventKey *event)
{
	NolphinIcon *icon;
	const GList *item;

	item = NULL;
	/* Chose the icon to start with.
	 * If we have a keyboard focus, start with it.
	 * Otherwise, use the single selected icon.
	 */
	icon = container->details->keyboard_focus;
	if (icon == NULL) {
		icon = get_first_selected_icon (container);
	}

	if (icon != NULL) {
		/* must have at least @icon in the list */
		g_assert (container->details->icons != NULL);
		item = g_list_find (container->details->icons, icon);
		g_assert (item != NULL);

		item = next ? item->next : item->prev;
		if (item == NULL) {
			item = next ? g_list_first (container->details->icons) : g_list_last (container->details->icons);
		}

	} else if (container->details->icons != NULL) {
		/* no selection yet, pick the first or last item to select */
		item = next ? g_list_first (container->details->icons) : g_list_last (container->details->icons);
	}

	icon = (item != NULL) ? item->data : NULL;

	if (icon != NULL) {
		keyboard_move_to (container, icon, NULL, event);
	}
}
#endif

static void
destroy (GtkWidget *object)
{
	NolphinIconContainer *container;

	container = NOLPHIN_ICON_CONTAINER (object);

        nolphin_icon_container_clear (container);

	if (container->details->rubberband_info.timer_id != 0) {
		g_source_remove (container->details->rubberband_info.timer_id);
		container->details->rubberband_info.timer_id = 0;
	}

        if (container->details->idle_id != 0) {
		g_source_remove (container->details->idle_id);
		container->details->idle_id = 0;
	}

	if (container->details->stretch_idle_id != 0) {
		g_source_remove (container->details->stretch_idle_id);
		container->details->stretch_idle_id = 0;
	}

        if (container->details->align_idle_id != 0) {
		g_source_remove (container->details->align_idle_id);
		container->details->align_idle_id = 0;
	}

        if (container->details->selection_changed_id != 0) {
		g_source_remove (container->details->selection_changed_id);
		container->details->selection_changed_id = 0;
	}

        if (container->details->size_allocation_count_id != 0) {
		g_source_remove (container->details->size_allocation_count_id);
		container->details->size_allocation_count_id = 0;
	}

	/* destroy interactive search dialog */
	if (container->details->search_window) {
		gtk_widget_destroy (container->details->search_window);
		container->details->search_window = NULL;
		container->details->search_entry = NULL;
	}

	remove_search_entry_timeout (container);

	GTK_WIDGET_CLASS (nolphin_icon_container_parent_class)->destroy (object);
}

static void
finalize (GObject *object)
{
	NolphinIconContainerDetails *details;

	details = NOLPHIN_ICON_CONTAINER (object)->details;

	g_signal_handlers_disconnect_by_func (nolphin_icon_view_preferences,
					      text_ellipsis_limit_changed_container_callback,
					      object);
	g_signal_handlers_disconnect_by_func (nolphin_desktop_preferences,
					      text_ellipsis_limit_changed_container_callback,
					      object);

    g_signal_handlers_disconnect_by_func (nolphin_preferences,
                                          tooltip_prefs_changed_callback,
                                          object);

	g_hash_table_destroy (details->icon_set);
	details->icon_set = NULL;

	g_free (details->font);
	g_free (details->filter_highlight_text);

	if (details->a11y_item_action_queue != NULL) {
		while (!g_queue_is_empty (details->a11y_item_action_queue)) {
			g_free (g_queue_pop_head (details->a11y_item_action_queue));
		}
		g_queue_free (details->a11y_item_action_queue);
	}
	if (details->a11y_item_action_idle_handler != 0) {
		g_source_remove (details->a11y_item_action_idle_handler);
	}

    g_free (details->view_constants);

    g_list_free (details->current_selection);
    g_free(details);

	G_OBJECT_CLASS (nolphin_icon_container_parent_class)->finalize (object);
}

/* GtkWidget methods.  */

static gboolean
clear_size_allocation_count (gpointer data)
{
	NolphinIconContainer *container;

	container = NOLPHIN_ICON_CONTAINER (data);

	container->details->size_allocation_count_id = 0;
	container->details->size_allocation_count = 0;

	return FALSE;
}

static void
size_allocate (GtkWidget *widget,
	       GtkAllocation *allocation)
{
	NolphinIconContainer *container;
	gboolean need_layout_redone;
	GtkAllocation wid_allocation;

	container = NOLPHIN_ICON_CONTAINER (widget);

	need_layout_redone = !container->details->has_been_allocated;
	gtk_widget_get_allocation (widget, &wid_allocation);

	if (allocation->width != wid_allocation.width) {
		need_layout_redone = TRUE;
	}

	if (allocation->height != wid_allocation.height) {
		need_layout_redone = TRUE;
	}

	/* Under some conditions we can end up in a loop when size allocating.
	 * This happens when the icons don't fit without a scrollbar, but fits
	 * when a scrollbar is added (bug #129963 for details).
	 * We keep track of this looping by increasing a counter in size_allocate
	 * and clearing it in a high-prio idle (the only way to detect the loop is
	 * done).
	 * When we've done at more than two iterations (with/without scrollbar)
	 * we terminate this looping by not redoing the layout when the width
	 * is wider than the current one (i.e when removing the scrollbar).
	 */
	if (container->details->size_allocation_count_id == 0) {
		container->details->size_allocation_count_id =
			g_idle_add_full  (G_PRIORITY_HIGH,
					  clear_size_allocation_count,
					  container, NULL);
	}
	container->details->size_allocation_count++;
	if (container->details->size_allocation_count > 2 &&
	    allocation->width >= wid_allocation.width) {
		need_layout_redone = FALSE;
	}

    if (is_renaming (container)) {
        container->details->renaming_allocation_count++;

        if (container->details->renaming_allocation_count == 1) {
            need_layout_redone = FALSE;
        }
    }

	GTK_WIDGET_CLASS (nolphin_icon_container_parent_class)->size_allocate (widget, allocation);

	container->details->has_been_allocated = TRUE;

	if (need_layout_redone) {
		nolphin_icon_container_redo_layout (container);
	}
}

static GtkSizeRequestMode
get_request_mode (GtkWidget *widget)
{
  /* Don't trade size at all, since we get whatever we get anyway. */
  return GTK_SIZE_REQUEST_CONSTANT_SIZE;
}

/* We need to implement these since the GtkScrolledWindow uses them
   to guess whether to show scrollbars or not, and if we don't report
   anything it'll tend to get it wrong causing double calls
   to size_allocate (at different sizes) during its size allocation. */
static void
get_prefered_width (GtkWidget *widget,
		    gint      *minimum_size,
		    gint      *natural_size)
{
	EelCanvasGroup *root;
	double x1, x2;
	int cx1, cx2;
	int width;

	root = eel_canvas_root (EEL_CANVAS (widget));
	eel_canvas_item_get_bounds (EEL_CANVAS_ITEM (root),
				    &x1, NULL, &x2, NULL);
	eel_canvas_w2c (EEL_CANVAS (widget), x1, 0, &cx1, NULL);
	eel_canvas_w2c (EEL_CANVAS (widget), x2, 0, &cx2, NULL);

	width = cx2 - cx1;
	if (natural_size) {
		*natural_size = width;
	}
	if (minimum_size) {
		*minimum_size = width;
	}
}

static void
get_prefered_height (GtkWidget *widget,
		     gint      *minimum_size,
		     gint      *natural_size)
{
	EelCanvasGroup *root;
	double y1, y2;
	int cy1, cy2;
	int height;

	root = eel_canvas_root (EEL_CANVAS (widget));
	eel_canvas_item_get_bounds (EEL_CANVAS_ITEM (root),
				    NULL, &y1, NULL, &y2);
	eel_canvas_w2c (EEL_CANVAS (widget), 0, y1, NULL, &cy1);
	eel_canvas_w2c (EEL_CANVAS (widget), 0, y2, NULL, &cy2);

	height = cy2 - cy1;
	if (natural_size) {
		*natural_size = height;
	}
	if (minimum_size) {
		*minimum_size = height;
	}
}

static void
realize (GtkWidget *widget)
{
	GtkAdjustment *vadj, *hadj;
	NolphinIconContainer *container;

	GTK_WIDGET_CLASS (nolphin_icon_container_parent_class)->realize (widget);

	container = NOLPHIN_ICON_CONTAINER (widget);

	/* Set up DnD.  */
	nolphin_icon_dnd_init (container);

	hadj = gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (widget));
	g_signal_connect (hadj, "value_changed",
			  G_CALLBACK (handle_hadjustment_changed), widget);

	vadj = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (widget));
	g_signal_connect (vadj, "value_changed",
			  G_CALLBACK (handle_vadjustment_changed), widget);

}

static void
unrealize (GtkWidget *widget)
{
	NolphinIconContainer *container;

	container = NOLPHIN_ICON_CONTAINER (widget);

	nolphin_icon_dnd_fini (container);
	remove_search_entry_timeout (container);

	GTK_WIDGET_CLASS (nolphin_icon_container_parent_class)->unrealize (widget);
}

static void
style_updated (GtkWidget *widget)
{
	NolphinIconContainer *container;

	container = NOLPHIN_ICON_CONTAINER (widget);
	container->details->use_drop_shadows = container->details->drop_shadows_requested;

	/* Don't chain up to parent, if this is a desktop container,
	 * because that resets the background of the window.
	 */
	if (!nolphin_icon_container_get_is_desktop (container)) {
		GTK_WIDGET_CLASS (nolphin_icon_container_parent_class)->style_updated (widget);
	}

	if (gtk_widget_get_realized (widget)) {
		nolphin_icon_container_invalidate_labels (container);
		nolphin_icon_container_request_update_all (container);
	}
}

static gboolean
button_press_event (GtkWidget *widget,
		    GdkEventButton *event)
{
	NolphinIconContainer *container;
	gboolean selection_changed;
	gboolean return_value;
	gboolean clicked_on_item;

	container = NOLPHIN_ICON_CONTAINER (widget);
        container->details->button_down_time = event->time;

        /* Forget about the old keyboard selection now that we've started mousing. */
        clear_keyboard_focus (container);
	clear_keyboard_rubberband_start (container);

       // hide and clear the type-ahead search when a mouse click occur
       if (event->type == GDK_BUTTON_PRESS && container->details->search_window){
           remove_search_entry_timeout (container);
           gtk_widget_hide (container->details->search_window);
           gtk_entry_set_text (GTK_ENTRY (container->details->search_entry), "");
       }

	if (event->type == GDK_2BUTTON_PRESS || event->type == GDK_3BUTTON_PRESS) {
		/* We use our own double-click detection. */
		return TRUE;
	}

	/* Invoke the canvas event handler and see if an item picks up the event. */
	clicked_on_item = GTK_WIDGET_CLASS (nolphin_icon_container_parent_class)->button_press_event (widget, event);

	/* Move focus to icon container, unless we're still renaming (to avoid exiting
	 * renaming mode)
	 */
  	if (!gtk_widget_has_focus (widget) && !(is_renaming (container) || is_renaming_pending (container))) {
    		gtk_widget_grab_focus (widget);
    	}

    if (clicked_on_item) {
        NolphinIcon *icon; // current icon which was clicked on

        /* when icon is in renaming mode and user clicks on the image part of icon renaming should get closed */
        icon = get_first_selected_icon (container); // this function gets the clicked icon

        if (clicked_on_icon (container, icon, event) &&
            icon == nolphin_icon_container_get_icon_being_renamed (container)) {
            nolphin_icon_container_end_renaming_mode (container, TRUE);
        }

		return TRUE;
	}

	if (event->button == DRAG_BUTTON &&
	    event->type == GDK_BUTTON_PRESS) {
		/* Clear the last click icon for double click */
		container->details->double_click_icon[1] = container->details->double_click_icon[0];
		container->details->double_click_icon[0] = NULL;
	}

	/* Button 1 does rubber banding. */
	if (event->button == RUBBERBAND_BUTTON) {
		if (! button_event_modifies_selection (event)) {
			selection_changed = unselect_all (container);
			if (selection_changed) {
				g_signal_emit (container,
						 signals[SELECTION_CHANGED], 0);
			}
		}

		start_rubberbanding (container, event);
		return TRUE;
	}

	/* Prevent multi-button weirdness such as bug 6181 */
	if (container->details->rubberband_info.active) {
		return TRUE;
	}

	/* Button 2 may be passed to the window manager. */
	if (event->button == MIDDLE_BUTTON) {
		selection_changed = unselect_all (container);
		if (selection_changed) {
			g_signal_emit (container, signals[SELECTION_CHANGED], 0);
		}
		g_signal_emit (widget, signals[MIDDLE_CLICK], 0, event);
		return TRUE;
	}

	/* Button 3 does a contextual menu. */
	if (event->button == CONTEXTUAL_MENU_BUTTON) {
		nolphin_icon_container_end_renaming_mode (container, TRUE);
		selection_changed = unselect_all (container);
		if (selection_changed) {
			g_signal_emit (container, signals[SELECTION_CHANGED], 0);
		}
		g_signal_emit (widget, signals[CONTEXT_CLICK_BACKGROUND], 0, event);
		return TRUE;
	}

	/* Otherwise, we emit a button_press message. */
	g_signal_emit (widget,
		       signals[BUTTON_PRESS], 0, event,
		       &return_value);
	return return_value;
}

static void
nolphin_icon_container_did_not_drag (NolphinIconContainer *container,
				      GdkEventButton *event)
{
	NolphinIconContainerDetails *details;
	gboolean selection_changed;
	static gint64 last_click_time = 0;
	static gint click_count = 0;
	gint double_click_time;
	gint64 current_time;

	details = container->details;

	if (details->icon_selected_on_button_down &&
	    ((event->state & GDK_CONTROL_MASK) != 0 ||
	     (event->state & GDK_SHIFT_MASK) == 0)) {
		if (button_event_modifies_selection (event)) {
			details->range_selection_base_icon = NULL;
			icon_toggle_selected (container, details->drag_icon);
			g_signal_emit (container,
				       signals[SELECTION_CHANGED], 0);
		} else {
			details->range_selection_base_icon = details->drag_icon;
			selection_changed = select_one_unselect_others
				(container, details->drag_icon);

			if (selection_changed) {
				g_signal_emit (container,
					       signals[SELECTION_CHANGED], 0);
			}
		}
    }

	if (details->drag_icon != NULL &&
	    (details->single_click_mode ||
	     event->button == MIDDLE_BUTTON)) {
		/* Determine click count */
		g_object_get (G_OBJECT (gtk_widget_get_settings (GTK_WIDGET (container))),
			      "gtk-double-click-time", &double_click_time,
			      NULL);
		current_time = g_get_monotonic_time ();
		if (current_time - last_click_time < double_click_time * 1000) {
			click_count++;
		} else {
			click_count = 0;
		}

		/* Stash time for next compare */
		last_click_time = current_time;

		/* If single-click mode, activate the selected icons, unless modifying
		 * the selection or pressing for a very long time, or double clicking.
		 */


		if (click_count == 0 &&
		    event->time - details->button_down_time < MAX_CLICK_TIME &&
		    ! button_event_modifies_selection (event)) {

			/* It's a tricky UI issue whether this should activate
			 * just the clicked item (as if it were a link), or all
			 * the selected items (as if you were issuing an "activate
			 * selection" command). For now, we're trying the activate
			 * entire selection version to see how it feels. Note that
			 * NolphinList goes the other way because its "links" seem
			 * much more link-like.
			 */
			if (event->button == MIDDLE_BUTTON) {
				activate_selected_items_alternate (container, NULL);
			} else {
				activate_selected_items (container);
			}
		}
	}

    if (details->drag_icon != NULL &&
        handle_icon_slow_two_click (container, details->drag_icon, event)) {
        if (!details->skip_rename_on_release)
            nolphin_icon_container_start_renaming_selected_item (container, FALSE);
    }
}

static gboolean
clicked_within_double_click_interval (NolphinIconContainer *container)
{
	static gint64 last_click_time = 0;
	static gint click_count = 0;
	gint64 current_time;
    gint interval;

    /* fetch system double-click time */
    g_object_get (G_OBJECT (gtk_widget_get_settings (GTK_WIDGET (container))),
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
		return TRUE;
	} else {
		return FALSE;
	}
}

static gboolean
clicked_within_slow_click_interval_on_text (NolphinIconContainer *container, NolphinIcon *icon, GdkEventButton *event)
{
    static gint64 last_slow_click_time = 0;
    static gint slow_click_count = 0;
    gint64 current_time;
    gint interval;
    gint double_click_interval;

    /* fetch system double-click time */
    g_object_get (G_OBJECT (gtk_widget_get_settings (GTK_WIDGET (container))),
                  "gtk-double-click-time", &double_click_interval,
                  NULL);

    /* Cancel pending click-to-rename after double-click-time + 800ms */

    interval = double_click_interval + 800;

    current_time = g_get_monotonic_time ();
    if (current_time - last_slow_click_time < interval * 1000) {
        slow_click_count = 1;
    } else {
        slow_click_count = 0;
    }

    /* Stash time for next compare */
    last_slow_click_time = current_time;

    /* Only allow second click on text to trigger this */
    if (slow_click_count == 1 &&
        icon == get_first_selected_icon (container) &&
        clicked_on_text (container, icon, event)) {
        slow_click_count = 0;
        return TRUE;
    } else {
        return FALSE;
    }
}

static void
clear_drag_state (NolphinIconContainer *container)
{
	container->details->drag_icon = NULL;
	container->details->drag_state = DRAG_STATE_INITIAL;
}

static gboolean
start_stretching (NolphinIconContainer *container)
{
	NolphinIconContainerDetails *details;
	NolphinIcon *icon;
	GtkWidget *toplevel;
	GtkCornerType corner;
	GdkCursor *cursor;

	details = container->details;
	icon = details->stretch_icon;

	/* Check if we hit the stretch handles. */
	if (!nolphin_icon_canvas_item_hit_test_stretch_handles (icon->item,
								 details->drag_x, details->drag_y,
								 &corner)) {
		return FALSE;
	}

	switch (corner) {
	case GTK_CORNER_TOP_LEFT:
		cursor = gdk_cursor_new (GDK_TOP_LEFT_CORNER);
		break;
	case GTK_CORNER_BOTTOM_LEFT:
		cursor = gdk_cursor_new (GDK_BOTTOM_LEFT_CORNER);
		break;
	case GTK_CORNER_TOP_RIGHT:
		cursor = gdk_cursor_new (GDK_TOP_RIGHT_CORNER);
		break;
	case GTK_CORNER_BOTTOM_RIGHT:
		cursor = gdk_cursor_new (GDK_BOTTOM_RIGHT_CORNER);
		break;
	default:
		cursor = NULL;
		break;
	}
	/* Set up the dragging. */
	details->drag_state = DRAG_STATE_STRETCH;
	eel_canvas_w2c (EEL_CANVAS (container),
			  details->drag_x,
			  details->drag_y,
			  &details->stretch_start.pointer_x,
			  &details->stretch_start.pointer_y);
	eel_canvas_w2c (EEL_CANVAS (container),
			  icon->x, icon->y,
			  &details->stretch_start.icon_x,
			  &details->stretch_start.icon_y);
	icon_get_size (container, icon,
		       &details->stretch_start.icon_size);

	eel_canvas_item_grab (EEL_CANVAS_ITEM (icon->item),
				(GDK_POINTER_MOTION_MASK
				 | GDK_BUTTON_RELEASE_MASK),
				cursor,
				GDK_CURRENT_TIME);
	if (cursor)
		g_object_unref (cursor);

	/* Ensure the window itself is focused.. */
	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (container));
	if (toplevel != NULL && gtk_widget_get_realized (toplevel)) {
		gdk_window_focus (gtk_widget_get_window (toplevel), GDK_CURRENT_TIME);
	}

	return TRUE;
}

static gboolean
update_stretch_at_idle (NolphinIconContainer *container)
{
	NolphinIconContainerDetails *details;
	NolphinIcon *icon;
	double world_x, world_y;
	StretchState stretch_state;

	details = container->details;
	icon = details->stretch_icon;

	if (icon == NULL) {
		container->details->stretch_idle_id = 0;
		return FALSE;
	}

	eel_canvas_w2c (EEL_CANVAS (container),
			  details->world_x, details->world_y,
			  &stretch_state.pointer_x, &stretch_state.pointer_y);

	compute_stretch (&details->stretch_start,
			 &stretch_state);

	eel_canvas_c2w (EEL_CANVAS (container),
			  stretch_state.icon_x, stretch_state.icon_y,
			  &world_x, &world_y);

	nolphin_icon_container_icon_set_position (container, icon, world_x, world_y);
	icon_set_size (container, icon, stretch_state.icon_size, FALSE, FALSE);

	container->details->stretch_idle_id = 0;

	return FALSE;
}

static void
continue_stretching (NolphinIconContainer *container,
		     double world_x, double world_y)
{

	g_return_if_fail (NOLPHIN_IS_ICON_CONTAINER (container));

	container->details->world_x = world_x;
	container->details->world_y = world_y;

	if (container->details->stretch_idle_id == 0) {
		container->details->stretch_idle_id = g_idle_add ((GSourceFunc) update_stretch_at_idle, container);
	}
}

static gboolean
keyboard_stretching (NolphinIconContainer *container,
		     GdkEventKey           *event)
{
	NolphinIcon *icon;
	guint size;

	icon = container->details->stretch_icon;

	if (icon == NULL || !icon->is_selected) {
		return FALSE;
	}

	icon_get_size (container, icon, &size);

	switch (event->keyval) {
	case GDK_KEY_equal:
	case GDK_KEY_plus:
	case GDK_KEY_KP_Add:
		icon_set_size (container, icon, size + 5, FALSE, FALSE);
		break;
	case GDK_KEY_minus:
	case GDK_KEY_KP_Subtract:
		icon_set_size (container, icon, size - 5, FALSE, FALSE);
		break;
	case GDK_KEY_0:
	case GDK_KEY_KP_0:
		nolphin_icon_container_move_icon (container, icon,
						   icon->x, icon->y,
						   1.0,
						   FALSE, TRUE, TRUE);
		break;
    default:
        break;
	}

	return TRUE;
}

static void
ungrab_stretch_icon (NolphinIconContainer *container)
{
	eel_canvas_item_ungrab (EEL_CANVAS_ITEM (container->details->stretch_icon->item),
				  GDK_CURRENT_TIME);
}

static void
end_stretching (NolphinIconContainer *container,
		double world_x, double world_y)
{
	NolphinIconPosition position;
	NolphinIcon *icon;

	continue_stretching (container, world_x, world_y);
	ungrab_stretch_icon (container);

	/* now that we're done stretching, update the icon's position */

	icon = container->details->drag_icon;
	if (nolphin_icon_container_is_layout_rtl (container)) {
		position.x = icon->saved_ltr_x = nolphin_icon_container_get_mirror_x_position (container, icon, icon->x);
	} else {
		position.x = icon->x;
	}
	position.y = icon->y;
	position.scale = icon->scale;
	g_signal_emit (container,
			 signals[ICON_POSITION_CHANGED], 0,
			 icon->data, &position);

	clear_drag_state (container);
	nolphin_icon_container_redo_layout (container);
}

static gboolean
undo_stretching (NolphinIconContainer *container)
{
	NolphinIcon *stretched_icon;

	stretched_icon = container->details->stretch_icon;

	if (stretched_icon == NULL) {
		return FALSE;
	}

	if (container->details->drag_state == DRAG_STATE_STRETCH) {
		ungrab_stretch_icon (container);
		clear_drag_state (container);
	}
	nolphin_icon_canvas_item_set_show_stretch_handles
		(stretched_icon->item, FALSE);

	nolphin_icon_container_icon_set_position (container, stretched_icon,
			   container->details->stretch_initial_x,
			   container->details->stretch_initial_y);
	icon_set_size (container,
		       stretched_icon,
		       container->details->stretch_initial_size,
		       TRUE,
		       TRUE);

	container->details->stretch_icon = NULL;
	emit_stretch_ended (container, stretched_icon);
	nolphin_icon_container_redo_layout (container);

	return TRUE;
}

static gboolean
button_release_event (GtkWidget *widget,
		      GdkEventButton *event)
{
	NolphinIconContainer *container;
	NolphinIconContainerDetails *details;
	double world_x, world_y;

	container = NOLPHIN_ICON_CONTAINER (widget);
	details = container->details;

	if (event->button == RUBBERBAND_BUTTON && details->rubberband_info.active) {
		stop_rubberbanding (container, event->time);
        return GTK_WIDGET_CLASS (nolphin_icon_container_parent_class)->button_release_event (widget, event);
	}

	if (event->button == details->drag_button) {
		details->drag_button = 0;

		switch (details->drag_state) {
		case DRAG_STATE_MOVE_OR_COPY:
			if (!details->drag_started) {
				nolphin_icon_container_did_not_drag (container, event);
			} else {
				nolphin_icon_dnd_end_drag (container);
				DEBUG ("Ending drag from icon container");
			}
			break;
		case DRAG_STATE_STRETCH:
			eel_canvas_window_to_world
				(EEL_CANVAS (container), event->x, event->y, &world_x, &world_y);
			end_stretching (container, world_x, world_y);
			break;
        case DRAG_STATE_INITIAL:
		default:
			break;
		}

		clear_drag_state (container);
		return TRUE;
	}

	return GTK_WIDGET_CLASS (nolphin_icon_container_parent_class)->button_release_event (widget, event);
}

static int
motion_notify_event (GtkWidget *widget,
		     GdkEventMotion *event)
{
	NolphinIconContainer *container;
	NolphinIconContainerDetails *details;
	double world_x, world_y;
	int canvas_x, canvas_y;
	GdkDragAction actions;

	container = NOLPHIN_ICON_CONTAINER (widget);
	details = container->details;

	if (details->drag_button != 0) {
		switch (details->drag_state) {
		case DRAG_STATE_MOVE_OR_COPY:
			if (details->drag_started) {
				break;
			}

			eel_canvas_window_to_world
				(EEL_CANVAS (container), event->x, event->y, &world_x, &world_y);

			if (gtk_drag_check_threshold (widget,
						      details->drag_x,
						      details->drag_y,
						      world_x,
						      world_y)) {
				details->drag_started = TRUE;
				details->drag_state = DRAG_STATE_MOVE_OR_COPY;

				nolphin_icon_container_end_renaming_mode (container, TRUE);

				eel_canvas_w2c (EEL_CANVAS (container),
						  details->drag_x,
						  details->drag_y,
						  &canvas_x,
						  &canvas_y);

				actions = GDK_ACTION_COPY
					| GDK_ACTION_LINK
					| GDK_ACTION_ASK;

				if (container->details->drag_allow_moves) {
					actions |= GDK_ACTION_MOVE;
				}

				nolphin_icon_dnd_begin_drag (container,
							      actions,
							      details->drag_button,
							      event,
							      canvas_x,
							      canvas_y);
				DEBUG ("Beginning drag from icon container");
			}
			break;
		case DRAG_STATE_STRETCH:
			eel_canvas_window_to_world
				(EEL_CANVAS (container), event->x, event->y, &world_x, &world_y);
			continue_stretching (container, world_x, world_y);
			break;
        case DRAG_STATE_INITIAL:
		default:
			break;
		}
	}

	return GTK_WIDGET_CLASS (nolphin_icon_container_parent_class)->motion_notify_event (widget, event);
}

static void
nolphin_icon_container_search_position_func (NolphinIconContainer *container,
					      GtkWidget *search_dialog)
{
    gint x, y;
    gint cont_x, cont_y;
    gint cont_width, cont_height;
    GdkWindow *cont_window;
    GtkRequisition requisition;

    /* Desktop containers only: every other container forwards keypresses to the
     * filter bar rather than starting an interactive search, so nothing else
     * reaches here - see nolphin_icon_container_key_press_event ().
     */
    cont_window = gtk_widget_get_window (GTK_WIDGET (container));

    gtk_widget_realize (search_dialog);

    gdk_window_get_origin (cont_window, &cont_x, &cont_y);
    cont_width = gdk_window_get_width (cont_window);
    cont_height = gdk_window_get_height (cont_window);

    gtk_widget_get_preferred_size (search_dialog, &requisition, NULL);

    x = cont_x + cont_width - requisition.width;
    y = cont_y + cont_height - requisition.height;

    if (eel_check_is_wayland ()) {
        GtkWidget *toplevel = gtk_widget_get_toplevel (GTK_WIDGET (container));
        GdkWindow *parent_window = gtk_widget_get_window (toplevel);
        gint parent_x, parent_y;

        gdk_window_get_origin (parent_window, &parent_x, &parent_y);

        GdkRectangle anchor_rect = {
            x - parent_x,
            y - parent_y,
            1, 1
        };

        gdk_window_move_to_rect (gtk_widget_get_window (search_dialog),
                                 &anchor_rect,
                                 GDK_GRAVITY_NORTH_WEST,
                                 GDK_GRAVITY_NORTH_WEST,
                                 0,
                                 0, 0);
        return;
    }

    gdk_window_move (gtk_widget_get_window (search_dialog), x, y);
}

/* Cut and paste from gtkwindow.c */
static void
send_focus_change (GtkWidget *widget, gboolean in)
{
	GdkEvent *fevent;

	fevent = gdk_event_new (GDK_FOCUS_CHANGE);

	g_object_ref (widget);
	((GdkEventFocus *) fevent)->in = in;

	gtk_widget_send_focus_change (widget, fevent);

	fevent->focus_change.type = GDK_FOCUS_CHANGE;
	fevent->focus_change.window = g_object_ref (gtk_widget_get_window (widget));
	fevent->focus_change.in = in;

	gtk_widget_event (widget, fevent);

	g_object_notify (G_OBJECT (widget), "has-focus");

	g_object_unref (widget);
	gdk_event_free (fevent);
}

static void
nolphin_icon_container_search_dialog_hide (GtkWidget *search_dialog,
					    NolphinIconContainer *container)
{
	if (container->details->search_entry_changed_id) {
		g_signal_handler_disconnect (container->details->search_entry,
					     container->details->search_entry_changed_id);
		container->details->search_entry_changed_id = 0;
	}

	remove_search_entry_timeout (container);

	/* send focus-in event */
	send_focus_change (GTK_WIDGET (container->details->search_entry), FALSE);
	gtk_widget_hide (search_dialog);
	gtk_entry_set_text (GTK_ENTRY (container->details->search_entry), "");
}

static gboolean
nolphin_icon_container_search_entry_flush_timeout (gpointer data)
{
	NolphinIconContainer *container = data;

	container->details->typeselect_flush_timeout = 0;
	nolphin_icon_container_search_dialog_hide (container->details->search_window, container);

	return FALSE;
}

static void
add_search_entry_timeout (NolphinIconContainer *container)
{
	container->details->typeselect_flush_timeout =
		g_timeout_add_seconds (NOLPHIN_ICON_CONTAINER_SEARCH_DIALOG_TIMEOUT,
				       nolphin_icon_container_search_entry_flush_timeout,
				       container);
}

static void
remove_search_entry_timeout (NolphinIconContainer *container)
{
	if (container->details->typeselect_flush_timeout) {
		g_source_remove (container->details->typeselect_flush_timeout);
		container->details->typeselect_flush_timeout = 0;
	}
}

static void
reset_search_entry_timeout (NolphinIconContainer *container)
{
	remove_search_entry_timeout (container);
	add_search_entry_timeout (container);
}

/* Because we're visible but offscreen, we just set a flag in the preedit
 * callback.
 */
static void
nolphin_icon_container_search_preedit_changed (GtkEntry *entry,
						gchar *preedit,
						NolphinIconContainer *container)
{
	container->details->imcontext_changed = 1;
	reset_search_entry_timeout (container);
}

static void
nolphin_icon_container_search_activate (GtkEntry *entry,
					 NolphinIconContainer *container)
{
	nolphin_icon_container_search_dialog_hide (container->details->search_window,
						    container);

	activate_selected_items (container);
}

static gboolean
nolphin_icon_container_search_delete_event (GtkWidget *widget,
					     GdkEventAny *event,
					     NolphinIconContainer *container)
{
	nolphin_icon_container_search_dialog_hide (widget, container);

	return TRUE;
}

static gboolean
nolphin_icon_container_search_button_press_event (GtkWidget *widget,
						   GdkEventButton *event,
						   NolphinIconContainer *container)
{
	nolphin_icon_container_search_dialog_hide (widget, container);

	if (event->window == gtk_layout_get_bin_window (GTK_LAYOUT (container))) {
		button_press_event (GTK_WIDGET (container), event);
	}

	return TRUE;
}

static gboolean
nolphin_icon_container_search_entry_button_press_event (GtkWidget *widget,
							 GdkEventButton *event,
							 NolphinIconContainer *container)
{
	reset_search_entry_timeout (container);

	return FALSE;
}

static void
nolphin_icon_container_search_populate_popup (GtkEntry *entry,
					       GtkMenu *menu,
					       NolphinIconContainer *container)
{
	remove_search_entry_timeout (container);
	g_signal_connect_swapped (menu, "hide",
				  G_CALLBACK (add_search_entry_timeout), container);
}

void
nolphin_icon_container_get_icon_text (NolphinIconContainer *container,
				       NolphinIconData      *data,
				       char                 **editable_text,
				       char                 **additional_text,
                       gboolean              *pinned,
                       gboolean              *fav_unavailable,
				       gboolean               include_invisible)
{
	NolphinIconContainerClass *klass;

	klass = NOLPHIN_ICON_CONTAINER_GET_CLASS (container);
	g_assert (klass->get_icon_text != NULL);

    klass->get_icon_text (container, data, editable_text, additional_text, pinned, fav_unavailable, include_invisible);
}

static gboolean
nolphin_icon_container_search_iter (NolphinIconContainer *container,
				     const char *key, gint n)
{
	GList *p;
	NolphinIcon *icon;
	char *name;
	int count;
	char *normalized_key, *case_normalized_key;
	char *normalized_name, *case_normalized_name;

	g_assert (key != NULL);
	g_assert (n >= 1);

	normalized_key = g_utf8_normalize (key, -1, G_NORMALIZE_ALL);
	if (!normalized_key) {
		return FALSE;
	}
	case_normalized_key = g_utf8_casefold (normalized_key, -1);
	g_free (normalized_key);
	if (!case_normalized_key) {
		return FALSE;
	}

	icon = NULL;
	name = NULL;
	count = 0;
	for (p = container->details->icons; p != NULL && count != n; p = p->next) {
		icon = p->data;
		nolphin_icon_container_get_icon_text (container, icon->data, &name,
						       NULL, NULL, NULL, TRUE);

		/* This can happen if a key event is handled really early while
		 * loading the icon container, before the items have all been
		 * updated once.
		 */
		if (!name) {
			continue;
		}

		normalized_name = g_utf8_normalize (name, -1, G_NORMALIZE_ALL);
		if (!normalized_name) {
			continue;
		}
		case_normalized_name = g_utf8_casefold (normalized_name, -1);
		g_free (normalized_name);
		if (!case_normalized_name) {
			continue;
		}

		if (strncmp (case_normalized_key, case_normalized_name,
			     strlen (case_normalized_key)) == 0) {
			count++;
		}

		g_free (case_normalized_name);
		g_free (name);
		name = NULL;
	}

	g_free (case_normalized_key);

	if (count == n) {
		if (select_one_unselect_others (container, icon)) {
			g_signal_emit (container, signals[SELECTION_CHANGED], 0);
		}
		schedule_keyboard_icon_reveal (container, icon);

		return TRUE;
	}

	return FALSE;
}

static void
nolphin_icon_container_search_move (GtkWidget *window,
				     NolphinIconContainer *container,
				     gboolean up)
{
	gboolean ret;
	gint len;
	const gchar *text;

	text = gtk_entry_get_text (GTK_ENTRY (container->details->search_entry));

	g_assert (text != NULL);

	if (container->details->selected_iter == 0) {
		return;
	}

	if (up && container->details->selected_iter == 1) {
		return;
	}

	len = strlen (text);

	if (len < 1) {
		return;
	}

	/* search */
	unselect_all (container);

	ret = nolphin_icon_container_search_iter (container, text,
		up?((container->details->selected_iter) - 1):((container->details->selected_iter + 1)));

	if (ret) {
		/* found */
		container->details->selected_iter += up?(-1):(1);
	} else {
		/* return to old iter */
		nolphin_icon_container_search_iter (container, text,
					container->details->selected_iter);
	}
}

static gboolean
nolphin_icon_container_search_scroll_event (GtkWidget *widget,
					     GdkEventScroll *event,
					     NolphinIconContainer *container)
{
	gboolean retval = FALSE;

	if (event->direction == GDK_SCROLL_UP) {
		nolphin_icon_container_search_move (widget, container, TRUE);
		retval = TRUE;
	} else if (event->direction == GDK_SCROLL_DOWN) {
		nolphin_icon_container_search_move (widget, container, FALSE);
		retval = TRUE;
	}

	reset_search_entry_timeout (container);

	return retval;
}

static gboolean
nolphin_icon_container_search_key_press_event (GtkWidget *widget,
						GdkEventKey *event,
						NolphinIconContainer *container)
{
	gboolean retval = FALSE;

	g_assert (GTK_IS_WIDGET (widget));
	g_assert (NOLPHIN_IS_ICON_CONTAINER (container));

	/* close window and cancel the search */
	if (event->keyval == GDK_KEY_Escape || event->keyval == GDK_KEY_Tab) {
		nolphin_icon_container_search_dialog_hide (widget, container);
		return TRUE;
	}

	/* close window and activate alternate */
	if (event->keyval == GDK_KEY_Return && event->state & GDK_SHIFT_MASK) {
		nolphin_icon_container_search_dialog_hide (widget,
							    container);

		activate_selected_items_alternate (container, NULL);
		return TRUE;
	}

	/* select previous matching iter */
	if (event->keyval == GDK_KEY_Up || event->keyval == GDK_KEY_KP_Up) {
		nolphin_icon_container_search_move (widget, container, TRUE);
		retval = TRUE;
	}

	if (((event->state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) == (GDK_CONTROL_MASK | GDK_SHIFT_MASK))
	    && (event->keyval == GDK_KEY_g || event->keyval == GDK_KEY_G)) {
		nolphin_icon_container_search_move (widget, container, TRUE);
		retval = TRUE;
	}

	/* select next matching iter */
	if (event->keyval == GDK_KEY_Down || event->keyval == GDK_KEY_KP_Down) {
		nolphin_icon_container_search_move (widget, container, FALSE);
		retval = TRUE;
	}

	if (((event->state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) == GDK_CONTROL_MASK)
	    && (event->keyval == GDK_KEY_g || event->keyval == GDK_KEY_G)) {
		nolphin_icon_container_search_move (widget, container, FALSE);
		retval = TRUE;
	}

	reset_search_entry_timeout (container);

	return retval;
}

static void
nolphin_icon_container_search_init (GtkWidget   *entry,
				     NolphinIconContainer *container)
{
	gint ret;
	gint len;
	const gchar *text;

	g_assert (GTK_IS_ENTRY (entry));
	g_assert (NOLPHIN_IS_ICON_CONTAINER (container));

	text = gtk_entry_get_text (GTK_ENTRY (entry));
	len = strlen (text);

	/* search */
	unselect_all (container);
	reset_search_entry_timeout (container);

	if (len < 1) {
		return;
	}

	ret = nolphin_icon_container_search_iter (container, text, 1);

	if (ret) {
		container->details->selected_iter = 1;
	}
}

static void
nolphin_icon_container_ensure_interactive_directory (NolphinIconContainer *container)
{
	GtkWidget *frame, *vbox, *toplevel;

	if (container->details->search_window != NULL) {
		return;
	}

	container->details->search_window = gtk_window_new (GTK_WINDOW_POPUP);
    toplevel = gtk_widget_get_toplevel (GTK_WIDGET (container));

    gtk_window_set_transient_for (GTK_WINDOW (container->details->search_window),
                                  GTK_WINDOW (toplevel));

	gtk_window_set_type_hint (GTK_WINDOW (container->details->search_window),
				  GDK_WINDOW_TYPE_HINT_COMBO);

	g_signal_connect (container->details->search_window, "delete_event",
			  G_CALLBACK (nolphin_icon_container_search_delete_event),
			  container);
	g_signal_connect (container->details->search_window, "key_press_event",
			  G_CALLBACK (nolphin_icon_container_search_key_press_event),
			  container);
	g_signal_connect (container->details->search_window, "button_press_event",
			  G_CALLBACK (nolphin_icon_container_search_button_press_event),
			  container);
	g_signal_connect (container->details->search_window, "scroll_event",
			  G_CALLBACK (nolphin_icon_container_search_scroll_event),
			  container);

	frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_ETCHED_IN);
	gtk_widget_show (frame);
	gtk_container_add (GTK_CONTAINER (container->details->search_window), frame);

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_show (vbox);
	gtk_container_add (GTK_CONTAINER (frame), vbox);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 3);

	/* add entry */
	container->details->search_entry = gtk_entry_new ();
	gtk_widget_show (container->details->search_entry);
	g_signal_connect (container->details->search_entry, "populate-popup",
			  G_CALLBACK (nolphin_icon_container_search_populate_popup),
			  container);
	g_signal_connect (container->details->search_entry, "activate",
			  G_CALLBACK (nolphin_icon_container_search_activate),
			  container);
	g_signal_connect (container->details->search_entry, "preedit-changed",
			  G_CALLBACK (nolphin_icon_container_search_preedit_changed),
			  container);
	g_signal_connect (container->details->search_entry, "button-press-event",
			  G_CALLBACK (nolphin_icon_container_search_entry_button_press_event),
			  container);
	gtk_container_add (GTK_CONTAINER (vbox), container->details->search_entry);

	gtk_widget_realize (container->details->search_entry);

    if (eel_check_is_wayland () && nolphin_icon_container_get_is_desktop (container)) {
        GdkRectangle anchor_rect = { 0, 0, 1, 1 };

        gdk_window_move_to_rect (gtk_widget_get_window (container->details->search_window),
                                 &anchor_rect,
                                 GDK_GRAVITY_NORTH_WEST,
                                 GDK_GRAVITY_NORTH_WEST,
                                 0,
                                 0, 0);
    }
}

/* Pops up the interactive search entry.  If keybinding is TRUE then the user
 * started this by typing the start_interactive_search keybinding.  Otherwise, it came from
 */
static gboolean
nolphin_icon_container_start_interactive_search (NolphinIconContainer *container)
{
	/* We only start interactive search if we have focus.  If one of our
	 * children have focus, we don't want to start the search.
	 */
	GtkWidgetClass *entry_parent_class;

	if (container->details->search_window != NULL &&
	    gtk_widget_get_visible (container->details->search_window)) {
		return TRUE;
	}

	if (!gtk_widget_has_focus (GTK_WIDGET (container))) {
		return FALSE;
	}

	nolphin_icon_container_ensure_interactive_directory (container);

	/* done, show it */
	nolphin_icon_container_search_position_func (container, container->details->search_window);
	gtk_widget_show (container->details->search_window);
	if (container->details->search_entry_changed_id == 0) {
		container->details->search_entry_changed_id =
			g_signal_connect (container->details->search_entry, "changed",
				G_CALLBACK (nolphin_icon_container_search_init),
				container);
	}

	/* Grab focus will select all the text.  We don't want that to happen, so we
	* call the parent instance and bypass the selection change.  This is probably
	* really non-kosher. */
	entry_parent_class = g_type_class_peek_parent (GTK_ENTRY_GET_CLASS (container->details->search_entry));
	(entry_parent_class->grab_focus) (container->details->search_entry);

	/* send focus-in event */
	send_focus_change (container->details->search_entry, TRUE);

	/* search first matching iter */
	nolphin_icon_container_search_init (container->details->search_entry, container);

	return TRUE;
}

static gboolean
handle_popups (NolphinIconContainer *container,
	       GdkEventKey           *event,
	       const char            *signal)
{
	GdkEventButton button_event = { 0 };

	/* ensure we clear the drag state before showing the menu */
	clear_drag_state (container);

	g_signal_emit_by_name (container, signal, &button_event);

	return TRUE;
}

static int
key_press_event (GtkWidget *widget,
		 GdkEventKey *event)
{
	NolphinIconContainer *container;
	gboolean handled;

	container = NOLPHIN_ICON_CONTAINER (widget);
	handled = FALSE;

	if (is_renaming (container) || is_renaming_pending (container)) {
		switch (event->keyval) {
		case GDK_KEY_Return:
		case GDK_KEY_KP_Enter:
			nolphin_icon_container_end_renaming_mode (container, TRUE);
			handled = TRUE;
			break;
		case GDK_KEY_Escape:
			nolphin_icon_container_end_renaming_mode (container, FALSE);
			handled = TRUE;
			break;
		default:
			break;
		}
	} else if (container->details->search_window != NULL &&
	           gtk_widget_get_visible (container->details->search_window)) {
		/* Workaround for BGO #662591, where container is still focused
		 * although we're in search mode. Forward events to
		 * search_entry, where they belong. */
		GdkEvent *new_event = gdk_event_copy ((GdkEvent *) event);
		GdkWindow *window = ((GdkEventKey *) new_event)->window;
		((GdkEventKey *) new_event)->window = gtk_widget_get_window (container->details->search_entry);

		handled = gtk_widget_event (container->details->search_window, new_event);

		((GdkEventKey *) new_event)->window = window;
		gdk_event_free(new_event);
	} else {
		switch (event->keyval) {
		case GDK_KEY_Home:
		case GDK_KEY_KP_Home:
			keyboard_home (container, event);
			handled = TRUE;
			break;
		case GDK_KEY_End:
		case GDK_KEY_KP_End:
			keyboard_end (container, event);
			handled = TRUE;
			break;
		case GDK_KEY_Left:
		case GDK_KEY_KP_Left:
			/* Don't eat Alt-Left, as that is used for history browsing */
			if ((event->state & GDK_MOD1_MASK) == 0) {
				keyboard_left (container, event);
				handled = TRUE;
			}
			break;
		case GDK_KEY_Up:
		case GDK_KEY_KP_Up:
			/* Don't eat Alt-Up, as that is used for alt-shift-Up */
			if ((event->state & GDK_MOD1_MASK) == 0) {
				keyboard_up (container, event);
				handled = TRUE;
			}
			break;
		case GDK_KEY_Right:
		case GDK_KEY_KP_Right:
			/* Don't eat Alt-Right, as that is used for history browsing */
			if ((event->state & GDK_MOD1_MASK) == 0) {
				keyboard_right (container, event);
				handled = TRUE;
			}
			break;
		case GDK_KEY_Down:
		case GDK_KEY_KP_Down:
			/* Don't eat Alt-Down, as that is used for Open */
			if ((event->state & GDK_MOD1_MASK) == 0) {
				keyboard_down (container, event);
				handled = TRUE;
			}
			break;
		case GDK_KEY_space:
			if (!nolphin_icon_container_get_is_desktop (container)) {
				gboolean filter_handled = FALSE;
				g_signal_emit (container, signals[CHECK_FILTER_EVENT], 0, (GdkEvent *) event, &filter_handled);
				if (filter_handled) {
					handled = TRUE;
					break;
				}
			}
			keyboard_space (container, event);
			handled = TRUE;
			break;
#ifndef TAB_NAVIGATION_DISABLED
		case GDK_KEY_Tab:
		case GDK_KEY_ISO_Left_Tab:
			select_previous_or_next_icon (container,
						      (event->state & GDK_SHIFT_MASK) == 0, event);
			handled = TRUE;
			break;
#endif
		case GDK_KEY_Return:
		case GDK_KEY_KP_Enter:
			if ((event->state & GDK_SHIFT_MASK) != 0) {
				activate_selected_items_alternate (container, NULL);
			} else {
				activate_selected_items (container);
			}

			handled = TRUE;
			break;
 		case GDK_KEY_Escape:
			handled = undo_stretching (container);
			break;
 		case GDK_KEY_plus:
 		case GDK_KEY_minus:
 		case GDK_KEY_equal:
 		case GDK_KEY_KP_Add:
 		case GDK_KEY_KP_Subtract:
 		case GDK_KEY_0:
 		case GDK_KEY_KP_0:
			if (event->state & GDK_CONTROL_MASK) {
				handled = keyboard_stretching (container, event);
			}
			break;
		case GDK_KEY_F10:
			/* handle Ctrl+F10 because we want to display the
			 * background popup even if something is selected.
			 * The other cases are handled by popup_menu().
			 */
			if (event->state & GDK_CONTROL_MASK) {
				handled = handle_popups (container, event,
							 "context_click_background");
			}
			break;
		case GDK_KEY_v:
			/* Eat Control + v to not enable type ahead */
			if ((event->state & GDK_CONTROL_MASK) != 0) {
				handled = TRUE;
			}
			break;
		default:
			break;
		}
	}

	if (!handled) {
		handled = GTK_WIDGET_CLASS (nolphin_icon_container_parent_class)->key_press_event (widget, event);
	}

	/* For non-desktop containers, forward keypresses to the filter bar.
	 * The signal handler validates the keypress and returns TRUE if handled.
	 * Desktop containers keep the old interactive search behavior. */
	if (!handled &&
	    event->keyval != GDK_KEY_asciitilde &&
	    event->keyval != GDK_KEY_KP_Divide &&
	    event->keyval != GDK_KEY_slash &&
	    event->keyval != GDK_KEY_Delete) {

	    if (!nolphin_icon_container_get_is_desktop (container)) {
	        g_signal_emit (container, signals[CHECK_FILTER_EVENT], 0, (GdkEvent *) event, &handled);
	    } else {
	        /* Desktop: old interactive search behavior */
	        GdkEvent *new_event;
	        GdkWindow *window;
	        char *old_text;
	        const char *new_text;
	        gboolean retval;
	        gboolean text_modified;
	        gulong popup_menu_id;

	        nolphin_icon_container_ensure_interactive_directory (container);

	        old_text = g_strdup (gtk_entry_get_text (GTK_ENTRY (container->details->search_entry)));
	        new_event = gdk_event_copy ((GdkEvent *) event);
	        window = ((GdkEventKey *) new_event)->window;
	        ((GdkEventKey *) new_event)->window = gtk_widget_get_window (container->details->search_entry);
	        gtk_widget_realize (container->details->search_window);

	        popup_menu_id = g_signal_connect (container->details->search_entry,
	                          "popup_menu", G_CALLBACK (gtk_true), NULL);

	        gtk_widget_show (container->details->search_window);

	        container->details->imcontext_changed = FALSE;
	        retval = gtk_widget_event (container->details->search_entry, new_event);
	        gtk_widget_hide (container->details->search_window);

	        g_signal_handler_disconnect (container->details->search_entry,
	                         popup_menu_id);

	        new_text = gtk_entry_get_text (GTK_ENTRY (container->details->search_entry));
	        text_modified = strcmp (old_text, new_text) != 0;
	        g_free (old_text);
	        if (container->details->imcontext_changed ||
	            (retval && text_modified)) {
	            if (nolphin_icon_container_start_interactive_search (container)) {
	                gtk_widget_grab_focus (GTK_WIDGET (container));
	                return TRUE;
	            } else {
	                gtk_entry_set_text (GTK_ENTRY (container->details->search_entry), "");
	                return FALSE;
	            }
	        }

	        ((GdkEventKey *) new_event)->window = window;
	        gdk_event_free (new_event);
	    }
	}

	return handled;
}

static gboolean
popup_menu (GtkWidget *widget)
{
	NolphinIconContainer *container;

	container = NOLPHIN_ICON_CONTAINER (widget);

	if (has_selection (container)) {
		handle_popups (container, NULL,
			       "context_click_selection");
	} else {
		handle_popups (container, NULL,
			       "context_click_background");
	}

	return TRUE;
}

static void
draw_canvas_background (EelCanvas *canvas,
                        cairo_t   *cr)
{
    /* Don't chain up to the parent to avoid clearing and redrawing.
     * This is overridden by nolphin-icon-view-grid-container. */
    return;
}

static AtkObject *
get_accessible (GtkWidget *widget)
{
	AtkObject *accessible;

	if ((accessible = eel_accessibility_get_atk_object (widget))) {
		return accessible;
	}

	accessible = g_object_new
		(nolphin_icon_container_accessible_get_type (), NULL);

	return eel_accessibility_set_atk_object_return (widget, accessible);
}

static void
grab_notify_cb  (GtkWidget        *widget,
		 gboolean          was_grabbed)
{
	NolphinIconContainer *container;

	container = NOLPHIN_ICON_CONTAINER (widget);

	if (container->details->rubberband_info.active &&
	    !was_grabbed) {
		/* we got a (un)grab-notify during rubberband.
		 * This happens when a new modal dialog shows
		 * up (e.g. authentication or an error). Stop
		 * the rubberbanding so that we can handle the
		 * dialog. */
		stop_rubberbanding (container,
				    GDK_CURRENT_TIME);
	}
}

static void
text_ellipsis_limit_changed_container_callback (gpointer callback_data)
{
	NolphinIconContainer *container;

	container = NOLPHIN_ICON_CONTAINER (callback_data);
	invalidate_label_sizes (container);
	schedule_redo_layout (container);
}

static void
real_lay_down_icons (NolphinIconContainer *container,
                     GList             *icons,
                     double start_y)
{
    g_assert_not_reached ();
}

static void
real_icon_set_position (NolphinIconContainer *container,
                        NolphinIcon          *icon,
                        double x,
                        double y)
{
    g_assert_not_reached ();
}

static void
real_move_icon (NolphinIconContainer *container,
                NolphinIcon *icon,
                int x, int y,
                double scale,
                gboolean raise,
                gboolean snap,
                gboolean update_position)
{
    g_assert_not_reached ();
}

static void
real_update_icon (NolphinIconContainer *container,
                  NolphinIcon *icon,
                  gboolean visible)
{
    g_assert_not_reached ();
}

static void
real_align_icons (NolphinIconContainer *container)
{
    g_assert_not_reached ();
}

static void
real_icon_get_bounding_box (NolphinIcon *icon,
                            int *x1_return, int *y1_return,
                            int *x2_return, int *y2_return,
                            NolphinIconCanvasItemBoundsUsage usage)
{
    g_assert_not_reached ();
}

static void
real_set_zoom_level (NolphinIconContainer *container,
                     gint               new_level)
{
    g_assert_not_reached ();
}

/* Initialization.  */

static void
nolphin_icon_container_class_init (NolphinIconContainerClass *class)
{
	GtkWidgetClass *widget_class;
	EelCanvasClass *canvas_class;

	G_OBJECT_CLASS (class)->finalize = finalize;

    class->lay_down_icons = real_lay_down_icons;
    class->icon_set_position = real_icon_set_position;
    class->move_icon = real_move_icon;
    class->update_icon = real_update_icon;
    class->align_icons = real_align_icons;
    class->finish_adding_new_icons = NULL;
    class->icon_get_bounding_box = real_icon_get_bounding_box;
    class->set_zoom_level = real_set_zoom_level;
    class->is_grid_container = FALSE;

	/* Signals.  */

	signals[SELECTION_CHANGED]
		= g_signal_new ("selection_changed",
		                G_TYPE_FROM_CLASS (class),
		                G_SIGNAL_RUN_LAST,
		                G_STRUCT_OFFSET (NolphinIconContainerClass,
						 selection_changed),
		                NULL, NULL,
		                g_cclosure_marshal_VOID__VOID,
		                G_TYPE_NONE, 0);
	signals[BUTTON_PRESS]
		= g_signal_new ("button_press",
		                G_TYPE_FROM_CLASS (class),
		                G_SIGNAL_RUN_LAST,
		                G_STRUCT_OFFSET (NolphinIconContainerClass,
						 button_press),
		                NULL, NULL,
		                g_cclosure_marshal_generic,
		                G_TYPE_BOOLEAN, 1,
				GDK_TYPE_EVENT);
	signals[ACTIVATE]
		= g_signal_new ("activate",
		                G_TYPE_FROM_CLASS (class),
		                G_SIGNAL_RUN_LAST,
		                G_STRUCT_OFFSET (NolphinIconContainerClass,
						 activate),
		                NULL, NULL,
		                g_cclosure_marshal_VOID__POINTER,
		                G_TYPE_NONE, 1,
				G_TYPE_POINTER);
	signals[ACTIVATE_ALTERNATE]
		= g_signal_new ("activate_alternate",
		                G_TYPE_FROM_CLASS (class),
		                G_SIGNAL_RUN_LAST,
		                G_STRUCT_OFFSET (NolphinIconContainerClass,
						 activate_alternate),
		                NULL, NULL,
		                g_cclosure_marshal_VOID__POINTER,
		                G_TYPE_NONE, 1,
				G_TYPE_POINTER);
	signals[ACTIVATE_PREVIEWER]
		= g_signal_new ("activate_previewer",
		                G_TYPE_FROM_CLASS (class),
		                G_SIGNAL_RUN_LAST,
		                G_STRUCT_OFFSET (NolphinIconContainerClass,
						 activate_previewer),
		                NULL, NULL,
		                g_cclosure_marshal_generic,
		                G_TYPE_NONE, 2,
				G_TYPE_POINTER, G_TYPE_POINTER);
	signals[CONTEXT_CLICK_SELECTION]
		= g_signal_new ("context_click_selection",
		                G_TYPE_FROM_CLASS (class),
		                G_SIGNAL_RUN_LAST,
		                G_STRUCT_OFFSET (NolphinIconContainerClass,
						 context_click_selection),
		                NULL, NULL,
		                g_cclosure_marshal_VOID__POINTER,
		                G_TYPE_NONE, 1,
				G_TYPE_POINTER);
	signals[CONTEXT_CLICK_BACKGROUND]
		= g_signal_new ("context_click_background",
		                G_TYPE_FROM_CLASS (class),
		                G_SIGNAL_RUN_LAST,
		                G_STRUCT_OFFSET (NolphinIconContainerClass,
						 context_click_background),
		                NULL, NULL,
		                g_cclosure_marshal_VOID__POINTER,
		                G_TYPE_NONE, 1,
				G_TYPE_POINTER);
	signals[MIDDLE_CLICK]
		= g_signal_new ("middle_click",
		                G_TYPE_FROM_CLASS (class),
		                G_SIGNAL_RUN_LAST,
		                G_STRUCT_OFFSET (NolphinIconContainerClass,
						 middle_click),
		                NULL, NULL,
		                g_cclosure_marshal_VOID__POINTER,
		                G_TYPE_NONE, 1,
				G_TYPE_POINTER);
	signals[ICON_POSITION_CHANGED]
		= g_signal_new ("icon_position_changed",
		                G_TYPE_FROM_CLASS (class),
		                G_SIGNAL_RUN_LAST,
		                G_STRUCT_OFFSET (NolphinIconContainerClass,
						 icon_position_changed),
		                NULL, NULL,
		                g_cclosure_marshal_generic,
		                G_TYPE_NONE, 2,
				G_TYPE_POINTER,
				G_TYPE_POINTER);
	signals[ICON_STRETCH_STARTED]
		= g_signal_new ("icon_stretch_started",
		                G_TYPE_FROM_CLASS (class),
		                G_SIGNAL_RUN_LAST,
		                G_STRUCT_OFFSET (NolphinIconContainerClass,
						 icon_stretch_started),
		                NULL, NULL,
		                g_cclosure_marshal_VOID__POINTER,
		                G_TYPE_NONE, 1,
				G_TYPE_POINTER);
	signals[ICON_STRETCH_ENDED]
		= g_signal_new ("icon_stretch_ended",
		                G_TYPE_FROM_CLASS (class),
		                G_SIGNAL_RUN_LAST,
		                G_STRUCT_OFFSET (NolphinIconContainerClass,
						     icon_stretch_ended),
		                NULL, NULL,
		                g_cclosure_marshal_VOID__POINTER,
		                G_TYPE_NONE, 1,
				G_TYPE_POINTER);
	signals[ICON_RENAME_STARTED]
		= g_signal_new ("icon_rename_started",
		                G_TYPE_FROM_CLASS (class),
		                G_SIGNAL_RUN_LAST,
		                G_STRUCT_OFFSET (NolphinIconContainerClass,
						 icon_rename_started),
		                NULL, NULL,
		                g_cclosure_marshal_VOID__POINTER,
		                G_TYPE_NONE, 1,
				G_TYPE_POINTER);
	signals[ICON_RENAME_ENDED]
		= g_signal_new ("icon_rename_ended",
		                G_TYPE_FROM_CLASS (class),
		                G_SIGNAL_RUN_LAST,
		                G_STRUCT_OFFSET (NolphinIconContainerClass,
						 icon_rename_ended),
		                NULL, NULL,
		                g_cclosure_marshal_generic,
		                G_TYPE_NONE, 2,
				G_TYPE_POINTER,
				G_TYPE_STRING);
	signals[GET_ICON_URI]
		= g_signal_new ("get_icon_uri",
		                G_TYPE_FROM_CLASS (class),
		                G_SIGNAL_RUN_LAST,
		                G_STRUCT_OFFSET (NolphinIconContainerClass,
						 get_icon_uri),
		                NULL, NULL,
		                g_cclosure_marshal_generic,
		                G_TYPE_STRING, 1,
				G_TYPE_POINTER);
	signals[GET_ICON_DROP_TARGET_URI]
		= g_signal_new ("get_icon_drop_target_uri",
		                G_TYPE_FROM_CLASS (class),
		                G_SIGNAL_RUN_LAST,
		                G_STRUCT_OFFSET (NolphinIconContainerClass,
						 get_icon_drop_target_uri),
		                NULL, NULL,
		                g_cclosure_marshal_generic,
		                G_TYPE_STRING, 1,
				G_TYPE_POINTER);
	signals[MOVE_COPY_ITEMS]
		= g_signal_new ("move_copy_items",
		                G_TYPE_FROM_CLASS (class),
		                G_SIGNAL_RUN_LAST,
		                G_STRUCT_OFFSET (NolphinIconContainerClass,
						 move_copy_items),
		                NULL, NULL,
		                g_cclosure_marshal_generic,
		                G_TYPE_NONE, 6,
				G_TYPE_POINTER,
				G_TYPE_POINTER,
				G_TYPE_POINTER,
				GDK_TYPE_DRAG_ACTION,
				G_TYPE_INT,
				G_TYPE_INT);
	signals[HANDLE_NETSCAPE_URL]
		= g_signal_new ("handle_netscape_url",
		                G_TYPE_FROM_CLASS (class),
		                G_SIGNAL_RUN_LAST,
		                G_STRUCT_OFFSET (NolphinIconContainerClass,
						 handle_netscape_url),
		                NULL, NULL,
		                g_cclosure_marshal_generic,
		                G_TYPE_NONE, 5,
				G_TYPE_STRING,
				G_TYPE_STRING,
				GDK_TYPE_DRAG_ACTION,
				G_TYPE_INT,
				G_TYPE_INT);
	signals[HANDLE_URI_LIST]
		= g_signal_new ("handle_uri_list",
		                G_TYPE_FROM_CLASS (class),
		                G_SIGNAL_RUN_LAST,
		                G_STRUCT_OFFSET (NolphinIconContainerClass,
						     handle_uri_list),
		                NULL, NULL,
		                g_cclosure_marshal_generic,
		                G_TYPE_NONE, 5,
				G_TYPE_STRING,
				G_TYPE_STRING,
				GDK_TYPE_DRAG_ACTION,
				G_TYPE_INT,
				G_TYPE_INT);
	signals[HANDLE_TEXT]
		= g_signal_new ("handle_text",
		                G_TYPE_FROM_CLASS (class),
		                G_SIGNAL_RUN_LAST,
		                G_STRUCT_OFFSET (NolphinIconContainerClass,
						 handle_text),
		                NULL, NULL,
		                g_cclosure_marshal_generic,
		                G_TYPE_NONE, 5,
				G_TYPE_STRING,
				G_TYPE_STRING,
				GDK_TYPE_DRAG_ACTION,
				G_TYPE_INT,
				G_TYPE_INT);
	signals[HANDLE_RAW]
		= g_signal_new ("handle_raw",
		                G_TYPE_FROM_CLASS (class),
		                G_SIGNAL_RUN_LAST,
		                G_STRUCT_OFFSET (NolphinIconContainerClass,
						 handle_raw),
		                NULL, NULL,
		                g_cclosure_marshal_generic,
		                G_TYPE_NONE, 7,
				G_TYPE_POINTER,
				G_TYPE_INT,
				G_TYPE_STRING,
				G_TYPE_STRING,
				GDK_TYPE_DRAG_ACTION,
				G_TYPE_INT,
				G_TYPE_INT);
	signals[GET_CONTAINER_URI]
		= g_signal_new ("get_container_uri",
		                G_TYPE_FROM_CLASS (class),
		                G_SIGNAL_RUN_LAST,
		                G_STRUCT_OFFSET (NolphinIconContainerClass,
						 get_container_uri),
		                NULL, NULL,
		                g_cclosure_marshal_generic,
		                G_TYPE_STRING, 0);
	signals[CAN_ACCEPT_ITEM]
		= g_signal_new ("can_accept_item",
		                G_TYPE_FROM_CLASS (class),
		                G_SIGNAL_RUN_LAST,
		                G_STRUCT_OFFSET (NolphinIconContainerClass,
						 can_accept_item),
		                NULL, NULL,
		                g_cclosure_marshal_generic,
		                G_TYPE_INT, 2,
				G_TYPE_POINTER,
				G_TYPE_STRING);
	signals[GET_STORED_LAYOUT_TIMESTAMP]
		= g_signal_new ("get_stored_layout_timestamp",
		                G_TYPE_FROM_CLASS (class),
		                G_SIGNAL_RUN_LAST,
		                G_STRUCT_OFFSET (NolphinIconContainerClass,
						 get_stored_layout_timestamp),
		                NULL, NULL,
		                g_cclosure_marshal_generic,
		                G_TYPE_BOOLEAN, 2,
				G_TYPE_POINTER,
				G_TYPE_POINTER);
	signals[STORE_LAYOUT_TIMESTAMP]
		= g_signal_new ("store_layout_timestamp",
		                G_TYPE_FROM_CLASS (class),
		                G_SIGNAL_RUN_LAST,
		                G_STRUCT_OFFSET (NolphinIconContainerClass,
						 store_layout_timestamp),
		                NULL, NULL,
		                g_cclosure_marshal_generic,
		                G_TYPE_BOOLEAN, 2,
				G_TYPE_POINTER,
				G_TYPE_POINTER);
	signals[LAYOUT_CHANGED]
		= g_signal_new ("layout_changed",
		                G_TYPE_FROM_CLASS (class),
		                G_SIGNAL_RUN_LAST,
		                G_STRUCT_OFFSET (NolphinIconContainerClass,
						 layout_changed),
		                NULL, NULL,
		                g_cclosure_marshal_VOID__VOID,
		                G_TYPE_NONE, 0);
	signals[BAND_SELECT_STARTED]
		= g_signal_new ("band_select_started",
		                G_TYPE_FROM_CLASS (class),
		                G_SIGNAL_RUN_LAST,
		                G_STRUCT_OFFSET (NolphinIconContainerClass,
						 band_select_started),
		                NULL, NULL,
		                g_cclosure_marshal_VOID__VOID,
		                G_TYPE_NONE, 0);
	signals[BAND_SELECT_ENDED]
		= g_signal_new ("band_select_ended",
		                G_TYPE_FROM_CLASS (class),
		                G_SIGNAL_RUN_LAST,
		                G_STRUCT_OFFSET (NolphinIconContainerClass,
						     band_select_ended),
		                NULL, NULL,
		                g_cclosure_marshal_VOID__VOID,
		                G_TYPE_NONE, 0);
	signals[ICON_ADDED]
		= g_signal_new ("icon_added",
		                G_TYPE_FROM_CLASS (class),
		                G_SIGNAL_RUN_LAST,
		                G_STRUCT_OFFSET (NolphinIconContainerClass,
						 icon_added),
		                NULL, NULL,
		                g_cclosure_marshal_VOID__POINTER,
		                G_TYPE_NONE, 1, G_TYPE_POINTER);
	signals[ICON_REMOVED]
		= g_signal_new ("icon_removed",
		                G_TYPE_FROM_CLASS (class),
		                G_SIGNAL_RUN_LAST,
		                G_STRUCT_OFFSET (NolphinIconContainerClass,
						 icon_removed),
		                NULL, NULL,
		                g_cclosure_marshal_VOID__POINTER,
		                G_TYPE_NONE, 1, G_TYPE_POINTER);

	signals[CLEARED]
		= g_signal_new ("cleared",
		                G_TYPE_FROM_CLASS (class),
		                G_SIGNAL_RUN_LAST,
		                G_STRUCT_OFFSET (NolphinIconContainerClass,
						 cleared),
		                NULL, NULL,
		                g_cclosure_marshal_VOID__VOID,
		                G_TYPE_NONE, 0);

    signals[GET_TOOLTIP_TEXT]
        = g_signal_new ("get-tooltip-text",
                        G_TYPE_FROM_CLASS (class),
                        G_SIGNAL_RUN_LAST,
                        0,
                        NULL, NULL,
                        NULL,
                        G_TYPE_STRING, 1,
                        G_TYPE_POINTER);

    signals[CHECK_FILTER_EVENT]
        = g_signal_new ("check-filter-event",
                        G_TYPE_FROM_CLASS (class),
                        G_SIGNAL_RUN_LAST,
                        0,
                        g_signal_accumulator_true_handled, NULL,
                        g_cclosure_marshal_generic,
                        G_TYPE_BOOLEAN, 1,
                        GDK_TYPE_EVENT);

	/* GtkWidget class.  */

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->destroy = destroy;
	widget_class->size_allocate = size_allocate;
	widget_class->get_request_mode = get_request_mode;
	widget_class->get_preferred_width = get_prefered_width;
	widget_class->get_preferred_height = get_prefered_height;
	widget_class->realize = realize;
	widget_class->unrealize = unrealize;
	widget_class->button_press_event = button_press_event;
	widget_class->button_release_event = button_release_event;
	widget_class->motion_notify_event = motion_notify_event;
	widget_class->key_press_event = key_press_event;
	widget_class->popup_menu = popup_menu;
	widget_class->get_accessible = get_accessible;
	widget_class->style_updated = style_updated;
	widget_class->grab_notify = grab_notify_cb;

	canvas_class = EEL_CANVAS_CLASS (class);
	canvas_class->draw_background = draw_canvas_background;

	gtk_widget_class_install_style_property (widget_class,
						 g_param_spec_boolean ("activate_prelight_icon_label",
								     "Activate Prelight Icon Label",
								     "Whether icon labels should make use of its prelight color in prelight state",
								     TRUE,
								     G_PARAM_READABLE));
}

static void
update_selected (NolphinIconContainer *container)
{
	GList *node;
	NolphinIcon *icon;

	for (node = container->details->icons; node != NULL; node = node->next) {
		icon = node->data;
		if (icon->is_selected) {
			eel_canvas_item_request_update (EEL_CANVAS_ITEM (icon->item));
		}
	}
}

static gboolean
handle_focus_in_event (GtkWidget *widget, GdkEventFocus *event, gpointer user_data)
{
	update_selected (NOLPHIN_ICON_CONTAINER (widget));

	return FALSE;
}

static gboolean
handle_focus_out_event (GtkWidget *widget, GdkEventFocus *event, gpointer user_data)
{
	/* End renaming and commit change. */
	nolphin_icon_container_end_renaming_mode (NOLPHIN_ICON_CONTAINER (widget), TRUE);
	update_selected (NOLPHIN_ICON_CONTAINER (widget));

	return FALSE;
}

static void
nolphin_icon_container_init (NolphinIconContainer *container)
{
	NolphinIconContainerDetails *details;

	details = g_new0 (NolphinIconContainerDetails, 1);

	details->icon_set = g_hash_table_new (g_direct_hash, g_direct_equal);
	details->layout_timestamp = UNDEFINED_TIME;
	details->zoom_level = NOLPHIN_ZOOM_LEVEL_STANDARD;

	details->font_size_table[NOLPHIN_ZOOM_LEVEL_SMALLEST] = -2 * PANGO_SCALE;
	details->font_size_table[NOLPHIN_ZOOM_LEVEL_SMALLER] = -2 * PANGO_SCALE;
	details->font_size_table[NOLPHIN_ZOOM_LEVEL_SMALL] = -0 * PANGO_SCALE;
	details->font_size_table[NOLPHIN_ZOOM_LEVEL_STANDARD] = 0 * PANGO_SCALE;
	details->font_size_table[NOLPHIN_ZOOM_LEVEL_LARGE] = 0 * PANGO_SCALE;
	details->font_size_table[NOLPHIN_ZOOM_LEVEL_LARGER] = 0 * PANGO_SCALE;
	details->font_size_table[NOLPHIN_ZOOM_LEVEL_LARGEST] = 0 * PANGO_SCALE;

    details->fixed_text_height = -1;

    details->view_constants = g_new0 (NolphinViewLayoutConstants, 1);

	container->details = details;

	g_signal_connect (container, "focus-in-event",
			  G_CALLBACK (handle_focus_in_event), NULL);
	g_signal_connect (container, "focus-out-event",
			  G_CALLBACK (handle_focus_out_event), NULL);

    g_signal_connect_swapped (nolphin_preferences,
                              "changed::" NOLPHIN_PREFERENCES_TOOLTIPS_DESKTOP,
                              G_CALLBACK (tooltip_prefs_changed_callback),
                              container);

    g_signal_connect_swapped (nolphin_preferences,
                              "changed::" NOLPHIN_PREFERENCES_TOOLTIPS_ICON_VIEW,
                              G_CALLBACK (tooltip_prefs_changed_callback),
                              container);

    g_signal_connect_swapped (nolphin_preferences,
                              "changed::" NOLPHIN_PREFERENCES_TOOLTIP_FILE_TYPE,
                              G_CALLBACK (tooltip_prefs_changed_callback),
                              container);

    g_signal_connect_swapped (nolphin_preferences,
                              "changed::" NOLPHIN_PREFERENCES_TOOLTIP_MOD_DATE,
                              G_CALLBACK (tooltip_prefs_changed_callback),
                              container);

    g_signal_connect_swapped (nolphin_preferences,
                              "changed::" NOLPHIN_PREFERENCES_TOOLTIP_ACCESS_DATE,
                              G_CALLBACK (tooltip_prefs_changed_callback),
                              container);

    g_signal_connect_swapped (nolphin_preferences,
                              "changed::" NOLPHIN_PREFERENCES_TOOLTIP_FULL_PATH,
                              G_CALLBACK (tooltip_prefs_changed_callback),
                              container);

    container->details->show_desktop_tooltips = g_settings_get_boolean (nolphin_preferences,
                                                                        NOLPHIN_PREFERENCES_TOOLTIPS_DESKTOP);
    container->details->show_icon_view_tooltips = g_settings_get_boolean (nolphin_preferences,
                                                                          NOLPHIN_PREFERENCES_TOOLTIPS_ICON_VIEW);
    container->details->tooltip_flags = nolphin_global_preferences_get_tooltip_flags ();

    details->skip_rename_on_release = FALSE;
    details->dnd_grid = NULL;
    details->current_selection_count = -1;
    details->renaming_allocation_count = 0;

    details->update_visible_icons_id = 0;
    details->ok_to_load_deferred_attrs = FALSE;

    details->h_adjust = 100;
    details->v_adjust = 100;
}

typedef struct {
	NolphinIconContainer *container;
	GdkEventButton	      *event;
} ContextMenuParameters;

static gboolean
handle_icon_double_click (NolphinIconContainer *container,
			  NolphinIcon *icon,
			  GdkEventButton *event)
{
	NolphinIconContainerDetails *details;

	if (event->button != DRAG_BUTTON) {
		return FALSE;
	}

	details = container->details;

	if (!details->single_click_mode &&
	    clicked_within_double_click_interval (container) &&
	    details->double_click_icon[0] == details->double_click_icon[1] &&
	    details->double_click_button[0] == details->double_click_button[1]) {
		if (!button_event_modifies_selection (event)) {
			activate_selected_items (container);
			return TRUE;
		} else if ((event->state & GDK_CONTROL_MASK) == 0 &&
			   (event->state & GDK_SHIFT_MASK) != 0) {
			activate_selected_items_alternate (container, icon);
			return TRUE;
		}
	}

	return FALSE;
}

static gboolean
handle_icon_slow_two_click (NolphinIconContainer *container,
                                     NolphinIcon *icon,
                               GdkEventButton *event)
{
    NolphinIconContainerDetails *details;

    NolphinFile *file = NOLPHIN_FILE (icon->data);

    if (!nolphin_file_can_rename (file))
        return FALSE;

    if (event->button != DRAG_BUTTON) {
        return FALSE;
    }

    details = container->details;

    if (!details->click_to_rename)
        return FALSE;

    GList *selection = nolphin_icon_container_get_selection (container);
    gint selected_count = g_list_length (selection);
    g_list_free (selection);

    if (selected_count != 1)
        return FALSE;

    if (!details->single_click_mode &&
        clicked_within_slow_click_interval_on_text (container, icon, event) &&
        details->double_click_icon[0] == details->double_click_icon[1] &&
        details->double_click_button[0] == details->double_click_button[1]) {
        if (!button_event_modifies_selection (event)) {
            return TRUE;
        }
    }

    return FALSE;
}

/* NolphinIcon event handling.  */

/* Conceptually, pressing button 1 together with CTRL or SHIFT toggles
 * selection of a single icon without affecting the other icons;
 * without CTRL or SHIFT, it selects a single icon and un-selects all
 * the other icons.  But in this latter case, the de-selection should
 * only happen when the button is released if the icon is already
 * selected, because the user might select multiple icons and drag all
 * of them by doing a simple click-drag.
*/

static gboolean
handle_icon_button_press (NolphinIconContainer *container,
			  NolphinIcon *icon,
			  GdkEventButton *event)
{
	NolphinIconContainerDetails *details;

	details = container->details;

    details->skip_rename_on_release = FALSE;

	if (event->type == GDK_2BUTTON_PRESS || event->type == GDK_3BUTTON_PRESS) {
		return TRUE;
	}

	if (event->button != DRAG_BUTTON
	    && event->button != CONTEXTUAL_MENU_BUTTON
	    && event->button != DRAG_MENU_BUTTON) {
		return TRUE;
	}

	if ((event->button == DRAG_BUTTON) &&
	    event->type == GDK_BUTTON_PRESS) {
		/* The next double click has to be on this icon */
		details->double_click_icon[1] = details->double_click_icon[0];
		details->double_click_icon[0] = icon;

		details->double_click_button[1] = details->double_click_button[0];
		details->double_click_button[0] = event->button;
	}

    if (handle_icon_double_click (container, icon, event)) {
		/* Double clicking does not trigger a D&D action. */
		details->drag_button = 0;
		details->drag_icon = NULL;
		return TRUE;
	}

	if (event->button == DRAG_BUTTON
	    || event->button == DRAG_MENU_BUTTON) {
			details->drag_button = event->button;
		details->drag_icon = icon;
		details->drag_x = event->x;
		details->drag_y = event->y;
		details->drag_state = DRAG_STATE_MOVE_OR_COPY;
		details->drag_started = FALSE;

		/* Check to see if this is a click on the stretch handles.
		 * If so, it won't modify the selection.
		 */
		if (icon == container->details->stretch_icon) {
			if (start_stretching (container)) {
				return TRUE;
			}
		}
	}

	/* Modify the selection as appropriate. Selection is modified
	 * the same way for contextual menu as it would be without.
	 */
	details->icon_selected_on_button_down = icon->is_selected;

    GList *sel = nolphin_icon_container_get_selected_icons (container);
    details->skip_rename_on_release = g_list_length (sel) > 1;
    g_list_free (sel);

	if ((event->button == DRAG_BUTTON || event->button == MIDDLE_BUTTON) &&
	    (event->state & GDK_SHIFT_MASK) != 0) {
		NolphinIcon *start_icon;

		start_icon = details->range_selection_base_icon;
		if (start_icon == NULL || !start_icon->is_selected) {
			start_icon = icon;
			details->range_selection_base_icon = icon;
		}
		if (select_range (container, start_icon, icon,
				  (event->state & GDK_CONTROL_MASK) == 0)) {
			g_signal_emit (container,
				       signals[SELECTION_CHANGED], 0);
		}
	} else if (!details->icon_selected_on_button_down) {
		details->range_selection_base_icon = icon;
		if (button_event_modifies_selection (event)) {
			icon_toggle_selected (container, icon);
			g_signal_emit (container,
				       signals[SELECTION_CHANGED], 0);
		} else {
			select_one_unselect_others (container, icon);
			g_signal_emit (container,
				       signals[SELECTION_CHANGED], 0);
		}
	}

	if (event->button == CONTEXTUAL_MENU_BUTTON) {
		clear_drag_state (container);

		g_signal_emit (container,
			       signals[CONTEXT_CLICK_SELECTION], 0,
			       event);
	}


	return TRUE;
}

static int
item_event_callback (EelCanvasItem *item,
		     GdkEvent *event,
		     gpointer data)
{
	NolphinIconContainer *container;
	NolphinIcon *icon;

	container = NOLPHIN_ICON_CONTAINER (data);

	icon = NOLPHIN_ICON_CANVAS_ITEM (item)->user_data;
	g_assert (icon != NULL);

    if (event->type == GDK_BUTTON_PRESS) {
        if (handle_icon_button_press (container, icon, &event->button)) {
			/* Stop the event from being passed along further. Returning
			 * TRUE ain't enough.
			 */
			return TRUE;
		}
    }
    return FALSE;
}

GtkWidget *
nolphin_icon_container_new (void)
{
	return gtk_widget_new (NOLPHIN_TYPE_ICON_CONTAINER, NULL);
}

/* Clear all of the icons in the container. */
void
nolphin_icon_container_clear (NolphinIconContainer *container)
{
	NolphinIconContainerDetails *details;
	GList *p;

	g_return_if_fail (NOLPHIN_IS_ICON_CONTAINER (container));

	details = container->details;
	details->layout_timestamp = UNDEFINED_TIME;
	details->store_layout_timestamps_when_finishing_new_icons = FALSE;

    details->fixed_text_height = -1;

    if (container->details->update_visible_icons_id > 0) {
        g_source_remove (container->details->update_visible_icons_id);
        container->details->update_visible_icons_id = 0;
    }

	if (details->icons == NULL) {
		return;
	}

	nolphin_icon_container_end_renaming_mode (container, TRUE);

	clear_keyboard_focus (container);
	clear_keyboard_rubberband_start (container);
	unschedule_keyboard_icon_reveal (container);
	set_pending_icon_to_reveal (container, NULL);
	details->stretch_icon = NULL;
	details->drop_target = NULL;

    details->ok_to_load_deferred_attrs = FALSE;

	for (p = details->icons; p != NULL; p = p->next) {
		icon_free (p->data);
	}
	g_list_free (details->icons);
	details->icons = NULL;
	g_list_free (details->new_icons);
	details->new_icons = NULL;

 	g_hash_table_destroy (details->icon_set);
 	details->icon_set = g_hash_table_new (g_direct_hash, g_direct_equal);
}

gboolean
nolphin_icon_container_is_empty (NolphinIconContainer *container)
{
	return container->details->icons == NULL;
}

NolphinIconData *
nolphin_icon_container_get_first_visible_icon (NolphinIconContainer *container)
{
	GList *l;
	NolphinIcon *icon, *best_icon;
	double x, y;
	double x1, y1, x2, y2;
	double *pos, best_pos;
	double hadj_v, vadj_v, h_page_size;
	gboolean better_icon;
	gboolean compare_lt;

	hadj_v = gtk_adjustment_get_value (gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (container)));
	vadj_v = gtk_adjustment_get_value (gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (container)));
	h_page_size = gtk_adjustment_get_page_size (gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (container)));

	if (nolphin_icon_container_is_layout_rtl (container)) {
		x = hadj_v + h_page_size - GET_VIEW_CONSTANT (container, icon_pad_left) - 1;
		y = vadj_v;
	} else {
		x = hadj_v;
		y = vadj_v;
	}

	eel_canvas_c2w (EEL_CANVAS (container),
			x, y,
			&x, &y);

	l = container->details->icons;
	best_icon = NULL;
	best_pos = 0;
	while (l != NULL) {
		icon = l->data;

		if (nolphin_icon_container_icon_is_positioned (icon)) {
			eel_canvas_item_get_bounds (EEL_CANVAS_ITEM (icon->item),
						    &x1, &y1, &x2, &y2);

			compare_lt = FALSE;
			if (nolphin_icon_container_is_layout_vertical (container)) {
				pos = &x1;
				if (nolphin_icon_container_is_layout_rtl (container)) {
					compare_lt = TRUE;
					better_icon = x1 < x + GET_VIEW_CONSTANT (container, icon_pad_left);
				} else {
					better_icon = x2 > x + GET_VIEW_CONSTANT (container, icon_pad_left);
				}
			} else {
				pos = &y1;
				better_icon = y2 > y + GET_VIEW_CONSTANT (container, icon_pad_top);
			}
			if (better_icon) {
				if (best_icon == NULL) {
					better_icon = TRUE;
				} else if (compare_lt) {
					better_icon = best_pos < *pos;
				} else {
					better_icon = best_pos > *pos;
				}

				if (better_icon) {
					best_icon = icon;
					best_pos = *pos;
				}
			}
		}

		l = l->next;
	}

	return best_icon ? best_icon->data : NULL;
}

/* puts the icon at the top of the screen */
void
nolphin_icon_container_scroll_to_icon (NolphinIconContainer  *container,
					NolphinIconData       *data)
{
	GList *l;
	NolphinIcon *icon;
	GtkAdjustment *hadj, *vadj;
	EelIRect bounds;
	GtkAllocation allocation;

	hadj = gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (container));
	vadj = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (container));
	gtk_widget_get_allocation (GTK_WIDGET (container), &allocation);

	/* We need to force a relayout now if there are updates queued
	 * since we need the final positions */
	nolphin_icon_container_layout_now (container);

	l = container->details->icons;
	while (l != NULL) {
		icon = l->data;

		if (icon->data == data &&
		    nolphin_icon_container_icon_is_positioned (icon)) {

			if (nolphin_icon_container_is_auto_layout (container)) {
				/* ensure that we reveal the entire row/column */
				icon_get_row_and_column_bounds (container, icon, &bounds, TRUE);
			} else {
				item_get_canvas_bounds (container, EEL_CANVAS_ITEM (icon->item), &bounds, TRUE);
			}

			if (nolphin_icon_container_is_layout_vertical (container)) {
				if (nolphin_icon_container_is_layout_rtl (container)) {
					gtk_adjustment_set_value (hadj, bounds.x1 - allocation.width);
				} else {
					gtk_adjustment_set_value (hadj, bounds.x0);
				}
			} else {
				gtk_adjustment_set_value (vadj, bounds.y0);
			}
		}

		l = l->next;
	}
}

/* Call a function for all the icons. */
typedef struct {
	NolphinIconCallback callback;
	gpointer callback_data;
} CallbackAndData;

static void
call_icon_callback (gpointer data, gpointer callback_data)
{
	NolphinIcon *icon;
	CallbackAndData *callback_and_data;

	icon = data;
	callback_and_data = callback_data;
	(* callback_and_data->callback) (icon->data, callback_and_data->callback_data);
}

void
nolphin_icon_container_for_each (NolphinIconContainer *container,
				  NolphinIconCallback callback,
				  gpointer callback_data)
{
	CallbackAndData callback_and_data;

	g_return_if_fail (NOLPHIN_IS_ICON_CONTAINER (container));

	callback_and_data.callback = callback;
	callback_and_data.callback_data = callback_data;

	g_list_foreach (container->details->icons,
			call_icon_callback, &callback_and_data);
}

static int
selection_changed_at_idle_callback (gpointer data)
{
	NolphinIconContainer *container;

	container = NOLPHIN_ICON_CONTAINER (data);

	g_signal_emit (container,
		       signals[SELECTION_CHANGED], 0);

	container->details->selection_changed_id = 0;
	return FALSE;
}

/* utility routine to remove a single icon from the container */

static void
icon_destroy (NolphinIconContainer *container,
	      NolphinIcon *icon)
{
	NolphinIconContainerDetails *details;
	gboolean was_selected;
	NolphinIcon *icon_to_focus;
	GList *item;

	details = container->details;

	item = g_list_find (details->icons, icon);
	item = item->next ? item->next : item->prev;
	icon_to_focus = (item != NULL) ? item->data : NULL;

	details->icons = g_list_remove (details->icons, icon);
	details->new_icons = g_list_remove (details->new_icons, icon);
	g_hash_table_remove (details->icon_set, icon->data);

	was_selected = icon->is_selected;

	if (details->keyboard_focus == icon ||
	    details->keyboard_focus == NULL) {
		if (icon_to_focus != NULL) {
			set_keyboard_focus (container, icon_to_focus);
		} else {
			clear_keyboard_focus (container);
		}
	}

	if (details->keyboard_rubberband_start == icon) {
		clear_keyboard_rubberband_start (container);
	}

	if (details->keyboard_icon_to_reveal == icon) {
		unschedule_keyboard_icon_reveal (container);
	}
	if (details->drag_icon == icon) {
		clear_drag_state (container);
	}
	if (details->drop_target == icon) {
		details->drop_target = NULL;
	}
	if (details->range_selection_base_icon == icon) {
		details->range_selection_base_icon = NULL;
	}
	if (details->pending_icon_to_reveal == icon) {
		set_pending_icon_to_reveal (container, NULL);
	}
	if (details->stretch_icon == icon) {
		details->stretch_icon = NULL;
	}

	icon_free (icon);

	if (was_selected) {
		/* Coalesce multiple removals causing multiple selection_changed events */
		details->selection_changed_id = g_idle_add (selection_changed_at_idle_callback, container);
	}
}

/* activate any selected items in the container */
static void
activate_selected_items (NolphinIconContainer *container)
{
	GList *selection;

	g_return_if_fail (NOLPHIN_IS_ICON_CONTAINER (container));

	selection = nolphin_icon_container_get_selection (container);
	if (selection != NULL) {
	  	g_signal_emit (container,
				 signals[ACTIVATE], 0,
				 selection);
	}

    g_list_free (selection);
}

static void
preview_selected_items (NolphinIconContainer *container)
{
	GList *selection;
	GArray *locations;
	guint idx;

	g_return_if_fail (NOLPHIN_IS_ICON_CONTAINER (container));

	selection = nolphin_icon_container_get_selection (container);
	locations = nolphin_icon_container_get_selected_icon_locations (container);

	for (idx = 0; idx < locations->len; idx++) {
		GdkPoint *point = &(g_array_index (locations, GdkPoint, idx));
		gint scroll_x, scroll_y;

		eel_canvas_get_scroll_offsets (EEL_CANVAS (container),
					       &scroll_x, &scroll_y);

		point->x -= scroll_x;
		point->y -= scroll_y;
	}

	if (selection != NULL) {
	  	g_signal_emit (container,
			       signals[ACTIVATE_PREVIEWER], 0,
			       selection, locations);
	}

    g_list_free (selection);
}

static void
activate_selected_items_alternate (NolphinIconContainer *container,
				   NolphinIcon *icon)
{
	GList *selection;

	g_assert (NOLPHIN_IS_ICON_CONTAINER (container));

	if (icon != NULL) {
		selection = g_list_prepend (NULL, icon->data);
	} else {
		selection = nolphin_icon_container_get_selection (container);
	}
	if (selection != NULL) {
	  	g_signal_emit (container,
				 signals[ACTIVATE_ALTERNATE], 0,
				 selection);
	}
	g_list_free (selection);
}

NolphinIconInfo *
nolphin_icon_container_get_icon_images (NolphinIconContainer *container,
                                     NolphinIconData      *data,
                                     int                size,
                                     gboolean           for_drag_accept,
                                     gboolean          *has_open_window,
                                     gboolean           visible)
{
	NolphinIconContainerClass *klass;

	klass = NOLPHIN_ICON_CONTAINER_GET_CLASS (container);
	g_assert (klass->get_icon_images != NULL);

	return klass->get_icon_images (container, data, size, for_drag_accept, has_open_window, visible);
}

static void
nolphin_icon_container_freeze_updates (NolphinIconContainer *container)
{
	NolphinIconContainerClass *klass;

	klass = NOLPHIN_ICON_CONTAINER_GET_CLASS (container);
	g_assert (klass->freeze_updates != NULL);

	klass->freeze_updates (container);
}

static void
nolphin_icon_container_unfreeze_updates (NolphinIconContainer *container)
{
	NolphinIconContainerClass *klass;

	klass = NOLPHIN_ICON_CONTAINER_GET_CLASS (container);
	g_assert (klass->unfreeze_updates != NULL);

	klass->unfreeze_updates (container);
}

static gboolean
update_visible_icons_cb (NolphinIconContainer *container)
{
	GtkAdjustment *vadj, *hadj;
	double min_y, max_y;
	double min_x, max_x;
	double x0, y0, x1, y1;
	GList *node;
	NolphinIcon *icon;
	gboolean visible;
	GtkAllocation allocation;

    container->details->update_visible_icons_id = 0;

	hadj = gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (container));
	vadj = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (container));
	gtk_widget_get_allocation (GTK_WIDGET (container), &allocation);

	min_x = gtk_adjustment_get_value (hadj);
	max_x = min_x + allocation.width;

	min_y = gtk_adjustment_get_value (vadj);
	max_y = min_y + allocation.height;

	eel_canvas_c2w (EEL_CANVAS (container),
			min_x, min_y, &min_x, &min_y);
	eel_canvas_c2w (EEL_CANVAS (container),
			max_x, max_y, &max_x, &max_y);

	for (node = g_list_last (container->details->icons); node != NULL; node = node->prev) {
		icon = node->data;

		if (nolphin_icon_container_icon_is_positioned (icon)) {
			eel_canvas_item_get_bounds (EEL_CANVAS_ITEM (icon->item),
						    &x0,
						    &y0,
						    &x1,
						    &y1);
			eel_canvas_item_i2w (EEL_CANVAS_ITEM (icon->item)->parent,
					     &x0,
					     &y0);
			eel_canvas_item_i2w (EEL_CANVAS_ITEM (icon->item)->parent,
					     &x1,
					     &y1);

            gint overshoot;

			if (nolphin_icon_container_is_layout_vertical (container)) {
                overshoot = (max_x - min_x) / 2;

				visible = x1 >= min_x - overshoot && x0 <= max_x + overshoot;
			} else {
                overshoot = (max_y - min_y) / 2;

				visible = y1 >= min_y - overshoot && y0 <= max_y + overshoot;
			}

			if (visible) {
				nolphin_icon_canvas_item_set_is_visible (icon->item, TRUE);
                NolphinFile *file = NOLPHIN_FILE (icon->data);

                if (!icon->ok_to_show_thumb) {

                    icon->ok_to_show_thumb = TRUE;

                    if (nolphin_file_get_load_deferred_attrs (file) == NOLPHIN_FILE_LOAD_DEFERRED_ATTRS_NO) {
                        nolphin_file_set_load_deferred_attrs (file, NOLPHIN_FILE_LOAD_DEFERRED_ATTRS_YES);
                    }

                    nolphin_file_invalidate_attributes (file, NOLPHIN_FILE_DEFERRED_ATTRIBUTES);
                } else {
                    gchar *uri = nolphin_file_get_uri (file);
                    nolphin_thumbnail_prioritize (uri);
                    g_free (uri);
                }

                nolphin_icon_container_update_icon (container, icon);
			} else {
				nolphin_icon_canvas_item_set_is_visible (icon->item, FALSE);
			}
		}
	}

    return G_SOURCE_REMOVE;
}

static void
queue_update_visible_icons(NolphinIconContainer *container,
                           gint               delay)
{
    NolphinIconContainerDetails *details = container->details;

    if (details->update_visible_icons_id > 0) {
        g_source_remove (details->update_visible_icons_id);
    }

    details->update_visible_icons_id = g_timeout_add (delay, (GSourceFunc) update_visible_icons_cb, container);
}

static void
handle_vadjustment_changed (GtkAdjustment *adjustment,
			    NolphinIconContainer *container)
{
	if (!nolphin_icon_container_is_layout_vertical (container)) {
		queue_update_visible_icons (container, NORMAL_UPDATE_VISIBLE_DELAY);
	}
}

static void
handle_hadjustment_changed (GtkAdjustment *adjustment,
			    NolphinIconContainer *container)
{
	if (nolphin_icon_container_is_layout_vertical (container)) {
		queue_update_visible_icons (container, NORMAL_UPDATE_VISIBLE_DELAY);
	}
}

static gboolean
is_old_or_unknown_icon_data (NolphinIconContainer *container,
			     NolphinIconData *data)
{
	time_t timestamp;
	gboolean success;
    gboolean is_transient;

    /* Undefined at startup */
	if (container->details->layout_timestamp == UNDEFINED_TIME) {
		return FALSE;
	}

    is_transient = NOLPHIN_IS_DESKTOP_ICON_FILE (data) && container->details->keep_aligned;

	g_signal_emit (container,
		       signals[GET_STORED_LAYOUT_TIMESTAMP], 0,
		       data, &timestamp, &success);

	return (!success || is_transient || timestamp < container->details->layout_timestamp);
}

gboolean
nolphin_icon_container_icon_is_new_for_monitor (NolphinIconContainer *container,
                                             NolphinIcon          *icon,
                                             gint               current_monitor)
{
    if (container->details->auto_layout || !container->details->is_desktop) {
        return FALSE;
    }

    return nolphin_file_get_monitor_number (NOLPHIN_FILE (icon->data)) != current_monitor;
}

/**
 * nolphin_icon_container_add:
 * @container: A NolphinIconContainer
 * @data: Icon data.
 *
 * Add icon to represent @data to container.
 * Returns FALSE if there was already such an icon.
 **/
gboolean
nolphin_icon_container_add (NolphinIconContainer *container,
			     NolphinIconData *data)
{
	NolphinIconContainerDetails *details;
	NolphinIcon *icon;
	EelCanvasItem *band, *item;

	g_return_val_if_fail (NOLPHIN_IS_ICON_CONTAINER (container), FALSE);
	g_return_val_if_fail (data != NULL, FALSE);

	details = container->details;

	if (g_hash_table_lookup (details->icon_set, data) != NULL) {
		return FALSE;
	}

	/* Create the new icon, including the canvas item. */
	icon = g_new0 (NolphinIcon, 1);
	icon->data = data;
	icon->x = ICON_UNPOSITIONED_VALUE;
	icon->y = ICON_UNPOSITIONED_VALUE;

	/* Whether the saved icon position should only be used
	 * if the previous icon position is free. If the position
	 * is occupied, another position near the last one will
	 */
	icon->has_lazy_position = is_old_or_unknown_icon_data (container, data);
	icon->scale = 1.0;
 	icon->item = NOLPHIN_ICON_CANVAS_ITEM
		(eel_canvas_item_new (EEL_CANVAS_GROUP (EEL_CANVAS (container)->root),
				      nolphin_icon_canvas_item_get_type (),
				      "visible", FALSE,
				      NULL));
	icon->item->user_data = icon;

	/* Make sure the icon is under the selection_rectangle */
	item = EEL_CANVAS_ITEM (icon->item);
	band = NOLPHIN_ICON_CONTAINER (item->canvas)->details->rubberband_info.selection_rectangle;
	if (band) {
		eel_canvas_item_send_behind (item, band);
	}

	/* Put it on both lists. */
	details->icons = g_list_prepend (details->icons, icon);
	details->new_icons = g_list_prepend (details->new_icons, icon);

	g_hash_table_insert (details->icon_set, data, icon);

	details->needs_resort = TRUE;

	/* Run an idle function to add the icons. */
	schedule_redo_layout (container);

	return TRUE;
}

void
nolphin_icon_container_layout_now (NolphinIconContainer *container)
{
	if (container->details->idle_id != 0) {
		unschedule_redo_layout (container);
		redo_layout_internal (container);
	}

	/* Also need to make sure we're properly resized, for instance
	 * newly added files may trigger a change in the size allocation and
	 * thus toggle scrollbars on */
	gtk_container_check_resize (GTK_CONTAINER (gtk_widget_get_parent (GTK_WIDGET (container))));
}

/**
 * nolphin_icon_container_remove:
 * @container: A NolphinIconContainer.
 * @data: Icon data.
 *
 * Remove the icon with this data.
 **/
gboolean
nolphin_icon_container_remove (NolphinIconContainer *container,
				NolphinIconData *data)
{
	NolphinIcon *icon;

	g_return_val_if_fail (NOLPHIN_IS_ICON_CONTAINER (container), FALSE);
	g_return_val_if_fail (data != NULL, FALSE);

	nolphin_icon_container_end_renaming_mode (container, FALSE);

	icon = g_hash_table_lookup (container->details->icon_set, data);

	if (icon == NULL) {
		return FALSE;
	}

    gtk_widget_set_tooltip_text (GTK_WIDGET (EEL_CANVAS_ITEM (icon->item)->canvas), "");
	icon_destroy (container, icon);
	schedule_redo_layout (container);

	g_signal_emit (container, signals[ICON_REMOVED], 0, icon);

	return TRUE;
}

/**
 * nolphin_icon_container_request_update:
 * @container: A NolphinIconContainer.
 * @data: Icon data.
 *
 * Update the icon with this data.
 **/
void
nolphin_icon_container_request_update (NolphinIconContainer *container,
					NolphinIconData *data)
{
	NolphinIcon *icon;

	g_return_if_fail (NOLPHIN_IS_ICON_CONTAINER (container));
	g_return_if_fail (data != NULL);

	icon = g_hash_table_lookup (container->details->icon_set, data);

	if (icon != NULL) {
		nolphin_icon_container_update_icon (container, icon);
		container->details->needs_resort = TRUE;
		schedule_redo_layout (container);
	}
}

/* invalidate the entire labels (i.e. their attributes) for all the icons */
void
nolphin_icon_container_invalidate_labels (NolphinIconContainer *container)
{
    GList *p;
    NolphinIcon *icon;

    container->details->fixed_text_height = -1;

    for (p = container->details->icons; p != NULL; p = p->next) {
        icon = p->data;

        nolphin_icon_canvas_item_invalidate_label (icon->item);
    }
}

/* zooming */

NolphinZoomLevel
nolphin_icon_container_get_zoom_level (NolphinIconContainer *container)
{
    return container->details->zoom_level;
}

void
nolphin_icon_container_set_zoom_level (NolphinIconContainer *container,
                                    gint               new_level)
{
    NOLPHIN_ICON_CONTAINER_GET_CLASS (container)->set_zoom_level (container, new_level);

    nolphin_icon_container_invalidate_labels (container);
    nolphin_icon_container_request_update_all (container);
}

/**
 * nolphin_icon_container_request_update_all:
 * For each icon, synchronizes the displayed information (image, text) with the
 * information from the model.
 *
 * @container: An icon container.
 **/
void
nolphin_icon_container_request_update_all (NolphinIconContainer *container)
{
	GList *node;
	NolphinIcon *icon;

	g_return_if_fail (NOLPHIN_IS_ICON_CONTAINER (container));

	for (node = container->details->icons; node != NULL; node = node->next) {
		icon = node->data;
		nolphin_icon_container_update_icon (container, icon);
	}

	container->details->needs_resort = TRUE;
	nolphin_icon_container_redo_layout (container);

    gtk_widget_queue_draw (GTK_WIDGET (container));
}

/**
 * nolphin_icon_container_reveal:
 * Change scroll position as necessary to reveal the specified item.
 */
void
nolphin_icon_container_reveal (NolphinIconContainer *container, NolphinIconData *data)
{
	NolphinIcon *icon;

	g_return_if_fail (NOLPHIN_IS_ICON_CONTAINER (container));
	g_return_if_fail (data != NULL);

	icon = g_hash_table_lookup (container->details->icon_set, data);

	if (icon != NULL) {
		reveal_icon (container, icon);
	}
}

GList *
nolphin_icon_container_get_real_selection (NolphinIconContainer *container)
{
    GList *list, *p;

    list = NULL;
    for (p = container->details->icons; p != NULL; p = p->next) {
        NolphinIcon *icon;

        icon = p->data;
        if (icon->is_selected) {
            list = g_list_prepend (list, icon->data);
        }
    }

    return g_list_reverse (list);
}

/**
 * nolphin_icon_container_get_selection:
 * @container: An icon container.
 *
 * Get a list of the icons currently selected in @container.
 *
 * Return value: A GList of the programmer-specified data associated to each
 * selected icon, or NULL if no icon is selected.  The caller is expected to
 * free the list when it is not needed anymore.
 **/
GList *
nolphin_icon_container_get_selection (NolphinIconContainer *container)
{
	g_return_val_if_fail (NOLPHIN_IS_ICON_CONTAINER (container), NULL);

    nolphin_icon_container_update_selection (container);

    return g_list_copy (container->details->current_selection);
}

/**
 * nolphin_icon_container_peek_selection:
 * @container: An icon container.
 *
 * Get an exiting list of the icons currently selected in @container.
 *
 * Return value: A GList of the programmer-specified data associated to each
 * selected icon, or NULL if no icon is selected.  This list belongs to the
 * NolphinIconContainer and should not be freed.
 **/
GList *
nolphin_icon_container_peek_selection (NolphinIconContainer *container)
{
    g_return_val_if_fail (NOLPHIN_IS_ICON_CONTAINER (container), NULL);

    if (container->details->current_selection_count == -1) {
        nolphin_icon_container_update_selection (container);
    }

    return container->details->current_selection;
}

gint
nolphin_icon_container_get_selection_count (NolphinIconContainer *container)
{
    g_return_val_if_fail (NOLPHIN_IS_ICON_CONTAINER (container), 0);

    if (container->details->current_selection_count == -1) {
        nolphin_icon_container_update_selection (container);
    }

    return container->details->current_selection_count;
}

static GList *
nolphin_icon_container_get_selected_icons (NolphinIconContainer *container)
{
	GList *list, *p;

	g_return_val_if_fail (NOLPHIN_IS_ICON_CONTAINER (container), NULL);

	list = NULL;
	for (p = container->details->icons; p != NULL; p = p->next) {
		NolphinIcon *icon;

		icon = p->data;
		if (icon->is_selected) {
			list = g_list_prepend (list, icon);
		}
	}

	return g_list_reverse (list);
}

/**
 * nolphin_icon_container_invert_selection:
 * @container: An icon container.
 *
 * Inverts the selection in @container.
 *
 **/
void
nolphin_icon_container_invert_selection (NolphinIconContainer *container)
{
	GList *p;

	g_return_if_fail (NOLPHIN_IS_ICON_CONTAINER (container));

	for (p = container->details->icons; p != NULL; p = p->next) {
		NolphinIcon *icon;

		icon = p->data;
		icon_toggle_selected (container, icon);
	}

	g_signal_emit (container, signals[SELECTION_CHANGED], 0);
}


/* Returns an array of GdkPoints of locations of the icons. */
static GArray *
nolphin_icon_container_get_icon_locations (NolphinIconContainer *container,
					    GList *icons)
{
	GArray *result;
	GList *node;
	int index;

	result = g_array_new (FALSE, TRUE, sizeof (GdkPoint));
	result = g_array_set_size (result, g_list_length (icons));

	for (index = 0, node = icons; node != NULL; index++, node = node->next) {
	     	g_array_index (result, GdkPoint, index).x =
	     		((NolphinIcon *)node->data)->x;
	     	g_array_index (result, GdkPoint, index).y =
			((NolphinIcon *)node->data)->y;
	}

	return result;
}

/**
 * nolphin_icon_container_get_selected_icon_locations:
 * @container: An icon container widget.
 *
 * Returns an array of GdkPoints of locations of the selected icons.
 **/
GArray *
nolphin_icon_container_get_selected_icon_locations (NolphinIconContainer *container)
{
	GArray *result;
	GList *icons;

	g_return_val_if_fail (NOLPHIN_IS_ICON_CONTAINER (container), NULL);

	icons = nolphin_icon_container_get_selected_icons (container);
	result = nolphin_icon_container_get_icon_locations (container, icons);
	g_list_free (icons);

	return result;
}

/**
 * nolphin_icon_container_select_all:
 * @container: An icon container widget.
 *
 * Select all the icons in @container at once.
 **/
void
nolphin_icon_container_select_all (NolphinIconContainer *container)
{
	gboolean selection_changed;
	GList *p;
	NolphinIcon *icon;

	g_return_if_fail (NOLPHIN_IS_ICON_CONTAINER (container));

	selection_changed = FALSE;

	for (p = container->details->icons; p != NULL; p = p->next) {
		icon = p->data;

		selection_changed |= icon_set_selected (container, icon, TRUE);
	}

	if (selection_changed) {
		g_signal_emit (container,
				 signals[SELECTION_CHANGED], 0);
	}
}

/**
 * nolphin_icon_container_select_first:
 * @container: An icon container widget.
 *
 * Selects the first icon in display order, unselecting any others.
 * The icon list is resorted first if pending changes (such as
 * a filtered add) left it out of order.
 **/
void
nolphin_icon_container_select_first (NolphinIconContainer *container)
{
    NolphinIcon *icon;

    g_return_if_fail (NOLPHIN_IS_ICON_CONTAINER (container));

    if (container->details->needs_resort) {
        nolphin_icon_container_resort (container);
        container->details->needs_resort = FALSE;
    }

    if (container->details->icons == NULL) {
        return;
    }

    icon = container->details->icons->data;

    /* Drop any stale keyboard focus from the previous selection so arrow-key
     * navigation starts from this new selection. */
    clear_keyboard_focus (container);
    clear_keyboard_rubberband_start (container);
    container->details->range_selection_base_icon = icon;

    if (select_one_unselect_others (container, icon)) {
        g_signal_emit (container, signals[SELECTION_CHANGED], 0);
    }
}

/**
 * nolphin_icon_container_set_selection:
 * @container: An icon container widget.
 * @selection: A list of NolphinIconData *.
 *
 * Set the selection to exactly the icons in @container which have
 * programmer data matching one of the items in @selection.
 **/
void
nolphin_icon_container_set_selection (NolphinIconContainer *container,
				       GList *selection)
{
	gboolean selection_changed;
	GHashTable *hash;
	GList *p;
	gboolean res;
	NolphinIcon *icon, *selected_icon;

	g_return_if_fail (NOLPHIN_IS_ICON_CONTAINER (container));

	selection_changed = FALSE;
	selected_icon = NULL;

	hash = g_hash_table_new (NULL, NULL);
	for (p = selection; p != NULL; p = p->next) {
		g_hash_table_insert (hash, p->data, p->data);
	}
	for (p = container->details->icons; p != NULL; p = p->next) {
		icon = p->data;

		res = icon_set_selected
			(container, icon,
			 g_hash_table_lookup (hash, icon->data) != NULL);
		selection_changed |= res;

		if (res) {
			selected_icon = icon;
		}
	}
	g_hash_table_destroy (hash);

	if (selection_changed) {
		/* if only one item has been selected, use it as range
		 * selection base (cf. handle_icon_button_press) */
		if (g_list_length (selection) == 1) {
			container->details->range_selection_base_icon = selected_icon;
		}

		g_signal_emit (container,
				 signals[SELECTION_CHANGED], 0);
	}
}

/**
 * nolphin_icon_container_select_list_unselect_others.
 * @container: An icon container widget.
 * @selection: A list of NolphinIcon *.
 *
 * Set the selection to exactly the icons in @selection.
 **/
void
nolphin_icon_container_select_list_unselect_others (NolphinIconContainer *container,
						     GList *selection)
{
	gboolean selection_changed;
	GHashTable *hash;
	GList *p;
	NolphinIcon *icon;

	g_return_if_fail (NOLPHIN_IS_ICON_CONTAINER (container));

	selection_changed = FALSE;

	hash = g_hash_table_new (NULL, NULL);
	for (p = selection; p != NULL; p = p->next) {
		g_hash_table_insert (hash, p->data, p->data);
	}
	for (p = container->details->icons; p != NULL; p = p->next) {
		icon = p->data;

		selection_changed |= icon_set_selected
			(container, icon,
			 g_hash_table_lookup (hash, icon) != NULL);
	}
	g_hash_table_destroy (hash);

	if (selection_changed) {
		g_signal_emit (container,
				 signals[SELECTION_CHANGED], 0);
	}
}

/**
 * nolphin_icon_container_unselect_all:
 * @container: An icon container widget.
 *
 * Deselect all the icons in @container.
 **/
void
nolphin_icon_container_unselect_all (NolphinIconContainer *container)
{
	if (unselect_all (container)) {
		g_signal_emit (container,
				 signals[SELECTION_CHANGED], 0);
	}
}

/**
 * nolphin_icon_container_get_icon_by_uri:
 * @container: An icon container widget.
 * @uri: The uri of an icon to find.
 *
 * Locate an icon, given the URI. The URI must match exactly.
 * Later we may have to have some way of figuring out if the
 * URI specifies the same object that does not require an exact match.
 **/
NolphinIcon *
nolphin_icon_container_get_icon_by_uri (NolphinIconContainer *container,
					 const char *uri)
{
	NolphinIconContainerDetails *details;
	GList *p;

	/* Eventually, we must avoid searching the entire icon list,
	   but it's OK for now.
	   A hash table mapping uri to icon is one possibility.
	*/

	details = container->details;

	for (p = details->icons; p != NULL; p = p->next) {
		NolphinIcon *icon;
		char *icon_uri;
		gboolean is_match;

		icon = p->data;

		icon_uri = nolphin_icon_container_get_icon_uri
			(container, icon);
		is_match = strcmp (uri, icon_uri) == 0;
		g_free (icon_uri);

		if (is_match) {
			return icon;
		}
	}

	return NULL;
}

static NolphinIcon *
get_nth_selected_icon (NolphinIconContainer *container, int index)
{
	GList *p;
	NolphinIcon *icon;
	int selection_count;

	g_assert (index > 0);

	/* Find the nth selected icon. */
	selection_count = 0;
	for (p = container->details->icons; p != NULL; p = p->next) {
		icon = p->data;
		if (icon->is_selected) {
			if (++selection_count == index) {
				return icon;
			}
		}
	}
	return NULL;
}

static NolphinIcon *
get_first_selected_icon (NolphinIconContainer *container)
{
        return get_nth_selected_icon (container, 1);
}

static gboolean
has_multiple_selection (NolphinIconContainer *container)
{
        return get_nth_selected_icon (container, 2) != NULL;
}

static gboolean
all_selected (NolphinIconContainer *container)
{
	GList *p;
	NolphinIcon *icon;

	for (p = container->details->icons; p != NULL; p = p->next) {
		icon = p->data;
		if (!icon->is_selected) {
			return FALSE;
		}
	}
	return TRUE;
}

static gboolean
has_selection (NolphinIconContainer *container)
{
        return get_nth_selected_icon (container, 1) != NULL;
}

/**
 * nolphin_icon_container_show_stretch_handles:
 * @container: An icon container widget.
 *
 * Makes stretch handles visible on the first selected icon.
 **/
void
nolphin_icon_container_show_stretch_handles (NolphinIconContainer *container)
{
	NolphinIconContainerDetails *details;
	NolphinIcon *icon;
	guint initial_size;

	icon = get_first_selected_icon (container);
	if (icon == NULL) {
		return;
	}

	/* Check if it already has stretch handles. */
	details = container->details;
	if (details->stretch_icon == icon) {
		return;
	}

	/* Get rid of the existing stretch handles and put them on the new icon. */
	if (details->stretch_icon != NULL) {
		nolphin_icon_canvas_item_set_show_stretch_handles
			(details->stretch_icon->item, FALSE);
		ungrab_stretch_icon (container);
		emit_stretch_ended (container, details->stretch_icon);
	}
	nolphin_icon_canvas_item_set_show_stretch_handles (icon->item, TRUE);
	details->stretch_icon = icon;

	icon_get_size (container, icon, &initial_size);

	/* only need to keep size in one dimension, since they are constrained to be the same */
	container->details->stretch_initial_x = icon->x;
	container->details->stretch_initial_y = icon->y;
	container->details->stretch_initial_size = initial_size;

	emit_stretch_started (container, icon);
}

/**
 * nolphin_icon_container_has_stretch_handles
 * @container: An icon container widget.
 *
 * Returns true if the first selected item has stretch handles.
 **/
gboolean
nolphin_icon_container_has_stretch_handles (NolphinIconContainer *container)
{
	NolphinIcon *icon;

	icon = get_first_selected_icon (container);
	if (icon == NULL) {
		return FALSE;
	}

	return icon == container->details->stretch_icon;
}

/**
 * nolphin_icon_container_is_stretched
 * @container: An icon container widget.
 *
 * Returns true if the any selected item is stretched to a size other than 1.0.
 **/
gboolean
nolphin_icon_container_is_stretched (NolphinIconContainer *container)
{
	GList *p;
	NolphinIcon *icon;

	for (p = container->details->icons; p != NULL; p = p->next) {
		icon = p->data;
		if (icon->is_selected && icon->scale != 1.0) {
			return TRUE;
		}
	}
	return FALSE;
}

/**
 * nolphin_icon_container_unstretch
 * @container: An icon container widget.
 *
 * Gets rid of any icon stretching.
 **/
void
nolphin_icon_container_unstretch (NolphinIconContainer *container)
{
	GList *p;
	NolphinIcon *icon;

	for (p = container->details->icons; p != NULL; p = p->next) {
		icon = p->data;
		if (icon->is_selected) {
			nolphin_icon_container_move_icon (container, icon,
							   icon->x, icon->y,
							   1.0,
							   FALSE, TRUE, TRUE);
		}
	}
}

static void
compute_stretch (StretchState *start,
		 StretchState *current)
{
	gboolean right, bottom;
	int x_stretch, y_stretch;

	/* FIXME bugzilla.gnome.org 45390: This doesn't correspond to
         * the way the handles are drawn.
	 */
	/* Figure out which handle we are dragging. */
	right = start->pointer_x > start->icon_x + (int) start->icon_size / 2;
	bottom = start->pointer_y > start->icon_y + (int) start->icon_size / 2;

	/* Figure out how big we should stretch. */
	x_stretch = start->pointer_x - current->pointer_x;
	y_stretch = start->pointer_y - current->pointer_y;
	if (right) {
		x_stretch = - x_stretch;
	}
	if (bottom) {
		y_stretch = - y_stretch;
	}
	current->icon_size = MAX ((int) start->icon_size + MIN (x_stretch, y_stretch),
				  (int) NOLPHIN_ICON_SIZE_SMALLEST);

	/* Figure out where the corner of the icon should be. */
	current->icon_x = start->icon_x;
	if (!right) {
		current->icon_x += start->icon_size - current->icon_size;
	}
	current->icon_y = start->icon_y;
	if (!bottom) {
		current->icon_y += start->icon_size - current->icon_size;
	}
}

char *
nolphin_icon_container_get_icon_uri (NolphinIconContainer *container,
				      NolphinIcon *icon)
{
	char *uri;

	uri = NULL;
	g_signal_emit (container,
			 signals[GET_ICON_URI], 0,
			 icon->data,
			 &uri);
	return uri;
}

char *
nolphin_icon_container_get_icon_drop_target_uri (NolphinIconContainer *container,
				   	     	  NolphinIcon *icon)
{
	char *uri;

	uri = NULL;
	g_signal_emit (container,
			 signals[GET_ICON_DROP_TARGET_URI], 0,
			 icon->data,
			 &uri);
	return uri;
}

void
nolphin_icon_container_update_tooltip_text (NolphinIconContainer  *container,
                                         NolphinIconCanvasItem *item)
{
    NolphinIcon *icon;
    NolphinFile *file;
    char *text;

    if (item == NULL) {
        gtk_widget_set_tooltip_text (GTK_WIDGET (container), "");
        return;
    }

    icon = item->user_data;
    file = NOLPHIN_FILE (icon->data);

    text = NULL;
    g_signal_emit (container,
                   signals[GET_TOOLTIP_TEXT], 0,
                   file,
                   &text);

    gtk_widget_set_tooltip_markup (GTK_WIDGET (container), text);

    g_free (text);
}

/* Call to reset the scroll region only if the container is not empty,
 * to avoid having the flag linger until the next file is added.
 */
static void
reset_scroll_region_if_not_empty (NolphinIconContainer *container)
{
	if (!nolphin_icon_container_is_empty (container)) {
		nolphin_icon_container_reset_scroll_region (container);
	}
}

/* Switch from automatic layout to manual or vice versa.
 * If we switch to manual layout, we restore the icon positions from the
 * last manual layout.
 */
void
nolphin_icon_container_set_auto_layout (NolphinIconContainer *container,
					 gboolean auto_layout)
{
	g_return_if_fail (NOLPHIN_IS_ICON_CONTAINER (container));
	g_return_if_fail (auto_layout == FALSE || auto_layout == TRUE);

	if (container->details->auto_layout == auto_layout) {
		return;
	}

	reset_scroll_region_if_not_empty (container);

    container->details->stored_auto_layout = auto_layout;
	container->details->auto_layout = auto_layout;

	if (!auto_layout) {
		reload_icon_positions (container);
		nolphin_icon_container_freeze_icon_positions (container);
	}

	container->details->needs_resort = TRUE;
	nolphin_icon_container_redo_layout (container);

	g_signal_emit (container, signals[LAYOUT_CHANGED], 0);
}

void
nolphin_icon_container_set_horizontal_layout (NolphinIconContainer *container,
                                           gboolean           horizontal)
{
    GtkTextDirection dir;
    NolphinIconLayoutMode layout_mode;

    g_return_if_fail (NOLPHIN_IS_ICON_CONTAINER (container));
    g_return_if_fail (horizontal == FALSE || horizontal == TRUE);

    if (container->details->horizontal == horizontal) {
        return;
    }

    container->details->horizontal = horizontal;

    dir = gtk_widget_get_direction (GTK_WIDGET (container));

    if (dir == GTK_TEXT_DIR_LTR) {
        layout_mode = horizontal ? NOLPHIN_ICON_LAYOUT_L_R_T_B : NOLPHIN_ICON_LAYOUT_T_B_L_R;
    } else {
        layout_mode = horizontal ? NOLPHIN_ICON_LAYOUT_R_L_T_B : NOLPHIN_ICON_LAYOUT_T_B_R_L;
    }

    container->details->layout_mode = layout_mode;
}

gboolean
nolphin_icon_container_get_horizontal_layout (NolphinIconContainer *container)
{
    g_return_val_if_fail (NOLPHIN_IS_ICON_CONTAINER (container), FALSE);

    return container->details->horizontal;
}

void
nolphin_icon_container_set_grid_adjusts (NolphinIconContainer *container,
                                      gint               h_adjust,
                                      gint               v_adjust)
{
    g_return_if_fail (NOLPHIN_IS_ICON_CONTAINER (container));

    container->details->h_adjust = h_adjust;
    container->details->v_adjust = v_adjust;
}

gboolean
nolphin_icon_container_is_keep_aligned (NolphinIconContainer *container)
{
	return container->details->keep_aligned;
}

static gboolean
align_icons_callback (gpointer callback_data)
{
	NolphinIconContainer *container;

	container = NOLPHIN_ICON_CONTAINER (callback_data);
	align_icons (container);
	container->details->align_idle_id = 0;

	return FALSE;
}

static void
unschedule_align_icons (NolphinIconContainer *container)
{
        if (container->details->align_idle_id != 0) {
		g_source_remove (container->details->align_idle_id);
		container->details->align_idle_id = 0;
	}
}

static void
schedule_align_icons (NolphinIconContainer *container)
{
	if (container->details->align_idle_id == 0
	    && container->details->has_been_allocated) {
		container->details->align_idle_id = g_idle_add
			(align_icons_callback, container);
	}
}

void
nolphin_icon_container_set_keep_aligned (NolphinIconContainer *container,
					  gboolean keep_aligned)
{
	if (container->details->keep_aligned != keep_aligned) {
		container->details->keep_aligned = keep_aligned;

		if (keep_aligned && !container->details->auto_layout) {
			schedule_align_icons (container);
		} else {
			unschedule_align_icons (container);
		}
	}
}

void
nolphin_icon_container_set_layout_mode (NolphinIconContainer *container,
					 NolphinIconLayoutMode mode)
{
	g_return_if_fail (NOLPHIN_IS_ICON_CONTAINER (container));

	container->details->layout_mode = mode;

	container->details->needs_resort = TRUE;

    if (gtk_widget_get_realized (GTK_WIDGET (container))) {
        nolphin_icon_container_invalidate_labels (container);
        nolphin_icon_container_redo_layout (container);
        g_signal_emit (container, signals[LAYOUT_CHANGED], 0);
    }
}

void
nolphin_icon_container_set_label_position (NolphinIconContainer *container,
					    NolphinIconLabelPosition position)
{
	g_return_if_fail (NOLPHIN_IS_ICON_CONTAINER (container));

	if (container->details->label_position != position) {
		container->details->label_position = position;

		nolphin_icon_container_invalidate_labels (container);
		nolphin_icon_container_request_update_all (container);

		schedule_redo_layout (container);
	}
}

/* Switch from automatic to manual layout, freezing all the icons in their
 * current positions instead of restoring icon positions from the last manual
 * layout as set_auto_layout does.
 */
void
nolphin_icon_container_freeze_icon_positions (NolphinIconContainer *container)
{
	gboolean changed;
	GList *p;
	NolphinIcon *icon;
	NolphinIconPosition position;

	changed = container->details->auto_layout;
	container->details->auto_layout = FALSE;

	for (p = container->details->icons; p != NULL; p = p->next) {
		icon = p->data;

		position.x = icon->saved_ltr_x;
		position.y = icon->y;
		position.scale = icon->scale;
        position.monitor = nolphin_desktop_utils_get_monitor_for_widget (GTK_WIDGET (container));
		g_signal_emit (container, signals[ICON_POSITION_CHANGED], 0,
				 icon->data, &position);
	}

	if (changed) {
		g_signal_emit (container, signals[LAYOUT_CHANGED], 0);
	}
}

/* Re-sort, switching to automatic layout if it was in manual layout. */
void
nolphin_icon_container_sort (NolphinIconContainer *container)
{
	gboolean changed;

    container->details->stored_auto_layout = container->details->auto_layout;

	changed = !container->details->auto_layout;
	container->details->auto_layout = TRUE;

	reset_scroll_region_if_not_empty (container);
	container->details->needs_resort = TRUE;

	nolphin_icon_container_redo_layout (container);

	if (changed) {
		g_signal_emit (container, signals[LAYOUT_CHANGED], 0);
	}
}

gboolean
nolphin_icon_container_is_auto_layout (NolphinIconContainer *container)
{
	g_return_val_if_fail (NOLPHIN_IS_ICON_CONTAINER (container), FALSE);

	return container->details->auto_layout;
}

static void
pending_icon_to_rename_destroy_callback (NolphinIconCanvasItem *item, NolphinIconContainer *container)
{
	g_assert (container->details->pending_icon_to_rename != NULL);
	g_assert (container->details->pending_icon_to_rename->item == item);
	container->details->pending_icon_to_rename = NULL;
}

static NolphinIcon*
get_pending_icon_to_rename (NolphinIconContainer *container)
{
	return container->details->pending_icon_to_rename;
}

static void
set_pending_icon_to_rename (NolphinIconContainer *container, NolphinIcon *icon)
{
	NolphinIcon *old_icon;

	old_icon = container->details->pending_icon_to_rename;

	if (icon == old_icon) {
		return;
	}

	if (old_icon != NULL) {
		g_signal_handlers_disconnect_by_func
			(old_icon->item,
			 G_CALLBACK (pending_icon_to_rename_destroy_callback),
			 container);
	}

	if (icon != NULL) {
		g_signal_connect (icon->item, "destroy",
				  G_CALLBACK (pending_icon_to_rename_destroy_callback), container);
	}

	container->details->pending_icon_to_rename = icon;
}

static void
process_pending_icon_to_rename (NolphinIconContainer *container)
{
	NolphinIcon *pending_icon_to_rename;

	pending_icon_to_rename = get_pending_icon_to_rename (container);

	if (pending_icon_to_rename != NULL) {
		if (pending_icon_to_rename->is_selected && !has_multiple_selection (container)) {
			nolphin_icon_container_start_renaming_selected_item (container, FALSE);
		} else {
			set_pending_icon_to_rename (container, NULL);
		}
	}
}

static gboolean
is_renaming_pending (NolphinIconContainer *container)
{
	return get_pending_icon_to_rename (container) != NULL;
}

static gboolean
is_renaming (NolphinIconContainer *container)
{
	return container->details->renaming;
}

/**
 * nolphin_icon_container_start_renaming_selected_item
 * @container: An icon container widget.
 * @select_all: Whether the whole file should initially be selected, or
 *              only its basename (i.e. everything except its extension).
 *
 * Displays the edit name widget on the first selected icon
 **/
void
nolphin_icon_container_start_renaming_selected_item (NolphinIconContainer *container,
						      gboolean select_all)
{
	NolphinIconContainerDetails *details;
	NolphinIcon *icon;
	EelDRect icon_rect;
	EelDRect text_rect;
	PangoContext *context;
	PangoFontDescription *desc;
	const char *editable_text;
	int x, y, width;
	int start_offset, end_offset;

	/* Check if it already in renaming mode, if so - select all */
	details = container->details;
	if (details->renaming) {
		eel_editable_label_select_region (EEL_EDITABLE_LABEL (details->rename_widget),
						  0,
						  -1);
		return;
	}

	/* Find selected icon */
	icon = get_first_selected_icon (container);
	if (icon == NULL) {
		return;
	}

	g_assert (!has_multiple_selection (container));


	if (!nolphin_icon_container_icon_is_positioned (icon)) {
		set_pending_icon_to_rename (container, icon);
		return;
	}

	set_pending_icon_to_rename (container, NULL);

	/* Make a copy of the original editable text for a later compare */
	editable_text = nolphin_icon_canvas_item_get_editable_text (icon->item);

	/* This could conceivably be NULL if a rename was triggered really early. */
	if (editable_text == NULL) {
		return;
	}

	details->original_text = g_strdup (editable_text);

	/* Freeze updates so files added while renaming don't cause rename to loose focus, bug #318373 */
	nolphin_icon_container_freeze_updates (container);

	/* Create text renaming widget, if it hasn't been created already.
	 * We deal with the broken icon text item widget by keeping it around
	 * so its contents can still be cut and pasted as part of the clipboard
	 */
	if (details->rename_widget == NULL) {
		details->rename_widget = eel_editable_label_new ("Test text");
		eel_editable_label_set_line_wrap (EEL_EDITABLE_LABEL (details->rename_widget), TRUE);
		eel_editable_label_set_line_wrap_mode (EEL_EDITABLE_LABEL (details->rename_widget), PANGO_WRAP_WORD_CHAR);
		eel_editable_label_set_draw_outline (EEL_EDITABLE_LABEL (details->rename_widget), TRUE);

		if (details->label_position != NOLPHIN_ICON_LABEL_POSITION_BESIDE) {
			eel_editable_label_set_justify (EEL_EDITABLE_LABEL (details->rename_widget), GTK_JUSTIFY_CENTER);
		}

		gtk_misc_set_padding (GTK_MISC (details->rename_widget), 1, 1);
		gtk_layout_put (GTK_LAYOUT (container),
				details->rename_widget, 0, 0);
	}

	/* Set the right font */
	if (details->font && g_strcmp0 (details->font, "") != 0) {
		desc = pango_font_description_from_string (details->font);
	} else {
		context = gtk_widget_get_pango_context (GTK_WIDGET (container));
		desc = pango_font_description_copy (pango_context_get_font_description (context));
	}

    if (pango_font_description_get_size (desc) > 0) {
        pango_font_description_set_size (desc,
                                         pango_font_description_get_size (desc) +
                                         container->details->font_size_table [container->details->zoom_level]);
    }

	eel_editable_label_set_font_description (EEL_EDITABLE_LABEL (details->rename_widget),
						 desc);
	pango_font_description_free (desc);

	icon_rect = nolphin_icon_canvas_item_get_icon_rectangle (icon->item);
    text_rect = nolphin_icon_canvas_item_get_text_rectangle (icon->item, TRUE);

	if (nolphin_icon_container_is_layout_vertical (container) &&
	    container->details->label_position == NOLPHIN_ICON_LABEL_POSITION_BESIDE) {
		/* for one-line editables, the width changes dynamically */
		width = -1;
	} else {
		width = nolphin_icon_canvas_item_get_max_text_width (icon->item);
	}

	if (details->label_position == NOLPHIN_ICON_LABEL_POSITION_BESIDE) {
		eel_canvas_w2c (EEL_CANVAS_ITEM (icon->item)->canvas,
				text_rect.x0,
				text_rect.y0,
				&x, &y);
	} else {
		eel_canvas_w2c (EEL_CANVAS_ITEM (icon->item)->canvas,
				(icon_rect.x0 + icon_rect.x1) / 2,
				icon_rect.y1,
				&x, &y);
		x = x - width / 2 - 1;
	}

	gtk_layout_move (GTK_LAYOUT (container),
			 details->rename_widget,
			 x, y);

	gtk_widget_set_size_request (details->rename_widget,
				     width, -1);
	eel_editable_label_set_text (EEL_EDITABLE_LABEL (details->rename_widget),
				     editable_text);
	if (select_all) {
		start_offset = 0;
		end_offset = -1;
	} else {
		/* if it is a directory it should select all of the text regardless of select_all option */
		if (nolphin_file_is_directory (NOLPHIN_FILE (icon->data))) {
			start_offset = 0;
			end_offset = -1;
		} else {
			eel_filename_get_rename_region (editable_text, &start_offset, &end_offset);
		}
	}

	gtk_widget_show (details->rename_widget);
	gtk_widget_grab_focus (details->rename_widget);

	eel_editable_label_select_region (EEL_EDITABLE_LABEL (details->rename_widget),
					  start_offset,
					  end_offset);

	g_signal_emit (container,
		       signals[ICON_RENAME_STARTED], 0,
		       GTK_EDITABLE (details->rename_widget));

	nolphin_icon_container_update_icon (container, icon);

    details->renaming_allocation_count = 0;

	/* We are in renaming mode */
	details->renaming = TRUE;
	nolphin_icon_canvas_item_set_renaming (icon->item, TRUE);
}

NolphinIcon *
nolphin_icon_container_get_icon_being_renamed (NolphinIconContainer *container)
{
    NolphinIcon *rename_icon;

    if (!is_renaming (container)) {
        return NULL;
    }

    g_assert (!has_multiple_selection (container));

    rename_icon = get_first_selected_icon (container);
    g_assert (rename_icon != NULL);

    return rename_icon;
}

void
nolphin_icon_container_end_renaming_mode (NolphinIconContainer *container, gboolean commit)
{
	NolphinIcon *icon;
	const char *changed_text = NULL;

	set_pending_icon_to_rename (container, NULL);

	icon = nolphin_icon_container_get_icon_being_renamed (container);
	if (icon == NULL) {
		return;
	}

	/* We are not in renaming mode */
	container->details->renaming = FALSE;
	nolphin_icon_canvas_item_set_renaming (icon->item, FALSE);

    container->details->renaming_allocation_count = 0;

	if (commit) {
		set_pending_icon_to_reveal (container, icon);
	}

	gtk_widget_grab_focus (GTK_WIDGET (container));

	if (commit) {
		/* Verify that text has been modified before signalling change. */
		changed_text = eel_editable_label_get_text (EEL_EDITABLE_LABEL (container->details->rename_widget));
		if (strcmp (container->details->original_text, changed_text) == 0) {
			changed_text = NULL;
		}
	}

	g_signal_emit (container,
		       signals[ICON_RENAME_ENDED], 0,
		       icon->data,
		       changed_text);

	gtk_widget_hide (container->details->rename_widget);
	g_free (container->details->original_text);

    nolphin_icon_container_unfreeze_updates (container);
}

void
nolphin_icon_container_set_single_click_mode (NolphinIconContainer *container,
					       gboolean single_click_mode)
{
	g_return_if_fail (NOLPHIN_IS_ICON_CONTAINER (container));

	container->details->single_click_mode = single_click_mode;
}

void
nolphin_icon_container_set_click_to_rename_enabled (NolphinIconContainer *container,
                                                           gboolean enabled)
{
    g_return_if_fail (NOLPHIN_IS_ICON_CONTAINER (container));

    container->details->click_to_rename = enabled;
}

/* Return if the icon container is a fixed size */
gboolean
nolphin_icon_container_get_is_fixed_size (NolphinIconContainer *container)
{
	g_return_val_if_fail (NOLPHIN_IS_ICON_CONTAINER (container), FALSE);

	return container->details->is_fixed_size;
}

/* Set the icon container to be a fixed size */
void
nolphin_icon_container_set_is_fixed_size (NolphinIconContainer *container,
					   gboolean is_fixed_size)
{
	g_return_if_fail (NOLPHIN_IS_ICON_CONTAINER (container));

	container->details->is_fixed_size = is_fixed_size;
}

gboolean
nolphin_icon_container_get_is_desktop (NolphinIconContainer *container)
{
	g_return_val_if_fail (NOLPHIN_IS_ICON_CONTAINER (container), FALSE);

	return container->details->is_desktop;
}

void
nolphin_icon_container_set_is_desktop (NolphinIconContainer *container,
					   gboolean is_desktop)
{
    g_return_if_fail (NOLPHIN_IS_ICON_CONTAINER (container));

    container->details->is_desktop = is_desktop;

    g_signal_handlers_disconnect_by_func (nolphin_icon_view_preferences,
                                          text_ellipsis_limit_changed_container_callback,
                                          container);
    g_signal_handlers_disconnect_by_func (nolphin_desktop_preferences,
                                          text_ellipsis_limit_changed_container_callback,
                                          container);

    if (is_desktop) {
        GtkStyleContext *context;

        context = gtk_widget_get_style_context (GTK_WIDGET (container));
        gtk_style_context_add_class (context, "nolphin-desktop");

        g_signal_connect_swapped (nolphin_desktop_preferences,
                                  "changed::" NOLPHIN_PREFERENCES_DESKTOP_TEXT_ELLIPSIS_LIMIT,
                                  G_CALLBACK (text_ellipsis_limit_changed_container_callback),
                                  container);
    } else {
        g_signal_connect_swapped (nolphin_icon_view_preferences,
                                  "changed::" NOLPHIN_PREFERENCES_ICON_VIEW_TEXT_ELLIPSIS_LIMIT,
                                  G_CALLBACK (text_ellipsis_limit_changed_container_callback),
                                  container);
    }
}

void
nolphin_icon_container_set_filter_highlight (NolphinIconContainer *container,
                                          const char *filter_text)
{
    g_return_if_fail (NOLPHIN_IS_ICON_CONTAINER (container));

    g_free (container->details->filter_highlight_text);
    container->details->filter_highlight_text = g_strdup (filter_text);

    nolphin_icon_container_invalidate_labels (container);
    nolphin_icon_container_request_update_all (container);
}

const char *
nolphin_icon_container_get_filter_highlight (NolphinIconContainer *container)
{
    g_return_val_if_fail (NOLPHIN_IS_ICON_CONTAINER (container), NULL);
    return container->details->filter_highlight_text;
}

void
nolphin_icon_container_set_margins (NolphinIconContainer *container,
				     int left_margin,
				     int right_margin,
				     int top_margin,
				     int bottom_margin)
{
	g_return_if_fail (NOLPHIN_IS_ICON_CONTAINER (container));

	container->details->left_margin = left_margin;
	container->details->right_margin = right_margin;
	container->details->top_margin = top_margin;
	container->details->bottom_margin = bottom_margin;

	/* redo layout of icons as the margins have changed */
	schedule_redo_layout (container);
}

void
nolphin_icon_container_set_use_drop_shadows (NolphinIconContainer  *container,
					      gboolean                use_drop_shadows)
{
	if (container->details->drop_shadows_requested == use_drop_shadows) {
		return;
	}

	container->details->drop_shadows_requested = use_drop_shadows;
	container->details->use_drop_shadows = use_drop_shadows;
	gtk_widget_queue_draw (GTK_WIDGET (container));
}

/* handle theme changes */

void
nolphin_icon_container_set_font (NolphinIconContainer *container,
				  const char *font)
{
	g_return_if_fail (NOLPHIN_IS_ICON_CONTAINER (container));

	if (g_strcmp0 (container->details->font, font) == 0) {
		return;
	}

	g_free (container->details->font);
	container->details->font = g_strdup (font);

	nolphin_icon_container_invalidate_labels (container);
	nolphin_icon_container_request_update_all (container);
	gtk_widget_queue_draw (GTK_WIDGET (container));
}

void
nolphin_icon_container_set_font_size_table (NolphinIconContainer *container,
					     const int font_size_table[NOLPHIN_ZOOM_LEVEL_LARGEST + 1])
{
	int old_font_size;
	int i;

	g_return_if_fail (NOLPHIN_IS_ICON_CONTAINER (container));
	g_return_if_fail (font_size_table != NULL);

	old_font_size = container->details->font_size_table[container->details->zoom_level];

	for (i = 0; i <= NOLPHIN_ZOOM_LEVEL_LARGEST; i++) {
		if (container->details->font_size_table[i] != font_size_table[i]) {
			container->details->font_size_table[i] = font_size_table[i];
		}
	}

	if (old_font_size != container->details->font_size_table[container->details->zoom_level]) {
		nolphin_icon_container_invalidate_labels (container);
		nolphin_icon_container_request_update_all (container);
	}
}

/**
 * nolphin_icon_container_get_icon_description
 * @container: An icon container widget.
 * @data: Icon data
 *
 * Gets the description for the icon. This function may return NULL.
 **/
char*
nolphin_icon_container_get_icon_description (NolphinIconContainer *container,
				              NolphinIconData      *data)
{
	NolphinIconContainerClass *klass;

	klass = NOLPHIN_ICON_CONTAINER_GET_CLASS (container);

	if (klass->get_icon_description) {
		return klass->get_icon_description (container, data);
	} else {
		return NULL;
	}
}

gboolean
nolphin_icon_container_get_allow_moves (NolphinIconContainer *container)
{
	g_return_val_if_fail (NOLPHIN_IS_ICON_CONTAINER (container), FALSE);

	return container->details->drag_allow_moves;
}

void
nolphin_icon_container_set_allow_moves	(NolphinIconContainer *container,
					 gboolean               allow_moves)
{
	g_return_if_fail (NOLPHIN_IS_ICON_CONTAINER (container));

	container->details->drag_allow_moves = allow_moves;
}

void
nolphin_icon_container_set_forced_icon_size (NolphinIconContainer *container,
					      int                    forced_icon_size)
{
	g_return_if_fail (NOLPHIN_IS_ICON_CONTAINER (container));

	if (forced_icon_size != container->details->forced_icon_size) {
		container->details->forced_icon_size = forced_icon_size;

		invalidate_label_sizes (container);
        update_icons (container);
		nolphin_icon_container_request_update_all (container);
	}
}

void
nolphin_icon_container_set_all_columns_same_width (NolphinIconContainer *container,
						    gboolean               all_columns_same_width)
{
	g_return_if_fail (NOLPHIN_IS_ICON_CONTAINER (container));

	if (all_columns_same_width != container->details->all_columns_same_width) {
		container->details->all_columns_same_width = all_columns_same_width;

		nolphin_icon_container_invalidate_labels (container);
		nolphin_icon_container_request_update_all (container);
	}
}

/**
 * nolphin_icon_container_set_highlighted_for_clipboard
 * @container: An icon container widget.
 * @data: Icon Data associated with all icons that should be highlighted.
 *        Others will be unhighlighted.
 **/
void
nolphin_icon_container_set_highlighted_for_clipboard (NolphinIconContainer *container,
						       GList                 *clipboard_icon_data)
{
	GList *l;
	NolphinIcon *icon;
	gboolean highlighted_for_clipboard;

	g_return_if_fail (NOLPHIN_IS_ICON_CONTAINER (container));

	for (l = container->details->icons; l != NULL; l = l->next) {
		icon = l->data;
		highlighted_for_clipboard = (g_list_find (clipboard_icon_data, icon->data) != NULL);

		eel_canvas_item_set (EEL_CANVAS_ITEM (icon->item),
				     "highlighted-for-clipboard", highlighted_for_clipboard,
				     NULL);
	}

}

/* NolphinIconContainerAccessible */

static NolphinIconContainerAccessiblePrivate *
accessible_get_priv (AtkObject *accessible)
{
	NolphinIconContainerAccessiblePrivate *priv;

	priv = g_object_get_qdata (G_OBJECT (accessible),
				   accessible_private_data_quark);

	return priv;
}

/* AtkAction interface */

static gboolean
nolphin_icon_container_accessible_do_action (AtkAction *accessible, int i)
{
	GtkWidget *widget;
	NolphinIconContainer *container;
	GList *selection;

	g_return_val_if_fail (i < LAST_ACTION, FALSE);

	widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible));
	if (!widget) {
		return FALSE;
	}

	container = NOLPHIN_ICON_CONTAINER (widget);
	switch (i) {
	case ACTION_ACTIVATE :
		selection = nolphin_icon_container_get_selection (container);

		if (selection) {
			g_signal_emit_by_name (container, "activate", selection);
            g_list_free (selection);
		}
		break;
	case ACTION_MENU :
		handle_popups (container, NULL,"context_click_background");
		break;
	default :
		g_warning ("Invalid action passed to NolphinIconContainerAccessible::do_action");
		return FALSE;
	}
	return TRUE;
}

static int
nolphin_icon_container_accessible_get_n_actions (AtkAction *accessible)
{
	return LAST_ACTION;
}

static const char *
nolphin_icon_container_accessible_action_get_description (AtkAction *accessible,
							   int i)
{
	NolphinIconContainerAccessiblePrivate *priv;

	g_assert (i < LAST_ACTION);

	priv = accessible_get_priv (ATK_OBJECT (accessible));

	if (priv->action_descriptions[i]) {
		return priv->action_descriptions[i];
	} else {
		return nolphin_icon_container_accessible_action_descriptions[i];
	}
}

static const char *
nolphin_icon_container_accessible_action_get_name (AtkAction *accessible, int i)
{
	g_assert (i < LAST_ACTION);

	return nolphin_icon_container_accessible_action_names[i];
}

static const char *
nolphin_icon_container_accessible_action_get_keybinding (AtkAction *accessible,
							  int i)
{
	g_assert (i < LAST_ACTION);

	return NULL;
}

static gboolean
nolphin_icon_container_accessible_action_set_description (AtkAction *accessible,
							   int i,
							   const char *description)
{
	NolphinIconContainerAccessiblePrivate *priv;

	g_assert (i < LAST_ACTION);

	priv = accessible_get_priv (ATK_OBJECT (accessible));

	if (priv->action_descriptions[i]) {
		g_free (priv->action_descriptions[i]);
	}
	priv->action_descriptions[i] = g_strdup (description);

	return FALSE;
}

static void
nolphin_icon_container_accessible_action_interface_init (AtkActionIface *iface)
{
	iface->do_action = nolphin_icon_container_accessible_do_action;
	iface->get_n_actions = nolphin_icon_container_accessible_get_n_actions;
	iface->get_description = nolphin_icon_container_accessible_action_get_description;
	iface->get_name = nolphin_icon_container_accessible_action_get_name;
	iface->get_keybinding = nolphin_icon_container_accessible_action_get_keybinding;
	iface->set_description = nolphin_icon_container_accessible_action_set_description;
}

/* AtkSelection interface */

static void
nolphin_icon_container_accessible_update_selection (AtkObject *accessible)
{
	NolphinIconContainer *container;
	NolphinIconContainerAccessiblePrivate *priv;
	GList *l;
	NolphinIcon *icon;

	container = NOLPHIN_ICON_CONTAINER (gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible)));

	priv = accessible_get_priv (accessible);

	if (priv->selection) {
		g_list_free (priv->selection);
		priv->selection = NULL;
	}

	for (l = container->details->icons; l != NULL; l = l->next) {
		icon = l->data;
		if (icon->is_selected) {
			priv->selection = g_list_prepend (priv->selection,
							  icon);
		}
	}

	priv->selection = g_list_reverse (priv->selection);
}

static void
nolphin_icon_container_accessible_selection_changed_cb (NolphinIconContainer *container,
							 gpointer data)
{
	g_signal_emit_by_name (data, "selection_changed");
}

static void
nolphin_icon_container_accessible_icon_added_cb (NolphinIconContainer *container,
						  NolphinIconData *icon_data,
						  gpointer data)
{
	NolphinIcon *icon;
	AtkObject *atk_parent;
	AtkObject *atk_child;
	int index;

	icon = g_hash_table_lookup (container->details->icon_set, icon_data);
	if (icon) {
		atk_parent = ATK_OBJECT (data);
		atk_child = atk_gobject_accessible_for_object
			(G_OBJECT (icon->item));
		index = g_list_index (container->details->icons, icon);

		g_signal_emit_by_name (atk_parent, "children_changed::add",
				       index, atk_child, NULL);
	}
}

static void
nolphin_icon_container_accessible_icon_removed_cb (NolphinIconContainer *container,
						    NolphinIconData *icon_data,
						    gpointer data)
{
	NolphinIcon *icon;
	AtkObject *atk_parent;
	AtkObject *atk_child;
	int index;

	icon = g_hash_table_lookup (container->details->icon_set, icon_data);
	if (icon) {
		atk_parent = ATK_OBJECT (data);
		atk_child = atk_gobject_accessible_for_object
			(G_OBJECT (icon->item));
		index = g_list_index (container->details->icons, icon);

		g_signal_emit_by_name (atk_parent, "children_changed::remove",
				       index, atk_child, NULL);
	}
}

static void
nolphin_icon_container_accessible_cleared_cb (NolphinIconContainer *container,
					       gpointer data)
{
	g_signal_emit_by_name (data, "children_changed", 0, NULL, NULL);
}


static gboolean
nolphin_icon_container_accessible_add_selection (AtkSelection *accessible,
						  int i)
{
	GtkWidget *widget;
	NolphinIconContainer *container;
	GList *selection;
	NolphinIcon *icon;

	widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible));
	if (!widget) {
		return FALSE;
	}

        container = NOLPHIN_ICON_CONTAINER (widget);

	icon = g_list_nth_data (container->details->icons, i);
	if (icon) {
		selection = nolphin_icon_container_get_selection (container);
		selection = g_list_prepend (selection,
					    icon->data);
		nolphin_icon_container_set_selection (container, selection);

		g_list_free (selection);
		return TRUE;
	}

	return FALSE;
}

static gboolean
nolphin_icon_container_accessible_clear_selection (AtkSelection *accessible)
{
	GtkWidget *widget;
	NolphinIconContainer *container;

	widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible));
	if (!widget) {
		return FALSE;
	}

        container = NOLPHIN_ICON_CONTAINER (widget);

	nolphin_icon_container_unselect_all (container);

	return TRUE;
}

static AtkObject *
nolphin_icon_container_accessible_ref_selection (AtkSelection *accessible,
						  int i)
{
	AtkObject *atk_object;
	NolphinIconContainerAccessiblePrivate *priv;
	NolphinIcon *icon;

	nolphin_icon_container_accessible_update_selection (ATK_OBJECT (accessible));
	priv = accessible_get_priv (ATK_OBJECT (accessible));

	icon = g_list_nth_data (priv->selection, i);
	if (icon) {
		atk_object = atk_gobject_accessible_for_object (G_OBJECT (icon->item));
		if (atk_object) {
			g_object_ref (atk_object);
		}

		return atk_object;
	} else {
		return NULL;
	}
}

static int
nolphin_icon_container_accessible_get_selection_count (AtkSelection *accessible)
{
	int count;
	NolphinIconContainerAccessiblePrivate *priv;

	nolphin_icon_container_accessible_update_selection (ATK_OBJECT (accessible));
	priv = accessible_get_priv (ATK_OBJECT (accessible));

	count = g_list_length (priv->selection);

	return count;
}

static gboolean
nolphin_icon_container_accessible_is_child_selected (AtkSelection *accessible,
						      int i)
{
	NolphinIconContainer *container;
	NolphinIcon *icon;
	GtkWidget *widget;

	widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible));
	if (!widget) {
		return FALSE;
	}

        container = NOLPHIN_ICON_CONTAINER (widget);

	icon = g_list_nth_data (container->details->icons, i);
	return icon ? icon->is_selected : FALSE;
}

static gboolean
nolphin_icon_container_accessible_remove_selection (AtkSelection *accessible,
						     int i)
{
	NolphinIconContainer *container;
	NolphinIconContainerAccessiblePrivate *priv;
	GList *selection;
	NolphinIcon *icon;
	GtkWidget *widget;

	widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible));
	if (!widget) {
		return FALSE;
	}

	nolphin_icon_container_accessible_update_selection (ATK_OBJECT (accessible));
	priv = accessible_get_priv (ATK_OBJECT (accessible));

        container = NOLPHIN_ICON_CONTAINER (widget);

	icon = g_list_nth_data (priv->selection, i);
	if (icon) {
		selection = nolphin_icon_container_get_selection (container);
		selection = g_list_remove (selection, icon->data);
		nolphin_icon_container_set_selection (container, selection);

		g_list_free (selection);
		return TRUE;
	}

	return FALSE;
}

static gboolean
nolphin_icon_container_accessible_select_all_selection (AtkSelection *accessible)
{
	NolphinIconContainer *container;
	GtkWidget *widget;

	widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible));
	if (!widget) {
		return FALSE;
	}

        container = NOLPHIN_ICON_CONTAINER (widget);

	nolphin_icon_container_select_all (container);

	return TRUE;
}

void
nolphin_icon_container_widget_to_file_operation_position (NolphinIconContainer *container,
							   GdkPoint              *position)
{
	double x, y;

	g_return_if_fail (position != NULL);

	x = position->x;
	y = position->y;

	eel_canvas_window_to_world (EEL_CANVAS (container), x, y, &x, &y);

	position->x = (int) x;
	position->y = (int) y;

	/* ensure that we end up in the middle of the icon */
	position->x -= nolphin_get_icon_size_for_zoom_level (container->details->zoom_level) / 2;
	position->y -= nolphin_get_icon_size_for_zoom_level (container->details->zoom_level) / 2;
}

static void
nolphin_icon_container_accessible_selection_interface_init (AtkSelectionIface *iface)
{
	iface->add_selection = nolphin_icon_container_accessible_add_selection;
	iface->clear_selection = nolphin_icon_container_accessible_clear_selection;
	iface->ref_selection = nolphin_icon_container_accessible_ref_selection;
	iface->get_selection_count = nolphin_icon_container_accessible_get_selection_count;
	iface->is_child_selected = nolphin_icon_container_accessible_is_child_selected;
	iface->remove_selection = nolphin_icon_container_accessible_remove_selection;
	iface->select_all_selection = nolphin_icon_container_accessible_select_all_selection;
}


static gint
nolphin_icon_container_accessible_get_n_children (AtkObject *accessible)
{
	NolphinIconContainer *container;
	GtkWidget *widget;
	gint i;

	widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible));
	if (!widget) {
		return FALSE;
	}

	container = NOLPHIN_ICON_CONTAINER (widget);

	i = g_hash_table_size (container->details->icon_set);
	if (container->details->rename_widget) {
		i++;
	}
	return i;
}

static AtkObject*
nolphin_icon_container_accessible_ref_child (AtkObject *accessible, int i)
{
        AtkObject *atk_object;
        NolphinIconContainer *container;
        NolphinIcon *icon;
	GtkWidget *widget;

	widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible));
	if (!widget) {
		return NULL;
	}

        container = NOLPHIN_ICON_CONTAINER (widget);

        icon = g_list_nth_data (container->details->icons, i);
        if (icon) {
                atk_object = atk_gobject_accessible_for_object (G_OBJECT (icon->item));
                g_object_ref (atk_object);

                return atk_object;
        } else {
		if (i == (int)g_list_length (container->details->icons)) {
			if (container->details->rename_widget) {
				atk_object = gtk_widget_get_accessible (container->details->rename_widget);
				g_object_ref (atk_object);

                		return atk_object;
			}
		}
                return NULL;
        }
}

static void
nolphin_icon_container_accessible_initialize (AtkObject *accessible,
					       gpointer data)
{
	NolphinIconContainer *container;
	NolphinIconContainerAccessiblePrivate *priv;

	if (ATK_OBJECT_CLASS (accessible_parent_class)->initialize) {
		ATK_OBJECT_CLASS (accessible_parent_class)->initialize (accessible, data);
	}

	priv = g_new0 (NolphinIconContainerAccessiblePrivate, 1);
	g_object_set_qdata (G_OBJECT (accessible),
			    accessible_private_data_quark,
			    priv);

	if (GTK_IS_ACCESSIBLE (accessible)) {
		nolphin_icon_container_accessible_update_selection
			(ATK_OBJECT (accessible));

		container = NOLPHIN_ICON_CONTAINER (gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible)));
		g_signal_connect (G_OBJECT (container), "selection_changed",
				  G_CALLBACK (nolphin_icon_container_accessible_selection_changed_cb),
				  accessible);
		g_signal_connect (G_OBJECT (container), "icon_added",
				  G_CALLBACK (nolphin_icon_container_accessible_icon_added_cb),
				  accessible);
		g_signal_connect (G_OBJECT (container), "icon_removed",
				  G_CALLBACK (nolphin_icon_container_accessible_icon_removed_cb),
				  accessible);
		g_signal_connect (G_OBJECT (container), "cleared",
				  G_CALLBACK (nolphin_icon_container_accessible_cleared_cb),
				  accessible);
	}
}

static void
nolphin_icon_container_accessible_finalize (GObject *object)
{
	NolphinIconContainerAccessiblePrivate *priv;
	int i;

	priv = accessible_get_priv (ATK_OBJECT (object));
	if (priv->selection) {
		g_list_free (priv->selection);
	}

	for (i = 0; i < LAST_ACTION; i++) {
		if (priv->action_descriptions[i]) {
			g_free (priv->action_descriptions[i]);
		}
	}

	g_free (priv);

	G_OBJECT_CLASS (accessible_parent_class)->finalize (object);
}

static void
nolphin_icon_container_accessible_class_init (AtkObjectClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	accessible_parent_class = g_type_class_peek_parent (klass);

	gobject_class->finalize = nolphin_icon_container_accessible_finalize;

	klass->get_n_children = nolphin_icon_container_accessible_get_n_children;
	klass->ref_child = nolphin_icon_container_accessible_ref_child;
	klass->initialize = nolphin_icon_container_accessible_initialize;

	accessible_private_data_quark = g_quark_from_static_string ("icon-container-accessible-private-data");
}

static GType
nolphin_icon_container_accessible_get_type (void)
{
        static GType type = 0;

        if (!type) {
                static GInterfaceInfo atk_action_info = {
                        (GInterfaceInitFunc) nolphin_icon_container_accessible_action_interface_init,
                        (GInterfaceFinalizeFunc) NULL,
                        NULL
                };

                static GInterfaceInfo atk_selection_info = {
                        (GInterfaceInitFunc) nolphin_icon_container_accessible_selection_interface_init,
                        (GInterfaceFinalizeFunc) NULL,
                        NULL
                };

		type = eel_accessibility_create_derived_type
			("NolphinIconContainerAccessible",
			 EEL_TYPE_CANVAS,
			 nolphin_icon_container_accessible_class_init);

                g_type_add_interface_static (type, ATK_TYPE_ACTION,
                                             &atk_action_info);
                g_type_add_interface_static (type, ATK_TYPE_SELECTION,
                                             &atk_selection_info);
        }

        return type;
}

gboolean
nolphin_icon_container_is_layout_rtl (NolphinIconContainer *container)
{
	g_return_val_if_fail (NOLPHIN_IS_ICON_CONTAINER (container), 0);

    return gtk_widget_get_direction (GTK_WIDGET(container)) == GTK_TEXT_DIR_RTL;
}

gboolean
nolphin_icon_container_is_layout_vertical (NolphinIconContainer *container)
{
	g_return_val_if_fail (NOLPHIN_IS_ICON_CONTAINER (container), FALSE);

	return (container->details->layout_mode == NOLPHIN_ICON_LAYOUT_T_B_L_R ||
		container->details->layout_mode == NOLPHIN_ICON_LAYOUT_T_B_R_L);
}

gint
nolphin_icon_container_get_max_layout_lines_for_pango (NolphinIconContainer  *container)
{
    return NOLPHIN_ICON_CONTAINER_GET_CLASS (container)->get_max_layout_lines_for_pango (container);
}

gint
nolphin_icon_container_get_max_layout_lines (NolphinIconContainer  *container)
{
    return NOLPHIN_ICON_CONTAINER_GET_CLASS (container)->get_max_layout_lines (container);
}

void
nolphin_icon_container_begin_loading (NolphinIconContainer *container)
{
	gboolean dummy;

    clear_drag_state (container);

	if (nolphin_icon_container_get_store_layout_timestamps (container)) {
		container->details->layout_timestamp = UNDEFINED_TIME;
		g_signal_emit (container,
			       signals[GET_STORED_LAYOUT_TIMESTAMP], 0,
			       NULL, &container->details->layout_timestamp, &dummy);
	}
}

void
nolphin_icon_container_store_layout_timestamps_now (NolphinIconContainer *container)
{
	NolphinIcon *icon;
	GList *p;
	gboolean dummy;

	container->details->layout_timestamp = time (NULL);
	g_signal_emit (container,
		       signals[STORE_LAYOUT_TIMESTAMP], 0,
		       NULL, &container->details->layout_timestamp, &dummy);

	for (p = container->details->icons; p != NULL; p = p->next) {
		icon = p->data;

		g_signal_emit (container,
			       signals[STORE_LAYOUT_TIMESTAMP], 0,
			       icon->data, &container->details->layout_timestamp, &dummy);
	}
}


void
nolphin_icon_container_end_loading (NolphinIconContainer *container,
				     gboolean               all_icons_added)
{
	if (all_icons_added &&
	    nolphin_icon_container_get_store_layout_timestamps (container)) {
		if (container->details->new_icons == NULL) {
			nolphin_icon_container_store_layout_timestamps_now (container);
		} else {
			container->details->store_layout_timestamps_when_finishing_new_icons = TRUE;
		}
	}
}

gboolean
nolphin_icon_container_get_store_layout_timestamps (NolphinIconContainer *container)
{
	return container->details->store_layout_timestamps;
}


void
nolphin_icon_container_set_store_layout_timestamps (NolphinIconContainer *container,
						     gboolean               store_layout_timestamps)
{
	container->details->store_layout_timestamps = store_layout_timestamps;
}

gint
nolphin_icon_container_get_canvas_height (NolphinIconContainer *container,
                                       GtkAllocation      allocation)
{
    return (allocation.height - container->details->top_margin - container->details->bottom_margin)
               / EEL_CANVAS (container)->pixels_per_unit;
}

gint
nolphin_icon_container_get_canvas_width (NolphinIconContainer *container,
                                      GtkAllocation      allocation)
{
    return (allocation.width - container->details->left_margin - container->details->right_margin)
               / EEL_CANVAS (container)->pixels_per_unit;
}

double
nolphin_icon_container_get_mirror_x_position (NolphinIconContainer *container, NolphinIcon *icon, double x)
{
    EelDRect icon_bounds;
    GtkAllocation allocation;

    gtk_widget_get_allocation (GTK_WIDGET (container), &allocation);
    icon_bounds = nolphin_icon_canvas_item_get_icon_rectangle (icon->item);

    return nolphin_icon_container_get_canvas_width (container, allocation) - x - (icon_bounds.x1 - icon_bounds.x0);
}

void
nolphin_icon_container_set_rtl_positions (NolphinIconContainer *container)
{
    GList *l;
    NolphinIcon *icon;
    double x;

    if (!container->details->icons) {
        return;
    }

    for (l = container->details->icons; l != NULL; l = l->next) {
        icon = l->data;
        x = nolphin_icon_container_get_mirror_x_position (container, icon, icon->saved_ltr_x);
        nolphin_icon_container_icon_set_position (container, icon, x, icon->y);
    }
}

void
nolphin_icon_container_sort_icons (NolphinIconContainer *container,
        GList                **icons)
{
    NolphinIconContainerClass *klass;

    klass = NOLPHIN_ICON_CONTAINER_GET_CLASS (container);
    g_assert (klass->compare_icons != NULL);

    *icons = g_list_sort_with_data (*icons, compare_icons, container);
}

void
nolphin_icon_container_resort (NolphinIconContainer *container)
{
    nolphin_icon_container_sort_icons (container, &container->details->icons);
}

void
nolphin_icon_container_icon_raise (NolphinIconContainer *container, NolphinIcon *icon)
{
    EelCanvasItem *item, *band;

    item = EEL_CANVAS_ITEM (icon->item);
    band = container->details->rubberband_info.selection_rectangle;

    eel_canvas_item_send_behind (item, band);
}

void
nolphin_icon_container_finish_adding_icon (NolphinIconContainer *container,
            NolphinIcon *icon)
{
    eel_canvas_item_show (EEL_CANVAS_ITEM (icon->item));

    g_signal_connect_object (icon->item, "event",
                 G_CALLBACK (item_event_callback), container, 0);

    g_signal_emit (container, signals[ICON_ADDED], 0, icon->data);
}

void
nolphin_icon_container_update_selection (NolphinIconContainer *container)
{
    g_return_if_fail (NOLPHIN_IS_ICON_CONTAINER (container));

    if (container->details->current_selection != NULL) {
        g_list_free (container->details->current_selection);

        container->details->current_selection = NULL;
        container->details->current_selection_count = 0;
    }

    container->details->current_selection = nolphin_icon_container_get_real_selection (container);
    container->details->current_selection_count = g_list_length (container->details->current_selection);
}

void
nolphin_icon_container_move_icon (NolphinIconContainer *container,
                   NolphinIcon *icon,
                   int x, int y,
                   double scale,
                   gboolean raise,
                   gboolean snap,
                   gboolean update_position)
{
    NOLPHIN_ICON_CONTAINER_GET_CLASS (container)->move_icon (container,
                                                          icon,
                                                          x, y,
                                                          scale,
                                                          raise,
                                                          snap,
                                                          update_position);
}

void
nolphin_icon_container_icon_set_position (NolphinIconContainer *container,
                                       NolphinIcon          *icon,
                                       gdouble            x,
                                       gdouble            y)
{
    NOLPHIN_ICON_CONTAINER_GET_CLASS (container)->icon_set_position (container, icon, x, y);
}

void
nolphin_icon_container_icon_get_bounding_box (NolphinIconContainer *container,
                                           NolphinIcon *icon,
                                           int *x1_return, int *y1_return,
                                           int *x2_return, int *y2_return,
                                           NolphinIconCanvasItemBoundsUsage usage)
{
    NOLPHIN_ICON_CONTAINER_GET_CLASS (container)->icon_get_bounding_box (icon, x1_return, y1_return, x2_return, y2_return, usage);
}

void
nolphin_icon_container_update_icon (NolphinIconContainer *container,
                                 NolphinIcon          *icon)
{
    gboolean ok = FALSE;

    if (icon != NULL) {
        NolphinFile *file = NOLPHIN_FILE (icon->data);

        ok = icon->ok_to_show_thumb ||
             (nolphin_file_get_load_deferred_attrs (file) == NOLPHIN_FILE_LOAD_DEFERRED_ATTRS_PRELOAD);
    }

    NOLPHIN_ICON_CONTAINER_GET_CLASS (container)->update_icon (container, icon, ok);
}

gint
nolphin_icon_container_get_additional_text_line_count (NolphinIconContainer *container)
{
    return NOLPHIN_ICON_CONTAINER_GET_CLASS (container)->get_additional_text_line_count (container);
}

void
nolphin_icon_container_set_ok_to_load_deferred_attrs (NolphinIconContainer *container,
                                                   gboolean           ok)
{
    container->details->ok_to_load_deferred_attrs = ok;

    if (ok) {
        queue_update_visible_icons (container, INITIAL_UPDATE_VISIBLE_DELAY);
    }
}

#if ! defined (NOLPHIN_OMIT_SELF_CHECK)

static char *
check_compute_stretch (int icon_x, int icon_y, int icon_size,
               int start_pointer_x, int start_pointer_y,
               int end_pointer_x, int end_pointer_y)
{
    StretchState start, current;

    start.icon_x = icon_x;
    start.icon_y = icon_y;
    start.icon_size = icon_size;
    start.pointer_x = start_pointer_x;
    start.pointer_y = start_pointer_y;
    current.pointer_x = end_pointer_x;
    current.pointer_y = end_pointer_y;

    compute_stretch (&start, &current);

    return g_strdup_printf ("%d,%d:%d",
                current.icon_x,
                current.icon_y,
                current.icon_size);
}

void
nolphin_self_check_icon_container (void)
{
    EEL_CHECK_STRING_RESULT (check_compute_stretch (0, 0, 16, 0, 0, 0, 0), "0,0:16");
    EEL_CHECK_STRING_RESULT (check_compute_stretch (0, 0, 16, 16, 16, 17, 17), "0,0:17");
    EEL_CHECK_STRING_RESULT (check_compute_stretch (0, 0, 16, 16, 16, 17, 16), "0,0:16");
    EEL_CHECK_STRING_RESULT (check_compute_stretch (100, 100, 64, 105, 105, 40, 40), "35,35:129");
}

#endif /* ! NOLPHIN_OMIT_SELF_CHECK */
