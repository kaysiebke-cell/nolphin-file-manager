/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nolphin-file-chooser-preview.c: Vorschau rechts in Dateiauswahldialogen
 */

#include <config.h>

#include <glib/gi18n.h>
#include <string.h>

#include "nolphin-file-chooser-preview.h"

#define PREVIEW_SIZE       256
#define PREVIEW_WIDTH      280
#define IMAGE_MAX_BYTES    (24 * 1024 * 1024)
#define TEXT_PEEK_BYTES    2048
#define TEXT_MAX_LINES     14

typedef struct {
	GtkWidget *image;
	GtkWidget *text;
	GtkWidget *name;
	GtkWidget *details;
} PreviewWidgets;

static void
preview_widgets_free (PreviewWidgets *w)
{
	g_free (w);
}

static GdkPixbuf *
load_scaled (const gchar *path)
{
	GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file_at_size (path, PREVIEW_SIZE, PREVIEW_SIZE, NULL);

	if (pixbuf != NULL) {
		GdkPixbuf *rotated = gdk_pixbuf_apply_embedded_orientation (pixbuf);

		if (rotated != NULL) {
			g_object_unref (pixbuf);
			pixbuf = rotated;
		}
	}
	return pixbuf;
}

/* Liest den Anfang einer Textdatei; NULL, wenn sie nicht als Text lesbar ist. */
static gchar *
read_text_peek (const gchar *path)
{
	gchar buffer[TEXT_PEEK_BYTES + 1];
	FILE *f = fopen (path, "rb");
	gsize n;
	gchar **lines;
	GString *out;
	gint i;

	if (f == NULL) {
		return NULL;
	}
	n = fread (buffer, 1, TEXT_PEEK_BYTES, f);
	fclose (f);
	buffer[n] = '\0';

	if (n == 0 || memchr (buffer, '\0', n) != NULL) {
		return NULL;
	}
	if (!g_utf8_validate (buffer, n, NULL)) {
		/* am Ende kann ein Zeichen abgeschnitten sein: bis zum letzten Zeilenende kürzen */
		gchar *nl = g_strrstr_len (buffer, n, "\n");

		if (nl == NULL) {
			return NULL;
		}
		*nl = '\0';
		if (!g_utf8_validate (buffer, -1, NULL)) {
			return NULL;
		}
	}

	lines = g_strsplit (buffer, "\n", -1);
	out = g_string_new (NULL);
	for (i = 0; lines[i] != NULL && i < TEXT_MAX_LINES; i++) {
		gchar *line = g_strdup (lines[i]);

		if (g_utf8_strlen (line, -1) > 60) {
			gchar *end = g_utf8_offset_to_pointer (line, 60);

			*end = '\0';
		}
		g_string_append (out, line);
		g_string_append_c (out, '\n');
		g_free (line);
	}
	g_strfreev (lines);
	return g_string_free (out, FALSE);
}

static void
clear_preview (PreviewWidgets *w)
{
	gtk_image_clear (GTK_IMAGE (w->image));
	gtk_label_set_text (GTK_LABEL (w->text), "");
	gtk_label_set_text (GTK_LABEL (w->name), "");
	gtk_label_set_text (GTK_LABEL (w->details), "");
	gtk_widget_hide (w->text);
}

static void
on_update_preview (GtkFileChooser *chooser, gpointer user_data)
{
	PreviewWidgets *w = user_data;
	GFile *file = gtk_file_chooser_get_preview_file (chooser);
	GFileInfo *info;
	const gchar *ctype;
	gchar *path, *size_text, *date_text, *details;
	GDateTime *mtime;
	GdkPixbuf *pixbuf = NULL;
	gchar *snippet = NULL;
	goffset size;
	gboolean is_dir;

	clear_preview (w);

	if (file == NULL) {
		gtk_file_chooser_set_preview_widget_active (chooser, FALSE);
		return;
	}

	info = g_file_query_info (file,
				  G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME ","
				  G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE ","
				  G_FILE_ATTRIBUTE_STANDARD_SIZE ","
				  G_FILE_ATTRIBUTE_STANDARD_TYPE ","
				  G_FILE_ATTRIBUTE_STANDARD_ICON ","
				  G_FILE_ATTRIBUTE_THUMBNAIL_PATH ","
				  G_FILE_ATTRIBUTE_TIME_MODIFIED,
				  G_FILE_QUERY_INFO_NONE, NULL, NULL);
	if (info == NULL) {
		gtk_file_chooser_set_preview_widget_active (chooser, FALSE);
		g_object_unref (file);
		return;
	}

	gtk_file_chooser_set_preview_widget_active (chooser, TRUE);
	path = g_file_get_path (file);
	ctype = g_file_info_get_content_type (info);
	size = g_file_info_get_size (info);
	is_dir = g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY;

	if (!is_dir && path != NULL && ctype != NULL) {
		if (g_str_has_prefix (ctype, "image/") && size <= IMAGE_MAX_BYTES) {
			pixbuf = load_scaled (path);
		}
		if (pixbuf == NULL) {
			const gchar *thumb = g_file_info_get_attribute_byte_string (info, G_FILE_ATTRIBUTE_THUMBNAIL_PATH);

			if (thumb != NULL) {
				pixbuf = load_scaled (thumb);
			}
		}
		if (pixbuf == NULL && g_content_type_is_a (ctype, "text/plain")) {
			snippet = read_text_peek (path);
		}
	}

	if (pixbuf != NULL) {
		gtk_image_set_from_pixbuf (GTK_IMAGE (w->image), pixbuf);
		g_object_unref (pixbuf);
	} else if (snippet != NULL) {
		gtk_label_set_text (GTK_LABEL (w->text), snippet);
		gtk_widget_show (w->text);
	} else {
		GIcon *icon = g_file_info_get_icon (info);

		if (icon != NULL) {
			gtk_image_set_from_gicon (GTK_IMAGE (w->image), icon, GTK_ICON_SIZE_DIALOG);
			gtk_image_set_pixel_size (GTK_IMAGE (w->image), 96);
		}
	}

	gtk_label_set_text (GTK_LABEL (w->name), g_file_info_get_display_name (info));

	{
		gchar *type_desc = ctype != NULL ? g_content_type_get_description (ctype) : NULL;

		size_text = is_dir ? g_strdup (_("Ordner")) : g_format_size (size);
		mtime = g_file_info_get_modification_date_time (info);
		date_text = mtime != NULL ? g_date_time_format (mtime, "%d.%m.%Y %H:%M") : g_strdup ("");
		details = g_strdup_printf ("%s\n%s\n%s", type_desc != NULL ? type_desc : "", size_text, date_text);
		gtk_label_set_text (GTK_LABEL (w->details), details);
		g_free (type_desc);
	}

	g_free (details);
	g_free (size_text);
	g_free (date_text);
	if (mtime != NULL) {
		g_date_time_unref (mtime);
	}
	g_free (snippet);
	g_free (path);
	g_object_unref (info);
	g_object_unref (file);
}

void
nolphin_file_chooser_add_preview (GtkFileChooser *chooser)
{
	PreviewWidgets *w;
	GtkWidget *box;

	g_return_if_fail (GTK_IS_FILE_CHOOSER (chooser));

	w = g_new0 (PreviewWidgets, 1);

	box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
	gtk_widget_set_size_request (box, PREVIEW_WIDTH, -1);
	gtk_widget_set_margin_start (box, 12);
	gtk_widget_set_margin_end (box, 12);
	gtk_widget_set_margin_top (box, 12);

	w->image = gtk_image_new ();
	gtk_box_pack_start (GTK_BOX (box), w->image, FALSE, FALSE, 0);

	w->name = gtk_label_new ("");
	gtk_label_set_line_wrap (GTK_LABEL (w->name), TRUE);
	gtk_label_set_line_wrap_mode (GTK_LABEL (w->name), PANGO_WRAP_WORD_CHAR);
	gtk_label_set_max_width_chars (GTK_LABEL (w->name), 28);
	gtk_label_set_justify (GTK_LABEL (w->name), GTK_JUSTIFY_CENTER);
	gtk_label_set_selectable (GTK_LABEL (w->name), FALSE);
	gtk_box_pack_start (GTK_BOX (box), w->name, FALSE, FALSE, 0);

	w->details = gtk_label_new ("");
	gtk_label_set_justify (GTK_LABEL (w->details), GTK_JUSTIFY_CENTER);
	gtk_style_context_add_class (gtk_widget_get_style_context (w->details), "dim-label");
	gtk_box_pack_start (GTK_BOX (box), w->details, FALSE, FALSE, 0);

	w->text = gtk_label_new ("");
	gtk_label_set_xalign (GTK_LABEL (w->text), 0.0);
	gtk_label_set_yalign (GTK_LABEL (w->text), 0.0);
	gtk_label_set_ellipsize (GTK_LABEL (w->text), PANGO_ELLIPSIZE_END);
	gtk_widget_set_no_show_all (w->text, TRUE);
	{
		PangoAttrList *attrs = pango_attr_list_new ();

		pango_attr_list_insert (attrs, pango_attr_family_new ("monospace"));
		pango_attr_list_insert (attrs, pango_attr_scale_new (0.85));
		gtk_label_set_attributes (GTK_LABEL (w->text), attrs);
		pango_attr_list_unref (attrs);
	}
	gtk_box_pack_start (GTK_BOX (box), w->text, FALSE, FALSE, 0);

	gtk_widget_show_all (box);
	gtk_widget_hide (w->text);

	gtk_file_chooser_set_preview_widget (chooser, box);
	gtk_file_chooser_set_use_preview_label (chooser, FALSE);
	g_object_set_data_full (G_OBJECT (chooser), "nolphin-preview-widgets", w, (GDestroyNotify) preview_widgets_free);
	g_signal_connect (chooser, "update-preview", G_CALLBACK (on_update_preview), w);
}
