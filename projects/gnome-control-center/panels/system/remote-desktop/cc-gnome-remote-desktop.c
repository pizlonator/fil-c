/*
 * Copyright (C) 2018 Red Hat, Inc.
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

#include "config.h"

#include "cc-gnome-remote-desktop.h"

const SecretSchema *
cc_grd_rdp_credentials_get_schema (void)
{
  static const SecretSchema grd_rdp_credentials_schema = {
    .name = "org.gnome.RemoteDesktop.RdpCredentials",
    .flags = SECRET_SCHEMA_NONE,
    .attributes = {
      { "credentials", SECRET_SCHEMA_ATTRIBUTE_STRING },
      { "NULL", 0 },
    },
  };

  return &grd_rdp_credentials_schema;
}

void
cc_grd_store_rdp_credentials (const gchar  *username,
                              const gchar  *password,
                              GCancellable *cancellable)
{
  GVariantBuilder builder;
  g_autofree gchar *credentials = NULL;

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a{sv}"));
  g_variant_builder_add (&builder, "{sv}", "username", g_variant_new_string (username));
  g_variant_builder_add (&builder, "{sv}", "password", g_variant_new_string (password));
  credentials = g_variant_print (g_variant_builder_end (&builder), TRUE);

  secret_password_store_sync (cc_grd_rdp_credentials_get_schema (),
                              SECRET_COLLECTION_DEFAULT,
                              "GNOME Remote Desktop RDP credentials",
                              credentials,
                              NULL, NULL,
                              NULL);
}

gchar *
cc_grd_lookup_rdp_username (GCancellable *cancellable)
{
  g_autoptr(GError) error = NULL;
  gchar *username = NULL;
  g_autofree gchar *secret = NULL;
  g_autoptr(GVariant) variant = NULL;

  secret = secret_password_lookup_sync (cc_grd_rdp_credentials_get_schema (),
                                        cancellable, &error,
                                        NULL);
  if (error)
    {
      g_warning ("Failed to get username: %s", error->message);
      return NULL;
    }

  if (secret == NULL)
    {
      g_debug ("No RDP credentials available");
      return NULL;
    }

  variant = g_variant_parse (NULL, secret, NULL, NULL, &error);
  if (variant == NULL)
    {
      g_warning ("Invalid credentials format in the keyring: %s", error->message);
      return NULL;
    }

  g_variant_lookup (variant, "username", "s", &username);

  return username;
}

gchar *
cc_grd_lookup_rdp_password (GCancellable *cancellable)
{
  g_autoptr(GError) error = NULL;
  g_autofree gchar *secret = NULL;
  gchar *password = NULL;
  g_autoptr(GVariant) variant = NULL;

  secret = secret_password_lookup_sync (cc_grd_rdp_credentials_get_schema (),
                                          cancellable, &error,
                                          NULL);
  if (error)
    {
      g_warning ("Failed to get password: %s", error->message);
      return NULL;
    }

  if (secret == NULL)
    {
      g_debug ("No RDP credentials available");
      return NULL;
    }

  variant = g_variant_parse (NULL, secret, NULL, NULL, &error);
  if (variant == NULL)
    {
      g_warning ("Invalid credentials format in the keyring: %s", error->message);
      return NULL;
    }

  g_variant_lookup (variant, "password", "s", &password);

  return password;
}
