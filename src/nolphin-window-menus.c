/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nolphin
 *
 * Copyright (C) 2000, 2001 Eazel, Inc.
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
 */

/* nolphin-window-menus.h - implementation of nolphin window menu operations,
 *                           split into separate file just for convenience.
 */
#include <config.h>

#include <locale.h>

#include "nolphin-window-menus.h"
#include "nolphin-actions.h"
#include "nolphin-location-stats.h"
#include "nolphin-application.h"
#include "nolphin-workspace-panel.h"
#include "nolphin-connect-server-dialog.h"
#include "nolphin-file-management-properties.h"
#include "nolphin-navigation-action.h"
#include "nolphin-notebook.h"
#include "nolphin-window-manage-views.h"
#include "nolphin-window-bookmarks.h"
#include "nolphin-window-private.h"
#include "nolphin-desktop-window.h"
#include "nolphin-location-bar.h"
#include "nolphin-icon-view.h"
#include "nolphin-list-view.h"
#include "nolphin-toolbar.h"

#include <gtk/gtk.h>
#include <gio/gio.h>
#include <glib/gi18n.h>

#include <eel/eel-vfs-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-stock-dialogs.h>

#include <libnolphin-extension/nolphin-menu-provider.h>
#include <libnolphin-private/nolphin-file-utilities.h>
#include <libnolphin-private/nolphin-global-preferences.h>
#include <libnolphin-private/nolphin-icon-names.h>
#include <libnolphin-private/nolphin-ui-utilities.h>
#include <libnolphin-private/nolphin-module.h>
#include <libnolphin-private/nolphin-undo-manager.h>
#include <libnolphin-private/nolphin-program-choosing.h>
#include <libnolphin-private/nolphin-search-directory.h>
#include <libnolphin-private/nolphin-search-engine.h>
#include <libnolphin-private/nolphin-signaller.h>
#include <libnolphin-private/nolphin-trash-monitor.h>
#include <string.h>

#define MENU_PATH_EXTENSION_ACTIONS                     "/MenuBar/File/Extension Actions"
#define POPUP_PATH_EXTENSION_ACTIONS                     "/background/Before Zoom Items/Extension Actions"
#define MENU_BAR_PATH                                    "/MenuBar"

#define NETWORK_URI          "network:"
#define COMPUTER_URI         "computer:"

static void set_content_view_type(NolphinWindow *window,
                                  const gchar *view_id);

enum {
    NULL_VIEW,
    ICON_VIEW,
    LIST_VIEW,
    COMPACT_VIEW,
    GALLERY_VIEW,
    SIDEBAR_PLACES,
    SIDEBAR_TREE,
    TOOLBAR_PATHBAR,
    TOOLBAR_ENTRY
};

static void
action_close_window_slot_callback (GtkAction *action,
				   gpointer user_data)
{
	NolphinWindow *window;
	NolphinWindowSlot *slot;

	window = NOLPHIN_WINDOW (user_data);
	slot = nolphin_window_get_active_slot (window);

	nolphin_window_pane_close_slot (slot->pane, slot);
}

static void
action_duplicate_tab_callback (GtkAction *action,
			       gpointer   user_data)
{
	if (!NOLPHIN_IS_DESKTOP_WINDOW (user_data)) {
		nolphin_window_duplicate_tab (NOLPHIN_WINDOW (user_data));
	}
}

static void
action_close_all_tabs_callback (GtkAction *action,
				gpointer   user_data)
{
	if (!NOLPHIN_IS_DESKTOP_WINDOW (user_data)) {
		nolphin_window_close_all_tabs (NOLPHIN_WINDOW (user_data));
	}
}

static void
action_workspace_callback (GtkAction *action, gpointer user_data)
{
	if (!NOLPHIN_IS_DESKTOP_WINDOW (user_data)) {
		NolphinWindow *window = NOLPHIN_WINDOW (user_data);

		nolphin_workspace_panel_show_workspaces (nolphin_window_get_workspace_panel (window), window,
							  g_strcmp0 (gtk_action_get_name (action), "Workspace Save") == 0);
	}
}

static void
action_duplicate_pane_callback (GtkAction *action, gpointer user_data)
{
	if (!NOLPHIN_IS_DESKTOP_WINDOW (user_data)) {
		nolphin_window_split_view_add_pane (NOLPHIN_WINDOW (user_data));
	}
}

static void
action_maximize_pane_callback (GtkAction *action, gpointer user_data)
{
	if (!NOLPHIN_IS_DESKTOP_WINDOW (user_data)) {
		nolphin_window_toggle_maximize_pane (NOLPHIN_WINDOW (user_data));
	}
}

static void
action_close_pane_callback (GtkAction *action, gpointer user_data)
{
	if (!NOLPHIN_IS_DESKTOP_WINDOW (user_data)) {
		nolphin_window_close_active_pane (NOLPHIN_WINDOW (user_data));
	}
}

static void
action_lock_tab_callback (GtkAction *action,
			  gpointer   user_data)
{
	if (!NOLPHIN_IS_DESKTOP_WINDOW (user_data)) {
		nolphin_window_toggle_lock_tab (NOLPHIN_WINDOW (user_data));
	}
}

static void
action_rename_tab_callback (GtkAction *action,
			    gpointer   user_data)
{
	if (!NOLPHIN_IS_DESKTOP_WINDOW (user_data)) {
		nolphin_window_rename_tab (NOLPHIN_WINDOW (user_data));
	}
}

/* Bearbeiten ▸ Zwischenablage ▸ Inhalt anzeigen / Leeren */
static void
action_show_clipboard_callback (GtkAction *action,
				gpointer   user_data)
{
	GtkClipboard *clipboard = gtk_clipboard_get_for_display (gtk_widget_get_display (GTK_WIDGET (user_data)),
								 GDK_SELECTION_CLIPBOARD);
	GString *text = g_string_new (NULL);
	GtkWidget *dialog;
	gchar **uris = gtk_clipboard_wait_for_uris (clipboard);

	if (uris != NULL && uris[0] != NULL) {
		guint i;

		for (i = 0; uris[i] != NULL; i++) {
			gchar *name = g_uri_unescape_string (uris[i], NULL);

			g_string_append_printf (text, "%s\n", name != NULL ? name : uris[i]);
			g_free (name);
		}
	} else if (gtk_clipboard_wait_is_text_available (clipboard)) {
		gchar *content = gtk_clipboard_wait_for_text (clipboard);

		if (content != NULL) {
			g_string_append (text, content);
			g_free (content);
		}
	} else if (gtk_clipboard_wait_is_image_available (clipboard)) {
		g_string_append (text, _("Ein Bild"));
	}
	g_strfreev (uris);

	if (text->len == 0) {
		g_string_assign (text, _("Die Zwischenablage ist leer."));
	} else if (text->len > 2000) {
		g_string_truncate (text, 2000);
		g_string_append (text, " …");
	}

	dialog = gtk_message_dialog_new (GTK_WINDOW (user_data), GTK_DIALOG_DESTROY_WITH_PARENT,
					 GTK_MESSAGE_INFO, GTK_BUTTONS_OK, "%s", _("Inhalt der Zwischenablage"));
	/* Der Inhalt steht in einem festen, scrollbaren Feld, damit auch sehr
	 * langer Text den Dialog nicht über den Bildschirm hinaus wachsen lässt. */
	{
		GtkWidget *area = gtk_message_dialog_get_message_area (GTK_MESSAGE_DIALOG (dialog));
		GtkWidget *scroller = gtk_scrolled_window_new (NULL, NULL);
		GtkWidget *view = gtk_text_view_new ();

		gtk_text_view_set_editable (GTK_TEXT_VIEW (view), FALSE);
		gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (view), GTK_WRAP_WORD_CHAR);
		gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (view)), text->str, -1);
		gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroller), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
		gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (scroller), 60);
		gtk_scrolled_window_set_max_content_height (GTK_SCROLLED_WINDOW (scroller), 260);
		gtk_scrolled_window_set_propagate_natural_height (GTK_SCROLLED_WINDOW (scroller), TRUE);
		gtk_widget_set_size_request (scroller, 420, -1);
		gtk_container_add (GTK_CONTAINER (scroller), view);
		gtk_box_pack_start (GTK_BOX (area), scroller, TRUE, TRUE, 0);
		gtk_widget_show_all (scroller);
	}
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
	g_string_free (text, TRUE);
}

static void
action_clear_clipboard_callback (GtkAction *action,
				 gpointer   user_data)
{
	GtkClipboard *clipboard = gtk_clipboard_get_for_display (gtk_widget_get_display (GTK_WIDGET (user_data)),
								 GDK_SELECTION_CLIPBOARD);

	/* gtk_clipboard_clear () wirkt nur auf eigene Inhalte; ein leerer Text
	 * ersetzt auch Inhalte anderer Anwendungen. */
	gtk_clipboard_clear (clipboard);
	gtk_clipboard_set_text (clipboard, "", 0);
}

static void
action_restore_closed_tab_callback (GtkAction *action,
				    gpointer   user_data)
{
	if (!NOLPHIN_IS_DESKTOP_WINDOW (user_data)) {
		nolphin_window_restore_closed_tab (NOLPHIN_WINDOW (user_data));
	}
}

static void
action_connect_to_server_callback (GtkAction *action,
				   gpointer user_data)
{
	NolphinWindow *window = NOLPHIN_WINDOW (user_data);
	GtkWidget *dialog;

	dialog = nolphin_connect_server_dialog_new (window);

	gtk_widget_show (dialog);
}

static void
action_stop_callback (GtkAction *action,
		      gpointer user_data)
{
	NolphinWindow *window;
	NolphinWindowSlot *slot;

	window = NOLPHIN_WINDOW (user_data);
	slot = nolphin_window_get_active_slot (window);

	nolphin_window_slot_stop_loading (slot);
}

#ifdef TEXT_CHANGE_UNDO
static void
action_undo_callback (GtkAction *action,
		      gpointer user_data)
{
	NolphinApplication *app;

	app = nolphin_application_get_singleton ();
	nolphin_undo_manager_undo (app->undo_manager);
}
#endif

static void
action_home_callback (GtkAction *action,
		      gpointer user_data)
{
    if (!NOLPHIN_IS_DESKTOP_WINDOW (user_data)) {
        NolphinWindow *window;
        NolphinWindowSlot *slot;

        window = NOLPHIN_WINDOW (user_data);

        slot = nolphin_window_get_active_slot (window);

        nolphin_window_slot_go_home (slot, nolphin_event_get_window_open_flags ());
    }
}

static void
action_go_to_computer_callback (GtkAction *action,
				gpointer user_data)
{
	NolphinWindow *window;
	NolphinWindowSlot *slot;
	GFile *computer;

	window = NOLPHIN_WINDOW (user_data);
	slot = nolphin_window_get_active_slot (window);

	computer = g_file_new_for_uri (COMPUTER_URI);
	nolphin_window_slot_open_location (slot, computer,
					    nolphin_event_get_window_open_flags ());
	g_object_unref (computer);
}

static void
action_go_to_network_callback (GtkAction *action,
				gpointer user_data)
{
	NolphinWindow *window;
	NolphinWindowSlot *slot;
	GFile *network;

	window = NOLPHIN_WINDOW (user_data);
	slot = nolphin_window_get_active_slot (window);

	network = g_file_new_for_uri (NETWORK_URI);
	nolphin_window_slot_open_location (slot, network,
					    nolphin_event_get_window_open_flags ());
	g_object_unref (network);
}

static void
action_go_to_templates_callback (GtkAction *action,
				 gpointer user_data)
{
	NolphinWindow *window;
	NolphinWindowSlot *slot;
	char *path;
	GFile *location;

	window = NOLPHIN_WINDOW (user_data);
	slot = nolphin_window_get_active_slot (window);

	nolphin_ensure_valid_templates_directory ();
	path = nolphin_get_templates_directory ();
	location = g_file_new_for_path (path);
	g_free (path);
	nolphin_window_slot_open_location (slot, location,
					    nolphin_event_get_window_open_flags ());
	g_object_unref (location);
}

static void
action_go_to_trash_callback (GtkAction *action,
			     gpointer user_data)
{
	NolphinWindow *window;
	NolphinWindowSlot *slot;
	GFile *trash;

	window = NOLPHIN_WINDOW (user_data);
	slot = nolphin_window_get_active_slot (window);

	trash = g_file_new_for_uri ("trash:///");
	nolphin_window_slot_open_location (slot, trash,
					    nolphin_event_get_window_open_flags ());
	g_object_unref (trash);
}

static void
action_reload_callback (GtkAction *action,
			gpointer user_data)
{
	NolphinWindowSlot *slot;

	slot = nolphin_window_get_active_slot (NOLPHIN_WINDOW (user_data));
	nolphin_window_slot_queue_reload (slot, TRUE);
}

static NolphinView *
get_current_view (NolphinWindow *window)
{
	NolphinWindowSlot *slot;
	NolphinView *view;

	slot = nolphin_window_get_active_slot (window);
	view = nolphin_window_slot_get_current_view (slot);

	return view;
}

static void
action_zoom_in_callback (GtkAction *action,
			 gpointer user_data)
{
    if (!NOLPHIN_IS_DESKTOP_WINDOW (user_data)) {
        nolphin_view_bump_zoom_level (get_current_view (user_data), 1);
    }
}

static void
action_zoom_out_callback (GtkAction *action,
			  gpointer user_data)
{
    if (!NOLPHIN_IS_DESKTOP_WINDOW (user_data)) {
        nolphin_view_bump_zoom_level (get_current_view (user_data), -1);
    }
}

static void
action_zoom_normal_callback (GtkAction *action,
			     gpointer user_data)
{
    if (!NOLPHIN_IS_DESKTOP_WINDOW (user_data)) {
        nolphin_view_restore_default_zoom_level (get_current_view (user_data));
    }
}

static void
action_show_hidden_files_callback (GtkAction *action,
				   gpointer callback_data)
{
	NolphinWindow *window;
	NolphinWindowShowHiddenFilesMode mode;

	window = NOLPHIN_WINDOW (callback_data);

	if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action))) {
		mode = NOLPHIN_WINDOW_SHOW_HIDDEN_FILES_ENABLE;
	} else {
		mode = NOLPHIN_WINDOW_SHOW_HIDDEN_FILES_DISABLE;
	}

	nolphin_window_set_hidden_files_mode (window, mode);
}

static void
action_preferences_callback (GtkAction *action,
			     gpointer user_data)
{
	GtkWindow *window;

	window = GTK_WINDOW (user_data);

	nolphin_file_management_properties_dialog_show (window, NULL);
}

static void
action_about_nolphin_callback (GtkAction *action,
				gpointer user_data)
{
	const gchar *license[] = {
		N_("Nolphin ist freie Software und kann, unter den Bedingungen der GNU General Public License wie durch die Freie Software Stiftung veröffentlicht, als solche nach belieben weiter verteilt und/oder modifiziert werden; entweder unter Version 2 der Lizenz, oder (wie Sie möchten) gemäß irgend einer späteren Version."),
		N_("Nolphin wird verteilt in der Hoffnung dass es Brauchbar sein möge, aber OHNE JEGLICHE GEWÄHRLEISTUNG; selbst ohne die ausdrückliche Gewährleistung der MARKTFÄHIGKEIT oder EIGNUNG FÜR EINEN BESTIMMTEN ZWECK. Siehe die GNU General Public License für Einzelheiten."),
		N_("Mit Nolphin sollten Sie eine Kopie der GNU General Public License erhalen haben; falls nicht, schreiben Sie an die  Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA")
	};
	gchar *license_trans;
	GDateTime *date;

	license_trans = g_strjoin ("\n\n", _(license[0]), _(license[1]),
					     _(license[2]), NULL);

	date = g_date_time_new_now_local ();

	gtk_show_about_dialog (GTK_WINDOW (user_data),
			       "program-name", _("Nolphin"),
			       "version", VERSION,
			       "comments", _("Mit Nolphin können Ordner und Dateien verwaltet werden, sowohl auf Ihrem Rechner als auch im Internet."),
			       "license", license_trans,
			       "wrap-license", TRUE,
			      "logo-icon-name", "folder",
			      NULL);

	g_free (license_trans);
	g_date_time_unref (date);
}

static void
action_up_callback (GtkAction *action,
		     gpointer user_data)
{
	NolphinWindow *window = user_data;
	NolphinWindowSlot *slot;

	slot = nolphin_window_get_active_slot (window);
	nolphin_window_slot_go_up (slot, nolphin_event_get_window_open_flags ());
}

static void
action_nolphin_manual_callback (GtkAction *action,
				 gpointer user_data)
{
	NolphinWindow *window;
	GError *error;
	GtkWidget *dialog;
	const char* helpuri;
	const char* name = gtk_action_get_name (action);

	error = NULL;
	window = NOLPHIN_WINDOW (user_data);

	if (g_str_equal (name, "NolphinHelpSearch")) {
		helpuri = "help:gnome-help/files-search";
	} else if (g_str_equal (name,"NolphinHelpSort")) {
		helpuri = "help:gnome-help/files-sort";
	} else if (g_str_equal (name, "NolphinHelpLost")) {
		helpuri = "help:gnome-help/files-lost";
	} else if (g_str_equal (name, "NolphinHelpShare")) {
		helpuri = "help:gnome-help/files-share";
	} else {
		helpuri = "help:gnome-help/files";
	}

	if (NOLPHIN_IS_DESKTOP_WINDOW (window)) {
		nolphin_launch_application_from_command (gtk_window_get_screen (GTK_WINDOW (window)), "gnome-help", FALSE, NULL);
	} else {
		gtk_show_uri (gtk_window_get_screen (GTK_WINDOW (window)),
			      helpuri,
			      gtk_get_current_event_time (), &error);
	}

	if (error) {
		dialog = gtk_message_dialog_new (GTK_WINDOW (window),
						 GTK_DIALOG_MODAL,
						 GTK_MESSAGE_ERROR,
						 GTK_BUTTONS_OK,
						 _("Beim Anzeigen der Hilfe ist ein Fehler aufgetreten: \n%s"),
						 error->message);
		g_signal_connect (G_OBJECT (dialog), "response",
				  G_CALLBACK (gtk_widget_destroy),
				  NULL);

		gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
		gtk_widget_show (dialog);
		g_error_free (error);
	}
}

static void
action_show_shortcuts_window (GtkAction *action,
                              gpointer user_data)
{
    NolphinWindow *window;
    static GtkWidget *shortcuts_window;

    window = NOLPHIN_WINDOW (user_data);

    if (shortcuts_window == NULL)
    {
        GtkBuilder *builder;

        builder = gtk_builder_new_from_resource ("/org/nolphin/nolphin-shortcuts.ui");
        shortcuts_window = GTK_WIDGET (gtk_builder_get_object (builder, "keyboard_shortcuts"));

        gtk_window_set_position (GTK_WINDOW (shortcuts_window), GTK_WIN_POS_CENTER);

        g_signal_connect (shortcuts_window, "destroy",
                          G_CALLBACK (gtk_widget_destroyed), &shortcuts_window);

        g_object_unref (builder);
    }

    if (GTK_WINDOW (window) != gtk_window_get_transient_for (GTK_WINDOW (shortcuts_window)))
    {
        gtk_window_set_transient_for (GTK_WINDOW (shortcuts_window), GTK_WINDOW (window));
    }

    gtk_widget_show_all (shortcuts_window);
    gtk_window_present (GTK_WINDOW (shortcuts_window));
}

static void
menu_item_select_cb (GtkMenuItem *proxy,
		     NolphinWindow *window)
{
	GtkAction *action;
	char *message;

	action = gtk_activatable_get_related_action (GTK_ACTIVATABLE (proxy));
	g_return_if_fail (action != NULL);

	g_object_get (G_OBJECT (action), "tooltip", &message, NULL);
	if (message) {
		gtk_statusbar_push (GTK_STATUSBAR (window->details->statusbar),
				    window->details->help_message_cid, message);
		g_free (message);
	}
}

static void
menu_item_deselect_cb (GtkMenuItem *proxy,
		       NolphinWindow *window)
{
	gtk_statusbar_pop (GTK_STATUSBAR (window->details->statusbar),
			   window->details->help_message_cid);
}

static void
disconnect_proxy_cb (GtkUIManager *manager,
		     GtkAction *action,
		     GtkWidget *proxy,
		     NolphinWindow *window)
{
	if (GTK_IS_MENU_ITEM (proxy)) {
		g_signal_handlers_disconnect_by_func
			(proxy, G_CALLBACK (menu_item_select_cb), window);
		g_signal_handlers_disconnect_by_func
			(proxy, G_CALLBACK (menu_item_deselect_cb), window);
	}
}

static void
trash_state_changed_cb (NolphinTrashMonitor *monitor,
			gboolean state,
			NolphinWindow *window)
{
	GtkActionGroup *action_group;
	GtkAction *action;
    gchar *icon_name;

	action_group = nolphin_window_get_main_action_group (window);
	action = gtk_action_group_get_action (action_group, "Go to Trash");

    icon_name = nolphin_trash_monitor_get_symbolic_icon_name ();

    if (icon_name) {
        g_object_set (action, "icon-name", icon_name, NULL);
        g_clear_pointer (&icon_name, g_free);
    }
}

static void
nolphin_window_initialize_trash_icon_monitor (NolphinWindow *window)
{
	NolphinTrashMonitor *monitor;

	monitor = nolphin_trash_monitor_get ();

	trash_state_changed_cb (monitor, TRUE, window);

	g_signal_connect (monitor, "trash_state_changed",
			  G_CALLBACK (trash_state_changed_cb), window);
}

#define MENU_ITEM_MAX_WIDTH_CHARS 32

static void
action_close_all_windows_callback (GtkAction *action,
				   gpointer user_data)
{
	if (!NOLPHIN_IS_DESKTOP_WINDOW (user_data)) {
		nolphin_application_close_all_windows (nolphin_application_get_singleton ());
	}
}

static void
action_back_callback (GtkAction *action,
		      gpointer user_data)
{
	nolphin_window_back_or_forward (NOLPHIN_WINDOW (user_data),
					 TRUE, 0, nolphin_event_get_window_open_flags ());
}

static void
action_forward_callback (GtkAction *action,
			 gpointer user_data)
{
	nolphin_window_back_or_forward (NOLPHIN_WINDOW (user_data),
					 FALSE, 0, nolphin_event_get_window_open_flags ());
}

static void
action_split_view_switch_next_pane_callback(GtkAction *action,
					    gpointer user_data)
{
	nolphin_window_pane_grab_focus (nolphin_window_get_next_pane (NOLPHIN_WINDOW (user_data)));
}

static void
action_split_view_same_location_callback (GtkAction *action,
					  gpointer user_data)
{
	NolphinWindow *window;
	NolphinWindowPane *next_pane;
	GFile *location;

	window = NOLPHIN_WINDOW (user_data);
	next_pane = nolphin_window_get_next_pane (window);

	if (!next_pane) {
		return;
	}
	location = nolphin_window_slot_get_location (next_pane->active_slot);
	if (location) {
		nolphin_window_slot_open_location (nolphin_window_get_active_slot (window),
						    location, 0);
		g_object_unref (location);
	}
}

static void
action_show_hide_sidebar_callback (GtkAction *action,
				   gpointer user_data)
{
	NolphinWindow *window;

	if (!NOLPHIN_IS_DESKTOP_WINDOW (user_data)) {
		window = NOLPHIN_WINDOW (user_data);

		if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action))) {
			nolphin_window_show_sidebar (window);
		} else {
			nolphin_window_hide_sidebar (window);
		}
	}
}

static void
nolphin_window_update_split_view_actions_sensitivity (NolphinWindow *window)
{
	GtkActionGroup *action_group;
	GtkAction *action;
	gboolean have_multiple_panes;
	gboolean next_pane_is_in_same_location;
	GFile *active_pane_location;
	GFile *next_pane_location;
	NolphinWindowPane *next_pane;
	NolphinWindowSlot *active_slot;

	active_slot = nolphin_window_get_active_slot (window);
	action_group = nolphin_window_get_main_action_group (window);

	/* collect information */
	have_multiple_panes = nolphin_window_split_view_showing (window);
	if (active_slot != NULL) {
		active_pane_location = nolphin_window_slot_get_location (active_slot);
	} else {
		active_pane_location = NULL;
	}

	next_pane = nolphin_window_get_next_pane (window);
	if (next_pane && next_pane->active_slot) {
		next_pane_location = nolphin_window_slot_get_location (next_pane->active_slot);
		next_pane_is_in_same_location = (active_pane_location && next_pane_location &&
						 g_file_equal (active_pane_location, next_pane_location));
	} else {
		next_pane_location = NULL;
		next_pane_is_in_same_location = FALSE;
	}

	/* switch to next pane */
	action = gtk_action_group_get_action (action_group, "SplitViewNextPane");
	gtk_action_set_sensitive (action, have_multiple_panes);

	/* same location */
	action = gtk_action_group_get_action (action_group, "SplitViewSameLocation");
	gtk_action_set_sensitive (action, have_multiple_panes && !next_pane_is_in_same_location);

	/* clean up */
	g_clear_object (&active_pane_location);
	g_clear_object (&next_pane_location);
}

static void
action_split_view_callback (GtkAction *action,
			    gpointer user_data)
{
	NolphinWindow *window;
	gboolean is_active;

    if (NOLPHIN_IS_DESKTOP_WINDOW (user_data)) {
        return;
    }

	window = NOLPHIN_WINDOW (user_data);

	is_active = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));
	if (is_active != nolphin_window_split_view_showing (window)) {
		NolphinWindowSlot *slot;

		if (is_active) {
			/* F3 always means side-by-side ("vertikale Teilung"),
			 * regardless of what orientation a previous Shift+F3
			 * left the paned in. */
			gtk_orientable_set_orientation (GTK_ORIENTABLE (window->details->split_view_hpane),
							GTK_ORIENTATION_HORIZONTAL);
			nolphin_window_split_view_on (window);
		} else {
			nolphin_window_split_view_off (window);
		}

		slot = nolphin_window_get_active_slot (window);
		if (slot != NULL) {
			nolphin_view_update_menus (slot->content_view);
		}
	}

    nolphin_window_update_show_hide_ui_elements (window);
}

static void
action_show_hide_terminal_callback (GtkAction *action,
				    gpointer   user_data)
{
	NolphinWindow *window;
	gboolean is_active;

	if (NOLPHIN_IS_DESKTOP_WINDOW (user_data)) {
		return;
	}

	window = NOLPHIN_WINDOW (user_data);

	is_active = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));
	if (is_active != nolphin_window_terminal_showing (window)) {
		nolphin_window_set_show_terminal (window, is_active);
	}
}

static void
action_show_hide_preview_callback (GtkAction *action,
				   gpointer   user_data)
{
	NolphinWindow *window;
	gboolean is_active;

	if (NOLPHIN_IS_DESKTOP_WINDOW (user_data)) {
		return;
	}

	window = NOLPHIN_WINDOW (user_data);

	is_active = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));
	if (is_active != nolphin_window_preview_showing (window)) {
		nolphin_window_set_show_preview (window, is_active);
	}
}

static void
action_split_view_horizontal_callback (GtkAction *action,
				       gpointer   user_data)
{
	if (NOLPHIN_IS_DESKTOP_WINDOW (user_data)) {
		return;
	}

	nolphin_window_split_view_toggle_horizontal (NOLPHIN_WINDOW (user_data));
}

static void
sidebar_radio_entry_changed_cb (GtkAction *action,
                GtkRadioAction *current,
                gpointer user_data)
{
    gint current_value;
    NolphinWindow *window = NOLPHIN_WINDOW (user_data);

    current_value = gtk_radio_action_get_current_value (current);

    switch (current_value) {
        case SIDEBAR_PLACES:
            nolphin_window_set_sidebar_id (window, NOLPHIN_WINDOW_SIDEBAR_PLACES);
            break;
        case SIDEBAR_TREE:
            nolphin_window_set_sidebar_id (window, NOLPHIN_WINDOW_SIDEBAR_TREE);
            break;
        default:
            ;
            break;
    }
}

static void
view_radio_entry_changed_cb (GtkAction *action,
                             GtkRadioAction *current,
                             gpointer user_data)
{
    gint current_value;
    NolphinWindow *window = NOLPHIN_WINDOW (user_data);

    if (NOLPHIN_IS_DESKTOP_WINDOW (window)) {
        return;
    }

    current_value = gtk_radio_action_get_current_value (current);

    switch (current_value) {
        case ICON_VIEW:
            set_content_view_type (window, NOLPHIN_ICON_VIEW_ID);
            break;
        case LIST_VIEW:
            set_content_view_type (window, NOLPHIN_LIST_VIEW_ID);
            break;
        case COMPACT_VIEW:
            set_content_view_type (window, FM_COMPACT_VIEW_ID);
            break;
        case GALLERY_VIEW:
            set_content_view_type (window, NOLPHIN_GALLERY_VIEW_ID);
            break;
        default:
            ;
            break;
    }
}

static void
toolbar_radio_entry_changed_cb (GtkAction *action,
                                GtkRadioAction *current,
                                gpointer user_data)
{
    NolphinWindow *window = NOLPHIN_WINDOW (user_data);
    NolphinWindowPane *pane;
    GtkAction *toggle_action;
    gint current_value;

    if (NOLPHIN_IS_DESKTOP_WINDOW (window)) {
        return;
    }

    pane = nolphin_window_get_active_pane (window);
    toggle_action = gtk_action_group_get_action (pane->action_group, NOLPHIN_ACTION_TOGGLE_LOCATION);

    current_value = gtk_radio_action_get_current_value (current);
    switch (current_value) {
        case TOOLBAR_PATHBAR:
            g_settings_set_boolean (nolphin_preferences, NOLPHIN_PREFERENCES_SHOW_LOCATION_ENTRY, FALSE);
            gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (toggle_action), FALSE);
            break;
        case TOOLBAR_ENTRY:
            g_settings_set_boolean (nolphin_preferences, NOLPHIN_PREFERENCES_SHOW_LOCATION_ENTRY, TRUE);
            gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (toggle_action), TRUE);
            break;
        default:
            ;
            break;
    }
}

/* TODO: bind all of this with g_settings_bind and GBinding */
static guint
sidebar_id_to_value (const gchar *sidebar_id)
{
	guint retval = SIDEBAR_PLACES;

	if (g_strcmp0 (sidebar_id, NOLPHIN_WINDOW_SIDEBAR_TREE) == 0)
		retval = SIDEBAR_TREE;

	return retval;
}

static void
update_side_bar_radio_buttons (NolphinWindow *window)
{
    GtkActionGroup *action_group;
    GtkAction *action;
    guint current_value;

    action_group = nolphin_window_get_main_action_group (window);

    action = gtk_action_group_get_action (action_group,
                          "Sidebar Places");
    current_value = sidebar_id_to_value (nolphin_window_get_sidebar_id (window));

    g_signal_handlers_block_by_func (action, sidebar_radio_entry_changed_cb, window);
    gtk_radio_action_set_current_value (GTK_RADIO_ACTION (action), current_value);
    g_signal_handlers_unblock_by_func (action, sidebar_radio_entry_changed_cb, window);
}

void
nolphin_window_update_show_hide_ui_elements (NolphinWindow *window)
{
    NolphinWindowPane *pane;
	GtkActionGroup *action_group;
	GtkAction *action;

	action_group = nolphin_window_get_main_action_group (window);

	action = gtk_action_group_get_action (action_group,
					      NOLPHIN_ACTION_SHOW_HIDE_EXTRA_PANE);
    gtk_action_block_activate (action);
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action),
				      nolphin_window_split_view_showing (window));
    gtk_action_unblock_activate (action);

	action = gtk_action_group_get_action (action_group,
					      NOLPHIN_ACTION_SHOW_HIDE_TERMINAL);
    gtk_action_block_activate (action);
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action),
				      nolphin_window_terminal_showing (window));
    gtk_action_unblock_activate (action);

	action = gtk_action_group_get_action (action_group,
					      NOLPHIN_ACTION_SHOW_HIDE_PREVIEW);
    gtk_action_block_activate (action);
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action),
				      nolphin_window_preview_showing (window));
    gtk_action_unblock_activate (action);

	nolphin_window_update_split_view_actions_sensitivity (window);

    update_side_bar_radio_buttons (window);

    pane = nolphin_window_get_active_pane (window);
    if (pane != NULL) {
        action_group = nolphin_window_pane_get_toolbar_action_group (pane);

        action = gtk_action_group_get_action (action_group,
                                              NOLPHIN_ACTION_SHOW_HIDE_EXTRA_PANE);
        gtk_action_block_activate (action);
        gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action),
                                      nolphin_window_split_view_showing (window));
        gtk_action_unblock_activate (action);
    }
}

static void
action_add_bookmark_callback (GtkAction *action,
			      gpointer user_data)
{
    if (!NOLPHIN_IS_DESKTOP_WINDOW (user_data)) {
        nolphin_window_add_bookmark_for_current_location (NOLPHIN_WINDOW (user_data));
    }
}

static void
action_edit_bookmarks_callback (GtkAction *action,
				gpointer user_data)
{
    if (!NOLPHIN_IS_DESKTOP_WINDOW (user_data)) {
        nolphin_window_edit_bookmarks (NOLPHIN_WINDOW (user_data));
    }
}

static void
connect_proxy_cb (GtkActionGroup *action_group,
                  GtkAction *action,
                  GtkWidget *proxy,
                  NolphinWindow *window)
{
    GtkWidget *label;

	if (!GTK_IS_MENU_ITEM (proxy))
		return;

    label = gtk_bin_get_child (GTK_BIN (proxy));

    if (GTK_IS_LABEL (label)) {
       gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
       gtk_label_set_max_width_chars (GTK_LABEL (label), MENU_ITEM_MAX_WIDTH_CHARS);
    }

	g_signal_connect (proxy, "select",
			  G_CALLBACK (menu_item_select_cb), window);
	g_signal_connect (proxy, "deselect",
			  G_CALLBACK (menu_item_deselect_cb), window);
}

static void
action_new_window_callback (GtkAction *action,
                            gpointer user_data)
{
    NolphinWindow *current_window;

    current_window = NOLPHIN_WINDOW (user_data);

    if (NOLPHIN_IS_DESKTOP_WINDOW (current_window)) {
        NolphinFile *file;
        NolphinView *view;
        gchar *desktop_uri;

        desktop_uri = nolphin_get_desktop_directory_uri ();

        file = nolphin_file_get_existing_by_uri (desktop_uri);

        view = nolphin_window_slot_get_current_view (nolphin_window_get_active_slot (current_window));
        nolphin_view_activate_file (view, file, 0);

        g_free (desktop_uri);
        nolphin_file_unref (file);
    } else {
        NolphinApplication *application;
        NolphinWindow *new_window;
        gchar *uri;
        GFile *loc;

        uri = nolphin_window_slot_get_current_uri (nolphin_window_get_active_slot (current_window));

        if (eel_uri_is_search (uri)) {
            NolphinDirectory *dir;
            NolphinQuery *query;

            dir = nolphin_directory_get_by_uri (uri);
            query = nolphin_search_directory_get_query (NOLPHIN_SEARCH_DIRECTORY (dir));

            if (query != NULL) {
                g_free (uri);

                uri = nolphin_query_get_location (query);
                g_object_unref (query);
            }

            nolphin_directory_unref (dir);
        }

        loc = g_file_new_for_uri (uri);

        application = nolphin_application_get_singleton ();

        new_window = nolphin_application_create_window (application,
                                                     gtk_window_get_screen (GTK_WINDOW (current_window)));

        nolphin_window_slot_open_location (nolphin_window_get_active_slot (new_window), loc, 0);

        g_object_unref (loc);
        g_free (uri);
    }
}

static void
action_clear_history_callback (GtkAction *action,
			       gpointer user_data)
{
	nolphin_location_stats_clear (nolphin_location_stats_get_default ());
}

static void
action_new_tab_callback (GtkAction *action,
			 gpointer user_data)
{
	NolphinWindow *window;

	window = NOLPHIN_WINDOW (user_data);
	nolphin_window_new_tab (window);
}

void action_toggle_location_entry_callback (GtkToggleAction *action, gpointer user_data);

static void
toggle_location_entry (NolphinWindow     *window,
                       NolphinWindowPane *pane,
                       gboolean        from_accel_or_menu)
{
    gboolean current_view, temp_toolbar_visible, default_toolbar_visible, grab_focus_only, already_has_focus;
    GtkToggleAction *button_action;
    GtkActionGroup *action_group;

    current_view = nolphin_toolbar_get_show_location_entry (NOLPHIN_TOOLBAR (pane->tool_bar));
    temp_toolbar_visible = pane->temporary_navigation_bar;
    default_toolbar_visible = g_settings_get_boolean (nolphin_window_state,
                                                      NOLPHIN_WINDOW_STATE_START_WITH_TOOLBAR);
    already_has_focus = nolphin_location_bar_has_focus (NOLPHIN_LOCATION_BAR (pane->location_bar));

    grab_focus_only = from_accel_or_menu && (pane->last_focus_widget == NULL || !already_has_focus) && current_view;

    if ((temp_toolbar_visible || default_toolbar_visible) && !grab_focus_only) {
        nolphin_toolbar_set_show_location_entry (NOLPHIN_TOOLBAR (pane->tool_bar), !current_view);

        action_group = pane->toolbar_action_group;
        button_action = GTK_TOGGLE_ACTION (gtk_action_group_get_action (action_group, NOLPHIN_ACTION_TOGGLE_LOCATION));

        g_signal_handlers_block_by_func (button_action, action_toggle_location_entry_callback, window);
        gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (button_action), !current_view);
        g_signal_handlers_unblock_by_func (button_action, action_toggle_location_entry_callback, window);
    } else {
        nolphin_window_pane_ensure_location_bar (pane);
    }
}

void
action_toggle_location_entry_callback (GtkToggleAction *action,
                                        gpointer user_data)
{
    NolphinWindow *window = user_data;
    NolphinWindowPane *pane;

    pane = nolphin_window_get_active_pane (window);
    toggle_location_entry (window, pane, FALSE);
}

void nolphin_window_show_location_entry (NolphinWindow *window) {
	NolphinWindowPane *pane;

    pane = nolphin_window_get_active_pane (window);
    toggle_location_entry (window, pane, TRUE);
}

static void
action_menu_edit_location_callback (GtkAction *action,
				gpointer user_data)
{
	NolphinWindow *window = user_data;
	NolphinWindowPane *pane;

    if (!NOLPHIN_IS_DESKTOP_WINDOW (user_data)) {
        pane = nolphin_window_get_active_pane (window);
        toggle_location_entry (window, pane, TRUE);
    }
}

static void
action_show_thumbnails_callback (GtkAction * action,
                                 gpointer user_data)
{
    NolphinWindowSlot *slot;
    NolphinWindowPane *pane;
    NolphinWindow *window;
    gboolean value;

    window = NOLPHIN_WINDOW (user_data);

    slot = nolphin_window_get_active_slot (window);
    pane = nolphin_window_get_active_pane(window);

    value = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));
    nolphin_window_slot_set_show_thumbnails(slot, value);

    toolbar_set_show_thumbnails_button (value, pane);
    menu_set_show_thumbnails_action(value, window);
}

static void
set_content_view_type(NolphinWindow *window,
                      const gchar *view_id)
{
    NolphinWindowSlot *slot;

    slot = nolphin_window_get_active_slot (window);
    nolphin_window_slot_set_content_view (slot, view_id);
}

static void
action_icon_view_callback (GtkAction *action,
                           gpointer user_data)
{
    NolphinWindow *window;

    window = NOLPHIN_WINDOW (user_data);

    set_content_view_type (window, NOLPHIN_ICON_VIEW_ID);
}


static void
action_list_view_callback (GtkAction *action,
                           gpointer user_data)
{
    NolphinWindow *window;

    window = NOLPHIN_WINDOW (user_data);

    set_content_view_type (window, NOLPHIN_LIST_VIEW_ID);
}


static void
action_compact_view_callback (GtkAction *action,
                           gpointer user_data)
{
    NolphinWindow *window;

    window = NOLPHIN_WINDOW (user_data);

    set_content_view_type (window, FM_COMPACT_VIEW_ID);
}

guint
action_for_view_id (const char *view_id)
{
    if (g_strcmp0(view_id, NOLPHIN_ICON_VIEW_ID) == 0) {
        return ICON_VIEW;
    } else if (g_strcmp0(view_id, NOLPHIN_LIST_VIEW_ID) == 0) {
        return LIST_VIEW;
    } else if (g_strcmp0(view_id, FM_COMPACT_VIEW_ID) == 0) {
        return COMPACT_VIEW;
    } else if (g_strcmp0(view_id, NOLPHIN_GALLERY_VIEW_ID) == 0) {
        return GALLERY_VIEW;
    } else {
        return NULL_VIEW;
    }
}

void
toolbar_set_view_button (guint action_id, NolphinWindow *window)
{
    GtkAction *action, *action1, *action2;
    GtkActionGroup *action_group;
    if (action_id == NULL_VIEW) {
        return;
    }
    action_group = nolphin_window_pane_get_toolbar_action_group (nolphin_window_get_active_pane (window));

    action = gtk_action_group_get_action(action_group,
                                         NOLPHIN_ACTION_ICON_VIEW);
    action1 = gtk_action_group_get_action(action_group,
                                         NOLPHIN_ACTION_LIST_VIEW);
    action2 = gtk_action_group_get_action(action_group,
                                         NOLPHIN_ACTION_COMPACT_VIEW);

    g_signal_handlers_block_matched (action,
                         G_SIGNAL_MATCH_FUNC,
                         0, 0,
                         NULL,
                         action_icon_view_callback,
                         window);

    g_signal_handlers_block_matched (action1,
                         G_SIGNAL_MATCH_FUNC,
                         0, 0,
                         NULL,
                         action_list_view_callback,
                         window);
    g_signal_handlers_block_matched (action2,
                         G_SIGNAL_MATCH_FUNC,
                         0, 0,
                         NULL,
                         action_compact_view_callback,
                         window);

    if (action_id != ICON_VIEW) {
        gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(action), FALSE);
    } else {
        gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(action), TRUE);
    }

    if (action_id != LIST_VIEW) {
        gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(action1), FALSE);
    } else {
        gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(action1), TRUE);
    }

    if (action_id != COMPACT_VIEW) {
        gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(action2), FALSE);
    } else {
        gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(action2), TRUE);
    }

    g_signal_handlers_unblock_matched (action,
                           G_SIGNAL_MATCH_FUNC,
                           0, 0,
                           NULL,
                           action_icon_view_callback,
                           window);


    g_signal_handlers_unblock_matched (action1,
                           G_SIGNAL_MATCH_FUNC,
                           0, 0,
                           NULL,
                           action_list_view_callback,
                           window);


    g_signal_handlers_unblock_matched (action2,
                           G_SIGNAL_MATCH_FUNC,
                           0, 0,
                           NULL,
                           action_compact_view_callback,
                           window);

}

void
toolbar_set_show_thumbnails_button (gboolean value, NolphinWindowPane *pane)
{
    GtkAction *action;
    GtkActionGroup *action_group;

    action_group = nolphin_window_pane_get_toolbar_action_group (pane);


    action = gtk_action_group_get_action(action_group,
                                         NOLPHIN_ACTION_SHOW_THUMBNAILS);

    g_signal_handlers_block_matched (action,
                         G_SIGNAL_MATCH_FUNC,
                         0, 0,
                         NULL,
                         action_show_thumbnails_callback,
                         NULL);

    gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(action), value);

    g_signal_handlers_unblock_matched (action,
                           G_SIGNAL_MATCH_FUNC,
                           0, 0,
                           NULL,
                           action_show_thumbnails_callback,
                           NULL);
}

void
menu_set_show_thumbnails_action (gboolean value, NolphinWindow *window)
{
    GtkAction *action;

    action = gtk_action_group_get_action (window->details->main_action_group,
                                          NOLPHIN_ACTION_SHOW_THUMBNAILS);

    g_signal_handlers_block_matched (action,
                         G_SIGNAL_MATCH_FUNC,
                         0, 0,
                         NULL,
                         action_show_thumbnails_callback,
                         NULL);

    gtk_toggle_action_set_active(GTK_TOGGLE_ACTION(action), value);

    g_signal_handlers_unblock_matched (action,
                           G_SIGNAL_MATCH_FUNC,
                           0, 0,
                           NULL,
                           action_show_thumbnails_callback,
                           NULL);
}

void
toolbar_set_create_folder_button (gboolean value, NolphinWindowPane *pane)
{
    GtkActionGroup *action_group;
    GtkAction *action;

    action_group = nolphin_window_pane_get_toolbar_action_group (pane);

    action = gtk_action_group_get_action(action_group,
                                         NOLPHIN_ACTION_NEW_FOLDER);

    gtk_action_set_sensitive (action, value);
}

void
menu_set_view_selection (guint action_id,
                         NolphinWindow *window)
{
    GtkAction *action;

    if (action_id == NULL_VIEW) {
        return;
    }

    g_signal_handlers_block_by_func (window->details->main_action_group,
                                     view_radio_entry_changed_cb,
                                     window);

    action = gtk_action_group_get_action (window->details->main_action_group,
                                          NOLPHIN_ACTION_ICON_VIEW);

    gtk_radio_action_set_current_value (GTK_RADIO_ACTION (action), action_id);

    g_signal_handlers_unblock_by_func (window->details->main_action_group,
                                       view_radio_entry_changed_cb,
                                       window);
}

static void
action_tabs_previous_callback (GtkAction *action,
			       gpointer user_data)
{
	NolphinWindowPane *pane;
	NolphinWindow *window = user_data;

	pane = nolphin_window_get_active_pane (window);
	nolphin_notebook_set_current_page_relative (NOLPHIN_NOTEBOOK (pane->notebook), -1);
}

static void
action_tabs_next_callback (GtkAction *action,
			   gpointer user_data)
{
	NolphinWindowPane *pane;
	NolphinWindow *window = user_data;

	pane = nolphin_window_get_active_pane (window);
	nolphin_notebook_set_current_page_relative (NOLPHIN_NOTEBOOK (pane->notebook), 1);
}

static void
reorder_tab (NolphinWindowPane *pane, int offset)
{
	int page_num;

	g_return_if_fail (pane != NULL);

	page_num = gtk_notebook_get_current_page (
		GTK_NOTEBOOK (pane->notebook));
	g_return_if_fail (page_num != -1);
	nolphin_notebook_reorder_child_relative (
		NOLPHIN_NOTEBOOK (pane->notebook), page_num, offset);
}

static void
action_tabs_move_left_callback (GtkAction *action,
				gpointer user_data)
{
	NolphinWindow *window = user_data;
	reorder_tab (nolphin_window_get_active_pane (window), -1);
}

static void
action_tabs_move_right_callback (GtkAction *action,
				 gpointer user_data)
{
	NolphinWindow *window = user_data;
	reorder_tab (nolphin_window_get_active_pane (window), 1);
}

static void
action_tab_change_action_activate_callback (GtkAction *action,
					    gpointer user_data)
{
	NolphinWindowPane *pane;
	NolphinWindow *window = user_data;
	GtkNotebook *notebook;
	int num;

	pane = nolphin_window_get_active_pane (window);
	notebook = GTK_NOTEBOOK (pane->notebook);

	num = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (action), "num"));
	if (num < gtk_notebook_get_n_pages (notebook)) {
		gtk_notebook_set_current_page (notebook, num);
	}
}

static void
action_new_folder_callback (GtkAction *action,
                            gpointer user_data)
{
    g_assert (NOLPHIN_IS_WINDOW (user_data));
    NolphinWindow *window = user_data;
    NolphinView *view = get_current_view (window);

    nolphin_view_new_folder (view);
}

static void
open_in_terminal_other (const gchar *path)
{
    gchar *gsetting_terminal;
    gchar **token;
    gchar **argv;
    gint i;

    gsetting_terminal = g_settings_get_string (gnome_terminal_preferences,
                                               GNOME_DESKTOP_TERMINAL_EXEC);

    token = g_strsplit (gsetting_terminal, " ", 0);
    argv = g_new (gchar *, g_strv_length (token) + 1);
    for (i = 0; token[i] != NULL; i++) {
        argv[i] = token[i];
    }
    argv[i] = NULL;

    g_spawn_async (path, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL);

    g_free (gsetting_terminal);
    g_strfreev (token);
    g_free (argv);
}


static void
action_open_terminal_callback(GtkAction *action, gpointer callback_data)
{
    NolphinWindow *window;
    NolphinView *view;

    window = NOLPHIN_WINDOW(callback_data);

    view = get_current_view (window);

    gchar *path;
    gchar *uri = nolphin_view_get_backing_uri (view);
    GFile *gfile = g_file_new_for_uri (uri);
    path = g_file_get_path (gfile);
    open_in_terminal_other (path);
    g_free (uri);
    g_free (path);
    g_object_unref (gfile);
}

#define NOLPHIN_VIEW_MENUBAR_FILE_PATH                  "/MenuBar/File"

static void
on_file_menu_show (GtkWidget *widget, gpointer user_data)
{
    NolphinWindow *window;
    NolphinView *view;

    window = NOLPHIN_WINDOW (user_data);
    view = get_current_view (window);

    nolphin_view_update_actions_and_extensions (view);
}

/* Das .deb-Paket-Formular lebt jetzt dauerhaft als Reiter in der rechten
 * Arbeitsbereich-Leiste (nolphin-workspace-panel.c) statt als eigenes
 * Dialogfenster - dieser Menüeintrag (Hilfe ▸ .deb-Paket erstellen …)
 * zeigt die Leiste nur noch an und wechselt zu diesem Reiter, statt die
 * Eingabemaske ein zweites Mal zu bauen. */
static void
action_build_deb_callback (GtkAction *action, gpointer user_data)
{
	NolphinWindow *window = NOLPHIN_WINDOW (user_data);

	nolphin_workspace_panel_show_deb_builder (nolphin_window_get_workspace_panel (window), window);
}

static const GtkActionEntry main_entries[] = {
  /* name, stock id, label */  { "File", NULL, N_("_Datei") },
  /* name, stock id, label */  { "Edit", NULL, N_("_Bearbeiten") },
  /* name, stock id, label */  { "View", NULL, N_("_Ansicht") },
  /* name, stock id, label */  { "Help", NULL, N_("_Hilfe") },
  /* name, stock id */         { "Close", "xsi-window-close-symbolic",
  /* label, accelerator */       N_("_Beenden"), "<control>W",
  /* tooltip */                  N_("Diesen Ordner schließen"),
                                 G_CALLBACK (action_close_window_slot_callback) },
                               { "Preferences", "xsi-toolbox-symbolic",
                                 N_("_Einstellungen"),
                                 NULL, N_("Nolphin-Einstellungen bearbeiten"),
                                 G_CALLBACK (action_preferences_callback) },
  /* name, stock id */         { NOLPHIN_ACTION_SPLIT_VIEW_HORIZONTAL, NULL,
  /* label, accelerator */       N_("Ansicht _horizontal teilen"), "<shift>F3",
  /* tooltip */                  N_("Eine zusätzliche Ordneransicht darunter öffnen (oder ein bereits geöffnetes zusätzliches Fenster neu ausrichten)"),
                                 G_CALLBACK (action_split_view_horizontal_callback) },
  /* name, stock id */         { NOLPHIN_ACTION_DUPLICATE_TAB, NULL,
  /* label, accelerator */       N_("Reiter _duplizieren"), NULL,
  /* tooltip */                  N_("Einen neuen Reiter am selben Ort wie diesen öffnen"),
                                 G_CALLBACK (action_duplicate_tab_callback) },
  /* name, stock id */         { NOLPHIN_ACTION_CLOSE_ALL_TABS, NULL,
  /* label, accelerator */       N_("_Alle Reiter schließen"), NULL,
  /* tooltip */                  N_("Jeden Reiter in diesem Fenster schließen"),
                                 G_CALLBACK (action_close_all_tabs_callback) },
  /* name, stock id, label */  { "WorkspacesMenu", NULL, N_("_Arbeitsbereiche") },
  /* name, stock id */         { "Workspace Save", NULL,
  /* label, accelerator */       N_("_Speichern …"), NULL,
  /* tooltip */                  N_("Den aktuellen Arbeitsbereich (Reiter, Teilung, Fenstergröße, Panels) unter einem Namen speichern"),
                                 G_CALLBACK (action_workspace_callback) },
  /* name, stock id */         { "Workspace Load", NULL,
  /* label, accelerator */       N_("_Laden …"), NULL,
  /* tooltip */                  N_("Einen gespeicherten Arbeitsbereich laden"),
                                 G_CALLBACK (action_workspace_callback) },
  /* name, stock id */         { "Workspace Duplicate", NULL,
  /* label, accelerator */       N_("_Duplizieren …"), NULL,
  /* tooltip */                  N_("Einen gespeicherten Arbeitsbereich duplizieren"),
                                 G_CALLBACK (action_workspace_callback) },
  /* name, stock id */         { "Workspace Manage", NULL,
  /* label, accelerator */       N_("_Verwalten …"), NULL,
  /* tooltip */                  N_("Gespeicherte Arbeitsbereiche laden, duplizieren und löschen"),
                                 G_CALLBACK (action_workspace_callback) },
  /* name, stock id, label */  { "SplitViewMenu", NULL, N_("_Geteilte Ansicht") },
  /* name, stock id */         { "Duplicate Pane", NULL,
  /* label, accelerator */       N_("Bereich _duplizieren"), NULL,
  /* tooltip */                  N_("Einen weiteren Bereich (bis zu vier) am Ort des aktiven Bereichs öffnen"),
                                 G_CALLBACK (action_duplicate_pane_callback) },
  /* name, stock id */         { "Maximize Pane", NULL,
  /* label, accelerator */       N_("Bereich _maximieren"), NULL,
  /* tooltip */                  N_("Den aktiven Bereich allein anzeigen oder die anderen wieder einblenden"),
                                 G_CALLBACK (action_maximize_pane_callback) },
  /* name, stock id */         { "Close Pane", NULL,
  /* label, accelerator */       N_("Bereich _schließen"), NULL,
  /* tooltip */                  N_("Den aktiven Bereich schließen"),
                                 G_CALLBACK (action_close_pane_callback) },
  /* name, stock id, label */  { "TabsMenu", NULL, N_("_Reiter") },
  /* name, stock id, label */  { "ClipboardMenu", NULL, N_("_Zwischenablage") },
  /* name, stock id */         { "Lock Tab", NULL,
  /* label, accelerator */       N_("Reiter _sperren"), NULL,
  /* tooltip */                  N_("Den Reiter gegen versehentliches Schließen sperren oder entsperren"),
                                 G_CALLBACK (action_lock_tab_callback) },
  /* name, stock id */         { "Rename Tab", NULL,
  /* label, accelerator */       N_("Reiter _umbenennen …"), NULL,
  /* tooltip */                  N_("Dem Reiter einen eigenen Namen geben"),
                                 G_CALLBACK (action_rename_tab_callback) },
  /* name, stock id */         { "Show Clipboard", NULL,
  /* label, accelerator */       N_("Inhalt _anzeigen"), NULL,
  /* tooltip */                  N_("Anzeigen, was sich in der Zwischenablage befindet"),
                                 G_CALLBACK (action_show_clipboard_callback) },
  /* name, stock id */         { "Clear Clipboard", NULL,
  /* label, accelerator */       N_("_Leeren"), NULL,
  /* tooltip */                  N_("Die Zwischenablage leeren"),
                                 G_CALLBACK (action_clear_clipboard_callback) },
  /* name, stock id */         { NOLPHIN_ACTION_RESTORE_CLOSED_TAB, NULL,
  /* label, accelerator */       N_("Geschlossenen _Reiter wiederherstellen"), "<control><shift>T",
  /* tooltip */                  N_("Den zuletzt geschlossenen Reiter wiederherstellen"),
                                 G_CALLBACK (action_restore_closed_tab_callback) },
#ifdef TEXT_CHANGE_UNDO
  /* name, stock id, label */  { "Undo", NULL, N_("_Rückgängig"),
                                 "<control>Z", N_("Die letzte Änderung am Text rückgängig machen"),
                                 G_CALLBACK (action_undo_callback) },
#endif
  /* name, stock id, label */  { "Up", "xsi-go-up-symbolic", N_("_Übergeordneten Ordner öffnen"),
                                 "<alt>Up", N_("Den übergeordneten Ordner öffnen"),
                                 G_CALLBACK (action_up_callback) },
  /* name, stock id, label */  { "UpAccel", NULL, "UpAccel",
                                 "", NULL,
                                 G_CALLBACK (action_up_callback) },
  /* name, stock id */         { "Stop", "xsi-process-stop-symbolic",
  /* label, accelerator */       N_("_Anhalten"), NULL,
  /* tooltip */                  N_("Das Laden des aktuellen Ortes anhalten"),
                                 G_CALLBACK (action_stop_callback) },
  /* name, stock id */         { "Reload", "xsi-view-refresh-symbolic",
  /* label, accelerator */       N_("_Aktualisieren"), "<control>R",
  /* tooltip */                  N_("Den aktuellen Ort aktualisieren"),
                                 G_CALLBACK (action_reload_callback) },
  /* name, stock id */         { "NolphinHelp", "xsi-help-contents-symbolic",
  /* label, accelerator */       N_("_Alle Themen"), "F1",
  /* tooltip */                  N_("Nolphin-Hilfethemen anzeigen"),
                                 G_CALLBACK (action_nolphin_manual_callback) },
                               { "NolphinShortcuts", "xsi-keyboard-shortcuts-symbolic",
                                 N_("_Tastenkombinationen"), "<control>F1",
                                 N_("Tastenkombinationen anzeigen"),
                                 G_CALLBACK (action_show_shortcuts_window) },
  /** name, stock id          { "NolphinHelpSearch", NULL,
     label, accelerator        N_("Search for files"), NULL,
     tooltip                   N_("Locate files based on file name and type. Save your searches for later use."),
                                 G_CALLBACK (action_nolphin_manual_callback) },
     name, stock id          { "NolphinHelpSort", NULL,
     label, accelerator        N_("Sort files and folders"), NULL,
     tooltip                   N_("Arrange files by name, size, type, or when they were changed."),
                                 G_CALLBACK (action_nolphin_manual_callback) },
     name, stock id          { "NolphinHelpLost", NULL,
     label, accelerator        N_("Find a lost file"), NULL,
     tooltip                   N_("Follow these tips if you can't find a file you created or downloaded."),
                                 G_CALLBACK (action_nolphin_manual_callback) },
     name, stock id          { "NolphinHelpShare", NULL,
     label, accelerator        N_("Share and transfer files"), NULL,
     tooltip                   N_("Easily transfer files to your contacts and devices from the file manager."),
                                 G_CALLBACK (action_nolphin_manual_callback) }, **/
  /* name, stock id */         { "Build Deb Package", "xsi-package-x-generic-symbolic",
  /* label, accelerator */       N_(".deb-_Paket erstellen …"), NULL,
  /* tooltip */                  N_("Aus ausgewählten Dateien ein installierbares .deb-Paket erstellen"),
                                 G_CALLBACK (action_build_deb_callback) },
  /* name, stock id */         { "About Nolphin", "xsi-help-about-symbolic",
  /* label, accelerator */       N_("_Über"), NULL,
  /* tooltip */                  N_("Danksagungen für die Urheber von Nolphin anzeigen"),
                                 G_CALLBACK (action_about_nolphin_callback) },
  /* name, stock id */         { "Zoom In", "xsi-zoom-in-symbolic",
  /* label, accelerator */       N_("Ver_größern"), "<control>plus",
  /* tooltip */                  N_("Ansicht vergrößern"),
                                 G_CALLBACK (action_zoom_in_callback) },
  /* name, stock id */         { "ZoomInAccel", NULL,
  /* label, accelerator */       "ZoomInAccel", "<control>equal",
  /* tooltip */                  NULL,
                                 G_CALLBACK (action_zoom_in_callback) },
  /* name, stock id */         { "ZoomInAccel2", NULL,
  /* label, accelerator */       "ZoomInAccel2", "<control>KP_Add",
  /* tooltip */                  NULL,
                                 G_CALLBACK (action_zoom_in_callback) },
  /* name, stock id */         { "Zoom Out", "xsi-zoom-out-symbolic",
  /* label, accelerator */       N_("Ver_kleinern"), "<control>minus",
  /* tooltip */                  N_("Ansicht verkleinern"),
                                 G_CALLBACK (action_zoom_out_callback) },
  /* name, stock id */         { "ZoomOutAccel", NULL,
  /* label, accelerator */       "ZoomOutAccel", "<control>KP_Subtract",
  /* tooltip */                  NULL,
                                 G_CALLBACK (action_zoom_out_callback) },
  /* name, stock id */         { "Zoom Normal", "xsi-zoom-original-symbolic",
  /* label, accelerator */       N_("_Normale Größe"), "<control>0",
  /* tooltip */                  N_("Die normale Ansichtsgröße verwenden"),
                                 G_CALLBACK (action_zoom_normal_callback) },
  /* name, stock id */         { "Connect to Server", NULL,
  /* label, accelerator */       N_("Mit _Server verbinden …"), NULL,
  /* tooltip */                  N_("Verbinden mit einem entfernten Rechner oder freigegebenen Datenträger"),
                                 G_CALLBACK (action_connect_to_server_callback) },
  /* name, stock id */         { "Home", NOLPHIN_ICON_SYMBOLIC_HOME,
  /* label, accelerator */       N_("_Persönlicher Ordner"), "<alt>Home",
  /* tooltip */                  N_("Persönlichen Ordner öffnen"),
                                 G_CALLBACK (action_home_callback) },
  /* name, stock id */         { "Go to Computer", NOLPHIN_ICON_SYMBOLIC_COMPUTER,
  /* label, accelerator */       N_("_Rechner"), NULL,
  /* tooltip */                  N_("Alle lokalen und entfernten Laufwerke und Ordner durchsuchen, die von diesem Rechner aus erreichbar sind"),
                                 G_CALLBACK (action_go_to_computer_callback) },
  /* name, stock id */         { "Go to Network", NOLPHIN_ICON_SYMBOLIC_NETWORK,
  /* label, accelerator */       N_("_Netzwerk"), NULL,
  /* tooltip */                  N_("Lokale und als Lesezeichen gespeicherte Orte durchsuchen"),
                                 G_CALLBACK (action_go_to_network_callback) },
  /* name, stock id */         { "Go to Templates", NOLPHIN_ICON_SYMBOLIC_FOLDER_TEMPLATES,
  /* label, accelerator */       N_("_Vorlagen"), NULL,
  /* tooltip */                  N_("Persönlichen Vorlagenordner öffnen"),
                                 G_CALLBACK (action_go_to_templates_callback) },
  /* name, stock id */         { "Go to Trash", NOLPHIN_ICON_SYMBOLIC_TRASH,
  /* label, accelerator */       N_("_Papierkorb"), NULL,
  /* tooltip */                  N_("Persönlichen Papierkorb öffnen"),
                                 G_CALLBACK (action_go_to_trash_callback) },
  /* name, stock id, label */  { "Go", NULL, N_("_Gehen zu") },
  /* name, stock id, label */  { "Bookmarks", NULL, N_("_Lesezeichen") },
  /* name, stock id, label */  { "HistoryMenu", NULL, N_("_Verlauf") },
  /* name, stock id, label */  { "FrequentMenu", NULL, N_("_Häufig verwendet") },
  /* name, stock id */         { "ClearHistory", NULL,
  /* label, accelerator */       N_("Verlauf _löschen"), NULL,
  /* tooltip */                  N_("Verlauf und Besuchszähler der Orte löschen"),
                                 G_CALLBACK (action_clear_history_callback) },
  /* name, stock id, label */  { "Tabs", NULL, N_("_Reiter") },
  /* name, stock id, label */  { "New Window", NULL, N_("Neues _Fenster"),
                                 "<control>N", N_("Ein neues Nolphin-Fenster für den angezeigten Ort öffnen"),
                                 G_CALLBACK (action_new_window_callback) },
  /* name, stock id, label */  { "New Tab", "xsi-tab-new-symbolic", N_("Neuer _Reiter"),
                                 "<control>T", N_("Einen weiteren Reiter für diesen Ort öffnen"),
                                 G_CALLBACK (action_new_tab_callback) },
  /* name, stock id, label */  { "Close All Windows", NULL, N_("Alle _Fenster schließen"),
                                 "<control>Q", N_("Alle Navigationsfenster schließen"),
                                 G_CALLBACK (action_close_all_windows_callback) },
  /* name, stock id, label */  { NOLPHIN_ACTION_BACK, "xsi-go-previous-symbolic", N_("_Zurück"),
				 "<alt>Left", N_("Zum vorher besuchten Ort gehen"),
				 G_CALLBACK (action_back_callback) },
  /* name, stock id, label */  { NOLPHIN_ACTION_FORWARD, "xsi-go-next-symbolic", N_("_Vorwärts"),
				 "<alt>Right", N_("Zum als nächstes besuchten Ort gehen"),
				 G_CALLBACK (action_forward_callback) },
  /* name, stock id, label */  { NOLPHIN_ACTION_EDIT_LOCATION, NULL, N_("_Pfadeingabe ein/aus"),
                                 "<control>L", N_("Zwischen Pfadeingabe und Verlaufsnavigation umschalten"),
                                 G_CALLBACK (action_menu_edit_location_callback) },
  /* name, stock id, label */  { "SplitViewNextPane", NULL, N_("Zur anderen Leiste _wechseln"),
				 "F6", N_("Den Fokus, in einem Fenster mit geteilter Ansicht, an die andere Leiste übergeben"),
				 G_CALLBACK (action_split_view_switch_next_pane_callback) },
  /* name, stock id, label */  { "SplitViewSameLocation", NULL, N_("Gleicher Ort wie _andere Leiste"),
				 "<alt>S", N_("Zum selben Ort wie in der zusätzlichen Leiste gehen"),
				 G_CALLBACK (action_split_view_same_location_callback) },
  /* name, stock id, label */  { "Add Bookmark", "xsi-bookmark-new-symbolic", N_("Lesezeichen _hinzufügen"),
                                 "<control>d", N_("Ein Lesezeichen für den aktuellen Ort zu diesem Menü hinzufügen"),
                                 G_CALLBACK (action_add_bookmark_callback) },
  /* name, stock id, label */  { "Edit Bookmarks", NULL, N_("Lesezeichen _bearbeiten …"),
                                 "<control>b", N_("Ein Fenster anzeigen, dass das Bearbeiten der Lesezeichen in diesem Menü erlaubt"),
                                 G_CALLBACK (action_edit_bookmarks_callback) },
  { "TabsPrevious", NULL, N_("_Vorheriger Reiter"), "<control>Page_Up",
    N_("Vorherigen Reiter aktivieren"),
    G_CALLBACK (action_tabs_previous_callback) },
  { "TabsNext", NULL, N_("_Nächster Reiter"), "<control>Page_Down",
    N_("Nächsten Reiter aktivieren"),
    G_CALLBACK (action_tabs_next_callback) },
  { "TabsMoveLeft", NULL, N_("Reiter nach _links verschieben"), "<shift><control>Page_Up",
    N_("Aktuellen Reiter nach links verschieben"),
    G_CALLBACK (action_tabs_move_left_callback) },
  { "TabsMoveRight", NULL, N_("Reiter nach _rechts verschieben"), "<shift><control>Page_Down",
    N_("Aktuellen Reiter nach rechts verschieben"),
    G_CALLBACK (action_tabs_move_right_callback) },
  { "Sidebar List", NULL, N_("Seitenleiste") },
  { "Toolbar List", NULL, N_("Werkzeugleiste") }
};

static const GtkToggleActionEntry main_toggle_entries[] = {
  /* name, stock id */         { "Show Hidden Files", NULL,
  /* label, accelerator */       N_("_Verborgene Dateien anzeigen"), "<control>H",
  /* tooltip */                  N_("Verborgene Dateien im momentan geöffneten Fenster anzeigen/verbergen"),
                                 G_CALLBACK (action_show_hidden_files_callback),
                                 TRUE },
  /* name, stock id */     { "Show Hide Toolbar", NULL,
  /* label, accelerator */   N_("_Hauptwerkzeugleiste"), NULL,
  /* tooltip */              N_("Die Sichtbarkeit der Hauptwerkzeugleiste dieses Fensters ändern"),
			     NULL,
  /* is_active */            TRUE },
  /* name, stock id */     { "Show Hide Sidebar", NULL,
  /* label, accelerator */   N_("Seitenleiste _anzeigen"), "F9",
  /* tooltip */              N_("Die Sichtbarkeit der Seitenleiste dieses Fensters ändern"),
                             G_CALLBACK (action_show_hide_sidebar_callback),
  /* is_active */            TRUE },
  /* name, stock id */     { "Show Hide Statusbar", NULL,
  /* label, accelerator */   N_("St_atusleiste"), NULL,
  /* tooltip */              N_("Die Sichtbarkeit der Statusleiste dieses Fensters ändern"),
                             NULL,
  /* is_active */            TRUE },
  /* name, stock id */     { NOLPHIN_ACTION_SHOW_HIDE_MENUBAR, NULL,
  /* label, accelerator */   N_("_Menüleiste"), NULL,
  /* tooltip */              N_("Die Standardsichtbarkeit der Menüleiste ändern"),
                             NULL,
  /* is_active */            TRUE },
  /* name, stock id */     { "Search", "xsi-edit-find-symbolic",
  /* label, accelerator */   N_("Nach Dateien _suchen …"), "<control>f",
  /* tooltip */              N_("Dokumente und Ordner suchen"),
			     NULL,
  /* is_active */            FALSE },
  /* name, stock id */     { NOLPHIN_ACTION_SHOW_HIDE_EXTRA_PANE, NULL,
  /* label, accelerator */   N_("_Zusätzliche Leiste"), "F3",
  /* tooltip */              N_("Eine weitere Ordneransicht nebeneinander öffnen"),
                             G_CALLBACK (action_split_view_callback),
  /* is_active */            FALSE },
  /* name, stock id */     { NOLPHIN_ACTION_SHOW_HIDE_TERMINAL, NULL,
  /* label, accelerator */   N_("_Terminal"), "F4",
  /* tooltip */              N_("Ein integriertes Terminal im aktuellen Ordner öffnen"),
                             G_CALLBACK (action_show_hide_terminal_callback),
  /* is_active */            FALSE },
  /* name, stock id */     { NOLPHIN_ACTION_SHOW_HIDE_PREVIEW, NULL,
  /* label, accelerator */   N_("Info & _Vorschau"), "F11",
  /* tooltip */              N_("Informationen und eine Vorschau der ausgewählten Datei anzeigen"),
                             G_CALLBACK (action_show_hide_preview_callback),
  /* is_active */            FALSE },
    /* name, stock id */         { NOLPHIN_ACTION_SHOW_THUMBNAILS, NULL,
  /* label, accelerator */       N_("_Vorschaubilder anzeigen"), NULL,
  /* tooltip */                  N_("Die Anzeige der Vorschaubilder im aktuellen Verzeichnis umschalten"),
  /* callback */                 G_CALLBACK (action_show_thumbnails_callback),
  /* default */                  FALSE },
};

static const GtkRadioActionEntry sidebar_radio_entries[] = {
	{ "Sidebar Places", NULL,
	  N_("Orte"), NULL, N_("»Orte« als Voreinstellung für die Seitenleiste festlegen"),
	  SIDEBAR_PLACES },
	{ "Sidebar Tree", NULL,
	  N_("Baumansicht"), "F7", N_("»Baumansicht« als Voreinstellung für die Seitenleiste festlegen"),
	  SIDEBAR_TREE }
};

static const GtkRadioActionEntry view_radio_entries[] = {
    { "IconView", NULL,
      N_("Symbolansicht"), "<ctrl>1", N_("Icon View"),
      ICON_VIEW },
    { "ListView", NULL,
      N_("Listenansicht"), "<ctrl>2", N_("List View"),
      LIST_VIEW },
    { "CompactView", NULL,
      N_("Kompaktansicht"), "<ctrl>3", N_("Compact View"),
      COMPACT_VIEW },
    { "GalleryView", NULL,
      N_("Galerieansicht"), "<ctrl>4", N_("Gallery View"),
      GALLERY_VIEW }
};

static const GtkRadioActionEntry toolbar_radio_entries[] = {
    { NOLPHIN_ACTION_TOOLBAR_ALWAYS_SHOW_PATHBAR, NULL,
      N_("Pfadleiste"), NULL, N_("Pfadleiste immer bevorzugen"),
      TOOLBAR_PATHBAR },
    { NOLPHIN_ACTION_TOOLBAR_ALWAYS_SHOW_ENTRY, NULL,
      N_("Pfadeingabe"), NULL, N_("Pfadeingabe immer bevorzugen"),
      TOOLBAR_ENTRY }
};

GtkActionGroup *
nolphin_window_create_toolbar_action_group (NolphinWindow *window)
{
    gboolean show_location_entry_initially;

	NolphinNavigationState *navigation_state;
	GtkActionGroup *action_group;
	GtkAction *action;

	action_group = gtk_action_group_new ("ToolbarActions");
	gtk_action_group_set_translation_domain (action_group, GETTEXT_PACKAGE);

	action = g_object_new (NOLPHIN_TYPE_NAVIGATION_ACTION,
			       "name", NOLPHIN_ACTION_BACK,
			       "label", _("_Zurück"),
			       "icon_name", "xsi-go-previous-symbolic",
			       "tooltip", _("Zum vorher besuchten Ort gehen"),
			       "arrow-tooltip", _("Im Verlauf zurück bewegen"),
			       "window", window,
			       "direction", NOLPHIN_NAVIGATION_DIRECTION_BACK,
			       "sensitive", FALSE,
			       NULL);
	g_signal_connect (action, "activate",
			  G_CALLBACK (action_back_callback), window);
	gtk_action_group_add_action (action_group, action);

	g_object_unref (action);

	action = g_object_new (NOLPHIN_TYPE_NAVIGATION_ACTION,
			       "name", NOLPHIN_ACTION_FORWARD,
			       "label", _("_Vorwärts"),
			       "icon_name", "xsi-go-next-symbolic",
			       "tooltip", _("Zum als nächstes besuchten Ort gehen"),
			       "arrow-tooltip", _("Im Verlauf vorwärts bewegen"),
			       "window", window,
			       "direction", NOLPHIN_NAVIGATION_DIRECTION_FORWARD,
			       "sensitive", FALSE,
			       NULL);
	g_signal_connect (action, "activate",
			  G_CALLBACK (action_forward_callback), window);
	gtk_action_group_add_action (action_group, action);

	g_object_unref (action);

	/**
	 * Nolphin 2.30/2.32 type actions
	 */
   	action = g_object_new (NOLPHIN_TYPE_NAVIGATION_ACTION,
   			       "name", NOLPHIN_ACTION_UP,
   			       "label", _("_Hoch"),
   			       "icon_name", "xsi-go-up-symbolic",
   			       "tooltip", _("Zum übergeordneten Ordner gehen"),
   			       "arrow-tooltip", _("Im Verlauf vorwärts bewegen"),
   			       "window", window,
   			       "direction", NOLPHIN_NAVIGATION_DIRECTION_UP,
   			       NULL);
   	g_signal_connect (action, "activate",
   			  G_CALLBACK (action_up_callback), window);
   	gtk_action_group_add_action (action_group, action);

   	g_object_unref (action);

   	action = g_object_new (NOLPHIN_TYPE_NAVIGATION_ACTION,
   			       "name", NOLPHIN_ACTION_RELOAD,
   			       "label", _("_Aktualisieren"),
   			       "icon_name", "xsi-view-refresh-symbolic",
   			       "tooltip", _("Den aktuellen Ort aktualisieren"),
   			       "window", window,
   			       "direction", NOLPHIN_NAVIGATION_DIRECTION_RELOAD,
   			       NULL);
   	g_signal_connect (action, "activate",
   			  G_CALLBACK (action_reload_callback), window);
   	gtk_action_group_add_action (action_group, action);

   	g_object_unref (action);

   	action = g_object_new (NOLPHIN_TYPE_NAVIGATION_ACTION,
   			       "name", NOLPHIN_ACTION_HOME,
   			       "label", _("_Persönlicher Ordner"),
   			       "icon_name", "xsi-go-home-symbolic",
   			       "tooltip", _("Zum persönlichen Ordner gehen"),
   			       "window", window,
   			       "direction", NOLPHIN_NAVIGATION_DIRECTION_HOME,
   			       NULL);
   	g_signal_connect (action, "activate",
   			  G_CALLBACK (action_home_callback), window);
   	gtk_action_group_add_action (action_group, action);

   	g_object_unref (action);

   	action = g_object_new (NOLPHIN_TYPE_NAVIGATION_ACTION,
   			       "name", NOLPHIN_ACTION_COMPUTER,
   			       "label", _("_Rechner"),
   			       "icon_name", "xsi-computer-symbolic",
   			       "tooltip", _("Zum Rechner gehen"),
   			       "window", window,
   			       "direction", NOLPHIN_NAVIGATION_DIRECTION_COMPUTER,
   			       NULL);
   	g_signal_connect (action, "activate",
   			  G_CALLBACK (action_go_to_computer_callback), window);
   	gtk_action_group_add_action (action_group, action);

   	g_object_unref (action);

    action = GTK_ACTION (gtk_toggle_action_new (NOLPHIN_ACTION_TOGGLE_LOCATION,
                                                _("Speicherort"),
                                                _("Pfadeingabe ein/aus"),
                                                NULL));
    gtk_action_group_add_action (action_group, GTK_ACTION (action));
    show_location_entry_initially = g_settings_get_boolean (nolphin_preferences, NOLPHIN_PREFERENCES_SHOW_LOCATION_ENTRY);
    gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), show_location_entry_initially);
    g_signal_connect (action, "activate",
                      G_CALLBACK (action_toggle_location_entry_callback), window);
    gtk_action_set_icon_name (GTK_ACTION (action), "nolphin-location-symbolic");

    g_object_unref (action);

    action = GTK_ACTION (gtk_action_new (NOLPHIN_ACTION_NEW_FOLDER,
                                                _("Neuer Ordner"),
                                                _("Neuen Ordner erstellen"),
                                                NULL));
    gtk_action_group_add_action (action_group, GTK_ACTION (action));
    g_signal_connect (action, "activate",
                      G_CALLBACK (action_new_folder_callback), window);
    gtk_action_set_icon_name (GTK_ACTION (action), "xsi-folder-new-symbolic");
    g_object_unref (action);

    action = GTK_ACTION (gtk_action_new (NOLPHIN_ACTION_OPEN_IN_TERMINAL,
                                                _("Im Terminal öffnen"),
                                                _("Terminal im aktiven Ordner öffnen"),
                                                NULL));
    gtk_action_group_add_action (action_group, GTK_ACTION (action));
    g_signal_connect (action, "activate",
                      G_CALLBACK (action_open_terminal_callback), window);
    gtk_action_set_icon_name (GTK_ACTION (action), "xsi-utilities-terminal-symbolic");
    g_object_unref (action);


    action = GTK_ACTION (gtk_toggle_action_new (NOLPHIN_ACTION_ICON_VIEW,
                         _("Symbole"),
                         _("Symbolansicht"),
                         NULL));
    g_signal_connect (action, "activate",
                      G_CALLBACK (action_icon_view_callback),
                      window);
   	gtk_action_group_add_action (action_group, action);
    gtk_action_set_icon_name (GTK_ACTION (action), "xsi-view-grid-symbolic");
   	g_object_unref (action);

    action = GTK_ACTION (gtk_toggle_action_new (NOLPHIN_ACTION_LIST_VIEW,
                         _("Liste"),
                         _("Listenansicht"),
                         NULL));
    g_signal_connect (action, "activate",
                      G_CALLBACK (action_list_view_callback),
                      window);
   	gtk_action_group_add_action (action_group, action);
    gtk_action_set_icon_name (GTK_ACTION (action), "xsi-view-list-symbolic");

   	g_object_unref (action);

    action = GTK_ACTION (gtk_toggle_action_new (NOLPHIN_ACTION_COMPACT_VIEW,
                         _("Kompakt"),
                         _("Kompaktansicht"),
                         NULL));
   	g_signal_connect (action, "activate",
                      G_CALLBACK (action_compact_view_callback),
                      window);
   	gtk_action_group_add_action (action_group, action);
    gtk_action_set_icon_name (GTK_ACTION (action), "xsi-view-compact-symbolic");

   	g_object_unref (action);

 	action = GTK_ACTION (gtk_toggle_action_new (NOLPHIN_ACTION_SEARCH,
 				_("Suche"),_("Dokumente und Ordner suchen"),
 				NULL));

  	gtk_action_group_add_action (action_group, action);
    gtk_action_set_icon_name (GTK_ACTION (action), "xsi-edit-find-symbolic");

  	g_object_unref (action);
    
    action = GTK_ACTION (gtk_toggle_action_new (NOLPHIN_ACTION_SHOW_THUMBNAILS,
                         _("Vorschaubilder anzeigen"),
                         _("Show Thumbnails"),
                         NULL));
   	g_signal_connect (action, "activate",
                      G_CALLBACK (action_show_thumbnails_callback),
                      window);
   	gtk_action_group_add_action (action_group, action);
    gtk_action_set_icon_name (GTK_ACTION (action), "xsi-preview-symbolic");

   	g_object_unref (action);

    action = GTK_ACTION (gtk_toggle_action_new (NOLPHIN_ACTION_SHOW_HIDE_EXTRA_PANE,
                         NULL,
                         _("Eine weitere Ordneransicht nebeneinander öffnen"),
                         NULL));
    g_signal_connect (action, "activate",
                      G_CALLBACK (action_split_view_callback),
                      window);

    gtk_action_group_add_action (action_group, action);
    gtk_action_set_icon_name (GTK_ACTION (action), "xsi-view-dual-symbolic");

    g_object_unref (action);

	navigation_state = nolphin_window_get_navigation_state (window);
	nolphin_navigation_state_add_group (navigation_state, action_group);

	return action_group;
}

static void
window_menus_set_bindings (NolphinWindow *window)
{
	GtkActionGroup *action_group;
	GtkAction *action;

	action_group = nolphin_window_get_main_action_group (window);

	action = gtk_action_group_get_action (action_group,
					      NOLPHIN_ACTION_SHOW_HIDE_TOOLBAR);

	g_settings_bind (nolphin_window_state,
			 NOLPHIN_WINDOW_STATE_START_WITH_TOOLBAR,
			 action,
			 "active",
			 G_SETTINGS_BIND_DEFAULT);

	action = gtk_action_group_get_action (action_group,
					      NOLPHIN_ACTION_SHOW_HIDE_STATUSBAR);

	g_settings_bind (nolphin_window_state,
			 NOLPHIN_WINDOW_STATE_START_WITH_STATUS_BAR,
			 action,
			 "active",
			 G_SETTINGS_BIND_DEFAULT);

    action = gtk_action_group_get_action (action_group,
                          NOLPHIN_ACTION_SHOW_HIDE_MENUBAR);

    g_settings_bind (nolphin_window_state,
             NOLPHIN_WINDOW_STATE_START_WITH_MENU_BAR,
             action,
             "active",
             G_SETTINGS_BIND_DEFAULT);

	action = gtk_action_group_get_action (action_group,
					      NOLPHIN_ACTION_SHOW_HIDE_SIDEBAR);

    g_object_bind_property (window,
                            "show-sidebar",
                            action,
                            "active",
                            G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
}

void
nolphin_window_initialize_actions (NolphinWindow *window)
{
	GtkActionGroup *action_group;
	const gchar *nav_state_actions[] = {
		NOLPHIN_ACTION_BACK, NOLPHIN_ACTION_FORWARD, NOLPHIN_ACTION_UP, NOLPHIN_ACTION_RELOAD, NOLPHIN_ACTION_COMPUTER, NOLPHIN_ACTION_HOME, NOLPHIN_ACTION_EDIT_LOCATION,
		NOLPHIN_ACTION_TOGGLE_LOCATION, NOLPHIN_ACTION_SEARCH, NULL
	};

	action_group = nolphin_window_get_main_action_group (window);
	window->details->nav_state = nolphin_navigation_state_new (action_group,
								    nav_state_actions);

	window_menus_set_bindings (window);
	nolphin_window_update_show_hide_ui_elements (window);

	g_signal_connect (window, "loading_uri",
			  G_CALLBACK (nolphin_window_update_split_view_actions_sensitivity),
			  NULL);
}

/**
 * nolphin_window_initialize_menus
 *
 * Create and install the set of menus for this window.
 * @window: A recently-created NolphinWindow.
 */
void
nolphin_window_initialize_menus (NolphinWindow *window)
{
	GtkActionGroup *action_group;
	GtkUIManager *ui_manager;
	GtkAction *action;
      GtkAction *action_to_hide;
	gint i;

	if (window->details->ui_manager == NULL){
        window->details->ui_manager = gtk_ui_manager_new ();
    }
	ui_manager = window->details->ui_manager;

	/* shell actions */
	action_group = gtk_action_group_new ("ShellActions");
	gtk_action_group_set_translation_domain (action_group, GETTEXT_PACKAGE);
	window->details->main_action_group = action_group;
	gtk_action_group_add_actions (action_group,
				      main_entries, G_N_ELEMENTS (main_entries),
				      window);

      /* if root then hide menu items that do not work */
    if (nolphin_user_is_root () && !nolphin_treating_root_as_normal ()) {
        action_to_hide = gtk_action_group_get_action (action_group, "Go to Templates");
        gtk_action_set_visible (action_to_hide, FALSE);
    }

    /* hide menu items that are not currently supported by the vfs */
    action_to_hide = gtk_action_group_get_action (action_group, "Go to Computer");
    gtk_action_set_visible (action_to_hide, eel_vfs_supports_uri_scheme ("computer"));
    action_to_hide = gtk_action_group_get_action (action_group, "Go to Trash");
    gtk_action_set_visible (action_to_hide, eel_vfs_supports_uri_scheme ("trash"));
    action_to_hide = gtk_action_group_get_action (action_group, "Go to Network");
    gtk_action_set_visible (action_to_hide, eel_vfs_supports_uri_scheme ("network"));

	gtk_action_group_add_toggle_actions (action_group,
					     main_toggle_entries, G_N_ELEMENTS (main_toggle_entries),
					     window);
	gtk_action_group_add_radio_actions (action_group,
					    sidebar_radio_entries, G_N_ELEMENTS (sidebar_radio_entries),
					    0, G_CALLBACK (sidebar_radio_entry_changed_cb),
					    window);
    gtk_action_group_add_radio_actions (action_group,
                        view_radio_entries, G_N_ELEMENTS (view_radio_entries),
                        0, G_CALLBACK (view_radio_entry_changed_cb),
                        window);

    gboolean use_entry = g_settings_get_boolean (nolphin_preferences, NOLPHIN_PREFERENCES_SHOW_LOCATION_ENTRY);
    gtk_action_group_add_radio_actions (action_group,
                                        toolbar_radio_entries, G_N_ELEMENTS (toolbar_radio_entries),
                                        use_entry ? TOOLBAR_ENTRY : TOOLBAR_PATHBAR,
                                        G_CALLBACK (toolbar_radio_entry_changed_cb),
                                        window);

	action = gtk_action_group_get_action (action_group, NOLPHIN_ACTION_UP);
	g_object_set (action, "short_label", _("_Hoch"), NULL);

	action = gtk_action_group_get_action (action_group, NOLPHIN_ACTION_HOME);
	g_object_set (action, "short_label", _("_Persönlicher Ordner"), NULL);

  	action = gtk_action_group_get_action (action_group, NOLPHIN_ACTION_EDIT_LOCATION);
  	g_object_set (action, "short_label", _("_Ort"), NULL);

	action = gtk_action_group_get_action (action_group, NOLPHIN_ACTION_SHOW_HIDDEN_FILES);

    if (NOLPHIN_IS_DESKTOP_WINDOW (window)) {
        gtk_action_set_visible (action, FALSE);
    } else {
        g_signal_handlers_block_by_func (action, action_show_hidden_files_callback, window);
        gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action),
                                      g_settings_get_boolean (nolphin_preferences,
                                      NOLPHIN_PREFERENCES_SHOW_HIDDEN_FILES));
        g_signal_handlers_unblock_by_func (action, action_show_hidden_files_callback, window);
    }

    g_signal_connect_object ( NOLPHIN_WINDOW (window), "notify::sidebar-view-id",
                             G_CALLBACK (update_side_bar_radio_buttons), window, 0);

	/* Alt+N for the first 10 tabs */
	for (i = 0; i < 10; ++i) {
		gchar action_name[80];
		gchar accelerator[80];

		snprintf(action_name, sizeof (action_name), "Tab%d", i);
		action = gtk_action_new (action_name, NULL, NULL, NULL);
		g_object_set_data (G_OBJECT (action), "num", GINT_TO_POINTER (i));
		g_signal_connect (action, "activate",
				G_CALLBACK (action_tab_change_action_activate_callback), window);
		snprintf(accelerator, sizeof (accelerator), "<alt>%d", (i+1)%10);
		gtk_action_group_add_action_with_accel (action_group, action, accelerator);
		g_object_unref (action);
		gtk_ui_manager_add_ui (ui_manager,
				gtk_ui_manager_new_merge_id (ui_manager),
				"/",
				action_name,
				action_name,
				GTK_UI_MANAGER_ACCELERATOR,
				FALSE);

	}

	gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);
	g_object_unref (action_group); /* owned by ui_manager */

	gtk_window_add_accel_group (GTK_WINDOW (window),
				    gtk_ui_manager_get_accel_group (ui_manager));

	g_signal_connect (ui_manager, "connect_proxy",
			  G_CALLBACK (connect_proxy_cb), window);
	g_signal_connect (ui_manager, "disconnect_proxy",
			  G_CALLBACK (disconnect_proxy_cb), window);

	/* add the UI */
	gtk_ui_manager_add_ui_from_resource (ui_manager, "/org/nolphin/nolphin-shell-ui.xml", NULL);

    GtkWidget *menuitem, *submenu;
    menuitem = gtk_ui_manager_get_widget (nolphin_window_get_ui_manager (window), NOLPHIN_VIEW_MENUBAR_FILE_PATH);
    submenu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (menuitem));
    g_signal_connect (submenu, "show", G_CALLBACK (on_file_menu_show), window);

	nolphin_window_initialize_trash_icon_monitor (window);
}

void
nolphin_window_finalize_menus (NolphinWindow *window)
{
	g_signal_handlers_disconnect_by_func (nolphin_trash_monitor_get(),
					      trash_state_changed_cb, window);
}

static GList *
get_extension_menus (NolphinWindow *window)
{
	NolphinWindowSlot *slot;
	GList *providers;
	GList *items;
	GList *l;

	providers = nolphin_module_get_extensions_for_type (NOLPHIN_TYPE_MENU_PROVIDER);
	items = NULL;

	slot = nolphin_window_get_active_slot (window);

	for (l = providers; l != NULL; l = l->next) {
		NolphinMenuProvider *provider;
		GList *file_items;

		provider = NOLPHIN_MENU_PROVIDER (l->data);
		file_items = nolphin_menu_provider_get_background_items (provider,
									  GTK_WIDGET (window),
									  slot->viewed_file);
		items = g_list_concat (items, file_items);
	}

	nolphin_module_extension_list_free (providers);

	return items;
}

static void
add_extension_menu_items (NolphinWindow *window,
			  guint merge_id,
			  GtkActionGroup *action_group,
			  GList *menu_items,
			  const char *subdirectory)
{
	GtkUIManager *ui_manager;
	GList *l;

	ui_manager = window->details->ui_manager;

	for (l = menu_items; l; l = l->next) {
		NolphinMenuItem *item;
		NolphinMenu *menu;
		GtkAction *action;
		char *path;

		item = NOLPHIN_MENU_ITEM (l->data);

		g_object_get (item, "menu", &menu, NULL);

		action = nolphin_action_from_menu_item (item, GTK_WIDGET (window));
		gtk_action_group_add_action_with_accel (action_group, action, NULL);

		path = g_build_path ("/", POPUP_PATH_EXTENSION_ACTIONS, subdirectory, NULL);
		gtk_ui_manager_add_ui (ui_manager,
				       merge_id,
				       path,
				       gtk_action_get_name (action),
				       gtk_action_get_name (action),
				       (menu != NULL) ? GTK_UI_MANAGER_MENU : GTK_UI_MANAGER_MENUITEM,
				       FALSE);
		g_free (path);

		path = g_build_path ("/", MENU_PATH_EXTENSION_ACTIONS, subdirectory, NULL);
		gtk_ui_manager_add_ui (ui_manager,
				       merge_id,
				       path,
				       gtk_action_get_name (action),
				       gtk_action_get_name (action),
				       (menu != NULL) ? GTK_UI_MANAGER_MENU : GTK_UI_MANAGER_MENUITEM,
				       FALSE);
		g_free (path);

		/* recursively fill the menu */
		if (menu != NULL) {
			char *subdir;
			GList *children;

			children = nolphin_menu_get_items (menu);

			subdir = g_build_path ("/", subdirectory, "/", gtk_action_get_name (action), NULL);
			add_extension_menu_items (window,
						  merge_id,
						  action_group,
						  children,
						  subdir);

			nolphin_menu_item_list_free (children);
			g_free (subdir);
		}
	}
}

void
nolphin_window_load_extension_menus (NolphinWindow *window)
{
	GtkActionGroup *action_group;
	GList *items;
	guint merge_id;

	if (window->details->extensions_menu_merge_id != 0) {
		gtk_ui_manager_remove_ui (window->details->ui_manager,
					  window->details->extensions_menu_merge_id);
		window->details->extensions_menu_merge_id = 0;
	}

	if (window->details->extensions_menu_action_group != NULL) {
		gtk_ui_manager_remove_action_group (window->details->ui_manager,
						    window->details->extensions_menu_action_group);
		window->details->extensions_menu_action_group = NULL;
	}

	merge_id = gtk_ui_manager_new_merge_id (window->details->ui_manager);
	window->details->extensions_menu_merge_id = merge_id;
	action_group = gtk_action_group_new ("ExtensionsMenuGroup");
	window->details->extensions_menu_action_group = action_group;
	gtk_action_group_set_translation_domain (action_group, GETTEXT_PACKAGE);
	gtk_ui_manager_insert_action_group (window->details->ui_manager, action_group, 0);
	g_object_unref (action_group); /* owned by ui manager */

	items = get_extension_menus (window);

	if (items != NULL) {
		add_extension_menu_items (window, merge_id, action_group, items, "");

		g_list_foreach (items, (GFunc) g_object_unref, NULL);
		g_list_free (items);
	}
}
