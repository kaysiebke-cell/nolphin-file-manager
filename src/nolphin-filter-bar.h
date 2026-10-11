/* nolphin-filter-bar.h
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

#ifndef NOLPHIN_FILTER_BAR_H
#define NOLPHIN_FILTER_BAR_H

#include <gtk/gtk.h>

#define NOLPHIN_TYPE_FILTER_BAR nolphin_filter_bar_get_type ()

G_DECLARE_FINAL_TYPE (NolphinFilterBar, nolphin_filter_bar, NOLPHIN, FILTER_BAR, GtkBox)

GtkWidget  *nolphin_filter_bar_new             (void);
const char *nolphin_filter_bar_get_text        (NolphinFilterBar *bar);
void        nolphin_filter_bar_set_text        (NolphinFilterBar *bar,
                                             const char    *text);

#endif /* NOLPHIN_FILTER_BAR_H */
