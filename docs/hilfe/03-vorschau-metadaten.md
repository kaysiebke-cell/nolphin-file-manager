# Vorschau und Metadaten

Wählen Sie eine Datei, zeigt der Arbeitsbereich ihre Vorschau und die wichtigsten Angaben. Welche Angaben erscheinen, hängt vom Dateityp ab.

## PDF

Titel, Autor, Seitenzahl, Seitengröße und PDF-Version, dazu ein Bild der ersten Seite.

![Vorschau einer PDF-Datei mit Titel, Autor, Seiten und Seitengröße](bilder/vorschau-pdf.png)

> **Voraussetzung:** Das Paket **poppler-utils** (`pdfinfo`, `pdftocairo`). Fehlt es, steht im Panel ein Hinweis.

## Video und Audio

Dauer, Container, Video- und Audiocodec, Auflösung, Bildrate, Kanäle und Abtastrate.

![Vorschau eines Videos mit Dauer, Codec, Auflösung und Bildrate](bilder/vorschau-video.png)

> **Voraussetzung:** Das Paket **gstreamer1.0-tools** (`gst-discoverer-1.0`).

## Weitere Typen

- **Bilder:** Vorschaubild
- **Textdateien:** die ersten Zeilen
- **Ordner:** Typ, Änderungsdatum, Rechte, Eigentümer, Ort
- **CAD und 3D:** STL (ASCII/binär, Anzahl der Dreiecke), STEP/STP (Kopfdaten), FreeCAD-Dateien (Dokumentangaben und eingebettetes Vorschaubild); IGES, OBJ, 3MF, DXF und DWG werden erkannt – gibt es für ein Format noch keine Vorschau, steht das ausdrücklich da.

## Bewertung, Tags und Kommentar

Am Ende des Info-Panels können Sie jede Datei **bewerten** (ein bis fünf Sterne), mit **Tags** versehen und **kommentieren**.

![Info-Panel mit vier Sternen, Tags und Kommentar](bilder/info-bewertung.png)

| Feld | Bedienung |
| ---- | --------- |
| Bewertung | auf einen Stern klicken; ein zweiter Klick auf denselben Stern entfernt die Bewertung |
| Tags | mehrere Begriffe mit Komma trennen, z. B. `Urlaub, Wald, 2026` |
| Kommentar | frei formulierter Text; gespeichert wird beim Verlassen des Feldes |

Die Angaben werden als Datei-Metadaten gespeichert (GVfs) und bleiben an der Datei, solange sie auf demselben Rechner liegt.
