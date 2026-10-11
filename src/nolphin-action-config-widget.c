/* nolphin-action-config-widget.h */

/*  A widget that displays a list of actions to enable or disable.
 *  This is usually part of a NolphinPluginManagerWidget
 */

#include <config.h>
#include "nolphin-action-config-widget.h"
#include "nolphin-application.h"
#include "nolphin-view.h"
#include "nolphin-file.h"
#include <glib.h>
#include <libnolphin-private/nolphin-action-manager.h>
#include <libnolphin-private/nolphin-action-symbols.h>
#include "nolphin-global-preferences.h"

G_DEFINE_TYPE (NolphinActionConfigWidget, nolphin_action_config_widget, NOLPHIN_TYPE_CONFIG_BASE_WIDGET);


typedef struct {
    NolphinActionConfigWidget *widget;

    gchar *name;
    gchar *comment;
    gchar *stock_id;
    gchar *icon_name;
    gchar *filename;
} ActionProxy;

static void
action_proxy_free (ActionProxy *proxy)
{
    g_clear_pointer (&proxy->name, g_free);
    g_clear_pointer (&proxy->comment, g_free);
    g_clear_pointer (&proxy->stock_id, g_free);
    g_clear_pointer (&proxy->icon_name, g_free);
    g_clear_pointer (&proxy->filename, g_free);
    g_free (proxy);
}

static GtkWidget *
get_button_for_row (GtkWidget *row)
{
    GtkWidget *ret;

    GtkWidget *box = gtk_bin_get_child (GTK_BIN (row));
    GList *clist = gtk_container_get_children (GTK_CONTAINER (box));

    ret = clist->data;

    g_list_free (clist);

    return ret;
}

static void
on_row_activated (GtkWidget *box, GtkWidget *row, GtkWidget *widget)
{
    GtkWidget *button = get_button_for_row (row);

    gtk_button_clicked (GTK_BUTTON (button));
}

static void
on_check_toggled(GtkWidget *button, ActionProxy *proxy)
{
    gboolean enabled = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));

    gchar **blacklist = g_settings_get_strv (nolphin_plugin_preferences, NOLPHIN_PLUGIN_PREFERENCES_DISABLED_ACTIONS);

    GPtrArray *new_list = g_ptr_array_new ();

    guint i;

    if (enabled) {
        for (i = 0; i < g_strv_length (blacklist); i++) {
            if (g_strcmp0 (blacklist[i], proxy->filename) == 0)
                continue;
            g_ptr_array_add (new_list, g_strdup (blacklist[i]));
        }
    } else {
        for (i = 0; i < g_strv_length (blacklist); i++) {
            g_ptr_array_add (new_list, g_strdup (blacklist[i]));
        }

        g_ptr_array_add (new_list, g_strdup (proxy->filename));
    }

    g_ptr_array_add (new_list, NULL);

    gchar **new_list_ptr = (char **) g_ptr_array_free (new_list, FALSE);

    g_signal_handler_block (nolphin_plugin_preferences, proxy->widget->bl_handler);
    g_settings_set_strv (nolphin_plugin_preferences,
    		             NOLPHIN_PLUGIN_PREFERENCES_DISABLED_ACTIONS,
						 (const gchar * const *) new_list_ptr);
    g_signal_handler_unblock (nolphin_plugin_preferences, proxy->widget->bl_handler);

    g_strfreev (blacklist);
    g_strfreev (new_list_ptr);
}

static ActionProxy *
make_action_proxy (const gchar *filename, const gchar *fullpath)
{
    GKeyFile *key_file = g_key_file_new();

    g_key_file_load_from_file (key_file, fullpath, G_KEY_FILE_NONE, NULL);

    if (g_key_file_has_key (key_file, ACTION_FILE_GROUP, KEY_ACTIVE, NULL)) {
        if (!g_key_file_get_boolean (key_file, ACTION_FILE_GROUP, KEY_ACTIVE, NULL)) {
            g_key_file_free (key_file);
            return NULL;
        }
    }

    ActionProxy *proxy = g_new0 (ActionProxy, 1);

    gchar *name = g_key_file_get_locale_string (key_file,
                                                ACTION_FILE_GROUP,
                                                KEY_NAME,
                                                NULL,
                                                NULL);
    if (name != NULL)
        proxy->name = g_strdup (name);

    gchar *comment = g_key_file_get_locale_string (key_file,
                                            ACTION_FILE_GROUP,
                                            KEY_COMMENT,
                                            NULL,
                                            NULL);
    if (comment != NULL)
        proxy->comment = g_strdup (comment);


    gchar *icon_name = g_key_file_get_string (key_file,
                                              ACTION_FILE_GROUP,
                                              KEY_ICON_NAME,
                                              NULL);
    if (icon_name != NULL)
        proxy->icon_name = g_strdup (icon_name);

    gchar *stock_id = g_key_file_get_string (key_file,
                                             ACTION_FILE_GROUP,
                                             KEY_STOCK_ID,
                                             NULL);

    if (stock_id != NULL)
        proxy->stock_id = g_strdup (stock_id);

    proxy->filename = g_strdup (filename);

    g_free (name);
    g_free (comment);
    g_free (icon_name);
    g_free (stock_id);
    g_key_file_free (key_file);

    return proxy;
}

static void
populate_from_directory (NolphinActionConfigWidget *widget, const gchar *path)
{
    GDir *dir;

    dir = g_dir_open (path, 0, NULL);

    if (dir) {
        const char *name;

        while ((name = g_dir_read_name (dir))) {
            if (g_str_has_suffix (name, ".nolphin_action")) {
                char *filename;
                GList *l;
                gboolean seen = FALSE;

                /* Gleichnamige Aktionen aus mehreren Datenordnern nur einmal listen. */
                for (l = widget->actions; l != NULL && !seen; l = l->next) {
                    g_autofree gchar *base = g_path_get_basename (((ActionProxy *) l->data)->filename);

                    seen = g_strcmp0 (base, name) == 0;
                }
                if (seen) {
                    continue;
                }

                filename = g_build_filename (path, name, NULL);
                ActionProxy *p = make_action_proxy (name, filename);

                if (p != NULL) {
                    p->widget = widget;
                    widget->actions = g_list_append (widget->actions, p);
                }

                g_free (filename);
            }
        }

        g_dir_close (dir);
    }
}

static gchar *
strip_accel (const gchar *input)
{
    gchar *ret = NULL;

    gchar **split = g_strsplit (input, "_", 2);

    if (g_strv_length (split) == 1)
        ret = g_strdup (split[0]);
    else if (g_strv_length (split) == 2)
        ret = g_strconcat (split[0], split[1], NULL);

    g_strfreev (split);

    return ret;
}

static gint
sort_actions (gconstpointer a, gconstpointer b)
{
    ActionProxy *action_a = (ActionProxy *) a;
    ActionProxy *action_b = (ActionProxy *) b;

    return g_strcmp0 (action_a->filename, action_b->filename);
}

static void
refresh_widget (NolphinActionConfigWidget *widget)
{
    gchar **data_dirs;
    gchar *path;
    guint i;

    if (widget->actions != NULL) {
        g_list_free_full (widget->actions, (GDestroyNotify) action_proxy_free);
        widget->actions = NULL;
    }

    nolphin_config_base_widget_clear_list (NOLPHIN_CONFIG_BASE_WIDGET (widget));

    data_dirs = (gchar **) g_get_system_data_dirs ();

    for (i = 0; i < g_strv_length (data_dirs); i++) {
        path = nolphin_action_manager_get_system_directory_path (data_dirs[i]);
        populate_from_directory (widget, path);
        g_clear_pointer (&path, g_free);
    }

    path = nolphin_action_manager_get_user_directory_path ();
    populate_from_directory (widget, path);
    g_clear_pointer (&path, g_free);

    if (widget->actions == NULL) {
        GtkWidget *empty_label = gtk_label_new (NULL);
        gchar *markup = NULL;

        markup = g_strdup_printf ("<i>%s</i>", _("Keine Aktionen gefunden"));

        gtk_label_set_markup (GTK_LABEL (empty_label), markup);
        g_free (markup);

        GtkWidget *empty_row = gtk_list_box_row_new ();
        gtk_container_add (GTK_CONTAINER (empty_row), empty_label);

        gtk_widget_show_all (empty_row);
        gtk_container_add (GTK_CONTAINER (NOLPHIN_CONFIG_BASE_WIDGET (widget)->listbox), empty_row);
        gtk_widget_set_sensitive (GTK_WIDGET (NOLPHIN_CONFIG_BASE_WIDGET (widget)->listbox), FALSE);
    } else {
        GList *l;
        gchar **blacklist = g_settings_get_strv (nolphin_plugin_preferences,
        		                                 NOLPHIN_PLUGIN_PREFERENCES_DISABLED_ACTIONS);

        widget->actions = g_list_sort (widget->actions, (GCompareFunc) sort_actions);

        for (l = widget->actions; l != NULL; l=l->next) {
            ActionProxy *proxy = l->data;

            gboolean active = TRUE;
            guint i = 0;

            for (i = 0; i < g_strv_length (blacklist); i++) {
                if (g_strcmp0 (blacklist[i], proxy->filename) == 0) {
                    active = FALSE;
                    break;
                }
            }

            GtkWidget *w;
            GtkWidget *box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

            GtkWidget *button = gtk_check_button_new ();

            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), active);
            g_signal_connect (button, "toggled", G_CALLBACK (on_check_toggled), proxy);
            gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 2);

            w = gtk_image_new ();
            if (proxy->stock_id != NULL)
                gtk_image_set_from_stock (GTK_IMAGE (w), proxy->stock_id, GTK_ICON_SIZE_MENU);
            else if (proxy->icon_name != NULL)
                gtk_image_set_from_icon_name (GTK_IMAGE (w), proxy->icon_name, GTK_ICON_SIZE_MENU);
            gtk_box_pack_start (GTK_BOX (box), w, FALSE, FALSE, 2);

            gchar *display_name = strip_accel (proxy->name);
            w = gtk_label_new (display_name);
            g_free (display_name);

            gtk_widget_set_tooltip_text(w, proxy->comment);

            gtk_box_pack_start (GTK_BOX (box), w, FALSE, FALSE, 2);

            GtkWidget *row = gtk_list_box_row_new ();
            g_object_set(G_OBJECT (row),
                         "margin-top", 4,
                         "margin-bottom", 4,
                         NULL);
            gtk_container_add (GTK_CONTAINER (row), box);

            gtk_widget_show_all (row);
            gtk_container_add (GTK_CONTAINER (NOLPHIN_CONFIG_BASE_WIDGET (widget)->listbox), row);
        }

        gtk_widget_set_sensitive (GTK_WIDGET (NOLPHIN_CONFIG_BASE_WIDGET (widget)->listbox), TRUE);

        g_strfreev (blacklist);
    }

    nolphin_config_base_widget_set_default_buttons_sensitive (NOLPHIN_CONFIG_BASE_WIDGET (widget), widget->actions != NULL);
}

static void
on_settings_changed (GSettings *settings, gchar *key, gpointer user_data)
{
    NolphinActionConfigWidget *w = NOLPHIN_ACTION_CONFIG_WIDGET (user_data);

    refresh_widget (w);
}

static void
on_enable_clicked (GtkWidget *button, NolphinActionConfigWidget *widget)
{
    g_settings_set_strv (nolphin_plugin_preferences,
    		             NOLPHIN_PLUGIN_PREFERENCES_DISABLED_ACTIONS,
						 NULL);
}

static void
on_disable_clicked (GtkWidget *button, NolphinActionConfigWidget *widget)
{
    GPtrArray *new_list = g_ptr_array_new ();

    GList *l;

    for (l = widget->actions; l != NULL; l = l->next)
        g_ptr_array_add (new_list, g_strdup (((ActionProxy *) l->data)->filename));

    g_ptr_array_add (new_list, NULL);

    gchar **new_list_ptr = (char **) g_ptr_array_free (new_list, FALSE);
    g_settings_set_strv (nolphin_plugin_preferences,
    		             NOLPHIN_PLUGIN_PREFERENCES_DISABLED_ACTIONS,
						 (const gchar * const *) new_list_ptr);

    g_strfreev (new_list_ptr);
}

static void
on_open_folder_clicked (GtkWidget *button, NolphinActionConfigWidget *widget)
{
    gchar *path = NULL;
    path = g_build_filename (g_get_user_data_dir (), "nolphin", "actions", NULL);
    GFile *location = g_file_new_for_path (path);

    nolphin_application_open_location (nolphin_application_get_singleton (),
                                    location,
                                    NULL,
                                    "nolphin",
                                    FALSE);

    g_free (path);
    g_object_unref (location);
}

static void
on_layout_editor_clicked (GtkWidget *button, NolphinActionConfigWidget *widget)
{
    g_spawn_command_line_async ("nolphin-action-layout-editor", NULL);
}

static void
on_dir_changed (GFileMonitor     *monitor,
                GFile            *file,
                GFile            *other_file,
                GFileMonitorEvent event_type,
                gpointer          user_data)
{
    NolphinActionConfigWidget *widget = NOLPHIN_ACTION_CONFIG_WIDGET (user_data);

    refresh_widget (widget);
}

static void
try_monitor_path (NolphinActionConfigWidget *widget, const gchar *path)
{
    GFile *dir = g_file_new_for_path (path);

    GFileMonitor *mon = g_file_monitor_directory (dir, G_FILE_MONITOR_SEND_MOVED, NULL, NULL);

    g_object_unref (dir);

    if (mon != NULL) {
        g_signal_connect (mon, "changed", G_CALLBACK (on_dir_changed), widget);
        widget->dir_monitors = g_list_append (widget->dir_monitors, mon);
    }
}

static void setup_dir_monitors (NolphinActionConfigWidget *widget)
{
    widget->dir_monitors = NULL;

    gchar **data_dirs = (gchar **) g_get_system_data_dirs ();

    guint i;
    for (i = 0; i < g_strv_length (data_dirs); i++) {
        gchar *path = nolphin_action_manager_get_system_directory_path (data_dirs[i]);
        try_monitor_path (widget, path);
        g_free (path);
    }

    gchar *path = nolphin_action_manager_get_user_directory_path ();
    try_monitor_path (widget, path);
    g_free (path);
}

static void
nolphin_action_config_widget_finalize (GObject *object)
{
    NolphinActionConfigWidget *widget = NOLPHIN_ACTION_CONFIG_WIDGET (object);

    if (widget->actions != NULL) {
        g_list_free_full (widget->actions, (GDestroyNotify) action_proxy_free);
        widget->actions = NULL;
    }

    GList *l;

    for (l = widget->dir_monitors; l != NULL; l = l->next) {
        g_file_monitor_cancel (l->data);
        g_object_unref (l->data);
    }

    g_list_free (widget->dir_monitors);

    g_signal_handler_disconnect (nolphin_plugin_preferences, widget->bl_handler);

    G_OBJECT_CLASS (nolphin_action_config_widget_parent_class)->finalize (object);
}

static void
nolphin_action_config_widget_class_init (NolphinActionConfigWidgetClass *klass)
{
    GObjectClass *oclass;
    oclass = G_OBJECT_CLASS (klass);

    oclass->finalize = nolphin_action_config_widget_finalize;
}

static void
nolphin_action_config_widget_init (NolphinActionConfigWidget *self)
{
    self->actions = NULL;

    self->bl_handler = g_signal_connect (nolphin_plugin_preferences, 
                                         "changed::" NOLPHIN_PLUGIN_PREFERENCES_DISABLED_ACTIONS,
                                         G_CALLBACK (on_settings_changed), self);

    GtkWidget *label = nolphin_config_base_widget_get_label (NOLPHIN_CONFIG_BASE_WIDGET (self));

    gchar *title = g_strdup (_("Aktionen"));
    gchar *markup = g_strdup_printf ("<b>%s</b>", title);

    gtk_label_set_markup (GTK_LABEL (label), markup);

    g_free (title);
    g_free (markup);

    GtkWidget *widget = gtk_button_new_from_icon_name ("xsi-folder-symbolic", GTK_ICON_SIZE_BUTTON);

    GtkWidget *bb = NOLPHIN_CONFIG_BASE_WIDGET (self)->rbuttonbox;
    gtk_box_pack_end (GTK_BOX (bb),
                      widget,
                      FALSE, FALSE, 0);
    gtk_widget_show (widget);
    g_signal_connect (widget, "clicked", G_CALLBACK (on_open_folder_clicked), self);

    widget = gtk_button_new_with_label (_("Anordnung bearbeiten"));

    bb = NOLPHIN_CONFIG_BASE_WIDGET (self)->lbuttonbox;
    gtk_box_pack_start (GTK_BOX (bb),
                        widget,
                        FALSE, FALSE, 0);
    gtk_widget_show (widget);
    g_signal_connect (widget, "clicked", G_CALLBACK (on_layout_editor_clicked), self);

    g_signal_connect (nolphin_config_base_widget_get_enable_button (NOLPHIN_CONFIG_BASE_WIDGET (self)), "clicked",
                                                                 G_CALLBACK (on_enable_clicked), self);

    g_signal_connect (nolphin_config_base_widget_get_disable_button (NOLPHIN_CONFIG_BASE_WIDGET (self)), "clicked",
                                                                  G_CALLBACK (on_disable_clicked), self);

    g_signal_connect (NOLPHIN_CONFIG_BASE_WIDGET (self)->listbox, "row-activated", G_CALLBACK (on_row_activated), self);

    refresh_widget (self);

    setup_dir_monitors (self);
}

GtkWidget *
nolphin_action_config_widget_new (void)
{
  return g_object_new (NOLPHIN_TYPE_ACTION_CONFIG_WIDGET, NULL);
}
