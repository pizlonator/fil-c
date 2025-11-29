#include <gtk/gtk.h>

/* Some of the accessible attributes are general enough
 * that GTK maintains them for every widget. These tests
 * are checking them.
 */

static void
test_hidden (void)
{
  GtkWidget *window;
  GtkWidget *widget;

  window = gtk_window_new ();
  widget = gtk_button_new ();
  gtk_window_set_child (GTK_WINDOW (window), widget);

  gtk_test_accessible_assert_state (widget, GTK_ACCESSIBLE_STATE_HIDDEN, TRUE);

  gtk_window_present (GTK_WINDOW (window));

  gtk_test_accessible_assert_state (widget, GTK_ACCESSIBLE_STATE_HIDDEN, FALSE);

  gtk_window_destroy (GTK_WINDOW (window));
}

static void
test_disabled (void)
{
  GtkWidget *widget;

  widget = gtk_expander_new ("");
  g_object_ref_sink (widget);

  gtk_test_accessible_assert_state (widget, GTK_ACCESSIBLE_STATE_DISABLED, FALSE);

  gtk_widget_set_sensitive (widget, FALSE);

  gtk_test_accessible_assert_state (widget, GTK_ACCESSIBLE_STATE_DISABLED, TRUE);

  g_object_unref (widget);
}

static void
test_orientation (void)
{
  GtkWidget *widget;

  widget = gtk_scale_new (GTK_ORIENTATION_HORIZONTAL, NULL);
  g_object_ref_sink (widget);

  gtk_test_accessible_assert_property (widget, GTK_ACCESSIBLE_PROPERTY_ORIENTATION, GTK_ORIENTATION_HORIZONTAL);

  gtk_orientable_set_orientation (GTK_ORIENTABLE (widget), GTK_ORIENTATION_VERTICAL);

  gtk_test_accessible_assert_property (widget, GTK_ACCESSIBLE_PROPERTY_ORIENTATION, GTK_ORIENTATION_VERTICAL);

  g_object_unref (widget);
}

static void
test_labelled_by (void)
{
  GtkWidget *widget;
  GtkWidget *label;

  widget = gtk_switch_new ();
  g_object_ref_sink (widget);

  gtk_test_accessible_assert_relation (widget, GTK_ACCESSIBLE_RELATION_LABELLED_BY, NULL);

  label = gtk_label_new ("Switch");
  g_object_ref_sink (label);
  gtk_widget_add_mnemonic_label (widget, label);

  gtk_test_accessible_assert_relation (widget, GTK_ACCESSIBLE_RELATION_LABELLED_BY, label, NULL);

  g_object_unref (widget);
  g_object_unref (label);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/a11y/general/hidden", test_hidden);
  g_test_add_func ("/a11y/general/disabled", test_disabled);
  g_test_add_func ("/a11y/general/orientation", test_orientation);
  g_test_add_func ("/a11y/general/labelled-by", test_labelled_by);

  return g_test_run ();
}
