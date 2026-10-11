/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nolphin-clipboard-monitor.h: lets you notice clipboard changes.
    
   Copyright (C) 2004 Red Hat, Inc.
  
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

#ifndef NOLPHIN_CLIPBOARD_MONITOR_H
#define NOLPHIN_CLIPBOARD_MONITOR_H

#include <gtk/gtk.h>

#define NOLPHIN_TYPE_CLIPBOARD_MONITOR nolphin_clipboard_monitor_get_type()
#define NOLPHIN_CLIPBOARD_MONITOR(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NOLPHIN_TYPE_CLIPBOARD_MONITOR, NolphinClipboardMonitor))
#define NOLPHIN_CLIPBOARD_MONITOR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NOLPHIN_TYPE_CLIPBOARD_MONITOR, NolphinClipboardMonitorClass))
#define NOLPHIN_IS_CLIPBOARD_MONITOR(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NOLPHIN_TYPE_CLIPBOARD_MONITOR))
#define NOLPHIN_IS_CLIPBOARD_MONITOR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NOLPHIN_TYPE_CLIPBOARD_MONITOR))
#define NOLPHIN_CLIPBOARD_MONITOR_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NOLPHIN_TYPE_CLIPBOARD_MONITOR, NolphinClipboardMonitorClass))

typedef struct NolphinClipboardMonitorDetails NolphinClipboardMonitorDetails;
typedef struct NolphinClipboardInfo NolphinClipboardInfo;

typedef struct {
	GObject parent_slot;

	NolphinClipboardMonitorDetails *details;
} NolphinClipboardMonitor;

typedef struct {
	GObjectClass parent_slot;
  
	void (* clipboard_changed) (NolphinClipboardMonitor *monitor);
	void (* clipboard_info) (NolphinClipboardMonitor *monitor,
	                         NolphinClipboardInfo *info);
} NolphinClipboardMonitorClass;

struct NolphinClipboardInfo {
	GList *files;
	gboolean cut;
};

GType   nolphin_clipboard_monitor_get_type (void);

NolphinClipboardMonitor *   nolphin_clipboard_monitor_get (void);
void nolphin_clipboard_monitor_set_clipboard_info (NolphinClipboardMonitor *monitor,
                                                    NolphinClipboardInfo *info);
NolphinClipboardInfo * nolphin_clipboard_monitor_get_clipboard_info (NolphinClipboardMonitor *monitor);
void nolphin_clipboard_monitor_emit_changed (void);

void nolphin_clear_clipboard_callback (GtkClipboard *clipboard,
                                        gpointer      user_data);
void nolphin_get_clipboard_callback   (GtkClipboard     *clipboard,
                                        GtkSelectionData *selection_data,
                                        guint             info,
                                        gpointer          user_data);



#endif /* NOLPHIN_CLIPBOARD_MONITOR_H */

