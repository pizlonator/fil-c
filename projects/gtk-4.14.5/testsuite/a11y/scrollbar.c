#include <gtk/gtk.h>

static void
scrollbar_role (void)
{
  GtkWidget *widget = gtk_scrollbar_new (GTK_ORIENTATION_HORIZONTAL, NULL);
  g_object_ref_sink (widget);

  gtk_test_accessible_assert_role (widget, GTK_ACCESSIBLE_ROLE_SCROLLBAR);

  g_object_unref (widget);
}

static void
scrollbar_state (void)
{
  GtkWidget *widget = gtk_scrollbar_new (GTK_ORIENTATION_HORIZONTAL, NULL);
  g_object_ref_sink (widget);

  gtk_test_accessible_assert_state (widget, GTK_ACCESSIBLE_STATE_DISABLED, FALSE);

  gtk_widget_set_sensitive (widget, FALSE);

  gtk_test_accessible_assert_state (widget, GTK_ACCESSIBLE_STATE_DISABLED, TRUE);

  g_object_unref (widget);
}

static void
scrollbar_properties (void)
{
  GtkAdjustment *adj = gtk_adjustment_new (0.0, 0.0, 100.0, 1.0, 10.0, 10.0);
  GtkWidget *widget = gtk_scrollbar_new (GTK_ORIENTATION_HORIZONTAL, adj);
  g_object_ref_sink (widget);

  gtk_test_accessible_assert_property (widget, GTK_ACCESSIBLE_PROPERTY_ORIENTATION, GTK_ORIENTATION_HORIZONTAL);

  gtk_test_accessible_assert_property (widget, GTK_ACCESSIBLE_PROPERTY_VALUE_MAX, 100.);
  gtk_test_accessible_assert_property (widget, GTK_ACCESSIBLE_PROPERTY_VALUE_MIN, 0.);
  gtk_test_accessible_assert_property (widget, GTK_ACCESSIBLE_PROPERTY_VALUE_NOW, 0.);

  gtk_orientable_set_orientation (GTK_ORIENTABLE (widget), GTK_ORIENTATION_VERTICAL);
  gtk_adjustment_set_value (adj, 50.0);

  gtk_test_accessible_assert_property (widget, GTK_ACCESSIBLE_PROPERTY_ORIENTATION, GTK_ORIENTATION_VERTICAL);
  gtk_test_accessible_assert_property (widget, GTK_ACCESSIBLE_PROPERTY_VALUE_MAX, 100.);
  gtk_test_accessible_assert_property (widget, GTK_ACCESSIBLE_PROPERTY_VALUE_MIN, 0.);
  gtk_test_accessible_assert_property (widget, GTK_ACCESSIBLE_PROPERTY_VALUE_NOW, 50.0);

  g_object_unref (widget);
}

static void
scrollbar_relations (void)
{
  GtkWidget *sw = gtk_scrolled_window_new ();
  GtkWidget *hscrollbar;
  GtkWidget *vscrollbar;
  GtkWidget *child;

  g_object_ref_sink (sw);

  hscrollbar = gtk_scrolled_window_get_hscrollbar (GTK_SCROLLED_WINDOW (sw));
  vscrollbar = gtk_scrolled_window_get_vscrollbar (GTK_SCROLLED_WINDOW (sw));

  gtk_test_accessible_assert_relation (hscrollbar, GTK_ACCESSIBLE_RELATION_CONTROLS, NULL);
  gtk_test_accessible_assert_relation (vscrollbar, GTK_ACCESSIBLE_RELATION_CONTROLS, NULL);

  child = gtk_text_view_new ();
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), child);

  gtk_test_accessible_assert_relation (hscrollbar, GTK_ACCESSIBLE_RELATION_CONTROLS, child, NULL);
  gtk_test_accessible_assert_relation (vscrollbar, GTK_ACCESSIBLE_RELATION_CONTROLS, child, NULL);

  g_object_unref (sw);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/a11y/scrollbar/role", scrollbar_role);
  g_test_add_func ("/a11y/scrollbar/state", scrollbar_state);
  g_test_add_func ("/a11y/scrollbar/properties", scrollbar_properties);
  g_test_add_func ("/a11y/scrollbar/relations", scrollbar_relations);

  return g_test_run ();
}
