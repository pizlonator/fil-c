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

#include <gtk/gtk.h>
#include "gtk-path-tool.h"

#include <glib/gi18n-lib.h>

typedef struct
{
  int contours;
  int ops;
  int lines;
  int quads;
  int cubics;
  int conics;
} Statistics;

static gboolean
stats_cb (GskPathOperation        op,
          const graphene_point_t *pts,
          gsize                   n_pts,
          float                   weight,
          gpointer                user_data)
{
  Statistics *stats = user_data;

  stats->ops++;

  switch (op)
    {
    case GSK_PATH_MOVE:
      stats->contours++;
      break;
    case GSK_PATH_CLOSE:
    case GSK_PATH_LINE:
      stats->lines++;
      break;
    case GSK_PATH_QUAD:
      stats->quads++;
      break;
    case GSK_PATH_CUBIC:
      stats->cubics++;
      break;
    case GSK_PATH_CONIC:
      stats->conics++;
      break;
    default:
      g_assert_not_reached ();
    }

  return TRUE;
}

static void
collect_statistics (GskPath    *path,
                    Statistics *stats)
{
  memset (stats, 0, sizeof (Statistics));
  gsk_path_foreach (path, -1, stats_cb, stats);
}

void
do_info (int *argc, const char ***argv)
{
  GError *error = NULL;
  char **args = NULL;
  GOptionContext *context;
  GOptionEntry entries[] = {
    { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &args, NULL, N_("PATH") },
    { NULL, },
  };
  GskPath *path;
  GskPathMeasure *measure;
  graphene_rect_t bounds;

  g_set_prgname ("gtk4-path-tool info");

  context = g_option_context_new (NULL);
  g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);
  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_set_summary (context, _("Print information about a path."));

  if (!g_option_context_parse (context, argc, (char ***)argv, &error))
    {
      g_printerr ("%s\n", error->message);
      g_error_free (error);
      exit (1);
    }

  g_option_context_free (context);

  if (args == NULL)
    {
      g_printerr ("%s\n", _("No paths given."));
      exit (1);
    }

  path = get_path (args[0]);
  measure = gsk_path_measure_new (path);

  if (gsk_path_is_empty (path))
    g_print ("%s\n", _("Path is empty."));
  else
    {
      Statistics stats;

      if (gsk_path_is_closed (path))
        g_print ("%s\n", _("Path is closed"));

      g_print ("%s %g\n", _("Path length"), gsk_path_measure_get_length (measure));

      if (gsk_path_get_bounds (path, &bounds))
        g_print ("%s: %g %g %g %g\n", _("Bounds"),
                 bounds.origin.x, bounds.origin.y,
                 bounds.size.width, bounds.size.height);

      collect_statistics (path, &stats);

      g_print (_("%d contours"), stats.contours);
      g_print ("\n");
      g_print (_("%d operations"), stats.ops);
      g_print ("\n");
      if (stats.lines)
        {
          g_print (_("%d lines"), stats.lines);
          g_print ("\n");
        }
      if (stats.quads)
        {
          g_print (_("%d quadratics"), stats.quads);
          g_print ("\n");
        }
      if (stats.cubics)
        {
          g_print (_("%d cubics"), stats.cubics);
          g_print ("\n");
        }
      if (stats.conics)
        {
          g_print (_("%d conics"), stats.conics);
          g_print ("\n");
        }
    }
}
