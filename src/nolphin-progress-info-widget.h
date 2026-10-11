/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nolphin-progress-info-widget.h: file operation progress user interface.
 *
 * Copyright (C) 2007, 2011 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
 * Boston, MA 02110-1335, USA.
 *
 * Authors: Alexander Larsson <alexl@redhat.com>
 *          Cosimo Cecchi <cosimoc@redhat.com>
 *
 */

#ifndef __NOLPHIN_PROGRESS_INFO_WIDGET_H__
#define __NOLPHIN_PROGRESS_INFO_WIDGET_H__

#include <gtk/gtk.h>

#include <libnolphin-private/nolphin-progress-info.h>

#define NOLPHIN_TYPE_PROGRESS_INFO_WIDGET nolphin_progress_info_widget_get_type()
#define NOLPHIN_PROGRESS_INFO_WIDGET(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST ((obj), NOLPHIN_TYPE_PROGRESS_INFO_WIDGET, NolphinProgressInfoWidget))
#define NOLPHIN_PROGRESS_INFO_WIDGET_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST ((klass), NOLPHIN_TYPE_PROGRESS_INFO_WIDGET, NolphinProgressInfoWidgetClass))
#define NOLPHIN_IS_PROGRESS_INFO_WIDGET(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE ((obj), NOLPHIN_TYPE_PROGRESS_INFO_WIDGET))
#define NOLPHIN_IS_PROGRESS_INFO_WIDGET_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE ((klass), NOLPHIN_TYPE_PROGRESS_INFO_WIDGET))
#define NOLPHIN_PROGRESS_INFO_WIDGET_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS ((obj), NOLPHIN_TYPE_PROGRESS_INFO_WIDGET, NolphinProgressInfoWidgetClass))

typedef struct _NolphinProgressInfoWidgetPriv NolphinProgressInfoWidgetPriv;

typedef struct {
	GtkBox parent;

	/* private */
	NolphinProgressInfoWidgetPriv *priv;
} NolphinProgressInfoWidget;

typedef struct {
	GtkBoxClass parent_class;
} NolphinProgressInfoWidgetClass;

struct _NolphinProgressInfoWidgetPriv {
    NolphinProgressInfo *info;

    GtkWidget *stack;
    GtkWidget *separator;

    /* pre-start page */
    GtkWidget *pre_info; /* GtkLabel */

    GtkWidget *pending_start_pause_button;
    GtkWidget *running_start_pause_button;

    GtkWidget *status; /* GtkLabel */
    GtkWidget *details; /* GtkLabel */
    GtkWidget *progress_bar;
};

GType nolphin_progress_info_widget_get_type (void);

GtkWidget * nolphin_progress_info_widget_new (NolphinProgressInfo *info);

#endif /* __NOLPHIN_PROGRESS_INFO_WIDGET_H__ */
