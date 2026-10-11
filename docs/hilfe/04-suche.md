# Suche

**Strg+F** öffnet die Suche im Arbeitsbereich. Tippen Sie einen Dateinamen und bestätigen Sie mit **Eingabe**; die Treffer erscheinen in der Hauptansicht, samt Speicherort und Änderungsdatum.

## Die Suchleiste

| Element | Bedeutung |
| ------- | --------- |
| Dateiname | Text, der im Namen vorkommen soll |
| **Aa** | Groß-/Kleinschreibung beachten |
| **.\*** | den Text als regulären Ausdruck lesen |
| Inhalt | Text, der **in** der Datei vorkommen soll (nur lokale Dateien) |
| Rekursiv-Schalter (Pfeil) | in Unterordnern mitsuchen |

Die Suche beginnt im aktuell geöffneten Ordner.

## Operatoren im Dateinamen

Im Namensfeld können Sie Begriffe verknüpfen:

| Eingabe | Bedeutung |
| ------- | --------- |
| `jahr bericht` | **UND**: beide Begriffe kommen im Namen vor (Leerzeichen) |
| `foto ODER bericht` | **ODER** (auch `OR` oder `\|`) |
| `bericht NICHT entwurf` | **NICHT** (auch `NOT` oder ein Minus: `bericht -entwurf`) |
| `"mein bericht.txt"` | **exakt**: der ganze Name muss genau so lauten |
| `*.txt` | **Platzhalter**: `*` für beliebig viele, `?` für genau ein Zeichen |

UND bindet stärker als ODER: `foto ODER bericht NICHT entwurf` bedeutet „foto“ **oder** („bericht“ und nicht „entwurf“).

> **Hinweis:** Die Schlüsselwörter gelten unabhängig von der Schreibweise. Suchen Sie nach einer Datei, die selbst „nicht“ oder „oder“ heißt, setzen Sie den Namen in Anführungszeichen.

### Beispiele

- `rechnung 2026` – Namen mit „rechnung“ **und** „2026“
- `jpg ODER png` – Bilder in beiden Formaten
- `bericht -entwurf -alt` – Berichte, die weder „entwurf“ noch „alt“ enthalten
- `"README"` – nur eine Datei, die genau so heißt

## Reguläre Ausdrücke

Mit dem Schalter **.\*** gilt der Text als regulärer Ausdruck, zum Beispiel `^IMG_\d{4}\.jpg$`. Ist der Ausdruck ungültig, meldet Nolphin das, statt eine leere Trefferliste zu zeigen.

## Gespeicherte Auswahlen

Wollen Sie nicht suchen, sondern eine Auswahl wiederverwenden: Bearbeiten ▸ **Auswahl speichern …** merkt sich die markierten Dateien eines Ordners, **Gespeicherte Auswahl wiederherstellen …** markiert sie wieder.

## Noch nicht möglich

Die Suche nach Besitzer, Gruppe und Berechtigungen sowie in Tags und Kommentaren ist noch nicht umgesetzt.
