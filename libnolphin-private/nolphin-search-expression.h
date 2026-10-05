/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nolphin-search-expression.h: Operatoren für die Dateinamensuche (§33)
 *
 * Syntax (Groß-/Kleinschreibung der Schlüsselwörter egal):
 *   a b            UND  (Leerzeichen, wie bisher)
 *   a ODER b       ODER (auch "OR" oder "|")
 *   NICHT a        NICHT (auch "NOT" oder "-a")
 *   "a b"          exakte Übereinstimmung des ganzen Namens
 *   a* ?a          Platzhalter
 * UND bindet stärker als ODER. Ein Begriff ohne Platzhalter bedeutet
 * "enthält". Der Aufrufer liefert Text und Namen bereits normalisiert
 * (und bei Bedarf kleingeschrieben).
 */

#ifndef NOLPHIN_SEARCH_EXPRESSION_H
#define NOLPHIN_SEARCH_EXPRESSION_H

#include <glib.h>

G_BEGIN_DECLS

typedef struct _NolphinSearchExpression NolphinSearchExpression;

NolphinSearchExpression *nolphin_search_expression_new     (const gchar *text);
gboolean                 nolphin_search_expression_matches (NolphinSearchExpression *expr,
							    const gchar *name);
void                     nolphin_search_expression_free    (NolphinSearchExpression *expr);

G_END_DECLS

#endif /* NOLPHIN_SEARCH_EXPRESSION_H */
