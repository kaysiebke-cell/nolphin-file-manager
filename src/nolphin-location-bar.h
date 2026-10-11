/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nolphin
 *
 * Copyright (C) 2000 Eazel, Inc.
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
 * Author: Maciej Stachowiak <mjs@eazel.com>
 *         Ettore Perazzoli <ettore@gnu.org>
 */

/* nolphin-location-bar.h - Location bar for Nolphin
 */

#ifndef NOLPHIN_LOCATION_BAR_H
#define NOLPHIN_LOCATION_BAR_H

#include <libnolphin-private/nolphin-entry.h>
#include <gtk/gtk.h>

#define NOLPHIN_TYPE_LOCATION_BAR nolphin_location_bar_get_type()
#define NOLPHIN_LOCATION_BAR(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NOLPHIN_TYPE_LOCATION_BAR, NolphinLocationBar))
#define NOLPHIN_LOCATION_BAR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NOLPHIN_TYPE_LOCATION_BAR, NolphinLocationBarClass))
#define NOLPHIN_IS_LOCATION_BAR(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NOLPHIN_TYPE_LOCATION_BAR))
#define NOLPHIN_IS_LOCATION_BAR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NOLPHIN_TYPE_LOCATION_BAR))
#define NOLPHIN_LOCATION_BAR_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NOLPHIN_TYPE_LOCATION_BAR, NolphinLocationBarClass))

typedef struct NolphinLocationBarDetails NolphinLocationBarDetails;

typedef struct NolphinLocationBar {
	GtkBox parent;
	NolphinLocationBarDetails *details;
} NolphinLocationBar;

typedef struct {
	GtkBoxClass parent_class;

	/* for GtkBindingSet */
	void         (* cancel)           (NolphinLocationBar *bar);
} NolphinLocationBarClass;

GType      nolphin_location_bar_get_type     	(void);
GtkWidget* nolphin_location_bar_new          	(void);
NolphinEntry * nolphin_location_bar_get_entry (NolphinLocationBar *location_bar);

void	nolphin_location_bar_activate	 (NolphinLocationBar *bar);
void    nolphin_location_bar_set_location     (NolphinLocationBar *bar,
						const char          *location);
gboolean nolphin_location_bar_has_focus (NolphinLocationBar *location_bar);

#endif /* NOLPHIN_LOCATION_BAR_H */
