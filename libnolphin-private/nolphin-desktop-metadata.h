/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Nolphin
 *
 * Copyright (C) 2011 Red Hat, Inc.
 *
 * Nolphin is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nolphin is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; see the file COPYING.  If not,
 * write to the Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
 * Boston, MA 02110-1335, USA.
 *
 * Authors: Cosimo Cecchi <cosimoc@redhat.com>
 */

#ifndef __NOLPHIN_DESKTOP_METADATA_H__
#define __NOLPHIN_DESKTOP_METADATA_H__

#include <glib.h>

#include <libnolphin-private/nolphin-file.h>

void nolphin_desktop_metadata_init (void);
void nolphin_desktop_set_metadata_string (NolphinFile *file,
                                           const gchar *name,
                                           const gchar *key,
                                           const gchar *string);

void nolphin_desktop_set_metadata_stringv (NolphinFile *file,
                                            const char *name,
                                            const char *key,
                                            const char * const *stringv);

void nolphin_desktop_clear_metadata       (NolphinFile *file);

gchar *nolphin_desktop_get_metadata_string (NolphinFile *file,
                                         const gchar *name,
                                         const gchar *key);

gchar **nolphin_desktop_get_metadata_stringv (NolphinFile *file,
                                           const char *name,
                                           const char *key);

gboolean nolphin_desktop_update_metadata_from_keyfile (NolphinFile *file,
                                                        const gchar *name);

#endif /* __NOLPHIN_DESKTOP_METADATA_H__ */
