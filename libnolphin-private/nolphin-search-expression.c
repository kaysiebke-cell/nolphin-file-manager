/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nolphin-search-expression.c: Operatoren für die Dateinamensuche (§33)
 */

#include <config.h>
#include <string.h>

#include "nolphin-search-expression.h"

typedef struct {
	GPatternSpec *pattern;
	gboolean      negated;
} Term;

/* Eine UND-Gruppe aus Begriffen; der Ausdruck ist ODER über Gruppen. */
typedef struct {
	GPtrArray *terms; /* Term* */
} Group;

struct _NolphinSearchExpression {
	GPtrArray *groups; /* Group* */
};

static void
term_free (Term *term)
{
	g_pattern_spec_free (term->pattern);
	g_free (term);
}

static void
group_free (Group *group)
{
	g_ptr_array_unref (group->terms);
	g_free (group);
}

static Group *
group_new (void)
{
	Group *group = g_new0 (Group, 1);

	group->terms = g_ptr_array_new_with_free_func ((GDestroyNotify) term_free);
	return group;
}

static gboolean
is_keyword (const gchar *token, const gchar *a, const gchar *b)
{
	return g_ascii_strcasecmp (token, a) == 0 || (b != NULL && g_ascii_strcasecmp (token, b) == 0);
}

/* Teilt den Text an Leerzeichen; "…" bleibt als ein Token (mit Anführungszeichen). */
static GPtrArray *
tokenize (const gchar *text)
{
	GPtrArray *tokens = g_ptr_array_new_with_free_func (g_free);
	const gchar *p = text;

	while (*p != '\0') {
		const gchar *start;

		while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') {
			p++;
		}
		if (*p == '\0') {
			break;
		}
		start = p;
		if (*p == '"' || (*p == '-' && p[1] == '"')) {
			p += (*p == '-') ? 2 : 1;
			while (*p != '\0' && *p != '"') {
				p++;
			}
			if (*p == '"') {
				p++;
			}
		} else {
			while (*p != '\0' && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n') {
				p++;
			}
		}
		g_ptr_array_add (tokens, g_strndup (start, p - start));
	}
	return tokens;
}

static void
escape_glob_literal (GString *out, const gchar *s, gsize len)
{
	gsize i;

	/* GPattern kennt keine Maskierung; Platzhalterzeichen in einer exakten
	 * Angabe werden daher als "?" (genau ein beliebiges Zeichen) ersetzt. */
	for (i = 0; i < len; i++) {
		if (s[i] == '*') {
			g_string_append_c (out, '?');
		} else {
			g_string_append_c (out, s[i]);
		}
	}
}

static Term *
term_from_token (const gchar *token, gboolean negated)
{
	Term *term = g_new0 (Term, 1);
	gchar *pattern;
	gsize len = strlen (token);

	if (token[0] == '"') {
		/* exakt: ganzer Name */
		GString *s = g_string_new (NULL);
		gsize inner = len - 1;

		if (inner > 0 && token[len - 1] == '"') {
			inner--;
		}
		escape_glob_literal (s, token + 1, inner);
		pattern = g_string_free (s, FALSE);
	} else if (strchr (token, '*') == NULL && strchr (token, '?') == NULL) {
		pattern = g_strdup_printf ("*%s*", token);
	} else {
		pattern = g_strdup (token);
	}

	term->pattern = g_pattern_spec_new (pattern);
	term->negated = negated;
	g_free (pattern);
	return term;
}

NolphinSearchExpression *
nolphin_search_expression_new (const gchar *text)
{
	NolphinSearchExpression *expr = g_new0 (NolphinSearchExpression, 1);
	GPtrArray *tokens = tokenize (text != NULL ? text : "");
	Group *group = group_new ();
	gboolean negate_next = FALSE;
	guint i;

	expr->groups = g_ptr_array_new_with_free_func ((GDestroyNotify) group_free);

	for (i = 0; i < tokens->len; i++) {
		const gchar *token = g_ptr_array_index (tokens, i);

		if (is_keyword (token, "ODER", "OR") || g_strcmp0 (token, "|") == 0) {
			if (group->terms->len > 0) {
				g_ptr_array_add (expr->groups, group);
				group = group_new ();
			}
			negate_next = FALSE;
			continue;
		}
		if (is_keyword (token, "UND", "AND")) {
			continue;
		}
		if (is_keyword (token, "NICHT", "NOT")) {
			negate_next = !negate_next;
			continue;
		}
		if (token[0] == '-' && token[1] != '\0') {
			g_ptr_array_add (group->terms, term_from_token (token + 1, !negate_next));
		} else {
			g_ptr_array_add (group->terms, term_from_token (token, negate_next));
		}
		negate_next = FALSE;
	}

	if (group->terms->len > 0) {
		g_ptr_array_add (expr->groups, group);
	} else {
		group_free (group);
	}
	g_ptr_array_unref (tokens);
	return expr;
}

gboolean
nolphin_search_expression_matches (NolphinSearchExpression *expr, const gchar *name)
{
	gsize len;
	gchar *reversed;
	gboolean any = FALSE;
	guint g, t;

	if (expr->groups->len == 0) {
		return TRUE; /* leere Suche trifft alles */
	}

	len = strlen (name);
	reversed = g_utf8_strreverse (name, -1);

	for (g = 0; g < expr->groups->len && !any; g++) {
		Group *group = g_ptr_array_index (expr->groups, g);
		gboolean all = TRUE;

		for (t = 0; t < group->terms->len; t++) {
			Term *term = g_ptr_array_index (group->terms, t);
			gboolean m = g_pattern_spec_match (term->pattern, len, name, reversed);

			if (m == term->negated) {
				all = FALSE;
				break;
			}
		}
		any = all;
	}

	g_free (reversed);
	return any;
}

void
nolphin_search_expression_free (NolphinSearchExpression *expr)
{
	if (expr != NULL) {
		g_ptr_array_unref (expr->groups);
		g_free (expr);
	}
}
