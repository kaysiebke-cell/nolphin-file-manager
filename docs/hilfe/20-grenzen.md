# Bekannte Grenzen und häufige Fragen

Nolphin ist Entwicklungssoftware. Diese Seite sagt offen, was (noch) nicht geht.

## Was ist noch nicht umgesetzt?

- **Suche:** keine Kriterien nach Besitzer, Gruppe und Berechtigungen; keine Suche in Tags und Kommentaren.
- **PDF und Medien:** Die Angaben kommen über die Programme `pdfinfo`, `pdftocairo` und `gst-discoverer-1.0`, nicht über Bibliotheken.
- **Regeln:** werden nur von Hand angewendet und nicht gespeichert.
- **Versionskontrolle:** Overlays gibt es nur für Git, nicht für SVN oder Mercurial.
- **Netzwerk:** SFTP, FTP und WebDAV hängen von den GVfs-Diensten Ihres Systems ab und sind nicht mit allen Servern geprüft.

## Häufige Fragen

### Eine Funktion zeigt „nicht installiert“ oder einen Hinweis.

Nolphin nutzt vorhandene Programme. Unter **Hilfe ▸ Diagnose ▸ Systeminformationen** sehen Sie, was fehlt. Unter Linux Mint installieren Sie es meist mit `sudo apt install <Paket>`:

| Funktion | Paket |
| -------- | ----- |
| PDF-Vorschau | `poppler-utils` |
| Audio-/Video-Angaben | `gstreamer1.0-tools` |
| Synchronisation | `rsync` |
| ZIP | `zip`, `unzip` |
| 7Z und weitere Archive | `p7zip-full` |
| ACL | `acl` |
| Verschlüsselung | `gnupg` |
| Git | `git` |

### Wo sind meine Tags und Bewertungen gespeichert?

Als Datei-Metadaten Ihres Benutzerkontos (GVfs). Sie gehören zu der Datei auf diesem Rechner; kopieren Sie sie auf ein anderes System, kommen die Angaben nicht automatisch mit.

### Kann ich eine gelöschte Datei zurückholen?

Dateien, die Sie in den **Papierkorb** verschieben, liegen dort und lassen sich wiederherstellen. „Löschen“ entfernt sie endgültig.

### Das Programm verhält sich seltsam.

Öffnen Sie **Hilfe ▸ Diagnose ▸ Protokolle anzeigen**, erstellen Sie bei Bedarf einen **Fehlerbericht** und beschreiben Sie, was geschehen ist.

### Wie melde ich einen Fehler?

Nennen Sie Nolphin-Version, Linux-Mint-Version, das Problem, die Schritte zur Reproduktion, das erwartete und das tatsächliche Verhalten sowie Fehlermeldungen. Den Fehlerbericht aus der Diagnose können Sie beilegen.

### Wie erkenne ich, ob ich die installierte Version oder einen Entwicklungsstand nutze?

Ein Start aus dem Build-Ordner (also nicht aus `/usr/…`) kennzeichnet sich selbst:

- Im **Fenstertitel** steht der Zusatz **[Entwicklung]**.
- Unter **Hilfe ▸ Über** und unter **Hilfe ▸ Diagnose ▸ Systeminformationen** steht „Entwicklungsstand“; die Diagnose nennt außerdem den Pfad, aus dem Nolphin gestartet wurde.
- Der Entwicklungsstand läuft unter einer eigenen Kennung und kann **neben** der installierten Version geöffnet sein, ohne in ihr zu landen.

Die installierte Version hat keinen Zusatz im Titel.
