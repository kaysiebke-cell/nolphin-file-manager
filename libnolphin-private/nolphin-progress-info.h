/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nolphin-progress-info.h: file operation progress info.
 
   Copyright (C) 2007 Red Hat, Inc.
  
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
  
   Author: Alexander Larsson <alexl@redhat.com>
*/

#ifndef NOLPHIN_PROGRESS_INFO_H
#define NOLPHIN_PROGRESS_INFO_H

#include <glib-object.h>
#include <gio/gio.h>

#define NOLPHIN_TYPE_PROGRESS_INFO         (nolphin_progress_info_get_type ())
#define NOLPHIN_PROGRESS_INFO(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), NOLPHIN_TYPE_PROGRESS_INFO, NolphinProgressInfo))
#define NOLPHIN_PROGRESS_INFO_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), NOLPHIN_TYPE_PROGRESS_INFO, NolphinProgressInfoClass))
#define NOLPHIN_IS_PROGRESS_INFO(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), NOLPHIN_TYPE_PROGRESS_INFO))
#define NOLPHIN_IS_PROGRESS_INFO_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), NOLPHIN_TYPE_PROGRESS_INFO))
#define NOLPHIN_PROGRESS_INFO_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), NOLPHIN_TYPE_PROGRESS_INFO, NolphinProgressInfoClass))

typedef struct _NolphinProgressInfo      NolphinProgressInfo;
typedef struct _NolphinProgressInfoClass NolphinProgressInfoClass;

GType nolphin_progress_info_get_type (void) G_GNUC_CONST;

/* Signals:
   "changed" - status or details changed
   "progress-changed" - the percentage progress changed (or we pulsed if in activity_mode
   "started" - emited on job start
   "finished" - emitted when job is done
   
   All signals are emitted from idles in main loop.
   All methods are threadsafe.
 */

NolphinProgressInfo *nolphin_progress_info_new (void);

GList *       nolphin_get_all_progress_info (void);

char *        nolphin_progress_info_get_status      (NolphinProgressInfo *info);
char *        nolphin_progress_info_get_details     (NolphinProgressInfo *info);
char *        nolphin_progress_info_get_initial_details (NolphinProgressInfo *info);
double        nolphin_progress_info_get_progress    (NolphinProgressInfo *info);
GCancellable *nolphin_progress_info_get_cancellable (NolphinProgressInfo *info);
void          nolphin_progress_info_cancel          (NolphinProgressInfo *info);
gboolean      nolphin_progress_info_get_is_started  (NolphinProgressInfo *info);
gboolean      nolphin_progress_info_get_is_finished (NolphinProgressInfo *info);
gboolean      nolphin_progress_info_get_is_paused   (NolphinProgressInfo *info);
gboolean      nolphin_progress_info_get_had_error   (NolphinProgressInfo *info);
void          nolphin_progress_info_set_had_error   (NolphinProgressInfo *info);

void          nolphin_progress_info_queue           (NolphinProgressInfo *info);
void          nolphin_progress_info_start           (NolphinProgressInfo *info);
void          nolphin_progress_info_finish          (NolphinProgressInfo *info);
void          nolphin_progress_info_pause           (NolphinProgressInfo *info);
void          nolphin_progress_info_resume          (NolphinProgressInfo *info);
void          nolphin_progress_info_set_status      (NolphinProgressInfo *info,
						      const char           *status);
void          nolphin_progress_info_take_status     (NolphinProgressInfo *info,
						      char                 *status);
void          nolphin_progress_info_set_details     (NolphinProgressInfo *info,
						      const char           *details);
void          nolphin_progress_info_take_initial_details (NolphinProgressInfo *info,
                              char                 *initial_details);
void          nolphin_progress_info_take_details    (NolphinProgressInfo *info,
						      char                 *details);
void          nolphin_progress_info_set_progress    (NolphinProgressInfo *info,
						      double                current,
						      double                total);
void          nolphin_progress_info_pulse_progress  (NolphinProgressInfo *info);

gdouble       nolphin_progress_info_get_elapsed_time (NolphinProgressInfo *info);


#endif /* NOLPHIN_PROGRESS_INFO_H */
