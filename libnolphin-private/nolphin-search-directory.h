/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nolphin-search-directory.h: Subclass of NolphinDirectory to implement
   a virtual directory consisting of the search directory and the search
   icons
 
   Copyright (C) 2005 Novell, Inc
  
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
*/

#ifndef NOLPHIN_SEARCH_DIRECTORY_H
#define NOLPHIN_SEARCH_DIRECTORY_H

#include <libnolphin-private/nolphin-directory.h>
#include <libnolphin-private/nolphin-query.h>

#define NOLPHIN_TYPE_SEARCH_DIRECTORY nolphin_search_directory_get_type()
#define NOLPHIN_SEARCH_DIRECTORY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NOLPHIN_TYPE_SEARCH_DIRECTORY, NolphinSearchDirectory))
#define NOLPHIN_SEARCH_DIRECTORY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NOLPHIN_TYPE_SEARCH_DIRECTORY, NolphinSearchDirectoryClass))
#define NOLPHIN_IS_SEARCH_DIRECTORY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NOLPHIN_TYPE_SEARCH_DIRECTORY))
#define NOLPHIN_IS_SEARCH_DIRECTORY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NOLPHIN_TYPE_SEARCH_DIRECTORY))
#define NOLPHIN_SEARCH_DIRECTORY_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NOLPHIN_TYPE_SEARCH_DIRECTORY, NolphinSearchDirectoryClass))

typedef struct NolphinSearchDirectoryDetails NolphinSearchDirectoryDetails;

typedef struct {
	NolphinDirectory parent_slot;
	NolphinSearchDirectoryDetails *details;
} NolphinSearchDirectory;

typedef struct {
	NolphinDirectoryClass parent_slot;
} NolphinSearchDirectoryClass;

GType   nolphin_search_directory_get_type             (void);

char   *nolphin_search_directory_generate_new_uri     (void);

NolphinSearchDirectory *nolphin_search_directory_new_from_saved_search (const char *uri);

gboolean       nolphin_search_directory_is_saved_search (NolphinSearchDirectory *search);
gboolean       nolphin_search_directory_is_modified     (NolphinSearchDirectory *search);
void           nolphin_search_directory_save_search     (NolphinSearchDirectory *search);
void           nolphin_search_directory_save_to_file    (NolphinSearchDirectory *search,
							  const char              *save_file_uri);

NolphinQuery *nolphin_search_directory_get_query       (NolphinSearchDirectory *search);
void           nolphin_search_directory_set_query       (NolphinSearchDirectory *search,
							  NolphinQuery           *query);

#endif /* NOLPHIN_SEARCH_DIRECTORY_H */
