/*
 * nolphin-dbus-manager: nolphin DBus interface
 *
 * Copyright (C) 2010, Red Hat, Inc.
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
 * Author: Cosimo Cecchi <cosimoc@redhat.com>
 *
 */

#ifndef __NOLPHIN_DBUS_MANAGER_H__
#define __NOLPHIN_DBUS_MANAGER_H__

#include <glib-object.h>
#include <gio/gio.h>

typedef struct _NolphinDBusManager NolphinDBusManager;
typedef struct _NolphinDBusManagerClass NolphinDBusManagerClass;

GType nolphin_dbus_manager_get_type (void);
NolphinDBusManager * nolphin_dbus_manager_new (void);

#endif /* __NOLPHIN_DBUS_MANAGER_H__ */
