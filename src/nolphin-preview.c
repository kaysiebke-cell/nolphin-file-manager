/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nolphin-preview.c: right-hand info/preview panel (F11)
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

#include "nolphin-preview.h"

#include <glib/gi18n.h>
#include <libnolphin-private/nolphin-global-preferences.h>
#include <libnolphin-private/nolphin-cad.h>
#include <libnolphin-private/nolphin-media.h>
#include <libnolphin-private/nolphin-metadata.h>

#define PREVIEW_IMAGE_SIZE 256
#define RATING_STARS 5


/* Obergrenze für den Textinhalt, den die Vorschau-Leiste lädt und
 * anzeigt - größere Textdateien bekommen weiterhin Icon/Metadaten,
 * aber keinen eingebetteten Inhalt (dafür "Öffnen mit" nutzen). */
#define TEXT_PREVIEW_MAX_SIZE (256 * 1024)

struct _NolphinPreview
{
    GtkBox parent_instance;

    GtkWidget *image;
    GtkWidget *name_label;
    GtkWidget *info_grid;
    GtkWidget *fallback_label;

    /* Semantisches Tagging: Sternebewertung (1-5), Tags und
     * Kommentar der einzelnen Datei (nolphin_file_get/set_rating,
     * _keywords, _comment - dieselben Daten wie die Kontextmenü-Dialoge).
     * Eigene, dauerhafte Widgets (nicht im info_grid), damit ein
     * "changed" der Datei die Eingabe nicht mitten im Tippen zerstört. */
    GtkWidget *meta_box;
    GtkWidget *star_buttons[RATING_STARS];
    GtkWidget *tags_entry;
    GtkWidget *comment_entry;
    gboolean meta_updating;

    /* Echter, markier- und kopierbarer Dateiinhalt für Textdateien
     * (siehe TEXT_PREVIEW_MAX_SIZE) - anders als die restliche
     * Vorschau (Icon/Metadaten-Raster) kein reines Abbild. */
    GtkWidget *text_scrolled;
    GtkWidget *text_view;
    GCancellable *text_cancellable;
    NolphinFile *text_info_file;

    /* The single file this panel is currently tracking for async
     * refresh (late thumbnails, in-progress folder size, live
     * metadata changes). NULL in the empty/multi-selection cases,
     * where there's nothing sensible to keep watching. */
    NolphinFile *watched_file;
    gulong changed_handler_id;
    gulong deep_count_handler_id;

    /* §29: 3D/CAD metadata for watched_file, filled in asynchronously
     * (nolphin_cad_get_info_async runs a background thread - see
     * nolphin-cad.c). cad_info_file marks which file the two fields
     * below apply to (may be an attempt that failed, cad_info_error
     * set instead of cad_info) - NULL when nothing has been requested
     * yet for the current watched_file. */
    /* §34: PDF-/Video-/Audio-Angaben für watched_file, asynchron geladen. */
    GCancellable *media_cancellable;
    NolphinFile *media_info_file;
    NolphinMediaInfo *media_info;
    gchar *media_info_error;

    GCancellable *cad_cancellable;
    NolphinFile *cad_info_file;
    NolphinCadInfo *cad_info;
    gchar *cad_info_error;
};

G_DEFINE_TYPE (NolphinPreview, nolphin_preview, GTK_TYPE_BOX)

static void display_subject (NolphinPreview *preview, NolphinFile *file);

static void
clear_cad_state (NolphinPreview *preview)
{
    if (preview->cad_cancellable != NULL) {
        g_cancellable_cancel (preview->cad_cancellable);
        g_clear_object (&preview->cad_cancellable);
    }
    g_clear_pointer (&preview->cad_info, nolphin_cad_info_free);
    g_clear_pointer (&preview->cad_info_error, g_free);
    g_clear_pointer (&preview->cad_info_file, nolphin_file_unref);
}

static void
clear_media_state (NolphinPreview *preview)
{
    if (preview->media_cancellable != NULL) {
        g_cancellable_cancel (preview->media_cancellable);
        g_clear_object (&preview->media_cancellable);
    }
    g_clear_pointer (&preview->media_info, nolphin_media_info_free);
    g_clear_pointer (&preview->media_info_error, g_free);
    g_clear_pointer (&preview->media_info_file, nolphin_file_unref);
}

static void
clear_text_state (NolphinPreview *preview)
{
    if (preview->text_cancellable != NULL) {
        g_cancellable_cancel (preview->text_cancellable);
        g_clear_object (&preview->text_cancellable);
    }
    g_clear_pointer (&preview->text_info_file, nolphin_file_unref);
    gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (preview->text_view)), "", -1);
    gtk_widget_hide (preview->text_scrolled);
}

static void
stop_watching_file (NolphinPreview *preview)
{
    if (preview->watched_file == NULL) {
        return;
    }

    if (preview->changed_handler_id != 0) {
        g_signal_handler_disconnect (preview->watched_file, preview->changed_handler_id);
        preview->changed_handler_id = 0;
    }
    if (preview->deep_count_handler_id != 0) {
        g_signal_handler_disconnect (preview->watched_file, preview->deep_count_handler_id);
        preview->deep_count_handler_id = 0;
    }

    clear_cad_state (preview);
    clear_media_state (preview);
    clear_text_state (preview);

    nolphin_file_unref (preview->watched_file);
    preview->watched_file = NULL;
}

static void
watched_file_changed_cb (NolphinFile *file,
                          gpointer     user_data)
{
    NolphinPreview *preview = NOLPHIN_PREVIEW (user_data);

    /* Re-render with the same subject; picks up whatever changed
     * (permissions, a newly-arrived thumbnail, deep count progress). */
    display_subject (preview, file);
}

static void
add_info_row (GtkGrid *grid, gint row, const gchar *label_text, const gchar *value_text)
{
    GtkWidget *label, *value;

    if (value_text == NULL || value_text[0] == '\0') {
        return;
    }

    label = gtk_label_new (label_text);
    gtk_widget_set_halign (label, GTK_ALIGN_START);
    gtk_style_context_add_class (gtk_widget_get_style_context (label), "dim-label");
    gtk_grid_attach (grid, label, 0, row, 1, 1);

    value = gtk_label_new (value_text);
    gtk_widget_set_halign (value, GTK_ALIGN_START);
    gtk_label_set_line_wrap (GTK_LABEL (value), TRUE);
    gtk_label_set_selectable (GTK_LABEL (value), TRUE);
    gtk_grid_attach (grid, value, 1, row, 1, 1);

    gtk_widget_show (label);
    gtk_widget_show (value);
}

static void
clear_grid (GtkGrid *grid)
{
    GList *children, *l;

    children = gtk_container_get_children (GTK_CONTAINER (grid));
    for (l = children; l != NULL; l = l->next) {
        gtk_widget_destroy (GTK_WIDGET (l->data));
    }
    g_list_free (children);
}

typedef struct {
    NolphinPreview *preview; /* reffed, so this stays valid even if the
                               * panel is torn down mid-request */
    NolphinFile *file;       /* reffed - the subject this analysis is for */
} CadRequest;

static void
cad_request_free (CadRequest *req)
{
    g_object_unref (req->preview);
    nolphin_file_unref (req->file);
    g_free (req);
}

static void
cad_info_ready_cb (GObject *source, GAsyncResult *result, gpointer user_data)
{
    CadRequest *req = user_data;
    GError *error = NULL;
    NolphinCadInfo *info = nolphin_cad_get_info_finish (result, &error);

    /* Only apply the result if this is still the file the panel is
     * showing - otherwise the user has already moved on and this is a
     * stale, possibly-cancelled request that arrived late. */
    if (req->file == req->preview->watched_file) {
        g_clear_pointer (&req->preview->cad_info, nolphin_cad_info_free);
        g_clear_pointer (&req->preview->cad_info_error, g_free);
        g_clear_pointer (&req->preview->cad_info_file, nolphin_file_unref);

        req->preview->cad_info_file = nolphin_file_ref (req->file);
        if (info != NULL) {
            req->preview->cad_info = info;
        } else {
            req->preview->cad_info_error = g_strdup (error != NULL ? error->message : _("Unbekannter Fehler"));
        }

        /* Re-render: display_single_file() will now find the cache
         * populated and show the real rows instead of re-requesting. */
        display_subject (req->preview, req->file);
    } else {
        nolphin_cad_info_free (info);
    }

    g_clear_error (&error);
    cad_request_free (req);
}

static void
add_cad_info_rows (GtkGrid *grid, gint *row, NolphinCadInfo *info)
{
    gchar *text;

    switch (info->format) {
        case NOLPHIN_CAD_FORMAT_STL:
            add_info_row (grid, (*row)++, _("STL-Typ:"),
                          info->stl_is_binary ? _("Binär") : _("ASCII"));
            if (info->stl_triangle_count_known) {
                text = g_strdup_printf ("%" G_GUINT64_FORMAT, info->stl_triangle_count);
                add_info_row (grid, (*row)++, _("Dreiecke:"), text);
                g_free (text);
            } else {
                add_info_row (grid, (*row)++, _("Dreiecke:"), _("nicht gezählt (Datei zu groß)"));
            }
            break;
        case NOLPHIN_CAD_FORMAT_STEP:
            add_info_row (grid, (*row)++, _("STEP-Beschreibung:"), info->step_description);
            add_info_row (grid, (*row)++, _("STEP-Dateiname:"), info->step_file_name);
            add_info_row (grid, (*row)++, _("STEP-Zeitstempel:"), info->step_timestamp);
            add_info_row (grid, (*row)++, _("STEP-Autor:"), info->step_author);
            add_info_row (grid, (*row)++, _("STEP-Schema:"), info->step_schema);
            break;
        case NOLPHIN_CAD_FORMAT_FCSTD:
            add_info_row (grid, (*row)++, _("FreeCAD-Kommentar:"), info->fcstd_comment);
            add_info_row (grid, (*row)++, _("FreeCAD-Autor:"), info->fcstd_author);
            add_info_row (grid, (*row)++, _("FreeCAD-Firma:"), info->fcstd_company);
            add_info_row (grid, (*row)++, _("FreeCAD erstellt:"), info->fcstd_created_date);
            add_info_row (grid, (*row)++, _("FreeCAD geändert:"), info->fcstd_last_modified_date);
            break;
        default:
            break;
    }
}

typedef struct {
    NolphinPreview *preview; /* reffed */
    NolphinFile *file;       /* reffed - the subject this request is for */
} MediaRequest;

static void
media_request_free (MediaRequest *req)
{
    g_object_unref (req->preview);
    nolphin_file_unref (req->file);
    g_free (req);
}

static void
media_info_ready_cb (GObject *source, GAsyncResult *result, gpointer user_data)
{
    MediaRequest *req = user_data;
    GError *error = NULL;
    NolphinMediaInfo *info = nolphin_media_get_info_finish (result, &error);

    /* Nur anwenden, wenn die Datei noch angezeigt wird (sonst verspätetes Ergebnis). */
    if (req->file == req->preview->watched_file) {
        g_clear_pointer (&req->preview->media_info, nolphin_media_info_free);
        g_clear_pointer (&req->preview->media_info_error, g_free);
        g_clear_pointer (&req->preview->media_info_file, nolphin_file_unref);

        req->preview->media_info_file = nolphin_file_ref (req->file);
        if (info != NULL) {
            req->preview->media_info = info;
            info = NULL;
        } else {
            req->preview->media_info_error = g_strdup (error != NULL ? error->message : _("Unbekannter Fehler"));
        }
        display_subject (req->preview, req->file);
    }

    nolphin_media_info_free (info);
    g_clear_error (&error);
    media_request_free (req);
}

typedef struct {
    NolphinPreview *preview; /* reffed */
    NolphinFile *file;       /* reffed - the subject this load is for */
} TextRequest;

static void
text_request_free (TextRequest *req)
{
    g_object_unref (req->preview);
    nolphin_file_unref (req->file);
    g_free (req);
}

static void
text_load_ready_cb (GObject *source, GAsyncResult *result, gpointer user_data)
{
    TextRequest *req = user_data;
    gchar *contents = NULL;
    gsize length = 0;
    GError *error = NULL;

    g_file_load_contents_finish (G_FILE (source), result, &contents, &length, NULL, &error);

    /* Nur anwenden, wenn der Nutzer in der Zwischenzeit nicht schon
     * eine andere Datei ausgewählt hat (sonst stiller, verworfener
     * Treffer - wie beim CAD-Pendant oben). */
    if (req->file == req->preview->watched_file) {
        if (error == NULL && length > 0 && g_utf8_validate (contents, length, NULL)) {
            gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (req->preview->text_view)),
                                      contents, length);
            gtk_widget_show (req->preview->text_scrolled);
        } else {
            /* Kein gültiger UTF-8-Text (z. B. andere Kodierung) - keine
             * vorgetäuschte Anzeige, Leiste bleibt beim Icon/den
             * Metadaten; der Inhalt ist weiterhin per "Öffnen mit"
             * erreichbar. */
            gtk_widget_hide (req->preview->text_scrolled);
        }
    }

    g_free (contents);
    g_clear_error (&error);
    text_request_free (req);
}

static void
update_rating_buttons (NolphinPreview *preview, int rating)
{
    int i;

    for (i = 0; i < RATING_STARS; i++) {
        gtk_button_set_label (GTK_BUTTON (preview->star_buttons[i]), i < rating ? "★" : "☆");
    }
}

static void
star_clicked_cb (GtkButton *button, gpointer user_data)
{
    NolphinPreview *preview = NOLPHIN_PREVIEW (user_data);
    int n = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (button), "star-index")) + 1;

    if (preview->watched_file == NULL || preview->meta_updating) {
        return;
    }

    /* Erneuter Klick auf den aktuellen Stern nimmt die Bewertung zurück. */
    if (n == nolphin_file_get_rating (preview->watched_file)) {
        n = 0;
    }

    nolphin_file_set_rating (preview->watched_file, n);
    update_rating_buttons (preview, n);
}

/* Tags kommagetrennt, wie im Tag-Dialog der Ansicht: nur die vom Nutzer
 * vergebenen (metadata::emblems), nicht die automatisch erzeugten. */
static gchar *
tags_to_string (NolphinFile *file)
{
    GList *tags = nolphin_file_get_metadata_list (file, NOLPHIN_METADATA_KEY_EMBLEMS);
    GString *joined = g_string_new (NULL);
    GList *l;

    for (l = tags; l != NULL; l = l->next) {
        if (joined->len > 0) {
            g_string_append (joined, ", ");
        }
        g_string_append (joined, l->data);
    }
    g_list_free_full (tags, g_free);

    return g_string_free (joined, FALSE);
}

static void
save_tags (NolphinPreview *preview)
{
    gchar **parts;
    GList *new_tags = NULL;
    gchar *old;
    guint i;

    if (preview->watched_file == NULL || preview->meta_updating) {
        return;
    }

    parts = g_strsplit (gtk_entry_get_text (GTK_ENTRY (preview->tags_entry)), ",", -1);
    for (i = 0; parts[i] != NULL; i++) {
        gchar *tag = g_strstrip (parts[i]);

        if (tag[0] != '\0' && g_list_find_custom (new_tags, tag, (GCompareFunc) g_strcmp0) == NULL) {
            new_tags = g_list_append (new_tags, tag);
        }
    }

    /* Nur schreiben, wenn sich etwas geändert hat (sonst "changed"-Rauschen). */
    old = tags_to_string (preview->watched_file);
    {
        GString *now = g_string_new (NULL);
        GList *l;

        for (l = new_tags; l != NULL; l = l->next) {
            if (now->len > 0) {
                g_string_append (now, ", ");
            }
            g_string_append (now, l->data);
        }
        if (g_strcmp0 (old, now->str) != 0) {
            nolphin_file_set_keywords (preview->watched_file, new_tags);
        }
        g_string_free (now, TRUE);
    }

    g_free (old);
    g_list_free (new_tags);
    g_strfreev (parts);
}

static void
save_comment (NolphinPreview *preview)
{
    const gchar *text;
    gchar *old;

    if (preview->watched_file == NULL || preview->meta_updating) {
        return;
    }

    text = gtk_entry_get_text (GTK_ENTRY (preview->comment_entry));
    old = nolphin_file_get_comment (preview->watched_file);
    if (g_strcmp0 (old, text) != 0) {
        nolphin_file_set_comment (preview->watched_file, text);
    }
    g_free (old);
}

static void
meta_entry_activate_cb (GtkEntry *entry, gpointer user_data)
{
    NolphinPreview *preview = NOLPHIN_PREVIEW (user_data);

    if (GTK_WIDGET (entry) == preview->tags_entry) {
        save_tags (preview);
    } else {
        save_comment (preview);
    }
}

static gboolean
meta_entry_focus_out_cb (GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
    meta_entry_activate_cb (GTK_ENTRY (widget), user_data);
    return FALSE;
}

/* Gleicht Sterne/Tags/Kommentar mit den Metadaten von @file ab; ein
 * gerade fokussiertes Eingabefeld wird nicht überschrieben. */
static void
refresh_meta_box (NolphinPreview *preview, NolphinFile *file)
{
    gchar *text;

    if (file == NULL) {
        gtk_widget_hide (preview->meta_box);
        return;
    }

    preview->meta_updating = TRUE;

    update_rating_buttons (preview, nolphin_file_get_rating (file));

    if (!gtk_widget_has_focus (preview->tags_entry)) {
        text = tags_to_string (file);
        gtk_entry_set_text (GTK_ENTRY (preview->tags_entry), text);
        g_free (text);
    }
    if (!gtk_widget_has_focus (preview->comment_entry)) {
        text = nolphin_file_get_comment (file);
        gtk_entry_set_text (GTK_ENTRY (preview->comment_entry), text);
        g_free (text);
    }

    preview->meta_updating = FALSE;
    gtk_widget_show (preview->meta_box);
}

static GtkWidget *
build_meta_box (NolphinPreview *preview)
{
    GtkWidget *box, *row, *label;
    guint i;

    box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);

    row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    label = gtk_label_new (_("Bewertung:"));
    gtk_style_context_add_class (gtk_widget_get_style_context (label), "dim-label");
    gtk_box_pack_start (GTK_BOX (row), label, FALSE, FALSE, 8);
    for (i = 0; i < RATING_STARS; i++) {
        GtkWidget *b = gtk_button_new_with_label ("☆");

        gtk_button_set_relief (GTK_BUTTON (b), GTK_RELIEF_NONE);
        gtk_widget_set_can_focus (b, FALSE);
        gtk_widget_set_tooltip_text (b, _("Bewertung setzen (erneuter Klick entfernt sie)"));
        g_object_set_data (G_OBJECT (b), "star-index", GINT_TO_POINTER (i));
        g_signal_connect (b, "clicked", G_CALLBACK (star_clicked_cb), preview);
        gtk_box_pack_start (GTK_BOX (row), b, FALSE, FALSE, 0);
        preview->star_buttons[i] = b;
    }
    gtk_box_pack_start (GTK_BOX (box), row, FALSE, FALSE, 0);

    preview->tags_entry = gtk_entry_new ();
    gtk_entry_set_placeholder_text (GTK_ENTRY (preview->tags_entry), _("Tags, durch Komma getrennt …"));
    g_signal_connect (preview->tags_entry, "activate", G_CALLBACK (meta_entry_activate_cb), preview);
    g_signal_connect (preview->tags_entry, "focus-out-event", G_CALLBACK (meta_entry_focus_out_cb), preview);
    gtk_box_pack_start (GTK_BOX (box), preview->tags_entry, FALSE, FALSE, 0);

    preview->comment_entry = gtk_entry_new ();
    gtk_entry_set_placeholder_text (GTK_ENTRY (preview->comment_entry), _("Kommentar hinzufügen …"));
    g_signal_connect (preview->comment_entry, "activate", G_CALLBACK (meta_entry_activate_cb), preview);
    g_signal_connect (preview->comment_entry, "focus-out-event", G_CALLBACK (meta_entry_focus_out_cb), preview);
    gtk_box_pack_start (GTK_BOX (box), preview->comment_entry, FALSE, FALSE, 0);

    return box;
}

static void
display_single_file (NolphinPreview *preview, NolphinFile *file)
{
    GtkGrid *grid = GTK_GRID (preview->info_grid);
    gint row = 0;
    gchar *display_name;
    gchar *text;
    goffset size;
    gboolean is_dir;

    display_name = nolphin_file_get_display_name (file);
    gtk_label_set_text (GTK_LABEL (preview->name_label), display_name);
    gtk_label_set_line_wrap (GTK_LABEL (preview->name_label), TRUE);
    g_free (display_name);

    clear_grid (grid);

    text = nolphin_file_get_string_attribute (file, "type");
    add_info_row (grid, row++, _("Typ:"), text);
    g_free (text);

    is_dir = nolphin_file_is_directory (file);
    size = nolphin_file_get_size (file);
    if (is_dir) {
        text = nolphin_file_get_string_attribute (file, "deep_size");
    } else if (size >= 0) {
        text = nolphin_file_get_string_attribute (file, "size");
    } else {
        text = NULL;
    }
    add_info_row (grid, row++, _("Größe:"), text);
    g_free (text);

    text = nolphin_file_get_string_attribute (file, "date_modified_full");
    add_info_row (grid, row++, _("Geändert:"), text);
    g_free (text);

    text = nolphin_file_get_string_attribute (file, "date_accessed_full");
    add_info_row (grid, row++, _("Zugegriffen:"), text);
    g_free (text);

    text = nolphin_file_get_string_attribute (file, "permissions");
    add_info_row (grid, row++, _("Zugriffsrechte:"), text);
    g_free (text);

    text = nolphin_file_get_string_attribute (file, "owner");
    add_info_row (grid, row++, _("Eigentümer:"), text);
    g_free (text);

    text = nolphin_file_get_string_attribute (file, "group");
    add_info_row (grid, row++, _("Gruppe:"), text);
    g_free (text);

    text = nolphin_file_get_string_attribute (file, "where");
    add_info_row (grid, row++, _("Ort:"), text);
    g_free (text);

    text = nolphin_file_get_symbolic_link_target_path (file);
    if (text != NULL) {
        add_info_row (grid, row++, _("Verknüpfungsziel:"), text);
        g_free (text);
    }

    /* §29: 3D/CAD metadata. Detection itself is free (just an
     * extension check); the actual read happens in a background
     * thread and is cached per-file so re-rendering (e.g. once the
     * result arrives) doesn't re-request it. */
    {
        GFile *location = nolphin_file_get_location (file);
        NolphinCadFormat cad_format = nolphin_cad_detect_format (location);

        if (cad_format == NOLPHIN_CAD_FORMAT_UNKNOWN) {
            g_object_unref (location);
        } else if (!nolphin_cad_format_has_backend (cad_format)) {
            add_info_row (grid, row++, _("3D/CAD-Format:"), nolphin_cad_format_get_label (cad_format));
            add_info_row (grid, row++, _("3D/CAD-Vorschau:"),
                          _("Für dieses Format ist auf diesem System kein Backend verfügbar"));
            g_object_unref (location);
        } else if (preview->cad_info_file == file) {
            /* Already attempted for this exact file - show the cached
             * outcome instead of asking again. */
            add_info_row (grid, row++, _("3D/CAD-Format:"), nolphin_cad_format_get_label (cad_format));
            if (preview->cad_info != NULL) {
                add_cad_info_rows (grid, &row, preview->cad_info);
            } else {
                add_info_row (grid, row++, _("3D/CAD-Vorschau:"), preview->cad_info_error);
            }
            g_object_unref (location);
        } else {
            CadRequest *req;

            add_info_row (grid, row++, _("3D/CAD-Format:"), nolphin_cad_format_get_label (cad_format));
            add_info_row (grid, row++, _("3D/CAD-Vorschau:"), _("Wird analysiert …"));

            if (preview->cad_cancellable != NULL) {
                g_cancellable_cancel (preview->cad_cancellable);
                g_object_unref (preview->cad_cancellable);
            }
            preview->cad_cancellable = g_cancellable_new ();

            req = g_new0 (CadRequest, 1);
            req->preview = g_object_ref (preview);
            req->file = nolphin_file_ref (file);

            nolphin_cad_get_info_async (location, preview->cad_cancellable, cad_info_ready_cb, req);
            g_object_unref (location);
        }
    }

    /* §34 Erweiterte Vorschau: PDF, Video, Audio (asynchron, mit Cache je Datei). */
    if (!is_dir) {
        gchar *mime = nolphin_file_get_mime_type (file);

        if (nolphin_media_is_supported (mime)) {
            if (preview->media_info_file == file) {
                if (preview->media_info != NULL) {
                    guint k;

                    for (k = 0; k < preview->media_info->rows->len; k++) {
                        NolphinMediaRow *r = preview->media_info->rows->pdata[k];

                        add_info_row (grid, row++, r->label, r->value);
                    }
                    add_info_row (grid, row++, _("Hinweis:"), preview->media_info->notice);
                } else {
                    add_info_row (grid, row++, _("Medien-Vorschau:"), preview->media_info_error);
                }
            } else {
                GFile *location = nolphin_file_get_location (file);
                MediaRequest *req;

                add_info_row (grid, row++, _("Medien-Angaben:"), _("Wird gelesen …"));

                if (preview->media_cancellable != NULL) {
                    g_cancellable_cancel (preview->media_cancellable);
                    g_object_unref (preview->media_cancellable);
                }
                preview->media_cancellable = g_cancellable_new ();

                req = g_new0 (MediaRequest, 1);
                req->preview = g_object_ref (preview);
                req->file = nolphin_file_ref (file);
                nolphin_media_get_info_async (location, mime, preview->media_cancellable,
                                              media_info_ready_cb, req);
                g_object_unref (location);
            }
        }
        g_free (mime);
    }

    /* Image preview. Skip large files rather than decode them
     * synchronously - nolphin_file_get_icon_pixbuf() returns whatever
     * is already cached/generated (falling back to a generic mime
     * icon) without blocking; a real thumbnail that's still being
     * generated in the background arrives later via "changed". */
    if (g_settings_get_boolean (nolphin_preferences, NOLPHIN_PREFERENCES_PREVIEW_ENABLED) &&
        (size < 0 || size <= g_settings_get_int64 (nolphin_preferences, NOLPHIN_PREFERENCES_PREVIEW_MAX_FILE_SIZE))) {
        GdkPixbuf *pixbuf;

        pixbuf = nolphin_file_get_icon_pixbuf (file, PREVIEW_IMAGE_SIZE, FALSE,
                                               gtk_widget_get_scale_factor (GTK_WIDGET (preview)),
                                               NOLPHIN_FILE_ICON_FLAGS_USE_THUMBNAILS);
        if (pixbuf != NULL) {
            gtk_image_set_from_pixbuf (GTK_IMAGE (preview->image), pixbuf);
            g_object_unref (pixbuf);
        } else {
            gtk_image_clear (GTK_IMAGE (preview->image));
        }
    } else {
        gtk_image_clear (GTK_IMAGE (preview->image));
    }

    /* If FreeCAD embedded a thumbnail in this .FCStd, prefer showing
     * that actual preview over the generic file-type icon above. */
    if (preview->cad_info_file == file && preview->cad_info != NULL &&
        preview->cad_info->fcstd_thumbnail_png != NULL) {
        GInputStream *stream = g_memory_input_stream_new_from_bytes (preview->cad_info->fcstd_thumbnail_png);
        GdkPixbuf *thumb = gdk_pixbuf_new_from_stream (stream, NULL, NULL);

        g_object_unref (stream);
        if (thumb != NULL) {
            gtk_image_set_from_pixbuf (GTK_IMAGE (preview->image), thumb);
            g_object_unref (thumb);
        }
    }

    /* Erste PDF-Seite als Vorschaubild, sobald verfügbar. */
    if (preview->media_info_file == file && preview->media_info != NULL &&
        preview->media_info->thumb_png != NULL) {
        GInputStream *stream = g_memory_input_stream_new_from_bytes (preview->media_info->thumb_png);
        GdkPixbuf *thumb = gdk_pixbuf_new_from_stream (stream, NULL, NULL);

        g_object_unref (stream);
        if (thumb != NULL) {
            gtk_image_set_from_pixbuf (GTK_IMAGE (preview->image), thumb);
            g_object_unref (thumb);
        }
    }

    /* Echter Dateiinhalt für Textdateien - markier- und kopierbar,
     * anders als das reine Icon/die Metadaten oben. §-los auf
     * ausdrücklichen Nutzerwunsch: Text lesen/kopieren können ist eine
     * Grundvoraussetzung in einem Dateimanager, nicht nur ein "nice
     * to have". Läuft asynchron und nur innerhalb von
     * TEXT_PREVIEW_MAX_SIZE, damit die Leiste bei großen Dateien nicht
     * blockiert oder unbrauchbar wird. */
    if (!is_dir && size >= 0 && size <= TEXT_PREVIEW_MAX_SIZE && nolphin_file_contains_text (file)) {
        if (preview->text_info_file != file) {
            GFile *location = nolphin_file_get_location (file);
            TextRequest *req;

            if (preview->text_cancellable != NULL) {
                g_cancellable_cancel (preview->text_cancellable);
                g_object_unref (preview->text_cancellable);
            }
            preview->text_cancellable = g_cancellable_new ();

            g_clear_pointer (&preview->text_info_file, nolphin_file_unref);
            preview->text_info_file = nolphin_file_ref (file);

            req = g_new0 (TextRequest, 1);
            req->preview = g_object_ref (preview);
            req->file = nolphin_file_ref (file);

            g_file_load_contents_async (location, preview->text_cancellable, text_load_ready_cb, req);
            g_object_unref (location);
        }
    } else {
        clear_text_state (preview);
    }

    refresh_meta_box (preview, file);

    if (is_dir) {
        nolphin_file_recompute_deep_counts (file);
    }
}

static void
display_multi_selection (NolphinPreview *preview, GList *selection)
{
    GtkGrid *grid = GTK_GRID (preview->info_grid);
    GList *l;
    guint count;
    goffset total_size;
    gchar *text;

    count = g_list_length (selection);
    total_size = 0;
    for (l = selection; l != NULL; l = l->next) {
        NolphinFile *file = NOLPHIN_FILE (l->data);
        if (!nolphin_file_is_directory (file)) {
            total_size += nolphin_file_get_size (file);
        }
    }

    text = g_strdup_printf (ngettext ("%u Objekt ausgewählt", "%u Objekte ausgewählt", count), count);
    gtk_label_set_text (GTK_LABEL (preview->name_label), text);
    g_free (text);

    clear_grid (grid);
    refresh_meta_box (preview, NULL);

    text = g_format_size (total_size);
    add_info_row (grid, 0, _("Gesamtgröße:"), text);
    g_free (text);
    add_info_row (grid, 1, _("Hinweis:"), _("Ordnergrößen nicht eingerechnet"));

    gtk_image_clear (GTK_IMAGE (preview->image));
}

static void
display_subject (NolphinPreview *preview, NolphinFile *file)
{
    if (file != preview->watched_file) {
        stop_watching_file (preview);

        preview->watched_file = nolphin_file_ref (file);
        preview->changed_handler_id =
            g_signal_connect (file, "changed", G_CALLBACK (watched_file_changed_cb), preview);
        preview->deep_count_handler_id =
            g_signal_connect (file, "updated_deep_count_in_progress",
                              G_CALLBACK (watched_file_changed_cb), preview);
    }

    display_single_file (preview, file);

    gtk_stack_set_visible_child_name (GTK_STACK (gtk_widget_get_parent (preview->fallback_label)),
                                      "content");
}

void
nolphin_preview_set_selection (NolphinPreview *preview,
                                GList          *selection,
                                NolphinFile    *directory_as_file)
{
    g_return_if_fail (NOLPHIN_IS_PREVIEW (preview));

    if (g_list_length (selection) == 1) {
        display_subject (preview, NOLPHIN_FILE (selection->data));
    } else if (selection != NULL) {
        stop_watching_file (preview);
        display_multi_selection (preview, selection);
    } else if (directory_as_file != NULL) {
        display_subject (preview, directory_as_file);
    } else {
        nolphin_preview_clear (preview);
    }
}

void
nolphin_preview_clear (NolphinPreview *preview)
{
    g_return_if_fail (NOLPHIN_IS_PREVIEW (preview));

    stop_watching_file (preview);
    gtk_label_set_text (GTK_LABEL (preview->name_label), "");
    clear_grid (GTK_GRID (preview->info_grid));
    refresh_meta_box (preview, NULL);
    gtk_image_clear (GTK_IMAGE (preview->image));
}

static void
nolphin_preview_dispose (GObject *object)
{
    NolphinPreview *preview = NOLPHIN_PREVIEW (object);

    stop_watching_file (preview);

    G_OBJECT_CLASS (nolphin_preview_parent_class)->dispose (object);
}

static void
nolphin_preview_class_init (NolphinPreviewClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->dispose = nolphin_preview_dispose;
}

static void
nolphin_preview_init (NolphinPreview *preview)
{
    GtkWidget *scrolled, *content_box, *stack;

    gtk_orientable_set_orientation (GTK_ORIENTABLE (preview), GTK_ORIENTATION_VERTICAL);

    stack = gtk_stack_new ();

    content_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_margin_start (content_box, 8);
    gtk_widget_set_margin_end (content_box, 8);
    gtk_widget_set_margin_top (content_box, 28);
    gtk_widget_set_margin_bottom (content_box, 8);

    preview->image = gtk_image_new ();
    gtk_widget_set_halign (preview->image, GTK_ALIGN_CENTER);
    gtk_box_pack_start (GTK_BOX (content_box), preview->image, FALSE, FALSE, 0);

    preview->name_label = gtk_label_new ("");
    gtk_label_set_line_wrap (GTK_LABEL (preview->name_label), TRUE);
    gtk_label_set_justify (GTK_LABEL (preview->name_label), GTK_JUSTIFY_CENTER);
    gtk_style_context_add_class (gtk_widget_get_style_context (preview->name_label), "heading");
    gtk_box_pack_start (GTK_BOX (content_box), preview->name_label, FALSE, FALSE, 0);

    preview->info_grid = gtk_grid_new ();
    gtk_grid_set_row_spacing (GTK_GRID (preview->info_grid), 4);
    gtk_grid_set_column_spacing (GTK_GRID (preview->info_grid), 8);
    gtk_box_pack_start (GTK_BOX (content_box), preview->info_grid, FALSE, FALSE, 0);

    preview->meta_box = build_meta_box (preview);
    gtk_box_pack_start (GTK_BOX (content_box), preview->meta_box, FALSE, FALSE, 0);

    preview->text_view = gtk_text_view_new ();
    gtk_text_view_set_editable (GTK_TEXT_VIEW (preview->text_view), TRUE);
    gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (preview->text_view), TRUE);
    gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (preview->text_view), GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_left_margin (GTK_TEXT_VIEW (preview->text_view), 4);
    gtk_text_view_set_right_margin (GTK_TEXT_VIEW (preview->text_view), 4);

    /* Eigene, vom Rest des Panels unabhängige Markierungsfarbe - das
     * dunkle Panel-Hintergrund-CSS (apply_workspace_panel_background())
     * reicht zwar nicht bis hierher runter, aber die System-Vorgabe für
     * "selected text" war in der Praxis kaum vom unmarkierten Text zu
     * unterscheiden. Deutlich sichtbares Blau statt Theme-Raten. */
    {
        GtkCssProvider *provider = gtk_css_provider_new ();

        gtk_css_provider_load_from_data (provider,
                                         "textview.nolphin-preview-text text selection {"
                                         "  background-color: @theme_selected_bg_color;"
                                         "  color: @theme_selected_fg_color;"
                                         "}",
                                         -1, NULL);
        gtk_style_context_add_provider (gtk_widget_get_style_context (preview->text_view),
                                        GTK_STYLE_PROVIDER (provider),
                                        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        g_object_unref (provider);
    }
    gtk_style_context_add_class (gtk_widget_get_style_context (preview->text_view), "nolphin-preview-text");
    gtk_style_context_add_class (gtk_widget_get_style_context (preview->text_view), "view");

    preview->text_scrolled = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (preview->text_scrolled),
                                    GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand (preview->text_scrolled, TRUE);
    gtk_widget_set_size_request (preview->text_scrolled, -1, 200);
    gtk_container_add (GTK_CONTAINER (preview->text_scrolled), preview->text_view);
    gtk_box_pack_start (GTK_BOX (content_box), preview->text_scrolled, TRUE, TRUE, 0);

    scrolled = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
                                    GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_container_add (GTK_CONTAINER (scrolled), content_box);
    gtk_stack_add_named (GTK_STACK (stack), scrolled, "content");

    preview->fallback_label = gtk_label_new (_("Keine Datei ausgewählt"));
    gtk_style_context_add_class (gtk_widget_get_style_context (preview->fallback_label), "dim-label");
    gtk_stack_add_named (GTK_STACK (stack), preview->fallback_label, "empty");

    gtk_stack_set_visible_child_name (GTK_STACK (stack), "empty");

    gtk_box_pack_start (GTK_BOX (preview), stack, TRUE, TRUE, 0);

    gtk_widget_show_all (stack);
    gtk_widget_hide (preview->text_scrolled);
    gtk_widget_hide (preview->meta_box);
}

GtkWidget *
nolphin_preview_new (void)
{
    return GTK_WIDGET (g_object_new (NOLPHIN_TYPE_PREVIEW, NULL));
}
