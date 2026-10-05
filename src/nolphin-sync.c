/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nolphin-sync.c: Ordnersynchronisation über rsync (§44)
 *
 * Vergleicht zwei Ordner (Trockenlauf), zeigt die Unterschiede und
 * synchronisiert einseitig: lokal → Ziel oder Ziel → lokal. Konflikte (die
 * Zieldatei ist neuer) entscheidet der Benutzer pro Datei: Quelle behalten,
 * Ziel behalten oder beide behalten. Es werden nie Dateien im Ziel gelöscht.
 */

#include <config.h>

#include "nolphin-tools.h"

#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <string.h>
#include <sys/stat.h>

enum { SC_APPLY, SC_ACTION, SC_PATH, SC_CHOICE, SC_CONFLICT, SC_N };
enum { CHOICE_SOURCE, CHOICE_TARGET, CHOICE_BOTH };

typedef struct {
	NolphinWindow *window;
	GFile *local;          /* lokaler Ordner */
	GFile *remote;         /* Ziel- bzw. Gegenordner */
	GtkWidget *local_label;
	GtkWidget *remote_label;
	GtkWidget *direction_combo;
	GtkWidget *checksum_check;
	GtkWidget *compare_button;
	GtkWidget *sync_button;
	GtkWidget *spinner;
	GtkWidget *status;
	GtkWidget *tree;
	GtkListStore *store;
	GtkListStore *choice_model;
	gboolean busy;
	gboolean compared;
} SyncPage;

static void
sync_set_status (SyncPage *d, const gchar *text)
{
	gtk_label_set_text (GTK_LABEL (d->status), text != NULL ? text : "");
}

static void
sync_update_buttons (SyncPage *d)
{
	gboolean ready = d->local != NULL && d->remote != NULL && !d->busy;

	gtk_widget_set_sensitive (d->compare_button, ready);
	gtk_widget_set_sensitive (d->sync_button, ready && d->compared);
	if (d->busy) {
		gtk_spinner_start (GTK_SPINNER (d->spinner));
	} else {
		gtk_spinner_stop (GTK_SPINNER (d->spinner));
	}
}

/* Quelle und Ziel nach gewählter Richtung (0: lokal → Ziel, 1: Ziel → lokal). Pfade mit "/" am Ende. */
static gboolean
sync_paths (SyncPage *d, gchar **src, gchar **dst)
{
	gboolean reverse = gtk_combo_box_get_active (GTK_COMBO_BOX (d->direction_combo)) == 1;
	gchar *l = d->local != NULL ? g_file_get_path (d->local) : NULL;
	gchar *r = d->remote != NULL ? g_file_get_path (d->remote) : NULL;

	if (l == NULL || r == NULL) {
		g_free (l);
		g_free (r);
		return FALSE;
	}
	*src = g_strconcat (reverse ? r : l, "/", NULL);
	*dst = g_strconcat (reverse ? l : r, "/", NULL);
	g_free (l);
	g_free (r);
	return TRUE;
}

/* --- Vergleichen (rsync -n) --------------------------------------------------- */

static gint64
mtime_of (const gchar *base, const gchar *rel)
{
	gchar *full = g_strconcat (base, rel, NULL);
	GStatBuf st;
	gint64 t = -1;

	if (g_stat (full, &st) == 0) {
		t = (gint64) st.st_mtime;
	}
	g_free (full);
	return t;
}

static void
compare_done (GObject *source, GAsyncResult *result, gpointer user_data)
{
	SyncPage *d = user_data;
	GBytes *out = NULL;
	GError *error = NULL;
	gchar *src = NULL, *dst = NULL;
	guint count = 0, conflicts = 0;

	g_subprocess_communicate_finish (G_SUBPROCESS (source), result, &out, NULL, &error);
	d->busy = FALSE;

	if (error != NULL || !g_subprocess_get_successful (G_SUBPROCESS (source))) {
		sync_set_status (d, error != NULL ? error->message : _("rsync meldete einen Fehler."));
		g_clear_error (&error);
		g_clear_pointer (&out, g_bytes_unref);
		d->compared = FALSE;
		sync_update_buttons (d);
		return;
	}

	sync_paths (d, &src, &dst);
	gtk_list_store_clear (d->store);
	if (out != NULL) {
		gsize len = 0;
		const gchar *data = g_bytes_get_data (out, &len);
		gchar *text = g_utf8_make_valid (data != NULL ? data : "", len);
		gchar **lines = g_strsplit (text, "\n", -1);
		guint i;

		for (i = 0; lines[i] != NULL; i++) {
			const gchar *line = lines[i];
			gchar flags[16];
			const gchar *rel;
			gboolean is_new, conflict = FALSE;
			GtkTreeIter iter;

			/* Dateien: ">f.st...... pfad" (Quelle → Ziel) */
			if (strlen (line) < 13 || line[0] != '>' || line[1] != 'f') {
				continue;
			}
			memcpy (flags, line, 11);
			flags[11] = '\0';
			rel = line + 12;
			is_new = strchr (flags, '+') != NULL;

			if (!is_new) {
				gint64 s = mtime_of (src, rel), t = mtime_of (dst, rel);

				conflict = t > s && s >= 0;
			}
			gtk_list_store_append (d->store, &iter);
			gtk_list_store_set (d->store, &iter,
					    SC_APPLY, !conflict,
					    SC_ACTION, is_new ? _("Neu") : (conflict ? _("Konflikt: Ziel ist neuer") : _("Geändert")),
					    SC_PATH, rel,
					    SC_CHOICE, CHOICE_SOURCE,
					    SC_CONFLICT, conflict, -1);
			count++;
			if (conflict) {
				conflicts++;
			}
		}
		g_strfreev (lines);
		g_free (text);
	}
	g_free (src);
	g_free (dst);
	g_clear_pointer (&out, g_bytes_unref);

	d->compared = count > 0;
	if (count == 0) {
		sync_set_status (d, _("Keine Unterschiede: Ziel und Quelle sind gleich."));
	} else {
		gchar *msg = g_strdup_printf (_("%u Unterschiede, davon %u Konflikte. Das ist nur eine Vorschau – es wurde noch nichts verändert."),
					      count, conflicts);

		sync_set_status (d, msg);
		g_free (msg);
	}
	sync_update_buttons (d);
}

static void
on_sync_compare (GtkButton *button, gpointer data)
{
	SyncPage *d = data;
	gchar *src, *dst;
	GPtrArray *argv;
	GSubprocess *proc;
	GError *error = NULL;

	if (g_find_program_in_path ("rsync") == NULL) {
		sync_set_status (d, _("Das Werkzeug »rsync« ist nicht installiert."));
		return;
	}
	if (!sync_paths (d, &src, &dst)) {
		sync_set_status (d, _("Beide Ordner müssen lokal erreichbar sein (eingebundene Netzwerkordner sind möglich)."));
		return;
	}

	argv = g_ptr_array_new ();
	g_ptr_array_add (argv, "rsync");
	g_ptr_array_add (argv, "-rtn");
	g_ptr_array_add (argv, "--itemize-changes");
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (d->checksum_check))) {
		g_ptr_array_add (argv, "-c");
	}
	g_ptr_array_add (argv, src);
	g_ptr_array_add (argv, dst);
	g_ptr_array_add (argv, NULL);

	proc = g_subprocess_newv ((const gchar * const *) argv->pdata,
				  G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_SILENCE, &error);
	g_ptr_array_free (argv, TRUE);
	g_free (src);
	g_free (dst);
	if (proc == NULL) {
		sync_set_status (d, error->message);
		g_clear_error (&error);
		return;
	}
	d->busy = TRUE;
	d->compared = FALSE;
	sync_set_status (d, _("Wird verglichen (Trockenlauf) …"));
	sync_update_buttons (d);
	g_subprocess_communicate_async (proc, NULL, NULL, compare_done, d);
	g_object_unref (proc);
}

/* --- Synchronisieren -------------------------------------------------------------- */

typedef struct {
	GString *normal;   /* Quelle behalten: überschreiben */
	GString *both;     /* beide behalten: Ziel wird gesichert */
	guint n_normal, n_both, n_skipped;
} SyncPlan;

static gboolean
plan_row_cb (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
	SyncPlan *p = data;
	gboolean apply;
	gint choice;
	gchar *rel = NULL;

	gtk_tree_model_get (model, iter, SC_APPLY, &apply, SC_PATH, &rel, SC_CHOICE, &choice, -1);
	if (!apply || choice == CHOICE_TARGET) {
		p->n_skipped++;
	} else if (choice == CHOICE_BOTH) {
		g_string_append_printf (p->both, "%s\n", rel);
		p->n_both++;
	} else {
		g_string_append_printf (p->normal, "%s\n", rel);
		p->n_normal++;
	}
	g_free (rel);
	return FALSE;
}

static gboolean
run_rsync_files (const gchar *src, const gchar *dst, const gchar *list, gboolean keep_both, gchar **error_text)
{
	gchar *tmp_path = NULL;
	gint fd = g_file_open_tmp ("nolphin-sync-XXXXXX", &tmp_path, NULL);
	GPtrArray *argv = g_ptr_array_new ();
	GSubprocess *proc;
	GError *error = NULL;
	gchar *files_from, *stderr_text = NULL;
	gboolean ok = FALSE;
	GBytes *err = NULL;

	if (fd < 0) {
		*error_text = g_strdup (_("Temporäre Datei konnte nicht angelegt werden."));
		g_ptr_array_free (argv, TRUE);
		return FALSE;
	}
	write (fd, list, strlen (list));
	close (fd);

	files_from = g_strdup_printf ("--files-from=%s", tmp_path);
	g_ptr_array_add (argv, "rsync");
	g_ptr_array_add (argv, "-tp");
	if (keep_both) {
		g_ptr_array_add (argv, "--backup");
		g_ptr_array_add (argv, "--suffix=.nolphin-ziel");
	}
	g_ptr_array_add (argv, files_from);
	g_ptr_array_add (argv, (gpointer) src);
	g_ptr_array_add (argv, (gpointer) dst);
	g_ptr_array_add (argv, NULL);

	proc = g_subprocess_newv ((const gchar * const *) argv->pdata,
				  G_SUBPROCESS_FLAGS_STDOUT_SILENCE | G_SUBPROCESS_FLAGS_STDERR_PIPE, &error);
	if (proc != NULL) {
		if (g_subprocess_communicate (proc, NULL, NULL, NULL, &err, &error)) {
			ok = g_subprocess_get_successful (proc);
			if (!ok && err != NULL) {
				stderr_text = g_strndup (g_bytes_get_data (err, NULL), g_bytes_get_size (err));
			}
		}
		g_object_unref (proc);
	}
	if (!ok) {
		*error_text = stderr_text != NULL ? stderr_text : g_strdup (error != NULL ? error->message : _("rsync meldete einen Fehler."));
	}
	g_clear_error (&error);
	g_clear_pointer (&err, g_bytes_unref);
	g_remove (tmp_path);
	g_free (tmp_path);
	g_free (files_from);
	g_ptr_array_free (argv, TRUE);
	return ok;
}

static void
on_sync_apply (GtkButton *button, gpointer data)
{
	SyncPage *d = data;
	SyncPlan plan = { g_string_new (NULL), g_string_new (NULL), 0, 0, 0 };
	gchar *src, *dst, *err = NULL;
	GtkWidget *toplevel = gtk_widget_get_toplevel (GTK_WIDGET (button));
	GtkWidget *dialog;
	gboolean proceed = FALSE, ok = TRUE;

	if (!sync_paths (d, &src, &dst)) {
		g_string_free (plan.normal, TRUE);
		g_string_free (plan.both, TRUE);
		return;
	}
	gtk_tree_model_foreach (GTK_TREE_MODEL (d->store), plan_row_cb, &plan);

	if (plan.n_normal + plan.n_both == 0) {
		sync_set_status (d, _("Nichts zu übernehmen: alle Zeilen sind abgewählt oder auf »Ziel behalten« gesetzt."));
		goto out;
	}

	dialog = gtk_message_dialog_new (GTK_IS_WINDOW (toplevel) ? GTK_WINDOW (toplevel) : NULL,
					 GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
					 "%s", _("Jetzt synchronisieren?"));
	{
		gchar *sec = g_strdup_printf (_("%u Datei(en) werden übernommen, %u davon mit gesichertem Ziel (».nolphin-ziel«), %u übersprungen. "
						"Im Ziel wird nie etwas gelöscht."),
					      plan.n_normal + plan.n_both, plan.n_both, plan.n_skipped);

		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", sec);
		g_free (sec);
	}
	gtk_dialog_add_buttons (GTK_DIALOG (dialog), _("_Abbrechen"), GTK_RESPONSE_CANCEL,
				_("_Synchronisieren"), GTK_RESPONSE_OK, NULL);
	proceed = gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK;
	gtk_widget_destroy (dialog);
	if (!proceed) {
		goto out;
	}

	if (plan.n_normal > 0) {
		ok = run_rsync_files (src, dst, plan.normal->str, FALSE, &err);
	}
	if (ok && plan.n_both > 0) {
		ok = run_rsync_files (src, dst, plan.both->str, TRUE, &err);
	}
	if (ok) {
		gchar *msg = g_strdup_printf (_("Synchronisiert: %u Datei(en) übernommen. Zum Prüfen erneut vergleichen."),
					      plan.n_normal + plan.n_both);

		sync_set_status (d, msg);
		g_free (msg);
		gtk_list_store_clear (d->store);
		d->compared = FALSE;
		sync_update_buttons (d);
	} else {
		sync_set_status (d, err != NULL ? err : _("Synchronisieren fehlgeschlagen."));
	}
out:
	g_free (err);
	g_free (src);
	g_free (dst);
	g_string_free (plan.normal, TRUE);
	g_string_free (plan.both, TRUE);
}

/* --- Oberfläche -------------------------------------------------------------------- */

static void
on_sync_toggled (GtkCellRendererToggle *cell, gchar *path_str, gpointer data)
{
	SyncPage *d = data;
	GtkTreeIter iter;
	gboolean v;

	if (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (d->store), &iter, path_str)) {
		gtk_tree_model_get (GTK_TREE_MODEL (d->store), &iter, SC_APPLY, &v, -1);
		gtk_list_store_set (d->store, &iter, SC_APPLY, !v, -1);
	}
}

static void
on_sync_choice_changed (GtkCellRendererCombo *cell, gchar *path_str, GtkTreeIter *new_iter, gpointer data)
{
	SyncPage *d = data;
	GtkTreeIter iter;
	gint idx;
	GtkTreePath *p = gtk_tree_model_get_path (GTK_TREE_MODEL (d->choice_model), new_iter);

	idx = gtk_tree_path_get_indices (p)[0];
	gtk_tree_path_free (p);
	if (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (d->store), &iter, path_str)) {
		gtk_list_store_set (d->store, &iter, SC_CHOICE, idx, SC_APPLY, idx != CHOICE_TARGET, -1);
	}
}

static void
choice_cell_data (GtkTreeViewColumn *col, GtkCellRenderer *cell, GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
	gint choice;
	gboolean conflict;
	static const gchar *names[3];

	names[0] = _("Quelle behalten");
	names[1] = _("Ziel behalten");
	names[2] = _("Beide behalten");
	gtk_tree_model_get (model, iter, SC_CHOICE, &choice, SC_CONFLICT, &conflict, -1);
	g_object_set (cell, "text", names[CLAMP (choice, 0, 2)], "editable", conflict, NULL);
}

static void
sync_folder_chosen (SyncPage *d, gboolean for_local, GFile *f)
{
	gchar *name = g_file_get_parse_name (f);

	if (for_local) {
		g_clear_object (&d->local);
		d->local = g_object_ref (f);
		gtk_label_set_text (GTK_LABEL (d->local_label), name);
	} else {
		g_clear_object (&d->remote);
		d->remote = g_object_ref (f);
		gtk_label_set_text (GTK_LABEL (d->remote_label), name);
	}
	g_free (name);
	gtk_list_store_clear (d->store);
	d->compared = FALSE;
	sync_set_status (d, NULL);
	sync_update_buttons (d);
}

static void
on_sync_choose (GtkButton *button, gpointer data)
{
	SyncPage *d = data;
	gboolean for_local = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (button), "for-local"));
	GtkWidget *toplevel = gtk_widget_get_toplevel (GTK_WIDGET (button));
	GtkWidget *chooser = gtk_file_chooser_dialog_new (for_local ? _("Lokalen Ordner wählen") : _("Zielordner wählen"),
							  GTK_IS_WINDOW (toplevel) ? GTK_WINDOW (toplevel) : NULL,
							  GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
							  _("_Abbrechen"), GTK_RESPONSE_CANCEL,
							  _("_Auswählen"), GTK_RESPONSE_ACCEPT, NULL);

	if (gtk_dialog_run (GTK_DIALOG (chooser)) == GTK_RESPONSE_ACCEPT) {
		GFile *f = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (chooser));

		if (f != NULL) {
			sync_folder_chosen (d, for_local, f);
			g_object_unref (f);
		}
	}
	gtk_widget_destroy (chooser);
}

static void
sync_page_free (gpointer data)
{
	SyncPage *d = data;

	g_clear_object (&d->local);
	g_clear_object (&d->remote);
	g_free (d);
}

static GtkWidget *
folder_row (SyncPage *d, const gchar *caption, gboolean for_local, GtkWidget **out_label)
{
	GtkWidget *row = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
	GtkWidget *line = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	GtkWidget *cap = gtk_label_new (caption);
	GtkWidget *label = gtk_label_new (_("(kein Ordner gewählt)"));
	GtkWidget *b = gtk_button_new_with_label (_("Wählen …"));

	gtk_widget_set_halign (cap, GTK_ALIGN_START);
	gtk_style_context_add_class (gtk_widget_get_style_context (cap), "dim-label");
	gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_MIDDLE);
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	g_object_set_data (G_OBJECT (b), "for-local", GINT_TO_POINTER (for_local));
	g_signal_connect (b, "clicked", G_CALLBACK (on_sync_choose), d);
	gtk_box_pack_start (GTK_BOX (line), label, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (line), b, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (row), cap, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (row), line, FALSE, FALSE, 0);
	*out_label = label;
	return row;
}

GtkWidget *
nolphin_sync_page_new (NolphinWindow *window)
{
	SyncPage *d = g_new0 (SyncPage, 1);
	GtkWidget *outer, *scroller, *tree_scroller, *row, *intro;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkTreeIter it;

	d->window = window;
	d->store = gtk_list_store_new (SC_N, G_TYPE_BOOLEAN, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT, G_TYPE_BOOLEAN);
	d->choice_model = gtk_list_store_new (1, G_TYPE_STRING);
	gtk_list_store_append (d->choice_model, &it);
	gtk_list_store_set (d->choice_model, &it, 0, _("Quelle behalten"), -1);
	gtk_list_store_append (d->choice_model, &it);
	gtk_list_store_set (d->choice_model, &it, 0, _("Ziel behalten"), -1);
	gtk_list_store_append (d->choice_model, &it);
	gtk_list_store_set (d->choice_model, &it, 0, _("Beide behalten"), -1);

	outer = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
	gtk_container_set_border_width (GTK_CONTAINER (outer), 12);
	gtk_box_pack_start (GTK_BOX (outer), nolphin_tools_back_button_new (window), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (outer), nolphin_tools_heading_new (_("Ordner vergleichen und synchronisieren")), FALSE, FALSE, 0);

	intro = gtk_label_new (_("Vergleicht zwei Ordner (über rsync) und kopiert nur das, was neu oder geändert ist, in eine Richtung. "
				 "Zuerst gibt es immer eine Vorschau; im Ziel wird nie etwas gelöscht."));
	gtk_label_set_line_wrap (GTK_LABEL (intro), TRUE);
	gtk_widget_set_halign (intro, GTK_ALIGN_START);
	gtk_style_context_add_class (gtk_widget_get_style_context (intro), "dim-label");
	gtk_box_pack_start (GTK_BOX (outer), intro, FALSE, FALSE, 0);

	gtk_box_pack_start (GTK_BOX (outer), folder_row (d, _("Lokaler Ordner"), TRUE, &d->local_label), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (outer), folder_row (d, _("Zielordner (auch eingebundener Netzwerkordner)"), FALSE, &d->remote_label), FALSE, FALSE, 0);

	d->direction_combo = gtk_combo_box_text_new ();
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (d->direction_combo), _("Lokal → Ziel"));
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (d->direction_combo), _("Ziel → Lokal"));
	gtk_combo_box_set_active (GTK_COMBO_BOX (d->direction_combo), 0);
	gtk_box_pack_start (GTK_BOX (outer), d->direction_combo, FALSE, FALSE, 0);

	d->checksum_check = gtk_check_button_new_with_label (_("Inhalt prüfen (Prüfsumme, langsamer)"));
	gtk_box_pack_start (GTK_BOX (outer), d->checksum_check, FALSE, FALSE, 0);

	row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	d->compare_button = gtk_button_new_with_label (_("Vergleichen (Vorschau)"));
	gtk_style_context_add_class (gtk_widget_get_style_context (d->compare_button), "suggested-action");
	g_signal_connect (d->compare_button, "clicked", G_CALLBACK (on_sync_compare), d);
	gtk_box_pack_start (GTK_BOX (row), d->compare_button, FALSE, FALSE, 0);
	d->sync_button = gtk_button_new_with_label (_("Synchronisieren"));
	g_signal_connect (d->sync_button, "clicked", G_CALLBACK (on_sync_apply), d);
	gtk_box_pack_start (GTK_BOX (row), d->sync_button, FALSE, FALSE, 0);
	d->spinner = gtk_spinner_new ();
	gtk_box_pack_start (GTK_BOX (row), d->spinner, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (outer), row, FALSE, FALSE, 0);

	d->tree = gtk_tree_view_new_with_model (GTK_TREE_MODEL (d->store));
	renderer = gtk_cell_renderer_toggle_new ();
	g_signal_connect (renderer, "toggled", G_CALLBACK (on_sync_toggled), d);
	column = gtk_tree_view_column_new_with_attributes (_("Übernehmen"), renderer, "active", SC_APPLY, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (d->tree), column);
	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer, "ellipsize", PANGO_ELLIPSIZE_MIDDLE, NULL);
	column = gtk_tree_view_column_new_with_attributes (_("Datei"), renderer, "text", SC_PATH, NULL);
	gtk_tree_view_column_set_expand (column, TRUE);
	gtk_tree_view_append_column (GTK_TREE_VIEW (d->tree), column);
	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Art"), renderer, "text", SC_ACTION, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (d->tree), column);
	renderer = gtk_cell_renderer_combo_new ();
	g_object_set (renderer, "model", d->choice_model, "text-column", 0, "has-entry", FALSE, NULL);
	g_signal_connect (renderer, "changed", G_CALLBACK (on_sync_choice_changed), d);
	column = gtk_tree_view_column_new_with_attributes (_("Entscheidung"), renderer, NULL);
	gtk_tree_view_column_set_cell_data_func (column, renderer, choice_cell_data, d, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (d->tree), column);

	tree_scroller = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (tree_scroller), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_widget_set_size_request (tree_scroller, -1, 220);
	gtk_container_add (GTK_CONTAINER (tree_scroller), d->tree);
	gtk_box_pack_start (GTK_BOX (outer), tree_scroller, TRUE, TRUE, 0);

	d->status = gtk_label_new ("");
	gtk_label_set_line_wrap (GTK_LABEL (d->status), TRUE);
	gtk_widget_set_halign (d->status, GTK_ALIGN_START);
	gtk_box_pack_start (GTK_BOX (outer), d->status, FALSE, FALSE, 0);

	scroller = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroller), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (scroller), outer);
	g_object_set_data_full (G_OBJECT (scroller), "sync-data", d, sync_page_free);
	sync_update_buttons (d);
	return scroller;
}

void
nolphin_sync_page_open (GtkWidget *page, GFile *local_folder)
{
	SyncPage *d = page != NULL ? g_object_get_data (G_OBJECT (page), "sync-data") : NULL;

	if (d != NULL && local_folder != NULL) {
		sync_folder_chosen (d, TRUE, local_folder);
	}
}
