/* -*- Mode: C; indent-tabs-mode: f; c-basic-offset: 4; tab-width: 4 -*-

   nolphin-file.c: Nolphin file model.

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

#include <config.h>
#include "nolphin-file.h"

#include "nolphin-directory-notify.h"
#include "nolphin-directory-private.h"
#include "nolphin-signaller.h"
#include "nolphin-desktop-directory.h"
#include "nolphin-desktop-directory-file.h"
#include "nolphin-desktop-icon-file.h"
#include "nolphin-file-attributes.h"
#include "nolphin-file-private.h"
#include "nolphin-archive.h"
#include "nolphin-file-operations.h"
#include "nolphin-file-utilities.h"
#include "nolphin-global-preferences.h"
#include "nolphin-icon-names.h"
#include "nolphin-lib-self-check-functions.h"
#include "nolphin-link.h"
#include "nolphin-metadata.h"
#include "nolphin-module.h"
#include "nolphin-search-directory.h"
#include "nolphin-search-engine.h"
#include "nolphin-search-directory-file.h"
#include "nolphin-thumbnails.h"
#include "nolphin-trash-monitor.h"
#include "nolphin-vfs-file.h"
#include "nolphin-file-undo-operations.h"
#include "nolphin-file-undo-manager.h"
#include <eel/eel-debug.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-vfs-extensions.h>
#include <eel/eel-string.h>
#include <grp.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include <glib.h>
#include <libnolphin-extension/nolphin-file-info.h>
#include <libnolphin-extension/nolphin-extension-private.h>
#include <libxapp/xapp-favorites.h>
#include <pwd.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

#ifdef HAVE_SELINUX
#include <selinux/selinux.h>
#endif

#define DEBUG_FLAG NOLPHIN_DEBUG_FILE
#include <libnolphin-private/nolphin-debug.h>
#include <libnolphin-private/nolphin-git-overlay.h>

/* Time in seconds to cache getpwuid results */
#define GETPWUID_CACHE_TIME (5*60)

#define ICON_NAME_THUMBNAIL_LOADING   "image-loading"

#undef NOLPHIN_FILE_DEBUG_REF
#undef NOLPHIN_FILE_DEBUG_REF_VALGRIND

#ifdef NOLPHIN_FILE_DEBUG_REF_VALGRIND
#include <valgrind/valgrind.h>
#define DEBUG_REF_PRINTF VALGRIND_PRINTF_BACKTRACE
#else
#define DEBUG_REF_PRINTF printf
#endif

/* Files that start with these characters sort after files that don't. */
#define SORT_LAST_CHAR1 '.'
#define SORT_LAST_CHAR2 '#'

/* Name of Nolphin trash directories */
#define TRASH_DIRECTORY_NAME ".Trash"

#define METADATA_ID_IS_LIST_MASK (1<<31)

#define MAX_THUMBNAIL_TRIES 2

typedef enum {
	SHOW_HIDDEN = 1 << 0,
} FilterOptions;

typedef enum {
	NOLPHIN_DATE_FORMAT_REGULAR = 0,
	NOLPHIN_DATE_FORMAT_REGULAR_WITH_TIME = 1,
	NOLPHIN_DATE_FORMAT_FULL = 2,
} NolphinDateCompactFormat;

typedef void (* ModifyListFunction) (GList **list, NolphinFile *file);

enum {
	CHANGED,
	UPDATED_DEEP_COUNT_IN_PROGRESS,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static GHashTable *symbolic_links;

static GQuark attribute_name_q,
	attribute_size_q,
	attribute_type_q,
	attribute_detailed_type_q,
	attribute_modification_date_q,
	attribute_date_modified_q,
	attribute_date_modified_full_q,
	attribute_date_modified_with_time_q,
	attribute_accessed_date_q,
	attribute_date_accessed_q,
	attribute_date_accessed_full_q,
    attribute_creation_date_q,
    attribute_date_created_q,
    attribute_date_created_full_q,
    attribute_date_created_with_time_q,
	attribute_mime_type_q,
	attribute_size_detail_q,
	attribute_deep_size_q,
	attribute_deep_file_count_q,
	attribute_deep_directory_count_q,
	attribute_deep_total_count_q,
	attribute_date_changed_q,
	attribute_date_changed_full_q,
	attribute_trashed_on_q,
	attribute_trashed_on_full_q,
	attribute_trash_orig_path_q,
	attribute_date_permissions_q,
	attribute_date_permissions_full_q,
	attribute_permissions_q,
	attribute_selinux_context_q,
	attribute_octal_permissions_q,
	attribute_owner_q,
	attribute_group_q,
	attribute_uri_q,
	attribute_where_q,
	attribute_link_target_q,
	attribute_volume_q,
	attribute_free_space_q,
	attribute_extension_q,
	attribute_search_result_snippet_q,
	attribute_search_result_count_q;

static void     nolphin_file_info_iface_init                (NolphinFileInfoInterface *iface);

static gboolean update_info_and_name                         (NolphinFile          *file,
							      GFileInfo             *info);
static const char * nolphin_file_peek_display_name (NolphinFile *file);
static const char * nolphin_file_peek_display_name_collation_key (NolphinFile *file);
static const char * nolphin_file_peek_sort_collation_key (NolphinFile *file, GQuark attribute);
static void nolphin_file_invalidate_sort_collation_key (NolphinFile *file);
static void file_mount_unmounted (GMount *mount,  gpointer data);
static void metadata_hash_free (GHashTable *hash);
static void invalidate_thumbnail (NolphinFile *file);

G_DEFINE_TYPE_WITH_CODE (NolphinFile, nolphin_file, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (NOLPHIN_TYPE_FILE_INFO,
						nolphin_file_info_iface_init));

static void
nolphin_file_init (NolphinFile *file)
{
	file->details = G_TYPE_INSTANCE_GET_PRIVATE ((file), NOLPHIN_TYPE_FILE, NolphinFileDetails);

    file->details->desktop_monitor = -1;
    file->details->cached_position_x = -1;
    file->details->cached_position_y = -1;
    file->details->pinning = FILE_META_STATE_INIT;
    file->details->favorite = FILE_META_STATE_INIT;
    file->details->load_deferred_attrs = NOLPHIN_FILE_LOAD_DEFERRED_ATTRS_NO;

	nolphin_file_clear_info (file);
	nolphin_file_invalidate_extension_info_internal (file);

	file->details->free_space = -1;
}

static GObject*
nolphin_file_constructor (GType                  type,
			   guint                  n_construct_properties,
			   GObjectConstructParam *construct_params)
{
  GObject *object;
  NolphinFile *file;

  object = (* G_OBJECT_CLASS (nolphin_file_parent_class)->constructor) (type,
									 n_construct_properties,
									 construct_params);

  file = NOLPHIN_FILE (object);

  /* Set to default type after full construction */
  if (NOLPHIN_FILE_GET_CLASS (file)->default_file_type != G_FILE_TYPE_UNKNOWN) {
	  file->details->type = NOLPHIN_FILE_GET_CLASS (file)->default_file_type;
  }

  return object;
}

gboolean
nolphin_file_set_display_name (NolphinFile *file,
				const char *display_name,
				const char *edit_name,
				gboolean custom)
{
	gboolean changed;

	if (custom && display_name == NULL) {
		/* We're re-setting a custom display name, invalidate it if
		   we already set it so that the old one is re-read */
		if (file->details->got_custom_display_name) {
			file->details->got_custom_display_name = FALSE;
			nolphin_file_invalidate_attributes (file,
							     NOLPHIN_FILE_ATTRIBUTE_INFO);
		}
		return FALSE;
	}

	if (display_name == NULL || *display_name == 0) {
		return FALSE;
	}

	if (!custom && file->details->got_custom_display_name) {
		return FALSE;
	}

	if (edit_name == NULL) {
		edit_name = display_name;
	}

	changed = FALSE;

	if (g_strcmp0 (file->details->display_name, display_name) != 0) {
		changed = TRUE;

        g_clear_pointer (&file->details->display_name, g_ref_string_release);

		if (g_strcmp0 (file->details->name, display_name) == 0) {
			file->details->display_name = g_ref_string_acquire (file->details->name);
		} else {
			file->details->display_name = g_ref_string_new (display_name);
		}

		g_free (file->details->display_name_collation_key);
		file->details->display_name_collation_key = g_utf8_collate_key_for_filename (display_name, -1);
	}

	if (g_strcmp0 (file->details->edit_name, edit_name) != 0) {
		changed = TRUE;

        g_clear_pointer (&file->details->edit_name, g_ref_string_release);
		if (g_strcmp0 (file->details->display_name, edit_name) == 0) {
			file->details->edit_name = g_ref_string_acquire (file->details->display_name);
		} else {
			file->details->edit_name = g_ref_string_new (edit_name);
		}
	}

	file->details->got_custom_display_name = custom;
	return changed;
}

static void
nolphin_file_clear_display_name (NolphinFile *file)
{
    g_clear_pointer (&file->details->display_name, g_ref_string_release);
    g_clear_pointer (&file->details->display_name_collation_key, g_free);
    g_clear_pointer (&file->details->edit_name, g_ref_string_release);
}

static gboolean
foreach_metadata_free (gpointer  key,
		       gpointer  value,
		       gpointer  user_data)
{
	guint id;

	id = GPOINTER_TO_UINT (key);

	if (id & METADATA_ID_IS_LIST_MASK) {
		g_strfreev ((char **)value);
	} else {
		g_free ((char *)value);
	}
	return TRUE;
}


static void
metadata_hash_free (GHashTable *hash)
{
	g_hash_table_foreach_remove (hash,
				     foreach_metadata_free,
				     NULL);
	g_hash_table_destroy (hash);
}

static gboolean
metadata_hash_equal (GHashTable *hash1,
		     GHashTable *hash2)
{
	GHashTableIter iter;
	gpointer key1, value1, value2;
	guint id;

	if (hash1 == NULL && hash2 == NULL) {
		return TRUE;
	}

	if (hash1 == NULL || hash2 == NULL) {
		return FALSE;
	}

	if (g_hash_table_size (hash1) !=
	    g_hash_table_size (hash2)) {
		return FALSE;
	}

	g_hash_table_iter_init (&iter, hash1);
	while (g_hash_table_iter_next (&iter, &key1, &value1)) {
		value2 = g_hash_table_lookup (hash2, key1);
		if (value2 == NULL) {
			return FALSE;
		}
		id = GPOINTER_TO_UINT (key1);
		if (id & METADATA_ID_IS_LIST_MASK) {
			if (!eel_g_strv_equal ((char **)value1, (char **)value2)) {
				return FALSE;
			}
		} else {
			if (strcmp ((char *)value1, (char *)value2) != 0) {
				return FALSE;
			}
		}
	}

	return TRUE;
}

static void
clear_metadata (NolphinFile *file)
{
	if (file->details->metadata) {
		metadata_hash_free (file->details->metadata);
		file->details->metadata = NULL;
	}
}

static GHashTable *
get_metadata_from_info (GFileInfo *info)
{
	GHashTable *metadata;
	char **attrs;
	guint id;
	int i;
	GFileAttributeType type;
	gpointer value;

	attrs = g_file_info_list_attributes (info, "metadata");

	metadata = g_hash_table_new (NULL, NULL);

	for (i = 0; attrs[i] != NULL; i++) {
		id = nolphin_metadata_get_id (attrs[i] + strlen ("metadata::"));
		if (id == 0) {
			continue;
		}

		if (!g_file_info_get_attribute_data (info, attrs[i],
						     &type, &value, NULL)) {
			continue;
		}

		if (type == G_FILE_ATTRIBUTE_TYPE_STRING) {
			g_hash_table_insert (metadata, GUINT_TO_POINTER (id),
					     g_strdup ((char *)value));
		} else if (type == G_FILE_ATTRIBUTE_TYPE_STRINGV) {
			id |= METADATA_ID_IS_LIST_MASK;
			g_hash_table_insert (metadata, GUINT_TO_POINTER (id),
					     g_strdupv ((char **)value));
		}
	}

	g_strfreev (attrs);

	return metadata;
}

gboolean
nolphin_file_update_metadata_from_info (NolphinFile *file,
					 GFileInfo *info)
{
	gboolean changed = FALSE;

	if (g_file_info_has_namespace (info, "metadata")) {
		GHashTable *metadata;

		metadata = get_metadata_from_info (info);
		if (!metadata_hash_equal (metadata,
					  file->details->metadata)) {
			changed = TRUE;
			clear_metadata (file);
			file->details->metadata = metadata;
		} else {
			metadata_hash_free (metadata);
		}
	} else if (file->details->metadata) {
		changed = TRUE;
		clear_metadata (file);
	}

	return changed;
}

void
nolphin_file_clear_info (NolphinFile *file)
{
	file->details->got_file_info = FALSE;
	if (file->details->get_info_error) {
		g_error_free (file->details->get_info_error);
		file->details->get_info_error = NULL;
	}
	/* Reset to default type, which might be other than unknown for
	   special kinds of files like the desktop or a search directory */
	file->details->type = NOLPHIN_FILE_GET_CLASS (file)->default_file_type;

	if (!file->details->got_custom_display_name) {
		nolphin_file_clear_display_name (file);
	}

	if (!file->details->got_custom_activation_uri &&
	    file->details->activation_uri != NULL) {
		g_free (file->details->activation_uri);
		file->details->activation_uri = NULL;
	}

	if (file->details->icon != NULL) {
		g_object_unref (file->details->icon);
		file->details->icon = NULL;
	}

    g_clear_object (&file->details->thumbnail);

	g_free (file->details->thumbnail_path);
	file->details->thumbnail_path = NULL;
	file->details->thumbnailing_failed = FALSE;
    file->details->last_thumbnail_try_mtime = 0;

	file->details->is_launcher = FALSE;
	file->details->is_foreign_link = FALSE;
	file->details->is_trusted_link = FALSE;
	file->details->is_symlink = FALSE;
	file->details->is_hidden = FALSE;
	file->details->is_mountpoint = FALSE;
	file->details->uid = -1;
	file->details->gid = -1;
	file->details->can_read = TRUE;
	file->details->can_write = TRUE;
	file->details->can_execute = TRUE;
	file->details->can_delete = TRUE;
	file->details->can_trash = TRUE;
	file->details->can_rename = TRUE;
	file->details->can_mount = FALSE;
	file->details->can_unmount = FALSE;
	file->details->can_eject = FALSE;
	file->details->can_start = FALSE;
	file->details->can_start_degraded = FALSE;
	file->details->can_stop = FALSE;
	file->details->start_stop_type = G_DRIVE_START_STOP_TYPE_UNKNOWN;
	file->details->can_poll_for_media = FALSE;
	file->details->is_media_check_automatic = FALSE;
	file->details->has_permissions = FALSE;
	file->details->permissions = 0;
	file->details->size = -1;
	file->details->sort_order = 0;
	file->details->mtime = 0;
	file->details->atime = 0;
	file->details->ctime = 0;
    file->details->btime = 0;
	file->details->trash_time = 0;
    file->details->load_deferred_attrs = NOLPHIN_FILE_LOAD_DEFERRED_ATTRS_NO;
	g_free (file->details->symlink_name);
	file->details->symlink_name = NULL;
    g_clear_pointer (&file->details->mime_type, g_ref_string_release);
    g_clear_pointer (&file->details->selinux_context, g_free);
    g_clear_pointer (&file->details->description, g_free);
    g_clear_pointer (&file->details->owner, g_ref_string_release);
    g_clear_pointer (&file->details->owner_real, g_ref_string_release);
    g_clear_pointer (&file->details->group, g_ref_string_release);
    g_clear_pointer (&file->details->filesystem_id, g_ref_string_release);

    nolphin_file_invalidate_sort_collation_key (file);

    file->details->is_desktop_orphan = FALSE;

    file->details->desktop_monitor = -1;

	clear_metadata (file);
}

static NolphinFile *
nolphin_file_new_from_filename (NolphinDirectory *directory,
				 const char *filename,
				 gboolean self_owned)
{
	NolphinFile *file;

	g_assert (NOLPHIN_IS_DIRECTORY (directory));
	g_assert (filename != NULL);
	g_assert (filename[0] != '\0');

	if (NOLPHIN_IS_DESKTOP_DIRECTORY (directory)) {
		if (self_owned) {
			file = NOLPHIN_FILE (g_object_new (NOLPHIN_TYPE_DESKTOP_DIRECTORY_FILE, NULL));
		} else {
			/* This doesn't normally happen, unless the user somehow types in a uri
			 * that references a file like this. (See #349840) */
			file = NOLPHIN_FILE (g_object_new (NOLPHIN_TYPE_VFS_FILE, NULL));
		}
	} else if (NOLPHIN_IS_SEARCH_DIRECTORY (directory)) {
		if (self_owned) {
			file = NOLPHIN_FILE (g_object_new (NOLPHIN_TYPE_SEARCH_DIRECTORY_FILE, NULL));
		} else {
			/* This doesn't normally happen, unless the user somehow types in a uri
			 * that references a file like this. (See #349840) */
			file = NOLPHIN_FILE (g_object_new (NOLPHIN_TYPE_VFS_FILE, NULL));
		}
	} else {
		file = NOLPHIN_FILE (g_object_new (NOLPHIN_TYPE_VFS_FILE, NULL));
	}

	file->details->directory = nolphin_directory_ref (directory);

	file->details->name = g_ref_string_new (filename);

#ifdef NOLPHIN_FILE_DEBUG_REF
	DEBUG_REF_PRINTF("%10p ref'd\n", file);
#endif

	return file;
}

static void
modify_link_hash_table (NolphinFile *file,
			ModifyListFunction modify_function)
{
	char *target_uri;
	gboolean found;
	gpointer original_key;
	GList **list_ptr;

	/* Check if there is a symlink name. If none, we are OK. */
	if (file->details->symlink_name == NULL) {
		return;
	}

	/* Create the hash table first time through. */
	if (symbolic_links == NULL) {
		symbolic_links = g_hash_table_new (g_str_hash, g_str_equal);
	}

	target_uri = nolphin_file_get_symbolic_link_target_uri (file);
	g_return_if_fail (target_uri != NULL);

	/* Find the old contents of the hash table. */
	found = g_hash_table_lookup_extended
		(symbolic_links, target_uri,
		 &original_key, (gpointer *)&list_ptr);
	if (!found) {
		list_ptr = g_new0 (GList *, 1);
		original_key = g_strdup (target_uri);
		g_hash_table_insert (symbolic_links, original_key, list_ptr);
	}
	(* modify_function) (list_ptr, file);
	if (*list_ptr == NULL) {
		g_hash_table_remove (symbolic_links, target_uri);
		g_free (list_ptr);
		g_free (original_key);
	}
	g_free (target_uri);
}

static void
symbolic_link_weak_notify (gpointer      data,
			   GObject      *where_the_object_was)
{
	GList **list = data;
	/* This really shouldn't happen, but we're seeing some strange things in
	   bug #358172 where the symlink hashtable isn't correctly updated. */
	*list = g_list_remove (*list, where_the_object_was);
}

static void
add_to_link_hash_table_list (GList **list, NolphinFile *file)
{
	if (g_list_find (*list, file) != NULL) {
		g_warning ("Adding file to symlink_table multiple times. "
			   "Please add feedback of what you were doing at http://bugzilla.gnome.org/show_bug.cgi?id=358172\n");
		return;
	}
	g_object_weak_ref (G_OBJECT (file), symbolic_link_weak_notify, list);
	*list = g_list_prepend (*list, file);
}

static void
add_to_link_hash_table (NolphinFile *file)
{
	modify_link_hash_table (file, add_to_link_hash_table_list);
}

static void
remove_from_link_hash_table_list (GList **list, NolphinFile *file)
{
	if (g_list_find (*list, file) != NULL) {
		g_object_weak_unref (G_OBJECT (file), symbolic_link_weak_notify, list);
		*list = g_list_remove (*list, file);
	}
}

static void
remove_from_link_hash_table (NolphinFile *file)
{
	modify_link_hash_table (file, remove_from_link_hash_table_list);
}

NolphinFile *
nolphin_file_new_from_info (NolphinDirectory *directory,
			     GFileInfo *info)
{
	NolphinFile *file;

	g_return_val_if_fail (NOLPHIN_IS_DIRECTORY (directory), NULL);
	g_return_val_if_fail (info != NULL, NULL);

	file = NOLPHIN_FILE (g_object_new (NOLPHIN_TYPE_VFS_FILE, NULL));

	file->details->directory = nolphin_directory_ref (directory);

	update_info_and_name (file, info);

#ifdef NOLPHIN_FILE_DEBUG_REF
	DEBUG_REF_PRINTF("%10p ref'd\n", file);
#endif

	return file;
}

static NolphinFile *
nolphin_file_get_internal (GFile *location, gboolean create)
{
	gboolean self_owned;
	NolphinDirectory *directory;
	NolphinFile *file;
	GFile *parent;
	char *basename;

	g_assert (location != NULL);

	parent = g_file_get_parent (location);

	self_owned = FALSE;
	if (parent == NULL) {
		self_owned = TRUE;
		parent = g_object_ref (location);
	}

	/* Get object that represents the directory. */
	directory = nolphin_directory_get_internal (parent, create);

	g_object_unref (parent);

	/* Get the name for the file. */
	if (self_owned && directory != NULL) {
		basename = nolphin_directory_get_name_for_self_as_new_file (directory);
	} else {
		basename = g_file_get_basename (location);
	}
	/* Check to see if it's a file that's already known. */
	if (directory == NULL) {
		file = NULL;
	} else if (self_owned) {
		file = directory->details->as_file;
	} else {
		file = nolphin_directory_find_file_by_name (directory, basename);
	}

	/* Ref or create the file. */
	if (file != NULL) {
		nolphin_file_ref (file);
	} else if (create && directory != NULL) {
		file = nolphin_file_new_from_filename (directory, basename, self_owned);
		if (self_owned) {
			g_assert (directory->details->as_file == NULL);
			directory->details->as_file = file;
		} else {
			nolphin_directory_add_file (directory, file);
		}
	}

	g_free (basename);
	nolphin_directory_unref (directory);

	return file;
}

NolphinFile *
nolphin_file_get (GFile *location)
{
	return nolphin_file_get_internal (location, TRUE);
}

NolphinFile *
nolphin_file_get_existing (GFile *location)
{
	return nolphin_file_get_internal (location, FALSE);
}

NolphinFile *
nolphin_file_get_existing_by_uri (const char *uri)
{
	GFile *location;
	NolphinFile *file;

	location = g_file_new_for_uri (uri);
	file = nolphin_file_get_internal (location, FALSE);
	g_object_unref (location);

	return file;
}

NolphinFile *
nolphin_file_get_by_uri (const char *uri)
{
	GFile *location;
	NolphinFile *file;

	location = g_file_new_for_uri (uri);
	file = nolphin_file_get_internal (location, TRUE);
	g_object_unref (location);

	return file;
}

gboolean
nolphin_file_is_self_owned (NolphinFile *file)
{
	return file->details->directory->details->as_file == file;
}

static void
finalize (GObject *object)
{
	NolphinDirectory *directory;
	NolphinFile *file;
	char *uri;

	file = NOLPHIN_FILE (object);

#ifdef NOLPHIN_FILE_DEBUG_REF
    NOLPHIN_FILE_URI ("finalize: ", file);
#endif

	g_assert (file->details->operations_in_progress == NULL);

	if (file->details->is_thumbnailing) {
		uri = nolphin_file_get_uri (file);
		nolphin_thumbnail_remove_from_queue (uri);
		g_free (uri);
	}

	nolphin_async_destroying_file (file);

	remove_from_link_hash_table (file);

	directory = file->details->directory;

	if (nolphin_file_is_self_owned (file)) {
		directory->details->as_file = NULL;
	} else {
		if (!file->details->is_gone) {
			nolphin_directory_remove_file (directory, file);
		}
	}

	if (file->details->get_info_error) {
		g_error_free (file->details->get_info_error);
	}

	nolphin_directory_unref (directory);
	g_clear_pointer (&file->details->name, g_ref_string_release);
	g_clear_pointer (&file->details->display_name, g_ref_string_release);
	g_free (file->details->display_name_collation_key);
	g_free (file->details->sort_collation_key);
	g_clear_pointer (&file->details->edit_name, g_ref_string_release);
	if (file->details->icon) {
		g_object_unref (file->details->icon);
	}
	g_free (file->details->thumbnail_path);
	g_free (file->details->symlink_name);
	g_clear_pointer (&file->details->mime_type, g_ref_string_release);
	g_clear_pointer (&file->details->owner, g_ref_string_release);
	g_clear_pointer (&file->details->owner_real, g_ref_string_release);
	g_clear_pointer (&file->details->group, g_ref_string_release);
	g_free (file->details->selinux_context);
	g_free (file->details->description);
	g_free (file->details->activation_uri);
	g_clear_object (&file->details->custom_icon);

    g_clear_object (&file->details->thumbnail);

	if (file->details->mount) {
		g_signal_handlers_disconnect_by_func (file->details->mount, file_mount_unmounted, file);
		g_object_unref (file->details->mount);
	}

	g_clear_pointer (&file->details->filesystem_id, g_ref_string_release);
	g_free (file->details->trash_orig_path);

	g_list_free_full (file->details->mime_list, g_free);
	g_list_free_full (file->details->pending_extension_emblems, g_free);
	g_list_free_full (file->details->extension_emblems, g_free);
	g_list_free_full (file->details->pending_info_providers, g_object_unref);

	if (file->details->pending_extension_attributes) {
		g_hash_table_destroy (file->details->pending_extension_attributes);
	}

	if (file->details->extension_attributes) {
		g_hash_table_destroy (file->details->extension_attributes);
	}

	if (file->details->metadata) {
		metadata_hash_free (file->details->metadata);
	}

	G_OBJECT_CLASS (nolphin_file_parent_class)->finalize (object);
}

NolphinFile *
nolphin_file_ref (NolphinFile *file)
{
	if (file == NULL) {
		return NULL;
	}
	g_return_val_if_fail (NOLPHIN_IS_FILE (file), NULL);

#ifdef NOLPHIN_FILE_DEBUG_REF
	DEBUG_REF_PRINTF("%10p ref'd\n", file);
#endif

	return g_object_ref (file);
}

void
nolphin_file_unref (NolphinFile *file)
{
	if (file == NULL) {
		return;
	}

	g_return_if_fail (NOLPHIN_IS_FILE (file));

#ifdef NOLPHIN_FILE_DEBUG_REF
	DEBUG_REF_PRINTF("%10p unref'd\n", file);
#endif

	g_object_unref (file);
}

/**
 * nolphin_file_get_parent_uri_for_display:
 *
 * Get the uri for the parent directory.
 *
 * @file: The file in question.
 *
 * Return value: A string representing the parent's location,
 * formatted for user display (including stripping "file://").
 * If the parent is NULL, returns the empty string.
 */
char *
nolphin_file_get_parent_uri_for_display (NolphinFile *file)
{
	GFile *parent;
	char *result;

	g_assert (NOLPHIN_IS_FILE (file));

	parent = nolphin_file_get_parent_location (file);
	if (parent) {
		result = g_file_get_parse_name (parent);
		g_object_unref (parent);
	} else {
		result = g_strdup ("");
	}

	return result;
}

/**
 * nolphin_file_get_parent_uri:
 *
 * Get the uri for the parent directory.
 *
 * @file: The file in question.
 *
 * Return value: A string for the parent's location, in "raw URI" form.
 * Use nolphin_file_get_parent_uri_for_display instead if the
 * result is to be displayed on-screen.
 * If the parent is NULL, returns the empty string.
 */
char *
nolphin_file_get_parent_uri (NolphinFile *file)
{
	g_assert (NOLPHIN_IS_FILE (file));

	if (nolphin_file_is_self_owned (file)) {
		/* Callers expect an empty string, not a NULL. */
		return g_strdup ("");
	}

	return nolphin_directory_get_uri (file->details->directory);
}

GFile *
nolphin_file_get_parent_location (NolphinFile *file)
{
	g_assert (NOLPHIN_IS_FILE (file));

	if (nolphin_file_is_self_owned (file)) {
		/* Callers expect an empty string, not a NULL. */
		return NULL;
	}

	return nolphin_directory_get_location (file->details->directory);
}

NolphinFile *
nolphin_file_get_parent (NolphinFile *file)
{
	g_assert (NOLPHIN_IS_FILE (file));

	if (nolphin_file_is_self_owned (file)) {
		return NULL;
	}

	return nolphin_directory_get_corresponding_file (file->details->directory);
}

/**
 * nolphin_file_can_read:
 *
 * Check whether the user is allowed to read the contents of this file.
 *
 * @file: The file to check.
 *
 * Return value: FALSE if the user is definitely not allowed to read
 * the contents of the file. If the user has read permission, or
 * the code can't tell whether the user has read permission,
 * returns TRUE (so failures must always be handled).
 */
gboolean
nolphin_file_can_read (NolphinFile *file)
{
	g_return_val_if_fail (NOLPHIN_IS_FILE (file), FALSE);

	return file->details->can_read;
}

/**
 * nolphin_file_can_write:
 *
 * Check whether the user is allowed to write to this file.
 *
 * @file: The file to check.
 *
 * Return value: FALSE if the user is definitely not allowed to write
 * to the file. If the user has write permission, or
 * the code can't tell whether the user has write permission,
 * returns TRUE (so failures must always be handled).
 */
gboolean
nolphin_file_can_write (NolphinFile *file)
{
	g_return_val_if_fail (NOLPHIN_IS_FILE (file), FALSE);

	return file->details->can_write;
}

/**
 * nolphin_file_can_execute:
 *
 * Check whether the user is allowed to execute this file.
 *
 * @file: The file to check.
 *
 * Return value: FALSE if the user is definitely not allowed to execute
 * the file. If the user has execute permission, or
 * the code can't tell whether the user has execute permission,
 * returns TRUE (so failures must always be handled).
 */
gboolean
nolphin_file_can_execute (NolphinFile *file)
{
	g_return_val_if_fail (NOLPHIN_IS_FILE (file), FALSE);

	return file->details->can_execute;
}

gboolean
nolphin_file_can_mount (NolphinFile *file)
{
	g_return_val_if_fail (NOLPHIN_IS_FILE (file), FALSE);

	return file->details->can_mount;
}

gboolean
nolphin_file_can_unmount (NolphinFile *file)
{
	g_return_val_if_fail (NOLPHIN_IS_FILE (file), FALSE);

	if (getenv("LTSP_CLIENT")) {
		return FALSE;
	}

	return file->details->can_unmount ||
		(file->details->mount != NULL &&
		 g_mount_can_unmount (file->details->mount));
}

gboolean
nolphin_file_can_eject (NolphinFile *file)
{
	g_return_val_if_fail (NOLPHIN_IS_FILE (file), FALSE);

	if (getenv("LTSP_CLIENT")) {
		return FALSE;
	}

	return file->details->can_eject ||
		(file->details->mount != NULL &&
		 g_mount_can_eject (file->details->mount));
}

gboolean
nolphin_file_can_start (NolphinFile *file)
{
	gboolean ret;
	GDrive *drive;

	g_return_val_if_fail (NOLPHIN_IS_FILE (file), FALSE);

	ret = FALSE;

	if (file->details->can_start) {
		ret = TRUE;
		goto out;
	}

	if (file->details->mount != NULL) {
		drive = g_mount_get_drive (file->details->mount);
		if (drive != NULL) {
			ret = g_drive_can_start (drive);
			g_object_unref (drive);
		}
	}

 out:
	return ret;
}

gboolean
nolphin_file_can_start_degraded (NolphinFile *file)
{
	gboolean ret;
	GDrive *drive;

	g_return_val_if_fail (NOLPHIN_IS_FILE (file), FALSE);

	ret = FALSE;

	if (file->details->can_start_degraded) {
		ret = TRUE;
		goto out;
	}

	if (file->details->mount != NULL) {
		drive = g_mount_get_drive (file->details->mount);
		if (drive != NULL) {
			ret = g_drive_can_start_degraded (drive);
			g_object_unref (drive);
		}
	}

 out:
	return ret;
}

gboolean
nolphin_file_can_poll_for_media (NolphinFile *file)
{
	gboolean ret;
	GDrive *drive;

	g_return_val_if_fail (NOLPHIN_IS_FILE (file), FALSE);

	ret = FALSE;

	if (file->details->can_poll_for_media) {
		ret = TRUE;
		goto out;
	}

	if (file->details->mount != NULL) {
		drive = g_mount_get_drive (file->details->mount);
		if (drive != NULL) {
			ret = g_drive_can_poll_for_media (drive);
			g_object_unref (drive);
		}
	}

 out:
	return ret;
}

gboolean
nolphin_file_is_media_check_automatic (NolphinFile *file)
{
	gboolean ret;
	GDrive *drive;

	g_return_val_if_fail (NOLPHIN_IS_FILE (file), FALSE);

	ret = FALSE;

	if (file->details->is_media_check_automatic) {
		ret = TRUE;
		goto out;
	}

	if (file->details->mount != NULL) {
		drive = g_mount_get_drive (file->details->mount);
		if (drive != NULL) {
			ret = g_drive_is_media_check_automatic (drive);
			g_object_unref (drive);
		}
	}

 out:
	return ret;
}


gboolean
nolphin_file_can_stop (NolphinFile *file)
{
	gboolean ret;
	GDrive *drive;

	g_return_val_if_fail (NOLPHIN_IS_FILE (file), FALSE);

	ret = FALSE;

	if (file->details->can_stop) {
		ret = TRUE;
		goto out;
	}

	if (file->details->mount != NULL) {
		drive = g_mount_get_drive (file->details->mount);
		if (drive != NULL) {
			ret = g_drive_can_stop (drive);
			g_object_unref (drive);
		}
	}

 out:
	return ret;
}

GDriveStartStopType
nolphin_file_get_start_stop_type (NolphinFile *file)
{
	GDriveStartStopType ret;
	GDrive *drive;

	g_return_val_if_fail (NOLPHIN_IS_FILE (file), FALSE);

	ret = file->details->start_stop_type;
	if (ret != G_DRIVE_START_STOP_TYPE_UNKNOWN)
		goto out;

	if (file->details->mount != NULL) {
		drive = g_mount_get_drive (file->details->mount);
		if (drive != NULL) {
			ret = g_drive_get_start_stop_type (drive);
			g_object_unref (drive);
		}
	}

 out:
	return ret;
}

void
nolphin_file_mount (NolphinFile                   *file,
		     GMountOperation                *mount_op,
		     GCancellable                   *cancellable,
		     NolphinFileOperationCallback   callback,
		     gpointer                        callback_data)
{
	GError *error;

	if (NOLPHIN_FILE_GET_CLASS (file)->mount == NULL) {
		if (callback) {
			error = NULL;
			g_set_error_literal (&error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                                             _("Diese Datei kann nicht eingehängt werden"));
			callback (file, NULL, error, callback_data);
			g_error_free (error);
		}
	} else {
		NOLPHIN_FILE_GET_CLASS (file)->mount (file, mount_op, cancellable, callback, callback_data);
	}
}

typedef struct {
	NolphinFile *file;
	NolphinFileOperationCallback callback;
	gpointer callback_data;
} UnmountData;

static void
unmount_done (void *callback_data)
{
	UnmountData *data;

	data = (UnmountData *)callback_data;
	if (data->callback) {
		data->callback (data->file, NULL, NULL, data->callback_data);
	}
	nolphin_file_unref (data->file);
	g_free (data);
}

void
nolphin_file_unmount (NolphinFile                   *file,
		       GMountOperation                *mount_op,
		       GCancellable                   *cancellable,
		       NolphinFileOperationCallback   callback,
		       gpointer                        callback_data)
{
	GError *error;
	UnmountData *data;

	if (file->details->can_unmount) {
		if (NOLPHIN_FILE_GET_CLASS (file)->unmount != NULL) {
			NOLPHIN_FILE_GET_CLASS (file)->unmount (file, mount_op, cancellable, callback, callback_data);
		} else {
			if (callback) {
				error = NULL;
				g_set_error_literal (&error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
						     _("Diese Datei kann nicht ausgehängt werden"));
				callback (file, NULL, error, callback_data);
				g_error_free (error);
			}
		}
	} else if (file->details->mount != NULL &&
		   g_mount_can_unmount (file->details->mount)) {
		data = g_new0 (UnmountData, 1);
		data->file = nolphin_file_ref (file);
		data->callback = callback;
		data->callback_data = callback_data;
		nolphin_file_operations_unmount_mount_full (NULL, file->details->mount, NULL, FALSE, TRUE, unmount_done, data);
	} else if (callback) {
		callback (file, NULL, NULL, callback_data);
	}
}

void
nolphin_file_eject (NolphinFile                   *file,
		     GMountOperation                *mount_op,
		     GCancellable                   *cancellable,
		     NolphinFileOperationCallback   callback,
		     gpointer                        callback_data)
{
	GError *error;
	UnmountData *data;

	if (file->details->can_eject) {
		if (NOLPHIN_FILE_GET_CLASS (file)->eject != NULL) {
			NOLPHIN_FILE_GET_CLASS (file)->eject (file, mount_op, cancellable, callback, callback_data);
		} else {
			if (callback) {
				error = NULL;
				g_set_error_literal (&error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
						     _("Diese Datei kann nicht ausgeworfen werden"));
				callback (file, NULL, error, callback_data);
				g_error_free (error);
			}
		}
	} else if (file->details->mount != NULL &&
		   g_mount_can_eject (file->details->mount)) {
		data = g_new0 (UnmountData, 1);
		data->file = nolphin_file_ref (file);
		data->callback = callback;
		data->callback_data = callback_data;
		nolphin_file_operations_unmount_mount_full (NULL, file->details->mount, NULL, TRUE, TRUE, unmount_done, data);
	} else if (callback) {
		callback (file, NULL, NULL, callback_data);
	}
}

void
nolphin_file_start (NolphinFile                   *file,
		     GMountOperation                *start_op,
		     GCancellable                   *cancellable,
		     NolphinFileOperationCallback   callback,
		     gpointer                        callback_data)
{
	GError *error;

	if ((file->details->can_start || file->details->can_start_degraded) &&
	    NOLPHIN_FILE_GET_CLASS (file)->start != NULL) {
		NOLPHIN_FILE_GET_CLASS (file)->start (file, start_op, cancellable, callback, callback_data);
	} else {
		if (callback) {
			error = NULL;
			g_set_error_literal (&error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                                             _("Diese Datei kann nicht gestartet werden"));
			callback (file, NULL, error, callback_data);
			g_error_free (error);
		}
	}
}

static void
file_stop_callback (GObject *source_object,
		    GAsyncResult *res,
		    gpointer callback_data)
{
	NolphinFileOperation *op;
	gboolean stopped;
	GError *error;

	op = callback_data;

	error = NULL;
	stopped = g_drive_stop_finish (G_DRIVE (source_object),
				       res, &error);

	if (!stopped &&
	    error->domain == G_IO_ERROR &&
	    (error->code == G_IO_ERROR_FAILED_HANDLED ||
	     error->code == G_IO_ERROR_CANCELLED)) {
		g_error_free (error);
		error = NULL;
	}

	nolphin_file_operation_complete (op, NULL, error);
	if (error) {
		g_error_free (error);
	}
}

void
nolphin_file_stop (NolphinFile                   *file,
		    GMountOperation                *mount_op,
		    GCancellable                   *cancellable,
		    NolphinFileOperationCallback   callback,
		    gpointer                        callback_data)
{
	GError *error;

	if (NOLPHIN_FILE_GET_CLASS (file)->stop != NULL) {
		if (file->details->can_stop) {
			NOLPHIN_FILE_GET_CLASS (file)->stop (file, mount_op, cancellable, callback, callback_data);
		} else {
			if (callback) {
				error = NULL;
				g_set_error_literal (&error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
						     _("Diese Datei kann nicht angehalten werden"));
				callback (file, NULL, error, callback_data);
				g_error_free (error);
			}
		}
	} else {
		GDrive *drive;

		drive = NULL;
		if (file->details->mount != NULL)
			drive = g_mount_get_drive (file->details->mount);

		if (drive != NULL && g_drive_can_stop (drive)) {
			NolphinFileOperation *op;

			op = nolphin_file_operation_new (file, callback, callback_data);
			if (cancellable) {
				g_object_unref (op->cancellable);
				op->cancellable = g_object_ref (cancellable);
			}

			g_drive_stop (drive,
				      G_MOUNT_UNMOUNT_NONE,
				      mount_op,
				      op->cancellable,
				      file_stop_callback,
				      op);
		} else {
			if (callback) {
				error = NULL;
				g_set_error_literal (&error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
						     _("Diese Datei kann nicht angehalten werden"));
				callback (file, NULL, error, callback_data);
				g_error_free (error);
			}
		}

		if (drive != NULL) {
			g_object_unref (drive);
		}
	}
}

void
nolphin_file_poll_for_media (NolphinFile *file)
{
	if (file->details->can_poll_for_media) {
		if (NOLPHIN_FILE_GET_CLASS (file)->stop != NULL) {
			NOLPHIN_FILE_GET_CLASS (file)->poll_for_media (file);
		}
	} else if (file->details->mount != NULL) {
		GDrive *drive;
		drive = g_mount_get_drive (file->details->mount);
		if (drive != NULL) {
			g_drive_poll_for_media (drive,
						NULL,  /* cancellable */
						NULL,  /* GAsyncReadyCallback */
						NULL); /* user_data */
			g_object_unref (drive);
		}
	}
}

/**
 * nolphin_file_is_desktop_directory:
 *
 * Check whether this file is the desktop directory.
 *
 * @file: The file to check.
 *
 * Return value: TRUE if this is the physical desktop directory.
 */
gboolean
nolphin_file_is_desktop_directory (NolphinFile *file)
{
	GFile *dir;

	dir = file->details->directory->details->location;

	if (dir == NULL) {
		return FALSE;
	}

	return nolphin_is_desktop_directory_file (dir, file->details->name);
}

static gboolean
is_desktop_file (NolphinFile *file)
{
	return nolphin_file_is_mime_type (file, "application/x-desktop");
}

static gboolean
can_rename_desktop_file (NolphinFile *file)
{
	GFile *location;
	gboolean res;

	location = nolphin_file_get_location (file);
	res = g_file_is_native (location);
	g_object_unref (location);
	return res;
}

/**
 * nolphin_file_can_rename:
 *
 * Check whether the user is allowed to change the name of the file.
 *
 * @file: The file to check.
 *
 * Return value: FALSE if the user is definitely not allowed to change
 * the name of the file. If the user is allowed to change the name, or
 * the code can't tell whether the user is allowed to change the name,
 * returns TRUE (so rename failures must always be handled).
 */
gboolean
nolphin_file_can_rename (NolphinFile *file)
{
	gboolean can_rename;

	g_return_val_if_fail (NOLPHIN_IS_FILE (file), FALSE);

	/* Nonexistent files can't be renamed. */
	if (nolphin_file_is_gone (file)) {
		return FALSE;
	}

	/* Self-owned files can't be renamed */
	if (nolphin_file_is_self_owned (file)) {
		return FALSE;
	}

	if ((is_desktop_file (file) && !can_rename_desktop_file (file)) ||
	     nolphin_file_is_home (file)) {
		return FALSE;
	}

	can_rename = TRUE;

	/* Certain types of links can't be renamed */
	if (NOLPHIN_IS_DESKTOP_ICON_FILE (file)) {
		NolphinDesktopLink *link;

		link = nolphin_desktop_icon_file_get_link (NOLPHIN_DESKTOP_ICON_FILE (file));

		if (link != NULL) {
			can_rename = nolphin_desktop_link_can_rename (link);
			g_object_unref (link);
		}
	}

	if (!can_rename) {
		return FALSE;
	}

	return file->details->can_rename;
}

gboolean
nolphin_file_can_delete (NolphinFile *file)
{
	g_return_val_if_fail (NOLPHIN_IS_FILE (file), FALSE);

	/* Nonexistent files can't be deleted. */
	if (nolphin_file_is_gone (file)) {
		return FALSE;
	}

	/* Self-owned files can't be deleted */
	if (nolphin_file_is_self_owned (file)) {
		return FALSE;
	}

	return file->details->can_delete;
}

gboolean
nolphin_file_can_trash (NolphinFile *file)
{
	g_return_val_if_fail (NOLPHIN_IS_FILE (file), FALSE);

	/* Nonexistent files can't be deleted. */
	if (nolphin_file_is_gone (file)) {
		return FALSE;
	}

	/* Self-owned files can't be deleted */
	if (nolphin_file_is_self_owned (file)) {
		return FALSE;
	}

	return file->details->can_trash;
}

GFile *
nolphin_file_get_location (NolphinFile *file)
{
	GFile *dir;

	g_return_val_if_fail (NOLPHIN_IS_FILE (file), NULL);

	dir = file->details->directory->details->location;

	if (nolphin_file_is_self_owned (file)) {
		return g_object_ref (dir);
	}

	return g_file_get_child (dir, file->details->name);
}

/* Return the actual uri associated with the passed-in file. */
char *
nolphin_file_get_uri (NolphinFile *file)
{
	char *uri;
	GFile *loc;

	g_return_val_if_fail (NOLPHIN_IS_FILE (file), NULL);

	loc = nolphin_file_get_location (file);
	uri = g_file_get_uri (loc);
	g_object_unref (loc);

	return uri;
}


/* Return the local uri associated with the passed-in file.
 * If the local uri can't be resolved, the uri from nolphin_file_get_uri
 * is returned instead.
 */
char *
nolphin_file_get_local_uri (NolphinFile *file)
{
	char *uri, *path;
	GFile *loc;

	g_return_val_if_fail (NOLPHIN_IS_FILE (file), NULL);

	if (NOLPHIN_IS_DESKTOP_ICON_FILE (file)) {
		return nolphin_file_get_uri (file);
	}

	loc = nolphin_file_get_location (file);
	path = g_file_get_path (loc);
	g_object_unref (loc);

	if (path == NULL) {
		if (file->details->activation_uri != NULL) {
			return g_strdup (file->details->activation_uri);
		}
		return nolphin_file_get_uri (file);
	}

	uri = g_filename_to_uri (path, NULL, NULL);
	g_free (path);

	return uri;
}

/* Return the actual path associated with the passed-in file. */
char *
nolphin_file_get_path (NolphinFile *file)
{
    char *path;
    GFile *loc;

    g_return_val_if_fail (NOLPHIN_IS_FILE (file), NULL);

    loc = nolphin_file_get_location (file);
    path = g_file_get_path (loc);
    g_object_unref (loc);

    return path;
}

char *
nolphin_file_get_uri_scheme (NolphinFile *file)
{
	GFile *loc;
	char *scheme;

	g_return_val_if_fail (NOLPHIN_IS_FILE (file), NULL);

	if (file->details->directory == NULL ||
	    file->details->directory->details->location == NULL) {
		return NULL;
	}

	loc = nolphin_directory_get_location (file->details->directory);
	scheme = g_file_get_uri_scheme (loc);
	g_object_unref (loc);

	return scheme;
}

gboolean
nolphin_file_has_uri_scheme (NolphinFile    *file,
                          const gchar *scheme)
{
    gchar *file_uri_scheme;
    gboolean has;

    g_return_val_if_fail (NOLPHIN_IS_FILE (file), FALSE);

    file_uri_scheme = nolphin_file_get_uri_scheme (file);
    has = g_strcmp0 (scheme, file_uri_scheme) == 0;
    g_free (file_uri_scheme);

    return has;
}

NolphinFileOperation *
nolphin_file_operation_new (NolphinFile *file,
			     NolphinFileOperationCallback callback,
			     gpointer callback_data)
{
	NolphinFileOperation *op;

	op = g_new0 (NolphinFileOperation, 1);
	op->file = nolphin_file_ref (file);
	op->callback = callback;
	op->callback_data = callback_data;
	op->cancellable = g_cancellable_new ();

	op->file->details->operations_in_progress = g_list_prepend
		(op->file->details->operations_in_progress, op);

	return op;
}

static void
nolphin_file_operation_remove (NolphinFileOperation *op)
{
	op->file->details->operations_in_progress = g_list_remove
		(op->file->details->operations_in_progress, op);
}

void
nolphin_file_operation_free (NolphinFileOperation *op)
{
	nolphin_file_operation_remove (op);
	nolphin_file_unref (op->file);
	g_object_unref (op->cancellable);
	if (op->free_data) {
		op->free_data (op->data);
	}

	if (op->undo_info != NULL) {
		nolphin_file_undo_manager_set_action (op->undo_info);
	}

	g_free (op);
}

void
nolphin_file_operation_complete (NolphinFileOperation *op, GFile *result_file, GError *error)
{
	/* Claim that something changed even if the operation failed.
	 * This makes it easier for some clients who see the "reverting"
	 * as "changing back".
	 */
	nolphin_file_operation_remove (op);
	nolphin_file_changed (op->file);
	if (op->callback) {
		(* op->callback) (op->file, result_file, error, op->callback_data);
	}
	nolphin_file_operation_free (op);
}

void
nolphin_file_operation_cancel (NolphinFileOperation *op)
{
	/* Cancel the operation if it's still in progress. */
	g_cancellable_cancel (op->cancellable);
}

static void
rename_get_info_callback (GObject *source_object,
			  GAsyncResult *res,
			  gpointer callback_data)
{
	NolphinFileOperation *op;
	NolphinDirectory *directory;
	NolphinFile *existing_file;
	char *old_name;
	char *old_uri;
	char *new_uri;
	const char *new_name;
	GFileInfo *new_info;
	GError *error;

	op = callback_data;

	error = NULL;
	new_info = g_file_query_info_finish (G_FILE (source_object), res, &error);
	if (new_info != NULL) {
		directory = op->file->details->directory;

		new_name = g_file_info_get_name (new_info);

		/* If there was another file by the same name in this
		 * directory and it is not the same file that we are
		 * renaming, mark it gone.
		 */
		existing_file = nolphin_directory_find_file_by_name (directory, new_name);
		if (existing_file != NULL && existing_file != op->file) {
			nolphin_file_mark_gone (existing_file);
			nolphin_file_changed (existing_file);
		}

		old_uri = nolphin_file_get_uri (op->file);
		old_name = g_strdup (op->file->details->name);

		update_info_and_name (op->file, new_info);

		g_free (old_name);

		new_uri = nolphin_file_get_uri (op->file);
		nolphin_directory_moved (old_uri, new_uri);

        xapp_favorites_rename (xapp_favorites_get_default (),
                               old_uri,
                               new_uri);

		g_free (new_uri);
		g_free (old_uri);

		/* the rename could have affected the display name if e.g.
		 * we're in a vfolder where the name comes from a desktop file
		 * and a rename affects the contents of the desktop file.
		 */
		if (op->file->details->got_custom_display_name) {
			nolphin_file_invalidate_attributes (op->file,
							     NOLPHIN_FILE_ATTRIBUTE_INFO |
							     NOLPHIN_FILE_ATTRIBUTE_LINK_INFO);
		}

		g_object_unref (new_info);
	}
	nolphin_file_operation_complete (op, NULL, error);
	if (error) {
		g_error_free (error);
	}
}

static void
rename_callback (GObject *source_object,
		 GAsyncResult *res,
		 gpointer callback_data)
{
	NolphinFileOperation *op;
	GFile *new_file;
	GError *error;

	op = callback_data;

	error = NULL;
	new_file = g_file_set_display_name_finish (G_FILE (source_object),
						   res, &error);

	if (new_file != NULL) {
		if (op->undo_info != NULL) {
			nolphin_file_undo_info_rename_set_data_post (NOLPHIN_FILE_UNDO_INFO_RENAME (op->undo_info),
								      new_file);
		}

		g_file_query_info_async (new_file,
					 NOLPHIN_FILE_DEFAULT_ATTRIBUTES,
					 0,
					 G_PRIORITY_DEFAULT,
					 op->cancellable,
					 rename_get_info_callback, op);
	} else {
		nolphin_file_operation_complete (op, NULL, error);
		g_error_free (error);
	}
}

static gboolean
name_is (NolphinFile *file, const char *new_name)
{
    return strcmp (new_name, file->details->name) == 0;
}

void
nolphin_file_rename (NolphinFile *file,
		      const char *new_name,
		      NolphinFileOperationCallback callback,
		      gpointer callback_data)
{
	NolphinFileOperation *op;
	char *uri;
	char *old_name;
	char *new_file_name;
	gboolean success, name_changed;
	gboolean is_renameable_desktop_file;
	GFile *location;
	GError *error;

	g_return_if_fail (NOLPHIN_IS_FILE (file));
	g_return_if_fail (new_name != NULL);
	g_return_if_fail (callback != NULL);

	is_renameable_desktop_file =
		is_desktop_file (file) && can_rename_desktop_file (file);

	/* Return an error for incoming names containing path separators.
	 * But not for .desktop files as '/' are allowed for them */
	if (strstr (new_name, "/") != NULL && !is_renameable_desktop_file) {
		error = g_error_new (G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
				     _("Schrägstriche sind in Dateinamen nicht erlaubt"));
		(* callback) (file, NULL, error, callback_data);
		g_error_free (error);
		return;
	}

	/* Can't rename a file that's already gone.
	 * We need to check this here because there may be a new
	 * file with the same name.
	 */
	if (nolphin_file_is_gone (file)) {
	       	/* Claim that something changed even if the rename
		 * failed. This makes it easier for some clients who
		 * see the "reverting" to the old name as "changing
		 * back".
		 */
		nolphin_file_changed (file);
		error = g_error_new (G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
				     _("Datei nicht gefunden"));
		(* callback) (file, NULL, error, callback_data);
		g_error_free (error);
		return;
	}

	/* Test the name-hasn't-changed case explicitly, for two reasons.
	 * (1) rename returns an error if new & old are same.
	 * (2) We don't want to send file-changed signal if nothing changed.
	 */
	if (!NOLPHIN_IS_DESKTOP_ICON_FILE (file) &&
	    !is_renameable_desktop_file &&
	    name_is (file, new_name)) {
		(* callback) (file, NULL, NULL, callback_data);
		return;
	}

	/* Self-owned files can't be renamed. Test the name-not-actually-changing
	 * case before this case.
	 */
	if (nolphin_file_is_self_owned (file)) {
	       	/* Claim that something changed even if the rename
		 * failed. This makes it easier for some clients who
		 * see the "reverting" to the old name as "changing
		 * back".
		 */
		nolphin_file_changed (file);
		error = g_error_new (G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
				     _("Dateien im obersten Ordner können nicht umbenannt werden"));

		(* callback) (file, NULL, error, callback_data);
		g_error_free (error);
		return;
	}

	if (NOLPHIN_IS_DESKTOP_ICON_FILE (file)) {
		NolphinDesktopLink *link;

		link = nolphin_desktop_icon_file_get_link (NOLPHIN_DESKTOP_ICON_FILE (file));
		old_name = nolphin_file_get_display_name (file);

		if ((old_name != NULL && strcmp (new_name, old_name) == 0)) {
			success = TRUE;
		} else {
			success = (link != NULL && nolphin_desktop_link_rename (link, new_name));
		}

		if (success) {
			(* callback) (file, NULL, NULL, callback_data);
		} else {
			error = g_error_new (G_IO_ERROR, G_IO_ERROR_FAILED,
					     _("Schreibtischsymbol konnte nicht umbenannt werden"));
			(* callback) (file, NULL, error, callback_data);
			g_error_free (error);
		}

		g_free (old_name);
		g_object_unref (link);
		return;
	}

	if (is_renameable_desktop_file) {
		/* Don't actually change the name if the new name is the same.
		 * This helps for the vfolder method where this can happen and
		 * we want to minimize actual changes
		 */
		uri = nolphin_file_get_uri (file);
		old_name = nolphin_link_local_get_text (uri);
		if (old_name != NULL && strcmp (new_name, old_name) == 0) {
			success = TRUE;
			name_changed = FALSE;
		} else {
			success = nolphin_link_local_set_text (uri, new_name);
			name_changed = TRUE;
		}
		g_free (old_name);
		g_free (uri);

		if (!success) {
			error = g_error_new (G_IO_ERROR, G_IO_ERROR_FAILED,
					     _("Schreibtischdatei konnte nicht umbenannt werden"));
			(* callback) (file, NULL, error, callback_data);
			g_error_free (error);
			return;
		}

        if (g_str_has_suffix (new_name, ".desktop")) {
            new_file_name = g_strdup (new_name);
        } else {
            new_file_name = g_strdup_printf ("%s.desktop", new_name);
        }

        new_file_name = g_strdelimit (new_file_name, "/", '-');

		if (name_is (file, new_file_name)) {
			if (name_changed) {
				nolphin_file_invalidate_attributes (file,
								     NOLPHIN_FILE_ATTRIBUTE_INFO |
								     NOLPHIN_FILE_ATTRIBUTE_LINK_INFO);
			}

			(* callback) (file, NULL, NULL, callback_data);
			g_free (new_file_name);
			return;
		}
	} else {
		new_file_name = g_strdup (new_name);
	}

	/* Set up a renaming operation. */
	op = nolphin_file_operation_new (file, callback, callback_data);
	op->is_rename = TRUE;
	location = nolphin_file_get_location (file);

	/* Tell the undo manager a rename is taking place */
	if (!nolphin_file_undo_manager_pop_flag ()) {
		op->undo_info = nolphin_file_undo_info_rename_new ();

		old_name = nolphin_file_get_display_name (file);
		nolphin_file_undo_info_rename_set_data_pre (NOLPHIN_FILE_UNDO_INFO_RENAME (op->undo_info),
							     location, old_name, new_file_name);
		g_free (old_name);
	}

	/* Do the renaming. */
	g_file_set_display_name_async (location,
				       new_file_name,
				       G_PRIORITY_DEFAULT,
				       op->cancellable,
				       rename_callback,
				       op);
	g_free (new_file_name);
	g_object_unref (location);
}

gboolean
nolphin_file_rename_in_progress (NolphinFile *file)
{
	GList *node;
	NolphinFileOperation *op;

	for (node = file->details->operations_in_progress; node != NULL; node = node->next) {
		op = node->data;
		if (op->is_rename) {
			return TRUE;
		}
	}
	return FALSE;
}

void
nolphin_file_cancel (NolphinFile *file,
		      NolphinFileOperationCallback callback,
		      gpointer callback_data)
{
	GList *node, *next;
	NolphinFileOperation *op;

	for (node = file->details->operations_in_progress; node != NULL; node = next) {
		next = node->next;
		op = node->data;

		g_assert (op->file == file);
		if (op->callback == callback && op->callback_data == callback_data) {
			nolphin_file_operation_cancel (op);
		}
	}
}

gboolean
nolphin_file_matches_uri (NolphinFile *file, const char *match_uri)
{
	GFile *match_file, *location;
	gboolean result;

	g_return_val_if_fail (NOLPHIN_IS_FILE (file), FALSE);
	g_return_val_if_fail (match_uri != NULL, FALSE);

	location = nolphin_file_get_location (file);
	match_file = g_file_new_for_uri (match_uri);
	result = g_file_equal (location, match_file);
	g_object_unref (location);
	g_object_unref (match_file);

	return result;
}

int
nolphin_file_compare_location (NolphinFile *file_1,
                                NolphinFile *file_2)
{
	GFile *loc_a, *loc_b;
	gboolean res;

	loc_a = nolphin_file_get_location (file_1);
	loc_b = nolphin_file_get_location (file_2);

	res = !g_file_equal (loc_a, loc_b);

	g_object_unref (loc_a);
	g_object_unref (loc_b);

	return (gint) res;
}

gboolean
nolphin_file_is_local (NolphinFile *file)
{
	g_return_val_if_fail (NOLPHIN_IS_FILE (file), FALSE);

	return nolphin_directory_is_local (file->details->directory);
}

static void
update_link (NolphinFile *link_file, NolphinFile *target_file)
{
	g_assert (NOLPHIN_IS_FILE (link_file));
	g_assert (NOLPHIN_IS_FILE (target_file));

	/* FIXME bugzilla.gnome.org 42044: If we don't put any code
	 * here then the hash table is a waste of time.
	 */
}

static GList *
get_link_files (NolphinFile *target_file)
{
	char *uri;
	GList **link_files;

	if (symbolic_links == NULL) {
		link_files = NULL;
	} else {
		uri = nolphin_file_get_uri (target_file);
		link_files = g_hash_table_lookup (symbolic_links, uri);
		g_free (uri);

        if (link_files) {
            if (!nolphin_file_is_symbolic_link (target_file)) {
                return nolphin_file_list_copy (*link_files);
            } else {
                /* We keep a hash table of uri keys with lists of NolphinFiles attached, to keep
                 * track of what files point to a particular uri.
                 *
                 * When a file gets updated, it fetches the potential list of any files that
                 * are actually links to itself.  If our original file is also actually a symbolic
                 * link that points to to one of these link files, we'll end up triggering a cyclic
                 * update - one file triggers an update for the other, which in turn triggers an update
                 * back to the original file, and so on.
                 *
                 * So we check if target_file is itself a link.  If it is, we compare its symbolic
                 * target location with the location of the returned link_files.  We skip any that match
                 * (meaning, any link-back files that are themselves the target of the current symbolic link
                 * file.
                 */
                GList *derecursed = NULL;
                GList *l;

                l = *link_files;

                while (l != NULL) {
                    NolphinFile *link_file;
                    gboolean add;

                    add = TRUE;

                    link_file = NOLPHIN_FILE (l->data);

                    if (nolphin_file_is_symbolic_link (target_file)) {
                        gchar *target_symlink_uri = nolphin_file_get_symbolic_link_target_uri (target_file);
                        gchar *link_uri = nolphin_file_get_uri (link_file);

                        GFile *target_symlink_gfile = g_file_new_for_uri (target_symlink_uri);
                        GFile *link_gfile = g_file_new_for_uri (link_uri);

                        if (g_file_equal (target_symlink_gfile, link_gfile)) {
                            add = FALSE;
                        }

                        g_object_unref (target_symlink_gfile);
                        g_object_unref (link_gfile);
                        g_free (target_symlink_uri);
                        g_free (link_uri);
                    }

                    if (add) {
                        derecursed = g_list_prepend (derecursed, nolphin_file_ref (link_file));
                    }

                    l = l->next;
                }

                return derecursed;
            }
        }
    }

	return NULL;
}

static void
update_links_if_target (NolphinFile *target_file)
{
	GList *link_files, *p;

	link_files = get_link_files (target_file);
	for (p = link_files; p != NULL; p = p->next) {
		update_link (NOLPHIN_FILE (p->data), target_file);
	}
	nolphin_file_list_free (link_files);
}


static guint64 cached_thumbnail_limit;
int cached_thumbnail_size;
double scaled_cached_thumbnail_size;
static int show_image_thumbs;

static gboolean
access_ok (const gchar *path)
{
    if (g_access (path, R_OK|W_OK) != 0) {
        if (errno != ENOENT && errno != EFAULT) {
            return FALSE;
        }
    }

    return TRUE;
}

static gboolean
update_info_internal (NolphinFile *file,
		      GFileInfo *info,
		      gboolean update_name)
{
	GList *node;
	gboolean changed;
	gboolean is_symlink, is_hidden, is_mountpoint;
	gboolean has_permissions;
	guint32 permissions;
	gboolean can_read, can_write, can_execute, can_delete, can_trash, can_rename, can_mount, can_unmount, can_eject;
	gboolean can_start, can_start_degraded, can_stop, can_poll_for_media, is_media_check_automatic;
	GDriveStartStopType start_stop_type;
	gboolean thumbnailing_failed;
	int uid, gid;
	goffset size;
	int sort_order;
	time_t atime, mtime, ctime, btime;
	time_t trash_time;
	GTimeVal g_trash_time;
	const char * time_string;
	const char *symlink_name, *selinux_context, *name, *thumbnail_path;
    char *mime_type;
	GFileType file_type;
	GIcon *icon;
	char *old_activation_uri;
	const char *activation_uri;
	const char *description;
	const char *filesystem_id;
	const char *trash_orig_path;
	const char *group, *owner, *owner_real;
	gboolean free_owner, free_group;
    const char *edit_name;

	if (file->details->is_gone) {
		return FALSE;
	}

	if (info == NULL) {
		nolphin_file_mark_gone (file);
		return TRUE;
	}

	file->details->file_info_is_up_to_date = TRUE;

    file->details->thumbnail_access_problem = FALSE;

    file->details->pinning = FILE_META_STATE_INIT;
    file->details->favorite = FILE_META_STATE_INIT;

	/* FIXME bugzilla.gnome.org 42044: Need to let links that
	 * point to the old name know that the file has been renamed.
	 */

	remove_from_link_hash_table (file);

	changed = FALSE;

	if (!file->details->got_file_info) {
		changed = TRUE;
	}
	file->details->got_file_info = TRUE;

    edit_name = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_STANDARD_EDIT_NAME);

	changed |= nolphin_file_set_display_name (file,
						  g_file_info_get_display_name (info),
						  edit_name,
						  FALSE);

	file_type = g_file_info_get_file_type (info);
	if (file->details->type != file_type) {
		changed = TRUE;
	}
	file->details->type = file_type;

	if (!file->details->got_custom_activation_uri && !nolphin_file_is_in_trash (file)) {
		activation_uri = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_STANDARD_TARGET_URI);
		if (activation_uri == NULL) {
			if (file->details->activation_uri) {
				g_free (file->details->activation_uri);
				file->details->activation_uri = NULL;
				changed = TRUE;
			}
		} else {
			old_activation_uri = file->details->activation_uri;
			file->details->activation_uri = g_strdup (activation_uri);

			if (old_activation_uri) {
				if (strcmp (old_activation_uri,
					    file->details->activation_uri) != 0) {
					changed = TRUE;
				}
				g_free (old_activation_uri);
			} else {
				changed = TRUE;
			}
		}
	}

    is_symlink = g_file_info_get_attribute_boolean (info, G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK);
	if (file->details->is_symlink != is_symlink) {
		changed = TRUE;
	}
	file->details->is_symlink = is_symlink;

    is_hidden = g_file_info_get_attribute_boolean (info, G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN) ||
                g_file_info_get_attribute_boolean (info, G_FILE_ATTRIBUTE_STANDARD_IS_BACKUP);

	if (file->details->is_hidden != is_hidden) {
		changed = TRUE;
	}
	file->details->is_hidden = is_hidden;

	is_mountpoint = g_file_info_get_attribute_boolean (info, G_FILE_ATTRIBUTE_UNIX_IS_MOUNTPOINT);
	if (file->details->is_mountpoint != is_mountpoint) {
		changed = TRUE;
	}
	file->details->is_mountpoint = is_mountpoint;

	has_permissions = g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_UNIX_MODE);
	permissions = g_file_info_get_attribute_uint32 (info, G_FILE_ATTRIBUTE_UNIX_MODE);;
	if (file->details->has_permissions != has_permissions ||
	    file->details->permissions != permissions) {
		changed = TRUE;
	}
	file->details->has_permissions = has_permissions;
	file->details->permissions = permissions;

	/* We default to TRUE for this if we can't know */
	can_read = TRUE;
	can_write = TRUE;
	can_execute = TRUE;
	can_delete = TRUE;
	can_rename = TRUE;
	can_trash = FALSE;
	can_mount = FALSE;
	can_unmount = FALSE;
	can_eject = FALSE;
	can_start = FALSE;
	can_start_degraded = FALSE;
	can_stop = FALSE;
	can_poll_for_media = FALSE;
	is_media_check_automatic = FALSE;
	start_stop_type = G_DRIVE_START_STOP_TYPE_UNKNOWN;
	if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_ACCESS_CAN_READ)) {
		can_read = g_file_info_get_attribute_boolean (info,
							      G_FILE_ATTRIBUTE_ACCESS_CAN_READ);
	}
	if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE)) {
		can_write = g_file_info_get_attribute_boolean (info,
							       G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE);
	}
	if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_ACCESS_CAN_EXECUTE)) {
		can_execute = g_file_info_get_attribute_boolean (info,
								G_FILE_ATTRIBUTE_ACCESS_CAN_EXECUTE);
	}
	if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_ACCESS_CAN_DELETE)) {
		can_delete = g_file_info_get_attribute_boolean (info,
								G_FILE_ATTRIBUTE_ACCESS_CAN_DELETE);
	}
	if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_ACCESS_CAN_TRASH)) {
		can_trash = g_file_info_get_attribute_boolean (info,
							       G_FILE_ATTRIBUTE_ACCESS_CAN_TRASH);
	}
	if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_ACCESS_CAN_RENAME)) {
		can_rename = g_file_info_get_attribute_boolean (info,
								G_FILE_ATTRIBUTE_ACCESS_CAN_RENAME);
	}
	if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_MOUNTABLE_CAN_MOUNT)) {
		can_mount = g_file_info_get_attribute_boolean (info,
							       G_FILE_ATTRIBUTE_MOUNTABLE_CAN_MOUNT);
	}
	if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_MOUNTABLE_CAN_UNMOUNT)) {
		can_unmount = g_file_info_get_attribute_boolean (info,
								 G_FILE_ATTRIBUTE_MOUNTABLE_CAN_UNMOUNT);
	}
	if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_MOUNTABLE_CAN_EJECT)) {
		can_eject = g_file_info_get_attribute_boolean (info,
							       G_FILE_ATTRIBUTE_MOUNTABLE_CAN_EJECT);
	}
	if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_MOUNTABLE_CAN_START)) {
		can_start = g_file_info_get_attribute_boolean (info,
							       G_FILE_ATTRIBUTE_MOUNTABLE_CAN_START);
	}
	if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_MOUNTABLE_CAN_START_DEGRADED)) {
		can_start_degraded = g_file_info_get_attribute_boolean (info,
							       G_FILE_ATTRIBUTE_MOUNTABLE_CAN_START_DEGRADED);
	}
	if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_MOUNTABLE_CAN_STOP)) {
		can_stop = g_file_info_get_attribute_boolean (info,
							      G_FILE_ATTRIBUTE_MOUNTABLE_CAN_STOP);
	}
	if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_MOUNTABLE_START_STOP_TYPE)) {
		start_stop_type = g_file_info_get_attribute_uint32 (info,
								    G_FILE_ATTRIBUTE_MOUNTABLE_START_STOP_TYPE);
	}
	if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_MOUNTABLE_CAN_POLL)) {
		can_poll_for_media = g_file_info_get_attribute_boolean (info,
									G_FILE_ATTRIBUTE_MOUNTABLE_CAN_POLL);
	}
	if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_MOUNTABLE_IS_MEDIA_CHECK_AUTOMATIC)) {
		is_media_check_automatic = g_file_info_get_attribute_boolean (info,
									      G_FILE_ATTRIBUTE_MOUNTABLE_IS_MEDIA_CHECK_AUTOMATIC);
	}
	if (file->details->can_read != can_read ||
	    file->details->can_write != can_write ||
	    file->details->can_execute != can_execute ||
	    file->details->can_delete != can_delete ||
	    file->details->can_trash != can_trash ||
	    file->details->can_rename != can_rename ||
	    file->details->can_mount != can_mount ||
	    file->details->can_unmount != can_unmount ||
	    file->details->can_eject != can_eject ||
	    file->details->can_start != can_start ||
	    file->details->can_start_degraded != can_start_degraded ||
	    file->details->can_stop != can_stop ||
	    file->details->start_stop_type != start_stop_type ||
	    file->details->can_poll_for_media != can_poll_for_media ||
	    file->details->is_media_check_automatic != is_media_check_automatic) {
		changed = TRUE;
	}

	file->details->can_read = can_read;
	file->details->can_write = can_write;
	file->details->can_execute = can_execute;
	file->details->can_delete = can_delete;
	file->details->can_trash = can_trash;
	file->details->can_rename = can_rename;
	file->details->can_mount = can_mount;
	file->details->can_unmount = can_unmount;
	file->details->can_eject = can_eject;
	file->details->can_start = can_start;
	file->details->can_start_degraded = can_start_degraded;
	file->details->can_stop = can_stop;
	file->details->start_stop_type = start_stop_type;
	file->details->can_poll_for_media = can_poll_for_media;
	file->details->is_media_check_automatic = is_media_check_automatic;

    file->details->favorite_checked = FALSE;

	free_owner = FALSE;
	owner = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_OWNER_USER);
	owner_real = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_OWNER_USER_REAL);
	free_group = FALSE;
	group = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_OWNER_GROUP);

	uid = -1;
	gid = -1;
	if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_UNIX_UID)) {
		uid = g_file_info_get_attribute_uint32 (info, G_FILE_ATTRIBUTE_UNIX_UID);
		if (owner == NULL) {
			free_owner = TRUE;
			owner = g_strdup_printf ("%d", uid);
		}
	}
	if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_UNIX_GID)) {
		gid = g_file_info_get_attribute_uint32 (info, G_FILE_ATTRIBUTE_UNIX_GID);
		if (group == NULL) {
			free_group = TRUE;
			group = g_strdup_printf ("%d", gid);
		}
	}
	if (file->details->uid != uid ||
	    file->details->gid != gid) {
		changed = TRUE;
	}
	file->details->uid = uid;
	file->details->gid = gid;

	if (g_strcmp0 (file->details->owner, owner) != 0) {
		changed = TRUE;
		g_clear_pointer (&file->details->owner, g_ref_string_release);
		file->details->owner = g_ref_string_new_intern (owner);
	}

	if (g_strcmp0 (file->details->owner_real, owner_real) != 0) {
		changed = TRUE;
        g_clear_pointer (&file->details->owner_real, g_ref_string_release);
		file->details->owner_real = g_ref_string_new_intern (owner_real);
	}

	if (g_strcmp0 (file->details->group, group) != 0) {
		changed = TRUE;
        g_clear_pointer (&file->details->group, g_ref_string_release);
		file->details->group = g_ref_string_new_intern (group);
	}

	if (free_owner) {
		g_free ((char *)owner);
	}
	if (free_group) {
		g_free ((char *)group);
	}

	size = -1;
	if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_STANDARD_SIZE)) {
		size = g_file_info_get_size (info);
	}
	if (file->details->size != size) {
		changed = TRUE;
	}
	file->details->size = size;

    sort_order = g_file_info_get_attribute_int32 (info, G_FILE_ATTRIBUTE_STANDARD_SORT_ORDER);

	if (file->details->sort_order != sort_order) {
		changed = TRUE;
	}
	file->details->sort_order = sort_order;

	atime = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_TIME_ACCESS);
	ctime = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_TIME_CHANGED);
    mtime = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_TIME_MODIFIED);
	btime = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_TIME_CREATED);
	if (file->details->atime != atime ||
	    file->details->mtime != mtime ||
	    file->details->ctime != ctime ||
        file->details->btime != btime) {
		if (file->details->thumbnail == NULL) {
			file->details->thumbnail_is_up_to_date = FALSE;
		}

		changed = TRUE;
	}
	file->details->atime = atime;
	file->details->ctime = ctime;
	file->details->mtime = mtime;
    file->details->btime = btime;

	if (file->details->thumbnail != NULL &&
	    file->details->thumbnail_mtime != 0 &&
	    file->details->thumbnail_mtime != mtime) {
		file->details->thumbnail_is_up_to_date = FALSE;
		changed = TRUE;
	}

	icon = g_file_info_get_icon (info);
	if (!g_icon_equal (icon, file->details->icon)) {
		changed = TRUE;

		if (file->details->icon) {
			g_object_unref (file->details->icon);
		}
		file->details->icon = g_object_ref (icon);
	}

	thumbnail_path =  g_file_info_get_attribute_byte_string (info, G_FILE_ATTRIBUTE_THUMBNAIL_PATH);

	if (g_strcmp0 (file->details->thumbnail_path, thumbnail_path) != 0) {
		changed = TRUE;
		g_free (file->details->thumbnail_path);

        if (show_image_thumbs != NOLPHIN_SPEED_TRADEOFF_NEVER && thumbnail_path != NULL && !access_ok (thumbnail_path)) {
            file->details->thumbnail_access_problem = TRUE;
            file->details->thumbnail_path = NULL;
        } else {
            file->details->thumbnail_path = g_strdup (thumbnail_path);
        }
	}

	thumbnailing_failed =  g_file_info_get_attribute_boolean (info, G_FILE_ATTRIBUTE_THUMBNAILING_FAILED);
	if (file->details->thumbnailing_failed != thumbnailing_failed) {
		changed = TRUE;
		file->details->thumbnailing_failed = thumbnailing_failed;
	}

    symlink_name = g_file_info_get_attribute_byte_string (info, G_FILE_ATTRIBUTE_STANDARD_SYMLINK_TARGET);

	if (g_strcmp0 (file->details->symlink_name, symlink_name) != 0) {
		changed = TRUE;
		g_free (file->details->symlink_name);
		file->details->symlink_name = g_strdup (symlink_name);
	}

	selinux_context = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_SELINUX_CONTEXT);
	if (g_strcmp0 (file->details->selinux_context, selinux_context) != 0) {
		changed = TRUE;
		g_free (file->details->selinux_context);
		file->details->selinux_context = g_strdup (selinux_context);
	}

	description = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_STANDARD_DESCRIPTION);
	if (g_strcmp0 (file->details->description, description) != 0) {
		changed = TRUE;
		g_free (file->details->description);
		file->details->description = g_strdup (description);
	}

	filesystem_id = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_ID_FILESYSTEM);
	if (g_strcmp0 (file->details->filesystem_id, filesystem_id) != 0) {
		changed = TRUE;
        g_clear_pointer (&file->details->filesystem_id, g_ref_string_release);
		file->details->filesystem_id = g_ref_string_new_intern (filesystem_id);
	}

	trash_time = 0;
	time_string = g_file_info_get_attribute_string (info, "trash::deletion-date");
	if (time_string != NULL) {
		g_time_val_from_iso8601 (time_string, &g_trash_time);
		trash_time = g_trash_time.tv_sec;
	}
	if (file->details->trash_time != trash_time) {
		changed = TRUE;
		file->details->trash_time = trash_time;
	}

	trash_orig_path = g_file_info_get_attribute_byte_string (info, "trash::orig-path");
	if (g_strcmp0 (file->details->trash_orig_path, trash_orig_path) != 0) {
		changed = TRUE;
		g_free (file->details->trash_orig_path);
		file->details->trash_orig_path = g_strdup (trash_orig_path);
	}

    if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_PREVIEW_ICON))
    {
        file->details->has_preview_icon = TRUE;
    }

	changed |=
		nolphin_file_update_metadata_from_info (file, info);

	if (update_name) {
		name = g_file_info_get_name (info);
		if (file->details->name == NULL ||
		    strcmp (file->details->name, name) != 0) {
			changed = TRUE;

			node = nolphin_directory_begin_file_name_change
				(file->details->directory, file);

            g_clear_pointer (&file->details->name, g_ref_string_release);
			if (g_strcmp0 (file->details->display_name, name) == 0) {
				file->details->name = g_ref_string_acquire (file->details->display_name);
			} else {
				file->details->name = g_ref_string_new (name);
			}

			if (!file->details->got_custom_display_name &&
			    g_file_info_get_display_name (info) == NULL) {
				/* If the file info's display name is NULL,
				 * nolphin_file_set_display_name() did
				 * not unset the display name.
				 */
				nolphin_file_clear_display_name (file);
			}

			nolphin_directory_end_file_name_change
				(file->details->directory, file, node);
		}
	}

    mime_type = nolphin_get_best_guess_file_mimetype (file->details->name, info, size);

    if (g_strcmp0 (file->details->mime_type, mime_type) != 0) {
        changed = TRUE;
        g_clear_pointer (&file->details->mime_type, g_ref_string_release);
        file->details->mime_type = g_ref_string_new (mime_type);
    }

    g_free (mime_type);

	if (changed) {
		nolphin_file_invalidate_sort_collation_key (file);

		add_to_link_hash_table (file);

		update_links_if_target (file);
	}

	return changed;
}

static gboolean
update_info_and_name (NolphinFile *file,
		      GFileInfo *info)
{
	return update_info_internal (file, info, TRUE);
}

gboolean
nolphin_file_update_info (NolphinFile *file,
			   GFileInfo *info)
{
	return update_info_internal (file, info, FALSE);
}

static gboolean
update_name_internal (NolphinFile *file,
		      const char *name,
		      gboolean in_directory)
{
	GList *node;

	g_assert (name != NULL);

	if (file->details->is_gone) {
		return FALSE;
	}

	if (name_is (file, name)) {
		return FALSE;
	}

	node = NULL;
	if (in_directory) {
		node = nolphin_directory_begin_file_name_change
			(file->details->directory, file);
	}

    g_clear_pointer (&file->details->name, g_ref_string_release);
	file->details->name = g_ref_string_new (name);

	if (!file->details->got_custom_display_name) {
		nolphin_file_clear_display_name (file);
	}

	if (in_directory) {
		nolphin_directory_end_file_name_change
			(file->details->directory, file, node);
	}

	return TRUE;
}

gboolean
nolphin_file_update_name (NolphinFile *file, const char *name)
{
	gboolean ret;

	ret = update_name_internal (file, name, TRUE);

	if (ret) {
		update_links_if_target (file);
	}

	return ret;
}

gboolean
nolphin_file_update_name_and_directory (NolphinFile *file,
					 const char *name,
					 NolphinDirectory *new_directory)
{
	NolphinDirectory *old_directory;
	FileMonitors *monitors;

	g_return_val_if_fail (NOLPHIN_IS_FILE (file), FALSE);
	g_return_val_if_fail (NOLPHIN_IS_DIRECTORY (file->details->directory), FALSE);
	g_return_val_if_fail (!file->details->is_gone, FALSE);
	g_return_val_if_fail (!nolphin_file_is_self_owned (file), FALSE);
	g_return_val_if_fail (NOLPHIN_IS_DIRECTORY (new_directory), FALSE);

	old_directory = file->details->directory;
	if (old_directory == new_directory) {
		if (name) {
			return update_name_internal (file, name, TRUE);
		} else {
			return FALSE;
		}
	}

	nolphin_file_ref (file);

	/* FIXME bugzilla.gnome.org 42044: Need to let links that
	 * point to the old name know that the file has been moved.
	 */

	remove_from_link_hash_table (file);

	monitors = nolphin_directory_remove_file_monitors (old_directory, file);
	nolphin_directory_remove_file (old_directory, file);

	file->details->directory = nolphin_directory_ref (new_directory);
	nolphin_directory_unref (old_directory);

	if (name) {
		update_name_internal (file, name, FALSE);
	}

	nolphin_directory_add_file (new_directory, file);
	nolphin_directory_add_file_monitors (new_directory, file, monitors);

	add_to_link_hash_table (file);

	update_links_if_target (file);

	nolphin_file_unref (file);

	return TRUE;
}

void
nolphin_file_set_directory (NolphinFile *file,
			     NolphinDirectory *new_directory)
{
	nolphin_file_update_name_and_directory (file, NULL, new_directory);
}

static Knowledge
get_item_count (NolphinFile *file,
		guint *count)
{
	gboolean known, unreadable;

	known = nolphin_file_get_directory_item_count
		(file, count, &unreadable);
	if (!known) {
		return UNKNOWN;
	}
	if (unreadable) {
		return UNKNOWABLE;
	}
	return KNOWN;
}

static Knowledge
get_size (NolphinFile *file,
	  goffset *size)
{
	/* If we tried and failed, then treat it like there is no size
	 * to know.
	 */
	if (file->details->get_info_failed) {
		return UNKNOWABLE;
	}

	/* If the info is NULL that means we haven't even tried yet,
	 * so it's just unknown, not unknowable.
	 */
	if (!file->details->got_file_info) {
		return UNKNOWN;
	}

	/* If we got info with no size in it, it means there is no
	 * such thing as a size as far as gnome-vfs is concerned,
	 * so "unknowable".
	 */
	if (file->details->size == -1) {
		return UNKNOWABLE;
	}

	/* We have a size! */
	*size = file->details->size;
	return KNOWN;
}

static Knowledge
get_time (NolphinFile *file,
	  time_t *time_out,
	  NolphinDateType type)
{
	time_t time;

	/* If we tried and failed, then treat it like there is no size
	 * to know.
	 */
	if (file->details->get_info_failed) {
		return UNKNOWABLE;
	}

	/* If the info is NULL that means we haven't even tried yet,
	 * so it's just unknown, not unknowable.
	 */
	if (!file->details->got_file_info) {
		return UNKNOWN;
	}

	time = 0;
	switch (type) {
	case NOLPHIN_DATE_TYPE_MODIFIED:
		time = file->details->mtime;
		break;
	case NOLPHIN_DATE_TYPE_ACCESSED:
		time = file->details->atime;
		break;
    case NOLPHIN_DATE_TYPE_CREATED:
        time = file->details->btime;
        break;
	case NOLPHIN_DATE_TYPE_TRASHED:
		time = file->details->trash_time;
		break;
	case NOLPHIN_DATE_TYPE_CHANGED:
    case NOLPHIN_DATE_TYPE_PERMISSIONS_CHANGED:
	/* FIXME is some logic needed here ?
	 */
	default:
		g_assert_not_reached ();
		break;
	}

	*time_out = time;

	/* If we got info with no modification time in it, it means
	 * there is no such thing as a modification time as far as
	 * gnome-vfs is concerned, so "unknowable".
	 */
	if (time == 0) {
		return UNKNOWABLE;
	}
	return KNOWN;
}

static int
compare_directories_by_count (NolphinFile *file_1, NolphinFile *file_2)
{
	/* Sort order:
	 *   Directories with unknown # of items
	 *   Directories with "unknowable" # of items
	 *   Directories with 0 items
	 *   Directories with n items
	 */

	Knowledge count_known_1, count_known_2;
	guint count_1, count_2;

	count_known_1 = get_item_count (file_1, &count_1);
	count_known_2 = get_item_count (file_2, &count_2);

	if (count_known_1 > count_known_2) {
		return -1;
	}
	if (count_known_1 < count_known_2) {
		return +1;
	}

	/* count_known_1 and count_known_2 are equal now. Check if count
	 * details are UNKNOWABLE or UNKNOWN.
	 */
	if (count_known_1 == UNKNOWABLE || count_known_1 == UNKNOWN) {
		return 0;
	}

	if (count_1 < count_2) {
		return -1;
	}
	if (count_1 > count_2) {
		return +1;
	}

	return 0;
}

static int
compare_files_by_size (NolphinFile *file_1, NolphinFile *file_2)
{
	/* Sort order:
	 *   Files with unknown size.
	 *   Files with "unknowable" size.
	 *   Files with smaller sizes.
	 *   Files with large sizes.
	 */

	Knowledge size_known_1, size_known_2;
	goffset size_1 = 0, size_2 = 0;

	size_known_1 = get_size (file_1, &size_1);
	size_known_2 = get_size (file_2, &size_2);

	if (size_known_1 > size_known_2) {
		return -1;
	}
	if (size_known_1 < size_known_2) {
		return +1;
	}

	/* size_known_1 and size_known_2 are equal now. Check if size
	 * details are UNKNOWABLE or UNKNOWN
	 */
	if (size_known_1 == UNKNOWABLE || size_known_1 == UNKNOWN) {
		return 0;
	}

	if (size_1 < size_2) {
		return -1;
	}
	if (size_1 > size_2) {
		return +1;
	}

	return 0;
}

static int
compare_by_size (NolphinFile *file_1, NolphinFile *file_2)
{
	/* Sort order:
	 *   Directories with n items
	 *   Directories with 0 items
	 *   Directories with "unknowable" # of items
	 *   Directories with unknown # of items
	 *   Files with large sizes.
	 *   Files with smaller sizes.
	 *   Files with "unknowable" size.
	 *   Files with unknown size.
	 */

	gboolean is_directory_1, is_directory_2;

	is_directory_1 = nolphin_file_is_directory (file_1);
	is_directory_2 = nolphin_file_is_directory (file_2);

	if (is_directory_1 && !is_directory_2) {
		return -1;
	}
	if (is_directory_2 && !is_directory_1) {
		return +1;
	}

	if (is_directory_1) {
		return compare_directories_by_count (file_1, file_2);
	} else {
		return compare_files_by_size (file_1, file_2);
	}
}

static int
compare_by_display_name (NolphinFile *file_1, NolphinFile *file_2)
{
	const char *name_1, *name_2;
	const char *key_1, *key_2;
	gboolean sort_last_1, sort_last_2;
	int compare=0;

	name_1 = nolphin_file_peek_display_name (file_1);
	name_2 = nolphin_file_peek_display_name (file_2);

	sort_last_1 = name_1 && (name_1[0] == SORT_LAST_CHAR1 || name_1[0] == SORT_LAST_CHAR2);
	sort_last_2 = name_2 && (name_2[0] == SORT_LAST_CHAR1 || name_2[0] == SORT_LAST_CHAR2);

	if (sort_last_1 && !sort_last_2) {
		compare = +1;
	} else if (!sort_last_1 && sort_last_2) {
		compare = -1;
	} else if (name_1 == NULL || name_2 == NULL) {
        if (name_1 && !name_2)
            compare = +1;
        else if (!name_1 && name_2)
            compare = -1;
    } else {
		key_1 = nolphin_file_peek_display_name_collation_key (file_1);
		key_2 = nolphin_file_peek_display_name_collation_key (file_2);
		compare = g_strcmp0 (key_1, key_2);
	}

	return compare;
}

static int
compare_by_directory_name (NolphinFile *file_1, NolphinFile *file_2)
{
	char *directory_1, *directory_2;
	int compare;

	if (file_1->details->directory == file_2->details->directory) {
		return 0;
	}

	directory_1 = nolphin_file_get_parent_uri_for_display (file_1);
	directory_2 = nolphin_file_get_parent_uri_for_display (file_2);

	compare = g_utf8_collate (directory_1, directory_2);

	g_free (directory_1);
	g_free (directory_2);

	return compare;
}

static gboolean
file_has_note (NolphinFile *file)
{
	char *note;
	gboolean res;

	note = nolphin_file_get_metadata (file, NOLPHIN_METADATA_KEY_ANNOTATION, NULL);
	res = note != NULL && note[0] != 0;
	g_free (note);

	return res;
}

static GList *
prepend_automatic_keywords (NolphinFile *file,
                            NolphinFile *view_file,
                            GList *names)
{
	/* Prepend in reverse order. */

	if (file_has_note (file)) {
		names = g_list_prepend
			(names, g_strdup (NOLPHIN_FILE_EMBLEM_NAME_NOTE));
	}

	if (!nolphin_file_can_read (file)) {
		names = g_list_prepend
			(names, g_strdup (NOLPHIN_FILE_EMBLEM_NAME_CANT_READ));
	}
	if (nolphin_file_is_symbolic_link (file) && !nolphin_file_is_in_favorites (file)) {
		names = g_list_prepend
			(names, g_strdup (NOLPHIN_FILE_EMBLEM_NAME_SYMBOLIC_LINK));
	}
    if (nolphin_file_get_is_favorite (file) && !nolphin_file_is_in_favorites (file)) {
        names = g_list_prepend
            (names, g_strdup (NOLPHIN_FILE_EMBLEM_NAME_FAVORITE));
    }

	return names;
}

static int
compare_by_type (NolphinFile *file_1, NolphinFile *file_2, gboolean detailed)
{
	gboolean is_directory_1;
	gboolean is_directory_2;
	char *type_string_1;
	char *type_string_2;
	int result;

	/* Directories go first. Then, if mime types are identical,
	 * don't bother getting strings (for speed). This assumes
	 * that the string is dependent entirely on the mime type,
	 * which is true now but might not be later.
	 */
	is_directory_1 = nolphin_file_is_directory (file_1);
	is_directory_2 = nolphin_file_is_directory (file_2);

	if (is_directory_1 && is_directory_2) {
		return 0;
	}

	if (is_directory_1) {
		return -1;
	}

	if (is_directory_2) {
		return +1;
	}

    if (detailed) {
        type_string_1 = nolphin_file_get_detailed_type_as_string (file_1);
        type_string_2 = nolphin_file_get_detailed_type_as_string (file_2);
    } else {
        type_string_1 = nolphin_file_get_type_as_string (file_1);
        type_string_2 = nolphin_file_get_type_as_string (file_2);
    }

	if (type_string_1 == NULL || type_string_2 == NULL) {
		if (type_string_1 != NULL) {
			return -1;
		}

		if (type_string_2 != NULL) {
			return 1;
		}

		return 0;
	}

	result = g_utf8_collate (type_string_1, type_string_2);

	g_free (type_string_1);
	g_free (type_string_2);

	return result;
}

static int
compare_by_extension (NolphinFile *file_1, NolphinFile *file_2)
{
    const char *name_1, *name_2;
    char *ext_1, *ext_2;
    char *ext_1_folded, *ext_2_folded;
    int result;

    name_1 = nolphin_file_peek_display_name (file_1);
    name_2 = nolphin_file_peek_display_name (file_2);

    ext_1 = eel_filename_get_extension_offset (name_1);
    ext_2 = eel_filename_get_extension_offset (name_2);

    if (ext_1 == NULL && ext_2 == NULL) {
        return 0;
    }

    if (ext_1 == NULL) {
        return -1;
    }

    if (ext_2 == NULL) {
        return 1;
    }

    /* Skip the dot */
    ext_1++;
    ext_2++;

    /* Case-insensitive comparison */
    ext_1_folded = g_utf8_casefold (ext_1, -1);
    ext_2_folded = g_utf8_casefold (ext_2, -1);

    result = g_utf8_collate (ext_1_folded, ext_2_folded);

    g_free (ext_1_folded);
    g_free (ext_2_folded);

    return result;
}

static int
compare_by_time (NolphinFile *file_1, NolphinFile *file_2, NolphinDateType type)
{
	/* Sort order:
	 *   Files with unknown times.
	 *   Files with "unknowable" times.
	 *   Files with older times.
	 *   Files with newer times.
	 */

	Knowledge time_known_1, time_known_2;
	time_t time_1, time_2;

	time_1 = 0;
	time_2 = 0;

	time_known_1 = get_time (file_1, &time_1, type);
	time_known_2 = get_time (file_2, &time_2, type);

	if (time_known_1 > time_known_2) {
		return -1;
	}
	if (time_known_1 < time_known_2) {
		return +1;
	}

	/* Now time_known_1 is equal to time_known_2. Check whether
	 * we failed to get modification times for files
	 */
	if(time_known_1 == UNKNOWABLE || time_known_1 == UNKNOWN) {
		return 0;
	}

	if (time_1 < time_2) {
		return -1;
	}
	if (time_1 > time_2) {
		return +1;
	}

	return 0;
}

static int
compare_by_full_path (NolphinFile *file_1, NolphinFile *file_2)
{
	int compare;

	compare = compare_by_directory_name (file_1, file_2);
	if (compare != 0) {
		return compare;
	}
	return compare_by_display_name (file_1, file_2);
}

static gint
compare_by_search_result_count (NolphinFile *file_1,
                                NolphinFile *file_2,
                                gpointer search_dir)
{
    gint count_1 = nolphin_file_get_search_result_count (file_1, search_dir);
    gint count_2 = nolphin_file_get_search_result_count (file_2, search_dir);

    return (count_1 == count_2) ? 0 : (count_1 - count_2);
}

static int
nolphin_file_compare_for_sort_internal (NolphinFile *file_1,
					 NolphinFile *file_2,
					 gboolean directories_first,
					 gboolean favorites_first,
					 gboolean reversed)
{
	gboolean is_directory_1, is_directory_2;
    gboolean pinned_1, pinned_2;
    gboolean favorite_1, favorite_2;

    favorite_1 = nolphin_file_get_is_favorite (file_1);
    favorite_2 = nolphin_file_get_is_favorite (file_2);

    pinned_1 = nolphin_file_get_pinning (file_1);
    pinned_2 = nolphin_file_get_pinning (file_2);
    
    if (favorites_first) {
        if (favorite_1 && !favorite_2) {
            return -1;
        }

        if (favorite_2 && !favorite_1) {
            return +1;
        }
    }

    if (pinned_1 && !pinned_2) {
        return -1;
    }

    if (pinned_2 && !pinned_1) {
        return +1;
    }

	if (directories_first) {
		is_directory_1 = nolphin_file_is_directory (file_1);
		is_directory_2 = nolphin_file_is_directory (file_2);

		if (is_directory_1 && !is_directory_2) {
			return -1;
		}

		if (is_directory_2 && !is_directory_1) {
			return +1;
		}
	}

	if (file_1->details->sort_order < file_2->details->sort_order) {
		return reversed ? 1 : -1;
	} else if (file_1->details->sort_order > file_2->details->sort_order) {
		return reversed ? -1 : 1;
	}

	return 0;
}

/**
 * nolphin_file_compare_for_sort:
 * @file_1: A file object
 * @file_2: Another file object
 * @sort_type: Sort criterion
 * @directories_first: Put all directories before any non-directories
 * @favorites_first: Put all favorited items before any non-favorited items
 * @reversed: Reverse the order of the items, except that
 * the directories_first flag is still respected.
 *
 * Return value: int < 0 if @file_1 should come before file_2 in a
 * sorted list; int > 0 if @file_2 should come before file_1 in a
 * sorted list; 0 if @file_1 and @file_2 are equal for this sort criterion. Note
 * that each named sort type may actually break ties several ways, with the name
 * of the sort criterion being the primary but not only differentiator.
 **/
int
nolphin_file_compare_for_sort (NolphinFile *file_1,
				NolphinFile *file_2,
				NolphinFileSortType sort_type,
				gboolean directories_first,
				gboolean favorites_first,
				gboolean reversed,
                gpointer search_dir)
{
	int result;

	if (file_1 == file_2) {
		return 0;
	}

	result = nolphin_file_compare_for_sort_internal (file_1, file_2, directories_first, favorites_first, reversed);

	if (result == 0) {
		switch (sort_type) {
		case NOLPHIN_FILE_SORT_BY_DISPLAY_NAME:
			result = compare_by_display_name (file_1, file_2);
			if (result == 0) {
				result = compare_by_directory_name (file_1, file_2);
			}
			break;
		case NOLPHIN_FILE_SORT_BY_SIZE:
			/* Compare directory sizes ourselves, then if necessary
			 * use GnomeVFS to compare file sizes.
			 */
			result = compare_by_size (file_1, file_2);
			if (result == 0) {
				result = compare_by_full_path (file_1, file_2);
			}
			break;
		case NOLPHIN_FILE_SORT_BY_TYPE:
			result = compare_by_type (file_1, file_2, FALSE);
			if (result == 0) {
				result = compare_by_full_path (file_1, file_2);
			}
			break;
        case NOLPHIN_FILE_SORT_BY_DETAILED_TYPE:
            result = compare_by_type (file_1, file_2, TRUE);
            if (result == 0) {
                result = compare_by_full_path (file_1, file_2);
            }
            break;
        case NOLPHIN_FILE_SORT_BY_EXTENSION:
            result = compare_by_extension (file_1, file_2);
            if (result == 0) {
                result = compare_by_full_path (file_1, file_2);
            }
            break;
		case NOLPHIN_FILE_SORT_BY_MTIME:
			result = compare_by_time (file_1, file_2, NOLPHIN_DATE_TYPE_MODIFIED);
			if (result == 0) {
				result = compare_by_full_path (file_1, file_2);
			}
			break;
		case NOLPHIN_FILE_SORT_BY_ATIME:
			result = compare_by_time (file_1, file_2, NOLPHIN_DATE_TYPE_ACCESSED);
			if (result == 0) {
				result = compare_by_full_path (file_1, file_2);
			}
			break;
        case NOLPHIN_FILE_SORT_BY_BTIME:
            result = compare_by_time (file_1, file_2, NOLPHIN_DATE_TYPE_CREATED);
            if (result == 0) {
                result = compare_by_full_path (file_1, file_2);
            }
            break;
		case NOLPHIN_FILE_SORT_BY_TRASHED_TIME:
			result = compare_by_time (file_1, file_2, NOLPHIN_DATE_TYPE_TRASHED);
			if (result == 0) {
				result = compare_by_full_path (file_1, file_2);
			}
			break;
        case NOLPHIN_FILE_SORT_BY_SEARCH_RESULT_COUNT:
            result = compare_by_search_result_count (file_1, file_2, search_dir);
            if (result == 0) {
                result = compare_by_full_path (file_1, file_2);
            }
            break;
		case NOLPHIN_FILE_SORT_NONE:
		default:
			g_return_val_if_reached (0);
		}

		if (reversed) {
			result = -result;
		}
	}

	return result;
}

int
nolphin_file_compare_for_sort_by_attribute_q   (NolphinFile                   *file_1,
						 NolphinFile                   *file_2,
						 GQuark                          attribute,
						 gboolean                        directories_first,
						 gboolean                        favorites_first,
						 gboolean                        reversed,
                         gpointer                        search_dir)
{
	int result;

	if (file_1 == file_2) {
		return 0;
	}

	/* Convert certain attributes into NolphinFileSortTypes and use
	 * nolphin_file_compare_for_sort()
	 */
	if (attribute == 0 || attribute == attribute_name_q) {
		return nolphin_file_compare_for_sort (file_1, file_2,
						       NOLPHIN_FILE_SORT_BY_DISPLAY_NAME,
						       directories_first,
						       favorites_first,
						       reversed,
                               search_dir);
	} else if (attribute == attribute_size_q) {
		return nolphin_file_compare_for_sort (file_1, file_2,
						       NOLPHIN_FILE_SORT_BY_SIZE,
						       directories_first,
						       favorites_first,
						       reversed,
                               search_dir);
	} else if (attribute == attribute_type_q) {
		return nolphin_file_compare_for_sort (file_1, file_2,
						       NOLPHIN_FILE_SORT_BY_TYPE,
						       directories_first,
						       favorites_first,
						       reversed,
                               search_dir);
	} else if (attribute == attribute_detailed_type_q) {
        return nolphin_file_compare_for_sort (file_1, file_2,
                               NOLPHIN_FILE_SORT_BY_DETAILED_TYPE,
                               directories_first,
                               favorites_first,
                               reversed,
                               search_dir);
	} else if (attribute == attribute_extension_q) {
        return nolphin_file_compare_for_sort (file_1, file_2,
                               NOLPHIN_FILE_SORT_BY_EXTENSION,
                               directories_first,
                               favorites_first,
                               reversed,
                               search_dir);
	} else if (attribute == attribute_modification_date_q ||
               attribute == attribute_date_modified_q ||
               attribute == attribute_date_modified_with_time_q ||
               attribute == attribute_date_modified_full_q) {
		return nolphin_file_compare_for_sort (file_1, file_2,
						       NOLPHIN_FILE_SORT_BY_MTIME,
						       directories_first,
						       favorites_first,
						       reversed,
                               search_dir);
    } else if (attribute == attribute_accessed_date_q ||
               attribute == attribute_date_accessed_q ||
               attribute == attribute_date_accessed_full_q) {
		return nolphin_file_compare_for_sort (file_1, file_2,
						       NOLPHIN_FILE_SORT_BY_ATIME,
						       directories_first,
						       favorites_first,
						       reversed,
                               search_dir);
    } else if (attribute == attribute_creation_date_q ||
               attribute == attribute_date_created_q ||
               attribute == attribute_date_created_with_time_q ||
               attribute == attribute_date_created_full_q) {
        return nolphin_file_compare_for_sort (file_1, file_2,
                               NOLPHIN_FILE_SORT_BY_BTIME,
                               directories_first,
                               favorites_first,
                               reversed,
                               search_dir);
    } else if (attribute == attribute_trashed_on_q ||
               attribute == attribute_trashed_on_full_q) {
		return nolphin_file_compare_for_sort (file_1, file_2,
						       NOLPHIN_FILE_SORT_BY_TRASHED_TIME,
						       directories_first,
						       favorites_first,
						       reversed,
                               search_dir);
	} else if (attribute == attribute_search_result_count_q) {
        return nolphin_file_compare_for_sort (file_1, file_2,
                               NOLPHIN_FILE_SORT_BY_SEARCH_RESULT_COUNT,
                               directories_first,
                               favorites_first,
                               reversed,
                               search_dir);
    }

	result = nolphin_file_compare_for_sort_internal (file_1, file_2, directories_first, favorites_first, reversed);

	if (result == 0) {
		result = strcmp (nolphin_file_peek_sort_collation_key (file_1, attribute),
				 nolphin_file_peek_sort_collation_key (file_2, attribute));

		if (result == 0) {
			result = compare_by_full_path (file_1, file_2);
		}

		if (reversed) {
			result = -result;
		}
	}

	return result;
}

int
nolphin_file_compare_for_sort_by_attribute     (NolphinFile                   *file_1,
						 NolphinFile                   *file_2,
						 const char                     *attribute,
						 gboolean                        directories_first,
						 gboolean                        favorites_first,
						 gboolean                        reversed,
                         gpointer                        search_dir)
{
	return nolphin_file_compare_for_sort_by_attribute_q (file_1, file_2,
							      g_quark_from_string (attribute),
							      directories_first,
							      favorites_first,
							      reversed,
                                  search_dir);
}

/* ---- §16 Gruppierung ---- */

gboolean
nolphin_file_sort_type_is_valid_group_type (NolphinFileSortType group_type)
{
	switch (group_type) {
	case NOLPHIN_FILE_SORT_BY_DISPLAY_NAME:
	case NOLPHIN_FILE_SORT_BY_SIZE:
	case NOLPHIN_FILE_SORT_BY_TYPE:
	case NOLPHIN_FILE_SORT_BY_MTIME:
		return TRUE;
	default:
		return FALSE;
	}
}

/* Groups by first letter (upper-cased) of the display name, with two
 * catch-all buckets: "#" for names that don't start with a letter or
 * digit, and "0-9" for names that start with a digit. @out_order is
 * only meaningful for those two catch-alls (0 and 1); letter buckets
 * all share order 2 and are then distinguished from each other by a
 * plain string comparison of the label itself. */
static char *
group_key_for_name (NolphinFile *file, int *out_order)
{
	const char *name;
	gunichar first;

	name = nolphin_file_peek_display_name (file);

	if (name == NULL || name[0] == '\0') {
		*out_order = 0;
		return g_strdup ("#");
	}

	first = g_utf8_get_char_validated (name, -1);

	if (first == (gunichar) -1 || first == (gunichar) -2) {
		*out_order = 0;
		return g_strdup ("#");
	}

	if (g_unichar_isdigit (first)) {
		*out_order = 1;
		return g_strdup ("0-9");
	}

	if (g_unichar_isalpha (first)) {
		gunichar upper;
		char buf[6];
		int len;

		upper = g_unichar_toupper (first);
		len = g_unichar_to_utf8 (upper, buf);
		buf[len] = '\0';

		*out_order = 2;
		return g_strdup (buf);
	}

	*out_order = 0;
	return g_strdup ("#");
}

static char *
group_key_for_type (NolphinFile *file, int *out_order)
{
	if (nolphin_file_is_directory (file)) {
		*out_order = 0;
		return g_strdup (_("Ordner"));
	}

	*out_order = 1;
	return nolphin_file_get_type_as_string (file);
}

static char *
group_key_for_size (NolphinFile *file, int *out_order)
{
	goffset size;

	if (nolphin_file_is_directory (file)) {
		*out_order = 0;
		return g_strdup (_("Ordner"));
	}

	size = nolphin_file_get_size (file);

	if (size < 0) {
		*out_order = 5;
		return g_strdup (_("Unbekannte Größe"));
	}
	if (size < 100 * 1024) {
		*out_order = 1;
		return g_strdup (_("Winzig (unter 100 KB)"));
	}
	if (size < 10 * 1024 * 1024) {
		*out_order = 2;
		return g_strdup (_("Klein (unter 10 MB)"));
	}
	if (size < 1024 * 1024 * 1024) {
		*out_order = 3;
		return g_strdup (_("Mittel (unter 1 GB)"));
	}
	*out_order = 4;
	return g_strdup (_("Groß (1 GB oder mehr)"));
}

/* Day-granularity difference between @mtime's local calendar day and
 * today's local calendar day, in whole days. May be off by one around
 * a DST transition - acceptable for a UX grouping bucket, not exact
 * date arithmetic. */
static long
days_before_today (time_t mtime)
{
	time_t now;
	struct tm tm_now;
	struct tm tm_file;
	time_t midnight_now;
	time_t midnight_file;

	now = time (NULL);
	localtime_r (&now, &tm_now);
	localtime_r (&mtime, &tm_file);

	tm_now.tm_hour = 0;
	tm_now.tm_min = 0;
	tm_now.tm_sec = 0;
	midnight_now = mktime (&tm_now);

	tm_file.tm_hour = 0;
	tm_file.tm_min = 0;
	tm_file.tm_sec = 0;
	midnight_file = mktime (&tm_file);

	return (long) ((midnight_now - midnight_file) / 86400);
}

static char *
group_key_for_mtime (NolphinFile *file, int *out_order)
{
	time_t mtime;
	long diff_days;
	struct tm tm_now;
	struct tm tm_file;
	time_t now;

	mtime = nolphin_file_get_mtime (file);

	if (mtime <= 0) {
		*out_order = 6;
		return g_strdup (_("Unbekanntes Datum"));
	}

	diff_days = days_before_today (mtime);

	if (diff_days < 0) {
		/* File modified "in the future" (clock skew, restored
		 * backup, ...) - treat like today rather than inventing
		 * a separate bucket for it. */
		diff_days = 0;
	}

	if (diff_days == 0) {
		*out_order = 0;
		return g_strdup (_("Heute"));
	}
	if (diff_days == 1) {
		*out_order = 1;
		return g_strdup (_("Gestern"));
	}
	if (diff_days <= 7) {
		*out_order = 2;
		return g_strdup (_("Diese Woche"));
	}

	now = time (NULL);
	localtime_r (&now, &tm_now);
	localtime_r (&mtime, &tm_file);

	if (tm_now.tm_year == tm_file.tm_year && tm_now.tm_mon == tm_file.tm_mon) {
		*out_order = 3;
		return g_strdup (_("Diesen Monat"));
	}
	if (tm_now.tm_year == tm_file.tm_year) {
		*out_order = 4;
		return g_strdup (_("Dieses Jahr"));
	}

	*out_order = 5;
	return g_strdup (_("Älter"));
}

static char *
group_key_for_type_internal (NolphinFile *file, NolphinFileSortType group_type, int *out_order)
{
	switch (group_type) {
	case NOLPHIN_FILE_SORT_BY_DISPLAY_NAME:
		return group_key_for_name (file, out_order);
	case NOLPHIN_FILE_SORT_BY_SIZE:
		return group_key_for_size (file, out_order);
	case NOLPHIN_FILE_SORT_BY_TYPE:
		return group_key_for_type (file, out_order);
	case NOLPHIN_FILE_SORT_BY_MTIME:
		return group_key_for_mtime (file, out_order);
	default:
		g_return_val_if_reached (NULL);
	}
}

char *
nolphin_file_get_group_key (NolphinFile *file, NolphinFileSortType group_type)
{
	int order;

	g_return_val_if_fail (nolphin_file_sort_type_is_valid_group_type (group_type), NULL);

	return group_key_for_type_internal (file, group_type, &order);
}

int
nolphin_file_compare_for_group (NolphinFile *file_1, NolphinFile *file_2, NolphinFileSortType group_type)
{
	char *key_1, *key_2;
	int order_1, order_2;
	int result;

	g_return_val_if_fail (nolphin_file_sort_type_is_valid_group_type (group_type), 0);

	if (file_1 == file_2) {
		return 0;
	}

	key_1 = group_key_for_type_internal (file_1, group_type, &order_1);
	key_2 = group_key_for_type_internal (file_2, group_type, &order_2);

	if (order_1 != order_2) {
		result = (order_1 < order_2) ? -1 : 1;
	} else {
		result = g_utf8_collate (key_1, key_2);
	}

	g_free (key_1);
	g_free (key_2);

	return result;
}

/* ---- §17 Auswahl nach Dateityp ---- */

const char *
nolphin_file_type_category_get_label (NolphinFileTypeCategory category)
{
	switch (category) {
	case NOLPHIN_FILE_TYPE_CATEGORY_FOLDER:
		return _("Ordner");
	case NOLPHIN_FILE_TYPE_CATEGORY_IMAGE:
		return _("Bilder");
	case NOLPHIN_FILE_TYPE_CATEGORY_VIDEO:
		return _("Videos");
	case NOLPHIN_FILE_TYPE_CATEGORY_AUDIO:
		return _("Ton");
	case NOLPHIN_FILE_TYPE_CATEGORY_TEXT:
		return _("Textdateien");
	case NOLPHIN_FILE_TYPE_CATEGORY_ARCHIVE:
		return _("Archive");
	case NOLPHIN_FILE_TYPE_CATEGORY_OTHER:
	default:
		return _("Sonstiges");
	}
}

gboolean
nolphin_file_matches_type_category (NolphinFile *file, NolphinFileTypeCategory category)
{
	char *mime_type;
	gboolean result;

	g_return_val_if_fail (NOLPHIN_IS_FILE (file), FALSE);

	if (category == NOLPHIN_FILE_TYPE_CATEGORY_FOLDER) {
		return nolphin_file_is_directory (file);
	}

	if (nolphin_file_is_directory (file)) {
		/* A directory can only ever match the FOLDER category
		 * above - none of the content-based categories apply. */
		return FALSE;
	}

	if (category == NOLPHIN_FILE_TYPE_CATEGORY_ARCHIVE) {
		GFile *location;
		NolphinArchiveFormat format;

		location = nolphin_file_get_location (file);
		format = nolphin_archive_detect_format (location);
		g_object_unref (location);

		return format != NOLPHIN_ARCHIVE_FORMAT_UNKNOWN;
	}

	mime_type = nolphin_file_get_mime_type (file);
	if (mime_type == NULL) {
		return FALSE;
	}

	switch (category) {
	case NOLPHIN_FILE_TYPE_CATEGORY_IMAGE:
		result = g_str_has_prefix (mime_type, "image/");
		break;
	case NOLPHIN_FILE_TYPE_CATEGORY_VIDEO:
		result = g_str_has_prefix (mime_type, "video/");
		break;
	case NOLPHIN_FILE_TYPE_CATEGORY_AUDIO:
		result = g_str_has_prefix (mime_type, "audio/");
		break;
	case NOLPHIN_FILE_TYPE_CATEGORY_TEXT:
		result = g_str_has_prefix (mime_type, "text/");
		break;
	case NOLPHIN_FILE_TYPE_CATEGORY_OTHER:
		result = !g_str_has_prefix (mime_type, "image/") &&
			 !g_str_has_prefix (mime_type, "video/") &&
			 !g_str_has_prefix (mime_type, "audio/") &&
			 !g_str_has_prefix (mime_type, "text/") &&
			 !nolphin_file_matches_type_category (file, NOLPHIN_FILE_TYPE_CATEGORY_ARCHIVE);
		break;
	default:
		result = FALSE;
		break;
	}

	g_free (mime_type);
	return result;
}

/**
 * nolphin_file_compare_name:
 * @file: A file object
 * @pattern: A string we are comparing it with
 *
 * Return value: result of a comparison of the file name and the given pattern,
 * using the same sorting order as sort by name.
 **/
int
nolphin_file_compare_display_name (NolphinFile *file,
				    const char *pattern)
{
	const char *name;
	int result;

	g_return_val_if_fail (pattern != NULL, -1);

	name = nolphin_file_peek_display_name (file);
	result = g_utf8_collate (name, pattern);
	return result;
}


gboolean
nolphin_file_is_hidden_file (NolphinFile *file)
{
	return file->details->is_hidden;
}

/**
 * nolphin_file_should_show:
 * @file: the file to check.
 * @show_hidden: whether we want to show hidden files or not.
 *
 * Determines if a #NolphinFile should be shown. Note that when browsing
 * a trash directory, this function will always return %TRUE.
 *
 * Returns: %TRUE if the file should be shown, %FALSE if it shouldn't.
 */
gboolean
nolphin_file_should_show (NolphinFile *file,
			   gboolean show_hidden,
			   gboolean show_foreign)
{
	/* Never hide any files in trash. */
	if (nolphin_file_is_in_trash (file)) {
		return TRUE;
	} else {
		return (show_hidden || !nolphin_file_is_hidden_file (file)) &&
			(show_foreign || !(nolphin_file_is_in_desktop (file) && nolphin_file_is_foreign_link (file)));
	}
}

gboolean
nolphin_file_is_home (NolphinFile *file)
{
	GFile *dir;

	dir = file->details->directory->details->location;
	if (dir == NULL) {
		return FALSE;
	}

	return nolphin_is_home_directory_file (dir, file->details->name);
}

gboolean
nolphin_file_is_in_desktop (NolphinFile *file)
{
	if (file->details->directory->details->location) {
		return nolphin_is_desktop_directory (file->details->directory->details->location);
	}
	return FALSE;
}

static gboolean
filter_hidden_partition_callback (gpointer data,
				  gpointer callback_data)
{
	NolphinFile *file;
	FilterOptions options;

	file = NOLPHIN_FILE (data);
	options = GPOINTER_TO_INT (callback_data);

	return nolphin_file_should_show (file,
					  options & SHOW_HIDDEN,
					  TRUE);
}

GList *
nolphin_file_list_filter_hidden (GList    *files,
				  gboolean  show_hidden)
{
	GList *filtered_files;
	GList *removed_files;

	/* FIXME bugzilla.gnome.org 40653:
	 * Eventually this should become a generic filtering thingy.
	 */

	filtered_files = nolphin_file_list_copy (files);
	filtered_files = eel_g_list_partition (filtered_files,
					       filter_hidden_partition_callback,
					       GINT_TO_POINTER ((show_hidden ? SHOW_HIDDEN : 0)),
					       &removed_files);
	nolphin_file_list_free (removed_files);

	return filtered_files;
}

char *
nolphin_file_get_metadata (NolphinFile *file,
			    const char *key,
			    const char *default_metadata)
{
	guint id;
	char *value;

	g_return_val_if_fail (key != NULL, g_strdup (default_metadata));
	g_return_val_if_fail (key[0] != '\0', g_strdup (default_metadata));

	if (file == NULL ||
	    file->details->metadata == NULL) {
		return g_strdup (default_metadata);
	}

	g_return_val_if_fail (NOLPHIN_IS_FILE (file), g_strdup (default_metadata));

    if (NOLPHIN_FILE_GET_CLASS (file)->get_metadata) {
        value = NOLPHIN_FILE_GET_CLASS (file)->get_metadata (file, key);
        if (value) {
            return value;
        } else {
            return g_strdup (default_metadata);
        }
    }

	id = nolphin_metadata_get_id (key);
	value = g_hash_table_lookup (file->details->metadata, GUINT_TO_POINTER (id));

	if (value) {
		return g_strdup (value);
	}
	return g_strdup (default_metadata);
}

GList *
nolphin_file_get_metadata_list (NolphinFile *file,
				 const char *key)
{
	GList *res;
	guint id;
	char **value;

	g_return_val_if_fail (key != NULL, NULL);
	g_return_val_if_fail (key[0] != '\0', NULL);

	if (file == NULL ||
	    file->details->metadata == NULL) {
		return NULL;
	}

	g_return_val_if_fail (NOLPHIN_IS_FILE (file), NULL);

    if (NOLPHIN_FILE_GET_CLASS (file)->get_metadata_as_list) {
        gchar **meta_strv = NOLPHIN_FILE_GET_CLASS (file)->get_metadata_as_list (file, key);

        res = eel_strv_to_glist (meta_strv);

        g_strfreev (meta_strv);
        return res;
    }

	id = nolphin_metadata_get_id (key);
	id |= METADATA_ID_IS_LIST_MASK;

	value = g_hash_table_lookup (file->details->metadata, GUINT_TO_POINTER (id));

	if (value) {
        return eel_strv_to_glist (value);
	}

	return NULL;
}

void
nolphin_file_set_metadata (NolphinFile *file,
			    const char *key,
			    const char *default_metadata,
			    const char *metadata)
{
	const char *val;

	g_return_if_fail (NOLPHIN_IS_FILE (file));
	g_return_if_fail (key != NULL);
	g_return_if_fail (key[0] != '\0');

	val = metadata;
	if (val == NULL) {
		val = default_metadata;
	}

	NOLPHIN_FILE_CLASS (G_OBJECT_GET_CLASS (file))->set_metadata (file, key, val);
}

void
nolphin_file_set_metadata_list (NolphinFile *file,
				 const char *key,
				 GList *list)
{
	char **val;
	int len, i;
	GList *l;

	g_return_if_fail (NOLPHIN_IS_FILE (file));
	g_return_if_fail (key != NULL);
	g_return_if_fail (key[0] != '\0');

	len = g_list_length (list);
	val = g_new (char *, len + 1);
	for (l = list, i = 0; l != NULL; l = l->next, i++) {
		val[i] = l->data;
	}
	val[i] = NULL;

	NOLPHIN_FILE_CLASS (G_OBJECT_GET_CLASS (file))->set_metadata_as_list (file, key, val);

	g_free (val);
}

void
nolphin_file_set_desktop_grid_adjusts (NolphinFile   *file,
                                    const char *key,
                                    int         int_a,
                                    int         int_b)
{
    char **val;

    g_return_if_fail (NOLPHIN_IS_FILE (file));
    g_return_if_fail (key != NULL);
    g_return_if_fail (key[0] != '\0');

    val = g_new (char *, 3);

    val[0] = g_strdup_printf ("%d", int_a);
    val[1] = g_strdup_printf ("%d", int_b);
    val[2] = NULL;

    NOLPHIN_FILE_CLASS (G_OBJECT_GET_CLASS (file))->set_metadata_as_list (file, key, val);

    g_strfreev ((gchar **) val);
}

void
nolphin_file_get_desktop_grid_adjusts (NolphinFile   *file,
                                    const char *key,
                                    int        *int_a,
                                    int        *int_b)
{
    char c;
    gint result;

    g_return_if_fail (key != NULL);
    g_return_if_fail (key[0] != '\0');

    if (file == NULL ||
        file->details->metadata == NULL) {
        return;
    }

    g_return_if_fail (NOLPHIN_IS_FILE (file));

    if (NOLPHIN_FILE_GET_CLASS (file)->get_metadata_as_list) {
        gchar **meta_strv = NOLPHIN_FILE_GET_CLASS (file)->get_metadata_as_list (file, key);

        if (!meta_strv) {
            if (int_a)
                *int_a = 100;
            if (int_b)
                *int_b = 100;

            return;
        }

        if (int_a) {
            if (sscanf (meta_strv[0], " %d %c", &result, &c) == 1) {
                *int_a = result;
            }
        }

        if (int_b) {
            if (sscanf (meta_strv[1], " %d %c", &result, &c) == 1) {
                *int_b = result;
            }
        }

        g_strfreev (meta_strv);
    }
}

gboolean
nolphin_file_get_boolean_metadata (NolphinFile *file,
				    const char   *key,
				    gboolean      default_metadata)
{
	char *result_as_string;
	gboolean result;

	g_return_val_if_fail (key != NULL, default_metadata);
	g_return_val_if_fail (key[0] != '\0', default_metadata);

	if (file == NULL) {
		return default_metadata;
	}

	g_return_val_if_fail (NOLPHIN_IS_FILE (file), default_metadata);

	result_as_string = nolphin_file_get_metadata
		(file, key, default_metadata ? "true" : "false");
	g_assert (result_as_string != NULL);

	if (g_ascii_strcasecmp (result_as_string, "true") == 0) {
		result = TRUE;
	} else if (g_ascii_strcasecmp (result_as_string, "false") == 0) {
		result = FALSE;
	} else {
		g_error ("boolean metadata with value other than true or false");
		result = default_metadata;
	}

	g_free (result_as_string);
	return result;
}

int
nolphin_file_get_integer_metadata (NolphinFile *file,
				    const char   *key,
				    int           default_metadata)
{
	char *result_as_string;
	char default_as_string[32];
	int result;
	char c;

	g_return_val_if_fail (key != NULL, default_metadata);
	g_return_val_if_fail (key[0] != '\0', default_metadata);

	if (file == NULL) {
		return default_metadata;
	}
	g_return_val_if_fail (NOLPHIN_IS_FILE (file), default_metadata);

	g_snprintf (default_as_string, sizeof (default_as_string), "%d", default_metadata);
	result_as_string = nolphin_file_get_metadata
		(file, key, default_as_string);

	/* Normally we can't get a a NULL, but we check for it here to
	 * handle the oddball case of a non-existent directory.
	 */
	if (result_as_string == NULL) {
		result = default_metadata;
	} else {
		if (sscanf (result_as_string, " %d %c", &result, &c) != 1) {
			result = default_metadata;
		}
		g_free (result_as_string);
	}

	return result;
}

static gboolean
get_time_from_time_string (const char *time_string,
			   time_t *time)
{
	long scanned_time;
	char c;

	g_assert (time != NULL);

	/* Only accept string if it has one integer with nothing
	 * afterwards.
	 */
	if (time_string == NULL ||
	    sscanf (time_string, "%ld%c", &scanned_time, &c) != 1) {
		return FALSE;
	}
	*time = (time_t) scanned_time;
	return TRUE;
}

time_t
nolphin_file_get_time_metadata (NolphinFile *file,
				 const char   *key)
{
	time_t time;
	char *time_string;

	time_string = nolphin_file_get_metadata (file, key, NULL);
	if (!get_time_from_time_string (time_string, &time)) {
		time = UNDEFINED_TIME;
	}
	g_free (time_string);

	return time;
}

void
nolphin_file_set_time_metadata (NolphinFile *file,
				 const char   *key,
				 time_t        time)
{
	char time_str[21];
	char *metadata;

	if (time != UNDEFINED_TIME) {
		/* 2^64 turns out to be 20 characters */
		g_snprintf (time_str, 20, "%ld", (long int)time);
		time_str[20] = '\0';
		metadata = time_str;
	} else {
		metadata = NULL;
	}

	nolphin_file_set_metadata (file, key, NULL, metadata);
}


void
nolphin_file_set_boolean_metadata (NolphinFile *file,
				    const char   *key,
				    gboolean      default_metadata,
				    gboolean      metadata)
{
	g_return_if_fail (NOLPHIN_IS_FILE (file));
	g_return_if_fail (key != NULL);
	g_return_if_fail (key[0] != '\0');

	nolphin_file_set_metadata (file, key,
				    default_metadata ? "true" : "false",
				    metadata ? "true" : "false");
}

void
nolphin_file_set_integer_metadata (NolphinFile *file,
				    const char   *key,
				    int           default_metadata,
				    int           metadata)
{
	char value_as_string[32];
	char default_as_string[32];

	g_return_if_fail (NOLPHIN_IS_FILE (file));
	g_return_if_fail (key != NULL);
	g_return_if_fail (key[0] != '\0');

	g_snprintf (value_as_string, sizeof (value_as_string), "%d", metadata);
	g_snprintf (default_as_string, sizeof (default_as_string), "%d", default_metadata);

	nolphin_file_set_metadata (file, key,
				    default_as_string, value_as_string);
}

static const char *
nolphin_file_peek_display_name_collation_key (NolphinFile *file)
{
	const char *res;

	res = file->details->display_name_collation_key;
	if (res == NULL)
		res = "";

	return res;
}

/* The returned key points into the file's one-entry cache, so it is only valid
 * until the next call for that same file with a different attribute, or until the
 * file's key is invalidated. Peeking two attributes of one file needs a copy.
 */
static const char *
nolphin_file_peek_sort_collation_key (NolphinFile *file, GQuark attribute)
{
	if (file->details->sort_collation_attribute != attribute ||
	    file->details->sort_collation_key == NULL) {
		char *value;
		char *valid;

		value = nolphin_file_get_string_attribute_q (file, attribute);

		/* Columns can supply arbitrary bytes, and keys are only
		 * comparable to other keys, so every value has to produce one. */
		valid = g_utf8_make_valid (value != NULL ? value : "", -1);

		g_free (file->details->sort_collation_key);
		file->details->sort_collation_key = g_utf8_collate_key_for_filename (valid, -1);
		file->details->sort_collation_attribute = attribute;

		g_free (valid);
		g_free (value);
	}

	return file->details->sort_collation_key;
}

static void
nolphin_file_invalidate_sort_collation_key (NolphinFile *file)
{
	file->details->sort_collation_attribute = 0;
	g_clear_pointer (&file->details->sort_collation_key, g_free);
}

static const char *
nolphin_file_peek_display_name (NolphinFile *file)
{
	const char *name;
	char *escaped_name;

	/* FIXME: for some reason we can get a NolphinFile instance which is
	 *        no longer valid or could be freed somewhere else in the same time.
	 *        There's race condition somewhere. See bug 602500.
	 */
	if (file == NULL || nolphin_file_is_gone (file))
		return "";

	/* Default to display name based on filename if its not set yet */

	if (file->details->display_name == NULL) {
		name = file->details->name;
		if (g_utf8_validate (name, -1, NULL)) {
			nolphin_file_set_display_name (file,
							name,
							NULL,
							FALSE);
		} else {
			escaped_name = g_uri_escape_string (name, G_URI_RESERVED_CHARS_ALLOWED_IN_PATH, TRUE);
			nolphin_file_set_display_name (file,
							escaped_name,
							NULL,
							FALSE);
			g_free (escaped_name);
		}
	}

	return file->details->display_name;
}

char *
nolphin_file_get_display_name (NolphinFile *file)
{
	return g_strdup (nolphin_file_peek_display_name (file));
}

char *
nolphin_file_get_edit_name (NolphinFile *file)
{
	const char *res;

	res = file->details->edit_name;
	if (res == NULL)
		res = "";

	return g_strdup (res);
}

const char *
nolphin_file_peek_name (NolphinFile *file)
{
    return file->details->name;
}

char *
nolphin_file_get_name (NolphinFile *file)
{
	return g_strdup (file->details->name);
}

/**
 * nolphin_file_get_description:
 * @file: a #NolphinFile.
 *
 * Gets the standard::description key from @file, if
 * it has been cached.
 *
 * Returns: a string containing the value of the standard::description
 * 	key, or %NULL.
 */
char *
nolphin_file_get_description (NolphinFile *file)
{
	return g_strdup (file->details->description);
}

void
nolphin_file_monitor_add (NolphinFile *file,
			   gconstpointer client,
			   NolphinFileAttributes attributes)
{
	g_return_if_fail (NOLPHIN_IS_FILE (file));
	g_return_if_fail (client != NULL);

	NOLPHIN_FILE_CLASS (G_OBJECT_GET_CLASS (file))->monitor_add (file, client, attributes);
}

void
nolphin_file_monitor_remove (NolphinFile *file,
			      gconstpointer client)
{
	g_return_if_fail (NOLPHIN_IS_FILE (file));
	g_return_if_fail (client != NULL);

	NOLPHIN_FILE_CLASS (G_OBJECT_GET_CLASS (file))->monitor_remove (file, client);
}

gboolean
nolphin_file_is_launcher (NolphinFile *file)
{
	return file->details->is_launcher;
}

gboolean
nolphin_file_is_foreign_link (NolphinFile *file)
{
	return file->details->is_foreign_link;
}

gboolean
nolphin_file_is_trusted_link (NolphinFile *file)
{
	return file->details->is_trusted_link;
}

gboolean
nolphin_file_has_activation_uri (NolphinFile *file)
{
	return file->details->activation_uri != NULL;
}


/* Return the uri associated with the passed-in file, which may not be
 * the actual uri if the file is an desktop file or a nolphin
 * xml link file.
 */
char *
nolphin_file_get_activation_uri (NolphinFile *file)
{
	g_return_val_if_fail (NOLPHIN_IS_FILE (file), NULL);

	if (file->details->activation_uri != NULL) {
		return g_strdup (file->details->activation_uri);
	}

	return nolphin_file_get_uri (file);
}

GFile *
nolphin_file_get_activation_location (NolphinFile *file)
{
	g_return_val_if_fail (NOLPHIN_IS_FILE (file), NULL);

	if (file->details->activation_uri != NULL) {
		return g_file_new_for_uri (file->details->activation_uri);
	}

	return nolphin_file_get_location (file);
}


char *
nolphin_file_get_drop_target_uri (NolphinFile *file)
{
	char *uri, *target_uri;
	GFile *location;
	NolphinDesktopLink *link;

	g_return_val_if_fail (NOLPHIN_IS_FILE (file), NULL);

	if (NOLPHIN_IS_DESKTOP_ICON_FILE (file)) {
		link = nolphin_desktop_icon_file_get_link (NOLPHIN_DESKTOP_ICON_FILE (file));

		if (link != NULL) {
			location = nolphin_desktop_link_get_activation_location (link);
			g_object_unref (link);
			if (location != NULL) {
				uri = g_file_get_uri (location);
				g_object_unref (location);
				return uri;
			}
		}
	}

	uri = nolphin_file_get_uri (file);

	/* Check for Nolphin link */
	if (nolphin_file_is_nolphin_link (file)) {
		location = nolphin_file_get_location (file);
		/* FIXME bugzilla.gnome.org 43020: This does sync. I/O and works only locally. */
		if (g_file_is_native (location)) {
			target_uri = nolphin_link_local_get_link_uri (uri);
			if (target_uri != NULL) {
				g_free (uri);
				uri = target_uri;
			}
		}
		g_object_unref (location);
	}

	return uri;
}

static gboolean
is_uri_relative (const char *uri)
{
	char *scheme;
	gboolean ret;

	scheme = g_uri_parse_scheme (uri);
	ret = (scheme == NULL);
	g_free (scheme);
	return ret;
}

static char *
get_custom_icon_metadata_uri (NolphinFile *file)
{
	char *custom_icon_uri;
	char *uri;
	char *dir_uri;

	uri = nolphin_file_get_metadata (file, NOLPHIN_METADATA_KEY_CUSTOM_ICON, NULL);
	if (uri != NULL &&
	    nolphin_file_is_directory (file) &&
	    is_uri_relative (uri)) {
		dir_uri = nolphin_file_get_uri (file);
		custom_icon_uri = g_build_filename (dir_uri, uri, NULL);
		g_free (dir_uri);
		g_free (uri);
	} else {
		custom_icon_uri = uri;
	}
	return custom_icon_uri;
}

static char *
get_custom_icon_metadata_name (NolphinFile *file)
{
	char *icon_name;

	icon_name = nolphin_file_get_metadata (file,
						NOLPHIN_METADATA_KEY_CUSTOM_ICON_NAME, NULL);

	return icon_name;
}

static GIcon *
get_custom_icon (NolphinFile *file)
{
	char *custom_icon_uri, *custom_icon_name;
	GFile *icon_file;
	GIcon *icon;

	if (file == NULL) {
		return NULL;
	}

	icon = NULL;

	/* Metadata takes precedence; first we look at the custom
	 * icon URI, then at the custom icon name.
	 */
	custom_icon_uri = get_custom_icon_metadata_uri (file);

	if (custom_icon_uri) {
		icon_file = g_file_new_for_uri (custom_icon_uri);
		icon = g_file_icon_new (icon_file);
		g_object_unref (icon_file);
		g_free (custom_icon_uri);
	}

	if (icon == NULL) {
		custom_icon_name = get_custom_icon_metadata_name (file);

		if (custom_icon_name != NULL) {
			icon = g_themed_icon_new (custom_icon_name);
			g_free (custom_icon_name);
		}
	}

	if (icon == NULL && file->details->got_link_info && file->details->custom_icon != NULL) {
		icon = g_object_ref (file->details->custom_icon);
 	}

	return icon;
}

GFilesystemPreviewType
nolphin_file_get_filesystem_use_preview (NolphinFile *file)
{
	GFilesystemPreviewType use_preview;
	NolphinFile *parent;

	parent = nolphin_file_get_parent (file);
	if (parent != NULL) {
		use_preview = parent->details->filesystem_use_preview;
		g_object_unref (parent);
	} else {
		use_preview = 0;
	}

	return use_preview;
}

static gboolean
is_local_favorite (NolphinFile *file)
{
    GFile *fav_file = nolphin_file_get_location (file);
    gboolean ret;

    // This will be a FavoriteVfsFile - its native check checks the target file.
    ret = g_file_is_native (fav_file);
    g_object_unref (fav_file);

    return ret;
}

static gboolean
file_is_cad_model (NolphinFile *file)
{
	g_autofree gchar *mime_type = nolphin_file_get_mime_type (file);
	g_autofree gchar *name = g_ascii_strdown (nolphin_file_peek_display_name (file), -1);

	if (g_str_has_suffix (name, ".fcstd") || g_str_has_suffix (name, ".fcbak")) {
		return TRUE;
	}
	return mime_type != NULL &&
	       (g_str_has_prefix (mime_type, "model/") ||
	        g_str_equal (mime_type, "application/vnd.step") ||
	        g_str_equal (mime_type, "application/vnd.brep") ||
	        g_str_equal (mime_type, "application/sla"));
}

gboolean
nolphin_file_should_show_thumbnail (NolphinFile *file)
{
	GFilesystemPreviewType use_preview;
    NolphinFile *dir = NULL;
    char* metadata_str = NULL;

    if (!NOLPHIN_IS_FILE (file)) {
        return FALSE;
    }

	use_preview = nolphin_file_get_filesystem_use_preview (file);
    if (use_preview == G_FILESYSTEM_PREVIEW_TYPE_NEVER && !file->details->has_preview_icon) {
        /* file system says to never thumbnail anything */
		return FALSE;
	}
    
    /* Only care about the file size, if the thumbnail has not been created yet.
     * 3D/CAD-Dateien sind ausgenommen: sie sind fast immer größer als das
     * Bildlimit, werden aber extern gerendert und nicht im Prozess dekodiert. */
	if (file->details->thumbnail_path == NULL &&
	    nolphin_file_get_size (file) > cached_thumbnail_limit &&
	    !file_is_cad_model (file)) {
		return FALSE;
	}

    if (file->details->thumbnail_access_problem) {
        return FALSE;
    }

    if (!nolphin_global_preferences_get_ignore_view_metadata ()) {
        if (nolphin_file_is_directory (file)) {
            dir = nolphin_file_ref (file);
        } else {
            dir = nolphin_file_get_parent (file);
        }

        if (nolphin_global_preferences_get_inherit_show_thumbnails_preference ()) {
            NolphinFile *tmp = NULL;

            while (dir != NULL) {
                metadata_str = nolphin_file_get_metadata(dir,
                                                    NOLPHIN_METADATA_KEY_SHOW_THUMBNAILS,
                                                    NULL);
                if (metadata_str == NULL) { // do this here to avoid string comparisons with a NULL string
                    tmp = nolphin_file_get_parent (dir);
                    nolphin_file_unref (dir);
                    dir = tmp;
                }
                else if (g_ascii_strcasecmp (metadata_str, "true") == 0) {
                    g_free(metadata_str);
                    nolphin_file_unref (dir);
                    return TRUE;
                }
                else if (g_ascii_strcasecmp (metadata_str, "false") == 0) {
                    g_free(metadata_str);
                    nolphin_file_unref (dir);
                    return FALSE;
                }
                else {
                    g_free(metadata_str);
                    tmp = nolphin_file_get_parent (dir);
                    nolphin_file_unref (dir);
                    dir = tmp;
                }
            }
        } else {
            metadata_str = nolphin_file_get_metadata (dir,
                                                   NOLPHIN_METADATA_KEY_SHOW_THUMBNAILS,
                                                   NULL);
            nolphin_file_unref (dir);
            if (metadata_str != NULL ) {
                if (g_ascii_strcasecmp (metadata_str, "true") == 0) {
                    g_free(metadata_str);
                    return TRUE;
                }
                else if (g_ascii_strcasecmp (metadata_str, "false") == 0) {
                    g_free(metadata_str);
                    return FALSE;
                }
            }
        }
    }

    /* Use global preference */
    if (show_image_thumbs == NOLPHIN_SPEED_TRADEOFF_ALWAYS) {
        return TRUE;
    }
    if (show_image_thumbs == NOLPHIN_SPEED_TRADEOFF_NEVER) {
        return FALSE;
    }
    if (use_preview == G_FILESYSTEM_PREVIEW_TYPE_IF_LOCAL) {
        // favorites:/// can have local and non-local files.
        // we check if each individual file is local (nolphin_file_is_local()
        // checks if the parent is native, which for favorites:///
        // is false.)
        if (nolphin_file_is_in_favorites (file)) {
            return is_local_favorite (file);
        }

        /* file system says we should treat file as if it's local */
        return TRUE;
    }
    /* local files is the only left to check */
    return nolphin_file_is_local (file);
}

static void
delete_failed_thumbnail_marker (NolphinFile *file)
{
    gchar *uri, *filename, *path;
    GChecksum *checksum;
    guint8 digest[16];
    gsize digest_len = sizeof (digest);
    gint success;

    uri = nolphin_file_get_uri (file);
    checksum = g_checksum_new (G_CHECKSUM_MD5);
    g_checksum_update (checksum, (const guchar *) uri, strlen (uri));

    g_checksum_get_digest (checksum, digest, &digest_len);
    g_assert (digest_len == 16);

    filename = g_strconcat (g_checksum_get_string (checksum), ".png", NULL);
    g_checksum_free (checksum);

    path = g_build_filename (g_get_user_cache_dir (),
                             "thumbnails/fail/gnome-thumbnail-factory",
                             filename,
                             NULL);
    g_free (filename);

    success = g_unlink (path);

    if (success != 0) {
        if (errno != ENOENT && errno != EFAULT) {
            g_warning ("Could not remove failed thumbnail marker for '%s'. The path was '%s'",
                       file->details->display_name,
                       path);
        }
    }

    g_free (uri);
    g_free (path);
}

void
nolphin_file_delete_thumbnail (NolphinFile *file)
{
    if (file->details->thumbnail_path == NULL) {
        if (file->details->thumbnailing_failed) {
            delete_failed_thumbnail_marker (file);
        }

        return;
    }

    gint success;

    nolphin_file_invalidate_attributes (file, NOLPHIN_FILE_ATTRIBUTE_THUMBNAIL);
    g_clear_object (&file->details->thumbnail);

    success = g_unlink (file->details->thumbnail_path);

    if (success != 0) {
        g_warning ("Could not remove thumb for '%s'.  The thumbnail path is '%s'",
                   file->details->display_name,
                   file->details->thumbnail_path);
    }
}

gboolean
nolphin_file_has_loaded_thumbnail (NolphinFile *file)
{
    return file->details->thumbnail_is_up_to_date;
}

static void
prepend_icon_name (const char *name,
		   GThemedIcon *icon)
{
	g_themed_icon_prepend_name(icon, name);
}

GIcon *
nolphin_file_get_gicon (NolphinFile *file,
			 NolphinFileIconFlags flags)
{
	const char * const * names;
	const char *name;
	GPtrArray *prepend_array;
	GMount *mount;
	GIcon *icon, *mount_icon = NULL, *emblemed_icon;
	GEmblem *emblem;
	int i;
	gboolean is_folder = FALSE, is_inode_directory = FALSE;

	if (file == NULL) {
		return NULL;
	}

	icon = get_custom_icon (file);

	if (icon != NULL) {
		return icon;
	}

	if (file->details->icon) {
		icon = NULL;

		/* fetch the mount icon here, we'll use it later */
		if (flags & NOLPHIN_FILE_ICON_FLAGS_USE_MOUNT_ICON ||
		    flags & NOLPHIN_FILE_ICON_FLAGS_USE_MOUNT_ICON_AS_EMBLEM) {
			mount = nolphin_file_get_mount (file);

			if (mount != NULL) {
				mount_icon = g_mount_get_icon (mount);
				g_object_unref (mount);
			}
		}

		if (((flags & NOLPHIN_FILE_ICON_FLAGS_FOR_DRAG_ACCEPT) ||
		     (flags & NOLPHIN_FILE_ICON_FLAGS_FOR_OPEN_FOLDER) ||
		     (flags & NOLPHIN_FILE_ICON_FLAGS_USE_MOUNT_ICON) ||
		     (flags & NOLPHIN_FILE_ICON_FLAGS_USE_MOUNT_ICON_AS_EMBLEM) ||
		     ((flags & NOLPHIN_FILE_ICON_FLAGS_IGNORE_VISITING) == 0 &&
		      nolphin_file_has_open_window (file))) &&
		    G_IS_THEMED_ICON (file->details->icon)) {
			names = g_themed_icon_get_names (G_THEMED_ICON (file->details->icon));
			prepend_array = g_ptr_array_new ();

			for (i = 0; names[i] != NULL; i++) {
				name = names[i];

				if (strcmp (name, "folder") == 0) {
					is_folder = TRUE;
				}
				if (strcmp (name, "inode-directory") == 0) {
					is_inode_directory = TRUE;
				}
			}

			/* Here, we add icons in reverse order of precedence,
			 * because they are later prepended */

			/* "folder" should override "inode-directory", not the other way around */
			if (is_inode_directory) {
				g_ptr_array_add (prepend_array, (char *)"folder");
			}
			if (is_folder && (flags & NOLPHIN_FILE_ICON_FLAGS_FOR_OPEN_FOLDER)) {
				g_ptr_array_add (prepend_array, (char *)"folder-open");
			}
			if (is_folder &&
			    (flags & NOLPHIN_FILE_ICON_FLAGS_IGNORE_VISITING) == 0 &&
			    nolphin_file_has_open_window (file)) {
				g_ptr_array_add (prepend_array, (char *)"folder-visiting");
			}
			if (is_folder &&
			    (flags & NOLPHIN_FILE_ICON_FLAGS_FOR_DRAG_ACCEPT)) {
				g_ptr_array_add (prepend_array, (char *)"folder-drag-accept");
			}

			if (prepend_array->len) {
				/* When constructing GThemed Icon, pointers from the array
				 * are reused, but not the array itself, so the cast is safe */
				icon = g_themed_icon_new_from_names ((char**) names, -1);
				g_ptr_array_foreach (prepend_array, (GFunc) prepend_icon_name, icon);
			}

			g_ptr_array_free (prepend_array, TRUE);
		}

		if (icon == NULL) {
			icon = g_object_ref (file->details->icon);
		}

		if ((flags & NOLPHIN_FILE_ICON_FLAGS_USE_MOUNT_ICON) &&
		    mount_icon != NULL) {
			g_object_unref (icon);
			icon = mount_icon;
		} else if ((flags & NOLPHIN_FILE_ICON_FLAGS_USE_MOUNT_ICON_AS_EMBLEM) &&
			     mount_icon != NULL && !g_icon_equal (mount_icon, icon)) {

			emblem = g_emblem_new (mount_icon);
			emblemed_icon = g_emblemed_icon_new (icon, emblem);

			g_object_unref (emblem);
			g_object_unref (icon);
			g_object_unref (mount_icon);

			icon = emblemed_icon;
		} else if (mount_icon != NULL) {
			g_object_unref (mount_icon);
		}

		return icon;
	}

	return g_themed_icon_new ("text-x-generic");
}


static const gchar *
get_symbolic_icon_name_for_file (NolphinFile *file)
{
    if (nolphin_file_is_home (file)) {
        return NOLPHIN_ICON_SYMBOLIC_HOME;
    }

    // Special folders (XDG)
    if (nolphin_file_is_user_special_directory (file, G_USER_DIRECTORY_DESKTOP)) {
        return NOLPHIN_ICON_SYMBOLIC_DESKTOP;
    }

    if (nolphin_file_is_user_special_directory (file, G_USER_DIRECTORY_DOCUMENTS)) {
        return NOLPHIN_ICON_SYMBOLIC_FOLDER_DOCUMENTS;
    }

    if (nolphin_file_is_user_special_directory (file, G_USER_DIRECTORY_DOWNLOAD)) {
        return NOLPHIN_ICON_SYMBOLIC_FOLDER_DOWNLOAD;
    }

    if (nolphin_file_is_user_special_directory (file, G_USER_DIRECTORY_MUSIC)) {
        return NOLPHIN_ICON_SYMBOLIC_FOLDER_MUSIC;
    }

    if (nolphin_file_is_user_special_directory (file, G_USER_DIRECTORY_PICTURES)) {
        return NOLPHIN_ICON_SYMBOLIC_FOLDER_PICTURES;
    }

    if (nolphin_file_is_user_special_directory (file, G_USER_DIRECTORY_PUBLIC_SHARE)) {
        return NOLPHIN_ICON_SYMBOLIC_FOLDER_PUBLIC_SHARE;
    }

    if (nolphin_file_is_user_special_directory (file, G_USER_DIRECTORY_TEMPLATES)) {
        return NOLPHIN_ICON_SYMBOLIC_FOLDER_TEMPLATES;
    }

    if (nolphin_file_is_user_special_directory (file, G_USER_DIRECTORY_VIDEOS)) {
        return NOLPHIN_ICON_SYMBOLIC_FOLDER_VIDEOS;
    }

    if (nolphin_file_is_user_special_directory (file, G_USER_DIRECTORY_TEMPLATES)) {
        return NOLPHIN_ICON_SYMBOLIC_FOLDER_TEMPLATES;
    }

    return NULL;
}

gchar *
nolphin_file_get_control_icon_name (NolphinFile *file)
{
    gchar *uri;
    gchar *icon_name;

    icon_name = NULL;

    uri = nolphin_file_get_uri (file);

    if (eel_uri_is_search (uri)) {
        icon_name = g_strdup (NOLPHIN_ICON_SYMBOLIC_FOLDER_SAVED_SEARCH);
    } else if (eel_uri_is_trash (uri)) {
        icon_name = nolphin_trash_monitor_get_symbolic_icon_name ();
    } else if (eel_uri_is_recent (uri)) {
        icon_name = g_strdup (NOLPHIN_ICON_SYMBOLIC_FOLDER_RECENT);
    } else if (eel_uri_is_favorite (uri)) {
        icon_name = g_strdup (NOLPHIN_ICON_SYMBOLIC_FOLDER_FAVORITES);
    } else if (eel_uri_is_network (uri)) {
        icon_name = g_strdup (NOLPHIN_ICON_SYMBOLIC_NETWORK);
    } else {
        const gchar *static_name;

        static_name = get_symbolic_icon_name_for_file (file);

        if (static_name != NULL) {
            icon_name = g_strdup (static_name);
        } else {
            GFile *location;

            location = nolphin_file_get_location (file);

            if (!g_file_is_native (location)) {
                icon_name = g_strdup (NOLPHIN_ICON_SYMBOLIC_FOLDER_REMOTE);
            } else if (file->details->mime_type != NULL) {
                GIcon *gicon;

                gicon = g_content_type_get_symbolic_icon (file->details->mime_type);

                if (G_IS_THEMED_ICON (gicon)) {
                    icon_name = g_strdup (g_themed_icon_get_names (G_THEMED_ICON (gicon))[0]);
                }

                g_object_unref (gicon);

                if (g_strcmp0 (icon_name, "inode-directory-symbolic") == 0) {
                    g_free (icon_name);
                    icon_name = g_strdup ("xsi-folder-symbolic");
                }
            }

            g_object_unref (location);
        }
    }

    g_free (uri);

    if (icon_name == NULL) {
        icon_name = g_strdup ("text-x-generic");
    }

    return icon_name;
}

gboolean
nolphin_file_get_pinning (NolphinFile *file)
{
    g_return_val_if_fail (NOLPHIN_IS_FILE (file), FALSE);

    if (file->details->pinning == FILE_META_STATE_INIT) {
        file->details->pinning =  nolphin_file_get_boolean_metadata (file, NOLPHIN_METADATA_KEY_PINNED, FALSE);
    }

    return file->details->pinning;
}

void
nolphin_file_set_pinning (NolphinFile *file,
                       gboolean  pin)
{
    g_return_if_fail (NOLPHIN_IS_FILE (file));

    nolphin_file_set_boolean_metadata (file, NOLPHIN_METADATA_KEY_PINNED, TRUE, pin);
}

gboolean
nolphin_file_get_is_favorite (NolphinFile *file)
{
    g_return_val_if_fail (NOLPHIN_IS_FILE (file), FALSE);

    if (file->details->favorite == FILE_META_STATE_INIT) {
        gboolean meta_bool = nolphin_file_get_boolean_metadata (file, NOLPHIN_METADATA_KEY_FAVORITE, FALSE);
        file->details->favorite = meta_bool ? FILE_META_STATE_TRUE : FILE_META_STATE_FALSE;
    }

    return file->details->favorite;
}

void
nolphin_file_set_is_favorite (NolphinFile *file,
                           gboolean  favorite)
{
    g_return_if_fail (NOLPHIN_IS_FILE (file));
    NolphinFile *real_file;
    gchar *uri;

    if (nolphin_file_is_in_favorites (file) && !nolphin_file_is_broken_symbolic_link (file)) {
        uri = nolphin_file_get_symbolic_link_target_uri (file);
        real_file = nolphin_file_get_existing_by_uri (uri);
    } else {
        uri = nolphin_file_get_uri (file);
        real_file = nolphin_file_ref (file);
    }

    if (real_file != NULL) {
        nolphin_file_set_boolean_metadata (real_file, NOLPHIN_METADATA_KEY_FAVORITE, FALSE, favorite);
    }

    if (favorite)
    {
        xapp_favorites_add (xapp_favorites_get_default (), uri);
    }
    else
    {
        xapp_favorites_remove (xapp_favorites_get_default (), uri);
    }

    nolphin_file_unref (real_file);
    g_free (uri);
}

NolphinIconInfo *
nolphin_file_get_icon (NolphinFile *file,
			int size,
            int max_width,
            int scale,
			NolphinFileIconFlags flags)
{
	NolphinIconInfo *icon;
	GIcon *gicon;
	GdkPixbuf *raw_pixbuf, *scaled_pixbuf;
	int modified_size;

	if (file == NULL) {
		return NULL;
	}

	gicon = get_custom_icon (file);
	if (gicon != NULL) {
		icon = nolphin_icon_info_lookup (gicon, size, scale);
		g_object_unref (gicon);
		return icon;
	}

	DEBUG ("Called file_get_icon(), at size %d, force thumbnail %d", size,
	       flags & NOLPHIN_FILE_ICON_FLAGS_FORCE_THUMBNAIL_SIZE);

	if ((flags & NOLPHIN_FILE_ICON_FLAGS_USE_THUMBNAILS) &&
	    nolphin_file_should_show_thumbnail (file)) {

        if (flags & NOLPHIN_FILE_ICON_FLAGS_FORCE_THUMBNAIL_SIZE) {
            modified_size = size * scale;
        } else {
            modified_size = size * scale * scaled_cached_thumbnail_size;
            DEBUG ("Modifying icon size to %d, as our cached thumbnail size is %d",
                   modified_size, cached_thumbnail_size);
        }

		if (file->details->thumbnail) {
			int w, h, s;
			double thumb_scale;

			raw_pixbuf = g_object_ref (file->details->thumbnail);

			w = gdk_pixbuf_get_width (raw_pixbuf);
			h = gdk_pixbuf_get_height (raw_pixbuf);

            if (flags & NOLPHIN_FILE_ICON_FLAGS_PIN_HEIGHT_FOR_DESKTOP) {
                g_assert (max_width > 0);

                thumb_scale = (gdouble) modified_size / h;

                if (w * thumb_scale > max_width) {
                    thumb_scale = (gdouble) max_width / w;
                }

                s = thumb_scale * h;
            } else {
                s = MAX (w, h);
                /* Don't scale up small thumbnails in the standard view */
                if (s <= cached_thumbnail_size) {
                    thumb_scale = (double)size / NOLPHIN_ICON_SIZE_STANDARD;
                }
                else {
                    thumb_scale = (double)modified_size / s;
                }
                /* Make sure that icons don't get smaller than NOLPHIN_ICON_SIZE_SMALLEST */
                if (s*thumb_scale <= NOLPHIN_ICON_SIZE_SMALLEST) {
                    thumb_scale = (double) NOLPHIN_ICON_SIZE_SMALLEST / s;
                }
            }

            scaled_pixbuf = gdk_pixbuf_scale_simple (raw_pixbuf,
                                                     MAX (w * thumb_scale, 1),
                                                     MAX (h * thumb_scale, 1),
                                                     GDK_INTERP_BILINEAR);

            /* Only apply frame if icon has no transparency, and is large enough */
            if (!gdk_pixbuf_get_has_alpha (raw_pixbuf) && s >= 128 * scale && !file_is_cad_model (file)) {
                nolphin_thumbnail_frame_image (&scaled_pixbuf);
            }

            if (flags & NOLPHIN_FILE_ICON_FLAGS_PIN_HEIGHT_FOR_DESKTOP) {
                gint check_height;

                check_height = gdk_pixbuf_get_height (scaled_pixbuf);

                if (check_height < size) {
                    nolphin_thumbnail_pad_top_and_bottom (&scaled_pixbuf, size - check_height);
                }
            }

			g_object_unref (raw_pixbuf);

			/* Don't scale up if more than 25%, then read the original
			   image instead. */
			if (modified_size > 256 * 1.25 * scale &&
			    !file->details->thumbnail_wants_original &&
			    nolphin_can_thumbnail_internally (file)) {
				/* Invalidate if we resize upward */
				file->details->thumbnail_wants_original = TRUE;
				nolphin_file_invalidate_attributes (file, NOLPHIN_FILE_ATTRIBUTE_THUMBNAIL);
			}

			DEBUG ("Returning thumbnailed image, at size %d %d",
			       (int) (w * thumb_scale), (int) (h * thumb_scale));

            icon = nolphin_icon_info_new_for_pixbuf (scaled_pixbuf, scale);
            g_object_unref (scaled_pixbuf);

            return icon;
		} else if (file->details->thumbnail_path == NULL &&
			   file->details->can_read &&
			   !file->details->is_thumbnailing &&
			   !file->details->thumbnailing_failed) {
			if (nolphin_can_thumbnail (file)) {
				nolphin_create_thumbnail (file);
			}
		}
	}

    if (file->details->is_thumbnailing &&
	    flags & NOLPHIN_FILE_ICON_FLAGS_USE_THUMBNAILS)
		gicon = g_themed_icon_new (ICON_NAME_THUMBNAIL_LOADING);
	else
		gicon = nolphin_file_get_gicon (file, flags);

	if (gicon) {
		icon = nolphin_icon_info_lookup (gicon, size, scale);
		g_object_unref (gicon);
		return icon;
	} else {
        /* This is inefficient, but *very* unlikely to be hit */
        GIcon *generic = g_themed_icon_new ("text-x-generic");
        icon = nolphin_icon_info_lookup (generic, size, scale);
        g_object_unref (generic);

		return icon;
	}
}

GdkPixbuf *
nolphin_file_get_icon_pixbuf (NolphinFile *file,
			       int size,
			       gboolean force_size,
                   int scale,
			       NolphinFileIconFlags flags)
{
	NolphinIconInfo *info;
	GdkPixbuf *pixbuf;

	info = nolphin_file_get_icon (file, size, 0, scale, flags);
	if (force_size) {
		pixbuf =  nolphin_icon_info_get_pixbuf_at_size (info, size);
	} else {
		pixbuf = nolphin_icon_info_get_pixbuf (info);
	}
	nolphin_icon_info_unref (info);

	return pixbuf;
}

gboolean
nolphin_file_get_date (NolphinFile *file,
			NolphinDateType date_type,
			time_t *date)
{
	if (date != NULL) {
		*date = 0;
	}

	g_return_val_if_fail (date_type == NOLPHIN_DATE_TYPE_CHANGED
			      || date_type == NOLPHIN_DATE_TYPE_ACCESSED
			      || date_type == NOLPHIN_DATE_TYPE_MODIFIED
                  || date_type == NOLPHIN_DATE_TYPE_CREATED
			      || date_type == NOLPHIN_DATE_TYPE_TRASHED
			      || date_type == NOLPHIN_DATE_TYPE_PERMISSIONS_CHANGED, FALSE);

	if (file == NULL) {
		return FALSE;
	}

	g_return_val_if_fail (NOLPHIN_IS_FILE (file), FALSE);

	return NOLPHIN_FILE_CLASS (G_OBJECT_GET_CLASS (file))->get_date (file, date_type, date);
}

static char *
nolphin_file_get_where_string (NolphinFile *file)
{
	if (file == NULL) {
		return NULL;
	}

	g_return_val_if_fail (NOLPHIN_IS_FILE (file), NULL);

	return NOLPHIN_FILE_CLASS (G_OBJECT_GET_CLASS (file))->get_where_string (file);
}

static char *
nolphin_file_get_trash_original_file_parent_as_string (NolphinFile *file)
{
	NolphinFile *orig_file, *parent;
	GFile *location;
	char *filename;

	if (file->details->trash_orig_path != NULL) {
		orig_file = nolphin_file_get_trash_original_file (file);
		parent = nolphin_file_get_parent (orig_file);
		location = nolphin_file_get_location (parent);

		filename = g_file_get_parse_name (location);

		g_object_unref (location);
		nolphin_file_unref (parent);
		nolphin_file_unref (orig_file);

		return filename;
	}

	return NULL;
}

static const gchar *
nolphin_date_type_to_string (NolphinDateType type)
{
    switch (type) {
        case NOLPHIN_DATE_TYPE_MODIFIED:
            return "NOLPHIN_DATE_TYPE_MODIFIED";
        case NOLPHIN_DATE_TYPE_CHANGED:
            return "NOLPHIN_DATE_TYPE_CHANGED";
        case NOLPHIN_DATE_TYPE_ACCESSED:
            return "NOLPHIN_DATE_TYPE_ACCESSED";
        case NOLPHIN_DATE_TYPE_PERMISSIONS_CHANGED:
            return "NOLPHIN_DATE_TYPE_PERMISSIONS_CHANGED";
        case NOLPHIN_DATE_TYPE_TRASHED:
            return "NOLPHIN_DATE_TYPE_TRASHED";
        case NOLPHIN_DATE_TYPE_CREATED:
            return "NOLPHIN_DATE_TYPE_CREATED";
    }

    return "(unknown)";
}

/**
 * nolphin_file_get_date_as_string:
 *
 * Get a user-displayable string representing a file modification date.
 * The caller is responsible for g_free-ing this string.
 * @file: NolphinFile representing the file in question.
 *
 * Returns: Newly allocated string ready to display to the user.
 *
 **/
static char *
nolphin_file_get_date_as_string (NolphinFile       *file,
                              NolphinDateType    date_type,
                              NolphinDateCompactFormat  date_format)
{
	time_t file_time_raw;
    GDateTime *file_date_time, *file_date_time_utc;
    GDateTime *today_midnight, *now;
    GTimeZone *current_timezone;
	gint days_ago;
	gboolean use_24;
	const gchar *format;
	gchar *result;
    int date_format_pref;

  	if (!nolphin_file_get_date (file, date_type, &file_time_raw))
		return NULL;

    current_timezone = prefs_current_timezone;

    file_date_time_utc = g_date_time_new_from_unix_utc (file_time_raw);

    if (!file_date_time_utc) {
        DEBUG ("File '%s' has invalid time for %s",
               nolphin_file_peek_name (file),
               nolphin_date_type_to_string (date_type));
        return NULL;
    }

    file_date_time = g_date_time_to_timezone (file_date_time_utc, current_timezone);
    g_date_time_unref (file_date_time_utc);

    date_format_pref = prefs_current_date_format;

	if (date_format_pref == NOLPHIN_DATE_FORMAT_LOCALE) {
		result = g_date_time_format (file_date_time, "%c");
		goto out;
	} else if (date_format_pref == NOLPHIN_DATE_FORMAT_ISO) {
		result = g_date_time_format (file_date_time, "%Y-%m-%d %H:%M:%S");
		goto out;
	}

    if (date_format != NOLPHIN_DATE_FORMAT_FULL) {
        GDateTime *file_date;

        now = g_date_time_new_now (current_timezone);

        today_midnight = g_date_time_new (current_timezone,
                                          g_date_time_get_year (now),
                                          g_date_time_get_month (now),
                                          g_date_time_get_day_of_month (now),
                                          0, 0, 0);

        file_date = g_date_time_new (current_timezone,
                                     g_date_time_get_year (file_date_time),
                                     g_date_time_get_month (file_date_time),
                                     g_date_time_get_day_of_month (file_date_time),
                                     0, 0, 0);

        days_ago = g_date_time_difference (today_midnight, file_date) / G_TIME_SPAN_DAY;

        use_24 = prefs_current_24h_time_format;

		// Show only the time if date is on today
		if (days_ago < 1) {
			if (use_24) {
				/* Translators: Time in 24h format */
				format = _("%H:%M");
			} else {
				/* Translators: Time in 12h format */
				format = _("%H:%M");
			}
		}
		// Show the word "Yesterday" and time if date is on yesterday
		else if (days_ago < 2) {
			if (date_format == NOLPHIN_DATE_FORMAT_REGULAR) {
				// xgettext:no-c-format
				format = _("Gestern");
			} else {
				if (use_24) {
					/* Translators: this is the word Yesterday followed by
					 * a time in 24h format. i.e. "Yesterday 23:04" */
					// xgettext:no-c-format
					format = _("Gestern %H:%M");
				} else {
					/* Translators: this is the word Yesterday followed by
					 * a time in 12h format. i.e. "Yesterday 9:04 PM" */
					// xgettext:no-c-format
					format = _("Gestern %H:%M");
				}
			}
		}
		// Show a week day and time if date is in the last week
		else if (days_ago < 7) {
			if (date_format == NOLPHIN_DATE_FORMAT_REGULAR) {
				// xgettext:no-c-format
				format = _("%A");
			} else {
				if (use_24) {
					/* Translators: this is the name of the week day followed by
					 * a time in 24h format. i.e. "Monday 23:04" */
					// xgettext:no-c-format
					format = _("%A %H:%M");
				} else {
					/* Translators: this is the week day name followed by
					 * a time in 12h format. i.e. "Monday 9:04 PM" */
					// xgettext:no-c-format
					format = _("%A %H:%M");
				}
			}
		} else if (g_date_time_get_year (file_date) == g_date_time_get_year (now)) {
			if (date_format == NOLPHIN_DATE_FORMAT_REGULAR) {
				/* Translators: this is the day of the month followed
				 * by the abbreviated month name i.e. "3 February" */
				// xgettext:no-c-format
				format = _("%e. %B");
			} else {
				if (use_24) {
					/* Translators: this is the day of the month followed
					 * by the abbreviated month name followed by a time in
					 * 24h format i.e. "3 February 23:04" */
					// xgettext:no-c-format
					format = _("%e. %B, %H:%M");
				} else {
					/* Translators: this is the day of the month followed
					 * by the abbreviated month name followed by a time in
					 * 12h format i.e. "3 February 9:04" */
					// xgettext:no-c-format
					format = _("%e. %B %H:%M");
				}
			}
		} else {
			if (date_format == NOLPHIN_DATE_FORMAT_REGULAR) {
				/* Translators: this is the day of the month followed by the abbreviated
				 * month name followed by the year i.e. "3 Feb 2015" */
				// xgettext:no-c-format
				format = _("%e. %b. %Y");
			} else {
				if (use_24) {
					/* Translators: this is the day number followed
					 * by the abbreviated month name followed by the year followed
					 * by a time in 24h format i.e. "3 Feb 2015 23:04" */
					// xgettext:no-c-format
					format = _("%e. %b. %Y, %H:%M");
				} else {
					/* Translators: this is the day number followed
					 * by the abbreviated month name followed by the year followed
					 * by a time in 12h format i.e. "3 Feb 2015 9:04 PM" */
					// xgettext:no-c-format
					format = _("%e. %b. %Y %H:%M");
				}
			}
		}

		g_date_time_unref (file_date);
		g_date_time_unref (now);
		g_date_time_unref (today_midnight);
	} else {
		// xgettext:no-c-format
		format = _("%c");
	}

	result = g_date_time_format (file_date_time, format);

 out:
	g_date_time_unref (file_date_time);

	return result;
}

static NolphinSpeedTradeoffValue show_directory_item_count;

static void
show_directory_item_count_changed_callback (gpointer callback_data)
{
	show_directory_item_count = g_settings_get_enum (nolphin_preferences, NOLPHIN_PREFERENCES_SHOW_DIRECTORY_ITEM_COUNTS);
}

static gboolean
get_speed_tradeoff_preference_for_file (NolphinFile *file, NolphinSpeedTradeoffValue value)
{
	GFilesystemPreviewType use_preview;

	g_return_val_if_fail (NOLPHIN_IS_FILE (file), FALSE);

	use_preview = nolphin_file_get_filesystem_use_preview (file);

	if (value == NOLPHIN_SPEED_TRADEOFF_ALWAYS) {
		if (use_preview == G_FILESYSTEM_PREVIEW_TYPE_NEVER) {
			return FALSE;
		} else {
			return TRUE;
		}
	}

	if (value == NOLPHIN_SPEED_TRADEOFF_NEVER) {
		return FALSE;
	}

	g_assert (value == NOLPHIN_SPEED_TRADEOFF_LOCAL_ONLY);

	if (use_preview == G_FILESYSTEM_PREVIEW_TYPE_NEVER) {
		/* file system says to never preview anything */
		return FALSE;
	} else if (use_preview == G_FILESYSTEM_PREVIEW_TYPE_IF_LOCAL) {
		/* file system says we should treat file as if it's local */
		return TRUE;
	} else {
		/* only local files */
		return nolphin_file_is_local (file);
	}
}

gboolean
nolphin_file_should_show_directory_item_count (NolphinFile *file)
{
	static gboolean show_directory_item_count_callback_added = FALSE;

	g_return_val_if_fail (NOLPHIN_IS_FILE (file), FALSE);

	if (file->details->mime_type &&
	    strcmp (file->details->mime_type, "x-directory/smb-share") == 0) {
		return FALSE;
	}

	/* Add the callback once for the life of our process */
	if (!show_directory_item_count_callback_added) {
		g_signal_connect_swapped (nolphin_preferences,
					  "changed::" NOLPHIN_PREFERENCES_SHOW_DIRECTORY_ITEM_COUNTS,
					  G_CALLBACK(show_directory_item_count_changed_callback),
					  NULL);
		show_directory_item_count_callback_added = TRUE;

		/* Peek for the first time */
		show_directory_item_count_changed_callback (NULL);
	}

	return get_speed_tradeoff_preference_for_file (file, show_directory_item_count);
}

gboolean
nolphin_file_should_show_type (NolphinFile *file)
{
	char *uri;
	gboolean ret;

	g_return_val_if_fail (NOLPHIN_IS_FILE (file), FALSE);

	uri = nolphin_file_get_uri (file);
	ret = ((strcmp (uri, "computer:///") != 0) &&
	       (strcmp (uri, "network:///") != 0) &&
	       (strcmp (uri, "smb:///") != 0));
	g_free (uri);

	return ret;
}

/**
 * nolphin_file_get_directory_item_count
 *
 * Get the number of items in a directory.
 * @file: NolphinFile representing a directory.
 * @count: Place to put count.
 * @count_unreadable: Set to TRUE (if non-NULL) if permissions prevent
 * the item count from being read on this directory. Otherwise set to FALSE.
 *
 * Returns: TRUE if count is available.
 *
 **/
gboolean
nolphin_file_get_directory_item_count (NolphinFile *file,
					guint *count,
					gboolean *count_unreadable)
{
	if (count != NULL) {
		*count = 0;
	}
	if (count_unreadable != NULL) {
		*count_unreadable = FALSE;
	}

	g_return_val_if_fail (NOLPHIN_IS_FILE (file), FALSE);

	if (!nolphin_file_is_directory (file)) {
		return FALSE;
	}

	if (!nolphin_file_should_show_directory_item_count (file)) {
		return FALSE;
	}

	return NOLPHIN_FILE_CLASS (G_OBJECT_GET_CLASS (file))->get_item_count
		(file, count, count_unreadable);
}

/**
 * nolphin_file_get_deep_counts
 *
 * Get the statistics about items inside a directory.
 * @file: NolphinFile representing a directory or file.
 * @directory_count: Place to put count of directories inside.
 * @files_count: Place to put count of files inside.
 * @unreadable_directory_count: Number of directories encountered
 * that were unreadable.
 * @total_size: Total size of all files and directories visited.
 * @force: Whether the deep counts should even be collected if
 * nolphin_file_should_show_directory_item_count returns FALSE
 * for this file.
 *
 * Returns: Status to indicate whether sizes are available.
 *
 **/
NolphinRequestStatus
nolphin_file_get_deep_counts (NolphinFile *file,
			       guint *directory_count,
			       guint *file_count,
			       guint *unreadable_directory_count,
                   guint *hidden_count,
			       goffset *total_size,
			       gboolean force)
{
	if (directory_count != NULL) {
		*directory_count = 0;
	}
	if (file_count != NULL) {
		*file_count = 0;
	}
	if (unreadable_directory_count != NULL) {
		*unreadable_directory_count = 0;
	}
	if (total_size != NULL) {
		*total_size = 0;
	}

    if (hidden_count != NULL) {
        *hidden_count = 0;
    }

	g_return_val_if_fail (NOLPHIN_IS_FILE (file), NOLPHIN_REQUEST_DONE);

	if (!force && !nolphin_file_should_show_directory_item_count (file)) {
		/* Set field so an existing value isn't treated as up-to-date
		 * when preference changes later.
		 */
		file->details->deep_counts_status = NOLPHIN_REQUEST_NOT_STARTED;
		return file->details->deep_counts_status;
	}

	return NOLPHIN_FILE_CLASS (G_OBJECT_GET_CLASS (file))->get_deep_counts
		(file, directory_count, file_count,
		 unreadable_directory_count, hidden_count, total_size);
}

void
nolphin_file_recompute_deep_counts (NolphinFile *file)
{
	if (file->details->deep_counts_status != NOLPHIN_REQUEST_IN_PROGRESS) {
		file->details->deep_counts_status = NOLPHIN_REQUEST_NOT_STARTED;
		if (file->details->directory != NULL) {
			nolphin_directory_add_file_to_work_queue (file->details->directory, file);
			nolphin_directory_async_state_changed (file->details->directory);
		}
	}
}


/**
 * nolphin_file_get_directory_item_mime_types
 *
 * Get the list of mime-types present in a directory.
 * @file: NolphinFile representing a directory. It is an error to
 * call this function on a file that is not a directory.
 * @mime_list: Place to put the list of mime-types.
 *
 * Returns: TRUE if mime-type list is available.
 *
 **/
gboolean
nolphin_file_get_directory_item_mime_types (NolphinFile *file,
					     GList **mime_list)
{
	g_return_val_if_fail (NOLPHIN_IS_FILE (file), FALSE);
	g_return_val_if_fail (mime_list != NULL, FALSE);

	if (!nolphin_file_is_directory (file)
	    || !file->details->got_mime_list) {
		*mime_list = NULL;
		return FALSE;
	}

	*mime_list = eel_g_str_list_copy (file->details->mime_list);
	return TRUE;
}

gboolean
nolphin_file_can_get_size (NolphinFile *file)
{
	return file->details->size == -1;
}


/**
 * nolphin_file_get_size
 *
 * Get the file size.
 * @file: NolphinFile representing the file in question.
 *
 * Returns: Size in bytes.
 *
 **/
goffset
nolphin_file_get_size (NolphinFile *file)
{
	/* Before we have info on the file, we don't know the size. */
	if (file->details->size == -1)
		return 0;
	return file->details->size;
}

time_t
nolphin_file_get_mtime (NolphinFile *file)
{
	return file->details->mtime;
}

time_t
nolphin_file_get_ctime (NolphinFile *file)
{
	return file->details->ctime;
}


static void
set_attributes_get_info_callback (GObject *source_object,
				  GAsyncResult *res,
				  gpointer callback_data)
{
	NolphinFileOperation *op;
	GFileInfo *new_info;
	GError *error;

	op = callback_data;

	error = NULL;
	new_info = g_file_query_info_finish (G_FILE (source_object), res, &error);
	if (new_info != NULL) {
		if (nolphin_file_update_info (op->file, new_info)) {
			nolphin_file_changed (op->file);
		}
		g_object_unref (new_info);
	}
	nolphin_file_operation_complete (op, NULL, error);
	if (error) {
		g_error_free (error);
	}
}


static void
set_attributes_callback (GObject *source_object,
			 GAsyncResult *result,
			 gpointer callback_data)
{
	NolphinFileOperation *op;
	GError *error;
	gboolean res;

	op = callback_data;

	error = NULL;
	res = g_file_set_attributes_finish (G_FILE (source_object),
					    result,
					    NULL,
					    &error);

	if (res) {
		g_file_query_info_async (G_FILE (source_object),
					 NOLPHIN_FILE_DEFAULT_ATTRIBUTES,
					 0,
					 G_PRIORITY_DEFAULT,
					 op->cancellable,
					 set_attributes_get_info_callback, op);
	} else {
		nolphin_file_operation_complete (op, NULL, error);
		g_error_free (error);
	}
}

void
nolphin_file_set_attributes (NolphinFile *file,
			      GFileInfo *attributes,
			      NolphinFileOperationCallback callback,
			      gpointer callback_data)
{
	NolphinFileOperation *op;
	GFile *location;

	op = nolphin_file_operation_new (file, callback, callback_data);

	location = nolphin_file_get_location (file);
	g_file_set_attributes_async (location,
				     attributes,
				     0,
				     G_PRIORITY_DEFAULT,
				     op->cancellable,
				     set_attributes_callback,
				     op);
	g_object_unref (location);
}


/**
 * nolphin_file_can_get_permissions:
 *
 * Check whether the permissions for a file are determinable.
 * This might not be the case for files on non-UNIX file systems.
 *
 * @file: The file in question.
 *
 * Return value: TRUE if the permissions are valid.
 */
gboolean
nolphin_file_can_get_permissions (NolphinFile *file)
{
	return file->details->has_permissions;
}

/**
 * nolphin_file_can_set_permissions:
 *
 * Check whether the current user is allowed to change
 * the permissions of a file.
 *
 * @file: The file in question.
 *
 * Return value: TRUE if the current user can change the
 * permissions of @file, FALSE otherwise. It's always possible
 * that when you actually try to do it, you will fail.
 */
gboolean
nolphin_file_can_set_permissions (NolphinFile *file)
{
	uid_t user_id;

    if (file == NULL)
    {
        return FALSE;
    }

	if (file->details->uid != -1 &&
	    nolphin_file_is_local (file)) {
		/* Check the user. */
		user_id = geteuid();

		/* Owner is allowed to set permissions. */
		if (user_id == (uid_t) file->details->uid) {
			return TRUE;
		}

		/* Root is also allowed to set permissions. */
		if (user_id == 0) {
			return TRUE;
		}

		/* Nobody else is allowed. */
		return FALSE;
	}

	/* pretend to have full chmod rights when no info is available, relevant when
	 * the FS can't provide ownership info, for instance for FTP */
	return TRUE;
}

guint
nolphin_file_get_permissions (NolphinFile *file)
{
	g_return_val_if_fail (nolphin_file_can_get_permissions (file), 0);

	return file->details->permissions;
}

/**
 * nolphin_file_set_permissions:
 *
 * Change a file's permissions. This should only be called if
 * nolphin_file_can_set_permissions returned TRUE.
 *
 * @file: NolphinFile representing the file in question.
 * @new_permissions: New permissions value. This is the whole
 * set of permissions, not a delta.
 **/
void
nolphin_file_set_permissions (NolphinFile *file,
			       guint32 new_permissions,
			       NolphinFileOperationCallback callback,
			       gpointer callback_data)
{
	GFileInfo *info;
	GError *error;

	if (!nolphin_file_can_set_permissions (file)) {
		/* Claim that something changed even if the permission change failed.
		 * This makes it easier for some clients who see the "reverting"
		 * to the old permissions as "changing back".
		 */
		nolphin_file_changed (file);
		error = g_error_new (G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED,
				     _("Einstellen der Zugriffsrechte ist nicht erlaubt"));
		(* callback) (file, NULL, error, callback_data);
		g_error_free (error);
		return;
	}

	/* Test the permissions-haven't-changed case explicitly
	 * because we don't want to send the file-changed signal if
	 * nothing changed.
	 */
	if (new_permissions == file->details->permissions) {
		(* callback) (file, NULL, NULL, callback_data);
		return;
	}

	if (!nolphin_file_undo_manager_pop_flag ()) {
		NolphinFileUndoInfo *undo_info;
        GFile *location = nolphin_file_get_location (file);

		undo_info = nolphin_file_undo_info_permissions_new (location,
								     file->details->permissions,
								     new_permissions);
        g_object_unref (location);
		nolphin_file_undo_manager_set_action (undo_info);
	}

	info = g_file_info_new ();
	g_file_info_set_attribute_uint32 (info, G_FILE_ATTRIBUTE_UNIX_MODE, new_permissions);
	nolphin_file_set_attributes (file, info, callback, callback_data);

	g_object_unref (info);
}

/**
 * nolphin_file_can_get_selinux_context:
 *
 * Check whether the selinux context for a file are determinable.
 * This might not be the case for files on non-UNIX file systems,
 * files without a context or systems that don't support selinux.
 *
 * @file: The file in question.
 *
 * Return value: TRUE if the permissions are valid.
 */
gboolean
nolphin_file_can_get_selinux_context (NolphinFile *file)
{
	return file->details->selinux_context != NULL;
}


/**
 * nolphin_file_get_selinux_context:
 *
 * Get a user-displayable string representing a file's selinux
 * context
 * @file: NolphinFile representing the file in question.
 *
 * Returns: Newly allocated string ready to display to the user.
 *
 **/
char *
nolphin_file_get_selinux_context (NolphinFile *file)
{
	char *translated;
	char *raw;

	g_return_val_if_fail (NOLPHIN_IS_FILE (file), NULL);

	if (!nolphin_file_can_get_selinux_context (file)) {
		return NULL;
	}

	raw = file->details->selinux_context;

#ifdef HAVE_SELINUX
	if (selinux_raw_to_trans_context (raw, &translated) == 0) {
		char *tmp;
		tmp = g_strdup (translated);
		freecon (translated);
		translated = tmp;
	}
	else
#endif
	{
		translated = g_strdup (raw);
	}

	return translated;
}

static char *
get_real_name (const char *name, const char *gecos)
{
	char *locale_string, *part_before_comma, *capitalized_login_name, *real_name;

	if (gecos == NULL) {
		return NULL;
	}

	locale_string = eel_str_strip_substring_and_after (gecos, ",");
	if (!g_utf8_validate (locale_string, -1, NULL)) {
		part_before_comma = g_locale_to_utf8 (locale_string, -1, NULL, NULL, NULL);
		g_free (locale_string);
	} else {
		part_before_comma = locale_string;
	}

	if (!g_utf8_validate (name, -1, NULL)) {
		locale_string = g_locale_to_utf8 (name, -1, NULL, NULL, NULL);
	} else {
		locale_string = g_strdup (name);
	}

	capitalized_login_name = eel_str_capitalize (locale_string);
	g_free (locale_string);

	if (capitalized_login_name == NULL) {
		real_name = part_before_comma;
	} else {
		real_name = eel_str_replace_substring
			(part_before_comma, "&", capitalized_login_name);
		g_free (part_before_comma);
	}


	if (g_strcmp0 (real_name, NULL) == 0
	    || g_strcmp0 (name, real_name) == 0
	    || g_strcmp0 (capitalized_login_name, real_name) == 0) {
		g_free (real_name);
		real_name = NULL;
	}

	g_free (capitalized_login_name);

	return real_name;
}

static gboolean
get_group_id_from_group_name (const char *group_name, uid_t *gid)
{
	struct group *group;

	g_assert (gid != NULL);

	group = getgrnam (group_name);

	if (group == NULL) {
		return FALSE;
	}

	*gid = group->gr_gid;

	return TRUE;
}

static gboolean
get_ids_from_user_name (const char *user_name, uid_t *uid, uid_t *gid)
{
	struct passwd *password_info;

	g_assert (uid != NULL || gid != NULL);

	password_info = getpwnam (user_name);

	if (password_info == NULL) {
		return FALSE;
	}

	if (uid != NULL) {
		*uid = password_info->pw_uid;
	}

	if (gid != NULL) {
		*gid = password_info->pw_gid;
	}

	return TRUE;
}

static gboolean
get_user_id_from_user_name (const char *user_name, uid_t *id)
{
	return get_ids_from_user_name (user_name, id, NULL);
}

static gboolean
get_id_from_digit_string (const char *digit_string, uid_t *id)
{
	long scanned_id;
	char c;

	g_assert (id != NULL);

	/* Only accept string if it has one integer with nothing
	 * afterwards.
	 */
	if (sscanf (digit_string, "%ld%c", &scanned_id, &c) != 1) {
		return FALSE;
	}
	*id = scanned_id;
	return TRUE;
}

/**
 * nolphin_file_can_get_owner:
 *
 * Check whether the owner a file is determinable.
 * This might not be the case for files on non-UNIX file systems.
 *
 * @file: The file in question.
 *
 * Return value: TRUE if the owner is valid.
 */
gboolean
nolphin_file_can_get_owner (NolphinFile *file)
{
	/* Before we have info on a file, the owner is unknown. */
	return file->details->uid != -1;
}

/**
 * nolphin_file_get_owner_name:
 *
 * Get the user name of the file's owner. If the owner has no
 * name, returns the userid as a string. The caller is responsible
 * for g_free-ing this string.
 *
 * @file: The file in question.
 *
 * Return value: A newly-allocated string.
 */
char *
nolphin_file_get_owner_name (NolphinFile *file)
{
	return nolphin_file_get_owner_as_string (file, FALSE);
}

/**
 * nolphin_file_can_set_owner:
 *
 * Check whether the current user is allowed to change
 * the owner of a file.
 *
 * @file: The file in question.
 *
 * Return value: TRUE if the current user can change the
 * owner of @file, FALSE otherwise. It's always possible
 * that when you actually try to do it, you will fail.
 */
gboolean
nolphin_file_can_set_owner (NolphinFile *file)
{
	/* Not allowed to set the owner if we can't
	 * even read it. This can happen on non-UNIX file
	 * systems.
	 */
	if (!nolphin_file_can_get_owner (file)) {
		return FALSE;
	}

	/* Only root is also allowed to set the owner. */
	return nolphin_user_is_root ();
}

/**
 * nolphin_file_set_owner:
 *
 * Set the owner of a file. This will only have any effect if
 * nolphin_file_can_set_owner returns TRUE.
 *
 * @file: The file in question.
 * @user_name_or_id: The user name to set the owner to.
 * If the string does not match any user name, and the
 * string is an integer, the owner will be set to the
 * userid represented by that integer.
 * @callback: Function called when asynch owner change succeeds or fails.
 * @callback_data: Parameter passed back with callback function.
 */
void
nolphin_file_set_owner (NolphinFile *file,
			 const char *user_name_or_id,
			 NolphinFileOperationCallback callback,
			 gpointer callback_data)
{
	GError *error;
	GFileInfo *info;
	uid_t new_id;

	if (!nolphin_file_can_set_owner (file)) {
		/* Claim that something changed even if the permission
		 * change failed. This makes it easier for some
		 * clients who see the "reverting" to the old owner as
		 * "changing back".
		 */
		nolphin_file_changed (file);
		error = g_error_new (G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED,
				     _("Einstellen des Besitzers ist nicht erlaubt"));
		(* callback) (file, NULL, error, callback_data);
		g_error_free (error);
		return;
	}

	/* If no match treating user_name_or_id as name, try treating
	 * it as id.
	 */
	if (!get_user_id_from_user_name (user_name_or_id, &new_id)
	    && !get_id_from_digit_string (user_name_or_id, &new_id)) {
		/* Claim that something changed even if the permission
		 * change failed. This makes it easier for some
		 * clients who see the "reverting" to the old owner as
		 * "changing back".
		 */
		nolphin_file_changed (file);
		error = g_error_new (G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
				     _("Der angegebene Besitzer »%s« existiert nicht"), user_name_or_id);
		(* callback) (file, NULL, error, callback_data);
		g_error_free (error);
		return;
	}

	/* Test the owner-hasn't-changed case explicitly because we
	 * don't want to send the file-changed signal if nothing
	 * changed.
	 */
	if (new_id == (uid_t) file->details->uid) {
		(* callback) (file, NULL, NULL, callback_data);
		return;
	}

	if (!nolphin_file_undo_manager_pop_flag ()) {
		NolphinFileUndoInfo *undo_info;
        GFile *location = nolphin_file_get_location (file);
		char* current_owner;

		current_owner = nolphin_file_get_owner_as_string (file, FALSE);

		undo_info = nolphin_file_undo_info_ownership_new (NOLPHIN_FILE_UNDO_OP_CHANGE_OWNER,
								   location,
								   current_owner,
								   user_name_or_id);
		nolphin_file_undo_manager_set_action (undo_info);
        g_object_unref (location);
		g_free (current_owner);
	}

	info = g_file_info_new ();
	g_file_info_set_attribute_uint32 (info, G_FILE_ATTRIBUTE_UNIX_UID, new_id);
	nolphin_file_set_attributes (file, info, callback, callback_data);
	g_object_unref (info);
}

/**
 * nolphin_get_user_names:
 *
 * Get a list of user names. For users with a different associated
 * "real name", the real name follows the standard user name, separated
 * by a carriage return. The caller is responsible for freeing this list
 * and its contents.
 */
GList *
nolphin_get_user_names (void)
{
	GList *list;
	char *real_name, *name;
	struct passwd *user;

	list = NULL;

	setpwent ();

	while ((user = getpwent ()) != NULL) {
		real_name = get_real_name (user->pw_name, user->pw_gecos);
		if (real_name != NULL) {
			name = g_strconcat (user->pw_name, "\n", real_name, NULL);
		} else {
			name = g_strdup (user->pw_name);
		}
		g_free (real_name);
		list = g_list_prepend (list, name);
	}

	endpwent ();

	return g_list_sort (list, (GCompareFunc) g_utf8_collate);
}

/**
 * nolphin_file_can_get_group:
 *
 * Check whether the group a file is determinable.
 * This might not be the case for files on non-UNIX file systems.
 *
 * @file: The file in question.
 *
 * Return value: TRUE if the group is valid.
 */
gboolean
nolphin_file_can_get_group (NolphinFile *file)
{
	/* Before we have info on a file, the group is unknown. */
	return file->details->gid != -1;
}

/**
 * nolphin_file_get_group_name:
 *
 * Get the name of the file's group. If the group has no
 * name, returns the groupid as a string. The caller is responsible
 * for g_free-ing this string.
 *
 * @file: The file in question.
 *
 * Return value: A newly-allocated string.
 **/
char *
nolphin_file_get_group_name (NolphinFile *file)
{
	return g_strdup (file->details->group);
}

/**
 * nolphin_file_can_set_group:
 *
 * Check whether the current user is allowed to change
 * the group of a file.
 *
 * @file: The file in question.
 *
 * Return value: TRUE if the current user can change the
 * group of @file, FALSE otherwise. It's always possible
 * that when you actually try to do it, you will fail.
 */
gboolean
nolphin_file_can_set_group (NolphinFile *file)
{
	uid_t user_id;

	/* Not allowed to set the permissions if we can't
	 * even read them. This can happen on non-UNIX file
	 * systems.
	 */
	if (!nolphin_file_can_get_group (file)) {
		return FALSE;
	}

	/* Check the user. */
	user_id = geteuid();

	/* Owner is allowed to set group (with restrictions). */
	if (user_id == (uid_t) file->details->uid) {
		return TRUE;
	}

	/* Root is also allowed to set group. */
	if (user_id == 0) {
		return TRUE;
	}

	/* Nobody else is allowed. */
	return FALSE;
}

/* Get a list of group names, filtered to only the ones
 * that contain the given username. If the username is
 * NULL, returns a list of all group names.
 */
static GList *
nolphin_get_group_names_for_user (void)
{
	GList *list;
	struct group *group;
	int count, i;
	gid_t gid_list[NGROUPS_MAX + 1];


	list = NULL;

	count = getgroups (NGROUPS_MAX + 1, gid_list);
	for (i = 0; i < count; i++) {
		group = getgrgid (gid_list[i]);
		if (group == NULL)
			break;

		list = g_list_prepend (list, g_strdup (group->gr_name));
	}

	return g_list_sort (list, (GCompareFunc) g_utf8_collate);
}

/**
 * nolphin_get_group_names:
 *
 * Get a list of all group names.
 */
GList *
nolphin_get_all_group_names (void)
{
	GList *list;
	struct group *group;

	list = NULL;

	setgrent ();

	while ((group = getgrent ()) != NULL)
		list = g_list_prepend (list, g_strdup (group->gr_name));

	endgrent ();

	return g_list_sort (list, (GCompareFunc) g_utf8_collate);
}

/**
 * nolphin_file_get_settable_group_names:
 *
 * Get a list of all group names that the current user
 * can set the group of a specific file to.
 *
 * @file: The NolphinFile in question.
 */
GList *
nolphin_file_get_settable_group_names (NolphinFile *file)
{
	uid_t user_id;
	GList *result;

	if (!nolphin_file_can_set_group (file)) {
		return NULL;
	}

	/* Check the user. */
	user_id = geteuid();

	if (user_id == 0) {
		/* Root is allowed to set group to anything. */
		result = nolphin_get_all_group_names ();
	} else if (user_id == (uid_t) file->details->uid) {
		/* Owner is allowed to set group to any that owner is member of. */
		result = nolphin_get_group_names_for_user ();
	} else {
		g_warning ("unhandled case in nolphin_get_settable_group_names");
		result = NULL;
	}

	return result;
}

/**
 * nolphin_file_set_group:
 *
 * Set the group of a file. This will only have any effect if
 * nolphin_file_can_set_group returns TRUE.
 *
 * @file: The file in question.
 * @group_name_or_id: The group name to set the owner to.
 * If the string does not match any group name, and the
 * string is an integer, the group will be set to the
 * group id represented by that integer.
 * @callback: Function called when asynch group change succeeds or fails.
 * @callback_data: Parameter passed back with callback function.
 */
void
nolphin_file_set_group (NolphinFile *file,
			 const char *group_name_or_id,
			 NolphinFileOperationCallback callback,
			 gpointer callback_data)
{
	GError *error;
	GFileInfo *info;
	uid_t new_id;

	if (!nolphin_file_can_set_group (file)) {
		/* Claim that something changed even if the group
		 * change failed. This makes it easier for some
		 * clients who see the "reverting" to the old group as
		 * "changing back".
		 */
		nolphin_file_changed (file);
		error = g_error_new (G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED,
				     _("Einstellen der Gruppe nicht erlaubt"));
		(* callback) (file, NULL, error, callback_data);
		g_error_free (error);
		return;
	}

	/* If no match treating group_name_or_id as name, try treating
	 * it as id.
	 */
	if (!get_group_id_from_group_name (group_name_or_id, &new_id)
	    && !get_id_from_digit_string (group_name_or_id, &new_id)) {
		/* Claim that something changed even if the group
		 * change failed. This makes it easier for some
		 * clients who see the "reverting" to the old group as
		 * "changing back".
		 */
		nolphin_file_changed (file);
		error = g_error_new (G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
				     _("Die angegebene Gruppe »%s« existiert nicht"), group_name_or_id);
		(* callback) (file, NULL, error, callback_data);
		g_error_free (error);
		return;
	}

	if (new_id == (gid_t) file->details->gid) {
		(* callback) (file, NULL, NULL, callback_data);
		return;
	}

	if (!nolphin_file_undo_manager_pop_flag ()) {
		NolphinFileUndoInfo *undo_info;
        GFile *location = nolphin_file_get_location (file);
		char *current_group;

		current_group = nolphin_file_get_group_name (file);
		undo_info = nolphin_file_undo_info_ownership_new (NOLPHIN_FILE_UNDO_OP_CHANGE_GROUP,
								   location,
								   current_group,
								   group_name_or_id);
		nolphin_file_undo_manager_set_action (undo_info);
        g_object_unref (location);
		g_free (current_group);
	}

	info = g_file_info_new ();
	g_file_info_set_attribute_uint32 (info, G_FILE_ATTRIBUTE_UNIX_GID, new_id);
	nolphin_file_set_attributes (file, info, callback, callback_data);
	g_object_unref (info);
}

/**
 * nolphin_file_get_octal_permissions_as_string:
 *
 * Get a user-displayable string representing a file's permissions
 * as an octal number. The caller
 * is responsible for g_free-ing this string.
 * @file: NolphinFile representing the file in question.
 *
 * Returns: Newly allocated string ready to display to the user.
 *
 **/
static char *
nolphin_file_get_octal_permissions_as_string (NolphinFile *file)
{
	guint32 permissions;

	g_assert (NOLPHIN_IS_FILE (file));

	if (!nolphin_file_can_get_permissions (file)) {
		return NULL;
	}

	permissions = file->details->permissions;
	return g_strdup_printf ("%03o", permissions);
}

/**
 * nolphin_file_get_permissions_as_string:
 *
 * Get a user-displayable string representing a file's permissions. The caller
 * is responsible for g_free-ing this string.
 * @file: NolphinFile representing the file in question.
 *
 * Returns: Newly allocated string ready to display to the user.
 *
 **/
static char *
nolphin_file_get_permissions_as_string (NolphinFile *file)
{
	guint32 permissions;
	gboolean is_directory;
	gboolean is_link;
	gboolean suid, sgid, sticky;

	if (!nolphin_file_can_get_permissions (file)) {
		return NULL;
	}

	g_assert (NOLPHIN_IS_FILE (file));

	permissions = file->details->permissions;
	is_directory = nolphin_file_is_directory (file);
	is_link = nolphin_file_is_symbolic_link (file);

	/* We use ls conventions for displaying these three obscure flags */
	suid = permissions & S_ISUID;
	sgid = permissions & S_ISGID;
	sticky = permissions & S_ISVTX;

	return g_strdup_printf ("%c%c%c%c%c%c%c%c%c%c",
				 is_link ? 'l' : is_directory ? 'd' : '-',
		 		 permissions & S_IRUSR ? 'r' : '-',
				 permissions & S_IWUSR ? 'w' : '-',
				 permissions & S_IXUSR
				 	? (suid ? 's' : 'x')
				 	: (suid ? 'S' : '-'),
				 permissions & S_IRGRP ? 'r' : '-',
				 permissions & S_IWGRP ? 'w' : '-',
				 permissions & S_IXGRP
				 	? (sgid ? 's' : 'x')
				 	: (sgid ? 'S' : '-'),
				 permissions & S_IROTH ? 'r' : '-',
				 permissions & S_IWOTH ? 'w' : '-',
				 permissions & S_IXOTH
				 	? (sticky ? 't' : 'x')
				 	: (sticky ? 'T' : '-'));
}

/**
 * nolphin_file_get_owner_as_string:
 *
 * Get a user-displayable string representing a file's owner. The caller
 * is responsible for g_free-ing this string.
 * @file: NolphinFile representing the file in question.
 * @include_real_name: Whether or not to append the real name (if any)
 * for this user after the user name.
 *
 * Returns: Newly allocated string ready to display to the user.
 *
 **/
char *
nolphin_file_get_owner_as_string (NolphinFile *file, gboolean include_real_name)
{
	char *user_name;

	/* Before we have info on a file, the owner is unknown. */
	if (file->details->owner == NULL &&
	    file->details->owner_real == NULL) {
		return NULL;
	}

	if (file->details->owner_real == NULL) {
		user_name = g_strdup (file->details->owner);
	} else if (file->details->owner == NULL) {
		user_name = g_strdup (file->details->owner_real);
	} else if (include_real_name &&
		   strcmp (file->details->owner, file->details->owner_real) != 0) {
		user_name = g_strdup_printf ("%s - %s",
					     file->details->owner,
					     file->details->owner_real);
	} else {
		user_name = g_strdup (file->details->owner);
	}

	return user_name;
}

static char *
format_item_count_for_display (guint item_count,
			       gboolean includes_directories,
			       gboolean includes_files)
{
	g_assert (includes_directories || includes_files);

	return g_strdup_printf (includes_directories
			? (includes_files
			   ? ngettext ("%'u Objekt", "%'u Objekte", item_count)
			   : ngettext ("%'u Ordner", "%'u Ordner", item_count))
			: ngettext ("%'u Datei", "%'u Dateien", item_count), item_count);
}

/**
 * nolphin_file_get_size_as_string:
 *
 * Get a user-displayable string representing a file size. The caller
 * is responsible for g_free-ing this string. The string is an item
 * count for directories.
 * @file: NolphinFile representing the file in question.
 *
 * Returns: Newly allocated string ready to display to the user.
 *
 **/
static char *
nolphin_file_get_size_as_string (NolphinFile *file)
{
	guint item_count;
	gboolean count_unreadable;

	if (file == NULL) {
		return NULL;
	}

	g_assert (NOLPHIN_IS_FILE (file));

	if (nolphin_file_is_directory (file)) {
		if (!nolphin_file_get_directory_item_count (file, &item_count, &count_unreadable)) {
			return NULL;
		}
		return format_item_count_for_display (item_count, TRUE, TRUE);
	}

	if (file->details->size == -1) {
		return NULL;
	}

	return g_format_size_full (file->details->size, nolphin_global_preferences_get_size_prefix_preference ());
}

/**
 * nolphin_file_get_size_as_string_with_real_size:
 *
 * Get a user-displayable string representing a file size. The caller
 * is responsible for g_free-ing this string. The string is an item
 * count for directories.
 * This function adds the real size in the string.
 * @file: NolphinFile representing the file in question.
 *
 * Returns: Newly allocated string ready to display to the user.
 *
 **/
static char *
nolphin_file_get_size_as_string_with_real_size (NolphinFile *file)
{
	guint item_count;
	gboolean count_unreadable;
	int prefix;

	if (file == NULL) {
		return NULL;
	}

	g_assert (NOLPHIN_IS_FILE (file));

	if (nolphin_file_is_directory (file)) {
		if (!nolphin_file_get_directory_item_count (file, &item_count, &count_unreadable)) {
			return NULL;
		}
		return format_item_count_for_display (item_count, TRUE, TRUE);
	}

	if (file->details->size == -1) {
		return NULL;
	}

	prefix = nolphin_global_preferences_get_size_prefix_preference ();
	return g_format_size_full (file->details->size, prefix);
}


static char *
nolphin_file_get_deep_count_as_string_internal (NolphinFile *file,
						 gboolean report_size,
						 gboolean report_directory_count,
						 gboolean report_file_count)
{
	NolphinRequestStatus status;
	guint directory_count;
	guint file_count;
	guint unreadable_count;
    guint hidden_count;
	guint total_count;
	goffset total_size;
	int prefix;

	/* Must ask for size or some kind of count, but not both. */
	g_assert (!report_size || (!report_directory_count && !report_file_count));
	g_assert (report_size || report_directory_count || report_file_count);

	if (file == NULL) {
		return NULL;
	}

	g_assert (NOLPHIN_IS_FILE (file));
	g_assert (nolphin_file_is_directory (file));

	status = nolphin_file_get_deep_counts
		(file, &directory_count, &file_count, &unreadable_count, &hidden_count, &total_size, FALSE);

	/* Check whether any info is available. */
	if (status == NOLPHIN_REQUEST_NOT_STARTED) {
		return NULL;
	}

	total_count = file_count + directory_count;

	if (total_count == 0) {
		switch (status) {
		case NOLPHIN_REQUEST_IN_PROGRESS:
			/* Don't return confident "zero" until we're finished looking,
			 * because of next case.
			 */
			return NULL;
		case NOLPHIN_REQUEST_DONE:
			/* Don't return "zero" if we there were contents but we couldn't read them. */
			if (unreadable_count != 0) {
				return NULL;
			}
		case NOLPHIN_REQUEST_NOT_STARTED:
		default:
            break;
		}
	}

	/* Note that we don't distinguish the "everything was readable" case
	 * from the "some things but not everything was readable" case here.
	 * Callers can distinguish them using nolphin_file_get_deep_counts
	 * directly if desired.
	 */
	if (report_size) {
		prefix = nolphin_global_preferences_get_size_prefix_preference ();
		return g_format_size_full (total_size, prefix);
	}

	return format_item_count_for_display (report_directory_count
		? (report_file_count ? total_count : directory_count)
		: file_count,
		report_directory_count, report_file_count);
}

/**
 * nolphin_file_get_deep_size_as_string:
 *
 * Get a user-displayable string representing the size of all contained
 * items (only makes sense for directories). The caller
 * is responsible for g_free-ing this string.
 * @file: NolphinFile representing the file in question.
 *
 * Returns: Newly allocated string ready to display to the user.
 *
 **/
static char *
nolphin_file_get_deep_size_as_string (NolphinFile *file)
{
	return nolphin_file_get_deep_count_as_string_internal (file, TRUE, FALSE, FALSE);
}

/**
 * nolphin_file_get_deep_total_count_as_string:
 *
 * Get a user-displayable string representing the count of all contained
 * items (only makes sense for directories). The caller
 * is responsible for g_free-ing this string.
 * @file: NolphinFile representing the file in question.
 *
 * Returns: Newly allocated string ready to display to the user.
 *
 **/
static char *
nolphin_file_get_deep_total_count_as_string (NolphinFile *file)
{
	return nolphin_file_get_deep_count_as_string_internal (file, FALSE, TRUE, TRUE);
}

/**
 * nolphin_file_get_deep_file_count_as_string:
 *
 * Get a user-displayable string representing the count of all contained
 * items, not including directories. It only makes sense to call this
 * function on a directory. The caller
 * is responsible for g_free-ing this string.
 * @file: NolphinFile representing the file in question.
 *
 * Returns: Newly allocated string ready to display to the user.
 *
 **/
static char *
nolphin_file_get_deep_file_count_as_string (NolphinFile *file)
{
	return nolphin_file_get_deep_count_as_string_internal (file, FALSE, FALSE, TRUE);
}

/**
 * nolphin_file_get_deep_directory_count_as_string:
 *
 * Get a user-displayable string representing the count of all contained
 * directories. It only makes sense to call this
 * function on a directory. The caller
 * is responsible for g_free-ing this string.
 * @file: NolphinFile representing the file in question.
 *
 * Returns: Newly allocated string ready to display to the user.
 *
 **/
static char *
nolphin_file_get_deep_directory_count_as_string (NolphinFile *file)
{
	return nolphin_file_get_deep_count_as_string_internal (file, FALSE, TRUE, FALSE);
}

/**
 * nolphin_file_get_string_attribute:
 *
 * Get a user-displayable string from a named attribute. Use g_free to
 * free this string. If the value is unknown, returns NULL. You can call
 * nolphin_file_get_string_attribute_with_default if you want a non-NULL
 * default.
 *
 * @file: NolphinFile representing the file in question.
 * @attribute_name: The name of the desired attribute. The currently supported
 * set includes "name", "type", "detailed_type", "mime_type", "size", "deep_size", "deep_directory_count",
 * "deep_file_count", "deep_total_count", "date_modified", "date_changed", "date_accessed",
 * "date_permissions", "owner", "group", "permissions", "octal_permissions", "uri", "where",
 * "link_target", "volume", "free_space", "selinux_context", "trashed_on", "trashed_orig_path"
 *
 * Returns: Newly allocated string ready to display to the user, or NULL
 * if the value is unknown or @attribute_name is not supported.
 *
 **/
char *
nolphin_file_get_string_attribute_q (NolphinFile *file, GQuark attribute_q)
{
	char *extension_attribute;

	if (attribute_q == attribute_name_q) {
		return nolphin_file_get_display_name (file);
	}
    if (attribute_q == attribute_extension_q) {
        const char *name;
        char *ext;
        name = nolphin_file_peek_display_name (file);
        ext = eel_filename_get_extension_offset (name);
        if (ext) {
            return g_strdup (ext + 1);
        }

        return g_strdup ("");
    }
	if (attribute_q == attribute_type_q) {
		return nolphin_file_get_type_as_string (file);
	}
    if (attribute_q == attribute_detailed_type_q) {
        return nolphin_file_get_detailed_type_as_string (file);
    }
	if (attribute_q == attribute_mime_type_q) {
		return nolphin_file_get_mime_type (file);
	}
	if (attribute_q == attribute_size_q) {
		return nolphin_file_get_size_as_string (file);
	}
	if (attribute_q == attribute_size_detail_q) {
		return nolphin_file_get_size_as_string_with_real_size (file);
	}
	if (attribute_q == attribute_deep_size_q) {
		return nolphin_file_get_deep_size_as_string (file);
	}
	if (attribute_q == attribute_deep_file_count_q) {
		return nolphin_file_get_deep_file_count_as_string (file);
	}
	if (attribute_q == attribute_deep_directory_count_q) {
		return nolphin_file_get_deep_directory_count_as_string (file);
	}
	if (attribute_q == attribute_deep_total_count_q) {
		return nolphin_file_get_deep_total_count_as_string (file);
	}
	if (attribute_q == attribute_trash_orig_path_q) {
		return nolphin_file_get_trash_original_file_parent_as_string (file);
	}
	if (attribute_q == attribute_date_modified_q) {
		return nolphin_file_get_date_as_string (file,
							 NOLPHIN_DATE_TYPE_MODIFIED,
							 NOLPHIN_DATE_FORMAT_REGULAR);
	}
	if (attribute_q == attribute_date_modified_full_q) {
		return nolphin_file_get_date_as_string (file,
							 NOLPHIN_DATE_TYPE_MODIFIED,
							 NOLPHIN_DATE_FORMAT_FULL);
	}
	if (attribute_q == attribute_date_modified_with_time_q) {
		return nolphin_file_get_date_as_string (file,
							 NOLPHIN_DATE_TYPE_MODIFIED,
		                     NOLPHIN_DATE_FORMAT_REGULAR_WITH_TIME);
	}
	if (attribute_q == attribute_date_changed_q) {
		return nolphin_file_get_date_as_string (file,
							 NOLPHIN_DATE_TYPE_CHANGED,
							 NOLPHIN_DATE_FORMAT_REGULAR);
	}
	if (attribute_q == attribute_date_changed_full_q) {
		return nolphin_file_get_date_as_string (file,
							 NOLPHIN_DATE_TYPE_CHANGED,
							 NOLPHIN_DATE_FORMAT_FULL);
	}
	if (attribute_q == attribute_date_accessed_q) {
		return nolphin_file_get_date_as_string (file,
							 NOLPHIN_DATE_TYPE_ACCESSED,
							 NOLPHIN_DATE_FORMAT_REGULAR);
	}
	if (attribute_q == attribute_date_accessed_full_q) {
		return nolphin_file_get_date_as_string (file,
							 NOLPHIN_DATE_TYPE_ACCESSED,
							 NOLPHIN_DATE_FORMAT_FULL);
	}
    if (attribute_q == attribute_date_created_q) {
        return nolphin_file_get_date_as_string (file,
                             NOLPHIN_DATE_TYPE_CREATED,
                             NOLPHIN_DATE_FORMAT_REGULAR);
    }
    if (attribute_q == attribute_date_created_full_q) {
        return nolphin_file_get_date_as_string (file,
                             NOLPHIN_DATE_TYPE_CREATED,
                             NOLPHIN_DATE_FORMAT_FULL);
    }
    if (attribute_q == attribute_date_created_with_time_q) {
        return nolphin_file_get_date_as_string (file,
                             NOLPHIN_DATE_TYPE_CREATED,
                             NOLPHIN_DATE_FORMAT_REGULAR_WITH_TIME);
    }
	if (attribute_q == attribute_trashed_on_q) {
		return nolphin_file_get_date_as_string (file,
							 NOLPHIN_DATE_TYPE_TRASHED,
							 NOLPHIN_DATE_FORMAT_FULL);
	}
	if (attribute_q == attribute_trashed_on_full_q) {
		return nolphin_file_get_date_as_string (file,
							 NOLPHIN_DATE_TYPE_TRASHED,
							 NOLPHIN_DATE_FORMAT_REGULAR);
	}
	if (attribute_q == attribute_date_permissions_q) {
		return nolphin_file_get_date_as_string (file,
							 NOLPHIN_DATE_TYPE_PERMISSIONS_CHANGED,
							 NOLPHIN_DATE_FORMAT_FULL);
	}
	if (attribute_q == attribute_date_permissions_full_q) {
		return nolphin_file_get_date_as_string (file,
							 NOLPHIN_DATE_TYPE_PERMISSIONS_CHANGED,
							 NOLPHIN_DATE_FORMAT_REGULAR);
	}
	if (attribute_q == attribute_permissions_q) {
		return nolphin_file_get_permissions_as_string (file);
	}
	if (attribute_q == attribute_selinux_context_q) {
		return nolphin_file_get_selinux_context (file);
	}
	if (attribute_q == attribute_octal_permissions_q) {
		return nolphin_file_get_octal_permissions_as_string (file);
	}
	if (attribute_q == attribute_owner_q) {
		return nolphin_file_get_owner_as_string (file, TRUE);
	}
	if (attribute_q == attribute_group_q) {
		return nolphin_file_get_group_name (file);
	}
	if (attribute_q == attribute_uri_q) {
		return nolphin_file_get_uri (file);
	}
	if (attribute_q == attribute_where_q) {
		return nolphin_file_get_where_string (file);
	}
	if (attribute_q == attribute_link_target_q) {
		return nolphin_file_get_symbolic_link_target_path (file);
	}
	if (attribute_q == attribute_volume_q) {
		return nolphin_file_get_volume_name (file);
	}
	if (attribute_q == attribute_free_space_q) {
		return nolphin_file_get_volume_free_space (file);
	}

	extension_attribute = NULL;

	if (file->details->pending_extension_attributes) {
		extension_attribute = g_hash_table_lookup (file->details->pending_extension_attributes,
							   GINT_TO_POINTER (attribute_q));
	}

	if (extension_attribute == NULL && file->details->extension_attributes) {
		extension_attribute = g_hash_table_lookup (file->details->extension_attributes,
							   GINT_TO_POINTER (attribute_q));
	}

	return g_strdup (extension_attribute);
}

char *
nolphin_file_get_string_attribute (NolphinFile *file, const char *attribute_name)
{
	return nolphin_file_get_string_attribute_q (file, g_quark_from_string (attribute_name));
}


/**
 * nolphin_file_get_string_attribute_with_default:
 *
 * Get a user-displayable string from a named attribute. Use g_free to
 * free this string. If the value is unknown, returns a string representing
 * the unknown value, which varies with attribute. You can call
 * nolphin_file_get_string_attribute if you want NULL instead of a default
 * result.
 *
 * @file: NolphinFile representing the file in question.
 * @attribute_name: The name of the desired attribute. See the description of
 * nolphin_file_get_string for the set of available attributes.
 *
 * Returns: Newly allocated string ready to display to the user, or a string
 * such as "unknown" if the value is unknown or @attribute_name is not supported.
 *
 **/
char *
nolphin_file_get_string_attribute_with_default_q (NolphinFile *file, GQuark attribute_q)
{
	char *result;
	guint item_count;
	gboolean count_unreadable;
	NolphinRequestStatus status;

	result = nolphin_file_get_string_attribute_q (file, attribute_q);
	if (result != NULL) {
		return result;
	}

	/* Supply default values for the ones we know about. */
	/* FIXME bugzilla.gnome.org 40646:
	 * Use hash table and switch statement or function pointers for speed?
	 */
	if (attribute_q == attribute_size_q) {
		if (!nolphin_file_should_show_directory_item_count (file)) {
			return g_strdup ("--");
		}
		count_unreadable = FALSE;
		if (nolphin_file_is_directory (file)) {
			nolphin_file_get_directory_item_count (file, &item_count, &count_unreadable);
		}
		return g_strdup (count_unreadable ? _("? Objekte") : "...");
	}
	if (attribute_q == attribute_deep_size_q) {
		status = nolphin_file_get_deep_counts (file, NULL, NULL, NULL, NULL, NULL, FALSE);
		if (status == NOLPHIN_REQUEST_DONE) {
			/* This means no contents at all were readable */
			return g_strdup (_("? Byte"));
		}
		return g_strdup ("...");
	}
	if (attribute_q == attribute_deep_file_count_q
	    || attribute_q == attribute_deep_directory_count_q
	    || attribute_q == attribute_deep_total_count_q) {
		status = nolphin_file_get_deep_counts (file, NULL, NULL, NULL, NULL, NULL, FALSE);
		if (status == NOLPHIN_REQUEST_DONE) {
			/* This means no contents at all were readable */
			return g_strdup (_("? Objekte"));
		}
		return g_strdup ("...");
	}
    if (attribute_q == attribute_type_q
        || attribute_q == attribute_detailed_type_q
        || attribute_q == attribute_mime_type_q) {
        return g_strdup (_("Unbekannt"));
    }
	if (attribute_q == attribute_trashed_on_q) {
		/* If n/a */
		return g_strdup ("");
	}
	if (attribute_q == attribute_trash_orig_path_q) {
		/* If n/a */
		return g_strdup ("");
	}

	/* Fallback, use for both unknown attributes and attributes
	 * for which we have no more appropriate default.
	 */
	return g_strdup (_("Unbekannt"));
}

char *
nolphin_file_get_string_attribute_with_default (NolphinFile *file, const char *attribute_name)
{
	return nolphin_file_get_string_attribute_with_default_q (file, g_quark_from_string (attribute_name));
}

gboolean
nolphin_file_is_date_sort_attribute_q (GQuark attribute_q)
{
	if (attribute_q == attribute_modification_date_q ||
	    attribute_q == attribute_date_modified_q ||
	    attribute_q == attribute_date_modified_full_q ||
	    attribute_q == attribute_date_modified_with_time_q ||
	    attribute_q == attribute_accessed_date_q ||
	    attribute_q == attribute_date_accessed_q ||
	    attribute_q == attribute_date_accessed_full_q ||
	    attribute_q == attribute_date_changed_q ||
	    attribute_q == attribute_date_changed_full_q ||
	    attribute_q == attribute_trashed_on_q ||
	    attribute_q == attribute_trashed_on_full_q ||
	    attribute_q == attribute_date_permissions_q ||
	    attribute_q == attribute_date_permissions_full_q ||
        attribute_q == attribute_creation_date_q ||
        attribute_q == attribute_date_created_q ||
        attribute_q == attribute_date_created_full_q ||
        attribute_q == attribute_date_created_with_time_q) {
		return TRUE;
	}

	return FALSE;
}

gboolean
nolphin_file_attribute_slow_sort (const gchar *sort_attribute)
{
	GQuark attribute_q = g_quark_from_string (sort_attribute);

	return attribute_q == attribute_size_q ||
	       attribute_q == attribute_size_detail_q ||
	       attribute_q == attribute_deep_size_q ||
	       attribute_q == attribute_deep_file_count_q ||
	       attribute_q == attribute_deep_directory_count_q ||
	       attribute_q == attribute_deep_total_count_q;
}

struct {
        const char *icon_name;
        const char *display_name;
} mime_type_map[] = {
    { "application-x-executable", N_("Programm") },
    { "audio-x-generic", N_("Ton") },
    { "font-x-generic", N_("Schriftart") },
    { "image-x-generic", N_("Bild") },
    { "package-x-generic", N_("Archiv") },
    { "text-html", N_("Auszeichnung") },
    { "text-x-generic", N_("Text") },
    { "text-x-generic-template", N_("Text") },
    { "text-x-script", N_("Programm") },
    { "video-x-generic", N_("Video") },
    { "x-office-address-book", N_("Kontakte") },
    { "x-office-calendar", N_("Kalender") },
    { "x-office-document", N_("Dokument") },
    { "x-office-presentation", N_("Präsentation") },
    { "x-office-spreadsheet", N_("Tabellendokument") },
};

static char *
get_basic_type_for_mime_type (const char *mime_type)
{
    char *icon_name;
    char *basic_type = NULL;

    icon_name = g_content_type_get_generic_icon_name (mime_type);
    if (icon_name != NULL) {
        guint i;

        for (i = 0; i < G_N_ELEMENTS (mime_type_map); i++) {
            if (strcmp (mime_type_map[i].icon_name, icon_name) == 0) {
                basic_type = g_strdup (gettext (mime_type_map[i].display_name));
                break;
            }
        }
    }

    g_free (icon_name);

    return basic_type;
}

static char *
get_description (NolphinFile     *file,
                 gboolean      detailed)
{
    const char *mime_type;

    g_assert (NOLPHIN_IS_FILE (file));

    mime_type = file->details->mime_type;

    if (mime_type == NULL) {
        return NULL;
    }

    if (g_content_type_is_unknown (mime_type)) {
        if (nolphin_file_is_executable (file)) {
            return g_strdup (_("Programm"));
        }
        return g_strdup (_("Binär"));
    }

    if (strcmp (mime_type, "inode/directory") == 0) {
        return g_strdup (_("Ordner"));
     }

    if (detailed) {
        char *description;

        description = g_content_type_get_description (mime_type);
        if (description != NULL) {
            return description;
        }
    } else {
        char *category;

        category = get_basic_type_for_mime_type (mime_type);
        if (category != NULL) {
            return category;
        }

        if (category == NULL) {
            return g_content_type_get_description (mime_type);
        }
    }

    return g_strdup (mime_type);
}

/* Takes ownership of string */
static char *
update_description_for_link (NolphinFile *file, char *string)
{
	char *res;

	if (nolphin_file_is_symbolic_link (file) && !nolphin_file_is_in_favorites (file)) {
		g_assert (!nolphin_file_is_broken_symbolic_link (file));
		if (string == NULL) {
			return g_strdup (_("Verknüpfung"));
		}
		/* Note to localizers: convert file type string for file
		 * (e.g. "folder", "plain text") to file type for symbolic link
		 * to that kind of file (e.g. "link to folder").
		 */
		res = g_strdup_printf (_("Verknüpfung mit %s"), string);
		g_free (string);
		return res;
	}

	return string;
}

char *
nolphin_file_get_type_as_string (NolphinFile *file)
{
	if (file == NULL) {
		return NULL;
	}

	if (nolphin_file_is_broken_symbolic_link (file)) {
		return g_strdup (_("Verknüpfung (fehlerhaft)"));
    }

    return update_description_for_link (file, get_description (file, FALSE));
}

char *
nolphin_file_get_detailed_type_as_string (NolphinFile *file)
{
    if (file == NULL) {
        return NULL;
    }

    if (nolphin_file_is_broken_symbolic_link (file)) {
        return g_strdup (_("Verknüpfung (fehlerhaft)"));
	}

	return update_description_for_link (file, get_description (file, TRUE));
}

/**
 * nolphin_file_get_file_type
 *
 * Return this file's type.
 * @file: NolphinFile representing the file in question.
 *
 * Returns: The type.
 *
 **/
GFileType
nolphin_file_get_file_type (NolphinFile *file)
{
	if (file == NULL) {
		return G_FILE_TYPE_UNKNOWN;
	}

	return file->details->type;
}

/**
 * nolphin_file_get_mime_type
 *
 * Return this file's default mime type.
 * @file: NolphinFile representing the file in question.
 *
 * Returns: The mime type.
 *
 **/
char *
nolphin_file_get_mime_type (NolphinFile *file)
{
	if (file != NULL) {
		g_return_val_if_fail (NOLPHIN_IS_FILE (file), NULL);
		if (file->details->mime_type != NULL) {
			return g_strdup (file->details->mime_type);
		}
	}
	return g_strdup ("application/octet-stream");
}

/**
 * nolphin_file_is_mime_type
 *
 * Check whether a file is of a particular MIME type, or inherited
 * from it.
 * @file: NolphinFile representing the file in question.
 * @mime_type: The MIME-type string to test (e.g. "text/plain")
 *
 * Return value: TRUE if @mime_type exactly matches the
 * file's MIME type.
 *
 **/
gboolean
nolphin_file_is_mime_type (NolphinFile *file, const char *mime_type)
{
	g_return_val_if_fail (NOLPHIN_IS_FILE (file), FALSE);
	g_return_val_if_fail (mime_type != NULL, FALSE);

	if (file->details->mime_type == NULL) {
		return FALSE;
	}
	return g_content_type_is_a (file->details->mime_type, mime_type);
}

gboolean
nolphin_file_is_launchable (NolphinFile *file)
{
	gboolean type_can_be_executable;

	type_can_be_executable = FALSE;
	if (file->details->mime_type != NULL) {
		type_can_be_executable =
			g_content_type_can_be_executable (file->details->mime_type);
	}

	return type_can_be_executable &&
		nolphin_file_can_get_permissions (file) &&
		nolphin_file_can_execute (file) &&
		nolphin_file_is_executable (file) &&
		!nolphin_file_is_directory (file);
}


/**
 * nolphin_file_get_emblem_icons
 *
 * Return the list of names of emblems that this file should display,
 * in canonical order.
 * @file: NolphinFile representing the file in question.
 * @view_file: NolphinFile representing the view's directory_as_file.
 *
 * Returns: A list of emblem names.
 *
 **/
GList *
nolphin_file_get_emblem_icons (NolphinFile *file,
                            NolphinFile *view_file)
{
	GList *keywords, *l;
	GList *icons;
	char *icon_names[2];
	char *keyword;
	GIcon *icon;

	if (file == NULL) {
		return NULL;
	}

	g_return_val_if_fail (NOLPHIN_IS_FILE (file), NULL);

	keywords = nolphin_file_get_keywords (file);
	keywords = prepend_automatic_keywords (file, view_file, keywords);

	/* §18.1: Schloss-Emblem nur, wenn access::can-write tatsächlich
	 * vorhanden und FALSE ist (nolphin_file_can_write() liefert
	 * bereits TRUE zurück, wenn das Attribut fehlt oder unbekannt
	 * ist - siehe deren Dokumentation). Zusätzlich nur, wenn die
	 * Datei innerhalb eines ansonsten beschreibbaren Ortes liegt und
	 * nicht im Papierkorb ist, damit nicht jede Datei eines komplett
	 * schreibgeschützten Ortes (z. B. eines schreibgeschützten
	 * Datenträgers) einzeln markiert wird. */
	if (view_file && nolphin_file_can_write (view_file)) {
		if (!nolphin_file_can_write (file) && !nolphin_file_is_in_trash (file)) {
			keywords = g_list_prepend (keywords, g_strdup (NOLPHIN_FILE_EMBLEM_NAME_CANT_WRITE));
		}
	}

	icons = NULL;
	for (l = keywords; l != NULL; l = l->next) {
		keyword = l->data;

		icon_names[0] = g_strconcat ("emblem-", keyword, NULL);
		icon_names[1] = keyword;
		icon = g_themed_icon_new_from_names (icon_names, 2);
		g_free (icon_names[0]);

		icons = g_list_prepend (icons, icon);
	}

	g_list_free_full (keywords, g_free);

	/* Git-Overlay: Status-Emblem für Dateien in einem Repository. */
	if (!nolphin_file_is_in_trash (file)) {
		GFile *location = nolphin_file_get_location (file);
		const gchar * const *names = nolphin_git_overlay_get_emblem_names (nolphin_git_overlay_get_status (location));

		if (names != NULL) {
			icons = g_list_prepend (icons, g_themed_icon_new_from_names ((char **) names, -1));
		}
		g_object_unref (location);
	}

	return icons;
}

static GList *
sort_keyword_list_and_remove_duplicates (GList *keywords)
{
	GList *p;
	GList *duplicate_link;

	if (keywords != NULL) {
		keywords = g_list_sort (keywords, (GCompareFunc) g_utf8_collate);

		p = keywords;
		while (p->next != NULL) {
			if (strcmp ((const char *) p->data, (const char *) p->next->data) == 0) {
				duplicate_link = p->next;
				keywords = g_list_remove_link (keywords, duplicate_link);
				g_list_free_full (duplicate_link, g_free);
			} else {
				p = p->next;
			}
		}
	}

	return keywords;
}

/**
 * nolphin_file_get_keywords
 *
 * Return this file's keywords.
 * @file: NolphinFile representing the file in question.
 *
 * Returns: A list of keywords.
 *
 **/
GList *
nolphin_file_get_keywords (NolphinFile *file)
{
	GList *keywords;

	if (file == NULL) {
		return NULL;
	}

	g_return_val_if_fail (NOLPHIN_IS_FILE (file), NULL);

	keywords = eel_g_str_list_copy (file->details->extension_emblems);
	keywords = g_list_concat (keywords, eel_g_str_list_copy (file->details->pending_extension_emblems));
	keywords = g_list_concat (keywords, nolphin_file_get_metadata_list (file, NOLPHIN_METADATA_KEY_EMBLEMS));

	return sort_keyword_list_and_remove_duplicates (keywords);
}

/**
 * nolphin_file_set_keywords
 *
 * §35: Tags/Emblems vergeben. Ersetzt die benutzerdefinierten Tags der
 * Datei vollständig durch @keywords (als metadata::emblems über GVFS
 * gespeichert, siehe vfs_file_set_metadata_as_list()). Automatisch
 * erzeugte Schlüsselwörter (schreibgeschützt, unlesbar, symbolische
 * Verknüpfung, Notiz, Favorit - siehe prepend_automatic_keywords())
 * gehören nicht dazu und werden hier nicht berührt.
 * @file: NolphinFile representing the file in question.
 * @keywords: Liste der neuen Tags (list of char *, wird von dieser
 * Funktion nicht übernommen/freigegeben).
 **/
void
nolphin_file_set_keywords (NolphinFile *file,
			    GList *keywords)
{
	g_return_if_fail (NOLPHIN_IS_FILE (file));

	nolphin_file_set_metadata_list (file, NOLPHIN_METADATA_KEY_EMBLEMS, keywords);
}

/**
 * nolphin_file_get_rating
 *
 * §35: Bewertung (0-5 Sterne), über GVFS-Metadaten gespeichert.
 * @file: NolphinFile representing the file in question.
 *
 * Returns: Bewertung von 0 (keine) bis 5.
 **/
int
nolphin_file_get_rating (NolphinFile *file)
{
	int rating;

	if (file == NULL) {
		return 0;
	}

	g_return_val_if_fail (NOLPHIN_IS_FILE (file), 0);

	rating = nolphin_file_get_integer_metadata (file, NOLPHIN_METADATA_KEY_RATING, 0);
	if (rating < 0) {
		rating = 0;
	} else if (rating > 5) {
		rating = 5;
	}
	return rating;
}

/**
 * nolphin_file_set_rating
 *
 * §35: Bewertung (0-5 Sterne) setzen.
 * @file: NolphinFile representing the file in question.
 * @rating: Neue Bewertung, wird auf den Bereich 0-5 begrenzt.
 **/
void
nolphin_file_set_rating (NolphinFile *file,
			  int rating)
{
	g_return_if_fail (NOLPHIN_IS_FILE (file));

	if (rating < 0) {
		rating = 0;
	} else if (rating > 5) {
		rating = 5;
	}

	nolphin_file_set_integer_metadata (file, NOLPHIN_METADATA_KEY_RATING, 0, rating);
}

/**
 * nolphin_file_get_comment
 *
 * §35: Kommentar der Datei lesen (über metadata::annotation, GVFS).
 * @file: NolphinFile representing the file in question.
 *
 * Returns: Neu allozierter Kommentartext (ggf. leerer String), muss mit
 * g_free() freigegeben werden.
 **/
char *
nolphin_file_get_comment (NolphinFile *file)
{
	if (file == NULL) {
		return g_strdup ("");
	}

	g_return_val_if_fail (NOLPHIN_IS_FILE (file), g_strdup (""));

	return nolphin_file_get_metadata (file, NOLPHIN_METADATA_KEY_ANNOTATION, "");
}

/**
 * nolphin_file_set_comment
 *
 * §35: Kommentar der Datei setzen (über metadata::annotation, GVFS).
 * @file: NolphinFile representing the file in question.
 * @comment: Neuer Kommentartext; NULL oder leerer String löscht den
 * Kommentar.
 **/
void
nolphin_file_set_comment (NolphinFile *file,
			   const char *comment)
{
	g_return_if_fail (NOLPHIN_IS_FILE (file));

	nolphin_file_set_metadata (file, NOLPHIN_METADATA_KEY_ANNOTATION, "", comment);
}

/**
 * nolphin_file_is_symbolic_link
 *
 * Check if this file is a symbolic link.
 * @file: NolphinFile representing the file in question.
 *
 * Returns: True if the file is a symbolic link.
 *
 **/
gboolean
nolphin_file_is_symbolic_link (NolphinFile *file)
{
	return file->details->is_symlink;
}

gboolean
nolphin_file_is_mountpoint (NolphinFile *file)
{
	return file->details->is_mountpoint;
}

GMount *
nolphin_file_get_mount (NolphinFile *file)
{
	if (file->details->mount) {
		return g_object_ref (file->details->mount);
	}
	return NULL;
}

static void
file_mount_unmounted (GMount *mount,
		      gpointer data)
{
	NolphinFile *file;

	file = NOLPHIN_FILE (data);

	nolphin_file_invalidate_attributes (file, NOLPHIN_FILE_ATTRIBUTE_MOUNT);
}

void
nolphin_file_set_mount (NolphinFile *file,
			 GMount *mount)
{
	if (file->details->mount) {
		g_signal_handlers_disconnect_by_func (file->details->mount, file_mount_unmounted, file);
		g_object_unref (file->details->mount);
		file->details->mount = NULL;
	}

	if (mount) {
		file->details->mount = g_object_ref (mount);
		g_signal_connect (mount, "unmounted",
				  G_CALLBACK (file_mount_unmounted), file);
	}
}

/**
 * nolphin_file_is_broken_symbolic_link
 *
 * Check if this file is a symbolic link with a missing target.
 * @file: NolphinFile representing the file in question.
 *
 * Returns: True if the file is a symbolic link with a missing target.
 *
 **/
gboolean
nolphin_file_is_broken_symbolic_link (NolphinFile *file)
{
	if (file == NULL) {
		return FALSE;
	}

	g_return_val_if_fail (NOLPHIN_IS_FILE (file), FALSE);

	/* Non-broken symbolic links return the target's type for get_file_type. */
	return nolphin_file_get_file_type (file) == G_FILE_TYPE_SYMBOLIC_LINK;
}

static void
get_fs_free_cb (GObject *source_object,
		GAsyncResult *res,
		gpointer user_data)
{
	NolphinFile *file;
	guint64 free_space;
	GFileInfo *info;

	file = NOLPHIN_FILE (user_data);

	free_space = (guint64)-1;
	info = g_file_query_filesystem_info_finish (G_FILE (source_object),
						    res, NULL);
	if (info) {
		if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_FILESYSTEM_FREE)) {
			free_space = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_FILESYSTEM_FREE);
		}
		g_object_unref (info);
	}

	if (file->details->free_space != free_space) {
		file->details->free_space = free_space;
		nolphin_file_emit_changed (file);
	}

	nolphin_file_unref (file);
}

/**
 * nolphin_file_get_volume_free_space
 * Get a nicely formatted char with free space on the file's volume
 * @file: NolphinFile representing the file in question.
 *
 * Returns: newly-allocated copy of file size in a formatted string
 */
char *
nolphin_file_get_volume_free_space (NolphinFile *file)
{
	GFile *location;
	char *res;
	time_t now;
	int prefix;

	now = time (NULL);
	/* Update first time and then every 2 seconds */
	if (file->details->free_space_read == 0 ||
	    (now - file->details->free_space_read) > 2)  {
		file->details->free_space_read = now;
		location = nolphin_file_get_location (file);
		g_file_query_filesystem_info_async (location,
						    G_FILE_ATTRIBUTE_FILESYSTEM_FREE,
						    0, NULL,
						    get_fs_free_cb,
						    nolphin_file_ref (file));
		g_object_unref (location);
	}

	res = NULL;
	if (file->details->free_space != (guint64)-1) {
		prefix = nolphin_global_preferences_get_size_prefix_preference ();
		res = g_format_size_full (file->details->free_space, prefix);
	}

	return res;
}

/**
 * nolphin_file_get_volume_name
 * Get the path of the volume the file resides on
 * @file: NolphinFile representing the file in question.
 *
 * Returns: newly-allocated copy of the volume name of the target file,
 * if the volume name isn't set, it returns the mount path of the volume
 */
char *
nolphin_file_get_volume_name (NolphinFile *file)
{
	GFile *location;
	char *res;
	GMount *mount;

	res = NULL;

	location = nolphin_file_get_location (file);
	mount = g_file_find_enclosing_mount (location, NULL, NULL);
	if (mount) {
		res = g_mount_get_name (mount);
		g_object_unref (mount);
	}
	g_object_unref (location);

	return res;
}

/**
 * nolphin_file_get_symbolic_link_target_path
 *
 * Get the file path of the target of a symbolic link. It is an error
 * to call this function on a file that isn't a symbolic link.
 * @file: NolphinFile representing the symbolic link in question.
 *
 * Returns: newly-allocated copy of the file path of the target of the symbolic link.
 */
char *
nolphin_file_get_symbolic_link_target_path (NolphinFile *file)
{
	if (!nolphin_file_is_symbolic_link (file)) {
		g_warning ("File has symlink target, but  is not marked as symlink");
	}

	return g_strdup (file->details->symlink_name);
}

/**
 * nolphin_file_get_symbolic_link_target_uri
 *
 * Get the uri of the target of a symbolic link. It is an error
 * to call this function on a file that isn't a symbolic link.
 * @file: NolphinFile representing the symbolic link in question.
 *
 * Returns: newly-allocated copy of the uri of the target of the symbolic link.
 */
char *
nolphin_file_get_symbolic_link_target_uri (NolphinFile *file)
{
	GFile *location, *parent, *target;
	char *target_uri;

	if (!nolphin_file_is_symbolic_link (file)) {
		g_warning ("File has symlink target, but  is not marked as symlink");
	}

	if (file->details->symlink_name == NULL) {
		return NULL;
	} else {
		target = NULL;

		location = nolphin_file_get_location (file);
		parent = g_file_get_parent (location);
		g_object_unref (location);
		if (parent) {
			target = g_file_resolve_relative_path (parent, file->details->symlink_name);
			g_object_unref (parent);
		}

		target_uri = NULL;
		if (target) {
			target_uri = g_file_get_uri (target);
			g_object_unref (target);
		}
		return target_uri;
	}
}

/**
 * nolphin_file_is_nolphin_link
 *
 * Check if this file is a "nolphin link", meaning a historical
 * nolphin xml link file or a desktop file.
 * @file: NolphinFile representing the file in question.
 *
 * Returns: True if the file is a nolphin link.
 *
 **/
gboolean
nolphin_file_is_nolphin_link (NolphinFile *file)
{
	if (file->details->mime_type == NULL) {
		return FALSE;
	}
	return g_content_type_equals (file->details->mime_type, "application/x-desktop");
}

/**
 * nolphin_file_is_directory
 *
 * Check if this file is a directory.
 * @file: NolphinFile representing the file in question.
 *
 * Returns: TRUE if @file is a directory.
 *
 **/
gboolean
nolphin_file_is_directory (NolphinFile *file)
{
	return nolphin_file_get_file_type (file) == G_FILE_TYPE_DIRECTORY;
}

/**
 * nolphin_file_is_user_special_directory
 *
 * Check if this file is a special platform directory.
 * @file: NolphinFile representing the file in question.
 * @special_directory: GUserDirectory representing the type to test for
 *
 * Returns: TRUE if @file is a special directory of the given kind.
 */
gboolean
nolphin_file_is_user_special_directory (NolphinFile *file,
					 GUserDirectory special_directory)
{
	gboolean is_special_dir;
	const gchar *special_dir;

	special_dir = g_get_user_special_dir (special_directory);
	is_special_dir = FALSE;

	if (special_dir) {
		GFile *loc;
		GFile *special_gfile;

		loc = nolphin_file_get_location (file);
		special_gfile = g_file_new_for_path (special_dir);
		is_special_dir = g_file_equal (loc, special_gfile);
		g_object_unref (special_gfile);
		g_object_unref (loc);
	}

	return is_special_dir;
}

static const char * fallback_mime_types[] = { "application/x-gtar",
                                              "application/x-zip",
                                              "application/x-zip-compressed",
                                              "application/zip",
                                              "application/x-zip",
                                              "application/x-tar",
                                              "application/x-7z-compressed",
                                              "application/x-rar",
                                              "application/x-rar-compressed",
                                              "application/x-jar",
                                              "application/x-java-archive",
                                              "application/x-war",
                                              "application/x-ear",
                                              "application/x-arj",
                                              "application/x-gzip",
                                              "application/x-bzip-compressed-tar",
                                              "application/x-compressed-tar",
                                              "application/x-xz-compressed-tar" };

gboolean
nolphin_file_is_archive (NolphinFile *file)
{
    gchar *mime_type;
    gchar **file_roller_mimetypes;
    guint i;

    g_return_val_if_fail (file != NULL, FALSE);

    mime_type = nolphin_file_get_mime_type (file);

    file_roller_mimetypes = nolphin_global_preferences_get_fileroller_mimetypes ();

    if (file_roller_mimetypes != NULL) {
        for (i = 0; i < g_strv_length (file_roller_mimetypes); i++) {
            if (!strcmp (mime_type, file_roller_mimetypes[i])) {
                g_free (mime_type);
                return TRUE;
            }
        }
    } else {
        for (i = 0; i < G_N_ELEMENTS (fallback_mime_types); i++) {
            if (!strcmp (mime_type, fallback_mime_types[i])) {
                g_free (mime_type);
                return TRUE;
            }
        }
    }

    g_free (mime_type);

    return FALSE;
}


/**
 * nolphin_file_is_in_trash
 *
 * Check if this file is a file in trash.
 * @file: NolphinFile representing the file in question.
 *
 * Returns: TRUE if @file is in a trash.
 *
 **/
gboolean
nolphin_file_is_in_trash (NolphinFile *file)
{
	g_assert (NOLPHIN_IS_FILE (file));

	return nolphin_directory_is_in_trash (file->details->directory);
}

/**
 * nolphin_file_is_in_recent
 *
 * Check if this file is a file in Recent.
 * @file: NolphinFile representing the file in question.
 *
 * Returns: TRUE if @file is in Recent.
 *
 **/
gboolean
nolphin_file_is_in_recent (NolphinFile *file)
{
   g_assert (NOLPHIN_IS_FILE (file));

   return nolphin_directory_is_in_recent (file->details->directory);
}

/**
 * nolphin_file_is_in_favorites
 *
 * Check if this file is a Favorite file.
 * @file: NolphinFile representing the file in question.
 *
 * Returns: TRUE if @file is a Favorite.
 *
 **/
gboolean
nolphin_file_is_in_favorites (NolphinFile *file)
{
   g_assert (NOLPHIN_IS_FILE (file));

   return nolphin_directory_is_in_favorites (file->details->directory);
}

/**
 * nolphin_file_is_in_search
 *
 * Check if this file is a search result.
 * @file: NolphinFile representing the file in question.
 *
 * Returns: TRUE if @file is a search result.
 *
 **/
gboolean
nolphin_file_is_in_search (NolphinFile *file)
{
   g_assert (NOLPHIN_IS_FILE (file));

   return nolphin_directory_is_in_search (file->details->directory);
}

/**
 * nolphin_file_is_unavailable_favorite
 *
 * Check if this file is currently an unreachable Favorite file. This is useful
 * for the NolphinView classes to set an appropriate font weight for unavailable files
 * like unmounted locations.
 * @file: NolphinFile representing the file in question.
 *
 * Returns: TRUE if @file is a Favorite and is available.
 *
 **/
gboolean
nolphin_file_is_unavailable_favorite (NolphinFile *file)
{
    g_assert (NOLPHIN_IS_FILE (file));

    if (!nolphin_directory_is_in_favorites (file->details->directory)) {
        return FALSE;
    }

    return !nolphin_file_get_boolean_metadata (file, NOLPHIN_METADATA_KEY_FAVORITE_AVAILABLE, TRUE);
}

/**
 * nolphin_file_is_in_admin
 *
 * Check if this file is using admin backend.
 * @file: NolphinFile representing the file in question.
 *
 * Returns: TRUE if @file is using admin backend.
 *
 **/
gboolean
nolphin_file_is_in_admin (NolphinFile *file)
{
    g_assert (NOLPHIN_IS_FILE (file));

    return nolphin_directory_is_in_admin (file->details->directory);
}

GError *
nolphin_file_get_file_info_error (NolphinFile *file)
{
	if (!file->details->get_info_failed) {
		return NULL;
	}

	return file->details->get_info_error;
}

/**
 * nolphin_file_contains_text
 *
 * Check if this file contains text.
 * This is private and is used to decide whether or not to read the top left text.
 * @file: NolphinFile representing the file in question.
 *
 * Returns: TRUE if @file has a text MIME type.
 *
 **/
gboolean
nolphin_file_contains_text (NolphinFile *file)
{
	if (file == NULL) {
		return FALSE;
	}

	/* All text files inherit from text/plain */
	return nolphin_file_is_mime_type (file, "text/plain");
}

/**
 * nolphin_file_is_executable
 *
 * Check if this file is executable at all.
 * @file: NolphinFile representing the file in question.
 *
 * Returns: TRUE if any of the execute bits are set. FALSE if
 * not, or if the permissions are unknown.
 *
 **/
gboolean
nolphin_file_is_executable (NolphinFile *file)
{
	if (!file->details->has_permissions) {
		/* File's permissions field is not valid.
		 * Can't access specific permissions, so return FALSE.
		 */
		return FALSE;
	}

	return file->details->can_execute;
}

char *
nolphin_file_get_filesystem_id (NolphinFile *file)
{
	return g_strdup (file->details->filesystem_id);
}

NolphinFile *
nolphin_file_get_trash_original_file (NolphinFile *file)
{
	GFile *location;
	NolphinFile *original_file;

	original_file = NULL;

	if (file->details->trash_orig_path != NULL) {
		location = g_file_new_for_path (file->details->trash_orig_path);
		original_file = nolphin_file_get (location);
		g_object_unref (location);
	}

	return original_file;

}

void
nolphin_file_mark_gone (NolphinFile *file)
{
	NolphinDirectory *directory;

	if (file->details->is_gone)
		return;

	file->details->is_gone = TRUE;

	update_links_if_target (file);

	/* Drop it from the symlink hash ! */
	remove_from_link_hash_table (file);

	/* Let the directory know it's gone. */
	directory = file->details->directory;
	/* Hold a temporary ref so the object stays alive through both
	 * nolphin_directory_remove_file() (which may drop the last directory ref
	 * and free the object) AND the nolphin_file_clear_info() call below.
	 * Without this, remove_file can free the NolphinFile and clear_info then
	 * dereferences the freed pointer, causing a SIGSEGV (bug #3712). */
	nolphin_file_ref (file);
	if (!nolphin_file_is_self_owned (file)) {
		nolphin_directory_remove_file (directory, file);
	}

	nolphin_file_clear_info (file);
	nolphin_file_unref (file);

	/* FIXME bugzilla.gnome.org 42429:
	 * Maybe we can get rid of the name too eventually, but
	 * for now that would probably require too many if statements
	 * everywhere anyone deals with the name. Maybe we can give it
	 * a hard-coded "<deleted>" name or something.
	 */
}

/**
 * nolphin_file_changed
 *
 * Notify the user that this file has changed.
 * @file: NolphinFile representing the file in question.
 **/
void
nolphin_file_changed (NolphinFile *file)
{
	GList fake_list;

	g_return_if_fail (NOLPHIN_IS_FILE (file));

	if (nolphin_file_is_self_owned (file)) {
		nolphin_file_emit_changed (file);
	} else {
		fake_list.data = file;
		fake_list.next = NULL;
		fake_list.prev = NULL;
		nolphin_directory_emit_change_signals
			(file->details->directory, &fake_list);
	}
}

/**
 * nolphin_file_updated_deep_count_in_progress
 *
 * Notify clients that a newer deep count is available for
 * the directory in question.
 */
void
nolphin_file_updated_deep_count_in_progress (NolphinFile *file) {
	GList *link_files, *node;

	g_assert (NOLPHIN_IS_FILE (file));
	g_assert (nolphin_file_is_directory (file));

	/* Send out a signal. */
	g_signal_emit (file, signals[UPDATED_DEEP_COUNT_IN_PROGRESS], 0, file);

	/* Tell link files pointing to this object about the change. */
	link_files = get_link_files (file);
	for (node = link_files; node != NULL; node = node->next) {
		nolphin_file_updated_deep_count_in_progress (NOLPHIN_FILE (node->data));
	}
	nolphin_file_list_free (link_files);
}

/**
 * nolphin_file_emit_changed
 *
 * Emit a file changed signal.
 * This can only be called by the directory, since the directory
 * also has to emit a files_changed signal.
 *
 * @file: NolphinFile representing the file in question.
 **/
void
nolphin_file_emit_changed (NolphinFile *file)
{
	GList *link_files, *p;

	g_assert (NOLPHIN_IS_FILE (file));

	nolphin_file_invalidate_sort_collation_key (file);

	/* Send out a signal. */
	g_signal_emit (file, signals[CHANGED], 0, file);

	/* Tell link files pointing to this object about the change. */
	link_files = get_link_files (file);
	for (p = link_files; p != NULL; p = p->next) {
		if (p->data != file) {
			nolphin_file_changed (NOLPHIN_FILE (p->data));
		}
	}
	nolphin_file_list_free (link_files);
}

/**
 * nolphin_file_is_gone
 *
 * Check if a file has already been deleted.
 * @file: NolphinFile representing the file in question.
 *
 * Returns: TRUE if the file is already gone.
 **/
gboolean
nolphin_file_is_gone (NolphinFile *file)
{
	g_return_val_if_fail (NOLPHIN_IS_FILE (file), FALSE);

	return file->details->is_gone;
}

/**
 * nolphin_file_is_not_yet_confirmed
 *
 * Check if we're in a state where we don't know if a file really
 * exists or not, before the initial I/O is complete.
 * @file: NolphinFile representing the file in question.
 *
 * Returns: TRUE if the file is already gone.
 **/
gboolean
nolphin_file_is_not_yet_confirmed (NolphinFile *file)
{
	g_return_val_if_fail (NOLPHIN_IS_FILE (file), FALSE);

	return !file->details->got_file_info;
}

/**
 * nolphin_file_check_if_ready
 *
 * Check whether the values for a set of file attributes are
 * currently available, without doing any additional work. This
 * is useful for callers that want to reflect updated information
 * when it is ready but don't want to force the work required to
 * obtain the information, which might be slow network calls, e.g.
 *
 * @file: The file being queried.
 * @file_attributes: A bit-mask with the desired information.
 *
 * Return value: TRUE if all of the specified attributes are currently readable.
 */
gboolean
nolphin_file_check_if_ready (NolphinFile *file,
			      NolphinFileAttributes file_attributes)
{
	/* To be parallel with call_when_ready, return
	 * TRUE for NULL file.
	 */
	if (file == NULL) {
		return TRUE;
	}

	g_return_val_if_fail (NOLPHIN_IS_FILE (file), FALSE);

	return NOLPHIN_FILE_CLASS (G_OBJECT_GET_CLASS (file))->check_if_ready (file, file_attributes);
}

void
nolphin_file_call_when_ready (NolphinFile *file,
			       NolphinFileAttributes file_attributes,
			       NolphinFileCallback callback,
			       gpointer callback_data)

{
	if (file == NULL) {
		(* callback) (file, callback_data);
		return;
	}

	g_return_if_fail (NOLPHIN_IS_FILE (file));

	NOLPHIN_FILE_CLASS (G_OBJECT_GET_CLASS (file))->call_when_ready
		(file, file_attributes, callback, callback_data);
}

void
nolphin_file_cancel_call_when_ready (NolphinFile *file,
				      NolphinFileCallback callback,
				      gpointer callback_data)
{
	g_return_if_fail (callback != NULL);

	if (file == NULL) {
		return;
	}

	g_return_if_fail (NOLPHIN_IS_FILE (file));

	NOLPHIN_FILE_CLASS (G_OBJECT_GET_CLASS (file))->cancel_call_when_ready
		(file, callback, callback_data);
}

static GString *
add_line (GString *string, const gchar *add, gboolean prefix_newline)
{
    if (prefix_newline)
        string = g_string_append (string, "\n");
    return g_string_append (string, add);
}

gchar *
nolphin_file_construct_tooltip (NolphinFile *file, NolphinFileTooltipFlags flags, gpointer search_dir)
{
    gchar *scheme = nolphin_file_get_uri_scheme (file);
    gchar *nice = NULL;
    gchar *tmp = NULL;
    gchar *date;

    if (g_strcmp0 (scheme, "x-nolphin-desktop") == 0) {
        g_free (scheme);
        return NULL;
    }

    GString *string = g_string_new ("");

    tmp = nolphin_file_get_display_name (file);
    nice = g_strdup_printf (_("Name: %s"), tmp);
    string = add_line (string, nice, FALSE);
    g_free (nice);
    g_free (tmp);

    if (flags & NOLPHIN_FILE_TOOLTIP_FLAGS_FILE_TYPE) {
        tmp = nolphin_file_get_detailed_type_as_string (file);
        nice = g_strdup_printf (_("Dateityp: %s"), tmp);
        string = add_line (string, nice, TRUE);
        g_free (tmp);
        g_free (nice);
    }

    if (nolphin_file_is_directory (file)) {
        guint item_count;
        if (nolphin_file_get_directory_item_count (file, &item_count, NULL)) {
            gchar *launchpad_sucks = THOU_TO_STR (item_count);
            gchar *count = g_strdup_printf (ngettext ("%s Objekt", "%s Objekte", item_count), launchpad_sucks);
            g_free (launchpad_sucks);
            nice = g_strdup_printf (_("Enthält: %s"), count);
            string = add_line (string, nice, TRUE);
            g_free (count);
            g_free (nice);
        }
    } else {
        gchar *size_string;
        gint prefix;
        prefix = nolphin_global_preferences_get_size_prefix_preference ();
        size_string = g_format_size_full (nolphin_file_get_size (file), prefix);
        nice = g_strdup_printf (_("Größe: %s"), size_string);
        string = add_line (string, nice, TRUE);
        g_free (size_string);
        g_free (nice);
    }

    if (flags & NOLPHIN_FILE_TOOLTIP_FLAGS_ACCESS_DATE) {
        date = nolphin_file_get_date_as_string (file, NOLPHIN_DATE_TYPE_ACCESSED, TRUE);
        tmp = g_strdup_printf (_("Zugegriffen: %s"), date);
        g_free (date);
        string = add_line (string, tmp, TRUE);
        g_free (tmp);
    }

    if (flags & NOLPHIN_FILE_TOOLTIP_FLAGS_MOD_DATE) {
        date = nolphin_file_get_date_as_string (file, NOLPHIN_DATE_TYPE_MODIFIED, TRUE);
        tmp = g_strdup_printf (_("Geändert: %s"), date);
        g_free (date);
        string = add_line (string, tmp, TRUE);
        g_free (tmp);
    }

    if (flags & NOLPHIN_FILE_TOOLTIP_FLAGS_CREATED_DATE) {
        date = nolphin_file_get_date_as_string (file, NOLPHIN_DATE_TYPE_CREATED, TRUE);
        tmp = g_strdup_printf (_("Erstellt: %s"), date);
        g_free (date);
        string = add_line (string, tmp, TRUE);
        g_free (tmp);
    }

    if (flags & NOLPHIN_FILE_TOOLTIP_FLAGS_PATH) {
        gchar *truncated;
        tmp = nolphin_file_get_where_string (file);
        truncated = eel_str_middle_truncate (tmp, 60);
        nice = g_strdup_printf (_("Ort: %s"), truncated);
        string = add_line (string, nice, TRUE);
        g_free (tmp);
        g_free (truncated);
        g_free (nice);

        if (nolphin_file_is_symbolic_link (file) && !nolphin_file_is_in_favorites (file)) {
            tmp = nolphin_file_get_symbolic_link_target_path (file);
            const gchar *existing_i18n = _("Verknüpfungsziel:");
            nice = g_strdup_printf ("%s %s", existing_i18n, tmp);
            string = add_line (string, nice, TRUE);
            g_free (tmp);
            g_free (nice);
        }
    }

    gchar *escaped = g_markup_escape_text (string->str, -1);
    string = g_string_assign (string, escaped);
    g_free (escaped);

    if (search_dir != NULL) {
        gchar *snippet = nolphin_file_get_search_result_snippet (file, search_dir);
        if (snippet != NULL) {
            gchar *ellipsized = g_strdup_printf ("%s", snippet);
            string = add_line (string, "\n----------------------", FALSE);
            string = add_line (string, ellipsized, TRUE);
            g_free (snippet);
            g_free (ellipsized);
        }
    }

    g_free (scheme);
    return g_string_free (string, FALSE);
}

gint
nolphin_file_get_monitor_number (NolphinFile *file)
{
    if (file->details->desktop_monitor == -1) {
        file->details->desktop_monitor = nolphin_file_get_integer_metadata (file, NOLPHIN_METADATA_KEY_MONITOR, -1);
    }

    return file->details->desktop_monitor;
}

void
nolphin_file_set_monitor_number (NolphinFile *file, gint monitor)
{
    nolphin_file_set_integer_metadata (file, NOLPHIN_METADATA_KEY_MONITOR, -1, monitor);
    file->details->desktop_monitor = monitor;
}

void
nolphin_file_get_position (NolphinFile *file, GdkPoint *point)
{
    gint x, y;

    if (file->details->cached_position_x == -1) {
        char *position_string;
        gboolean position_good;
        char c;

        /* Get the current position of this icon from the metadata. */
        position_string = nolphin_file_get_metadata (file, NOLPHIN_METADATA_KEY_ICON_POSITION, "");

        position_good = sscanf (position_string, " %d , %d %c", &x, &y, &c) == 2;
        g_free (position_string);

        if (position_good) {
            point->x = x;
            point->y = y;
        } else {
            point->x = -1;
            point->y = -1;
        }

        file->details->cached_position_x = point->x;
        file->details->cached_position_y = point->y;
    } else {
        point->x = file->details->cached_position_x;
        point->y = file->details->cached_position_y;
    }
}

void
nolphin_file_set_position (NolphinFile *file, gint x, gint y)
{
    gchar *position_string;

    if (x > -1 && y > -1) {
        position_string = g_strdup_printf ("%d,%d", x, y);
    } else {
        position_string = NULL;
    }
    nolphin_file_set_metadata (file, NOLPHIN_METADATA_KEY_ICON_POSITION, NULL, position_string);

    file->details->cached_position_x = x;
    file->details->cached_position_y = y;

    g_free (position_string);
}

gboolean
nolphin_file_get_is_desktop_orphan (NolphinFile *file)
{
    return file->details->is_desktop_orphan;
}

void
nolphin_file_set_is_desktop_orphan (NolphinFile *file,
                                 gboolean  is_desktop_orphan)
{
    file->details->is_desktop_orphan = is_desktop_orphan;
}

gboolean
nolphin_file_has_thumbnail_access_problem   (NolphinFile *file)
{
    return file->details->thumbnail_access_problem;
}

static void
invalidate_directory_count (NolphinFile *file)
{
	file->details->directory_count_is_up_to_date = FALSE;
}

static void
invalidate_deep_counts (NolphinFile *file)
{
	file->details->deep_counts_status = NOLPHIN_REQUEST_NOT_STARTED;
}

static void
invalidate_mime_list (NolphinFile *file)
{
	file->details->mime_list_is_up_to_date = FALSE;
}

static void
invalidate_file_info (NolphinFile *file)
{
	file->details->file_info_is_up_to_date = FALSE;
}

static void
invalidate_link_info (NolphinFile *file)
{
	file->details->link_info_is_up_to_date = FALSE;
}

static void
invalidate_thumbnail (NolphinFile *file)
{
	file->details->thumbnail_is_up_to_date = FALSE;
}

static void
invalidate_mount (NolphinFile *file)
{
	file->details->mount_is_up_to_date = FALSE;
}

static void
invalidate_favorite_check (NolphinFile *file)
{
    file->details->favorite_checked = FALSE;
}

void
nolphin_file_invalidate_extension_info_internal (NolphinFile *file)
{
	if (file->details->pending_info_providers)
		g_list_free_full (file->details->pending_info_providers, g_object_unref);

	file->details->pending_info_providers =
		nolphin_module_get_extensions_for_type (NOLPHIN_TYPE_INFO_PROVIDER);
}

void
nolphin_file_set_load_deferred_attrs (NolphinFile                  *file,
                                   NolphinFileLoadDeferredAttrs  load_deferred)
{
    file->details->load_deferred_attrs = load_deferred;
}

NolphinFileLoadDeferredAttrs
nolphin_file_get_load_deferred_attrs (NolphinFile *file)
{
    return file->details->load_deferred_attrs;
}

void
nolphin_file_invalidate_attributes_internal (NolphinFile *file,
					      NolphinFileAttributes file_attributes)
{
	Request request;

	if (file == NULL) {
		return;
	}

	if (NOLPHIN_IS_DESKTOP_ICON_FILE (file)) {
		/* Desktop icon files are always up to date.
		 * If we invalidate their attributes they
		 * will lose data, so we just ignore them.
		 */
		return;
	}

	request = nolphin_directory_set_up_request (file_attributes);

	if (REQUEST_WANTS_TYPE (request, REQUEST_DIRECTORY_COUNT)) {
		invalidate_directory_count (file);
	}
	if (REQUEST_WANTS_TYPE (request, REQUEST_DEEP_COUNT)) {
		invalidate_deep_counts (file);
	}
	if (REQUEST_WANTS_TYPE (request, REQUEST_MIME_LIST)) {
		invalidate_mime_list (file);
	}
	if (REQUEST_WANTS_TYPE (request, REQUEST_FILE_INFO)) {
		invalidate_file_info (file);
	}
	if (REQUEST_WANTS_TYPE (request, REQUEST_LINK_INFO)) {
		invalidate_link_info (file);
	}
	if (REQUEST_WANTS_TYPE (request, REQUEST_EXTENSION_INFO)) {
		nolphin_file_invalidate_extension_info_internal (file);
	}
	if (REQUEST_WANTS_TYPE (request, REQUEST_THUMBNAIL)) {
		invalidate_thumbnail (file);
	}
	if (REQUEST_WANTS_TYPE (request, REQUEST_MOUNT)) {
		invalidate_mount (file);
	}
    if (REQUEST_WANTS_TYPE (request, REQUEST_FAVORITE_CHECK)) {
        invalidate_favorite_check (file);
    }
	/* FIXME bugzilla.gnome.org 45075: implement invalidating metadata */
}

gboolean
nolphin_file_has_open_window (NolphinFile *file)
{
	return file->details->has_open_window;
}

void
nolphin_file_set_has_open_window (NolphinFile *file,
				   gboolean has_open_window)
{
	has_open_window = (has_open_window != FALSE);

	if (file->details->has_open_window != has_open_window) {
		file->details->has_open_window = has_open_window;
		nolphin_file_changed (file);
	}
}


gboolean
nolphin_file_is_thumbnailing (NolphinFile *file)
{
	g_return_val_if_fail (NOLPHIN_IS_FILE (file), FALSE);

	return file->details->is_thumbnailing;
}

void
nolphin_file_set_is_thumbnailing (NolphinFile *file,
				   gboolean is_thumbnailing)
{
	g_return_if_fail (NOLPHIN_IS_FILE (file));

	file->details->is_thumbnailing = is_thumbnailing;
}

/**
 * nolphin_file_invalidate_attributes
 *
 * Invalidate the specified attributes and force a reload.
 * @file: NolphinFile representing the file in question.
 * @file_attributes: attributes to froget.
 **/

void
nolphin_file_invalidate_attributes (NolphinFile *file,
				     NolphinFileAttributes file_attributes)
{
	/* Cancel possible in-progress loads of any of these attributes */
	nolphin_directory_cancel_loading_file_attributes (file->details->directory,
							   file,
							   file_attributes);

	/* Actually invalidate the values */
	nolphin_file_invalidate_attributes_internal (file, file_attributes);

	nolphin_directory_add_file_to_work_queue (file->details->directory, file);

	/* Kick off I/O if necessary */
	nolphin_directory_async_state_changed (file->details->directory);
}

NolphinFileAttributes
nolphin_file_get_all_attributes (void)
{
	return  NOLPHIN_FILE_ATTRIBUTE_INFO |
		NOLPHIN_FILE_ATTRIBUTE_LINK_INFO |
		NOLPHIN_FILE_ATTRIBUTE_DEEP_COUNTS |
		NOLPHIN_FILE_ATTRIBUTE_DIRECTORY_ITEM_COUNT |
		NOLPHIN_FILE_ATTRIBUTE_DIRECTORY_ITEM_MIME_TYPES |
		NOLPHIN_FILE_ATTRIBUTE_EXTENSION_INFO |
		NOLPHIN_FILE_ATTRIBUTE_THUMBNAIL |
		NOLPHIN_FILE_ATTRIBUTE_MOUNT |
        NOLPHIN_FILE_ATTRIBUTE_FAVORITE_CHECK;
}

void
nolphin_file_invalidate_all_attributes (NolphinFile *file)
{
	NolphinFileAttributes all_attributes;
	all_attributes = nolphin_file_get_all_attributes ();
	nolphin_file_invalidate_attributes (file, all_attributes);
}


/**
 * nolphin_file_dump
 *
 * Debugging call, prints out the contents of the file
 * fields.
 *
 * @file: file to dump.
 **/
void
nolphin_file_dump (NolphinFile *file)
{
	long size = file->details->deep_size;
	char *uri;
	const char *file_kind;

	uri = nolphin_file_get_uri (file);
	g_print ("uri: %s \n", uri);
	if (!file->details->got_file_info) {
		g_print ("no file info \n");
	} else if (file->details->get_info_failed) {
		g_print ("failed to get file info \n");
	} else {
		g_print ("size: %ld \n", size);
		switch (file->details->type) {
		case G_FILE_TYPE_REGULAR:
			file_kind = "regular file";
			break;
		case G_FILE_TYPE_DIRECTORY:
			file_kind = "folder";
			break;
		case G_FILE_TYPE_SPECIAL:
			file_kind = "special";
			break;
		case G_FILE_TYPE_SYMBOLIC_LINK:
			file_kind = "symbolic link";
			break;
		case G_FILE_TYPE_UNKNOWN:
		case G_FILE_TYPE_MOUNTABLE:
		case G_FILE_TYPE_SHORTCUT:
        /* FIXME should probably show details for these file types */
		default:
			file_kind = "unknown";
			break;
		}
		g_print ("kind: %s \n", file_kind);
		if (file->details->type == G_FILE_TYPE_SYMBOLIC_LINK) {
			g_print ("link to %s \n", file->details->symlink_name);
			/* FIXME bugzilla.gnome.org 42430: add following of symlinks here */
		}
		/* FIXME bugzilla.gnome.org 42431: add permissions and other useful stuff here */
	}
	g_free (uri);
}

/**
 * nolphin_file_list_ref
 *
 * Ref all the files in a list.
 * @list: GList of files.
 **/
GList *
nolphin_file_list_ref (GList *list)
{
	g_list_foreach (list, (GFunc) nolphin_file_ref, NULL);
	return list;
}

/**
 * nolphin_file_list_unref
 *
 * Unref all the files in a list.
 * @list: GList of files.
 **/
void
nolphin_file_list_unref (GList *list)
{
	g_list_foreach (list, (GFunc) nolphin_file_unref, NULL);
}

/**
 * nolphin_file_list_free
 *
 * Free a list of files after unrefing them.
 * @list: GList of files.
 **/
void
nolphin_file_list_free (GList *list)
{
	nolphin_file_list_unref (list);
	g_list_free (list);
}

/**
 * nolphin_file_list_copy
 *
 * Copy the list of files, making a new ref of each,
 * @list: GList of files.
 **/
GList *
nolphin_file_list_copy (GList *list)
{
	return g_list_copy (nolphin_file_list_ref (list));
}

GList *
nolphin_file_list_from_uris (GList *uri_list)
{
	GList *l, *file_list;
	const char *uri;
	GFile *file;

	file_list = NULL;

	for (l = uri_list; l != NULL; l = l->next) {
		uri = l->data;
		file = g_file_new_for_uri (uri);
		file_list = g_list_prepend (file_list, file);
	}
	return g_list_reverse (file_list);
}

static gboolean
get_attributes_for_default_sort_type (NolphinFile *file,
                      gboolean *is_recent,
				      gboolean *is_download,
				      gboolean *is_trash)
{
	gboolean is_recent_dir, is_download_dir, is_desktop_dir, is_trash_dir, retval;

    *is_recent = FALSE;
	*is_download = FALSE;
	*is_trash = FALSE;
	retval = FALSE;

	/* special handling for certain directories */
	if (file && nolphin_file_is_directory (file)) {
        is_recent_dir =
            nolphin_file_is_in_recent (file);
		is_download_dir =
			nolphin_file_is_user_special_directory (file, G_USER_DIRECTORY_DOWNLOAD);
		is_desktop_dir =
			nolphin_file_is_user_special_directory (file, G_USER_DIRECTORY_DESKTOP);
		is_trash_dir =
			nolphin_file_is_in_trash (file);

		if (is_download_dir && !is_desktop_dir) {
			*is_download = TRUE;
			retval = TRUE;
		} else if (is_trash_dir) {
			*is_trash = TRUE;
			retval = TRUE;
		} else if (is_recent_dir) {
            *is_recent = TRUE;
            retval = TRUE;
        }
	}

	return retval;
}

NolphinFileSortType
nolphin_file_get_default_sort_type (NolphinFile *file,
				     gboolean *reversed)
{
	NolphinFileSortType retval;
	gboolean is_recent, is_download, is_trash, res;

	retval = NOLPHIN_FILE_SORT_NONE;

    is_recent = is_download = is_trash = FALSE;
    res = get_attributes_for_default_sort_type (file, &is_recent, &is_download, &is_trash);

	if (res) {
        if (is_recent) {
			retval = NOLPHIN_FILE_SORT_BY_ATIME;
        } else if (is_download) {
            retval = NOLPHIN_FILE_SORT_BY_MTIME;
		} else if (is_trash) {
			retval = NOLPHIN_FILE_SORT_BY_TRASHED_TIME;
		}

		if (reversed != NULL) {
			*reversed = res;
		}
	}

	return retval;
}

const gchar *
nolphin_file_get_default_sort_attribute (NolphinFile *file,
					  gboolean *reversed)
{
	const gchar *retval;
	gboolean is_recent, is_download, is_trash, res;

	retval = NULL;
	is_download = is_trash = FALSE;
	res = get_attributes_for_default_sort_type (file, &is_recent, &is_download, &is_trash);

	if (res) {
        if (is_recent || is_download) {
			retval = g_quark_to_string (attribute_date_modified_q);
		} else if (is_trash) {
			retval = g_quark_to_string (attribute_trashed_on_q);
		}

		if (reversed != NULL) {
			*reversed = res;
		}
	}

	return retval;
}

static int
compare_by_display_name_cover (gconstpointer a, gconstpointer b)
{
	return compare_by_display_name (NOLPHIN_FILE (a), NOLPHIN_FILE (b));
}

/**
 * nolphin_file_list_sort_by_display_name
 *
 * Sort the list of files by file name.
 * @list: GList of files.
 **/
GList *
nolphin_file_list_sort_by_display_name (GList *list)
{
	return g_list_sort (list, compare_by_display_name_cover);
}

static GList *ready_data_list = NULL;

typedef struct
{
	GList *file_list;
	GList *remaining_files;
	NolphinFileListCallback callback;
	gpointer callback_data;
} FileListReadyData;

static void
file_list_ready_data_free (FileListReadyData *data)
{
	GList *l;

	l = g_list_find (ready_data_list, data);
	if (l != NULL) {
		ready_data_list = g_list_delete_link (ready_data_list, l);

		nolphin_file_list_free (data->file_list);
		g_list_free (data->remaining_files);
		g_free (data);
	}
}

static FileListReadyData *
file_list_ready_data_new (GList *file_list,
			  NolphinFileListCallback callback,
			  gpointer callback_data)
{
	FileListReadyData *data;

	data = g_new0 (FileListReadyData, 1);
	data->file_list = nolphin_file_list_copy (file_list);
	data->remaining_files = g_list_copy (file_list);
	data->callback = callback;
	data->callback_data = callback_data;

	ready_data_list = g_list_prepend (ready_data_list, data);

	return data;
}

static void
file_list_file_ready_callback (NolphinFile *file,
			       gpointer user_data)
{
	FileListReadyData *data;

	data = user_data;
	data->remaining_files = g_list_remove (data->remaining_files, file);

	if (data->remaining_files == NULL) {
		if (data->callback) {
			(*data->callback) (data->file_list, data->callback_data);
		}

		file_list_ready_data_free (data);
	}
}

void
nolphin_file_list_call_when_ready (GList *file_list,
				    NolphinFileAttributes attributes,
				    NolphinFileListHandle **handle,
				    NolphinFileListCallback callback,
				    gpointer callback_data)
{
	GList *l;
	FileListReadyData *data;
	NolphinFile *file;

	g_return_if_fail (file_list != NULL);

	data = file_list_ready_data_new
		(file_list, callback, callback_data);

	if (handle) {
		*handle = (NolphinFileListHandle *) data;
	}


	l = file_list;
	while (l != NULL) {
		file = NOLPHIN_FILE (l->data);
		/* Need to do this here, as the list can be modified by this call */
		l = l->next;
		nolphin_file_call_when_ready (file,
					       attributes,
					       file_list_file_ready_callback,
					       data);
	}
}

void
nolphin_file_list_cancel_call_when_ready (NolphinFileListHandle *handle)
{
	GList *l;
	NolphinFile *file;
	FileListReadyData *data;

	g_return_if_fail (handle != NULL);

	data = (FileListReadyData *) handle;

	l = g_list_find (ready_data_list, data);
	if (l != NULL) {
		for (l = data->remaining_files; l != NULL; l = l->next) {
			file = NOLPHIN_FILE (l->data);

			NOLPHIN_FILE_CLASS (G_OBJECT_GET_CLASS (file))->cancel_call_when_ready
				(file, file_list_file_ready_callback, data);
		}

		file_list_ready_data_free (data);
	}
}

static void
thumbnail_limit_changed_callback (gpointer user_data)
{
	g_settings_get (nolphin_preferences,
			NOLPHIN_PREFERENCES_IMAGE_FILE_THUMBNAIL_LIMIT,
			"t", &cached_thumbnail_limit);

	/* Tell the world that icons might have changed. We could invent a narrower-scope
	 * signal to mean only "thumbnails might have changed" if this ends up being slow
	 * for some reason.
	 */
	emit_change_signals_for_all_files_in_all_directories ();
}

static void
thumbnail_size_changed_callback (gpointer user_data)
{
	cached_thumbnail_size = g_settings_get_int (nolphin_icon_view_preferences,
						    NOLPHIN_PREFERENCES_ICON_VIEW_THUMBNAIL_SIZE);

    scaled_cached_thumbnail_size = (double) cached_thumbnail_size / (double) NOLPHIN_ICON_SIZE_STANDARD;
	/* Tell the world that icons might have changed. We could invent a narrower-scope
	 * signal to mean only "thumbnails might have changed" if this ends up being slow
	 * for some reason.
	 */
	emit_change_signals_for_all_files_in_all_directories ();
}

static void
show_thumbnails_changed_callback (gpointer user_data)
{
	show_image_thumbs = g_settings_get_enum (nolphin_preferences, NOLPHIN_PREFERENCES_SHOW_IMAGE_FILE_THUMBNAILS);

	/* Tell the world that icons might have changed. We could invent a narrower-scope
	 * signal to mean only "thumbnails might have changed" if this ends up being slow
	 * for some reason.
	 */
	emit_change_signals_for_all_files_in_all_directories ();
}

static void
mime_type_data_changed_callback (GObject *signaller, gpointer user_data)
{
	/* Tell the world that icons might have changed. We could invent a narrower-scope
	 * signal to mean only "thumbnails might have changed" if this ends up being slow
	 * for some reason.
	 */
	emit_change_signals_for_all_files_in_all_directories ();
}

static void
icon_theme_changed_callback (GtkIconTheme *icon_theme,
			     gpointer user_data)
{
	/* Clear all pixmap caches as the icon => pixmap lookup changed */
	nolphin_icon_info_clear_caches ();

	/* Tell the world that icons might have changed. We could invent a narrower-scope
	 * signal to mean only "thumbnails might have changed" if this ends up being slow
	 * for some reason.
	 */
	emit_change_signals_for_all_files_in_all_directories ();
}

static void
real_set_metadata (NolphinFile  *file,
		   const char    *key,
		   const char    *value)
{
	/* Dummy default impl */
}

static void
real_set_metadata_as_list (NolphinFile *file,
			   const char   *key,
			   char         **value)
{
	/* Dummy default impl */
}

static void
nolphin_file_class_init (NolphinFileClass *class)
{
	GtkIconTheme *icon_theme;

	nolphin_file_info_getter = nolphin_file_get_internal;

	attribute_name_q = g_quark_from_static_string ("name");
	attribute_size_q = g_quark_from_static_string ("size");
	attribute_type_q = g_quark_from_static_string ("type");
    attribute_detailed_type_q = g_quark_from_static_string ("detailed_type");
	attribute_modification_date_q = g_quark_from_static_string ("modification_date");
	attribute_date_modified_q = g_quark_from_static_string ("date_modified");
	attribute_date_modified_full_q = g_quark_from_static_string ("date_modified_full");
	attribute_date_modified_with_time_q = g_quark_from_static_string ("date_modified_with_time");
	attribute_accessed_date_q = g_quark_from_static_string ("accessed_date");
	attribute_date_accessed_q = g_quark_from_static_string ("date_accessed");
	attribute_date_accessed_full_q = g_quark_from_static_string ("date_accessed_full");
    attribute_creation_date_q = g_quark_from_static_string ("creation_date");
    attribute_date_created_q = g_quark_from_static_string ("date_created");
    attribute_date_created_full_q = g_quark_from_static_string ("date_created_full");
    attribute_date_created_with_time_q = g_quark_from_static_string ("date_created_with_time");
	attribute_mime_type_q = g_quark_from_static_string ("mime_type");
	attribute_size_detail_q = g_quark_from_static_string ("size_detail");
	attribute_deep_size_q = g_quark_from_static_string ("deep_size");
	attribute_deep_file_count_q = g_quark_from_static_string ("deep_file_count");
	attribute_deep_directory_count_q = g_quark_from_static_string ("deep_directory_count");
	attribute_deep_total_count_q = g_quark_from_static_string ("deep_total_count");
	attribute_date_changed_q = g_quark_from_static_string ("date_changed");
	attribute_date_changed_full_q = g_quark_from_static_string ("date_changed_full");
	attribute_trashed_on_q = g_quark_from_static_string ("trashed_on");
	attribute_trashed_on_full_q = g_quark_from_static_string ("trashed_on_full");
	attribute_trash_orig_path_q = g_quark_from_static_string ("trash_orig_path");
	attribute_date_permissions_q = g_quark_from_static_string ("date_permissions");
	attribute_date_permissions_full_q = g_quark_from_static_string ("date_permissions_full");
	attribute_permissions_q = g_quark_from_static_string ("permissions");
	attribute_selinux_context_q = g_quark_from_static_string ("selinux_context");
	attribute_octal_permissions_q = g_quark_from_static_string ("octal_permissions");
	attribute_owner_q = g_quark_from_static_string ("owner");
	attribute_group_q = g_quark_from_static_string ("group");
	attribute_uri_q = g_quark_from_static_string ("uri");
	attribute_where_q = g_quark_from_static_string ("where");
	attribute_link_target_q = g_quark_from_static_string ("link_target");
	attribute_volume_q = g_quark_from_static_string ("volume");
	attribute_free_space_q = g_quark_from_static_string ("free_space");
    attribute_extension_q = g_quark_from_static_string ("extension");
    attribute_search_result_snippet_q = g_quark_from_string ("search_result_snippet");
    attribute_search_result_count_q = g_quark_from_string ("search_result_count");

	G_OBJECT_CLASS (class)->finalize = finalize;
	G_OBJECT_CLASS (class)->constructor = nolphin_file_constructor;

	class->set_metadata = real_set_metadata;
	class->set_metadata_as_list = real_set_metadata_as_list;
    class->get_metadata = NULL;
    class->get_metadata_as_list = NULL;

	signals[CHANGED] =
		g_signal_new ("changed",
		              G_TYPE_FROM_CLASS (class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (NolphinFileClass, changed),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE, 0);

	signals[UPDATED_DEEP_COUNT_IN_PROGRESS] =
		g_signal_new ("updated_deep_count_in_progress",
		              G_TYPE_FROM_CLASS (class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (NolphinFileClass, updated_deep_count_in_progress),
		              NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE, 0);

	g_type_class_add_private (class, sizeof (NolphinFileDetails));

	thumbnail_limit_changed_callback (NULL);
	g_signal_connect_swapped (nolphin_preferences,
				  "changed::" NOLPHIN_PREFERENCES_IMAGE_FILE_THUMBNAIL_LIMIT,
				  G_CALLBACK (thumbnail_limit_changed_callback),
				  NULL);
	thumbnail_size_changed_callback (NULL);
	g_signal_connect_swapped (nolphin_preferences,
				  "changed::" NOLPHIN_PREFERENCES_ICON_VIEW_THUMBNAIL_SIZE,
				  G_CALLBACK (thumbnail_size_changed_callback),
				  NULL);
	show_thumbnails_changed_callback (NULL);
	g_signal_connect_swapped (nolphin_preferences,
				  "changed::" NOLPHIN_PREFERENCES_SHOW_IMAGE_FILE_THUMBNAILS,
				  G_CALLBACK (show_thumbnails_changed_callback),
				  NULL);
    g_signal_connect_swapped (nolphin_preferences,
				  "changed::" NOLPHIN_PREFERENCES_INHERIT_SHOW_THUMBNAILS,
				  G_CALLBACK (show_thumbnails_changed_callback),
				  NULL);

	icon_theme = gtk_icon_theme_get_default ();
	g_signal_connect_object (icon_theme,
				 "changed",
				 G_CALLBACK (icon_theme_changed_callback),
				 NULL, 0);

	g_signal_connect (nolphin_signaller_get_current (),
			  "mime_data_changed",
			  G_CALLBACK (mime_type_data_changed_callback),
			  NULL);
}

static void
nolphin_file_add_emblem (NolphinFile *file,
			  const char *emblem_name)
{
	if (file->details->pending_info_providers) {
		file->details->pending_extension_emblems = g_list_prepend (file->details->pending_extension_emblems,
									   g_strdup (emblem_name));
	} else {
		file->details->extension_emblems = g_list_prepend (file->details->extension_emblems,
								   g_strdup (emblem_name));
	}

	nolphin_file_changed (file);
}

static void
nolphin_file_add_string_attribute (NolphinFile *file,
				    const char *attribute_name,
				    const char *value)
{
	if (file->details->pending_info_providers) {
		/* Lazily create hashtable */
		if (!file->details->pending_extension_attributes) {
			file->details->pending_extension_attributes =
				g_hash_table_new_full (g_direct_hash, g_direct_equal,
						       NULL,
						       (GDestroyNotify)g_free);
		}
		g_hash_table_insert (file->details->pending_extension_attributes,
				     GINT_TO_POINTER (g_quark_from_string (attribute_name)),
				     g_strdup (value));
	} else {
		if (!file->details->extension_attributes) {
			file->details->extension_attributes =
				g_hash_table_new_full (g_direct_hash, g_direct_equal,
						       NULL,
						       (GDestroyNotify)g_free);
		}
		g_hash_table_insert (file->details->extension_attributes,
				     GINT_TO_POINTER (g_quark_from_string (attribute_name)),
				     g_strdup (value));
	}

	nolphin_file_changed (file);
}

gboolean
nolphin_file_add_search_result_data (NolphinFile         *file,
                                  gpointer          search_dir,
                                  FileSearchResult *result)
{
    if (file->details->search_results == NULL) {
        file->details->search_results = g_hash_table_new_full (NULL, NULL,
                                                               NULL, (GDestroyNotify) file_search_result_free);
    }

    if (!g_hash_table_replace (file->details->search_results,
                               search_dir,
                               result)) {

#ifndef ENABLE_TRACKER // this is abnormal only in -advanced search.
        g_warning ("Search hits directory already existed - %s", nolphin_file_peek_name (file));
#endif
        return FALSE;
    }

    return  TRUE;
}

void
nolphin_file_clear_search_result_data (NolphinFile      *file,
                                    gpointer       search_dir)
{
    g_return_if_fail (file->details->search_results != NULL);

    if (!g_hash_table_remove (file->details->search_results,
                              search_dir)) {

        g_warning ("Attempting to remove search hits that don't exist - %s", nolphin_file_peek_name (file));
    }

    if (g_hash_table_size (file->details->search_results) == 0) {
        g_hash_table_destroy (file->details->search_results);
        file->details->search_results = NULL;
    }
}

static FileSearchResult*
get_file_search_result (NolphinFile *file, gpointer search_dir)
{
    if (file->details->search_results == NULL) {
        return NULL;
    }

    return g_hash_table_lookup (file->details->search_results, search_dir);
}

gboolean
nolphin_file_has_search_result (NolphinFile *file, gpointer search_dir)
{
    return g_hash_table_contains (file->details->search_results, search_dir);
}

gint
nolphin_file_get_search_result_count (NolphinFile *file, gpointer search_dir)
{
    FileSearchResult *result = get_file_search_result (file, search_dir);

    if (result != NULL) {
        return result->hits;
    }

    return 0;
}

gchar *
nolphin_file_get_search_result_count_as_string (NolphinFile *file, gpointer search_dir)
{
    gint count = nolphin_file_get_search_result_count (file, search_dir);

    if (count > 0) {
        return g_strdup_printf ("%d", count);
    }

    return NULL;
}

gchar *
nolphin_file_get_search_result_snippet (NolphinFile *file, gpointer search_dir)
{
    FileSearchResult *result = get_file_search_result (file, search_dir);

    if (result != NULL) {
        return g_strdup (result->snippet);
    }

    return NULL;
}

static void
nolphin_file_invalidate_extension_info (NolphinFile *file)
{
	nolphin_file_invalidate_attributes (file, NOLPHIN_FILE_ATTRIBUTE_EXTENSION_INFO);
}

void
nolphin_file_info_providers_done (NolphinFile *file)
{
	g_list_free_full (file->details->extension_emblems, g_free);
	file->details->extension_emblems = file->details->pending_extension_emblems;
	file->details->pending_extension_emblems = NULL;

	if (file->details->extension_attributes) {
		g_hash_table_destroy (file->details->extension_attributes);
	}

	file->details->extension_attributes = file->details->pending_extension_attributes;
	file->details->pending_extension_attributes = NULL;

	nolphin_file_changed (file);
}

static void
nolphin_file_info_iface_init (NolphinFileInfoInterface *iface)
{
	iface->is_gone = nolphin_file_is_gone;
	iface->get_name = nolphin_file_get_name;
	iface->get_file_type = nolphin_file_get_file_type;
	iface->get_location = nolphin_file_get_location;
	iface->get_uri = nolphin_file_get_uri;
	iface->get_parent_location = nolphin_file_get_parent_location;
	iface->get_parent_uri = nolphin_file_get_parent_uri;
	iface->get_parent_info = nolphin_file_get_parent;
	iface->get_mount = nolphin_file_get_mount;
	iface->get_uri_scheme = nolphin_file_get_uri_scheme;
	iface->get_activation_uri = nolphin_file_get_activation_uri;
	iface->get_mime_type = nolphin_file_get_mime_type;
	iface->is_mime_type = nolphin_file_is_mime_type;
	iface->is_directory = nolphin_file_is_directory;
	iface->can_write = nolphin_file_can_write;
	iface->add_emblem = nolphin_file_add_emblem;
	iface->get_string_attribute = nolphin_file_get_string_attribute;
	iface->add_string_attribute = nolphin_file_add_string_attribute;
	iface->invalidate_extension_info = nolphin_file_invalidate_extension_info;
}

#if !defined (NOLPHIN_OMIT_SELF_CHECK)

void
nolphin_self_check_file (void)
{
	NolphinFile *file_1;
	NolphinFile *file_2;
	GList *list;

        /* refcount checks */

        EEL_CHECK_INTEGER_RESULT (nolphin_directory_number_outstanding (), 0);

	file_1 = nolphin_file_get_by_uri ("file:///home/");

	EEL_CHECK_INTEGER_RESULT (G_OBJECT (file_1)->ref_count, 1);
	EEL_CHECK_INTEGER_RESULT (G_OBJECT (file_1->details->directory)->ref_count, 1);
        EEL_CHECK_INTEGER_RESULT (nolphin_directory_number_outstanding (), 1);

	nolphin_file_unref (file_1);

        EEL_CHECK_INTEGER_RESULT (nolphin_directory_number_outstanding (), 0);

	file_1 = nolphin_file_get_by_uri ("file:///etc");
	file_2 = nolphin_file_get_by_uri ("file:///usr");

        list = NULL;
        list = g_list_prepend (list, file_1);
        list = g_list_prepend (list, file_2);

        nolphin_file_list_ref (list);

	EEL_CHECK_INTEGER_RESULT (G_OBJECT (file_1)->ref_count, 2);
	EEL_CHECK_INTEGER_RESULT (G_OBJECT (file_2)->ref_count, 2);

	nolphin_file_list_unref (list);

	EEL_CHECK_INTEGER_RESULT (G_OBJECT (file_1)->ref_count, 1);
	EEL_CHECK_INTEGER_RESULT (G_OBJECT (file_2)->ref_count, 1);

	nolphin_file_list_free (list);

        EEL_CHECK_INTEGER_RESULT (nolphin_directory_number_outstanding (), 0);


        /* name checks */
	file_1 = nolphin_file_get_by_uri ("file:///home/");

	EEL_CHECK_STRING_RESULT (nolphin_file_get_name (file_1), "home");

	EEL_CHECK_BOOLEAN_RESULT (nolphin_file_get_by_uri ("file:///home/") == file_1, TRUE);
	nolphin_file_unref (file_1);

	EEL_CHECK_BOOLEAN_RESULT (nolphin_file_get_by_uri ("file:///home") == file_1, TRUE);
	nolphin_file_unref (file_1);

	nolphin_file_unref (file_1);

	file_1 = nolphin_file_get_by_uri ("file:///home");
	EEL_CHECK_STRING_RESULT (nolphin_file_get_name (file_1), "home");
	nolphin_file_unref (file_1);

#if 0
	/* ALEX: I removed this, because it was breaking distchecks.
	 * It used to work, but when canonical uris changed from
	 * foo: to foo:/// it broke. I don't expect it to matter
	 * in real life */
	file_1 = nolphin_file_get_by_uri (":");
	EEL_CHECK_STRING_RESULT (nolphin_file_get_name (file_1), ":");
	nolphin_file_unref (file_1);
#endif

	file_1 = nolphin_file_get_by_uri ("eazel:");
	EEL_CHECK_STRING_RESULT (nolphin_file_get_name (file_1), "eazel");
	nolphin_file_unref (file_1);

	/* sorting */
	file_1 = nolphin_file_get_by_uri ("file:///etc");
	file_2 = nolphin_file_get_by_uri ("file:///usr");

	EEL_CHECK_INTEGER_RESULT (G_OBJECT (file_1)->ref_count, 1);
	EEL_CHECK_INTEGER_RESULT (G_OBJECT (file_2)->ref_count, 1);

	EEL_CHECK_BOOLEAN_RESULT (nolphin_file_compare_for_sort (file_1, file_2, NOLPHIN_FILE_SORT_BY_DISPLAY_NAME, FALSE, FALSE, FALSE, NULL) < 0, TRUE);
	EEL_CHECK_BOOLEAN_RESULT (nolphin_file_compare_for_sort (file_1, file_2, NOLPHIN_FILE_SORT_BY_DISPLAY_NAME, FALSE, FALSE, TRUE, NULL) > 0, TRUE);
	EEL_CHECK_BOOLEAN_RESULT (nolphin_file_compare_for_sort (file_1, file_1, NOLPHIN_FILE_SORT_BY_DISPLAY_NAME, FALSE, FALSE, FALSE, NULL) == 0, TRUE);
	EEL_CHECK_BOOLEAN_RESULT (nolphin_file_compare_for_sort (file_1, file_1, NOLPHIN_FILE_SORT_BY_DISPLAY_NAME, TRUE, FALSE, FALSE, NULL) == 0, TRUE);
	EEL_CHECK_BOOLEAN_RESULT (nolphin_file_compare_for_sort (file_1, file_1, NOLPHIN_FILE_SORT_BY_DISPLAY_NAME, FALSE, FALSE, TRUE, NULL) == 0, TRUE);
	EEL_CHECK_BOOLEAN_RESULT (nolphin_file_compare_for_sort (file_1, file_1, NOLPHIN_FILE_SORT_BY_DISPLAY_NAME, TRUE, FALSE, TRUE, NULL) == 0, TRUE);
    
    

	nolphin_file_unref (file_1);
	nolphin_file_unref (file_2);
}

#endif /* !NOLPHIN_OMIT_SELF_CHECK */
