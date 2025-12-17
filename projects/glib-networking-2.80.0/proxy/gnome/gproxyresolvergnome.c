/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * GIO - GLib Input, Output and Streaming Library
 *
 * Copyright 2010 Red Hat, Inc.
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

#include "gproxyresolvergnome.h"

#include <glib/gi18n-lib.h>
#include <gdesktop-enums.h>

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

/* We have to has-a GSimpleProxyResolver rather than is-a one,
 * because a dynamic type cannot reimplement an interface that
 * its parent also implements... for some reason.
 */

struct _GProxyResolverGnome {
  GObject parent_instance;

  GProxyResolver *base_resolver;

  GSettings *proxy_settings;
  GSettings *http_settings;
  GSettings *https_settings;
  GSettings *ftp_settings;
  GSettings *socks_settings;
  gboolean need_update;

  GDesktopProxyMode mode;
  gchar *autoconfig_url;
  gboolean use_same_proxy;

  GDBusProxy *pacrunner;

  GMutex lock;
};

static GProxyResolverInterface *g_proxy_resolver_gnome_parent_iface;

static void g_proxy_resolver_gnome_iface_init (GProxyResolverInterface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (GProxyResolverGnome,
                                g_proxy_resolver_gnome,
                                G_TYPE_OBJECT, G_TYPE_FLAG_FINAL,
                                G_IMPLEMENT_INTERFACE_DYNAMIC (G_TYPE_PROXY_RESOLVER,
                                                               g_proxy_resolver_gnome_iface_init))

static void
g_proxy_resolver_gnome_class_finalize (GProxyResolverGnomeClass *klass)
{
}

static void
gsettings_changed (GSettings   *settings,
                   const gchar *key,
                   gpointer     user_data)
{
  GProxyResolverGnome *resolver = user_data;

  g_mutex_lock (&resolver->lock);
  resolver->need_update = TRUE;
  g_mutex_unlock (&resolver->lock);
}

static void
g_proxy_resolver_gnome_finalize (GObject *object)
{
  GProxyResolverGnome *resolver = G_PROXY_RESOLVER_GNOME (object);

  if (resolver->proxy_settings)
    {
      g_signal_handlers_disconnect_by_func (resolver->proxy_settings,
                                            (gpointer)gsettings_changed,
                                            resolver);
      g_object_unref (resolver->proxy_settings);

      g_signal_handlers_disconnect_by_func (resolver->http_settings,
                                            (gpointer)gsettings_changed,
                                            resolver);
      g_object_unref (resolver->http_settings);

      g_signal_handlers_disconnect_by_func (resolver->https_settings,
                                            (gpointer)gsettings_changed,
                                            resolver);
      g_object_unref (resolver->https_settings);

      g_signal_handlers_disconnect_by_func (resolver->ftp_settings,
                                            (gpointer)gsettings_changed,
                                            resolver);
      g_object_unref (resolver->ftp_settings);

      g_signal_handlers_disconnect_by_func (resolver->socks_settings,
                                            (gpointer)gsettings_changed,
                                            resolver);
      g_object_unref (resolver->socks_settings);
    }

  g_clear_object (&resolver->base_resolver);
  g_clear_object (&resolver->pacrunner);

  g_free (resolver->autoconfig_url);

  g_mutex_clear (&resolver->lock);

  G_OBJECT_CLASS (g_proxy_resolver_gnome_parent_class)->finalize (object);
}

static void
g_proxy_resolver_gnome_init (GProxyResolverGnome *resolver)
{
  g_mutex_init (&resolver->lock);

  resolver->base_resolver = g_simple_proxy_resolver_new (NULL, NULL);

  resolver->proxy_settings = g_settings_new (GNOME_PROXY_SETTINGS_SCHEMA);
  g_signal_connect (resolver->proxy_settings, "changed",
                    G_CALLBACK (gsettings_changed), resolver);
  resolver->http_settings = g_settings_get_child (resolver->proxy_settings,
                                                  GNOME_PROXY_HTTP_CHILD_SCHEMA);
  g_signal_connect (resolver->http_settings, "changed",
                    G_CALLBACK (gsettings_changed), resolver);
  resolver->https_settings = g_settings_get_child (resolver->proxy_settings,
                                                   GNOME_PROXY_HTTPS_CHILD_SCHEMA);
  g_signal_connect (resolver->https_settings, "changed",
                    G_CALLBACK (gsettings_changed), resolver);
  resolver->ftp_settings = g_settings_get_child (resolver->proxy_settings,
                                                 GNOME_PROXY_FTP_CHILD_SCHEMA);
  g_signal_connect (resolver->ftp_settings, "changed",
                    G_CALLBACK (gsettings_changed), resolver);
  resolver->socks_settings = g_settings_get_child (resolver->proxy_settings,
                                                   GNOME_PROXY_SOCKS_CHILD_SCHEMA);
  g_signal_connect (resolver->socks_settings, "changed",
                    G_CALLBACK (gsettings_changed), resolver);

  resolver->need_update = TRUE;
}

/* called with lock held */
static void
update_settings (GProxyResolverGnome *resolver)
{
  GSimpleProxyResolver *simple = G_SIMPLE_PROXY_RESOLVER (resolver->base_resolver);
  gchar **ignore_hosts;
  gchar *host, *http_proxy, *proxy;
  guint port;
  GError *error = NULL;

  resolver->need_update = FALSE;

  g_free (resolver->autoconfig_url);
  g_simple_proxy_resolver_set_default_proxy (simple, NULL);
  g_simple_proxy_resolver_set_ignore_hosts (simple, NULL);
  g_simple_proxy_resolver_set_uri_proxy (simple, "http", NULL);
  g_simple_proxy_resolver_set_uri_proxy (simple, "https", NULL);
  g_simple_proxy_resolver_set_uri_proxy (simple, "ftp", NULL);

  resolver->mode =
    g_settings_get_enum (resolver->proxy_settings, GNOME_PROXY_MODE_KEY);
  resolver->autoconfig_url =
    g_settings_get_string (resolver->proxy_settings, GNOME_PROXY_AUTOCONFIG_URL_KEY);

  if (resolver->mode == G_DESKTOP_PROXY_MODE_AUTO && !resolver->pacrunner)
    {
      resolver->pacrunner =
        g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                       G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES |
                                       G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
                                       NULL,
                                       "org.gtk.GLib.PACRunner",
                                       "/org/gtk/GLib/PACRunner",
                                       "org.gtk.GLib.PACRunner",
                                       NULL, &error);
      if (error)
        {
          g_warning ("Could not start proxy autoconfiguration helper:"
                     "\n    %s\nProxy autoconfiguration will not work",
                     error->message);
        }
      g_clear_error (&error);
    }
  else if (resolver->mode != G_DESKTOP_PROXY_MODE_AUTO && resolver->pacrunner)
    {
      g_object_unref (resolver->pacrunner);
      resolver->pacrunner = NULL;
    }

  ignore_hosts =
    g_settings_get_strv (resolver->proxy_settings, GNOME_PROXY_IGNORE_HOSTS_KEY);
  g_simple_proxy_resolver_set_ignore_hosts (simple, ignore_hosts);
  g_strfreev (ignore_hosts);

  if (resolver->mode == G_DESKTOP_PROXY_MODE_AUTO)
    {
      /* We use the base_resolver to handle ignore_hosts in the AUTO case,
       * so we have to set a non-"direct://" default proxy so we can distinguish
       * the two cases.
       */
       g_simple_proxy_resolver_set_default_proxy (simple, "use-proxy:");
    }

  if (resolver->mode != G_DESKTOP_PROXY_MODE_MANUAL)
    return;

  host = g_settings_get_string (resolver->http_settings, GNOME_PROXY_HTTP_HOST_KEY);
  port = g_settings_get_int (resolver->http_settings, GNOME_PROXY_HTTP_PORT_KEY);
  if (host && *host)
    {
      if (g_settings_get_boolean (resolver->http_settings, GNOME_PROXY_HTTP_USE_AUTH_KEY))
        {
          gchar *user, *password;
          gchar *enc_user, *enc_password;

          user = g_settings_get_string (resolver->http_settings, GNOME_PROXY_HTTP_USER_KEY);
          enc_user = g_uri_escape_string (user, NULL, TRUE);
          g_free (user);
          password = g_settings_get_string (resolver->http_settings, GNOME_PROXY_HTTP_PASSWORD_KEY);
          enc_password = g_uri_escape_string (password, NULL, TRUE);
          g_free (password);

          http_proxy = g_strdup_printf ("http://%s:%s@%s:%u",
                                        enc_user, enc_password,
                                        host, port);
          g_free (enc_user);
          g_free (enc_password);
        }
      else
        http_proxy = g_strdup_printf ("http://%s:%u", host, port);

      if (g_uri_is_valid (http_proxy, G_URI_FLAGS_NONE, &error))
        {
          g_simple_proxy_resolver_set_uri_proxy (simple, "http", http_proxy);
          if (g_settings_get_boolean (resolver->proxy_settings, GNOME_PROXY_USE_SAME_PROXY_KEY))
            g_simple_proxy_resolver_set_default_proxy (simple, http_proxy);
        }
      else
        {
          g_warning ("Invalid HTTP proxy URI %s from GNOME settings: %s", http_proxy, error->message); 
          g_clear_pointer (&http_proxy, g_free);
          g_clear_error (&error);
        }
    }
  else
    http_proxy = NULL;
  g_free (host);

  host = g_settings_get_string (resolver->https_settings, GNOME_PROXY_HTTPS_HOST_KEY);
  port = g_settings_get_int (resolver->https_settings, GNOME_PROXY_HTTPS_PORT_KEY);
  if (host && *host)
    {
      proxy = g_strdup_printf ("http://%s:%u", host, port);
      if (g_uri_is_valid (proxy, G_URI_FLAGS_NONE, &error))
        {
          g_simple_proxy_resolver_set_uri_proxy (simple, "https", proxy);
        }
      else
        {
          g_warning ("Invalid HTTPS proxy URI %s from GNOME settings: %s", proxy, error->message);
          g_clear_error (&error);
        }
      g_free (proxy);
    }
  else if (http_proxy)
    g_simple_proxy_resolver_set_uri_proxy (simple, "https", http_proxy);
  g_free (host);

  host = g_settings_get_string (resolver->socks_settings, GNOME_PROXY_SOCKS_HOST_KEY);
  port = g_settings_get_int (resolver->socks_settings, GNOME_PROXY_SOCKS_PORT_KEY);
  if (host && *host)
    {
      proxy = g_strdup_printf ("socks://%s:%u", host, port);
      if (g_uri_is_valid (proxy, G_URI_FLAGS_NONE, &error))
        {
          g_simple_proxy_resolver_set_default_proxy (simple, proxy);
        }
      else
        {
          g_warning ("Invalid SOCKS proxy URI %s from GNOME settings: %s", proxy, error->message);
          g_clear_error (&error);
        }
      g_free (proxy);
    }
  g_free (host);

  g_free (http_proxy);

  host = g_settings_get_string (resolver->ftp_settings, GNOME_PROXY_FTP_HOST_KEY);
  port = g_settings_get_int (resolver->ftp_settings, GNOME_PROXY_FTP_PORT_KEY);
  if (host && *host)
    {
      proxy = g_strdup_printf ("ftp://%s:%u", host, port);
      if (g_uri_is_valid (proxy, G_URI_FLAGS_NONE, &error))
        {
          g_simple_proxy_resolver_set_uri_proxy (simple, "ftp", proxy);
        }
      else
        {
          g_warning ("Invalid FTP proxy URI %s from GNOME settings: %s", proxy, error->message);
          g_clear_error (&error);
        }
      g_free (proxy);
    }
  g_free (host);
}

static gboolean
g_proxy_resolver_gnome_is_supported (GProxyResolver *object)
{
  const char *desktops;

  desktops = g_getenv ("XDG_CURRENT_DESKTOP");
  if (!desktops)
    return FALSE;

  /* Remember that XDG_CURRENT_DESKTOP is a list of strings. Desktops that
   * pretend to be GNOME and want to use our proxy settings will list
   * themselves alongside GNOME. That's fine; they'll get our proxy settings.
   */
  return strstr (desktops, "GNOME") != NULL;
}

/* Threadsafely determines what to do with @uri; returns %FALSE if an
 * error occurs, %TRUE and an array of proxies if the mode is NONE or
 * MANUAL, or if @uri is covered by ignore-hosts, or %TRUE and a
 * (transfer-full) pacrunner and autoconfig url if the mode is AUTOMATIC.
 */
static gboolean
g_proxy_resolver_gnome_lookup_internal (GProxyResolverGnome   *resolver,
                                        const gchar           *uri,
                                        gchar               ***out_proxies,
                                        GDBusProxy           **out_pacrunner,
                                        gchar                **out_autoconfig_url,
                                        GCancellable          *cancellable,
                                        GError               **error)
{
  gchar **proxies = NULL;

  *out_proxies = NULL;
  *out_pacrunner = NULL;
  *out_autoconfig_url = NULL;

  g_mutex_lock (&resolver->lock);
  if (resolver->need_update)
    update_settings (resolver);

  proxies = g_proxy_resolver_lookup (resolver->base_resolver,
                                     uri, cancellable, error);
  if (!proxies)
    goto done;

  /* Parent class does ignore-host handling */
  if (!strcmp (proxies[0], "direct://") && !proxies[1])
    goto done;

  if (resolver->pacrunner)
    {
      g_clear_pointer (&proxies, g_strfreev);
      *out_pacrunner = g_object_ref (resolver->pacrunner);
      *out_autoconfig_url = g_strdup (resolver->autoconfig_url);
      goto done;
    }

 done:
  g_mutex_unlock (&resolver->lock);

  if (proxies)
    {
      *out_proxies = proxies;
      return TRUE;
    }
  else if (*out_pacrunner)
    return TRUE;
  else
    return FALSE;
}

static gchar **
g_proxy_resolver_gnome_lookup (GProxyResolver  *proxy_resolver,
                               const gchar     *uri,
                               GCancellable    *cancellable,
                               GError         **error)
{
  GProxyResolverGnome *resolver = G_PROXY_RESOLVER_GNOME (proxy_resolver);
  GDBusProxy *pacrunner;
  gchar **proxies, *autoconfig_url;

  if (!g_proxy_resolver_gnome_lookup_internal (resolver, uri,
                                               &proxies, &pacrunner, &autoconfig_url,
                                               cancellable, error))
    return NULL;

  if (pacrunner)
    {
      GVariant *vproxies;

      vproxies = g_dbus_proxy_call_sync (pacrunner,
                                         "Lookup",
                                         g_variant_new ("(ss)",
                                                        autoconfig_url,
                                                        uri),
                                         G_DBUS_CALL_FLAGS_NONE,
                                         -1,
                                         cancellable, error);
      if (vproxies)
        {
          g_variant_get (vproxies, "(^as)", &proxies);
          g_variant_unref (vproxies);
        }
      else
        proxies = NULL;

      g_object_unref (pacrunner);
      g_free (autoconfig_url);
    }

  return proxies;
}

static void
got_autoconfig_proxies (GObject      *source,
                        GAsyncResult *result,
                        gpointer      user_data)
{
  GTask *task = user_data;
  GVariant *vproxies;
  char **proxies;
  GError *error = NULL;

  vproxies = g_dbus_proxy_call_finish (G_DBUS_PROXY (source),
                                       result, &error);
  if (vproxies)
    {
      g_variant_get (vproxies, "(^as)", &proxies);
      g_task_return_pointer (task, proxies, (GDestroyNotify)g_strfreev);
      g_variant_unref (vproxies);
    }
  else
    g_task_return_error (task, error);
  g_object_unref (task);
}

static void
g_proxy_resolver_gnome_lookup_async (GProxyResolver      *proxy_resolver,
                                     const gchar         *uri,
                                     GCancellable        *cancellable,
                                     GAsyncReadyCallback  callback,
                                     gpointer             user_data)
{
  GProxyResolverGnome *resolver = G_PROXY_RESOLVER_GNOME (proxy_resolver);
  GTask *task;
  char **proxies, *autoconfig_url;
  GDBusProxy *pacrunner;
  GError *error = NULL;

  task = g_task_new (resolver, cancellable, callback, user_data);
  g_task_set_source_tag (task, g_proxy_resolver_gnome_lookup_async);
  g_task_set_name (task, "[glib-networking] g_proxy_resolver_gnome_lookup_async");

   if (!g_proxy_resolver_gnome_lookup_internal (resolver, uri,
                                                &proxies, &pacrunner, &autoconfig_url,
                                                cancellable, &error))
     {
       g_task_return_error (task, error);
       g_object_unref (task);
       return;
     }
   else if (proxies)
     {
       g_task_return_pointer (task, proxies, (GDestroyNotify)g_strfreev);
       g_object_unref (task);
       return;
     }

   g_dbus_proxy_call (pacrunner,
                      "Lookup",
                      g_variant_new ("(ss)",
                                     autoconfig_url,
                                     uri),
                      G_DBUS_CALL_FLAGS_NONE,
                      -1,
                      cancellable,
                      got_autoconfig_proxies,
                      task);
   g_object_unref (pacrunner);
   g_free (autoconfig_url);
}

static gchar **
g_proxy_resolver_gnome_lookup_finish (GProxyResolver  *resolver,
                                      GAsyncResult    *result,
                                      GError         **error)
{
  g_return_val_if_fail (g_task_is_valid (result, resolver), NULL);
  g_return_val_if_fail (g_task_get_source_tag (G_TASK (result)) == g_proxy_resolver_gnome_lookup_async, NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
g_proxy_resolver_gnome_class_init (GProxyResolverGnomeClass *resolver_class)
{
  GObjectClass *object_class;
  
  object_class = G_OBJECT_CLASS (resolver_class);
  object_class->finalize = g_proxy_resolver_gnome_finalize;
}

static void
g_proxy_resolver_gnome_iface_init (GProxyResolverInterface *iface)
{
  g_proxy_resolver_gnome_parent_iface = g_type_interface_peek_parent (iface);

  iface->is_supported = g_proxy_resolver_gnome_is_supported;
  iface->lookup = g_proxy_resolver_gnome_lookup;
  iface->lookup_async = g_proxy_resolver_gnome_lookup_async;
  iface->lookup_finish = g_proxy_resolver_gnome_lookup_finish;
}

void
g_proxy_resolver_gnome_register (GIOModule *module)
{
  g_proxy_resolver_gnome_register_type (G_TYPE_MODULE (module));
  if (!module)
    g_io_extension_point_register (G_PROXY_RESOLVER_EXTENSION_POINT_NAME);
  g_io_extension_point_implement (G_PROXY_RESOLVER_EXTENSION_POINT_NAME,
                                  g_proxy_resolver_gnome_get_type(),
                                  "gnome",
                                  80);
}
