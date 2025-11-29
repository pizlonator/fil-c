#include <gtk/gtk.h>

static void
value_set_unset (void)
{
  GtkAdjustment *adjustment = gtk_adjustment_new (0, 0, 100, 1, 10, 10);
  GtkWidget *scrollbar = gtk_scrollbar_new (GTK_ORIENTATION_HORIZONTAL, adjustment);

  g_object_ref_sink (scrollbar);

  gtk_test_accessible_assert_property (scrollbar, GTK_ACCESSIBLE_PROPERTY_VALUE_NOW, 0.);

  gtk_adjustment_set_value (adjustment, 10);

  gtk_test_accessible_assert_property (scrollbar, GTK_ACCESSIBLE_PROPERTY_VALUE_NOW, 10.);

  gtk_adjustment_set_value (adjustment, 0);

  gtk_test_accessible_assert_property (scrollbar, GTK_ACCESSIBLE_PROPERTY_VALUE_NOW, 0.);

  g_object_unref (scrollbar);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/a11y/value/set-unset", value_set_unset);

  return g_test_run ();
}
