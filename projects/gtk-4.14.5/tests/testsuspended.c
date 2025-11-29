#include <gtk/gtk.h>

static void
quit_cb (GtkWidget *widget,
         gpointer   data)
{
  gboolean *done = data;

  *done = TRUE;

  g_main_context_wakeup (NULL);
}

static void
report_suspended_state (GtkWindow *window)
{
  g_print ("Window is %s\n",
           gtk_window_is_suspended (window) ? "suspended" : "active");
}

static void
suspended_changed_cb (GtkWindow *window)
{
  report_suspended_state (window);
}

int main (int argc, char *argv[])
{
  GtkWidget *window;
  gboolean done = FALSE;

  gtk_init ();

  window = gtk_window_new ();
  gtk_window_set_default_size (GTK_WINDOW (window), 200, 200);
  g_signal_connect (window, "destroy", G_CALLBACK (quit_cb), &done);
  g_signal_connect (window, "notify::suspended",
                    G_CALLBACK (suspended_changed_cb), &done);
  gtk_window_present (GTK_WINDOW (window));
  report_suspended_state (GTK_WINDOW (window));

  while (!done)
    g_main_context_iteration (NULL, TRUE);

  return EXIT_SUCCESS;
}
