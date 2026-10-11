/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* NolphinUndoManager - Manages undo and redo transactions.
 *                       This is the public interface used by the application.                      
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

#ifndef NOLPHIN_UNDO_MANAGER_H
#define NOLPHIN_UNDO_MANAGER_H

#include <libnolphin-private/nolphin-undo.h>

#define NOLPHIN_TYPE_UNDO_MANAGER nolphin_undo_manager_get_type()
#define NOLPHIN_UNDO_MANAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NOLPHIN_TYPE_UNDO_MANAGER, NolphinUndoManager))
#define NOLPHIN_UNDO_MANAGER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NOLPHIN_TYPE_UNDO_MANAGER, NolphinUndoManagerClass))
#define NOLPHIN_IS_UNDO_MANAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NOLPHIN_TYPE_UNDO_MANAGER))
#define NOLPHIN_IS_UNDO_MANAGER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NOLPHIN_TYPE_UNDO_MANAGER))
#define NOLPHIN_UNDO_MANAGER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NOLPHIN_TYPE_UNDO_MANAGER, NolphinUndoManagerClass))
	
typedef struct NolphinUndoManagerDetails NolphinUndoManagerDetails;

typedef struct {
	GObject parent;
	NolphinUndoManagerDetails *details;
} NolphinUndoManager;

typedef struct {
	GObjectClass parent_slot;
	void (* changed) (GObject *object, gpointer data);
} NolphinUndoManagerClass;

GType                nolphin_undo_manager_get_type                           (void);
NolphinUndoManager *nolphin_undo_manager_new                                (void);

/* Undo operations. */
void                 nolphin_undo_manager_undo                               (NolphinUndoManager *undo_manager);

/* Attach the undo manager to a Gtk object so that object and the widgets inside it can participate in undo. */
void                 nolphin_undo_manager_attach                             (NolphinUndoManager *manager,
									       GObject             *object);

void		nolphin_undo_manager_append (NolphinUndoManager *manager,
					      NolphinUndoTransaction *transaction);
void            nolphin_undo_manager_forget (NolphinUndoManager *manager,
					      NolphinUndoTransaction *transaction);

#endif /* NOLPHIN_UNDO_MANAGER_H */
