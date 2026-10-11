/* Exercises nolphin_encryption_encrypt_async()/decrypt_async()
 * (libnolphin-private/nolphin-encryption.c). Real gpg subprocess,
 * real files: encrypts a file with a passphrase, verifies the
 * output is a real, different-looking binary blob (not a no-op
 * copy), decrypts it back and checks the content matches exactly,
 * then - the important negative case - tries decrypting with the
 * WRONG passphrase and checks that fails cleanly instead of
 * silently producing wrong/garbage output as if it had succeeded. */

#include <gtk/gtk.h>
#include <libnolphin-private/nolphin-encryption.h>
#include <string.h>

static gint exit_code = 0;
static GFile *work_dir;
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
	gchar *tmpl, *plain_path, *encrypted_path, *decrypted_path, *wrong_decrypt_path;
	GError *error = NULL;
	GFile *plain_file, *encrypted_file, *decrypted_file, *wrong_decrypt_file;
	gboolean ok;
	char *plain_contents, *decrypted_contents;
	gsize plain_len, decrypted_len;
	char *encrypted_contents;
	gsize encrypted_len;
	const char *original_text = "Das ist ein geheimer Testinhalt fuer Nolphin.\n";

	gtk_init (&argc, &argv);

	if (!nolphin_encryption_is_available ()) {
		g_print ("SKIP: gpg not installed on this system\n");
		return 0;
	}

	tmpl = g_dir_make_tmp ("nolphin-encryption-test-XXXXXX", &error);
	if (tmpl == NULL) {
		g_printerr ("FAIL: could not create temp dir: %s\n", error->message);
		return 1;
	}
	work_dir = g_file_new_for_path (tmpl);

	plain_path = g_build_filename (tmpl, "secret.txt", NULL);
	encrypted_path = g_build_filename (tmpl, "secret.txt.gpg", NULL);
	decrypted_path = g_build_filename (tmpl, "secret-decrypted.txt", NULL);
	wrong_decrypt_path = g_build_filename (tmpl, "should-not-exist.txt", NULL);
	g_free (tmpl);

	if (!g_file_set_contents (plain_path, original_text, -1, &error)) {
		g_printerr ("FAIL: could not write test file: %s\n", error->message);
		return 1;
	}

	plain_file = g_file_new_for_path (plain_path);
	encrypted_file = g_file_new_for_path (encrypted_path);
	decrypted_file = g_file_new_for_path (decrypted_path);
	wrong_decrypt_file = g_file_new_for_path (wrong_decrypt_path);

	/* --- encrypt --- */
	pending_result = NULL;
	nolphin_encryption_encrypt_async (plain_file, encrypted_file, "korrektes-passwort-42",
					   NULL, on_ready, NULL);
	gtk_main ();
	ok = nolphin_encryption_encrypt_finish (pending_result, &error);
	g_object_unref (pending_result);

	if (!ok) {
		fail (error ? error->message : "encrypt failed with no error set");
		g_clear_error (&error);
	} else {
		g_print ("PASS: encryption reported success\n");
	}

	if (!g_file_get_contents (plain_path, &plain_contents, &plain_len, NULL)) {
		fail ("could not re-read plaintext file");
		plain_contents = NULL;
		plain_len = 0;
	}
	if (g_file_get_contents (encrypted_path, &encrypted_contents, &encrypted_len, NULL)) {
		/* A real gpg output is a different, binary blob - not a
		 * plain copy of the input and not suspiciously identical
		 * in size to something trivial. This is the check that
		 * would catch a fake "success" that just copied the file. */
		if (encrypted_len == plain_len &&
		    memcmp (encrypted_contents, plain_contents, plain_len) == 0) {
			fail ("encrypted file is byte-identical to the plaintext - not actually encrypted");
		} else {
			g_print ("PASS: encrypted file differs from the plaintext (%" G_GSIZE_FORMAT " vs %" G_GSIZE_FORMAT " bytes)\n",
				 encrypted_len, plain_len);
		}
		g_free (encrypted_contents);
	} else {
		fail ("encrypted output file was not created");
	}

	/* --- decrypt with the correct passphrase --- */
	pending_result = NULL;
	nolphin_encryption_decrypt_async (encrypted_file, decrypted_file, "korrektes-passwort-42",
					   NULL, on_ready, NULL);
	gtk_main ();
	ok = nolphin_encryption_decrypt_finish (pending_result, &error);
	g_object_unref (pending_result);

	if (!ok) {
		fail (error ? error->message : "decrypt (correct passphrase) failed with no error set");
		g_clear_error (&error);
	} else if (g_file_get_contents (decrypted_path, &decrypted_contents, &decrypted_len, NULL)) {
		if (decrypted_len == plain_len && memcmp (decrypted_contents, plain_contents, plain_len) == 0) {
			g_print ("PASS: decrypted content matches the original exactly\n");
		} else {
			fail ("decrypted content does not match the original");
		}
		g_free (decrypted_contents);
	} else {
		fail ("decrypted output file was not created");
	}

	/* --- decrypt with the WRONG passphrase must fail, not silently
	 * produce garbage while reporting success --- */
	pending_result = NULL;
	nolphin_encryption_decrypt_async (encrypted_file, wrong_decrypt_file, "falsches-passwort",
					   NULL, on_ready, NULL);
	gtk_main ();
	ok = nolphin_encryption_decrypt_finish (pending_result, &error);
	g_object_unref (pending_result);

	if (ok) {
		fail ("decrypting with the WRONG passphrase reported success - fake success!");
	} else {
		g_print ("PASS: decrypting with the wrong passphrase correctly failed (%s)\n",
			 error ? error->message : "(no message)");
		g_clear_error (&error);
	}
	if (g_file_query_exists (wrong_decrypt_file, NULL)) {
		fail ("a (presumably garbage) output file was left behind after a failed decryption");
	}

	g_free (plain_contents);
	g_free (plain_path);
	g_free (encrypted_path);
	g_free (decrypted_path);
	g_free (wrong_decrypt_path);
	g_object_unref (plain_file);
	g_object_unref (encrypted_file);
	g_object_unref (decrypted_file);
	g_object_unref (wrong_decrypt_file);
	g_object_unref (work_dir);

	if (exit_code == 0) {
		g_print ("All encryption tests passed.\n");
	}

	return exit_code;
}
