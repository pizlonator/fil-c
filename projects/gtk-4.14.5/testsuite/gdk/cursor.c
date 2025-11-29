#include <gtk/gtk.h>
#include <cairo-gobject.h>

static void
test_cursor_named (void)
{
  GdkCursor *cursor;

  cursor = gdk_cursor_new_from_name ("default", NULL);

  g_assert_cmpstr (gdk_cursor_get_name (cursor), ==, "default");
  g_assert_null (gdk_cursor_get_fallback (cursor));
  g_assert_cmpint (gdk_cursor_get_hotspot_x (cursor), ==, 0);
  g_assert_cmpint (gdk_cursor_get_hotspot_y (cursor), ==, 0);
  g_assert_null (gdk_cursor_get_texture (cursor));

  g_object_unref (cursor);
}

static void
test_cursor_texture (void)
{
  GdkPixbuf *pixbuf;
  GdkTexture *texture;
  GdkCursor *cursor;

  pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, 32, 32);
  texture = gdk_texture_new_for_pixbuf (pixbuf);
  cursor = gdk_cursor_new_from_texture (texture, 1, 2, NULL);

  g_assert_null (gdk_cursor_get_name (cursor));
  g_assert_null (gdk_cursor_get_fallback (cursor));
  g_assert_cmpint (gdk_cursor_get_hotspot_x (cursor), ==, 1);
  g_assert_cmpint (gdk_cursor_get_hotspot_y (cursor), ==, 2);
  g_assert_true (gdk_cursor_get_texture (cursor) == texture);

  g_object_unref (cursor);
  g_object_unref (texture);
  g_object_unref (pixbuf);
}

static void
test_cursor_fallback (void)
{
  GdkCursor *fallback;
  GdkCursor *cursor;

  fallback = gdk_cursor_new_from_name ("default", NULL);
  cursor = gdk_cursor_new_from_name ("text", fallback);

  g_assert_true (gdk_cursor_get_fallback (cursor) == fallback);

  g_object_unref (cursor);
  g_object_unref (fallback);
}

int
main (int argc, char *argv[])
{
  (g_test_init) (&argc, &argv, NULL);

  gtk_init ();

  g_test_add_func ("/cursor/named", test_cursor_named);
  g_test_add_func ("/cursor/texture", test_cursor_texture);
  g_test_add_func ("/cursor/fallback", test_cursor_fallback);

  return g_test_run ();
}
