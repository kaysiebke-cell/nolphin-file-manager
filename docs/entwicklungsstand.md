[Startseite](../README.md) | [← Nolphin und Nemo](nolphin-und-nemo.md) | [Projektstruktur →](projektstruktur.md)

# Entwicklungsstand

Nolphin ist Entwicklungssoftware: Funktionen können sich ändern.

## Umgesetzt

* [x] Dateiverwaltung auf Nemo-Basis mit Tabs, geteilter Ansicht (bis zu vier Bereiche) und Ansichten
* [x] Rechter Arbeitsbereich mit Panel-Wechsel
* [x] Vorschau- und Info-Panel, Bewertung, Tags und Kommentar
* [x] PDF-, Audio- und Video-Informationen (über `pdfinfo`, `pdftocairo`, `gst-discoverer-1.0`)
* [x] Eigenschaften im Arbeitsbereich
* [x] Integriertes Terminal (F4)
* [x] Archive komprimieren und entpacken, Archiv-Manager
* [x] DEB-Paket-Ersteller
* [x] Massenumbenennung
* [x] CAD- und 3D-Erkennung (STL, STEP, FCStd u. a.)
* [x] Git-Aktionen und Git-Overlay-Symbole
* [x] Prüfsummen, GPG, ACL
* [x] Papierkorb-Automatik
* [x] Gespeicherte Dateiauswahlen
* [x] Arbeitsbereiche und Merken der letzten Sitzung
* [x] Gehe zu: Verlauf, häufig verwendet, Reiter
* [x] Synchronisation (`rsync`), Regeln, Duplikaterkennung, Versionierung
* [x] Diagnose mit Protokoll, Systeminformationen, Plugin-Status und Fehlerbericht
* [x] Einstellungen exportieren, importieren, zurücksetzen
* [x] Lauffähig unter XFCE und MATE: Cinnamon-Bibliotheken und -Schemas sind optional (siehe [Andere Desktops](andere-desktops.md))
* [x] Handy-Austausch, **nur verschlüsselt und nur mit der Handy-App**: Eintrag „Handy“ in der Seitenleiste, eigener Server mit TLS (selbst erstelltes Zertifikat, pro Schlüssel neu; Fingerabdruck im QR-Code, den die App prüft), feste Ordner `~/Handy/Vom Handy` und `~/Handy/Zum Handy`, Ziehen auf „Handy“ und „An Handy senden“. Der Weg über den Browser des Handys und die Webseite dafür sind entfallen. Voraussetzung am PC: `openssl` und `glib-networking`; fehlt eines, startet der Austausch nicht (kein Rückfall auf unverschlüsselt). Automatisch getestet (`meson test`, 26 von 26): echte TLS-Verbindungen mit Fingerabdruck-Prüfung, Klartext ohne Antwort, Schlüssel- und Zertifikatwechsel, Wiederaufnahme, QR-Größe. Absichtlich eingebaute Fehler wurden vom Test erkannt. Am echten Handy (Android 14) bestätigt: verschlüsselt verbinden, Datei vom PC laden, Datei und Ordner mit Unterordner zum PC senden.
* [x] Handy-App für Android als optionale Erweiterung (eigenes Projekt [`nolphin-handy`](https://github.com/kaysiebke-cell/nolphin-handy), Version 4.27, Stand 13. Oktober 2026). **Am echten Handy gesehen und geprüft:** Verbinden mit dem Scanner der App (liest auch schräg gehaltene Codes), Ablehnung eines falschen Fingerabdrucks und alter Links mit deutlicher Meldung, Senden von Dateien und von Ordnern mit Unterordnern an den PC, Laden einer Datei vom PC, der Dateimanager mit Kopieren, Verschieben, Umbenennen, Löschen, ZIP entpacken und packen, Umbenennen mehrerer Dateien mit Vorschau, Speicheranalyse (Hauptspeicher und SD-Karte) und Papierkorb (Hauptspeicher und SD-Karte, Zurückholen und Leeren). **Nur am PC automatisch getestet** (`nolphin-handy/test.sh`): QR-Leser (181 Prüfungen, darunter starke Schräglage mit Mauszeiger auf dem Code und ein künstlich verwischtes Testbild), Link-Prüfung (55), Fingerabdruck-Prüfung der App (16), App-Seite gegen den echten Nolphin-Server (7), Dateioperationen (60), Sortierung (22), ZIP und Umbenennen (52), Speicheranalyse (39) und Papierkorb (39). **Nicht am Handy geprüft:** ob andere Scanner-Apps `nolphin://`-Links weitergeben (der Scanner von Xiaomi gibt nichts an die App zurück und wird nicht benutzt), automatisches Löschen im Papierkorb nach 30 Tagen, der helle Modus der Fenster für Speicheranalyse und Papierkorb. Die Oberfläche selbst ist nicht automatisch getestet. Die Verbindung ist verschlüsselt; wer den QR-Code sieht, kann sich bis zum Widerruf verbinden.
* [x] GID-Projekte: lokale Projektliste in der Seitenleiste, README im Arbeitsbereich im Aussehen von GitHub (Tabellen, Aufgabenlisten, lokale Bilder, HTML-Teilmenge)

## Teststand

`meson test -C build` (Stand: 11. Oktober 2026): **alle 25 Tests bestehen**, sowohl im normalen Build als auch im Build ohne `cinnamon-desktop` und `xapp`.

Neu sind der Thumbnailer-Test (eigene Vorschaubild-Erzeugung), der Test zur Wahl des Desktop-Schemas und der Test des Symbol-Ersatzes (prüft auch, dass jeder im Quellcode benutzte `xsi`-Name in der Ersatz-Liste steht), siehe [Andere Desktops](andere-desktops.md). Der Schema-Test prüft gegen die auf dem Rechner installierten Schemas und überspringt Fälle, deren Schemas fehlen.

Neu sind der Test des Markdown-Renderers (Tabellen, Aufgabenlisten, Listenebenen, Überschriften, HTML-Teilmenge, Entities, lokale Bilder) und der Test der Projektliste der GID-Projekte (Hinzufügen, Entfernen, Branch-Erkennung).

Die drei Tests Copy, Search Engine und Directory Async waren Demo-Programme aus dem Nemo-Ursprung, die Argumente erwarteten oder endlos liefen. Sie sind jetzt echte Tests: Kopieren in einen Temp-Ordner, Namensuche mit den Operatoren und Laden eines Ordners samt neuer Datei.

Die Tests brauchen eine laufende grafische Sitzung (GTK). Ohne Barrierefreiheits-Bus, etwa in einer reinen Shell-Sitzung, bricht der Eel-Test wegen einer Warnung ab; mit `NO_AT_BRIDGE=1 meson test -C build` läuft er durch. Für die Werkzeuge Sync, Regeln, Duplikate und Versionen sowie die Fensterfunktionen gibt es noch keine automatischen Tests.

---

[Weiter: Projektstruktur →](projektstruktur.md)
