/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nolphin-file-conflict-dialog: dialog that handles file conflicts
   during transfer operations.

   Copyright (C) 2008, Cosimo Cecchi

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
   
   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
   Boston, MA 02110-1335, USA.
   
   Authors: Cosimo Cecchi <cosimoc@gnome.org>
*/

#ifndef NOLPHIN_FILE_CONFLICT_DIALOG_H
#define NOLPHIN_FILE_CONFLICT_DIALOG_H

#include <glib-object.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

#define NOLPHIN_TYPE_FILE_CONFLICT_DIALOG \
	(nolphin_file_conflict_dialog_get_type ())
#define NOLPHIN_FILE_CONFLICT_DIALOG(o) \
	(G_TYPE_CHECK_INSTANCE_CAST ((o), NOLPHIN_TYPE_FILE_CONFLICT_DIALOG,\
				     NolphinFileConflictDialog))
#define NOLPHIN_FILE_CONFLICT_DIALOG_CLASS(k) \
	(G_TYPE_CHECK_CLASS_CAST((k), NOLPHIN_TYPE_FILE_CONFLICT_DIALOG,\
				 NolphinFileConflictDialogClass))
#define NOLPHIN_IS_FILE_CONFLICT_DIALOG(o) \
	(G_TYPE_CHECK_INSTANCE_TYPE ((o), NOLPHIN_TYPE_FILE_CONFLICT_DIALOG))
#define NOLPHIN_IS_FILE_CONFLICT_DIALOG_CLASS(k) \
	(G_TYPE_CHECK_CLASS_TYPE ((k), NOLPHIN_TYPE_FILE_CONFLICT_DIALOG))
#define NOLPHIN_FILE_CONFLICT_DIALOG_GET_CLASS(o) \
	(G_TYPE_INSTANCE_GET_CLASS ((o), NOLPHIN_TYPE_FILE_CONFLICT_DIALOG,\
				    NolphinFileConflictDialogClass))

typedef struct _NolphinFileConflictDialog        NolphinFileConflictDialog;
typedef struct _NolphinFileConflictDialogClass   NolphinFileConflictDialogClass;
typedef struct _NolphinFileConflictDialogDetails NolphinFileConflictDialogDetails;

struct _NolphinFileConflictDialog {
	GtkDialog parent;
	NolphinFileConflictDialogDetails *details;
};

struct _NolphinFileConflictDialogClass {
	GtkDialogClass parent_class;
};

enum
{
	CONFLICT_RESPONSE_SKIP = 1,
	CONFLICT_RESPONSE_AUTO_RENAME = 2,
	CONFLICT_RESPONSE_REPLACE = 3,
	CONFLICT_RESPONSE_RENAME = 4
};

GType nolphin_file_conflict_dialog_get_type (void) G_GNUC_CONST;

GtkWidget* nolphin_file_conflict_dialog_new              (GtkWindow *parent,
							   GFile *source,
							   GFile *destination,
							   GFile *dest_dir);
char*      nolphin_file_conflict_dialog_get_new_name     (NolphinFileConflictDialog *dialog);
gboolean   nolphin_file_conflict_dialog_get_apply_to_all (NolphinFileConflictDialog *dialog);

#endif /* NOLPHIN_FILE_CONFLICT_DIALOG_H */
