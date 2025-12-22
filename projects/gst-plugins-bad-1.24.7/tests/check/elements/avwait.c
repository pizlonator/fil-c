/* GStreamer unit test for avwait
 *
 * Copyright (C) 2018 Vivia Nikolaidou <vivia@toolsonair.com>
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
# include <config.h>
#endif

#include <gst/check/gstcheck.h>
#include <gst/audio/audio.h>
#include <gst/video/video.h>

typedef enum _SwitchType
{
  DO_NOT_SWITCH = -1,
  SWITCH_FALSE = 0,
  SWITCH_TRUE = 1
} SwitchType;

static guint audio_buffer_count, video_buffer_count;
static SwitchType switch_after_2s;
static GstVideoTimeCode *target_tc;
static GstVideoTimeCode *end_tc;
static GstClockTime target_running_time;
static gboolean recording;
static gint mode;
static gboolean audio_late;

static GstAudioInfo ainfo;

static guint n_abuffers, n_vbuffers;
static GstClockTime first_audio_timestamp, last_audio_timestamp;
static GstClockTime first_video_timestamp, last_video_timestamp;

typedef struct _ElementPadAndSwitchType
{
  GstElement *element;
  GstPad *pad;
  SwitchType switch_after_2s;
} ElementPadAndSwitchType;

typedef struct _PadAndBoolean
{
  GstPad *pad;
  gboolean b;
} PadAndBoolean;

static void
set_default_params (void)
{
  n_abuffers = 16;
  n_vbuffers = 160;
  switch_after_2s = DO_NOT_SWITCH;
  target_tc = NULL;
  end_tc = NULL;
  target_running_time = GST_CLOCK_TIME_NONE;
  recording = TRUE;
  mode = 2;
  audio_late = FALSE;

  first_audio_timestamp = GST_CLOCK_TIME_NONE;
  last_audio_timestamp = GST_CLOCK_TIME_NONE;
  first_video_timestamp = GST_CLOCK_TIME_NONE;
  last_video_timestamp = GST_CLOCK_TIME_NONE;
};

static GstFlowReturn
output_achain (GstPad * pad, GstObject * parent, GstBuffer * buffer)
{
  GstClockTime timestamp, duration;

  timestamp = GST_BUFFER_TIMESTAMP (buffer);

  duration =
      gst_util_uint64_scale (gst_buffer_get_size (buffer) / ainfo.bpf,
      GST_SECOND, ainfo.rate);

  if (first_audio_timestamp == GST_CLOCK_TIME_NONE)
    first_audio_timestamp = timestamp;

  last_audio_timestamp = timestamp + duration;

  audio_buffer_count++;
  gst_buffer_unref (buffer);
  return GST_FLOW_OK;
}

static GstFlowReturn
output_vchain (GstPad * pad, GstObject * parent, GstBuffer * buffer)
{
  GstClockTime timestamp;

  timestamp = GST_BUFFER_TIMESTAMP (buffer);

  if (first_video_timestamp == GST_CLOCK_TIME_NONE)
    first_video_timestamp = timestamp;

  last_video_timestamp = timestamp + GST_BUFFER_DURATION (buffer);

  video_buffer_count++;
  gst_buffer_unref (buffer);
  return GST_FLOW_OK;
}

static gpointer
push_abuffers (gpointer data)
{
  GstSegment segment;
  gint i;
  GstCaps *caps;
  guint buf_size = 1000;
  guint channels = 2;
  PadAndBoolean *e = data;
  GstPad *pad = e->pad;
  gboolean audio_late = e->b;
  GstClockTime timestamp;

  if (audio_late) {
    timestamp = 50 * GST_MSECOND;
  } else {
    timestamp = 0;
  }

  gst_pad_send_event (pad, gst_event_new_stream_start ("test"));

  gst_audio_info_set_format (&ainfo, GST_AUDIO_FORMAT_S8, buf_size, channels,
      NULL);
  caps = gst_audio_info_to_caps (&ainfo);
  gst_pad_send_event (pad, gst_event_new_caps (caps));
  gst_caps_unref (caps);

  gst_segment_init (&segment, GST_FORMAT_TIME);
  gst_pad_send_event (pad, gst_event_new_segment (&segment));

  for (i = 0; i < n_abuffers; i++) {
    GstBuffer *buf = gst_buffer_new_and_alloc (channels * buf_size);

    gst_buffer_memset (buf, 0, 0, channels * buf_size);

    GST_BUFFER_TIMESTAMP (buf) = timestamp;
    timestamp += 1 * GST_SECOND;
    GST_BUFFER_DURATION (buf) = timestamp - GST_BUFFER_TIMESTAMP (buf);

    fail_unless (gst_pad_chain (pad, buf) == GST_FLOW_OK);
  }
  gst_pad_send_event (pad, gst_event_new_eos ());

  return NULL;
}

static gpointer
push_vbuffers (gpointer data)
{
  GstSegment segment;
  ElementPadAndSwitchType *e = data;
  GstPad *pad = e->pad;
  gint i;
  GstClockTime timestamp = 0;
  GstVideoTimeCode *tc;

  gst_pad_send_event (pad, gst_event_new_stream_start ("test"));
  gst_segment_init (&segment, GST_FORMAT_TIME);
  gst_pad_send_event (pad, gst_event_new_segment (&segment));
  tc = gst_video_time_code_new (40, 1, NULL, GST_VIDEO_TIME_CODE_FLAGS_NONE, 0,
      0, 0, 0, 0);

  for (i = 0; i < n_vbuffers; i++) {
    GstBuffer *buf = gst_buffer_new_and_alloc (1000);

    gst_buffer_memset (buf, 0, i, 1);

    GST_BUFFER_TIMESTAMP (buf) = timestamp;
    timestamp += 25 * GST_MSECOND;
    GST_BUFFER_DURATION (buf) = timestamp - GST_BUFFER_TIMESTAMP (buf);
    gst_buffer_add_video_time_code_meta (buf, tc);
    gst_video_time_code_increment_frame (tc);

    fail_unless (gst_pad_chain (pad, buf) == GST_FLOW_OK);

    if (timestamp == 2 * GST_SECOND && e->switch_after_2s != DO_NOT_SWITCH) {
      g_object_set (e->element, "recording", !!e->switch_after_2s, NULL);
    }
  }
  gst_pad_send_event (pad, gst_event_new_eos ());
  gst_video_time_code_free (tc);

  return NULL;
}

static void
test_avwait_generic (void)
{
  GstElement *avwait;
  GstPad *asink, *vsink, *asrc, *vsrc, *aoutput_sink, *voutput_sink;
  GThread *athread, *vthread;
  GstBus *bus;
  ElementPadAndSwitchType *e;
  PadAndBoolean *pb;

  audio_buffer_count = 0;
  video_buffer_count = 0;

  avwait = gst_element_factory_make ("avwait", NULL);
  fail_unless (avwait != NULL);
  g_object_set (avwait, "mode", mode,
      "target-running-time", target_running_time, "recording", recording, NULL);
  if (target_tc != NULL)
    g_object_set (avwait, "target-timecode", target_tc, NULL);
  if (end_tc != NULL)
    g_object_set (avwait, "end-timecode", end_tc, NULL);

  bus = gst_bus_new ();
  gst_element_set_bus (avwait, bus);

  asink = gst_element_get_static_pad (avwait, "asink");
  fail_unless (asink != NULL);

  vsink = gst_element_get_static_pad (avwait, "vsink");
  fail_unless (vsink != NULL);

  asrc = gst_element_get_static_pad (avwait, "asrc");
  aoutput_sink = gst_pad_new ("sink", GST_PAD_SINK);
  fail_unless (aoutput_sink != NULL);
  fail_unless (gst_pad_link (asrc, aoutput_sink) == GST_PAD_LINK_OK);

  vsrc = gst_element_get_static_pad (avwait, "vsrc");
  voutput_sink = gst_pad_new ("sink", GST_PAD_SINK);
  fail_unless (voutput_sink != NULL);
  fail_unless (gst_pad_link (vsrc, voutput_sink) == GST_PAD_LINK_OK);

  gst_pad_set_chain_function (aoutput_sink, output_achain);

  gst_pad_set_chain_function (voutput_sink, output_vchain);

  gst_pad_set_active (aoutput_sink, TRUE);
  gst_pad_set_active (voutput_sink, TRUE);
  fail_unless (gst_element_set_state (avwait,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS);
  e = g_new0 (ElementPadAndSwitchType, 1);
  e->element = avwait;
  e->pad = vsink;
  e->switch_after_2s = switch_after_2s;
  pb = g_new0 (PadAndBoolean, 1);
  pb->pad = asink;
  pb->b = audio_late;

  athread = g_thread_new ("athread", (GThreadFunc) push_abuffers, pb);
  vthread = g_thread_new ("vthread", (GThreadFunc) push_vbuffers, e);

  g_thread_join (vthread);
  g_thread_join (athread);

  /* teardown */
  gst_element_set_state (avwait, GST_STATE_NULL);
  gst_bus_set_flushing (bus, TRUE);
  gst_object_unref (bus);
  g_free (e);
  g_free (pb);
  gst_pad_unlink (asrc, aoutput_sink);
  gst_object_unref (asrc);
  gst_pad_unlink (vsrc, voutput_sink);
  gst_object_unref (vsrc);
  gst_object_unref (asink);
  gst_object_unref (vsink);
  gst_pad_set_active (aoutput_sink, FALSE);
  gst_object_unref (aoutput_sink);
  gst_pad_set_active (voutput_sink, FALSE);
  gst_object_unref (voutput_sink);
  gst_object_unref (avwait);
}

GST_START_TEST (test_avwait_switch_to_true)
{
  set_default_params ();
  recording = FALSE;
  switch_after_2s = SWITCH_TRUE;
  test_avwait_generic ();
  fail_unless_equals_uint64 (first_audio_timestamp, 2 * GST_SECOND);
  fail_unless_equals_uint64 (first_video_timestamp, 2 * GST_SECOND);
}

GST_END_TEST;

GST_START_TEST (test_avwait_switch_to_false)
{
  set_default_params ();
  recording = TRUE;
  switch_after_2s = SWITCH_FALSE;
  test_avwait_generic ();
  fail_unless_equals_uint64 (first_audio_timestamp, first_video_timestamp);
  fail_unless_equals_uint64 (first_video_timestamp, 0);
  fail_unless_equals_uint64 (last_video_timestamp, 2 * GST_SECOND);
  fail_unless_equals_uint64 (last_audio_timestamp, 2 * GST_SECOND);
}

GST_END_TEST;

GST_START_TEST (test_avwait_1s_switch_to_true)
{
  set_default_params ();
  recording = FALSE;
  switch_after_2s = SWITCH_TRUE;
  mode = 1;
  target_running_time = 1 * GST_SECOND;
  test_avwait_generic ();
  fail_unless_equals_uint64 (first_audio_timestamp, 2 * GST_SECOND);
  fail_unless_equals_uint64 (first_video_timestamp, 2 * GST_SECOND);
}

GST_END_TEST;

GST_START_TEST (test_avwait_1s_switch_to_false)
{
  set_default_params ();
  recording = TRUE;
  switch_after_2s = SWITCH_FALSE;
  mode = 1;
  target_running_time = 1 * GST_SECOND;
  test_avwait_generic ();
  fail_unless_equals_uint64 (first_audio_timestamp, 1 * GST_SECOND);
  fail_unless_equals_uint64 (first_video_timestamp, 1 * GST_SECOND);
  fail_unless_equals_uint64 (last_video_timestamp, 2 * GST_SECOND);
  fail_unless_equals_uint64 (last_audio_timestamp, 2 * GST_SECOND);
}

GST_END_TEST;

GST_START_TEST (test_avwait_3s_switch_to_true)
{
  set_default_params ();
  recording = FALSE;
  switch_after_2s = SWITCH_TRUE;
  mode = 1;
  target_running_time = 3 * GST_SECOND;
  test_avwait_generic ();
  fail_unless_equals_uint64 (first_audio_timestamp, 3 * GST_SECOND);
  fail_unless_equals_uint64 (first_video_timestamp, 3 * GST_SECOND);
}

GST_END_TEST;

GST_START_TEST (test_avwait_3s_switch_to_false)
{
  set_default_params ();
  recording = TRUE;
  switch_after_2s = SWITCH_FALSE;
  mode = 1;
  target_running_time = 3 * GST_SECOND;
  test_avwait_generic ();
  fail_unless_equals_uint64 (first_audio_timestamp, GST_CLOCK_TIME_NONE);
  fail_unless_equals_uint64 (first_video_timestamp, GST_CLOCK_TIME_NONE);
  fail_unless_equals_uint64 (last_audio_timestamp, GST_CLOCK_TIME_NONE);
  fail_unless_equals_uint64 (last_video_timestamp, GST_CLOCK_TIME_NONE);
}

GST_END_TEST;

GST_START_TEST (test_avwait_1stc_switch_to_true)
{
  set_default_params ();
  recording = FALSE;
  switch_after_2s = SWITCH_TRUE;
  mode = 0;
  target_tc =
      gst_video_time_code_new (40, 1, NULL, GST_VIDEO_TIME_CODE_FLAGS_NONE, 0,
      0, 1, 0, 0);
  end_tc =
      gst_video_time_code_new (40, 1, NULL, GST_VIDEO_TIME_CODE_FLAGS_NONE, 0,
      0, 3, 0, 0);
  test_avwait_generic ();
  fail_unless_equals_uint64 (first_audio_timestamp, 2 * GST_SECOND);
  fail_unless_equals_uint64 (first_video_timestamp, 2 * GST_SECOND);
  fail_unless_equals_uint64 (last_video_timestamp, 3 * GST_SECOND);
  fail_unless_equals_uint64 (last_audio_timestamp, 3 * GST_SECOND);
}

GST_END_TEST;

GST_START_TEST (test_avwait_1stc_switch_to_false)
{
  set_default_params ();
  recording = TRUE;
  switch_after_2s = SWITCH_FALSE;
  mode = 0;
  target_tc =
      gst_video_time_code_new (40, 1, NULL, GST_VIDEO_TIME_CODE_FLAGS_NONE, 0,
      0, 1, 0, 0);
  end_tc =
      gst_video_time_code_new (40, 1, NULL, GST_VIDEO_TIME_CODE_FLAGS_NONE, 0,
      0, 3, 0, 0);
  test_avwait_generic ();
  fail_unless_equals_uint64 (first_audio_timestamp, 1 * GST_SECOND);
  fail_unless_equals_uint64 (first_video_timestamp, 1 * GST_SECOND);
  fail_unless_equals_uint64 (last_video_timestamp, 2 * GST_SECOND);
  fail_unless_equals_uint64 (last_audio_timestamp, 2 * GST_SECOND);
}

GST_END_TEST;

GST_START_TEST (test_avwait_3stc_switch_to_true)
{
  set_default_params ();
  recording = FALSE;
  switch_after_2s = SWITCH_TRUE;
  mode = 0;
  target_tc =
      gst_video_time_code_new (40, 1, NULL, GST_VIDEO_TIME_CODE_FLAGS_NONE, 0,
      0, 3, 0, 0);
  test_avwait_generic ();
  fail_unless_equals_uint64 (first_audio_timestamp, 3 * GST_SECOND);
  fail_unless_equals_uint64 (first_video_timestamp, 3 * GST_SECOND);
}

GST_END_TEST;

GST_START_TEST (test_avwait_3stc_switch_to_false)
{
  set_default_params ();
  recording = TRUE;
  switch_after_2s = SWITCH_FALSE;
  mode = 0;
  target_tc =
      gst_video_time_code_new (40, 1, NULL, GST_VIDEO_TIME_CODE_FLAGS_NONE, 0,
      0, 3, 0, 0);
  test_avwait_generic ();
  fail_unless_equals_uint64 (first_audio_timestamp, GST_CLOCK_TIME_NONE);
  fail_unless_equals_uint64 (first_video_timestamp, GST_CLOCK_TIME_NONE);
  fail_unless_equals_uint64 (last_audio_timestamp, GST_CLOCK_TIME_NONE);
  fail_unless_equals_uint64 (last_video_timestamp, GST_CLOCK_TIME_NONE);
}

GST_END_TEST;

GST_START_TEST (test_avwait_audio_late)
{
  set_default_params ();
  recording = TRUE;
  audio_late = TRUE;
  test_avwait_generic ();
  fail_unless_equals_uint64 (first_audio_timestamp, 50 * GST_MSECOND);
  fail_unless_equals_uint64 (first_video_timestamp, 50 * GST_MSECOND);
}

GST_END_TEST;

static Suite *
avwait_suite (void)
{
  Suite *s = suite_create ("avwait");
  TCase *tc_chain;

  tc_chain = tcase_create ("avwait");
  tcase_add_test (tc_chain, test_avwait_switch_to_true);
  tcase_add_test (tc_chain, test_avwait_switch_to_false);
  tcase_add_test (tc_chain, test_avwait_1s_switch_to_true);
  tcase_add_test (tc_chain, test_avwait_1s_switch_to_false);
  tcase_add_test (tc_chain, test_avwait_3s_switch_to_true);
  tcase_add_test (tc_chain, test_avwait_3s_switch_to_false);
  tcase_add_test (tc_chain, test_avwait_1stc_switch_to_true);
  tcase_add_test (tc_chain, test_avwait_1stc_switch_to_false);
  tcase_add_test (tc_chain, test_avwait_3stc_switch_to_true);
  tcase_add_test (tc_chain, test_avwait_3stc_switch_to_false);
  tcase_add_test (tc_chain, test_avwait_audio_late);
  suite_add_tcase (s, tc_chain);

  return s;
}

GST_CHECK_MAIN (avwait);
