/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nolphin-desktop-icon-file.c: Subclass of NolphinFile to help implement the
   virtual desktop icons.
 
   Copyright (C) 2003 Red Hat, Inc.
  
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
  
   Author: Alexander Larsson <alexl@redhat.com>
*/

#include <config.h>
#include "nolphin-desktop-icon-file.h"

#include "nolphin-desktop-metadata.h"
#include "nolphin-desktop-directory-file.h"
#include "nolphin-directory-notify.h"
#include "nolphin-directory-private.h"
#include "nolphin-file-attributes.h"
#include "nolphin-file-private.h"
#include "nolphin-file-utilities.h"
#include "nolphin-file-operations.h"
#include <eel/eel-glib-extensions.h>
#include "nolphin-desktop-directory.h"
#include <glib/gi18n.h>
#include <string.h>
#include <gio/gio.h>

struct NolphinDesktopIconFileDetails {
	NolphinDesktopLink *link;
};

G_DEFINE_TYPE(NolphinDesktopIconFile, nolphin_desktop_icon_file, NOLPHIN_TYPE_FILE)


static void
desktop_icon_file_monitor_add (NolphinFile *file,
			       gconstpointer client,
			       NolphinFileAttributes attributes)
{
	nolphin_directory_monitor_add_internal
		(file->details->directory, file,
		 client, TRUE, attributes, NULL, NULL);
}

static void
desktop_icon_file_monitor_remove (NolphinFile *file,
				  gconstpointer client)
{
	nolphin_directory_monitor_remove_internal
		(file->details->directory, file, client);
}

static void
desktop_icon_file_call_when_ready (NolphinFile *file,
				   NolphinFileAttributes attributes,
				   NolphinFileCallback callback,
				   gpointer callback_data)
{
	nolphin_directory_call_when_ready_internal
		(file->details->directory, file,
		 attributes, FALSE, NULL, callback, callback_data);
}

static void
desktop_icon_file_cancel_call_when_ready (NolphinFile *file,
					  NolphinFileCallback callback,
					  gpointer callback_data)
{
	nolphin_directory_cancel_callback_internal
		(file->details->directory, file,
		 NULL, callback, callback_data);
}

static gboolean
desktop_icon_file_check_if_ready (NolphinFile *file,
				  NolphinFileAttributes attributes)
{
	return nolphin_directory_check_if_ready_internal
		(file->details->directory, file,
		 attributes);
}

static gboolean
desktop_icon_file_get_item_count (NolphinFile *file, 
				  guint *count,
				  gboolean *count_unreadable)
{
	if (count != NULL) {
		*count = 0;
	}
	if (count_unreadable != NULL) {
		*count_unreadable = FALSE;
	}
	return TRUE;
}

static NolphinRequestStatus
desktop_icon_file_get_deep_counts (NolphinFile *file,
				   guint *directory_count,
				   guint *file_count,
				   guint *unreadable_directory_count,
                   guint *hidden_count,
				   goffset *total_size)
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
	return NOLPHIN_REQUEST_DONE;
}

static gboolean
desktop_icon_file_get_date (NolphinFile *file,
			    NolphinDateType date_type,
			    time_t *date)
{
	NolphinDesktopIconFile *desktop_file;

	desktop_file = NOLPHIN_DESKTOP_ICON_FILE (file);

	return nolphin_desktop_link_get_date (desktop_file->details->link,
					       date_type, date);
}

static char *
desktop_icon_file_get_where_string (NolphinFile *file)
{
	return g_strdup (_("auf dem Schreibtisch"));
}

static void
nolphin_desktop_icon_file_init (NolphinDesktopIconFile *desktop_file)
{
	desktop_file->details =	G_TYPE_INSTANCE_GET_PRIVATE (desktop_file,
							     NOLPHIN_TYPE_DESKTOP_ICON_FILE,
							     NolphinDesktopIconFileDetails);
}

static void
update_info_from_link (NolphinDesktopIconFile *icon_file)
{
	NolphinFile *file;
	NolphinDesktopLink *link;
	char *display_name;
	GMount *mount;
	
	file = NOLPHIN_FILE (icon_file);
	
	link = icon_file->details->link;

	if (link == NULL) {
		return;
	}

	g_clear_pointer (&file->details->mime_type, g_ref_string_release);
	file->details->mime_type = g_ref_string_new_intern ("application/x-nolphin-link");
	file->details->type = G_FILE_TYPE_SHORTCUT;
	file->details->size = 0;
	file->details->has_permissions = FALSE;
	file->details->can_read = TRUE;
	file->details->can_write = TRUE;

	file->details->can_mount = FALSE;
	file->details->can_unmount = FALSE;
	file->details->can_eject = FALSE;
	if (file->details->mount) {
		g_object_unref (file->details->mount);
	}
	mount = nolphin_desktop_link_get_mount (link);
	file->details->mount = mount;
	if (mount) {
		file->details->can_unmount = g_mount_can_unmount (mount);
		file->details->can_eject = g_mount_can_eject (mount);
	}
	
	file->details->file_info_is_up_to_date = TRUE;

	display_name = nolphin_desktop_link_get_display_name (link);
	nolphin_file_set_display_name (file,
					display_name, NULL, TRUE);
	g_free (display_name);

	if (file->details->icon != NULL) {
		g_object_unref (file->details->icon);
	}
	file->details->icon = nolphin_desktop_link_get_icon (link);
	g_free (file->details->activation_uri);
	file->details->activation_uri = nolphin_desktop_link_get_activation_uri (link);
	file->details->got_link_info = TRUE;
	file->details->link_info_is_up_to_date = TRUE;

	file->details->directory_count = 0;
	file->details->got_directory_count = TRUE;
	file->details->directory_count_is_up_to_date = TRUE;
}

void
nolphin_desktop_icon_file_update (NolphinDesktopIconFile *icon_file)
{
	NolphinFile *file;
	
	update_info_from_link (icon_file);
	file = NOLPHIN_FILE (icon_file);
	nolphin_file_changed (file);
}

void
nolphin_desktop_icon_file_remove (NolphinDesktopIconFile *icon_file)
{
	NolphinFile *file;
	GList list;

	icon_file->details->link = NULL;

	file = NOLPHIN_FILE (icon_file);
	
	/* ref here because we might be removing the last ref when we
	 * mark the file gone below, but we need to keep a ref at
	 * least long enough to send the change notification. 
	 */
	nolphin_file_ref (file);
	
	file->details->is_gone = TRUE;
	
	list.data = file;
	list.next = NULL;
	list.prev = NULL;
	
	nolphin_directory_remove_file (file->details->directory, file);
	nolphin_directory_emit_change_signals (file->details->directory, &list);
	
	nolphin_file_unref (file);
}

NolphinDesktopIconFile *
nolphin_desktop_icon_file_new (NolphinDesktopLink *link)
{
	NolphinFile *file;
	NolphinDirectory *directory;
	NolphinDesktopIconFile *icon_file;
	GList list;
	char *name;

	directory = nolphin_directory_get_by_uri (EEL_DESKTOP_URI);

	file = NOLPHIN_FILE (g_object_new (NOLPHIN_TYPE_DESKTOP_ICON_FILE, NULL));

#ifdef NOLPHIN_FILE_DEBUG_REF
	printf("%10p ref'd\n", file);
	eazel_dump_stack_trace ("\t", 10);
#endif

	file->details->directory = directory;

	icon_file = NOLPHIN_DESKTOP_ICON_FILE (file);
	icon_file->details->link = link;

	name = nolphin_desktop_link_get_file_name (link);
	file->details->name = g_ref_string_new (name);
	g_free (name);

	update_info_from_link (icon_file);

	nolphin_desktop_update_metadata_from_keyfile (file, file->details->name);

	nolphin_directory_add_file (directory, file);

	list.data = file;
	list.next = NULL;
	list.prev = NULL;
	nolphin_directory_emit_files_added (directory, &list);

	return icon_file;
}

/* Note: This can return NULL if the link was recently removed (i.e. unmounted) */
NolphinDesktopLink *
nolphin_desktop_icon_file_get_link (NolphinDesktopIconFile *icon_file)
{
	if (icon_file->details->link)
		return g_object_ref (icon_file->details->link);
	else
		return NULL;
}

static void
nolphin_desktop_icon_file_unmount (NolphinFile                   *file,
				    GMountOperation                *mount_op,
				    GCancellable                   *cancellable,
				    NolphinFileOperationCallback   callback,
				    gpointer                        callback_data)
{
	NolphinDesktopIconFile *desktop_file;
	GMount *mount;
	
	desktop_file = NOLPHIN_DESKTOP_ICON_FILE (file);
	if (desktop_file) {
		mount = nolphin_desktop_link_get_mount (desktop_file->details->link);
		if (mount != NULL) {
			nolphin_file_operations_unmount_mount (NULL, mount, FALSE, TRUE);
		}
	}
	
}

static void
nolphin_desktop_icon_file_eject (NolphinFile                   *file,
				  GMountOperation                *mount_op,
				  GCancellable                   *cancellable,
				  NolphinFileOperationCallback   callback,
				  gpointer                        callback_data)
{
	NolphinDesktopIconFile *desktop_file;
	GMount *mount;
	
	desktop_file = NOLPHIN_DESKTOP_ICON_FILE (file);
	if (desktop_file) {
		mount = nolphin_desktop_link_get_mount (desktop_file->details->link);
		if (mount != NULL) {
			nolphin_file_operations_unmount_mount (NULL, mount, TRUE, TRUE);
		}
	}
}

static void
nolphin_desktop_icon_file_set_metadata (NolphinFile           *file,
					 const char             *key,
					 const char             *value)
{
	nolphin_desktop_set_metadata_string (file, file->details->name, key, value);
}

static void
nolphin_desktop_icon_file_set_metadata_as_list (NolphinFile           *file,
						 const char             *key,
						 char                  **value)
{
	nolphin_desktop_set_metadata_stringv (file, file->details->name, key, (const gchar **) value);
}

static void
nolphin_desktop_icon_file_class_init (NolphinDesktopIconFileClass *klass)
{
	GObjectClass *object_class;
	NolphinFileClass *file_class;

	object_class = G_OBJECT_CLASS (klass);
	file_class = NOLPHIN_FILE_CLASS (klass);

	file_class->default_file_type = G_FILE_TYPE_DIRECTORY;
	
	file_class->monitor_add = desktop_icon_file_monitor_add;
	file_class->monitor_remove = desktop_icon_file_monitor_remove;
	file_class->call_when_ready = desktop_icon_file_call_when_ready;
	file_class->cancel_call_when_ready = desktop_icon_file_cancel_call_when_ready;
	file_class->check_if_ready = desktop_icon_file_check_if_ready;
	file_class->get_item_count = desktop_icon_file_get_item_count;
	file_class->get_deep_counts = desktop_icon_file_get_deep_counts;
	file_class->get_date = desktop_icon_file_get_date;
	file_class->get_where_string = desktop_icon_file_get_where_string;
	file_class->set_metadata = nolphin_desktop_icon_file_set_metadata;
	file_class->set_metadata_as_list = nolphin_desktop_icon_file_set_metadata_as_list;
	file_class->unmount = nolphin_desktop_icon_file_unmount;
	file_class->eject = nolphin_desktop_icon_file_eject;

	g_type_class_add_private (object_class, sizeof(NolphinDesktopIconFileDetails));
}
