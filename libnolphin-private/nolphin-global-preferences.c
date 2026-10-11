/* -*- Mode: C; indent-tabs-mode: f; c-basic-offset: 4; tab-width: 4 -*- */

/* nolphin-global-preferences.c - Nolphin specific preference keys and
                                   functions.

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

   Authors: Ramiro Estrugo <ramiro@eazel.com>
*/

#include <config.h>
#include "nolphin-global-preferences.h"

#include "nolphin-file-utilities.h"
#include "nolphin-file.h"
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-string.h>
#include <eel/eel-debug.h>
#include <glib/gi18n.h>
#include <gio/gdesktopappinfo.h>

#define DEBUG_FLAG NOLPHIN_DEBUG_PREFERENCES
#include <string.h>
#include <time.h>
#include <libnolphin-private/nolphin-debug.h>

GSettings *nolphin_preferences;
GSettings *nolphin_icon_view_preferences;
GSettings *nolphin_list_view_preferences;
GSettings *nolphin_compact_view_preferences;
GSettings *nolphin_desktop_preferences;
GSettings *nolphin_tree_sidebar_preferences;
GSettings *nolphin_window_state;
GSettings *gtk_filechooser_preferences;
GSettings *nolphin_plugin_preferences;
GSettings *nolphin_menu_config_preferences;
GSettings *nolphin_search_preferences;
GSettings *gnome_lockdown_preferences;
GSettings *gnome_background_preferences;
GSettings *gnome_media_handling_preferences;
GSettings *gnome_terminal_preferences;
GSettings *cinnamon_privacy_preferences;
GSettings *cinnamon_interface_preferences;
GSettings *gnome_interface_preferences;

GTimeZone      *prefs_current_timezone;
gboolean        prefs_current_24h_time_format;
NolphinDateFormat  prefs_current_date_format;

GTimer    *nolphin_startup_timer;

static gboolean ignore_view_metadata = FALSE;
static gboolean inherit_folder_view_preference = FALSE;
static gboolean inherit_show_thumbnails_preference = FALSE;
static int      size_prefixes_preference = 0;

static gchar **file_roller_mimetypes = NULL;


GFileMonitor *tz_mon;

/*
 * Public functions
 */
char *
nolphin_global_preferences_get_default_folder_viewer_preference_as_iid (void)
{
	int preference_value;
	const char *viewer_iid;

	preference_value =
		g_settings_get_enum (nolphin_preferences, NOLPHIN_PREFERENCES_DEFAULT_FOLDER_VIEWER);

	if (preference_value == NOLPHIN_DEFAULT_FOLDER_VIEWER_LIST_VIEW) {
		viewer_iid = NOLPHIN_LIST_VIEW_IID;
	} else if (preference_value == NOLPHIN_DEFAULT_FOLDER_VIEWER_COMPACT_VIEW) {
		viewer_iid = NOLPHIN_COMPACT_VIEW_IID;
	} else {
		viewer_iid = NOLPHIN_ICON_VIEW_IID;
	}

	return g_strdup (viewer_iid);
}

gboolean
nolphin_global_preferences_get_inherit_folder_viewer_preference (void)
{
    return inherit_folder_view_preference;
}

gboolean
nolphin_global_preferences_get_ignore_view_metadata (void)
{
    return ignore_view_metadata;
}

gboolean
nolphin_global_preferences_get_inherit_show_thumbnails_preference (void)
{
    return inherit_show_thumbnails_preference;
}

int
nolphin_global_preferences_get_size_prefix_preference (void)
{
    switch (size_prefixes_preference) {
        case 0: // base-10
            return G_FORMAT_SIZE_DEFAULT;
        case 1: // base-10 full
            return G_FORMAT_SIZE_DEFAULT |                           G_FORMAT_SIZE_LONG_FORMAT;
        case 2: // base-2
            return G_FORMAT_SIZE_DEFAULT | G_FORMAT_SIZE_IEC_UNITS;
        case 3: // base-2 full
            return G_FORMAT_SIZE_DEFAULT | G_FORMAT_SIZE_IEC_UNITS | G_FORMAT_SIZE_LONG_FORMAT;
    }

    return 0;
}

char *
nolphin_global_preferences_get_desktop_iid (void)
{
    gboolean use_grid;
    const char *viewer_iid;

    use_grid = g_settings_get_boolean (nolphin_desktop_preferences, NOLPHIN_PREFERENCES_USE_DESKTOP_GRID);

    if (use_grid) {
        viewer_iid = NOLPHIN_DESKTOP_ICON_GRID_VIEW_IID;
    } else {
        viewer_iid = NOLPHIN_DESKTOP_ICON_VIEW_IID;
    }

    return g_strdup (viewer_iid);
}

gint
nolphin_global_preferences_get_tooltip_flags (void)
{
    NolphinFileTooltipFlags flags = NOLPHIN_FILE_TOOLTIP_FLAGS_NONE;

    if (g_settings_get_boolean (nolphin_preferences, NOLPHIN_PREFERENCES_TOOLTIP_FILE_TYPE))
        flags |= NOLPHIN_FILE_TOOLTIP_FLAGS_FILE_TYPE;
    if (g_settings_get_boolean (nolphin_preferences, NOLPHIN_PREFERENCES_TOOLTIP_MOD_DATE))
        flags |= NOLPHIN_FILE_TOOLTIP_FLAGS_MOD_DATE;
    if (g_settings_get_boolean (nolphin_preferences, NOLPHIN_PREFERENCES_TOOLTIP_ACCESS_DATE))
        flags |= NOLPHIN_FILE_TOOLTIP_FLAGS_ACCESS_DATE;
    if (g_settings_get_boolean (nolphin_preferences, NOLPHIN_PREFERENCES_TOOLTIP_FULL_PATH))
        flags |= NOLPHIN_FILE_TOOLTIP_FLAGS_PATH;
    if (g_settings_get_boolean (nolphin_preferences, NOLPHIN_PREFERENCES_TOOLTIP_CREATED_DATE))
        flags |= NOLPHIN_FILE_TOOLTIP_FLAGS_CREATED_DATE;
    return flags;
}

gboolean
nolphin_global_preferences_should_load_plugin (const gchar *name, const gchar *key)
{
    gchar **disabled_list = g_settings_get_strv (nolphin_plugin_preferences, key);

    gboolean ret = TRUE;
    guint i = 0;

    for (i = 0; i < g_strv_length (disabled_list); i++) {
        if (g_strcmp0 (disabled_list[i], name) == 0)
            ret = FALSE;
    }

    g_strfreev (disabled_list);
    return ret;
}

static void
boolean_changed_cb (GSettings *settings,
                    gchar     *key,
                    gboolean  *user_data)
{
    *user_data = g_settings_get_boolean (settings, key);
}

static void
enum_changed_cb (GSettings *settings,
                 gchar     *key,
                 int       *user_data)
{
    *user_data = g_settings_get_enum (settings, key);
}

static void
setup_cached_pref_keys (void)
{
    g_signal_connect (nolphin_preferences,
                      "changed::" NOLPHIN_PREFERENCES_IGNORE_VIEW_METADATA,
                      G_CALLBACK (boolean_changed_cb), &ignore_view_metadata);

    boolean_changed_cb (nolphin_preferences, NOLPHIN_PREFERENCES_IGNORE_VIEW_METADATA, &ignore_view_metadata);

    g_signal_connect (nolphin_preferences,
                      "changed::" NOLPHIN_PREFERENCES_INHERIT_FOLDER_VIEWER,
                      G_CALLBACK (boolean_changed_cb), &inherit_folder_view_preference);

    boolean_changed_cb (nolphin_preferences, NOLPHIN_PREFERENCES_INHERIT_FOLDER_VIEWER, &inherit_folder_view_preference);

    g_signal_connect (nolphin_preferences,
                      "changed::" NOLPHIN_PREFERENCES_INHERIT_SHOW_THUMBNAILS,
                      G_CALLBACK (boolean_changed_cb), &inherit_show_thumbnails_preference);

    boolean_changed_cb (nolphin_preferences, NOLPHIN_PREFERENCES_INHERIT_SHOW_THUMBNAILS, &inherit_show_thumbnails_preference);

    g_signal_connect (nolphin_preferences,
                      "changed::" NOLPHIN_PREFERENCES_SIZE_PREFIXES,
                      G_CALLBACK (enum_changed_cb), &size_prefixes_preference);

    enum_changed_cb (nolphin_preferences, NOLPHIN_PREFERENCES_SIZE_PREFIXES, &size_prefixes_preference);
}


gchar **
nolphin_global_preferences_get_fileroller_mimetypes (void)
{
    static gsize once_init = 0;

    if (g_once_init_enter (&once_init)) {
        if (nolphin_is_file_roller_installed ()) {
            GAppInfo *app_info;
            gchar ***results;
            gchar **result;
            gint i;

            results = g_desktop_app_info_search ("file-roller");

            if (results != NULL && results[0] != NULL) {
                const gchar *best;

                best = results[0][0];

                app_info = G_APP_INFO (g_desktop_app_info_new (best));

                if (app_info) {
                    file_roller_mimetypes = g_strdupv ((gchar **) g_app_info_get_supported_types (app_info));
                    g_object_unref (app_info);
                }

                if (app_info == NULL) {
                    g_warning ("Unable to retrieve list of file-roller mimetypes");
                }

                i = 0;
                result = results[i];

                while (result != NULL) {
                    g_strfreev (result);
                    result = results[++i];
                }

                g_free (results);
            }
        }

        g_once_init_leave (&once_init, 1);
    }

    return file_roller_mimetypes;
}

gchar *
nolphin_global_preferences_get_mono_system_font (void)
{
    return g_settings_get_string (gnome_interface_preferences, NOLPHIN_PREFERENCES_MONO_FONT_NAME);
}

static gint
sort_shortest_first (gconstpointer a, gconstpointer b)
{
    return strlen ((const gchar *) a) > strlen((const gchar *) b);
}

static GList *mono_families = NULL;

gchar *
nolphin_global_preferences_get_mono_font_family_match (const gchar *in_family_name)
{
    static gsize once_init = 0;
    DEBUG ("Looking up monospaced version of '%s'", in_family_name);
    // Create a static list of available monospace family names -
    //
    // FreeMono
    // Noto Mono
    // Tlwg Mono
    // ...
    // ...

    if (g_once_init_enter (&once_init)) {
        DEBUG ("First run: initialize");

        PangoFontMap *map = pango_cairo_font_map_get_default ();
        PangoFontFamily **families;
        gint n_families, i;

        pango_font_map_list_families (map, &families, &n_families);

        for (i = 0; i < n_families; i++) {
            PangoFontFamily *family = families[i];

            // should be pango_font_family_is_monospace (family)) but Fonts lie.
            if (g_strstr_len (pango_font_family_get_name (family), -1, "Mono")) {
                mono_families = g_list_prepend (mono_families, g_strdup (pango_font_family_get_name (family)));
            }
        }

        g_free (families);

        mono_families = g_list_sort (mono_families, (GCompareFunc) sort_shortest_first);
        GList *l;

        if (DEBUGGING) {
            for (l = mono_families; l != NULL; l = l->next) {
                DEBUG ("Added '%s'", (gchar *) l->data);
            }
        }

        g_once_init_leave (&once_init, 1);
    }

    // Split the passed-in font string into an array ( like ["Liberation", "Sans" "Regular"] )
    gchar **split_in_family_name = g_strsplit (in_family_name, " ", -1);
    GList *tmp = g_list_copy (mono_families);
    GList *l;

    const gchar *best = NULL;
    gint best_count = 0;

    l = tmp;

    // Work thru the list of mono font strings
    do {
        gint current_count = 0, i;
        const gchar *iter_family_name = (const gchar *) l->data;

        // Split them the same way ( ["Noto", "Sans", "Mono"] )
        gchar **split_iter_family_name = g_strsplit (iter_family_name, " ", -1);

        DEBUG ("  Comparing %s with %s", in_family_name, iter_family_name);

        // Now see how many parts split_iter_family_name and split_n_family have in common.
        for (i = 0; i < g_strv_length (split_iter_family_name); i++) {
            if (g_strv_contains ((const gchar * const*) split_in_family_name, split_iter_family_name[i])) {
                current_count++;
                DEBUG ("    Matched %s to %s (%d segments)", split_iter_family_name[i], in_family_name,  current_count);
            } else {
                if (i == 0) { // If you can't get the first segment correct, get  removed. This avoids
                              // Liberation Sans Regular getting  Noto Sans Mono instead of Liberation Sans Mono.
                              // In that case, the scores would both be 1, but it's obvious the Noto should be discarded.
                    l = tmp = g_list_remove (tmp, iter_family_name);
                    DEBUG ("    Discarded %s\n", iter_family_name);
                    break;
                }
            }
        }
        // Is this score better than the last? (more segments matched) - save it.
        if (current_count > best_count)  {
            best_count = current_count;
            best = iter_family_name;
            DEBUG ("  New winner: %s with %d matches", best, best_count);
        }

        g_strfreev(split_iter_family_name);
        l = l->next;
    } while (l != NULL);

    g_list_free (tmp);
    g_strfreev (split_in_family_name);

    // We return the best match, or Monospace if there were none - this is the default monospace font Pango uses
    // if you use <tt></tt> markup.
    if (!best) {
        DEBUG ("Match not found, falling back to 'Monospace'");
        if (g_strstr_len (in_family_name, -1, "Bold")) {
            best = "Monospace Bold";
        }
        else if (g_strstr_len (in_family_name, -1, "Italic")) {
            best = "Monospace Italic";
        }
        else if (g_strstr_len (in_family_name, -1, "Bold Italic")) {
            best = "Monospace Bold Italic";
        }
        else {
            best = "Monospace";
        }
    }

    DEBUG ("Finished: '%s' ---->   '%s'", in_family_name, best);
    return g_strdup (best);
}


/* FALSE, wenn die Uhr-Einstellung (clock-use-24h) nicht vom Desktop kommt. */
static gboolean clock_settings_native = TRUE;

/* Ohne Uhr-Einstellung des Desktops (z. B. XFCE) richtet sich das
 * Uhrzeitformat nach der Systemsprache: hat sie kein AM/PM, gilt 24 Stunden. */
static gboolean
locale_uses_24h_clock (void)
{
    char buf[16] = "";
    time_t t = 0;
    struct tm tm_value;

    if (localtime_r (&t, &tm_value) == NULL) {
        return TRUE;
    }
    strftime (buf, sizeof (buf), "%p", &tm_value);
    return buf[0] == '\0';
}

static void
on_time_data_changed (gpointer user_data)
{
    prefs_current_date_format = g_settings_get_enum (nolphin_preferences, NOLPHIN_PREFERENCES_DATE_FORMAT);
    prefs_current_24h_time_format = clock_settings_native
        ? g_settings_get_boolean (cinnamon_interface_preferences, "clock-use-24h")
        : locale_uses_24h_clock ();

    if (prefs_current_timezone != NULL) {
        g_time_zone_unref (prefs_current_timezone);
    }

    prefs_current_timezone = g_time_zone_new_local ();
}

static void
setup_cached_time_data (void)
{
    GFile *tz;

    prefs_current_timezone = NULL;

    g_signal_connect_swapped (nolphin_preferences,
                              "changed::" NOLPHIN_PREFERENCES_DATE_FORMAT,
                              G_CALLBACK (on_time_data_changed), NULL);

    g_signal_connect_swapped (cinnamon_interface_preferences,
                              "changed::clock-use-24h",
                              G_CALLBACK (on_time_data_changed), NULL);


    tz = g_file_new_for_path ("/etc/localtime");

    tz_mon = g_file_monitor_file (tz, 0, NULL, NULL);
    g_object_unref (tz);

    g_signal_connect_swapped (tz_mon,
                              "changed",
                              G_CALLBACK (on_time_data_changed), NULL);

    on_time_data_changed (NULL);
}

/* Ein System-Schema (Cinnamon, MATE, GNOME) wird nur genommen, wenn es
 * installiert ist und alle benoetigten Schluessel hat. g_settings_new()
 * bricht das Programm sonst ab - das darf auf XFCE/MATE ohne
 * Cinnamon-Pakete nicht passieren. Gibt es keinen Treffer, wird das
 * Ersatz-Schema von Nolphin (org.nolphin.compat.*) benutzt. */
static gboolean desktop_settings_native = TRUE;

/* Schema-Vorsilbe des laufenden Desktops (XDG_CURRENT_DESKTOP, z. B. "MATE",
 * "X-Cinnamon", "ubuntu:GNOME"); NULL, wenn kein Desktop mit eigenem Schema
 * erkannt wird (z. B. XFCE). */
static const char *
desktop_schema_prefix (const char *desktop)
{
    gchar **tokens;
    const char *prefix = NULL;
    guint i;

    if (desktop == NULL) {
        return NULL;
    }
    tokens = g_strsplit (desktop, ":", -1);
    for (i = 0; tokens[i] != NULL && prefix == NULL; i++) {
        gchar *lower = g_ascii_strdown (tokens[i], -1);
        if (g_strcmp0 (lower, "mate") == 0) {
            prefix = "org.mate.";
        } else if (strstr (lower, "cinnamon") != NULL) {
            prefix = "org.cinnamon.";
        } else if (g_strcmp0 (lower, "gnome") == 0) {
            prefix = "org.gnome.";
        }
        g_free (lower);
    }
    g_strfreev (tokens);
    return prefix;
}

static gboolean
schema_usable (GSettingsSchemaSource *source, const char *id, const char * const *required_keys)
{
    GSettingsSchema *schema = g_settings_schema_source_lookup (source, id, TRUE);
    gboolean complete = TRUE;
    guint k;

    if (schema == NULL) {
        return FALSE;
    }
    for (k = 0; required_keys[k] != NULL; k++) {
        if (!g_settings_schema_has_key (schema, required_keys[k])) {
            complete = FALSE;
            break;
        }
    }
    g_settings_schema_unref (schema);
    return complete;
}

/* Wählt unter @candidates das Schema, das installiert ist und alle
 * @required_keys hat. Kandidaten des laufenden Desktops (@desktop) kommen
 * zuerst - unter MATE also org.mate.*, auch wenn Cinnamon-Schemas ebenfalls
 * installiert sind -, danach alle in der angegebenen Reihenfolge. */
const char *
nolphin_global_preferences_pick_schema (const char * const *candidates,
                                        const char * const *required_keys,
                                        const char         *desktop)
{
    GSettingsSchemaSource *source = g_settings_schema_source_get_default ();
    const char *prefix = desktop_schema_prefix (desktop);
    guint i;

    if (source == NULL) {
        return NULL;
    }
    if (prefix != NULL) {
        for (i = 0; candidates[i] != NULL; i++) {
            if (g_str_has_prefix (candidates[i], prefix) && schema_usable (source, candidates[i], required_keys)) {
                return candidates[i];
            }
        }
    }
    for (i = 0; candidates[i] != NULL; i++) {
        if (schema_usable (source, candidates[i], required_keys)) {
            return candidates[i];
        }
    }
    return NULL;
}

static GSettings *
new_settings_with_fallback (const char * const *candidates,
                            const char * const *required_keys,
                            const char         *compat_schema,
                            gboolean           *native_out)
{
    const char *chosen = nolphin_global_preferences_pick_schema (candidates, required_keys,
                                                                 g_getenv ("XDG_CURRENT_DESKTOP"));

    if (chosen != NULL) {
        if (native_out != NULL) {
            *native_out = TRUE;
        }
        return g_settings_new (chosen);
    }

    if (native_out != NULL) {
        *native_out = FALSE;
    }
    g_message ("Kein passendes Desktop-Schema (%s ...) gefunden, benutze Nolphin-Ersatz %s",
               candidates[0], compat_schema);
    return g_settings_new (compat_schema);
}

gboolean
nolphin_global_preferences_desktop_settings_native (void)
{
    return desktop_settings_native;
}

void
nolphin_global_preferences_init (void)
{
	static gboolean initialized = FALSE;

	if (initialized) {
		return;
	}

	initialized = TRUE;

	nolphin_preferences = g_settings_new("org.nolphin.preferences");
	nolphin_window_state = g_settings_new("org.nolphin.window-state");
	nolphin_icon_view_preferences = g_settings_new("org.nolphin.icon-view");
	nolphin_list_view_preferences = g_settings_new("org.nolphin.list-view");
	nolphin_compact_view_preferences = g_settings_new("org.nolphin.compact-view");
	nolphin_desktop_preferences = g_settings_new("org.nolphin.desktop");
    /* Some settings such as show hidden files are shared between Nautilus and GTK file chooser */
    gtk_filechooser_preferences = g_settings_new_with_path ("org.gtk.Settings.FileChooser",
                                                            "/org/gtk/settings/file-chooser/");
	nolphin_tree_sidebar_preferences = g_settings_new("org.nolphin.sidebar-panels.tree");
    nolphin_plugin_preferences = g_settings_new("org.nolphin.plugins");
    nolphin_menu_config_preferences = g_settings_new("org.nolphin.preferences.menu-config");
    nolphin_search_preferences = g_settings_new("org.nolphin.search");
    {
        static const char * const lockdown[] = { "org.cinnamon.desktop.lockdown", "org.mate.lockdown", "org.gnome.desktop.lockdown", NULL };
        static const char * const lockdown_keys[] = { "disable-command-line", NULL };
        static const char * const background[] = { "org.cinnamon.desktop.background", "org.mate.background", NULL };
        static const char * const no_keys[] = { NULL };
        static const char * const media[] = { "org.cinnamon.desktop.media-handling", "org.mate.media-handling", "org.gnome.desktop.media-handling", NULL };
        static const char * const media_keys[] = { "automount", "automount-open", "autorun-never", NULL };
        static const char * const terminal[] = { "org.cinnamon.desktop.default-applications.terminal", "org.mate.applications-terminal", "org.gnome.desktop.default-applications.terminal", NULL };
        static const char * const terminal_keys[] = { "exec", NULL };
        static const char * const privacy[] = { "org.cinnamon.desktop.privacy", "org.gnome.desktop.privacy", NULL };
        static const char * const privacy_keys[] = { "remember-recent-files", NULL };
        static const char * const cinn_iface[] = { "org.cinnamon.desktop.interface", NULL };
        static const char * const cinn_iface_keys[] = { "clock-use-24h", NULL };
        /* System mono font */
        static const char * const iface[] = { "org.gnome.desktop.interface", "org.mate.interface", "org.cinnamon.desktop.interface", NULL };
        static const char * const iface_keys[] = { "monospace-font-name", NULL };

        gnome_lockdown_preferences = new_settings_with_fallback (lockdown, lockdown_keys, "org.nolphin.compat.lockdown", NULL);
        gnome_background_preferences = new_settings_with_fallback (background, no_keys, "org.nolphin.compat.background", NULL);
        gnome_media_handling_preferences = new_settings_with_fallback (media, media_keys, "org.nolphin.compat.media-handling", &desktop_settings_native);
        gnome_terminal_preferences = new_settings_with_fallback (terminal, terminal_keys, "org.nolphin.compat.terminal", NULL);
        cinnamon_privacy_preferences = new_settings_with_fallback (privacy, privacy_keys, "org.nolphin.compat.privacy", NULL);
        cinnamon_interface_preferences = new_settings_with_fallback (cinn_iface, cinn_iface_keys, "org.nolphin.compat.interface", &clock_settings_native);
        gnome_interface_preferences = new_settings_with_fallback (iface, iface_keys, "org.nolphin.compat.interface", NULL);
    }

    setup_cached_pref_keys ();
    setup_cached_time_data ();

    eel_debug_call_at_shutdown (nolphin_global_preferences_finalize);
}

void
nolphin_global_preferences_finalize (void)
{
    g_strfreev (file_roller_mimetypes);
    g_list_free_full (mono_families, g_free);

    g_object_unref (tz_mon);

    g_object_unref (nolphin_preferences);
    g_object_unref (nolphin_window_state);
    g_object_unref (nolphin_icon_view_preferences);
    g_object_unref (nolphin_list_view_preferences);
    g_object_unref (nolphin_compact_view_preferences);
    g_object_unref (nolphin_desktop_preferences);
    g_object_unref (nolphin_tree_sidebar_preferences);
    g_object_unref (nolphin_plugin_preferences);
    g_object_unref (nolphin_menu_config_preferences);
    g_object_unref (nolphin_search_preferences);
    g_object_unref (gnome_lockdown_preferences);
    g_object_unref (gnome_background_preferences);
    g_object_unref (gnome_media_handling_preferences);
    g_object_unref (gnome_terminal_preferences);
    g_object_unref (cinnamon_privacy_preferences);
    g_object_unref (cinnamon_interface_preferences);
    g_object_unref (gnome_interface_preferences);
}
