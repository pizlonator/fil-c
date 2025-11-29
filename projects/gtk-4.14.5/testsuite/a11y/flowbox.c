#include <gtk/gtk.h>

static void
flowbox_role (void)
{
  GtkWidget *widget = gtk_flow_box_new ();

  g_object_ref_sink (widget);

  gtk_flow_box_insert (GTK_FLOW_BOX (widget), gtk_label_new ("a"), 0);

  gtk_test_accessible_assert_role (GTK_ACCESSIBLE (widget), GTK_ACCESSIBLE_ROLE_GRID);
  gtk_test_accessible_assert_role (GTK_ACCESSIBLE (gtk_widget_get_first_child (widget)), GTK_ACCESSIBLE_ROLE_GRID_CELL);

  g_object_unref (widget);
}

static void
flowbox_state (void)
{
  GtkWidget *window = gtk_window_new ();
  GtkWidget *widget = gtk_flow_box_new ();

  gtk_window_set_child (GTK_WINDOW (window), widget);

  gtk_flow_box_insert (GTK_FLOW_BOX (widget), gtk_label_new ("a"), 0);

  gtk_test_accessible_assert_state (GTK_ACCESSIBLE (gtk_widget_get_first_child (widget)), GTK_ACCESSIBLE_STATE_SELECTED, FALSE);

  gtk_flow_box_select_child (GTK_FLOW_BOX (widget),
                             GTK_FLOW_BOX_CHILD (gtk_widget_get_first_child (widget)));

  gtk_test_accessible_assert_state (GTK_ACCESSIBLE (gtk_widget_get_first_child (widget)), GTK_ACCESSIBLE_STATE_SELECTED, TRUE);

  gtk_window_destroy (GTK_WINDOW (window));
}

static void
flowbox_properties (void)
{
  GtkWidget *widget = gtk_flow_box_new ();

  g_object_ref_sink (widget);

  gtk_flow_box_insert (GTK_FLOW_BOX (widget), gtk_label_new ("a"), 0);

  gtk_test_accessible_assert_property (GTK_ACCESSIBLE (widget), GTK_ACCESSIBLE_PROPERTY_MULTI_SELECTABLE, FALSE);

  gtk_flow_box_set_selection_mode (GTK_FLOW_BOX (widget), GTK_SELECTION_MULTIPLE);

  gtk_test_accessible_assert_property (GTK_ACCESSIBLE (widget), GTK_ACCESSIBLE_PROPERTY_MULTI_SELECTABLE, TRUE);

  g_object_unref (widget);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/a11y/flowbox/role", flowbox_role);
  g_test_add_func ("/a11y/flowbox/state", flowbox_state);
  g_test_add_func ("/a11y/flowbox/properties", flowbox_properties);

  return g_test_run ();
}
