/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nolphin-help.c: Hilfe zu den Funktionen von Nolphin
 *
 * Die Themen sind Markdown-Dateien (docs/hilfe/NN-thema.md, installiert
 * nach <datadir>/nolphin/help/hilfe/); Bilder liegen in .../help/bilder/.
 * Dargestellt werden sie mit der Markdown-Ansicht der GID-Projekte. Das
 * Hilfeverzeichnis wird in dieser Reihenfolge gesucht: NOLPHIN_HELPDIR,
 * die installierte Stelle, dann docs/ neben dem Quellbaum (für Läufe aus
 * dem Build-Ordner).
 */

#include <config.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <string.h>
#include <unistd.h>

#include "nolphin-help.h"
#include "nolphin-markdown-view.h"

#define HELP_MAX_BYTES (512 * 1024)

typedef struct {
	gchar *id;       /* Dateiname ohne Nummer und Endung, z. B. "git" */
	gchar *title;    /* erste Überschrift */
	gchar *text;     /* ganzer Markdown-Text */
	gchar *folded;   /* kleingeschriebener Text samt Titel, für die Suche */
} HelpTopic;

typedef struct {
	GPtrArray *topics;       /* HelpTopic* */
	gchar     *root;         /* enthält hilfe/ und bilder/ */
	GtkWidget *list;
	GtkWidget *search;
	GtkWidget *view;
	GtkWidget *scrolled;
	GtkWidget *empty_label;
} HelpPage;

static GtkWidget *help_page = NULL;
static GtkWidget *help_host = NULL;
static gulong     help_key_handler = 0;

static void
topic_free (HelpTopic *t)
{
	g_free (t->id);
	g_free (t->title);
	g_free (t->text);
	g_free (t->folded);
	g_free (t);
}

static void
help_page_free (HelpPage *d)
{
	g_ptr_array_unref (d->topics);
	g_free (d->root);
	g_free (d);
}

static gboolean
root_is_valid (const gchar *root)
{
	gchar *dir;
	gboolean ok;

	if (root == NULL) {
		return FALSE;
	}
	dir = g_build_filename (root, "hilfe", NULL);
	ok = g_file_test (dir, G_FILE_TEST_IS_DIR);
	g_free (dir);
	return ok;
}

static gchar *
find_help_root (void)
{
	const gchar *env = g_getenv ("NOLPHIN_HELPDIR");
	gchar *candidate, *exe, *dir;

	if (env != NULL && root_is_valid (env)) {
		return g_strdup (env);
	}

	candidate = g_build_filename (NOLPHIN_DATADIR, "help", NULL);
	if (root_is_valid (candidate)) {
		return candidate;
	}
	g_free (candidate);

	/* Lauf aus dem Build-Ordner: <repo>/build/src/nolphin -> <repo>/docs */
	exe = g_file_read_link ("/proc/self/exe", NULL);
	if (exe != NULL) {
		dir = g_path_get_dirname (exe);
		candidate = g_build_filename (dir, "..", "..", "docs", NULL);
		g_free (dir);
		g_free (exe);
		if (root_is_valid (candidate)) {
			gchar *clean = g_canonicalize_filename (candidate, NULL);

			g_free (candidate);
			return clean;
		}
		g_free (candidate);
	}
	return NULL;
}

static gchar *
title_from_text (const gchar *text, const gchar *fallback)
{
	const gchar *p = text;

	while (p != NULL && *p != '\0') {
		const gchar *end = strchr (p, '\n');
		gsize len = end != NULL ? (gsize) (end - p) : strlen (p);

		if (len > 2 && p[0] == '#' && p[1] == ' ') {
			return g_strndup (p + 2, len - 2);
		}
		if (end == NULL) {
			break;
		}
		p = end + 1;
	}
	return g_strdup (fallback);
}

static gint
compare_names (gconstpointer a, gconstpointer b)
{
	return g_strcmp0 (*(gchar * const *) a, *(gchar * const *) b);
}

static void
load_topics (HelpPage *d)
{
	gchar *dirpath = g_build_filename (d->root, "hilfe", NULL);
	GDir *dir = g_dir_open (dirpath, 0, NULL);
	GPtrArray *names = g_ptr_array_new_with_free_func (g_free);
	const gchar *name;
	guint i;

	while (dir != NULL && (name = g_dir_read_name (dir)) != NULL) {
		if (g_str_has_suffix (name, ".md")) {
			g_ptr_array_add (names, g_strdup (name));
		}
	}
	if (dir != NULL) {
		g_dir_close (dir);
	}
	g_ptr_array_sort (names, compare_names);

	for (i = 0; i < names->len; i++) {
		const gchar *file = g_ptr_array_index (names, i);
		gchar *path = g_build_filename (dirpath, file, NULL);
		gchar *text = NULL;
		gsize length = 0;

		if (g_file_get_contents (path, &text, &length, NULL) && length <= HELP_MAX_BYTES &&
		    g_utf8_validate (text, length, NULL)) {
			HelpTopic *t = g_new0 (HelpTopic, 1);
			const gchar *base = file;
			gchar *stem, *lower_text, *lower_title;

			while (g_ascii_isdigit (*base)) {
				base++;
			}
			if (*base == '-') {
				base++;
			}
			stem = g_strndup (base, strlen (base) - 3);
			t->id = stem;
			t->text = text;
			t->title = title_from_text (text, stem);
			lower_title = g_utf8_strdown (t->title, -1);
			lower_text = g_utf8_strdown (text, -1);
			t->folded = g_strconcat (lower_title, "\n", lower_text, NULL);
			g_free (lower_title);
			g_free (lower_text);
			g_ptr_array_add (d->topics, t);
		} else {
			g_free (text);
		}
		g_free (path);
	}
	g_ptr_array_unref (names);
	g_free (dirpath);
}

static void
close_help (void)
{
	GtkWidget *page = help_page;
	GtkWidget *host = help_host;

	if (page == NULL) {
		return;
	}
	if (host != NULL && help_key_handler != 0) {
		g_signal_handler_disconnect (host, help_key_handler);
	}
	help_key_handler = 0;
	help_host = NULL;
	help_page = NULL;

	if (host != NULL) {
		GtkWidget *stack = g_object_get_data (G_OBJECT (host), "nolphin-content-stack");

		if (stack != NULL) {
			gtk_stack_set_visible_child_name (GTK_STACK (stack), "files");
		}
	}
	gtk_widget_destroy (page);
}

static gboolean
on_help_key_press (GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
	if (event->keyval == GDK_KEY_Escape) {
		close_help ();
		return GDK_EVENT_STOP;
	}
	return GDK_EVENT_PROPAGATE;
}

static void
on_close_clicked (GtkButton *button, gpointer user_data)
{
	close_help ();
}

static void
on_page_destroy (GtkWidget *widget, gpointer user_data)
{
	if (help_page == widget) {
		help_page = NULL;
	}
}

static void
select_topic_by_id (HelpPage *d, const gchar *id)
{
	guint i;

	for (i = 0; i < d->topics->len; i++) {
		HelpTopic *t = g_ptr_array_index (d->topics, i);

		if (g_strcmp0 (t->id, id) == 0) {
			GtkListBoxRow *row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (d->list), i);

			if (row != NULL) {
				gtk_list_box_select_row (GTK_LIST_BOX (d->list), row);
			}
			return;
		}
	}
}

static void
on_topic_selected (GtkListBox *box, GtkListBoxRow *row, gpointer user_data)
{
	HelpPage *d = user_data;
	HelpTopic *t;
	gint index;

	if (row == NULL) {
		return;
	}
	index = gtk_list_box_row_get_index (row);
	if (index < 0 || (guint) index >= d->topics->len) {
		return;
	}
	t = g_ptr_array_index (d->topics, index);
	nolphin_markdown_view_set_text_with_base (d->view, t->text, d->root);
	gtk_adjustment_set_value (gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (d->scrolled)), 0);
}

static void
on_link_clicked (GtkWidget *view, const gchar *href, gpointer user_data)
{
	HelpPage *d = user_data;

	if (g_str_has_prefix (href, "hilfe:")) {
		select_topic_by_id (d, href + 6);
	} else if (g_str_has_prefix (href, "http://") || g_str_has_prefix (href, "https://")) {
		gtk_show_uri_on_window (NULL, href, GDK_CURRENT_TIME, NULL);
	}
}

static gboolean
topic_filter (GtkListBoxRow *row, gpointer user_data)
{
	HelpPage *d = user_data;
	const gchar *needle = gtk_entry_get_text (GTK_ENTRY (d->search));
	gint index = gtk_list_box_row_get_index (row);
	HelpTopic *t;
	gchar *folded;
	gboolean match;

	if (needle == NULL || needle[0] == '\0') {
		return TRUE;
	}
	if (index < 0 || (guint) index >= d->topics->len) {
		return TRUE;
	}
	t = g_ptr_array_index (d->topics, index);
	folded = g_utf8_strdown (needle, -1);
	match = strstr (t->folded, folded) != NULL;
	g_free (folded);
	return match;
}

static void
on_search_changed (GtkSearchEntry *entry, gpointer user_data)
{
	HelpPage *d = user_data;
	GtkListBoxRow *selected;

	gtk_list_box_invalidate_filter (GTK_LIST_BOX (d->list));

	/* Ist das gewählte Thema weggefiltert, das erste sichtbare wählen. */
	selected = gtk_list_box_get_selected_row (GTK_LIST_BOX (d->list));
	if (selected == NULL || !gtk_widget_get_child_visible (GTK_WIDGET (selected)) ||
	    !topic_filter (selected, d)) {
		guint i;

		for (i = 0; i < d->topics->len; i++) {
			GtkListBoxRow *row = gtk_list_box_get_row_at_index (GTK_LIST_BOX (d->list), i);

			if (row != NULL && topic_filter (row, d)) {
				gtk_list_box_select_row (GTK_LIST_BOX (d->list), row);
				break;
			}
		}
	}
}

static GtkWidget *
build_page (const gchar *topic_id)
{
	GtkWidget *page, *paned, *left, *list_scroll, *bar, *close_button, *title;
	HelpPage *d = g_new0 (HelpPage, 1);
	guint i;

	d->topics = g_ptr_array_new_with_free_func ((GDestroyNotify) topic_free);
	d->root = find_help_root ();

	page = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	g_object_set_data_full (G_OBJECT (page), "nolphin-help-data", d, (GDestroyNotify) help_page_free);

	title = gtk_label_new (NULL);
	gtk_label_set_markup (GTK_LABEL (title), _("<b>Hilfe zu Nolphin</b>"));
	gtk_widget_set_halign (title, GTK_ALIGN_START);
	gtk_widget_set_margin_start (title, 24);
	gtk_widget_set_margin_top (title, 12);
	gtk_widget_set_margin_bottom (title, 8);
	gtk_box_pack_start (GTK_BOX (page), title, FALSE, FALSE, 0);

	if (d->root != NULL) {
		load_topics (d);
	}

	if (d->topics->len == 0) {
		GtkWidget *msg = gtk_label_new (_("Die Hilfetexte wurden nicht gefunden. Sie liegen im Ordner »help« der Nolphin-Daten (oder in NOLPHIN_HELPDIR)."));

		gtk_label_set_line_wrap (GTK_LABEL (msg), TRUE);
		gtk_widget_set_margin_start (msg, 24);
		gtk_widget_set_margin_end (msg, 24);
		gtk_widget_set_vexpand (msg, TRUE);
		gtk_widget_set_valign (msg, GTK_ALIGN_START);
		gtk_widget_set_halign (msg, GTK_ALIGN_START);
		gtk_box_pack_start (GTK_BOX (page), msg, TRUE, TRUE, 0);
		goto bottom;
	}

	paned = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
	gtk_widget_set_vexpand (paned, TRUE);
	gtk_box_pack_start (GTK_BOX (page), paned, TRUE, TRUE, 0);

	left = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_widget_set_size_request (left, 260, -1);
	d->search = gtk_search_entry_new ();
	gtk_entry_set_placeholder_text (GTK_ENTRY (d->search), _("Themen durchsuchen"));
	gtk_widget_set_margin_start (d->search, 8);
	gtk_widget_set_margin_end (d->search, 8);
	gtk_box_pack_start (GTK_BOX (left), d->search, FALSE, FALSE, 0);

	d->list = gtk_list_box_new ();
	gtk_list_box_set_selection_mode (GTK_LIST_BOX (d->list), GTK_SELECTION_SINGLE);
	gtk_list_box_set_filter_func (GTK_LIST_BOX (d->list), topic_filter, d, NULL);
	list_scroll = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (list_scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (list_scroll), d->list);
	gtk_box_pack_start (GTK_BOX (left), list_scroll, TRUE, TRUE, 0);
	gtk_paned_pack1 (GTK_PANED (paned), left, FALSE, FALSE);

	d->view = nolphin_markdown_view_new ();
	nolphin_markdown_view_set_link_handler (d->view, on_link_clicked, d);
	gtk_text_view_set_left_margin (GTK_TEXT_VIEW (d->view), 24);
	gtk_text_view_set_right_margin (GTK_TEXT_VIEW (d->view), 24);
	d->scrolled = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (d->scrolled), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (d->scrolled), d->view);
	gtk_paned_pack2 (GTK_PANED (paned), d->scrolled, TRUE, FALSE);

	for (i = 0; i < d->topics->len; i++) {
		HelpTopic *t = g_ptr_array_index (d->topics, i);
		GtkWidget *label = gtk_label_new (t->title);
		GtkWidget *row = gtk_list_box_row_new ();

		gtk_label_set_xalign (GTK_LABEL (label), 0.0);
		gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
		gtk_widget_set_margin_start (label, 12);
		gtk_widget_set_margin_end (label, 12);
		gtk_widget_set_margin_top (label, 6);
		gtk_widget_set_margin_bottom (label, 6);
		gtk_container_add (GTK_CONTAINER (row), label);
		gtk_list_box_insert (GTK_LIST_BOX (d->list), row, -1);
	}
	g_signal_connect (d->list, "row-selected", G_CALLBACK (on_topic_selected), d);
	g_signal_connect (d->search, "search-changed", G_CALLBACK (on_search_changed), d);

bottom:
	bar = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_widget_set_margin_start (bar, 24);
	gtk_widget_set_margin_end (bar, 24);
	gtk_widget_set_margin_top (bar, 12);
	gtk_widget_set_margin_bottom (bar, 12);
	close_button = gtk_button_new_with_label (_("Schließen"));
	gtk_widget_set_size_request (close_button, 120, -1);
	g_signal_connect (close_button, "clicked", G_CALLBACK (on_close_clicked), NULL);
	gtk_box_pack_end (GTK_BOX (bar), close_button, FALSE, FALSE, 0);
	gtk_box_pack_end (GTK_BOX (page), bar, FALSE, FALSE, 0);

	gtk_widget_show_all (page);

	if (d->topics->len > 0) {
		select_topic_by_id (d, topic_id != NULL ? topic_id : ((HelpTopic *) g_ptr_array_index (d->topics, 0))->id);
		if (gtk_list_box_get_selected_row (GTK_LIST_BOX (d->list)) == NULL) {
			gtk_list_box_select_row (GTK_LIST_BOX (d->list), gtk_list_box_get_row_at_index (GTK_LIST_BOX (d->list), 0));
		}
	}
	return page;
}

void
nolphin_help_show (GtkWindow *window, const gchar *topic_id)
{
	GtkWidget *stack;

	g_return_if_fail (GTK_IS_WINDOW (window));

	stack = g_object_get_data (G_OBJECT (window), "nolphin-content-stack");
	if (stack == NULL) {
		return;
	}

	if (help_page != NULL && help_host == GTK_WIDGET (window)) {
		if (topic_id != NULL) {
			HelpPage *d = g_object_get_data (G_OBJECT (help_page), "nolphin-help-data");

			if (d != NULL && d->list != NULL) {
				select_topic_by_id (d, topic_id);
			}
		}
		gtk_stack_set_visible_child_name (GTK_STACK (stack), "help");
		return;
	}
	if (help_page != NULL) {
		close_help ();
	}

	help_page = build_page (topic_id);
	help_host = GTK_WIDGET (window);
	g_signal_connect (help_page, "destroy", G_CALLBACK (on_page_destroy), NULL);
	gtk_stack_add_named (GTK_STACK (stack), help_page, "help");
	help_key_handler = g_signal_connect (window, "key-press-event", G_CALLBACK (on_help_key_press), NULL);
	gtk_stack_set_visible_child_name (GTK_STACK (stack), "help");
}
