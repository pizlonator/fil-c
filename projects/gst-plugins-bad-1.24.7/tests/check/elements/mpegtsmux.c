/* GStreamer
 *
 * Copyright (C) 2011 Alessandro Decina <alessandro.d@gmail.com>
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
#include <string.h>
#include <gst/video/video.h>

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink_%d",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate video_src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-h264")
    );

static GstStaticPadTemplate audio_src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/mpeg")
    );

/* For ease of programming we use globals to keep refs for our floating
 * src and sink pads we create; otherwise we always have to do get_pad,
 * get_peer, and then remove references in every test function */
static GstPad *mysrcpad, *mysinkpad;

#define AUDIO_CAPS_STRING "audio/mpeg, " \
                        "channels = (int) 1, " \
                        "rate = (int) 8000, " \
                        "mpegversion = (int) 1, "\
                        "parsed = (boolean) true "
#define VIDEO_CAPS_STRING "video/x-h264, " \
                          "stream-format = (string) byte-stream, " \
                          "alignment = (string) nal, " \
                          "parsed = (boolean) true "

#define KEYFRAME_DISTANCE 10

typedef void (CheckOutputBuffersFunc) (GList * buffers);

/* setup and teardown needs some special handling for muxer */
static GstPad *
setup_src_pad (GstElement * element,
    GstStaticPadTemplate * template, const gchar * sinkname, gchar ** padname)
{
  GstPad *srcpad, *sinkpad;

  GST_DEBUG_OBJECT (element, "setting up sending pad");
  /* sending pad */
  srcpad = gst_pad_new_from_static_template (template, "src");
  fail_if (srcpad == NULL, "Could not create a srcpad");

  if (!(sinkpad = gst_element_get_static_pad (element, sinkname)))
    sinkpad = gst_element_request_pad_simple (element, sinkname);
  fail_if (sinkpad == NULL, "Could not get sink pad from %s",
      GST_ELEMENT_NAME (element));
  /* we can't test the reference count of the sinkpad here because it's either
   * 2 or 3: 1 by us, 1 by tsmux and potentially another one by the srcpad
   * task of tsmux if it just happens to iterate over the pads */
  fail_unless (gst_pad_link (srcpad, sinkpad) == GST_PAD_LINK_OK,
      "Could not link source and %s sink pads", GST_ELEMENT_NAME (element));

  if (padname)
    *padname = g_strdup (GST_PAD_NAME (sinkpad));

  gst_object_unref (sinkpad);   /* because we got it higher up */

  return srcpad;
}

static void
teardown_src_pad (GstElement * element, const gchar * sinkname)
{
  GstPad *srcpad, *sinkpad;

  /* clean up floating src pad */
  if (!(sinkpad = gst_element_get_static_pad (element, sinkname)))
    sinkpad = gst_element_request_pad_simple (element, sinkname);
  srcpad = gst_pad_get_peer (sinkpad);

  gst_pad_unlink (srcpad, sinkpad);
  GST_DEBUG ("src %p", srcpad);

  gst_object_unref (sinkpad);
  gst_object_unref (srcpad);
  gst_object_unref (srcpad);

}

static GstElement *
setup_tsmux (GstStaticPadTemplate * srctemplate, const gchar * sinkname,
    gchar ** padname)
{
  GstElement *mux;

  GST_DEBUG ("setup_tsmux");
  mux = gst_check_setup_element ("mpegtsmux");
  mysrcpad = setup_src_pad (mux, srctemplate, sinkname, padname);
  mysinkpad = gst_check_setup_sink_pad (mux, &sink_template);
  gst_pad_set_active (mysrcpad, TRUE);
  gst_pad_set_active (mysinkpad, TRUE);

  return mux;
}

static void
cleanup_tsmux (GstElement * mux, const gchar * sinkname)
{
  GST_DEBUG ("cleanup_mux");
  gst_element_set_state (mux, GST_STATE_NULL);

  gst_pad_set_active (mysrcpad, FALSE);
  gst_pad_set_active (mysinkpad, FALSE);
  teardown_src_pad (mux, sinkname);
  gst_check_teardown_sink_pad (mux);
  gst_check_teardown_element (mux);
}

static void
check_tsmux_pad_given_muxer (GstElement * mux,
    const gchar * src_caps_string, gint pes_id, gint pmt_id,
    CheckOutputBuffersFunc check_func, guint n_bufs, gssize input_buf_size)
{
  GstClockTime ts;
  GstBuffer *inbuffer, *outbuffer;
  GstCaps *caps;
  gint num_buffers;
  gint i;
  gint pmt_pid = -1, el_pid = -1, pcr_pid = -1, packets = 0;
  GstQuery *drain;

  caps = gst_caps_from_string (src_caps_string);
  gst_check_setup_events (mysrcpad, mux, caps, GST_FORMAT_TIME);
  gst_caps_unref (caps);

  ts = 0;
  for (i = 0; i < n_bufs; ++i) {
    GstFlowReturn flow;

    if (input_buf_size >= 0)
      inbuffer = gst_buffer_new_and_alloc (input_buf_size);
    else
      inbuffer = gst_buffer_new_and_alloc (g_random_int_range (1, 49141));

    GST_BUFFER_TIMESTAMP (inbuffer) = ts;
    ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);

    if (i % KEYFRAME_DISTANCE == 0 && pes_id == 0xe0) {
      GST_TRACE ("input keyframe");
      GST_BUFFER_FLAG_UNSET (inbuffer, GST_BUFFER_FLAG_DELTA_UNIT);
    } else {
      GST_TRACE ("input delta");
      GST_BUFFER_FLAG_SET (inbuffer, GST_BUFFER_FLAG_DELTA_UNIT);
    }
    flow = gst_pad_push (mysrcpad, inbuffer);
    if (flow != GST_FLOW_OK)
      fail ("Got %s flow instead of OK", gst_flow_get_name (flow));
    ts += 40 * GST_MSECOND;
  }

  drain = gst_query_new_drain ();
  gst_pad_peer_query (mysrcpad, drain);
  gst_query_unref (drain);

  if (check_func)
    check_func (buffers);

  num_buffers = g_list_length (buffers);
  /* all output might get aggregated */
  fail_unless (num_buffers >= 1);

  /* collect buffers in adapter for convenience */
  for (i = 0; i < num_buffers; ++i) {
    guint8 *odata;
    gint size;
    GstMapInfo map;

    outbuffer = GST_BUFFER (buffers->data);
    fail_if (outbuffer == NULL);
    buffers = g_list_remove (buffers, outbuffer);
    ASSERT_BUFFER_REFCOUNT (outbuffer, "outbuffer", 1);

    gst_buffer_map (outbuffer, &map, GST_MAP_READ);
    odata = map.data;
    size = map.size;
    fail_unless (size % 188 == 0);

    for (; size; odata += 188, size -= 188) {
      guint pid, y;
      guint8 *data = odata;

      /* need sync_byte */
      fail_unless (*data == 0x47);
      data++;

      y = GST_READ_UINT16_BE (data);
      pid = y & (0x1FFF);
      data += 2;
      GST_TRACE ("pid: %d", pid);

      y = (y >> 14) & 0x1;
      /* only check packets with payload_start_indicator == 1 */
      if (!y) {
        GST_TRACE ("not at start");
        continue;
      }

      y = *data;
      data++;

      if (y & 0x20) {
        /* adaptation field */
        y = *data;
        data++;
        data += y;
        GST_TRACE ("adaptation %d", y);
      }

      if (pid == 0) {
        /* look for PAT */
        /* pointer field */
        y = *data;
        data++;
        data += y;
        /* table_id */
        y = *data;
        data++;
        fail_unless (y == 0x0);
        /* skip */
        data += 5;
        /* section_number */
        y = *data;
        fail_unless (y == 0);
        data++;
        /* last_section_number */
        y = *data;
        fail_unless (y == 0);
        data++;
        /* program_number */
        y = GST_READ_UINT16_BE (data);
        fail_unless (y != 0);
        data += 2;
        /* program_map_PID */
        y = GST_READ_UINT16_BE (data);
        pmt_pid = y & 0x1FFF;
        fail_unless (pmt_pid > 0x10 && pmt_pid != 0x1FF);
      } else if (pid == pmt_pid) {
        /* look for PMT */
        /* pointer field */
        y = *data;
        data++;
        data += y;
        /* table_id */
        y = *data;
        data++;
        fail_unless (y == 0x2);
        /* skip */
        data += 5;
        /* section_number */
        y = *data;
        fail_unless (y == 0);
        data++;
        /* last_section_number */
        y = *data;
        fail_unless (y == 0);
        data++;
        /* PCR_PID */
        y = GST_READ_UINT16_BE (data);
        data += 2;
        pcr_pid = y & 0x1FFF;
        /* program_info_length */
        y = GST_READ_UINT16_BE (data);
        data += 2;
        y = y & 0x0FFF;
        data += y;
        /* parsing only ES stream */
        /* stream_type */
        y = *data;
        data++;
        fail_unless (y == pmt_id);
        /* elementary_PID */
        y = GST_READ_UINT16_BE (data);
        data += 2;
        el_pid = y & 0x1FFF;
        fail_unless (el_pid > 0x10 && el_pid != 0x1FF);
      } else if (pid == el_pid) {
        packets++;
        /* expect to see a PES packet start */
        y = GST_READ_UINT32_BE (data);
        fail_unless (y >> 8 == 0x1);
        /* stream_id */
        y = y & 0xFF;
        fail_unless ((pes_id & 0xF0) == (y & 0xF0));
      }
    }
    gst_buffer_unmap (outbuffer, &map);
    gst_buffer_unref (outbuffer);
    outbuffer = NULL;
  }

  fail_unless (pmt_pid > 0);
  fail_unless (el_pid > 0);
  fail_unless (pcr_pid == el_pid);
  fail_unless (packets > 0);

  g_list_free (buffers);
  buffers = NULL;
}

static void
check_tsmux_pad (GstStaticPadTemplate * srctemplate,
    const gchar * src_caps_string, gint pes_id, gint pmt_id,
    const gchar * sinkname, CheckOutputBuffersFunc check_func, guint n_bufs,
    gssize input_buf_size, guint alignment)
{
  gchar *padname;
  GstElement *mux;

  mux = setup_tsmux (srctemplate, sinkname, &padname);

  if (alignment != 0)
    g_object_set (mux, "alignment", alignment, NULL);

  fail_unless (gst_element_set_state (mux,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  check_tsmux_pad_given_muxer (mux, src_caps_string, pes_id, pmt_id,
      check_func, n_bufs, input_buf_size);

  cleanup_tsmux (mux, padname);
  g_free (padname);
}

GST_START_TEST (test_reappearing_pad_while_playing)
{
  gchar *padname;
  GstElement *mux;
  GstPad *pad;

  mux = gst_check_setup_element ("mpegtsmux");
  mysrcpad = setup_src_pad (mux, &video_src_template, "sink_%d", &padname);
  mysinkpad = gst_check_setup_sink_pad (mux, &sink_template);
  gst_pad_set_active (mysrcpad, TRUE);
  gst_pad_set_active (mysinkpad, TRUE);

  fail_unless (gst_element_set_state (mux,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  check_tsmux_pad_given_muxer (mux, VIDEO_CAPS_STRING, 0xE0, 0x1b, NULL, 1, 1);

  pad = gst_element_get_static_pad (mux, padname);
  gst_pad_set_active (mysrcpad, FALSE);
  teardown_src_pad (mux, padname);
  gst_element_release_request_pad (mux, pad);
  gst_object_unref (pad);
  g_free (padname);

  mysrcpad = setup_src_pad (mux, &video_src_template, "sink_%d", &padname);
  gst_pad_set_active (mysrcpad, TRUE);

  check_tsmux_pad_given_muxer (mux, VIDEO_CAPS_STRING, 0xE0, 0x1b, NULL, 1, 1);

  cleanup_tsmux (mux, padname);
  g_free (padname);
}

GST_END_TEST;

GST_START_TEST (test_reappearing_pad_while_stopped)
{
  gchar *padname;
  GstElement *mux;
  GstPad *pad;

  mux = gst_check_setup_element ("mpegtsmux");
  mysrcpad = setup_src_pad (mux, &video_src_template, "sink_%d", &padname);
  mysinkpad = gst_check_setup_sink_pad (mux, &sink_template);
  gst_pad_set_active (mysrcpad, TRUE);
  gst_pad_set_active (mysinkpad, TRUE);

  fail_unless (gst_element_set_state (mux,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  check_tsmux_pad_given_muxer (mux, VIDEO_CAPS_STRING, 0xE0, 0x1b, NULL, 1, 1);

  gst_element_set_state (mux, GST_STATE_NULL);

  pad = gst_element_get_static_pad (mux, padname);
  gst_pad_set_active (mysrcpad, FALSE);
  teardown_src_pad (mux, padname);
  gst_element_release_request_pad (mux, pad);
  gst_object_unref (pad);
  g_free (padname);

  mysrcpad = setup_src_pad (mux, &video_src_template, "sink_%d", &padname);
  gst_pad_set_active (mysrcpad, TRUE);

  fail_unless (gst_element_set_state (mux,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  check_tsmux_pad_given_muxer (mux, VIDEO_CAPS_STRING, 0xE0, 0x1b, NULL, 1, 1);

  cleanup_tsmux (mux, padname);
  g_free (padname);
}

GST_END_TEST;

GST_START_TEST (test_unused_pad)
{
  gchar *padname;
  GstElement *mux;
  GstPad *pad;

  mux = gst_check_setup_element ("mpegtsmux");
  mysrcpad = setup_src_pad (mux, &video_src_template, "sink_%d", &padname);
  mysinkpad = gst_check_setup_sink_pad (mux, &sink_template);
  gst_pad_set_active (mysrcpad, TRUE);
  gst_pad_set_active (mysinkpad, TRUE);

  fail_unless (gst_element_set_state (mux,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  pad = gst_element_get_static_pad (mux, padname);
  gst_pad_set_active (mysrcpad, FALSE);
  teardown_src_pad (mux, padname);
  gst_element_release_request_pad (mux, pad);
  gst_object_unref (pad);
  g_free (padname);

  mysrcpad = setup_src_pad (mux, &video_src_template, "sink_%d", &padname);
  gst_pad_set_active (mysrcpad, TRUE);

  cleanup_tsmux (mux, padname);
  g_free (padname);
}

GST_END_TEST;

GST_START_TEST (test_video)
{
  check_tsmux_pad (&video_src_template, VIDEO_CAPS_STRING, 0xE0, 0x1b,
      "sink_%d", NULL, 1, 1, 0);
}

GST_END_TEST;


GST_START_TEST (test_audio)
{
  check_tsmux_pad (&audio_src_template, AUDIO_CAPS_STRING, 0xC0, 0x03,
      "sink_%d", NULL, 1, 1, 0);
}

GST_END_TEST;

GST_START_TEST (test_multiple_state_change)
{
  GstElement *mux;
  gchar *padname;
  GstSegment segment;
  GstCaps *caps;
  size_t i;

  /* it's just a sample of all possible permutations of all states and their
   * transitions */
  GstState states[] = { GST_STATE_PLAYING, GST_STATE_PAUSED, GST_STATE_PLAYING,
    GST_STATE_READY, GST_STATE_PAUSED, GST_STATE_PLAYING, GST_STATE_NULL
  };

  size_t num_transitions_to_test = 10;

  mux = setup_tsmux (&video_src_template, "sink_%d", &padname);
  gst_segment_init (&segment, GST_FORMAT_TIME);

  caps = gst_caps_from_string (VIDEO_CAPS_STRING);
  gst_check_setup_events (mysrcpad, mux, caps, GST_FORMAT_TIME);
  gst_caps_unref (caps);

  for (i = 0; i < num_transitions_to_test; ++i) {
    GstQuery *drain;
    GstState next_state = states[i % G_N_ELEMENTS (states)];
    fail_unless (gst_element_set_state (mux,
            next_state) == GST_STATE_CHANGE_SUCCESS,
        "could not set to %s", gst_element_state_get_name (next_state));

    /* push some buffers when playing - this triggers a lot of activity */
    if (GST_STATE_PLAYING == next_state) {
      GstBuffer *inbuffer;

      fail_unless (gst_pad_push_event (mysrcpad,
              gst_event_new_segment (&segment)));

      inbuffer = gst_buffer_new_and_alloc (1);
      ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);

      GST_BUFFER_PTS (inbuffer) = 0;
      fail_unless (GST_FLOW_OK == gst_pad_push (mysrcpad, inbuffer));

      drain = gst_query_new_drain ();
      gst_pad_peer_query (mysrcpad, drain);
      gst_query_unref (drain);
    }
  }

  cleanup_tsmux (mux, padname);
  g_free (padname);
}

GST_END_TEST;

static void
test_align_check_output (GList * bufs)
{
  GST_LOG ("%u buffers", g_list_length (bufs));
  while (bufs != NULL) {
    GstBuffer *buf = bufs->data;
    gsize size;

    size = gst_buffer_get_size (buf);
    GST_LOG ("buffer, size = %5u", (guint) size);
    fail_unless_equals_int (size, 7 * 188);
    bufs = bufs->next;
  }
}

GST_START_TEST (test_align)
{
  check_tsmux_pad (&video_src_template, VIDEO_CAPS_STRING, 0xE0, 0x1b,
      "sink_%d", test_align_check_output, 817, -1, 7);
}

GST_END_TEST;

static void
test_keyframe_propagation_check_output (GList * bufs)
{
  guint keyframe_count = 0;

  GST_LOG ("%u buffers", g_list_length (bufs));
  while (bufs != NULL) {
    GstBuffer *buf = bufs->data;
    gboolean keyunit;

    keyunit = !GST_BUFFER_FLAG_IS_SET (buf, GST_BUFFER_FLAG_DELTA_UNIT);

    if (keyunit)
      ++keyframe_count;

    GST_LOG ("buffer, keyframe=%d", keyunit);
    bufs = bufs->next;
  }
  fail_unless_equals_int (keyframe_count, 50 / KEYFRAME_DISTANCE);
}

GST_START_TEST (test_keyframe_flag_propagation)
{
  check_tsmux_pad (&video_src_template, VIDEO_CAPS_STRING, 0xE0, 0x1b,
      "sink_%d", test_keyframe_propagation_check_output, 50, -1, 0);
}

GST_END_TEST;

static Suite *
mpegtsmux_suite (void)
{
  Suite *s = suite_create ("mpegtsmux");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);

  tcase_add_test (tc_chain, test_audio);
  tcase_add_test (tc_chain, test_video);
  tcase_add_test (tc_chain, test_multiple_state_change);
  tcase_add_test (tc_chain, test_align);
  tcase_add_test (tc_chain, test_keyframe_flag_propagation);
  tcase_add_test (tc_chain, test_reappearing_pad_while_playing);
  tcase_add_test (tc_chain, test_reappearing_pad_while_stopped);
  tcase_add_test (tc_chain, test_unused_pad);

  return s;
}

GST_CHECK_MAIN (mpegtsmux);
