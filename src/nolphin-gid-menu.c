/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nolphin-gid-menu.c: Menüeinträge der GID-Projekte (§60.4)
 */

#include <config.h>

#include "nolphin-gid-menu.h"
#include "nolphin-gid-projects.h"
#include "nolphin-gid-sidebar.h"
#include "nolphin-workspace-panel.h"

#include <libnolphin-private/nolphin-global-preferences.h>

#include <glib/gi18n.h>

#define MENU_PATH_GID_PLACEHOLDER "/MenuBar/Other Menus/Go/GIDProjectsMenu/GID Projects Placeholder"
#define ACTION_TOGGLE "Show Hide GID Projects"

static void
on_project_activate (GtkAction *action, gpointer user_data)
{
	NolphinWindow *window = NOLPHIN_WINDOW (user_data);
	const gchar *path = g_object_get_data (G_OBJECT (action), "gid-path");
	GFile *folder;

	if (path == NULL)
		return;
	folder = g_file_new_for_path (path);
	nolphin_workspace_panel_show_gid (nolphin_window_get_workspace_panel (window), window, folder);
	g_object_unref (folder);
}

/* "_" in Ordnernamen würde als Mnemonic gelesen. */
static gchar *
escape_label (const gchar *name)
{
	gchar **parts = g_strsplit (name, "_", -1);
	gchar *escaped = g_strjoinv ("__", parts);

	g_strfreev (parts);
	return escaped;
}

static void
refresh_projects_menu (NolphinWindow *window)
{
	GtkUIManager *ui = nolphin_window_get_ui_manager (window);
	GtkActionGroup *old_group = g_object_get_data (G_OBJECT (window), "nolphin-gid-group");
	guint old_id = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (window), "nolphin-gid-merge-id"));
	GtkActionGroup *group;
	guint merge_id;
	gchar **projects;
	guint i;

	if (old_id != 0)
		gtk_ui_manager_remove_ui (ui, old_id);
	if (old_group != NULL)
		gtk_ui_manager_remove_action_group (ui, old_group);

	merge_id = gtk_ui_manager_new_merge_id (ui);
	group = gtk_action_group_new ("GIDProjectsGroup");
	gtk_ui_manager_insert_action_group (ui, group, -1);

	projects = nolphin_gid_projects_get ();
	for (i = 0; projects[i] != NULL; i++) {
		gchar *name = g_strdup_printf ("gid-project-%u", i);
		gchar *base = g_path_get_basename (projects[i]);
		gchar *label = escape_label (base);
		GtkAction *action = gtk_action_new (name, label, projects[i], NULL);

		g_object_set_data_full (G_OBJECT (action), "gid-path", g_strdup (projects[i]), g_free);
		g_signal_connect (action, "activate", G_CALLBACK (on_project_activate), window);
		gtk_action_group_add_action (group, action);
		gtk_ui_manager_add_ui (ui, merge_id, MENU_PATH_GID_PLACEHOLDER, name, name,
				       GTK_UI_MANAGER_MENUITEM, FALSE);
		g_object_unref (action);
		g_free (label);
		g_free (base);
		g_free (name);
	}
	if (projects[0] == NULL) {
		GtkAction *none = gtk_action_new ("gid-project-none", _("(Keine Projekte)"), NULL, NULL);

		gtk_action_set_sensitive (none, FALSE);
		gtk_action_group_add_action (group, none);
		gtk_ui_manager_add_ui (ui, merge_id, MENU_PATH_GID_PLACEHOLDER, "gid-project-none",
				       "gid-project-none", GTK_UI_MANAGER_MENUITEM, FALSE);
		g_object_unref (none);
	}
	g_strfreev (projects);

	g_object_set_data_full (G_OBJECT (window), "nolphin-gid-group", group, g_object_unref);
	g_object_set_data (G_OBJECT (window), "nolphin-gid-merge-id", GUINT_TO_POINTER (merge_id));
}

static void
on_add_activate (GtkAction *action, gpointer user_data)
{
	nolphin_gid_choose_and_add_project (NOLPHIN_WINDOW (user_data));
}

static void
on_toggle_activate (GtkAction *action, gpointer user_data)
{
	g_settings_set_boolean (nolphin_window_state, NOLPHIN_WINDOW_STATE_SHOW_GID_PROJECTS,
				gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action)));
}

/* Der Schalter folgt der Einstellung, auch wenn sie in einem anderen Fenster geändert wurde. */
static void
on_show_setting_changed (GSettings *settings, const gchar *key, gpointer user_data)
{
	GtkActionGroup *group = g_object_get_data (G_OBJECT (user_data), "nolphin-gid-static-group");
	GtkToggleAction *toggle = group != NULL ?
		GTK_TOGGLE_ACTION (gtk_action_group_get_action (group, ACTION_TOGGLE)) : NULL;
	gboolean value = g_settings_get_boolean (settings, key);

	if (toggle != NULL && gtk_toggle_action_get_active (toggle) != value) {
		g_signal_handlers_block_by_func (toggle, on_toggle_activate, NULL);
		gtk_toggle_action_set_active (toggle, value);
		g_signal_handlers_unblock_by_func (toggle, on_toggle_activate, NULL);
	}
}

/* Registriert die statischen Aktionen. Muss vor dem Zusammenführen des
 * Menü-XML geschehen, sonst findet der UI-Manager sie beim Aufbau nicht
 * ("missing action"). */
void
nolphin_gid_menu_register_actions (NolphinWindow *window)
{
	static const GtkActionEntry entries[] = {
		{ "GIDProjectsMenu", NULL, N_("_GID-Projekte") },
		{ "GID Add Project", "list-add-symbolic",
		  N_("Projekt _hinzufügen …"), NULL,
		  N_("Einen lokalen Ordner als GID-Projekt hinzufügen"),
		  G_CALLBACK (on_add_activate) },
	};
	GtkUIManager *ui;
	GtkActionGroup *group;
	GtkToggleAction *toggle;

	g_return_if_fail (NOLPHIN_IS_WINDOW (window));

	if (g_object_get_data (G_OBJECT (window), "nolphin-gid-static-group") != NULL) {
		return;
	}

	ui = nolphin_window_get_ui_manager (window);
	group = gtk_action_group_new ("GIDStaticGroup");
	gtk_action_group_add_actions (group, entries, G_N_ELEMENTS (entries), window);
	toggle = gtk_toggle_action_new (ACTION_TOGGLE, _("GID-_Projekte anzeigen"),
					_("Den Bereich GID-Projekte in der Seitenleiste anzeigen oder verbergen"), NULL);
	gtk_toggle_action_set_active (toggle, g_settings_get_boolean (nolphin_window_state,
								      NOLPHIN_WINDOW_STATE_SHOW_GID_PROJECTS));
	g_signal_connect (toggle, "toggled", G_CALLBACK (on_toggle_activate), NULL);
	gtk_action_group_add_action (group, GTK_ACTION (toggle));
	g_object_unref (toggle);
	gtk_ui_manager_insert_action_group (ui, group, -1);
	g_object_set_data_full (G_OBJECT (window), "nolphin-gid-static-group", group, g_object_unref);

	g_signal_connect_object (nolphin_window_state, "changed::" NOLPHIN_WINDOW_STATE_SHOW_GID_PROJECTS,
				 G_CALLBACK (on_show_setting_changed), window, 0);
}

void
nolphin_gid_menu_initialize (NolphinWindow *window)
{
	g_return_if_fail (NOLPHIN_IS_WINDOW (window));

	nolphin_gid_menu_register_actions (window);

	refresh_projects_menu (window);
	g_signal_connect_object (nolphin_preferences, "changed::" NOLPHIN_PREFERENCES_GID_PROJECTS,
				 G_CALLBACK (refresh_projects_menu), window, G_CONNECT_SWAPPED);
}
