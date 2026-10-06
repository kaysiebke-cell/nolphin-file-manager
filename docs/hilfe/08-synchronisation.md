# Ordner vergleichen und synchronisieren

Mit **Bearbeiten ▸ Werkzeuge ▸ Ordner vergleichen / synchronisieren …** gleichen Sie zwei Ordner ab, zum Beispiel einen Arbeitsordner mit einer Sicherung oder einem Netzlaufwerk.

![Synchronisations-Panel mit Neu, Geändert und Konflikt](bilder/panel-sync.png)

## So gehen Sie vor

1. Öffnen Sie den **Quellordner** in der Hauptansicht, bevor Sie das Werkzeug starten – er erscheint als „Lokaler Ordner“.
2. Wählen Sie über **Wählen …** den **Zielordner** (auch ein eingebundener Netzwerkordner).
3. Wählen Sie die Richtung: **Lokal → Ziel** oder **Ziel → Lokal**.
4. Drücken Sie **Vergleichen (Vorschau)**. Die Liste zeigt jeden Unterschied.
5. Haken Sie ab, was Sie nicht übernehmen wollen, und drücken Sie **Synchronisieren**.

## Die Liste

| Spalte | Bedeutung |
| ------ | --------- |
| Übernehmen | Häkchen = wird beim Synchronisieren übertragen |
| Datei | Pfad relativ zum Ordner |
| Art | **Neu** (nur in der Quelle), **Geändert** (unterschiedlich), **Konflikt** (das Ziel ist neuer als die Quelle) |
| Entscheidung | was bei einer Datei geschieht (siehe unten) |

Konflikte sind zunächst **nicht** angehakt, damit nichts Neueres im Ziel versehentlich überschrieben wird.

## Konflikte entscheiden

Bei einem Konflikt können Sie in der Spalte **Entscheidung** (Klick auf den Eintrag) wählen:

| Entscheidung | Wirkung |
| ------------ | ------- |
| Quelle behalten | die Datei im Ziel wird durch die Quelle ersetzt |
| Ziel behalten | die Datei im Ziel bleibt unverändert |
| Beide behalten | die Quelle wird übernommen, die bisherige Zielvariante bleibt als Kopie mit der Endung `.nolphin-ziel` erhalten |

## Zusätzliche Optionen

- **Inhalt prüfen (Prüfsumme, langsamer):** vergleicht nicht nur Größe und Zeit, sondern den Inhalt.

## Was Nolphin garantiert

- Es wird immer nur **in eine Richtung** abgeglichen.
- Im Ziel wird **nie etwas gelöscht**, auch wenn die Datei in der Quelle fehlt.
- Zuerst gibt es immer die Vorschau.

> **Voraussetzung:** Das Programm **rsync**. Fehlt es, meldet Nolphin das.

Ersetzte Zieldateien landen **nicht** im Papierkorb. Wählen Sie bei „Geändert“ und „Konflikt“ also bewusst, oder nutzen Sie „Beide behalten“, wenn Sie sichergehen wollen.
