[← Handy-Austausch: Analyse](handy-austausch-analyse.md) | [Startseite](../README.md)

# Handy-Austausch: Schnittstelle (Version 1)

Diese Schnittstelle bietet Nolphin am PC an, und die Android-App verwendet sie. Der Browser des Handys nutzt dieselben Wege. Quelle der Wahrheit ist `src/nolphin-phone-server.c`.

## Grundlagen

- HTTP/1.1, **unverschlüsselt**, nur im eigenen Netz (siehe [Sicherheit](handy-austausch-analyse.md#sicherheit)).
- Der Server bindet an **eine** IPv4-Adresse des PCs (privates Netz). Der Port wird bei jedem Start vom System gewählt.
- Jede Anfrage braucht den Schlüssel als Abfrageparameter `t`. Ohne gültigen Schlüssel antwortet der Server mit `403`.
- Jede Antwort schließt die Verbindung (`Connection: close`). Antworten haben `Cache-Control: no-store` und `X-Content-Type-Options: nosniff`.
- Nur Verbindungen aus Loopback-, privaten und Link-Local-Netzen werden bearbeitet.
- Anfragen mit `Transfer-Encoding: chunked` werden bei `POST` mit `501` abgelehnt; Uploads brauchen `Content-Length`.

## Verbindungslink und QR-Code, Format Version 1

```
http://<IPv4>:<Port>/?t=<Schlüssel>&v=1&e=<Ablauf>
nolphin://connect?host=<IPv4>:<Port>&t=<Schlüssel>&v=1&e=<Ablauf>
```

| Feld | Bedeutung | Regeln |
| ---- | --------- | ------ |
| `<IPv4>` | Adresse des PCs | genau vier Zahlen 0 bis 255; privat (`10/8`, `172.16/12`, `192.168/16`), Link-Local (`169.254/16`) oder Loopback (`127/8`); Hostnamen und IPv6 werden abgelehnt |
| `<Port>` | Port des Servers | 1 bis 65535 |
| `t` | Schlüssel | genau 32 Zeichen `0-9a-f` (128 Bit aus `/dev/urandom`) |
| `v` | Version des Links | `1`; andere Werte lehnt die App ab. Fehlt `v`, gilt Version 1 (ältere Nolphin-Versionen) |
| `e` | Ablaufzeit | Unix-Zeit in Sekunden; die App lehnt Links ab, die um mehr als 10 Minuten abgelaufen sind (Toleranz für Uhren). Fehlt `e`, wird nicht geprüft |

Der Server erzwingt den Ablauf unabhängig davon (siehe `403`). Der Link darf höchstens 300 Zeichen lang sein. Der erste Weg (`http://`) steht im QR-Code, damit die Kamera ihn als Link öffnet; der zweite (`nolphin://`) erzeugt die Seite im Browser für den Sprung in die App.

## Lebensdauer des Schlüssels

- Ein Schlüssel gilt höchstens **12 Stunden** (`TOKEN_LIFETIME_DEFAULT`). Für Tests lässt sich das mit der Umgebungsvariable `NOLPHIN_PHONE_TOKEN_LIFETIME` (Sekunden) verkürzen.
- **Widerruf:** „Neuen QR-Code erzeugen“ im Panel (oder `nolphin_phone_server_rotate_token ()`) erzeugt einen neuen Schlüssel mit neuer Ablaufzeit. Der alte ist sofort ungültig.
- **Merken über einen Neustart:** Adresse, Port und Schlüssel stehen in `~/.config/nolphin/phone-session` (nur für den Benutzer lesbar), solange der Schlüssel gilt. Startet Nolphin den Austausch erneut, nimmt es sie wieder auf; das Handy verbindet sich dann von selbst, ohne neuen QR-Code. Ist der Port belegt, die Adresse eine andere oder der Schlüssel abgelaufen, entsteht ein neuer Schlüssel.
- **Beenden** (Knopf oder Menü) und „Neuen QR-Code erzeugen“ widerrufen die gemerkte Sitzung; die Datei wird gelöscht beziehungsweise ersetzt. Schließen von Nolphin oder die Selbstabschaltung widerrufen sie nicht.
- **Selbstabschaltung:** 15 Minuten ohne gültige Anfrage beenden den Server. Eine verbundene App hält ihn durch `GET /wait` am Leben.

## Wege

### `GET /info?t=…`

Prüft Schlüssel und Gegenstelle.

```json
{"app":"nolphin","api":1,"name":"kaysiebke-ThinkPad","expires":1791769130,"maxUpload":2147483648,"share":true}
```

`api` ist die Version der Schnittstelle (hier 1), `expires` die Ablaufzeit des Schlüssels, `maxUpload` die größte erlaubte Datei in Bytes, `share` sagt, ob ein Ausgangsordner (`~/Handy`) vorhanden ist, `folders`, ob `/up` das Feld `d` (Unterordner) versteht.

### `GET /ls?t=…`

Dateien im Ausgangsordner `~/Handy` (nur Dateien, keine Unterordner) als JSON-Liste (höchstens 500, ohne versteckte Dateien, ohne Verknüpfungen, ohne Dateien, die in den letzten 3 Sekunden geändert wurden):

```json
[{"n":"Größe Übung ä ö ü ß.jpg","s":4096},{"n":"Bericht (Entwurf) #2 & Co 100%.pdf","s":9000}]
```

`n` ist der Dateiname (UTF-8), `s` die Größe in Bytes.

### `GET /wait?t=…&v=<Stand>`

Wartepunkt für die App. Antwortet, sobald sich die Liste gegenüber dem Stand `v` ändert, **spätestens nach 20 Sekunden**. Ohne `v` antwortet er sofort.

```json
{"v":"<md5 der Liste>","files":[{"n":"…","s":123}]}
```

Die App schickt den zuletzt erhaltenen `v` mit. Jede Anfrage zählt als Aktivität (gegen die Selbstabschaltung).

### `GET /dl?t=…&n=<Name>`

Lädt eine Datei aus dem Ausgangsordner. `n` ist der Dateiname, prozentkodiert (UTF-8). Antwort `200` mit `Content-Length`, `Content-Type: application/octet-stream` und `Content-Disposition: attachment`. Der Name muss **genau** so im Ordner stehen (keine Pfade, keine Änderung durch die Bereinigung, nicht mit Punkt beginnend), sonst `404`. Verknüpfungen und Nicht-Dateien liefern `404`. Der Inhalt wird in Stücken von 64 KiB gesendet, nicht im Ganzen geladen.

### `GET /dir?t=…&r=home&p=<Unterordner>` und `GET /dl?t=…&r=home&p=<Unterordner>&n=<Name>` (persönlicher Ordner)

Mit `r=home` gelten `/dir` und `/dl` für den **persönlichen Ordner** (`~`, für Tests `NOLPHIN_PHONE_HOME`) statt für „Zum Handy“, nur lesend. Ohne `r` bleibt alles wie bisher. Jeder andere Wert von `r` liefert `404`. Es gelten dieselben Schutzregeln wie für den Ausgangsordner: Schlüssel, privates Netz, nur Namen ohne `..` und ohne führenden Punkt (versteckte Dateien und Ordner wie `.ssh` sind weder sichtbar noch ladbar), nur echte Ordner und normale Dateien, keine Symbolischen Verknüpfungen (`O_NOFOLLOW`). `GET /info` meldet die Fähigkeit mit `"home":true`; Apps prüfen das, weil ältere Versionen `r` ignorieren würden.

### `POST /up?t=…&n=<Name>[&d=<Ordner>]`

Lädt eine Datei in den Eingangsordner „Vom Handy“ hoch. Der Körper sind die Rohdaten der Datei, `Content-Length` ist Pflicht.

Der Name wird bereinigt: nur der letzte Pfadteil (`/` und `\` trennen), Steuerzeichen und Richtungsumkehr-Zeichen entfernt, führende Punkte und Leerzeichen entfernt, höchstens 200 Bytes (an einer Zeichengrenze gekürzt), leer wird zu „Datei“. Die Datei wird zuerst in eine Temporärdatei geschrieben und dann unter dem Namen abgelegt. **Es wird nie überschrieben**: Gibt es den Namen, wird `Name (1).ext`, `Name (2).ext` usw. verwendet. Bei Abbruch wird die Temporärdatei gelöscht.

**Ordner (`d`, optional):** Mit `d=Ordner1/Unter` kommt die Datei in diesen Unterordner des Eingangsordners. Die Ordner werden bei Bedarf angelegt, bestehende werden weiterverwendet (Dateien darin werden nie überschrieben, siehe oben). `/` trennt die Ebenen (höchstens 32). Jede Ebene muss ein unveränderter, bereinigter Name sein und darf nicht mit einem Punkt beginnen. Sonst gibt es `400` und es wird nichts abgelegt. Ordner, die als Link oder Datei vorhanden sind, werden nicht benutzt. Ob der PC das kann, steht in `/info` als `"folders":true`; fehlt das Feld, schickt die App einen Ordner als ZIP.

Antwort bei Erfolg: `200` mit `{"ok":true,"name":"<endgültiger Name>"}`.

### `GET /apk?t=…`

Liefert die Handy-App (`application/vnd.android.package-archive`), falls sie installiert ist. Suchreihenfolge: Umgebungsvariable `NOLPHIN_PHONE_APK`, `~/.local/share/nolphin/phone/nolphin-handy.apk`, `<datadir>/nolphin/phone/`, die Systemdatenverzeichnisse. Sonst `404`.

### `GET /?t=…`

Die Seite für den Browser des Handys (Hochladen, Liste zum Herunterladen, App-Abschnitt). Mit Content-Security-Policy, ohne externe Quellen.

## Fehlercodes

| Code | Wo | Bedeutung |
| ---- | -- | --------- |
| 400 | alle | kaputte Anfrage, Kopfzeilen über 16 KiB |
| 403 | alle | Schlüssel fehlt, falsch oder widerrufen (Text „Der Link ist ungültig oder wurde widerrufen.“) oder **abgelaufen** (Text „Der Link ist abgelaufen.“) |
| 404 | `/dl`, `/apk`, sonstige | Datei nicht vorhanden oder nicht erlaubt, unbekannter Weg |
| 400 | `/up` | ungültiger Ordnername in `d` |
| 411 | `/up` | `Content-Length` fehlt |
| 413 | `/up` | Datei größer als 2 GiB |
| 500 | `/up` | Schreibfehler, Datei kann nicht abgelegt werden |
| 501 | `/up` | `Transfer-Encoding: chunked` wird nicht unterstützt |
| 507 | `/up` | kein Speicherplatz am PC |

Bricht die Verbindung mitten im Upload ab, antwortet der Server nicht mehr (es gibt niemanden mehr); die Teildatei wird entfernt.

## Grenzen im Server

Kopfzeilen 16 KiB, 30 s Zeitgrenze je Verbindung, 8 gleichzeitige Arbeitsthreads (ein `GET /wait` belegt einen davon bis zu 20 s), Upload 2 GiB, Liste 500 Einträge.

## Verhalten der App

- **Verbinden:** Link prüfen (siehe Format), `GET /info` aufrufen, bei Erfolg speichern. Antwortet eine ältere Nolphin-Version mit `404` auf `/info`, prüft die App stattdessen `GET /ls`. Meldungen: Schlüssel abgelaufen, Schlüssel abgelehnt (Neustart oder neuer Code), keine Verbindung, kein Nolphin, andere Version der Schnittstelle, Adresse nicht privat.
- **PC → Handy:** Hintergrunddienst mit `GET /wait`. Neue Dateien (Name und Größe noch nicht bekannt) werden mit `GET /dl` nach `Download/Nolphin` geladen, in Stücken von 64 KiB, mit Fortschritt und Abbruch. Beim allerersten Verbinden werden vorhandene Dateien nicht geladen. Wird abgebrochen, lädt die App die Datei nicht von selbst erneut.
- **Handy → PC:** Teilen-Menü („Nolphin (PC)“) oder Dateiauswahl in der App. Die Datei wird zunächst im Zwischenspeicher der App abgelegt (so ist die Länge immer genau), dann mit `POST /up` und fester Länge gesendet, in Stücken von 64 KiB, mit Fortschritt und Abbruch.
- **Aktualisieren:** Die App holt im Hintergrund `GET /apk` (höchstens alle 30 Minuten) und vergleicht die Versionsnummer der Datei mit der eigenen. Ist sie höher, zeigt die Startseite „Neue Version“ und „Aktualisieren“; die Installation läuft über den Paketinstaller von Android (gleiche Signatur, Daten und Verbindung bleiben). Beim ersten Mal fragt Android nach der Erlaubnis „Apps installieren“ (`REQUEST_INSTALL_PACKAGES`).
- **Schlüssel ungültig (`403`) beim Warten:** Statusmeldung „Schlüssel abgelaufen: QR-Code am PC neu scannen“, die App versucht es weiter.
- **Verbindung weg:** Statusmeldung „PC nicht erreichbar, neuer Versuch …“, alle fünf Sekunden ein neuer Versuch; Teildateien werden gelöscht.

## Beispiele

```bash
URL='http://192.168.178.20:43913'; T='4d6d1633ec8ca225aaeb5ddfbc5a1b6d'
curl "$URL/info?t=$T"                                   # Prüfen
curl "$URL/ls?t=$T"                                     # Liste
curl -o foto.jpg "$URL/dl?t=$T&n=foto.jpg"              # Laden
curl -X POST --data-binary @foto.jpg "$URL/up?t=$T&n=foto.jpg"   # Senden
```
