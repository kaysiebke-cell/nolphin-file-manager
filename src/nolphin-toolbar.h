/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nolphin
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

#ifndef __NOLPHIN_TOOLBAR_H__
#define __NOLPHIN_TOOLBAR_H__

#include <gtk/gtk.h>

#define NOLPHIN_TYPE_TOOLBAR nolphin_toolbar_get_type()
#define NOLPHIN_TOOLBAR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST ((obj), NOLPHIN_TYPE_TOOLBAR, NolphinToolbar))
#define NOLPHIN_TOOLBAR_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST ((klass), NOLPHIN_TYPE_TOOLBAR, NolphinToolbarClass))
#define NOLPHIN_IS_TOOLBAR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE ((obj), NOLPHIN_TYPE_TOOLBAR))
#define NOLPHIN_IS_TOOLBAR_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE ((klass), NOLPHIN_TYPE_TOOLBAR))
#define NOLPHIN_TOOLBAR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS ((obj), NOLPHIN_TYPE_TOOLBAR, NolphinToolbarClass))

typedef struct _NolphinToolbar NolphinToolbar;
typedef struct _NolphinToolbarPriv NolphinToolbarPriv;
typedef struct _NolphinToolbarClass NolphinToolbarClass;

typedef enum {
	NOLPHIN_TOOLBAR_MODE_PATH_BAR,
	NOLPHIN_TOOLBAR_MODE_LOCATION_BAR,
} NolphinToolbarMode;

struct _NolphinToolbar {
	GtkBox parent;

	/* private */
	NolphinToolbarPriv *priv;
};

struct _NolphinToolbarClass {
	GtkBoxClass parent_class;
};

GType nolphin_toolbar_get_type (void);

GtkWidget *nolphin_toolbar_new (GtkActionGroup *action_group);

gboolean  nolphin_toolbar_get_show_location_entry (NolphinToolbar *self);
GtkWidget *nolphin_toolbar_get_path_bar (NolphinToolbar *self);
GtkWidget *nolphin_toolbar_get_location_bar (NolphinToolbar *self);

void nolphin_toolbar_set_show_main_bar (NolphinToolbar *self,
					 gboolean show_main_bar);
void nolphin_toolbar_set_show_location_entry (NolphinToolbar *self,
					       gboolean show_location_entry);
void nolphin_toolbar_update_for_location (NolphinToolbar *self);
#endif /* __NOLPHIN_TOOLBAR_H__ */
