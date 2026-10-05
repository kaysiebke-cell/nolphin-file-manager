/* Prüft die Projektliste und die Branch-Erkennung der GID-Projekte
 * (src/nolphin-gid-projects.c, §60.2) mit einem Speicher-Backend für
 * GSettings und echten temporären Ordnern. */

#include <gio/gio.h>
#include <glib/gstdio.h>
#include <string.h>

#include "../src/nolphin-gid-projects.h"

GSettings *nolphin_preferences = NULL;

static gint exit_code = 0;

static void
check (gboolean ok, const gchar *message)
{
	if (!ok) {
		g_printerr ("FAIL: %s\n", message);
		exit_code = 1;
	}
}

static void
write_file (const gchar *dir, const gchar *name, const gchar *contents)
{
	gchar *path = g_build_filename (dir, name, NULL);

	g_file_set_contents (path, contents, -1, NULL);
	g_free (path);
}

int
main (void)
{
	gchar *root = g_dir_make_tmp ("nolphin-gid-test-XXXXXX", NULL);
	gchar *a = g_build_filename (root, "projekt-a", NULL);
	gchar *b = g_build_filename (root, "projekt-b", NULL);
	gchar *dot_git, *branch, *missing, *wt, *wt_git, *gitdir;
	gchar **list;

	g_assert_nonnull (root);
	g_mkdir (a, 0755);
	g_mkdir (b, 0755);
	write_file (a, "README.md", "# A\n");

	nolphin_preferences = g_settings_new ("org.nolphin.preferences");

	list = nolphin_gid_projects_get ();
	check (g_strv_length (list) == 0, "Liste zu Beginn nicht leer");
	g_strfreev (list);

	check (nolphin_gid_projects_add (a), "Ordner a nicht hinzugefügt");
	check (nolphin_gid_projects_add (b), "Ordner b nicht hinzugefügt");
	check (!nolphin_gid_projects_add (a), "Duplikat wurde hinzugefügt");
	{
		gchar *with_slash = g_strconcat (a, "/", NULL);

		check (!nolphin_gid_projects_add (with_slash), "Duplikat mit Schrägstrich wurde hinzugefügt");
		g_free (with_slash);
	}
	missing = g_build_filename (root, "gibt-es-nicht", NULL);
	check (!nolphin_gid_projects_add (missing), "nicht vorhandener Ordner wurde hinzugefügt");
	check (!nolphin_gid_projects_add (a), "Duplikat nach Fehlversuch hinzugefügt");

	list = nolphin_gid_projects_get ();
	check (g_strv_length (list) == 2, "Liste hat nicht genau zwei Einträge");
	check (g_strcmp0 (list[0], a) == 0 && g_strcmp0 (list[1], b) == 0, "Reihenfolge nicht erhalten");
	g_strfreev (list);
	check (nolphin_gid_projects_contains (b), "contains(b) falsch");

	nolphin_gid_projects_remove (a);
	check (!nolphin_gid_projects_contains (a), "a nach remove noch in der Liste");
	check (nolphin_gid_projects_contains (b), "b nach remove(a) verloren");
	check (g_file_test (a, G_FILE_TEST_IS_DIR), "remove hat den Ordner gelöscht");
	{
		gchar *readme = g_build_filename (a, "README.md", NULL);

		check (g_file_test (readme, G_FILE_TEST_EXISTS), "remove hat Dateien gelöscht");
		g_free (readme);
	}
	nolphin_gid_projects_remove (missing); /* unbekannter Eintrag: kein Fehler */

	/* Branch-Erkennung */
	branch = nolphin_gid_project_get_branch (a);
	check (branch == NULL, "Ordner ohne .git hat einen Branch");

	dot_git = g_build_filename (a, ".git", NULL);
	g_mkdir (dot_git, 0755);
	write_file (dot_git, "HEAD", "ref: refs/heads/feature/neu\n");
	branch = nolphin_gid_project_get_branch (a);
	check (g_strcmp0 (branch, "feature/neu") == 0, "Branchname falsch gelesen");
	g_free (branch);

	write_file (dot_git, "HEAD", "0123456789abcdef0123456789abcdef01234567\n");
	branch = nolphin_gid_project_get_branch (a);
	check (g_strcmp0 (branch, "0123456") == 0, "loser HEAD nicht als Kurz-Hash geliefert");
	g_free (branch);

	/* Worktree: ".git" ist eine Datei mit "gitdir:" */
	wt = g_build_filename (root, "worktree", NULL);
	gitdir = g_build_filename (root, "gitdir", NULL);
	g_mkdir (wt, 0755);
	g_mkdir (gitdir, 0755);
	write_file (gitdir, "HEAD", "ref: refs/heads/main\n");
	wt_git = g_strdup_printf ("gitdir: %s\n", gitdir);
	write_file (wt, ".git", wt_git);
	branch = nolphin_gid_project_get_branch (wt);
	check (g_strcmp0 (branch, "main") == 0, "Worktree-Branch falsch gelesen");
	g_free (branch);

	/* Aufräumen */
	{
		gchar *cmd[] = { "rm", "-rf", root, NULL };
		g_spawn_sync (NULL, cmd, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL, NULL, NULL);
	}
	g_object_unref (nolphin_preferences);
	if (exit_code == 0)
		g_print ("PASS\n");
	return exit_code;
}
