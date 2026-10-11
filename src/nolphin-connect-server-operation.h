/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Nolphin
 *
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
 *
 * Author: Cosimo Cecchi <cosimoc@gnome.org>
 */

#ifndef __NOLPHIN_CONNECT_SERVER_OPERATION_H__
#define __NOLPHIN_CONNECT_SERVER_OPERATION_H__

#include <gio/gio.h>
#include <gtk/gtk.h>

#include "nolphin-connect-server-dialog.h"

#define NOLPHIN_TYPE_CONNECT_SERVER_OPERATION\
	(nolphin_connect_server_operation_get_type ())
#define NOLPHIN_CONNECT_SERVER_OPERATION(obj)\
  (G_TYPE_CHECK_INSTANCE_CAST ((obj),\
			       NOLPHIN_TYPE_CONNECT_SERVER_OPERATION,\
			       NolphinConnectServerOperation))
#define NOLPHIN_CONNECT_SERVER_OPERATION_CLASS(klass)\
  (G_TYPE_CHECK_CLASS_CAST ((klass), NOLPHIN_TYPE_CONNECT_SERVER_OPERATION,\
			    NolphinConnectServerOperationClass))
#define NOLPHIN_IS_CONNECT_SERVER_OPERATION(obj)\
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NOLPHIN_TYPE_CONNECT_SERVER_OPERATION)

typedef struct _NolphinConnectServerOperationDetails
  NolphinConnectServerOperationDetails;

typedef struct {
	GtkMountOperation parent;
	NolphinConnectServerOperationDetails *details;
} NolphinConnectServerOperation;

typedef struct {
	GtkMountOperationClass parent_class;
} NolphinConnectServerOperationClass;

GType nolphin_connect_server_operation_get_type (void);

GMountOperation *
nolphin_connect_server_operation_new (NolphinConnectServerDialog *dialog);


#endif /* __NOLPHIN_CONNECT_SERVER_OPERATION_H__ */
