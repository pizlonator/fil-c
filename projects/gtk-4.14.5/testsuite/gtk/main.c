#include <gtk/gtk.h>
#include <locale.h>

static void
test_init (void)
{
  gboolean ret;

  g_assert_false (gtk_is_initialized ());
  ret = gtk_init_check ();
  g_assert_true (ret);
  g_assert_true (gtk_is_initialized ());
}

static void
test_version (void)
{
  g_assert_cmpuint (gtk_get_major_version (), ==, GTK_MAJOR_VERSION);
  g_assert_cmpuint (gtk_get_minor_version (), ==, GTK_MINOR_VERSION);
  g_assert_cmpuint (gtk_get_micro_version (), ==, GTK_MICRO_VERSION);
  g_assert_cmpuint (gtk_get_binary_age (), ==, GTK_BINARY_AGE);
  g_assert_cmpuint (gtk_get_interface_age (), ==, GTK_INTERFACE_AGE);

 g_assert_null (gtk_check_version (GTK_MAJOR_VERSION, GTK_MINOR_VERSION, GTK_MICRO_VERSION));
 g_assert_nonnull (gtk_check_version (5, 0, 0));
 g_assert_nonnull (gtk_check_version (1, 0, 0));
 g_assert_nonnull (gtk_check_version (3, 1000, 10));
}

int
main (int argc, char *argv[])
{
  /* Don't use gtk_test_init here because it implicitly initializes GTK. */
  (g_test_init) (&argc, &argv, NULL);
  gtk_disable_setlocale();
  setlocale (LC_ALL, "C");

  g_test_add_func ("/main/init", test_init);
  g_test_add_func ("/main/version", test_version);

  return g_test_run ();
}
