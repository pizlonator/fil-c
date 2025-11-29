#include <gtk/gtk.h>

static void
launched_cb (GObject *source,
             GAsyncResult *result,
             gpointer data)
{
  GtkFileLauncher *launcher = GTK_FILE_LAUNCHER (source);
  GError *error = NULL;

  if (!gtk_file_launcher_launch_finish (launcher, result, &error))
    {
      g_print ("Launching failed: %s\n", error->message);
      g_error_free (error);
    }
}

int
main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkFileLauncher *launcher;

  gtk_init ();

  window = gtk_window_new ();

  launcher = gtk_file_launcher_new (NULL);

  gtk_window_present (GTK_WINDOW (window));

  for (int i = 1; i < argc; i++)
    {
      GFile *file = g_file_new_for_commandline_arg (argv[i]);

      g_print ("launching %s\n", argv[i]);

      gtk_file_launcher_set_file (launcher, file);
      gtk_file_launcher_launch (launcher, GTK_WINDOW (window), NULL, launched_cb, NULL);
      g_object_unref (file);
    }

  while (g_list_model_get_n_items (gtk_window_get_toplevels ()) > 0)
    g_main_context_iteration (NULL, FALSE);

  return 0;
}
