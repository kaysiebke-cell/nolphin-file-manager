/* -*- Mode: C; indent-tabs-mode: f; c-basic-offset: 4; tab-width: 4 -*- */

/* fm-icon-container.h - the container widget for file manager icons

   Copyright (C) 2002 Sun Microsystems, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
   Boston, MA 02110-1335, USA.

   Author: Michael Meeks <michael@ximian.com>
*/
#include <config.h>

#include "nolphin-icon-view-container.h"

#include <string.h>
#include <errno.h>
#include <math.h>

#include <glib/gi18n.h>
#include <gio/gio.h>
#include <eel/eel-glib-extensions.h>
#include "nolphin-icon-private.h"
#include <libnolphin-private/nolphin-global-preferences.h>
#include <libnolphin-private/nolphin-file-attributes.h>
#include <libnolphin-private/nolphin-metadata.h>
#include <libnolphin-private/nolphin-thumbnails.h>
#include <libnolphin-private/nolphin-desktop-icon-file.h>
#include <libnolphin-private/nolphin-desktop-utils.h>

#define DEBUG_FLAG NOLPHIN_DEBUG_ICON_CONTAINER
#include "nolphin-debug.h"

/* Maximum size (pixels) allowed for icons at the standard zoom level. */
#define MINIMUM_IMAGE_SIZE 24
#define MAXIMUM_IMAGE_SIZE 96

/* If icon size is bigger than this, request large embedded text.
 * Its selected so that the non-large text should fit in "normal" icon sizes
 */
#define ICON_SIZE_FOR_LARGE_EMBEDDED_TEXT 55

static void get_max_icon_dimensions (GList *icon_start,
                                     GList *icon_end,
                                     double *max_icon_width,
                                     double *max_icon_height,
                                     double *max_text_width,
                                     double *max_text_height,
                                     double *max_bounds_height);
static void find_empty_location (NolphinIconContainer *container,
                                 NolphinPlacementGrid *grid,
                                 NolphinIcon *icon,
                                 int start_x,
                                 int start_y,
                                 int *x,
                                 int *y);

G_DEFINE_TYPE (NolphinIconViewContainer, nolphin_icon_view_container, NOLPHIN_TYPE_ICON_CONTAINER);

static GQuark attribute_none_q;
static GQuark *caption_attributes = NULL;

static NolphinIconView *
get_icon_view (NolphinIconContainer *container)
{
	/* Type unsafe comparison for performance */
	return ((NolphinIconViewContainer *)container)->view;
}


static NolphinIconInfo *
nolphin_icon_view_container_get_icon_images (NolphinIconContainer *container,
                                          NolphinIconData      *data,
                                          int                size,
                                          gboolean           for_drag_accept,
					                      gboolean          *has_window_open,
                                          gboolean           visible)
{
	NolphinIconView *icon_view;
	NolphinFile *file;
	NolphinFileIconFlags flags;
	NolphinIconInfo *icon_info;
	GdkPixbuf *pixbuf;
	GIcon *emblemed_icon;
	GEmblem *emblem;
	GList *emblem_icons, *l;
    gint scale;

	file = (NolphinFile *) data;

	g_assert (NOLPHIN_IS_FILE (file));
	icon_view = get_icon_view (container);
	g_return_val_if_fail (icon_view != NULL, NULL);

	*has_window_open = nolphin_file_has_open_window (file);

	flags = NOLPHIN_FILE_ICON_FLAGS_USE_MOUNT_ICON_AS_EMBLEM |
			NOLPHIN_FILE_ICON_FLAGS_FORCE_THUMBNAIL_SIZE;

    if (visible) {
        flags |= NOLPHIN_FILE_ICON_FLAGS_USE_THUMBNAILS;
    }

	if (for_drag_accept) {
		flags |= NOLPHIN_FILE_ICON_FLAGS_FOR_DRAG_ACCEPT;
	}

	emblem_icons = nolphin_file_get_emblem_icons (file,
		                                       nolphin_view_get_directory_as_file (NOLPHIN_VIEW (icon_view)));

    scale = gtk_widget_get_scale_factor (GTK_WIDGET (icon_view));
	icon_info = nolphin_file_get_icon (file, size, size, scale, flags);

	/* apply emblems */
	if (emblem_icons != NULL) {
        gint w, h, s;
        gboolean bad_ratio;

        l = emblem_icons;

        pixbuf = nolphin_icon_info_get_pixbuf (icon_info);

        w = gdk_pixbuf_get_width (pixbuf);
        h = gdk_pixbuf_get_height (pixbuf);

        s = MAX (w, h);
        if (s < size)
            size = s;

        bad_ratio = (int)nolphin_icon_get_emblem_size_for_icon_size (size) * scale > (int)(w * 0.75) ||
                    (int)nolphin_icon_get_emblem_size_for_icon_size (size) * scale > (int)(h * 0.75);

        if (bad_ratio)
            goto skip_emblem; /* Would prefer to not use goto, but
                               * I don't want to do these checks on
                               * non-emblemed icons (the majority)
                               * as it would be too costly */

        emblem = g_emblem_new (l->data);

		emblemed_icon = g_emblemed_icon_new (G_ICON (pixbuf), emblem);
		g_object_unref (emblem);

		for (l = l->next; l != NULL; l = l->next) {
			emblem = g_emblem_new (l->data);
			g_emblemed_icon_add_emblem (G_EMBLEMED_ICON (emblemed_icon),
						    emblem);
			g_object_unref (emblem);
		}

        nolphin_icon_info_clear (&icon_info);
		icon_info = nolphin_icon_info_lookup (emblemed_icon, size, scale);
        g_object_unref (emblemed_icon);

skip_emblem:
		g_object_unref (pixbuf);

	}

	if (emblem_icons != NULL) {
		g_list_free_full (emblem_icons, g_object_unref);
	}

	return icon_info;
}

static char *
nolphin_icon_view_container_get_icon_description (NolphinIconContainer *container,
						   NolphinIconData      *data)
{
	NolphinFile *file;
	char *mime_type;
	const char *description;

	file = NOLPHIN_FILE (data);
	g_assert (NOLPHIN_IS_FILE (file));

	if (NOLPHIN_IS_DESKTOP_ICON_FILE (file)) {
		return NULL;
	}

	mime_type = nolphin_file_get_mime_type (file);
	description = g_content_type_get_description (mime_type);
	g_free (mime_type);
	return g_strdup (description);
}

static void
update_auto_strv_as_quarks (GSettings   *settings,
			    const gchar *key,
			    gpointer     user_data)
{
	GQuark **storage = user_data;
	int i = 0;
	char **value;

	value = g_settings_get_strv (settings, key);

	g_free (*storage);
	*storage = g_new (GQuark, g_strv_length (value) + 1);

	for (i = 0; value[i] != NULL; ++i) {
		(*storage)[i] = g_quark_from_string (value[i]);
	}
	(*storage)[i] = 0;

	g_strfreev (value);
}

/*
 * Get the preference for which caption text should appear
 * beneath icons.
 */
static GQuark *
nolphin_icon_view_container_get_icon_text_attributes_from_preferences (void)
{
	if (caption_attributes == NULL) {
		update_auto_strv_as_quarks (nolphin_icon_view_preferences,
					    NOLPHIN_PREFERENCES_ICON_VIEW_CAPTIONS,
					    &caption_attributes);
	}

	/* We don't need to sanity check the attributes list even though it came
	 * from preferences.
	 *
	 * There are 2 ways that the values in the list could be bad.
	 *
	 * 1) The user picks "bad" values.  "bad" values are those that result in
	 *    there being duplicate attributes in the list.
	 *
	 * 2) Value stored in GConf are tampered with.  Its possible physically do
	 *    this by pulling the rug underneath GConf and manually editing its
	 *    config files.  Its also possible to use a third party GConf key
	 *    editor and store garbage for the keys in question.
	 *
	 * Thankfully, the Nolphin preferences machinery deals with both of
	 * these cases.
	 *
	 * In the first case, the preferences dialog widgetry prevents
	 * duplicate attributes by making "bad" choices insensitive.
	 *
	 * In the second case, the preferences getter (and also the auto storage) for
	 * string_array values are always valid members of the enumeration associated
	 * with the preference.
	 *
	 * So, no more error checking on attributes is needed here and we can return
	 * a the auto stored value.
	 */
	return caption_attributes;
}

static int
quarkv_length (GQuark *attributes)
{
	int i;
	i = 0;
	while (attributes[i] != 0) {
		i++;
	}
	return i;
}

/**
 * nolphin_icon_view_get_icon_text_attribute_names:
 *
 * Get a list representing which text attributes should be displayed
 * beneath an icon. The result is dependent on zoom level and possibly
 * user configuration. Don't free the result.
 * @view: NolphinIconView to query.
 *
 **/
static GQuark *
nolphin_icon_view_container_get_icon_text_attribute_names (NolphinIconContainer *container,
							    int *len)
{
	GQuark *attributes;
	int piece_count;

	const int pieces_by_level[] = {
		0,	/* NOLPHIN_ZOOM_LEVEL_SMALLEST */
		0,	/* NOLPHIN_ZOOM_LEVEL_SMALLER */
		0,	/* NOLPHIN_ZOOM_LEVEL_SMALL */
		1,	/* NOLPHIN_ZOOM_LEVEL_STANDARD */
		2,	/* NOLPHIN_ZOOM_LEVEL_LARGE */
		2,	/* NOLPHIN_ZOOM_LEVEL_LARGER */
		3	/* NOLPHIN_ZOOM_LEVEL_LARGEST */
	};

	piece_count = pieces_by_level[nolphin_icon_container_get_zoom_level (container)];

	attributes = nolphin_icon_view_container_get_icon_text_attributes_from_preferences ();

	*len = MIN (piece_count, quarkv_length (attributes));

	return attributes;
}

/* This callback returns the text, both the editable part, and the
 * part below that is not editable.
 */
static void
nolphin_icon_view_container_get_icon_text (NolphinIconContainer *container,
					    NolphinIconData      *data,
					    char                 **editable_text,
					    char                 **additional_text,
                        gboolean              *pinned,
                        gboolean              *fav_unavailable,
					    gboolean               include_invisible)
{
	GQuark *attributes;
	char *text_array[4];
	int i, j, num_attributes;
	NolphinIconView *icon_view;
	NolphinFile *file;
	gboolean use_additional;

	file = NOLPHIN_FILE (data);

	g_assert (NOLPHIN_IS_FILE (file));
	g_assert (editable_text != NULL);
	icon_view = get_icon_view (container);
	g_return_if_fail (icon_view != NULL);

	use_additional = (additional_text != NULL);

	/* In the smallest zoom mode, no text is drawn. */
	if (nolphin_icon_container_get_zoom_level (container) == NOLPHIN_ZOOM_LEVEL_SMALLEST &&
            !include_invisible) {
		*editable_text = NULL;
        if (pinned) {
            *pinned = FALSE;
        }
	} else {
		/* Strip the suffix for nolphin object xml files. */
		*editable_text = nolphin_file_get_display_name (file);
        if (pinned) {
            *pinned = nolphin_file_get_pinning (file) && !nolphin_file_is_in_favorites (file);
        }
	}

    if (fav_unavailable) {
        *fav_unavailable = nolphin_file_is_unavailable_favorite (file);
    }

	if (!use_additional) {
		return;
	}

	if (nolphin_icon_view_is_compact (icon_view)) {
		*additional_text = NULL;
		return;
	}

	if (NOLPHIN_IS_DESKTOP_ICON_FILE (file) ||
	    nolphin_file_is_nolphin_link (file)) {
		/* Don't show the normal extra information for desktop icons,
		 * or desktop files, it doesn't make sense. */
 		*additional_text = NULL;
		return;
	}

	/* Find out what attributes go below each icon. */
	attributes = nolphin_icon_view_container_get_icon_text_attribute_names (container,
									   &num_attributes);

	/* Get the attributes. */
	j = 0;
	for (i = 0; i < num_attributes; ++i) {
		if (attributes[i] == attribute_none_q) {
			continue;
		}

		text_array[j++] =
			nolphin_file_get_string_attribute_with_default_q (file, attributes[i]);
	}
	text_array[j] = NULL;

	/* Return them. */
	if (j == 0) {
		*additional_text = NULL;
	} else if (j == 1) {
		/* Only one item, avoid the strdup + free */
		*additional_text = text_array[0];
	} else {
		*additional_text = g_strjoinv ("\n", text_array);

		for (i = 0; i < j; i++) {
			g_free (text_array[i]);
		}
	}
}

/* Sort as follows:
 *   0) computer link
 *   1) home link
 *   2) network link
 *   3) mount links
 *   4) trash link
 *   5) other
 */
typedef enum {
	SORT_COMPUTER_LINK,
	SORT_HOME_LINK,
	SORT_NETWORK_LINK,
	SORT_MOUNT_LINK,
	SORT_TRASH_LINK,
	SORT_OTHER
} SortCategory;

static SortCategory
get_sort_category (NolphinFile *file)
{
	NolphinDesktopLink *link;
	SortCategory category;

	category = SORT_OTHER;

	if (NOLPHIN_IS_DESKTOP_ICON_FILE (file)) {
		link = nolphin_desktop_icon_file_get_link (NOLPHIN_DESKTOP_ICON_FILE (file));
		if (link != NULL) {
			switch (nolphin_desktop_link_get_link_type (link)) {
			case NOLPHIN_DESKTOP_LINK_COMPUTER:
				category = SORT_COMPUTER_LINK;
				break;
			case NOLPHIN_DESKTOP_LINK_HOME:
				category = SORT_HOME_LINK;
				break;
			case NOLPHIN_DESKTOP_LINK_MOUNT:
				category = SORT_MOUNT_LINK;
				break;
			case NOLPHIN_DESKTOP_LINK_TRASH:
				category = SORT_TRASH_LINK;
				break;
			case NOLPHIN_DESKTOP_LINK_NETWORK:
				category = SORT_NETWORK_LINK;
				break;
			default:
				category = SORT_OTHER;
				break;
			}
			g_object_unref (link);
		}
	}

	return category;
}

static int
fm_desktop_icon_container_icons_compare (NolphinIconContainer *container,
					 NolphinIconData      *data_a,
					 NolphinIconData      *data_b)
{
	NolphinFile *file_a;
	NolphinFile *file_b;
	NolphinView *directory_view;
	SortCategory category_a, category_b;

	file_a = (NolphinFile *) data_a;
	file_b = (NolphinFile *) data_b;

	directory_view = NOLPHIN_VIEW (NOLPHIN_ICON_VIEW_CONTAINER (container)->view);
	g_return_val_if_fail (directory_view != NULL, 0);

	category_a = get_sort_category (file_a);
	category_b = get_sort_category (file_b);

	if (category_a == category_b) {
		return nolphin_file_compare_for_sort
			(file_a, file_b, NOLPHIN_FILE_SORT_BY_DISPLAY_NAME,
			 nolphin_view_should_sort_directories_first (directory_view),
			 nolphin_view_should_sort_favorites_first (directory_view),
			 FALSE,
             NULL);
	}

	if (category_a < category_b) {
		return -1;
	} else {
		return +1;
	}
}

static int
nolphin_icon_view_container_compare_icons (NolphinIconContainer *container,
					    NolphinIconData      *icon_a,
					    NolphinIconData      *icon_b)
{
	NolphinIconView *icon_view;

	icon_view = get_icon_view (container);
	g_return_val_if_fail (icon_view != NULL, 0);

	if (NOLPHIN_ICON_VIEW_CONTAINER (container)->sort_for_desktop) {
		return fm_desktop_icon_container_icons_compare
			(container, icon_a, icon_b);
	}

	/* Type unsafe comparisons for performance */
	return nolphin_icon_view_compare_files (icon_view,
					   (NolphinFile *)icon_a,
					   (NolphinFile *)icon_b);
}

static void
nolphin_icon_view_container_freeze_updates (NolphinIconContainer *container)
{
	NolphinIconView *icon_view;
	icon_view = get_icon_view (container);
	g_return_if_fail (icon_view != NULL);
	nolphin_view_freeze_updates (NOLPHIN_VIEW (icon_view));
}

static void
nolphin_icon_view_container_unfreeze_updates (NolphinIconContainer *container)
{
	NolphinIconView *icon_view;
	icon_view = get_icon_view (container);
	g_return_if_fail (icon_view != NULL);
	nolphin_view_unfreeze_updates (NOLPHIN_VIEW (icon_view));
}

inline static void
nolphin_icon_view_container_icon_get_bounding_box (NolphinIcon *icon,
               int *x1_return, int *y1_return,
               int *x2_return, int *y2_return,
               NolphinIconCanvasItemBoundsUsage usage)
{
    double x1, y1, x2, y2;

    if (usage == BOUNDS_USAGE_FOR_DISPLAY) {
        eel_canvas_item_get_bounds (EEL_CANVAS_ITEM (icon->item),
                        &x1, &y1, &x2, &y2);
    } else if (usage == BOUNDS_USAGE_FOR_LAYOUT) {
        nolphin_icon_canvas_item_get_bounds_for_layout (icon->item,
                                 &x1, &y1, &x2, &y2);
    } else if (usage == BOUNDS_USAGE_FOR_ENTIRE_ITEM) {
        nolphin_icon_canvas_item_get_bounds_for_entire_item (icon->item,
                                      &x1, &y1, &x2, &y2);
    } else {
        g_assert_not_reached ();
    }

    if (x1_return != NULL) {
        *x1_return = x1;
    }

    if (y1_return != NULL) {
        *y1_return = y1;
    }

    if (x2_return != NULL) {
        *x2_return = x2;
    }

    if (y2_return != NULL) {
        *y2_return = y2;
    }
}

static gboolean
get_stored_icon_position (NolphinIconContainer *container,
                          NolphinIconData      *data,
                          NolphinIconPosition  *position)
{
    GdkPoint point;
    char *scale_string;
    NolphinIconView *icon_view;
    NolphinFile *file;

    g_assert (NOLPHIN_IS_ICON_CONTAINER (container));
    g_assert (NOLPHIN_IS_FILE (data));
    g_assert (position != NULL);

    file = NOLPHIN_FILE (data);

    icon_view = get_icon_view (container);
    g_assert (NOLPHIN_IS_ICON_VIEW (icon_view));

    if (nolphin_icon_view_is_compact (icon_view) || nolphin_file_get_is_desktop_orphan (file)) {
        return FALSE;
    }

    nolphin_file_get_position (file, &point);
    position->x = point.x;
    position->y = point.y;

    /* If it is the desktop directory, maybe the gnome-libs metadata has information about it */

    /* Disable scaling if not on the desktop */
    if (nolphin_icon_container_get_is_desktop (container)) {
        /* Get the scale of the icon from the metadata. */
        scale_string = nolphin_file_get_metadata
            (file, NOLPHIN_METADATA_KEY_ICON_SCALE, "1");
        position->scale = g_ascii_strtod (scale_string, NULL);
        if (errno != 0) {
            position->scale = 1.0;
        }

        g_free (scale_string);
    } else {
        position->scale = 1.0;
    }

    return position->x > ICON_UNPOSITIONED_VALUE;
}

static void
lay_down_one_line (NolphinIconContainer *container,
           GList *line_start,
           GList *line_end,
           double y,
           double max_height,
           GArray *positions,
           gboolean whole_text,
           gint     gap)
{
    GList *p;
    NolphinIcon *icon;
    double x, y_offset;
    NolphinCanvasRects *position;
    int i;
    gboolean is_rtl;

    is_rtl = nolphin_icon_container_is_layout_rtl (container);

    /* Lay out the icons along the baseline. */
    x = gap;
    i = 0;
    for (p = line_start; p != line_end; p = p->next) {
        icon = p->data;

        position = &g_array_index (positions, NolphinCanvasRects, i++);

        if (container->details->label_position == NOLPHIN_ICON_LABEL_POSITION_BESIDE) {
            y_offset = (max_height - position->height) / 2;
        } else {
            y_offset = position->y_offset;
        }

        nolphin_icon_container_icon_set_position
            (container, icon,
             is_rtl ? nolphin_icon_container_get_mirror_x_position (container, icon, x + position->x_offset) : x + position->x_offset,
             y + y_offset);
        nolphin_icon_canvas_item_set_entire_text (icon->item, whole_text);

        icon->saved_ltr_x = is_rtl ? nolphin_icon_container_get_mirror_x_position (container, icon, icon->x) : icon->x;

        x += position->width;
    }
}

static void
lay_down_one_column (NolphinIconContainer *container,
             GList *line_start,
             GList *line_end,
             double x,
             double y_start,
             double y_iter,
             GArray *positions)
{
    GList *p;
    NolphinIcon *icon;
    double y;
    NolphinCanvasRects *position;
    int i;
    gboolean is_rtl;

        is_rtl = nolphin_icon_container_is_layout_rtl (container);

    /* Lay out the icons along the baseline. */
    y = y_start;
    i = 0;
    for (p = line_start; p != line_end; p = p->next) {
        icon = p->data;

        position = &g_array_index (positions, NolphinCanvasRects, i++);

        nolphin_icon_container_icon_set_position
            (container, icon,
             is_rtl ? nolphin_icon_container_get_mirror_x_position (container, icon, x + position->x_offset) : x + position->x_offset,
             y + position->y_offset);

        icon->saved_ltr_x = is_rtl ? nolphin_icon_container_get_mirror_x_position (container, icon, icon->x) : icon->x;

        y += y_iter;
    }
}

#define ICON_TEXT_GAP 4
#define COLUMN_GAP 4
#define ROW_GAP 10

static void
lay_down_icons_horizontal (NolphinIconContainer *container,
               GList *icons,
               double start_y)
{
    GList *p, *line_start;
    NolphinIcon *icon;
    double canvas_width, y;
    GArray *positions;
    NolphinCanvasRects *position;
    EelDRect bounds;
    EelDRect icon_bounds;
    EelDRect text_bounds;
    double line_width;
    double grid_width;
    double max_text_width, max_icon_width;
    int icon_width;
    int i;
    int num_columns;
    double ppu;
    int icon_text_gap, column_gap, row_gap;
    int device_canvas_width;
    GtkAllocation allocation;
    gint icon_size, text_size, use_size;

    g_assert (NOLPHIN_IS_ICON_CONTAINER (container));

    if (icons == NULL) {
        return;
    }

    positions = g_array_new (FALSE, FALSE, sizeof (NolphinCanvasRects));
    gtk_widget_get_allocation (GTK_WIDGET (container), &allocation);

    /* Lay out icons a line at a time. */
    canvas_width = nolphin_icon_container_get_canvas_width (container, allocation);
    max_icon_width = max_text_width = 0.0;

    ppu = EEL_CANVAS (container)->pixels_per_unit;
    icon_text_gap = floor (ICON_TEXT_GAP / ppu);
    column_gap = floor (COLUMN_GAP / ppu);
    row_gap = floor (ROW_GAP / ppu);

    device_canvas_width = floor (canvas_width * ppu);
    icon_size = nolphin_get_icon_size_for_zoom_level (container->details->zoom_level);
    text_size = nolphin_get_icon_text_width_for_zoom_level (container->details->zoom_level);

    use_size = MAX (icon_size, text_size) + 15;
    icon_size /= ppu;
    if (container->details->label_position == NOLPHIN_ICON_LABEL_POSITION_BESIDE) {
        /* Would it be worth caching these bounds for the next loop? */
        for (p = icons; p != NULL; p = p->next) {
            icon = p->data;

            icon_bounds = nolphin_icon_canvas_item_get_icon_rectangle (icon->item);
            max_icon_width = MAX (max_icon_width, ceil (icon_bounds.x1 - icon_bounds.x0));

            text_bounds = nolphin_icon_canvas_item_get_text_rectangle (icon->item, TRUE);
            max_text_width = MAX (max_text_width, ceil (text_bounds.x1 - text_bounds.x0));
        }

        grid_width = max_icon_width + max_text_width + column_gap;
    } else {
        num_columns = device_canvas_width / use_size;
        /* Minimum of one column */
        num_columns = MAX (num_columns, 1);
        /* -1 prevents jitter */
        grid_width = (((device_canvas_width / num_columns) / ppu) - 1.0);
    }

    line_width = container->details->label_position == NOLPHIN_ICON_LABEL_POSITION_BESIDE ? column_gap : 0;
    line_start = icons;
    y = start_y + row_gap;
    i = 0;

    for (p = icons; p != NULL; p = p->next) {
        icon = p->data;

        if (container->details->fixed_text_height == -1) {
            container->details->fixed_text_height = nolphin_icon_canvas_item_get_fixed_text_height_for_layout (icon->item) / ppu;
        }

        /* Assume it's only one level hierarchy to avoid costly affine calculations */
        nolphin_icon_canvas_item_get_bounds_for_layout (icon->item,
                                 &bounds.x0, &bounds.y0,
                                 &bounds.x1, &bounds.y1);

        icon_bounds = nolphin_icon_canvas_item_get_icon_rectangle (icon->item);
        icon_width = grid_width;

        /* If this icon doesn't fit, it's time to lay out the line that's queued up. */
        if (line_start != p && line_width + icon_width >= canvas_width ) {
            if (container->details->label_position == NOLPHIN_ICON_LABEL_POSITION_BESIDE) {
                y += row_gap;
            } else {
                /* Advance to the baseline. */
                y += icon_text_gap + icon_size;
            }

            lay_down_one_line (container, line_start, p, y, icon_size, positions, FALSE, column_gap);

            if (container->details->label_position == NOLPHIN_ICON_LABEL_POSITION_BESIDE) {
                y += row_gap + icon_size;
            } else {
                /* Advance to next line. */
                y += container->details->fixed_text_height + row_gap;
            }

            line_width = container->details->label_position == NOLPHIN_ICON_LABEL_POSITION_BESIDE ? column_gap : 0;
            line_start = p;
            i = 0;
        }

        g_array_set_size (positions, i + 1);
        position = &g_array_index (positions, NolphinCanvasRects, i++);
        position->width = icon_width;
        position->height = icon_bounds.y1 - icon_bounds.y0;

        if (container->details->label_position == NOLPHIN_ICON_LABEL_POSITION_BESIDE) {
            position->x_offset = max_icon_width + (2 * row_gap) - (icon_bounds.x1 - icon_bounds.x0);
            position->y_offset = 0;
        } else {
            position->x_offset = (icon_width - (icon_bounds.x1 - icon_bounds.x0)) / 2;
            position->y_offset = icon_bounds.y0 - icon_bounds.y1;
        }

        /* Add this icon. */
        line_width += icon_width;
    }

    /* Lay down that last line of icons. */
    if (line_start != NULL) {
            if (container->details->label_position == NOLPHIN_ICON_LABEL_POSITION_BESIDE) {
                y += row_gap;
            } else {
                /* Advance to the baseline. */
                y += icon_text_gap + icon_size;
            }

        lay_down_one_line (container, line_start, NULL, y, icon_size, positions, TRUE, column_gap);
    }

    g_array_free (positions, TRUE);
}

/* column-wise layout. At the moment, this only works with label-beside-icon (used by "Compact View"). */
static void
lay_down_icons_vertical (NolphinIconContainer *container,
             GList *icons,
             double start_y)
{
    GList *p, *line_start;
    NolphinIcon *icon;
    double x, canvas_height;
    GArray *positions;
    NolphinCanvasRects *position;
    EelDRect icon_bounds;
    EelDRect text_bounds;
    GtkAllocation allocation;

    double ppu;
    int gap;
    double line_height;

    double max_height;
    double max_height_with_borders;
    double max_width;
    double max_width_in_column;

    double max_bounds_height;
    double max_bounds_height_with_borders;

    double max_text_width, max_icon_width;
    double max_text_height, max_icon_height;
    int height;
    int i;

    g_assert (NOLPHIN_IS_ICON_CONTAINER (container));
    g_assert (container->details->label_position == NOLPHIN_ICON_LABEL_POSITION_BESIDE);

    if (icons == NULL) {
        return;
    }

    ppu = EEL_CANVAS (container)->pixels_per_unit;
    gap = floor (ICON_TEXT_GAP / ppu);

    positions = g_array_new (FALSE, FALSE, sizeof (NolphinCanvasRects));
    gtk_widget_get_allocation (GTK_WIDGET (container), &allocation);

    /* Lay out icons a column at a time. */
    canvas_height = nolphin_icon_container_get_canvas_height (container, allocation);

    max_icon_width = max_text_width = 0.0;
    max_icon_height = max_text_height = 0.0;
    max_bounds_height = 0.0;

    get_max_icon_dimensions (icons, NULL,
                 &max_icon_width, &max_icon_height,
                 &max_text_width, &max_text_height,
                 &max_bounds_height);

    max_width = max_icon_width + max_text_width;
    max_height = MAX (max_icon_height, max_text_height);
    max_height_with_borders = gap + max_height;

    max_bounds_height_with_borders = gap + max_bounds_height;

    line_height = gap;
    line_start = icons;
    x = 0;
    i = 0;

    max_width_in_column = 0.0;

    for (p = icons; p != NULL; p = p->next) {
        icon = p->data;

        /* If this icon doesn't fit, it's time to lay out the column that's queued up. */

        /* We use the bounds height here, since for wrapping we also want to consider
         * overlapping emblems at the bottom. We may wrap a little bit too early since
         * the icon with the max. bounds height may actually not be in the last row, but
         * it is better than visual glitches
         */
        if (line_start != p && line_height + (max_bounds_height_with_borders-1) >= canvas_height ) {
            x += gap;

            /* correctly set (per-column) width */
            if (!container->details->all_columns_same_width) {
                for (i = 0; i < (int) positions->len; i++) {
                    position = &g_array_index (positions, NolphinCanvasRects, i);
                    position->width = max_width_in_column;
                }
            }

            lay_down_one_column (container, line_start, p, x, gap, max_height_with_borders, positions);

            /* Advance to next column. */
            if (container->details->all_columns_same_width) {
                x += max_width + gap;
            } else {
                x += max_width_in_column + gap;
            }

            line_height = gap;
            line_start = p;
            i = 0;

            max_width_in_column = 0;
        }

        icon_bounds = nolphin_icon_canvas_item_get_icon_rectangle (icon->item);
        text_bounds = nolphin_icon_canvas_item_get_text_rectangle (icon->item, TRUE);

        max_width_in_column = MAX (max_width_in_column,
                       ceil (icon_bounds.x1 - icon_bounds.x0) +
                       ceil (text_bounds.x1 - text_bounds.x0));

        g_array_set_size (positions, i + 1);
        position = &g_array_index (positions, NolphinCanvasRects, i++);
        if (container->details->all_columns_same_width) {
            position->width = max_width;
        }
        position->height = max_height;
        position->y_offset = gap;
        position->x_offset = gap;

        position->x_offset += max_icon_width - ceil (icon_bounds.x1 - icon_bounds.x0);

        height = MAX (ceil (icon_bounds.y1 - icon_bounds.y0), ceil(text_bounds.y1 - text_bounds.y0));
        position->y_offset += (max_height - height) / 2;

        /* Add this icon. */
        line_height += max_height_with_borders;
    }

    /* Lay down that last column of icons. */
    if (line_start != NULL) {
        x += gap;
        lay_down_one_column (container, line_start, NULL, x, gap, max_height_with_borders, positions);
    }

    g_array_free (positions, TRUE);
}

static void
lay_down_icons_vertical_desktop (NolphinIconContainer *container, GList *icons)
{
    GList *p, *placed_icons, *unplaced_icons;
    int total, new_length, placed;
    NolphinIcon *icon;
    int height, max_width, column_width, icon_width, icon_height;
    int x, y, x1, x2, y1, y2;
    EelDRect icon_rect;
    GtkAllocation allocation;

    /* Get container dimensions */
    gtk_widget_get_allocation (GTK_WIDGET (container), &allocation);
    height = nolphin_icon_container_get_canvas_height (container, allocation);

    /* Determine which icons have and have not been placed */
    placed_icons = NULL;
    unplaced_icons = NULL;

    total = g_list_length (container->details->icons);
    new_length = g_list_length (icons);
    placed = total - new_length;
    if (placed > 0) {
        NolphinPlacementGrid *grid;
        /* Add only placed icons in list */
        for (p = container->details->icons; p != NULL; p = p->next) {
            icon = p->data;
            if (nolphin_icon_container_icon_is_positioned (icon)) {
                nolphin_icon_container_icon_set_position(container, icon, icon->saved_ltr_x, icon->y);
                placed_icons = g_list_prepend (placed_icons, icon);
            } else {
                icon->x = 0;
                icon->y = 0;
                unplaced_icons = g_list_prepend (unplaced_icons, icon);
            }
        }
        placed_icons = g_list_reverse (placed_icons);
        unplaced_icons = g_list_reverse (unplaced_icons);

        grid = nolphin_placement_grid_new (container, FALSE);

        if (grid) {
            for (p = placed_icons; p != NULL; p = p->next) {
                nolphin_placement_grid_mark_icon
                    (grid, (NolphinIcon*)p->data);
            }

            /* Place unplaced icons in the best locations */
            for (p = unplaced_icons; p != NULL; p = p->next) {
                icon = p->data;

                icon_rect = nolphin_icon_canvas_item_get_icon_rectangle (icon->item);

                /* Start the icon in the first column */
                x = GET_VIEW_CONSTANT (container, desktop_pad_horizontal) + (GET_VIEW_CONSTANT (container, snap_size_x) / 2) - ((icon_rect.x1 - icon_rect.x0) / 2);
                y = GET_VIEW_CONSTANT (container, desktop_pad_vertical) + GET_VIEW_CONSTANT (container, snap_size_y) - (icon_rect.y1 - icon_rect.y0);

                find_empty_location (container,
                             grid,
                             icon,
                             x, y,
                             &x, &y);

                nolphin_icon_container_icon_set_position (container, icon, x, y);
                icon->saved_ltr_x = x;
                nolphin_placement_grid_mark_icon (grid, icon);
            }

            nolphin_placement_grid_free (grid);
        }

        g_list_free (placed_icons);
        g_list_free (unplaced_icons);
    } else {
        /* There are no placed icons.  Just lay them down using our rules */
        x = GET_VIEW_CONSTANT (container, desktop_pad_horizontal);

        while (icons != NULL) {
            int center_x;
            int baseline;
            int icon_height_for_bound_check;
            gboolean should_snap;

            should_snap = container->details->keep_aligned;


            y = GET_VIEW_CONSTANT (container, desktop_pad_vertical);

            max_width = 0;

            /* Calculate max width for column */
            for (p = icons; p != NULL; p = p->next) {
                icon = p->data;

                nolphin_icon_container_icon_get_bounding_box (container, icon, &x1, &y1, &x2, &y2,
                               BOUNDS_USAGE_FOR_LAYOUT);
                icon_width = x2 - x1;
                icon_height = y2 - y1;

                nolphin_icon_container_icon_get_bounding_box (container, icon, NULL, &y1, NULL, &y2,
                               BOUNDS_USAGE_FOR_ENTIRE_ITEM);
                icon_height_for_bound_check = y2 - y1;

                if (should_snap) {
                    /* Snap the baseline to a grid position */
                    icon_rect = nolphin_icon_canvas_item_get_icon_rectangle (icon->item);
                    baseline = y + (icon_rect.y1 - icon_rect.y0);
                    baseline = SNAP_CEIL_VERTICAL (baseline);
                    y = baseline - (icon_rect.y1 - icon_rect.y0);
                }

                /* Check and see if we need to move to a new column */
                if (y != GET_VIEW_CONSTANT (container, desktop_pad_vertical) && y + icon_height_for_bound_check > height) {
                    break;
                }

                if (max_width < icon_width) {
                    max_width = icon_width;
                }

                y += icon_height + GET_VIEW_CONSTANT (container, desktop_pad_vertical);
            }

            y = GET_VIEW_CONSTANT (container, desktop_pad_vertical);

            center_x = x + max_width / 2;
            column_width = max_width;
            if (should_snap) {
                /* Find the grid column to center on */
                center_x = SNAP_CEIL_HORIZONTAL (center_x);
                column_width = (center_x - x) + (max_width / 2);
            }

            /* Lay out column */
            for (p = icons; p != NULL; p = p->next) {
                icon = p->data;
                nolphin_icon_container_icon_get_bounding_box (container, icon, &x1, &y1, &x2, &y2,
                               BOUNDS_USAGE_FOR_LAYOUT);
                icon_height = y2 - y1;

                nolphin_icon_container_icon_get_bounding_box (container, icon, NULL, &y1, NULL, &y2,
                               BOUNDS_USAGE_FOR_ENTIRE_ITEM);
                icon_height_for_bound_check = y2 - y1;

                icon_rect = nolphin_icon_canvas_item_get_icon_rectangle (icon->item);

                if (should_snap) {
                    baseline = y + (icon_rect.y1 - icon_rect.y0);
                    baseline = SNAP_CEIL_VERTICAL (baseline);
                    y = baseline - (icon_rect.y1 - icon_rect.y0);
                }

                /* Check and see if we need to move to a new column */
                if (y != GET_VIEW_CONSTANT (container, desktop_pad_vertical) && y > height - icon_height_for_bound_check &&
                    /* Make sure we lay out at least one icon per column, to make progress */
                    p != icons) {
                    x += column_width + GET_VIEW_CONSTANT (container, desktop_pad_horizontal);
                    break;
                }

                nolphin_icon_container_icon_set_position (container, icon,
                           center_x - (icon_rect.x1 - icon_rect.x0) / 2,
                           y);

                icon->saved_ltr_x = icon->x;
                y += icon_height + GET_VIEW_CONSTANT (container, desktop_pad_vertical);
            }
            icons = p;
        }
    }

    /* These modes are special. We freeze all of our positions
     * after we do the layout.
     */
    /* FIXME bugzilla.gnome.org 42478:
     * This should not be tied to the direction of layout.
     * It should be a separate switch.
     */
    nolphin_icon_container_freeze_icon_positions (container);
}

static void
nolphin_icon_view_container_lay_down_icons (NolphinIconContainer *container, GList *icons, double start_y)
{
    switch (container->details->layout_mode)
    {
    case NOLPHIN_ICON_LAYOUT_L_R_T_B:
    case NOLPHIN_ICON_LAYOUT_R_L_T_B:
        lay_down_icons_horizontal (container, icons, start_y);
        break;

    case NOLPHIN_ICON_LAYOUT_T_B_L_R:
    case NOLPHIN_ICON_LAYOUT_T_B_R_L:
        if (nolphin_icon_container_get_is_desktop (container)) {
            lay_down_icons_vertical_desktop (container, icons);
        } else {
            lay_down_icons_vertical (container, icons, start_y);
        }
        break;

    default:
        g_assert_not_reached ();
    }
}


static void
get_max_icon_dimensions (GList *icon_start,
             GList *icon_end,
             double *max_icon_width,
             double *max_icon_height,
             double *max_text_width,
             double *max_text_height,
             double *max_bounds_height)
{
    NolphinIcon *icon;
    EelDRect icon_bounds;
    EelDRect text_bounds;
    GList *p;
    double y1, y2;

    *max_icon_width = *max_text_width = 0.0;
    *max_icon_height = *max_text_height = 0.0;
    *max_bounds_height = 0.0;

    /* Would it be worth caching these bounds for the next loop? */
    for (p = icon_start; p != icon_end; p = p->next) {
        icon = p->data;

        icon_bounds = nolphin_icon_canvas_item_get_icon_rectangle (icon->item);
        *max_icon_width = MAX (*max_icon_width, ceil (icon_bounds.x1 - icon_bounds.x0));
        *max_icon_height = MAX (*max_icon_height, ceil (icon_bounds.y1 - icon_bounds.y0));

        text_bounds = nolphin_icon_canvas_item_get_text_rectangle (icon->item, TRUE);
        *max_text_width = MAX (*max_text_width, ceil (text_bounds.x1 - text_bounds.x0));
        *max_text_height = MAX (*max_text_height, ceil (text_bounds.y1 - text_bounds.y0));

        nolphin_icon_canvas_item_get_bounds_for_layout (icon->item,
                                 NULL, &y1,
                                 NULL, &y2);
        *max_bounds_height = MAX (*max_bounds_height, y2 - y1);
    }
}

static void
snap_position (NolphinIconContainer *container,
           NolphinIcon *icon,
           int *x, int *y)
{
    int center_x;
    int baseline_y;
    int icon_width;
    int icon_height;
    int total_width;
    int total_height;
    EelDRect icon_position;
    GtkAllocation allocation;

    icon_position = nolphin_icon_canvas_item_get_icon_rectangle (icon->item);
    icon_width = icon_position.x1 - icon_position.x0;
    icon_height = icon_position.y1 - icon_position.y0;

    gtk_widget_get_allocation (GTK_WIDGET (container), &allocation);
    total_width = nolphin_icon_container_get_canvas_width (container, allocation);
    total_height = nolphin_icon_container_get_canvas_height (container, allocation);

    if (nolphin_icon_container_is_layout_rtl (container))
        *x = nolphin_icon_container_get_mirror_x_position (container, icon, *x);

    if (*x + icon_width / 2 < GET_VIEW_CONSTANT (container, desktop_pad_horizontal) + GET_VIEW_CONSTANT (container, snap_size_x)) {
        *x = GET_VIEW_CONSTANT (container, desktop_pad_horizontal) + GET_VIEW_CONSTANT (container, snap_size_x) - icon_width / 2;
    }

    if (*x + icon_width / 2 > total_width - (GET_VIEW_CONSTANT (container, desktop_pad_horizontal) + GET_VIEW_CONSTANT (container, snap_size_x))) {
        *x = total_width - (GET_VIEW_CONSTANT (container, desktop_pad_horizontal) + GET_VIEW_CONSTANT (container, snap_size_x) + (icon_width / 2));
    }

    if (*y + icon_height < GET_VIEW_CONSTANT (container, desktop_pad_vertical) + GET_VIEW_CONSTANT (container, snap_size_y)) {
        *y = GET_VIEW_CONSTANT (container, desktop_pad_vertical) + GET_VIEW_CONSTANT (container, snap_size_y) - icon_height;
    }

    if (*y + icon_height > total_height - (GET_VIEW_CONSTANT (container, desktop_pad_vertical) + GET_VIEW_CONSTANT (container, snap_size_y))) {
        *y = total_height - (GET_VIEW_CONSTANT (container, desktop_pad_vertical) + GET_VIEW_CONSTANT (container, snap_size_y) + (icon_height / 2));
    }

    center_x = *x + icon_width / 2;
    *x = SNAP_NEAREST_HORIZONTAL (center_x) - (icon_width / 2);
    if (nolphin_icon_container_is_layout_rtl (container)) {
        *x = nolphin_icon_container_get_mirror_x_position (container, icon, *x);
    }


    /* Find the grid position vertically and place on the proper baseline */
    baseline_y = *y + icon_height;
    baseline_y = SNAP_NEAREST_VERTICAL (baseline_y);
    *y = baseline_y - icon_height;
}

static int
compare_icons_by_position (gconstpointer a, gconstpointer b)
{
    NolphinIcon *icon_a, *icon_b;
    int x1, y1, x2, y2;
    int center_a;
    int center_b;

    icon_a = (NolphinIcon*)a;
    icon_b = (NolphinIcon*)b;

    nolphin_icon_view_container_icon_get_bounding_box (icon_a, &x1, &y1, &x2, &y2,
                   BOUNDS_USAGE_FOR_DISPLAY);
    center_a = x1 + (x2 - x1) / 2;
    nolphin_icon_view_container_icon_get_bounding_box (icon_b, &x1, &y1, &x2, &y2,
                   BOUNDS_USAGE_FOR_DISPLAY);
    center_b = x1 + (x2 - x1) / 2;

    return center_a == center_b ?
        icon_a->y - icon_b->y :
        center_a - center_b;
}

/* x, y are the top-left coordinates of the icon. */
static void
nolphin_icon_view_container_icon_set_position (NolphinIconContainer *container,
                                            NolphinIcon          *icon,
                                            double             x,
                                            double             y)
{
    double pixels_per_unit;
    int container_left, container_top, container_right, container_bottom;
    int x1, x2, y1, y2;
    int container_x, container_y, container_width, container_height;
    EelDRect icon_bounds;
    int item_width, item_height;
    int height_above, width_left;
    int min_x, max_x, min_y, max_y;

    if (icon->x == x && icon->y == y) {
        return;
    }

    if (icon == nolphin_icon_container_get_icon_being_renamed (container)) {
        nolphin_icon_container_end_renaming_mode (container, TRUE);
    }

    if (nolphin_icon_container_get_is_fixed_size (container)) {
        GtkAllocation alloc;

        gtk_widget_get_allocation (GTK_WIDGET (container), &alloc);
        container_x = alloc.x;
        container_y = alloc.y;
        container_width = alloc.width - container->details->left_margin - container->details->right_margin;
        container_height = alloc.height - container->details->top_margin - container->details->bottom_margin;
        pixels_per_unit = EEL_CANVAS (container)->pixels_per_unit;
        /* Clip the position of the icon within our desktop bounds */
        container_left = container_x / pixels_per_unit;
        container_top =  container_y / pixels_per_unit;
        container_right = container_left + container_width / pixels_per_unit;
        container_bottom = container_top + container_height / pixels_per_unit;

        nolphin_icon_container_icon_get_bounding_box (container, icon, &x1, &y1, &x2, &y2,
                       BOUNDS_USAGE_FOR_ENTIRE_ITEM);
        item_width = x2 - x1;
        item_height = y2 - y1;

        icon_bounds = nolphin_icon_canvas_item_get_icon_rectangle (icon->item);

        /* determine icon rectangle relative to item rectangle */
        height_above = icon_bounds.y0 - y1;
        width_left = icon_bounds.x0 - x1;

        min_x = container_left + GET_VIEW_CONSTANT (container, desktop_pad_horizontal) + width_left;
        max_x = container_right - GET_VIEW_CONSTANT (container, desktop_pad_horizontal) - item_width + width_left;
        x = CLAMP (x, min_x, max_x);

        min_y = container_top + height_above + GET_VIEW_CONSTANT (container, desktop_pad_vertical);
        max_y = container_bottom - GET_VIEW_CONSTANT (container, desktop_pad_vertical) - item_height + height_above;
        y = CLAMP (y, min_y, max_y);
    }

    if (icon->x == ICON_UNPOSITIONED_VALUE) {
        icon->x = 0;
    }
    if (icon->y == ICON_UNPOSITIONED_VALUE) {
        icon->y = 0;
    }

    eel_canvas_item_move (EEL_CANVAS_ITEM (icon->item),
                x - icon->x,
                y - icon->y);

    icon->x = x;
    icon->y = y;
}

static void
nolphin_icon_view_container_move_icon (NolphinIconContainer *container,
                   NolphinIcon *icon,
                   int x, int y,
                   double scale,
                   gboolean raise,
                   gboolean snap,
                   gboolean update_position)
{
    NolphinIconContainerDetails *details;
    gboolean emit_signal;
    NolphinIconPosition position;

    details = container->details;

    emit_signal = FALSE;

    if (icon == nolphin_icon_container_get_icon_being_renamed (container)) {
        nolphin_icon_container_end_renaming_mode (container, TRUE);
    }

    if (scale != icon->scale) {
        icon->scale = scale;
        nolphin_icon_container_update_icon (container, icon);
        if (update_position) {
            nolphin_icon_container_redo_layout (container);
            emit_signal = TRUE;
        }
    }

    if (!details->auto_layout) {
        if (details->keep_aligned && snap) {
            snap_position (container, icon, &x, &y);
        }

        if (x != icon->x || y != icon->y) {
            nolphin_icon_container_icon_set_position (container, icon, x, y);
            emit_signal = update_position;
        }

        icon->saved_ltr_x = nolphin_icon_container_is_layout_rtl (container) ? nolphin_icon_container_get_mirror_x_position (container, icon, icon->x) : icon->x;
    }

    if (emit_signal) {
        position.x = icon->saved_ltr_x;
        position.y = icon->y;
        position.scale = scale;
        position.monitor =  nolphin_desktop_utils_get_monitor_for_widget (GTK_WIDGET (container));
        g_signal_emit_by_name (container, "icon_position_changed", icon->data, &position);
    }

    if (raise) {
        nolphin_icon_container_icon_raise (container, icon);
    }

    /* FIXME bugzilla.gnome.org 42474:
     * Handling of the scroll region is inconsistent here. In
     * the scale-changing case, redo_layout is called, which updates the
     * scroll region appropriately. In other cases, it's up to the
     * caller to make sure the scroll region is updated. This could
     * lead to hard-to-track-down bugs.
     */
}

static void
icon_get_size (NolphinIconContainer *container,
           NolphinIcon *icon,
           guint *size)
{
    if (size != NULL) {
        *size = MAX (nolphin_get_icon_size_for_zoom_level (container->details->zoom_level)
                   * icon->scale, NOLPHIN_ICON_SIZE_SMALLEST);
    }
}

static void
nolphin_icon_view_container_update_icon (NolphinIconContainer *container,
                                      NolphinIcon          *icon,
                                      gboolean           visible)
{
    NolphinIconContainerDetails *details;
    guint icon_size;
    guint min_image_size, max_image_size;
    NolphinIconInfo *icon_info;
    GdkPixbuf *pixbuf;
    char *editable_text, *additional_text;
    gboolean has_open_window;
    gboolean pinned;
    gboolean fav_unavailable;

    if (icon == NULL) {
        return;
    }

    details = container->details;

    /* compute the maximum size based on the scale factor */
    min_image_size = MINIMUM_IMAGE_SIZE * EEL_CANVAS (container)->pixels_per_unit;
    max_image_size = MAX (MAXIMUM_IMAGE_SIZE * EEL_CANVAS (container)->pixels_per_unit, NOLPHIN_ICON_MAXIMUM_SIZE);

    /* Get the appropriate images for the file. */
    if (container->details->forced_icon_size > 0) {
        icon_size = container->details->forced_icon_size;
    } else {
        icon_get_size (container, icon, &icon_size);
    }

    icon_size = MAX (icon_size, min_image_size);
    icon_size = MIN (icon_size, max_image_size);

    DEBUG ("Icon size, getting for size %d", icon_size);

    /* Get the icons. */
    icon_info = nolphin_icon_container_get_icon_images (container,
                                                     icon->data,
                                                     icon_size,
                                                     icon == details->drop_target,
                                                     &has_open_window,
                                                     visible);

    if (container->details->forced_icon_size > 0) {
        gint scale_factor;

        scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (container));
        pixbuf = nolphin_icon_info_get_pixbuf_at_size (icon_info, icon_size * scale_factor);
    } else {
        pixbuf = nolphin_icon_info_get_pixbuf (icon_info);
    }

    nolphin_icon_info_unref (icon_info);

    nolphin_icon_container_get_icon_text (container,
                           icon->data,
                           &editable_text,
                           &additional_text,
                           &pinned,
                           &fav_unavailable,
                           FALSE);


    /* If name of icon being renamed was changed from elsewhere, end renaming mode.
     * Alternatively, we could replace the characters in the editable text widget
     * with the new name, but that could cause timing problems if the user just
     * happened to be typing at that moment.
     */
    if (icon == nolphin_icon_container_get_icon_being_renamed (container) &&
        g_strcmp0 (editable_text,
               nolphin_icon_canvas_item_get_editable_text (icon->item)) != 0) {
        nolphin_icon_container_end_renaming_mode (container, FALSE);
    }

    eel_canvas_item_set (EEL_CANVAS_ITEM (icon->item),
                 "editable_text", editable_text,
                 "additional_text", additional_text,
                 "highlighted_for_drop", icon == details->drop_target,
                 "pinned", pinned,
                 "fav-unavailable", fav_unavailable,
                 NULL);

    nolphin_icon_canvas_item_set_image (icon->item, pixbuf);

    /* Let the pixbufs go. */
    g_object_unref (pixbuf);

    g_free (editable_text);
    g_free (additional_text);
}

static void
find_empty_location (NolphinIconContainer *container,
             NolphinPlacementGrid *grid,
             NolphinIcon *icon,
             int start_x,
             int start_y,
             int *x,
             int *y)
{
    double icon_width, icon_height;
    int canvas_width;
    int canvas_height;
    int height_for_bound_check;
    EelIRect icon_position;
    EelDRect pixbuf_rect;
    gboolean collision;
    GtkAllocation allocation;

    /* Get container dimensions */
    gtk_widget_get_allocation (GTK_WIDGET (container), &allocation);
    canvas_width  = nolphin_icon_container_get_canvas_width (container, allocation);
    canvas_height = nolphin_icon_container_get_canvas_height (container, allocation);

    nolphin_icon_container_icon_get_bounding_box (container, icon,
                   &icon_position.x0, &icon_position.y0,
                   &icon_position.x1, &icon_position.y1,
                   BOUNDS_USAGE_FOR_LAYOUT);
    icon_width = icon_position.x1 - icon_position.x0;
    icon_height = icon_position.y1 - icon_position.y0;

    nolphin_icon_container_icon_get_bounding_box (container, icon,
                   NULL, &icon_position.y0,
                   NULL, &icon_position.y1,
                   BOUNDS_USAGE_FOR_ENTIRE_ITEM);
    height_for_bound_check = icon_position.y1 - icon_position.y0;

    pixbuf_rect = nolphin_icon_canvas_item_get_icon_rectangle (icon->item);

    /* Start the icon on a grid location */
    snap_position (container, icon, &start_x, &start_y);

    icon_position.x0 = start_x;
    icon_position.y0 = start_y;
    icon_position.x1 = icon_position.x0 + icon_width;
    icon_position.y1 = icon_position.y0 + icon_height;

    do {
        EelIRect grid_position;
        gboolean need_new_column;

        collision = FALSE;

        nolphin_placement_grid_canvas_position_to_grid_position (grid,
                          icon_position,
                          &grid_position);

        need_new_column = icon_position.y0 + height_for_bound_check + GET_VIEW_CONSTANT (container, desktop_pad_vertical) > canvas_height;

        if (need_new_column ||
            !nolphin_placement_grid_position_is_free (grid, grid_position)) {
            icon_position.y0 += GET_VIEW_CONSTANT (container, snap_size_y);
            icon_position.y1 = icon_position.y0 + icon_height;

            if (need_new_column) {
                /* Move to the next column */
                icon_position.y0 = GET_VIEW_CONSTANT (container, desktop_pad_vertical) + GET_VIEW_CONSTANT (container, snap_size_y) - (pixbuf_rect.y1 - pixbuf_rect.y0);
                while (icon_position.y0 < GET_VIEW_CONSTANT (container, desktop_pad_vertical)) {
                    icon_position.y0 += GET_VIEW_CONSTANT (container, snap_size_y);
                }
                icon_position.y1 = icon_position.y0 + icon_height;

                icon_position.x0 += GET_VIEW_CONSTANT (container, snap_size_x);
                icon_position.x1 = icon_position.x0 + icon_width;
            }

            collision = TRUE;
        }
    } while (collision && (icon_position.x1 < canvas_width));

    *x = icon_position.x0;
    *y = icon_position.y0;
}

static void
nolphin_icon_view_container_align_icons (NolphinIconContainer *container)
{
    GList *unplaced_icons;
    GList *l;
    NolphinPlacementGrid *grid;

    unplaced_icons = g_list_copy (container->details->icons);

    unplaced_icons = g_list_sort (unplaced_icons,
                      compare_icons_by_position);

    if (nolphin_icon_container_is_layout_rtl (container)) {
        unplaced_icons = g_list_reverse (unplaced_icons);
    }

    grid = nolphin_placement_grid_new (container, TRUE);

    if (!grid) {
        g_list_free (unplaced_icons);
        return;
    }

    for (l = unplaced_icons; l != NULL; l = l->next) {
        NolphinIcon *icon;
        int x, y;

        icon = l->data;
        x = icon->saved_ltr_x;
        y = icon->y;
        find_empty_location (container, grid,
                     icon, x, y, &x, &y);

        nolphin_icon_container_icon_set_position (container, icon, x, y);
        icon->saved_ltr_x = icon->x;
        nolphin_placement_grid_mark_icon (grid, icon);
    }

    g_list_free (unplaced_icons);

    nolphin_placement_grid_free (grid);

    if (nolphin_icon_container_is_layout_rtl (container)) {
        nolphin_icon_container_set_rtl_positions (container);
    }
}

static void
nolphin_icon_view_container_reload_icon_positions (NolphinIconContainer *container)
{
    GList *p, *no_position_icons;
    NolphinIcon *icon;
    gboolean have_stored_position;
    NolphinIconPosition position;
    EelDRect bounds;
    double bottom;
    EelCanvasItem *item;

    g_assert (!container->details->auto_layout);

    nolphin_icon_container_resort (container);

    no_position_icons = NULL;

    /* Place all the icons with positions. */
    bottom = 0;
    for (p = container->details->icons; p != NULL; p = p->next) {
        icon = p->data;

        have_stored_position = get_stored_icon_position (container,
                                                         icon->data,
                                                         &position);

        if (have_stored_position) {
            nolphin_icon_container_icon_set_position (container, icon, position.x, position.y);
            item = EEL_CANVAS_ITEM (icon->item);
            nolphin_icon_canvas_item_get_bounds_for_layout (icon->item,
                                     &bounds.x0,
                                     &bounds.y0,
                                     &bounds.x1,
                                     &bounds.y1);
            eel_canvas_item_i2w (item->parent,
                         &bounds.x0,
                         &bounds.y0);
            eel_canvas_item_i2w (item->parent,
                         &bounds.x1,
                         &bounds.y1);
            if (bounds.y1 > bottom) {
                bottom = bounds.y1;
            }
        } else {
            no_position_icons = g_list_prepend (no_position_icons, icon);
        }
    }
    no_position_icons = g_list_reverse (no_position_icons);

    /* Place all the other icons. */
    NOLPHIN_ICON_CONTAINER_GET_CLASS (container)->lay_down_icons (container, no_position_icons, bottom + GET_VIEW_CONSTANT (container, icon_pad_bottom));
    g_list_free (no_position_icons);
}

static gboolean
assign_icon_position (NolphinIconContainer *container,
              NolphinIcon *icon)
{
    gboolean have_stored_position;
    NolphinIconPosition position;

    /* Get the stored position. */
    have_stored_position = FALSE;
    position.scale = 1.0;

    have_stored_position = get_stored_icon_position (container,
                                                     icon->data,
                                                     &position);

    icon->scale = position.scale;
    if (!container->details->auto_layout) {
        if (have_stored_position) {
            nolphin_icon_container_icon_set_position (container, icon, position.x, position.y);
            icon->saved_ltr_x = icon->x;
        } else {
            return FALSE;
        }
    }
    return TRUE;
}

static void
nolphin_icon_view_container_finish_adding_new_icons (NolphinIconContainer *container)
{
    GList *p, *new_icons, *no_position_icons, *semi_position_icons;
    NolphinIcon *icon;
    double bottom;
    gint current_monitor;

    new_icons = container->details->new_icons;
    container->details->new_icons = NULL;

    current_monitor = nolphin_desktop_utils_get_monitor_for_widget (GTK_WIDGET(container));

    /* Position most icons (not unpositioned manual-layout icons). */
    new_icons = g_list_reverse (new_icons);
    no_position_icons = semi_position_icons = NULL;
    for (p = new_icons; p != NULL; p = p->next) {
        icon = p->data;
        nolphin_icon_container_update_icon (container, icon);
        if (icon->has_lazy_position || nolphin_icon_container_icon_is_new_for_monitor (container, icon, current_monitor)) {
            assign_icon_position (container, icon);
            semi_position_icons = g_list_prepend (semi_position_icons, icon);
        } else if (!assign_icon_position (container, icon)) {
            no_position_icons = g_list_prepend (no_position_icons, icon);
        }

        nolphin_icon_container_finish_adding_icon (container, icon);
    }
    g_list_free (new_icons);

    if (semi_position_icons != NULL) {
        NolphinPlacementGrid *grid;
        time_t now;
        gboolean dummy;

        g_assert (!container->details->auto_layout);

        semi_position_icons = g_list_reverse (semi_position_icons);

        /* This is currently only used on the desktop.
         * Thus, we pass FALSE for tight, like lay_down_icons_tblr */
        grid = nolphin_placement_grid_new (container, FALSE);

        for (p = container->details->icons; p != NULL; p = p->next) {
            icon = p->data;

            if (nolphin_icon_container_icon_is_positioned (icon) && !icon->has_lazy_position) {
                nolphin_placement_grid_mark_icon (grid, icon);
            }
        }

        now = time (NULL);

        for (p = semi_position_icons; p != NULL; p = p->next) {
            NolphinIconPosition position;
            int x, y;

            icon = p->data;
            x = icon->x;
            y = icon->y;

            find_empty_location (container, grid,
                         icon, x, y, &x, &y);

            nolphin_icon_container_icon_set_position (container, icon, x, y);

            position.x = icon->x;
            position.y = icon->y;
            position.scale = icon->scale;
            position.monitor = current_monitor;
            nolphin_placement_grid_mark_icon (grid, icon);
            g_signal_emit_by_name (container, "icon_position_changed",
                                   icon->data, &position);
            g_signal_emit_by_name (container, "store_layout_timestamp",
                                   icon->data, &now, &dummy);

            /* ensure that next time we run this code, the formerly semi-positioned
             * icons are treated as being positioned. */
            icon->has_lazy_position = FALSE;
        }

        nolphin_placement_grid_free (grid);

        g_list_free (semi_position_icons);
    }

    /* Position the unpositioned manual layout icons. */
    if (no_position_icons != NULL) {
        g_assert (!container->details->auto_layout);

        nolphin_icon_container_sort_icons (container, &no_position_icons);
        if (nolphin_icon_container_get_is_desktop (container)) {
            NOLPHIN_ICON_CONTAINER_GET_CLASS (container)->lay_down_icons (container, no_position_icons, GET_VIEW_CONSTANT (container, container_pad_top));
        } else {
            nolphin_icon_container_get_all_icon_bounds (container, NULL, NULL, NULL, &bottom, BOUNDS_USAGE_FOR_LAYOUT);
            NOLPHIN_ICON_CONTAINER_GET_CLASS (container)->lay_down_icons (container, no_position_icons, bottom + GET_VIEW_CONSTANT (container, icon_pad_bottom));
        }
        g_list_free (no_position_icons);
    }

    if (container->details->store_layout_timestamps_when_finishing_new_icons) {
        nolphin_icon_container_store_layout_timestamps_now (container);
        container->details->store_layout_timestamps_when_finishing_new_icons = FALSE;
    }
}

static void
nolphin_icon_view_container_set_zoom_level (NolphinIconContainer *container, gint new_level)
{
    NolphinIconContainerDetails *details;
    int pinned_level;
    double pixels_per_unit;

    details = container->details;

    nolphin_icon_container_end_renaming_mode (container, TRUE);

    pinned_level = new_level;
    if (pinned_level < NOLPHIN_ZOOM_LEVEL_SMALLEST) {
        pinned_level = NOLPHIN_ZOOM_LEVEL_SMALLEST;
    } else if (pinned_level > NOLPHIN_ZOOM_LEVEL_LARGEST) {
        pinned_level = NOLPHIN_ZOOM_LEVEL_LARGEST;
    }

    if (pinned_level == details->zoom_level) {
        return;
    }

    details->zoom_level = pinned_level;

    pixels_per_unit = (double) nolphin_get_icon_size_for_zoom_level (pinned_level) / NOLPHIN_ICON_SIZE_STANDARD;
    eel_canvas_set_pixels_per_unit (EEL_CANVAS (container), pixels_per_unit);
}

static int text_ellipsis_limits[NOLPHIN_ZOOM_LEVEL_N_ENTRIES];
static int desktop_text_ellipsis_limit;

static gboolean
get_text_ellipsis_limit_for_zoom (char **strs,
                  const char *zoom_level,
                  int *limit)
{
    char **p;
    char *str;
    gboolean success;

    success = FALSE;

    /* default */
    *limit = 3;

    if (zoom_level != NULL) {
        str = g_strdup_printf ("%s:%%d", zoom_level);
    } else {
        str = g_strdup ("%d");
    }

    if (strs != NULL) {
        for (p = strs; *p != NULL; p++) {
            if (sscanf (*p, str, limit)) {
                success = TRUE;
            }
        }
    }

    g_free (str);

    return success;
}

static const char * zoom_level_names[] = {
    "smallest",
    "smaller",
    "small",
    "standard",
    "large",
    "larger",
    "largest"
};

static void
text_ellipsis_limit_changed_callback (gpointer callback_data)
{
    char **pref;
    unsigned int i;
    int one_limit;

    pref = g_settings_get_strv (nolphin_icon_view_preferences,
                    NOLPHIN_PREFERENCES_ICON_VIEW_TEXT_ELLIPSIS_LIMIT);

    /* set default */
    get_text_ellipsis_limit_for_zoom (pref, NULL, &one_limit);
    for (i = 0; i < NOLPHIN_ZOOM_LEVEL_N_ENTRIES; i++) {
        text_ellipsis_limits[i] = one_limit;
    }

    /* override for each zoom level */
    for (i = 0; i < G_N_ELEMENTS(zoom_level_names); i++) {
        if (get_text_ellipsis_limit_for_zoom (pref,
                              zoom_level_names[i],
                              &one_limit)) {
            text_ellipsis_limits[i] = one_limit;
        }
    }

    g_strfreev (pref);
}

static void
desktop_text_ellipsis_limit_changed_callback (gpointer callback_data)
{
    int pref;

    pref = g_settings_get_int (nolphin_desktop_preferences, NOLPHIN_PREFERENCES_DESKTOP_TEXT_ELLIPSIS_LIMIT);
    desktop_text_ellipsis_limit = pref;
}

static gchar *
on_get_tooltip_text (NolphinIconContainer *container,
                     NolphinFile          *file,
                     gpointer           user_data)
{
    gboolean is_desktop, show_tooltip;
    gchar *tooltip_text = NULL;

    is_desktop = container->details->is_desktop;

    show_tooltip = (container->details->show_desktop_tooltips && is_desktop) ||
                   (container->details->show_icon_view_tooltips && !is_desktop);

    if (show_tooltip) {
        tooltip_text = nolphin_file_construct_tooltip (file, container->details->tooltip_flags, NULL);
    }

    return tooltip_text;
}

static gint
nolphin_icon_view_container_get_max_layout_lines_for_pango (NolphinIconContainer  *container)
{
    int limit;

    if (nolphin_icon_container_get_is_desktop (container)) {
        limit = desktop_text_ellipsis_limit;
    } else {
        limit = text_ellipsis_limits[container->details->zoom_level];
    }

    if (limit <= 0) {
        return G_MININT;
    }

    return -limit;
}

static gint
nolphin_icon_view_container_get_max_layout_lines (NolphinIconContainer  *container)
{
    int limit;

    if (nolphin_icon_container_get_is_desktop (container)) {
        limit = desktop_text_ellipsis_limit;
    } else {
        limit = text_ellipsis_limits[container->details->zoom_level];
    }

    if (limit <= 0) {
        return 3;
    }

    return limit;
}

static gint
nolphin_icon_view_container_get_additional_text_line_count (NolphinIconContainer *container)
{
    GQuark *attributes;
    gint len, i, real_count;

    attributes = nolphin_icon_view_container_get_icon_text_attribute_names (container, &len);

    i = 0;
    real_count = 0;

    for (i = 0; i < len; ++i) {
        if (attributes[i] == attribute_none_q) {
            continue;
        }

        real_count++;
    }

    return real_count;
}

static void
finalize (GObject *object)
{
    g_signal_handlers_disconnect_by_func (nolphin_icon_view_preferences,
                                          text_ellipsis_limit_changed_callback,
                                          NULL);

    g_signal_handlers_disconnect_by_func (nolphin_desktop_preferences,
                                          desktop_text_ellipsis_limit_changed_callback,
                                          NULL);

    g_signal_handlers_disconnect_by_func (nolphin_icon_view_preferences,
                                          update_auto_strv_as_quarks,
                                          &caption_attributes);

    G_OBJECT_CLASS (nolphin_icon_view_container_parent_class)->finalize (object);
}

static void
nolphin_icon_view_container_class_init (NolphinIconViewContainerClass *klass)
{
	NolphinIconContainerClass *ic_class;

    G_OBJECT_CLASS (klass)->finalize = finalize;

	ic_class = &klass->parent_class;

	attribute_none_q = g_quark_from_static_string ("none");

    ic_class->is_grid_container = FALSE;
	ic_class->get_icon_text = nolphin_icon_view_container_get_icon_text;
	ic_class->get_icon_images = nolphin_icon_view_container_get_icon_images;
	ic_class->get_icon_description = nolphin_icon_view_container_get_icon_description;
    ic_class->get_max_layout_lines_for_pango = nolphin_icon_view_container_get_max_layout_lines_for_pango;
    ic_class->get_max_layout_lines = nolphin_icon_view_container_get_max_layout_lines;

	ic_class->compare_icons = nolphin_icon_view_container_compare_icons;
	ic_class->freeze_updates = nolphin_icon_view_container_freeze_updates;
	ic_class->unfreeze_updates = nolphin_icon_view_container_unfreeze_updates;
    ic_class->lay_down_icons = nolphin_icon_view_container_lay_down_icons;
    ic_class->icon_set_position = nolphin_icon_view_container_icon_set_position;
    ic_class->move_icon = nolphin_icon_view_container_move_icon;
    ic_class->update_icon = nolphin_icon_view_container_update_icon;
    ic_class->align_icons = nolphin_icon_view_container_align_icons;
    ic_class->reload_icon_positions = nolphin_icon_view_container_reload_icon_positions;
    ic_class->finish_adding_new_icons = nolphin_icon_view_container_finish_adding_new_icons;
    ic_class->icon_get_bounding_box = nolphin_icon_view_container_icon_get_bounding_box;
    ic_class->set_zoom_level = nolphin_icon_view_container_set_zoom_level;
    ic_class->get_additional_text_line_count = nolphin_icon_view_container_get_additional_text_line_count;
}

static void
nolphin_icon_view_container_init (NolphinIconViewContainer *icon_container)
{
	gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (icon_container)),
				     GTK_STYLE_CLASS_VIEW);

    static gboolean setup_prefs = FALSE;

    g_signal_connect (icon_container, "get-tooltip-text", G_CALLBACK (on_get_tooltip_text), NULL);

    if (!setup_prefs) {
        g_signal_connect_swapped (nolphin_icon_view_preferences,
                      "changed::" NOLPHIN_PREFERENCES_ICON_VIEW_TEXT_ELLIPSIS_LIMIT,
                      G_CALLBACK (text_ellipsis_limit_changed_callback),
                      NULL);
        text_ellipsis_limit_changed_callback (NULL);

        g_signal_connect_swapped (nolphin_desktop_preferences,
                      "changed::" NOLPHIN_PREFERENCES_DESKTOP_TEXT_ELLIPSIS_LIMIT,
                      G_CALLBACK (desktop_text_ellipsis_limit_changed_callback),
                      NULL);
        desktop_text_ellipsis_limit_changed_callback (NULL);

        g_signal_connect (nolphin_icon_view_preferences,
                          "changed::" NOLPHIN_PREFERENCES_ICON_VIEW_CAPTIONS,
                          G_CALLBACK (update_auto_strv_as_quarks),
                          &caption_attributes);

        setup_prefs = TRUE;
    }
}

NolphinIconContainer *
nolphin_icon_view_container_construct (NolphinIconViewContainer *icon_container,
                                    NolphinIconView          *view,
                                    gboolean               is_desktop)
{
	AtkObject *atk_obj;
    NolphinViewLayoutConstants *constants = NOLPHIN_ICON_CONTAINER (icon_container)->details->view_constants;

	g_return_val_if_fail (NOLPHIN_IS_ICON_VIEW (view), NULL);

	icon_container->view = view;
    nolphin_icon_container_set_is_desktop (NOLPHIN_ICON_CONTAINER (icon_container), is_desktop);

	atk_obj = gtk_widget_get_accessible (GTK_WIDGET (icon_container));
	atk_object_set_name (atk_obj, _("Symbolansicht"));

    constants = NOLPHIN_ICON_CONTAINER (icon_container)->details->view_constants;

    constants->icon_pad_left = 4;
    constants->icon_pad_right = 4;
    constants->icon_pad_top = 4;
    constants->icon_pad_bottom = 4;
    constants->container_pad_left = 4;
    constants->container_pad_right = 4;
    constants->container_pad_top = 4;
    constants->container_pad_bottom = 4;
    constants->standard_icon_grid_width = 155;
    constants->text_beside_icon_grid_width = 205;
    constants->desktop_pad_horizontal = 10;
    constants->desktop_pad_vertical = 10;
    constants->snap_size_x = 78;
    constants->snap_size_y = 20;
    constants->max_text_width_standard = 135;
    constants->max_text_width_beside = 90;
    constants->max_text_width_beside_top_to_bottom = 150;

	return NOLPHIN_ICON_CONTAINER (icon_container);
}

NolphinIconContainer *
nolphin_icon_view_container_new (NolphinIconView *view,
                              gboolean      is_desktop)
{
    return nolphin_icon_view_container_construct (g_object_new (NOLPHIN_TYPE_ICON_VIEW_CONTAINER, NULL),
                                               view,
                                               is_desktop);
}

void
nolphin_icon_view_container_set_sort_desktop (NolphinIconViewContainer *container,
					       gboolean         desktop)
{
	container->sort_for_desktop = desktop;
}
