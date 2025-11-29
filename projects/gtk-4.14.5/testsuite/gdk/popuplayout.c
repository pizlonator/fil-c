#include <stdlib.h>
#include <gtk/gtk.h>

static void
test_popup_layout_basic (void)
{
  GdkPopupLayout *layout;
  GdkPopupLayout *layout2;
  GdkRectangle anchor = { 0, 0, 20, 20 };
  const GdkRectangle *rect;
  int dx, dy;
  int l, r, t, b;

  layout = gdk_popup_layout_new (&anchor, GDK_GRAVITY_SOUTH, GDK_GRAVITY_NORTH);

  rect = gdk_popup_layout_get_anchor_rect (layout);
  g_assert_true (gdk_rectangle_equal (&anchor, rect));

  layout2 = gdk_popup_layout_copy (layout);

  gdk_popup_layout_ref (layout2);
  g_assert_true (gdk_popup_layout_equal (layout, layout2));
  gdk_popup_layout_unref (layout2);

  gdk_popup_layout_set_offset (layout, 10, 10);
  g_assert_false (gdk_popup_layout_equal (layout, layout2));
  gdk_popup_layout_get_offset (layout, &dx, &dy);
  g_assert_true (dx == 10 && dy == 10);

  gdk_popup_layout_set_shadow_width (layout, 1, 2, 3, 4);
  gdk_popup_layout_get_shadow_width (layout, &l, &r, &t, &b);
  g_assert_true (l == 1 && r == 2 && t == 3 && b == 4);

  anchor.x = 1;
  anchor.y = 2;
  gdk_popup_layout_set_anchor_rect (layout, &anchor);
  rect = gdk_popup_layout_get_anchor_rect (layout);
  g_assert_true (gdk_rectangle_equal (&anchor, rect));

  gdk_popup_layout_set_rect_anchor (layout, GDK_GRAVITY_NORTH_WEST);
  g_assert_true (gdk_popup_layout_get_rect_anchor (layout) == GDK_GRAVITY_NORTH_WEST);

  gdk_popup_layout_set_surface_anchor (layout, GDK_GRAVITY_SOUTH_EAST);
  g_assert_true (gdk_popup_layout_get_surface_anchor (layout) == GDK_GRAVITY_SOUTH_EAST);

  gdk_popup_layout_set_anchor_hints (layout, GDK_ANCHOR_FLIP_X | GDK_ANCHOR_RESIZE_Y);
  g_assert_true (gdk_popup_layout_get_anchor_hints (layout) == (GDK_ANCHOR_FLIP_X | GDK_ANCHOR_RESIZE_Y));

  gdk_popup_layout_unref (layout2);
  gdk_popup_layout_unref (layout);
}

int
main (int argc, char *argv[])
{
  (g_test_init) (&argc, &argv, NULL);

  gtk_init ();

  g_test_add_func ("/popuplayout/basic", test_popup_layout_basic);

  return g_test_run ();
}
