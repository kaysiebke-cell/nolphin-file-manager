/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nolphin-progress-ui-handler.h: file operation progress user interface.
 *
 * Copyright (C) 2007, 2011 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
 * Boston, MA 02110-1335, USA.
 *
 * Authors: Alexander Larsson <alexl@redhat.com>
 *          Cosimo Cecchi <cosimoc@redhat.com>
 *
 */

#ifndef __NOLPHIN_PROGRESS_UI_HANDLER_H__
#define __NOLPHIN_PROGRESS_UI_HANDLER_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define NOLPHIN_TYPE_PROGRESS_UI_HANDLER nolphin_progress_ui_handler_get_type()
#define NOLPHIN_PROGRESS_UI_HANDLER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NOLPHIN_TYPE_PROGRESS_UI_HANDLER, NolphinProgressUIHandler))
#define NOLPHIN_PROGRESS_UI_HANDLER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NOLPHIN_TYPE_PROGRESS_UI_HANDLER, NolphinProgressUIHandlerClass))
#define NOLPHIN_IS_PROGRESS_UI_HANDLER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NOLPHIN_TYPE_PROGRESS_UI_HANDLER))
#define NOLPHIN_IS_PROGRESS_UI_HANDLER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NOLPHIN_TYPE_PROGRESS_UI_HANDLER))
#define NOLPHIN_PROGRESS_UI_HANDLER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NOLPHIN_TYPE_PROGRESS_UI_HANDLER, NolphinProgressUIHandlerClass))

typedef struct _NolphinProgressUIHandlerPriv NolphinProgressUIHandlerPriv;

typedef struct {
  GObject parent;

  /* private */
  NolphinProgressUIHandlerPriv *priv;
} NolphinProgressUIHandler;

typedef struct {
  GObjectClass parent_class;
} NolphinProgressUIHandlerClass;

GType nolphin_progress_ui_handler_get_type (void);

NolphinProgressUIHandler * nolphin_progress_ui_handler_new (void);

G_END_DECLS

#endif /* __NOLPHIN_PROGRESS_UI_HANDLER_H__ */
