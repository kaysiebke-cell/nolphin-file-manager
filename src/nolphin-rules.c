/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nolphin-rules.c: Regeln anwenden und Stapelverarbeitung (§43)
 *
 * "Wenn Dateityp/Name/Größe/Datum … dann Aktion …": wird nur manuell auf einen
 * gewählten Ordner (Regeln) oder auf die aktuelle Auswahl (Stapelverarbeitung)
 * angewendet, mit Vorschau der betroffenen Dateien vor der Ausführung.
 * Befehle laufen ohne Shell, einzeln pro Datei.
 */

#include <config.h>

#include "nolphin-tools.h"

#include <glib/gi18n.h>
#include <string.h>
#include <libnolphin-private/nolphin-archive.h>

#define MAX_MATCHES 50000

typedef enum { CAT_ALL, CAT_IMAGE, CAT_VIDEO, CAT_AUDIO, CAT_TEXT, CAT_ARCHIVE, CAT_DOC } Category;
typedef enum { ACT_MOVE, ACT_COPY, ACT_TRASH, ACT_COMMAND } ActionKind;

typedef struct {
	Category category;
	gchar *name_pattern;       /* leer = egal */
	gint size_op;              /* 0 egal, 1 größer als, 2 kleiner als */
	gint64 size_kib;
	gint date_op;              /* 0 egal, 1 älter als, 2 neuer als */
	gint days;
	gboolean recursive;
	/* Quelle */
	gchar *folder;             /* Ordnerpfad oder NULL */
	GList *paths;              /* Auswahl (gchar *) oder NULL */
} RuleCfg;

static void
rule_cfg_free (RuleCfg *c)
{
	g_free (c->name_pattern);
	g_free (c->folder);
	g_list_free_full (c->paths, g_free);
	g_free (c);
}

/* --- Filter --------------------------------------------------------------------- */

static gboolean
category_matches (Category cat, const gchar *ct, const gchar *path)
{
	if (cat == CAT_ALL) {
		return TRUE;
	}
	if (ct == NULL) {
		return FALSE;
	}
	switch (cat) {
	case CAT_IMAGE:   return g_str_has_prefix (ct, "image/");
	case CAT_VIDEO:   return g_str_has_prefix (ct, "video/");
	case CAT_AUDIO:   return g_str_has_prefix (ct, "audio/");
	case CAT_TEXT:    return g_content_type_is_a (ct, "text/plain");
	case CAT_DOC:     return g_strcmp0 (ct, "application/pdf") == 0 || g_strcmp0 (ct, "application/msword") == 0 ||
				 g_strcmp0 (ct, "application/rtf") == 0 || g_str_has_prefix (ct, "application/vnd.");
	case CAT_ARCHIVE: {
		GFile *f = g_file_new_for_path (path);
		gboolean is = nolphin_archive_detect_format (f) != NOLPHIN_ARCHIVE_FORMAT_UNKNOWN;

		g_object_unref (f);
		return is;
	}
	default:          return TRUE;
	}
}

static gboolean
file_matches (RuleCfg *c, const gchar *path, GFileInfo *info)
{
	goffset size = g_file_info_get_size (info);
	gint64 mtime = (gint64) g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_TIME_MODIFIED);
	gchar *base;

	if (!category_matches (c->category, g_file_info_get_content_type (info), path)) {
		return FALSE;
	}
	if (c->name_pattern != NULL && c->name_pattern[0] != '\0') {
		gchar *a, *b;
		gboolean ok;

		base = g_path_get_basename (path);
		a = g_utf8_casefold (c->name_pattern, -1);
		b = g_utf8_casefold (base, -1);
		ok = g_pattern_match_simple (a, b);
		g_free (a);
		g_free (b);
		g_free (base);
		if (!ok) {
			return FALSE;
		}
	}
	if (c->size_op == 1 && size <= c->size_kib * 1024) {
		return FALSE;
	}
	if (c->size_op == 2 && size >= c->size_kib * 1024) {
		return FALSE;
	}
	if (c->date_op != 0) {
		gint64 cutoff = (gint64) time (NULL) - (gint64) c->days * 86400;

		if (c->date_op == 1 && mtime >= cutoff) {
			return FALSE;
		}
		if (c->date_op == 2 && mtime < cutoff) {
			return FALSE;
		}
	}
	return TRUE;
}

#define INFO_ATTRS G_FILE_ATTRIBUTE_STANDARD_NAME "," G_FILE_ATTRIBUTE_STANDARD_TYPE "," \
		   G_FILE_ATTRIBUTE_STANDARD_SIZE "," G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE "," \
		   G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK "," G_FILE_ATTRIBUTE_TIME_MODIFIED

static void
scan_dir (RuleCfg *c, GFile *dir, GPtrArray *out, GCancellable *cancellable)
{
	GFileEnumerator *en = g_file_enumerate_children (dir, INFO_ATTRS, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, cancellable, NULL);
	GFileInfo *info;

	if (en == NULL) {
		return;
	}
	while (out->len < MAX_MATCHES && (info = g_file_enumerator_next_file (en, cancellable, NULL)) != NULL) {
		GFile *child = g_file_get_child (dir, g_file_info_get_name (info));
		gchar *path = g_file_get_path (child);

		if (!g_file_info_get_is_symlink (info)) {
			if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY) {
				if (c->recursive) {
					scan_dir (c, child, out, cancellable);
				}
			} else if (g_file_info_get_file_type (info) == G_FILE_TYPE_REGULAR && path != NULL &&
				   file_matches (c, path, info)) {
				g_ptr_array_add (out, g_strdup (path));
			}
		}
		g_free (path);
		g_object_unref (child);
		g_object_unref (info);
		if (g_cancellable_is_cancelled (cancellable)) {
			break;
		}
	}
	g_object_unref (en);
}

static void
match_thread (GTask *task, gpointer source, gpointer task_data, GCancellable *cancellable)
{
	RuleCfg *c = task_data;
	GPtrArray *out = g_ptr_array_new_with_free_func (g_free);

	if (c->paths != NULL) {
		GList *l;

		for (l = c->paths; l != NULL; l = l->next) {
			GFile *f = g_file_new_for_path (l->data);
			GFileInfo *info = g_file_query_info (f, INFO_ATTRS, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL, NULL);

			if (info != NULL && g_file_info_get_file_type (info) == G_FILE_TYPE_REGULAR &&
			    file_matches (c, l->data, info)) {
				g_ptr_array_add (out, g_strdup (l->data));
			}
			g_clear_object (&info);
			g_object_unref (f);
		}
	} else if (c->folder != NULL) {
		GFile *dir = g_file_new_for_path (c->folder);

		scan_dir (c, dir, out, cancellable);
		g_object_unref (dir);
	}
	g_task_return_pointer (task, out, (GDestroyNotify) g_ptr_array_unref);
}

/* --- Ausführen -------------------------------------------------------------------- */

typedef struct {
	ActionKind kind;
	gchar *target_dir;     /* Verschieben/Kopieren */
	gchar *command;        /* Befehl mit %f */
	GList *paths;          /* gchar* */
} ExecJob;

static void
exec_job_free (ExecJob *j)
{
	g_free (j->target_dir);
	g_free (j->command);
	g_list_free_full (j->paths, g_free);
	g_free (j);
}

/* freier Zielname: "name (1).ext" usw. */
static GFile *
unique_target (GFile *dir, const gchar *basename)
{
	GFile *f = g_file_get_child (dir, basename);
	guint n;

	for (n = 1; g_file_query_exists (f, NULL) && n < 1000; n++) {
		gchar *dot = strrchr (basename, '.');
		gchar *name;

		g_object_unref (f);
		if (dot != NULL && dot != basename) {
			gchar *stem = g_strndup (basename, dot - basename);

			name = g_strdup_printf ("%s (%u)%s", stem, n, dot);
			g_free (stem);
		} else {
			name = g_strdup_printf ("%s (%u)", basename, n);
		}
		f = g_file_get_child (dir, name);
		g_free (name);
	}
	return f;
}

static gchar *
run_command (const gchar *tmpl, const gchar *path)
{
	gchar **argv = NULL;
	GError *error = NULL;
	gint n, i;
	GPtrArray *final = g_ptr_array_new_with_free_func (g_free);
	GSubprocess *proc;
	gchar *result = NULL;
	gboolean has_placeholder = FALSE;

	if (!g_shell_parse_argv (tmpl, &n, &argv, &error)) {
		result = g_strdup (error->message);
		g_clear_error (&error);
		g_ptr_array_free (final, TRUE);
		return result;
	}
	for (i = 0; i < n; i++) {
		if (strstr (argv[i], "%f") != NULL) {
			gchar **parts = g_strsplit (argv[i], "%f", -1);

			g_ptr_array_add (final, g_strjoinv (path, parts));
			g_strfreev (parts);
			has_placeholder = TRUE;
		} else {
			g_ptr_array_add (final, g_strdup (argv[i]));
		}
	}
	if (!has_placeholder) {
		g_ptr_array_add (final, g_strdup (path));
	}
	g_ptr_array_add (final, NULL);

	proc = g_subprocess_newv ((const gchar * const *) final->pdata,
				  G_SUBPROCESS_FLAGS_STDOUT_SILENCE | G_SUBPROCESS_FLAGS_STDERR_SILENCE, &error);
	if (proc == NULL) {
		result = g_strdup (error->message);
		g_clear_error (&error);
	} else {
		if (!g_subprocess_wait (proc, NULL, &error)) {
			result = g_strdup (error->message);
			g_clear_error (&error);
		} else if (!g_subprocess_get_successful (proc)) {
			result = g_strdup_printf (_("Befehl endete mit Fehlercode %d"), g_subprocess_get_exit_status (proc));
		}
		g_object_unref (proc);
	}
	g_strfreev (argv);
	g_ptr_array_free (final, TRUE);
	return result;
}

static void
exec_thread (GTask *task, gpointer source, gpointer task_data, GCancellable *cancellable)
{
	ExecJob *j = task_data;
	GString *log = g_string_new (NULL);
	GList *l;
	guint ok = 0, failed = 0;
	GFile *target = j->target_dir != NULL ? g_file_new_for_path (j->target_dir) : NULL;

	for (l = j->paths; l != NULL; l = l->next) {
		const gchar *path = l->data;
		GFile *src = g_file_new_for_path (path);
		gchar *base = g_path_get_basename (path);
		gchar *err = NULL;
		GError *error = NULL;

		switch (j->kind) {
		case ACT_MOVE:
		case ACT_COPY: {
			GFile *dest = unique_target (target, base);
			gboolean good = j->kind == ACT_MOVE
				? g_file_move (src, dest, G_FILE_COPY_NONE, NULL, NULL, NULL, &error)
				: g_file_copy (src, dest, G_FILE_COPY_NONE, NULL, NULL, NULL, &error);

			if (!good) {
				err = g_strdup (error->message);
				g_clear_error (&error);
			}
			g_object_unref (dest);
			break;
		}
		case ACT_TRASH:
			if (!g_file_trash (src, NULL, &error)) {
				err = g_strdup (error->message);
				g_clear_error (&error);
			}
			break;
		case ACT_COMMAND:
			err = run_command (j->command, path);
			break;
		}

		if (err == NULL) {
			ok++;
			g_string_append_printf (log, "OK      %s\n", path);
		} else {
			failed++;
			g_string_append_printf (log, "FEHLER  %s – %s\n", path, err);
			g_free (err);
		}
		g_free (base);
		g_object_unref (src);
	}
	g_string_append_printf (log, "\n%u erfolgreich, %u fehlgeschlagen.\n", ok, failed);
	g_clear_object (&target);
	g_task_return_pointer (task, g_string_free (log, FALSE), g_free);
}

/* --- Seite -------------------------------------------------------------------------- */

enum { RC_APPLY, RC_NAME, RC_SIZE, RC_PATH, RC_N };

typedef struct {
	NolphinWindow *window;
	GtkWidget *heading;
	GtkWidget *source_label;
	GtkWidget *source_button;
	GtkWidget *cat_combo, *name_entry, *size_combo, *size_spin, *date_combo, *date_spin, *recursive_check;
	GtkWidget *action_combo, *target_label, *target_button, *command_entry;
	GtkWidget *preview_button, *apply_button, *spinner;
	GtkWidget *tree;
	GtkListStore *store;
	GtkWidget *status, *result_view, *result_scroller;
	gchar *folder;
	GList *selection;      /* gchar* */
	gchar *target_dir;
	gboolean busy;
} RulesPage;

static void
rules_set_status (RulesPage *d, const gchar *text)
{
	gtk_label_set_text (GTK_LABEL (d->status), text != NULL ? text : "");
}

static void
rules_update (RulesPage *d)
{
	gint act = gtk_combo_box_get_active (GTK_COMBO_BOX (d->action_combo));
	gboolean has_source = d->folder != NULL || d->selection != NULL;

	gtk_widget_set_visible (d->target_label, act == ACT_MOVE || act == ACT_COPY);
	gtk_widget_set_visible (d->target_button, act == ACT_MOVE || act == ACT_COPY);
	gtk_widget_set_visible (d->command_entry, act == ACT_COMMAND);
	gtk_widget_set_visible (d->recursive_check, d->selection == NULL);
	gtk_widget_set_visible (d->source_button, d->selection == NULL);
	gtk_widget_set_sensitive (d->preview_button, has_source && !d->busy);
	gtk_widget_set_sensitive (d->apply_button, gtk_tree_model_iter_n_children (GTK_TREE_MODEL (d->store), NULL) > 0 && !d->busy);
	if (d->busy) {
		gtk_spinner_start (GTK_SPINNER (d->spinner));
	} else {
		gtk_spinner_stop (GTK_SPINNER (d->spinner));
	}
}

static RuleCfg *
rules_collect (RulesPage *d)
{
	RuleCfg *c = g_new0 (RuleCfg, 1);
	GList *l;

	c->category = (Category) gtk_combo_box_get_active (GTK_COMBO_BOX (d->cat_combo));
	c->name_pattern = g_strdup (gtk_entry_get_text (GTK_ENTRY (d->name_entry)));
	c->size_op = gtk_combo_box_get_active (GTK_COMBO_BOX (d->size_combo));
	c->size_kib = (gint64) gtk_spin_button_get_value (GTK_SPIN_BUTTON (d->size_spin));
	c->date_op = gtk_combo_box_get_active (GTK_COMBO_BOX (d->date_combo));
	c->days = (gint) gtk_spin_button_get_value (GTK_SPIN_BUTTON (d->date_spin));
	c->recursive = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (d->recursive_check));
	if (d->selection != NULL) {
		for (l = d->selection; l != NULL; l = l->next) {
			c->paths = g_list_append (c->paths, g_strdup (l->data));
		}
	} else {
		c->folder = g_strdup (d->folder);
	}
	return c;
}

static void
preview_ready (GObject *source, GAsyncResult *result, gpointer user_data)
{
	RulesPage *d = user_data;
	GPtrArray *matches = g_task_propagate_pointer (G_TASK (result), NULL);
	guint i;

	d->busy = FALSE;
	gtk_list_store_clear (d->store);
	for (i = 0; matches != NULL && i < matches->len; i++) {
		const gchar *path = matches->pdata[i];
		GFile *f = g_file_new_for_path (path);
		GFileInfo *info = g_file_query_info (f, G_FILE_ATTRIBUTE_STANDARD_SIZE, G_FILE_QUERY_INFO_NONE, NULL, NULL);
		gchar *base = g_path_get_basename (path);
		gchar *sz = info != NULL ? g_format_size (g_file_info_get_size (info)) : g_strdup ("");
		GtkTreeIter iter;

		gtk_list_store_append (d->store, &iter);
		gtk_list_store_set (d->store, &iter, RC_APPLY, TRUE, RC_NAME, base, RC_SIZE, sz, RC_PATH, path, -1);
		g_free (sz);
		g_free (base);
		g_clear_object (&info);
		g_object_unref (f);
	}
	if (matches != NULL) {
		gchar *msg;

		if (matches->len == 0) {
			rules_set_status (d, _("Keine Datei erfüllt die Bedingungen."));
		} else {
			msg = g_strdup_printf (_("%u Datei(en) betroffen. Das ist nur eine Vorschau – es wurde noch nichts verändert."), matches->len);
			rules_set_status (d, msg);
			g_free (msg);
		}
		g_ptr_array_unref (matches);
	}
	gtk_widget_hide (d->result_scroller);
	rules_update (d);
}

static void
on_rules_preview (GtkButton *button, gpointer data)
{
	RulesPage *d = data;
	GTask *task;
	RuleCfg *c = rules_collect (d);

	d->busy = TRUE;
	rules_set_status (d, _("Wird durchsucht …"));
	rules_update (d);
	task = g_task_new (NULL, NULL, preview_ready, d);
	g_task_set_task_data (task, c, (GDestroyNotify) rule_cfg_free);
	g_task_run_in_thread (task, match_thread);
	g_object_unref (task);
}

typedef struct { GList *paths; } CheckedCollect;

static gboolean
collect_checked (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
	CheckedCollect *c = data;
	gboolean apply;
	gchar *p = NULL;

	gtk_tree_model_get (model, iter, RC_APPLY, &apply, RC_PATH, &p, -1);
	if (apply && p != NULL) {
		c->paths = g_list_append (c->paths, p);
	} else {
		g_free (p);
	}
	return FALSE;
}

static void
exec_ready (GObject *source, GAsyncResult *result, gpointer user_data)
{
	RulesPage *d = user_data;
	gchar *log = g_task_propagate_pointer (G_TASK (result), NULL);

	d->busy = FALSE;
	if (log != NULL) {
		gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (d->result_view)), log, -1);
		gtk_widget_show (d->result_scroller);
		g_free (log);
	}
	gtk_list_store_clear (d->store);
	rules_set_status (d, _("Fertig. Ergebnis siehe unten; zum Prüfen erneut eine Vorschau erstellen."));
	rules_update (d);
}

static void
on_rules_apply (GtkButton *button, gpointer data)
{
	RulesPage *d = data;
	CheckedCollect cc = { NULL };
	ExecJob *j;
	GTask *task;
	gint act = gtk_combo_box_get_active (GTK_COMBO_BOX (d->action_combo));
	GtkWidget *toplevel = gtk_widget_get_toplevel (GTK_WIDGET (button));
	GtkWidget *dialog;
	guint n;
	gboolean go;
	const gchar *what;

	gtk_tree_model_foreach (GTK_TREE_MODEL (d->store), collect_checked, &cc);
	n = g_list_length (cc.paths);
	if (n == 0) {
		rules_set_status (d, _("Keine Dateien markiert."));
		return;
	}
	if ((act == ACT_MOVE || act == ACT_COPY) && d->target_dir == NULL) {
		rules_set_status (d, _("Bitte zuerst einen Zielordner wählen."));
		g_list_free_full (cc.paths, g_free);
		return;
	}
	if (act == ACT_COMMAND && gtk_entry_get_text (GTK_ENTRY (d->command_entry))[0] == '\0') {
		rules_set_status (d, _("Bitte einen Befehl eingeben (z. B. »gzip %f«)."));
		g_list_free_full (cc.paths, g_free);
		return;
	}

	what = act == ACT_MOVE ? _("verschoben werden") : act == ACT_COPY ? _("kopiert werden") : act == ACT_TRASH ? _("in den Papierkorb verschoben werden") : _("mit dem Befehl bearbeitet werden");
	dialog = gtk_message_dialog_new (GTK_IS_WINDOW (toplevel) ? GTK_WINDOW (toplevel) : NULL,
					 GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
					 _("Sollen %u Datei(en) %s?"), n, what);
	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s",
						  act == ACT_COMMAND ? _("Der Befehl läuft ohne Shell, einzeln für jede Datei.")
								     : _("Vorher wurde die Auswahl in der Vorschau geprüft."));
	gtk_dialog_add_buttons (GTK_DIALOG (dialog), _("_Abbrechen"), GTK_RESPONSE_CANCEL, _("_Ausführen"), GTK_RESPONSE_OK, NULL);
	go = gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK;
	gtk_widget_destroy (dialog);
	if (!go) {
		g_list_free_full (cc.paths, g_free);
		return;
	}

	j = g_new0 (ExecJob, 1);
	j->kind = (ActionKind) act;
	j->target_dir = g_strdup (d->target_dir);
	j->command = g_strdup (gtk_entry_get_text (GTK_ENTRY (d->command_entry)));
	j->paths = cc.paths;

	d->busy = TRUE;
	rules_set_status (d, _("Wird ausgeführt …"));
	rules_update (d);
	task = g_task_new (NULL, NULL, exec_ready, d);
	g_task_set_task_data (task, j, (GDestroyNotify) exec_job_free);
	g_task_run_in_thread (task, exec_thread);
	g_object_unref (task);
}

static void
on_rules_toggled (GtkCellRendererToggle *cell, gchar *path_str, gpointer data)
{
	RulesPage *d = data;
	GtkTreeIter iter;
	gboolean v;

	if (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (d->store), &iter, path_str)) {
		gtk_tree_model_get (GTK_TREE_MODEL (d->store), &iter, RC_APPLY, &v, -1);
		gtk_list_store_set (d->store, &iter, RC_APPLY, !v, -1);
	}
}

static GFile *
choose_folder (GtkWidget *parent, const gchar *title)
{
	GtkWidget *toplevel = gtk_widget_get_toplevel (parent);
	GtkWidget *chooser = gtk_file_chooser_dialog_new (title, GTK_IS_WINDOW (toplevel) ? GTK_WINDOW (toplevel) : NULL,
							  GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
							  _("_Abbrechen"), GTK_RESPONSE_CANCEL,
							  _("_Auswählen"), GTK_RESPONSE_ACCEPT, NULL);
	GFile *f = NULL;

	if (gtk_dialog_run (GTK_DIALOG (chooser)) == GTK_RESPONSE_ACCEPT) {
		f = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (chooser));
	}
	gtk_widget_destroy (chooser);
	return f;
}

static void
on_rules_choose_source (GtkButton *button, gpointer data)
{
	RulesPage *d = data;
	GFile *f = choose_folder (GTK_WIDGET (button), _("Ordner wählen"));

	if (f != NULL) {
		g_free (d->folder);
		d->folder = g_file_get_path (f);
		gtk_label_set_text (GTK_LABEL (d->source_label), d->folder != NULL ? d->folder : "");
		gtk_list_store_clear (d->store);
		rules_update (d);
		g_object_unref (f);
	}
}

static void
on_rules_choose_target (GtkButton *button, gpointer data)
{
	RulesPage *d = data;
	GFile *f = choose_folder (GTK_WIDGET (button), _("Zielordner wählen"));

	if (f != NULL) {
		g_free (d->target_dir);
		d->target_dir = g_file_get_path (f);
		gtk_label_set_text (GTK_LABEL (d->target_label), d->target_dir != NULL ? d->target_dir : "");
		g_object_unref (f);
	}
}

static void
on_rules_action_changed (GtkComboBox *combo, gpointer data)
{
	rules_update (data);
}

static void
rules_page_free (gpointer data)
{
	RulesPage *d = data;

	g_free (d->folder);
	g_free (d->target_dir);
	g_list_free_full (d->selection, g_free);
	g_free (d);
}

static GtkWidget *
labeled (const gchar *text, GtkWidget *widget)
{
	GtkWidget *row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	GtkWidget *l = gtk_label_new (text);

	gtk_widget_set_size_request (l, 90, -1);
	gtk_widget_set_halign (l, GTK_ALIGN_START);
	gtk_box_pack_start (GTK_BOX (row), l, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (row), widget, TRUE, TRUE, 0);
	return row;
}

GtkWidget *
nolphin_rules_page_new (NolphinWindow *window)
{
	RulesPage *d = g_new0 (RulesPage, 1);
	GtkWidget *outer, *scroller, *row, *tree_scroller, *intro, *frame_label, *b;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	d->window = window;
	d->store = gtk_list_store_new (RC_N, G_TYPE_BOOLEAN, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

	outer = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
	gtk_container_set_border_width (GTK_CONTAINER (outer), 12);
	gtk_box_pack_start (GTK_BOX (outer), nolphin_tools_back_button_new (window), FALSE, FALSE, 0);
	d->heading = nolphin_tools_heading_new (_("Regeln anwenden"));
	gtk_box_pack_start (GTK_BOX (outer), d->heading, FALSE, FALSE, 0);

	intro = gtk_label_new (_("Wenn die Bedingungen zutreffen, dann die Aktion – nur manuell und erst nach der Vorschau. "
				 "Alle ausgefüllten Bedingungen müssen gleichzeitig zutreffen."));
	gtk_label_set_line_wrap (GTK_LABEL (intro), TRUE);
	gtk_widget_set_halign (intro, GTK_ALIGN_START);
	gtk_style_context_add_class (gtk_widget_get_style_context (intro), "dim-label");
	gtk_box_pack_start (GTK_BOX (outer), intro, FALSE, FALSE, 0);

	row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	d->source_label = gtk_label_new (_("(keine Quelle)"));
	gtk_label_set_ellipsize (GTK_LABEL (d->source_label), PANGO_ELLIPSIZE_MIDDLE);
	gtk_widget_set_halign (d->source_label, GTK_ALIGN_START);
	gtk_box_pack_start (GTK_BOX (row), d->source_label, TRUE, TRUE, 0);
	d->source_button = gtk_button_new_with_label (_("Ordner wählen …"));
	g_signal_connect (d->source_button, "clicked", G_CALLBACK (on_rules_choose_source), d);
	gtk_box_pack_start (GTK_BOX (row), d->source_button, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (outer), row, FALSE, FALSE, 0);
	d->recursive_check = gtk_check_button_new_with_label (_("Unterordner einbeziehen"));
	gtk_box_pack_start (GTK_BOX (outer), d->recursive_check, FALSE, FALSE, 0);

	frame_label = gtk_label_new (NULL);
	gtk_label_set_markup (GTK_LABEL (frame_label), _("<b>Wenn …</b>"));
	gtk_widget_set_halign (frame_label, GTK_ALIGN_START);
	gtk_box_pack_start (GTK_BOX (outer), frame_label, FALSE, FALSE, 0);

	d->cat_combo = gtk_combo_box_text_new ();
	{
		const gchar *cats[] = { N_("Beliebig"), N_("Bilder"), N_("Videos"), N_("Audio"), N_("Text"), N_("Archive"), N_("Dokumente") };
		guint i;

		for (i = 0; i < G_N_ELEMENTS (cats); i++) {
			gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (d->cat_combo), _(cats[i]));
		}
	}
	gtk_combo_box_set_active (GTK_COMBO_BOX (d->cat_combo), 0);
	gtk_box_pack_start (GTK_BOX (outer), labeled (_("Dateityp:"), d->cat_combo), FALSE, FALSE, 0);

	d->name_entry = gtk_entry_new ();
	gtk_entry_set_placeholder_text (GTK_ENTRY (d->name_entry), _("Name mit Platzhaltern, z. B. *.jpg"));
	gtk_box_pack_start (GTK_BOX (outer), labeled (_("Name:"), d->name_entry), FALSE, FALSE, 0);

	row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	d->size_combo = gtk_combo_box_text_new ();
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (d->size_combo), _("egal"));
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (d->size_combo), _("größer als"));
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (d->size_combo), _("kleiner als"));
	gtk_combo_box_set_active (GTK_COMBO_BOX (d->size_combo), 0);
	d->size_spin = gtk_spin_button_new_with_range (0, 100000000, 1);
	gtk_box_pack_start (GTK_BOX (row), d->size_combo, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (row), d->size_spin, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (row), gtk_label_new (_("KiB")), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (outer), labeled (_("Größe:"), row), FALSE, FALSE, 0);

	row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	d->date_combo = gtk_combo_box_text_new ();
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (d->date_combo), _("egal"));
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (d->date_combo), _("älter als"));
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (d->date_combo), _("neuer als"));
	gtk_combo_box_set_active (GTK_COMBO_BOX (d->date_combo), 0);
	d->date_spin = gtk_spin_button_new_with_range (0, 36500, 1);
	gtk_box_pack_start (GTK_BOX (row), d->date_combo, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (row), d->date_spin, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (row), gtk_label_new (_("Tage")), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (outer), labeled (_("Änderung:"), row), FALSE, FALSE, 0);

	frame_label = gtk_label_new (NULL);
	gtk_label_set_markup (GTK_LABEL (frame_label), _("<b>… dann</b>"));
	gtk_widget_set_halign (frame_label, GTK_ALIGN_START);
	gtk_box_pack_start (GTK_BOX (outer), frame_label, FALSE, FALSE, 0);

	d->action_combo = gtk_combo_box_text_new ();
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (d->action_combo), _("Verschieben nach …"));
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (d->action_combo), _("Kopieren nach …"));
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (d->action_combo), _("In den Papierkorb verschieben"));
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (d->action_combo), _("Befehl ausführen"));
	gtk_combo_box_set_active (GTK_COMBO_BOX (d->action_combo), ACT_MOVE);
	g_signal_connect (d->action_combo, "changed", G_CALLBACK (on_rules_action_changed), d);
	gtk_box_pack_start (GTK_BOX (outer), d->action_combo, FALSE, FALSE, 0);

	row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	d->target_label = gtk_label_new (_("(kein Zielordner)"));
	gtk_label_set_ellipsize (GTK_LABEL (d->target_label), PANGO_ELLIPSIZE_MIDDLE);
	gtk_widget_set_halign (d->target_label, GTK_ALIGN_START);
	gtk_box_pack_start (GTK_BOX (row), d->target_label, TRUE, TRUE, 0);
	d->target_button = gtk_button_new_with_label (_("Zielordner …"));
	g_signal_connect (d->target_button, "clicked", G_CALLBACK (on_rules_choose_target), d);
	gtk_box_pack_start (GTK_BOX (row), d->target_button, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (outer), row, FALSE, FALSE, 0);
	d->command_entry = gtk_entry_new ();
	gtk_entry_set_placeholder_text (GTK_ENTRY (d->command_entry), _("Befehl, %f steht für die Datei, z. B. gzip %f"));
	gtk_box_pack_start (GTK_BOX (outer), d->command_entry, FALSE, FALSE, 0);

	row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	d->preview_button = gtk_button_new_with_label (_("Vorschau"));
	gtk_style_context_add_class (gtk_widget_get_style_context (d->preview_button), "suggested-action");
	g_signal_connect (d->preview_button, "clicked", G_CALLBACK (on_rules_preview), d);
	gtk_box_pack_start (GTK_BOX (row), d->preview_button, FALSE, FALSE, 0);
	d->apply_button = gtk_button_new_with_label (_("Anwenden"));
	g_signal_connect (d->apply_button, "clicked", G_CALLBACK (on_rules_apply), d);
	gtk_box_pack_start (GTK_BOX (row), d->apply_button, FALSE, FALSE, 0);
	d->spinner = gtk_spinner_new ();
	gtk_box_pack_start (GTK_BOX (row), d->spinner, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (outer), row, FALSE, FALSE, 0);

	d->tree = gtk_tree_view_new_with_model (GTK_TREE_MODEL (d->store));
	renderer = gtk_cell_renderer_toggle_new ();
	g_signal_connect (renderer, "toggled", G_CALLBACK (on_rules_toggled), d);
	column = gtk_tree_view_column_new_with_attributes ("", renderer, "active", RC_APPLY, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (d->tree), column);
	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer, "ellipsize", PANGO_ELLIPSIZE_MIDDLE, NULL);
	column = gtk_tree_view_column_new_with_attributes (_("Datei"), renderer, "text", RC_NAME, NULL);
	gtk_tree_view_column_set_expand (column, TRUE);
	gtk_tree_view_append_column (GTK_TREE_VIEW (d->tree), column);
	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Größe"), renderer, "text", RC_SIZE, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (d->tree), column);
	tree_scroller = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (tree_scroller), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_widget_set_size_request (tree_scroller, -1, 180);
	gtk_container_add (GTK_CONTAINER (tree_scroller), d->tree);
	gtk_box_pack_start (GTK_BOX (outer), tree_scroller, TRUE, TRUE, 0);

	d->status = gtk_label_new ("");
	gtk_label_set_line_wrap (GTK_LABEL (d->status), TRUE);
	gtk_widget_set_halign (d->status, GTK_ALIGN_START);
	gtk_box_pack_start (GTK_BOX (outer), d->status, FALSE, FALSE, 0);

	d->result_view = gtk_text_view_new ();
	gtk_text_view_set_editable (GTK_TEXT_VIEW (d->result_view), FALSE);
	gtk_text_view_set_monospace (GTK_TEXT_VIEW (d->result_view), TRUE);
	d->result_scroller = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (d->result_scroller), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_widget_set_size_request (d->result_scroller, -1, 130);
	gtk_container_add (GTK_CONTAINER (d->result_scroller), d->result_view);
	gtk_box_pack_start (GTK_BOX (outer), d->result_scroller, FALSE, TRUE, 0);
	gtk_widget_show (d->result_view);
	gtk_widget_set_no_show_all (d->result_scroller, TRUE);

	b = NULL;
	(void) b;
	scroller = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroller), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (scroller), outer);
	g_object_set_data_full (G_OBJECT (scroller), "rules-data", d, rules_page_free);
	g_signal_connect_after (outer, "map", G_CALLBACK (gtk_widget_queue_resize), NULL);
	return scroller;
}

void
nolphin_rules_page_open (GtkWidget *page, GFile *folder, GList *selection_paths, gboolean batch_mode)
{
	RulesPage *d = page != NULL ? g_object_get_data (G_OBJECT (page), "rules-data") : NULL;
	GList *l;

	if (d == NULL) {
		return;
	}
	g_clear_pointer (&d->folder, g_free);
	g_list_free_full (d->selection, g_free);
	d->selection = NULL;
	for (l = selection_paths; l != NULL; l = l->next) {
		d->selection = g_list_append (d->selection, g_strdup (l->data));
	}
	if (d->selection != NULL) {
		gchar *txt = g_strdup_printf (ngettext ("Gewählte Datei (%u)", "Gewählte Dateien (%u)", g_list_length (d->selection)),
					      g_list_length (d->selection));

		gtk_label_set_text (GTK_LABEL (d->source_label), txt);
		g_free (txt);
	} else if (folder != NULL) {
		d->folder = g_file_get_path (folder);
		gtk_label_set_text (GTK_LABEL (d->source_label), d->folder != NULL ? d->folder : "");
	}
	gtk_label_set_text (GTK_LABEL (d->heading), batch_mode ? _("Stapelverarbeitung") : _("Regeln anwenden"));
	gtk_list_store_clear (d->store);
	gtk_widget_hide (d->result_scroller);
	rules_set_status (d, NULL);
	rules_update (d);
}
