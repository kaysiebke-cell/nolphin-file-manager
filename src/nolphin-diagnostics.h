/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nolphin-diagnostics.h: Verwaltung und Diagnose (§48)
 *
 * Protokolle lokal speichern und anzeigen, Systeminformationen, Plugin-Status
 * und Fehlerberichte als lokale Datei (kein automatisches Senden).
 */

#ifndef NOLPHIN_DIAGNOSTICS_H
#define NOLPHIN_DIAGNOSTICS_H

#include <gtk/gtk.h>
#include "nolphin-window.h"

G_BEGIN_DECLS

typedef enum {
	NOLPHIN_DIAG_TAB_LOGS,
	NOLPHIN_DIAG_TAB_SYSTEM,
	NOLPHIN_DIAG_TAB_PLUGINS,
	NOLPHIN_DIAG_TAB_REPORT
} NolphinDiagTab;

/* Leitet alle GLib-Meldungen zusätzlich in ~/.local/share/nolphin/logs/nolphin.log. */
void   nolphin_diagnostics_init_logging (void);

/* TRUE, wenn Nolphin nicht aus dem installierten Paket (/usr/…), sondern
 * z. B. aus dem Build-Ordner läuft. Liefert den Programmpfad über @path
 * (mit g_free() freigeben, darf NULL sein). */
gboolean nolphin_is_development_build (gchar **path);

gchar *nolphin_diagnostics_get_log_path    (void);
gchar *nolphin_diagnostics_read_log_tail   (guint max_lines);
gchar *nolphin_diagnostics_system_info     (void);
gchar *nolphin_diagnostics_plugin_status   (void);
gboolean nolphin_diagnostics_write_report  (const gchar *path, GError **error);

/* Seite für den rechten Arbeitsbereich. */
GtkWidget *nolphin_diagnostics_page_new      (NolphinWindow *window);
void       nolphin_diagnostics_page_show_tab (GtkWidget *page, NolphinDiagTab tab);

G_END_DECLS

#endif /* NOLPHIN_DIAGNOSTICS_H */
