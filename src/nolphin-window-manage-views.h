/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/*
 *  Nolphin
 *
 *  Copyright (C) 1999, 2000 Red Hat, Inc.
 *  Copyright (C) 1999, 2000, 2001 Eazel, Inc.
 *
 *  Nolphin is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  Nolphin is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public
 *  License along with this program; if not, write to the Free
 *  Software Foundation, Inc., 51 Franklin Street, Suite 500, MA 02110-1335, USA.
 *
 *  Author: Darin Adler <darin@bentspoon.com>
 *
 */

#ifndef NOLPHIN_WINDOW_MANAGE_VIEWS_H
#define NOLPHIN_WINDOW_MANAGE_VIEWS_H

#include "nolphin-window.h"
#include "nolphin-window-pane.h"

void nolphin_window_manage_views_close_slot (NolphinWindowSlot *slot);


/* NolphinWindowInfo implementation: */
void nolphin_window_report_location_change   (NolphinWindow     *window);

#endif /* NOLPHIN_WINDOW_MANAGE_VIEWS_H */
