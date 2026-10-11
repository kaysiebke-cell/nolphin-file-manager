/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nolphin-encryption.c: gpg-based file encryption
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

#include "nolphin-encryption.h"

#include <glib/gi18n.h>
#include <string.h>

GQuark
nolphin_encryption_error_quark (void)
{
    return g_quark_from_static_string ("nolphin-encryption-error-quark");
}

gboolean
nolphin_encryption_is_available (void)
{
    gchar *path = g_find_program_in_path ("gpg");
    gboolean available = (path != NULL);
    g_free (path);
    return available;
}

static void
communicate_ready_cb (GObject *source, GAsyncResult *result, gpointer user_data)
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
        g_task_return_new_error (task, NOLPHIN_ENCRYPTION_ERROR, NOLPHIN_ENCRYPTION_ERROR_TOOL_FAILED,
                                 "%s%s%s",
                                 _("gpg wurde mit einem Fehler beendet."),
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
run_gpg_async (const gchar   *mode_args[],
              guint          n_mode_args,
              GFile         *source,
              GFile         *destination,
              const gchar   *passphrase,
              GCancellable  *cancellable,
              GTask         *task)
{
    gchar *tool_path, *source_path, *dest_path;
    GPtrArray *argv;
    GSubprocessLauncher *launcher;
    GSubprocess *subprocess;
    GError *error = NULL;
    guint i;

    tool_path = g_find_program_in_path ("gpg");
    if (tool_path == NULL) {
        g_task_return_new_error (task, NOLPHIN_ENCRYPTION_ERROR, NOLPHIN_ENCRYPTION_ERROR_TOOL_NOT_FOUND,
                                 _("Das Werkzeug »gpg« ist nicht installiert."));
        g_object_unref (task);
        return;
    }

    source_path = g_file_get_path (source);
    dest_path = g_file_get_path (destination);
    if (source_path == NULL || dest_path == NULL) {
        g_free (tool_path);
        g_free (source_path);
        g_free (dest_path);
        g_task_return_new_error (task, NOLPHIN_ENCRYPTION_ERROR, NOLPHIN_ENCRYPTION_ERROR_REMOTE_FILE,
                                 _("Verschlüsselung wird für entfernte Dateien noch nicht unterstützt."));
        g_object_unref (task);
        return;
    }

    argv = g_ptr_array_new_with_free_func (g_free);
    g_ptr_array_add (argv, tool_path); /* ownership moves to argv */
    g_ptr_array_add (argv, g_strdup ("--batch"));
    g_ptr_array_add (argv, g_strdup ("--yes"));
    g_ptr_array_add (argv, g_strdup ("--pinentry-mode"));
    g_ptr_array_add (argv, g_strdup ("loopback"));
    g_ptr_array_add (argv, g_strdup ("--passphrase-fd"));
    g_ptr_array_add (argv, g_strdup ("0"));
    for (i = 0; i < n_mode_args; i++) {
        g_ptr_array_add (argv, g_strdup (mode_args[i]));
    }
    g_ptr_array_add (argv, g_strdup ("-o"));
    g_ptr_array_add (argv, g_strdup (dest_path));
    g_ptr_array_add (argv, g_strdup (source_path));
    g_ptr_array_add (argv, NULL);

    g_free (source_path);
    g_free (dest_path);

    launcher = g_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDIN_PIPE |
                                          G_SUBPROCESS_FLAGS_STDERR_PIPE |
                                          G_SUBPROCESS_FLAGS_STDOUT_SILENCE);
    subprocess = g_subprocess_launcher_spawnv (launcher, (const gchar * const *) argv->pdata, &error);
    g_object_unref (launcher);
    g_ptr_array_free (argv, TRUE);

    if (subprocess == NULL) {
        g_task_return_error (task, error);
        g_object_unref (task);
        return;
    }

    /* The passphrase goes in as stdin, feeding gpg's --passphrase-fd 0 -
     * never as a command-line argument, so it never shows up in a
     * process listing. */
    g_subprocess_communicate_utf8_async (subprocess, passphrase, cancellable,
                                         communicate_ready_cb, task);
    g_object_unref (subprocess);
}

void
nolphin_encryption_encrypt_async (GFile                *source,
                                  GFile                *destination,
                                  const gchar          *passphrase,
                                  GCancellable         *cancellable,
                                  GAsyncReadyCallback   callback,
                                  gpointer              user_data)
{
    GTask *task;
    static const gchar *mode_args[] = { "--symmetric", "--cipher-algo", "AES256" };

    g_return_if_fail (G_IS_FILE (source));
    g_return_if_fail (G_IS_FILE (destination));
    g_return_if_fail (passphrase != NULL && passphrase[0] != '\0');

    task = g_task_new (NULL, cancellable, callback, user_data);
    g_task_set_source_tag (task, nolphin_encryption_encrypt_async);

    run_gpg_async (mode_args, G_N_ELEMENTS (mode_args), source, destination,
                  passphrase, cancellable, task);
}

gboolean
nolphin_encryption_encrypt_finish (GAsyncResult *result, GError **error)
{
    return g_task_propagate_boolean (G_TASK (result), error);
}

void
nolphin_encryption_decrypt_async (GFile                *source,
                                  GFile                *destination,
                                  const gchar          *passphrase,
                                  GCancellable         *cancellable,
                                  GAsyncReadyCallback   callback,
                                  gpointer              user_data)
{
    GTask *task;
    static const gchar *mode_args[] = { "--decrypt" };

    g_return_if_fail (G_IS_FILE (source));
    g_return_if_fail (G_IS_FILE (destination));
    g_return_if_fail (passphrase != NULL && passphrase[0] != '\0');

    task = g_task_new (NULL, cancellable, callback, user_data);
    g_task_set_source_tag (task, nolphin_encryption_decrypt_async);

    run_gpg_async (mode_args, G_N_ELEMENTS (mode_args), source, destination,
                  passphrase, cancellable, task);
}

gboolean
nolphin_encryption_decrypt_finish (GAsyncResult *result, GError **error)
{
    return g_task_propagate_boolean (G_TASK (result), error);
}
