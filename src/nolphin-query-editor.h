/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
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
 * You should have received a copy of the GNU General Public
 * License along with this program; see the file COPYING.  If not,
 * write to the Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
 * Boston, MA 02110-1335, USA.
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 *
 */

#ifndef NOLPHIN_QUERY_EDITOR_H
#define NOLPHIN_QUERY_EDITOR_H

#include <gtk/gtk.h>

#include <libnolphin-private/nolphin-query.h>

G_BEGIN_DECLS

#define NOLPHIN_TYPE_QUERY_EDITOR (nolphin_query_editor_get_type ())

G_DECLARE_FINAL_TYPE (NolphinQueryEditor, nolphin_query_editor, NOLPHIN, QUERY_EDITOR, GtkBox)

GtkWidget* nolphin_query_editor_new                (void);

NolphinQuery   *nolphin_query_editor_get_query          (NolphinQueryEditor *editor);
void         nolphin_query_editor_set_query          (NolphinQueryEditor *editor,
                                                   NolphinQuery       *query);
GFile       *nolphin_query_editor_get_location       (NolphinQueryEditor *editor);
void         nolphin_query_editor_set_location       (NolphinQueryEditor *editor,
                                                   GFile           *location);
void         nolphin_query_editor_set_active         (NolphinQueryEditor *editor,
                                                   gchar           *base_uri,
                                                   gboolean         active);
gboolean     nolphin_query_editor_get_active         (NolphinQueryEditor *editor);
const gchar *nolphin_query_editor_get_base_uri       (NolphinQueryEditor *editor);

G_END_DECLS

#endif /* NOLPHIN_QUERY_EDITOR_H */
