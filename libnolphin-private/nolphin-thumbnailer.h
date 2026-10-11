/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nolphin-thumbnailer.h: eigene Vorschaubild-Erzeugung nach der
 * Freedesktop-Thumbnail-Spezifikation
 *
 * Wird benutzt, wenn cinnamon-desktop fehlt oder dessen Schema
 * org.cinnamon.desktop.thumbnailers nicht installiert ist (z. B. XFCE, MATE).
 * Liest/schreibt ~/.cache/thumbnails (large, fail) im selben Format wie
 * GNOME/Cinnamon, sodass beide Seiten dieselben Vorschaubilder nutzen.
 * Bilder erzeugt gdk-pixbuf direkt, alle anderen Typen die installierten
 * *.thumbnailer-Programme (XDG_DATA_DIRS/thumbnailers), die als Argumentliste
 * ohne Shell gestartet werden.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 */

#ifndef NOLPHIN_THUMBNAILER_H
#define NOLPHIN_THUMBNAILER_H

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <time.h>

G_BEGIN_DECLS

/* Größe der "large"-Vorschaubilder */
#define NOLPHIN_THUMBNAILER_LARGE_SIZE 256

/* Schnell (kein Prozessstart): Ist für diesen Typ ein Vorschaubild möglich
 * (Bild-Format oder installierter Thumbnailer), liegt die Datei lokal vor,
 * und hat die Erzeugung nicht schon für diese Änderungszeit versagt? */
gboolean   nolphin_thumbnailer_can_thumbnail  (const gchar *uri,
                                               const gchar *mime_type,
                                               time_t       mtime);

/* Blockiert (startet ggf. ein Programm, höchstens 30 s) - nur aus einem
 * Arbeits-Thread aufrufen. NULL, wenn kein Vorschaubild entstand. */
GdkPixbuf *nolphin_thumbnailer_generate       (const gchar *uri,
                                               const gchar *mime_type);

/* Legt das Vorschaubild im Zwischenspeicher ab / vermerkt einen Fehlschlag. */
void       nolphin_thumbnailer_save           (GdkPixbuf   *pixbuf,
                                               const gchar *uri,
                                               time_t       mtime);
void       nolphin_thumbnailer_create_failed  (const gchar *uri,
                                               time_t       mtime);

/* Pfad der Vorschaubild- bzw. Fehlschlag-Datei für @uri (neu zugewiesen). */
gchar     *nolphin_thumbnailer_path_for_uri   (const gchar *uri, gboolean failed);

/* Ist der Zwischenspeicher für den Benutzer beschreibbar? */
gboolean   nolphin_thumbnailer_cache_ok       (void);

G_END_DECLS

#endif /* NOLPHIN_THUMBNAILER_H */
