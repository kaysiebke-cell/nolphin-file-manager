/*
 *  nolphin-file-info.h - Information about a file 
 *
 *  Copyright (C) 2003 Novell, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 51 Franklin Street, Suite 500, MA 02110-1335, USA.
 *
 */

/* NolphinFileInfo is an interface to the NolphinFile object.  It 
 * provides access to the asynchronous data in the NolphinFile.
 * Extensions are passed objects of this type for operations. */

#ifndef NOLPHIN_FILE_INFO_H
#define NOLPHIN_FILE_INFO_H

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define NOLPHIN_TYPE_FILE_INFO           (nolphin_file_info_get_type ())
#define NOLPHIN_FILE_INFO(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), NOLPHIN_TYPE_FILE_INFO, NolphinFileInfo))
#define NOLPHIN_IS_FILE_INFO(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NOLPHIN_TYPE_FILE_INFO))
#define NOLPHIN_FILE_INFO_GET_IFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE ((obj), NOLPHIN_TYPE_FILE_INFO, NolphinFileInfoInterface))

#ifndef NOLPHIN_FILE_DEFINED
#define NOLPHIN_FILE_DEFINED
/* Using NolphinFile for the vtable to make implementing this in 
 * NolphinFile easier */
typedef struct NolphinFile          NolphinFile;
#endif

typedef NolphinFile NolphinFileInfo;
typedef struct _NolphinFileInfoInterface NolphinFileInfoInterface;

struct _NolphinFileInfoInterface 
{
	GTypeInterface g_iface;

	gboolean          (*is_gone)              (NolphinFileInfo *file);
	
	char *            (*get_name)             (NolphinFileInfo *file);
	char *            (*get_uri)              (NolphinFileInfo *file);
	char *            (*get_parent_uri)       (NolphinFileInfo *file);
	char *            (*get_uri_scheme)       (NolphinFileInfo *file);
	
	char *            (*get_mime_type)        (NolphinFileInfo *file);
	gboolean          (*is_mime_type)         (NolphinFileInfo *file,
						   const char       *mime_Type);
	gboolean          (*is_directory)         (NolphinFileInfo *file);
	
	void              (*add_emblem)           (NolphinFileInfo *file,
						   const char       *emblem_name);
	char *            (*get_string_attribute) (NolphinFileInfo *file,
						   const char       *attribute_name);
	void              (*add_string_attribute) (NolphinFileInfo *file,
						   const char       *attribute_name,
						   const char       *value);
	void              (*invalidate_extension_info) (NolphinFileInfo *file);
	
	char *            (*get_activation_uri)   (NolphinFileInfo *file);

	GFileType         (*get_file_type)        (NolphinFileInfo *file);
	GFile *           (*get_location)         (NolphinFileInfo *file);
	GFile *           (*get_parent_location)  (NolphinFileInfo *file);
	NolphinFileInfo* (*get_parent_info)      (NolphinFileInfo *file);
	GMount *          (*get_mount)            (NolphinFileInfo *file);
	gboolean          (*can_write)            (NolphinFileInfo *file);
  
};

GList            *nolphin_file_info_list_copy            (GList            *files);
void              nolphin_file_info_list_free            (GList            *files);
GType             nolphin_file_info_get_type             (void);

/* Return true if the file has been deleted */
gboolean          nolphin_file_info_is_gone              (NolphinFileInfo *file);

/* Name and Location */
GFileType         nolphin_file_info_get_file_type        (NolphinFileInfo *file);
GFile *           nolphin_file_info_get_location         (NolphinFileInfo *file);
char *            nolphin_file_info_get_name             (NolphinFileInfo *file);
char *            nolphin_file_info_get_uri              (NolphinFileInfo *file);
char *            nolphin_file_info_get_activation_uri   (NolphinFileInfo *file);
GFile *           nolphin_file_info_get_parent_location  (NolphinFileInfo *file);
char *            nolphin_file_info_get_parent_uri       (NolphinFileInfo *file);
GMount *          nolphin_file_info_get_mount            (NolphinFileInfo *file);
char *            nolphin_file_info_get_uri_scheme       (NolphinFileInfo *file);
/* It's not safe to call this recursively multiple times, as it works
 * only for files already cached by Nolphin.
 */
NolphinFileInfo* nolphin_file_info_get_parent_info      (NolphinFileInfo *file);

/* File Type */
char *            nolphin_file_info_get_mime_type        (NolphinFileInfo *file);
gboolean          nolphin_file_info_is_mime_type         (NolphinFileInfo *file,
							   const char       *mime_type);
gboolean          nolphin_file_info_is_directory         (NolphinFileInfo *file);
gboolean          nolphin_file_info_can_write            (NolphinFileInfo *file);


/* Modifying the NolphinFileInfo */
void              nolphin_file_info_add_emblem           (NolphinFileInfo *file,
							   const char       *emblem_name);
char *            nolphin_file_info_get_string_attribute (NolphinFileInfo *file,
							   const char       *attribute_name);
void              nolphin_file_info_add_string_attribute (NolphinFileInfo *file,
							   const char       *attribute_name,
							   const char       *value);

/* Invalidating file info */
void              nolphin_file_info_invalidate_extension_info (NolphinFileInfo *file);

NolphinFileInfo *nolphin_file_info_lookup                (GFile *location);
NolphinFileInfo *nolphin_file_info_create                (GFile *location);
NolphinFileInfo *nolphin_file_info_lookup_for_uri        (const char *uri);
NolphinFileInfo *nolphin_file_info_create_for_uri        (const char *uri);

G_END_DECLS

#endif
