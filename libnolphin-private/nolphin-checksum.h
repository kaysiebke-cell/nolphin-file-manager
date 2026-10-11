/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nolphin-checksum.h: file checksum computation
 *
 * MD5/SHA-1/SHA-256/SHA-512 via GChecksum (read the file in chunks
 * through GIO, so it works for any GVFS backend, not just local
 * files). BLAKE2 via the system's b2sum, since GChecksum doesn't
 * support it - this one needs a local path and fails cleanly with
 * NOLPHIN_CHECKSUM_ERROR_TOOL_NOT_FOUND if b2sum isn't installed, or
 * NOLPHIN_CHECKSUM_ERROR_REMOTE_FILE for a non-local GFile.
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

#ifndef NOLPHIN_CHECKSUM_H
#define NOLPHIN_CHECKSUM_H

#include <gio/gio.h>

G_BEGIN_DECLS

typedef enum {
    NOLPHIN_CHECKSUM_MD5,
    NOLPHIN_CHECKSUM_SHA1,
    NOLPHIN_CHECKSUM_SHA256,
    NOLPHIN_CHECKSUM_SHA512,
    NOLPHIN_CHECKSUM_BLAKE2
} NolphinChecksumType;

#define NOLPHIN_CHECKSUM_ERROR (nolphin_checksum_error_quark ())
GQuark nolphin_checksum_error_quark (void);

typedef enum {
    NOLPHIN_CHECKSUM_ERROR_TOOL_NOT_FOUND,
    NOLPHIN_CHECKSUM_ERROR_REMOTE_FILE,
    NOLPHIN_CHECKSUM_ERROR_TOOL_FAILED
} NolphinChecksumError;

/* "MD5", "SHA-1", "SHA-256", "SHA-512", "BLAKE2" */
const gchar *nolphin_checksum_type_get_label (NolphinChecksumType type);

/* MD5/SHA* are always available (built into GLib). BLAKE2 needs
 * b2sum on PATH - checked with g_find_program_in_path(), not assumed. */
gboolean     nolphin_checksum_type_is_available (NolphinChecksumType type);

/* Computes @file's checksum in a background thread; never blocks the
 * caller. Cancelling @cancellable stops an in-progress read/subprocess
 * as soon as possible. */
void   nolphin_checksum_compute_async  (GFile                *file,
                                        NolphinChecksumType   type,
                                        GCancellable         *cancellable,
                                        GAsyncReadyCallback   callback,
                                        gpointer              user_data);

/* Returns the lower-case hex digest, or NULL with @error set. Free
 * the return value with g_free(). */
gchar *nolphin_checksum_compute_finish (GAsyncResult *result, GError **error);

/* Case-insensitive comparison of a computed digest against a
 * user-entered one, tolerating surrounding whitespace. */
gboolean nolphin_checksum_matches (const gchar *computed, const gchar *expected);

G_END_DECLS

#endif /* NOLPHIN_CHECKSUM_H */
