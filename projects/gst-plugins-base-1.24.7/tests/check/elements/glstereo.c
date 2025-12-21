/* GStreamer
 *
 * Copyright (C) 2019 Matthew Waters <matthew@centricular.com>
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

GST_START_TEST (test_glstereosplit_negotiate)
{
  GstHarness *mix;
  GstBuffer *buf;

  mix =
      gst_harness_new_parse ("gltestsrc num-buffers=1 ! "
      "glviewconvert output-mode-override=side-by-side ! "
      "glstereosplit name=s glstereomix name=m s.left ! m. s.right ! m.");
  gst_harness_use_systemclock (mix);
  gst_harness_set_blocking_push_mode (mix);
  gst_harness_set_sink_caps_str (mix,
      "video/x-raw(memory:GLMemory),format=RGBA,width=1,height=1,"
      "framerate=30/1,texture-target=2D");

  gst_harness_play (mix);
  buf = gst_harness_pull (mix);
  fail_unless (buf != NULL);
  gst_clear_buffer (&buf);

  gst_harness_teardown (mix);
}

GST_END_TEST;

static Suite *
glstereo_suite (void)
{
  Suite *s = suite_create ("glstereo");
  TCase *tc = tcase_create ("general");

  tcase_add_test (tc, test_glstereosplit_negotiate);
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
  s = glstereo_suite ();
  return gst_check_run_suite (s, "glstereo", __FILE__);
}
