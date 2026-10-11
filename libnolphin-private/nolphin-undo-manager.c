/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* NolphinUndoManager - Undo/Redo transaction manager.
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

#include <config.h>
#include <libnolphin-private/nolphin-undo-manager.h>
#include <libnolphin-private/nolphin-undo-transaction.h>

#include <gtk/gtk.h>
#include "nolphin-undo-private.h"

struct NolphinUndoManagerDetails {
	NolphinUndoTransaction *transaction;

	/* These are used to tell undo from redo. */
	gboolean current_transaction_is_redo;
	gboolean new_transaction_is_redo;

	/* These are used only so that we can complain if we get more
	 * than one transaction inside undo.
	 */
	gboolean undo_in_progress;
        int num_transactions_during_undo;
};

enum {
	CHANGED,
	LAST_SIGNAL
};
static guint signals[LAST_SIGNAL] = { 0 };

typedef struct {
	char *path;
	char *no_undo_menu_item_label;
	char *no_undo_menu_item_hint;
} UndoMenuHandlerConnection;

G_DEFINE_TYPE (NolphinUndoManager,
	       nolphin_undo_manager,
	       G_TYPE_OBJECT)

static void
release_transaction (NolphinUndoManager *manager)
{
	NolphinUndoTransaction *transaction;

	transaction = manager->details->transaction;
	manager->details->transaction = NULL;
	if (transaction != NULL) {
		g_object_unref (transaction);
	}
}

void
nolphin_undo_manager_append (NolphinUndoManager *manager,
			      NolphinUndoTransaction *transaction)
{
	NolphinUndoTransaction *duplicate_transaction;

	/* Check, complain, and ignore the passed-in transaction if we
	 * get more than one within a single undo operation. The single
	 * transaction we get during the undo operation is supposed to
	 * be the one for redoing the undo (or re-undoing the redo).
	 */
	if (manager->details->undo_in_progress) {
		manager->details->num_transactions_during_undo += 1;
		g_return_if_fail (manager->details->num_transactions_during_undo == 1);		
	}
	
	g_return_if_fail (transaction != NULL);

	/* Keep a copy of this transaction (dump the old one). */
	duplicate_transaction = g_object_ref (transaction);
	release_transaction (manager);
	manager->details->transaction = duplicate_transaction;
	manager->details->current_transaction_is_redo =
		manager->details->new_transaction_is_redo;
	
	/* Fire off signal indicating that the undo state has changed. */
	g_signal_emit (manager, signals[CHANGED], 0);
}

void
nolphin_undo_manager_forget (NolphinUndoManager *manager,
			      NolphinUndoTransaction *transaction)
{
	/* Nothing to forget unless the item we are passed is the
	 * transaction we are currently holding.
	 */
	if (transaction != manager->details->transaction) {
		return;
	}

	/* Get rid of the transaction we are holding on to. */
	release_transaction (manager);
	
	/* Fire off signal indicating that the undo state has changed. */
	g_signal_emit (manager, signals[CHANGED], 0);
}

NolphinUndoManager *
nolphin_undo_manager_new (void)
{
	return NOLPHIN_UNDO_MANAGER (g_object_new (nolphin_undo_manager_get_type (), NULL));
}

static void
nolphin_undo_manager_init (NolphinUndoManager *manager)
{
	manager->details = g_new0 (NolphinUndoManagerDetails, 1);
}

void
nolphin_undo_manager_undo (NolphinUndoManager *manager)
{
	NolphinUndoTransaction *transaction;

	g_return_if_fail (NOLPHIN_IS_UNDO_MANAGER (manager));

	transaction = manager->details->transaction;
	manager->details->transaction = NULL;
	if (transaction != NULL) {
		/* Perform the undo. New transactions that come in
		 * during an undo are redo transactions. New
		 * transactions that come in during a redo are undo
		 * transactions. Transactions that come in outside
		 * are always undo and never redo.
		 */
		manager->details->new_transaction_is_redo =
			!manager->details->current_transaction_is_redo;
		manager->details->undo_in_progress = TRUE;
		manager->details->num_transactions_during_undo = 0;
		nolphin_undo_transaction_undo (transaction);
		manager->details->undo_in_progress = FALSE;
		manager->details->new_transaction_is_redo = FALSE;

		/* Let go of the transaction. */
		g_object_unref (transaction);

		/* Fire off signal indicating the undo state has changed. */
		g_signal_emit (manager, signals[CHANGED], 0);
	}
}

static void
finalize (GObject *object)
{
	NolphinUndoManager *manager;

	manager = NOLPHIN_UNDO_MANAGER (object);

	release_transaction (manager);

	g_free (manager->details);

	if (G_OBJECT_CLASS (nolphin_undo_manager_parent_class)->finalize) {
		(* G_OBJECT_CLASS (nolphin_undo_manager_parent_class)->finalize) (object);
	}
}

void
nolphin_undo_manager_attach (NolphinUndoManager *manager, GObject *target)
{
	g_return_if_fail (NOLPHIN_IS_UNDO_MANAGER (manager));
	g_return_if_fail (G_IS_OBJECT (target));

	nolphin_undo_attach_undo_manager (G_OBJECT (target), manager);
}

static void
nolphin_undo_manager_class_init (NolphinUndoManagerClass *class)
{
	G_OBJECT_CLASS (class)->finalize = finalize;

	signals[CHANGED] = g_signal_new
		("changed",
		 G_TYPE_FROM_CLASS (class),
		 G_SIGNAL_RUN_LAST,
		 G_STRUCT_OFFSET (NolphinUndoManagerClass,
				  changed),
		 NULL, NULL,
		 g_cclosure_marshal_VOID__VOID,
		 G_TYPE_NONE, 0);
}
