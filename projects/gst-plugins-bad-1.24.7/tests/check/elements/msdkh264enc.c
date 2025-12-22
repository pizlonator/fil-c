/* GStreamer
 *
 * Unit test for msdkh264enc
 *
 * Copyright (c) 2018 Wang,Fei <fei.w.wang@intel.com>
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

static GstStaticPadTemplate h264enc_sinktemp = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-h264, "
        "width = (int) [1, MAX], "
        "height = (int) [1, MAX], " "framerate = (fraction) [0, MAX]"));

static GstStaticPadTemplate h264enc_srctemp = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw, "
        "format = (string) NV12, "
        "width = (int) [1, MAX], "
        "height = (int) [1, MAX], " "framerate = (fraction) [0, MAX]"));


static GstPad *sinkpad, *srcpad;

static GstElement *
setup_element (const gchar * caps)
{
  GstElement *element = NULL;
  GstCaps *srccaps = NULL;
  GstBus *bus = NULL;

  element = gst_check_setup_element ("msdkh264enc");
  if (caps) {
    srccaps = gst_caps_from_string (caps);
    fail_unless (srccaps != NULL);
  }
  srcpad = gst_check_setup_src_pad (element, &h264enc_srctemp);
  sinkpad = gst_check_setup_sink_pad (element, &h264enc_sinktemp);
  gst_pad_set_active (srcpad, TRUE);
  gst_pad_set_active (sinkpad, TRUE);
  gst_check_setup_events (srcpad, element, srccaps, GST_FORMAT_TIME);

  bus = gst_bus_new ();
  gst_element_set_bus (element, bus);

  fail_unless (gst_element_set_state (element,
          GST_STATE_PLAYING) != GST_STATE_CHANGE_FAILURE,
      "could not set to playing");

  gst_caps_unref (srccaps);

  buffers = NULL;
  return element;
}

static void
cleanup_element (GstElement * element)
{
  GstBus *bus;

  /* Free parsed buffers */
  gst_check_drop_buffers ();

  bus = GST_ELEMENT_BUS (element);
  gst_bus_set_flushing (bus, TRUE);
  gst_object_unref (bus);

  gst_pad_set_active (srcpad, FALSE);
  gst_pad_set_active (sinkpad, FALSE);
  gst_check_teardown_src_pad (element);
  gst_check_teardown_sink_pad (element);
  gst_check_teardown_element (element);
}


GST_START_TEST (msdk_h264enc)
{
  GstElement *msdkh264enc;
  GstBuffer *buffer;
  gint i;
  GList *l;
  GstCaps *outcaps, *sinkcaps;
  GstSegment seg;

  msdkh264enc =
      setup_element
      ("video/x-raw,format=(string)NV12,width=(int)320,height=(int)240,framerate=(fraction)25/1,interlace-mode=(string)progressive");

  gst_segment_init (&seg, GST_FORMAT_TIME);
  seg.stop = gst_util_uint64_scale (10, GST_SECOND, 25);
  fail_unless (gst_pad_push_event (srcpad, gst_event_new_segment (&seg)));

  buffer = gst_buffer_new_allocate (NULL, 320 * 240 + 2 * 160 * 120, NULL);
  gst_buffer_memset (buffer, 0, 0, -1);

  for (i = 0; i < 10; i++) {
    GST_BUFFER_TIMESTAMP (buffer) = gst_util_uint64_scale (i, GST_SECOND, 25);
    GST_BUFFER_DURATION (buffer) = gst_util_uint64_scale (1, GST_SECOND, 25);
    fail_unless (gst_pad_push (srcpad, gst_buffer_ref (buffer)) == GST_FLOW_OK);
  }

  gst_buffer_unref (buffer);

  fail_unless (gst_pad_push_event (srcpad, gst_event_new_eos ()));

  fail_unless_equals_int (g_list_length (buffers), 10);

  outcaps =
      gst_caps_from_string
      ("video/x-h264,width=(int)320,height=(int)240,framerate=(fraction)25/1");

  for (l = buffers, i = 0; l; l = l->next, i++) {
    buffer = l->data;

    fail_unless_equals_uint64 (GST_BUFFER_DURATION (buffer),
        gst_util_uint64_scale (1, GST_SECOND, 25));

    sinkcaps = gst_pad_get_current_caps (sinkpad);
    fail_unless (gst_caps_can_intersect (sinkcaps, outcaps));
    gst_caps_unref (sinkcaps);
  }

  gst_caps_unref (outcaps);
  cleanup_element (msdkh264enc);
}

GST_END_TEST;

static Suite *
msdkh264enc_suite (void)
{
  Suite *s = suite_create ("msdkh264enc");
  TCase *tc_chain = tcase_create ("general");
  GstElementFactory *factory;

  suite_add_tcase (s, tc_chain);

  factory = gst_element_factory_find ("msdkh264enc");
  if (factory) {
    tcase_add_test (tc_chain, msdk_h264enc);
    gst_object_unref (factory);
  }

  return s;
}

GST_CHECK_MAIN (msdkh264enc);
