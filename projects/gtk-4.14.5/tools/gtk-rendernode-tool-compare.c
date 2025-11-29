/*  Copyright 2024 Red Hat, Inc.
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

#include "testsuite/reftests/reftest-compare.h"

static GdkTexture *
texture_from_file (const char   *filename,
                   GskRenderer  *renderer,
                   GError      **error)
{
  GdkTexture *texture;

  if (g_str_has_suffix (filename, ".node"))
    {
      GskRenderNode *node = load_node_file (filename);
      texture = gsk_renderer_render_texture (renderer, node, NULL);
      gsk_render_node_unref (node);
    }
  else
    {
      texture = gdk_texture_new_from_filename (filename, error);
    }

  return texture;
}

void
do_compare (int          *argc,
            const char ***argv)
{
  GOptionContext *context;
  char *opt_filename = NULL;
  gboolean opt_quiet = FALSE;
  char **filenames = NULL;
  char *renderer_name = NULL;
  const GOptionEntry entries[] = {
    { "renderer", 0, 0, G_OPTION_ARG_STRING, &renderer_name, N_("Renderer to use"), N_("RENDERER") },

    { "output", 'o', 0, G_OPTION_ARG_FILENAME, &opt_filename, N_("Output file"), N_("FILE") },
    { "quiet", 'q', 0, G_OPTION_ARG_NONE, &opt_quiet, "Don't talk", NULL },

    { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &filenames, NULL, N_("FILE1 FILE2") },
    { NULL, }
  };
  GskRenderer *renderer;
  GdkTexture *texture[2];
  GdkTexture *diff;
  GError *error = NULL;

  g_set_prgname ("gtk4-rendernode-tool compare");
  context = g_option_context_new (NULL);
  g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);
  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_set_summary (context, _("Compare .node or .png files."));

  if (!g_option_context_parse (context, argc, (char ***)argv, &error))
    {
      g_printerr ("%s\n", error->message);
      g_error_free (error);
      exit (1);
    }

  g_option_context_free (context);

  if (filenames == NULL || g_strv_length (filenames) != 2)
    {
      g_printerr (_("Must specify two files\n"));
      exit (1);
    }

  renderer = create_renderer (renderer_name, &error);
  if (renderer == NULL)
    {
      g_printerr (_("Failed to create renderer: %s\n"), error->message);
      exit (1);
    }

  for (int i = 0; i < 2; i++)
    {
      texture[i] = texture_from_file (filenames[i], renderer, &error);
      if (texture[i] == NULL)
        {
          g_printerr (_("Failed to load %s: %s\n"), filenames[i], error->message);
          exit (1);
        }
    }

  diff = reftest_compare_textures (texture[0], texture[1]);

  if (opt_filename && diff)
    {
      if (!gdk_texture_save_to_png (diff, opt_filename))
        {
          g_printerr (_("Could not save diff image to %s\n"), opt_filename);
          exit (1);
        }
    }

  if (!opt_quiet)
    {
      if (diff)
        {
          if (opt_filename)
            g_print (_("Differences witten to %s.\n"), opt_filename);
          else
            g_print (_("The images are different.\n"));
        }
      else
        g_print (_("No differences.\n"));
    }

  if (diff)
    exit (1);

  g_strfreev (filenames);
  g_object_unref (texture[0]);
  g_object_unref (texture[1]);

  gsk_renderer_unrealize (renderer);
  g_object_unref (renderer);
}
