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


static void
set_window_title (GtkWindow  *window,
                  const char *filename)
{
  char *name;

  name = g_path_get_basename (filename);
  gtk_window_set_title (window, name);
  g_free (name);
}

static void
quit_cb (GtkWidget *widget,
         gpointer   user_data)
{
  gboolean *is_done = user_data;

  *is_done = TRUE;

  g_main_context_wakeup (NULL);
}

static void
show_file (const char *filename,
           gboolean    decorated)
{
  GskRenderNode *node;
  GdkPaintable *paintable;
  GtkWidget *sw;
  GtkWidget *window;
  gboolean done = FALSE;
  GtkSnapshot *snapshot;
  GtkWidget *picture;

  node = load_node_file (filename);

  snapshot = gtk_snapshot_new ();
  gtk_snapshot_append_node (snapshot, node);
  paintable = gtk_snapshot_free_to_paintable (snapshot, NULL);

  picture = gtk_picture_new_for_paintable (paintable);
  gtk_picture_set_can_shrink (GTK_PICTURE (picture), FALSE);
  gtk_picture_set_content_fit (GTK_PICTURE (picture), GTK_CONTENT_FIT_SCALE_DOWN);

  sw = gtk_scrolled_window_new ();
  gtk_scrolled_window_set_propagate_natural_width (GTK_SCROLLED_WINDOW (sw), TRUE);
  gtk_scrolled_window_set_propagate_natural_height (GTK_SCROLLED_WINDOW (sw), TRUE);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), picture);

  window = gtk_window_new ();
  gtk_window_set_decorated (GTK_WINDOW (window), decorated);
  set_window_title (GTK_WINDOW (window), filename);
  gtk_window_set_child (GTK_WINDOW (window), sw);

  gtk_window_present (GTK_WINDOW (window));
  g_signal_connect (window, "destroy", G_CALLBACK (quit_cb), &done);

  while (!done)
    g_main_context_iteration (NULL, TRUE);

  g_clear_object (&paintable);
  g_clear_pointer (&node, gsk_render_node_unref);
}

void
do_show (int          *argc,
         const char ***argv)
{
  GOptionContext *context;
  char **filenames = NULL;
  gboolean decorated = TRUE;
  const GOptionEntry entries[] = {
    { "undecorated", 0, G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE, &decorated, N_("Don't add a titlebar"), NULL },
    { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &filenames, NULL, N_("FILE") },
    { NULL, }
  };
  GError *error = NULL;

  if (gdk_display_get_default () == NULL)
    {
      g_printerr (_("Could not initialize windowing system\n"));
      exit (1);
    }

  g_set_prgname ("gtk4-rendernode-tool show");
  context = g_option_context_new (NULL);
  g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);
  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_set_summary (context, _("Show the render node."));

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
      g_printerr (_("Can only preview a single .node file\n"));
      exit (1);
    }

  show_file (filenames[0], decorated);

  g_strfreev (filenames);
}
