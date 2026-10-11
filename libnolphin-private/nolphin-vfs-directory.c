/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nolphin-vfs-directory.c: Subclass of NolphinDirectory to help implement the
   virtual trash directory.
 
   Copyright (C) 1999, 2000 Eazel, Inc.
  
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
#include "nolphin-vfs-directory.h"

#include "nolphin-directory-private.h"
#include "nolphin-file-private.h"

G_DEFINE_TYPE (NolphinVFSDirectory, nolphin_vfs_directory, NOLPHIN_TYPE_DIRECTORY);

static void
nolphin_vfs_directory_init (NolphinVFSDirectory *directory)
{

}

static gboolean
vfs_contains_file (NolphinDirectory *directory,
		   NolphinFile *file)
{
	g_assert (NOLPHIN_IS_VFS_DIRECTORY (directory));
	g_assert (NOLPHIN_IS_FILE (file));

	return file->details->directory == directory;
}

static void
vfs_call_when_ready (NolphinDirectory *directory,
		     NolphinFileAttributes file_attributes,
		     gboolean wait_for_file_list,
		     NolphinDirectoryCallback callback,
		     gpointer callback_data)
{
	g_assert (NOLPHIN_IS_VFS_DIRECTORY (directory));

	nolphin_directory_call_when_ready_internal
		(directory,
		 NULL,
		 file_attributes,
		 wait_for_file_list,
		 callback,
		 NULL,
		 callback_data);
}

static void
vfs_cancel_callback (NolphinDirectory *directory,
		     NolphinDirectoryCallback callback,
		     gpointer callback_data)
{
	g_assert (NOLPHIN_IS_VFS_DIRECTORY (directory));

	nolphin_directory_cancel_callback_internal
		(directory,
		 NULL,
		 callback,
		 NULL,
		 callback_data);
}

static void
vfs_file_monitor_add (NolphinDirectory *directory,
		      gconstpointer client,
		      gboolean monitor_hidden_files,
		      NolphinFileAttributes file_attributes,
		      NolphinDirectoryCallback callback,
		      gpointer callback_data)
{
	g_assert (NOLPHIN_IS_VFS_DIRECTORY (directory));
	g_assert (client != NULL);

	nolphin_directory_monitor_add_internal
		(directory, NULL,
		 client,
		 monitor_hidden_files,
		 file_attributes,
		 callback, callback_data);
}

static void
vfs_file_monitor_remove (NolphinDirectory *directory,
			 gconstpointer client)
{
	g_assert (NOLPHIN_IS_VFS_DIRECTORY (directory));
	g_assert (client != NULL);
	
	nolphin_directory_monitor_remove_internal (directory, NULL, client);
}

static void
vfs_force_reload (NolphinDirectory *directory)
{
	NolphinFileAttributes all_attributes;

	g_assert (NOLPHIN_IS_DIRECTORY (directory));

	all_attributes = nolphin_file_get_all_attributes ();
	nolphin_directory_force_reload_internal (directory,
						  all_attributes);
}

static gboolean
vfs_are_all_files_seen (NolphinDirectory *directory)
{
	g_assert (NOLPHIN_IS_VFS_DIRECTORY (directory));
	
	return directory->details->directory_loaded;
}

static gboolean
vfs_is_not_empty (NolphinDirectory *directory)
{
	g_assert (NOLPHIN_IS_VFS_DIRECTORY (directory));
	g_assert (nolphin_directory_is_anyone_monitoring_file_list (directory));

	return directory->details->file_list != NULL;
}

static void
nolphin_vfs_directory_class_init (NolphinVFSDirectoryClass *klass)
{
	NolphinDirectoryClass *directory_class = NOLPHIN_DIRECTORY_CLASS (klass);

	directory_class->contains_file = vfs_contains_file;
	directory_class->call_when_ready = vfs_call_when_ready;
	directory_class->cancel_callback = vfs_cancel_callback;
	directory_class->file_monitor_add = vfs_file_monitor_add;
	directory_class->file_monitor_remove = vfs_file_monitor_remove;
	directory_class->force_reload = vfs_force_reload;
	directory_class->are_all_files_seen = vfs_are_all_files_seen;
	directory_class->is_not_empty = vfs_is_not_empty;
}
