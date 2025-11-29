#include <gtk/gtk.h>

static void
test_basic (void)
{
  GdkDisplayManager *manager;
  GdkDisplay *d, *d2;
  GSList *list;

  manager = gdk_display_manager_get ();
  g_assert_nonnull (manager);

  d = gdk_display_manager_get_default_display (manager);
  g_assert_nonnull (d);
  g_object_get (manager, "default-display", &d2, NULL);
  g_assert_true (d == d2);
  g_object_unref (d2);

  list = gdk_display_manager_list_displays (manager);
  g_assert_nonnull (g_slist_find (list, d));
  g_slist_free (list);
}

static void
test_set_default (void)
{
  GdkDisplayManager *manager;
  GdkDisplay *d, *d2;
  const char *name;

  manager = gdk_display_manager_get ();
  g_assert_nonnull (manager);

  d = gdk_display_manager_get_default_display (manager);
  name = gdk_display_get_name (d);
  d2 = gdk_display_manager_open_display (manager, name);
  g_object_set (manager, "default-display", d2, NULL);

  d = gdk_display_manager_get_default_display (manager);
  g_assert_true (d == d2);
}

static void
test_display_basic (void)
{
  GdkDisplay *d = gdk_display_get_default ();

  g_assert_false (gdk_display_is_closed (d));
}

int
main (int argc, char *argv[])
{
  (g_test_init) (&argc, &argv, NULL);

  /* Open default display */
  gdk_display_open (NULL);

  g_test_add_func ("/display/basic", test_display_basic);
  g_test_add_func ("/displaymanager/basic", test_basic);
  g_test_add_func ("/displaymanager/set-default", test_set_default);

  return g_test_run ();
}
