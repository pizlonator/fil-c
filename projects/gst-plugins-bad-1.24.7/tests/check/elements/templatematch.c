/*
 * GStreamer
 *
 * unit test for templatematch
 *
 * Copyright (C) 2014 Samsung Electronics. All rights reserved.
 *   Author: Thiago Santos <ts.santos@sisa.samsung.com>
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

#include <gst/check/gstcheck.h>

#define CAPS_TMPL   "video/x-raw, format=(string)BGR"

GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (CAPS_TMPL)
    );
GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (CAPS_TMPL)
    );

/* Create a 16x16 buffer split in 4 equal squares
 * BG
 * Rb
 *
 * B=Blue, G=Green, R=Red, b=black
 */
static GstBuffer *
create_input_buffer (void)
{
  guint8 *data;
  gsize size;
  gint i, j, base;

  size = 3 * 16 * 16;           /* BGR 16x16 */
  data = g_malloc0 (size);

  /* blue and green */
  for (j = 0; j < 8; j++) {
    for (i = 0; i < 8; i++) {
      base = j * 16;
      data[base + i] = 255;
      data[base + i + 1] = 0;
      data[base + i + 2] = 0;

      data[base + 8 + i] = 0;
      data[base + 8 + i + 1] = 255;
      data[base + 8 + i + 2] = 0;
    }
  }
  /* red */
  for (j = 0; j < 8; j++) {
    for (i = 0; i < 8; i++) {
      base = 8 * 8 + j * 16;
      data[base + i] = 0;
      data[base + i + 1] = 0;
      data[base + i + 2] = 255;
    }
  }

  return gst_buffer_new_wrapped (data, size);
}

/* Test to make sure that we are using the same rgb format as opencv expects for
 * template matching (BGR).
 * Will use a blue 8x8 square as the template and as input a 16x16 frame divided
 * evenly in 4 squares: Blue (top-left), Green (top-right), Red (bottom-left) and
 * Black (bottom-right)
 *
 * https://bugzilla.gnome.org/show_bug.cgi?id=678485
 */
GST_START_TEST (test_match_blue_square)
{
  GstElement *element;
  GstPad *sinkpad, *srcpad;
  GstCaps *caps =
      gst_caps_from_string (CAPS_TMPL
      ", width=(int)16, height=(int)16, framerate=1/1");
  GstBus *bus;
  GstMessage *msg;
  const GstStructure *structure;
  gchar *path;
  GstBuffer *buf;
  guint x, y, width, height;

  element = gst_check_setup_element ("templatematch");
  srcpad = gst_check_setup_src_pad (element, &srctemplate);
  sinkpad = gst_check_setup_sink_pad (element, &sinktemplate);
  gst_pad_set_active (srcpad, TRUE);
  gst_check_setup_events (srcpad, element, caps, GST_FORMAT_TIME);
  gst_pad_set_active (sinkpad, TRUE);

  bus = gst_bus_new ();
  gst_element_set_bus (element, bus);

  path = g_build_filename (GST_TEST_FILES_PATH, "blue-square.png", NULL);
  g_object_set (element, "template", path, NULL);
  g_free (path);

  fail_unless (gst_element_set_state (element,
          GST_STATE_PLAYING) != GST_STATE_CHANGE_FAILURE,
      "could not set to playing");

  buf = create_input_buffer ();
  fail_unless (gst_pad_push (srcpad, buf) == GST_FLOW_OK);

  /* make sure that the template match message was posted, detecting the
   * blue area in the top left corner */
  msg = gst_bus_pop_filtered (bus, GST_MESSAGE_ELEMENT);
  fail_unless (msg != NULL);
  fail_unless (GST_MESSAGE_SRC (msg) == GST_OBJECT_CAST (element));
  structure = gst_message_get_structure (msg);
  fail_unless (gst_structure_has_name (structure, "template_match"));
  fail_unless (gst_structure_get_uint (structure, "x", &x));
  fail_unless (gst_structure_get_uint (structure, "y", &y));
  fail_unless (gst_structure_get_uint (structure, "width", &width));
  fail_unless (gst_structure_get_uint (structure, "height", &height));
  fail_unless (x == 0);
  fail_unless (y == 0);
  fail_unless (width == 8);
  fail_unless (height == 8);

  gst_message_unref (msg);

  gst_element_set_state (element, GST_STATE_NULL);
  gst_bus_set_flushing (bus, TRUE);
  gst_object_unref (bus);
  gst_caps_unref (caps);
  gst_check_drop_buffers ();
  gst_pad_set_active (srcpad, FALSE);
  gst_pad_set_active (sinkpad, FALSE);
  gst_object_unref (srcpad);
  gst_object_unref (sinkpad);
  gst_check_teardown_src_pad (element);
  gst_check_teardown_sink_pad (element);
  gst_check_teardown_element (element);
}

GST_END_TEST;

static Suite *
templatematch_suite (void)
{
  Suite *s = suite_create ("templatematch");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_match_blue_square);

  return s;
}

GST_CHECK_MAIN (templatematch);
