/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nolphin-application: main Nolphin application class.
 *
 * Copyright (C) 2000 Red Hat, Inc.
 * Copyright (C) 2010 Cosimo Cecchi <cosimoc@gnome.org>
 *
 * Nolphin is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nolphin is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
 * Boston, MA 02110-1335, USA.
 */

#ifndef __NOLPHIN_MAIN_APPLICATION_H__
#define __NOLPHIN_MAIN_APPLICATION_H__

#include <gdk/gdk.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

#include <libnolphin-private/nolphin-undo-manager.h>

#include "nolphin-window.h"
#include "nolphin-application.h"

#define NOLPHIN_TYPE_MAIN_APPLICATION nolphin_main_application_get_type()
#define NOLPHIN_MAIN_APPLICATION(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NOLPHIN_TYPE_MAIN_APPLICATION, NolphinMainApplication))
#define NOLPHIN_MAIN_APPLICATION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NOLPHIN_TYPE_MAIN_APPLICATION, NolphinMainApplicationClass))
#define NOLPHIN_IS_MAIN_APPLICATION(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NOLPHIN_TYPE_MAIN_APPLICATION))
#define NOLPHIN_IS_MAIN_APPLICATION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NOLPHIN_TYPE_MAIN_APPLICATION))
#define NOLPHIN_MAIN_APPLICATION_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NOLPHIN_TYPE_MAIN_APPLICATION, NolphinMainApplicationClass))

typedef struct _NolphinMainApplicationPriv NolphinMainApplicationPriv;

typedef struct {
	NolphinApplication parent;

	NolphinMainApplicationPriv *priv;
} NolphinMainApplication;

typedef struct {
	NolphinApplicationClass parent_class;
} NolphinMainApplicationClass;

GType nolphin_main_application_get_type (void);

NolphinApplication *nolphin_main_application_get_singleton (void);

#endif /* __NOLPHIN_MAIN_APPLICATION_H__ */
