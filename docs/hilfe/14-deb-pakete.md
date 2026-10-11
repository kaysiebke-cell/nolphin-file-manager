# DEB-Pakete erstellen

Mit dem DEB-Ersteller verpacken Sie eigene Dateien als installierbares Debian-Paket (`.deb`) – ohne zusätzliche Werkzeuge. Nolphin schreibt das Paket selbst, ohne `dpkg-deb`.

**Hilfe ▸ .deb-Paket erstellen …** öffnet das Panel.

![Der DEB-Ersteller mit ausgefüllten Eingabefeldern](bilder/panel-deb-ersteller.png)

## Pflichtangaben

| Feld | Beispiel |
| ---- | -------- |
| Name des Programms | `demo-tool` |
| Versionsnummer | `1.0.0` |
| Kurzbeschreibung | Ein kleines Beispielprogramm |
| Ersteller (Name) | Max Mustermann |
| Speichern in | Ordner, in dem die `.deb`-Datei entstehen soll |

Unter **Weitere Angaben** stehen Optionen wie Sektion, Priorität, Abhängigkeiten und Homepage.

## Inhalt des Pakets

Mit **Datei hinzufügen** und **Ordner hinzufügen** wählen Sie, was ins Paket kommt. Die Liste zeigt je Eintrag:

- **Datei / Ordner:** die Quelle auf Ihrem Rechner
- **Installieren nach:** der Zielpfad auf dem fremden System (z. B. `/usr/bin/demo-tool`)
- **Startbar:** ob die Datei ausführbar sein soll

„Entfernen“ nimmt einen Eintrag wieder heraus. **DEB erstellen** schreibt das Paket.

> **Hinweis:** Das Paket enthält nur, was Sie ausdrücklich hinzufügen. Prüfen Sie es vor der Weitergabe, am besten auf einem Testsystem.
