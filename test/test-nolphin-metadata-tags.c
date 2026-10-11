/* METADATEN UND TAGS: prüft, dass die neuen Metadaten-Schlüssel
 * (NOLPHIN_METADATA_KEY_RATING für die Bewertung) korrekt in der
 * zentralen Schlüssel-Registrierung (nolphin-metadata.c) eingetragen
 * sind. nolphin_metadata_get_id() liefert für jeden *nicht*
 * registrierten Schlüssel denselben Wert (0) zurück - ein neuer
 * Schlüssel, der in nolphin-metadata.h definiert, aber in
 * used_metadata_names[] (nolphin-metadata.c) vergessen wird, würde
 * sonst unbemerkt mit jedem anderen vergessenen Schlüssel kollidieren
 * und falsche Werte lesen/schreiben. Das eigentliche Lesen/Schreiben
 * der Tags/Bewertung/Kommentare über GVFS (nolphin_file_set_keywords(),
 * nolphin_file_get_rating()/set_rating(), nolphin_file_get_comment()/
 * set_comment() in libnolphin-private/nolphin-file.c) braucht einen
 * laufenden gvfsd-metadata-Dienst über D-Bus und kann in dieser
 * Build-Umgebung nicht ausgeführt werden - siehe Änderungsprotokoll. */

#include <glib.h>
#include <libnolphin-private/nolphin-metadata.h>

static gint exit_code = 0;

static void
fail (const gchar *message)
{
	g_printerr ("FAIL: %s\n", message);
	exit_code = 1;
}

int
main (int argc, char **argv)
{
	guint id_rating, id_emblems, id_annotation, id_unknown;

	id_rating = nolphin_metadata_get_id (NOLPHIN_METADATA_KEY_RATING);
	if (id_rating == 0) {
		fail ("NOLPHIN_METADATA_KEY_RATING ist nicht in used_metadata_names[] eingetragen (id == 0)");
	} else {
		g_print ("PASS: NOLPHIN_METADATA_KEY_RATING ist registriert (id=%u)\n", id_rating);
	}

	id_emblems = nolphin_metadata_get_id (NOLPHIN_METADATA_KEY_EMBLEMS);
	if (id_emblems == 0) {
		fail ("NOLPHIN_METADATA_KEY_EMBLEMS ist nicht registriert (id == 0)");
	} else {
		g_print ("PASS: NOLPHIN_METADATA_KEY_EMBLEMS ist registriert (id=%u)\n", id_emblems);
	}

	id_annotation = nolphin_metadata_get_id (NOLPHIN_METADATA_KEY_ANNOTATION);
	if (id_annotation == 0) {
		fail ("NOLPHIN_METADATA_KEY_ANNOTATION ist nicht registriert (id == 0)");
	} else {
		g_print ("PASS: NOLPHIN_METADATA_KEY_ANNOTATION ist registriert (id=%u)\n", id_annotation);
	}

	if (id_rating == id_emblems || id_rating == id_annotation || id_emblems == id_annotation) {
		fail ("die drei Metadatenschlüssel kollidieren auf dieselbe id");
	} else {
		g_print ("PASS: Tags/Bewertung/Kommentar haben drei verschiedene ids, keine Kollision\n");
	}

	id_unknown = nolphin_metadata_get_id ("dieser-schluessel-existiert-nicht");
	if (id_unknown != 0) {
		fail ("ein unbekannter Schlüssel sollte id 0 liefern");
	} else {
		g_print ("PASS: ein unbekannter Schlüssel liefert korrekt id 0\n");
	}

	if (exit_code == 0) {
		g_print ("All metadata-tags registration tests passed.\n");
	}

	return exit_code;
}
