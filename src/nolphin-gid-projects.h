/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nolphin-gid-projects.h: Projektliste der GID-Projekte (§60.2)
 *
 * Gespeichert in GSettings (Schlüssel "gid-projects" in
 * org.nolphin.preferences). Alles bleibt lokal.
 */

#ifndef NOLPHIN_GID_PROJECTS_H
#define NOLPHIN_GID_PROJECTS_H

#include <glib.h>

G_BEGIN_DECLS

/* Liefert eine NULL-terminierte Liste absoluter Pfade (mit g_strfreev()
 * freigeben), in der Reihenfolge des Hinzufügens. */
gchar   **nolphin_gid_projects_get      (void);
/* Fügt den Ordner @path hinzu. FALSE, wenn er kein vorhandener Ordner oder
 * schon in der Liste ist. */
gboolean  nolphin_gid_projects_add      (const gchar *path);
/* Nimmt @path aus der Liste. Löscht niemals Dateien (§60.2). */
void      nolphin_gid_projects_remove   (const gchar *path);
gboolean  nolphin_gid_projects_contains (const gchar *path);

/* Aktueller Git-Branch des Projektordners (bei losem HEAD: Kurz-Hash) aus
 * .git/HEAD gelesen; NULL, wenn der Ordner kein Git-Repository ist. */
gchar    *nolphin_gid_project_get_branch (const gchar *path);

G_END_DECLS

#endif /* NOLPHIN_GID_PROJECTS_H */
