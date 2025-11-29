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
 * Author: Benjamin Otte
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

static void
benchmark_node (GskRenderNode *node,
                const char    *renderer_name,
                guint          runs,
                gboolean       download)
{
  GError *error = NULL;
  GskRenderer *renderer;
  guint i;

  renderer = create_renderer (renderer_name, &error);
  if (renderer == NULL)
    {
      g_printerr ("Could not benchmark renderer \"%s\": %s\n", renderer_name, error->message);
      g_clear_error (&error);
      return;
    }

  for (i = 0; i < runs; i++)
    {
      GdkTexture *texture;
      gint64 start_time, end_time, duration;

      start_time = g_get_monotonic_time ();

      texture = gsk_renderer_render_texture (renderer, node, NULL);
      if (download)
        {
          GdkTextureDownloader *downloader;
          GBytes *bytes;
          gsize stride;

          downloader = gdk_texture_downloader_new (texture);
          bytes = gdk_texture_downloader_download_bytes (downloader, &stride);
          g_bytes_unref (bytes);
          gdk_texture_downloader_free (downloader);
        }

      end_time = g_get_monotonic_time ();

      duration = end_time - start_time;
      g_print ("%s\t%lld.%03ds\n",
               renderer_name,
               (long long) duration / G_USEC_PER_SEC,
               (int) ((duration * 1000 / G_USEC_PER_SEC) % 1000)); 
      g_object_unref (texture);
    }

  gsk_renderer_unrealize (renderer);
  g_object_unref (renderer);
}

void
do_benchmark (int          *argc,
              const char ***argv)
{
  GOptionContext *context;
  char **filenames = NULL;
  char **renderers = NULL;
  gboolean nodownload = FALSE;
  int runs = 3;
  const GOptionEntry entries[] = {
    { "renderer", 0, 0, G_OPTION_ARG_STRING_ARRAY, &renderers, N_("Add renderer to benchmark"), N_("RENDERER") },
    { "runs", 0, 0, G_OPTION_ARG_INT, &runs, N_("Number of runs with each renderer"), N_("RUNS") },
    { "no-download", 0, 0, G_OPTION_ARG_NONE, &nodownload, N_("Don’t download result/wait for GPU to finish"), NULL },
    { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &filenames, NULL, N_("FILE…") },
    { NULL, }
  };
  GskRenderNode *node;
  GError *error = NULL;
  gsize i;

  if (gdk_display_get_default () == NULL)
    {
      g_printerr (_("Could not initialize windowing system\n"));
      exit (1);
    }

  g_set_prgname ("gtk4-rendernode-tool benchmark");
  context = g_option_context_new (NULL);
  g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);
  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_set_summary (context, _("Benchmark rendering of a .node file."));

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
      g_printerr (_("Can only benchmark a single .node file\n"));
      exit (1);
    }

  if (renderers == NULL || renderers[0] == NULL)
    renderers = g_strdupv ((char **) (const char *[]) { "gl", "ngl", "vulkan", "cairo", NULL });
  
  node = load_node_file (filenames[0]);

  for (i = 0; renderers[i] != NULL; i++)
    {
      benchmark_node (node, renderers[i], runs, !nodownload);
    }

  gsk_render_node_unref (node);

  g_strfreev (filenames);
  g_strfreev (renderers);
}
