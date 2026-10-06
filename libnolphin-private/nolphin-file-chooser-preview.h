/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nolphin-file-chooser-preview.h: Vorschau rechts in Dateiauswahldialogen
 *
 * Hängt an einen GtkFileChooser eine Vorschau für die markierte Datei:
 * Bilder und vorhandene Vorschaubilder (PDF, Video …) als Bild, Textdateien
 * mit den ersten Zeilen, sonst Symbol, Typ, Größe und Änderungsdatum.
 */

#ifndef NOLPHIN_FILE_CHOOSER_PREVIEW_H
#define NOLPHIN_FILE_CHOOSER_PREVIEW_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

void nolphin_file_chooser_add_preview (GtkFileChooser *chooser);

G_END_DECLS

#endif /* NOLPHIN_FILE_CHOOSER_PREVIEW_H */
