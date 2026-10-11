/*
 * nolphin-window-slot-dnd.c - Handle DnD for widgets acting as
 * NolphinWindowSlot proxies
 *
 * Copyright (C) 2000, 2001 Eazel, Inc.
 * Copyright (C) 2010, Red Hat, Inc.
 *
 * The Gnome Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 * The Gnome Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with the Gnome Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
 * Boston, MA 02110-1335, USA.
 *
 * Authors: Pavel Cisler <pavel@eazel.com>,
 * 	    Ettore Perazzoli <ettore@gnu.org>
 */

#ifndef __NOLPHIN_WINDOW_SLOT_DND_H__
#define __NOLPHIN_WINDOW_SLOT_DND_H__

#include <gio/gio.h>
#include <gtk/gtk.h>

#include <libnolphin-private/nolphin-dnd.h>

#include "nolphin-window-slot.h"

void nolphin_drag_slot_proxy_init (GtkWidget *widget,
                                    NolphinFile *target_file,
                                    NolphinWindowSlot *target_slot);

#endif /* __NOLPHIN_WINDOW_SLOT_DND_H__ */
