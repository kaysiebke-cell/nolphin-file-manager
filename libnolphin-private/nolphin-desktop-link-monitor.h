/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nolphin-desktop-link-monitor.h: singleton that manages the desktop links
    
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

#ifndef NOLPHIN_DESKTOP_LINK_MONITOR_H
#define NOLPHIN_DESKTOP_LINK_MONITOR_H

#include <gtk/gtk.h>
#include <libnolphin-private/nolphin-desktop-link.h>

#define NOLPHIN_TYPE_DESKTOP_LINK_MONITOR nolphin_desktop_link_monitor_get_type()
#define NOLPHIN_DESKTOP_LINK_MONITOR(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NOLPHIN_TYPE_DESKTOP_LINK_MONITOR, NolphinDesktopLinkMonitor))
#define NOLPHIN_DESKTOP_LINK_MONITOR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NOLPHIN_TYPE_DESKTOP_LINK_MONITOR, NolphinDesktopLinkMonitorClass))
#define NOLPHIN_IS_DESKTOP_LINK_MONITOR(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NOLPHIN_TYPE_DESKTOP_LINK_MONITOR))
#define NOLPHIN_IS_DESKTOP_LINK_MONITOR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NOLPHIN_TYPE_DESKTOP_LINK_MONITOR))
#define NOLPHIN_DESKTOP_LINK_MONITOR_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NOLPHIN_TYPE_DESKTOP_LINK_MONITOR, NolphinDesktopLinkMonitorClass))

typedef struct NolphinDesktopLinkMonitorDetails NolphinDesktopLinkMonitorDetails;

typedef struct {
	GObject parent_slot;
	NolphinDesktopLinkMonitorDetails *details;
} NolphinDesktopLinkMonitor;

typedef struct {
	GObjectClass parent_slot;
} NolphinDesktopLinkMonitorClass;

GType   nolphin_desktop_link_monitor_get_type (void);

NolphinDesktopLinkMonitor *   nolphin_desktop_link_monitor_get (void);
void nolphin_desktop_link_monitor_delete_link (NolphinDesktopLinkMonitor *monitor,
						NolphinDesktopLink *link,
						GtkWidget *parent_view);

/* Used by nolphin-desktop-link.c */
char * nolphin_desktop_link_monitor_make_filename_unique (NolphinDesktopLinkMonitor *monitor,
							   const char *filename);

#endif /* NOLPHIN_DESKTOP_LINK_MONITOR_H */
