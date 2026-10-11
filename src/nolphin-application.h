/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nolphin-application: main Nolphin application class.
 *
 * Copyright (C) 2000 Red Hat, Inc.
 * Copyright (C) 2010 Cosimo Cecchi <cosimoc@gnome.org>
 *
 * Nolphin is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nolphin is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
 * Boston, MA 02110-1335, USA.
 */

#ifndef __NOLPHIN_APPLICATION_H__
#define __NOLPHIN_APPLICATION_H__

#include <gdk/gdk.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

#include <libnolphin-private/nolphin-undo-manager.h>

#include "nolphin-window.h"

#define NOLPHIN_TYPE_APPLICATION nolphin_application_get_type()
#define NOLPHIN_APPLICATION(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), NOLPHIN_TYPE_APPLICATION, NolphinApplication))
#define NOLPHIN_APPLICATION_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), NOLPHIN_TYPE_APPLICATION, NolphinApplicationClass))
#define NOLPHIN_IS_APPLICATION(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NOLPHIN_TYPE_APPLICATION))
#define NOLPHIN_IS_APPLICATION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), NOLPHIN_TYPE_APPLICATION))
#define NOLPHIN_APPLICATION_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), NOLPHIN_TYPE_APPLICATION, NolphinApplicationClass))

typedef struct _NolphinApplicationPriv NolphinApplicationPriv;
typedef struct NolphinApplicationClass NolphinApplicationClass;

typedef struct {
	GtkApplication parent;

	NolphinUndoManager *undo_manager;

	NolphinApplicationPriv *priv;
} NolphinApplication;

struct NolphinApplicationClass {
	GtkApplicationClass parent_class;

    void         (* continue_startup) (NolphinApplication *application);
    void         (* continue_quit) (NolphinApplication *application);

    void         (* open_location) (NolphinApplication *application,
                                    GFile *location,
                                    GFile *selection,
                                    const char *startup_id,
                                    const gboolean open_in_tabs);

    void         (* show_items)    (NolphinApplication *application,
                                    GFile          **uris,
                                    gint             n_uris,
                                    const char      *startup_id);

    NolphinWindow * (* create_window) (NolphinApplication *application,
                                    GdkScreen       *screen);

    void         (* notify_unmount_done) (NolphinApplication *application,
                                          const gchar *message);

    void         (* notify_unmount_show) (NolphinApplication *application,
                                          const gchar *message);

    void         (* close_all_windows)   (NolphinApplication *application);

};

GType nolphin_application_get_type (void);
NolphinApplication *nolphin_application_initialize_singleton (GType object_type,
                                                        const gchar *first_property_name,
                                                        ...);
NolphinApplication *nolphin_application_get_singleton (void);
void nolphin_application_quit (NolphinApplication *self);
NolphinWindow *     nolphin_application_create_window (NolphinApplication *application,
                                                 GdkScreen           *screen);
void nolphin_application_open_location (NolphinApplication *application,
                                     GFile *location,
                                     GFile *selection,
                                     const char *startup_id,
                                     const gboolean open_in_tabs);
void nolphin_application_show_items    (NolphinApplication *application,
                                     GFile          **uris,
                                     gint             n_uris,
                                     const char      *startup_id);
void nolphin_application_close_all_windows (NolphinApplication *self);

void nolphin_application_notify_unmount_show (NolphinApplication *application,
                                               const gchar *message);

void nolphin_application_notify_unmount_done (NolphinApplication *application,
                                               const gchar *message);
gboolean nolphin_application_check_required_directory (NolphinApplication *application,
                                                    gchar           *path);
void nolphin_application_check_thumbnail_cache (NolphinApplication *application);
gboolean nolphin_application_get_cache_bad (NolphinApplication *application);
void nolphin_application_clear_cache_flag (NolphinApplication *application);
void nolphin_application_set_cache_flag (NolphinApplication *application);
void nolphin_application_ignore_cache_problem (NolphinApplication *application);
gboolean nolphin_application_get_cache_problem_ignored (NolphinApplication *application);
gboolean nolphin_application_get_show_desktop (NolphinApplication *application);

#endif /* __NOLPHIN_APPLICATION_H__ */
