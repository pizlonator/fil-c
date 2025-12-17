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

#include <glib/gi18n-lib.h>

#include "gproxyresolvergnome.h"
#include "visibility.h"


GLIB_NETWORKING_EXPORT void
g_io_gnomeproxy_load (GIOModule *module)
{
  gchar *locale_dir;
#ifdef G_OS_WIN32
  gchar *base_dir;
#endif

  g_proxy_resolver_gnome_register (module);

#ifdef G_OS_WIN32
  base_dir = g_win32_get_package_installation_directory_of_module (NULL);
  locale_dir = g_build_filename (base_dir, "share", "locale", NULL);
  g_free (base_dir);
#else
  locale_dir = g_strdup (LOCALE_DIR);
#endif

  bindtextdomain (GETTEXT_PACKAGE, locale_dir);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  g_free (locale_dir);
}

GLIB_NETWORKING_EXPORT void
g_io_gnomeproxy_unload (GIOModule *module)
{
}

GLIB_NETWORKING_EXPORT gchar **
g_io_gnomeproxy_query (void)
{
  gchar *eps[] = {
    G_PROXY_RESOLVER_EXTENSION_POINT_NAME,
    NULL
  };
  return g_strdupv (eps);
}
