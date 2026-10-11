[Startseite](../README.md) | [← Bauen und Installieren](bauen-und-installieren.md) | [Funktionen im Detail →](funktionen.md)

# Der rechte Arbeitsbereich

```text
┌──────────────────────────────────────────────────────────────────┐
│ Menü / Werkzeugleiste / Adresse                                  │
├──────────────┬──────────────────────────┬────────────────────────┤
│ Seitenleiste │ Hauptansicht             │ Arbeitsbereich         │
│ Orte         │ Dateien und Ordner       │   Vorschau (Ruhelage)  │
│              │                          │   Eigenschaften        │
│              │                          │   Archiv               │
│ GID-Projekte │                          │   Terminal             │
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
│              │                          │   GID-Projekt (README) │
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
| Handy-Austausch | Seitenleiste „Handy“, Datei ▸ Handy-Austausch | QR-Code, Link und Liste der vom Handy empfangenen Dateien |
| GID-Projekt | Seitenleiste „GID-Projekte“, Gehe zu ▸ GID-Projekte | `README.md` des gewählten Projekts im Aussehen von GitHub |

Ein Panel wird im Arbeitsbereich wiederverwendet und öffnet kein zusätzliches Fenster. Klassische Dialoge gibt es weiterhin für Dateiauswahl, Lösch- und Überschreibbestätigungen, Fehlermeldungen, Mount-Zugangsdaten und „Über Nolphin“.

## So sehen die Panels aus

**Eigenschaften** – Zugriffsrechte, Besitzer und Gruppe sowie zusätzliche Benutzer und Gruppen (ACL), ohne das Hauptfenster zu verlassen:

![Eigenschaften-Panel für den Ordner „Bilder“ mit Zugriffsrechten und ACL-Bereich](bilder/panel-eigenschaften.png)

**Symbol auswählen** – ein Symbol für Ordner oder Dateien wählen, durchsuchbar und nach Kategorien sortiert:

![Panel zur Symbolauswahl mit Kategorien und Suchfeld](bilder/panel-symbol-waehlen.png)

**Git** – Änderungen anzeigen, speichern (Commit), mit dem Server abgleichen, Verlauf und Unterschiede. Die Statusliste unten zeigt, was hinzugefügt, geändert oder noch unversioniert ist. Außerhalb eines Repositorys meldet das Panel das ehrlich.

![Git-Panel mit den Schritten Änderungen, Speichern und Server sowie der Statusliste](bilder/panel-git.png)

Im Ordner selbst zeigen **Overlay-Symbole** den Git-Status jeder Datei (Häkchen = hinzugefügt, Ausrufezeichen = geändert, Plus = unversioniert). Alle Git-Aktionen stehen im Kontextmenü unter „Git“:

| Overlays im Ordner | Git-Untermenü |
| --- | --- |
| ![Dateien eines Git-Ordners mit Overlay-Symbolen](bilder/git-overlays.png) | ![Kontextmenü mit Git-Untermenü](bilder/git-menue.png) |

**Archiv** – Name, Format und Zielort wählen; Passwort und Teilarchive stehen unter „Erweiterte Optionen“:

![Archiv-Panel mit Dateiname, Format ZIP, Ort und erweiterten Optionen](bilder/panel-archiv.png)

**Archiv-Manager** – bei einem vorhandenen Archiv zeigt „Archiv öffnen …“ den Inhalt und erlaubt Entpacken, Hinzufügen, Ersetzen, Entfernen und Prüfen:

| Kontextmenü eines Archivs | Archiv-Manager |
| --- | --- |
| ![Kontextmenü einer ZIP-Datei mit Archiv-Einträgen](bilder/kontextmenue-archiv.png) | ![Archiv-Manager mit Inhaltsliste einer ZIP-Datei](bilder/panel-archiv-manager.png) |

**DEB-Ersteller** – Paketname, Version, Beschreibung und Ersteller eintragen, Dateien und Ordner mit Zielpfad hinzufügen und das Paket erstellen:

![DEB-Ersteller-Panel mit ausgefüllten Eingabefeldern und leerer Dateiliste](bilder/panel-deb-ersteller.png)

**Terminal (F4)** – das eingebettete Terminal öffnet im aktuellen Ordner und bleibt neben der Hauptansicht:

![Terminal-Panel neben der Dateiansicht des Ordners „Bilder“](bilder/panel-terminal.png)

**Vorschau und Metadaten** – PDF und Video zeigen eine Vorschau samt Eckdaten; zu jeder Datei lassen sich Bewertung, Tags und Kommentar festhalten:

| PDF | Video |
| --- | --- |
| ![Vorschau einer PDF-Datei mit Titel, Autor, Seiten und Seitengröße](bilder/vorschau-pdf.png) | ![Vorschau eines Videos mit Dauer, Codec, Auflösung und Bildrate](bilder/vorschau-video.png) |

![Info-Panel eines Bildes mit vier Sternen, Tags und Kommentar](bilder/info-bewertung.png)

**Werkzeuge** – zu finden unter Bearbeiten ▸ Werkzeuge; jedes Werkzeug arbeitet mit einer **Vorschau**, bevor etwas verändert wird:

![Menü Bearbeiten ▸ Werkzeuge mit Ordner vergleichen, Regeln, Stapelverarbeitung, Duplikate und Versionen](bilder/menue-werkzeuge.png)

| Duplikate | Versionen |
| --- | --- |
| ![Duplikate-Panel mit einer Gruppe gleicher Dateien](bilder/panel-duplikate.png) | ![Versionen-Panel mit zwei gespeicherten Versionen und Unterschieden](bilder/panel-versionen.png) |
| **Synchronisation** | **Regeln** |
| ![Synchronisations-Panel mit Neu, Geändert und Konflikt](bilder/panel-sync.png) | ![Regeln-Panel mit Bedingung, Aktion und Vorschau](bilder/panel-regeln.png) |

**Massenumbenennung** – Suchen/Ersetzen und Nummerierung mit Vorschau der neuen Namen:

![Massenumbenennung mit Suchen, Ersetzen und Nummerierung](bilder/panel-massenumbenennung.png)

**Geteilte Ansicht und Arbeitsbereiche** – bis zu vier Bereiche mit eigenen Orten und Reitern. Über den Pfeil neben dem Split-Knopf oder *Ansicht ▸ Geteilte Ansicht ▸ Layout* wählst du ein fertiges Layout: zwei oder drei Spalten, 2×2, einen großen Bereich mit zwei kleinen oder zwei Zeilen. Vorhandene Bereiche und ihre Reiter bleiben dabei erhalten. Mit Strg+Alt+1 bis 4 springst du direkt zu einem Bereich, mit Alt+Umschalt+Links zurück zum zuletzt aktiven, Strg+Alt+0 blendet die Bereichsnummern kurz ein und Strg+Alt+X tauscht den aktiven Bereich mit dem nächsten. Das zuletzt gewählte Layout wird beim Neustart wiederhergestellt (Einstellung „Tabs beim Start wiederherstellen"). Das gesamte Layout lässt sich außerdem als Arbeitsbereich speichern:

| Layout-Auswahl | Geteilte Ansicht |
| --- | --- |
| ![Layout-Auswahl mit fünf Kacheln: zwei Spalten, drei Spalten, 2×2, ein großer Bereich plus zwei, zwei Zeilen](bilder/layout-auswahl.png) | ![Menü Ansicht ▸ Geteilte Ansicht mit dem Untermenü Layout](bilder/menue-geteilte-ansicht.png) |
| ![Vier Bereiche im Raster mit verschiedenen Ordnern und sichtbarem Abstand](bilder/ansicht-vier-bereiche.png) | |
| ![Menü Datei ▸ Arbeitsbereiche](bilder/menue-arbeitsbereiche.png) | ![Arbeitsbereiche-Panel mit einem gespeicherten Arbeitsbereich](bilder/panel-arbeitsbereiche.png) |

**Gehe zu und Hilfe** – Verlauf, häufig verwendete Orte und Reiter im Menü „Gehe zu“, die Diagnose im Menü „Hilfe“:

| Gehe zu | Hilfe |
| --- | --- |
| ![Menü Gehe zu mit Häufig verwendet](bilder/menue-gehe-zu.png) | ![Menü Hilfe mit Diagnose-Untermenü](bilder/menue-hilfe.png) |

**Diagnose** – Protokolle, Systeminformationen mit gefundenen Werkzeugen, Plugin-Status und Fehlerbericht in einem Panel:

![Diagnose-Panel mit Systeminformationen und gefundenen Werkzeugen](bilder/panel-diagnose.png)

**GID-Projekte** – lokale Projektordner unten in der Seitenleiste (mit Git-Branch), rechts die `README.md` des gewählten Projekts im Aussehen von GitHub: Überschriften mit Linie, Tabellen, Aufgabenlisten, Zitate, Bilder, Codeblöcke und aufklappbare Abschnitte:

![Seitenleiste mit drei GID-Projekten und rechts die README des Projekts „webseite“ mit Tabelle, Aufgabenliste, Bild und Codeblock](bilder/gid-projekte.png)

| Gehe zu ▸ GID-Projekte | Kontextmenü eines Projekts |
| --- | --- |
| ![Menü Gehe zu mit dem Untermenü GID-Projekte und der Projektliste](bilder/menue-gid-projekte.png) | ![Kontextmenü eines GID-Projekts: README anzeigen, Im Dateimanager öffnen, Aus GID-Projekten entfernen, Projekt hinzufügen](bilder/gid-kontextmenue.png) |

---

[Weiter: Funktionen im Detail →](funktionen.md)
