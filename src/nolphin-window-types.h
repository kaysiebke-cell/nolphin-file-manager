/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

/*
 *  nolphin-window-types: typedefs for window-related types.
 *
 *  Copyright (C) 1999, 2000, 2010 Red Hat, Inc.
 *  Copyright (C) 1999, 2000, 2001 Eazel, Inc.
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
 *  Authors: Elliot Lee <sopwith@redhat.com>
 *           Darin Adler <darin@bentspoon.com>
 *
 */

#ifndef __NOLPHIN_WINDOW_TYPES_H__
#define __NOLPHIN_WINDOW_TYPES_H__

typedef struct _NolphinWindowPane NolphinWindowPane;
typedef struct _NolphinWindowPaneClass NolphinWindowPaneClass;

typedef struct NolphinWindow NolphinWindow;

typedef struct NolphinWindowSlot NolphinWindowSlot;
typedef struct NolphinWindowSlotClass NolphinWindowSlotClass;

typedef void (* NolphinWindowGoToCallback) (NolphinWindow *window,
                                             GError *error,
                                             gpointer user_data);

typedef enum {
        NOLPHIN_WINDOW_OPEN_FLAG_CLOSE_BEHIND = 1<<0,
        NOLPHIN_WINDOW_OPEN_FLAG_NEW_WINDOW = 1<<1,
        NOLPHIN_WINDOW_OPEN_FLAG_NEW_TAB = 1<<2,
        NOLPHIN_WINDOW_OPEN_FLAG_SEARCH = 1<<3,
        NOLPHIN_WINDOW_OPEN_FLAG_MOUNT = 1<<4
} NolphinWindowOpenFlags;

#endif /* __NOLPHIN_WINDOW_TYPES_H__ */
