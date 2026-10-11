/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nolphin-desktop-directory.h: Subclass of NolphinDirectory to implement
   a virtual directory consisting of the desktop directory and the desktop
   icons
 
   Copyright (C) 2003 Red Hat, Inc.
  
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
  
   Author: Alexander Larsson <alexl@redhat.com>
*/

#ifndef NOLPHIN_DESKTOP_DIRECTORY_H
#define NOLPHIN_DESKTOP_DIRECTORY_H

#include <libnolphin-private/nolphin-directory.h>

#define NOLPHIN_TYPE_DESKTOP_DIRECTORY nolphin_desktop_directory_get_type()
#define NOLPHIN_DESKTOP_DIRECTORY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NOLPHIN_TYPE_DESKTOP_DIRECTORY, NolphinDesktopDirectory))
#define NOLPHIN_DESKTOP_DIRECTORY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NOLPHIN_TYPE_DESKTOP_DIRECTORY, NolphinDesktopDirectoryClass))
#define NOLPHIN_IS_DESKTOP_DIRECTORY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NOLPHIN_TYPE_DESKTOP_DIRECTORY))
#define NOLPHIN_IS_DESKTOP_DIRECTORY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NOLPHIN_TYPE_DESKTOP_DIRECTORY))
#define NOLPHIN_DESKTOP_DIRECTORY_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NOLPHIN_TYPE_DESKTOP_DIRECTORY, NolphinDesktopDirectoryClass))

typedef struct NolphinDesktopDirectoryDetails NolphinDesktopDirectoryDetails;

typedef struct {
	NolphinDirectory parent_slot;
	NolphinDesktopDirectoryDetails *details;
    gint display_number;
} NolphinDesktopDirectory;

typedef struct {
	NolphinDirectoryClass parent_slot;

} NolphinDesktopDirectoryClass;

GType   nolphin_desktop_directory_get_type             (void);
NolphinDirectory * nolphin_desktop_directory_get_real_directory   (NolphinDesktopDirectory *desktop_directory);

#endif /* NOLPHIN_DESKTOP_DIRECTORY_H */
