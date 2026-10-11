/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
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
 */

#ifndef NOLPHIN_SEARCH_ENGINE_ADVANCED_H
#define NOLPHIN_SEARCH_ENGINE_ADVANCED_H

#include <libnolphin-private/nolphin-search-engine.h>

#define NOLPHIN_TYPE_SEARCH_ENGINE_ADVANCED		(nolphin_search_engine_advanced_get_type ())
#define NOLPHIN_SEARCH_ENGINE_ADVANCED(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), NOLPHIN_TYPE_SEARCH_ENGINE_ADVANCED, NolphinSearchEngineAdvanced))
#define NOLPHIN_SEARCH_ENGINE_ADVANCED_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), NOLPHIN_TYPE_SEARCH_ENGINE_ADVANCED, NolphinSearchEngineAdvancedClass))
#define NOLPHIN_IS_SEARCH_ENGINE_SIMPLE(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), NOLPHIN_TYPE_SEARCH_ENGINE_ADVANCED))
#define NOLPHIN_IS_SEARCH_ENGINE_SIMPLE_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), NOLPHIN_TYPE_SEARCH_ENGINE_ADVANCED))
#define NOLPHIN_SEARCH_ENGINE_ADVANCED_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), NOLPHIN_TYPE_SEARCH_ENGINE_ADVANCED, NolphinSearchEngineAdvancedClass))

typedef struct NolphinSearchEngineAdvancedDetails NolphinSearchEngineAdvancedDetails;

typedef struct NolphinSearchEngineAdvanced {
	NolphinSearchEngine parent;
	NolphinSearchEngineAdvancedDetails *details;
} NolphinSearchEngineAdvanced;

typedef struct {
	NolphinSearchEngineClass parent_class;
} NolphinSearchEngineAdvancedClass;

GType          nolphin_search_engine_advanced_get_type  (void);

NolphinSearchEngine* nolphin_search_engine_advanced_new       (void);
void           free_search_helpers (void);

gboolean nolphin_search_engine_advanced_check_filename_pattern (NolphinQuery   *query,
                                                             GError     **error);
gboolean nolphin_search_engine_advanced_check_content_pattern  (NolphinQuery *query,
                                                             GError   **error);
#endif /* NOLPHIN_SEARCH_ENGINE_ADVANCED_H */
