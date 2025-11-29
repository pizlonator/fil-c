#include <gtk.h>

#include "gdk/gdkmemorytextureprivate.h"
#include "gdk/gdktextureprivate.h"


#define assert_texture_diff_equal(a, b, expected) G_STMT_START { \
  cairo_region_t *_r; \
\
  _r = cairo_region_create (); \
  gdk_texture_diff (a, b, _r); \
  g_assert_true (cairo_region_equal (_r, expected)); \
  cairo_region_destroy(_r); \
\
  _r = cairo_region_create (); \
  gdk_texture_diff (b, a, _r); \
  g_assert_true (cairo_region_equal (_r, expected)); \
  cairo_region_destroy(_r); \
}G_STMT_END

static void
compare_pixels (int     width,
                int     height,
                guchar *data1,
                gsize   stride1,
                guchar *data2,
                gsize   stride2)
{
  int x, y;
  for (y = 0; y < height; y++)
    {
      const guint32 *p1 = (const guint32*) (data1 + y * stride1);
      const guint32 *p2 = (const guint32*) (data2 + y * stride2);

      for (x = 0; x < width; x++)
        {
          g_assert_cmphex (p1[x], ==, p2[x]);
        }
    }
}

static GdkTexture *
red_texture_new (int width,
                 int height)
{
  /* We use GDK_MEMORY_R8G8B8A8 here because it's the expected PNG RGBA format */
  const GdkMemoryFormat format = GDK_MEMORY_R8G8B8A8;
  int i;
  guint32 *data;
  GBytes *bytes;
  GdkTexture *texture;

  data = g_malloc (width * height * 4);

  for (i = 0; i < width * height; i++)
    data[i] = 0xFF0000FF;

  bytes = g_bytes_new_take ((guint8 *) data, width * height * 4);
  texture = gdk_memory_texture_new (width, height, format, bytes, width * 4);
  g_bytes_unref (bytes);

  return texture;
}

static void
compare_textures (GdkTexture *texture1,
                  GdkTexture *texture2)
{
  int width, height;
  gsize stride;
  guchar *data1;
  guchar *data2;

  g_assert_true (gdk_texture_get_width (texture1) == gdk_texture_get_width (texture2));
  g_assert_true (gdk_texture_get_height (texture1) == gdk_texture_get_height (texture2));

  width = gdk_texture_get_width (texture1);
  height = gdk_texture_get_height (texture1);
  stride = 4 * width;

  data1 = g_new0 (guchar, stride * height);
  gdk_texture_download (texture1, data1, stride);

  data2 = g_new0 (guchar, stride * height);
  gdk_texture_download (texture2, data2, stride);

  compare_pixels (width, height,
                  data1, stride,
                  data2, stride);

  g_free (data1);
  g_free (data2);
}

static void
test_texture_from_pixbuf (void)
{
  GdkPixbuf *pixbuf;
  GdkTexture *texture;
  GError *error = NULL;
  guchar *data;
  int width, height;
  gsize stride;
  cairo_surface_t *surface;
  cairo_t *cr;

  pixbuf = gdk_pixbuf_new_from_resource ("/org/gtk/libgtk/icons/16x16/places/user-trash.png", &error);
  g_assert_no_error (error);
  g_assert_nonnull (pixbuf);
  g_assert_true (gdk_pixbuf_get_has_alpha (pixbuf));

  width = gdk_pixbuf_get_width (pixbuf);
  height = gdk_pixbuf_get_height (pixbuf);

  texture = gdk_texture_new_for_pixbuf (pixbuf);

  g_assert_nonnull (texture);
  g_assert_cmpint (gdk_texture_get_width (texture), ==, width);
  g_assert_cmpint (gdk_texture_get_height (texture), ==, height);

  stride = 4 * width;
  data = g_new0 (guchar, stride * height);
  gdk_texture_download (texture, data, stride);

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
  cr = cairo_create (surface);
  gdk_cairo_set_source_pixbuf (cr, pixbuf, 0, 0);
  cairo_paint (cr);
  cairo_destroy (cr);

  compare_pixels (width, height,
                  data, stride,
                  cairo_image_surface_get_data (surface),
                  cairo_image_surface_get_stride (surface));

  g_free (data);

  g_object_unref (pixbuf);
  g_object_unref (texture);
  cairo_surface_destroy (surface);
}

static void
test_texture_from_resource (void)
{
  GdkTexture *texture;
  int width, height;

  texture = gdk_texture_new_from_resource ("/org/gtk/libgtk/icons/16x16/places/user-trash.png");

  g_assert_nonnull (texture);
  g_object_get (texture,
                "width", &width,
                "height", &height,
                NULL);
  g_assert_cmpint (width, ==, 16);
  g_assert_cmpint (height, ==, 16);

  g_object_unref (texture);
}

static void
test_texture_save_to_png (void)
{
  GdkTexture *texture;
  GError *error = NULL;
  GFile *file;
  GdkTexture *texture2;

  texture = gdk_texture_new_from_resource ("/org/gtk/libgtk/icons/16x16/places/user-trash.png");

  g_assert_true (gdk_texture_save_to_png (texture, "test.png"));
  file = g_file_new_for_path ("test.png");
  texture2 = gdk_texture_new_from_file (file, &error);
  g_object_unref (file);
  g_assert_no_error (error);

  compare_textures (texture, texture2);

  g_object_unref (texture);
  g_object_unref (texture2);

  /* libpng has a builtin user limit of 1M pixels per dimension, so we make it 1px too big. */
  texture = red_texture_new (1 * 1000 * 1000 + 1, 1);
  g_assert_true (gdk_texture_save_to_png (texture, "test.png"));
}

static void
test_texture_save_to_tiff (void)
{
  GdkTexture *texture;
  GError *error = NULL;
  GFile *file;
  GdkTexture *texture2;

  texture = gdk_texture_new_from_resource ("/org/gtk/libgtk/icons/16x16/places/user-trash.png");

  gdk_texture_save_to_tiff (texture, "test.tiff");
  file = g_file_new_for_path ("test.tiff");
  texture2 = gdk_texture_new_from_file (file, &error);
  g_object_unref (file);
  g_assert_no_error (error);

  compare_textures (texture, texture2);

  g_object_unref (texture);
  g_object_unref (texture2);
}

static void
test_texture_subtexture (void)
{
  cairo_t *cr;
  cairo_surface_t *surface;
  cairo_pattern_t *pattern;
  GdkTexture *texture, *subtexture;
  guchar *data, *subdata;
  gsize stride, substride;

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 64, 64);
  cr = cairo_create (surface);

  pattern = cairo_pattern_create_linear (0, 0, 64, 64);
  cairo_pattern_add_color_stop_rgba (pattern, 0, 1, 0, 0, 0.2);
  cairo_pattern_add_color_stop_rgba (pattern, 1, 1, 0, 1, 1);

  cairo_set_source (cr, pattern);
  cairo_paint (cr);

  texture = gdk_texture_new_for_surface (surface);

  cairo_pattern_destroy (pattern);
  cairo_destroy (cr);
  cairo_surface_destroy (surface);

  subtexture = gdk_memory_texture_new_subtexture (GDK_MEMORY_TEXTURE (texture), 0, 0, 32, 32);

  g_assert_cmpint (gdk_texture_get_width (subtexture), ==, 32);
  g_assert_cmpint (gdk_texture_get_height (subtexture), ==, 32);

  data = g_new0 (guchar, 64 * 64 * 4);
  stride = 64 * 4;
  gdk_texture_download (texture, data, stride);

  subdata = g_new0 (guchar, 32 * 32 * 4);
  substride = 32 * 4;
  gdk_texture_download (subtexture, subdata, substride);

  compare_pixels (32, 32,
                  data, stride,
                  subdata, substride);

  g_free (data);
  g_free (subdata);

  g_object_unref (subtexture);
  g_object_unref (texture);
}

static void
test_texture_icon (void)
{
  GdkTexture *texture;
  GdkTexture *texture2;
  GInputStream *stream;
  GdkPixbuf *pixbuf;
  GError *error = NULL;

  texture = gdk_texture_new_from_resource ("/org/gtk/libgtk/icons/16x16/places/user-trash.png");

  stream = g_loadable_icon_load (G_LOADABLE_ICON (texture), 16, NULL, NULL, &error);
  g_assert_no_error (error);
  g_assert_nonnull (stream);

  pixbuf = gdk_pixbuf_new_from_stream (stream, NULL, &error);
  g_assert_no_error (error);
  g_assert_nonnull (pixbuf);

  texture2 = gdk_texture_new_for_pixbuf (pixbuf);

  compare_textures (texture, texture2);

  g_object_unref (texture2);
  g_object_unref (pixbuf);
  g_object_unref (stream);
  g_object_unref (texture);
}

static void
icon_loaded (GObject *source,
             GAsyncResult *result,
             gpointer data)
{
  GdkTexture *texture = GDK_TEXTURE (source);
  GError *error = NULL;
  GdkTexture *texture2;
  GdkPixbuf *pixbuf;
  GInputStream *stream;
  gboolean *done = (gboolean *)data;

  stream = g_loadable_icon_load_finish (G_LOADABLE_ICON (texture), result, NULL, &error);
  g_assert_no_error (error);
  g_assert_nonnull (stream);

  pixbuf = gdk_pixbuf_new_from_stream (stream, NULL, &error);
  g_assert_no_error (error);
  g_assert_nonnull (pixbuf);

  texture2 = gdk_texture_new_for_pixbuf (pixbuf);

  compare_textures (texture, texture2);

  g_object_unref (texture2);
  g_object_unref (pixbuf);
  g_object_unref (stream);
  g_object_unref (texture);

  *done = TRUE;
}

static void
test_texture_icon_async (void)
{
  GdkTexture *texture;
  gboolean done = FALSE;

  texture = gdk_texture_new_from_resource ("/org/gtk/libgtk/icons/16x16/places/user-trash.png");

  g_loadable_icon_load_async (G_LOADABLE_ICON (texture), 16, NULL, icon_loaded, &done);

  while (!done)
    g_main_context_iteration (NULL, FALSE);
}

static void
test_texture_icon_serialize (void)
{
  GdkTexture *texture;
  GVariant *data;
  GIcon *icon;

  texture = gdk_texture_new_from_resource ("/org/gtk/libgtk/icons/16x16/places/user-trash.png");

  data = g_icon_serialize (G_ICON (texture));
  g_assert_nonnull (data);
  icon = g_icon_deserialize (data);
  g_assert_true (G_IS_BYTES_ICON (icon));

  g_variant_unref (data);
  g_object_unref (icon);
  g_object_unref (texture);
}

static void
test_texture_diff (void)
{
  GdkTexture *texture0;
  GdkTexture *texture;
  GdkTexture *texture2;
  cairo_region_t *empty;
  cairo_region_t *full;
  cairo_region_t *center;
  cairo_region_t *left;
  cairo_region_t *left_center;

  texture0 = gdk_texture_new_from_resource ("/org/gtk/libgtk/icons/16x16/places/user-trash.png");
  texture = gdk_texture_new_from_resource ("/org/gtk/libgtk/icons/16x16/places/user-trash.png");
  texture2 = gdk_texture_new_from_resource ("/org/gtk/libgtk/icons/16x16/places/user-trash.png");

  empty = cairo_region_create();
  full = cairo_region_create_rectangle (&(cairo_rectangle_int_t) { 0, 0, 16, 16 });
  center = cairo_region_create_rectangle (&(cairo_rectangle_int_t) { 4, 4, 8 ,8 });
  left = cairo_region_create_rectangle (&(cairo_rectangle_int_t) { 0, 4, 4, 4 });
  left_center = cairo_region_copy (left);
  cairo_region_union (left_center, center);

  assert_texture_diff_equal (texture, texture, empty);

  /* No diff set, so we get the full area */
  assert_texture_diff_equal (texture, texture2, full);

  gdk_texture_set_diff (texture, texture2, cairo_region_copy (center));

  assert_texture_diff_equal (texture, texture2, center);

  gdk_texture_set_diff (texture0, texture, cairo_region_copy (left));

  assert_texture_diff_equal (texture0, texture2, left_center);

  g_object_unref (texture);

  assert_texture_diff_equal (texture0, texture2, left_center);

  cairo_region_destroy (full);
  cairo_region_destroy (center);
  cairo_region_destroy (left);
  cairo_region_destroy (left_center);
  g_object_unref (texture2);
  g_object_unref (texture0);
}

static void
test_texture_downloader (void)
{
  GdkTexture *texture;
  GdkTexture *texture2;
  GdkTextureDownloader *downloader;
  GdkTextureDownloader *downloader2;
  gsize stride;
  GBytes *bytes;
  guchar *data;

  texture = gdk_texture_new_from_resource ("/org/gtk/libgtk/icons/16x16/places/user-trash.png");
  texture2 = gdk_texture_new_from_resource ("/org/gtk/libgtk/icons/16x16/places/user-trash.png");

  downloader = gdk_texture_downloader_new (texture);

  downloader2 = gdk_texture_downloader_copy (downloader);
  g_assert_true (gdk_texture_downloader_get_texture (downloader2) == texture);
  gdk_texture_downloader_free (downloader2);

  gdk_texture_downloader_set_texture (downloader, texture2);
  gdk_texture_downloader_set_format (downloader, GDK_MEMORY_R16G16B16A16);
  g_assert_true (gdk_texture_downloader_get_format (downloader) == GDK_MEMORY_R16G16B16A16);

  bytes = gdk_texture_downloader_download_bytes (downloader, &stride);

  g_assert_true (stride == 4 * 2 * 16);
  g_assert_true (g_bytes_get_size (bytes) == stride * 16);

  data = g_malloc (stride * 16);
  gdk_texture_downloader_download_into (downloader, data, stride);

  g_assert_true (memcmp (data, g_bytes_get_data (bytes, NULL), stride * 16) == 0);

  g_free (data);
  g_bytes_unref (bytes);
  gdk_texture_downloader_free (downloader);

  g_object_unref (texture2);
  g_object_unref (texture);
}

int
main (int argc, char *argv[])
{
  /* We want to use resources from libgtk, so we need gtk initialized */
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/texture/from-pixbuf", test_texture_from_pixbuf);
  g_test_add_func ("/texture/from-resource", test_texture_from_resource);
  g_test_add_func ("/texture/save-to-png", test_texture_save_to_png);
  g_test_add_func ("/texture/save-to-tiff", test_texture_save_to_tiff);
  g_test_add_func ("/texture/subtexture", test_texture_subtexture);
  g_test_add_func ("/texture/icon/load", test_texture_icon);
  g_test_add_func ("/texture/icon/load-async", test_texture_icon_async);
  g_test_add_func ("/texture/icon/serialize", test_texture_icon_serialize);
  g_test_add_func ("/texture/diff", test_texture_diff);
  g_test_add_func ("/texture/downloader", test_texture_downloader);

  return g_test_run ();
}
