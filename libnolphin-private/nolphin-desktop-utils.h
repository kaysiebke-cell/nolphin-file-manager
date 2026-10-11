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

#ifndef NOLPHIN_DESKTOP_UTILS_H
#define NOLPHIN_DESKTOP_UTILS_H

#include <gdk/gdk.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

void nolphin_desktop_utils_get_monitor_work_rect (gint num, GdkRectangle *rect);
void nolphin_desktop_utils_get_monitor_geometry (gint num, GdkRectangle *rect);
gint nolphin_desktop_utils_get_primary_monitor (void);
gint nolphin_desktop_utils_get_monitor_for_widget (GtkWidget *widget);
gint nolphin_desktop_utils_get_num_monitors (void);
gboolean nolphin_desktop_utils_get_monitor_cloned (gint monitor, gint x_primary);
gint nolphin_desktop_utils_get_scale_factor (void);

gboolean nolphin_desktop_utils_configure_layer_shell (GtkWindow *window,
                                                   gint       monitor_num,
                                                   gboolean   keyboard_on_demand);

G_END_DECLS

#endif
