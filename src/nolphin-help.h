/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nolphin-help.h: Hilfe zu den Funktionen von Nolphin
 *
 * Die Hilfe ersetzt – wie die Einstellungen – den Dateibereich des
 * Hauptfensters (Seite "help" im Inhalts-Stack). Links stehen die Themen,
 * rechts der Text. Mit Esc oder "Schließen" geht es zurück zu den Dateien.
 */

#ifndef NOLPHIN_HELP_H
#define NOLPHIN_HELP_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

/* Zeigt die Hilfe im Hauptfenster; @topic_id (z. B. "git") wählt ein
 * Thema, NULL das erste. */
void nolphin_help_show (GtkWindow *window, const gchar *topic_id);

G_END_DECLS

#endif /* NOLPHIN_HELP_H */
