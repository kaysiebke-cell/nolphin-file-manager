#ifndef NOLPHIN_ICON_INFO_H
#define NOLPHIN_ICON_INFO_H

#include <glib-object.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdk.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

/* Names for Nolphin's different zoom levels, from tiniest items to largest items */
typedef enum {
    NOLPHIN_ZOOM_LEVEL_NULL = -1,
	NOLPHIN_ZOOM_LEVEL_SMALLEST = 0,
	NOLPHIN_ZOOM_LEVEL_SMALLER,
	NOLPHIN_ZOOM_LEVEL_SMALL,
	NOLPHIN_ZOOM_LEVEL_STANDARD,
	NOLPHIN_ZOOM_LEVEL_LARGE,
	NOLPHIN_ZOOM_LEVEL_LARGER,
	NOLPHIN_ZOOM_LEVEL_LARGEST
} NolphinZoomLevel;

#define NOLPHIN_ZOOM_LEVEL_N_ENTRIES (NOLPHIN_ZOOM_LEVEL_LARGEST + 1)

/* Nominal icon sizes for each Nolphin zoom level.
 * This scheme assumes that icons are designed to
 * fit in a square space, though each image needn't
 * be square. Since individual icons can be stretched,
 * each icon is not constrained to this nominal size.
 */

#define NOLPHIN_COMPACT_FORCED_ICON_SIZE 16

#define NOLPHIN_LIST_ICON_SIZE_SMALLEST 16
#define NOLPHIN_LIST_ICON_SIZE_SMALLER  16
#define NOLPHIN_LIST_ICON_SIZE_SMALL    24
#define NOLPHIN_LIST_ICON_SIZE_STANDARD 32
#define NOLPHIN_LIST_ICON_SIZE_LARGE    48
#define NOLPHIN_LIST_ICON_SIZE_LARGER   72
#define NOLPHIN_LIST_ICON_SIZE_LARGEST  96

#define NOLPHIN_ICON_SIZE_SMALLEST 24
#define NOLPHIN_ICON_SIZE_SMALLER  32
#define NOLPHIN_ICON_SIZE_SMALL    48
#define NOLPHIN_ICON_SIZE_STANDARD 64
#define NOLPHIN_ICON_SIZE_LARGE    96
#define NOLPHIN_ICON_SIZE_LARGER   128
#define NOLPHIN_ICON_SIZE_LARGEST  256

#define NOLPHIN_DESKTOP_ICON_SIZE_SMALLER 24
#define NOLPHIN_DESKTOP_ICON_SIZE_SMALL 32
#define NOLPHIN_DESKTOP_ICON_SIZE_STANDARD 48
#define NOLPHIN_DESKTOP_ICON_SIZE_LARGE 64
#define NOLPHIN_DESKTOP_ICON_SIZE_LARGER 96

#define NOLPHIN_DESKTOP_TEXT_WIDTH_SMALLER 64
#define NOLPHIN_DESKTOP_TEXT_WIDTH_SMALL 84
#define NOLPHIN_DESKTOP_TEXT_WIDTH_STANDARD 110
#define NOLPHIN_DESKTOP_TEXT_WIDTH_LARGE 150
#define NOLPHIN_DESKTOP_TEXT_WIDTH_LARGER 200

#define NOLPHIN_ICON_TEXT_WIDTH_SMALLEST  0
#define NOLPHIN_ICON_TEXT_WIDTH_SMALLER   64
#define NOLPHIN_ICON_TEXT_WIDTH_SMALL     84
#define NOLPHIN_ICON_TEXT_WIDTH_STANDARD  110
#define NOLPHIN_ICON_TEXT_WIDTH_LARGE     96
#define NOLPHIN_ICON_TEXT_WIDTH_LARGER    128
#define NOLPHIN_ICON_TEXT_WIDTH_LARGEST   256

/* Maximum size of an icon that the icon factory will ever produce */
#define NOLPHIN_ICON_MAXIMUM_SIZE     320

typedef struct {
    gint ref_count;
    gboolean sole_owner;
    gint64 last_use_time;
    GdkPixbuf *pixbuf;

    char *icon_name;
    gint orig_scale;
} NolphinIconInfo;

NolphinIconInfo *    nolphin_icon_info_ref                          (NolphinIconInfo      *icon);
void              nolphin_icon_info_unref                        (NolphinIconInfo      *icon);
void              nolphin_icon_info_clear                        (NolphinIconInfo     **info);
NolphinIconInfo *    nolphin_icon_info_new_for_pixbuf               (GdkPixbuf         *pixbuf,
                                                               int                scale);
NolphinIconInfo *    nolphin_icon_info_lookup                       (GIcon             *icon,
                                                               int                size,
                                                               int                scale);
NolphinIconInfo *    nolphin_icon_info_lookup_from_name             (const char        *name,
                                                               int                size,
                                                               int                scale);
NolphinIconInfo *    nolphin_icon_info_lookup_from_path             (const char        *path,
                                                               int                size,
                                                               int                scale);
gboolean              nolphin_icon_info_is_fallback                  (NolphinIconInfo  *icon);
GdkPixbuf *           nolphin_icon_info_get_pixbuf                   (NolphinIconInfo  *icon);
GdkPixbuf *           nolphin_icon_info_get_pixbuf_nodefault         (NolphinIconInfo  *icon);
GdkPixbuf *           nolphin_icon_info_get_pixbuf_nodefault_at_size (NolphinIconInfo  *icon,
								       gsize              forced_size);
GdkPixbuf *           nolphin_icon_info_get_pixbuf_at_size           (NolphinIconInfo  *icon,
								       gsize              forced_size);
GdkPixbuf *           nolphin_icon_info_get_desktop_pixbuf_at_size (NolphinIconInfo  *icon,
                                                                 gsize          max_height,
                                                                 gsize          max_width);
const char *          nolphin_icon_info_get_used_name                (NolphinIconInfo  *icon);

void                  nolphin_icon_info_clear_caches                 (void);

/* Relationship between zoom levels and icons sizes. */
guint nolphin_get_icon_size_for_zoom_level          (NolphinZoomLevel  zoom_level);
guint nolphin_get_icon_text_width_for_zoom_level    (NolphinZoomLevel  zoom_level);

guint nolphin_get_list_icon_size_for_zoom_level     (NolphinZoomLevel  zoom_level);

guint nolphin_get_desktop_icon_size_for_zoom_level  (NolphinZoomLevel  zoom_level);
guint nolphin_get_desktop_text_width_for_zoom_level (NolphinZoomLevel  zoom_level);

gint  nolphin_get_icon_size_for_stock_size          (GtkIconSize        size);
guint nolphin_icon_get_emblem_size_for_icon_size    (guint              size);

GIcon * nolphin_user_special_directory_get_gicon (GUserDirectory directory);


G_END_DECLS

#endif /* NOLPHIN_ICON_INFO_H */

