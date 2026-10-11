/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nolphin-file.h: Nolphin file model.
 
   Copyright (C) 1999, 2000, 2001 Eazel, Inc.
  
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
  
   Author: Darin Adler <darin@bentspoon.com>
*/

#ifndef NOLPHIN_FILE_H
#define NOLPHIN_FILE_H

#include <gtk/gtk.h>
#include <gio/gio.h>
#include <pango/pango.h>
#include <libnolphin-private/nolphin-file-attributes.h>
#include <libnolphin-private/nolphin-icon-info.h>
#include <libnolphin-private/nolphin-search-engine.h>

/* NolphinFile is an object used to represent a single element of a
 * NolphinDirectory. It's lightweight and relies on NolphinDirectory
 * to do most of the work.
 */

/* NolphinFile is defined both here and in nolphin-directory.h. */
#ifndef NOLPHIN_FILE_DEFINED
#define NOLPHIN_FILE_DEFINED
typedef struct NolphinFile NolphinFile;
#endif

#define NOLPHIN_TYPE_FILE nolphin_file_get_type()
#define NOLPHIN_FILE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NOLPHIN_TYPE_FILE, NolphinFile))
#define NOLPHIN_FILE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NOLPHIN_TYPE_FILE, NolphinFileClass))
#define NOLPHIN_IS_FILE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NOLPHIN_TYPE_FILE))
#define NOLPHIN_IS_FILE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NOLPHIN_TYPE_FILE))
#define NOLPHIN_FILE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NOLPHIN_TYPE_FILE, NolphinFileClass))

typedef enum {
	NOLPHIN_FILE_SORT_NONE,
	NOLPHIN_FILE_SORT_BY_DISPLAY_NAME,
	NOLPHIN_FILE_SORT_BY_SIZE,
	NOLPHIN_FILE_SORT_BY_TYPE,
    NOLPHIN_FILE_SORT_BY_DETAILED_TYPE,
	NOLPHIN_FILE_SORT_BY_MTIME,
    NOLPHIN_FILE_SORT_BY_ATIME,
	NOLPHIN_FILE_SORT_BY_TRASHED_TIME,
	NOLPHIN_FILE_SORT_BY_BTIME,
	NOLPHIN_FILE_SORT_BY_SEARCH_RESULT_COUNT,
	NOLPHIN_FILE_SORT_BY_EXTENSION
} NolphinFileSortType;
typedef enum {
	NOLPHIN_REQUEST_NOT_STARTED,
	NOLPHIN_REQUEST_IN_PROGRESS,
	NOLPHIN_REQUEST_DONE
} NolphinRequestStatus;

typedef enum {
	NOLPHIN_FILE_ICON_FLAGS_NONE = 0,
	NOLPHIN_FILE_ICON_FLAGS_USE_THUMBNAILS = (1<<0),
	NOLPHIN_FILE_ICON_FLAGS_IGNORE_VISITING = (1<<1),
	NOLPHIN_FILE_ICON_FLAGS_EMBEDDING_TEXT = (1<<2),
	NOLPHIN_FILE_ICON_FLAGS_FOR_DRAG_ACCEPT = (1<<3),
	NOLPHIN_FILE_ICON_FLAGS_FOR_OPEN_FOLDER = (1<<4),
	/* whether the thumbnail size must match the display icon size */
	NOLPHIN_FILE_ICON_FLAGS_FORCE_THUMBNAIL_SIZE = (1<<5),
	/* uses the icon of the mount if present */
	NOLPHIN_FILE_ICON_FLAGS_USE_MOUNT_ICON = (1<<6),
	/* render the mount icon as an emblem over the regular one */
	NOLPHIN_FILE_ICON_FLAGS_USE_MOUNT_ICON_AS_EMBLEM = (1<<7),
    NOLPHIN_FILE_ICON_FLAGS_PIN_HEIGHT_FOR_DESKTOP = (1<<8)
} NolphinFileIconFlags;	

typedef enum {
    NOLPHIN_DATE_TYPE_MODIFIED,
    NOLPHIN_DATE_TYPE_CHANGED,
    NOLPHIN_DATE_TYPE_ACCESSED,
    NOLPHIN_DATE_TYPE_PERMISSIONS_CHANGED,
    NOLPHIN_DATE_TYPE_TRASHED,
    NOLPHIN_DATE_TYPE_CREATED
} NolphinDateType;

typedef enum {
    NOLPHIN_FILE_TOOLTIP_FLAGS_NONE = 0,
    NOLPHIN_FILE_TOOLTIP_FLAGS_FILE_TYPE =  (1<<0),
    NOLPHIN_FILE_TOOLTIP_FLAGS_MOD_DATE = (1<<1),
    NOLPHIN_FILE_TOOLTIP_FLAGS_ACCESS_DATE = (1<<2),
    NOLPHIN_FILE_TOOLTIP_FLAGS_PATH = (1<<3),
    NOLPHIN_FILE_TOOLTIP_FLAGS_CREATED_DATE = (1<<4)
} NolphinFileTooltipFlags;

typedef enum {
    NOLPHIN_FILE_LOAD_DEFERRED_ATTRS_NO,
    NOLPHIN_FILE_LOAD_DEFERRED_ATTRS_YES,
    NOLPHIN_FILE_LOAD_DEFERRED_ATTRS_PRELOAD
} NolphinFileLoadDeferredAttrs;

/* Emblems sometimes displayed for NolphinFiles. Do not localize. */ 
#define NOLPHIN_FILE_EMBLEM_NAME_SYMBOLIC_LINK "symbolic-link"
#define NOLPHIN_FILE_EMBLEM_NAME_CANT_READ "unreadable"
#define NOLPHIN_FILE_EMBLEM_NAME_CANT_WRITE "readonly"
#define NOLPHIN_FILE_EMBLEM_NAME_TRASH "trash"
#define NOLPHIN_FILE_EMBLEM_NAME_NOTE "note"
#define NOLPHIN_FILE_EMBLEM_NAME_FAVORITE "xapp-favorite"

typedef void (*NolphinFileCallback)          (NolphinFile  *file,
				               gpointer       callback_data);
typedef void (*NolphinFileListCallback)      (GList         *file_list,
				               gpointer       callback_data);
typedef void (*NolphinFileOperationCallback) (NolphinFile  *file,
					       GFile         *result_location,
					       GError        *error,
					       gpointer       callback_data);
typedef int (*NolphinWidthMeasureCallback)   (const char    *string,
					       void	     *context);
typedef char * (*NolphinTruncateCallback)    (const char    *string,
					       int	      width,
					       void	     *context);

#define NOLPHIN_FILE_ATTRIBUTES_FOR_ICON (NOLPHIN_FILE_ATTRIBUTE_INFO | NOLPHIN_FILE_ATTRIBUTE_LINK_INFO | NOLPHIN_FILE_ATTRIBUTE_THUMBNAIL)
#define NOLPHIN_FILE_DEFERRED_ATTRIBUTES (NOLPHIN_FILE_ATTRIBUTE_THUMBNAIL | NOLPHIN_FILE_ATTRIBUTE_EXTENSION_INFO)

typedef void NolphinFileListHandle;

/* GObject requirements. */
GType                   nolphin_file_get_type                          (void);

/* Getting at a single file. */
NolphinFile *          nolphin_file_get                               (GFile                          *location);
NolphinFile *          nolphin_file_get_by_uri                        (const char                     *uri);

/* Get a file only if the nolphin version already exists */
NolphinFile *          nolphin_file_get_existing                      (GFile                          *location);
NolphinFile *          nolphin_file_get_existing_by_uri               (const char                     *uri);

/* Covers for g_object_ref and g_object_unref that provide two conveniences:
 * 1) Using these is type safe.
 * 2) You are allowed to call these with NULL,
 */
NolphinFile *          nolphin_file_ref                               (NolphinFile                   *file);
void                    nolphin_file_unref                             (NolphinFile                   *file);

/* Monitor the file. */
void                    nolphin_file_monitor_add                       (NolphinFile                   *file,
									 gconstpointer                   client,
									 NolphinFileAttributes          attributes);
void                    nolphin_file_monitor_remove                    (NolphinFile                   *file,
									 gconstpointer                   client);

/* Waiting for data that's read asynchronously.
 * This interface currently works only for metadata, but could be expanded
 * to other attributes as well.
 */
void                    nolphin_file_call_when_ready                   (NolphinFile                   *file,
									 NolphinFileAttributes          attributes,
									 NolphinFileCallback            callback,
									 gpointer                        callback_data);
void                    nolphin_file_cancel_call_when_ready            (NolphinFile                   *file,
									 NolphinFileCallback            callback,
									 gpointer                        callback_data);
gboolean                nolphin_file_check_if_ready                    (NolphinFile                   *file,
									 NolphinFileAttributes          attributes);
void                    nolphin_file_invalidate_attributes             (NolphinFile                   *file,
									 NolphinFileAttributes          attributes);
void                    nolphin_file_invalidate_all_attributes         (NolphinFile                   *file);

void                    nolphin_file_increment_thumbnail_try_count     (NolphinFile                   *file);

/* Basic attributes for file objects. */
gboolean                nolphin_file_contains_text                     (NolphinFile                   *file);
char *                  nolphin_file_get_display_name                  (NolphinFile                   *file);
char *                  nolphin_file_get_edit_name                     (NolphinFile                   *file);
char *                  nolphin_file_get_name                          (NolphinFile                   *file);
const char *            nolphin_file_peek_name                         (NolphinFile                   *file);

GFile *                 nolphin_file_get_location                      (NolphinFile                   *file);
char *			 nolphin_file_get_description			 (NolphinFile			 *file);
char *                  nolphin_file_get_uri                           (NolphinFile                   *file);
char *                  nolphin_file_get_local_uri                     (NolphinFile                   *file);
char *                  nolphin_file_get_path                          (NolphinFile                   *file);
char *                  nolphin_file_get_uri_scheme                    (NolphinFile                   *file);
gboolean                nolphin_file_has_uri_scheme                    (NolphinFile *file, const gchar *scheme);
NolphinFile *          nolphin_file_get_parent                        (NolphinFile                   *file);
GFile *                 nolphin_file_get_parent_location               (NolphinFile                   *file);
char *                  nolphin_file_get_parent_uri                    (NolphinFile                   *file);
char *                  nolphin_file_get_parent_uri_for_display        (NolphinFile                   *file);
gboolean                nolphin_file_can_get_size                      (NolphinFile                   *file);
goffset                 nolphin_file_get_size                          (NolphinFile                   *file);
time_t                  nolphin_file_get_mtime                         (NolphinFile                   *file);
time_t                  nolphin_file_get_ctime                         (NolphinFile                   *file);
GFileType               nolphin_file_get_file_type                     (NolphinFile                   *file);
char *                  nolphin_file_get_mime_type                     (NolphinFile                   *file);
gboolean                nolphin_file_is_mime_type                      (NolphinFile                   *file,
									 const char                     *mime_type);
gboolean                nolphin_file_is_launchable                     (NolphinFile                   *file);
gboolean                nolphin_file_is_symbolic_link                  (NolphinFile                   *file);
gboolean                nolphin_file_is_mountpoint                     (NolphinFile                   *file);
GMount *                nolphin_file_get_mount                         (NolphinFile                   *file);
void                    nolphin_file_set_mount                         (NolphinFile                   *file,
                                                                     GMount                     *mount);
char *                  nolphin_file_get_volume_free_space             (NolphinFile                   *file);
char *                  nolphin_file_get_volume_name                   (NolphinFile                   *file);
char *                  nolphin_file_get_symbolic_link_target_path     (NolphinFile                   *file);
char *                  nolphin_file_get_symbolic_link_target_uri      (NolphinFile                   *file);
gboolean                nolphin_file_is_broken_symbolic_link           (NolphinFile                   *file);
gboolean                nolphin_file_is_nolphin_link                  (NolphinFile                   *file);
gboolean                nolphin_file_is_executable                     (NolphinFile                   *file);
gboolean                nolphin_file_is_directory                      (NolphinFile                   *file);
gboolean                nolphin_file_is_user_special_directory         (NolphinFile                   *file,
									 GUserDirectory                 special_directory);
gboolean		nolphin_file_is_archive			(NolphinFile			*file);
gboolean                nolphin_file_is_in_trash                       (NolphinFile                   *file);
gboolean                nolphin_file_is_in_recent                      (NolphinFile                   *file);
gboolean                nolphin_file_is_in_favorites                   (NolphinFile                   *file);
gboolean                nolphin_file_is_in_search                      (NolphinFile                   *file);
gboolean                nolphin_file_is_unavailable_favorite           (NolphinFile                   *file);
gboolean                nolphin_file_is_in_admin                       (NolphinFile                   *file);
gboolean                nolphin_file_is_in_desktop                     (NolphinFile                   *file);
gboolean		nolphin_file_is_home				(NolphinFile                   *file);
gboolean                nolphin_file_is_desktop_directory              (NolphinFile                   *file);
GError *                nolphin_file_get_file_info_error               (NolphinFile                   *file);
gboolean                nolphin_file_get_directory_item_count          (NolphinFile                   *file,
									 guint                          *count,
									 gboolean                       *count_unreadable);
void                    nolphin_file_recompute_deep_counts             (NolphinFile                   *file);
NolphinRequestStatus   nolphin_file_get_deep_counts                   (NolphinFile                   *file,
									 guint                          *directory_count,
									 guint                          *file_count,
									 guint                          *unreadable_directory_count,
                                     guint                          *hidden_count,
									 goffset                         *total_size,
									 gboolean                        force);
gboolean                nolphin_file_should_show_thumbnail             (NolphinFile                   *file);
void                    nolphin_file_delete_thumbnail                  (NolphinFile                   *file);
gboolean                nolphin_file_has_loaded_thumbnail              (NolphinFile                   *file);
gboolean                nolphin_file_should_show_directory_item_count  (NolphinFile                   *file);
gboolean                nolphin_file_should_show_type                  (NolphinFile                   *file);
GList *                 nolphin_file_get_keywords                      (NolphinFile                   *file);
void                    nolphin_file_set_keywords                      (NolphinFile                   *file,
                                                                     GList                         *keywords);
GList *                 nolphin_file_get_emblem_icons                  (NolphinFile                   *file,
                                                                     NolphinFile                   *view_file);
int                     nolphin_file_get_rating                        (NolphinFile                   *file);
void                    nolphin_file_set_rating                        (NolphinFile                   *file,
                                                                     int                            rating);
char *                  nolphin_file_get_comment                       (NolphinFile                   *file);
void                    nolphin_file_set_comment                       (NolphinFile                   *file,
                                                                     const char                    *comment);
gboolean                nolphin_file_get_directory_item_mime_types     (NolphinFile                   *file,
									 GList                         **mime_list);

void                    nolphin_file_set_attributes                    (NolphinFile                   *file, 
									 GFileInfo                      *attributes,
									 NolphinFileOperationCallback   callback,
									 gpointer                        callback_data);
GFilesystemPreviewType  nolphin_file_get_filesystem_use_preview        (NolphinFile *file);

char *                  nolphin_file_get_filesystem_id                 (NolphinFile                   *file);

NolphinFile *          nolphin_file_get_trash_original_file           (NolphinFile                   *file);

/* Permissions. */
gboolean                nolphin_file_can_get_permissions               (NolphinFile                   *file);
gboolean                nolphin_file_can_set_permissions               (NolphinFile                   *file);
guint                   nolphin_file_get_permissions                   (NolphinFile                   *file);
gboolean                nolphin_file_can_get_owner                     (NolphinFile                   *file);
gboolean                nolphin_file_can_set_owner                     (NolphinFile                   *file);
gboolean                nolphin_file_can_get_group                     (NolphinFile                   *file);
gboolean                nolphin_file_can_set_group                     (NolphinFile                   *file);
char *                  nolphin_file_get_owner_name                    (NolphinFile                   *file);
char *                  nolphin_file_get_group_name                    (NolphinFile                   *file);
GList *                 nolphin_get_user_names                         (void);
GList *                 nolphin_get_all_group_names                    (void);
GList *                 nolphin_file_get_settable_group_names          (NolphinFile                   *file);
gboolean                nolphin_file_can_get_selinux_context           (NolphinFile                   *file);
char *                  nolphin_file_get_selinux_context               (NolphinFile                   *file);

/* "Capabilities". */
gboolean                nolphin_file_can_read                          (NolphinFile                   *file);
gboolean                nolphin_file_can_write                         (NolphinFile                   *file);
gboolean                nolphin_file_can_execute                       (NolphinFile                   *file);
gboolean                nolphin_file_can_rename                        (NolphinFile                   *file);
gboolean                nolphin_file_can_delete                        (NolphinFile                   *file);
gboolean                nolphin_file_can_trash                         (NolphinFile                   *file);

gboolean                nolphin_file_can_mount                         (NolphinFile                   *file);
gboolean                nolphin_file_can_unmount                       (NolphinFile                   *file);
gboolean                nolphin_file_can_eject                         (NolphinFile                   *file);
gboolean                nolphin_file_can_start                         (NolphinFile                   *file);
gboolean                nolphin_file_can_start_degraded                (NolphinFile                   *file);
gboolean                nolphin_file_can_stop                          (NolphinFile                   *file);
GDriveStartStopType     nolphin_file_get_start_stop_type               (NolphinFile                   *file);
gboolean                nolphin_file_can_poll_for_media                (NolphinFile                   *file);
gboolean                nolphin_file_is_media_check_automatic          (NolphinFile                   *file);

void                    nolphin_file_mount                             (NolphinFile                   *file,
									 GMountOperation                *mount_op,
									 GCancellable                   *cancellable,
									 NolphinFileOperationCallback   callback,
									 gpointer                        callback_data);
void                    nolphin_file_unmount                           (NolphinFile                   *file,
									 GMountOperation                *mount_op,
									 GCancellable                   *cancellable,
									 NolphinFileOperationCallback   callback,
									 gpointer                        callback_data);
void                    nolphin_file_eject                             (NolphinFile                   *file,
									 GMountOperation                *mount_op,
									 GCancellable                   *cancellable,
									 NolphinFileOperationCallback   callback,
									 gpointer                        callback_data);

void                    nolphin_file_start                             (NolphinFile                   *file,
									 GMountOperation                *start_op,
									 GCancellable                   *cancellable,
									 NolphinFileOperationCallback   callback,
									 gpointer                        callback_data);
void                    nolphin_file_stop                              (NolphinFile                   *file,
									 GMountOperation                *mount_op,
									 GCancellable                   *cancellable,
									 NolphinFileOperationCallback   callback,
									 gpointer                        callback_data);
void                    nolphin_file_poll_for_media                    (NolphinFile                   *file);

/* Basic operations for file objects. */
void                    nolphin_file_set_owner                         (NolphinFile                   *file,
									 const char                     *user_name_or_id,
									 NolphinFileOperationCallback   callback,
									 gpointer                        callback_data);
void                    nolphin_file_set_group                         (NolphinFile                   *file,
									 const char                     *group_name_or_id,
									 NolphinFileOperationCallback   callback,
									 gpointer                        callback_data);
void                    nolphin_file_set_permissions                   (NolphinFile                   *file,
									 guint32                         permissions,
									 NolphinFileOperationCallback   callback,
									 gpointer                        callback_data);
void                    nolphin_file_rename                            (NolphinFile                   *file,
									 const char                     *new_name,
									 NolphinFileOperationCallback   callback,
									 gpointer                        callback_data);
void                    nolphin_file_cancel                            (NolphinFile                   *file,
									 NolphinFileOperationCallback   callback,
									 gpointer                        callback_data);

/* Return true if this file has already been deleted.
 * This object will be unref'd after sending the files_removed signal,
 * but it could hang around longer if someone ref'd it.
 */
gboolean                nolphin_file_is_gone                           (NolphinFile                   *file);

/* Return true if this file is not confirmed to have ever really
 * existed. This is true when the NolphinFile object has been created, but no I/O
 * has yet confirmed the existence of a file by that name.
 */
gboolean                nolphin_file_is_not_yet_confirmed              (NolphinFile                   *file);

/* Simple getting and setting top-level metadata. */
char *                  nolphin_file_get_metadata                      (NolphinFile                   *file,
									 const char                     *key,
									 const char                     *default_metadata);
GList *                 nolphin_file_get_metadata_list                 (NolphinFile                   *file,
									 const char                     *key);
void                    nolphin_file_set_metadata                      (NolphinFile                   *file,
									 const char                     *key,
									 const char                     *default_metadata,
									 const char                     *metadata);
void                    nolphin_file_set_metadata_list                 (NolphinFile                   *file,
									 const char                     *key,
									 GList                          *list);
void                    nolphin_file_set_desktop_grid_adjusts (NolphinFile   *file,
                                                            const char *key,
                                                            int         int_a,
                                                            int         int_b);
void                    nolphin_file_get_desktop_grid_adjusts (NolphinFile   *file,
                                                            const char *key,
                                                            int        *int_a,
                                                            int        *int_b);
/* Covers for common data types. */
gboolean                nolphin_file_get_boolean_metadata              (NolphinFile                   *file,
									 const char                     *key,
									 gboolean                        default_metadata);
void                    nolphin_file_set_boolean_metadata              (NolphinFile                   *file,
									 const char                     *key,
									 gboolean                        default_metadata,
									 gboolean                        metadata);
int                     nolphin_file_get_integer_metadata              (NolphinFile                   *file,
									 const char                     *key,
									 int                             default_metadata);
void                    nolphin_file_set_integer_metadata              (NolphinFile                   *file,
									 const char                     *key,
									 int                             default_metadata,
									 int                             metadata);

#define UNDEFINED_TIME ((time_t) (-1))

time_t                  nolphin_file_get_time_metadata                 (NolphinFile                  *file,
									 const char                    *key);
void                    nolphin_file_set_time_metadata                 (NolphinFile                  *file,
									 const char                    *key,
									 time_t                         time);


/* Attributes for file objects as user-displayable strings. */
char *                  nolphin_file_get_string_attribute              (NolphinFile                   *file,
									 const char                     *attribute_name);
char *                  nolphin_file_get_string_attribute_q            (NolphinFile                   *file,
									 GQuark                          attribute_q);
char *                  nolphin_file_get_string_attribute_with_default (NolphinFile                   *file,
									 const char                     *attribute_name);
char *                  nolphin_file_get_string_attribute_with_default_q (NolphinFile                  *file,
									 GQuark                          attribute_q);

/* Matching with another URI. */
gboolean                nolphin_file_matches_uri                       (NolphinFile                   *file,
									 const char                     *uri);

/* Is the file local? */
gboolean                nolphin_file_is_local                          (NolphinFile                   *file);

/* Comparing two file objects for sorting */
NolphinFileSortType    nolphin_file_get_default_sort_type             (NolphinFile                   *file,
									 gboolean                       *reversed);
const gchar *           nolphin_file_get_default_sort_attribute        (NolphinFile                   *file,
									 gboolean                       *reversed);

int                     nolphin_file_compare_for_sort                  (NolphinFile                   *file_1,
									 NolphinFile                   *file_2,
									 NolphinFileSortType            sort_type,
									 gboolean			 directories_first,
									 gboolean            favorites_first,
									 gboolean		  	 reversed,
                                     gpointer                       search_dir);
int                     nolphin_file_compare_for_sort_by_attribute     (NolphinFile                   *file_1,
									 NolphinFile                   *file_2,
									 const char                     *attribute,
									 gboolean                        directories_first,
									 gboolean                        favorites_first,
									 gboolean                        reversed,
                                     gpointer                        search_dir);
int                     nolphin_file_compare_for_sort_by_attribute_q   (NolphinFile                   *file_1,
									 NolphinFile                   *file_2,
									 GQuark                          attribute,
									 gboolean                        directories_first,
									 gboolean                        favorites_first,
									 gboolean                        reversed,
                                     gpointer                        search_dir);
gboolean                nolphin_file_is_date_sort_attribute_q          (GQuark                          attribute);
gboolean                nolphin_file_attribute_slow_sort               (const gchar                    *sort_attribute);

/* Gruppierung. @group_type restricts to the four criteria
 * supported here (name/size/type/mtime) - any other
 * NolphinFileSortType is not a valid grouping criterion and will
 * assert. nolphin_file_get_group_key() returns a newly-allocated,
 * human-readable label for the group @file belongs to (candidate
 * text for a future group header row); nolphin_file_compare_for_group()
 * gives a stable ordering between the GROUPS themselves (not the
 * files within a group), so a caller can sort by (group order, then
 * the normal sort attribute) to get a grouped, still fully sorted,
 * list. Bucket boundaries (date/size ranges) are not specified
 * elsewhere and are this implementation's own reasonable choice. */
gboolean                nolphin_file_sort_type_is_valid_group_type      (NolphinFileSortType             group_type);
char *                  nolphin_file_get_group_key                     (NolphinFile                   *file,
									 NolphinFileSortType             group_type);
int                     nolphin_file_compare_for_group                 (NolphinFile                   *file_1,
									 NolphinFile                   *file_2,
									 NolphinFileSortType             group_type);

/* AUSWAHL: "Auswahl nach Dateityp". Coarse, MIME-prefix-based
 * categories - a file can match more than one caller-relevant
 * category conceptually (e.g. a .txt is both TEXT and, loosely,
 * a "document"), so this only exposes the small set of unambiguous
 * buckets: a file's mime type either starts with "image/"/"video/"/
 * "audio/"/"text/", is a directory, is a recognized archive
 * (reuses nolphin_archive_detect_format() from nolphin-archive.h),
 * or falls into OTHER. Not an attempt at a complete freedesktop.org
 * MIME taxonomy - deliberately kept small and unambiguous. */
typedef enum {
	NOLPHIN_FILE_TYPE_CATEGORY_FOLDER,
	NOLPHIN_FILE_TYPE_CATEGORY_IMAGE,
	NOLPHIN_FILE_TYPE_CATEGORY_VIDEO,
	NOLPHIN_FILE_TYPE_CATEGORY_AUDIO,
	NOLPHIN_FILE_TYPE_CATEGORY_TEXT,
	NOLPHIN_FILE_TYPE_CATEGORY_ARCHIVE,
	NOLPHIN_FILE_TYPE_CATEGORY_OTHER
} NolphinFileTypeCategory;

const char *            nolphin_file_type_category_get_label           (NolphinFileTypeCategory         category);
gboolean                nolphin_file_matches_type_category              (NolphinFile                   *file,
									 NolphinFileTypeCategory         category);

int                     nolphin_file_compare_display_name              (NolphinFile                   *file_1,
									 const char                     *pattern);
int                     nolphin_file_compare_location                  (NolphinFile                    *file_1,
                                                                         NolphinFile                    *file_2);

/* filtering functions for use by various directory views */
gboolean                nolphin_file_is_hidden_file                    (NolphinFile                   *file);
gboolean                nolphin_file_should_show                       (NolphinFile                   *file,
									 gboolean                        show_hidden,
									 gboolean                        show_foreign);
GList                  *nolphin_file_list_filter_hidden                (GList                          *files,
									 gboolean                        show_hidden);


/* Get the URI that's used when activating the file.
 * Getting this can require reading the contents of the file.
 */
gboolean                nolphin_file_is_launcher                       (NolphinFile                   *file);
gboolean                nolphin_file_is_foreign_link                   (NolphinFile                   *file);
gboolean                nolphin_file_is_trusted_link                   (NolphinFile                   *file);
gboolean                nolphin_file_has_activation_uri                (NolphinFile                   *file);
char *                  nolphin_file_get_activation_uri                (NolphinFile                   *file);
GFile *                 nolphin_file_get_activation_location           (NolphinFile                   *file);

char *                  nolphin_file_get_drop_target_uri               (NolphinFile                   *file);

GIcon *                 nolphin_file_get_gicon                         (NolphinFile                   *file,
                                                                     NolphinFileIconFlags           flags);
gchar *                 nolphin_file_get_control_icon_name             (NolphinFile                   *file);

NolphinIconInfo *      nolphin_file_get_icon                          (NolphinFile                   *file,
									 int                             size,
                                     int                             max_width,
                                     int                             scale,
									 NolphinFileIconFlags           flags);
GdkPixbuf *             nolphin_file_get_icon_pixbuf                   (NolphinFile                   *file,
									 int                             size,
									 gboolean                        force_size,
                                     int                             scale,
									 NolphinFileIconFlags           flags);

gboolean                nolphin_file_has_open_window                   (NolphinFile                   *file);
void                    nolphin_file_set_has_open_window               (NolphinFile                   *file,
									 gboolean                        has_open_window);

/* Thumbnailing handling */
gboolean                nolphin_file_is_thumbnailing                   (NolphinFile                   *file);

/* Convenience functions for dealing with a list of NolphinFile objects that each have a ref.
 * These are just convenient names for functions that work on lists of GtkObject *.
 */
GList *                 nolphin_file_list_ref                          (GList                          *file_list);
void                    nolphin_file_list_unref                        (GList                          *file_list);
void                    nolphin_file_list_free                         (GList                          *file_list);
GList *                 nolphin_file_list_copy                         (GList                          *file_list);
GList *                 nolphin_file_list_from_uris                    (GList                          *uri_list);
GList *			nolphin_file_list_sort_by_display_name		(GList				*file_list);
void                    nolphin_file_list_call_when_ready              (GList                          *file_list,
									 NolphinFileAttributes          attributes,
									 NolphinFileListHandle        **handle,
									 NolphinFileListCallback        callback,
									 gpointer                        callback_data);
void                    nolphin_file_list_cancel_call_when_ready       (NolphinFileListHandle         *handle);

char *   nolphin_file_get_owner_as_string            (NolphinFile          *file,
                                                          gboolean           include_real_name);
char *   nolphin_file_get_type_as_string             (NolphinFile          *file);
char *   nolphin_file_get_detailed_type_as_string    (NolphinFile          *file);

gchar *  nolphin_file_construct_tooltip              (NolphinFile *file, NolphinFileTooltipFlags flags, gpointer search_dir);

gboolean nolphin_file_has_thumbnail_access_problem   (NolphinFile *file);

gint     nolphin_file_get_monitor_number             (NolphinFile *file);
void     nolphin_file_set_monitor_number             (NolphinFile *file, gint monitor);
void     nolphin_file_get_position                   (NolphinFile *file, GdkPoint *point);
void     nolphin_file_set_position                   (NolphinFile *file, gint x, gint y);
gboolean nolphin_file_get_is_desktop_orphan          (NolphinFile *file);
void     nolphin_file_set_is_desktop_orphan          (NolphinFile *file, gboolean is_desktop_orphan);

gboolean nolphin_file_get_pinning                    (NolphinFile *file);
void     nolphin_file_set_pinning                    (NolphinFile *file, gboolean  pin);
gboolean nolphin_file_get_is_favorite                (NolphinFile *file);
void     nolphin_file_set_is_favorite                (NolphinFile *file, gboolean favorite);
void     nolphin_file_set_load_deferred_attrs        (NolphinFile *file,
                                                   NolphinFileLoadDeferredAttrs load_deferred_attrs);
NolphinFileLoadDeferredAttrs nolphin_file_get_load_deferred_attrs (NolphinFile *file);

gboolean nolphin_file_add_search_result_data             (NolphinFile *file, gpointer search_dir, FileSearchResult *result);
void nolphin_file_clear_search_result_data           (NolphinFile *file, gpointer search_dir);
gboolean nolphin_file_has_search_result              (NolphinFile *file, gpointer search_dir);
gint nolphin_file_get_search_result_count            (NolphinFile *file, gpointer search_dir);
gchar *nolphin_file_get_search_result_count_as_string (NolphinFile *file, gpointer search_dir);
gchar *nolphin_file_get_search_result_snippet        (NolphinFile *file, gpointer search_dir);

/* Debugging */
void                    nolphin_file_dump                              (NolphinFile                   *file);

typedef struct NolphinFileDetails NolphinFileDetails;

struct NolphinFile {
	GObject parent_slot;
	NolphinFileDetails *details;
};

typedef struct {
	GObjectClass parent_slot;

	/* Subclasses can set this to something other than G_FILE_TYPE_UNKNOWN and
	   it will be used as the default file type. This is useful when creating
	   a "virtual" NolphinFile subclass that you can't actually get real
	   information about. For exaple NolphinDesktopDirectoryFile. */
	GFileType default_file_type; 
	
	/* Called when the file notices any change. */
	void                  (* changed)                (NolphinFile *file);

	/* Called periodically while directory deep count is being computed. */
	void                  (* updated_deep_count_in_progress) (NolphinFile *file);

	/* Virtual functions (mainly used for trash directory). */
	void                  (* monitor_add)            (NolphinFile           *file,
							  gconstpointer           client,
							  NolphinFileAttributes  attributes);
	void                  (* monitor_remove)         (NolphinFile           *file,
							  gconstpointer           client);
	void                  (* call_when_ready)        (NolphinFile           *file,
							  NolphinFileAttributes  attributes,
							  NolphinFileCallback    callback,
							  gpointer                callback_data);
	void                  (* cancel_call_when_ready) (NolphinFile           *file,
							  NolphinFileCallback    callback,
							  gpointer                callback_data);
	gboolean              (* check_if_ready)         (NolphinFile           *file,
							  NolphinFileAttributes  attributes);
	gboolean              (* get_item_count)         (NolphinFile           *file,
							  guint                  *count,
							  gboolean               *count_unreadable);
	NolphinRequestStatus (* get_deep_counts)        (NolphinFile           *file,
							  guint                  *directory_count,
							  guint                  *file_count,
							  guint                  *unreadable_directory_count,
                              guint                  *hidden_count,
							  goffset       *total_size);
	gboolean              (* get_date)               (NolphinFile           *file,
							  NolphinDateType        type,
							  time_t                 *date);
	char *                (* get_where_string)       (NolphinFile           *file);

	void                  (* set_metadata)           (NolphinFile           *file,
                                                      const char         *key,
                                                      const char         *value);
	void                  (* set_metadata_as_list)   (NolphinFile           *file,
                                                      const char         *key,
                                                      char              **value);
    gchar *               (* get_metadata)           (NolphinFile           *file,
                                                      const char         *key);
    gchar **              (* get_metadata_as_list)   (NolphinFile           *file,
                                                      const char         *key);
	void                  (* mount)                  (NolphinFile                   *file,
							  GMountOperation                *mount_op,
							  GCancellable                   *cancellable,
							  NolphinFileOperationCallback   callback,
							  gpointer                        callback_data);
	void                 (* unmount)                 (NolphinFile                   *file,
							  GMountOperation                *mount_op,
							  GCancellable                   *cancellable,
							  NolphinFileOperationCallback   callback,
							  gpointer                        callback_data);
	void                 (* eject)                   (NolphinFile                   *file,
							  GMountOperation                *mount_op,
							  GCancellable                   *cancellable,
							  NolphinFileOperationCallback   callback,
							  gpointer                        callback_data);

	void                  (* start)                  (NolphinFile                   *file,
							  GMountOperation                *start_op,
							  GCancellable                   *cancellable,
							  NolphinFileOperationCallback   callback,
							  gpointer                        callback_data);
	void                 (* stop)                    (NolphinFile                   *file,
							  GMountOperation                *mount_op,
							  GCancellable                   *cancellable,
							  NolphinFileOperationCallback   callback,
							  gpointer                        callback_data);

	void                 (* poll_for_media)          (NolphinFile                   *file);
} NolphinFileClass;

#define NOLPHIN_FILE_URI(msg,f)                                    \
    {                                                           \
        gchar *uri = nolphin_file_get_uri (NOLPHIN_FILE (f));         \
        g_message ("%s: %p - %s", msg, f, uri);                 \
        g_free (uri);                                           \
    }                                                           \

#endif /* NOLPHIN_FILE_H */
