#ifndef _NOLPHIN_DESKTOP_PREFERENCES_H_
#define _NOLPHIN_DESKTOP_PREFERENCES_H_

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define NOLPHIN_TYPE_DESKTOP_PREFERENCES (nolphin_desktop_preferences_get_type ())

G_DECLARE_FINAL_TYPE (NolphinDesktopPreferences, nolphin_desktop_preferences, NOLPHIN, DESKTOP_PREFERENCES, GtkBin)

NolphinDesktopPreferences *nolphin_desktop_preferences_new (void);

G_END_DECLS

#endif /* _NOLPHIN_DESKTOP_PREFERENCES_H_ */