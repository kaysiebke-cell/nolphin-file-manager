/* Prüft den Archiv-Manager (libnolphin-private/nolphin-archive-manage.c)
 * an einem echten ZIP: auflisten, Datei hinzufügen, Eintrag entfernen,
 * Eintrag extrahieren. Fehlt das Werkzeug "zip"/"unzip", wird
 * übersprungen. Prüft außerdem die Format-Fähigkeiten. */

#include <gio/gio.h>
#include <glib/gstdio.h>
#include <stdlib.h>
#include <string.h>
#include <libnolphin-private/nolphin-archive-manage.h>

static gint exit_code = 0;
static GMainLoop *loop;
static GAsyncResult *pending;

static void
fail (const gchar *message)
{
	g_printerr ("FAIL: %s\n", message);
	exit_code = 1;
}

static void
on_ready (GObject *source, GAsyncResult *result, gpointer user_data)
{
	pending = g_object_ref (result);
	g_main_loop_quit (loop);
}

static GAsyncResult *
wait_result (void)
{
	GAsyncResult *r;

	g_main_loop_run (loop);
	r = pending;
	pending = NULL;
	return r;
}

static GPtrArray *
list_entries (GFile *archive)
{
	GAsyncResult *r;
	GPtrArray *entries;
	GError *error = NULL;

	nolphin_archive_list_async (archive, NULL, on_ready, NULL);
	r = wait_result ();
	entries = nolphin_archive_list_finish (r, &error);
	g_object_unref (r);
	if (entries == NULL) {
		g_printerr ("FAIL: list: %s\n", error != NULL ? error->message : "?");
		exit_code = 1;
		g_clear_error (&error);
	}
	return entries;
}

static gboolean
has_entry (GPtrArray *entries, const gchar *path)
{
	guint i;

	for (i = 0; entries != NULL && i < entries->len; i++) {
		NolphinArchiveEntry *e = g_ptr_array_index (entries, i);
		if (g_strcmp0 (e->path, path) == 0) {
			return TRUE;
		}
	}
	return FALSE;
}

static gboolean
finish_ok (const gchar *what)
{
	GAsyncResult *r = wait_result ();
	GError *error = NULL;
	gboolean ok = nolphin_archive_manage_finish (r, &error);

	g_object_unref (r);
	if (!ok) {
		g_printerr ("FAIL: %s: %s\n", what, error != NULL ? error->message : "?");
		exit_code = 1;
		g_clear_error (&error);
	}
	return ok;
}

int
main (int argc, char **argv)
{
	gchar *dir, *a_path, *b_path, *c_path, *zip_path, *out_path, *cmd;
	GFile *archive, *c_file, *out_dir;
	GPtrArray *entries;
	GList *list;
	GError *error = NULL;

	loop = g_main_loop_new (NULL, FALSE);

	if (!nolphin_archive_manage_can_list (NOLPHIN_ARCHIVE_FORMAT_ZIP) ||
	    !nolphin_archive_manage_can_modify (NOLPHIN_ARCHIVE_FORMAT_ZIP)) {
		fail ("ZIP muss auflistbar und änderbar sein");
	}
	if (nolphin_archive_manage_can_list (NOLPHIN_ARCHIVE_FORMAT_UNKNOWN)) {
		fail ("unbekanntes Format darf nicht auflistbar sein");
	}
	if (nolphin_archive_manage_can_list (NOLPHIN_ARCHIVE_FORMAT_GZ)) {
		fail ("Einzeldatei-Format darf nicht auflistbar sein");
	}

	if (g_find_program_in_path ("zip") == NULL || g_find_program_in_path ("unzip") == NULL) {
		g_print ("SKIP: zip/unzip nicht installiert\n");
		return exit_code;
	}

	dir = g_dir_make_tmp ("nolphin-archman-XXXXXX", &error);
	if (dir == NULL) {
		g_printerr ("FAIL: temp dir: %s\n", error->message);
		return 1;
	}
	a_path = g_build_filename (dir, "a.txt", NULL);
	b_path = g_build_filename (dir, "b.txt", NULL);
	c_path = g_build_filename (dir, "c.txt", NULL);
	zip_path = g_build_filename (dir, "test.zip", NULL);
	out_path = g_build_filename (dir, "out", NULL);
	g_file_set_contents (a_path, "Alpha\n", -1, NULL);
	g_file_set_contents (b_path, "Beta\n", -1, NULL);
	g_file_set_contents (c_path, "Gamma\n", -1, NULL);
	g_mkdir (out_path, 0755);

	cmd = g_strdup_printf ("cd '%s' && zip -q test.zip a.txt b.txt", dir);
	if (system (cmd) != 0) {
		fail ("zip konnte das Testarchiv nicht anlegen");
		return 1;
	}
	g_free (cmd);

	archive = g_file_new_for_path (zip_path);

	/* 1. Auflisten */
	entries = list_entries (archive);
	if (entries == NULL || entries->len != 2 || !has_entry (entries, "a.txt") || !has_entry (entries, "b.txt")) {
		fail ("Auflistung des ZIP ergab nicht a.txt und b.txt");
	}
	if (entries != NULL) {
		g_ptr_array_unref (entries);
	}

	/* 2. Datei hinzufügen */
	c_file = g_file_new_for_path (c_path);
	list = g_list_append (NULL, c_file);
	nolphin_archive_add_files_async (archive, list, NULL, NULL, on_ready, NULL);
	finish_ok ("hinzufügen");
	entries = list_entries (archive);
	if (!has_entry (entries, "c.txt")) {
		fail ("c.txt fehlt nach dem Hinzufügen");
	}
	if (entries != NULL) {
		g_ptr_array_unref (entries);
	}

	/* 3. Eintrag entfernen */
	list = g_list_append (NULL, (gpointer) "a.txt");
	nolphin_archive_remove_entries_async (archive, list, NULL, on_ready, NULL);
	finish_ok ("entfernen");
	g_list_free (list);
	entries = list_entries (archive);
	if (has_entry (entries, "a.txt") || !has_entry (entries, "b.txt") || !has_entry (entries, "c.txt")) {
		fail ("a.txt wurde nicht (allein) entfernt");
	}
	if (entries != NULL) {
		g_ptr_array_unref (entries);
	}

	/* 4. Eintrag extrahieren */
	out_dir = g_file_new_for_path (out_path);
	list = g_list_append (NULL, (gpointer) "b.txt");
	nolphin_archive_extract_entries_async (archive, list, out_dir, NULL, on_ready, NULL);
	finish_ok ("extrahieren");
	g_list_free (list);
	{
		gchar *contents = NULL, *extracted = g_build_filename (out_path, "b.txt", NULL);

		if (!g_file_get_contents (extracted, &contents, NULL, NULL) || g_strcmp0 (contents, "Beta\n") != 0) {
			fail ("extrahierte b.txt hat falschen Inhalt");
		}
		g_free (contents);
		g_free (extracted);
	}

	cmd = g_strdup_printf ("rm -rf '%s'", dir);
	if (system (cmd) != 0) {
		g_printerr ("Hinweis: Aufräumen von %s fehlgeschlagen\n", dir);
	}
	g_free (cmd);
	return exit_code;
}
