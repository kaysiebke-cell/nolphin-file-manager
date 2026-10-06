# Diagnose und Fehlerbericht

Wenn etwas nicht wie erwartet funktioniert, hilft **Hilfe ▸ Diagnose**.

![Das Menü Hilfe mit dem Diagnose-Untermenü](bilder/menue-hilfe.png)

| Eintrag | Zeigt |
| ------- | ----- |
| Protokolle anzeigen | die letzten Zeilen des lokalen Protokolls |
| Fehlerbericht erstellen … | schreibt einen Bericht in eine Datei |
| Systeminformationen | Version, System, verfügbare Dienste und gefundene Werkzeuge |
| Plugin-Status | welche Erweiterungen geladen wurden |

Alle vier öffnen dasselbe Panel auf je einem eigenen Reiter.

![Das Diagnose-Panel mit den Systeminformationen](bilder/panel-diagnose.png)

## Systeminformationen

Der Reiter nennt:

- **Programm:** Nolphin- und GTK-Version
- **System:** Betriebssystem, Desktop, Sprache, Theme
- **GVfs-Dienste:** welche Adressarten (z. B. `sftp`, `smb`, `dav`) Ihr System kennt
- **Gefundene Werkzeuge:** `zip`, `7z`, `rsync`, `git`, `gpg`, `pdfinfo` … mit Pfad – oder „fehlt“

Fehlt ein Werkzeug, erklärt das, warum eine Funktion einen Hinweis zeigt. Mit **In die Zwischenablage kopieren** übernehmen Sie den Text.

## Protokoll

Nolphin schreibt Meldungen lokal nach `~/.local/share/nolphin/logs/nolphin.log`. Das Panel zeigt die letzten Zeilen.

## Fehlerbericht

Der Fehlerbericht ist eine **lokale Datei** mit Versionen, Systeminformationen und dem Protokollende. Nolphin **sendet nichts automatisch**; Sie entscheiden, wem Sie die Datei geben.

> **Tipp:** Ein hilfreicher Bericht nennt zusätzlich, was Sie getan haben, was Sie erwartet haben und was stattdessen passiert ist.
