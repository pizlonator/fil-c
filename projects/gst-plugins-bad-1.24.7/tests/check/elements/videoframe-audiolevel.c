/* GStreamer unit test for videoframe-audiolevel
 *
 * Copyright (C) 2015 Vivia Nikolaidou <vivia@toolsonair.com>
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

/* suppress warnings for deprecated API such as GValueArray
 * with newer GLib versions (>= 2.31.0) */
#define GLIB_DISABLE_DEPRECATION_WARNINGS

#include <gst/check/gstcheck.h>
#include <gst/audio/audio.h>

static gboolean got_eos;
static guint audio_buffer_count, video_buffer_count;
static GstSegment current_audio_segment, current_video_segment;
static guint num_msgs;
static GQueue v_timestamp_q, msg_timestamp_q;

static guint n_abuffers, n_vbuffers;
static guint channels, fill_value;
static gdouble expected_rms;
static gboolean audiodelay, videodelay, per_channel, long_video;
static gboolean early_video, late_video;
static gboolean video_gaps, video_overlaps;
static gboolean audio_nondiscont, audio_drift;

static guint fill_value_per_channel[] = { 0, 1 };
static gdouble expected_rms_per_channel[] = { 0, 0.0078125 };

static void
set_default_params (void)
{
  n_abuffers = 40;
  n_vbuffers = 15;
  channels = 2;
  expected_rms = 0.0078125;
  fill_value = 1;
  audiodelay = FALSE;
  videodelay = FALSE;
  per_channel = FALSE;
  long_video = FALSE;
  video_gaps = FALSE;
  video_overlaps = FALSE;
  audio_nondiscont = FALSE;
  audio_drift = FALSE;
  early_video = FALSE;
  late_video = FALSE;
};

static GstFlowReturn
output_achain (GstPad * pad, GstObject * parent, GstBuffer * buffer)
{
  GstClockTime timestamp;
  guint8 b;
  gboolean audio_jitter = audio_nondiscont || audio_drift || early_video;

  timestamp = GST_BUFFER_TIMESTAMP (buffer);
  if (!audio_jitter)
    fail_unless_equals_int64 (timestamp,
        (audio_buffer_count % n_abuffers) * 1 * GST_SECOND);
  timestamp =
      gst_segment_to_stream_time (&current_audio_segment, GST_FORMAT_TIME,
      timestamp);
  if (!audio_jitter)
    fail_unless_equals_int64 (timestamp,
        (audio_buffer_count % n_abuffers) * 1 * GST_SECOND);

  timestamp = GST_BUFFER_TIMESTAMP (buffer);
  timestamp =
      gst_segment_to_running_time (&current_audio_segment, GST_FORMAT_TIME,
      timestamp);
  if (!audio_jitter)
    fail_unless_equals_int64 (timestamp, audio_buffer_count * 1 * GST_SECOND);

  gst_buffer_extract (buffer, 0, &b, 1);

  if (per_channel) {
    fail_unless_equals_int (b, fill_value_per_channel[0]);
  } else {
    fail_unless_equals_int (b, fill_value);
  }

  audio_buffer_count++;
  gst_buffer_unref (buffer);
  return GST_FLOW_OK;
}

static gboolean
output_aevent (GstPad * pad, GstObject * parent, GstEvent * event)
{
  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_FLUSH_STOP:
      gst_segment_init (&current_audio_segment, GST_FORMAT_UNDEFINED);
      break;
    case GST_EVENT_SEGMENT:
      gst_event_copy_segment (event, &current_audio_segment);
      break;
    case GST_EVENT_EOS:
      got_eos = TRUE;
      break;
    default:
      break;
  }

  gst_event_unref (event);
  return TRUE;
}

static GstFlowReturn
output_vchain (GstPad * pad, GstObject * parent, GstBuffer * buffer)
{
  GstClockTime timestamp;
  guint8 b;
  gboolean jitter = video_gaps || video_overlaps || late_video;

  timestamp = GST_BUFFER_TIMESTAMP (buffer);
  if (!jitter)
    fail_unless_equals_int64 (timestamp,
        (video_buffer_count % n_vbuffers) * 25 * GST_MSECOND);
  timestamp =
      gst_segment_to_stream_time (&current_video_segment, GST_FORMAT_TIME,
      timestamp);
  if (!jitter)
    fail_unless_equals_int64 (timestamp,
        (video_buffer_count % n_vbuffers) * 25 * GST_MSECOND);

  timestamp = GST_BUFFER_TIMESTAMP (buffer);
  timestamp =
      gst_segment_to_running_time (&current_video_segment, GST_FORMAT_TIME,
      timestamp);
  if (!jitter)
    fail_unless_equals_int64 (timestamp, video_buffer_count * 25 * GST_MSECOND);

  gst_buffer_extract (buffer, 0, &b, 1);
  if (!jitter)
    fail_unless_equals_int (b, video_buffer_count % n_vbuffers);

  video_buffer_count++;
  gst_buffer_unref (buffer);
  return GST_FLOW_OK;
}

static gboolean
output_vevent (GstPad * pad, GstObject * parent, GstEvent * event)
{
  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_FLUSH_STOP:
      gst_segment_init (&current_video_segment, GST_FORMAT_UNDEFINED);
      break;
    case GST_EVENT_SEGMENT:
      gst_event_copy_segment (event, &current_video_segment);
      break;
    case GST_EVENT_EOS:
      got_eos = TRUE;
      break;
    default:
      break;
  }

  gst_event_unref (event);
  return TRUE;
}

static gpointer
push_abuffers (gpointer data)
{
  GstSegment segment;
  GstPad *pad = data;
  gint i, j, k;
  GstClockTime timestamp = 0;
  GstAudioInfo info;
  GstCaps *caps;
  guint buf_size = 1000;

  if (audiodelay)
    g_usleep (2000);

  if (early_video)
    timestamp = 50 * GST_MSECOND;

  gst_pad_send_event (pad, gst_event_new_stream_start ("test"));

  gst_audio_info_set_format (&info, GST_AUDIO_FORMAT_S8, buf_size, channels,
      NULL);
  caps = gst_audio_info_to_caps (&info);
  gst_pad_send_event (pad, gst_event_new_caps (caps));
  gst_caps_unref (caps);

  gst_segment_init (&segment, GST_FORMAT_TIME);
  gst_pad_send_event (pad, gst_event_new_segment (&segment));

  for (i = 0; i < n_abuffers; i++) {
    GstBuffer *buf = gst_buffer_new_and_alloc (channels * buf_size);

    if (per_channel) {
      GstMapInfo map;
      guint8 *in_data;

      gst_buffer_map (buf, &map, GST_MAP_WRITE);
      in_data = map.data;

      for (j = 0; j < buf_size; j++) {
        for (k = 0; k < channels; k++) {
          in_data[j * channels + k] = fill_value_per_channel[k];
        }
      }

      gst_buffer_unmap (buf, &map);
    } else {
      gst_buffer_memset (buf, 0, fill_value, channels * buf_size);
    }

    GST_BUFFER_TIMESTAMP (buf) = timestamp;
    timestamp += 1 * GST_SECOND;
    if (audio_drift)
      timestamp += 50 * GST_MSECOND;
    else if (i == 4 && audio_nondiscont)
      timestamp += 30 * GST_MSECOND;
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
  GstPad *pad = data;
  gint i;
  GstClockTime timestamp = 0;

  if (videodelay)
    g_usleep (2000);

  if (late_video)
    timestamp = 50 * GST_MSECOND;

  gst_pad_send_event (pad, gst_event_new_stream_start ("test"));
  gst_segment_init (&segment, GST_FORMAT_TIME);
  gst_pad_send_event (pad, gst_event_new_segment (&segment));

  for (i = 0; i < n_vbuffers; i++) {
    GstBuffer *buf = gst_buffer_new_and_alloc (1000);
    GstClockTime *rtime = g_new (GstClockTime, 1);

    gst_buffer_memset (buf, 0, i, 1);

    GST_BUFFER_TIMESTAMP (buf) = timestamp;
    timestamp += 25 * GST_MSECOND;
    GST_BUFFER_DURATION (buf) = timestamp - GST_BUFFER_TIMESTAMP (buf);
    *rtime = gst_segment_to_running_time (&segment, GST_FORMAT_TIME, timestamp);
    g_queue_push_tail (&v_timestamp_q, rtime);

    if (i == 4) {
      if (video_gaps)
        timestamp += 10 * GST_MSECOND;
      else if (video_overlaps)
        timestamp -= 10 * GST_MSECOND;
    }

    fail_unless (gst_pad_chain (pad, buf) == GST_FLOW_OK);
  }
  gst_pad_send_event (pad, gst_event_new_eos ());

  return NULL;
}

static GstBusSyncReply
on_message (GstBus * bus, GstMessage * message, gpointer user_data)
{
  const GstStructure *s = gst_message_get_structure (message);
  const gchar *name = gst_structure_get_name (s);
  GValueArray *rms_arr;
  const GValue *array_val;
  const GValue *value;
  gdouble rms;
  gint channels2;
  guint i;
  GstClockTime *rtime;

  if (message->type != GST_MESSAGE_ELEMENT
      || strcmp (name, "videoframe-audiolevel") != 0)
    goto done;

  num_msgs++;
  rtime = g_new (GstClockTime, 1);
  if (!gst_structure_get_clock_time (s, "running-time", rtime)) {
    g_warning ("Could not parse running time");
    g_free (rtime);
  } else {
    g_queue_push_tail (&msg_timestamp_q, rtime);
  }

  /* the values are packed into GValueArrays with the value per channel */
  array_val = gst_structure_get_value (s, "rms");
  rms_arr = (GValueArray *) g_value_get_boxed (array_val);
  channels2 = rms_arr->n_values;
  fail_unless_equals_int (channels2, channels);

  for (i = 0; i < channels; ++i) {
    value = g_value_array_get_nth (rms_arr, i);
    rms = g_value_get_double (value);
    if (per_channel) {
      fail_unless_equals_float (rms, expected_rms_per_channel[i]);
    } else if (early_video && *rtime <= 50 * GST_MSECOND) {
      fail_unless_equals_float (rms, 0);
    } else {
      fail_unless_equals_float (rms, expected_rms);
    }
  }

done:
  return GST_BUS_PASS;
}

static void
test_videoframe_audiolevel_generic (void)
{
  GstElement *alevel;
  GstPad *asink, *vsink, *asrc, *vsrc, *aoutput_sink, *voutput_sink;
  GThread *athread, *vthread;
  GstBus *bus;
  guint i;

  got_eos = FALSE;
  audio_buffer_count = 0;
  video_buffer_count = 0;
  num_msgs = 0;

  g_queue_init (&v_timestamp_q);
  g_queue_init (&msg_timestamp_q);

  alevel = gst_element_factory_make ("videoframe-audiolevel", NULL);
  fail_unless (alevel != NULL);

  bus = gst_bus_new ();
  gst_element_set_bus (alevel, bus);
  gst_bus_set_sync_handler (bus, on_message, NULL, NULL);

  asink = gst_element_get_static_pad (alevel, "asink");
  fail_unless (asink != NULL);

  vsink = gst_element_get_static_pad (alevel, "vsink");
  fail_unless (vsink != NULL);

  asrc = gst_element_get_static_pad (alevel, "asrc");
  aoutput_sink = gst_pad_new ("sink", GST_PAD_SINK);
  fail_unless (aoutput_sink != NULL);
  fail_unless (gst_pad_link (asrc, aoutput_sink) == GST_PAD_LINK_OK);

  vsrc = gst_element_get_static_pad (alevel, "vsrc");
  voutput_sink = gst_pad_new ("sink", GST_PAD_SINK);
  fail_unless (voutput_sink != NULL);
  fail_unless (gst_pad_link (vsrc, voutput_sink) == GST_PAD_LINK_OK);

  gst_pad_set_chain_function (aoutput_sink, output_achain);
  gst_pad_set_event_function (aoutput_sink, output_aevent);

  gst_pad_set_chain_function (voutput_sink, output_vchain);
  gst_pad_set_event_function (voutput_sink, output_vevent);

  gst_pad_set_active (aoutput_sink, TRUE);
  gst_pad_set_active (voutput_sink, TRUE);
  fail_unless (gst_element_set_state (alevel,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS);

  athread = g_thread_new ("athread", (GThreadFunc) push_abuffers, asink);
  vthread = g_thread_new ("vthread", (GThreadFunc) push_vbuffers, vsink);

  g_thread_join (vthread);
  g_thread_join (athread);

  fail_unless (got_eos);
  fail_unless_equals_int (audio_buffer_count, n_abuffers);
  fail_unless_equals_int (video_buffer_count, n_vbuffers);
  if (!long_video)
    fail_unless_equals_int (num_msgs, n_vbuffers);

  fail_unless_equals_int (g_queue_get_length (&v_timestamp_q), n_vbuffers);
  /* num_msgs is equal to n_vbuffers except in the case of long_video */
  fail_unless_equals_int (g_queue_get_length (&msg_timestamp_q), num_msgs);

  for (i = 0; i < g_queue_get_length (&msg_timestamp_q); i++) {
    GstClockTime *vt = g_queue_pop_head (&v_timestamp_q);
    GstClockTime *mt = g_queue_pop_head (&msg_timestamp_q);
    fail_unless (vt != NULL);
    fail_unless (mt != NULL);
    if (!video_gaps && !video_overlaps && !early_video)
      fail_unless_equals_uint64 (*vt, *mt);
    g_free (vt);
    g_free (mt);
  }

  /* teardown */
  gst_element_set_state (alevel, GST_STATE_NULL);
  gst_bus_set_flushing (bus, TRUE);
  gst_object_unref (bus);
  g_queue_foreach (&v_timestamp_q, (GFunc) g_free, NULL);
  g_queue_foreach (&msg_timestamp_q, (GFunc) g_free, NULL);
  g_queue_clear (&v_timestamp_q);
  g_queue_clear (&msg_timestamp_q);
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
  gst_object_unref (alevel);
}

GST_START_TEST (test_videoframe_audiolevel_16chan_1)
{
  set_default_params ();
  channels = 16;
  test_videoframe_audiolevel_generic ();
}

GST_END_TEST;

GST_START_TEST (test_videoframe_audiolevel_8chan_1)
{
  set_default_params ();
  channels = 8;
  test_videoframe_audiolevel_generic ();
}

GST_END_TEST;

GST_START_TEST (test_videoframe_audiolevel_2chan_1)
{
  set_default_params ();
  test_videoframe_audiolevel_generic ();
}

GST_END_TEST;

GST_START_TEST (test_videoframe_audiolevel_1chan_1)
{
  set_default_params ();
  channels = 1;
  test_videoframe_audiolevel_generic ();
}

GST_END_TEST;

GST_START_TEST (test_videoframe_audiolevel_16chan_0)
{
  set_default_params ();
  channels = 16;
  expected_rms = 0;
  fill_value = 0;
  test_videoframe_audiolevel_generic ();
}

GST_END_TEST;

GST_START_TEST (test_videoframe_audiolevel_8chan_0)
{
  set_default_params ();
  channels = 8;
  expected_rms = 0;
  fill_value = 0;
  test_videoframe_audiolevel_generic ();
}

GST_END_TEST;

GST_START_TEST (test_videoframe_audiolevel_2chan_0)
{
  set_default_params ();
  channels = 2;
  expected_rms = 0;
  fill_value = 0;
  test_videoframe_audiolevel_generic ();
}

GST_END_TEST;

GST_START_TEST (test_videoframe_audiolevel_1chan_0)
{
  set_default_params ();
  channels = 1;
  expected_rms = 0;
  fill_value = 0;
  test_videoframe_audiolevel_generic ();
}

GST_END_TEST;

GST_START_TEST (test_videoframe_audiolevel_adelay)
{
  set_default_params ();
  audiodelay = TRUE;
  test_videoframe_audiolevel_generic ();
}

GST_END_TEST;

GST_START_TEST (test_videoframe_audiolevel_vdelay)
{
  set_default_params ();
  videodelay = TRUE;
  test_videoframe_audiolevel_generic ();
}

GST_END_TEST;

GST_START_TEST (test_videoframe_audiolevel_per_channel)
{
  set_default_params ();
  per_channel = TRUE;
  test_videoframe_audiolevel_generic ();
}

GST_END_TEST;

GST_START_TEST (test_videoframe_audiolevel_long_video)
{
  set_default_params ();
  n_abuffers = 6;
  n_vbuffers = 255;
  long_video = TRUE;
  test_videoframe_audiolevel_generic ();
}

GST_END_TEST;

GST_START_TEST (test_videoframe_audiolevel_video_gaps)
{
  set_default_params ();
  video_gaps = TRUE;
  test_videoframe_audiolevel_generic ();
}

GST_END_TEST;

GST_START_TEST (test_videoframe_audiolevel_video_overlaps)
{
  set_default_params ();
  video_overlaps = TRUE;
  test_videoframe_audiolevel_generic ();
}

GST_END_TEST;

GST_START_TEST (test_videoframe_audiolevel_audio_nondiscont)
{
  set_default_params ();
  audio_nondiscont = TRUE;
  test_videoframe_audiolevel_generic ();
}

GST_END_TEST;

GST_START_TEST (test_videoframe_audiolevel_audio_drift)
{
  set_default_params ();
  audio_drift = TRUE;
  test_videoframe_audiolevel_generic ();
}

GST_END_TEST;
GST_START_TEST (test_videoframe_audiolevel_early_video)
{
  set_default_params ();
  early_video = TRUE;
  test_videoframe_audiolevel_generic ();
}

GST_END_TEST;

GST_START_TEST (test_videoframe_audiolevel_late_video)
{
  set_default_params ();
  late_video = TRUE;
  test_videoframe_audiolevel_generic ();
}

GST_END_TEST;


static Suite *
videoframe_audiolevel_suite (void)
{
  Suite *s = suite_create ("videoframe-audiolevel");
  TCase *tc_chain;

  tc_chain = tcase_create ("videoframe-audiolevel");
  tcase_add_test (tc_chain, test_videoframe_audiolevel_16chan_1);
  tcase_add_test (tc_chain, test_videoframe_audiolevel_8chan_1);
  tcase_add_test (tc_chain, test_videoframe_audiolevel_2chan_1);
  tcase_add_test (tc_chain, test_videoframe_audiolevel_1chan_1);
  tcase_add_test (tc_chain, test_videoframe_audiolevel_16chan_0);
  tcase_add_test (tc_chain, test_videoframe_audiolevel_8chan_0);
  tcase_add_test (tc_chain, test_videoframe_audiolevel_2chan_0);
  tcase_add_test (tc_chain, test_videoframe_audiolevel_1chan_0);
  tcase_add_test (tc_chain, test_videoframe_audiolevel_adelay);
  tcase_add_test (tc_chain, test_videoframe_audiolevel_vdelay);
  tcase_add_test (tc_chain, test_videoframe_audiolevel_per_channel);
  tcase_add_test (tc_chain, test_videoframe_audiolevel_long_video);
  tcase_add_test (tc_chain, test_videoframe_audiolevel_video_gaps);
  tcase_add_test (tc_chain, test_videoframe_audiolevel_video_overlaps);
  tcase_add_test (tc_chain, test_videoframe_audiolevel_audio_nondiscont);
  tcase_add_test (tc_chain, test_videoframe_audiolevel_audio_drift);
  tcase_add_test (tc_chain, test_videoframe_audiolevel_early_video);
  tcase_add_test (tc_chain, test_videoframe_audiolevel_late_video);
  suite_add_tcase (s, tc_chain);

  return s;
}

GST_CHECK_MAIN (videoframe_audiolevel);
