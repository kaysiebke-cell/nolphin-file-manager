/* Exercises nolphin-cad.c against synthetic STL/STEP/FCStd files built
 * on the fly (no real sample files exist in this repo or on this
 * system). Each case is checked
 * with real assertions, not just "did it crash". Exits 0 if every
 * case passes, 1 on the first mismatch. */

#include <gtk/gtk.h>
#include <libnolphin-private/nolphin-cad.h>
#include <libnolphin-private/nolphin-archive.h>
#include <string.h>

static gint exit_code = 0;
static GFile *work_dir;
static GFile *fcstd_file;

static void
fail (const gchar *message)
{
    g_printerr ("FAIL: %s\n", message);
    exit_code = 1;
    gtk_main_quit ();
}

/* ---- binary STL ---- */

static gboolean
write_binary_stl (const gchar *path, GError **error)
{
    guint8 buf[84 + 50];
    guint32 count_le = GUINT32_TO_LE (1);
    gfloat coords[12] = { 0,0,1,  0,0,0,  1,0,0,  0,1,0 }; /* normal + 3 verts */
    guint16 attr = 0;

    memset (buf, 0, 80);
    memcpy (buf, "test binary stl", strlen ("test binary stl"));
    memcpy (buf + 80, &count_le, 4);
    memcpy (buf + 84, coords, sizeof (coords));
    memcpy (buf + 84 + 48, &attr, 2);

    return g_file_set_contents (path, (const gchar *) buf, sizeof (buf), error);
}

static void
on_binary_stl_done (GObject *source, GAsyncResult *result, gpointer user_data)
{
    GError *error = NULL;
    NolphinCadInfo *info = nolphin_cad_get_info_finish (result, &error);

    if (info == NULL) {
        gchar *msg = g_strdup_printf ("binary STL analysis failed: %s", error ? error->message : "?");
        g_clear_error (&error);
        fail (msg);
        g_free (msg);
        return;
    }

    if (!info->stl_is_binary) {
        fail ("binary.stl misclassified as ASCII");
    } else if (!info->stl_triangle_count_known || info->stl_triangle_count != 1) {
        fail ("binary.stl triangle count wrong (expected 1)");
    } else {
        g_print ("PASS: binary STL detected, is_binary=TRUE, triangle_count=1\n");
    }

    nolphin_cad_info_free (info);
    gtk_main_quit ();
}

static void
check_binary_stl (void)
{
    GFile *file;
    gchar *path;
    GError *error = NULL;

    path = g_build_filename (g_file_peek_path (work_dir), "binary.stl", NULL);
    if (!write_binary_stl (path, &error)) {
        gchar *msg = g_strdup_printf ("could not write binary.stl: %s", error->message);
        g_free (path);
        fail (msg);
        g_free (msg);
        return;
    }

    file = g_file_new_for_path (path);
    g_free (path);

    if (nolphin_cad_detect_format (file) != NOLPHIN_CAD_FORMAT_STL) {
        fail ("binary.stl not detected as STL");
        g_object_unref (file);
        return;
    }

    nolphin_cad_get_info_async (file, NULL, on_binary_stl_done, NULL);
    gtk_main ();

    g_object_unref (file);
}

/* ---- ASCII STL ---- */

static void
on_ascii_stl_done (GObject *source, GAsyncResult *result, gpointer user_data)
{
    GError *error = NULL;
    NolphinCadInfo *info = nolphin_cad_get_info_finish (result, &error);

    if (info == NULL) {
        gchar *msg = g_strdup_printf ("ASCII STL analysis failed: %s", error ? error->message : "?");
        g_clear_error (&error);
        fail (msg);
        g_free (msg);
        return;
    }

    if (info->stl_is_binary) {
        fail ("ascii.stl misclassified as binary");
    } else if (!info->stl_triangle_count_known || info->stl_triangle_count != 2) {
        gchar *msg = g_strdup_printf ("ascii.stl triangle count wrong: known=%d count=%lu (expected 2)",
                                      info->stl_triangle_count_known,
                                      (unsigned long) info->stl_triangle_count);
        fail (msg);
        g_free (msg);
    } else {
        g_print ("PASS: ASCII STL detected, is_binary=FALSE, triangle_count=2\n");
    }

    nolphin_cad_info_free (info);
    gtk_main_quit ();
}

static void
check_ascii_stl (void)
{
    GFile *file;
    gchar *path;
    GError *error = NULL;
    const gchar *content =
        "solid test\n"
        "  facet normal 0 0 1\n"
        "    outer loop\n"
        "      vertex 0 0 0\n"
        "      vertex 1 0 0\n"
        "      vertex 0 1 0\n"
        "    endloop\n"
        "  endfacet\n"
        "  facet normal 0 0 1\n"
        "    outer loop\n"
        "      vertex 1 1 0\n"
        "      vertex 1 0 0\n"
        "      vertex 0 1 0\n"
        "    endloop\n"
        "  endfacet\n"
        "endsolid test\n";

    path = g_build_filename (g_file_peek_path (work_dir), "ascii.stl", NULL);
    if (!g_file_set_contents (path, content, -1, &error)) {
        gchar *msg = g_strdup_printf ("could not write ascii.stl: %s", error->message);
        g_free (path);
        fail (msg);
        g_free (msg);
        return;
    }

    file = g_file_new_for_path (path);
    g_free (path);

    nolphin_cad_get_info_async (file, NULL, on_ascii_stl_done, NULL);
    gtk_main ();

    g_object_unref (file);
}

/* ---- STEP ---- */

static void
on_step_done (GObject *source, GAsyncResult *result, gpointer user_data)
{
    GError *error = NULL;
    NolphinCadInfo *info = nolphin_cad_get_info_finish (result, &error);

    if (info == NULL) {
        gchar *msg = g_strdup_printf ("STEP analysis failed: %s", error ? error->message : "?");
        g_clear_error (&error);
        fail (msg);
        g_free (msg);
        return;
    }

    if (g_strcmp0 (info->step_description, "A test part, nothing real") != 0) {
        fail ("STEP description mismatch");
    } else if (g_strcmp0 (info->step_file_name, "test.step") != 0) {
        fail ("STEP file_name mismatch");
    } else if (g_strcmp0 (info->step_timestamp, "2026-01-15T10:00:00") != 0) {
        fail ("STEP timestamp mismatch");
    } else if (g_strcmp0 (info->step_author, "Jane Tester") != 0) {
        fail ("STEP author mismatch");
    } else if (g_strcmp0 (info->step_schema, "CONFIG_CONTROL_DESIGN") != 0) {
        fail ("STEP schema mismatch");
    } else {
        g_print ("PASS: STEP header fields all extracted correctly\n");
    }

    nolphin_cad_info_free (info);
    gtk_main_quit ();
}

static void
check_step (void)
{
    GFile *file;
    gchar *path;
    GError *error = NULL;
    const gchar *content =
        "ISO-10303-21;\n"
        "HEADER;\n"
        "FILE_DESCRIPTION(('A test part, nothing real'),'2;1');\n"
        "FILE_NAME('test.step','2026-01-15T10:00:00',('Jane Tester'),('Test Org'),'','','');\n"
        "FILE_SCHEMA(('CONFIG_CONTROL_DESIGN'));\n"
        "ENDSEC;\n"
        "DATA;\n"
        "ENDSEC;\n"
        "END-ISO-10303-21;\n";

    path = g_build_filename (g_file_peek_path (work_dir), "part.step", NULL);
    if (!g_file_set_contents (path, content, -1, &error)) {
        gchar *msg = g_strdup_printf ("could not write part.step: %s", error->message);
        g_free (path);
        fail (msg);
        g_free (msg);
        return;
    }

    file = g_file_new_for_path (path);
    g_free (path);

    nolphin_cad_get_info_async (file, NULL, on_step_done, NULL);
    gtk_main ();

    g_object_unref (file);
}

/* ---- FCStd (a ZIP containing Document.xml) ---- */

static void
on_fcstd_cad_done (GObject *source, GAsyncResult *result, gpointer user_data)
{
    GError *error = NULL;
    NolphinCadInfo *info = nolphin_cad_get_info_finish (result, &error);

    if (info == NULL) {
        gchar *msg = g_strdup_printf ("FCStd analysis failed: %s", error ? error->message : "?");
        g_clear_error (&error);
        fail (msg);
        g_free (msg);
        return;
    }

    if (g_strcmp0 (info->fcstd_comment, "A test document") != 0) {
        fail ("FCStd Comment mismatch");
    } else if (g_strcmp0 (info->fcstd_author, "Jane Tester") != 0) {
        fail ("FCStd author (CreatedBy) mismatch");
    } else if (g_strcmp0 (info->fcstd_company, "Test Org") != 0) {
        fail ("FCStd Company mismatch");
    } else {
        g_print ("PASS: FCStd Document.xml metadata extracted correctly via libgsf\n");
    }

    nolphin_cad_info_free (info);
    gtk_main_quit ();
}

static void
on_fcstd_compress_done (GObject *source, GAsyncResult *result, gpointer user_data)
{
    GError *error = NULL;

    if (!nolphin_archive_compress_finish (result, &error)) {
        gchar *msg = g_strdup_printf ("could not build synthetic .FCStd (zip): %s", error->message);
        g_clear_error (&error);
        fail (msg);
        g_free (msg);
        return;
    }

    nolphin_cad_get_info_async (fcstd_file, NULL, on_fcstd_cad_done, NULL);
}

static void
check_fcstd (void)
{
    GFile *doc_xml_path;
    GError *error = NULL;
    GList *sources = NULL;
    gchar *doc_path;
    const gchar *content =
        "<?xml version='1.0' encoding='utf-8'?>\n"
        "<Document SchemaVersion=\"4\">\n"
        "  <Properties Count=\"3\">\n"
        "    <Property name=\"Comment\" type=\"App::PropertyString\">\n"
        "      <String value=\"A test document\"/>\n"
        "    </Property>\n"
        "    <Property name=\"CreatedBy\" type=\"App::PropertyString\">\n"
        "      <String value=\"Jane Tester\"/>\n"
        "    </Property>\n"
        "    <Property name=\"Company\" type=\"App::PropertyString\">\n"
        "      <String value=\"Test Org\"/>\n"
        "    </Property>\n"
        "  </Properties>\n"
        "</Document>\n";

    doc_path = g_build_filename (g_file_peek_path (work_dir), "Document.xml", NULL);
    if (!g_file_set_contents (doc_path, content, -1, &error)) {
        gchar *msg = g_strdup_printf ("could not write Document.xml: %s", error->message);
        g_free (doc_path);
        fail (msg);
        g_free (msg);
        return;
    }

    doc_xml_path = g_file_new_for_path (doc_path);
    g_free (doc_path);

    fcstd_file = g_file_get_child (work_dir, "model.fcstd");
    sources = g_list_append (sources, doc_xml_path);

    /* .FCStd is "just" a ZIP with Document.xml at its root - build one
     * with the archive backend already verified in the previous
     * commit, then feed it back through the CAD backend. */
    nolphin_archive_compress_async (sources, fcstd_file, NOLPHIN_ARCHIVE_FORMAT_ZIP,
                                    NULL, 0, NULL, on_fcstd_compress_done, NULL);
    gtk_main ();

    g_list_free (sources);
    g_object_unref (doc_xml_path);
    g_clear_object (&fcstd_file);
}

int
main (int argc, char **argv)
{
    gchar *tmpl;
    GError *error = NULL;

    gtk_init (&argc, &argv);

    tmpl = g_dir_make_tmp ("nolphin-cad-test-XXXXXX", &error);
    if (tmpl == NULL) {
        g_printerr ("FAIL: could not create temp dir: %s\n", error->message);
        return 1;
    }
    work_dir = g_file_new_for_path (tmpl);
    g_free (tmpl);

    check_binary_stl ();
    if (exit_code == 0) check_ascii_stl ();
    if (exit_code == 0) check_step ();
    if (exit_code == 0) check_fcstd ();

    g_object_unref (work_dir);

    return exit_code;
}
