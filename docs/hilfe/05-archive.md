# Archive

## Archiv erstellen

Markieren Sie Dateien oder Ordner und wählen Sie im Kontextmenü **Komprimieren …**. Das Panel fragt nach Name, Format und Zielort.

![Archiv-Panel mit Dateiname, Format und Zielort](bilder/panel-archiv.png)

| Format | Erstellen | Entpacken |
| ------ | :-------: | :-------: |
| ZIP, 7Z | ✓ | ✓ |
| TAR, TAR.GZ, TAR.BZ2, TAR.XZ, TAR.ZST, TAR.LZ4 | ✓ | ✓ |
| Einzeldateien: XZ, ZST, LZ4 | ✓ | ✓ |
| CAB, ARJ, LZH, ISO, CPIO, RPM, DEB | – | ✓ (über `7z`) |

Unter **Erweiterte Optionen** stellen Sie ein **Passwort** und **Teilarchive** ein.

## Archiv entpacken

Im Kontextmenü eines Archivs:

- **Hier entpacken** – in einen neuen Ordner neben dem Archiv
- **Entpacken nach …** – in einen frei gewählten Ordner
- **Archiv prüfen** – testet, ob das Archiv unbeschädigt ist

![Kontextmenü einer ZIP-Datei mit den Archiv-Einträgen](bilder/kontextmenue-archiv.png)

## Archiv-Manager

**Archiv öffnen …** zeigt den Inhalt eines Archivs im Panel, ohne es zu entpacken.

![Archiv-Manager mit der Inhaltsliste einer ZIP-Datei](bilder/panel-archiv-manager.png)

| Schaltfläche | Wirkung |
| ------------ | ------- |
| Entpacken … | markierte Einträge in einen Ordner entpacken |
| Dateien hinzufügen … | Dateien ins Archiv aufnehmen |
| Ordner hinzufügen … | einen Ordner samt Inhalt aufnehmen |
| Ersetzen … | einen Eintrag durch eine andere Datei ersetzen |
| Entfernen | markierte Einträge aus dem Archiv löschen |
| Prüfen | Archiv auf Fehler testen |
| Neu laden | Inhaltsliste aktualisieren |

> **Hinweis:** Bei Formaten wie TAR.GZ muss das Archiv zum Ändern neu gepackt werden; das geschieht automatisch. Bei großen Archiven kann es entsprechend dauern.

## Wenn etwas fehlt

Nolphin nutzt die installierten Programme (`zip`, `unzip`, `tar`, `7z`, `zstd`, `lz4`, `xz`). Ist eines nicht vorhanden, steht das im Panel; unter Hilfe ▸ Diagnose ▸ Systeminformationen sehen Sie die Liste aller gefundenen Werkzeuge. Argumente werden nie über eine Shell übergeben, auch Dateinamen mit Sonderzeichen sind sicher.
