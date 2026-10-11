/* Exercises nolphin-trash-cleanup.c (libnolphin-private) without ever
 * touching a real Trash: trash:// aggregates every mounted volume's
 * Trash for the current UID regardless of $HOME, so it cannot be
 * safely sandboxed per-process for a destructive test - confirmed
 * live (a fresh dbus-run-session with an overridden $HOME still
 * reported the real, non-empty Trash's item count).
 *
 * Instead this test covers:
 * - nolphin_trash_cleanup_is_expired(): the pure decision function,
 *   exhaustively, with synthetic ISO 8601 dates and a fixed "now" -
 *   this is where an off-by-one or timezone bug would actually hide.
 * - nolphin_trash_cleanup_get_total_size_async(): against a real,
 *   freshly created scratch directory (nested folders, known file
 *   sizes) instead of trash:/// - exercises the exact same recursive
 *   GIO enumeration code with no risk to any real data. */

#include <gtk/gtk.h>
#include <libnolphin-private/nolphin-trash-cleanup.h>
#include <string.h>

static gint exit_code = 0;
static GAsyncResult *pending_result;

static void
fail (const gchar *message)
{
	g_printerr ("FAIL: %s\n", message);
	exit_code = 1;
}

static void
on_ready (GObject *source, GAsyncResult *result, gpointer user_data)
{
	pending_result = g_object_ref (result);
	gtk_main_quit ();
}

int
main (int argc, char **argv)
{
	GDateTime *now;
	gchar *tmpl;
	GFile *scratch_dir, *sub_dir;
	GError *error = NULL;
	goffset total_size;

	gtk_init (&argc, &argv);

	/* --- is_expired: pure decision logic --- */

	now = g_date_time_new_from_iso8601 ("2026-09-25T12:00:00Z", NULL);
	g_assert (now != NULL);

	if (nolphin_trash_cleanup_is_expired ("2026-09-20T12:00:00Z", 10, now)) {
		fail ("is_expired: an item deleted 5 days ago must NOT be expired at a 10-day retention");
	} else {
		g_print ("PASS: is_expired correctly keeps a recent item under a longer retention\n");
	}

	if (!nolphin_trash_cleanup_is_expired ("2026-09-10T12:00:00Z", 10, now)) {
		fail ("is_expired: an item deleted 15 days ago MUST be expired at a 10-day retention");
	} else {
		g_print ("PASS: is_expired correctly expires an old item\n");
	}

	/* exactly on the boundary (15 days is default trash-time; use 15
	 * days ago at retention=15 - should count as expired, >=) */
	if (!nolphin_trash_cleanup_is_expired ("2026-09-10T12:00:00Z", 15, now)) {
		fail ("is_expired: an item exactly at the retention boundary must be treated as expired (>=)");
	} else {
		g_print ("PASS: is_expired treats the exact boundary as expired\n");
	}

	if (nolphin_trash_cleanup_is_expired ("2026-09-20T12:00:00Z", 0, now)) {
		fail ("is_expired: retention_days <= 0 must always return FALSE (auto-cleanup off)");
	} else {
		g_print ("PASS: is_expired never expires anything when retention_days is 0\n");
	}

	if (nolphin_trash_cleanup_is_expired (NULL, 10, now)) {
		fail ("is_expired: a missing deletion date must never count as expired");
	} else {
		g_print ("PASS: is_expired never expires an item with no deletion date\n");
	}

	if (nolphin_trash_cleanup_is_expired ("not-a-real-date", 10, now)) {
		fail ("is_expired: an unparseable date must never count as expired");
	} else {
		g_print ("PASS: is_expired never expires an item with an unparseable date\n");
	}

	g_date_time_unref (now);

	/* --- get_total_size_async: real recursive enumeration, on a plain scratch dir --- */

	tmpl = g_dir_make_tmp ("nolphin-trash-cleanup-test-XXXXXX", &error);
	if (tmpl == NULL) {
		g_printerr ("FAIL: could not create temp dir: %s\n", error->message);
		return 1;
	}

	{
		gchar *sub_path = g_build_filename (tmpl, "sub", NULL);
		gchar *file_a = g_build_filename (tmpl, "a.txt", NULL);
		gchar *file_b = g_build_filename (sub_path, "b.txt", NULL);

		g_mkdir_with_parents (sub_path, 0755);
		/* Exact byte counts, no trailing newline surprises. */
		g_file_set_contents (file_a, "12345", -1, NULL);      /* 5 bytes */
		g_file_set_contents (file_b, "1234567890", -1, NULL); /* 10 bytes */

		g_free (sub_path);
		g_free (file_a);
		g_free (file_b);
	}

	scratch_dir = g_file_new_for_path (tmpl);
	sub_dir = g_file_get_child (scratch_dir, "sub");
	(void) sub_dir;

	pending_result = NULL;
	nolphin_trash_cleanup_get_total_size_async (scratch_dir, NULL, on_ready, NULL);
	gtk_main ();
	total_size = nolphin_trash_cleanup_get_total_size_finish (pending_result, &error);
	g_object_unref (pending_result);

	if (error != NULL) {
		fail (error->message);
		g_clear_error (&error);
	} else if (total_size != 15) {
		gchar *msg = g_strdup_printf ("get_total_size returned %" G_GOFFSET_FORMAT ", expected 15 (5 + 10 bytes, recursing into 'sub/')", total_size);
		fail (msg);
		g_free (msg);
	} else {
		g_print ("PASS: get_total_size correctly sums files recursively across subdirectories\n");
	}

	g_object_unref (scratch_dir);
	g_free (tmpl);

	if (exit_code == 0) {
		g_print ("All trash-cleanup tests passed.\n");
	}

	return exit_code;
}
