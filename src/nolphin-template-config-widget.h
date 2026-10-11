/* nolphin-script-config-widget.h */

/*  A widget that displays a list of scripts to enable or disable.
 *  This is usually part of a NolphinPluginManagerWidget
 */

#ifndef __NOLPHIN_TEMPLATE_CONFIG_WIDGET_H__
#define __NOLPHIN_TEMPLATE_CONFIG_WIDGET_H__

#include <glib-object.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "nolphin-config-base-widget.h"

G_BEGIN_DECLS

#define NOLPHIN_TYPE_TEMPLATE_CONFIG_WIDGET (nolphin_template_config_widget_get_type())

#define NOLPHIN_TEMPLATE_CONFIG_WIDGET(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), NOLPHIN_TYPE_TEMPLATE_CONFIG_WIDGET, NolphinTemplateConfigWidget))
#define NOLPHIN_TEMPLATE_CONFIG_WIDGET_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), NOLPHIN_TYPE_TEMPLATE_CONFIG_WIDGET, NolphinTemplateConfigWidgetClass))
#define NOLPHIN_IS_TEMPLATE_CONFIG_WIDGET(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NOLPHIN_TYPE_TEMPLATE_CONFIG_WIDGET))
#define NOLPHIN_IS_TEMPLATE_CONFIG_WIDGET_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), NOLPHIN_TYPE_TEMPLATE_CONFIG_WIDGET))
#define NOLPHIN_TEMPLATE_CONFIG_WIDGET_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), NOLPHIN_TYPE_TEMPLATE_CONFIG_WIDGET, NolphinTemplateConfigWidgetClass))

typedef struct _NolphinTemplateConfigWidget NolphinTemplateConfigWidget;
typedef struct _NolphinTemplateConfigWidgetClass NolphinTemplateConfigWidgetClass;

struct _NolphinTemplateConfigWidget
{
  NolphinConfigBaseWidget parent;

  GList *templates;

  GList *dir_monitors;
  GtkWidget *remove_button;
  GtkWidget *rename_button;
  GtkWidget *edit_button;
};

struct _NolphinTemplateConfigWidgetClass
{
  NolphinConfigBaseWidgetClass parent_class;
};

GType nolphin_template_config_widget_get_type (void);

GtkWidget  *nolphin_template_config_widget_new                   (void);

G_END_DECLS

#endif /* __NOLPHIN_TEMPLATE_CONFIG_WIDGET_H__ */
