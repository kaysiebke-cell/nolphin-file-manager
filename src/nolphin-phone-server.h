/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nolphin-phone-server.h: kleiner HTTPS-Server für den Handy-Austausch
 *
 * Reines GIO, keine Bibliothek. Die Handy-App verbindet sich (per QR-Code) verschlüsselt, prüft das
 * Zertifikat über den Fingerabdruck im Link, lädt Dateien in den Eingangsordner hoch und kann Dateien
 * aus dem Freigabeordner herunterladen. Jede Anfrage braucht den Schlüssel aus dem Link; es werden nur
 * Verbindungen aus privaten Netzen angenommen. Es gibt keinen unverschlüsselten Weg.
 */

#ifndef NOLPHIN_PHONE_SERVER_H
#define NOLPHIN_PHONE_SERVER_H

#include <gio/gio.h>

G_BEGIN_DECLS

typedef struct _NolphinPhoneServer NolphinPhoneServer;

/* Wird aus einem Arbeitsthread aufgerufen, nicht aus der Hauptschleife.
 * @saved_name ist der endgültige Dateiname im Eingangsordner. */
typedef void (*NolphinPhoneReceivedFunc) (const gchar *saved_name,
					  goffset      size,
					  gpointer     user_data);

/* Vorgabe für die größte angenommene Datei eines Uploads (2 GiB); einstellbar über GSettings. */
#define NOLPHIN_PHONE_MAX_UPLOAD ((goffset) 2 * 1024 * 1024 * 1024)

/* Einstellbare Werte (aus GSettings); 0 bedeutet Vorgabe. */
typedef struct {
	gint64  token_lifetime;   /* Gültigkeit des Schlüssels in Sekunden (Vorgabe 12 Stunden) */
	goffset max_upload;       /* größte angenommene Datei in Bytes */
	guint16 port;             /* gewünschter Port (0: vom System wählen; ist er belegt, wird ein anderer gewählt) */
	const gchar *resume_token;  /* früherer Schlüssel (32 Hex-Zeichen) oder NULL: dann wird ein neuer erzeugt */
	gint64  resume_expires;   /* Ablauf des früheren Schlüssels (Unix-Zeit); nur gültig, wenn noch in der Zukunft */
	const gchar *resume_cert_pem;  /* früheres Zertifikat und Schlüssel (PEM): nur zusammen mit resume_token gültig, */
	const gchar *resume_key_pem;   /* sonst werden beide neu erzeugt */
} NolphinPhoneServerOptions;

/* Startet den Server auf @bind_address (IPv4, Port wird vom System gewählt).
 * @user_data_free wird aufgerufen, sobald kein Arbeitsthread @user_data mehr
 * braucht (nach nolphin_phone_server_stop ()). @inbox muss ein lokaler Ordner sein (wird bei Bedarf angelegt), @share ein
 * lokaler Ordner oder NULL (dann gibt es keine Downloads). */
NolphinPhoneServer *nolphin_phone_server_start (const gchar              *bind_address,
						GFile                    *inbox,
						GFile                    *share,
						NolphinPhoneReceivedFunc  received,
						gpointer                  user_data,
						GDestroyNotify            user_data_free,
						const NolphinPhoneServerOptions *options,
						GError                  **error);

guint16      nolphin_phone_server_get_port          (NolphinPhoneServer *server);
/* Fingerabdruck des aktuellen Zertifikats (22 Zeichen, steht im Link als f=) und das Zertifikat samt
 * Schlüssel als PEM (zum Merken der Sitzung). Neu zugewiesen, mit g_free () freizugeben. */
gchar       *nolphin_phone_server_dup_fingerprint   (NolphinPhoneServer *server);
gchar       *nolphin_phone_server_dup_certificate_pem (NolphinPhoneServer *server);
gchar       *nolphin_phone_server_dup_key_pem       (NolphinPhoneServer *server);
/* Aktueller Schlüssel (neu zugewiesen, mit g_free () freizugeben). */
gchar       *nolphin_phone_server_dup_token         (NolphinPhoneServer *server);
/* Unix-Zeit (Sekunden), ab der der aktuelle Schlüssel nicht mehr gilt. */
gint64       nolphin_phone_server_get_expires       (NolphinPhoneServer *server);
/* Erzeugt einen neuen Schlüssel und ein neues Zertifikat; das Alte ist sofort ungültig (Widerruf). */
gboolean     nolphin_phone_server_rotate_token      (NolphinPhoneServer *server,
						     GError            **error);
/* g_get_monotonic_time () der letzten gültigen Anfrage (oder des Starts). */
gint64       nolphin_phone_server_get_last_activity (NolphinPhoneServer *server);
/* g_get_monotonic_time () der letzten Anfrage der Handy-App am Wartepunkt (/wait) oder 0, wenn noch keine kam. */
gint64       nolphin_phone_server_get_last_app      (NolphinPhoneServer *server);
/* Beendet den Server; @received wird danach nicht mehr aufgerufen. */
void         nolphin_phone_server_stop              (NolphinPhoneServer *server);

/* Bereinigt einen vom Handy gelieferten Dateinamen: nur der letzte
 * Pfadteil, keine Steuerzeichen, kein führender Punkt, höchstens 200 Bytes.
 * Gibt nie NULL zurück ("Datei" als Ersatz). Öffentlich für Tests. */
gchar *nolphin_phone_sanitize_name (const gchar *name);

G_END_DECLS

#endif /* NOLPHIN_PHONE_SERVER_H */
