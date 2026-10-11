/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nolphin-column-choose.h - A column chooser widget

   Copyright (C) 2004 Novell, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the column COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
   Boston, MA 02110-1335, USA.

   Authors: Dave Camp <dave@ximian.com>
*/

#ifndef NOLPHIN_COLUMN_CHOOSER_H
#define NOLPHIN_COLUMN_CHOOSER_H

#include <gtk/gtk.h>
#include <libnolphin-private/nolphin-file.h>

#define NOLPHIN_TYPE_COLUMN_CHOOSER nolphin_column_chooser_get_type()
#define NOLPHIN_COLUMN_CHOOSER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NOLPHIN_TYPE_COLUMN_CHOOSER, NolphinColumnChooser))
#define NOLPHIN_COLUMN_CHOOSER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NOLPHIN_TYPE_COLUMN_CHOOSER, NolphinColumnChooserClass))
#define NOLPHIN_IS_COLUMN_CHOOSER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NOLPHIN_TYPE_COLUMN_CHOOSER))
#define NOLPHIN_IS_COLUMN_CHOOSER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NOLPHIN_TYPE_COLUMN_CHOOSER))
#define NOLPHIN_COLUMN_CHOOSER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NOLPHIN_TYPE_COLUMN_CHOOSER, NolphinColumnChooserClass))

typedef struct _NolphinColumnChooserDetails NolphinColumnChooserDetails;

typedef struct {
	GtkBox parent;
	
	NolphinColumnChooserDetails *details;
} NolphinColumnChooser;

typedef struct {
        GtkBoxClass parent_slot;

	void (*changed) (NolphinColumnChooser *chooser);
	void (*use_default) (NolphinColumnChooser *chooser);
} NolphinColumnChooserClass;

GType      nolphin_column_chooser_get_type            (void);
GtkWidget *nolphin_column_chooser_new                 (NolphinFile *file);
void       nolphin_column_chooser_set_settings    (NolphinColumnChooser   *chooser,
						    char                   **visible_columns, 
						    char                   **column_order);
void       nolphin_column_chooser_get_settings    (NolphinColumnChooser *chooser,
						    char                  ***visible_columns, 
						    char                  ***column_order);

#endif /* NOLPHIN_COLUMN_CHOOSER_H */
