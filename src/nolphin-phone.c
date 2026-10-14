/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nolphin-phone.c: Handy-Austausch (Panel im rechten Arbeitsbereich)
 *
 * Startet den kleinen Server aus nolphin-phone-server.c im lokalen Netz und
 * zeigt den Verbindungslink als QR-Code. Das Handy scannt ihn mit der
 * Handy-App und tauscht Dateien verschlüsselt aus (TLS mit festem Zertifikat, Fingerabdruck im QR-Code).
 * Kein Browser-Weg, kein unverschlüsselter Weg.
 */

#include <config.h>

#include "nolphin-phone.h"
#include "nolphin-phone-server.h"
#include "nolphin-phone-tls.h"
#include "nolphin-qr.h"
#include "nolphin-tools.h"
#include "nolphin-window-menus.h"
#include <libnolphin-private/nolphin-global-preferences.h>

#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

/* Einstellungen (GSettings, Abschnitt 31); ohne Einstellungen gelten diese Vorgaben. */
static gint
setting_int (const gchar *key, gint fallback)
{
	return nolphin_preferences != NULL ? g_settings_get_int (nolphin_preferences, key) : fallback;
}

enum { COL_TIME, COL_NAME, COL_SIZE, COL_N };

/* Laufende Austausch-Server (einer je Fenster); nur im Hauptthread verwendet. */
static GSList *live_servers = NULL;

/* Die App fragt am Wartepunkt alle 20 Sekunden an; bleibt das länger als 45 Sekunden aus, gilt sie als getrennt. */
#define APP_CONNECTED_SECONDS 45

gboolean
nolphin_phone_is_connected (void)
{
	GSList *l;
	gint64 now = g_get_monotonic_time ();

	for (l = live_servers; l != NULL; l = l->next) {
		gint64 seen = nolphin_phone_server_get_last_app (l->data);

		if (seen != 0 && (now - seen) / G_USEC_PER_SEC <= APP_CONNECTED_SECONDS) {
			return TRUE;
		}
	}
	return FALSE;
}

/* --- Feste Ordner ---------------------------------------------------------------------
 * ~/Handy: alle Dateien darin kann das Handy laden.
 * ~/Handy/Vom Handy: hierhin kommen Dateien vom Handy (Unterordner werden nicht angeboten). */

gchar *
nolphin_phone_base_path (void)
{
	if (nolphin_preferences != NULL) {
		gchar *configured = g_settings_get_string (nolphin_preferences, NOLPHIN_PREFERENCES_PHONE_FOLDER);

		if (configured != NULL && g_path_is_absolute (configured)) {
			return configured;
		}
		g_free (configured);
	}
	return g_build_filename (g_get_home_dir (), _("Handy"), NULL);
}

gchar *
nolphin_phone_inbox_path (void)
{
	gchar *base = nolphin_phone_base_path ();
	gchar *path = g_build_filename (base, _("Vom Handy"), NULL);

	g_free (base);
	return path;
}

gchar *
nolphin_phone_outbox_path (void)
{
	return nolphin_phone_base_path ();
}

gboolean
nolphin_phone_ensure_folders (void)
{
	gchar *in = nolphin_phone_inbox_path ();
	gchar *out = nolphin_phone_outbox_path ();
	gboolean ok = g_mkdir_with_parents (in, 0755) == 0 && g_mkdir_with_parents (out, 0755) == 0;

	g_free (in);
	g_free (out);
	return ok;
}

/* --- Gemerkte Sitzung ------------------------------------------------------------------
 * Adresse, Port und Schlüssel werden in ~/.config/nolphin/phone-session (nur für den Benutzer lesbar)
 * gemerkt, solange der Schlüssel gilt. So findet das Handy den PC nach einem Neustart von Nolphin
 * von selbst wieder, ohne neuen QR-Code. „Beenden“ und „Neuen QR-Code erzeugen“ widerrufen sie. */

static gchar *
session_path (void)
{
	return g_build_filename (g_get_user_config_dir (), "nolphin", "phone-session", NULL);
}

static void
session_clear (void)
{
	gchar *path = session_path ();

	g_unlink (path);
	g_free (path);
}

static void
session_save (const gchar *address, NolphinPhoneServer *server)
{
	GKeyFile *kf = g_key_file_new ();
	gchar *path = session_path ();
	gchar *dir = g_path_get_dirname (path);
	gchar *token = nolphin_phone_server_dup_token (server);
	gchar *cert_pem = nolphin_phone_server_dup_certificate_pem (server);
	gchar *key_pem = nolphin_phone_server_dup_key_pem (server);
	gchar *data;
	gsize length;

	g_key_file_set_string (kf, "session", "address", address);
	g_key_file_set_integer (kf, "session", "port", nolphin_phone_server_get_port (server));
	g_key_file_set_string (kf, "session", "token", token);
	g_key_file_set_int64 (kf, "session", "expires", nolphin_phone_server_get_expires (server));
	g_key_file_set_string (kf, "session", "cert", cert_pem != NULL ? cert_pem : "");
	g_key_file_set_string (kf, "session", "key", key_pem != NULL ? key_pem : "");
	data = g_key_file_to_data (kf, &length, NULL);
	g_mkdir_with_parents (dir, 0700);
	g_chmod (dir, 0700);
	/* Die Datei enthält den privaten Schlüssel: erst mit Rechten 0600 anlegen, dann füllen */
	{
		gint fd = g_open (path, O_WRONLY | O_CREAT | O_TRUNC, 0600);

		if (fd >= 0) {
			gsize done = 0;

			while (done < length) {
				gssize n = write (fd, data + done, length - done);

				if (n <= 0) {
					break;
				}
				done += (gsize) n;
			}
			fchmod (fd, 0600);
			close (fd);
		}
	}
	memset (data, 0, length);
	g_free (data);
	if (key_pem != NULL) {
		memset (key_pem, 0, strlen (key_pem));
	}
	g_free (key_pem);
	g_free (cert_pem);
	g_free (token);
	g_free (dir);
	g_free (path);
	g_key_file_unref (kf);
}

/* Liefert TRUE und die gemerkten Werte, wenn die Sitzung zu @address passt und noch gilt. */
static gboolean
session_load (const gchar *address, guint16 *port, gchar **token, gint64 *expires, gchar **cert_pem, gchar **key_pem)
{
	GKeyFile *kf = g_key_file_new ();
	gchar *path = session_path ();
	gchar *saved = NULL;
	gboolean ok = FALSE;

	if (g_key_file_load_from_file (kf, path, G_KEY_FILE_NONE, NULL)) {
		saved = g_key_file_get_string (kf, "session", "address", NULL);
		*token = g_key_file_get_string (kf, "session", "token", NULL);
		*port = (guint16) g_key_file_get_integer (kf, "session", "port", NULL);
		*expires = g_key_file_get_int64 (kf, "session", "expires", NULL);
		*cert_pem = g_key_file_get_string (kf, "session", "cert", NULL);
		*key_pem = g_key_file_get_string (kf, "session", "key", NULL);
		ok = saved != NULL && *token != NULL && g_strcmp0 (saved, address) == 0
		     && *expires > g_get_real_time () / G_USEC_PER_SEC + 60
		     && *cert_pem != NULL && **cert_pem != '\0' && *key_pem != NULL && **key_pem != '\0';
		if (!ok) {
			g_clear_pointer (token, g_free);
			g_clear_pointer (cert_pem, g_free);
			if (*key_pem != NULL) {
				memset (*key_pem, 0, strlen (*key_pem));
			}
			g_clear_pointer (key_pem, g_free);
		}
	}
	g_free (saved);
	g_free (path);
	g_key_file_unref (kf);
	return ok;
}

/* Gemeinsamer Zustand zwischen Panel (Hauptthread) und Server (Arbeitsthreads).
 * @page wird nur im Hauptthread gelesen und gesetzt. */
typedef struct {
	gint   ref_count;
	gpointer page;
} PhoneShared;

typedef struct {
	NolphinWindow      *window;
	PhoneShared        *shared;
	NolphinPhoneServer *server;
	GFile              *inbox;
	GFile              *share;
	NolphinQr           qr;
	gboolean            has_qr;
	guint               timeout_id;

	GtkWidget *status;
	GtkWidget *address_combo;
	GtkWidget *start_button;
	GtkWidget *address_box;   /* nur sichtbar, wenn der PC mehrere Netzwerkadressen hat */
	GtkWidget *qr_area;
	GtkWidget *url_entry;
	GtkWidget *copy_button;
	GtkWidget *rotate_button;
	gchar     *address;       /* gewählte Netzwerkadresse des laufenden Austauschs */
	GtkListStore *store;
} PhonePage;

typedef struct {
	PhoneShared *shared;
	gchar       *name;
	goffset      size;
} ReceivedEvent;

static PhoneShared *
shared_ref (PhoneShared *s)
{
	g_atomic_int_inc (&s->ref_count);
	return s;
}

static void
shared_unref (gpointer data)
{
	PhoneShared *s = data;

	if (g_atomic_int_dec_and_test (&s->ref_count)) {
		g_free (s);
	}
}

/* --- Netzwerkadressen ---------------------------------------------------------------- */

static gboolean
is_private_ipv4 (guint32 host_order)
{
	return (host_order >> 24) == 10 ||
	       (host_order >> 20) == (172u << 4 | 1) ||   /* 172.16.0.0/12 */
	       (host_order >> 16) == (192u << 8 | 168);
}

/* Private IPv4-Adressen aktiver Schnittstellen, ohne virtuelle (Docker, VM, Tunnel). Adressen eines USB-Kabelnetzes
 * (USB-Tethering vom Handy) stehen vorn, damit der Austausch sie zuerst nimmt, wenn das Kabel steckt. */
static GPtrArray *
local_addresses (void)
{
	GPtrArray *list = g_ptr_array_new_with_free_func (g_free);
	guint cable_count = 0;
	struct ifaddrs *all = NULL, *it;

	if (getifaddrs (&all) != 0) {
		return list;
	}
	for (it = all; it != NULL; it = it->ifa_next) {
		struct sockaddr_in *sin;
		gchar text[INET_ADDRSTRLEN];

		if (it->ifa_addr == NULL || it->ifa_addr->sa_family != AF_INET ||
		    (it->ifa_flags & IFF_UP) == 0 || (it->ifa_flags & IFF_LOOPBACK) != 0 ||
		    g_str_has_prefix (it->ifa_name, "docker") || g_str_has_prefix (it->ifa_name, "br-") ||
		    g_str_has_prefix (it->ifa_name, "veth") || g_str_has_prefix (it->ifa_name, "virbr") ||
		    g_str_has_prefix (it->ifa_name, "tun") || g_str_has_prefix (it->ifa_name, "tap")) {
			continue;
		}
		sin = (struct sockaddr_in *) it->ifa_addr;
		if (!is_private_ipv4 (ntohl (sin->sin_addr.s_addr))) {
			continue;
		}
		if (inet_ntop (AF_INET, &sin->sin_addr, text, sizeof text) != NULL) {
			if (nolphin_phone_is_cable_interface (it->ifa_name)) {
				g_ptr_array_insert (list, (gint) cable_count++, g_strdup (text));
			} else {
				g_ptr_array_add (list, g_strdup (text));
			}
		}
	}
	freeifaddrs (all);
	return list;
}

/* Gehört diese Adresse zu einem USB-Kabelnetz? */
static gboolean
address_is_cable (const gchar *address)
{
	struct ifaddrs *all = NULL, *it;
	gboolean found = FALSE;

	if (address == NULL || getifaddrs (&all) != 0) {
		return FALSE;
	}
	for (it = all; it != NULL && !found; it = it->ifa_next) {
		gchar text[INET_ADDRSTRLEN];

		if (it->ifa_addr == NULL || it->ifa_addr->sa_family != AF_INET ||
		    !nolphin_phone_is_cable_interface (it->ifa_name)) {
			continue;
		}
		if (inet_ntop (AF_INET, &((struct sockaddr_in *) it->ifa_addr)->sin_addr, text, sizeof text) != NULL &&
		    strcmp (text, address) == 0) {
			found = TRUE;
		}
	}
	freeifaddrs (all);
	return found;
}

static void
phone_refresh_addresses (PhonePage *d)
{
	GPtrArray *list;
	gchar *previous = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (d->address_combo));
	guint i;
	gint active = 0;

	if (d->server != NULL) {
		g_free (previous);
		return;
	}
	list = local_addresses ();
	gtk_combo_box_text_remove_all (GTK_COMBO_BOX_TEXT (d->address_combo));
	for (i = 0; i < list->len; i++) {
		gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (d->address_combo), g_ptr_array_index (list, i));
		if (previous != NULL && strcmp (previous, g_ptr_array_index (list, i)) == 0) {
			active = (gint) i;
		}
	}
	gtk_combo_box_set_active (GTK_COMBO_BOX (d->address_combo), list->len > 0 ? active : -1);
	if (list->len == 0) {
		gtk_label_set_text (GTK_LABEL (d->status),
				    _("Kein lokales Netzwerk gefunden. Verbinde den PC mit dem WLAN oder LAN, in dem auch das Handy ist."));
	} else {
		gtk_label_set_text (GTK_LABEL (d->status), _("Gestoppt."));
	}
	gtk_widget_set_sensitive (d->start_button, list->len > 0);
	gtk_widget_set_visible (d->address_box, list->len > 1);
	g_ptr_array_free (list, TRUE);
	g_free (previous);
}

/* --- Anzeige ----------------------------------------------------------------------------- */

static void
phone_update_ui (PhonePage *d)
{
	gboolean running = d->server != NULL;

	gtk_button_set_label (GTK_BUTTON (d->start_button), running ? _("Beenden") : _("Starten"));
	gtk_widget_set_sensitive (d->address_combo, !running);
	gtk_widget_set_sensitive (d->copy_button, running);
	gtk_widget_set_sensitive (d->rotate_button, running);
	nolphin_window_set_phone_running (d->window, running);
	gtk_widget_queue_draw (d->qr_area);
}

static gboolean
on_qr_draw (GtkWidget *widget, cairo_t *cr, gpointer data)
{
	PhonePage *d = data;
	gint w = gtk_widget_get_allocated_width (widget);
	gint h = gtk_widget_get_allocated_height (widget);
	gint quiet = 4, x, y, cells, px, ox, oy;

	if (!d->has_qr || d->server == NULL) {
		GtkStyleContext *ctx = gtk_widget_get_style_context (widget);

		gtk_render_background (ctx, cr, 0, 0, w, h);
		return FALSE;
	}
	cells = d->qr.size + 2 * quiet;
	px = MAX (1, MIN (w, h) / cells);
	ox = (w - px * cells) / 2;
	oy = (h - px * cells) / 2;
	/* immer schwarz auf weiß, sonst lesen Handys den Code in dunklen Themes nicht */
	cairo_set_source_rgb (cr, 1, 1, 1);
	cairo_rectangle (cr, ox, oy, px * cells, px * cells);
	cairo_fill (cr);
	cairo_set_source_rgb (cr, 0, 0, 0);
	for (y = 0; y < d->qr.size; y++) {
		for (x = 0; x < d->qr.size; x++) {
			if (nolphin_qr_get (&d->qr, x, y)) {
				cairo_rectangle (cr, ox + (x + quiet) * px, oy + (y + quiet) * px, px, px);
			}
		}
	}
	cairo_fill (cr);
	return FALSE;
}

/* --- Server starten und beenden --------------------------------------------------------- */

static void
phone_stop (PhonePage *d, const gchar *status)
{
	if (d->timeout_id != 0) {
		g_source_remove (d->timeout_id);
		d->timeout_id = 0;
	}
	if (d->server != NULL) {
		live_servers = g_slist_remove (live_servers, d->server);
		nolphin_phone_server_stop (d->server);
		d->server = NULL;
	}
	d->has_qr = FALSE;
	gtk_entry_set_text (GTK_ENTRY (d->url_entry), "");
	gtk_label_set_text (GTK_LABEL (d->status), status != NULL ? status : _("Gestoppt."));
	phone_update_ui (d);
}

static void
phone_notify (const gchar *title, const gchar *body)
{
	GApplication *app = g_application_get_default ();
	GNotification *n;

	if (app == NULL) {
		return;
	}
	n = g_notification_new (title);
	g_notification_set_body (n, body);
	g_application_send_notification (app, "nolphin-phone", n);
	g_object_unref (n);
}

static gboolean
on_idle_check (gpointer data)
{
	PhonePage *d = data;
	gint64 idle;
	gint minutes;

	if (d->server == NULL) {
		d->timeout_id = 0;
		return G_SOURCE_REMOVE;
	}
	idle = (g_get_monotonic_time () - nolphin_phone_server_get_last_activity (d->server)) / G_USEC_PER_SEC;
	minutes = setting_int (NOLPHIN_PREFERENCES_PHONE_IDLE_MINUTES, 15);
	if (idle >= (gint64) minutes * 60) {
		d->timeout_id = 0;
		gchar *text = g_strdup_printf (_("Es kam %d Minuten lang keine Anfrage vom Handy."), minutes);
		gchar *status = g_strdup_printf (_("Automatisch beendet: %s"), text);

		phone_stop (d, status);
		phone_notify (_("Handy-Austausch beendet"), text);
		g_free (text);
		g_free (status);
		return G_SOURCE_REMOVE;
	}
	return G_SOURCE_CONTINUE;
}

/* Läuft im Hauptthread und gehört zu einer Datei, die das Handy geschickt hat. */
static gboolean
received_idle (gpointer data)
{
	ReceivedEvent *e = data;
	PhonePage *d = e->shared->page;

	if (d != NULL && d->server != NULL) {
		GtkTreeIter it;
		GDateTime *now = g_date_time_new_now_local ();
		gchar *when = g_date_time_format (now, "%H:%M:%S");
		gchar *size = g_format_size (e->size);

		gtk_list_store_insert (d->store, &it, 0);
		gtk_list_store_set (d->store, &it, COL_TIME, when, COL_NAME, e->name, COL_SIZE, size, -1);
		{
			gchar *msg = g_strdup_printf (_("Empfangen: %s"), e->name);

			gtk_label_set_text (GTK_LABEL (d->status), msg);
			phone_notify (_("Datei vom Handy empfangen"), e->name);
			g_free (msg);
		}
		g_free (when);
		g_free (size);
		g_date_time_unref (now);
	}
	shared_unref (e->shared);
	g_free (e->name);
	g_free (e);
	return G_SOURCE_REMOVE;
}

/* Arbeitsthread des Servers: nichts anfassen, nur an den Hauptthread übergeben. */
static void
on_file_received (const gchar *name, goffset size, gpointer user_data)
{
	ReceivedEvent *e = g_new0 (ReceivedEvent, 1);

	e->shared = shared_ref (user_data);
	e->name = g_strdup (name);
	e->size = size;
	g_idle_add (received_idle, e);
}

/* Verbindungslink, Format Version 2:
 *   nolphin://<Adresse>:<Port>/?t=<Schlüssel, 32 Hex-Zeichen>&v=2&f=<Fingerabdruck, 22 Zeichen base64url>
 * Er steht im QR-Code und im Eingabefeld (siehe docs/handy-austausch-api.md). Gescannt wird in der Handy-App; ihre
 * eigene Kamera liest den Code und verbindet sofort. Das Schema nolphin:// sorgt dafür, dass kein Browser den Link
 * öffnet (er hätte nur ein selbst erstelltes Zertifikat und keine Seite). Kamera-Apps anderer Hersteller zeigen den
 * Link nur als Text oder öffnen ihn nicht in der App; die App liest ihn auch aus der Zwischenablage. Sie verbindet sich
 * über https mit genau dem Zertifikat dieses Fingerabdrucks. Der Ablauf (e=) steht nicht im Link: Er würde den QR-Code
 * über die größte darstellbare Größe treiben, und der Server prüft ihn ohnehin. */
static void
phone_refresh_url (PhonePage *d)
{
	gchar *token = nolphin_phone_server_dup_token (d->server);
	gchar *fp = nolphin_phone_server_dup_fingerprint (d->server);
	gchar *url = g_strdup_printf ("nolphin://%s:%u/?t=%s&v=2&f=%s", d->address,
				      nolphin_phone_server_get_port (d->server), token, fp);

	d->has_qr = nolphin_qr_encode (url, -1, &d->qr);
	gtk_entry_set_text (GTK_ENTRY (d->url_entry), url);
	gtk_widget_queue_draw (d->qr_area);
	g_free (token);
	g_free (fp);
	g_free (url);
}

static void
phone_start (PhonePage *d)
{
	GError *error = NULL;
	gchar *address = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (d->address_combo));

	if (address == NULL) {
		gtk_label_set_text (GTK_LABEL (d->status), _("Keine Netzwerkadresse ausgewählt."));
		return;
	}
	if (!nolphin_phone_ensure_folders ()) {
		gtk_label_set_text (GTK_LABEL (d->status), _("Der Ordner „Handy“ im persönlichen Ordner kann nicht angelegt werden."));
		g_free (address);
		return;
	}
	{
		NolphinPhoneServerOptions options = { 0 };
		guint16 saved_port = 0;
		gchar *saved_token = NULL, *saved_cert = NULL, *saved_key = NULL;
		gint64 saved_expires = 0;

		if (session_load (address, &saved_port, &saved_token, &saved_expires, &saved_cert, &saved_key)) {
			options.port = saved_port;
			options.resume_token = saved_token;
			options.resume_expires = saved_expires;
			options.resume_cert_pem = saved_cert;
			options.resume_key_pem = saved_key;
		}
		options.listen_all = TRUE;   /* WLAN und USB-Kabelnetz (USB-Tethering) mit derselben Sitzung */
		options.token_lifetime = (gint64) setting_int (NOLPHIN_PREFERENCES_PHONE_TOKEN_HOURS, 12) * 3600;
		options.max_upload = (goffset) setting_int (NOLPHIN_PREFERENCES_PHONE_MAX_UPLOAD_MB, 2048) * 1024 * 1024;
		d->server = nolphin_phone_server_start (address, d->inbox, d->share, on_file_received,
							shared_ref (d->shared), shared_unref, &options, &error);
		g_free (saved_token);
		g_free (saved_cert);
		if (saved_key != NULL) {
			memset (saved_key, 0, strlen (saved_key));
		}
		g_free (saved_key);
	}
	if (d->server == NULL) {
		shared_unref (d->shared);   /* der Server hat user_data bei einem Fehler nicht übernommen */
		phone_stop (d, error->message);
		g_clear_error (&error);
		g_free (address);
		return;
	}
	g_free (d->address);
	d->address = address;
	live_servers = g_slist_prepend (live_servers, d->server);
	session_save (d->address, d->server);
	phone_refresh_url (d);
	if (address_is_cable (d->address)) {
		gtk_label_set_text (GTK_LABEL (d->status),
				    d->has_qr ? _("Läuft über das USB-Kabel, verschlüsselt. Code mit der Handy-App scannen.")
					      : _("Läuft über das USB-Kabel, verschlüsselt. Link in die Handy-App einfügen."));
	} else {
		gtk_label_set_text (GTK_LABEL (d->status),
				    d->has_qr ? _("Läuft, verschlüsselt. Code mit der Handy-App scannen.")
					      : _("Läuft, verschlüsselt. Link in die Handy-App einfügen."));
	}
	d->timeout_id = g_timeout_add_seconds (30, on_idle_check, d);
	phone_update_ui (d);
}

/* --- Bedienelemente ----------------------------------------------------------------------- */

static void
on_start_clicked (GtkButton *button, gpointer data)
{
	PhonePage *d = data;

	if (d->server != NULL) {
		session_clear ();
		phone_stop (d, NULL);
	} else {
		phone_start (d);
	}
}

static void
on_rotate_clicked (GtkButton *button, gpointer data)
{
	PhonePage *d = data;
	GError *error = NULL;

	if (d->server == NULL) {
		return;
	}
	if (nolphin_phone_server_rotate_token (d->server, &error)) {
		session_save (d->address, d->server);
		phone_refresh_url (d);
		gtk_label_set_text (GTK_LABEL (d->status),
				    _("Neuer QR-Code. Der alte ist ungültig."));
	} else {
		gtk_label_set_text (GTK_LABEL (d->status), error->message);
		g_clear_error (&error);
	}
}

static void
on_copy_clicked (GtkButton *button, gpointer data)
{
	PhonePage *d = data;

	gtk_clipboard_set_text (gtk_clipboard_get (GDK_SELECTION_CLIPBOARD), gtk_entry_get_text (GTK_ENTRY (d->url_entry)), -1);
}

static void
phone_page_free (gpointer data)
{
	PhonePage *d = data;

	/* Die Bedienelemente sind hier schon zerstört: nur Server und Zeitgeber beenden. */
	if (d->timeout_id != 0) {
		g_source_remove (d->timeout_id);
	}
	if (d->server != NULL) {
		live_servers = g_slist_remove (live_servers, d->server);
		nolphin_phone_server_stop (d->server);
	}
	d->shared->page = NULL;
	shared_unref (d->shared);
	g_clear_object (&d->inbox);
	g_clear_object (&d->share);
	g_free (d->address);
	g_free (d);
}

GtkWidget *
nolphin_phone_page_new (NolphinWindow *window)
{
	PhonePage *d = g_new0 (PhonePage, 1);
	GtkWidget *outer, *scroller, *row, *tree, *tree_scroller, *label, *hint;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	gchar *path;

	d->window = window;
	d->shared = g_new0 (PhoneShared, 1);
	d->shared->ref_count = 1;
	d->shared->page = d;
	path = nolphin_phone_inbox_path ();
	d->inbox = g_file_new_for_path (path);
	g_free (path);
	path = nolphin_phone_outbox_path ();
	d->share = g_file_new_for_path (path);
	g_free (path);
	d->store = gtk_list_store_new (COL_N, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

	outer = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
	gtk_container_set_border_width (GTK_CONTAINER (outer), 12);
	gtk_box_pack_start (GTK_BOX (outer), nolphin_tools_back_button_new (window), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (outer), nolphin_tools_heading_new (_("Handy-Austausch")), FALSE, FALSE, 0);

	d->status = gtk_label_new ("");
	gtk_label_set_line_wrap (GTK_LABEL (d->status), TRUE);
	gtk_label_set_xalign (GTK_LABEL (d->status), 0.0);
	gtk_box_pack_start (GTK_BOX (outer), d->status, FALSE, FALSE, 0);

	d->start_button = gtk_button_new_with_label (_("Starten"));
	gtk_style_context_add_class (gtk_widget_get_style_context (d->start_button), "suggested-action");
	g_signal_connect (d->start_button, "clicked", G_CALLBACK (on_start_clicked), d);
	gtk_box_pack_start (GTK_BOX (outer), d->start_button, FALSE, FALSE, 0);

	/* Netzwerkadresse: nur sichtbar, wenn der PC in mehreren Netzen hängt */
	d->address_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
	gtk_widget_set_no_show_all (d->address_box, TRUE);
	label = gtk_label_new (_("Netzwerkadresse des PCs"));
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_style_context_add_class (gtk_widget_get_style_context (label), "dim-label");
	d->address_combo = gtk_combo_box_text_new ();
	gtk_box_pack_start (GTK_BOX (d->address_box), label, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (d->address_box), d->address_combo, FALSE, FALSE, 0);
	gtk_widget_show_all (d->address_box);
	gtk_box_pack_start (GTK_BOX (outer), d->address_box, FALSE, FALSE, 0);

	d->qr_area = gtk_drawing_area_new ();
	gtk_widget_set_size_request (d->qr_area, 320, 320);   /* 6 Bildpunkte je Feld bei Version 6: besser lesbar für die Kamera */
	gtk_widget_set_halign (d->qr_area, GTK_ALIGN_CENTER);
	g_signal_connect (d->qr_area, "draw", G_CALLBACK (on_qr_draw), d);
	gtk_box_pack_start (GTK_BOX (outer), d->qr_area, FALSE, FALSE, 0);

	row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	d->url_entry = gtk_entry_new ();
	gtk_editable_set_editable (GTK_EDITABLE (d->url_entry), FALSE);
	d->copy_button = gtk_button_new_from_icon_name ("edit-copy-symbolic", GTK_ICON_SIZE_BUTTON);
	gtk_widget_set_tooltip_text (d->copy_button, _("Link kopieren"));
	gtk_entry_set_width_chars (GTK_ENTRY (d->url_entry), 8);
	g_signal_connect (d->copy_button, "clicked", G_CALLBACK (on_copy_clicked), d);
	d->rotate_button = gtk_button_new_from_icon_name ("view-refresh-symbolic", GTK_ICON_SIZE_BUTTON);
	gtk_widget_set_tooltip_text (d->rotate_button, _("Neuen QR-Code erzeugen (der alte wird sofort ungültig)"));
	g_signal_connect (d->rotate_button, "clicked", G_CALLBACK (on_rotate_clicked), d);
	gtk_box_pack_start (GTK_BOX (row), d->url_entry, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (row), d->copy_button, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (row), d->rotate_button, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (outer), row, FALSE, FALSE, 0);

	label = nolphin_tools_heading_new (_("Empfangen"));
	gtk_box_pack_start (GTK_BOX (outer), label, FALSE, FALSE, 0);
	tree = gtk_tree_view_new_with_model (GTK_TREE_MODEL (d->store));
	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Zeit"), renderer, "text", COL_TIME, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);
	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer, "ellipsize", PANGO_ELLIPSIZE_MIDDLE, NULL);
	column = gtk_tree_view_column_new_with_attributes (_("Datei"), renderer, "text", COL_NAME, NULL);
	gtk_tree_view_column_set_expand (column, TRUE);
	gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);
	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Größe"), renderer, "text", COL_SIZE, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);
	tree_scroller = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (tree_scroller), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_widget_set_size_request (tree_scroller, -1, 120);
	gtk_container_add (GTK_CONTAINER (tree_scroller), tree);
	gtk_box_pack_start (GTK_BOX (outer), tree_scroller, TRUE, TRUE, 0);

	hint = gtk_label_new (_("Aufs Handy: Dateien auf „Handy“ in der Seitenleiste ziehen oder Rechtsklick ▸ An Handy senden. "
				"Den QR-Code nur dem eigenen Handy zeigen."));
	gtk_label_set_line_wrap (GTK_LABEL (hint), TRUE);
	gtk_label_set_xalign (GTK_LABEL (hint), 0.0);
	gtk_style_context_add_class (gtk_widget_get_style_context (hint), "dim-label");
	gtk_box_pack_start (GTK_BOX (outer), hint, FALSE, FALSE, 0);

	scroller = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroller), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (scroller), outer);
	g_object_set_data_full (G_OBJECT (scroller), "phone-data", d, phone_page_free);

	phone_refresh_addresses (d);
	phone_update_ui (d);
	return scroller;
}

void
nolphin_phone_page_open (GtkWidget *page, gboolean autostart)
{
	PhonePage *d = page != NULL ? g_object_get_data (G_OBJECT (page), "phone-data") : NULL;

	if (d == NULL) {
		return;
	}
	phone_refresh_addresses (d);
	if (!autostart) {
		return;
	}
	if (d->server == NULL && gtk_combo_box_get_active (GTK_COMBO_BOX (d->address_combo)) >= 0) {
		phone_start (d);
	}
	/* Die Hauptansicht zeigt ~/Handy mit dem Unterordner "Vom Handy". */
	if (nolphin_phone_ensure_folders ()) {
		gchar *base = nolphin_phone_base_path ();
		GFile *folder = g_file_new_for_path (base);

		nolphin_window_go_to (d->window, folder);
		g_object_unref (folder);
		g_free (base);
	}
}

/* Menü Datei ▸ Handy ▸ Neuen QR-Code erzeugen */
void
nolphin_phone_page_rotate (GtkWidget *page)
{
	PhonePage *d = page != NULL ? g_object_get_data (G_OBJECT (page), "phone-data") : NULL;

	if (d != NULL) {
		on_rotate_clicked (NULL, d);
	}
}

/* Menü Datei ▸ Handy ▸ Austausch beenden */
void
nolphin_phone_page_stop (GtkWidget *page)
{
	PhonePage *d = page != NULL ? g_object_get_data (G_OBJECT (page), "phone-data") : NULL;

	if (d != NULL && d->server != NULL) {
		session_clear ();
		phone_stop (d, NULL);
	}
}
