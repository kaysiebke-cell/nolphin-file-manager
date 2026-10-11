/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nolphin-git-overlay.c: Git-Status-Symbole an Dateien (Overlays)
 */

#include <config.h>

#include "nolphin-git-overlay.h"
#include "nolphin-file.h"
#include "nolphin-file-private.h"

#include <string.h>

/* Wie lange ein Status gilt, bevor die nächste Abfrage ihn neu holt. */
#define STATUS_MAX_AGE_US (5 * G_USEC_PER_SEC)

typedef struct {
	gchar *root;		/* Repository-Wurzel (Pfad) */
	GHashTable *status;	/* relativer Pfad -> NolphinGitFileStatus */
	gint64 fetched;		/* Zeitpunkt des letzten Abrufs, 0 = nie */
	gboolean pending;
} RepoState;

/* Verzeichnis (Pfad) -> Repository-Wurzel (Pfad) oder "" für "keins". */
static GHashTable *dir_roots = NULL;
/* Wurzel (Pfad) -> RepoState */
static GHashTable *repos = NULL;

static void
repo_state_free (RepoState *r)
{
	g_free (r->root);
	g_clear_pointer (&r->status, g_hash_table_unref);
	g_free (r);
}

static void
ensure_tables (void)
{
	if (dir_roots == NULL) {
		dir_roots = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
		repos = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, (GDestroyNotify) repo_state_free);
	}
}

/* Läuft über das Dateisystem nach oben (".git" suchen); Ergebnis je
 * Verzeichnis gemerkt, damit das Zeichnen nicht ständig stat()'t. */
static const gchar *
find_root_for_dir (const gchar *dir)
{
	const gchar *cached = g_hash_table_lookup (dir_roots, dir);
	gchar *cur, *found = NULL;

	if (cached != NULL) {
		return cached[0] != '\0' ? cached : NULL;
	}

	cur = g_strdup (dir);
	while (cur != NULL) {
		gchar *dot_git = g_build_filename (cur, ".git", NULL);
		gchar *parent;

		if (g_file_test (dot_git, G_FILE_TEST_EXISTS)) {
			g_free (dot_git);
			found = cur;
			break;
		}
		g_free (dot_git);

		parent = g_path_get_dirname (cur);
		if (g_strcmp0 (parent, cur) == 0) {
			g_free (parent);
			g_free (cur);
			break;
		}
		g_free (cur);
		cur = parent;
	}

	g_hash_table_insert (dir_roots, g_strdup (dir), g_strdup (found != NULL ? found : ""));
	g_free (found);

	cached = g_hash_table_lookup (dir_roots, dir);
	return cached[0] != '\0' ? cached : NULL;
}

/* Status-Tabelle von git um die Vorfahren-Ordner erweitern: ein Ordner mit
 * Änderungen darin wird selbst als geändert markiert. Untracked-Ordner
 * kommen von git mit abschließendem "/". */
static GHashTable *
build_index (GHashTable *raw)
{
	GHashTableIter iter;
	gpointer key, value;
	GHashTable *index = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

	g_hash_table_iter_init (&iter, raw);
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		gchar *path = g_strdup (key);
		NolphinGitFileStatus st = GPOINTER_TO_INT (value);
		gsize len = strlen (path);
		gchar *slash;

		if (len > 0 && path[len - 1] == '/') {
			path[len - 1] = '\0';
		}
		g_hash_table_replace (index, g_strdup (path), GINT_TO_POINTER (st));

		if (st != NOLPHIN_GIT_STATUS_IGNORED) {
			while ((slash = strrchr (path, '/')) != NULL) {
				*slash = '\0';
				if (!g_hash_table_contains (index, path)) {
					g_hash_table_insert (index, g_strdup (path),
							     GINT_TO_POINTER (NOLPHIN_GIT_STATUS_MODIFIED));
				}
			}
		}
		g_free (path);
	}

	return index;
}

static void
notify_path (const gchar *root, const gchar *rel)
{
	gchar *abs = g_build_filename (root, rel, NULL);
	GFile *location = g_file_new_for_path (abs);
	NolphinFile *file = nolphin_file_get_existing (location);

	if (file != NULL) {
		nolphin_file_emit_changed (file);
		nolphin_file_unref (file);
	}
	g_object_unref (location);
	g_free (abs);
}

static void
status_ready_cb (GObject *source, GAsyncResult *result, gpointer user_data)
{
	gchar *root = user_data;
	RepoState *repo = g_hash_table_lookup (repos, root);
	GHashTable *raw, *fresh, *old;
	GHashTableIter iter;
	gpointer key, value, other;

	raw = nolphin_git_get_status_finish (result, NULL);

	if (repo != NULL) {
		repo->pending = FALSE;
		repo->fetched = g_get_monotonic_time ();

		if (raw != NULL) {
			fresh = build_index (raw);
			old = repo->status;
			repo->status = fresh;

			/* Nur Dateien neu zeichnen, deren Status sich geändert hat. */
			g_hash_table_iter_init (&iter, fresh);
			while (g_hash_table_iter_next (&iter, &key, &value)) {
				if (old == NULL ||
				    !g_hash_table_lookup_extended (old, key, NULL, &other) || other != value) {
					notify_path (root, key);
				}
			}
			if (old != NULL) {
				g_hash_table_iter_init (&iter, old);
				while (g_hash_table_iter_next (&iter, &key, &value)) {
					if (!g_hash_table_contains (fresh, key)) {
						notify_path (root, key);
					}
				}
				g_hash_table_unref (old);
			}
		}
	}

	if (raw != NULL) {
		g_hash_table_unref (raw);
	}
	g_free (root);
}

NolphinGitFileStatus
nolphin_git_overlay_get_status (GFile *file)
{
	gchar *path, *dir;
	const gchar *root;
	RepoState *repo;
	NolphinGitFileStatus result = NOLPHIN_GIT_STATUS_UNMODIFIED;

	if (file == NULL || !g_file_is_native (file) || !nolphin_git_is_available ()) {
		return result;
	}

	path = g_file_get_path (file);
	if (path == NULL) {
		return result;
	}

	ensure_tables ();

	dir = g_path_get_dirname (path);
	root = find_root_for_dir (dir);
	g_free (dir);

	if (root != NULL && g_str_has_prefix (path, root)) {
		const gchar *rel = path + strlen (root);

		while (*rel == '/') {
			rel++;
		}

		repo = g_hash_table_lookup (repos, root);
		if (repo == NULL) {
			repo = g_new0 (RepoState, 1);
			repo->root = g_strdup (root);
			g_hash_table_insert (repos, repo->root, repo);
		}

		if (repo->status != NULL && rel[0] != '\0') {
			result = GPOINTER_TO_INT (g_hash_table_lookup (repo->status, rel));
		}

		if (!repo->pending &&
		    (repo->fetched == 0 || g_get_monotonic_time () - repo->fetched > STATUS_MAX_AGE_US)) {
			GFile *root_file = g_file_new_for_path (repo->root);

			repo->pending = TRUE;
			nolphin_git_get_status_async (root_file, NULL, status_ready_cb, g_strdup (repo->root));
			g_object_unref (root_file);
		}
	}

	g_free (path);
	return result;
}

const gchar * const *
nolphin_git_overlay_get_emblem_names (NolphinGitFileStatus status)
{
	static const gchar * const modified[] = { "emblem-vcs-modified", "emblem-important", NULL };
	static const gchar * const added[] = { "emblem-vcs-added", "emblem-default", NULL };
	static const gchar * const untracked[] = { "emblem-vcs-unknown", "emblem-new", NULL };
	static const gchar * const conflict[] = { "emblem-vcs-conflicting", "emblem-unreadable", NULL };

	switch (status) {
	case NOLPHIN_GIT_STATUS_MODIFIED:
	case NOLPHIN_GIT_STATUS_RENAMED:
		return modified;
	case NOLPHIN_GIT_STATUS_STAGED:
	case NOLPHIN_GIT_STATUS_ADDED:
		return added;
	case NOLPHIN_GIT_STATUS_UNTRACKED:
		return untracked;
	case NOLPHIN_GIT_STATUS_CONFLICTED:
		return conflict;
	default:
		return NULL;
	}
}
