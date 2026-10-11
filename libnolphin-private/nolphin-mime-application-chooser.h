/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
   nolphin-mime-application-chooser.c: Manages applications for mime types
 
   Copyright (C) 2004 Novell, Inc.
 
   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but APPLICATIONOUT ANY WARRANTY; applicationout even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along application the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
   Boston, MA 02110-1335, USA.

   Authors: Dave Camp <dave@novell.com>
*/

#ifndef NOLPHIN_MIME_APPLICATION_CHOOSER_H
#define NOLPHIN_MIME_APPLICATION_CHOOSER_H

#include <gtk/gtk.h>

#define NOLPHIN_TYPE_MIME_APPLICATION_CHOOSER         (nolphin_mime_application_chooser_get_type ())
#define NOLPHIN_MIME_APPLICATION_CHOOSER(obj)         (G_TYPE_CHECK_INSTANCE_CAST ((obj), NOLPHIN_TYPE_MIME_APPLICATION_CHOOSER, NolphinMimeApplicationChooser))
#define NOLPHIN_MIME_APPLICATION_CHOOSER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), NOLPHIN_TYPE_MIME_APPLICATION_CHOOSER, NolphinMimeApplicationChooserClass))
#define NOLPHIN_IS_MIME_APPLICATION_CHOOSER(obj)      (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NOLPHIN_TYPE_MIME_APPLICATION_CHOOSER)

typedef struct _NolphinMimeApplicationChooser        NolphinMimeApplicationChooser;
typedef struct _NolphinMimeApplicationChooserClass   NolphinMimeApplicationChooserClass;
typedef struct _NolphinMimeApplicationChooserDetails NolphinMimeApplicationChooserDetails;

struct _NolphinMimeApplicationChooser {
	GtkBox parent;
	NolphinMimeApplicationChooserDetails *details;
};

struct _NolphinMimeApplicationChooserClass {
	GtkBoxClass parent_class;
};

GType      nolphin_mime_application_chooser_get_type (void);
GtkWidget * nolphin_mime_application_chooser_new (const char *uri,
                                                    GList *files,
                                               const char *mime_type,
                                                GtkWidget *ok_button);
GAppInfo  *nolphin_mime_application_chooser_get_info (NolphinMimeApplicationChooser *chooser);
const gchar *nolphin_mime_application_chooser_get_uri (NolphinMimeApplicationChooser *chooser);

#endif /* NOLPHIN_MIME_APPLICATION_CHOOSER_H */
