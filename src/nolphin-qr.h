/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nolphin-qr.h: eigener QR-Code-Encoder (Byte-Modus, Fehlerkorrektur M,
 * Version 1 bis 6). Keine Bibliothek, nur GLib.
 */

#ifndef NOLPHIN_QR_H
#define NOLPHIN_QR_H

#include <glib.h>

G_BEGIN_DECLS

/* Version 6 hat 41 x 41 Module. */
#define NOLPHIN_QR_MAX_SIZE 41

typedef struct {
	gint   size;                                          /* Module je Kante */
	guint8 modules[NOLPHIN_QR_MAX_SIZE * NOLPHIN_QR_MAX_SIZE]; /* 1 = dunkel */
} NolphinQr;

/* Kodiert @text (UTF-8, höchstens 106 Bytes). @force_mask: -1 wählt die
 * Maske mit der geringsten Strafe, 0 bis 7 erzwingt eine Maske (Tests).
 * Gibt FALSE zurück, wenn der Text zu lang ist. */
gboolean nolphin_qr_encode (const gchar *text, gint force_mask, NolphinQr *out);

static inline gboolean
nolphin_qr_get (const NolphinQr *qr, gint x, gint y)
{
	return qr->modules[y * qr->size + x] != 0;
}

G_END_DECLS

#endif /* NOLPHIN_QR_H */
