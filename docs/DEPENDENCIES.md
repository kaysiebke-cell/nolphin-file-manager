# Abhängigkeiten

Diese Datei wird bei jeder Änderung der Abhängigkeiten aktualisiert. Sie listet alle Bibliotheken und Werkzeuge, die Nolphin tatsächlich
verwendet - Paketnamen für Linux Mint/Ubuntu/Debian, getrennt nach Pflicht und Optional.

Stand: geprüft anhand von `meson.build`, `meson_options.txt` und aller `g_find_program_in_path()`-
Aufrufe im tatsächlichen Quellcode (nicht nur anhand der Spezifikation).

---

## Fertiger Befehl zum Installieren aller Pflicht- und Standard-Optional-Abhängigkeiten

```bash
sudo apt install \
  meson ninja-build \
  libgtk-3-dev libvte-2.91-dev \
  libxapp-dev libcinnamon-desktop-dev \   # optional, siehe Abschnitt 2
  libgail-3-dev libx11-dev libjson-glib-dev libpango1.0-dev \
  libgirepository1.0-dev gobject-introspection gtk-doc-tools intltool itstool \
  libgsf-1-dev \
  libexif-dev libexempi-dev \
  dpkg git gnupg acl coreutils zip unzip tar p7zip-full file-roller
```

---

## 1. Pflicht (Build schlägt ohne diese fehl)

Aus `meson.build` (`dependency(...)` ohne `required: false`):

| Bibliothek | Debian/Mint-Paket | Mindestversion |
|---|---|---|
| GTK3 | `libgtk-3-dev` | ≥ 3.10.0 |
| GLib/GIO/GObject/GModule | `libglib2.0-dev` | ≥ 2.45.7 |
| GObject Introspection | `libgirepository1.0-dev`, `gobject-introspection` | ≥ 1.0 |
| JSON-GLib | `libjson-glib-dev` | ≥ 1.6 |
| GAIL | `libgail-3-dev` | - |
| X11 | `libx11-dev` | - |
| VTE (integriertes Terminal) | `libvte-2.91-dev` | ≥ 0.40 |
| Pango | `libpango1.0-dev` | ≥ 1.44.0 |
| libgsf | `libgsf-1-dev` | - |
| Build-Werkzeuge | `meson`, `ninja-build`, `gtk-doc-tools`, `intltool`, `itstool` | - |

**Gelöst (Kompatibilität XFCE/MATE):** cinnamon-desktop und XApp sind jetzt Meson-Feature-
Optionen (siehe Abschnitt 2), keine Pflicht mehr.

---

## 2. Optional zur Build-Zeit (Meson-Feature-Flags)

Aus `meson_options.txt`, jeweils per `-D<option>=false` (bei `cinnamon_desktop`/`xapp`: `=disabled`) abschaltbar. Ohne beide: `meson setup build -Dcinnamon_desktop=disabled -Dxapp=disabled`.

Ohne Desktop-Schemas von Cinnamon/MATE/GNOME zur Laufzeit nutzt Nolphin eigene Ersatz-Schemas (`org.nolphin.compat.*`). Nachstellen ohne Installation: `test/simulate-other-desktop.sh`.

| Bibliothek | Debian/Mint-Paket | Option | Standard | Funktion |
|---|---|---|---|---|
| cinnamon-desktop | `libcinnamon-desktop-dev` | `cinnamon_desktop` (auto/enabled/disabled) | auto | Vorschaubild-Fabrik. Ohne sie (oder ohne ihr Schema `org.cinnamon.desktop.thumbnailers` zur Laufzeit) erzeugt Nolphin Vorschaubilder selbst (`nolphin-thumbnailer.c`: gdk-pixbuf + installierte `*.thumbnailer`) |
| XApp | `libxapp-dev` | `xapp` (auto/enabled/disabled) | auto | Favoriten, XApp-Statussymbol/-Fenster. Ohne sie: keine Favoriten (Menüpunkte ausgeblendet), GtkStatusIcon statt XApp-Statussymbol |
| xapp-symbolic-icons | `xapp-symbolic-icons` | (nur Laufzeit) | empfohlen | Symbole `xsi-*` für Menüs, Werkzeugleiste, Seitenleiste. Ohne das Paket nutzt Nolphin Standardsymbole des Symbolthemas (`nolphin-icon-fallback.c`) |
| libexif | `libexif-dev` | `exif` | an | EXIF-Daten von Bildern |
| Exempi (XMP) | `libexempi-dev` | `xmp` | an | XMP-Metadaten |
| Tracker | `libtracker-sparql-3.0-dev` (o. ä., versionsabhängig) | `tracker` | aus | Tracker-Suchindex |
| gtk-layer-shell + Wayland | `libgtk-layer-shell-dev` | `gtk_layer_shell` | aus | Desktop-Zeichnung unter Wayland |
| SELinux | `libselinux1-dev` | `selinux` | aus | SELinux-Unterstützung |

## 3. Optional zur Laufzeit (externe Werkzeuge über GSubprocess)

Diese werden nicht gelinkt, sondern nur bei tatsächlicher Nutzung per
`g_find_program_in_path()` gesucht. Fehlt eines, wird die jeweilige Funktion laut Code
ausgegraut bzw. meldet einen Fehler - nichts wird vorgetäuscht.

| Werkzeug | Debian/Mint-Paket | Genutzt für |
|---|---|---|
| `git` | `git` | Git-Integration |
| `gpg` | `gnupg` | Verschlüsselung |
| `getfacl`, `setfacl` | `acl` | ACL-Verwaltung |
| `b2sum` | `coreutils` (Standard bereits installiert) | BLAKE2-Prüfsummen |
| `zip`, `unzip` | `zip`, `unzip` | Archive (ZIP) |
| `tar` | `tar` (Standard bereits installiert) | Archive (TAR/TAR.GZ/TAR.BZ2/TAR.XZ) |
| `7z` | `p7zip-full` | Archive (7-Zip) |
| `dpkg-deb` | `dpkg` (Standard bereits installiert) | „Als .deb-Paket erstellen …" (zusätzliche Funktion) |
| `file-roller` | `file-roller` | Nur als Öffnen-Helfer bei Drag & Drop auf ein Archivsymbol und als MIME-Vorschlag - nicht die eigentliche Archiv-Engine |

Nicht in Meson gelistete, aber vorgesehene Komponenten:

- **Poppler-GLib** (PDF-Vorschau): im aktuellen Quellcode **nicht** eingebunden - FEHLT,
  nicht nur "optional deaktiviert".
- **GStreamer** (Video-/Audioinformationen): im aktuellen Quellcode **nicht** eingebunden -
  FEHLT.
- **rsync**, **libreoffice --headless**: im aktuellen Quellcode nicht als kontrollierter
  Subprozess eingebunden - FEHLT (Phase 2/erweiterte Vorschau, noch nicht umgesetzt).

Diese drei Punkte sind keine neuen Erkenntnisse dieser Änderung, sondern eine ehrliche
Bestandsaufnahme beim Erstellen dieser Datei (keine Funktion wird als
implementiert behauptet, die nicht geprüft wurde).

---

## Handy-App für Android (nur zum Bauen der App)

**Freigegeben am 11. Oktober 2026** (Entscheidung des Benutzers). Die App liegt im eigenen Projekt `nolphin-handy` und wird nur gebaut, wenn der Entwickler dort `build.sh` aufruft. Nolphin selbst braucht nichts davon; liegt die APK nicht vor, läuft der Handy-Austausch über den Browser des Handys.

NEUE ABHÄNGIGKEIT: JDK (`javac`, `keytool`), Android SDK (`android.jar`, `aapt2`, `d8`, `zipalign`, `apksigner`), `secret-tool`
WOFÜR: Bau und Signatur der Handy-App (APK), Test des QR-Lesers der App (`nolphin-handy/test.sh`), Passwort des Signaturschlüssels im Systemschlüsselbund (Abschnitt 53.4)
PFLICHT ODER OPTIONAL: optional, nur für den Bau der App; nicht Teil von Nolphin
OHNE SIE: Nolphin baut, startet und läuft unverändert; der Handy-Austausch funktioniert im Browser des Handys, nur ohne App
ALTERNATIVE OHNE NEUE ABHÄNGIGKEIT: nur der Weg über den Browser des Handys (ohne App)

| Werkzeug | Paket | Bemerkung |
|---|---|---|
| JDK | `openjdk-17-jdk-headless` (oder neuer) | `javac`, `keytool` |
| Android SDK | kein Mint-Paket (vom Entwickler selbst installiert, `ANDROID_HOME`, Standard `~/Android/Sdk`) | `platforms/android-34/android.jar`, `build-tools` mit `aapt2`, `d8`, `zipalign`, `apksigner` |
| Schlüsselbund-Werkzeug | `libsecret-tools` (`secret-tool`) | speichert das Passwort des Signaturschlüssels; `sudo apt install libsecret-tools` |
| Testwerkzeuge | `gcc`, `libglib2.0-dev` | `nolphin-handy/test.sh` übersetzt den C-Encoder für die Testcodes |
