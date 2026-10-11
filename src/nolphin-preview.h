/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nolphin-preview.h: right-hand info/preview panel (F11)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
 * Boston, MA 02110-1335, USA.
 */

#ifndef NOLPHIN_PREVIEW_H
#define NOLPHIN_PREVIEW_H

#include <gtk/gtk.h>
#include <libnolphin-private/nolphin-file.h>

G_BEGIN_DECLS

#define NOLPHIN_TYPE_PREVIEW (nolphin_preview_get_type ())
G_DECLARE_FINAL_TYPE (NolphinPreview, nolphin_preview, NOLPHIN, PREVIEW, GtkBox)

GtkWidget *nolphin_preview_new (void);

/* @selection: the current selection (list of NolphinFile*), may be NULL/empty.
 * @directory_as_file: the folder currently being viewed, shown as a fallback
 * subject when @selection is empty (may be NULL, e.g. no active view). */
void nolphin_preview_set_selection (NolphinPreview *preview,
                                     GList          *selection,
                                     NolphinFile    *directory_as_file);

void nolphin_preview_clear (NolphinPreview *preview);

G_END_DECLS

#endif /* NOLPHIN_PREVIEW_H */
