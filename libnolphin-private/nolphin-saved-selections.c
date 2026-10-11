/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nolphin-saved-selections.c: named, per-folder saved selections
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

#include <config.h>

#include "nolphin-saved-selections.h"

#include <glib/gstdio.h>
#include <string.h>

static gchar *
store_path (void)
{
    return g_build_filename (g_get_user_data_dir (), "nolphin", "saved-selections.ini", NULL);
}

static GKeyFile *
load_store (void)
{
    GKeyFile *key_file = g_key_file_new ();
    gchar *path = store_path ();

    /* A missing file just means "nothing saved yet" - start from an
     * empty, valid key file rather than treating that as an error. */
    g_key_file_load_from_file (key_file, path, G_KEY_FILE_NONE, NULL);

    g_free (path);
    return key_file;
}

static gboolean
save_store (GKeyFile *key_file, GError **error)
{
    gchar *path = store_path ();
    gchar *dir = g_path_get_dirname (path);
    gboolean success;

    g_mkdir_with_parents (dir, 0700);
    success = g_key_file_save_to_file (key_file, path, error);

    g_free (dir);
    g_free (path);
    return success;
}

gboolean
nolphin_saved_selections_save (const gchar  *folder_uri,
                               const gchar  *name,
                               GList        *basenames,
                               GError      **error)
{
    GKeyFile *key_file;
    gchar **values;
    GList *l;
    guint i, count;
    gboolean success;

    g_return_val_if_fail (folder_uri != NULL, FALSE);
    g_return_val_if_fail (name != NULL && name[0] != '\0', FALSE);

    count = g_list_length (basenames);
    values = g_new0 (gchar *, count + 1);
    for (l = basenames, i = 0; l != NULL; l = l->next, i++) {
        values[i] = (gchar *) l->data;
    }

    key_file = load_store ();
    g_key_file_set_string_list (key_file, folder_uri, name, (const gchar * const *) values, count);
    g_free (values); /* elements are borrowed from @basenames, not owned */

    success = save_store (key_file, error);
    g_key_file_free (key_file);

    return success;
}

GList *
nolphin_saved_selections_restore (const gchar *folder_uri, const gchar *name)
{
    GKeyFile *key_file;
    gchar **values;
    gsize count, i;
    GList *result = NULL;

    g_return_val_if_fail (folder_uri != NULL, NULL);
    g_return_val_if_fail (name != NULL, NULL);

    key_file = load_store ();
    values = g_key_file_get_string_list (key_file, folder_uri, name, &count, NULL);
    g_key_file_free (key_file);

    if (values == NULL) {
        return NULL;
    }

    for (i = 0; i < count; i++) {
        result = g_list_prepend (result, g_strdup (values[i]));
    }
    result = g_list_reverse (result);

    g_strfreev (values);
    return result;
}

GList *
nolphin_saved_selections_list_names (const gchar *folder_uri)
{
    GKeyFile *key_file;
    gchar **keys;
    gsize count, i;
    GList *result = NULL;

    g_return_val_if_fail (folder_uri != NULL, NULL);

    key_file = load_store ();
    keys = g_key_file_get_keys (key_file, folder_uri, &count, NULL);
    g_key_file_free (key_file);

    if (keys == NULL) {
        return NULL;
    }

    for (i = 0; i < count; i++) {
        result = g_list_prepend (result, g_strdup (keys[i]));
    }
    result = g_list_reverse (result);

    g_strfreev (keys);
    return result;
}

gboolean
nolphin_saved_selections_delete (const gchar *folder_uri, const gchar *name)
{
    GKeyFile *key_file;
    gboolean existed;

    g_return_val_if_fail (folder_uri != NULL, FALSE);
    g_return_val_if_fail (name != NULL, FALSE);

    key_file = load_store ();
    existed = g_key_file_has_key (key_file, folder_uri, name, NULL);

    if (existed) {
        g_key_file_remove_key (key_file, folder_uri, name, NULL);
        save_store (key_file, NULL);
    }

    g_key_file_free (key_file);
    return existed;
}
