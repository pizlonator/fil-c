/* GStreamer
 *
 * Copyright (C) 2020 Matthew Waters <matthew@centricular.com>
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
#include <gst/gl/gl.h>
#include <gst/check/gstcheck.h>
#include <gst/check/gstharness.h>

static void
replace_display (GstHarness * h)
{
  GstContext *new_context;
  GstGLDisplay *new_display;
  GstGLContext *expected, *gl_context;
  GstBuffer *buf;

  /* replaces the GstGLDisplay used by @h with verification */

  fail_unless_equals_int (GST_FLOW_OK, gst_harness_push_from_src (h));
  /* need a second buffer to pull one, videoaggregator has one frame latency */
  fail_unless_equals_int (GST_FLOW_OK, gst_harness_push_from_src (h));
  buf = gst_harness_pull (h);
  fail_unless (buf != NULL);
  gst_clear_buffer (&buf);

  g_object_get (G_OBJECT (h->element), "context", &gl_context, NULL);
  fail_unless (gl_context != NULL);
  gst_clear_object (&gl_context);

  new_display = gst_gl_display_new ();
  fail_unless (gst_gl_display_create_context (new_display, NULL, &expected,
          NULL));
  fail_unless (expected != NULL);
  fail_unless (gst_gl_display_add_context (new_display, expected));

  new_context = gst_context_new (GST_GL_DISPLAY_CONTEXT_TYPE, TRUE);
  gst_context_set_gl_display (new_context, new_display);

  gst_element_set_context (h->element, new_context);
  gst_context_unref (new_context);
  new_context = NULL;

  fail_unless_equals_int (GST_FLOW_OK, gst_harness_push_from_src (h));
  buf = gst_harness_pull (h);
  fail_unless (buf != NULL);
  gst_clear_buffer (&buf);

  g_object_get (G_OBJECT (h->element), "context", &gl_context, NULL);
  fail_unless (gl_context != NULL);

  fail_unless (gl_context == expected);
  fail_unless (new_display == gl_context->display);

  gst_object_unref (expected);
  gst_object_unref (gl_context);
  gst_object_unref (new_display);
}

GST_START_TEST (test_glvideomixer_negotiate)
{
  GstHarness *mix;
  GstBuffer *buf;

  mix = gst_harness_new_with_padnames ("glvideomixer", "sink_0", "src");
  gst_harness_set_live (mix, FALSE);
  gst_harness_set_blocking_push_mode (mix);
  gst_harness_set_caps_str (mix,
      "video/x-raw(memory:GLMemory),format=RGBA,width=1,height=1,framerate=25/1,texture-target=2D",
      "video/x-raw(memory:GLMemory),format=RGBA,width=1,height=1,framerate=25/1,texture-target=2D");
  gst_harness_add_src (mix, "gltestsrc", FALSE);
  gst_harness_set_live (mix->src_harness, FALSE);
  gst_harness_set_blocking_push_mode (mix->src_harness);

  fail_unless_equals_int (GST_FLOW_OK, gst_harness_push_from_src (mix));
  fail_unless (gst_harness_push_event (mix, gst_event_new_eos ()));

  buf = gst_harness_pull (mix);
  fail_unless (buf != NULL);
  gst_clear_buffer (&buf);

  gst_harness_teardown (mix);
}

GST_END_TEST;

GST_START_TEST (test_glvideomixer_display_replace)
{
  GstHarness *mix;

  mix = gst_harness_new_with_padnames ("glvideomixer", "sink_0", "src");
  gst_harness_set_live (mix, FALSE);
  gst_harness_set_blocking_push_mode (mix);
  gst_harness_set_caps_str (mix,
      "video/x-raw(memory:GLMemory),format=RGBA,width=1,height=1,framerate=25/1,texture-target=2D",
      "video/x-raw(memory:GLMemory),format=RGBA,width=1,height=1,framerate=25/1,texture-target=2D");
  gst_harness_add_src (mix, "gltestsrc", FALSE);
  gst_harness_set_live (mix->src_harness, FALSE);
  gst_harness_set_blocking_push_mode (mix->src_harness);

  replace_display (mix);

  gst_harness_teardown (mix);
}

GST_END_TEST;

static Suite *
glmixer_suite (void)
{
  Suite *s = suite_create ("glmixer");
  TCase *tc = tcase_create ("general");

  tcase_add_test (tc, test_glvideomixer_negotiate);
  tcase_add_test (tc, test_glvideomixer_display_replace);
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
  s = glmixer_suite ();
  return gst_check_run_suite (s, "glmixer", __FILE__);
}
