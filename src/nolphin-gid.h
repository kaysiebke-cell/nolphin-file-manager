/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nolphin-gid.h: GID-Projekte (§60)
 *
 * Lokale Projektordner: das Panel "gid" im rechten Arbeitsbereich, das die
 * README.md des gewählten Projekts als gerenderten Markdown-Text zeigt. Die
 * Projektliste steht in nolphin-gid-projects.h. Alles bleibt lokal, es gibt
 * keine Netzwerkzugriffe.
 */

#ifndef NOLPHIN_GID_H
#define NOLPHIN_GID_H

#include <gtk/gtk.h>
#include <gio/gio.h>
#include "nolphin-window.h"
#include "nolphin-gid-projects.h"

G_BEGIN_DECLS

/* Panel-Seite "gid" des rechten Arbeitsbereichs (§60.3). */
GtkWidget *nolphin_gid_page_new  (NolphinWindow *window);
void       nolphin_gid_page_open (GtkWidget *page, GFile *project_folder);

G_END_DECLS

#endif /* NOLPHIN_GID_H */
