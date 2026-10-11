#ifndef _NOLPHIN_DESKTOP_OVERLAY_H_
#define _NOLPHIN_DESKTOP_OVERLAY_H_

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define NOLPHIN_TYPE_DESKTOP_OVERLAY (nolphin_desktop_overlay_get_type ())

G_DECLARE_FINAL_TYPE (NolphinDesktopOverlay, nolphin_desktop_overlay, NOLPHIN, DESKTOP_OVERLAY, GObject)

NolphinDesktopOverlay *nolphin_desktop_overlay_new (void);
void                nolphin_desktop_overlay_show (NolphinDesktopOverlay *overlay,
                                               gint                monitor);
void                nolphin_desktop_overlay_update_in_place (NolphinDesktopOverlay *overlay);
G_END_DECLS

#endif /* _NOLPHIN_DESKTOP_OVERLAY_H_ */