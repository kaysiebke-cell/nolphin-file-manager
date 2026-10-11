/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nolphin-file-private.h:
 
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

#ifndef NOLPHIN_FILE_PRIVATE_H
#define NOLPHIN_FILE_PRIVATE_H

#include <libnolphin-private/nolphin-directory.h>
#include <libnolphin-private/nolphin-file.h>
#include <libnolphin-private/nolphin-monitor.h>
#include <libnolphin-private/nolphin-file-undo-operations.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-string.h>

#define NOLPHIN_FILE_DEFAULT_ATTRIBUTES				\
	"standard::*,access::*,mountable::*,time::*,unix::*,owner::*,selinux::*,thumbnail::*,id::filesystem,trash::orig-path,trash::deletion-date,metadata::*,preview::icon"

/* These are in the typical sort order. Known things come first, then
 * things where we can't know, finally things where we don't yet know.
 */
typedef enum {
	KNOWN,
	UNKNOWABLE,
	UNKNOWN
} Knowledge;

typedef enum {
    FILE_META_STATE_INIT = -1,
    FILE_META_STATE_FALSE = 0,
    FILE_META_STATE_TRUE = 1,
} NolphinFileMetaState;

struct NolphinFileDetails
{
	NolphinDirectory *directory;
	
	GRefString *name;

	/* File info: */
	GFileType type;

	GRefString *display_name;
	char *display_name_collation_key;
	GRefString *edit_name;

	/* One-entry cache: the collation key for the attribute last sorted by, and
	 * the attribute it was built from. Invalidated when the info is cleared or
	 * updated, and on every change notification - the latter is what covers
	 * extension-supplied attributes and moves between directories. */
	GQuark sort_collation_attribute;
	char *sort_collation_key;

	goffset size; /* -1 is unknown */
	
	int sort_order;
	
	guint32 permissions;
	int uid; /* -1 is none */
	int gid; /* -1 is none */

	GRefString *owner;
	GRefString *owner_real;
	GRefString *group;
	
	time_t atime; /* 0 is unknown */
	time_t mtime; /* 0 is unknown */
	time_t ctime; /* 0 is unknown */
    time_t btime; /* 0 is unknown */
	
	char *symlink_name;
	
	GRefString *mime_type;
	
	char *selinux_context;
	char *description;
	
	GError *get_info_error;
	
	guint directory_count;

	guint deep_directory_count;
	guint deep_file_count;
	guint deep_unreadable_count;
    guint deep_hidden_count;
	goffset deep_size;

	GIcon *icon;

	char *thumbnail_path;
    eel_boolean_bit thumbnail_access_problem : 1;
	GdkPixbuf *thumbnail;
	time_t thumbnail_mtime;
    gint thumbnail_throttle_count;
    time_t last_thumbnail_try_mtime;

	GList *mime_list; /* If this is a directory, the list of MIME types in it. */

    GHashTable *search_results;

	/* Info you might get from a link (.desktop, .directory or nolphin link) */
	GIcon *custom_icon;
	char *activation_uri;

	/* used during DND, for checking whether source and destination are on
	 * the same file system.
	 */
	GRefString *filesystem_id;

	char *trash_orig_path;

	/* The following is for file operations in progress. Since
	 * there are normally only a few of these, we can move them to
	 * a separate hash table or something if required to keep the
	 * file objects small.
	 */
	GList *operations_in_progress;

	/* NolphinInfoProviders that need to be run for this file */
	GList *pending_info_providers;

	/* Emblems provided by extensions */
	GList *extension_emblems;
	GList *pending_extension_emblems;

	/* Attributes provided by extensions */
	GHashTable *extension_attributes;
	GHashTable *pending_extension_attributes;

	GHashTable *metadata;

	/* Mount for mountpoint or the references GMount for a "mountable" */
	GMount *mount;
	
	/* boolean fields: bitfield to save space, since there can be
           many NolphinFile objects. */

	eel_boolean_bit unconfirmed                   : 1;
	eel_boolean_bit is_gone                       : 1;
	/* Set when emitting files_added on the directory to make sure we
	   add a file, and only once */
	eel_boolean_bit is_added                      : 1;
	/* Set by the NolphinDirectory while it's loading the file
	 * list so the file knows not to do redundant I/O.
	 */
	eel_boolean_bit loading_directory             : 1;
	eel_boolean_bit got_file_info                 : 1;
	eel_boolean_bit get_info_failed               : 1;
	eel_boolean_bit file_info_is_up_to_date       : 1;
	
	eel_boolean_bit got_directory_count           : 1;
	eel_boolean_bit directory_count_failed        : 1;
	eel_boolean_bit directory_count_is_up_to_date : 1;

	eel_boolean_bit deep_counts_status      : 2; /* NolphinRequestStatus */
	/* no deep_counts_are_up_to_date field; since we expose
           intermediate values for this attribute, we do actually
           forget it rather than invalidating. */

	eel_boolean_bit got_mime_list                 : 1;
	eel_boolean_bit mime_list_failed              : 1;
	eel_boolean_bit mime_list_is_up_to_date       : 1;

	eel_boolean_bit mount_is_up_to_date           : 1;

	eel_boolean_bit got_link_info                 : 1;
	eel_boolean_bit link_info_is_up_to_date       : 1;
	eel_boolean_bit got_custom_display_name       : 1;
	eel_boolean_bit got_custom_activation_uri     : 1;

    eel_boolean_bit has_preview_icon              : 1;
	eel_boolean_bit thumbnail_is_up_to_date       : 1;
	eel_boolean_bit thumbnail_wants_original      : 1;
	eel_boolean_bit thumbnail_tried_original      : 1;
	eel_boolean_bit thumbnailing_failed           : 1;
	
	eel_boolean_bit is_thumbnailing               : 1;

    eel_boolean_bit is_desktop_orphan             : 1;

	/* TRUE if the file is open in a spatial window */
	eel_boolean_bit has_open_window               : 1;

	eel_boolean_bit is_launcher                   : 1;
	eel_boolean_bit is_trusted_link               : 1;
	eel_boolean_bit is_foreign_link               : 1;
	eel_boolean_bit is_symlink                    : 1;
	eel_boolean_bit is_mountpoint                 : 1;
	eel_boolean_bit is_hidden                     : 1;

    eel_boolean_bit favorite_checked              : 1;

	eel_boolean_bit has_permissions               : 1;
	
	eel_boolean_bit can_read                      : 1;
	eel_boolean_bit can_write                     : 1;
	eel_boolean_bit can_execute                   : 1;
	eel_boolean_bit can_delete                    : 1;
	eel_boolean_bit can_trash                     : 1;
	eel_boolean_bit can_rename                    : 1;
	eel_boolean_bit can_mount                     : 1;
	eel_boolean_bit can_unmount                   : 1;
	eel_boolean_bit can_eject                     : 1;
	eel_boolean_bit can_start                     : 1;
	eel_boolean_bit can_start_degraded            : 1;
	eel_boolean_bit can_stop                      : 1;
	eel_boolean_bit start_stop_type               : 3; /* GDriveStartStopType */
	eel_boolean_bit can_poll_for_media            : 1;
	eel_boolean_bit is_media_check_automatic      : 1;

	eel_boolean_bit filesystem_readonly           : 1;
	eel_boolean_bit filesystem_use_preview        : 2; /* GFilesystemPreviewType */
    eel_boolean_bit filesystem_info_is_up_to_date : 1;

    NolphinFileLoadDeferredAttrs load_deferred_attrs;
    NolphinFileMetaState pinning;
    NolphinFileMetaState favorite;

	time_t trash_time; /* 0 is unknown */

	guint64 free_space; /* (guint)-1 for unknown */
	time_t free_space_read; /* The time free_space was updated, or 0 for never */

    gint desktop_monitor;
    gint cached_position_x;
    gint cached_position_y;
};

typedef struct {
	NolphinFile *file;
	GCancellable *cancellable;
	NolphinFileOperationCallback callback;
	gpointer callback_data;
	gboolean is_rename;
	
	gpointer data;
	GDestroyNotify free_data;
	NolphinFileUndoInfo *undo_info;
} NolphinFileOperation;

NolphinFile *nolphin_file_new_from_info                  (NolphinDirectory      *directory,
							    GFileInfo              *info);
void          nolphin_file_emit_changed                   (NolphinFile           *file);
void          nolphin_file_mark_gone                      (NolphinFile           *file);

void          nolphin_file_set_directory                  (NolphinFile           *file,
							    NolphinDirectory      *directory);
gboolean      nolphin_file_get_date                       (NolphinFile           *file,
							    NolphinDateType        date_type,
							    time_t                 *date);
void          nolphin_file_updated_deep_count_in_progress (NolphinFile           *file);


void          nolphin_file_clear_info                     (NolphinFile           *file);
/* Compare file's state with a fresh file info struct, return FALSE if
 * no change, update file and return TRUE if the file info contains
 * new state.  */
gboolean      nolphin_file_update_info                    (NolphinFile           *file,
							    GFileInfo              *info);
gboolean      nolphin_file_update_name                    (NolphinFile           *file,
							    const char             *name);
gboolean      nolphin_file_update_metadata_from_info      (NolphinFile           *file,
							    GFileInfo              *info);

gboolean      nolphin_file_update_name_and_directory      (NolphinFile           *file,
							    const char             *name,
							    NolphinDirectory      *directory);

gboolean      nolphin_file_set_display_name               (NolphinFile           *file,
							    const char             *display_name,
							    const char             *edit_name,
							    gboolean                custom);

/* Mark specified attributes for this file out of date without canceling current
 * I/O or kicking off new I/O.
 */
void                   nolphin_file_invalidate_attributes_internal     (NolphinFile           *file,
									 NolphinFileAttributes  file_attributes);
NolphinFileAttributes nolphin_file_get_all_attributes                 (void);
gboolean               nolphin_file_is_self_owned                      (NolphinFile           *file);
void                   nolphin_file_invalidate_count_and_mime_list     (NolphinFile           *file);
gboolean               nolphin_file_rename_in_progress                 (NolphinFile           *file);
void                   nolphin_file_invalidate_extension_info_internal (NolphinFile           *file);
void                   nolphin_file_info_providers_done                (NolphinFile           *file);


/* Thumbnailing: */
void          nolphin_file_set_is_thumbnailing            (NolphinFile           *file,
							    gboolean                is_thumbnailing);

NolphinFileOperation *nolphin_file_operation_new      (NolphinFile                  *file,
							 NolphinFileOperationCallback  callback,
							 gpointer                       callback_data);
void                   nolphin_file_operation_free     (NolphinFileOperation         *op);
void                   nolphin_file_operation_complete (NolphinFileOperation         *op,
							 GFile                         *result_location,
							 GError                        *error);
void                   nolphin_file_operation_cancel   (NolphinFileOperation         *op);
#endif
