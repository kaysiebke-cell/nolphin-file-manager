/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* fm-icon-view.c - implementation of icon view of directory.

   Copyright (C) 2000, 2001 Eazel, Inc.

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

   Authors: John Sullivan <sullivan@eazel.com>
*/

#include <config.h>

#include "nolphin-icon-view.h"

#include "nolphin-actions.h"
#include "nolphin-icon-view-container.h"
#include "nolphin-icon-view-grid-container.h"
#include "nolphin-error-reporting.h"
#include "nolphin-view-dnd.h"
#include "nolphin-view-factory.h"
#include "nolphin-window.h"
#include "nolphin-desktop-window.h"
#include "nolphin-desktop-manager.h"
#include "nolphin-application.h"

#include <stdlib.h>
#include <eel/eel-vfs-extensions.h>
#include <errno.h>
#include <fcntl.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <libnolphin-private/nolphin-clipboard-monitor.h>
#include <libnolphin-private/nolphin-directory.h>
#include <libnolphin-private/nolphin-dnd.h>
#include <libnolphin-private/nolphin-file-dnd.h>
#include <libnolphin-private/nolphin-file-utilities.h>
#include <libnolphin-private/nolphin-ui-utilities.h>
#include <libnolphin-private/nolphin-global-preferences.h>
#include <libnolphin-private/nolphin-icon-container.h>
#include <libnolphin-private/nolphin-icon-dnd.h>
#include <libnolphin-private/nolphin-icon.h>
#include <libnolphin-private/nolphin-link.h>
#include <libnolphin-private/nolphin-metadata.h>
#include <libnolphin-private/nolphin-clipboard.h>
#include <libnolphin-private/nolphin-desktop-icon-file.h>
#include <libnolphin-private/nolphin-desktop-utils.h>
#include <libnolphin-private/nolphin-desktop-directory.h>

#define DEBUG_FLAG NOLPHIN_DEBUG_ICON_VIEW
#include <libnolphin-private/nolphin-debug.h>

#include <locale.h>
#include <signal.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define POPUP_PATH_ICON_APPEARANCE		"/selection/Icon Appearance Items"

enum
{
	PROP_COMPACT = 1,
	PROP_GALLERY,
	PROP_SUPPORTS_AUTO_LAYOUT,
	PROP_IS_DESKTOP,
	PROP_SUPPORTS_KEEP_ALIGNED,
	PROP_SUPPORTS_LABELS_BESIDE_ICONS,
	NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

typedef struct {
	const NolphinFileSortType sort_type;
	const char *metadata_text;
	const char *action;
	const char *menu_label;
	const char *menu_hint;
} SortCriterion;

typedef enum {
	MENU_ITEM_TYPE_STANDARD,
	MENU_ITEM_TYPE_CHECK,
	MENU_ITEM_TYPE_RADIO,
	MENU_ITEM_TYPE_TREE
} MenuItemType;

struct NolphinIconViewDetails
{
	GList *icons_not_positioned;

	guint react_to_icon_change_idle_id;

	const SortCriterion *sort;
	gboolean sort_reversed;

	GtkActionGroup *icon_action_group;
	guint icon_merge_id;

	gboolean compact;
	gboolean gallery;

	gulong clipboard_handler_id;

	GtkWidget *icon_container;

	gboolean supports_auto_layout;
	gboolean is_desktop;
	gboolean supports_keep_aligned;
	gboolean supports_labels_beside_icons;
};


/* Note that the first item in this list is the default sort,
 * and that the items show up in the menu in the order they
 * appear in this list.
 */
static const SortCriterion sort_criteria[] = {
	{
		NOLPHIN_FILE_SORT_BY_DISPLAY_NAME,
		"name",
		"Sort by Name",
		N_("nach _Name"),
		N_("Symbole nach Name in Zeilen sortieren")
	},
	{
		NOLPHIN_FILE_SORT_BY_SIZE,
		"size",
		"Sort by Size",
		N_("nach _Größe"),
		N_("Symbole nach Größe in Zeilen sortieren")
	},
	{
		NOLPHIN_FILE_SORT_BY_TYPE,
		"type",
		"Sort by Type",
		N_("nach _Typ"),
		N_("Symbole nach Typ in Zeilen sortieren")
	},
    {
        NOLPHIN_FILE_SORT_BY_DETAILED_TYPE,
        "detailed_type",
        "Sort by Detailed Type",
        N_("nach _datailliertem Dateityp"),
        N_("Symbole nach detailliertem Dateityp in Reihen sortieren")
    },
	{
		NOLPHIN_FILE_SORT_BY_MTIME,
		"modification date",
		"Sort by Modification Date",
		N_("nach Änderungs_datum"),
		N_("Symbole nach Änderungsdatum in Zeilen sortieren")
	},
  {
    	NOLPHIN_FILE_SORT_BY_TRASHED_TIME,
    	"trashed",
    	"Sort by Trash Time",
    	N_("nach Löschdatum"),
    	N_("Symbole nach Löschdatum in Zeilen sortieren")
  },
  {
    	NOLPHIN_FILE_SORT_BY_EXTENSION,
    	"extension",
    	"Sort by Extension",
    	N_("nach _Erweiterung"),
    	N_("Symbole zeilenweise nach Erweiterung sortiert halten")
  }
};

static void                 nolphin_icon_view_set_directory_sort_by        (NolphinIconView           *icon_view,
									     NolphinFile         *file,
									     const char           *sort_by);
static void                 nolphin_icon_view_set_zoom_level               (NolphinIconView           *view,
									     NolphinZoomLevel     new_level,
									     gboolean              always_emit);
static void                 nolphin_icon_view_update_click_mode            (NolphinIconView           *icon_view);
static void                 nolphin_icon_view_update_click_to_rename_mode  (NolphinIconView           *icon_view);
static gboolean             nolphin_icon_view_is_desktop      (NolphinIconView           *icon_view);
static void                 nolphin_icon_view_reveal_selection       (NolphinView               *view);
static const SortCriterion *get_sort_criterion_by_sort_type           (NolphinFileSortType  sort_type);
static void                 switch_to_manual_layout                   (NolphinIconView     *view);
static void                 update_layout_menus                       (NolphinIconView     *view);
static NolphinFileSortType get_default_sort_order                    (NolphinFile         *file,
								       gboolean             *reversed);
static void                 nolphin_icon_view_clear_full                 (NolphinView *view,
                                                                       gboolean  destroying);
static const SortCriterion *get_sort_criterion_by_metadata_text (const char *metadata_text);
static void		    nolphin_icon_view_remove_file (NolphinView *view, NolphinFile *file, NolphinDirectory *directory);

G_DEFINE_TYPE (NolphinIconView, nolphin_icon_view, NOLPHIN_TYPE_VIEW);

static void
nolphin_icon_view_destroy (GtkWidget *object)
{
	NolphinIconView *icon_view;

	icon_view = NOLPHIN_ICON_VIEW (object);

	nolphin_icon_view_clear_full (NOLPHIN_VIEW (object), TRUE);

        if (icon_view->details->react_to_icon_change_idle_id != 0) {
                g_source_remove (icon_view->details->react_to_icon_change_idle_id);
		icon_view->details->react_to_icon_change_idle_id = 0;
        }

	if (icon_view->details->clipboard_handler_id != 0) {
		g_signal_handler_disconnect (nolphin_clipboard_monitor_get (),
					     icon_view->details->clipboard_handler_id);
		icon_view->details->clipboard_handler_id = 0;
	}

	if (icon_view->details->icons_not_positioned) {
		nolphin_file_list_free (icon_view->details->icons_not_positioned);
		icon_view->details->icons_not_positioned = NULL;
	}

	GTK_WIDGET_CLASS (nolphin_icon_view_parent_class)->destroy (object);
}

static void
sync_directory_monitor_number (NolphinIconView *view, NolphinFile *file)
{
    NolphinDirectory *directory;
    NolphinDesktopWindow *desktop_window;
    gint monitor;

    if (!view->details->is_desktop) {
        return;
    }

    desktop_window = NOLPHIN_DESKTOP_WINDOW (nolphin_view_get_nolphin_window (NOLPHIN_VIEW (view)));

    monitor = nolphin_desktop_window_get_monitor (desktop_window);

    directory = nolphin_view_get_model (NOLPHIN_VIEW (view));

    NOLPHIN_DESKTOP_DIRECTORY (directory)->display_number = monitor;
}


static NolphinIconContainer *
get_icon_container (NolphinIconView *icon_view)
{
	return NOLPHIN_ICON_CONTAINER (icon_view->details->icon_container);
}

NolphinIconContainer *
nolphin_icon_view_get_icon_container (NolphinIconView *icon_view)
{
	return get_icon_container (icon_view);
}

static gboolean
nolphin_icon_view_supports_manual_layout (NolphinIconView *view)
{
	g_return_val_if_fail (NOLPHIN_IS_ICON_VIEW (view), FALSE);

	return !nolphin_icon_view_is_compact (view);
}

static void
real_set_sort_criterion (NolphinIconView *icon_view,
                         const SortCriterion *sort,
                         gboolean clear,
			 gboolean set_metadata)
{
	NolphinFile *file;

	file = nolphin_view_get_directory_as_file (NOLPHIN_VIEW (icon_view));

    sync_directory_monitor_number (icon_view, file);

	if (clear) {
		nolphin_file_set_metadata (file,
                                NOLPHIN_METADATA_KEY_ICON_VIEW_SORT_BY,
                                NULL,
                                NULL);
		nolphin_file_set_metadata (file,
                                NOLPHIN_METADATA_KEY_ICON_VIEW_SORT_REVERSED,
                                NULL,
                                NULL);
        icon_view->details->sort =
            get_sort_criterion_by_sort_type (get_default_sort_order (file, &icon_view->details->sort_reversed));
    } else if (set_metadata) {
		/* Store the new sort setting. */
        nolphin_icon_view_set_directory_sort_by (icon_view,
                                              file,
                                              sort->metadata_text);
    }

	/* Update the layout menus to match the new sort setting. */
	update_layout_menus (icon_view);
}

static void
set_sort_criterion (NolphinIconView *icon_view,
		    const SortCriterion *sort,
		    gboolean set_metadata)
{
	if (sort == NULL ||
	    icon_view->details->sort == sort) {
		return;
	}

	icon_view->details->sort = sort;

        real_set_sort_criterion (icon_view, sort, FALSE, set_metadata);
}

static void
clear_sort_criterion (NolphinIconView *icon_view)
{
	real_set_sort_criterion (icon_view, NULL, TRUE, TRUE);
}

static void
nolphin_icon_view_clean_up (NolphinIconView *icon_view)
{
	NolphinIconContainer *icon_container;
	gboolean saved_sort_reversed;

	icon_container = get_icon_container (icon_view);

	/* Hardwire Clean Up to always be by name, in forward order */
	saved_sort_reversed = icon_view->details->sort_reversed;

	nolphin_icon_view_set_sort_reversed (icon_view, FALSE, FALSE);
	set_sort_criterion (icon_view, &sort_criteria[0], FALSE);

	nolphin_icon_container_sort (icon_container);
	nolphin_icon_container_freeze_icon_positions (icon_container);

	nolphin_icon_view_set_sort_reversed (icon_view, saved_sort_reversed, FALSE);
}

static void
action_clean_up_callback (GtkAction *action, gpointer callback_data)
{
	nolphin_icon_view_clean_up (NOLPHIN_ICON_VIEW (callback_data));
}

static gboolean
nolphin_icon_view_using_auto_layout (NolphinIconView *icon_view)
{
	return nolphin_icon_container_is_auto_layout
		(get_icon_container (icon_view));
}

static void
action_sort_radio_callback (GtkAction *action,
			    GtkRadioAction *current,
			    NolphinIconView *view)
{
	NolphinFileSortType sort_type;

	sort_type = gtk_radio_action_get_current_value (current);

	/* Note that id might be a toggle item.
	 * Ignore non-sort ids so that they don't cause sorting.
	 */
	if (sort_type == NOLPHIN_FILE_SORT_NONE) {
		switch_to_manual_layout (view);
	} else {
		nolphin_icon_view_set_sort_criterion_by_sort_type (view, sort_type);
	}
}

static void
list_covers (NolphinIconData *data, gpointer callback_data)
{
	GSList **file_list;

	file_list = callback_data;

	*file_list = g_slist_prepend (*file_list, data);
}

static void
unref_cover (NolphinIconData *data, gpointer callback_data)
{
	nolphin_file_unref (NOLPHIN_FILE (data));
}

static void
nolphin_icon_view_clear_full (NolphinView *view, gboolean destroying)
{
	NolphinIconContainer *icon_container;
	GSList *file_list;

	g_return_if_fail (NOLPHIN_IS_ICON_VIEW (view));

	icon_container = get_icon_container (NOLPHIN_ICON_VIEW (view));
	if (!icon_container)
		return;

	/* Clear away the existing icons. */
	file_list = NULL;
	nolphin_icon_container_for_each (icon_container, list_covers, &file_list);
	nolphin_icon_container_clear (icon_container);

    if (!destroying) {
        nolphin_icon_container_update_scroll_region (icon_container);
    }

	g_slist_foreach (file_list, (GFunc)unref_cover, NULL);
	g_slist_free (file_list);
}

static void
nolphin_icon_view_clear (NolphinView *view)
{
    nolphin_icon_view_clear_full (view, FALSE);
}

static gboolean
should_show_file_on_screen (NolphinView *view, NolphinFile *file)
{
	if (!nolphin_view_should_show_file (view, file)) {
		return FALSE;
	}

	return TRUE;
}

static void
nolphin_icon_view_remove_file (NolphinView *view, NolphinFile *file, NolphinDirectory *directory)
{
	NolphinIconView *icon_view;

	/* This used to assert that 'directory == nolphin_view_get_model (view)', but that
	 * resulted in a lot of crash reports (bug #352592). I don't see how that trace happens.
	 * It seems that somehow we get a files_changed event sent to the view from a directory
	 * that isn't the model, but the code disables the monitor and signal callback handlers when
	 * changing directories. Maybe we can get some more information when this happens.
	 * Further discussion in bug #368178.
	 */
	if (directory != nolphin_view_get_model (view)) {
		char *file_uri, *dir_uri, *model_uri;
		file_uri = nolphin_file_get_uri (file);
		dir_uri = nolphin_directory_get_uri (directory);
		model_uri = nolphin_directory_get_uri (nolphin_view_get_model (view));
		g_warning ("nolphin_icon_view_remove_file() - directory not icon view model, shouldn't happen.\n"
			   "file: %p:%s, dir: %p:%s, model: %p:%s, view loading: %d\n"
			   "If you see this, please add this info to http://bugzilla.gnome.org/show_bug.cgi?id=368178",
			   file, file_uri, directory, dir_uri, nolphin_view_get_model (view), model_uri, nolphin_view_get_loading (view));
		g_free (file_uri);
		g_free (dir_uri);
		g_free (model_uri);
	}

	icon_view = NOLPHIN_ICON_VIEW (view);

	if (nolphin_icon_container_remove (get_icon_container (icon_view),
					    NOLPHIN_ICON_CONTAINER_ICON_DATA (file))) {
		nolphin_file_unref (file);
	}
}

static void
nolphin_icon_view_add_file (NolphinView *view, NolphinFile *file, NolphinDirectory *directory)
{
	NolphinIconView *icon_view;
	NolphinIconContainer *icon_container;

	g_assert (directory == nolphin_view_get_model (view));

	icon_view = NOLPHIN_ICON_VIEW (view);

    if (icon_view->details->is_desktop &&
        !should_show_file_on_screen (view, file)) {
        return;
    }

    icon_container = get_icon_container (icon_view);

    if (nolphin_file_has_thumbnail_access_problem (file)) {
        nolphin_application_set_cache_flag (nolphin_application_get_singleton ());
        nolphin_window_slot_check_bad_cache_bar (nolphin_view_get_nolphin_window_slot (view));
    }

	/* Reset scroll region for the first icon added when loading a directory. */
	if (nolphin_view_get_loading (view) && nolphin_icon_container_is_empty (icon_container)) {
		nolphin_icon_container_reset_scroll_region (icon_container);
	}

	if (nolphin_icon_container_add (icon_container,
					 NOLPHIN_ICON_CONTAINER_ICON_DATA (file))) {
		nolphin_file_ref (file);
	}
}

static void
nolphin_icon_view_file_changed (NolphinView *view, NolphinFile *file, NolphinDirectory *directory)
{
	NolphinIconView *icon_view;

	g_assert (directory == nolphin_view_get_model (view));

	g_return_if_fail (view != NULL);
	icon_view = NOLPHIN_ICON_VIEW (view);

	if (!icon_view->details->is_desktop) {
		nolphin_icon_container_request_update
			(get_icon_container (icon_view),
			 NOLPHIN_ICON_CONTAINER_ICON_DATA (file));
		return;
	}

	if (!should_show_file_on_screen (view, file)) {
		nolphin_icon_view_remove_file (view, file, directory);
	} else {

		nolphin_icon_container_request_update
			(get_icon_container (icon_view),
			 NOLPHIN_ICON_CONTAINER_ICON_DATA (file));
	}
}

static gboolean
nolphin_icon_view_supports_auto_layout (NolphinIconView *view)
{
	g_return_val_if_fail (NOLPHIN_IS_ICON_VIEW (view), FALSE);

	return view->details->supports_auto_layout;
}

static gboolean
nolphin_icon_view_is_desktop (NolphinIconView *view)
{
	g_return_val_if_fail (NOLPHIN_IS_ICON_VIEW (view), FALSE);

	return view->details->is_desktop;
}

static gboolean
nolphin_icon_view_supports_keep_aligned (NolphinIconView *view)
{
	g_return_val_if_fail (NOLPHIN_IS_ICON_VIEW (view), FALSE);

	return view->details->supports_keep_aligned;
}

static gboolean
nolphin_icon_view_supports_labels_beside_icons (NolphinIconView *view)
{
	g_return_val_if_fail (NOLPHIN_IS_ICON_VIEW (view), FALSE);

	return view->details->supports_labels_beside_icons;
}

static void
update_layout_menus (NolphinIconView *view)
{
	gboolean is_auto_layout;
	GtkAction *action;
	const char *action_name;
	NolphinFile *file;

	if (view->details->icon_action_group == NULL) {
		return;
	}

	is_auto_layout = nolphin_icon_view_using_auto_layout (view);
	file = nolphin_view_get_directory_as_file (NOLPHIN_VIEW (view));

	if (nolphin_icon_view_supports_auto_layout (view)) {
		/* Mark sort criterion. */
		action_name = is_auto_layout ? view->details->sort->action : NOLPHIN_ACTION_MANUAL_LAYOUT;
		action = gtk_action_group_get_action (view->details->icon_action_group,
						      action_name);
		gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), TRUE);

        action = gtk_action_group_get_action (view->details->icon_action_group,
                                              NOLPHIN_ACTION_REVERSED_ORDER);
		gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action),
					      view->details->sort_reversed);
		gtk_action_set_sensitive (action, is_auto_layout);

		action = gtk_action_group_get_action (view->details->icon_action_group,
		                                      NOLPHIN_ACTION_SORT_TRASH_TIME);

		if (file != NULL && nolphin_file_is_in_trash (file)) {
			gtk_action_set_visible (action, TRUE);
		} else {
			gtk_action_set_visible (action, FALSE);
		}
	}

	action = gtk_action_group_get_action (view->details->icon_action_group,
					      NOLPHIN_ACTION_MANUAL_LAYOUT);
	gtk_action_set_visible (action,
				nolphin_icon_view_supports_manual_layout (view));

	/* Clean Up is only relevant for manual layout */
	action = gtk_action_group_get_action (view->details->icon_action_group,
					      NOLPHIN_ACTION_CLEAN_UP);
	gtk_action_set_sensitive (action, !is_auto_layout);

	if (nolphin_icon_view_is_desktop (view)) {
		gtk_action_set_label (action, _("_Schreibtisch alphabetisch sortieren"));
	}

	action = gtk_action_group_get_action (view->details->icon_action_group,
					      NOLPHIN_ACTION_KEEP_ALIGNED);
	gtk_action_set_visible (action,
				nolphin_icon_view_supports_keep_aligned (view));
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action),
				      nolphin_icon_container_is_keep_aligned (get_icon_container (view)));
	gtk_action_set_sensitive (action, !is_auto_layout);
}


gchar *
nolphin_icon_view_get_directory_sort_by (NolphinIconView *icon_view,
					  NolphinFile *file)
{
	const SortCriterion *default_sort_criterion;

	if (!nolphin_icon_view_supports_auto_layout (icon_view)) {
		return g_strdup ("name");
	}

	default_sort_criterion = get_sort_criterion_by_sort_type (get_default_sort_order (file, NULL));
	g_return_val_if_fail (default_sort_criterion != NULL, NULL);

    sync_directory_monitor_number (icon_view, file);

    return nolphin_file_get_metadata (file,
                                   NOLPHIN_METADATA_KEY_ICON_VIEW_SORT_BY,
                                   default_sort_criterion->metadata_text);
}

static NolphinFileSortType
get_default_sort_order (NolphinFile *file, gboolean *reversed)
{
	NolphinFileSortType retval, default_sort_order;
	gboolean default_sort_in_reverse_order;

	default_sort_order = g_settings_get_enum (nolphin_preferences,
						  NOLPHIN_PREFERENCES_DEFAULT_SORT_ORDER);
	default_sort_in_reverse_order = g_settings_get_boolean (nolphin_preferences,
								NOLPHIN_PREFERENCES_DEFAULT_SORT_IN_REVERSE_ORDER);

	retval = nolphin_file_get_default_sort_type (file, reversed);

	if (retval == NOLPHIN_FILE_SORT_NONE) {

		if (reversed != NULL) {
			*reversed = default_sort_in_reverse_order;
		}

		retval = CLAMP (default_sort_order, NOLPHIN_FILE_SORT_BY_DISPLAY_NAME,
				NOLPHIN_FILE_SORT_BY_MTIME);
	}

	return retval;
}

static void
nolphin_icon_view_set_directory_sort_by (NolphinIconView *icon_view,
					  NolphinFile *file,
					  const char *sort_by)
{
	const SortCriterion *default_sort_criterion;

	if (!nolphin_icon_view_supports_auto_layout (icon_view)) {
		return;
	}

	default_sort_criterion = get_sort_criterion_by_sort_type (get_default_sort_order (file, NULL));
	g_return_if_fail (default_sort_criterion != NULL);

    sync_directory_monitor_number (icon_view, file);

    nolphin_file_set_metadata (file,
                            NOLPHIN_METADATA_KEY_ICON_VIEW_SORT_BY,
                            default_sort_criterion->metadata_text,
                            sort_by);
}

gboolean
nolphin_icon_view_get_directory_sort_reversed (NolphinIconView *icon_view,
						NolphinFile *file)
{
	gboolean reversed;

	if (!nolphin_icon_view_supports_auto_layout (icon_view)) {
		return FALSE;
	}

	get_default_sort_order (file, &reversed);

    sync_directory_monitor_number (icon_view, file);

    return nolphin_file_get_boolean_metadata (file,
                                           NOLPHIN_METADATA_KEY_ICON_VIEW_SORT_REVERSED,
                                           reversed);
}

static void
nolphin_icon_view_set_directory_sort_reversed (NolphinIconView *icon_view,
						NolphinFile *file,
						gboolean sort_reversed)
{
	gboolean reversed;

	if (!nolphin_icon_view_supports_auto_layout (icon_view)) {
		return;
	}

	get_default_sort_order (file, &reversed);

    sync_directory_monitor_number (icon_view, file);

    nolphin_file_set_boolean_metadata (file,
                                    NOLPHIN_METADATA_KEY_ICON_VIEW_SORT_REVERSED,
                                    reversed,
                                    sort_reversed);
}

static gboolean
get_default_directory_keep_aligned (void)
{
	return TRUE;
}

static gboolean
nolphin_icon_view_get_directory_keep_aligned (NolphinIconView *icon_view,
					       NolphinFile *file)
{
	if (!nolphin_icon_view_supports_keep_aligned (icon_view)) {
		return FALSE;
	}

    sync_directory_monitor_number (icon_view, file);

    return nolphin_file_get_boolean_metadata (file,
                                           NOLPHIN_METADATA_KEY_ICON_VIEW_KEEP_ALIGNED,
                                           get_default_directory_keep_aligned ());
}

void
nolphin_icon_view_set_directory_keep_aligned (NolphinIconView *icon_view,
					       NolphinFile *file,
					       gboolean keep_aligned)
{
	if (!nolphin_icon_view_supports_keep_aligned (icon_view)) {
		return;
	}

    sync_directory_monitor_number (icon_view, file);

    nolphin_file_set_boolean_metadata (file,
                                    NOLPHIN_METADATA_KEY_ICON_VIEW_KEEP_ALIGNED,
                                    get_default_directory_keep_aligned (),
                                    keep_aligned);
}

static gboolean
nolphin_icon_view_get_directory_auto_layout (NolphinIconView *icon_view,
					      NolphinFile *file)
{
	if (!nolphin_icon_view_supports_auto_layout (icon_view)) {
		return FALSE;
	}

	if (!nolphin_icon_view_supports_manual_layout (icon_view)) {
		return TRUE;
	}

    sync_directory_monitor_number (icon_view, file);

    return nolphin_file_get_boolean_metadata (file,
                                           NOLPHIN_METADATA_KEY_ICON_VIEW_AUTO_LAYOUT,
                                           TRUE);
}

static void
nolphin_icon_view_set_directory_auto_layout (NolphinIconView *icon_view,
					      NolphinFile *file,
					gboolean auto_layout)
{
	if (!nolphin_icon_view_supports_auto_layout (icon_view) ||
	    !nolphin_icon_view_supports_manual_layout (icon_view)) {
		return;
	}

    sync_directory_monitor_number (icon_view, file);

    nolphin_file_set_boolean_metadata (file,
                                    NOLPHIN_METADATA_KEY_ICON_VIEW_AUTO_LAYOUT,
                                    TRUE,
                                    auto_layout);
}

void
nolphin_icon_view_set_directory_horizontal_layout (NolphinIconView *icon_view,
                                                NolphinFile     *file,
                                                gboolean      horizontal)
{
    sync_directory_monitor_number (icon_view, file);

    nolphin_file_set_boolean_metadata (file,
                                    NOLPHIN_METADATA_KEY_DESKTOP_GRID_HORIZONTAL,
                                    FALSE,
                                    horizontal);
}

gboolean
nolphin_icon_view_get_directory_horizontal_layout (NolphinIconView *icon_view,
                                                NolphinFile     *file)
{
    sync_directory_monitor_number (icon_view, file);

    return nolphin_file_get_boolean_metadata (file,
                                           NOLPHIN_METADATA_KEY_DESKTOP_GRID_HORIZONTAL,
                                           FALSE);
}

void
nolphin_icon_view_set_directory_grid_adjusts (NolphinIconView *icon_view,
                                           NolphinFile     *file,
                                           gint          horizontal,
                                           gint          vertical)
{
    sync_directory_monitor_number (icon_view, file);

    nolphin_file_set_desktop_grid_adjusts (file,
                                        NOLPHIN_METADATA_KEY_DESKTOP_GRID_ADJUST,
                                        horizontal, vertical);
}

void
nolphin_icon_view_get_directory_grid_adjusts (NolphinIconView *icon_view,
                                           NolphinFile     *file,
                                           gint         *horizontal,
                                           gint         *vertical)
{
    gint h, v;

    sync_directory_monitor_number (icon_view, file);

    nolphin_file_get_desktop_grid_adjusts (file,
                                        NOLPHIN_METADATA_KEY_DESKTOP_GRID_ADJUST,
                                        &h, &v);

    if (horizontal)
        *horizontal = h;

    if (vertical)
        *vertical = v;
}

gboolean
nolphin_icon_view_set_sort_reversed (NolphinIconView *icon_view,
                                  gboolean      new_value,
                                  gboolean      set_metadata)
{
    if (icon_view->details->sort_reversed == new_value) {
        return FALSE;
    }

    icon_view->details->sort_reversed = new_value;

    if (set_metadata) {
        /* Store the new sort setting. */
        nolphin_icon_view_set_directory_sort_reversed (icon_view, nolphin_view_get_directory_as_file (NOLPHIN_VIEW (icon_view)), new_value);
    }

    /* Update the layout menus to match the new sort-order setting. */
    update_layout_menus (icon_view);

    return TRUE;
}

void
nolphin_icon_view_flip_sort_reversed (NolphinIconView *icon_view)
{
    nolphin_icon_view_set_sort_reversed (icon_view, !icon_view->details->sort_reversed, TRUE);
}

static const SortCriterion *
get_sort_criterion_by_metadata_text (const char *metadata_text)
{
	guint i;

	/* Figure out what the new sort setting should be. */
	for (i = 0; i < G_N_ELEMENTS (sort_criteria); i++) {
		if (g_strcmp0 (sort_criteria[i].metadata_text, metadata_text) == 0) {
			return &sort_criteria[i];
		}
	}
	return NULL;
}

static const SortCriterion *
get_sort_criterion_by_sort_type (NolphinFileSortType sort_type)
{
	guint i;

	/* Figure out what the new sort setting should be. */
	for (i = 0; i < G_N_ELEMENTS (sort_criteria); i++) {
		if (sort_type == sort_criteria[i].sort_type) {
			return &sort_criteria[i];
		}
	}

	return &sort_criteria[0];
}

#define DEFAULT_ZOOM_LEVEL(icon_view) icon_view->details->compact ? default_compact_zoom_level : default_zoom_level

static NolphinZoomLevel
get_default_zoom_level (NolphinIconView *icon_view)
{
	NolphinZoomLevel default_zoom_level, default_compact_zoom_level;

	default_zoom_level = g_settings_get_enum (nolphin_icon_view_preferences,
						  NOLPHIN_PREFERENCES_ICON_VIEW_DEFAULT_ZOOM_LEVEL);
	default_compact_zoom_level = g_settings_get_enum (nolphin_compact_view_preferences,
							  NOLPHIN_PREFERENCES_COMPACT_VIEW_DEFAULT_ZOOM_LEVEL);

    if (NOLPHIN_ICON_VIEW_GET_CLASS (icon_view)->use_grid_container) {
        return NOLPHIN_ZOOM_LEVEL_STANDARD;
    }

	return CLAMP (DEFAULT_ZOOM_LEVEL(icon_view), NOLPHIN_ZOOM_LEVEL_SMALLEST, NOLPHIN_ZOOM_LEVEL_LARGEST);
}

static void
set_labels_beside_icons (NolphinIconView *icon_view)
{
	gboolean labels_beside;

	if (nolphin_icon_view_supports_labels_beside_icons (icon_view)) {
		labels_beside = nolphin_icon_view_is_compact (icon_view) ||
			g_settings_get_boolean (nolphin_icon_view_preferences,
						NOLPHIN_PREFERENCES_ICON_VIEW_LABELS_BESIDE_ICONS);

		if (labels_beside) {
			nolphin_icon_container_set_label_position
				(get_icon_container (icon_view),
				 NOLPHIN_ICON_LABEL_POSITION_BESIDE);
		} else {
			nolphin_icon_container_set_label_position
				(get_icon_container (icon_view),
				 NOLPHIN_ICON_LABEL_POSITION_UNDER);
		}
	}
}

static void
set_columns_same_width (NolphinIconView *icon_view)
{
	gboolean all_columns_same_width;

	if (nolphin_icon_view_is_compact (icon_view)) {
		all_columns_same_width = g_settings_get_boolean (nolphin_compact_view_preferences,
								 NOLPHIN_PREFERENCES_COMPACT_VIEW_ALL_COLUMNS_SAME_WIDTH);
		nolphin_icon_container_set_all_columns_same_width (get_icon_container (icon_view), all_columns_same_width);
	}
}

static void
nolphin_icon_view_begin_loading (NolphinView *view)
{
	NolphinIconView *icon_view;
	GtkWidget *icon_container;
	NolphinFile *file;
	int level;
    int h_adjust, v_adjust;
	char *sort_name, *uri;

	g_return_if_fail (NOLPHIN_IS_ICON_VIEW (view));

	icon_view = NOLPHIN_ICON_VIEW (view);
	file = nolphin_view_get_directory_as_file (view);
	uri = nolphin_file_get_uri (file);
	icon_container = GTK_WIDGET (get_icon_container (icon_view));

    nolphin_icon_container_set_ok_to_load_deferred_attrs (NOLPHIN_ICON_CONTAINER (icon_container), FALSE);

	nolphin_icon_container_begin_loading (NOLPHIN_ICON_CONTAINER (icon_container));

	nolphin_icon_container_set_allow_moves (NOLPHIN_ICON_CONTAINER (icon_container),
						 !eel_uri_is_search (uri));

	g_free (uri);

	/* Set up the zoom level from the metadata. */
	if (nolphin_view_supports_zooming (NOLPHIN_VIEW (icon_view))) {
        if (nolphin_global_preferences_get_ignore_view_metadata () && !NOLPHIN_ICON_VIEW_GET_CLASS (view)->use_grid_container) {
            if (nolphin_window_get_ignore_meta_zoom_level (nolphin_view_get_nolphin_window (view)) == -1) {
                nolphin_window_set_ignore_meta_zoom_level (nolphin_view_get_nolphin_window (view), get_default_zoom_level (icon_view));
            }

            level = nolphin_window_get_ignore_meta_zoom_level (nolphin_view_get_nolphin_window (NOLPHIN_VIEW (icon_view)));
        } else {
            sync_directory_monitor_number (icon_view, file);

            if (icon_view->details->compact) {
                level = nolphin_file_get_integer_metadata (file,
                                                        NOLPHIN_METADATA_KEY_COMPACT_VIEW_ZOOM_LEVEL,
                                                        get_default_zoom_level (icon_view));
            } else {
                level = nolphin_file_get_integer_metadata (file,
                                                        NOLPHIN_METADATA_KEY_ICON_VIEW_ZOOM_LEVEL,
                                                        get_default_zoom_level (icon_view));
    		}
        }

		nolphin_icon_view_set_zoom_level (icon_view, level, TRUE);
	}

	/* Set the sort mode.
	 * It's OK not to resort the icons because the
	 * container doesn't have any icons at this point.
	 */
	sort_name = nolphin_icon_view_get_directory_sort_by (icon_view, file);
	set_sort_criterion (icon_view, get_sort_criterion_by_metadata_text (sort_name), FALSE);
	g_free (sort_name);

	/* Set the sort direction from the metadata. */
	nolphin_icon_view_set_sort_reversed (icon_view, nolphin_icon_view_get_directory_sort_reversed (icon_view, file), FALSE);

    nolphin_icon_container_set_horizontal_layout (get_icon_container (icon_view),
                                               nolphin_icon_view_get_directory_horizontal_layout (icon_view, file));

	nolphin_icon_container_set_keep_aligned (get_icon_container (icon_view),
                    nolphin_icon_view_get_directory_keep_aligned (icon_view, file));

    nolphin_icon_view_get_directory_grid_adjusts (NOLPHIN_ICON_VIEW (view),
                                               file,
                                               &h_adjust,
                                               &v_adjust);

    nolphin_icon_container_set_grid_adjusts (get_icon_container (icon_view), h_adjust, v_adjust);

	set_labels_beside_icons (icon_view);
	set_columns_same_width (icon_view);

	/* We must set auto-layout last, because it invokes the layout_changed
	 * callback, which works incorrectly if the other layout criteria are
	 * not already set up properly (see bug 6500, e.g.)
	 */
	nolphin_icon_container_set_auto_layout
		(get_icon_container (icon_view),
		 nolphin_icon_view_get_directory_auto_layout (icon_view, file));

	/* e.g. keep aligned may have changed */
	update_layout_menus (icon_view);
}

static void
icon_view_notify_clipboard_info (NolphinClipboardMonitor *monitor,
                                 NolphinClipboardInfo *info,
                                 NolphinIconView *icon_view)
{
	GList *icon_data;

	icon_data = NULL;
	if (info && info->cut) {
		icon_data = info->files;
	}

	nolphin_icon_container_set_highlighted_for_clipboard (
							       get_icon_container (icon_view), icon_data);
}

static void
nolphin_icon_view_end_loading (NolphinView *view,
			  gboolean all_files_seen)
{
	NolphinIconView *icon_view;
	GtkWidget *icon_container;
	NolphinClipboardMonitor *monitor;
	NolphinClipboardInfo *info;

	icon_view = NOLPHIN_ICON_VIEW (view);

	icon_container = GTK_WIDGET (get_icon_container (icon_view));
	nolphin_icon_container_end_loading (NOLPHIN_ICON_CONTAINER (icon_container), all_files_seen);

	monitor = nolphin_clipboard_monitor_get ();
	info = nolphin_clipboard_monitor_get_clipboard_info (monitor);
    nolphin_icon_container_set_ok_to_load_deferred_attrs (NOLPHIN_ICON_CONTAINER (icon_container), TRUE);
	icon_view_notify_clipboard_info (monitor, info, icon_view);
}

static NolphinZoomLevel
nolphin_icon_view_get_zoom_level (NolphinView *view)
{
	g_return_val_if_fail (NOLPHIN_IS_ICON_VIEW (view), NOLPHIN_ZOOM_LEVEL_STANDARD);

	return nolphin_icon_container_get_zoom_level (get_icon_container (NOLPHIN_ICON_VIEW (view)));
}

static void
nolphin_icon_view_set_zoom_level (NolphinIconView *view,
				   NolphinZoomLevel new_level,
				   gboolean always_emit)
{
	NolphinIconContainer *icon_container;

	g_return_if_fail (NOLPHIN_IS_ICON_VIEW (view));
	g_return_if_fail (new_level >= NOLPHIN_ZOOM_LEVEL_SMALLEST &&
			  new_level <= NOLPHIN_ZOOM_LEVEL_LARGEST);

	icon_container = get_icon_container (view);
	if (nolphin_icon_container_get_zoom_level (icon_container) == new_level) {
		if (always_emit) {
			g_signal_emit_by_name (view, "zoom_level_changed");
		}
		return;
	}

    if (nolphin_global_preferences_get_ignore_view_metadata () && !NOLPHIN_ICON_VIEW_GET_CLASS (view)->use_grid_container) {
        nolphin_window_set_ignore_meta_zoom_level (nolphin_view_get_nolphin_window (NOLPHIN_VIEW (view)), new_level);
    } else {
        sync_directory_monitor_number (view, nolphin_view_get_directory_as_file (NOLPHIN_VIEW (view)));

        if (view->details->compact) {
            nolphin_file_set_integer_metadata (nolphin_view_get_directory_as_file (NOLPHIN_VIEW (view)),
                                            NOLPHIN_METADATA_KEY_COMPACT_VIEW_ZOOM_LEVEL,
                                            get_default_zoom_level (view),
                                            new_level);
        } else {
            nolphin_file_set_integer_metadata (nolphin_view_get_directory_as_file (NOLPHIN_VIEW (view)),
                                            NOLPHIN_METADATA_KEY_ICON_VIEW_ZOOM_LEVEL,
                                            get_default_zoom_level (view),
                                            new_level);
        }
    }

	nolphin_icon_container_set_zoom_level (icon_container, new_level);

	g_signal_emit_by_name (view, "zoom_level_changed");

	if (nolphin_view_get_active (NOLPHIN_VIEW (view))) {
		nolphin_view_update_menus (NOLPHIN_VIEW (view));
	}
}

static void
nolphin_icon_view_bump_zoom_level (NolphinView *view, int zoom_increment)
{
	NolphinZoomLevel new_level;

	g_return_if_fail (NOLPHIN_IS_ICON_VIEW (view));

	new_level = nolphin_icon_view_get_zoom_level (view) + zoom_increment;

	if (new_level >= NOLPHIN_ZOOM_LEVEL_SMALLEST &&
	    new_level <= NOLPHIN_ZOOM_LEVEL_LARGEST) {
		nolphin_view_zoom_to_level (view, new_level);
	}
}

static void
nolphin_icon_view_zoom_to_level (NolphinView *view,
			    NolphinZoomLevel zoom_level)
{
	NolphinIconView *icon_view;

	g_assert (NOLPHIN_IS_ICON_VIEW (view));

	icon_view = NOLPHIN_ICON_VIEW (view);
	nolphin_icon_view_set_zoom_level (icon_view, zoom_level, FALSE);
}

static void
nolphin_icon_view_restore_default_zoom_level (NolphinView *view)
{
	NolphinIconView *icon_view;

	g_return_if_fail (NOLPHIN_IS_ICON_VIEW (view));

	icon_view = NOLPHIN_ICON_VIEW (view);
	nolphin_view_zoom_to_level
		(view, get_default_zoom_level (icon_view));
}

static NolphinZoomLevel
nolphin_icon_view_get_default_zoom_level (NolphinView *view)
{
    g_return_val_if_fail (NOLPHIN_IS_ICON_VIEW (view), NOLPHIN_ZOOM_LEVEL_NULL);

    return get_default_zoom_level(NOLPHIN_ICON_VIEW (view));
}

static gboolean
nolphin_icon_view_can_zoom_in (NolphinView *view)
{
	g_return_val_if_fail (NOLPHIN_IS_ICON_VIEW (view), FALSE);

	return nolphin_icon_view_get_zoom_level (view)
		< NOLPHIN_ZOOM_LEVEL_LARGEST;
}

static gboolean
nolphin_icon_view_can_zoom_out (NolphinView *view)
{
	g_return_val_if_fail (NOLPHIN_IS_ICON_VIEW (view), FALSE);

	return nolphin_icon_view_get_zoom_level (view)
		> NOLPHIN_ZOOM_LEVEL_SMALLEST;
}

static gboolean
nolphin_icon_view_is_empty (NolphinView *view)
{
	g_assert (NOLPHIN_IS_ICON_VIEW (view));

	return nolphin_icon_container_is_empty
		(get_icon_container (NOLPHIN_ICON_VIEW (view)));
}

static GList *
nolphin_icon_view_get_selection (NolphinView *view)
{
	GList *list;

	g_return_val_if_fail (NOLPHIN_IS_ICON_VIEW (view), NULL);

	list = nolphin_icon_container_get_selection
		(get_icon_container (NOLPHIN_ICON_VIEW (view)));
	nolphin_file_list_ref (list);
	return list;
}

static GList *
nolphin_icon_view_peek_selection (NolphinView *view)
{
    GList *list;

    g_return_val_if_fail (NOLPHIN_IS_ICON_VIEW (view), NULL);

    list = nolphin_icon_container_peek_selection (get_icon_container (NOLPHIN_ICON_VIEW (view)));
    nolphin_file_list_ref (list);
    return list;
}

static gint
nolphin_icon_view_get_selection_count (NolphinView *view)
{
    g_return_val_if_fail (NOLPHIN_IS_ICON_VIEW (view), 0);

    return nolphin_icon_container_get_selection_count (get_icon_container (NOLPHIN_ICON_VIEW (view)));
}

static void
count_item (NolphinIconData *icon_data,
	    gpointer callback_data)
{
	guint *count;

	count = callback_data;
	(*count)++;
}

static guint
nolphin_icon_view_get_item_count (NolphinView *view)
{
	guint count;

	g_return_val_if_fail (NOLPHIN_IS_ICON_VIEW (view), 0);

	count = 0;

	nolphin_icon_container_for_each
		(get_icon_container (NOLPHIN_ICON_VIEW (view)),
		 count_item, &count);

	return count;
}

void
nolphin_icon_view_set_sort_criterion_by_sort_type (NolphinIconView     *icon_view,
                                                NolphinFileSortType  sort_type)
{
	const SortCriterion *sort;

	g_assert (NOLPHIN_IS_ICON_VIEW (icon_view));

	sort = get_sort_criterion_by_sort_type (sort_type);
	g_return_if_fail (sort != NULL);

	if (sort == icon_view->details->sort
	    && nolphin_icon_view_using_auto_layout (icon_view)) {
		return;
	}

	set_sort_criterion (icon_view, sort, TRUE);
	nolphin_icon_container_sort (get_icon_container (icon_view));
	nolphin_icon_view_reveal_selection (NOLPHIN_VIEW (icon_view));
}


static void
action_reversed_order_callback (GtkAction *action,
				gpointer user_data)
{
	NolphinIconView *icon_view;

	icon_view = NOLPHIN_ICON_VIEW (user_data);

	if (nolphin_icon_view_set_sort_reversed (icon_view,
			       gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)),
			       TRUE)) {
		nolphin_icon_container_sort (get_icon_container (icon_view));
		nolphin_icon_view_reveal_selection (NOLPHIN_VIEW (icon_view));
	}
}

static void
action_keep_aligned_callback (GtkAction *action,
			      gpointer user_data)
{
	NolphinIconView *icon_view;
	NolphinFile *file;
	gboolean keep_aligned;

	icon_view = NOLPHIN_ICON_VIEW (user_data);

	keep_aligned = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));

	file = nolphin_view_get_directory_as_file (NOLPHIN_VIEW (icon_view));
	nolphin_icon_view_set_directory_keep_aligned (icon_view,
						 file,
						 keep_aligned);

	nolphin_icon_container_set_keep_aligned (get_icon_container (icon_view),
						  keep_aligned);
}

static void
switch_to_manual_layout (NolphinIconView *icon_view)
{
	if (!nolphin_icon_view_using_auto_layout (icon_view)) {
		return;
	}

	icon_view->details->sort = &sort_criteria[0];

	nolphin_icon_container_set_auto_layout
		(get_icon_container (icon_view), FALSE);
}

static void
layout_changed_callback (NolphinIconContainer *container,
			 NolphinIconView *icon_view)
{
	NolphinFile *file;

	g_assert (NOLPHIN_IS_ICON_VIEW (icon_view));
	g_assert (container == get_icon_container (icon_view));

	file = nolphin_view_get_directory_as_file (NOLPHIN_VIEW (icon_view));

	if (file != NULL) {
		nolphin_icon_view_set_directory_auto_layout
			(icon_view,
			 file,
			 nolphin_icon_view_using_auto_layout (icon_view));
	}

	update_layout_menus (icon_view);
}

static gboolean
nolphin_icon_view_can_rename_file (NolphinView *view, NolphinFile *file)
{
	if (!(nolphin_icon_view_get_zoom_level (view) > NOLPHIN_ZOOM_LEVEL_SMALLEST)) {
		return FALSE;
	}

	return NOLPHIN_VIEW_CLASS(nolphin_icon_view_parent_class)->can_rename_file (view, file);
}

static void
nolphin_icon_view_start_renaming_file (NolphinView *view,
				  NolphinFile *file,
				  gboolean select_all)
{
	/* call parent class to make sure the right icon is selected */
	NOLPHIN_VIEW_CLASS(nolphin_icon_view_parent_class)->start_renaming_file (view, file, select_all);

	/* start renaming */
	nolphin_icon_container_start_renaming_selected_item
		(get_icon_container (NOLPHIN_ICON_VIEW (view)), select_all);
}

static const GtkActionEntry icon_view_entries[] = {
  /* name, stock id, label */  { "Arrange Items", NULL, N_("Objekte _anordnen") },
  /* name, stock id */         { "Clean Up", NULL,
  /* label, accelerator */       N_("Nach Namen _sortieren"), NULL,
  /* tooltip */                  N_("Die Symbole neu anordnen, damit sie besser ins Fenster passen und sich nicht überlagern"),
                                 G_CALLBACK (action_clean_up_callback) },
};

static const GtkToggleActionEntry icon_view_toggle_entries[] = {

  /* name, stock id */      { "Reversed Order", NULL,
  /* label, accelerator */    N_("Umge_kehrte Reihenfolge"), NULL,
  /* tooltip */               N_("Symbole in umgekehrter Reihenfolge anzeigen"),
                              G_CALLBACK (action_reversed_order_callback),
                              0 },
  /* name, stock id */      { "Keep Aligned", NULL,
  /* label, accelerator */    N_("Anordnung _feststellen"), NULL,
  /* tooltip */               N_("Symbole an einem Raster ausrichten"),
                              G_CALLBACK (action_keep_aligned_callback),
                              0 },
};

static const GtkRadioActionEntry arrange_radio_entries[] = {
  { "Manual Layout", NULL,
    N_("_Manuell"), NULL,
    N_("Symbole dort lassen, wo sie abgelegt werden"),
    NOLPHIN_FILE_SORT_NONE },
  { "Sort by Name", NULL,
    N_("Nach _Name"), NULL,
    N_("Symbole nach Name in Zeilen sortieren"),
    NOLPHIN_FILE_SORT_BY_DISPLAY_NAME },
  { "Sort by Size", NULL,
    N_("Nach _Größe"), NULL,
    N_("Symbole nach Größe in Zeilen sortieren"),
    NOLPHIN_FILE_SORT_BY_SIZE },
  { "Sort by Type", NULL,
    N_("Nach _Typ"), NULL,
    N_("Symbole nach Typ in Zeilen sortieren"),
    NOLPHIN_FILE_SORT_BY_TYPE },
  { "Sort by Detailed Type", NULL,
    N_("Nach _detailliertem Dateityp"), NULL,
    N_("Symbole nach detailliertem Dateityp in Reihen sortieren"),
    NOLPHIN_FILE_SORT_BY_DETAILED_TYPE },
  { "Sort by Modification Date", NULL,
    N_("Nach Änderungs_datum"), NULL,
    N_("Symbole nach Änderungsdatum in Zeilen sortieren"),
    NOLPHIN_FILE_SORT_BY_MTIME },
  { "Sort by Trash Time", NULL,
    N_("Nach L_öschdatum"), NULL,
    N_("Symbole nach Löschdatum in Zeilen sortieren"),
    NOLPHIN_FILE_SORT_BY_TRASHED_TIME },
  { "Sort by Extension", NULL,
    N_("Nach _Erweiterung"), NULL,
    N_("Symbole zeilenweise nach Erweiterung sortiert halten"),
    NOLPHIN_FILE_SORT_BY_EXTENSION },
};

static void
nolphin_icon_view_merge_menus (NolphinView *view)
{
	NolphinIconView *icon_view;
	GtkUIManager *ui_manager;
	GtkActionGroup *action_group;
	GtkAction *action;

        g_assert (NOLPHIN_IS_ICON_VIEW (view));

	NOLPHIN_VIEW_CLASS (nolphin_icon_view_parent_class)->merge_menus (view);

	icon_view = NOLPHIN_ICON_VIEW (view);

	ui_manager = nolphin_view_get_ui_manager (NOLPHIN_VIEW (icon_view));

	action_group = gtk_action_group_new ("IconViewActions");
	gtk_action_group_set_translation_domain (action_group, GETTEXT_PACKAGE);
	icon_view->details->icon_action_group = action_group;
	gtk_action_group_add_actions (action_group,
				      icon_view_entries, G_N_ELEMENTS (icon_view_entries),
				      icon_view);
	gtk_action_group_add_toggle_actions (action_group,
					     icon_view_toggle_entries, G_N_ELEMENTS (icon_view_toggle_entries),
					     icon_view);
	gtk_action_group_add_radio_actions (action_group,
					    arrange_radio_entries,
					    G_N_ELEMENTS (arrange_radio_entries),
					    -1,
					    G_CALLBACK (action_sort_radio_callback),
					    icon_view);

	gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);
	g_object_unref (action_group); /* owned by ui manager */

	icon_view->details->icon_merge_id =
		gtk_ui_manager_add_ui_from_resource (ui_manager, "/org/nolphin/nolphin-icon-view-ui.xml", NULL);

	/* Do one-time state-setting here; context-dependent state-setting
	 * is done in update_menus.
	 */
	if (!nolphin_icon_view_supports_auto_layout (icon_view)) {
		action = gtk_action_group_get_action (action_group,
						      NOLPHIN_ACTION_ARRANGE_ITEMS);
		gtk_action_set_visible (action, FALSE);
	}

	update_layout_menus (icon_view);
}

static void
nolphin_icon_view_unmerge_menus (NolphinView *view)
{
	NolphinIconView *icon_view;
	GtkUIManager *ui_manager;

	icon_view = NOLPHIN_ICON_VIEW (view);

	NOLPHIN_VIEW_CLASS (nolphin_icon_view_parent_class)->unmerge_menus (view);

	ui_manager = nolphin_view_get_ui_manager (view);
	if (ui_manager != NULL) {
		nolphin_ui_unmerge_ui (ui_manager,
					&icon_view->details->icon_merge_id,
					&icon_view->details->icon_action_group);
	}
}

static void
nolphin_icon_view_update_menus (NolphinView *view)
{
    NolphinIconView *icon_view;
    GtkAction *action;
    gboolean editable;

    icon_view = NOLPHIN_ICON_VIEW (view);

	NOLPHIN_VIEW_CLASS (nolphin_icon_view_parent_class)->update_menus(view);

	editable = nolphin_view_is_editable (view);
	action = gtk_action_group_get_action (icon_view->details->icon_action_group,
					      NOLPHIN_ACTION_MANUAL_LAYOUT);
	gtk_action_set_sensitive (action, editable);
}

static void
nolphin_icon_view_reset_to_defaults (NolphinView *view)
{
	NolphinIconContainer *icon_container;
	NolphinIconView *icon_view;

	icon_view = NOLPHIN_ICON_VIEW (view);
	icon_container = get_icon_container (icon_view);

	clear_sort_criterion (icon_view);
	nolphin_icon_container_set_keep_aligned
		(icon_container, get_default_directory_keep_aligned ());

	nolphin_icon_container_sort (icon_container);

	update_layout_menus (icon_view);

	nolphin_icon_view_restore_default_zoom_level (view);

    if (nolphin_global_preferences_get_ignore_view_metadata ()) {
        NolphinWindow *window = nolphin_view_get_nolphin_window (view);
        nolphin_window_set_ignore_meta_zoom_level (window, NOLPHIN_ZOOM_LEVEL_NULL);
    }
}

static void
nolphin_icon_view_select_all (NolphinView *view)
{
	NolphinIconContainer *icon_container;

	g_return_if_fail (NOLPHIN_IS_ICON_VIEW (view));

	icon_container = get_icon_container (NOLPHIN_ICON_VIEW (view));
        nolphin_icon_container_select_all (icon_container);
}

static void
nolphin_icon_view_reveal_selection (NolphinView *view)
{
	GList *selection;

	g_return_if_fail (NOLPHIN_IS_ICON_VIEW (view));

        selection = nolphin_view_get_selection (view);

	/* Make sure at least one of the selected items is scrolled into view */
	if (selection != NULL) {
		nolphin_icon_container_reveal
			(get_icon_container (NOLPHIN_ICON_VIEW (view)),
			 selection->data);
	}

        nolphin_file_list_free (selection);
}

static GArray *
nolphin_icon_view_get_selected_icon_locations (NolphinView *view)
{
	g_return_val_if_fail (NOLPHIN_IS_ICON_VIEW (view), NULL);

	return nolphin_icon_container_get_selected_icon_locations
		(get_icon_container (NOLPHIN_ICON_VIEW (view)));
}


static void
nolphin_icon_view_set_selection (NolphinView *view, GList *selection)
{
	g_return_if_fail (NOLPHIN_IS_ICON_VIEW (view));

	nolphin_icon_container_set_selection
		(get_icon_container (NOLPHIN_ICON_VIEW (view)), selection);
}

static void
nolphin_icon_view_invert_selection (NolphinView *view)
{
	g_return_if_fail (NOLPHIN_IS_ICON_VIEW (view));

	nolphin_icon_container_invert_selection
		(get_icon_container (NOLPHIN_ICON_VIEW (view)));
}

static gboolean
nolphin_icon_view_using_manual_layout (NolphinView *view)
{
	g_return_val_if_fail (NOLPHIN_IS_ICON_VIEW (view), FALSE);

	return !nolphin_icon_view_using_auto_layout (NOLPHIN_ICON_VIEW (view));
}

static void
nolphin_icon_view_widget_to_file_operation_position (NolphinView *view,
						GdkPoint *position)
{
	g_assert (NOLPHIN_IS_ICON_VIEW (view));

	nolphin_icon_container_widget_to_file_operation_position
		(get_icon_container (NOLPHIN_ICON_VIEW (view)), position);
}

static void
icon_container_activate_callback (NolphinIconContainer *container,
				  GList *file_list,
				  NolphinIconView *icon_view)
{
	g_assert (NOLPHIN_IS_ICON_VIEW (icon_view));
	g_assert (container == get_icon_container (icon_view));

	nolphin_view_activate_files (NOLPHIN_VIEW (icon_view),
				      file_list,
				      0, TRUE);
}

static void
icon_container_activate_previewer_callback (NolphinIconContainer *container,
					    GList *file_list,
					    GArray *locations,
					    NolphinIconView *icon_view)
{
	g_assert (NOLPHIN_IS_ICON_VIEW (icon_view));
	g_assert (container == get_icon_container (icon_view));

	nolphin_view_preview_files (NOLPHIN_VIEW (icon_view),
				     file_list, locations);
}

/* this is called in one of these cases:
 * - we activate with enter holding shift
 * - we activate with space holding shift
 * - we double click an icon holding shift
 * - we middle click an icon
 *
 * If we don't open in new windows by default, the behavior should be
 * - middle click, shift + activate -> open in new tab
 * - shift + double click -> open in new window
 *
 * If we open in new windows by default, the behaviour should be
 * - middle click, or shift + activate, or shift + double-click -> close parent
 */
static void
icon_container_activate_alternate_callback (NolphinIconContainer *container,
					    GList *file_list,
					    NolphinIconView *icon_view)
{
	GdkEvent *event;
	GdkEventButton *button_event;
	GdkEventKey *key_event;
	gboolean open_in_tab, open_in_window, close_behind;
	NolphinWindowOpenFlags flags;

	g_assert (NOLPHIN_IS_ICON_VIEW (icon_view));
	g_assert (container == get_icon_container (icon_view));

	flags = 0;
	event = gtk_get_current_event ();
	open_in_tab = FALSE;
	open_in_window = FALSE;
	close_behind = FALSE;

	if (g_settings_get_boolean (nolphin_preferences,
				    NOLPHIN_PREFERENCES_ALWAYS_USE_BROWSER)) {
		if (event->type == GDK_BUTTON_PRESS ||
		    event->type == GDK_BUTTON_RELEASE ||
		    event->type == GDK_2BUTTON_PRESS ||
		    event->type == GDK_3BUTTON_PRESS) {
			button_event = (GdkEventButton *) event;
			open_in_window = ((button_event->state & GDK_SHIFT_MASK) != 0);
			open_in_tab = !open_in_window;
		} else if (event->type == GDK_KEY_PRESS ||
			   event->type == GDK_KEY_RELEASE) {
			key_event = (GdkEventKey *) event;
			open_in_tab = ((key_event->state & GDK_SHIFT_MASK) != 0);
		}
	} else {
		close_behind = TRUE;
	}

	if (open_in_tab) {
		flags |= NOLPHIN_WINDOW_OPEN_FLAG_NEW_TAB;
	}

	if (open_in_window) {
		flags |= NOLPHIN_WINDOW_OPEN_FLAG_NEW_WINDOW;
	}

	if (close_behind) {
		flags |= NOLPHIN_WINDOW_OPEN_FLAG_CLOSE_BEHIND;
	}

	DEBUG ("Activate alternate, open in tab %d, close behind %d, new window %d\n",
	       open_in_tab, close_behind, open_in_window);

	nolphin_view_activate_files (NOLPHIN_VIEW (icon_view),
				      file_list,
				      flags,
				      TRUE);
}

static void
band_select_started_callback (NolphinIconContainer *container,
			      NolphinIconView *icon_view)
{
	g_assert (NOLPHIN_IS_ICON_VIEW (icon_view));
	g_assert (container == get_icon_container (icon_view));

	nolphin_view_start_batching_selection_changes (NOLPHIN_VIEW (icon_view));
}

static void
band_select_ended_callback (NolphinIconContainer *container,
			    NolphinIconView *icon_view)
{
	g_assert (NOLPHIN_IS_ICON_VIEW (icon_view));
	g_assert (container == get_icon_container (icon_view));

	nolphin_view_stop_batching_selection_changes (NOLPHIN_VIEW (icon_view));
}

int
nolphin_icon_view_compare_files (NolphinIconView   *icon_view,
				  NolphinFile *a,
				  NolphinFile *b)
{
	NolphinView *view = NOLPHIN_VIEW (icon_view);
	NolphinIconContainer *container = nolphin_icon_view_get_icon_container (icon_view);

	if (container != NULL &&
	    nolphin_icon_container_get_filter_highlight (container) != NULL) {
		gint pos_a = nolphin_view_get_filter_match (view, a);
		gint pos_b = nolphin_view_get_filter_match (view, b);

		if (pos_a != pos_b) {
			return (pos_a < pos_b) ? -1 : 1;
		}
	}

	return nolphin_file_compare_for_sort
		(a, b, icon_view->details->sort->sort_type,
		 /* Use type-unsafe cast for performance */
		 nolphin_view_should_sort_directories_first ((NolphinView *)icon_view),
		 nolphin_view_should_sort_favorites_first ((NolphinView *)icon_view),
		 icon_view->details->sort_reversed,
         NULL);
}

static int
compare_files (NolphinView   *icon_view,
	       NolphinFile *a,
	       NolphinFile *b)
{
	return nolphin_icon_view_compare_files ((NolphinIconView *)icon_view, a, b);
}

static void
nolphin_icon_view_screen_changed (GtkWidget *widget,
				   GdkScreen *previous_screen)
{
	NolphinView *view;
	GList *files, *l;
	NolphinFile *file;
	NolphinDirectory *directory;
	NolphinIconContainer *icon_container;

	if (GTK_WIDGET_CLASS (nolphin_icon_view_parent_class)->screen_changed) {
		GTK_WIDGET_CLASS (nolphin_icon_view_parent_class)->screen_changed (widget, previous_screen);
	}

	view = NOLPHIN_VIEW (widget);
	if (NOLPHIN_ICON_VIEW (view)->details->is_desktop) {
		icon_container = get_icon_container (NOLPHIN_ICON_VIEW (view));

		directory = nolphin_view_get_model (view);
		files = nolphin_directory_get_file_list (directory);

		for (l = files; l != NULL; l = l->next) {
			file = l->data;

			if (!should_show_file_on_screen (view, file)) {
				nolphin_icon_view_remove_file (view, file, directory);
			} else {
				if (nolphin_icon_container_add (icon_container,
								 NOLPHIN_ICON_CONTAINER_ICON_DATA (file))) {
					nolphin_file_ref (file);
				}
			}
		}

		nolphin_file_list_unref (files);
		g_list_free (files);
	}
}

static gboolean
nolphin_icon_view_scroll_event (GtkWidget *widget,
			   GdkEventScroll *scroll_event)
{
	NolphinIconView *icon_view;
	GdkEvent *event_copy;
	GdkEventScroll *scroll_event_copy;
	gboolean ret;

	icon_view = NOLPHIN_ICON_VIEW (widget);

	if (icon_view->details->compact &&
	    (scroll_event->direction == GDK_SCROLL_UP ||
	     scroll_event->direction == GDK_SCROLL_DOWN ||
	     scroll_event->direction == GDK_SCROLL_SMOOTH)) {
		ret = nolphin_view_handle_scroll_event (NOLPHIN_VIEW (icon_view), scroll_event);
		if (!ret) {
			/* in column-wise layout, re-emit vertical mouse scroll events as horizontal ones,
			 * if they don't bump zoom */
			event_copy = gdk_event_copy ((GdkEvent *) scroll_event);
			scroll_event_copy = (GdkEventScroll *) event_copy;

			/* transform vertical integer smooth scroll events into horizontal events */
			if (scroll_event_copy->direction == GDK_SCROLL_SMOOTH &&
				   scroll_event_copy->delta_x == 0) {
				if (scroll_event_copy->delta_y == 1.0) {
					scroll_event_copy->direction = GDK_SCROLL_DOWN;
				} else if (scroll_event_copy->delta_y == -1.0) {
					scroll_event_copy->direction = GDK_SCROLL_UP;
				}
			}

			if (scroll_event_copy->direction == GDK_SCROLL_UP) {
				scroll_event_copy->direction = GDK_SCROLL_LEFT;
			} else if (scroll_event_copy->direction == GDK_SCROLL_DOWN) {
				scroll_event_copy->direction = GDK_SCROLL_RIGHT;
			}

			ret = GTK_WIDGET_CLASS (nolphin_icon_view_parent_class)->scroll_event (widget, scroll_event_copy);
			gdk_event_free (event_copy);
		}

		return ret;
	}

	return GTK_WIDGET_CLASS (nolphin_icon_view_parent_class)->scroll_event (widget, scroll_event);
}

static void
selection_changed_callback (NolphinIconContainer *container,
			    NolphinIconView *icon_view)
{
	g_assert (NOLPHIN_IS_ICON_VIEW (icon_view));
	g_assert (container == get_icon_container (icon_view));

	nolphin_view_notify_selection_changed (NOLPHIN_VIEW (icon_view));
}

static void
icon_container_context_click_selection_callback (NolphinIconContainer *container,
						 GdkEventButton *event,
						 NolphinIconView *icon_view)
{
	g_assert (NOLPHIN_IS_ICON_CONTAINER (container));
	g_assert (NOLPHIN_IS_ICON_VIEW (icon_view));

	nolphin_view_pop_up_selection_context_menu
		(NOLPHIN_VIEW (icon_view), event);
}

static void
icon_container_context_click_background_callback (NolphinIconContainer *container,
						  GdkEventButton *event,
						  NolphinIconView *icon_view)
{
	g_assert (NOLPHIN_IS_ICON_CONTAINER (container));
	g_assert (NOLPHIN_IS_ICON_VIEW (icon_view));

	nolphin_view_pop_up_background_context_menu
		(NOLPHIN_VIEW (icon_view), event);
}

static gboolean
nolphin_icon_view_react_to_icon_change_idle_callback (gpointer data)
{
        NolphinIconView *icon_view;

        g_assert (NOLPHIN_IS_ICON_VIEW (data));

        icon_view = NOLPHIN_ICON_VIEW (data);
        icon_view->details->react_to_icon_change_idle_id = 0;

	/* Rebuild the menus since some of them (e.g. Restore Stretched Icons)
	 * may be different now.
	 */
	nolphin_view_update_menus (NOLPHIN_VIEW (icon_view));

        /* Don't call this again (unless rescheduled) */
        return FALSE;
}

static void
icon_position_changed_callback (NolphinIconContainer *container,
				NolphinFile *file,
				const NolphinIconPosition *position,
				NolphinIconView *icon_view)
{
	char scale_string[G_ASCII_DTOSTR_BUF_SIZE];

	g_assert (NOLPHIN_IS_ICON_VIEW (icon_view));
	g_assert (container == get_icon_container (icon_view));
	g_assert (NOLPHIN_IS_FILE (file));

	/* Schedule updating menus for the next idle. Doing it directly here
	 * noticeably slows down icon stretching.  The other work here to
	 * store the icon position and scale does not seem to noticeably
	 * slow down icon stretching. It would be trickier to move to an
	 * idle call, because we'd have to keep track of potentially multiple
	 * sets of file/geometry info.
	 */
	if (nolphin_view_get_active (NOLPHIN_VIEW (icon_view)) &&
	    icon_view->details->react_to_icon_change_idle_id == 0) {
                icon_view->details->react_to_icon_change_idle_id
                        = g_idle_add (nolphin_icon_view_react_to_icon_change_idle_callback,
				      icon_view);
	}

	/* Store the new position of the icon in the metadata. */
	if (!nolphin_file_get_is_desktop_orphan (file)) {
		nolphin_file_set_position (file, position->x, position->y);
        nolphin_file_set_monitor_number (file, position->monitor);
	}

	g_ascii_dtostr (scale_string, sizeof (scale_string), position->scale);

    sync_directory_monitor_number (icon_view, file);

	nolphin_file_set_metadata (file, NOLPHIN_METADATA_KEY_ICON_SCALE, "1.0", scale_string);
}

/* Attempt to change the filename to the new text.  Notify user if operation fails. */
static void
icon_rename_ended_cb (NolphinIconContainer *container,
		      NolphinFile *file,
		      const char *new_name,
		      NolphinIconView *icon_view)
{
	g_assert (NOLPHIN_IS_FILE (file));

	nolphin_view_set_is_renaming (NOLPHIN_VIEW (icon_view), FALSE);

	/* Don't allow a rename with an empty string. Revert to original
	 * without notifying the user.
	 */
	if ((new_name == NULL) || (new_name[0] == '\0')) {
		return;
	}

	nolphin_rename_file (file, new_name, NULL, NULL);
}

static void
icon_rename_started_cb (NolphinIconContainer *container,
			GtkWidget *widget,
			gpointer callback_data)
{
	NolphinView *directory_view;

	directory_view = NOLPHIN_VIEW (callback_data);
	nolphin_clipboard_set_up_editable
		(GTK_EDITABLE (widget),
		 nolphin_view_get_ui_manager (directory_view),
		 FALSE);
}

static char *
get_icon_uri_callback (NolphinIconContainer *container,
		       NolphinFile *file,
		       NolphinIconView *icon_view)
{
	g_assert (NOLPHIN_IS_ICON_CONTAINER (container));
	g_assert (NOLPHIN_IS_FILE (file));
	g_assert (NOLPHIN_IS_ICON_VIEW (icon_view));

	return nolphin_file_get_local_uri (file);
}

static char *
get_icon_drop_target_uri_callback (NolphinIconContainer *container,
		       		   NolphinFile *file,
		       		   NolphinIconView *icon_view)
{
	g_return_val_if_fail (NOLPHIN_IS_ICON_CONTAINER (container), NULL);
	g_return_val_if_fail (NOLPHIN_IS_FILE (file), NULL);
	g_return_val_if_fail (NOLPHIN_IS_ICON_VIEW (icon_view), NULL);

	return nolphin_file_get_drop_target_uri (file);
}

/* Preferences changed callbacks */
static void
nolphin_icon_view_click_policy_changed (NolphinView *directory_view)
{
    g_assert (NOLPHIN_IS_ICON_VIEW (directory_view));

    nolphin_icon_view_update_click_mode (NOLPHIN_ICON_VIEW (directory_view));
}

static void
nolphin_icon_view_click_to_rename_mode_changed (NolphinView *directory_view)
{
    g_assert (NOLPHIN_IS_ICON_VIEW (directory_view));

    nolphin_icon_view_update_click_to_rename_mode (NOLPHIN_ICON_VIEW (directory_view));
}

static void
image_display_policy_changed_callback (gpointer callback_data)
{
	NolphinIconView *icon_view;

	icon_view = NOLPHIN_ICON_VIEW (callback_data);

	nolphin_icon_container_request_update_all (get_icon_container (icon_view));
}

static void
text_attribute_names_changed_callback (gpointer callback_data)
{
	NolphinIconView *icon_view;

	icon_view = NOLPHIN_ICON_VIEW (callback_data);

	nolphin_icon_container_request_update_all (get_icon_container (icon_view));
}

static void
default_sort_order_changed_callback (gpointer callback_data)
{
	NolphinIconView *icon_view;
	NolphinFile *file;
	char *sort_name;
	NolphinIconContainer *icon_container;

	g_return_if_fail (NOLPHIN_IS_ICON_VIEW (callback_data));

	icon_view = NOLPHIN_ICON_VIEW (callback_data);

	file = nolphin_view_get_directory_as_file (NOLPHIN_VIEW (icon_view));
	sort_name = nolphin_icon_view_get_directory_sort_by (icon_view, file);
	set_sort_criterion (icon_view, get_sort_criterion_by_metadata_text (sort_name), FALSE);
	g_free (sort_name);

	icon_container = get_icon_container (icon_view);
	g_return_if_fail (NOLPHIN_IS_ICON_CONTAINER (icon_container));

	nolphin_icon_container_request_update_all (icon_container);
}

static void
default_sort_in_reverse_order_changed_callback (gpointer callback_data)
{
	NolphinIconView *icon_view;
	NolphinFile *file;
	NolphinIconContainer *icon_container;

	g_return_if_fail (NOLPHIN_IS_ICON_VIEW (callback_data));

	icon_view = NOLPHIN_ICON_VIEW (callback_data);

	file = nolphin_view_get_directory_as_file (NOLPHIN_VIEW (icon_view));
	nolphin_icon_view_set_sort_reversed (icon_view, nolphin_icon_view_get_directory_sort_reversed (icon_view, file), FALSE);
	icon_container = get_icon_container (icon_view);
	g_return_if_fail (NOLPHIN_IS_ICON_CONTAINER (icon_container));

	nolphin_icon_container_request_update_all (icon_container);
}

static void
default_zoom_level_changed_callback (gpointer callback_data)
{
	NolphinIconView *icon_view;
	NolphinFile *file;
	int level;

	g_return_if_fail (NOLPHIN_IS_ICON_VIEW (callback_data));

	icon_view = NOLPHIN_ICON_VIEW (callback_data);

	if (nolphin_view_supports_zooming (NOLPHIN_VIEW (icon_view))) {
		file = nolphin_view_get_directory_as_file (NOLPHIN_VIEW (icon_view));

        if (nolphin_global_preferences_get_ignore_view_metadata () &&
            nolphin_window_get_ignore_meta_zoom_level (nolphin_view_get_nolphin_window (NOLPHIN_VIEW (icon_view))) > -1) {
            level = nolphin_window_get_ignore_meta_zoom_level (nolphin_view_get_nolphin_window (NOLPHIN_VIEW (icon_view)));
        } else {
            sync_directory_monitor_number (icon_view, file);

            if (nolphin_icon_view_is_compact (icon_view)) {
                level = nolphin_file_get_integer_metadata (file,
                                                        NOLPHIN_METADATA_KEY_COMPACT_VIEW_ZOOM_LEVEL,
                                                        get_default_zoom_level (icon_view));
            } else {
                level = nolphin_file_get_integer_metadata (file,
                                                        NOLPHIN_METADATA_KEY_ICON_VIEW_ZOOM_LEVEL,
                                                        get_default_zoom_level (icon_view));
            }
        }
        nolphin_view_zoom_to_level (NOLPHIN_VIEW (icon_view), level);
    }
}

static void
labels_beside_icons_changed_callback (gpointer callback_data)
{
	NolphinIconView *icon_view;

	g_return_if_fail (NOLPHIN_IS_ICON_VIEW (callback_data));

	icon_view = NOLPHIN_ICON_VIEW (callback_data);

	set_labels_beside_icons (icon_view);
}

static void
all_columns_same_width_changed_callback (gpointer callback_data)
{
	NolphinIconView *icon_view;

	g_assert (NOLPHIN_IS_ICON_VIEW (callback_data));

	icon_view = NOLPHIN_ICON_VIEW (callback_data);

	set_columns_same_width (icon_view);
}


static void
nolphin_icon_view_sort_directories_first_changed (NolphinView *directory_view)
{
	NolphinIconView *icon_view;

	icon_view = NOLPHIN_ICON_VIEW (directory_view);

	if (nolphin_icon_view_using_auto_layout (icon_view)) {
		nolphin_icon_container_sort
			(get_icon_container (icon_view));
	}
}

static void
nolphin_icon_view_sort_favorites_first_changed (NolphinView *directory_view)
{
	NolphinIconView *icon_view;

	icon_view = NOLPHIN_ICON_VIEW (directory_view);

	if (nolphin_icon_view_using_auto_layout (icon_view)) {
		nolphin_icon_container_sort
			(get_icon_container (icon_view));
	}
}

static gboolean
icon_view_can_accept_item (NolphinIconContainer *container,
			   NolphinFile *target_item,
			   const char *item_uri,
			   NolphinView *view)
{
	return nolphin_drag_can_accept_item (target_item, item_uri);
}

static char *
icon_view_get_container_uri (NolphinIconContainer *container,
			     NolphinView *view)
{
	return nolphin_view_get_uri (view);
}

static void
icon_view_move_copy_items (NolphinIconContainer *container,
			   const GList *item_uris,
			   GArray *relative_item_points,
			   const char *target_dir,
			   int copy_action,
			   int x, int y,
			   NolphinView *view)
{
	nolphin_clipboard_clear_if_colliding_uris (GTK_WIDGET (view),
						    item_uris,
						    nolphin_view_get_copied_files_atom (view));
	nolphin_view_move_copy_items (view, item_uris, relative_item_points, target_dir,
				       copy_action, x, y);
}

static void
nolphin_icon_view_update_click_mode (NolphinIconView *icon_view)
{
	NolphinIconContainer	*icon_container;
	int			click_mode;

	icon_container = get_icon_container (icon_view);
	g_assert (icon_container != NULL);

	click_mode = g_settings_get_enum (nolphin_preferences, NOLPHIN_PREFERENCES_CLICK_POLICY);

	nolphin_icon_container_set_single_click_mode (icon_container,
						       click_mode == NOLPHIN_CLICK_POLICY_SINGLE);
}

static void
nolphin_icon_view_update_click_to_rename_mode (NolphinIconView *icon_view)
{
    NolphinIconContainer   *icon_container;
    gboolean enabled;

    icon_container = get_icon_container (icon_view);
    g_assert (icon_container != NULL);

    enabled = g_settings_get_boolean (nolphin_preferences, NOLPHIN_PREFERENCES_CLICK_TO_RENAME);

    nolphin_icon_container_set_click_to_rename_enabled (icon_container,
                                                     enabled);
}

static gboolean
get_stored_layout_timestamp (NolphinIconContainer *container,
			     NolphinIconData *icon_data,
			     time_t *timestamp,
			     NolphinIconView *view)
{
	NolphinFile *file;
	NolphinDirectory *directory;

	if (icon_data == NULL) {
		directory = nolphin_view_get_model (NOLPHIN_VIEW (view));
		if (directory == NULL) {
			return FALSE;
		}

		file = nolphin_directory_get_corresponding_file (directory);

        sync_directory_monitor_number (view, file);

        *timestamp = nolphin_file_get_time_metadata (file, NOLPHIN_METADATA_KEY_ICON_VIEW_LAYOUT_TIMESTAMP);

		nolphin_file_unref (file);
	} else {
        sync_directory_monitor_number (view, NOLPHIN_FILE (icon_data));

        *timestamp = nolphin_file_get_time_metadata (NOLPHIN_FILE (icon_data), NOLPHIN_METADATA_KEY_ICON_POSITION_TIMESTAMP);
	}

	return TRUE;
}

static gboolean
store_layout_timestamp (NolphinIconContainer *container,
			NolphinIconData *icon_data,
			const time_t *timestamp,
			NolphinIconView *view)
{
	NolphinFile *file;
	NolphinDirectory *directory;

	if (icon_data == NULL) {
		directory = nolphin_view_get_model (NOLPHIN_VIEW (view));
		if (directory == NULL) {
			return FALSE;
		}

		file = nolphin_directory_get_corresponding_file (directory);

        sync_directory_monitor_number (view, file);

		nolphin_file_set_time_metadata (file,
                                     NOLPHIN_METADATA_KEY_ICON_VIEW_LAYOUT_TIMESTAMP,
                                     (time_t) *timestamp);
		nolphin_file_unref (file);
	} else {
        sync_directory_monitor_number (view, NOLPHIN_FILE (icon_data));

		nolphin_file_set_time_metadata (NOLPHIN_FILE (icon_data),
                                     NOLPHIN_METADATA_KEY_ICON_POSITION_TIMESTAMP,
                                     (time_t) *timestamp);
	}

	return TRUE;
}

static gboolean
focus_in_event_callback (GtkWidget *widget, GdkEventFocus *event, gpointer user_data)
{
	NolphinWindowSlot *slot;
	NolphinIconView *icon_view = NOLPHIN_ICON_VIEW (user_data);

	/* make the corresponding slot (and the pane that contains it) active */
	slot = nolphin_view_get_nolphin_window_slot (NOLPHIN_VIEW (icon_view));
	nolphin_window_slot_make_hosting_pane_active (slot);

	return FALSE;
}

static gboolean
button_press_callback (GtkWidget *widget, GdkEventFocus *event, gpointer user_data)
{
    NolphinView *view = NOLPHIN_VIEW (user_data);
    GdkEventButton *event_button = (GdkEventButton *)event;
    gint selection_count = nolphin_view_get_selection_count (NOLPHIN_VIEW (view));

    if (!nolphin_view_get_active (view) && selection_count > 0) {
        NolphinWindowSlot *slot = nolphin_view_get_nolphin_window_slot (view);
        nolphin_window_slot_make_hosting_pane_active (slot);
        return GDK_EVENT_STOP;
    }

    /* double left click on blank will go to parent folder */
    if (g_settings_get_boolean (nolphin_preferences, NOLPHIN_PREFERENCES_CLICK_DOUBLE_PARENT_FOLDER) &&
        (event_button->button == 1) && (event_button->type == GDK_2BUTTON_PRESS)) {
        if (selection_count == 0) {
            NolphinWindowSlot *slot = nolphin_view_get_nolphin_window_slot (view);
            nolphin_window_slot_go_up (slot, 0);
            return GDK_EVENT_STOP;
        }
    }

    return GDK_EVENT_PROPAGATE;
}

static void
nolphin_icon_view_update_filter_text (NolphinView   *view,
                                   const char *filter_text)
{
    NolphinIconContainer *container;

    container = nolphin_icon_view_get_icon_container (NOLPHIN_ICON_VIEW (view));
    if (container != NULL) {
        nolphin_icon_container_set_filter_highlight (container, filter_text);
    }
}

static void
nolphin_icon_view_select_first (NolphinView *view)
{
    NolphinIconContainer *container;

    container = nolphin_icon_view_get_icon_container (NOLPHIN_ICON_VIEW (view));
    if (container != NULL) {
        nolphin_icon_container_select_first (container);
    }
}

static gboolean
icon_container_activate_filter_cb (NolphinIconContainer *container,
                                   GdkEvent *event,
                                   NolphinIconView *icon_view)
{
    return nolphin_view_activate_filter (NOLPHIN_VIEW (icon_view), (GdkEventKey *) event);
}

static NolphinIconContainer *
create_icon_container (NolphinIconView *icon_view)
{
	NolphinIconContainer *icon_container;

    if (NOLPHIN_ICON_VIEW_GET_CLASS (icon_view)->use_grid_container) {
        icon_container = nolphin_icon_view_grid_container_new (icon_view,
                                                            icon_view->details->is_desktop);
    } else {
        icon_container = nolphin_icon_view_container_new (icon_view,
                                                       icon_view->details->is_desktop);
    }

	icon_view->details->icon_container = GTK_WIDGET (icon_container);
	g_object_add_weak_pointer (G_OBJECT (icon_container),
				   (gpointer *) &icon_view->details->icon_container);

	gtk_widget_set_can_focus (GTK_WIDGET (icon_container), TRUE);

    g_signal_connect_object (icon_container, "button_press_event",
                 G_CALLBACK (button_press_callback), icon_view, 0);
	g_signal_connect_object (icon_container, "focus_in_event",
				 G_CALLBACK (focus_in_event_callback), icon_view, 0);
	g_signal_connect_object (icon_container, "activate",
				 G_CALLBACK (icon_container_activate_callback), icon_view, 0);
	g_signal_connect_object (icon_container, "activate_alternate",
				 G_CALLBACK (icon_container_activate_alternate_callback), icon_view, 0);
	g_signal_connect_object (icon_container, "activate_previewer",
				 G_CALLBACK (icon_container_activate_previewer_callback), icon_view, 0);
	g_signal_connect_object (icon_container, "band_select_started",
				 G_CALLBACK (band_select_started_callback), icon_view, 0);
	g_signal_connect_object (icon_container, "band_select_ended",
				 G_CALLBACK (band_select_ended_callback), icon_view, 0);
	g_signal_connect_object (icon_container, "context_click_selection",
				 G_CALLBACK (icon_container_context_click_selection_callback), icon_view, 0);
	g_signal_connect_object (icon_container, "context_click_background",
				 G_CALLBACK (icon_container_context_click_background_callback), icon_view, 0);
	g_signal_connect_object (icon_container, "icon_position_changed",
				 G_CALLBACK (icon_position_changed_callback), icon_view, 0);
	g_signal_connect_object (icon_container, "selection_changed",
				 G_CALLBACK (selection_changed_callback), icon_view, 0);
	/* FIXME: many of these should move into fm-icon-container as virtual methods */
	g_signal_connect_object (icon_container, "get_icon_uri",
				 G_CALLBACK (get_icon_uri_callback), icon_view, 0);
	g_signal_connect_object (icon_container, "get_icon_drop_target_uri",
				 G_CALLBACK (get_icon_drop_target_uri_callback), icon_view, 0);
	g_signal_connect_object (icon_container, "move_copy_items",
				 G_CALLBACK (icon_view_move_copy_items), icon_view, 0);
	g_signal_connect_object (icon_container, "get_container_uri",
				 G_CALLBACK (icon_view_get_container_uri), icon_view, 0);
	g_signal_connect_object (icon_container, "can_accept_item",
				 G_CALLBACK (icon_view_can_accept_item), icon_view, 0);
	g_signal_connect_object (icon_container, "layout_changed",
				 G_CALLBACK (layout_changed_callback), icon_view, 0);
	g_signal_connect_object (icon_container, "icon_rename_started",
				 G_CALLBACK (icon_rename_started_cb), icon_view, 0);
	g_signal_connect_object (icon_container, "icon_rename_ended",
				 G_CALLBACK (icon_rename_ended_cb), icon_view, 0);
	g_signal_connect_object (icon_container, "icon_stretch_started",
				 G_CALLBACK (nolphin_view_update_menus), icon_view,
				 G_CONNECT_SWAPPED);
	g_signal_connect_object (icon_container, "icon_stretch_ended",
				 G_CALLBACK (nolphin_view_update_menus), icon_view,
				 G_CONNECT_SWAPPED);

	g_signal_connect_object (icon_container, "get_stored_layout_timestamp",
				 G_CALLBACK (get_stored_layout_timestamp), icon_view, 0);
	g_signal_connect_object (icon_container, "store_layout_timestamp",
				 G_CALLBACK (store_layout_timestamp), icon_view, 0);
	g_signal_connect_object (icon_container, "check-filter-event",
				 G_CALLBACK (icon_container_activate_filter_cb), icon_view, 0);

	gtk_container_add (GTK_CONTAINER (icon_view),
			   GTK_WIDGET (icon_container));

	nolphin_icon_view_update_click_mode (icon_view);
    nolphin_icon_view_update_click_to_rename_mode (icon_view);

	gtk_widget_show (GTK_WIDGET (icon_container));

	return icon_container;
}

/* Handles an URL received from Mozilla */
static void
icon_view_handle_netscape_url (NolphinIconContainer *container, const char *encoded_url,
			       const char *target_uri,
			       GdkDragAction action, int x, int y, NolphinIconView *view)
{
	nolphin_view_handle_netscape_url_drop (NOLPHIN_VIEW (view),
						encoded_url, target_uri, action, x, y);
}

static void
icon_view_handle_uri_list (NolphinIconContainer *container, const char *item_uris,
			   const char *target_uri,
			   GdkDragAction action, int x, int y, NolphinIconView *view)
{
	nolphin_view_handle_uri_list_drop (NOLPHIN_VIEW (view),
					    item_uris, target_uri, action, x, y);
}

static void
icon_view_handle_text (NolphinIconContainer *container, const char *text,
		       const char *target_uri,
		       GdkDragAction action, int x, int y, NolphinIconView *view)
{
	nolphin_view_handle_text_drop (NOLPHIN_VIEW (view),
					text, target_uri, action, x, y);
}

static void
icon_view_handle_raw (NolphinIconContainer *container, const char *raw_data,
		      int length, const char *target_uri, const char *direct_save_uri,
		      GdkDragAction action, int x, int y, NolphinIconView *view)
{
	nolphin_view_handle_raw_drop (NOLPHIN_VIEW (view),
				       raw_data, length, target_uri, direct_save_uri, action, x, y);
}

static char *
icon_view_get_first_visible_file (NolphinView *view)
{
	NolphinFile *file;
	NolphinIconView *icon_view;

	icon_view = NOLPHIN_ICON_VIEW (view);

	file = NOLPHIN_FILE (nolphin_icon_container_get_first_visible_icon (get_icon_container (icon_view)));

	if (file) {
		return nolphin_file_get_uri (file);
	}

	return NULL;
}

static void
icon_view_scroll_to_file (NolphinView *view,
			  const char *uri)
{
	NolphinFile *file;
	NolphinIconView *icon_view;

	icon_view = NOLPHIN_ICON_VIEW (view);

	if (uri != NULL) {
		/* Only if existing, since we don't want to add the file to
		   the directory if it has been removed since then */
		file = nolphin_file_get_existing_by_uri (uri);
		if (file != NULL) {
			nolphin_icon_container_scroll_to_icon (get_icon_container (icon_view),
								NOLPHIN_ICON_CONTAINER_ICON_DATA (file));
			nolphin_file_unref (file);
		}
	}
}

static const char *
nolphin_icon_view_get_id (NolphinView *view)
{
	if (nolphin_icon_view_is_compact (NOLPHIN_ICON_VIEW (view))) {
		return FM_COMPACT_VIEW_ID;
	}
	if (nolphin_icon_view_is_gallery (NOLPHIN_ICON_VIEW (view))) {
		return NOLPHIN_GALLERY_VIEW_ID;
	}

	return NOLPHIN_ICON_VIEW_ID;
}

static void
set_compact_view (NolphinIconView *icon_view,
                  gboolean      compact)
{
    icon_view->details->compact = compact;

    if (compact) {
        nolphin_icon_container_set_layout_mode (get_icon_container (icon_view),
                                             gtk_widget_get_direction (GTK_WIDGET(icon_view)) == GTK_TEXT_DIR_RTL ?
                                                                                                     NOLPHIN_ICON_LAYOUT_T_B_R_L :
                                                                                                     NOLPHIN_ICON_LAYOUT_T_B_L_R);
        nolphin_icon_container_set_forced_icon_size (get_icon_container (icon_view),
                                                  NOLPHIN_COMPACT_FORCED_ICON_SIZE);
    } else {
        nolphin_icon_container_set_layout_mode (get_icon_container (icon_view),
                                             gtk_widget_get_direction (GTK_WIDGET(icon_view)) == GTK_TEXT_DIR_RTL ?
                                                                                                     NOLPHIN_ICON_LAYOUT_R_L_T_B :
                                                                                                     NOLPHIN_ICON_LAYOUT_L_R_T_B);
        nolphin_icon_container_set_forced_icon_size (get_icon_container (icon_view),
                                                  icon_view->details->gallery ? NOLPHIN_ICON_SIZE_LARGEST : 0);
    }
}

/* Galerieansicht - dieselbe Symbolansicht, nur mit erzwungener
 * großer Symbolgröße statt der über die Zoomstufe gesteuerten Größe,
 * analog zu set_compact_view() oben. */
static void
set_gallery_view (NolphinIconView *icon_view,
                  gboolean      gallery)
{
    icon_view->details->gallery = gallery;

    if (!icon_view->details->compact) {
        nolphin_icon_container_set_forced_icon_size (get_icon_container (icon_view),
                                                  gallery ? NOLPHIN_ICON_SIZE_LARGEST : 0);
    }
}

static void
nolphin_icon_view_set_property (GObject         *object,
			   guint            prop_id,
			   const GValue    *value,
			   GParamSpec      *pspec)
{
	NolphinIconView *icon_view;

	icon_view = NOLPHIN_ICON_VIEW (object);

	switch (prop_id)  {
	case PROP_COMPACT:
        set_compact_view (icon_view, g_value_get_boolean (value));
		break;
	case PROP_GALLERY:
        set_gallery_view (icon_view, g_value_get_boolean (value));
		break;
	case PROP_SUPPORTS_AUTO_LAYOUT:
		icon_view->details->supports_auto_layout = g_value_get_boolean (value);
		break;
	case PROP_IS_DESKTOP:
		icon_view->details->is_desktop = g_value_get_boolean (value);
		break;
	case PROP_SUPPORTS_KEEP_ALIGNED:
		icon_view->details->supports_keep_aligned = g_value_get_boolean (value);
		break;
	case PROP_SUPPORTS_LABELS_BESIDE_ICONS:
		icon_view->details->supports_labels_beside_icons = g_value_get_boolean (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
nolphin_icon_view_finalize (GObject *object)
{
	NolphinIconView *icon_view;

	icon_view = NOLPHIN_ICON_VIEW (object);

	g_free (icon_view->details);

	g_signal_handlers_disconnect_by_func (nolphin_preferences,
					      default_sort_order_changed_callback,
					      icon_view);
	g_signal_handlers_disconnect_by_func (nolphin_preferences,
					      default_sort_in_reverse_order_changed_callback,
					      icon_view);
	g_signal_handlers_disconnect_by_func (nolphin_preferences,
					      image_display_policy_changed_callback,
					      icon_view);

	g_signal_handlers_disconnect_by_func (nolphin_icon_view_preferences,
					      default_zoom_level_changed_callback,
					      icon_view);
	g_signal_handlers_disconnect_by_func (nolphin_icon_view_preferences,
					      labels_beside_icons_changed_callback,
					      icon_view);
	g_signal_handlers_disconnect_by_func (nolphin_icon_view_preferences,
					      text_attribute_names_changed_callback,
					      icon_view);

	g_signal_handlers_disconnect_by_func (nolphin_compact_view_preferences,
					      default_zoom_level_changed_callback,
					      icon_view);
	g_signal_handlers_disconnect_by_func (nolphin_compact_view_preferences,
					      all_columns_same_width_changed_callback,
					      icon_view);

	G_OBJECT_CLASS (nolphin_icon_view_parent_class)->finalize (object);
}

static void
nolphin_icon_view_constructed (GObject *object)
{
    NolphinIconView *icon_view;
    NolphinIconContainer *icon_container;

    icon_view = NOLPHIN_ICON_VIEW (object);

    G_OBJECT_CLASS (nolphin_icon_view_parent_class)->constructed (G_OBJECT (icon_view));

    g_return_if_fail (gtk_bin_get_child (GTK_BIN (icon_view)) == NULL);

    icon_container = create_icon_container (icon_view);

    /* Set our default layout mode */
    if (!icon_view->details->is_desktop) {
        nolphin_icon_container_set_layout_mode (icon_container,
                                             gtk_widget_get_direction (GTK_WIDGET(icon_container)) == GTK_TEXT_DIR_RTL ?
                                             NOLPHIN_ICON_LAYOUT_R_L_T_B :
                                             NOLPHIN_ICON_LAYOUT_L_R_T_B);
    }

    g_signal_connect_swapped (nolphin_preferences,
                  "changed::" NOLPHIN_PREFERENCES_DEFAULT_SORT_ORDER,
                  G_CALLBACK (default_sort_order_changed_callback),
                  icon_view);
    g_signal_connect_swapped (nolphin_preferences,
                  "changed::" NOLPHIN_PREFERENCES_DEFAULT_SORT_IN_REVERSE_ORDER,
                  G_CALLBACK (default_sort_in_reverse_order_changed_callback),
                  icon_view);
    g_signal_connect_swapped (nolphin_preferences,
                  "changed::" NOLPHIN_PREFERENCES_SHOW_IMAGE_FILE_THUMBNAILS,
                  G_CALLBACK (image_display_policy_changed_callback),
                  icon_view);
    g_signal_connect_swapped (nolphin_icon_view_preferences,
                  "changed::" NOLPHIN_PREFERENCES_ICON_VIEW_DEFAULT_ZOOM_LEVEL,
                  G_CALLBACK (default_zoom_level_changed_callback),
                  icon_view);
    g_signal_connect_swapped (nolphin_icon_view_preferences,
                  "changed::" NOLPHIN_PREFERENCES_ICON_VIEW_LABELS_BESIDE_ICONS,
                  G_CALLBACK (labels_beside_icons_changed_callback),
                  icon_view);
    g_signal_connect_swapped (nolphin_icon_view_preferences,
                  "changed::" NOLPHIN_PREFERENCES_ICON_VIEW_CAPTIONS,
                  G_CALLBACK (text_attribute_names_changed_callback),
                  icon_view);

    g_signal_connect_swapped (nolphin_compact_view_preferences,
                  "changed::" NOLPHIN_PREFERENCES_COMPACT_VIEW_DEFAULT_ZOOM_LEVEL,
                  G_CALLBACK (default_zoom_level_changed_callback),
                  icon_view);
    g_signal_connect_swapped (nolphin_compact_view_preferences,
                  "changed::" NOLPHIN_PREFERENCES_COMPACT_VIEW_ALL_COLUMNS_SAME_WIDTH,
                  G_CALLBACK (all_columns_same_width_changed_callback),
                  icon_view);

    g_signal_connect_object (get_icon_container (icon_view), "handle_netscape_url",
                 G_CALLBACK (icon_view_handle_netscape_url), icon_view, 0);
    g_signal_connect_object (get_icon_container (icon_view), "handle_uri_list",
                 G_CALLBACK (icon_view_handle_uri_list), icon_view, 0);
    g_signal_connect_object (get_icon_container (icon_view), "handle_text",
                 G_CALLBACK (icon_view_handle_text), icon_view, 0);
    g_signal_connect_object (get_icon_container (icon_view), "handle_raw",
                 G_CALLBACK (icon_view_handle_raw), icon_view, 0);

    icon_view->details->clipboard_handler_id =
        g_signal_connect (nolphin_clipboard_monitor_get (),
                          "clipboard_info",
                          G_CALLBACK (icon_view_notify_clipboard_info), icon_view);

    nolphin_icon_container_set_is_desktop (icon_container, FALSE);
}

static const gchar *
nolphin_icon_view_get_sort_attribute (NolphinView *view)
{
	NolphinIconView *icon_view = NOLPHIN_ICON_VIEW (view);
	return icon_view->details->sort->metadata_text;
}

static void
nolphin_icon_view_class_init (NolphinIconViewClass *klass)
{
	NolphinViewClass *nolphin_view_class;
	GObjectClass *oclass;

    klass->use_grid_container = FALSE;

	nolphin_view_class = NOLPHIN_VIEW_CLASS (klass);
	oclass = G_OBJECT_CLASS (klass);

	oclass->set_property = nolphin_icon_view_set_property;
	oclass->finalize = nolphin_icon_view_finalize;
    oclass->constructed = nolphin_icon_view_constructed;

	GTK_WIDGET_CLASS (klass)->destroy = nolphin_icon_view_destroy;
	GTK_WIDGET_CLASS (klass)->screen_changed = nolphin_icon_view_screen_changed;
	GTK_WIDGET_CLASS (klass)->scroll_event = nolphin_icon_view_scroll_event;

	nolphin_view_class->add_file = nolphin_icon_view_add_file;
	nolphin_view_class->begin_loading = nolphin_icon_view_begin_loading;
	nolphin_view_class->bump_zoom_level = nolphin_icon_view_bump_zoom_level;
	nolphin_view_class->can_rename_file = nolphin_icon_view_can_rename_file;
	nolphin_view_class->can_zoom_in = nolphin_icon_view_can_zoom_in;
	nolphin_view_class->can_zoom_out = nolphin_icon_view_can_zoom_out;
	nolphin_view_class->clear = nolphin_icon_view_clear;
	nolphin_view_class->end_loading = nolphin_icon_view_end_loading;
	nolphin_view_class->file_changed = nolphin_icon_view_file_changed;
	nolphin_view_class->get_selected_icon_locations = nolphin_icon_view_get_selected_icon_locations;
    nolphin_view_class->get_selection = nolphin_icon_view_get_selection;
    nolphin_view_class->peek_selection = nolphin_icon_view_peek_selection;
	nolphin_view_class->get_selection_count = nolphin_icon_view_get_selection_count;
	nolphin_view_class->get_selection_for_file_transfer = nolphin_icon_view_get_selection;
	nolphin_view_class->get_item_count = nolphin_icon_view_get_item_count;
	nolphin_view_class->is_empty = nolphin_icon_view_is_empty;
	nolphin_view_class->remove_file = nolphin_icon_view_remove_file;
	nolphin_view_class->reset_to_defaults = nolphin_icon_view_reset_to_defaults;
	nolphin_view_class->restore_default_zoom_level = nolphin_icon_view_restore_default_zoom_level;
    nolphin_view_class->get_default_zoom_level = nolphin_icon_view_get_default_zoom_level;
	nolphin_view_class->reveal_selection = nolphin_icon_view_reveal_selection;
	nolphin_view_class->select_all = nolphin_icon_view_select_all;
	nolphin_view_class->set_selection = nolphin_icon_view_set_selection;
	nolphin_view_class->invert_selection = nolphin_icon_view_invert_selection;
	nolphin_view_class->compare_files = compare_files;
	nolphin_view_class->update_filter_text = nolphin_icon_view_update_filter_text;
	nolphin_view_class->select_first = nolphin_icon_view_select_first;
	nolphin_view_class->zoom_to_level = nolphin_icon_view_zoom_to_level;
	nolphin_view_class->get_zoom_level = nolphin_icon_view_get_zoom_level;
        nolphin_view_class->click_policy_changed = nolphin_icon_view_click_policy_changed;
        nolphin_view_class->click_to_rename_mode_changed = nolphin_icon_view_click_to_rename_mode_changed;
        nolphin_view_class->merge_menus = nolphin_icon_view_merge_menus;
        nolphin_view_class->unmerge_menus = nolphin_icon_view_unmerge_menus;
        nolphin_view_class->sort_directories_first_changed = nolphin_icon_view_sort_directories_first_changed;
        nolphin_view_class->sort_favorites_first_changed = nolphin_icon_view_sort_favorites_first_changed;
        nolphin_view_class->start_renaming_file = nolphin_icon_view_start_renaming_file;
        nolphin_view_class->update_menus = nolphin_icon_view_update_menus;
	nolphin_view_class->using_manual_layout = nolphin_icon_view_using_manual_layout;
	nolphin_view_class->widget_to_file_operation_position = nolphin_icon_view_widget_to_file_operation_position;
	nolphin_view_class->get_view_id = nolphin_icon_view_get_id;
	nolphin_view_class->get_first_visible_file = icon_view_get_first_visible_file;
	nolphin_view_class->scroll_to_file = icon_view_scroll_to_file;
	nolphin_view_class->get_sort_attribute = nolphin_icon_view_get_sort_attribute;

	properties[PROP_COMPACT] =
		g_param_spec_boolean ("compact",
				      "Compact",
				      "Whether this view provides a compact listing",
				      FALSE,
				      G_PARAM_WRITABLE);
	properties[PROP_GALLERY] =
		g_param_spec_boolean ("gallery",
				      "Gallery",
				      "Whether this view forces large gallery-style icons",
				      FALSE,
				      G_PARAM_WRITABLE);
	properties[PROP_SUPPORTS_AUTO_LAYOUT] =
		g_param_spec_boolean ("supports-auto-layout",
				      "Supports auto layout",
				      "Whether this view supports auto layout",
				      TRUE,
				      G_PARAM_WRITABLE |
				      G_PARAM_CONSTRUCT_ONLY);
	properties[PROP_IS_DESKTOP] =
		g_param_spec_boolean ("is-desktop",
				      "Is a desktop view",
				      "Whether this view is on a desktop",
				      FALSE,
				      G_PARAM_WRITABLE |
				      G_PARAM_CONSTRUCT_ONLY);
	properties[PROP_SUPPORTS_KEEP_ALIGNED] =
		g_param_spec_boolean ("supports-keep-aligned",
				      "Supports keep aligned",
				      "Whether this view supports keep aligned",
				      FALSE,
				      G_PARAM_WRITABLE |
				      G_PARAM_CONSTRUCT_ONLY);
	properties[PROP_SUPPORTS_LABELS_BESIDE_ICONS] =
		g_param_spec_boolean ("supports-labels-beside-icons",
				      "Supports labels beside icons",
				      "Whether this view supports labels beside icons",
				      TRUE,
				      G_PARAM_WRITABLE |
				      G_PARAM_CONSTRUCT_ONLY);

	g_object_class_install_properties (oclass, NUM_PROPERTIES, properties);
}

static void
nolphin_icon_view_init (NolphinIconView *icon_view)
{
    icon_view->details = g_new0 (NolphinIconViewDetails, 1);
    icon_view->details->sort = &sort_criteria[0];
}

static NolphinView *
nolphin_icon_view_create (NolphinWindowSlot *slot)
{
	NolphinIconView *view;

	view = g_object_new (NOLPHIN_TYPE_ICON_VIEW,
			     "window-slot", slot,
			     NULL);
#if GTK_CHECK_VERSION (3, 20, 0)
	gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET(view)), GTK_STYLE_CLASS_VIEW);
#endif

    set_compact_view (view, FALSE);

	return NOLPHIN_VIEW (view);
}

static NolphinView *
nolphin_compact_view_create (NolphinWindowSlot *slot)
{
	NolphinIconView *view;

	view = g_object_new (NOLPHIN_TYPE_ICON_VIEW,
			     "window-slot", slot,
			     NULL);
#if GTK_CHECK_VERSION (3, 20, 0)
	gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET(view)), GTK_STYLE_CLASS_VIEW);
#endif

    set_compact_view (view, TRUE);

    return NOLPHIN_VIEW (view);
}

static NolphinView *
nolphin_gallery_view_create (NolphinWindowSlot *slot)
{
	NolphinIconView *view;

	view = g_object_new (NOLPHIN_TYPE_ICON_VIEW,
			     "window-slot", slot,
			     NULL);
#if GTK_CHECK_VERSION (3, 20, 0)
	gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET(view)), GTK_STYLE_CLASS_VIEW);
#endif

    set_compact_view (view, FALSE);
    set_gallery_view (view, TRUE);

    return NOLPHIN_VIEW (view);
}

static gboolean
nolphin_icon_view_supports_uri (const char *uri,
			   GFileType file_type,
			   const char *mime_type)
{
	if (file_type == G_FILE_TYPE_DIRECTORY) {
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

#define TRANSLATE_VIEW_INFO(view_info)					\
	view_info.view_combo_label = _(view_info.view_combo_label);	\
	view_info.view_menu_label_with_mnemonic = _(view_info.view_menu_label_with_mnemonic); \
	view_info.error_label = _(view_info.error_label);		\
	view_info.startup_error_label = _(view_info.startup_error_label); \
	view_info.display_location_label = _(view_info.display_location_label); \


static NolphinViewInfo nolphin_icon_view = {
	(char *)NOLPHIN_ICON_VIEW_ID,
	/* translators: this is used in the view selection dropdown
	 * of navigation windows and in the preferences dialog */
	(char *)N_("Symbolansicht"),
	/* translators: this is used in the view menu */
	(char *)N_("_Symbole"),
	(char *)N_("Fehler in der Symbolansicht."),
	(char *)N_("Fehler in der Symbolansicht beim Starten."),
	(char *)N_("Diesen Ort mit der Symbolansicht anzeigen"),
	nolphin_icon_view_create,
	nolphin_icon_view_supports_uri
};

static NolphinViewInfo nolphin_compact_view = {
	(char *)FM_COMPACT_VIEW_ID,
	/* translators: this is used in the view selection dropdown
	 * of navigation windows and in the preferences dialog */
	(char *)N_("Kompaktansicht"),
	/* translators: this is used in the view menu */
	(char *)N_("_Kompakt"),
	(char *)N_("Die Kompaktansicht stieß auf einen Fehler."),
	(char *)N_("Die Kompaktansicht stieß beim Starten auf einen Fehler."),
	(char *)N_("Diesen Ort mit der Kompaktansicht anzeigen"),
	nolphin_compact_view_create,
	nolphin_icon_view_supports_uri
};

static NolphinViewInfo nolphin_gallery_view = {
	(char *)NOLPHIN_GALLERY_VIEW_ID,
	/* translators: this is used in the view selection dropdown
	 * of navigation windows and in the preferences dialog */
	(char *)N_("Galerieansicht"),
	/* translators: this is used in the view menu */
	(char *)N_("_Galerie"),
	(char *)N_("Die Galerieansicht stieß auf einen Fehler."),
	(char *)N_("Die Galerieansicht stieß beim Starten auf einen Fehler."),
	(char *)N_("Diesen Ort mit der Galerieansicht anzeigen"),
	nolphin_gallery_view_create,
	nolphin_icon_view_supports_uri
};

gboolean
nolphin_icon_view_is_compact (NolphinIconView *view)
{
	return view->details->compact;
}

gboolean
nolphin_icon_view_is_gallery (NolphinIconView *view)
{
	return view->details->gallery;
}

void
nolphin_icon_view_register (void)
{
	TRANSLATE_VIEW_INFO (nolphin_icon_view)
		nolphin_view_factory_register (&nolphin_icon_view);
}

void
nolphin_icon_view_compact_register (void)
{
	TRANSLATE_VIEW_INFO (nolphin_compact_view)
		nolphin_view_factory_register (&nolphin_compact_view);
}

void
nolphin_icon_view_gallery_register (void)
{
	TRANSLATE_VIEW_INFO (nolphin_gallery_view)
		nolphin_view_factory_register (&nolphin_gallery_view);
}

