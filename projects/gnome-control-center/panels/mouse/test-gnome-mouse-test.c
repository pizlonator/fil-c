#include <config.h>
#include <gtk/gtk.h>

#include "cc-mouse-resources.h"
#include "cc-mouse-test.h"

int main (int argc, char **argv)
{
  GtkWidget *widget;
  GtkWidget *window;

  gtk_init ();

  widget = cc_mouse_test_new ();

  window = gtk_window_new ();
  gtk_window_set_child (GTK_WINDOW (window), widget);
  gtk_window_present (GTK_WINDOW (window));

  while (g_list_model_get_n_items (gtk_window_get_toplevels ()) > 0)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
