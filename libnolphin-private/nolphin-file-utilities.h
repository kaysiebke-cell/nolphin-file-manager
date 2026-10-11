/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nolphin-file-utilities.h - interface for file manipulation routines.

   Copyright (C) 1999, 2000, 2001 Eazel, Inc.

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

   Authors: John Sullivan <sullivan@eazel.com>
*/

#ifndef NOLPHIN_FILE_UTILITIES_H
#define NOLPHIN_FILE_UTILITIES_H

#include <gio/gio.h>
#include <gtk/gtk.h>

/* Used in view classes to mark pinned and favorite files */
#define UNAVAILABLE_TEXT_WEIGHT PANGO_WEIGHT_LIGHT
#define NORMAL_TEXT_WEIGHT PANGO_WEIGHT_NORMAL
#define PINNED_TEXT_WEIGHT PANGO_WEIGHT_BOLD

#define DEFAULT_NOLPHIN_DIRECTORY_MODE (0755)
#define DEFAULT_DESKTOP_DIRECTORY_MODE (0755)

/* These functions all return something something that needs to be
 * freed with g_free, is not NULL, and is guaranteed to exist.
 */
char *   nolphin_get_xdg_dir                        (const char *type);
char *   nolphin_get_user_directory                 (void);
char *   nolphin_get_desktop_directory              (void);
GFile *  nolphin_get_desktop_location               (void);
char *   nolphin_get_desktop_directory_uri          (void);
char *   nolphin_get_home_directory_uri             (void);
gboolean nolphin_is_desktop_directory_file          (GFile *dir,
						      const char *filename);
gboolean nolphin_is_root_directory                  (GFile *dir);
gboolean nolphin_is_desktop_directory               (GFile *dir);
gboolean nolphin_is_home_directory                  (GFile *dir);
gboolean nolphin_is_home_directory_file             (GFile *dir,
						      const char *filename);
gboolean nolphin_is_in_system_dir                   (GFile *location);
char *   nolphin_get_gmc_desktop_directory          (void);

gboolean nolphin_should_use_templates_directory     (void);
char *   nolphin_get_templates_directory            (void);
void     nolphin_ensure_valid_templates_directory   (void);
char *   nolphin_get_templates_directory_uri        (void);

char *   nolphin_get_searches_directory             (void);

char *	 nolphin_compute_title_for_location	     (GFile *file);
char *   nolphin_compute_search_title_for_location (GFile *location);
/* This function returns something that needs to be freed with g_free,
 * is not NULL, but is not garaunteed to exist */
char *   nolphin_get_desktop_directory_uri_no_create (void);

/* Locate a file in either the uers directory or the datadir. */
char *   nolphin_get_data_file_path                 (const char *partial_path);

gboolean nolphin_is_file_roller_installed           (void);

/* Inhibit/Uninhibit GNOME Power Manager */
int    nolphin_inhibit_power_manager                (const char *message) G_GNUC_WARN_UNUSED_RESULT;
void     nolphin_uninhibit_power_manager            (int cookie);

/* Return an allocated file name that is guranteed to be unique, but
 * tries to make the name readable to users.
 * This isn't race-free, so don't use for security-related things
 */
char *   nolphin_ensure_unique_file_name            (const char *directory_uri,
						      const char *base_name,
			                              const char *extension);
char *   nolphin_unique_temporary_file_name         (void);

GFile *  nolphin_find_existing_uri_in_hierarchy     (GFile *location);

GFile *
nolphin_find_file_insensitive (GFile *parent, const gchar *name);

char * nolphin_get_accel_map_file (void);

char * nolphin_get_scripts_directory_path (void);

GHashTable * nolphin_trashed_files_get_original_directories (GList *files,
							      GList **unhandled_files);
void nolphin_restore_files_from_trash (GList *files,
					GtkWindow *parent_window);

typedef void (*NolphinMountGetContent) (const char **content, gpointer user_data);

char ** nolphin_get_cached_x_content_types_for_mount (GMount *mount);
void nolphin_get_x_content_types_for_mount_async (GMount *mount,
						   NolphinMountGetContent callback,
						   GCancellable *cancellable,
						   gpointer user_data);

gchar *nolphin_get_mount_icon_name (GMount *mount);
gchar *nolphin_get_volume_icon_name (GVolume *volume);
gchar *nolphin_get_drive_icon_name (GDrive *drive);

gchar *nolphin_get_best_guess_file_mimetype (const gchar *filename,
                                          GFileInfo   *info,
                                          goffset      size);

gboolean nolphin_treating_root_as_normal (void);
gboolean nolphin_user_is_root (void);

GMount *nolphin_get_mount_for_location_safe (GFile *location);
gboolean nolphin_location_is_network_safe (GFile *location);
gboolean nolphin_path_is_network_safe (const gchar *path);
#endif /* NOLPHIN_FILE_UTILITIES_H */
