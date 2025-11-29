#include <gtk/gtk.h>
#include "gdk/loaders/gdkpngprivate.h"
#include "gdk/loaders/gdktiffprivate.h"
#include "gdk/loaders/gdkjpegprivate.h"

static void
assert_texture_equal (GdkTexture *t1,
                      GdkTexture *t2)
{
  int width;
  int height;
  int stride;
  guchar *d1;
  guchar *d2;

  width = gdk_texture_get_width (t1);
  height = gdk_texture_get_height (t1);
  stride = 4 * width;

  g_assert_cmpint (width, ==, gdk_texture_get_width (t2));
  g_assert_cmpint (height, ==, gdk_texture_get_height (t2));

  d1 = g_malloc (stride * height);
  d2 = g_malloc (stride * height);

  gdk_texture_download (t1, d1, stride);
  gdk_texture_download (t2, d2, stride);

  g_assert_cmpmem (d1, stride * height, d2, stride * height);

  g_free (d1);
  g_free (d2);
}

static void
test_load_image (gconstpointer data)
{
  const char *filename = data;
  GdkTexture *texture;
  char *path;
  GFile *file;
  GBytes *bytes;
  GError *error = NULL;

  path = g_test_build_filename (G_TEST_DIST, "image-data", filename, NULL);
  file = g_file_new_for_path (path);
  bytes = g_file_load_bytes (file, NULL, NULL, &error);
  g_assert_no_error (error);

  /* use the internal api, we want to avoid pixbuf fallback here */
  if (g_str_has_suffix (filename, ".png"))
    texture = gdk_load_png (bytes, NULL, &error);
  else if (g_str_has_suffix (filename, ".tiff"))
    texture = gdk_load_tiff (bytes, &error);
  else if (g_str_has_suffix (filename, ".jpeg"))
    texture = gdk_load_jpeg (bytes, &error);
  else
    g_assert_not_reached ();

  g_assert_no_error (error);
  g_assert_true (GDK_IS_TEXTURE (texture));
  g_assert_cmpint (gdk_texture_get_width (texture), ==, 32);
  g_assert_cmpint (gdk_texture_get_height (texture), ==, 32);

  g_object_unref (texture);
  g_bytes_unref (bytes);
  g_object_unref (file);
  g_free (path);
}

static void
test_save_image (gconstpointer test_data)
{
  const char *filename = test_data;
  char *path;
  GdkTexture *texture;
  GFile *file;
  GdkTexture *texture2;
  GError *error = NULL;
  GBytes *bytes = NULL;
  GIOStream *stream;

  path = g_test_build_filename (G_TEST_DIST, "image-data", filename, NULL);
  texture = gdk_texture_new_from_filename (path, &error);
  g_assert_no_error (error);

  /* Test the internal apis here */
  if (g_str_has_suffix (filename, ".png"))
    bytes = gdk_save_png (texture);
  else if (g_str_has_suffix (filename, ".tiff"))
    bytes = gdk_save_tiff (texture);
  else if (g_str_has_suffix (filename, ".jpeg"))
    bytes = gdk_save_jpeg (texture);
  else
    g_assert_not_reached ();

  file = g_file_new_tmp ("imageXXXXXX", (GFileIOStream **)&stream, NULL);
  g_object_unref (stream);
  g_file_replace_contents (file,
                           g_bytes_get_data (bytes, NULL),
                           g_bytes_get_size (bytes),
                           NULL, FALSE, 0,
                           NULL, NULL, &error);
  g_assert_no_error (error);

  texture2 = gdk_texture_new_from_file (file, &error);
  g_assert_no_error (error);


  if (!g_str_has_suffix (filename, ".jpeg"))
    assert_texture_equal (texture, texture2);

  g_bytes_unref (bytes);
  g_object_unref (texture2);
  g_object_unref (texture);
  g_object_unref (file);
  g_free (path);
}

static void
test_load_image_fail (gconstpointer data)
{
  const char *filename = data;
  char *path;
  GdkTexture *texture;
  GError *error = NULL;

  path = g_test_build_filename (G_TEST_DIST, "bad-image-data", filename, NULL);
  texture = gdk_texture_new_from_filename (path, &error);
  g_assert_nonnull (error);
  g_assert_null (texture);

  g_error_free (error);

  g_free (path);
}

int
main (int argc, char *argv[])
{
  char *path;
  GDir *dir;
  GError *error = NULL;
  const char *name;

  (g_test_init) (&argc, &argv, NULL);

  path = g_test_build_filename (G_TEST_DIST, "image-data", NULL);
  dir = g_dir_open (path, 0, &error);
  g_assert_no_error (error);
  g_free (path);

  while ((name = g_dir_read_name (dir)) != NULL)
   {
     char *test = g_strconcat ("/image/load/", name, NULL);
     g_test_add_data_func (test, name, test_load_image);
     g_free (test);
   }

  path = g_test_build_filename (G_TEST_DIST, "bad-image-data", NULL);
  dir = g_dir_open (path, 0, &error);
  g_assert_no_error (error);
  g_free (path);

  while ((name = g_dir_read_name (dir)) != NULL)
   {
     char *test = g_strconcat ("/image/fail/", name, NULL);
     g_test_add_data_func (test, name, test_load_image_fail);
     g_free (test);
   }

  g_test_add_data_func ("/image/save/image.png", "image.png", test_save_image);
  g_test_add_data_func ("/image/save/image.tiff", "image.tiff", test_save_image);
  g_test_add_data_func ("/image/save/image.jpeg", "image.jpeg", test_save_image);

  return g_test_run ();
}
