/* Prüft die Medien-Information (libnolphin-private/nolphin-media.c):
 * unterstützte MIME-Typen und – wenn pdfinfo installiert ist – das
 * Auslesen einer kleinen, selbst geschriebenen PDF-Datei. */

#include <gio/gio.h>
#include <stdlib.h>
#include <string.h>
#include <libnolphin-private/nolphin-media.h>

static gint exit_code = 0;
static GMainLoop *loop;
static GAsyncResult *pending;

static void
fail (const gchar *message)
{
	g_printerr ("FAIL: %s\n", message);
	exit_code = 1;
}

static void
on_ready (GObject *source, GAsyncResult *result, gpointer user_data)
{
	pending = g_object_ref (result);
	g_main_loop_quit (loop);
}

static const gchar minimal_pdf[] =
	"%PDF-1.4\n"
	"1 0 obj<</Type/Catalog/Pages 2 0 R>>endobj\n"
	"2 0 obj<</Type/Pages/Kids[3 0 R]/Count 1>>endobj\n"
	"3 0 obj<</Type/Page/Parent 2 0 R/MediaBox[0 0 595 842]>>endobj\n"
	"trailer<</Root 1 0 R/Size 4>>\n"
	"%%EOF\n";

int
main (int argc, char **argv)
{
	gchar *dir, *path, *cmd;
	GFile *file;
	GError *error = NULL;
	NolphinMediaInfo *info;
	gboolean seitenzahl = FALSE;
	guint i;

	loop = g_main_loop_new (NULL, FALSE);

	if (!nolphin_media_is_supported ("application/pdf") ||
	    !nolphin_media_is_supported ("video/mp4") ||
	    !nolphin_media_is_supported ("audio/ogg")) {
		fail ("PDF, Video und Audio müssen unterstützt sein");
	}
	if (nolphin_media_is_supported ("text/plain") || nolphin_media_is_supported (NULL)) {
		fail ("Text und NULL dürfen nicht unterstützt sein");
	}

	if (g_find_program_in_path ("pdfinfo") == NULL) {
		g_print ("SKIP: pdfinfo nicht installiert\n");
		return exit_code;
	}

	dir = g_dir_make_tmp ("nolphin-media-XXXXXX", &error);
	if (dir == NULL) {
		g_printerr ("FAIL: temp dir: %s\n", error->message);
		return 1;
	}
	path = g_build_filename (dir, "test.pdf", NULL);
	g_file_set_contents (path, minimal_pdf, -1, NULL);
	file = g_file_new_for_path (path);

	nolphin_media_get_info_async (file, "application/pdf", NULL, on_ready, NULL);
	g_main_loop_run (loop);
	info = nolphin_media_get_info_finish (pending, &error);
	if (info == NULL) {
		g_printerr ("FAIL: Info: %s\n", error != NULL ? error->message : "?");
		exit_code = 1;
	} else {
		for (i = 0; i < info->rows->len; i++) {
			NolphinMediaRow *row = g_ptr_array_index (info->rows, i);
			if (row->value != NULL && strcmp (row->value, "1") == 0) {
				seitenzahl = TRUE;
			}
		}
		if (!seitenzahl) {
			fail ("PDF-Info enthält keine Seitenzahl 1");
		}
		nolphin_media_info_free (info);
	}
	g_object_unref (pending);

	cmd = g_strdup_printf ("rm -rf '%s'", dir);
	if (system (cmd) != 0) {
		g_printerr ("Hinweis: Aufräumen von %s fehlgeschlagen\n", dir);
	}
	g_free (cmd);
	return exit_code;
}
