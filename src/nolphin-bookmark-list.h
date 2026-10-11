/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nolphin
 *
 * Copyright (C) 1999, 2000 Eazel, Inc.
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Suite 500, MA 02110-1335, USA.
 *
 * Authors: John Sullivan <sullivan@eazel.com>
 */

/* nolphin-bookmark-list.h - interface for centralized list of bookmarks.
 */

#ifndef NOLPHIN_BOOKMARK_LIST_H
#define NOLPHIN_BOOKMARK_LIST_H

#include <libnolphin-private/nolphin-bookmark.h>
#include <gio/gio.h>

typedef struct NolphinBookmarkList NolphinBookmarkList;
typedef struct NolphinBookmarkListClass NolphinBookmarkListClass;

#define NOLPHIN_TYPE_BOOKMARK_LIST nolphin_bookmark_list_get_type()
#define NOLPHIN_BOOKMARK_LIST(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NOLPHIN_TYPE_BOOKMARK_LIST, NolphinBookmarkList))
#define NOLPHIN_BOOKMARK_LIST_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NOLPHIN_TYPE_BOOKMARK_LIST, NolphinBookmarkListClass))
#define NOLPHIN_IS_BOOKMARK_LIST(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NOLPHIN_TYPE_BOOKMARK_LIST))
#define NOLPHIN_IS_BOOKMARK_LIST_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NOLPHIN_TYPE_BOOKMARK_LIST))
#define NOLPHIN_BOOKMARK_LIST_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NOLPHIN_TYPE_BOOKMARK_LIST, NolphinBookmarkListClass))

struct NolphinBookmarkList {
	GObject object;

	GList *list; 
	GFileMonitor *monitor;
	GQueue *pending_ops;
    GVolumeMonitor *volume_monitor;

    guint idle_notify_id;
};

struct NolphinBookmarkListClass {
	GObjectClass parent_class;
	void (* changed) (NolphinBookmarkList *bookmarks);
};

GType                   nolphin_bookmark_list_get_type            (void);
NolphinBookmarkList *  nolphin_bookmark_list_get_default                 (void);
void                    nolphin_bookmark_list_append              (NolphinBookmarkList   *bookmarks,
								    NolphinBookmark *bookmark);
gboolean                nolphin_bookmark_list_contains            (NolphinBookmarkList   *bookmarks,
								    NolphinBookmark *bookmark);
void                    nolphin_bookmark_list_delete_item_at      (NolphinBookmarkList   *bookmarks,
								    guint                   index);
void                    nolphin_bookmark_list_delete_items_with_uri (NolphinBookmarkList *bookmarks,
								    const char		   *uri);
void                    nolphin_bookmark_list_insert_item         (NolphinBookmarkList   *bookmarks,
								    NolphinBookmark *bookmark,
								    guint                   index);
GList *                 nolphin_bookmark_list_get_for_uri         (NolphinBookmarkList   *bookmarks,
                                                                const char *uri);
guint                   nolphin_bookmark_list_length              (NolphinBookmarkList   *bookmarks);
NolphinBookmark *      nolphin_bookmark_list_item_at             (NolphinBookmarkList   *bookmarks,
								    guint                   index);
void                    nolphin_bookmark_list_move_item           (NolphinBookmarkList *bookmarks,
								    guint                 index,
								    guint                 destination);
void                    nolphin_bookmark_list_sort_ascending           (NolphinBookmarkList *bookmarks);
void                    nolphin_bookmark_list_set_window_geometry (NolphinBookmarkList   *bookmarks,
								    const char             *geometry);
const char *            nolphin_bookmark_list_get_window_geometry (NolphinBookmarkList   *bookmarks);

#endif /* NOLPHIN_BOOKMARK_LIST_H */
