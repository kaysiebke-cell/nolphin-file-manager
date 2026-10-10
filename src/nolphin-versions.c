/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nolphin-versions.c: Versionierung (§45)
 *
 * Nolphin speichert Versionen einer Datei selbst unter
 * ~/.local/share/nolphin/versions/<Pfad-Kennung>/: manuell speichern,
 * mit Zeitstempel anzeigen, wiederherstellen, löschen und (bei Textdateien)
 * Unterschiede zur aktuellen Datei anzeigen.
 */

#include <config.h>

#include "nolphin-tools.h"

#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <string.h>

#define MAX_VERSION_BYTES (2 * G_GINT64_CONSTANT (1024) * 1024 * 1024)
#define MAX_DIFF_BYTES (400 * 1024)

/* --- Speicher ------------------------------------------------------------------ */

static gchar *
versions_dir_for_path (const gchar *path)
{
	gchar *id = g_compute_checksum_for_string (G_CHECKSUM_SHA256, path, -1);
	gchar *short_id = g_strndup (id, 24);
	gchar *dir = g_build_filename (g_get_user_data_dir (), "nolphin", "versions", short_id, NULL);

	g_free (id);
	g_free (short_id);
	return dir;
}

static GKeyFile *
index_load (const gchar *dir)
{
	GKeyFile *kf = g_key_file_new ();
	gchar *path = g_build_filename (dir, "index.ini", NULL);

	g_key_file_load_from_file (kf, path, G_KEY_FILE_NONE, NULL);
	g_free (path);
	return kf;
}

static gboolean
index_save (const gchar *dir, GKeyFile *kf, GError **error)
{
	gchar *path = g_build_filename (dir, "index.ini", NULL);
	gchar *data = g_key_file_to_data (kf, NULL, NULL);
	gboolean ok = g_file_set_contents (path, data, -1, error);

	g_free (data);
	g_free (path);
	return ok;
}

gboolean
nolphin_versions_save_file (GFile *file, const gchar *comment, GError **error)
{
	gchar *path = g_file_get_path (file), *dir, *id, *dest_path, *group;
	GFileInfo *info;
	GKeyFile *kf;
	GFile *dest;
	gboolean ok;
	goffset size = 0;

	if (path == NULL) {
		g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED, "%s",
			     _("Versionen gibt es nur für lokale Dateien."));
		return FALSE;
	}
	info = g_file_query_info (file, G_FILE_ATTRIBUTE_STANDARD_TYPE "," G_FILE_ATTRIBUTE_STANDARD_SIZE,
				  G_FILE_QUERY_INFO_NONE, NULL, error);
	if (info == NULL) {
		g_free (path);
		return FALSE;
	}
	if (g_file_info_get_file_type (info) != G_FILE_TYPE_REGULAR) {
		g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_REGULAR_FILE, "%s",
			     _("Versionen gibt es nur für einzelne Dateien, nicht für Ordner."));
		g_object_unref (info);
		g_free (path);
		return FALSE;
	}
	size = g_file_info_get_size (info);
	g_object_unref (info);
	if (size > MAX_VERSION_BYTES) {
		g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "%s", _("Die Datei ist zu groß für eine Version (über 2 GiB)."));
		g_free (path);
		return FALSE;
	}

	dir = versions_dir_for_path (path);
	g_mkdir_with_parents (dir, 0700);
	id = g_strdup_printf ("%" G_GINT64_FORMAT, g_get_real_time ());
	dest_path = g_build_filename (dir, id, NULL);
	dest = g_file_new_for_path (dest_path);

	ok = g_file_copy (file, dest, G_FILE_COPY_NONE, NULL, NULL, NULL, error);
	if (ok) {
		kf = index_load (dir);
		g_key_file_set_string (kf, "file", "path", path);
		group = g_strconcat ("v", id, NULL);
		g_key_file_set_int64 (kf, group, "time", g_get_real_time () / G_USEC_PER_SEC);
		g_key_file_set_int64 (kf, group, "size", size);
		g_key_file_set_string (kf, group, "comment", comment != NULL ? comment : "");
		ok = index_save (dir, kf, error);
		g_free (group);
		g_key_file_free (kf);
	}

	g_object_unref (dest);
	g_free (dest_path);
	g_free (id);
	g_free (dir);
	g_free (path);
	return ok;
}

/* --- Seite ------------------------------------------------------------------------ */

enum { VC_ID, VC_TIME, VC_SIZE, VC_COMMENT, VC_N };

typedef struct {
	NolphinWindow *window;
	GFile *file;
	GtkWidget *file_label;
	GtkWidget *comment_entry;
	GtkWidget *tree;
	GtkListStore *store;
	GtkWidget *status;
	GtkWidget *diff_view;
	GtkWidget *diff_scroller;
} VerPage;

static void
ver_set_status (VerPage *d, const gchar *text)
{
	gtk_label_set_text (GTK_LABEL (d->status), text != NULL ? text : "");
}

static void
ver_reload (VerPage *d)
{
	gchar *path, *dir;
	GKeyFile *kf;
	gchar **groups;
	gsize n, i;
	GList *entries = NULL, *l;

	gtk_list_store_clear (d->store);
	if (d->file == NULL || (path = g_file_get_path (d->file)) == NULL) {
		return;
	}
	dir = versions_dir_for_path (path);
	kf = index_load (dir);
	groups = g_key_file_get_groups (kf, &n);
	for (i = 0; i < n; i++) {
		if (groups[i][0] == 'v') {
			entries = g_list_prepend (entries, g_strdup (groups[i] + 1));
		}
	}
	g_strfreev (groups);
	/* neueste zuerst: die ID ist ein Zeitstempel in Mikrosekunden */
	entries = g_list_sort (entries, (GCompareFunc) g_strcmp0);
	entries = g_list_reverse (entries);

	for (l = entries; l != NULL; l = l->next) {
		gchar *group = g_strconcat ("v", (gchar *) l->data, NULL);
		gint64 t = g_key_file_get_int64 (kf, group, "time", NULL);
		gint64 size = g_key_file_get_int64 (kf, group, "size", NULL);
		gchar *comment = g_key_file_get_string (kf, group, "comment", NULL);
		GDateTime *dt = g_date_time_new_from_unix_local (t);
		gchar *time_text = g_date_time_format (dt, "%d.%m.%Y %H:%M:%S");
		gchar *size_text = g_format_size (size);
		GtkTreeIter iter;

		gtk_list_store_append (d->store, &iter);
		gtk_list_store_set (d->store, &iter, VC_ID, l->data, VC_TIME, time_text, VC_SIZE, size_text,
				    VC_COMMENT, comment != NULL ? comment : "", -1);
		g_free (size_text);
		g_free (time_text);
		g_date_time_unref (dt);
		g_free (comment);
		g_free (group);
	}
	g_list_free_full (entries, g_free);
	g_key_file_free (kf);
	g_free (dir);
	g_free (path);
}

static gchar *
ver_selected_id (VerPage *d)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	gchar *id = NULL;

	if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (GTK_TREE_VIEW (d->tree)), &model, &iter)) {
		gtk_tree_model_get (model, &iter, VC_ID, &id, -1);
	}
	return id;
}

static void
on_ver_save (GtkButton *button, gpointer data)
{
	VerPage *d = data;
	GError *error = NULL;

	if (d->file == NULL) {
		return;
	}
	if (nolphin_versions_save_file (d->file, gtk_entry_get_text (GTK_ENTRY (d->comment_entry)), &error)) {
		ver_set_status (d, _("Version gespeichert."));
		gtk_entry_set_text (GTK_ENTRY (d->comment_entry), "");
		ver_reload (d);
	} else {
		ver_set_status (d, error->message);
		g_clear_error (&error);
	}
}

static gboolean
ver_confirm (GtkWidget *parent, const gchar *text, const gchar *secondary, const gchar *accept)
{
	GtkWidget *toplevel = gtk_widget_get_toplevel (parent);
	GtkWidget *dialog = gtk_message_dialog_new (GTK_IS_WINDOW (toplevel) ? GTK_WINDOW (toplevel) : NULL,
						    GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_QUESTION,
						    GTK_BUTTONS_NONE, "%s", text);
	gboolean ok;

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", secondary);
	gtk_dialog_add_buttons (GTK_DIALOG (dialog), _("_Abbrechen"), GTK_RESPONSE_CANCEL, accept, GTK_RESPONSE_OK, NULL);
	ok = gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK;
	gtk_widget_destroy (dialog);
	return ok;
}

static void
on_ver_restore (GtkButton *button, gpointer data)
{
	VerPage *d = data;
	gchar *id = ver_selected_id (d), *path, *dir, *store_path;
	GError *error = NULL;
	GFile *src;

	if (id == NULL || d->file == NULL) {
		ver_set_status (d, _("Bitte eine Version in der Liste auswählen."));
		g_free (id);
		return;
	}
	if (!ver_confirm (GTK_WIDGET (button), _("Diese Version wiederherstellen?"),
			  _("Der aktuelle Stand der Datei wird vorher selbst als Version gesichert."), _("_Wiederherstellen"))) {
		g_free (id);
		return;
	}

	path = g_file_get_path (d->file);
	dir = versions_dir_for_path (path);
	store_path = g_build_filename (dir, id, NULL);
	src = g_file_new_for_path (store_path);

	/* aktuellen Stand sichern (falls die Datei noch existiert), dann überschreiben */
	if (g_file_query_exists (d->file, NULL)) {
		nolphin_versions_save_file (d->file, _("Vor Wiederherstellung"), NULL);
	}
	if (g_file_copy (src, d->file, G_FILE_COPY_OVERWRITE, NULL, NULL, NULL, &error)) {
		ver_set_status (d, _("Version wiederhergestellt."));
	} else {
		ver_set_status (d, error->message);
		g_clear_error (&error);
	}
	ver_reload (d);

	g_object_unref (src);
	g_free (store_path);
	g_free (dir);
	g_free (path);
	g_free (id);
}

static void
on_ver_delete (GtkButton *button, gpointer data)
{
	VerPage *d = data;
	gchar *id = ver_selected_id (d), *path, *dir, *store_path, *group;
	GKeyFile *kf;

	if (id == NULL || d->file == NULL) {
		ver_set_status (d, _("Bitte eine Version in der Liste auswählen."));
		g_free (id);
		return;
	}
	if (!ver_confirm (GTK_WIDGET (button), _("Diese Version löschen?"),
			  _("Die gesicherte Version wird endgültig entfernt."), _("_Löschen"))) {
		g_free (id);
		return;
	}
	path = g_file_get_path (d->file);
	dir = versions_dir_for_path (path);
	store_path = g_build_filename (dir, id, NULL);
	g_remove (store_path);
	kf = index_load (dir);
	group = g_strconcat ("v", id, NULL);
	g_key_file_remove_group (kf, group, NULL);
	index_save (dir, kf, NULL);
	ver_set_status (d, _("Version gelöscht."));
	ver_reload (d);

	g_free (group);
	g_key_file_free (kf);
	g_free (store_path);
	g_free (dir);
	g_free (path);
	g_free (id);
}

static void
on_ver_diff (GtkButton *button, gpointer data)
{
	VerPage *d = data;
	gchar *id = ver_selected_id (d), *path, *dir, *store_path;
	GFileInfo *info;
	const gchar *type;

	if (id == NULL || d->file == NULL) {
		ver_set_status (d, _("Bitte eine Version in der Liste auswählen."));
		g_free (id);
		return;
	}
	info = g_file_query_info (d->file, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE "," G_FILE_ATTRIBUTE_STANDARD_SIZE,
				  G_FILE_QUERY_INFO_NONE, NULL, NULL);
	type = info != NULL ? g_file_info_get_content_type (info) : NULL;

	if (info == NULL || type == NULL || !g_content_type_is_a (type, "text/plain") ||
	    g_file_info_get_size (info) > MAX_DIFF_BYTES) {
		ver_set_status (d, _("Unterschiede gibt es nur für Textdateien bis 400 KiB."));
	} else if (g_find_program_in_path ("diff") == NULL) {
		ver_set_status (d, _("Das Werkzeug »diff« (Paket diffutils) ist nicht installiert."));
	} else {
		GSubprocess *proc;
		GBytes *out = NULL, *in;
		gchar *cur = g_file_get_path (d->file);
		gchar *label_old, *label_new, *text;
		const gchar *argv[] = { "diff", "-u", "--label", NULL, "--label", NULL, NULL, NULL, NULL };

		path = cur;
		dir = versions_dir_for_path (path);
		store_path = g_build_filename (dir, id, NULL);
		label_old = g_strdup (_("Gespeicherte Version"));
		label_new = g_strdup (_("Aktuelle Datei"));
		argv[3] = label_old;
		argv[5] = label_new;
		argv[6] = store_path;
		argv[7] = cur;

		proc = g_subprocess_newv (argv, G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_SILENCE |
					  G_SUBPROCESS_FLAGS_STDIN_PIPE, NULL);
		if (proc != NULL) {
			in = g_bytes_new_static ("", 0);
			g_subprocess_communicate (proc, in, NULL, &out, NULL, NULL);
			g_bytes_unref (in);
			/* Bei gleichen Dateien ist die Ausgabe leer (Daten-Zeiger NULL). */
			text = (out != NULL && g_bytes_get_size (out) > 0) ?
			       g_utf8_make_valid (g_bytes_get_data (out, NULL), g_bytes_get_size (out)) : g_strdup ("");
			if (text[0] == '\0') {
				g_free (text);
				text = g_strdup (_("Keine Unterschiede: Version und aktuelle Datei sind gleich."));
			}
			gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (d->diff_view)), text, -1);
			gtk_widget_show (d->diff_scroller);
			ver_set_status (d, NULL);
			g_free (text);
			g_clear_pointer (&out, g_bytes_unref);
			g_object_unref (proc);
		}
		g_free (label_old);
		g_free (label_new);
		g_free (store_path);
		g_free (dir);
		g_free (cur);
	}
	g_clear_object (&info);
	g_free (id);
}

static void
ver_page_free (gpointer data)
{
	VerPage *d = data;

	g_clear_object (&d->file);
	g_free (d);
}

GtkWidget *
nolphin_versions_page_new (NolphinWindow *window)
{
	VerPage *d = g_new0 (VerPage, 1);
	GtkWidget *outer, *scroller, *tree_scroller, *row, *b, *intro;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	d->window = window;
	d->store = gtk_list_store_new (VC_N, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

	outer = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
	gtk_container_set_border_width (GTK_CONTAINER (outer), 12);
	gtk_box_pack_start (GTK_BOX (outer), nolphin_tools_back_button_new (window), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (outer), nolphin_tools_heading_new (_("Versionen")), FALSE, FALSE, 0);

	intro = gtk_label_new (_("Nolphin sichert Versionen der Datei selbst (in ~/.local/share/nolphin/versions). "
				 "Sie lassen sich wiederherstellen, löschen und bei Textdateien mit der aktuellen Datei vergleichen."));
	gtk_label_set_line_wrap (GTK_LABEL (intro), TRUE);
	gtk_widget_set_halign (intro, GTK_ALIGN_START);
	gtk_style_context_add_class (gtk_widget_get_style_context (intro), "dim-label");
	gtk_box_pack_start (GTK_BOX (outer), intro, FALSE, FALSE, 0);

	d->file_label = gtk_label_new (_("(keine Datei gewählt)"));
	gtk_label_set_ellipsize (GTK_LABEL (d->file_label), PANGO_ELLIPSIZE_MIDDLE);
	gtk_widget_set_halign (d->file_label, GTK_ALIGN_START);
	gtk_label_set_selectable (GTK_LABEL (d->file_label), TRUE);
	gtk_box_pack_start (GTK_BOX (outer), d->file_label, FALSE, FALSE, 0);

	row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	d->comment_entry = gtk_entry_new ();
	gtk_entry_set_placeholder_text (GTK_ENTRY (d->comment_entry), _("Kommentar (optional)"));
	gtk_box_pack_start (GTK_BOX (row), d->comment_entry, TRUE, TRUE, 0);
	b = gtk_button_new_with_label (_("Version speichern"));
	gtk_style_context_add_class (gtk_widget_get_style_context (b), "suggested-action");
	g_signal_connect (b, "clicked", G_CALLBACK (on_ver_save), d);
	gtk_box_pack_start (GTK_BOX (row), b, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (outer), row, FALSE, FALSE, 0);

	d->tree = gtk_tree_view_new_with_model (GTK_TREE_MODEL (d->store));
	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Zeitpunkt"), renderer, "text", VC_TIME, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (d->tree), column);
	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Größe"), renderer, "text", VC_SIZE, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (d->tree), column);
	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	column = gtk_tree_view_column_new_with_attributes (_("Kommentar"), renderer, "text", VC_COMMENT, NULL);
	gtk_tree_view_column_set_expand (column, TRUE);
	gtk_tree_view_append_column (GTK_TREE_VIEW (d->tree), column);

	tree_scroller = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (tree_scroller), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_widget_set_size_request (tree_scroller, -1, 170);
	gtk_container_add (GTK_CONTAINER (tree_scroller), d->tree);
	gtk_box_pack_start (GTK_BOX (outer), tree_scroller, FALSE, TRUE, 0);

	row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	b = gtk_button_new_with_label (_("Wiederherstellen"));
	g_signal_connect (b, "clicked", G_CALLBACK (on_ver_restore), d);
	gtk_box_pack_start (GTK_BOX (row), b, FALSE, FALSE, 0);
	b = gtk_button_new_with_label (_("Unterschiede"));
	g_signal_connect (b, "clicked", G_CALLBACK (on_ver_diff), d);
	gtk_box_pack_start (GTK_BOX (row), b, FALSE, FALSE, 0);
	b = gtk_button_new_with_label (_("Löschen"));
	g_signal_connect (b, "clicked", G_CALLBACK (on_ver_delete), d);
	gtk_box_pack_start (GTK_BOX (row), b, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (outer), row, FALSE, FALSE, 0);

	d->status = gtk_label_new ("");
	gtk_label_set_line_wrap (GTK_LABEL (d->status), TRUE);
	gtk_widget_set_halign (d->status, GTK_ALIGN_START);
	gtk_box_pack_start (GTK_BOX (outer), d->status, FALSE, FALSE, 0);

	d->diff_view = gtk_text_view_new ();
	gtk_text_view_set_editable (GTK_TEXT_VIEW (d->diff_view), FALSE);
	gtk_text_view_set_monospace (GTK_TEXT_VIEW (d->diff_view), TRUE);
	d->diff_scroller = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (d->diff_scroller), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_widget_set_size_request (d->diff_scroller, -1, 200);
	gtk_container_add (GTK_CONTAINER (d->diff_scroller), d->diff_view);
	gtk_box_pack_start (GTK_BOX (outer), d->diff_scroller, TRUE, TRUE, 0);
	gtk_widget_show (d->diff_view);
	gtk_widget_set_no_show_all (d->diff_scroller, TRUE);

	scroller = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroller), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (scroller), outer);
	g_object_set_data_full (G_OBJECT (scroller), "ver-data", d, ver_page_free);

	return scroller;
}

void
nolphin_versions_page_open (GtkWidget *page, GFile *file)
{
	VerPage *d = page != NULL ? g_object_get_data (G_OBJECT (page), "ver-data") : NULL;
	gchar *name;

	if (d == NULL || file == NULL) {
		return;
	}
	g_clear_object (&d->file);
	d->file = g_object_ref (file);
	name = g_file_get_parse_name (file);
	gtk_label_set_text (GTK_LABEL (d->file_label), name);
	g_free (name);
	gtk_widget_hide (d->diff_scroller);
	ver_set_status (d, NULL);
	ver_reload (d);
}
