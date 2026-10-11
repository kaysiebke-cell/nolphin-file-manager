/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nolphin-directory.h: Nolphin directory model.
 
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

#ifndef NOLPHIN_DIRECTORY_H
#define NOLPHIN_DIRECTORY_H

#include <gtk/gtk.h>
#include <gio/gio.h>
#include <libnolphin-private/nolphin-file-attributes.h>
#include <libnolphin-private/nolphin-file.h>

/* NolphinDirectory is a class that manages the model for a directory,
   real or virtual, for Nolphin, mainly the file-manager component. The directory is
   responsible for managing both real data and cached metadata. On top of
   the file system independence provided by gio, the directory
   object also provides:
  
       1) A synchronization framework, which notifies via signals as the
          set of known files changes.
       2) An abstract interface for getting attributes and performing
          operations on files.
*/

#define NOLPHIN_TYPE_DIRECTORY nolphin_directory_get_type()
#define NOLPHIN_DIRECTORY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NOLPHIN_TYPE_DIRECTORY, NolphinDirectory))
#define NOLPHIN_DIRECTORY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NOLPHIN_TYPE_DIRECTORY, NolphinDirectoryClass))
#define NOLPHIN_IS_DIRECTORY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NOLPHIN_TYPE_DIRECTORY))
#define NOLPHIN_IS_DIRECTORY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NOLPHIN_TYPE_DIRECTORY))
#define NOLPHIN_DIRECTORY_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NOLPHIN_TYPE_DIRECTORY, NolphinDirectoryClass))

/* NolphinFile is defined both here and in nolphin-file.h. */
#ifndef NOLPHIN_FILE_DEFINED
#define NOLPHIN_FILE_DEFINED
typedef struct NolphinFile NolphinFile;
#endif

typedef struct NolphinDirectoryDetails NolphinDirectoryDetails;

typedef struct
{
	GObject object;
	NolphinDirectoryDetails *details;
} NolphinDirectory;

typedef void (*NolphinDirectoryCallback) (NolphinDirectory *directory,
					   GList             *files,
					   gpointer           callback_data);

typedef struct
{
	GObjectClass parent_class;

	/*** Notification signals for clients to connect to. ***/

	/* The files_added signal is emitted as the directory model 
	 * discovers new files.
	 */
	void     (* files_added)         (NolphinDirectory          *directory,
					  GList                      *added_files);

	/* The files_changed signal is emitted as changes occur to
	 * existing files that are noticed by the synchronization framework,
	 * including when an old file has been deleted. When an old file
	 * has been deleted, this is the last chance to forget about these
	 * file objects, which are about to be unref'd. Use a call to
	 * nolphin_file_is_gone () to test for this case.
	 */
	void     (* files_changed)       (NolphinDirectory         *directory,
					  GList                     *changed_files);

	/* The done_loading signal is emitted when a directory load
	 * request completes. This is needed because, at least in the
	 * case where the directory is empty, the caller will receive
	 * no kind of notification at all when a directory load
	 * initiated by `nolphin_directory_file_monitor_add' completes.
	 */
	void     (* done_loading)        (NolphinDirectory         *directory);

	void     (* load_error)          (NolphinDirectory         *directory,
					  GError                    *error);

	/*** Virtual functions for subclasses to override. ***/
	gboolean (* contains_file)       (NolphinDirectory         *directory,
					  NolphinFile              *file);
	void     (* call_when_ready)     (NolphinDirectory         *directory,
					  NolphinFileAttributes     file_attributes,
					  gboolean                   wait_for_file_list,
					  NolphinDirectoryCallback  callback,
					  gpointer                   callback_data);
	void     (* cancel_callback)     (NolphinDirectory         *directory,
					  NolphinDirectoryCallback  callback,
					  gpointer                   callback_data);
	void     (* file_monitor_add)    (NolphinDirectory          *directory,
					  gconstpointer              client,
					  gboolean                   monitor_hidden_files,
					  NolphinFileAttributes     monitor_attributes,
					  NolphinDirectoryCallback  initial_files_callback,
					  gpointer                   callback_data);
	void     (* file_monitor_remove) (NolphinDirectory         *directory,
					  gconstpointer              client);
	void     (* force_reload)        (NolphinDirectory         *directory);
	gboolean (* are_all_files_seen)  (NolphinDirectory         *directory);
	gboolean (* is_not_empty)        (NolphinDirectory         *directory);

	/* get_file_list is a function pointer that subclasses may override to
	 * customize collecting the list of files in a directory.
	 * For example, the NolphinDesktopDirectory overrides this so that it can
	 * merge together the list of files in the $HOME/Desktop directory with
	 * the list of standard icons (Computer, Home, Trash) on the desktop.
	 */
	GList *	 (* get_file_list)	 (NolphinDirectory *directory);

	/* Should return FALSE if the directory is read-only and doesn't
	 * allow setting of metadata.
	 * An example of this is the search directory.
	 */
	gboolean (* is_editable)         (NolphinDirectory *directory);
} NolphinDirectoryClass;

/* Basic GObject requirements. */
GType              nolphin_directory_get_type                 (void);

/* Get a directory given a uri.
 * Creates the appropriate subclass given the uri mappings.
 * Returns a referenced object, not a floating one. Unref when finished.
 * If two windows are viewing the same uri, the directory object is shared.
 */
NolphinDirectory *nolphin_directory_get                      (GFile                     *location);
NolphinDirectory *nolphin_directory_get_by_uri               (const char                *uri);
NolphinDirectory *nolphin_directory_get_for_file             (NolphinFile              *file);

/* Covers for g_object_ref and g_object_unref that provide two conveniences:
 * 1) Using these is type safe.
 * 2) You are allowed to call these with NULL,
 */
NolphinDirectory *nolphin_directory_ref                      (NolphinDirectory         *directory);
void               nolphin_directory_unref                    (NolphinDirectory         *directory);

/* Access to a URI. */
char *             nolphin_directory_get_uri                  (NolphinDirectory         *directory);
GFile *            nolphin_directory_get_location             (NolphinDirectory         *directory);

/* Is this file still alive and in this directory? */
gboolean           nolphin_directory_contains_file            (NolphinDirectory         *directory,
								NolphinFile              *file);

/* Get the uri of the file in the directory, NULL if not found */
char *             nolphin_directory_get_file_uri             (NolphinDirectory         *directory,
								const char                *file_name);

/* Get (and ref) a NolphinFile object for this directory. */
NolphinFile *     nolphin_directory_get_corresponding_file   (NolphinDirectory         *directory);

/* Waiting for data that's read asynchronously.
 * The file attribute and metadata keys are for files in the directory.
 */
void               nolphin_directory_call_when_ready          (NolphinDirectory         *directory,
								NolphinFileAttributes     file_attributes,
								gboolean                   wait_for_all_files,
								NolphinDirectoryCallback  callback,
								gpointer                   callback_data);
void               nolphin_directory_cancel_callback          (NolphinDirectory         *directory,
								NolphinDirectoryCallback  callback,
								gpointer                   callback_data);


/* Monitor the files in a directory. */
void               nolphin_directory_file_monitor_add         (NolphinDirectory         *directory,
								gconstpointer              client,
								gboolean                   monitor_hidden_files,
								NolphinFileAttributes     attributes,
								NolphinDirectoryCallback  initial_files_callback,
								gpointer                   callback_data);
void               nolphin_directory_file_monitor_remove      (NolphinDirectory         *directory,
								gconstpointer              client);
void               nolphin_directory_force_reload             (NolphinDirectory         *directory);

/* Get a list of all files currently known in the directory. */
GList *            nolphin_directory_get_file_list            (NolphinDirectory         *directory);

GList *            nolphin_directory_match_pattern            (NolphinDirectory         *directory,
							        const char *glob);

/* "Auswahl nach Dateityp" - see NolphinFileTypeCategory in
 * nolphin-file.h for what counts as a match. */
GList *            nolphin_directory_match_type_category       (NolphinDirectory         *directory,
							        NolphinFileTypeCategory    category);


/* Return true if the directory has information about all the files.
 * This will be false until the directory has been read at least once.
 */
gboolean           nolphin_directory_are_all_files_seen       (NolphinDirectory         *directory);

/* Return true if the directory is local. */
gboolean           nolphin_directory_is_local                 (NolphinDirectory         *directory);

gboolean           nolphin_directory_is_in_trash              (NolphinDirectory         *directory);
gboolean           nolphin_directory_is_in_recent             (NolphinDirectory         *directory);
gboolean           nolphin_directory_is_in_favorites          (NolphinDirectory         *directory);
gboolean           nolphin_directory_is_in_admin              (NolphinDirectory         *directory);
gboolean           nolphin_directory_is_in_search             (NolphinDirectory         *directory);
/* Return false if directory contains anything besides a Nolphin metafile.
 * Only valid if directory is monitored. Used by the Trash monitor.
 */
gboolean           nolphin_directory_is_not_empty             (NolphinDirectory         *directory);

/* Convenience functions for dealing with a list of NolphinDirectory objects that each have a ref.
 * These are just convenient names for functions that work on lists of GtkObject *.
 */
GList *            nolphin_directory_list_ref                 (GList                     *directory_list);
void               nolphin_directory_list_unref               (GList                     *directory_list);
void               nolphin_directory_list_free                (GList                     *directory_list);
GList *            nolphin_directory_list_copy                (GList                     *directory_list);
GList *            nolphin_directory_list_sort_by_uri         (GList                     *directory_list);

/* Fast way to check if a directory is the desktop directory */
gboolean           nolphin_directory_is_desktop_directory     (NolphinDirectory         *directory);

gboolean           nolphin_directory_is_editable              (NolphinDirectory         *directory);

void               nolphin_directory_set_show_thumbnails      (NolphinDirectory         *directory,
                                gboolean show_thumbnails);

#endif /* NOLPHIN_DIRECTORY_H */
