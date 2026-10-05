/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nolphin-gid-projects.c: Projektliste und Branch-Erkennung der GID-Projekte (§60.2)
 *
 * Bewusst ohne Abhängigkeit zu Fenster und Panel, damit sie ohne Display
 * getestet werden kann.
 */

#include <config.h>

#include "nolphin-gid-projects.h"

#include <libnolphin-private/nolphin-global-preferences.h>

#include <string.h>

static gchar *
normalize_path (const gchar *path)
{
	gchar *absolute = g_canonicalize_filename (path, NULL);
	gsize len = strlen (absolute);

	while (len > 1 && absolute[len - 1] == G_DIR_SEPARATOR)
		absolute[--len] = '\0';
	return absolute;
}

gchar **
nolphin_gid_projects_get (void)
{
	return g_settings_get_strv (nolphin_preferences, NOLPHIN_PREFERENCES_GID_PROJECTS);
}

gboolean
nolphin_gid_projects_contains (const gchar *path)
{
	gchar *norm;
	gchar **list;
	gboolean found;

	g_return_val_if_fail (path != NULL, FALSE);

	norm = normalize_path (path);
	list = nolphin_gid_projects_get ();
	found = g_strv_contains ((const gchar * const *) list, norm);
	g_strfreev (list);
	g_free (norm);
	return found;
}

gboolean
nolphin_gid_projects_add (const gchar *path)
{
	gchar *norm;
	gchar **list;
	GPtrArray *array;
	guint i;
	gboolean added = FALSE;

	g_return_val_if_fail (path != NULL, FALSE);

	norm = normalize_path (path);
	list = nolphin_gid_projects_get ();
	if (g_file_test (norm, G_FILE_TEST_IS_DIR) &&
	    !g_strv_contains ((const gchar * const *) list, norm)) {
		array = g_ptr_array_new ();
		for (i = 0; list[i] != NULL; i++)
			g_ptr_array_add (array, list[i]);
		g_ptr_array_add (array, norm);
		g_ptr_array_add (array, NULL);
		g_settings_set_strv (nolphin_preferences, NOLPHIN_PREFERENCES_GID_PROJECTS,
				     (const gchar * const *) array->pdata);
		g_ptr_array_free (array, TRUE);
		added = TRUE;
	}
	g_strfreev (list);
	g_free (norm);
	return added;
}

void
nolphin_gid_projects_remove (const gchar *path)
{
	gchar *norm;
	gchar **list;
	GPtrArray *array;
	guint i;

	g_return_if_fail (path != NULL);

	norm = normalize_path (path);
	list = nolphin_gid_projects_get ();
	array = g_ptr_array_new ();
	for (i = 0; list[i] != NULL; i++) {
		if (g_strcmp0 (list[i], norm) != 0)
			g_ptr_array_add (array, list[i]);
	}
	g_ptr_array_add (array, NULL);
	g_settings_set_strv (nolphin_preferences, NOLPHIN_PREFERENCES_GID_PROJECTS,
			     (const gchar * const *) array->pdata);
	g_ptr_array_free (array, TRUE);
	g_strfreev (list);
	g_free (norm);
}

/* --- Git-Branch ------------------------------------------------------------------ */

static gchar *
head_file_for_project (const gchar *path)
{
	gchar *dot_git = g_build_filename (path, ".git", NULL);
	gchar *head = NULL;

	if (g_file_test (dot_git, G_FILE_TEST_IS_DIR)) {
		head = g_build_filename (dot_git, "HEAD", NULL);
	} else if (g_file_test (dot_git, G_FILE_TEST_IS_REGULAR)) {
		/* Worktree/Submodul: ".git" ist eine Datei "gitdir: <Pfad>". */
		gchar *contents = NULL;

		if (g_file_get_contents (dot_git, &contents, NULL, NULL) &&
		    g_str_has_prefix (contents, "gitdir:")) {
			gchar *dir = g_strstrip (g_strdup (contents + strlen ("gitdir:")));

			if (!g_path_is_absolute (dir)) {
				gchar *abs = g_build_filename (path, dir, NULL);
				g_free (dir);
				dir = abs;
			}
			head = g_build_filename (dir, "HEAD", NULL);
			g_free (dir);
		}
		g_free (contents);
	}
	g_free (dot_git);
	return head;
}

gchar *
nolphin_gid_project_get_branch (const gchar *path)
{
	gchar *head_path, *contents = NULL, *branch = NULL;

	g_return_val_if_fail (path != NULL, NULL);

	head_path = head_file_for_project (path);
	if (head_path == NULL)
		return NULL;
	if (g_file_get_contents (head_path, &contents, NULL, NULL)) {
		g_strstrip (contents);
		if (g_str_has_prefix (contents, "ref: refs/heads/")) {
			branch = g_strdup (contents + strlen ("ref: refs/heads/"));
		} else if (g_str_has_prefix (contents, "ref: ")) {
			branch = g_strdup (contents + strlen ("ref: "));
		} else if (strlen (contents) >= 7) {
			branch = g_strndup (contents, 7);
		}
	}
	g_free (contents);
	g_free (head_path);
	return branch;
}
