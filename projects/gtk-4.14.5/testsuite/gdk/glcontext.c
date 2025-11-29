#include <gtk/gtk.h>

#define ALL_APIS (GDK_GL_API_GL | GDK_GL_API_GLES)

static GdkGLAPI
is_unique (GdkGLAPI api)
{
  return (api & (api - 1)) == 0;
}

static void
test_allowed_backends (gconstpointer data)
{
  GdkGLAPI allowed = GPOINTER_TO_SIZE (data);
  GdkGLAPI not_allowed = (~allowed) & ALL_APIS;
  GdkGLAPI api, random_apis;
  GdkDisplay *display;
  GdkGLContext *context;
  GError *error = NULL;

  display = gdk_display_get_default ();
  if (!gdk_display_prepare_gl (display, &error))
    {
      g_test_skip_printf ("no GL support: %s", error->message);
      g_clear_error (&error);
      return;
    }

  context = gdk_display_create_gl_context (display, &error);
  g_assert_nonnull (context);
  g_assert_no_error (error);
  g_assert_cmpint (gdk_gl_context_get_api (context), ==, 0);
  g_assert_cmpint (gdk_gl_context_get_allowed_apis (context), ==, ALL_APIS);

  gdk_gl_context_set_allowed_apis (context, allowed);
  g_assert_cmpint (gdk_gl_context_get_allowed_apis (context), ==, allowed);
  g_assert_cmpint (gdk_gl_context_get_api (context), ==, 0);

  if (!gdk_gl_context_realize (context, &error))
    {
      g_assert_cmpint (gdk_gl_context_get_api (context), ==, 0);

      if (not_allowed && g_error_matches (error, GDK_GL_ERROR, GDK_GL_ERROR_NOT_AVAILABLE))
        {
          g_clear_error (&error);
          g_object_unref (context);
          return;
        }
      g_assert_no_error (error);
    }

  g_assert_no_error (error);

  g_assert_cmpint (gdk_gl_context_get_allowed_apis (context), ==, allowed);

  api = gdk_gl_context_get_api (context);
  g_assert_cmpint (api, !=, 0);
  g_assert_true (is_unique (api));
  g_assert_cmpint (api & allowed, ==, api);
  g_assert_cmpint (api & not_allowed, ==, 0);

  random_apis = g_test_rand_int_range (0, ALL_APIS + 1);
  gdk_gl_context_set_allowed_apis (context, random_apis);
  g_assert_cmpint (gdk_gl_context_get_allowed_apis (context), ==, random_apis);
  g_assert_cmpint (gdk_gl_context_get_api (context), ==, api);

  g_object_unref (context);
}

static void
test_use_es (void)
{
  GdkDisplay *display;
  GdkGLContext *context;
  GError *error = NULL;
  GdkGLAPI allowed_apis, api;
  GdkGLContext *shared;

  display = gdk_display_get_default ();
  if (!gdk_display_prepare_gl (display, &error))
    {
      g_test_skip_printf ("no GL support: %s", error->message);
      g_clear_error (&error);
      return;
    }

  context = gdk_display_create_gl_context (display, &error);
  g_assert_nonnull (context);
  g_assert_no_error (error);

  g_object_set (context, "allowed-apis", GDK_GL_API_GL | GDK_GL_API_GLES, NULL);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gdk_gl_context_set_use_es (context, 1);
  g_assert_true (gdk_gl_context_get_allowed_apis (context) == GDK_GL_API_GLES);
  gdk_gl_context_set_use_es (context, 0);
  g_assert_true (gdk_gl_context_get_allowed_apis (context) == GDK_GL_API_GL);
  gdk_gl_context_set_use_es (context, -1);
  g_assert_true (gdk_gl_context_get_allowed_apis (context) == (GDK_GL_API_GL | GDK_GL_API_GLES));
G_GNUC_END_IGNORE_DEPRECATIONS

  api = gdk_gl_context_realize (context, &error);
  g_assert_no_error (error);
  g_assert_true (api != 0);

  g_object_get (context,
                "allowed-apis", &allowed_apis,
                "api", &api,
                "shared-context", &shared,
                NULL);

  g_assert_true (allowed_apis == (GDK_GL_API_GL | GDK_GL_API_GLES));
  g_assert_true (api == GDK_GL_API_GL || api == GDK_GL_API_GLES);
  g_assert_null (shared);

  g_object_unref (context);
}

static void
test_version (void)
{
  GdkDisplay *display;
  GdkGLContext *context;
  GError *error = NULL;
  int major, minor;

  display = gdk_display_get_default ();
  if (!gdk_display_prepare_gl (display, &error))
    {
      g_test_skip_printf ("no GL support: %s", error->message);
      g_clear_error (&error);
      return;
    }

  context = gdk_display_create_gl_context (display, &error);
  g_assert_nonnull (context);
  g_assert_no_error (error);

  gdk_gl_context_get_required_version (context, &major, &minor);
  g_assert_true (major == 0 && minor == 0);

  gdk_gl_context_set_required_version (context, 4, 0);
  gdk_gl_context_get_required_version (context, &major, &minor);
  g_assert_true (major == 4 && minor == 0);

  g_object_unref (context);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_data_func ("/allowed-apis/none", GSIZE_TO_POINTER (0), test_allowed_backends);
  g_test_add_data_func ("/allowed-apis/gl", GSIZE_TO_POINTER (GDK_GL_API_GL), test_allowed_backends);
  g_test_add_data_func ("/allowed-apis/gles", GSIZE_TO_POINTER (GDK_GL_API_GLES), test_allowed_backends);
  g_test_add_data_func ("/allowed-apis/all", GSIZE_TO_POINTER (GDK_GL_API_GL | GDK_GL_API_GLES), test_allowed_backends);

  g_test_add_func ("/allowed-apis/use-es", test_use_es);
  g_test_add_func ("/allowed-apis/version", test_version);

  return g_test_run ();
}
