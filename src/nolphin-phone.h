/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nolphin-phone.h: Handy-Austausch als Seite im rechten Arbeitsbereich
 */

#ifndef NOLPHIN_PHONE_H
#define NOLPHIN_PHONE_H

#include <gtk/gtk.h>
#include "nolphin-window.h"

G_BEGIN_DECLS

GtkWidget *nolphin_phone_page_new  (NolphinWindow *window);
/* Aktualisiert beim Öffnen der Seite die Liste der Netzwerkadressen und
 * startet bei @autostart den Austausch, falls er noch nicht läuft. */
void       nolphin_phone_page_open (GtkWidget *page, gboolean autostart);

/* Für die Menüeinträge „Neuen QR-Code erzeugen“ und „Austausch beenden“. */
void       nolphin_phone_page_rotate (GtkWidget *page);
void       nolphin_phone_page_stop   (GtkWidget *page);

/* Ordner: Basis aus GSettings (Vorgabe ~/Handy) (Ausgang: alle Dateien darin) mit "Vom Handy" (Eingang).
 * Die Pfade sind neu zugewiesen und mit g_free () freizugeben. */
gchar     *nolphin_phone_base_path    (void);
gchar     *nolphin_phone_inbox_path   (void);
gchar     *nolphin_phone_outbox_path  (void);
/* Legt beide Ordner an, falls sie fehlen. */
gboolean   nolphin_phone_ensure_folders (void);

G_END_DECLS

#endif /* NOLPHIN_PHONE_H */
