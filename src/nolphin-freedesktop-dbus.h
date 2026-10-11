/*
 * nolphin-freedesktop-dbus: Implementation for the org.freedesktop DBus file-management interfaces
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
 * Authors: Akshay Gupta <kitallis@gmail.com>
 *          Federico Mena Quintero <federico@gnome.org>
 */


#ifndef __NOLPHIN_FREEDESKTOP_DBUS_H__
#define __NOLPHIN_FREEDESKTOP_DBUS_H__

#include <glib-object.h>

#define NOLPHIN_TYPE_FREEDESKTOP_DBUS nolphin_freedesktop_dbus_get_type()
#define NOLPHIN_FREEDESKTOP_DBUS(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NOLPHIN_TYPE_FREEDESKTOP_DBUS, NolphinFreedesktopDBus))
#define NOLPHIN_FREEDESKTOP_DBUS_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NOLPHIN_TYPE_FREEDESKTOP_DBUS, NolphinFreedesktopDBusClass))
#define NOLPHIN_IS_FREEDESKTOP_DBUS(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NOLPHIN_TYPE_FREEDESKTOP_DBUS))
#define NOLPHIN_IS_FREEDESKTOP_DBUS_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NOLPHIN_TYPE_FREEDESKTOP_DBUS))
#define NOLPHIN_FREEDESKTOP_DBUS_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NOLPHIN_TYPE_FREEDESKTOP_DBUS, NolphinFreedesktopDBusClass))

typedef struct _NolphinFreedesktopDBus NolphinFreedesktopDBus;
typedef struct _NolphinFreedesktopDBusClass NolphinFreedesktopDBusClass;

GType nolphin_freedesktop_dbus_get_type (void);
NolphinFreedesktopDBus * nolphin_freedesktop_dbus_new (void);

void nolphin_freedesktop_dbus_set_open_locations (NolphinFreedesktopDBus *fdb, const gchar **locations);

#endif /* __NOLPHIN_FREEDESKTOP_DBUS_H__ */
