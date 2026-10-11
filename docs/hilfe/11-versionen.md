# Versionen einer Datei

Mit der Versionsverwaltung sichern Sie den Stand einer einzelnen Datei und holen ihn später zurück – ohne ein Git-Repository.

## Version speichern

1. Datei markieren.
2. **Bearbeiten ▸ Werkzeuge ▸ Versionen ▸ Version speichern**.

Eine kurze Meldung bestätigt das Speichern. Die Sicherung liegt unter `~/.local/share/nolphin/versions`. Ordner und entfernte Dateien lassen sich nicht sichern – das meldet Nolphin.

## Versionen anzeigen

**Versionen ▸ Versionen anzeigen …** öffnet das Panel:

![Versionen-Panel mit zwei gespeicherten Versionen und den Unterschieden](bilder/panel-versionen.png)

| Schaltfläche | Wirkung |
| ------------ | ------- |
| Version speichern | legt einen neuen Stand an; im Feld darüber lässt sich ein **Kommentar** eintragen |
| Wiederherstellen | ersetzt die aktuelle Datei durch die gewählte Version |
| Unterschiede | zeigt bei Textdateien, was sich gegenüber dem aktuellen Stand geändert hat |
| Löschen | entfernt die gewählte Version |

In der **Unterschiede**-Ansicht stehen entfernte Zeilen mit `-`, hinzugekommene mit `+`.

> **Tipp:** Speichern Sie eine Version, bevor Sie eine Datei umfangreich ändern. Soll der Verlauf ganzer Ordner nachvollziehbar sein, nutzen Sie stattdessen Git.
