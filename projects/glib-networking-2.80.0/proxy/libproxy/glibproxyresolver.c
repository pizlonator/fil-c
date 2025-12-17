/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * GIO - GLib Input, Output and Streaming Library
 *
 * Copyright 2010 Collabora, Ltd.
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
 *
 * Author: Nicolas Dufresne <nicolas.dufresne@collabora.co.uk>
 */

#include "config.h"

#include <proxy.h>
#include <stdlib.h>
#include <string.h>

#include "glibproxyresolver.h"

#include <glib.h>
#include <glib/gi18n-lib.h>

struct _GLibproxyResolver {
  GObject parent_instance;
  pxProxyFactory *factory;
};

static void g_libproxy_resolver_iface_init (GProxyResolverInterface *iface);

#ifdef GLIBPROXY_MODULE
static void
g_libproxy_resolver_class_finalize (GLibproxyResolverClass *klass)
{
}

G_DEFINE_DYNAMIC_TYPE_EXTENDED (GLibproxyResolver,
                                g_libproxy_resolver,
                                G_TYPE_OBJECT, G_TYPE_FLAG_FINAL,
                                G_IMPLEMENT_INTERFACE_DYNAMIC (G_TYPE_PROXY_RESOLVER,
                                                               g_libproxy_resolver_iface_init))
#else
G_DEFINE_FINAL_TYPE_WITH_CODE (GLibproxyResolver,
                               g_libproxy_resolver,
                               G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_PROXY_RESOLVER,
                                                      g_libproxy_resolver_iface_init))
#endif

static void
g_libproxy_resolver_finalize (GObject *object)
{
  GLibproxyResolver *resolver = G_LIBPROXY_RESOLVER (object);
  
  if (resolver->factory)
    {
      px_proxy_factory_free (resolver->factory);
      resolver->factory = NULL;
    }

  /* must chain up */
  G_OBJECT_CLASS (g_libproxy_resolver_parent_class)->finalize (object);
}

static gboolean
is_running_environment_proxy_test (void)
{
  return g_strcmp0 (g_getenv ("GIO_PROXY_TEST_NAME"), "environment") == 0;
}

static void
g_libproxy_resolver_init (GLibproxyResolver *resolver)
{
  if (!is_running_environment_proxy_test ())
    resolver->factory = px_proxy_factory_new ();
}

static gboolean
g_libproxy_resolver_is_supported (GProxyResolver *object)
{
  GLibproxyResolver *resolver = G_LIBPROXY_RESOLVER (object);
  return resolver->factory != NULL;
}

static gchar **
copy_proxies (gchar **proxies)
{
  gchar **copy;
  int len = 0;
  int i, j;
  GError *error = NULL;

  for (i = 0; proxies[i]; i++)
    {
      if (!strncmp ("socks://", proxies[i], 8))
        len += 3;
      else
        len++;
    }

  copy = g_new (gchar *, len + 1);
  for (i = j = 0; proxies[i]; i++, j++)
    {
      if (!g_uri_is_valid (proxies[i], G_URI_FLAGS_NONE, &error))
        {
          g_warning ("Received invalid URI %s from libproxy: %s", proxies[i], error->message);
          g_clear_error (&error);
          j--;
          continue;
        }

      if (!strncmp ("socks://", proxies[i], 8))
        {
          copy[j++] = g_strdup_printf ("socks5://%s", proxies[i] + 8);
          copy[j++] = g_strdup_printf ("socks4a://%s", proxies[i] + 8);
          copy[j] = g_strdup_printf ("socks4://%s", proxies[i] + 8);
        }
      else
        {
          copy[j] = g_strdup (proxies[i]);
        }
    }
  copy[j] = NULL;

  return copy;
}

static void
get_libproxy_proxies (GTask        *task,
                      gpointer      source_object,
                      gpointer      task_data,
                      GCancellable *cancellable)
{
  GLibproxyResolver *resolver = source_object;
  const gchar *uri = task_data;
  GError *error = NULL;
  gchar **proxies;

  if (g_task_return_error_if_cancelled (task))
    return;

  proxies = px_proxy_factory_get_proxies (resolver->factory, uri);
  if (proxies)
    {
      /* We always copy to be able to translate "socks" entry into
       * three entries ("socks5", "socks4a", "socks4").
       */
      g_task_return_pointer (task, copy_proxies (proxies), (GDestroyNotify) g_strfreev);
      px_proxy_factory_free_proxies (proxies);
    }
  else
    {
      g_set_error_literal (&error, G_IO_ERROR, G_IO_ERROR_FAILED,
                           _("Proxy resolver internal error."));
      g_task_return_error (task, error);
    }
}

static gchar **
g_libproxy_resolver_lookup (GProxyResolver  *iresolver,
                            const gchar     *uri,
                            GCancellable    *cancellable,
                            GError         **error)
{
  GLibproxyResolver *resolver = G_LIBPROXY_RESOLVER (iresolver);
  GTask *task;
  gchar **proxies;

  task = g_task_new (resolver, cancellable, NULL, NULL);
  g_task_set_name (task, "[glib-networking] g_libproxy_resolver_lookup");
  g_task_set_task_data (task, g_strdup (uri), g_free);
  g_task_set_return_on_cancel (task, TRUE);

  g_task_run_in_thread_sync (task, get_libproxy_proxies);
  proxies = g_task_propagate_pointer (task, error);
  g_object_unref (task);

  return proxies;
}

static void
g_libproxy_resolver_lookup_async (GProxyResolver      *resolver,
                                  const gchar         *uri,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
  GTask *task;

  task = g_task_new (resolver, cancellable, callback, user_data);
  g_task_set_source_tag (task, g_libproxy_resolver_lookup_async);
  g_task_set_name (task, "[glib-networking] g_libproxy_resolver_lookup_async");
  g_task_set_task_data (task, g_strdup (uri), g_free);
  g_task_set_return_on_cancel (task, TRUE);
  g_task_run_in_thread (task, get_libproxy_proxies);
  g_object_unref (task);
}

static gchar **
g_libproxy_resolver_lookup_finish (GProxyResolver     *resolver,
                                   GAsyncResult       *result,
                                   GError            **error)
{
  g_return_val_if_fail (g_task_is_valid (result, resolver), NULL);
  g_return_val_if_fail (g_task_get_source_tag (G_TASK (result)) == g_libproxy_resolver_lookup_async, NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
g_libproxy_resolver_class_init (GLibproxyResolverClass *resolver_class)
{
  GObjectClass *object_class;
  
  object_class = G_OBJECT_CLASS (resolver_class);
  object_class->finalize = g_libproxy_resolver_finalize;
}

static void
g_libproxy_resolver_iface_init (GProxyResolverInterface *iface)
{
  iface->is_supported = g_libproxy_resolver_is_supported;
  iface->lookup = g_libproxy_resolver_lookup;
  iface->lookup_async = g_libproxy_resolver_lookup_async;
  iface->lookup_finish = g_libproxy_resolver_lookup_finish;
}

#ifdef GLIBPROXY_MODULE
void
g_libproxy_resolver_register (GIOModule *module)
{
  g_libproxy_resolver_register_type (G_TYPE_MODULE (module));
  if (!module)
    g_io_extension_point_register (G_PROXY_RESOLVER_EXTENSION_POINT_NAME);
  g_io_extension_point_implement (G_PROXY_RESOLVER_EXTENSION_POINT_NAME,
                                  g_libproxy_resolver_get_type(),
                                  "libproxy",
                                  10);
}
#endif
