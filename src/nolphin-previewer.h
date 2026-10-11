/*
 * nolphin-previewer: nolphin previewer DBus wrapper
 *
 * Copyright (C) 2011, Red Hat, Inc.
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Suite 500, MA 02110-1335, USA.
 *
 * Author: Cosimo Cecchi <cosimoc@redhat.com>
 *
 */

#ifndef __NOLPHIN_PREVIEWER_H__
#define __NOLPHIN_PREVIEWER_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define NOLPHIN_TYPE_PREVIEWER nolphin_previewer_get_type()
#define NOLPHIN_PREVIEWER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NOLPHIN_TYPE_PREVIEWER, NolphinPreviewer))
#define NOLPHIN_PREVIEWER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NOLPHIN_TYPE_PREVIEWER, NolphinPreviewerClass))
#define NOLPHIN_IS_PREVIEWER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NOLPHIN_TYPE_PREVIEWER))
#define NOLPHIN_IS_PREVIEWER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NOLPHIN_TYPE_PREVIEWER))
#define NOLPHIN_PREVIEWER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NOLPHIN_TYPE_PREVIEWER, NolphinPreviewerClass))

typedef struct _NolphinPreviewerPriv NolphinPreviewerPriv;

typedef struct {
  GObject parent;

  /* private */
  NolphinPreviewerPriv *priv;
} NolphinPreviewer;

typedef struct {
  GObjectClass parent_class;
} NolphinPreviewerClass;

GType nolphin_previewer_get_type (void);

NolphinPreviewer *nolphin_previewer_get_singleton (void);
void nolphin_previewer_call_show_file (NolphinPreviewer *previewer,
                                        const gchar *uri,
                                        guint xid,
					gboolean close_if_already_visible);
void nolphin_previewer_call_close (NolphinPreviewer *previewer);

G_END_DECLS

#endif /* __NOLPHIN_PREVIEWER_H__ */
