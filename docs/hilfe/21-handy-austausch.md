# Handy-Austausch

Bilder und Dateien in **beide Richtungen** zwischen Handy und PC austauschen, über das WLAN, ohne App auf dem Handy und ohne Zusatzprogramm. Es gibt nichts einzustellen.

![Seitenleiste mit dem Eintrag Handy, in der Mitte der Ordner Handy mit „Vom Handy“ und „Zum Handy“, rechts das Panel mit QR-Code, Link und der Liste der empfangenen Dateien](bilder/panel-handy.png)

## Verbinden

1. Klicken Sie in der Seitenleiste unter **Mein Rechner** auf **Handy** (oder wählen Sie **Datei ▸ Handy-Austausch …**).
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

Geht das Scannen nicht, kopieren Sie den Link unter dem QR-Code im Panel und tippen in der App auf **Link aus der Zwischenablage einfügen** (oder tippen Sie ihn in das Feld darunter).

Der Schlüssel im QR-Code gilt, solange der Austausch am PC läuft. Nach einem Neustart von Nolphin scannen Sie den Code einfach erneut. Steht in der App „Schlüssel abgelaufen“, ist das der Grund.

Die App gibt es nur, wenn sie beim Installieren von Nolphin gebaut wurde (siehe `android/README.md`). Sonst funktioniert der Austausch weiter im Browser.

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
| Der Upload bricht ab | Prüfen Sie, ob Platz im Ordner `Vom Handy` ist. Einzelne Dateien dürfen höchstens 2 GiB groß sein. |
| Der PC hat mehrere Netzwerke | Dann erscheint im Panel eine Auswahl der Netzwerkadresse. Wählen Sie die des WLANs, in dem das Handy ist. |

## Warnungen bei der Installation

Die App kommt nicht aus dem Play Store. Deshalb warnen Chrome, Google Play Protect und je nach Hersteller weitere Schutzprogramme (zum Beispiel „Mi Protect“ bei Xiaomi), bevor sie sich installieren lässt. Das ist normal. Bestätigen Sie mit **Trotzdem herunterladen** und beim Schutzprogramm mit **Überspringen** oder **Installieren**. Die App ist klein (rund 20 KB), enthält keine Bibliotheken und ihr Quelltext liegt im Verzeichnis `android/` von Nolphin.

## Grenzen

Das Handy kann **nicht** vom PC aus durchsucht werden. Der Austausch läuft über die App oder die Seite im Handy-Browser. Ohne die App kann der PC nichts aufs Handy schieben: Der Browser holt Dateien nur nach einem Tipp. Wer die Ordner des Handys wie einen normalen Ordner öffnen möchte, schließt es per USB-Kabel an (Android, Übertragungsmodus „Dateien“).
