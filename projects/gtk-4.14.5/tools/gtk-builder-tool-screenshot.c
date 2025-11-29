/*  Copyright 2015 Red Hat, Inc.
 *
 * GTK is free software; you can redistribute it and/or modify it
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
#include "gtkbuilderprivate.h"
#include "gtk-builder-tool.h"

static gboolean
quit_when_idle (gpointer loop)
{
  g_main_loop_quit (loop);

  return G_SOURCE_REMOVE;
}

static GMainLoop *loop;

static void
draw_paintable (GdkPaintable *paintable,
                gpointer      out_texture)
{
  GtkSnapshot *snapshot;
  GskRenderNode *node;
  GdkTexture *texture;
  GskRenderer *renderer;
  graphene_rect_t bounds;

  snapshot = gtk_snapshot_new ();
  gdk_paintable_snapshot (paintable,
                          snapshot,
                          gdk_paintable_get_intrinsic_width (paintable),
                          gdk_paintable_get_intrinsic_height (paintable));
  node = gtk_snapshot_free_to_node (snapshot);

  /* If the window literally draws nothing, we assume it hasn't been mapped yet and as such
   * the invalidations were only side effects of resizes.
   */
  if (node == NULL)
    return;

  if (gsk_render_node_get_node_type (node) == GSK_CLIP_NODE)
    {
      GskRenderNode *child;

      child = gsk_render_node_ref (gsk_clip_node_get_child (node));
      gsk_render_node_unref (node);
      node = child;
    }

  renderer = gtk_native_get_renderer (
                 gtk_widget_get_native (
                     gtk_widget_paintable_get_widget (GTK_WIDGET_PAINTABLE (paintable))));

  gsk_render_node_get_bounds (node, &bounds);
  graphene_rect_union (&bounds,
                       &GRAPHENE_RECT_INIT (
                         0, 0,
                         gdk_paintable_get_intrinsic_width (paintable),
                         gdk_paintable_get_intrinsic_height (paintable)
                       ),
                       &bounds);

  texture = gsk_renderer_render_texture (renderer, node, &bounds);
  g_object_set_data_full (G_OBJECT (texture),
                          "source-render-node",
                          node,
                          (GDestroyNotify) gsk_render_node_unref);

  g_signal_handlers_disconnect_by_func (paintable, draw_paintable, out_texture);

  *(GdkTexture **) out_texture = texture;

  g_idle_add (quit_when_idle, loop);
}

static GdkTexture *
snapshot_widget (GtkWidget *widget)
{
  GdkPaintable *paintable;
  GdkTexture *texture = NULL;

  g_assert_true (gtk_widget_get_realized (widget));

  loop = g_main_loop_new (NULL, FALSE);

  /* We wait until the widget is drawn for the first time.
   *
   * We also use an inhibit mechanism, to give module functions a chance
   * to delay the snapshot.
   */
  paintable = gtk_widget_paintable_new (widget);
  g_signal_connect (paintable, "invalidate-contents", G_CALLBACK (draw_paintable), &texture);
  g_main_loop_run (loop);

  g_main_loop_unref (loop);
  g_object_unref (paintable);

  return texture;
}

static void
set_window_title (GtkWindow  *window,
                  const char *filename,
                  const char *id)
{
  char *name;
  char *title;

  name = g_path_get_basename (filename);

  if (id)
    title = g_strdup_printf ("%s in %s", id, name);
  else
    title = g_strdup (name);

  gtk_window_set_title (window, title);

  g_free (title);
  g_free (name);
}

static char *
get_save_filename (const char *filename,
                   gboolean    as_node)
{
  int length = strlen (filename);
  const char *extension = as_node ? ".node" : ".png";
  char *result;

  if (strcmp (filename + (length - 3), ".ui") == 0)
    {
      char *basename = g_strndup (filename, length - 3);
      result = g_strconcat (basename, extension, NULL);
      g_free (basename);
    }
  else
    result = g_strconcat (filename, extension, NULL);

  return result;
}

static void
screenshot_file (const char *filename,
                 const char *id,
                 const char *cssfile,
                 const char *save_file,
                 gboolean    as_node,
                 gboolean    force)
{
  GtkBuilder *builder;
  GError *error = NULL;
  GObject *object;
  GtkWidget *window;
  GtkWidget *menu_button = NULL;
  GtkWidget *target = NULL;
  GdkTexture *texture;
  char *save_to;
  GBytes *bytes;

  if (cssfile)
    {
      GtkCssProvider *provider;

      provider = gtk_css_provider_new ();
      gtk_css_provider_load_from_path (provider, cssfile);

      gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                                  GTK_STYLE_PROVIDER (provider),
                                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }

  builder = gtk_builder_new ();
  if (!gtk_builder_add_from_file (builder, filename, &error))
    {
      g_printerr ("%s\n", error->message);
      exit (1);
    }

  object = NULL;

  if (id)
    {
      object = gtk_builder_get_object (builder, id);
    }
  else
    {
      GSList *objects, *l;

      objects = gtk_builder_get_objects (builder);
      for (l = objects; l; l = l->next)
        {
          GObject *obj = l->data;

          if (GTK_IS_WINDOW (obj))
            {
              object = obj;
              break;
            }
          else if (GTK_IS_WIDGET (obj))
            {
              if (object == NULL)
                object = obj;
            }
        }
      g_slist_free (objects);
    }

  if (object == NULL)
    {
      if (id)
        g_printerr (_("No object with ID '%s' found\n"), id);
      else
        g_printerr (_("No object found\n"));
      exit (1);
    }

  if (!GTK_IS_WIDGET (object))
    {
      g_printerr (_("Objects of type %s can't be screenshot\n"), G_OBJECT_TYPE_NAME (object));
      exit (1);
    }

  if (GTK_IS_WINDOW (object))
    {
      window = GTK_WIDGET (object);
      target = window;
    }
  else if (GTK_IS_POPOVER (object))
    {
      window = gtk_window_new ();

      if (GTK_IS_BUILDABLE (object))
        id = gtk_buildable_get_buildable_id (GTK_BUILDABLE (object));

      set_window_title (GTK_WINDOW (window), filename, id);

      menu_button = gtk_menu_button_new ();
      gtk_menu_button_set_popover (GTK_MENU_BUTTON (menu_button), GTK_WIDGET (object));
      gtk_window_set_child (GTK_WINDOW (window), menu_button);

      target = GTK_WIDGET (object);
    }
  else
    {
      GtkWidget *widget = GTK_WIDGET (object);

      window = gtk_window_new ();

      if (GTK_IS_BUILDABLE (object))
        id = gtk_buildable_get_buildable_id (GTK_BUILDABLE (object));

      set_window_title (GTK_WINDOW (window), filename, id);

      g_object_ref (widget);
      if (gtk_widget_get_parent (widget) != NULL)
        gtk_box_remove (GTK_BOX (gtk_widget_get_parent (widget)), widget);
      gtk_window_set_child (GTK_WINDOW (window), widget);
      g_object_unref (widget);

      target = widget;
    }

  gtk_widget_set_visible (window, TRUE);
  if (menu_button)
    gtk_menu_button_popup (GTK_MENU_BUTTON (menu_button));

  texture = snapshot_widget (target);

  g_object_unref (builder);

  if (texture == NULL)
    {
      g_printerr (_("Failed to take a screenshot\n"));
      exit (1);
    }

  save_to = (char *)save_file;

  if (save_to == NULL)
    save_to = get_save_filename (filename, as_node);

  if (g_file_test (save_to, G_FILE_TEST_EXISTS) && !force)
    {
      g_printerr (_("File %s exists.\n"
                    "Use --force to overwrite.\n"), save_to);
      exit (1);
    }

  if (as_node)
    {
      GskRenderNode *node;

      node = (GskRenderNode *) g_object_get_data (G_OBJECT (texture), "source-render-node");
      bytes = gsk_render_node_serialize (node);
    }
  else
    {
      bytes = gdk_texture_save_to_png_bytes (texture);
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

  g_object_unref (texture);
}

void
do_screenshot (int          *argc,
               const char ***argv)
{
  GOptionContext *context;
  char *id = NULL;
  char *css = NULL;
  char **filenames = NULL;
  gboolean as_node = FALSE;
  gboolean force = FALSE;
  const GOptionEntry entries[] = {
    { "id", 0, 0, G_OPTION_ARG_STRING, &id, N_("Screenshot only the named object"), N_("ID") },
    { "css", 0, 0, G_OPTION_ARG_FILENAME, &css, N_("Use style from CSS file"), N_("FILE") },
    { "node", 0, 0, G_OPTION_ARG_NONE, &as_node, N_("Save as node file instead of png"), NULL },
    { "force", 0, 0, G_OPTION_ARG_NONE, &force, N_("Overwrite existing file"), NULL },
    { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &filenames, NULL, N_("FILE…") },
    { NULL, }
  };
  GError *error = NULL;

  if (gdk_display_get_default () == NULL)
    {
      g_printerr (_("Could not initialize windowing system\n"));
      exit (1);
    }

  g_set_prgname ("gtk4-builder-tool render");
  context = g_option_context_new (NULL);
  g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);
  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_set_summary (context, _("Render a .ui file to an image."));

  if (!g_option_context_parse (context, argc, (char ***)argv, &error))
    {
      g_printerr ("%s\n", error->message);
      g_error_free (error);
      exit (1);
    }

  g_option_context_free (context);

  if (filenames == NULL)
    {
      g_printerr (_("No .ui file specified\n"));
      exit (1);
    }

  if (g_strv_length (filenames) > 2)
    {
      g_printerr (_("Can only render a single .ui file to a single output file\n"));
      exit (1);
    }

  screenshot_file (filenames[0], id, css, filenames[1], as_node, force);

  g_strfreev (filenames);
  g_free (id);
  g_free (css);
}
