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

#include "cc-location-page.h"
#include "cc-util.h"

#include <gio/gdesktopappinfo.h>
#include <glib/gi18n.h>

#define LOCATION_ENABLED "enabled"
#define APP_PERMISSIONS_TABLE "location"
#define APP_PERMISSIONS_ID "location"

struct _CcLocationPage
{
  AdwNavigationPage parent_instance;

  GtkLabel     *privacy_policy_link;

  GtkListBox   *location_apps_list_box;
  AdwSwitchRow *location_row;

  GSettings    *location_settings;
  GCancellable *cancellable;

  GDBusProxy   *perm_store;
  GVariant     *location_apps_perms;
  GVariant     *location_apps_data;
  GHashTable   *location_app_switches;
  GHashTable   *location_app_rows;

  GtkSizeGroup *location_icon_size_group;
};

G_DEFINE_TYPE (CcLocationPage, cc_location_page, ADW_TYPE_NAVIGATION_PAGE)

typedef struct
{
  CcLocationPage *self;
  GtkWidget      *widget;
  gchar          *app_id;
  gboolean        changing_state;
  gboolean        pending_state;
} LocationAppStateData;

static void
location_app_state_data_free (LocationAppStateData *data)
{
    g_free (data->app_id);
    g_slice_free (LocationAppStateData, data);
}

static void
on_perm_store_set_done (GObject      *source_object,
                        GAsyncResult *res,
                        gpointer      user_data)
{
  g_autoptr(GVariant) results = NULL;
  g_autoptr(GError) error = NULL;
  LocationAppStateData *data;

  results = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
                                      res,
                                      &error);
  if (results == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Failed to store permissions: %s", error->message);

      return;
    }

  data = (LocationAppStateData *) user_data;
  data->changing_state = FALSE;
  gtk_switch_set_state (GTK_SWITCH (data->widget), data->pending_state);
}

static gboolean
on_location_app_state_set (GtkSwitch *widget,
                           gboolean   state,
                           gpointer   user_data)
{
  LocationAppStateData *data = (LocationAppStateData *) user_data;
  CcLocationPage *self = data->self;
  GVariant *params;
  GVariantIter iter;
  const gchar *key;
  gchar **value;
  GVariantBuilder builder;
  gboolean active_location;

  if (data->changing_state)
    return TRUE;

  active_location = g_settings_get_boolean (self->location_settings,
                                            LOCATION_ENABLED);
  data->changing_state = TRUE;
  data->pending_state = active_location && state;

  g_variant_iter_init (&iter, self->location_apps_perms);
  g_variant_builder_init (&builder, G_VARIANT_TYPE_ARRAY);
  while (g_variant_iter_loop (&iter, "{&s^a&s}", &key, &value))
    {
      /* It's OK to drop the entry if it's not in expected format */
      if (g_strv_length (value) < 2)
        continue;

      if (g_strcmp0 (data->app_id, key) == 0)
        value[0] = state ? "EXACT" : "NONE";

      g_variant_builder_add (&builder, "{s^as}", key, value);
    }

  params = g_variant_new ("(sbsa{sas}v)",
                          APP_PERMISSIONS_TABLE,
                          TRUE,
                          APP_PERMISSIONS_ID,
                          &builder,
                          self->location_apps_data);

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
  gboolean active_location;
  gboolean active_app;
  gboolean state;

  active_location = g_variant_get_boolean (variant);
  active_app = gtk_switch_get_active (w);
  state = active_location && active_app;

  g_value_set_boolean (value, state);

  return TRUE;
}

static void
add_location_app (CcLocationPage *self,
                  const gchar    *app_id,
                  gboolean        enabled,
                  gint64          last_used)
{
  LocationAppStateData *data;
  GDesktopAppInfo *app_info;
  GDateTime *t;
  GtkWidget *row, *w;
  GIcon *icon;
  gchar *last_used_str;
  gchar *desktop_id;

  w = g_hash_table_lookup (self->location_app_switches, app_id);
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
  gtk_list_box_append (self->location_apps_list_box, row);

  icon = g_app_info_get_icon (G_APP_INFO (app_info));
  w = gtk_image_new_from_gicon (icon);
  gtk_image_set_icon_size (GTK_IMAGE (w), GTK_ICON_SIZE_LARGE);
  gtk_widget_set_valign (w, GTK_ALIGN_CENTER);
  gtk_size_group_add_widget (self->location_icon_size_group, w);
  adw_action_row_add_prefix (ADW_ACTION_ROW (row), w);

  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row),
                                 g_app_info_get_name (G_APP_INFO (app_info)));

  t = g_date_time_new_from_unix_utc (last_used);
  last_used_str = cc_util_get_smart_date (t);
  w = gtk_label_new (last_used_str);
  g_free (last_used_str);
  gtk_widget_add_css_class (w, "dim-label");
  gtk_widget_set_margin_start (w, 12);
  gtk_widget_set_margin_end (w, 12);
  gtk_widget_set_valign (w, GTK_ALIGN_CENTER);
  adw_action_row_add_suffix (ADW_ACTION_ROW (row), w);

  w = gtk_switch_new ();
  gtk_switch_set_active (GTK_SWITCH (w), enabled);
  gtk_widget_set_valign (w, GTK_ALIGN_CENTER);
  adw_action_row_add_suffix (ADW_ACTION_ROW (row), w);
  adw_action_row_set_activatable_widget (ADW_ACTION_ROW (row), w);

  g_settings_bind_with_mapping (self->location_settings,
                                LOCATION_ENABLED,
                                w,
                                "state",
                                G_SETTINGS_BIND_GET,
                                update_app_switch_state,
                                NULL,
                                g_object_ref (w),
                                g_object_unref);

  g_hash_table_insert (self->location_app_switches,
                       g_strdup (app_id),
                       g_object_ref (w));
  g_hash_table_insert (self->location_app_rows,
                       g_strdup (app_id),
                       g_object_ref (row));

  data = g_slice_new (LocationAppStateData);
  data->self = self;
  data->app_id = g_strdup (app_id);
  data->widget = w;
  data->changing_state = FALSE;
  g_signal_connect_data (w,
                         "state-set",
                         G_CALLBACK (on_location_app_state_set),
                         data,
                         (GClosureNotify) location_app_state_data_free,
                         0);
}

/* Steals permissions and permissions_data references */
static void
update_perm_store (CcLocationPage *self,
                   GVariant       *permissions,
                   GVariant       *permissions_data)
{
  GVariantIter iter;
  const gchar *key;
  gchar **value;
  GHashTableIter row_iter;
  GtkWidget *row;

  g_clear_pointer (&self->location_apps_perms, g_variant_unref);
  self->location_apps_perms = permissions;

  g_clear_pointer (&self->location_apps_data, g_variant_unref);
  self->location_apps_data = permissions_data;

  /* We iterate over all rows, if the permissions do not contain the app id of
     the row, we remove it. */
  g_hash_table_iter_init (&row_iter, self->location_app_rows);
  while (g_hash_table_iter_next (&row_iter, (gpointer *) &key, (gpointer *) &row))
    {
      if (!g_variant_lookup_value (permissions, key, NULL))
        {
          gtk_list_box_remove (self->location_apps_list_box, row);
          g_hash_table_remove (self->location_app_switches, key);
          g_hash_table_iter_remove (&row_iter);
        }
    }

  g_variant_iter_init (&iter, permissions);
  while (g_variant_iter_loop (&iter, "{&s^a&s}", &key, &value))
    {
      gboolean enabled;
      gint64 last_used;

      if (g_strv_length (value) < 2)
        {
          g_debug ("Permissions for %s in incorrect format, ignoring..", key);
          continue;
        }

      enabled = (g_strcmp0 (value[0], "NONE") != 0);
      last_used = g_ascii_strtoll (value[1], NULL, 10);

      add_location_app (self, key, enabled, last_used);
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
  g_autoptr(GVariant) boxed_permission_data = NULL;
  g_autoptr(GVariant) table = NULL;
  g_autoptr(GVariant) id = NULL;

  if (g_strcmp0 (signal_name, "Changed") != 0)
    return;

  table = g_variant_get_child_value (parameters, 0);
  id = g_variant_get_child_value (parameters, 1);

  if (g_strcmp0 (g_variant_get_string (table, NULL), "location") != 0 || g_strcmp0 (g_variant_get_string (id, NULL), "location") != 0)
    return;

  permissions = g_variant_get_child_value (parameters, 4);
  boxed_permission_data = g_variant_get_child_value (parameters, 3);
  permissions_data = g_variant_get_variant (boxed_permission_data);
  update_perm_store (user_data, permissions, permissions_data);
}

static void
on_perm_store_lookup_done(GObject *source_object,
                          GAsyncResult *res,
                          gpointer user_data)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GVariant) boxed_permission_data = NULL;
  GVariant *ret, *permissions, *permissions_data;

  ret = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
                                  res,
                                  &error);
  if (ret == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Failed fetch permissions from flatpak permission store: %s",
                   error->message);
      return;
    }

  permissions = g_variant_get_child_value (ret, 0);
  boxed_permission_data = g_variant_get_child_value (ret, 1);
  permissions_data = g_variant_get_variant (boxed_permission_data);
  update_perm_store (user_data, permissions, permissions_data);

  g_signal_connect_object (source_object,
                           "g-signal",
                           G_CALLBACK (on_perm_store_signal),
                           user_data,
                           0);
}

static void
on_perm_store_ready (GObject      *source_object,
                     GAsyncResult *res,
                     gpointer      user_data)
{
  g_autoptr(GError) error = NULL;
  CcLocationPage *self;
  GDBusProxy *proxy;
  GVariant *params;

  proxy = g_dbus_proxy_new_for_bus_finish (res, &error);
  if (proxy == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
          g_warning ("Failed to connect to flatpak permission store: %s",
                     error->message);

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
cc_location_page_finalize (GObject *object)
{
  CcLocationPage *self = CC_LOCATION_PAGE (object);

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

  g_clear_object (&self->location_settings);
  g_clear_object (&self->perm_store);
  g_clear_object (&self->location_icon_size_group);
  g_clear_pointer (&self->location_apps_perms, g_variant_unref);
  g_clear_pointer (&self->location_apps_data, g_variant_unref);
  g_clear_pointer (&self->location_app_switches, g_hash_table_unref);
  g_clear_pointer (&self->location_app_rows, g_hash_table_unref);

  G_OBJECT_CLASS (cc_location_page_parent_class)->finalize (object);
}

static void
cc_location_page_class_init (CcLocationPageClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = cc_location_page_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/privacy/cc-location-page.ui");

  gtk_widget_class_bind_template_child (widget_class, CcLocationPage, privacy_policy_link);
  gtk_widget_class_bind_template_child (widget_class, CcLocationPage, location_apps_list_box);
  gtk_widget_class_bind_template_child (widget_class, CcLocationPage, location_row);
}

static void
cc_location_page_init (CcLocationPage *self)
{
  g_autofree gchar *privacy_policy_link = NULL;

  gtk_widget_init_template (GTK_WIDGET (self));

  self->location_icon_size_group = gtk_size_group_new (GTK_SIZE_GROUP_BOTH);
  self->location_settings = g_settings_new ("org.gnome.system.location");

  self->cancellable = g_cancellable_new ();

  /* Translators: This will be presented as the text of a link to the privacy policy */
  privacy_policy_link = g_strdup_printf ("<a href='https://location.services.mozilla.com/privacy'>%s</a>", _("Learn about what data is collected, and how it is used."));
  gtk_label_set_label (GTK_LABEL (self->privacy_policy_link), privacy_policy_link);

  g_settings_bind (self->location_settings,
                   LOCATION_ENABLED,
                   self->location_row,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);

  self->location_app_switches = g_hash_table_new_full (g_str_hash,
                                                       g_str_equal,
                                                       g_free,
                                                       g_object_unref);
  self->location_app_rows = g_hash_table_new_full (g_str_hash,
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
