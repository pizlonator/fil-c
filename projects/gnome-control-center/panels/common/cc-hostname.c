/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* cc-hostname.c
 *
 * Copyright 2023 Red Hat Inc
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Author(s):
 *   Felipe Borges <felipeborges@gnome.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "cc-hostname"

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "cc-hostname.h"
#include "hostname-helper.h"
#include "shell/cc-object-storage.h"

#define HOSTNAME_BUS_NAME "org.freedesktop.hostname1"
#define HOSTNAME_OBJECT_PATH "/org/freedesktop/hostname1"

struct _CcHostname
{
  GObject     parent_instance;

  GDBusProxy *proxy;
};

G_DEFINE_TYPE (CcHostname, cc_hostname, G_TYPE_OBJECT)

static void
cc_hostname_dispose (GObject *object)
{
  CcHostname *self = CC_HOSTNAME (object);

  g_clear_object (&self->proxy);

  G_OBJECT_CLASS (cc_hostname_parent_class)->dispose (object);
}

static void
cc_hostname_constructed (GObject *object)
{
  CcHostname *self = CC_HOSTNAME (object);
  g_autoptr(GError) error = NULL;

  self->proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                               G_DBUS_PROXY_FLAGS_NONE,
                                               NULL,
                                               HOSTNAME_BUS_NAME,
                                               HOSTNAME_OBJECT_PATH,
                                               HOSTNAME_BUS_NAME,
                                               NULL,
                                               &error);
  if (self->proxy == NULL) {
    g_critical ("Couldn't connect to hostnamed: %s", error->message);

    return;
  }
}

static void
cc_hostname_init (CcHostname *self)
{
}

static void
cc_hostname_class_init (CcHostnameClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = cc_hostname_constructed;
  object_class->dispose = cc_hostname_dispose;
}

CcHostname *
cc_hostname_get_default (void)
{
  g_autoptr(CcHostname) self = NULL;

  if (cc_object_storage_has_object (CC_OBJECT_HOSTNAME)) {
    self = cc_object_storage_get_object (CC_OBJECT_HOSTNAME);
  } else {
    self = g_object_new (CC_TYPE_HOSTNAME, NULL);
    cc_object_storage_add_object (CC_OBJECT_HOSTNAME, self);
  }

  return self;
}

gchar *
cc_hostname_get_property (CcHostname  *self,
                          const gchar *property)
{
  g_autoptr(GVariant) variant = NULL;
  g_autoptr(GVariant) inner = NULL;
  g_autoptr(GError) error = NULL;

  g_return_val_if_fail (CC_IS_HOSTNAME (self), NULL);
  g_return_val_if_fail (property != NULL, NULL);

  if (!self->proxy)
    return g_strdup ("");

  variant = g_dbus_proxy_get_cached_property (self->proxy, property);
  if (variant)
    return g_variant_dup_string (variant, NULL);

  variant = g_dbus_proxy_call_sync (self->proxy,
                                    "org.freedesktop.DBus.Properties.Get",
                                    g_variant_new ("(ss)", HOSTNAME_BUS_NAME, property),
                                    G_DBUS_CALL_FLAGS_NONE,
                                    -1,
                                    NULL,
                                    &error); 
  if (variant == NULL) {
    g_warning ("Failed to get property '%s': %s", property, error->message);

    return NULL;
  }

  g_variant_get (variant, "(v)", &inner);
  return g_variant_dup_string (inner, NULL);
}

gchar *
cc_hostname_get_display_hostname (CcHostname *self)
{
  g_autofree gchar *str = NULL;

  g_return_val_if_fail (CC_IS_HOSTNAME (self), NULL);

  str = cc_hostname_get_property (self, "PrettyHostname");
  /* Empty strings means that we need to fallback */
  if (str != NULL && *str == '\0')
     return cc_hostname_get_property (self, "Hostname");

  return g_steal_pointer (&str);
}

gchar *
cc_hostname_get_static_hostname (CcHostname *self)
{
  g_autofree gchar *str = NULL;

  g_return_val_if_fail (CC_IS_HOSTNAME (self), NULL);

  str = cc_hostname_get_property (self, "StaticHostname");
  /* Empty strings means that we need to fallback */
  if (str != NULL && *str == '\0')
     return cc_hostname_get_property (self, "Hostname");

  return g_steal_pointer (&str);
}

void
cc_hostname_set_hostname (CcHostname  *self,
                          const gchar *hostname)
{
  g_autofree gchar *static_hostname = NULL;
  g_autoptr(GVariant) pretty_result = NULL;
  g_autoptr(GVariant) static_result = NULL;
  g_autoptr(GError) pretty_error = NULL;
  g_autoptr(GError) static_error = NULL;

  g_return_if_fail (CC_IS_HOSTNAME (self));
  g_return_if_fail (hostname != NULL);

  g_debug ("Setting PrettyHostname to '%s'", hostname);
  pretty_result = g_dbus_proxy_call_sync (self->proxy,
                                          "SetPrettyHostname",
                                          g_variant_new ("(sb)", hostname, FALSE),
                                          G_DBUS_CALL_FLAGS_NONE,
                                          -1, NULL, &pretty_error);
  if (pretty_result == NULL)
    g_warning ("Could not set PrettyHostname: %s", pretty_error->message);

  /* Set the static hostname */
  static_hostname = pretty_hostname_to_static (hostname, FALSE);
  g_assert (hostname);

  g_debug ("Setting StaticHostname to '%s'", static_hostname);  
  static_result = g_dbus_proxy_call_sync (self->proxy,
                                          "SetStaticHostname",
                                          g_variant_new ("(sb)", static_hostname, FALSE),
                                          G_DBUS_CALL_FLAGS_NONE,
                                          -1, NULL, &static_error);
  if (static_result == NULL)
    g_warning ("Could not set StaticHostname: %s", static_error->message);
}

gchar *
cc_hostname_get_chassis_type (CcHostname *self)
{
  g_return_val_if_fail (CC_IS_HOSTNAME (self), NULL);

  return cc_hostname_get_property (self, "Chassis");
}

gboolean
cc_hostname_is_vm_chassis (CcHostname *self)
{
  g_autofree gchar *chassis_type = NULL;

  g_return_val_if_fail (CC_IS_HOSTNAME (self), FALSE);

  chassis_type = cc_hostname_get_chassis_type (self);
  return g_strcmp0 (chassis_type, "vm") == 0;
}
