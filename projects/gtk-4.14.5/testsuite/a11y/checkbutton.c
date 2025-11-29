#include <gtk/gtk.h>

static void
check_button_role (void)
{
  GtkWidget *button = gtk_check_button_new ();
  g_object_ref_sink (button);

  gtk_test_accessible_assert_role (button, GTK_ACCESSIBLE_ROLE_CHECKBOX);

  g_object_unref (button);
}

static void
check_button_checked (void)
{
  GtkWidget *button = gtk_check_button_new ();
  g_object_ref_sink (button);

  gtk_test_accessible_assert_state (button, GTK_ACCESSIBLE_STATE_CHECKED, GTK_ACCESSIBLE_TRISTATE_FALSE);

  gtk_check_button_set_active (GTK_CHECK_BUTTON (button), TRUE);

  gtk_test_accessible_assert_state (button, GTK_ACCESSIBLE_STATE_CHECKED, GTK_ACCESSIBLE_TRISTATE_TRUE);

  gtk_check_button_set_inconsistent (GTK_CHECK_BUTTON (button), TRUE);

  gtk_test_accessible_assert_state (button, GTK_ACCESSIBLE_STATE_CHECKED, GTK_ACCESSIBLE_TRISTATE_MIXED);

  g_object_unref (button);
}

static void
check_button_label (void)
{
  GtkWidget *button = gtk_check_button_new_with_label ("Hello");
  g_object_ref_sink (button);

  gtk_test_accessible_assert_property (button, GTK_ACCESSIBLE_PROPERTY_LABEL, "Hello");

  g_object_unref (button);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/a11y/checkbutton/role", check_button_role);
  g_test_add_func ("/a11y/checkbutton/checked", check_button_checked);
  g_test_add_func ("/a11y/checkbutton/label", check_button_label);

  return g_test_run ();
}
