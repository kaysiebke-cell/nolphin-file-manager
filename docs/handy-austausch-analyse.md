[Startseite](../README.md) | [Handy-Austausch: Schnittstelle →](handy-austausch-api.md)

# Handy-Austausch: Analyse, Referenzprojekte und Architektur

Stand: 11. Oktober 2026. Dieses Dokument beantwortet die Phasen 1 bis 4 des Entwicklungsplans „Nolphin + Android-App“. Was geprüft wurde und was nicht, steht jeweils dabei.

## Phase 1: Ist-Zustand (aus dem Quellcode)

### Dateien und Funktionen

| Aufgabe | Datei | Funktion / Stelle |
| ------- | ----- | ----------------- |
| QR-Code erzeugen | `src/nolphin-qr.c` | `nolphin_qr_encode ()` (Byte-Modus, Fehlerkorrektur M, Version 1 bis 6) |
| QR-Code zeichnen | `src/nolphin-phone.c` | `on_qr_draw ()` (immer schwarz auf weiß) |
| Verbindungslink bauen | `src/nolphin-phone.c` | `phone_refresh_url ()`: `nolphin://<Adresse>:<Port>/?t=<Schlüssel>&v=2&f=<Fingerabdruck>` |
| Zertifikat erzeugen, Fingerabdruck | `src/nolphin-phone-tls.c` | `nolphin_phone_tls_generate ()` (ruft `openssl req -x509` mit festen Argumenten auf, privater Temporärordner, danach gelöscht), `nolphin_phone_tls_fingerprint ()` |
| Schlüssel erzeugen | `src/nolphin-phone-server.c` | `make_token ()`: 16 Zufallsbytes aus `/dev/urandom`, als 32 Hex-Zeichen |
| Schlüssel prüfen | `src/nolphin-phone-server.c` | `check_token ()` (Vergleich ohne frühen Abbruch, Ablaufzeit) |
| Schlüssel wechseln (Widerruf) | `src/nolphin-phone-server.c` | `nolphin_phone_server_rotate_token ()`; Knopf in `src/nolphin-phone.c` (`on_rotate_clicked ()`) |
| HTTPS-Server starten und beenden | `src/nolphin-phone-server.c` | `nolphin_phone_server_start ()`, `nolphin_phone_server_stop ()` |
| Anfragen verteilen | `src/nolphin-phone-server.c` | `on_run ()` (Arbeitsthreads über `GThreadedSocketService`; jede Verbindung wird mit `g_tls_server_connection_new ()` verschlüsselt) |
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
| QR-Format Version 2 mit `v` und `f` (Fingerabdruck) | Versionierung und festes Zertifikat. Die Ablaufzeit `e` steht nicht mehr im Link (QR-Größe), der Server erzwingt sie. |
| `403` mit dem Text „Der Link ist abgelaufen.“ | Die App kann „abgelaufen“ von „falscher Schlüssel“ unterscheiden. Alle Meldungen des Servers sind deutsch (Spezifikation, Abschnitt 49). |

### Warum der QR-Code ein Link und kein JSON ist

Der Plan nennt als Beispiel ein JSON-Objekt. Der QR-Code enthält stattdessen einen **Link** mit dem Schema `nolphin://`: Er ist kurz (der QR-Encoder fasst höchstens 106 Zeichen, der längste Link hat 96), lässt sich als Text kopieren und einfügen, und **kein Browser** öffnet ihn. Der Fingerabdruck `f` bindet die App an genau das Zertifikat dieses Austauschs.

```
nolphin://192.168.1.10:40000/?t=0123456789abcdef0123456789abcdef&v=2&f=QULDtffIw8Ed6DmETVJ7zA
```

(Alle Werte sind erfunden. Der erste Entwurf nutzte `http://` und eine Seite im Browser des Handys; die ist mit der Verschlüsselung entfallen.)

## Verbindungsablauf

1. Die App muss auf dem Handy installiert sein. Die **Erstinstallation** geht nicht mehr über den Browser des Handys, sondern von Hand (zum Beispiel per USB oder über eine übertragene Datei). Android erlaubt keine stille Installation und warnt, weil die App nicht aus einem Store kommt. Spätere Aktualisierungen holt die App selbst über die verschlüsselte Verbindung (`GET /apk`).
2. In Nolphin auf **Handy** klicken: Der Austausch startet (Zertifikat und Schlüssel entstehen), der QR-Code erscheint.
3. In der App **„QR-Code scannen und verbinden“** tippen und den Code mit der Kamera der App scannen. Der eigene QR-Leser verbindet sofort. Alternativ den Link aus der Zwischenablage einfügen (die App bietet einen frisch kopierten Link von selbst an). Scanner-Apps anderer Hersteller geben das Ergebnis nicht an die App zurück (am Xiaomi-Scanner geprüft).
4. Die App prüft den Link (nur private IPv4-Adressen, Port, Schlüsselform, Version 2, Fingerabdruck) und fragt `GET /info` über TLS ab, **nur mit dem Zertifikat dieses Fingerabdrucks**. **Erst wenn das gelingt, wird die Verbindung gespeichert.** Jeder Fehler hat eine eigene Meldung.
5. Der Hintergrunddienst hält die Verbindung (`GET /wait`) und lädt neue Dateien aus dem Ordner „Handy“. Dateien vom Handy gehen über das Teilen-Menü oder die Dateiauswahl.

**Android App Links** (`https://…` öffnet die App ohne Rückfrage) würden eine **öffentlich erreichbare HTTPS-Domain** mit einer Datei `/.well-known/assetlinks.json` voraussetzen. Das gibt es hier nicht, und es wird keine erfundene Domain verwendet.

## Sicherheit

| Gefahr | Gegenmaßnahme | Rest |
| ------ | ------------- | ---- |
| Fremde im Netz verbinden sich | 128-Bit-Schlüssel aus `/dev/urandom`, nur Verbindungen aus privaten Netzen, nur während der Austausch läuft, nur über TLS. Eine Sperre nach falschen Schlüsseln gibt es bewusst nicht (Raten ist aussichtslos, und sie könnte das eigene Handy aussperren) | Wer den Link kennt, hat Zugriff |
| Alter Link bleibt gültig | Ablauf nach 12 Stunden, Schlüsselwechsel auf Knopfdruck, Beenden löscht ihn, Selbstabschaltung nach 15 Minuten ohne Anfrage | |
| Verbindung wird abgehört (WLAN mitlesen) | **TLS 1.3/1.2 mit selbst erstelltem Zertifikat**, pro Schlüssel neu; Klartext bekommt keine Antwort | Wer den **QR-Code sieht** (Bildschirm, Foto), hat den Schlüssel und den Fingerabdruck. Den Code nur dem eigenen Handy zeigen und danach „Beenden“ |
| Fremder Rechner gibt sich als der PC aus (Man-in-the-Middle) | Die App akzeptiert **nur das Zertifikat mit dem Fingerabdruck aus dem QR-Code** (Pinning), keine Zertifizierungsstelle, kein Namensvergleich | Ein Angreifer, der den QR-Code vor dem Handy sieht, kennt auch den Fingerabdruck |
| Fremder Link öffnet die App und verbindet mit einem fremden Rechner | Links von außen (Kamera-App, Browser, andere App) verbinden erst nach Bestätigung mit Anzeige der Adresse | Der Nutzer muss die Adresse mit dem PC vergleichen |
| Die App wird unverschlüsselt ausgeliefert | `/apk` gibt es nur noch über TLS; Android prüft bei Aktualisierungen die Signatur | Die Erstinstallation passiert von Hand und liegt beim Nutzer |
| Pfadtricks im Dateinamen | Namen werden bereinigt, Downloads nur normaler Dateien im Ausgangsordner, keine Symlinks, keine versteckten Dateien | |
| Zu große oder abgebrochene Uploads | Grenze 2 GiB, Temporärdatei, bei Abbruch gelöscht | |
| Falscher Rechner | Die App prüft über `/info`, dass Nolphin antwortet, und akzeptiert nur private Adressen und das Zertifikat mit dem Fingerabdruck | |
| Dateien ohne Zustimmung | Zustimmung auf Sitzungsebene (der Nutzer startet den Austausch und zeigt den Code), am Handy zusätzlich abschaltbar „Vor dem Laden fragen“ | Keine Bestätigung pro Datei am PC |

### Umgesetzt: HTTPS mit festem Zertifikat (12./13. Oktober 2026)

- Nolphin erzeugt bei jedem neuen Schlüssel ein **selbstsigniertes Zertifikat** (EC P-256, gültig 3 Tage, mit `openssl`, das als Laufzeit-Voraussetzung freigegeben ist, siehe [Abhängigkeiten](DEPENDENCIES.md)). Der private Schlüssel entsteht in einem privaten Temporärordner (0700) und wird sofort gelöscht; er lebt im Speicher des Servers und in `~/.config/nolphin/phone-session` (0600).
- Der Server spricht nur TLS (`GTlsServerConnection`). Fehlt `openssl` oder `glib-networking`, startet der Austausch nicht; es gibt **keinen Rückfall auf Klartext**.
- Der Fingerabdruck (die ersten 16 Bytes von SHA-256 über das Zertifikat, 22 Zeichen base64url) steht im QR-Code. Die App prüft ihn mit einem eigenen `X509TrustManager` (Android-Framework, ohne Bibliothek) und lehnt alles andere ab. Die App verbietet Klartext auch auf Systemebene.
- Die **Webseite für den Browser des Handys ist entfallen** (ein Browser kann den Fingerabdruck nicht prüfen und würde eine Warnung zeigen).
- **Grenzen:** Das Zertifikat schützt den Weg, nicht den QR-Code: Wer ihn sieht oder fotografiert, hat alles, bis er widerrufen wird (Beenden oder „Neuen QR-Code erzeugen“). Wer am PC selbst Zugriff hat, wird dadurch nicht aufgehalten. Die Erstinstallation der App erfolgt von Hand.

## Tests (echte Ergebnisse)

### Verschlüsselung (12. und 13. Oktober 2026)

| Test | Ergebnis |
| ---- | -------- |
| `meson test` mit echten TLS-Verbindungen (der Test-Client prüft den Fingerabdruck wie die App): richtiger Fingerabdruck angenommen, falscher abgelehnt, Klartext ohne Antwort, ohne `openssl` kein Start, ungültige Adresse nicht in den Aufruf übernommen, kein Temporärordner bleibt liegen, Wiederaufnahme nur mit Schlüssel und Zertifikat, Schlüsselwechsel erneuert das Zertifikat, längster Link passt in den QR-Code | alle bestanden (26 von 26 Tests) |
| Absichtlich eingebaute Fehler (Test-Client nimmt jedes Zertifikat an; Server ohne TLS; Fingerabdruckprüfung der App entfernt; `http://` erlaubt) | jeweils vom Test erkannt, danach zurückgesetzt |
| `nolphin-handy/test.sh`: Fingerabdruck-Prüfung der App gegen zwei Zertifikate und einen Klartext-Server | 16 Prüfungen bestanden |
| `nolphin-handy/test.sh`: App-Seite gegen den **echten Nolphin-Server** (C, GnuTLS): beide Seiten berechnen denselben Fingerabdruck | 7 Prüfungen bestanden |
| `nolphin-handy/test.sh`: Link-Prüfung (Version 2, alte Links, Fingerabdruck) | 55 Prüfungen bestanden |
| `nolphin-handy/test.sh`: QR-Leser, auch bei starker Schräglage, mit Mauszeiger auf dem Code, Schatten, Rauschen und Unschärfe | 181 Prüfungen bestanden (der alte Stand scheiterte an 12 von 18 Schräglagen-Bildern) |
| Am echten Handy (Android 14, Xiaomi): verschlüsselt verbinden über den Scanner der App, Datei vom PC laden, Datei und Ordner mit Unterordner zum PC senden | bestanden |
| Am echten Handy: falscher Fingerabdruck; alter Link (Version 1) | je mit deutlicher Meldung abgelehnt; eine gute Verbindung bleibt unverändert |
| Am echten Handy: Scanner von Xiaomi | gibt das Ergebnis nicht an die App zurück (Text oder Browser), deshalb nicht mehr benutzt |

**Nicht geprüft:** ob andere Kamera- und Scanner-Apps `nolphin://`-Links an die App weitergeben; das Panel mit dem Papierkorb von Nolphin auf anderen Desktops; Mitschnitt des Datenverkehrs am Router (der Test mit einer Klartext-Anfrage ersetzt ihn, aber nicht vollständig).

### Stand vor der Verschlüsselung (11. Oktober 2026)

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

1. ~~HTTPS mit angeheftetem Zertifikat~~ — **umgesetzt**, siehe oben.
2. **Bestätigung pro Übertragung am PC** (nach dem Vorbild von LocalSends `prepare-upload`) einbauen? Heute genügt der Start des Austauschs.
3. **Prüfsumme** (sha256) für Übertragungen einbauen?
