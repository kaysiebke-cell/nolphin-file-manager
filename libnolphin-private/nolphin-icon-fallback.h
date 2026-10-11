/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nolphin-icon-fallback.h: Ersatz für fehlende xsi-Symbole
 *
 * Nolphin benennt viele Menü-, Werkzeugleisten- und Seitenleisten-Symbole
 * "xsi-<name>-symbolic". Diese kommen vom Linux-Mint-Paket
 * xapp-symbolic-icons. Fehlt es (z. B. XFCE oder MATE ohne Mint-Pakete),
 * zeigte Nolphin Verbotsschilder statt Symbolen und wirkte wie ein Fremdkörper.
 *
 * Hier wird zur Laufzeit, ohne fremde Dateien mitzuliefern, für jedes fehlende
 * Symbol ein Verweis auf das passende Standardsymbol des aktuellen Themas
 * angelegt (z. B. xsi-edit-copy-symbolic -> edit-copy-symbolic), in einem
 * eigenen Ordner im Benutzer-Cache. Vorhandene xsi-Symbole werden nie ersetzt.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 */

#ifndef NOLPHIN_ICON_FALLBACK_H
#define NOLPHIN_ICON_FALLBACK_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

/* Alle xsi-Namen, die Nolphin benutzt (NULL-terminiert). Ein Test prüft, dass
 * jeder im Quellcode vorkommende Name hier steht. */
const gchar * const *nolphin_icon_fallback_get_names (void);

/* Legt in @link_dir Verweise für alle in @theme fehlenden xsi-Symbole an und
 * hängt @link_dir an den Suchpfad von @theme an. Mehrfaches Aufrufen ist
 * möglich (z. B. nach einem Themenwechsel). Gibt die Zahl der Ersatz-Verweise
 * zurück. */
guint nolphin_icon_fallback_apply (GtkIconTheme *theme, const gchar *link_dir);

/* Wendet das auf das Standardthema an (Ordner: ~/.cache/nolphin/xsi-fallback)
 * und wiederholt es bei einem Wechsel des Symbolthemas. */
void  nolphin_icon_fallback_init (void);

G_END_DECLS

#endif /* NOLPHIN_ICON_FALLBACK_H */
