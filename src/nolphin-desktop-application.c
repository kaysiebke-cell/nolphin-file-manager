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

#include "nolphin-desktop-application.h"

#include "nolphin-desktop-icon-view.h"
#include "nolphin-desktop-icon-grid-view.h"
#include "nolphin-icon-view.h"
#include "nolphin-list-view.h"
#include "nolphin-freedesktop-dbus.h"

#include "nolphin-desktop-manager.h"
#include "nolphin-image-properties-page.h"
#include "nolphin-previewer.h"
#include "nolphin-progress-ui-handler.h"
#include "nolphin-window-bookmarks.h"
#include "nolphin-window-private.h"

#include <eel/eel-vfs-extensions.h>
#include <libnolphin-private/nolphin-desktop-link-monitor.h>
#include <libnolphin-private/nolphin-desktop-metadata.h>
#include <libnolphin-private/nolphin-file-utilities.h>
#include <libnolphin-private/nolphin-global-preferences.h>
#include <libnolphin-private/nolphin-module.h>
#include <libnolphin-private/nolphin-signaller.h>
#include <libnolphin-private/nolphin-undo-manager.h>
#include <libnolphin-extension/nolphin-menu-provider.h>

#define DEBUG_FLAG NOLPHIN_DEBUG_APPLICATION
#include <libnolphin-private/nolphin-debug.h>

#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <glib/gstdio.h>
#include <glib/gi18n.h>
#include <gdk/gdkx.h>
#include <X11/Xatom.h>

G_DEFINE_TYPE (NolphinDesktopApplication, nolphin_desktop_application, NOLPHIN_TYPE_APPLICATION);

struct _NolphinDesktopApplicationPriv {
    NolphinDesktopManager *desktop_manager;
    NolphinFreedesktopDBus *fdb_manager;
};

static void
nolphin_desktop_application_init (NolphinDesktopApplication *application)
{
    application->priv = G_TYPE_INSTANCE_GET_PRIVATE (application,
                                                     NOLPHIN_TYPE_DESKTOP_APPLICATION,
                                                     NolphinDesktopApplicationPriv);
}

static void
nolphin_desktop_application_finalize (GObject *object)
{
    NolphinDesktopApplication *app = NOLPHIN_DESKTOP_APPLICATION (object);

    g_clear_object (&app->priv->fdb_manager);

    G_OBJECT_CLASS (nolphin_desktop_application_parent_class)->finalize (object);
}

static void
init_desktop (NolphinDesktopApplication *self)
{
	/* Initialize the desktop link monitor singleton */
	nolphin_desktop_link_monitor_get ();
    nolphin_desktop_metadata_init ();

    self->priv->desktop_manager = nolphin_desktop_manager_get ();
}

/* from libwnck/xutils.c, comes as LGPLv2+ */
static char *
latin1_to_utf8 (const char *latin1)
{
  GString *str;
  const char *p;

  str = g_string_new (NULL);

  p = latin1;
  while (*p)
    {
      g_string_append_unichar (str, (gunichar) *p);
      ++p;
    }

  return g_string_free (str, FALSE);
}

/* derived from libwnck/xutils.c, comes as LGPLv2+ */
static void
_get_wmclass (Display *xdisplay,
              Window   xwindow,
              char   **res_class,
              char   **res_name)
{
  XClassHint ch;

  ch.res_name = NULL;
  ch.res_class = NULL;

  gdk_error_trap_push ();
  XGetClassHint (xdisplay, xwindow, &ch);
  gdk_error_trap_pop_ignored ();

  if (res_class)
    *res_class = NULL;

  if (res_name)
    *res_name = NULL;

  if (ch.res_name)
    {
      if (res_name)
        *res_name = latin1_to_utf8 (ch.res_name);

      XFree (ch.res_name);
    }

  if (ch.res_class)
    {
      if (res_class)
        *res_class = latin1_to_utf8 (ch.res_class);

      XFree (ch.res_class);
    }
}

static gboolean
desktop_handler_is_ignored (GdkWindow *window, gchar **ignored)
{
    gboolean ret;
    Window xw;
    Display *xd;
    guint i;

    if (ignored == NULL) {
        return FALSE;
    }

    ret = FALSE;

    xw = gdk_x11_window_get_xid (GDK_X11_WINDOW (window));
    xd = gdk_x11_display_get_xdisplay (gdk_display_get_default ());

    for (i = 0; i < g_strv_length (ignored); i++) {
        gchar *wmclass, *wm_class_casefolded, *match_string_casefolded;

        _get_wmclass (xd, xw, &wmclass, NULL);

        if (wmclass != NULL) {
            wm_class_casefolded = g_utf8_casefold (wmclass, -1);
            match_string_casefolded = g_utf8_casefold (ignored[i], -1);

            if (g_strstr_len (wm_class_casefolded, -1, match_string_casefolded) != NULL) {
                ret = TRUE;
            }

            g_free (wm_class_casefolded);
            g_free (match_string_casefolded);

            g_free (wmclass);
        }

        if (ret == TRUE)
            break;
    }

    return ret;
}

static gboolean
desktop_already_managed (void)
{
    GdkScreen *screen;
    GList *windows, *iter;
    gboolean ret;

    screen = gdk_screen_get_default ();

    windows = gdk_screen_get_window_stack (screen);

    ret = FALSE;

    for (iter = windows; iter != NULL; iter = iter->next) {
        GdkWindow *window = GDK_WINDOW (iter->data);

        if (gdk_window_get_type_hint (window) == GDK_WINDOW_TYPE_HINT_DESKTOP) {
            GSettings *desktop_preferences = g_settings_new("org.nolphin.desktop");

            gchar **ignored = g_settings_get_strv (desktop_preferences, NOLPHIN_PREFERENCES_DESKTOP_IGNORED_DESKTOP_HANDLERS);
            if (!desktop_handler_is_ignored (window, ignored)) {
                ret = TRUE;
            }
            g_strfreev (ignored);
            g_object_unref (desktop_preferences);

            break;
        }
    }

    g_list_free_full (windows, (GDestroyNotify) g_object_unref);

    if (ret) {
        g_warning ("Desktop already managed by another application, skipping desktop setup.\n"
                   "To change this, modify org.nolphin.desktop 'ignored-desktop-handlers'.\n");
    }

    return ret;
}

static gboolean
nolphin_desktop_application_local_command_line (GApplication *application,
                                             gchar      ***arguments,
                                             gint         *exit_status)
{
    GFile **files;
    gint len;

    gboolean version = FALSE;
    gboolean kill_shell = FALSE;
    gboolean debug = FALSE;

    const GOptionEntry options[] = {
        { "version", '\0', 0, G_OPTION_ARG_NONE, &version,
          N_("Die Programmversion anzeigen"), NULL },
        { "debug", 0, 0, G_OPTION_ARG_NONE, &debug,
          "Enable debugging code.  Example usage: 'NOLPHIN_DEBUG=Desktop,Actions nolphin-desktop --debug'.  Use NOLPHIN_DEBUG=all for more topics.", NULL },
        { "quit", 'q', 0, G_OPTION_ARG_NONE, &kill_shell, 
          N_("Nolphin-Schreibtisch beenden."), NULL },
        { NULL }
    };

    GOptionContext *context;
    GError *error = NULL;
    gint argc = 0;
    gchar **argv = NULL;

    *exit_status = EXIT_SUCCESS;

    context = g_option_context_new (_("\n\nDen Schreibtisch mit der Dateiverwaltung handhaben"));
    g_option_context_add_main_entries (context, options, NULL);
    g_option_context_add_group (context, gtk_get_option_group (TRUE));

    argv = *arguments;
    argc = g_strv_length (argv);

    if (!g_option_context_parse (context, &argc, &argv, &error)) {
        g_printerr ("Could not parse arguments: %s\n", error->message);
        g_error_free (error);

        *exit_status = EXIT_FAILURE;
        goto out;
    }

    if (version) {
        g_print ("nolphin-desktop " VERSION "\n");
        goto out;
    }

    if (debug) {
#if (GLIB_CHECK_VERSION(2,80,0))
        const gchar* const domains[] = { "Nolphin", NULL };
        g_log_writer_default_set_debug_domains (domains);
#else
        g_setenv ("G_MESSAGES_DEBUG", "all", TRUE);
#endif
    }

    if (nolphin_user_is_root () && !nolphin_treating_root_as_normal ()) {
        g_printerr ("nolphin-desktop cannot be run as root, please try again as a normal user.\n"
                    "Check 'man nolphin' to see how to change this behavior.");
        goto out;
    }

    g_application_register (application, NULL, &error);

    if (error != NULL) {
        g_printerr ("Could not register the application: %s\n", error->message);
        g_error_free (error);

        *exit_status = EXIT_FAILURE;
        goto out;
    }

    if (kill_shell) {
        g_printerr ("Killing nolphin-desktop, as requested\n");
        g_action_group_activate_action (G_ACTION_GROUP (application),
                        "quit", NULL);
        goto out;
    }

    if (desktop_already_managed ()) {
        goto out;
    }

    files = g_malloc0 (2 * sizeof (GFile *));
    len = 1;
    files[0] = g_file_new_for_uri (EEL_DESKTOP_URI);
    files[1] = NULL;

    g_application_open (application, files, len, "");

    g_object_unref (files[0]);
    g_free (files);

 out:
    g_option_context_free (context);

    return TRUE;
}

static void
nolphin_desktop_application_continue_startup (NolphinApplication *app)
{
    /* Check the user's Desktop and config directories and post warnings
     * if there are problems.
     */
    nolphin_application_check_required_directory (app, nolphin_get_desktop_directory ());
    nolphin_application_check_required_directory (app, nolphin_get_user_directory ());

    NOLPHIN_DESKTOP_APPLICATION (app)->priv->fdb_manager = nolphin_freedesktop_dbus_new ();

	/* register views */
	nolphin_desktop_icon_view_register ();
    nolphin_desktop_icon_grid_view_register ();
    nolphin_icon_view_register ();
}

static void
nolphin_desktop_application_continue_quit (NolphinApplication *app)
{
    NolphinDesktopApplication *self = NOLPHIN_DESKTOP_APPLICATION (app);

    g_clear_object (&self->priv->desktop_manager);
}

static void
nolphin_desktop_application_open (GApplication *app,
                               GFile **files,
                               gint n_files,
                               const gchar *geometry)
{
    NolphinDesktopApplication *self = NOLPHIN_DESKTOP_APPLICATION (app);

    DEBUG ("Open called on the GApplication instance; %d files", n_files);

    if (self->priv->desktop_manager == NULL) {
        init_desktop (self);
    }

    /* FIXME: how to do this? */
}

/* File managers known to accept --select with the same "open parent, select
 * URI" semantics as nolphin. */
static const gchar * const select_capable_handlers[] = {
    "nolphin",
    "nautilus",
    "caja",
    "dolphin",
    NULL
};

static gboolean
handler_supports_select (const gchar *exe)
{
    gchar *basename;
    gboolean found = FALSE;
    gint i;

    if (exe == NULL) {
        return FALSE;
    }

    basename = g_path_get_basename (exe);

    for (i = 0; select_capable_handlers[i] != NULL; i++) {
        if (g_strcmp0 (basename, select_capable_handlers[i]) == 0) {
            found = TRUE;
            break;
        }
    }

    g_free (basename);
    return found;
}

static void
nolphin_desktop_application_open_location (NolphinApplication     *application,
                                        GFile               *location,
                                        GFile               *selection,
                                        const char          *startup_id,
                                        const gboolean      open_in_tabs)
{
    /* Route through show_items so all "open with selection" paths share
     * the same launch logic.  When there's no selection, fall back to
     * g_app_info_launch with the location so we don't lose plain-folder
     * opens (this path isn't currently exercised by the freedesktop dbus
     * handlers but is part of the open_location contract). */
    if (selection != NULL) {
        GFile *one = selection;
        nolphin_application_show_items (application, &one, 1, startup_id);
        return;
    }

    if (location != NULL) {
        GAppInfo *appinfo;
        GError *error = NULL;
        GList *uri_list;

        appinfo = g_app_info_get_default_for_type ("inode/directory", TRUE);
        if (!appinfo) {
            g_warning ("Cannot launch file browser, no mimetype handler for inode/directory");
            return;
        }

        uri_list = g_list_prepend (NULL, location);
        if (!g_app_info_launch (appinfo, uri_list, NULL, &error)) {
            gchar *uri = g_file_get_uri (location);
            g_warning ("Could not launch file browser to display file: %s", uri);
            g_free (uri);
            g_clear_error (&error);
        }

        g_list_free (uri_list);
        g_clear_object (&appinfo);
    }
}

static void
nolphin_desktop_application_show_items (NolphinApplication *application,
                                     GFile          **uris,
                                     gint             n_uris,
                                     const char      *startup_id)
{
    GAppInfo *appinfo;
    GError *error = NULL;
    const gchar *exe;
    gint i;

    if (uris == NULL || n_uris == 0) {
        return;
    }

    appinfo = g_app_info_get_default_for_type ("inode/directory", TRUE);
    if (!appinfo) {
        g_warning ("Cannot launch file browser, no mimetype handler for inode/directory");
        return;
    }

    exe = g_app_info_get_executable (appinfo);

    if (handler_supports_select (exe)) {
        GPtrArray *argv = g_ptr_array_new_with_free_func (g_free);

        g_ptr_array_add (argv, g_strdup (exe));
        g_ptr_array_add (argv, g_strdup ("--select"));
        for (i = 0; i < n_uris; i++) {
            g_ptr_array_add (argv, g_file_get_uri (uris[i]));
        }
        g_ptr_array_add (argv, NULL);

        if (!g_spawn_async (NULL, (gchar **) argv->pdata, NULL,
                            G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, &error)) {
            g_warning ("Could not launch file browser %s: %s", exe,
                       error ? error->message : "(unknown)");
            g_clear_error (&error);
        }

        g_ptr_array_unref (argv);
    } else {
        GList *uri_list = NULL;

        for (i = n_uris - 1; i >= 0; i--) {
            uri_list = g_list_prepend (uri_list, uris[i]);
        }

        if (!g_app_info_launch (appinfo, uri_list, NULL, &error)) {
            g_warning ("Could not launch file browser: %s",
                       error ? error->message : "(unknown)");
            g_clear_error (&error);
        }

        g_list_free (uri_list);
    }

    g_clear_object (&appinfo);
}

static NolphinWindow *
nolphin_desktop_application_create_window (NolphinApplication *application,
                                        GdkScreen       *screen)
{
    return NULL;
}

static void
nolphin_desktop_application_class_init (NolphinDesktopApplicationClass *class)
{
    GObjectClass *object_class;
    GApplicationClass *application_class;
    NolphinApplicationClass *nolphin_app_class;

    object_class = G_OBJECT_CLASS (class);
    object_class->finalize = nolphin_desktop_application_finalize;

    application_class = G_APPLICATION_CLASS (class);
    application_class->open = nolphin_desktop_application_open;
    application_class->local_command_line = nolphin_desktop_application_local_command_line;

    nolphin_app_class = NOLPHIN_APPLICATION_CLASS (class);
    nolphin_app_class->continue_startup = nolphin_desktop_application_continue_startup;
    nolphin_app_class->create_window = nolphin_desktop_application_create_window;
    nolphin_app_class->continue_quit = nolphin_desktop_application_continue_quit;
    nolphin_app_class->open_location = nolphin_desktop_application_open_location;
    nolphin_app_class->show_items = nolphin_desktop_application_show_items;

    g_type_class_add_private (class, sizeof (NolphinDesktopApplicationPriv));
}

NolphinApplication *
nolphin_desktop_application_get_singleton (void)
{
    return nolphin_application_initialize_singleton (NOLPHIN_TYPE_DESKTOP_APPLICATION,
                                                  "application-id", "org.NolphinDesktop",
                                                  "flags", G_APPLICATION_HANDLES_OPEN,
                                                  "register-session", TRUE,
                                                  NULL);
}

