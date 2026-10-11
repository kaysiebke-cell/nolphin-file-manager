/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nolphin-acl.h: POSIX ACL viewing/editing via getfacl/setfacl
 *
 * Whether ACLs are actually supported for a given file is not
 * pre-guessed - nolphin_acl_get_entries_async() simply runs getfacl
 * and reports whatever it says, including a filesystem that doesn't
 * support ACLs at all (surfaced as a normal GError, not silently
 * hidden or faked as "no entries").
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

#ifndef NOLPHIN_ACL_H
#define NOLPHIN_ACL_H

#include <gio/gio.h>

G_BEGIN_DECLS

#define NOLPHIN_ACL_ERROR (nolphin_acl_error_quark ())
GQuark nolphin_acl_error_quark (void);

typedef enum {
    NOLPHIN_ACL_ERROR_TOOL_NOT_FOUND,
    NOLPHIN_ACL_ERROR_REMOTE_FILE,
    NOLPHIN_ACL_ERROR_TOOL_FAILED
} NolphinAclError;

typedef enum {
    NOLPHIN_ACL_ENTRY_USER_OWNER,  /* user::rwx - the file's own owner */
    NOLPHIN_ACL_ENTRY_USER,        /* user:NAME:rwx - a named user */
    NOLPHIN_ACL_ENTRY_GROUP_OWNER, /* group::rwx - the file's own group */
    NOLPHIN_ACL_ENTRY_GROUP,       /* group:NAME:rwx - a named group */
    NOLPHIN_ACL_ENTRY_MASK,        /* mask::rwx */
    NOLPHIN_ACL_ENTRY_OTHER        /* other::rwx */
} NolphinAclEntryType;

typedef struct {
    NolphinAclEntryType type;
    gchar    *qualifier; /* username/groupname for USER/GROUP entries, NULL otherwise */
    gboolean  can_read;
    gboolean  can_write;
    gboolean  can_execute;
} NolphinAclEntry;

void             nolphin_acl_entry_free      (NolphinAclEntry *entry);
void             nolphin_acl_entry_list_free (GList *entries);

gboolean nolphin_acl_is_available (void);

/* Lists every ACL entry currently on @file (base owner/group/other
 * plus any named user:/group: entries and the mask). */
void   nolphin_acl_get_entries_async  (GFile               *file,
                                       GCancellable        *cancellable,
                                       GAsyncReadyCallback  callback,
                                       gpointer             user_data);
/* Returns a newly-allocated GList of newly-allocated NolphinAclEntry*
 * (free with nolphin_acl_entry_list_free()), or NULL with @error set. */
GList *nolphin_acl_get_entries_finish (GAsyncResult *result, GError **error);

/* Adds or updates a named user/group entry (or the mask/other base
 * entries). @recursive applies -R for a directory. */
void     nolphin_acl_set_entry_async  (GFile                *file,
                                       NolphinAclEntryType   type,
                                       const gchar          *qualifier,
                                       gboolean              can_read,
                                       gboolean              can_write,
                                       gboolean              can_execute,
                                       gboolean              recursive,
                                       GCancellable         *cancellable,
                                       GAsyncReadyCallback   callback,
                                       gpointer              user_data);
gboolean nolphin_acl_set_entry_finish (GAsyncResult *result, GError **error);

/* Removes a named user/group entry (only USER/GROUP make sense here -
 * the base owner/group-owner/other/mask entries can't be removed,
 * only changed). */
void     nolphin_acl_remove_entry_async  (GFile                *file,
                                          NolphinAclEntryType   type,
                                          const gchar          *qualifier,
                                          gboolean              recursive,
                                          GCancellable         *cancellable,
                                          GAsyncReadyCallback   callback,
                                          gpointer              user_data);
gboolean nolphin_acl_remove_entry_finish (GAsyncResult *result, GError **error);

G_END_DECLS

#endif /* NOLPHIN_ACL_H */
