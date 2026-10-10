/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nolphin-gid-menu.h: Menüeinträge der GID-Projekte (§60.4)
 *
 * Gehe zu ▸ GID-Projekte (Liste der Projekte, Projekt hinzufügen …) und
 * Ansicht ▸ Seitenleiste ▸ GID-Projekte anzeigen. Kein neues Hauptmenü.
 */

#ifndef NOLPHIN_GID_MENU_H
#define NOLPHIN_GID_MENU_H

#include "nolphin-window.h"

G_BEGIN_DECLS

void nolphin_gid_menu_register_actions (NolphinWindow *window);
void nolphin_gid_menu_initialize (NolphinWindow *window);

G_END_DECLS

#endif /* NOLPHIN_GID_MENU_H */
