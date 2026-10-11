/* nolphin-desktop-manager.h */

#ifndef _NOLPHIN_DESKTOP_MANAGER_H
#define _NOLPHIN_DESKTOP_MANAGER_H

#include <glib-object.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include "nolphin-window.h"

G_BEGIN_DECLS

#define NOLPHIN_TYPE_DESKTOP_MANAGER nolphin_desktop_manager_get_type ()

G_DECLARE_FINAL_TYPE (NolphinDesktopManager, nolphin_desktop_manager, NOLPHIN, DESKTOP_MANAGER, GObject)

typedef enum {
    DESKTOP_ARRANGE_VERTICAL,
    DESKTOP_ARRANGE_HORIZONTAL
} NolphinDesktopLayoutDirection;

NolphinDesktopManager* nolphin_desktop_manager_get (void);

gboolean nolphin_desktop_manager_has_desktop_windows (NolphinDesktopManager *manager);
gboolean nolphin_desktop_manager_get_monitor_is_active (NolphinDesktopManager *manager,
                                                                   gint  monitor);
gboolean nolphin_desktop_manager_get_monitor_is_primary (NolphinDesktopManager *manager,
                                                                   gint  monitor);

gboolean nolphin_desktop_manager_get_primary_only (NolphinDesktopManager *manager);
void     nolphin_desktop_manager_get_window_rect_for_monitor (NolphinDesktopManager *manager,
                                                           gint                monitor,
                                                           GdkRectangle       *rect);
gboolean nolphin_desktop_manager_has_good_workarea_info (NolphinDesktopManager *manager);

void     nolphin_desktop_manager_get_margins             (NolphinDesktopManager *manager,
                                                       gint                monitor,
                                                       gint               *left,
                                                       gint               *right,
                                                       gint               *top,
                                                       gint               *bottom);

GtkWindow *nolphin_desktop_manager_get_window_for_monitor  (NolphinDesktopManager *manager,
                                                         gint                monitor);
void nolphin_desktop_manager_get_overlay_info              (NolphinDesktopManager *manager,
                                                         gint                monitor,
                                                         GtkActionGroup    **action_group,
                                                         gint               *h_adjust,
                                                         gint               *v_adjust);
void     nolphin_desktop_manager_show_desktop_overlay    (NolphinDesktopManager *manager,
                                                       gint                initial_monitor);
gboolean nolphin_desktop_manager_get_is_cinnamon         (NolphinDesktopManager *manager);

G_END_DECLS

#endif /* _NOLPHIN_DESKTOP_MANAGER_H */
