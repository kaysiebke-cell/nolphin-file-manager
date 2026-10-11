/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Copyright (C) 2005 Novell, Inc.
 *
 * Nolphin is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nolphin is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; see the file COPYING.  If not,
 * write to the Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
 * Boston, MA 02110-1335, USA.
 *
 * Author: Anders Carlsson <andersca@imendio.com>
 *
 */

#include <config.h>
#include <string.h>

#include "nolphin-query.h"
#include <eel/eel-glib-extensions.h>
#include <glib/gi18n.h>
#include <libnolphin-private/nolphin-file-utilities.h>

struct NolphinQueryDetails {
    gchar *file_pattern;
    gchar *content_pattern;
    char *location_uri;
    GList *mime_types;
    gboolean show_hidden;
    gboolean file_case_sensitive;
    gboolean file_use_regex;
    gboolean content_case_sensitive;
    gboolean content_use_regex;
    gboolean count_hits;
    gboolean recurse;
};

G_DEFINE_TYPE (NolphinQuery, nolphin_query, G_TYPE_OBJECT);

static void
finalize (GObject *object)
{
	NolphinQuery *query;

	query = NOLPHIN_QUERY (object);
    g_free (query->details->file_pattern);
	g_free (query->details->content_pattern);
	g_free (query->details->location_uri);

	G_OBJECT_CLASS (nolphin_query_parent_class)->finalize (object);
}

static void
nolphin_query_class_init (NolphinQueryClass *class)
{
	GObjectClass *gobject_class;

	gobject_class = G_OBJECT_CLASS (class);
	gobject_class->finalize = finalize;

	g_type_class_add_private (class, sizeof (NolphinQueryDetails));
}

static void
nolphin_query_init (NolphinQuery *query)
{
	query->details = G_TYPE_INSTANCE_GET_PRIVATE (query, NOLPHIN_TYPE_QUERY,
						      NolphinQueryDetails);
}

NolphinQuery *
nolphin_query_new (void)
{
	return g_object_new (NOLPHIN_TYPE_QUERY,  NULL);
}

char *
nolphin_query_get_file_pattern (NolphinQuery *query)
{
    g_return_val_if_fail (NOLPHIN_IS_QUERY (query), NULL);

	return g_strdup (query->details->file_pattern);
}

void
nolphin_query_set_file_pattern (NolphinQuery *query, const char *text)
{
    g_return_if_fail (NOLPHIN_IS_QUERY (query));

	g_free (query->details->file_pattern);
	query->details->file_pattern = g_strstrip (g_strdup (text));
}

char *
nolphin_query_get_content_pattern (NolphinQuery *query)
{
    g_return_val_if_fail (NOLPHIN_IS_QUERY (query), NULL);

    return g_strdup (query->details->content_pattern);
}

void
nolphin_query_set_content_pattern (NolphinQuery *query, const char *text)
{
    g_return_if_fail (NOLPHIN_IS_QUERY (query));

    g_clear_pointer (&query->details->content_pattern, g_free);

    if (text && text[0] != '\0') {
        query->details->content_pattern = g_strstrip (g_strdup (text));
    }
}

gboolean
nolphin_query_has_content_pattern (NolphinQuery *query)
{
    g_return_val_if_fail (NOLPHIN_IS_QUERY (query), FALSE);

    return query->details->content_pattern != NULL;
}

char *
nolphin_query_get_location (NolphinQuery *query)
{
    g_return_val_if_fail (NOLPHIN_IS_QUERY (query), NULL);

	return g_strdup (query->details->location_uri);
}

void
nolphin_query_set_location (NolphinQuery *query, const char *uri)
{
    g_return_if_fail (NOLPHIN_IS_QUERY (query));

	g_free (query->details->location_uri);
	query->details->location_uri = g_strdup (uri);
}

GList *
nolphin_query_get_mime_types (NolphinQuery *query)
{
    g_return_val_if_fail (NOLPHIN_IS_QUERY (query), NULL);

	return eel_g_str_list_copy (query->details->mime_types);
}

void
nolphin_query_set_mime_types (NolphinQuery *query, GList *mime_types)
{
    g_return_if_fail (NOLPHIN_IS_QUERY (query));

	g_list_free_full (query->details->mime_types, g_free);
	query->details->mime_types = eel_g_str_list_copy (mime_types);
}

void
nolphin_query_add_mime_type (NolphinQuery *query, const char *mime_type)
{
    g_return_if_fail (NOLPHIN_IS_QUERY (query));

	query->details->mime_types = g_list_append (query->details->mime_types,
						    g_strdup (mime_type));
}

void
nolphin_query_set_show_hidden (NolphinQuery *query, gboolean hidden)
{
    g_return_if_fail (NOLPHIN_IS_QUERY (query));

    query->details->show_hidden = hidden;
}

gboolean
nolphin_query_get_show_hidden (NolphinQuery *query)
{
    g_return_val_if_fail (NOLPHIN_IS_QUERY (query), FALSE);

    return query->details->show_hidden;
}

char *
nolphin_query_to_readable_string (NolphinQuery *query)
{
    g_return_val_if_fail (NOLPHIN_IS_QUERY (query), NULL);

    GFile *file;
    gchar *location_title, *readable;

	if (!query || !query->details->file_pattern || query->details->file_pattern[0] == '\0') {
		return g_strdup (_("Suche"));
	}

    file = g_file_new_for_uri (query->details->location_uri);
    location_title = nolphin_compute_search_title_for_location (file);

    g_object_unref (file);

    readable = g_strdup_printf (_("in »%s« suchen"), location_title);

    g_free (location_title);

    return readable;
}

gboolean
nolphin_query_get_file_case_sensitive (NolphinQuery *query)
{
    g_return_val_if_fail (NOLPHIN_IS_QUERY (query), FALSE);
    return query->details->file_case_sensitive;
}

void
nolphin_query_set_file_case_sensitive (NolphinQuery *query, gboolean case_sensitive)
{
    g_return_if_fail (NOLPHIN_IS_QUERY (query));

    query->details->file_case_sensitive = case_sensitive;
}

gboolean
nolphin_query_get_content_case_sensitive (NolphinQuery *query)
{
    g_return_val_if_fail (NOLPHIN_IS_QUERY (query), FALSE);
    return query->details->content_case_sensitive;
}

void
nolphin_query_set_content_case_sensitive (NolphinQuery *query, gboolean case_sensitive)
{
    g_return_if_fail (NOLPHIN_IS_QUERY (query));

    query->details->content_case_sensitive = case_sensitive;
}

gboolean
nolphin_query_get_use_file_regex (NolphinQuery *query)
{
    g_return_val_if_fail (NOLPHIN_IS_QUERY (query), FALSE);
    return query->details->file_use_regex;
}

void
nolphin_query_set_use_file_regex (NolphinQuery *query, gboolean file_use_regex)
{
    g_return_if_fail (NOLPHIN_IS_QUERY (query));

    query->details->file_use_regex = file_use_regex;
}

gboolean
nolphin_query_get_use_content_regex (NolphinQuery *query)
{
    g_return_val_if_fail (NOLPHIN_IS_QUERY (query), FALSE);
    return query->details->content_use_regex;
}

void
nolphin_query_set_use_content_regex (NolphinQuery *query, gboolean content_use_regex)
{
    g_return_if_fail (NOLPHIN_IS_QUERY (query));

    query->details->content_use_regex = content_use_regex;
}

gboolean
nolphin_query_get_count_hits (NolphinQuery *query)
{
    g_return_val_if_fail (NOLPHIN_IS_QUERY (query), FALSE);
    return query->details->count_hits;
}

void
nolphin_query_set_count_hits (NolphinQuery *query, gboolean count_hits)
{
    g_return_if_fail (NOLPHIN_IS_QUERY (query));

    query->details->count_hits = count_hits;
}

gboolean
nolphin_query_get_recurse (NolphinQuery *query)
{
    g_return_val_if_fail (NOLPHIN_IS_QUERY (query), FALSE);
    return query->details->recurse;
}

void
nolphin_query_set_recurse (NolphinQuery *query, gboolean recurse)
{
    g_return_if_fail (NOLPHIN_IS_QUERY (query));

    query->details->recurse = recurse;
}



/* Gespeicherte Suchen: einfache GKeyFile-Datei (Gruppe "Nolphin Saved Search"). */
#define SAVED_SEARCH_GROUP "Nolphin Saved Search"

gboolean
nolphin_query_save (NolphinQuery *query, char *file)
{
	GKeyFile *kf;
	GList *l;
	GPtrArray *mimes;
	gboolean ok;
	GError *error = NULL;
	char *tmp;

	g_return_val_if_fail (NOLPHIN_IS_QUERY (query) && file != NULL, FALSE);

	kf = g_key_file_new ();
	g_key_file_set_string (kf, SAVED_SEARCH_GROUP, "FilePattern", query->details->file_pattern ? query->details->file_pattern : "");
	g_key_file_set_string (kf, SAVED_SEARCH_GROUP, "ContentPattern", query->details->content_pattern ? query->details->content_pattern : "");
	g_key_file_set_string (kf, SAVED_SEARCH_GROUP, "Location", query->details->location_uri ? query->details->location_uri : "");
	g_key_file_set_boolean (kf, SAVED_SEARCH_GROUP, "ShowHidden", query->details->show_hidden);
	g_key_file_set_boolean (kf, SAVED_SEARCH_GROUP, "FileCaseSensitive", query->details->file_case_sensitive);
	g_key_file_set_boolean (kf, SAVED_SEARCH_GROUP, "FileUseRegex", query->details->file_use_regex);
	g_key_file_set_boolean (kf, SAVED_SEARCH_GROUP, "ContentCaseSensitive", query->details->content_case_sensitive);
	g_key_file_set_boolean (kf, SAVED_SEARCH_GROUP, "ContentUseRegex", query->details->content_use_regex);
	g_key_file_set_boolean (kf, SAVED_SEARCH_GROUP, "Recurse", query->details->recurse);

	mimes = g_ptr_array_new ();
	for (l = query->details->mime_types; l != NULL; l = l->next) {
		g_ptr_array_add (mimes, l->data);
	}
	g_key_file_set_string_list (kf, SAVED_SEARCH_GROUP, "MimeTypes", (const gchar * const *) mimes->pdata, mimes->len);
	g_ptr_array_free (mimes, TRUE);

	tmp = g_key_file_to_data (kf, NULL, NULL);
	ok = g_file_set_contents (file, tmp, -1, &error);
	if (!ok) {
		g_warning ("Konnte gespeicherte Suche nicht schreiben: %s", error->message);
		g_error_free (error);
	}
	g_free (tmp);
	g_key_file_free (kf);

	return ok;
}

NolphinQuery *
nolphin_query_load (char *file)
{
	GKeyFile *kf;
	NolphinQuery *query;
	char *str;
	char **mimes;
	gsize n, i;

	g_return_val_if_fail (file != NULL, NULL);

	kf = g_key_file_new ();
	if (!g_key_file_load_from_file (kf, file, G_KEY_FILE_NONE, NULL) ||
	    !g_key_file_has_group (kf, SAVED_SEARCH_GROUP)) {
		g_key_file_free (kf);
		return NULL;
	}

	query = nolphin_query_new ();

	str = g_key_file_get_string (kf, SAVED_SEARCH_GROUP, "FilePattern", NULL);
	nolphin_query_set_file_pattern (query, str);
	g_free (str);
	str = g_key_file_get_string (kf, SAVED_SEARCH_GROUP, "ContentPattern", NULL);
	if (str != NULL && str[0] != '\0') {
		nolphin_query_set_content_pattern (query, str);
	}
	g_free (str);
	str = g_key_file_get_string (kf, SAVED_SEARCH_GROUP, "Location", NULL);
	if (str != NULL && str[0] != '\0') {
		nolphin_query_set_location (query, str);
	}
	g_free (str);

	nolphin_query_set_show_hidden (query, g_key_file_get_boolean (kf, SAVED_SEARCH_GROUP, "ShowHidden", NULL));
	nolphin_query_set_file_case_sensitive (query, g_key_file_get_boolean (kf, SAVED_SEARCH_GROUP, "FileCaseSensitive", NULL));
	nolphin_query_set_use_file_regex (query, g_key_file_get_boolean (kf, SAVED_SEARCH_GROUP, "FileUseRegex", NULL));
	nolphin_query_set_content_case_sensitive (query, g_key_file_get_boolean (kf, SAVED_SEARCH_GROUP, "ContentCaseSensitive", NULL));
	nolphin_query_set_use_content_regex (query, g_key_file_get_boolean (kf, SAVED_SEARCH_GROUP, "ContentUseRegex", NULL));
	nolphin_query_set_recurse (query, g_key_file_get_boolean (kf, SAVED_SEARCH_GROUP, "Recurse", NULL));

	mimes = g_key_file_get_string_list (kf, SAVED_SEARCH_GROUP, "MimeTypes", &n, NULL);
	for (i = 0; mimes != NULL && i < n; i++) {
		nolphin_query_add_mime_type (query, mimes[i]);
	}
	g_strfreev (mimes);
	g_key_file_free (kf);

	return query;
}
