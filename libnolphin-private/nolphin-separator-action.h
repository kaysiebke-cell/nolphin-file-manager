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

#ifndef NOLPHIN_SEPARATOR_ACTION_H
#define NOLPHIN_SEPARATOR_ACTION_H

#include <gtk/gtk.h>

#define NOLPHIN_TYPE_SEPARATOR_ACTION nolphin_separator_action_get_type()
#define NOLPHIN_SEPARATOR_ACTION(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NOLPHIN_TYPE_SEPARATOR_ACTION, NolphinSeparatorAction))
#define NOLPHIN_SEPARATOR_ACTION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NOLPHIN_TYPE_SEPARATOR_ACTION, NolphinSeparatorActionClass))
#define NOLPHIN_IS_SEPARATOR_ACTION(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NOLPHIN_TYPE_SEPARATOR_ACTION))
#define NOLPHIN_IS_SEPARATOR_ACTION_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NOLPHIN_TYPE_SEPARATOR_ACTION))
#define NOLPHIN_SEPARATOR_ACTION_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NOLPHIN_TYPE_SEPARATOR_ACTION, NolphinSeparatorActionClass))

typedef struct _NolphinSeparatorAction NolphinSeparatorAction;
typedef struct _NolphinSeparatorActionClass NolphinSeparatorActionClass;

struct _NolphinSeparatorAction {
    GtkAction parent;
};

struct _NolphinSeparatorActionClass {
	GtkActionClass parent_class;
};

GType         nolphin_separator_action_get_type             (void);
GtkAction    *nolphin_separator_action_new                  (const gchar *name);

#endif /* NOLPHIN_SEPARATOR_ACTION_H */
