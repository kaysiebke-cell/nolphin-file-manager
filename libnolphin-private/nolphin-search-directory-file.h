/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nolphin-search-directory-file.h: Subclass of NolphinFile to implement the
   the case of the search directory
 
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

#ifndef NOLPHIN_SEARCH_DIRECTORY_FILE_H
#define NOLPHIN_SEARCH_DIRECTORY_FILE_H

#include <libnolphin-private/nolphin-file.h>

#define NOLPHIN_TYPE_SEARCH_DIRECTORY_FILE nolphin_search_directory_file_get_type()
#define NOLPHIN_SEARCH_DIRECTORY_FILE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NOLPHIN_TYPE_SEARCH_DIRECTORY_FILE, NolphinSearchDirectoryFile))
#define NOLPHIN_SEARCH_DIRECTORY_FILE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NOLPHIN_TYPE_SEARCH_DIRECTORY_FILE, NolphinSearchDirectoryFileClass))
#define NOLPHIN_IS_SEARCH_DIRECTORY_FILE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NOLPHIN_TYPE_SEARCH_DIRECTORY_FILE))
#define NOLPHIN_IS_SEARCH_DIRECTORY_FILE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NOLPHIN_TYPE_SEARCH_DIRECTORY_FILE))
#define NOLPHIN_SEARCH_DIRECTORY_FILE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NOLPHIN_TYPE_SEARCH_DIRECTORY_FILE, NolphinSearchDirectoryFileClass))

typedef struct NolphinSearchDirectoryFileDetails NolphinSearchDirectoryFileDetails;

typedef struct {
	NolphinFile parent_slot;
	NolphinSearchDirectoryFileDetails *details;
} NolphinSearchDirectoryFile;

typedef struct {
	NolphinFileClass parent_slot;
} NolphinSearchDirectoryFileClass;

GType   nolphin_search_directory_file_get_type (void);
void    nolphin_search_directory_file_update_display_name (NolphinSearchDirectoryFile *search_file);

#endif /* NOLPHIN_SEARCH_DIRECTORY_FILE_H */
