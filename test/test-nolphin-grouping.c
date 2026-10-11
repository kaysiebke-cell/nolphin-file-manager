/* Exercises nolphin_file_get_group_key()/nolphin_file_compare_for_group()
 * (libnolphin-private/nolphin-file.c), the data-model backend for grouping
 * "Gruppierung nach Name, Typ, Datum, Groesse". Real files are created
 * in a temp directory with known names/sizes/mtimes, NolphinFile
 * objects for them are obtained through nolphin_file_get_by_uri(), and
 * both the returned group LABELS and the ORDER of the groups relative
 * to each other are checked against expected values - so a change
 * that silently breaks bucketing (e.g. swapped size thresholds, an
 * off-by-one in the date-day math) fails a real assertion instead of
 * just "looking plausible" in a live GUI nobody is watching. */

#include <gtk/gtk.h>
#include <glib/gstdio.h>
#include <libnolphin-private/nolphin-file.h>
#include <string.h>

static gint exit_code = 0;
static GFile *work_dir;

static void
fail (const gchar *message)
{
	g_printerr ("FAIL: %s\n", message);
	exit_code = 1;
}

static NolphinFile *
file_for_path (const char *path)
{
	GFile *gfile;
	char *uri;
	NolphinFile *file;

	gfile = g_file_new_for_path (path);
	uri = g_file_get_uri (gfile);
	file = nolphin_file_get_by_uri (uri);
	g_free (uri);
	g_object_unref (gfile);

	return file;
}

static void
ready_callback (NolphinFile *file, gpointer callback_data)
{
	gboolean *ready = callback_data;
	*ready = TRUE;
}

static void
wait_until_info_ready (NolphinFile *file)
{
	gboolean ready;
	int tries;

	if (nolphin_file_check_if_ready (file, NOLPHIN_FILE_ATTRIBUTES_FOR_ICON)) {
		return;
	}

	ready = FALSE;
	nolphin_file_call_when_ready (file, NOLPHIN_FILE_ATTRIBUTES_FOR_ICON,
				       ready_callback, &ready);

	for (tries = 0; tries < 500 && !ready; tries++) {
		while (gtk_events_pending ()) {
			gtk_main_iteration ();
		}
		g_usleep (10000);
	}

	nolphin_file_cancel_call_when_ready (file, ready_callback, &ready);
}

static void
check_name_grouping (void)
{
	char *alice_path, *zebra_path, *num_path, *dot_path;
	NolphinFile *alice, *zebra, *num, *dot;
	char *key;

	alice_path = g_build_filename (g_file_peek_path (work_dir), "Alice.txt", NULL);
	zebra_path = g_build_filename (g_file_peek_path (work_dir), "zebra.txt", NULL);
	num_path = g_build_filename (g_file_peek_path (work_dir), "42-report.txt", NULL);
	dot_path = g_build_filename (g_file_peek_path (work_dir), ".hidden-ish", NULL);

	g_file_set_contents (alice_path, "a", -1, NULL);
	g_file_set_contents (zebra_path, "z", -1, NULL);
	g_file_set_contents (num_path, "n", -1, NULL);
	g_file_set_contents (dot_path, "d", -1, NULL);

	alice = file_for_path (alice_path);
	zebra = file_for_path (zebra_path);
	num = file_for_path (num_path);
	dot = file_for_path (dot_path);

	wait_until_info_ready (alice);
	wait_until_info_ready (zebra);
	wait_until_info_ready (num);
	wait_until_info_ready (dot);

	key = nolphin_file_get_group_key (alice, NOLPHIN_FILE_SORT_BY_DISPLAY_NAME);
	if (g_strcmp0 (key, "A") != 0) {
		fail ("expected 'Alice.txt' to group under 'A'");
	}
	g_free (key);

	key = nolphin_file_get_group_key (zebra, NOLPHIN_FILE_SORT_BY_DISPLAY_NAME);
	if (g_strcmp0 (key, "Z") != 0) {
		fail ("expected 'zebra.txt' to group under 'Z' (case-insensitive)");
	}
	g_free (key);

	key = nolphin_file_get_group_key (num, NOLPHIN_FILE_SORT_BY_DISPLAY_NAME);
	if (g_strcmp0 (key, "0-9") != 0) {
		fail ("expected '42-report.txt' to group under '0-9'");
	}
	g_free (key);

	key = nolphin_file_get_group_key (dot, NOLPHIN_FILE_SORT_BY_DISPLAY_NAME);
	if (g_strcmp0 (key, "#") != 0) {
		fail ("expected '.hidden-ish' to group under '#'");
	}
	g_free (key);

	/* Group ORDER: '#' < '0-9' < letters, and within letters A < Z. */
	if (nolphin_file_compare_for_group (dot, num, NOLPHIN_FILE_SORT_BY_DISPLAY_NAME) >= 0) {
		fail ("'#' group should sort before '0-9' group");
	}
	if (nolphin_file_compare_for_group (num, alice, NOLPHIN_FILE_SORT_BY_DISPLAY_NAME) >= 0) {
		fail ("'0-9' group should sort before letter groups");
	}
	if (nolphin_file_compare_for_group (alice, zebra, NOLPHIN_FILE_SORT_BY_DISPLAY_NAME) >= 0) {
		fail ("'A' group should sort before 'Z' group");
	}

	g_free (alice_path);
	g_free (zebra_path);
	g_free (num_path);
	g_free (dot_path);
	nolphin_file_unref (alice);
	nolphin_file_unref (zebra);
	nolphin_file_unref (num);
	nolphin_file_unref (dot);
}

static void
check_size_grouping (void)
{
	char *tiny_path, *big_path, *dir_path;
	NolphinFile *tiny, *big, *dir;
	char *key;
	char *filler;

	tiny_path = g_build_filename (g_file_peek_path (work_dir), "tiny.txt", NULL);
	big_path = g_build_filename (g_file_peek_path (work_dir), "big.bin", NULL);
	dir_path = g_build_filename (g_file_peek_path (work_dir), "a-folder", NULL);

	g_file_set_contents (tiny_path, "just a few bytes", -1, NULL);

	/* 200 KB - lands in the "Small (under 10 MB)" bucket, clear of the
	 * 100 KB tiny/small boundary. */
	filler = g_strnfill (200 * 1024, 'x');
	g_file_set_contents (big_path, filler, 200 * 1024, NULL);
	g_free (filler);

	g_mkdir (dir_path, 0755);

	tiny = file_for_path (tiny_path);
	big = file_for_path (big_path);
	dir = file_for_path (dir_path);

	wait_until_info_ready (tiny);
	wait_until_info_ready (big);
	wait_until_info_ready (dir);

	key = nolphin_file_get_group_key (dir, NOLPHIN_FILE_SORT_BY_SIZE);
	if (g_strcmp0 (key, "Ordner") != 0) {
		fail ("expected a directory to group under 'Folders' for size grouping");
	}
	g_free (key);

	key = nolphin_file_get_group_key (tiny, NOLPHIN_FILE_SORT_BY_SIZE);
	if (strstr (key, "Winzig") == NULL) {
		fail ("expected a 17-byte file to land in the 'Tiny' size bucket");
	}
	g_free (key);

	key = nolphin_file_get_group_key (big, NOLPHIN_FILE_SORT_BY_SIZE);
	if (strstr (key, "Klein") == NULL) {
		fail ("expected a 200 KB file to land in the 'Small' size bucket");
	}
	g_free (key);

	/* Folders first, then ascending by size. */
	if (nolphin_file_compare_for_group (dir, tiny, NOLPHIN_FILE_SORT_BY_SIZE) >= 0) {
		fail ("'Folders' group should sort before file-size groups");
	}
	if (nolphin_file_compare_for_group (tiny, big, NOLPHIN_FILE_SORT_BY_SIZE) >= 0) {
		fail ("'Tiny' group should sort before 'Small' group");
	}

	g_free (tiny_path);
	g_free (big_path);
	g_free (dir_path);
	nolphin_file_unref (tiny);
	nolphin_file_unref (big);
	nolphin_file_unref (dir);
}

static void
check_type_grouping (void)
{
	char *dir_path, *file_path;
	NolphinFile *dir, *file;
	char *dir_key, *file_key;

	dir_path = g_build_filename (g_file_peek_path (work_dir), "another-folder", NULL);
	file_path = g_build_filename (g_file_peek_path (work_dir), "plain.txt", NULL);

	g_mkdir (dir_path, 0755);
	g_file_set_contents (file_path, "text", -1, NULL);

	dir = file_for_path (dir_path);
	file = file_for_path (file_path);

	wait_until_info_ready (dir);
	wait_until_info_ready (file);

	dir_key = nolphin_file_get_group_key (dir, NOLPHIN_FILE_SORT_BY_TYPE);
	if (g_strcmp0 (dir_key, "Ordner") != 0) {
		fail ("expected a directory to group under 'Folders' for type grouping");
	}

	file_key = nolphin_file_get_group_key (file, NOLPHIN_FILE_SORT_BY_TYPE);
	if (file_key == NULL || file_key[0] == '\0') {
		fail ("expected a non-empty type label for a plain file");
	}
	if (g_strcmp0 (file_key, dir_key) == 0) {
		fail ("a plain file should not share the 'Folders' type-group label");
	}

	if (nolphin_file_compare_for_group (dir, file, NOLPHIN_FILE_SORT_BY_TYPE) >= 0) {
		fail ("'Folders' type group should sort before other type groups");
	}

	g_free (dir_key);
	g_free (file_key);
	g_free (dir_path);
	g_free (file_path);
	nolphin_file_unref (dir);
	nolphin_file_unref (file);
}

static void
check_mtime_grouping (void)
{
	char *today_path;
	NolphinFile *today;
	char *key;

	today_path = g_build_filename (g_file_peek_path (work_dir), "just-written.txt", NULL);
	g_file_set_contents (today_path, "fresh", -1, NULL);

	today = file_for_path (today_path);
	wait_until_info_ready (today);

	key = nolphin_file_get_group_key (today, NOLPHIN_FILE_SORT_BY_MTIME);
	if (g_strcmp0 (key, "Heute") != 0) {
		fail ("expected a just-written file to group under 'Today'");
	}
	g_free (key);

	g_free (today_path);
	nolphin_file_unref (today);
}

static void
check_invalid_group_type_rejected (void)
{
	if (nolphin_file_sort_type_is_valid_group_type (NOLPHIN_FILE_SORT_BY_EXTENSION)) {
		fail ("NOLPHIN_FILE_SORT_BY_EXTENSION is not one of the four supported grouping criteria");
	}
	if (!nolphin_file_sort_type_is_valid_group_type (NOLPHIN_FILE_SORT_BY_DISPLAY_NAME)) {
		fail ("NOLPHIN_FILE_SORT_BY_DISPLAY_NAME must be a valid grouping criterion");
	}
}

int
main (int argc, char **argv)
{
	gchar *tmpl;
	GError *error = NULL;

	gtk_init (&argc, &argv);

	tmpl = g_dir_make_tmp ("nolphin-grouping-test-XXXXXX", &error);
	if (tmpl == NULL) {
		g_printerr ("FAIL: could not create temp dir: %s\n", error->message);
		return 1;
	}
	work_dir = g_file_new_for_path (tmpl);
	g_free (tmpl);

	check_name_grouping ();
	check_size_grouping ();
	check_type_grouping ();
	check_mtime_grouping ();
	check_invalid_group_type_rejected ();

	if (exit_code == 0) {
		g_print ("PASS: name/size/type/mtime group keys and group ordering all match expectations\n");
	}

	g_object_unref (work_dir);

	return exit_code;
}
