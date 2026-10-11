/* Werkzeug (kein automatischer Test): zeigt für jedes angegebene Symbolthema,
 * welche der xsi-Symbole auch NACH dem Ersatz (nolphin-icon-fallback.c) fehlen.
 * Das Ergebnis hängt von den auf dem Rechner installierten Themen ab.
 *
 * Aufruf: icon-theme-coverage <Thema> [<Thema> ...]
 * Beispiel: icon-theme-coverage Adwaita Yaru Papirus Mint-Y
 *
 * Mit der Umgebungsvariable NOLPHIN_COVERAGE_ICONS=<Ordner> wird nur dieser
 * Symbolordner durchsucht (statt der Systemordner). So lässt sich der Fall
 * "xapp-symbolic-icons ist nicht installiert" nachstellen: Ordner mit Verweisen
 * auf alle Themen anlegen und die xsi-*-Dateien daraus löschen. */

#include <gtk/gtk.h>
#include <libnolphin-private/nolphin-icon-fallback.h>
#include <string.h>

/* gefunden UND wirklich ladbar (eine kaputte Datei zählt nicht als vorhanden) */
static gboolean
resolves (GtkIconTheme *theme, const gchar *name)
{
	GtkIconInfo *info = gtk_icon_theme_lookup_icon (theme, name, 16, 0);
	GdkPixbuf *pb;
	gboolean ok;

	if (info == NULL) {
		return FALSE;
	}
	pb = gtk_icon_info_load_icon (info, NULL);
	ok = (pb != NULL);
	g_clear_object (&pb);
	g_object_unref (info);
	return ok;
}

int
main (int argc, char **argv)
{
	gint t;

	gtk_init (&argc, &argv);
	if (argc < 2) {
		g_printerr ("Aufruf: %s <Thema> [<Thema> ...]\n", argv[0]);
		return 2;
	}

	for (t = 1; t < argc; t++) {
		GtkIconTheme *theme = gtk_icon_theme_new ();
		const gchar * const *names = nolphin_icon_fallback_get_names ();
		gchar *dir = g_dir_make_tmp ("nolphin-coverage-XXXXXX", NULL);
		GString *missing = g_string_new (NULL);
		guint native = 0, replaced = 0, none = 0, colored = 0, i;

		if (g_getenv ("NOLPHIN_COVERAGE_ICONS") != NULL) {
			const gchar *only[] = { g_getenv ("NOLPHIN_COVERAGE_ICONS"), NULL };
			gtk_icon_theme_set_search_path (theme, only, 1);
		}
		gtk_icon_theme_set_custom_theme (theme, argv[t]);
		for (i = 0; names[i] != NULL; i++) {
			if (resolves (theme, names[i])) {
				native++;
			}
		}
		nolphin_icon_fallback_apply (theme, dir);
		for (i = 0; names[i] != NULL; i++) {
			if (!resolves (theme, names[i])) {
				none++;
				g_string_append_printf (missing, " %s", names[i] + 4);
			}
		}
		replaced = i - native - none;

		/* Ersatz, dessen Ziel kein symbolisches (einfarbiges) Symbol ist */
		for (i = 0; names[i] != NULL; i++) {
			GtkIconInfo *info = gtk_icon_theme_lookup_icon (theme, names[i], 16, 0);
			if (info != NULL) {
				const gchar *file = gtk_icon_info_get_filename (info);
				gchar *target = file != NULL ? g_file_read_link (file, NULL) : NULL;
				if (target != NULL) {
					gchar *base = g_path_get_basename (target);
					if (strstr (base, "symbolic") == NULL) {
						colored++;
						g_string_append_printf (missing, " (%s: nur %s)", names[i] + 4, base);
					}
					g_free (base);
					g_free (target);
				}
				g_object_unref (info);
			}
		}
		g_print ("%-16s von %u: %u selbst, %u Ersatz (davon %u nicht symbolisch), %u fehlen%s\n",
		         argv[t], (guint) g_strv_length ((gchar **) names), native, replaced, colored, none, missing->str);
		g_string_free (missing, TRUE);
		g_free (dir);
		g_object_unref (theme);
	}
	return 0;
}
