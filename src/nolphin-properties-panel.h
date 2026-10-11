/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nolphin-properties-panel.h: Eigenschaften als Seite der rechten
 * Arbeitsleiste - im selben Layout wie die Datei-Vorschau (Symbol, Name,
 * Informationsraster), statt den alten Eigenschaften-Dialog einzubetten.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 */

#ifndef NOLPHIN_PROPERTIES_PANEL_H
#define NOLPHIN_PROPERTIES_PANEL_H

#include <gtk/gtk.h>
#include <libnolphin-private/nolphin-file.h>

G_BEGIN_DECLS

#define NOLPHIN_TYPE_PROPERTIES_PANEL (nolphin_properties_panel_get_type ())
G_DECLARE_FINAL_TYPE (NolphinPropertiesPanel, nolphin_properties_panel, NOLPHIN, PROPERTIES_PANEL, GtkBox)

GtkWidget *nolphin_properties_panel_new (void);

/* @files: Liste von NolphinFile*. Genau ein Objekt: volle Ansicht mit
 * Umbenennen, Zugriffsrechten und "Öffnen mit". Mehrere: Zusammenfassung.
 * Die Liste wird kopiert (die Dateien werden referenziert). */
void nolphin_properties_panel_set_files (NolphinPropertiesPanel *panel, GList *files);

G_END_DECLS

#endif /* NOLPHIN_PROPERTIES_PANEL_H */
