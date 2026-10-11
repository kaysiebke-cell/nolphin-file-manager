/* Prüft die Such-Operatoren (libnolphin-private/nolphin-search-expression.c):
 * UND (Leerzeichen, altes Verhalten), ODER, NICHT, -Begriff, exakte
 * Übereinstimmung in Anführungszeichen und Platzhalter. */

#include <glib.h>
#include <libnolphin-private/nolphin-search-expression.h>

static gint exit_code = 0;

static void
check (const gchar *query, const gchar *name, gboolean want)
{
	NolphinSearchExpression *expr = nolphin_search_expression_new (query);
	gboolean got = nolphin_search_expression_matches (expr, name);

	if (got != want) {
		g_printerr ("FAIL: Suche »%s« auf »%s«: %s statt %s\n", query, name,
			    got ? "Treffer" : "kein Treffer", want ? "Treffer" : "kein Treffer");
		exit_code = 1;
	}
	nolphin_search_expression_free (expr);
}

int
main (void)
{
	/* altes Verhalten */
	check ("bericht", "jahresbericht.txt", TRUE);
	check ("bericht", "notiz.txt", FALSE);
	check ("jahr bericht", "jahresbericht.txt", TRUE);
	check ("jahr foto", "jahresbericht.txt", FALSE);
	check ("", "irgendwas", TRUE);
	check ("*.txt", "a.txt", TRUE);
	check ("*.txt", "a.pdf", FALSE);

	/* ODER */
	check ("foto ODER bericht", "bericht.txt", TRUE);
	check ("foto oder bericht", "foto.png", TRUE);
	check ("foto OR bericht", "notiz.txt", FALSE);
	check ("a b ODER c", "ab.txt", TRUE);   /* UND bindet stärker */
	check ("a b ODER c", "a.txt", FALSE);
	check ("a b ODER c", "c.txt", TRUE);

	/* NICHT */
	check ("bericht NICHT entwurf", "bericht.txt", TRUE);
	check ("bericht NICHT entwurf", "bericht-entwurf.txt", FALSE);
	check ("bericht -entwurf", "bericht-entwurf.txt", FALSE);
	check ("bericht not entwurf", "bericht.txt", TRUE);
	check ("-entwurf", "bericht.txt", TRUE);
	check ("-entwurf", "entwurf.txt", FALSE);

	/* exakt */
	check ("\"bericht\"", "bericht", TRUE);
	check ("\"bericht\"", "jahresbericht", FALSE);
	check ("\"mein bericht.txt\"", "mein bericht.txt", TRUE);
	check ("\"mein bericht.txt\" ODER foto", "foto.png", TRUE);
	check ("-\"bericht\"", "bericht", FALSE);
	check ("-\"bericht\"", "jahresbericht", TRUE);

	return exit_code;
}
