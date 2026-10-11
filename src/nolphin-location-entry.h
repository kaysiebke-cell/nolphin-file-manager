/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nolphin
 *
 * Copyright (C) 2000 Eazel, Inc.
 *
 * Nolphin is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nolphin is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; see the file COPYING.  If not,
 * write to the Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
 * Boston, MA 02110-1335, USA.
 *
 * Author: Maciej Stachowiak <mjs@eazel.com>
 *         Ettore Perazzoli <ettore@gnu.org>
 */

#ifndef NOLPHIN_LOCATION_ENTRY_H
#define NOLPHIN_LOCATION_ENTRY_H

#include <libnolphin-private/nolphin-entry.h>

#define NOLPHIN_TYPE_LOCATION_ENTRY nolphin_location_entry_get_type()
#define NOLPHIN_LOCATION_ENTRY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NOLPHIN_TYPE_LOCATION_ENTRY, NolphinLocationEntry))
#define NOLPHIN_LOCATION_ENTRY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NOLPHIN_TYPE_LOCATION_ENTRY, NolphinLocationEntryClass))
#define NOLPHIN_IS_LOCATION_ENTRY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NOLPHIN_TYPE_LOCATION_ENTRY))
#define NOLPHIN_IS_LOCATION_ENTRY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NOLPHIN_TYPE_LOCATION_ENTRY))
#define NOLPHIN_LOCATION_ENTRY_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NOLPHIN_TYPE_LOCATION_ENTRY, NolphinLocationEntryClass))

typedef struct NolphinLocationEntryDetails NolphinLocationEntryDetails;

typedef struct NolphinLocationEntry {
	NolphinEntry parent;
	NolphinLocationEntryDetails *details;
} NolphinLocationEntry;

typedef struct {
	NolphinEntryClass parent_class;
} NolphinLocationEntryClass;

typedef enum {
	NOLPHIN_LOCATION_ENTRY_ACTION_GOTO,
	NOLPHIN_LOCATION_ENTRY_ACTION_CLEAR
} NolphinLocationEntryAction;

GType      nolphin_location_entry_get_type     	(void);
GtkWidget* nolphin_location_entry_new          	(void);
void       nolphin_location_entry_set_special_text     (NolphinLocationEntry *entry,
							 const char            *special_text);
void       nolphin_location_entry_set_secondary_action (NolphinLocationEntry *entry,
							 NolphinLocationEntryAction secondary_action);
NolphinLocationEntryAction nolphin_location_entry_get_secondary_action (NolphinLocationEntry *entry);
void       nolphin_location_entry_update_current_location (NolphinLocationEntry *entry,
							    const char *path);

#endif /* NOLPHIN_LOCATION_ENTRY_H */
