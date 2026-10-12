/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nolphin-phone-tls.c: Zertifikat und Fingerabdruck für den verschlüsselten Handy-Austausch
 */

#include <config.h>

#include "nolphin-phone-tls.h"

#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include <string.h>

/* Das Zertifikat gilt nur für die Dauer einer Sitzung (Schlüssel: höchstens 12 Stunden), mit Reserve. */
#define CERT_DAYS "3"

gboolean
nolphin_phone_tls_available (GError **error)
{
	if (!g_tls_backend_supports_tls (g_tls_backend_get_default ())) {
		g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
				     _("Dieses System kann kein TLS (das Paket glib-networking fehlt). "
				       "Der Handy-Austausch läuft nur verschlüsselt und startet deshalb nicht."));
		return FALSE;
	}
	return TRUE;
}

static void
remove_dir (const gchar *dir)
{
	const gchar *names[] = { "key.pem", "cert.pem", NULL };
	gint i;

	for (i = 0; names[i] != NULL; i++) {
		gchar *p = g_build_filename (dir, names[i], NULL);

		g_unlink (p);
		g_free (p);
	}
	g_rmdir (dir);
}

gboolean
nolphin_phone_tls_generate (const gchar *ip_address, gchar **cert_pem, gchar **key_pem, GError **error)
{
	GInetAddress *inet;
	gchar *openssl, *dir, *cert_path, *key_path, *san, *stderr_text = NULL;
	GSubprocess *proc;
	gboolean ok = FALSE;

	g_return_val_if_fail (cert_pem != NULL && key_pem != NULL, FALSE);
	*cert_pem = NULL;
	*key_pem = NULL;

	/* nur eine echte IPv4-Adresse darf in den Aufruf */
	inet = ip_address != NULL ? g_inet_address_new_from_string (ip_address) : NULL;
	if (inet == NULL || g_inet_address_get_family (inet) != G_SOCKET_FAMILY_IPV4) {
		g_clear_object (&inet);
		g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
			     _("Ungültige Netzwerkadresse: %s"), ip_address != NULL ? ip_address : "");
		return FALSE;
	}
	g_object_unref (inet);

	openssl = g_find_program_in_path ("openssl");
	if (openssl == NULL) {
		g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
				     _("Das Programm openssl fehlt. Der Handy-Austausch läuft nur verschlüsselt und "
				       "startet deshalb nicht. Installieren Sie das Paket openssl."));
		return FALSE;
	}
	dir = g_dir_make_tmp ("nolphin-tls-XXXXXX", error);   /* Rechte 0700 */
	if (dir == NULL) {
		g_free (openssl);
		return FALSE;
	}
	cert_path = g_build_filename (dir, "cert.pem", NULL);
	key_path = g_build_filename (dir, "key.pem", NULL);
	san = g_strdup_printf ("subjectAltName=IP:%s", ip_address);

	proc = g_subprocess_new (G_SUBPROCESS_FLAGS_STDOUT_SILENCE | G_SUBPROCESS_FLAGS_STDERR_PIPE, error,
				 openssl, "req", "-x509", "-newkey", "ec", "-pkeyopt", "ec_paramgen_curve:prime256v1",
				 "-nodes", "-days", CERT_DAYS, "-subj", "/CN=nolphin", "-addext", san,
				 "-keyout", key_path, "-out", cert_path, NULL);
	if (proc != NULL) {
		if (g_subprocess_communicate_utf8 (proc, NULL, NULL, NULL, &stderr_text, error)
		    && g_subprocess_get_successful (proc)) {
			gchar *c = NULL, *k = NULL;

			if (g_file_get_contents (cert_path, &c, NULL, NULL) && g_file_get_contents (key_path, &k, NULL, NULL)
			    && g_str_has_prefix (c, "-----BEGIN CERTIFICATE-----") && strstr (k, "PRIVATE KEY") != NULL) {
				*cert_pem = c;
				*key_pem = k;
				ok = TRUE;
			} else {
				g_free (c);
				g_free (k);
				g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
						     _("openssl hat kein Zertifikat geliefert."));
			}
		} else if (error != NULL && *error == NULL) {
			g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
				     _("openssl konnte das Zertifikat nicht erzeugen: %s"),
				     stderr_text != NULL ? stderr_text : "");
		}
		g_object_unref (proc);
	}
	remove_dir (dir);
	g_free (stderr_text);
	g_free (san);
	g_free (key_path);
	g_free (cert_path);
	g_free (dir);
	g_free (openssl);
	return ok;
}

gchar *
nolphin_phone_tls_fingerprint (GTlsCertificate *certificate)
{
	GByteArray *der = NULL;
	GChecksum *sum;
	guint8 digest[32];
	gsize len = sizeof digest;
	gchar *b64;
	gsize i, out = 0;

	if (certificate == NULL) {
		return NULL;
	}
	g_object_get (certificate, "certificate", &der, NULL);
	if (der == NULL || der->len == 0) {
		if (der != NULL) {
			g_byte_array_unref (der);
		}
		return NULL;
	}
	sum = g_checksum_new (G_CHECKSUM_SHA256);
	g_checksum_update (sum, der->data, der->len);
	g_checksum_get_digest (sum, digest, &len);
	g_checksum_free (sum);
	g_byte_array_unref (der);

	b64 = g_base64_encode (digest, 16);   /* 24 Zeichen, davon 2 "=" am Ende */
	for (i = 0; b64[i] != '\0' && b64[i] != '='; i++) {
		b64[out++] = b64[i] == '+' ? '-' : b64[i] == '/' ? '_' : b64[i];
	}
	b64[out] = '\0';
	return b64;
}

gchar *
nolphin_phone_tls_fingerprint_of_pem (const gchar *cert_pem)
{
	GTlsCertificate *c;
	gchar *fp;

	if (cert_pem == NULL) {
		return NULL;
	}
	c = g_tls_certificate_new_from_pem (cert_pem, -1, NULL);
	fp = nolphin_phone_tls_fingerprint (c);
	g_clear_object (&c);
	return fp;
}
