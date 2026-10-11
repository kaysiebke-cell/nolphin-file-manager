/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Copyright (C) 2005 Mr Jamie McCracken
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
 * Author: Jamie McCracken (jamiemcc@gnome.org)
 *
 */

#ifndef NOLPHIN_SEARCH_ENGINE_TRACKER_H
#define NOLPHIN_SEARCH_ENGINE_TRACKER_H

#include <libnolphin-private/nolphin-search-engine.h>

#define NOLPHIN_TYPE_SEARCH_ENGINE_TRACKER		(nolphin_search_engine_tracker_get_type ())
#define NOLPHIN_SEARCH_ENGINE_TRACKER(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), NOLPHIN_TYPE_SEARCH_ENGINE_TRACKER, NolphinSearchEngineTracker))
#define NOLPHIN_SEARCH_ENGINE_TRACKER_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), NOLPHIN_TYPE_SEARCH_ENGINE_TRACKER, NolphinSearchEngineTrackerClass))
#define NOLPHIN_IS_SEARCH_ENGINE_TRACKER(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), NOLPHIN_TYPE_SEARCH_ENGINE_TRACKER))
#define NOLPHIN_IS_SEARCH_ENGINE_TRACKER_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), NOLPHIN_TYPE_SEARCH_ENGINE_TRACKER))
#define NOLPHIN_SEARCH_ENGINE_TRACKER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), NOLPHIN_TYPE_SEARCH_ENGINE_TRACKER, NolphinSearchEngineTrackerClass))

typedef struct NolphinSearchEngineTrackerDetails NolphinSearchEngineTrackerDetails;

typedef struct NolphinSearchEngineTracker {
	NolphinSearchEngine parent;
	NolphinSearchEngineTrackerDetails *details;
} NolphinSearchEngineTracker;

typedef struct {
	NolphinSearchEngineClass parent_class;
} NolphinSearchEngineTrackerClass;

GType nolphin_search_engine_tracker_get_type (void);

NolphinSearchEngine* nolphin_search_engine_tracker_new (void);

#endif /* NOLPHIN_SEARCH_ENGINE_TRACKER_H */
