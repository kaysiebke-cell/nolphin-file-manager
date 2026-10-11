/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Nolphin
 *
 * Copyright (C) 2011 Red Hat, Inc.
 *
 * Nolphin is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nolphin is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; see the file COPYING.  If not,
 * write to the Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
 * Boston, MA 02110-1335, USA.
 *
 * Author: Cosimo Cecchi <cosimoc@redhat.com>
 */

#ifndef __NOLPHIN_PROGRESS_INFO_MANAGER_H__
#define __NOLPHIN_PROGRESS_INFO_MANAGER_H__

#include <glib-object.h>

#include <libnolphin-private/nolphin-progress-info.h>

#define NOLPHIN_TYPE_PROGRESS_INFO_MANAGER nolphin_progress_info_manager_get_type()
#define NOLPHIN_PROGRESS_INFO_MANAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NOLPHIN_TYPE_PROGRESS_INFO_MANAGER, NolphinProgressInfoManager))
#define NOLPHIN_PROGRESS_INFO_MANAGER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NOLPHIN_TYPE_PROGRESS_INFO_MANAGER, NolphinProgressInfoManagerClass))
#define NOLPHIN_IS_PROGRESS_INFO_MANAGER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NOLPHIN_TYPE_PROGRESS_INFO_MANAGER))
#define NOLPHIN_IS_PROGRESS_INFO_MANAGER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NOLPHIN_TYPE_PROGRESS_INFO_MANAGER))
#define NOLPHIN_PROGRESS_INFO_MANAGER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NOLPHIN_TYPE_PROGRESS_INFO_MANAGER, NolphinProgressInfoManagerClass))

typedef struct _NolphinProgressInfoManager NolphinProgressInfoManager;
typedef struct _NolphinProgressInfoManagerClass NolphinProgressInfoManagerClass;
typedef struct _NolphinProgressInfoManagerPriv NolphinProgressInfoManagerPriv;

struct _NolphinProgressInfoManager {
  GObject parent;

  /* private */
  NolphinProgressInfoManagerPriv *priv;
};

struct _NolphinProgressInfoManagerClass {
  GObjectClass parent_class;
};

GType nolphin_progress_info_manager_get_type (void);

NolphinProgressInfoManager* nolphin_progress_info_manager_new (void);

void nolphin_progress_info_manager_add_new_info (NolphinProgressInfoManager *self,
                                                  NolphinProgressInfo *info);
GList *nolphin_progress_info_manager_get_all_infos (NolphinProgressInfoManager *self);

G_END_DECLS

#endif /* __NOLPHIN_PROGRESS_INFO_MANAGER_H__ */
