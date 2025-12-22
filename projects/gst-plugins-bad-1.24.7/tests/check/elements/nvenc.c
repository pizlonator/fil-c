/* GStreamer
 * Copyright (C) 2019 Seungha Yang <seungha.yang@navercorp.com>
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
#include <gst/check/gstharness.h>

static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-h264, "
        "width = (int) [1, MAX], "
        "height = (int) [1, MAX], " "framerate = (fraction) [0, MAX]"));

static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw, "
        "format = (string) NV12, "
        "width = (int) [1, MAX], "
        "height = (int) [1, MAX], " "framerate = (fraction) [0, MAX]"));

static GstPad *sinkpad, *srcpad;

static GstElement *
setup_nvenc (const gchar * src_caps_str, GstCaps ** srccaps)
{
  GstElement *nvenc;
  GstCaps *caps = NULL;
  GstBus *bus;

  caps = gst_caps_from_string (src_caps_str);
  fail_unless (caps != NULL);

  nvenc = gst_check_setup_element ("nvh264enc");
  fail_unless (nvenc != NULL);
  srcpad = gst_check_setup_src_pad (nvenc, &srctemplate);
  sinkpad = gst_check_setup_sink_pad (nvenc, &sinktemplate);
  gst_pad_set_active (srcpad, TRUE);
  gst_pad_set_active (sinkpad, TRUE);

  bus = gst_bus_new ();
  gst_element_set_bus (nvenc, bus);

  *srccaps = caps;

  buffers = NULL;
  return nvenc;
}

static void
cleanup_nvenc (GstElement * nvenc)
{
  GstBus *bus;

  /* Free parsed buffers */
  gst_check_drop_buffers ();

  bus = GST_ELEMENT_BUS (nvenc);
  gst_bus_set_flushing (bus, TRUE);
  gst_object_unref (bus);

  gst_pad_set_active (srcpad, FALSE);
  gst_pad_set_active (sinkpad, FALSE);
  gst_check_teardown_src_pad (nvenc);
  gst_check_teardown_sink_pad (nvenc);
  gst_check_teardown_element (nvenc);
}

GST_START_TEST (test_encode_simple)
{
  GstElement *nvenc;
  GstBuffer *buffer;
  gint i;
  GList *iter;
  GstCaps *outcaps, *sinkcaps, *srccaps;
  GstSegment seg;

  nvenc =
      setup_nvenc
      ("video/x-raw,format=(string)NV12,width=(int)320,height=(int)240,"
      "framerate=(fraction)25/1,interlace-mode=(string)progressive", &srccaps);

  ASSERT_SET_STATE (nvenc, GST_STATE_PLAYING, GST_STATE_CHANGE_SUCCESS);

  gst_segment_init (&seg, GST_FORMAT_TIME);
  seg.stop = gst_util_uint64_scale (10, GST_SECOND, 25);

  gst_check_setup_events (srcpad, nvenc, srccaps, GST_FORMAT_TIME);
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

  /* All buffers must be there now */
  fail_unless_equals_int (g_list_length (buffers), 10);

  outcaps =
      gst_caps_from_string
      ("video/x-h264,width=(int)320,height=(int)240,framerate=(fraction)25/1");

  for (iter = buffers; iter; iter = g_list_next (iter)) {
    buffer = iter->data;

    fail_unless_equals_uint64 (GST_BUFFER_DURATION (buffer),
        gst_util_uint64_scale (1, GST_SECOND, 25));

    sinkcaps = gst_pad_get_current_caps (sinkpad);
    fail_unless (gst_caps_can_intersect (sinkcaps, outcaps));
    gst_caps_unref (sinkcaps);
  }

  gst_caps_unref (outcaps);
  gst_caps_unref (srccaps);

  cleanup_nvenc (nvenc);
}

GST_END_TEST;


GST_START_TEST (test_reuse)
{
  GstElement *nvenc;
  GstBuffer *buffer;
  gint i, loop;
  GList *iter;
  GstCaps *outcaps, *sinkcaps;
  GstSegment seg;
  GstCaps *srccaps;

  nvenc =
      setup_nvenc
      ("video/x-raw,format=(string)NV12,width=(int)320,height=(int)240,"
      "framerate=(fraction)25/1,interlace-mode=(string)progressive", &srccaps);

  for (loop = 0; loop < 2; loop++) {
    ASSERT_SET_STATE (nvenc, GST_STATE_PLAYING, GST_STATE_CHANGE_SUCCESS);

    gst_segment_init (&seg, GST_FORMAT_TIME);
    seg.stop = gst_util_uint64_scale (10, GST_SECOND, 25);

    gst_check_setup_events (srcpad, nvenc, srccaps, GST_FORMAT_TIME);
    fail_unless (gst_pad_push_event (srcpad, gst_event_new_segment (&seg)));

    gst_segment_init (&seg, GST_FORMAT_TIME);
    seg.stop = gst_util_uint64_scale (10, GST_SECOND, 25);

    fail_unless (gst_pad_push_event (srcpad, gst_event_new_segment (&seg)));

    buffer = gst_buffer_new_allocate (NULL, 320 * 240 + 2 * 160 * 120, NULL);
    gst_buffer_memset (buffer, 0, 0, -1);

    for (i = 0; i < 10; i++) {
      GST_BUFFER_TIMESTAMP (buffer) = gst_util_uint64_scale (i, GST_SECOND, 25);
      GST_BUFFER_DURATION (buffer) = gst_util_uint64_scale (1, GST_SECOND, 25);
      fail_unless (gst_pad_push (srcpad,
              gst_buffer_ref (buffer)) == GST_FLOW_OK);
    }

    gst_buffer_unref (buffer);

    fail_unless (gst_pad_push_event (srcpad, gst_event_new_eos ()));

    /* All buffers must be there now */
    fail_unless_equals_int (g_list_length (buffers), 10);

    outcaps =
        gst_caps_from_string
        ("video/x-h264,width=(int)320,height=(int)240,framerate=(fraction)25/1");

    for (iter = buffers; iter; iter = g_list_next (iter)) {
      buffer = iter->data;

      fail_unless_equals_uint64 (GST_BUFFER_DURATION (buffer),
          gst_util_uint64_scale (1, GST_SECOND, 25));

      sinkcaps = gst_pad_get_current_caps (sinkpad);
      fail_unless (gst_caps_can_intersect (sinkcaps, outcaps));
      gst_caps_unref (sinkcaps);
    }
    gst_check_drop_buffers ();
    gst_caps_unref (outcaps);

    ASSERT_SET_STATE (nvenc, GST_STATE_READY, GST_STATE_CHANGE_SUCCESS);
  }

  gst_caps_unref (srccaps);

  cleanup_nvenc (nvenc);
}

GST_END_TEST;

GST_START_TEST (test_caps_interlace_mode)
{
  GstElement *nvenc;
  GstBuffer *buffer;
  GstCaps *caps, *srccaps;
  GstSegment seg;

  nvenc =
      setup_nvenc
      ("video/x-raw,format=(string)NV12,width=(int)320,height=(int)240,"
      "framerate=(fraction)25/1", &srccaps);

  ASSERT_SET_STATE (nvenc, GST_STATE_PLAYING, GST_STATE_CHANGE_SUCCESS);

  gst_check_setup_events (srcpad, nvenc, srccaps, GST_FORMAT_TIME);
  gst_segment_init (&seg, GST_FORMAT_TIME);
  fail_unless (gst_pad_push_event (srcpad, gst_event_new_segment (&seg)));

  buffer = gst_buffer_new_allocate (NULL, 320 * 240 + 2 * 160 * 120, NULL);
  gst_buffer_memset (buffer, 0, 0, -1);

  /* empty interlace-mode */
  GST_BUFFER_TIMESTAMP (buffer) = 0;
  GST_BUFFER_DURATION (buffer) = gst_util_uint64_scale (1, GST_SECOND, 25);
  fail_unless (gst_pad_push (srcpad, gst_buffer_ref (buffer)) == GST_FLOW_OK);

  /* always valid interlace mode */
  caps =
      gst_caps_from_string
      ("video/x-raw,format=(string)NV12,width=(int)320,height=(int)240,"
      "framerate=(fraction)25/1,interlace-mode=(string)progressive");
  fail_unless (gst_pad_push_event (srcpad, gst_event_new_caps (caps)));
  gst_caps_unref (caps);

  GST_BUFFER_TIMESTAMP (buffer) = gst_util_uint64_scale (1, GST_SECOND, 25);
  GST_BUFFER_DURATION (buffer) = gst_util_uint64_scale (1, GST_SECOND, 25);
  fail_unless (gst_pad_push (srcpad, gst_buffer_ref (buffer)) == GST_FLOW_OK);

  /* not-supported interlace mode */
  caps =
      gst_caps_from_string
      ("video/x-raw,format=(string)NV12,width=(int)320,height=(int)240,"
      "framerate=(fraction)25/1,interlace-mode=(string)alternate");
  fail_unless (gst_pad_push_event (srcpad, gst_event_new_caps (caps)));
  gst_caps_unref (caps);

  GST_BUFFER_TIMESTAMP (buffer) = gst_util_uint64_scale (2, GST_SECOND, 25);
  GST_BUFFER_DURATION (buffer) = gst_util_uint64_scale (1, GST_SECOND, 25);
  fail_if (gst_pad_push (srcpad, gst_buffer_ref (buffer)) == GST_FLOW_OK);
  gst_buffer_unref (buffer);
  gst_caps_unref (srccaps);

  cleanup_nvenc (nvenc);
}

GST_END_TEST;

#define MAX_PUSH_BUFFER 64

static void
resolution_change_common (gint from_width, gint from_height, gint to_width,
    gint to_height)
{
  GstHarness *h;
  GstCaps *caps;
  GstEvent *event;
  GstBuffer *in_buf, *out_buf = NULL;
  GstFlowReturn ret;
  gint i = 0;

  h = gst_harness_new_parse ("nvh264enc ! h264parse");
  fail_unless (h != NULL);

  gst_harness_play (h);

  caps = gst_caps_from_string ("video/x-raw,format=NV12");
  gst_caps_set_simple (caps, "width", G_TYPE_INT, from_width,
      "height", G_TYPE_INT, from_height, NULL);
  gst_harness_set_src_caps (h, caps);

  in_buf = gst_buffer_new_and_alloc ((from_width * from_height) * 3 / 2);
  gst_buffer_memset (in_buf, 0, 0, -1);

  GST_BUFFER_DURATION (in_buf) = GST_SECOND;
  GST_BUFFER_DTS (in_buf) = GST_CLOCK_TIME_NONE;

  /* Push buffers until get encoder output */
  do {
    fail_if (i > MAX_PUSH_BUFFER);

    GST_BUFFER_PTS (in_buf) = i * GST_SECOND;

    ret = gst_harness_push (h, gst_buffer_ref (in_buf));
    fail_unless (ret == GST_FLOW_OK, "GstFlowReturn was %s",
        gst_flow_get_name (ret));

    out_buf = gst_harness_try_pull (h);
    i++;
  } while (out_buf == NULL);
  gst_buffer_unref (out_buf);
  gst_buffer_unref (in_buf);

  /* change resolution */
  caps = gst_caps_from_string ("video/x-raw,format=NV12");
  gst_caps_set_simple (caps, "width", G_TYPE_INT, to_width,
      "height", G_TYPE_INT, to_width, NULL);

  GST_DEBUG ("Set new resolution %dx%d", to_width, to_height);
  gst_harness_set_src_caps (h, caps);
  in_buf = gst_buffer_new_and_alloc ((to_width * to_height) * 3 / 2);
  gst_buffer_memset (in_buf, 0, 0, -1);

  GST_BUFFER_PTS (in_buf) = i * GST_SECOND;

  ret = gst_harness_push (h, gst_buffer_ref (in_buf));
  fail_unless (ret == GST_FLOW_OK, "GstFlowReturn was %s",
      gst_flow_get_name (ret));

  /* push EOS to drain all buffers */
  fail_unless (gst_harness_push_event (h, gst_event_new_eos ()));

  do {
    gboolean term = FALSE;
    event = gst_harness_pull_event (h);
    /* wait until get EOS event */
    if (event) {
      if (GST_EVENT_TYPE (event) == GST_EVENT_EOS)
        term = TRUE;

      gst_event_unref (event);

      if (term)
        break;
    }
  } while (out_buf == NULL);

  /* check the last resolution from caps */
  caps = gst_pad_get_current_caps (h->sinkpad);
  fail_unless (caps != NULL);

  GST_DEBUG ("last encoder src caps %" GST_PTR_FORMAT, caps);
  {
    gint val;
    GstStructure *s;

    s = gst_caps_get_structure (caps, 0);
    fail_unless_equals_string (gst_structure_get_name (s), "video/x-h264");
    fail_unless (gst_structure_get_int (s, "width", &val));
    fail_unless_equals_int (val, to_width);
    fail_unless (gst_structure_get_int (s, "height", &val));
    fail_unless_equals_int (val, to_height);
  }
  gst_caps_unref (caps);

  gst_harness_teardown (h);
}

GST_START_TEST (test_resolution_change_to_larger)
{
  resolution_change_common (64, 64, 128, 128);
}

GST_END_TEST;

GST_START_TEST (test_resolution_change_to_smaller)
{
  resolution_change_common (128, 128, 64, 64);
}

GST_END_TEST;

static gboolean
check_nvenc_available (void)
{
  gboolean ret = TRUE;
  GstElement *nvenc;

  nvenc = gst_element_factory_make ("nvh264enc", NULL);
  if (!nvenc) {
    GST_WARNING ("nvh264enc is not available, possibly driver load failure");
    return FALSE;
  }

  /* GST_STATE_READY is meaning that driver could be loaded */
  if (gst_element_set_state (nvenc,
          GST_STATE_PAUSED) != GST_STATE_CHANGE_SUCCESS) {
    GST_WARNING ("cannot open device");
    ret = FALSE;
  }

  gst_element_set_state (nvenc, GST_STATE_NULL);
  gst_object_unref (nvenc);

  return ret;
}

static Suite *
nvenc_suite (void)
{
  Suite *s;
  TCase *tc_chain;

  /* HACK: cuda device init/deinit with fork seems to problematic */
  g_setenv ("CK_FORK", "no", TRUE);

  s = suite_create ("nvenc");
  tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);

  if (!check_nvenc_available ()) {
    GST_DEBUG ("Skip nvenc test since cannot open device");
    goto end;
  }

  tcase_add_test (tc_chain, test_encode_simple);
  tcase_add_test (tc_chain, test_reuse);
  tcase_add_test (tc_chain, test_caps_interlace_mode);
  tcase_add_test (tc_chain, test_resolution_change_to_larger);
  tcase_add_test (tc_chain, test_resolution_change_to_smaller);

end:
  return s;
}

GST_CHECK_MAIN (nvenc);
