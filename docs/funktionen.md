[Startseite](../README.md) | [← Der rechte Arbeitsbereich](arbeitsbereich.md) | [Einstellungen und Anpassung →](einstellungen.md)

# Funktionen im Detail

## Dateiverwaltung

Tabs, Split View (F3), Symbol-, Listen- und Kompaktansicht, Kopieren und Verschieben mit Pause und Abbruch, Rückgängig und Wiederholen, Papierkorb, Drag & Drop, Berechtigungen, Netzwerkzugriff über GVfs (SMB, NFS, HTTP, HTTPS).

## Archive

| Erstellen und Entpacken | Nur Entpacken und Auslesen (über `7z`) |
| ----------------------- | -------------------------------------- |
| ZIP, TAR, TAR.GZ, TAR.BZ2, TAR.XZ, TAR.ZST, TAR.LZ4, 7Z | CAB, ARJ, LZH, ISO, CPIO, RPM, DEB |
| Einzeldateien: XZ, ZST, LZ4 | |

Ist das benötigte Werkzeug nicht installiert, wird das erkannt und angezeigt. Der **Archiv-Manager** liest den Inhalt eines Archivs aus und ändert es gezielt (hinzufügen, ersetzen, entfernen); er arbeitet mit denselben Systemwerkzeugen.

## Vorschau und Metadaten

* **PDF:** Titel, Autor, Seitenzahl, Seitengröße und PDF-Version über `pdfinfo`, Vorschaubild über `pdftocairo` (Paket `poppler-utils`).
* **Audio und Video:** Dauer, Container, Codecs, Auflösung, Bildrate, Kanäle und Abtastrate über `gst-discoverer-1.0` (Paket `gstreamer1.0-tools`).
* **Bewertung, Tags, Kommentar:** werden als Datei-Metadaten über GIO gespeichert und im Info-Panel angezeigt.

Fehlt eines der Werkzeuge, steht im Panel ein Hinweis, statt Werte vorzutäuschen.

## Git

Der Git-Status jeder Datei erscheint als Overlay-Symbol im Ordner. Das Panel führt durch die Schritte „Änderungen“, „Speichern“ und „Mit dem Server“ und zeigt Verlauf und Unterschiede.

## Werkzeuge

* **Ordner vergleichen und synchronisieren:** vergleicht zwei Ordner über `rsync -n` und zeigt, was neu, geändert oder in Konflikt ist. Abgeglichen wird in **eine** Richtung, im Ziel wird **nie etwas gelöscht**.
* **Regeln anwenden:** Bedingungen (Dateityp, Name mit Platzhaltern, Größe, Änderungsdatum) und eine Aktion; zuerst die Vorschau, dann „Anwenden“. Regeln werden derzeit nicht dauerhaft gespeichert.
* **Duplikate finden:** nach Name, Größe oder Inhalt (Prüfsumme) in einem Ordner samt Unterordnern; gelöscht wird nur in den Papierkorb und nach Bestätigung.
* **Versionen:** Nolphin sichert Versionen einer Datei in `~/.local/share/nolphin/versions`; wiederherstellen, löschen und – bei Textdateien – mit der aktuellen Datei vergleichen.

## Suche

Die Dateinamensuche versteht neben Platzhaltern (`*`, `?`) und regulären Ausdrücken (Schalter `.*`) diese Operatoren:

| Eingabe | Bedeutung |
| ------- | --------- |
| `jahr bericht` | UND: beide Begriffe im Namen (Leerzeichen) |
| `foto ODER bericht` | ODER (auch `OR` oder `\|`); UND bindet stärker als ODER |
| `bericht NICHT entwurf` | NICHT (auch `NOT` oder `-entwurf`) |
| `"mein bericht.txt"` | exakt: der ganze Name muss übereinstimmen |

Die Schlüsselwörter gelten unabhängig von der Groß-/Kleinschreibung; ein Begriff, der selbst „nicht“ oder „oder“ heißt, lässt sich deshalb nur in Anführungszeichen suchen.

## Fenster, Reiter und Orte

* **Geteilte Ansicht:** bis zu vier Bereiche, Aufteilung waagerecht oder senkrecht, Bereiche duplizieren, maximieren und schließen.
* **Arbeitsbereiche:** speichern Reiter, Teilung, Fenstergröße und Panels unter einem Namen; Laden, Duplizieren und Löschen im Panel. Beim Beenden merkt sich Nolphin die letzte Sitzung.
* **Gehe zu:** Verlauf, häufig verwendete Orte und Reiter; „Zwischenablage als Datei einfügen“ im Bearbeiten-Menü.

## Handy-Austausch

* **Einstieg:** Klick auf **Handy** in der Seitenleiste unter „Mein Rechner“ oder Datei ▸ Handy-Austausch. Nolphin startet den Austausch, zeigt den **QR-Code** im Arbeitsbereich und öffnet den Ordner `~/Handy` in der Hauptansicht. Es gibt nichts einzustellen.
* **Handy → PC:** Das Handy scannt den Code und lädt Bilder und Dateien im Browser hoch. Sie landen in `~/Handy/Vom Handy`. Eine App ist nicht nötig; es gibt keine Fremdabhängigkeit.
* **PC → Handy:** Dateien auf **Handy** in der Seitenleiste ziehen oder Rechtsklick ▸ **An Handy senden**. Beides kopiert nach `~/Handy/Zum Handy`; das Handy lädt sie auf der Seite im Browser.
* **Handy-App (Android):** Der erste Scan des QR-Codes bietet die App zum Installieren an, jeder weitere Scan verbindet sie. Sie lädt neue Dateien aus „Zum Handy“ von selbst nach `Download/Nolphin` und sendet Dateien über das Teilen-Menü an den PC. Reines Android-Framework, keine Bibliotheken, Quelltext in `android/`.
* **Sicherheit:** Schlüssel im Link, nur Verbindungen aus privaten Netzen, nie überschreiben, Selbstabschaltung nach 15 Minuten ohne Anfrage. Die Verbindung ist nicht verschlüsselt, nur im eigenen WLAN verwenden.

## GID-Projekte

* **Projektliste:** Der Bereich „GID-Projekte“ sitzt unter Orte bzw. Ordnerbaum und lässt sich über Ansicht ▸ Seitenleiste ▸ „GID-Projekte anzeigen“ ein- und ausblenden. Jedes Projekt ist ein lokaler Ordner, bei einem Git-Repository steht der aktuelle Branch daneben (aus `.git/HEAD` gelesen, ohne `git`).
* **Hinzufügen und Entfernen:** „Projekt hinzufügen …“ in der Seitenleiste, Gehe zu ▸ GID-Projekte oder im Kontextmenü eines Ordners „Als GID-Projekt hinzufügen“. Entfernen nimmt nur den Eintrag aus der Liste und löscht nie Dateien. Ein nicht mehr vorhandener Ordner wird markiert, nicht gelöscht.
* **README im Arbeitsbereich:** Ein Klick auf ein Projekt zeigt dessen `README.md` (auch `README.markdown`) als gerenderten, markier- und kopierbaren Text. Fehlt die README, steht dort ein klarer Hinweis. Links ins Projekt öffnen den Ordner in der Hauptansicht, Web-Links den Browser.
* **Darstellung wie bei GitHub:** Überschriften mit Linie, Tabellen (Ausrichtung, Streifen, bei Bedarf waagerecht scrollbar), Aufgabenlisten mit Kästchen, Zitate, Code, Trennlinien, Fett, Kursiv, Durchgestrichen. Die Farben kommen aus dem aktiven GTK-Theme, hell wie dunkel.
* **HTML wie auf GitHub:** `<p>`, `<div>` und `<center>` mit Ausrichtung, `<h1>` bis `<h6>`, `<details>` mit `<summary>` (zugeklappt, per Klick aufklappbar), `<kbd>`, `<sub>`, `<sup>`, `<br>` und `<img>` mit Breite.
* **Bilder nur lokal:** Bilder aus dem Projektordner werden angezeigt und an die Panelbreite angepasst. Bilder von Webseiten lädt Nolphin bewusst nie, sie erscheinen als Alternativtext. Es gibt keinen Netzwerkzugriff.

## Diagnose

Unter Hilfe ▸ Diagnose zeigt Nolphin das Protokoll, Systeminformationen mit den gefundenen Hilfswerkzeugen, den Status der geladenen Plugins und erstellt einen Fehlerbericht.

## DEB-Paket erstellen

Der Ersteller (`src/nolphin-deb-builder.c`) schreibt das Paket aus `debian-binary`, `control.tar.gz` und `data.tar.gz` direkt als `ar`-Archiv. Pflichtfelder sind Paketname, Version, Architektur, Maintainer und Beschreibung. Sektion, Priorität, Abhängigkeiten und Homepage sind optional.

## CAD- und 3D-Dateien

* **STL:** ASCII oder binär, bei Binär-STL die Anzahl der Dreiecke
* **STEP/STP:** Header-Informationen (Beschreibung, Dateiname, Zeitstempel, Autor, Schema)
* **FreeCAD `.FCStd`:** Dokumentinformationen und eingebettetes Vorschaubild
* **IGES, OBJ, 3MF, DXF, DWG:** Erkennung; gibt es für ein Format noch keine echte Vorschau, steht das ausdrücklich da

## Prüfsummen, Verschlüsselung, ACL

* **Prüfsummen:** MD5, SHA-1, SHA-256, SHA-512, BLAKE2
* **Verschlüsselung:** GPG
* **ACL:** Verwaltung über `getfacl` und `setfacl`

## Papierkorb und Auswahlen

Der Papierkorb wird nach einer einstellbaren Aufbewahrungsdauer bereinigt, bei Überschreiten eines Größenlimits erscheint eine Warnung. Benannte Dateiauswahlen lassen sich speichern und wiederherstellen.

---

[Weiter: Einstellungen und Anpassung →](einstellungen.md)
