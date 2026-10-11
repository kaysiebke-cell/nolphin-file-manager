/* nolphin-deb-builder.c
 *
 * Ein .deb ist kein magisches Format: es ist ein "ar"-Archiv mit genau drei
 * Mitgliedern - "debian-binary", "control.tar.gz" und "data.tar.gz". Diese
 * Datei schreibt alle drei (samt der ustar- und gzip-Container, in denen sie
 * stecken) direkt mit GLib/GIO-Bordmitteln - ohne dpkg-deb, dpkg, apt, tar,
 * gzip oder ar als externen Prozess aufzurufen.
 */

#include <config.h>

#include "nolphin-deb-builder.h"

#include <gio/gio.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <errno.h>
#include <string.h>
#include <time.h>

#define TAR_BLOCK_SIZE 512
#define AR_HEADER_SIZE 60

NolphinDebBuilderFile *
nolphin_deb_builder_file_new (const gchar *source_path, const gchar *install_path, gboolean force_executable)
{
	NolphinDebBuilderFile *file = g_new0 (NolphinDebBuilderFile, 1);
	file->source_path = g_strdup (source_path);
	file->install_path = g_strdup (install_path);
	file->force_executable = force_executable;
	return file;
}

void
nolphin_deb_builder_file_free (NolphinDebBuilderFile *file)
{
	if (file == NULL) {
		return;
	}
	g_free (file->source_path);
	g_free (file->install_path);
	g_free (file);
}

/* ---------------------------------------------------------------------- */
/* ustar-Schreiber                                                        */
/* ---------------------------------------------------------------------- */

static void
tar_set_field (guint8 *header, gsize offset, gsize length, const gchar *value)
{
	gsize value_len = strlen (value);
	gsize copy_len = MIN (value_len, length);

	memset (header + offset, 0, length);
	memcpy (header + offset, value, copy_len);
}

static void
tar_set_octal (guint8 *header, gsize offset, gsize length, guint64 value)
{
	gchar buf[32];

	g_snprintf (buf, sizeof (buf), "%.*llo", (int) (length - 1), (unsigned long long) value);

	memset (header + offset, 0, length);
	memcpy (header + offset, buf, MIN (strlen (buf), length - 1));
}

static gboolean
build_tar_header (guint8 header[TAR_BLOCK_SIZE], const gchar *name, gsize size,
		  guint mode, gchar typeflag, const gchar *linkname, GError **error)
{
	guint32 checksum = 0;
	gchar chksum_buf[8];
	gsize i;

	memset (header, 0, TAR_BLOCK_SIZE);

	tar_set_field (header, 0, 100, name);
	tar_set_octal (header, 100, 8, mode);
	tar_set_octal (header, 108, 8, 0);    /* uid */
	tar_set_octal (header, 116, 8, 0);    /* gid */
	tar_set_octal (header, 124, 12, size);
	tar_set_octal (header, 136, 12, (guint64) time (NULL));
	memset (header + 148, ' ', 8);        /* chksum, vorerst Leerzeichen */
	header[156] = typeflag;
	if (linkname != NULL) {
		tar_set_field (header, 157, 100, linkname);
	}
	memcpy (header + 257, "ustar", 5);
	header[262] = '\0';
	header[263] = '0';
	header[264] = '0';
	tar_set_field (header, 265, 32, "root");
	tar_set_field (header, 297, 32, "root");

	for (i = 0; i < TAR_BLOCK_SIZE; i++) {
		checksum += header[i];
	}

	g_snprintf (chksum_buf, sizeof (chksum_buf), "%06o", checksum);
	memcpy (header + 148, chksum_buf, 6);
	header[154] = '\0';
	header[155] = ' ';

	return TRUE;
}

static gboolean
write_tar_entry_full (GOutputStream *out, const gchar *name, const guint8 *data,
		      gsize size, guint mode, gchar typeflag, const gchar *linkname,
		      GError **error)
{
	guint8 header[TAR_BLOCK_SIZE];

	/* Namen über 100 Zeichen: GNU-Erweiterung "././@LongLink" (Typ 'L'
	 * für den Namen, 'K' für das Symlink-Ziel). dpkg und tar lesen das. */
	if (strlen (name) > 100) {
		gsize len = strlen (name) + 1;

		if (!write_tar_entry_full (out, "././@LongLink", (const guint8 *) name, len, 0, 'L', NULL, error)) {
			return FALSE;
		}
	}
	if (linkname != NULL && strlen (linkname) > 100) {
		gsize len = strlen (linkname) + 1;

		if (!write_tar_entry_full (out, "././@LongLink", (const guint8 *) linkname, len, 0, 'K', NULL, error)) {
			return FALSE;
		}
	}

	if (!build_tar_header (header, name, size, mode, typeflag, linkname, error)) {
		return FALSE;
	}

	if (!g_output_stream_write_all (out, header, TAR_BLOCK_SIZE, NULL, NULL, error)) {
		return FALSE;
	}

	if (size > 0) {
		gsize padding = (TAR_BLOCK_SIZE - (size % TAR_BLOCK_SIZE)) % TAR_BLOCK_SIZE;

		if (!g_output_stream_write_all (out, data, size, NULL, NULL, error)) {
			return FALSE;
		}

		if (padding > 0) {
			guint8 zeros[TAR_BLOCK_SIZE] = { 0 };
			if (!g_output_stream_write_all (out, zeros, padding, NULL, NULL, error)) {
				return FALSE;
			}
		}
	}

	return TRUE;
}

static gboolean
write_tar_entry (GOutputStream *out, const gchar *name, const guint8 *data,
		 gsize size, guint mode, gchar typeflag, GError **error)
{
	return write_tar_entry_full (out, name, data, size, mode, typeflag, NULL, error);
}

static gboolean
write_tar_end (GOutputStream *out, GError **error)
{
	guint8 zeros[TAR_BLOCK_SIZE * 2] = { 0 };
	return g_output_stream_write_all (out, zeros, sizeof (zeros), NULL, NULL, error);
}

/* ---------------------------------------------------------------------- */
/* gzip-Ausgabestrom (GIO-eigener Deflate-Codec, kein externes gzip)      */
/* ---------------------------------------------------------------------- */

static GOutputStream *
open_gzip_output_stream (const gchar *path, GError **error)
{
	GFile *file;
	GFileOutputStream *raw;
	GZlibCompressor *compressor;
	GOutputStream *compressed;

	file = g_file_new_for_path (path);
	raw = g_file_replace (file, NULL, FALSE, G_FILE_CREATE_REPLACE_DESTINATION, NULL, error);
	g_object_unref (file);

	if (raw == NULL) {
		return NULL;
	}

	compressor = g_zlib_compressor_new (G_ZLIB_COMPRESSOR_FORMAT_GZIP, -1);
	compressed = g_converter_output_stream_new (G_OUTPUT_STREAM (raw), G_CONVERTER (compressor));
	g_object_unref (compressor);
	g_object_unref (raw);

	return compressed;
}

/* ---------------------------------------------------------------------- */
/* ar-Container (das äußere .deb-Archiv)                                  */
/* ---------------------------------------------------------------------- */

static gboolean
write_ar_member (FILE *out, const gchar *name, const guint8 *data, gsize size, GError **error)
{
	gchar header[AR_HEADER_SIZE];
	gchar size_str[16];
	gsize name_len = strlen (name);

	memset (header, ' ', AR_HEADER_SIZE);
	memcpy (header, name, MIN (name_len, 16));
	memcpy (header + 16, "0", 1);       /* mtime */
	memcpy (header + 28, "0", 1);       /* uid */
	memcpy (header + 34, "0", 1);       /* gid */
	memcpy (header + 40, "100644", 6);  /* mode */

	g_snprintf (size_str, sizeof (size_str), "%" G_GSIZE_FORMAT, size);
	memcpy (header + 48, size_str, MIN (strlen (size_str), 10));

	header[58] = '`';
	header[59] = '\n';

	if (fwrite (header, 1, AR_HEADER_SIZE, out) != AR_HEADER_SIZE) {
		g_set_error (error, G_IO_ERROR, g_io_error_from_errno (errno),
			    _("Konnte Archiv-Kopfdaten nicht schreiben"));
		return FALSE;
	}

	if (size > 0 && fwrite (data, 1, size, out) != size) {
		g_set_error (error, G_IO_ERROR, g_io_error_from_errno (errno),
			    _("Konnte Archivdaten nicht schreiben"));
		return FALSE;
	}

	if (size % 2 != 0) {
		fputc ('\n', out);
	}

	return TRUE;
}

/* ---------------------------------------------------------------------- */
/* Pfad-Hilfsfunktionen                                                   */
/* ---------------------------------------------------------------------- */

/* Entfernt führende/abschließende "/" - "/usr/bin/" wird zu "usr/bin". */
static gchar *
strip_slashes (const gchar *path)
{
	gchar *trimmed;
	gchar *start;
	gchar *result;
	gsize len;

	trimmed = g_strdup (path != NULL ? path : "");
	g_strstrip (trimmed);

	start = trimmed;
	while (*start == '/') {
		start++;
	}

	result = g_strdup (start);
	g_free (trimmed);

	len = strlen (result);
	while (len > 0 && result[len - 1] == '/') {
		result[len - 1] = '\0';
		len--;
	}

	return result;
}

static gboolean
validate_install_path (const gchar *path, GError **error)
{
	gchar **parts;
	guint i;

	if (path == NULL || path[0] != '/' || path[1] == '\0') {
		g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
			    _("Zielpfad muss ein absoluter Pfad sein: %s"), path != NULL ? path : _("(leer)"));
		return FALSE;
	}

	parts = g_strsplit (path, "/", -1);
	for (i = 0; parts[i] != NULL; i++) {
		if (g_strcmp0 (parts[i], "..") == 0) {
			g_strfreev (parts);
			g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
				    _("Zielpfad darf kein \"..\" enthalten: %s"), path);
			return FALSE;
		}
	}
	g_strfreev (parts);

	return TRUE;
}

/* Sammelt alle Verzeichnis-Vorfahren (ohne führendes "./", ohne
 * abschließendes "/") aller Zielpfade, damit im data.tar für jeden
 * Verzeichnisteil ein eigener Eintrag existiert. */
static GList *
collect_ordered_directories (GList *files)
{
	GHashTable *seen = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	GList *ordered = NULL;
	GList *l;

	for (l = files; l != NULL; l = l->next) {
		NolphinDebBuilderFile *f = l->data;
		gchar *dirname = g_path_get_dirname (f->install_path);
		gchar *norm = strip_slashes (dirname);
		gchar **parts = g_strsplit (norm, "/", -1);
		GString *running = g_string_new (NULL);
		guint i;

		g_free (dirname);

		for (i = 0; parts[i] != NULL; i++) {
			if (parts[i][0] == '\0') {
				continue;
			}
			if (running->len > 0) {
				g_string_append_c (running, '/');
			}
			g_string_append (running, parts[i]);

			if (!g_hash_table_contains (seen, running->str)) {
				gchar *dup = g_strdup (running->str);
				g_hash_table_add (seen, dup);
				ordered = g_list_prepend (ordered, g_strdup (dup));
			}
		}

		g_string_free (running, TRUE);
		g_strfreev (parts);
		g_free (norm);
	}

	g_hash_table_destroy (seen);
	ordered = g_list_reverse (ordered);

	/* Kürzere Pfade (Eltern) müssen vor ihren Unterordnern geschrieben
	 * werden; da jeder Elternpfad ein reines Präfix seines Kindes ist,
	 * reicht eine einfache lexikografische Sortierung. */
	ordered = g_list_sort (ordered, (GCompareFunc) g_strcmp0);

	return ordered;
}

/* ---------------------------------------------------------------------- */

static gboolean
create_from_files (const gchar  *output_deb_path,
			    const gchar  *package_name,
			    const gchar  *version,
			    const gchar  *architecture,
			    const gchar  *maintainer,
			    const gchar  *description,
			    const gchar  *section,
			    const gchar  *priority,
			    const gchar  *depends,
			    const gchar  *homepage,
			    GList        *files,
			    GError      **error)
{
	gchar *tmpdir = NULL;
	gchar *control_tar_path = NULL;
	gchar *data_tar_path = NULL;
	GOutputStream *data_gz = NULL;
	GOutputStream *control_gz = NULL;
	GString *control_text = NULL;
	GString *md5sums_text = NULL;
	GList *dirs = NULL;
	GList *l;
	guint64 installed_size_kb = 0;
	gboolean success = FALSE;
	FILE *out = NULL;
	gchar *control_bytes = NULL;
	gchar *data_bytes = NULL;
	gsize control_len = 0;
	gsize data_len = 0;

	for (l = files; l != NULL; l = l->next) {
		NolphinDebBuilderFile *f = l->data;
		if (!validate_install_path (f->install_path, error)) {
			return FALSE;
		}
	}

	tmpdir = g_dir_make_tmp ("nolphin-deb-XXXXXX", error);
	if (tmpdir == NULL) {
		return FALSE;
	}

	/* --- data.tar.gz: die eigentlichen Dateien --- */

	data_tar_path = g_build_filename (tmpdir, "data.tar.gz", NULL);
	data_gz = open_gzip_output_stream (data_tar_path, error);
	if (data_gz == NULL) {
		goto out;
	}

	if (!write_tar_entry (data_gz, "./", NULL, 0, 0755, '5', error)) {
		goto out;
	}

	dirs = collect_ordered_directories (files);
	for (l = dirs; l != NULL; l = l->next) {
		gchar *dir_name = g_strdup_printf ("./%s/", (gchar *) l->data);
		gboolean ok = write_tar_entry (data_gz, dir_name, NULL, 0, 0755, '5', error);
		g_free (dir_name);
		if (!ok) {
			goto out;
		}
	}

	md5sums_text = g_string_new (NULL);

	for (l = files; l != NULL; l = l->next) {
		NolphinDebBuilderFile *f = l->data;
		gchar *member_name;
		gchar *norm_path;
		gboolean ok;

		norm_path = strip_slashes (f->install_path);
		member_name = g_strdup_printf ("./%s", norm_path);
		g_free (norm_path);

		if (g_file_test (f->source_path, G_FILE_TEST_IS_SYMLINK)) {
			gchar *target = g_file_read_link (f->source_path, error);

			if (target == NULL) {
				g_free (member_name);
				goto out;
			}

			ok = write_tar_entry_full (data_gz, member_name, NULL, 0, 0777, '2', target, error);
			g_free (target);

			if (!ok) {
				g_free (member_name);
				goto out;
			}
		} else {
			gchar *contents = NULL;
			gsize length = 0;
			GStatBuf statbuf;
			guint mode = 0644;
			gchar *checksum;

			if (!g_file_get_contents (f->source_path, &contents, &length, error)) {
				g_free (member_name);
				goto out;
			}

			if (g_stat (f->source_path, &statbuf) == 0) {
				mode = statbuf.st_mode & 0777;
			}
			if (f->force_executable) {
				mode |= 0111;
			}

			ok = write_tar_entry (data_gz, member_name, (guint8 *) contents, length, mode, '0', error);

			if (ok) {
				checksum = g_compute_checksum_for_data (G_CHECKSUM_MD5, (guchar *) contents, length);
				g_string_append_printf (md5sums_text, "%s  %s\n", checksum, member_name + 2 /* "./" abschneiden */);
				g_free (checksum);
				installed_size_kb += (length + 1023) / 1024;
			}

			g_free (contents);

			if (!ok) {
				g_free (member_name);
				goto out;
			}
		}

		g_free (member_name);
	}

	if (!write_tar_end (data_gz, error)) {
		goto out;
	}
	if (!g_output_stream_close (data_gz, NULL, error)) {
		goto out;
	}
	g_clear_object (&data_gz);

	/* --- control.tar.gz: Paketmetadaten --- */

	control_text = g_string_new (NULL);
	g_string_append_printf (control_text, "Package: %s\n", package_name);
	g_string_append_printf (control_text, "Version: %s\n", version);
	if (section != NULL && *section != '\0') {
		g_string_append_printf (control_text, "Section: %s\n", section);
	}
	g_string_append_printf (control_text, "Priority: %s\n",
				(priority != NULL && *priority != '\0') ? priority : "optional");
	g_string_append_printf (control_text, "Architecture: %s\n", architecture);
	if (depends != NULL && *depends != '\0') {
		g_string_append_printf (control_text, "Depends: %s\n", depends);
	}
	if (installed_size_kb > 0) {
		g_string_append_printf (control_text, "Installed-Size: %" G_GUINT64_FORMAT "\n", installed_size_kb);
	}
	if (homepage != NULL && *homepage != '\0') {
		g_string_append_printf (control_text, "Homepage: %s\n", homepage);
	}
	g_string_append_printf (control_text, "Maintainer: %s\n", maintainer);
	g_string_append_printf (control_text, "Description: %s\n",
				(description != NULL && *description != '\0') ? description : package_name);

	control_tar_path = g_build_filename (tmpdir, "control.tar.gz", NULL);
	control_gz = open_gzip_output_stream (control_tar_path, error);
	if (control_gz == NULL) {
		goto out;
	}

	if (!write_tar_entry (control_gz, "./control", (guint8 *) control_text->str, control_text->len, 0644, '0', error)) {
		goto out;
	}

	if (md5sums_text->len > 0) {
		if (!write_tar_entry (control_gz, "./md5sums", (guint8 *) md5sums_text->str, md5sums_text->len, 0644, '0', error)) {
			goto out;
		}
	}

	if (!write_tar_end (control_gz, error)) {
		goto out;
	}
	if (!g_output_stream_close (control_gz, NULL, error)) {
		goto out;
	}
	g_clear_object (&control_gz);

	/* --- ar-Container: debian-binary + control.tar.gz + data.tar.gz --- */

	out = fopen (output_deb_path, "wb");
	if (out == NULL) {
		g_set_error (error, G_IO_ERROR, g_io_error_from_errno (errno),
			    _("Konnte %s nicht anlegen"), output_deb_path);
		goto out;
	}

	if (fwrite ("!<arch>\n", 1, 8, out) != 8) {
		g_set_error (error, G_IO_ERROR, g_io_error_from_errno (errno), _("Fehler beim Schreiben"));
		goto out;
	}

	if (!write_ar_member (out, "debian-binary", (const guint8 *) "2.0\n", 4, error)) {
		goto out;
	}

	if (!g_file_get_contents (control_tar_path, &control_bytes, &control_len, error)) {
		goto out;
	}
	if (!write_ar_member (out, "control.tar.gz", (guint8 *) control_bytes, control_len, error)) {
		goto out;
	}

	if (!g_file_get_contents (data_tar_path, &data_bytes, &data_len, error)) {
		goto out;
	}
	if (!write_ar_member (out, "data.tar.gz", (guint8 *) data_bytes, data_len, error)) {
		goto out;
	}

	success = TRUE;

out:
	if (out != NULL) {
		fclose (out);
	}
	if (!success && output_deb_path != NULL) {
		g_unlink (output_deb_path);
	}

	g_clear_object (&data_gz);
	g_clear_object (&control_gz);

	if (control_text != NULL) {
		g_string_free (control_text, TRUE);
	}
	if (md5sums_text != NULL) {
		g_string_free (md5sums_text, TRUE);
	}

	g_list_free_full (dirs, g_free);

	g_free (control_bytes);
	g_free (data_bytes);

	if (control_tar_path != NULL) {
		g_unlink (control_tar_path);
	}
	if (data_tar_path != NULL) {
		g_unlink (data_tar_path);
	}
	if (tmpdir != NULL) {
		g_rmdir (tmpdir);
	}
	g_free (control_tar_path);
	g_free (data_tar_path);
	g_free (tmpdir);

	return success;
}

/* Ein Ordner als Quelle wird rekursiv in einzelne Paketdateien aufgelöst.
 * Dateirechte kommen aus dem Dateisystem; Symlinks (auch auf Ordner) bleiben
 * Symlinks und werden nicht verfolgt. */
static void
expand_directory (GList **out_list, const gchar *source_dir, const gchar *install_dir)
{
	GDir *dir = g_dir_open (source_dir, 0, NULL);
	const gchar *name;

	if (dir == NULL) {
		return;
	}

	while ((name = g_dir_read_name (dir)) != NULL) {
		gchar *src = g_build_filename (source_dir, name, NULL);
		gchar *dst = g_strdup_printf ("%s/%s", install_dir, name);

		if (g_file_test (src, G_FILE_TEST_IS_DIR) && !g_file_test (src, G_FILE_TEST_IS_SYMLINK)) {
			expand_directory (out_list, src, dst);
		} else {
			*out_list = g_list_prepend (*out_list, nolphin_deb_builder_file_new (src, dst, FALSE));
		}

		g_free (src);
		g_free (dst);
	}

	g_dir_close (dir);
}

gboolean
nolphin_deb_builder_create (const gchar  *output_deb_path,
			    const gchar  *package_name,
			    const gchar  *version,
			    const gchar  *architecture,
			    const gchar  *maintainer,
			    const gchar  *description,
			    const gchar  *section,
			    const gchar  *priority,
			    const gchar  *depends,
			    const gchar  *homepage,
			    GList        *files,
			    GError      **error)
{
	GList *expanded = NULL;
	GList *l;
	gboolean result;

	for (l = files; l != NULL; l = l->next) {
		NolphinDebBuilderFile *f = l->data;

		if (g_file_test (f->source_path, G_FILE_TEST_IS_DIR) &&
		    !g_file_test (f->source_path, G_FILE_TEST_IS_SYMLINK)) {
			gchar *install_dir = strip_slashes (f->install_path);
			gchar *with_slash = g_strdup_printf ("/%s", install_dir);

			expand_directory (&expanded, f->source_path, with_slash);
			g_free (with_slash);
			g_free (install_dir);
		} else {
			expanded = g_list_prepend (expanded,
						   nolphin_deb_builder_file_new (f->source_path, f->install_path, f->force_executable));
		}
	}
	expanded = g_list_reverse (expanded);

	result = create_from_files (output_deb_path, package_name, version, architecture, maintainer,
				    description, section, priority, depends, homepage, expanded, error);

	g_list_free_full (expanded, (GDestroyNotify) nolphin_deb_builder_file_free);
	return result;
}
