# Handy-Austausch

Bilder und Dateien zwischen Handy und PC austauschen, über das WLAN, ohne App auf dem Handy und ohne Zusatzprogramm.

![Seitenleiste mit dem Eintrag Handy, in der Mitte der Eingangsordner mit zwei empfangenen Bildern, rechts das Panel mit QR-Code, Link und der Liste der empfangenen Dateien](bilder/panel-handy.png)

## So geht es

1. Klicken Sie in der Seitenleiste unter **Mein Rechner** auf **Handy** (oder wählen Sie **Datei ▸ Handy-Austausch …**).
2. Nolphin startet den Austausch, zeigt rechts den **QR-Code** und öffnet in der Hauptansicht den **Eingangsordner** `~/Handy-Eingang`.
3. Scannen Sie den QR-Code mit der Kamera des Handys. Handy und PC müssen im selben WLAN sein.
4. Im Browser des Handys wählen Sie Bilder oder Dateien aus. Sie erscheinen im Eingangsordner und in der Liste „Empfangen“.
5. **Beenden** schließt den Austausch.

Der Link steht unter dem QR-Code und lässt sich mit dem Knopf daneben kopieren, falls die Kamera den Code nicht erkennt.

## Dateien vom PC aufs Handy

Unter **Einstellungen** (eingeklappt, unten im Panel) wählen Sie einen **Freigabeordner**. Die Dateien darin erscheinen auf der Seite im Handy-Browser und lassen sich dort laden. Ohne Freigabeordner gibt es keine Downloads.

Ebenfalls unter **Einstellungen**: der Eingangsordner und die Netzwerkadresse des PCs, falls der Rechner in mehreren Netzen hängt. Beides lässt sich nur ändern, solange der Austausch gestoppt ist, und wird nicht über das Schließen des Fensters hinaus gespeichert.

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
| Der Upload bricht ab | Prüfen Sie, ob Platz im Eingangsordner ist. Einzelne Dateien dürfen höchstens 2 GiB groß sein. |

## Grenzen

Das Handy kann **nicht** vom PC aus durchsucht werden. Der Austausch läuft nur über die Seite im Handy-Browser. Wer die Ordner des Handys wie einen normalen Ordner öffnen möchte, schließt es per USB-Kabel an (Android, Übertragungsmodus „Dateien“).
