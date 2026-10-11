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

## Vorschau in der Dateiauswahl

Auch in Nolphins eigenen Auswahldialogen (z. B. beim Hinzufügen von Dateien zu einem Archiv oder Paket, bei Vorlagen und beim Importieren von Einstellungen) erscheint **rechts eine Vorschau** der markierten Datei: bei Bildern und vorhandenen Vorschaubildern (PDF, Video …) das Bild, bei Textdateien die ersten Zeilen, sonst Symbol, Typ, Größe und Änderungsdatum.

### In anderen Programmen (zum Beispiel beim Hochladen)

Dieselbe Vorschau lässt sich auch den Auswahldialogen **anderer GTK-Programme** hinzufügen, etwa Chrome beim Hochladen. Dazu gehört das Zusatzmodul `nolphin-preview-module`, das mit dem Paket kommt. Es ist zunächst **aus**. Unter **Linux Mint** (und anderen Systemen mit X11-Sitzung) schalten Sie es für Ihren Benutzer ein, indem Sie die Umgebungsvariable `GTK3_MODULES` in der Datei `~/.xsessionrc` setzen. Das ist derselbe Mechanismus, über den Mint das xapp-Modul in alle GTK-Programme lädt:

```
echo 'export GTK3_MODULES="nolphin-preview-module${GTK3_MODULES:+:$GTK3_MODULES}"' >> ~/.xsessionrc
```

Danach **einmal abmelden und wieder anmelden**. Erst dann starten alle Programme, auch der Dateiauswahl-Dienst des Systems, mit dem Modul. Hat die Datei schon eine Zeile mit `GTK3_MODULES`, ergänzen Sie `nolphin-preview-module` dort (Module werden mit Doppelpunkt getrennt), statt eine zweite anzuhängen.

Zum Ausschalten löschen Sie die Zeile aus `~/.xsessionrc` und melden sich neu an.

> **Hinweis zu Cinnamon:** Die Einstellungen `gtk-modules` in `~/.config/gtk-3.0/settings.ini` und `enabled-gtk-modules` in den Cinnamon-Einstellungen wirken hier **nicht**; die Umgebungsvariable ist der Weg, der funktioniert.

> **Hinweis:** Nur Dialoge zum **Öffnen von Dateien** bekommen die Vorschau; Ordner- und Speichern-Dialoge bleiben unverändert, ebenso Dialoge, die schon eine eigene Vorschau haben. Programme, die nicht den GTK-3-Dialog nutzen (zum Beispiel solche mit eigener Oberfläche), sind nicht betroffen.

## Bewertung, Tags und Kommentar

Am Ende des Info-Panels können Sie jede Datei **bewerten** (ein bis fünf Sterne), mit **Tags** versehen und **kommentieren**.

![Info-Panel mit vier Sternen, Tags und Kommentar](bilder/info-bewertung.png)

| Feld | Bedienung |
| ---- | --------- |
| Bewertung | auf einen Stern klicken; ein zweiter Klick auf denselben Stern entfernt die Bewertung |
| Tags | mehrere Begriffe mit Komma trennen, z. B. `Urlaub, Wald, 2026` |
| Kommentar | frei formulierter Text; gespeichert wird beim Verlassen des Feldes |

Die Angaben werden als Datei-Metadaten gespeichert (GVfs) und bleiben an der Datei, solange sie auf demselben Rechner liegt.
