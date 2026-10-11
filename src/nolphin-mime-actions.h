/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nolphin-mime-actions.h - uri-specific versions of mime action functions

   Copyright (C) 2000 Eazel, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
   Boston, MA 02110-1335, USA.

   Authors: Maciej Stachowiak <mjs@eazel.com>
*/

#ifndef NOLPHIN_MIME_ACTIONS_H
#define NOLPHIN_MIME_ACTIONS_H

#include <gio/gio.h>

#include <libnolphin-private/nolphin-file.h>

#include "nolphin-window.h"

NolphinFileAttributes nolphin_mime_actions_get_required_file_attributes (void);

GAppInfo *             nolphin_mime_get_default_application_for_file     (NolphinFile            *file);
GList *                nolphin_mime_get_applications_for_file            (NolphinFile            *file);

GAppInfo *             nolphin_mime_get_default_application_for_files    (GList                   *files);
GList *                nolphin_mime_get_applications_for_files           (GList                   *file);

gboolean               nolphin_mime_file_opens_in_view                   (NolphinFile            *file);
gboolean               nolphin_mime_file_opens_in_external_app           (NolphinFile            *file);
void                   nolphin_mime_activate_files                       (GtkWindow               *parent_window,
									   NolphinWindowSlot      *slot,
									   GList                   *files,
									   const char              *launch_directory,
									   NolphinWindowOpenFlags  flags,
									   gboolean                 user_confirmation);
void                   nolphin_mime_activate_file                        (GtkWindow               *parent_window,
									   NolphinWindowSlot      *slot_info,
									   NolphinFile            *file,
									   const char              *launch_directory,
									   NolphinWindowOpenFlags  flags);
void                   nolphin_mime_launch_fm_and_select_file            (GFile *file);

#endif /* NOLPHIN_MIME_ACTIONS_H */
