[Startseite](../README.md) | [Der rechte Arbeitsbereich →](arbeitsbereich.md)

# Bauen und Installieren

## Voraussetzungen

Nolphin läuft ausschließlich unter Linux und ist für Linux Mint mit Cinnamon optimiert.

* Meson (≥ 0.64), Ninja, C-Compiler (C11)
* GTK3, GLib, GIO, GVfs, VTE 2.91
* optional: XApp (`-Dxapp=disabled`), cinnamon-desktop (`-Dcinnamon_desktop=disabled`), libexif, exempi
* Laufzeitwerkzeuge, je nach Funktion: `zip`/`unzip`, `tar`, `7z`, `zstd`, `lz4`, `xz`, `git`, `gpg`, `getfacl`/`setfacl`, `rsync`, `pdfinfo`/`pdftocairo` (poppler-utils), `gst-discoverer-1.0` (gstreamer1.0-tools)

## Bauen und testen

```bash
meson setup build
ninja -C build
meson test -C build
```

## Installieren

```bash
sudo ninja -C build install
```

Installationsweg und Paketierung (`debian/`) können sich während der Entwicklung ändern.

---

[Weiter: Der rechte Arbeitsbereich →](arbeitsbereich.md)
