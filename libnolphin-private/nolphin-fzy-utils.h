/* nolphin-fzy-utils.h - Fuzzy matching utilities for Nolphin
 *
 * Copyright (C) 2026 Linux Mint
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef NOLPHIN_FZY_UTILS_H
#define NOLPHIN_FZY_UTILS_H

#include <glib.h>
#include <pango/pango.h>

char *nolphin_fzy_strip_combining_marks (const char *str,
                                      char      **nfkd_out);

PangoAttrList *nolphin_fzy_match_attrs (const char *needle,
                                     const char *haystack);

#endif /* NOLPHIN_FZY_UTILS_H */
