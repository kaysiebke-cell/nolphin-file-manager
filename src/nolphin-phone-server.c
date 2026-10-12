/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nolphin-phone-server.c: kleiner HTTP-Server für den Handy-Austausch
 *
 * Ablauf: Das Handy öffnet http://<PC>:<Port>/?t=<Schlüssel>. Die Seite
 * lädt Dateien per POST /up hoch und listet/lädt Dateien des Freigabe-
 * ordners über GET /ls und GET /dl. Jede Anfrage braucht den Schlüssel.
 * Verbindungen werden in Arbeitsthreads bearbeitet (GThreadedSocketService).
 */

#include <config.h>

#include "nolphin-phone-server.h"

#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define HEADER_MAX 16384
#define IO_TIMEOUT_SECONDS 30
#define MAX_THREADS 8
#define LIST_MAX 500
#define TOKEN_BYTES 16
#define IO_BUFFER 65536
/* Ein Schlüssel gilt höchstens so lange (Sekunden); NOLPHIN_PHONE_TOKEN_LIFETIME überschreibt das für Tests. */
#define TOKEN_LIFETIME_DEFAULT (12 * 3600)

struct _NolphinPhoneServer {
	GThreadedSocketService  *service;  /* Lebensdauer: der Server hängt am Dienst */
	GMutex                   lock;      /* schützt token und expires */
	gchar                   *token;
	gint64                   expires;   /* Unix-Zeit in Sekunden, ab der der Schlüssel nicht mehr gilt */
	gint64                   lifetime;  /* Gültigkeit eines Schlüssels in Sekunden */
	goffset                  max_upload;/* größte angenommene Datei in Bytes */
	guint16                  port;
	gchar                   *inbox_path;
	gchar                   *share_path;
	gchar                   *home_path;  /* persönlicher Ordner, abrufbar mit r=home (nur lesen) */
	NolphinPhoneReceivedFunc received;
	gpointer                 user_data;
	GDestroyNotify           user_data_free;
	gint                     stopped;       /* atomar */
	gint                     last_activity; /* atomar, g_get_monotonic_time () / 1e6 */
	gint                     last_app;      /* atomar, wie last_activity, aber nur vom Wartepunkt der App (0 = nie) */
};

typedef struct {
	gchar   *method;
	gchar   *path;
	GHashTable *query;
	goffset  content_length;   /* -1 = nicht angegeben */
	gboolean chunked;
	gboolean expect_continue;
} Request;

/* --- Hilfsfunktionen ------------------------------------------------------------ */

static void
server_free (gpointer data)
{
	NolphinPhoneServer *s = data;

	if (s->user_data_free != NULL) {
		s->user_data_free (s->user_data);
	}
	g_free (s->token);
	g_mutex_clear (&s->lock);
	g_free (s->inbox_path);
	g_free (s->share_path);
	g_free (s->home_path);
	g_free (s);
}

static void
touch_activity (NolphinPhoneServer *s)
{
	g_atomic_int_set (&s->last_activity, (gint) (g_get_monotonic_time () / G_USEC_PER_SEC));
}

static void
touch_app (NolphinPhoneServer *s)
{
	g_atomic_int_set (&s->last_app, (gint) (g_get_monotonic_time () / G_USEC_PER_SEC));
}

static gboolean
is_stopped (NolphinPhoneServer *s)
{
	return g_atomic_int_get (&s->stopped) != 0;
}

/* Vergleich ohne frühen Abbruch. */
static gboolean
token_equal (const gchar *a, const gchar *b)
{
	gsize la = strlen (a), lb = strlen (b), i;
	guint8 diff = la == lb ? 0 : 1;

	for (i = 0; i < la && i < lb; i++) {
		diff |= (guint8) (a[i] ^ b[i]);
	}
	return diff == 0;
}

static gint64
token_lifetime_default (void)
{
	const gchar *env = g_getenv ("NOLPHIN_PHONE_TOKEN_LIFETIME");
	gint64 v = env != NULL ? g_ascii_strtoll (env, NULL, 10) : 0;

	return v > 0 ? v : TOKEN_LIFETIME_DEFAULT;
}

typedef enum { TOKEN_OK, TOKEN_INVALID, TOKEN_EXPIRED } TokenState;

static TokenState
check_token (NolphinPhoneServer *s, const gchar *given)
{
	TokenState state;

	g_mutex_lock (&s->lock);
	if (given == NULL || !token_equal (given, s->token)) {
		state = TOKEN_INVALID;
	} else if (g_get_real_time () / G_USEC_PER_SEC >= s->expires) {
		state = TOKEN_EXPIRED;
	} else {
		state = TOKEN_OK;
	}
	g_mutex_unlock (&s->lock);
	return state;
}

/* TRUE, wenn @t genau 32 Zeichen aus 0-9a-f ist (Format des Schlüssels). */
static gboolean
is_token (const gchar *t)
{
	guint i;

	if (strlen (t) != 32) {
		return FALSE;
	}
	for (i = 0; i < 32; i++) {
		if (!((t[i] >= '0' && t[i] <= '9') || (t[i] >= 'a' && t[i] <= 'f'))) {
			return FALSE;
		}
	}
	return TRUE;
}

static gchar *
make_token (GError **error)
{
	guint8 raw[TOKEN_BYTES];
	GString *hex = g_string_new (NULL);
	gint fd = g_open ("/dev/urandom", O_RDONLY | O_CLOEXEC, 0);
	gsize got = 0;
	gint i;

	if (fd < 0) {
		g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
			     _("Zufallsquelle nicht verfügbar: %s"), g_strerror (errno));
		g_string_free (hex, TRUE);
		return NULL;
	}
	while (got < sizeof raw) {
		gssize n = read (fd, raw + got, sizeof raw - got);

		if (n <= 0) {
			if (n < 0 && errno == EINTR) {
				continue;
			}
			close (fd);
			g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
					     _("Zufallsquelle nicht lesbar"));
			g_string_free (hex, TRUE);
			return NULL;
		}
		got += (gsize) n;
	}
	close (fd);
	for (i = 0; i < TOKEN_BYTES; i++) {
		g_string_append_printf (hex, "%02x", raw[i]);
	}
	return g_string_free (hex, FALSE);
}

gchar *
nolphin_phone_sanitize_name (const gchar *name)
{
	const gchar *base, *p;
	GString *out;
	gchar *valid, *result;
	gsize len;

	if (name == NULL) {
		return g_strdup ("Datei");
	}
	/* nur der letzte Pfadteil, auch bei Windows-Trennern */
	base = name;
	for (p = name; *p != '\0'; p++) {
		if (*p == '/' || *p == '\\') {
			base = p + 1;
		}
	}
	valid = g_utf8_make_valid (base, -1);
	out = g_string_new (NULL);
	for (p = valid; *p != '\0'; p = g_utf8_next_char (p)) {
		gunichar c = g_utf8_get_char (p);

		if (c < 0x20 || c == 0x7F || (c >= 0x80 && c < 0xA0) || c == 0x2028 || c == 0x2029 ||
		    c == 0x202E /* Umkehr der Schreibrichtung */) {
			continue;
		}
		g_string_append_unichar (out, c);
	}
	g_free (valid);

	/* führende Punkte und Leerzeichen, Leerzeichen am Ende */
	while (out->len > 0 && (out->str[0] == '.' || out->str[0] == ' ')) {
		g_string_erase (out, 0, 1);
	}
	while (out->len > 0 && out->str[out->len - 1] == ' ') {
		g_string_truncate (out, out->len - 1);
	}
	/* auf höchstens 200 Bytes an einer Zeichengrenze kürzen */
	if (out->len > 200) {
		const gchar *q = out->str;

		while (*q != '\0' && (gsize) (q - out->str) + g_utf8_skip[(guchar) *q] <= 200) {
			q = g_utf8_next_char (q);
		}
		g_string_truncate (out, (gsize) (q - out->str));
	}
	len = out->len;
	result = g_string_free (out, FALSE);
	if (len == 0) {
		g_free (result);
		return g_strdup ("Datei");
	}
	return result;
}

static void
json_append_string (GString *out, const gchar *s)
{
	const gchar *p;

	g_string_append_c (out, '"');
	for (p = s; *p != '\0'; p++) {
		guchar c = (guchar) *p;

		switch (c) {
		case '"':  g_string_append (out, "\\\""); break;
		case '\\': g_string_append (out, "\\\\"); break;
		case '\n': g_string_append (out, "\\n"); break;
		case '\r': g_string_append (out, "\\r"); break;
		case '\t': g_string_append (out, "\\t"); break;
		default:
			if (c < 0x20 || c == '<' || c == '>' || c == '&') {
				g_string_append_printf (out, "\\u%04x", c);
			} else {
				g_string_append_c (out, (gchar) c);
			}
		}
	}
	g_string_append_c (out, '"');
}

/* --- Anfrage lesen --------------------------------------------------------------- */

static void
request_clear (Request *r)
{
	g_free (r->method);
	g_free (r->path);
	if (r->query != NULL) {
		g_hash_table_destroy (r->query);
	}
	memset (r, 0, sizeof *r);
}

static void
parse_query (Request *r, const gchar *query)
{
	gchar **pairs = g_strsplit (query, "&", 32);
	gint i;

	for (i = 0; pairs[i] != NULL; i++) {
		gchar *eq = strchr (pairs[i], '=');
		gchar *key, *value;

		if (eq == NULL) {
			continue;
		}
		*eq = '\0';
		key = g_uri_unescape_string (pairs[i], NULL);
		value = g_uri_unescape_string (eq + 1, NULL);
		if (key != NULL && value != NULL) {
			g_hash_table_replace (r->query, key, value);
		} else {
			g_free (key);
			g_free (value);
		}
	}
	g_strfreev (pairs);
}

static gboolean
parse_request (const gchar *head, gsize head_len, Request *r)
{
	gchar *text = g_strndup (head, head_len);
	gchar **lines = g_strsplit (text, "\r\n", -1);
	gchar **parts;
	gchar *qmark;
	gint i;
	gboolean ok = FALSE;

	g_free (text);
	r->content_length = -1;
	r->query = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	if (lines[0] == NULL) {
		goto out;
	}
	parts = g_strsplit (lines[0], " ", 4);
	if (g_strv_length (parts) != 3 || !g_str_has_prefix (parts[2], "HTTP/1.") || parts[1][0] != '/') {
		g_strfreev (parts);
		goto out;
	}
	r->method = g_strdup (parts[0]);
	qmark = strchr (parts[1], '?');
	if (qmark != NULL) {
		*qmark = '\0';
		parse_query (r, qmark + 1);
	}
	r->path = g_strdup (parts[1]);
	g_strfreev (parts);

	for (i = 1; lines[i] != NULL; i++) {
		const gchar *l = lines[i];

		if (g_ascii_strncasecmp (l, "content-length:", 15) == 0) {
			const gchar *v = l + 15;
			gchar *end = NULL;
			gint64 n;

			while (*v == ' ') {
				v++;
			}
			if (!g_ascii_isdigit (*v)) {
				goto out;
			}
			n = g_ascii_strtoll (v, &end, 10);
			if (n < 0 || *end != '\0' || (r->content_length >= 0 && r->content_length != n)) {
				goto out;
			}
			r->content_length = n;
		} else if (g_ascii_strncasecmp (l, "transfer-encoding:", 18) == 0) {
			r->chunked = TRUE;
		} else if (g_ascii_strncasecmp (l, "expect:", 7) == 0 && strstr (l, "100-continue") != NULL) {
			r->expect_continue = TRUE;
		}
	}
	ok = TRUE;
out:
	g_strfreev (lines);
	return ok;
}

/* Liest bis zum Ende der Kopfzeilen. @buf fasst HEADER_MAX + 1 Bytes.
 * @have: gelesene Bytes (kann bereits Körper enthalten), @head_len: Länge der
 * Kopfzeilen inklusive Leerzeile. */
static gboolean
read_head (GInputStream *in, gchar *buf, gsize *have, gsize *head_len)
{
	gsize n = 0;

	while (n < HEADER_MAX) {
		gssize r = g_input_stream_read (in, buf + n, HEADER_MAX - n, NULL, NULL);
		gchar *end;

		if (r <= 0) {
			return FALSE;
		}
		n += (gsize) r;
		buf[n] = '\0';
		end = g_strstr_len (buf, (gssize) n, "\r\n\r\n");
		if (end != NULL) {
			*head_len = (gsize) (end - buf) + 4;
			*have = n;
			return TRUE;
		}
	}
	return FALSE;
}

/* --- Antworten -------------------------------------------------------------------- */

static void
send_head (GOutputStream *out, gint status, const gchar *reason, const gchar *content_type,
	   goffset length, const gchar *extra)
{
	gchar *head = g_strdup_printf ("HTTP/1.1 %d %s\r\n"
				       "Content-Type: %s\r\n"
				       "Content-Length: %" G_GINT64_FORMAT "\r\n"
				       "Connection: close\r\n"
				       "Cache-Control: no-store\r\n"
				       "X-Content-Type-Options: nosniff\r\n"
				       "Referrer-Policy: no-referrer\r\n"
				       "%s"
				       "\r\n",
				       status, reason, content_type, (gint64) length, extra != NULL ? extra : "");

	g_output_stream_write_all (out, head, strlen (head), NULL, NULL, NULL);
	g_free (head);
}

static void
send_text (GOutputStream *out, gint status, const gchar *reason, const gchar *content_type,
	   const gchar *body, const gchar *extra)
{
	send_head (out, status, reason, content_type, (goffset) strlen (body), extra);
	g_output_stream_write_all (out, body, strlen (body), NULL, NULL, NULL);
}

static void
send_error (GOutputStream *out, gint status, const gchar *reason, const gchar *message)
{
	send_text (out, status, reason, "text/plain; charset=utf-8", message, NULL);
}

static gchar *find_apk (void);

/* --- Seite ------------------------------------------------------------------------ */

#define PAGE_HTML \
"<!doctype html><html><head><meta charset=\"utf-8\">" \
"<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">" \
"<title>Nolphin</title><style>" \
":root{color-scheme:light dark;font-family:system-ui,sans-serif}" \
"body{margin:0 auto;padding:16px;max-width:640px}" \
"h1{font-size:1.3rem}h2{font-size:1.05rem;margin-top:1.6rem}" \
"label.btn{display:inline-block;padding:12px 18px;border-radius:10px;background:#2b7de9;color:#fff;font-size:1rem}" \
"ul{list-style:none;padding:0}li{padding:8px 0;border-bottom:1px solid #8884;word-break:break-all}" \
".ok{color:#2a9d3f}.err{color:#d33}" \
"</style></head>" \
"<body data-done=\"@@DONE@@\" data-failed=\"@@FAILED@@\" data-nofiles=\"@@NOFILES@@\" data-share=\"@@SHARE@@\" data-apk=\"@@APK@@\">" \
"<h1>Nolphin</h1>" \
"<h2>@@UPLOAD@@</h2>" \
"<p><label class=\"btn\">@@CHOOSE@@<input id=\"f\" type=\"file\" multiple hidden></label></p>" \
"<ul id=\"up\"></ul>" \
"<div id=\"appbox\" hidden><h2>@@APPTITLE@@</h2><p><a id=\"appc\">@@APPCONNECT@@</a></p><p><a id=\"appg\">@@APPGET@@</a></p><p><a id=\"appstay\" href=\"#\">@@APPSTAY@@</a></p></div>" \
"<div id=\"dlbox\"><h2>@@DOWNLOAD@@</h2><ul id=\"dl\"></ul></div>" \
"<script>" \
"(function(){" \
"var T=new URLSearchParams(location.search).get('t')||'',d=document.body.dataset,q=encodeURIComponent(T);" \
"function add(ul,text){var e=document.createElement('li');e.textContent=text;ul.appendChild(e);return e;}" \
"function size(n){return n<1024?n+' B':n<1048576?Math.round(n/1024)+' KB':(n/1048576).toFixed(1)+' MB';}" \
"function send(file,done){" \
"var row=add(document.getElementById('up'),file.name+' …'),x=new XMLHttpRequest();" \
"x.open('POST','/up?t='+q+'&n='+encodeURIComponent(file.name));" \
"x.upload.onprogress=function(e){if(e.lengthComputable)row.textContent=file.name+' '+Math.round(e.loaded*100/e.total)+' %';};" \
"x.onload=function(){if(x.status==200){row.textContent=file.name+' – '+d.done;row.className='ok';}" \
"else{row.textContent=file.name+' – '+d.failed+' ('+x.status+')';row.className='err';}done();};" \
"x.onerror=function(){row.textContent=file.name+' – '+d.failed;row.className='err';done();};" \
"x.send(file);}" \
"if(d.apk==='1'){var box=document.getElementById('appbox');box.hidden=false;" \
"var apk=location.protocol+'//'+location.host+'/apk?t='+q;" \
"var go='intent://connect?host='+encodeURIComponent(location.host)+'&t='+q+" \
"'#Intent;scheme=nolphin;package=org.nolphin.handy;S.browser_fallback_url='+encodeURIComponent(apk)+';end';" \
"document.getElementById('appc').href=go;document.getElementById('appg').href=apk;" \
"var stay=false;try{stay=localStorage.getItem('nolphin-browser')==='1';}catch(e){}" \
"document.getElementById('appstay').onclick=function(){try{localStorage.setItem('nolphin-browser','1');}catch(e){}" \
"box.hidden=true;return false;};" \
"if(!stay)setTimeout(function(){location.href=go;},400);}" \
"function list(){if(d.share!=='1'){document.getElementById('dlbox').hidden=true;return;}" \
"fetch('/ls?t='+q).then(function(r){return r.json();}).then(function(items){" \
"var ul=document.getElementById('dl'),sig=JSON.stringify(items);if(ul.dataset.sig===sig)return;ul.dataset.sig=sig;ul.textContent='';" \
"if(!items.length)add(ul,d.nofiles);" \
"items.forEach(function(f){var e=add(ul,'');var a=document.createElement('a');" \
"a.href='/dl?t='+q+'&n='+encodeURIComponent(f.n);a.textContent=f.n+' ('+size(f.s)+')';e.appendChild(a);});" \
"}).catch(function(){});}" \
"document.getElementById('f').onchange=function(){var files=Array.prototype.slice.call(this.files),i=0;this.value='';" \
"(function next(){if(i<files.length)send(files[i++],next);})();};" \
"list();setInterval(list,4000);})();" \
"</script></body></html>"

static gchar *
replace_all (gchar *text, const gchar *key, const gchar *plain)
{
	gchar *escaped = g_markup_escape_text (plain, -1);
	gchar **parts = g_strsplit (text, key, -1);
	gchar *joined = g_strjoinv (escaped, parts);

	g_strfreev (parts);
	g_free (escaped);
	g_free (text);
	return joined;
}

static void
handle_index (NolphinPhoneServer *s, GOutputStream *out)
{
	gchar *html = g_strdup (PAGE_HTML);
	const gchar *csp = "Content-Security-Policy: default-src 'none'; style-src 'unsafe-inline'; "
			   "script-src 'unsafe-inline'; connect-src 'self'\r\n";

	html = replace_all (html, "@@DONE@@", _("fertig"));
	html = replace_all (html, "@@FAILED@@", _("fehlgeschlagen"));
	html = replace_all (html, "@@NOFILES@@", _("Keine Dateien freigegeben."));
	html = replace_all (html, "@@SHARE@@", s->share_path != NULL ? "1" : "0");
	html = replace_all (html, "@@UPLOAD@@", _("An den PC senden"));
	html = replace_all (html, "@@CHOOSE@@", _("Dateien auswählen …"));
	html = replace_all (html, "@@DOWNLOAD@@", _("Vom PC laden"));
	{
		gchar *apk = find_apk ();

		html = replace_all (html, "@@APK@@", apk != NULL ? "1" : "0");
		g_free (apk);
	}
	html = replace_all (html, "@@APPTITLE@@", _("Nolphin-Handy-App (optional)"));
	html = replace_all (html, "@@APPCONNECT@@", _("Mit der App verbinden"));
	html = replace_all (html, "@@APPGET@@", _("App herunterladen"));
	html = replace_all (html, "@@APPSTAY@@", _("Im Browser bleiben"));
	send_text (out, 200, "OK", "text/html; charset=utf-8", html, csp);
	g_free (html);
}

/* --- Liste und Download ---------------------------------------------------------- */

static gint
compare_names (gconstpointer a, gconstpointer b)
{
	return g_utf8_collate (*(const gchar * const *) a, *(const gchar * const *) b);
}

/* Gerätedaten für die App: prüft, dass am anderen Ende wirklich Nolphin läuft (API-Version 1). */
static void
handle_info (NolphinPhoneServer *s, GOutputStream *out)
{
	GString *json = g_string_new ("{\"app\":\"nolphin\",\"api\":1,\"home\":true,\"folders\":true,\"name\":");
	gint64 expires;

	json_append_string (json, g_get_host_name () != NULL ? g_get_host_name () : "");
	g_mutex_lock (&s->lock);
	expires = s->expires;
	g_mutex_unlock (&s->lock);
	g_string_append_printf (json, ",\"expires\":%" G_GINT64_FORMAT ",\"maxUpload\":%" G_GINT64_FORMAT ",\"share\":%s}",
				expires, (gint64) s->max_upload, s->share_path != NULL ? "true" : "false");
	send_text (out, 200, "OK", "application/json; charset=utf-8", json->str, NULL);
	g_string_free (json, TRUE);
}

/* Eine Datei, die sich in den letzten Sekunden geändert hat, wird noch geschrieben
 * (zum Beispiel beim Kopieren aus der Dateiansicht) und erscheint erst danach. */
#define BUSY_SECONDS 3

/* Baut die Liste der Dateien im Ausgangsordner als JSON-Array. */
static GString *
build_listing (NolphinPhoneServer *s)
{
	GString *json = g_string_new ("[");
	GPtrArray *names = g_ptr_array_new_with_free_func (g_free);
	GDir *dir;
	const gchar *name;
	gint64 now = g_get_real_time () / G_USEC_PER_SEC;
	guint i;

	dir = s->share_path != NULL ? g_dir_open (s->share_path, 0, NULL) : NULL;
	while (dir != NULL && (name = g_dir_read_name (dir)) != NULL && names->len < LIST_MAX) {
		if (name[0] != '.' && g_utf8_validate (name, -1, NULL)) {
			g_ptr_array_add (names, g_strdup (name));
		}
	}
	if (dir != NULL) {
		g_dir_close (dir);
	}
	g_ptr_array_sort (names, compare_names);
	for (i = 0; i < names->len; i++) {
		gchar *path = g_build_filename (s->share_path, g_ptr_array_index (names, i), NULL);
		GStatBuf st;

		/* nur normale Dateien, keine symbolischen Verknüpfungen, keine Dateien im Schreiben */
		if (g_lstat (path, &st) == 0 && S_ISREG (st.st_mode) &&
		    now - (gint64) MAX (st.st_mtime, st.st_ctime) >= BUSY_SECONDS) {
			if (json->len > 1) {
				g_string_append_c (json, ',');
			}
			g_string_append (json, "{\"n\":");
			json_append_string (json, g_ptr_array_index (names, i));
			g_string_append_printf (json, ",\"s\":%" G_GINT64_FORMAT "}", (gint64) st.st_size);
		}
		g_free (path);
	}
	g_string_append_c (json, ']');
	g_ptr_array_free (names, TRUE);
	return json;
}

static void
handle_list (NolphinPhoneServer *s, GOutputStream *out)
{
	GString *json = build_listing (s);

	send_text (out, 200, "OK", "application/json; charset=utf-8", json->str, NULL);
	g_string_free (json, TRUE);
}

/* Löst einen Unterordner (relativ zu @base, mit "/" getrennt) auf. Erlaubt sind nur
 * normale Namen (keine "..", keine versteckten Namen) und echte Ordner (keine Verknüpfungen),
 * damit die App nie aus @base hinauskommt. Gibt NULL zurück, wenn der Weg nicht gilt. */
static gchar *
resolve_in (const gchar *base, const gchar *rel)
{
	gchar **parts;
	gchar *path;
	gint i;

	if (base == NULL) {
		return NULL;
	}
	path = g_strdup (base);
	if (rel == NULL || rel[0] == '\0') {
		return path;
	}
	parts = g_strsplit (rel, "/", -1);
	for (i = 0; parts[i] != NULL; i++) {
		gchar *safe, *next;
		gboolean ok;
		GStatBuf st;

		if (parts[i][0] == '\0') {
			continue;
		}
		safe = nolphin_phone_sanitize_name (parts[i]);
		ok = strcmp (safe, parts[i]) == 0 && parts[i][0] != '.';
		g_free (safe);
		if (!ok) {
			g_free (path);
			path = NULL;
			break;
		}
		next = g_build_filename (path, parts[i], NULL);
		g_free (path);
		path = next;
		if (g_lstat (path, &st) != 0 || !S_ISDIR (st.st_mode)) {
			g_free (path);
			path = NULL;
			break;
		}
	}
	g_strfreev (parts);
	return path;
}

/* Wurzel einer Anfrage: ohne "r" der Ausgangsordner („Zum Handy“), mit r=home der persönliche Ordner.
 * Jeder andere Wert gilt nicht (NULL). Das Ergebnis gehört dem Aufrufer. */
static gchar *
request_base (NolphinPhoneServer *s, const Request *r)
{
	const gchar *root = g_hash_table_lookup (r->query, "r");

	if (root == NULL || root[0] == '\0') {
		return g_strdup (s->share_path);
	}
	if (strcmp (root, "home") == 0) {
		return g_strdup (s->home_path);
	}
	return NULL;
}

/* GET /dir?r=<home>&p=<Unterordner>: Ordner und Dateien eines Unterordners für die Ordneransicht der App.
 * Antwort: {"dirs":[{"n":"Name"}],"files":[{"n":"Name","s":Größe}]}. */
static void
handle_dir (NolphinPhoneServer *s, const Request *r, GOutputStream *out)
{
	gchar *base = request_base (s, r);
	gchar *dirpath = resolve_in (base, g_hash_table_lookup (r->query, "p"));
	GString *dirs = g_string_new (NULL);
	GString *files = g_string_new (NULL);
	GPtrArray *names = g_ptr_array_new_with_free_func (g_free);
	GDir *dir;
	const gchar *name;
	gint64 now = g_get_real_time () / G_USEC_PER_SEC;
	guint i;

	if (dirpath == NULL) {
		send_error (out, 404, "Not Found", _("Nicht gefunden."));
		goto out;
	}
	dir = g_dir_open (dirpath, 0, NULL);
	while (dir != NULL && (name = g_dir_read_name (dir)) != NULL && names->len < LIST_MAX) {
		if (name[0] != '.' && g_utf8_validate (name, -1, NULL)) {
			g_ptr_array_add (names, g_strdup (name));
		}
	}
	if (dir != NULL) {
		g_dir_close (dir);
	}
	g_ptr_array_sort (names, compare_names);
	for (i = 0; i < names->len; i++) {
		gchar *path = g_build_filename (dirpath, g_ptr_array_index (names, i), NULL);
		GStatBuf st;

		if (g_lstat (path, &st) == 0 && S_ISDIR (st.st_mode)) {
			if (dirs->len > 0) {
				g_string_append_c (dirs, ',');
			}
			g_string_append (dirs, "{\"n\":");
			json_append_string (dirs, g_ptr_array_index (names, i));
			g_string_append_c (dirs, '}');
		} else if (g_lstat (path, &st) == 0 && S_ISREG (st.st_mode) &&
			   now - (gint64) MAX (st.st_mtime, st.st_ctime) >= BUSY_SECONDS) {
			if (files->len > 0) {
				g_string_append_c (files, ',');
			}
			g_string_append (files, "{\"n\":");
			json_append_string (files, g_ptr_array_index (names, i));
			g_string_append_printf (files, ",\"s\":%" G_GINT64_FORMAT "}", (gint64) st.st_size);
		}
		g_free (path);
	}
	{
		gchar *body = g_strdup_printf ("{\"dirs\":[%s],\"files\":[%s]}", dirs->str, files->str);

		send_text (out, 200, "OK", "application/json; charset=utf-8", body, NULL);
		g_free (body);
	}
out:
	g_free (base);
	g_free (dirpath);
	g_ptr_array_free (names, TRUE);
	g_string_free (dirs, TRUE);
	g_string_free (files, TRUE);
}

/* Wartepunkt für die App: antwortet, sobald sich die Liste gegenüber dem
 * Stand "v" ändert, spätestens nach WAIT_SECONDS. Antwort: {"v":"<Stand>","files":[…]}. */
#define WAIT_SECONDS 20

static void
handle_wait (NolphinPhoneServer *s, const Request *r, GOutputStream *out)
{
	const gchar *known = g_hash_table_lookup (r->query, "v");
	GString *json = NULL;
	gchar *version = NULL;
	gint i;

	for (i = 0; i < WAIT_SECONDS * 2 && !is_stopped (s); i++) {
		if (json != NULL) {
			g_string_free (json, TRUE);
			g_free (version);
		}
		json = build_listing (s);
		version = g_compute_checksum_for_string (G_CHECKSUM_MD5, json->str, -1);
		if (known == NULL || strcmp (known, version) != 0) {
			break;
		}
		touch_activity (s);   /* eine verbundene App zählt als Aktivität */
		touch_app (s);
		g_usleep (500 * 1000);
	}
	if (json == NULL) {
		return;
	}
	{
		GString *body = g_string_new ("{\"v\":\"");

		g_string_append (body, version);
		g_string_append (body, "\",\"files\":");
		g_string_append (body, json->str);
		g_string_append_c (body, '}');
		send_text (out, 200, "OK", "application/json; charset=utf-8", body->str, NULL);
		g_string_free (body, TRUE);
	}
	g_string_free (json, TRUE);
	g_free (version);
}

/* Sendet die geöffnete Datei @fd (Größe aus @st) und schließt sie. */
static void
stream_fd (NolphinPhoneServer *s, GOutputStream *out, gint fd, const GStatBuf *st,
	   const gchar *content_type, const gchar *extra)
{
	gchar *buf;

	send_head (out, 200, "OK", content_type, (goffset) st->st_size, extra);
	buf = g_malloc (IO_BUFFER);
	while (!is_stopped (s)) {
		gssize n = read (fd, buf, IO_BUFFER);

		if (n < 0 && errno == EINTR) {
			continue;
		}
		if (n <= 0 || !g_output_stream_write_all (out, buf, (gsize) n, NULL, NULL, NULL)) {
			break;
		}
		touch_activity (s);
	}
	g_free (buf);
	close (fd);
}

/* Ort der Handy-App (APK): NOLPHIN_PHONE_APK (Tests) oder ein Datenverzeichnis des Systems. */
static gchar *
find_apk (void)
{
	const gchar *env = g_getenv ("NOLPHIN_PHONE_APK");
	const gchar * const *dirs;
	gchar *path;

	if (env != NULL && g_file_test (env, G_FILE_TEST_IS_REGULAR)) {
		return g_strdup (env);
	}
	path = g_build_filename (g_get_user_data_dir (), "nolphin", "phone", "nolphin-handy.apk", NULL);
	if (g_file_test (path, G_FILE_TEST_IS_REGULAR)) {
		return path;
	}
	g_free (path);
#ifdef NOLPHIN_DATADIR
	path = g_build_filename (NOLPHIN_DATADIR, "phone", "nolphin-handy.apk", NULL);
	if (g_file_test (path, G_FILE_TEST_IS_REGULAR)) {
		return path;
	}
	g_free (path);
#endif
	for (dirs = g_get_system_data_dirs (); *dirs != NULL; dirs++) {
		path = g_build_filename (*dirs, "nolphin", "phone", "nolphin-handy.apk", NULL);
		if (g_file_test (path, G_FILE_TEST_IS_REGULAR)) {
			return path;
		}
		g_free (path);
	}
	return NULL;
}

static void
handle_apk (NolphinPhoneServer *s, GOutputStream *out)
{
	gchar *path = find_apk ();
	GStatBuf st;
	gint fd;

	fd = path != NULL ? g_open (path, O_RDONLY | O_CLOEXEC, 0) : -1;
	g_free (path);
	if (fd < 0 || fstat (fd, &st) != 0 || !S_ISREG (st.st_mode)) {
		if (fd >= 0) {
			close (fd);
		}
		send_error (out, 404, "Not Found", _("Nicht gefunden."));
		return;
	}
	stream_fd (s, out, fd, &st, "application/vnd.android.package-archive",
		   "Content-Disposition: attachment; filename=\"nolphin-handy.apk\"\r\n");
}

static void
handle_download (NolphinPhoneServer *s, const Request *r, GOutputStream *out)
{
	const gchar *name = g_hash_table_lookup (r->query, "n");
	gchar *safe, *path, *base, *escaped, *extra;
	GStatBuf st;
	gint fd;

	if (s->share_path == NULL || name == NULL) {
		send_error (out, 404, "Not Found", _("Nicht gefunden."));
		return;
	}
	safe = nolphin_phone_sanitize_name (name);
	/* Der Name muss genau so im Ordner stehen: keine Pfade, keine Änderung durch die Bereinigung. */
	if (strcmp (safe, name) != 0 || name[0] == '.') {
		g_free (safe);
		send_error (out, 404, "Not Found", _("Nicht gefunden."));
		return;
	}
	g_free (safe);
	/* Optional "p": Unterordner, aus dem die Datei kommt (Ordneransicht der App) */
	{
		gchar *root = request_base (s, r);

		base = resolve_in (root, g_hash_table_lookup (r->query, "p"));
		g_free (root);
	}
	if (base == NULL) {
		send_error (out, 404, "Not Found", _("Nicht gefunden."));
		return;
	}
	path = g_build_filename (base, name, NULL);
	g_free (base);
	fd = g_open (path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW, 0);
	g_free (path);
	if (fd < 0 || fstat (fd, &st) != 0 || !S_ISREG (st.st_mode)) {
		if (fd >= 0) {
			close (fd);
		}
		send_error (out, 404, "Not Found", _("Nicht gefunden."));
		return;
	}
	escaped = g_uri_escape_string (name, NULL, FALSE);
	extra = g_strdup_printf ("Content-Disposition: attachment; filename=\"download\"; filename*=UTF-8''%s\r\n", escaped);
	stream_fd (s, out, fd, &st, "application/octet-stream", extra);
	g_free (escaped);
	g_free (extra);
}

/* --- Upload ----------------------------------------------------------------------- */

/* Legt @tmp unter einem freien Namen im Ordner ab (link () überschreibt nie).
 * Gibt den endgültigen Namen zurück oder NULL. */
static gchar *
publish_unique (const gchar *dir, const gchar *tmp, const gchar *name)
{
	const gchar *dot = strrchr (name, '.');
	gchar *stem = (dot != NULL && dot != name) ? g_strndup (name, (gsize) (dot - name)) : g_strdup (name);
	const gchar *ext = (dot != NULL && dot != name) ? dot : "";
	gchar *result = NULL;
	gint n;

	for (n = 0; n < 1000 && result == NULL; n++) {
		gchar *candidate = n == 0 ? g_strdup (name) : g_strdup_printf ("%s (%d)%s", stem, n, ext);
		gchar *path = g_build_filename (dir, candidate, NULL);

		if (link (tmp, path) == 0) {
			result = candidate;
			candidate = NULL;
		} else if (errno != EEXIST) {
			g_free (path);
			g_free (candidate);
			break;
		}
		g_free (path);
		g_free (candidate);
	}
	g_free (stem);
	return result;
}

/* Legt einen Unterordner im Eingangsordner an ("Ordner/Unter"). Jede Ebene muss ein ungefährlicher Name sein
 * und ein echter Ordner (kein Link); bestehende Ordner werden weiterverwendet. Gibt den vollen Pfad zurück oder
 * NULL bei einem ungültigen Namen. Das Ergebnis gehört dem Aufrufer. */
static gchar *
ensure_subdir (const gchar *base, const gchar *rel)
{
	gchar **parts = g_strsplit (rel, "/", -1);
	gchar *path = g_strdup (base);
	guint i, levels = 0;

	for (i = 0; parts[i] != NULL; i++) {
		gchar *safe, *next;
		struct stat st;
		gboolean ok;

		if (parts[i][0] == '\0') {
			continue;
		}
		if (++levels > 32) {
			g_free (path);
			path = NULL;
			break;
		}
		safe = nolphin_phone_sanitize_name (parts[i]);
		ok = strcmp (safe, parts[i]) == 0 && parts[i][0] != '.';
		g_free (safe);
		if (!ok) {
			g_free (path);
			path = NULL;
			break;
		}
		next = g_build_filename (path, parts[i], NULL);
		g_free (path);
		path = next;
		if (g_lstat (path, &st) == 0) {
			if (!S_ISDIR (st.st_mode)) {
				g_free (path);
				path = NULL;
				break;
			}
		} else if (g_mkdir (path, 0755) != 0 && errno != EEXIST) {
			g_free (path);
			path = NULL;
			break;
		}
	}
	g_strfreev (parts);
	return path;
}

static void
handle_upload (NolphinPhoneServer *s, const Request *r, GInputStream *in, GOutputStream *out,
	       const gchar *body, gsize body_have)
{
	const gchar *raw_name = g_hash_table_lookup (r->query, "n");
	const gchar *raw_dir = g_hash_table_lookup (r->query, "d");
	gchar *name, *tmpl, *final_name, *target_dir;
	gint fd;
	goffset remaining, total;
	gchar *buf;
	gboolean failed = FALSE;
	gint err = 0;

	if (r->chunked) {
		send_error (out, 501, "Not Implemented", _("Dieser Upload wird nicht unterstützt (Transfer-Encoding chunked)."));
		return;
	}
	if (r->content_length < 0) {
		send_error (out, 411, "Length Required", _("Die Dateigröße fehlt (Content-Length)."));
		return;
	}
	if (r->content_length > s->max_upload) {
		gchar *msg = g_strdup_printf (_("Die Datei ist zu groß (höchstens %" G_GINT64_FORMAT " MiB)."), (gint64) (s->max_upload / (1024 * 1024)));

		send_error (out, 413, "Payload Too Large", msg);
		g_free (msg);
		return;
	}
	if (raw_dir != NULL && raw_dir[0] != '\0') {
		target_dir = ensure_subdir (s->inbox_path, raw_dir);
		if (target_dir == NULL) {
			send_error (out, 400, "Bad Request", _("Ungültiger Ordnername."));
			return;
		}
	} else {
		target_dir = g_strdup (s->inbox_path);
	}
	name = nolphin_phone_sanitize_name (raw_name);
	tmpl = g_build_filename (s->inbox_path, ".nolphin-XXXXXX", NULL);
	fd = g_mkstemp_full (tmpl, O_WRONLY | O_CLOEXEC, 0644);
	if (fd < 0) {
		send_error (out, 500, "Internal Server Error", _("Die Datei kann am PC nicht angelegt werden."));
		g_free (tmpl);
		g_free (name);
		g_free (target_dir);
		return;
	}
	if (r->expect_continue) {
		const gchar *go = "HTTP/1.1 100 Continue\r\n\r\n";

		g_output_stream_write_all (out, go, strlen (go), NULL, NULL, NULL);
	}

	total = remaining = r->content_length;
	if ((goffset) body_have > remaining) {
		body_have = (gsize) remaining;
	}
	if (body_have > 0) {
		if (write (fd, body, body_have) != (gssize) body_have) {
			failed = TRUE;
			err = errno;
		}
		remaining -= (goffset) body_have;
	}
	buf = g_malloc (IO_BUFFER);
	while (!failed && remaining > 0 && !is_stopped (s)) {
		gssize n = g_input_stream_read (in, buf, (gsize) MIN ((goffset) IO_BUFFER, remaining), NULL, NULL);
		gssize done = 0;

		if (n <= 0) {
			failed = TRUE;
			err = 0;
			break;
		}
		while (done < n) {
			gssize w = write (fd, buf + done, (gsize) (n - done));

			if (w < 0) {
				if (errno == EINTR) {
					continue;
				}
				failed = TRUE;
				err = errno;
				break;
			}
			done += w;
		}
		remaining -= n;
		touch_activity (s);
	}
	g_free (buf);
	if (close (fd) != 0 && !failed) {
		failed = TRUE;
		err = errno;
	}
	if (is_stopped (s) && remaining > 0) {
		failed = TRUE;
	}
	if (failed || remaining > 0) {
		g_unlink (tmpl);
		if (err == ENOSPC || err == EDQUOT) {
			send_error (out, 507, "Insufficient Storage", _("Am PC ist kein Speicherplatz mehr frei."));
		} else if (err != 0) {
			send_error (out, 500, "Internal Server Error", _("Schreibfehler am PC."));
		}
		/* bei abgebrochener Verbindung gibt es niemanden mehr, der antwortet */
		g_free (tmpl);
		g_free (name);
		g_free (target_dir);
		return;
	}
	final_name = publish_unique (target_dir, tmpl, name);
	g_unlink (tmpl);
	g_free (tmpl);
	if (final_name == NULL) {
		send_error (out, 500, "Internal Server Error", _("Die Datei kann am PC nicht abgelegt werden."));
		g_free (name);
		g_free (target_dir);
		return;
	}
	{
		GString *json = g_string_new ("{\"ok\":true,\"name\":");

		json_append_string (json, final_name);
		g_string_append_c (json, '}');
		send_text (out, 200, "OK", "application/json; charset=utf-8", json->str, NULL);
		g_string_free (json, TRUE);
	}
	if (s->received != NULL && !is_stopped (s)) {
		if (strcmp (target_dir, s->inbox_path) == 0) {
			s->received (final_name, total, s->user_data);
		} else {
			/* mit Ordnerpfad relativ zum Eingangsordner, damit klar ist, wohin die Datei kam */
			const gchar *rel = target_dir + strlen (s->inbox_path);
			gchar *shown;

			while (*rel == G_DIR_SEPARATOR) {
				rel++;
			}
			shown = g_strdup_printf ("%s/%s", rel, final_name);
			s->received (shown, total, s->user_data);
			g_free (shown);
		}
	}
	g_free (final_name);
	g_free (name);
	g_free (target_dir);
}

/* --- Verbindung ------------------------------------------------------------------- */

static gboolean
peer_is_local (GSocketConnection *connection)
{
	GSocketAddress *remote = g_socket_connection_get_remote_address (connection, NULL);
	gboolean ok = FALSE;

	if (remote != NULL && G_IS_INET_SOCKET_ADDRESS (remote)) {
		GInetAddress *a = g_inet_socket_address_get_address (G_INET_SOCKET_ADDRESS (remote));

		ok = g_inet_address_get_family (a) == G_SOCKET_FAMILY_IPV4 &&
		     (g_inet_address_get_is_loopback (a) ||
		      g_inet_address_get_is_site_local (a) ||
		      g_inet_address_get_is_link_local (a));
	}
	g_clear_object (&remote);
	return ok;
}

static gboolean
on_run (GThreadedSocketService *service G_GNUC_UNUSED, GSocketConnection *connection,
        GObject *source G_GNUC_UNUSED, gpointer data)
{
	NolphinPhoneServer *s = data;
	GSocket *socket = g_socket_connection_get_socket (connection);
	GInputStream *in = g_io_stream_get_input_stream (G_IO_STREAM (connection));
	GOutputStream *out = g_io_stream_get_output_stream (G_IO_STREAM (connection));
	gchar *buf;
	gsize have = 0, head_len = 0;
	Request req = { 0 };
	const gchar *token;

	if (is_stopped (s) || !peer_is_local (connection)) {
		return TRUE;
	}
	g_socket_set_timeout (socket, IO_TIMEOUT_SECONDS);

	buf = g_malloc (HEADER_MAX + 1);
	if (!read_head (in, buf, &have, &head_len) || !parse_request (buf, head_len, &req)) {
		send_error (out, 400, "Bad Request", _("Ungültige Anfrage."));
		goto done;
	}
	token = g_hash_table_lookup (req.query, "t");
	switch (check_token (s, token)) {
	case TOKEN_EXPIRED:
		send_error (out, 403, "Forbidden", _("Der Link ist abgelaufen."));
		goto done;
	case TOKEN_INVALID:
		send_error (out, 403, "Forbidden", _("Der Link ist ungültig oder wurde widerrufen."));
		goto done;
	default:
		break;
	}
	touch_activity (s);

	if (strcmp (req.method, "GET") == 0 && strcmp (req.path, "/") == 0) {
		handle_index (s, out);
	} else if (strcmp (req.method, "GET") == 0 && strcmp (req.path, "/info") == 0) {
		handle_info (s, out);
	} else if (strcmp (req.method, "GET") == 0 && strcmp (req.path, "/ls") == 0) {
		handle_list (s, out);
	} else if (strcmp (req.method, "GET") == 0 && strcmp (req.path, "/dir") == 0) {
		handle_dir (s, &req, out);
	} else if (strcmp (req.method, "GET") == 0 && strcmp (req.path, "/wait") == 0) {
		handle_wait (s, &req, out);
	} else if (strcmp (req.method, "GET") == 0 && strcmp (req.path, "/apk") == 0) {
		handle_apk (s, out);
	} else if (strcmp (req.method, "GET") == 0 && strcmp (req.path, "/dl") == 0) {
		handle_download (s, &req, out);
	} else if (strcmp (req.method, "POST") == 0 && strcmp (req.path, "/up") == 0) {
		handle_upload (s, &req, in, out, buf + head_len, have - head_len);
	} else {
		send_error (out, 404, "Not Found", _("Nicht gefunden."));
	}
done:
	request_clear (&req);
	g_free (buf);
	g_io_stream_close (G_IO_STREAM (connection), NULL, NULL);
	return TRUE;
}

/* --- Öffentliche Funktionen -------------------------------------------------------- */

NolphinPhoneServer *
nolphin_phone_server_start (const gchar *bind_address, GFile *inbox, GFile *share,
			    NolphinPhoneReceivedFunc received, gpointer user_data,
			    GDestroyNotify user_data_free, const NolphinPhoneServerOptions *options, GError **error)
{
	NolphinPhoneServer *s;
	GInetAddress *inet;
	GSocketAddress *address, *effective = NULL;
	gchar *inbox_path, *share_path = NULL, *token;
	gboolean resumed;

	g_return_val_if_fail (bind_address != NULL && inbox != NULL, NULL);

	inbox_path = g_file_get_path (inbox);
	if (inbox_path == NULL) {
		g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
				     _("Der Eingangsordner muss ein lokaler Ordner sein."));
		return NULL;
	}
	if (share != NULL) {
		share_path = g_file_get_path (share);
		if (share_path == NULL) {
			g_free (inbox_path);
			g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
					     _("Der Freigabeordner muss ein lokaler Ordner sein."));
			return NULL;
		}
	}
	if (g_mkdir_with_parents (inbox_path, 0755) != 0) {
		g_set_error (error, G_IO_ERROR, g_io_error_from_errno (errno),
			     _("Eingangsordner kann nicht angelegt werden: %s"), g_strerror (errno));
		g_free (inbox_path);
		g_free (share_path);
		return NULL;
	}
	resumed = options != NULL && options->resume_token != NULL && is_token (options->resume_token)
		  && options->resume_expires > g_get_real_time () / G_USEC_PER_SEC + 60;
	token = resumed ? g_strdup (options->resume_token) : make_token (error);
	if (token == NULL) {
		g_free (inbox_path);
		g_free (share_path);
		return NULL;
	}
	inet = g_inet_address_new_from_string (bind_address);
	if (inet == NULL || g_inet_address_get_family (inet) != G_SOCKET_FAMILY_IPV4) {
		g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
			     _("Ungültige Netzwerkadresse: %s"), bind_address);
		g_clear_object (&inet);
		g_free (inbox_path);
		g_free (share_path);
		g_free (token);
		return NULL;
	}

	s = g_new0 (NolphinPhoneServer, 1);
	g_mutex_init (&s->lock);
	s->lifetime = options != NULL && options->token_lifetime > 0 ? options->token_lifetime : token_lifetime_default ();
	s->max_upload = options != NULL && options->max_upload > 0 ? options->max_upload : NOLPHIN_PHONE_MAX_UPLOAD;
	s->token = token;
	s->expires = resumed ? options->resume_expires : g_get_real_time () / G_USEC_PER_SEC + s->lifetime;
	s->inbox_path = inbox_path;
	s->share_path = share_path;
	/* NOLPHIN_PHONE_HOME nur für Tests */
	s->home_path = g_strdup (g_getenv ("NOLPHIN_PHONE_HOME") != NULL ? g_getenv ("NOLPHIN_PHONE_HOME") : g_get_home_dir ());
	s->received = received;
	s->user_data = user_data;
	s->user_data_free = user_data_free;
	touch_activity (s);

	s->service = G_THREADED_SOCKET_SERVICE (g_threaded_socket_service_new (MAX_THREADS));
	g_object_set_data_full (G_OBJECT (s->service), "nolphin-phone-server", s, server_free);
	address = g_inet_socket_address_new (inet, options != NULL ? options->port : 0);
	if (options == NULL || options->port == 0
	    || !g_socket_listener_add_address (G_SOCKET_LISTENER (s->service), address, G_SOCKET_TYPE_STREAM,
					       G_SOCKET_PROTOCOL_DEFAULT, NULL, &effective, NULL)) {
		g_object_unref (address);
		address = g_inet_socket_address_new (inet, 0);   /* Wunschport belegt: vom System wählen lassen */
		effective = NULL;
	}
	g_object_unref (inet);
	if (effective == NULL && !g_socket_listener_add_address (G_SOCKET_LISTENER (s->service), address,
								 G_SOCKET_TYPE_STREAM, G_SOCKET_PROTOCOL_DEFAULT,
								 NULL, &effective, error)) {
		g_object_unref (address);
		s->user_data_free = NULL;      /* bei einem Fehler gehört user_data weiter dem Aufrufer */
		g_object_unref (s->service);   /* gibt über server_free auch s frei */
		return NULL;
	}
	g_object_unref (address);
	s->port = g_inet_socket_address_get_port (G_INET_SOCKET_ADDRESS (effective));
	g_object_unref (effective);
	g_signal_connect (s->service, "run", G_CALLBACK (on_run), s);
	g_socket_service_start (G_SOCKET_SERVICE (s->service));
	return s;
}

guint16
nolphin_phone_server_get_port (NolphinPhoneServer *server)
{
	return server->port;
}

gchar *
nolphin_phone_server_dup_token (NolphinPhoneServer *server)
{
	gchar *t;

	g_mutex_lock (&server->lock);
	t = g_strdup (server->token);
	g_mutex_unlock (&server->lock);
	return t;
}

gint64
nolphin_phone_server_get_expires (NolphinPhoneServer *server)
{
	gint64 e;

	g_mutex_lock (&server->lock);
	e = server->expires;
	g_mutex_unlock (&server->lock);
	return e;
}

gboolean
nolphin_phone_server_rotate_token (NolphinPhoneServer *server, GError **error)
{
	gchar *fresh = make_token (error);

	if (fresh == NULL) {
		return FALSE;
	}
	g_mutex_lock (&server->lock);
	g_free (server->token);
	server->token = fresh;
	server->expires = g_get_real_time () / G_USEC_PER_SEC + server->lifetime;
	g_mutex_unlock (&server->lock);
	return TRUE;
}

gint64
nolphin_phone_server_get_last_app (NolphinPhoneServer *server)
{
	gint v = g_atomic_int_get (&server->last_app);

	return v == 0 ? 0 : (gint64) v * G_USEC_PER_SEC;
}

gint64
nolphin_phone_server_get_last_activity (NolphinPhoneServer *server)
{
	return (gint64) g_atomic_int_get (&server->last_activity) * G_USEC_PER_SEC;
}

void
nolphin_phone_server_stop (NolphinPhoneServer *server)
{
	GThreadedSocketService *service = server->service;

	g_atomic_int_set (&server->stopped, 1);
	g_signal_handlers_disconnect_by_data (service, server);
	g_socket_service_stop (G_SOCKET_SERVICE (service));
	g_socket_listener_close (G_SOCKET_LISTENER (service));
	/* Arbeitsthreads halten den Dienst (und damit den Server) noch am Leben. */
	g_object_unref (service);
}
