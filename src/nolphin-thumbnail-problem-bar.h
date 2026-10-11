/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street - Suite 500, Boston, MA 02110-1335, USA.
 *
 */

#ifndef __NOLPHIN_THUMBNAIL_PROBLEM_BAR_H
#define __NOLPHIN_THUMBNAIL_PROBLEM_BAR_H

#include "nolphin-view.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define NOLPHIN_TYPE_THUMBNAIL_PROBLEM_BAR         (nolphin_thumbnail_problem_bar_get_type ())
#define NOLPHIN_THUMBNAIL_PROBLEM_BAR(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), NOLPHIN_TYPE_THUMBNAIL_PROBLEM_BAR, NolphinThumbnailProblemBar))
#define NOLPHIN_THUMBNAIL_PROBLEM_BAR_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), NOLPHIN_TYPE_THUMBNAIL_PROBLEM_BAR, NolphinThumbnailProblemBarClass))
#define NOLPHIN_IS_THUMBNAIL_PROBLEM_BAR(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), NOLPHIN_TYPE_THUMBNAIL_PROBLEM_BAR))
#define NOLPHIN_IS_THUMBNAIL_PROBLEM_BAR_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), NOLPHIN_TYPE_THUMBNAIL_PROBLEM_BAR))
#define NOLPHIN_THUMBNAIL_PROBLEM_BAR_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), NOLPHIN_TYPE_THUMBNAIL_PROBLEM_BAR, NolphinThumbnailProblemBarClass))

typedef struct NolphinThumbnailProblemBarPrivate NolphinThumbnailProblemBarPrivate;

typedef struct
{
	GtkInfoBar parent;

	NolphinThumbnailProblemBarPrivate *priv;
} NolphinThumbnailProblemBar;

typedef struct
{
	GtkInfoBarClass parent_class;
} NolphinThumbnailProblemBarClass;

GType		 nolphin_thumbnail_problem_bar_get_type	(void) G_GNUC_CONST;
GtkWidget       *nolphin_thumbnail_problem_bar_new         (NolphinView *view);

G_END_DECLS

#endif /* __NOLPHIN_THUMBNAIL_PROBLEM_BAR_H */
