#include <gtk/gtk.h>

/* This function will be called from a thread and/or the main loop.
 * Textures are threadsafe after all. */
static void
ensure_texture_access (GdkTexture *texture)
{
  /* Make sure to initialize the pixel to anything but red */
  guint32 pixel = 0;

  g_test_message ("Checking texture access in thread %p...", g_thread_self());
  /* Just to be sure */
  g_assert_cmpint (gdk_texture_get_width (texture), ==, 1);
  g_assert_cmpint (gdk_texture_get_height (texture), ==, 1);

  /* download the pixel */
  gdk_texture_download (texture, (guchar *) &pixel, 4);

  /* check the pixel is now red */
  g_assert_cmphex (pixel, ==, 0xFFFF0000);

  g_test_message ("...done in thread %p", g_thread_self());
}

static void
texture_download_done (GObject      *texture,
                       GAsyncResult *res,
                       gpointer      loop)
{
  ensure_texture_access (GDK_TEXTURE (texture));

  g_main_loop_quit (loop);
}

static void
texture_download_thread (GTask        *task,
                         gpointer      texture,
                         gpointer      unused,
                         GCancellable *cancellable)
{
  g_test_message ("Starting thread %p.", g_thread_self());
  /* not sure this can happen, but if it does, we
   * should clear_current() here. */
  g_assert_null (gdk_gl_context_get_current ());

  ensure_texture_access (GDK_TEXTURE (texture));

  /* Makes sure the GL context is still NULL, because all the
   * GL stuff should have happened in the main thread. */
  g_assert_null (gdk_gl_context_get_current ());
  g_test_message ("Returning from thread %p.", g_thread_self());

  g_task_return_boolean (task, TRUE);
}

static void
texture_threads (void)
{
  GskRenderer *gl_renderer;
  GskRenderNode *node;
  GMainLoop *loop;
  GdkTexture *texture;
  GTask *task;
  GError *error = NULL;

  /* 1. Get a GL renderer */
  gl_renderer = gsk_gl_renderer_new ();
  if (!gsk_renderer_realize_for_display (gl_renderer, gdk_display_get_default (), &error))
    {
      g_test_skip (error->message);

      g_clear_error (&error);
      g_clear_object (&gl_renderer);
      return;
    }

  /* 2. Get a GL texture */
  node = gsk_color_node_new (&(GdkRGBA) { 1, 0, 0, 1 }, &GRAPHENE_RECT_INIT(0, 0, 1, 1));
  texture = gsk_renderer_render_texture (gl_renderer, node, &GRAPHENE_RECT_INIT(0, 0, 1, 1));
  gsk_render_node_unref (node);

  /* 3. This is a bit fishy, but we want to make sure that
   * the texture's GL context is current in the main thread.
   *
   * If we had access to the context, we'd make_current() here.
   */
  ensure_texture_access (texture);
  g_assert_nonnull (gdk_gl_context_get_current ());

  /* 4. Acquire the main loop, so the run_in_thread() doesn't
   * try to acquire it if it manages to outrace this thread.
   */
  g_assert_true (g_main_context_acquire (NULL));

  /* 5. Run a thread trying to download the texture */
  loop = g_main_loop_new (NULL, TRUE);
  task = g_task_new (texture, NULL, texture_download_done, loop);
  g_task_run_in_thread (task, texture_download_thread);
  g_clear_object (&task);

  /* 6. Run the main loop waiting for the thread to return */
  g_main_loop_run (loop);

  /* 7. All good */
  gsk_renderer_unrealize (gl_renderer);
  g_clear_pointer (&loop, g_main_loop_unref);
  g_clear_object (&gl_renderer);
  g_main_context_release (NULL);
  gdk_gl_context_clear_current ();
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/texture-threads", texture_threads);

  return g_test_run ();
}
