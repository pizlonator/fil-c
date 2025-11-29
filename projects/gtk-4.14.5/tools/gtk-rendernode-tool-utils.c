/*  Copyright 2023 Red Hat, Inc.
 *
 * GTK is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * GTK is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GTK; see the file COPYING.  If not,
 * see <http://www.gnu.org/licenses/>.
 *
 * Author: Matthias Clasen
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <glib/gi18n-lib.h>
#include <glib/gprintf.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include "gtk-rendernode-tool.h"

#ifdef GDK_WINDOWING_BROADWAY
#include <gsk/broadway/gskbroadwayrenderer.h>
#endif

static void
deserialize_error_func (const GskParseLocation *start,
                        const GskParseLocation *end,
                        const GError           *error,
                        gpointer                user_data)
{
  GString *string = g_string_new ("<data>");

  g_string_append_printf (string, ":%zu:%zu",
                          start->lines + 1, start->line_chars + 1);
  if (start->lines != end->lines || start->line_chars != end->line_chars)
    {
      g_string_append (string, "-");
      if (start->lines != end->lines)
        g_string_append_printf (string, "%zu:", end->lines + 1);
      g_string_append_printf (string, "%zu", end->line_chars + 1);
    }

  g_printerr (_("Error at %s: %s\n"), string->str, error->message);

  g_string_free (string, TRUE);
}

GskRenderNode *
load_node_file (const char *filename)
{
  GFile *file;
  GBytes *bytes;
  GError *error = NULL;

  file = g_file_new_for_commandline_arg (filename);
  bytes = g_file_load_bytes (file, NULL, NULL, &error);
  g_object_unref (file);

  if (bytes == NULL)
    {
      g_printerr (_("Failed to load node file: %s\n"), error->message);
      g_clear_error (&error);
      exit (1);
    }

  return gsk_render_node_deserialize (bytes, deserialize_error_func, NULL);
}

/* keep in sync with gsk/gskrenderer.c */
static GskRenderer *
get_renderer_for_name (const char *renderer_name)
{
  if (renderer_name == NULL)
    return NULL;
#ifdef GDK_WINDOWING_BROADWAY
  else if (g_ascii_strcasecmp (renderer_name, "broadway") == 0)
    return gsk_broadway_renderer_new ();
#endif
  else if (g_ascii_strcasecmp (renderer_name, "cairo") == 0)
    return gsk_cairo_renderer_new ();
  else if (g_ascii_strcasecmp (renderer_name, "opengl") == 0 ||
           g_ascii_strcasecmp (renderer_name, "gl") == 0)
    return gsk_gl_renderer_new ();
  else if (g_ascii_strcasecmp (renderer_name, "ngl") == 0)
    return gsk_ngl_renderer_new ();
#ifdef GDK_RENDERING_VULKAN
  else if (g_ascii_strcasecmp (renderer_name, "vulkan") == 0)
    return gsk_vulkan_renderer_new ();
#endif
  else
    return NULL;
}

GskRenderer *
create_renderer (const char *name, GError **error)
{
  GskRenderer *renderer;

  if (name == NULL)
    {
      /* awwwwwkward - there should be code to get the
       * default renderer without a surface */
      static GdkSurface *window = NULL;

      if (window == NULL)
        window = gdk_surface_new_toplevel (gdk_display_get_default ());
      return gsk_renderer_new_for_surface (window);
    }

  renderer = get_renderer_for_name (name);

  if (renderer == NULL)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                   "No renderer named \"%s\"", name);
      return NULL;
    }

  if (!gsk_renderer_realize_for_display (renderer, gdk_display_get_default (), error))
    {
      g_object_unref (renderer);
      return NULL;
    }

  return renderer;
}
