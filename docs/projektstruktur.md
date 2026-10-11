[Startseite](../README.md) | [← Entwicklungsstand](entwicklungsstand.md) | [Fehler melden →](fehler-melden.md)

# Projektstruktur

```text
src/                    Hauptquellcode (Fenster, Ansichten, Panels)
libnolphin-private/     interne Bibliothek (Archiv, Prüfsummen, ACL, GPG, Schemas)
libnolphin-extension/   Erweiterungs-Schnittstelle
gresources/             Beschreibungen der Oberfläche
test/                   Tests
docs/                   Referenzdokumente, Hilfetexte (docs/hilfe/), Bilder (bilder/)
debian/                 Debian-Paketierung
po/                     Übersetzungsdateien
```

Die Einstellungen liegen im GSettings-Schema `org.nolphin` (`libnolphin-private/org.nolphin.gschema.xml`).

## Weitere Dokumente

| Thema | Dokument |
| ----- | -------- |
| Drag & Drop | [dnd.txt](dnd.txt) |
| Tastatur und Maus | [key_mouse_navigation.txt](key_mouse_navigation.txt) |
| Ein- und Ausgabe | [nolphin-io.txt](nolphin-io.txt) |

---

[Weiter: Fehler melden →](fehler-melden.md)
