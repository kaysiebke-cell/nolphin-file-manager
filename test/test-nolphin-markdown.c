/* Prüft den Markdown-Renderer (src/nolphin-markdown-view.c, §60.3) ohne
 * Display: Der Text wird in einen GtkTextBuffer gerendert; geprüft werden
 * der entstehende Klartext, die Tags an bestimmten Stellen und die
 * Link-Ziele. */

#include <gtk/gtk.h>
#include <glib/gstdio.h>
#include <string.h>

#include "../src/nolphin-markdown-view.h"

static gint exit_code = 0;

static void
fail (const gchar *message)
{
	g_printerr ("FAIL: %s\n", message);
	exit_code = 1;
}

static gchar *
buffer_text (GtkTextBuffer *buffer)
{
	GtkTextIter a, b;

	gtk_text_buffer_get_bounds (buffer, &a, &b);
	/* get_slice: Kind-Anker (horizontale Linien) erscheinen als U+FFFC und
	 * Offsets bleiben mit den Puffer-Offsets deckungsgleich. */
	return gtk_text_buffer_get_slice (buffer, &a, &b, TRUE);
}

/* Tag @name an der Stelle, an der @needle zuerst im Text steht? */
static gboolean
has_tag_at (GtkTextBuffer *buffer, const gchar *text, const gchar *needle, const gchar *tag_name)
{
	const gchar *pos = strstr (text, needle);
	GtkTextIter it;
	GtkTextTag *tag;

	if (pos == NULL)
		return FALSE;
	tag = gtk_text_tag_table_lookup (gtk_text_buffer_get_tag_table (buffer), tag_name);
	if (tag == NULL)
		return FALSE;
	gtk_text_buffer_get_iter_at_offset (buffer, &it, (gint) g_utf8_pointer_to_offset (text, pos));
	return gtk_text_iter_has_tag (&it, tag);
}

static const gchar *
href_at (GtkTextBuffer *buffer, const gchar *text, const gchar *needle)
{
	const gchar *pos = strstr (text, needle);
	GtkTextIter it;
	GSList *tags, *l;
	const gchar *href = NULL;

	if (pos == NULL)
		return NULL;
	gtk_text_buffer_get_iter_at_offset (buffer, &it, (gint) g_utf8_pointer_to_offset (text, pos));
	tags = gtk_text_iter_get_tags (&it);
	for (l = tags; l != NULL; l = l->next)
		if (nolphin_markdown_tag_get_href (l->data) != NULL)
			href = nolphin_markdown_tag_get_href (l->data);
	g_slist_free (tags);
	return href;
}

static void
expect_text (GtkTextBuffer *buffer, const gchar *markdown, const gchar *expected, const gchar *label)
{
	gchar *text;

	nolphin_markdown_render_to_buffer (buffer, markdown);
	text = buffer_text (buffer);
	if (g_strcmp0 (text, expected) != 0) {
		gchar *msg = g_strdup_printf ("%s: erwartet \"%s\", erhalten \"%s\"", label, expected, text);
		fail (msg);
		g_free (msg);
	}
	g_free (text);
}

static gchar *
anchor_at (GtkTextBuffer *buffer, gint offset)
{
	GtkTextIter it;
	GtkTextChildAnchor *anchor;

	gtk_text_buffer_get_iter_at_offset (buffer, &it, offset);
	anchor = gtk_text_iter_get_child_anchor (&it);
	return anchor != NULL ? nolphin_markdown_anchor_describe (anchor) : NULL;
}

static void
expect_anchor (GtkTextBuffer *buffer, const gchar *markdown, gint offset, const gchar *expected, const gchar *label)
{
	gchar *d;

	nolphin_markdown_render_to_buffer (buffer, markdown);
	d = anchor_at (buffer, offset);
	if (g_strcmp0 (d, expected) != 0) {
		gchar *msg = g_strdup_printf ("%s: erwartet \"%s\", erhalten \"%s\"", label,
					      expected != NULL ? expected : "(kein Anker)",
					      d != NULL ? d : "(kein Anker)");
		fail (msg);
		g_free (msg);
	}
	g_free (d);
}

/* Tabellen, Aufgabenlisten, Listenebenen, Überschriften (GitHub-Darstellung). */
static void
test_github_layout (GtkTextBuffer *buffer)
{
	gchar *text;

	/* Tabelle mit Ausrichtung und Inline-Markdown in den Zellen */
	expect_anchor (buffer, "| A | B | C |\n|:--|:-:|--:|\n| 1 | **2** | `3` |\n| x | y | z |",
		       0, "table:3x3:l,c,r:A|B|C/1|<b>2</b>|<tt>3</tt>/x|y|z", "Tabelle mit Ausrichtung");
	expect_anchor (buffer, "A | B\n--|--\n1 | 2", 0, "table:2x2:l,l:A|B/1|2", "Tabelle ohne äußere Striche");
	expect_anchor (buffer, "| A | B |\n|---|---|\n| nur eine |", 0, "table:2x2:l,l:A|B/nur eine|", "kurze Zeile wird aufgefüllt");
	expect_anchor (buffer, "| A | B |\n|---|---|\n| 1 | 2 | 3 |", 0, "table:2x2:l,l:A|B/1|2", "überzählige Zelle entfällt");
	expect_anchor (buffer, "| a \\| b | c |\n|---|---|\n| 1 | 2 |", 0, "table:2x2:l,l:a | b|c/1|2", "maskierter Strich");
	expect_anchor (buffer, "| [x](https://e.org) |\n|---|\n| 1 |", 0,
		       "table:2x1:l:<a href=\"https://e.org\">x</a>/1", "Link in Zelle");
	expect_anchor (buffer, "| A | B |\n|---|\n| 1 | 2 |", 0, NULL, "ungleiche Spaltenzahl ist keine Tabelle");
	expect_text (buffer, "Text\n\n| A |\n|---|\n| 1 |\n\nDanach", "Text\n\xEF\xBF\xBC\nDanach\n", "Tabelle zwischen Absätzen");

	/* Aufgabenliste */
	expect_anchor (buffer, "- [x] fertig", 0, "check:1", "angehaktes Kästchen");
	expect_anchor (buffer, "- [ ] offen", 0, "check:0", "leeres Kästchen");
	expect_text (buffer, "- [x] mit **Text**", "\xEF\xBF\xBC mit Text\n", "Kästchen mit Hervorhebung");

	/* Listenebenen unabhängig von der Einrückungstiefe */
	expect_text (buffer, "- a\n    - b\n        - c\n- d", "• a\n◦ b\n▪ c\n• d\n", "Ebenen bei 4 Leerzeichen");
	expect_text (buffer, "- a\n  - b\n- c", "• a\n◦ b\n• c\n", "Ebenen bei 2 Leerzeichen");
	expect_text (buffer, "- a\n\nAbsatz\n\n- b", "• a\nAbsatz\n• b\n", "Absatz beendet die Liste");

	/* Überschriften H1..H6 */
	nolphin_markdown_render_to_buffer (buffer, "# a\n## b\n### c\n#### d\n##### e\n###### f");
	text = buffer_text (buffer);
	{
		const gchar *names[] = { "h1", "h2", "h3", "h4", "h5", "h6" };
		const gchar *needles[] = { "a\n", "b\n", "c\n", "d\n", "e\n", "f\n" };
		guint k;

		for (k = 0; k < 6; k++)
			if (!has_tag_at (buffer, text, needles[k], names[k])) {
				gchar *msg = g_strdup_printf ("Überschrift %s ohne Tag", names[k]);
				fail (msg);
				g_free (msg);
			}
	}
	g_free (text);

	/* Code: Zeilen tragen codeblock, erste/letzte die Abstands-Tags */
	nolphin_markdown_render_to_buffer (buffer, "```\neins\nzwei\ndrei\n```");
	text = buffer_text (buffer);
	if (!has_tag_at (buffer, text, "eins", "code-first") || has_tag_at (buffer, text, "zwei", "code-first") ||
	    has_tag_at (buffer, text, "zwei", "code-last") || !has_tag_at (buffer, text, "drei", "code-last") ||
	    !has_tag_at (buffer, text, "zwei", "codeblock"))
		fail ("Codeblock: erste/letzte Zeile falsch getaggt");
	g_free (text);
}

/* Bilder (§60.3): lokale Bilder innerhalb des Basisordners werden zu
 * Ankern, alles andere bleibt Alternativtext. */
static void
test_images (GtkTextBuffer *buffer)
{
	gchar *dir = g_dir_make_tmp ("nolphin-md-img-XXXXXX", NULL);
	gchar *png = g_build_filename (dir, "bild.png", NULL);
	gchar *sub = g_build_filename (dir, "projekt", NULL);
	gchar *inner_png = g_build_filename (sub, "bild.png", NULL);
	GdkPixbuf *pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, 20, 10);
	gchar *text;
	GtkTextIter it;
	GtkTextChildAnchor *anchor;

	g_mkdir (sub, 0755);
	gdk_pixbuf_fill (pixbuf, 0x336699ff);
	gdk_pixbuf_save (pixbuf, png, "png", NULL, NULL);
	gdk_pixbuf_save (pixbuf, inner_png, "png", NULL, NULL);
	g_object_unref (pixbuf);

	nolphin_markdown_render_to_buffer_with_base (buffer, "Vor ![Logo](bild.png) nach", sub);
	text = buffer_text (buffer);
	if (g_strcmp0 (text, "Vor \xEF\xBF\xBC nach\n") != 0)
		fail ("lokales Bild ist kein Anker im Text");
	g_free (text);
	gtk_text_buffer_get_iter_at_offset (buffer, &it, 4);
	anchor = gtk_text_iter_get_child_anchor (&it);
	if (anchor == NULL ||
	    g_strcmp0 (g_object_get_data (G_OBJECT (anchor), "nolphin-md-image-path"), inner_png) != 0 ||
	    g_strcmp0 (g_object_get_data (G_OBJECT (anchor), "nolphin-md-image-alt"), "Logo") != 0)
		fail ("Bildanker ohne Pfad/Alternativtext");

	/* Bild in Link: Anker trägt das Link-Ziel */
	nolphin_markdown_render_to_buffer_with_base (buffer, "[![Badge](bild.png)](https://example.org/b)", sub);
	text = buffer_text (buffer);
	if (g_strcmp0 (href_at (buffer, text, "\xEF\xBF\xBC"), "https://example.org/b") != 0)
		fail ("Bild in Link ohne Link-Ziel");
	g_free (text);

	expect_text (buffer, "![Logo](https://example.org/x.png)", "Logo\n", "entferntes Bild");
	nolphin_markdown_render_to_buffer_with_base (buffer, "![Logo](https://example.org/x.png)", sub);
	text = buffer_text (buffer);
	if (g_strcmp0 (text, "Logo\n") != 0)
		fail ("entferntes Bild mit Basisordner wurde nicht zu Alternativtext");
	g_free (text);

	nolphin_markdown_render_to_buffer_with_base (buffer, "![Weg](../bild.png)", sub);
	text = buffer_text (buffer);
	if (g_strcmp0 (text, "Weg\n") != 0)
		fail ("Bild außerhalb des Projekts wurde angezeigt");
	g_free (text);

	nolphin_markdown_render_to_buffer_with_base (buffer, "![Fehlt](gibt-es-nicht.png)", sub);
	text = buffer_text (buffer);
	if (g_strcmp0 (text, "Fehlt\n") != 0)
		fail ("fehlende Bilddatei wurde nicht zu Alternativtext");
	g_free (text);

	nolphin_markdown_render_to_buffer_with_base (buffer, "![Logo](bild.png)", NULL);
	text = buffer_text (buffer);
	if (g_strcmp0 (text, "Logo\n") != 0)
		fail ("ohne Basisordner wurde ein Bild angezeigt");
	g_free (text);

	{
		gchar *cmd[] = { "rm", "-rf", dir, NULL };
		g_spawn_sync (NULL, cmd, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL, NULL, NULL);
	}
	g_free (inner_png);
	g_free (sub);
	g_free (png);
	g_free (dir);
}

int
main (int argc, char **argv)
{
	GtkTextBuffer *buffer;
	gchar *text;
	const gchar *doc =
		"# Titel\n"
		"\n"
		"Hallo **fett** und *kursiv* und `code` und [Link](https://example.org/x).\n"
		"\n"
		"- eins\n"
		"- zwei\n"
		"  - verschachtelt\n"
		"1. erstens\n"
		"\n"
		"```c\n"
		"int x = 1;\n"
		"```\n"
		"\n"
		"> Zitat\n"
		"\n"
		"---\n"
		"Siehe https://example.org/auto.\n";

	/* Ohne Display zulässig: es werden nur Puffer und Tags benutzt. */
	gtk_init_check (&argc, &argv);
	g_type_ensure (GDK_TYPE_RGBA);
	g_type_ensure (GDK_TYPE_COLOR);
	buffer = gtk_text_buffer_new (NULL);

	nolphin_markdown_render_to_buffer (buffer, doc);
	text = buffer_text (buffer);

	if (!g_str_has_prefix (text, "Titel\nHallo fett und kursiv und code und Link.\n"))
		fail ("Überschrift/Absatz: Klartext stimmt nicht");
	if (!has_tag_at (buffer, text, "Titel", "h1"))
		fail ("Überschrift trägt kein h1-Tag");
	if (!has_tag_at (buffer, text, "fett", "bold"))
		fail ("fett ohne bold-Tag");
	if (has_tag_at (buffer, text, "und kursiv", "bold"))
		fail ("bold läuft über den Hervorhebungsbereich hinaus");
	if (!has_tag_at (buffer, text, "kursiv", "italic"))
		fail ("kursiv ohne italic-Tag");
	if (!has_tag_at (buffer, text, "code und", "code"))
		fail ("Inline-Code ohne code-Tag");
	if (!has_tag_at (buffer, text, "Link", "link"))
		fail ("Link ohne link-Tag");
	if (g_strcmp0 (href_at (buffer, text, "Link"), "https://example.org/x") != 0)
		fail ("Link-Ziel falsch");
	if (strstr (text, "**") != NULL || strstr (text, "`") != NULL || strstr (text, "](") != NULL)
		fail ("Markdown-Zeichen im gerenderten Text übrig");
	if (strstr (text, "• eins\n") == NULL || strstr (text, "• zwei\n") == NULL)
		fail ("Aufzählung fehlt");
	if (strstr (text, "◦ verschachtelt\n") == NULL)
		fail ("verschachtelte Aufzählung fehlt");
	if (strstr (text, "1. erstens\n") == NULL)
		fail ("nummerierte Liste fehlt");
	if (strstr (text, "int x = 1;\n") == NULL || !has_tag_at (buffer, text, "int x", "codeblock"))
		fail ("Codeblock fehlt oder ohne codeblock-Tag");
	if (strstr (text, "```") != NULL)
		fail ("Code-Zaun im Text übrig");
	if (strstr (text, "Zitat\n") == NULL || !has_tag_at (buffer, text, "Zitat", "quote"))
		fail ("Zitat fehlt oder ohne quote-Tag");
	{
		const gchar *hr_pos = strstr (text, "Zitat\n\n");

		if (hr_pos == NULL || !has_tag_at (buffer, text, hr_pos + strlen ("Zitat\n"), "hr"))
			fail ("horizontale Linie: Leerzeile mit hr-Tag fehlt");
	}
	if (g_strcmp0 (href_at (buffer, text, "https://example.org/auto"), "https://example.org/auto") != 0)
		fail ("automatische URL nicht verlinkt (oder Satzpunkt im Ziel)");
	g_free (text);

	/* Einzelfälle */
	expect_text (buffer, NULL, "", "NULL");
	expect_text (buffer, "", "", "leer");
	expect_text (buffer, "Zeile eins\nZeile zwei", "Zeile eins Zeile zwei\n", "weicher Umbruch");
	expect_text (buffer, "Zeile eins  \nZeile zwei", "Zeile eins\nZeile zwei\n", "harter Umbruch");
	expect_text (buffer, "# C# #", "C#\n", "schließende Rauten");
	expect_text (buffer, "#kein Titel", "#kein Titel\n", "Raute ohne Leerzeichen");
	expect_text (buffer, "Titel\n=====", "Titel\n", "Setext");
	expect_text (buffer, "a \\*b\\* c", "a *b* c\n", "Escape");
	expect_text (buffer, "snake_case_name", "snake_case_name\n", "Unterstrich im Wort");
	expect_text (buffer, "2 * 3 * 4", "2 * 3 * 4\n", "Sterne mit Leerzeichen");
	expect_text (buffer, "<!-- weg -->\nsichtbar", "sichtbar\n", "HTML-Kommentar");
	expect_text (buffer, "<p align=\"center\">Mitte</p>", "Mitte\n", "HTML-Tags entfallen");
	expect_text (buffer, "![Logo](x.png)", "Logo\n", "Bild als Alternativtext");
	expect_text (buffer, "- [ ] offen\n- [x] fertig", "\xEF\xBF\xBC offen\n\xEF\xBF\xBC fertig\n", "Aufgabenliste");
	expect_text (buffer, "ungeschlossen **fett", "ungeschlossen **fett\n", "ungeschlossene Hervorhebung");
	expect_text (buffer, "```\nnie geschlossen", "nie geschlossen\n", "ungeschlossener Codeblock");
	expect_text (buffer, "| a | b |\n|---|---|\n| 1 | 2 |", "\xEF\xBF\xBC\n", "Tabelle ist ein Anker");
	expect_text (buffer, "[![Badge](b.svg)](https://example.org)", "Badge\n", "Bild-Link");
	expect_text (buffer, "ä **ö** ü", "ä ö ü\n", "UTF-8");

	test_images (buffer);
	test_github_layout (buffer);

	g_object_unref (buffer);
	if (exit_code == 0)
		g_print ("PASS\n");
	return exit_code;
}
