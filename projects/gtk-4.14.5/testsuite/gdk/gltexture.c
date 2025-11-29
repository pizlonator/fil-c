#include <gtk/gtk.h>
#include <epoxy/gl.h>
#include "gdk/gdktextureprivate.h"
#include "gdk/gdkglcontextprivate.h"
#include "gdk/gdkgltextureprivate.h"

static cairo_surface_t *
make_surface (void)
{
  cairo_surface_t *surface;
  cairo_t *cr;
  guchar *data;

  data = g_malloc (64 * 64 * 4);
  surface = cairo_image_surface_create_for_data (data,
                                                 CAIRO_FORMAT_ARGB32,
                                                 64, 64, 64 * 4);
  cr = cairo_create (surface);
  cairo_set_source_rgb (cr, 1, 0, 0);
  cairo_paint (cr);
  cairo_destroy (cr);

  return surface;
}

static unsigned int
make_gl_texture (GdkGLContext    *context,
                 cairo_surface_t *surface)
{
  unsigned int id;

  glGenTextures (1, &id);
  glActiveTexture (GL_TEXTURE0);
  glBindTexture (GL_TEXTURE_2D, id);
  glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA8, 64, 64, 0, GL_BGRA, GL_UNSIGNED_BYTE,
                cairo_image_surface_get_data (surface));

  g_assert_true (glGetError () == GL_NO_ERROR);

  return id;
}

enum {
  SAME_CONTEXT,
  NO_CONTEXT,
  SHARED_CONTEXT
};

static void
test_gltexture (int test)
{
  GdkDisplay *display;
  GdkGLContext *context;
  GdkGLContext *context2 = NULL;
  GdkGLTextureBuilder *builder;
  GdkTexture *texture;
  cairo_surface_t *surface;
  GError *error = NULL;
  unsigned int id;
  guchar *data;

  display = gdk_display_get_default ();
  if (!gdk_display_prepare_gl (display, &error))
    {
      g_test_message ("no GL support: %s", error->message);
      g_test_skip ("no GL support");
      g_clear_error (&error);
      return;
    }

  context = gdk_display_create_gl_context (display, &error);
  g_assert_nonnull (context);
  g_assert_no_error (error);

  gdk_gl_context_realize (context, &error);
  g_assert_no_error (error);

  surface = make_surface ();

  gdk_gl_context_make_current (context);

  id = make_gl_texture (context, surface);

  if (test == NO_CONTEXT)
    gdk_gl_context_clear_current ();
  else if (test == SHARED_CONTEXT)
    {
      context2 = gdk_display_create_gl_context (display, &error);
      g_assert_nonnull (context2);
      g_assert_no_error (error);

      gdk_gl_context_realize (context2, &error);
      g_assert_no_error (error);

      gdk_gl_context_make_current (context2);
    }

  builder = gdk_gl_texture_builder_new ();
  gdk_gl_texture_builder_set_id (builder, id);
  gdk_gl_texture_builder_set_context (builder, context);
  gdk_gl_texture_builder_set_width (builder, 64);
  gdk_gl_texture_builder_set_height (builder, 64);
  texture = gdk_gl_texture_builder_build (builder, NULL, NULL);

  data = g_malloc0 (64 * 64 * 4);
  gdk_texture_download (texture, data, 64 * 4);

  g_assert_true (memcmp (data, cairo_image_surface_get_data (surface), 64 * 64 * 4) == 0);

  g_free (data);
  g_object_unref (texture);
  g_object_unref (builder);

  cairo_surface_destroy (surface);

  g_object_unref (context);

  g_clear_object (&context2);
}

static void
test_gltexture_same_context (void)
{
  test_gltexture (SAME_CONTEXT);
}

static void
test_gltexture_no_context (void)
{
  test_gltexture (NO_CONTEXT);
}

static void
test_gltexture_shared_context (void)
{
  test_gltexture (SHARED_CONTEXT);
}

static void
test_gltexture_updates (void)
{
  GdkDisplay *display;
  GdkGLContext *context;
  GdkGLTextureBuilder *builder;
  cairo_surface_t *surface;
  GError *error = NULL;
  unsigned int id;
  guchar *data;
  gpointer sync;
  GdkTexture *old_texture;
  GdkTexture *texture;
  cairo_region_t *update_region, *diff;

  display = gdk_display_get_default ();
  if (!gdk_display_prepare_gl (display, &error))
    {
      g_test_message ("no GL support: %s", error->message);
      g_test_skip ("no GL support");
      g_clear_error (&error);
      return;
    }

  context = gdk_display_create_gl_context (display, &error);
  g_assert_nonnull (context);
  g_assert_no_error (error);

  builder = gdk_gl_texture_builder_new ();
  gdk_gl_texture_builder_set_id (builder, 10);

  surface = make_surface ();

  gdk_gl_context_make_current (context);

  id = make_gl_texture (context, surface);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  old_texture = gdk_gl_texture_new (context, id, 64, 64, NULL, NULL);
G_GNUC_END_IGNORE_DEPRECATIONS

  id = make_gl_texture (context, surface);

  if (gdk_gl_context_has_feature (context, GDK_GL_FEATURE_SYNC))
    sync = glFenceSync (GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
  else
    sync = NULL;

  update_region = cairo_region_create_rectangle (&(cairo_rectangle_int_t) { 10, 10, 32, 32 });

  builder = gdk_gl_texture_builder_new ();
  g_object_set (builder,
                "context", context,
                "id", id,
                "width", 64,
                "height", 64,
                "sync", sync,
                "update-texture", old_texture,
                "update-region", update_region,
                NULL);

  g_assert_true (gdk_gl_texture_builder_get_sync (builder) == sync);
  g_assert_true (gdk_gl_texture_builder_get_update_texture (builder) == old_texture);
  g_assert_true (cairo_region_equal (gdk_gl_texture_builder_get_update_region (builder), update_region));

  texture = gdk_gl_texture_builder_build (builder, NULL, NULL);

  g_assert_true (gdk_gl_texture_get_context (GDK_GL_TEXTURE (texture)) == context);
  g_assert_true (gdk_gl_texture_get_id (GDK_GL_TEXTURE (texture)) == id);
  g_assert_false (gdk_gl_texture_has_mipmap (GDK_GL_TEXTURE (texture)));
  g_assert_true (gdk_gl_texture_get_sync (GDK_GL_TEXTURE (texture)) == sync);

  data = g_malloc0 (64 * 64 * 4);
  gdk_texture_download (texture, data, 64 * 4);

  g_assert_true (memcmp (data, cairo_image_surface_get_data (surface), 64 * 64 * 4) == 0);

  diff = cairo_region_create ();
  gdk_texture_diff (texture, old_texture, diff);
  g_assert_true (cairo_region_equal (diff, update_region));
  cairo_region_destroy (diff);

  diff = cairo_region_create ();
  gdk_texture_diff (old_texture, texture, diff);
  g_assert_true (cairo_region_equal (diff, update_region));
  cairo_region_destroy (diff);

  g_free (data);
  g_object_unref (texture);
  g_object_unref (builder);

  cairo_surface_destroy (surface);

  if (sync)
    glDeleteSync (sync);
  cairo_region_destroy (update_region);
  g_object_unref (old_texture);

  g_object_unref (context);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/gltexture/same-context", test_gltexture_same_context);
  g_test_add_func ("/gltexture/no-context", test_gltexture_no_context);
  g_test_add_func ("/gltexture/shared-context", test_gltexture_shared_context);
  g_test_add_func ("/gltexture/updates", test_gltexture_updates);

  return g_test_run ();
}
