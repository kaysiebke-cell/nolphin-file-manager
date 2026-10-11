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
	if (g_getenv ("NOLPHIN_PREVIEW_DEBUG") != NULL && GTK_IS_DIALOG (widget)) {
		g_printerr ("nolphin-preview: map %s (Dateiauswahl: %d)\n", G_OBJECT_TYPE_NAME (widget),
			    GTK_IS_FILE_CHOOSER_DIALOG (widget));
	}
	if (!GTK_IS_FILE_CHOOSER_DIALOG (widget) || g_object_get_data (G_OBJECT (widget), DONE_KEY) != NULL) {
		return TRUE;
	}
	g_object_set_data (G_OBJECT (widget), DONE_KEY, GINT_TO_POINTER (1));

	chooser = GTK_FILE_CHOOSER (widget);
	if (g_getenv ("NOLPHIN_PREVIEW_DEBUG") != NULL) {
		g_printerr ("nolphin-preview: Dateiauswahl, Aktion %d, Vorschau-Widget %p (%s)\n",
			    gtk_file_chooser_get_action (chooser), (void *) gtk_file_chooser_get_preview_widget (chooser),
			    gtk_file_chooser_get_preview_widget (chooser) != NULL ? G_OBJECT_TYPE_NAME (gtk_file_chooser_get_preview_widget (chooser)) : "-");
	}
	/* Eine einfache Bildvorschau (GtkImage, so setzt sie z. B. der Dateiauswahl-Dienst
	 * xdg-desktop-portal-gtk) wird ersetzt; eigene Vorschau-Widgets anderer
	 * Programme (GIMP, Bildbetrachter …) bleiben unberührt. */
	if (gtk_file_chooser_get_action (chooser) != GTK_FILE_CHOOSER_ACTION_OPEN ||
	    (gtk_file_chooser_get_preview_widget (chooser) != NULL &&
	     !GTK_IS_IMAGE (gtk_file_chooser_get_preview_widget (chooser)))) {
		return TRUE;
	}
	/* Das ersetzte Widget am Leben halten: Der Besitzer des Dialogs (z. B. der
	 * Portal-Dienst) hat dafür einen eigenen "update-preview"-Handler, der es
	 * weiter anspricht – ein freigegebenes Widget würde dort abstürzen. */
	{
		GtkWidget *old = gtk_file_chooser_get_preview_widget (chooser);

		if (old != NULL) {
			g_object_set_data_full (G_OBJECT (chooser), "nolphin-replaced-preview",
						g_object_ref (old), g_object_unref);
		}
	}
	nolphin_file_chooser_add_preview (chooser);
	return TRUE;
}

G_MODULE_EXPORT void
gtk_module_init (gint *argc, gchar ***argv)
{
	guint map_signal;

	/* Das Modul wird in gtk_init() geladen, bevor GtkWidget je benutzt wurde.
	 * Erst mit referenzierter Klasse gibt es das Signal "map". */
	g_type_class_ref (GTK_TYPE_WIDGET);
	map_signal = g_signal_lookup ("map", GTK_TYPE_WIDGET);
	if (map_signal == 0) {
		return;
	}

	/* "map" aller Widgets beobachten; der Haken prüft nur auf Dateiauswahldialoge. */
	g_signal_add_emission_hook (map_signal, 0, on_widget_map, NULL, NULL);
}
