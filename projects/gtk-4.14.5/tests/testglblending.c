#include <stdlib.h>
#include <gtk/gtk.h>

#include "gtkgears.h"

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
  GtkWidget *window, *fixed, *gears, *spinner;
  gboolean done = FALSE;

  gtk_init ();

  window = gtk_window_new ();
  gtk_window_set_title (GTK_WINDOW (window), "Test GL/gtk inter-blending");
  gtk_window_set_default_size (GTK_WINDOW (window), 250, 250);
  g_signal_connect (window, "destroy", G_CALLBACK (quit_cb), &done);

  fixed = gtk_fixed_new ();
  gtk_window_set_child (GTK_WINDOW (window), fixed);

  spinner = gtk_spinner_new ();
  gtk_spinner_start (GTK_SPINNER (spinner));
  gtk_widget_set_size_request (spinner, 50, 50);
  gtk_fixed_put (GTK_FIXED (fixed), spinner, 90, 80);

  spinner = gtk_spinner_new ();
  gtk_spinner_start (GTK_SPINNER (spinner));
  gtk_widget_set_size_request (spinner, 50, 50);
  gtk_fixed_put (GTK_FIXED (fixed), spinner, 100, 80);

  spinner = gtk_spinner_new ();
  gtk_spinner_start (GTK_SPINNER (spinner));
  gtk_widget_set_size_request (spinner, 50, 50);
  gtk_fixed_put (GTK_FIXED (fixed), spinner, 110, 80);


  gears = gtk_gears_new ();
  gtk_widget_set_size_request (gears, 70, 50);
  gtk_fixed_put (GTK_FIXED (fixed), gears, 60, 100);

  spinner = gtk_spinner_new ();
  gtk_spinner_start (GTK_SPINNER (spinner));
  gtk_widget_set_size_request (spinner, 50, 50);
  gtk_fixed_put (GTK_FIXED (fixed), spinner, 90, 110);

  spinner = gtk_spinner_new ();
  gtk_spinner_start (GTK_SPINNER (spinner));
  gtk_widget_set_size_request (spinner, 50, 50);
  gtk_fixed_put (GTK_FIXED (fixed), spinner, 100, 110);

  spinner = gtk_spinner_new ();
  gtk_spinner_start (GTK_SPINNER (spinner));
  gtk_widget_set_size_request (spinner, 50, 50);
  gtk_fixed_put (GTK_FIXED (fixed), spinner, 110, 110);


  gears = gtk_gears_new ();
  gtk_widget_set_size_request (gears, 70, 50);
  gtk_fixed_put (GTK_FIXED (fixed), gears, 60, 130);

  spinner = gtk_spinner_new ();
  gtk_spinner_start (GTK_SPINNER (spinner));
  gtk_widget_set_size_request (spinner, 50, 50);
  gtk_fixed_put (GTK_FIXED (fixed), spinner, 90, 150);

  spinner = gtk_spinner_new ();
  gtk_spinner_start (GTK_SPINNER (spinner));
  gtk_widget_set_size_request (spinner, 50, 50);
  gtk_fixed_put (GTK_FIXED (fixed), spinner, 100, 150);

  spinner = gtk_spinner_new ();
  gtk_spinner_start (GTK_SPINNER (spinner));
  gtk_widget_set_size_request (spinner, 50, 50);
  gtk_fixed_put (GTK_FIXED (fixed), spinner, 110, 150);

  gtk_window_present (GTK_WINDOW (window));

  while (!done)
    g_main_context_iteration (NULL, TRUE);

  return EXIT_SUCCESS;
}
