/*
 *  nolphin-info-provider.h - Type definitions for Nolphin extensions
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
 *  Author: Dave Camp <dave@ximian.com>
 * 
 */

/* This interface is implemented by Nolphin extensions that want to 
 * provide information about files.  Extensions are called when Nolphin 
 * needs information about a file.  They are passed a NolphinFileInfo 
 * object which should be filled with relevant information */

#ifndef NOLPHIN_EXTENSION_TYPES_H
#define NOLPHIN_EXTENSION_TYPES_H

#include <glib-object.h>

G_BEGIN_DECLS

/**
 * SECTION:nolphin-extension-types
 * @Title: Types and Enums
 * @Short_description: Module initialization functions, enums, handle struct
 **/

/**
 * NolphinOperationHandle:
 *
 * Handle for asynchronous interfaces.  These are opaque handles that must
 * be unique within an extension object.  These are returned by operations
 * that return NOLPHIN_OPERATION_IN_PROGRESS.
 *
 * For python extensions, the handle is a dummy struct created by the nolphin
 * python bindings on the extension's behalf.  It can be used as a unique
 * key for a dict, for instance, for keeping track of multiple operations
 * at once.
 */
typedef struct _NolphinOperationHandle NolphinOperationHandle;

/**
 * NolphinOperationResult:
 * @NOLPHIN_OPERATION_COMPLETE: Returned if the call succeeded, and the extension is done
 *  with the request.
 * @NOLPHIN_OPERATION_FAILED: Returned if the call failed.
 * @NOLPHIN_OPERATION_IN_PROGRESS: Returned if the extension has begun an async operation.
 *  For C extensions, if this is returned, the extension must set the handle parameter.
 *  For python extensions, handle is already filled, and unique, and can be used for
 *  identifying purposes within the extension.  In either case, the extension must call
 *  the callback closure when the operation is complete (complete_invoke.)
 */
typedef enum {
    NOLPHIN_OPERATION_COMPLETE,
    NOLPHIN_OPERATION_FAILED,
    NOLPHIN_OPERATION_IN_PROGRESS
} NolphinOperationResult;

void nolphin_module_initialize (GTypeModule  *module);
void nolphin_module_shutdown   (void);
void nolphin_module_list_types (const GType **types,
				 int          *num_types);

G_END_DECLS

#endif
