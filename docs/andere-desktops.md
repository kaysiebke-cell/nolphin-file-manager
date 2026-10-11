[Startseite](../README.md) | [← Einstellungen und Anpassung](einstellungen.md) | [Nolphin und Nemo →](nolphin-und-nemo.md)

# Andere Desktops: XFCE und MATE

Nolphin ist für Linux Mint Cinnamon optimiert, läuft aber auch auf anderen GTK-Desktops wie XFCE und MATE. Die Cinnamon-Bestandteile sind deshalb optional:

* **Einstellungen:** Nolphin liest Terminal, Uhrzeitformat, „Zuletzt verwendet“, Schrift und Medien-Optionen aus dem passenden installierten Schema: zuerst das des laufenden Desktops (`XDG_CURRENT_DESKTOP`: unter MATE `org.mate.*`, unter Cinnamon `org.cinnamon.*`, unter GNOME `org.gnome.*`), danach der Reihe nach Cinnamon, MATE, GNOME. So gilt unter MATE das MATE-Terminal, auch wenn Cinnamon-Schemas ebenfalls installiert sind. Fehlt keines davon, nimmt es eigene Ersatz-Schemas (`org.nolphin.compat.*`), die nur in Nolphin wirken. Die Medien-Optionen (Automount, Autostart) sind dann ausgegraut, weil sie dem Desktop gehören.
* **Symbole:** Nolphin benutzt 86 Symbole mit dem Namen `xsi-…-symbolic` (Seitenleiste, Werkzeugleiste, Menüs), die das Linux-Mint-Paket `xapp-symbolic-icons` liefert. Fehlt es, legt Nolphin beim Start für jedes fehlende Symbol einen Verweis auf das passende Standardsymbol des aktuellen Symbolthemas an (z. B. `xsi-edit-copy-symbolic` → `edit-copy-symbolic`, `xsi-search-symbolic` → `edit-find-symbolic`) und wiederholt das bei einem Themenwechsel. Die Verweise liegen in `~/.cache/nolphin/xsi-fallback`; es werden keine fremden Dateien mitgeliefert, und vorhandene `xsi`-Symbole werden nie ersetzt. In GTK eingebaute Symbole werden als Datei kopiert, die Endung `.symbolic.png` bleibt erhalten (sonst färbt GTK sie falsch), und kaputte Verweise im Thema werden übersprungen. Das Werkzeug `test/icon-theme-coverage` (`NOLPHIN_COVERAGE_ICONS=<Ordner ohne xsi-Dateien> icon-theme-coverage <Thema> …`) zeigt je Thema, was nach dem Ersatz noch fehlt. Ohne diese Maßnahme zeigte Nolphin Verbotsschilder statt Symbolen und wirkte wie ein Fremdkörper.
* **XApp:** Ohne `libxapp` (`-Dxapp=disabled`) gibt es keine Favoriten (die Menüpunkte fehlen), das Statussymbol für Dateivorgänge ist ein `GtkStatusIcon`, und im Fortschrittsfenster entfällt die Fortschrittsanzeige in der Fensterleiste.
* **cinnamon-desktop:** Ohne die Bibliothek (`-Dcinnamon_desktop=disabled`) oder ohne ihr Schema `org.cinnamon.desktop.thumbnailers` erzeugt Nolphin Vorschaubilder selbst (`libnolphin-private/nolphin-thumbnailer.c`): Bilder über gdk-pixbuf, alle anderen Typen über die installierten `*.thumbnailer`. Das Format entspricht der Freedesktop-Spezifikation, GNOME und Cinnamon nutzen dieselben Dateien in `~/.cache/thumbnails`. Programme werden ohne Shell gestartet und nach 30 Sekunden beendet. Auf Mint bleibt die bisherige Bibliothek im Einsatz.

Bauen ohne beide Bibliotheken:

```bash
meson setup build-minimal -Dcinnamon_desktop=disabled -Dxapp=disabled
ninja -C build-minimal
```

Ohne Installation und ohne eigene XFCE-/MATE-Sitzung lässt sich ein Start in einer nachgestellten Umgebung (eigene D-Bus-Sitzung, Cinnamon-/MATE-Schemas ausgeblendet) prüfen:

```bash
BUILD=build-minimal test/simulate-other-desktop.sh XFCE
```

## Teststand der Desktops (11. Oktober 2026)

Geprüft auf einem Linux-Mint-Rechner, auf dem zusätzlich XFCE und MATE installiert wurden. Alle Funktionsprüfungen liefen in einer echten Sitzung des jeweiligen Desktops mit der Variante ohne `cinnamon-desktop` und `xapp`; Testdaten lagen in eigenen Ordnern und wurden danach gelöscht.

| Prüfung | MATE | XFCE |
|---|---|---|
| Start und Oberfläche, normaler Build | ja (vom Entwickler bestätigt) | ja (vom Entwickler bestätigt) |
| Start ohne `cinnamon-desktop` und `xapp` | ja | ja |
| Vorschaubilder durch das eigene Modul | ja: 6 PNG und 1 JPG, Cache-Dateien tragen `Software: Nolphin`, 256 px | ja: dasselbe |
| Integriertes Terminal im richtigen Ordner | ja | ja |
| Kopieren mit Fortschrittsfenster | ja: 120 Dateien, 12,6 GB, Fortschritt und Titel in der Taskleiste | ja: dasselbe |
| Pause und Fortsetzen | ja: Stand blieb 7 s bei 106 Dateien, danach alle vollständig, Stichproben bitgenau gleich | ja: Stand blieb 7 s bei 51 Dateien, danach alle vollständig, Stichproben bitgenau gleich |
| Favoriten-Menüpunkte ausgeblendet | ja | ja |
| Start ohne Cinnamon-/MATE-Schemas | nachgestellt (Skript) | nachgestellt (Skript) |
| Symbole ohne `xapp-symbolic-icons` | nachgestellt: Seitenleiste, Werkzeugleiste, Menü und Kontextmenü zeigen Symbole (vorher Verbotsschilder) | nachgestellt: dasselbe |
| Statussymbol im Panel bei verstecktem Fortschrittsfenster (Build ohne `xapp`, also `GtkStatusIcon`) | nicht geprüft | ja: erscheint nach dem Schließen des Fensters, ein Klick holt es zurück, am Ende des Vorgangs verschwindet es |
| Symbol-Ersatz mit 11 installierten Symbolthemen (Adwaita, Yaru, Papirus, Papirus-Dark, elementary-xfce, mate, breeze, Mint-Y, Mint-X, Mint-L, hicolor), jeweils ohne `xsi`-Dateien | alle 86 Symbole ersetzt, ladbar und symbolisch | dasselbe |
| Aussehen im Vergleich zum Dateimanager des Desktops (Caja bzw. Thunar) | gleiches Design, gleiche Farben, Schrift, Titelleiste und Ordnersymbole | gleiches Design, gleiche Farben, Schrift, Titelleiste und Ordnersymbole |
| Terminal-Schema des laufenden Desktops hat Vorrang | Test mit den installierten Schemas (`Settings schema test`) | nicht anwendbar (XFCE hat kein eigenes Schema) |

## Bekannte Grenzen auf anderen Desktops

* Das Statussymbol im Panel wurde unter XFCE geprüft, unter MATE nicht (der Panel dort wurde nicht angesehen).
* Der Rechner hatte weiter die Cinnamon-Pakete. Ein Rechner ganz ohne sie wurde nicht getestet, nur die Nachstellung.
* Der externe Terminal-Ersatz (`x-terminal-emulator`, `mate-terminal`, `xfce4-terminal`, `gnome-terminal`, `xterm`) für Aktionen, die ein Programm in einem Terminal starten, ist eingebaut, aber nicht live ausgelöst. Im Test war `xfce4-terminal` nicht installiert.
* Der Symbol-Ersatz wurde mit den 11 oben genannten Themen geprüft. Bei anderen Themen kann für einzelne Symbole die Lücke bleiben (es wird nichts vorgetäuscht); das Werkzeug `icon-theme-coverage` zeigt es.
* Ohne `cinnamon-desktop` gibt es keine Prüfung und Reparatur der Rechte des Vorschaubild-Zwischenspeichers (`--fix-cache`).
* Ein Ordner mit 40 000 kleinen Dateien machte die Oberfläche in der Testumgebung für über eine halbe Minute zäh und fror sie zeitweise ein. Ob das nur an der Umgebung liegt, ist nicht verglichen.

---

[Weiter: Nolphin und Nemo →](nolphin-und-nemo.md)
