#include <gtk.h>

gboolean done = FALSE;

static void
open_done (GObject *source,
           GAsyncResult *result,
           gpointer data)
{
  GtkFileDialog *dialog = GTK_FILE_DIALOG (source);
  GFile *file;
  GError *error = NULL;

  file = gtk_file_dialog_open_finish (dialog, result, &error);
  if (!file)
    {
      g_print ("Error: %s %d %s\n", g_quark_to_string (error->domain), error->code, error->message);
      g_error_free (error);
    }
  else
    {
      g_print ("%s\n", g_file_peek_path (file));
      g_object_unref (file);
    }

  done = TRUE;
}

static void
select_done (GObject *source,
             GAsyncResult *result,
             gpointer data)
{
  GtkFileDialog *dialog = GTK_FILE_DIALOG (source);
  GFile *file;
  GError *error = NULL;

  file = gtk_file_dialog_select_folder_finish (dialog, result, &error);
  if (!file)
    {
      g_print ("Error: %s %d %s\n", g_quark_to_string (error->domain), error->code, error->message);
      g_error_free (error);
    }
  else
    {
      g_print ("%s\n", g_file_peek_path (file));
      g_object_unref (file);
    }

  done = TRUE;
}

static void
save_done (GObject *source,
           GAsyncResult *result,
           gpointer data)
{
  GtkFileDialog *dialog = GTK_FILE_DIALOG (source);
  GFile *file;
  GError *error = NULL;

  file = gtk_file_dialog_save_finish (dialog, result, &error);
  if (!file)
    {
      g_print ("Error: %s %d %s\n", g_quark_to_string (error->domain), error->code, error->message);
      g_error_free (error);
    }
  else
    {
      g_print ("%s\n", g_file_peek_path (file));
      g_object_unref (file);
    }

  done = TRUE;
}

static void
open_multiple_done (GObject *source,
                    GAsyncResult *result,
                    gpointer data)
{
  GtkFileDialog *dialog = GTK_FILE_DIALOG (source);
  GListModel *model;
  GError *error = NULL;

  model = gtk_file_dialog_open_multiple_finish (dialog, result, &error);
  if (!model)
    {
      g_print ("Error: %s %d %s\n", g_quark_to_string (error->domain), error->code, error->message);
      g_error_free (error);
    }
  else
    {
      for (unsigned int i = 0; i < g_list_model_get_n_items (model); i++)
        {
          GFile *file = g_list_model_get_item (model, i);
          g_print ("%s\n", g_file_peek_path (file));
          g_object_unref (file);
        }
      g_object_unref (model);
    }

  done = TRUE;
}

static void
select_multiple_done (GObject *source,
                      GAsyncResult *result,
                      gpointer data)
{
  GtkFileDialog *dialog = GTK_FILE_DIALOG (source);
  GListModel *model;
  GError *error = NULL;

  model = gtk_file_dialog_select_multiple_folders_finish (dialog, result, &error);
  if (!model)
    {
      g_print ("Error: %s %d %s\n", g_quark_to_string (error->domain), error->code, error->message);
      g_error_free (error);
    }
  else
    {
      for (unsigned int i = 0; i < g_list_model_get_n_items (model); i++)
        {
          GFile *file = g_list_model_get_item (model, i);
          g_print ("%s\n", g_file_peek_path (file));
          g_object_unref (file);
        }
      g_object_unref (model);
    }

  done = TRUE;
}

static int
cancel_dialog (gpointer data)
{
  GCancellable *cancellable = data;

  g_cancellable_cancel (cancellable);

  return G_SOURCE_REMOVE;
}

int
main (int argc, char *argv[])
{
  GtkFileDialog *dialog;
  GCancellable *cancellable;
  char *title = NULL;
  gboolean modal = TRUE;
  char *initial_folder = NULL;
  char *initial_name = NULL;
  char *initial_file = NULL;
  char *accept_label = NULL;
  int timeout = -1;
  GOptionEntry options[] = {
    { "title", 0, 0, G_OPTION_ARG_STRING, &title, "Title", "TITLE" },
    { "nonmodal", 0, G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE, &modal, "Non-modal", NULL },
    { "initial-folder", 0, 0, G_OPTION_ARG_FILENAME, &initial_folder, "Initial folder", "FOLDER" },
    { "initial-name", 0, 0, G_OPTION_ARG_STRING, &initial_name, "Initial name", "NAME" },
    { "initial-file", 0, 0, G_OPTION_ARG_FILENAME, &initial_file, "Initial file", "FILE" },
    { "accept-label", 0, 0, G_OPTION_ARG_STRING, &accept_label, "Accept label", "LABEL" },
    { "timeout", 0, 0, G_OPTION_ARG_INT, &timeout, "Timeout", "SECONDS" },
    { NULL }
  };
  char *action = NULL;
  GOptionContext *context;
  GError *error = NULL;

  context = g_option_context_new ("ACTION");
  g_option_context_add_main_entries (context, options, NULL);
  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_print ("Failed to parse args: %s\n", error->message);
      exit (1);
    }

  action = argv[1];

  gtk_init ();

  dialog = gtk_file_dialog_new ();

  if (title)
    gtk_file_dialog_set_title (dialog, title);
  gtk_file_dialog_set_modal (dialog, modal);
  if (initial_folder)
    {
      GFile *file = g_file_new_for_commandline_arg (initial_folder);
      gtk_file_dialog_set_initial_folder (dialog, file);
      g_object_unref (file);
    }
  if (initial_name)
    gtk_file_dialog_set_initial_name (dialog, initial_name);
  if (initial_file)
    {
      GFile *file = g_file_new_for_commandline_arg (initial_file);
      gtk_file_dialog_set_initial_file (dialog, file);
      g_object_unref (file);
    }
  if (accept_label)
    gtk_file_dialog_set_accept_label (dialog, accept_label);

  cancellable = g_cancellable_new ();

  if (timeout > 0)
    g_timeout_add_seconds (timeout, cancel_dialog, cancellable);

  if (action == NULL)
    {
      g_print ("no action\n");
      exit (1);
    }
  else if (strcmp (action, "open") == 0)
    gtk_file_dialog_open (dialog, NULL, cancellable, open_done, NULL);
  else if (g_pattern_match_simple ("select?folder", action) &&
           strchr ("-_", action[strlen ("select")]))
    gtk_file_dialog_select_folder (dialog, NULL, cancellable, select_done, NULL);
  else if (strcmp (action, "save") == 0)
    gtk_file_dialog_save (dialog, NULL, cancellable, save_done, NULL);
  else if (g_pattern_match_simple ("open?multiple", action) &&
           strchr ("-_", action[strlen ("open")]))
    gtk_file_dialog_open_multiple (dialog, NULL, cancellable, open_multiple_done, NULL);
  else if (g_pattern_match_simple ("select?multiple", action) &&
           strchr ("-_", action[strlen ("select")]))
    gtk_file_dialog_select_multiple_folders (dialog, NULL, cancellable, select_multiple_done, NULL);
  else
    {
      g_print ("invalid action: %s\n", action);
      g_print ("one of open, select-folder, save, open-multiple, select-multiple\n");
      exit (1);
    }

  while (!done)
    g_main_context_iteration (NULL, TRUE);

  g_object_unref (dialog);

  return 0;
}
