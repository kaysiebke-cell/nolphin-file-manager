/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nolphin-tools.h: Werkzeug-Seiten im rechten Arbeitsbereich (Phase 3)
 *
 * Duplikate finden (§46), Versionen (§45), Ordner synchronisieren (§44) und
 * Regeln/Stapelverarbeitung (§43). Jede Seite läuft im Panel, ohne eigenes
 * Fenster.
 */

#ifndef NOLPHIN_TOOLS_H
#define NOLPHIN_TOOLS_H

#include <gtk/gtk.h>
#include <gio/gio.h>
#include "nolphin-window.h"

G_BEGIN_DECLS

/* Gemeinsamer "Zur Vorschau"-Knopf für alle Werkzeug-Seiten. */
GtkWidget *nolphin_tools_back_button_new (NolphinWindow *window);

/* Seitenüberschrift im einheitlichen Stil */
GtkWidget *nolphin_tools_heading_new (const gchar *text);

/* Duplikate finden (§46) */
GtkWidget *nolphin_duplicates_page_new (NolphinWindow *window);
void       nolphin_duplicates_page_open (GtkWidget *page, GFile *folder);

/* Versionen (§45) */
GtkWidget *nolphin_versions_page_new  (NolphinWindow *window);
void       nolphin_versions_page_open (GtkWidget *page, GFile *file);
/* Speichert den aktuellen Stand von @file sofort als neue Version. */
gboolean   nolphin_versions_save_file (GFile *file, const gchar *comment, GError **error);

/* Ordner vergleichen / synchronisieren (§44) */
GtkWidget *nolphin_sync_page_new  (NolphinWindow *window);
void       nolphin_sync_page_open (GtkWidget *page, GFile *local_folder);

/* Regeln anwenden und Stapelverarbeitung (§43). @selection: GList von Pfaden
 * (gchar *); NULL = Ordner @folder durchsuchen. */
GtkWidget *nolphin_rules_page_new  (NolphinWindow *window);
void       nolphin_rules_page_open (GtkWidget *page, GFile *folder, GList *selection_paths, gboolean batch_mode);

G_END_DECLS

#endif /* NOLPHIN_TOOLS_H */
