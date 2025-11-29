#include "config.h"

#include <gtk/gtk.h>
#include <gdk/gdkdisplayprivate.h>
#include <gdk/gdkglcontextprivate.h>
#include <gdk/gdkdmabuffourccprivate.h>
#include <gdk/gdkdmabuftextureprivate.h>

static void
test_dmabuf_formats_basic (void)
{
  GdkDisplay *display;
  GdkDmabufFormats *formats;

  display = gdk_display_get_default ();

  formats = gdk_display_get_dmabuf_formats (display);

#ifdef HAVE_DMABUF
  /* We always have basic linear formats */
  g_assert_true (gdk_dmabuf_formats_get_n_formats (formats) >= 6);

  g_assert_true (gdk_dmabuf_formats_contains (formats, DRM_FORMAT_ARGB8888, DRM_FORMAT_MOD_LINEAR));
  g_assert_true (gdk_dmabuf_formats_contains (formats, DRM_FORMAT_RGBA8888, DRM_FORMAT_MOD_LINEAR));
  g_assert_true (gdk_dmabuf_formats_contains (formats, DRM_FORMAT_BGRA8888, DRM_FORMAT_MOD_LINEAR));
  g_assert_true (gdk_dmabuf_formats_contains (formats, DRM_FORMAT_ABGR16161616F, DRM_FORMAT_MOD_LINEAR));
  g_assert_true (gdk_dmabuf_formats_contains (formats, DRM_FORMAT_RGB888, DRM_FORMAT_MOD_LINEAR));
  g_assert_true (gdk_dmabuf_formats_contains (formats, DRM_FORMAT_BGR888, DRM_FORMAT_MOD_LINEAR));
#else
  g_assert_true (gdk_dmabuf_formats_get_n_formats (formats) == 0);
#endif
}

static void
test_dmabuf_formats_builder (void)
{
  GdkDmabufFormatsBuilder *builder;
  GdkDmabufFormats *formats1, *formats2;
  guint32 fourcc;
  guint64 modifier;

  builder = gdk_dmabuf_formats_builder_new ();
  gdk_dmabuf_formats_builder_add_format (builder, DRM_FORMAT_ARGB8888, DRM_FORMAT_MOD_LINEAR);
  gdk_dmabuf_formats_builder_add_format (builder, DRM_FORMAT_RGBA8888, DRM_FORMAT_MOD_LINEAR);
  formats1 = gdk_dmabuf_formats_builder_free_to_formats (builder);

  g_assert_true (gdk_dmabuf_formats_contains (formats1, DRM_FORMAT_ARGB8888, DRM_FORMAT_MOD_LINEAR));
  g_assert_true (gdk_dmabuf_formats_contains (formats1, DRM_FORMAT_RGBA8888, DRM_FORMAT_MOD_LINEAR));
  g_assert_false (gdk_dmabuf_formats_contains (formats1, DRM_FORMAT_BGRA8888, DRM_FORMAT_MOD_LINEAR));
  g_assert_true (gdk_dmabuf_formats_get_n_formats (formats1) == 2);
  gdk_dmabuf_formats_get_format (formats1, 0, &fourcc, &modifier);
  g_assert_true (fourcc == DRM_FORMAT_ARGB8888 || fourcc == DRM_FORMAT_RGBA8888);
  g_assert_true (modifier == DRM_FORMAT_MOD_LINEAR);

  g_assert_false (gdk_dmabuf_formats_equal (formats1, NULL));

  builder = gdk_dmabuf_formats_builder_new ();
  gdk_dmabuf_formats_builder_add_formats (builder, formats1);
  formats2 = gdk_dmabuf_formats_builder_free_to_formats (builder);

  g_assert_true (gdk_dmabuf_formats_equal (formats1, formats2));

  gdk_dmabuf_formats_unref (formats2);

  builder = gdk_dmabuf_formats_builder_new ();
  gdk_dmabuf_formats_builder_add_format (builder, DRM_FORMAT_RGBA8888, DRM_FORMAT_MOD_LINEAR);
  gdk_dmabuf_formats_builder_add_format (builder, DRM_FORMAT_ARGB8888, DRM_FORMAT_MOD_LINEAR);
  formats2 = gdk_dmabuf_formats_builder_free_to_formats (builder);

  g_assert_true (gdk_dmabuf_formats_equal (formats1, formats2));

  gdk_dmabuf_formats_unref (formats2);

  builder = gdk_dmabuf_formats_builder_new ();
  gdk_dmabuf_formats_builder_add_formats (builder, formats1);
  gdk_dmabuf_formats_builder_add_format (builder, DRM_FORMAT_RGB888, DRM_FORMAT_MOD_LINEAR);
  formats2 = gdk_dmabuf_formats_builder_free_to_formats (builder);

  g_assert_false (gdk_dmabuf_formats_equal (formats1, formats2));

  gdk_dmabuf_formats_unref (formats2);
  gdk_dmabuf_formats_unref (formats1);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/dmabuf/formats/basic", test_dmabuf_formats_basic);
  g_test_add_func ("/dmabuf/formats/builder", test_dmabuf_formats_builder);

  return g_test_run ();
}
