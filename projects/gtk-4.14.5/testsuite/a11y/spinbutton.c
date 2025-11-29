#include <gtk/gtk.h>

static void
spinbutton_role (void)
{
  GtkWidget *widget = gtk_spin_button_new_with_range (0, 100, 1);
  g_object_ref_sink (widget);

  gtk_test_accessible_assert_role (widget, GTK_ACCESSIBLE_ROLE_SPIN_BUTTON);

  g_object_unref (widget);
}

static void
spinbutton_properties (void)
{
  GtkWidget *widget = gtk_spin_button_new_with_range (0, 100, 1);
  g_object_ref_sink (widget);

  gtk_test_accessible_assert_property (widget, GTK_ACCESSIBLE_PROPERTY_VALUE_MAX, 100.);
  gtk_test_accessible_assert_property (widget, GTK_ACCESSIBLE_PROPERTY_VALUE_MIN, 0.);
  gtk_test_accessible_assert_property (widget, GTK_ACCESSIBLE_PROPERTY_VALUE_NOW, 0.);

  gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), 50.);

  gtk_test_accessible_assert_property (widget, GTK_ACCESSIBLE_PROPERTY_VALUE_MAX, 100.);
  gtk_test_accessible_assert_property (widget, GTK_ACCESSIBLE_PROPERTY_VALUE_MIN, 0.);
  gtk_test_accessible_assert_property (widget, GTK_ACCESSIBLE_PROPERTY_VALUE_NOW, 50.0);

  g_object_unref (widget);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/a11y/spinbutton/role", spinbutton_role);
  g_test_add_func ("/a11y/spinbutton/properties", spinbutton_properties);

  return g_test_run ();
}
