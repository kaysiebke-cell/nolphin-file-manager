/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nolphin-trash-cleanup.h: automatic Trash retention + size-limit
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

#ifndef NOLPHIN_TRASH_CLEANUP_H
#define NOLPHIN_TRASH_CLEANUP_H

#include <gio/gio.h>

G_BEGIN_DECLS

/* Pure decision function, deliberately exposed on its own so it can
 * be unit-tested in complete isolation from any real Trash content
 * (trash:// aggregates every mounted volume's Trash for the current
 * UID regardless of $HOME, so it cannot be safely sandboxed for a
 * destructive test - see nolphin_trash_cleanup_purge_expired_async()).
 *
 * @deletion_date_iso8601: value of a Trash item's
 *   trash::deletion-date attribute, or NULL if the attribute is
 *   missing (then always returns FALSE - a missing date never counts
 *   as "expired").
 * @retention_days: <= 0 always returns FALSE (auto-cleanup off).
 * @now: reference "current time" (UTC). Pass a fixed value in tests
 *   for determinism; real callers pass a freshly-taken current time.
 */
gboolean nolphin_trash_cleanup_is_expired (const gchar *deletion_date_iso8601,
                                           gint          retention_days,
                                           GDateTime    *now);

/* Total size in bytes of everything under @root, recursing into
 * directories. @root may be NULL, meaning trash:/// (the real Trash).
 * Passing an ordinary local directory instead - as the automated test
 * does - exercises the exact same recursive-enumeration code path
 * without touching Trash at all. */
void    nolphin_trash_cleanup_get_total_size_async  (GFile                *root,
                                                      GCancellable         *cancellable,
                                                      GAsyncReadyCallback   callback,
                                                      gpointer              user_data);
goffset nolphin_trash_cleanup_get_total_size_finish (GAsyncResult *result, GError **error);

/* Permanently deletes every item directly under @root (NULL = the
 * real trash:///) whose trash::deletion-date makes
 * nolphin_trash_cleanup_is_expired() return TRUE for @retention_days.
 * @retention_days <= 0 means "never auto-delete" - Trash is not even
 * read in that case. Returns (via finish) how many items were
 * actually deleted.
 *
 * There is deliberately no automated test exercising this function
 * end-to-end against the real trash:// backend: trash:// cannot be
 * safely sandboxed per-process (see above), so a live test would risk
 * deleting real user Trash content. Its correctness rests on
 * nolphin_trash_cleanup_is_expired() (exhaustively unit-tested) and
 * nolphin_trash_cleanup_get_total_size_async()'s enumeration code
 * (tested against a real directory) - this function itself is a thin
 * combination of both plus g_file_delete(). */
void   nolphin_trash_cleanup_purge_expired_async  (GFile                *root,
                                                    gint                  retention_days,
                                                    GCancellable         *cancellable,
                                                    GAsyncReadyCallback   callback,
                                                    gpointer              user_data);
guint  nolphin_trash_cleanup_purge_expired_finish (GAsyncResult *result, GError **error);

G_END_DECLS

#endif /* NOLPHIN_TRASH_CLEANUP_H */
