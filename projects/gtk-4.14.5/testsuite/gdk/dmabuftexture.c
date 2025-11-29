#include "config.h"

#include <gtk/gtk.h>
#include <gdk/gdkdisplayprivate.h>
#include <gdk/gdkglcontextprivate.h>
#include <gdk/gdkdmabuffourccprivate.h>
#include <gdk/gdkdmabuftextureprivate.h>

static cairo_surface_t *
make_surface (int width,
              int height)
{
  cairo_surface_t *surface;
  cairo_t *cr;
  guchar *data;
  int stride;

  stride = width * 4;
  data = g_malloc (stride * height);
  surface = cairo_image_surface_create_for_data (data,
                                                 CAIRO_FORMAT_ARGB32,
                                                 width, height, stride);
  cr = cairo_create (surface);
  cairo_set_source_rgb (cr, 1, 0, 0);
  cairo_paint (cr);
  cairo_destroy (cr);

  return surface;
}

static unsigned int
upload_gl_texture (GdkGLContext    *context,
                   cairo_surface_t *surface)
{
  unsigned int id;
  int width, height;

  width = cairo_image_surface_get_width (surface);
  height = cairo_image_surface_get_height (surface);

  glGenTextures (1, &id);
  glActiveTexture (GL_TEXTURE0);
  glBindTexture (GL_TEXTURE_2D, id);
  glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE,
                cairo_image_surface_get_data (surface));

  g_assert_true (glGetError () == GL_NO_ERROR);

  return id;
}

static void
export_dmabuf (GdkGLContext *context,
               unsigned int  texture_id,
               GdkDmabuf    *dmabuf)
{
  gboolean ret;

  ret = gdk_gl_context_export_dmabuf (context, texture_id, dmabuf);

  g_assert_true (ret);
}

static void
free_dmabuf (gpointer data)
{
  GdkDmabuf *dmabuf = data;

  for (int i = 0; i < dmabuf->n_planes; i++)
    close (dmabuf->planes[i].fd);

  g_free (dmabuf);
}

static GdkTexture *
make_dmabuf_texture (GdkDisplay   *display,
                     int           width,
                     int           height,
                     gboolean      premultiplied,
                     GdkDmabuf    *dmabuf)
{
  GdkDmabufTextureBuilder *builder;
  GdkTexture *texture;
  GdkDmabuf *dmabuf2;

  builder = gdk_dmabuf_texture_builder_new ();

  gdk_dmabuf_texture_builder_set_display (builder, display);
  gdk_dmabuf_texture_builder_set_width (builder, width);
  gdk_dmabuf_texture_builder_set_premultiplied (builder, premultiplied);
  gdk_dmabuf_texture_builder_set_height (builder, height);
  gdk_dmabuf_texture_builder_set_fourcc (builder, dmabuf->fourcc);
  gdk_dmabuf_texture_builder_set_modifier (builder, dmabuf->modifier);
  gdk_dmabuf_texture_builder_set_n_planes (builder, dmabuf->n_planes);
  for (int i = 0; i < dmabuf->n_planes; i++)
    {
      gdk_dmabuf_texture_builder_set_fd (builder, i, dmabuf->planes[i].fd);
      gdk_dmabuf_texture_builder_set_stride (builder, i, dmabuf->planes[i].stride);
      gdk_dmabuf_texture_builder_set_offset (builder, i, dmabuf->planes[i].offset);
    }

  dmabuf2 = g_new (GdkDmabuf, 1);
  memcpy (dmabuf2, dmabuf, sizeof (GdkDmabuf));

  texture = gdk_dmabuf_texture_builder_build (builder, free_dmabuf, dmabuf2, NULL);

  g_assert_true (texture != NULL);

  g_object_unref (builder);

  return texture;
}

/* Make a dmabuftexture by exporting a GL texture,
 * then download it and compare with the original
 */
static void
test_dmabuf_export (void)
{
  GdkDisplay *display;
  GdkGLContext *context;
  GError *error = NULL;
  cairo_surface_t *surface;
  unsigned int texture_id;
  GdkDmabuf dmabuf;
  GdkTexture *texture;
  guchar *data;

  display = gdk_display_get_default ();
  if (!gdk_display_prepare_gl (display, &error))
    {
      g_test_skip_printf ("no GL support: %s", error->message);
      g_clear_error (&error);
      return;
    }

  if (gdk_dmabuf_formats_get_n_formats (gdk_display_get_dmabuf_formats (display)) == 0)
    {
      g_test_skip_printf ("no dmabuf support");
      return;
    }

  context = gdk_display_create_gl_context (display, &error);
  g_assert_nonnull (context);
  g_assert_no_error (error);

  gdk_gl_context_realize (context, &error);
  g_assert_no_error (error);

  surface = make_surface (64, 64);

  gdk_gl_context_make_current (context);

  texture_id = upload_gl_texture (context, surface);
  export_dmabuf (context, texture_id, &dmabuf);
  texture = make_dmabuf_texture (display, 64, 64, TRUE, &dmabuf);

  data = g_malloc (64 * 64 * 4);
  gdk_texture_download (texture, data, 64 * 4);

  g_assert_true (memcmp (cairo_image_surface_get_data (surface), data, 64 * 64 * 4) == 0);

  g_free (data);
  g_object_unref (texture);
  cairo_surface_destroy (surface);

  gdk_gl_context_make_current (context);
  glDeleteTextures (1, &texture_id);

  g_object_unref (context);
}

/* Make a dmabuftexture by exporting a GL texture,
 * then import it into another GL context, download
 * the resulting texture, and compare it to the original.
 */
static void
test_dmabuf_import (void)
{
  GdkDisplay *display;
  GdkGLContext *context;
  GdkGLContext *context2;
  GError *error = NULL;
  cairo_surface_t *surface;
  unsigned int texture_id;
  unsigned int texture_id2;
  GdkDmabuf dmabuf;
  const GdkDmabuf *dmabuf2;
  GdkTexture *texture;
  GdkTexture *texture2;
  GdkGLTextureBuilder *builder;
  guchar *data;
  gboolean external;

  display = gdk_display_get_default ();
  if (!gdk_display_prepare_gl (display, &error))
    {
      g_test_skip_printf ("no GL support: %s", error->message);
      g_clear_error (&error);
      return;
    }

  if (gdk_dmabuf_formats_get_n_formats (gdk_display_get_dmabuf_formats (display)) == 0)
    {
      g_test_skip_printf ("no dmabuf support");
      return;
    }

  context = gdk_display_create_gl_context (display, &error);
  g_assert_nonnull (context);
  g_assert_no_error (error);

  gdk_gl_context_realize (context, &error);
  g_assert_no_error (error);

  surface = make_surface (64, 64);

  gdk_gl_context_make_current (context);

  texture_id = upload_gl_texture (context, surface);
  export_dmabuf (context, texture_id, &dmabuf);
  texture = make_dmabuf_texture (display, 64, 64, TRUE, &dmabuf);

  context2 = gdk_display_create_gl_context (display, &error);
  g_assert_nonnull (context2);
  g_assert_no_error (error);

  gdk_gl_context_realize (context2, &error);
  g_assert_no_error (error);

  dmabuf2 = gdk_dmabuf_texture_get_dmabuf (GDK_DMABUF_TEXTURE (texture));
  texture_id2 = gdk_gl_context_import_dmabuf (context2, 64, 64, dmabuf2, &external);
  g_assert_cmpint (texture_id2, !=, 0);
  g_assert_false (external);

  builder = gdk_gl_texture_builder_new ();
  gdk_gl_texture_builder_set_context (builder, context2);
  gdk_gl_texture_builder_set_id (builder, texture_id2);
  gdk_gl_texture_builder_set_width (builder, 64);
  gdk_gl_texture_builder_set_height (builder, 64);
  gdk_gl_texture_builder_set_format (builder, GDK_MEMORY_A8R8G8B8_PREMULTIPLIED);
  texture2 = gdk_gl_texture_builder_build (builder, NULL, NULL);

  data = g_malloc (64 * 64 * 4);
  gdk_texture_download (texture2, data, 64 * 4);

  g_assert_true (memcmp (cairo_image_surface_get_data (surface), data, 64 * 64 * 4) == 0);

  g_free (data);
  g_object_unref (texture);
  g_object_unref (texture2);

  gdk_gl_context_make_current (context);
  glDeleteTextures (1, &texture_id);

  g_object_unref (context);
  g_object_unref (context2);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/dmabuf/export", test_dmabuf_export);
  g_test_add_func ("/dmabuf/import", test_dmabuf_import);

  return g_test_run ();
}
