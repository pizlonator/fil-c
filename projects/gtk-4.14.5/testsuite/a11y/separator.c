#include <gtk/gtk.h>

static void
separator_role (void)
{
  GtkWidget *widget = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
  g_object_ref_sink (widget);

  gtk_test_accessible_assert_role (widget, GTK_ACCESSIBLE_ROLE_SEPARATOR);

  g_object_unref (widget);
}

static void
separator_properties (void)
{
  GtkWidget *widget = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
  g_object_ref_sink (widget);

  gtk_test_accessible_assert_property (widget, GTK_ACCESSIBLE_PROPERTY_ORIENTATION, GTK_ORIENTATION_HORIZONTAL);

  gtk_orientable_set_orientation (GTK_ORIENTABLE (widget), GTK_ORIENTATION_VERTICAL);

  gtk_test_accessible_assert_property (widget, GTK_ACCESSIBLE_PROPERTY_ORIENTATION, GTK_ORIENTATION_VERTICAL);

  g_object_unref (widget);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/a11y/separator/role", separator_role);
  g_test_add_func ("/a11y/separator/properties", separator_properties);

  return g_test_run ();
}
