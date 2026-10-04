/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nolphin-thumbnail-cache.h: Thumbnail code for icon factory.
 
   Copyright (C) 2000, 2001 Eazel, Inc.
   Copyright (C) 2002, 2003 Red Hat, Inc.
  
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
  
   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
   Boston, MA 02110-1335, USA.
  
   Author: Andy Hertzfeld <andy@eazel.com>
*/

#include <config.h>
#include "nolphin-thumbnails.h"

#define GNOME_DESKTOP_USE_UNSTABLE_API

#include "nolphin-directory-notify.h"
#include "nolphin-global-preferences.h"
#include "nolphin-file-utilities.h"
#include <math.h>
#include <eel/eel-graphic-effects.h>
#include <eel/eel-string.h>
#include <eel/eel-debug.h>
#include <eel/eel-vfs-extensions.h>
#include <gtk/gtk.h>
#include <errno.h>
#include <glib/gstdio.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <libcinnamon-desktop/gnome-desktop-thumbnail.h>

#define DEBUG_FLAG NOLPHIN_DEBUG_THUMBNAILS
#include <libnolphin-private/nolphin-debug.h>

#include "nolphin-file-private.h"

#define DEBUG_THREADS 0

/* Should never be a reasonable actual mtime */
#define INVALID_MTIME 0

/* Cool-off period between last file modification time and thumbnail creation */
#define RECENT_MTIME_COOLDOWN 2

#define NOLPHIN_THUMBNAIL_FRAME_LEFT 3
#define NOLPHIN_THUMBNAIL_FRAME_TOP 3
#define NOLPHIN_THUMBNAIL_FRAME_RIGHT 3
#define NOLPHIN_THUMBNAIL_FRAME_BOTTOM 3


typedef enum {
    THUMBNAIL_ADD,
    THUMBNAIL_REMOVE,
    THUMBNAIL_BUMP,
    THUMBNAIL_THREAD_EXIT
} ThumbnailCommandType;

/* multipurpose structure used for making thumbnails, associating a uri with where the thumbnail is to be stored */
typedef struct {
    char *image_uri;
    char *mime_type;
    time_t original_file_mtime;
    gint64 add_time;
    ThumbnailCommandType cmd_type;
    guint cancelled : 1;
} NolphinThumbnailInfo;

/* How it works:
 * 
 * When nolphin_create_thumbnail(), nolphin_thumbnail_remove_from_queue or nolphin_thumbnail_prioritize are called,
 * a new NolphinThumbnailInfo is made, with info->cmd_type set based on the method called.
 *
 * These are added to the feeder queue, which feeds the feeder_task thread. When a new info arrives, it gets
 * processed based on info->cmd_type.

 * - nolphin_create_thumbnail (THUMBNAIL_ADD): The info is looked up by uri in thumbnails_to_make_hash. If
 *   the info is already there, the existing info's mtime is updated, and the info gets pushed to the front
 *   of the threadpool queue. Otherwise, the incoming info is added to thumbnails_to_make_hash, and pushed
 *   to the threadpool queue.
 *
 * - nolphin_thumbnail_remove_from_queue (THUMBNAIL_REMOVE): The info is looked up by uri in thumbnails_to_make_hash.
 *   If the info is found, it gets removed from thumbnails_to_make_hash, and info->cancelled is set to TRUE, so when
 *   it comes up in the threadpool queue, it is ignored and freed.
 *
 * - nolphin_thumbnail_prioritize (THUMBNAIL_BUMP): The info is looked up by uri in thumbnails_to_make_hash. If found,
 *   it gets moved to the front of the threadpool queue.
 *
 *
 * - No mutex locking occurs in the public methods, only in the feeder and threadpool threads.
 * - NolphinThumbnailInfos are garbage-collected in the threadpool worker only.
 */

/* Workers that actually make the thumbnail. */
static volatile GThreadPool *tpool = NULL;

/* Table of uris queued to the thread pool (tpool) */
static GMutex thumbnails_mutex;
static GHashTable *thumbnails_to_make_hash = NULL;

/* Every action goes thru the feeder queue. It gets processed in the feeder_task's thread. */
static GAsyncQueue *feeder_queue = NULL;
static GTask *feeder_task = NULL;

/* Causes the feeder_task to end. Only called when Nolphin is shutting down. */
GCancellable *cancellable = NULL;

static GnomeDesktopThumbnailFactory *thumbnail_factory = NULL;

static gint
get_max_threads (void) {
    gint max_threads = 1;
    gint num_processors = g_get_num_processors ();

    gint pref = g_settings_get_int (nolphin_preferences, NOLPHIN_PREFERENCES_MAX_THUMBNAIL_THREADS);

    if (pref == -1) {
        if (num_processors >= 8) {
            max_threads = 4;
        }
        else if (num_processors >= 4) {
            max_threads = 2;
        }
        else {
            max_threads = 1;
        }
    } else {
        max_threads = pref;
    }

    max_threads = MAX (1, max_threads);

#if DEBUG_THREADS
    g_message ("Thumbnailer threads: %d (setting: %d, system count: %d)", max_threads, pref, num_processors);
#else
    DEBUG ("Thumbnailer threads: %d (setting: %d, system count: %d)", max_threads, pref, num_processors);
#endif

    return max_threads;
}

static gint
lifo_sorter (gconstpointer a,
             gconstpointer b,
             gpointer      data)
{
    gint64 ta = ((const NolphinThumbnailInfo *) a)->add_time;
    gint64 tb = ((const NolphinThumbnailInfo *) b)->add_time;

    return tb > ta ? +1 : ta == tb ? 0 : -1;
}

static gboolean
get_file_mtime (const char *file_uri, time_t* mtime)
{
    GFile *file;
    GFileInfo *info;
    gboolean ret;

    ret = FALSE;
    *mtime = INVALID_MTIME;

    file = g_file_new_for_uri (file_uri);
    info = g_file_query_info (file, G_FILE_ATTRIBUTE_TIME_MODIFIED, 0, NULL, NULL);
    if (info) {
        if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_TIME_MODIFIED)) {
            *mtime = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_TIME_MODIFIED);
            ret = TRUE;
        }

        g_object_unref (info);
    }
    g_object_unref (file);

    return ret;
}

static void
free_thumbnail_info (NolphinThumbnailInfo *info)
{
    g_free (info->image_uri);
    g_free (info->mime_type);
    g_free (info);
}

static GnomeDesktopThumbnailFactory *
get_thumbnail_factory (void)
{
    static gsize once_init = 0;

    if (g_once_init_enter (&once_init)) {
        thumbnail_factory = gnome_desktop_thumbnail_factory_new (GNOME_DESKTOP_THUMBNAIL_SIZE_LARGE);

        g_once_init_leave (&once_init, 1);
    }

    return thumbnail_factory;
}

static GdkPixbuf *
nolphin_get_thumbnail_frame (void)
{
    static GdkPixbuf *thumbnail_frame = NULL;
    static gsize once_init = 0;

    if (g_once_init_enter (&once_init)) {
        GInputStream *stream = g_resources_open_stream ("/org/nolphin/icons/thumbnail_frame.png", 0, NULL);
        if (stream != NULL) {
            thumbnail_frame = gdk_pixbuf_new_from_stream (stream, NULL, NULL);
            g_object_unref (stream);
        }

        g_once_init_leave (&once_init, 1);
    }

    return thumbnail_frame;
}

static GHashTable *
get_types_table (void)
{
    static GHashTable *image_mime_types = NULL;
    GSList *format_list, *l;
    char **types;
    int i;

    static gsize once_init = 0;

    if (g_once_init_enter (&once_init)) {
        image_mime_types = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                  g_free, NULL);
        format_list = gdk_pixbuf_get_formats ();
        for (l = format_list; l; l = l->next) {
            types = gdk_pixbuf_format_get_mime_types (l->data);

            for (i = 0; types[i] != NULL; i++) {
                g_hash_table_add (image_mime_types, types[i]);
            }

            g_free (types);
        }

        g_slist_free (format_list);

        g_once_init_leave (&once_init, 1);
    }

    return image_mime_types;
}

static gboolean
pixbuf_can_load_type (const char *mime_type)
{
    GHashTable *image_mime_types;

    image_mime_types = get_types_table ();

    return g_hash_table_contains (image_mime_types, mime_type);
}

/* This is a one-shot idle callback called from the main loop to call
   notify_file_changed() for a thumbnail. It frees the uri afterwards.
   We do this in an idle callback as I don't think nolphin_file_changed() is
   thread-safe. */
static gboolean
thumbnail_thread_notify_file_changed (gpointer image_uri)
{
    NolphinFile *file;

    file = nolphin_file_get_by_uri ((char *) image_uri);

    DEBUG ("(Thumbnail Thread) Notifying file changed file: %p uri: %s", file, (char*) image_uri);

    if (file != NULL) {
        nolphin_file_set_is_thumbnailing (file, FALSE);
        nolphin_file_invalidate_attributes (file,
                                         NOLPHIN_FILE_ATTRIBUTE_THUMBNAIL |
                                         NOLPHIN_FILE_ATTRIBUTE_INFO);
        nolphin_file_unref (file);
    }

    g_free (image_uri);

    return G_SOURCE_REMOVE;
}

/* Always on thumbnail thread */
static void
remove_from_hash_table (NolphinThumbnailInfo *info)
{
    g_mutex_lock (&thumbnails_mutex);
    g_hash_table_remove (thumbnails_to_make_hash, info->image_uri);
    g_mutex_unlock (&thumbnails_mutex);

    free_thumbnail_info (info);
}

/* Fallback-Vorschauen für CAD-Formate, für die der Systemthumbnailer
 * nichts liefert: FreeCAD (.FCStd/.FCBak) trägt sein Vorschaubild im Zip
 * (thumbnails/Thumbnail.png); STEP/IGES/BREP rendert f3d ohne die
 * "thumbnail"-Konfiguration, die bei manchen Dateien scheitert. */
static gboolean
has_suffix_ci (const char *uri, const char *suffix)
{
    g_autofree gchar *lower = g_ascii_strdown (uri, -1);

    return g_str_has_suffix (lower, suffix);
}

static gboolean
is_freecad_uri (const char *uri)
{
    return has_suffix_ci (uri, ".fcstd") || has_suffix_ci (uri, ".fcbak");
}

static gboolean
is_f3d_cad_mime (const char *mime_type)
{
    return mime_type != NULL &&
           (g_str_equal (mime_type, "application/vnd.step") ||
            g_str_equal (mime_type, "model/iges") ||
            g_str_equal (mime_type, "application/vnd.brep"));
}

/* stl-thumb öffnet pro Aufruf einen GL-Kontext; mehrere parallele Aufrufe
 * aus dem Thumbnail-Threadpool scheitern teilweise, daher serialisiert. */
static gboolean
is_stl_mime (const char *mime_type)
{
    return mime_type != NULL &&
           (g_str_equal (mime_type, "model/stl") ||
            g_str_equal (mime_type, "model/x.stl-ascii") ||
            g_str_equal (mime_type, "model/x.stl-binary") ||
            g_str_equal (mime_type, "application/sla"));
}

G_LOCK_DEFINE_STATIC (fallback_render);

static gboolean
can_thumbnail_cad_fallback (const char *uri, const char *mime_type)
{
    if (is_freecad_uri (uri)) {
        return g_find_program_in_path ("unzip") != NULL;
    }
    if (is_stl_mime (mime_type)) {
        return g_find_program_in_path ("stl-thumb") != NULL;
    }
    return is_f3d_cad_mime (mime_type) && g_find_program_in_path ("f3d") != NULL;
}

static GdkPixbuf *
generate_cad_fallback_thumbnail (const char *uri, const char *mime_type)
{
    g_autofree gchar *path = g_filename_from_uri (uri, NULL, NULL);
    g_autofree gchar *dir = g_dir_make_tmp ("nolphin-thumb-XXXXXX", NULL);
    g_autofree gchar *png = NULL;
    GdkPixbuf *pixbuf = NULL;

    if (path == NULL || dir == NULL) {
        return NULL;
    }
    png = g_build_filename (dir, "thumb.png", NULL);

    if (is_freecad_uri (uri)) {
        /* Bevorzugt die größte Body-/Feature-Shape (BREP) per f3d rendern -
         * das zeigt das ganze Bauteil; das eingebettete Thumbnail ist nur
         * ein kleiner, oft abgeschnittener Bildschirmausschnitt. */
        g_autofree gchar *qpath = g_shell_quote (path);
        g_autofree gchar *qdir = g_shell_quote (dir);
        g_autofree gchar *qpng = g_shell_quote (png);
        g_autofree gchar *cmd = g_strdup_printf (
            "cd %s && member=$(unzip -l %s | awk '$4 ~ /\\.Shape\\.brp$/ && $4 !~ /AddSub|Suppressed|Internal|Sketch/ {print ($4 ~ /^Body/ ? 1 : 0), $1, $4}'"
            " | sort -k1,1n -k2,2n | tail -1 | cut -d' ' -f3-)"
            " && [ -n \"$member\" ] && unzip -p %s \"$member\" > part.brep"
            " && f3d --no-config --load-plugins=occt --verbose=quiet --resolution=256,256 --output=%s part.brep",
            qdir, qpath, qpath, qpng);
        g_autofree gchar *qembedded = g_shell_quote (png);
        const gchar *argv[] = { "/bin/sh", "-c", cmd, NULL };

        g_spawn_sync (NULL, (gchar **) argv, NULL, G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL,
                      NULL, NULL, NULL, NULL, NULL, NULL);

        if (!g_file_test (png, G_FILE_TEST_EXISTS)) {
            g_autofree gchar *cmd2 = g_strdup_printf ("unzip -p %s thumbnails/Thumbnail.png > %s", qpath, qembedded);
            const gchar *argv2[] = { "/bin/sh", "-c", cmd2, NULL };

            g_spawn_sync (NULL, (gchar **) argv2, NULL, G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL,
                          NULL, NULL, NULL, NULL, NULL, NULL);
        }
    } else if (is_stl_mime (mime_type)) {
        const gchar *argv[] = { "stl-thumb", "-f", "png", "-s", "256", path, png, NULL };

        G_LOCK (fallback_render);
        g_spawn_sync (NULL, (gchar **) argv, NULL, G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL |
                      G_SPAWN_STDERR_TO_DEV_NULL, NULL, NULL, NULL, NULL, NULL, NULL);
        G_UNLOCK (fallback_render);
    } else if (is_f3d_cad_mime (mime_type)) {
        const gchar *argv[] = { "f3d", "--no-config", "--load-plugins=occt", "--verbose=quiet",
                                "--resolution=256,256", NULL, NULL, NULL };
        g_autofree gchar *out = g_strdup_printf ("--output=%s", png);

        argv[5] = out;
        argv[6] = path;
        g_spawn_sync (NULL, (gchar **) argv, NULL, G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL |
                      G_SPAWN_STDERR_TO_DEV_NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    }

    pixbuf = gdk_pixbuf_new_from_file (png, NULL);
    g_unlink (png);
    {
        g_autofree gchar *brep = g_build_filename (dir, "part.brep", NULL);
        g_unlink (brep);
    }
    g_rmdir (dir);
    return pixbuf;
}

/* Thumbnail thread */
static void
thumbnail_thread (gpointer data,
                  gpointer user_data)
{
    NolphinThumbnailInfo *info = (NolphinThumbnailInfo *) data;
    GdkPixbuf *pixbuf;
    time_t current_time;
    gchar *image_uri = info->image_uri;
    gboolean free_uri = FALSE;

    if (g_cancellable_is_cancelled (cancellable) || info->cancelled) {
        DEBUG ("Skipping cancelled file: %s", info->image_uri);
        remove_from_hash_table (info);
        return;
    }

    time (&current_time);

    /* Don't try to create a thumbnail if the file was modified recently.
       This prevents constant re-thumbnailing of changing files. */ 
    if (current_time < info->original_file_mtime + RECENT_MTIME_COOLDOWN) {
        DEBUG ("(Thumbnail Thread) Skipping for %d seconds: %s",
               RECENT_MTIME_COOLDOWN, info->image_uri);

        /* Reschedule thumbnailing via a change notification */
        g_timeout_add_seconds (RECENT_MTIME_COOLDOWN, thumbnail_thread_notify_file_changed,
                               g_strdup (info->image_uri));
        remove_from_hash_table (info);
        return;
    }

    /* Create the thumbnail. */
    DEBUG ("(Thumbnail Thread) Creating thumbnail: %s", info->image_uri);

    if (eel_uri_is_network (info->image_uri)) {
        GFile *file = g_file_new_for_uri (info->image_uri);
        GError *err = NULL;
        free_uri = TRUE;

        image_uri = g_filename_to_uri (g_file_peek_path (file), NULL, &err);

        if (err) {
            DEBUG ("(Thumbnail Thread) Failed to convert local_filepath to uri: %s", err->message);
            g_error_free (err);

            image_uri = g_strdup (info->image_uri); // revert back to our original image_uri
        }

        g_object_unref (file);
    }

    /**
     * the following function internally uses g_filename_to_uri whenever it finds a %i in the thumbnailer config file
     * because of that we have to convert our path from the network URI to a local file:// URI or else any
     * thumbnailers that use %i wont generate thumbnails correctly
     */
    pixbuf = gnome_desktop_thumbnail_factory_generate_thumbnail (thumbnail_factory,
                                                                 image_uri,
                                                                 info->mime_type);
    if (free_uri) {
        g_free (image_uri);
    }

    if (pixbuf == NULL) {
        pixbuf = generate_cad_fallback_thumbnail (info->image_uri, info->mime_type);
    }

    if (pixbuf) {
        gnome_desktop_thumbnail_factory_save_thumbnail (thumbnail_factory,
                                                        pixbuf,
                                                        info->image_uri,
                                                        info->original_file_mtime);
        g_object_unref (pixbuf);
    } else {
        gnome_desktop_thumbnail_factory_create_failed_thumbnail (thumbnail_factory, 
                                                                 info->image_uri,
                                                                 info->original_file_mtime);
    }

    /* We need to call nolphin_file_changed(), but I don't think that is
       thread safe. So add an idle handler and do it from the main loop. */
    g_idle_add_full (G_PRIORITY_HIGH_IDLE,
                     thumbnail_thread_notify_file_changed,
                     g_strdup (info->image_uri), NULL);

#if DEBUG_THREADS
    g_message ("%u unprocessed (Done) (%u threads free)",
               g_thread_pool_unprocessed ((GThreadPool *) tpool),
               g_thread_pool_get_num_unused_threads ());
#endif
    remove_from_hash_table (info);
}

/* Mainloop */
static  void
feeder_task_complete (GObject      *source,
                       GAsyncResult *res,
                       gpointer      user_data)
{
    g_task_propagate_boolean (G_TASK (res), NULL);
    DEBUG ("(Finalize) Feeder task done");
}

/* Feeder thread */
static void
feeder_thread (GTask        *task,
                gpointer      source,
                gpointer      task_data,
                GCancellable *cancellable)
{
    gpointer data;

    while (!g_cancellable_is_cancelled (cancellable) && (data = g_async_queue_pop (feeder_queue))) {

#if DEBUG_THREADS
    g_message ("Pop from feeder (Add) %i items in feeder", g_async_queue_length (feeder_queue));
#endif
        NolphinThumbnailInfo *feeder_info = (NolphinThumbnailInfo *) data;
        NolphinThumbnailInfo *existing_info = NULL;

        switch (feeder_info->cmd_type) {
            case THUMBNAIL_ADD:
                DEBUG ("(Add thumbnail) Locking mutex");
                g_mutex_lock (&thumbnails_mutex);
                existing_info = g_hash_table_lookup (thumbnails_to_make_hash, feeder_info->image_uri);

                if (existing_info == NULL) {
                    DEBUG ("(Main Thread) Adding new file to thumbnail: %s", feeder_info->image_uri);
#if DEBUG_THREADS
                    g_message ("%u unprocessed (Add)", g_thread_pool_unprocessed ((GThreadPool *) tpool));
#endif
                    g_hash_table_insert (thumbnails_to_make_hash, feeder_info->image_uri, feeder_info);
                    g_thread_pool_push ((GThreadPool *) tpool, feeder_info, NULL);

                    // Don't free this later.
                    feeder_info = NULL;
                } else {
                    DEBUG ("(Main Thread) Updating existing file mtime and prioritizing: %s", feeder_info->image_uri);

                    /* The file in the queue might need a new original mtime */
                    existing_info->original_file_mtime = feeder_info->original_file_mtime;
                    existing_info->add_time = g_get_monotonic_time ();
                    g_thread_pool_move_to_front ((GThreadPool *) tpool, existing_info);
                }
                DEBUG ("(Add thumbnail) Unlocking mutex");
                g_mutex_unlock (&thumbnails_mutex);
                break;
            case THUMBNAIL_REMOVE:
                if (!thumbnails_to_make_hash)
                    break;

                DEBUG ("(Remove from queue) Locking mutex");
                g_mutex_lock (&thumbnails_mutex);
                existing_info = g_hash_table_lookup (thumbnails_to_make_hash, feeder_info->image_uri);

                if (existing_info) {
                    DEBUG ("(Remove from queue) Removing %s", feeder_info->image_uri);
                    g_hash_table_remove (thumbnails_to_make_hash, feeder_info->image_uri);
                    existing_info->cancelled = TRUE;
                }
                DEBUG ("(Remove from queue) Unlocking mutex");
                g_mutex_unlock (&thumbnails_mutex);
                break;
            case THUMBNAIL_BUMP:
                if (!thumbnails_to_make_hash)
                    break;

                DEBUG ("(Prioritize) Locking mutex");
                g_mutex_lock (&thumbnails_mutex);
                existing_info = g_hash_table_lookup (thumbnails_to_make_hash, feeder_info->image_uri);

                if (existing_info) {
                    DEBUG ("(Prioritize) Moving to front: %s", feeder_info->image_uri);
                    existing_info->add_time = g_get_monotonic_time ();
                    g_thread_pool_move_to_front ((GThreadPool *) tpool, existing_info);
                }
                DEBUG ("(Prioritize) Unlocking mutex");
                g_mutex_unlock (&thumbnails_mutex);
                break;
            case THUMBNAIL_THREAD_EXIT:
                DEBUG ("(Finalize) Received THUMBNAIL_THREAD_EXIT, cancelling");
                g_cancellable_cancel (cancellable);
                break;
        }

        g_clear_pointer (&feeder_info, free_thumbnail_info);
    }

    g_task_return_boolean (task, TRUE);
}

static void
finalize_thumbnailer (void)
{
    NolphinThumbnailInfo *info;
    gpointer data;

    if (feeder_queue == NULL)
        return;

    DEBUG ("(Finalize) Shutdown thumbnailer.");

    info = g_new0 (NolphinThumbnailInfo, 1);
    info->cmd_type = THUMBNAIL_THREAD_EXIT;

    g_async_queue_push (feeder_queue, info);

    while (!g_task_get_completed (feeder_task)) {
        gtk_main_iteration ();
    }

    while ((data = g_async_queue_try_pop (feeder_queue))) {
        if (!data) {
            break;
        }
        NolphinThumbnailInfo *existing_info = (NolphinThumbnailInfo *) data;
        free_thumbnail_info (existing_info);
    }

    g_object_unref (feeder_task);
    g_async_queue_unref (feeder_queue);

    // This will drain and free any remaining infos.
    g_thread_pool_free ((GThreadPool *) tpool, FALSE, TRUE);

    g_hash_table_destroy (thumbnails_to_make_hash);
}

/* Mainloop */
void
nolphin_create_thumbnail (NolphinFile *file)
{
    time_t file_mtime = 0;
    static gsize once_init = 0;
    if (g_once_init_enter (&once_init)) {
        thumbnails_to_make_hash = g_hash_table_new (g_str_hash, g_str_equal);
        DEBUG ("Initialize thread pool");

        tpool = g_thread_pool_new ((GFunc) thumbnail_thread, NULL,
                                   get_max_threads (),
                                   FALSE, NULL);
        g_thread_pool_set_sort_function ((GThreadPool *) tpool, (GCompareDataFunc) lifo_sorter, NULL);

        feeder_queue = g_async_queue_new ();
        cancellable = g_cancellable_new ();
        feeder_task = g_task_new (NULL, cancellable, feeder_task_complete, NULL);
        g_task_run_in_thread (feeder_task, feeder_thread);

        eel_debug_call_at_shutdown ((EelFunction) finalize_thumbnailer);

        g_once_init_leave (&once_init, 1);
    }

    /* The gdk-pixbuf-thumbnailer tool has special hardcoded handling for recent: and trash: uris.
     * we need to find the activation uri here instead */
    if (nolphin_file_is_in_favorites (file)) {
        NolphinFile *real_file;
        gchar *uri;

        uri = nolphin_file_get_symbolic_link_target_uri (file);

        if (uri != NULL) {
            real_file = nolphin_file_get_by_uri (uri);
            if (real_file != NULL) {
                nolphin_create_thumbnail (real_file);
                nolphin_file_unref (real_file);
            }

            g_free (uri);
        }

        return;
    }

    gchar *file_uri = nolphin_file_get_uri (file);

    /* Hopefully the NolphinFile will already have the image file mtime,
       so we can just use that. Otherwise we have to get it ourselves. */
    if (file->details->got_file_info &&
        file->details->file_info_is_up_to_date &&
        file->details->mtime != 0) {
        file_mtime = file->details->mtime;
    } else {
        get_file_mtime (file_uri, &file_mtime);
    }

    NolphinThumbnailInfo *info;
    info = g_new0 (NolphinThumbnailInfo, 1);
    info->image_uri = file_uri;
    info->mime_type = nolphin_file_get_mime_type (file);
    info->original_file_mtime = file_mtime;
    info->add_time = g_get_monotonic_time ();
    info->cmd_type = THUMBNAIL_ADD;

    nolphin_file_set_is_thumbnailing (file, TRUE);

#if DEBUG_THREADS
    g_message ("Push to feeder (Add) %i items in feeder", g_async_queue_length (feeder_queue));
#endif

    g_async_queue_push (feeder_queue, info);
}

/* Mainloop */
void
nolphin_thumbnail_remove_from_queue (const char *file_uri)
{
    if (feeder_queue == NULL)
        return;

    NolphinThumbnailInfo *info;

    info = g_new0 (NolphinThumbnailInfo, 1);
    info->image_uri = g_strdup (file_uri);
    info->cmd_type = THUMBNAIL_REMOVE;

#if DEBUG_THREADS
    g_message ("Push to feeder (Remove) %i items in feeder", g_async_queue_length (feeder_queue));
#endif

    g_async_queue_push (feeder_queue, info);
}

/* Mainloop */
void
nolphin_thumbnail_prioritize (const char *file_uri)
{
    if (feeder_queue == NULL)
        return;

    NolphinThumbnailInfo *info;

    info = g_new0 (NolphinThumbnailInfo, 1);
    info->image_uri = g_strdup (file_uri);
    info->cmd_type = THUMBNAIL_BUMP;

#if DEBUG_THREADS
    g_message ("Push to feeder (Bump) %i items in feeder", g_async_queue_length (feeder_queue));
#endif

    g_async_queue_push (feeder_queue, info);
}

gboolean
nolphin_can_thumbnail_internally (NolphinFile *file)
{
    g_autofree gchar *mime_type = NULL;

    mime_type = nolphin_file_get_mime_type (file);
    return pixbuf_can_load_type (mime_type);
}

gboolean
nolphin_can_thumbnail (NolphinFile *file)
{
    GnomeDesktopThumbnailFactory *factory;
    g_autofree gchar *mime_type = NULL;
    g_autofree gchar *uri = NULL;
    time_t mtime;
    gboolean res;

    uri = nolphin_file_get_uri (file);
    mime_type = nolphin_file_get_mime_type (file);
    mtime = nolphin_file_get_mtime (file);
    
    factory = get_thumbnail_factory ();
    res = gnome_desktop_thumbnail_factory_can_thumbnail (factory,
                                                         uri,
                                                         mime_type,
                                                         mtime);
    if (!res && !nolphin_file_is_directory (file)) {
        res = can_thumbnail_cad_fallback (uri, mime_type);
    }
    return res;
}


void
nolphin_thumbnail_frame_image (GdkPixbuf **pixbuf)
{
    GdkPixbuf *pixbuf_with_frame, *frame;

    /* The pixbuf isn't already framed (i.e., it was not made by
     * an old Nolphin), so we must embed it in a frame.
     */

    frame = nolphin_get_thumbnail_frame ();
    if (frame == NULL) {
        return;
    }

    pixbuf_with_frame = eel_embed_image_in_frame (*pixbuf, frame,
                                                  NOLPHIN_THUMBNAIL_FRAME_LEFT,
                                                  NOLPHIN_THUMBNAIL_FRAME_TOP,
                                                  NOLPHIN_THUMBNAIL_FRAME_RIGHT,
                                                  NOLPHIN_THUMBNAIL_FRAME_BOTTOM);
    g_object_unref (*pixbuf);
    *pixbuf = pixbuf_with_frame;
}

void
nolphin_thumbnail_pad_top_and_bottom (GdkPixbuf **pixbuf,
                                   gint        extra_height)
{
    GdkPixbuf *pixbuf_with_padding;
    GdkRectangle rect;
    GdkRGBA transparent = { 0, 0, 0, 0.0 };
    cairo_surface_t *surface;
    cairo_t *cr;
    gint width, height;

    width = gdk_pixbuf_get_width (*pixbuf);
    height = gdk_pixbuf_get_height (*pixbuf);

    surface = gdk_window_create_similar_image_surface (NULL,
                                                       CAIRO_FORMAT_ARGB32,
                                                       width,
                                                       height + extra_height,
                                                       0);

    cr = cairo_create (surface);

    rect.x = 0;
    rect.y = 0;
    rect.width = width;
    rect.height = height + extra_height;

    gdk_cairo_rectangle (cr, &rect);
    gdk_cairo_set_source_rgba (cr, &transparent);
    cairo_fill (cr);

    gdk_cairo_set_source_pixbuf (cr,
                                 *pixbuf,
                                 0,
                                 extra_height / 2);
    cairo_paint (cr);

    pixbuf_with_padding = gdk_pixbuf_get_from_surface (surface,
                                                       0,
                                                       0,
                                                       width,
                                                       height + extra_height);

    g_object_unref (*pixbuf);
    cairo_surface_destroy (surface);
    cairo_destroy (cr);

    *pixbuf = pixbuf_with_padding;
}

gboolean
nolphin_thumbnail_factory_check_status (void)
{
    return gnome_desktop_thumbnail_cache_check_permissions (get_thumbnail_factory (), TRUE);
}
