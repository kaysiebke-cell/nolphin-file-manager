/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nolphin-media.h: erweiterte Vorschau für PDF, Video und Audio (§34)
 *
 * PDF-Angaben und die Vorschau der ersten Seite kommen von pdfinfo und
 * pdftocairo (Paket poppler-utils), Video-/Audio-Angaben von
 * gst-discoverer-1.0 (Paket gstreamer1.0-tools). Beides läuft asynchron
 * in einem Worker-Thread über kontrollierte Subprozesse. Fehlt ein
 * Werkzeug, wird das als Hinweis geliefert statt eine Vorschau vorzutäuschen.
 */

#ifndef NOLPHIN_MEDIA_H
#define NOLPHIN_MEDIA_H

#include <gio/gio.h>

G_BEGIN_DECLS

typedef struct {
	gchar *label;
	gchar *value;
} NolphinMediaRow;

typedef struct {
	GPtrArray *rows;      /* NolphinMediaRow*, schon deutsch beschriftet */
	GBytes    *thumb_png; /* erste PDF-Seite als PNG, sonst NULL */
	gchar     *notice;    /* Hinweis, z. B. fehlendes Werkzeug; sonst NULL */
} NolphinMediaInfo;

/* Gibt es für diesen MIME-Typ eine erweiterte Vorschau (PDF, Video, Audio)? */
gboolean nolphin_media_is_supported (const gchar *mime_type);

void               nolphin_media_get_info_async  (GFile *location, const gchar *mime_type,
						  GCancellable *cancellable,
						  GAsyncReadyCallback callback, gpointer user_data);
NolphinMediaInfo  *nolphin_media_get_info_finish (GAsyncResult *result, GError **error);
void               nolphin_media_info_free       (NolphinMediaInfo *info);

G_END_DECLS

#endif /* NOLPHIN_MEDIA_H */
