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

## Übersetzung
- REQUIREMENT: Deutsche Oberfläche.
- DATEI: `po/de.po`, `po/nolphin.pot`, `po/POTFILES.in`, `action-layout-editor/nolphin_action_layout_editor.py`.
- ÄNDERUNG: Vorlage neu erzeugt (war veraltet), `de.po` abgeglichen, Layout-Editor in `POTFILES.in`, `NOLPHIN_LOCALEDIR` auch dort beachtet.
- WARUM: Die Einstellungsseiten erschienen englisch, weil die Glade-Texte im Katalog nur als veraltet geführt wurden.
- TEST: `msgfmt -c`, Build; die Übersetzung im Programm danach nicht erneut angesehen.

## Entfernt
- `src/nolphin-properties-window.{c,h}` (Eigenschaften laufen im Panel).
