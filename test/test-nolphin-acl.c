/* Exercises nolphin_acl_get_entries_async()/set_entry_async()/
 * remove_entry_async() (libnolphin-private/nolphin-acl.c) against a
 * real file on a real ACL-capable filesystem: reads the plain base
 * ACL first, adds a named-user entry (using the test's own current
 * group, since a real system always has at least that one extra
 * qualifier available without needing root), verifies it shows up
 * with the right permission bits, then removes it and verifies it's
 * gone again - reading getfacl's ACTUAL output each time rather than
 * assuming the operation worked. */

#include <gtk/gtk.h>
#include <libnolphin-private/nolphin-acl.h>
#include <grp.h>
#include <unistd.h>
#include <string.h>

static gint exit_code = 0;
static GFile *test_file;
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

static NolphinAclEntry *
find_group_entry (GList *entries, const gchar *groupname)
{
	GList *l;
	for (l = entries; l != NULL; l = l->next) {
		NolphinAclEntry *e = l->data;
		if (e->type == NOLPHIN_ACL_ENTRY_GROUP &&
		    g_strcmp0 (e->qualifier, groupname) == 0) {
			return e;
		}
	}
	return NULL;
}

int
main (int argc, char **argv)
{
	gchar *tmpl, *path, *groupname;
	GError *error = NULL;
	GList *entries;
	gid_t groups[64];
	int n_groups;
	gid_t target_gid = 0;
	gboolean have_extra_group = FALSE;
	struct group *grp;

	gtk_init (&argc, &argv);

	if (!nolphin_acl_is_available ()) {
		g_print ("SKIP: getfacl/setfacl not installed on this system\n");
		return 0;
	}

	/* Need a second group besides our primary one to use as the
	 * named-group qualifier - same approach as
	 * test-nolphin-ownership.c's group case. */
	n_groups = getgroups (64, groups);
	if (n_groups > 0) {
		gid_t primary = getegid ();
		int i;
		for (i = 0; i < n_groups; i++) {
			if (groups[i] != primary) {
				target_gid = groups[i];
				have_extra_group = TRUE;
				break;
			}
		}
	}
	if (!have_extra_group) {
		g_print ("SKIP: this user belongs to only one group, nothing to use as a named-group ACL entry\n");
		return 0;
	}

	grp = getgrgid (target_gid);
	if (grp == NULL) {
		g_print ("SKIP: could not resolve group name for gid %d\n", (int) target_gid);
		return 0;
	}
	groupname = g_strdup (grp->gr_name);

	tmpl = g_dir_make_tmp ("nolphin-acl-test-XXXXXX", &error);
	if (tmpl == NULL) {
		g_printerr ("FAIL: could not create temp dir: %s\n", error->message);
		return 1;
	}
	path = g_build_filename (tmpl, "acl-datei.txt", NULL);
	g_free (tmpl);

	if (!g_file_set_contents (path, "acl test content", -1, &error)) {
		g_printerr ("FAIL: could not write test file: %s\n", error->message);
		return 1;
	}

	test_file = g_file_new_for_path (path);
	g_free (path);

	/* --- baseline: no named-group entry yet --- */
	pending_result = NULL;
	nolphin_acl_get_entries_async (test_file, NULL, on_ready, NULL);
	gtk_main ();
	entries = nolphin_acl_get_entries_finish (pending_result, &error);
	g_object_unref (pending_result);

	if (entries == NULL && error != NULL) {
		fail (error->message);
		g_clear_error (&error);
		goto cleanup;
	}

	if (find_group_entry (entries, groupname) != NULL) {
		fail ("named-group entry already present before the test added it - unexpected pre-existing state");
	} else {
		g_print ("PASS: baseline ACL has no entry for '%s' yet\n", groupname);
	}
	nolphin_acl_entry_list_free (entries);

	/* --- add a named-group entry: read+write, no execute --- */
	pending_result = NULL;
	nolphin_acl_set_entry_async (test_file, NOLPHIN_ACL_ENTRY_GROUP, groupname,
				     TRUE, TRUE, FALSE, FALSE, NULL, on_ready, NULL);
	gtk_main ();
	if (!nolphin_acl_set_entry_finish (pending_result, &error)) {
		fail (error ? error->message : "set_entry failed with no error set");
		g_clear_error (&error);
	} else {
		g_print ("PASS: setfacl reported success\n");
	}
	g_object_unref (pending_result);

	pending_result = NULL;
	nolphin_acl_get_entries_async (test_file, NULL, on_ready, NULL);
	gtk_main ();
	entries = nolphin_acl_get_entries_finish (pending_result, &error);
	g_object_unref (pending_result);

	{
		NolphinAclEntry *e = find_group_entry (entries, groupname);
		if (e == NULL) {
			fail ("added group entry does not show up in getfacl output");
		} else if (!e->can_read || !e->can_write || e->can_execute) {
			fail ("added group entry has the wrong permission bits (expected rw-)");
		} else {
			g_print ("PASS: added group entry shows up with the correct rw- permissions\n");
		}
	}
	nolphin_acl_entry_list_free (entries);

	/* --- remove it again --- */
	pending_result = NULL;
	nolphin_acl_remove_entry_async (test_file, NOLPHIN_ACL_ENTRY_GROUP, groupname,
					FALSE, NULL, on_ready, NULL);
	gtk_main ();
	if (!nolphin_acl_remove_entry_finish (pending_result, &error)) {
		fail (error ? error->message : "remove_entry failed with no error set");
		g_clear_error (&error);
	} else {
		g_print ("PASS: setfacl -x reported success\n");
	}
	g_object_unref (pending_result);

	pending_result = NULL;
	nolphin_acl_get_entries_async (test_file, NULL, on_ready, NULL);
	gtk_main ();
	entries = nolphin_acl_get_entries_finish (pending_result, &error);
	g_object_unref (pending_result);

	if (find_group_entry (entries, groupname) != NULL) {
		fail ("group entry still present after removal - setfacl -x did not actually remove it");
	} else {
		g_print ("PASS: group entry is gone after removal\n");
	}
	nolphin_acl_entry_list_free (entries);

cleanup:
	g_free (groupname);
	g_object_unref (test_file);

	if (exit_code == 0) {
		g_print ("All ACL tests passed.\n");
	}

	return exit_code;
}
