/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nolphin-file-drag.c - Drag & drop handling code that operated on 
   NolphinFile objects.

   Copyright (C) 2000 Eazel, Inc.

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

   Authors: Pavel Cisler <pavel@eazel.com>,
*/

#include <config.h>
#include "nolphin-file-dnd.h"
#include "nolphin-desktop-icon-file.h"

#include "nolphin-dnd.h"
#include "nolphin-directory.h"
#include "nolphin-file-utilities.h"
#include <string.h>

static gboolean
nolphin_drag_can_accept_files (NolphinFile *drop_target_item)
{
	NolphinDirectory *directory;

	if (nolphin_file_is_directory (drop_target_item)) {
		gboolean res;

		/* target is a directory, accept if editable */
		directory = nolphin_directory_get_for_file (drop_target_item);
		res = nolphin_directory_is_editable (directory);
		nolphin_directory_unref (directory);
		return res;
	}
	
	if (NOLPHIN_IS_DESKTOP_ICON_FILE (drop_target_item)) {
		return TRUE;
	}
	
	/* Launchers are an acceptable drop target */
	if (nolphin_file_is_launcher (drop_target_item)) {
		return TRUE;
	}

	if (nolphin_is_file_roller_installed () &&
	    nolphin_file_is_archive (drop_target_item)) {
		return TRUE;
	}
	
	return FALSE;
}

gboolean
nolphin_drag_can_accept_item (NolphinFile *drop_target_item,
			       const char *item_uri)
{
	if (nolphin_file_matches_uri (drop_target_item, item_uri)) {
		/* can't accept itself */
		return FALSE;
	}

	return nolphin_drag_can_accept_files (drop_target_item);
}
				       
gboolean
nolphin_drag_can_accept_items (NolphinFile *drop_target_item,
				const GList *items)
{
	int max;

	if (drop_target_item == NULL)
		return FALSE;

	g_assert (NOLPHIN_IS_FILE (drop_target_item));

	/* Iterate through selection checking if item will get accepted by the
	 * drop target. If more than 100 items selected, return an over-optimisic
	 * result
	 */
	for (max = 100; items != NULL && max >= 0; items = items->next, max--) {
		if (!nolphin_drag_can_accept_item (drop_target_item, 
			((NolphinDragSelectionItem *)items->data)->uri)) {
			return FALSE;
		}
	}
	
	return TRUE;
}

gboolean
nolphin_drag_can_accept_info (NolphinFile *drop_target_item,
			       NolphinIconDndTargetType drag_type,
			       const GList *items)
{
	switch (drag_type) {
		case NOLPHIN_ICON_DND_GNOME_ICON_LIST:
			return nolphin_drag_can_accept_items (drop_target_item, items);

		case NOLPHIN_ICON_DND_URI_LIST:
		case NOLPHIN_ICON_DND_NETSCAPE_URL:
		case NOLPHIN_ICON_DND_TEXT:
			return nolphin_drag_can_accept_files (drop_target_item);

		case NOLPHIN_ICON_DND_XDNDDIRECTSAVE:
		case NOLPHIN_ICON_DND_RAW:
			return nolphin_drag_can_accept_files (drop_target_item); /* Check if we can accept files at this location */

		case NOLPHIN_ICON_DND_ROOTWINDOW_DROP:
			return FALSE;

		default:
			g_assert_not_reached ();
			return FALSE;
	}
}

