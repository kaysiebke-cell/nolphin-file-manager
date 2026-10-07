# Änderungsprotokoll

Format nach `NOLPHIN_SPEC.md`, Abschnitt 8. Stand: 5. Oktober 2026.
Unter TEST steht nur, was tatsächlich ausgeführt wurde: **Meson** = automatischer Test in `test/`, **GUI** = von Hand in der laufenden Anwendung geprüft.

## Vorschau und Metadaten
- REQUIREMENT: Erweiterte Vorschau (PDF, Video, Audio); Bewertung, Tags, Kommentar.
- DATEI: `libnolphin-private/nolphin-media.{c,h}`, `src/nolphin-preview.c`.
- ÄNDERUNG: Eckdaten über `pdfinfo`, Vorschaubild über `pdftocairo`, Audio/Video über `gst-discoverer-1.0` (GSubprocess, keine Shell). Bewertung, Tags und Kommentar im Info-Panel, gespeichert als GIO-Metadaten. Fehlt ein Werkzeug, erscheint ein Hinweis.
- WARUM: Poppler-GLib- und GStreamer-Entwicklungspakete sind nicht installiert (würde `sudo` brauchen).
- TEST: Meson „Media info test“ (MIME-Typen, PDF-Seitenzahl); GUI: PDF, MP4, Bewertung/Tags/Kommentar.

## Git-Overlays
- REQUIREMENT: Git-Status als Symbol an Dateien.
- DATEI: `libnolphin-private/nolphin-git-overlay.{c,h}`, `libnolphin-private/nolphin-file.c`.
- ÄNDERUNG: Status je Repository asynchron geladen und zwischengespeichert, Emblem je Status.
- WARUM: Git-Zustand im Ordner sichtbar machen, ohne Panel zu öffnen.
- TEST: Meson „Git overlay test“; GUI: Ordner mit hinzugefügter, geänderter, unversionierter Datei.

## Archiv-Manager
- REQUIREMENT: Archive auflisten und gezielt ändern.
- DATEI: `libnolphin-private/nolphin-archive-manage.{c,h}`, `src/nolphin-workspace-panel.c`.
- ÄNDERUNG: Auflisten, Hinzufügen, Ersetzen, Entfernen, Extrahieren über die Systemwerkzeuge; Panel „Archiv-Manager“.
- WARUM: Archive sollen im Arbeitsbereich bearbeitbar sein (Abschnitt 58.1, Panel statt Dialog).
- TEST: Meson „Archive manager test“ (ZIP: auflisten, hinzufügen, entfernen, extrahieren); GUI: Auflisten und Panel bei ZIP.
  Nicht in der Oberfläche getestet: Hinzufügen/Entfernen über die Knöpfe.

## Werkzeuge: Synchronisation, Regeln, Duplikate, Versionen
- REQUIREMENT: Ordner abgleichen, Regeln anwenden, Duplikate finden, Versionen.
- DATEI: `src/nolphin-sync.c`, `nolphin-rules.c`, `nolphin-duplicates.c`, `nolphin-versions.c`, `nolphin-tools.{c,h}`, `src/nolphin-workspace-panel.c`.
- ÄNDERUNG: Vier Panels mit Vorschau vor jeder Änderung; Sync über `rsync` nur einseitig und ohne Löschen im Ziel; Duplikate nur in den Papierkorb; Versionen unter `~/.local/share/nolphin/versions`. Regeln werden nicht dauerhaft gespeichert.
- WARUM: Vertragsfunktionen aus Phase 3.
- TEST: GUI: Vergleich mit Neu/Geändert/Konflikt, Regelvorschau, Duplikatgruppe, zwei Versionen mit Unterschieden. Keine Meson-Tests.

## Fenster, Arbeitsbereiche, Gehe zu
- REQUIREMENT: Geteilte Ansicht bis vier Bereiche, Arbeitsbereiche, Verlauf/Häufig/Reiter.
- DATEI: `src/nolphin-window.c`, `nolphin-window-pane.c`, `nolphin-window-slot.{c,h}`, `nolphin-notebook.{c,h}`, `nolphin-window-menus.c`, `nolphin-window-bookmarks.c`.
- ÄNDERUNG: Layoutbaum für die Teilung, Speichern/Laden/Duplizieren/Löschen von Arbeitsbereichen (`~/.config/nolphin/workspaces`), letzte Sitzung.
- WARUM: Vertragsfunktionen aus Phase 2/3.
- TEST: GUI: Vier Bereiche, Arbeitsbereich speichern. Layout-Speichern beim Schließen mit drei oder vier Bereichen nicht geprüft.

## Diagnose und Einstellungen
- REQUIREMENT: Diagnose mit Fehlerbericht; Einstellungen exportieren, importieren, zurücksetzen.
- DATEI: `src/nolphin-diagnostics.{c,h}`, `src/nolphin-file-management-properties.c`, `src/nolphin-main.c`.
- ÄNDERUNG: Panel mit Protokoll, Systeminformationen, Plugin-Status, Fehlerbericht; drei Knöpfe auf den Einstellungsseiten.
- TEST: GUI: Systeminformationen. Export/Import/Zurücksetzen nur als dconf-Ablauf geprüft, nicht über die Knöpfe.

## Suche mit Operatoren
- REQUIREMENT: Erweiterte Suche, Abschnitt 33 (UND, ODER, NICHT, exakte Übereinstimmung, Platzhalter).
- DATEI: `libnolphin-private/nolphin-search-expression.{c,h}`, `libnolphin-private/nolphin-search-engine-advanced.c`.
- ÄNDERUNG: Neues Modul parst den Suchtext in ODER-Gruppen aus UND-Begriffen mit optionalem NICHT; die Engine nutzt es statt der festen Liste von Mustern. Ohne Operatoren bleibt das alte Verhalten (Leerzeichen = UND, Begriff = „enthält“).
- WARUM: Die Operatoren fehlten; reguläre Ausdrücke und Platzhalter gab es schon.
- TEST: Meson „Search expression test“ (Ausdrücke) und „Search Engine test“ (Suche in Temp-Ordner); GUI: „notizen ODER tabelle NICHT kopie“.
  Nicht umgesetzt: Suche in Tags und Kommentaren, Kriterien Besitzer/Gruppe/Berechtigungen.

## Tests statt Demo-Programme
- REQUIREMENT: Nur Getestetes gilt als implementiert (Abschnitt „Implementiert“).
- DATEI: `test/test-copy.c`, `test/test-nolphin-search-engine.c`, `test/test-nolphin-directory-async.c`, `test/meson.build`.
- ÄNDERUNG: Die drei Demo-Programme aus dem Nemo-Ursprung (Argumente nötig bzw. Endlosschleife) prüfen jetzt Ergebnisse in einem Temp-Ordner; `test-copy` behält mit Argumenten den manuellen Modus.
- TEST: `meson test -C build`: 20 von 20 bestanden.

## Entscheidung zu Regeln, SVN und Mercurial
- Regeln werden laut Vertrag nur manuell auf einen gewählten Ordner angewendet; automatisches Auslösen ist nicht vorgesehen und wird nicht gebaut.
- SVN/Mercurial-Overlays stehen nicht im Vertrag, die Programme sind nicht installiert; nicht gebaut.

## Eingebaute Hilfe
- REQUIREMENT: Hilfe zu den Funktionen von Nolphin, geöffnet wie die Einstellungen (im Hauptbereich statt in einem Fenster).
- DATEI: `src/nolphin-help.{c,h}`, `src/nolphin-window-menus.c`, `docs/hilfe/*.md`, `docs/meson.build`, `src/meson.build`, `po/POTFILES.in`.
- ÄNDERUNG: Neue Seite „help“ im Inhalts-Stack des Hauptfensters: Themenliste mit Suchfeld links, rechts das Thema als Markdown (Überschriften, Tabellen, Bilder, Links) in der Markdown-Ansicht der GID-Projekte. 20 Themen unter `docs/hilfe/` mit den Bildern aus `docs/bilder/`; installiert nach `<datadir>/nolphin/help/`. Das Verzeichnis wird über `NOLPHIN_HELPDIR`, die Installation oder `docs/` neben dem Build-Ordner gefunden. Bilder sind auf 720 Pixel Breite begrenzt (`nolphin_markdown_view_set_max_image_width`, neue Option der Markdown-Ansicht; Standard unverändert). Links `hilfe:<thema>` springen zwischen Themen. F1 und Hilfe ▸ Alle Themen öffnen die Seite; Esc oder „Schließen“ führt zurück.
- WARUM: Bisher öffnete F1 die GNOME-Hilfe, die Nolphin-Funktionen nicht kennt.
- TEST: GUI: F1, Themenwechsel, Link in der Themenübersicht, Themensuche („rsync“), Esc. Die Texte habe ich gegen den Code geprüft (Menüeinträge, Auswahllisten, Verhalten); nicht jede Aussage wurde in der Oberfläche ausprobiert.

## Entwicklungsstand erkennbar machen
- REQUIREMENT: Installierte Version und Build-Ordner-Stand sollen sich unterscheiden lassen und nebeneinander laufen.
- DATEI: `src/nolphin-diagnostics.{c,h}`, `src/nolphin-main-application.c`, `src/nolphin-window.c`, `src/nolphin-window-menus.c`, `docs/hilfe/20-grenzen.md`.
- ÄNDERUNG: `nolphin_is_development_build()` prüft, ob das Programm nicht unter `/usr/` liegt. Dann gilt die Anwendungs-ID `org.Nolphin.Dev`, der Fenstertitel trägt „[Entwicklung]“, Über-Dialog und Systeminformationen nennen den Entwicklungsstand und den Startpfad.
- WARUM: Beide Stände teilten sich den D-Bus-Namen; ein Start aus dem Build-Ordner konnte in der installierten Instanz landen und war nicht zu unterscheiden.
- TEST: GUI: Build-Ordner-Stand zeigt „[Entwicklung]“ und meldet `org.Nolphin.Dev` am Session-Bus. Das installierte Paket enthält diese Änderung erst nach einem Neubau.

## Installation als Paket
- Das Paket kollidierte mit `nemo-data` bei zehn Menü-Symbolen (`menu-*.png`); sie werden nicht mehr ins System-Icon-Verzeichnis installiert (sie stecken in der Programm-Ressource).
- Eine ältere Installation unter `/usr/local` (Schema, Programm) überdeckte das Paket und musste entfernt werden.

## Vorschau in der Dateiauswahl
- REQUIREMENT: Auswahldialoge zeigen rechts die Vorschau der markierten Datei, auch beim Hochladen in anderen Programmen.
- DATEI: `libnolphin-private/nolphin-file-chooser-preview.{c,h}`, `gtk-module/`, `src/nolphin-workspace-panel.c`, `src/nolphin-file-management-properties.c`, `src/nolphin-template-config-widget.c`, `src/nolphin-properties-panel.c`, `debian/nolphin.install`.
- ÄNDERUNG: Gemeinsamer Baustein hängt an einen GtkFileChooser ein Vorschau-Widget (Bild, Vorschaubild, Textanfang oder Symbol; Name, Typ, Größe, Datum). Er ist in Nolphins Dialoge zum Dateiauswählen eingebaut. Zusätzlich lädt das GTK-Modul `libnolphin-preview-module.so` (installiert nach `<libdir>/gtk-3.0/modules`) den Baustein in „Datei öffnen“-Dialoge anderer GTK-3-Programme, sofern dort keine eigene Vorschau gesetzt ist. Aktiviert wird es pro Benutzer über die Umgebungsvariable `GTK3_MODULES` (`~/.xsessionrc`, wird in der X11-Sitzung vor dem Mint-Skript `80xapp-gtk3-module` gelesen). Die GTK-Einstellung `gtk-modules` (`settings.ini`) und `enabled-gtk-modules` von Cinnamon haben auf dem Testsystem nicht gewirkt.
- WARUM: Der Hochladen-Dialog gehört dem aufrufenden Programm; ein GTK-Modul ist der Weg, ihn von außen zu ergänzen, ohne einen eigenen Portal-Dienst zu bauen.
- TEST: In einem getrennten Display (Xephyr): Dialog mit Modul über `GTK_MODULES`, `settings.ini` und `GTK3_MODULES` (ohne Hilfsvariablen für den Modulpfad), Vorschau von Bild und Textdatei. Fehler beim ersten Wurf: In C-Programmen lädt GTK das Modul in `gtk_init()` vor der ersten Widget-Klasse, `g_signal_lookup("map")` lieferte 0 und der Haken fehlte (mein Python-Test hatte das verdeckt, `zenity` hat es gezeigt); behoben mit `g_type_class_ref (GTK_TYPE_WIDGET)` und erneut mit `zenity` geprüft. Nicht getestet: Chrome selbst (nutzt je nach Einstellung den GTK-Dialog oder den Portal-Dialog) und Nolphins eigene Dialoge in der Oberfläche.

## Übersetzung
- REQUIREMENT: Deutsche Oberfläche.
- DATEI: `po/de.po`, `po/nolphin.pot`, `po/POTFILES.in`, `action-layout-editor/nolphin_action_layout_editor.py`.
- ÄNDERUNG: Vorlage neu erzeugt (war veraltet), `de.po` abgeglichen, Layout-Editor in `POTFILES.in`, `NOLPHIN_LOCALEDIR` auch dort beachtet.
- WARUM: Die Einstellungsseiten erschienen englisch, weil die Glade-Texte im Katalog nur als veraltet geführt wurden.
- TEST: `msgfmt -c`, Build; die Übersetzung im Programm danach nicht erneut angesehen.

## Entfernt
- `src/nolphin-properties-window.{c,h}` (Eigenschaften laufen im Panel).
