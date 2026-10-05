# Nolphin

**Ein nativer Linux-Dateimanager für Linux Mint Cinnamon – mit einem festen rechten Arbeitsbereich für Vorschau, Eigenschaften, Archive, Terminal, Git und Paketbau.**

Nolphin ist ein eigenständiger Dateimanager auf Basis von Nemo 6.7.7 (GTK3, C11). Er wurde vollständig in **Nolphin** umbenannt und um eigene Funktionen erweitert. Die Hauptansicht bleibt sichtbar, während rechts daneben die jeweils gewählte Funktion bedient wird – ohne zusätzliche Fenster.

> **Hinweis:** Nolphin ist ein unabhängiges Open-Source-Projekt und kein offizielles Projekt von Linux Mint.

![Nolphin: links die Orte, in der Mitte die Dateien des Ordners „Bilder“, rechts der Arbeitsbereich mit den Ordner-Informationen](docs/bilder/uebersicht.png)

---

## Projektphilosophie

**Die Funktion kommt zur Datei – nicht umgekehrt.**

Wer mit technischen Dateien arbeitet, wechselt sonst ständig zwischen Dateimanager, Terminal und Zusatzprogrammen. Nolphin holt diese Werkzeuge in das Hauptfenster.

* **Ehrlich statt vorgetäuscht:** Fehlt ein Hilfsprogramm oder ein Vorschau-Backend, zeigt Nolphin das an, statt eine Funktion zu simulieren.
* **Vertraute Bedienung:** Die klassische Nemo-Bedienung bleibt erhalten.
* **Nur Belegtes ist fertig:** Diese README beschreibt den Stand des Codes. Als „umgesetzt“ gilt, was im Code vorhanden ist, als „getestet“ nur, was die Meson-Tests abdecken.

---

## Schnellstart

Aus dem Quellcode bauen und direkt aus dem Build-Verzeichnis starten:

```bash
meson setup build
ninja -C build
env GSETTINGS_SCHEMA_DIR="$(pwd)/build/libnolphin-private" \
    LD_LIBRARY_PATH="$(pwd)/build/libnolphin-extension" \
    ./build/src/nolphin
```

Mit **F11** blendest du den rechten Arbeitsbereich ein, mit **F4** das Terminal.

Voraussetzungen, Installation und Tests: siehe [Bauen und Installieren](#bauen-und-installieren).

---

## Highlights

* **Rechter Arbeitsbereich (F11):** ein fester Bereich für Vorschau, Eigenschaften, Archive, Terminal, Git, DEB-Ersteller, Massenumbenennung und die Werkzeuge unten – ohne zusätzliche Fenster.
* **Vorschau mit Medieninfos:** Bilder, **PDF** (Titel, Autor, Seiten, Seitengröße), **Video** und **Audio** (Dauer, Codec, Auflösung, Bildrate, Kanäle).
* **Metadaten:** **Bewertung (Sterne), Tags und Kommentar** direkt im Info-Panel.
* **Git direkt im Dateimanager:** Overlay-Symbole an den Dateien (hinzugefügt, geändert, unversioniert), Panel für Status, Speichern, Server-Abgleich, Verlauf und Unterschiede.
* **Archive:** Komprimieren, Entpacken, Prüfen und – im **Archiv-Manager** – Inhalt auflisten sowie Dateien hinzufügen, ersetzen und entfernen.
* **Werkzeuge:** Ordner **vergleichen und synchronisieren** (rsync, löscht nie), **Regeln anwenden**, **Duplikate finden**, **Versionen** einer Datei speichern und wiederherstellen.
* **Mehrere Bereiche:** geteilte Ansicht mit bis zu vier Bereichen und **Arbeitsbereiche**, die Reiter, Teilung und Fenstergröße speichern.
* **Gehe zu:** Verlauf, häufig verwendete Orte und die Reiter des Fensters im Menü.
* **Diagnose:** Protokolle, Systeminformationen, Plugin-Status und Fehlerbericht im Panel.
* **Einstellungen exportieren, importieren und zurücksetzen.**
* **DEB-Pakete erstellen:** Nolphin baut ein `.deb` selbst als `ar`-Archiv, ohne `dpkg-deb`.
* **Massenumbenennung:** Suchen/Ersetzen, Nummerierung und Groß-/Kleinschreibung mit Vorschau, rückgängig machbar.
* **CAD- und 3D-Erkennung:** STL, STEP/STP, FreeCAD `.FCStd` und weitere Formate.
* **Sicherheit:** Prüfsummen, GPG-Verschlüsselung, ACL-Verwaltung.
* **Papierkorb-Automatik** und **gespeicherte Dateiauswahlen**.
* **Nemo-Grundlagen:** Tabs, Symbol-, Listen- und Kompaktansicht, Fortschritt, Rückgängig, Netzwerk über GVfs.

---

## Der rechte Arbeitsbereich

```text
┌──────────────────────────────────────────────────────────────────┐
│ Menü / Werkzeugleiste / Adresse                                  │
├──────────────┬──────────────────────────┬────────────────────────┤
│ Seitenleiste │ Hauptansicht             │ Arbeitsbereich         │
│ Orte         │ Dateien und Ordner       │   Vorschau (Ruhelage)  │
│              │                          │   Eigenschaften        │
│              │                          │   Archiv               │
│              │                          │   Terminal             │
│              │                          │   Suche                │
│              │                          │   Git                  │
│              │                          │   DEB-Ersteller        │
│              │                          │   Massenumbenennung    │
│              │                          │   Archiv-Manager       │
│              │                          │   Arbeitsbereiche      │
│              │                          │   Diagnose             │
│              │                          │   Duplikate, Versionen │
│              │                          │   Synchronisation      │
│              │                          │   Regeln               │
├──────────────┴──────────────────────────┴────────────────────────┤
│ Statusleiste                                                     │
└──────────────────────────────────────────────────────────────────┘
```

| Panel | Aufruf | Inhalt |
| ----- | ------ | ------ |
| Vorschau / Information | **F11** | Vorschau und Dateiinformationen; hierhin kehrt der Arbeitsbereich zurück |
| Eigenschaften | Menü, Kontextmenü | Zugriffsrechte, Besitzer, Gruppe, ACL |
| Archiv | Menü, Kontextmenü | Komprimieren |
| Archiv-Manager | Kontextmenü „Archiv öffnen …“ | Inhalt, Hinzufügen, Ersetzen, Entfernen, Prüfen, Entpacken |
| Terminal | **F4** | VTE-Terminal im aktuellen Ordner |
| Suche | Suchaktion | Such- und Filterleiste |
| Git | Menü, Kontextmenü | Status und Aktionen |
| DEB-Ersteller | Menü, Kontextmenü | `.deb`-Paket aus Dateien und Ordnern |
| Massenumbenennung | Menü | Umbenennen mit Vorschau |
| Arbeitsbereiche | Datei ▸ Arbeitsbereiche | Layout speichern, laden, duplizieren, löschen |
| Diagnose | Hilfe ▸ Diagnose | Protokolle, Systeminformationen, Plugin-Status, Fehlerbericht |
| Duplikate | Bearbeiten ▸ Werkzeuge | Gleiche Dateien finden, in den Papierkorb legen |
| Versionen | Bearbeiten ▸ Werkzeuge ▸ Versionen | Versionen einer Datei speichern, vergleichen, wiederherstellen |
| Synchronisation | Bearbeiten ▸ Werkzeuge | Zwei Ordner vergleichen und abgleichen |
| Regeln | Bearbeiten ▸ Werkzeuge | Bedingung und Aktion mit Vorschau auf einen Ordner anwenden |

Ein Panel wird im Arbeitsbereich wiederverwendet und öffnet kein zusätzliches Fenster. Klassische Dialoge gibt es weiterhin für Dateiauswahl, Lösch- und Überschreibbestätigungen, Fehlermeldungen, Mount-Zugangsdaten und „Über Nolphin“.

### So sehen die Panels aus

**Eigenschaften** – Zugriffsrechte, Besitzer und Gruppe sowie zusätzliche Benutzer und Gruppen (ACL), ohne das Hauptfenster zu verlassen:

![Eigenschaften-Panel für den Ordner „Bilder“ mit Zugriffsrechten und ACL-Bereich](docs/bilder/panel-eigenschaften.png)

**Symbol auswählen** – ein Symbol für Ordner oder Dateien wählen, durchsuchbar und nach Kategorien sortiert:

![Panel zur Symbolauswahl mit Kategorien und Suchfeld](docs/bilder/panel-symbol-waehlen.png)

**Git** – Änderungen anzeigen, speichern (Commit), mit dem Server abgleichen, Verlauf und Unterschiede. Die Statusliste unten zeigt, was hinzugefügt, geändert oder noch unversioniert ist. Außerhalb eines Repositorys meldet das Panel das ehrlich.

![Git-Panel mit den Schritten Änderungen, Speichern und Server sowie der Statusliste](docs/bilder/panel-git.png)

Im Ordner selbst zeigen **Overlay-Symbole** den Git-Status jeder Datei (Häkchen = hinzugefügt, Ausrufezeichen = geändert, Plus = unversioniert). Alle Git-Aktionen stehen im Kontextmenü unter „Git“:

| Overlays im Ordner | Git-Untermenü |
| --- | --- |
| ![Dateien eines Git-Ordners mit Overlay-Symbolen](docs/bilder/git-overlays.png) | ![Kontextmenü mit Git-Untermenü](docs/bilder/git-menue.png) |

**Archiv** – Name, Format und Zielort wählen; Passwort und Teilarchive stehen unter „Erweiterte Optionen“:

![Archiv-Panel mit Dateiname, Format ZIP, Ort und erweiterten Optionen](docs/bilder/panel-archiv.png)

**Archiv-Manager** – bei einem vorhandenen Archiv zeigt „Archiv öffnen …“ den Inhalt und erlaubt Entpacken, Hinzufügen, Ersetzen, Entfernen und Prüfen:

| Kontextmenü eines Archivs | Archiv-Manager |
| --- | --- |
| ![Kontextmenü einer ZIP-Datei mit Archiv-Einträgen](docs/bilder/kontextmenue-archiv.png) | ![Archiv-Manager mit Inhaltsliste einer ZIP-Datei](docs/bilder/panel-archiv-manager.png) |

**DEB-Ersteller** – Paketname, Version, Beschreibung und Ersteller eintragen, Dateien und Ordner mit Zielpfad hinzufügen und das Paket erstellen:

![DEB-Ersteller-Panel mit ausgefüllten Eingabefeldern und leerer Dateiliste](docs/bilder/panel-deb-ersteller.png)

**Terminal (F4)** – das eingebettete Terminal öffnet im aktuellen Ordner und bleibt neben der Hauptansicht:

![Terminal-Panel neben der Dateiansicht des Ordners „Bilder“](docs/bilder/panel-terminal.png)

**Vorschau und Metadaten** – PDF und Video zeigen eine Vorschau samt Eckdaten; zu jeder Datei lassen sich Bewertung, Tags und Kommentar festhalten:

| PDF | Video |
| --- | --- |
| ![Vorschau einer PDF-Datei mit Titel, Autor, Seiten und Seitengröße](docs/bilder/vorschau-pdf.png) | ![Vorschau eines Videos mit Dauer, Codec, Auflösung und Bildrate](docs/bilder/vorschau-video.png) |

![Info-Panel eines Bildes mit vier Sternen, Tags und Kommentar](docs/bilder/info-bewertung.png)

**Werkzeuge** – zu finden unter Bearbeiten ▸ Werkzeuge; jedes Werkzeug arbeitet mit einer **Vorschau**, bevor etwas verändert wird:

![Menü Bearbeiten ▸ Werkzeuge mit Ordner vergleichen, Regeln, Stapelverarbeitung, Duplikate und Versionen](docs/bilder/menue-werkzeuge.png)

| Duplikate | Versionen |
| --- | --- |
| ![Duplikate-Panel mit einer Gruppe gleicher Dateien](docs/bilder/panel-duplikate.png) | ![Versionen-Panel mit zwei gespeicherten Versionen und Unterschieden](docs/bilder/panel-versionen.png) |
| **Synchronisation** | **Regeln** |
| ![Synchronisations-Panel mit Neu, Geändert und Konflikt](docs/bilder/panel-sync.png) | ![Regeln-Panel mit Bedingung, Aktion und Vorschau](docs/bilder/panel-regeln.png) |

**Massenumbenennung** – Suchen/Ersetzen und Nummerierung mit Vorschau der neuen Namen:

![Massenumbenennung mit Suchen, Ersetzen und Nummerierung](docs/bilder/panel-massenumbenennung.png)

**Geteilte Ansicht und Arbeitsbereiche** – bis zu vier Bereiche nebeneinander; das gesamte Layout lässt sich als Arbeitsbereich speichern:

| Geteilte Ansicht | Vier Bereiche |
| --- | --- |
| ![Menü Ansicht ▸ Geteilte Ansicht](docs/bilder/menue-geteilte-ansicht.png) | ![Vier Bereiche mit verschiedenen Ordnern](docs/bilder/ansicht-vier-bereiche.png) |
| ![Menü Datei ▸ Arbeitsbereiche](docs/bilder/menue-arbeitsbereiche.png) | ![Arbeitsbereiche-Panel mit einem gespeicherten Arbeitsbereich](docs/bilder/panel-arbeitsbereiche.png) |

**Gehe zu und Hilfe** – Verlauf, häufig verwendete Orte und Reiter im Menü „Gehe zu“, die Diagnose im Menü „Hilfe“:

| Gehe zu | Hilfe |
| --- | --- |
| ![Menü Gehe zu mit Häufig verwendet](docs/bilder/menue-gehe-zu.png) | ![Menü Hilfe mit Diagnose-Untermenü](docs/bilder/menue-hilfe.png) |

**Diagnose** – Protokolle, Systeminformationen mit gefundenen Werkzeugen, Plugin-Status und Fehlerbericht in einem Panel:

![Diagnose-Panel mit Systeminformationen und gefundenen Werkzeugen](docs/bilder/panel-diagnose.png)

---

## Funktionen im Detail

### Dateiverwaltung

Tabs, Split View (F3), Symbol-, Listen- und Kompaktansicht, Kopieren und Verschieben mit Pause und Abbruch, Rückgängig und Wiederholen, Papierkorb, Drag & Drop, Berechtigungen, Netzwerkzugriff über GVfs (SMB, NFS, HTTP, HTTPS).

### Archive

| Erstellen und Entpacken | Nur Entpacken und Auslesen (über `7z`) |
| ----------------------- | -------------------------------------- |
| ZIP, TAR, TAR.GZ, TAR.BZ2, TAR.XZ, TAR.ZST, TAR.LZ4, 7Z | CAB, ARJ, LZH, ISO, CPIO, RPM, DEB |
| Einzeldateien: XZ, ZST, LZ4 | |

Ist das benötigte Werkzeug nicht installiert, wird das erkannt und angezeigt. Der **Archiv-Manager** liest den Inhalt eines Archivs aus und ändert es gezielt (hinzufügen, ersetzen, entfernen); er arbeitet mit denselben Systemwerkzeugen.

### Vorschau und Metadaten

* **PDF:** Titel, Autor, Seitenzahl, Seitengröße und PDF-Version über `pdfinfo`, Vorschaubild über `pdftocairo` (Paket `poppler-utils`).
* **Audio und Video:** Dauer, Container, Codecs, Auflösung, Bildrate, Kanäle und Abtastrate über `gst-discoverer-1.0` (Paket `gstreamer1.0-tools`).
* **Bewertung, Tags, Kommentar:** werden als Datei-Metadaten über GIO gespeichert und im Info-Panel angezeigt.

Fehlt eines der Werkzeuge, steht im Panel ein Hinweis, statt Werte vorzutäuschen.

### Git

Der Git-Status jeder Datei erscheint als Overlay-Symbol im Ordner. Das Panel führt durch die Schritte „Änderungen“, „Speichern“ und „Mit dem Server“ und zeigt Verlauf und Unterschiede.

### Werkzeuge

* **Ordner vergleichen und synchronisieren:** vergleicht zwei Ordner über `rsync -n` und zeigt, was neu, geändert oder in Konflikt ist. Abgeglichen wird in **eine** Richtung, im Ziel wird **nie etwas gelöscht**.
* **Regeln anwenden:** Bedingungen (Dateityp, Name mit Platzhaltern, Größe, Änderungsdatum) und eine Aktion; zuerst die Vorschau, dann „Anwenden“. Regeln werden derzeit nicht dauerhaft gespeichert.
* **Duplikate finden:** nach Name, Größe oder Inhalt (Prüfsumme) in einem Ordner samt Unterordnern; gelöscht wird nur in den Papierkorb und nach Bestätigung.
* **Versionen:** Nolphin sichert Versionen einer Datei in `~/.local/share/nolphin/versions`; wiederherstellen, löschen und – bei Textdateien – mit der aktuellen Datei vergleichen.

### Fenster, Reiter und Orte

* **Geteilte Ansicht:** bis zu vier Bereiche, Aufteilung waagerecht oder senkrecht, Bereiche duplizieren, maximieren und schließen.
* **Arbeitsbereiche:** speichern Reiter, Teilung, Fenstergröße und Panels unter einem Namen; Laden, Duplizieren und Löschen im Panel. Beim Beenden merkt sich Nolphin die letzte Sitzung.
* **Gehe zu:** Verlauf, häufig verwendete Orte und Reiter; „Zwischenablage als Datei einfügen“ im Bearbeiten-Menü.

### Diagnose

Unter Hilfe ▸ Diagnose zeigt Nolphin das Protokoll, Systeminformationen mit den gefundenen Hilfswerkzeugen, den Status der geladenen Plugins und erstellt einen Fehlerbericht.

### DEB-Paket erstellen

Der Ersteller (`src/nolphin-deb-builder.c`) schreibt das Paket aus `debian-binary`, `control.tar.gz` und `data.tar.gz` direkt als `ar`-Archiv. Pflichtfelder sind Paketname, Version, Architektur, Maintainer und Beschreibung. Sektion, Priorität, Abhängigkeiten und Homepage sind optional.

### CAD- und 3D-Dateien

* **STL:** ASCII oder binär, bei Binär-STL die Anzahl der Dreiecke
* **STEP/STP:** Header-Informationen (Beschreibung, Dateiname, Zeitstempel, Autor, Schema)
* **FreeCAD `.FCStd`:** Dokumentinformationen und eingebettetes Vorschaubild
* **IGES, OBJ, 3MF, DXF, DWG:** Erkennung; gibt es für ein Format noch keine echte Vorschau, steht das ausdrücklich da

### Prüfsummen, Verschlüsselung, ACL

* **Prüfsummen:** MD5, SHA-1, SHA-256, SHA-512, BLAKE2
* **Verschlüsselung:** GPG
* **ACL:** Verwaltung über `getfacl` und `setfacl`

### Papierkorb und Auswahlen

Der Papierkorb wird nach einer einstellbaren Aufbewahrungsdauer bereinigt, bei Überschreiten eines Größenlimits erscheint eine Warnung. Benannte Dateiauswahlen lassen sich speichern und wiederherstellen.

---

## Einstellungen und Anpassung

Ansichten, Verhalten, Anzeige, Listenspalten, Vorschau, Werkzeugleiste, Kontextmenüs, Dokumentvorlagen und Module haben jeweils eine eigene Einstellungsseite. Unten stehen **Exportieren …**, **Importieren …** und **Zurücksetzen …** für alle Einstellungen.

> **Bekannt:** Einzelne Texte (zum Beispiel im Layout-Editor und in manchen Einstellungsseiten) sind noch englisch.

| | |
| --- | --- |
| **Verhalten** ![Einstellungen: Verhalten](docs/bilder/einstellungen-verhalten.png) | **Anzeige** ![Einstellungen: Anzeige](docs/bilder/einstellungen-anzeige.png) |
| **Listenspalten** ![Einstellungen: Listenspalten](docs/bilder/einstellungen-listenspalten.png) | **Vorschau** ![Einstellungen: Vorschau](docs/bilder/einstellungen-vorschau.png) |
| **Werkzeugleiste** ![Einstellungen: Werkzeugleiste](docs/bilder/einstellungen-werkzeugleiste.png) | **Kontextmenüs** ![Einstellungen: Kontextmenüs](docs/bilder/einstellungen-kontextmenues.png) |
| **Dokumentvorlagen** ![Einstellungen: Dokumentvorlagen](docs/bilder/einstellungen-vorlagen.png) | **Plugins und Aktionen** ![Einstellungen: Aktionen](docs/bilder/einstellungen-aktionen.png) |

Reihenfolge und Aussehen der Aktionen in den Menüs bearbeitest du im Layout-Editor:

![Nolphin Actions Layout Editor mit der Liste der Aktionen und Schaltflächen zum Verschieben](docs/bilder/aktionen-layout-editor.png)

---

## Entwicklungsstand

Nolphin ist Entwicklungssoftware: Funktionen können sich ändern.

### Umgesetzt

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

### Geplant und bekannte Grenzen

* [ ] Erweiterte Suche: UND, ODER, NICHT, kombinierbare Filter
* [ ] Netzwerk: SFTP, FTP, WebDAV prüfen
* [ ] PDF und Medien direkt über Poppler-GLib und GStreamer-Bibliotheken statt über Kommandozeilenwerkzeuge
* [ ] Regeln dauerhaft speichern und automatisch auslösen
* [ ] Überlagerungen für SVN und Mercurial (bisher nur Git)
* [ ] Vollständige Übersetzung aller Einstellungstexte
* [ ] Eigene Tests und Dokumentation für die neuen Module

### Teststand

`meson test -C build` (Stand: 5. Oktober 2026): **13 von 16 Tests bestehen.**

| Test | Ergebnis |
| ---- | -------- |
| Copy test | Fehler |
| Search Engine test | Timeout nach 30 s |
| Directory Async test | Timeout nach 30 s |

Diese drei Punkte sind offen und nicht als erledigt zu betrachten.

---

## Nolphin und Nemo

| Bereich | Nemo | Nolphin |
| ------- | :--: | :-----: |
| Klassische Dateiverwaltung, Tabs, Split View | ✓ | ✓ |
| Integriertes Terminal | ✓ | ✓ (im Arbeitsbereich) |
| Rechter Arbeitsbereich mit Panels | – | ✓ |
| Archive im Panel | – | ✓ |
| DEB-Paket-Ersteller | – | ✓ |
| Massenumbenennung mit Vorschau | – | ✓ |
| STL-, STEP- und FCStd-Informationen | – | ✓ |
| Git-Aktionen | – | ✓ |
| Prüfsummen, GPG, ACL | – | ✓ |
| Papierkorb-Automatik | – | ✓ |
| Gespeicherte Dateiauswahlen | – | ✓ |
| Git-Overlays am Dateisymbol | – | ✓ |
| PDF-, Audio- und Video-Informationen | – | ✓ |
| Bewertung, Tags, Kommentar im Info-Panel | – | ✓ |
| Archiv-Manager | – | ✓ |
| Synchronisation, Regeln, Duplikate, Versionen | – | ✓ |
| Arbeitsbereiche, bis zu vier Bereiche | – | ✓ |
| Diagnose-Panel mit Fehlerbericht | – | ✓ |
| Einstellungen exportieren und importieren | – | ✓ |

---

## Bauen und Installieren

### Voraussetzungen

Nolphin läuft ausschließlich unter Linux und ist für Linux Mint mit Cinnamon optimiert.

* Meson (≥ 0.64), Ninja, C-Compiler (C11)
* GTK3, GLib, GIO, GVfs, VTE 2.91, XApp, cinnamon-desktop
* optional: libexif, exempi
* Laufzeitwerkzeuge, je nach Funktion: `zip`/`unzip`, `tar`, `7z`, `zstd`, `lz4`, `xz`, `git`, `gpg`, `getfacl`/`setfacl`, `rsync`, `pdfinfo`/`pdftocairo` (poppler-utils), `gst-discoverer-1.0` (gstreamer1.0-tools)

### Bauen und testen

```bash
meson setup build
ninja -C build
meson test -C build
```

### Installieren

```bash
sudo ninja -C build install
```

Installationsweg und Paketierung (`debian/`) können sich während der Entwicklung ändern.

---

## Projektstruktur

```text
src/                    Hauptquellcode (Fenster, Ansichten, Panels)
libnolphin-private/     interne Bibliothek (Archiv, Prüfsummen, ACL, GPG, Schemas)
libnolphin-extension/   Erweiterungs-Schnittstelle
gresources/             Beschreibungen der Oberfläche
test/                   Tests
docs/                   Referenzdokumente, Bilder für diese README (docs/bilder/)
debian/                 Debian-Paketierung
po/                     Übersetzungsdateien
```

Die Einstellungen liegen im GSettings-Schema `org.nolphin` (`libnolphin-private/org.nolphin.gschema.xml`).

### Weitere Dokumente

| Thema | Dokument |
| ----- | -------- |
| Drag & Drop | [docs/dnd.txt](docs/dnd.txt) |
| Tastatur und Maus | [docs/key_mouse_navigation.txt](docs/key_mouse_navigation.txt) |
| Ein- und Ausgabe | [docs/nolphin-io.txt](docs/nolphin-io.txt) |

---

## Herkunft

Nolphin begann als Quellstand von **Nemo 6.7.7**, wurde umbenannt, strukturell angepasst und schrittweise um eigene Funktionen erweitert. Der Verlauf steht in der Git-Historie.

---

## Fehler melden

Ein hilfreicher Bericht enthält:

```text
Nolphin-Version bzw. Commit:
Linux-Mint-Version:
Cinnamon-Version:

Problem:
Schritte zur Reproduktion:
Erwartetes Verhalten:
Tatsächliches Verhalten:
Fehlermeldungen:
```

---

## Lizenz

**GPL-2.0-or-later**, siehe [COPYING](COPYING).

---

**Nolphin – Dateien verwalten. Informationen sehen. Mehr direkt erledigen.**
