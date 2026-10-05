/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nolphin
 *
 * Copyright (C) 2000, 2001 Eazel, Inc.
 * Copyright (C) 2005 Red Hat, Inc.
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
 * Author: John Sullivan <sullivan@eazel.com>
 *         Alexander Larsson <alexl@redhat.com>
 */

#include <config.h>

#include <locale.h> 

#include "nolphin-actions.h"
#include "nolphin-application.h"
#include "nolphin-bookmark-list.h"
#include "nolphin-bookmarks-window.h"
#include "nolphin-window-bookmarks.h"
#include "nolphin-window-private.h"
#include <libnolphin-private/nolphin-undo-manager.h>
#include <libnolphin-private/nolphin-ui-utilities.h>
#include <eel/eel-debug.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-vfs-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <glib/gi18n.h>
#include "nolphin-location-stats.h"
#include <libnolphin-private/nolphin-query.h>
#include <libnolphin-private/nolphin-search-directory.h>

#define MENU_ITEM_MAX_WIDTH_CHARS 32
#define MENU_PATH_BOOKMARKS_PLACEHOLDER	 "/MenuBar/Other Menus/Bookmarks/Bookmarks Placeholder"
#define MENU_PATH_HISTORY_PLACEHOLDER	 "/MenuBar/Other Menus/Go/HistoryMenu/History Placeholder"
#define MENU_PATH_FREQUENT_PLACEHOLDER	 "/MenuBar/Other Menus/Go/FrequentMenu/Frequent Placeholder"
#define STATS_MENU_HISTORY_MAX 15
#define STATS_MENU_FREQUENT_MAX 10

static GtkWindow *bookmarks_window = NULL;

static void refresh_bookmarks_menu (NolphinWindow *window);

static void
remove_bookmarks_for_uri_if_yes (GtkDialog *dialog, int response, gpointer callback_data)
{
	const char *uri;
	NolphinWindow *window;

	g_assert (GTK_IS_DIALOG (dialog));
	g_assert (callback_data != NULL);

	window = callback_data;

	if (response == GTK_RESPONSE_YES) {
		uri = g_object_get_data (G_OBJECT (dialog), "uri");
		nolphin_bookmark_list_delete_items_with_uri (window->details->bookmark_list, uri);
	}

	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
show_bogus_bookmark_window (NolphinWindow *window,
			    NolphinBookmark *bookmark)
{
	GtkDialog *dialog;
	GFile *location;
	char *uri_for_display;
	char *prompt;
	char *detail;

	location = nolphin_bookmark_get_location (bookmark);
	uri_for_display = g_file_get_parse_name (location);
	
	prompt = _("Sollen die Lesezeichen mit dem nicht vorhandenen Ort aus Ihrer Liste entfernt werden?");
	detail = g_strdup_printf (_("Der Ort »%s« existiert nicht."), uri_for_display);
	
	dialog = eel_show_yes_no_dialog (prompt, detail,
					 _("Lesezeichen für nicht existierenden Ort"),
					 GTK_STOCK_CANCEL,
					 GTK_WINDOW (window));

	g_signal_connect (dialog, "response",
	                  G_CALLBACK (remove_bookmarks_for_uri_if_yes), window);
	g_object_set_data_full (G_OBJECT (dialog), "uri", g_file_get_uri (location), g_free);

	gtk_dialog_set_default_response (dialog, GTK_RESPONSE_NO);

	g_object_unref (location);
	g_free (uri_for_display);
	g_free (detail);
}

static GtkWindow *
get_or_create_bookmarks_window (NolphinWindow *window)
{
	GObject *undo_manager_source;

	undo_manager_source = G_OBJECT (window);

	if (bookmarks_window == NULL) {
		bookmarks_window = create_bookmarks_window (window->details->bookmark_list,
		                                            undo_manager_source);
	} else {
		edit_bookmarks_dialog_set_signals (undo_manager_source);
	}

	return bookmarks_window;
}

/**
 * nolphin_bookmarks_exiting:
 * 
 * Last chance to save state before app exits.
 * Called when application exits; don't call from anywhere else.
 **/
void
nolphin_bookmarks_exiting (void)
{
	if (bookmarks_window != NULL) {
		nolphin_bookmarks_window_save_geometry (bookmarks_window);
		gtk_widget_destroy (GTK_WIDGET (bookmarks_window));
	}
}

/**
 * add_bookmark_for_current_location
 * 
 * Add a bookmark for the displayed location to the bookmarks menu.
 * Does nothing if there's already a bookmark for the displayed location.
 */
/* Aktuelle Suche als Datei speichern und als Lesezeichen (Seitenleiste)
 * ablegen. Rückgabe FALSE: aktueller Ort ist keine Suche. */
static gboolean
add_saved_search_bookmark (NolphinWindow *window, NolphinWindowSlot *slot)
{
	gchar *uri, *readable, *dir, *safe, *path, *file_name;
	NolphinDirectory *directory;
	NolphinQuery *query;
	GFile *location;
	NolphinBookmark *bookmark;
	gchar *name = NULL;
	guint i;

	uri = nolphin_window_slot_get_current_uri (slot);
	if (uri == NULL || !eel_uri_is_search (uri)) {
		g_free (uri);
		return FALSE;
	}

	directory = nolphin_directory_get_by_uri (uri);
	g_free (uri);
	query = directory != NULL ? nolphin_search_directory_get_query (NOLPHIN_SEARCH_DIRECTORY (directory)) : NULL;
	nolphin_directory_unref (directory);
	if (query == NULL) {
		return FALSE;
	}

	/* Kein Dialog (Vertrag 58.1: die Suche läuft im Panel): der Name ergibt sich aus
	 * der Suche selbst und lässt sich über "Lesezeichen bearbeiten" ändern. */
	readable = nolphin_query_to_readable_string (query);
	name = g_strstrip (g_strdup (readable));
	g_free (readable);
	if (name[0] == '\0') {
		g_free (name);
		name = g_strdup (_("Gespeicherte Suche"));
	}

	/* Dateiname: nur unkritische Zeichen, Eindeutigkeit über Zähler. */
	safe = g_strdup (name);
	for (i = 0; safe[i] != '\0'; i++) {
		if (safe[i] == '/' || safe[i] == '\\' || g_ascii_iscntrl (safe[i])) {
			safe[i] = '_';
		}
	}
	dir = g_build_filename (g_get_user_config_dir (), "nolphin", "saved-searches", NULL);
	g_mkdir_with_parents (dir, 0700);
	path = NULL;
	for (i = 0; i < 1000; i++) {
		g_free (path);
		file_name = i == 0 ? g_strdup_printf ("%s.nsearch", safe) : g_strdup_printf ("%s-%u.nsearch", safe, i);
		path = g_build_filename (dir, file_name, NULL);
		g_free (file_name);
		if (!g_file_test (path, G_FILE_TEST_EXISTS)) {
			break;
		}
	}

	if (nolphin_query_save (query, path)) {
		location = g_file_new_for_path (path);
		bookmark = nolphin_bookmark_new (location, name, "xsi-folder-saved-search-symbolic", NULL);
		nolphin_bookmark_list_append (window->details->bookmark_list, bookmark);
		g_object_unref (bookmark);
		g_object_unref (location);

		{
			GNotification *note = g_notification_new (_("Suche gespeichert"));
			gchar *body = g_strdup_printf (_("Als Lesezeichen »%s« in der Seitenleiste."), name);

			g_notification_set_body (note, body);
			g_application_send_notification (G_APPLICATION (nolphin_application_get_singleton ()), NULL, note);
			g_free (body);
			g_object_unref (note);
		}
	}

	g_free (path);
	g_free (dir);
	g_free (safe);
	g_free (name);
	g_object_unref (query);
	return TRUE;
}

void
nolphin_window_add_bookmark_for_current_location (NolphinWindow *window)
{
	NolphinBookmark *bookmark;
	NolphinWindowSlot *slot;
	NolphinBookmarkList *list;

	slot = nolphin_window_get_active_slot (window);

	if (add_saved_search_bookmark (window, slot)) {
		return;
	}

	bookmark = slot->current_location_bookmark;
	list = window->details->bookmark_list;

	if (!nolphin_bookmark_list_contains (list, bookmark)) {
		nolphin_bookmark_list_append (list, bookmark); 
	}
}

void
nolphin_window_edit_bookmarks (NolphinWindow *window)
{
	GtkWindow *dialog;

	dialog = get_or_create_bookmarks_window (window);

	gtk_window_set_screen (
		dialog, gtk_window_get_screen (GTK_WINDOW (window)));
        gtk_window_present (dialog);
}

static void
remove_bookmarks_menu_items (NolphinWindow *window)
{
	GtkUIManager *ui_manager;
	
	ui_manager = nolphin_window_get_ui_manager (window);
	if (window->details->bookmarks_merge_id != 0) {
		gtk_ui_manager_remove_ui (ui_manager,
					  window->details->bookmarks_merge_id);
		window->details->bookmarks_merge_id = 0;
	}
	if (window->details->bookmarks_action_group != NULL) {
		gtk_ui_manager_remove_action_group (ui_manager,
						    window->details->bookmarks_action_group);
		window->details->bookmarks_action_group = NULL;
	}
}

static void
connect_proxy_cb (GtkActionGroup *action_group,
                  GtkAction *action,
                  GtkWidget *proxy,
                  gpointer dummy)
{
	GtkLabel *label;
	const gchar *icon_name;

	if (!GTK_IS_MENU_ITEM (proxy))
		return;

	label = GTK_LABEL (gtk_bin_get_child (GTK_BIN (proxy)));

	gtk_label_set_use_underline (label, FALSE);
	gtk_label_set_ellipsize (label, PANGO_ELLIPSIZE_END);
	gtk_label_set_max_width_chars (label, MENU_ITEM_MAX_WIDTH_CHARS);

	icon_name = g_object_get_data (G_OBJECT (action), "menu-icon-name");

	if (icon_name != NULL) {
		gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (proxy),
					       gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_MENU));
	}
}

/* Struct that stores all the info necessary to activate a bookmark. */
typedef struct {
        NolphinBookmark *bookmark;
        NolphinWindow *window;
	GCallback refresh_callback;
	NolphinBookmarkFailedCallback failed_callback;
} BookmarkHolder;

static BookmarkHolder *
bookmark_holder_new (NolphinBookmark *bookmark, 
		     NolphinWindow *window,
		     GCallback refresh_callback,
		     NolphinBookmarkFailedCallback failed_callback)
{
	BookmarkHolder *new_bookmark_holder;

	new_bookmark_holder = g_new (BookmarkHolder, 1);
	new_bookmark_holder->window = window;
	new_bookmark_holder->bookmark = bookmark;
	new_bookmark_holder->failed_callback = failed_callback;
	new_bookmark_holder->refresh_callback = refresh_callback;
	/* Ref the bookmark because it might be unreffed away while 
	 * we're holding onto it (not an issue for window).
	 */
	g_object_ref (bookmark);
	g_signal_connect_object (bookmark, "notify::icon",
				 refresh_callback,
				 window, G_CONNECT_SWAPPED);
	g_signal_connect_object (bookmark, "notify::name",
				 refresh_callback,
				 window, G_CONNECT_SWAPPED);

	return new_bookmark_holder;
}

static void
bookmark_holder_free (BookmarkHolder *bookmark_holder)
{
	g_signal_handlers_disconnect_by_func (bookmark_holder->bookmark,
					      bookmark_holder->refresh_callback, bookmark_holder->window);
	g_object_unref (bookmark_holder->bookmark);
	g_free (bookmark_holder);
}

static void
bookmark_holder_free_cover (gpointer callback_data, GClosure *closure)
{
	bookmark_holder_free (callback_data);
}

static void
activate_bookmark_in_menu_item (GtkAction *action, gpointer user_data)
{
    NolphinWindowSlot *slot;
    BookmarkHolder *holder;
    GFile *location;

    holder = (BookmarkHolder *)user_data;

    location = nolphin_bookmark_get_location (holder->bookmark);
    slot = nolphin_window_get_active_slot (holder->window);
    nolphin_window_slot_open_location (slot, location, nolphin_event_get_window_open_flags ());
    g_object_unref (location);
}

void
nolphin_menus_append_bookmark_to_menu (NolphinWindow *window, 
					NolphinBookmark *bookmark, 
					const char *parent_path,
					const char *parent_id,
					guint index_in_parent,
					GtkActionGroup *action_group,
					guint merge_id,
					GCallback refresh_callback,
					NolphinBookmarkFailedCallback failed_callback)
{
	BookmarkHolder *bookmark_holder;
	char action_name[128];
	const char *name;
	gchar *icon_name;
	GtkAction *action;

	g_assert (NOLPHIN_IS_WINDOW (window));
	g_assert (NOLPHIN_IS_BOOKMARK (bookmark));

	bookmark_holder = bookmark_holder_new (bookmark, window, refresh_callback, failed_callback);
	name = nolphin_bookmark_get_name (bookmark);

	/* Create menu item with pixbuf */
	icon_name = nolphin_bookmark_get_icon_name (bookmark);

	g_snprintf (action_name, sizeof (action_name), "%s%d", parent_id, index_in_parent);

	action = gtk_action_new (action_name,
				 name,
				 _("Zum durch dieses Lesezeichen angegebenen Ort gehen"),
				 NULL);
	
	g_object_set_data_full (G_OBJECT (action), "menu-icon-name",
				icon_name,
				g_free);

	g_signal_connect_data (action, "activate",
			       G_CALLBACK (activate_bookmark_in_menu_item),
			       bookmark_holder, 
			       bookmark_holder_free_cover, 0);

	gtk_action_group_add_action (action_group,
				     GTK_ACTION (action));

	g_object_unref (action);

	gtk_ui_manager_add_ui (window->details->ui_manager,
			       merge_id,
			       parent_path,
			       action_name,
			       action_name,
			       GTK_UI_MANAGER_MENUITEM,
			       FALSE);
}

static void
update_bookmarks (NolphinWindow *window)
{
        NolphinBookmarkList *bookmarks;
	NolphinBookmark *bookmark;
	guint bookmark_count;
	guint index;
	GtkUIManager *ui_manager;

	g_assert (NOLPHIN_IS_WINDOW (window));
	g_assert (window->details->bookmarks_merge_id == 0);
	g_assert (window->details->bookmarks_action_group == NULL);

	if (window->details->bookmark_list == NULL) {
		window->details->bookmark_list = nolphin_bookmark_list_get_default ();
	}

	bookmarks = window->details->bookmark_list;

	ui_manager = nolphin_window_get_ui_manager (NOLPHIN_WINDOW (window));
	
	window->details->bookmarks_merge_id = gtk_ui_manager_new_merge_id (ui_manager);
	window->details->bookmarks_action_group = gtk_action_group_new ("BookmarksGroup");
	g_signal_connect (window->details->bookmarks_action_group, "connect-proxy",
			  G_CALLBACK (connect_proxy_cb), NULL);

	gtk_ui_manager_insert_action_group (ui_manager,
					    window->details->bookmarks_action_group,
					    -1);
	g_object_unref (window->details->bookmarks_action_group);

	/* append new set of bookmarks */
	bookmark_count = nolphin_bookmark_list_length (bookmarks);

	for (index = 0; index < bookmark_count; ++index) {
		bookmark = nolphin_bookmark_list_item_at (bookmarks, index);

		nolphin_menus_append_bookmark_to_menu
			(NOLPHIN_WINDOW (window),
			 bookmark,
			 MENU_PATH_BOOKMARKS_PLACEHOLDER,
			 "dynamic",
			 index,
			 window->details->bookmarks_action_group,
			 window->details->bookmarks_merge_id,
			 G_CALLBACK (refresh_bookmarks_menu), 
			 show_bogus_bookmark_window);
	}
}

static void
refresh_bookmarks_menu (NolphinWindow *window)
{
	g_assert (NOLPHIN_IS_WINDOW (window));

	remove_bookmarks_menu_items (window);
	update_bookmarks (window);
}

/**
 * nolphin_window_initialize_bookmarks_menu
 * 
 * Fill in bookmarks menu with stored bookmarks, and wire up signals
 * so we'll be notified when bookmark list changes.
 */
/* Verlauf und "Häufig verwendet" im Gehe-zu-Menü: aus nolphin-location-stats,
 * als dynamische Einträge nach dem Muster der Lesezeichen. */
static void
stats_menu_noop_refresh (NolphinWindow *window)
{
}

static void
fill_stats_menu (NolphinWindow *window, GList *uris, const char *path, const char *id,
		 GtkActionGroup *group, guint merge_id)
{
	GList *l;
	guint index = 0;

	for (l = uris; l != NULL; l = l->next, index++) {
		GFile *location = g_file_new_for_uri (l->data);
		gchar *label = g_file_get_parse_name (location);
		NolphinBookmark *bookmark = nolphin_bookmark_new (location, label, NULL, NULL);

		nolphin_menus_append_bookmark_to_menu (window, bookmark, path, id, index, group, merge_id,
						       G_CALLBACK (stats_menu_noop_refresh), NULL);
		g_object_unref (bookmark);
		g_object_unref (location);
		g_free (label);
	}
}

static void
refresh_stats_menus (NolphinWindow *window)
{
	GtkUIManager *ui_manager = nolphin_window_get_ui_manager (window);
	NolphinLocationStats *stats = nolphin_location_stats_get_default ();
	GtkActionGroup *old_group = g_object_get_data (G_OBJECT (window), "nolphin-stats-group");
	GtkActionGroup *group;
	guint old_id = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (window), "nolphin-stats-merge-id"));
	guint merge_id;
	GList *recent, *frequent;
	GtkAction *clear_action;

	if (old_id != 0) {
		gtk_ui_manager_remove_ui (ui_manager, old_id);
	}
	if (old_group != NULL) {
		gtk_ui_manager_remove_action_group (ui_manager, old_group);
	}

	merge_id = gtk_ui_manager_new_merge_id (ui_manager);
	group = gtk_action_group_new ("LocationStatsGroup");
	g_signal_connect (group, "connect-proxy", G_CALLBACK (connect_proxy_cb), NULL);
	gtk_ui_manager_insert_action_group (ui_manager, group, -1);

	recent = nolphin_location_stats_get_recent (stats, STATS_MENU_HISTORY_MAX);
	frequent = nolphin_location_stats_get_frequent (stats, STATS_MENU_FREQUENT_MAX);
	fill_stats_menu (window, recent, MENU_PATH_HISTORY_PLACEHOLDER, "hist", group, merge_id);
	fill_stats_menu (window, frequent, MENU_PATH_FREQUENT_PLACEHOLDER, "freq", group, merge_id);

	clear_action = gtk_action_group_get_action (nolphin_window_get_main_action_group (window), "ClearHistory");
	if (clear_action != NULL) {
		gtk_action_set_sensitive (clear_action, recent != NULL);
	}

	g_list_free_full (recent, g_free);
	g_list_free_full (frequent, g_free);

	g_object_set_data_full (G_OBJECT (window), "nolphin-stats-group", group, g_object_unref);
	g_object_set_data (G_OBJECT (window), "nolphin-stats-merge-id", GUINT_TO_POINTER (merge_id));
}

void 
nolphin_window_initialize_bookmarks_menu (NolphinWindow *window)
{
	g_assert (NOLPHIN_IS_WINDOW (window));

	refresh_bookmarks_menu (window);

	/* Recreate dynamic part of menu if bookmark list changes */
	g_signal_connect_object (window->details->bookmark_list, "changed",
				 G_CALLBACK (refresh_bookmarks_menu),
				 window, G_CONNECT_SWAPPED);

	refresh_stats_menus (window);
	g_signal_connect_object (nolphin_location_stats_get_default (), "changed",
				 G_CALLBACK (refresh_stats_menus),
				 window, G_CONNECT_SWAPPED);
}
