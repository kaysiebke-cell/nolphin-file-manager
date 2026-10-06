/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nolphin-preview-module.c: GTK-Modul, das Dateiauswahldialogen eine Vorschau gibt
 *
 * Wird – wie z. B. das xapp-Modul – über die GTK-Einstellung "gtk-modules"
 * in GTK-3-Programme geladen. Öffnet ein Programm einen Dateiauswahldialog
 * zum Öffnen einer Datei (Hochladen, Datei öffnen …) und hat dort keine
 * eigene Vorschau eingerichtet, erscheint rechts die Vorschau von Nolphin.
 * Ordner- und Speichern-Dialoge bleiben unverändert.
 */

#include <gtk/gtk.h>

#include "nolphin-file-chooser-preview.h"

#define DONE_KEY "nolphin-preview-module-done"

static gboolean
on_widget_map (GSignalInvocationHint *hint, guint n_params, const GValue *params, gpointer data)
{
	GtkWidget *widget;
	GtkFileChooser *chooser;

	if (n_params < 1 || !G_VALUE_HOLDS_OBJECT (&params[0])) {
		return TRUE;
	}
	widget = g_value_get_object (&params[0]);
	if (!GTK_IS_FILE_CHOOSER_DIALOG (widget) || g_object_get_data (G_OBJECT (widget), DONE_KEY) != NULL) {
		return TRUE;
	}
	g_object_set_data (G_OBJECT (widget), DONE_KEY, GINT_TO_POINTER (1));

	chooser = GTK_FILE_CHOOSER (widget);
	if (gtk_file_chooser_get_action (chooser) != GTK_FILE_CHOOSER_ACTION_OPEN ||
	    gtk_file_chooser_get_preview_widget (chooser) != NULL) {
		return TRUE;
	}
	nolphin_file_chooser_add_preview (chooser);
	return TRUE;
}

G_MODULE_EXPORT void
gtk_module_init (gint *argc, gchar ***argv)
{
	/* "map" aller Widgets beobachten; der Haken prüft nur auf Dateiauswahldialoge. */
	g_signal_add_emission_hook (g_signal_lookup ("map", GTK_TYPE_WIDGET), 0, on_widget_map, NULL, NULL);
}
