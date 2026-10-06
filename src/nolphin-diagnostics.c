/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nolphin-diagnostics.c: Verwaltung und Diagnose (§48)
 */

#include <config.h>

#include "nolphin-diagnostics.h"
#include "nolphin-workspace-panel.h"

#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include <locale.h>
#include <stdio.h>
#include <string.h>
#include <libnolphin-private/nolphin-global-preferences.h>
#include <libnolphin-private/nolphin-module.h>

#define LOG_MAX_BYTES (1024 * 1024)

static FILE *log_file = NULL;

/* --- Protokoll ------------------------------------------------------------- */

gchar *
nolphin_diagnostics_get_log_path (void)
{
	return g_build_filename (g_get_user_data_dir (), "nolphin", "logs", "nolphin.log", NULL);
}

static const gchar *
level_name (GLogLevelFlags level)
{
	switch (level & G_LOG_LEVEL_MASK) {
	case G_LOG_LEVEL_ERROR:    return "FATAL";
	case G_LOG_LEVEL_CRITICAL: return "KRITISCH";
	case G_LOG_LEVEL_WARNING:  return "WARNUNG";
	case G_LOG_LEVEL_MESSAGE:  return "MELDUNG";
	case G_LOG_LEVEL_INFO:     return "INFO";
	case G_LOG_LEVEL_DEBUG:    return "DEBUG";
	default:                   return "LOG";
	}
}

static void
log_handler (const gchar *domain, GLogLevelFlags level, const gchar *message, gpointer data)
{
	if (log_file != NULL && (level & (G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL |
					  G_LOG_LEVEL_WARNING | G_LOG_LEVEL_MESSAGE))) {
		GDateTime *now = g_date_time_new_now_local ();
		gchar *stamp = g_date_time_format (now, "%Y-%m-%d %H:%M:%S");

		fprintf (log_file, "%s [%s] %s: %s\n", stamp, level_name (level),
			 domain != NULL ? domain : "-", message != NULL ? message : "");
		fflush (log_file);
		g_free (stamp);
		g_date_time_unref (now);
	}
	g_log_default_handler (domain, level, message, NULL);
}

void
nolphin_diagnostics_init_logging (void)
{
	gchar *path = nolphin_diagnostics_get_log_path ();
	gchar *dir = g_path_get_dirname (path);
	GStatBuf st;

	if (log_file != NULL) {
		g_free (path);
		g_free (dir);
		return;
	}

	g_mkdir_with_parents (dir, 0700);

	/* Einfache Rotation: ab 1 MiB wird das alte Protokoll zu nolphin.log.1 */
	if (g_stat (path, &st) == 0 && st.st_size > LOG_MAX_BYTES) {
		gchar *old = g_strconcat (path, ".1", NULL);

		g_rename (path, old);
		g_free (old);
	}

	log_file = g_fopen (path, "a");
	if (log_file != NULL) {
		GDateTime *now = g_date_time_new_now_local ();
		gchar *stamp = g_date_time_format (now, "%Y-%m-%d %H:%M:%S");

		fprintf (log_file, "%s [START] Nolphin %s\n", stamp, VERSION);
		fflush (log_file);
		g_free (stamp);
		g_date_time_unref (now);
		g_log_set_default_handler (log_handler, NULL);
	}

	g_free (path);
	g_free (dir);
}

gchar *
nolphin_diagnostics_read_log_tail (guint max_lines)
{
	gchar *path = nolphin_diagnostics_get_log_path ();
	gchar *contents = NULL;
	gsize len = 0;
	gchar *result;

	if (!g_file_get_contents (path, &contents, &len, NULL)) {
		g_free (path);
		return g_strdup (_("Es gibt noch kein Protokoll."));
	}
	g_free (path);

	/* nur die letzten max_lines Zeilen */
	{
		gchar *p = contents + len;
		guint lines = 0;

		while (p > contents && lines <= max_lines) {
			p--;
			if (*p == '\n' && p != contents + len - 1) {
				lines++;
			}
		}
		result = g_utf8_make_valid (lines > max_lines ? p + 1 : contents, -1);
	}
	g_free (contents);

	if (result[0] == '\0') {
		g_free (result);
		return g_strdup (_("Das Protokoll ist leer."));
	}
	return result;
}

/* --- Systeminformationen -------------------------------------------------------- */

static gchar *
os_pretty_name (void)
{
	gchar *contents = NULL, *name = NULL;

	if (g_file_get_contents ("/etc/os-release", &contents, NULL, NULL)) {
		gchar **lines = g_strsplit (contents, "\n", -1);
		guint i;

		for (i = 0; lines[i] != NULL; i++) {
			if (g_str_has_prefix (lines[i], "PRETTY_NAME=")) {
				name = g_shell_unquote (lines[i] + 12, NULL);
				break;
			}
		}
		g_strfreev (lines);
		g_free (contents);
	}
	return name != NULL ? name : g_strdup (_("unbekannt"));
}

gboolean
nolphin_is_development_build (gchar **path)
{
	static gchar *exe = NULL;
	static gboolean dev = FALSE;
	static gboolean known = FALSE;

	if (!known) {
		exe = g_file_read_link ("/proc/self/exe", NULL);
		/* Das installierte Paket liegt unter /usr/ (Pakete) – alles andere gilt als Entwicklung. */
		dev = exe != NULL && !g_str_has_prefix (exe, "/usr/");
		known = TRUE;
	}
	if (path != NULL) {
		*path = g_strdup (exe);
	}
	return dev;
}

gchar *
nolphin_diagnostics_system_info (void)
{
	static const gchar *tools[] = {
		"tar", "gzip", "bzip2", "xz", "zstd", "lz4", "zip", "unzip", "7z", "unrar",
		"rsync", "gpg", "git", "getfacl", "setfacl", "b2sum", "dconf", "dpkg-deb",
		"file-roller", "libreoffice", "diff", "pdfinfo", "pdftocairo", "gst-discoverer-1.0", NULL
	};
	GString *s = g_string_new (NULL);
	gchar *os = os_pretty_name ();
	const gchar * const *schemes;
	GtkSettings *settings = gtk_settings_get_default ();
	gchar *gtk_theme = NULL, *icon_theme = NULL;
	guint i;

	g_string_append_printf (s, "%s\n", _("== Programm =="));
	{
		gchar *exe = NULL;
		gboolean dev = nolphin_is_development_build (&exe);

		g_string_append_printf (s, "Nolphin %s%s\n", VERSION, dev ? _(" (Entwicklungsstand)") : "");
		if (exe != NULL) {
			g_string_append_printf (s, _("Gestartet aus: %s\n"), exe);
		}
		g_free (exe);
	}
	g_string_append_printf (s, "GTK %u.%u.%u, GLib %u.%u.%u\n",
				gtk_get_major_version (), gtk_get_minor_version (), gtk_get_micro_version (),
				glib_major_version, glib_minor_version, glib_micro_version);

	g_string_append_printf (s, "\n%s\n", _("== System =="));
	g_string_append_printf (s, "%s: %s\n", _("Betriebssystem"), os);
	g_string_append_printf (s, "%s: %s (%s)\n", _("Desktop"),
				g_getenv ("XDG_CURRENT_DESKTOP") != NULL ? g_getenv ("XDG_CURRENT_DESKTOP") : "-",
				g_getenv ("XDG_SESSION_TYPE") != NULL ? g_getenv ("XDG_SESSION_TYPE") : "-");
	g_string_append_printf (s, "%s: %s\n", _("Sprache"), setlocale (LC_MESSAGES, NULL) != NULL ? setlocale (LC_MESSAGES, NULL) : "-");
	if (settings != NULL) {
		g_object_get (settings, "gtk-theme-name", &gtk_theme, "gtk-icon-theme-name", &icon_theme, NULL);
		g_string_append_printf (s, "%s: %s, %s: %s\n", _("GTK-Theme"), gtk_theme != NULL ? gtk_theme : "-",
					_("Symbole"), icon_theme != NULL ? icon_theme : "-");
	}

	g_string_append_printf (s, "\n%s\n", _("== Verfügbare GVFS-Backends (Adressen) =="));
	schemes = g_vfs_get_supported_uri_schemes (g_vfs_get_default ());
	for (i = 0; schemes != NULL && schemes[i] != NULL; i++) {
		g_string_append_printf (s, "%s%s", i > 0 ? ", " : "", schemes[i]);
	}
	g_string_append_c (s, '\n');

	g_string_append_printf (s, "\n%s\n", _("== Gefundene Werkzeuge =="));
	for (i = 0; tools[i] != NULL; i++) {
		gchar *p = g_find_program_in_path (tools[i]);

		g_string_append_printf (s, "%-20s %s\n", tools[i], p != NULL ? p : _("fehlt"));
		g_free (p);
	}

	g_free (os);
	g_free (gtk_theme);
	g_free (icon_theme);
	return g_string_free (s, FALSE);
}

gchar *
nolphin_diagnostics_plugin_status (void)
{
	GString *s = g_string_new (NULL);
	const GList *l;
	gchar **disabled = g_settings_get_strv (nolphin_plugin_preferences, NOLPHIN_PLUGIN_PREFERENCES_DISABLED_EXTENSIONS);
	guint i;

	g_string_append_printf (s, "%s\n", _("== Erweiterungen (Module) =="));
	if (nolphin_module_get_status () == NULL) {
		g_string_append (s, _("Keine Erweiterungen gefunden.\n"));
	}
	for (l = nolphin_module_get_status (); l != NULL; l = l->next) {
		const NolphinModuleStatus *st = l->data;

		if (st->loaded) {
			g_string_append_printf (s, "%s %s\n", _("[geladen]"), st->path);
		} else {
			g_string_append_printf (s, "%s %s\n    %s: %s\n", _("[FEHLER]"), st->path, _("Ladefehler"), st->error);
		}
	}

	g_string_append_printf (s, "\n%s\n", _("== Deaktivierte Erweiterungen =="));
	if (disabled == NULL || disabled[0] == NULL) {
		g_string_append (s, _("Keine.\n"));
	}
	for (i = 0; disabled != NULL && disabled[i] != NULL; i++) {
		g_string_append_printf (s, "%s\n", disabled[i]);
	}
	g_strfreev (disabled);

	return g_string_free (s, FALSE);
}

gboolean
nolphin_diagnostics_write_report (const gchar *path, GError **error)
{
	gchar *sys = nolphin_diagnostics_system_info ();
	gchar *plugins = nolphin_diagnostics_plugin_status ();
	gchar *log = nolphin_diagnostics_read_log_tail (200);
	GDateTime *now = g_date_time_new_now_local ();
	gchar *stamp = g_date_time_format (now, "%Y-%m-%d %H:%M:%S");
	gchar *report = g_strdup_printf ("%s %s\n%s\n\n%s\n%s\n\n%s\n%s\n",
					 _("Nolphin-Fehlerbericht vom"), stamp,
					 _("(Lokal erzeugt, wird nicht automatisch gesendet.)"),
					 sys, plugins, _("== Protokoll (letzte 200 Zeilen) =="), log);
	gboolean ok = g_file_set_contents (path, report, -1, error);

	g_free (report);
	g_free (stamp);
	g_date_time_unref (now);
	g_free (log);
	g_free (plugins);
	g_free (sys);
	return ok;
}

/* --- Seite im Arbeitsbereich -------------------------------------------------- */

typedef struct {
	NolphinWindow *window;
	GtkWidget *notebook;
	GtkWidget *log_view;
	GtkWidget *system_view;
	GtkWidget *plugin_view;
	GtkWidget *report_status;
} DiagPage;

static GtkWidget *
text_view_in_scroller (GtkWidget **out_view)
{
	GtkWidget *scroller = gtk_scrolled_window_new (NULL, NULL);
	GtkWidget *view = gtk_text_view_new ();

	gtk_text_view_set_editable (GTK_TEXT_VIEW (view), FALSE);
	gtk_text_view_set_monospace (GTK_TEXT_VIEW (view), TRUE);
	gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (view), GTK_WRAP_WORD_CHAR);
	gtk_text_view_set_left_margin (GTK_TEXT_VIEW (view), 6);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroller), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_widget_set_size_request (scroller, -1, 260);
	gtk_container_add (GTK_CONTAINER (scroller), view);
	*out_view = view;
	return scroller;
}

static void
set_view_text (GtkWidget *view, const gchar *text)
{
	gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (view)), text, -1);
}

static void
diag_refresh (DiagPage *d)
{
	gchar *t;
	GtkTextIter end;

	t = nolphin_diagnostics_read_log_tail (500);
	set_view_text (d->log_view, t);
	g_free (t);
	/* ans Ende scrollen: dort stehen die neuesten Einträge */
	gtk_text_buffer_get_end_iter (gtk_text_view_get_buffer (GTK_TEXT_VIEW (d->log_view)), &end);
	gtk_text_view_scroll_to_iter (GTK_TEXT_VIEW (d->log_view), &end, 0.0, FALSE, 0.0, 0.0);

	t = nolphin_diagnostics_system_info ();
	set_view_text (d->system_view, t);
	g_free (t);

	t = nolphin_diagnostics_plugin_status ();
	set_view_text (d->plugin_view, t);
	g_free (t);
}

static void
on_refresh_clicked (GtkButton *button, gpointer data)
{
	diag_refresh (data);
}

static void
on_copy_system_clicked (GtkButton *button, gpointer data)
{
	DiagPage *d = data;
	gchar *t = nolphin_diagnostics_system_info ();

	gtk_clipboard_set_text (gtk_clipboard_get_for_display (gtk_widget_get_display (GTK_WIDGET (button)),
							       GDK_SELECTION_CLIPBOARD), t, -1);
	g_free (t);
	(void) d;
}

static void
on_report_clicked (GtkButton *button, gpointer data)
{
	DiagPage *d = data;
	GtkWidget *toplevel = gtk_widget_get_toplevel (GTK_WIDGET (button));
	GtkWidget *chooser = gtk_file_chooser_dialog_new (_("Fehlerbericht speichern"),
							  GTK_IS_WINDOW (toplevel) ? GTK_WINDOW (toplevel) : NULL,
							  GTK_FILE_CHOOSER_ACTION_SAVE,
							  _("_Abbrechen"), GTK_RESPONSE_CANCEL,
							  _("_Speichern"), GTK_RESPONSE_ACCEPT, NULL);
	GDateTime *now = g_date_time_new_now_local ();
	gchar *name = g_date_time_format (now, "nolphin-fehlerbericht-%Y%m%d-%H%M%S.txt");

	gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (chooser), TRUE);
	gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (chooser), name);
	g_free (name);
	g_date_time_unref (now);

	if (gtk_dialog_run (GTK_DIALOG (chooser)) == GTK_RESPONSE_ACCEPT) {
		gchar *path = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (chooser));
		GError *error = NULL;

		if (path != NULL && nolphin_diagnostics_write_report (path, &error)) {
			gchar *msg = g_strdup_printf (_("Fehlerbericht gespeichert: %s"), path);

			gtk_label_set_text (GTK_LABEL (d->report_status), msg);
			g_free (msg);
		} else {
			gtk_label_set_text (GTK_LABEL (d->report_status), error != NULL ? error->message : _("Speichern fehlgeschlagen."));
			g_clear_error (&error);
		}
		g_free (path);
	}
	gtk_widget_destroy (chooser);
}

static void
on_back_clicked (GtkButton *button, gpointer data)
{
	DiagPage *d = data;

	nolphin_workspace_panel_show_preview (nolphin_window_get_workspace_panel (d->window));
}

static GtkWidget *
button_row_with (const gchar *label, GCallback cb, gpointer data)
{
	GtkWidget *b = gtk_button_new_with_label (label);

	g_signal_connect (b, "clicked", cb, data);
	gtk_widget_set_halign (b, GTK_ALIGN_START);
	return b;
}

GtkWidget *
nolphin_diagnostics_page_new (NolphinWindow *window)
{
	DiagPage *d = g_new0 (DiagPage, 1);
	GtkWidget *outer, *back, *title, *tab, *box, *scroller, *b, *intro;

	d->window = window;

	outer = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
	gtk_container_set_border_width (GTK_CONTAINER (outer), 12);

	back = gtk_button_new_with_label (_("Zur Vorschau"));
	gtk_button_set_image (GTK_BUTTON (back), gtk_image_new_from_icon_name ("go-previous-symbolic", GTK_ICON_SIZE_BUTTON));
	gtk_button_set_always_show_image (GTK_BUTTON (back), TRUE);
	gtk_widget_set_halign (back, GTK_ALIGN_START);
	g_signal_connect (back, "clicked", G_CALLBACK (on_back_clicked), d);
	gtk_box_pack_start (GTK_BOX (outer), back, FALSE, FALSE, 0);

	title = gtk_label_new (_("Diagnose"));
	gtk_widget_set_halign (title, GTK_ALIGN_START);
	gtk_style_context_add_class (gtk_widget_get_style_context (title), "heading");
	gtk_box_pack_start (GTK_BOX (outer), title, FALSE, FALSE, 0);

	d->notebook = gtk_notebook_new ();
	gtk_box_pack_start (GTK_BOX (outer), d->notebook, TRUE, TRUE, 0);

	/* Protokolle */
	box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_container_set_border_width (GTK_CONTAINER (box), 8);
	scroller = text_view_in_scroller (&d->log_view);
	gtk_box_pack_start (GTK_BOX (box), scroller, TRUE, TRUE, 0);
	b = button_row_with (_("Aktualisieren"), G_CALLBACK (on_refresh_clicked), d);
	gtk_box_pack_start (GTK_BOX (box), b, FALSE, FALSE, 0);
	tab = gtk_label_new (_("Protokolle"));
	gtk_notebook_append_page (GTK_NOTEBOOK (d->notebook), box, tab);

	/* Systeminformationen */
	box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_container_set_border_width (GTK_CONTAINER (box), 8);
	scroller = text_view_in_scroller (&d->system_view);
	gtk_box_pack_start (GTK_BOX (box), scroller, TRUE, TRUE, 0);
	b = button_row_with (_("In die Zwischenablage kopieren"), G_CALLBACK (on_copy_system_clicked), d);
	gtk_box_pack_start (GTK_BOX (box), b, FALSE, FALSE, 0);
	tab = gtk_label_new (_("Systeminformationen"));
	gtk_notebook_append_page (GTK_NOTEBOOK (d->notebook), box, tab);

	/* Plugin-Status */
	box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_container_set_border_width (GTK_CONTAINER (box), 8);
	scroller = text_view_in_scroller (&d->plugin_view);
	gtk_box_pack_start (GTK_BOX (box), scroller, TRUE, TRUE, 0);
	tab = gtk_label_new (_("Plugin-Status"));
	gtk_notebook_append_page (GTK_NOTEBOOK (d->notebook), box, tab);

	/* Fehlerbericht */
	box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
	gtk_container_set_border_width (GTK_CONTAINER (box), 8);
	intro = gtk_label_new (_("Erzeugt eine Textdatei mit Systeminformationen, Plugin-Status und den letzten Protokollzeilen. "
				 "Sie wird nur lokal gespeichert und nie automatisch gesendet."));
	gtk_label_set_line_wrap (GTK_LABEL (intro), TRUE);
	gtk_widget_set_halign (intro, GTK_ALIGN_START);
	gtk_box_pack_start (GTK_BOX (box), intro, FALSE, FALSE, 0);
	b = button_row_with (_("Fehlerbericht erstellen …"), G_CALLBACK (on_report_clicked), d);
	gtk_style_context_add_class (gtk_widget_get_style_context (b), "suggested-action");
	gtk_box_pack_start (GTK_BOX (box), b, FALSE, FALSE, 0);
	d->report_status = gtk_label_new ("");
	gtk_label_set_line_wrap (GTK_LABEL (d->report_status), TRUE);
	gtk_label_set_selectable (GTK_LABEL (d->report_status), TRUE);
	gtk_widget_set_halign (d->report_status, GTK_ALIGN_START);
	gtk_box_pack_start (GTK_BOX (box), d->report_status, FALSE, FALSE, 0);
	tab = gtk_label_new (_("Fehlerbericht"));
	gtk_notebook_append_page (GTK_NOTEBOOK (d->notebook), box, tab);

	g_object_set_data_full (G_OBJECT (outer), "diag-page", d, g_free);
	diag_refresh (d);
	return outer;
}

void
nolphin_diagnostics_page_show_tab (GtkWidget *page, NolphinDiagTab tab)
{
	DiagPage *d = g_object_get_data (G_OBJECT (page), "diag-page");

	if (d == NULL) {
		return;
	}
	diag_refresh (d);
	gtk_notebook_set_current_page (GTK_NOTEBOOK (d->notebook), (gint) tab);
}
