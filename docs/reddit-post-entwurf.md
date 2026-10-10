# Reddit-Entwurf (nur Entwurf, nicht im Repository eingecheckt)

Geeignete Subreddits: r/linuxmint (Englisch), ggf. r/linux_gaming ist unpassend.
r/linux erlaubt Eigenwerbung nur eingeschränkt – vorher die Regeln des Subreddits lesen.

---

**Titel:**

I forked Nemo into "Nolphin": a Linux Mint file manager with a fixed right-hand workspace (preview, terminal, git, archives, .deb builder)

**Text:**

Hi everyone,

I've been working on a file manager for Linux Mint Cinnamon called **Nolphin**. It's a fork of Nemo 6.7.7 (GTK3, C11, GPL), renamed and extended. It is an independent hobby project, not affiliated with Linux Mint.

**Why I started this:**

1. In Nemo I couldn't get a proper file preview panel working without errors, the way Dolphin has it. That was the starting point.
2. It always bothered me that every function opened a new window, and those windows piled up on top of the file manager. Since the right-hand sidebar for the preview was already there, it made sense to expand it into a proper workspace: every function lives cleanly in that sidebar, so you can work in parallel without a window covering the file manager.
3. The file management didn't cover what my projects need. That starts with CAD files, and also my small GitHub projects, which I wanted to see and manage right inside the file manager.

(The name is a nod to Dolphin, which is where the preview idea came from.)

**The idea:** the function comes to the file, not the other way round. Instead of jumping between file manager, terminal and extra tools, the main view stays visible and a fixed workspace on the right (F11) shows whatever you need:

- Preview with media info (images, PDF, video, audio)
- Properties / permissions / ACL
- Terminal (F4, VTE) in the current folder
- Git panel with overlay icons on files (status, commit, push/pull, log, diff)
- Archive manager (list, add, replace, remove, test, extract)
- .deb package builder
- Bulk rename with preview, folder compare/sync (rsync, never deletes), duplicate finder, file versions
- "GID projects": project folders in the sidebar that render the project's README.md in the workspace

Where a helper tool or preview backend is missing, Nolphin says so instead of faking the feature.

Screenshots and the full feature list are in the README: https://github.com/kaysiebke-cell/nolphin-file-manager

**How it was built:** I built Nolphin together with an AI assistant (Claude Code). The ideas, the goals and the decisions about what the file manager should do are mine; the coding was a joint effort, and I test and review the results myself. I'm mentioning it openly because I think that's the fair thing to do.

**Honest status:** it works on my machine (Linux Mint, Cinnamon), but it hasn't had wide testing, so expect rough edges. The UI is German by default (gettext, English falls back from the source strings – translation coverage is incomplete).

I'd love feedback: what's missing, what's annoying, what would make you try it? Bug reports on GitHub are very welcome.

---

## Deutsche Fassung (zum Lesen, entspricht dem englischen Text)

**Titel:**

Ich habe Nemo zu „Nolphin“ weiterentwickelt: ein Linux-Mint-Dateimanager mit festem rechtem Arbeitsbereich (Vorschau, Terminal, Git, Archive, .deb-Ersteller)

**Text:**

Hallo zusammen,

ich arbeite an einem Dateimanager für Linux Mint Cinnamon namens **Nolphin**. Er ist ein Fork von Nemo 6.7.7 (GTK3, C11, GPL), umbenannt und erweitert. Es ist ein unabhängiges Hobbyprojekt und nicht mit Linux Mint verbunden.

**Warum ich damit angefangen habe:**

1. In Nemo konnte ich keine ordentliche Dateivorschau einbauen, ohne dass Fehler auftraten – so, wie Dolphin sie hat. Davon ging alles aus.
2. Es hat mich immer gestört, dass sich für jede Funktion ein neues Fenster öffnete und diese Fenster über dem Dateimanager lagen. Da die rechte Seitenleiste für die Dateivorschau schon da war, bot es sich an, sie zu einem richtigen Arbeitsbereich auszubauen: Jede Funktion sitzt sauber in dieser Seitenleiste, sodass man parallel arbeiten kann, ohne dass ein Fenster den Dateimanager überdeckt.
3. Die Verwaltung reichte für meine Projekte nicht aus. Das fing bei CAD-Dateien an und betraf auch meine kleinen GitHub-Projekte, die ich direkt im Dateimanager sehen und verwalten wollte.

(Der Name ist eine Anspielung auf Dolphin, von dem die Vorschau-Idee kommt.)

**Die Idee:** Die Funktion kommt zur Datei, nicht umgekehrt. Statt ständig zwischen Dateimanager, Terminal und Zusatzprogrammen zu wechseln, bleibt die Hauptansicht sichtbar, und rechts zeigt ein fester Arbeitsbereich (F11), was du gerade brauchst:

- Vorschau mit Medieninfos (Bilder, PDF, Video, Audio)
- Eigenschaften / Berechtigungen / ACL
- Terminal (F4, VTE) im aktuellen Ordner
- Git-Panel mit Overlay-Symbolen an den Dateien (Status, Commit, Push/Pull, Verlauf, Unterschiede)
- Archiv-Manager (Inhalt anzeigen, hinzufügen, ersetzen, entfernen, prüfen, entpacken)
- .deb-Paket-Ersteller
- Massenumbenennung mit Vorschau, Ordner vergleichen/synchronisieren (rsync, löscht nie), Duplikate finden, Dateiversionen
- „GID-Projekte“: Projektordner in der Seitenleiste, deren README.md im Arbeitsbereich angezeigt wird

Fehlt ein Hilfsprogramm oder ein Vorschau-Backend, sagt Nolphin das, statt die Funktion vorzutäuschen.

Screenshots und die vollständige Funktionsliste stehen im README: https://github.com/kaysiebke-cell/nolphin-file-manager

**Wie es entstanden ist:** Ich habe Nolphin gemeinsam mit einem KI-Assistenten (Claude Code) gebaut. Die Ideen, die Ziele und die Entscheidungen, was der Dateimanager können soll, stammen von mir; die Programmierung war Gemeinschaftsarbeit, und die Ergebnisse teste und prüfe ich selbst. Ich erwähne es offen, weil ich das für fair halte.

**Ehrlicher Stand:** Auf meinem Rechner (Linux Mint, Cinnamon) läuft es, aber es wurde noch nicht breit getestet. Rechne also mit Ecken und Kanten. Die Oberfläche ist standardmäßig deutsch (gettext, für Englisch gibt es als Ausweichtext die Quelltexte – die Übersetzung ist unvollständig).

Ich freue mich über Rückmeldungen: Was fehlt, was nervt, was würde dich zum Ausprobieren bewegen? Fehlermeldungen auf GitHub sind sehr willkommen.

---

## Vor dem Posten prüfen (nicht ich, sondern du)

- Stimmt der Satz zur Sprache? Ich habe im Code nicht geprüft, wie vollständig die englische Übersetzung ist. Passe den Absatz an oder streiche ihn.
- Stimmt „works on my machine"? Ändere es nach deinem tatsächlichen Test.
- Der Absatz „How it was built" erwähnt die Zusammenarbeit mit der KI. Prüfe, ob „ich teste und prüfe die Ergebnisse selbst" für dich stimmt; ändere es sonst ab.
- Das Repository ist öffentlich, die Lizenz (GPL, von Nemo geerbt) liegt als COPYING bei – das ist für einen Fork wichtig.
- Poste ein Bild (z. B. `docs/bilder/uebersicht.png`) direkt in den Beitrag.
