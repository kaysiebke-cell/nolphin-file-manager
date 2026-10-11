/* nolphin-simple-button.h */

#ifndef __NOLPHIN_SIMPLE_BUTTON_H__
#define __NOLPHIN_SIMPLE_BUTTON_H__

#include <glib-object.h>
#include <gtk/gtk.h>
#include "nolphin-extension-types.h"

G_BEGIN_DECLS

#define NOLPHIN_TYPE_SIMPLE_BUTTON nolphin_simple_button_get_type()

G_DECLARE_FINAL_TYPE (NolphinSimpleButton, nolphin_simple_button, NOLPHIN, SIMPLE_BUTTON, GtkButton)

NolphinSimpleButton *nolphin_simple_button_new (void);
NolphinSimpleButton *nolphin_simple_button_new_from_icon_name (const gchar *icon_name, int icon_size);
NolphinSimpleButton *nolphin_simple_button_new_from_stock (const gchar *stock_id, int icon_size);
NolphinSimpleButton *nolphin_simple_button_new_from_file (const gchar *path, int icon_size);

G_END_DECLS

#endif /* __NOLPHIN_SIMPLE_BUTTON_H__ */
