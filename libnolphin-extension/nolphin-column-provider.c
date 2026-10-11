/*
 *  nolphin-column-provider.c - Interface for Nolphin extensions 
 *                               that provide column specifications.
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
 *  Author:  Dave Camp <dave@ximian.com>
 *
 */

#include <config.h>
#include "nolphin-column-provider.h"

#include <glib-object.h>

G_DEFINE_INTERFACE (NolphinColumnProvider, nolphin_column_provider, G_TYPE_OBJECT)

/**
 * SECTION:nolphin-column-provider
 * @Title: NolphinColumnProvider
 * @Short_description: An interface to provide additional Nolphin list view columns.
 *
 * This allows additional columns to be shown in the list view.  This interface
 * generally needs to be used in tandem with a #NolphinInfoProvider, to feed file
 * info back to populate the column(s).
 *
 **/

static void
nolphin_column_provider_default_init (NolphinColumnProviderInterface *klass)
{
}

/**
 * nolphin_column_provider_get_columns:
 * @provider: a #NolphinColumnProvider
 *
 * Returns: (element-type NolphinColumn) (transfer full): the provided #NolphinColumn objects
 **/
GList *
nolphin_column_provider_get_columns (NolphinColumnProvider *provider)
{
	g_return_val_if_fail (NOLPHIN_IS_COLUMN_PROVIDER (provider), NULL);
	g_return_val_if_fail (NOLPHIN_COLUMN_PROVIDER_GET_IFACE (provider)->get_columns != NULL, NULL);

	return NOLPHIN_COLUMN_PROVIDER_GET_IFACE (provider)->get_columns (provider);
}

