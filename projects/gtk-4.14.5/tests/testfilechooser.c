/* testfilechooser.c
 * Copyright (C) 2003  Red Hat, Inc.
 * Author: Owen Taylor
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

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <time.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <gtk/gtk.h>

#ifdef G_OS_WIN32
#  include <io.h>
#  define localtime_r(t,b) *(b) = *localtime (t)
#  ifndef S_ISREG
#    define S_ISREG(m) ((m) & _S_IFREG)
#  endif
#endif

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

#if 0
static GtkWidget *preview_label;
static GtkWidget *preview_image;
#endif
static GtkFileChooserAction action;

static void
response_cb (GtkDialog *dialog,
	     int        response_id,
             gpointer   data)
{
  gboolean *done = data;

  if (response_id == GTK_RESPONSE_OK)
    {
      GListModel *files;
      guint i, n;

      files = gtk_file_chooser_get_files (GTK_FILE_CHOOSER (dialog));
      n = g_list_model_get_n_items (files);

      g_print ("Selected files:\n");
      for (i = 0; i < n; i++)
        {
          GFile *file = g_list_model_get_item (files, i);
          char *uri = g_file_get_uri (file);
          g_print ("  %s\n", uri ? uri : "(null)");
          g_free (uri);
          g_object_unref (file);
        }

      g_object_unref (files);
    }
  else
    g_print ("Dialog was closed\n");

  *done = TRUE;

  g_main_context_wakeup (NULL);
}

static void
filter_changed (GtkFileChooserDialog *dialog,
		gpointer              data)
{
  g_print ("file filter changed\n");
}

#include <stdio.h>
#include <errno.h>
#define _(s) (s)


static void
set_current_folder (GtkFileChooser *chooser,
		    const char     *name)
{
  GFile *file = g_file_new_for_path (name);
  if (!gtk_file_chooser_set_current_folder (chooser, file, NULL))
    {
      GtkWidget *dialog;

      dialog = gtk_message_dialog_new (GTK_WINDOW (chooser),
				       GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
				       GTK_MESSAGE_ERROR,
				       GTK_BUTTONS_CLOSE,
				       "Could not set the folder to %s",
				       name);
      gtk_window_present (GTK_WINDOW (dialog));
      g_signal_connect (dialog, "response",
                        G_CALLBACK (gtk_window_destroy),
                        NULL);
    }
  g_object_unref (file);
}

static void
set_folder_nonexistent_cb (GtkButton      *button,
			   GtkFileChooser *chooser)
{
  set_current_folder (chooser, "/nonexistent");
}

static void
set_folder_existing_nonexistent_cb (GtkButton      *button,
				    GtkFileChooser *chooser)
{
  set_current_folder (chooser, "/usr/nonexistent");
}

static void
set_filename (GtkFileChooser *chooser,
	      const char     *name)
{
  GFile *file = g_file_new_for_path (name);
  if (!gtk_file_chooser_set_file (chooser, file, NULL))
    {
      GtkWidget *dialog;

      dialog = gtk_message_dialog_new (GTK_WINDOW (chooser),
				       GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
				       GTK_MESSAGE_ERROR,
				       GTK_BUTTONS_CLOSE,
				       "Could not select %s",
				       name);
      gtk_window_present (GTK_WINDOW (dialog));
      g_signal_connect (dialog, "response",
                        G_CALLBACK (gtk_window_destroy),
                        NULL);
    }
  g_object_unref (file);
}

static void
set_filename_nonexistent_cb (GtkButton      *button,
			     GtkFileChooser *chooser)
{
  set_filename (chooser, "/nonexistent");
}

static void
set_filename_existing_nonexistent_cb (GtkButton      *button,
				      GtkFileChooser *chooser)
{
  set_filename (chooser, "/usr/nonexistent");
}

static void
get_selection_cb (GtkButton      *button,
		  GtkFileChooser *chooser)
{
  GListModel *selection;
  guint i, n;

  selection = gtk_file_chooser_get_files (chooser);
  n = g_list_model_get_n_items (selection);

  g_print ("Selection: ");

  for (i = 0; i < n; i++)
    {
      GFile *file = g_list_model_get_item (selection, i);
      char *uri = g_file_get_uri (file);
      g_print ("%s\n", uri);
      g_free (uri);
      g_object_unref (file);
    }

  g_object_unref (selection);
}

static void
get_current_name_cb (GtkButton      *button,
		     GtkFileChooser *chooser)
{
  char *name;

  name = gtk_file_chooser_get_current_name (chooser);
  g_print ("Current name: %s\n", name ? name : "NULL");
  g_free (name);
}

static void
unmap_and_remap_cb (GtkButton *button,
		    GtkFileChooser *chooser)
{
  gtk_widget_set_visible (GTK_WIDGET (chooser), FALSE);
  gtk_widget_set_visible (GTK_WIDGET (chooser), TRUE);
}

static void
kill_dependent (GtkWindow *win, GtkWidget *dep)
{
  gtk_window_destroy (GTK_WINDOW (dep));
}

int
main (int argc, char **argv)
{
  GtkWidget *control_window;
  GtkWidget *vbbox;
  GtkWidget *button;
  GtkWidget *dialog;
  GtkFileFilter *filter;
  gboolean force_rtl = FALSE;
  gboolean multiple = FALSE;
  char *action_arg = NULL;
  char *initial_filename = NULL;
  char *initial_folder = NULL;
  GFile *file;
  GError *error = NULL;
  GOptionEntry options[] = {
    { "action", 'a', 0, G_OPTION_ARG_STRING, &action_arg, "Filechooser action", "ACTION" },
    { "multiple", 'm', 0, G_OPTION_ARG_NONE, &multiple, "Select multiple", NULL },
    { "right-to-left", 'r', 0, G_OPTION_ARG_NONE, &force_rtl, "Force right-to-left layout.", NULL },
    { "initial-filename", 'f', 0, G_OPTION_ARG_FILENAME, &initial_filename, "Initial filename to select", "FILENAME" },
    { "initial-folder", 'F', 0, G_OPTION_ARG_FILENAME, &initial_folder, "Initial folder to show", "FILENAME" },
    { NULL }
  };
  GOptionContext *context;
  gboolean done = FALSE;

  context = g_option_context_new ("");
  g_option_context_add_main_entries (context, options, NULL);
  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_print ("Failed to parse args: %s\n", error->message);
      g_error_free (error);
      return 1;
    }
  g_option_context_free (context);

  gtk_init ();

  if (initial_filename && initial_folder)
    {
      g_print ("Only one of --initial-filename and --initial-folder may be specified");
      return 1;
    }

  if (force_rtl)
    gtk_widget_set_default_direction (GTK_TEXT_DIR_RTL);

  action = GTK_FILE_CHOOSER_ACTION_OPEN;

  if (action_arg != NULL)
    {
      if (! strcmp ("open", action_arg))
	action = GTK_FILE_CHOOSER_ACTION_OPEN;
      else if (! strcmp ("save", action_arg))
	action = GTK_FILE_CHOOSER_ACTION_SAVE;
      else if (! strcmp ("select_folder", action_arg))
	action = GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER;
      else
	{
	  g_print ("--action must be one of \"open\", \"save\", \"select_folder\"\n");
	  return 1;
	}

      g_free (action_arg);
    }

  dialog = g_object_new (GTK_TYPE_FILE_CHOOSER_DIALOG,
			 "action", action,
			 "select-multiple", multiple,
			 NULL);

  switch (action)
    {
    case GTK_FILE_CHOOSER_ACTION_OPEN:
    case GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER:
      gtk_window_set_title (GTK_WINDOW (dialog), "Select a file");
      gtk_dialog_add_buttons (GTK_DIALOG (dialog),
			      _("_Cancel"), GTK_RESPONSE_CANCEL,
			      _("_Open"), GTK_RESPONSE_OK,
			      NULL);
      break;
    case GTK_FILE_CHOOSER_ACTION_SAVE:
      gtk_window_set_title (GTK_WINDOW (dialog), "Save a file");
      gtk_dialog_add_buttons (GTK_DIALOG (dialog),
			      _("_Cancel"), GTK_RESPONSE_CANCEL,
			      _("_Save"), GTK_RESPONSE_OK,
			      NULL);
      break;
    default:
      g_assert_not_reached ();
    }
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

  g_signal_connect (dialog, "response",
		    G_CALLBACK (response_cb), &done);

  /* Filters */
  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, "All Files");
  gtk_file_filter_add_pattern (filter, "*");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);

  /* Make this filter the default */
  gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (dialog), filter);
  g_object_unref (filter);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, "Starts with D");
  gtk_file_filter_add_pattern (filter, "D*");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);
  g_object_unref (filter);

  g_signal_connect (dialog, "notify::filter",
		    G_CALLBACK (filter_changed), NULL);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, "PNG and JPEG");
  gtk_file_filter_add_mime_type (filter, "image/jpeg");
  gtk_file_filter_add_mime_type (filter, "image/png");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);
  g_object_unref (filter);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, "Images");
  gtk_file_filter_add_pixbuf_formats (filter);
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);
  g_object_unref (filter);

  /* Choices */

  gtk_file_chooser_add_choice (GTK_FILE_CHOOSER (dialog), "choice1",
                               "Choose one:",
                               (const char *[]){"one", "two", "three", NULL},
                               (const char *[]){"One", "Two", "Three", NULL});
  gtk_file_chooser_set_choice (GTK_FILE_CHOOSER (dialog), "choice1", "two");

  /* Shortcuts */

  file = g_file_new_for_uri ("file:///usr/share/pixmaps");
  gtk_file_chooser_add_shortcut_folder (GTK_FILE_CHOOSER (dialog), file, NULL);
  g_object_unref (file);

  file = g_file_new_for_path (g_get_user_special_dir (G_USER_DIRECTORY_MUSIC));
  gtk_file_chooser_add_shortcut_folder (GTK_FILE_CHOOSER (dialog), file, NULL);
  g_object_unref (file);

  /* Initial filename or folder */

  if (initial_filename)
    set_filename (GTK_FILE_CHOOSER (dialog), initial_filename);

  if (initial_folder)
    set_current_folder (GTK_FILE_CHOOSER (dialog), initial_folder);

  gtk_window_present (GTK_WINDOW (dialog));

  /* Extra controls for manipulating the test environment
   */

  control_window = gtk_window_new ();

  vbbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_window_set_child (GTK_WINDOW (control_window), vbbox);

  button = gtk_button_new_with_label ("set_current_folder (\"/nonexistent\")");
  gtk_box_append (GTK_BOX (vbbox), button);
  g_signal_connect (button, "clicked",
		    G_CALLBACK (set_folder_nonexistent_cb), dialog);

  button = gtk_button_new_with_label ("set_current_folder (\"/usr/nonexistent\")");
  gtk_box_append (GTK_BOX (vbbox), button);
  g_signal_connect (button, "clicked",
		    G_CALLBACK (set_folder_existing_nonexistent_cb), dialog);

  button = gtk_button_new_with_label ("set_filename (\"/nonexistent\")");
  gtk_box_append (GTK_BOX (vbbox), button);
  g_signal_connect (button, "clicked",
		    G_CALLBACK (set_filename_nonexistent_cb), dialog);

  button = gtk_button_new_with_label ("set_filename (\"/usr/nonexistent\")");
  gtk_box_append (GTK_BOX (vbbox), button);
  g_signal_connect (button, "clicked",
		    G_CALLBACK (set_filename_existing_nonexistent_cb), dialog);

  button = gtk_button_new_with_label ("Get selection");
  gtk_box_append (GTK_BOX (vbbox), button);
  g_signal_connect (button, "clicked",
		    G_CALLBACK (get_selection_cb), dialog);

  button = gtk_button_new_with_label ("Get current name");
  gtk_box_append (GTK_BOX (vbbox), button);
  g_signal_connect (button, "clicked",
		    G_CALLBACK (get_current_name_cb), dialog);

  button = gtk_button_new_with_label ("Unmap and remap");
  gtk_box_append (GTK_BOX (vbbox), button);
  g_signal_connect (button, "clicked",
		    G_CALLBACK (unmap_and_remap_cb), dialog);

  gtk_widget_show (control_window);

  g_signal_connect (dialog, "destroy",
		    G_CALLBACK (kill_dependent), control_window);

  /* We need to hold a ref until we have destroyed the widgets, just in case
   * someone else destroys them.  We explicitly destroy windows to catch leaks.
   */
  g_object_ref (dialog);
  while (!done)
    g_main_context_iteration (NULL, TRUE);
  gtk_window_destroy (GTK_WINDOW (dialog));
  g_object_unref (dialog);

  return 0;
}
