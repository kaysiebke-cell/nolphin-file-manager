# Git

Nolphin zeigt den Git-Zustand direkt an den Dateien und bietet die wichtigsten Befehle im Kontextmenü und in einem Panel. Voraussetzung ist ein installiertes `git`.

## Symbole an den Dateien

In einem Repository tragen Dateien kleine **Overlay-Symbole**:

| Symbol | Bedeutung |
| ------ | --------- |
| Häkchen | hinzugefügt (für den nächsten Commit vorgemerkt) |
| Ausrufezeichen | geändert |
| Plus | neu, noch nicht versioniert |
| eigenes Symbol | Konflikt |

![Dateien eines Git-Ordners mit Overlay-Symbolen](bilder/git-overlays.png)

Der Status wird im Hintergrund geholt und aktualisiert sich von selbst.

## Das Git-Menü

Rechtsklick auf eine Datei ▸ **Git**:

![Kontextmenü mit dem Git-Untermenü](bilder/git-menue.png)

| Eintrag | Wirkung |
| ------- | ------- |
| Status anzeigen | Liste der Änderungen im Panel |
| Hinzufügen | Datei für den Commit vormerken |
| Commit … | Änderungen mit Beschreibung speichern |
| Mit Server abgleichen | vergleicht Ihren Stand mit dem Server, ohne etwas zu ändern |
| Synchronisieren | holt neue Änderungen vom Server und lädt Ihre hoch |
| Pull / Push | herunterladen / hochladen |
| Log anzeigen / Diff anzeigen | Verlauf und Unterschiede |
| Repository klonen … | ein Repository (z. B. von GitHub) in den aktuellen Ordner laden |
| Remote hinzufügen … | Adresse des Servers eintragen oder ändern |

Liegt der Ort in keinem Repository, bietet Nolphin beim Aufruf an, eines anzulegen.

## Das Panel in drei Schritten

![Git-Panel mit den Schritten Änderungen, Speichern und Server](bilder/panel-git.png)

1. **Änderungen:** „Status anzeigen“ listet, was hinzugefügt, geändert oder unversioniert ist; „Hinzufügen“ merkt Dateien vor.
2. **Speichern:** eine kurze Beschreibung eintragen und **Speichern** drücken (das ist der Commit).
3. **Mit dem Server:** Synchronisieren, Abgleichen, Herunterladen, Hochladen; „Server eintragen …“ und „Repository klonen …“ richten die Verbindung ein.

Unter **Ansehen** zeigen „Verlauf“ und „Unterschiede“ die früheren Stände und die aktuellen Änderungen.

> **Hinweis:** Zugangsdaten verwaltet Git selbst (SSH-Schlüssel oder gespeicherte Anmeldung). Nolphin speichert keine Passwörter.
