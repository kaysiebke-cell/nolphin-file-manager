/*
 *  nolphin-info-provider.c - Interface for Nolphin extensions that 
 *                             provide info about files.
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
#include "nolphin-info-provider.h"

#include <glib-object.h>

G_DEFINE_INTERFACE (NolphinInfoProvider, nolphin_info_provider, G_TYPE_OBJECT)

/**
 * SECTION:nolphin-info-provider
 * @Title: NolphinInfoProvider
 * @Short_description: An interface to allow collection of additional file info.
 *
 * This interface can be used to collect additional file info, generally used
 * together with a #NolphinColumnProvider.  It can be used as a synchronous or async
 * interface.
 *
 * Additional, it can act in the background and notify Nolphin when information has
 * changed from some external source.
 **/

static void
nolphin_info_provider_default_init (NolphinInfoProviderInterface *klass)
{
}

NolphinOperationResult 
nolphin_info_provider_update_file_info (NolphinInfoProvider     *provider,
                                     NolphinFileInfo         *file,
                                     GClosure             *update_complete,
                                     NolphinOperationHandle **handle)
{
	g_return_val_if_fail (NOLPHIN_IS_INFO_PROVIDER (provider),
			      NOLPHIN_OPERATION_FAILED);
	g_return_val_if_fail (NOLPHIN_INFO_PROVIDER_GET_IFACE (provider)->update_file_info != NULL,
			      NOLPHIN_OPERATION_FAILED);
	g_return_val_if_fail (update_complete != NULL, 
			      NOLPHIN_OPERATION_FAILED);
	g_return_val_if_fail (handle != NULL, NOLPHIN_OPERATION_FAILED);

	return NOLPHIN_INFO_PROVIDER_GET_IFACE (provider)->update_file_info 
		(provider, file, update_complete, handle);
}

void
nolphin_info_provider_cancel_update (NolphinInfoProvider    *provider,
                                  NolphinOperationHandle *handle)
{
	g_return_if_fail (NOLPHIN_IS_INFO_PROVIDER (provider));
	g_return_if_fail (NOLPHIN_INFO_PROVIDER_GET_IFACE (provider)->cancel_update != NULL);
	g_return_if_fail (handle != NULL);

	NOLPHIN_INFO_PROVIDER_GET_IFACE (provider)->cancel_update (provider,
								    handle);
}

void
nolphin_info_provider_update_complete_invoke (GClosure            *update_complete,
                                           NolphinInfoProvider    *provider,
                                           NolphinOperationHandle *handle,
                                           NolphinOperationResult  result)
{
	GValue args[3] = { { 0, } };
	GValue return_val = { 0, };
	
	g_return_if_fail (update_complete != NULL);
	g_return_if_fail (NOLPHIN_IS_INFO_PROVIDER (provider));

	g_value_init (&args[0], NOLPHIN_TYPE_INFO_PROVIDER);
	g_value_init (&args[1], G_TYPE_POINTER);
	g_value_init (&args[2], G_TYPE_INT);

	g_value_set_object (&args[0], provider);
	g_value_set_pointer (&args[1], handle);
	g_value_set_int (&args[2], result);

	g_closure_invoke (update_complete, &return_val, 3, args, NULL);

	g_value_unset (&args[0]);
	g_value_unset (&args[1]);
	g_value_unset (&args[2]);
}

