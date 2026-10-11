/*
 *  nolphin-menu.h - Menus exported by NolphinMenuProvider objects.
 *
 *  Copyright (C) 2005 Raffaele Sandrini
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
 *  Author:  Raffaele Sandrini <rasa@gmx.ch>
 *
 */

#include <config.h>
#include "nolphin-menu.h"
#include "nolphin-extension-i18n.h"

#include <glib.h>

typedef struct {
	GList *item_list;
} NolphinMenuPrivate;

struct _NolphinMenu
{
    GObject parent_class;

    NolphinMenuPrivate *priv;
};

G_DEFINE_TYPE_WITH_PRIVATE (NolphinMenu, nolphin_menu, G_TYPE_OBJECT)

/**
 * SECTION:nolphin-menu
 * @Title: NolphinMenu
 * @Short_description: A menu added to Nolphin's context menus by an extension
 *
 * Menu items and submenus can be added to Nolphin's selected item and background
 * context menus by a #NolphinMenuProvider.  Separators and embedded widgets are also
 * possible (see #NolphinSimpleButton.)
 **/

void
nolphin_menu_append_item (NolphinMenu *menu, NolphinMenuItem *item)
{
	g_return_if_fail (menu != NULL);
	g_return_if_fail (item != NULL);
	
	menu->priv->item_list = g_list_append (menu->priv->item_list, g_object_ref (item));
}

/**
 * nolphin_menu_get_items:
 * @menu: a #NolphinMenu
 *
 * Returns: (element-type NolphinMenuItem) (transfer full): the provided #NolphinMenuItem list
 */
GList *
nolphin_menu_get_items (NolphinMenu *menu)
{
	GList *item_list;

	g_return_val_if_fail (menu != NULL, NULL);
	
	item_list = g_list_copy (menu->priv->item_list);
	g_list_foreach (item_list, (GFunc)g_object_ref, NULL);
	
	return item_list;
}

/**
 * nolphin_menu_item_list_free:
 * @item_list: (element-type NolphinMenuItem): a list of #NolphinMenuItem
 *
 */
void
nolphin_menu_item_list_free (GList *item_list)
{
	g_return_if_fail (item_list != NULL);
	
	g_list_foreach (item_list, (GFunc)g_object_unref, NULL);
	g_list_free (item_list);
}

/* Type initialization */

static void
nolphin_menu_finalize (GObject *object)
{
	NolphinMenu *menu = NOLPHIN_MENU (object);

	if (menu->priv->item_list) {
        nolphin_menu_item_list_free (menu->priv->item_list);
	}

	G_OBJECT_CLASS (nolphin_menu_parent_class)->finalize (object);
}

static void
nolphin_menu_init (NolphinMenu *menu)
{
	menu->priv = G_TYPE_INSTANCE_GET_PRIVATE (menu, NOLPHIN_TYPE_MENU, NolphinMenuPrivate);

    menu->priv->item_list = NULL;
}

static void
nolphin_menu_class_init (NolphinMenuClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    object_class->finalize = nolphin_menu_finalize;
}

/* public constructors */

NolphinMenu *
nolphin_menu_new (void)
{
	NolphinMenu *obj;
	
	obj = NOLPHIN_MENU (g_object_new (NOLPHIN_TYPE_MENU, NULL));
	
	return obj;
}
