/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nolphin-cad.c: native 3D/CAD file recognition and metadata
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
 */

#include <config.h>

#include "nolphin-cad.h"

#include <glib/gi18n.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>

#include <gsf/gsf-input-stdio.h>
#include <gsf/gsf-infile-zip.h>
#include <gsf/gsf-input.h>
#include <gsf/gsf-infile.h>

/* ASCII STL files above this size are recognized and reported, but not
 * fully scanned for a triangle count - that would mean reading the
 * whole thing (STL has no header count for the ASCII variant), and a
 * background thread still has a real wall-clock cost users are
 * waiting on. 20 MB is comfortably larger than any STL a file manager
 * preview realistically needs to fully characterize. */
#define ASCII_STL_SCAN_LIMIT (20 * 1024 * 1024)

/* How much of a STEP file's HEADER; section we read - real headers are
 * a few hundred bytes; this is generous headroom. */
#define STEP_HEADER_READ_LIMIT 8192

GQuark
nolphin_cad_error_quark (void)
{
    return g_quark_from_static_string ("nolphin-cad-error-quark");
}

typedef struct {
    const gchar *ext;
    NolphinCadFormat format;
    const gchar *label;
    gboolean has_backend;
} CadFormatEntry;

static const CadFormatEntry cad_formats[] = {
    { ".stl",   NOLPHIN_CAD_FORMAT_STL,   "STL",     TRUE  },
    { ".step",  NOLPHIN_CAD_FORMAT_STEP,  "STEP",    TRUE  },
    { ".stp",   NOLPHIN_CAD_FORMAT_STEP,  "STEP",    TRUE  },
    { ".fcstd", NOLPHIN_CAD_FORMAT_FCSTD, "FreeCAD", TRUE  },
    { ".iges",  NOLPHIN_CAD_FORMAT_IGES,  "IGES",    FALSE },
    { ".igs",   NOLPHIN_CAD_FORMAT_IGES,  "IGES",    FALSE },
    { ".obj",   NOLPHIN_CAD_FORMAT_OBJ,   "OBJ",     FALSE },
    { ".3mf",   NOLPHIN_CAD_FORMAT_3MF,   "3MF",     FALSE },
    { ".dxf",   NOLPHIN_CAD_FORMAT_DXF,   "DXF",     FALSE },
    { ".dwg",   NOLPHIN_CAD_FORMAT_DWG,   "DWG",     FALSE },
};

NolphinCadFormat
nolphin_cad_detect_format (GFile *file)
{
    gchar *name;
    gchar *lower;
    guint i;
    NolphinCadFormat result = NOLPHIN_CAD_FORMAT_UNKNOWN;

    name = g_file_get_basename (file);
    if (name == NULL) {
        return NOLPHIN_CAD_FORMAT_UNKNOWN;
    }
    lower = g_ascii_strdown (name, -1);
    g_free (name);

    for (i = 0; i < G_N_ELEMENTS (cad_formats); i++) {
        if (g_str_has_suffix (lower, cad_formats[i].ext)) {
            result = cad_formats[i].format;
            break;
        }
    }

    g_free (lower);
    return result;
}

const gchar *
nolphin_cad_format_get_label (NolphinCadFormat format)
{
    guint i;

    for (i = 0; i < G_N_ELEMENTS (cad_formats); i++) {
        if (cad_formats[i].format == format) {
            return cad_formats[i].label;
        }
    }
    return _("Unbekannt");
}

gboolean
nolphin_cad_format_has_backend (NolphinCadFormat format)
{
    guint i;

    for (i = 0; i < G_N_ELEMENTS (cad_formats); i++) {
        if (cad_formats[i].format == format) {
            return cad_formats[i].has_backend;
        }
    }
    return FALSE;
}

void
nolphin_cad_info_free (NolphinCadInfo *info)
{
    if (info == NULL) {
        return;
    }

    g_free (info->step_description);
    g_free (info->step_file_name);
    g_free (info->step_timestamp);
    g_free (info->step_author);
    g_free (info->step_schema);

    g_free (info->fcstd_comment);
    g_free (info->fcstd_author);
    g_free (info->fcstd_company);
    g_free (info->fcstd_created_date);
    g_free (info->fcstd_last_modified_date);
    g_clear_pointer (&info->fcstd_thumbnail_png, g_bytes_unref);

    g_free (info);
}

/* ---------------------------------------------------------------- STL */

static void
parse_stl (const gchar *path, guint64 filesize, NolphinCadInfo *info, GError **error)
{
    FILE *fp;
    guint8 header[84];
    size_t got;

    info->format = NOLPHIN_CAD_FORMAT_STL;

    fp = fopen (path, "rb");
    if (fp == NULL) {
        g_set_error (error, NOLPHIN_CAD_ERROR, NOLPHIN_CAD_ERROR_READ_FAILED,
                    "%s", g_strerror (errno));
        return;
    }

    got = fread (header, 1, sizeof (header), fp);

    if (got == sizeof (header)) {
        guint32 count_le;
        memcpy (&count_le, header + 80, 4);
        guint32 count = GUINT32_FROM_LE (count_le);
        guint64 expected_size = 84 + (guint64) count * 50;

        if (expected_size == filesize) {
            /* Matches the binary layout exactly: 80-byte header, a
             * uint32 triangle count, then count * 50-byte records. */
            info->stl_is_binary = TRUE;
            info->stl_triangle_count = count;
            info->stl_triangle_count_known = TRUE;
            fclose (fp);
            return;
        }
    }

    /* Doesn't fit the binary layout - check for the ASCII "solid"
     * keyword (optionally preceded by whitespace). */
    {
        gchar prefix[64];
        gchar *trimmed;
        size_t n;

        rewind (fp);
        n = fread (prefix, 1, sizeof (prefix) - 1, fp);
        prefix[n] = '\0';
        trimmed = prefix;
        while (*trimmed == ' ' || *trimmed == '\t' || *trimmed == '\r' || *trimmed == '\n') {
            trimmed++;
        }

        info->stl_is_binary = FALSE;
        info->stl_triangle_count_known = FALSE;

        if (g_ascii_strncasecmp (trimmed, "solid", 5) != 0) {
            /* Neither a well-formed binary STL nor starting with
             * "solid" - still report it as ASCII-shaped rather than
             * failing outright, since malformed/truncated STL files
             * do occur; we just can't be more specific than that. */
        }

        if (filesize <= ASCII_STL_SCAN_LIMIT) {
            guint64 count = 0;
            gchar buf[8192];
            size_t r;

            rewind (fp);
            while ((r = fread (buf, 1, sizeof (buf), fp)) > 0) {
                size_t i;
                for (i = 0; i + 12 <= r; i++) {
                    if (memcmp (buf + i, "facet normal", 12) == 0) {
                        count++;
                    }
                }
                /* Note: a "facet normal" straddling a chunk boundary
                 * is missed here. Acceptable for a best-effort count
                 * on a bounded scan, not claimed to be exact. */
            }
            info->stl_triangle_count = count;
            info->stl_triangle_count_known = TRUE;
        }
    }

    fclose (fp);
}

/* --------------------------------------------------------------- STEP */

/* Extracts the single-quoted strings from @text (STEP escapes a
 * literal quote as ''), stopping at the first unquoted ';' or after
 * @max_len bytes. Caller frees the returned array (and its strings). */
static GPtrArray *
extract_quoted_strings (const gchar *text, gsize max_len)
{
    GPtrArray *result = g_ptr_array_new_with_free_func (g_free);
    const gchar *p = text;
    const gchar *end = text + max_len;

    while (p < end && *p != '\0') {
        if (*p == ';') {
            break;
        }
        if (*p == '\'') {
            GString *str = g_string_new (NULL);
            p++;
            while (p < end && *p != '\0') {
                if (*p == '\'') {
                    if (p + 1 < end && *(p + 1) == '\'') {
                        g_string_append_c (str, '\'');
                        p += 2;
                        continue;
                    }
                    p++;
                    break;
                }
                g_string_append_c (str, *p);
                p++;
            }
            g_ptr_array_add (result, g_string_free (str, FALSE));
        } else {
            p++;
        }
    }

    return result;
}

static gchar *
find_step_statement (const gchar *header_text, const gchar *keyword)
{
    gchar *pos = strstr (header_text, keyword);
    if (pos == NULL) {
        return NULL;
    }
    pos += strlen (keyword);
    pos = strchr (pos, '(');
    return pos != NULL ? pos + 1 : NULL;
}

static void
parse_step (const gchar *path, NolphinCadInfo *info, GError **error)
{
    FILE *fp;
    gchar *buf;
    size_t got;
    gchar *stmt;

    info->format = NOLPHIN_CAD_FORMAT_STEP;

    fp = fopen (path, "rb");
    if (fp == NULL) {
        g_set_error (error, NOLPHIN_CAD_ERROR, NOLPHIN_CAD_ERROR_READ_FAILED,
                    "%s", g_strerror (errno));
        return;
    }

    buf = g_malloc (STEP_HEADER_READ_LIMIT + 1);
    got = fread (buf, 1, STEP_HEADER_READ_LIMIT, fp);
    buf[got] = '\0';
    fclose (fp);

    if (g_ascii_strncasecmp (buf, "ISO-10303-21", 12) != 0) {
        g_free (buf);
        g_set_error (error, NOLPHIN_CAD_ERROR, NOLPHIN_CAD_ERROR_NOT_A_CAD_FILE,
                    _("Die Datei beginnt nicht mit der erwarteten ISO-10303-21-STEP-Signatur."));
        return;
    }

    stmt = find_step_statement (buf, "FILE_DESCRIPTION");
    if (stmt != NULL) {
        GPtrArray *fields = extract_quoted_strings (stmt, STEP_HEADER_READ_LIMIT);
        if (fields->len > 0) {
            info->step_description = g_strdup (g_ptr_array_index (fields, 0));
        }
        g_ptr_array_free (fields, TRUE);
    }

    stmt = find_step_statement (buf, "FILE_NAME");
    if (stmt != NULL) {
        GPtrArray *fields = extract_quoted_strings (stmt, STEP_HEADER_READ_LIMIT);
        if (fields->len > 0) {
            info->step_file_name = g_strdup (g_ptr_array_index (fields, 0));
        }
        if (fields->len > 1) {
            info->step_timestamp = g_strdup (g_ptr_array_index (fields, 1));
        }
        if (fields->len > 2) {
            info->step_author = g_strdup (g_ptr_array_index (fields, 2));
        }
        g_ptr_array_free (fields, TRUE);
    }

    stmt = find_step_statement (buf, "FILE_SCHEMA");
    if (stmt != NULL) {
        GPtrArray *fields = extract_quoted_strings (stmt, STEP_HEADER_READ_LIMIT);
        if (fields->len > 0) {
            info->step_schema = g_strdup (g_ptr_array_index (fields, 0));
        }
        g_ptr_array_free (fields, TRUE);
    }

    g_free (buf);
}

/* -------------------------------------------------------------- FCStd */

typedef struct {
    GHashTable *properties;   /* property name -> string value */
    gchar *current_property;
} FcstdParseState;

static void
fcstd_xml_start_element (GMarkupParseContext *context,
                         const gchar *element_name,
                         const gchar **attribute_names,
                         const gchar **attribute_values,
                         gpointer user_data,
                         GError **error)
{
    FcstdParseState *state = user_data;
    guint i;

    if (g_strcmp0 (element_name, "Property") == 0) {
        g_clear_pointer (&state->current_property, g_free);
        for (i = 0; attribute_names[i] != NULL; i++) {
            if (g_strcmp0 (attribute_names[i], "name") == 0) {
                state->current_property = g_strdup (attribute_values[i]);
                break;
            }
        }
    } else if (g_strcmp0 (element_name, "String") == 0 && state->current_property != NULL) {
        for (i = 0; attribute_names[i] != NULL; i++) {
            if (g_strcmp0 (attribute_names[i], "value") == 0) {
                g_hash_table_insert (state->properties,
                                     g_strdup (state->current_property),
                                     g_strdup (attribute_values[i]));
                break;
            }
        }
    }
}

static const GMarkupParser fcstd_xml_parser = {
    fcstd_xml_start_element,
    NULL, NULL, NULL, NULL
};

static gchar *
lookup_any (GHashTable *properties, ...)
{
    va_list args;
    const gchar *key;
    gchar *result = NULL;

    va_start (args, properties);
    while ((key = va_arg (args, const gchar *)) != NULL) {
        const gchar *value = g_hash_table_lookup (properties, key);
        if (value != NULL && value[0] != '\0') {
            result = g_strdup (value);
            break;
        }
    }
    va_end (args);

    return result;
}

static void
parse_fcstd (const gchar *path, NolphinCadInfo *info, GError **error)
{
    GsfInput *input;
    GsfInfile *zip;
    GsfInput *doc_xml;
    GError *gsf_error = NULL;

    info->format = NOLPHIN_CAD_FORMAT_FCSTD;

    input = gsf_input_stdio_new (path, &gsf_error);
    if (input == NULL) {
        g_set_error (error, NOLPHIN_CAD_ERROR, NOLPHIN_CAD_ERROR_READ_FAILED,
                    "%s", gsf_error ? gsf_error->message : "?");
        g_clear_error (&gsf_error);
        return;
    }

    zip = gsf_infile_zip_new (input, &gsf_error);
    g_object_unref (input);
    if (zip == NULL) {
        g_set_error (error, NOLPHIN_CAD_ERROR, NOLPHIN_CAD_ERROR_NOT_A_CAD_FILE,
                    _("Kein gültiger ZIP-Container (FCStd-Dateien sind ZIP-Archive): %s"),
                    gsf_error ? gsf_error->message : "?");
        g_clear_error (&gsf_error);
        return;
    }

    doc_xml = gsf_infile_child_by_name (zip, "Document.xml");
    if (doc_xml == NULL) {
        g_object_unref (zip);
        g_set_error (error, NOLPHIN_CAD_ERROR, NOLPHIN_CAD_ERROR_NOT_A_CAD_FILE,
                    _("Keine Document.xml enthalten - das sieht nicht nach einer FreeCAD-Datei aus."));
        return;
    }

    {
        gsf_off_t size = gsf_input_size (doc_xml);
        guint8 const *data = gsf_input_read (doc_xml, size, NULL);

        if (data != NULL) {
            FcstdParseState state = { g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free), NULL };
            GMarkupParseContext *ctx = g_markup_parse_context_new (&fcstd_xml_parser, 0, &state, NULL);

            g_markup_parse_context_parse (ctx, (const gchar *) data, size, NULL);
            g_markup_parse_context_end_parse (ctx, NULL);
            g_markup_parse_context_free (ctx);
            g_free (state.current_property);

            /* Property names per FreeCAD's documented Document.xml
             * layout (RECHERCHE: general format knowledge, not
             * verified against a real sample file - none exists on
             * this system or in this repository). Absent/mismatched
             * names simply leave the corresponding field NULL. */
            info->fcstd_comment = lookup_any (state.properties, "Comment", NULL);
            info->fcstd_author = lookup_any (state.properties, "CreatedBy", "LastModifiedBy", NULL);
            info->fcstd_company = lookup_any (state.properties, "Company", NULL);
            info->fcstd_created_date = lookup_any (state.properties, "CreationDate", NULL);
            info->fcstd_last_modified_date = lookup_any (state.properties, "LastModifiedDate", NULL);

            g_hash_table_unref (state.properties);
        }
    }
    g_object_unref (doc_xml);

    /* Best-effort thumbnail extraction. UNSICHER: this exact path
     * ("thumbnails/Thumbnail.png") is based on general knowledge of
     * how FreeCAD embeds preview images, not verified against a real
     * .FCStd file - if it's wrong or absent, we simply have no
     * thumbnail, which is not treated as an error. */
    {
        GsfInfile *thumb_dir = GSF_INFILE (gsf_infile_child_by_name (zip, "thumbnails"));
        if (thumb_dir != NULL) {
            GsfInput *thumb_png = gsf_infile_child_by_name (thumb_dir, "Thumbnail.png");
            if (thumb_png != NULL) {
                gsf_off_t tsize = gsf_input_size (thumb_png);
                guint8 const *tdata = gsf_input_read (thumb_png, tsize, NULL);
                if (tdata != NULL) {
                    info->fcstd_thumbnail_png = g_bytes_new (tdata, tsize);
                }
                g_object_unref (thumb_png);
            }
            g_object_unref (thumb_dir);
        }
    }

    g_object_unref (zip);
}

/* ------------------------------------------------------------- async */

static void
cad_info_thread (GTask *task, gpointer source_object, gpointer task_data, GCancellable *cancellable)
{
    GFile *file = task_data;
    NolphinCadFormat format = nolphin_cad_detect_format (file);
    NolphinCadInfo *info;
    GError *error = NULL;
    gchar *path;
    GFileInfo *finfo;

    if (!nolphin_cad_format_has_backend (format)) {
        g_task_return_new_error (task, NOLPHIN_CAD_ERROR, NOLPHIN_CAD_ERROR_NO_BACKEND,
                                 _("Für %s-Dateien ist auf diesem System noch kein Backend verfügbar."),
                                 nolphin_cad_format_get_label (format));
        return;
    }

    path = g_file_get_path (file);
    if (path == NULL) {
        g_task_return_new_error (task, NOLPHIN_CAD_ERROR, NOLPHIN_CAD_ERROR_READ_FAILED,
                                 _("Entfernte Orte werden für die CAD-Dateianalyse noch nicht unterstützt."));
        return;
    }

    info = g_new0 (NolphinCadInfo, 1);
    info->format = format;

    switch (format) {
        case NOLPHIN_CAD_FORMAT_STL:
            finfo = g_file_query_info (file, G_FILE_ATTRIBUTE_STANDARD_SIZE,
                                       G_FILE_QUERY_INFO_NONE, cancellable, &error);
            if (finfo == NULL) {
                nolphin_cad_info_free (info);
                g_free (path);
                g_task_return_error (task, error);
                return;
            }
            parse_stl (path, (guint64) g_file_info_get_size (finfo), info, &error);
            g_object_unref (finfo);
            break;
        case NOLPHIN_CAD_FORMAT_STEP:
            parse_step (path, info, &error);
            break;
        case NOLPHIN_CAD_FORMAT_FCSTD:
            parse_fcstd (path, info, &error);
            break;
        default:
            g_assert_not_reached ();
    }

    g_free (path);

    if (error != NULL) {
        nolphin_cad_info_free (info);
        g_task_return_error (task, error);
        return;
    }

    g_task_return_pointer (task, info, (GDestroyNotify) nolphin_cad_info_free);
}

void
nolphin_cad_get_info_async (GFile               *file,
                            GCancellable        *cancellable,
                            GAsyncReadyCallback   callback,
                            gpointer              user_data)
{
    GTask *task = g_task_new (NULL, cancellable, callback, user_data);

    g_task_set_task_data (task, g_object_ref (file), g_object_unref);
    g_task_run_in_thread (task, cad_info_thread);
    g_object_unref (task);
}

NolphinCadInfo *
nolphin_cad_get_info_finish (GAsyncResult *result, GError **error)
{
    return g_task_propagate_pointer (G_TASK (result), error);
}
