#include <gtk/gtk.h>

static void
level_bar_role (void)
{
  GtkWidget *widget = gtk_level_bar_new ();
  g_object_ref_sink (widget);

  gtk_test_accessible_assert_role (widget, GTK_ACCESSIBLE_ROLE_METER);

  g_object_unref (widget);
}

static void
level_bar_properties (void)
{
  GtkWidget *widget = gtk_level_bar_new ();
  g_object_ref_sink (widget);

  gtk_test_accessible_assert_property (widget, GTK_ACCESSIBLE_PROPERTY_VALUE_MAX, 1.);
  gtk_test_accessible_assert_property (widget, GTK_ACCESSIBLE_PROPERTY_VALUE_MIN, 0.);
  gtk_test_accessible_assert_property (widget, GTK_ACCESSIBLE_PROPERTY_VALUE_NOW, 0.);
  gtk_test_accessible_assert_property (widget, GTK_ACCESSIBLE_PROPERTY_VALUE_TEXT, NULL);

  gtk_level_bar_set_max_value (GTK_LEVEL_BAR (widget), 100.0);
  gtk_level_bar_set_min_value (GTK_LEVEL_BAR (widget), 10.0);

  gtk_test_accessible_assert_property (widget, GTK_ACCESSIBLE_PROPERTY_VALUE_MAX, 100.0);
  gtk_test_accessible_assert_property (widget, GTK_ACCESSIBLE_PROPERTY_VALUE_MIN, 10.0);
  gtk_test_accessible_assert_property (widget, GTK_ACCESSIBLE_PROPERTY_VALUE_NOW, 10.0);

  gtk_level_bar_set_value (GTK_LEVEL_BAR (widget), 40.0);

  gtk_test_accessible_assert_property (widget, GTK_ACCESSIBLE_PROPERTY_VALUE_MAX, 100.0);
  gtk_test_accessible_assert_property (widget, GTK_ACCESSIBLE_PROPERTY_VALUE_MIN, 10.0);
  gtk_test_accessible_assert_property (widget, GTK_ACCESSIBLE_PROPERTY_VALUE_NOW, 40.0);

  g_object_unref (widget);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/a11y/levelbar/role", level_bar_role);
  g_test_add_func ("/a11y/levelbar/properties", level_bar_properties);

  return g_test_run ();
}
