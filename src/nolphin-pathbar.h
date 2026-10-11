/* nolphin-pathbar.h
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
 *
 * 
 */

#ifndef NOLPHIN_PATHBAR_H
#define NOLPHIN_PATHBAR_H

#include <gtk/gtk.h>
#include <gio/gio.h>

typedef struct _NolphinPathBar      NolphinPathBar;
typedef struct _NolphinPathBarClass NolphinPathBarClass;
typedef struct _NolphinPathBarDetails NolphinPathBarDetails;

#define NOLPHIN_TYPE_PATH_BAR                 (nolphin_path_bar_get_type ())
#define NOLPHIN_PATH_BAR(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), NOLPHIN_TYPE_PATH_BAR, NolphinPathBar))
#define NOLPHIN_PATH_BAR_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), NOLPHIN_TYPE_PATH_BAR, NolphinPathBarClass))
#define NOLPHIN_IS_PATH_BAR(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NOLPHIN_TYPE_PATH_BAR))
#define NOLPHIN_IS_PATH_BAR_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), NOLPHIN_TYPE_PATH_BAR))
#define NOLPHIN_PATH_BAR_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), NOLPHIN_TYPE_PATH_BAR, NolphinPathBarClass))

struct _NolphinPathBar
{
	GtkContainer parent;
	
	NolphinPathBarDetails *priv;
};

struct _NolphinPathBarClass
{
	GtkContainerClass parent_class;

  	void (* path_clicked)   (NolphinPathBar  *path_bar,
				 GFile             *location);
  	void (* path_set)       (NolphinPathBar  *path_bar,
				 GFile             *location);
};

GType    nolphin_path_bar_get_type (void) G_GNUC_CONST;

gboolean nolphin_path_bar_set_path    (NolphinPathBar *path_bar, GFile *file);
GFile *  nolphin_path_bar_get_path_for_button (NolphinPathBar *path_bar,
						GtkWidget       *button);
void     nolphin_path_bar_clear_buttons (NolphinPathBar *path_bar);

#endif /* NOLPHIN_PATHBAR_H */
