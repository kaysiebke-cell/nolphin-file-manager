/* Prüft das asynchrone Laden eines Ordners (libnolphin-private/nolphin-directory.c):
 * ein Temp-Ordner mit drei Dateien muss vollständig geladen werden ("done-loading"),
 * nach dem Anlegen einer vierten Datei und erneutem Laden muss auch diese
 * gemeldet werden. */

#include <gtk/gtk.h>
#include <glib/gstdio.h>
#include <stdlib.h>
#include <libnolphin-private/nolphin-directory.h>
#include <libnolphin-private/nolphin-file.h>

static gint exit_code = 0;
static gint added = 0;
static gint loads_done = 0;
static gchar *dir_path;
static void *client;

static void
files_added_cb (NolphinDirectory *directory, GList *files)
{
	added += g_list_length (files);
}

static gboolean
reload_cb (gpointer data)
{
	nolphin_directory_force_reload (data);
	return G_SOURCE_REMOVE;
}

static void
done_loading_cb (NolphinDirectory *directory)
{
	loads_done++;

	if (loads_done == 1) {
		gchar *path;

		if (added != 3) {
			g_printerr ("FAIL: erstes Laden meldete %d statt 3 Dateien\n", added);
			exit_code = 1;
		}
		path = g_build_filename (dir_path, "vierte.txt", NULL);
		g_file_set_contents (path, "4", -1, NULL);
		g_free (path);
		g_timeout_add (200, reload_cb, directory);
	} else {
		if (added < 4) {
			g_printerr ("FAIL: neue Datei wurde nach dem Neuladen nicht gemeldet (%d)\n", added);
			exit_code = 1;
		}
		gtk_main_quit ();
	}
}

static gboolean
timeout_cb (gpointer data)
{
	g_printerr ("FAIL: Ordner wurde nicht fertig geladen (loads=%d, added=%d)\n", loads_done, added);
	exit_code = 1;
	gtk_main_quit ();
	return G_SOURCE_REMOVE;
}

int
main (int argc, char **argv)
{
	NolphinDirectory *directory;
	gchar *uri, *cmd;
	GError *error = NULL;
	const gchar *names[] = { "a.txt", "b.txt", "c.txt", NULL };
	int i;

	gtk_init (&argc, &argv);
	client = g_new0 (int, 1);

	dir_path = g_dir_make_tmp ("nolphin-dir-XXXXXX", &error);
	if (dir_path == NULL) {
		g_printerr ("FAIL: temp dir: %s\n", error->message);
		return 1;
	}
	for (i = 0; names[i] != NULL; i++) {
		gchar *path = g_build_filename (dir_path, names[i], NULL);
		g_file_set_contents (path, "x", -1, NULL);
		g_free (path);
	}

	uri = g_filename_to_uri (dir_path, NULL, NULL);
	directory = nolphin_directory_get_by_uri (uri);
	g_signal_connect (directory, "files-added", G_CALLBACK (files_added_cb), NULL);
	g_signal_connect (directory, "done-loading", G_CALLBACK (done_loading_cb), NULL);
	nolphin_directory_file_monitor_add (directory, client, TRUE, NOLPHIN_FILE_ATTRIBUTE_INFO, NULL, NULL);

	g_timeout_add_seconds (20, timeout_cb, NULL);
	gtk_main ();

	cmd = g_strdup_printf ("rm -rf '%s'", dir_path);
	if (system (cmd) != 0) {
		g_printerr ("Hinweis: Aufräumen von %s fehlgeschlagen\n", dir_path);
	}
	g_free (cmd);
	return exit_code;
}
