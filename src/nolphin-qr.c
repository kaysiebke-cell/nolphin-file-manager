/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nolphin-qr.c: eigener QR-Code-Encoder
 *
 * Umfang: Byte-Modus, Fehlerkorrektur-Stufe M, Version 1 bis 6 (bis 106
 * Bytes). Das reicht für einen Verbindungslink. Aufbau nach ISO/IEC 18004:
 * Datenbits, Reed-Solomon über GF(256) mit dem Polynom 0x11D, Verschränkung
 * der Blöcke, Funktionsmuster, Zickzack-Platzierung, Maske, Formatinfo.
 */

#include <config.h>

#include "nolphin-qr.h"

#include <string.h>

#define QR_MAX_VERSION 6
#define QR_MAX_CODEWORDS 172

typedef struct {
	gint total_cw;   /* Codewörter insgesamt */
	gint blocks;     /* Anzahl gleich großer Blöcke */
	gint data_cw;    /* Datencodewörter insgesamt */
	gint ec_per_block;
} QrVersionInfo;

/* Stufe M, Index = Version. */
static const QrVersionInfo qr_versions[QR_MAX_VERSION + 1] = {
	{ 0, 0, 0, 0 },
	{  26, 1,  16, 10 },
	{  44, 1,  28, 16 },
	{  70, 1,  44, 26 },
	{ 100, 2,  64, 18 },
	{ 134, 2,  86, 24 },
	{ 172, 4, 108, 16 },
};

typedef struct {
	gint    size;
	guint8 *modules;
	guint8 *is_function;
} QrMatrix;

static inline void
qr_set (QrMatrix *m, gint x, gint y, gboolean dark, gboolean function)
{
	m->modules[y * m->size + x] = dark ? 1 : 0;
	if (function) {
		m->is_function[y * m->size + x] = 1;
	}
}

/* --- GF(256) und Reed-Solomon ------------------------------------------------- */

static guint8
gf_mul (guint8 x, guint8 y)
{
	guint z = 0;
	gint i;

	for (i = 7; i >= 0; i--) {
		z = (z << 1) ^ ((z >> 7) * 0x11D);
		z ^= ((y >> i) & 1) * x;
	}
	return (guint8) z;
}

static void
rs_divisor (gint degree, guint8 *result)
{
	guint8 root = 1;
	gint i, j;

	memset (result, 0, degree);
	result[degree - 1] = 1;
	for (i = 0; i < degree; i++) {
		for (j = 0; j < degree; j++) {
			result[j] = gf_mul (result[j], root);
			if (j + 1 < degree) {
				result[j] ^= result[j + 1];
			}
		}
		root = gf_mul (root, 0x02);
	}
}

static void
rs_remainder (const guint8 *data, gint len, const guint8 *divisor, gint degree, guint8 *result)
{
	gint i, k;

	memset (result, 0, degree);
	for (i = 0; i < len; i++) {
		guint8 factor = data[i] ^ result[0];

		memmove (result, result + 1, degree - 1);
		result[degree - 1] = 0;
		for (k = 0; k < degree; k++) {
			result[k] ^= gf_mul (divisor[k], factor);
		}
	}
}

/* --- Funktionsmuster ----------------------------------------------------------- */

static void
draw_finder (QrMatrix *m, gint cx, gint cy)
{
	gint dx, dy;

	for (dy = -4; dy <= 4; dy++) {
		for (dx = -4; dx <= 4; dx++) {
			gint dist = MAX (ABS (dx), ABS (dy));
			gint x = cx + dx, y = cy + dy;

			if (x >= 0 && x < m->size && y >= 0 && y < m->size) {
				qr_set (m, x, y, dist != 2 && dist != 4, TRUE);
			}
		}
	}
}

static void
draw_alignment (QrMatrix *m, gint cx, gint cy)
{
	gint dx, dy;

	for (dy = -2; dy <= 2; dy++) {
		for (dx = -2; dx <= 2; dx++) {
			qr_set (m, cx + dx, cy + dy, MAX (ABS (dx), ABS (dy)) != 1, TRUE);
		}
	}
}

static void
draw_format (QrMatrix *m, gint mask)
{
	gint size = m->size;
	guint data = (0 << 3) | (guint) mask; /* Stufe M = 00 */
	guint rem = data;
	guint bits;
	gint i;

	for (i = 0; i < 10; i++) {
		rem = (rem << 1) ^ ((rem >> 9) * 0x537);
	}
	bits = ((data << 10) | rem) ^ 0x5412;

#define FBIT(n) (((bits >> (n)) & 1) != 0)
	for (i = 0; i <= 5; i++) {
		qr_set (m, 8, i, FBIT (i), TRUE);
	}
	qr_set (m, 8, 7, FBIT (6), TRUE);
	qr_set (m, 8, 8, FBIT (7), TRUE);
	qr_set (m, 7, 8, FBIT (8), TRUE);
	for (i = 9; i < 15; i++) {
		qr_set (m, 14 - i, 8, FBIT (i), TRUE);
	}
	for (i = 0; i < 8; i++) {
		qr_set (m, size - 1 - i, 8, FBIT (i), TRUE);
	}
	for (i = 8; i < 15; i++) {
		qr_set (m, 8, size - 15 + i, FBIT (i), TRUE);
	}
	qr_set (m, 8, size - 8, TRUE, TRUE); /* immer dunkel */
#undef FBIT
}

static void
draw_function_patterns (QrMatrix *m, gint version)
{
	gint size = m->size, i;

	for (i = 0; i < size; i++) {
		qr_set (m, 6, i, i % 2 == 0, TRUE);
		qr_set (m, i, 6, i % 2 == 0, TRUE);
	}
	draw_finder (m, 3, 3);
	draw_finder (m, size - 4, 3);
	draw_finder (m, 3, size - 4);
	if (version >= 2) {
		gint a = 4 * version + 10;

		draw_alignment (m, a, a);
	}
	draw_format (m, 0); /* reserviert die Bereiche, wird später überschrieben */
}

/* --- Daten ---------------------------------------------------------------------- */

static void
place_data (QrMatrix *m, const guint8 *data, gint len)
{
	gint size = m->size, i = 0, right, vert, j;

	for (right = size - 1; right >= 1; right -= 2) {
		if (right == 6) {
			right = 5;
		}
		for (vert = 0; vert < size; vert++) {
			for (j = 0; j < 2; j++) {
				gint x = right - j;
				gboolean upward = ((right + 1) & 2) == 0;
				gint y = upward ? size - 1 - vert : vert;

				if (!m->is_function[y * size + x] && i < len * 8) {
					m->modules[y * size + x] = (data[i >> 3] >> (7 - (i & 7))) & 1;
					i++;
				}
			}
		}
	}
}

static gboolean
mask_applies (gint mask, gint x, gint y)
{
	switch (mask) {
	case 0: return (x + y) % 2 == 0;
	case 1: return y % 2 == 0;
	case 2: return x % 3 == 0;
	case 3: return (x + y) % 3 == 0;
	case 4: return (x / 3 + y / 2) % 2 == 0;
	case 5: return x * y % 2 + x * y % 3 == 0;
	case 6: return (x * y % 2 + x * y % 3) % 2 == 0;
	default: return ((x + y) % 2 + x * y % 3) % 2 == 0;
	}
}

static void
apply_mask (QrMatrix *m, gint mask)
{
	gint x, y;

	for (y = 0; y < m->size; y++) {
		for (x = 0; x < m->size; x++) {
			if (!m->is_function[y * m->size + x] && mask_applies (mask, x, y)) {
				m->modules[y * m->size + x] ^= 1;
			}
		}
	}
}

/* --- Maskenwahl (Strafpunkte nach Norm) ------------------------------------------ */

static gint
penalty (const QrMatrix *m)
{
	gint size = m->size, x, y, score = 0, dark = 0, k;
#define M(x, y) (m->modules[(y) * size + (x)])

	/* N1: gleichfarbige Reihen und Spalten ab 5 Modulen */
	for (y = 0; y < size; y++) {
		gint run = 1;

		for (x = 1; x <= size; x++) {
			if (x < size && M (x, y) == M (x - 1, y)) {
				run++;
			} else {
				if (run >= 5) {
					score += 3 + (run - 5);
				}
				run = 1;
			}
		}
	}
	for (x = 0; x < size; x++) {
		gint run = 1;

		for (y = 1; y <= size; y++) {
			if (y < size && M (x, y) == M (x, y - 1)) {
				run++;
			} else {
				if (run >= 5) {
					score += 3 + (run - 5);
				}
				run = 1;
			}
		}
	}
	/* N2: 2x2-Blöcke */
	for (y = 0; y < size - 1; y++) {
		for (x = 0; x < size - 1; x++) {
			guint8 c = M (x, y);

			if (c == M (x + 1, y) && c == M (x, y + 1) && c == M (x + 1, y + 1)) {
				score += 3;
			}
		}
	}
	/* N3: Muster 1:1:3:1:1 mit vier hellen Modulen davor oder danach */
	{
		static const guint8 pat[11] = { 1, 0, 1, 1, 1, 0, 1, 0, 0, 0, 0 };

		for (y = 0; y < size; y++) {
			for (x = 0; x + 11 <= size; x++) {
				gboolean a = TRUE, b = TRUE;

				for (k = 0; k < 11; k++) {
					if (M (x + k, y) != pat[k]) {
						a = FALSE;
					}
					if (M (x + k, y) != pat[10 - k]) {
						b = FALSE;
					}
				}
				score += (a ? 40 : 0) + (b ? 40 : 0);
			}
		}
		for (x = 0; x < size; x++) {
			for (y = 0; y + 11 <= size; y++) {
				gboolean a = TRUE, b = TRUE;

				for (k = 0; k < 11; k++) {
					if (M (x, y + k) != pat[k]) {
						a = FALSE;
					}
					if (M (x, y + k) != pat[10 - k]) {
						b = FALSE;
					}
				}
				score += (a ? 40 : 0) + (b ? 40 : 0);
			}
		}
	}
	/* N4: Anteil dunkler Module */
	for (y = 0; y < size; y++) {
		for (x = 0; x < size; x++) {
			dark += M (x, y);
		}
	}
	{
		gint percent = dark * 100 / (size * size);
		gint steps = ABS (percent - 50) / 5;

		score += steps * 10;
	}
#undef M
	return score;
}

/* --- Öffentliche Funktion ---------------------------------------------------------- */

gboolean
nolphin_qr_encode (const gchar *text, gint force_mask, NolphinQr *out)
{
	guint8 data[QR_MAX_CODEWORDS];
	guint8 all[QR_MAX_CODEWORDS];
	guint8 divisor[32];
	guint8 ecc[4][32];
	guint8 modules[NOLPHIN_QR_MAX_SIZE * NOLPHIN_QR_MAX_SIZE];
	guint8 is_function[NOLPHIN_QR_MAX_SIZE * NOLPHIN_QR_MAX_SIZE];
	guint8 best[NOLPHIN_QR_MAX_SIZE * NOLPHIN_QR_MAX_SIZE];
	const QrVersionInfo *vi;
	QrMatrix m;
	gsize len;
	gint version, bitlen = 0, i, b, k, n = 0, mask, best_score = G_MAXINT;
	gint block_data, size;

	g_return_val_if_fail (text != NULL && out != NULL, FALSE);
	g_return_val_if_fail (force_mask >= -1 && force_mask <= 7, FALSE);

	len = strlen (text);
	for (version = 1; version <= QR_MAX_VERSION; version++) {
		if ((gsize) qr_versions[version].data_cw >= len + 2) {
			break;
		}
	}
	if (version > QR_MAX_VERSION) {
		return FALSE;
	}
	vi = &qr_versions[version];

	/* Datenbits: Modus 0100, Länge (8 Bit), Bytes, Abschluss, Füllbytes */
	memset (data, 0, sizeof data);
#define PUT_BITS(val, count) \
	G_STMT_START { \
		for (k = (count) - 1; k >= 0; k--) { \
			if ((((val) >> k) & 1) != 0) { \
				data[bitlen >> 3] |= (guint8) (0x80 >> (bitlen & 7)); \
			} \
			bitlen++; \
		} \
	} G_STMT_END
	PUT_BITS (4, 4);
	PUT_BITS ((guint) len, 8);
	for (i = 0; i < (gint) len; i++) {
		PUT_BITS ((guint8) text[i], 8);
	}
#undef PUT_BITS
	{
		gint capacity_bits = vi->data_cw * 8;

		bitlen += MIN (4, capacity_bits - bitlen);
		bitlen = (bitlen + 7) & ~7;
		for (i = bitlen / 8, k = 0xEC; i < vi->data_cw; i++, k ^= 0xEC ^ 0x11) {
			data[i] = (guint8) k;
		}
	}

	/* Reed-Solomon je Block, danach spaltenweise verschränken */
	block_data = vi->data_cw / vi->blocks;
	rs_divisor (vi->ec_per_block, divisor);
	for (b = 0; b < vi->blocks; b++) {
		rs_remainder (data + b * block_data, block_data, divisor, vi->ec_per_block, ecc[b]);
	}
	for (i = 0; i < block_data; i++) {
		for (b = 0; b < vi->blocks; b++) {
			all[n++] = data[b * block_data + i];
		}
	}
	for (i = 0; i < vi->ec_per_block; i++) {
		for (b = 0; b < vi->blocks; b++) {
			all[n++] = ecc[b][i];
		}
	}
	g_assert (n == vi->total_cw);

	size = version * 4 + 17;
	m.size = size;
	m.modules = modules;
	m.is_function = is_function;
	memset (modules, 0, sizeof modules);
	memset (is_function, 0, sizeof is_function);
	draw_function_patterns (&m, version);
	place_data (&m, all, n);

	for (mask = 0; mask < 8; mask++) {
		if (force_mask >= 0 && mask != force_mask) {
			continue;
		}
		apply_mask (&m, mask);
		draw_format (&m, mask);
		{
			gint score = penalty (&m);

			if (score < best_score) {
				best_score = score;
				memcpy (best, modules, (gsize) size * size);
			}
		}
		apply_mask (&m, mask); /* rückgängig (XOR) */
	}

	out->size = size;
	memset (out->modules, 0, sizeof out->modules);
	memcpy (out->modules, best, (gsize) size * size);
	return TRUE;
}
