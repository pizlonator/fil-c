#include <gtk/gtk.h>

static void
image_role (void)
{
  GtkWidget *widget = gtk_image_new ();
  g_object_ref_sink (widget);

  gtk_test_accessible_assert_role (widget, GTK_ACCESSIBLE_ROLE_IMG);

  g_object_unref (widget);
}

static void
picture_role (void)
{
  GtkWidget *widget = gtk_picture_new ();
  g_object_ref_sink (widget);

  gtk_test_accessible_assert_role (widget, GTK_ACCESSIBLE_ROLE_IMG);

  g_object_unref (widget);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/a11y/image/role", image_role);
  g_test_add_func ("/a11y/picture/role", picture_role);

  return g_test_run ();
}
