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

#ifndef NOLPHIN_SEARCH_ENGINE_H
#define NOLPHIN_SEARCH_ENGINE_H

#include <glib-object.h>
#include <libnolphin-private/nolphin-query.h>

#define NOLPHIN_TYPE_SEARCH_ENGINE		(nolphin_search_engine_get_type ())
#define NOLPHIN_SEARCH_ENGINE(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), NOLPHIN_TYPE_SEARCH_ENGINE, NolphinSearchEngine))
#define NOLPHIN_SEARCH_ENGINE_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), NOLPHIN_TYPE_SEARCH_ENGINE, NolphinSearchEngineClass))
#define NOLPHIN_IS_SEARCH_ENGINE(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), NOLPHIN_TYPE_SEARCH_ENGINE))
#define NOLPHIN_IS_SEARCH_ENGINE_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), NOLPHIN_TYPE_SEARCH_ENGINE))
#define NOLPHIN_SEARCH_ENGINE_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), NOLPHIN_TYPE_SEARCH_ENGINE, NolphinSearchEngineClass))

typedef struct NolphinSearchEngineDetails NolphinSearchEngineDetails;

typedef struct NolphinSearchEngine {
	GObject parent;
	NolphinSearchEngineDetails *details;
} NolphinSearchEngine;

typedef struct {
	GObjectClass parent_class;
	
	/* VTable */
	void (*set_query) (NolphinSearchEngine *engine, NolphinQuery *query);
	void (*start) (NolphinSearchEngine *engine);
	void (*stop) (NolphinSearchEngine *engine);

	/* Signals */
	void (*hits_added) (NolphinSearchEngine *engine, GList *hit_infos);
	void (*hits_subtracted) (NolphinSearchEngine *engine, GList *hits);
	void (*finished) (NolphinSearchEngine *engine);
	void (*error) (NolphinSearchEngine *engine, const char *error_message);
} NolphinSearchEngineClass;

GType          nolphin_search_engine_get_type  (void);
gboolean       nolphin_search_engine_enabled (void);

NolphinSearchEngine* nolphin_search_engine_new       (void);

void           nolphin_search_engine_set_query (NolphinSearchEngine *engine, NolphinQuery *query);
void	       nolphin_search_engine_start (NolphinSearchEngine *engine);
void	       nolphin_search_engine_stop (NolphinSearchEngine *engine);

void	       nolphin_search_engine_hits_added (NolphinSearchEngine *engine, GList *hits);
void	       nolphin_search_engine_hits_subtracted (NolphinSearchEngine *engine, GList *hits);
void	       nolphin_search_engine_finished (NolphinSearchEngine *engine);
void	       nolphin_search_engine_error (NolphinSearchEngine *engine, const char *error_message);

typedef struct {
    gchar     *uri;           // The file uri;
    gchar     *snippet;          // List of hits.
    gint64     hits;
} FileSearchResult;

FileSearchResult *file_search_result_new     (gchar *uri, gchar *snippet);
void              file_search_result_free    (FileSearchResult *result);
void              file_search_result_add_hit (FileSearchResult *result);

gboolean       nolphin_search_engine_check_filename_pattern (NolphinQuery   *query,
                                                          GError     **error);
gboolean       nolphin_search_engine_check_content_pattern  (NolphinQuery   *query,
                                                          GError     **error);

void              nolphin_search_engine_report_accounting (void);
#endif /* NOLPHIN_SEARCH_ENGINE_H */
