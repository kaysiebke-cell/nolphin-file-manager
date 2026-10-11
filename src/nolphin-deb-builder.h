/* nolphin-deb-builder.h
 *
 * Baut ein Debian-Binärpaket (.deb) komplett selbst, ohne dpkg-deb, dpkg,
 * apt oder ein anderes externes Werkzeug aufzurufen. Ein .deb ist ein
 * ar-Archiv aus drei Mitgliedern (debian-binary, control.tar.gz,
 * data.tar.gz) - diese Datei erzeugt sie direkt byteweise.
 */

#ifndef NOLPHIN_DEB_BUILDER_H
#define NOLPHIN_DEB_BUILDER_H

#include <glib.h>

G_BEGIN_DECLS

typedef struct {
	gchar    *source_path;      /* Datei/Symlink auf der Festplatte */
	gchar    *install_path;     /* absoluter Zielpfad im installierten System,
				     * z. B. "/usr/bin/meinprogramm" */
	gboolean  force_executable; /* zusätzlich zu den Quell-Dateirechten +x setzen */
} NolphinDebBuilderFile;

NolphinDebBuilderFile *nolphin_deb_builder_file_new  (const gchar *source_path,
						      const gchar *install_path,
						      gboolean     force_executable);
void                   nolphin_deb_builder_file_free (NolphinDebBuilderFile *file);

/* section, priority, depends und homepage sind optional - NULL oder "" lässt
 * das jeweilige control-Feld einfach weg. */
gboolean nolphin_deb_builder_create (const gchar  *output_deb_path,
				     const gchar  *package_name,
				     const gchar  *version,
				     const gchar  *architecture,
				     const gchar  *maintainer,
				     const gchar  *description,
				     const gchar  *section,
				     const gchar  *priority,
				     const gchar  *depends,
				     const gchar  *homepage,
				     GList        *files /* element-type NolphinDebBuilderFile* */,
				     GError      **error);

G_END_DECLS

#endif /* NOLPHIN_DEB_BUILDER_H */
