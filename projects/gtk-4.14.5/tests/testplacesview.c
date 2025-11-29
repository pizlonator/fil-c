#include <gtk/gtk.h>
#define GTK_COMPILATION
#include "gtk/gtkplacesviewprivate.h"

static void
quit_cb (GtkWidget *widget,
         gpointer   data)
{
  gboolean *done = data;

  *done = TRUE;

  g_main_context_wakeup (NULL);
}

int
main (int argc, char *argv[])
{
  GtkWidget *win;
  GtkWidget *view;
  gboolean done = FALSE;

  gtk_init ();

  win = gtk_window_new ();
  gtk_window_set_default_size (GTK_WINDOW (win), 400, 600);

  view = gtk_places_view_new ();

  gtk_window_set_child (GTK_WINDOW (win), view);
  gtk_window_present (GTK_WINDOW (win));

  g_signal_connect (win, "destroy", G_CALLBACK (quit_cb), &done);

  while (!done)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
