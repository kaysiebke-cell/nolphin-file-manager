/*
 *  Copyright © 2002 Christophe Fergeau
 *  Copyright © 2003 Marco Pesenti Gritti
 *  Copyright © 2003, 2004 Christian Persch
 *    (ephy-notebook.c)
 *
 *  Copyright © 2008 Free Software Foundation, Inc.
 *    (nolphin-notebook.c)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *  $Id: nolphin-notebook.h 8210 2008-04-11 20:05:25Z chpe $
 */

#ifndef NOLPHIN_NOTEBOOK_H
#define NOLPHIN_NOTEBOOK_H

#include <glib.h>
#include <gtk/gtk.h>
#include "nolphin-window-slot.h"

G_BEGIN_DECLS

#define NOLPHIN_TYPE_NOTEBOOK		(nolphin_notebook_get_type ())
#define NOLPHIN_NOTEBOOK(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), NOLPHIN_TYPE_NOTEBOOK, NolphinNotebook))
#define NOLPHIN_NOTEBOOK_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), NOLPHIN_TYPE_NOTEBOOK, NolphinNotebookClass))
#define NOLPHIN_IS_NOTEBOOK(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), NOLPHIN_TYPE_NOTEBOOK))
#define NOLPHIN_IS_NOTEBOOK_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), NOLPHIN_TYPE_NOTEBOOK))
#define NOLPHIN_NOTEBOOK_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), NOLPHIN_TYPE_NOTEBOOK, NolphinNotebookClass))

typedef struct _NolphinNotebookClass	NolphinNotebookClass;
typedef struct _NolphinNotebook		NolphinNotebook;

struct _NolphinNotebook
{
	GtkNotebook parent;
};

struct _NolphinNotebookClass
{
        GtkNotebookClass parent_class;

	/* Signals */
	void	 (* tab_close_request)  (NolphinNotebook *notebook,
					 NolphinWindowSlot *slot);
};

GType		nolphin_notebook_get_type		(void);

int		nolphin_notebook_add_tab	(NolphinNotebook *nb,
						 NolphinWindowSlot *slot,
						 int position,
						 gboolean jump_to);
gint		nolphin_notebook_find_tab_num_at_pos (NolphinNotebook *nb,
						   gint 	 abs_x,
						   gint 	 abs_y);
	
void		nolphin_notebook_set_show_tabs	(NolphinNotebook *nb,
						 gboolean show_tabs);

void		nolphin_notebook_set_dnd_enabled (NolphinNotebook *nb,
						   gboolean enabled);
void		nolphin_notebook_update_tabs_visibility (NolphinNotebook *nb);
void		nolphin_notebook_sync_tab_label (NolphinNotebook *nb,
						  NolphinWindowSlot *slot);
void		nolphin_notebook_sync_loading   (NolphinNotebook *nb,
						  NolphinWindowSlot *slot);

void		nolphin_notebook_reorder_child_relative (NolphinNotebook *notebook,
						      int	    page_num,
						      int 	    offset);
void		nolphin_notebook_set_current_page_relative (NolphinNotebook *notebook,
							     int offset);

gboolean        nolphin_notebook_can_reorder_child_relative (NolphinNotebook *notebook,
							  int	    	page_num,
							  int 	    	offset);
gboolean        nolphin_notebook_can_set_current_page_relative (NolphinNotebook *notebook,
								 int offset);

G_END_DECLS

#endif /* NOLPHIN_NOTEBOOK_H */

