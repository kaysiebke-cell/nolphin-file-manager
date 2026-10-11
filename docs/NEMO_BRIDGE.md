# Nolphin als Nemo-Ersatz einrichten

Diese Anleitung beschreibt, wie Nolphin auf einem Linux Mint/Cinnamon-System
alle praktischen Aufgaben von Nemo übernimmt (Desktop-Icons, Standard-
Dateimanager), ohne Nemo zu deinstallieren.

## Warum Nemo installiert bleibt

Das Paket `cinnamon` hat `nemo` als feste Abhängigkeit:

```bash
apt-cache depends cinnamon | grep nemo
#   Hängt ab von: nemo
#   Hängt ab von: gir1.2-nemo-3.0
#   Hängt ab von: nemo-fileroller
```

`apt remove nemo` würde deshalb auch `cinnamon` selbst entfernen und die
grafische Oberfläche unbenutzbar machen. Nemo bleibt daher installiert
(erfüllt die Paketabhängigkeit), wird aber über die folgenden
Einstellungen nie mehr aufgerufen.

## 1. Nolphin bauen und systemweit installieren

```bash
cd /home/kaysiebke/nolphin-file-manager-master
meson setup build
ninja -C build
sudo ninja -C build install
```

Dadurch werden u. a. folgende Dateien auf dem System angelegt/aktualisiert:

- `/usr/local/bin/nolphin`
- `/usr/local/bin/nolphin-desktop`
- `/usr/local/share/applications/org.nolphin.FileManager.desktop`
- `/usr/local/share/applications/nolphin-autostart.desktop`
- `/usr/local/share/glib-2.0/schemas/org.nolphin.gschema.xml` (kompiliert)

## 2. Desktop-Icons: Nolphin statt Nemo

```bash
# Nolphins Autostart-Eintrag an die Stelle kopieren, die beim Login
# tatsächlich ausgewertet wird
mkdir -p ~/.config/autostart
cp /home/kaysiebke/nolphin-file-manager-master/data/nolphin-autostart.desktop \
   ~/.config/autostart/nolphin-autostart.desktop

# Nemos Desktop-Icon-Verwaltung abschalten (Einstellung, keine Deinstallation)
gsettings set org.nemo.desktop show-desktop-icons false
```

Wirkt vollständig erst nach Ab-/Neuanmelden. Für einen sofortigen Test ohne
Neustart:

```bash
pkill -f nolphin-desktop
nohup /usr/local/bin/nolphin-desktop >/tmp/nolphin-desktop.log 2>&1 &
disown
```

## 3. Standard-Dateimanager: Nolphin statt Nemo

```bash
xdg-mime default org.nolphin.FileManager.desktop inode/directory application/x-directory
```

Test: `gio open ~` öffnet danach Nolphin statt Nemo.

## 4. Manuell nachzuziehen: Taskleisten-/Menü-Favoriten

Ein in der Taskleiste angepinntes Nemo-Icon zeigt weiterhin auf
`nemo.desktop` (liegt in einer von Cinnamon selbst verwalteten
Konfigurationsdatei, z. B.
`~/.config/cinnamon/spices/grouped-window-list@cinnamon.org/<instanz>.json`).
Per Rechtsklick auf das Icon → „Lösen", danach Nolphin manuell anpinnen.

## 5. Code-Fix: Arbeitsbereich-Leiste erschien auf dem Desktop

Beim ersten Test zeigte `nolphin-desktop` fälschlich die rechte
Vorschau-/Eigenschaften-Leiste des normalen Dateimanager-Fensters mit auf
dem Desktop an.

**Geänderte Datei:** `src/nolphin-window.c`, Funktion
`nolphin_window_constructed()`.

**Ursache:** Das Desktop-Fenster (`NolphinDesktopWindow`) erbt vom normalen
Fenstertyp (`NolphinWindow`) und bekam dadurch ungewollt dessen
Vorschau-Leiste mit, weil diese standardmäßig automatisch eingeblendet wird.

**Fix:**

```c
if (!NOLPHIN_IS_DESKTOP_WINDOW (window)) {
    nolphin_window_set_show_preview (window,
                                     g_settings_get_boolean (nolphin_window_state,
                                                              NOLPHIN_WINDOW_STATE_START_WITH_PREVIEW));
}
```

Danach neu bauen und installieren (Schritt 1 wiederholen), damit der Fix in
`/usr/local/bin/nolphin-desktop` ankommt.

## 6. Wiederherstellung (falls etwas nicht funktioniert)

Strg+Alt+F3 (reine Text-Konsole, kein Cinnamon nötig), dort einloggen, dann:

```bash
gsettings set org.nemo.desktop show-desktop-icons true
rm ~/.config/autostart/nolphin-autostart.desktop
xdg-mime default nemo.desktop inode/directory application/x-directory
```

Zurück zur grafischen Oberfläche: Strg+Alt+F1 (oder F2). Nemo war zu keinem
Zeitpunkt deinstalliert; diese drei Befehle stellen den vorherigen Zustand
exakt wieder her. Nur falls das Nemo-Paket selbst beschädigt sein sollte:

```bash
sudo apt install --reinstall nemo nemo-data nemo-fileroller
```
