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
#ifdef CAIRO_HAS_SVG_SURFACE
#include <cairo-svg.h>
#endif
#ifdef CAIRO_HAS_PDF_SURFACE
#include <cairo-pdf.h>
#endif

static char *
get_save_filename (const char *filename)
{
  int length = strlen (filename);
  const char *extension = ".png";
  char *result;

  if (strcmp (filename + (length - strlen (".node")), ".node") == 0)
    {
      char *basename = g_strndup (filename, length - strlen (".node"));
      result = g_strconcat (basename, extension, NULL);
      g_free (basename);
    }
  else
    result = g_strconcat (filename, extension, NULL);

  return result;
}

#if defined(CAIRO_HAS_SVG_SURFACE) || defined(CAIRO_HAS_PDF_SURFACE)
static cairo_status_t
cairo_serializer_write (gpointer             user_data,
                        const unsigned char *data,
                        unsigned int         length)
{
  g_byte_array_append (user_data, data, length);

  return CAIRO_STATUS_SUCCESS;
}
#endif

#ifdef CAIRO_HAS_SVG_SURFACE
static GBytes *
create_svg (GskRenderNode  *node,
            GError        **error)
{
  cairo_surface_t *surface;
  cairo_t *cr;
  graphene_rect_t bounds;
  GByteArray *array;

  gsk_render_node_get_bounds (node, &bounds);
  array = g_byte_array_new ();

  surface = cairo_svg_surface_create_for_stream (cairo_serializer_write,
                                                 array,
                                                 bounds.size.width,
                                                 bounds.size.height);
  cairo_svg_surface_set_document_unit (surface, CAIRO_SVG_UNIT_PX);
  cairo_surface_set_device_offset (surface, -bounds.origin.x, -bounds.origin.y);

  cr = cairo_create (surface);
  gsk_render_node_draw (node, cr);
  cairo_destroy (cr);

  cairo_surface_finish (surface);
  if (cairo_surface_status (surface) == CAIRO_STATUS_SUCCESS)
    {
      cairo_surface_destroy (surface);
      return g_byte_array_free_to_bytes (array);
    }
  else
    {
      g_set_error (error,
                   G_IO_ERROR, G_IO_ERROR_FAILED,
                   "%s", cairo_status_to_string (cairo_surface_status (surface)));
      cairo_surface_destroy (surface);
      g_byte_array_unref (array);
      return NULL;
    }
}
#endif

#ifdef CAIRO_HAS_PDF_SURFACE
static GBytes *
create_pdf (GskRenderNode  *node,
            GError        **error)
{
  cairo_surface_t *surface;
  cairo_t *cr;
  graphene_rect_t bounds;
  GByteArray *array;

  gsk_render_node_get_bounds (node, &bounds);
  array = g_byte_array_new ();

  surface = cairo_pdf_surface_create_for_stream (cairo_serializer_write,
                                                 array,
                                                 bounds.size.width,
                                                 bounds.size.height);
  cairo_surface_set_device_offset (surface, -bounds.origin.x, -bounds.origin.y);

  cr = cairo_create (surface);
  gsk_render_node_draw (node, cr);
  cairo_show_page (cr);
  cairo_destroy (cr);

  cairo_surface_finish (surface);
  if (cairo_surface_status (surface) == CAIRO_STATUS_SUCCESS)
    {
      cairo_surface_destroy (surface);
      return g_byte_array_free_to_bytes (array);
    }
  else
    {
      g_set_error (error,
                   G_IO_ERROR, G_IO_ERROR_FAILED,
                   "%s", cairo_status_to_string (cairo_surface_status (surface)));
      cairo_surface_destroy (surface);
      g_byte_array_unref (array);
      return NULL;
    }
}
#endif

static void
render_file (const char *filename,
             const char *renderer_name,
             const char *save_file)
{
  GskRenderNode *node;
  GBytes *bytes;
  char *save_to;
  GError *error = NULL;

  save_to = (char *) save_file;

  if (save_to == NULL)
    {
      save_to = get_save_filename (filename);
      if (g_file_test (save_to, G_FILE_TEST_EXISTS))
        {
          g_printerr (_("File %s exists.\n"
                        "If you want to overwrite, specify the filename.\n"), save_to);
          exit (1);
        }
    }

  node = load_node_file (filename);

#ifdef CAIRO_HAS_SVG_SURFACE
  if (g_str_has_suffix (save_to, ".svg"))
    {
      bytes = create_svg (node, &error);
      if (bytes == NULL)
        {
          g_printerr (_("Failed to generate SVG: %s\n"), error->message);
          exit (1);
        }
    }
  else
#endif
#ifdef CAIRO_HAS_PDF_SURFACE
  if (g_str_has_suffix (save_to, ".pdf"))
    {
      bytes = create_pdf (node, &error);
      if (bytes == NULL)
        {
          g_printerr (_("Failed to generate SVG: %s\n"), error->message);
          exit (1);
        }
    }
  else
#endif
    {
      GdkTexture *texture;
      GskRenderer *renderer;

      renderer = create_renderer (renderer_name, &error);
      if (renderer == NULL)
        {
          g_printerr (_("Failed to create renderer: %s\n"), error->message);
          exit (1);
        }

      texture = gsk_renderer_render_texture (renderer, node, NULL);

      if (g_str_has_suffix (save_to, ".tif") ||
          g_str_has_suffix (save_to, ".tiff"))
        bytes = gdk_texture_save_to_tiff_bytes (texture);
      else
        bytes = gdk_texture_save_to_png_bytes (texture);

      g_object_unref (texture);
    }

  if (g_file_set_contents (save_to,
                           g_bytes_get_data (bytes, NULL),
                           g_bytes_get_size (bytes),
                           &error))
    {
      if (save_file == NULL)
        g_print (_("Output written to %s.\n"), save_to);
    }
  else
    {
      g_printerr (_("Failed to save %s: %s\n"), save_to, error->message);
      exit (1);
    }

  g_bytes_unref (bytes);

  if (save_to != save_file)
    g_free (save_to);

  gsk_render_node_unref (node);
}

void
do_render (int          *argc,
           const char ***argv)
{
  GOptionContext *context;
  char **filenames = NULL;
  char *renderer = NULL;
  const GOptionEntry entries[] = {
    { "renderer", 0, 0, G_OPTION_ARG_STRING, &renderer, N_("Renderer to use"), N_("RENDERER") },
    { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &filenames, NULL, N_("FILE…") },
    { NULL, }
  };
  GError *error = NULL;

  if (gdk_display_get_default () == NULL)
    {
      g_printerr (_("Could not initialize windowing system\n"));
      exit (1);
    }

  g_set_prgname ("gtk4-rendernode-tool render");
  context = g_option_context_new (NULL);
  g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);
  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_set_summary (context, _("Render a .node file to an image."));

  if (!g_option_context_parse (context, argc, (char ***)argv, &error))
    {
      g_printerr ("%s\n", error->message);
      g_error_free (error);
      exit (1);
    }

  g_option_context_free (context);

  if (filenames == NULL)
    {
      g_printerr (_("No .node file specified\n"));
      exit (1);
    }

  if (g_strv_length (filenames) > 2)
    {
      g_printerr (_("Can only render a single .node file to a single output file\n"));
      exit (1);
    }

  render_file (filenames[0], renderer, filenames[1]);

  g_strfreev (filenames);
}
