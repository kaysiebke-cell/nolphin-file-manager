/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006 Paolo Borelli <pborelli@katamail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street - Suite 500, Boston, MA 02110-1335, USA.
 *
 * Authors: Paolo Borelli <pborelli@katamail.com>
 *
 */

#include "config.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "nolphin-trash-bar.h"

#include "nolphin-view.h"
#include <libnolphin-private/nolphin-file-operations.h>
#include <libnolphin-private/nolphin-file-utilities.h>
#include <libnolphin-private/nolphin-file.h>
#include <libnolphin-private/nolphin-trash-monitor.h>

#define NOLPHIN_TRASH_BAR_GET_PRIVATE(o)\
	(G_TYPE_INSTANCE_GET_PRIVATE ((o), NOLPHIN_TYPE_TRASH_BAR, NolphinTrashBarPrivate))

enum {
	PROP_VIEW = 1,
	NUM_PROPERTIES
};

enum {
	TRASH_BAR_RESPONSE_EMPTY = 1,
	TRASH_BAR_RESPONSE_RESTORE
};

struct NolphinTrashBarPrivate
{
	NolphinView *view;
	gulong selection_handler_id;
};

G_DEFINE_TYPE (NolphinTrashBar, nolphin_trash_bar, GTK_TYPE_INFO_BAR);

static void
selection_changed_cb (NolphinView *view,
		      NolphinTrashBar *bar)
{
	int count;
    gchar *uri;

    uri = nolphin_view_get_uri (view);

	count = nolphin_view_get_selection_count (view);

    /* You can't currently restore individual files that aren't in the trash:/// root,
     * so make the button insensitive if that's the case.  We should allow it (along with
     * deleting individual files permanently) but it's not trivial. */

	gtk_info_bar_set_response_sensitive (GTK_INFO_BAR (bar),
					     TRASH_BAR_RESPONSE_RESTORE,
					     (count > 0 && g_strcmp0 (uri, "trash:///") == 0));

    g_free (uri);
}

static void
disconnect_view (NolphinTrashBar *bar)
{
    if (bar->priv->selection_handler_id != 0) {
        g_signal_handler_disconnect (bar->priv->view, bar->priv->selection_handler_id);
        bar->priv->selection_handler_id = 0;
    }
}

static void
connect_view_and_update_button (NolphinTrashBar *bar)
{
    bar->priv->selection_handler_id =
    g_signal_connect (bar->priv->view, "selection-changed",
                      G_CALLBACK (selection_changed_cb), bar);

    g_signal_connect_object (bar->priv->view, "destroy",
                             G_CALLBACK (disconnect_view), bar,
                             G_CONNECT_SWAPPED);

	selection_changed_cb (bar->priv->view, bar);
}

static void
nolphin_trash_bar_set_property (GObject      *object,
				 guint         prop_id,
				 const GValue *value,
				 GParamSpec   *pspec)
{
	NolphinTrashBar *bar;

	bar = NOLPHIN_TRASH_BAR (object);

	switch (prop_id) {
	case PROP_VIEW:
		bar->priv->view = g_value_get_object (value);
		connect_view_and_update_button (bar);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
nolphin_trash_bar_dispose (GObject *obj)
{
    NolphinTrashBar *bar;

    bar = NOLPHIN_TRASH_BAR (obj);

    disconnect_view (bar);

    G_OBJECT_CLASS (nolphin_trash_bar_parent_class)->dispose (obj);
}

static void
nolphin_trash_bar_trash_state_changed (NolphinTrashMonitor *trash_monitor,
					gboolean              state,
					gpointer              data)
{
	NolphinTrashBar *bar;

	bar = NOLPHIN_TRASH_BAR (data);

	gtk_info_bar_set_response_sensitive (GTK_INFO_BAR (bar),
					     TRASH_BAR_RESPONSE_EMPTY,
					     !nolphin_trash_monitor_is_empty ());
}

static void
nolphin_trash_bar_class_init (NolphinTrashBarClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	object_class->set_property = nolphin_trash_bar_set_property;
	object_class->dispose = nolphin_trash_bar_dispose;

	g_object_class_install_property (object_class,
					 PROP_VIEW,
					 g_param_spec_object ("view",
							      "view",
							      "the NolphinView",
							      NOLPHIN_TYPE_VIEW,
							      G_PARAM_WRITABLE |
							      G_PARAM_CONSTRUCT_ONLY |
							      G_PARAM_STATIC_STRINGS));

	g_type_class_add_private (klass, sizeof (NolphinTrashBarPrivate));
}

static void
trash_bar_response_cb (GtkInfoBar *infobar,
		       gint response_id,
		       gpointer user_data)
{
	NolphinTrashBar *bar;
	GtkWidget *window;
	GList *files;

	bar = NOLPHIN_TRASH_BAR (infobar);
	window = gtk_widget_get_toplevel (GTK_WIDGET (bar));

	switch (response_id) {
	case TRASH_BAR_RESPONSE_EMPTY:
		nolphin_file_operations_empty_trash (window);
		break;
	case TRASH_BAR_RESPONSE_RESTORE:
		files = nolphin_view_get_selection (bar->priv->view);
		nolphin_restore_files_from_trash (files, GTK_WINDOW (window));
		nolphin_file_list_free (files);
		break;
	default:
		break;
	}
}

static void
nolphin_trash_bar_init (NolphinTrashBar *bar)
{
	GtkWidget *content_area, *action_area, *w;
	GtkWidget *label;

	bar->priv = NOLPHIN_TRASH_BAR_GET_PRIVATE (bar);
	content_area = gtk_info_bar_get_content_area (GTK_INFO_BAR (bar));
	action_area = gtk_info_bar_get_action_area (GTK_INFO_BAR (bar));

	gtk_orientable_set_orientation (GTK_ORIENTABLE (action_area),
					GTK_ORIENTATION_HORIZONTAL);

	label = gtk_label_new (_("Papierkorb"));
	gtk_style_context_add_class (gtk_widget_get_style_context (label),
				     "nolphin-cluebar-label");
	gtk_widget_show (label);
	gtk_container_add (GTK_CONTAINER (content_area), label);

	w = gtk_info_bar_add_button (GTK_INFO_BAR (bar),
				     _("Ausgewählte Objekte wiederherstellen"),
				     TRASH_BAR_RESPONSE_RESTORE);
	gtk_widget_set_tooltip_text (w,
				     _("Ausgewählte Objekte an ihrem ursprünglichen Ort wiederherstellen"));

	w = gtk_info_bar_add_button (GTK_INFO_BAR (bar),
				     _("_Papierkorb leeren"),
				     TRASH_BAR_RESPONSE_EMPTY);
	gtk_widget_set_tooltip_text (w,
				     _("Alle Objekte im Papierkorb löschen"));

	g_signal_connect_object (nolphin_trash_monitor_get (),
				 "trash_state_changed",
				 G_CALLBACK (nolphin_trash_bar_trash_state_changed),
				 bar,
				 0);
	nolphin_trash_bar_trash_state_changed (nolphin_trash_monitor_get (),
						FALSE, bar);

	g_signal_connect (bar, "response",
			  G_CALLBACK (trash_bar_response_cb), bar);
}

GtkWidget *
nolphin_trash_bar_new (NolphinView *view)
{
	return g_object_new (NOLPHIN_TYPE_TRASH_BAR,
			     "view", view,
			     NULL);
}
