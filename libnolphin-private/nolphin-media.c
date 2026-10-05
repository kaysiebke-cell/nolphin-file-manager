/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nolphin-media.c: erweiterte Vorschau für PDF, Video und Audio (§34)
 */

#include <config.h>

#include "nolphin-media.h"

#include <glib/gi18n.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Größere Dateien werden nicht analysiert (kein Blockieren, kein Speicherhunger). */
#define MEDIA_MAX_FILE_SIZE (2 * G_GINT64_CONSTANT (1024) * 1024 * 1024)

typedef enum { KIND_PDF, KIND_VIDEO, KIND_AUDIO } Kind;

typedef struct {
	gchar *path;
	gchar *uri;
	Kind kind;
} Job;

static void
job_free (Job *j)
{
	g_free (j->path);
	g_free (j->uri);
	g_free (j);
}

void
nolphin_media_info_free (NolphinMediaInfo *info)
{
	if (info == NULL) {
		return;
	}
	if (info->rows != NULL) {
		g_ptr_array_unref (info->rows);
	}
	g_clear_pointer (&info->thumb_png, g_bytes_unref);
	g_free (info->notice);
	g_free (info);
}

static void
row_free (NolphinMediaRow *r)
{
	g_free (r->label);
	g_free (r->value);
	g_free (r);
}

static void
add_row (GPtrArray *rows, const gchar *label, const gchar *value)
{
	NolphinMediaRow *r;

	if (value == NULL || value[0] == '\0') {
		return;
	}
	r = g_new0 (NolphinMediaRow, 1);
	r->label = g_strdup (label);
	r->value = g_strdup (value);
	g_ptr_array_add (rows, r);
}

static NolphinMediaInfo *
info_new (void)
{
	NolphinMediaInfo *info = g_new0 (NolphinMediaInfo, 1);

	info->rows = g_ptr_array_new_with_free_func ((GDestroyNotify) row_free);
	return info;
}

gboolean
nolphin_media_is_supported (const gchar *mime_type)
{
	return mime_type != NULL &&
	       (g_strcmp0 (mime_type, "application/pdf") == 0 ||
		g_str_has_prefix (mime_type, "video/") ||
		g_str_has_prefix (mime_type, "audio/"));
}

/* Führt ein Werkzeug aus (LC_ALL=C für stabile Ausgabe), Standardausgabe als Bytes. */
static GBytes *
run_capture (const gchar * const *argv, GCancellable *cancellable, GError **error)
{
	GSubprocessLauncher *launcher = g_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE |
								   G_SUBPROCESS_FLAGS_STDERR_SILENCE |
								   G_SUBPROCESS_FLAGS_STDIN_PIPE);
	GSubprocess *proc;
	GBytes *in, *out = NULL;

	g_subprocess_launcher_setenv (launcher, "LC_ALL", "C", TRUE);
	proc = g_subprocess_launcher_spawnv (launcher, argv, error);
	g_object_unref (launcher);
	if (proc == NULL) {
		return NULL;
	}

	in = g_bytes_new_static ("", 0);
	if (!g_subprocess_communicate (proc, in, cancellable, &out, NULL, error)) {
		g_bytes_unref (in);
		g_object_unref (proc);
		return NULL;
	}
	g_bytes_unref (in);

	if (!g_subprocess_get_successful (proc)) {
		g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "%s", _("Das Werkzeug meldete einen Fehler."));
		g_clear_pointer (&out, g_bytes_unref);
	}
	g_object_unref (proc);
	return out;
}

static gchar *
bytes_to_text (GBytes *b)
{
	gsize len = 0;
	const gchar *d = g_bytes_get_data (b, &len);

	return g_utf8_make_valid (d != NULL ? d : "", len);
}

static gboolean
tool_present (const gchar *tool)
{
	gchar *p = g_find_program_in_path (tool);
	gboolean ok = p != NULL;

	g_free (p);
	return ok;
}

/* --- PDF -------------------------------------------------------------------- */

static void
pdf_info (Job *job, NolphinMediaInfo *info, GCancellable *cancellable)
{
	GError *error = NULL;

	if (!tool_present ("pdfinfo")) {
		info->notice = g_strdup (_("PDF-Angaben: »pdfinfo« (Paket poppler-utils) ist nicht installiert."));
	} else {
		const gchar *argv[] = { "pdfinfo", job->path, NULL };
		GBytes *out = run_capture (argv, cancellable, &error);

		if (out == NULL) {
			info->notice = g_strdup_printf (_("PDF-Angaben nicht lesbar: %s"),
							error != NULL ? error->message : "");
			g_clear_error (&error);
		} else {
			gchar *text = bytes_to_text (out);
			gchar **lines = g_strsplit (text, "\n", -1);
			guint i;

			for (i = 0; lines[i] != NULL; i++) {
				gchar *colon = strchr (lines[i], ':');
				gchar *key, *val;

				if (colon == NULL) {
					continue;
				}
				key = g_strndup (lines[i], colon - lines[i]);
				val = g_strstrip (g_strdup (colon + 1));
				if (g_strcmp0 (key, "Title") == 0) {
					add_row (info->rows, _("Titel:"), val);
				} else if (g_strcmp0 (key, "Author") == 0) {
					add_row (info->rows, _("Autor:"), val);
				} else if (g_strcmp0 (key, "Pages") == 0) {
					add_row (info->rows, _("Seiten:"), val);
				} else if (g_strcmp0 (key, "Page size") == 0) {
					add_row (info->rows, _("Seitengröße:"), val);
				} else if (g_strcmp0 (key, "PDF version") == 0) {
					add_row (info->rows, _("PDF-Version:"), val);
				} else if (g_strcmp0 (key, "Encrypted") == 0 && g_str_has_prefix (val, "yes")) {
					add_row (info->rows, _("Verschlüsselt:"), _("Ja"));
				}
				g_free (key);
				g_free (val);
			}
			g_strfreev (lines);
			g_free (text);
			g_bytes_unref (out);
		}
	}

	if (!tool_present ("pdftocairo")) {
		if (info->notice == NULL) {
			info->notice = g_strdup (_("PDF-Vorschau: »pdftocairo« (Paket poppler-utils) ist nicht installiert."));
		}
	} else {
		const gchar *argv[] = { "pdftocairo", "-png", "-singlefile", "-f", "1", "-l", "1",
					"-scale-to", "256", job->path, "-", NULL };

		info->thumb_png = run_capture (argv, cancellable, NULL);
	}
}

/* --- Video / Audio ---------------------------------------------------------- */

/* "0:30:56.204000000" -> "0:30:56" */
static gchar *
short_duration (const gchar *d)
{
	const gchar *dot = strchr (d, '.');

	return dot != NULL ? g_strndup (d, dot - d) : g_strdup (d);
}

static void
gst_info (Job *job, NolphinMediaInfo *info, GCancellable *cancellable)
{
	GError *error = NULL;
	const gchar *argv[] = { "gst-discoverer-1.0", "-t", "8", job->uri, NULL };
	GBytes *out;
	gchar *text, **lines;
	guint i;
	gchar stream = 0;     /* 'v' | 'a' | 0 */
	gboolean in_tags = FALSE;
	gint width = 0, height = 0;

	if (!tool_present ("gst-discoverer-1.0")) {
		info->notice = g_strdup (_("Medien-Angaben: »gst-discoverer-1.0« (Paket gstreamer1.0-tools) ist nicht installiert."));
		return;
	}

	out = run_capture (argv, cancellable, &error);
	if (out == NULL) {
		info->notice = g_strdup_printf (_("Medien-Angaben nicht lesbar: %s"), error != NULL ? error->message : "");
		g_clear_error (&error);
		return;
	}

	text = bytes_to_text (out);
	lines = g_strsplit (text, "\n", -1);
	for (i = 0; lines[i] != NULL; i++) {
		gchar *line = g_strstrip (g_strdup (lines[i]));

		if (g_str_has_prefix (line, "Duration: ")) {
			gchar *d = short_duration (line + 10);

			add_row (info->rows, _("Dauer:"), d);
			g_free (d);
		} else if (g_str_has_prefix (line, "container #")) {
			const gchar *c = strstr (line, ": ");

			stream = 0;
			in_tags = FALSE;
			if (c != NULL) {
				add_row (info->rows, _("Container:"), c + 2);
			}
		} else if (g_str_has_prefix (line, "video #") || g_str_has_prefix (line, "audio #")) {
			const gchar *c = strstr (line, ": ");

			stream = line[0] == 'v' ? 'v' : 'a';
			in_tags = FALSE;
			width = height = 0;
			if (c != NULL) {
				add_row (info->rows, stream == 'v' ? _("Videocodec:") : _("Audiocodec:"), c + 2);
			}
		} else if (g_str_has_prefix (line, "Tags:")) {
			in_tags = TRUE;
		} else if (stream == 'v' && g_str_has_prefix (line, "Width: ")) {
			width = atoi (line + 7);
		} else if (stream == 'v' && g_str_has_prefix (line, "Height: ")) {
			height = atoi (line + 8);
			if (width > 0 && height > 0) {
				gchar *res = g_strdup_printf ("%d × %d", width, height);

				add_row (info->rows, _("Auflösung:"), res);
				g_free (res);
			}
		} else if (stream == 'v' && g_str_has_prefix (line, "Frame rate: ")) {
			gint num = 0, den = 1;

			if (sscanf (line + 12, "%d/%d", &num, &den) >= 1 && den > 0 && num > 0) {
				gchar *fr = g_strdup_printf (_("%.2f Bilder/s"), (double) num / den);

				add_row (info->rows, _("Bildrate:"), fr);
				g_free (fr);
			}
		} else if (stream == 'a' && g_str_has_prefix (line, "Channels: ")) {
			add_row (info->rows, _("Kanäle:"), (gchar[2]) { line[10], 0 });
		} else if (stream == 'a' && g_str_has_prefix (line, "Sample rate: ")) {
			gchar *sr = g_strdup_printf (_("%s Hz"), line + 13);

			add_row (info->rows, _("Abtastrate:"), sr);
			g_free (sr);
		} else if (in_tags) {
			const gchar *c = strstr (line, ": ");

			if (c != NULL) {
				gchar *key = g_strndup (line, c - line);

				if (g_strcmp0 (key, "title") == 0) {
					add_row (info->rows, _("Titel:"), c + 2);
				} else if (g_strcmp0 (key, "artist") == 0) {
					add_row (info->rows, _("Künstler:"), c + 2);
				} else if (g_strcmp0 (key, "album") == 0) {
					add_row (info->rows, _("Album:"), c + 2);
				} else if (g_strcmp0 (key, "genre") == 0) {
					add_row (info->rows, _("Genre:"), c + 2);
				}
				g_free (key);
			}
		}
		g_free (line);
	}
	g_strfreev (lines);
	g_free (text);
	g_bytes_unref (out);
}

/* --- Thread-Einstieg ------------------------------------------------------------ */

static void
media_thread (GTask *task, gpointer source, gpointer task_data, GCancellable *cancellable)
{
	Job *job = task_data;
	NolphinMediaInfo *info = info_new ();

	if (job->kind == KIND_PDF) {
		pdf_info (job, info, cancellable);
	} else {
		gst_info (job, info, cancellable);
	}

	if (g_cancellable_is_cancelled (cancellable)) {
		nolphin_media_info_free (info);
		g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_CANCELLED, "%s", _("Abgebrochen."));
		return;
	}
	g_task_return_pointer (task, info, (GDestroyNotify) nolphin_media_info_free);
}

void
nolphin_media_get_info_async (GFile *location, const gchar *mime_type, GCancellable *cancellable,
			      GAsyncReadyCallback callback, gpointer user_data)
{
	GTask *task = g_task_new (NULL, cancellable, callback, user_data);
	Job *job;
	GFileInfo *finfo;

	if (!nolphin_media_is_supported (mime_type) || !g_file_is_native (location)) {
		g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED, "%s",
					 _("Für diese Datei gibt es keine erweiterte Vorschau."));
		g_object_unref (task);
		return;
	}

	finfo = g_file_query_info (location, G_FILE_ATTRIBUTE_STANDARD_SIZE, G_FILE_QUERY_INFO_NONE, NULL, NULL);
	if (finfo != NULL && g_file_info_get_size (finfo) > MEDIA_MAX_FILE_SIZE) {
		g_object_unref (finfo);
		g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED, "%s", _("Die Datei ist zu groß für die Vorschau."));
		g_object_unref (task);
		return;
	}
	g_clear_object (&finfo);

	job = g_new0 (Job, 1);
	job->path = g_file_get_path (location);
	job->uri = g_file_get_uri (location);
	job->kind = g_strcmp0 (mime_type, "application/pdf") == 0 ? KIND_PDF
		    : (g_str_has_prefix (mime_type, "video/") ? KIND_VIDEO : KIND_AUDIO);
	g_task_set_task_data (task, job, (GDestroyNotify) job_free);
	g_task_run_in_thread (task, media_thread);
	g_object_unref (task);
}

NolphinMediaInfo *
nolphin_media_get_info_finish (GAsyncResult *result, GError **error)
{
	return g_task_propagate_pointer (G_TASK (result), error);
}
