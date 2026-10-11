/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Copyright (C) 2005 Novell, Inc.
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
 * Author: Anders Carlsson <andersca@imendio.com>
 *
 */

#ifndef NOLPHIN_QUERY_H
#define NOLPHIN_QUERY_H

#include <glib-object.h>

#define NOLPHIN_TYPE_QUERY		(nolphin_query_get_type ())
#define NOLPHIN_QUERY(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), NOLPHIN_TYPE_QUERY, NolphinQuery))
#define NOLPHIN_QUERY_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), NOLPHIN_TYPE_QUERY, NolphinQueryClass))
#define NOLPHIN_IS_QUERY(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), NOLPHIN_TYPE_QUERY))
#define NOLPHIN_IS_QUERY_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), NOLPHIN_TYPE_QUERY))
#define NOLPHIN_QUERY_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), NOLPHIN_TYPE_QUERY, NolphinQueryClass))

typedef struct NolphinQueryDetails NolphinQueryDetails;

typedef struct NolphinQuery {
	GObject parent;
	NolphinQueryDetails *details;
} NolphinQuery;

typedef struct {
	GObjectClass parent_class;
} NolphinQueryClass;

GType          nolphin_query_get_type (void);
gboolean       nolphin_query_enabled  (void);

NolphinQuery* nolphin_query_new      (void);

char *         nolphin_query_get_file_pattern (NolphinQuery *query);
void           nolphin_query_set_file_pattern (NolphinQuery *query, const char *text);

char *         nolphin_query_get_content_pattern (NolphinQuery *query);
void           nolphin_query_set_content_pattern (NolphinQuery *query, const char *text);
gboolean       nolphin_query_has_content_pattern (NolphinQuery *query);

char *         nolphin_query_get_location       (NolphinQuery *query);
void           nolphin_query_set_location       (NolphinQuery *query, const char *uri);

GList *        nolphin_query_get_mime_types     (NolphinQuery *query);
void           nolphin_query_set_mime_types     (NolphinQuery *query, GList *mime_types);
void           nolphin_query_add_mime_type      (NolphinQuery *query, const char *mime_type);

void           nolphin_query_set_show_hidden    (NolphinQuery *query, gboolean hidden);
gboolean       nolphin_query_get_show_hidden    (NolphinQuery *query);

gboolean       nolphin_query_get_file_case_sensitive (NolphinQuery *query);
void           nolphin_query_set_file_case_sensitive (NolphinQuery *query, gboolean case_sensitive);

gboolean       nolphin_query_get_content_case_sensitive (NolphinQuery *query);
void           nolphin_query_set_content_case_sensitive (NolphinQuery *query, gboolean case_sensitive);

gboolean       nolphin_query_get_use_file_regex      (NolphinQuery *query);
void           nolphin_query_set_use_file_regex      (NolphinQuery *query, gboolean file_use_regex);

gboolean       nolphin_query_get_use_content_regex      (NolphinQuery *query);
void           nolphin_query_set_use_content_regex      (NolphinQuery *query, gboolean content_use_regex);

gboolean       nolphin_query_get_recurse         (NolphinQuery *query);
void           nolphin_query_set_recurse         (NolphinQuery *query, gboolean recurse);

char *         nolphin_query_to_readable_string (NolphinQuery *query);
NolphinQuery *nolphin_query_load               (char *file);
gboolean       nolphin_query_save               (NolphinQuery *query, char *file);

#endif /* NOLPHIN_QUERY_H */
