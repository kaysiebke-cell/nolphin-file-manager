/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nolphin-saved-selections.h: named, per-folder saved selections
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

#ifndef NOLPHIN_SAVED_SELECTIONS_H
#define NOLPHIN_SAVED_SELECTIONS_H

#include <glib.h>

G_BEGIN_DECLS

/* Small local GKeyFile store under
 * $XDG_DATA_HOME/nolphin/saved-selections.ini - one group per folder
 * URI, one key per saved selection name within that folder, holding
 * the exact (non-display) filesystem basenames that were selected.
 * Deliberately synchronous: this is a tiny local text file, not a
 * large or remote operation (applies to slow work, not this). */

/* Remembers @basenames (a GList of gchar*, not modified/freed) as a
 * named selection for @folder_uri, overwriting any existing selection
 * of the same name in that folder. Returns FALSE and sets @error on
 * I/O failure. */
gboolean nolphin_saved_selections_save (const gchar  *folder_uri,
                                        const gchar  *name,
                                        GList        *basenames,
                                        GError      **error);

/* Returns a newly-allocated GList of newly-allocated gchar* basenames
 * for @name in @folder_uri, or NULL if no such saved selection
 * exists. Free with g_list_free_full (list, g_free). */
GList *nolphin_saved_selections_restore (const gchar *folder_uri, const gchar *name);

/* Newly-allocated GList of newly-allocated gchar* names of every
 * selection saved for @folder_uri (empty list, not NULL, if none). */
GList *nolphin_saved_selections_list_names (const gchar *folder_uri);

/* Removes one named selection. Returns TRUE if it existed and was
 * removed. */
gboolean nolphin_saved_selections_delete (const gchar *folder_uri, const gchar *name);

G_END_DECLS

#endif /* NOLPHIN_SAVED_SELECTIONS_H */
