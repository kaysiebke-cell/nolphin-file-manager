/* -*- Mode: C; indent-tabs-mode: f; c-basic-offset: 4; tab-width: 4 -*- */

/* nolphin-dnd.h - Common Drag & drop handling code shared by the icon container
   and the list view.

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

   Authors: Pavel Cisler <pavel@eazel.com>,
	    Ettore Perazzoli <ettore@gnu.org>
*/

#ifndef NOLPHIN_DND_H
#define NOLPHIN_DND_H

#include <gtk/gtk.h>

/* Drag & Drop target names. */
#define NOLPHIN_ICON_DND_GNOME_ICON_LIST_TYPE	"x-special/gnome-icon-list"
#define NOLPHIN_ICON_DND_URI_LIST_TYPE		"text/uri-list"
#define NOLPHIN_ICON_DND_NETSCAPE_URL_TYPE	"_NETSCAPE_URL"
#define NOLPHIN_ICON_DND_BGIMAGE_TYPE		"property/bgimage"
#define NOLPHIN_ICON_DND_ROOTWINDOW_DROP_TYPE	"application/x-rootwindow-drop"
#define NOLPHIN_ICON_DND_XDNDDIRECTSAVE_TYPE	"XdndDirectSave0" /* XDS Protocol Type */
#define NOLPHIN_ICON_DND_RAW_TYPE	"application/octet-stream"

/* Item of the drag selection list */
typedef struct {
	char *uri;
	gboolean got_icon_position;
	int icon_x, icon_y;
	int icon_width, icon_height;
} NolphinDragSelectionItem;

/* Standard Drag & Drop types. */
typedef enum {
	NOLPHIN_ICON_DND_GNOME_ICON_LIST,
	NOLPHIN_ICON_DND_URI_LIST,
	NOLPHIN_ICON_DND_NETSCAPE_URL,
	NOLPHIN_ICON_DND_TEXT,
	NOLPHIN_ICON_DND_XDNDDIRECTSAVE,
	NOLPHIN_ICON_DND_RAW,
	NOLPHIN_ICON_DND_ROOTWINDOW_DROP
} NolphinIconDndTargetType;

typedef enum {
	NOLPHIN_DND_ACTION_FIRST = GDK_ACTION_ASK << 1,
	NOLPHIN_DND_ACTION_SET_AS_BACKGROUND = NOLPHIN_DND_ACTION_FIRST << 0,
	NOLPHIN_DND_ACTION_SET_AS_FOLDER_BACKGROUND = NOLPHIN_DND_ACTION_FIRST << 1,
	NOLPHIN_DND_ACTION_SET_AS_GLOBAL_BACKGROUND = NOLPHIN_DND_ACTION_FIRST << 2
} NolphinDndAction;

/* drag&drop-related information. */
typedef struct {
	GtkTargetList *target_list;

    gchar *source_fs;
    gboolean can_delete_source;

	/* Stuff saved at "receive data" time needed later in the drag. */
	gboolean got_drop_data_type;
	NolphinIconDndTargetType data_type;
	GtkSelectionData *selection_data;
	char *direct_save_uri;

	/* Start of the drag, in window coordinates. */
	int start_x, start_y;

	/* List of NolphinDragSelectionItems, representing items being dragged, or NULL
	 * if data about them has not been received from the source yet.
	 */
	GList *selection_list;

	/* has the drop occured ? */
	gboolean drop_occured;

	/* whether or not need to clean up the previous dnd data */
	gboolean need_to_destroy;

	/* autoscrolling during dragging */
	int auto_scroll_timeout_id;
	gboolean waiting_to_autoscroll;
	gint64 start_auto_scroll_in;

} NolphinDragInfo;

typedef void		(* NolphinDragEachSelectedItemDataGet)	(const char *url,
                                 const char *path_str,
								 int x, int y, int w, int h, 
								 gpointer data);
typedef void		(* NolphinDragEachSelectedItemIterator)	(NolphinDragEachSelectedItemDataGet iteratee, 
								 gpointer iterator_context, 
								 gpointer data);

void			    nolphin_drag_init				(NolphinDragInfo		      *drag_info,
									 const GtkTargetEntry		      *drag_types,
									 int				       drag_type_count,
									 gboolean			       add_text_targets);
void			    nolphin_drag_finalize			(NolphinDragInfo		      *drag_info);
NolphinDragSelectionItem  *nolphin_drag_selection_item_new		(void);
void			    nolphin_drag_destroy_selection_list	(GList				      *selection_list);
GList			   *nolphin_drag_build_selection_list		(GtkSelectionData		      *data);

char **			    nolphin_drag_uri_array_from_selection_list (const GList			      *selection_list);
GList *			    nolphin_drag_uri_list_from_selection_list	(const GList			      *selection_list);

char **			    nolphin_drag_uri_array_from_list		(const GList			      *uri_list);
GList *			    nolphin_drag_uri_list_from_array		(const char			     **uris);

gboolean		    nolphin_drag_items_local			(const char			      *target_uri,
									 const GList			      *selection_list);
gboolean		    nolphin_drag_uris_local			(const char			      *target_uri,
									 const GList			      *source_uri_list);
gboolean		    nolphin_drag_items_in_trash		(const GList			      *selection_list);
gboolean		    nolphin_drag_items_on_desktop		(const GList			      *selection_list);
void                nolphin_drag_default_drop_action_for_icons (GdkDragContext *context,
                                                             const char     *target_uri_string,
                                                             const GList    *items,
                                                             int            *action,
                                                             gchar         **source_fs,
                                                             gboolean       *can_delete_source);
GdkDragAction		    nolphin_drag_default_drop_action_for_netscape_url (GdkDragContext			     *context);
GdkDragAction		    nolphin_drag_default_drop_action_for_uri_list     (GdkDragContext			     *context,
										const char			     *target_uri_string);
gboolean		    nolphin_drag_drag_data_get			(GtkWidget			      *widget,
									 GdkDragContext			      *context,
									 GtkSelectionData		      *selection_data,
									 guint				       info,
									 guint32			       time,
									 gpointer			       container_context,
									 NolphinDragEachSelectedItemIterator  each_selected_item_iterator);
int			    nolphin_drag_modifier_based_action		(int				       default_action,
									 int				       non_default_action);

GdkDragAction		    nolphin_drag_drop_action_ask		(GtkWidget			      *widget,
									 GdkDragAction			       possible_actions);

gboolean		    nolphin_drag_autoscroll_in_scroll_region	(GtkWidget			      *widget);
void			    nolphin_drag_autoscroll_calculate_delta	(GtkWidget			      *widget,
									 float				      *x_scroll_delta,
									 float				      *y_scroll_delta);
void			    nolphin_drag_autoscroll_start		(NolphinDragInfo		      *drag_info,
									 GtkWidget			      *widget,
									 GSourceFunc			       callback,
									 gpointer			       user_data);
void			    nolphin_drag_autoscroll_stop		(NolphinDragInfo		      *drag_info);

gboolean		    nolphin_drag_selection_includes_special_link (GList			      *selection_list);

#endif
