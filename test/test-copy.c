#include "test.h"

#include <stdlib.h>
#include <glib/gstdio.h>

#include <libnolphin-private/nolphin-file-operations.h>
#include <libnolphin-private/nolphin-progress-info.h>
#include <libnolphin-private/nolphin-progress-info-manager.h>

static gboolean self_test_mode = FALSE;
static gboolean self_test_success = FALSE;

static void
copy_done (GHashTable *debuting_uris, 
           gboolean success,
           gpointer data)
{
	g_print ("Copy done\n");
	if (self_test_mode) {
		self_test_success = success;
		gtk_main_quit ();
	}
}

static gboolean
self_test_timeout (gpointer data)
{
	g_printerr ("FAIL: Kopiervorgang wurde nicht fertig\n");
	gtk_main_quit ();
	return G_SOURCE_REMOVE;
}

/* Ohne Argumente: legt Quelle und Ziel im Temp-Ordner an, kopiert und
 * prüft Inhalt und Erfolgsmeldung. Mit Argumenten bleibt es das
 * manuelle Werkzeug "test-copy <Quellen...> <Zielordner>". */
static int
run_self_test (void)
{
	gchar *dir, *src_path, *dest_path, *copied, *contents = NULL, *cmd;
	GFile *src, *dest;
	GList *sources;
	GtkWidget *window;
	NolphinProgressInfoManager *manager;
	guint timeout;
	int result = 1;

	dir = g_dir_make_tmp ("nolphin-copy-XXXXXX", NULL);
	if (dir == NULL) {
		g_printerr ("FAIL: kein Temp-Ordner\n");
		return 1;
	}
	src_path = g_build_filename (dir, "quelle.txt", NULL);
	dest_path = g_build_filename (dir, "ziel", NULL);
	g_file_set_contents (src_path, "Inhalt zum Kopieren\n", -1, NULL);
	g_mkdir (dest_path, 0755);

	self_test_mode = TRUE;
	src = g_file_new_for_path (src_path);
	dest = g_file_new_for_path (dest_path);
	sources = g_list_append (NULL, src);
	window = test_window_new ("copy test", 5);
	gtk_widget_show (window);
	manager = nolphin_progress_info_manager_new ();

	nolphin_file_operations_copy (sources, NULL, dest, GTK_WINDOW (window), copy_done, NULL);

	timeout = g_timeout_add_seconds (15, self_test_timeout, NULL);
	gtk_main ();
	g_source_remove (timeout);

	copied = g_build_filename (dest_path, "quelle.txt", NULL);
	if (!self_test_success) {
		g_printerr ("FAIL: Kopieren meldete keinen Erfolg\n");
	} else if (!g_file_get_contents (copied, &contents, NULL, NULL) ||
		   g_strcmp0 (contents, "Inhalt zum Kopieren\n") != 0) {
		g_printerr ("FAIL: Kopie fehlt oder hat falschen Inhalt\n");
	} else {
		result = 0;
	}

	g_free (contents);
	g_free (copied);
	g_object_unref (manager);
	cmd = g_strdup_printf ("rm -rf '%s'", dir);
	if (system (cmd) != 0) {
		g_printerr ("Hinweis: Aufräumen von %s fehlgeschlagen\n", dir);
	}
	g_free (cmd);
	return result;
}

static void
changed_cb (NolphinProgressInfo *info,
	    gpointer data)
{
	g_print ("Changed: %s -- %s\n",
		 nolphin_progress_info_get_status (info),
		 nolphin_progress_info_get_details (info));
}

static void
progress_changed_cb (NolphinProgressInfo *info,
		     gpointer data)
{
	g_print ("Progress changed: %f\n",
		 nolphin_progress_info_get_progress (info));
}

static void
finished_cb (NolphinProgressInfo *info,
	     gpointer data)
{
	g_print ("Finished\n");
	gtk_main_quit ();
}

int 
main (int argc, char* argv[])
{
	GtkWidget *window;
	GList *sources;
	GFile *dest;
	GFile *source;
	int i;
	GList *infos;
        NolphinProgressInfoManager *manager;
	NolphinProgressInfo *progress_info;
	
	test_init (&argc, &argv);

	if (argc == 1) {
		return run_self_test ();
	}
	if (argc < 3) {
		g_print ("Usage test-copy <sources...> <dest dir>\n");
		return 1;
	}

	sources = NULL;
	for (i = 1; i < argc - 1; i++) {
		source = g_file_new_for_commandline_arg (argv[i]);
		sources = g_list_prepend (sources, source);
	}
	sources = g_list_reverse (sources);
	
	dest = g_file_new_for_commandline_arg (argv[i]);
	
	window = test_window_new ("copy test", 5);
	
	gtk_widget_show (window);

        manager = nolphin_progress_info_manager_new ();

	nolphin_file_operations_copy (sources,
				       NULL /* GArray *relative_item_points */,
				       dest,
				       GTK_WINDOW (window),
				       copy_done, NULL);
        
	infos = nolphin_progress_info_manager_get_all_infos (manager);

	if (infos == NULL) {
		g_object_unref (manager);
		return 0;
	}

	progress_info = NOLPHIN_PROGRESS_INFO (infos->data);

	g_signal_connect (progress_info, "changed", (GCallback)changed_cb, NULL);
	g_signal_connect (progress_info, "progress-changed", (GCallback)progress_changed_cb, NULL);
	g_signal_connect (progress_info, "finished", (GCallback)finished_cb, NULL);
	
	gtk_main ();

        g_object_unref (manager);
	
	return 0;
}


