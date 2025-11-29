/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.Free
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib/gstdio.h>
#include <gmodule.h>
#include "gtkimmodule.h"
#include "gtkimmoduleprivate.h"
#include "gtkimcontextsimple.h"
#include "gtkmodulesprivate.h"
#include "gtksettings.h"
#include "gtkprivate.h"

#ifdef GDK_WINDOWING_X11
#include "x11/gdkx.h"
#endif

#ifdef GDK_WINDOWING_WAYLAND
#include "wayland/gdkwayland.h"
#include "gtkimcontextwayland.h"
#endif

#ifdef GDK_WINDOWING_BROADWAY
#include "broadway/gdkbroadway.h"
#include "gtkimcontextbroadway.h"
#endif

#ifdef GDK_WINDOWING_WIN32
#include "win32/gdkwin32.h"
#include "gtkimcontextime.h"
#endif

#ifdef GDK_WINDOWING_MACOS
#include "macos/gdkmacos.h"
#include "gtkimcontextquartz.h"
#endif

#ifdef G_OS_WIN32
#include <windows.h>
#endif

#define SIMPLE_ID     "gtk-im-context-simple"
#define ALT_SIMPLE_ID "simple"
#define NONE_ID       "gtk-im-context-none"
#define ALT_NONE_ID   "none"

/**
 * _gtk_im_module_create:
 * @context_id: the context ID for the context type to create
 *
 * Create an IM context of a type specified by the string
 * ID @context_id.
 *
 * Returns: a newly created input context of or @context_id, or
 *   if that could not be created, a newly created `GtkIMContextSimple`
 */
GtkIMContext *
_gtk_im_module_create (const char *context_id)
{
  GIOExtensionPoint *ep;
  GIOExtension *ext;
  GType type;
  GtkIMContext *context = NULL;

  if (strcmp (context_id, NONE_ID) == 0)
    return NULL;

  ep = g_io_extension_point_lookup (GTK_IM_MODULE_EXTENSION_POINT_NAME);
  ext = g_io_extension_point_get_extension_by_name (ep, context_id);
  if (ext)
    {
      type = g_io_extension_get_type (ext);
      context = g_object_new (type, NULL);
    }

  return context;
}

static gboolean
match_backend (GdkDisplay *display,
               const char *context_id)
{
  if (g_strcmp0 (context_id, "wayland") == 0)
    {
#ifdef GDK_WINDOWING_WAYLAND
      return GDK_IS_WAYLAND_DISPLAY (display) &&
             gdk_wayland_display_query_registry (display,
                                                 "zwp_text_input_manager_v3");
#else
      return FALSE;
#endif
    }

  if (g_strcmp0 (context_id, "broadway") == 0)
#ifdef GDK_WINDOWING_BROADWAY
    return GDK_IS_BROADWAY_DISPLAY (display);
#else
    return FALSE;
#endif

  if (g_strcmp0 (context_id, "ime") == 0)
#ifdef GDK_WINDOWING_WIN32
    return GDK_IS_WIN32_DISPLAY (display);
#else
    return FALSE;
#endif

  if (g_strcmp0 (context_id, "quartz") == 0)
#ifdef GDK_WINDOWING_MACOS
    return GDK_IS_MACOS_DISPLAY (display);
#else
    return FALSE;
#endif

  return TRUE;
}

static const char *
lookup_immodule (GdkDisplay  *display,
                 char       **immodules_list)
{
  guint i;

  for (i = 0; immodules_list[i]; i++)
    {
      if (!match_backend (display, immodules_list[i]))
        continue;

      if (g_strcmp0 (immodules_list[i], SIMPLE_ID) == 0 ||
          g_strcmp0 (immodules_list[i], ALT_SIMPLE_ID) == 0)
        return SIMPLE_ID;
      else if (g_strcmp0 (immodules_list[i], NONE_ID) == 0 ||
               g_strcmp0 (immodules_list[i], ALT_NONE_ID) == 0)
        return NONE_ID;
      else
        {
          GIOExtensionPoint *ep;
          GIOExtension *ext;

          ep = g_io_extension_point_lookup (GTK_IM_MODULE_EXTENSION_POINT_NAME);
          ext = g_io_extension_point_get_extension_by_name (ep, immodules_list[i]);
          if (ext)
            return g_io_extension_get_name (ext);
        }
    }

  return NULL;
}

/**
 * _gtk_im_module_get_default_context_id:
 * @display: The display to look up the module for
 *
 * Return the context_id of the best IM context type
 * for the given window.
 *
 * Returns: the context ID (will never be %NULL)
 */
const char *
_gtk_im_module_get_default_context_id (GdkDisplay *display)
{
  const char *context_id = NULL;
  const char *envvar;
  GtkSettings *settings;
  GIOExtensionPoint *ep;
  GList *l;
  char *tmp;

  envvar = g_getenv ("GTK_IM_MODULE");
  if (envvar)
    {
      char **immodules;
      immodules = g_strsplit (envvar, ":", 0);
      context_id = lookup_immodule (display, immodules);
      g_strfreev (immodules);

      if (context_id)
        return context_id;
      else
        {
          static gboolean warned;

          if (!warned)
            {
              g_warning ("No IM module matching GTK_IM_MODULE=%s found", envvar);
              warned = TRUE;
            }
        }
    }

  /* Check if the certain immodule is set in XSETTINGS. */
  settings = gtk_settings_get_for_display (display);
  g_object_get (G_OBJECT (settings), "gtk-im-module", &tmp, NULL);
  if (tmp)
    {
      char **immodules;

      immodules = g_strsplit (tmp, ":", 0);
      context_id = lookup_immodule (display, immodules);
      g_strfreev (immodules);
      g_free (tmp);

      if (context_id)
        return context_id;
    }

  ep = g_io_extension_point_lookup (GTK_IM_MODULE_EXTENSION_POINT_NAME);
  for (l = g_io_extension_point_get_extensions (ep); l; l = l->next)
    {
      GIOExtension *ext = l->data;

      context_id = g_io_extension_get_name (ext);
      if (match_backend (display, context_id))
        return context_id;
    }

  g_error ("GTK was run without any IM module being present. This must not happen.");

  return SIMPLE_ID;
}

void
gtk_im_module_ensure_extension_point (void)
{
  GIOExtensionPoint *ep;
  static gboolean registered = FALSE;

  if (registered)
    return;

  GTK_DEBUG (MODULES, "Registering extension point %s", GTK_IM_MODULE_EXTENSION_POINT_NAME);

  ep = g_io_extension_point_register (GTK_IM_MODULE_EXTENSION_POINT_NAME);
  g_io_extension_point_set_required_type (ep, GTK_TYPE_IM_CONTEXT);

  registered = TRUE;
}

void
gtk_im_modules_init (void)
{
  GIOModuleScope *scope;
  char **paths;
  int i;

  gtk_im_module_ensure_extension_point ();

  g_type_ensure (gtk_im_context_simple_get_type ());
#ifdef GDK_WINDOWING_WAYLAND
  g_type_ensure (gtk_im_context_wayland_get_type ());
#endif
#ifdef GDK_WINDOWING_BROADWAY
  g_type_ensure (gtk_im_context_broadway_get_type ());
#endif
#ifdef GDK_WINDOWING_WIN32
  g_type_ensure (gtk_im_context_ime_get_type ());
#endif
#ifdef GDK_WINDOWING_MACOS
  g_type_ensure (gtk_im_context_quartz_get_type ());
#endif

  scope = g_io_module_scope_new (G_IO_MODULE_SCOPE_BLOCK_DUPLICATES);

  paths = _gtk_get_module_path ("immodules");
  for (i = 0; paths[i]; i++)
    {
      GTK_DEBUG (MODULES, "Scanning io modules in %s", paths[i]);
      g_io_modules_scan_all_in_directory_with_scope (paths[i], scope);
    }
  g_strfreev (paths);

  g_io_module_scope_free (scope);

  if (GTK_DEBUG_CHECK (MODULES))
    {
      GIOExtensionPoint *ep;
      GList *list, *l;

      ep = g_io_extension_point_lookup (GTK_IM_MODULE_EXTENSION_POINT_NAME);
      list = g_io_extension_point_get_extensions (ep);
      for (l = list; l; l = l->next)
        {
          GIOExtension *ext = l->data;
          g_print ("extension: %s: type %s\n",
                   g_io_extension_get_name (ext),
                   g_type_name (g_io_extension_get_type (ext)));
        }
    }
}
