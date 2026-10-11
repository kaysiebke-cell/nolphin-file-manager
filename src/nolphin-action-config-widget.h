/* nolphin-action-config-widget.h */

/*  A widget that displays a list of actions to enable or disable.
 *  This is usually part of a NolphinPluginManagerWidget
 */

#ifndef __NOLPHIN_ACTION_CONFIG_WIDGET_H__
#define __NOLPHIN_ACTION_CONFIG_WIDGET_H__

#include <glib-object.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "nolphin-config-base-widget.h"

G_BEGIN_DECLS

#define NOLPHIN_TYPE_ACTION_CONFIG_WIDGET (nolphin_action_config_widget_get_type())

#define NOLPHIN_ACTION_CONFIG_WIDGET(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), NOLPHIN_TYPE_ACTION_CONFIG_WIDGET, NolphinActionConfigWidget))
#define NOLPHIN_ACTION_CONFIG_WIDGET_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), NOLPHIN_TYPE_ACTION_CONFIG_WIDGET, NolphinActionConfigWidgetClass))
#define NOLPHIN_IS_ACTION_CONFIG_WIDGET(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NOLPHIN_TYPE_ACTION_CONFIG_WIDGET))
#define NOLPHIN_IS_ACTION_CONFIG_WIDGET_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), NOLPHIN_TYPE_ACTION_CONFIG_WIDGET))
#define NOLPHIN_ACTION_CONFIG_WIDGET_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), NOLPHIN_TYPE_ACTION_CONFIG_WIDGET, NolphinActionConfigWidgetClass))

typedef struct _NolphinActionConfigWidget NolphinActionConfigWidget;
typedef struct _NolphinActionConfigWidgetClass NolphinActionConfigWidgetClass;

struct _NolphinActionConfigWidget
{
  NolphinConfigBaseWidget parent;

  GList *actions;

  GList *dir_monitors;
  gulong bl_handler;
};

struct _NolphinActionConfigWidgetClass
{
  NolphinConfigBaseWidgetClass parent_class;
};

GType nolphin_action_config_widget_get_type (void);

GtkWidget  *nolphin_action_config_widget_new                   (void);

G_END_DECLS

#endif /* __NOLPHIN_ACTION_CONFIG_WIDGET_H__ */
