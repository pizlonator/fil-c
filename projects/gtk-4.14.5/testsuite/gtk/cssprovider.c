#include <gtk/gtk.h>

static void
assert_section_is_not_null (GtkCssProvider *provider,
                            GtkCssSection  *section,
                            const GError   *error,
                            gpointer        unused)
{
  g_assert_nonnull (section);
}

static void
test_section_in_load_from_data (void)
{
  GtkCssProvider *provider;

  provider = gtk_css_provider_new ();
  g_signal_connect (provider, "parsing-error",
                    G_CALLBACK (assert_section_is_not_null), NULL);
  gtk_css_provider_load_from_string (provider, "random garbage goes here");
  g_object_unref (provider);
}

static void
test_section_load_nonexisting_file (void)
{
  GtkCssProvider *provider;

  provider = gtk_css_provider_new ();
  g_signal_connect (provider, "parsing-error",
                    G_CALLBACK (assert_section_is_not_null), NULL);
  gtk_css_provider_load_from_path (provider, "this/path/does/absolutely/not/exist.css");
  g_object_unref (provider);
}

int
main (int argc, char *argv[])
{
  gtk_init ();
  (g_test_init) (&argc, &argv, NULL);

  g_test_add_func ("/cssprovider/section-in-load-from-data", test_section_in_load_from_data);
  g_test_add_func ("/cssprovider/load-nonexisting-file", test_section_load_nonexisting_file);

  return g_test_run ();
}
