/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2018 Red Hat, Inc
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Matthias Clasen <mclasen@redhat.com>
 */

#include "cc-microphone-page.h"
#include "cc-util.h"

#include <gio/gdesktopappinfo.h>
#include <glib/gi18n.h>

#define APP_PERMISSIONS_TABLE "devices"
#define APP_PERMISSIONS_ID "microphone"

struct _CcMicrophonePage
{
  AdwNavigationPage parent_instance;

  AdwSwitchRow *microphone_row;
  GtkListBox   *microphone_apps_list_box;

  GSettings    *privacy_settings;
  GCancellable *cancellable;

  GDBusProxy   *perm_store;
  GVariant     *microphone_apps_perms;
  GVariant     *microphone_apps_data;
  GHashTable   *microphone_app_switches;

  GtkSizeGroup *microphone_icon_size_group;
};

G_DEFINE_TYPE (CcMicrophonePage, cc_microphone_page, ADW_TYPE_NAVIGATION_PAGE)

typedef struct
{
  CcMicrophonePage *self;
  GtkWidget *widget;
  gchar *app_id;
  gboolean changing_state;
  gboolean pending_state;
} MicrophoneAppStateData;

static void
microphone_app_state_data_free (MicrophoneAppStateData *data)
{
    g_free (data->app_id);
    g_slice_free (MicrophoneAppStateData, data);
}

static void
on_perm_store_set_done (GObject *source_object,
                        GAsyncResult *res,
                        gpointer user_data)
{
  MicrophoneAppStateData *data;
  GVariant *results;
  GError *error = NULL;

  results = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
                                      res,
                                      &error);
  if (results == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Failed to store permissions: %s", error->message);
      g_error_free (error);

      return;
    }
  g_variant_unref (results);

  data = (MicrophoneAppStateData *) user_data;
  data->changing_state = FALSE;
  gtk_switch_set_state (GTK_SWITCH (data->widget), data->pending_state);
}

static gboolean
on_microphone_app_state_set (GtkSwitch *widget,
                           gboolean   state,
                           gpointer   user_data)
{
  MicrophoneAppStateData *data = (MicrophoneAppStateData *) user_data;
  CcMicrophonePage *self = data->self;
  GVariant *params;
  GVariantIter iter;
  const gchar *key;
  gchar **value;
  GVariantBuilder builder;
  gboolean active_microphone;

  if (data->changing_state)
    return TRUE;

  active_microphone = !g_settings_get_boolean (self->privacy_settings,
                                               "disable-microphone");
  data->changing_state = TRUE;
  data->pending_state = active_microphone && state;

  g_variant_iter_init (&iter, self->microphone_apps_perms);
  g_variant_builder_init (&builder, G_VARIANT_TYPE_ARRAY);
  while (g_variant_iter_loop (&iter, "{&s^a&s}", &key, &value))
    {
      if (g_strv_length (value) != 1)
        /* It's OK to drop the entry if it's not in expected format */
        continue;

      if (g_strcmp0 (data->app_id, key) == 0)
        value[0] = state ? "yes" : "no";

      g_variant_builder_add (&builder, "{s^as}", key, value);
    }

  params = g_variant_new ("(sbsa{sas}v)",
                          APP_PERMISSIONS_TABLE,
                          TRUE,
                          APP_PERMISSIONS_ID,
                          &builder,
                          self->microphone_apps_data);

  g_dbus_proxy_call (self->perm_store,
                     "Set",
                     params,
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     self->cancellable,
                     on_perm_store_set_done,
                     data);

  return TRUE;
}

static gboolean
update_app_switch_state (GValue   *value,
                         GVariant *variant,
                         gpointer  data)
{
  GtkSwitch *w = GTK_SWITCH (data);
  gboolean active_microphone;
  gboolean active_app;
  gboolean state;

  active_microphone = !g_variant_get_boolean (variant);
  active_app = gtk_switch_get_active (w);
  state = active_microphone && active_app;

  g_value_set_boolean (value, state);

  return TRUE;
}


static void
add_microphone_app (CcMicrophonePage *self,
                  const gchar    *app_id,
                  gboolean        enabled)
{
  GDesktopAppInfo *app_info;
  char *desktop_id;
  GtkWidget *row, *w;
  GIcon *icon;
  MicrophoneAppStateData *data;

  w = g_hash_table_lookup (self->microphone_app_switches, app_id);
  if (w != NULL)
    {
      gtk_switch_set_active (GTK_SWITCH (w), enabled);

      return;
    }

  desktop_id = g_strdup_printf ("%s.desktop", app_id);
  app_info = g_desktop_app_info_new (desktop_id);
  g_free (desktop_id);
  if (app_info == NULL)
      return;

  row = adw_action_row_new ();
  gtk_list_box_append (self->microphone_apps_list_box, row);

  icon = g_app_info_get_icon (G_APP_INFO (app_info));
  w = gtk_image_new_from_gicon (icon);
  gtk_image_set_icon_size (GTK_IMAGE (w), GTK_ICON_SIZE_LARGE);
  gtk_widget_set_valign (w, GTK_ALIGN_CENTER);
  gtk_size_group_add_widget (self->microphone_icon_size_group, w);
  adw_action_row_add_prefix (ADW_ACTION_ROW (row), w);
  adw_action_row_set_activatable_widget (ADW_ACTION_ROW (row), w);

  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row),
                                 g_app_info_get_name (G_APP_INFO (app_info)));

  w = gtk_switch_new ();
  gtk_switch_set_active (GTK_SWITCH (w), enabled);
  gtk_widget_set_valign (w, GTK_ALIGN_CENTER);
  adw_action_row_add_suffix (ADW_ACTION_ROW (row), w);

  g_settings_bind_with_mapping (self->privacy_settings,
                                "disable-microphone",
                                w,
                                "state",
                                G_SETTINGS_BIND_GET,
                                update_app_switch_state,
                                NULL,
                                g_object_ref (w),
                                g_object_unref);

  g_hash_table_insert (self->microphone_app_switches,
                       g_strdup (app_id),
                       g_object_ref (w));

  data = g_slice_new (MicrophoneAppStateData);
  data->self = self;
  data->app_id = g_strdup (app_id);
  data->widget = w;
  data->changing_state = FALSE;
  g_signal_connect_data (G_OBJECT (w),
                         "state-set",
                         G_CALLBACK (on_microphone_app_state_set),
                         data,
                         (GClosureNotify) microphone_app_state_data_free,
                         0);
}

/* Steals permissions and permissions_data references */
static void
update_perm_store (CcMicrophonePage *self,
                   GVariant *permissions,
                   GVariant *permissions_data)
{
  GVariantIter iter;
  const gchar *key;
  gchar **value;

  g_clear_pointer (&self->microphone_apps_perms, g_variant_unref);
  self->microphone_apps_perms = permissions;
  g_clear_pointer (&self->microphone_apps_data, g_variant_unref);
  self->microphone_apps_data = permissions_data;

  g_variant_iter_init (&iter, permissions);
  while (g_variant_iter_loop (&iter, "{&s^a&s}", &key, &value))
    {
      gboolean enabled;

      if (g_strv_length (value) != 1)
        {
          g_debug ("Permissions for %s in incorrect format, ignoring.", key);
          continue;
        }

      enabled = (g_strcmp0 (value[0], "no") != 0);

      add_microphone_app (self, key, enabled);
    }
}

static void
on_perm_store_signal (GDBusProxy *proxy,
                      gchar      *sender_name,
                      gchar      *signal_name,
                      GVariant   *parameters,
                      gpointer    user_data)
{
  GVariant *permissions, *permissions_data;

  if (g_strcmp0 (signal_name, "Changed") != 0)
    return;

  permissions = g_variant_get_child_value (parameters, 4);
  permissions_data = g_variant_get_child_value (parameters, 3);
  update_perm_store (user_data, permissions, permissions_data);
}

static void
on_perm_store_lookup_done (GObject      *source_object,
                           GAsyncResult *res,
                           gpointer      user_data)
{
  CcMicrophonePage *self = user_data;
  g_autoptr(GError) error = NULL;
  GVariant *ret, *permissions, *permissions_data;

  ret = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
                                  res,
                                  &error);
  if (ret == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Failed fetch permissions from flatpak permission store: %s", error->message);
      return;
    }

  permissions = g_variant_get_child_value (ret, 0);
  permissions_data = g_variant_get_child_value (ret, 1);
  update_perm_store (user_data, permissions, permissions_data);

  g_signal_connect_object (source_object,
                           "g-signal",
                           G_CALLBACK (on_perm_store_signal),
                           self,
                           0);
}

static void
on_perm_store_ready (GObject      *source_object,
                     GAsyncResult *res,
                     gpointer      user_data)
{
  CcMicrophonePage *self;
  g_autoptr(GError) error = NULL;
  GDBusProxy *proxy;
  GVariant *params;

  proxy = g_dbus_proxy_new_for_bus_finish (res, &error);
  if (proxy == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Failed to connect to flatpak permission store: %s", error->message);
      return;
    }
  self = user_data;
  self->perm_store = proxy;

  params = g_variant_new ("(ss)",
                          APP_PERMISSIONS_TABLE,
                          APP_PERMISSIONS_ID);
  g_dbus_proxy_call (self->perm_store,
                     "Lookup",
                     params,
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     self->cancellable,
                     on_perm_store_lookup_done,
                     self);
}

static void
cc_microphone_page_finalize (GObject *object)
{
  CcMicrophonePage *self = CC_MICROPHONE_PAGE (object);

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

  g_clear_object (&self->privacy_settings);
  g_clear_object (&self->perm_store);
  g_clear_object (&self->microphone_icon_size_group);
  g_clear_pointer (&self->microphone_apps_perms, g_variant_unref);
  g_clear_pointer (&self->microphone_apps_data, g_variant_unref);
  g_clear_pointer (&self->microphone_app_switches, g_hash_table_unref);

  G_OBJECT_CLASS (cc_microphone_page_parent_class)->finalize (object);
}

static void
cc_microphone_page_class_init (CcMicrophonePageClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = cc_microphone_page_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/privacy/cc-microphone-page.ui");

  gtk_widget_class_bind_template_child (widget_class, CcMicrophonePage, microphone_row);
  gtk_widget_class_bind_template_child (widget_class, CcMicrophonePage, microphone_apps_list_box);
}

static void
cc_microphone_page_init (CcMicrophonePage *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->microphone_icon_size_group = gtk_size_group_new (GTK_SIZE_GROUP_BOTH);

  self->privacy_settings = g_settings_new ("org.gnome.desktop.privacy");

  self->cancellable = g_cancellable_new ();

  g_settings_bind (self->privacy_settings,
                   "disable-microphone",
                   self->microphone_row,
                   "active",
                   G_SETTINGS_BIND_INVERT_BOOLEAN);

  self->microphone_app_switches = g_hash_table_new_full (g_str_hash,
                                                       g_str_equal,
                                                       g_free,
                                                       g_object_unref);

  g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                            G_DBUS_PROXY_FLAGS_NONE,
                            NULL,
                            "org.freedesktop.impl.portal.PermissionStore",
                            "/org/freedesktop/impl/portal/PermissionStore",
                            "org.freedesktop.impl.portal.PermissionStore",
                            self->cancellable,
                            on_perm_store_ready,
                            self);
}
