/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nolphin-duplicates.c: Duplikaterkennung (§46)
 *
 * Findet Duplikate in einem gewählten Ordner nach gleichem Namen, gleicher
 * Größe oder gleicher Prüfsumme (SHA-256). Ergebnisse erscheinen gruppiert;
 * Löschen geht nur in den Papierkorb und nur nach Bestätigung.
 */

#include <config.h>

#include "nolphin-tools.h"

#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <string.h>

#define MAX_SCAN_FILES 200000

typedef enum { MODE_NAME, MODE_SIZE, MODE_CHECKSUM } DupMode;

typedef struct {
	gchar *path;
	goffset size;
	gint64 mtime;
} DupFile;

typedef struct {
	GPtrArray *files;   /* DupFile* */
} DupGroup;

static void
dup_file_free (DupFile *f)
{
	g_free (f->path);
	g_free (f);
}

static void
dup_group_free (DupGroup *g)
{
	g_ptr_array_unref (g->files);
	g_free (g);
}

/* --- Suche im Worker-Thread --------------------------------------------------- */

typedef struct {
	gchar *root;
	DupMode mode;
	gboolean include_hidden;
} ScanJob;

static void
scan_job_free (ScanJob *j)
{
	g_free (j->root);
	g_free (j);
}

static void
collect_files (GFile *dir, gboolean include_hidden, GPtrArray *out, GCancellable *cancellable)
{
	GFileEnumerator *en;
	GFileInfo *info;

	en = g_file_enumerate_children (dir,
					G_FILE_ATTRIBUTE_STANDARD_NAME "," G_FILE_ATTRIBUTE_STANDARD_TYPE ","
					G_FILE_ATTRIBUTE_STANDARD_SIZE "," G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK ","
					G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN "," G_FILE_ATTRIBUTE_TIME_MODIFIED,
					G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, cancellable, NULL);
	if (en == NULL) {
		return;
	}
	while (out->len < MAX_SCAN_FILES && (info = g_file_enumerator_next_file (en, cancellable, NULL)) != NULL) {
		const gchar *name = g_file_info_get_name (info);
		GFile *child;

		if (g_file_info_get_is_symlink (info) || (!include_hidden && g_file_info_get_is_hidden (info))) {
			g_object_unref (info);
			continue;
		}
		child = g_file_get_child (dir, name);
		if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY) {
			collect_files (child, include_hidden, out, cancellable);
		} else if (g_file_info_get_file_type (info) == G_FILE_TYPE_REGULAR) {
			DupFile *f = g_new0 (DupFile, 1);

			f->path = g_file_get_path (child);
			f->size = g_file_info_get_size (info);
			f->mtime = (gint64) g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_TIME_MODIFIED);
			if (f->path != NULL) {
				g_ptr_array_add (out, f);
			} else {
				dup_file_free (f);
			}
		}
		g_object_unref (child);
		g_object_unref (info);
		if (g_cancellable_is_cancelled (cancellable)) {
			break;
		}
	}
	g_object_unref (en);
}

static gchar *
sha256_of_file (const gchar *path, GCancellable *cancellable)
{
	FILE *fp = g_fopen (path, "rb");
	GChecksum *sum;
	guchar buf[65536];
	gsize n;
	gchar *digest;

	if (fp == NULL) {
		return NULL;
	}
	sum = g_checksum_new (G_CHECKSUM_SHA256);
	while ((n = fread (buf, 1, sizeof buf, fp)) > 0) {
		g_checksum_update (sum, buf, n);
		if (g_cancellable_is_cancelled (cancellable)) {
			break;
		}
	}
	fclose (fp);
	digest = g_cancellable_is_cancelled (cancellable) ? NULL : g_strdup (g_checksum_get_string (sum));
	g_checksum_free (sum);
	return digest;
}

/* Hilfstabelle: Schlüssel -> GPtrArray von DupFile* (Besitz bleibt bei den Gruppen) */
static GHashTable *
group_table_new (void)
{
	return g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_ptr_array_unref);
}

static void
group_add (GHashTable *t, const gchar *key, DupFile *f)
{
	GPtrArray *arr = g_hash_table_lookup (t, key);

	if (arr == NULL) {
		arr = g_ptr_array_new ();
		g_hash_table_insert (t, g_strdup (key), arr);
	}
	g_ptr_array_add (arr, f);
}

static void
scan_thread (GTask *task, gpointer source, gpointer task_data, GCancellable *cancellable)
{
	ScanJob *job = task_data;
	GPtrArray *all = g_ptr_array_new_with_free_func ((GDestroyNotify) dup_file_free);
	GHashTable *by_key = group_table_new ();
	GList *groups = NULL, *keys, *l;
	GFile *root = g_file_new_for_path (job->root);
	guint i;

	collect_files (root, job->include_hidden, all, cancellable);
	g_object_unref (root);

	if (job->mode == MODE_NAME) {
		for (i = 0; i < all->len; i++) {
			DupFile *f = all->pdata[i];
			gchar *base = g_path_get_basename (f->path);

			group_add (by_key, base, f);
			g_free (base);
		}
	} else {
		/* Zuerst nach Größe (leere Dateien zählen nicht als Duplikate) */
		for (i = 0; i < all->len; i++) {
			DupFile *f = all->pdata[i];
			gchar *key = g_strdup_printf ("%" G_GINT64_FORMAT, (gint64) f->size);

			if (f->size > 0) {
				group_add (by_key, key, f);
			}
			g_free (key);
		}
		if (job->mode == MODE_CHECKSUM) {
			GHashTable *by_hash = group_table_new ();
			GHashTableIter it;
			gpointer k, v;

			g_hash_table_iter_init (&it, by_key);
			while (g_hash_table_iter_next (&it, &k, &v) && !g_cancellable_is_cancelled (cancellable)) {
				GPtrArray *arr = v;
				guint j;

				if (arr->len < 2) {
					continue;
				}
				for (j = 0; j < arr->len; j++) {
					DupFile *f = arr->pdata[j];
					gchar *digest = sha256_of_file (f->path, cancellable);

					if (digest != NULL) {
						group_add (by_hash, digest, f);
						g_free (digest);
					}
				}
			}
			g_hash_table_destroy (by_key);
			by_key = by_hash;
		}
	}

	/* Gruppen mit mindestens zwei Dateien; die Dateiobjekte wandern in die Gruppe */
	keys = g_hash_table_get_keys (by_key);
	for (l = keys; l != NULL; l = l->next) {
		GPtrArray *arr = g_hash_table_lookup (by_key, l->data);

		if (arr->len >= 2) {
			DupGroup *g = g_new0 (DupGroup, 1);
			guint j;

			g->files = g_ptr_array_new_with_free_func ((GDestroyNotify) dup_file_free);
			for (j = 0; j < arr->len; j++) {
				DupFile *src = arr->pdata[j], *copy = g_new0 (DupFile, 1);

				copy->path = g_strdup (src->path);
				copy->size = src->size;
				copy->mtime = src->mtime;
				g_ptr_array_add (g->files, copy);
			}
			groups = g_list_append (groups, g);
		}
	}
	g_list_free (keys);
	g_hash_table_destroy (by_key);
	g_ptr_array_unref (all);

	if (g_cancellable_is_cancelled (cancellable)) {
		g_list_free_full (groups, (GDestroyNotify) dup_group_free);
		g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_CANCELLED, "%s", _("Abgebrochen."));
		return;
	}
	g_task_return_pointer (task, groups, NULL);
}

/* --- Seite ----------------------------------------------------------------------- */

enum { DC_CHECK, DC_NAME, DC_SIZE, DC_DATE, DC_PATH, DC_IS_GROUP, DC_IS_FILE, DC_N };

typedef struct {
	NolphinWindow *window;
	GFile *folder;
	GtkWidget *folder_label;
	GtkWidget *mode_combo;
	GtkWidget *hidden_check;
	GtkWidget *start_button;
	GtkWidget *cancel_button;
	GtkWidget *trash_button;
	GtkWidget *spinner;
	GtkWidget *status;
	GtkWidget *tree;
	GtkTreeStore *store;
	GCancellable *cancellable;
} DupPage;

static void
dup_set_status (DupPage *d, const gchar *text)
{
	gtk_label_set_text (GTK_LABEL (d->status), text != NULL ? text : "");
}

static void
dup_set_busy (DupPage *d, gboolean busy)
{
	gtk_widget_set_sensitive (d->start_button, !busy && d->folder != NULL);
	gtk_widget_set_sensitive (d->cancel_button, busy);
	gtk_widget_set_sensitive (d->trash_button, !busy);
	if (busy) {
		gtk_spinner_start (GTK_SPINNER (d->spinner));
	} else {
		gtk_spinner_stop (GTK_SPINNER (d->spinner));
	}
}

static void
dup_fill (DupPage *d, GList *groups)
{
	GList *l;
	guint group_count = 0, file_count = 0;
	goffset wasted = 0;

	gtk_tree_store_clear (d->store);
	for (l = groups; l != NULL; l = l->next) {
		DupGroup *g = l->data;
		GtkTreeIter parent;
		gchar *title, *size_text;
		guint j;
		DupFile *first = g->files->pdata[0];

		size_text = g_format_size (first->size);
		title = g_strdup_printf (ngettext ("%u Datei – je %s", "%u Dateien – je %s", g->files->len), g->files->len, size_text);
		gtk_tree_store_append (d->store, &parent, NULL);
		gtk_tree_store_set (d->store, &parent, DC_CHECK, FALSE, DC_NAME, title, DC_SIZE, "", DC_DATE, "",
				    DC_PATH, NULL, DC_IS_GROUP, TRUE, DC_IS_FILE, FALSE, -1);
		g_free (title);
		g_free (size_text);

		for (j = 0; j < g->files->len; j++) {
			DupFile *f = g->files->pdata[j];
			GtkTreeIter child;
			gchar *base = g_path_get_basename (f->path), *dir = g_path_get_dirname (f->path);
			gchar *sz = g_format_size (f->size);
			GDateTime *dt = g_date_time_new_from_unix_local (f->mtime);
			gchar *date = g_date_time_format (dt, "%d.%m.%Y %H:%M");
			gchar *label = g_strdup_printf ("%s  —  %s", base, dir);

			gtk_tree_store_append (d->store, &child, &parent);
			gtk_tree_store_set (d->store, &child, DC_CHECK, FALSE, DC_NAME, label, DC_SIZE, sz, DC_DATE, date,
					    DC_PATH, f->path, DC_IS_GROUP, FALSE, DC_IS_FILE, TRUE, -1);
			g_free (label);
			g_free (date);
			g_date_time_unref (dt);
			g_free (sz);
			g_free (base);
			g_free (dir);
			file_count++;
			if (j > 0) {
				wasted += f->size;
			}
		}
		group_count++;
	}
	gtk_tree_view_expand_all (GTK_TREE_VIEW (d->tree));

	if (group_count == 0) {
		dup_set_status (d, _("Keine Duplikate gefunden."));
	} else {
		gchar *w = g_format_size (wasted);
		gchar *msg = g_strdup_printf (_("%u Gruppen mit %u Dateien – vermeidbarer Platz: %s"), group_count, file_count, w);

		dup_set_status (d, msg);
		g_free (msg);
		g_free (w);
	}
}

static void
dup_scan_ready (GObject *source, GAsyncResult *result, gpointer user_data)
{
	DupPage *d = user_data;
	GError *error = NULL;
	GList *groups = g_task_propagate_pointer (G_TASK (result), &error);

	dup_set_busy (d, FALSE);
	if (error != NULL) {
		dup_set_status (d, error->message);
		g_clear_error (&error);
		return;
	}
	dup_fill (d, groups);
	g_list_free_full (groups, (GDestroyNotify) dup_group_free);
}

static void
on_dup_start (GtkButton *button, gpointer data)
{
	DupPage *d = data;
	ScanJob *job;
	GTask *task;

	if (d->folder == NULL) {
		return;
	}
	g_clear_object (&d->cancellable);
	d->cancellable = g_cancellable_new ();

	job = g_new0 (ScanJob, 1);
	job->root = g_file_get_path (d->folder);
	job->mode = (DupMode) gtk_combo_box_get_active (GTK_COMBO_BOX (d->mode_combo));
	job->include_hidden = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (d->hidden_check));
	if (job->root == NULL) {
		scan_job_free (job);
		dup_set_status (d, _("Nur lokale Ordner werden unterstützt."));
		return;
	}

	gtk_tree_store_clear (d->store);
	dup_set_status (d, _("Wird durchsucht …"));
	dup_set_busy (d, TRUE);

	task = g_task_new (NULL, d->cancellable, dup_scan_ready, d);
	g_task_set_task_data (task, job, (GDestroyNotify) scan_job_free);
	g_task_run_in_thread (task, scan_thread);
	g_object_unref (task);
}

static void
on_dup_cancel (GtkButton *button, gpointer data)
{
	DupPage *d = data;

	if (d->cancellable != NULL) {
		g_cancellable_cancel (d->cancellable);
	}
}

static void
on_dup_toggled (GtkCellRendererToggle *cell, gchar *path_str, gpointer data)
{
	DupPage *d = data;
	GtkTreeIter iter;
	gboolean value, is_group;

	if (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (d->store), &iter, path_str)) {
		gtk_tree_model_get (GTK_TREE_MODEL (d->store), &iter, DC_CHECK, &value, DC_IS_GROUP, &is_group, -1);
		if (!is_group) {
			gtk_tree_store_set (d->store, &iter, DC_CHECK, !value, -1);
		}
	}
}

/* Alle bis auf die erste Datei jeder Gruppe markieren (die erste bleibt erhalten). */
static void
on_dup_mark_extras (GtkButton *button, gpointer data)
{
	DupPage *d = data;
	GtkTreeIter group;
	gboolean valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (d->store), &group);

	while (valid) {
		GtkTreeIter child;
		gboolean first = TRUE, ok = gtk_tree_model_iter_children (GTK_TREE_MODEL (d->store), &child, &group);

		while (ok) {
			gtk_tree_store_set (d->store, &child, DC_CHECK, !first, -1);
			first = FALSE;
			ok = gtk_tree_model_iter_next (GTK_TREE_MODEL (d->store), &child);
		}
		valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (d->store), &group);
	}
}

typedef struct {
	GList *paths;
} TrashCollect;

static gboolean
collect_checked_cb (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
	TrashCollect *c = data;
	gboolean checked, is_group;
	gchar *p = NULL;

	gtk_tree_model_get (model, iter, DC_CHECK, &checked, DC_IS_GROUP, &is_group, DC_PATH, &p, -1);
	if (checked && !is_group && p != NULL) {
		c->paths = g_list_prepend (c->paths, p);
	} else {
		g_free (p);
	}
	return FALSE;
}

static void
on_dup_trash (GtkButton *button, gpointer data)
{
	DupPage *d = data;
	TrashCollect c = { NULL };
	GtkWidget *toplevel = gtk_widget_get_toplevel (GTK_WIDGET (button));
	GtkWidget *dialog;
	guint n, trashed = 0, failed = 0;
	GList *l;

	gtk_tree_model_foreach (GTK_TREE_MODEL (d->store), collect_checked_cb, &c);
	n = g_list_length (c.paths);
	if (n == 0) {
		dup_set_status (d, _("Keine Dateien markiert."));
		return;
	}

	dialog = gtk_message_dialog_new (GTK_IS_WINDOW (toplevel) ? GTK_WINDOW (toplevel) : NULL,
					 GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
					 ngettext ("%u markierte Datei in den Papierkorb verschieben?",
						   "%u markierte Dateien in den Papierkorb verschieben?", n), n);
	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s",
						  _("Die Dateien lassen sich aus dem Papierkorb wiederherstellen."));
	gtk_dialog_add_buttons (GTK_DIALOG (dialog), _("_Abbrechen"), GTK_RESPONSE_CANCEL,
				_("In den _Papierkorb"), GTK_RESPONSE_OK, NULL);
	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK) {
		for (l = c.paths; l != NULL; l = l->next) {
			GFile *f = g_file_new_for_path (l->data);

			if (g_file_trash (f, NULL, NULL)) {
				trashed++;
			} else {
				failed++;
			}
			g_object_unref (f);
		}
		{
			gchar *msg = g_strdup_printf (_("%u in den Papierkorb verschoben, %u fehlgeschlagen. Zum Aktualisieren erneut suchen."),
						      trashed, failed);

			dup_set_status (d, msg);
			g_free (msg);
		}
		gtk_tree_store_clear (d->store);
	}
	gtk_widget_destroy (dialog);
	g_list_free_full (c.paths, g_free);
}

static void dup_set_folder (DupPage *d, GFile *folder);

static void
on_dup_choose_folder (GtkButton *button, gpointer data)
{
	DupPage *d = data;
	GtkWidget *toplevel = gtk_widget_get_toplevel (GTK_WIDGET (button));
	GtkWidget *chooser = gtk_file_chooser_dialog_new (_("Ordner durchsuchen"),
							  GTK_IS_WINDOW (toplevel) ? GTK_WINDOW (toplevel) : NULL,
							  GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
							  _("_Abbrechen"), GTK_RESPONSE_CANCEL,
							  _("_Auswählen"), GTK_RESPONSE_ACCEPT, NULL);

	if (gtk_dialog_run (GTK_DIALOG (chooser)) == GTK_RESPONSE_ACCEPT) {
		GFile *f = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (chooser));

		if (f != NULL) {
			dup_set_folder (d, f);
			g_object_unref (f);
		}
	}
	gtk_widget_destroy (chooser);
}

static void
dup_page_free (gpointer data)
{
	DupPage *d = data;

	if (d->cancellable != NULL) {
		g_cancellable_cancel (d->cancellable);
		g_object_unref (d->cancellable);
	}
	g_clear_object (&d->folder);
	g_free (d);
}

GtkWidget *
nolphin_duplicates_page_new (NolphinWindow *window)
{
	DupPage *d = g_new0 (DupPage, 1);
	GtkWidget *outer, *row, *scroller, *tree_scroller, *b, *intro;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	d->window = window;
	d->store = gtk_tree_store_new (DC_N, G_TYPE_BOOLEAN, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
				       G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN);

	outer = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
	gtk_container_set_border_width (GTK_CONTAINER (outer), 12);
	gtk_box_pack_start (GTK_BOX (outer), nolphin_tools_back_button_new (window), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (outer), nolphin_tools_heading_new (_("Duplikate finden")), FALSE, FALSE, 0);

	intro = gtk_label_new (_("Sucht in einem Ordner (samt Unterordnern) nach Dateien mit gleichem Namen, gleicher Größe oder gleichem Inhalt. "
				 "Gelöscht wird nur in den Papierkorb und nur nach Bestätigung."));
	gtk_label_set_line_wrap (GTK_LABEL (intro), TRUE);
	gtk_widget_set_halign (intro, GTK_ALIGN_START);
	gtk_style_context_add_class (gtk_widget_get_style_context (intro), "dim-label");
	gtk_box_pack_start (GTK_BOX (outer), intro, FALSE, FALSE, 0);

	row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	d->folder_label = gtk_label_new (_("(kein Ordner gewählt)"));
	gtk_label_set_ellipsize (GTK_LABEL (d->folder_label), PANGO_ELLIPSIZE_MIDDLE);
	gtk_widget_set_halign (d->folder_label, GTK_ALIGN_START);
	gtk_box_pack_start (GTK_BOX (row), d->folder_label, TRUE, TRUE, 0);
	b = gtk_button_new_with_label (_("Ordner wählen …"));
	g_signal_connect (b, "clicked", G_CALLBACK (on_dup_choose_folder), d);
	gtk_box_pack_start (GTK_BOX (row), b, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (outer), row, FALSE, FALSE, 0);

	row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	d->mode_combo = gtk_combo_box_text_new ();
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (d->mode_combo), _("Gleicher Name"));
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (d->mode_combo), _("Gleiche Größe"));
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (d->mode_combo), _("Gleicher Inhalt (Prüfsumme)"));
	gtk_combo_box_set_active (GTK_COMBO_BOX (d->mode_combo), MODE_CHECKSUM);
	gtk_box_pack_start (GTK_BOX (row), d->mode_combo, TRUE, TRUE, 0);
	d->hidden_check = gtk_check_button_new_with_label (_("Versteckte Dateien"));
	gtk_box_pack_start (GTK_BOX (row), d->hidden_check, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (outer), row, FALSE, FALSE, 0);

	row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	d->start_button = gtk_button_new_with_label (_("Suchen"));
	gtk_style_context_add_class (gtk_widget_get_style_context (d->start_button), "suggested-action");
	g_signal_connect (d->start_button, "clicked", G_CALLBACK (on_dup_start), d);
	gtk_box_pack_start (GTK_BOX (row), d->start_button, FALSE, FALSE, 0);
	d->cancel_button = gtk_button_new_with_label (_("Abbrechen"));
	g_signal_connect (d->cancel_button, "clicked", G_CALLBACK (on_dup_cancel), d);
	gtk_box_pack_start (GTK_BOX (row), d->cancel_button, FALSE, FALSE, 0);
	d->spinner = gtk_spinner_new ();
	gtk_box_pack_start (GTK_BOX (row), d->spinner, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (outer), row, FALSE, FALSE, 0);

	d->tree = gtk_tree_view_new_with_model (GTK_TREE_MODEL (d->store));
	g_object_set_data (G_OBJECT (d->tree), "dup-page", outer);
	renderer = gtk_cell_renderer_toggle_new ();
	g_signal_connect (renderer, "toggled", G_CALLBACK (on_dup_toggled), d);
	/* Das Kästchen nur bei Dateien, nicht bei Gruppenzeilen zeigen */
	column = gtk_tree_view_column_new_with_attributes ("", renderer, "active", DC_CHECK, "visible", DC_IS_FILE, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (d->tree), column);
	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer, "ellipsize", PANGO_ELLIPSIZE_MIDDLE, NULL);
	column = gtk_tree_view_column_new_with_attributes (_("Datei"), renderer, "text", DC_NAME, NULL);
	gtk_tree_view_column_set_expand (column, TRUE);
	gtk_tree_view_append_column (GTK_TREE_VIEW (d->tree), column);
	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Größe"), renderer, "text", DC_SIZE, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (d->tree), column);
	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Geändert"), renderer, "text", DC_DATE, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (d->tree), column);

	tree_scroller = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (tree_scroller), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_widget_set_size_request (tree_scroller, -1, 260);
	gtk_container_add (GTK_CONTAINER (tree_scroller), d->tree);
	gtk_box_pack_start (GTK_BOX (outer), tree_scroller, TRUE, TRUE, 0);

	row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	b = gtk_button_new_with_label (_("Alle außer der ersten markieren"));
	g_signal_connect (b, "clicked", G_CALLBACK (on_dup_mark_extras), d);
	gtk_box_pack_start (GTK_BOX (row), b, FALSE, FALSE, 0);
	d->trash_button = gtk_button_new_with_label (_("Markierte in den Papierkorb"));
	g_signal_connect (d->trash_button, "clicked", G_CALLBACK (on_dup_trash), d);
	gtk_box_pack_start (GTK_BOX (row), d->trash_button, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (outer), row, FALSE, FALSE, 0);

	d->status = gtk_label_new ("");
	gtk_label_set_line_wrap (GTK_LABEL (d->status), TRUE);
	gtk_widget_set_halign (d->status, GTK_ALIGN_START);
	gtk_box_pack_start (GTK_BOX (outer), d->status, FALSE, FALSE, 0);

	scroller = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroller), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (scroller), outer);
	g_object_set_data_full (G_OBJECT (scroller), "dup-data", d, dup_page_free);
	g_object_set_data (G_OBJECT (outer), "dup-data", d);
	g_object_set_data (G_OBJECT (outer), "dup-scroller", scroller);
	dup_set_busy (d, FALSE);
	gtk_widget_set_sensitive (d->cancel_button, FALSE);

	return scroller;
}

static void
dup_set_folder (DupPage *d, GFile *folder)
{
	gchar *name;

	g_clear_object (&d->folder);
	d->folder = g_object_ref (folder);
	name = g_file_get_parse_name (folder);
	gtk_label_set_text (GTK_LABEL (d->folder_label), name);
	g_free (name);
	gtk_tree_store_clear (d->store);
	dup_set_status (d, NULL);
	dup_set_busy (d, FALSE);
}

/* @page ist die von nolphin_duplicates_page_new() gelieferte Seite. */
void
nolphin_duplicates_page_open (GtkWidget *page, GFile *folder)
{
	DupPage *d = page != NULL ? g_object_get_data (G_OBJECT (page), "dup-data") : NULL;

	if (d != NULL && folder != NULL) {
		dup_set_folder (d, folder);
	}
}
