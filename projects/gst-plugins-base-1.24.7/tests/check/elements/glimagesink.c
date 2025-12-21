/* GStreamer
 *
 * Unit tests for glimagesink
 *
 * Copyright (C) 2014 Julien Isorce <j.isorce@samsung.com>
 * Copyright (C) 2016 Matthew Waters <matthew@centricular.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/check/gstcheck.h>
#include <gst/gl/gl.h>

static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("RGBA"))
    );

static GstElement *sinkelement = NULL;
static GstPad *srcpad = NULL;

#define PAD_FUNC(name, type, param, check) \
  static gpointer do_##name##_func (type * param) { \
  fail_unless (gst_pad_##name (srcpad, param) == check); \
  return NULL; \
}

/* *INDENT-OFF* */
PAD_FUNC (peer_query, GstQuery, query, TRUE)
PAD_FUNC (push, GstBuffer, buf, GST_FLOW_OK)
/* *INDENT-ON* */

/* On OSX it's required to have a main loop running in the
 * main thread while using the display connection.
 * So the call that use this display connection needs to be
 * done in another thread.
 * On other platforms a direct call can be done. */
#ifdef __APPLE__
static GMainLoop *loop;
static GThread *thread;
static GMutex lock;
static GCond cond;

static gboolean
_unlock_mutex (gpointer * unused)
{
  g_cond_broadcast (&cond);
  g_mutex_unlock (&lock);

  return G_SOURCE_REMOVE;
}

static gpointer
_thread_main (gpointer * unused)
{
  g_mutex_lock (&lock);
  g_idle_add ((GSourceFunc) _unlock_mutex, NULL);

  g_main_loop_run (loop);

  g_main_loop_unref (loop);
  loop = NULL;

  return NULL;
}

static void
_start_thread (void)
{
  loop = g_main_loop_new (NULL, FALSE);
  g_mutex_init (&lock);
  g_cond_init (&cond);

  thread = g_thread_new ("GLOSXTestThread", (GThreadFunc) _thread_main, NULL);

  g_mutex_lock (&lock);
  while (!loop) {
    g_cond_wait (&cond, &lock);
  }
  g_mutex_unlock (&lock);
}

static void
_stop_thread (void)
{
  g_main_loop_quit (loop);
  g_thread_join (thread);

  g_mutex_clear (&lock);
  g_cond_clear (&cond);
}
#endif

static void
setup_glimagesink (void)
{
  GstCaps *caps = NULL;

  sinkelement = gst_check_setup_element ("glimagesink");
  srcpad = gst_check_setup_src_pad (sinkelement, &srctemplate);
  gst_pad_set_active (srcpad, TRUE);

  caps =
      gst_caps_from_string
      ("video/x-raw, width=320, height=240, format=RGBA, framerate=30/1");
  gst_check_setup_events (srcpad, sinkelement, caps, GST_FORMAT_TIME);
  gst_caps_unref (caps);
}

static void
cleanup_glimagesink (void)
{
  gst_element_set_state (sinkelement, GST_STATE_NULL);
  gst_element_get_state (sinkelement, NULL, NULL, GST_CLOCK_TIME_NONE);
  gst_pad_set_active (srcpad, FALSE);
  gst_check_teardown_src_pad (sinkelement);
  gst_check_teardown_element (sinkelement);
}

/* Verify that glimagesink releases the buffers it currently
 * owns, upon a drain query. */
GST_START_TEST (test_query_drain)
{
  GstBuffer *buf = NULL;
  GstBufferPool *originpool = NULL;
  GstBufferPool *pool = NULL;
  GstCaps *caps = NULL;
  GstQuery *query = NULL;
  GstStructure *config = NULL;
  gint i = 0;
  guint min = 0;
  guint max = 0;
  guint size = 0;
  const gint maxbuffers = 4;

#ifdef __APPLE__
  _start_thread ();
#endif

  /* GstBaseSink handles the drain query as well. */
  g_object_set (sinkelement, "enable-last-sample", TRUE, NULL);

  ASSERT_SET_STATE (sinkelement, GST_STATE_PLAYING, GST_STATE_CHANGE_ASYNC);

  caps = gst_pad_get_current_caps (srcpad);
  fail_unless (gst_caps_is_fixed (caps));

  /* Let's retrieve the GstGLBufferPool to change its min
   * and max nb buffers. For that just send an allocation
   * query and change the pool config. */
  query = gst_query_new_allocation (caps, TRUE);
  do_peer_query_func (query);

  fail_unless (gst_query_get_n_allocation_pools (query) == 1);

  gst_query_parse_nth_allocation_pool (query, 0, &originpool, &size, &min,
      &max);
  fail_unless (originpool != NULL);
  gst_query_unref (query);

  config = gst_buffer_pool_get_config (originpool);
  gst_buffer_pool_config_set_params (config, caps, size, maxbuffers,
      maxbuffers);
  gst_buffer_pool_config_set_gl_min_free_queue_size (config, 0);
  fail_unless (gst_buffer_pool_set_config (originpool, config));

  /* The gl pool is setup and ready to be activated. */
  fail_unless (gst_buffer_pool_set_active (originpool, TRUE));

  /* Let's build an upstream pool that will be feed with
   * gl buffers. */
  pool = gst_buffer_pool_new ();
  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_set_params (config, caps, size, maxbuffers,
      maxbuffers);
  fail_unless (gst_buffer_pool_set_config (pool, config));
  gst_caps_unref (caps);

  fail_unless (gst_buffer_pool_set_active (pool, TRUE));

  /* Unpopulate the pool and forget about its initial buffers 
   * It is necessary because the pool has to know there are 
   * N outstanding buffers. */
  for (i = 0; i < maxbuffers; ++i) {
    fail_unless (gst_buffer_pool_acquire_buffer (pool, &buf,
            NULL) == GST_FLOW_OK);
    gst_object_replace ((GstObject **) & buf->pool, NULL);
    gst_buffer_unref (buf);
  }

  /* Transfer buffers from the gl pool to the upstream pool. */
  for (i = 0; i < maxbuffers; ++i) {
    fail_unless (gst_buffer_pool_acquire_buffer (originpool, &buf,
            NULL) == GST_FLOW_OK);
    gst_object_replace ((GstObject **) & buf->pool, (GstObject *) pool);
    gst_buffer_unref (buf);
  }

  /* Push a lot of buffers like if a real pipeline was running. */
  for (i = 0; i < 10 * maxbuffers; ++i) {
    fail_unless (gst_buffer_pool_acquire_buffer (pool, &buf,
            NULL) == GST_FLOW_OK);
    do_push_func (buf);
  }

  /* Claim back buffers to the upstream pool. This is the point
   * of this unit test, i.e. this test checks that glimagesink
   * releases the buffers it currently owns, upon drain query. */
  query = gst_query_new_drain ();
  do_peer_query_func (query);
  gst_query_unref (query);

  /* Transfer buffers back to the downstream pool to be release
   * properly. This also make sure that all buffers are returned.
   * Indeed gst_buffer_pool_acquire_buffer is blocking here and
   * we have set a maximum. */
  for (i = 0; i < maxbuffers; ++i) {
    fail_unless (gst_buffer_pool_acquire_buffer (pool, &buf,
            NULL) == GST_FLOW_OK);
    gst_object_replace ((GstObject **) & buf->pool, (GstObject *) originpool);
    gst_buffer_unref (buf);
  }

  fail_unless (gst_buffer_pool_set_active (originpool, FALSE));
  gst_object_unref (originpool);

  /* At this point the gl pool contains all its buffers. We can
   * deactivate it to release the textures. Note that only the gl
   * pool can release the textures properly because it has a
   * reference on the gl context. */
  fail_unless (gst_buffer_pool_set_active (pool, FALSE));
  gst_object_unref (pool);

#ifdef __APPLE__
  _stop_thread ();
#endif
}

GST_END_TEST;

static Suite *
glimagesink_suite (void)
{
  Suite *s = suite_create ("glimagesink");
  TCase *tc = tcase_create ("general");

  tcase_set_timeout (tc, 5);

  tcase_add_checked_fixture (tc, setup_glimagesink, cleanup_glimagesink);
  tcase_add_test (tc, test_query_drain);
  suite_add_tcase (s, tc);

  return s;
}

int
main (int argc, char **argv)
{
  Suite *s;
  g_setenv ("GST_GL_XINITTHREADS", "1", TRUE);
  g_setenv ("GST_XINITTHREADS", "1", TRUE);
  gst_check_init (&argc, &argv);
  s = glimagesink_suite ();
  return gst_check_run_suite (s, "glimagesink", __FILE__);
}
