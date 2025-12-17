/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * GIO - GLib Input, Output and Streaming Library
 *
 * Copyright 2009 Red Hat, Inc
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

#include <gio/gio.h>
#include <glib.h>
#include <glib/gprintf.h>
#include <stdarg.h>

#include "gtlslog.h"

void g_tls_log (GLogLevelFlags  level,
                gpointer        conn,
                const gchar    *file,
                const gchar    *line,
                const gchar    *func,
                const gchar    *format,
                ...)
{
  if (level < G_LOG_LEVEL_DEBUG || ENABLE_DEBUG_LOGS)
    {
      gchar *header = NULL;
      gchar *message = NULL;
      gchar *thread = NULL;
      va_list args;
      int ret;

      va_start (args, format);
      ret = g_vasprintf (&message, format, args);
      va_end (args);

      if (ret <= 0)
        goto out;

      if (conn && G_IS_TLS_CONNECTION (conn)) {
        if (G_IS_TLS_CLIENT_CONNECTION (conn))
          header = g_strdup_printf ("CLIENT[%p]: ", conn);
        else if (G_IS_TLS_SERVER_CONNECTION (conn))
          header = g_strdup_printf ("SERVER[%p]: ", conn);
        else
          g_assert_not_reached ();
      } else {
        header = g_strdup ("");
      }

      thread = g_strdup_printf ("%p", g_thread_self ());
      g_log_structured (G_LOG_DOMAIN, level,
                        "GLIB_NET_THREAD", thread,
                        "CODE_FILE", file,
                        "CODE_LINE", line,
                        "CODE_FUNC", func,
                        "MESSAGE", "%s%s", header, message);

    out:
      g_free (header);
      g_free (message);
      g_free (thread);
    }
}
