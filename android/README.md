# Nolphin Handy (Android-App)

Kleine App für den Handy-Austausch von Nolphin. Sie ist der **Empfänger auf dem Handy**: Legen Sie am PC Dateien in „Zum Handy“, kommen sie von selbst auf das Handy. Dateien vom Handy schicken Sie über das Teilen-Menü an den PC.

Nur das Android-Framework, **keine Bibliotheken**, kein Gradle. Mindestens Android 10.

## Einrichten (Anwender)

1. Klicken Sie in Nolphin in der Seitenleiste auf **Handy** und scannen Sie den QR-Code mit der Kamera.
2. **Erster Scan:** Die Seite im Handy-Browser bietet die App zum Installieren an. Laden Sie sie, erlauben Sie die Installation und scannen Sie den QR-Code noch einmal.
3. **Jeder weitere Scan** öffnet die App und verbindet sie mit dem PC. Der Schlüssel im Code gilt, solange der Austausch am PC läuft; nach einem Neustart scannen Sie einfach erneut.

Die App lädt neue Dateien nach `Download/Nolphin`. Beim allerersten Verbinden werden bereits vorhandene Dateien nicht geladen; „Alle Dateien vom PC laden“ holt sie nach.

## Bauen (Entwickler)

Voraussetzungen: JDK (`javac`, `keytool`) und das Android-SDK (`android.jar`, `aapt2`, `d8`, `zipalign`, `apksigner`).

```bash
android/build.sh
```

Ergebnis: `android/build/nolphin-handy.apk`. Beim ersten Lauf entsteht ein Signaturschlüssel in `~/.config/nolphin/` (nicht im Repository). Liegt die APK vor dem Installieren von Nolphin da, wird sie nach `<datadir>/nolphin/phone/` mitinstalliert und der Handy-Austausch bietet sie zum Laden an. Ohne APK läuft alles weiter im Handy-Browser.
