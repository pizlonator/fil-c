#include <gtk/gtk.h>

static void
entry_role (void)
{
  GtkWidget *widget = gtk_entry_new ();
  g_object_ref_sink (widget);

  gtk_test_accessible_assert_role (widget, GTK_ACCESSIBLE_ROLE_TEXT_BOX);

  g_object_unref (widget);
}

static void
entry_properties (void)
{
  GtkWidget *widget = gtk_entry_new ();
  g_object_ref_sink (widget);

  gtk_test_accessible_assert_property (widget, GTK_ACCESSIBLE_PROPERTY_PLACEHOLDER, NULL);
  gtk_test_accessible_assert_property (widget, GTK_ACCESSIBLE_PROPERTY_READ_ONLY, FALSE);

  gtk_entry_set_placeholder_text (GTK_ENTRY (widget), "Hello");
  gtk_editable_set_editable (GTK_EDITABLE (widget), FALSE);

  gtk_test_accessible_assert_property (widget, GTK_ACCESSIBLE_PROPERTY_PLACEHOLDER, "Hello");
  gtk_test_accessible_assert_property (widget, GTK_ACCESSIBLE_PROPERTY_READ_ONLY, TRUE);
  gtk_test_accessible_assert_property (widget, GTK_ACCESSIBLE_PROPERTY_HAS_POPUP, TRUE);

  g_object_unref (widget);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/a11y/entry/role", entry_role);
  g_test_add_func ("/a11y/entry/properties", entry_properties);

  return g_test_run ();
}
