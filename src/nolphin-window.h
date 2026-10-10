/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

/*
 *  Nolphin
 *
 *  Copyright (C) 1999, 2000 Red Hat, Inc.
 *  Copyright (C) 1999, 2000, 2001 Eazel, Inc.
 *
 *  Nolphin is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  Nolphin is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Suite 500, MA 02110-1335, USA.
 *
 *  Authors: Elliot Lee <sopwith@redhat.com>
 *           Darin Adler <darin@bentspoon.com>
 *
 */
/* nolphin-window.h: Interface of the main window object */

#ifndef NOLPHIN_WINDOW_H
#define NOLPHIN_WINDOW_H

#include <gtk/gtk.h>
#include <eel/eel-glib-extensions.h>
#include <libnolphin-private/nolphin-bookmark.h>
#include <libnolphin-private/nolphin-search-directory.h>

#include "nolphin-navigation-state.h"
#include "nolphin-view.h"
#include "nolphin-window-types.h"

#define NOLPHIN_TYPE_WINDOW nolphin_window_get_type()
#define NOLPHIN_WINDOW(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NOLPHIN_TYPE_WINDOW, NolphinWindow))
#define NOLPHIN_WINDOW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NOLPHIN_TYPE_WINDOW, NolphinWindowClass))
#define NOLPHIN_IS_WINDOW(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NOLPHIN_TYPE_WINDOW))
#define NOLPHIN_IS_WINDOW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NOLPHIN_TYPE_WINDOW))
#define NOLPHIN_WINDOW_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NOLPHIN_TYPE_WINDOW, NolphinWindowClass))

typedef enum {
        NOLPHIN_WINDOW_SHOW_HIDDEN_FILES_ENABLE,
        NOLPHIN_WINDOW_SHOW_HIDDEN_FILES_DISABLE
} NolphinWindowShowHiddenFilesMode;

typedef enum {
        NOLPHIN_WINDOW_NOT_SHOWN,
        NOLPHIN_WINDOW_POSITION_SET,
        NOLPHIN_WINDOW_SHOULD_SHOW
} NolphinWindowShowState;

typedef enum {
	NOLPHIN_WINDOW_OPEN_SLOT_NONE = 0,
	NOLPHIN_WINDOW_OPEN_SLOT_APPEND = 1
}  NolphinWindowOpenSlotFlags;

enum {
    SORT_NULL = -1,
    SORT_ASCENDING = 0,
    SORT_DESCENDING = 1
};

#define NOLPHIN_WINDOW_SIDEBAR_PLACES "places"
#define NOLPHIN_WINDOW_SIDEBAR_TREE "tree"

typedef struct NolphinWindowDetails NolphinWindowDetails;

typedef struct {
        GtkApplicationWindowClass parent_spot;

	/* Function pointers for overriding, without corresponding signals */

        void   (* sync_title) (NolphinWindow *window,
			       NolphinWindowSlot *slot);
        NolphinIconInfo * (* get_icon) (NolphinWindow *window,
                                         NolphinWindowSlot *slot);

        void   (* prompt_for_location) (NolphinWindow *window, const char *initial);
        void   (* close) (NolphinWindow *window);

        /* Signals used only for keybindings */
        void   (* go_up)  (NolphinWindow *window);
	void   (* reload) (NolphinWindow *window);
} NolphinWindowClass;

struct NolphinWindow {
        GtkApplicationWindow parent_object;
        
        NolphinWindowDetails *details;
};

GType            nolphin_window_get_type             (void);
NolphinWindow *     nolphin_window_new                  (GtkApplication    *application,
                                                   GdkScreen         *screen);
void             nolphin_window_close                (NolphinWindow    *window);

void             nolphin_window_connect_content_view (NolphinWindow    *window,
						       NolphinView      *view);
void             nolphin_window_disconnect_content_view (NolphinWindow    *window,
							  NolphinView      *view);

void             nolphin_window_go_to                (NolphinWindow    *window,
                                                       GFile             *location);
void             nolphin_window_go_to_tab            (NolphinWindow    *window,
                                                       GFile             *location);
void             nolphin_window_go_to_full           (NolphinWindow    *window,
                                                       GFile             *location,
                                                       NolphinWindowGoToCallback callback,
                                                       gpointer           user_data);
void             nolphin_window_new_tab              (NolphinWindow    *window);
void             nolphin_window_duplicate_tab        (NolphinWindow    *window);
void             nolphin_window_close_all_tabs       (NolphinWindow    *window);
void             nolphin_window_toggle_lock_tab      (NolphinWindow    *window);
void             nolphin_window_workspace_capture    (NolphinWindow    *window, GKeyFile *kf);
gboolean         nolphin_window_workspace_apply      (NolphinWindow    *window, GKeyFile *kf);
typedef enum {
	NOLPHIN_SPLIT_LAYOUT_TWO_COLUMNS,
	NOLPHIN_SPLIT_LAYOUT_THREE_COLUMNS,
	NOLPHIN_SPLIT_LAYOUT_GRID,
	NOLPHIN_SPLIT_LAYOUT_BIG_PLUS_TWO,
	NOLPHIN_SPLIT_LAYOUT_TWO_ROWS
} NolphinSplitLayout;

void             nolphin_window_apply_split_layout   (NolphinWindow    *window,
                                                       NolphinSplitLayout layout);
void             nolphin_window_activate_pane_number (NolphinWindow    *window,
                                                       gint               number);
void             nolphin_window_activate_previous_pane (NolphinWindow  *window);
void             nolphin_window_show_pane_numbers    (NolphinWindow    *window);
void             nolphin_window_split_view_add_pane  (NolphinWindow    *window);
void             nolphin_window_close_active_pane    (NolphinWindow    *window);
void             nolphin_window_toggle_maximize_pane (NolphinWindow    *window);
void             nolphin_window_rename_tab           (NolphinWindow    *window);
void             nolphin_window_sync_tab_actions     (NolphinWindow    *window);
gboolean         nolphin_window_has_closed_tab_history (NolphinWindow  *window);
void             nolphin_window_restore_closed_tab   (NolphinWindow    *window);

GtkUIManager *   nolphin_window_get_ui_manager       (NolphinWindow    *window);
GtkActionGroup * nolphin_window_get_main_action_group (NolphinWindow   *window);

/* Die rechte Arbeitsbereich-Leiste (§30) - GtkStack mit "preview" als
 * Ruhelage; siehe nolphin-workspace-panel.h für die nolphin_workspace_
 * panel_show_*()-Funktionen, mit denen einzelne Funktionen (Eigenschaften,
 * Archiv, Git, .deb-Paket) sie bei Bedarf aktivieren. */
GtkWidget      * nolphin_window_get_workspace_panel   (NolphinWindow   *window);
GtkWidget      * nolphin_window_get_terminal          (NolphinWindow   *window);
NolphinNavigationState * 
                 nolphin_window_get_navigation_state (NolphinWindow    *window);

void                 nolphin_window_report_load_complete     (NolphinWindow *window,
                                                               NolphinView *view);

NolphinWindowSlot * nolphin_window_get_extra_slot       (NolphinWindow *window);
NolphinWindowShowHiddenFilesMode
                     nolphin_window_get_hidden_files_mode (NolphinWindow *window);
void                 nolphin_window_set_hidden_files_mode (NolphinWindow *window,
                                                            NolphinWindowShowHiddenFilesMode  mode);
void                 nolphin_window_report_load_underway  (NolphinWindow *window,
                                                            NolphinView *view);
void                 nolphin_window_view_visible          (NolphinWindow *window,
                                                            NolphinView *view);
GList *              nolphin_window_get_panes             (NolphinWindow *window);
NolphinWindowSlot * nolphin_window_get_active_slot       (NolphinWindow *window);
void                 nolphin_window_push_status           (NolphinWindow *window,
                                                            const char *text);
GtkWidget *          nolphin_window_ensure_location_bar   (NolphinWindow *window);
void                 nolphin_window_sync_location_widgets (NolphinWindow *window);
void                 nolphin_window_sync_search_widgets   (NolphinWindow *window);
void                 nolphin_window_grab_focus            (NolphinWindow *window);
void                 nolphin_window_sync_create_folder_button (NolphinWindow *window);
void     nolphin_window_hide_sidebar         (NolphinWindow *window);
void     nolphin_window_show_sidebar         (NolphinWindow *window);
void     nolphin_window_back_or_forward      (NolphinWindow *window,
                                               gboolean        back,
                                               guint           distance,
                                               NolphinWindowOpenFlags flags);
void     nolphin_window_split_view_on        (NolphinWindow *window);
void     nolphin_window_split_view_off       (NolphinWindow *window);
gboolean nolphin_window_split_view_showing   (NolphinWindow *window);
void     nolphin_window_split_view_toggle_horizontal (NolphinWindow *window);

void     nolphin_window_set_show_terminal    (NolphinWindow *window,
                                               gboolean        show);
gboolean nolphin_window_terminal_showing     (NolphinWindow *window);
void     nolphin_window_sync_terminal_location (NolphinWindow *window);

void     nolphin_window_set_show_preview     (NolphinWindow *window,
                                               gboolean        show);
gboolean nolphin_window_preview_showing      (NolphinWindow *window);
void     nolphin_window_sync_preview_selection (NolphinWindow *window);

gboolean nolphin_window_disable_chrome_mapping (GValue *value,
                                                 GVariant *variant,
                                                 gpointer user_data);

void     nolphin_window_set_sidebar_id (NolphinWindow *window,
                                    const gchar *id);

const gchar *    nolphin_window_get_sidebar_id (NolphinWindow *window);

void    nolphin_window_set_show_sidebar (NolphinWindow *window,
                                      gboolean show);

gboolean  nolphin_window_get_show_sidebar (NolphinWindow *window);

const gchar *nolphin_window_get_ignore_meta_view_id (NolphinWindow *window);
void         nolphin_window_set_ignore_meta_view_id (NolphinWindow *window, const gchar *id);
gint         nolphin_window_get_ignore_meta_zoom_level (NolphinWindow *window);
void         nolphin_window_set_ignore_meta_zoom_level (NolphinWindow *window, gint level);
GList       *nolphin_window_get_ignore_meta_visible_columns (NolphinWindow *window);
void         nolphin_window_set_ignore_meta_visible_columns (NolphinWindow *window, GList *list);
GList       *nolphin_window_get_ignore_meta_column_order (NolphinWindow *window);
void         nolphin_window_set_ignore_meta_column_order (NolphinWindow *window, GList *list);
const gchar *nolphin_window_get_ignore_meta_sort_column (NolphinWindow *window);
void         nolphin_window_set_ignore_meta_sort_column (NolphinWindow *window, const gchar *column);
gint         nolphin_window_get_ignore_meta_sort_direction (NolphinWindow *window);
void         nolphin_window_set_ignore_meta_sort_direction (NolphinWindow *window, gint direction);

void         nolphin_window_clear_secondary_pane_location (NolphinWindow *window);
NolphinWindowOpenFlags nolphin_event_get_window_open_flags   (void);

void nolphin_window_slot_added (NolphinWindow *window,  NolphinWindowSlot *slot);
void nolphin_window_slot_removed (NolphinWindow *window,  NolphinWindowSlot *slot);

#endif
