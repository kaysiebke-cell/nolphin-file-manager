/* nolphin-plugin-manager.h */

/*  A GtkWidget that can be inserted into a UI that provides a simple interface for
 *  managing the loading of extensions, actions and scripts
 */

#ifndef __NOLPHIN_PLUGIN_MANAGER_H__
#define __NOLPHIN_PLUGIN_MANAGER_H__

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define NOLPHIN_TYPE_PLUGIN_MANAGER (nolphin_plugin_manager_get_type())

G_DECLARE_FINAL_TYPE (NolphinPluginManager, nolphin_plugin_manager, NOLPHIN, PLUGIN_MANAGER, GtkBin)

NolphinPluginManager       *nolphin_plugin_manager_new                   (void);

G_END_DECLS

#endif /* __NOLPHIN_PLUGIN_MANAGER_H__ */
