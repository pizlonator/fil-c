#include <gtk/gtk.h>

static void
search_entry_role (void)
{
  GtkWidget *widget = gtk_search_entry_new ();
  g_object_ref_sink (widget);

  gtk_test_accessible_assert_role (widget, GTK_ACCESSIBLE_ROLE_SEARCH_BOX);

  g_object_unref (widget);
}

static void
search_entry_properties (void)
{
  GtkWidget *widget = gtk_search_entry_new ();
  g_object_ref_sink (widget);

  gtk_test_accessible_assert_property (widget, GTK_ACCESSIBLE_PROPERTY_PLACEHOLDER, NULL);
  gtk_test_accessible_assert_property (widget, GTK_ACCESSIBLE_PROPERTY_READ_ONLY, FALSE);

  g_object_set (widget, "placeholder-text", "Hello", NULL);
  gtk_editable_set_editable (GTK_EDITABLE (widget), FALSE);

  gtk_test_accessible_assert_property (widget, GTK_ACCESSIBLE_PROPERTY_PLACEHOLDER, "Hello");
  gtk_test_accessible_assert_property (widget, GTK_ACCESSIBLE_PROPERTY_READ_ONLY, TRUE);

  g_object_unref (widget);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/a11y/searchentry/role", search_entry_role);
  g_test_add_func ("/a11y/searchentry/properties", search_entry_properties);

  return g_test_run ();
}
