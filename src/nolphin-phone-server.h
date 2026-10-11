/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nolphin-phone-server.h: kleiner HTTP-Server für den Handy-Austausch
 *
 * Reines GIO, keine Bibliothek. Das Handy öffnet den Link (per QR-Code) im
 * Browser, lädt Dateien in den Eingangsordner hoch und kann Dateien aus dem
 * Freigabeordner herunterladen. Jede Anfrage braucht den Schlüssel aus dem
 * Link; es werden nur Verbindungen aus privaten Netzen angenommen.
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

/* Höchste angenommene Dateigröße eines Uploads (2 GiB). */
#define NOLPHIN_PHONE_MAX_UPLOAD ((goffset) 2 * 1024 * 1024 * 1024)

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
						GError                  **error);

guint16      nolphin_phone_server_get_port          (NolphinPhoneServer *server);
/* Aktueller Schlüssel (neu zugewiesen, mit g_free () freizugeben). */
gchar       *nolphin_phone_server_dup_token         (NolphinPhoneServer *server);
/* Unix-Zeit (Sekunden), ab der der aktuelle Schlüssel nicht mehr gilt. */
gint64       nolphin_phone_server_get_expires       (NolphinPhoneServer *server);
/* Erzeugt einen neuen Schlüssel; der alte ist sofort ungültig (Widerruf). */
gboolean     nolphin_phone_server_rotate_token      (NolphinPhoneServer *server,
						     GError            **error);
/* g_get_monotonic_time () der letzten gültigen Anfrage (oder des Starts). */
gint64       nolphin_phone_server_get_last_activity (NolphinPhoneServer *server);
/* Beendet den Server; @received wird danach nicht mehr aufgerufen. */
void         nolphin_phone_server_stop              (NolphinPhoneServer *server);

/* Bereinigt einen vom Handy gelieferten Dateinamen: nur der letzte
 * Pfadteil, keine Steuerzeichen, kein führender Punkt, höchstens 200 Bytes.
 * Gibt nie NULL zurück ("Datei" als Ersatz). Öffentlich für Tests. */
gchar *nolphin_phone_sanitize_name (const gchar *name);

G_END_DECLS

#endif /* NOLPHIN_PHONE_SERVER_H */
