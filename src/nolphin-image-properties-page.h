/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* 
 * Copyright (C) 2004 Red Hat, Inc
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
 * Boston, MA 02110-1335, USA.
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 */

#ifndef NOLPHIN_IMAGE_PROPERTIES_PAGE_H
#define NOLPHIN_IMAGE_PROPERTIES_PAGE_H

#include <gtk/gtk.h>

#define NOLPHIN_TYPE_IMAGE_PROPERTIES_PAGE nolphin_image_properties_page_get_type()
#define NOLPHIN_IMAGE_PROPERTIES_PAGE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NOLPHIN_TYPE_IMAGE_PROPERTIES_PAGE, NolphinImagePropertiesPage))
#define NOLPHIN_IMAGE_PROPERTIES_PAGE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NOLPHIN_TYPE_IMAGE_PROPERTIES_PAGE, NolphinImagePropertiesPageClass))
#define NOLPHIN_IS_IMAGE_PROPERTIES_PAGE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NOLPHIN_TYPE_IMAGE_PROPERTIES_PAGE))
#define NOLPHIN_IS_IMAGE_PROPERTIES_PAGE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NOLPHIN_TYPE_IMAGE_PROPERTIES_PAGE))
#define NOLPHIN_IMAGE_PROPERTIES_PAGE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NOLPHIN_TYPE_IMAGE_PROPERTIES_PAGE, NolphinImagePropertiesPageClass))

typedef struct NolphinImagePropertiesPageDetails NolphinImagePropertiesPageDetails;

typedef struct {
	GtkBox parent;
	NolphinImagePropertiesPageDetails *details;
} NolphinImagePropertiesPage;

typedef struct {
	GtkBoxClass parent;
} NolphinImagePropertiesPageClass;

GType nolphin_image_properties_page_get_type (void);
void  nolphin_image_properties_page_register (void);

#endif /* NOLPHIN_IMAGE_PROPERTIES_PAGE_H */
