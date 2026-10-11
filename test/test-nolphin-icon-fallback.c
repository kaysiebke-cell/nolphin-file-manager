/* Prüft nolphin-icon-fallback.c mit einem echten (kleinen) Symbolthema in einem
 * Testordner und einem echten GtkIconTheme: fehlende xsi-Symbole bekommen den
 * Ersatz, vorhandene bleiben unangetastet. Zusätzlich liest der Test den
 * Quellcode und prüft, dass JEDER dort vorkommende xsi-Name in der Liste steht
 * (sonst käme er bei fehlendem Paket ohne Ersatz). Argument: Wurzel der Quellen. */

#include <gtk/gtk.h>
#include <glib/gstdio.h>
#include <libnolphin-private/nolphin-icon-fallback.h>
#include <string.h>
#include <unistd.h>

static gint exit_code = 0;

static void
check (gboolean ok, const gchar *pass, const gchar *fail_msg)
{
	if (ok) {
		g_print ("PASS: %s\n", pass);
	} else {
		g_printerr ("FAIL: %s\n", fail_msg);
		exit_code = 1;
	}
}

static void
write_svg (const gchar *dir, const gchar *name)
{
	gchar *p = g_build_filename (dir, name, NULL);
	g_file_set_contents (p, "<svg xmlns='http://www.w3.org/2000/svg' width='16' height='16'>"
	                        "<rect width='16' height='16' fill='#444444'/></svg>", -1, NULL);
	g_free (p);
}

/* maßgeblich ist die Suche, die auch GtkImage & Co. benutzen */
static gboolean
found (GtkIconTheme *theme, const gchar *name)
{
	GtkIconInfo *info = gtk_icon_theme_lookup_icon (theme, name, 16, 0);
	gboolean ok = (info != NULL);
	if (info != NULL) {
		g_object_unref (info);
	}
	return ok;
}

static gboolean
list_has (const gchar * const *list, const gchar *name)
{
	guint i;
	for (i = 0; list[i] != NULL; i++) {
		if (g_strcmp0 (list[i], name) == 0) {
			return TRUE;
		}
	}
	return FALSE;
}

/* sammelt alle xsi-Namen aus Quelldateien unter @dir */
static void
scan_dir (const gchar *dir, GHashTable *found)
{
	GDir *d = g_dir_open (dir, 0, NULL);
	const gchar *name;

	while (d != NULL && (name = g_dir_read_name (d)) != NULL) {
		gchar *path = g_build_filename (dir, name, NULL);
		if (g_file_test (path, G_FILE_TEST_IS_DIR)) {
			scan_dir (path, found);
		} else if (g_str_has_suffix (name, ".c") || g_str_has_suffix (name, ".h") ||
		           g_str_has_suffix (name, ".xml") || g_str_has_suffix (name, ".glade") ||
		           g_str_has_suffix (name, ".in") || g_str_has_suffix (name, ".ui")) {
			gchar *content = NULL;
			if (g_file_get_contents (path, &content, NULL, NULL)) {
				const gchar *p = content;
				while ((p = strstr (p, "xsi-")) != NULL) {
					const gchar *e = p + 4;
					while (g_ascii_islower (*e) || g_ascii_isdigit (*e) || *e == '-') {
						e++;
					}
					if (e > p + 4 && (size_t)(e - p) > strlen ("-symbolic") && g_str_has_suffix (g_strndup (p, e - p), "-symbolic")) {
						g_hash_table_add (found, g_strndup (p, e - p));
					}
					p = e;
				}
				g_free (content);
			}
		}
		g_free (path);
	}
	if (d != NULL) {
		g_dir_close (d);
	}
}

int
main (int argc, char **argv)
{
	GError *error = NULL;
	gchar *root = g_dir_make_tmp ("nolphin-icon-fallback-test-XXXXXX", &error);
	gchar *icons, *actions, *links;
	GtkIconTheme *theme;
	const gchar *search[2];
	guint created, again;

	gtk_init (&argc, &argv);

	icons = g_build_filename (root, "icons", NULL);
	actions = g_build_filename (icons, "hicolor", "scalable", "actions", NULL);
	links = g_build_filename (root, "links", NULL);
	g_mkdir_with_parents (actions, 0700);
	{
		gchar *idx = g_build_filename (icons, "hicolor", "index.theme", NULL);
		g_file_set_contents (idx, "[Icon Theme]\nName=Test\nComment=Test\nDirectories=scalable/actions\n\n"
		                          "[scalable/actions]\nSize=16\nMinSize=1\nMaxSize=512\nType=Scalable\nContext=Actions\n", -1, NULL);
		g_free (idx);
	}
	write_svg (actions, "edit-copy-symbolic.svg");   /* gleichnamiges Standardsymbol */
	write_svg (actions, "edit-find-symbolic.svg");   /* anderer Name: xsi-search-symbolic */
	write_svg (actions, "xsi-folder-symbolic.svg");  /* xsi-Symbol ist selbst vorhanden */
	{
		/* umfärbbares Pixel-Symbol: doppelte Endung muss erhalten bleiben */
		GdkPixbuf *px = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, 16, 16);
		gchar *pp = g_build_filename (actions, "go-down-symbolic.symbolic.png", NULL);
		gdk_pixbuf_fill (px, 0x000000ff);
		g_assert (gdk_pixbuf_save (px, pp, "png", NULL, NULL));
		g_free (pp);
		g_object_unref (px);
	}

	/* kaputter Verweis im Thema (wie bei manchen installierten Themen) */
	{
		gchar *dangling = g_build_filename (actions, "edit-cut-symbolic.svg", NULL);
		g_assert (symlink ("/gibt/es/nicht/edit-cut-symbolic.svg", dangling) == 0);
		g_free (dangling);
	}

	theme = gtk_icon_theme_new ();
	search[0] = icons;
	search[1] = NULL;
	gtk_icon_theme_set_search_path (theme, search, 1);
	gtk_icon_theme_set_custom_theme (theme, "hicolor");

	check (!found (theme, "xsi-edit-copy-symbolic"),
	       "Ausgangslage: xsi-edit-copy-symbolic fehlt im Thema", "Ausgangslage falsch");

	created = nolphin_icon_fallback_apply (theme, links);
	check (found (theme, "xsi-edit-copy-symbolic"),
	       "fehlendes xsi-edit-copy-symbolic wird durch edit-copy-symbolic ersetzt",
	       "Ersatz für xsi-edit-copy-symbolic fehlt");
	check (found (theme, "xsi-search-symbolic"),
	       "xsi-search-symbolic wird über die Alternative edit-find-symbolic ersetzt",
	       "Ersatz über Alternativnamen fehlt");
	check (!found (theme, "xsi-zoom-in-symbolic"),
	       "ohne passendes Standardsymbol wird nichts vorgetäuscht",
	       "xsi-zoom-in-symbolic gibt es, obwohl kein Standardsymbol da ist");
	{
		gchar *own = g_build_filename (links, "xsi-folder-symbolic.svg", NULL);
		check (!g_file_test (own, G_FILE_TEST_EXISTS),
		       "ein vorhandenes xsi-Symbol bekommt keinen Verweis (wird nicht ersetzt)",
		       "vorhandenes xsi-Symbol wurde überschrieben");
		g_free (own);
	}
	{
		GtkIconInfo *info = gtk_icon_theme_lookup_icon (theme, "xsi-edit-copy-symbolic", 16, 0);
		GdkPixbuf *pb = info != NULL ? gtk_icon_info_load_icon (info, NULL) : NULL;
		check (pb != NULL && gdk_pixbuf_get_width (pb) > 0,
		       "das Ersatzsymbol lässt sich wirklich laden", "Ersatzsymbol nicht ladbar");
		g_clear_object (&pb);
		if (info != NULL) {
			g_object_unref (info);
		}
	}

	{
		gchar *pngl = g_build_filename (links, "xsi-go-down-symbolic.symbolic.png", NULL);
		check (g_file_test (pngl, G_FILE_TEST_EXISTS) && found (theme, "xsi-go-down-symbolic"),
		       "bei .symbolic.png bleibt die doppelte Endung erhalten (GTK erkennt es als symbolisch)",
		       "Verweis auf .symbolic.png hat die falsche Endung");
		g_free (pngl);
	}
	{
		/* Symbole, die GTK selbst eingebaut hat (Ressource, keine Datei): Thema ohne
		 * Dateien, nur mit dem, was in GTK steckt. */
		GtkIconTheme *bare = gtk_icon_theme_new ();
		gchar *empty = g_build_filename (root, "leer", NULL);
		gchar *links2 = g_build_filename (root, "links2", NULL);
		const gchar *only[2];
		g_mkdir_with_parents (empty, 0700);
		only[0] = empty;
		only[1] = NULL;
		gtk_icon_theme_set_search_path (bare, only, 1);

		if (found (bare, "pan-end-symbolic")) {
			GtkIconInfo *info;
			GdkPixbuf *pb;
			gchar *res = g_build_filename (links2, "xsi-pan-end-symbolic.symbolic.png", NULL);

			nolphin_icon_fallback_apply (bare, links2);
			info = gtk_icon_theme_lookup_icon (bare, "xsi-pan-end-symbolic", 16, 0);
			pb = info != NULL ? gtk_icon_info_load_icon (info, NULL) : NULL;
			check (g_file_test (res, G_FILE_TEST_IS_REGULAR) && pb != NULL,
			       "ein in GTK eingebautes Symbol wird als echte Datei kopiert und ist ladbar",
			       "eingebautes GTK-Symbol wird nicht ersetzt oder ist nicht ladbar");
			g_clear_object (&pb);
			if (info != NULL) {
				g_object_unref (info);
			}
			g_free (res);
		} else {
			g_print ("SKIP: dieses GTK hat pan-end-symbolic nicht eingebaut\n");
		}
		g_free (empty);
		g_free (links2);
		g_object_unref (bare);
	}
	{
		gchar *cut = g_build_filename (links, "xsi-edit-cut-symbolic.svg", NULL);
		check (!g_file_test (cut, G_FILE_TEST_IS_SYMLINK) && !found (theme, "xsi-edit-cut-symbolic"),
		       "ein kaputter Verweis im Thema wird nicht als Ersatz benutzt",
		       "kaputtes Standardsymbol wurde als Ersatz verlinkt");
		g_free (cut);
	}
	again = nolphin_icon_fallback_apply (theme, links);
	check (again == created && created >= 5,
	       "zweiter Aufruf liefert dasselbe (5 Ersatz-Verweise: edit-copy, edit-find, search, saved-search, use-regex)",
	       "zweiter Aufruf ändert das Ergebnis");

	/* --- jeder im Quellcode benutzte xsi-Name steht in der Liste --- */
	if (argc > 1) {
		GHashTable *found = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
		const gchar *dirs[] = { "src", "libnolphin-private", "eel", "gresources", "data", "libnolphin-extension", NULL };
		GHashTableIter it;
		gpointer key;
		gint i, missing = 0;

		for (i = 0; dirs[i] != NULL; i++) {
			gchar *d = g_build_filename (argv[1], dirs[i], NULL);
			scan_dir (d, found);
			g_free (d);
		}
		g_hash_table_iter_init (&it, found);
		while (g_hash_table_iter_next (&it, &key, NULL)) {
			if (!list_has (nolphin_icon_fallback_get_names (), key)) {
				g_printerr ("  nicht in der Liste: %s\n", (const gchar *) key);
				missing++;
			}
		}
		check (g_hash_table_size (found) > 50 && missing == 0,
		       "alle im Quellcode vorkommenden xsi-Namen stehen in der Ersatz-Liste",
		       "xsi-Namen im Quellcode fehlen in der Ersatz-Liste");
	}

	if (exit_code == 0) {
		g_print ("All icon-fallback tests passed.\n");
	}
	return exit_code;
}
