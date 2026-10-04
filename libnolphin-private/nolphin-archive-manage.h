/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nolphin-archive-manage.h: Archivverwaltung (§36) - Inhalt anzeigen,
 * Dateien hinzufügen, ersetzen, entfernen und einzelne Einträge entpacken.
 *
 * Alles läuft über die vorhandenen Systemwerkzeuge (tar, zip/unzip, 7z),
 * jeweils asynchron in einem Worker-Thread. TAR-basierte Formate lassen
 * sich nicht in-place ändern: dafür wird das Archiv entpackt, geändert und
 * neu gepackt (§36: der Benutzer wird bei großen Archiven darauf hingewiesen,
 * siehe nolphin_archive_manage_needs_repack()).
 */

#ifndef NOLPHIN_ARCHIVE_MANAGE_H
#define NOLPHIN_ARCHIVE_MANAGE_H

#include <gio/gio.h>
#include <libnolphin-private/nolphin-archive.h>

G_BEGIN_DECLS

typedef struct {
	gchar    *path;      /* Pfad im Archiv, Ordner ohne abschließenden "/" */
	goffset   size;      /* entpackte Größe, -1 wenn unbekannt */
	gboolean  is_dir;
	gchar    *modified;  /* Änderungsdatum als Text, kann NULL sein */
} NolphinArchiveEntry;

void nolphin_archive_entry_free (NolphinArchiveEntry *entry);

/* Kann der Inhalt dieses Archivs angezeigt / geändert werden? */
gboolean nolphin_archive_manage_can_list   (NolphinArchiveFormat format);
gboolean nolphin_archive_manage_can_modify (NolphinArchiveFormat format);
/* TRUE für TAR-Formate: Ändern bedeutet Neupacken des ganzen Archivs. */
gboolean nolphin_archive_manage_needs_repack (NolphinArchiveFormat format);

/* Inhalt auflisten: Ergebnis ist ein GPtrArray von NolphinArchiveEntry
 * (mit nolphin_archive_entry_free als Free-Funktion). */
void       nolphin_archive_list_async  (GFile *archive, GCancellable *cancellable,
					GAsyncReadyCallback callback, gpointer user_data);
GPtrArray *nolphin_archive_list_finish (GAsyncResult *result, GError **error);

/* Dateien/Ordner (@sources: GList von GFile) in den Ordner @target_dir
 * innerhalb des Archivs legen (NULL = oberste Ebene). Gleichnamige
 * Einträge werden ersetzt. */
void nolphin_archive_add_files_async (GFile *archive, GList *sources, const gchar *target_dir,
				      GCancellable *cancellable,
				      GAsyncReadyCallback callback, gpointer user_data);

/* Den Eintrag @entry_path durch @source ersetzen (Name im Archiv bleibt). */
void nolphin_archive_replace_entry_async (GFile *archive, const gchar *entry_path, GFile *source,
					  GCancellable *cancellable,
					  GAsyncReadyCallback callback, gpointer user_data);

/* Einträge entfernen / nach @destination entpacken (@entry_paths: GList von
 * gchar*; Ordner mit abschließendem "/", dann samt Inhalt). */
void nolphin_archive_remove_entries_async  (GFile *archive, GList *entry_paths,
					    GCancellable *cancellable,
					    GAsyncReadyCallback callback, gpointer user_data);
void nolphin_archive_extract_entries_async (GFile *archive, GList *entry_paths, GFile *destination,
					    GCancellable *cancellable,
					    GAsyncReadyCallback callback, gpointer user_data);

/* Gemeinsames _finish für add/replace/remove/extract. */
gboolean nolphin_archive_manage_finish (GAsyncResult *result, GError **error);

G_END_DECLS

#endif /* NOLPHIN_ARCHIVE_MANAGE_H */
