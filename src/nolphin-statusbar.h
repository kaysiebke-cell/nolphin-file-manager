/* nolphin-statusbar.h
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

#ifndef NOLPHIN_STATUSBAR_H
#define NOLPHIN_STATUSBAR_H

#include <gtk/gtk.h>
#include <gio/gio.h>
#include "nolphin-window.h"
#include "nolphin-window-slot.h"
#include "nolphin-view.h"

typedef struct _NolphinStatusBar      NolphinStatusBar;
typedef struct _NolphinStatusBarClass NolphinStatusBarClass;


#define NOLPHIN_TYPE_STATUS_BAR                 (nolphin_status_bar_get_type ())
#define NOLPHIN_STATUS_BAR(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), NOLPHIN_TYPE_STATUS_BAR, NolphinStatusBar))
#define NOLPHIN_STATUS_BAR_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), NOLPHIN_TYPE_STATUS_BAR, NolphinStatusBarClass))
#define NOLPHIN_IS_STATUS_BAR(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NOLPHIN_TYPE_STATUS_BAR))
#define NOLPHIN_IS_STATUS_BAR_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), NOLPHIN_TYPE_STATUS_BAR))
#define NOLPHIN_STATUS_BAR_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), NOLPHIN_TYPE_STATUS_BAR, NolphinStatusBarClass))

#define NOLPHIN_STATUSBAR_ICON_SIZE_NAME "statusbar-icon"
#define NOLPHIN_STATUSBAR_ICON_SIZE 11

struct _NolphinStatusBar
{
    GtkBox parent;
    NolphinWindow *window;
    GtkWidget *real_statusbar;

    GtkWidget *zoom_slider;

    GtkWidget *tree_button;
    GtkWidget *places_button;
    GtkWidget *show_button;
    GtkWidget *hide_button;
    GtkWidget *separator;
};

struct _NolphinStatusBarClass
{
    GtkBoxClass parent_class;
};

GType    nolphin_status_bar_get_type (void) G_GNUC_CONST;

GtkWidget *nolphin_status_bar_new (NolphinWindow *window);

GtkWidget *nolphin_status_bar_get_real_statusbar (NolphinStatusBar *bar);

void       nolphin_status_bar_sync_button_states (NolphinStatusBar *bar);

void       nolphin_status_bar_sync_zoom_widgets (NolphinStatusBar *bar);

#endif /* NOLPHIN_STATUSBAR_H */
