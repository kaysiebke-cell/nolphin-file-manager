/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nolphin-phone.c: Handy-Austausch (Panel im rechten Arbeitsbereich)
 *
 * Startet den kleinen Server aus nolphin-phone-server.c im lokalen Netz und
 * zeigt den Verbindungslink als QR-Code. Das Handy scannt ihn mit der
 * Kamera und tauscht Dateien im Browser aus. Keine App, keine Bibliothek.
 */

#include <config.h>

#include "nolphin-phone.h"
#include "nolphin-phone-server.h"
#include "nolphin-qr.h"
#include "nolphin-tools.h"

#include <glib/gi18n.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <string.h>

/* Ohne Anfrage vom Handy schaltet sich der Server nach dieser Zeit ab. */
#define IDLE_LIMIT_SECONDS (15 * 60)

enum { COL_TIME, COL_NAME, COL_SIZE, COL_N };

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
	GtkWidget *inbox_label;
	GtkWidget *inbox_button;
	GtkWidget *share_label;
	GtkWidget *share_button;
	GtkWidget *share_clear_button;
	GtkWidget *qr_area;
	GtkWidget *url_entry;
	GtkWidget *copy_button;
	GtkWidget *open_button;
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

/* Private IPv4-Adressen aktiver Schnittstellen, ohne virtuelle (Docker, VM, Tunnel). */
static GPtrArray *
local_addresses (void)
{
	GPtrArray *list = g_ptr_array_new_with_free_func (g_free);
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
			g_ptr_array_add (list, g_strdup (text));
		}
	}
	freeifaddrs (all);
	return list;
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
	gtk_widget_set_sensitive (d->inbox_button, !running);
	gtk_widget_set_sensitive (d->share_button, !running);
	gtk_widget_set_sensitive (d->share_clear_button, !running && d->share != NULL);
	gtk_widget_set_sensitive (d->copy_button, running);
	gtk_widget_queue_draw (d->qr_area);
}

static void
phone_set_labels (PhonePage *d)
{
	gchar *path = g_file_get_path (d->inbox);

	gtk_label_set_text (GTK_LABEL (d->inbox_label), path != NULL ? path : "");
	g_free (path);
	if (d->share != NULL) {
		path = g_file_get_path (d->share);
		gtk_label_set_text (GTK_LABEL (d->share_label), path != NULL ? path : "");
		g_free (path);
	} else {
		gtk_label_set_text (GTK_LABEL (d->share_label), _("(nichts freigegeben)"));
	}
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

	if (d->server == NULL) {
		d->timeout_id = 0;
		return G_SOURCE_REMOVE;
	}
	idle = (g_get_monotonic_time () - nolphin_phone_server_get_last_activity (d->server)) / G_USEC_PER_SEC;
	if (idle >= IDLE_LIMIT_SECONDS) {
		d->timeout_id = 0;
		phone_stop (d, _("Automatisch beendet: 15 Minuten lang keine Anfrage vom Handy."));
		phone_notify (_("Handy-Austausch beendet"), _("Es kam 15 Minuten lang keine Anfrage vom Handy."));
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

static void
phone_start (PhonePage *d)
{
	GError *error = NULL;
	gchar *address = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT (d->address_combo));
	gchar *url;

	if (address == NULL) {
		gtk_label_set_text (GTK_LABEL (d->status), _("Keine Netzwerkadresse ausgewählt."));
		return;
	}
	d->server = nolphin_phone_server_start (address, d->inbox, d->share, on_file_received,
						shared_ref (d->shared), shared_unref, &error);
	if (d->server == NULL) {
		shared_unref (d->shared);   /* der Server hat user_data bei einem Fehler nicht übernommen */
		phone_stop (d, error->message);
		g_clear_error (&error);
		g_free (address);
		return;
	}
	url = g_strdup_printf ("http://%s:%u/?t=%s", address, nolphin_phone_server_get_port (d->server),
			       nolphin_phone_server_get_token (d->server));
	d->has_qr = nolphin_qr_encode (url, -1, &d->qr);
	gtk_entry_set_text (GTK_ENTRY (d->url_entry), url);
	gtk_label_set_text (GTK_LABEL (d->status),
			    d->has_qr ? _("Läuft. Scanne den QR-Code mit der Kamera des Handys. Handy und PC müssen im selben WLAN sein.")
				      : _("Läuft. Der QR-Code konnte nicht erzeugt werden; öffne den Link unten im Browser des Handys."));
	d->timeout_id = g_timeout_add_seconds (30, on_idle_check, d);
	phone_update_ui (d);
	g_free (url);
	g_free (address);
}

/* --- Bedienelemente ----------------------------------------------------------------------- */

static void
on_start_clicked (GtkButton *button, gpointer data)
{
	PhonePage *d = data;

	if (d->server != NULL) {
		phone_stop (d, NULL);
	} else {
		phone_start (d);
	}
}

static GFile *
choose_folder (GtkWidget *parent_widget, const gchar *title, GFile *current)
{
	GtkWidget *toplevel = gtk_widget_get_toplevel (parent_widget);
	GtkWidget *chooser = gtk_file_chooser_dialog_new (title, GTK_IS_WINDOW (toplevel) ? GTK_WINDOW (toplevel) : NULL,
							  GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
							  _("_Abbrechen"), GTK_RESPONSE_CANCEL,
							  _("_Auswählen"), GTK_RESPONSE_ACCEPT, NULL);
	GFile *result = NULL;

	if (current != NULL && g_file_query_exists (current, NULL)) {
		gtk_file_chooser_set_file (GTK_FILE_CHOOSER (chooser), current, NULL);
	}
	if (gtk_dialog_run (GTK_DIALOG (chooser)) == GTK_RESPONSE_ACCEPT) {
		result = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (chooser));
	}
	gtk_widget_destroy (chooser);
	if (result != NULL && g_file_get_path (result) == NULL) {
		g_clear_object (&result);   /* nur lokale Ordner */
	}
	return result;
}

static void
on_inbox_choose (GtkButton *button, gpointer data)
{
	PhonePage *d = data;
	GFile *f = choose_folder (GTK_WIDGET (button), _("Eingangsordner wählen"), d->inbox);

	if (f != NULL) {
		g_object_unref (d->inbox);
		d->inbox = f;
		phone_set_labels (d);
	}
}

static void
on_share_choose (GtkButton *button, gpointer data)
{
	PhonePage *d = data;
	GFile *f = choose_folder (GTK_WIDGET (button), _("Freigabeordner wählen"), d->share);

	if (f != NULL) {
		g_clear_object (&d->share);
		d->share = f;
		phone_set_labels (d);
		phone_update_ui (d);
	}
}

static void
on_share_clear (GtkButton *button, gpointer data)
{
	PhonePage *d = data;

	g_clear_object (&d->share);
	phone_set_labels (d);
	phone_update_ui (d);
}

static void
on_copy_clicked (GtkButton *button, gpointer data)
{
	PhonePage *d = data;

	gtk_clipboard_set_text (gtk_clipboard_get (GDK_SELECTION_CLIPBOARD), gtk_entry_get_text (GTK_ENTRY (d->url_entry)), -1);
}

static void
on_open_inbox (GtkButton *button, gpointer data)
{
	PhonePage *d = data;

	if (g_file_query_exists (d->inbox, NULL)) {
		nolphin_window_go_to (d->window, d->inbox);
	} else {
		gtk_label_set_text (GTK_LABEL (d->status), _("Der Eingangsordner existiert noch nicht. Er wird beim Starten angelegt."));
	}
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
		nolphin_phone_server_stop (d->server);
	}
	d->shared->page = NULL;
	shared_unref (d->shared);
	g_clear_object (&d->inbox);
	g_clear_object (&d->share);
	g_free (d);
}

static GtkWidget *
folder_row (const gchar *caption, GtkWidget **out_label, GtkWidget **out_button, GtkWidget **out_extra,
	    GCallback choose_cb, GCallback extra_cb, PhonePage *d)
{
	GtkWidget *row = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
	GtkWidget *line = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	GtkWidget *cap = gtk_label_new (caption);
	GtkWidget *label = gtk_label_new ("");
	GtkWidget *b = gtk_button_new_with_label (_("Wählen …"));

	gtk_widget_set_halign (cap, GTK_ALIGN_START);
	gtk_label_set_line_wrap (GTK_LABEL (cap), TRUE);
	gtk_label_set_xalign (GTK_LABEL (cap), 0.0);
	gtk_style_context_add_class (gtk_widget_get_style_context (cap), "dim-label");
	gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_MIDDLE);
	gtk_label_set_width_chars (GTK_LABEL (label), 8);
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	g_signal_connect (b, "clicked", choose_cb, d);
	gtk_box_pack_start (GTK_BOX (line), label, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (line), b, FALSE, FALSE, 0);
	if (extra_cb != NULL) {
		GtkWidget *x = gtk_button_new_from_icon_name ("edit-clear-symbolic", GTK_ICON_SIZE_BUTTON);

		gtk_widget_set_tooltip_text (x, _("Freigabe entfernen"));
		g_signal_connect (x, "clicked", extra_cb, d);
		gtk_box_pack_start (GTK_BOX (line), x, FALSE, FALSE, 0);
		*out_extra = x;
	}
	gtk_box_pack_start (GTK_BOX (row), cap, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (row), line, FALSE, FALSE, 0);
	*out_label = label;
	*out_button = b;
	return row;
}

GtkWidget *
nolphin_phone_page_new (NolphinWindow *window)
{
	PhonePage *d = g_new0 (PhonePage, 1);
	GtkWidget *outer, *scroller, *intro, *row, *tree, *tree_scroller, *label;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	gchar *default_inbox;

	d->window = window;
	d->shared = g_new0 (PhoneShared, 1);
	d->shared->ref_count = 1;
	d->shared->page = d;
	default_inbox = g_build_filename (g_get_home_dir (), _("Handy-Eingang"), NULL);
	d->inbox = g_file_new_for_path (default_inbox);
	g_free (default_inbox);
	d->store = gtk_list_store_new (COL_N, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

	outer = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
	gtk_container_set_border_width (GTK_CONTAINER (outer), 12);
	gtk_box_pack_start (GTK_BOX (outer), nolphin_tools_back_button_new (window), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (outer), nolphin_tools_heading_new (_("Handy-Austausch")), FALSE, FALSE, 0);

	intro = gtk_label_new (_("Tauscht Bilder und Dateien mit dem Handy über das WLAN. Starte den Austausch und scanne den QR-Code mit der "
				 "Kamera des Handys. Auf dem Handy öffnet sich eine Seite im Browser; eine App ist nicht nötig. "
				 "Der Link enthält einen Schlüssel, nur wer ihn kennt, kommt hinein. Die Verbindung ist nicht verschlüsselt: "
				 "nur im eigenen Netz verwenden."));
	gtk_label_set_line_wrap (GTK_LABEL (intro), TRUE);
	gtk_label_set_xalign (GTK_LABEL (intro), 0.0);
	gtk_style_context_add_class (gtk_widget_get_style_context (intro), "dim-label");
	gtk_box_pack_start (GTK_BOX (outer), intro, FALSE, FALSE, 0);

	gtk_box_pack_start (GTK_BOX (outer),
			    folder_row (_("Eingangsordner (hierhin kommen Dateien vom Handy)"), &d->inbox_label, &d->inbox_button,
					NULL, G_CALLBACK (on_inbox_choose), NULL, d), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (outer),
			    folder_row (_("Freigabeordner (optional, Dateien zum Laden aufs Handy)"), &d->share_label, &d->share_button,
					&d->share_clear_button, G_CALLBACK (on_share_choose), G_CALLBACK (on_share_clear), d), FALSE, FALSE, 0);

	label = gtk_label_new (_("Netzwerkadresse des PCs"));
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_style_context_add_class (gtk_widget_get_style_context (label), "dim-label");
	gtk_box_pack_start (GTK_BOX (outer), label, FALSE, FALSE, 0);
	row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	d->address_combo = gtk_combo_box_text_new ();
	d->start_button = gtk_button_new_with_label (_("Starten"));
	gtk_style_context_add_class (gtk_widget_get_style_context (d->start_button), "suggested-action");
	g_signal_connect (d->start_button, "clicked", G_CALLBACK (on_start_clicked), d);
	gtk_box_pack_start (GTK_BOX (row), d->address_combo, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (row), d->start_button, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (outer), row, FALSE, FALSE, 0);

	d->status = gtk_label_new ("");
	gtk_label_set_line_wrap (GTK_LABEL (d->status), TRUE);
	gtk_label_set_xalign (GTK_LABEL (d->status), 0.0);
	gtk_box_pack_start (GTK_BOX (outer), d->status, FALSE, FALSE, 0);

	d->qr_area = gtk_drawing_area_new ();
	gtk_widget_set_size_request (d->qr_area, 240, 240);
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
	gtk_box_pack_start (GTK_BOX (row), d->url_entry, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (row), d->copy_button, FALSE, FALSE, 0);
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
	gtk_widget_set_size_request (tree_scroller, -1, 140);
	gtk_container_add (GTK_CONTAINER (tree_scroller), tree);
	gtk_box_pack_start (GTK_BOX (outer), tree_scroller, TRUE, TRUE, 0);

	d->open_button = gtk_button_new_with_label (_("Eingangsordner öffnen"));
	gtk_widget_set_halign (d->open_button, GTK_ALIGN_START);
	g_signal_connect (d->open_button, "clicked", G_CALLBACK (on_open_inbox), d);
	gtk_box_pack_start (GTK_BOX (outer), d->open_button, FALSE, FALSE, 0);

	scroller = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroller), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (scroller), outer);
	g_object_set_data_full (G_OBJECT (scroller), "phone-data", d, phone_page_free);

	phone_set_labels (d);
	phone_refresh_addresses (d);
	phone_update_ui (d);
	return scroller;
}

void
nolphin_phone_page_open (GtkWidget *page)
{
	PhonePage *d = page != NULL ? g_object_get_data (G_OBJECT (page), "phone-data") : NULL;

	if (d != NULL) {
		phone_refresh_addresses (d);
	}
}
