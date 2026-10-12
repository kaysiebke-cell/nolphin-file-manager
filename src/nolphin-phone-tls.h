/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nolphin-phone-tls.h: Zertifikat und Fingerabdruck für den verschlüsselten Handy-Austausch
 *
 * Nolphin erzeugt für den Austausch ein eigenes, selbstsigniertes Zertifikat (EC P-256). Der Fingerabdruck
 * steht im QR-Code; die Handy-App akzeptiert nur genau dieses Zertifikat ("Pinning"). Zum Erzeugen wird das
 * Programm openssl benutzt (mit festen Argumenten, ohne Shell).
 */

#ifndef NOLPHIN_PHONE_TLS_H
#define NOLPHIN_PHONE_TLS_H

#include <gio/gio.h>

G_BEGIN_DECLS

/* Länge des Fingerabdrucks in Zeichen (128 Bit, base64url ohne Auffüllung). */
#define NOLPHIN_PHONE_FINGERPRINT_LENGTH 22

/* Erzeugt ein Zertifikat und den privaten Schlüssel (PEM) für die IPv4-Adresse @ip_address.
 * Die Dateien liegen nur kurz in einem privaten Temporärordner und werden danach gelöscht.
 * Schlägt fehl (mit verständlicher Meldung), wenn openssl fehlt oder die Adresse ungültig ist. */
gboolean nolphin_phone_tls_generate (const gchar *ip_address, gchar **cert_pem, gchar **key_pem, GError **error);

/* Fingerabdruck eines Zertifikats: die ersten 16 Bytes von SHA-256 über die DER-Form, base64url ohne "=".
 * NULL bei ungültigen Daten. Mit g_free () freizugeben. */
gchar *nolphin_phone_tls_fingerprint (GTlsCertificate *certificate);
gchar *nolphin_phone_tls_fingerprint_of_pem (const gchar *cert_pem);

/* Prüft, ob dieses System TLS kann (glib-networking). */
gboolean nolphin_phone_tls_available (GError **error);

G_END_DECLS

#endif /* NOLPHIN_PHONE_TLS_H */
