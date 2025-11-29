#include <gtk/gtk.h>

static void
window_role (void)
{
  GtkWidget *window = gtk_window_new ();

  gtk_test_accessible_assert_role (window, GTK_ACCESSIBLE_ROLE_APPLICATION);

  gtk_window_destroy (GTK_WINDOW (window));
}

static void
window_state (void)
{
  GtkWidget *window = gtk_window_new ();

  gtk_window_present (GTK_WINDOW (window));

  gtk_test_accessible_assert_state (window, GTK_ACCESSIBLE_STATE_HIDDEN, FALSE);

  gtk_widget_set_visible (window, FALSE);

  gtk_test_accessible_assert_state (window, GTK_ACCESSIBLE_STATE_HIDDEN, TRUE);

  gtk_window_destroy (GTK_WINDOW (window));
}

static void
window_properties (void)
{
  GtkWidget *window = gtk_window_new ();

  gtk_window_set_modal (GTK_WINDOW (window), TRUE);

  gtk_test_accessible_assert_property (window, GTK_ACCESSIBLE_PROPERTY_MODAL, TRUE);
  gtk_window_set_modal (GTK_WINDOW (window), FALSE);

  gtk_test_accessible_assert_property (window, GTK_ACCESSIBLE_PROPERTY_MODAL, FALSE);

  gtk_window_destroy (GTK_WINDOW (window));
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/a11y/window/role", window_role);
  g_test_add_func ("/a11y/window/state", window_state);
  g_test_add_func ("/a11y/window/properties", window_properties);

  return g_test_run ();
}
