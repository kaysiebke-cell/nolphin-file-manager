/*
 *  nolphin-info-provider.h - Interface for Nolphin extensions that 
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

/* This interface is implemented by Nolphin extensions that want to 
 * provide information about files.  Extensions are called when Nolphin 
 * needs information about a file.  They are passed a NolphinFileInfo 
 * object which should be filled with relevant information */

#ifndef NOLPHIN_INFO_PROVIDER_H
#define NOLPHIN_INFO_PROVIDER_H

#include <glib-object.h>
#include "nolphin-extension-types.h"
#include "nolphin-file-info.h"

G_BEGIN_DECLS

#define NOLPHIN_TYPE_INFO_PROVIDER           (nolphin_info_provider_get_type ())

G_DECLARE_INTERFACE (NolphinInfoProvider, nolphin_info_provider,
                     NOLPHIN, INFO_PROVIDER,
                     GObject)

typedef NolphinInfoProviderInterface NolphinInfoProviderIface;

typedef void (*NolphinInfoProviderUpdateComplete) (NolphinInfoProvider    *provider,
						    NolphinOperationHandle *handle,
						    NolphinOperationResult  result,
						    gpointer                 user_data);

struct _NolphinInfoProviderInterface {
	GTypeInterface g_iface;

	NolphinOperationResult (*update_file_info) (NolphinInfoProvider     *provider,
						     NolphinFileInfo         *file,
						     GClosure                 *update_complete,
						     NolphinOperationHandle **handle);
	void                    (*cancel_update)    (NolphinInfoProvider     *provider,
						     NolphinOperationHandle  *handle);
};

/* pre-G_DECLARE_INTERFACE/G_DEFINE_INTERFACE compatibility */
#define NolphinInfoProviderIface NolphinInfoProviderInterface

/* Interface Functions */
NolphinOperationResult nolphin_info_provider_update_file_info       (NolphinInfoProvider     *provider,
								       NolphinFileInfo         *file,
								       GClosure                 *update_complete,
								       NolphinOperationHandle **handle);
void                    nolphin_info_provider_cancel_update          (NolphinInfoProvider     *provider,
								       NolphinOperationHandle  *handle);

/* Helper functions for implementations */
void                    nolphin_info_provider_update_complete_invoke (GClosure                 *update_complete,
								       NolphinInfoProvider     *provider,
								       NolphinOperationHandle  *handle,
								       NolphinOperationResult   result);

G_END_DECLS

#endif
