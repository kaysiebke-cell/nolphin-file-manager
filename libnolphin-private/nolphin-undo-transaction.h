/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* NolphinUndoTransaction - An object for an undoable transaction.
 *                           Used internally by undo machinery.
 *                           Not public.
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

#ifndef NOLPHIN_UNDO_TRANSACTION_H
#define NOLPHIN_UNDO_TRANSACTION_H

#include <libnolphin-private/nolphin-undo.h>

#define NOLPHIN_TYPE_UNDO_TRANSACTION nolphin_undo_transaction_get_type()
#define NOLPHIN_UNDO_TRANSACTION(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NOLPHIN_TYPE_UNDO_TRANSACTION, NolphinUndoTransaction))
#define NOLPHIN_UNDO_TRANSACTION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NOLPHIN_TYPE_UNDO_TRANSACTION, NolphinUndoTransactionClass))
#define NOLPHIN_IS_UNDO_TRANSACTION(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NOLPHIN_TYPE_UNDO_TRANSACTION))
#define NOLPHIN_IS_UNDO_TRANSACTION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NOLPHIN_TYPE_UNDO_TRANSACTION))
#define NOLPHIN_UNDO_TRANSACTION_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NOLPHIN_TYPE_UNDO_TRANSACTION, NolphinUndoTransactionClass))

/* The typedef for NolphinUndoTransaction is in nolphin-undo.h
   to avoid circular deps */
typedef struct _NolphinUndoTransactionClass NolphinUndoTransactionClass;

struct _NolphinUndoTransaction {
	GObject parent_slot;
	
	char *operation_name;
	char *undo_menu_item_label;
	char *undo_menu_item_hint;
	char *redo_menu_item_label;
	char *redo_menu_item_hint;
	GList *atom_list;

	NolphinUndoManager *owner;
};

struct _NolphinUndoTransactionClass {
	GObjectClass parent_slot;
};

GType                    nolphin_undo_transaction_get_type            (void);
NolphinUndoTransaction *nolphin_undo_transaction_new                 (const char              *operation_name,
									const char              *undo_menu_item_label,
									const char              *undo_menu_item_hint,
									const char              *redo_menu_item_label,
									const char              *redo_menu_item_hint);
void                     nolphin_undo_transaction_add_atom            (NolphinUndoTransaction *transaction,
									const NolphinUndoAtom  *atom);
void                     nolphin_undo_transaction_add_to_undo_manager (NolphinUndoTransaction *transaction,
									NolphinUndoManager     *manager);
void                     nolphin_undo_transaction_unregister_object   (GObject                 *atom_target);
void                     nolphin_undo_transaction_undo                (NolphinUndoTransaction *transaction);

#endif /* NOLPHIN_UNDO_TRANSACTION_H */
