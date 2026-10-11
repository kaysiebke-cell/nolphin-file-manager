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

#include <config.h>
#include <libnolphin-private/nolphin-undo.h>
#include <libnolphin-private/nolphin-undo-manager.h>
#include <libnolphin-private/nolphin-undo-transaction.h>

#include "nolphin-undo-private.h"
#include <gtk/gtk.h>

#define NOLPHIN_UNDO_TRANSACTION_LIST_DATA "Nolphin undo transaction list"

/* undo atoms */
static void undo_atom_list_free                  (GList                        *list);
static void undo_atom_list_undo_and_free         (GList                        *list);

G_DEFINE_TYPE (NolphinUndoTransaction, nolphin_undo_transaction,
	       G_TYPE_OBJECT);

NolphinUndoTransaction *
nolphin_undo_transaction_new (const char *operation_name,
			       const char *undo_menu_item_label,
			       const char *undo_menu_item_hint,
			       const char *redo_menu_item_label,
			       const char *redo_menu_item_hint)
{
	NolphinUndoTransaction *transaction;

	transaction = NOLPHIN_UNDO_TRANSACTION (g_object_new (nolphin_undo_transaction_get_type (), NULL));
	
	transaction->operation_name = g_strdup (operation_name);
	transaction->undo_menu_item_label = g_strdup (undo_menu_item_label);
	transaction->undo_menu_item_hint = g_strdup (undo_menu_item_hint);
	transaction->redo_menu_item_label = g_strdup (redo_menu_item_label);
	transaction->redo_menu_item_hint = g_strdup (redo_menu_item_hint);

	return transaction;
}

static void 
nolphin_undo_transaction_init (NolphinUndoTransaction *transaction)
{
}

static void
remove_transaction_from_object (gpointer list_data, gpointer callback_data)
{
	NolphinUndoAtom *atom;
	NolphinUndoTransaction *transaction;
	GList *list;
	
	g_assert (list_data != NULL);
	atom = list_data;
	transaction = NOLPHIN_UNDO_TRANSACTION (callback_data);

	/* Remove the transaction from the list on the atom. */
	list = g_object_get_data (atom->target, NOLPHIN_UNDO_TRANSACTION_LIST_DATA);

	if (list != NULL) {
		list = g_list_remove (list, transaction);
		g_object_set_data (atom->target, NOLPHIN_UNDO_TRANSACTION_LIST_DATA, list);
	}
}

static void
remove_transaction_from_atom_targets (NolphinUndoTransaction *transaction)
{

	g_list_foreach (transaction->atom_list,
			remove_transaction_from_object,
			transaction);	
}

static void
nolphin_undo_transaction_finalize (GObject *object)
{
	NolphinUndoTransaction *transaction;

	transaction = NOLPHIN_UNDO_TRANSACTION (object);
	
	remove_transaction_from_atom_targets (transaction);
	undo_atom_list_free (transaction->atom_list);

	g_free (transaction->operation_name);
	g_free (transaction->undo_menu_item_label);
	g_free (transaction->undo_menu_item_hint);
	g_free (transaction->redo_menu_item_label);
	g_free (transaction->redo_menu_item_hint);

	if (transaction->owner != NULL) {
		g_object_unref (transaction->owner);
	}
	
	G_OBJECT_CLASS (nolphin_undo_transaction_parent_class)->finalize (object);
}

void
nolphin_undo_transaction_add_atom (NolphinUndoTransaction *transaction, 
				    const NolphinUndoAtom *atom)
{
	GList *list;
	
	g_return_if_fail (NOLPHIN_IS_UNDO_TRANSACTION (transaction));
	g_return_if_fail (atom != NULL);
	g_return_if_fail (G_IS_OBJECT (atom->target));

	/* Add the atom to the atom list in the transaction. */
	transaction->atom_list = g_list_append
		(transaction->atom_list, g_memdup (atom, sizeof (*atom)));

	/* Add the transaction to the list on the atoms target object. */
	list = g_object_get_data (atom->target, NOLPHIN_UNDO_TRANSACTION_LIST_DATA);
	if (g_list_find (list, transaction) != NULL) {
		return;
	}

	/* If it's not already on that atom, this object is new. */
	list = g_list_prepend (list, transaction);
	g_object_set_data (atom->target, NOLPHIN_UNDO_TRANSACTION_LIST_DATA, list);

	/* Connect a signal handler to the atom so it will unregister
	 * itself when it's destroyed.
	 */
	g_signal_connect (atom->target, "destroy",
			  G_CALLBACK (nolphin_undo_transaction_unregister_object),
			  NULL);
}

void 
nolphin_undo_transaction_undo (NolphinUndoTransaction *transaction)
{
	g_return_if_fail (NOLPHIN_IS_UNDO_TRANSACTION (transaction));

	remove_transaction_from_atom_targets (transaction);
	undo_atom_list_undo_and_free (transaction->atom_list);
	
	transaction->atom_list = NULL;
}

void
nolphin_undo_transaction_add_to_undo_manager (NolphinUndoTransaction *transaction,
					       NolphinUndoManager *manager)
{
	g_return_if_fail (NOLPHIN_IS_UNDO_TRANSACTION (transaction));
	g_return_if_fail (transaction->owner == NULL);

	if (manager != NULL) {
		nolphin_undo_manager_append (manager, transaction);
		transaction->owner = g_object_ref (manager);
	}
}

static void
remove_atoms (NolphinUndoTransaction *transaction,
	      GObject *object)
{
	GList *p, *next;
	NolphinUndoAtom *atom;

	g_assert (NOLPHIN_IS_UNDO_TRANSACTION (transaction));
	g_assert (G_IS_OBJECT (object));

	/* Destroy any atoms for this object. */
	for (p = transaction->atom_list; p != NULL; p = next) {
		atom = p->data;
		next = p->next;

		if (atom->target == object) {
			transaction->atom_list = g_list_remove_link
				(transaction->atom_list, p);
			undo_atom_list_free (p);
		}
	}

	/* If all the atoms are gone, forget this transaction.
	 * This may end up freeing the transaction.
	 */
	if (transaction->atom_list == NULL) {
		nolphin_undo_manager_forget (
			transaction->owner, transaction);
	}
}

static void
remove_atoms_cover (gpointer list_data, gpointer callback_data)
{
	if (NOLPHIN_IS_UNDO_TRANSACTION (list_data)) {
		remove_atoms (NOLPHIN_UNDO_TRANSACTION (list_data), G_OBJECT (callback_data));
	}
}

void
nolphin_undo_transaction_unregister_object (GObject *object)
{
	GList *list;

	g_return_if_fail (G_IS_OBJECT (object));

	/* Remove atoms from each transaction on the list. */
	list = g_object_get_data (object, NOLPHIN_UNDO_TRANSACTION_LIST_DATA);
	if (list != NULL) {
		g_list_foreach (list, remove_atoms_cover, object);	
		g_list_free (list);
		g_object_set_data (object, NOLPHIN_UNDO_TRANSACTION_LIST_DATA, NULL);
	}
}

static void
undo_atom_free (NolphinUndoAtom *atom)
{
	/* Call the destroy-notify function if it's present. */
	if (atom->callback_data_destroy_notify != NULL) {
		(* atom->callback_data_destroy_notify) (atom->callback_data);
	}

	/* Free the atom storage. */
	g_free (atom);
}

static void
undo_atom_undo_and_free (NolphinUndoAtom *atom)
{
	/* Call the function that does the actual undo. */
	(* atom->callback) (atom->target, atom->callback_data);

	/* Get rid of the atom now that it's spent. */
	undo_atom_free (atom);
}

static void
undo_atom_free_cover (gpointer atom, gpointer callback_data)
{
	g_assert (atom != NULL);
	g_assert (callback_data == NULL);
	undo_atom_free (atom);
}

static void
undo_atom_undo_and_free_cover (gpointer atom, gpointer callback_data)
{
	g_assert (atom != NULL);
	g_assert (callback_data == NULL);
	undo_atom_undo_and_free (atom);
}

static void
undo_atom_list_free (GList *list)
{
	g_list_foreach (list, undo_atom_free_cover, NULL);
	g_list_free (list);
}

static void
undo_atom_list_undo_and_free (GList *list)
{
	g_list_foreach (list, undo_atom_undo_and_free_cover, NULL);
	g_list_free (list);
}

static void
nolphin_undo_transaction_class_init (NolphinUndoTransactionClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = nolphin_undo_transaction_finalize;
}
