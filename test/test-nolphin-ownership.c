/* Exercises nolphin_file_set_group_recursive()/nolphin_file_set_owner_
 * recursive() (libnolphin-private/nolphin-file-operations.c). Two real
 * cases:
 *
 * 1. Recursive GROUP change to a group this test's own user is a
 *    member of - standard POSIX chgrp semantics allow this without
 *    root, so it's expected to genuinely succeed, and the test checks
 *    the GID actually changed on both a top-level and a nested file.
 *
 * 2. Recursive OWNER change to uid 0 (root) - expected to fail, since
 *    this test doesn't run as root. The point of this case is
 *    specifically to catch a "fake success" regression: the job must
 *    report failure via its callback, and the file's uid must be
 *    unchanged, not silently report success while doing nothing.
 *
 * If no group other than the primary one is available, the group case
 * is skipped (not failed) with a clear message - that's an
 * environment limitation, not a code problem. */

#include <gtk/gtk.h>
#include <libnolphin-private/nolphin-file-operations.h>
#include <grp.h>
#include <unistd.h>
#include <string.h>

static gint exit_code = 0;
static GFile *work_dir;
static GFile *top_file;
static GFile *nested_file;
static guint32 original_uid;
static guint32 target_gid;

static void
fail (const gchar *message)
{
    g_printerr ("FAIL: %s\n", message);
    exit_code = 1;
    gtk_main_quit ();
}

static guint32
query_gid (GFile *file)
{
    GFileInfo *info = g_file_query_info (file, G_FILE_ATTRIBUTE_UNIX_GID,
                                         G_FILE_QUERY_INFO_NONE, NULL, NULL);
    guint32 gid = (guint32) -1;
    if (info != NULL) {
        gid = g_file_info_get_attribute_uint32 (info, G_FILE_ATTRIBUTE_UNIX_GID);
        g_object_unref (info);
    }
    return gid;
}

static guint32
query_uid (GFile *file)
{
    GFileInfo *info = g_file_query_info (file, G_FILE_ATTRIBUTE_UNIX_UID,
                                         G_FILE_QUERY_INFO_NONE, NULL, NULL);
    guint32 uid = (guint32) -1;
    if (info != NULL) {
        uid = g_file_info_get_attribute_uint32 (info, G_FILE_ATTRIBUTE_UNIX_UID);
        g_object_unref (info);
    }
    return uid;
}

/* ---- owner (expected to fail: not root) ---- */

static void
on_owner_done (gboolean success, gpointer user_data)
{
    if (success) {
        fail ("recursive chown to root reported SUCCESS while running as a normal user - fake success!");
        return;
    }

    if (query_uid (top_file) != original_uid) {
        fail ("recursive chown to root changed the uid despite reporting failure");
        return;
    }

    g_print ("PASS: recursive owner change correctly failed (not root) and left uid unchanged\n");
    gtk_main_quit ();
}

static void
check_owner_recursive_fails_without_root (void)
{
    gchar *uri = g_file_get_uri (work_dir);

    original_uid = query_uid (top_file);

    nolphin_file_set_owner_recursive (uri, 0 /* root */, on_owner_done, NULL);
    g_free (uri);

    gtk_main ();
}

/* ---- group (expected to succeed: chgrp to a group we belong to) ---- */

static void
on_group_done (gboolean success, gpointer user_data)
{
    if (!success) {
        fail ("recursive group change to a group this user belongs to reported failure");
        return;
    }

    if (query_gid (top_file) != target_gid) {
        fail ("top-level file's gid did not actually change");
        return;
    }
    if (query_gid (nested_file) != target_gid) {
        fail ("nested file's gid did not actually change - recursion didn't reach it");
        return;
    }

    g_print ("PASS: recursive group change succeeded and reached both the top-level and a nested file\n");
    gtk_main_quit ();
}

static gboolean
find_alternate_group (guint32 *out_gid)
{
    gid_t groups[64];
    int n = getgroups (64, groups);
    gid_t primary = getegid ();
    int i;

    if (n <= 0) {
        return FALSE;
    }

    for (i = 0; i < n; i++) {
        if (groups[i] != primary) {
            *out_gid = (guint32) groups[i];
            return TRUE;
        }
    }
    return FALSE;
}

static void
check_group_recursive (void)
{
    gchar *uri;

    if (!find_alternate_group (&target_gid)) {
        g_print ("SKIP: this user belongs to only one group, nothing to chgrp to for a real test\n");
        return;
    }

    uri = g_file_get_uri (work_dir);
    nolphin_file_set_group_recursive (uri, target_gid, on_group_done, NULL);
    g_free (uri);

    gtk_main ();
}

int
main (int argc, char **argv)
{
    gchar *tmpl, *top_path, *subdir_path, *nested_path;
    GError *error = NULL;
    GFile *subdir;

    gtk_init (&argc, &argv);

    tmpl = g_dir_make_tmp ("nolphin-ownership-test-XXXXXX", &error);
    if (tmpl == NULL) {
        g_printerr ("FAIL: could not create temp dir: %s\n", error->message);
        return 1;
    }
    work_dir = g_file_new_for_path (tmpl);

    top_path = g_build_filename (tmpl, "top.txt", NULL);
    subdir_path = g_build_filename (tmpl, "subdir", NULL);
    nested_path = g_build_filename (subdir_path, "nested.txt", NULL);
    g_free (tmpl);

    if (!g_file_set_contents (top_path, "top", -1, &error)) {
        g_printerr ("FAIL: could not write top.txt: %s\n", error->message);
        return 1;
    }
    subdir = g_file_new_for_path (subdir_path);
    if (!g_file_make_directory (subdir, NULL, &error)) {
        g_printerr ("FAIL: could not create subdir: %s\n", error->message);
        return 1;
    }
    g_object_unref (subdir);
    if (!g_file_set_contents (nested_path, "nested", -1, &error)) {
        g_printerr ("FAIL: could not write nested.txt: %s\n", error->message);
        return 1;
    }

    top_file = g_file_new_for_path (top_path);
    nested_file = g_file_new_for_path (nested_path);
    g_free (top_path);
    g_free (subdir_path);
    g_free (nested_path);

    check_owner_recursive_fails_without_root ();
    if (exit_code == 0) check_group_recursive ();

    g_object_unref (top_file);
    g_object_unref (nested_file);
    g_object_unref (work_dir);

    return exit_code;
}
