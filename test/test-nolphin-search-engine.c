/* Prüft die Dateinamensuche (libnolphin-private/nolphin-search-engine-advanced.c)
 * in einem Temp-Ordner: Teilwort (UND), ODER, NICHT und exakte Namen.
 * Die Suche läuft bis "finished"; nach 15 s gilt sie als hängengeblieben. */

#include <gtk/gtk.h>
#include <string.h>
#include <stdlib.h>
#include <libnolphin-private/nolphin-search-engine.h>
#include <libnolphin-private/nolphin-search-engine-advanced.h>
#include <libnolphin-private/nolphin-query.h>

static gint exit_code = 0;
static GHashTable *found;

static void
hits_added_cb (NolphinSearchEngine *engine, GList *hits)
{
	for (; hits != NULL; hits = hits->next) {
		FileSearchResult *r = hits->data;
		gchar *name = g_path_get_basename (r->uri);

		g_hash_table_add (found, name);
	}
}

static void
finished_cb (NolphinSearchEngine *engine)
{
	gtk_main_quit ();
}

static gboolean
timeout_cb (gpointer data)
{
	g_printerr ("FAIL: Suche wurde nicht fertig\n");
	exit_code = 1;
	gtk_main_quit ();
	return G_SOURCE_REMOVE;
}

/* Sucht @pattern in @dir und vergleicht die gefundenen Namen (sortiert, kommagetrennt). */
static void
check (const gchar *dir, const gchar *pattern, const gchar *want)
{
	NolphinSearchEngine *engine = nolphin_search_engine_advanced_new ();
	NolphinQuery *query = nolphin_query_new ();
	GList *names, *l;
	GString *got = g_string_new (NULL);
	gchar *uri = g_filename_to_uri (dir, NULL, NULL);
	guint timeout;

	found = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	g_signal_connect (engine, "hits-added", G_CALLBACK (hits_added_cb), NULL);
	g_signal_connect (engine, "finished", G_CALLBACK (finished_cb), NULL);

	nolphin_query_set_location (query, uri);
	nolphin_query_set_file_pattern (query, pattern);
	nolphin_search_engine_set_query (engine, query);
	nolphin_search_engine_start (engine);

	timeout = g_timeout_add_seconds (15, timeout_cb, NULL);
	gtk_main ();
	g_source_remove (timeout);

	names = g_hash_table_get_keys (found);
	names = g_list_sort (names, (GCompareFunc) strcmp);
	for (l = names; l != NULL; l = l->next) {
		if (got->len > 0) {
			g_string_append_c (got, ',');
		}
		g_string_append (got, l->data);
	}
	if (strcmp (got->str, want) != 0) {
		g_printerr ("FAIL: Suche »%s«: »%s« statt »%s«\n", pattern, got->str, want);
		exit_code = 1;
	}

	g_list_free (names);
	g_string_free (got, TRUE);
	g_free (uri);
	g_object_unref (query);
	g_object_unref (engine);
	g_hash_table_unref (found);
}

int
main (int argc, char **argv)
{
	static const gchar *files[] = { "bericht.txt", "bericht-entwurf.txt", "foto.png", "notiz.txt", NULL };
	gchar *dir, *cmd;
	GError *error = NULL;
	guint i;

	gtk_init (&argc, &argv);

	dir = g_dir_make_tmp ("nolphin-search-XXXXXX", &error);
	if (dir == NULL) {
		g_printerr ("FAIL: temp dir: %s\n", error->message);
		return 1;
	}
	for (i = 0; files[i] != NULL; i++) {
		gchar *path = g_build_filename (dir, files[i], NULL);
		g_file_set_contents (path, "x", -1, NULL);
		g_free (path);
	}

	check (dir, "bericht", "bericht-entwurf.txt,bericht.txt");
	check (dir, "bericht foto", "");
	check (dir, "foto ODER notiz", "foto.png,notiz.txt");
	check (dir, "bericht NICHT entwurf", "bericht.txt");
	check (dir, "\"foto.png\"", "foto.png");
	check (dir, "*.txt -bericht", "notiz.txt");

	cmd = g_strdup_printf ("rm -rf '%s'", dir);
	if (system (cmd) != 0) {
		g_printerr ("Hinweis: Aufräumen von %s fehlgeschlagen\n", dir);
	}
	g_free (cmd);
	return exit_code;
}
