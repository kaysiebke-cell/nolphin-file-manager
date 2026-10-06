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

Dieselbe Vorschau lässt sich auch den Auswahldialogen **anderer GTK-Programme** hinzufügen, etwa Chrome beim Hochladen. Dazu gehört das Zusatzmodul `nolphin-preview-module`, das mit dem Paket kommt. Es ist zunächst **aus**. Auf **Cinnamon (Linux Mint)** schalten Sie es für Ihren Benutzer mit dieser Einstellung ein:

```
gsettings set org.cinnamon.settings-daemon.plugins.xsettings enabled-gtk-modules "['nolphin-preview-module']"
```

Die Einstellung gilt für Programme, die **danach neu gestartet** werden (Chrome, Editoren …). Der Dateiauswahl-Dienst des Systems (`xdg-desktop-portal-gtk`) wird beim Abmelden und Anmelden neu gestartet, dann zeigen auch Dialoge, die er für andere Programme öffnet, die Vorschau. Enthält die Liste schon andere Module, tragen Sie `nolphin-preview-module` zusätzlich ein, statt sie zu ersetzen. Zum Ausschalten setzen Sie sie auf den vorherigen Wert zurück (meist `[]`):

```
gsettings reset org.cinnamon.settings-daemon.plugins.xsettings enabled-gtk-modules
```

Auf anderen Desktops mit GTK 3 genügt die Zeile `gtk-modules=nolphin-preview-module` im Abschnitt `[Settings]` von `~/.config/gtk-3.0/settings.ini`; unter Cinnamon überstimmt die Desktop-Einstellung sie.

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
