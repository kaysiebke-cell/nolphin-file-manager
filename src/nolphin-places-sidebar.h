/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 *  Nolphin
 *
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Suite 500, MA 02110-1335, USA.
 *
 *  Author : Mr Jamie McCracken (jamiemcc at blueyonder dot co dot uk)
 *
 */
#ifndef _NOLPHIN_PLACES_SIDEBAR_H
#define _NOLPHIN_PLACES_SIDEBAR_H

#include "nolphin-window.h"

#include <gtk/gtk.h>

#define NOLPHIN_PLACES_SIDEBAR_ID    "places"

#define NOLPHIN_TYPE_PLACES_SIDEBAR nolphin_places_sidebar_get_type()
#define NOLPHIN_PLACES_SIDEBAR(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NOLPHIN_TYPE_PLACES_SIDEBAR, NolphinPlacesSidebar))
#define NOLPHIN_PLACES_SIDEBAR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NOLPHIN_TYPE_PLACES_SIDEBAR, NolphinPlacesSidebarClass))
#define NOLPHIN_IS_PLACES_SIDEBAR(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NOLPHIN_TYPE_PLACES_SIDEBAR))
#define NOLPHIN_IS_PLACES_SIDEBAR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NOLPHIN_TYPE_PLACES_SIDEBAR))
#define NOLPHIN_PLACES_SIDEBAR_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NOLPHIN_TYPE_PLACES_SIDEBAR, NolphinPlacesSidebarClass))


GType nolphin_places_sidebar_get_type (void);
GtkWidget * nolphin_places_sidebar_new (NolphinWindow *window);


#endif
