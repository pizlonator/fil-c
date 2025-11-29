#include <gtk/gtk.h>

static gboolean
main_loop_quit_cb (gpointer data)
{
  gboolean *done = data;

  *done = TRUE;

  g_main_context_wakeup (NULL);

  return FALSE;
}

static void
show_and_wait (GtkWidget *widget)
{
  gboolean done = FALSE;

  g_timeout_add (500, main_loop_quit_cb, &done);
  gtk_widget_set_visible (widget, TRUE);
  while (!done)
    g_main_context_iteration (NULL, FALSE);
}

/* this was triggering a crash in gtk_flow_box_measure(),
 * see #2702
 */
static void
test_measure_crash (void)
{
  GtkWidget *window, *box, *child;

  window = gtk_window_new ();
  box = gtk_flow_box_new ();
  gtk_widget_set_valign (GTK_WIDGET (box), GTK_ALIGN_START);
  child = g_object_new (GTK_TYPE_FLOW_BOX_CHILD,
                        "css-name", "nopadding",
                        NULL);
  gtk_flow_box_insert (GTK_FLOW_BOX (box), child, -1);
  gtk_orientable_set_orientation (GTK_ORIENTABLE (box), GTK_ORIENTATION_VERTICAL);
  gtk_flow_box_set_row_spacing (GTK_FLOW_BOX (box), 0);

  gtk_window_set_child (GTK_WINDOW (window), box);

  show_and_wait (window);

  gtk_window_destroy (GTK_WINDOW (window));
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv);

  g_test_add_func ("/flowbox/measure-crash", test_measure_crash);

  return g_test_run ();
}
