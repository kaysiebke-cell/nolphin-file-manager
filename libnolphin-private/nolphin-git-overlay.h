/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nolphin-git-overlay.h: Git-Status-Symbole an Dateien (Overlays)
 *
 * Liefert für lokale Dateien in einem Git-Repository den Status, der als
 * Emblem am Datei-Icon gezeigt wird. Der Status wird pro Repository
 * zwischengespeichert und asynchron (über nolphin-git) aktualisiert; die
 * Abfrage selbst blockiert nie. Sobald neue Daten da sind, bekommen die
 * betroffenen Dateien ein "changed", damit die Ansicht neu zeichnet.
 */

#ifndef NOLPHIN_GIT_OVERLAY_H
#define NOLPHIN_GIT_OVERLAY_H

#include <gio/gio.h>
#include <libnolphin-private/nolphin-git.h>

G_BEGIN_DECLS

/* Status von @file (NOLPHIN_GIT_STATUS_UNMODIFIED, wenn kein Repository,
 * keine Änderung oder noch unbekannt). Stößt bei Bedarf eine
 * Aktualisierung an. */
NolphinGitFileStatus nolphin_git_overlay_get_status (GFile *file);

/* Emblem-Namen (Fallback-Kette, NULL-terminiert, statisch) für @status,
 * NULL wenn kein Symbol gezeigt werden soll. */
const gchar * const *nolphin_git_overlay_get_emblem_names (NolphinGitFileStatus status);

G_END_DECLS

#endif /* NOLPHIN_GIT_OVERLAY_H */
