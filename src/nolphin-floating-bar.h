/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* Nolphin - Floating status bar.
 *
 * Copyright (C) 2011 Red Hat Inc.
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
 *
 * Authors: Cosimo Cecchi <cosimoc@redhat.com>
 *
 */

#ifndef __NOLPHIN_FLOATING_BAR_H__
#define __NOLPHIN_FLOATING_BAR_H__

#include <gtk/gtk.h>

#define NOLPHIN_FLOATING_BAR_ACTION_ID_STOP 1

#define NOLPHIN_TYPE_FLOATING_BAR nolphin_floating_bar_get_type()
#define NOLPHIN_FLOATING_BAR(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NOLPHIN_TYPE_FLOATING_BAR, NolphinFloatingBar))
#define NOLPHIN_FLOATING_BAR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NOLPHIN_TYPE_FLOATING_BAR, NolphinFloatingBarClass))
#define NOLPHIN_IS_FLOATING_BAR(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NOLPHIN_TYPE_FLOATING_BAR))
#define NOLPHIN_IS_FLOATING_BAR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NOLPHIN_TYPE_FLOATING_BAR))
#define NOLPHIN_FLOATING_BAR_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NOLPHIN_TYPE_FLOATING_BAR, NolphinFloatingBarClass))

typedef struct _NolphinFloatingBar NolphinFloatingBar;
typedef struct _NolphinFloatingBarClass NolphinFloatingBarClass;
typedef struct _NolphinFloatingBarDetails NolphinFloatingBarDetails;

struct _NolphinFloatingBar {
	GtkBox parent;
	NolphinFloatingBarDetails *priv;
};

struct _NolphinFloatingBarClass {
	GtkBoxClass parent_class;
};

/* GObject */
GType       nolphin_floating_bar_get_type  (void);

GtkWidget * nolphin_floating_bar_new              (const gchar *label,
						    gboolean show_spinner);

void        nolphin_floating_bar_set_label        (NolphinFloatingBar *self,
						    const gchar *label);
void        nolphin_floating_bar_set_show_spinner (NolphinFloatingBar *self,
						    gboolean show_spinner);

void        nolphin_floating_bar_add_action       (NolphinFloatingBar *self,
						    const gchar *stock_id,
						    gint action_id);
void        nolphin_floating_bar_cleanup_actions  (NolphinFloatingBar *self);

#endif /* __NOLPHIN_FLOATING_BAR_H__ */

