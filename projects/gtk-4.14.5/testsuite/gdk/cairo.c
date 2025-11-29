#include <gtk/gtk.h>

static void
test_set_source_big_pixbuf (void)
{
  cairo_surface_t *surface;
  GdkPixbuf *pixbuf;
  cairo_t *cr;

#define WAY_TOO_BIG 65540

  /* Check that too big really is to big.
   * If this check fails, somebody improved Cairo and this test is useless.
   */
  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, WAY_TOO_BIG, 1);
  g_assert_cmpint (cairo_surface_status (surface), !=, CAIRO_STATUS_SUCCESS);
  cairo_surface_destroy (surface);

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 10, 10);
  cr = cairo_create (surface);
  pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, WAY_TOO_BIG, 1);

  gdk_cairo_set_source_pixbuf (cr, pixbuf, 0, 0);
  g_assert_cmpint (cairo_status (cr), !=, CAIRO_STATUS_SUCCESS);

  g_object_unref (pixbuf);
  cairo_destroy (cr);
  cairo_surface_destroy (surface);
}

int
main (int argc, char *argv[])
{
  (g_test_init) (&argc, &argv, NULL);
  gtk_init ();

  g_test_add_func ("/drawing/set-source-big-pixbuf", test_set_source_big_pixbuf);

  return g_test_run ();
}
