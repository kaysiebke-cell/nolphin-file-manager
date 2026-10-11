/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nolphin-checksum.c: file checksum computation
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

#include "nolphin-checksum.h"

#include <glib/gi18n.h>
#include <string.h>

#define READ_CHUNK_SIZE (256 * 1024)

GQuark
nolphin_checksum_error_quark (void)
{
    return g_quark_from_static_string ("nolphin-checksum-error-quark");
}

const gchar *
nolphin_checksum_type_get_label (NolphinChecksumType type)
{
    switch (type) {
    case NOLPHIN_CHECKSUM_MD5:
        return "MD5";
    case NOLPHIN_CHECKSUM_SHA1:
        return "SHA-1";
    case NOLPHIN_CHECKSUM_SHA256:
        return "SHA-256";
    case NOLPHIN_CHECKSUM_SHA512:
        return "SHA-512";
    case NOLPHIN_CHECKSUM_BLAKE2:
        return "BLAKE2";
    default:
        g_return_val_if_reached ("");
    }
}

static GChecksumType
to_gchecksum_type (NolphinChecksumType type)
{
    switch (type) {
    case NOLPHIN_CHECKSUM_MD5:
        return G_CHECKSUM_MD5;
    case NOLPHIN_CHECKSUM_SHA1:
        return G_CHECKSUM_SHA1;
    case NOLPHIN_CHECKSUM_SHA256:
        return G_CHECKSUM_SHA256;
    case NOLPHIN_CHECKSUM_SHA512:
        return G_CHECKSUM_SHA512;
    default:
        g_return_val_if_reached (G_CHECKSUM_MD5);
    }
}

gboolean
nolphin_checksum_type_is_available (NolphinChecksumType type)
{
    if (type == NOLPHIN_CHECKSUM_BLAKE2) {
        gchar *path = g_find_program_in_path ("b2sum");
        gboolean available = (path != NULL);
        g_free (path);
        return available;
    }

    /* MD5/SHA* are always available - GChecksum is part of GLib itself. */
    return TRUE;
}

/* --- GChecksum-based types (MD5/SHA1/SHA256/SHA512), any GVFS backend --- */

typedef struct {
    GFile *file;
    NolphinChecksumType type;
} GChecksumThreadData;

static void
gchecksum_thread_data_free (GChecksumThreadData *data)
{
    g_clear_object (&data->file);
    g_free (data);
}

static void
compute_via_gchecksum_thread (GTask        *task,
                              gpointer      source_object,
                              gpointer      task_data,
                              GCancellable *cancellable)
{
    GChecksumThreadData *data = task_data;
    GFileInputStream *stream;
    GChecksum *checksum;
    guchar *buffer;
    GError *error = NULL;

    stream = g_file_read (data->file, cancellable, &error);
    if (stream == NULL) {
        g_task_return_error (task, error);
        return;
    }

    checksum = g_checksum_new (to_gchecksum_type (data->type));
    buffer = g_malloc (READ_CHUNK_SIZE);

    while (TRUE) {
        gssize bytes_read;

        bytes_read = g_input_stream_read (G_INPUT_STREAM (stream), buffer,
                                          READ_CHUNK_SIZE, cancellable, &error);
        if (bytes_read < 0) {
            g_checksum_free (checksum);
            g_free (buffer);
            g_object_unref (stream);
            g_task_return_error (task, error);
            return;
        }
        if (bytes_read == 0) {
            break;
        }

        g_checksum_update (checksum, buffer, bytes_read);
    }

    g_free (buffer);
    g_input_stream_close (G_INPUT_STREAM (stream), NULL, NULL);
    g_object_unref (stream);

    g_task_return_pointer (task, g_strdup (g_checksum_get_string (checksum)),
                           g_free);
    g_checksum_free (checksum);
}

/* --- BLAKE2 via b2sum, local files only --------------------------------- */

static void
b2sum_communicate_cb (GObject *source, GAsyncResult *result, gpointer user_data)
{
    GTask *task = user_data;
    GSubprocess *subprocess = G_SUBPROCESS (source);
    GError *error = NULL;
    gchar *stdout_buf = NULL;
    gchar *digest;
    gchar *space;

    if (!g_subprocess_communicate_utf8_finish (subprocess, result, &stdout_buf, NULL, &error)) {
        g_task_return_error (task, error);
        g_object_unref (task);
        g_free (stdout_buf);
        return;
    }

    if (!g_subprocess_get_successful (subprocess)) {
        g_task_return_new_error (task, NOLPHIN_CHECKSUM_ERROR, NOLPHIN_CHECKSUM_ERROR_TOOL_FAILED,
                                 _("b2sum wurde mit einem Fehler beendet."));
        g_object_unref (task);
        g_free (stdout_buf);
        return;
    }

    /* Output is "<hex digest>  <filename>\n" - take the first token. */
    space = stdout_buf != NULL ? strchr (stdout_buf, ' ') : NULL;
    if (space == NULL) {
        g_task_return_new_error (task, NOLPHIN_CHECKSUM_ERROR, NOLPHIN_CHECKSUM_ERROR_TOOL_FAILED,
                                 _("Unerwartete Ausgabe von b2sum."));
        g_object_unref (task);
        g_free (stdout_buf);
        return;
    }

    digest = g_strndup (stdout_buf, space - stdout_buf);
    g_task_return_pointer (task, digest, g_free);
    g_object_unref (task);
    g_free (stdout_buf);
}

static void
compute_via_b2sum (GFile *file, GCancellable *cancellable, GTask *task)
{
    gchar *path;
    gchar *tool_path;
    GSubprocessLauncher *launcher;
    GSubprocess *subprocess;
    GError *error = NULL;

    tool_path = g_find_program_in_path ("b2sum");
    if (tool_path == NULL) {
        g_task_return_new_error (task, NOLPHIN_CHECKSUM_ERROR, NOLPHIN_CHECKSUM_ERROR_TOOL_NOT_FOUND,
                                 _("Das Werkzeug »b2sum« ist nicht installiert."));
        g_object_unref (task);
        return;
    }

    path = g_file_get_path (file);
    if (path == NULL) {
        g_free (tool_path);
        g_task_return_new_error (task, NOLPHIN_CHECKSUM_ERROR, NOLPHIN_CHECKSUM_ERROR_REMOTE_FILE,
                                 _("BLAKE2-Prüfsummen werden für entfernte Dateien noch nicht unterstützt."));
        g_object_unref (task);
        return;
    }

    launcher = g_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE |
                                          G_SUBPROCESS_FLAGS_STDERR_SILENCE);
    subprocess = g_subprocess_launcher_spawn (launcher, &error, tool_path, path, NULL);
    g_object_unref (launcher);
    g_free (tool_path);
    g_free (path);

    if (subprocess == NULL) {
        g_task_return_error (task, error);
        g_object_unref (task);
        return;
    }

    g_subprocess_communicate_utf8_async (subprocess, NULL, cancellable,
                                         b2sum_communicate_cb, task);
    g_object_unref (subprocess);
}

void
nolphin_checksum_compute_async (GFile                *file,
                                NolphinChecksumType   type,
                                GCancellable         *cancellable,
                                GAsyncReadyCallback   callback,
                                gpointer              user_data)
{
    GTask *task;

    g_return_if_fail (G_IS_FILE (file));

    task = g_task_new (NULL, cancellable, callback, user_data);
    g_task_set_source_tag (task, nolphin_checksum_compute_async);

    if (type == NOLPHIN_CHECKSUM_BLAKE2) {
        compute_via_b2sum (file, cancellable, task);
        return;
    }

    {
        GChecksumThreadData *data = g_new0 (GChecksumThreadData, 1);
        data->file = g_object_ref (file);
        data->type = type;
        g_task_set_task_data (task, data, (GDestroyNotify) gchecksum_thread_data_free);
    }

    g_task_run_in_thread (task, compute_via_gchecksum_thread);
    g_object_unref (task);
}

gchar *
nolphin_checksum_compute_finish (GAsyncResult *result, GError **error)
{
    return g_task_propagate_pointer (G_TASK (result), error);
}

gboolean
nolphin_checksum_matches (const gchar *computed, const gchar *expected)
{
    gchar *a, *b;
    gboolean result;

    if (computed == NULL || expected == NULL) {
        return FALSE;
    }

    a = g_strdup (computed);
    b = g_strstrip (g_strdup (expected));
    g_strstrip (a);

    result = (g_ascii_strcasecmp (a, b) == 0);

    g_free (a);
    g_free (b);

    return result;
}
