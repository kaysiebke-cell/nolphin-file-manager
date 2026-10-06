/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nolphin-help.c: Hilfe zu den Funktionen von Nolphin
 *
 * Textformat der Themen: "# " Überschrift, "## " Zwischenüberschrift,
 * "- " Aufzählungspunkt, "  " (zwei Leerzeichen) Code/Eingabe, sonst Absatz.
 * Die Texte beschreiben nur, was im Programm vorhanden ist.
 */

#include <config.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "nolphin-help.h"

typedef struct {
	const gchar *id;
	const gchar *title;
	const gchar *text;
} HelpTopic;

static const HelpTopic topics[] = {
	{ "ueberblick", N_("Überblick"),
	  "# Überblick\n"
	  "Nolphin zeigt links die Orte, in der Mitte die Dateien und rechts einen festen Arbeitsbereich. Die Funktion kommt zur Datei: Vorschau, Eigenschaften, Archive, Terminal, Git und die Werkzeuge öffnen sich rechts, ohne dass ein zusätzliches Fenster erscheint.\n"
	  "## Der Arbeitsbereich\n"
	  "- F11 blendet den Arbeitsbereich ein und aus. In Ruhe zeigt er die Vorschau und die Informationen zur gewählten Datei.\n"
	  "- F4 öffnet das Terminal im aktuellen Ordner.\n"
	  "- „Zur Vorschau“ oben im Panel führt zurück zur Vorschau.\n"
	  "- Klassische Dialoge gibt es nur für Dateiauswahl, Bestätigungen (Löschen, Überschreiben), Fehlermeldungen und „Über Nolphin“.\n"
	  "## Ehrlich statt vorgetäuscht\n"
	  "Fehlt ein Hilfsprogramm (zum Beispiel für PDF-Vorschau oder ein Archivformat), meldet Nolphin das. Unter Hilfe ▸ Diagnose ▸ Systeminformationen sehen Sie, welche Werkzeuge gefunden wurden." },

	{ "vorschau", N_("Vorschau und Metadaten"),
	  "# Vorschau und Metadaten\n"
	  "Wählen Sie eine Datei, zeigt der Arbeitsbereich Vorschau und Eckdaten.\n"
	  "## Was angezeigt wird\n"
	  "- Bilder, Text und Ordner mit Typ, Größe, Datum, Zugriffsrechten, Eigentümer und Ort.\n"
	  "- PDF: Titel, Autor, Seitenzahl, Seitengröße, PDF-Version und ein Bild der ersten Seite (benötigt poppler-utils).\n"
	  "- Audio und Video: Dauer, Container, Codecs, Auflösung, Bildrate, Kanäle, Abtastrate (benötigt gstreamer1.0-tools).\n"
	  "- CAD und 3D: STL, STEP, FreeCAD-Dateien u. a.\n"
	  "## Bewertung, Tags, Kommentar\n"
	  "Unten im Info-Panel können Sie Sterne vergeben (erneuter Klick auf denselben Stern entfernt die Bewertung), Tags kommagetrennt eintragen und einen Kommentar schreiben. Die Angaben werden als Datei-Metadaten gespeichert.\n"
	  "Fehlt ein Werkzeug, steht im Panel ein Hinweis statt erfundener Werte." },

	{ "suche", N_("Suche"),
	  "# Suche\n"
	  "Strg+F öffnet die Suche im Arbeitsbereich. Tippen Sie den Dateinamen ein und bestätigen Sie mit Eingabe. Der Schalter „Aa“ beachtet die Groß-/Kleinschreibung, „.*“ schaltet auf reguläre Ausdrücke um. Unter „Inhalt“ suchen Sie im Text von Dateien.\n"
	  "## Operatoren im Dateinamen\n"
	  "- Leerzeichen bedeutet UND: „jahr bericht“ findet Namen mit beiden Begriffen.\n"
	  "- ODER (auch OR oder |): „foto ODER bericht“. UND bindet stärker als ODER.\n"
	  "- NICHT (auch NOT oder ein Minus): „bericht NICHT entwurf“ oder „bericht -entwurf“.\n"
	  "- Anführungszeichen suchen den ganzen Namen exakt: „\"mein bericht.txt\"“.\n"
	  "- * und ? sind Platzhalter: „*.txt“.\n"
	  "Die Schlüsselwörter gelten unabhängig von der Schreibweise. Wollen Sie nach einem Namen suchen, der selbst „nicht“ oder „oder“ lautet, setzen Sie ihn in Anführungszeichen.\n"
	  "## Auswahl merken\n"
	  "Bearbeiten ▸ „Auswahl speichern …“ merkt die aktuelle Auswahl eines Ordners unter einem Namen; „Gespeicherte Auswahl wiederherstellen …“ holt sie zurück." },

	{ "archive", N_("Archive"),
	  "# Archive\n"
	  "## Erstellen und Entpacken\n"
	  "Dateien markieren, Rechtsklick ▸ „Komprimieren …“. Im Panel wählen Sie Name, Format und Zielort; Passwort und Teilarchive stehen unter „Erweiterte Optionen“. Erstellt werden ZIP, TAR, TAR.GZ, TAR.BZ2, TAR.XZ, TAR.ZST, TAR.LZ4 und 7Z. Entpacken geht über „Hier entpacken“ oder „Entpacken nach …“; CAB, ARJ, LZH, ISO, CPIO, RPM und DEB lassen sich nur entpacken und auslesen.\n"
	  "## Archiv-Manager\n"
	  "Rechtsklick auf ein Archiv ▸ „Archiv öffnen …“ zeigt den Inhalt. Dort können Sie Dateien oder Ordner hinzufügen, einen Eintrag ersetzen oder entfernen, das Archiv prüfen und einzelne Einträge entpacken. Bei Formaten, die dafür neu gepackt werden müssen, geschieht das automatisch.\n"
	  "Ist das nötige Programm (zip, unzip, tar, 7z …) nicht installiert, wird das angezeigt." },

	{ "git", N_("Git"),
	  "# Git\n"
	  "## Overlays\n"
	  "In einem Git-Ordner zeigen kleine Symbole an den Dateien den Stand: hinzugefügt, geändert, unversioniert oder im Konflikt. Der Stand wird im Hintergrund geladen und aktualisiert sich selbst.\n"
	  "## Panel\n"
	  "Rechtsklick ▸ „Git“ öffnet das Panel in drei Schritten:\n"
	  "- 1. Änderungen: Status anzeigen, Dateien hinzufügen.\n"
	  "- 2. Speichern: Beschreibung eintragen und committen.\n"
	  "- 3. Mit dem Server: synchronisieren, abgleichen, herunterladen (Pull), hochladen (Push), Server eintragen, Repository klonen.\n"
	  "Darunter stehen Verlauf und Unterschiede. Liegt der Ort in keinem Repository, bietet Nolphin an, eines anzulegen." },

	{ "werkzeuge", N_("Werkzeuge"),
	  "# Werkzeuge\n"
	  "Alle Werkzeuge finden Sie unter Bearbeiten ▸ Werkzeuge. Jedes zeigt zuerst eine Vorschau; verändert wird erst nach „Anwenden“ oder „Synchronisieren“.\n"
	  "## Ordner vergleichen und synchronisieren\n"
	  "Wählen Sie Quelle und Zielordner (auch ein eingebundener Netzwerkordner) und „Vergleichen (Vorschau)“. Die Liste zeigt Neu, Geändert und Konflikt (Ziel ist neuer). Abgeglichen wird in eine Richtung, im Ziel wird nie etwas gelöscht. Konflikte sind zunächst nicht angehakt. Dafür wird rsync benötigt.\n"
	  "## Regeln anwenden\n"
	  "„Wenn“ Dateityp, Name (mit Platzhaltern), Größe oder Änderungsdatum zutreffen, „dann“ eine Aktion wie Verschieben nach … Die Vorschau nennt die betroffenen Dateien. Regeln wirken nur auf Wunsch auf den gewählten Ordner, nicht automatisch, und werden nicht gespeichert.\n"
	  "## Duplikate finden\n"
	  "Sucht in einem Ordner samt Unterordnern nach gleichem Namen, gleicher Größe oder gleichem Inhalt (Prüfsumme). „Alle außer der ersten markieren“ und „Markierte in den Papierkorb“ räumen auf; gelöscht wird nur in den Papierkorb.\n"
	  "## Versionen\n"
	  "Bearbeiten ▸ Werkzeuge ▸ Versionen ▸ „Version speichern“ sichert den Stand einer Datei (unter ~/.local/share/nolphin/versions). „Versionen anzeigen …“ listet sie; wiederherstellen, löschen oder bei Textdateien mit der aktuellen Datei vergleichen.\n"
	  "## Massenumbenennung\n"
	  "Mehrere Dateien markieren, Bearbeiten ▸ „Massenumbenennung …“: Suchen und Ersetzen, Nummerierung (vor oder nach dem Namen), Groß-/Kleinschreibung. Die Spalte „Neuer Name“ zeigt das Ergebnis vorab." },

	{ "fenster", N_("Fenster, Reiter, Arbeitsbereiche"),
	  "# Fenster, Reiter, Arbeitsbereiche\n"
	  "## Reiter\n"
	  "Strg+T öffnet einen neuen Reiter, Gehe zu ▸ Reiter listet alle Reiter des Fensters.\n"
	  "## Geteilte Ansicht\n"
	  "Ansicht ▸ Geteilte Ansicht (F3) fügt einen Bereich hinzu; bis zu vier Bereiche sind möglich. Sie können waagerecht teilen (Umschalt+F3), Bereiche duplizieren, maximieren und schließen. Jeder Bereich hat seinen eigenen Ordner.\n"
	  "## Arbeitsbereiche\n"
	  "Datei ▸ Arbeitsbereiche ▸ „Speichern …“ merkt Reiter, Teilung, Fenstergröße und Panels unter einem Namen. Im Panel laden, duplizieren oder löschen Sie sie. Beim Beenden merkt sich Nolphin außerdem die letzte Sitzung.\n"
	  "## Gehe zu\n"
	  "Das Menü bietet Verlauf, Häufig verwendet und die Reiter. Bearbeiten ▸ „Zwischenablage als Datei einfügen“ legt den Inhalt der Zwischenablage als Datei ab." },

	{ "deb", N_("DEB-Pakete erstellen"),
	  "# DEB-Pakete erstellen\n"
	  "Hilfe ▸ „.deb-Paket erstellen …“ öffnet das Panel. Pflichtfelder sind Name, Version, Beschreibung und Ersteller; Sektion, Priorität, Abhängigkeiten und Homepage stehen unter „Weitere Angaben“. Fügen Sie Dateien und Ordner mit ihrem Zielpfad hinzu und wählen Sie „DEB erstellen“. Nolphin schreibt das Paket selbst, ohne dpkg-deb." },

	{ "terminal", N_("Terminal"),
	  "# Terminal\n"
	  "F4 (oder Ansicht ▸ Terminal) öffnet ein Terminal im Arbeitsbereich, im aktuellen Ordner. Es bleibt neben der Dateiansicht stehen. Rechtsklick ▸ „Im Terminal öffnen“ startet es für einen bestimmten Ordner." },

	{ "sicherheit", N_("Prüfsummen, Verschlüsselung, Rechte"),
	  "# Prüfsummen, Verschlüsselung, Rechte\n"
	  "- Prüfsummen: Rechtsklick ▸ „Prüfsumme berechnen …“ (MD5, SHA-1, SHA-256, SHA-512, BLAKE2).\n"
	  "- Verschlüsseln: Rechtsklick ▸ „Verschlüsseln …“ mit GPG.\n"
	  "- Eigenschaften (Alt+Eingabe): Zugriffsrechte, Besitzer, Gruppe und zusätzliche Benutzer und Gruppen (ACL), bei Ordnern auch „auf Inhalt anwenden“.\n"
	  "- Papierkorb: Der Papierkorb wird nach einer einstellbaren Dauer bereinigt; bei Überschreiten eines Größenlimits erscheint eine Warnung." },

	{ "einstellungen", N_("Einstellungen"),
	  "# Einstellungen\n"
	  "Bearbeiten ▸ Einstellungen öffnet die Einstellungen im Hauptbereich, ebenfalls mit Esc zu schließen. Die Seiten heißen Ansichten, Verhalten, Anzeige, Listenspalten, Vorschau, Werkzeugleiste, Kontextmenü, Dokumentvorlagen und Module.\n"
	  "## Sichern und zurücksetzen\n"
	  "Unten stehen „Exportieren …“ (alle Einstellungen in eine Datei), „Importieren …“ und „Zurücksetzen …“.\n"
	  "## Module und Aktionen\n"
	  "Auf der Seite „Module“ schalten Sie Aktionen und Erweiterungen ein und aus; „Anordnung bearbeiten“ öffnet den Layout-Editor für Reihenfolge und Aussehen der Aktionen in den Menüs." },

	{ "diagnose", N_("Diagnose und Fehlerbericht"),
	  "# Diagnose und Fehlerbericht\n"
	  "Hilfe ▸ Diagnose öffnet ein Panel mit vier Reitern:\n"
	  "- Protokolle: die letzten Zeilen des lokalen Protokolls (~/.local/share/nolphin/logs/nolphin.log).\n"
	  "- Systeminformationen: Programm- und Systemversion, verfügbare GVfs-Dienste und gefundene Hilfswerkzeuge.\n"
	  "- Plugin-Status: welche Erweiterungen geladen wurden.\n"
	  "- Fehlerbericht: erzeugt eine lokale Datei zum Weitergeben. Nolphin sendet nichts automatisch.\n"
	  "Beim Melden eines Problems helfen Systeminformationen und die Schritte zur Reproduktion." },

	{ "tasten", N_("Tastenkürzel"),
	  "# Tastenkürzel\n"
	  "- F1: diese Hilfe; Strg+F1: Tastenkombinationen\n"
	  "- F11: Arbeitsbereich ein/aus; F4: Terminal; F3: geteilte Ansicht\n"
	  "- Strg+F: Suche; Strg+L: Adresse eingeben; Strg+H: versteckte Dateien\n"
	  "- Strg+T: neuer Reiter; Strg+N: neues Fenster; Umschalt+Strg+N: neuer Ordner\n"
	  "- Alt+Eingabe: Eigenschaften; F2: umbenennen; Strg+A: alles auswählen; Strg+S: nach Muster auswählen\n"
	  "- Strg+1 bis Strg+4: Symbol-, Listen-, Kompakt- und Galerieansicht\n"
	  "- Alt+Hoch, Alt+Links, Alt+Rechts, Alt+Pos1: übergeordneter Ordner, zurück, vorwärts, persönlicher Ordner\n"
	  "Die vollständige Liste zeigt Hilfe ▸ Tastenkombinationen." },
};

#define N_TOPICS G_N_ELEMENTS (topics)

static GtkWidget *help_page = NULL;
static GtkWidget *help_host = NULL;
static gulong     help_key_handler = 0;

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
render_topic (GtkTextBuffer *buffer, const gchar *text)
{
	gchar **lines = g_strsplit (text, "\n", -1);
	GtkTextIter iter;
	gint i;

	gtk_text_buffer_set_text (buffer, "", 0);
	gtk_text_buffer_get_end_iter (buffer, &iter);

	for (i = 0; lines[i] != NULL; i++) {
		const gchar *line = lines[i];
		const gchar *tag = NULL;

		if (line[0] == '\0') {
			continue;
		}
		if (g_str_has_prefix (line, "## ")) {
			tag = "h2";
			line += 3;
		} else if (g_str_has_prefix (line, "# ")) {
			tag = "h1";
			line += 2;
		} else if (g_str_has_prefix (line, "- ")) {
			tag = "bullet";
		}

		if (tag != NULL) {
			if (g_strcmp0 (tag, "bullet") == 0) {
				gchar *bulleted = g_strdup_printf ("•  %s\n", line + 2);

				gtk_text_buffer_insert_with_tags_by_name (buffer, &iter, bulleted, -1, tag, NULL);
				g_free (bulleted);
			} else {
				gchar *with_nl = g_strdup_printf ("%s\n", line);

				gtk_text_buffer_insert_with_tags_by_name (buffer, &iter, with_nl, -1, tag, NULL);
				g_free (with_nl);
			}
		} else {
			gchar *with_nl = g_strdup_printf ("%s\n", line);

			gtk_text_buffer_insert_with_tags_by_name (buffer, &iter, with_nl, -1, "para", NULL);
			g_free (with_nl);
		}
	}
	g_strfreev (lines);
}

static void
on_topic_selected (GtkListBox *box, GtkListBoxRow *row, gpointer user_data)
{
	GtkTextBuffer *buffer = GTK_TEXT_BUFFER (user_data);
	GtkWidget *view;
	gint index;

	if (row == NULL) {
		return;
	}
	index = gtk_list_box_row_get_index (row);
	if (index < 0 || (guint) index >= N_TOPICS) {
		return;
	}
	render_topic (buffer, topics[index].text);

	view = g_object_get_data (G_OBJECT (buffer), "nolphin-help-view");
	if (view != NULL) {
		GtkWidget *scrolled = gtk_widget_get_parent (view);

		if (GTK_IS_SCROLLED_WINDOW (scrolled)) {
			gtk_adjustment_set_value (gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (scrolled)), 0);
		}
	}
}

static GtkWidget *
build_page (const gchar *topic_id)
{
	GtkWidget *page, *paned, *list, *list_scroll, *text_scroll, *view, *bar, *close_button, *title;
	GtkTextBuffer *buffer;
	GtkListBoxRow *initial = NULL;
	guint i;

	page = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

	title = gtk_label_new (NULL);
	gtk_label_set_markup (GTK_LABEL (title), _("<b>Hilfe zu Nolphin</b>"));
	gtk_widget_set_halign (title, GTK_ALIGN_START);
	gtk_widget_set_margin_start (title, 24);
	gtk_widget_set_margin_top (title, 12);
	gtk_widget_set_margin_bottom (title, 8);
	gtk_box_pack_start (GTK_BOX (page), title, FALSE, FALSE, 0);

	paned = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
	gtk_widget_set_vexpand (paned, TRUE);
	gtk_box_pack_start (GTK_BOX (page), paned, TRUE, TRUE, 0);

	list = gtk_list_box_new ();
	gtk_list_box_set_selection_mode (GTK_LIST_BOX (list), GTK_SELECTION_SINGLE);
	list_scroll = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (list_scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_widget_set_size_request (list_scroll, 240, -1);
	gtk_container_add (GTK_CONTAINER (list_scroll), list);
	gtk_paned_pack1 (GTK_PANED (paned), list_scroll, FALSE, FALSE);

	view = gtk_text_view_new ();
	gtk_text_view_set_editable (GTK_TEXT_VIEW (view), FALSE);
	gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (view), FALSE);
	gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (view), GTK_WRAP_WORD_CHAR);
	gtk_text_view_set_left_margin (GTK_TEXT_VIEW (view), 24);
	gtk_text_view_set_right_margin (GTK_TEXT_VIEW (view), 24);
	gtk_text_view_set_top_margin (GTK_TEXT_VIEW (view), 12);
	gtk_text_view_set_bottom_margin (GTK_TEXT_VIEW (view), 24);
	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
	g_object_set_data (G_OBJECT (buffer), "nolphin-help-view", view);

	gtk_text_buffer_create_tag (buffer, "h1", "scale", 1.5, "weight", 700,
				    "pixels-below-lines", 8, "pixels-above-lines", 4, NULL);
	gtk_text_buffer_create_tag (buffer, "h2", "scale", 1.2, "weight", 700,
				    "pixels-above-lines", 14, "pixels-below-lines", 4, NULL);
	gtk_text_buffer_create_tag (buffer, "para", "scale", 1.1, "pixels-below-lines", 8, NULL);
	gtk_text_buffer_create_tag (buffer, "bullet", "scale", 1.1, "left-margin", 40, "indent", -16,
				    "pixels-below-lines", 4, NULL);

	text_scroll = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (text_scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (text_scroll), view);
	gtk_paned_pack2 (GTK_PANED (paned), text_scroll, TRUE, FALSE);

	for (i = 0; i < N_TOPICS; i++) {
		GtkWidget *label = gtk_label_new (_(topics[i].title));
		GtkWidget *row = gtk_list_box_row_new ();

		gtk_widget_set_halign (label, GTK_ALIGN_START);
		gtk_widget_set_margin_start (label, 12);
		gtk_widget_set_margin_end (label, 12);
		gtk_widget_set_margin_top (label, 6);
		gtk_widget_set_margin_bottom (label, 6);
		gtk_container_add (GTK_CONTAINER (row), label);
		gtk_list_box_insert (GTK_LIST_BOX (list), row, -1);

		if (topic_id != NULL && g_strcmp0 (topic_id, topics[i].id) == 0) {
			initial = GTK_LIST_BOX_ROW (row);
		}
	}
	g_signal_connect (list, "row-selected", G_CALLBACK (on_topic_selected), buffer);

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

	if (initial == NULL) {
		initial = gtk_list_box_get_row_at_index (GTK_LIST_BOX (list), 0);
	}
	gtk_list_box_select_row (GTK_LIST_BOX (list), initial);
	g_object_set_data (G_OBJECT (page), "nolphin-help-list", list);
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
			GtkWidget *list = g_object_get_data (G_OBJECT (help_page), "nolphin-help-list");
			guint i;

			for (i = 0; list != NULL && i < N_TOPICS; i++) {
				if (g_strcmp0 (topic_id, topics[i].id) == 0) {
					gtk_list_box_select_row (GTK_LIST_BOX (list),
								 gtk_list_box_get_row_at_index (GTK_LIST_BOX (list), i));
				}
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
