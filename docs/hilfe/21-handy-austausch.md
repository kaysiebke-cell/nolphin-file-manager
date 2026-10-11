# Handy-Austausch

Bilder und Dateien in **beide Richtungen** zwischen Handy und PC austauschen, über das WLAN, ohne App auf dem Handy und ohne Zusatzprogramm. Es gibt nichts einzustellen.

![Seitenleiste mit dem Eintrag Handy, in der Mitte der Ordner Handy mit „Vom Handy“ und „Zum Handy“, rechts das Panel mit QR-Code, Link und der Liste der empfangenen Dateien](bilder/panel-handy.png)

## Verbinden

1. Klicken Sie in der Seitenleiste unter **Mein Rechner** auf **Handy** (oder wählen Sie **Datei ▸ Handy ▸ Austausch öffnen …**).
2. Nolphin startet den Austausch, zeigt rechts den **QR-Code** und öffnet in der Hauptansicht den Ordner `~/Handy`.
3. Scannen Sie den QR-Code mit der Kamera des Handys. Handy und PC müssen im selben WLAN sein.
4. **Beenden** schließt den Austausch.

Der Link steht unter dem QR-Code und lässt sich mit dem Knopf daneben kopieren, falls die Kamera den Code nicht erkennt.

## Mit der App (empfohlen)

Im Browser holt das Handy Dateien nur ab, wenn Sie sie antippen. Die kleine **Nolphin-Handy-App** ist der Empfänger auf dem Handy: Was Sie am PC in „Zum Handy“ legen, kommt von selbst an.

1. **Erster Scan:** Scannen Sie den QR-Code mit der Kamera des Handys. Die Seite im Handy-Browser bietet die App zum Installieren an. Laden Sie sie und erlauben Sie die Installation (siehe unten, „Warnungen bei der Installation“).
2. **Verbinden in der App:** Öffnen Sie die Nolphin-Handy-App und tippen Sie auf **QR-Code scannen**. Richten Sie die Kamera auf den QR-Code am PC. Die App liest ihn selbst, es ist keine Eingabe nötig. Danach steht dort „Verbunden“.
3. **Jeder weitere Scan** geht genauso, oder mit der Kamera des Handys: Die Seite öffnet dann die App und verbindet sie.
4. Neue Dateien landen auf dem Handy im Ordner `Download/Nolphin`, mit einer Benachrichtigung.
5. Dateien vom Handy schicken Sie über **Teilen ▸ Nolphin (PC)** oder in der App mit **Dateien an den PC senden …**.

Der Scanner hat unten zwei Ausweichwege: **Foto aufnehmen** (die Kamera-App des Handys macht ein scharfes Foto, die App liest den Code darin, das Foto wird danach gelöscht) und **Bild wählen** (ein vorhandenes Bild oder einen Screenshot des Codes). Beide lesen auch große Fotos.

Geht das Scannen nicht, kopieren Sie den Link unter dem QR-Code im Panel und tippen in der App auf **Link aus der Zwischenablage einfügen** (oder tippen Sie ihn in das Feld darunter).

Der Schlüssel im QR-Code gilt, solange der Austausch am PC läuft, höchstens 12 Stunden. Nach einem Neustart von Nolphin oder nach **Neuen QR-Code erzeugen** (Knopf neben dem Link im Panel) ist der alte Code sofort ungültig; scannen Sie dann einfach den neuen. Steht in der App „Schlüssel abgelaufen“, ist das der Grund.

Die App prüft jeden Link, bevor sie ihn speichert: Sie akzeptiert nur Adressen aus dem eigenen Netz, fragt am PC nach, ob dort Nolphin antwortet und der Schlüssel gilt, und meldet sonst genau, was nicht stimmt.

### Übertragungen in der App

- Beim Laden und Senden zeigt die App den **Fortschritt** („Lade Bild.jpg: 45 %“) und einen Knopf **Übertragung abbrechen**. Eine abgebrochene Datei wird nicht von selbst erneut geladen; mit **Alle Dateien vom PC laden** holen Sie sie nach.
- Mit dem Häkchen **Vor dem Laden vom PC fragen** lädt die App neue Dateien nicht sofort, sondern zeigt eine Benachrichtigung „Datei vom PC bereit“ mit der Aktion **Laden**.
- Bricht die Verbindung ab, löscht die App die Teildatei und versucht es weiter („PC nicht erreichbar, neuer Versuch …“).

Die App gibt es nur, wenn sie beim Installieren von Nolphin gebaut wurde (siehe `android/README.md`). Sonst funktioniert der Austausch weiter im Browser.

## Menü

Unter **Datei ▸ Handy** gibt es vier Einträge: **Austausch öffnen …**, **An Handy senden** (nur bei gewählten Dateien), **Neuen QR-Code erzeugen** und **Austausch beenden** (die beiden letzten nur, solange der Austausch läuft).

## Einstellungen

Die Werte stehen in GSettings (`org.nolphin.preferences`) und lassen sich mit `gsettings` oder dconf ändern:

| Schlüssel | Bedeutung | Vorgabe |
| --------- | --------- | ------- |
| `phone-folder` | Ordner mit „Vom Handy“ und „Zum Handy“ (leer: `~/Handy`) | leer |
| `phone-token-hours` | Gültigkeit des Schlüssels in Stunden (1 bis 168) | 12 |
| `phone-idle-minutes` | Selbstabschaltung ohne Anfrage in Minuten (1 bis 240) | 15 |
| `phone-max-upload-mb` | größte Datei vom Handy in MiB (1 bis 4096) | 2048 |

Die Werte gelten beim nächsten Start des Austauschs.

## Die beiden Ordner

| Ordner | Richtung | Inhalt |
| ------ | -------- | ------ |
| `~/Handy/Vom Handy` | Handy → PC | Hierhin kommen die Dateien, die Sie am Handy auswählen. |
| `~/Handy/Zum Handy` | PC → Handy | Alles darin kann das Handy laden. |

Beide Ordner legt Nolphin selbst an.

## Vom Handy zum PC

Wählen Sie auf der Seite im Handy-Browser **Dateien auswählen …** und dann Bilder oder Dateien. Sie erscheinen in `Vom Handy` und in der Liste „Empfangen“ im Panel.

## Vom PC zum Handy

Es gibt zwei Wege:

- **Ziehen:** Ziehen Sie Dateien aus der Hauptansicht auf **Handy** in der Seitenleiste.
- **Rechtsklick:** Wählen Sie Dateien und dann **An Handy senden**.

![Kontextmenü einer Datei mit dem Eintrag „An Handy senden“](bilder/kontextmenue-handy.png)

Beide Wege **kopieren**. Das Original bleibt, wo es ist. Mit der App kommt die Datei von selbst aufs Handy. Im Browser erscheint sie unter **Vom PC laden** und lässt sich dort laden; die Liste aktualisiert sich alle paar Sekunden. Eine Datei erscheint erst, wenn sie fertig kopiert ist (nach etwa drei Sekunden).

## Sicherheit

- Der Link enthält einen zufälligen Schlüssel. Ohne ihn antwortet Nolphin nicht.
- Es werden nur Verbindungen aus dem eigenen Netz angenommen (private Adressen).
- Der Austausch läuft nur, solange Sie ihn gestartet haben. Nach 15 Minuten ohne Anfrage beendet er sich selbst.
- Hochgeladene Dateien überschreiben nie etwas. Gibt es den Namen schon, hängt Nolphin „(1)“ an.
- Die Verbindung ist **nicht verschlüsselt**. Verwenden Sie den Austausch nur im eigenen WLAN, nicht in fremden oder öffentlichen Netzen.

## Wenn es nicht klappt

| Problem | Ursache und Lösung |
| ------- | ------------------ |
| „Kein lokales Netzwerk gefunden“ | Der PC ist mit keinem WLAN oder LAN verbunden. Verbinden Sie ihn und klicken Sie erneut auf **Handy**. |
| Die Seite öffnet sich am Handy nicht | Eine Firewall am PC blockiert die Verbindung, oder das WLAN trennt die Geräte voneinander (zum Beispiel ein Gast-WLAN). |
| Die Kamera erkennt den Code nicht | Link kopieren und im Browser des Handys eintippen. |
| Der Scanner liest den Code nicht | Unten im Scanner steht eine Zeile mit der Kameragröße, der Zahl der gelesenen Bilder und der Zahl der gefundenen Suchmuster. „Suchmuster: 3“ heißt, der Scanner sieht den Code, kann ihn aber nicht lesen: etwas näher oder weiter weg gehen und ruhig halten. Bei 0 bis 2 sieht er den Code nicht: den ganzen Code ins Bild nehmen, Spiegelungen auf dem Bildschirm vermeiden. Als Ausweg den Link kopieren und in der App einfügen. |
| Das Senden vom Handy schlägt fehl | Die App nennt den Grund in ihrem Protokoll („Schlüssel abgelaufen“, „PC antwortet mit Fehler …“ oder die Art des Fehlers). Nach einem Neustart von Nolphin am PC den QR-Code neu scannen. |
| Der Upload bricht ab | Prüfen Sie, ob Platz im Ordner `Vom Handy` ist. Einzelne Dateien dürfen höchstens 2 GiB groß sein. |
| Der PC hat mehrere Netzwerke | Dann erscheint im Panel eine Auswahl der Netzwerkadresse. Wählen Sie die des WLANs, in dem das Handy ist. |

## Warnungen bei der Installation

Die App kommt nicht aus dem Play Store. Deshalb warnen Chrome, Google Play Protect und je nach Hersteller weitere Schutzprogramme (zum Beispiel „Mi Protect“ bei Xiaomi), bevor sie sich installieren lässt. Das ist normal. Bestätigen Sie mit **Trotzdem herunterladen** und beim Schutzprogramm mit **Überspringen** oder **Installieren**. Die App ist klein (rund 20 KB), enthält keine Bibliotheken und ihr Quelltext liegt im Verzeichnis `android/` von Nolphin.

## Grenzen

Das Handy kann **nicht** vom PC aus durchsucht werden. Der Austausch läuft über die App oder die Seite im Handy-Browser. Ohne die App kann der PC nichts aufs Handy schieben: Der Browser holt Dateien nur nach einem Tipp. Wer die Ordner des Handys wie einen normalen Ordner öffnen möchte, schließt es per USB-Kabel an (Android, Übertragungsmodus „Dateien“).
