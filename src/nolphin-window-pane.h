/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nolphin-window-pane.h: Nolphin window pane

   Copyright (C) 2008 Free Software Foundation, Inc.

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

   Author: Holger Berndt <berndth@gmx.de>
*/

#ifndef NOLPHIN_WINDOW_PANE_H
#define NOLPHIN_WINDOW_PANE_H

#include <glib-object.h>

#include "nolphin-window.h"

#include <libnolphin-private/nolphin-icon-info.h>

#define NOLPHIN_TYPE_WINDOW_PANE	 (nolphin_window_pane_get_type())
#define NOLPHIN_WINDOW_PANE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), NOLPHIN_TYPE_WINDOW_PANE, NolphinWindowPaneClass))
#define NOLPHIN_WINDOW_PANE(obj)	 (G_TYPE_CHECK_INSTANCE_CAST ((obj), NOLPHIN_TYPE_WINDOW_PANE, NolphinWindowPane))
#define NOLPHIN_IS_WINDOW_PANE(obj)      (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NOLPHIN_TYPE_WINDOW_PANE))
#define NOLPHIN_IS_WINDOW_PANE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), NOLPHIN_TYPE_WINDOW_PANE))
#define NOLPHIN_WINDOW_PANE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), NOLPHIN_TYPE_WINDOW_PANE, NolphinWindowPaneClass))

struct _NolphinWindowPaneClass {
	GtkBoxClass parent_class;
};

/* A NolphinWindowPane is a layer between a slot and a window.
 * Each slot is contained in one pane, and each pane can contain
 * one or more slots. It also supports the notion of an "active slot".
 * On the other hand, each pane is contained in a window, while each
 * window can contain one or multiple panes. Likewise, the window has
 * the notion of an "active pane".
 *
 * A navigation window may have one or more panes.
 */
struct _NolphinWindowPane {
	GtkBox parent;

	/* hosting window */
	NolphinWindow *window;

	/* available slots, and active slot.
	 * Both of them may never be NULL. */
	GList *slots;
	NolphinWindowSlot *active_slot;

	/* location bar */
	GtkWidget *location_bar;
	GtkWidget *path_bar;
	GtkWidget *search_bar;
	GtkWidget *tool_bar;

	gboolean temporary_navigation_bar;
	gboolean temporary_search_bar;

	gboolean show_location_entry;

	/* notebook */
	GtkWidget *notebook;

	GtkActionGroup *action_group;
	GtkActionGroup *toolbar_action_group;

	GtkWidget *last_focus_widget;
};

GType nolphin_window_pane_get_type (void);

NolphinWindowPane *nolphin_window_pane_new (NolphinWindow *window);

NolphinWindowSlot *nolphin_window_pane_open_slot (NolphinWindowPane *pane,
					    NolphinWindowOpenSlotFlags flags);
/* This removes the slot from the given pane but does not close the pane and/or
 * window as well if there are no more slots left afterwards. This
 * functionality is provided by `nolphin_window_pane_close_slot' below.
 */
void nolphin_window_pane_remove_slot_unsafe (NolphinWindowPane *pane,
					  NolphinWindowSlot *slot);

void nolphin_window_pane_sync_location_widgets (NolphinWindowPane *pane);
void nolphin_window_pane_sync_search_widgets (NolphinWindowPane *pane);
void nolphin_window_pane_set_active (NolphinWindowPane *pane, gboolean is_active);
void nolphin_window_pane_close_slot (NolphinWindowPane *pane, NolphinWindowSlot *slot);
GtkActionGroup * nolphin_window_pane_get_toolbar_action_group (NolphinWindowPane   *pane);
void nolphin_window_pane_grab_focus (NolphinWindowPane *pane);
void nolphin_window_pane_sync_up_actions (NolphinWindowPane *pane);
/* bars */
void     nolphin_window_pane_ensure_location_bar (NolphinWindowPane *pane);

#endif /* NOLPHIN_WINDOW_PANE_H */
