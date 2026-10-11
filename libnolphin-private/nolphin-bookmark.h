/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nolphin-bookmark.h - implementation of individual bookmarks.
 *
 * Copyright (C) 1999, 2000 Eazel, Inc.
 * Copyright (C) 2011, Red Hat, Inc.
 *
 * The Gnome Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * The Gnome Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with the Gnome Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
 * Boston, MA 02110-1335, USA.
 *
 * Authors: John Sullivan <sullivan@eazel.com>
 *          Cosimo Cecchi <cosimoc@redhat.com>
 */

#ifndef NOLPHIN_BOOKMARK_H
#define NOLPHIN_BOOKMARK_H

#include <gtk/gtk.h>
#include <gio/gio.h>
typedef struct NolphinBookmark NolphinBookmark;

#define NOLPHIN_TYPE_BOOKMARK nolphin_bookmark_get_type()
#define NOLPHIN_BOOKMARK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NOLPHIN_TYPE_BOOKMARK, NolphinBookmark))
#define NOLPHIN_BOOKMARK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NOLPHIN_TYPE_BOOKMARK, NolphinBookmarkClass))
#define NOLPHIN_IS_BOOKMARK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NOLPHIN_TYPE_BOOKMARK))
#define NOLPHIN_IS_BOOKMARK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NOLPHIN_TYPE_BOOKMARK))
#define NOLPHIN_BOOKMARK_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NOLPHIN_TYPE_BOOKMARK, NolphinBookmarkClass))

typedef struct NolphinBookmarkDetails NolphinBookmarkDetails;

struct NolphinBookmark {
	GObject object;
	NolphinBookmarkDetails *details;	
};

typedef struct
{
  gchar  *bookmark_name;
  gchar **emblems;
} NolphinBookmarkMetadata;

struct NolphinBookmarkClass {
	GObjectClass parent_class;

	/* Signals that clients can connect to. */

	/* The contents-changed signal is emitted when the bookmark's contents
	 * (custom name or URI) changed.
	 */
	void	(* contents_changed) (NolphinBookmark *bookmark);

    gboolean (* location_mounted)         (GFile *location);
};

typedef struct NolphinBookmarkClass NolphinBookmarkClass;

GType                 nolphin_bookmark_get_type               (void);
NolphinBookmark *    nolphin_bookmark_new                    (GFile                *location,
                                                        const char           *custom_name,
                                                        const char           *icon_name,
                                                        NolphinBookmarkMetadata *md);
NolphinBookmark *    nolphin_bookmark_copy                   (NolphinBookmark      *bookmark);
const char *          nolphin_bookmark_get_name               (NolphinBookmark      *bookmark);
GFile *               nolphin_bookmark_get_location           (NolphinBookmark      *bookmark);
char *                nolphin_bookmark_get_uri                (NolphinBookmark      *bookmark);
gchar *               nolphin_bookmark_get_icon_name          (NolphinBookmark      *bookmark);
gboolean	      nolphin_bookmark_get_has_custom_name    (NolphinBookmark      *bookmark);		
void                  nolphin_bookmark_set_custom_name        (NolphinBookmark      *bookmark,
								const char            *new_name);		
gboolean              nolphin_bookmark_uri_get_exists         (NolphinBookmark      *bookmark);
int                   nolphin_bookmark_compare_with           (gconstpointer          a,
								gconstpointer          b);
int                   nolphin_bookmark_compare_uris           (gconstpointer          a,
								gconstpointer          b);

void                  nolphin_bookmark_set_scroll_pos         (NolphinBookmark      *bookmark,
								const char            *uri);
char *                nolphin_bookmark_get_scroll_pos         (NolphinBookmark      *bookmark);


/* Helper functions for displaying bookmarks */
GtkWidget *           nolphin_bookmark_menu_item_new          (NolphinBookmark      *bookmark);

void                  nolphin_bookmark_connect                (NolphinBookmark *bookmark);

/* Bookmark metadata struct functions */

NolphinBookmarkMetadata *nolphin_bookmark_get_updated_metadata   (NolphinBookmark  *bookmark);
NolphinBookmarkMetadata *nolphin_bookmark_get_current_metadata   (NolphinBookmark  *bookmark);
gboolean              nolphin_bookmark_metadata_compare       (NolphinBookmarkMetadata *d1,
                                                            NolphinBookmarkMetadata *d2);
NolphinBookmarkMetadata *nolphin_bookmark_metadata_new           (void);
NolphinBookmarkMetadata *nolphin_bookmark_metadata_copy          (NolphinBookmarkMetadata *meta);
void                  nolphin_bookmark_metadata_free          (NolphinBookmarkMetadata *metadata);

#endif /* NOLPHIN_BOOKMARK_H */
