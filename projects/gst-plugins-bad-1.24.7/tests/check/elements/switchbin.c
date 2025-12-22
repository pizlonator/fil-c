/* GStreamer
 *
 * unit test for switchbin element
 * Copyright (C) 2018 Jan Schmidt <jan@centricular.com>
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
#include <gst/check/gstcheck.h>
#include <gst/check/gstharness.h>

GST_START_TEST (test_switchbin_simple)
{
  GstBus *bus;
  GstElement *switchbin, *e0, *e1;
  GstCaps *c0, *c1;
  GstHarness *h;
  guint path_index;

  GstBuffer *in_buf;
  GstBuffer *out_buf;

  bus = gst_bus_new ();

  switchbin = gst_element_factory_make ("switchbin", NULL);
  fail_unless (switchbin != NULL);
  g_object_set (switchbin, "num-paths", 2, NULL);
  gst_element_set_bus (switchbin, bus);
  h = gst_harness_new_with_element (switchbin, "sink", "src");

  e0 = gst_element_factory_make ("identity", NULL);
  c0 = gst_caps_from_string ("audio/x-raw,format=S16LE,rate=48000,channels=2");
  e1 = gst_element_factory_make ("identity", NULL);
  c1 = gst_caps_from_string ("audio/x-raw,format=S16LE,rate=44100,channels=1");

  /* switchbin owns the elements after this */
  gst_child_proxy_set (GST_CHILD_PROXY (switchbin),
      "path0::element", e0, "path0::caps", c0,
      "path1::element", e1, "path1::caps", c1, NULL);

  /* Create a small buffer push it in, pull it out,
   * and check the switchbin selected the right path for these caps */
  gst_harness_set_src_caps (h, c0);
  in_buf = gst_harness_create_buffer (h, 480);
  gst_harness_push (h, in_buf);
  out_buf = gst_harness_pull (h);
  fail_unless (in_buf == out_buf);
  gst_buffer_unref (out_buf);
  g_object_get (switchbin, "current-path", &path_index, NULL);
  fail_unless (path_index == 0);

  /* Change caps, push more */
  gst_harness_set_src_caps (h, c1);
  in_buf = gst_harness_create_buffer (h, 480);
  gst_harness_push (h, in_buf);
  out_buf = gst_harness_pull (h);
  fail_unless (in_buf == out_buf);
  gst_buffer_unref (out_buf);
  g_object_get (switchbin, "current-path", &path_index, NULL);
  fail_unless (path_index == 1);

  while (TRUE) {
    GstMessage *msg = gst_bus_pop (bus);
    if (!msg)
      break;

    GST_DEBUG ("got message %s",
        gst_message_type_get_name (GST_MESSAGE_TYPE (msg)));
    fail_if (GST_MESSAGE_TYPE (msg) == GST_MESSAGE_ERROR);
    gst_message_unref (msg);
  }

  gst_harness_teardown (h);
  gst_bus_set_flushing (bus, TRUE);
  gst_object_unref (bus);
  gst_object_unref (switchbin);
}

GST_END_TEST;

static Suite *
switchbin_suite (void)
{
  Suite *s = suite_create ("switchbin");
  TCase *tc_basic = tcase_create ("general");

  suite_add_tcase (s, tc_basic);
  tcase_add_test (tc_basic, test_switchbin_simple);

  return s;
}

GST_CHECK_MAIN (switchbin);
