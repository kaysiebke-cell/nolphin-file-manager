/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nolphin-acl.c: POSIX ACL viewing/editing via getfacl/setfacl
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

#include "nolphin-acl.h"

#include <glib/gi18n.h>
#include <string.h>

GQuark
nolphin_acl_error_quark (void)
{
    return g_quark_from_static_string ("nolphin-acl-error-quark");
}

void
nolphin_acl_entry_free (NolphinAclEntry *entry)
{
    if (entry == NULL) {
        return;
    }
    g_free (entry->qualifier);
    g_free (entry);
}

void
nolphin_acl_entry_list_free (GList *entries)
{
    g_list_free_full (entries, (GDestroyNotify) nolphin_acl_entry_free);
}

gboolean
nolphin_acl_is_available (void)
{
    gchar *getfacl_path = g_find_program_in_path ("getfacl");
    gchar *setfacl_path = g_find_program_in_path ("setfacl");
    gboolean available = (getfacl_path != NULL && setfacl_path != NULL);

    g_free (getfacl_path);
    g_free (setfacl_path);
    return available;
}

static const gchar *
type_keyword (NolphinAclEntryType type)
{
    switch (type) {
    case NOLPHIN_ACL_ENTRY_USER_OWNER:
    case NOLPHIN_ACL_ENTRY_USER:
        return "u";
    case NOLPHIN_ACL_ENTRY_GROUP_OWNER:
    case NOLPHIN_ACL_ENTRY_GROUP:
        return "g";
    case NOLPHIN_ACL_ENTRY_MASK:
        return "m";
    case NOLPHIN_ACL_ENTRY_OTHER:
        return "o";
    default:
        g_return_val_if_reached ("u");
    }
}

/* --- get entries --------------------------------------------------- */

static void
get_entries_communicate_cb (GObject *source, GAsyncResult *result, gpointer user_data)
{
    GTask *task = user_data;
    GSubprocess *subprocess = G_SUBPROCESS (source);
    GError *error = NULL;
    gchar *stdout_buf = NULL;
    gchar *stderr_buf = NULL;
    GList *entries = NULL;
    gchar **lines;
    guint i;

    if (!g_subprocess_communicate_utf8_finish (subprocess, result, &stdout_buf, &stderr_buf, &error)) {
        g_task_return_error (task, error);
        g_object_unref (task);
        g_free (stdout_buf);
        g_free (stderr_buf);
        return;
    }

    if (!g_subprocess_get_successful (subprocess)) {
        gchar *detail = (stderr_buf != NULL) ? g_strstrip (g_strdup (stderr_buf)) : NULL;
        g_task_return_new_error (task, NOLPHIN_ACL_ERROR, NOLPHIN_ACL_ERROR_TOOL_FAILED,
                                 "%s%s%s",
                                 _("getfacl wurde mit einem Fehler beendet."),
                                 (detail != NULL && detail[0] != '\0') ? "\n" : "",
                                 (detail != NULL) ? detail : "");
        g_free (detail);
        g_object_unref (task);
        g_free (stdout_buf);
        g_free (stderr_buf);
        return;
    }

    lines = g_strsplit (stdout_buf != NULL ? stdout_buf : "", "\n", -1);
    for (i = 0; lines[i] != NULL; i++) {
        gchar *line = g_strstrip (lines[i]);
        gchar **parts;
        NolphinAclEntry *entry;
        const gchar *perms;

        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }

        /* "<kind>:[qualifier]:rwx" - exactly two colons, qualifier
         * may be empty (base owner/group-owner/mask/other). */
        parts = g_strsplit (line, ":", 3);
        if (parts[0] == NULL || parts[1] == NULL || parts[2] == NULL) {
            g_strfreev (parts);
            continue;
        }

        entry = g_new0 (NolphinAclEntry, 1);
        if (strcmp (parts[0], "user") == 0) {
            entry->type = (parts[1][0] != '\0') ? NOLPHIN_ACL_ENTRY_USER : NOLPHIN_ACL_ENTRY_USER_OWNER;
        } else if (strcmp (parts[0], "group") == 0) {
            entry->type = (parts[1][0] != '\0') ? NOLPHIN_ACL_ENTRY_GROUP : NOLPHIN_ACL_ENTRY_GROUP_OWNER;
        } else if (strcmp (parts[0], "mask") == 0) {
            entry->type = NOLPHIN_ACL_ENTRY_MASK;
        } else if (strcmp (parts[0], "other") == 0) {
            entry->type = NOLPHIN_ACL_ENTRY_OTHER;
        } else {
            /* "default:..." (default ACL on a directory) or anything
             * else not modelled here yet - skip rather than guess. */
            g_strfreev (parts);
            nolphin_acl_entry_free (entry);
            continue;
        }

        entry->qualifier = (parts[1][0] != '\0') ? g_strdup (parts[1]) : NULL;

        perms = parts[2];
        entry->can_read    = (strchr (perms, 'r') != NULL);
        entry->can_write   = (strchr (perms, 'w') != NULL);
        entry->can_execute = (strchr (perms, 'x') != NULL);

        g_strfreev (parts);
        entries = g_list_append (entries, entry);
    }
    g_strfreev (lines);

    g_task_return_pointer (task, entries, (GDestroyNotify) nolphin_acl_entry_list_free);
    g_object_unref (task);
    g_free (stdout_buf);
    g_free (stderr_buf);
}

void
nolphin_acl_get_entries_async (GFile                *file,
                               GCancellable         *cancellable,
                               GAsyncReadyCallback   callback,
                               gpointer              user_data)
{
    GTask *task;
    gchar *tool_path, *path;
    GSubprocessLauncher *launcher;
    GSubprocess *subprocess;
    GError *error = NULL;

    g_return_if_fail (G_IS_FILE (file));

    task = g_task_new (NULL, cancellable, callback, user_data);
    g_task_set_source_tag (task, nolphin_acl_get_entries_async);

    tool_path = g_find_program_in_path ("getfacl");
    if (tool_path == NULL) {
        g_task_return_new_error (task, NOLPHIN_ACL_ERROR, NOLPHIN_ACL_ERROR_TOOL_NOT_FOUND,
                                 _("Das Werkzeug »getfacl« ist nicht installiert."));
        g_object_unref (task);
        return;
    }

    path = g_file_get_path (file);
    if (path == NULL) {
        g_free (tool_path);
        g_task_return_new_error (task, NOLPHIN_ACL_ERROR, NOLPHIN_ACL_ERROR_REMOTE_FILE,
                                 _("ACLs werden für entfernte Dateien noch nicht unterstützt."));
        g_object_unref (task);
        return;
    }

    launcher = g_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE |
                                          G_SUBPROCESS_FLAGS_STDERR_PIPE);
    subprocess = g_subprocess_launcher_spawn (launcher, &error, tool_path,
                                              "-p", "--omit-header", path, NULL);
    g_object_unref (launcher);
    g_free (tool_path);
    g_free (path);

    if (subprocess == NULL) {
        g_task_return_error (task, error);
        g_object_unref (task);
        return;
    }

    g_subprocess_communicate_utf8_async (subprocess, NULL, cancellable,
                                         get_entries_communicate_cb, task);
    g_object_unref (subprocess);
}

GList *
nolphin_acl_get_entries_finish (GAsyncResult *result, GError **error)
{
    return g_task_propagate_pointer (G_TASK (result), error);
}

/* --- set / remove ---------------------------------------------------- */

static void
setfacl_communicate_cb (GObject *source, GAsyncResult *result, gpointer user_data)
{
    GTask *task = user_data;
    GSubprocess *subprocess = G_SUBPROCESS (source);
    GError *error = NULL;
    gchar *stderr_buf = NULL;

    if (!g_subprocess_communicate_utf8_finish (subprocess, result, NULL, &stderr_buf, &error)) {
        g_task_return_error (task, error);
        g_object_unref (task);
        g_free (stderr_buf);
        return;
    }

    if (!g_subprocess_get_successful (subprocess)) {
        gchar *detail = (stderr_buf != NULL) ? g_strstrip (g_strdup (stderr_buf)) : NULL;
        g_task_return_new_error (task, NOLPHIN_ACL_ERROR, NOLPHIN_ACL_ERROR_TOOL_FAILED,
                                 "%s%s%s",
                                 _("setfacl wurde mit einem Fehler beendet."),
                                 (detail != NULL && detail[0] != '\0') ? "\n" : "",
                                 (detail != NULL) ? detail : "");
        g_free (detail);
        g_object_unref (task);
        g_free (stderr_buf);
        return;
    }

    g_task_return_boolean (task, TRUE);
    g_object_unref (task);
    g_free (stderr_buf);
}

static void
run_setfacl_async (GFile *file, const gchar *option, const gchar *spec,
                   gboolean recursive, GCancellable *cancellable, GTask *task)
{
    gchar *tool_path, *path;
    GSubprocessLauncher *launcher;
    GSubprocess *subprocess;
    GError *error = NULL;
    GPtrArray *argv;

    tool_path = g_find_program_in_path ("setfacl");
    if (tool_path == NULL) {
        g_task_return_new_error (task, NOLPHIN_ACL_ERROR, NOLPHIN_ACL_ERROR_TOOL_NOT_FOUND,
                                 _("Das Werkzeug »setfacl« ist nicht installiert."));
        g_object_unref (task);
        return;
    }

    path = g_file_get_path (file);
    if (path == NULL) {
        g_free (tool_path);
        g_task_return_new_error (task, NOLPHIN_ACL_ERROR, NOLPHIN_ACL_ERROR_REMOTE_FILE,
                                 _("ACLs werden für entfernte Dateien noch nicht unterstützt."));
        g_object_unref (task);
        return;
    }

    argv = g_ptr_array_new_with_free_func (g_free);
    g_ptr_array_add (argv, tool_path);
    if (recursive) {
        g_ptr_array_add (argv, g_strdup ("-R"));
    }
    g_ptr_array_add (argv, g_strdup (option));
    g_ptr_array_add (argv, g_strdup (spec));
    g_ptr_array_add (argv, path);
    g_ptr_array_add (argv, NULL);

    launcher = g_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDERR_PIPE |
                                          G_SUBPROCESS_FLAGS_STDOUT_SILENCE);
    subprocess = g_subprocess_launcher_spawnv (launcher, (const gchar * const *) argv->pdata, &error);
    g_object_unref (launcher);
    g_ptr_array_free (argv, TRUE);

    if (subprocess == NULL) {
        g_task_return_error (task, error);
        g_object_unref (task);
        return;
    }

    g_subprocess_communicate_utf8_async (subprocess, NULL, cancellable,
                                         setfacl_communicate_cb, task);
    g_object_unref (subprocess);
}

void
nolphin_acl_set_entry_async (GFile                *file,
                             NolphinAclEntryType   type,
                             const gchar          *qualifier,
                             gboolean              can_read,
                             gboolean              can_write,
                             gboolean              can_execute,
                             gboolean              recursive,
                             GCancellable         *cancellable,
                             GAsyncReadyCallback   callback,
                             gpointer              user_data)
{
    GTask *task;
    gchar *spec;

    g_return_if_fail (G_IS_FILE (file));

    task = g_task_new (NULL, cancellable, callback, user_data);
    g_task_set_source_tag (task, nolphin_acl_set_entry_async);

    spec = g_strdup_printf ("%s:%s:%s%s%s",
                            type_keyword (type),
                            (qualifier != NULL) ? qualifier : "",
                            can_read ? "r" : "-",
                            can_write ? "w" : "-",
                            can_execute ? "x" : "-");

    run_setfacl_async (file, "-m", spec, recursive, cancellable, task);
    g_free (spec);
}

gboolean
nolphin_acl_set_entry_finish (GAsyncResult *result, GError **error)
{
    return g_task_propagate_boolean (G_TASK (result), error);
}

void
nolphin_acl_remove_entry_async (GFile                *file,
                                NolphinAclEntryType   type,
                                const gchar          *qualifier,
                                gboolean              recursive,
                                GCancellable         *cancellable,
                                GAsyncReadyCallback   callback,
                                gpointer              user_data)
{
    GTask *task;
    gchar *spec;

    g_return_if_fail (G_IS_FILE (file));
    g_return_if_fail (type == NOLPHIN_ACL_ENTRY_USER || type == NOLPHIN_ACL_ENTRY_GROUP);
    g_return_if_fail (qualifier != NULL && qualifier[0] != '\0');

    task = g_task_new (NULL, cancellable, callback, user_data);
    g_task_set_source_tag (task, nolphin_acl_remove_entry_async);

    spec = g_strdup_printf ("%s:%s", type_keyword (type), qualifier);

    run_setfacl_async (file, "-x", spec, recursive, cancellable, task);
    g_free (spec);
}

gboolean
nolphin_acl_remove_entry_finish (GAsyncResult *result, GError **error)
{
    return g_task_propagate_boolean (G_TASK (result), error);
}
