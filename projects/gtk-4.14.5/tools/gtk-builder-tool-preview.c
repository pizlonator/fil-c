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

static void
quit_cb (GtkWidget *widget,
         gpointer   user_data)
{
  gboolean *is_done = user_data;

  *is_done = TRUE;

  g_main_context_wakeup (NULL);
}

static void
preview_file (const char *filename,
              const char *id,
              const char *cssfile)
{
  GtkBuilder *builder;
  GError *error = NULL;
  GObject *object;
  GtkWidget *window;
  gboolean done = FALSE;

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
        g_printerr (_("No previewable object found\n"));
      exit (1);
    }

  if (!GTK_IS_WIDGET (object))
    {
      g_printerr (_("Objects of type %s can't be previewed\n"), G_OBJECT_TYPE_NAME (object));
      exit (1);
    }

  if (GTK_IS_WINDOW (object))
    window = GTK_WIDGET (object);
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
    }

  gtk_window_present (GTK_WINDOW (window));
  g_signal_connect (window, "destroy", G_CALLBACK (quit_cb), &done);

  while (!done)
    g_main_context_iteration (NULL, TRUE);

  g_object_unref (builder);
}

void
do_preview (int          *argc,
            const char ***argv)
{
  GOptionContext *context;
  char *id = NULL;
  char *css = NULL;
  char **filenames = NULL;
  const GOptionEntry entries[] = {
    { "id", 0, 0, G_OPTION_ARG_STRING, &id, N_("Preview only the named object"), N_("ID") },
    { "css", 0, 0, G_OPTION_ARG_FILENAME, &css, N_("Use style from CSS file"), N_("FILE") },
    { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &filenames, NULL, N_("FILE") },
    { NULL, }
  };
  GError *error = NULL;

  if (gdk_display_get_default () == NULL)
    {
      g_printerr (_("Could not initialize windowing system\n"));
      exit (1);
    }

  g_set_prgname ("gtk4-builder-tool preview");
  context = g_option_context_new (NULL);
  g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);
  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_set_summary (context, _("Preview the file."));

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

  if (g_strv_length (filenames) > 1)
    {
      g_printerr (_("Can only preview a single .ui file\n"));
      exit (1);
    }

  preview_file (filenames[0], id, css);

  g_strfreev (filenames);
  g_free (id);
  g_free (css);
}
