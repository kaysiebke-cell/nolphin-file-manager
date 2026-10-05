/* nolphin-markdown-view.c
 *
 * Eigenständiger Markdown-Renderer auf Basis von GtkTextBuffer/GtkTextTag
 * (siehe Kopfdatei und Vertrag §60.3). Blockstruktur wird zeilenweise
 * erkannt, Inline-Auszeichnungen rekursiv über die Absatztexte.
 */

#include "nolphin-markdown-view.h"

#include <glib/gstdio.h>
#include <string.h>

#define HREF_KEY "nolphin-md-href"
#define HANDLER_KEY "nolphin-md-handler"
#define HR_KEY "nolphin-md-hr"
#define IMG_PATH_KEY "nolphin-md-image-path"
#define IMG_ALT_KEY "nolphin-md-image-alt"
#define KIDS_KEY "nolphin-md-children"
/* Größere Bilddateien werden nicht geladen (Alternativtext statt Bild). */
#define IMAGE_MAX_BYTES (16 * 1024 * 1024)

typedef struct {
	GtkTextBuffer *buffer;
	const gchar   *base_dir;   /* Ordner für relative Bildpfade oder NULL */
} Ctx;

typedef struct {
	NolphinMarkdownLinkFunc func;
	gpointer                data;
} LinkHandler;

/* ------------------------------------------------------------------ */
/* Tags                                                                */
/* ------------------------------------------------------------------ */

static void
setup_tags (GtkTextBuffer *buffer)
{
	static const struct { const gchar *name; gdouble scale; gint above; gint below; } headings[] = {
		{ "h1", 1.8,  12, 6 }, { "h2", 1.5,  12, 6 }, { "h3", 1.25, 10, 4 },
		{ "h4", 1.1,  8,  4 }, { "h5", 1.0,  8,  4 }, { "h6", 0.9,  8,  4 },
	};
	GtkTextTagTable *table = gtk_text_buffer_get_tag_table (buffer);
	guint i;

	if (gtk_text_tag_table_lookup (table, "bold") != NULL)
		return;

	for (i = 0; i < G_N_ELEMENTS (headings); i++) {
		gtk_text_buffer_create_tag (buffer, headings[i].name,
					    "scale", headings[i].scale,
					    "weight", PANGO_WEIGHT_BOLD,
					    "pixels-above-lines", headings[i].above,
					    "pixels-below-lines", headings[i].below,
					    NULL);
	}
	gtk_text_buffer_create_tag (buffer, "bold", "weight", PANGO_WEIGHT_BOLD, NULL);
	gtk_text_buffer_create_tag (buffer, "italic", "style", PANGO_STYLE_ITALIC, NULL);
	gtk_text_buffer_create_tag (buffer, "strike", "strikethrough", TRUE, NULL);
	gtk_text_buffer_create_tag (buffer, "code", "family", "monospace", NULL);
	gtk_text_buffer_create_tag (buffer, "codeblock",
				    "family", "monospace",
				    "left-margin", 12,
				    "right-margin", 12,
				    "pixels-above-lines", 2,
				    "pixels-below-lines", 8,
				    "wrap-mode", GTK_WRAP_CHAR,
				    NULL);
	gtk_text_buffer_create_tag (buffer, "link", "underline", PANGO_UNDERLINE_SINGLE, NULL);
	gtk_text_buffer_create_tag (buffer, "quote", NULL);
	gtk_text_buffer_create_tag (buffer, "hr", "pixels-below-lines", 8, NULL);
	gtk_text_buffer_create_tag (buffer, "dim", NULL);
}

static GtkTextTag *
T (Ctx *c, const gchar *name)
{
	return gtk_text_tag_table_lookup (gtk_text_buffer_get_tag_table (c->buffer), name);
}

/* Absatz-Tag mit linkem Rand, Erstzeileneinzug und Abstand nach unten;
 * wird einmal angelegt und danach wiederverwendet. */
static GtkTextTag *
para_tag (Ctx *c, gint left, gint indent, gint below)
{
	gchar *name = g_strdup_printf ("p-%d-%d-%d", left, indent, below);
	GtkTextTag *tag = gtk_text_tag_table_lookup (gtk_text_buffer_get_tag_table (c->buffer), name);

	if (tag == NULL) {
		tag = gtk_text_buffer_create_tag (c->buffer, name,
						  "left-margin", left,
						  "indent", indent,
						  "pixels-below-lines", below,
						  "wrap-mode", GTK_WRAP_WORD_CHAR,
						  NULL);
	}
	g_free (name);
	return tag;
}

static GtkTextTag *
make_link_tag (Ctx *c, const gchar *href, gsize len)
{
	GtkTextTag *tag = gtk_text_buffer_create_tag (c->buffer, NULL, NULL);

	g_object_set_data_full (G_OBJECT (tag), HREF_KEY, g_strndup (href, len), g_free);
	return tag;
}

const gchar *
nolphin_markdown_tag_get_href (GtkTextTag *tag)
{
	return tag != NULL ? g_object_get_data (G_OBJECT (tag), HREF_KEY) : NULL;
}

/* ------------------------------------------------------------------ */
/* Inline                                                              */
/* ------------------------------------------------------------------ */

static gint
buffer_length (Ctx *c)
{
	return gtk_text_buffer_get_char_count (c->buffer);
}

static void
insert_run (Ctx *c, const gchar *s, const gchar *e, GPtrArray *tags)
{
	GtkTextIter it, a;
	gint start;
	guint i;

	if (e <= s)
		return;
	gtk_text_buffer_get_end_iter (c->buffer, &it);
	start = gtk_text_iter_get_offset (&it);
	gtk_text_buffer_insert (c->buffer, &it, s, (gint) (e - s));
	gtk_text_buffer_get_iter_at_offset (c->buffer, &a, start);
	gtk_text_buffer_get_end_iter (c->buffer, &it);
	for (i = 0; i < tags->len; i++)
		gtk_text_buffer_apply_tag (c->buffer, g_ptr_array_index (tags, i), &a, &it);
}

static void
insert_literal (Ctx *c, const gchar *text, GPtrArray *tags)
{
	insert_run (c, text, text + strlen (text), tags);
}

/* Ende eines Code-Spans, der an @p (nach den Backticks) beginnt, oder NULL. */
static const gchar *
find_code_close (const gchar *p, const gchar *end, gint n)
{
	const gchar *q = p;

	while (q < end) {
		if (*q == '`') {
			gint run = 0;
			while (q + run < end && q[run] == '`')
				run++;
			if (run == n)
				return q;
			q += run;
		} else {
			q++;
		}
	}
	return NULL;
}

static const gchar *
find_emph_close (const gchar *p, const gchar *end, gchar ch, gint n)
{
	const gchar *q = p;

	while (q < end) {
		if (*q == '\\' && q + 1 < end) {
			q += 2;
			continue;
		}
		if (*q == '`') {
			gint run = 0;
			const gchar *close;
			while (q + run < end && q[run] == '`')
				run++;
			close = find_code_close (q + run, end, run);
			q = close != NULL ? close + run : q + run;
			continue;
		}
		if (*q == ch) {
			gint run = 0;
			while (q + run < end && q[run] == ch)
				run++;
			if (run == n && q > p && !g_ascii_isspace (q[-1])) {
				if (ch == '_' && q + n < end && g_ascii_isalnum (q[n])) {
					q += run;
					continue;
				}
				return q;
			}
			q += run;
			continue;
		}
		q++;
	}
	return NULL;
}

/* Passende schließende Klammer zu @open an @p (zeigt auf die öffnende). */
static const gchar *
find_matching (const gchar *p, const gchar *end, gchar open, gchar close)
{
	gint depth = 0;
	const gchar *q;

	for (q = p; q < end; q++) {
		if (*q == '\\' && q + 1 < end) {
			q++;
		} else if (*q == open) {
			depth++;
		} else if (*q == close) {
			if (--depth == 0)
				return q;
		}
	}
	return NULL;
}

/* Pfad eines lokalen Bildes innerhalb von base_dir oder NULL (entfernte
 * Adressen, Pfade außerhalb des Ordners, fehlende Dateien). */
static gchar *
resolve_local_image (Ctx *c, const gchar *src, gsize len)
{
	gchar *raw, *unescaped, *full, *canon, *base_canon, *result = NULL;
	gchar *scheme, *cut;
	const gchar *rel;

	if (c->base_dir == NULL || len == 0)
		return NULL;
	raw = g_strndup (src, len);
	scheme = g_uri_parse_scheme (raw);
	if (scheme != NULL) {
		g_free (scheme);
		g_free (raw);
		return NULL;
	}
	cut = strpbrk (raw, "#?");
	if (cut != NULL)
		*cut = '\0';
	unescaped = g_uri_unescape_string (raw, NULL);
	g_free (raw);
	if (unescaped == NULL || *unescaped == '\0') {
		g_free (unescaped);
		return NULL;
	}
	rel = unescaped;
	while (*rel == '/')
		rel++;
	full = g_build_filename (c->base_dir, rel, NULL);
	canon = g_canonicalize_filename (full, NULL);
	base_canon = g_canonicalize_filename (c->base_dir, NULL);
	if (g_str_has_prefix (canon, base_canon) &&
	    (canon[strlen (base_canon)] == G_DIR_SEPARATOR || base_canon[strlen (base_canon) - 1] == G_DIR_SEPARATOR) &&
	    g_file_test (canon, G_FILE_TEST_IS_REGULAR))
		result = g_strdup (canon);
	g_free (unescaped);
	g_free (full);
	g_free (canon);
	g_free (base_canon);
	return result;
}

/* Setzt einen Anker für ein lokales Bild; die Ansicht hängt das Widget ein. */
static void
insert_image_anchor (Ctx *c, const gchar *path, const gchar *alt, gsize alt_len, GPtrArray *tags)
{
	GtkTextIter it, a;
	GtkTextChildAnchor *anchor;
	gint start;
	guint i;

	gtk_text_buffer_get_end_iter (c->buffer, &it);
	start = gtk_text_iter_get_offset (&it);
	anchor = gtk_text_buffer_create_child_anchor (c->buffer, &it);
	g_object_set_data_full (G_OBJECT (anchor), IMG_PATH_KEY, g_strdup (path), g_free);
	g_object_set_data_full (G_OBJECT (anchor), IMG_ALT_KEY, g_strndup (alt, alt_len), g_free);
	gtk_text_buffer_get_iter_at_offset (c->buffer, &a, start);
	gtk_text_buffer_get_end_iter (c->buffer, &it);
	for (i = 0; i < tags->len; i++)
		gtk_text_buffer_apply_tag (c->buffer, g_ptr_array_index (tags, i), &a, &it);
}

static void
render_inline (Ctx *c, const gchar *s, const gchar *end, GPtrArray *tags)
{
	const gchar *p = s, *run = s;

	while (p < end) {
		gchar ch = *p;

		if (ch == '\\' && p + 1 < end && p[1] == '\n') {
			insert_run (c, run, p, tags);
			insert_literal (c, "\n", tags);
			p += 2;
			run = p;
			continue;
		}
		if (ch == '\\' && p + 1 < end && strchr ("\\`*_{}[]()#+-.!~|<>", p[1]) != NULL) {
			insert_run (c, run, p, tags);
			run = p + 1;
			p += 2;
			continue;
		}
		if (ch == '\n') {
			gboolean hard = (p - run >= 2 && p[-1] == ' ' && p[-2] == ' ');

			insert_run (c, run, hard ? p - 2 : p, tags);
			insert_literal (c, hard ? "\n" : " ", tags);
			p++;
			run = p;
			continue;
		}
		if (ch == '`') {
			gint n = 0;
			const gchar *close;

			while (p + n < end && p[n] == '`')
				n++;
			close = find_code_close (p + n, end, n);
			if (close != NULL) {
				const gchar *cs = p + n, *ce = close;

				insert_run (c, run, p, tags);
				if (ce - cs >= 2 && *cs == ' ' && ce[-1] == ' ') {
					cs++;
					ce--;
				}
				g_ptr_array_add (tags, T (c, "code"));
				insert_run (c, cs, ce, tags);
				g_ptr_array_remove_index (tags, tags->len - 1);
				p = close + n;
				run = p;
			} else {
				p += n;
			}
			continue;
		}
		if (ch == '*' || ch == '_' || ch == '~') {
			gint n = 0, use;
			const gchar *close;
			gboolean open_ok;

			while (p + n < end && p[n] == ch)
				n++;
			if (ch == '~')
				use = (n >= 2) ? 2 : 0;
			else
				use = MIN (n, 3);
			open_ok = use > 0 && p + use < end && !g_ascii_isspace (p[use]);
			if (open_ok && ch == '_' && p > s && g_ascii_isalnum (p[-1]))
				open_ok = FALSE;
			close = open_ok ? find_emph_close (p + use, end, ch, use) : NULL;
			if (close != NULL && close > p + use) {
				insert_run (c, run, p, tags);
				if (ch == '~') {
					g_ptr_array_add (tags, T (c, "strike"));
				} else {
					if (use >= 2)
						g_ptr_array_add (tags, T (c, "bold"));
					if (use == 1 || use == 3)
						g_ptr_array_add (tags, T (c, "italic"));
				}
				render_inline (c, p + use, close, tags);
				g_ptr_array_set_size (tags, tags->len - (use == 3 ? 2 : 1));
				p = close + use;
				run = p;
			} else {
				p += n;
			}
			continue;
		}
		if (ch == '[' || (ch == '!' && p + 1 < end && p[1] == '[')) {
			gboolean image = (ch == '!');
			const gchar *open = image ? p + 1 : p;
			const gchar *close = find_matching (open, end, '[', ']');

			if (close != NULL && close + 1 < end && close[1] == '(') {
				const gchar *dest_close = find_matching (close + 1, end, '(', ')');

				if (dest_close != NULL) {
					const gchar *ds = close + 2, *de = dest_close;

					while (ds < de && g_ascii_isspace (*ds))
						ds++;
					if (ds < de && *ds == '<') {
						const gchar *gt = memchr (ds, '>', de - ds);
						ds++;
						de = gt != NULL ? gt : de;
					} else {
						const gchar *sp = ds;
						while (sp < de && !g_ascii_isspace (*sp))
							sp++;
						de = sp;
					}
					insert_run (c, run, p, tags);
					if (image) {
						gchar *img = resolve_local_image (c, ds, de - ds);

						if (img != NULL) {
							insert_image_anchor (c, img, open + 1, close - open - 1, tags);
							g_free (img);
						} else {
							/* Entfernte oder nicht auffindbare Bilder: Alternativtext, kursiv. */
							g_ptr_array_add (tags, T (c, "italic"));
							render_inline (c, open + 1, close, tags);
							g_ptr_array_remove_index (tags, tags->len - 1);
						}
					} else {
						g_ptr_array_add (tags, T (c, "link"));
						g_ptr_array_add (tags, make_link_tag (c, ds, de - ds));
						render_inline (c, open + 1, close, tags);
						g_ptr_array_set_size (tags, tags->len - 2);
					}
					p = dest_close + 1;
					run = p;
					continue;
				}
			}
			p++;
			continue;
		}
		if (ch == '<') {
			const gchar *gt = memchr (p, '>', end - p);

			if (gt != NULL && gt > p + 1) {
				gsize len = gt - p - 1;
				const gchar *in = p + 1;

				if ((len > 7 && strncmp (in, "http://", 7) == 0) ||
				    (len > 8 && strncmp (in, "https://", 8) == 0) ||
				    (len > 7 && strncmp (in, "mailto:", 7) == 0)) {
					insert_run (c, run, p, tags);
					g_ptr_array_add (tags, T (c, "link"));
					g_ptr_array_add (tags, make_link_tag (c, in, len));
					insert_run (c, in, gt, tags);
					g_ptr_array_set_size (tags, tags->len - 2);
					p = gt + 1;
					run = p;
					continue;
				}
				if (g_ascii_isalpha (*in) || *in == '/' || *in == '!') {
					gboolean br = (g_ascii_strncasecmp (in, "br", 2) == 0 &&
						       (len == 2 || in[2] == '/' || g_ascii_isspace (in[2])));

					insert_run (c, run, p, tags);
					if (br)
						insert_literal (c, "\n", tags);
					p = gt + 1;
					run = p;
					continue;
				}
			}
			p++;
			continue;
		}
		if (ch == 'h' && (p == s || !g_ascii_isalnum (p[-1])) &&
		    (strncmp (p, "http://", MIN (7, end - p)) == 0 || strncmp (p, "https://", MIN (8, end - p)) == 0) &&
		    end - p > 8) {
			const gchar *ue = p;

			while (ue < end && !g_ascii_isspace (*ue) && *ue != '<')
				ue++;
			while (ue > p && strchr (".,;:!?)", ue[-1]) != NULL)
				ue--;
			if (ue - p > 8 && (strncmp (p, "http://", 7) == 0 || strncmp (p, "https://", 8) == 0)) {
				insert_run (c, run, p, tags);
				g_ptr_array_add (tags, T (c, "link"));
				g_ptr_array_add (tags, make_link_tag (c, p, ue - p));
				insert_run (c, p, ue, tags);
				g_ptr_array_set_size (tags, tags->len - 2);
				p = ue;
				run = p;
				continue;
			}
		}
		p++;
	}
	insert_run (c, run, end, tags);
}

/* ------------------------------------------------------------------ */
/* Blöcke                                                              */
/* ------------------------------------------------------------------ */

static const gchar *
skip_spaces (const gchar *s)
{
	while (*s == ' ' || *s == '\t')
		s++;
	return s;
}

static gboolean
is_blank (const gchar *line)
{
	return *skip_spaces (line) == '\0';
}

static gboolean
is_fence (const gchar *line, gchar *ch, gint *len)
{
	const gchar *s = skip_spaces (line);
	gint n = 0;

	if (*s != '`' && *s != '~')
		return FALSE;
	*ch = *s;
	while (s[n] == *ch)
		n++;
	if (n < 3)
		return FALSE;
	/* Info-String nach ``` darf keine weiteren Backticks enthalten. */
	if (*ch == '`' && strchr (s + n, '`') != NULL)
		return FALSE;
	*len = n;
	return TRUE;
}

static gint
heading_level (const gchar *line, const gchar **text)
{
	const gchar *s = line;
	gint n = 0;

	while (n < 3 && *s == ' ') {
		s++;
		n++;
	}
	n = 0;
	while (*s == '#') {
		s++;
		n++;
	}
	if (n < 1 || n > 6 || (*s != ' ' && *s != '\t' && *s != '\0'))
		return 0;
	*text = skip_spaces (s);
	return n;
}

static gboolean
is_hr (const gchar *line)
{
	const gchar *s = skip_spaces (line);
	gchar ch = *s;
	gint n = 0;

	if (ch != '-' && ch != '*' && ch != '_')
		return FALSE;
	for (; *s != '\0'; s++) {
		if (*s == ch)
			n++;
		else if (*s != ' ' && *s != '\t')
			return FALSE;
	}
	return n >= 3;
}

/* "===" bzw. "---" unter einem Absatz (Setext-Überschrift): Ebene oder 0. */
static gint
setext_level (const gchar *line)
{
	const gchar *s = skip_spaces (line);
	gchar ch = *s;
	gint n = 0;

	if (ch != '=' && ch != '-')
		return 0;
	while (*s == ch) {
		s++;
		n++;
	}
	if (!is_blank (s) || n < 1)
		return 0;
	return ch == '=' ? 1 : 2;
}

static gboolean
is_list_item (const gchar *line, gint *indent, gchar *marker_out, const gchar **number, gint *number_len, const gchar **content)
{
	const gchar *s = line;
	gint ind = 0;

	while (*s == ' ' || *s == '\t') {
		ind += (*s == '\t') ? 4 : 1;
		s++;
	}
	if ((*s == '-' || *s == '*' || *s == '+') && (s[1] == ' ' || s[1] == '\t')) {
		if (is_hr (line))
			return FALSE;
		*marker_out = *s;
		*number = NULL;
		*number_len = 0;
		*content = skip_spaces (s + 1);
		*indent = ind;
		return TRUE;
	}
	if (g_ascii_isdigit (*s)) {
		const gchar *d = s;

		while (g_ascii_isdigit (*d) && d - s < 9)
			d++;
		if ((*d == '.' || *d == ')') && (d[1] == ' ' || d[1] == '\t')) {
			*marker_out = *d;
			*number = s;
			*number_len = (gint) (d - s);
			*content = skip_spaces (d + 1);
			*indent = ind;
			return TRUE;
		}
	}
	return FALSE;
}

static gboolean
is_table_sep (const gchar *line)
{
	const gchar *s = line;
	gboolean dash = FALSE;

	if (strchr (line, '|') == NULL && strchr (line, '-') == NULL)
		return FALSE;
	for (; *s != '\0'; s++) {
		if (*s == '-')
			dash = TRUE;
		else if (*s != '|' && *s != ':' && *s != ' ' && *s != '\t')
			return FALSE;
	}
	return dash && strchr (line, '|') != NULL;
}

static gboolean
starts_block (gchar **lines, gint i, gint n)
{
	const gchar *line = lines[i], *t;
	gchar ch;
	gint len, ind, nl;
	const gchar *num, *content;

	if (is_fence (line, &ch, &len) || heading_level (line, &t) > 0 || is_hr (line))
		return TRUE;
	if (*skip_spaces (line) == '>')
		return TRUE;
	if (is_list_item (line, &ind, &ch, &num, &nl, &content))
		return TRUE;
	if (strchr (line, '|') != NULL && i + 1 < n && is_table_sep (lines[i + 1]))
		return TRUE;
	if (g_str_has_prefix (skip_spaces (line), "<!--"))
		return TRUE;
	return FALSE;
}

static void
end_paragraph (Ctx *c, GPtrArray *tags, gint start_len)
{
	if (buffer_length (c) > start_len)
		insert_literal (c, "\n", tags);
}

static void
emit_text_block (Ctx *c, const gchar *text, GtkTextTag *block_tag, gint quote)
{
	GPtrArray *tags = g_ptr_array_new ();
	gint start = buffer_length (c);

	g_ptr_array_add (tags, block_tag);
	if (quote > 0)
		g_ptr_array_add (tags, T (c, "quote"));
	render_inline (c, text, text + strlen (text), tags);
	end_paragraph (c, tags, start);
	g_ptr_array_free (tags, TRUE);
}

static void
flush_paragraph (Ctx *c, GString *para, gint quote)
{
	gchar *text;

	if (para->len == 0)
		return;
	text = g_strchomp (g_strdup (para->str));
	emit_text_block (c, text, para_tag (c, quote * 16, 0, 8), quote);
	g_free (text);
	g_string_truncate (para, 0);
}

static void
emit_verbatim (Ctx *c, const gchar *text, gint quote)
{
	GPtrArray *tags = g_ptr_array_new ();
	gint start = buffer_length (c);

	g_ptr_array_add (tags, T (c, "codeblock"));
	if (quote > 0)
		g_ptr_array_add (tags, para_tag (c, 12 + quote * 16, 0, 8));
	insert_literal (c, text, tags);
	end_paragraph (c, tags, start);
	g_ptr_array_free (tags, TRUE);
}

static void render_blocks (Ctx *c, gchar **lines, gint n, gint quote);

static void
render_blocks (Ctx *c, gchar **lines, gint n, gint quote)
{
	GString *para = g_string_new (NULL);
	gint i = 0;

	while (i < n) {
		const gchar *line = lines[i], *htext;
		gchar ch;
		gint flen, level, ind, nl;
		const gchar *num, *content;

		if (is_blank (line)) {
			flush_paragraph (c, para, quote);
			i++;
			continue;
		}

		/* HTML-Kommentar */
		if (g_str_has_prefix (skip_spaces (line), "<!--")) {
			flush_paragraph (c, para, quote);
			while (i < n && strstr (lines[i], "-->") == NULL)
				i++;
			i++;
			continue;
		}

		/* Eingezäunter Codeblock */
		if (is_fence (line, &ch, &flen)) {
			GString *code = g_string_new (NULL);
			gchar cch;
			gint clen;

			flush_paragraph (c, para, quote);
			i++;
			while (i < n) {
				if (is_fence (lines[i], &cch, &clen) && cch == ch && clen >= flen &&
				    is_blank (skip_spaces (lines[i]) + clen))
					break;
				if (code->len > 0)
					g_string_append_c (code, '\n');
				g_string_append (code, lines[i]);
				i++;
			}
			i++; /* schließenden Zaun überspringen */
			if (code->len > 0)
				emit_verbatim (c, code->str, quote);
			g_string_free (code, TRUE);
			continue;
		}

		/* Setext-Überschrift: Absatz plus === bzw. --- */
		if (para->len > 0 && (level = setext_level (line)) > 0) {
			gchar *text = g_strstrip (g_strdup (para->str));
			gchar *hname = g_strdup_printf ("h%d", level);

			emit_text_block (c, text, T (c, hname), 0);
			g_free (hname);
			g_free (text);
			g_string_truncate (para, 0);
			i++;
			continue;
		}

		if ((level = heading_level (line, &htext)) > 0) {
			gchar *text = g_strdup (htext);
			gchar *hname = g_strdup_printf ("h%d", level);
			gsize len = strlen (text);

			flush_paragraph (c, para, quote);
			while (len > 0 && (text[len - 1] == ' ' || text[len - 1] == '\t'))
				text[--len] = '\0';
			/* Schließende Rauten nur, wenn ein Leerzeichen davor steht ("# C#" bleibt). */
			{
				gsize k = len;

				while (k > 0 && text[k - 1] == '#')
					k--;
				if (k < len && (k == 0 || text[k - 1] == ' ' || text[k - 1] == '\t')) {
					text[k] = '\0';
					g_strchomp (text);
				}
			}
			emit_text_block (c, text, T (c, hname), 0);
			g_free (hname);
			g_free (text);
			i++;
			continue;
		}

		if (is_hr (line)) {
			GPtrArray *tags = g_ptr_array_new ();
			GtkTextIter end_iter;
			GtkTextChildAnchor *anchor;

			flush_paragraph (c, para, quote);
			/* Die Linie selbst ist ein Widget, das die Ansicht an den Anker
			 * hängt (nolphin_markdown_view_set_text); im Puffer steht nur der
			 * Anker. */
			gtk_text_buffer_get_end_iter (c->buffer, &end_iter);
			anchor = gtk_text_buffer_create_child_anchor (c->buffer, &end_iter);
			g_object_set_data (G_OBJECT (anchor), HR_KEY, GINT_TO_POINTER (1));
			g_ptr_array_add (tags, T (c, "hr"));
			insert_literal (c, "\n", tags);
			g_ptr_array_free (tags, TRUE);
			i++;
			continue;
		}

		/* Zitat */
		if (*skip_spaces (line) == '>') {
			GPtrArray *inner = g_ptr_array_new_with_free_func (g_free);
			gchar **iv;
			guint k;

			flush_paragraph (c, para, quote);
			while (i < n && *skip_spaces (lines[i]) == '>') {
				const gchar *q = skip_spaces (lines[i]) + 1;

				if (*q == ' ')
					q++;
				g_ptr_array_add (inner, g_strdup (q));
				i++;
			}
			iv = g_new0 (gchar *, inner->len + 1);
			for (k = 0; k < inner->len; k++)
				iv[k] = g_ptr_array_index (inner, k);
			render_blocks (c, iv, (gint) inner->len, quote + 1);
			g_free (iv);
			g_ptr_array_free (inner, TRUE);
			continue;
		}

		/* Liste */
		if (is_list_item (line, &ind, &ch, &num, &nl, &content)) {
			gint item_indent = ind;
			GString *item = g_string_new (content);
			gchar *prefix;
			gint lvl = item_indent / 2;
			gint left = quote * 16 + 16 * (MIN (lvl, 6) + 1) + 16;
			GPtrArray *tags;
			gint start;
			gchar *text;

			flush_paragraph (c, para, quote);
			i++;
			/* Fortsetzungszeilen: nicht leer, kein neuer Block, nicht
			 * weniger eingerückt als der Inhalt. */
			while (i < n && !is_blank (lines[i]) && !starts_block (lines, i, n))
				{
				g_string_append_c (item, '\n');
				g_string_append (item, skip_spaces (lines[i]));
				i++;
			}
			if (g_str_has_prefix (item->str, "[ ] ") || g_str_has_prefix (item->str, "[x] ") ||
			    g_str_has_prefix (item->str, "[X] ")) {
				prefix = g_strdup (item->str[1] == ' ' ? "☐ " : "☑ ");
				g_string_erase (item, 0, 4);
			} else if (num != NULL) {
				gchar *digits = g_strndup (num, nl);

				prefix = g_strdup_printf ("%s%c ", digits, ch);
				g_free (digits);
			} else {
				static const gchar *bullets[] = { "• ", "◦ ", "▪ " };

				prefix = g_strdup (bullets[MIN (lvl, 2)]);
			}
			tags = g_ptr_array_new ();
			g_ptr_array_add (tags, para_tag (c, left, -16, 2));
			if (quote > 0)
				g_ptr_array_add (tags, T (c, "quote"));
			start = buffer_length (c);
			insert_literal (c, prefix, tags);
			text = g_strchomp (g_string_free (item, FALSE));
			render_inline (c, text, text + strlen (text), tags);
			end_paragraph (c, tags, start);
			g_free (text);
			g_free (prefix);
			g_ptr_array_free (tags, TRUE);
			continue;
		}

		/* Tabelle: als Festbreitentext, da nicht gerendert */
		if (strchr (line, '|') != NULL && i + 1 < n && is_table_sep (lines[i + 1])) {
			GString *tbl = g_string_new (NULL);

			flush_paragraph (c, para, quote);
			while (i < n && !is_blank (lines[i]) && strchr (lines[i], '|') != NULL) {
				if (tbl->len > 0)
					g_string_append_c (tbl, '\n');
				g_string_append (tbl, lines[i]);
				i++;
			}
			emit_verbatim (c, tbl->str, quote);
			g_string_free (tbl, TRUE);
			continue;
		}

		/* Absatzzeile */
		if (para->len > 0)
			g_string_append_c (para, '\n');
		g_string_append (para, skip_spaces (line));
		i++;
	}
	flush_paragraph (c, para, quote);
	g_string_free (para, TRUE);
}

void
nolphin_markdown_render_to_buffer (GtkTextBuffer *buffer, const gchar *markdown)
{
	nolphin_markdown_render_to_buffer_with_base (buffer, markdown, NULL);
}

void
nolphin_markdown_render_to_buffer_with_base (GtkTextBuffer *buffer, const gchar *markdown, const gchar *base_dir)
{
	Ctx c = { buffer, base_dir };
	gchar *clean, **lines;
	gint n;

	g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));

	gtk_text_buffer_set_text (buffer, "", -1);
	setup_tags (buffer);
	if (markdown == NULL || *markdown == '\0')
		return;

	clean = g_utf8_make_valid (markdown, -1);
	/* CRLF vereinheitlichen */
	{
		gchar *w = clean, *r = clean;
		for (; *r != '\0'; r++) {
			if (*r == '\r' && r[1] == '\n')
				continue;
			*w++ = *r;
		}
		*w = '\0';
	}
	lines = g_strsplit (clean, "\n", -1);
	n = (gint) g_strv_length (lines);
	render_blocks (&c, lines, n, 0);
	g_strfreev (lines);
	g_free (clean);
}

/* ------------------------------------------------------------------ */
/* Widget                                                              */
/* ------------------------------------------------------------------ */

static void
apply_theme (GtkWidget *view)
{
	GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
	GtkTextTagTable *table = gtk_text_buffer_get_tag_table (buffer);
	GtkStyleContext *style = gtk_widget_get_style_context (view);
	GdkRGBA fg, dim, shade, link;
	GtkTextTag *tag;

	if (gtk_text_tag_table_lookup (table, "bold") == NULL)
		return;

	gtk_style_context_get_color (style, gtk_style_context_get_state (style), &fg);
	dim = fg;
	dim.alpha = fg.alpha * 0.65;
	shade = fg;
	shade.alpha = fg.alpha * 0.10;

	if ((tag = gtk_text_tag_table_lookup (table, "code")) != NULL)
		g_object_set (tag, "background-rgba", &shade, NULL);
	if ((tag = gtk_text_tag_table_lookup (table, "codeblock")) != NULL)
		g_object_set (tag, "background-rgba", &shade, NULL);
	if ((tag = gtk_text_tag_table_lookup (table, "quote")) != NULL)
		g_object_set (tag, "foreground-rgba", &dim, NULL);
	if ((tag = gtk_text_tag_table_lookup (table, "dim")) != NULL)
		g_object_set (tag, "foreground-rgba", &dim, NULL);

	/* Linkfarbe aus dem aktiven GTK-Theme (Zustand "link"); definiert das
	 * Theme keine, bleibt es bei Textfarbe mit Unterstreichung. */
	gtk_style_context_save (style);
	gtk_style_context_set_state (style, GTK_STATE_FLAG_LINK);
	gtk_style_context_get_color (style, GTK_STATE_FLAG_LINK, &link);
	gtk_style_context_restore (style);
	if ((tag = gtk_text_tag_table_lookup (table, "link")) != NULL)
		g_object_set (tag, "foreground-rgba", &link, NULL);
}

static const gchar *
href_at_iter (const GtkTextIter *iter)
{
	GSList *tags = gtk_text_iter_get_tags (iter), *l;
	const gchar *href = NULL;

	for (l = tags; l != NULL && href == NULL; l = l->next)
		href = nolphin_markdown_tag_get_href (l->data);
	g_slist_free (tags);
	return href;
}

static gboolean
iter_for_event (GtkWidget *view, gdouble x, gdouble y, GtkTextIter *iter);

static void
set_image_width (GtkWidget *box, gint width)
{
	GdkPixbuf *orig = g_object_get_data (G_OBJECT (box), "nolphin-md-pixbuf");
	GtkWidget *image;
	gint ow, oh, target;

	if (orig == NULL)
		return;
	ow = gdk_pixbuf_get_width (orig);
	oh = gdk_pixbuf_get_height (orig);
	target = MAX (1, MIN (ow, width));
	if (GPOINTER_TO_INT (g_object_get_data (G_OBJECT (box), "nolphin-md-cur-width")) == target)
		return;
	g_object_set_data (G_OBJECT (box), "nolphin-md-cur-width", GINT_TO_POINTER (target));
	image = gtk_bin_get_child (GTK_BIN (box));
	if (target == ow) {
		gtk_image_set_from_pixbuf (GTK_IMAGE (image), orig);
	} else {
		GdkPixbuf *scaled = gdk_pixbuf_scale_simple (orig, target, MAX (1, (gint) ((gint64) oh * target / ow)),
							     GDK_INTERP_BILINEAR);

		gtk_image_set_from_pixbuf (GTK_IMAGE (image), scaled);
		g_object_unref (scaled);
	}
}

static gboolean
on_image_release (GtkWidget *box, GdkEventButton *event, gpointer view)
{
	LinkHandler *handler = g_object_get_data (G_OBJECT (view), HANDLER_KEY);
	const gchar *href = g_object_get_data (G_OBJECT (box), "nolphin-md-img-href");

	if (event->button == GDK_BUTTON_PRIMARY && href != NULL && handler != NULL && handler->func != NULL)
		handler->func (view, href, handler->data);
	return GDK_EVENT_PROPAGATE;
}

/* Widget für ein lokales Bild; kann es nicht geladen werden, erscheint der
 * Alternativtext. */
static GtkWidget *
make_image_widget (GtkWidget *view, const gchar *path, const gchar *alt, const gchar *href)
{
	GStatBuf st;
	GdkPixbuf *pixbuf = NULL;
	GtkWidget *box;

	if (g_stat (path, &st) == 0 && st.st_size <= IMAGE_MAX_BYTES)
		pixbuf = gdk_pixbuf_new_from_file (path, NULL);
	if (pixbuf == NULL) {
		gchar *base = g_path_get_basename (path);
		GtkWidget *label = gtk_label_new (alt != NULL && *alt != '\0' ? alt : base);

		gtk_style_context_add_class (gtk_widget_get_style_context (label), "dim-label");
		g_free (base);
		return label;
	}
	box = gtk_event_box_new ();
	gtk_event_box_set_visible_window (GTK_EVENT_BOX (box), FALSE);
	gtk_container_add (GTK_CONTAINER (box), gtk_image_new_from_pixbuf (pixbuf));
	gtk_widget_show_all (box);
	if (alt != NULL && *alt != '\0')
		gtk_widget_set_tooltip_text (box, alt);
	g_object_set_data_full (G_OBJECT (box), "nolphin-md-pixbuf", pixbuf, g_object_unref);
	if (href != NULL) {
		g_object_set_data_full (G_OBJECT (box), "nolphin-md-img-href", g_strdup (href), g_free);
		g_signal_connect (box, "button-release-event", G_CALLBACK (on_image_release), view);
	}
	return box;
}

/* Hängt für jeden Anker eine Widget ein: GtkSeparator für horizontale Linien,
 * das Bild bzw. sein Alternativtext für lokale Bilder. */
static void
attach_children (GtkWidget *view)
{
	GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
	GPtrArray *kids = g_ptr_array_new ();
	GtkTextIter it;

	gtk_text_buffer_get_start_iter (buffer, &it);
	do {
		GtkTextChildAnchor *anchor = gtk_text_iter_get_child_anchor (&it);
		GtkWidget *child = NULL;
		const gchar *img_path;

		if (anchor == NULL)
			continue;
		img_path = g_object_get_data (G_OBJECT (anchor), IMG_PATH_KEY);
		if (g_object_get_data (G_OBJECT (anchor), HR_KEY) != NULL) {
			child = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
			g_object_set_data (G_OBJECT (child), "nolphin-md-hr", GINT_TO_POINTER (1));
		} else if (img_path != NULL) {
			child = make_image_widget (view, img_path, g_object_get_data (G_OBJECT (anchor), IMG_ALT_KEY),
						   href_at_iter (&it));
		}
		if (child != NULL) {
			gtk_text_view_add_child_at_anchor (GTK_TEXT_VIEW (view), child, anchor);
			gtk_widget_show (child);
			g_ptr_array_add (kids, child);
		}
	} while (gtk_text_iter_forward_char (&it));
	g_object_set_data_full (G_OBJECT (view), KIDS_KEY, kids, (GDestroyNotify) g_ptr_array_unref);
	g_object_set_data (G_OBJECT (view), "nolphin-md-sep-width", GINT_TO_POINTER (0));
}

/* Linien und Bilder passen sich der Textbreite der Ansicht an. */
static void
on_size_allocate (GtkWidget *view, GdkRectangle *allocation, gpointer user_data)
{
	GPtrArray *kids = g_object_get_data (G_OBJECT (view), KIDS_KEY);
	gint width = allocation->width - gtk_text_view_get_left_margin (GTK_TEXT_VIEW (view)) -
		     gtk_text_view_get_right_margin (GTK_TEXT_VIEW (view));
	guint i;

	width = MAX (width, 1);
	if (kids == NULL || kids->len == 0 ||
	    GPOINTER_TO_INT (g_object_get_data (G_OBJECT (view), "nolphin-md-sep-width")) == width)
		return;
	g_object_set_data (G_OBJECT (view), "nolphin-md-sep-width", GINT_TO_POINTER (width));
	for (i = 0; i < kids->len; i++) {
		GtkWidget *kid = g_ptr_array_index (kids, i);

		if (g_object_get_data (G_OBJECT (kid), "nolphin-md-hr") != NULL)
			gtk_widget_set_size_request (kid, width, -1);
		else
			set_image_width (kid, width);
	}
}

static gboolean
iter_for_event (GtkWidget *view, gdouble x, gdouble y, GtkTextIter *iter)
{
	gint bx, by;

	gtk_text_view_window_to_buffer_coords (GTK_TEXT_VIEW (view), GTK_TEXT_WINDOW_WIDGET,
					       (gint) x, (gint) y, &bx, &by);
	return gtk_text_view_get_iter_at_location (GTK_TEXT_VIEW (view), iter, bx, by);
}

static gboolean
on_button_release (GtkWidget *view, GdkEventButton *event, gpointer user_data)
{
	GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
	LinkHandler *handler = g_object_get_data (G_OBJECT (view), HANDLER_KEY);
	GtkTextIter iter;
	const gchar *href;

	if (event->button != GDK_BUTTON_PRIMARY || handler == NULL || handler->func == NULL)
		return GDK_EVENT_PROPAGATE;
	if (gtk_text_buffer_get_has_selection (buffer))
		return GDK_EVENT_PROPAGATE;
	if (!iter_for_event (view, event->x, event->y, &iter))
		return GDK_EVENT_PROPAGATE;
	href = href_at_iter (&iter);
	if (href != NULL)
		handler->func (view, href, handler->data);
	return GDK_EVENT_PROPAGATE;
}

static gboolean
on_motion (GtkWidget *view, GdkEventMotion *event, gpointer user_data)
{
	GtkTextIter iter;
	GdkWindow *win = gtk_text_view_get_window (GTK_TEXT_VIEW (view), GTK_TEXT_WINDOW_TEXT);
	gboolean over = iter_for_event (view, event->x, event->y, &iter) && href_at_iter (&iter) != NULL;
	gboolean was = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (view), "nolphin-md-over"));

	if (win != NULL && over != was) {
		GdkCursor *cursor = over ? gdk_cursor_new_from_name (gtk_widget_get_display (view), "pointer") : NULL;

		gdk_window_set_cursor (win, cursor);
		if (cursor != NULL)
			g_object_unref (cursor);
		g_object_set_data (G_OBJECT (view), "nolphin-md-over", GINT_TO_POINTER (over));
	}
	return GDK_EVENT_PROPAGATE;
}

GtkWidget *
nolphin_markdown_view_new (void)
{
	GtkWidget *view = gtk_text_view_new ();

	gtk_text_view_set_editable (GTK_TEXT_VIEW (view), FALSE);
	gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (view), FALSE);
	gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (view), GTK_WRAP_WORD_CHAR);
	gtk_text_view_set_left_margin (GTK_TEXT_VIEW (view), 8);
	gtk_text_view_set_right_margin (GTK_TEXT_VIEW (view), 8);

	g_signal_connect (view, "button-release-event", G_CALLBACK (on_button_release), NULL);
	g_signal_connect (view, "motion-notify-event", G_CALLBACK (on_motion), NULL);
	g_signal_connect (view, "style-updated", G_CALLBACK (apply_theme), NULL);
	g_signal_connect (view, "size-allocate", G_CALLBACK (on_size_allocate), NULL);

	nolphin_markdown_view_set_text (view, NULL);
	return view;
}

void
nolphin_markdown_view_set_text (GtkWidget *view, const gchar *markdown)
{
	nolphin_markdown_view_set_text_with_base (view, markdown, NULL);
}

void
nolphin_markdown_view_set_text_with_base (GtkWidget *view, const gchar *markdown, const gchar *base_dir)
{
	GtkTextBuffer *buffer;

	g_return_if_fail (GTK_IS_TEXT_VIEW (view));

	/* Neuer Puffer je Inhalt, damit die Link-Tags des alten Inhalts
	 * nicht in der Tag-Tabelle liegen bleiben. */
	buffer = gtk_text_buffer_new (NULL);
	nolphin_markdown_render_to_buffer_with_base (buffer, markdown, base_dir);
	g_object_set_data (G_OBJECT (view), KIDS_KEY, NULL);
	gtk_text_view_set_buffer (GTK_TEXT_VIEW (view), buffer);
	g_object_unref (buffer);
	attach_children (view);
	apply_theme (view);
	gtk_widget_queue_resize (view);
}

void
nolphin_markdown_view_set_link_handler (GtkWidget *view, NolphinMarkdownLinkFunc func, gpointer user_data)
{
	LinkHandler *handler = g_new0 (LinkHandler, 1);

	handler->func = func;
	handler->data = user_data;
	g_object_set_data_full (G_OBJECT (view), HANDLER_KEY, handler, g_free);
}
