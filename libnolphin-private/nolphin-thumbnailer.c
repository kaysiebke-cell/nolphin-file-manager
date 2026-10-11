/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nolphin-thumbnailer.c: eigene Vorschaubild-Erzeugung, siehe Header
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 */

#include <config.h>

#include "nolphin-thumbnailer.h"

#include <glib/gstdio.h>
#include <gio/gio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#define THUMBNAILER_TIMEOUT_SECONDS 30
/* Gleicher Ordnername wie bei GNOME/Cinnamon: GIO prüft genau diesen Ordner,
 * um "thumbnail::failed" zu melden. */
#define FAIL_DIR_NAME "gnome-thumbnail-factory"

typedef struct {
    gchar  *try_exec;
    gchar  *exec;
} ThumbnailerEntry;

static GHashTable *thumbnailers_by_mime = NULL; /* mime -> ThumbnailerEntry* */
static GHashTable *pixbuf_mimes = NULL;         /* mime -> (nicht NULL) */

static void
thumbnailer_entry_free (ThumbnailerEntry *e)
{
    g_free (e->try_exec);
    g_free (e->exec);
    g_free (e);
}

static gboolean
program_available (const gchar *program)
{
    gchar *found;
    gboolean ok;

    if (program == NULL || program[0] == '\0') {
        return FALSE;
    }
    if (g_path_is_absolute (program)) {
        return g_file_test (program, G_FILE_TEST_IS_EXECUTABLE);
    }
    found = g_find_program_in_path (program);
    ok = (found != NULL);
    g_free (found);
    return ok;
}

static void
load_thumbnailer_dir (const gchar *base_dir)
{
    gchar *dir = g_build_filename (base_dir, "thumbnailers", NULL);
    GDir *d = g_dir_open (dir, 0, NULL);
    const gchar *name;

    while (d != NULL && (name = g_dir_read_name (d)) != NULL) {
        GKeyFile *kf;
        gchar *path, *exec, *try_exec;
        gchar **mimes;
        gsize n, i;

        if (!g_str_has_suffix (name, ".thumbnailer")) {
            continue;
        }

        path = g_build_filename (dir, name, NULL);
        kf = g_key_file_new ();
        if (g_key_file_load_from_file (kf, path, G_KEY_FILE_NONE, NULL)) {
            exec = g_key_file_get_string (kf, "Thumbnailer Entry", "Exec", NULL);
            try_exec = g_key_file_get_string (kf, "Thumbnailer Entry", "TryExec", NULL);
            mimes = g_key_file_get_string_list (kf, "Thumbnailer Entry", "MimeType", &n, NULL);

            if (exec != NULL && mimes != NULL) {
                for (i = 0; i < n; i++) {
                    /* Das zuerst gefundene (Benutzerordner vor System) gilt. */
                    if (!g_hash_table_contains (thumbnailers_by_mime, mimes[i])) {
                        ThumbnailerEntry *e = g_new0 (ThumbnailerEntry, 1);
                        e->exec = g_strdup (exec);
                        e->try_exec = g_strdup (try_exec);
                        g_hash_table_insert (thumbnailers_by_mime, g_strdup (mimes[i]), e);
                    }
                }
            }
            g_free (exec);
            g_free (try_exec);
            g_strfreev (mimes);
        }
        g_key_file_free (kf);
        g_free (path);
    }

    if (d != NULL) {
        g_dir_close (d);
    }
    g_free (dir);
}

static void
ensure_tables (void)
{
    static gsize once = 0;

    if (g_once_init_enter (&once)) {
        const gchar * const *sys_dirs = g_get_system_data_dirs ();
        GSList *formats, *l;
        guint i;

        thumbnailers_by_mime = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
                                                      (GDestroyNotify) thumbnailer_entry_free);
        load_thumbnailer_dir (g_get_user_data_dir ());
        for (i = 0; sys_dirs[i] != NULL; i++) {
            load_thumbnailer_dir (sys_dirs[i]);
        }

        pixbuf_mimes = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
        formats = gdk_pixbuf_get_formats ();
        for (l = formats; l != NULL; l = l->next) {
            gchar **mimes = gdk_pixbuf_format_get_mime_types (l->data);
            gint k;
            for (k = 0; mimes != NULL && mimes[k] != NULL; k++) {
                g_hash_table_insert (pixbuf_mimes, g_strdup (mimes[k]), GINT_TO_POINTER (1));
            }
            g_strfreev (mimes);
        }
        g_slist_free (formats);

        g_once_init_leave (&once, 1);
    }
}

static const ThumbnailerEntry *
find_usable_thumbnailer (const gchar *mime_type)
{
    const ThumbnailerEntry *e;

    if (mime_type == NULL) {
        return NULL;
    }
    ensure_tables ();
    e = g_hash_table_lookup (thumbnailers_by_mime, mime_type);
    if (e == NULL) {
        return NULL;
    }
    if (e->try_exec != NULL && e->try_exec[0] != '\0') {
        return program_available (e->try_exec) ? e : NULL;
    }
    {
        /* ohne TryExec: das erste Wort von Exec muss ein vorhandenes Programm sein */
        gchar **argv = NULL;
        gboolean ok = FALSE;
        if (g_shell_parse_argv (e->exec, NULL, &argv, NULL)) {
            ok = program_available (argv[0]);
        }
        g_strfreev (argv);
        return ok ? e : NULL;
    }
}

static gboolean
is_pixbuf_mime (const gchar *mime_type)
{
    ensure_tables ();
    return mime_type != NULL && g_hash_table_contains (pixbuf_mimes, mime_type);
}

static gchar *
cache_base_dir (void)
{
    return g_build_filename (g_get_user_cache_dir (), "thumbnails", NULL);
}

gchar *
nolphin_thumbnailer_path_for_uri (const gchar *uri, gboolean failed)
{
    gchar *base = cache_base_dir ();
    gchar *md5 = g_compute_checksum_for_string (G_CHECKSUM_MD5, uri, -1);
    gchar *name = g_strconcat (md5, ".png", NULL);
    gchar *path = failed ? g_build_filename (base, "fail", FAIL_DIR_NAME, name, NULL)
                         : g_build_filename (base, "large", name, NULL);

    g_free (name);
    g_free (md5);
    g_free (base);
    return path;
}

static gboolean
failed_for_mtime (const gchar *uri, time_t mtime)
{
    gchar *path = nolphin_thumbnailer_path_for_uri (uri, TRUE);
    GdkPixbuf *marker = NULL;
    gboolean same = FALSE;

    if (g_file_test (path, G_FILE_TEST_EXISTS)) {
        marker = gdk_pixbuf_new_from_file (path, NULL);
        if (marker != NULL) {
            const gchar *stored = gdk_pixbuf_get_option (marker, "tEXt::Thumb::MTime");
            gchar *wanted = g_strdup_printf ("%" G_GINT64_FORMAT, (gint64) mtime);
            same = (g_strcmp0 (stored, wanted) == 0);
            g_free (wanted);
            g_object_unref (marker);
        }
    }
    g_free (path);
    return same;
}

gboolean
nolphin_thumbnailer_can_thumbnail (const gchar *uri, const gchar *mime_type, time_t mtime)
{
    gchar *path, *base;
    gboolean ok;

    g_return_val_if_fail (uri != NULL, FALSE);

    if (mime_type == NULL || !(is_pixbuf_mime (mime_type) || find_usable_thumbnailer (mime_type) != NULL)) {
        return FALSE;
    }

    /* nur lokale Dateien; Netzwerk-Adressen werden vom Aufrufer vorher
     * in lokale Pfade umgesetzt */
    {
        GFile *f = g_file_new_for_uri (uri);
        path = g_file_get_path (f);
        g_object_unref (f);
    }
    if (path == NULL) {
        return FALSE;
    }

    /* keine Vorschaubilder von Vorschaubildern */
    base = cache_base_dir ();
    ok = !g_str_has_prefix (path, base);
    g_free (base);
    g_free (path);
    if (!ok) {
        return FALSE;
    }

    return !failed_for_mtime (uri, mtime);
}

typedef struct {
    GMainLoop   *loop;
    GSubprocess *proc;
    gboolean     finished;
} RunData;

static void
run_done_cb (GObject *source, GAsyncResult *res, gpointer user_data)
{
    RunData *data = user_data;

    g_subprocess_wait_finish (G_SUBPROCESS (source), res, NULL);
    data->finished = TRUE;
    g_main_loop_quit (data->loop);
}

static gboolean
run_timeout_cb (gpointer user_data)
{
    RunData *data = user_data;

    g_subprocess_force_exit (data->proc);
    return G_SOURCE_REMOVE;
}

/* Startet @argv ohne Shell und wartet höchstens @seconds; danach wird der
 * Prozess beendet. */
static gboolean
run_with_timeout (gchar **argv, guint seconds)
{
    RunData data = { NULL, NULL, FALSE };
    GMainContext *ctx;
    GSource *timeout;
    gboolean success;
    GError *error = NULL;

    data.proc = g_subprocess_newv ((const gchar * const *) argv,
                                   G_SUBPROCESS_FLAGS_STDOUT_SILENCE | G_SUBPROCESS_FLAGS_STDERR_SILENCE,
                                   &error);
    if (data.proc == NULL) {
        g_clear_error (&error);
        return FALSE;
    }

    ctx = g_main_context_new ();
    g_main_context_push_thread_default (ctx);
    data.loop = g_main_loop_new (ctx, FALSE);

    g_subprocess_wait_async (data.proc, NULL, run_done_cb, &data);
    timeout = g_timeout_source_new_seconds (seconds);
    g_source_set_callback (timeout, run_timeout_cb, &data, NULL);
    g_source_attach (timeout, ctx);
    g_main_loop_run (data.loop);
    g_source_destroy (timeout);
    g_source_unref (timeout);

    success = data.finished && g_subprocess_get_if_exited (data.proc) &&
              g_subprocess_get_exit_status (data.proc) == 0;

    g_main_loop_unref (data.loop);
    g_main_context_pop_thread_default (ctx);
    g_main_context_unref (ctx);
    g_object_unref (data.proc);
    return success;
}

/* ersetzt %u %i %o %s %% in einem einzelnen Argument */
static gchar *
expand_arg (const gchar *arg, const gchar *uri, const gchar *input_path,
            const gchar *output_path, guint size)
{
    GString *out = g_string_new (NULL);
    const gchar *p;

    for (p = arg; *p != '\0'; p++) {
        if (*p == '%' && p[1] != '\0') {
            p++;
            switch (*p) {
            case 'u': g_string_append (out, uri); break;
            case 'i': g_string_append (out, input_path); break;
            case 'o': g_string_append (out, output_path); break;
            case 's': g_string_append_printf (out, "%u", size); break;
            case '%': g_string_append_c (out, '%'); break;
            default: g_string_append_c (out, '%'); g_string_append_c (out, *p); break;
            }
        } else {
            g_string_append_c (out, *p);
        }
    }
    return g_string_free (out, FALSE);
}

static GdkPixbuf *
fit_to_large (GdkPixbuf *pixbuf)
{
    gint w = gdk_pixbuf_get_width (pixbuf), h = gdk_pixbuf_get_height (pixbuf);
    gint max = NOLPHIN_THUMBNAILER_LARGE_SIZE;
    GdkPixbuf *scaled;

    if (w <= max && h <= max) {
        return pixbuf;
    }
    if (w >= h) {
        h = MAX (1, (h * max) / w);
        w = max;
    } else {
        w = MAX (1, (w * max) / h);
        h = max;
    }
    scaled = gdk_pixbuf_scale_simple (pixbuf, w, h, GDK_INTERP_BILINEAR);
    g_object_unref (pixbuf);
    return scaled;
}

GdkPixbuf *
nolphin_thumbnailer_generate (const gchar *uri, const gchar *mime_type)
{
    GFile *file;
    gchar *path;
    GdkPixbuf *pixbuf = NULL;

    g_return_val_if_fail (uri != NULL, NULL);

    file = g_file_new_for_uri (uri);
    path = g_file_get_path (file);
    g_object_unref (file);
    if (path == NULL) {
        return NULL;
    }

    if (is_pixbuf_mime (mime_type)) {
        GdkPixbuf *raw = gdk_pixbuf_new_from_file_at_size (path, NOLPHIN_THUMBNAILER_LARGE_SIZE,
                                                           NOLPHIN_THUMBNAILER_LARGE_SIZE, NULL);
        if (raw != NULL) {
            pixbuf = gdk_pixbuf_apply_embedded_orientation (raw);
            g_object_unref (raw);
        }
    }

    if (pixbuf == NULL) {
        const ThumbnailerEntry *entry = find_usable_thumbnailer (mime_type);

        if (entry != NULL) {
            gchar **argv = NULL;
            gchar *out_path = NULL;
            gint fd = g_file_open_tmp ("nolphin-thumb-XXXXXX.png", &out_path, NULL);

            if (fd >= 0 && g_shell_parse_argv (entry->exec, NULL, &argv, NULL)) {
                gint i, n = g_strv_length (argv);
                gchar **expanded = g_new0 (gchar *, n + 1);

                close (fd);
                fd = -1;
                /* erst zerlegen, dann je Argument ersetzen: Dateinamen mit
                 * Leerzeichen oder Sonderzeichen werden so nie als Befehl
                 * gedeutet */
                for (i = 0; i < n; i++) {
                    expanded[i] = expand_arg (argv[i], uri, path, out_path, NOLPHIN_THUMBNAILER_LARGE_SIZE);
                }
                /* NOLPHIN_THUMBNAILER_TIMEOUT: nur für Tests */
                const gchar *t_env = g_getenv ("NOLPHIN_THUMBNAILER_TIMEOUT");
                guint t = t_env != NULL ? (guint) g_ascii_strtoull (t_env, NULL, 10) : 0;

                if (run_with_timeout (expanded, t > 0 ? t : THUMBNAILER_TIMEOUT_SECONDS)) {
                    GdkPixbuf *raw = gdk_pixbuf_new_from_file (out_path, NULL);
                    if (raw != NULL) {
                        pixbuf = fit_to_large (raw);
                    }
                }
                g_strfreev (expanded);
            }
            if (fd >= 0) {
                close (fd);
            }
            if (out_path != NULL) {
                g_unlink (out_path);
                g_free (out_path);
            }
            g_strfreev (argv);
        }
    }

    g_free (path);
    return pixbuf;
}

static gboolean
ensure_parent_dir (const gchar *file_path)
{
    gchar *dir = g_path_get_dirname (file_path);
    gint rc = g_mkdir_with_parents (dir, 0700);

    g_free (dir);
    return rc == 0;
}

/* Schreibt @pixbuf atomar (Temp-Datei + Umbenennen) mit den
 * Thumb::-Textfeldern der Spezifikation. */
static void
write_png (GdkPixbuf *pixbuf, const gchar *final_path, const gchar *uri, time_t mtime)
{
    gchar *tmp_path, *mtime_str;
    GError *error = NULL;

    if (!ensure_parent_dir (final_path)) {
        return;
    }

    tmp_path = g_strdup_printf ("%s.%d.tmp", final_path, (int) getpid ());
    mtime_str = g_strdup_printf ("%" G_GINT64_FORMAT, (gint64) mtime);

    if (gdk_pixbuf_save (pixbuf, tmp_path, "png", &error,
                         "tEXt::Thumb::URI", uri,
                         "tEXt::Thumb::MTime", mtime_str,
                         "tEXt::Software", "Nolphin",
                         NULL)) {
        g_chmod (tmp_path, 0600);
        if (g_rename (tmp_path, final_path) != 0) {
            g_unlink (tmp_path);
        }
    } else {
        g_clear_error (&error);
        g_unlink (tmp_path);
    }

    g_free (mtime_str);
    g_free (tmp_path);
}

void
nolphin_thumbnailer_save (GdkPixbuf *pixbuf, const gchar *uri, time_t mtime)
{
    gchar *path;

    g_return_if_fail (GDK_IS_PIXBUF (pixbuf));
    g_return_if_fail (uri != NULL);

    path = nolphin_thumbnailer_path_for_uri (uri, FALSE);
    write_png (pixbuf, path, uri, mtime);
    g_free (path);
}

void
nolphin_thumbnailer_create_failed (const gchar *uri, time_t mtime)
{
    gchar *path;
    GdkPixbuf *marker;

    g_return_if_fail (uri != NULL);

    marker = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, 1, 1);
    gdk_pixbuf_fill (marker, 0);
    path = nolphin_thumbnailer_path_for_uri (uri, TRUE);
    write_png (marker, path, uri, mtime);
    g_free (path);
    g_object_unref (marker);
}

gboolean
nolphin_thumbnailer_cache_ok (void)
{
    gchar *base = cache_base_dir ();
    GStatBuf st;
    gboolean ok = TRUE;

    /* Gibt es den Ordner noch nicht, wird er beim ersten Speichern angelegt. */
    if (g_stat (base, &st) == 0) {
        ok = (st.st_uid == getuid ()) && g_access (base, W_OK) == 0;
    }
    g_free (base);
    return ok;
}
