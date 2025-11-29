/*  Copyright 2023 Red Hat, Inc.
 *
 * GTK+ is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * GLib is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GTK+; see the file COPYING.  If not,
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
#include "gtk-path-tool.h"

static gboolean
collect_cb (GskPathOperation        op,
            const graphene_point_t *pts,
            gsize                   n_pts,
            float                   weight,
            gpointer                data)
{
  GskPathBuilder **builders = (GskPathBuilder **)data;

  switch (op)
    {
    case GSK_PATH_MOVE:
      if (builders[0])
        gsk_path_builder_move_to (builders[0], pts[0].x, pts[0].y);
      if (builders[1])
        gsk_path_builder_add_circle (builders[1], &pts[0], 4);
      break;

    case GSK_PATH_LINE:
    case GSK_PATH_CLOSE:
      if (builders[0])
        gsk_path_builder_line_to (builders[0], pts[1].x, pts[1].y);
      if (builders[1])
        gsk_path_builder_add_circle (builders[1], &pts[1], 4);
      break;

    case GSK_PATH_QUAD:
    case GSK_PATH_CONIC:
      if (builders[0])
        {
          gsk_path_builder_line_to (builders[0], pts[1].x, pts[1].y);
          gsk_path_builder_line_to (builders[0], pts[2].x, pts[2].y);
        }
      if (builders[1])
        gsk_path_builder_add_circle (builders[1], &pts[2], 4);
      if (builders[2])
        gsk_path_builder_add_circle (builders[2], &pts[1], 3);
      break;

    case GSK_PATH_CUBIC:
      if (builders[0])
        {
          gsk_path_builder_line_to (builders[0], pts[1].x, pts[1].y);
          gsk_path_builder_line_to (builders[0], pts[2].x, pts[2].y);
          gsk_path_builder_line_to (builders[0], pts[3].x, pts[3].y);
        }
      if (builders[1])
        gsk_path_builder_add_circle (builders[1], &pts[3], 4);
      if (builders[2])
        {
          gsk_path_builder_add_circle (builders[2], &pts[1], 3);
          gsk_path_builder_add_circle (builders[2], &pts[2], 3);
        }
      break;

    default:
      g_assert_not_reached ();
    }

  return TRUE;
}

void
do_render (int          *argc,
           const char ***argv)
{
  GError *error = NULL;
  const char *fill = "winding";
  const char *fg_color = "black";
  const char *bg_color = "white";
  const char *point_color = "red";
  gboolean do_stroke = FALSE;
  gboolean show_points = FALSE;
  gboolean show_controls = FALSE;
  double line_width = 1;
  const char *cap = "butt";
  const char *join = "miter";
  double miter_limit = 4;
  const char *dashes = NULL;
  double dash_offset = 0;
  const char *output_file = NULL;
  char **args = NULL;
  GOptionContext *context;
  GOptionGroup *options;
  const GOptionEntry entries[] = {
    { "fill", 0, G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE, &do_stroke, N_("Fill the path (the default)"), NULL },
    { "stroke", 0, 0, G_OPTION_ARG_NONE, &do_stroke, N_("Stroke the path"), NULL },
    { "points", 0, 0, G_OPTION_ARG_NONE, &show_points, N_("Show path points"), NULL },
    { "controls", 0, 0, G_OPTION_ARG_NONE, &show_controls, N_("Show control points"), NULL },
    { "output", 0, 0, G_OPTION_ARG_FILENAME, &output_file, N_("The output file"), N_("FILE") },
    { "fg-color", 0, 0, G_OPTION_ARG_STRING, &fg_color, N_("Foreground color"), N_("COLOR") },
    { "bg-color", 0, 0, G_OPTION_ARG_STRING, &bg_color, N_("Background color"), N_("COLOR") },
    { "point-color", 0, 0, G_OPTION_ARG_STRING, &point_color, N_("Point color"), N_("COLOR") },
    { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, &args, NULL, N_("PATH") },
    { NULL, }
  };
  const GOptionEntry fill_entries[] = {
    { "fill-rule", 0, 0, G_OPTION_ARG_STRING, &fill, N_("Fill rule (winding, even-odd)"), N_("VALUE") },
    { NULL, }
  };
  const GOptionEntry stroke_entries[] = {
    { "line-width", 0, 0, G_OPTION_ARG_DOUBLE, &line_width, N_("Line width (number)"), N_("VALUE") },
    { "line-cap", 0, 0, G_OPTION_ARG_STRING, &cap, N_("Line cap (butt, round, square)"), N_("VALUE") },
    { "line-join", 0, 0, G_OPTION_ARG_STRING, &join, N_("Line join (miter, miter-clip, round, bevel, arcs)"), N_("VALUE") },
    { "miter-limit", 0, 0, G_OPTION_ARG_DOUBLE, &miter_limit, N_("Miter limit (number)"), N_("VALUE") },
    { "dashes", 0, 0, G_OPTION_ARG_STRING, &dashes, N_("Dash pattern (comma-separated numbers)"), N_("VALUE") },
    { "dash-offset", 0, 0, G_OPTION_ARG_DOUBLE, &dash_offset, N_("Dash offset (number)"), N_("VALUE") },
    { NULL, }
  };

  GskPath *path;
  GskPath *line_path = NULL;
  GskPath *point_path = NULL;
  GskFillRule fill_rule;
  GdkRGBA fg, bg, pc;
  graphene_rect_t bounds;
  GskRenderNode *fg_node, *pc_node, *nodes[5], *node;
  GdkSurface *surface;
  GskRenderer *renderer;
  GdkTexture *texture;
  const char *filename;
  GskLineCap line_cap;
  GskLineJoin line_join;
  GskStroke *stroke;
  GskPathBuilder *builders[3] = { NULL, NULL, NULL };
  int i;

  if (gdk_display_get_default () == NULL)
    {
      g_printerr ("%s\n", _("Could not initialize windowing system"));
      exit (1);
    }

  g_set_prgname ("gtk4-path-tool render");
  context = g_option_context_new (NULL);
  g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);
  g_option_context_set_summary (context, _("Render the path to a png image."));

  g_option_context_add_main_entries (context, entries, NULL);

  options = g_option_group_new ("fill",
                                _("Options related to filling"),
                                _("Show help for fill options"),
                                NULL, NULL);
  g_option_group_add_entries (options, fill_entries);
  g_option_group_set_translation_domain (options, GETTEXT_PACKAGE);
  g_option_context_add_group (context, options);

  options = g_option_group_new ("stroke",
                                _("Options related to stroking"),
                                _("Show help for stroke options"),
                                NULL, NULL);
  g_option_group_add_entries (options, stroke_entries);
  g_option_group_set_translation_domain (options, GETTEXT_PACKAGE);
  g_option_context_add_group (context, options);

  if (!g_option_context_parse (context, argc, (char ***)argv, &error))
    {
      g_printerr ("%s\n", error->message);
      g_error_free (error);
      exit (1);
    }

  g_option_context_free (context);

  if (args == NULL)
    {
      g_printerr ("%s\n", _("No path specified"));
      exit (1);
    }

  if (g_strv_length (args) > 1)
    {
      g_printerr ("%s\n", _("Can only render a single path"));
      exit (1);
    }

  path = get_path (args[0]);

  if (show_controls)
    {
      builders[0] = gsk_path_builder_new ();
      builders[1] = gsk_path_builder_new ();
      builders[2] = gsk_path_builder_new ();
    }
  else if (show_points)
    {
      builders[1] = gsk_path_builder_new ();
    }

  if (builders[0] || builders[1] || builders[2])
    {
      gsk_path_foreach (path, -1, collect_cb, builders);

      if (builders[0])
        line_path = gsk_path_builder_free_to_path (builders[0]);
      if (builders[1] || builders[2])
        {
          GskPath *p1 = NULL;

          if (builders[1])
            p1 = gsk_path_builder_free_to_path (builders[1]);

          if (builders[2])
            {
              if (p1)
                {
                  gsk_path_builder_add_path (builders[2], p1);
                  gsk_path_unref (p1);
                }
              point_path = gsk_path_builder_free_to_path (builders[2]);
            }
          else
            point_path = p1;
        }
    }


  fill_rule = get_enum_value (GSK_TYPE_FILL_RULE, _("fill rule"), fill);
  get_color (&fg, fg_color);
  get_color (&bg, bg_color);
  get_color (&pc, point_color);

  line_cap = get_enum_value (GSK_TYPE_LINE_CAP, _("line cap"), cap);
  line_join = get_enum_value (GSK_TYPE_LINE_JOIN, _("line join"), join);

  stroke = gsk_stroke_new (line_width);
  gsk_stroke_set_line_cap (stroke, line_cap);
  gsk_stroke_set_line_join (stroke, line_join);
  gsk_stroke_set_miter_limit (stroke, miter_limit);
  gsk_stroke_set_dash_offset (stroke, dash_offset);
  _gsk_stroke_set_dashes (stroke, dashes);

  if (do_stroke)
    gsk_path_get_stroke_bounds (path, stroke, &bounds);
  else
    gsk_path_get_bounds (path, &bounds);
  graphene_rect_inset (&bounds, -10, -10);

  fg_node = gsk_color_node_new (&fg, &bounds);
  pc_node = gsk_color_node_new (&pc, &bounds);

  i = 0;
  nodes[i++] = gsk_color_node_new (&bg, &bounds);

  if (line_path)
    {
      GskStroke *line_stroke = gsk_stroke_new (1);
      gsk_stroke_set_dash (line_stroke, (const float[]) { 1, 1 }, 2);
      nodes[i++] = gsk_stroke_node_new (fg_node, line_path, line_stroke);
      gsk_stroke_free (line_stroke);
    }
  if (point_path)
    {
      nodes[i++] = gsk_fill_node_new (pc_node, point_path, GSK_FILL_RULE_WINDING);
      nodes[i++] = gsk_stroke_node_new (fg_node, point_path, stroke);
    }

  if (do_stroke)
    nodes[i++] = gsk_stroke_node_new (fg_node, path, stroke);
  else
    nodes[i++] = gsk_fill_node_new (fg_node, path, fill_rule);

  node = gsk_container_node_new (nodes, i);

  gsk_render_node_unref (fg_node);
  gsk_render_node_unref (pc_node);
  for (i--; i >= 0; i--)
    gsk_render_node_unref (nodes[i]);

  surface = gdk_surface_new_toplevel (gdk_display_get_default ());
  renderer = gsk_renderer_new_for_surface (surface);

  texture = gsk_renderer_render_texture (renderer, node, &bounds);

  filename = output_file ? output_file : "path.png";
  if (!gdk_texture_save_to_png (texture, filename))
    {
      char *msg = g_strdup_printf (_("Saving png to '%s' failed"), filename);
      g_printerr ("%s\n", msg);
      exit (1);
    }

  if (output_file == NULL)
    {
      char *msg = g_strdup_printf (_("Output written to '%s'."), filename);
      g_print ("%s\n", msg);
      g_free (msg);
    }

  g_object_unref (texture);
  gsk_renderer_unrealize (renderer);
  g_object_unref (renderer);
  g_object_unref (surface);
  gsk_render_node_unref (node);

  gsk_path_unref (path);
  g_clear_pointer (&line_path, gsk_path_unref);
  g_clear_pointer (&point_path, gsk_path_unref);

  g_strfreev (args);
}
