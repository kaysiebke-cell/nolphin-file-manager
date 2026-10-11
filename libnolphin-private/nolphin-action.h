/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-
 
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
  
   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
   Boston, MA 02110-1335, USA.

*/

#ifndef NOLPHIN_ACTION_H
#define NOLPHIN_ACTION_H

#include <gtk/gtk.h>
#include <glib.h>
#include "nolphin-file.h"

// GtkAction were deprecated before auto-free functionality was added.
G_DEFINE_AUTOPTR_CLEANUP_FUNC (GtkAction, g_object_unref)

#define NOLPHIN_TYPE_ACTION nolphin_action_get_type()
G_DECLARE_FINAL_TYPE (NolphinAction, nolphin_action, NOLPHIN, ACTION, GtkAction)

struct _NolphinAction {
    GtkAction parent_instance;

    gchar *uuid; // basename of key_file_path
    gchar *key_file_path;
    gchar *parent_dir;
    gboolean has_accel;
};

struct _NolphinActionClass {
    GtkActionClass parent_class;
};

NolphinAction   *nolphin_action_new                  (const gchar *name, const gchar *path);
void          nolphin_action_activate             (NolphinAction *action, GList *selection, NolphinFile *parent, GtkWindow *window);

const gchar  *nolphin_action_get_orig_label       (NolphinAction *action);
const gchar  *nolphin_action_get_orig_tt          (NolphinAction *action);
gchar        *nolphin_action_get_label            (NolphinAction *action, GList *selection, NolphinFile *parent, GtkWindow *window);
gchar        *nolphin_action_get_tt               (NolphinAction *action, GList *selection, NolphinFile *parent, GtkWindow *window);
void          nolphin_action_update_display_state (NolphinAction *action, GList *selection, NolphinFile *parent, gboolean for_places, GtkWindow *window);

// Layout model overrides
void          nolphin_action_override_label       (NolphinAction *action, const gchar *label);
void          nolphin_action_override_icon        (NolphinAction *action, const gchar *icon_name);
#endif /* NOLPHIN_ACTION_H */
