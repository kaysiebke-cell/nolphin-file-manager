/*
 *  nolphin-file-info.c - Information about a file 
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

#include <config.h>
#include "nolphin-file-info.h"
#include "nolphin-extension-private.h"

G_DEFINE_INTERFACE (NolphinFileInfo, nolphin_file_info, G_TYPE_OBJECT)

NolphinFileInfo *(*nolphin_file_info_getter) (GFile *location, gboolean create);

/**
 * SECTION:nolphin-file-info
 * @Title: NolphinFileInfo
 * @Short_description: A wrapper interface for extensions to access NolphinFile info.
 *
 * This inteface is implemented by NolphinFile and provides access to certain information
 * regarding a given file object.  It is also used to add file attributes and notify
 * a file of changes to those attribues when using a #NolphinInfoProvider.
 **/

static void
nolphin_file_info_default_init (NolphinFileInfoInterface *klass)
{
}

/**
 * nolphin_file_info_list_copy:
 * @files: (element-type NolphinFileInfo): the files to copy
 *
 * Returns: (element-type NolphinFileInfo) (transfer full): a copy of @files.
 *  Use #nolphin_file_info_list_free to free the list and unref its contents.
 */
GList *
nolphin_file_info_list_copy (GList *files)
{
    GList *ret;
    GList *l;
    
    ret = g_list_copy (files);
    for (l = ret; l != NULL; l = l->next) {
        g_object_ref (G_OBJECT (l->data));
    }

    return ret;
}

/**
 * nolphin_file_info_list_free:
 * @files: (element-type NolphinFileInfo): a list created with
 *   #nolphin_file_info_list_copy
 *
 */
void              
nolphin_file_info_list_free (GList *files)
{
    GList *l;
    
    for (l = files; l != NULL; l = l->next) {
        g_object_unref (G_OBJECT (l->data));
    }
    
    g_list_free (files);
}

gboolean
nolphin_file_info_is_gone (NolphinFileInfo *file)
{
	g_return_val_if_fail (NOLPHIN_IS_FILE_INFO (file), FALSE);
	g_return_val_if_fail (NOLPHIN_FILE_INFO_GET_IFACE (file)->is_gone != NULL, FALSE);
	
	return NOLPHIN_FILE_INFO_GET_IFACE (file)->is_gone (file);
}

GFileType
nolphin_file_info_get_file_type (NolphinFileInfo *file)
{
	g_return_val_if_fail (NOLPHIN_IS_FILE_INFO (file), G_FILE_TYPE_UNKNOWN);
	g_return_val_if_fail (NOLPHIN_FILE_INFO_GET_IFACE (file)->get_file_type != NULL, G_FILE_TYPE_UNKNOWN);

	return NOLPHIN_FILE_INFO_GET_IFACE (file)->get_file_type (file);
}

char *
nolphin_file_info_get_name (NolphinFileInfo *file)
{
	g_return_val_if_fail (NOLPHIN_IS_FILE_INFO (file), NULL);
	g_return_val_if_fail (NOLPHIN_FILE_INFO_GET_IFACE (file)->get_name != NULL, NULL);

	return NOLPHIN_FILE_INFO_GET_IFACE (file)->get_name (file);
}

/**
 * nolphin_file_info_get_location:
 * @file: a #NolphinFileInfo
 *
 * Returns: (transfer full): a #GFile for the location of @file
 */
GFile *
nolphin_file_info_get_location (NolphinFileInfo *file)
{
	g_return_val_if_fail (NOLPHIN_IS_FILE_INFO (file), NULL);
	g_return_val_if_fail (NOLPHIN_FILE_INFO_GET_IFACE (file)->get_location != NULL, NULL);

	return NOLPHIN_FILE_INFO_GET_IFACE (file)->get_location (file);
}
char *
nolphin_file_info_get_uri (NolphinFileInfo *file)
{
	g_return_val_if_fail (NOLPHIN_IS_FILE_INFO (file), NULL);
	g_return_val_if_fail (NOLPHIN_FILE_INFO_GET_IFACE (file)->get_uri != NULL, NULL);

	return NOLPHIN_FILE_INFO_GET_IFACE (file)->get_uri (file);
}

char *
nolphin_file_info_get_activation_uri (NolphinFileInfo *file)
{
	g_return_val_if_fail (NOLPHIN_IS_FILE_INFO (file), NULL);
	g_return_val_if_fail (NOLPHIN_FILE_INFO_GET_IFACE (file)->get_activation_uri != NULL, NULL);

	return NOLPHIN_FILE_INFO_GET_IFACE (file)->get_activation_uri (file);
}

/**
 * nolphin_file_info_get_parent_location:
 * @file: a #NolphinFileInfo
 *
 * Returns: (allow-none) (transfer full): a #GFile for the parent location of @file, 
 *   or %NULL if @file has no parent
 */
GFile *
nolphin_file_info_get_parent_location (NolphinFileInfo *file)
{
	g_return_val_if_fail (NOLPHIN_IS_FILE_INFO (file), NULL);
	g_return_val_if_fail (NOLPHIN_FILE_INFO_GET_IFACE (file)->get_parent_location != NULL, NULL);

	return NOLPHIN_FILE_INFO_GET_IFACE (file)->get_parent_location (file);
}

char *
nolphin_file_info_get_parent_uri (NolphinFileInfo *file)
{
	g_return_val_if_fail (NOLPHIN_IS_FILE_INFO (file), NULL);
	g_return_val_if_fail (NOLPHIN_FILE_INFO_GET_IFACE (file)->get_parent_uri != NULL, NULL);

	return NOLPHIN_FILE_INFO_GET_IFACE (file)->get_parent_uri (file);
}

/**
 * nolphin_file_info_get_parent_info:
 * @file: a #NolphinFileInfo
 *
 * Returns: (allow-none) (transfer full): a #NolphinFileInfo for the parent of @file, 
 *   or %NULL if @file has no parent
 */
NolphinFileInfo *
nolphin_file_info_get_parent_info (NolphinFileInfo *file)
{
	g_return_val_if_fail (NOLPHIN_IS_FILE_INFO (file), NULL);
	g_return_val_if_fail (NOLPHIN_FILE_INFO_GET_IFACE (file)->get_parent_info != NULL, NULL);

	return NOLPHIN_FILE_INFO_GET_IFACE (file)->get_parent_info (file);
}

/**
 * nolphin_file_info_get_mount:
 * @file: a #NolphinFileInfo
 *
 * Returns: (allow-none) (transfer full): a #GMount for the mount of @file, 
 *   or %NULL if @file has no mount
 */
GMount *
nolphin_file_info_get_mount (NolphinFileInfo *file)
{
	g_return_val_if_fail (NOLPHIN_IS_FILE_INFO (file), NULL);
	g_return_val_if_fail (NOLPHIN_FILE_INFO_GET_IFACE (file)->get_mount != NULL, NULL);
    
	return NOLPHIN_FILE_INFO_GET_IFACE (file)->get_mount (file);
}

char *
nolphin_file_info_get_uri_scheme (NolphinFileInfo *file)
{
	g_return_val_if_fail (NOLPHIN_IS_FILE_INFO (file), NULL);
	g_return_val_if_fail (NOLPHIN_FILE_INFO_GET_IFACE (file)->get_uri_scheme != NULL, NULL);

	return NOLPHIN_FILE_INFO_GET_IFACE (file)->get_uri_scheme (file);
}

char *
nolphin_file_info_get_mime_type (NolphinFileInfo *file)
{
	g_return_val_if_fail (NOLPHIN_IS_FILE_INFO (file), NULL);
	g_return_val_if_fail (NOLPHIN_FILE_INFO_GET_IFACE (file)->get_mime_type != NULL, NULL);

	return NOLPHIN_FILE_INFO_GET_IFACE (file)->get_mime_type (file);
}

gboolean
nolphin_file_info_is_mime_type (NolphinFileInfo *file,
				 const char *mime_type)
{
	g_return_val_if_fail (NOLPHIN_IS_FILE_INFO (file), FALSE);
	g_return_val_if_fail (mime_type != NULL, FALSE);
	g_return_val_if_fail (NOLPHIN_FILE_INFO_GET_IFACE (file)->is_mime_type != NULL, FALSE);

	return NOLPHIN_FILE_INFO_GET_IFACE (file)->is_mime_type (file,
								  mime_type);
}

gboolean
nolphin_file_info_is_directory (NolphinFileInfo *file)
{
	g_return_val_if_fail (NOLPHIN_IS_FILE_INFO (file), FALSE);
	g_return_val_if_fail (NOLPHIN_FILE_INFO_GET_IFACE (file)->is_directory != NULL, FALSE);

	return NOLPHIN_FILE_INFO_GET_IFACE (file)->is_directory (file);
}

gboolean
nolphin_file_info_can_write (NolphinFileInfo *file)
{
	g_return_val_if_fail (NOLPHIN_IS_FILE_INFO (file), FALSE);
	g_return_val_if_fail (NOLPHIN_FILE_INFO_GET_IFACE (file)->can_write != NULL, FALSE);

	return NOLPHIN_FILE_INFO_GET_IFACE (file)->can_write (file);
}

void
nolphin_file_info_add_emblem (NolphinFileInfo *file,
			       const char *emblem_name)
{
	g_return_if_fail (NOLPHIN_IS_FILE_INFO (file));
	g_return_if_fail (NOLPHIN_FILE_INFO_GET_IFACE (file)->add_emblem != NULL);

	NOLPHIN_FILE_INFO_GET_IFACE (file)->add_emblem (file, emblem_name);
}

char *
nolphin_file_info_get_string_attribute (NolphinFileInfo *file,
					 const char *attribute_name)
{
	g_return_val_if_fail (NOLPHIN_IS_FILE_INFO (file), NULL);
	g_return_val_if_fail (NOLPHIN_FILE_INFO_GET_IFACE (file)->get_string_attribute != NULL, NULL);
	g_return_val_if_fail (attribute_name != NULL, NULL);

	return NOLPHIN_FILE_INFO_GET_IFACE (file)->get_string_attribute 
		(file, attribute_name);
}

void
nolphin_file_info_add_string_attribute (NolphinFileInfo *file,
					 const char *attribute_name,
					 const char *value)
{
	g_return_if_fail (NOLPHIN_IS_FILE_INFO (file));
	g_return_if_fail (NOLPHIN_FILE_INFO_GET_IFACE (file)->add_string_attribute != NULL);
	g_return_if_fail (attribute_name != NULL);
	g_return_if_fail (value != NULL);
	
	NOLPHIN_FILE_INFO_GET_IFACE (file)->add_string_attribute 
		(file, attribute_name, value);
}

/**
 * nolphin_file_info_invalidate_extension_info:
 * @file: a #NolphinFile
 *
 * Notifies nolphin to re-run info provider extensions on the given file.
 *
 * This is useful if you have an extension that listens or responds to some external
 * interface for changes to local file metadata (such as a cloud drive changing file emblems.)
 *
 * When a change such as this occurs, call this on the file in question, and nolphin will
 * schedule a call to extension->update_file_info to update its own internal metadata.
 *
 * NOTE: This does *not* need to be called on the tail end of a update_full/update_complete
 * asynchronous extension.  Prior to Nolphin 3.6 this was indeed the case, however, due to a
 * recursion issue in nolphin-directory-async.c (see nolphin 9e67417f8f09.)
 */
void
nolphin_file_info_invalidate_extension_info (NolphinFileInfo *file)
{
	g_return_if_fail (NOLPHIN_IS_FILE_INFO (file));
	g_return_if_fail (NOLPHIN_FILE_INFO_GET_IFACE (file)->invalidate_extension_info != NULL);
	
	NOLPHIN_FILE_INFO_GET_IFACE (file)->invalidate_extension_info (file);
}

/**
 * nolphin_file_info_lookup:
 * @location: the location to lookup the file info for
 *
 * Returns: (transfer full): a #NolphinFileInfo
 */
NolphinFileInfo *
nolphin_file_info_lookup (GFile *location)
{
	return nolphin_file_info_getter (location, FALSE);
}

/**
 * nolphin_file_info_create:
 * @location: the location to create the file info for
 *
 * Returns: (transfer full): a #NolphinFileInfo
 */
NolphinFileInfo *
nolphin_file_info_create (GFile *location)
{
	return nolphin_file_info_getter (location, TRUE);
}

/**
 * nolphin_file_info_lookup_for_uri:
 * @uri: the URI to lookup the file info for
 *
 * Returns: (transfer full): a #NolphinFileInfo
 */
NolphinFileInfo *
nolphin_file_info_lookup_for_uri (const char *uri)
{
	GFile *location;
	NolphinFile *file;

	location = g_file_new_for_uri (uri);
	file = nolphin_file_info_lookup (location);
	g_object_unref (location);

	return file;
}

/**
 * nolphin_file_info_create_for_uri:
 * @uri: the URI to lookup the file info for
 *
 * Returns: (transfer full): a #NolphinFileInfo
 */
NolphinFileInfo *
nolphin_file_info_create_for_uri (const char *uri)
{
	GFile *location;
	NolphinFile *file;

	location = g_file_new_for_uri (uri);
	file = nolphin_file_info_create (location);
	g_object_unref (location);

	return file;
}
