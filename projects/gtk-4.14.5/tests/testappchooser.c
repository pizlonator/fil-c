/* testappchooser.c
 * Copyright (C) 2010 Red Hat, Inc.
 * Authors: Cosimo Cecchi
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <stdlib.h>
#include <gtk/gtk.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

static GtkWidget *toplevel;
static GFile *file;
static GtkWidget *grid, *file_l, *open;
static GtkWidget *radio_file, *radio_content, *dialog;
static GtkWidget *app_chooser_widget;
static GtkWidget *def, *recommended, *fallback, *other, *all;

static void
dialog_response (GtkDialog *d,
                 int        response_id,
                 gpointer   user_data)
{
  GAppInfo *app_info;
  const char *name;

  g_print ("Response: %d\n", response_id);

  if (response_id == GTK_RESPONSE_OK)
    {
      app_info = gtk_app_chooser_get_app_info (GTK_APP_CHOOSER (d));
      if (app_info)
        {
          name = g_app_info_get_name (app_info);
          g_print ("Application selected: %s\n", name);
          g_object_unref (app_info);
        }
      else
        g_print ("No application selected\n");
    }

  gtk_window_destroy (GTK_WINDOW (d));
  dialog = NULL;
}

static void
bind_props (void)
{
  g_object_bind_property (def, "active",
                          app_chooser_widget, "show-default",
                          G_BINDING_SYNC_CREATE);
  g_object_bind_property (recommended, "active",
                          app_chooser_widget, "show-recommended",
                          G_BINDING_SYNC_CREATE);
  g_object_bind_property (fallback, "active",
                          app_chooser_widget, "show-fallback",
                          G_BINDING_SYNC_CREATE);
  g_object_bind_property (other, "active",
                          app_chooser_widget, "show-other",
                          G_BINDING_SYNC_CREATE);
  g_object_bind_property (all, "active",
                          app_chooser_widget, "show-all",
                          G_BINDING_SYNC_CREATE);
}

static void
prepare_dialog (void)
{
  gboolean use_file = FALSE;
  char *content_type = NULL;

  if (gtk_check_button_get_active (GTK_CHECK_BUTTON (radio_file)))
    use_file = TRUE;
  else if (gtk_check_button_get_active (GTK_CHECK_BUTTON (radio_content)))
    use_file = FALSE;

  if (use_file)
    {
      dialog = gtk_app_chooser_dialog_new (GTK_WINDOW (toplevel), 0, file);
    }
  else
    {
      GFileInfo *info;

      info = g_file_query_info (file,
                                G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
                                0, NULL, NULL);
      content_type = g_strdup (g_file_info_get_content_type (info));

      g_object_unref (info);

      dialog = gtk_app_chooser_dialog_new_for_content_type (GTK_WINDOW (toplevel),
                                                            0, content_type);
    }

  gtk_app_chooser_dialog_set_heading (GTK_APP_CHOOSER_DIALOG (dialog), "Select one already, you <i>fool</i>");

  g_signal_connect (dialog, "response",
                    G_CALLBACK (dialog_response), NULL);

  g_free (content_type);

  app_chooser_widget = gtk_app_chooser_dialog_get_widget (GTK_APP_CHOOSER_DIALOG (dialog));
  bind_props ();
}

static void
display_dialog (void)
{
  if (dialog == NULL)
    prepare_dialog ();

  gtk_window_present (GTK_WINDOW (dialog));
}

static void
on_open_response (GtkWidget *file_chooser,
                  int        response)
{
  if (response == GTK_RESPONSE_ACCEPT)
    {
      char *path;

      file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (file_chooser));
      path = g_file_get_path (file);

      gtk_button_set_label (GTK_BUTTON (file_l), path);

      g_free (path);
    }

  gtk_window_destroy (GTK_WINDOW (file_chooser));

  gtk_widget_set_sensitive (open, TRUE);
}

static void
button_clicked (GtkButton *b,
                gpointer   user_data)
{
  GtkWidget *w;

  w = gtk_file_chooser_dialog_new ("Select file",
                                   GTK_WINDOW (toplevel),
                                   GTK_FILE_CHOOSER_ACTION_OPEN,
                                   "_Cancel", GTK_RESPONSE_CANCEL,
                                   "_Open", GTK_RESPONSE_ACCEPT,
                                   NULL);

  gtk_window_set_modal (GTK_WINDOW (w), TRUE);

  g_signal_connect (w, "response",
                    G_CALLBACK (on_open_response),
                    NULL);

  gtk_window_present (GTK_WINDOW (w));
}

static void
quit_cb (GtkWidget *widget,
         gpointer   data)
{
  gboolean *done = data;

  *done = TRUE;

  g_main_context_wakeup (NULL);
}

int
main (int argc, char **argv)
{
  GtkWidget *w1;
  char *path;
  gboolean done = FALSE;

  gtk_init ();

  toplevel = gtk_window_new ();
  grid = gtk_grid_new ();

  w1 = gtk_label_new ("File:");
  gtk_widget_set_halign (w1, GTK_ALIGN_START);
  gtk_grid_attach (GTK_GRID (grid),
                   w1, 0, 0, 1, 1);

  file_l = gtk_button_new ();
  path = g_build_filename (GTK_SRCDIR, "apple-red.png", NULL);
  file = g_file_new_for_path (path);
  gtk_button_set_label (GTK_BUTTON (file_l), path);
  g_free (path);

  gtk_widget_set_halign (file_l, GTK_ALIGN_START);
  gtk_grid_attach_next_to (GTK_GRID (grid), file_l,
                           w1, GTK_POS_RIGHT, 3, 1);
  g_signal_connect (file_l, "clicked",
                    G_CALLBACK (button_clicked), NULL);

  radio_file = gtk_check_button_new_with_label ("Use GFile");
  radio_content = gtk_check_button_new_with_label ("Use content type");
  gtk_check_button_set_group (GTK_CHECK_BUTTON (radio_content), GTK_CHECK_BUTTON (radio_file));
  gtk_check_button_set_group (GTK_CHECK_BUTTON (radio_file), GTK_CHECK_BUTTON (radio_content));
  gtk_check_button_set_active (GTK_CHECK_BUTTON (radio_file), TRUE);

  gtk_grid_attach (GTK_GRID (grid), radio_file,
                   0, 1, 1, 1);
  gtk_grid_attach_next_to (GTK_GRID (grid), radio_content,
                           radio_file, GTK_POS_BOTTOM, 1, 1);

  open = gtk_button_new_with_label ("Trigger App Chooser dialog");
  gtk_grid_attach_next_to (GTK_GRID (grid), open,
                           radio_content, GTK_POS_BOTTOM, 1, 1);

  recommended = gtk_check_button_new_with_label ("Show recommended");
  gtk_grid_attach_next_to (GTK_GRID (grid), recommended,
                           open, GTK_POS_BOTTOM, 1, 1);
  g_object_set (recommended, "active", TRUE, NULL);

  fallback = gtk_check_button_new_with_label ("Show fallback");
  gtk_grid_attach_next_to (GTK_GRID (grid), fallback,
                           recommended, GTK_POS_RIGHT, 1, 1);

  other = gtk_check_button_new_with_label ("Show other");
  gtk_grid_attach_next_to (GTK_GRID (grid), other,
                           fallback, GTK_POS_RIGHT, 1, 1);

  all = gtk_check_button_new_with_label ("Show all");
  gtk_grid_attach_next_to (GTK_GRID (grid), all,
                           other, GTK_POS_RIGHT, 1, 1);

  def = gtk_check_button_new_with_label ("Show default");
  gtk_grid_attach_next_to (GTK_GRID (grid), def,
                           all, GTK_POS_RIGHT, 1, 1);

  g_object_set (recommended, "active", TRUE, NULL);
  prepare_dialog ();
  g_signal_connect (open, "clicked",
                    G_CALLBACK (display_dialog), NULL);

  gtk_window_set_child (GTK_WINDOW (toplevel), grid);

  gtk_window_present (GTK_WINDOW (toplevel));
  g_signal_connect (toplevel, "destroy", G_CALLBACK (quit_cb), &done);

  while (!done)
    g_main_context_iteration (NULL, TRUE);

  return EXIT_SUCCESS;
}
