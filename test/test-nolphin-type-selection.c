/* Exercises nolphin_file_matches_type_category() and
 * nolphin_directory_match_type_category() (libnolphin-private/
 * nolphin-file.c and nolphin-directory.c), the backend for selection by file type
 * "Auswahl nach Dateityp". A real temp directory is populated with
 * one file per category (a folder, a PNG-named file, an MP4-named
 * file, an MP3-named file, a plain .txt file, a .zip file) plus one
 * file that should fall into OTHER (a .bin), then the directory is
 * actually read via nolphin_directory_get_by_uri()+file_monitor, and
 * a category match is checked against the real, live file listing -
 * not just the per-file predicate in isolation - so a bug in either
 * layer (mime-prefix check or the directory-level filter loop) would
 * fail a real assertion. */

#include <gtk/gtk.h>
#include <glib/gstdio.h>
#include <libnolphin-private/nolphin-file.h>
#include <libnolphin-private/nolphin-directory.h>
#include <string.h>

static gint exit_code = 0;
static GFile *work_dir;
static NolphinDirectory *directory;
static gboolean directory_ready;

static void
fail (const gchar *message)
{
	g_printerr ("FAIL: %s\n", message);
	exit_code = 1;
}

static void
files_added_cb (NolphinDirectory *dir, GList *added_files, gpointer user_data)
{
	/* The initial batch is what we're waiting for; later calls (there
	 * shouldn't be any once the temp dir is fully populated up front)
	 * would just mean the list keeps growing, which is fine too. */
	directory_ready = TRUE;
}

static void
wait_for_directory_load (void)
{
	int tries;

	for (tries = 0; tries < 500 && !directory_ready; tries++) {
		while (gtk_events_pending ()) {
			gtk_main_iteration ();
		}
		g_usleep (10000);
	}
}

static gboolean
list_contains_name (GList *list, const char *name)
{
	GList *l;

	for (l = list; l != NULL; l = l->next) {
		NolphinFile *file = NOLPHIN_FILE (l->data);
		char *display_name = nolphin_file_get_display_name (file);
		gboolean match = (g_strcmp0 (display_name, name) == 0);
		g_free (display_name);
		if (match) {
			return TRUE;
		}
	}
	return FALSE;
}

static void
check_category (NolphinFileTypeCategory category, const char *expected_name, guint expected_count)
{
	GList *matches;
	guint n;

	matches = nolphin_directory_match_type_category (directory, category);
	n = g_list_length (matches);

	if (n != expected_count) {
		char *msg = g_strdup_printf ("category '%s': expected %u match(es), got %u",
					     nolphin_file_type_category_get_label (category),
					     expected_count, n);
		fail (msg);
		g_free (msg);
	} else if (expected_count == 1 && !list_contains_name (matches, expected_name)) {
		char *msg = g_strdup_printf ("category '%s': match list doesn't contain expected file '%s'",
					     nolphin_file_type_category_get_label (category), expected_name);
		fail (msg);
		g_free (msg);
	}

	nolphin_file_list_free (matches);
}

int
main (int argc, char **argv)
{
	gchar *tmpl;
	GError *error = NULL;
	char *uri;
	char *path;

	gtk_init (&argc, &argv);

	tmpl = g_dir_make_tmp ("nolphin-type-selection-test-XXXXXX", &error);
	if (tmpl == NULL) {
		g_printerr ("FAIL: could not create temp dir: %s\n", error->message);
		return 1;
	}
	work_dir = g_file_new_for_path (tmpl);

	/* One representative file per category, plus one that should
	 * only match OTHER. Content doesn't matter - detection here is
	 * by mime type (from the filename extension via GIO's sniffing/
	 * fallback) and, for the archive case, by the same extension
	 * table nolphin-archive.c already uses. */
	path = g_build_filename (tmpl, "a-folder", NULL);
	g_mkdir (path, 0755);
	g_free (path);

	path = g_build_filename (tmpl, "photo.png", NULL);
	g_file_set_contents (path, "not a real png, only the name matters here", -1, NULL);
	g_free (path);

	path = g_build_filename (tmpl, "movie.mp4", NULL);
	g_file_set_contents (path, "x", -1, NULL);
	g_free (path);

	path = g_build_filename (tmpl, "song.mp3", NULL);
	g_file_set_contents (path, "x", -1, NULL);
	g_free (path);

	path = g_build_filename (tmpl, "notes.txt", NULL);
	g_file_set_contents (path, "plain text", -1, NULL);
	g_free (path);

	path = g_build_filename (tmpl, "bundle.zip", NULL);
	g_file_set_contents (path, "x", -1, NULL);
	g_free (path);

	path = g_build_filename (tmpl, "data.bin", NULL);
	g_file_set_contents (path, "\x01\x02\x03", 3, NULL);
	g_free (path);

	g_free (tmpl);

	uri = g_file_get_uri (work_dir);
	directory = nolphin_directory_get_by_uri (uri);
	g_free (uri);

	g_signal_connect (directory, "files_added", G_CALLBACK (files_added_cb), NULL);
	nolphin_directory_file_monitor_add (directory, &directory_ready, TRUE,
					     NOLPHIN_FILE_ATTRIBUTES_FOR_ICON, NULL, NULL);

	wait_for_directory_load ();

	if (!directory_ready) {
		fail ("directory never finished loading - can't verify anything");
	} else {
		check_category (NOLPHIN_FILE_TYPE_CATEGORY_FOLDER, "a-folder", 1);
		check_category (NOLPHIN_FILE_TYPE_CATEGORY_IMAGE, "photo.png", 1);
		check_category (NOLPHIN_FILE_TYPE_CATEGORY_VIDEO, "movie.mp4", 1);
		check_category (NOLPHIN_FILE_TYPE_CATEGORY_AUDIO, "song.mp3", 1);
		check_category (NOLPHIN_FILE_TYPE_CATEGORY_TEXT, "notes.txt", 1);
		check_category (NOLPHIN_FILE_TYPE_CATEGORY_ARCHIVE, "bundle.zip", 1);
		check_category (NOLPHIN_FILE_TYPE_CATEGORY_OTHER, "data.bin", 1);
	}

	if (exit_code == 0) {
		g_print ("PASS: all seven type categories matched exactly the expected file in a real directory listing\n");
	}

	nolphin_directory_file_monitor_remove (directory, &directory_ready);
	nolphin_directory_unref (directory);
	g_object_unref (work_dir);

	return exit_code;
}
