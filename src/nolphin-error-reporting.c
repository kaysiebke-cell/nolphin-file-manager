/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nolphin-error-reporting.h - implementation of file manager functions that report
 	                        errors to the user.

   Copyright (C) 2000 Eazel, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
   Boston, MA 02110-1335, USA.

   Authors: John Sullivan <sullivan@eazel.com>
*/

#include <config.h>

#include "nolphin-error-reporting.h"

#include <string.h>
#include <glib/gi18n.h>
#include <libnolphin-private/nolphin-file.h>
#include <eel/eel-string.h>
#include <eel/eel-stock-dialogs.h>

#define DEBUG_FLAG NOLPHIN_DEBUG_DIRECTORY_VIEW
#include <libnolphin-private/nolphin-debug.h>

#define NEW_NAME_TAG "Nolphin: new name"
#define MAXIMUM_DISPLAYED_FILE_NAME_LENGTH	50

static void finish_rename (NolphinFile *file, gboolean stop_timer, GError *error);

void
nolphin_report_error_loading_directory (NolphinFile *file,
					 GError *error,
					 GtkWindow *parent_window)
{
	char *file_name;
	char *message;

	if (error == NULL ||
	    error->message == NULL) {
		return;
	}

	if (error->domain == G_IO_ERROR &&
	    error->code == G_IO_ERROR_NOT_MOUNTED) {
		/* This case is retried automatically */
		return;
	}
	
	file_name = nolphin_file_get_display_name (file);
	
	if (error->domain == G_IO_ERROR) {
		switch (error->code) {
		case G_IO_ERROR_PERMISSION_DENIED:
			message = g_strdup_printf (_("Sie haben nicht die nötigen Zugriffsrechte, um den Inhalt von »%s« anzuzeigen."),
						   file_name);
			break;
		case G_IO_ERROR_NOT_FOUND:
			message = g_strdup_printf (_("»%s« konnte nicht gefunden werden. Vielleicht wurde es kürzlich gelöscht."),
						   file_name);
			break;
		default:
			message = g_strdup_printf (_("Leider konnte der gesamte Inhalt von »%s« nicht angezeigt werden: %s"), file_name,
						   error->message);
		}
	} else {
		message = g_strdup (error->message);
	}

	eel_show_error_dialog (_("Der Ordnerinhalt konnte nicht angezeigt werden."), message, parent_window);

	g_free (file_name);
	g_free (message);
}

void
nolphin_report_error_setting_group (NolphinFile *file,
				     GError *error,
				     GtkWindow *parent_window)
{
	char *file_name;
	char *message;

	if (error == NULL) {
		return;
	}

	file_name = nolphin_file_get_display_name (file);

	message = NULL;
	if (error->domain == G_IO_ERROR) {
		switch (error->code) {
		case G_IO_ERROR_PERMISSION_DENIED:
			message = g_strdup_printf (_("Sie haben nicht die nötigen Zugriffsrechte, um die Gruppe von »%s« zu ändern."),
						   file_name);
			break;
		default:
			break;
		}
	}
			
	if (message == NULL) {
		/* We should invent decent error messages for every case we actually experience. */
		g_warning ("Hit unhandled case %s:%d in nolphin_report_error_setting_group", 
			   g_quark_to_string (error->domain), error->code);
		/* fall through */
		message = g_strdup_printf (_("Leider konnte die Gruppe von »%s« nicht geändert werden: %s"), file_name,
					   error->message);
	}
	
	
	eel_show_error_dialog (_("Die Gruppe konnte nicht verändert werden."), message, parent_window);
	
	g_free (file_name);
	g_free (message);
}

void
nolphin_report_error_setting_owner (NolphinFile *file,
				     GError *error,
				     GtkWindow *parent_window)
{
	char *file_name;
	char *message;

	if (error == NULL) {
		return;
	}

	file_name = nolphin_file_get_display_name (file);

	message = g_strdup_printf (_("Leider konnte der Besitzer von »%s« nicht geändert werden: %s"), file_name, error->message);

	eel_show_error_dialog (_("Der Besitzer konnte nicht geändert werden."), message, parent_window);

	g_free (file_name);
	g_free (message);
}		

void
nolphin_report_error_setting_permissions (NolphinFile *file,
					   GError *error,
					   GtkWindow *parent_window)
{
	char *file_name;
	char *message;

	if (error == NULL) {
		return;
	}

	file_name = nolphin_file_get_display_name (file);

	message = g_strdup_printf (_("Entschuldigung, die Zugriffsrechte von »%s« konnten nicht geändert werden: %s"), file_name, error->message);

	eel_show_error_dialog (_("Die Zugriffsrechte konnten nicht geändert werden."), message, parent_window);

	g_free (file_name);
	g_free (message);
}		

typedef struct _NolphinRenameData {
	char *name;
	NolphinFileOperationCallback callback;
	gpointer callback_data;
} NolphinRenameData;

void
nolphin_report_error_renaming_file (NolphinFile *file,
				     const char *new_name,
				     GError *error,
				     GtkWindow *parent_window)
{
	char *original_name, *original_name_truncated;
	char *new_name_truncated;
	char *message;

	/* Truncate names for display since very long file names with no spaces
	 * in them won't get wrapped, and can create insanely wide dialog boxes.
	 */
	original_name = nolphin_file_get_display_name (file);
	original_name_truncated = eel_str_middle_truncate (original_name, MAXIMUM_DISPLAYED_FILE_NAME_LENGTH);
	g_free (original_name);
	
	new_name_truncated = eel_str_middle_truncate (new_name, MAXIMUM_DISPLAYED_FILE_NAME_LENGTH);

	message = NULL;
	if (error->domain == G_IO_ERROR) {
		switch (error->code) {
		case G_IO_ERROR_EXISTS:
			message = g_strdup_printf (_("Der Name »%s« wird in diesem Ordner bereits verwendet. Bitte einen anderen Namen wählen."), 
						   new_name_truncated);
			break;
		case G_IO_ERROR_NOT_FOUND:
			message = g_strdup_printf (_("Es gibt kein »%s« in diesem Ordner. Vielleicht wurde es gerade verschoben oder gelöscht?"), 
						   original_name_truncated);
			break;
		case G_IO_ERROR_PERMISSION_DENIED:
			message = g_strdup_printf (_("Ihnen fehlen die nötigen Zugriffsrechte, um »%s« umzubenennen."),
						   original_name_truncated);
			break;
		case G_IO_ERROR_INVALID_FILENAME:
			if (strchr (new_name, '/') != NULL) {
				message = g_strdup_printf (_("Der Name »%s« ist nicht erlaubt, da er das Zeichen »/« enthält. Bitte einen anderen Namen verwenden."),
							   new_name_truncated);
			} else {
				message = g_strdup_printf (_("Der Name »%s« ist nicht gültig. Bitte einen anderen Namen verwenden."),
							   new_name_truncated);
			}
			break;
                case G_IO_ERROR_FILENAME_TOO_LONG:
                        message = g_strdup_printf (_("Der Name »%s« ist zu lang. Bitte einen kürzeren Namen verwenden."),
                                                     new_name_truncated);
                        break;
		default:
			break;
		}
	}
	
	if (message == NULL) {
		/* We should invent decent error messages for every case we actually experience. */
		g_warning ("Hit unhandled case %s:%d in nolphin_report_error_renaming_file", 
			   g_quark_to_string (error->domain), error->code);
		/* fall through */
		message = g_strdup_printf (_("Leider konnte »%s« nicht in »%s« umbenannt werden: %s"), 
					   original_name_truncated, new_name_truncated,
					   error->message);
	}
	
	g_free (original_name_truncated);
	g_free (new_name_truncated);

	eel_show_error_dialog (_("Objekt konnte nicht umbenannt werden."), message, parent_window);
	g_free (message);
}

static void
nolphin_rename_data_free (NolphinRenameData *data)
{
	g_free (data->name);
	g_free (data);
}

static void
rename_callback (NolphinFile *file, GFile *result_location,
		 GError *error, gpointer callback_data)
{
	NolphinRenameData *data;

	g_assert (NOLPHIN_IS_FILE (file));
	g_assert (callback_data == NULL);
	
	data = g_object_get_data (G_OBJECT (file), NEW_NAME_TAG);
	g_assert (data != NULL);

	if (error &&
	    !(error->domain == G_IO_ERROR && error->code == G_IO_ERROR_CANCELLED)) {
		/* If rename failed, notify the user. */
		nolphin_report_error_renaming_file (file, data->name, error, NULL);
	}

	finish_rename (file, TRUE, error);
}

static void
cancel_rename_callback (gpointer callback_data)
{
	GError *error;
	
	error = g_error_new (G_IO_ERROR, G_IO_ERROR_CANCELLED, "Cancelled");
	finish_rename (NOLPHIN_FILE (callback_data), FALSE, error);
	g_error_free (error);
}

static void
finish_rename (NolphinFile *file, gboolean stop_timer, GError *error)
{
	NolphinRenameData *data;

	data = g_object_get_data (G_OBJECT (file), NEW_NAME_TAG);
	if (data == NULL) {
		return;
	}

	/* Cancel both the rename and the timed wait. */
	nolphin_file_cancel (file, rename_callback, NULL);
	if (stop_timer) {
		eel_timed_wait_stop (cancel_rename_callback, file);
	}

	if (data->callback != NULL) {
		data->callback (file, NULL, error, data->callback_data);
	}
	
	/* Let go of file name. */
	g_object_set_data (G_OBJECT (file), NEW_NAME_TAG, NULL);
}

void
nolphin_rename_file (NolphinFile *file,
		      const char *new_name,
		      NolphinFileOperationCallback callback,
		      gpointer callback_data)
{
	char *old_name, *wait_message;
	NolphinRenameData *data;
	char *uri;
	GError *error;

	g_return_if_fail (NOLPHIN_IS_FILE (file));
	g_return_if_fail (new_name != NULL);

	/* Stop any earlier rename that's already in progress. */
	error = g_error_new (G_IO_ERROR, G_IO_ERROR_CANCELLED, "Cancelled");
	finish_rename (file, TRUE, error);
	g_error_free (error);

	data = g_new0 (NolphinRenameData, 1);
	data->name = g_strdup (new_name);
	data->callback = callback;
	data->callback_data = callback_data;
	
	/* Attach the new name to the file. */
	g_object_set_data_full (G_OBJECT (file),
				NEW_NAME_TAG,
				data, (GDestroyNotify)nolphin_rename_data_free);

	/* Start the timed wait to cancel the rename. */
	old_name = nolphin_file_get_display_name (file);
	wait_message = g_strdup_printf (_("»%s« wird in »%s« umbenannt."),
					old_name,
					new_name);
	g_free (old_name);
	eel_timed_wait_start (cancel_rename_callback, file, wait_message, 
			      NULL); /* FIXME bugzilla.gnome.org 42395: Parent this? */
	g_free (wait_message);

	uri = nolphin_file_get_uri (file);
	DEBUG ("Renaming file %s to %s", uri, new_name);
	g_free (uri);

	/* Start the rename. */
	nolphin_file_rename (file, new_name,
			      rename_callback, NULL);
}
