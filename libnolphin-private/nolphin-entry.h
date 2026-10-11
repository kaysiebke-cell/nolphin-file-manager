/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* NolphinEntry: one-line text editing widget. This consists of bug fixes
 * and other improvements to GtkEntry, and all the changes could be rolled
 * into GtkEntry some day.
 *
 * Copyright (C) 2000 Eazel, Inc.
 *
 * Author: John Sullivan <sullivan@eazel.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
 * Boston, MA 02110-1335, USA.
 */

#ifndef NOLPHIN_ENTRY_H
#define NOLPHIN_ENTRY_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define NOLPHIN_TYPE_ENTRY nolphin_entry_get_type()
#define NOLPHIN_ENTRY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NOLPHIN_TYPE_ENTRY, NolphinEntry))
#define NOLPHIN_ENTRY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NOLPHIN_TYPE_ENTRY, NolphinEntryClass))
#define NOLPHIN_IS_ENTRY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NOLPHIN_TYPE_ENTRY))
#define NOLPHIN_IS_ENTRY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NOLPHIN_TYPE_ENTRY))
#define NOLPHIN_ENTRY_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NOLPHIN_TYPE_ENTRY, NolphinEntryClass))

typedef struct NolphinEntryDetails NolphinEntryDetails;

typedef struct {
	GtkEntry parent;
	NolphinEntryDetails *details;
} NolphinEntry;

typedef struct {
	GtkEntryClass parent_class;

	void (*user_changed)      (NolphinEntry *entry);
	void (*selection_changed) (NolphinEntry *entry);
} NolphinEntryClass;

GType       nolphin_entry_get_type                 (void);
GtkWidget  *nolphin_entry_new                      (void);
GtkWidget  *nolphin_entry_new_with_max_length      (guint16        max);
void        nolphin_entry_set_text                 (NolphinEntry *entry,
						     const char    *text);
void        nolphin_entry_select_all               (NolphinEntry *entry);
void        nolphin_entry_select_all_at_idle       (NolphinEntry *entry);
void        nolphin_entry_set_special_tab_handling (NolphinEntry *entry,
						     gboolean       special_tab_handling);

G_END_DECLS

#endif /* NOLPHIN_ENTRY_H */
