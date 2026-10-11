/* Prüft den Handy-Austausch: den HTTP-Server (src/nolphin-phone-server.c)
 * mit echten Verbindungen über 127.0.0.1 und den QR-Encoder
 * (src/nolphin-qr.c). */

#include <gio/gio.h>
#include <glib/gstdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../src/nolphin-phone-server.h"
#include "../src/nolphin-qr.h"

static gint exit_code = 0;

static void
check (gboolean ok, const gchar *message)
{
	if (!ok) {
		g_printerr ("FAIL: %s\n", message);
		exit_code = 1;
	}
}

static GMutex received_lock;
static GPtrArray *received_names;

static void
on_received (const gchar *name, goffset size, gpointer data)
{
	(void) size;
	(void) data;
	g_mutex_lock (&received_lock);
	g_ptr_array_add (received_names, g_strdup (name));
	g_mutex_unlock (&received_lock);
}

/* Schickt @request (Kopf) und @body und gibt die komplette Antwort zurück. */
static gchar *
http (guint16 port, const gchar *head, const gchar *body, gsize body_len)
{
	GSocketClient *client = g_socket_client_new ();
	GSocketConnection *conn;
	GString *answer = g_string_new (NULL);
	gchar buf[8192];
	gssize n;

	g_socket_client_set_timeout (client, 10);
	conn = g_socket_client_connect_to_host (client, "127.0.0.1", port, NULL, NULL);
	g_object_unref (client);
	if (conn == NULL) {
		g_string_free (answer, TRUE);
		return NULL;
	}
	g_output_stream_write_all (g_io_stream_get_output_stream (G_IO_STREAM (conn)), head, strlen (head), NULL, NULL, NULL);
	if (body_len > 0) {
		g_output_stream_write_all (g_io_stream_get_output_stream (G_IO_STREAM (conn)), body, body_len, NULL, NULL, NULL);
	}
	while ((n = g_input_stream_read (g_io_stream_get_input_stream (G_IO_STREAM (conn)), buf, sizeof buf, NULL, NULL)) > 0) {
		g_string_append_len (answer, buf, n);
	}
	g_object_unref (conn);
	return g_string_free (answer, FALSE);
}

static gint
status_of (const gchar *answer)
{
	return answer != NULL && g_str_has_prefix (answer, "HTTP/1.1 ") ? atoi (answer + 9) : 0;
}

static gchar *
get (guint16 port, const gchar *target)
{
	gchar *req = g_strdup_printf ("GET %s HTTP/1.1\r\nHost: x\r\n\r\n", target);
	gchar *r = http (port, req, NULL, 0);

	g_free (req);
	return r;
}

static gchar *
post (guint16 port, const gchar *target, const gchar *body, gsize len)
{
	gchar *req = g_strdup_printf ("POST %s HTTP/1.1\r\nHost: x\r\nContent-Length: %" G_GSIZE_FORMAT "\r\n\r\n", target, len);
	gchar *r = http (port, req, body, len);

	g_free (req);
	return r;
}

static gboolean
file_has (const gchar *dir, const gchar *name, const gchar *expected)
{
	gchar *path = g_build_filename (dir, name, NULL);
	gchar *contents = NULL;
	gboolean ok = g_file_get_contents (path, &contents, NULL, NULL) && strcmp (contents, expected) == 0;

	g_free (path);
	g_free (contents);
	return ok;
}

static gboolean
dir_is_empty (const gchar *path)
{
	GDir *d = g_dir_open (path, 0, NULL);
	gboolean empty = d == NULL || g_dir_read_name (d) == NULL;

	if (d != NULL) {
		g_dir_close (d);
	}
	return empty;
}

/* legt kurz nach dem Aufruf eine Datei im Freigabeordner an */
static gpointer
write_later (gpointer share_dir)
{
	gchar *path = g_build_filename ((const gchar *) share_dir, "spaeter.txt", NULL);

	g_usleep (300 * 1000);
	g_file_set_contents (path, "spaeter", -1, NULL);
	g_free (path);
	return NULL;
}

static void
test_sanitize (void)
{
	gchar *s;

	s = nolphin_phone_sanitize_name ("../../etc/passwd");
	check (strcmp (s, "passwd") == 0, "sanitize: Pfad wird auf den Dateinamen gekürzt");
	g_free (s);
	s = nolphin_phone_sanitize_name ("C:\\Users\\x\\bild.jpg");
	check (strcmp (s, "bild.jpg") == 0, "sanitize: Windows-Pfad");
	g_free (s);
	s = nolphin_phone_sanitize_name (".bashrc");
	check (strcmp (s, "bashrc") == 0, "sanitize: führender Punkt entfernt");
	g_free (s);
	s = nolphin_phone_sanitize_name ("a\nb\tc.txt");
	check (strcmp (s, "abc.txt") == 0, "sanitize: Steuerzeichen entfernt");
	g_free (s);
	s = nolphin_phone_sanitize_name ("..");
	check (strcmp (s, "Datei") == 0, "sanitize: leerer Name wird ersetzt");
	g_free (s);
	s = nolphin_phone_sanitize_name (NULL);
	check (strcmp (s, "Datei") == 0, "sanitize: NULL");
	g_free (s);
	{
		gchar *longname = g_strnfill (300, 'a');
		gchar *umlauts = g_strnfill (150, 'x');
		gchar *mixed = g_strconcat (umlauts, "ä", "ä", "ä", NULL);

		s = nolphin_phone_sanitize_name (longname);
		check (strlen (s) == 200, "sanitize: auf 200 Bytes gekürzt");
		g_free (s);
		s = nolphin_phone_sanitize_name (mixed);
		check (g_utf8_validate (s, -1, NULL), "sanitize: Kürzung lässt gültiges UTF-8");
		g_free (s);
		g_free (longname);
		g_free (umlauts);
		g_free (mixed);
	}
	s = nolphin_phone_sanitize_name ("\xff\xfe.txt");
	check (g_utf8_validate (s, -1, NULL), "sanitize: ungültiges UTF-8 wird repariert");
	g_free (s);
}

static void
test_qr (void)
{
	NolphinQr qr;
	gchar *big = g_strnfill (107, 'a');

	check (nolphin_qr_encode ("12345678901234", -1, &qr) && qr.size == 21, "qr: 14 Bytes ergeben Version 1");
	check (nolphin_qr_encode ("123456789012345", -1, &qr) && qr.size == 25, "qr: 15 Bytes ergeben Version 2");
	check (nolphin_qr_get (&qr, 0, 0) && nolphin_qr_get (&qr, 3, 3) && !nolphin_qr_get (&qr, 1, 1),
	       "qr: Suchmuster oben links");
	check (nolphin_qr_get (&qr, qr.size - 1, 0) && nolphin_qr_get (&qr, 0, qr.size - 1),
	       "qr: Suchmuster in den anderen Ecken");
	check (nolphin_qr_encode ("http://192.168.178.20:45678/?t=0123456789abcdef0123456789abcdef", -1, &qr) && qr.size == 37,
	       "qr: Verbindungslink ergibt Version 5");
	check (!nolphin_qr_encode (big, -1, &qr), "qr: zu langer Text wird abgelehnt");
	g_free (big);
}

static GMainLoop *loop;

/* GSocketService nimmt Verbindungen in der Hauptschleife an. Deshalb läuft die
 * Hauptschleife im Hauptthread und alle Prüfungen in diesem Arbeitsthread. */
static gpointer
run_tests (gpointer unused)
{
	(void) unused;
	gchar *root = g_dir_make_tmp ("nolphin-phone-test-XXXXXX", NULL);
	gchar *inbox = g_build_filename (root, "eingang", NULL);
	gchar *share = g_build_filename (root, "frei", NULL);
	GFile *inbox_file = g_file_new_for_path (inbox);
	GFile *share_file = g_file_new_for_path (share);
	NolphinPhoneServer *server;
	GError *error = NULL;
	guint16 port;
	gchar *r, *target, *token_q;
	const gchar *token;
	gchar *token_owned;
	gchar *big;
	gsize i;

	received_names = g_ptr_array_new_with_free_func (g_free);
	test_sanitize ();
	test_qr ();

	g_mkdir_with_parents (share, 0755);
	{
		gchar *p = g_build_filename (share, "a.txt", NULL);
		gchar *hidden = g_build_filename (share, ".versteckt", NULL);
		gchar *link = g_build_filename (share, "link", NULL);

		g_file_set_contents (p, "hallo vom pc", -1, NULL);
		g_file_set_contents (hidden, "geheim", -1, NULL);
		check (symlink ("/etc/passwd", link) == 0, "Testvorbereitung: Symlink");
		{
			gchar *sub = g_build_filename (share, "Unter ordner", NULL);
			gchar *b = g_build_filename (sub, "b.txt", NULL);
			gchar *dirlink = g_build_filename (share, "etclink", NULL);

			g_mkdir_with_parents (sub, 0755);
			g_file_set_contents (b, "hallo aus dem Unterordner", -1, NULL);
			check (symlink ("/etc", dirlink) == 0, "Testvorbereitung: Ordner-Symlink");
			g_free (sub);
			g_free (b);
			g_free (dirlink);
		}
		g_free (p);
		g_free (hidden);
		g_free (link);
	}

	check (nolphin_phone_server_start ("kein-ip", inbox_file, NULL, NULL, NULL, NULL, NULL, &error) == NULL && error != NULL,
	       "ungültige Adresse wird abgelehnt");
	g_clear_error (&error);

	server = nolphin_phone_server_start ("127.0.0.1", inbox_file, share_file, on_received, NULL, NULL, NULL, &error);
	if (server == NULL) {
		g_printerr ("FAIL: Server startet nicht: %s\n", error->message);
		exit_code = 1;
		g_main_loop_quit (loop);
		return NULL;
	}
	port = nolphin_phone_server_get_port (server);
	token_owned = nolphin_phone_server_dup_token (server);
	token = token_owned;
	check (port > 0 && strlen (token) == 32, "Port und Schlüssel vorhanden");
	check (g_file_query_exists (inbox_file, NULL), "Eingangsordner wurde angelegt");
	token_q = g_strdup_printf ("t=%s", token);

	/* Zugriffsschutz */
	r = get (port, "/");
	check (status_of (r) == 403, "ohne Schlüssel: 403");
	g_free (r);
	r = get (port, "/?t=falsch");
	check (status_of (r) == 403, "falscher Schlüssel: 403");
	g_free (r);
	target = g_strdup_printf ("/?%s", token_q);
	r = get (port, target);
	check (status_of (r) == 200 && strstr (r, "<script>") != NULL && strstr (r, "@@") == NULL,
	       "Startseite mit Schlüssel: 200, ohne offene Platzhalter");
	check (strstr (r, "Content-Security-Policy") != NULL, "Startseite mit Content-Security-Policy");
	g_free (r);
	g_free (target);
	r = post (port, "/up?t=falsch&n=x.txt", "x", 1);
	check (status_of (r) == 403, "Upload mit falschem Schlüssel: 403");
	g_free (r);
	check (dir_is_empty (inbox), "nichts im Eingangsordner nach verweigertem Upload");
	r = get (port, "/anything?x=1");
	check (status_of (r) == 403, "unbekannter Pfad ohne Schlüssel: 403");
	g_free (r);
	target = g_strdup_printf ("/nichts?%s", token_q);
	r = get (port, target);
	check (status_of (r) == 404, "unbekannter Pfad mit Schlüssel: 404");
	g_free (r);
	g_free (target);

	/* Upload */
	target = g_strdup_printf ("/up?%s&n=hello.txt", token_q);
	r = post (port, target, "abc", 3);
	check (status_of (r) == 200 && strstr (r, "\"hello.txt\"") != NULL, "Upload: 200 und Name");
	g_free (r);
	check (file_has (inbox, "hello.txt", "abc"), "Upload: Inhalt gespeichert");
	r = post (port, target, "zwei", 4);
	check (status_of (r) == 200 && strstr (r, "hello (1).txt") != NULL, "gleicher Name: nummeriert, nichts überschrieben");
	g_free (r);
	check (file_has (inbox, "hello.txt", "abc") && file_has (inbox, "hello (1).txt", "zwei"), "beide Dateien vorhanden");
	g_free (target);

	target = g_strdup_printf ("/up?%s&n=%s", token_q, "..%2F..%2Fevil.txt");
	r = post (port, target, "boese", 5);
	check (status_of (r) == 200, "Pfadname: angenommen");
	g_free (r);
	g_free (target);
	{
		gchar *outside = g_build_filename (root, "evil.txt", NULL);

		check (!g_file_test (outside, G_FILE_TEST_EXISTS), "Pfadtrick landet nicht außerhalb des Eingangsordners");
		g_free (outside);
	}
	check (file_has (inbox, "evil.txt", "boese"), "Pfadtrick: nur der Dateiname wird verwendet");

	target = g_strdup_printf ("/up?%s&n=%s", token_q, "%C3%A4%20%C3%BC.jpg");
	r = post (port, target, "bild", 4);
	check (status_of (r) == 200 && file_has (inbox, "\xc3\xa4 \xc3\xbc.jpg", "bild"), "Umlaute im Dateinamen");
	g_free (r);
	g_free (target);

	/* größerer Upload über mehrere Lesevorgänge */
	big = g_malloc (3 * 1024 * 1024);
	for (i = 0; i < 3 * 1024 * 1024; i++) {
		big[i] = (gchar) ('a' + (i * 7) % 26);
	}
	target = g_strdup_printf ("/up?%s&n=gross.bin", token_q);
	r = post (port, target, big, 3 * 1024 * 1024);
	check (status_of (r) == 200, "großer Upload: 200");
	g_free (r);
	g_free (target);
	{
		gchar *path = g_build_filename (inbox, "gross.bin", NULL);
		gchar *contents = NULL;
		gsize len = 0;

		check (g_file_get_contents (path, &contents, &len, NULL) && len == 3 * 1024 * 1024 && memcmp (contents, big, len) == 0,
		       "großer Upload: Inhalt unverändert");
		g_free (path);
		g_free (contents);
	}
	g_free (big);

	/* fehlerhafte Uploads */
	target = g_strdup_printf ("POST /up?%s&n=a HTTP/1.1\r\nHost: x\r\n\r\n", token_q);
	r = http (port, target, NULL, 0);
	check (status_of (r) == 411, "Upload ohne Content-Length: 411");
	g_free (r);
	g_free (target);
	target = g_strdup_printf ("POST /up?%s&n=a HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: chunked\r\n\r\n", token_q);
	r = http (port, target, NULL, 0);
	check (status_of (r) == 501, "Upload mit chunked: 501");
	g_free (r);
	g_free (target);
	target = g_strdup_printf ("POST /up?%s&n=a HTTP/1.1\r\nHost: x\r\nContent-Length: 99999999999\r\n\r\n", token_q);
	r = http (port, target, NULL, 0);
	check (status_of (r) == 413, "zu große Datei: 413");
	g_free (r);
	g_free (target);
	r = http (port, "GARBAGE\r\n\r\n", NULL, 0);
	check (status_of (r) == 400, "kaputte Anfrage: 400");
	g_free (r);
	{
		gchar *huge = g_strnfill (20000, 'x');
		gchar *req = g_strdup_printf ("GET /?%s&x=%s HTTP/1.1\r\n\r\n", token_q, huge);

		r = http (port, req, NULL, 0);
		check (r == NULL || status_of (r) == 400 || status_of (r) == 0, "zu langer Kopf wird abgelehnt");
		g_free (r);
		g_free (req);
		g_free (huge);
	}
	{
		/* Abbruch mitten im Upload hinterlässt keine Teildatei */
		GSocketClient *client = g_socket_client_new ();
		GSocketConnection *conn = g_socket_client_connect_to_host (client, "127.0.0.1", port, NULL, NULL);
		gchar *req = g_strdup_printf ("POST /up?%s&n=abbruch.bin HTTP/1.1\r\nContent-Length: 1000\r\n\r\nnur-ein-teil", token_q);

		g_output_stream_write_all (g_io_stream_get_output_stream (G_IO_STREAM (conn)), req, strlen (req), NULL, NULL, NULL);
		g_io_stream_close (G_IO_STREAM (conn), NULL, NULL);
		g_object_unref (conn);
		g_object_unref (client);
		g_free (req);
		g_usleep (500 * 1000);
		{
			gchar *p1 = g_build_filename (inbox, "abbruch.bin", NULL);
			GDir *d = g_dir_open (inbox, 0, NULL);
			const gchar *n;
			gboolean temp_left = FALSE;

			check (!g_file_test (p1, G_FILE_TEST_EXISTS), "abgebrochener Upload: keine Datei");
			while (d != NULL && (n = g_dir_read_name (d)) != NULL) {
				if (g_str_has_prefix (n, ".nolphin-")) {
					temp_left = TRUE;
				}
			}
			if (d != NULL) {
				g_dir_close (d);
			}
			check (!temp_left, "abgebrochener Upload: keine Teildatei übrig");
			g_free (p1);
		}
	}

	/* Dateien im Schreiben (frisch geändert) erscheinen erst nach ein paar Sekunden */
	{
		gchar *fresh = g_build_filename (share, "frisch.txt", NULL);

		g_file_set_contents (fresh, "x", -1, NULL);
		g_free (fresh);
	}
	target = g_strdup_printf ("/ls?%s", token_q);
	r = get (port, target);
	check (status_of (r) == 200 && strstr (r, "frisch.txt") == NULL, "frisch geschriebene Datei ist noch nicht in der Liste");
	g_free (r);
	g_free (target);
	g_usleep (3500 * 1000);

	/* Freigabeordner */
	target = g_strdup_printf ("/ls?%s", token_q);
	r = get (port, target);
	check (status_of (r) == 200 && strstr (r, "a.txt") != NULL && strstr (r, "frisch.txt") != NULL, "Liste enthält a.txt und die gealterte Datei");
	check (strstr (r, "link") == NULL && strstr (r, "versteckt") == NULL, "Liste ohne Symlink und versteckte Datei");
	g_free (r);
	g_free (target);
	target = g_strdup_printf ("/dl?%s&n=a.txt", token_q);
	r = get (port, target);
	check (status_of (r) == 200 && g_str_has_suffix (r, "hallo vom pc") && strstr (r, "application/octet-stream") != NULL &&
	       strstr (r, "attachment") != NULL, "Download: Inhalt, Typ und attachment");
	g_free (r);
	g_free (target);
	{
		const gchar *bad[] = { "link", "..%2Fa.txt", ".versteckt", "%2Fetc%2Fpasswd", "fehlt.txt", NULL };

		for (i = 0; bad[i] != NULL; i++) {
			gchar *msg = g_strdup_printf ("Download von '%s' wird abgelehnt (404)", bad[i]);

			target = g_strdup_printf ("/dl?%s&n=%s", token_q, bad[i]);
			r = get (port, target);
			check (status_of (r) == 404 && strstr (r, "root:") == NULL, msg);
			g_free (r);
			g_free (target);
			g_free (msg);
		}
	}

	/* Ordneransicht: /dir und /dl mit Unterordner, ohne Ausbruch aus dem Ausgangsordner */
	target = g_strdup_printf ("/dir?%s", token_q);
	r = get (port, target);
	check (status_of (r) == 200 && strstr (r, "\"dirs\":[{\"n\":\"Unter ordner\"}") != NULL && strstr (r, "a.txt") != NULL &&
	       strstr (r, "etclink") == NULL && strstr (r, "versteckt") == NULL, "/dir: Ordner und Dateien, ohne Ordner-Symlink");
	g_free (r);
	g_free (target);
	target = g_strdup_printf ("/dir?%s&p=Unter%%20ordner", token_q);
	r = get (port, target);
	check (status_of (r) == 200 && strstr (r, "b.txt") != NULL && strstr (r, "a.txt") == NULL, "/dir: Inhalt des Unterordners");
	g_free (r);
	g_free (target);
	target = g_strdup_printf ("/dl?%s&p=Unter%%20ordner&n=b.txt", token_q);
	r = get (port, target);
	check (status_of (r) == 200 && g_str_has_suffix (r, "hallo aus dem Unterordner"), "/dl mit Unterordner liefert die Datei");
	g_free (r);
	g_free (target);
	{
		const gchar *bad[] = { "..", "..%2F..", "etclink", "%2Fetc", "Unter%20ordner%2F..%2F..", ".versteckt", "fehlt", NULL };

		for (i = 0; bad[i] != NULL; i++) {
			gchar *msg = g_strdup_printf ("/dir mit p='%s' wird abgelehnt (404)", bad[i]);

			target = g_strdup_printf ("/dir?%s&p=%s", token_q, bad[i]);
			r = get (port, target);
			check (status_of (r) == 404 && strstr (r, "passwd") == NULL, msg);
			g_free (r);
			g_free (target);
			g_free (msg);
		}
		target = g_strdup_printf ("/dl?%s&p=etclink&n=passwd", token_q);
		r = get (port, target);
		check (status_of (r) == 404 && strstr (r, "root:") == NULL, "/dl über Ordner-Symlink wird abgelehnt");
		g_free (r);
		g_free (target);
	}

	/* Wartepunkt der App: sofort mit Stand, bei bekanntem Stand erst bei Änderung */
	{
		gchar *version, *q;
		gint64 t0;
		GThread *writer;

		target = g_strdup_printf ("/wait?%s", token_q);
		r = get (port, target);
		check (status_of (r) == 200 && strstr (r, "\"v\":\"") != NULL && strstr (r, "a.txt") != NULL, "wait ohne Stand antwortet sofort mit Stand und Liste");
		q = r != NULL ? strstr (r, "\"v\":\"") : NULL;
		version = q != NULL ? g_strndup (q + 5, 32) : g_strdup ("");
		g_free (r);
		g_free (target);

		target = g_strdup_printf ("/wait?%s&v=%s", token_q, version);
		writer = g_thread_new ("writer", write_later, share);
		t0 = g_get_monotonic_time ();
		r = get (port, target);
		check (status_of (r) == 200 && strstr (r, "spaeter.txt") != NULL, "wait mit Stand antwortet, sobald eine neue Datei da ist");
		check (g_get_monotonic_time () - t0 < 15 * G_USEC_PER_SEC, "wait wartet nicht bis zum Ablauf");
		g_thread_join (writer);
		g_free (r);
		g_free (target);
		g_free (version);
	}

	/* App-Datei (APK) */
	target = g_strdup_printf ("/apk?%s", token_q);
	g_unsetenv ("NOLPHIN_PHONE_APK");
	r = get (port, target);
	check (status_of (r) == 404 || status_of (r) == 200, "apk ohne Datei: 404 (oder vorhandene App)");
	g_free (r);
	{
		gchar *apk = g_build_filename (root, "test.apk", NULL);

		g_file_set_contents (apk, "PK-test-apk", -1, NULL);
		g_setenv ("NOLPHIN_PHONE_APK", apk, TRUE);
		r = get (port, target);
		check (status_of (r) == 200 && strstr (r, "application/vnd.android.package-archive") != NULL &&
		       g_str_has_suffix (r, "PK-test-apk"), "apk mit Datei: Inhalt und Typ");
		g_free (r);
		g_free (apk);
		g_unsetenv ("NOLPHIN_PHONE_APK");
	}
	g_free (target);
	{
		gchar *noauth = get (port, "/apk");

		check (status_of (noauth) == 403, "apk ohne Schlüssel: 403");
		g_free (noauth);
	}

	/* Gerätedaten (/info): prüft, dass Nolphin antwortet */
	target = g_strdup_printf ("/info?%s", token_q);
	r = get (port, target);
	check (status_of (r) == 200 && strstr (r, "\"app\":\"nolphin\"") != NULL && strstr (r, "\"api\":1") != NULL &&
	       strstr (r, "\"expires\":") != NULL && strstr (r, "\"share\":true") != NULL, "info: App, API-Version, Ablauf");
	g_free (r);
	g_free (target);
	r = get (port, "/info?t=falsch");
	check (status_of (r) == 403, "info mit falschem Schlüssel: 403");
	g_free (r);

	/* Widerruf: neuer Schlüssel, der alte ist sofort ungültig */
	{
		gchar *old_q = g_strdup (token_q);
		gchar *fresh;
		gint64 exp_before = nolphin_phone_server_get_expires (server);

		g_usleep (1100 * 1000);
		check (nolphin_phone_server_rotate_token (server, NULL), "Schlüsselwechsel");
		fresh = nolphin_phone_server_dup_token (server);
		check (strcmp (fresh, token) != 0 && strlen (fresh) == 32, "neuer Schlüssel unterscheidet sich");
		check (nolphin_phone_server_get_expires (server) > exp_before, "Ablaufzeit wird beim Wechsel erneuert");
		target = g_strdup_printf ("/info?%s", old_q);
		r = get (port, target);
		check (status_of (r) == 403, "alter Schlüssel nach dem Wechsel: 403");
		g_free (r);
		g_free (target);
		target = g_strdup_printf ("/info?t=%s", fresh);
		r = get (port, target);
		check (status_of (r) == 200, "neuer Schlüssel gilt");
		g_free (r);
		g_free (target);
		g_free (fresh);
		g_free (old_q);
	}

	/* Meldungen über empfangene Dateien */
	g_mutex_lock (&received_lock);
	check (received_names->len == 5, "Rückmeldung für jede empfangene Datei");
	g_mutex_unlock (&received_lock);

	/* Ablauf: Schlüssel mit kurzer Lebensdauer wird nach Ablauf abgelehnt, mit Hinweis "expired" */
	{
		NolphinPhoneServer *shortlived;
		gchar *tk, *tq;
		guint16 sp;

		g_setenv ("NOLPHIN_PHONE_TOKEN_LIFETIME", "2", TRUE);
		shortlived = nolphin_phone_server_start ("127.0.0.1", inbox_file, NULL, NULL, NULL, NULL, NULL, &error);
		g_unsetenv ("NOLPHIN_PHONE_TOKEN_LIFETIME");
		check (shortlived != NULL, "Server mit kurzer Schlüsselzeit startet");
		if (shortlived != NULL) {
			sp = nolphin_phone_server_get_port (shortlived);
			tk = nolphin_phone_server_dup_token (shortlived);
			tq = g_strdup_printf ("/info?t=%s", tk);
			r = get (sp, tq);
			check (status_of (r) == 200 && strstr (r, "\"share\":false") != NULL, "Schlüssel gilt vor dem Ablauf");
			g_free (r);
			g_usleep (3200 * 1000);
			r = get (sp, tq);
			check (status_of (r) == 403 && g_str_has_suffix (r, "Der Link ist abgelaufen."), "abgelaufener Schlüssel: 403 mit deutschem Hinweis");
			g_free (r);
			nolphin_phone_server_stop (shortlived);
			g_free (tk);
			g_free (tq);
		}
	}

	/* Einstellungen: eigene Gültigkeit und Grenze */
	{
		NolphinPhoneServerOptions opt = { 5, 1024 };
		NolphinPhoneServer *small = nolphin_phone_server_start ("127.0.0.1", inbox_file, NULL, NULL, NULL, NULL, &opt, &error);

		check (small != NULL, "Server mit eigenen Einstellungen startet");
		if (small != NULL) {
			gchar *tk2 = nolphin_phone_server_dup_token (small);
			gchar *tq2 = g_strdup_printf ("/up?t=%s&n=zu-gross.bin", tk2);
			gchar big_body[2048];

			memset (big_body, 'x', sizeof big_body);
			r = post (nolphin_phone_server_get_port (small), tq2, big_body, sizeof big_body);
			check (status_of (r) == 413, "Datei über der eingestellten Grenze: 413");
			g_free (r);
			g_free (tq2);
			g_free (tk2);
			check (nolphin_phone_server_get_expires (small) - g_get_real_time () / G_USEC_PER_SEC <= 5, "eingestellte Gültigkeit von 5 Sekunden");
			nolphin_phone_server_stop (small);
		}
	}

	/* Ende */
	nolphin_phone_server_stop (server);
	g_usleep (200 * 1000);
	r = get (port, "/");
	check (r == NULL || status_of (r) == 0, "nach dem Beenden keine Antwort mehr");
	g_free (r);

	{
		gchar *argv[] = { "rm", "-rf", root, NULL };

		g_spawn_sync (NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL, NULL, NULL);
	}
	g_free (token_q);
	g_free (token_owned);
	g_object_unref (inbox_file);
	g_object_unref (share_file);
	g_free (inbox);
	g_free (share);
	g_free (root);
	g_ptr_array_free (received_names, TRUE);
	if (exit_code == 0) {
		g_print ("alle Tests bestanden\n");
	}
	g_main_loop_quit (loop);
	return NULL;
}

int
main (void)
{
	GThread *thread;

	loop = g_main_loop_new (NULL, FALSE);
	thread = g_thread_new ("tests", run_tests, NULL);
	g_main_loop_run (loop);
	g_thread_join (thread);
	return exit_code;
}
