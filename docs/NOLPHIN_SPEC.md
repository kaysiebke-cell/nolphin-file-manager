# NOLPHIN – MASTER DEVELOPMENT CONTRACT (Version 2.3)

Verbindlicher Entwicklungs-, Funktions- und Arbeitsvertrag

**Aktualisierung 2.2:** Klarstellung und Konfliktauflösung zum rechten integrierten Arbeitsbereich (siehe Abschnitt 58.0 und 58.1).
**Aktualisierung 2.3:** Nachträgliche Dokumentation der bereits implementierten Funktion „DEB-Paket erstellen" als Teil des Archiv-Panels (siehe Abschnitt 36.1). Alle übrigen Abschnitte sind gegenüber Version 2.1 inhaltlich unverändert.

---

## 0. VERBINDLICHER ARBEITSMODUS

Du arbeitest an Nolphin, einem eigenständigen nativen Linux-Dateimanager für Linux Mint. Dieser Text ist der verbindliche Entwicklungsvertrag und die vollständige Funktionsspezifikation.

Absolute Regeln:

1. Du setzt die hier beschriebenen Anforderungen technisch um.
2. Du entfernst, vereinfachst oder ignorierst keine Anforderung eigenständig.
3. Du erfindest keine bereits implementierten Funktionen.
4. Du behauptest nie, dass eine Funktion fertig ist, wenn sie nicht tatsächlich implementiert und getestet wurde.
5. Du triffst keine eigenständigen Produktentscheidungen, wenn eine Anforderung eindeutig vorgegeben ist.
6. Bei echten technischen oder fachlichen Konflikten meldest du den Konflikt (Format siehe Abschnitt 58), statt selbst eine Anforderung zu streichen.
7. Bestehenden Code untersuchst du vor jeder Änderung.
8. Du überschreibst nie blind komplette Dateien, ohne deren Struktur und Funktion zu analysieren.
9. Nach einer Analyse wartest du auf meine Freigabe, bevor du größere Änderungen implementierst.
10. Jede Änderung muss einer konkreten Anforderung zugeordnet werden können.
11. Build, Installation und Funktionstests gehören zur Implementierung und dürfen nicht übersprungen werden.
12. Keine Pseudofunktionen, Fake-APIs, Platzhalter oder vorgetäuschten Implementierungen.
13. Ist eine Funktion wegen einer fehlenden Systemkomponente nicht verfügbar, wird dies technisch korrekt erkannt und dem Benutzer angezeigt.
14. Der Funktionsumfang darf nicht wegen einer bevorzugten Architektur künstlich reduziert werden.
15. Du arbeitest die Phasen aus Abschnitt 7 der Reihe nach ab. Eine spätere Phase beginnt erst nach meiner Freigabe.

---

## 1. ROLLE

Du arbeitest gleichzeitig als Lead Software Architect, Senior C Developer, Senior GTK3 Developer, GLib/GObject/GIO Developer, Linux-Systemintegrationsentwickler, Dateisystem- und Dateimanager-Entwickler, UI/UX-Entwickler, Build- und Release-Engineer, Test- und Qualitätssicherungsingenieur sowie als technischer Recherche-Assistent für Dateien, Bilder, 3D- und CAD-Formate.

Du entwickelst produktionsfähigen Code, keine Demonstration.

---

## 2. PRODUKTZIEL

Nolphin wird ein vollständiger, nativer Linux-Dateimanager:

- installierbar, stabil, performant, GTK3-nativ
- für Linux Mint (Cinnamon) optimiert, auf anderen GTK-basierten Linux-Desktops lauffähig
- als Standard-Dateimanager verwendbar
- umfangreiche Datei-, Verzeichnis-, Geräte-, Netzwerk-, Such-, Vorschau-, Archiv- und Systemfunktionen
- vollständig über die grafische Oberfläche bedienbar
- Tastatur- und Mausbedienung, Drag & Drop, Kontextmenüs
- Tabs, Split-Ansichten, Vorschau, integriertes Terminal
- konfigurierbare Einstellungen, erweiterbare Aktionen

Nolphin läuft ausschließlich unter Linux.

Die Benutzeroberfläche orientiert sich an der von mir bereitgestellten Referenz und an einer klassischen, übersichtlichen Linux-Dateimanager-Bedienung. Du darfst die Oberfläche nicht eigenständig in ein völlig anderes Produktdesign umwandeln. Ist keine Referenz im Projekt auffindbar, meldest du das in der Analyse.

---

## 3. TECHNISCHE GRUNDLAGE

Nolphin wird ausschließlich mit den Mitteln gebaut, die auch der Linux-Mint-Dateimanager Nemo verwendet bzw. die unter Linux Mint standardmäßig verfügbar sind. Nemo dient ausschließlich als technische Orientierung dafür, welche Bibliotheken, Systemdienste, APIs und Standards unter Linux Mint sinnvoll und verfügbar sind. Es wird kein Code aus Nemo oder anderen Projekten kopiert. Die Oberfläche richtet sich ausschließlich nach meiner bereitgestellten Referenz und den Anforderungen dieses Entwicklungsvertrags, nicht nach der Oberfläche von Nemo.

Nolphin ist eine vollständig eigenständige Anwendung. Es besteht keine Abhängigkeit zu Nemo, zu Nemo-Paketen, zu Nemo-Erweiterungsbibliotheken oder zu Nemo-Konfigurationsdateien. Nolphin darf keine Dateien, Einstellungen oder Konfigurationen anderer Dateimanager verändern.

### Sprache und Build

* C11
* Meson
* Ninja

### Kernbibliotheken

* GTK3
* GLib
* GObject
* GIO
* GVFS
* GdkPixbuf
* VTE 2.91 für das integrierte Terminal

Diese Bibliotheken bilden die technische Grundlage der Kernanwendung. Die Kernfunktionen von Nolphin dürfen nicht von Nemo oder Nemo-spezifischen Bibliotheken abhängig sein.

### Eigenständigkeit und Abgrenzung

Nolphin muss als eigenständige Linux-Anwendung entwickelt und betrieben werden.

Insbesondere gilt:

* Keine Abhängigkeit zu Nemo.
* Keine Verwendung von Nemo-Konfigurationsdateien.
* Keine Verwendung von Nemo-Erweiterungsbibliotheken.
* Keine Kommunikation mit Nemo, um grundlegende Nolphin-Funktionen bereitzustellen.
* Keine Übernahme von Nemo-Laufzeitdaten oder Nemo-internen UI-Einstellungen.
* Keine Veränderung von Dateien oder Einstellungen anderer Dateimanager.
* Keine Nachbildung von Nemo-internen APIs, wenn dafür eine standardisierte Linux-, GTK-, GLib-, GIO-, XDG- oder Freedesktop-Schnittstelle vorhanden ist.

Nemo darf ausschließlich als technische Referenz dafür dienen, welche unter Linux Mint verfügbaren Systemmechanismen für eine bestimmte Funktion grundsätzlich geeignet sind.

---

### Optionale Mint-Komponenten

Folgende Komponenten dürfen nur optional verwendet werden. Ihr Einsatz muss jeweils technisch begründet werden.

Nolphin muss ohne jede dieser Komponenten kompilieren, starten und alle Kernfunktionen ausführen können. Fehlt eine optionale Komponente, wird ausschließlich die davon abhängige Zusatzfunktion deaktiviert und der Grund technisch korrekt angezeigt.

* `libxapp` für XApp-Favoriten und XApp-Hilfsfunktionen
* `cinnamon-desktop` für Thumbnail-Erzeugung nach dem Freedesktop-Standard

  * ohne `cinnamon-desktop` liest Nolphin vorhandene Vorschaubilder aus `~/.cache/thumbnails`
  * zusätzlich ruft Nolphin installierte Freedesktop-Thumbnailer selbst auf, sofern diese vorhanden und verwendbar sind
* `libexif` für EXIF-Daten von Bildern
* `GStreamer` für Video- und Audioinformationen
* `Poppler-GLib` für PDF-Vorschauen

Eine optionale Komponente darf niemals zu einer künstlichen Abhängigkeit der gesamten Anwendung werden.

---

### Desktop-Integration und Systemdarstellung

Nolphin übernimmt die Darstellung und relevante Benutzereinstellungen des vorhandenen Linux-Desktops über standardisierte GTK-, GLib-, GSettings-, XDG- und Freedesktop-Schnittstellen.

Nolphin soll sich auf Linux Mint/Cinnamon wie eine native GTK-Anwendung in den vorhandenen Desktop einfügen, ohne selbst Bestandteil von Cinnamon oder Nemo zu werden.

Die Desktop-Integration muss deshalb über eine klar getrennte interne Desktop-Bridge beziehungsweise eine vergleichbare Abstraktionsschicht erfolgen.

Der Nolphin-Kern darf nicht direkt von Cinnamon-spezifischen Implementierungsdetails abhängig sein.

Die Desktop-Bridge ist dafür verantwortlich, standardisierte Informationen des Betriebssystems und der Desktop-Umgebung bereitzustellen, insbesondere:

* Sprache und Locale
* Übersetzungsumgebung
* GTK-Theme
* Icon-Theme
* Systemschrift
* relevante GTK-Schrifteinstellungen
* Skalierung und Darstellung
* relevante standardisierte Desktop-Einstellungen
* weitere standardisierte Darstellungseinstellungen, sofern GTK, GLib, GSettings, XDG oder Freedesktop diese bereitstellen

Die Kernanwendung verwendet ausschließlich die von dieser Abstraktionsschicht bereitgestellten Informationen und darf keine Desktop-spezifischen Konfigurationsdateien direkt auswerten, wenn dafür eine standardisierte API vorhanden ist.

---

### Sprache und Internationalisierung

Die Sprache der Anwendung wird nicht fest auf Deutsch oder Englisch vorgegeben.

Die Sprache wird anhand der System-/Benutzer-Locale ermittelt und über die Nolphin-Übersetzungsinfrastruktur umgesetzt.

Alle sichtbaren und übersetzbaren Texte müssen über die Übersetzungsinfrastruktur laufen.

Beispielsweise müssen sichtbare Texte über gettext beziehungsweise die dafür vorgesehene Nolphin-Internationalisierung verarbeitet werden:

```c
_("Datei")
_("Bearbeiten")
_("Öffnen")
_("Eigenschaften")
```

Texte dürfen nicht dauerhaft fest auf Deutsch, Englisch oder eine andere Sprache programmiert werden, wenn sie dem Benutzer angezeigt werden.

Die Sprachkette muss grundsätzlich nach folgendem Prinzip funktionieren:

```text
System-/Benutzer-Locale
        ↓
GLib / Locale
        ↓
gettext / Nolphin-Übersetzungen
        ↓
Nolphin-Benutzeroberfläche
```

Wenn für eine Sprache keine Nolphin-Übersetzung vorhanden ist, wird ein definierter Fallback verwendet.

Nolphin darf die vom Benutzer beziehungsweise System vorgegebene Sprache nicht ohne ausdrückliche Nolphin-Einstellung überschreiben.

---

### GTK-Theme und Layout-Integration

Nolphin verwendet das aktive GTK-Theme des Systems.

Die Anwendung darf nicht dauerhaft ein eigenes GTK-Theme erzwingen, wenn der Benutzer ein anderes Systemtheme ausgewählt hat.

Relevante GTK-Systemeinstellungen sollen über die vorgesehenen GTK-/GLib-Schnittstellen übernommen werden.

Dazu gehören insbesondere:

* aktiver GTK-Stil
* Systemschrift
* relevante Schriftgrößen
* relevante GTK-Darstellungseinstellungen
* relevante Skalierungsinformationen
* weitere von GTK bereitgestellte Darstellungseinstellungen

Nolphin darf diese Einstellungen nur überschreiben, wenn eine ausdrücklich definierte Nolphin-Einstellung dies verlangt.

Das interne Nolphin-Layout darf die fachlich definierte Oberfläche und Benutzerführung bestimmen. Es darf jedoch nicht versuchen, das Linux-Mint-Theme oder andere Desktop-Themes nachzubauen.

Die Anwendung soll GTK-Widgets und standardisierte GTK-Darstellungsmechanismen verwenden, damit sich die Darstellung automatisch an das aktive Systemtheme anpasst.

---

### Icons und Icon-Theme

Nolphin verwendet grundsätzlich das aktive System-Icon-Theme.

Standardicons müssen über die GTK-/Freedesktop-Icon-Theme-Mechanismen anhand semantischer Icon-Namen angefordert werden.

Beispielsweise soll Nolphin nicht dauerhaft eine eigene PNG-Datei für einen Ordner erzwingen, wenn das System ein entsprechendes Standardicon bereitstellt.

Das grundsätzliche Verhalten ist:

```text
Nolphin benötigt semantisches Icon
            ↓
GTK / System-Icon-Theme
            ↓
passendes Systemicon vorhanden?
       ┌──────────────┐
       │              │
      Ja             Nein
       │              │
       ↓              ↓
System-Icon      Nolphin-Fallback
```

Eigene Nolphin-Icons dürfen verwendet werden:

* wenn kein passendes standardisiertes Systemicon existiert
* wenn es sich um ein eindeutig Nolphin-spezifisches UI-Element handelt
* als definierter Fallback, wenn das benötigte Systemicon nicht verfügbar ist

Eigene eingebettete Icons dürfen jedoch nicht dazu verwendet werden, das aktive System-Icon-Theme grundsätzlich zu ersetzen.

Nolphin darf kein eigenes vollständiges Linux-Mint- oder Cinnamon-Icon-Theme mitbringen, um das Systemtheme nachzubauen.

Das Ziel ist:

```text
Linux Mint Icon Theme
        ↓
GTK Icon Theme
        ↓
Nolphin
```

und nicht:

```text
Linux Mint Icon Theme
        X
        ↓
Nolphin eigenes Icon-System
```

---

### Desktop-Bridge und Fallback-Verhalten

Die Desktop-Integration muss nach folgendem Prioritätsprinzip funktionieren:

```text
1. Standardisierte System-/GTK-Schnittstelle
                  ↓
2. Standardisierte GLib-/GSettings-/XDG-/Freedesktop-Schnittstelle
                  ↓
3. Optionale Desktop-spezifische Erweiterung
                  ↓
4. Nolphin-definierter Fallback
```

Eine Cinnamon-spezifische Schnittstelle darf nur als optionale Erweiterung eingesetzt werden.

Fehlt Cinnamon beziehungsweise eine Cinnamon-spezifische Bibliothek, muss Nolphin weiterhin als eigenständige GTK-Anwendung funktionieren.

Wenn eine Information über eine optionale Desktop-Komponente nicht verfügbar ist, darf Nolphin nicht abbrechen oder eine nicht vorhandene Funktion vortäuschen. Stattdessen wird die nächstliegende standardisierte Schnittstelle verwendet und anschließend, falls notwendig, ein definierter Fallback.

Damit muss Nolphin beispielsweise auch auf einem anderen GTK-basierten Linux-Desktop grundsätzlich starten und seine Kernfunktionen ausführen können.

---

### Standards

Nolphin verwendet nach Möglichkeit vorhandene Linux-/Freedesktop-/XDG-Standards anstelle eigener proprietärer Mechanismen.

Dazu gehören:

* Freedesktop/XDG für Benutzerverzeichnisse
* Freedesktop/XDG für MIME-Typen
* Freedesktop/XDG für Desktop-Dateien
* Freedesktop/XDG für Papierkorb
* Freedesktop/XDG für Thumbnails
* Freedesktop-Icon-Theme-Mechanismen
* GTK-Lesezeichendatei für Orte
* GVFS-Metadaten (`metadata::`-Attribute) für Tags, Bewertungen, Kommentare und Emblems
* GSettings/dconf für Anwendungseinstellungen
* GAppInfo für Anwendungen und „Öffnen mit“
* GIO/GVFS für lokale und entfernte Dateien sowie Geräte
* GTK/GDK für Benutzeroberfläche, Eingaben und Zwischenablage
* GNotification für Desktop-Benachrichtigungen

Eigene Implementierungen dürfen nur verwendet werden, wenn keine geeignete standardisierte Schnittstelle vorhanden ist oder die konkrete Nolphin-Funktion eine zusätzliche eigene Logik benötigt.

---

### Vorschaubilder

Die Thumbnail-Architektur muss ebenfalls möglichst unabhängig von einer einzelnen Desktop-Implementierung sein.

Priorität:

```text
vorhandenes Freedesktop-Thumbnail
        ↓
installierter Freedesktop-Thumbnailer
        ↓
optionale cinnamon-desktop-Unterstützung
        ↓
Nolphin-eigene Unterstützung nur wenn technisch erforderlich
        ↓
keine Vorschau, wenn kein Backend verfügbar
```

Nolphin darf keine Vorschau vortäuschen.

Wenn ein erforderliches Backend fehlt, wird dies technisch korrekt erkannt und dem Benutzer angezeigt.

---

### Systemwerkzeuge

Folgende Systemwerkzeuge dürfen unter Linux Mint vorhanden sein oder dort üblich nachinstalliert werden und dürfen ausschließlich über kontrollierte Subprozesse gemäß Abschnitt 53.5 verwendet werden:

* `file-roller`
* `tar`
* `gzip`
* `bzip2`
* `xz`
* `zstd`
* `lz4`
* `zip`
* `unzip`
* `7z`
* `unrar`
* `rsync`
* `gpg`
* `git`
* `getfacl`
* `setfacl`
* `b2sum`
* `libreoffice --headless`
* `dconf`
* `dpkg-deb` (Bestandteil des Basispakets `dpkg`, auf jedem Debian-/Ubuntu-basierten System inkl. Linux Mint vorinstalliert) – für „DEB-Paket erstellen" (Abschnitt 36.1)

Ihre Verfügbarkeit muss vor der Verwendung geprüft werden.

Fehlt ein Werkzeug, darf Nolphin die davon abhängige Funktion nicht als verfügbar darstellen.

Die übrige Anwendung muss weiterhin funktionieren.

---

### Nicht verwenden

Nicht verwenden:

* Qt
* QML
* KDE-Bibliotheken jeglicher Art
* Nemo als Laufzeitabhängigkeit
* Nemo-Bibliotheken
* Nemo-Konfigurationsdateien
* Nemo-Erweiterungsbibliotheken
* proprietäre oder nicht standardisierte Desktop-Integrationsmechanismen, wenn eine geeignete GTK-, GLib-, GIO-, GSettings-, XDG- oder Freedesktop-Schnittstelle vorhanden ist

---

### Neue Bibliotheken und Werkzeuge

Jede weitere Bibliothek oder jedes weitere externe Werkzeug darf nur nach technischer Begründung und meiner ausdrücklichen Freigabe verwendet werden.

Dabei muss dokumentiert werden:

1. Welche konkrete Funktion benötigt die Bibliothek beziehungsweise das Werkzeug?
2. Warum kann diese Funktion nicht mit den bereits freigegebenen Mitteln umgesetzt werden?
3. Ist die Abhängigkeit zwingend oder optional?
4. Wie verhält sich Nolphin, wenn die Abhängigkeit nicht installiert ist?
5. Welche Auswirkungen hat die Abhängigkeit auf Kompilierung, Installation und Laufzeit?
6. Ist die Abhängigkeit unter Linux Mint verfügbar beziehungsweise standardmäßig oder üblich installierbar?

---

### Grundsatz

Ist eine Funktion mit den freigegebenen Mitteln nicht zuverlässig und technisch korrekt umsetzbar, wird sie nicht nachgebaut, vorgetäuscht oder durch eine ungeprüfte Drittanbieterabhängigkeit ersetzt.

Stattdessen wird der technische Konflikt gemäß dem dafür vorgesehenen Abschnitt dieses Entwicklungsvertrags gemeldet.

Nolphin muss jederzeit klar zwischen:

* eigener Anwendungslogik,
* standardisierter Linux-/GTK-Systemintegration,
* optionaler Desktop-Integration,
* optionalen externen Komponenten
* und nicht verfügbaren Funktionen

unterscheiden können.

Das übergeordnete Ziel lautet:

**Nolphin ist eine eigenständige native Linux-Anwendung, die sich automatisch an Sprache, Theme, Schrift, Skalierung, Icons und relevante Darstellungseinstellungen des vorhandenen GTK/Linux-Desktops anpasst, ohne von Nemo abhängig zu sein und ohne einen bestimmten Desktop durch eigene Implementierungen nachzubauen.**

---

## 4. TECHNISCHE ZUORDNUNG

| Funktion | Technische Umsetzung |
|---|---|
| Lokale und entfernte Dateien, Netzwerk | GIO/GVFS |
| Papierkorb | GIO (trash://) |
| Zuletzt verwendet | GVFS (recent://) bzw. GtkRecentManager |
| Orte/Lesezeichen | GTK-Lesezeichendatei (~/.config/gtk-3.0/bookmarks) |
| Favoriten (Dateien und Ordner) | XApp-Favoriten (libxapp) |
| Dateizuordnungen, Öffnen mit | XDG-MIME, GAppInfo |
| Tags, Bewertung, Kommentare, Emblems | GVFS-Metadaten |
| Vorschaubilder | Freedesktop-Thumbnailer über cinnamon-desktop |
| Bilder | GdkPixbuf, libexif |
| PDF | Poppler-GLib |
| Video/Audio-Informationen | GStreamer |
| Geräte, Einbinden, LUKS-Entsperren | GIO GVolumeMonitor/GMount, GVFS/udisks |
| Zugangsdaten | GtkMountOperation mit Systemschlüsselbund |
| Benachrichtigungen | GNotification |
| Zwischenablage | GTK/GDK |
| Terminal | VTE 2.91 |
| Dateiüberwachung | GFileMonitor |
| Prüfsummen | GChecksum, BLAKE2 über b2sum |
| Archive | file-roller bzw. Systemwerkzeuge |
| Synchronisation | rsync |
| Verschlüsselung | gpg |
| Einstellungen | GSettings/dconf |
| Plugins | GModule |

---

## 5. ENTWICKLUNGSZYKLUS UND ARBEITSWEISE

Für jede größere Änderung gilt:

Gesamtziel erfassen → Dateien/Projekt prüfen → Bilder/Referenzen prüfen → relevante Anforderungen identifizieren → technische Analyse → Architektur → Freigabe → Implementierung → Build → Test → Nachweis

Nicht: Anforderung → sofort Code schreiben.

Gesamtbild vor Detailarbeit:

- Unterscheide zwischen Gesamtziel, aktueller Phase, aktueller Aufgabe, aktuellem Arbeitsschritt, relevanten Anforderungen und späteren Anforderungen.
- Der gesamte Vertrag bleibt verbindlich, aber nicht jede Anforderung ist für jeden Arbeitsschritt relevant. Eine einfache, konkrete Aufgabe darf nicht durch eine unnötige Vollanalyse aller Anforderungen verzögert werden.
- Bei jeder Aufgabe zuerst klären: Was soll am Ende tatsächlich funktionieren? Danach: Welche Dateien, Referenzen, Komponenten und Detailanforderungen werden dafür gebraucht? Erst dann beginnt die Detailarbeit.

Gesamtbild behalten + relevante Details korrekt bearbeiten + keine Anforderung eigenmächtig entfernen. Alle drei gelten gleichzeitig.

---

## 6. ERSTANALYSE (STARTBEFEHL)

Beim ersten Zugriff auf das Projekt veränderst du keinen Code. Du führst ausschließlich folgende Schritte aus:

1. Projektstruktur und vorhandene Dateien untersuchen
2. vorhandenen C-Code, GTK- und GObject-Struktur untersuchen
3. Architektur und Module erfassen
4. Build-System (Meson), Desktop-Datei, Icons, GSettings-Schema, Übersetzungen, Installationsstruktur untersuchen
5. vorhandene UI erfassen: Navigation, Dateiansicht, Tabs, Split View, Seitenleisten, Vorschau, Terminal, Menüs, Kontextmenüs, Einstellungen
6. vorhandene Tests erfassen
7. vorhandene Bilder, Referenzen, technische Dateien und 3D-/CAD-Dateien erfassen
8. technische Abhängigkeiten prüfen, insbesondere welche Komponenten aus Abschnitt 3 im Build-System bereits vorhanden sind
9. STL-/STEP-/STP-/FCStd-Unterstützung und vorhandene bzw. fehlende 3D-/CAD-Backends feststellen
10. Bild- und Dateirecherchebedarf bestimmen
11. Gap-Analyse erstellen (Abschnitt 6.1), gegliedert nach den Phasen aus Abschnitt 7
12. Architekturprobleme nennen
13. Konflikte nennen
14. Implementierungsreihenfolge innerhalb von Phase 1 vorschlagen

Danach: STOPP. Warte auf meine ausdrückliche Freigabe. Erst danach darf Code geändert werden.

### 6.1 Gap-Analyse

Für jede Funktion ist ausschließlich einer dieser Status erlaubt: IMPLEMENTIERT, TEILWEISE, FEHLT, UNBEKANNT.

| Anforderung | Phase | Status | Datei/Modul | Bemerkung |
|---|---|---|---|---|

Keine Funktion wird als IMPLEMENTIERT bezeichnet, solange dies nicht am tatsächlichen Projekt geprüft und getestet wurde.

---

## 7. PHASEN

Die Anforderungen werden in drei Phasen umgesetzt. Alle Phasen sind verbindlich; die Reihenfolge legt nur fest, was zuerst gebaut wird.

- **Phase 1 – Kern:** Abschnitte 11 bis 32 (Oberfläche, Tastatur, Navigation, Orte, Tabs, Split View, Terminal, Ansicht, Auswahl, Datei- und Bearbeiten-Funktionen, Kopieren/Verschieben, Löschen/Papierkorb, Öffnen, Kontextmenü, Drag & Drop, Zwischenablage, Eigenschaften, Berechtigungen, Filter/Suche Grundfunktionen, Vorschau Grundfunktionen, Einstellungen, Systemintegration)
- **Phase 2 – Erweitert:** Abschnitte 33 bis 41 (erweiterte Suche, erweiterte Vorschau, Metadaten und Tags, Archive, Netzwerk, Geräte und Dateisystem, Prüfsummen, Verschlüsselung, Git, Arbeitsbereiche)
- **Phase 3 – Zusatzfunktionen:** Abschnitte 42 bis 48 (3D/CAD, Aktionen und Automatisierung, Synchronisation, Versionierung, Duplikaterkennung, Plugins, Verwaltung und Diagnose)

Die Architektur von Phase 1 muss so angelegt sein, dass Phase 2 und 3 ohne Umbau des Hauptprogramms ergänzt werden können.

---

## 8. ÄNDERUNGSPROTOKOLL

Jede Änderung wird dokumentiert:

- REQUIREMENT: Welche Anforderung wird umgesetzt?
- DATEI: Welche Datei(en) werden geändert?
- ÄNDERUNG: Was wurde konkret geändert?
- WARUM: Warum ist die Änderung erforderlich?
- TEST: Wie wurde die Funktion getestet?

---

## 9. PROJEKTSTRUKTUR

Mindestens: `src/`, `data/`, `data/icons/`, `tests/`, `docs/`

Mögliche Module (jeweils .c/.h):

nolphin-main, nolphin-window, nolphin-file-view, nolphin-navigation, nolphin-tabs, nolphin-split-view, nolphin-sidebar, nolphin-preview, nolphin-preview-3d, nolphin-panel, nolphin-terminal, nolphin-search, nolphin-properties, nolphin-file-operations, nolphin-operation-queue, nolphin-device-manager, nolphin-network, nolphin-archive, nolphin-cad, nolphin-settings, nolphin-actions, nolphin-plugins, nolphin-metadata, nolphin-workspace, nolphin-sync, nolphin-versions

Die Modulaufteilung darf verbessert werden, wenn es technisch sinnvoller ist. Funktionalität darf dabei nicht entfernt werden.

Die Module `nolphin-properties`, `nolphin-terminal`, `nolphin-search` und `nolphin-archive` stellen jeweils ein Panel-Widget bereit, das von `nolphin-panel` (Panel-Engine) in den rechten Arbeitsbereich eingebunden wird; keines dieser Module implementiert seine Hauptfunktion als eigenes Top-Level-Fenster (siehe Abschnitt 58.1).

---

## 10. KERNARCHITEKTUR

Nolphin besteht aus getrennten Engines:

- **Dateisystem-Engine:** lokale Dateien, entfernte Dateien, virtuelle Ressourcen (alles über GIO/GVFS)
- **Operations-Engine:** Kopieren, Verschieben, Löschen, Umbenennen, Wiederherstellen, mit Warteschlange und Rückgängig-Verlauf
- **Such-Engine:** Dateisuche, Inhaltssuche, Filter (ohne eigenen Hintergrund-Index)
- **Vorschau-Engine:** Vorschau-Erzeugung, Thumbnail-System, Metadaten-Auslesen
- **Panel-Engine:** Verwaltung des integrierten rechten Arbeitsbereichs und seiner Funktionspanels
- **Plugin-Engine:** Laden und Verwalten von Plugins und Aktionen

Die Panel-Engine ist eine eigene UI-Schicht zwischen Hauptfenster und den einzelnen Funktionsmodulen. Sie stellt einen gemeinsamen rechten Arbeitsbereich bereit, in dem Vorschau, Informationen und interaktive Funktionen dargestellt werden können. Für Vorschau/Informationen, Eigenschaften, Terminal, Suche und Archiv ist diese Einbindung ausnahmslos verbindlich – keine Interpretation im Einzelfall (verbindliche Fassung: Abschnitt 58.1).

---

# PHASE 1 – KERN

## 11. FENSTER UND BENUTZEROBERFLÄCHE

- Hauptfenster: Menüleiste, Werkzeugleiste, Adressleiste, Hauptansicht, Seitenleiste, Statusleiste
- Panels: Orte, Ordnerbaum, Informationen/Vorschau, Terminal
- mehrere Fenster gleichzeitig möglich

Werkzeugleiste: Zurück, Vorwärts, Übergeordnet, Startseite, Aktualisieren, Suchen, Neuer Tab, Split, Ansichtsmodus, Einstellungen. Buttons spiegeln den tatsächlichen Zustand wider.

UI-Regeln: logisch, konsistent, übersichtlich, schnell verständlich, tastatur- und mausbedienbar, skalierbar. Keine redundanten oder widersprüchlichen Bedienkonzepte. Verändert eine UI-Änderung die Referenz wesentlich: dokumentieren und meine Freigabe einholen.

### 11.1 Menüleiste

Alle Funktionen von Nolphin werden in der eigenen Menüleiste von Nolphin untergebracht. Die Menüleiste von Nolphin hat genau diese sechs Hauptmenüs, in dieser Reihenfolge:

**Datei – Bearbeiten – Ansicht – Gehe zu – Lesezeichen – Hilfe**

Es werden keine weiteren Hauptmenüs angelegt. Jede Funktion dieses Vertrags, die über die Oberfläche bedienbar ist, muss in der Menüleiste von Nolphin erreichbar sein – auch wenn sie zusätzlich über Tastenkürzel, Werkzeugleiste oder Kontextmenü erreichbar ist. Zusätzliche Funktionen kommen in Untermenüs (▸).

Regeln:

- Jeder Menüeintrag ist mit derselben GAction verbunden wie das zugehörige Tastenkürzel, der Werkzeugleisten-Button und der Kontextmenü-Eintrag. Eine Funktion wird nur einmal implementiert.
- Tastenkürzel werden im Menü neben dem Eintrag angezeigt.
- Ein Menüeintrag wird erst hinzugefügt, wenn die Funktion tatsächlich implementiert ist. Keine leeren oder funktionslosen Einträge für spätere Phasen.
- Einträge, die im aktuellen Zustand nicht möglich sind (z. B. Einfügen bei leerer Zwischenablage), werden ausgegraut.
- Einträge, deren Systemkomponente fehlt (z. B. git, gpg, rsync), werden ausgegraut und zeigen beim Überfahren mit der Maus den Grund an.
- Die Menüleiste ist ein-/ausblendbar; bei ausgeblendeter Menüleiste blendet Alt sie vorübergehend ein.

**Datei**

- Neues Fenster
- Neuer Tab (Strg+T)
- Neuer Ordner (Strg+Shift+N)
- Neue Datei ▸ Leere Datei, Vorlagen aus ~/Vorlagen
- Verknüpfung erstellen ▸ Verknüpfung, Symbolische Verknüpfung, Hardlink
- Öffnen (Enter)
- Öffnen mit ▸ empfohlene Anwendungen, Andere Anwendung …
- In neuem Tab öffnen
- In neuem Bereich öffnen
- Mit Server verbinden …
- Arbeitsbereiche ▸ Speichern …, Laden, Duplizieren, Verwalten …
- Eigenschaften (Alt+Enter)
- Tab schließen (Strg+W)
- Fenster schließen
- Alle Fenster schließen / Beenden

**Bearbeiten**

- Rückgängig (Strg+Z), Wiederholen (Strg+Shift+Z)
- Ausschneiden (Strg+X), Kopieren (Strg+C), Einfügen (Strg+V)
- Zwischenablage als Datei einfügen
- Kopieren als ▸ Pfad, Dateiname
- Zwischenablage ▸ Inhalt anzeigen, Leeren
- Duplizieren
- Umbenennen (F2)
- Massenumbenennung …
- In den Papierkorb verschieben (Entf)
- Endgültig löschen (Shift+Entf)
- Alles auswählen (Strg+A), Auswahl aufheben, Auswahl umkehren
- Nach Kriterien auswählen …
- Auswahl ▸ Auswahl speichern …, Gespeicherte Auswahl wiederherstellen
- Archiv ▸ Komprimieren …, Hier entpacken, Entpacken nach …, Archiv testen
- Sicherheit ▸ Prüfsumme berechnen …, Verschlüsseln …, Entschlüsseln, Berechtigungen und ACL …
- Metadaten ▸ Tags bearbeiten …, Bewertung, Kommentar …, Emblem
- Werkzeuge ▸ Ordner vergleichen / Synchronisieren …, Duplikate finden …, Regeln anwenden …, Stapelverarbeitung …, Versionen ▸ (Version speichern, Versionen anzeigen …)
- Git ▸ Status, Hinzufügen, Commit …, Pull, Push, Log, Diff
- Aktionen ▸ eigene Aktionen
- Skripte ▸ Skripte aus dem Skriptordner, Skriptordner öffnen
- Plugins …
- Einstellungen

**Ansicht**

- Stopp
- Neu laden (F5)
- Seitenleiste ▸ Orte, Ordnerbaum, Ausblenden (F9)
- Informationsbereich (F11)
- Terminal (F4)
- Werkzeugleiste, Statusleiste, Menüleiste (jeweils ein/aus)
- Geteilte Ansicht ▸ Vertikal teilen (F3), Horizontal teilen (Shift+F3), Bereich duplizieren, Bereich maximieren, Bereich schließen
- Filterleiste (Strg+I)
- Versteckte Dateien anzeigen (Strg+H)
- Ansichtsmodus ▸ Symbole, Liste, Kompakt, Galerie
- Sortieren nach ▸ Name, Größe, Typ, Datum, Besitzer, Erweiterung, Umgekehrte Reihenfolge
- Gruppieren nach ▸ Keine, Name, Typ, Datum, Größe, Erweiterung
- Sichtbare Spalten …
- Vergrößern (Strg++), Verkleinern (Strg+-), Normale Größe (Strg+0)

**Gehe zu**

- Übergeordneter Ordner (Alt+↑)
- Zurück (Alt+←), Vorwärts (Alt+→)
- Verlauf ▸ zuletzt besuchte Ordner, Verlauf löschen
- Persönlicher Ordner (Alt+Pos1)
- Computer
- Dateisystem (/)
- Vorlagen
- Zuletzt verwendet
- Häufig verwendet
- Favoriten
- Netzwerk
- Papierkorb
- Ort eingeben … (Strg+L)
- Suchen … (Strg+F)
- Tabs ▸ Nächster Tab (Strg+Tab), Vorheriger Tab (Strg+Shift+Tab), Geschlossenen Tab wiederherstellen (Strg+Shift+T), Tab sperren, Tab umbenennen, Alle Tabs schließen

**Lesezeichen**

- Lesezeichen hinzufügen (Strg+D)
- Lesezeichen bearbeiten …
- Zu Favoriten hinzufügen
- Favoriten verwalten …
- danach: Liste aller Lesezeichen

**Hilfe**

- Tastenkürzel
- Diagnose ▸ Protokolle anzeigen, Fehlerbericht erstellen, Systeminformationen, Plugin-Status
- Über Nolphin

Einstellungen exportieren, importieren und zurücksetzen befinden sich im Einstellungsdialog (Bearbeiten ▸ Einstellungen).

Weicht diese Menüstruktur an einer Stelle von der Referenz ab oder ist eine Einordnung technisch unsinnig, meldest du das als Konflikt, statt die Struktur eigenständig zu ändern.

### 11.2 RECHTER INTEGRIERTER ARBEITSBEREICH

Der bisherige rechte Informations-/Vorschaubereich wird zu einem **integrierten rechten Arbeitsbereich** erweitert. Er ist ein zentraler Bestandteil des Hauptfensters und dient nicht nur der passiven Vorschau, sondern auch der interaktiven Bearbeitung von Funktionen. Die Bedienlogik orientiert sich dabei am Grundprinzip moderner Office-Anwendungen: Die Hauptansicht bleibt sichtbar, während eine ausgewählte Funktion in einem festen Arbeitsbereich geöffnet wird.

Grundaufbau:

┌─────────────────────────────────────────────────────────────────────────┐
│ Menü / Werkzeugleiste / Adresse                                         │
├───────────────┬──────────────────────────────────┬──────────────────────┤
│ Seitenleiste  │ Hauptansicht / Dateiansicht      │ Arbeitsbereich       │
│ Orte / Baum   │ Dateien und Ordner               │ Vorschau             │
│               │                                  │ Eigenschaften        │
│               │                                  │ Archiv               │
│               │                                  │ Terminal             │
│               │                                  │ Suche / Aktionen     │
│               │                                  │ Kompriemieren        │ 
│               │                                  │ gid                  │
│               │                                  │ deb-packet-erstellen │ 
│               │                                  │                      │          
├───────────────┴──────────────────────────────────┴──────────────────────┤
│ Statusleiste                                                            │
└─────────────────────────────────────────────────────────────────────────┘
```
```

Die Aufteilung in Pflicht-Panels und geplante Panels ist in Abschnitt 58.1 verbindlich geregelt. Die DEB-Paket-Erstellung ist Teil des Archiv-Panels (siehe Abschnitt 36.1). Weitere, in diesem Vertrag nicht spezifizierte Zusatz-Panels sind kein Bestandteil des aktuellen Funktionsumfangs (siehe Abschnitt 58.1.5).

Der rechte Arbeitsbereich muss:

- ein- und ausblendbar sein (F11)
- in seiner Breite veränderbar sein
- den aktuellen Zustand des ausgewählten Panels anzeigen
- beim Wechsel der Auswahl die kontextbezogenen Inhalte aktualisieren
- zwischen mehreren geöffneten bzw. verfügbaren Panel-Funktionen wechseln können, ohne das Hauptfenster zu verlassen
- die Hauptansicht und ihre Auswahl nicht unnötig verlieren oder neu laden
- vollständig tastatur- und mausbedienbar sein
- bei Bedarf ein Panel als maximierten Arbeitsbereich innerhalb des Hauptfensters darstellen können
- asynchrone Vorgänge anzeigen, ohne die GUI zu blockieren

#### Panel-Typen

**Pflicht-Panels (siehe Abschnitt 58.1 für die verbindliche technische Regelung, ausnahmslos ab Version 2.2):**

- **Vorschau/Informationen:** Bilder, Text, Markdown und weitere unterstützte Vorschauen; Dateiinformationen, Speicherort, Berechtigungen und technische Angaben
- **Eigenschaften:** interaktive Eigenschaften und Berechtigungsänderungen
- **Terminal:** VTE-Terminal für das aktuelle Verzeichnis
- **Suche:** Such- und Filterergebnisse sowie Suchoptionen
- **Archiv:** Komprimieren, Entpacken, Archivinhalt und Archivaktionen

**Weitere Panels (optional bzw. spätere Phasen, gleiche Panel-Architektur, aber ohne die ausnahmslose Pflicht aus Abschnitt 58.1):**

- **Aktionen:** kontextbezogene Aktionen, Stapelverarbeitung und weitere interaktive Werkzeuge, sofern die jeweilige Funktion dies unterstützt
- **Einstellungen:** einzelne Einstellungsseiten dürfen im Arbeitsbereich dargestellt werden, sofern geeignet; komplexe systemweite oder sicherheitsrelevante Entscheidungen dürfen weiterhin einen separaten Dialog verwenden
- **Git, Synchronisation, Versionierung, Duplikaterkennung, 3D-/CAD-Werkzeuge:** integrieren sich in dieselbe Panel-Architektur, sobald die jeweilige Phase (2 bzw. 3) freigegeben wird (siehe Abschnitt 40 für Git)

Die DEB-Paket-Erstellung ist als Teil des Archiv-Panels spezifiziert (Abschnitt 36.1) und zählt damit zum Pflicht-Panel Archiv. Weitere Panel-Ideen, die in keinem Abschnitt dieses Vertrags funktional beschrieben sind, sind kein Bestandteil des aktuellen Funktionsumfangs (siehe Abschnitt 58.1.5).

#### Panel-Navigation

Der Arbeitsbereich besitzt eine eindeutige Panel-Navigation. Ein Panel kann über Menü, Werkzeugleiste, Kontextmenü oder Tastenkürzel geöffnet werden. Wird bereits ein Panel angezeigt, wird es wiederverwendet und nicht als zusätzliches Fenster geöffnet.

Beispiel:

```text
Datei auswählen
      ↓
Funktion auswählen
      ↓
Rechter Arbeitsbereich
      ↓
passendes Panel laden
      ↓
Funktion innerhalb des Hauptfensters bedienen
```

Beim Schließen des Panels wird der normale Vorschau-/Informationszustand wiederhergestellt, sofern der Benutzer nicht ausdrücklich einen anderen Zustand gewählt hat.

#### Separate Fenster

Ein separates Top-Level-Fenster ist für eine Funktion nicht der Standard. Für Vorschau/Informationen, Eigenschaften, Terminal, Suche und Archiv gibt es dazu keine Ausnahme im Einzelfall mehr – die abschließende, verbindliche Regelung inklusive der vollständigen Ausnahmeliste steht in Abschnitt 58.1.2.

Für alle anderen, hier nicht als Pflicht-Panel gelisteten Funktionen gilt: Ein eigenes Fenster ist nur zulässig bei mindestens einem der folgenden, im Änderungsprotokoll (Abschnitt 8) konkret zu benennenden Gründe:

- die Funktion benötigt technisch nachweisbar ein eigenständiges Fenster (z. B. weil GTK dafür keinen einbettbaren Widget-Typ vorsieht)
- ein nativer Systemdialog ist erforderlich (siehe Ausnahmeliste in Abschnitt 58.1.2)
- die Funktion wurde in diesem Vertrag ausdrücklich als unabhängiges Fenster spezifiziert

„Das wäre einfacher" oder eine allgemeine Einschätzung „nicht sinnvoll im Hauptfenster darstellbar" reichen als Begründung nicht aus (vgl. Abschnitt 57).

#### Gemeinsame Zustandsverwaltung

Die Panel-Engine verwaltet mindestens:

- aktuellen Panel-Typ
- Sichtbarkeit des Arbeitsbereichs
- Panel-Breite
- zugehöriges Objekt bzw. Dateiauswahl
- laufenden Vorgang
- Panel-spezifischen Zustand
- Rückkehrzustand zur Vorschau/Information

Panel-Module dürfen ihre fachliche Logik behalten, müssen aber über eine gemeinsame Panel-Schnittstelle in den Arbeitsbereich eingebunden werden.

---

## 12. TASTATURBEDIENUNG

Alle Kürzel müssen tatsächlich funktionieren.

| Taste | Funktion |
|---|---|
| Pfeiltasten | Navigation in der Dateiansicht |
| Enter | Öffnen |
| Escape | Umbenennen, Filter oder Dialog abbrechen |
| Backspace / Alt+↑ | Übergeordneter Ordner |
| Alt+← / Alt+→ | Zurück / Vorwärts |
| Alt+Pos1 | Persönlicher Ordner |
| Strg+D | Lesezeichen hinzufügen |
| Strg++ / Strg+- / Strg+0 | Vergrößern / Verkleinern / Normale Größe |
| Leertaste | Vorschau der ausgewählten Datei ein-/ausblenden |
| Strg+C / Strg+X / Strg+V | Kopieren / Ausschneiden / Einfügen |
| Strg+Z / Strg+Shift+Z | Rückgängig / Wiederholen |
| Strg+A | Alles auswählen |
| Strg+Shift+N | Neuer Ordner |
| F2 | Umbenennen |
| Entf | In den Papierkorb verschieben |
| Shift+Entf | Endgültig löschen (mit Bestätigung) |
| Alt+Enter | Eigenschaften |
| F5 | Aktualisieren |
| Strg+H | Versteckte Dateien ein-/ausblenden |
| Strg+L | Pfad eingeben |
| Strg+I | Filterleiste für das aktuelle Verzeichnis |
| Strg+F | Vollständige Suche |
| Strg+T | Neuer Tab |
| Strg+W | Tab schließen |
| Strg+Shift+T | Geschlossenen Tab wiederherstellen |
| Strg+Tab / Strg+Shift+Tab | Nächster / vorheriger Tab |
| F3 | Vertikale Teilung |
| Shift+F3 | Horizontale Teilung |
| F4 | Integriertes Terminal ein-/ausblenden |
| F9 | Seitenleiste ein-/ausblenden |
| F11 | Informations-/Vorschaubereich ein-/ausblenden |

---

## 13. NAVIGATION

- Verlauf: Zurück, Vorwärts, Verlauf anzeigen, Verlauf löschen
- Verzeichnisse: Übergeordnet, Startseite, Persönlicher Ordner, Computer, Root-Dateisystem, Netzwerk, Papierkorb
- Pfadnavigation: Breadcrumbs, editierbare Adressleiste (Pfade und URIs), Pfad kopieren, Pfad einfügen, Pfad als Ort speichern
- Schnellnavigation: Orte, Favoriten, Zuletzt verwendet, Häufig verwendet (von Nolphin selbst gezählt), Geräte, Netzwerke
- zwischen Split-Bereichen wechseln

---

## 14. ORTE UND FAVORITEN

- Standardorte (XDG-Benutzerverzeichnisse): Persönlicher Ordner, Desktop, Dokumente, Downloads, Bilder, Musik, Videos, Papierkorb
- Eigene Orte (GTK-Lesezeichen): hinzufügen, entfernen, bearbeiten, umbenennen, verschieben
- Favoriten (XApp-Favoriten): Ordner und Dateien favorisieren, Favorit öffnen, Favoriten verwalten
- Geräte: Festplatten, USB-Geräte, externe Laufwerke, Netzwerkspeicher

Die Seitenleiste arbeitet konsistent mit der Hauptnavigation.

---

## 15. TABS

Jeder Tab hat eigenen Pfad, eigene Navigation, eigene Auswahl, eigene Dateiansicht und eigenen Zustand. Tabs verändern nie versehentlich den Zustand anderer Tabs.

- Verwaltung: neuer Tab, Tab schließen, alle Tabs schließen, Tab duplizieren, Tab verschieben, Tab umbenennen, Tab sperren (ein gesperrter Tab kann nicht versehentlich geschlossen werden)
- Navigation: nächster Tab, vorheriger Tab, Tab auswählen
- Wiederherstellung: geschlossenen Tab wiederherstellen, Tabs beim Start wiederherstellen
- Ordner in neuem Tab öffnen

---

## 16. SPLIT VIEW

- F3: vertikale Teilung, Shift+F3: horizontale Teilung
- ein bis vier Bereiche, rekursiv über GtkPaned aufgebaut
- jeder Bereich ist eine eigenständige Dateiansicht mit eigenem Pfad, Navigationszustand, eigener Auswahl, eigenen Tabs und eigenem Kontext
- Bereichsaktionen: aktivieren, wechseln, schließen, duplizieren, maximieren
- Dateiübertragung: zwischen Bereichen kopieren und verschieben, Drag & Drop zwischen Bereichen; der andere Bereich wird als Standardziel vorgeschlagen
- Ordner in neuem Bereich öffnen

---

## 17. INTEGRIERTES TERMINAL (F4)

- VTE 2.91, standardmäßig als **Panel im rechten integrierten Arbeitsbereich**
- das Terminal darf nicht als separates Top-Level-Fenster geöffnet werden, sofern der rechte Arbeitsbereich verfügbar ist
- Höhe und Breite des Terminal-Panels passen sich an den rechten Arbeitsbereich an; die Breite des Arbeitsbereichs ist veränderbar
- mehrere Terminals als Tabs innerhalb des Terminal-Panels
- Terminalpfad folgt dem aktuellen Verzeichnis; Navigation im Dateimanager aktualisiert den Terminalpfad
- „Terminal hier öffnen" im Kontextmenü öffnet das integrierte Terminal-Panel im gewählten Ordner
- F4 öffnet bzw. fokussiert das Terminal-Panel; bei erneutem F4 kann das Panel geschlossen werden
- Befehle und Skripte ausführen; Befehl als Aktion speichern (Abschnitt 43)

Das Terminal-Panel ist ein GtkWidget (kein GtkWindow/GtkDialog); `gtk_dialog_new()` bzw. `GTK_TYPE_DIALOG` sind für diese Funktion nicht zulässig (siehe Abschnitt 58.1).

Keine externe Terminalanwendung als Ersatz für das integrierte Terminal.

---

## 18. ANSICHT

- Ansichtsmodi: Symbolansicht, Kompakte Ansicht, Listenansicht mit Spalten (Detailansicht), aufklappbare Ordner in der Listenansicht (Baumansicht), Galerieansicht (große Vorschaubilder)
- Darstellung: Symbolgröße, Miniaturansichten, Dateiname, Größe, Typ, Änderungsdatum, Berechtigungen, Besitzer, Speicherort
- Sortierung: Name, Größe, Typ, Datum, Besitzer, Erweiterung – jeweils auf- und absteigend
- Gruppierung: Name, Typ, Datum, Größe, Erweiterung
- Anzeige: versteckte Dateien, rechter integrierter Arbeitsbereich, Statusleiste (mit freiem Speicherplatz), Seitenleiste
- Detailspalten konfigurierbar (ein-/ausblenden, Reihenfolge, Breite), Schriftgröße

---

### 18.1 Symbole (Icons)

- Datei- und Ordnersymbole kommen aus dem im System eingestellten Symbolthema (GtkIconTheme), passend zum MIME-Typ (`standard::icon`, `g_content_type_get_icon()`); keine eigenen Ersatzsymbole
- beim Abfragen von Dateiinformationen werden alle benötigten Attribute ausdrücklich angefordert, mindestens `standard::*`, `access::*`, `unix::mode`, `thumbnail::*`
- ein Schloss- bzw. „nicht lesbar"-Emblem erscheint nur, wenn `access::can-read` bzw. `access::can-write` tatsächlich vorhanden und FALSE ist; ein fehlendes Attribut gilt nie als „kein Zugriff"
- fehlt ein Symbol im Thema, wird eine sinnvolle Rückfallstufe verwendet (z. B. `text-x-generic`, `folder`, `application-x-executable`)
- das Programmsymbol wird nach `<präfix>/share/icons/hicolor/scalable/apps/` installiert und der Symbol-Cache aktualisiert

Test: Im persönlichen Ordner zeigen normale Dateien und Ordner ihre üblichen Symbole ohne Schloss; nur eine Datei ohne Leserechte (z. B. nach `chmod 000`) zeigt das Schloss.

---

## 19. AUSWAHL

- Einzelne Datei, mehrere Dateien, Bereich (Shift), Strg-Auswahl, Tastaturfokus
- Alles auswählen, Auswahl aufheben, Auswahl umkehren
- Auswahl nach Kriterien: Erweiterung, Dateityp, Name (mit Platzhaltern), Größe, Datum
- Auswahl speichern: aktuelle Auswahl unter einem Namen merken und später im selben Ordner wiederherstellen

---

## 20. DATEI-FUNKTIONEN

- Neu: Ordner, leere Datei, Dokument aus Vorlage (~/Vorlagen), Verknüpfung (.desktop), symbolische Verknüpfung, Hardlink (nur innerhalb desselben Dateisystems und nur für Dateien; sonst verständliche Meldung)
- Öffnen: Datei, Ordner, mit Standardanwendung, Öffnen mit …, in neuem Tab, in neuem Bereich
- Operationen: Kopieren, Ausschneiden, Einfügen, Verschieben, Umbenennen, Duplizieren, In Papierkorb verschieben, Löschen
- Verknüpfungen: Ziel anzeigen, zum Ziel springen
- Beenden: Fenster schließen, alle Fenster schließen; laufende Vorgänge werden vor dem Beenden angezeigt und müssen bestätigt werden

---

## 21. BEARBEITEN-FUNKTIONEN

- Rückgängig/Wiederholen funktioniert tatsächlich für mindestens: Umbenennen, Verschieben, Kopieren, In Papierkorb verschieben, Ordner erstellen
- Zwischenablage: aktuellen Inhalt anzeigen, leeren, Pfad kopieren, Dateiname kopieren, Text oder Bild aus der Zwischenablage als Datei einfügen

---

## 22. KOPIEREN UND VERSCHIEBEN

- einzelne Dateien, mehrere Dateien, Ordner, rekursiv, große Dateien
- Fortschritt: Fortschrittsbalken, Geschwindigkeit, verbleibende Zeit, übertragene und verbleibende Daten
- Steuerung: Pause, Fortsetzen, Abbrechen, Warteschlange, Reihenfolge wartender Vorgänge ändern
- Namenskonflikte: Überschreiben, Überspringen, Umbenennen, Ordner zusammenführen, Für alle anwenden
- Fehler werden verständlich angezeigt

---

## 23. LÖSCHEN UND PAPIERKORB

- In Papierkorb verschieben, endgültig löschen (mit Bestätigung), klar unterscheidbar
- Papierkorb öffnen, leeren, Dateien und Ordner wiederherstellen, einzelne Einträge endgültig löschen
- Papierkorbverwaltung: automatische Bereinigung nach Aufbewahrungsdauer (anhand DeletionDate der Freedesktop-Papierkorbinfo), Größenlimit mit Warnung

---

## 24. DATEIEN ÖFFNEN

Standardanwendung, Öffnen mit …, Standardanwendung ändern, passende Anwendungen vorschlagen (GAppInfo-Empfehlungen), ausführbare Dateien starten und Skripte ausführen (jeweils mit Rückfrage).

---

## 25. KONTEXTMENÜ

Passt sich an das Objekt an (Datei, Ordner, Archiv, mehrere Objekte, leerer Bereich):

- Datei: Öffnen, Öffnen mit …, Kopieren, Ausschneiden, Umbenennen, Duplizieren, Löschen, Eigenschaften
- Ordner: zusätzlich In neuem Tab öffnen, In neuem Bereich öffnen, Terminal hier öffnen
- Erweitert: Komprimieren, Entpacken, Senden an (E-Mail-Programm bzw. andere Anwendung), Prüfsumme berechnen, Verschlüsseln, Entschlüsseln
- Benutzeraktionen: eigene Aktionen, Skripte, Plugin-Einträge

---

## 26. DRAG & DROP

- Kopieren, Verschieben, Verknüpfen (Auswahl beim Loslassen)
- zwischen Tabs, Bereichen, Fenstern sowie zwischen lokalen und Netzwerkpfaden
- in andere Anwendungen und auf den Desktop, aus anderen Anwendungen in Nolphin
- sinnvolle Konfliktbehandlung

---

## 27. ZWISCHENABLAGE

Kopieren, Ausschneiden, Einfügen von mehreren Dateien und Ordnern, mit internen und externen Anwendungen, über GTK/GDK mit den Formaten text/uri-list und x-special/gnome-copied-files.

---

## 28. EIGENSCHAFTEN UND BERECHTIGUNGEN

- Basis: Name, Typ, MIME-Typ, Größe, Ordnergröße, Speicherort, Dateisystem, Verknüpfungsziel
- Zeit: Erstellung (soweit das Dateisystem sie liefert), Änderung, letzter Zugriff
- Besitz: Benutzer, Gruppe, Berechtigungen
- Technisch: Inode, Dateisystem-ID
- Berechtigungen ändern: Lesen, Schreiben, Ausführen für Besitzer, Gruppe und Andere; Besitzer und Gruppe ändern; Schreibschutz setzen/entfernen; rekursiv anwenden

Ordnergrößen blockieren nie die GUI. Keine stillen Root-Aktionen; Operationen, die Root-Rechte benötigen, werden klar gemeldet.

Eigenschaften und Berechtigungen werden standardmäßig im rechten integrierten Arbeitsbereich als interaktives Eigenschaften-Panel geöffnet. Diese Funktion ist ab Version 2.2 ausnahmslos als Panel-Widget zu implementieren; eine Umsetzung als `GTK_TYPE_DIALOG` (wie im bisherigen `NolphinPropertiesWindow`) gilt als Altlast und ist gemäß Abschnitt 58.1.3 umzubauen, nicht als Konflikt zu melden.

---

## 29. FILTER UND SUCHE (GRUNDFUNKTIONEN)

- Filterleiste (Strg+I): GtkSearchEntry, filtert die aktuelle Ansicht sofort
- Suche (Strg+F): Dateiname, Ordnername, Erweiterung, Pfad
- Ergebnisse: filtern, sortieren, öffnen, zum Speicherort springen
- keine Blockierung der GUI, auch bei großen Verzeichnissen
- die Suchfunktion wird standardmäßig im rechten integrierten Arbeitsbereich als Such-Panel dargestellt, sofern die Ergebnisse nicht ausdrücklich als eigene Hauptansicht benötigt werden
- `gtk_dialog_new()` bzw. `GTK_TYPE_DIALOG` sind für die Suchfunktion nicht zulässig (siehe Abschnitt 58.1)

---

## 30. VORSCHAU UND INFORMATIONSBEREICH (GRUNDFUNKTIONEN)

Der rechte Bereich (F11) ist der **integrierte rechte Arbeitsbereich** gemäß Abschnitt 11.2. Die Vorschau ist dessen Standardzustand, wenn keine andere Funktion geöffnet wurde. Dieses Panel zählt zu den Pflicht-Panels aus Abschnitt 58.1 und darf nicht als eigenes Fenster implementiert werden.

Vorschau-/Informationszustand:

- Datei: Name, Typ, Größe, Datum, Berechtigungen, Besitzer, Gruppe
- Speicher: Speicherort, Gerät, Dateisystem, Speicherverbrauch
- Bilder (JPG, PNG, SVG, WebP): Vorschau über GdkPixbuf mit erhaltenem Seitenverhältnis, Breite, Höhe
- Text und Markdown: Textvorschau
- nicht unterstützte Dateien: sinnvolle Fallback-Anzeige

Der rechte Arbeitsbereich darf von interaktiven Funktionspanels übernommen werden. Beim Öffnen eines solchen Panels bleibt die aktuelle Dateiauswahl und Hauptansicht erhalten. Nach dem Schließen wird wieder die vorherige Vorschau-/Informationsdarstellung hergestellt, sofern kein anderer Panel-Zustand gespeichert wurde.

Architektur: eigenständiges Vorschau-Modul mit Backend-Struktur sowie eigenständige Panel-Engine für die Einbindung in den rechten Arbeitsbereich. Alle Operationen sind asynchron. Beispiel-API für die Vorschau: `nolphin_preview_new()`, `nolphin_preview_set_file()`, `nolphin_preview_clear()`, `nolphin_preview_update()`. Die konkrete API darf angepasst werden, solange Vorschau und Panel-Container getrennt bleiben.

Für die Panel-Integration ist eine vergleichbare gemeinsame Schnittstelle vorzusehen, beispielsweise:

- Panel registrieren
- Panel öffnen
- Panel schließen
- Panel fokussieren
- Panel mit aktuellem Objekt aktualisieren
- Panel-Zustand erhalten/wiederherstellen

Konkrete Funktionsnamen sind nicht verbindlich; die Architektur muss jedoch eine einheitliche Einbindung aller interaktiven rechten Panels ermöglichen.

---

## 31. EINSTELLUNGEN

Vollständig über GSettings/dconf mit XML-Schema `data/org.nolphin.gschema.xml`. Keine fest codierte Konfiguration, wenn sie sinnvoll konfigurierbar ist.

- Allgemein: Startordner, Standardansicht, Fenstergröße, Verhalten beim Öffnen (Einfach-/Doppelklick), Bestätigungsdialoge
- Navigation: Tabs, Split View, Verlauf, Breadcrumbs, Tabs beim Start wiederherstellen
- Darstellung: Symbolgröße, Vorschauen, Spalten, Zeilenhöhe in der Listenansicht, Sortierung, Gruppierung
- Dateioperationen: Kopierverhalten, Überschreibverhalten, Papierkorb (Aufbewahrung, Größenlimit), Löschbestätigung, Warteschlange
- Suche: Suchpfade, Standardfilter
- Netzwerk: Standardprotokoll im Verbindungsdialog, gespeicherte Verbindungen
- Vorschau: aktivieren/deaktivieren, automatische Vorschau, maximale Dateigröße, unterstützte Typen, Breite des rechten Arbeitsbereichs
- Arbeitsbereich: sichtbar/ausgeblendet, zuletzt verwendetes Panel, Panel-Breite, Verhalten beim Öffnen von Funktionen, Rückkehr zur Vorschau nach dem Schließen eines Panels
- Terminal: Panel als Standarddarstellung, Terminalhöhe/-breite innerhalb des Arbeitsbereichs, Anzahl der Terminal-Tabs und Wiederherstellungsverhalten
- 3D/CAD: 3D-Vorschau aktivieren/deaktivieren, maximale Dateigröße, erlaubte Formate
- Kontextmenü: Aktionen und Skripte ein-/ausblenden

Einzelne Einstellungsseiten folgen, wo sinnvoll, der Panel-Architektur aus Abschnitt 11.2; dies ist optional, nicht Pflicht (Einstellungen zählt nicht zu den Pflicht-Panels aus Abschnitt 58.1).

Nach Schemaänderungen: `glib-compile-schemas`. Einstellungen sind persistent.

---

## 32. SYSTEMINTEGRATION

- Linux-Desktop-Integration über GIO/GVFS und XDG; optimiert für Cinnamon, ohne harte Abhängigkeit von einem Desktop
- Benachrichtigungen (GNotification) für längere Vorgänge; nie ein vorgetäuschter Erfolg
- Desktop-Datei `org.nolphin.FileManager.desktop`, Icon `org.nolphin.FileManager.svg`
- Standard-Dateimanager: `xdg-mime default org.nolphin.FileManager.desktop inode/directory` – die Registrierung wird tatsächlich geprüft
- Dateiüberwachung: Ansichten aktualisieren sich automatisch bei Änderungen (GFileMonitor)

---

# PHASE 2 – ERWEITERT

## 33. ERWEITERTE SUCHE

- Kriterien: Dateityp, Größe, Änderungsdatum, Erstellungsdatum, Besitzer, Gruppe, Berechtigungen
- Inhaltssuche: Textinhalt, Tags, Kommentare (aus GVFS-Metadaten)
- Operatoren: UND, ODER, NICHT, exakte Übereinstimmung, Platzhalter, reguläre Ausdrücke (GRegex)
- Ergebnisse: gruppieren, als Liste speichern, Suche speichern, Suchverlauf

---

## 34. ERWEITERTE VORSCHAU

- Bilder: EXIF über libexif, Seitenverhältnis; RAW-Fotos nur, wenn ein passender Thumbnailer installiert ist
- PDF über Poppler-GLib
- Office-Dokumente: Vorschaubild über installierte Thumbnailer; Seitenvorschau nur, wenn LibreOffice installiert ist
- Video und Audio: Dauer, Auflösung, Codec-Informationen über GStreamer; Vorschaubilder über Thumbnailer
- fehlt eine Komponente, wird das verständlich angezeigt

---

## 35. METADATEN UND TAGS

Gespeichert über GVFS-Metadaten:

- Tags vergeben, entfernen, danach suchen und filtern
- Bewertung (0–5 Sterne)
- Kommentare
- Emblems

Nur lesend, sofern in der Datei vorhanden: Autor, Titel, Beschreibung (z. B. aus PDF-, Bild- oder Audio-Metadaten).

Anzeige im Informationsbereich und im Eigenschaften-Dialog.

---

## 36. ARCHIVE UND KOMPRIMIERUNGSFORMATE
Abstrahierte Backend-Struktur über file-roller bzw. direkte Systemwerkzeuge (Abschnitt 53.5).

Komprimieren, Entpacken und die interaktive Archivverwaltung werden standardmäßig als **Archiv-Panel im rechten integrierten Arbeitsbereich** geöffnet. Ein separates Fenster ist hierfür nicht der Standard; `gtk_dialog_new()` bzw. `GTK_TYPE_DIALOG` sind für diese Funktion nicht zulässig (siehe Abschnitt 58.1).

Das Archiv-Panel muss mindestens die Auswahl von Format, Zielort, Optionen und Passwortschutz sowie Fortschritt, Fehler und Abschlusszustand darstellen können. Die bestehende Hauptansicht bleibt während des Vorgangs sichtbar.

Erstellung & Komprimierung:
- ZIP (.zip)
- TAR (.tar)
- TAR.GZ / TGZ (.tar.gz, .tgz)
- TAR.BZ2 / TBZ2 (.tar.bz2, .tbz2)
- TAR.XZ / TXZ (.tar.xz, .txz)
- TAR.ZST / TZST (.tar.zst, .tzst) (Zstandard)
- TAR.LZ4 (.tar.lz4) (LZ4)
- 7Z (.7z)
- Einzeldateikomprimierung ohne Tarball: GZ (.gz), BZ2 (.bz2), XZ (.xz), ZST (.zst), LZ4 (.lz4)

Entpacken / Nur Lesen (über file-roller oder installierte Systemwerkzeuge):
- RAR (.rar) (sofern unrar oder p7zip-rar installiert ist)
- CAB (.cab)
- ARJ (.arj)
- LZH / LHA (.lzh)
- ISO (.iso Image-Dateien entpacken/auslesen)
- CPIO / RPM / DEB (Paket- und Archiv-Container auslesen)

Erweiterte Einstellungen & Erstellungsdialog:
- Eingabe von Dateiname und Wahl des Formats über Dropdown-Menü
- Zielort-Auswahl (z. B. Home-Verzeichnis oder benutzerdefinierte Ordner)
- Passwordschutz & Verschlüsselung (inklusive Option "Dateiliste ebenfalls verschlüsseln", sofern vom Format unterstützt)
- Aufteilen in Teilarchive mit definierbarer Größe in MB

Verwaltung & Funktionen:
- Hier entpacken, nach … entpacken, einzelne oder mehrere Dateien extrahieren
- Archiv öffnen, Inhalt anzeigen, Dateien hinzufügen, entfernen, ersetzen, Archiv testen, Archivinformationen
- Bei TAR-basierten Formaten erfordert Bearbeiten ein Neupacken; der Benutzer wird bei großen Archiven darauf hingewiesen.
- Fehlt ein benötigtes Hilfsprogramm für ein Format, wird dies dynamisch erkannt und dem Benutzer verständlich mitgeteilt.

---

## 36.1 DEB-PAKET ERSTELLEN

Zusätzlich zu den reinen Lese-Funktionen für DEB-Archive (Abschnitt 36) kann Nolphin aus einem ausgewählten, bereits vorbereiteten Ordner (mit vorhandener `DEBIAN/control`-Struktur) über `dpkg-deb --build` ein installierbares .deb-Paket erzeugen.

- Kontextmenü auf einem geeigneten Ordner: „DEB-Paket erstellen …"
- Der Vorgang läuft im **Archiv-Panel** des rechten integrierten Arbeitsbereichs (Abschnitt 11.2/58.1), nicht als eigenes Fenster; `gtk_dialog_new()` bzw. `GTK_TYPE_DIALOG` sind auch für diese Funktion nicht zulässig.
- Zielort für die erzeugte .deb-Datei ist wählbar.
- Fortschritt, Fehler (z. B. fehlende oder ungültige `DEBIAN/control`-Datei, fehlendes `dpkg-deb`) und Abschlusszustand werden im Panel angezeigt.
- Fehlt `dpkg-deb`, wird die Funktion ausgegraut und der Grund angezeigt (Abschnitt 53.5).
- Menüeintrag unter Bearbeiten ▸ Archiv (Abschnitt 11.1).

Diese Funktion ist in der bestehenden Anwendung bereits implementiert; dieser Abschnitt dokumentiert sie nachträglich und ordnet sie verbindlich in die Panel-Architektur ein (Altlast-Prüfung gemäß Abschnitt 58.0: falls die bestehende Implementierung aktuell als eigenes Fenster läuft, ist das eine Altlast und wird ins Archiv-Panel überführt, kein Konflikt). Weicht der tatsächliche Funktionsumfang hiervon ab (z. B. ein zusätzliches Formular zur Eingabe von Name/Version/Architektur/Abhängigkeiten), wird dieser Abschnitt entsprechend präzisiert und mir zur Freigabe vorgelegt.

---

## 37. NETZWERK UND REMOTE-DATEIEN

- Netzwerk durchsuchen, Netzwerkgeräte, Freigaben, Server anzeigen
- Protokolle (soweit GVFS-Backends installiert): SMB (`smb://`), SFTP/SSH (`sftp://`), FTP (`ftp://`), WebDAV (`dav://`, `davs://`), NFS (`nfs://`), HTTP/HTTPS (`http://`, `https://`, nur lesend)
- Netzwerkressourcen hinzufügen, bearbeiten, entfernen, als Ort speichern
- Remote-Dateien: öffnen, kopieren, verschieben, löschen, umbenennen, herunter- und hochladen (nicht bei HTTP/HTTPS)
- Zugangsdaten über GtkMountOperation und den Systemschlüsselbund, nie im Klartext
- nicht installierte Backends werden dynamisch erkannt und gemeldet; keine vorgetäuschte Unterstützung

---

## 38. GERÄTE UND DATEISYSTEM

- Geräte: Festplatten, SSDs, USB-Sticks, SD-Karten, externe Festplatten, optische Laufwerke
- Aktionen: öffnen, einbinden, aushängen, sicher entfernen (soweit vom System unterstützt), Informationen anzeigen, LUKS-verschlüsselte Laufwerke entsperren (über GVFS/udisks)
- Speicherinformationen: Gesamtkapazität, belegt, frei, Dateisystemtyp, Mountpoint
- Verzeichnisse: Root `/`, `/home`, `/tmp`, `/var`, `/etc` und Benutzerverzeichnisse durchsuchbar (Schreibzugriff nur mit den vorhandenen Benutzerrechten)
- fehlerhafte oder nicht zugängliche Mountpoints werden sauber behandelt; Gerätezustände aktualisieren sich automatisch

---

## 39. SICHERHEITSFUNKTIONEN

- Prüfsummen: MD5, SHA-1, SHA-256, SHA-512 (GChecksum), BLAKE2 (b2sum); berechnen und mit einem eingegebenen Wert vergleichen
- Verschlüsselung über gpg: Datei verschlüsseln, entschlüsseln; Ordner verschlüsseln (als verschlüsseltes Archiv)
- ACL anzeigen und bearbeiten über getfacl/setfacl, sofern das Dateisystem ACL unterstützt

---

## 40. GIT

Über das Systemwerkzeug git: Status von Dateien anzeigen (als Emblem bzw. Spalte), Hinzufügen, Commit, Pull, Push, Log anzeigen, Diff anzeigen. Ohne installiertes git wird die Funktion ausgeblendet und der Grund angezeigt.

Die Bedienung erfolgt als Git-Panel im rechten integrierten Arbeitsbereich (siehe Abschnitt 11.2), sobald dieser Abschnitt in Phase 2 umgesetzt wird; kein separates Git-Fenster.

---

## 41. ARBEITSBEREICHE

Ein Arbeitsbereich umfasst: Tabs, Bereiche und Layout, Pfade, geöffnete Panels, Terminalzustand, Fensterposition und -größe.

- Arbeitsbereich speichern (mit Namen), laden, duplizieren, löschen
- letzten Arbeitsbereich beim Start automatisch wiederherstellen
- automatisches Speichern beim Beenden

---

# PHASE 3 – ZUSATZFUNKTIONEN

## 42. 3D- UND CAD-DATEIEN

Mindestens: STL, STEP, STP, FCStd (FreeCAD).

- STL: Erkennung, Unterscheidung ASCII/Binär, Dateigröße, Geometrieinformationen (z. B. Anzahl Dreiecke), Vorschau soweit Backend verfügbar
- STEP/STP: Erkennung, Dateiinformationen, technische Metadaten aus dem Dateikopf, Vorschau soweit Backend verfügbar
- FCStd: Erkennung (ZIP-Container), Dateiinformationen, Dokumentinformationen aus Document.xml, eingebettetes Vorschaubild, sofern vorhanden

Architektur getrennt vom Bild-Backend:

Vorschau
├── Bild
├── Text
├── PDF
├── Video/Audio
├── Dokument
└── 3D
├── STL
├── STEP/STP
└── FCStd


Dateityp-Erkennung → Backend-Auswahl → Metadaten → Vorschau → Aktionen. Neue Formate lassen sich ohne Umbau ergänzen. Weitere Formate (IGES, OBJ, 3MF, DXF, DWG) werden erkannt und als nicht unterstützt ausgewiesen.

Ein 3D-Renderer wird erst nach Prüfung verfügbarer Komponenten vorgeschlagen und nur nach meiner Freigabe eingebunden. Ohne Backend: Dateityp korrekt erkennen, Datei normal verwalten, fehlende Vorschau verständlich erklären. Keine vorgetäuschte 3D-Ansicht.

---

## 43. AKTIONEN UND AUTOMATISIERUNG

- Benutzerdefinierte Aktionen als Schlüsseldateien (z. B. `~/.local/share/nolphin/actions/*.nolphin_action`) mit Name, Befehl, Dateikontext (MIME-Typen, Erweiterungen), Auswahl (eine/mehrere Dateien, Ordner), Parameter, Sichtbarkeit, Ausführungsart (im Hintergrund oder im integrierten Terminal)
- Skripte: Ordner `~/.local/share/nolphin/scripts/`, erscheinen im Kontextmenü unter „Skripte"
- Stapelverarbeitung: Aktion auf viele Dateien anwenden
- Massenumbenennung: Suchen/Ersetzen, Nummerierung, Groß-/Kleinschreibung, Vorschau vor dem Ausführen, rückgängig machbar
- Regeln: „Wenn Dateityp/Name/Größe/Datum … dann Aktion …" – werden nur manuell auf einen gewählten Ordner angewendet, mit Vorschau der betroffenen Dateien vor der Ausführung

---

## 44. SYNCHRONISATION

Über rsync:

- Ordner vergleichen und Unterschiede anzeigen
- einseitig synchronisieren: lokal → Ziel oder Ziel → lokal (Ziel kann ein eingebundener Netzwerkordner sein)
- Konflikte: Quelle behalten, Ziel behalten, beide behalten, pro Datei entscheiden
- Vorschau („Trockenlauf") vor jeder Synchronisation

---

## 45. VERSIONIERUNG

Nolphin speichert Versionen selbst unter `~/.local/share/nolphin/versions/`:

- Version einer Datei speichern (manuell)
- Versionen anzeigen mit Zeitstempel
- Version wiederherstellen, Version löschen
- Unterschiede anzeigen (bei Textdateien)

---

## 46. DUPLIKATERKENNUNG

Duplikate in einem gewählten Ordner finden nach gleichem Namen, gleicher Größe und gleicher Prüfsumme. Ergebnisse gruppiert anzeigen; Löschen nur in den Papierkorb und nur nach Bestätigung.

---

## 47. PLUGINS UND ERWEITERUNGEN

- Plugins als Shared Libraries über GModule, mit definierter Plugin-Schnittstelle
- Plugin-Typen: Vorschau, Dateityp, Kontextmenü, Werkzeugleiste, Seitenleiste
- Plugin-Verwaltung: Liste, aktivieren/deaktivieren, Fehlerstatus
- ein fehlerhaftes Plugin darf Nolphin nicht unbemerkt instabil machen; Ladefehler werden angezeigt

Die Architektur ist so gebaut, dass Erweiterungen keine Änderungen am gesamten Hauptprogramm erfordern.

---

## 48. VERWALTUNG UND DIAGNOSE

- Einstellungen exportieren und importieren (dconf), auf Werkseinstellungen zurücksetzen
- Protokolle lokal speichern und anzeigen
- Fehlerberichte lokal als Datei erzeugen (kein automatisches Senden)
- Plugin-Status, Systeminformationen (Version, verfügbare GVFS-Backends, gefundene Werkzeuge)

---

# ALLGEMEINE REGELN (gelten ab Phase 1 für jede Funktion)

## 49. SPRACHE

Nolphin ist eine deutschsprachige Anwendung. Alle sichtbaren Texte (Menüs, Buttons, Dialoge, Meldungen, Tooltips, Statusleiste, Desktop-Datei, Einstellungen) sind auf Deutsch, unabhängig von der Systemsprache. Diese Entscheidung ist verbindlich und wird nicht rückgängig gemacht.

- Die vorhandene deutsche Umstellung des Projekts bleibt erhalten. Du baust sie nicht auf Englisch oder auf gettext zurück, ohne dass ich es ausdrücklich verlange.
- Jede neue Funktion bringt ihre Texte direkt auf Deutsch mit.
- Namen aus GTK, GLib, GIO und anderen Bibliotheken (Funktionen, Typen, Signale, Attribute, Icon-Namen) bleiben unverändert. Findest du Stellen, an denen solche Namen übersetzt wurden, meldest du sie als Fehler.
- Eigene Funktions- und Variablennamen im Code bleiben so, wie sie im Projekt vorliegen; sie werden nicht nachträglich zwischen Deutsch und Englisch umbenannt.

Test: Normaler Start aus dem Mint-Menü → alle Texte deutsch, keine englischen Reste.

---

## 50. BUILD

meson setup build        (bzw. meson setup --reconfigure build)
ninja -C build
ninja -C build install


Nach jeder Änderung: kompilieren, Compiler- und Linkerfehler beheben, Warnungen prüfen, Anwendung starten, Funktion testen. Optionale Komponenten (z. B. Poppler, GStreamer) werden in Meson als optionale Abhängigkeiten behandelt.

---

### 50.1 Abhängigkeiten und Installationen

- Erlaubt sind ausschließlich die Bibliotheken und Werkzeuge aus Abschnitt 3.
- Du installierst keine Pakete (apt, pip, npm oder andere) und fügst keine neue Abhängigkeit in meson.build ein, ohne vorher zu fragen. Format:

NEUE ABHÄNGIGKEIT: …
WOFÜR: …
PFLICHT ODER OPTIONAL: …
OHNE SIE: …
ALTERNATIVE OHNE NEUE ABHÄNGIGKEIT: …


  Danach auf meine Entscheidung warten.
- Reine Build-Pakete (z. B. `libgtk-3-dev`, `libvte-2.91-dev`, `meson`) aus Abschnitt 3 dürfen in der Entwicklungsumgebung installiert werden, um kompilieren zu können; jede solche Installation wird im Änderungsprotokoll genannt.
- Alle nicht zwingend nötigen Komponenten sind in Meson optionale Abhängigkeiten (`required: false` bzw. Feature-Optionen). Pflicht sind nur: GTK3, GLib/GIO, GdkPixbuf, VTE 2.91.
- Die Datei `docs/DEPENDENCIES.md` listet alle Abhängigkeiten mit Paketnamen für Linux Mint und einem fertigen `sudo apt install …`-Befehl (Pflicht und optional getrennt). Sie wird bei jeder Änderung aktualisiert.
- Tritt ein Build-Fehler wegen einer fehlenden Komponente auf, installierst du nicht einfach etwas Neues, sondern meldest den Fehler und die Ursache.

---

## 51. RECHERCHE UND UMGANG MIT DATEIEN UND BILDERN

Vor Beginn einer Aufgabe wird festgestellt, welche Recherche nötig ist: Software (Code, Bibliotheken, APIs, Systemkomponenten), Dateien (Formate, Metadaten, Zusammenhänge), Bilder (Referenzen, technische Bilder), 3D/CAD (Formate, verfügbare Linux-Werkzeuge).

Bei technischen Dateien: Datei identifizieren, Format feststellen, Inhalt/Metadaten prüfen, Analysewerkzeuge bestimmen, externe Quellen nur ergänzend nutzen, Unsicherheiten kennzeichnen, keine Eigenschaften erfinden.

Bei mehreren technischen Lösungen vorher prüfen: vorhandene Mint-/GTK-Komponenten, Systemwerkzeuge, Lizenzen, Einschränkungen, Performance, Wartbarkeit.

Ergebnisse werden getrennt ausgewiesen: DATEIINFORMATION (aus der Datei), RECHERCHE (externe Quellen), REFERENZBILD, TECHNISCHE BEWERTUNG, UNSICHER.

Nie behaupten, etwas geprüft zu haben, was nicht geprüft wurde. Nicht erreichbare Ressourcen: „NICHT VERIFIZIERT" angeben.

---

## 52. NICHT BLOCKIERENDE GUI

Asynchron oder außerhalb des UI-Threads: große Verzeichnisse, Suche, Inhaltssuche, Ordnergrößen, Kopieren, Verschieben, Löschen, Vorschauen, Metadaten, Netzwerk-, Archiv-, Prüfsummen-, Geräte-, Sync-, Duplikat- und 3D-Operationen.

---

## 53. CODEQUALITÄT UND SICHERHEIT

### 53.1 Codequalität
C11-konform, modular, wartbar, GTK3- und GLib-konform, speichersicher. Achten auf NULL-Prüfungen, GObject-Lebenszyklen, Referenzzählung, Signalverbindungen, Speicherfreigabe, Thread-Sicherheit, Datei-, URI- und Berechtigungsfehler.

### 53.2 Fehlerbehandlung
Fehler werden erkannt, protokolliert, verständlich angezeigt und möglichst mit einer sinnvollen Aktion versehen. Keine stillen Fehler, keine Fake-Erfolgsmeldungen.

### 53.3 Sicherheit
Besondere Vorsicht bei Löschen, Berechtigungs- und Besitzänderungen, symbolischen Links, externen Befehlen, Skripten, ausführbaren Dateien, Archiven, Netzwerkressourcen, Verschlüsselung, Regeln und Stapelverarbeitung. Benutzereingaben nie unsicher in Shell-Befehle einbauen.

### 53.4 Zugangsdaten
Passwörter werden nie im Klartext gespeichert oder protokolliert.

### 53.5 Externe Befehle
Pfad und Verfügbarkeit prüfen, fehlendes Programm erkennen und melden, Argumente als Liste über GSubprocess übergeben (keine unnötige Shell), Exit-Code und stdout/stderr auswerten, Prozess abbrechbar machen, Fehler anzeigen.

---

## 54. TESTS

Jede Funktion wird getestet. Mindestens:

- Dateioperationen: kopieren, verschieben, umbenennen, löschen, Papierkorb, wiederherstellen, Rückgängig
- Navigation: Zurück, Vorwärts, übergeordnet, Pfad, URI, Tabs
- Split: vertikal, horizontal, Kopieren zwischen Bereichen
- Terminal: F4, Pfadwechsel, Befehl, schließen, Panel-Fokus, mehrere Terminal-Tabs
- Rechter Arbeitsbereich: F11, Ein-/Ausblenden, Breitenänderung, Panel-Wechsel, Rückkehr zur Vorschau, Erhalt der Dateiauswahl
- Integrierte Panels: Eigenschaften, Suche und Archivfunktionen öffnen und bedienen, ohne ein separates Top-Level-Fenster zu erzeugen
- Suche: Name, Inhalt, Typ, Größe, Datum, Operatoren
- Vorschau: Bild, Text, PDF, unbekannter Typ, große Datei, ungültige Datei
- Geräte: erkennen, einbinden, aushängen
- Netzwerk: URI öffnen, fehlendes Backend, Verbindungsfehler
- Archive: erstellen, entpacken, testen
- 3D/CAD: STL, STEP, STP, FCStd erkennen, fehlendes Backend korrekt melden
- Einstellungen: Persistenz
- Sprache: alle Texte deutsch, keine englischen Reste (Abschnitt 49)
- Symbole: normale Symbole, Schloss nur bei fehlenden Rechten (Abschnitt 18.1)
- Eigenständigkeit: Build und Start ohne die optionalen Komponenten aus Abschnitt 3
- Pflicht-Panels (Abschnitt 58.1): Repository-weiter Suchlauf ohne Treffer für `GTK_TYPE_DIALOG`/`gtk_dialog_new` in den betroffenen Modulen; Fensterliste vor/nach Öffnen von Eigenschaften, Terminal, Suche und Archiv unverändert

Nach jeder größeren Implementierung:

- IMPLEMENTIERT: …
- GETESTET: …
- TESTERGEBNIS: …
- NOCH OFFEN: …

---

## 55. DEFINITION VON „FERTIG"

Eine Funktion ist nur dann IMPLEMENTIERT, wenn Code vorhanden ist, kompiliert, tatsächlich ausführbar ist, der Anwendungsfall getestet wurde, Fehlerfälle behandelt werden und keine Fake-Implementierung vorliegt. Ein Menüeintrag oder Button allein bedeutet nicht, dass die Funktion implementiert ist.

---

## 56. ABSCHLUSS-CHECKLISTE PRO ABSCHNITT

- [ ] Gesamtziel und relevante Anforderungen verstanden
- [ ] vorhandener Code, Dateien, Referenzen geprüft
- [ ] Implementierung durchgeführt
- [ ] alle neuen Texte auf Deutsch
- [ ] keine neue Abhängigkeit ohne Freigabe
- [ ] Funktion im richtigen Menü eingetragen (Abschnitt 11.1)
- [ ] Build erfolgreich, Warnungen geprüft
- [ ] Funktion gestartet und getestet, Fehlerfälle getestet
- [ ] Installation geprüft
- [ ] Dokumentation aktualisiert
- [ ] Status korrekt angegeben

---

## 57. VERBOTENE VORGEHENSWEISEN

- Code, APIs oder nicht existierende Dateien erfinden
- nicht getestete Funktionen als fertig markieren
- Anforderungen still entfernen oder ohne Meldung vereinfachen
- fremden Code kopieren
- Qt- oder KDE-Bibliotheken verwenden
- Pakete installieren oder Abhängigkeiten hinzufügen ohne meine Freigabe (Abschnitt 50.1)
- Abhängigkeiten zu Nemo oder Nemo-Paketen
- die deutsche Oberfläche auf Englisch oder gettext zurückbauen
- Pseudocode oder Platzhalter als fertige Funktion ausgeben
- Build- oder Testfehler ignorieren
- UI-Anforderungen eigenmächtig ändern
- Rechercheergebnisse als Dateieigenschaften ausgeben
- Vorschauen (Bild, PDF, 3D, CAD) vortäuschen
- mit einer späteren Phase beginnen, ohne dass ich die vorherige freigegeben habe
- weitere Hauptmenüs neben Datei, Bearbeiten, Ansicht, Gehe zu, Lesezeichen, Hilfe anlegen

Keine Begründung wie „zu komplex", „nicht notwendig" oder „das wäre einfacher" erlaubt das Weglassen. Stattdessen: analysieren, Einschränkungen dokumentieren, bei echtem Konflikt nachfragen.

---

## 58. UNKLARHEITEN, KONFLIKTE UND PRIORITÄTEN

Eindeutige Anforderung: umsetzen. Mehrdeutige Anforderung: Interpretation dokumentieren, bei wesentlichen Entscheidungen nachfragen.

Konfliktformat:

KONFLIKT: …
ANFORDERUNG A: …
ANFORDERUNG B: …
TECHNISCHE AUSWIRKUNG: …
VORSCHLAG: …


Dann auf meine Entscheidung warten.

Prioritäten: 1. meine ausdrücklichen Anforderungen, 2. tatsächliche Funktionalität, 3. Datenintegrität, 4. Stabilität, 5. korrekte Linux-Integration, 6. UI-Konsistency, 7. Performance, 8. Erweiterbarkeit, 9. Codequalität.

### 58.0 ECHTER KONFLIKT VS. ALTLAST

Nicht jede Abweichung zwischen bestehendem Code und diesem Vertrag ist ein Konflikt im Sinne des Konfliktformats.

**Echter Konflikt** (Konfliktformat verwenden, auf Freigabe warten): zwei Anforderungen dieses Vertrags widersprechen sich inhaltlich, oder eine Anforderung ist technisch nicht wie beschrieben umsetzbar.

**Altlast** (kein Konflikt, keine Rückfrage nötig, einfach umsetzen): bestehender Code entspricht einer bereits eindeutig entschiedenen Anforderung dieses Vertrags noch nicht. Das ist erwarteter, normaler Projektfortschritt, kein Entscheidungsbedarf. Beispiel: Eine Funktion ist aktuell als `GTK_TYPE_DIALOG` implementiert, obwohl dieser Vertrag für sie ausdrücklich ein Panel im rechten Arbeitsbereich vorschreibt (siehe Abschnitt 58.1) – das wird umgebaut, nicht gemeldet.

Im Zweifel gilt: Wenn dieser Vertrag für den fraglichen Fall bereits eine eindeutige Aussage trifft, ist es eine Altlast. Nur wenn der Vertrag selbst unklar, widersprüchlich oder technisch nicht umsetzbar ist, ist es ein echter Konflikt.

---

## 58.1 UI-ARCHITEKTUR: INTEGRIERTER RECHTER ARBEITSBEREICH (verbindliche Fassung, Version 2.2)

Dieser Abschnitt ist für die fünf Pflicht-Panels abschließend und verbindlich. Er ersetzt/konkretisiert alle weicheren Formulierungen an anderer Stelle des Vertrags (u. a. Abschnitt 10, 11.2, 17, 28, 29, 30, 31, 36). Bei Widerspruch zwischen diesem Abschnitt und einer älteren Formulierung gilt dieser Abschnitt.

### 58.1.1 Pflicht-Panels

Folgende fünf Funktionen MÜSSEN als Panel-Widget im rechten integrierten Arbeitsbereich laufen – ausnahmslos, ab sofort, ohne weitere Rückfrage:

1. Vorschau/Informationen (Abschnitt 30)
2. Eigenschaften und Berechtigungen (Abschnitt 28)
3. Terminal (Abschnitt 17)
4. Suche (Abschnitt 29)
5. Archiv – Komprimieren/Entpacken/DEB-Paket erstellen (Abschnitt 36, 36.1)

Für diese fünf Funktionen gilt technisch verbindlich:

- Implementierung als eigenständiges `GtkWidget` (z. B. Subklasse von `GtkBox` oder `GtkGrid`), NICHT als `GtkWindow`, `GtkDialog` oder `GTK_TYPE_DIALOG`-Subtyp.
- `gtk_dialog_new()`, `gtk_dialog_new_with_buttons()` und jede `GTK_TYPE_DIALOG`-Subklasse sind für diese fünf Funktionen verboten.
- Einbindung ausschließlich über die Panel-Engine (Abschnitt 10) in einen gemeinsamen Container (z. B. `GtkStack`) innerhalb des rechten `GtkPaned`-Bereichs.
- Öffnen/Wechseln über Menü, Werkzeugleiste, Kontextmenü oder Tastenkürzel aktiviert das jeweilige Panel im bestehenden Arbeitsbereich; es wird nie ein zusätzliches Top-Level-Fenster erzeugt.

### 58.1.2 Ausnahmen (abschließende Liste)

Nur folgende Dialoge/Fenster bleiben ausdrücklich als klassische GTK-Dialoge bzw. eigene Fenster bestehen, weil sie native Systemfunktionen sind oder eine eindeutige Bestätigung/Blockierung erfordern:

- `GtkFileChooserDialog` (Öffnen/Speichern unter, Zielordner wählen)
- Lösch-/Überschreib-Bestätigungsdialoge (z. B. „Endgültig löschen?")
- Fehlermeldungen und kritische Warnungen (`GtkMessageDialog`)
- `GtkMountOperation` (Zugangsdaten, LUKS-Entsperrung)
- Der „Über Nolphin"-Dialog
- Das Hauptfenster selbst sowie zusätzliche Datei-Manager-Fenster (Abschnitt 11, „mehrere Fenster gleichzeitig möglich")

Jede weitere Funktion, die nicht in 58.1.1 oder in dieser Ausnahmeliste steht, entscheidet sich nach der allgemeinen Regel in Abschnitt 11.2 („Separate Fenster"): Panel ist Standard, ein eigenes Fenster nur bei zwingendem technischem Grund – und dieser Grund wird im Änderungsprotokoll (Abschnitt 8) konkret benannt, nicht pauschal behauptet.

### 58.1.3 Migration bestehenden Codes (Altlasten, keine Konflikte)

Eine bestehende Implementierung, die einer der fünf Pflicht-Panel-Funktionen entspricht, aber aktuell als `GTK_TYPE_DIALOG` bzw. eigenes Top-Level-Fenster gebaut ist (bekanntes Beispiel: `NolphinPropertiesWindow`), ist eine Altlast gemäß Abschnitt 58.0 – kein Konflikt. Der Umbau in ein Panel-Widget ist hiermit für alle fünf Funktionen ausdrücklich freigegeben; dafür ist keine weitere Rückfrage nötig. Vorgehen:

1. Bestehende fachliche Logik (z. B. Berechnungen, GIO-Aufrufe, Callbacks) so weit wie möglich unverändert aus dem bisherigen Dialog-Code übernehmen.
2. Nur die Hülle austauschen: `GtkDialog`/`GtkWindow` → `GtkWidget`-Panel, das über die Panel-Engine eingebunden wird.
3. Bisherige Tastenkürzel, Menüeinträge, Kontextmenüeinträge und GActions bleiben erhalten und binden ab jetzt an das Panel statt an den Dialog.
4. Der Umbau wird wie jede andere Änderung im Änderungsprotokoll (Abschnitt 8) dokumentiert.

### 58.1.4 Panel-Engine – Architekturvorgaben

Die Panel-Engine (Abschnitt 10) verwaltet mindestens:

- Registrierung eines Panel-Typs (feste interne Kennung, z. B. `NOLPHIN_PANEL_EIGENSCHAFTEN`, `NOLPHIN_PANEL_TERMINAL`, `NOLPHIN_PANEL_SUCHE`, `NOLPHIN_PANEL_ARCHIV`, `NOLPHIN_PANEL_VORSCHAU`)
- Öffnen, Wechseln, Fokussieren und Schließen eines Panels
- Sichtbarkeit und Breite des Arbeitsbereichs (F11, ziehbarer Splitter)
- aktuelle Dateiauswahl/-objekt je Panel
- Rückkehr zum Vorschau-/Informationszustand nach Schließen eines Panels

Konkrete Funktions- und Signalnamen bleiben – wie schon in Abschnitt 30 festgelegt – nicht verbindlich vorgegeben; verbindlich ist ausschließlich, dass alle fünf Pflicht-Panels dieselbe Panel-Schnittstelle verwenden und keines davon einen eigenen Weg über ein Top-Level-Fenster nimmt.

### 58.1.5 Nicht spezifizierte Zusatz-Panels

Panel-Ideen, die in keinem Abschnitt dieses Vertrags funktional spezifiziert sind, sind ausdrücklich NICHT Teil des aktuellen Funktionsumfangs und werden nicht umgesetzt, bis ich sie in einem eigenen Abschnitt spezifiziere. Ein Modul darf hierfür vorbereitet, aber nicht implementiert werden. (Die DEB-Paket-Erstellung fällt seit Version 2.3 nicht mehr unter diese Regel, siehe Abschnitt 36.1.)

### 58.1.6 Test/Definition-of-Done für diesen Abschnitt

Zusätzlich zu Abschnitt 54 gilt für die fünf Pflicht-Panels:

- Ein Repository-weiter Suchlauf nach `GTK_TYPE_DIALOG`, `gtk_dialog_new` und `gtk_dialog_new_with_buttons` in den Modulen für Eigenschaften, Terminal, Suche und Archiv liefert keine Treffer außerhalb der in 58.1.2 gelisteten Ausnahmen.
- Öffnen jeder der fünf Funktionen erzeugt nachweislich kein zusätzliches Top-Level-Fenster (manueller Test: Fensterliste vor/nach dem Öffnen vergleichen).
- Panel-Wechsel, F11, Breitenänderung, Erhalt der Dateiauswahl und Rückkehr zur Vorschau funktionieren für alle fünf Panels identisch (siehe Abschnitt 54).

### 58.1.7 Änderungsprotokoll dieses Abschnitts

- REQUIREMENT: Beseitigung der Mehrdeutigkeit „soweit technisch und ergonomisch sinnvoll" für die fünf Pflicht-Panels; verbindliche Klärung, dass bestehende Dialog-Implementierungen (z. B. `NolphinPropertiesWindow`) Altlasten und keine Konflikte sind.
- DATEI: docs/NOLPHIN_SPEC.md, Abschnitt 9, 10, 11.2, 17, 28, 29, 30, 31, 36, 40, 54, 58, 58.1.
- ÄNDERUNG: siehe 58.1.1–58.1.6 oben; Abschnitt 58.0 neu eingeführt (echter Konflikt vs. Altlast); ASCII-Diagramm in Abschnitt 11.2 bereinigt (Tippfehler entfernt, nicht spezifizierte Panels als „geplant/offen" markiert statt implizit als Anforderung dargestellt).
- WARUM: Die bisherige Formulierung erlaubte unterschiedliche Auslegungen und führte dazu, dass eine bereits entschiedene Anforderung (Panel statt Dialog) wiederholt als klärungsbedürftiger Konflikt gemeldet wurde, statt umgesetzt zu werden.
- TEST: siehe 58.1.6.

---

## 59. GOLDENE REGEL

Ich entscheide, was Nolphin sein soll. Du analysierst, planst, implementierst, baust und testest. Du darfst technische Verbesserungen vorschlagen, aber keine Produktanforderung eigenständig entfernen, ersetzen oder abschwächen.

---

ENDE DES NOLPHIN MASTER DEVELOPMENT CONTRACT (Version 2.2)
