/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nolphin-window-slot.h: Nolphin window slot
 
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
  
   Author: Christian Neumair <cneumair@gnome.org>
*/

#ifndef NOLPHIN_WINDOW_SLOT_H
#define NOLPHIN_WINDOW_SLOT_H

#include "nolphin-view.h"
#include "nolphin-window-types.h"
#include "nolphin-query-editor.h"

#define NOLPHIN_TYPE_WINDOW_SLOT	 (nolphin_window_slot_get_type())
#define NOLPHIN_WINDOW_SLOT_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), NOLPHIN_TYPE_WINDOW_SLOT, NolphinWindowSlotClass))
#define NOLPHIN_WINDOW_SLOT(obj)	 (G_TYPE_CHECK_INSTANCE_CAST ((obj), NOLPHIN_TYPE_WINDOW_SLOT, NolphinWindowSlot))
#define NOLPHIN_IS_WINDOW_SLOT(obj)      (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NOLPHIN_TYPE_WINDOW_SLOT))
#define NOLPHIN_IS_WINDOW_SLOT_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), NOLPHIN_TYPE_WINDOW_SLOT))
#define NOLPHIN_WINDOW_SLOT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), NOLPHIN_TYPE_WINDOW_SLOT, NolphinWindowSlotClass))

typedef enum {
	NOLPHIN_LOCATION_CHANGE_STANDARD,
	NOLPHIN_LOCATION_CHANGE_BACK,
	NOLPHIN_LOCATION_CHANGE_FORWARD,
	NOLPHIN_LOCATION_CHANGE_RELOAD
} NolphinLocationChangeType;

struct NolphinWindowSlotClass {
	GtkBoxClass parent_class;

	/* wrapped NolphinWindowInfo signals, for overloading */
	void (* active)		(NolphinWindowSlot *slot);
	void (* inactive)	(NolphinWindowSlot *slot);
	void (* changed_pane)	(NolphinWindowSlot *slot);
};

/* Each NolphinWindowSlot corresponds to a location in the window
 * for displaying a NolphinView, i.e. a tab.
 */
struct NolphinWindowSlot {
	GtkBox parent;

	NolphinWindowPane *pane;

	/* slot contains
 	 *  1) an event box containing extra_location_widgets
 	 *  2) the view box for the content view
 	 */
	GtkWidget *extra_location_widgets;

	GtkWidget *view_overlay;
	GtkWidget *floating_bar;
    GtkWidget *cache_bar;
    GtkWidget *no_search_results_box;
    GtkWidget *no_results_label;

    GtkWidget *filter_bar;
    GtkWidget *filter_bar_revealer;
    gulong filter_activate_handler_id;

	guint set_status_timeout_id;
	guint loading_timeout_id;

	NolphinView *content_view;
	NolphinView *new_content_view;

	/* Information about bookmarks */
	NolphinBookmark *current_location_bookmark;
	NolphinBookmark *last_location_bookmark;

	/* Current location. */
	GFile *location;
	char *title;
	/* Vom Benutzer vergebener Reiter-Name (NULL = automatischer Titel) und
	 * Sperre gegen versehentliches Schließen. */
	char *custom_title;
	gboolean locked;
	char *status_text;

	NolphinFile *viewed_file;
	gboolean viewed_file_seen;
	gboolean viewed_file_in_trash;

	gboolean allow_stop;

	NolphinQueryEditor *query_editor;
	GtkWidget *query_editor_revealer;
	gulong qe_changed_id;
	gulong qe_cancel_id;

	/* New location. */
	NolphinLocationChangeType location_change_type;
	guint location_change_distance;
	GFile *pending_location;
	char *pending_scroll_to;
	GList *pending_selection;
	NolphinFile *determine_view_file;
	GCancellable *mount_cancellable;
	GError *mount_error;
	gboolean tried_mount;
	NolphinWindowGoToCallback open_callback;
	gpointer open_callback_user_data;

	gboolean needs_reload;

	GCancellable *find_mount_cancellable;

	gboolean visible;

	/* Back/Forward chain, and history list. 
	 * The data in these lists are NolphinBookmark pointers. 
	 */
	GList *back_list, *forward_list;
};

GType   nolphin_window_slot_get_type (void);

NolphinWindowSlot * nolphin_window_slot_new (NolphinWindowPane *pane);

void    nolphin_window_slot_update_title		   (NolphinWindowSlot *slot);
void    nolphin_window_slot_update_icon		   (NolphinWindowSlot *slot);
void    nolphin_window_slot_move_query_editor_to_panel (NolphinWindowSlot *slot);
gboolean nolphin_window_slot_open_saved_search (NolphinWindowSlot *slot, const char *path);
void    nolphin_window_slot_sync_query_editor_host (NolphinWindowSlot *slot);
void    nolphin_window_slot_set_query_editor_visible	   (NolphinWindowSlot *slot,
							    gboolean            visible);

GFile * nolphin_window_slot_get_location		   (NolphinWindowSlot *slot);
char *  nolphin_window_slot_get_location_uri		   (NolphinWindowSlot *slot);

void nolphin_window_slot_queue_reload (NolphinWindowSlot *slot,
                                    gboolean        clear_thumbs);
void nolphin_window_slot_force_reload (NolphinWindowSlot *slot);

/* convenience wrapper without selection and callback/user_data */
#define nolphin_window_slot_open_location(slot, location, flags)\
	nolphin_window_slot_open_location_full(slot, location, flags, NULL, NULL, NULL)

void nolphin_window_slot_open_location_full (NolphinWindowSlot *slot,
					      GFile *location,
					      NolphinWindowOpenFlags flags,
					      GList *new_selection, /* NolphinFile list */
					      NolphinWindowGoToCallback callback,
					      gpointer user_data);

void			nolphin_window_slot_stop_loading	      (NolphinWindowSlot	*slot);

void			nolphin_window_slot_set_content_view	      (NolphinWindowSlot	*slot,
								       const char		*id);
const char	       *nolphin_window_slot_get_content_view_id      (NolphinWindowSlot	*slot);
gboolean		nolphin_window_slot_content_view_matches_iid (NolphinWindowSlot	*slot,
								       const char		*iid);

void    nolphin_window_slot_go_home			   (NolphinWindowSlot *slot,
							    NolphinWindowOpenFlags flags);
void    nolphin_window_slot_go_up                         (NolphinWindowSlot *slot,
							    NolphinWindowOpenFlags flags);
void    nolphin_window_slot_set_content_view_widget	   (NolphinWindowSlot *slot,
							    NolphinView       *content_view);
void    nolphin_window_slot_set_viewed_file		   (NolphinWindowSlot *slot,
							    NolphinFile      *file);
void    nolphin_window_slot_set_allow_stop		   (NolphinWindowSlot *slot,
							    gboolean	    allow_stop);
void    nolphin_window_slot_set_status			   (NolphinWindowSlot *slot,
							    const char	 *status,
							    const char   *short_status,
                                gboolean      location_loading);

void    nolphin_window_slot_add_extra_location_widget     (NolphinWindowSlot *slot,
							    GtkWidget       *widget);
void    nolphin_window_slot_remove_extra_location_widgets (NolphinWindowSlot *slot);

NolphinView * nolphin_window_slot_get_current_view     (NolphinWindowSlot *slot);
char           * nolphin_window_slot_get_current_uri      (NolphinWindowSlot *slot);
NolphinWindow * nolphin_window_slot_get_window           (NolphinWindowSlot *slot);
void           nolphin_window_slot_make_hosting_pane_active (NolphinWindowSlot *slot);

gboolean nolphin_window_slot_should_close_with_mount (NolphinWindowSlot *slot,
						       GMount *mount);

void nolphin_window_slot_clear_forward_list (NolphinWindowSlot *slot);
void nolphin_window_slot_clear_back_list    (NolphinWindowSlot *slot);

void nolphin_window_slot_check_bad_cache_bar (NolphinWindowSlot *slot);

void nolphin_window_slot_set_show_thumbnails (NolphinWindowSlot *slot,
                                           gboolean show_thumbnails);

void nolphin_window_slot_hide_filter_bar (NolphinWindowSlot *slot);

#endif /* NOLPHIN_WINDOW_SLOT_H */
