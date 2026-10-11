/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nolphin-trash-cleanup.c: automatic Trash retention + size-limit
 * warning
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

#include "nolphin-trash-cleanup.h"

#define TRASH_ENUM_ATTRIBUTES \
    G_FILE_ATTRIBUTE_STANDARD_NAME "," \
    G_FILE_ATTRIBUTE_STANDARD_TYPE "," \
    G_FILE_ATTRIBUTE_STANDARD_SIZE "," \
    G_FILE_ATTRIBUTE_TRASH_DELETION_DATE

gboolean
nolphin_trash_cleanup_is_expired (const gchar *deletion_date_iso8601,
                                  gint          retention_days,
                                  GDateTime    *now)
{
    GDateTime *deletion_date;
    GTimeSpan age_microseconds;
    gdouble age_days;
    gboolean expired;

    if (retention_days <= 0 || deletion_date_iso8601 == NULL) {
        return FALSE;
    }

    deletion_date = g_date_time_new_from_iso8601 (deletion_date_iso8601, NULL);
    if (deletion_date == NULL) {
        /* Unparseable date: never treat as "definitely expired". */
        return FALSE;
    }

    age_microseconds = g_date_time_difference (now, deletion_date);
    age_days = (gdouble) age_microseconds / (gdouble) G_TIME_SPAN_DAY;
    expired = age_days >= (gdouble) retention_days;

    g_date_time_unref (deletion_date);

    return expired;
}

static goffset
file_size_recursive (GFile *file, GFileInfo *info, GCancellable *cancellable)
{
    /* A directory's own reported size is its on-disk inode/block
     * size (e.g. 4096), not the size of its contents - only sum
     * actual (non-directory) entries, recursing into subdirectories
     * without adding their own size. */
    if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY) {
        GFileEnumerator *enumerator;
        goffset total = 0;

        enumerator = g_file_enumerate_children (file, TRASH_ENUM_ATTRIBUTES,
                                                G_FILE_QUERY_INFO_NONE, cancellable, NULL);
        if (enumerator != NULL) {
            GFileInfo *child_info;

            while ((child_info = g_file_enumerator_next_file (enumerator, cancellable, NULL)) != NULL) {
                GFile *child = g_file_enumerator_get_child (enumerator, child_info);
                total += file_size_recursive (child, child_info, cancellable);
                g_object_unref (child);
                g_object_unref (child_info);
            }
            g_file_enumerator_close (enumerator, NULL, NULL);
            g_object_unref (enumerator);
        }

        return total;
    }

    return g_file_info_get_size (info);
}

typedef struct {
    GFile *root; /* owned; if the caller passed NULL this holds trash:/// */
} RootThreadData;

static RootThreadData *
root_thread_data_new (GFile *root)
{
    RootThreadData *data = g_new0 (RootThreadData, 1);
    data->root = (root != NULL) ? g_object_ref (root) : g_file_new_for_uri ("trash:///");
    return data;
}

static void
root_thread_data_free (RootThreadData *data)
{
    g_clear_object (&data->root);
    g_free (data);
}

static void
get_total_size_thread (GTask *task, gpointer source_object, gpointer task_data, GCancellable *cancellable)
{
    RootThreadData *data = task_data;
    GFileEnumerator *enumerator;
    GError *error = NULL;
    goffset total = 0;
    GFileInfo *info;

    enumerator = g_file_enumerate_children (data->root, TRASH_ENUM_ATTRIBUTES,
                                            G_FILE_QUERY_INFO_NONE, cancellable, &error);

    if (enumerator == NULL) {
        g_task_return_error (task, error);
        return;
    }

    while ((info = g_file_enumerator_next_file (enumerator, cancellable, NULL)) != NULL) {
        GFile *child = g_file_enumerator_get_child (enumerator, info);
        total += file_size_recursive (child, info, cancellable);
        g_object_unref (child);
        g_object_unref (info);
    }

    g_file_enumerator_close (enumerator, NULL, NULL);
    g_object_unref (enumerator);

    g_task_return_int (task, (gssize) total);
}

void
nolphin_trash_cleanup_get_total_size_async (GFile                *root,
                                            GCancellable         *cancellable,
                                            GAsyncReadyCallback   callback,
                                            gpointer              user_data)
{
    GTask *task = g_task_new (NULL, cancellable, callback, user_data);
    g_task_set_source_tag (task, nolphin_trash_cleanup_get_total_size_async);
    g_task_set_task_data (task, root_thread_data_new (root), (GDestroyNotify) root_thread_data_free);
    g_task_run_in_thread (task, get_total_size_thread);
    g_object_unref (task);
}

goffset
nolphin_trash_cleanup_get_total_size_finish (GAsyncResult *result, GError **error)
{
    return (goffset) g_task_propagate_int (G_TASK (result), error);
}

typedef struct {
    GFile *root; /* owned; trash:/// if the caller passed NULL */
    gint   retention_days;
} PurgeThreadData;

static void
purge_thread_data_free (PurgeThreadData *data)
{
    g_clear_object (&data->root);
    g_free (data);
}

static void
purge_expired_thread (GTask *task, gpointer source_object, gpointer task_data, GCancellable *cancellable)
{
    PurgeThreadData *data = task_data;
    GFileEnumerator *enumerator;
    GError *error = NULL;
    guint deleted_count = 0;
    GDateTime *now;
    GFileInfo *info;

    if (data->retention_days <= 0) {
        g_task_return_int (task, 0);
        return;
    }

    enumerator = g_file_enumerate_children (data->root, TRASH_ENUM_ATTRIBUTES,
                                            G_FILE_QUERY_INFO_NONE, cancellable, &error);

    if (enumerator == NULL) {
        g_task_return_error (task, error);
        return;
    }

    now = g_date_time_new_now_utc ();

    while ((info = g_file_enumerator_next_file (enumerator, cancellable, NULL)) != NULL) {
        const gchar *deletion_date_str =
            g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_TRASH_DELETION_DATE);

        if (nolphin_trash_cleanup_is_expired (deletion_date_str, data->retention_days, now)) {
            GFile *child = g_file_enumerator_get_child (enumerator, info);

            /* Deleting a trash:// child (as opposed to trashing it
             * again) is how every GVfs-based file manager permanently
             * removes an item already in the Trash. */
            if (g_file_delete (child, cancellable, NULL)) {
                deleted_count++;
            }
            g_object_unref (child);
        }
        g_object_unref (info);
    }

    g_date_time_unref (now);
    g_file_enumerator_close (enumerator, NULL, NULL);
    g_object_unref (enumerator);

    g_task_return_int (task, (gssize) deleted_count);
}

void
nolphin_trash_cleanup_purge_expired_async (GFile                *root,
                                           gint                  retention_days,
                                           GCancellable         *cancellable,
                                           GAsyncReadyCallback   callback,
                                           gpointer              user_data)
{
    GTask *task;
    PurgeThreadData *data;

    task = g_task_new (NULL, cancellable, callback, user_data);
    g_task_set_source_tag (task, nolphin_trash_cleanup_purge_expired_async);

    data = g_new0 (PurgeThreadData, 1);
    data->root = (root != NULL) ? g_object_ref (root) : g_file_new_for_uri ("trash:///");
    data->retention_days = retention_days;
    g_task_set_task_data (task, data, (GDestroyNotify) purge_thread_data_free);

    g_task_run_in_thread (task, purge_expired_thread);
    g_object_unref (task);
}

guint
nolphin_trash_cleanup_purge_expired_finish (GAsyncResult *result, GError **error)
{
    return (guint) g_task_propagate_int (G_TASK (result), error);
}
