/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
   nolphin-job-queue.h - file operation queue

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
   Boston, MA 02110-1335, USA.
*/

#ifndef __NOLPHIN_JOB_QUEUE_H__
#define __NOLPHIN_JOB_QUEUE_H__

#include <glib-object.h>

#include <libnolphin-private/nolphin-progress-info.h>

#define NOLPHIN_TYPE_JOB_QUEUE nolphin_job_queue_get_type()
#define NOLPHIN_JOB_QUEUE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NOLPHIN_TYPE_JOB_QUEUE, NolphinJobQueue))
#define NOLPHIN_JOB_QUEUE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NOLPHIN_TYPE_JOB_QUEUE, NolphinJobQueueClass))
#define NOLPHIN_IS_JOB_QUEUE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NOLPHIN_TYPE_JOB_QUEUE))
#define NOLPHIN_IS_JOB_QUEUE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NOLPHIN_TYPE_JOB_QUEUE))
#define NOLPHIN_JOB_QUEUE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NOLPHIN_TYPE_JOB_QUEUE, NolphinJobQueueClass))

typedef struct _NolphinJobQueue NolphinJobQueue;
typedef struct _NolphinJobQueueClass NolphinJobQueueClass;
typedef struct _NolphinJobQueuePriv NolphinJobQueuePriv;

struct _NolphinJobQueue {
  GObject parent;

  /* private */
  NolphinJobQueuePriv *priv;
};

struct _NolphinJobQueueClass {
  GObjectClass parent_class;
};

GType nolphin_job_queue_get_type (void);

NolphinJobQueue *nolphin_job_queue_get (void);

void nolphin_job_queue_add_new_job (NolphinJobQueue *self,
                                 GIOSchedulerJobFunc job_func,
                                 gpointer user_data,
                                 GCancellable *cancellable,
                                 NolphinProgressInfo *info,
                                 gboolean start_immediately);

void nolphin_job_queue_start_next_job (NolphinJobQueue *self);

void nolphin_job_queue_start_job_by_info (NolphinJobQueue     *self,
                                       NolphinProgressInfo *info);

GList *nolphin_job_queue_get_all_jobs (NolphinJobQueue *self);

G_END_DECLS

#endif /* __NOLPHIN_JOB_QUEUE_H__ */
