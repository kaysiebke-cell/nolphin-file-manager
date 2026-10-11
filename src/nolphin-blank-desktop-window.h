/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nolphin
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
 */

/* nolphin-blank-desktop-window.h
 */

#ifndef NOLPHIN_BLANK_DESKTOP_WINDOW_H
#define NOLPHIN_BLANK_DESKTOP_WINDOW_H

#include <gtk/gtk.h>

#include <libnolphin-private/nolphin-action-manager.h>

#define NOLPHIN_TYPE_BLANK_DESKTOP_WINDOW nolphin_blank_desktop_window_get_type()
#define NOLPHIN_BLANK_DESKTOP_WINDOW(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NOLPHIN_TYPE_BLANK_DESKTOP_WINDOW, NolphinBlankDesktopWindow))
#define NOLPHIN_BLANK_DESKTOP_WINDOW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NOLPHIN_TYPE_BLANK_DESKTOP_WINDOW, NolphinBlankDesktopWindowClass))
#define NOLPHIN_IS_BLANK_DESKTOP_WINDOW(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NOLPHIN_TYPE_BLANK_DESKTOP_WINDOW))
#define NOLPHIN_IS_BLANK_DESKTOP_WINDOW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NOLPHIN_TYPE_BLANK_DESKTOP_WINDOW))
#define NOLPHIN_BLANK_DESKTOP_WINDOW_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NOLPHIN_TYPE_BLANK_DESKTOP_WINDOW, NolphinBlankDesktopWindowClass))

typedef struct NolphinBlankDesktopWindowDetails NolphinBlankDesktopWindowDetails;

typedef struct {
	GtkWindow parent_spot;
	NolphinBlankDesktopWindowDetails *details;
} NolphinBlankDesktopWindow;

typedef struct {
	GtkWindowClass parent_spot;

    void   (* plugin_manager)  (NolphinBlankDesktopWindow *window);
} NolphinBlankDesktopWindowClass;

GType                   nolphin_blank_desktop_window_get_type            (void);
NolphinBlankDesktopWindow *nolphin_blank_desktop_window_new                 (gint monitor);
NolphinActionManager      *nolphin_desktop_manager_get_action_manager       (void);
void                    nolphin_blank_desktop_window_update_geometry     (NolphinBlankDesktopWindow *window);

#endif /* NOLPHIN_BLANK_DESKTOP_WINDOW_H */
