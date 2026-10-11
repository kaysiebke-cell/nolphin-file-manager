/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nolphin-application: main Nolphin application class.
 *
 * Copyright (C) 1999, 2000 Red Hat, Inc.
 * Copyright (C) 2000, 2001 Eazel, Inc.
 * Copyright (C) 2010, Cosimo Cecchi <cosimoc@gnome.org>
 *
 * Nolphin is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nolphin is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Suite 500, MA 02110-1335, USA.
 *
 * Authors: Elliot Lee <sopwith@redhat.com>,
 *          Darin Adler <darin@bentspoon.com>
 *          Cosimo Cecchi <cosimoc@gnome.org>
 *
 */

#include <config.h>

#include "nolphin-application.h"

#include "nolphin-desktop-window.h"

#if (defined(ENABLE_EMPTY_VIEW) && ENABLE_EMPTY_VIEW)
#include "nolphin-empty-view.h"
#endif /* ENABLE_EMPTY_VIEW */

#include "nolphin-freedesktop-dbus.h"
#include "nolphin-icon-view.h"
#include "nolphin-image-properties-page.h"
#include "nolphin-list-view.h"
#include "nolphin-previewer.h"
#include "nolphin-progress-ui-handler.h"
#include "nolphin-self-check-functions.h"
#include "nolphin-window.h"
#include "nolphin-window-bookmarks.h"
#include "nolphin-window-manage-views.h"
#include "nolphin-window-private.h"
#include "nolphin-window-slot.h"
#include "nolphin-statusbar.h"

#include <libnolphin-private/nolphin-dbus-manager.h>
#include <libnolphin-private/nolphin-directory-private.h>
#include <libnolphin-private/nolphin-file-utilities.h>
#include <libnolphin-private/nolphin-file-operations.h>
#include <libnolphin-private/nolphin-global-preferences.h>
#include <libnolphin-private/nolphin-lib-self-check-functions.h>
#include <libnolphin-private/nolphin-module.h>
#include <libnolphin-private/nolphin-signaller.h>
#include <libnolphin-private/nolphin-ui-utilities.h>
#include <libnolphin-private/nolphin-undo-manager.h>
#include <libnolphin-private/nolphin-icon-fallback.h>
#include <libnolphin-private/nolphin-thumbnails.h>
#include <libnolphin-private/nolphin-thumbnailer.h>
#include <libnolphin-private/nolphin-trash-cleanup.h>
#include <libnolphin-private/nolphin-trash-monitor.h>
#include <libnolphin-extension/nolphin-menu-provider.h>

#define DEBUG_FLAG NOLPHIN_DEBUG_APPLICATION
#include <libnolphin-private/nolphin-debug.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <fcntl.h>
#include <errno.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-string.h>
#include <libnolphin-private/nolphin-xapp-compat.h>

#define GNOME_DESKTOP_USE_UNSTABLE_API
#ifdef HAVE_CINNAMON_DESKTOP
#include <libcinnamon-desktop/gnome-desktop-thumbnail.h>
#endif

#define NOLPHIN_ACCEL_MAP_SAVE_DELAY 30

G_DEFINE_TYPE (NolphinApplication, nolphin_application, GTK_TYPE_APPLICATION);

struct _NolphinApplicationPriv {
	NolphinProgressUIHandler *progress_handler;

    gboolean cache_problem;
    gboolean ignore_cache_problem;
};

static NolphinApplication *singleton = NULL;

/* Common startup stuff */

/* The saving of the accelerator map was requested  */
static gboolean save_of_accel_map_requested = FALSE;

static GtkCssProvider *mandatory_css_provider = NULL;

static gboolean
css_provider_load_from_resource (GtkCssProvider *provider,
                     const char     *resource_path,
                     GError        **error)
{
   GBytes  *data;
   gboolean retval;

   data = g_resources_lookup_data (resource_path, 0, error);
   if (!data)
       return FALSE;

   retval = gtk_css_provider_load_from_data (provider,
                         g_bytes_get_data (data, NULL),
                         g_bytes_get_size (data),
                         error);
   g_bytes_unref (data);

   return retval;
}

static gchar *
load_file_contents_from_resource (const char     *resource_path,
                                  GError        **error)
{
   GBytes  *data;
   gchar *retval;

   data = NULL;

   data = g_resources_lookup_data (resource_path, 0, error);

   if (!data) {
       return FALSE;
   }

   retval = g_strdup ((gchar *) g_bytes_get_data (data, NULL));

   g_bytes_unref (data);

   return retval;
}

static void
add_css_provider_at_priority (const gchar *rpath, guint priority)
{
  GtkCssProvider *provider;
  GError *error = NULL;

  provider = gtk_css_provider_new ();

  if (!css_provider_load_from_resource (provider, rpath, &error))
    {
      g_warning ("Failed to load fallback css file: %s", error->message);
      if (error->message != NULL)
        g_error_free (error);
      goto out;
    }

  gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
                                             GTK_STYLE_PROVIDER (provider),
                                             priority);

out:
  g_object_unref (provider);
}

static void
add_fallback_mandatory_css_provider (const gchar *theme_name)
{
    GtkCssProvider *current_provider;
    gchar *css, *init_fallback_css, *final_fallback_css;
    GError *error = NULL;

    current_provider = gtk_css_provider_get_named (theme_name, NULL);

    css = gtk_css_provider_to_string (current_provider);

    init_fallback_css = NULL;

    if (!g_strstr_len (css, -1, "nolphin")) {
        g_warning ("The theme appears to have no nolphin support.  Adding some...");

        init_fallback_css = load_file_contents_from_resource ("/org/nolphin/nolphin-style-fallback-mandatory.css",
                                                              &error);

        if (!init_fallback_css) {
            g_warning ("Failed to load fallback mandatory css file: %s", error->message);
            g_clear_error (&error);

            goto out;
        }
    } else {
        goto out;
    }

    final_fallback_css = NULL;

    /* Our fallback uses @theme_ prefixed names for colors. If the active theme does also, skip
     * to apply */
    if (g_strstr_len (css, -1, "theme_selected_bg_color")) {
        final_fallback_css = g_strdup (init_fallback_css);

        goto apply;
    }

    /* Some themes don't prefix colors with theme_ - remove this from our fallback css */
    if (g_strstr_len (css, -1, "@define-color selected_bg_color")) {
        g_warning ("Replacing theme_selected_bg_color with selected_bg_color");
        final_fallback_css = eel_str_replace_substring (init_fallback_css,
                                                        "@theme_",
                                                        "@");
    } else {
        /* If we can find neither, just bail out */
        goto out;
    }

apply:
    g_free (init_fallback_css);

    mandatory_css_provider = gtk_css_provider_new ();

    gtk_css_provider_load_from_data (mandatory_css_provider,
                                     final_fallback_css,
                                     -1,
                                     &error);

    if (error) {
        g_warning ("Failed to create a fallback provider: %s", error->message);
        g_clear_error (&error);
        goto out;
    }

    gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
                                               GTK_STYLE_PROVIDER (mandatory_css_provider),
                                               GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
out:
    g_free (css);
}

static const char *supported_theme_hints[] = {
        "mint",
        "arc",
        "numix",
        "matcha"
};

static gboolean
is_known_supported_theme (const gchar *theme_name)
{
    gint i;
    gchar *name;
    gboolean ret;

    name = g_utf8_casefold (theme_name, -1);
    ret = FALSE;

    for (i = 0; i < G_N_ELEMENTS (supported_theme_hints); i++) {
        gchar *hint;

        hint = g_utf8_casefold (supported_theme_hints[i], -1);

        if (g_strstr_len (name, -1, hint)) {
            ret = TRUE;
        }

        g_free (hint);

        if (ret) {
            break;
        }
    }

    g_free (name);

    return ret;
}

static void
process_system_theme (GtkSettings *gtk_settings)
{
    gchar *theme_name;

    if (mandatory_css_provider != NULL) {
        gtk_style_context_remove_provider_for_screen (gdk_screen_get_default (), GTK_STYLE_PROVIDER (mandatory_css_provider));
        g_clear_object (&mandatory_css_provider);
    }

    g_object_get (gtk_settings,
                  "gtk-theme-name", &theme_name,
                  NULL);

    if (!is_known_supported_theme (theme_name)) {
        g_warning ("Current gtk theme is not known to have nolphin support (%s) - checking...", theme_name);
        add_fallback_mandatory_css_provider (theme_name);
    }

    gtk_style_context_reset_widgets (gdk_screen_get_default ());
    g_free (theme_name);
}

/* automatische Papierkorb-Bereinigung nach Aufbewahrungsdauer
 * (GSettings "trash-retention-days", 0 = deaktiviert) sowie eine
 * Benachrichtigung, wenn der Papierkorb ein Größenlimit überschreitet
 * (GSettings "trash-size-limit-mb", 0 = deaktiviert). Beides ist per
 * Voreinstellung aus (0), damit ohne ausdrückliche Konfiguration
 * nichts automatisch gelöscht wird. */

static gboolean trash_size_warning_active = FALSE;

static void
trash_size_checked_cb (GObject *source, GAsyncResult *result, gpointer user_data)
{
    GError *error = NULL;
    goffset total_bytes;
    gint limit_mb;

    total_bytes = nolphin_trash_cleanup_get_total_size_finish (result, &error);
    if (error != NULL) {
        g_clear_error (&error);
        return;
    }

    limit_mb = g_settings_get_int (nolphin_preferences, "trash-size-limit-mb");
    if (limit_mb <= 0 || total_bytes < (goffset) limit_mb * 1024 * 1024) {
        trash_size_warning_active = FALSE;
        return;
    }

    if (!trash_size_warning_active) {
        GNotification *notification;
        gchar *body;

        trash_size_warning_active = TRUE;
        body = g_strdup_printf (_("Der Papierkorb belegt mehr als %d MB. Erwäge, ihn zu leeren."), limit_mb);
        notification = g_notification_new (_("Papierkorb wird groß"));
        g_notification_set_body (notification, body);
        g_application_send_notification (G_APPLICATION (nolphin_application_get_singleton ()),
                                         "trash-size-warning", notification);
        g_object_unref (notification);
        g_free (body);
    }
}

static void
trash_cleanup_check_size (void)
{
    nolphin_trash_cleanup_get_total_size_async (NULL, NULL, trash_size_checked_cb, NULL);
}

static void
trash_purge_finished_cb (GObject *source, GAsyncResult *result, gpointer user_data)
{
    GError *error = NULL;
    nolphin_trash_cleanup_purge_expired_finish (result, &error);
    g_clear_error (&error);

    /* Re-check the size regardless of whether anything was purged, so
     * the size-limit warning reflects the current state. */
    trash_cleanup_check_size ();
}

static void
trash_cleanup_run_now (void)
{
    gint retention_days = g_settings_get_int (nolphin_preferences, "trash-retention-days");

    if (retention_days > 0) {
        nolphin_trash_cleanup_purge_expired_async (NULL, retention_days, NULL, trash_purge_finished_cb, NULL);
    } else {
        trash_cleanup_check_size ();
    }
}

static gboolean
trash_cleanup_timeout_cb (gpointer user_data)
{
    trash_cleanup_run_now ();
    return G_SOURCE_CONTINUE;
}

static void
trash_cleanup_trash_state_changed_cb (NolphinTrashMonitor *monitor, gboolean new_state, gpointer user_data)
{
    /* A content change only triggers a size re-check here - the
     * periodic timeout below is what runs the retention-based purge,
     * so a single "file moved to trash" doesn't itself trigger a
     * delete pass. */
    trash_cleanup_check_size ();
}

static void
init_trash_cleanup_scheduler (void)
{
    trash_cleanup_run_now ();
    g_timeout_add_seconds (6 * 60 * 60, trash_cleanup_timeout_cb, NULL);
    g_signal_connect (nolphin_trash_monitor_get (), "trash_state_changed",
                      G_CALLBACK (trash_cleanup_trash_state_changed_cb), NULL);
}

static void
init_icons_and_styles (void)
{
    /* initialize search path for custom icons */
    gtk_icon_theme_append_search_path (gtk_icon_theme_get_default (),
                       NOLPHIN_DATADIR G_DIR_SEPARATOR_S "icons");

    /* Nolphin's own icons are also bundled straight into the binary
     * as a GResource (see gresources/nolphin.gresource.xml) so they
     * are found even when running an uninstalled build - the line
     * above only helps once `ninja install` has actually put files
     * under NOLPHIN_DATADIR/icons, which a plain build/src/nolphin
     * never has. */
    gtk_icon_theme_add_resource_path (gtk_icon_theme_get_default (),
                       "/org/nolphin/icons");

    /* Fehlen die xsi-Symbole (Mint-Paket xapp-symbolic-icons), zeigt Nolphin
     * die Standardsymbole des aktuellen Themas statt Verbotsschildern. */
    nolphin_icon_fallback_init ();

    gtk_icon_size_register (NOLPHIN_STATUSBAR_ICON_SIZE_NAME,
                            NOLPHIN_STATUSBAR_ICON_SIZE,
                            NOLPHIN_STATUSBAR_ICON_SIZE);

    add_css_provider_at_priority ("/org/nolphin/nolphin-style-fallback.css",
                                  GTK_STYLE_PROVIDER_PRIORITY_FALLBACK);

    add_css_provider_at_priority ("/org/nolphin/nolphin-style-application.css",
                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    GtkSettings *gtk_settings = gtk_settings_get_default ();
    /* We create our own 'runtime theme' when we encounter one that doesn't
     * include nolphin support. We must listen for the theme changing so this
     * customization can be removed (then re-applied only if necessary for the
     * new theme). */
    g_signal_connect_swapped (gtk_settings, "notify::gtk-theme-name", G_CALLBACK (process_system_theme), gtk_settings);

    process_system_theme (gtk_settings);
}

static gboolean
save_accel_map (gpointer data)
{
    if (save_of_accel_map_requested) {
        char *accel_map_filename;
        accel_map_filename = nolphin_get_accel_map_file ();
        if (accel_map_filename) {
            gtk_accel_map_save (accel_map_filename);
            g_free (accel_map_filename);
        }
        save_of_accel_map_requested = FALSE;
    }

    return FALSE;
}

static void 
queue_accel_map_save_callback (GtkAccelMap *object, gchar *accel_path,
        guint accel_key, GdkModifierType accel_mods,
        gpointer user_data)
{
    if (!save_of_accel_map_requested) {
        save_of_accel_map_requested = TRUE;
        g_timeout_add_seconds (NOLPHIN_ACCEL_MAP_SAVE_DELAY, 
                               save_accel_map, NULL);
    }
}

static void
init_gtk_accels (void)
{
    char *accel_map_filename;

    /* load accelerator map, and register save callback */
    accel_map_filename = nolphin_get_accel_map_file ();
    if (accel_map_filename) {
        gtk_accel_map_load (accel_map_filename);
        g_free (accel_map_filename);
    }

    g_signal_connect (gtk_accel_map_get (), "changed",
              G_CALLBACK (queue_accel_map_save_callback), NULL);
}

static void
menu_provider_items_updated_handler (NolphinMenuProvider *provider, GtkWidget* parent_window, gpointer data)
{

    g_signal_emit_by_name (nolphin_signaller_get_current (),
                           "popup_menu_changed");
}

static void
init_menu_provider_callback (void)
{
    GList *providers;
    GList *l;

    providers = nolphin_module_get_extensions_for_type (NOLPHIN_TYPE_MENU_PROVIDER);

    for (l = providers; l != NULL; l = l->next) {
        NolphinMenuProvider *provider = NOLPHIN_MENU_PROVIDER (l->data);

        g_signal_connect_after (G_OBJECT (provider), "items_updated",
                                (GCallback)menu_provider_items_updated_handler,
                                NULL);
    }

    nolphin_module_extension_list_free (providers);
}

/* end Common Startup Stuff */

gboolean
nolphin_application_check_required_directory (NolphinApplication *application,
                                           gchar           *path)
{
    gboolean ret;

    g_assert (NOLPHIN_IS_APPLICATION (application));

    ret = TRUE;

    if (!g_file_test (path, G_FILE_TEST_IS_DIR)) {
        ret = FALSE;
    }

    if (!ret) {
        char *error_string;
        const char *detail_string;
        GtkDialog *dialog;

        error_string = g_strdup_printf (_("Nolphin konnte den benötigten Ordner »%s« nicht anlegen."),
                                        path);
        detail_string = _("Bevor Sie Nolphin benutzen, erstellen Sie bitte folgenden Ordner oder erteilen Sie Nolphin die Zugriffsrechte, diesen anzulegen.");

        dialog = eel_show_error_dialog (error_string, detail_string, NULL);
        /* We need the main event loop so the user has a chance to see the dialog. */
        gtk_application_add_window (GTK_APPLICATION (application),
                                    GTK_WINDOW (dialog));

        g_free (error_string);
    }

    g_free (path);

    return ret;
}

void
nolphin_application_open_location (NolphinApplication *application,
                                GFile           *location,
                                GFile           *selection,
                                const char      *startup_id,
                                const gboolean  open_in_tabs)
{
    NOLPHIN_APPLICATION_CLASS (G_OBJECT_GET_CLASS (application))->open_location (application,
                                                                              location,
                                                                              selection,
                                                                              startup_id,
                                                                              open_in_tabs);
}

void
nolphin_application_show_items (NolphinApplication *application,
                             GFile          **uris,
                             gint             n_uris,
                             const char      *startup_id)
{
    NOLPHIN_APPLICATION_CLASS (G_OBJECT_GET_CLASS (application))->show_items (application,
                                                                           uris,
                                                                           n_uris,
                                                                           startup_id);
}

NolphinWindow *
nolphin_application_create_window (NolphinApplication *application,
                                GdkScreen       *screen)
{
    return NOLPHIN_APPLICATION_CLASS (G_OBJECT_GET_CLASS (application))->create_window (application,
                                                                              screen);
}

void
nolphin_application_notify_unmount_done (NolphinApplication *application,
                                          const gchar *message)
{
    NOLPHIN_APPLICATION_CLASS (G_OBJECT_GET_CLASS (application))->notify_unmount_done (application,
                                                                                    message);
}

void
nolphin_application_notify_unmount_show (NolphinApplication *application,
                                          const gchar *message)
{
    NOLPHIN_APPLICATION_CLASS (G_OBJECT_GET_CLASS (application))->notify_unmount_show (application,
                                                                                    message);
}

void
nolphin_application_close_all_windows (NolphinApplication *application)
{
    NOLPHIN_APPLICATION_CLASS (G_OBJECT_GET_CLASS (application))->close_all_windows (application);
}

static GObject *
nolphin_application_constructor (GType type,
				  guint n_construct_params,
				  GObjectConstructParam *construct_params)
{
        GObject *retval;

        retval = G_OBJECT_CLASS (nolphin_application_parent_class)->constructor
                (type, n_construct_params, construct_params);

        singleton = NOLPHIN_APPLICATION (retval);
        g_object_add_weak_pointer (retval, (gpointer) &singleton);

        return retval;
}

static void
nolphin_application_init (NolphinApplication *application)
{
	GSimpleAction *action;

	application->priv = G_TYPE_INSTANCE_GET_PRIVATE (application,
                                                     NOLPHIN_TYPE_APPLICATION,
                                                     NolphinApplicationPriv);

    if (g_getenv("NOLPHIN_BENCHMARK_LOADING"))
        nolphin_startup_timer = g_timer_new ();

    action = g_simple_action_new ("quit", NULL);

    g_action_map_add_action (G_ACTION_MAP (application), G_ACTION (action));

	g_signal_connect_swapped (action, "activate",
				  G_CALLBACK (nolphin_application_quit), application);

	g_object_unref (action);
}

void
nolphin_application_quit (NolphinApplication *self)
{
	GApplication *app = G_APPLICATION (self);

    /* Run desktop or -main specific destruction - namely tearing down the
     * desktop manager before our g_list_foreach below takes out its windows.
     */

    NOLPHIN_APPLICATION_CLASS (G_OBJECT_GET_CLASS (self))->continue_quit (self);

	GList *windows;

	windows = gtk_application_get_windows (GTK_APPLICATION (app));

	/* Save session state once, before we destroy all windows.
	 * Save the last non-desktop window we find. */
	{
		NolphinWindow *last_window = NULL;

		for (GList *l = windows; l != NULL; l = l->next) {
			GtkWindow *w = GTK_WINDOW (l->data);

			if (!NOLPHIN_IS_WINDOW (w)) {
				continue;
			}

			/* Avoid saving the desktop window */
			if (NOLPHIN_IS_DESKTOP_WINDOW (w)) {
				continue;
			}

			last_window = NOLPHIN_WINDOW (w);
			break;
		}

		if (last_window != NULL) {
			nolphin_window_save_session_state (last_window);
		}
	}

	g_list_foreach (windows, (GFunc) gtk_widget_destroy, NULL);

    /* we have been asked to force quit */
    g_application_quit (G_APPLICATION (self));
}

static void
nolphin_application_startup (GApplication *app)
{
	NolphinApplication *self = NOLPHIN_APPLICATION (app);
	/* chain up to the GTK+ implementation early, so gtk_init()
	 * is called for us.
	 */
	G_APPLICATION_CLASS (nolphin_application_parent_class)->startup (app);

	/* create an undo manager */
	self->undo_manager = nolphin_undo_manager_new ();

	/* initialize preferences and create the global GSettings objects */
	nolphin_global_preferences_init ();

	/* automatische Papierkorb-Bereinigung + Größenlimit-Warnung */
	init_trash_cleanup_scheduler ();

    /* Run desktop- or main- specific things */
    NOLPHIN_APPLICATION_CLASS (G_OBJECT_GET_CLASS (self))->continue_startup (self);

	/* register property pages */
	nolphin_image_properties_page_register ();

	/* initialize theming */
	init_icons_and_styles ();
	init_gtk_accels ();

	/* initialize nolphin modules */
	nolphin_module_setup ();

	/* attach menu-provider module callback */
	init_menu_provider_callback ();

	/* Initialize the UI handler singleton for file operations */
	self->priv->progress_handler = nolphin_progress_ui_handler_new ();

    self->priv->cache_problem = FALSE;
    self->priv->ignore_cache_problem = FALSE;

    /* If 'treat-root-as-normal' is true, assume we're running root as well,
       so we can skip the permission checks */
    if (nolphin_user_is_root () && nolphin_treating_root_as_normal ()) {
        return;
    }
    /* silently do a full check of the cache and fix if running as root.
     * If running as a normal user, do a quick check, and we'll notify the
     * user later if there's a problem via an infobar */
#ifdef HAVE_CINNAMON_DESKTOP
    if (nolphin_user_is_root ()) {
        if (!gnome_desktop_thumbnail_cache_check_permissions (NULL, FALSE))
            gnome_desktop_thumbnail_cache_fix_permissions ();
    } else {
        if (!gnome_desktop_thumbnail_cache_check_permissions (NULL, TRUE))
            self->priv->cache_problem = TRUE;
    }
#else
    if (!nolphin_user_is_root () && !nolphin_thumbnailer_cache_ok ())
        self->priv->cache_problem = TRUE;
#endif
}

static void
nolphin_application_quit_mainloop (GApplication *app)
{
	DEBUG ("Quitting mainloop");

    nolphin_icon_info_clear_caches ();
    save_accel_map (NULL);
    g_object_unref (NOLPHIN_APPLICATION (app)->undo_manager);
    g_clear_object (&mandatory_css_provider);

    nolphin_application_notify_unmount_done (NOLPHIN_APPLICATION (app), NULL);

	G_APPLICATION_CLASS (nolphin_application_parent_class)->quit_mainloop (app);
}

static void
nolphin_application_window_removed (GtkApplication *app,
				     GtkWindow *window)
{
	NolphinPreviewer *previewer;

	/* chain to parent */
	GTK_APPLICATION_CLASS (nolphin_application_parent_class)->window_removed (app, window);

	/* if this was the last window, close the previewer */
	if (g_list_length (gtk_application_get_windows (app)) == 0) {
		previewer = nolphin_previewer_get_singleton ();
		nolphin_previewer_call_close (previewer);
	}
}

static void
nolphin_application_class_init (NolphinApplicationClass *class)
{
    GObjectClass *object_class;
    GApplicationClass *application_class;
    GtkApplicationClass *gtkapp_class;

    object_class = G_OBJECT_CLASS (class);
    object_class->constructor = nolphin_application_constructor;

    application_class = G_APPLICATION_CLASS (class);
    application_class->startup = nolphin_application_startup;
    application_class->quit_mainloop = nolphin_application_quit_mainloop;

    gtkapp_class = GTK_APPLICATION_CLASS (class);
    gtkapp_class->window_removed = nolphin_application_window_removed;

    g_type_class_add_private (class, sizeof (NolphinApplicationPriv));
}

NolphinApplication *
nolphin_application_initialize_singleton (GType object_type,
                                       const gchar *first_property_name,
                                       ...)
{
    NolphinApplication *application;
    va_list var_args;

    va_start (var_args, first_property_name);
    application = NOLPHIN_APPLICATION (g_object_new_valist (object_type, first_property_name, var_args));
    va_end (var_args);

    return application;
}

NolphinApplication *
nolphin_application_get_singleton (void)
{
    return singleton;
}

void
nolphin_application_check_thumbnail_cache (NolphinApplication *application)
{
    application->priv->cache_problem = !nolphin_thumbnail_factory_check_status ();
}

gboolean
nolphin_application_get_cache_bad (NolphinApplication *application)
{
    return application->priv->cache_problem;
}

void
nolphin_application_clear_cache_flag (NolphinApplication *application)
{
    application->priv->cache_problem = FALSE;
}

void
nolphin_application_set_cache_flag (NolphinApplication *application)
{
    application->priv->cache_problem = TRUE;
}

void
nolphin_application_ignore_cache_problem (NolphinApplication *application)
{
    application->priv->ignore_cache_problem = TRUE;
}

gboolean
nolphin_application_get_cache_problem_ignored (NolphinApplication *application)
{
    return application->priv->ignore_cache_problem;
}
