/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* fm-icon-view.h - interface for icon view of directory.

   Copyright (C) 2000 Eazel, Inc.

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

   Authors: Mike Engber <engber@eazel.com>
*/

#ifndef NOLPHIN_DESKTOP_ICON_VIEW_H
#define NOLPHIN_DESKTOP_ICON_VIEW_H

#include "nolphin-icon-view.h"

#define NOLPHIN_TYPE_DESKTOP_ICON_VIEW nolphin_desktop_icon_view_get_type()
#define NOLPHIN_DESKTOP_ICON_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NOLPHIN_TYPE_DESKTOP_ICON_VIEW, NolphinDesktopIconView))
#define NOLPHIN_DESKTOP_ICON_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NOLPHIN_TYPE_DESKTOP_ICON_VIEW, NolphinDesktopIconViewClass))
#define NOLPHIN_IS_DESKTOP_ICON_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NOLPHIN_TYPE_DESKTOP_ICON_VIEW))
#define NOLPHIN_IS_DESKTOP_ICON_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NOLPHIN_TYPE_DESKTOP_ICON_VIEW))
#define NOLPHIN_DESKTOP_ICON_VIEW_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NOLPHIN_TYPE_DESKTOP_ICON_VIEW, NolphinDesktopIconViewClass))

#define NOLPHIN_DESKTOP_ICON_VIEW_ID "OAFIID:Nolphin_File_Manager_Desktop_Icon_View"

typedef struct NolphinDesktopIconViewDetails NolphinDesktopIconViewDetails;
typedef struct {
	NolphinIconView parent;
	NolphinDesktopIconViewDetails *details;
} NolphinDesktopIconView;

typedef struct {
	NolphinIconViewClass parent_class;
} NolphinDesktopIconViewClass;

/* GObject support */
GType   nolphin_desktop_icon_view_get_type (void);
void nolphin_desktop_icon_view_register (void);

#endif /* NOLPHIN_DESKTOP_ICON_VIEW_H */
