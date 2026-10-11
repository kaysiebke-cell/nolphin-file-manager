/* Prüft nolphin-thumbnailer.c (eigene Vorschaubild-Erzeugung) gegen echte
 * Dateien: echte PNGs, ein echter Thumbnailer-Aufruf und der echte
 * Zwischenspeicher. XDG_CACHE_HOME und XDG_DATA_DIRS zeigen vorher auf einen
 * frischen Testordner (GLib merkt sich beides beim ersten Zugriff), damit der
 * echte ~/.cache/thumbnails des Benutzers nie berührt wird. */

#include <glib.h>
#include <glib/gstdio.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libnolphin-private/nolphin-thumbnailer.h>
#include <string.h>
#include <sys/stat.h>

static gint exit_code = 0;

static void
check (gboolean ok, const gchar *pass, const gchar *fail_msg)
{
	if (ok) {
		g_print ("PASS: %s\n", pass);
	} else {
		g_printerr ("FAIL: %s\n", fail_msg);
		exit_code = 1;
	}
}

static void
make_png (const gchar *path, int w, int h)
{
	GdkPixbuf *pb = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, w, h);
	gdk_pixbuf_fill (pb, 0x3366ccff);
	g_assert (gdk_pixbuf_save (pb, path, "png", NULL, NULL));
	g_object_unref (pb);
}

int
main (int argc, char **argv)
{
	GError *error = NULL;
	gchar *root = g_dir_make_tmp ("nolphin-thumbnailer-test-XXXXXX", &error);
	gchar *cache, *data, *tdir, *files, *script;
	g_assert (root != NULL);

	cache = g_build_filename (root, "cache", NULL);
	data = g_build_filename (root, "data", NULL);
	tdir = g_build_filename (data, "thumbnailers", NULL);
	files = g_build_filename (root, "files", NULL);
	g_mkdir_with_parents (tdir, 0700);
	g_mkdir_with_parents (files, 0700);
	g_setenv ("XDG_CACHE_HOME", cache, TRUE);
	g_setenv ("XDG_DATA_HOME", data, TRUE);
	/* /usr/share bleibt dabei: gdk-pixbuf braucht die MIME-Datenbank von dort */
	{
		gchar *dirs = g_strdup_printf ("%s:/usr/share", data);
		g_setenv ("XDG_DATA_DIRS", dirs, TRUE);
		g_free (dirs);
	}
	g_setenv ("NOLPHIN_THUMBNAILER_TIMEOUT", "2", TRUE);

	/* Test-Thumbnailer müssen vor dem ersten Aufruf vorhanden sein: das Modul
	 * liest die Ordner nur einmal ein. */
	script = g_build_filename (root, "fake-thumbnailer.sh", NULL);
	g_file_set_contents (script, "#!/bin/sh\ncp \"$1\" \"$2\"\n", -1, NULL);
	g_chmod (script, 0755);
	{
		gchar *entry = g_build_filename (tdir, "fake.thumbnailer", NULL);
		gchar *content = g_strdup_printf ("[Thumbnailer Entry]\nTryExec=%s\nExec=%s %%i %%o\nMimeType=application/x-fake;\n", script, script);
		g_file_set_contents (entry, content, -1, NULL);
		g_free (content);
		g_free (entry);
		entry = g_build_filename (tdir, "missing.thumbnailer", NULL);
		g_file_set_contents (entry, "[Thumbnailer Entry]\nTryExec=/gibt/es/nicht\nExec=/gibt/es/nicht %i %o\nMimeType=application/x-missing;\n", -1, NULL);
		g_free (entry);
	}

	/* --- Bild: Erkennung, Erzeugen, Skalieren --- */
	{
		gchar *png = g_build_filename (files, "bild.png", NULL);
		gchar *uri;
		GdkPixbuf *pb;
		make_png (png, 600, 300);
		uri = g_filename_to_uri (png, NULL, NULL);

		check (nolphin_thumbnailer_can_thumbnail (uri, "image/png", 1000),
		       "image/png gilt als thumbnail-fähig", "image/png nicht thumbnail-fähig");
		check (!nolphin_thumbnailer_can_thumbnail (uri, "text/plain", 1000),
		       "text/plain ohne Thumbnailer wird abgelehnt", "text/plain fälschlich thumbnail-fähig");

		pb = nolphin_thumbnailer_generate (uri, "image/png");
		check (pb != NULL && gdk_pixbuf_get_width (pb) == 256 && gdk_pixbuf_get_height (pb) == 128,
		       "600x300 wird auf 256x128 verkleinert (Seitenverhältnis erhalten)",
		       "Erzeugtes Vorschaubild hat nicht 256x128");

		/* --- Speichern im Zwischenspeicher, Format der Spezifikation --- */
		nolphin_thumbnailer_save (pb, uri, 1000);
		{
			gchar *path = nolphin_thumbnailer_path_for_uri (uri, FALSE);
			GdkPixbuf *saved = gdk_pixbuf_new_from_file (path, NULL);
			GStatBuf st;
			check (saved != NULL &&
			       g_strcmp0 (gdk_pixbuf_get_option (saved, "tEXt::Thumb::URI"), uri) == 0 &&
			       g_strcmp0 (gdk_pixbuf_get_option (saved, "tEXt::Thumb::MTime"), "1000") == 0,
			       "gespeichertes PNG trägt Thumb::URI und Thumb::MTime",
			       "gespeichertes PNG fehlt oder hat falsche Thumb::-Felder");
			check (g_stat (path, &st) == 0 && (st.st_mode & 0777) == 0600,
			       "Vorschaubild-Datei hat Rechte 0600", "Rechte der Vorschaubild-Datei nicht 0600");
			g_clear_object (&saved);
			g_free (path);
		}
		g_clear_object (&pb);

		/* --- Fehlschlag merken, aber nur für dieselbe Änderungszeit --- */
		nolphin_thumbnailer_create_failed (uri, 1000);
		check (!nolphin_thumbnailer_can_thumbnail (uri, "image/png", 1000),
		       "nach Fehlschlag wird dieselbe Datei nicht erneut versucht",
		       "Fehlschlag-Markierung wirkt nicht");
		check (nolphin_thumbnailer_can_thumbnail (uri, "image/png", 2000),
		       "geänderte Datei (neue mtime) wird wieder versucht",
		       "Fehlschlag gilt fälschlich auch für neue mtime");

		/* --- Vorschaubilder von Vorschaubildern: nein --- */
		{
			gchar *cached = nolphin_thumbnailer_path_for_uri (uri, FALSE);
			gchar *cached_uri = g_filename_to_uri (cached, NULL, NULL);
			check (!nolphin_thumbnailer_can_thumbnail (cached_uri, "image/png", 1000),
			       "Dateien im Vorschaubild-Ordner selbst werden nicht verkleinert",
			       "Datei im Zwischenspeicher fälschlich thumbnail-fähig");
			g_free (cached_uri);
			g_free (cached);
		}
		g_free (uri);
		g_free (png);
	}

	/* --- Thumbnailer-Programm: ohne Shell, auch bei bösen Dateinamen --- */
	{
		gchar *evil = g_build_filename (files, "a b;touch INJECTED.txt;$(touch INJECTED2).fake", NULL);
		gchar *injected = g_build_filename (files, "INJECTED.txt", NULL);
		gchar *uri;
		GdkPixbuf *pb;

		make_png (evil, 100, 50);
		uri = g_filename_to_uri (evil, NULL, NULL);

		check (nolphin_thumbnailer_can_thumbnail (uri, "application/x-fake", 5),
		       "Typ mit installiertem Thumbnailer ist thumbnail-fähig",
		       "installierter Thumbnailer wurde nicht erkannt");
		pb = nolphin_thumbnailer_generate (uri, "application/x-fake");
		check (pb != NULL && gdk_pixbuf_get_width (pb) == 100,
		       "Thumbnailer-Programm wurde gestartet und lieferte ein Bild",
		       "Thumbnailer lieferte kein Bild");
		check (!g_file_test (injected, G_FILE_TEST_EXISTS),
		       "Sonderzeichen im Dateinamen werden nicht als Befehl ausgeführt",
		       "Befehlseinschleusung über den Dateinamen war möglich!");
		g_clear_object (&pb);

		/* --- hängender Thumbnailer wird nach dem Zeitlimit beendet --- */
		g_file_set_contents (script, "#!/bin/sh\nsleep 60\n", -1, NULL);
		{
			gint64 start = g_get_monotonic_time ();
			pb = nolphin_thumbnailer_generate (uri, "application/x-fake");
			check (pb == NULL && (g_get_monotonic_time () - start) < 20 * G_USEC_PER_SEC,
			       "hängender Thumbnailer wird nach dem Zeitlimit abgebrochen",
			       "hängender Thumbnailer wurde nicht rechtzeitig beendet");
			g_clear_object (&pb);
		}

		/* --- Thumbnailer, dessen Programm fehlt, gilt nicht --- */
		check (!nolphin_thumbnailer_can_thumbnail (uri, "application/x-missing", 5),
		       "Thumbnailer mit fehlendem Programm gilt nicht als verfügbar",
		       "Thumbnailer ohne Programm fälschlich verfügbar");
		g_free (uri);
		g_free (injected);
		g_free (evil);
	}

	check (nolphin_thumbnailer_cache_ok (), "Zwischenspeicher-Prüfung meldet 'in Ordnung'",
	       "cache_ok meldet fälschlich ein Problem");

	if (exit_code == 0) {
		g_print ("All thumbnailer tests passed.\n");
	}
	return exit_code;
}
