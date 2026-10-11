/* Exercises the nolphin-archive.c backend end to end: creates two
 * source files, compresses them into a ZIP, extracts that ZIP into a
 * fresh directory, verifies the extracted content matches byte for
 * byte, then runs the "test archive" operation and checks it reports
 * success. Exits 0 on success, 1 on the first thing that doesn't
 * match, mirroring test-copy.c's gtk_main()-driven async style. */

#include <gtk/gtk.h>
#include <libnolphin-private/nolphin-archive.h>
#include <string.h>

static gint exit_code = 0;
static GFile *work_dir;
static GFile *source_dir;
static GFile *extract_dir;
static GFile *archive_file;

static const gchar *file_a_name = "alpha.txt";
static const gchar *file_a_content = "alpha contents";
static const gchar *file_b_name = "beta.txt";
static const gchar *file_b_content = "beta contents, a little longer";

static void
fail (const gchar *message)
{
    g_printerr ("FAIL: %s\n", message);
    exit_code = 1;
    gtk_main_quit ();
}

static gboolean
verify_extracted_file (const gchar *name, const gchar *expected_content)
{
    GFile *extracted;
    gchar *contents = NULL;
    gsize length = 0;
    GError *error = NULL;
    gboolean ok;

    extracted = g_file_get_child (extract_dir, name);
    ok = g_file_load_contents (extracted, NULL, &contents, &length, NULL, &error);
    g_object_unref (extracted);

    if (!ok) {
        g_printerr ("could not read back '%s': %s\n", name, error ? error->message : "?");
        g_clear_error (&error);
        return FALSE;
    }

    ok = (length == strlen (expected_content) && memcmp (contents, expected_content, length) == 0);
    if (!ok) {
        g_printerr ("content mismatch for '%s'\n", name);
    }
    g_free (contents);
    return ok;
}

static void
on_test_done (GObject *source, GAsyncResult *result, gpointer user_data)
{
    GError *error = NULL;

    if (!nolphin_archive_test_finish (result, &error)) {
        gchar *msg = g_strdup_printf ("archive test reported failure: %s", error->message);
        g_clear_error (&error);
        fail (msg);
        g_free (msg);
        return;
    }

    g_print ("PASS: create ZIP, extract, verify contents, test archive - all OK\n");
    gtk_main_quit ();
}

static void
on_extract_done (GObject *source, GAsyncResult *result, gpointer user_data)
{
    GError *error = NULL;

    if (!nolphin_archive_extract_finish (result, &error)) {
        gchar *msg = g_strdup_printf ("extract failed: %s", error->message);
        g_clear_error (&error);
        fail (msg);
        g_free (msg);
        return;
    }

    if (!verify_extracted_file (file_a_name, file_a_content)) {
        fail ("extracted alpha.txt does not match");
        return;
    }
    if (!verify_extracted_file (file_b_name, file_b_content)) {
        fail ("extracted beta.txt does not match");
        return;
    }

    nolphin_archive_test_async (archive_file, NULL, on_test_done, NULL);
}

static void
on_compress_done (GObject *source, GAsyncResult *result, gpointer user_data)
{
    GError *error = NULL;

    if (!nolphin_archive_compress_finish (result, &error)) {
        gchar *msg = g_strdup_printf ("compress failed: %s", error->message);
        g_clear_error (&error);
        fail (msg);
        g_free (msg);
        return;
    }

    if (!g_file_query_exists (archive_file, NULL)) {
        fail ("archive file was not created");
        return;
    }

    if (!g_file_make_directory (extract_dir, NULL, &error)) {
        gchar *msg = g_strdup_printf ("could not create extract dir: %s", error->message);
        g_clear_error (&error);
        fail (msg);
        g_free (msg);
        return;
    }

    nolphin_archive_extract_async (archive_file, extract_dir, NULL, on_extract_done, NULL);
}

int
main (int argc, char **argv)
{
    gchar *tmpl;
    GError *error = NULL;
    GFile *file_a, *file_b;
    GList *sources = NULL;

    gtk_init (&argc, &argv);

    if (!nolphin_archive_format_is_available (NOLPHIN_ARCHIVE_FORMAT_ZIP)) {
        g_print ("SKIP: 'zip'/'unzip' not installed on this system, cannot exercise the ZIP backend\n");
        return 0;
    }

    tmpl = g_dir_make_tmp ("nolphin-archive-test-XXXXXX", &error);
    if (tmpl == NULL) {
        g_printerr ("FAIL: could not create temp dir: %s\n", error->message);
        return 1;
    }
    work_dir = g_file_new_for_path (tmpl);
    g_free (tmpl);

    source_dir = g_file_get_child (work_dir, "src");
    extract_dir = g_file_get_child (work_dir, "extracted");
    archive_file = g_file_get_child (work_dir, "test.zip");

    if (!g_file_make_directory (source_dir, NULL, &error)) {
        g_printerr ("FAIL: could not create source dir: %s\n", error->message);
        return 1;
    }

    file_a = g_file_get_child (source_dir, file_a_name);
    file_b = g_file_get_child (source_dir, file_b_name);

    {
        gchar *path_a = g_file_get_path (file_a);
        gchar *path_b = g_file_get_path (file_b);
        gboolean wrote_a = g_file_set_contents (path_a, file_a_content, -1, &error);
        gboolean wrote_b = wrote_a && g_file_set_contents (path_b, file_b_content, -1, &error);
        g_free (path_a);
        g_free (path_b);

        if (!wrote_a || !wrote_b) {
            g_printerr ("FAIL: could not write source file: %s\n", error->message);
            return 1;
        }
    }

    sources = g_list_append (sources, file_a);
    sources = g_list_append (sources, file_b);

    nolphin_archive_compress_async (sources, archive_file, NOLPHIN_ARCHIVE_FORMAT_ZIP,
                                    NULL, 0, NULL, on_compress_done, NULL);

    gtk_main ();

    g_list_free (sources);
    g_object_unref (file_a);
    g_object_unref (file_b);
    g_object_unref (archive_file);
    g_object_unref (extract_dir);
    g_object_unref (source_dir);
    g_object_unref (work_dir);

    return exit_code;
}
