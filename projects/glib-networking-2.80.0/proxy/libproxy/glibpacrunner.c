/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * GIO - GLib Input, Output and Streaming Library
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

#include "config.h"

#include <stdlib.h>

#include <gio/gio.h>
#include "glibproxyresolver.h"

static const gchar introspection_xml[] =
  "<node>"
  "  <interface name='org.gtk.GLib.PACRunner'>"
  "    <method name='Lookup'>"
  "      <arg type='s' name='pac_url' direction='in'/>"
  "      <arg type='s' name='lookup_url' direction='in'/>"
  "      <arg type='as' name='proxies' direction='out'/>"
  "    </method>"
  "  </interface>"
  "</node>";

static GProxyResolver *resolver;
static GMainLoop *loop;

static void
got_proxies (GObject      *source,
             GAsyncResult *result,
             gpointer      user_data)
{
  GDBusMethodInvocation *invocation = user_data;
  gchar **proxies;
  GError *error = NULL;

  proxies = g_proxy_resolver_lookup_finish (resolver, result, &error);
  if (error)
    g_dbus_method_invocation_take_error (invocation, error);
  else
    {
      g_dbus_method_invocation_return_value (invocation,
                                             g_variant_new ("(^as)", proxies));
      g_strfreev (proxies);
    }
}

static void
handle_method_call (GDBusConnection       *connection,
                    const gchar           *sender,
                    const gchar           *object_path,
                    const gchar           *interface_name,
                    const gchar           *method_name,
                    GVariant              *parameters,
                    GDBusMethodInvocation *invocation,
                    gpointer               user_data)
{
  const gchar *pac_url, *lookup_url;

  g_variant_get (parameters, "(&s&s)", &pac_url, &lookup_url);

  if (!g_ascii_strncasecmp (pac_url, "http", 4) ||
      !g_ascii_strncasecmp (pac_url, "file:", 5))
    {
      gchar *libproxy_url = g_strdup_printf ("pac+%s", pac_url);
      g_setenv ("http_proxy", libproxy_url, TRUE);
      g_free (libproxy_url);
    }
  else
    g_setenv ("http_proxy", "wpad://", TRUE);

  g_proxy_resolver_lookup_async (resolver, lookup_url,
                                 NULL, got_proxies, invocation);
}

static const GDBusInterfaceVTable interface_vtable =
  {
    handle_method_call,
    NULL,
    NULL
  };

static void
on_bus_acquired (GDBusConnection *connection,
                 const gchar     *name,
                 gpointer         user_data)
{
  GDBusNodeInfo *introspection_data;
  GError *error = NULL;

  introspection_data = g_dbus_node_info_new_for_xml (introspection_xml, NULL);
  g_dbus_connection_register_object (connection,
                                     "/org/gtk/GLib/PACRunner",
                                     introspection_data->interfaces[0],
                                     &interface_vtable,
                                     NULL,
                                     NULL,
                                     &error);
  if (error)
    g_error ("Could not register server: %s", error->message);
}

static void
on_name_acquired (GDBusConnection *connection,
                  const gchar     *name,
                  gpointer         user_data)
{
}

static void
on_name_lost (GDBusConnection *connection,
              const gchar     *name,
              gpointer         user_data)
{
  g_main_loop_quit (loop);
}

int
main (int argc, char *argv[])
{
  int owner_id;

  /* Unset variables that would make libproxy try to use gconf or ksettings */
  g_unsetenv ("GNOME_DESKTOP_SESSION_ID");
  g_unsetenv ("DESKTOP_SESSION");
  g_unsetenv ("KDE_FULL_SESSION");

  /* Unset variables that libproxy would look at if it were smarter, and which
   * it might possibly look at in the future. Just covering our bases. */
  g_unsetenv ("XDG_CURRENT_DESKTOP");

  /* Unset static proxy settings */
  g_unsetenv ("http_proxy");
  g_unsetenv ("HTTP_PROXY");
  g_unsetenv ("https_proxy");
  g_unsetenv ("HTTPS_PROXY");
  g_unsetenv ("ftp_proxy");
  g_unsetenv ("FTP_PROXY");
  g_unsetenv ("no_proxy");
  g_unsetenv ("NO_PROXY");

  resolver = g_object_new (G_TYPE_LIBPROXY_RESOLVER, NULL);

  owner_id = g_bus_own_name (G_BUS_TYPE_SESSION,
                             "org.gtk.GLib.PACRunner",
                             G_BUS_NAME_OWNER_FLAGS_NONE,
                             on_bus_acquired,
                             on_name_acquired,
                             on_name_lost,
                             NULL,
                             NULL);

  loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (loop);

  g_bus_unown_name (owner_id);
  return 0;
}
