/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nolphin-vfs-file.h: Subclass of NolphinFile to implement the
   the case of a VFS file.
 
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

#ifndef NOLPHIN_VFS_FILE_H
#define NOLPHIN_VFS_FILE_H

#include <libnolphin-private/nolphin-file.h>

#define NOLPHIN_TYPE_VFS_FILE nolphin_vfs_file_get_type()
#define NOLPHIN_VFS_FILE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NOLPHIN_TYPE_VFS_FILE, NolphinVFSFile))
#define NOLPHIN_VFS_FILE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NOLPHIN_TYPE_VFS_FILE, NolphinVFSFileClass))
#define NOLPHIN_IS_VFS_FILE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NOLPHIN_TYPE_VFS_FILE))
#define NOLPHIN_IS_VFS_FILE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NOLPHIN_TYPE_VFS_FILE))
#define NOLPHIN_VFS_FILE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NOLPHIN_TYPE_VFS_FILE, NolphinVFSFileClass))

typedef struct NolphinVFSFileDetails NolphinVFSFileDetails;

typedef struct {
	NolphinFile parent_slot;
} NolphinVFSFile;

typedef struct {
	NolphinFileClass parent_slot;
} NolphinVFSFileClass;

GType   nolphin_vfs_file_get_type (void);

#endif /* NOLPHIN_VFS_FILE_H */
