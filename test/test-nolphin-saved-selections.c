/* Exercises nolphin_saved_selections_*() (libnolphin-private/nolphin-
 * saved-selections.c) against a real, isolated GKeyFile on disk - not
 * a fake/mocked store. $XDG_DATA_HOME is overridden to a fresh
 * scratch directory *before* any call that might touch
 * g_get_user_data_dir() (GLib caches it on first use), so this test
 * never reads or writes the real user's
 * ~/.local/share/nolphin/saved-selections.ini. */

#include <glib.h>
#include <libnolphin-private/nolphin-saved-selections.h>
#include <string.h>

static gint exit_code = 0;

static void
fail (const gchar *message)
{
	g_printerr ("FAIL: %s\n", message);
	exit_code = 1;
}

static gboolean
list_contains (GList *list, const gchar *value)
{
	GList *l;
	for (l = list; l != NULL; l = l->next) {
		if (g_strcmp0 ((const gchar *) l->data, value) == 0) {
			return TRUE;
		}
	}
	return FALSE;
}

int
main (int argc, char **argv)
{
	gchar *scratch_data_home;
	GError *error = NULL;
	GList *basenames, *restored, *names;
	gboolean ok;

	scratch_data_home = g_dir_make_tmp ("nolphin-saved-selections-test-XXXXXX", &error);
	if (scratch_data_home == NULL) {
		g_printerr ("FAIL: could not create temp dir: %s\n", error->message);
		return 1;
	}
	g_setenv ("XDG_DATA_HOME", scratch_data_home, TRUE);

	/* --- restoring a name that was never saved: NULL, not a crash --- */

	restored = nolphin_saved_selections_restore ("file:///tmp/some-folder", "does-not-exist");
	if (restored != NULL) {
		fail ("restore of a never-saved name must return NULL");
		g_list_free_full (restored, g_free);
	} else {
		g_print ("PASS: restoring an unknown saved selection returns NULL\n");
	}

	/* --- save + restore round-trip --- */

	basenames = NULL;
	basenames = g_list_append (basenames, "alpha.txt");
	basenames = g_list_append (basenames, "beta.txt");
	basenames = g_list_append (basenames, "with spaces & ümlaut.txt");

	ok = nolphin_saved_selections_save ("file:///tmp/project-a", "Wichtige Dateien", basenames, &error);
	g_list_free (basenames);

	if (!ok) {
		fail (error ? error->message : "save failed with no error set");
		g_clear_error (&error);
	} else {
		g_print ("PASS: saving a named selection succeeded\n");
	}

	restored = nolphin_saved_selections_restore ("file:///tmp/project-a", "Wichtige Dateien");
	if (restored == NULL) {
		fail ("restoring a just-saved selection returned NULL");
	} else if (g_list_length (restored) != 3 ||
		   !list_contains (restored, "alpha.txt") ||
		   !list_contains (restored, "beta.txt") ||
		   !list_contains (restored, "with spaces & ümlaut.txt")) {
		fail ("restored selection does not contain exactly the three saved basenames");
	} else {
		g_print ("PASS: restored selection matches exactly what was saved (incl. spaces/umlauts)\n");
	}
	g_list_free_full (restored, g_free);

	/* --- a different folder with the same name must stay separate --- */

	restored = nolphin_saved_selections_restore ("file:///tmp/project-b", "Wichtige Dateien");
	if (restored != NULL) {
		fail ("a same-named selection in a different folder must not leak across folders");
		g_list_free_full (restored, g_free);
	} else {
		g_print ("PASS: saved selections are correctly scoped per folder\n");
	}

	/* --- overwrite --- */

	basenames = g_list_append (NULL, "only-this-one.txt");
	nolphin_saved_selections_save ("file:///tmp/project-a", "Wichtige Dateien", basenames, NULL);
	g_list_free (basenames);

	restored = nolphin_saved_selections_restore ("file:///tmp/project-a", "Wichtige Dateien");
	if (restored == NULL || g_list_length (restored) != 1 ||
	    g_strcmp0 ((const gchar *) restored->data, "only-this-one.txt") != 0) {
		fail ("saving under an existing name did not overwrite the previous selection");
	} else {
		g_print ("PASS: saving under an existing name overwrites it\n");
	}
	g_list_free_full (restored, g_free);

	/* --- list_names --- */

	basenames = g_list_append (NULL, "x.txt");
	nolphin_saved_selections_save ("file:///tmp/project-a", "Zweite Auswahl", basenames, NULL);
	g_list_free (basenames);

	names = nolphin_saved_selections_list_names ("file:///tmp/project-a");
	if (g_list_length (names) != 2 ||
	    !list_contains (names, "Wichtige Dateien") ||
	    !list_contains (names, "Zweite Auswahl")) {
		fail ("list_names did not return exactly the two saved selection names");
	} else {
		g_print ("PASS: list_names returns exactly the saved selections for that folder\n");
	}
	g_list_free_full (names, g_free);

	/* --- delete --- */

	if (!nolphin_saved_selections_delete ("file:///tmp/project-a", "Zweite Auswahl")) {
		fail ("delete of an existing saved selection returned FALSE");
	} else {
		g_print ("PASS: delete reports success for an existing saved selection\n");
	}

	if (nolphin_saved_selections_delete ("file:///tmp/project-a", "Zweite Auswahl")) {
		fail ("delete of an already-deleted selection should return FALSE, not TRUE");
	} else {
		g_print ("PASS: delete correctly reports FALSE for a name that no longer exists\n");
	}

	restored = nolphin_saved_selections_restore ("file:///tmp/project-a", "Zweite Auswahl");
	if (restored != NULL) {
		fail ("a deleted selection must no longer be restorable");
		g_list_free_full (restored, g_free);
	} else {
		g_print ("PASS: a deleted selection is really gone\n");
	}

	g_free (scratch_data_home);

	if (exit_code == 0) {
		g_print ("All saved-selections tests passed.\n");
	}

	return exit_code;
}
