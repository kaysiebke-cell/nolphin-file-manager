/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nolphin-xapp-compat.h: libxapp ist optional
 *
 * Mit libxapp (HAVE_XAPP) werden dessen Header unverändert benutzt. Ohne
 * libxapp (z. B. XFCE/MATE ohne Linux-Mint-Pakete) stehen hier Ersatz-
 * definitionen mit denselben Namen bereit:
 *   - Favoriten: es gibt keine, alle Funktionen tun nichts bzw. liefern
 *     "keine Favoriten". Die Favoriten-Menüpunkte werden ausgeblendet
 *     (siehe nolphin-view.c).
 *   - Statussymbol: GtkStatusIcon.
 *   - Fenster: normales GtkWindow; die Fortschrittsanzeige in der
 *     Fensterleiste entfällt.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 */

#ifndef NOLPHIN_XAPP_COMPAT_H
#define NOLPHIN_XAPP_COMPAT_H

#include <config.h>
#include <gtk/gtk.h>

#ifdef HAVE_XAPP

#include <libxapp/xapp-favorites.h>
#include <libxapp/xapp-gtk-window.h>
#include <libxapp/xapp-status-icon.h>

#else /* !HAVE_XAPP */

/* --- Favoriten: nicht vorhanden --- */

typedef struct {
    gchar *uri;
} XAppFavoriteInfo;

typedef GObject XAppFavorites;

static inline XAppFavorites *xapp_favorites_get_default (void) { return NULL; }
static inline XAppFavoriteInfo *xapp_favorites_find_by_uri (XAppFavorites *f, const gchar *uri) { return NULL; }
static inline gint xapp_favorites_get_n_favorites (XAppFavorites *f) { return 0; }
static inline GList *xapp_favorites_get_favorites (XAppFavorites *f, const gchar * const *mime_types) { return NULL; }
static inline void xapp_favorites_add (XAppFavorites *f, const gchar *uri) { }
static inline void xapp_favorites_remove (XAppFavorites *f, const gchar *uri) { }
static inline void xapp_favorites_rename (XAppFavorites *f, const gchar *old_uri, const gchar *new_uri) { }
static inline void xapp_favorite_info_free (XAppFavoriteInfo *info) { }

/* --- Statussymbol --- */

typedef GtkStatusIcon XAppStatusIcon;

static inline XAppStatusIcon *xapp_status_icon_new (void) { return gtk_status_icon_new (); }
static inline void xapp_status_icon_set_icon_name (XAppStatusIcon *i, const gchar *n) { gtk_status_icon_set_from_icon_name (i, n); }
static inline void xapp_status_icon_set_tooltip_text (XAppStatusIcon *i, const gchar *t) { gtk_status_icon_set_tooltip_text (i, t); }
static inline void xapp_status_icon_set_visible (XAppStatusIcon *i, gboolean v) { gtk_status_icon_set_visible (i, v); }

/* --- Fenster --- */

#define XAPP_GTK_WINDOW(w) GTK_WINDOW (w)

static inline GtkWidget *xapp_gtk_window_new (GtkWindowType type) { return gtk_window_new (type); }
static inline void xapp_gtk_window_set_icon_name (GtkWindow *w, const gchar *n) { gtk_window_set_icon_name (w, n); }
static inline void xapp_gtk_window_set_progress (GtkWindow *w, gint percent) { }

#endif /* HAVE_XAPP */

#endif /* NOLPHIN_XAPP_COMPAT_H */
