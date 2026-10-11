/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * nolphin-icon-fallback.c: Ersatz für fehlende xsi-Symbole, siehe Header
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 */

#include <config.h>

#include "nolphin-icon-fallback.h"

#include <glib/gstdio.h>
#include <string.h>
#include <unistd.h>

static const gchar * const xsi_names[] = {
    "xsi-bookmark-new-symbolic", "xsi-computer-symbolic", "xsi-dialog-error-symbolic",
    "xsi-dialog-password-symbolic", "xsi-dialog-warning-symbolic", "xsi-display-symbolic",
    "xsi-document-new-symbolic", "xsi-document-open-symbolic", "xsi-document-properties-symbolic",
    "xsi-drive-harddisk-symbolic", "xsi-drive-removable-media-symbolic", "xsi-edit-clear-symbolic",
    "xsi-edit-copy-symbolic", "xsi-edit-cut-symbolic", "xsi-edit-delete-symbolic",
    "xsi-edit-find-symbolic", "xsi-edit-paste-symbolic", "xsi-edit-redo-symbolic",
    "xsi-edit-undo-symbolic", "xsi-emblem-important-symbolic", "xsi-favorite-symbolic",
    "xsi-folder-documents-symbolic", "xsi-folder-download-symbolic", "xsi-folder-music-symbolic",
    "xsi-folder-new-symbolic", "xsi-folder-open-symbolic", "xsi-folder-pictures-symbolic",
    "xsi-folder-publicshare-symbolic", "xsi-folder-recent-symbolic", "xsi-folder-remote-symbolic",
    "xsi-folder-saved-search-symbolic", "xsi-folder-symbolic", "xsi-folder-templates-symbolic",
    "xsi-folder-videos-symbolic", "xsi-folder-warning-symbolic", "xsi-go-down-symbolic",
    "xsi-go-home-symbolic", "xsi-go-jump-symbolic", "xsi-go-next-symbolic",
    "xsi-go-previous-symbolic", "xsi-go-up-symbolic", "xsi-help-about-symbolic",
    "xsi-help-contents-symbolic", "xsi-keyboard-shortcuts-symbolic", "xsi-list-add-symbolic",
    "xsi-list-remove-symbolic", "xsi-media-eject-symbolic", "xsi-media-mount-symbolic",
    "xsi-media-playback-pause-symbolic", "xsi-media-playback-start-symbolic",
    "xsi-media-playback-stop-symbolic", "xsi-mount-archive-symbolic", "xsi-network-server-symbolic",
    "xsi-network-workgroup-symbolic", "xsi-open-menu-symbolic", "xsi-package-x-generic-symbolic",
    "xsi-pan-end-symbolic", "xsi-pan-start-symbolic", "xsi-pin-symbolic", "xsi-preview-symbolic",
    "xsi-process-stop-symbolic", "xsi-search-symbolic", "xsi-tab-new-symbolic",
    "xsi-text-case-symbolic", "xsi-toolbox-symbolic", "xsi-tools-symbolic",
    "xsi-unfavorite-symbolic", "xsi-unpin-symbolic", "xsi-user-desktop-symbolic",
    "xsi-use-regex-symbolic", "xsi-user-favorites-symbolic", "xsi-user-home-symbolic",
    "xsi-user-trash-full-symbolic", "xsi-user-trash-symbolic", "xsi-utilities-terminal-symbolic",
    "xsi-video-display-symbolic", "xsi-view-compact-symbolic", "xsi-view-dual-symbolic",
    "xsi-view-grid-symbolic", "xsi-view-list-symbolic", "xsi-view-refresh-symbolic",
    "xsi-view-sort-ascending-symbolic", "xsi-window-close-symbolic", "xsi-zoom-in-symbolic",
    "xsi-zoom-original-symbolic", "xsi-zoom-out-symbolic",
    NULL
};

/* Namen, deren Standardsymbol anders heißt als der xsi-Name ohne Vorsilbe.
 * Es gilt der erste vorhandene Eintrag; zuletzt immer der Name ohne "xsi-". */
typedef struct {
    const gchar *xsi;
    const gchar *alternatives[6];
} Alias;

static const Alias aliases[] = {
    { "xsi-search-symbolic",            { "edit-find-symbolic", "system-search-symbolic", NULL } },
    { "xsi-favorite-symbolic",          { "starred-symbolic", "emblem-favorite-symbolic", "bookmark-new-symbolic", NULL } },
    { "xsi-unfavorite-symbolic",        { "non-starred-symbolic", "starred-symbolic", "emblem-favorite-symbolic", NULL } },
    { "xsi-user-favorites-symbolic",    { "starred-symbolic", "user-bookmarks-symbolic", "emblem-favorite-symbolic", NULL } },
    { "xsi-pin-symbolic",               { "view-pin-symbolic", "emblem-important-symbolic", NULL } },
    { "xsi-unpin-symbolic",             { "view-pin-symbolic", "emblem-important-symbolic", NULL } },
    { "xsi-preview-symbolic",           { "view-reveal-symbolic", "document-print-preview-symbolic", "image-x-generic-symbolic", NULL } },
    { "xsi-view-dual-symbolic",         { "view-dual-symbolic", "view-paged-symbolic", "sidebar-show-symbolic", NULL } },
    { "xsi-view-compact-symbolic",      { "view-list-symbolic", NULL } },
    { "xsi-view-grid-symbolic",         { "view-app-grid-symbolic", NULL } },
    { "xsi-tab-new-symbolic",           { "window-new-symbolic", "list-add-symbolic", NULL } },
    { "xsi-toolbox-symbolic",           { "applications-engineering-symbolic", "preferences-system-symbolic", "emblem-system-symbolic", NULL } },
    { "xsi-tools-symbolic",             { "applications-engineering-symbolic", "preferences-system-symbolic", "emblem-system-symbolic", NULL } },
    { "xsi-text-case-symbolic",         { "format-text-uppercase-symbolic", "format-text-bold-symbolic", NULL } },
    { "xsi-use-regex-symbolic",         { "edit-find-replace-symbolic", "edit-find-symbolic", NULL } },
    { "xsi-go-jump-symbolic",           { "go-next-symbolic", NULL } },
    { "xsi-mount-archive-symbolic",     { "package-x-generic-symbolic", "folder-symbolic", NULL } },
    { "xsi-folder-saved-search-symbolic", { "system-search-symbolic", "edit-find-symbolic", "folder-symbolic", NULL } },
    { "xsi-folder-recent-symbolic",     { "document-open-recent-symbolic", "folder-symbolic", NULL } },
    { "xsi-folder-remote-symbolic",     { "network-server-symbolic", "folder-symbolic", NULL } },
    { "xsi-folder-warning-symbolic",    { "dialog-warning-symbolic", "folder-symbolic", NULL } },
    { "xsi-folder-templates-symbolic",  { "folder-symbolic", NULL } },
    { "xsi-folder-publicshare-symbolic", { "folder-symbolic", NULL } },
    { "xsi-open-menu-symbolic",         { "view-more-symbolic", "open-menu", NULL } },
    { "xsi-display-symbolic",           { "video-display-symbolic", "computer-symbolic", NULL } },
    { "xsi-keyboard-shortcuts-symbolic", { "input-keyboard-symbolic", NULL } },
    { "xsi-media-mount-symbolic",       { "drive-harddisk-symbolic", NULL } },
    { "xsi-help-contents-symbolic",     { "help-browser-symbolic", NULL } },
    { "xsi-emblem-important-symbolic",  { "dialog-warning-symbolic", NULL } },
};

const gchar * const *
nolphin_icon_fallback_get_names (void)
{
    return xsi_names;
}

/* Dateiname des ersten passenden Standardsymbols in @theme, sonst NULL. */
static gchar *
find_replacement_file (GtkIconTheme *theme, const gchar *xsi)
{
    GPtrArray *candidates = g_ptr_array_new_with_free_func (g_free);
    gchar *result = NULL;
    gchar *plain;
    guint i, a;

    for (a = 0; a < G_N_ELEMENTS (aliases); a++) {
        if (g_strcmp0 (aliases[a].xsi, xsi) == 0) {
            for (i = 0; aliases[a].alternatives[i] != NULL; i++) {
                g_ptr_array_add (candidates, g_strdup (aliases[a].alternatives[i]));
            }
        }
    }
    g_ptr_array_add (candidates, g_strdup (xsi + strlen ("xsi-")));
    /* zuletzt das nicht-symbolische Symbol gleichen Namens */
    plain = g_strdup (xsi + strlen ("xsi-"));
    if (g_str_has_suffix (plain, "-symbolic")) {
        plain[strlen (plain) - strlen ("-symbolic")] = '\0';
        g_ptr_array_add (candidates, plain);
    } else {
        g_free (plain);
    }

    for (i = 0; i < candidates->len && result == NULL; i++) {
        GtkIconInfo *info = gtk_icon_theme_lookup_icon (theme, g_ptr_array_index (candidates, i), 16, 0);
        if (info != NULL) {
            const gchar *file = gtk_icon_info_get_filename (info);
            /* Nur etwas, das es wirklich gibt: eine Datei oder eine in GTK
             * eingebaute Ressource. Manche Themen haben kaputte Verweise
             * (Datei gefunden, aber ins Leere); dann gilt der nächste Kandidat. */
            if (file != NULL && (g_file_test (file, G_FILE_TEST_EXISTS) ||
                                 g_resources_get_info (file, G_RESOURCE_LOOKUP_FLAGS_NONE, NULL, NULL, NULL))) {
                result = g_strdup (file);
            }
            g_object_unref (info);
        }
    }
    g_ptr_array_free (candidates, TRUE);
    return result;
}

static void
remove_old_links (const gchar *link_dir)
{
    GDir *d = g_dir_open (link_dir, 0, NULL);
    const gchar *name;

    while (d != NULL && (name = g_dir_read_name (d)) != NULL) {
        if (g_str_has_prefix (name, "xsi-")) {
            gchar *p = g_build_filename (link_dir, name, NULL);
            g_unlink (p);
            g_free (p);
        }
    }
    if (d != NULL) {
        g_dir_close (d);
    }
}

guint
nolphin_icon_fallback_apply (GtkIconTheme *theme, const gchar *link_dir)
{
    gchar **path = NULL;
    gint n_path = 0, i;
    GPtrArray *without = g_ptr_array_new ();
    guint created = 0, k;

    g_return_val_if_fail (GTK_IS_ICON_THEME (theme), 0);
    g_return_val_if_fail (link_dir != NULL, 0);

    if (g_mkdir_with_parents (link_dir, 0700) != 0) {
        g_ptr_array_free (without, TRUE);
        return 0;
    }

    /* Beurteilt wird das Thema OHNE unsere eigenen Verweise, sonst würden
     * Verweise eines früheren Themas als "vorhanden" gelten. */
    gtk_icon_theme_get_search_path (theme, &path, &n_path);
    for (i = 0; i < n_path; i++) {
        if (g_strcmp0 (path[i], link_dir) != 0) {
            g_ptr_array_add (without, path[i]);
        }
    }
    g_ptr_array_add (without, NULL);
    gtk_icon_theme_set_search_path (theme, (const gchar **) without->pdata, without->len - 1);
    remove_old_links (link_dir);

    for (k = 0; xsi_names[k] != NULL; k++) {
        GtkIconInfo *existing = gtk_icon_theme_lookup_icon (theme, xsi_names[k], 16, 0);
        gchar *file;

        /* Nicht has_icon(): maßgeblich ist die Suche, die auch die Widgets benutzen. */
        if (existing != NULL) {
            g_object_unref (existing);
            continue;
        }

        file = find_replacement_file (theme, xsi_names[k]);
        if (file != NULL) {
            /* Umfärbbare Pixel-Symbole heißen "<name>.symbolic.png": die
             * doppelte Endung muss erhalten bleiben, sonst erkennt GTK sie
             * nicht als symbolisch. */
            const gchar *ext = g_str_has_suffix (file, ".symbolic.png") ? ".symbolic.png" : strrchr (file, '.');
            gchar *link = g_strconcat (link_dir, G_DIR_SEPARATOR_S, xsi_names[k],
                                       ext != NULL ? ext : ".svg", NULL);

            if (g_file_test (file, G_FILE_TEST_EXISTS)) {
                if (symlink (file, link) == 0) {
                    created++;
                }
            } else {
                /* GTK liefert für manche Symbole ein im Programm eingebautes
                 * (Ressourcen-Pfad, keine Datei): dessen Daten werden
                 * kopiert, ein Verweis ginge ins Leere. */
                GBytes *data = g_resources_lookup_data (file, G_RESOURCE_LOOKUP_FLAGS_NONE, NULL);
                if (data != NULL) {
                    gsize len;
                    gconstpointer bytes = g_bytes_get_data (data, &len);
                    if (g_file_set_contents (link, bytes, len, NULL)) {
                        created++;
                    }
                    g_bytes_unref (data);
                }
            }
            g_free (link);
            g_free (file);
        }
    }

    g_ptr_array_add (without, (gpointer) link_dir);
    gtk_icon_theme_set_search_path (theme, (const gchar **) without->pdata, without->len);

    g_ptr_array_free (without, TRUE);
    g_strfreev (path);
    return created;
}

static gchar *
default_link_dir (void)
{
    return g_build_filename (g_get_user_cache_dir (), "nolphin", "xsi-fallback", NULL);
}

static void
on_icon_theme_name_changed (GObject *settings, GParamSpec *pspec, gpointer user_data)
{
    gchar *dir = default_link_dir ();
    nolphin_icon_fallback_apply (gtk_icon_theme_get_default (), dir);
    g_free (dir);
}

void
nolphin_icon_fallback_init (void)
{
    gchar *dir = default_link_dir ();

    nolphin_icon_fallback_apply (gtk_icon_theme_get_default (), dir);
    g_free (dir);

    g_signal_connect (gtk_settings_get_default (), "notify::gtk-icon-theme-name",
                      G_CALLBACK (on_icon_theme_name_changed), NULL);
}
