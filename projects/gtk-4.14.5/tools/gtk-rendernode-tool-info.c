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

#define N_NODE_TYPES (GSK_SUBSURFACE_NODE + 1)
static void
count_nodes (GskRenderNode *node,
             unsigned int  *counts,
             unsigned int  *depth)
{
  unsigned int d, dd;

  counts[gsk_render_node_get_node_type (node)] += 1;
  g_assert (gsk_render_node_get_node_type (node) < N_NODE_TYPES);
  d = 0;

  switch (gsk_render_node_get_node_type (node))
    {
    case GSK_CONTAINER_NODE:
      for (unsigned int i = 0; i < gsk_container_node_get_n_children (node); i++)
        {
          count_nodes (gsk_container_node_get_child (node, i), counts, &dd);
          d = MAX (d, dd);
        }
      break;

    case GSK_CAIRO_NODE:
    case GSK_COLOR_NODE:
    case GSK_LINEAR_GRADIENT_NODE:
    case GSK_REPEATING_LINEAR_GRADIENT_NODE:
    case GSK_RADIAL_GRADIENT_NODE:
    case GSK_REPEATING_RADIAL_GRADIENT_NODE:
    case GSK_CONIC_GRADIENT_NODE:
    case GSK_BORDER_NODE:
    case GSK_TEXTURE_NODE:
    case GSK_INSET_SHADOW_NODE:
    case GSK_OUTSET_SHADOW_NODE:
      break;

    case GSK_TRANSFORM_NODE:
      count_nodes (gsk_transform_node_get_child (node), counts, &d);
      break;

    case GSK_OPACITY_NODE:
      count_nodes (gsk_opacity_node_get_child (node), counts, &d);
      break;

    case GSK_COLOR_MATRIX_NODE:
      count_nodes (gsk_color_matrix_node_get_child (node), counts, &d);
      break;

    case GSK_REPEAT_NODE:
      count_nodes (gsk_repeat_node_get_child (node), counts, &d);
      break;

    case GSK_CLIP_NODE:
      count_nodes (gsk_clip_node_get_child (node), counts, &d);
      break;

    case GSK_ROUNDED_CLIP_NODE:
      count_nodes (gsk_rounded_clip_node_get_child (node), counts, &d);
      break;

    case GSK_SHADOW_NODE:
      count_nodes (gsk_shadow_node_get_child (node), counts, &d);
      break;

    case GSK_BLEND_NODE:
      count_nodes (gsk_blend_node_get_bottom_child (node), counts, &d);
      count_nodes (gsk_blend_node_get_top_child (node), counts, &dd);
      d = MAX (d, dd);
      break;

    case GSK_CROSS_FADE_NODE:
      count_nodes (gsk_cross_fade_node_get_start_child (node), counts, &d);
      count_nodes (gsk_cross_fade_node_get_end_child (node), counts, &dd);
      d = MAX (d, dd);
      break;

    case GSK_TEXT_NODE:
      break;

    case GSK_BLUR_NODE:
      count_nodes (gsk_blur_node_get_child (node), counts, &d);
      break;

    case GSK_DEBUG_NODE:
      count_nodes (gsk_debug_node_get_child (node), counts, &d);
      break;

    case GSK_GL_SHADER_NODE:
      for (unsigned int i = 0; i < gsk_gl_shader_node_get_n_children (node); i++)
        {
          count_nodes (gsk_gl_shader_node_get_child (node, i), counts, &dd);
          d = MAX (d, dd);
        }
      break;

    case GSK_TEXTURE_SCALE_NODE:
      break;

    case GSK_MASK_NODE:
      count_nodes (gsk_mask_node_get_source (node), counts, &d);
      count_nodes (gsk_mask_node_get_mask (node), counts, &dd);
      d = MAX (d, dd);
      break;

    case GSK_FILL_NODE:
      count_nodes (gsk_fill_node_get_child (node), counts, &d);
      break;

    case GSK_STROKE_NODE:
      count_nodes (gsk_stroke_node_get_child (node), counts, &d);
      break;

    case GSK_SUBSURFACE_NODE:
      count_nodes (gsk_subsurface_node_get_child (node), counts, &d);
      break;

    case GSK_NOT_A_RENDER_NODE:
    default:
      g_assert_not_reached ();
    }

  *depth = d + 1;
}

static const char *
get_node_name (GskRenderNodeType type)
{
  GEnumClass *class;
  GEnumValue *value;
  const char *name;

  class = g_type_class_ref (GSK_TYPE_RENDER_NODE_TYPE);
  value = g_enum_get_value (class, type);
  name = value->value_nick;
  g_type_class_unref (class);

  return name;
}

static void
file_info (const char *filename)
{
  GskRenderNode *node;
  unsigned int counts[N_NODE_TYPES] = { 0, };
  unsigned int total = 0;
  unsigned int namelen = 0;
  unsigned int depth = 0;
  graphene_rect_t bounds;

  node = load_node_file (filename);

  count_nodes (node, counts, &depth);

  for (unsigned int i = 0; i < G_N_ELEMENTS (counts); i++)
    {
      total += counts[i];
      if (counts[i] > 0)
        namelen = MAX (namelen, strlen (get_node_name (i)));
    }

  g_print (_("Number of nodes: %u\n"), total);
  for (unsigned int i = 0; i < G_N_ELEMENTS (counts); i++)
    {
      if (counts[i] > 0)
        g_print ("  %*s: %u\n", namelen, get_node_name (i), counts[i]);
    }

  g_print (_("Depth: %u\n"), depth);

  gsk_render_node_get_bounds (node, &bounds);
  g_print (_("Bounds: %g x %g\n"), bounds.size.width, bounds.size.height);
  g_print (_("Origin: %g %g\n"), bounds.origin.x, bounds.origin.y);

  gsk_render_node_unref (node);
}

void
do_info (int          *argc,
         const char ***argv)
{
  GOptionContext *context;
  char **filenames = NULL;
  const GOptionEntry entries[] = {
    { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &filenames, NULL, N_("FILE") },
    { NULL, }
  };
  GError *error = NULL;

  g_set_prgname ("gtk4-rendernode-tool info");
  context = g_option_context_new (NULL);
  g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);
  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_set_summary (context, _("Provide information about the render node."));

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

  if (g_strv_length (filenames) > 1)
    {
      g_printerr (_("Can only accept a single .node file\n"));
      exit (1);
    }

  file_info (filenames[0]);

  g_strfreev (filenames);
}
