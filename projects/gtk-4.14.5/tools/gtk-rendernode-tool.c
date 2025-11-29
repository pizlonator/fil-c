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

static void G_GNUC_NORETURN
usage (void)
{
  g_print (_("Usage:\n"
             "  gtk4-rendernode-tool [COMMAND] [OPTION…] FILE\n"
             "\n"
             "Perform various tasks on GTK render nodes.\n"
             "\n"
             "Commands:\n"
             "  benchmark    Benchmark rendering of a node\n"
             "  compare      Compare nodes or images\n"
             "  info         Provide information about the node\n"
             "  show         Show the node\n"
             "  render       Take a screenshot of the node\n"
             "\n"));
  exit (1);
}

static GLogWriterOutput
log_writer_func (GLogLevelFlags   level,
                 const GLogField *fields,
                 gsize            n_fields,
                 gpointer         user_data)
{
  gsize i;
  const char *domain = NULL;
  const char *message = NULL;

  for (i = 0; i < n_fields; i++)
    {
      if (g_strcmp0 (fields[i].key, "GLIB_DOMAIN") == 0)
        domain = fields[i].value;
      else if (g_strcmp0 (fields[i].key, "MESSAGE") == 0)
        message = fields[i].value;
    }

  if (message != NULL && !g_log_writer_default_would_drop (level, domain))
    {
      const char *prefix;
      switch (level & G_LOG_LEVEL_MASK)
        {
        case G_LOG_LEVEL_ERROR:
          prefix = "ERROR";
          break;
        case G_LOG_LEVEL_CRITICAL:
          prefix = "CRITICAL";
          break;
        case G_LOG_LEVEL_WARNING:
          prefix = "WARNING";
          break;
        default:
          prefix = "INFO";
          break;
        }
      g_printerr ("%s-%s: %s\n", domain, prefix, message);
    }

  return G_LOG_WRITER_HANDLED;
}

int
main (int argc, const char *argv[])
{
  g_set_prgname ("gtk-rendernode-tool");

  g_log_set_writer_func (log_writer_func, NULL, NULL);

  gtk_init_check ();

  gtk_test_register_all_types ();

  if (argc < 2)
    usage ();

  if (strcmp (argv[1], "--help") == 0)
    usage ();

  argv++;
  argc--;

  if (strcmp (argv[0], "show") == 0)
    do_show (&argc, &argv);
  else if (strcmp (argv[0], "render") == 0)
    do_render (&argc, &argv);
  else if (strcmp (argv[0], "info") == 0)
    do_info (&argc, &argv);
  else if (strcmp (argv[0], "benchmark") == 0)
    do_benchmark (&argc, &argv);
  else if (strcmp (argv[0], "compare") == 0)
    do_compare (&argc, &argv);
  else
    usage ();

  return 0;
}
