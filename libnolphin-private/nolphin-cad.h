/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nolphin-cad.h: native 3D/CAD file recognition and metadata
 *
 * Deliberately dependency-free: STL is parsed directly from its
 * documented binary/ASCII layout, STEP from its plain-text HEADER;
 * section, and FCStd (a ZIP container) via libgsf, which the project
 * already depends on. None of this reads actual 3D geometry - there is
 * no CAD kernel (e.g. OpenCASCADE) or FreeCAD available on this system
 * (verified during the accompanying analysis), so no geometric preview
 * is possible yet. The preview is planned in stages.
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

#ifndef NOLPHIN_CAD_H
#define NOLPHIN_CAD_H

#include <gio/gio.h>

G_BEGIN_DECLS

typedef enum {
    NOLPHIN_CAD_FORMAT_UNKNOWN,
    NOLPHIN_CAD_FORMAT_STL,
    NOLPHIN_CAD_FORMAT_STEP,
    NOLPHIN_CAD_FORMAT_FCSTD,
    /* Recognized, but with no backend at all yet - not even
     * text-header metadata. get_info_async() reports a clean "no
     * backend available" error for these, never fake data. */
    NOLPHIN_CAD_FORMAT_IGES,
    NOLPHIN_CAD_FORMAT_OBJ,
    NOLPHIN_CAD_FORMAT_3MF,
    NOLPHIN_CAD_FORMAT_DXF,
    NOLPHIN_CAD_FORMAT_DWG
} NolphinCadFormat;

NolphinCadFormat nolphin_cad_detect_format (GFile *file);
const gchar      *nolphin_cad_format_get_label (NolphinCadFormat format);

/* Whether get_info_async() can extract anything at all for this
 * format on this system right now (true only for STL/STEP/FCStd). */
gboolean nolphin_cad_format_has_backend (NolphinCadFormat format);

typedef struct {
    NolphinCadFormat format;

    /* STL */
    gboolean stl_is_binary;       /* FALSE means ASCII */
    guint64  stl_triangle_count;  /* 0 if not determined (see stl_triangle_count_known) */
    gboolean stl_triangle_count_known;

    /* STEP - text pulled straight from the HEADER; section, NULL if
     * that entry wasn't found in the (bounded) prefix read. */
    gchar *step_description;
    gchar *step_file_name;
    gchar *step_timestamp;
    gchar *step_author;
    gchar *step_schema;

    /* FCStd - from Document.xml's <Properties>, same "NULL if absent"
     * rule. fcstd_thumbnail is NULL if the archive has none embedded
     * or it couldn't be found - this project has no sample .FCStd file
     * to verify the exact internal thumbnail path against, so treat
     * its absence as unremarkable, not an error. */
    gchar  *fcstd_comment;
    gchar  *fcstd_author;
    gchar  *fcstd_company;
    gchar  *fcstd_created_date;
    gchar  *fcstd_last_modified_date;
    GBytes *fcstd_thumbnail_png;
} NolphinCadInfo;

void nolphin_cad_get_info_async  (GFile               *file,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback   callback,
                                  gpointer              user_data);
/* Returns a newly allocated NolphinCadInfo (free with
 * nolphin_cad_info_free()), or NULL with @error set - including a
 * clear NOLPHIN_CAD_ERROR_NO_BACKEND for formats where none exists. */
NolphinCadInfo *nolphin_cad_get_info_finish (GAsyncResult *result, GError **error);

void nolphin_cad_info_free (NolphinCadInfo *info);

#define NOLPHIN_CAD_ERROR (nolphin_cad_error_quark ())
GQuark nolphin_cad_error_quark (void);

typedef enum {
    NOLPHIN_CAD_ERROR_NO_BACKEND,
    NOLPHIN_CAD_ERROR_NOT_A_CAD_FILE,
    NOLPHIN_CAD_ERROR_READ_FAILED
} NolphinCadErrorCode;

G_END_DECLS

#endif /* NOLPHIN_CAD_H */
