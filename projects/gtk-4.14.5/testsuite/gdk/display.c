#include <stdlib.h>

#include <gtk/gtk.h>

static void
test_unset_display_subprocess1 (void)
{
  GdkDisplayManager *manager;

  g_unsetenv ("DISPLAY");

  g_assert_false (gtk_init_check ());
  manager = gdk_display_manager_get ();
  g_assert_nonnull (manager);
  g_assert_null (gdk_display_manager_get_default_display (manager));
}

static void
test_unset_display_subprocess2 (void)
{
  g_unsetenv ("DISPLAY");

  gtk_init ();
}

static void
test_unset_display (void)
{
  g_test_trap_subprocess ("/display/unset-display/subprocess/1", 0, 0);
  g_test_trap_assert_passed ();

  g_test_trap_subprocess ("/display/unset-display/subprocess/2", 0, 0);
  g_test_trap_assert_failed ();
  g_test_trap_assert_stderr ("*Failed to open display*");
}

static void
test_bad_display_subprocess1 (void)
{
  GdkDisplayManager *manager;

  g_setenv ("DISPLAY", "poo", TRUE);

  g_assert_false (gtk_init_check ());
  manager = gdk_display_manager_get ();
  g_assert_nonnull (manager);
  g_assert_null (gdk_display_manager_get_default_display (manager));
}

static void
test_bad_display_subprocess2 (void)
{
  g_setenv ("DISPLAY", "poo", TRUE);
  gtk_init ();
}

static void
test_bad_display (void)
{
  g_test_trap_subprocess ("/display/bad-display/subprocess/1", 0, 0);
  g_test_trap_assert_passed ();

  g_test_trap_subprocess ("/display/bad-display/subprocess/2", 0, 0);
  g_test_trap_assert_failed ();
  g_test_trap_assert_stderr ("*Failed to open display*");
}

static void
test_debug_help (void)
{
  if (g_test_subprocess ())
    {
      g_setenv ("GDK_DEBUG", "help", TRUE);

      gtk_init_check ();
      return;
    }

  g_test_trap_subprocess (NULL, 0, G_TEST_SUBPROCESS_DEFAULT);
  g_test_trap_assert_passed ();
  g_test_trap_assert_stderr ("*Supported GDK_DEBUG values:*");
  g_test_trap_assert_stderr ("*Multiple values can be given, separated by : or space.*");
}

int
main (int argc, char *argv[])
{
  (g_test_init) (&argc, &argv, NULL);

  gdk_set_allowed_backends ("x11");

  g_test_add_func ("/display/unset-display", test_unset_display);
  g_test_add_func ("/display/unset-display/subprocess/1", test_unset_display_subprocess1);
  g_test_add_func ("/display/unset-display/subprocess/2", test_unset_display_subprocess2);
  g_test_add_func ("/display/bad-display", test_bad_display);
  g_test_add_func ("/display/bad-display/subprocess/1", test_bad_display_subprocess1);
  g_test_add_func ("/display/bad-display/subprocess/2", test_bad_display_subprocess2);
  g_test_add_func ("/debug/help", test_debug_help);

  return g_test_run ();
}
