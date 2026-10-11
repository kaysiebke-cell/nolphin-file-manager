/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nolphin-directory-private.h: Nolphin directory model.
 
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

#include <gio/gio.h>
#include <eel/eel-vfs-extensions.h>
#include <libnolphin-private/nolphin-directory.h>
#include <libnolphin-private/nolphin-file-queue.h>
#include <libnolphin-private/nolphin-file.h>
#include <libnolphin-private/nolphin-monitor.h>
#include <libnolphin-extension/nolphin-info-provider.h>

typedef struct LinkInfoReadState LinkInfoReadState;
typedef struct FileMonitors FileMonitors;
typedef struct DirectoryLoadState DirectoryLoadState;
typedef struct DirectoryCountState DirectoryCountState;
typedef struct DeepCountState DeepCountState;
typedef struct GetInfoState GetInfoState;
typedef struct NewFilesState NewFilesState;
typedef struct MimeListState MimeListState;
typedef struct ThumbnailState ThumbnailState;
typedef struct MountState MountState;
typedef struct FilesystemInfoState FilesystemInfoState;
typedef struct FavoriteCheckState FavoriteCheckState;

typedef enum {
	REQUEST_LINK_INFO,
	REQUEST_DEEP_COUNT,
	REQUEST_DIRECTORY_COUNT,
	REQUEST_FILE_INFO,
	REQUEST_FILE_LIST, /* always FALSE if file != NULL */
	REQUEST_MIME_LIST,
	REQUEST_EXTENSION_INFO,
	REQUEST_THUMBNAIL,
	REQUEST_MOUNT,
	REQUEST_FILESYSTEM_INFO,
    REQUEST_FAVORITE_CHECK,
	REQUEST_TYPE_LAST
} RequestType;

/* A request for information about one or more files. */
typedef guint32 Request;
typedef gint32 RequestCounter[REQUEST_TYPE_LAST];

#define REQUEST_WANTS_TYPE(request, type) ((request) & (1<<(type)))
#define REQUEST_SET_TYPE(request, type) (request) |= (1<<(type))

struct NolphinDirectoryDetails
{
	/* The location. */
	GFile *location;

	/* The file objects. */
	NolphinFile *as_file;
	GList *file_list;
	GHashTable *file_hash;

	/* Queues of files needing some I/O done. */
	NolphinFileQueue *high_priority_queue;
	NolphinFileQueue *low_priority_queue;
	NolphinFileQueue *extension_queue;

	/* These lists are going to be pretty short.  If we think they
	 * are going to get big, we can use hash tables instead.
	 */
	GList *call_when_ready_list;
	RequestCounter call_when_ready_counters;
	GList *monitor_list;
	RequestCounter monitor_counters;
	guint call_ready_idle_id;

	NolphinMonitor *monitor;
	gulong 		 mime_db_monitor;

	gboolean in_async_service_loop;
	gboolean state_changed;

	gboolean file_list_monitored;
	gboolean directory_loaded;
	gboolean directory_loaded_sent_notification;
	DirectoryLoadState *directory_load_in_progress;

	GList *pending_file_info; /* list of GnomeVFSFileInfo's that are pending */
	int confirmed_file_count;
        guint dequeue_pending_idle_id;

	GList *new_files_in_progress; /* list of NewFilesState * */

	/* List of GFile's that received CHANGE events while new files were being added in
	 * that same folder. We will process this CHANGE events after new_files_in_progress
	 * list is finished. See Bug 703179 for a case when this happens. */
	GList *new_files_in_progress_changes;

	DirectoryCountState *count_in_progress;

	NolphinFile *deep_count_file;
	DeepCountState *deep_count_in_progress;

	MimeListState *mime_list_in_progress;

	NolphinFile *get_info_file;
	GetInfoState *get_info_in_progress;

    NolphinFile *favorite_check_file;
    FavoriteCheckState *favorite_check_in_progress;
    guint favorite_check_idle_id;

	NolphinFile *extension_info_file;
	NolphinInfoProvider *extension_info_provider;
	NolphinOperationHandle *extension_info_in_progress;
	guint extension_info_idle;
    GClosure * extension_info_closure;

	ThumbnailState *thumbnail_state;

	MountState *mount_state;

	FilesystemInfoState *filesystem_info_state;

	LinkInfoReadState *link_info_read_state;

	GList *file_operations_in_progress; /* list of FileOperation * */

    gint max_deferred_file_count;
    gint early_load_file_count;
};

NolphinDirectory *nolphin_directory_get_existing                    (GFile                     *location);

/* async. interface */
void               nolphin_directory_async_state_changed             (NolphinDirectory         *directory);
void               nolphin_directory_call_when_ready_internal        (NolphinDirectory         *directory,
								       NolphinFile              *file,
								       NolphinFileAttributes     file_attributes,
								       gboolean                   wait_for_file_list,
								       NolphinDirectoryCallback  directory_callback,
								       NolphinFileCallback       file_callback,
								       gpointer                   callback_data);
gboolean           nolphin_directory_check_if_ready_internal         (NolphinDirectory         *directory,
								       NolphinFile              *file,
								       NolphinFileAttributes     file_attributes);
void               nolphin_directory_cancel_callback_internal        (NolphinDirectory         *directory,
								       NolphinFile              *file,
								       NolphinDirectoryCallback  directory_callback,
								       NolphinFileCallback       file_callback,
								       gpointer                   callback_data);
void               nolphin_directory_monitor_add_internal            (NolphinDirectory         *directory,
								       NolphinFile              *file,
								       gconstpointer              client,
								       gboolean                   monitor_hidden_files,
								       NolphinFileAttributes     attributes,
								       NolphinDirectoryCallback  callback,
								       gpointer                   callback_data);
void               nolphin_directory_monitor_remove_internal         (NolphinDirectory         *directory,
								       NolphinFile              *file,
								       gconstpointer              client);
void               nolphin_directory_get_info_for_new_files          (NolphinDirectory         *directory,
								       GList                     *vfs_uris);
NolphinFile *     nolphin_directory_get_existing_corresponding_file (NolphinDirectory         *directory);
void               nolphin_directory_invalidate_count_and_mime_list  (NolphinDirectory         *directory);
gboolean           nolphin_directory_is_file_list_monitored          (NolphinDirectory         *directory);
gboolean           nolphin_directory_is_anyone_monitoring_file_list  (NolphinDirectory         *directory);
gboolean           nolphin_directory_has_active_request_for_file     (NolphinDirectory         *directory,
								       NolphinFile              *file);
void               nolphin_directory_remove_file_monitor_link        (NolphinDirectory         *directory,
								       GList                     *link);
void               nolphin_directory_schedule_dequeue_pending        (NolphinDirectory         *directory);
void               nolphin_directory_stop_monitoring_file_list       (NolphinDirectory         *directory);
void               nolphin_directory_cancel                          (NolphinDirectory         *directory);
void               nolphin_async_destroying_file                     (NolphinFile              *file);
void               nolphin_directory_force_reload_internal           (NolphinDirectory         *directory,
								       NolphinFileAttributes     file_attributes);
void               nolphin_directory_cancel_loading_file_attributes  (NolphinDirectory         *directory,
								       NolphinFile              *file,
								       NolphinFileAttributes     file_attributes);

/* Calls shared between directory, file, and async. code. */
void               nolphin_directory_emit_files_added                (NolphinDirectory         *directory,
								       GList                     *added_files);
void               nolphin_directory_emit_files_changed              (NolphinDirectory         *directory,
								       GList                     *changed_files);
void               nolphin_directory_emit_change_signals             (NolphinDirectory         *directory,
								       GList                     *changed_files);
void               emit_change_signals_for_all_files		      (NolphinDirectory	 *directory);
void               emit_change_signals_for_all_files_in_all_directories (void);
void               nolphin_directory_emit_done_loading               (NolphinDirectory         *directory);
void               nolphin_directory_emit_load_error                 (NolphinDirectory         *directory,
								       GError                    *error);
NolphinDirectory *nolphin_directory_get_internal                    (GFile                     *location,
								       gboolean                   create);
char *             nolphin_directory_get_name_for_self_as_new_file   (NolphinDirectory         *directory);
Request            nolphin_directory_set_up_request                  (NolphinFileAttributes     file_attributes);

/* Interface to the file list. */
NolphinFile *     nolphin_directory_find_file_by_name               (NolphinDirectory         *directory,
								       const char                *filename);
NolphinFile *     nolphin_directory_find_file_by_internal_filename  (NolphinDirectory         *directory,
								       const char                *internal_filename);

void               nolphin_directory_remove_file                     (NolphinDirectory         *directory,
								       NolphinFile              *file);
FileMonitors *     nolphin_directory_remove_file_monitors            (NolphinDirectory         *directory,
								       NolphinFile              *file);
void               nolphin_directory_add_file_monitors               (NolphinDirectory         *directory,
								       NolphinFile              *file,
								       FileMonitors              *monitors);
void               nolphin_directory_add_file                        (NolphinDirectory         *directory,
								       NolphinFile              *file);
GList *            nolphin_directory_begin_file_name_change          (NolphinDirectory         *directory,
								       NolphinFile              *file);
void               nolphin_directory_end_file_name_change            (NolphinDirectory         *directory,
								       NolphinFile              *file,
								       GList                     *node);
void               nolphin_directory_moved                           (const char                *from_uri,
								       const char                *to_uri);
/* Interface to the work queue. */

void               nolphin_directory_add_file_to_work_queue          (NolphinDirectory *directory,
								       NolphinFile *file);
void               nolphin_directory_remove_file_from_work_queue     (NolphinDirectory *directory,
								       NolphinFile *file);


/* debugging functions */
int                nolphin_directory_number_outstanding              (void);
