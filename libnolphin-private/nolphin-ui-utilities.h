/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nolphin-ui-utilities.h - helper functions for GtkUIManager stuff

   Copyright (C) 2004 Red Hat, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
   Boston, MA 02110-1335, USA.

   Authors: Alexander Larsson <alexl@redhat.com>
*/
#ifndef NOLPHIN_UI_UTILITIES_H
#define NOLPHIN_UI_UTILITIES_H

#include <gtk/gtk.h>
#include <libnolphin-extension/nolphin-menu-item.h>

void        nolphin_ui_unmerge_ui                 (GtkUIManager      *ui_manager,
						    guint             *merge_id,
						    GtkActionGroup   **action_group);
void        nolphin_ui_prepare_merge_ui           (GtkUIManager      *ui_manager,
						    const char        *name,
						    guint             *merge_id,
						    GtkActionGroup   **action_group);
GtkAction * nolphin_action_from_menu_item         (NolphinMenuItem  *item,
                                                GtkWidget     *parent_widget);

GdkPixbuf * nolphin_ui_get_menu_icon              (const char        *icon_name,
                                                GtkWidget         *parent_widget);
gchar     * nolphin_make_action_uuid_for_path     (const gchar *path);
#endif /* NOLPHIN_UI_UTILITIES_H */
