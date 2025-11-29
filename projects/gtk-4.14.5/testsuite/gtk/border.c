#include <gtk/gtk.h>

static void
test_border_basic (void)
{
  GtkBorder *border;
  GtkBorder *border2;

  border = gtk_border_new ();
  *border = (GtkBorder) { 5, 6, 666, 777 };
  border2 = gtk_border_copy (border);

  g_assert_true (memcmp (border, border2, sizeof (GtkBorder)) == 0);

  gtk_border_free (border);
  gtk_border_free (border2);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv);

  g_test_add_func ("/border/basic", test_border_basic);

  return g_test_run ();
}
