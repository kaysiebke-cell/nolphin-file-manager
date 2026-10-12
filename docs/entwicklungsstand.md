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
* [x] Handy-Austausch über den Browser des Handys: Eintrag „Handy“ in der Seitenleiste, eigener Server und QR-Code, feste Ordner `~/Handy/Vom Handy` und `~/Handy/Zum Handy`, Ziehen auf „Handy“ und „An Handy senden“. Upload vom Handy am echten Handy bestätigt; Senden an das Handy per `curl` und im Emulator geprüft.
* [ ] Handy-App für Android (eigenes Projekt `nolphin-handy`): TEILWEISE. Im Emulator (Android 14) gegen den echten Server geprüft: Verbinden, Dateien in beide Richtungen, Sonderzeichen, 60 MB, Abbruch, Verbindungsverlust, Schlüsselwechsel. Am echten Handy: Installation und Verbinden gesehen; Senden aus der Galerie schlug fehl (behoben, am echten Handy nicht erneut bestätigt); der Live-QR-Scanner wurde als „funktioniert nicht“ gemeldet (Ursache offen). Freigabe am 11. Oktober 2026 erteilt. App 2.0 mit eigener Oberfläche (Start, Vom PC, Zum PC, Verlauf, Einstellungen) im Emulator geprüft, am echten Handy noch nicht.
* [x] GID-Projekte: lokale Projektliste in der Seitenleiste, README im Arbeitsbereich im Aussehen von GitHub (Tabellen, Aufgabenlisten, lokale Bilder, HTML-Teilmenge)

## Teststand

`meson test -C build` (Stand: 11. Oktober 2026): **alle 25 Tests bestehen**, sowohl im normalen Build als auch im Build ohne `cinnamon-desktop` und `xapp`.

Neu sind der Thumbnailer-Test (eigene Vorschaubild-Erzeugung), der Test zur Wahl des Desktop-Schemas und der Test des Symbol-Ersatzes (prüft auch, dass jeder im Quellcode benutzte `xsi`-Name in der Ersatz-Liste steht), siehe [Andere Desktops](andere-desktops.md). Der Schema-Test prüft gegen die auf dem Rechner installierten Schemas und überspringt Fälle, deren Schemas fehlen.

Neu sind der Test des Markdown-Renderers (Tabellen, Aufgabenlisten, Listenebenen, Überschriften, HTML-Teilmenge, Entities, lokale Bilder) und der Test der Projektliste der GID-Projekte (Hinzufügen, Entfernen, Branch-Erkennung).

Die drei Tests Copy, Search Engine und Directory Async waren Demo-Programme aus dem Nemo-Ursprung, die Argumente erwarteten oder endlos liefen. Sie sind jetzt echte Tests: Kopieren in einen Temp-Ordner, Namensuche mit den Operatoren und Laden eines Ordners samt neuer Datei.

Die Tests brauchen eine laufende grafische Sitzung (GTK). Ohne Barrierefreiheits-Bus, etwa in einer reinen Shell-Sitzung, bricht der Eel-Test wegen einer Warnung ab; mit `NO_AT_BRIDGE=1 meson test -C build` läuft er durch. Für die Werkzeuge Sync, Regeln, Duplikate und Versionen sowie die Fensterfunktionen gibt es noch keine automatischen Tests.

---

[Weiter: Projektstruktur →](projektstruktur.md)
