/* nolphin-plugin-manager.c */

/*  A GtkWidget that can be inserted into a UI that provides a simple interface for
 *  managing the loading of extensions, actions and scripts
 */

#include <config.h>
#include "nolphin-plugin-manager.h"
#include "nolphin-action-config-widget.h"
#include "nolphin-template-config-widget.h"
#include "nolphin-extension-config-widget.h"
#include <glib.h>

struct _NolphinPluginManager
{
    GtkBin parent_instance;
};

G_DEFINE_TYPE (NolphinPluginManager, nolphin_plugin_manager, GTK_TYPE_BIN);

static void
nolphin_plugin_manager_class_init (NolphinPluginManagerClass *klass)
{
}

static void
nolphin_plugin_manager_init (NolphinPluginManager *self)
{
    GtkWidget *widget, *grid;

    grid = gtk_grid_new ();

    gtk_widget_set_margin_left (grid, 4);
    gtk_widget_set_margin_right (grid, 4);
    gtk_widget_set_margin_top (grid, 4);
    gtk_widget_set_margin_bottom (grid, 4);
    gtk_grid_set_row_spacing (GTK_GRID (grid), 4);
    gtk_grid_set_column_spacing (GTK_GRID (grid), 4);
    gtk_grid_set_row_homogeneous (GTK_GRID (grid), TRUE);
    gtk_grid_set_column_homogeneous (GTK_GRID (grid), TRUE);

    widget = nolphin_action_config_widget_new ();
    gtk_grid_attach (GTK_GRID (grid), widget, 0, 0, 2, 1);

    widget = nolphin_extension_config_widget_new ();
    gtk_grid_attach (GTK_GRID (grid), widget, 0, 1, 2, 1);

    gtk_container_add (GTK_CONTAINER (self), grid);

    gtk_widget_show_all (GTK_WIDGET (self));
}

NolphinPluginManager *
nolphin_plugin_manager_new (void)
{
  return g_object_new (NOLPHIN_TYPE_PLUGIN_MANAGER, NULL);
}

