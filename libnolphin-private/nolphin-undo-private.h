/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* xxx
 *
 * Copyright (C) 2000 Eazel, Inc.
 *
 * Author: Gene Z. Ragan <gzr@eazel.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
 * Boston, MA 02110-1335, USA.
 */

#ifndef NOLPHIN_UNDO_PRIVATE_H
#define NOLPHIN_UNDO_PRIVATE_H

#include <libnolphin-private/nolphin-undo.h>
#include <libnolphin-private/nolphin-undo-manager.h>
#include <glib-object.h>

NolphinUndoManager * nolphin_undo_get_undo_manager    (GObject               *attached_object);
void                  nolphin_undo_attach_undo_manager (GObject               *object,
							 NolphinUndoManager   *manager);

#endif /* NOLPHIN_UNDO_PRIVATE_H */
