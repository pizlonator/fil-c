/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * GIO - GLib Input, Output and Streaming Library
 *
 * Copyright 2021 Igalia S.L.
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

#ifdef HAVE_RTLD_NOLOAD
#include <dlfcn.h>
#endif

#include "gtlshttp.h"

typedef gpointer SoupSession;
typedef gpointer SoupMessage;

static SoupSession *(*soup_session_new)(void);
static SoupMessage *(*soup_message_new)(const char *method, const char *uri);
static GInputStream *(*soup_session_send)(SoupSession *, SoupMessage *, GCancellable *, GError **);

static gsize libsoup_initialized;
static GModule *libsoup_module;

#define LIBSOUP_3_SONAME "libsoup-3.0.so.0"
#define LIBSOUP_2_SONAME "libsoup-2.4.so.1"

static void
init_libsoup (void)
{
  const char *libsoup_sonames[3] = { 0 };

  g_assert (g_module_supported ());

#ifdef HAVE_RTLD_NOLOAD
  {
    gpointer handle = NULL;

    /* In order to avoid causing conflicts we detect if libsoup 2 or 3 is loaded already.
    * If so use that. Otherwise we will try to load our own version to use preferring 3. */

    if ((handle = dlopen (LIBSOUP_3_SONAME, RTLD_NOW | RTLD_NOLOAD)))
      libsoup_sonames[0] = LIBSOUP_3_SONAME;
    else if ((handle = dlopen (LIBSOUP_2_SONAME, RTLD_NOW | RTLD_NOLOAD)))
      libsoup_sonames[0] = LIBSOUP_2_SONAME;
    else
      {
        libsoup_sonames[0] = LIBSOUP_3_SONAME;
        libsoup_sonames[1] = LIBSOUP_2_SONAME;
      }

    g_clear_pointer (&handle, dlclose);
  }
#else
#ifdef G_OS_WIN32
#ifdef _MSC_VER
  libsoup_sonames[0] = "soup-3.0-0.dll";
  libsoup_sonames[1] = "soup-2.4-1.dll";
#else
  libsoup_sonames[0] = "libsoup-3.0.dll";
  libsoup_sonames[1] = "libsoup-2.4.dll";
#endif
#else
  libsoup_sonames[0] = LIBSOUP_3_SONAME;
  libsoup_sonames[1] = LIBSOUP_2_SONAME;
#endif
#endif

  for (guint i = 0; libsoup_sonames[i]; i++)
    {
      libsoup_module = g_module_open (libsoup_sonames[i], G_MODULE_BIND_LAZY | G_MODULE_BIND_LOCAL);
      if (libsoup_module)
        {
          g_debug ("Loaded %s", g_module_name (libsoup_module));
          if (!g_module_symbol (libsoup_module, "soup_session_new", (gpointer *)&soup_session_new) ||
              !g_module_symbol (libsoup_module, "soup_message_new", (gpointer *)&soup_message_new) ||
              !g_module_symbol (libsoup_module, "soup_session_send", (gpointer *)&soup_session_send))
            {
              g_debug ("Failed to find all libsoup symbols");
              g_clear_pointer (&libsoup_module, g_module_close);
              continue;
            }
          break;
        }
    }

  if (!libsoup_module)
    g_debug ("Failed to load libsoup");
}

/**
 * g_tls_request_uri:
 * @uri: An HTTP URI to request
 * @cancellable: (nullable): A #GCancellable
 * @error: A #GError
 *
 * Synchronously requests an HTTP uri using the best available method.
 *
 * Note this is thread-safe.
 *
 * Returns: A #GInputStream of the response body or %NULL on failure
 */
GInputStream *
g_tls_request_uri (const char    *uri,
                   GCancellable  *cancellable,
                   GError       **error)
{
  GInputStream *istream = NULL;

  if (g_once_init_enter (&libsoup_initialized))
    {
      init_libsoup ();
      g_once_init_leave (&libsoup_initialized, TRUE);
    }

  if (libsoup_module)
    {
      SoupSession *session = soup_session_new ();
      SoupMessage *message = soup_message_new ("GET", uri);

      if (!message)
          g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "Failed to parse URI \"%s\"", uri);
      else
        {
          istream = soup_session_send (session, message, cancellable, error);
          g_object_unref (message);
        }

      g_object_unref (session);
    }
  else
    {
      GFile *file = g_file_new_for_uri (uri);
      istream = G_INPUT_STREAM (g_file_read (file, cancellable, error));
      g_object_unref (file);
    }

  return istream;
}
