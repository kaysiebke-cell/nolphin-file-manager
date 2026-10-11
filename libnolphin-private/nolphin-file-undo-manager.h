/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nolphin-file-undo-manager.h - Manages the undo/redo stack
 *
 * Copyright (C) 2007-2011 Amos Brocco
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
 * Boston, MA 02110-1335, USA.
 *
 * Author: Amos Brocco <amos.brocco@gmail.com>
 */

#ifndef __NOLPHIN_FILE_UNDO_MANAGER_H__
#define __NOLPHIN_FILE_UNDO_MANAGER_H__

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>
#include <gio/gio.h>

#include <libnolphin-private/nolphin-file-undo-operations.h>

typedef struct _NolphinFileUndoManager NolphinFileUndoManager;
typedef struct _NolphinFileUndoManagerClass NolphinFileUndoManagerClass;
typedef struct _NolphinFileUndoManagerPrivate NolphinFileUndoManagerPrivate;

#define NOLPHIN_TYPE_FILE_UNDO_MANAGER\
	(nolphin_file_undo_manager_get_type())
#define NOLPHIN_FILE_UNDO_MANAGER(object)\
	(G_TYPE_CHECK_INSTANCE_CAST((object), NOLPHIN_TYPE_FILE_UNDO_MANAGER,\
				    NolphinFileUndoManager))
#define NOLPHIN_FILE_UNDO_MANAGER_CLASS(klass)\
	(G_TYPE_CHECK_CLASS_CAST((klass), NOLPHIN_TYPE_FILE_UNDO_MANAGER,\
				 NolphinFileUndoManagerClass))
#define NOLPHIN_IS_FILE_UNDO_MANAGER(object)\
	(G_TYPE_CHECK_INSTANCE_TYPE((object), NOLPHIN_TYPE_FILE_UNDO_MANAGER))
#define NOLPHIN_IS_FILE_UNDO_MANAGER_CLASS(klass)\
	(G_TYPE_CHECK_CLASS_TYPE((klass), NOLPHIN_TYPE_FILE_UNDO_MANAGER))
#define NOLPHIN_FILE_UNDO_MANAGER_GET_CLASS(object)\
	(G_TYPE_INSTANCE_GET_CLASS((object), NOLPHIN_TYPE_FILE_UNDO_MANAGER,\
				   NolphinFileUndoManagerClass))

typedef enum {
	NOLPHIN_FILE_UNDO_MANAGER_STATE_NONE,
	NOLPHIN_FILE_UNDO_MANAGER_STATE_UNDO,
	NOLPHIN_FILE_UNDO_MANAGER_STATE_REDO
} NolphinFileUndoManagerState;

struct _NolphinFileUndoManager {
	GObject parent_instance;

	/* < private > */
	NolphinFileUndoManagerPrivate* priv;
};

struct _NolphinFileUndoManagerClass {
	GObjectClass parent_class;
};

GType nolphin_file_undo_manager_get_type (void) G_GNUC_CONST;

NolphinFileUndoManager * nolphin_file_undo_manager_get (void);

void nolphin_file_undo_manager_set_action (NolphinFileUndoInfo *info);
NolphinFileUndoInfo *nolphin_file_undo_manager_get_action (void);

NolphinFileUndoManagerState nolphin_file_undo_manager_get_state (void);

void nolphin_file_undo_manager_undo (GtkWindow *parent_window);
void nolphin_file_undo_manager_redo (GtkWindow *parent_window);

void nolphin_file_undo_manager_push_flag (void);
gboolean nolphin_file_undo_manager_pop_flag (void);

#endif /* __NOLPHIN_FILE_UNDO_MANAGER_H__ */
