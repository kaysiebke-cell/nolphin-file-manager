/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nolphin-archive.h: abstracted archive backend (create/extract/test)
 *
 * Drives the system's zip/tar/7z tools through GSubprocess - argv arrays
 * only, never a shell string - after checking each is actually installed.
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

#ifndef NOLPHIN_ARCHIVE_H
#define NOLPHIN_ARCHIVE_H

#include <gio/gio.h>

G_BEGIN_DECLS

/* Erstellung & Komprimierung (create_tool gesetzt) danach
 * Entpacken/Nur-Lesen (create_tool == NULL - kein vorgetaeuschtes
 * "Erstellen", das die zugrundeliegenden Werkzeuge nicht koennen). */
typedef enum {
    NOLPHIN_ARCHIVE_FORMAT_ZIP,
    NOLPHIN_ARCHIVE_FORMAT_TAR,
    NOLPHIN_ARCHIVE_FORMAT_TAR_GZ,
    NOLPHIN_ARCHIVE_FORMAT_TAR_BZ2,
    NOLPHIN_ARCHIVE_FORMAT_TAR_XZ,
    NOLPHIN_ARCHIVE_FORMAT_TAR_ZST,
    NOLPHIN_ARCHIVE_FORMAT_TAR_LZ4,
    NOLPHIN_ARCHIVE_FORMAT_SEVEN_ZIP,
    NOLPHIN_ARCHIVE_FORMAT_GZ,   /* Einzeldatei, kein Tarball */
    NOLPHIN_ARCHIVE_FORMAT_BZ2,
    NOLPHIN_ARCHIVE_FORMAT_XZ,
    NOLPHIN_ARCHIVE_FORMAT_ZST,
    NOLPHIN_ARCHIVE_FORMAT_LZ4,
    NOLPHIN_ARCHIVE_FORMAT_TAR_LZ,
    NOLPHIN_ARCHIVE_FORMAT_TAR_LZMA,
    NOLPHIN_ARCHIVE_FORMAT_TAR_LZO,
    NOLPHIN_ARCHIVE_FORMAT_TAR_Z,
    NOLPHIN_ARCHIVE_FORMAT_LZ,
    NOLPHIN_ARCHIVE_FORMAT_LZMA,
    NOLPHIN_ARCHIVE_FORMAT_LZO,
    NOLPHIN_ARCHIVE_FORMAT_JAR,   /* ZIP-Varianten */
    NOLPHIN_ARCHIVE_FORMAT_WAR,
    NOLPHIN_ARCHIVE_FORMAT_EAR,
    NOLPHIN_ARCHIVE_FORMAT_EPUB,
    NOLPHIN_ARCHIVE_FORMAT_CBZ,
    NOLPHIN_ARCHIVE_FORMAT_AR,
    NOLPHIN_ARCHIVE_FORMAT_TAR_7Z,
    NOLPHIN_ARCHIVE_FORMAT_EXE,   /* selbstentpackendes 7z-Archiv */
    NOLPHIN_ARCHIVE_FORMAT_CRX,   /* ZIP-Variante */
    NOLPHIN_ARCHIVE_FORMAT_RAR,  /* ab hier: (überwiegend) nur entpacken/lesen; ISO ist zusätzlich erstellbar */
    NOLPHIN_ARCHIVE_FORMAT_CAB,
    NOLPHIN_ARCHIVE_FORMAT_ARJ,
    NOLPHIN_ARCHIVE_FORMAT_LZH,
    NOLPHIN_ARCHIVE_FORMAT_ISO,
    NOLPHIN_ARCHIVE_FORMAT_CPIO,
    NOLPHIN_ARCHIVE_FORMAT_RPM,
    NOLPHIN_ARCHIVE_FORMAT_DEB,
    NOLPHIN_ARCHIVE_FORMAT_UNKNOWN
} NolphinArchiveFormat;

/* TRUE für die "Erstellung & Komprimierung"-Formate, FALSE für
 * die "Entpacken / Nur Lesen"-Formate (RAR bis DEB) - die Oberfläche
 * nutzt das, um im Format-Dropdown beim Erstellen nur die erstellbaren
 * Formate anzubieten. */
gboolean     nolphin_archive_format_can_create (NolphinArchiveFormat format);

/* Formate, die genau eine Datei komprimieren (kein Tar-Container):
 * GZ/BZ2/XZ/ZST/LZ4. nolphin_archive_compress_async() verlangt dafür
 * genau eine Quelle. */
gboolean     nolphin_archive_format_is_single_file (NolphinArchiveFormat format);

#define NOLPHIN_ARCHIVE_ERROR (nolphin_archive_error_quark ())
GQuark nolphin_archive_error_quark (void);

typedef enum {
    NOLPHIN_ARCHIVE_ERROR_TOOL_NOT_FOUND,
    NOLPHIN_ARCHIVE_ERROR_UNKNOWN_FORMAT,
    NOLPHIN_ARCHIVE_ERROR_TOOL_FAILED
} NolphinArchiveError;

/* Human-readable label ("ZIP", "7-Zip", ...) and the tool this format
 * needs (e.g. "zip", "7z") - the latter is what
 * nolphin_archive_format_is_available() checks for with
 * g_find_program_in_path(). */
const gchar *nolphin_archive_format_get_label (NolphinArchiveFormat format);
const gchar *nolphin_archive_format_get_extension (NolphinArchiveFormat format);
gboolean     nolphin_archive_format_is_available (NolphinArchiveFormat format);

/* Nur ZIP und 7-Zip koennen als Werkzeug selbst Passwort-Verschluesselung
 * bzw. Teilarchive (Mehrbaendigkeit) - die TAR-Varianten schlicht nicht,
 * das ist keine Nolphin-Einschraenkung. Die Oberflaeche fragt hier ab,
 * um die jeweiligen Felder auszugrauen statt eine vorgetaeuschte
 * Funktion anzubieten. */
gboolean     nolphin_archive_format_supports_password (NolphinArchiveFormat format);
gboolean     nolphin_archive_format_supports_split     (NolphinArchiveFormat format);

/* Guess the format from an archive's filename. FORMAT_UNKNOWN if no
 * known extension matches. */
NolphinArchiveFormat nolphin_archive_detect_format (GFile *archive_file);

/* Compress @sources (all siblings in the same directory) into a new
 * archive at @destination in @format. Fails with
 * NOLPHIN_ARCHIVE_ERROR_TOOL_NOT_FOUND if the required tool isn't
 * installed, checked up front before spawning anything.
 *
 * @password: NULL/"" for an unencrypted archive; otherwise a
 * password only ZIP and 7-Zip support (nolphin_archive_format_supports_
 * password()) - fails with NOLPHIN_ARCHIVE_ERROR_UNKNOWN_FORMAT for any
 * other format rather than silently ignoring it. Handed to the
 * underlying zip/7z tool as a plain argv entry, since neither tool
 * offers a way to read it from a pipe/fd - like every other GUI archive
 * tool built on them, it is therefore briefly visible in this process's
 * own argv (e.g. via /proc or `ps`) while the tool runs. Nolphin itself
 * never stores or logs it.
 *
 * @split_size_mb: 0 for a single archive file; otherwise the per-volume
 * size in MiB for a multi-volume archive - only ZIP and 7-Zip support
 * this (nolphin_archive_format_supports_split()), same failure mode as
 * an unsupported password otherwise. */
void     nolphin_archive_compress_async  (GList               *sources,
                                          GFile               *destination,
                                          NolphinArchiveFormat  format,
                                          const gchar          *password,
                                          guint                 split_size_mb,
                                          GCancellable        *cancellable,
                                          GAsyncReadyCallback   callback,
                                          gpointer              user_data);
gboolean nolphin_archive_compress_finish (GAsyncResult *result, GError **error);

/* Extract @archive_file into @destination_dir (must already exist). */
void     nolphin_archive_extract_async   (GFile               *archive_file,
                                          GFile               *destination_dir,
                                          GCancellable        *cancellable,
                                          GAsyncReadyCallback   callback,
                                          gpointer              user_data);
gboolean nolphin_archive_extract_finish  (GAsyncResult *result, GError **error);

/* Verify @archive_file's integrity without extracting it anywhere. */
void     nolphin_archive_test_async      (GFile               *archive_file,
                                          GCancellable        *cancellable,
                                          GAsyncReadyCallback   callback,
                                          gpointer              user_data);
gboolean nolphin_archive_test_finish     (GAsyncResult *result, GError **error);

G_END_DECLS

#endif /* NOLPHIN_ARCHIVE_H */
