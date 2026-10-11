/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nolphin
 *
 * Copyright (C) 2005 Red Hat, Inc.
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
 * Author:  Alexander Larsson <alexl@redhat.com>
 */

#ifndef NOLPHIN_WINDOW_BOOKMARKS_H
#define NOLPHIN_WINDOW_BOOKMARKS_H

#include <libnolphin-private/nolphin-bookmark.h>
#include <nolphin-window.h>
#include "nolphin-bookmark-list.h"

void                  nolphin_bookmarks_exiting                        (void);
void                  nolphin_window_add_bookmark_for_current_location (NolphinWindow *window);
void                  nolphin_window_edit_bookmarks                    (NolphinWindow *window);
void                  nolphin_window_initialize_bookmarks_menu         (NolphinWindow *window);

#endif
