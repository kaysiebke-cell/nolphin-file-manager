#include <config.h>

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <math.h>
#include "nolphin-desktop-preferences.h"

typedef struct
{
    GtkBuilder *builder;
    GSettings *desktop_settings;
} NolphinDesktopPreferencesPrivate;

struct _NolphinDesktopPreferences
{
    GtkBin parent_object;

    NolphinDesktopPreferencesPrivate *priv;
};

G_DEFINE_TYPE_WITH_PRIVATE (NolphinDesktopPreferences, nolphin_desktop_preferences, GTK_TYPE_BIN)

/* copied from nolphin-file-management-properties.c */
static void
bind_builder_bool (GtkBuilder *builder,
                   GSettings  *settings,
                   const char *widget_name,
                   const char *prefs)
{
    g_settings_bind (settings, prefs,
                     gtk_builder_get_object (builder, widget_name),
                     "active", G_SETTINGS_BIND_DEFAULT);
}

static void
bind_builder_string_combo (GtkBuilder *builder,
                           GSettings  *settings,
                           const char *widget_name,
                           const char *prefs)
{
    g_settings_bind (settings, prefs,
                     gtk_builder_get_object (builder, widget_name),
                     "active-id", G_SETTINGS_BIND_DEFAULT);
}

static void
nolphin_desktop_preferences_init (NolphinDesktopPreferences *preferences)
{
    GtkWidget *widget;
    NolphinDesktopPreferencesPrivate *priv = nolphin_desktop_preferences_get_instance_private (preferences);

    preferences->priv = priv;

    priv->desktop_settings = g_settings_new ("org.nolphin.desktop");

    priv->builder = gtk_builder_new ();
    gtk_builder_set_translation_domain (priv->builder, GETTEXT_PACKAGE);
    gtk_builder_add_from_resource (priv->builder, "/org/nolphin/nolphin-desktop-preferences.glade", NULL);

    widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "prefs_box"));

    gtk_container_add (GTK_CONTAINER (preferences), widget);

    bind_builder_string_combo (priv->builder,
                               priv->desktop_settings,
                               "layout_combo",
                               "desktop-layout");

    bind_builder_bool (priv->builder,
                       priv->desktop_settings,
                       "computer_switch",
                       "computer-icon-visible");

    bind_builder_bool (priv->builder,
                       priv->desktop_settings,
                       "home_switch",
                       "home-icon-visible");

    bind_builder_bool (priv->builder,
                       priv->desktop_settings,
                       "trash_switch",
                       "trash-icon-visible");

    bind_builder_bool (priv->builder,
                       priv->desktop_settings,
                       "drives_switch",
                       "volumes-visible");

    bind_builder_bool (priv->builder,
                       priv->desktop_settings,
                       "network_switch",
                       "network-icon-visible");

    bind_builder_bool (priv->builder,
                       priv->desktop_settings,
                       "orphan_switch",
                       "show-orphaned-desktop-icons");

    gtk_widget_show_all (GTK_WIDGET (preferences));
}

static void
nolphin_desktop_preferences_dispose (GObject *object)
{
    NolphinDesktopPreferences *preferences = NOLPHIN_DESKTOP_PREFERENCES (object);

    g_clear_object (&preferences->priv->builder);
    g_clear_object (&preferences->priv->desktop_settings);

    G_OBJECT_CLASS (nolphin_desktop_preferences_parent_class)->dispose (object);
}

static void
nolphin_desktop_preferences_class_init (NolphinDesktopPreferencesClass *klass)
{
    G_OBJECT_CLASS (klass)->dispose = nolphin_desktop_preferences_dispose;
}

NolphinDesktopPreferences *
nolphin_desktop_preferences_new (void)
{
    return g_object_new (NOLPHIN_TYPE_DESKTOP_PREFERENCES, NULL);
}
