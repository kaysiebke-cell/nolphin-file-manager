# Regeln anwenden

Eine Regel sagt: **Wenn** Dateien bestimmte Bedingungen erfüllen, **dann** soll eine Aktion folgen. Sie öffnen sie mit **Bearbeiten ▸ Werkzeuge ▸ Regeln anwenden …** und wenden sie auf einen von Ihnen gewählten Ordner an.

![Regeln-Panel mit Bedingung, Aktion und Vorschau](bilder/panel-regeln.png)

## Wenn …

| Bedingung | Eingabe |
| --------- | ------- |
| Dateityp | Beliebig, Bilder, Videos, Audio, Text, Archive oder Dokumente |
| Name | mit Platzhaltern, z. B. `*.jpg` oder `rechnung*` |
| Größe | „größer als“ oder „kleiner als“, in KiB |
| Änderung | „älter als“ oder „neuer als“, in Tagen |

Alle ausgefüllten Bedingungen müssen **gleichzeitig** zutreffen. Leere Felder zählen nicht.

## … dann

Wählen Sie die Aktion:

| Aktion | Wirkung |
| ------ | ------- |
| Verschieben nach … | verschiebt die Dateien in den Zielordner |
| Kopieren nach … | kopiert sie dorthin |
| In den Papierkorb verschieben | legt sie in den Papierkorb |
| Befehl ausführen | startet einen Befehl pro Datei, `%f` steht für die Datei, z. B. `gzip %f` |

Den Ort für Verschieben und Kopieren wählen Sie über **Zielordner …**. Befehle laufen **ohne Shell**, einzeln pro Datei.

## Ablauf

1. Ordner wählen („Ordner wählen …“); „Unterordner einbeziehen“ schließt tiefere Ebenen ein.
2. Bedingungen und Aktion einstellen.
3. **Vorschau**: die Liste nennt die betroffenen Dateien samt Größe. Haken entfernen, was nicht dazugehören soll.
4. **Anwenden**.

> **Wichtig:** Regeln werden **nur auf Ihren Wunsch** angewendet, nie automatisch im Hintergrund. Sie werden außerdem nicht dauerhaft gespeichert.

## Stapelverarbeitung

**Bearbeiten ▸ Werkzeuge ▸ Stapelverarbeitung …** ist dasselbe Panel, wirkt aber nicht auf einen ganzen Ordner, sondern auf die **aktuelle Auswahl**. So bearbeiten Sie gezielt eine Handvoll Dateien, etwa mit `gzip %f`.

## Beispiel

*Alle `*.bak`-Dateien eines Ordners in einen Sicherungsordner verschieben:*

- Name: `*.bak`
- dann: Verschieben nach … ▸ `~/Sicherung`
- Vorschau prüfen, dann Anwenden.
