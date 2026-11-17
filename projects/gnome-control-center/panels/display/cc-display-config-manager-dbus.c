/*
 * Copyright (C) 2017  Red Hat, Inc.
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include "cc-display-config-dbus.h"
#include "cc-display-config-manager-dbus.h"

#include <gio/gio.h>

struct _CcDisplayConfigManagerDBus
{
  CcDisplayConfigManager parent_instance;

  GCancellable *cancellable;
  GDBusConnection *connection;
  guint monitors_changed_id;

  GVariant *current_state;

  gboolean apply_allowed;
  gboolean night_light_supported;
};

G_DEFINE_TYPE (CcDisplayConfigManagerDBus,
               cc_display_config_manager_dbus,
               CC_TYPE_DISPLAY_CONFIG_MANAGER)

static CcDisplayConfig *
cc_display_config_manager_dbus_get_current (CcDisplayConfigManager *pself)
{
  CcDisplayConfigManagerDBus *self = CC_DISPLAY_CONFIG_MANAGER_DBUS (pself);

  if (!self->current_state)
    return NULL;

  return g_object_new (CC_TYPE_DISPLAY_CONFIG_DBUS,
                       "state", self->current_state,
                       "connection", self->connection, NULL);
}

static void
got_current_state (GObject      *object,
                   GAsyncResult *result,
                   gpointer      data)
{
  CcDisplayConfigManagerDBus *self;
  GVariant *variant;
  g_autoptr(GError) error = NULL;

  variant = g_dbus_connection_call_finish (G_DBUS_CONNECTION (object),
                                           result, &error);
  if (!variant)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          self = CC_DISPLAY_CONFIG_MANAGER_DBUS (data);
          g_clear_pointer (&self->current_state, g_variant_unref);
          _cc_display_config_manager_emit_changed (CC_DISPLAY_CONFIG_MANAGER (data));
          g_warning ("Error calling GetCurrentState: %s", error->message);
        }
      return;
    }

  self = CC_DISPLAY_CONFIG_MANAGER_DBUS (data);
  g_clear_pointer (&self->current_state, g_variant_unref);
  self->current_state = variant;

  _cc_display_config_manager_emit_changed (CC_DISPLAY_CONFIG_MANAGER (self));
}

static void
get_current_state (CcDisplayConfigManagerDBus *self)
{
  g_dbus_connection_call (self->connection,
                          "org.gnome.Mutter.DisplayConfig",
                          "/org/gnome/Mutter/DisplayConfig",
                          "org.gnome.Mutter.DisplayConfig",
                          "GetCurrentState",
                          NULL,
                          NULL,
                          G_DBUS_CALL_FLAGS_NO_AUTO_START,
                          -1,
                          self->cancellable,
                          got_current_state,
                          self);
}

static void
monitors_changed (GDBusConnection *connection,
                  const gchar     *sender_name,
                  const gchar     *object_path,
                  const gchar     *interface_name,
                  const gchar     *signal_name,
                  GVariant        *parameters,
                  gpointer         data)
{
  CcDisplayConfigManagerDBus *self = CC_DISPLAY_CONFIG_MANAGER_DBUS (data);
  get_current_state (self);
}

static void
bus_gotten (GObject      *object,
            GAsyncResult *result,
            gpointer      data)
{
  CcDisplayConfigManagerDBus *self;
  GDBusConnection *connection;
  g_autoptr(GError) error = NULL;
  g_autoptr(GDBusProxy) proxy = NULL;
  g_autoptr(GVariant) variant = NULL;

  connection = g_bus_get_finish (result, &error);
  if (!connection)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          _cc_display_config_manager_emit_changed (CC_DISPLAY_CONFIG_MANAGER (data));
          g_warning ("Error obtaining DBus connection: %s", error->message);
        }
      return;
    }

  self = CC_DISPLAY_CONFIG_MANAGER_DBUS (data);
  self->connection = connection;
  self->monitors_changed_id =
    g_dbus_connection_signal_subscribe (self->connection,
                                        "org.gnome.Mutter.DisplayConfig",
                                        "org.gnome.Mutter.DisplayConfig",
                                        "MonitorsChanged",
                                        "/org/gnome/Mutter/DisplayConfig",
                                        NULL,
                                        G_DBUS_SIGNAL_FLAGS_NONE,
                                        monitors_changed,
                                        self,
                                        NULL);

  proxy = g_dbus_proxy_new_sync (self->connection,
                                 G_DBUS_PROXY_FLAGS_NONE,
                                 NULL,
                                 "org.gnome.Mutter.DisplayConfig",
                                 "/org/gnome/Mutter/DisplayConfig",
                                 "org.gnome.Mutter.DisplayConfig",
                                 NULL,
                                 &error);
  if (!proxy)
    {
      g_warning ("Failed to create D-Bus proxy to \"org.gnome.Mutter.DisplayConfig\": %s",
                 error->message);
      return;
    }

  variant = g_dbus_proxy_get_cached_property (proxy, "ApplyMonitorsConfigAllowed");
  if (variant)
    self->apply_allowed = g_variant_get_boolean (variant);
  else
    g_warning ("Missing property 'ApplyMonitorsConfigAllowed' on DisplayConfig API");

  variant = g_dbus_proxy_get_cached_property (proxy, "NightLightSupported");
  if (variant)
    self->night_light_supported = g_variant_get_boolean (variant);
  else
    g_warning ("Missing property 'NightLightSupported' on DisplayConfig API");

  get_current_state (self);
}

static void
cc_display_config_manager_dbus_init (CcDisplayConfigManagerDBus *self)
{
  self->apply_allowed = TRUE;
  self->night_light_supported = TRUE;
  self->cancellable = g_cancellable_new ();
  g_bus_get (G_BUS_TYPE_SESSION, self->cancellable, bus_gotten, self);
}

static void
cc_display_config_manager_dbus_finalize (GObject *object)
{
  CcDisplayConfigManagerDBus *self = CC_DISPLAY_CONFIG_MANAGER_DBUS (object);

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

  if (self->monitors_changed_id && self->connection)
    g_dbus_connection_signal_unsubscribe (self->connection,
                                          self->monitors_changed_id);
  g_clear_object (&self->connection);
  g_clear_pointer (&self->current_state, g_variant_unref);

  G_OBJECT_CLASS (cc_display_config_manager_dbus_parent_class)->finalize (object);
}

static gboolean
cc_display_config_manager_dbus_get_apply_allowed (CcDisplayConfigManager *pself)
{
  CcDisplayConfigManagerDBus *self = CC_DISPLAY_CONFIG_MANAGER_DBUS (pself);

  return self->apply_allowed;
}

static gboolean
cc_display_config_manager_dbus_get_night_light_supported (CcDisplayConfigManager *pself)
{
  CcDisplayConfigManagerDBus *self = CC_DISPLAY_CONFIG_MANAGER_DBUS (pself);

  return self->night_light_supported;
}

static void
cc_display_config_manager_dbus_class_init (CcDisplayConfigManagerDBusClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  CcDisplayConfigManagerClass *parent_class = CC_DISPLAY_CONFIG_MANAGER_CLASS (klass);

  gobject_class->finalize = cc_display_config_manager_dbus_finalize;

  parent_class->get_current = cc_display_config_manager_dbus_get_current;
  parent_class->get_apply_allowed = cc_display_config_manager_dbus_get_apply_allowed;
  parent_class->get_night_light_supported = cc_display_config_manager_dbus_get_night_light_supported;
}

CcDisplayConfigManager *
cc_display_config_manager_dbus_new (void)
{
  return g_object_new (CC_TYPE_DISPLAY_CONFIG_MANAGER_DBUS, NULL);
}
