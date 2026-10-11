/* Prüft nolphin_global_preferences_pick_schema() gegen die auf dem Rechner
 * tatsächlich installierten Schemas (kein Nachbau): Das Schema des laufenden
 * Desktops hat Vorrang, auch wenn weitere installiert sind. Fehlt ein Schema
 * auf diesem Rechner, wird der Fall übersprungen (SKIP) und nicht als
 * bestanden gezählt. */

#include <gio/gio.h>
#include <libnolphin-private/nolphin-global-preferences.h>

static gint exit_code = 0;
static gint checked = 0;

static gboolean
installed (const gchar *id)
{
	GSettingsSchemaSource *src = g_settings_schema_source_get_default ();
	GSettingsSchema *s = src != NULL ? g_settings_schema_source_lookup (src, id, TRUE) : NULL;
	gboolean ok = (s != NULL);
	if (s != NULL) {
		g_settings_schema_unref (s);
	}
	return ok;
}

static void
expect (const gchar *text, const gchar *got, const gchar *want)
{
	checked++;
	if (g_strcmp0 (got, want) == 0) {
		g_print ("PASS: %s\n", text);
	} else {
		g_printerr ("FAIL: %s (erhalten %s, erwartet %s)\n", text,
		            got != NULL ? got : "NULL", want != NULL ? want : "NULL");
		exit_code = 1;
	}
}

int
main (void)
{
	static const char * const terminal[] = {
		"org.cinnamon.desktop.default-applications.terminal",
		"org.mate.applications-terminal",
		"org.gnome.desktop.default-applications.terminal", NULL };
	static const char * const exec_key[] = { "exec", NULL };
	static const char * const nothing[] = { "org.gibt.es.nicht", NULL };
	static const char * const iface[] = { "org.cinnamon.desktop.interface", "org.gnome.desktop.interface", NULL };
	static const char * const font_key[] = { "monospace-font-name", NULL };

	if (installed (terminal[0]) && installed (terminal[1])) {
		expect ("MATE: Schema von MATE vor Cinnamon, obwohl beide installiert sind",
		        nolphin_global_preferences_pick_schema (terminal, exec_key, "MATE"), terminal[1]);
		expect ("X-Cinnamon: Schema von Cinnamon",
		        nolphin_global_preferences_pick_schema (terminal, exec_key, "X-Cinnamon"), terminal[0]);
		expect ("XFCE (kein eigenes Schema): bisherige Reihenfolge, Cinnamon zuerst",
		        nolphin_global_preferences_pick_schema (terminal, exec_key, "XFCE"), terminal[0]);
		expect ("ohne XDG_CURRENT_DESKTOP: bisherige Reihenfolge",
		        nolphin_global_preferences_pick_schema (terminal, exec_key, NULL), terminal[0]);
		expect ("mehrteiliger Wert 'ubuntu:MATE' wird als MATE erkannt",
		        nolphin_global_preferences_pick_schema (terminal, exec_key, "ubuntu:MATE"), terminal[1]);
	} else {
		g_print ("SKIP: Cinnamon- und MATE-Terminal-Schema nicht beide installiert\n");
	}

	expect ("kein Kandidat installiert: NULL (dann gilt das Ersatz-Schema)",
	        nolphin_global_preferences_pick_schema (nothing, exec_key, "MATE"), NULL);

	if (installed (iface[0]) && installed (iface[1])) {
		/* Cinnamon hat den Schlüssel nicht: dieses Schema darf nicht gewählt werden */
		expect ("Schema ohne den benötigten Schlüssel wird übergangen",
		        nolphin_global_preferences_pick_schema (iface, font_key, "X-Cinnamon"), iface[1]);
	} else {
		g_print ("SKIP: Interface-Schemas von Cinnamon und GNOME nicht beide installiert\n");
	}

	if (checked == 0) {
		g_print ("SKIP: keine Prüfung möglich auf diesem Rechner\n");
		return 77;
	}
	if (exit_code == 0) {
		g_print ("All settings-schema tests passed.\n");
	}
	return exit_code;
}
