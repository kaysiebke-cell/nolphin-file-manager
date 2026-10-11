/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nolphin-view-factory.c: register and create NolphinViews
 
   Copyright (C) 2004 Red Hat Inc.
  
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

#include "nolphin-view-factory.h"

static GList *registered_views;

void
nolphin_view_factory_register (NolphinViewInfo *view_info)
{
	g_return_if_fail (view_info != NULL);
	g_return_if_fail (view_info->id != NULL);
	g_return_if_fail (nolphin_view_factory_lookup (view_info->id) == NULL);
	
	registered_views = g_list_append (registered_views, view_info);
}

const NolphinViewInfo *
nolphin_view_factory_lookup (const char *id)
{
	GList *l;
	NolphinViewInfo *view_info;

	g_return_val_if_fail (id != NULL, NULL);

	
	for (l = registered_views; l != NULL; l = l->next) {
		view_info = l->data;
		
		if (strcmp (view_info->id, id) == 0) {
			return view_info;
		}
	}
	return NULL;
}

NolphinView *
nolphin_view_factory_create (const char *id,
			      NolphinWindowSlot *slot)
{
	const NolphinViewInfo *view_info;
	NolphinView *view;

	view_info = nolphin_view_factory_lookup (id);
	if (view_info == NULL) {
		return NULL;
	}

	view = view_info->create (slot);
	if (g_object_is_floating (view)) {
		g_object_ref_sink (view);
	}
	return view;
}

gboolean
nolphin_view_factory_view_supports_uri (const char *id,
					 GFile *location,
					 GFileType file_type,
					 const char *mime_type)
{
	const NolphinViewInfo *view_info;
	char *uri;
	gboolean res;

	view_info = nolphin_view_factory_lookup (id);
	if (view_info == NULL) {
		return FALSE;
	}
	uri = g_file_get_uri (location);
	res = view_info->supports_uri (uri, file_type, mime_type);
	g_free (uri);
	return res;
	
}

GList *
nolphin_view_factory_get_views_for_uri (const char *uri,
					 GFileType file_type,
					 const char *mime_type)
{
	GList *l, *res;
	const NolphinViewInfo *view_info;

	res = NULL;
	
	for (l = registered_views; l != NULL; l = l->next) {
		view_info = l->data;

		if (view_info->supports_uri (uri, file_type, mime_type)) {
			res = g_list_prepend (res, g_strdup (view_info->id));
		}
	}
	
	return g_list_reverse (res);
}


