#include <gtk/gtk.h>

static void
color_dialog_button_role (void)
{
  GtkWidget *window, *widget;

  widget = gtk_color_dialog_button_new (NULL);
  window = gtk_window_new ();
  gtk_window_set_child (GTK_WINDOW (window), widget);
  gtk_window_present (GTK_WINDOW (window));

  while (!gtk_widget_get_realized (widget))
    g_main_context_iteration (NULL, FALSE);

  gtk_test_accessible_assert_role (widget, GTK_ACCESSIBLE_ROLE_GROUP);

  gtk_window_destroy (GTK_WINDOW (window));
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/a11y/color-dialog-button/role", color_dialog_button_role);

  return g_test_run ();
}
