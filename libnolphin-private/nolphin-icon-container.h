/* -*- Mode: C; indent-tabs-mode: f; c-basic-offset: 4; tab-width: 4 -*- */

/* gnome-icon-container.h - Icon container widget.

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

   Authors: Ettore Perazzoli <ettore@gnu.org>, Darin Adler <darin@bentspoon.com>
*/

#ifndef NOLPHIN_ICON_CONTAINER_H
#define NOLPHIN_ICON_CONTAINER_H

#include <eel/eel-canvas.h>
#include <libnolphin-private/nolphin-icon-info.h>
#include <libnolphin-private/nolphin-icon.h>

#define NOLPHIN_TYPE_ICON_CONTAINER nolphin_icon_container_get_type()
#define NOLPHIN_ICON_CONTAINER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NOLPHIN_TYPE_ICON_CONTAINER, NolphinIconContainer))
#define NOLPHIN_ICON_CONTAINER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NOLPHIN_TYPE_ICON_CONTAINER, NolphinIconContainerClass))
#define NOLPHIN_IS_ICON_CONTAINER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NOLPHIN_TYPE_ICON_CONTAINER))
#define NOLPHIN_IS_ICON_CONTAINER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NOLPHIN_TYPE_ICON_CONTAINER))
#define NOLPHIN_ICON_CONTAINER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NOLPHIN_TYPE_ICON_CONTAINER, NolphinIconContainerClass))

/* Initial unpositioned icon value */
#define ICON_UNPOSITIONED_VALUE -1

typedef struct {
	int x;
	int y;
	double scale;
    int monitor;
} NolphinIconPosition;

typedef enum {
	NOLPHIN_ICON_LAYOUT_L_R_T_B,
	NOLPHIN_ICON_LAYOUT_R_L_T_B,
	NOLPHIN_ICON_LAYOUT_T_B_L_R,
	NOLPHIN_ICON_LAYOUT_T_B_R_L
} NolphinIconLayoutMode;

typedef enum {
	NOLPHIN_ICON_LABEL_POSITION_UNDER,
	NOLPHIN_ICON_LABEL_POSITION_BESIDE
} NolphinIconLabelPosition;

#define	NOLPHIN_ICON_CONTAINER_TYPESELECT_FLUSH_DELAY 1000000

typedef struct NolphinIconContainerDetails NolphinIconContainerDetails;

typedef struct {
	EelCanvas canvas;
	NolphinIconContainerDetails *details;
} NolphinIconContainer;

typedef struct {
	EelCanvasClass parent_slot;
    gboolean is_grid_container;

	/* Operations on the container. */
	int          (* button_press) 	          (NolphinIconContainer *container,
						   GdkEventButton *event);
	void         (* context_click_background) (NolphinIconContainer *container,
						   GdkEventButton *event);
	void         (* middle_click) 		  (NolphinIconContainer *container,
						   GdkEventButton *event);

	/* Operations on icons. */
	void         (* activate)	  	  (NolphinIconContainer *container,
						   NolphinIconData *data);
	void         (* activate_alternate)       (NolphinIconContainer *container,
						   NolphinIconData *data);
	void         (* activate_previewer)       (NolphinIconContainer *container,
						   GList *files,
						   GArray *locations);
	void         (* context_click_selection)  (NolphinIconContainer *container,
						   GdkEventButton *event);
	void	     (* move_copy_items)	  (NolphinIconContainer *container,
						   const GList *item_uris,
						   GdkPoint *relative_item_points,
						   const char *target_uri,
						   GdkDragAction action,
						   int x,
						   int y);
	void	     (* handle_netscape_url)	  (NolphinIconContainer *container,
						   const char *url,
						   const char *target_uri,
						   GdkDragAction action,
						   int x,
						   int y);
	void	     (* handle_uri_list)    	  (NolphinIconContainer *container,
						   const char *uri_list,
						   const char *target_uri,
						   GdkDragAction action,
						   int x,
						   int y);
	void	     (* handle_text)		  (NolphinIconContainer *container,
						   const char *text,
						   const char *target_uri,
						   GdkDragAction action,
						   int x,
						   int y);
	void	     (* handle_raw)		  (NolphinIconContainer *container,
						   char *raw_data,
						   int length,
						   const char *target_uri,
						   const char *direct_save_uri,
						   GdkDragAction action,
						   int x,
						   int y);

	/* Queries on the container for subclass/client.
	 * These must be implemented. The default "do nothing" is not good enough.
	 */
	char *	     (* get_container_uri)	  (NolphinIconContainer *container);

	/* Queries on icons for subclass/client.
	 * These must be implemented. The default "do nothing" is not
	 * good enough, these are _not_ signals.
	 */
	NolphinIconInfo *(* get_icon_images)     (NolphinIconContainer *container,
						   NolphinIconData *data,
						   int icon_size,
						   gboolean for_drag_accept,
						   gboolean *has_window_open,
                           gboolean visible);
	void         (* get_icon_text)            (NolphinIconContainer *container,
						   NolphinIconData *data,
						   char **editable_text,
						   char **additional_text,
                           gboolean *pinned,
                           gboolean *fav_unavailable,
						   gboolean include_invisible);
    void         (* update_icon)              (NolphinIconContainer *container,
                                               NolphinIcon          *icon,
                                               gboolean           visible);
	char *       (* get_icon_description)     (NolphinIconContainer *container,
						   NolphinIconData *data);
	int          (* compare_icons)            (NolphinIconContainer *container,
						   NolphinIconData *icon_a,
						   NolphinIconData *icon_b);
	void         (* freeze_updates)           (NolphinIconContainer *container);
	void         (* unfreeze_updates)         (NolphinIconContainer *container);

    gint         (* get_max_layout_lines_for_pango) (NolphinIconContainer *container);
    gint         (* get_max_layout_lines)           (NolphinIconContainer *container);
    gint         (* get_additional_text_line_count) (NolphinIconContainer *container);

	/* Queries on icons for subclass/client.
	 * These must be implemented => These are signals !
	 * The default "do nothing" is not good enough.
	 */
	gboolean     (* can_accept_item)	  (NolphinIconContainer *container,
						   NolphinIconData *target, 
						   const char *item_uri);
	char *       (* get_icon_uri)             (NolphinIconContainer *container,
						   NolphinIconData *data);
	char *       (* get_icon_drop_target_uri) (NolphinIconContainer *container,
						   NolphinIconData *data);

	/* If icon data is NULL, the layout timestamp of the container should be retrieved.
	 * That is the time when the container displayed a fully loaded directory with
	 * all icon positions assigned.
	 *
	 * If icon data is not NULL, the position timestamp of the icon should be retrieved.
	 * That is the time when the file (i.e. icon data payload) was last displayed in a
	 * fully loaded directory with all icon positions assigned.
	 */
	gboolean     (* get_stored_layout_timestamp) (NolphinIconContainer *container,
						      NolphinIconData *data,
						      time_t *time);
	/* If icon data is NULL, the layout timestamp of the container should be stored.
	 * If icon data is not NULL, the position timestamp of the container should be stored.
	 */
	gboolean     (* store_layout_timestamp) (NolphinIconContainer *container,
						 NolphinIconData *data,
						 const time_t *time);

    void         (*lay_down_icons) (NolphinIconContainer *container, GList *icons, double start_y);
    void         (*icon_set_position) (NolphinIconContainer *container, NolphinIcon *icon, double x, double y);
    void         (*move_icon) (NolphinIconContainer *container, NolphinIcon *icon, int x, int y,
                               double scale, gboolean raise, gboolean snap, gboolean update_position);
    void         (*align_icons) (NolphinIconContainer *container);
    void         (*finish_adding_new_icons) (NolphinIconContainer *container);
    void         (*reload_icon_positions) (NolphinIconContainer *container);
    void         (*icon_get_bounding_box) (NolphinIcon *icon,
                                           int *x1_return, int *y1_return,
                                           int *x2_return, int *y2_return,
                                           NolphinIconCanvasItemBoundsUsage usage);
    void         (*set_zoom_level)        (NolphinIconContainer *container, gint new_level);
	/* Notifications for the whole container. */
	void	     (* band_select_started)	  (NolphinIconContainer *container);
	void	     (* band_select_ended)	  (NolphinIconContainer *container);
	void         (* selection_changed) 	  (NolphinIconContainer *container);
	void         (* layout_changed)           (NolphinIconContainer *container);

	/* Notifications for icons. */
	void         (* icon_position_changed)    (NolphinIconContainer *container,
						   NolphinIconData *data,
						   const NolphinIconPosition *position);
	void         (* icon_rename_started)      (NolphinIconContainer *container,
						   GtkWidget *renaming_widget);
	void         (* icon_rename_ended)        (NolphinIconContainer *container,
						   NolphinIconData *data,
						   const char *text);
	void	     (* icon_stretch_started)     (NolphinIconContainer *container,
						   NolphinIconData *data);
	void	     (* icon_stretch_ended)       (NolphinIconContainer *container,
						   NolphinIconData *data);
	int	     (* preview)		  (NolphinIconContainer *container,
						   NolphinIconData *data,
						   gboolean start_flag);
        void         (* icon_added)               (NolphinIconContainer *container,
                                                   NolphinIconData *data);
        void         (* icon_removed)             (NolphinIconContainer *container,
                                                   NolphinIconData *data);
        void         (* cleared)                  (NolphinIconContainer *container);
	gboolean     (* start_interactive_search) (NolphinIconContainer *container);
} NolphinIconContainerClass;

/* GtkObject */
GType             nolphin_icon_container_get_type                      (void);
GtkWidget *       nolphin_icon_container_new                           (void);


/* adding, removing, and managing icons */
void              nolphin_icon_container_clear                         (NolphinIconContainer  *view);
gboolean          nolphin_icon_container_icon_is_new_for_monitor       (NolphinIconContainer *container,
                                                                     NolphinIcon          *icon,
                                                                     gint               current_monitor);
gboolean          nolphin_icon_container_add                           (NolphinIconContainer  *view,
									 NolphinIconData       *data);
void              nolphin_icon_container_layout_now                    (NolphinIconContainer *container);
gboolean          nolphin_icon_container_remove                        (NolphinIconContainer  *view,
									 NolphinIconData       *data);
void              nolphin_icon_container_for_each                      (NolphinIconContainer  *view,
									 NolphinIconCallback    callback,
									 gpointer                callback_data);
void              nolphin_icon_container_request_update                (NolphinIconContainer  *view,
									 NolphinIconData       *data);
void              nolphin_icon_container_invalidate_labels             (NolphinIconContainer  *container);
void              nolphin_icon_container_request_update_all            (NolphinIconContainer  *container);
void              nolphin_icon_container_reveal                        (NolphinIconContainer  *container,
									 NolphinIconData       *data);
gboolean          nolphin_icon_container_is_empty                      (NolphinIconContainer  *container);
NolphinIconData *nolphin_icon_container_get_first_visible_icon        (NolphinIconContainer  *container);
void              nolphin_icon_container_scroll_to_icon                (NolphinIconContainer  *container,
									 NolphinIconData       *data);

void              nolphin_icon_container_begin_loading                 (NolphinIconContainer  *container);
void              nolphin_icon_container_end_loading                   (NolphinIconContainer  *container,
									 gboolean                all_icons_added);

/* control the layout */
gboolean          nolphin_icon_container_is_auto_layout                (NolphinIconContainer  *container);
void              nolphin_icon_container_set_auto_layout               (NolphinIconContainer  *container,
									 gboolean                auto_layout);

gboolean          nolphin_icon_container_is_keep_aligned               (NolphinIconContainer  *container);
void              nolphin_icon_container_set_keep_aligned              (NolphinIconContainer  *container,
									 gboolean                keep_aligned);
void              nolphin_icon_container_set_layout_mode               (NolphinIconContainer  *container,
									 NolphinIconLayoutMode  mode);
void              nolphin_icon_container_set_horizontal_layout (NolphinIconContainer *container,
                                                             gboolean           horizontal);
gboolean          nolphin_icon_container_get_horizontal_layout (NolphinIconContainer *container);
void              nolphin_icon_container_set_grid_adjusts (NolphinIconContainer *container,
                                                        gint               h_adjust,
                                                        gint               v_adjust);

void              nolphin_icon_container_set_label_position            (NolphinIconContainer  *container,
									 NolphinIconLabelPosition pos);
void              nolphin_icon_container_sort                          (NolphinIconContainer  *container);
void              nolphin_icon_container_freeze_icon_positions         (NolphinIconContainer  *container);

gint               nolphin_icon_container_get_max_layout_lines           (NolphinIconContainer  *container);
gint               nolphin_icon_container_get_max_layout_lines_for_pango (NolphinIconContainer  *container);

void              nolphin_icon_container_set_highlighted_for_clipboard (NolphinIconContainer  *container,
									 GList                  *clipboard_icon_data);

/* operations on all icons */
void              nolphin_icon_container_unselect_all                  (NolphinIconContainer  *view);
void              nolphin_icon_container_select_all                    (NolphinIconContainer  *view);
void              nolphin_icon_container_select_first                  (NolphinIconContainer  *container);


/* operations on the selection */
void              nolphin_icon_container_update_selection              (NolphinIconContainer *container);
GList     *       nolphin_icon_container_get_selection                 (NolphinIconContainer  *view);
GList     *       nolphin_icon_container_peek_selection                (NolphinIconContainer  *view);
gint              nolphin_icon_container_get_selection_count           (NolphinIconContainer  *container);
void			  nolphin_icon_container_invert_selection				(NolphinIconContainer  *view);
void              nolphin_icon_container_set_selection                 (NolphinIconContainer  *view,
									 GList                  *selection);
GArray    *       nolphin_icon_container_get_selected_icon_locations   (NolphinIconContainer  *view);
gboolean          nolphin_icon_container_has_stretch_handles           (NolphinIconContainer  *container);
gboolean          nolphin_icon_container_is_stretched                  (NolphinIconContainer  *container);
void              nolphin_icon_container_show_stretch_handles          (NolphinIconContainer  *container);
void              nolphin_icon_container_unstretch                     (NolphinIconContainer  *container);
void              nolphin_icon_container_start_renaming_selected_item  (NolphinIconContainer  *container,
									 gboolean                select_all);
/* options */
NolphinZoomLevel nolphin_icon_container_get_zoom_level                (NolphinIconContainer  *view);
void              nolphin_icon_container_set_zoom_level                (NolphinIconContainer  *view,
									 int                     new_zoom_level);
void              nolphin_icon_container_set_single_click_mode         (NolphinIconContainer  *container,
									 gboolean                single_click_mode);
void              nolphin_icon_container_set_click_to_rename_enabled (NolphinIconContainer *container,
                                                                             gboolean enabled);
void              nolphin_icon_container_enable_linger_selection       (NolphinIconContainer  *view,
									 gboolean                enable);
gboolean          nolphin_icon_container_get_is_fixed_size             (NolphinIconContainer  *container);
void              nolphin_icon_container_set_is_fixed_size             (NolphinIconContainer  *container,
									 gboolean                is_fixed_size);
gboolean          nolphin_icon_container_get_is_desktop                (NolphinIconContainer  *container);
void              nolphin_icon_container_set_is_desktop                (NolphinIconContainer  *container,
									 gboolean                is_desktop);
gboolean          nolphin_icon_container_get_show_desktop_tooltips     (NolphinIconContainer *container);
void              nolphin_icon_container_set_show_desktop_tooltips     (NolphinIconContainer *container,
                                                                              gboolean  show_tooltips);
void              nolphin_icon_container_set_filter_highlight          (NolphinIconContainer  *container,
									 const char             *filter_text);
const char       *nolphin_icon_container_get_filter_highlight          (NolphinIconContainer  *container);

void              nolphin_icon_container_reset_scroll_region           (NolphinIconContainer  *container);
void              nolphin_icon_container_set_font                      (NolphinIconContainer  *container,
									 const char             *font); 
void              nolphin_icon_container_set_font_size_table           (NolphinIconContainer  *container,
									 const int               font_size_table[NOLPHIN_ZOOM_LEVEL_LARGEST + 1]);
void              nolphin_icon_container_set_margins                   (NolphinIconContainer  *container,
									 int                     left_margin,
									 int                     right_margin,
									 int                     top_margin,
									 int                     bottom_margin);
void              nolphin_icon_container_set_use_drop_shadows          (NolphinIconContainer  *container,
									 gboolean                use_drop_shadows);
char*             nolphin_icon_container_get_icon_description          (NolphinIconContainer  *container,
                                                                         NolphinIconData       *data);
gboolean          nolphin_icon_container_get_allow_moves               (NolphinIconContainer  *container);
void              nolphin_icon_container_set_allow_moves               (NolphinIconContainer  *container,
									 gboolean                allow_moves);
void		  nolphin_icon_container_set_forced_icon_size		(NolphinIconContainer  *container,
									 int                     forced_icon_size);
void		  nolphin_icon_container_set_all_columns_same_width	(NolphinIconContainer  *container,
									 gboolean                all_columns_same_width);

gboolean	  nolphin_icon_container_is_layout_rtl			(NolphinIconContainer  *container);
gboolean	  nolphin_icon_container_is_layout_vertical		(NolphinIconContainer  *container);

gboolean          nolphin_icon_container_get_store_layout_timestamps   (NolphinIconContainer  *container);
void              nolphin_icon_container_set_store_layout_timestamps   (NolphinIconContainer  *container,
									 gboolean                store_layout);

void              nolphin_icon_container_widget_to_file_operation_position (NolphinIconContainer *container,
									     GdkPoint              *position);

void         nolphin_icon_container_setup_tooltip_preference_callback (NolphinIconContainer *container);
void         nolphin_icon_container_update_tooltip_text (NolphinIconContainer  *container,
                                                      NolphinIconCanvasItem *item);
gint         nolphin_icon_container_get_additional_text_line_count (NolphinIconContainer *container);
void         nolphin_icon_container_set_ok_to_load_deferred_attrs (NolphinIconContainer *container,
                                                                gboolean           ok);
#endif /* NOLPHIN_ICON_CONTAINER_H */
