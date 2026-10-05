/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nolphin-gid.c: GID-Projekte (§60)
 */

#include <config.h>

#include "nolphin-gid.h"
#include "nolphin-markdown-view.h"
#include "nolphin-tools.h"

#include <glib/gi18n.h>
#include <string.h>

/* Größer als das lädt die Seite nicht (die Textvorschau hat ebenfalls eine Grenze). */
#define README_MAX_BYTES (1024 * 1024)

/* --- Panel-Seite --------------------------------------------------------------- */

typedef struct {
	NolphinWindow *window;
	GtkWidget     *title;
	GtkWidget     *path_label;
	GtkWidget     *branch_label;
	GtkWidget     *status_label;
	GtkWidget     *stack;       /* "readme" | "message" */
	GtkWidget     *message;
	GtkWidget     *view;
	GFile         *folder;
	GCancellable  *cancellable;
} GidPage;

static void
gid_page_free (gpointer data)
{
	GidPage *d = data;

	g_cancellable_cancel (d->cancellable);
	g_object_unref (d->cancellable);
	g_clear_object (&d->folder);
	g_free (d);
}

static void
gid_page_show_message (GidPage *d, const gchar *text)
{
	gtk_label_set_text (GTK_LABEL (d->message), text);
	gtk_stack_set_visible_child_name (GTK_STACK (d->stack), "message");
}

/* Findet README.md (ohne Beachtung der Groß-/Kleinschreibung) im Projektordner. */
static GFile *
find_readme (GFile *folder)
{
	GFileEnumerator *en;
	GFileInfo *info;
	GFile *found = NULL;

	en = g_file_enumerate_children (folder, G_FILE_ATTRIBUTE_STANDARD_NAME "," G_FILE_ATTRIBUTE_STANDARD_TYPE,
					G_FILE_QUERY_INFO_NONE, NULL, NULL);
	if (en == NULL)
		return NULL;
	while (found == NULL && (info = g_file_enumerator_next_file (en, NULL, NULL)) != NULL) {
		const gchar *name = g_file_info_get_name (info);

		if (g_file_info_get_file_type (info) != G_FILE_TYPE_DIRECTORY &&
		    (g_ascii_strcasecmp (name, "README.md") == 0 ||
		     g_ascii_strcasecmp (name, "README.markdown") == 0))
			found = g_file_get_child (folder, name);
		g_object_unref (info);
	}
	g_object_unref (en);
	return found;
}

static void
on_readme_loaded (GObject *source, GAsyncResult *result, gpointer user_data)
{
	GWeakRef *ref = user_data;
	gchar *contents = NULL;
	gsize length = 0;
	GError *error = NULL;
	GtkWidget *page;
	GidPage *d;

	g_file_load_contents_finish (G_FILE (source), result, &contents, &length, NULL, &error);
	page = g_weak_ref_get (ref);

	if (page != NULL && !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		d = g_object_get_data (G_OBJECT (page), "gid-page-data");
		if (d != NULL) {
			if (error != NULL) {
				gchar *msg = g_strdup_printf (_("Die README.md konnte nicht gelesen werden: %s"),
							      error->message);
				gid_page_show_message (d, msg);
				g_free (msg);
			} else if (length > README_MAX_BYTES) {
				gid_page_show_message (d, _("Die README.md ist zu groß für die Anzeige."));
			} else {
				gchar *text = g_strndup (contents, length);

				gchar *base = d->folder != NULL ? g_file_get_path (d->folder) : NULL;

				nolphin_markdown_view_set_text_with_base (d->view, text, base);
				g_free (base);
				g_free (text);
				gtk_stack_set_visible_child_name (GTK_STACK (d->stack), "readme");
			}
		}
	}
	if (page != NULL)
		g_object_unref (page);
	g_clear_error (&error);
	g_free (contents);
	g_weak_ref_clear (ref);
	g_free (ref);
}

void
nolphin_gid_page_open (GtkWidget *page, GFile *project_folder)
{
	GidPage *d;
	gchar *path, *name, *branch;
	GFile *readme;
	GWeakRef *ref;

	g_return_if_fail (GTK_IS_WIDGET (page));
	g_return_if_fail (G_IS_FILE (project_folder));

	d = g_object_get_data (G_OBJECT (page), "gid-page-data");
	g_return_if_fail (d != NULL);

	/* laufendes Laden verwerfen */
	g_cancellable_cancel (d->cancellable);
	g_object_unref (d->cancellable);
	d->cancellable = g_cancellable_new ();

	g_set_object (&d->folder, project_folder);
	path = g_file_get_path (project_folder);
	name = g_file_get_basename (project_folder);
	gtk_label_set_text (GTK_LABEL (d->title), name);
	gtk_label_set_text (GTK_LABEL (d->path_label), path != NULL ? path : "");
	gtk_label_set_text (GTK_LABEL (d->status_label), "");

	branch = path != NULL ? nolphin_gid_project_get_branch (path) : NULL;
	if (branch != NULL) {
		gchar *text = g_strdup_printf (_("Branch: %s"), branch);

		gtk_label_set_text (GTK_LABEL (d->branch_label), text);
		g_free (text);
	} else {
		gtk_label_set_text (GTK_LABEL (d->branch_label), "");
	}
	gtk_widget_set_visible (d->branch_label, branch != NULL);
	g_free (branch);
	g_free (name);

	if (path == NULL || !g_file_query_exists (project_folder, NULL)) {
		gid_page_show_message (d, _("Der Projektordner ist nicht verfügbar."));
		g_free (path);
		return;
	}
	g_free (path);

	readme = find_readme (project_folder);
	if (readme == NULL) {
		gid_page_show_message (d, _("In diesem Projekt gibt es keine README.md."));
		return;
	}

	ref = g_new0 (GWeakRef, 1);
	g_weak_ref_init (ref, page);
	g_file_load_contents_async (readme, d->cancellable, on_readme_loaded, ref);
	g_object_unref (readme);
}

/* Klick auf einen Link der README (§60.3). */
static void
on_link_clicked (GtkWidget *view, const gchar *href, gpointer user_data)
{
	GidPage *d = user_data;
	gchar *scheme = g_uri_parse_scheme (href);
	gchar *rel, *unescaped, *root_path, *target_path;
	GFile *target;
	const gchar *cut;

	if (scheme != NULL) {
		g_app_info_launch_default_for_uri (href, NULL, NULL);
		g_free (scheme);
		return;
	}
	if (href[0] == '#' || d->folder == NULL)
		return;

	rel = g_strdup (href);
	cut = strpbrk (rel, "#?");
	if (cut != NULL)
		rel[cut - rel] = '\0';
	unescaped = g_uri_unescape_string (rel, NULL);
	g_free (rel);
	if (unescaped == NULL || *unescaped == '\0') {
		g_free (unescaped);
		return;
	}

	target = g_file_resolve_relative_path (d->folder, unescaped);
	g_free (unescaped);
	target_path = g_file_get_path (target);
	root_path = g_file_get_path (d->folder);

	/* Nur Ziele innerhalb des Projekts. */
	if (target_path == NULL || root_path == NULL ||
	    !(g_str_has_prefix (target_path, root_path) &&
	      (target_path[strlen (root_path)] == '\0' || target_path[strlen (root_path)] == G_DIR_SEPARATOR))) {
		gtk_label_set_text (GTK_LABEL (d->status_label), _("Das Linkziel liegt außerhalb des Projekts."));
	} else if (!g_file_query_exists (target, NULL)) {
		gchar *msg = g_strdup_printf (_("Linkziel nicht gefunden: %s"), href);

		gtk_label_set_text (GTK_LABEL (d->status_label), msg);
		g_free (msg);
	} else {
		GFile *show = target;
		GFile *parent = NULL;

		gtk_label_set_text (GTK_LABEL (d->status_label), "");
		if (g_file_query_file_type (target, G_FILE_QUERY_INFO_NONE, NULL) != G_FILE_TYPE_DIRECTORY) {
			parent = g_file_get_parent (target);
			if (parent != NULL)
				show = parent;
		}
		nolphin_window_go_to (d->window, show);
		g_clear_object (&parent);
	}
	g_free (target_path);
	g_free (root_path);
	g_object_unref (target);
}

GtkWidget *
nolphin_gid_page_new (NolphinWindow *window)
{
	GidPage *d = g_new0 (GidPage, 1);
	GtkWidget *outer, *header, *scroller;

	d->window = window;
	d->cancellable = g_cancellable_new ();

	outer = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_container_set_border_width (GTK_CONTAINER (outer), 12);
	gtk_box_pack_start (GTK_BOX (outer), nolphin_tools_back_button_new (window), FALSE, FALSE, 0);

	/* schmale Kopfzeile: Projektname, Pfad, Branch */
	header = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
	d->title = nolphin_tools_heading_new (_("GID-Projekt"));
	gtk_label_set_selectable (GTK_LABEL (d->title), TRUE);
	/* Lange Namen/Pfade dürfen das Panel nicht verbreitern. */
	gtk_label_set_ellipsize (GTK_LABEL (d->title), PANGO_ELLIPSIZE_END);
	gtk_label_set_width_chars (GTK_LABEL (d->title), 1);
	gtk_widget_set_halign (d->title, GTK_ALIGN_FILL);
	gtk_label_set_xalign (GTK_LABEL (d->title), 0.0);
	gtk_box_pack_start (GTK_BOX (header), d->title, FALSE, FALSE, 0);
	d->path_label = gtk_label_new ("");
	gtk_label_set_ellipsize (GTK_LABEL (d->path_label), PANGO_ELLIPSIZE_MIDDLE);
	gtk_label_set_selectable (GTK_LABEL (d->path_label), TRUE);
	gtk_label_set_width_chars (GTK_LABEL (d->path_label), 1);
	gtk_widget_set_halign (d->path_label, GTK_ALIGN_FILL);
	gtk_label_set_xalign (GTK_LABEL (d->path_label), 0.0);
	gtk_style_context_add_class (gtk_widget_get_style_context (d->path_label), "dim-label");
	gtk_box_pack_start (GTK_BOX (header), d->path_label, FALSE, FALSE, 0);
	d->branch_label = gtk_label_new ("");
	gtk_widget_set_halign (d->branch_label, GTK_ALIGN_START);
	gtk_widget_set_no_show_all (d->branch_label, TRUE);
	gtk_style_context_add_class (gtk_widget_get_style_context (d->branch_label), "dim-label");
	gtk_box_pack_start (GTK_BOX (header), d->branch_label, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (outer), header, FALSE, FALSE, 0);

	d->status_label = gtk_label_new ("");
	gtk_label_set_line_wrap (GTK_LABEL (d->status_label), TRUE);
	gtk_label_set_width_chars (GTK_LABEL (d->status_label), 1);
	gtk_widget_set_halign (d->status_label, GTK_ALIGN_FILL);
	gtk_label_set_xalign (GTK_LABEL (d->status_label), 0.0);
	gtk_box_pack_start (GTK_BOX (outer), d->status_label, FALSE, FALSE, 0);

	d->stack = gtk_stack_new ();
	d->view = nolphin_markdown_view_new ();
	nolphin_markdown_view_set_link_handler (d->view, on_link_clicked, d);
	scroller = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroller), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scroller), GTK_SHADOW_IN);
	gtk_container_add (GTK_CONTAINER (scroller), d->view);
	gtk_stack_add_named (GTK_STACK (d->stack), scroller, "readme");

	d->message = gtk_label_new ("");
	gtk_label_set_line_wrap (GTK_LABEL (d->message), TRUE);
	gtk_label_set_width_chars (GTK_LABEL (d->message), 1);
	gtk_widget_set_halign (d->message, GTK_ALIGN_FILL);
	gtk_label_set_xalign (GTK_LABEL (d->message), 0.0);
	gtk_widget_set_valign (d->message, GTK_ALIGN_START);
	gtk_style_context_add_class (gtk_widget_get_style_context (d->message), "dim-label");
	gtk_stack_add_named (GTK_STACK (d->stack), d->message, "message");
	gtk_box_pack_start (GTK_BOX (outer), d->stack, TRUE, TRUE, 0);

	gid_page_show_message (d, _("Wähle in der Seitenleiste ein GID-Projekt."));

	g_object_set_data_full (G_OBJECT (outer), "gid-page-data", d, gid_page_free);
	return outer;
}
