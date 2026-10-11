/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nolphin-vfs-directory.h: Subclass of NolphinDirectory to implement the
   the case of a VFS directory.
 
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

#ifndef NOLPHIN_VFS_DIRECTORY_H
#define NOLPHIN_VFS_DIRECTORY_H

#include <libnolphin-private/nolphin-directory.h>

#define NOLPHIN_TYPE_VFS_DIRECTORY nolphin_vfs_directory_get_type()
#define NOLPHIN_VFS_DIRECTORY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NOLPHIN_TYPE_VFS_DIRECTORY, NolphinVFSDirectory))
#define NOLPHIN_VFS_DIRECTORY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NOLPHIN_TYPE_VFS_DIRECTORY, NolphinVFSDirectoryClass))
#define NOLPHIN_IS_VFS_DIRECTORY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NOLPHIN_TYPE_VFS_DIRECTORY))
#define NOLPHIN_IS_VFS_DIRECTORY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NOLPHIN_TYPE_VFS_DIRECTORY))
#define NOLPHIN_VFS_DIRECTORY_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NOLPHIN_TYPE_VFS_DIRECTORY, NolphinVFSDirectoryClass))

typedef struct NolphinVFSDirectoryDetails NolphinVFSDirectoryDetails;

typedef struct {
	NolphinDirectory parent_slot;
} NolphinVFSDirectory;

typedef struct {
	NolphinDirectoryClass parent_slot;
} NolphinVFSDirectoryClass;

GType   nolphin_vfs_directory_get_type (void);

#endif /* NOLPHIN_VFS_DIRECTORY_H */
