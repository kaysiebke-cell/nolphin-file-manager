/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

/*
 *  Nolphin
 *
 *  Copyright (C) 2004 Red Hat, Inc.
 *  Copyright (C) 2003 Marco Pesenti Gritti
 *
 *  Nolphin is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  Nolphin is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Suite 500, MA 02110-1335, USA.
 *
 *
 *  Based on ephy-navigation-action.h from Epiphany
 *
 *  Authors: Alexander Larsson <alexl@redhat.com>
 *           Marco Pesenti Gritti
 *
 */

#ifndef NOLPHIN_NAVIGATION_ACTION_H
#define NOLPHIN_NAVIGATION_ACTION_H

#include <gtk/gtk.h>

#define NOLPHIN_TYPE_NAVIGATION_ACTION            (nolphin_navigation_action_get_type ())
#define NOLPHIN_NAVIGATION_ACTION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), NOLPHIN_TYPE_NAVIGATION_ACTION, NolphinNavigationAction))
#define NOLPHIN_NAVIGATION_ACTION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), NOLPHIN_TYPE_NAVIGATION_ACTION, NolphinNavigationActionClass))
#define NOLPHIN_IS_NAVIGATION_ACTION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NOLPHIN_TYPE_NAVIGATION_ACTION))
#define NOLPHIN_IS_NAVIGATION_ACTION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), NOLPHIN_TYPE_NAVIGATION_ACTION))
#define NOLPHIN_NAVIGATION_ACTION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), NOLPHIN_TYPE_NAVIGATION_ACTION, NolphinNavigationActionClass))

typedef struct _NolphinNavigationAction       NolphinNavigationAction;
typedef struct _NolphinNavigationActionClass  NolphinNavigationActionClass;
typedef struct NolphinNavigationActionPrivate NolphinNavigationActionPrivate;

typedef enum
{
    NOLPHIN_NAVIGATION_DIRECTION_BACK,
    NOLPHIN_NAVIGATION_DIRECTION_FORWARD,
    NOLPHIN_NAVIGATION_DIRECTION_UP,
    NOLPHIN_NAVIGATION_DIRECTION_RELOAD,
    NOLPHIN_NAVIGATION_DIRECTION_HOME,
    NOLPHIN_NAVIGATION_DIRECTION_COMPUTER,
    NOLPHIN_NAVIGATION_DIRECTION_EDIT,

} NolphinNavigationDirection;

struct _NolphinNavigationAction
{
	GtkAction parent;
	
	/*< private >*/
	NolphinNavigationActionPrivate *priv;
};

struct _NolphinNavigationActionClass
{
	GtkActionClass parent_class;
};

GType    nolphin_navigation_action_get_type   (void);

#endif
