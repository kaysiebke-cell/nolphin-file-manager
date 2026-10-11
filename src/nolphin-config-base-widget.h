/* nolphin-config-base-widget.h */

/*  A base widget class for extension/action/script config widgets.
 *  This is usually part of a NolphinPluginManagerWidget
 */

#ifndef __NOLPHIN_CONFIG_BASE_WIDGET_H__
#define __NOLPHIN_CONFIG_BASE_WIDGET_H__

#include <glib-object.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "nolphin-window-private.h"

G_BEGIN_DECLS

#define NOLPHIN_TYPE_CONFIG_BASE_WIDGET (nolphin_config_base_widget_get_type())

#define NOLPHIN_CONFIG_BASE_WIDGET(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), NOLPHIN_TYPE_CONFIG_BASE_WIDGET, NolphinConfigBaseWidget))
#define NOLPHIN_CONFIG_BASE_WIDGET_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), NOLPHIN_TYPE_CONFIG_BASE_WIDGET, NolphinConfigBaseWidgetClass))
#define NOLPHIN_IS_CONFIG_BASE_WIDGET(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NOLPHIN_TYPE_CONFIG_BASE_WIDGET))
#define NOLPHIN_IS_CONFIG_BASE_WIDGET_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), NOLPHIN_TYPE_CONFIG_BASE_WIDGET))
#define NOLPHIN_CONFIG_BASE_WIDGET_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), NOLPHIN_TYPE_CONFIG_BASE_WIDGET, NolphinConfigBaseWidgetClass))

typedef struct _NolphinConfigBaseWidget NolphinConfigBaseWidget;
typedef struct _NolphinConfigBaseWidgetClass NolphinConfigBaseWidgetClass;

struct _NolphinConfigBaseWidget
{
  GtkBin parent;

  GtkWidget *label;
  GtkWidget *listbox;
  GtkWidget *lbuttonbox;
  GtkWidget *rbuttonbox;
  GtkWidget *enable_button;
  GtkWidget *disable_button;
};

struct _NolphinConfigBaseWidgetClass
{
  GtkBinClass parent_class;
};

GType nolphin_config_base_widget_get_type (void);

GtkWidget *nolphin_config_base_widget_get_label          (NolphinConfigBaseWidget *widget);
GtkWidget *nolphin_config_base_widget_get_listbox        (NolphinConfigBaseWidget *widget);
GtkWidget *nolphin_config_base_widget_get_enable_button  (NolphinConfigBaseWidget *widget);
GtkWidget *nolphin_config_base_widget_get_disable_button (NolphinConfigBaseWidget *widget);

void       nolphin_config_base_widget_set_default_buttons_sensitive (NolphinConfigBaseWidget *widget, gboolean sensitive);

void       nolphin_config_base_widget_clear_list         (NolphinConfigBaseWidget *widget);
NolphinWindow *nolphin_config_base_widget_get_view_window   (NolphinConfigBaseWidget *widget, NolphinWindow *view_window);

G_END_DECLS

#endif /* __NOLPHIN_CONFIG_BASE_WIDGET_H__ */
