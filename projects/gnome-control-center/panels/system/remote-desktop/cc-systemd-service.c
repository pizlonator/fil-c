/*
 * Copyright (C) 2023 Intel, Inc
 * Copyright (C) 2023 Red Hat, Inc
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
 * Author: Thomas Wood <thomas.wood@intel.com>
 *
 */

#include "cc-systemd-service.h"

gboolean
cc_is_service_active (const char  *service,
                      GBusType     bus_type)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GDBusConnection) connection = NULL;
  g_autoptr(GVariant) unit_path_variant = NULL;
  g_autofree char *unit_path = NULL;
  g_autoptr(GVariant) active_state_prop = NULL;
  g_autoptr(GVariant) active_state_variant = NULL;
  const char *active_state = NULL;
  g_autoptr(GVariant) unit_state_prop = NULL;
  g_autoptr(GVariant) unit_state_variant = NULL;
  const char *unit_state = NULL;

  connection = g_bus_get_sync (bus_type, NULL, &error);
  if (!connection)
    {
      g_warning ("Failed connecting to D-Bus system bus: %s", error->message);
      return FALSE;
    }

  unit_path_variant =
    g_dbus_connection_call_sync (connection,
                                 "org.freedesktop.systemd1",
                                 "/org/freedesktop/systemd1",
                                 "org.freedesktop.systemd1.Manager",
                                 "GetUnit",
                                 g_variant_new ("(s)",
                                                service),
                                 (GVariantType *) "(o)",
                                 G_DBUS_CALL_FLAGS_NONE,
                                 -1,
                                 NULL,
                                 NULL);

  if (!unit_path_variant)
    return FALSE;
  g_variant_get_child (unit_path_variant, 0, "o", &unit_path);

  active_state_prop =
    g_dbus_connection_call_sync (connection,
                                 "org.freedesktop.systemd1",
                                 unit_path,
                                 "org.freedesktop.DBus.Properties",
                                 "Get",
                                 g_variant_new ("(ss)",
                                                "org.freedesktop.systemd1.Unit",
                                                "ActiveState"),
                                 (GVariantType *) "(v)",
                                 G_DBUS_CALL_FLAGS_NONE,
                                 -1,
                                 NULL,
                                 &error);

  if (!active_state_prop)
    {
      g_warning ("Failed to get service active state: %s", error->message);
      return FALSE;
    }
  g_variant_get_child (active_state_prop, 0, "v", &active_state_variant);
  active_state = g_variant_get_string (active_state_variant, NULL);

  if (g_strcmp0 (active_state, "active") != 0 &&
      g_strcmp0 (active_state, "activating") != 0)
    return FALSE;

  unit_state_prop =
    g_dbus_connection_call_sync (connection,
                                 "org.freedesktop.systemd1",
                                 unit_path,
                                 "org.freedesktop.DBus.Properties",
                                 "Get",
                                 g_variant_new ("(ss)",
                                                "org.freedesktop.systemd1.Unit",
                                                "UnitFileState"),
                                 (GVariantType *) "(v)",
                                 G_DBUS_CALL_FLAGS_NONE,
                                 -1,
                                 NULL,
                                 &error);

  if (!unit_state_prop)
    {
      g_warning ("Failed to get service active state: %s", error->message);
      return FALSE;
    }
  g_variant_get_child (unit_state_prop, 0, "v", &unit_state_variant);
  unit_state = g_variant_get_string (unit_state_variant, NULL);

  if (g_strcmp0 (unit_state, "enabled") == 0 ||
      g_strcmp0 (unit_state, "static") == 0)
    return TRUE;
  else
    return FALSE;
}

gboolean
cc_enable_service (const char  *service,
                   GBusType     bus_type,
                   GError     **error)
{
  g_autoptr(GDBusConnection) connection = NULL;
  g_autoptr(GVariant) start_result = NULL;
  g_autoptr(GVariant) enable_result = NULL;
  const char *service_list[] = { service, NULL };

  connection = g_bus_get_sync (bus_type, NULL, error);
  if (!connection)
    {
      g_prefix_error_literal (error, "Failed connecting to D-Bus system bus: ");
      return FALSE;
    }

  start_result = g_dbus_connection_call_sync (connection,
                                              "org.freedesktop.systemd1",
                                              "/org/freedesktop/systemd1",
                                              "org.freedesktop.systemd1.Manager",
                                              "StartUnit",
                                              g_variant_new ("(ss)",
                                                             service,
                                                             "replace"),
                                              (GVariantType *) "(o)",
                                              G_DBUS_CALL_FLAGS_NONE,
                                              -1,
                                              NULL,
                                              error);

  if (!start_result)
    {
      g_prefix_error_literal (error, "Failed to start service: ");
      return FALSE;
    }

  enable_result = g_dbus_connection_call_sync (connection,
                                               "org.freedesktop.systemd1",
                                               "/org/freedesktop/systemd1",
                                               "org.freedesktop.systemd1.Manager",
                                               "EnableUnitFiles",
                                               g_variant_new ("(^asbb)",
                                                              service_list,
                                                              FALSE, FALSE),
                                               (GVariantType *) "(ba(sss))",
                                               G_DBUS_CALL_FLAGS_NONE,
                                               -1,
                                               NULL,
                                               error);

  if (!enable_result)
    {
      g_prefix_error_literal (error, "Failed to enable service: ");
      return FALSE;
    }

  return TRUE;
}

gboolean
cc_disable_service (const char  *service,
                    GBusType     bus_type,
                    GError     **error)
{
  g_autoptr(GDBusConnection) connection = NULL;
  g_autoptr(GVariant) stop_result = NULL;
  g_autoptr(GVariant) disable_result = NULL;
  const char *service_list[] = { service, NULL };

  connection = g_bus_get_sync (bus_type, NULL, error);
  if (!connection)
    {
      g_prefix_error_literal (error, "Failed connecting to D-Bus system bus: ");
      return FALSE;
    }

  stop_result = g_dbus_connection_call_sync (connection,
                                             "org.freedesktop.systemd1",
                                             "/org/freedesktop/systemd1",
                                             "org.freedesktop.systemd1.Manager",
                                             "StopUnit",
                                             g_variant_new ("(ss)", service, "replace"),
                                             (GVariantType *) "(o)",
                                             G_DBUS_CALL_FLAGS_NONE,
                                             -1,
                                             NULL,
                                             error);
  if (!stop_result)
    {
      g_prefix_error_literal (error, "Failed to stop service: ");
      return FALSE;
    }

  disable_result = g_dbus_connection_call_sync (connection,
                                                "org.freedesktop.systemd1",
                                                "/org/freedesktop/systemd1",
                                                "org.freedesktop.systemd1.Manager",
                                                "DisableUnitFiles",
                                                g_variant_new ("(^asb)", service_list, FALSE,
                                                               FALSE),
                                                (GVariantType *) "(a(sss))",
                                                G_DBUS_CALL_FLAGS_NONE,
                                                -1,
                                                NULL,
                                                error);

  if (!stop_result)
    {
      g_prefix_error_literal (error, "Failed to disable service: ");
      return FALSE;
    }

  return TRUE;
}
