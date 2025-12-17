/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * GIO - GLib Input, Output and Streaming Library
 *
 * Copyright 2010 Red Hat, Inc
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
 * In addition, when the library is used with OpenSSL, a special
 * exception applies. Refer to the LICENSE_EXCEPTION file for details.
 */

#include "config.h"
#include "glib.h"

#include <errno.h>
#include <string.h>

#include <gnutls/gnutls.h>

#include "gtlsbackend-gnutls.h"
#include "gtlscertificate-gnutls.h"
#include "gtlsclientconnection-gnutls.h"
#include "gtlsfiledatabase-gnutls.h"
#include "gtlsserverconnection-gnutls.h"

struct _GTlsBackendGnutls
{
  GObject parent_instance;

  GMutex mutex;
  GTlsDatabase *default_database;
};

static void g_tls_backend_gnutls_interface_init (GTlsBackendInterface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (GTlsBackendGnutls, g_tls_backend_gnutls, G_TYPE_OBJECT, G_TYPE_FLAG_FINAL,
                                G_IMPLEMENT_INTERFACE_DYNAMIC (G_TYPE_TLS_BACKEND,
                                                               g_tls_backend_gnutls_interface_init);)

#ifdef GTLS_GNUTLS_DEBUG
static void
gtls_log_func (int level, const char *msg)
{
  g_print ("GTLS: %s", msg);
}
#endif

static gpointer
gtls_gnutls_init (gpointer data)
{
  GTypePlugin *plugin;

  gnutls_global_init ();

#ifdef GTLS_GNUTLS_DEBUG
  gnutls_global_set_log_function (gtls_log_func);
  gnutls_global_set_log_level (9);
#endif

  /* Leak the module to keep it from being unloaded. */
  plugin = g_type_get_plugin (G_TYPE_TLS_BACKEND_GNUTLS);
  if (plugin)
    g_type_plugin_use (plugin);
  return NULL;
}

GNUTLS_SKIP_GLOBAL_INIT

static GOnce gnutls_inited = G_ONCE_INIT;

static void
g_tls_backend_gnutls_init (GTlsBackendGnutls *backend)
{
  /* Once we call gtls_gnutls_init(), we can't allow the module to be
   * unloaded (since if gnutls gets unloaded but gcrypt doesn't, then
   * gcrypt will have dangling pointers to gnutls's mutex functions).
   * So we initialize it from here rather than at class init time so
   * that it doesn't happen unless the app is actually using TLS (as
   * opposed to just calling g_io_modules_scan_all_in_directory()).
   */
  g_once (&gnutls_inited, gtls_gnutls_init, NULL);

  g_mutex_init (&backend->mutex);
}

static void
g_tls_backend_gnutls_finalize (GObject *object)
{
  GTlsBackendGnutls *backend = G_TLS_BACKEND_GNUTLS (object);

  g_clear_object (&backend->default_database);
  g_mutex_clear (&backend->mutex);

  G_OBJECT_CLASS (g_tls_backend_gnutls_parent_class)->finalize (object);
}

static void
g_tls_backend_gnutls_class_init (GTlsBackendGnutlsClass *backend_class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (backend_class);

  gobject_class->finalize = g_tls_backend_gnutls_finalize;
}

static void
g_tls_backend_gnutls_class_finalize (GTlsBackendGnutlsClass *backend_class)
{
}

static GTlsDatabase *
g_tls_backend_gnutls_get_default_database (GTlsBackend *backend)
{
  GTlsBackendGnutls *self = G_TLS_BACKEND_GNUTLS (backend);
  GTlsDatabase *result;
  GError *error = NULL;

  g_mutex_lock (&self->mutex);

  if (self->default_database)
    {
      result = g_object_ref (self->default_database);
    }
  else
    {
      result = G_TLS_DATABASE (g_tls_database_gnutls_new (&error));
      if (error)
        {
          g_warning ("Failed to load TLS database: %s", error->message);
          g_clear_error (&error);
        }
      else
        {
          g_assert (result);
          self->default_database = g_object_ref (result);
        }
    }

  g_mutex_unlock (&self->mutex);

  return result;
}

static void
g_tls_backend_gnutls_interface_init (GTlsBackendInterface *iface)
{
  iface->get_certificate_type       = g_tls_certificate_gnutls_get_type;
  iface->get_client_connection_type = g_tls_client_connection_gnutls_get_type;
  iface->get_server_connection_type = g_tls_server_connection_gnutls_get_type;
  iface->get_file_database_type =     g_tls_file_database_gnutls_get_type;
  iface->get_default_database =       g_tls_backend_gnutls_get_default_database;
  iface->get_dtls_client_connection_type = g_tls_client_connection_gnutls_get_type;
  iface->get_dtls_server_connection_type = g_tls_server_connection_gnutls_get_type;
}

void
g_tls_backend_gnutls_register (GIOModule *module)
{
  g_tls_backend_gnutls_register_type (G_TYPE_MODULE (module));
  if (!module)
    g_io_extension_point_register (G_TLS_BACKEND_EXTENSION_POINT_NAME);
  g_io_extension_point_implement (G_TLS_BACKEND_EXTENSION_POINT_NAME,
                                  g_tls_backend_gnutls_get_type (),
                                  "gnutls",
                                  0);
}
