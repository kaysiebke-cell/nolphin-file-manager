/*
 *  nolphin-menu.h - Menus exported by NolphinMenuProvider objects.
 *
 *  Copyright (C) 2005 Raffaele Sandrini
 *  Copyright (C) 2003 Novell, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 51 Franklin Street, Suite 500, MA 02110-1335, USA.
 * 
 *  Author:  Dave Camp <dave@ximian.com>
 *           Raffaele Sandrini <rasa@gmx.ch>
 *
 */

#ifndef NOLPHIN_MENU_H
#define NOLPHIN_MENU_H

#include <glib-object.h>
#include <gtk/gtk.h>
#include "nolphin-extension-types.h"

G_BEGIN_DECLS

/* NolphinMenu defines */
#define NOLPHIN_TYPE_MENU         nolphin_menu_get_type ()
G_DECLARE_FINAL_TYPE (NolphinMenu, nolphin_menu, NOLPHIN, MENU, GObject)
/* NolphinMenuItem defines */
#define NOLPHIN_TYPE_MENU_ITEM    nolphin_menu_item_get_type()
G_DECLARE_FINAL_TYPE (NolphinMenuItem, nolphin_menu_item, NOLPHIN, MENU_ITEM, GObject)

/* NolphinMenu methods */
NolphinMenu *	nolphin_menu_new	(void);

void	nolphin_menu_append_item	(NolphinMenu      *menu,
					 NolphinMenuItem  *item);
GList*	nolphin_menu_get_items		(NolphinMenu *menu);
void	nolphin_menu_item_list_free	(GList *item_list);

/* NolphinMenuItem methods */
NolphinMenuItem *nolphin_menu_item_new           (const char       *name,
						    const char       *label,
						    const char       *tip,
						    const char       *icon);

NolphinMenuItem *nolphin_menu_item_new_widget (const char *name,
                                         GtkWidget  *widget_a,
                                         GtkWidget  *widget_b);

NolphinMenuItem *nolphin_menu_item_new_separator (const char *name);

void nolphin_menu_item_set_widget_a (NolphinMenuItem *item, GtkWidget *widget);
void nolphin_menu_item_set_widget_b (NolphinMenuItem *item, GtkWidget *widget);

void              nolphin_menu_item_activate      (NolphinMenuItem *item);
void              nolphin_menu_item_set_submenu   (NolphinMenuItem *item,
						    NolphinMenu     *menu);
/* NolphinMenuItem has the following properties:
 *   name (string)        - the identifier for the menu item
 *   label (string)       - the user-visible label of the menu item
 *   tip (string)         - the tooltip of the menu item 
 *   icon (string)        - the name of the icon to display in the menu item
 *   sensitive (boolean)  - whether the menu item is sensitive or not
 *   priority (boolean)   - used for toolbar items, whether to show priority
 *                          text.
 *   menu (NolphinMenu)  - The menu belonging to this item. May be null.
 *   widget (GtkWidget) - The optional widget to use in place of a normal menu entr
 */

G_END_DECLS

#endif /* NOLPHIN_MENU_H */
