# Handy-Austausch

Bilder, Dateien und Ordner in **beide Richtungen** zwischen Handy und PC austauschen, über das WLAN und **verschlüsselt**. Dafür gibt es die kleine **Nolphin-Handy-App** (eigenes Projekt, Android). Am PC gibt es nichts einzustellen; Nolphin braucht dort `openssl` (siehe unten).

![Seitenleiste mit dem Eintrag Handy, in der Mitte der Ordner Handy mit dem Unterordner „Vom Handy“, rechts das Panel mit QR-Code, Link und der Liste der empfangenen Dateien](bilder/panel-handy.png)

## Verbinden

1. Klicken Sie in der Seitenleiste unter **Mein Rechner** auf **Handy**, um den Ordner zu öffnen, und wählen Sie **Datei ▸ Handy ▸ Austausch öffnen …** für QR-Code und Link.
2. Nolphin startet den Austausch, zeigt rechts den **QR-Code** und öffnet in der Hauptansicht den Ordner `~/Handy`.
3. Öffnen Sie am Handy die Nolphin-Handy-App und tippen Sie auf **QR-Code scannen und verbinden**. Halten Sie die Kamera **ruhig und gerade** auf den QR-Code am PC, etwa 15 bis 30 cm entfernt. Die App liest ihn selbst und verbindet sich sofort. Danach steht dort „Verbunden“. Handy und PC müssen im selben WLAN sein.
4. **Beenden** schließt den Austausch.

Die App verbindet sich **nur mit diesem PC**: Der QR-Code enthält neben dem Schlüssel den Fingerabdruck des Zertifikats, das Nolphin für diesen Austausch erzeugt hat. Ein anderer Rechner, der sich als Ihr PC ausgibt, wird abgelehnt.

Der Link steht unter dem QR-Code und lässt sich mit dem Knopf daneben kopieren, falls die Kamera den Code nicht liest. In der App fügen Sie ihn mit **Link aus der Zwischenablage einfügen** ein; ein frisch kopierter Link wird beim Öffnen der App auch von selbst angeboten.

## Mit der App

1. Neue Dateien aus dem Ordner `~/Handy` landen von selbst auf dem Handy im Ordner `Download/Nolphin`, mit einer Benachrichtigung.
2. Dateien und ganze Ordner vom Handy schicken Sie über **Teilen ▸ Nolphin (PC)**, in der App mit **Dateien an den PC senden …** oder im Dateimanager der App mit **An den PC senden**.
3. Die App ist zugleich ein **Dateimanager für das Handy** (Kopieren, Verschieben, Umbenennen, ZIP, Papierkorb, Speicheranalyse). Das beschreibt die README des Projekts.
4. Lange auf das App-Symbol drücken zeigt die Verknüpfung **QR-Code scannen**.

Der Scanner der App hat keine Knöpfe. Wird das Bild unscharf, tippen Sie darauf (Scharfstellen). Oben im Scanner steht, ob der Code im Bild ist („QR-Code im Bild, aber noch unscharf“); unten steht eine Diagnosezeile. Der Scanner liest den Code auch bei schräg gehaltenem Handy und bei Spiegelungen. Mit der Zurück-Taste des Handys verlassen Sie ihn.

Der Schlüssel im QR-Code gilt, solange der Austausch am PC läuft, höchstens 12 Stunden. Nach einem Neustart von Nolphin oder nach **Neuen QR-Code erzeugen** (Knopf neben dem Link im Panel) ist der alte Code sofort ungültig, auch sein Zertifikat; scannen Sie dann einfach den neuen. Steht in der App „Schlüssel abgelaufen“ oder „Zertifikat passt nicht zum QR-Code“, ist das der Grund.

Die Startseite der App zeigt nur, was gerade nötig ist: ohne Verbindung einen Knopf zum Scannen, mit Verbindung den Status, „Dateien an den PC senden …“ und „Verbindung trennen“.

Die App prüft jeden Link, bevor sie ihn speichert: Sie akzeptiert nur Adressen aus dem eigenen Netz, fragt am PC nach, ob dort Nolphin antwortet und der Schlüssel gilt, und meldet sonst genau, was nicht stimmt. Ein Link, der **von außen** kommt (Kamera-App, Browser, andere App), verbindet erst nach einer Rückfrage mit der Anzeige der PC-Adresse. Bestätigen Sie nur, wenn Sie den Code gerade selbst an Ihrem PC angezeigt haben.

### Übertragungen in der App

- Beim Laden und Senden zeigt die App den **Fortschritt** („Lade Bild.jpg: 45 %“) und einen Knopf **Übertragung abbrechen**. Eine abgebrochene Datei wird nicht von selbst erneut geladen.
- Bricht die Verbindung ab, löscht die App die Teildatei und versucht es weiter („PC nicht erreichbar, neuer Versuch …“).

## Menü

Unter **Datei ▸ Handy** gibt es vier Einträge: **Austausch öffnen …**, **An Handy senden** (nur bei gewählten Dateien), **Neuen QR-Code erzeugen** und **Austausch beenden** (die beiden letzten nur, solange der Austausch läuft).

## Einstellungen

Die Werte stehen in GSettings (`org.nolphin.preferences`) und lassen sich mit `gsettings` oder dconf ändern:

| Schlüssel | Bedeutung | Vorgabe |
| --------- | --------- | ------- |
| `phone-folder` | Ordner „Handy“ mit dem Unterordner „Vom Handy“ (leer: `~/Handy`) | leer |
| `phone-token-hours` | Gültigkeit des Schlüssels in Stunden (1 bis 168) | 12 |
| `phone-idle-minutes` | Selbstabschaltung ohne Anfrage in Minuten (1 bis 240) | 15 |
| `phone-max-upload-mb` | größte Datei vom Handy in MiB (1 bis 4096) | 2048 |

Die Werte gelten beim nächsten Start des Austauschs.

## Die beiden Ordner

| Ordner | Richtung | Inhalt |
| ------ | -------- | ------ |
| `~/Handy/Vom Handy` | Handy → PC | Hierhin kommen die Dateien, die Sie am Handy auswählen. |
| `~/Handy` | PC → Handy | Alle Dateien darin kann das Handy laden (Unterordner nicht). |

Beide Ordner legt Nolphin selbst an.

## Vom Handy zum PC

Teilen Sie Dateien oder Ordner aus der App (oder aus jeder App mit dem Teilen-Menü) an **Nolphin (PC)**. Sie erscheinen in `Vom Handy` und in der Liste „Empfangen“ im Panel. Ordner kommen samt Unterordnern an.

## Vom PC zum Handy

Es gibt zwei Wege:

- **Ziehen:** Ziehen Sie Dateien aus der Hauptansicht auf **Handy** in der Seitenleiste.
- **Rechtsklick:** Wählen Sie Dateien und dann **An Handy senden**.

![Kontextmenü einer Datei mit dem Eintrag „An Handy senden“](bilder/kontextmenue-handy.png)

Beide Wege **kopieren**. Das Original bleibt, wo es ist. Die Datei kommt von selbst aufs Handy. Sie erscheint erst, wenn sie fertig kopiert ist (nach etwa drei Sekunden).

## Sicherheit

- Die Verbindung ist **verschlüsselt** (TLS). Ein Mitlesen im WLAN nützt nichts. Nolphin erzeugt dafür für jeden Schlüssel ein eigenes Zertifikat; die App nimmt nur genau dieses an.
- Der QR-Code enthält den **Schlüssel**. Zeigen Sie ihn nur Ihrem eigenen Handy, fotografieren Sie ihn nicht und beenden Sie den Austausch, wenn Sie fertig sind (**Beenden**). Wer den Code gesehen hat, kann sich verbinden, bis er widerrufen ist. **Neuen QR-Code erzeugen** widerruft ihn sofort.
- Es werden nur Verbindungen aus dem eigenen Netz angenommen (private Adressen).
- Der Austausch läuft nur, solange Sie ihn gestartet haben. Nach 15 Minuten ohne Anfrage beendet er sich selbst.
- Hochgeladene Dateien überschreiben nie etwas. Gibt es den Namen schon, hängt Nolphin „(1)“ an.
- Es gibt keinen unverschlüsselten Weg und keine Webseite für den Browser des Handys.
- Voraussetzung am PC sind `openssl` und `glib-networking` (bei Cinnamon-Systemen üblich). Fehlt eines, startet der Austausch nicht und nennt den Grund.
- Die Sitzung (Schlüssel, Zertifikat und dessen privater Schlüssel) steht nur für Sie lesbar in `~/.config/nolphin/phone-session`, damit das Handy den PC nach einem Neustart wiederfindet. **Beenden** und **Neuen QR-Code erzeugen** löschen oder ersetzen die Datei.

## Wenn es nicht klappt

| Problem | Ursache und Lösung |
| ------- | ------------------ |
| „Kein lokales Netzwerk gefunden“ | Der PC ist mit keinem WLAN oder LAN verbunden. Verbinden Sie ihn und klicken Sie erneut auf **Handy**. |
| Die App verbindet nicht („Keine Verbindung zum PC“) | Eine Firewall am PC blockiert die Verbindung, oder das WLAN trennt die Geräte voneinander (zum Beispiel ein Gast-WLAN). |
| Die Kamera erkennt den Code nicht | Link mit dem Knopf neben dem Link kopieren und in der App mit **Link aus der Zwischenablage einfügen** einfügen. |
| Der Scanner des Handys (zum Beispiel von Xiaomi) zeigt nur Text | Er gibt das Ergebnis nicht an die App weiter. Scannen Sie mit dem Scanner **in der App** (**QR-Code scannen und verbinden**). |
| Chrome zeigt „Dies ist keine sichere Verbindung“ | Ein Browser kann den Fingerabdruck nicht prüfen. Tippen Sie **nicht** auf „Weiter“, schließen Sie den Tab und scannen Sie in der App. |
| „Zertifikat des PCs passt nicht zum QR-Code“ | Am PC wurde ein neuer Code erzeugt (neuen Code scannen), oder jemand im Netz gibt sich als Ihr PC aus. Es wurde nichts gesendet. |
| Der Austausch startet nicht („openssl fehlt“) | Installieren Sie das Paket `openssl` (und `glib-networking`). Es gibt keinen Weg ohne Verschlüsselung. |
| Der Scanner der App liest den Code nicht | Unten im Scanner steht eine Zeile mit der Kameragröße, der Zahl der gelesenen Bilder und der Zahl der gefundenen Suchmuster. „Suchmuster: 3“ heißt, der Scanner sieht den Code, kann ihn aber nicht lesen: etwas näher oder weiter weg gehen, ruhig halten und aufs Bild tippen. Bei 0 bis 2 sieht er den Code nicht: den ganzen Code ins Bild nehmen, Spiegelungen auf dem Bildschirm vermeiden, den Mauszeiger vom Code wegbewegen. Als Ausweg den Link kopieren und in der App einfügen. |
| Das Senden vom Handy schlägt fehl | Die App nennt den Grund in ihrem Protokoll („Schlüssel abgelaufen“, „PC antwortet mit Fehler …“ oder die Art des Fehlers). Nach einem Neustart von Nolphin am PC den QR-Code neu scannen. |
| Der Upload bricht ab | Prüfen Sie, ob Platz im Ordner `Vom Handy` ist. Einzelne Dateien dürfen höchstens 2 GiB groß sein. |
| Der PC hat mehrere Netzwerke | Dann erscheint im Panel eine Auswahl der Netzwerkadresse. Wählen Sie die des WLANs, in dem das Handy ist. |

## Die App installieren

Die App kommt nicht aus dem Play Store. Die **Erstinstallation** geht nicht mehr über den Browser des Handys (das verschlüsselte Nolphin hat keine Webseite mehr): Kopieren Sie die Datei `nolphin-handy.apk` (nach der Installation von Nolphin unter `/usr/local/share/nolphin/phone/`, sonst im Build-Ordner der App) zum Beispiel per USB-Kabel aufs Handy und öffnen Sie sie dort. Android fragt nach der Erlaubnis „Apps aus dieser Quelle installieren“, und je nach Hersteller warnen Schutzprogramme (zum Beispiel „Mi Protect“ bei Xiaomi). Das ist normal. Die App enthält keine Bibliotheken, ihr Quelltext liegt im Projekt [nolphin-handy](https://github.com/kaysiebke-cell/nolphin-handy). **Aktualisierungen** holt die App danach selbst über die verschlüsselte Verbindung.

## Grenzen

Das Handy kann **nicht** vom PC aus durchsucht werden. Der Austausch läuft über die App; ohne sie gibt es keinen Handy-Austausch. Wer die Ordner des Handys wie einen normalen Ordner öffnen möchte, schließt es per USB-Kabel an (Android, Übertragungsmodus „Dateien“).
