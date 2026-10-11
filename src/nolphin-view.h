/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* nolphin-view.h
 *
 * Copyright (C) 1999, 2000  Free Software Foundaton
 * Copyright (C) 2000, 2001  Eazel, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
 * Boston, MA 02110-1335, USA.
 *
 * Authors: Ettore Perazzoli
 * 	    Darin Adler <darin@bentspoon.com>
 * 	    John Sullivan <sullivan@eazel.com>
 *          Pavel Cisler <pavel@eazel.com>
 */

#ifndef NOLPHIN_VIEW_H
#define NOLPHIN_VIEW_H

#include <gtk/gtk.h>
#include <gio/gio.h>

#define NOLPHIN_FILTER_NO_MATCH G_MAXINT

#include <libnolphin-private/nolphin-directory.h>
#include <libnolphin-private/nolphin-file.h>
#include <libnolphin-private/nolphin-icon-container.h>
#include <libnolphin-private/nolphin-link.h>

typedef struct NolphinView NolphinView;
typedef struct NolphinViewClass NolphinViewClass;

#include "nolphin-window.h"
#include "nolphin-window-slot.h"

#define NOLPHIN_TYPE_VIEW nolphin_view_get_type()
#define NOLPHIN_VIEW(obj)\
	(G_TYPE_CHECK_INSTANCE_CAST ((obj), NOLPHIN_TYPE_VIEW, NolphinView))
#define NOLPHIN_VIEW_CLASS(klass)\
	(G_TYPE_CHECK_CLASS_CAST ((klass), NOLPHIN_TYPE_VIEW, NolphinViewClass))
#define NOLPHIN_IS_VIEW(obj)\
	(G_TYPE_CHECK_INSTANCE_TYPE ((obj), NOLPHIN_TYPE_VIEW))
#define NOLPHIN_IS_VIEW_CLASS(klass)\
	(G_TYPE_CHECK_CLASS_TYPE ((klass), NOLPHIN_TYPE_VIEW))
#define NOLPHIN_VIEW_GET_CLASS(obj)\
	(G_TYPE_INSTANCE_GET_CLASS ((obj), NOLPHIN_TYPE_VIEW, NolphinViewClass))

typedef struct NolphinViewDetails NolphinViewDetails;

struct NolphinView {
	GtkScrolledWindow parent;

	NolphinViewDetails *details;
};

struct NolphinViewClass {
	GtkScrolledWindowClass parent_class;

	/* The 'clear' signal is emitted to empty the view of its contents.
	 * It must be replaced by each subclass.
	 */
	void 	(* clear) 		 (NolphinView *view);
	
	/* The 'begin_file_changes' signal is emitted before a set of files
	 * are added to the view. It can be replaced by a subclass to do any 
	 * necessary preparation for a set of new files. The default
	 * implementation does nothing.
	 */
	void 	(* begin_file_changes) (NolphinView *view);
	
	/* The 'add_file' signal is emitted to add one file to the view.
	 * It must be replaced by each subclass.
	 */
	void    (* add_file) 		 (NolphinView *view, 
					  NolphinFile *file,
					  NolphinDirectory *directory);
	void    (* remove_file)		 (NolphinView *view, 
					  NolphinFile *file,
					  NolphinDirectory *directory);

	/* The 'file_changed' signal is emitted to signal a change in a file,
	 * including the file being removed.
	 * It must be replaced by each subclass.
	 */
	void 	(* file_changed)         (NolphinView *view, 
					  NolphinFile *file,
					  NolphinDirectory *directory);

	/* The 'end_file_changes' signal is emitted after a set of files
	 * are added to the view. It can be replaced by a subclass to do any 
	 * necessary cleanup (typically, cleanup for code in begin_file_changes).
	 * The default implementation does nothing.
	 */
	void 	(* end_file_changes)    (NolphinView *view);
	
	/* The 'begin_loading' signal is emitted before any of the contents
	 * of a directory are added to the view. It can be replaced by a 
	 * subclass to do any necessary preparation to start dealing with a
	 * new directory. The default implementation does nothing.
	 */
	void 	(* begin_loading) 	 (NolphinView *view);

	/* The 'end_loading' signal is emitted after all of the contents
	 * of a directory are added to the view. It can be replaced by a 
	 * subclass to do any necessary clean-up. The default implementation 
	 * does nothing.
	 *
	 * If all_files_seen is true, the handler may assume that
	 * no load error ocurred, and all files of the underlying
	 * directory were loaded.
	 *
	 * Otherwise, end_loading was emitted due to cancellation,
	 * which usually means that not all files are available.
	 */
	void 	(* end_loading) 	 (NolphinView *view,
					  gboolean all_files_seen);

	/* The 'load_error' signal is emitted when the directory model
	 * reports an error in the process of monitoring the directory's
	 * contents.  The load error indicates that the process of 
	 * loading the contents has ended, but the directory is still
	 * being monitored. The default implementation handles common
	 * load failures like ACCESS_DENIED.
	 */
	void    (* load_error)           (NolphinView *view,
					  GError *error);

	/* Function pointers that don't have corresponding signals */

        /* reset_to_defaults is a function pointer that subclasses must 
         * override to set sort order, zoom level, etc to match default
         * values. 
         */
        void     (* reset_to_defaults)	         (NolphinView *view);

	/* get_backing uri is a function pointer for subclasses to
	 * override. Subclasses may replace it with a function that
	 * returns the URI for the location where to create new folders,
	 * files, links and paste the clipboard to.
	 */

	char *	(* get_backing_uri)		(NolphinView *view);

	/* get_selection is not a signal; it is just a function pointer for
	 * subclasses to replace (override). Subclasses must replace it
	 * with a function that returns a newly-allocated GList of
	 * NolphinFile pointers.
	 */
	GList *	(* get_selection) 	 	(NolphinView *view);

    /* peek_selection is not a signal; it is just a function pointer for
     * subclasses to replace (override). Subclasses must replace it
     * with a function that returns a pointer to the existing container
     * selection list.
     */
    GList * (* peek_selection)       (NolphinView *view);

    /* get_selection_count is not a signal; it is just a function pointer for
     * subclasses to replace (override). Subclasses must replace it
     * with a function that returns the current selection count.
     */
    gint    (* get_selection_count)       (NolphinView *view);

	/* get_selection_for_file_transfer  is a function pointer for
	 * subclasses to replace (override). Subclasses must replace it
	 * with a function that returns a newly-allocated GList of
	 * NolphinFile pointers. The difference from get_selection is
	 * that any files in the selection that also has a parent folder
	 * in the selection is not included.
	 */
	GList *	(* get_selection_for_file_transfer)(NolphinView *view);
	
        /* select_all is a function pointer that subclasses must override to
         * select all of the items in the view */
        void     (* select_all)	         	(NolphinView *view);

        /* set_selection is a function pointer that subclasses must
         * override to select the specified items (and unselect all
         * others). The argument is a list of NolphinFiles. */

        void     (* set_selection)	 	(NolphinView *view, 
        					 GList *selection);
        					 
        /* invert_selection is a function pointer that subclasses must
         * override to invert selection. */

        void     (* invert_selection)	 	(NolphinView *view);        					 

	/* Return an array of locations of selected icons in their view. */
	GArray * (* get_selected_icon_locations) (NolphinView *view);

	guint    (* get_item_count)             (NolphinView *view);

        /* bump_zoom_level is a function pointer that subclasses must override
         * to change the zoom level of an object. */
        void    (* bump_zoom_level)      	(NolphinView *view,
					  	 int zoom_increment);

        /* zoom_to_level is a function pointer that subclasses must override
         * to set the zoom level of an object to the specified level. */
        void    (* zoom_to_level) 		(NolphinView *view, 
        				         NolphinZoomLevel level);

        NolphinZoomLevel (* get_zoom_level)    (NolphinView *view);

	/* restore_default_zoom_level is a function pointer that subclasses must override
         * to restore the zoom level of an object to a default setting. */
        void    (* restore_default_zoom_level) (NolphinView *view);

    /* return the default zoom level for the current view */
        NolphinZoomLevel  (* get_default_zoom_level)   (NolphinView *view);
        /* can_zoom_in is a function pointer that subclasses must override to
         * return whether the view is at maximum size (furthest-in zoom level) */
        gboolean (* can_zoom_in)	 	(NolphinView *view);

        /* can_zoom_out is a function pointer that subclasses must override to
         * return whether the view is at minimum size (furthest-out zoom level) */
        gboolean (* can_zoom_out)	 	(NolphinView *view);
        
        /* reveal_selection is a function pointer that subclasses may
         * override to make sure the selected items are sufficiently
         * apparent to the user (e.g., scrolled into view). By default,
         * this does nothing.
         */
        void     (* reveal_selection)	 	(NolphinView *view);

        /* merge_menus is a function pointer that subclasses can override to
         * add their own menu items to the window's menu bar.
         * If overridden, subclasses must call parent class's function.
         */
        void    (* merge_menus)         	(NolphinView *view);
        void    (* unmerge_menus)         	(NolphinView *view);

        /* update_menus is a function pointer that subclasses can override to
         * update the sensitivity or wording of menu items in the menu bar.
         * It is called (at least) whenever the selection changes. If overridden, 
         * subclasses must call parent class's function.
         */
        void    (* update_menus)         	(NolphinView *view);

	/* sort_files is a function pointer that subclasses can override
	 * to provide a sorting order to determine which files should be
	 * presented when only a partial list is provided.
	 */
	int     (* compare_files)              (NolphinView *view,
						NolphinFile    *a,
						NolphinFile    *b);

	/* using_manual_layout is a function pointer that subclasses may
	 * override to control whether or not items can be freely positioned
	 * on the user-visible area.
	 * Note that this value is not guaranteed to be constant within the
	 * view's lifecycle. */
	gboolean (* using_manual_layout)     (NolphinView *view);

	/* is_read_only is a function pointer that subclasses may
	 * override to control whether or not the user is allowed to
	 * change the contents of the currently viewed directory. The
	 * default implementation checks the permissions of the
	 * directory.
	 */
	gboolean (* is_read_only)	        (NolphinView *view);

	/* is_empty is a function pointer that subclasses must
	 * override to report whether the view contains any items.
	 */
	gboolean (* is_empty)                   (NolphinView *view);

	gboolean (* can_rename_file)            (NolphinView *view,
						 NolphinFile *file);
	/* select_all specifies whether the whole filename should be selected
	 * or only its basename (i.e. everything except the extension)
	 * */
	void	 (* start_renaming_file)        (NolphinView *view,
					  	 NolphinFile *file,
						 gboolean select_all);

	/* convert *point from widget's coordinate system to a coordinate
	 * system used for specifying file operation positions, which is view-specific.
	 *
	 * This is used by the the icon view, which converts the screen position to a zoom
	 * level-independent coordinate system.
	 */
	void (* widget_to_file_operation_position) (NolphinView *view,
						    GdkPoint     *position);

	/* Preference change callbacks, overriden by icon and list views. 
	 * Icon and list views respond by synchronizing to the new preference
	 * values and forcing an update if appropriate.
	 */
    void    (* click_policy_changed)       (NolphinView *view);
	void	(* click_to_rename_mode_changed)   (NolphinView *view);
	void	(* sort_directories_first_changed) (NolphinView *view);
	void	(* sort_favorites_first_changed) (NolphinView *view);

	/* Get the id string for this view. Its a constant string, not memory managed */
	const char *   (* get_view_id)            (NolphinView          *view);

	/* Return the uri of the first visible file */	
	char *         (* get_first_visible_file) (NolphinView          *view);
	/* Scroll the view so that the file specified by the uri is at the top
	   of the view */
	void           (* scroll_to_file)	  (NolphinView          *view,
						   const char            *uri);

	void    (* update_filter_text)             (NolphinView *view,
						   const char  *filter_text);

	void    (* select_first)                   (NolphinView *view);

        /* Signals used only for keybindings */
        gboolean (* trash)                         (NolphinView *view);
        gboolean (* delete)                        (NolphinView *view);

	/* Returns the current sort attribute string, or NULL if unknown/default */
	const gchar * (* get_sort_attribute)       (NolphinView *view);
};

/* GObject support */
GType               nolphin_view_get_type                         (void);

/* Functions callable from the user interface and elsewhere. */
NolphinWindow     *nolphin_view_get_nolphin_window              (NolphinView  *view);
NolphinWindowSlot *nolphin_view_get_nolphin_window_slot     (NolphinView  *view);
char *              nolphin_view_get_uri                          (NolphinView  *view);

void                nolphin_view_display_selection_info           (NolphinView  *view);

GdkAtom	            nolphin_view_get_copied_files_atom            (NolphinView  *view);
gboolean            nolphin_view_get_active                       (NolphinView  *view);

/* Wrappers for signal emitters. These are normally called 
 * only by NolphinView itself. They have corresponding signals
 * that observers might want to connect with.
 */
gboolean            nolphin_view_get_loading                      (NolphinView  *view);

/* Hooks for subclasses to call. These are normally called only by 
 * NolphinView and its subclasses 
 */
void                nolphin_view_activate_files                   (NolphinView        *view,
								    GList                  *files,
								    NolphinWindowOpenFlags flags,
								    gboolean                confirm_multiple);
void                nolphin_view_activate_file (NolphinView *view,
                                             NolphinFile *file,
                                             NolphinWindowOpenFlags flags);
void                nolphin_view_preview_files                    (NolphinView        *view,
								    GList               *files,
								    GArray              *locations);
void                nolphin_view_start_batching_selection_changes (NolphinView  *view);
void                nolphin_view_stop_batching_selection_changes  (NolphinView  *view);
void                nolphin_view_notify_selection_changed         (NolphinView  *view);
GtkUIManager *      nolphin_view_get_ui_manager                   (NolphinView  *view);
NolphinDirectory  *nolphin_view_get_model                        (NolphinView  *view);
NolphinFile       *nolphin_view_get_directory_as_file            (NolphinView  *view);
void            nolphin_view_update_actions_and_extensions        (NolphinView *view);

void                nolphin_view_pop_up_background_context_menu   (NolphinView  *view,
								    GdkEventButton   *event);
void                nolphin_view_pop_up_selection_context_menu    (NolphinView  *view,
								    GdkEventButton   *event); 
gboolean            nolphin_view_should_show_file                 (NolphinView  *view,
								    NolphinFile     *file);
gboolean	    nolphin_view_should_sort_directories_first    (NolphinView  *view);
gboolean	    nolphin_view_should_sort_favorites_first    (NolphinView  *view);
void                nolphin_view_ignore_hidden_file_preferences   (NolphinView  *view);
void                nolphin_view_set_show_foreign                 (NolphinView  *view,
								    gboolean          show_foreign);
gboolean            nolphin_view_handle_scroll_event              (NolphinView  *view,
								    GdkEventScroll   *event);

void                nolphin_view_freeze_updates                   (NolphinView  *view);
void                nolphin_view_unfreeze_updates                 (NolphinView  *view);
gboolean            nolphin_view_get_is_renaming                  (NolphinView  *view);
void                nolphin_view_set_is_renaming                  (NolphinView  *view,
								    gboolean       renaming);

/* Type-to-filter */
void                nolphin_view_set_filter_text                  (NolphinView  *view,
                                                                    const char    *text);
void                nolphin_view_clear_filter                     (NolphinView  *view);
gboolean            nolphin_view_get_filter_active                (NolphinView  *view);
const char         *nolphin_view_get_filter_text                  (NolphinView  *view);
gint                nolphin_view_get_filter_match                 (NolphinView  *view,
                                                                    NolphinFile  *file);
gboolean            nolphin_view_activate_filter                  (NolphinView  *view,
                                                                    GdkEventKey   *event);
void                nolphin_view_add_subdirectory                (NolphinView  *view,
								   NolphinDirectory*directory);
void                nolphin_view_remove_subdirectory             (NolphinView  *view,
								   NolphinDirectory*directory);

gboolean            nolphin_view_is_editable                     (NolphinView *view);

/* NolphinView methods */
const char *      nolphin_view_get_view_id                (NolphinView      *view);

/* file operations */
char *            nolphin_view_get_backing_uri            (NolphinView      *view);
void              nolphin_view_move_copy_items            (NolphinView      *view,
							    const GList       *item_uris,
							    GArray            *relative_item_points,
							    const char        *target_uri,
							    int                copy_action,
							    int                x,
							    int                y);
void              nolphin_view_new_file_with_initial_contents (NolphinView *view,
								const char *parent_uri,
								const char *filename,
								const char *initial_contents,
								int length,
								GdkPoint *pos);

/* selection handling */
int               nolphin_view_get_selection_count        (NolphinView      *view);
/* Löst eine Aktion der Ansicht (z. B. "GitSync") per Name aus, wie ein
 * Klick im Kontextmenü - für Knöpfe in der rechten Arbeitsleiste. */
void              nolphin_view_activate_action_by_name    (NolphinView      *view,
                                                           const gchar      *action_name);
GList *           nolphin_view_get_selection              (NolphinView      *view);
GList *           nolphin_view_peek_selection             (NolphinView      *view);
gint              nolphin_view_get_selection_count        (NolphinView      *view);
void              nolphin_view_set_selection              (NolphinView      *view,
							    GList             *selection);


void              nolphin_view_load_location              (NolphinView      *view,
							    GFile             *location);
void              nolphin_view_stop_loading               (NolphinView      *view);

char **           nolphin_view_get_emblem_names_to_exclude (NolphinView     *view);
char *            nolphin_view_get_first_visible_file     (NolphinView      *view);
void              nolphin_view_scroll_to_file             (NolphinView      *view,
							    const char        *uri);
char *            nolphin_view_get_title                  (NolphinView      *view);
gboolean          nolphin_view_supports_zooming           (NolphinView      *view);
void              nolphin_view_bump_zoom_level            (NolphinView      *view,
							    int                zoom_increment);
void              nolphin_view_zoom_to_level              (NolphinView      *view,
							    NolphinZoomLevel  level);
void              nolphin_view_restore_default_zoom_level (NolphinView      *view);
gboolean          nolphin_view_can_zoom_in                (NolphinView      *view);
gboolean          nolphin_view_can_zoom_out               (NolphinView      *view);
NolphinZoomLevel nolphin_view_get_zoom_level             (NolphinView      *view);
void              nolphin_view_pop_up_location_context_menu (NolphinView    *view,
							      GdkEventButton  *event,
							      const char      *location);
void              nolphin_view_grab_focus                 (NolphinView      *view);
void              nolphin_view_update_menus               (NolphinView      *view);
void              nolphin_view_new_folder                 (NolphinView      *view);

#endif /* NOLPHIN_VIEW_H */
