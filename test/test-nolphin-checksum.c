/* Exercises nolphin_checksum_compute_async()/_finish() (libnolphin-
 * private/nolphin-checksum.c). Writes a real file with known content
 * ("hello world", no trailing newline) and checks the computed MD5/
 * SHA-256/BLAKE2 digests against independently known-correct values
 * (the same ones `printf '%s' "hello world" | md5sum` etc. produce),
 * plus nolphin_checksum_matches() case/whitespace handling. */

#include <gtk/gtk.h>
#include <libnolphin-private/nolphin-checksum.h>
#include <string.h>

static gint exit_code = 0;
static GFile *test_file;

static void
fail (const gchar *message)
{
	g_printerr ("FAIL: %s\n", message);
	exit_code = 1;
}

static GAsyncResult *pending_result;

static void
on_checksum_ready (GObject *source, GAsyncResult *result, gpointer user_data)
{
	pending_result = g_object_ref (result);
	gtk_main_quit ();
}

static void
check_type (NolphinChecksumType type, const char *expected_hex, const char *label)
{
	gchar *digest;
	GError *error = NULL;

	if (!nolphin_checksum_type_is_available (type)) {
		g_print ("SKIP: %s not available on this system (tool missing)\n", label);
		return;
	}

	pending_result = NULL;
	nolphin_checksum_compute_async (test_file, type, NULL, on_checksum_ready, NULL);
	gtk_main ();

	digest = nolphin_checksum_compute_finish (pending_result, &error);
	g_object_unref (pending_result);

	if (digest == NULL) {
		gchar *msg = g_strdup_printf ("%s: computation failed: %s", label,
					      error ? error->message : "(no error set)");
		fail (msg);
		g_free (msg);
		g_clear_error (&error);
		return;
	}

	if (g_ascii_strcasecmp (digest, expected_hex) != 0) {
		gchar *msg = g_strdup_printf ("%s: expected '%s', got '%s'", label, expected_hex, digest);
		fail (msg);
		g_free (msg);
	} else {
		g_print ("PASS: %s digest matches known-correct value\n", label);
	}

	if (!nolphin_checksum_matches (digest, expected_hex)) {
		fail ("nolphin_checksum_matches() rejected the correct digest");
	}
	{
		gchar *padded = g_strdup_printf ("  %s  \n", expected_hex);
		gchar *upper = g_ascii_strup (padded, -1);
		if (!nolphin_checksum_matches (digest, upper)) {
			fail ("nolphin_checksum_matches() should ignore case and surrounding whitespace");
		}
		g_free (padded);
		g_free (upper);
	}
	if (nolphin_checksum_matches (digest, "not-the-right-digest")) {
		fail ("nolphin_checksum_matches() accepted a wrong digest");
	}

	g_free (digest);
}

int
main (int argc, char **argv)
{
	gchar *tmpl, *path;
	GError *error = NULL;

	gtk_init (&argc, &argv);

	tmpl = g_dir_make_tmp ("nolphin-checksum-test-XXXXXX", &error);
	if (tmpl == NULL) {
		g_printerr ("FAIL: could not create temp dir: %s\n", error->message);
		return 1;
	}
	path = g_build_filename (tmpl, "hello.txt", NULL);
	g_free (tmpl);

	if (!g_file_set_contents (path, "hello world", -1, &error)) {
		g_printerr ("FAIL: could not write test file: %s\n", error->message);
		return 1;
	}

	test_file = g_file_new_for_path (path);
	g_free (path);

	check_type (NOLPHIN_CHECKSUM_MD5,
		   "5eb63bbbe01eeed093cb22bb8f5acdc3", "MD5");
	check_type (NOLPHIN_CHECKSUM_SHA256,
		   "b94d27b9934d3e08a52e52d7da7dabfac484efe37a5380ee9088f7ace2efcde9", "SHA-256");
	check_type (NOLPHIN_CHECKSUM_BLAKE2,
		   "021ced8799296ceca557832ab941a50b4a11f83478cf141f51f933f653ab9fbcc05a037cddbed06e309bf334942c4e58cdf1a46e237911ccd7fcf9787cbc7fd0",
		   "BLAKE2");

	g_object_unref (test_file);

	if (exit_code == 0) {
		g_print ("All checksum tests passed.\n");
	}

	return exit_code;
}
