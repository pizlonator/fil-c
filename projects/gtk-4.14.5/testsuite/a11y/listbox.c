#include <gtk/gtk.h>

static void
listbox_role (void)
{
  GtkWidget *widget = gtk_list_box_new ();

  g_object_ref_sink (widget);

  gtk_list_box_append (GTK_LIST_BOX (widget), gtk_label_new ("a"));

  gtk_test_accessible_assert_role (GTK_ACCESSIBLE (widget), GTK_ACCESSIBLE_ROLE_LIST);
  gtk_test_accessible_assert_role (GTK_ACCESSIBLE (gtk_widget_get_first_child (widget)), GTK_ACCESSIBLE_ROLE_LIST_ITEM);

  g_object_unref (widget);
}

static void
listbox_state (void)
{
  GtkWidget *window = gtk_window_new ();
  GtkWidget *widget = gtk_list_box_new ();

  gtk_window_set_child (GTK_WINDOW (window), widget);

  gtk_list_box_append (GTK_LIST_BOX (widget), gtk_label_new ("a"));

  gtk_test_accessible_assert_state (GTK_ACCESSIBLE (gtk_widget_get_first_child (widget)), GTK_ACCESSIBLE_STATE_SELECTED, FALSE);

  gtk_list_box_select_row (GTK_LIST_BOX (widget),
                           GTK_LIST_BOX_ROW (gtk_widget_get_first_child (widget)));

  gtk_test_accessible_assert_state (GTK_ACCESSIBLE (gtk_widget_get_first_child (widget)), GTK_ACCESSIBLE_STATE_SELECTED, TRUE);

  gtk_window_destroy (GTK_WINDOW (window));
}

static void
listbox_properties (void)
{
  GtkWidget *widget = gtk_list_box_new ();

  g_object_ref_sink (widget);

  gtk_list_box_append (GTK_LIST_BOX (widget), gtk_label_new ("a"));

  gtk_test_accessible_assert_property (GTK_ACCESSIBLE (widget), GTK_ACCESSIBLE_PROPERTY_MULTI_SELECTABLE, FALSE);

  gtk_list_box_set_selection_mode (GTK_LIST_BOX (widget), GTK_SELECTION_MULTIPLE);

  gtk_test_accessible_assert_property (GTK_ACCESSIBLE (widget), GTK_ACCESSIBLE_PROPERTY_MULTI_SELECTABLE, TRUE);

  g_object_unref (widget);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/a11y/listbox/role", listbox_role);
  g_test_add_func ("/a11y/listbox/state", listbox_state);
  g_test_add_func ("/a11y/listbox/properties", listbox_properties);

  return g_test_run ();
}
