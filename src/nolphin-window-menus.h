/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nolphin-window-pane.h: Nolphin window pane

   Copyright (C) 2008 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
   Boston, MA 02110-1335, USA.

   Author: Holger Berndt <berndth@gmx.de>
*/

#ifndef NOLPHIN_WINDOW_MENUS_H
#define NOLPHIN_WINDOW_MENUS_H

#include "nolphin-window.h"
#include "nolphin-window-pane.h"
#include <gtk/gtk.h>

guint           action_for_view_id  (const char *view_id                           );
void            toolbar_set_view_button     (guint action_id,      NolphinWindow *window);
void            toolbar_set_show_thumbnails_button (gboolean value, NolphinWindowPane *pane);
void            menu_set_show_thumbnails_action (gboolean value, NolphinWindow *window);
void            toolbar_set_create_folder_button (gboolean value, NolphinWindowPane *pane);
void            menu_set_view_selection (guint       action_id,
                                         NolphinWindow *window);
#endif /* NOLPHIN_WINDOW_PANE_H */
