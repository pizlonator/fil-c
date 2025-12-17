/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * GProxyResolverGnome tests
 *
 * Copyright 2011 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see
 * <http://www.gnu.org/licenses/>.
 */

#include <gio/gio.h>
#include <gdesktop-enums.h>

#include "common.c"

#define GNOME_PROXY_SETTINGS_SCHEMA       "org.gnome.system.proxy"
#define GNOME_PROXY_MODE_KEY              "mode"
#define GNOME_PROXY_AUTOCONFIG_URL_KEY    "autoconfig-url"
#define GNOME_PROXY_IGNORE_HOSTS_KEY      "ignore-hosts"
#define GNOME_PROXY_USE_SAME_PROXY_KEY    "use-same-proxy"

#define GNOME_PROXY_HTTP_CHILD_SCHEMA     "http"
#define GNOME_PROXY_HTTP_HOST_KEY         "host"
#define GNOME_PROXY_HTTP_PORT_KEY         "port"
#define GNOME_PROXY_HTTP_USE_AUTH_KEY     "use-authentication"
#define GNOME_PROXY_HTTP_USER_KEY         "authentication-user"
#define GNOME_PROXY_HTTP_PASSWORD_KEY     "authentication-password"

#define GNOME_PROXY_HTTPS_CHILD_SCHEMA    "https"
#define GNOME_PROXY_HTTPS_HOST_KEY        "host"
#define GNOME_PROXY_HTTPS_PORT_KEY        "port"

#define GNOME_PROXY_FTP_CHILD_SCHEMA      "ftp"
#define GNOME_PROXY_FTP_HOST_KEY          "host"
#define GNOME_PROXY_FTP_PORT_KEY          "port"

#define GNOME_PROXY_SOCKS_CHILD_SCHEMA    "socks"
#define GNOME_PROXY_SOCKS_HOST_KEY        "host"
#define GNOME_PROXY_SOCKS_PORT_KEY        "port"

static void
reset_proxy_settings (gpointer      fixture,
                      gconstpointer user_data)
{
  GSettings *settings, *child;

  settings = g_settings_new (GNOME_PROXY_SETTINGS_SCHEMA);
  g_settings_reset (settings, GNOME_PROXY_MODE_KEY);
  g_settings_reset (settings, GNOME_PROXY_USE_SAME_PROXY_KEY);

  child = g_settings_get_child (settings, GNOME_PROXY_HTTP_CHILD_SCHEMA);
  g_settings_reset (child, GNOME_PROXY_HTTP_HOST_KEY);
  g_settings_reset (child, GNOME_PROXY_HTTP_PORT_KEY);
  g_object_unref (child);

  child = g_settings_get_child (settings, GNOME_PROXY_HTTPS_CHILD_SCHEMA);
  g_settings_reset (child, GNOME_PROXY_HTTPS_HOST_KEY);
  g_settings_reset (child, GNOME_PROXY_HTTPS_PORT_KEY);
  g_object_unref (child);

  child = g_settings_get_child (settings, GNOME_PROXY_FTP_CHILD_SCHEMA);
  g_settings_reset (child, GNOME_PROXY_FTP_HOST_KEY);
  g_settings_reset (child, GNOME_PROXY_FTP_PORT_KEY);
  g_object_unref (child);

  child = g_settings_get_child (settings, GNOME_PROXY_SOCKS_CHILD_SCHEMA);
  g_settings_reset (child, GNOME_PROXY_SOCKS_HOST_KEY);
  g_settings_reset (child, GNOME_PROXY_SOCKS_PORT_KEY);
  g_object_unref (child);

  g_object_unref (settings);
}

static void
test_proxy_uri (gpointer      fixture,
                gconstpointer user_data)
{
  GSettings *settings, *child;

  settings = g_settings_new (GNOME_PROXY_SETTINGS_SCHEMA);
  g_settings_set_enum (settings, GNOME_PROXY_MODE_KEY, G_DESKTOP_PROXY_MODE_MANUAL);
  g_settings_set_boolean (settings, GNOME_PROXY_USE_SAME_PROXY_KEY, TRUE);

  child = g_settings_get_child (settings, GNOME_PROXY_HTTP_CHILD_SCHEMA);
  g_settings_set_string (child, GNOME_PROXY_HTTP_HOST_KEY, "proxy.example.com");
  g_settings_set_int (child, GNOME_PROXY_HTTP_PORT_KEY, 8080);
  g_object_unref (child);

  child = g_settings_get_child (settings, GNOME_PROXY_HTTPS_CHILD_SCHEMA);
  g_settings_set_string (child, GNOME_PROXY_HTTPS_HOST_KEY, "proxy-s.example.com");
  g_settings_set_int (child, GNOME_PROXY_HTTPS_PORT_KEY, 7070);
  g_object_unref (child);

  child = g_settings_get_child (settings, GNOME_PROXY_FTP_CHILD_SCHEMA);
  g_settings_set_string (child, GNOME_PROXY_FTP_HOST_KEY, "proxy-f.example.com");
  g_settings_set_int (child, GNOME_PROXY_FTP_PORT_KEY, 6060);
  g_object_unref (child);

  g_object_unref (settings);

  test_proxy_uri_common ();
}

static void
test_proxy_socks (gpointer      fixture,
                  gconstpointer user_data)
{
  GSettings *settings, *child;
  const gchar *ignore_hosts[2] = { "127.0.0.1", NULL };

  settings = g_settings_new (GNOME_PROXY_SETTINGS_SCHEMA);
  g_settings_set_enum (settings, GNOME_PROXY_MODE_KEY, G_DESKTOP_PROXY_MODE_MANUAL);
  g_settings_set (settings, GNOME_PROXY_IGNORE_HOSTS_KEY,
                  "@as", g_variant_new_strv (ignore_hosts, -1));

  child = g_settings_get_child (settings, GNOME_PROXY_SOCKS_CHILD_SCHEMA);
  g_settings_set_string (child, GNOME_PROXY_SOCKS_HOST_KEY, "proxy.example.com");
  g_settings_set_int (child, GNOME_PROXY_SOCKS_PORT_KEY, 1234);
  g_object_unref (child);
  g_object_unref (settings);

  test_proxy_socks_common ();
}

static void
test_proxy_ignore (gpointer      fixture,
                   gconstpointer user_data)
{
  GSettings *settings, *http;

  settings = g_settings_new (GNOME_PROXY_SETTINGS_SCHEMA);
  g_settings_set_enum (settings, GNOME_PROXY_MODE_KEY, G_DESKTOP_PROXY_MODE_MANUAL);
  g_settings_set (settings, GNOME_PROXY_IGNORE_HOSTS_KEY,
                  "@as", g_variant_new_strv (ignore_hosts, n_ignore_hosts));

  http = g_settings_get_child (settings, GNOME_PROXY_HTTP_CHILD_SCHEMA);
  g_settings_set_string (http, GNOME_PROXY_HTTP_HOST_KEY, "localhost");
  g_settings_set_int (http, GNOME_PROXY_HTTP_PORT_KEY, 8080);

  g_object_unref (http);
  g_object_unref (settings);

  test_proxy_ignore_common ();
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_setenv ("GIO_USE_PROXY_RESOLVER", "gnome", TRUE);
  g_setenv ("GSETTINGS_BACKEND", "memory", TRUE);
  g_setenv ("XDG_CURRENT_DESKTOP", "GNOME", TRUE);

  g_test_add_vtable ("/proxy/gnome/uri", 0, NULL,
                     reset_proxy_settings, test_proxy_uri, NULL);
  g_test_add_vtable ("/proxy/gnome/socks", 0, NULL,
                     reset_proxy_settings, test_proxy_socks, NULL);
  g_test_add_vtable ("/proxy/gnome/ignore", 0, NULL,
                     reset_proxy_settings, test_proxy_ignore, NULL);

  return g_test_run();
}
