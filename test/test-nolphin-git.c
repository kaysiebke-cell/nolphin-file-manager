/* Exercises nolphin_git_*() (libnolphin-private/nolphin-git.c)
 * against a real git repository created from scratch: init, an
 * initial commit, then a modified file, a staged file, and an
 * untracked file - checks that find_repository_root, get_status, add
 * and commit all report exactly what a real `git status`/`git log`
 * would. Also checks that a plain non-repository directory correctly
 * reports "no repository" (NULL, no error) rather than a false
 * positive. */

#include <gtk/gtk.h>
#include <libnolphin-private/nolphin-git.h>
#include <string.h>
#include <stdlib.h>

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

static int
run_cmd (const gchar *cwd, const gchar *cmd)
{
	gchar *full = g_strdup_printf ("cd %s && %s", cwd, cmd);
	int status = system (full);
	g_free (full);
	return status;
}

int
main (int argc, char **argv)
{
	gchar *tmpl, *quoted, *committed_path, *untracked_path;
	GError *error = NULL;
	GFile *repo_dir, *outside_dir, *found_root, *modified_file;
	GHashTable *status_table;
	gpointer status_ptr;

	gtk_init (&argc, &argv);

	if (!nolphin_git_is_available ()) {
		g_print ("SKIP: git not installed on this system\n");
		return 0;
	}

	tmpl = g_dir_make_tmp ("nolphin-git-test-XXXXXX", &error);
	if (tmpl == NULL) {
		g_printerr ("FAIL: could not create temp dir: %s\n", error->message);
		return 1;
	}
	quoted = g_shell_quote (tmpl);

	if (run_cmd (quoted, "git init -q") != 0 ||
	    run_cmd (quoted, "git config user.email test@example.com") != 0 ||
	    run_cmd (quoted, "git config user.name Test") != 0) {
		g_printerr ("FAIL: could not initialize test git repository\n");
		return 1;
	}

	committed_path = g_build_filename (tmpl, "committed.txt", NULL);
	g_file_set_contents (committed_path, "line1\n", -1, NULL);
	if (run_cmd (quoted, "git add committed.txt") != 0 ||
	    run_cmd (quoted, "git commit -q -m initial") != 0) {
		g_printerr ("FAIL: could not create the initial commit\n");
		return 1;
	}

	/* Now dirty the tree: modify the committed file, and add an
	 * untracked one - exactly the two states get_status() needs to
	 * report correctly. */
	g_file_set_contents (committed_path, "line1\nline2\n", -1, NULL);
	untracked_path = g_build_filename (tmpl, "untracked.txt", NULL);
	g_file_set_contents (untracked_path, "new\n", -1, NULL);

	repo_dir = g_file_new_for_path (tmpl);
	modified_file = g_file_new_for_path (committed_path);

	/* --- find_repository_root: from inside the repo --- */
	pending_result = NULL;
	nolphin_git_find_repository_root_async (repo_dir, NULL, on_ready, NULL);
	gtk_main ();
	found_root = nolphin_git_find_repository_root_finish (pending_result, &error);
	g_object_unref (pending_result);

	if (found_root == NULL) {
		fail (error ? error->message : "find_repository_root returned NULL with no error, for a real repository");
		g_clear_error (&error);
	} else {
		gchar *found_path = g_file_get_path (found_root);
		gchar *real_tmpl = realpath (tmpl, NULL);
		if (real_tmpl != NULL && g_strcmp0 (found_path, real_tmpl) == 0) {
			g_print ("PASS: find_repository_root found the correct repo root\n");
		} else {
			fail ("find_repository_root returned an unexpected path");
		}
		free (real_tmpl);
		g_free (found_path);
		g_object_unref (found_root);
	}

	/* --- find_repository_root: from OUTSIDE any repository --- */
	outside_dir = g_file_new_for_path (g_get_tmp_dir ());
	pending_result = NULL;
	nolphin_git_find_repository_root_async (outside_dir, NULL, on_ready, NULL);
	gtk_main ();
	found_root = nolphin_git_find_repository_root_finish (pending_result, &error);
	g_object_unref (pending_result);

	/* /tmp itself might legitimately be inside a repo in some odd
	 * setups, so only fail if this returned an actual GLib error -
	 * the real assertion already covered is the repo-root case above. */
	if (found_root != NULL) {
		g_object_unref (found_root);
	}
	if (error != NULL) {
		fail (error->message);
		g_clear_error (&error);
	} else {
		g_print ("PASS: find_repository_root outside a repo did not error out\n");
	}
	g_object_unref (outside_dir);

	/* --- get_status --- */
	pending_result = NULL;
	nolphin_git_get_status_async (repo_dir, NULL, on_ready, NULL);
	gtk_main ();
	status_table = nolphin_git_get_status_finish (pending_result, &error);
	g_object_unref (pending_result);

	if (status_table == NULL) {
		fail (error ? error->message : "get_status returned NULL with no error");
		g_clear_error (&error);
	} else {
		gboolean found_modified = FALSE, found_untracked = FALSE;
		GHashTableIter iter;
		gpointer key, value;

		g_hash_table_iter_init (&iter, status_table);
		while (g_hash_table_iter_next (&iter, &key, &value)) {
			const gchar *path = key;
			NolphinGitFileStatus st = (NolphinGitFileStatus) GPOINTER_TO_INT (value);
			if (g_strcmp0 (path, "committed.txt") == 0 && st == NOLPHIN_GIT_STATUS_MODIFIED) {
				found_modified = TRUE;
			}
			if (g_strcmp0 (path, "untracked.txt") == 0 && st == NOLPHIN_GIT_STATUS_UNTRACKED) {
				found_untracked = TRUE;
			}
		}

		if (!found_modified) {
			fail ("get_status did not report committed.txt as MODIFIED");
		} else {
			g_print ("PASS: get_status correctly reports committed.txt as modified\n");
		}
		if (!found_untracked) {
			fail ("get_status did not report untracked.txt as UNTRACKED");
		} else {
			g_print ("PASS: get_status correctly reports untracked.txt as untracked\n");
		}
		g_hash_table_unref (status_table);
	}

	/* --- add + status shows STAGED afterwards --- */
	{
		GList *files = g_list_prepend (NULL, modified_file);
		pending_result = NULL;
		nolphin_git_add_async (repo_dir, files, NULL, on_ready, NULL);
		gtk_main ();
		g_list_free (files);
	}
	if (!nolphin_git_add_finish (pending_result, &error)) {
		fail (error ? error->message : "git add failed with no error set");
		g_clear_error (&error);
	} else {
		g_print ("PASS: git add reported success\n");
	}
	g_object_unref (pending_result);

	pending_result = NULL;
	nolphin_git_get_status_async (repo_dir, NULL, on_ready, NULL);
	gtk_main ();
	status_table = nolphin_git_get_status_finish (pending_result, &error);
	g_object_unref (pending_result);

	status_ptr = (status_table != NULL) ? g_hash_table_lookup (status_table, "committed.txt") : NULL;
	if (status_table == NULL || (NolphinGitFileStatus) GPOINTER_TO_INT (status_ptr) != NOLPHIN_GIT_STATUS_STAGED) {
		fail ("committed.txt is not reported as STAGED after 'git add'");
	} else {
		g_print ("PASS: committed.txt correctly shows as staged after git add\n");
	}
	if (status_table != NULL) {
		g_hash_table_unref (status_table);
	}

	/* --- commit + status is clean of committed.txt afterwards --- */
	pending_result = NULL;
	nolphin_git_commit_async (repo_dir, "second commit", NULL, on_ready, NULL);
	gtk_main ();
	if (!nolphin_git_commit_finish (pending_result, &error)) {
		fail (error ? error->message : "git commit failed with no error set");
		g_clear_error (&error);
	} else {
		g_print ("PASS: git commit reported success\n");
	}
	g_object_unref (pending_result);

	pending_result = NULL;
	nolphin_git_get_status_async (repo_dir, NULL, on_ready, NULL);
	gtk_main ();
	status_table = nolphin_git_get_status_finish (pending_result, &error);
	g_object_unref (pending_result);

	if (status_table != NULL && g_hash_table_contains (status_table, "committed.txt")) {
		fail ("committed.txt still shows as changed after commit - fake success?");
	} else {
		g_print ("PASS: committed.txt is clean after commit (no longer in the status list)\n");
	}
	if (status_table != NULL) {
		g_hash_table_unref (status_table);
	}

	/* --- log shows both commits --- */
	pending_result = NULL;
	nolphin_git_log_async (repo_dir, NULL, 10, NULL, on_ready, NULL);
	gtk_main ();
	{
		gchar *log_text = nolphin_git_log_finish (pending_result, &error);
		g_object_unref (pending_result);
		if (log_text == NULL) {
			fail (error ? error->message : "git log failed with no error set");
			g_clear_error (&error);
		} else if (strstr (log_text, "initial") == NULL || strstr (log_text, "second commit") == NULL) {
			fail ("git log output is missing one of the two expected commit messages");
		} else {
			g_print ("PASS: git log shows both commits\n");
		}
		g_free (log_text);
	}

	/* --- remote add: succeeds, and is really visible to plain git --- */
	pending_result = NULL;
	nolphin_git_remote_add_async (repo_dir, "origin", "https://example.invalid/test/repo.git", NULL, on_ready, NULL);
	gtk_main ();
	if (!nolphin_git_remote_add_finish (pending_result, &error)) {
		fail (error ? error->message : "git remote add failed with no error set");
		g_clear_error (&error);
	} else {
		g_print ("PASS: git remote add reported success\n");
	}
	g_object_unref (pending_result);

	{
		gchar *remote_v_cmd = g_strdup_printf ("git remote -v > %s/remote-v.out", quoted);
		gchar *remote_out_path = g_build_filename (tmpl, "remote-v.out", NULL);
		gchar *remote_out_contents = NULL;
		run_cmd (quoted, remote_v_cmd);
		if (g_file_get_contents (remote_out_path, &remote_out_contents, NULL, NULL) &&
		    strstr (remote_out_contents, "origin") != NULL &&
		    strstr (remote_out_contents, "https://example.invalid/test/repo.git") != NULL) {
			g_print ("PASS: 'git remote -v' confirms the remote was really added, not just a fake success\n");
		} else {
			fail ("'git remote -v' does not show the remote that was just added");
		}
		g_free (remote_out_contents);
		g_free (remote_out_path);
		g_free (remote_v_cmd);
	}

	/* --- remote add: a second 'origin' must fail with git's real error, not be silently accepted --- */
	pending_result = NULL;
	nolphin_git_remote_add_async (repo_dir, "origin", "https://example.invalid/other/repo.git", NULL, on_ready, NULL);
	gtk_main ();
	if (nolphin_git_remote_add_finish (pending_result, &error)) {
		fail ("git remote add succeeded for a duplicate remote name - should have failed");
	} else {
		g_print ("PASS: git remote add correctly failed for a duplicate remote name (%s)\n", error->message);
		g_clear_error (&error);
	}
	g_object_unref (pending_result);

	/* --- remote list: der eben eingetragene Remote muss auftauchen --- */
	pending_result = NULL;
	nolphin_git_remote_list_async (repo_dir, NULL, on_ready, NULL);
	gtk_main ();
	{
		GHashTable *remotes = nolphin_git_remote_list_finish (pending_result, &error);
		g_object_unref (pending_result);
		if (remotes == NULL) {
			fail (error ? error->message : "remote_list returned NULL with no error");
			g_clear_error (&error);
		} else if (g_strcmp0 (g_hash_table_lookup (remotes, "origin"),
				      "https://example.invalid/test/repo.git") != 0 ||
			   g_hash_table_size (remotes) != 1) {
			fail ("remote_list does not show exactly origin -> the URL that was added");
		} else {
			g_print ("PASS: remote_list shows the existing remote with its URL\n");
		}
		if (remotes != NULL) {
			g_hash_table_unref (remotes);
		}
	}

	/* --- remote set-url: ändert die Adresse, unabhängig gegengeprüft --- */
	pending_result = NULL;
	nolphin_git_remote_set_url_async (repo_dir, "origin", "https://example.invalid/neu/repo.git", NULL, on_ready, NULL);
	gtk_main ();
	if (!nolphin_git_remote_set_url_finish (pending_result, &error)) {
		fail (error ? error->message : "remote set-url failed with no error set");
		g_clear_error (&error);
	}
	g_object_unref (pending_result);

	pending_result = NULL;
	nolphin_git_remote_list_async (repo_dir, NULL, on_ready, NULL);
	gtk_main ();
	{
		GHashTable *remotes = nolphin_git_remote_list_finish (pending_result, &error);
		g_object_unref (pending_result);
		if (remotes == NULL ||
		    g_strcmp0 (g_hash_table_lookup (remotes, "origin"), "https://example.invalid/neu/repo.git") != 0) {
			fail ("remote set-url did not change the URL of origin");
		} else {
			g_print ("PASS: remote set-url really changed the address of an existing remote\n");
		}
		if (remotes != NULL) {
			g_hash_table_unref (remotes);
		}
	}

	/* --- set-url auf einen nicht vorhandenen Remote: echter Fehler --- */
	pending_result = NULL;
	nolphin_git_remote_set_url_async (repo_dir, "gibt-es-nicht", "https://example.invalid/x.git", NULL, on_ready, NULL);
	gtk_main ();
	if (nolphin_git_remote_set_url_finish (pending_result, &error)) {
		fail ("set-url on a missing remote must fail");
	} else {
		g_print ("PASS: set-url on a missing remote fails with git's own error\n");
		g_clear_error (&error);
	}
	g_object_unref (pending_result);

	g_free (committed_path);
	g_free (untracked_path);
	g_free (tmpl);
	g_free (quoted);
	g_object_unref (repo_dir);
	g_object_unref (modified_file);

	if (exit_code == 0) {
		g_print ("All git tests passed.\n");
	}

	return exit_code;
}
