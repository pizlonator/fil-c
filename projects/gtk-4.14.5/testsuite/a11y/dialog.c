#include <gtk/gtk.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

static void
dialog_role (void)
{
  GtkWidget *dialog = gtk_dialog_new ();

  gtk_test_accessible_assert_role (dialog, GTK_ACCESSIBLE_ROLE_DIALOG);

  gtk_window_destroy (GTK_WINDOW (dialog));
}

static void
dialog_state (void)
{
  GtkWidget *dialog = gtk_dialog_new ();

  gtk_window_present (GTK_WINDOW (dialog));

  gtk_test_accessible_assert_state (dialog, GTK_ACCESSIBLE_STATE_HIDDEN, FALSE);

  gtk_widget_hide (dialog);

  gtk_test_accessible_assert_state (dialog, GTK_ACCESSIBLE_STATE_HIDDEN, TRUE);

  gtk_window_destroy (GTK_WINDOW (dialog));
}

static void
dialog_properties (void)
{
  GtkWidget *dialog = gtk_dialog_new ();

  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);

  gtk_test_accessible_assert_property (dialog, GTK_ACCESSIBLE_PROPERTY_MODAL, TRUE);
  gtk_window_set_modal (GTK_WINDOW (dialog), FALSE);

  gtk_test_accessible_assert_property (dialog, GTK_ACCESSIBLE_PROPERTY_MODAL, FALSE);

  gtk_window_destroy (GTK_WINDOW (dialog));
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/a11y/dialog/role", dialog_role);
  g_test_add_func ("/a11y/dialog/state", dialog_state);
  g_test_add_func ("/a11y/dialog/properties", dialog_properties);

  return g_test_run ();
}
