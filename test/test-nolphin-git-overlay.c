/* Prüft die Git-Overlay-Symbole (libnolphin-private/nolphin-git-overlay.c)
 * an einem echten Repository: unveränderte, geänderte und unversionierte
 * Dateien müssen den richtigen Status und die richtigen Emblem-Namen
 * bekommen, Dateien außerhalb eines Repositorys keinen Status. */

#include <gio/gio.h>
#include <stdlib.h>
#include <string.h>
#include <libnolphin-private/nolphin-git-overlay.h>

static gint exit_code = 0;

static void
fail (const gchar *message)
{
	g_printerr ("FAIL: %s\n", message);
	exit_code = 1;
}

static void
expect (const gchar *dir, const gchar *name, NolphinGitFileStatus want)
{
	gchar *path = g_build_filename (dir, name, NULL);
	GFile *file = g_file_new_for_path (path);
	NolphinGitFileStatus got = nolphin_git_overlay_get_status (file);

	if (got != want) {
		gchar *msg = g_strdup_printf ("%s: Status %d statt %d", name, got, want);
		fail (msg);
		g_free (msg);
	}
	g_object_unref (file);
	g_free (path);
}

static void
run (const gchar *dir, const gchar *cmd)
{
	gchar *full = g_strdup_printf ("cd '%s' && %s >/dev/null 2>&1", dir, cmd);

	if (system (full) != 0) {
		gchar *msg = g_strdup_printf ("Befehl fehlgeschlagen: %s", cmd);
		fail (msg);
		g_free (msg);
	}
	g_free (full);
}

int
main (int argc, char **argv)
{
	gchar *dir, *path;
	GError *error = NULL;
	GFile *outside;

	if (g_find_program_in_path ("git") == NULL) {
		g_print ("SKIP: git nicht installiert\n");
		return 0;
	}

	if (nolphin_git_overlay_get_emblem_names (NOLPHIN_GIT_STATUS_UNMODIFIED) != NULL) {
		fail ("unveränderte Dateien dürfen kein Emblem bekommen");
	}
	if (nolphin_git_overlay_get_emblem_names (NOLPHIN_GIT_STATUS_MODIFIED) == NULL ||
	    nolphin_git_overlay_get_emblem_names (NOLPHIN_GIT_STATUS_UNTRACKED) == NULL ||
	    nolphin_git_overlay_get_emblem_names (NOLPHIN_GIT_STATUS_CONFLICTED) == NULL) {
		fail ("geänderte, unversionierte und Konflikt-Dateien brauchen ein Emblem");
	}

	dir = g_dir_make_tmp ("nolphin-overlay-XXXXXX", &error);
	if (dir == NULL) {
		g_printerr ("FAIL: temp dir: %s\n", error->message);
		return 1;
	}

	run (dir, "git init -q");
	path = g_build_filename (dir, "fest.txt", NULL);
	g_file_set_contents (path, "eins\n", -1, NULL);
	g_free (path);
	path = g_build_filename (dir, "aendern.txt", NULL);
	g_file_set_contents (path, "eins\n", -1, NULL);
	g_free (path);
	run (dir, "git add . && git -c user.name=t -c user.email=t@t commit -q -m init");

	path = g_build_filename (dir, "aendern.txt", NULL);
	g_file_set_contents (path, "zwei\n", -1, NULL);
	g_free (path);
	path = g_build_filename (dir, "neu.txt", NULL);
	g_file_set_contents (path, "neu\n", -1, NULL);
	g_free (path);

	/* Der Status kommt asynchron: erster Aufruf stößt das Laden an,
	 * danach die Hauptschleife laufen lassen, bis er da ist. */
	{
		gchar *probe = g_build_filename (dir, "aendern.txt", NULL);
		GFile *probe_file = g_file_new_for_path (probe);
		gint64 deadline = g_get_monotonic_time () + 5 * G_USEC_PER_SEC;

		while (nolphin_git_overlay_get_status (probe_file) == NOLPHIN_GIT_STATUS_UNMODIFIED &&
		       g_get_monotonic_time () < deadline) {
			g_main_context_iteration (NULL, FALSE);
			g_usleep (10000);
		}
		g_object_unref (probe_file);
		g_free (probe);
	}

	expect (dir, "fest.txt", NOLPHIN_GIT_STATUS_UNMODIFIED);
	expect (dir, "aendern.txt", NOLPHIN_GIT_STATUS_MODIFIED);
	expect (dir, "neu.txt", NOLPHIN_GIT_STATUS_UNTRACKED);

	outside = g_file_new_for_path ("/etc/hostname");
	if (nolphin_git_overlay_get_status (outside) != NOLPHIN_GIT_STATUS_UNMODIFIED) {
		fail ("Datei außerhalb eines Repositorys darf keinen Git-Status haben");
	}
	g_object_unref (outside);

	path = g_strdup_printf ("rm -rf '%s'", dir);
	if (system (path) != 0) {
		g_printerr ("Hinweis: Aufräumen von %s fehlgeschlagen\n", dir);
	}
	g_free (path);
	return exit_code;
}
