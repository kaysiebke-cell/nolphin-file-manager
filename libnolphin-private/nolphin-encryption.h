/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nolphin-encryption.h: gpg-based file encryption
 *
 * Symmetric (passphrase-based) encryption via the system's gpg -
 * no key management, matching the common "encrypt this file with a
 * password" file-manager workflow. The passphrase is fed to gpg
 * through its stdin pipe (--passphrase-fd 0), never as a command-
 * line argument, so it never appears in `ps`/process listings.
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

#ifndef NOLPHIN_ENCRYPTION_H
#define NOLPHIN_ENCRYPTION_H

#include <gio/gio.h>

G_BEGIN_DECLS

#define NOLPHIN_ENCRYPTION_ERROR (nolphin_encryption_error_quark ())
GQuark nolphin_encryption_error_quark (void);

typedef enum {
    NOLPHIN_ENCRYPTION_ERROR_TOOL_NOT_FOUND,
    NOLPHIN_ENCRYPTION_ERROR_REMOTE_FILE,
    /* Covers a wrong passphrase too - gpg's own stderr (included in
     * the GError message) says why in whatever language gpg itself
     * reports in; not re-parsed/re-guessed here to avoid locale- and
     * version-specific string matching. */
    NOLPHIN_ENCRYPTION_ERROR_TOOL_FAILED
} NolphinEncryptionError;

gboolean nolphin_encryption_is_available (void);

/* Encrypts @source into a new file at @destination (typically
 * @source's name with ".gpg" appended - the caller decides). Local
 * files only for both, checked up front. */
void     nolphin_encryption_encrypt_async  (GFile               *source,
                                            GFile               *destination,
                                            const gchar         *passphrase,
                                            GCancellable        *cancellable,
                                            GAsyncReadyCallback  callback,
                                            gpointer             user_data);
gboolean nolphin_encryption_encrypt_finish (GAsyncResult *result, GError **error);

/* Decrypts @source (a .gpg file) into @destination. A wrong
 * passphrase surfaces as NOLPHIN_ENCRYPTION_ERROR_TOOL_FAILED with
 * gpg's own explanation in the GError message. */
void     nolphin_encryption_decrypt_async  (GFile               *source,
                                            GFile               *destination,
                                            const gchar         *passphrase,
                                            GCancellable        *cancellable,
                                            GAsyncReadyCallback  callback,
                                            gpointer             user_data);
gboolean nolphin_encryption_decrypt_finish (GAsyncResult *result, GError **error);

G_END_DECLS

#endif /* NOLPHIN_ENCRYPTION_H */
