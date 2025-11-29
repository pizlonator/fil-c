#include <gtk/gtk.h>

static void
box_role (void)
{
  GtkWidget *widget = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  g_object_ref_sink (widget);

  gtk_test_accessible_assert_role (widget, GTK_ACCESSIBLE_ROLE_GENERIC);

  g_object_unref (widget);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/a11y/box/role", box_role);

  return g_test_run ();
}
