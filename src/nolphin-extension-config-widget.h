/* nolphin-extension-config-widget.h */

/*  A widget that displays a list of extensions to enable or disable.
 *  This is usually part of a NolphinPluginManagerWidget
 */

#ifndef __NOLPHIN_EXTENSION_CONFIG_WIDGET_H__
#define __NOLPHIN_EXTENSION_CONFIG_WIDGET_H__

#include <glib-object.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "nolphin-config-base-widget.h"

G_BEGIN_DECLS

#define NOLPHIN_TYPE_EXTENSION_CONFIG_WIDGET (nolphin_extension_config_widget_get_type())

#define NOLPHIN_EXTENSION_CONFIG_WIDGET(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), NOLPHIN_TYPE_EXTENSION_CONFIG_WIDGET, NolphinExtensionConfigWidget))
#define NOLPHIN_EXTENSION_CONFIG_WIDGET_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), NOLPHIN_TYPE_EXTENSION_CONFIG_WIDGET, NolphinExtensionConfigWidgetClass))
#define NOLPHIN_IS_EXTENSION_CONFIG_WIDGET(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NOLPHIN_TYPE_EXTENSION_CONFIG_WIDGET))
#define NOLPHIN_IS_EXTENSION_CONFIG_WIDGET_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), NOLPHIN_TYPE_EXTENSION_CONFIG_WIDGET))
#define NOLPHIN_EXTENSION_CONFIG_WIDGET_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), NOLPHIN_TYPE_EXTENSION_CONFIG_WIDGET, NolphinExtensionConfigWidgetClass))

typedef struct _NolphinExtensionConfigWidget NolphinExtensionConfigWidget;
typedef struct _NolphinExtensionConfigWidgetClass NolphinExtensionConfigWidgetClass;

struct _NolphinExtensionConfigWidget
{
  NolphinConfigBaseWidget parent;
  GtkWidget *restart_button;

  GList *current_extensions;
  GList *initial_extension_ids;

  gulong bl_handler;
};

struct _NolphinExtensionConfigWidgetClass
{
  NolphinConfigBaseWidgetClass parent_class;
};

GType nolphin_extension_config_widget_get_type (void);

GtkWidget  *nolphin_extension_config_widget_new                   (void);

G_END_DECLS

#endif /* __NOLPHIN_EXTENSION_CONFIG_WIDGET_H__ */
