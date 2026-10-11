/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nolphin-column-utilities.h - Utilities related to column specifications

   Copyright (C) 2004 Novell, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the column COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
   Boston, MA 02110-1335, USA.

   Authors: Dave Camp <dave@ximian.com>
*/

#include <config.h>
#include "nolphin-column-utilities.h"

#include <string.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-vfs-extensions.h>
#include <glib/gi18n.h>
#include <libnolphin-extension/nolphin-column-provider.h>
#include <libnolphin-private/nolphin-module.h>

static GList *
get_builtin_columns (void)
{
	GList *columns;

	columns = g_list_append (NULL,
				 g_object_new (NOLPHIN_TYPE_COLUMN,
					       "name", "name",
					       "attribute", "name",
					       "label", _("Name"),
					       "description", _("Name und Symbol der Datei."),
					       NULL));
	columns = g_list_append (columns,
				 g_object_new (NOLPHIN_TYPE_COLUMN,
					       "name", "size",
					       "attribute", "size",
					       "label", _("Größe"),
					       "description", _("Die Dateigröße."),
					       "xalign", 1.0,
					       NULL));
	columns = g_list_append (columns,
				 g_object_new (NOLPHIN_TYPE_COLUMN,
					       "name", "type",
					       "attribute", "type",
					       "label", _("Dateityp"),
					       "description", _("Allgemeiner Dateityp"),
					       NULL));
    columns = g_list_append (columns,
                 g_object_new (NOLPHIN_TYPE_COLUMN,
                           "name", "detailed_type",
                           "attribute", "detailed_type",
                           "label", _("De­tail­lierter Dateityp"),
                           "description", _("Der konkrete Dateityp"),
                           NULL));
	columns = g_list_append (columns,
				 g_object_new (NOLPHIN_TYPE_COLUMN,
					       "name", "date_modified",
					       "attribute", "date_modified",
					       "label", _("Änderungsdatum"),
					       "description", _("Datum der letzten Dateiänderung."),
					       NULL));
    columns = g_list_append (columns,
                 g_object_new (NOLPHIN_TYPE_COLUMN,
                           "name", "date_modified_with_time",
                           "attribute", "date_modified_with_time",
                           "label", _("Geändert – Zeit"),
                           "description", _("Datum der letzten Dateiänderung."),
                           "xalign", 1.0,
                           NULL));

    columns = g_list_append (columns,
                 g_object_new (NOLPHIN_TYPE_COLUMN,
                           "name", "date_created",
                           "attribute", "date_created",
                           "label", _("Erstelldatum"),
                           "description", _("Das Datum, an dem die Datei erstellt wurde."),
                           NULL));
    columns = g_list_append (columns,
                 g_object_new (NOLPHIN_TYPE_COLUMN,
                           "name", "date_created_with_time",
                           "attribute", "date_created_with_time",
                           "label", _("Erstellt – Zeit"),
                           "description", _("Das Datum, an dem die Datei erstellt wurde."),
                           NULL));

	columns = g_list_append (columns,
				 g_object_new (NOLPHIN_TYPE_COLUMN,
					       "name", "date_accessed",
					       "attribute", "date_accessed",
					       "label", _("Zugriffsdatum"),
					       "description", _("Das Datum des letzten Dateizugriffs."),
					       NULL));

	columns = g_list_append (columns,
				 g_object_new (NOLPHIN_TYPE_COLUMN,
					       "name", "owner",
					       "attribute", "owner",
					       "label", _("Eigentümer"),
					       "description", _("Besitzer der Datei"),
					       NULL));

	columns = g_list_append (columns,
				 g_object_new (NOLPHIN_TYPE_COLUMN,
					       "name", "group",
					       "attribute", "group",
					       "label", _("Gruppe"),
					       "description", _("Die Dateigruppe."),
					       NULL));

	columns = g_list_append (columns,
				 g_object_new (NOLPHIN_TYPE_COLUMN,
					       "name", "permissions",
					       "attribute", "permissions",
					       "label", _("Zugriffsrechte"),
					       "description", _("Die Dateizugriffsrechte"),
					       NULL));

	columns = g_list_append (columns,
				 g_object_new (NOLPHIN_TYPE_COLUMN,
					       "name", "octal_permissions",
					       "attribute", "octal_permissions",
					       "label", _("Oktale Zugriffsrechte"),
					       "description", _("Die Zugriffsrechte auf die Datei in Oktalnotation."),
					       NULL));

	columns = g_list_append (columns,
				 g_object_new (NOLPHIN_TYPE_COLUMN,
					       "name", "mime_type",
					       "attribute", "mime_type",
					       "label", _("MIME-Typ"),
					       "description", _("Der MIME-Typ der Datei."),
					       NULL));
	columns = g_list_append (columns,
				 g_object_new (NOLPHIN_TYPE_COLUMN,
					       "name", "extension",
					       "attribute", "extension",
					       "label", _("Erweiterung"),
					       "description", _("Die Erweiterung der Datei."),
					       NULL));
#ifdef HAVE_SELINUX
	columns = g_list_append (columns,
				 g_object_new (NOLPHIN_TYPE_COLUMN,
					       "name", "selinux_context",
					       "attribute", "selinux_context",
					       "label", _("SELinux-Kontext"),
					       "description", _("Der SELinux-Sicherheitskontext dieser Datei."),
					       NULL));
#endif
	return columns;
}

static GList *
get_extension_columns (void)
{
	GList *columns;
	GList *providers;
	GList *l;
	
	providers = nolphin_module_get_extensions_for_type (NOLPHIN_TYPE_COLUMN_PROVIDER);
	
	columns = NULL;
	
	for (l = providers; l != NULL; l = l->next) {
		NolphinColumnProvider *provider;
		GList *provider_columns;
		
		provider = NOLPHIN_COLUMN_PROVIDER (l->data);
		provider_columns = nolphin_column_provider_get_columns (provider);
		columns = g_list_concat (columns, provider_columns);
	}

	nolphin_module_extension_list_free (providers);

	return columns;
}

static GList *
get_trash_columns (void)
{
	static GList *columns = NULL;

	if (columns == NULL) {
		columns = g_list_append (columns,
					 g_object_new (NOLPHIN_TYPE_COLUMN,
						       "name", "trashed_on",
						       "attribute", "trashed_on",
						       "label", _("Löschdatum"),
						       "description", _("Datum, an dem die Datei gelöscht wurde"),
						       NULL));
		columns = g_list_append (columns,
			                 g_object_new (NOLPHIN_TYPE_COLUMN,
			                               "name", "trash_orig_path",
			                               "attribute", "trash_orig_path",
			                               "label", _("Ursprungsort"),
			                               "description", _("Speicherort der Datei vor dem Löschen"),
			                               NULL));
	}

	return nolphin_column_list_copy (columns);
}

static GList *
get_search_columns (void)
{
    static GList *columns = NULL;

    if (columns == NULL) {
        columns = g_list_append (columns,
                             g_object_new (NOLPHIN_TYPE_COLUMN,
                                           "name", "search_result_count",
                                           "attribute", "search_result_count",
                                           // TRANSLATORS: This column is only useful for content search results,
                                           // and shows the number of occurrences of the search string within the file.
                                           "label", _("Aufrufe"),
                                           "description", _("Wie oft der Suchbegriff in der Datei vorkam"),
                                           NULL));
    }

    return nolphin_column_list_copy (columns);
}

static GList *
get_mixed_file_list_columns (void)
{
    static GList *columns = NULL;

    if (columns == NULL) {
        columns = g_list_append (columns,
                     g_object_new (NOLPHIN_TYPE_COLUMN,
                               "name", "where",
                               "attribute", "where",
                               // TRANSLATORS: The Location column displays the parent path of a given file. This
                               // is useful in special file listings like search results, Recents or Favorites.
                               "label", _("Speicherort"),
                               "description", _("Speicherort der Datei"),
                               "width-chars", 60,
                               "ellipsize", PANGO_ELLIPSIZE_END,
                               NULL));
    }

    return nolphin_column_list_copy (columns);
}

GList *
nolphin_get_common_columns (void)
{
	static GList *columns = NULL;

	if (!columns) {
		columns = g_list_concat (get_builtin_columns (),
		                         get_extension_columns ());
	}

	return nolphin_column_list_copy (columns);
}

GList *
nolphin_get_all_columns (void)
{
    GList *columns = NULL;

	columns = g_list_concat (nolphin_get_common_columns (),
	                         get_trash_columns ());
    columns = g_list_concat (columns, get_search_columns ());
    columns = g_list_concat (columns, get_mixed_file_list_columns ());

    return columns;
}

GList *
nolphin_get_columns_for_file (NolphinFile *file)
{
    GList *columns;
    gchar *uri;

    columns = nolphin_get_common_columns ();

    if (file == NULL) {
        return columns;
    }

    uri = nolphin_file_get_uri (file);

    if (eel_uri_is_trash (uri)) {
        columns = g_list_concat (columns,
                                 get_trash_columns ());
    } else if (eel_uri_is_search (uri)) {
        columns = g_list_concat (columns,
                                 get_search_columns ());
        columns = g_list_concat (columns,
                                 get_mixed_file_list_columns ());
    } else if (eel_uri_is_favorite (uri)) {
        columns = g_list_concat (columns,
                                 get_mixed_file_list_columns ());
    } else if (eel_uri_is_recent (uri)) {
        columns = g_list_concat (columns,
                                 get_mixed_file_list_columns ());
    }

    g_free (uri);

    return columns;
}

GList *
nolphin_column_list_copy (GList *columns) 
{
	GList *ret;
	GList *l;
	
	ret = g_list_copy (columns);
	
	for (l = ret; l != NULL; l = l->next) {
		g_object_ref (l->data);
	}

	return ret;
}

void
nolphin_column_list_free (GList *columns)
{
	GList *l;
	
	for (l = columns; l != NULL; l = l->next) {
		g_object_unref (l->data);
	}
	
	g_list_free (columns);
}

static int
strv_index (char **strv, const char *str)
{
	int i;

	for (i = 0; strv[i] != NULL; ++i) {
		if (strcmp (strv[i], str) == 0)
			return i;
	}

	return -1;
}

static int
column_compare (NolphinColumn *a, NolphinColumn *b, char **column_order)
{
	int index_a;
	int index_b;
	char *name;
	
	g_object_get (G_OBJECT (a), "name", &name, NULL);
	index_a = strv_index (column_order, name);
	g_free (name);

	g_object_get (G_OBJECT (b), "name", &name, NULL);
	index_b = strv_index (column_order, name);
	g_free (name);

	if (index_a == index_b) {
		int ret;
		char *label_a;
		char *label_b;
		
		g_object_get (G_OBJECT (a), "label", &label_a, NULL);
		g_object_get (G_OBJECT (b), "label", &label_b, NULL);
		ret = strcmp (label_a, label_b);
		g_free (label_a);
		g_free (label_b);
		
		return ret;
	} else if (index_a == -1) {
		return 1;
	} else if (index_b == -1) {
		return -1;
	} else {
		return index_a - index_b;
	}
}

GList *
nolphin_sort_columns (GList  *columns, 
		       char  **column_order)
{
	if (!column_order) {
		return NULL;
	}

	return g_list_sort_with_data (columns,
				      (GCompareDataFunc)column_compare,
				      column_order);
}
		       
