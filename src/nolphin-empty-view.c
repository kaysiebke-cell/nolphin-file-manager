/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* fm-empty-view.c - implementation of empty view of directory.

   Copyright (C) 2006 Free Software Foundation, Inc.
   
   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
   Boston, MA 02110-1335, USA.

   Authors: Christian Neumair <chris@gnome-de.org>
*/

#include <config.h>

#include "nolphin-empty-view.h"

#include "nolphin-view.h"
#include "nolphin-view-factory.h"

#include <string.h>
#include <libnolphin-private/nolphin-file-utilities.h>
#include <eel/eel-vfs-extensions.h>

struct NolphinEmptyViewDetails {
	int number_of_files;
};

static GList *nolphin_empty_view_get_selection                   (NolphinView   *view);
static GList *nolphin_empty_view_get_selection_for_file_transfer (NolphinView   *view);
static void   nolphin_empty_view_scroll_to_file                  (NolphinView      *view,
								   const char        *uri);

G_DEFINE_TYPE (NolphinEmptyView, nolphin_empty_view, NOLPHIN_TYPE_VIEW)

static void
nolphin_empty_view_add_file (NolphinView *view, NolphinFile *file, NolphinDirectory *directory)
{
	static GTimer *timer = NULL;
	static gdouble cumu = 0, elaps;
	NOLPHIN_EMPTY_VIEW (view)->details->number_of_files++;
	GdkPixbuf *icon;

	if (!timer) timer = g_timer_new ();

	g_timer_start (timer);
	icon = nolphin_file_get_icon_pixbuf (file, nolphin_get_icon_size_for_zoom_level (NOLPHIN_ZOOM_LEVEL_STANDARD), TRUE, 0, NOLPHIN_FILE_ICON_FLAGS_NONE);

	elaps = g_timer_elapsed (timer, NULL);
	g_timer_stop (timer);

	g_object_unref (icon);
	
	cumu += elaps;
	g_message ("entire loading: %.3f, cumulative %.3f", elaps, cumu);
}


static void
nolphin_empty_view_begin_loading (NolphinView *view)
{
}

static void
nolphin_empty_view_clear (NolphinView *view)
{
}


static void
nolphin_empty_view_file_changed (NolphinView *view, NolphinFile *file, NolphinDirectory *directory)
{
}

static GList *
nolphin_empty_view_get_selection (NolphinView *view)
{
	return NULL;
}


static GList *
nolphin_empty_view_get_selection_for_file_transfer (NolphinView *view)
{
	return NULL;
}

static guint
nolphin_empty_view_get_item_count (NolphinView *view)
{
	return NOLPHIN_EMPTY_VIEW (view)->details->number_of_files;
}

static gboolean
nolphin_empty_view_is_empty (NolphinView *view)
{
	return NOLPHIN_EMPTY_VIEW (view)->details->number_of_files == 0;
}

static void
nolphin_empty_view_end_file_changes (NolphinView *view)
{
}

static void
nolphin_empty_view_remove_file (NolphinView *view, NolphinFile *file, NolphinDirectory *directory)
{
	NOLPHIN_EMPTY_VIEW (view)->details->number_of_files--;
	g_assert (NOLPHIN_EMPTY_VIEW (view)->details->number_of_files >= 0);
}

static void
nolphin_empty_view_set_selection (NolphinView *view, GList *selection)
{
	nolphin_view_notify_selection_changed (view);
}

static void
nolphin_empty_view_select_all (NolphinView *view)
{
}

static void
nolphin_empty_view_reveal_selection (NolphinView *view)
{
}

static void
nolphin_empty_view_merge_menus (NolphinView *view)
{
	NOLPHIN_VIEW_CLASS (nolphin_empty_view_parent_class)->merge_menus (view);
}

static void
nolphin_empty_view_update_menus (NolphinView *view)
{
	NOLPHIN_VIEW_CLASS (nolphin_empty_view_parent_class)->update_menus (view);
}

/* Reset sort criteria and zoom level to match defaults */
static void
nolphin_empty_view_reset_to_defaults (NolphinView *view)
{
}

static void
nolphin_empty_view_bump_zoom_level (NolphinView *view, int zoom_increment)
{
}

static NolphinZoomLevel
nolphin_empty_view_get_zoom_level (NolphinView *view)
{
	return NOLPHIN_ZOOM_LEVEL_STANDARD;
}

static void
nolphin_empty_view_zoom_to_level (NolphinView *view,
			    NolphinZoomLevel zoom_level)
{
}

static void
nolphin_empty_view_restore_default_zoom_level (NolphinView *view)
{
}

static gboolean 
nolphin_empty_view_can_zoom_in (NolphinView *view) 
{
	return FALSE;
}

static gboolean 
nolphin_empty_view_can_zoom_out (NolphinView *view) 
{
	return FALSE;
}

static void
nolphin_empty_view_start_renaming_file (NolphinView *view,
				  NolphinFile *file,
				  gboolean select_all)
{
}

static void
nolphin_empty_view_click_policy_changed (NolphinView *directory_view)
{
}


static int
nolphin_empty_view_compare_files (NolphinView *view, NolphinFile *file1, NolphinFile *file2)
{
	if (file1 < file2) {
		return -1;
	}

	if (file1 > file2) {
		return +1;
	}

	return 0;
}

static gboolean
nolphin_empty_view_using_manual_layout (NolphinView *view)
{
	return FALSE;
}

static void
nolphin_empty_view_end_loading (NolphinView *view,
			   gboolean all_files_seen)
{
}

static char *
nolphin_empty_view_get_first_visible_file (NolphinView *view)
{
	return NULL;
}

static void
nolphin_empty_view_scroll_to_file (NolphinView *view,
			      const char *uri)
{
}

static void
nolphin_empty_view_sort_directories_first_changed (NolphinView *view)
{
}

static const char *
nolphin_empty_view_get_id (NolphinView *view)
{
	return NOLPHIN_EMPTY_VIEW_ID;
}

static void
nolphin_empty_view_class_init (NolphinEmptyViewClass *class)
{
	NolphinViewClass *nolphin_view_class;

	g_type_class_add_private (class, sizeof (NolphinEmptyViewDetails));

	nolphin_view_class = NOLPHIN_VIEW_CLASS (class);

	nolphin_view_class->add_file = nolphin_empty_view_add_file;
	nolphin_view_class->begin_loading = nolphin_empty_view_begin_loading;
	nolphin_view_class->bump_zoom_level = nolphin_empty_view_bump_zoom_level;
	nolphin_view_class->can_zoom_in = nolphin_empty_view_can_zoom_in;
	nolphin_view_class->can_zoom_out = nolphin_empty_view_can_zoom_out;
        nolphin_view_class->click_policy_changed = nolphin_empty_view_click_policy_changed;
	nolphin_view_class->clear = nolphin_empty_view_clear;
	nolphin_view_class->file_changed = nolphin_empty_view_file_changed;
	nolphin_view_class->get_selection = nolphin_empty_view_get_selection;
	nolphin_view_class->get_selection_for_file_transfer = nolphin_empty_view_get_selection_for_file_transfer;
	nolphin_view_class->get_item_count = nolphin_empty_view_get_item_count;
	nolphin_view_class->is_empty = nolphin_empty_view_is_empty;
	nolphin_view_class->remove_file = nolphin_empty_view_remove_file;
	nolphin_view_class->merge_menus = nolphin_empty_view_merge_menus;
	nolphin_view_class->update_menus = nolphin_empty_view_update_menus;
	nolphin_view_class->reset_to_defaults = nolphin_empty_view_reset_to_defaults;
	nolphin_view_class->restore_default_zoom_level = nolphin_empty_view_restore_default_zoom_level;
	nolphin_view_class->reveal_selection = nolphin_empty_view_reveal_selection;
	nolphin_view_class->select_all = nolphin_empty_view_select_all;
	nolphin_view_class->set_selection = nolphin_empty_view_set_selection;
	nolphin_view_class->compare_files = nolphin_empty_view_compare_files;
	nolphin_view_class->sort_directories_first_changed = nolphin_empty_view_sort_directories_first_changed;
	nolphin_view_class->start_renaming_file = nolphin_empty_view_start_renaming_file;
	nolphin_view_class->get_zoom_level = nolphin_empty_view_get_zoom_level;
	nolphin_view_class->zoom_to_level = nolphin_empty_view_zoom_to_level;
	nolphin_view_class->end_file_changes = nolphin_empty_view_end_file_changes;
	nolphin_view_class->using_manual_layout = nolphin_empty_view_using_manual_layout;
	nolphin_view_class->end_loading = nolphin_empty_view_end_loading;
	nolphin_view_class->get_view_id = nolphin_empty_view_get_id;
	nolphin_view_class->get_first_visible_file = nolphin_empty_view_get_first_visible_file;
	nolphin_view_class->scroll_to_file = nolphin_empty_view_scroll_to_file;
}

static void
nolphin_empty_view_init (NolphinEmptyView *empty_view)
{
	empty_view->details = G_TYPE_INSTANCE_GET_PRIVATE (empty_view, NOLPHIN_TYPE_EMPTY_VIEW,
							   NolphinEmptyViewDetails);
}

static NolphinView *
nolphin_empty_view_create (NolphinWindowSlot *slot)
{
	NolphinEmptyView *view;

	g_assert (NOLPHIN_IS_WINDOW_SLOT (slot));

	view = g_object_new (NOLPHIN_TYPE_EMPTY_VIEW,
			     "window-slot", slot,
			     NULL);

	return NOLPHIN_VIEW (view);
}

static gboolean
nolphin_empty_view_supports_uri (const char *uri,
				  GFileType file_type,
				  const char *mime_type)
{
	if (file_type == G_FILE_TYPE_DIRECTORY) {
		return TRUE;
	}
	if (strcmp (mime_type, NOLPHIN_SAVED_SEARCH_MIMETYPE) == 0){
		return TRUE;
	}
	if (g_str_has_prefix (uri, "trash:")) {
		return TRUE;
	}
	if (g_str_has_prefix (uri, EEL_SEARCH_URI)) {
		return TRUE;
	}

	return FALSE;
}

static NolphinViewInfo nolphin_empty_view = {
	NOLPHIN_EMPTY_VIEW_ID,
	"Empty",
	"Empty View",
	"_Empty View",
	"The empty view encountered an error.",
	"Display this location with the empty view.",
	nolphin_empty_view_create,
	nolphin_empty_view_supports_uri
};

void
nolphin_empty_view_register (void)
{
	nolphin_empty_view.id = nolphin_empty_view.id;
	nolphin_empty_view.view_combo_label = nolphin_empty_view.view_combo_label;
	nolphin_empty_view.view_menu_label_with_mnemonic = nolphin_empty_view.view_menu_label_with_mnemonic;
	nolphin_empty_view.error_label = nolphin_empty_view.error_label;
	nolphin_empty_view.display_location_label = nolphin_empty_view.display_location_label;

	nolphin_view_factory_register (&nolphin_empty_view);
}
