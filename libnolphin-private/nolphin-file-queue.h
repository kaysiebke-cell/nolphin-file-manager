/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   Copyright (C) 2001 Maciej Stachowiak
  
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

   Author: Maciej Stachowiak <mjs@noisehavoc.org>
*/

#ifndef NOLPHIN_FILE_QUEUE_H
#define NOLPHIN_FILE_QUEUE_H

#include <libnolphin-private/nolphin-file.h>

typedef struct NolphinFileQueue NolphinFileQueue;

NolphinFileQueue *nolphin_file_queue_new      (void);
void               nolphin_file_queue_destroy  (NolphinFileQueue *queue);

/* Add a file to the tail of the queue, unless it's already in the queue */
void               nolphin_file_queue_enqueue  (NolphinFileQueue *queue,
						 NolphinFile      *file);

/* Return the file at the head of the queue after removing it from the
 * queue. This is dangerous unless you have another ref to the file,
 * since it will unref it.  
 */
NolphinFile *     nolphin_file_queue_dequeue  (NolphinFileQueue *queue);

/* Remove a file from an arbitrary point in the queue in constant time. */
void               nolphin_file_queue_remove   (NolphinFileQueue *queue,
						 NolphinFile      *file);

/* Get the file at the head of the queue without removing or unrefing it. */
NolphinFile *     nolphin_file_queue_head     (NolphinFileQueue *queue);

gboolean           nolphin_file_queue_is_empty (NolphinFileQueue *queue);

#endif /* NOLPHIN_FILE_CHANGES_QUEUE_H */
