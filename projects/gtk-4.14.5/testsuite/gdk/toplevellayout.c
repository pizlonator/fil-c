#include <stdlib.h>
#include <gtk/gtk.h>

static void
test_toplevel_layout_basic (void)
{
  GdkToplevelLayout *layout;
  GdkToplevelLayout *layout2;
  gboolean max;
  gboolean full;
  GdkMonitor *monitor;

  layout = gdk_toplevel_layout_new ();

  g_assert_false (gdk_toplevel_layout_get_maximized (layout, &max));
  g_assert_false (gdk_toplevel_layout_get_fullscreen (layout, &full));

  gdk_toplevel_layout_set_maximized (layout, TRUE);
  g_assert_true (gdk_toplevel_layout_get_maximized (layout, &max));
  g_assert_true (max);

  layout2 = gdk_toplevel_layout_copy (layout);
  gdk_toplevel_layout_ref (layout2);
  g_assert_true (gdk_toplevel_layout_equal (layout, layout2));
  gdk_toplevel_layout_unref (layout2);

  gdk_toplevel_layout_set_maximized (layout, FALSE);
  g_assert_false (gdk_toplevel_layout_equal (layout, layout2));

  monitor = g_list_model_get_item (gdk_display_get_monitors (gdk_display_get_default ()), 0);
  gdk_toplevel_layout_set_fullscreen (layout, TRUE, monitor);

  g_assert_true (gdk_toplevel_layout_get_fullscreen (layout, &full));
  g_assert_true (full);
  g_assert_true (monitor == gdk_toplevel_layout_get_fullscreen_monitor (layout));

  g_object_unref (monitor);

  gdk_toplevel_layout_unref (layout2);
  gdk_toplevel_layout_unref (layout);
}

int
main (int argc, char *argv[])
{
  (g_test_init) (&argc, &argv, NULL);

  gtk_init ();

  g_test_add_func ("/toplevellayout/basic", test_toplevel_layout_basic);

  return g_test_run ();
}
