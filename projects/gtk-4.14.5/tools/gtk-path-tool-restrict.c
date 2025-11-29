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

void
do_restrict (int *argc, const char ***argv)
{
  GError *error = NULL;
  double start = G_MAXDOUBLE;
  double end = G_MAXDOUBLE;
  char **args = NULL;
  GOptionContext *context;
  GOptionEntry entries[] = {
    { "start", 0, 0, G_OPTION_ARG_DOUBLE, &start, N_("Beginning of segment"), N_("LENGTH") },
    { "end", 0, 0, G_OPTION_ARG_DOUBLE, &end, N_("End of segment"), N_("LENGTH") },
    { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &args, NULL, N_("PATH") },
    { NULL, },
  };
  GskPath *path, *result;
  GskPathMeasure *measure;
  GskPathBuilder *builder;
  GskPathPoint start_point, end_point;

  g_set_prgname ("gtk4-path-tool restrict");

  context = g_option_context_new (NULL);
  g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);
  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_set_summary (context, _("Restrict a path to a segment."));

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

  if (start == G_MAXDOUBLE)
    start = 0;

  if (end == G_MAXDOUBLE)
    end = gsk_path_measure_get_length (measure);

  builder = gsk_path_builder_new ();

  gsk_path_measure_get_point (measure, start, &start_point);
  gsk_path_measure_get_point (measure, end, &end_point);

  gsk_path_builder_add_segment (builder, path, &start_point, &end_point);

  result = gsk_path_builder_free_to_path (builder);

  if (result)
    {
      char *str = gsk_path_to_string (result);
      g_print ("%s\n", str);
      g_free (str);
    }
  else
    {
      g_printerr ("%s\n", _("That didn't work out."));
      exit (1);
    }
}
