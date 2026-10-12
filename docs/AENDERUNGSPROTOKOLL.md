# Änderungsprotokoll

Format: REQUIREMENT, DATEI, ÄNDERUNG, WARUM, TEST. Stand: 5. Oktober 2026.
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
- WARUM: geplante Funktionen aus Phase 3.
- TEST: GUI: Vergleich mit Neu/Geändert/Konflikt, Regelvorschau, Duplikatgruppe, zwei Versionen mit Unterschieden. Keine Meson-Tests.

## Fenster, Arbeitsbereiche, Gehe zu
- REQUIREMENT: Geteilte Ansicht bis vier Bereiche, Arbeitsbereiche, Verlauf/Häufig/Reiter.
- DATEI: `src/nolphin-window.c`, `nolphin-window-pane.c`, `nolphin-window-slot.{c,h}`, `nolphin-notebook.{c,h}`, `nolphin-window-menus.c`, `nolphin-window-bookmarks.c`.
- ÄNDERUNG: Layoutbaum für die Teilung, Speichern/Laden/Duplizieren/Löschen von Arbeitsbereichen (`~/.config/nolphin/workspaces`), letzte Sitzung.
- WARUM: geplante Funktionen aus Phase 2/3.
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
- Regeln werden nur manuell auf einen gewählten Ordner angewendet; automatisches Auslösen ist nicht vorgesehen und wird nicht gebaut.
- SVN/Mercurial-Overlays sind nicht geplant, die Programme sind nicht installiert; nicht gebaut.

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
- TEST: In einem getrennten Display (Xephyr): Dialog mit Modul über `GTK_MODULES`, `settings.ini` und `GTK3_MODULES` (ohne Hilfsvariablen für den Modulpfad), Vorschau von Bild und Textdatei. Fehler beim ersten Wurf: In C-Programmen lädt GTK das Modul in `gtk_init()` vor der ersten Widget-Klasse, `g_signal_lookup("map")` lieferte 0 und der Haken fehlte (mein Python-Test hatte das verdeckt, `zenity` hat es gezeigt); behoben mit `g_type_class_ref (GTK_TYPE_WIDGET)` und erneut mit `zenity` geprüft. Zweiter Fehler: `xdg-desktop-portal-gtk` setzt selbst ein einfaches `GtkImage` als Vorschau; das Modul überging solche Dialoge. Nun wird ein `GtkImage`-Vorschau-Widget ersetzt, das alte bleibt referenziert (ohne das stürzt der Dienst im eigenen Handler ab, `gtk_image_set_from_pixbuf` auf freigegebenem Widget). Im echten Portal-Dialog geprüft (OpenFile über D-Bus): Vorschau für Ordner und Bild erscheint. Nicht getestet: Chrome selbst (nutzt je nach Einstellung den GTK-Dialog oder den Portal-Dialog) und Nolphins eigene Dialoge in der Oberfläche.

## Handy-Austausch
- REQUIREMENT: Austausch mit dem Handy in beide Richtungen über WLAN ohne Fremdabhängigkeit und ohne Einstellungen (Spezifikation 2.6, Abschnitt 61).
- DATEI: `src/nolphin-qr.c/.h`, `src/nolphin-phone-server.c/.h`, `src/nolphin-phone.c/.h`, `src/nolphin-places-sidebar.c`, `src/nolphin-view.c`, `src/nolphin-actions.h`, `gresources/nolphin-directory-view-ui.xml`, `test/test-nolphin-phone-server.c`, Panel, Menü, Meson, `docs/hilfe/21-handy-austausch.md`, `docs/bilder/panel-handy.png`, `docs/bilder/kontextmenue-handy.png`.
- ÄNDERUNG: Eigener QR-Encoder und HTTP-Server (GIO). Eintrag „Handy“ in der Seitenleiste: Klick startet den Austausch und zeigt `~/Handy`; der Eintrag ist zugleich Ablageziel (kopiert nach `~/Handy/Zum Handy`). Kontextmenü „An Handy senden“. Feste Ordner `Vom Handy` (Eingang) und `Zum Handy` (Ausgang); die Ordnerwahl und der Bereich „Einstellungen“ entfallen. Die Netzwerkadresse erscheint nur bei mehreren Netzen. Die Download-Liste im Handy-Browser aktualisiert sich selbst. Zusätzlich Datei ▸ Handy-Austausch ….
- WARUM: Kein KDE Connect und keine Handy-App; der QR-Code ersetzt die Adresseingabe. Auf Rückmeldung des Benutzers: Einstieg in der Seitenleiste, Dateien in beide Richtungen ohne Umwege, die frei wählbaren Ordner waren unnötig und zeigten keine Wirkung.
- TEST: Meson-Test `Phone exchange test`, QR-Abgleich mit `qrcode`, isolierte Instanz: Klick auf „Handy“ (Ordner `~/Handy`), „An Handy senden“ (Datei kopiert, Original bleibt, Handy-Seite listet und lädt sie, Inhalt identisch), Ziehen auf „Handy“ (kopiert, Original bleibt). Echter Upload von einem Handy (Bild kam an). Nicht getestet: Download und Senden am echten Handy, Kamera-Scan mit anderen Geräten, mehrere Netzwerkschnittstellen, dunkles Theme.

## Handy-App (Android)
- REQUIREMENT: Ein Empfänger auf dem Handy, damit der PC Dateien aufs Handy schicken kann (der Browser holt nur nach einem Tipp). Der erste Scan des QR-Codes installiert die App, weitere Scans verbinden sie (Spezifikation 2.6, Abschnitt 61.7).
- DATEI: `android/` (AndroidManifest.xml, `src/org/nolphin/handy/*.java`, `build.sh`, README), `src/nolphin-phone-server.c` (Wartepunkt `/wait`, `/apk`, Dateien im Schreiben ausblenden, App-Abschnitt und automatischer App-Link in der Seite), `docs/meson.build`, `test/test-nolphin-phone-server.c`, Hilfe und Doku.
- ÄNDERUNG: App nur mit dem Android-Framework (Java, kein Gradle, keine Bibliothek), gebaut mit `aapt2`, `d8`, `apksigner`. Hintergrunddienst wartet am Server, lädt neue Dateien nach `Download/Nolphin`; Teilen-Menü und Dateiauswahl senden an den PC. Die Seite hinter dem QR-Code öffnet die App über einen `intent://`-Link und fällt auf den APK-Download zurück.
- WARUM: Auf Wunsch des Benutzers; ohne Empfänger auf dem Handy ist „PC → Handy“ nur eine Einbahnstraße.
- TEST: Meson-Test (Wartepunkt, `/apk`, frische Dateien); Android-Emulator (Android 14): Installation, Verbinden per `nolphin://`-Link, Datei am PC ablegen kommt von selbst an (Inhalt identisch), Alt-Bestand wird nicht geladen, Senden über das Teilen-Menü kommt am PC an (identisch), Intent-Link der Seite öffnet die App. Nicht geprüft: Chrome auf einem echten Handy (Installation der APK, automatisches Öffnen), Verhalten im Standby, Android-Versionen außer 14.

## Handy-App: QR-Scanner in der App
- REQUIREMENT: Die App muss sich selbst mit dem PC verbinden können. Nach dem Installieren sagte sie nur „Scanne den QR-Code“, hatte aber keine Scan-Funktion (Rückmeldung des Benutzers mit Screenshots vom Handy).
- DATEI: `android/src/org/nolphin/handy/QrDecoder.java` (neu), `ScanActivity.java` (neu), `MainActivity.java`, `AndroidManifest.xml` (Kamera), `android/test.sh`, `android/test/` (neu), Hilfe und Doku.
- ÄNDERUNG: Eigener QR-Leser in reinem Java (Suchmuster, Perspektive über Ausrichtungsmuster, Formatinfo, Reed-Solomon mit Fehlerkorrektur), Scanner-Bildschirm mit der Kamera des Handys (Camera-API des Frameworks, keine Bibliothek, keine Google-Dienste) mit Anzeige „Code erkannt“. Startseite der App: „QR-Code scannen“ zuerst, dazu „Link aus der Zwischenablage einfügen“ und ein Feld zum Eintippen.
- WARUM: Ohne Scanner kam die App nach der Installation nicht mit dem PC in Verbindung. Eine fertige Scan-Bibliothek kam wegen der Vorgabe „keine Fremdabhängigkeit“ nicht in Frage.
- NACHBESSERUNG nach Screenshots vom Handy des Benutzers: (1) Die App zeigte widersprüchliche Angaben (Status doppelt, „Verbindung trennen“ und „Link einfügen“ gleichzeitig, auch ohne Verbindung): Die Startseite zeigt jetzt nur, was im jeweiligen Zustand Sinn ergibt. (2) „Senden fehlgeschlagen“ bei Dateien aus der Galerie von MIUI: Die App brach ab, wenn der Anbieter die Größe nicht nannte oder falsch nannte. Sie speichert die Datei jetzt immer erst zwischen und sendet mit der echten Länge; Fehler nennen ihren Grund. (3) Der Scanner zeigt eine Diagnosezeile (Kameragröße, gelesene Bilder, Suchmuster, Zeit), nutzt die tatsächlich gelieferte Vorschaugröße und fordert das Format NV21 ausdrücklich an.
- TEST: `android/test.sh`: 154 Prüfungen auf dem PC (Codes vom C-Encoder, gedreht, verzerrt, unscharf, verrauscht; Rauschen und leeres Bild liefern nichts; Screenshot aus der Doku; ein echtes dunkles, unscharfes Handyfoto des Codes, das der Benutzer geschickt hat, wird korrekt gelesen, auch im ganzen Bild 1080×2400 in 0,4 s). Emulator: Startseite, Scanner-Bildschirm mit laufender Kamera und richtigem Seitenverhältnis. Nicht geprüft: Lesen mit der Live-Kamera eines echten Handys (im Emulator ohne Fenster lässt sich die virtuelle Kamera nicht auf das Plakat richten), Lesegeschwindigkeit auf dem Handy, Warnungen anderer Hersteller bei der Installation.

## Handy-Austausch: Schnittstelle Version 1 (Analyse nach dem Entwicklungsplan)
- REQUIREMENT: Entwicklungsplan „Nolphin + Android-App“: Ist-Zustand dokumentieren, Referenzprojekte vergleichen, Schnittstelle und QR-Format festlegen, Token begrenzen und widerrufen, Verbindung validieren, Fortschritt und Abbruch, Fehlerfälle testen.
- DATEI: `docs/handy-austausch-analyse.md`, `docs/handy-austausch-api.md` (neu); `src/nolphin-phone-server.c/.h` (`/info`, Ablauf, Widerruf), `src/nolphin-phone.c` (Linkformat Version 1, Knopf „Neuen QR-Code erzeugen“), `test/test-nolphin-phone-server.c`; `android/src/org/nolphin/handy/Link.java`, `PcCheck.java`, `Connector.java` (neu), `Conn.java`, `ScanActivity.java`, `MainActivity.java`, `SyncService.java`; `android/test/org/nolphin/handy/LinkTest.java` (neu).
- ÄNDERUNG: QR-Link `…/?t=<Schlüssel>&v=1&e=<Ablauf>`; Schlüssel gilt höchstens 12 Stunden und lässt sich widerrufen; `GET /info`; die App prüft den Link streng (nur private IPv4, Port, Schlüsselform, Version, Ablauf) und fragt `/info` am PC ab, bevor sie die Verbindung speichert (mit Rückfall auf `/ls` für ältere Nolphin-Versionen); Fortschritt und Abbruch beim Laden und Senden; Rückfrage vor dem Laden als Option; neuer Schlüssel beendet sofort die alte Wartezeit der App.
- WARUM: Anforderungen aus dem Plan (Token zeitlich begrenzen und widerrufen, Eingaben in Android validieren, Fortschritt und Abbruch, Fehlerfälle). Das LocalSend-Protokoll wurde bewusst nicht übernommen (HTTPS mit Zertifikat, Multicast, Flutter-Modelle; keine Fremdabhängigkeit).
- TEST: `meson test` (26 Tests), `android/test.sh` (154 QR-Prüfungen, 34 Linkprüfungen), Emulator Android 14 gegen den echten Server (siehe `docs/handy-austausch-analyse.md`, Abschnitt „Tests“). Nicht geprüft: Live-Kamera und Linux → Android am echten Handy, Aktion „Laden“ in der Benachrichtigung.

## Handy-Austausch: Menüleiste und Einstellungen (nach Prüfung gegen den Vertrag)
- REQUIREMENT: Abschnitt 11.1 (jede bedienbare Funktion in der Menüleiste, nicht mögliche Einträge ausgegraut) und Abschnitt 31 (keine fest codierte Konfiguration).
- DATEI: `gresources/nolphin-shell-ui.xml`, `gresources/nolphin-directory-view-ui.xml`, `src/nolphin-window-menus.c/.h`, `src/nolphin-view.c`, `src/nolphin-phone.c/.h`, `src/nolphin-phone-server.c/.h`, `libnolphin-private/org.nolphin.gschema.xml`, `libnolphin-private/nolphin-global-preferences.h`, `test/test-nolphin-phone-server.c`, `docs/hilfe/21-handy-austausch.md`.
- ÄNDERUNG: Untermenü Datei ▸ Handy mit „Austausch öffnen …“, „An Handy senden“, „Neuen QR-Code erzeugen“, „Austausch beenden“ (dieselben GActions wie Kontextmenü und Panel; die letzten beiden ausgegraut, solange der Austausch nicht läuft; „An Handy senden“ ausgegraut ohne Auswahl, nicht mehr im Hintergrund-Kontextmenü). GSettings-Schlüssel `phone-folder`, `phone-token-hours`, `phone-idle-minutes`, `phone-max-upload-mb`; der Server bekommt die Werte über `NolphinPhoneServerOptions`.
- WARUM: Verstöße gegen die Abschnitte 11.1 und 31.
- TEST: `meson test` (26 von 26, darunter neue Prüfungen für eigene Gültigkeit und Dateigrenze). Echte Anwendung (Xephyr, eigener D-Bus): Menü zeigt alle vier Einträge; ohne laufenden Austausch sind die letzten drei ausgegraut; Öffnen, Neuer QR-Code (Statusmeldung) und Beenden (Knopf wieder „Starten“, Einträge wieder ausgegraut) über das Menü; „An Handy senden“ bei gewähltem Objekt aktiv. Nicht geprüft: Änderung der GSettings-Werte in der laufenden Anwendung, kein Einstellungsdialog für die vier Schlüssel (nur gsettings/dconf).

## Freigaben und Signaturpasswort (Entscheidung des Benutzers vom 11. Oktober 2026)
- REQUIREMENT: Abschnitte 3, 50.1 und 53.4.
- DATEI: `android/build.sh`, `android/README.md`, `docs/DEPENDENCIES.md`, Vertragsabschnitte 61.7 und 61.8.
- ÄNDERUNG: Freigegeben: Android-Werkzeuge (JDK, Android SDK, `secret-tool`), Vertragsabschnitte 61.7 und 61.8, Verzeichnis `android/`, Handy-Panel. Das Passwort des Signaturschlüssels wird im Systemschlüsselbund abgelegt (`secret-tool`); die frühere Klartextdatei wird übernommen und mit `shred` gelöscht; Passwörter laufen über Umgebungsvariablen, nie über die Kommandozeile.
- WARUM: Abschnitt 53.4 (keine Passwörter im Klartext) und 50.1 (Werkzeuge nur nach Freigabe).
- TEST: `libsecret-tools` vom Benutzer installiert. `android/build.sh` übernahm das Passwort in den Schlüsselbund, löschte die Klartextdatei (`handy.keystore.pass` existiert nicht mehr), baute und signierte die APK; `secret-tool lookup` liefert das Passwort. Schlüssel unverändert, die neue APK lässt sich über die alte installieren.

## Handy-App: Scanner mit Ausweichwegen
- REQUIREMENT: Der Live-Scanner wurde am echten Handy als „funktioniert nicht“ gemeldet; die App darf nicht an einem einzigen Weg hängen (Vertragsabschnitt 61.7, freigegeben).
- DATEI: `android/src/org/nolphin/handy/QrDecoder.java`, `ScanActivity.java`, `android/test/org/nolphin/handy/QrDecoderTest.java`, `docs/hilfe/21-handy-austausch.md`.
- ÄNDERUNG: mehrstufiger Leser (`decodeMultiScale`: Originalgröße, halbe, viertel Größe) für große Fotos; Live-Vorschau jetzt nahe 1920×1080; Knöpfe „Foto aufnehmen“ (Kamera-App, Foto wird nach dem Lesen gelöscht) und „Bild wählen“ im Scanner.
- WARUM: Die Ursache des Live-Problems ist ohne Gerät nicht zu finden; mit einem scharfen Foto der Kamera-App entfällt die Abhängigkeit von der Live-Vorschau.
- TEST: `android/test.sh`: 163 von 163 (neu: Fotos bis 4000×3000 Punkte, gelesen in 146 bis 440 ms). Emulator: „Bild wählen“ mit einem gedrehten, unscharfen Foto des Codes von einem laufenden Server: Code gelesen, Verbindung geprüft, „Mit dem PC verbunden“. Nicht geprüft: „Foto aufnehmen“ und die Live-Vorschau am echten Handy.

## Handy-App: Fokus und schlanke Oberfläche (nach Rückmeldung mit Foto vom echten Handy)
- REQUIREMENT: Der Live-Scanner las am echten Handy nicht; die Oberfläche war überladen (Rückmeldung des Benutzers).
- DATEI: `android/src/org/nolphin/handy/ScanActivity.java`, `MainActivity.java`, `QrDecoder.java`, `docs/hilfe/21-handy-austausch.md`, `docs/handy-austausch-api.md`, `android/README.md`.
- ÄNDERUNG: Ursache laut Diagnosezeile des Benutzers (Kamerabild 1920×1080, 214 Bilder gelesen, 138 Suchmuster-Kandidaten): Das Bild war **unscharf**, der Autofokus griff nicht. Jetzt: Barcode-Szene (wenn vorhanden), fortlaufender Fokus mit Rückfall auf Autofokus, Fokusbereich in der Bildmitte, `cancelAutoFocus ()` nach dem Start, Autofokus alle 2,5 s und beim Antippen des Bildes; der Hinweis „QR-Code im Bild“ erscheint nur noch bei einem echten Dreieck aus drei Suchmustern. Entfernt: alle Knöpfe im Scanner, „Foto aufnehmen“, „Bild wählen“, „Link aus der Zwischenablage einfügen“, Linkfeld, „Mit Link verbinden“, „Alle Dateien vom PC laden“, das Häkchen „Vor dem Laden fragen“, „Neu verbinden“. Startseite getrennt: ein Knopf; verbunden: „Dateien an den PC senden“ und „Verbindung trennen“ (dazu „Übertragung abbrechen“ während einer Übertragung).
- WARUM: Der Benutzer will mit einem Handgriff verbinden; die zusätzlichen Knöpfe waren nicht verlangt.
- TEST: `android/test.sh` 163 von 163; Emulator: Startseite (ein Knopf), Scanner (keine Knöpfe, Hinweis oben), verbunden (Status und zwei Knöpfe), kein Absturz. **Nicht geprüft: der Fokus am echten Handy** (im Emulator nicht nachstellbar).

## Handy-App 2.0: eigene Oberfläche mit fünf Funktionen
- REQUIREMENT: Richtige App statt Browser, Dateien in beide Richtungen (Benutzer, 11. Oktober 2026; Variante „ohne Bibliotheken, eigenes Design“ freigegeben).
- DATEI: `android/src/org/nolphin/handy/` (`MainActivity`, `Host`, `Screen`, `HomeScreen`, `FromPcScreen`, `ToPcScreen`, `HistoryScreen`, `SettingsScreen`, `Ui`, `Thumbs`, `Store`, `RemoteFile`, `SyncService`, `Conn`), `android/res/`, `AndroidManifest.xml`, `android/build.sh`, `android/README.md`.
- ÄNDERUNG: Start, Vom PC (mit Vorschau), Zum PC (Fotoraster, Fortschritt), Verlauf, Einstellungen; Symbol, hell/dunkel; neue Berechtigung Fotos (`READ_MEDIA_IMAGES`); Version 2.0.
- WARUM: Wunsch des Benutzers nach einer richtigen App.
- TEST: `android/test.sh` 163 von 163; Emulator (Android 14) gegen den echten Server: Startseite getrennt und verbunden, Liste, Laden einer Datei, Fotoraster, Senden eines Fotos (liegt im Eingangsordner), Verlauf, Einstellungen, Dunkelmodus, kein Absturz. Nicht geprüft: neue Oberfläche am echten Handy, Berechtigungsdialog, Vorschaubilder echter Dateien, Scanner-Optik (unverändert).

## Handy-Austausch: ein Ordner und Aktualisieren in der App
- REQUIREMENT: Einfachere Ordner („Handy“ statt zwei Unterordner) und Updates der App ohne Deinstallieren (Benutzer, 11. Oktober 2026).
- DATEI: `src/nolphin-phone.c/.h`, `src/nolphin-view.c`, `src/nolphin-places-sidebar.c`, `android/src/org/nolphin/handy/Updater.java`, `HomeScreen.java`, `MainActivity.java`, `AndroidManifest.xml` (Version 2.1, `REQUEST_INSTALL_PACKAGES`), Hilfe und API-Doku.
- ÄNDERUNG: Ausgang ist `~/Handy` selbst (nur Dateien, Unterordner nicht), Eingang bleibt `~/Handy/Vom Handy`. App prüft `/apk`, zeigt „Neue Version“ und installiert über den Paketinstaller.
- WARUM: Rückmeldung des Benutzers (zu kompliziert, ständiges Neuinstallieren).
- TEST: Nolphin gebaut und installiert, Ordner `~/Handy` verwendet. Emulator: App 2.1 verbunden mit einem Server, der 2.2 liefert: Karte „Neue Version 2.2“, Erlaubnis-Seite, Installationsfrage, danach Version 2.2 installiert, Daten erhalten. Nicht geprüft: am echten Handy; der Ordnerwechsel gegen den echten Server mit der App.

## Handy-Austausch: Sitzung über Neustarts merken
- REQUIREMENT: Das Handy soll nach einem Neustart von Nolphin ohne neuen QR-Code wieder verbunden sein (Benutzer: „hier kommt nichts an“, ständiges Neu-Scannen).
- DATEI: `src/nolphin-phone-server.c/.h` (Optionen `port`, `resume_token`, `resume_expires`), `src/nolphin-phone.c` (`~/.config/nolphin/phone-session`, Modus 0600), `docs/handy-austausch-api.md`.
- ÄNDERUNG: Port und Schlüssel werden bis zum Ablauf (Vorgabe 12 Stunden) wieder verwendet. „Beenden“ und „Neuer QR-Code“ widerrufen. Sicherheitsfolge: Ein Schlüssel lebt über einen Neustart hinaus, aber nie länger als sein Ablauf; die Datei ist nur für den Benutzer lesbar.
- WARUM: Sonst bricht die Verbindung bei jedem Neustart, weil sich Port und Schlüssel ändern.
- TEST: Programm gegen den Server: gleicher Port, gleicher Schlüssel und gleicher Ablauf nach Neustart; abgelaufener oder kaputter Schlüssel wird nicht übernommen. Nolphin baut ohne Fehler. Nicht geprüft: Ablauf mit dem echten Panel und echtem Handy. Der Vertrag verlangt für Änderungen am Sicherheitsverhalten eine Freigabe, die für diese Änderung noch aussteht.

## Übersetzung
- REQUIREMENT: Deutsche Oberfläche.
- DATEI: `po/de.po`, `po/nolphin.pot`, `po/POTFILES.in`, `action-layout-editor/nolphin_action_layout_editor.py`.
- ÄNDERUNG: Vorlage neu erzeugt (war veraltet), `de.po` abgeglichen, Layout-Editor in `POTFILES.in`, `NOLPHIN_LOCALEDIR` auch dort beachtet.
- WARUM: Die Einstellungsseiten erschienen englisch, weil die Glade-Texte im Katalog nur als veraltet geführt wurden.
- TEST: `msgfmt -c`, Build; die Übersetzung im Programm danach nicht erneut angesehen.

## Entfernt
- `src/nolphin-properties-window.{c,h}` (Eigenschaften laufen im Panel).

## Handy-App in eigenes Projekt ausgelagert
- REQUIREMENT: Handy-App und Dateimanager getrennt halten; hier nur das Nötigste.
- DATEI: `android/` (entfernt), `docs/meson.build`, `docs/bauen-und-installieren.md`, `docs/DEPENDENCIES.md`, `README.md`.
- ÄNDERUNG: Der App-Quelltext liegt nur noch im Projekt `nolphin-handy` (GitHub kaysiebke-cell/nolphin-handy). Der PC-Teil (`nolphin-phone*.c/.h`) bleibt hier. Der Build installiert die APK weiter mit, wenn `../nolphin-handy/build/nolphin-handy.apk` vorhanden ist.
- WARUM: Zwei Kopien des App-Codes liefen auseinander.
- TEST: `meson setup` in einem frischen Ordner (Konfiguration ohne Fehler). Kein vollständiger Build und keine Tests ausgeführt.

## Seitenleiste: Eintrag „Vom Handy“
- REQUIREMENT: Klare Ordner für den Handy-Austausch, ohne mehrere Ordner mit verschiedenen Namen.
- DATEI: `src/nolphin-places-sidebar.c`.
- ÄNDERUNG: Unter „Handy“ (`~/Handy`, Dateien fürs Handy) gibt es einen zweiten Eintrag „Vom Handy“ (`~/Handy/Vom Handy`, alles, was vom Handy gesendet wird). Der alte, leere Ordner `~/Handy-Eingang` wird nicht benutzt.
- WARUM: Der Eingangsordner lag nur als Unterordner in „Handy“ und war nicht direkt auffindbar.
- TEST: `meson test` (26 von 26). Die Seitenleiste selbst nach der Installation nicht angesehen.

## Handy-App als optionale Erweiterung verlinkt
- REQUIREMENT: Die Handy-App ist eine optionale Erweiterung des Dateimanagers. Kleine, unauffällige Verlinkung und Beschreibung.
- DATEI: `README.md`, `docs/funktionen.md`, `docs/bilder/handy-erweiterung.png`.
- ÄNDERUNG: Der Highlight-Punkt „Handy-Austausch“ nennt jetzt zuerst den Weg ohne App und verweist auf einen kleinen Abschnitt „Optional: Nolphin Handy für Android“ mit dem Titelbild des Projekts (verkleinert, als Link) und drei Sätzen. In `docs/funktionen.md` ist der Absatz zur App als „optionale Erweiterung“ umgeschrieben und nennt, was die App kann.
- WARUM: Nolphin braucht die App nicht; sie sollte nicht wie ein Bestandteil wirken, aber auffindbar sein.
- TEST: Nur Dokumentation, kein Code geändert. Die Darstellung in der GID-Vorschau von Nolphin wurde nicht angesehen.
