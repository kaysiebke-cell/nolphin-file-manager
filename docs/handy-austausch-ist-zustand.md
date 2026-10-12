[← Handy-Austausch: Schnittstelle](handy-austausch-api.md) | [Startseite](../README.md)

# Handy-Austausch: Ist-Zustand und Abnahme

Stand: 11.10.2026. Alles in der Spalte „Beleg“ wurde geprüft; Nicht-Geprüftes ist als offen gekennzeichnet.

## Dateien und Funktionen (Linux)

| Aufgabe | Datei | Funktion / Stelle |
| ------- | ----- | ----------------- |
| QR-Code erzeugen | `src/nolphin-qr.c` | `nolphin_qr_encode ()` |
| Link und QR im Panel | `src/nolphin-phone.c` | URL `http://<Adresse>:<Port>/?t=<Schlüssel>&v=1&e=<Ablauf>` wird um Zeile 465 gebaut, `nolphin_qr_encode (url, …)` direkt danach |
| Schlüssel erzeugen | `src/nolphin-phone-server.c` | 128 Bit aus `/dev/urandom` (Zeile ~158); `nolphin_phone_server_rotate_token ()` widerruft |
| Server starten / stoppen | `src/nolphin-phone-server.c` | `nolphin_phone_server_start ()`, `nolphin_phone_server_stop ()` |
| Panel, Sitzung, Lebenszyklus | `src/nolphin-phone.c` | Sitzung in `~/.config/nolphin/phone-session` |
| Seitenleiste „Handy“ mit grünem Punkt | `src/nolphin-places-sidebar.c` | `name_cell_data_func ()`, `phone_status_tick ()`; Status aus `nolphin_phone_is_connected ()` |
| Tests | `test/test-nolphin-phone-server.c` | Ausbruchsversuche, Upload, Download, Ablauf, Ordneransicht |

## HTTP-Endpunkte (aus dem Dispatch in `nolphin-phone-server.c`)

| Methode | Pfad | Zweck |
| ------- | ---- | ----- |
| GET | `/` | Seite für den Handy-Browser |
| GET | `/info` | Name, Version der Schnittstelle, Ablauf, Größtgröße |
| GET | `/ls` | flache Dateiliste von `~/Handy` |
| GET | `/dir?r=&p=` | Ordner und Dateien eines Unterordners; `r=home` = persönlicher Ordner |
| GET | `/wait` | Wartepunkt der App (Long-Poll, 20 s) |
| GET | `/apk` | Handy-App zum Aktualisieren |
| GET | `/dl?r=&n=&p=` | Datei herunterladen, `p` optional (Unterordner), `r=home` = persönlicher Ordner |
| POST | `/up?n=` | Datei hochladen, Ziel `~/Handy/Vom Handy` |

Jede Anfrage braucht den Schlüssel `t`. Details in [handy-austausch-api.md](handy-austausch-api.md).

## Abnahme nach dem Anforderungstext

| Anforderung | Stand | Beleg |
| ----------- | ----- | ----- |
| QR-Code zeigt Verbindung, ohne Cloud | erfüllt | Panel „Austausch öffnen …“; lokales WLAN, nur private Netze |
| Zufälliger, kurzlebiger Schlüssel | erfüllt, einstellbar | 128 Bit urandom; Gültigkeit `phone-token-hours` (Vorgabe 12 h, derzeit auf 168 h gesetzt, auf Wunsch des Nutzers) |
| Verbindung trennen und wieder herstellen | erfüllt | „Austausch beenden“, „Neuen QR-Code erzeugen“, Sitzung wird gemerkt |
| Verbindungsstatus auf beiden Seiten | erfüllt | App: „Verbunden mit <PC>“; PC: grüner Punkt bei „Handy“ (gebaut und installiert, Anzeige am Bildschirm noch nicht bestätigt) |
| Handy → PC, mehrere Dateien | erfüllt | Foto-Raster und Dateiauswahl in der App; ein Upload per `curl` im Test erfolgreich |
| Große Dateien über Datenströme | erfüllt | 600 MB hoch und herunter, SHA-256 gleich, Server-Speicher blieb bei 84 MB |
| Dateinamen, Pfade, Größe prüfen | erfüllt | Tests mit `..`, `/etc`, Symlinks, versteckten Namen; Größtgröße 2 GiB |
| Kein Überschreiben | erfüllt, aber ohne Warnung | `publish_unique ()` legt `name (1).ext` an; eine Rückfrage gibt es nicht |
| Abbruch und Fehleranzeige | erfüllt | App-Knopf „Übertragung abbrechen“, Verlauf mit Fehlertext |
| PC → Handy | erfüllt | Rechtsklick „An Handy senden“ kopiert nach `~/Handy`; App lädt von selbst oder fragt nach |
| Speicherort auf dem Handy wählbar | erfüllt | Einstellungen ▸ Zielordner (auch SD-Karte), „Speichern unter“ |
| Fortschritt mit Geschwindigkeit und Restzeit | **offen** | App zeigt Dateiname und Prozent; Geschwindigkeit und Restzeit fehlen |
| Fortschritt auf dem PC | **offen** | Der PC sieht den Download-Fortschritt der App nicht |
| Statusmeldungen per WebSocket | **nicht vorhanden** | Long-Poll `/wait` statt WebSocket |
| HTTPS | **nicht vorhanden** | unverschlüsseltes HTTP im eigenen Netz, siehe Analyse |
| Firewall von Linux | **nicht geprüft** | |
| iOS | nicht vorgesehen | nur Android |

## Bekannte Probleme

- Der QR-Scanner der App stellt auf dem Xiaomi (23124RA7EO) nicht zuverlässig scharf. Ersatz: Link aus der Zwischenablage (Version 3.3).
- Die lange Gültigkeit des Schlüssels (7 Tage) widerspricht „kurzlebig“. Sie ist eine bewusste Einstellung.
