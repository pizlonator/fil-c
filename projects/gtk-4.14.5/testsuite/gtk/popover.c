#include <gtk/gtk.h>
#include <locale.h>
#include "gdk/gdkeventsprivate.h"

static gboolean
pop_up (gpointer data)
{
  gtk_popover_popup (GTK_POPOVER (data));

  return G_SOURCE_REMOVE;
}

static gboolean
tickle (gpointer data)
{
  GtkWidget *label;

  label = gtk_widget_get_first_child (GTK_WIDGET (data));
  gtk_widget_set_valign (label, GTK_ALIGN_START);

  return G_SOURCE_REMOVE;
}

static gboolean
stop (gpointer data)
{
  gboolean *done = data;

  *done = TRUE;

  g_main_context_wakeup (NULL);

  return G_SOURCE_REMOVE;
}

static void
test_show_popover (void)
{
  GtkWidget *window;
  GtkWidget *button;
  GtkWidget *popover;
  gboolean done;

  window = gtk_window_new ();
  button = gtk_menu_button_new ();
  popover = gtk_popover_new ();
  gtk_popover_set_child (GTK_POPOVER (popover), gtk_label_new ("Nu?"));
  gtk_menu_button_set_popover (GTK_MENU_BUTTON (button), popover);
  gtk_window_set_child (GTK_WINDOW (window), button);

  gtk_window_present (GTK_WINDOW (window));

  g_timeout_add (1000, pop_up, popover);
  g_timeout_add (2000, tickle, popover);
  done = FALSE;
  g_timeout_add (3000, stop, &done);

  while (!done)
    g_main_context_iteration (NULL, TRUE);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv);

  g_test_add_func ("/popover/show", test_show_popover);

  return g_test_run ();
}
