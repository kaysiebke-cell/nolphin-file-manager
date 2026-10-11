/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Nolphin
 *
 * Copyright (C) 2003 Red Hat, Inc.
 * Copyright (C) 2010 Cosimo Cecchi <cosimoc@gnome.org>
 *
 * Nolphin is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nolphin is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; see the file COPYING.  If not,
 * write to the Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
 * Boston, MA 02110-1335, USA.
 */

#ifndef NOLPHIN_CONNECT_SERVER_DIALOG_H
#define NOLPHIN_CONNECT_SERVER_DIALOG_H

#include <gio/gio.h>
#include <gtk/gtk.h>

#include "nolphin-application.h"
#include "nolphin-window.h"

#define NOLPHIN_TYPE_CONNECT_SERVER_DIALOG\
	(nolphin_connect_server_dialog_get_type ())
#define NOLPHIN_CONNECT_SERVER_DIALOG(obj)\
        (G_TYPE_CHECK_INSTANCE_CAST ((obj), NOLPHIN_TYPE_CONNECT_SERVER_DIALOG,\
				     NolphinConnectServerDialog))
#define NOLPHIN_CONNECT_SERVER_DIALOG_CLASS(klass)\
	(G_TYPE_CHECK_CLASS_CAST ((klass), NOLPHIN_TYPE_CONNECT_SERVER_DIALOG,\
				  NolphinConnectServerDialogClass))
#define NOLPHIN_IS_CONNECT_SERVER_DIALOG(obj)\
	(G_TYPE_CHECK_INSTANCE_TYPE ((obj), NOLPHIN_TYPE_CONNECT_SERVER_DIALOG)

typedef struct _NolphinConnectServerDialog NolphinConnectServerDialog;
typedef struct _NolphinConnectServerDialogClass NolphinConnectServerDialogClass;
typedef struct _NolphinConnectServerDialogDetails NolphinConnectServerDialogDetails;

struct _NolphinConnectServerDialog {
	GtkDialog parent;
	NolphinConnectServerDialogDetails *details;
};

struct _NolphinConnectServerDialogClass {
	GtkDialogClass parent_class;
};

GType nolphin_connect_server_dialog_get_type (void);

GtkWidget* nolphin_connect_server_dialog_new (NolphinWindow *window);

void nolphin_connect_server_dialog_display_location_async (NolphinConnectServerDialog *self,
							    GFile *location,
							    GAsyncReadyCallback callback,
							    gpointer user_data);
gboolean nolphin_connect_server_dialog_display_location_finish (NolphinConnectServerDialog *self,
								 GAsyncResult *result,
								 GError **error);

void nolphin_connect_server_dialog_fill_details_async (NolphinConnectServerDialog *self,
							GMountOperation *operation,
							const gchar *default_user,
							const gchar *default_domain,
							GAskPasswordFlags flags,
							GAsyncReadyCallback callback,
							gpointer user_data);
gboolean nolphin_connect_server_dialog_fill_details_finish (NolphinConnectServerDialog *self,
							     GAsyncResult *result);

#endif /* NOLPHIN_CONNECT_SERVER_DIALOG_H */
