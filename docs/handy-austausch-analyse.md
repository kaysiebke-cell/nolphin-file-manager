[Startseite](../README.md) | [Handy-Austausch: Schnittstelle →](handy-austausch-api.md)

# Handy-Austausch: Analyse, Referenzprojekte und Architektur

Stand: 11. Oktober 2026. Dieses Dokument beantwortet die Phasen 1 bis 4 des Entwicklungsplans „Nolphin + Android-App“. Was geprüft wurde und was nicht, steht jeweils dabei.

## Phase 1: Ist-Zustand (aus dem Quellcode)

### Dateien und Funktionen

| Aufgabe | Datei | Funktion / Stelle |
| ------- | ----- | ----------------- |
| QR-Code erzeugen | `src/nolphin-qr.c` | `nolphin_qr_encode ()` (Byte-Modus, Fehlerkorrektur M, Version 1 bis 6) |
| QR-Code zeichnen | `src/nolphin-phone.c` | `on_qr_draw ()` (immer schwarz auf weiß) |
| Verbindungslink bauen | `src/nolphin-phone.c` | `phone_refresh_url ()`: `http://<Adresse>:<Port>/?t=<Schlüssel>&v=1&e=<Ablauf>` |
| Schlüssel erzeugen | `src/nolphin-phone-server.c` | `make_token ()`: 16 Zufallsbytes aus `/dev/urandom`, als 32 Hex-Zeichen |
| Schlüssel prüfen | `src/nolphin-phone-server.c` | `check_token ()` (Vergleich ohne frühen Abbruch, Ablaufzeit) |
| Schlüssel wechseln (Widerruf) | `src/nolphin-phone-server.c` | `nolphin_phone_server_rotate_token ()`; Knopf in `src/nolphin-phone.c` (`on_rotate_clicked ()`) |
| HTTP-Server starten und beenden | `src/nolphin-phone-server.c` | `nolphin_phone_server_start ()`, `nolphin_phone_server_stop ()` |
| Anfragen verteilen | `src/nolphin-phone-server.c` | `on_run ()` (Arbeitsthreads über `GThreadedSocketService`) |
| Port | `src/nolphin-phone-server.c` | vom System gewählt (`g_inet_socket_address_new (inet, 0)`), bei jedem Start anders; kein fester Port |
| Netzwerkadresse wählen | `src/nolphin-phone.c` | `local_addresses ()` (private IPv4-Adressen aktiver, nicht virtueller Schnittstellen) |
| Start, Ende, Selbstabschaltung | `src/nolphin-phone.c` | `phone_start ()`, `phone_stop ()`, `on_idle_check ()` (15 Minuten ohne Anfrage) |
| Panel im Arbeitsbereich | `src/nolphin-phone.c` | `nolphin_phone_page_new ()`; Einstieg in `src/nolphin-places-sidebar.c` und `src/nolphin-window-menus.c` |
| Android-App | `android/` | siehe unten |

### Zugriffsschutz und Grenzen im Server

Alle Wege außer dem Verbindungsaufbau selbst brauchen den Schlüssel (`t`). Es werden nur Verbindungen aus Loopback-, privaten und Link-Local-Netzen angenommen (`peer_is_local ()`). Kopfzeilen höchstens 16 KiB, 30 s Zeitgrenze je Verbindung, höchstens 8 gleichzeitige Arbeitsthreads, Upload höchstens 2 GiB, Listen höchstens 500 Einträge, ein Schlüssel höchstens 12 Stunden.

### Was funktioniert und wie es geprüft wurde

| Funktion | Stand | Prüfung |
| -------- | ----- | ------- |
| Server mit Schlüssel, Upload, Download, Liste, Wartepunkt, App-Datei | arbeitet | Meson-Test `test-nolphin-phone-server` (Zugriffsschutz, Pfadtricks, Umlaute, 3 MiB, Abbruch, Ablauf, Widerruf, `/info`) |
| QR-Code erzeugen | arbeitet | Abgleich mit der Python-Bibliothek `qrcode` für Version 1, 3, 4, 5, 6 und alle 8 Masken (Matrix identisch) |
| QR-Code in der App lesen | arbeitet im Test | `android/test.sh`: 154 Prüfungen, darunter ein echtes dunkles, unscharfes Handyfoto des Codes. **Live-Kamera am echten Handy: vom Benutzer gemeldet „funktioniert nicht“, daraufhin wurde eine Diagnosezeile eingebaut; Ergebnis steht aus.** |
| Verbinden, Dateien in beide Richtungen, Sonderzeichen, 60 MB, Abbruch, Verbindungsverlust, Widerruf | arbeitet im Emulator | Android-Emulator (Android 14) gegen den echten Server, Protokoll in „Tests“ unten |
| Linux → Android am echten Handy | **nicht geprüft** | Nur der Weg Handy → PC (Browser-Upload) wurde vom Benutzer am echten Handy bestätigt |
| HTTPS | fehlt | siehe Sicherheit |
| Bestätigung durch den Empfänger | teilweise | am Handy abschaltbar („Vor dem Laden vom PC fragen“); am PC nur auf Sitzungsebene (Start und QR-Code) |
| Übertragungsprüfsumme (sha256) | fehlt | |
| Mehrere Geräte gleichzeitig | nicht vorgesehen | ein Schlüssel für die Sitzung, mehrere Handys könnten ihn verwenden |

## Phase 2: Referenzprojekte

Gelesen wurden die jeweiligen Projektseiten auf GitHub und, wo genannt, die Dateibäume über die GitHub-Schnittstelle. **Quelltexte einzelner Dateien wurden nicht gelesen.** Aussagen über Inhalte beruhen auf der Protokolldokumentation oder den Projektbeschreibungen, nicht auf Code. Nichts wurde übernommen.

| | LocalSend | Warpinator (Linux) und Warpinator Android | PairDrop |
| - | --------- | ----------------------------------------- | -------- |
| Lizenz | Apache-2.0 | GPL-3.0 (beide) | GPL-3.0 |
| Technik | Flutter/Dart, dazu Rust; REST über HTTPS | Python mit gRPC und Protobuf; Android in Java | Web-App (JavaScript), WebRTC und WebSocket, Node.js-Server |
| Verbindung | Multicast-UDP (`224.0.0.167`, Port 53317) oder HTTP-Suche im Netz | Zeroconf/mDNS, Gruppencode, TCP 42000/42001 | Gerätepaarung per QR-Code oder 6-stelligem Code, öffentliche Räume mit 5-stelligem Code |
| Sicherheit | selbstsigniertes Zertifikat je Gerät, Fingerabdruck, optionale PIN | gemeinsamer Gruppencode | gemeinsame Geheimnisse bei Paarung |
| Bestätigung | Empfänger nimmt über `prepare-upload` an, teilweise an oder lehnt ab | manuelle Annahme, solange der Standard-Gruppencode gilt | Übertragung beginnt erst, wenn der Empfänger annimmt |
| Schnittstelle | `/api/localsend/v2/`: `register`, `prepare-upload`, `upload`, `cancel`, `prepare-download`, `download`, `info` | gRPC, Protokolldatei `app/src/main/proto/warp.proto` (Android) | WebSocket-Signalisierung, Übertragung per WebRTC |
| Fehlercodes | 204, 400, 401, 403, 409, 422, 429, 500 (je Weg dokumentiert) | nicht gelesen | nicht gelesen |
| Android | Flutter-App, `app/android/app/src/main/AndroidManifest.xml` | `app/src/main/java/slowscript/warpinator/`: `MainService`, `GrpcService`, `Remote`, `Transfer`, `ShareActivity`, `Authenticator`, `CertServer` u. a. | Nur Web-Oberfläche im Browser |
| Server-Quellen (Dateinamen aus dem Dateibaum) | `app/lib/provider/network/server/controller/receive_controller.dart`, `send_controller.dart`, `server_provider.dart` | `src/` (Python) | `server/server.js`, `server/ws-server.js`, `server/peer.js` |
| QR / Pairing | – | – | `public/scripts/libs/qr-code.min.js`, `network.js` |

### Was als Vorbild taugt und was nicht

- **LocalSend, Ablauf mit Bestätigung** (`prepare-upload`, danach `upload` je Datei, `cancel`): Als Vorbild für eine spätere Bestätigung am PC pro Übertragung. Noch nicht umgesetzt.
- **LocalSend, Fehlercodes** (403 bei falschem Schlüssel, 422 bei falscher Prüfsumme): Die Idee, Schlüsselfehler und Fehler der Datei zu trennen, ist übernommen (403 mit Hinweis `expired`).
- **LocalSend, Info-Weg** (`/info`): Vorbild für `GET /info` in unserer Schnittstelle (zur Prüfung, dass wirklich Nolphin antwortet).
- **LocalSend, Protokoll als Ganzes:** nicht übernommen. Es verlangt HTTPS mit Zertifikat, Gerätesuche per Multicast und Flutter-Datenmodelle. Unsere App ist ohne Bibliotheken gebaut; das Protokoll würde viel Code ohne Mehrwert für „Handy ↔ eigener Rechner“ bringen. Außerdem sollen Nolphin und die App keine Fremdabhängigkeit haben.
- **Warpinator:** gRPC, Protobuf und Zeroconf sind Bibliotheken, die hier nicht verwendet werden sollen. Als Vorbild bleibt der Aufbau der Android-App (Vordergrunddienst, Teilen-Eintrag `ShareActivity`) und die Idee einer Gruppenkennung.
- **PairDrop:** Der QR-Code als Paarungsweg bestätigt unseren Ablauf. WebRTC und TURN sind für das lokale Netz unnötig.

### Lizenzen und Kompatibilität

Nolphin steht unter der GPL (Fork von Nemo). Apache-2.0 (LocalSend) und GPL-3.0 sind mit der GPL-Linie kompatibel. **Es wurde kein Fremdcode übernommen**, es gibt also keine Lizenzpflichten außer der Namensnennung bei einer späteren Übernahme. Die Lizenzangaben stammen von den GitHub-Projektseiten (für LocalSend zusätzlich über die GitHub-Lizenzabfrage bestätigt).

## Phase 3 und 4: Schnittstelle und Datenformat

Die vorhandene Schnittstelle genügt für Dateiübertragungen in beide Richtungen und wurde **erweitert, nicht ersetzt**. Die vollständige Beschreibung steht in [Handy-Austausch: Schnittstelle](handy-austausch-api.md). Neu in Version 1 der Schnittstelle:

| Erweiterung | Grund |
| ----------- | ----- |
| `GET /info` | Die App prüft vor dem Speichern, dass unter der Adresse wirklich Nolphin läuft, der Schlüssel gilt und die Schnittstellenversion passt. |
| Ablaufzeit des Schlüssels (12 Stunden) | Zeitliche Begrenzung. |
| Schlüsselwechsel (`rotate_token`, Knopf „Neuen QR-Code erzeugen“) | Widerruf: Der alte Schlüssel ist sofort ungültig. |
| QR-Format Version 1 mit `v` und `e` | Versionierung und Ablaufzeit im Code. |
| `403` mit dem Text „Der Link ist abgelaufen.“ | Die App kann „abgelaufen“ von „falscher Schlüssel“ unterscheiden. Alle Meldungen des Servers sind deutsch (Spezifikation, Abschnitt 49). |

### Warum der QR-Code ein Link und kein JSON ist

Der Plan nennt als Beispiel ein JSON-Objekt. Der QR-Code enthält stattdessen einen **Link**, weil die Kamera des Handys einen Link öffnen kann: Beim ersten Scan landet man so auf der Seite von Nolphin, die die App zum Installieren anbietet. Ein JSON-Text würde von der Kamera nur als Text angezeigt. Die Felder sind dieselben wie im Vorschlag: Adresse, Port, Schlüssel, Ablaufzeit, dazu die Version.

```
http://192.168.178.20:43913/?t=4d6d1633ec8ca225aaeb5ddfbc5a1b6d&v=1&e=1791769130
```

## Verbindungsablauf

### A. App ist installiert

1. In Nolphin auf **Handy** klicken: Der Austausch startet, der QR-Code erscheint.
2. In der App **QR-Code scannen** (eigener QR-Leser) oder mit der Kamera des Handys scannen. Im zweiten Fall öffnet die Seite die App über einen `intent://`-Link.
3. Die App prüft den Link (nur private IPv4-Adressen, Port, Schlüsselform, Version, Ablauf) und fragt `GET /info` am PC ab. **Erst wenn das gelingt, wird die Verbindung gespeichert.** Jeder Fehler hat eine eigene Meldung.
4. Der Hintergrunddienst hält die Verbindung (`GET /wait`) und lädt neue Dateien aus dem Ordner „Handy“. Dateien vom Handy gehen über das Teilen-Menü oder die Dateiauswahl.

### B. App ist noch nicht installiert

1. Der erste Scan mit der Kamera öffnet die Seite von Nolphin im Browser des Handys.
2. Die Seite versucht, die App zu öffnen. Fehlt sie, bietet sie **App herunterladen** an. Die APK kommt direkt von Nolphin (`GET /apk`), nicht von einer öffentlichen Adresse.
3. Nach der Installation scannt man den Code in der App oder noch einmal mit der Kamera.

**Was dieser Ablauf nicht kann:** Er installiert die App nicht von selbst. Android erlaubt das nicht. Es erscheinen Warnungen von Chrome, Google Play Protect und je nach Hersteller weiteren Schutzprogrammen, weil die App nicht aus einem Store kommt (auf dem Handy des Benutzers wurden die Warnungen von Chrome, Play Protect und Mi Protect beobachtet). Eine lokale Adresse wie `192.168.x.x` löst keine App-Installation aus.

**Android App Links** (`https://…` öffnet die App ohne Rückfrage) würden eine **öffentlich erreichbare HTTPS-Domain** mit einer Datei `/.well-known/assetlinks.json` voraussetzen. Das gibt es hier nicht und es wird keine erfundene Domain verwendet. Deshalb nutzt die App ein eigenes Schema (`nolphin://connect`) und den `intent://`-Weg mit Rückfalladresse. Ein Store-Eintrag (Play Store, F-Droid) wäre der saubere Weg zur Installation, ist aber nicht Teil dieses Projekts.

## Sicherheit

| Gefahr | Gegenmaßnahme | Rest |
| ------ | ------------- | ---- |
| Fremde im Netz verbinden sich | 128-Bit-Schlüssel aus `/dev/urandom`, nur Verbindungen aus privaten Netzen, nur während der Austausch läuft | Wer den Link kennt, hat Zugriff |
| Alter Link bleibt gültig | Ablauf nach 12 Stunden, Schlüsselwechsel auf Knopfdruck, Beenden löscht ihn, Selbstabschaltung nach 15 Minuten ohne Anfrage | |
| Link wird abgehört (WLAN mitlesen) | **keine**: HTTP ist unverschlüsselt | Jeder im selben WLAN, der den Datenverkehr mitschneidet, sieht Schlüssel und Dateien. Nur im eigenen, vertrauenswürdigen WLAN verwenden |
| Pfadtricks im Dateinamen | Namen werden bereinigt, Downloads nur normaler Dateien im Ausgangsordner, keine Symlinks, keine versteckten Dateien | |
| Zu große oder abgebrochene Uploads | Grenze 2 GiB, Temporärdatei, bei Abbruch gelöscht | |
| Falscher Rechner | Die App prüft über `/info`, dass Nolphin antwortet, und akzeptiert nur private Adressen | |
| Dateien ohne Zustimmung | Zustimmung auf Sitzungsebene (der Nutzer startet den Austausch und zeigt den Code), am Handy zusätzlich abschaltbar „Vor dem Laden fragen“ | Keine Bestätigung pro Datei am PC |

### Mögliche Härtung: HTTPS (noch nicht umgesetzt, braucht eine Entscheidung)

Nolphin (GIO) könnte ein selbstsigniertes Zertifikat erzeugen (`GTlsCertificate`) und den Fingerabdruck im QR-Code mitgeben (`&f=<sha256>`). Die App würde nur dieses Zertifikat annehmen (eigener `TrustManager`, Android-Framework). Beides geht ohne Bibliothek. Der Preis: Die Seite im Browser des Handys zeigt eine Zertifikatswarnung (selbstsigniert), und der erste Scan mit der normalen Kamera landet dort. Der Weg mit der App wäre verschlüsselt, der Weg über die Seite nicht mehr ohne Warnung.

## Tests (echte Ergebnisse)

| Test | Ergebnis |
| ---- | -------- |
| `meson test` (26 Tests, darunter Handy-Austausch mit `/info`, Widerruf, Ablauf) | alle bestanden |
| `android/test.sh`: QR-Leser (154 Prüfungen) | alle bestanden |
| `android/test.sh`: Linkprüfung (34 Prüfungen: gültige Links, öffentliche Adressen, Hostnamen, Ports, Schlüsselform, Version, Ablauf, fremde Schemata) | alle bestanden |
| Emulator: Link mit falschem Schlüssel, öffentlicher Adresse, nicht erreichbarem PC | je eine eigene Meldung, nichts gespeichert |
| Emulator: Verbinden, vier Dateien PC → Handy mit Umlauten, Leerzeichen, `#`, `&`, `%`, japanischen Zeichen | alle angekommen, Namen erhalten |
| Emulator: dieselben Dateien Handy → PC | alle angekommen, byte-identisch |
| Emulator: 60 MB in beide Richtungen | identisch; Java-Speicher der App bleibt bei rund 30 MB (kein Laden in den Arbeitsspeicher) |
| Emulator: Abbruch durch den Nutzer bei 13 % einer 300-MB-Datei | „Laden abgebrochen“, keine Teildatei, kein erneutes Laden von selbst |
| Emulator: Server beendet mitten in einer 700-MB-Übertragung | „Laden fehlgeschlagen“, Teildatei gelöscht, „PC nicht erreichbar, neuer Versuch …“ |
| Emulator: Schlüsselwechsel am PC | App meldet „Schlüssel abgelaufen: QR-Code am PC neu scannen“; neu verbinden klappt innerhalb weniger Sekunden |
| Emulator: „Vor dem Laden fragen“ | Datei wird nicht geladen, Benachrichtigung „Datei vom PC bereit“ |

**Nicht geprüft:** Live-Kamera am echten Handy, Linux → Android am echten Handy, die Aktion „Laden“ in der Benachrichtigung (nur das Erscheinen der Benachrichtigung), Verhalten im Standby und in anderen Android-Versionen als 14, Installation der APK über Chrome auf dem Emulator.

## Offene Entscheidungen

1. **HTTPS mit angeheftetem Zertifikat** einbauen? (siehe oben)
2. **Bestätigung pro Übertragung am PC** (nach dem Vorbild von LocalSends `prepare-upload`) einbauen? Heute genügt der Start des Austauschs.
3. **Prüfsumme** (sha256) für Übertragungen einbauen?
