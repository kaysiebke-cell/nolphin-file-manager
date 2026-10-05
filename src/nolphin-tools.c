/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nolphin-tools.c: gemeinsame Bausteine der Werkzeug-Seiten
 */

#include <config.h>

#include "nolphin-tools.h"
#include "nolphin-workspace-panel.h"

#include <glib/gi18n.h>

static void
on_back_clicked (GtkButton *button, gpointer data)
{
	nolphin_workspace_panel_show_preview (nolphin_window_get_workspace_panel (NOLPHIN_WINDOW (data)));
}

GtkWidget *
nolphin_tools_back_button_new (NolphinWindow *window)
{
	GtkWidget *b = gtk_button_new_with_label (_("Zur Vorschau"));

	gtk_button_set_image (GTK_BUTTON (b), gtk_image_new_from_icon_name ("go-previous-symbolic", GTK_ICON_SIZE_BUTTON));
	gtk_button_set_always_show_image (GTK_BUTTON (b), TRUE);
	gtk_widget_set_halign (b, GTK_ALIGN_START);
	g_signal_connect (b, "clicked", G_CALLBACK (on_back_clicked), window);
	return b;
}

GtkWidget *
nolphin_tools_heading_new (const gchar *text)
{
	GtkWidget *l = gtk_label_new (text);

	gtk_widget_set_halign (l, GTK_ALIGN_START);
	gtk_style_context_add_class (gtk_widget_get_style_context (l), "heading");
	return l;
}
