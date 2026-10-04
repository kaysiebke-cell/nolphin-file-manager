/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nolphin-location-stats.c: Verlauf und "Häufig verwendet"
 */

#include <config.h>

#include "nolphin-location-stats.h"

#include <eel/eel-vfs-extensions.h>
#include <string.h>

#define GROUP_VISITS "visits"
#define GROUP_HISTORY "history"
#define KEY_RECENT "recent"
#define MAX_RECENT 30
#define MAX_VISIT_ENTRIES 200

struct _NolphinLocationStats {
	GObject parent_instance;

	GKeyFile *keyfile;
	gchar *path;
};

enum { CHANGED, LAST_SIGNAL };
static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (NolphinLocationStats, nolphin_location_stats, G_TYPE_OBJECT)

static void
save (NolphinLocationStats *stats)
{
	gchar *dir = g_path_get_dirname (stats->path);
	gchar *data;

	g_mkdir_with_parents (dir, 0700);
	g_free (dir);

	data = g_key_file_to_data (stats->keyfile, NULL, NULL);
	if (!g_file_set_contents (stats->path, data, -1, NULL)) {
		g_warning ("Konnte Orte-Statistik nicht speichern: %s", stats->path);
	}
	g_free (data);
}

static void
nolphin_location_stats_finalize (GObject *object)
{
	NolphinLocationStats *stats = NOLPHIN_LOCATION_STATS (object);

	g_key_file_free (stats->keyfile);
	g_free (stats->path);

	G_OBJECT_CLASS (nolphin_location_stats_parent_class)->finalize (object);
}

static void
nolphin_location_stats_class_init (NolphinLocationStatsClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = nolphin_location_stats_finalize;

	signals[CHANGED] = g_signal_new ("changed", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
					 0, NULL, NULL, NULL, G_TYPE_NONE, 0);
}

static void
nolphin_location_stats_init (NolphinLocationStats *stats)
{
	stats->keyfile = g_key_file_new ();
	stats->path = g_build_filename (g_get_user_data_dir (), "nolphin", "locations.ini", NULL);
	g_key_file_load_from_file (stats->keyfile, stats->path, G_KEY_FILE_NONE, NULL);
}

NolphinLocationStats *
nolphin_location_stats_get_default (void)
{
	static NolphinLocationStats *instance = NULL;

	if (instance == NULL) {
		instance = g_object_new (NOLPHIN_TYPE_LOCATION_STATS, NULL);
	}
	return instance;
}

/* Wirft den Eintrag mit dem kleinsten Zähler raus, wenn die Tabelle zu groß wird. */
static void
trim_visits (NolphinLocationStats *stats)
{
	gsize n, i;
	gchar **keys = g_key_file_get_keys (stats->keyfile, GROUP_VISITS, &n, NULL);
	gchar *lowest = NULL;
	gint lowest_count = G_MAXINT;

	if (n <= MAX_VISIT_ENTRIES) {
		g_strfreev (keys);
		return;
	}
	for (i = 0; i < n; i++) {
		gint c = g_key_file_get_integer (stats->keyfile, GROUP_VISITS, keys[i], NULL);

		if (c < lowest_count) {
			lowest_count = c;
			lowest = keys[i];
		}
	}
	if (lowest != NULL) {
		g_key_file_remove_key (stats->keyfile, GROUP_VISITS, lowest, NULL);
	}
	g_strfreev (keys);
}

void
nolphin_location_stats_record (NolphinLocationStats *stats, GFile *location)
{
	gchar *uri;
	gsize n, i;
	gchar **old;
	GPtrArray *recent;
	gint count;

	g_return_if_fail (NOLPHIN_IS_LOCATION_STATS (stats));
	g_return_if_fail (G_IS_FILE (location));

	uri = g_file_get_uri (location);
	if (eel_uri_is_search (uri)) {
		g_free (uri);
		return;
	}

	count = g_key_file_get_integer (stats->keyfile, GROUP_VISITS, uri, NULL);
	g_key_file_set_integer (stats->keyfile, GROUP_VISITS, uri, count + 1);
	trim_visits (stats);

	/* Verlauf: neuester zuerst, ohne Duplikate */
	old = g_key_file_get_string_list (stats->keyfile, GROUP_HISTORY, KEY_RECENT, &n, NULL);
	recent = g_ptr_array_new_with_free_func (g_free);
	g_ptr_array_add (recent, g_strdup (uri));
	for (i = 0; old != NULL && i < n && recent->len < MAX_RECENT; i++) {
		if (strcmp (old[i], uri) != 0) {
			g_ptr_array_add (recent, g_strdup (old[i]));
		}
	}
	g_key_file_set_string_list (stats->keyfile, GROUP_HISTORY, KEY_RECENT,
				    (const gchar * const *) recent->pdata, recent->len);
	g_ptr_array_free (recent, TRUE);
	g_strfreev (old);
	g_free (uri);

	save (stats);
	g_signal_emit (stats, signals[CHANGED], 0);
}

GList *
nolphin_location_stats_get_recent (NolphinLocationStats *stats, guint max)
{
	gsize n, i;
	gchar **uris;
	GList *result = NULL;

	g_return_val_if_fail (NOLPHIN_IS_LOCATION_STATS (stats), NULL);

	uris = g_key_file_get_string_list (stats->keyfile, GROUP_HISTORY, KEY_RECENT, &n, NULL);
	for (i = 0; uris != NULL && i < n && i < max; i++) {
		result = g_list_prepend (result, g_strdup (uris[i]));
	}
	g_strfreev (uris);

	return g_list_reverse (result);
}

typedef struct {
	gchar *uri;
	gint count;
} VisitEntry;

static gint
compare_visits (gconstpointer a, gconstpointer b)
{
	const VisitEntry *x = a, *y = b;

	return y->count - x->count;
}

GList *
nolphin_location_stats_get_frequent (NolphinLocationStats *stats, guint max)
{
	gsize n, i;
	gchar **keys;
	GList *entries = NULL, *l, *result = NULL;
	guint taken = 0;

	g_return_val_if_fail (NOLPHIN_IS_LOCATION_STATS (stats), NULL);

	keys = g_key_file_get_keys (stats->keyfile, GROUP_VISITS, &n, NULL);
	for (i = 0; keys != NULL && i < n; i++) {
		VisitEntry *e = g_new (VisitEntry, 1);

		e->uri = keys[i];
		e->count = g_key_file_get_integer (stats->keyfile, GROUP_VISITS, keys[i], NULL);
		entries = g_list_prepend (entries, e);
	}
	entries = g_list_sort (g_list_reverse (entries), compare_visits);

	for (l = entries; l != NULL && taken < max; l = l->next, taken++) {
		result = g_list_prepend (result, g_strdup (((VisitEntry *) l->data)->uri));
	}

	g_list_free_full (entries, g_free);
	g_strfreev (keys);

	return g_list_reverse (result);
}

void
nolphin_location_stats_clear (NolphinLocationStats *stats)
{
	g_return_if_fail (NOLPHIN_IS_LOCATION_STATS (stats));

	g_key_file_remove_group (stats->keyfile, GROUP_VISITS, NULL);
	g_key_file_remove_group (stats->keyfile, GROUP_HISTORY, NULL);
	save (stats);
	g_signal_emit (stats, signals[CHANGED], 0);
}
