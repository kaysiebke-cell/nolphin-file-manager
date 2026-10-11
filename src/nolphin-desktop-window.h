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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Suite 500, MA 02110-1335, USA.
 *
 * Authors: Darin Adler <darin@bentspoon.com>
 */

/* nolphin-desktop-window.h
 */

#ifndef NOLPHIN_DESKTOP_WINDOW_H
#define NOLPHIN_DESKTOP_WINDOW_H

#include "nolphin-window.h"

#define NOLPHIN_TYPE_DESKTOP_WINDOW nolphin_desktop_window_get_type()
#define NOLPHIN_DESKTOP_WINDOW(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NOLPHIN_TYPE_DESKTOP_WINDOW, NolphinDesktopWindow))
#define NOLPHIN_DESKTOP_WINDOW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NOLPHIN_TYPE_DESKTOP_WINDOW, NolphinDesktopWindowClass))
#define NOLPHIN_IS_DESKTOP_WINDOW(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NOLPHIN_TYPE_DESKTOP_WINDOW))
#define NOLPHIN_IS_DESKTOP_WINDOW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NOLPHIN_TYPE_DESKTOP_WINDOW))
#define NOLPHIN_DESKTOP_WINDOW_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NOLPHIN_TYPE_DESKTOP_WINDOW, NolphinDesktopWindowClass))

typedef struct NolphinDesktopWindowDetails NolphinDesktopWindowDetails;

typedef struct {
	NolphinWindow parent_spot;
	NolphinDesktopWindowDetails *details;
} NolphinDesktopWindow;

typedef struct {
	NolphinWindowClass parent_spot;
} NolphinDesktopWindowClass;

GType                  nolphin_desktop_window_get_type            (void);
NolphinDesktopWindow     *nolphin_desktop_window_new                 (gint monitor);
gboolean               nolphin_desktop_window_loaded              (NolphinDesktopWindow *window);
gint                   nolphin_desktop_window_get_monitor         (NolphinDesktopWindow *window);
void                   nolphin_desktop_window_update_geometry     (NolphinDesktopWindow *window);
gboolean               nolphin_desktop_window_get_grid_adjusts    (NolphinDesktopWindow *window,
                                                                gint              *h_adjust,
                                                                gint              *v_adjust);
gboolean               nolphin_desktop_window_set_grid_adjusts    (NolphinDesktopWindow *window,
                                                                gint               h_adjust,
                                                                gint               v_adjust);
GtkActionGroup *       nolphin_desktop_window_get_action_group (NolphinDesktopWindow *window);
#endif /* NOLPHIN_DESKTOP_WINDOW_H */
