/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nolphin-file-undo-operations.h - Manages undo/redo of file operations
 *
 * Copyright (C) 2007-2011 Amos Brocco
 * Copyright (C) 2010 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street - Suite 500,
 * Boston, MA 02110-1335, USA.
 *
 * Authors: Amos Brocco <amos.brocco@gmail.com>
 *          Cosimo Cecchi <cosimoc@redhat.com>
 *
 */

#ifndef __NOLPHIN_FILE_UNDO_OPERATIONS_H__
#define __NOLPHIN_FILE_UNDO_OPERATIONS_H__

#include <gio/gio.h>
#include <gtk/gtk.h>

typedef enum {
	NOLPHIN_FILE_UNDO_OP_COPY,
	NOLPHIN_FILE_UNDO_OP_DUPLICATE,
	NOLPHIN_FILE_UNDO_OP_MOVE,
	NOLPHIN_FILE_UNDO_OP_RENAME,
	NOLPHIN_FILE_UNDO_OP_CREATE_EMPTY_FILE,
	NOLPHIN_FILE_UNDO_OP_CREATE_FILE_FROM_TEMPLATE,
	NOLPHIN_FILE_UNDO_OP_CREATE_FOLDER,
	NOLPHIN_FILE_UNDO_OP_MOVE_TO_TRASH,
	NOLPHIN_FILE_UNDO_OP_RESTORE_FROM_TRASH,
	NOLPHIN_FILE_UNDO_OP_CREATE_LINK,
	NOLPHIN_FILE_UNDO_OP_RECURSIVE_SET_PERMISSIONS,
	NOLPHIN_FILE_UNDO_OP_SET_PERMISSIONS,
	NOLPHIN_FILE_UNDO_OP_CHANGE_GROUP,
	NOLPHIN_FILE_UNDO_OP_CHANGE_OWNER,
	NOLPHIN_FILE_UNDO_OP_NUM_TYPES,
} NolphinFileUndoOp;

#define NOLPHIN_TYPE_FILE_UNDO_INFO         (nolphin_file_undo_info_get_type ())
#define NOLPHIN_FILE_UNDO_INFO(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), NOLPHIN_TYPE_FILE_UNDO_INFO, NolphinFileUndoInfo))
#define NOLPHIN_FILE_UNDO_INFO_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), NOLPHIN_TYPE_FILE_UNDO_INFO, NolphinFileUndoInfoClass))
#define NOLPHIN_IS_FILE_UNDO_INFO(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), NOLPHIN_TYPE_FILE_UNDO_INFO))
#define NOLPHIN_IS_FILE_UNDO_INFO_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), NOLPHIN_TYPE_FILE_UNDO_INFO))
#define NOLPHIN_FILE_UNDO_INFO_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), NOLPHIN_TYPE_FILE_UNDO_INFO, NolphinFileUndoInfoClass))

typedef struct _NolphinFileUndoInfo      NolphinFileUndoInfo;
typedef struct _NolphinFileUndoInfoClass NolphinFileUndoInfoClass;
typedef struct _NolphinFileUndoInfoDetails NolphinFileUndoInfoDetails;

struct _NolphinFileUndoInfo {
	GObject parent;
	NolphinFileUndoInfoDetails *priv;
};

struct _NolphinFileUndoInfoClass {
	GObjectClass parent_class;

	void (* undo_func) (NolphinFileUndoInfo *self,
			    GtkWindow            *parent_window);
	void (* redo_func) (NolphinFileUndoInfo *self,
			    GtkWindow            *parent_window);

	void (* strings_func) (NolphinFileUndoInfo *self,
			       gchar **undo_label,
			       gchar **undo_description,
			       gchar **redo_label,
			       gchar **redo_description);
};

GType nolphin_file_undo_info_get_type (void) G_GNUC_CONST;

void nolphin_file_undo_info_apply_async (NolphinFileUndoInfo *self,
					  gboolean undo,
					  GtkWindow *parent_window,
					  GAsyncReadyCallback callback,
					  gpointer user_data);
gboolean nolphin_file_undo_info_apply_finish (NolphinFileUndoInfo *self,
					       GAsyncResult *res,
					       gboolean *user_cancel,
					       GError **error);

void nolphin_file_undo_info_get_strings (NolphinFileUndoInfo *self,
					  gchar **undo_label,
					  gchar **undo_description,
					  gchar **redo_label,
					  gchar **redo_description);

/* copy/move/duplicate/link/restore from trash */
#define NOLPHIN_TYPE_FILE_UNDO_INFO_EXT         (nolphin_file_undo_info_ext_get_type ())
#define NOLPHIN_FILE_UNDO_INFO_EXT(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), NOLPHIN_TYPE_FILE_UNDO_INFO_EXT, NolphinFileUndoInfoExt))
#define NOLPHIN_FILE_UNDO_INFO_EXT_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), NOLPHIN_TYPE_FILE_UNDO_INFO_EXT, NolphinFileUndoInfoExtClass))
#define NOLPHIN_IS_FILE_UNDO_INFO_EXT(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), NOLPHIN_TYPE_FILE_UNDO_INFO_EXT))
#define NOLPHIN_IS_FILE_UNDO_INFO_EXT_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), NOLPHIN_TYPE_FILE_UNDO_INFO_EXT))
#define NOLPHIN_FILE_UNDO_INFO_EXT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), NOLPHIN_TYPE_FILE_UNDO_INFO_EXT, NolphinFileUndoInfoExtClass))

typedef struct _NolphinFileUndoInfoExt      NolphinFileUndoInfoExt;
typedef struct _NolphinFileUndoInfoExtClass NolphinFileUndoInfoExtClass;
typedef struct _NolphinFileUndoInfoExtDetails NolphinFileUndoInfoExtDetails;

struct _NolphinFileUndoInfoExt {
	NolphinFileUndoInfo parent;
	NolphinFileUndoInfoExtDetails *priv;
};

struct _NolphinFileUndoInfoExtClass {
	NolphinFileUndoInfoClass parent_class;
};

GType nolphin_file_undo_info_ext_get_type (void) G_GNUC_CONST;
NolphinFileUndoInfo *nolphin_file_undo_info_ext_new (NolphinFileUndoOp op_type,
						       gint item_count,
						       GFile *src_dir,
						       GFile *target_dir);
void nolphin_file_undo_info_ext_add_origin_target_pair (NolphinFileUndoInfoExt *self,
							 GFile                   *origin,
							 GFile                   *target);

/* create new file/folder */
#define NOLPHIN_TYPE_FILE_UNDO_INFO_CREATE         (nolphin_file_undo_info_create_get_type ())
#define NOLPHIN_FILE_UNDO_INFO_CREATE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), NOLPHIN_TYPE_FILE_UNDO_INFO_CREATE, NolphinFileUndoInfoCreate))
#define NOLPHIN_FILE_UNDO_INFO_CREATE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), NOLPHIN_TYPE_FILE_UNDO_INFO_CREATE, NolphinFileUndoInfoCreateClass))
#define NOLPHIN_IS_FILE_UNDO_INFO_CREATE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), NOLPHIN_TYPE_FILE_UNDO_INFO_CREATE))
#define NOLPHIN_IS_FILE_UNDO_INFO_CREATE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), NOLPHIN_TYPE_FILE_UNDO_INFO_CREATE))
#define NOLPHIN_FILE_UNDO_INFO_CREATE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), NOLPHIN_TYPE_FILE_UNDO_INFO_CREATE, NolphinFileUndoInfoCreateClass))

typedef struct _NolphinFileUndoInfoCreate      NolphinFileUndoInfoCreate;
typedef struct _NolphinFileUndoInfoCreateClass NolphinFileUndoInfoCreateClass;
typedef struct _NolphinFileUndoInfoCreateDetails NolphinFileUndoInfoCreateDetails;

struct _NolphinFileUndoInfoCreate {
	NolphinFileUndoInfo parent;
	NolphinFileUndoInfoCreateDetails *priv;
};

struct _NolphinFileUndoInfoCreateClass {
	NolphinFileUndoInfoClass parent_class;
};

GType nolphin_file_undo_info_create_get_type (void) G_GNUC_CONST;
NolphinFileUndoInfo *nolphin_file_undo_info_create_new (NolphinFileUndoOp op_type);
void nolphin_file_undo_info_create_set_data (NolphinFileUndoInfoCreate *self,
					      GFile                      *file,
					      const char                 *template,
					      gint                        length);

/* rename */
#define NOLPHIN_TYPE_FILE_UNDO_INFO_RENAME         (nolphin_file_undo_info_rename_get_type ())
#define NOLPHIN_FILE_UNDO_INFO_RENAME(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), NOLPHIN_TYPE_FILE_UNDO_INFO_RENAME, NolphinFileUndoInfoRename))
#define NOLPHIN_FILE_UNDO_INFO_RENAME_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), NOLPHIN_TYPE_FILE_UNDO_INFO_RENAME, NolphinFileUndoInfoRenameClass))
#define NOLPHIN_IS_FILE_UNDO_INFO_RENAME(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), NOLPHIN_TYPE_FILE_UNDO_INFO_RENAME))
#define NOLPHIN_IS_FILE_UNDO_INFO_RENAME_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), NOLPHIN_TYPE_FILE_UNDO_INFO_RENAME))
#define NOLPHIN_FILE_UNDO_INFO_RENAME_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), NOLPHIN_TYPE_FILE_UNDO_INFO_RENAME, NolphinFileUndoInfoRenameClass))

typedef struct _NolphinFileUndoInfoRename      NolphinFileUndoInfoRename;
typedef struct _NolphinFileUndoInfoRenameClass NolphinFileUndoInfoRenameClass;
typedef struct _NolphinFileUndoInfoRenameDetails NolphinFileUndoInfoRenameDetails;

struct _NolphinFileUndoInfoRename {
	NolphinFileUndoInfo parent;
	NolphinFileUndoInfoRenameDetails *priv;
};

struct _NolphinFileUndoInfoRenameClass {
	NolphinFileUndoInfoClass parent_class;
};

GType nolphin_file_undo_info_rename_get_type (void) G_GNUC_CONST;
NolphinFileUndoInfo *nolphin_file_undo_info_rename_new (void);
void nolphin_file_undo_info_rename_set_data_pre (NolphinFileUndoInfoRename *self,
						  GFile                      *old_file,
						  gchar                      *old_display_name,
						  gchar                      *new_display_name);
void nolphin_file_undo_info_rename_set_data_post (NolphinFileUndoInfoRename *self,
						   GFile                      *new_file);

/* trash */
#define NOLPHIN_TYPE_FILE_UNDO_INFO_TRASH         (nolphin_file_undo_info_trash_get_type ())
#define NOLPHIN_FILE_UNDO_INFO_TRASH(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), NOLPHIN_TYPE_FILE_UNDO_INFO_TRASH, NolphinFileUndoInfoTrash))
#define NOLPHIN_FILE_UNDO_INFO_TRASH_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), NOLPHIN_TYPE_FILE_UNDO_INFO_TRASH, NolphinFileUndoInfoTrashClass))
#define NOLPHIN_IS_FILE_UNDO_INFO_TRASH(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), NOLPHIN_TYPE_FILE_UNDO_INFO_TRASH))
#define NOLPHIN_IS_FILE_UNDO_INFO_TRASH_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), NOLPHIN_TYPE_FILE_UNDO_INFO_TRASH))
#define NOLPHIN_FILE_UNDO_INFO_TRASH_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), NOLPHIN_TYPE_FILE_UNDO_INFO_TRASH, NolphinFileUndoInfoTrashClass))

typedef struct _NolphinFileUndoInfoTrash      NolphinFileUndoInfoTrash;
typedef struct _NolphinFileUndoInfoTrashClass NolphinFileUndoInfoTrashClass;
typedef struct _NolphinFileUndoInfoTrashDetails NolphinFileUndoInfoTrashDetails;

struct _NolphinFileUndoInfoTrash {
	NolphinFileUndoInfo parent;
	NolphinFileUndoInfoTrashDetails *priv;
};

struct _NolphinFileUndoInfoTrashClass {
	NolphinFileUndoInfoClass parent_class;
};

GType nolphin_file_undo_info_trash_get_type (void) G_GNUC_CONST;
NolphinFileUndoInfo *nolphin_file_undo_info_trash_new (gint item_count);
void nolphin_file_undo_info_trash_add_file (NolphinFileUndoInfoTrash *self,
					     GFile                     *file);

/* recursive permissions */
#define NOLPHIN_TYPE_FILE_UNDO_INFO_REC_PERMISSIONS         (nolphin_file_undo_info_rec_permissions_get_type ())
#define NOLPHIN_FILE_UNDO_INFO_REC_PERMISSIONS(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), NOLPHIN_TYPE_FILE_UNDO_INFO_REC_PERMISSIONS, NolphinFileUndoInfoRecPermissions))
#define NOLPHIN_FILE_UNDO_INFO_REC_PERMISSIONS_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), NOLPHIN_TYPE_FILE_UNDO_INFO_REC_PERMISSIONS, NolphinFileUndoInfoRecPermissionsClass))
#define NOLPHIN_IS_FILE_UNDO_INFO_REC_PERMISSIONS(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), NOLPHIN_TYPE_FILE_UNDO_INFO_REC_PERMISSIONS))
#define NOLPHIN_IS_FILE_UNDO_INFO_REC_PERMISSIONS_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), NOLPHIN_TYPE_FILE_UNDO_INFO_REC_PERMISSIONS))
#define NOLPHIN_FILE_UNDO_INFO_REC_PERMISSIONS_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), NOLPHIN_TYPE_FILE_UNDO_INFO_REC_PERMISSIONS, NolphinFileUndoInfoRecPermissionsClass))

typedef struct _NolphinFileUndoInfoRecPermissions      NolphinFileUndoInfoRecPermissions;
typedef struct _NolphinFileUndoInfoRecPermissionsClass NolphinFileUndoInfoRecPermissionsClass;
typedef struct _NolphinFileUndoInfoRecPermissionsDetails NolphinFileUndoInfoRecPermissionsDetails;

struct _NolphinFileUndoInfoRecPermissions {
	NolphinFileUndoInfo parent;
	NolphinFileUndoInfoRecPermissionsDetails *priv;
};

struct _NolphinFileUndoInfoRecPermissionsClass {
	NolphinFileUndoInfoClass parent_class;
};

GType nolphin_file_undo_info_rec_permissions_get_type (void) G_GNUC_CONST;
NolphinFileUndoInfo *nolphin_file_undo_info_rec_permissions_new (GFile   *dest,
								   guint32 file_permissions,
								   guint32 file_mask,
								   guint32 dir_permissions,
								   guint32 dir_mask);
void nolphin_file_undo_info_rec_permissions_add_file (NolphinFileUndoInfoRecPermissions *self,
						       GFile                              *file,
						       guint32                             permission);

/* single file change permissions */
#define NOLPHIN_TYPE_FILE_UNDO_INFO_PERMISSIONS         (nolphin_file_undo_info_permissions_get_type ())
#define NOLPHIN_FILE_UNDO_INFO_PERMISSIONS(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), NOLPHIN_TYPE_FILE_UNDO_INFO_PERMISSIONS, NolphinFileUndoInfoPermissions))
#define NOLPHIN_FILE_UNDO_INFO_PERMISSIONS_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), NOLPHIN_TYPE_FILE_UNDO_INFO_PERMISSIONS, NolphinFileUndoInfoPermissionsClass))
#define NOLPHIN_IS_FILE_UNDO_INFO_PERMISSIONS(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), NOLPHIN_TYPE_FILE_UNDO_INFO_PERMISSIONS))
#define NOLPHIN_IS_FILE_UNDO_INFO_PERMISSIONS_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), NOLPHIN_TYPE_FILE_UNDO_INFO_PERMISSIONS))
#define NOLPHIN_FILE_UNDO_INFO_PERMISSIONS_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), NOLPHIN_TYPE_FILE_UNDO_INFO_PERMISSIONS, NolphinFileUndoInfoPermissionsClass))

typedef struct _NolphinFileUndoInfoPermissions      NolphinFileUndoInfoPermissions;
typedef struct _NolphinFileUndoInfoPermissionsClass NolphinFileUndoInfoPermissionsClass;
typedef struct _NolphinFileUndoInfoPermissionsDetails NolphinFileUndoInfoPermissionsDetails;

struct _NolphinFileUndoInfoPermissions {
	NolphinFileUndoInfo parent;
	NolphinFileUndoInfoPermissionsDetails *priv;
};

struct _NolphinFileUndoInfoPermissionsClass {
	NolphinFileUndoInfoClass parent_class;
};

GType nolphin_file_undo_info_permissions_get_type (void) G_GNUC_CONST;
NolphinFileUndoInfo *nolphin_file_undo_info_permissions_new (GFile   *file,
							       guint32  current_permissions,
							       guint32  new_permissions);

/* group and owner change */
#define NOLPHIN_TYPE_FILE_UNDO_INFO_OWNERSHIP         (nolphin_file_undo_info_ownership_get_type ())
#define NOLPHIN_FILE_UNDO_INFO_OWNERSHIP(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), NOLPHIN_TYPE_FILE_UNDO_INFO_OWNERSHIP, NolphinFileUndoInfoOwnership))
#define NOLPHIN_FILE_UNDO_INFO_OWNERSHIP_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), NOLPHIN_TYPE_FILE_UNDO_INFO_OWNERSHIP, NolphinFileUndoInfoOwnershipClass))
#define NOLPHIN_IS_FILE_UNDO_INFO_OWNERSHIP(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), NOLPHIN_TYPE_FILE_UNDO_INFO_OWNERSHIP))
#define NOLPHIN_IS_FILE_UNDO_INFO_OWNERSHIP_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), NOLPHIN_TYPE_FILE_UNDO_INFO_OWNERSHIP))
#define NOLPHIN_FILE_UNDO_INFO_OWNERSHIP_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), NOLPHIN_TYPE_FILE_UNDO_INFO_OWNERSHIP, NolphinFileUndoInfoOwnershipClass))

typedef struct _NolphinFileUndoInfoOwnership      NolphinFileUndoInfoOwnership;
typedef struct _NolphinFileUndoInfoOwnershipClass NolphinFileUndoInfoOwnershipClass;
typedef struct _NolphinFileUndoInfoOwnershipDetails NolphinFileUndoInfoOwnershipDetails;

struct _NolphinFileUndoInfoOwnership {
	NolphinFileUndoInfo parent;
	NolphinFileUndoInfoOwnershipDetails *priv;
};

struct _NolphinFileUndoInfoOwnershipClass {
	NolphinFileUndoInfoClass parent_class;
};

GType nolphin_file_undo_info_ownership_get_type (void) G_GNUC_CONST;
NolphinFileUndoInfo *nolphin_file_undo_info_ownership_new (NolphinFileUndoOp  op_type,
							     GFile              *file,
							     const char         *current_data,
							     const char         *new_data);

#endif /* __NOLPHIN_FILE_UNDO_OPERATIONS_H__ */
