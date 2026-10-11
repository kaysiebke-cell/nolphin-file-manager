/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nolphin-desktop-link.h: Class that handles the links on the desktop
    
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

#ifndef NOLPHIN_DESKTOP_LINK_H
#define NOLPHIN_DESKTOP_LINK_H

#include <libnolphin-private/nolphin-file.h>
#include <gio/gio.h>

#define NOLPHIN_TYPE_DESKTOP_LINK nolphin_desktop_link_get_type()
#define NOLPHIN_DESKTOP_LINK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NOLPHIN_TYPE_DESKTOP_LINK, NolphinDesktopLink))
#define NOLPHIN_DESKTOP_LINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NOLPHIN_TYPE_DESKTOP_LINK, NolphinDesktopLinkClass))
#define NOLPHIN_IS_DESKTOP_LINK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NOLPHIN_TYPE_DESKTOP_LINK))
#define NOLPHIN_IS_DESKTOP_LINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NOLPHIN_TYPE_DESKTOP_LINK))
#define NOLPHIN_DESKTOP_LINK_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NOLPHIN_TYPE_DESKTOP_LINK, NolphinDesktopLinkClass))

typedef struct NolphinDesktopLinkDetails NolphinDesktopLinkDetails;

typedef struct {
	GObject parent_slot;
	NolphinDesktopLinkDetails *details;
} NolphinDesktopLink;

typedef struct {
	GObjectClass parent_slot;
} NolphinDesktopLinkClass;

typedef enum {
	NOLPHIN_DESKTOP_LINK_HOME,
	NOLPHIN_DESKTOP_LINK_COMPUTER,
	NOLPHIN_DESKTOP_LINK_TRASH,
	NOLPHIN_DESKTOP_LINK_MOUNT,
	NOLPHIN_DESKTOP_LINK_NETWORK
} NolphinDesktopLinkType;

GType   nolphin_desktop_link_get_type (void);

NolphinDesktopLink *   nolphin_desktop_link_new                     (NolphinDesktopLinkType  type);
NolphinDesktopLink *   nolphin_desktop_link_new_from_mount          (GMount                 *mount);
NolphinDesktopLinkType nolphin_desktop_link_get_link_type           (NolphinDesktopLink     *link);
NolphinFile *          nolphin_desktop_link_get_file                (NolphinDesktopLink *link);
char *                  nolphin_desktop_link_get_file_name           (NolphinDesktopLink     *link);
char *                  nolphin_desktop_link_get_display_name        (NolphinDesktopLink     *link);
GIcon *                 nolphin_desktop_link_get_icon                (NolphinDesktopLink     *link);
GFile *                 nolphin_desktop_link_get_activation_location (NolphinDesktopLink     *link);
char *                  nolphin_desktop_link_get_activation_uri      (NolphinDesktopLink     *link);
gboolean                nolphin_desktop_link_get_date                (NolphinDesktopLink     *link,
								       NolphinDateType         date_type,
								       time_t                  *date);
GMount *                nolphin_desktop_link_get_mount               (NolphinDesktopLink     *link);
gboolean                nolphin_desktop_link_can_rename              (NolphinDesktopLink     *link);
gboolean                nolphin_desktop_link_rename                  (NolphinDesktopLink     *link,
								       const char              *name);


#endif /* NOLPHIN_DESKTOP_LINK_H */
