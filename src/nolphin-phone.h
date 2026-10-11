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

G_END_DECLS

#endif /* NOLPHIN_PHONE_H */
