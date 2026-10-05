/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nolphin-gid-sidebar.h: Bereich "GID-Projekte" der Seitenleiste (§60.2)
 *
 * Ein auf- und zuklappbarer Bereich am unteren Rand der Seitenleiste, der
 * zusätzlich zu Orte bzw. Ordnerbaum erscheint und diese nicht verändert.
 */

#ifndef NOLPHIN_GID_SIDEBAR_H
#define NOLPHIN_GID_SIDEBAR_H

#include <gtk/gtk.h>
#include "nolphin-window.h"

G_BEGIN_DECLS

GtkWidget *nolphin_gid_sidebar_new (NolphinWindow *window);

/* Fragt per Ordnerauswahl einen Projektordner ab und fügt ihn hinzu
 * (§60.2, Kontextmenü/Menü). Der Auswahldialog ist ein nativer
 * Systemdialog (§58.1.2). */
void       nolphin_gid_choose_and_add_project (NolphinWindow *window);

G_END_DECLS

#endif /* NOLPHIN_GID_SIDEBAR_H */
