#include <gtk/gtk.h>

static void
progress_bar_role (void)
{
  GtkWidget *widget = gtk_progress_bar_new ();
  g_object_ref_sink (widget);

  gtk_test_accessible_assert_role (widget, GTK_ACCESSIBLE_ROLE_PROGRESS_BAR);

  g_object_unref (widget);
}

static void
progress_bar_state (void)
{
  GtkWidget *widget = gtk_progress_bar_new ();
  g_object_ref_sink (widget);

  gtk_test_accessible_assert_state (widget, GTK_ACCESSIBLE_STATE_BUSY, FALSE);

  gtk_progress_bar_pulse (GTK_PROGRESS_BAR (widget));

  gtk_test_accessible_assert_state (widget, GTK_ACCESSIBLE_STATE_BUSY, TRUE);

  g_object_unref (widget);
}

static void
progress_bar_properties (void)
{
  GtkWidget *widget = gtk_progress_bar_new ();
  g_object_ref_sink (widget);

  gtk_test_accessible_assert_property (widget, GTK_ACCESSIBLE_PROPERTY_VALUE_MAX, 1.);
  gtk_test_accessible_assert_property (widget, GTK_ACCESSIBLE_PROPERTY_VALUE_MIN, 0.);
  gtk_test_accessible_assert_property (widget, GTK_ACCESSIBLE_PROPERTY_VALUE_NOW, 0.);
  gtk_test_accessible_assert_property (widget, GTK_ACCESSIBLE_PROPERTY_VALUE_TEXT, NULL);

  gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (widget), 0.5);

  gtk_test_accessible_assert_property (widget, GTK_ACCESSIBLE_PROPERTY_VALUE_MAX, 1.);
  gtk_test_accessible_assert_property (widget, GTK_ACCESSIBLE_PROPERTY_VALUE_MIN, 0.);
  gtk_test_accessible_assert_property (widget, GTK_ACCESSIBLE_PROPERTY_VALUE_NOW, 0.5);
  gtk_test_accessible_assert_property (widget, GTK_ACCESSIBLE_PROPERTY_VALUE_TEXT, NULL);
  g_assert_false (gtk_test_accessible_has_property (GTK_ACCESSIBLE (widget), GTK_ACCESSIBLE_PROPERTY_VALUE_TEXT));

  g_object_unref (widget);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/a11y/progressbar/role", progress_bar_role);
  g_test_add_func ("/a11y/progressbar/state", progress_bar_state);
  g_test_add_func ("/a11y/progressbar/properties", progress_bar_properties);

  return g_test_run ();
}
