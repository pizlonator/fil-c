/* GStreamer
 *
 * unit test for audioresample, based on the audioresample unit test
 *
 * Copyright (C) <2005> Thomas Vander Stichele <thomas at apestaart dot org>
 * Copyright (C) <2006> Tim-Philipp Müller <tim at centricular net>
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

#include <gst/check/gstcheck.h>

#include <gst/audio/audio.h>

#include <gst/fft/gstfft.h>
#include <gst/fft/gstffts16.h>
#include <gst/fft/gstffts32.h>
#include <gst/fft/gstfftf32.h>
#include <gst/fft/gstfftf64.h>

/* For ease of programming we use globals to keep refs for our floating
 * src and sink pads we create; otherwise we always have to do get_pad,
 * get_peer, and then remove references in every test function */
static GstPad *mysrcpad, *mysinkpad;

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
#define FORMATS  "{ F32LE, F64LE, S16LE, S32LE }"
#else
#define FORMATS  "{ F32BE, F64BE, S16BE, S32BE }"
#endif

#define RESAMPLE_CAPS                   \
    "audio/x-raw, "                     \
    "format = (string) "FORMATS", "     \
    "channels = (int) [ 1, MAX ], "     \
    "rate = (int) [ 1,  MAX ], "        \
    "layout = (string) interleaved"

static GstElement *
setup_audioresample (int channels, guint64 mask, int inrate, int outrate,
    const gchar * format)
{
  GstPadTemplate *sinktemplate;
  static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
      GST_PAD_SRC,
      GST_PAD_ALWAYS,
      GST_STATIC_CAPS (RESAMPLE_CAPS)
      );
  GstElement *audioresample;
  GstCaps *caps;
  GstStructure *structure;

  GST_DEBUG ("setup_audioresample");
  audioresample = gst_check_setup_element ("audioresample");

  caps = gst_caps_from_string (RESAMPLE_CAPS);
  structure = gst_caps_get_structure (caps, 0);
  gst_structure_set (structure, "channels", G_TYPE_INT, channels,
      "rate", G_TYPE_INT, inrate, "format", G_TYPE_STRING, format,
      "channel-mask", GST_TYPE_BITMASK, mask, NULL);
  fail_unless (gst_caps_is_fixed (caps));

  fail_unless (gst_element_set_state (audioresample,
          GST_STATE_PAUSED) == GST_STATE_CHANGE_SUCCESS,
      "could not set to paused");

  mysrcpad = gst_check_setup_src_pad (audioresample, &srctemplate);
  gst_pad_set_active (mysrcpad, TRUE);
  gst_check_setup_events (mysrcpad, audioresample, caps, GST_FORMAT_TIME);
  gst_caps_unref (caps);

  caps = gst_caps_from_string (RESAMPLE_CAPS);
  structure = gst_caps_get_structure (caps, 0);
  gst_structure_set (structure, "channels", G_TYPE_INT, channels,
      "rate", G_TYPE_INT, outrate, "format", G_TYPE_STRING, format, NULL);
  fail_unless (gst_caps_is_fixed (caps));
  sinktemplate =
      gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS, caps);

  mysinkpad =
      gst_check_setup_sink_pad_from_template (audioresample, sinktemplate);
  gst_pad_set_active (mysinkpad, TRUE);
  /* this installs a getcaps func that will always return the caps we set
   * later */
  gst_pad_use_fixed_caps (mysinkpad);

  gst_caps_unref (caps);
  gst_object_unref (sinktemplate);

  return audioresample;
}

static void
cleanup_audioresample (GstElement * audioresample)
{
  GST_DEBUG ("cleanup_audioresample");

  fail_unless (gst_element_set_state (audioresample,
          GST_STATE_NULL) == GST_STATE_CHANGE_SUCCESS, "could not set to NULL");

  gst_pad_set_active (mysrcpad, FALSE);
  gst_pad_set_active (mysinkpad, FALSE);
  gst_check_teardown_src_pad (audioresample);
  gst_check_teardown_sink_pad (audioresample);
  gst_check_teardown_element (audioresample);
  gst_check_drop_buffers ();
}

static void
fail_unless_perfect_stream (void)
{
  guint64 timestamp = 0L, duration = 0L;
  guint64 offset = 0L, offset_end = 0L;

  GList *l;
  GstBuffer *buffer;

  for (l = buffers; l; l = l->next) {
    buffer = GST_BUFFER (l->data);
    ASSERT_BUFFER_REFCOUNT (buffer, "buffer", 1);
    GST_DEBUG ("buffer timestamp %" G_GUINT64_FORMAT ", duration %"
        G_GUINT64_FORMAT " offset %" G_GUINT64_FORMAT " offset_end %"
        G_GUINT64_FORMAT,
        GST_BUFFER_TIMESTAMP (buffer),
        GST_BUFFER_DURATION (buffer),
        GST_BUFFER_OFFSET (buffer), GST_BUFFER_OFFSET_END (buffer));

    fail_unless_equals_uint64 (timestamp, GST_BUFFER_TIMESTAMP (buffer));
    fail_unless_equals_uint64 (offset, GST_BUFFER_OFFSET (buffer));
    duration = GST_BUFFER_DURATION (buffer);
    offset_end = GST_BUFFER_OFFSET_END (buffer);

    timestamp += duration;
    offset = offset_end;
    gst_buffer_unref (buffer);
  }
  g_list_free (buffers);
  buffers = NULL;
}

/* this tests that the output is a perfect stream if the input is */
static void
test_perfect_stream_instance (int inrate, int outrate, int samples,
    int numbuffers)
{
  GstElement *audioresample;
  GstBuffer *inbuffer, *outbuffer;
  GstCaps *caps;
  guint64 offset = 0;
  int i, j;
  GstMapInfo map;
  gint16 *p;

  audioresample =
      setup_audioresample (2, 0x3, inrate, outrate, GST_AUDIO_NE (S16));
  caps = gst_pad_get_current_caps (mysrcpad);
  fail_unless (gst_caps_is_fixed (caps));

  fail_unless (gst_element_set_state (audioresample,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  for (j = 1; j <= numbuffers; ++j) {

    inbuffer = gst_buffer_new_and_alloc (samples * 4);
    GST_BUFFER_DURATION (inbuffer) = GST_FRAMES_TO_CLOCK_TIME (samples, inrate);
    GST_BUFFER_TIMESTAMP (inbuffer) = GST_BUFFER_DURATION (inbuffer) * (j - 1);
    GST_BUFFER_OFFSET (inbuffer) = offset;
    offset += samples;
    GST_BUFFER_OFFSET_END (inbuffer) = offset;

    gst_buffer_map (inbuffer, &map, GST_MAP_WRITE);
    p = (gint16 *) map.data;

    /* create a 16 bit signed ramp */
    for (i = 0; i < samples; ++i) {
      *p = -32767 + i * (65535 / samples);
      ++p;
      *p = -32767 + i * (65535 / samples);
      ++p;
    }
    gst_buffer_unmap (inbuffer, &map);

    /* pushing gives away my reference ... */
    fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);
    /* ... but it ends up being collected on the global buffer list */
    fail_unless_equals_int (g_list_length (buffers), j);
  }

  /* FIXME: we should make audioresample handle eos by flushing out the last
   * samples, which will give us one more, small, buffer */
  fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);
  ASSERT_BUFFER_REFCOUNT (outbuffer, "outbuffer", 1);

  fail_unless_perfect_stream ();

  /* cleanup */
  gst_caps_unref (caps);
  cleanup_audioresample (audioresample);
}


/* make sure that outgoing buffers are contiguous in timestamp/duration and
 * offset/offsetend
 */
GST_START_TEST (test_perfect_stream)
{
  /* integral scalings */
  test_perfect_stream_instance (48000, 24000, 500, 20);
  test_perfect_stream_instance (48000, 12000, 500, 20);
  test_perfect_stream_instance (12000, 24000, 500, 20);
  test_perfect_stream_instance (12000, 48000, 500, 20);

  /* non-integral scalings */
  test_perfect_stream_instance (44100, 8000, 500, 20);
  test_perfect_stream_instance (8000, 44100, 500, 20);

  /* wacky scalings */
  test_perfect_stream_instance (12345, 54321, 500, 20);
  test_perfect_stream_instance (101, 99, 500, 20);
}

GST_END_TEST;

/* this tests that the output is a correct discontinuous stream
 * if the input is; ie input drops in time come out the same way */
static void
test_discont_stream_instance (int inrate, int outrate, int samples,
    int numbuffers)
{
  GstElement *audioresample;
  GstBuffer *inbuffer, *outbuffer;
  GstCaps *caps;
  GstClockTime ints;

  int i, j;
  GstMapInfo map;
  gint16 *p;

  GST_DEBUG ("inrate:%d outrate:%d samples:%d numbuffers:%d",
      inrate, outrate, samples, numbuffers);

  audioresample =
      setup_audioresample (2, 3, inrate, outrate, GST_AUDIO_NE (S16));
  caps = gst_pad_get_current_caps (mysrcpad);
  fail_unless (gst_caps_is_fixed (caps));

  fail_unless (gst_element_set_state (audioresample,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  for (j = 1; j <= numbuffers; ++j) {

    inbuffer = gst_buffer_new_and_alloc (samples * 4);
    GST_BUFFER_DURATION (inbuffer) = samples * GST_SECOND / inrate;
    /* "drop" half the buffers */
    ints = GST_BUFFER_DURATION (inbuffer) * 2 * (j - 1);
    GST_BUFFER_TIMESTAMP (inbuffer) = ints;
    GST_BUFFER_OFFSET (inbuffer) = (j - 1) * 2 * samples;
    GST_BUFFER_OFFSET_END (inbuffer) = j * 2 * samples + samples;

    gst_buffer_map (inbuffer, &map, GST_MAP_WRITE);
    p = (gint16 *) map.data;
    /* create a 16 bit signed ramp */
    for (i = 0; i < samples; ++i) {
      *p = -32767 + i * (65535 / samples);
      ++p;
      *p = -32767 + i * (65535 / samples);
      ++p;
    }
    gst_buffer_unmap (inbuffer, &map);

    GST_DEBUG ("Sending Buffer time:%" G_GUINT64_FORMAT " duration:%"
        G_GINT64_FORMAT " discont:%d offset:%" G_GUINT64_FORMAT " offset_end:%"
        G_GUINT64_FORMAT, GST_BUFFER_TIMESTAMP (inbuffer),
        GST_BUFFER_DURATION (inbuffer), GST_BUFFER_IS_DISCONT (inbuffer),
        GST_BUFFER_OFFSET (inbuffer), GST_BUFFER_OFFSET_END (inbuffer));
    /* pushing gives away my reference ... */
    fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);

    /* check if the timestamp of the pushed buffer matches the incoming one */
    outbuffer = g_list_nth_data (buffers, g_list_length (buffers) - 1);
    fail_if (outbuffer == NULL);
    fail_unless_equals_uint64 (ints, GST_BUFFER_TIMESTAMP (outbuffer));
    GST_DEBUG ("Got Buffer time:%" G_GUINT64_FORMAT " duration:%"
        G_GINT64_FORMAT " discont:%d offset:%" G_GUINT64_FORMAT " offset_end:%"
        G_GUINT64_FORMAT, GST_BUFFER_TIMESTAMP (outbuffer),
        GST_BUFFER_DURATION (outbuffer), GST_BUFFER_IS_DISCONT (outbuffer),
        GST_BUFFER_OFFSET (outbuffer), GST_BUFFER_OFFSET_END (outbuffer));
    if (j > 1) {
      fail_unless (GST_BUFFER_IS_DISCONT (outbuffer),
          "expected discont for buffer #%d", j);
    }
  }

  /* cleanup */
  gst_caps_unref (caps);
  cleanup_audioresample (audioresample);
}

GST_START_TEST (test_discont_stream)
{
  /* integral scalings */
  test_discont_stream_instance (48000, 24000, 5000, 20);
  test_discont_stream_instance (48000, 12000, 5000, 20);
  test_discont_stream_instance (12000, 24000, 5000, 20);
  test_discont_stream_instance (12000, 48000, 5000, 20);

  /* non-integral scalings */
  test_discont_stream_instance (44100, 8000, 5000, 20);
  test_discont_stream_instance (8000, 44100, 5000, 20);

  /* wacky scalings */
  test_discont_stream_instance (12345, 54321, 5000, 20);
  test_discont_stream_instance (101, 99, 5000, 20);
}

GST_END_TEST;



GST_START_TEST (test_reuse)
{
  GstElement *audioresample;
  GstEvent *newseg;
  GstBuffer *inbuffer;
  GstCaps *caps;
  GstSegment segment;

  audioresample = setup_audioresample (1, 0, 9343, 48000, GST_AUDIO_NE (S16));
  caps = gst_pad_get_current_caps (mysrcpad);
  fail_unless (gst_caps_is_fixed (caps));

  fail_unless (gst_element_set_state (audioresample,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  gst_segment_init (&segment, GST_FORMAT_TIME);
  newseg = gst_event_new_segment (&segment);
  fail_unless (gst_pad_push_event (mysrcpad, newseg) != FALSE);

  inbuffer = gst_buffer_new_and_alloc (9343 * 4);
  gst_buffer_memset (inbuffer, 0, 0, 9343 * 4);
  GST_BUFFER_DURATION (inbuffer) = GST_SECOND;
  GST_BUFFER_TIMESTAMP (inbuffer) = 0;
  GST_BUFFER_OFFSET (inbuffer) = 0;

  /* pushing gives away my reference ... */
  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);

  /* ... but it ends up being collected on the global buffer list */
  fail_unless_equals_int (g_list_length (buffers), 1);

  /* now reset and try again ... */
  fail_unless (gst_element_set_state (audioresample,
          GST_STATE_NULL) == GST_STATE_CHANGE_SUCCESS, "could not set to NULL");

  fail_unless (gst_element_set_state (audioresample,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  newseg = gst_event_new_segment (&segment);
  fail_unless (gst_pad_push_event (mysrcpad, newseg) != FALSE);

  inbuffer = gst_buffer_new_and_alloc (9343 * 4);
  gst_buffer_memset (inbuffer, 0, 0, 9343 * 4);
  GST_BUFFER_DURATION (inbuffer) = GST_SECOND;
  GST_BUFFER_TIMESTAMP (inbuffer) = 0;
  GST_BUFFER_OFFSET (inbuffer) = 0;

  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);

  /* ... it also ends up being collected on the global buffer list. If we
   * now have more than 2 buffers, then audioresample probably didn't clean
   * up its internal buffer properly and tried to push the remaining samples
   * when it got the second NEWSEGMENT event */
  fail_unless_equals_int (g_list_length (buffers), 2);

  cleanup_audioresample (audioresample);
  gst_caps_unref (caps);
}

GST_END_TEST;

GST_START_TEST (test_shutdown)
{
  GstElement *pipeline, *src, *cf1, *ar, *cf2, *sink;
  GstCaps *caps;
  guint i;

  /* create pipeline, force audioresample to actually resample */
  pipeline = gst_pipeline_new (NULL);

  src = gst_check_setup_element ("audiotestsrc");
  cf1 = gst_check_setup_element ("capsfilter");
  ar = gst_check_setup_element ("audioresample");
  cf2 = gst_check_setup_element ("capsfilter");
  g_object_set (cf2, "name", "capsfilter2", NULL);
  sink = gst_check_setup_element ("fakesink");

  caps = gst_caps_new_simple ("audio/x-raw", "rate", G_TYPE_INT, 11025, NULL);
  g_object_set (cf1, "caps", caps, NULL);
  gst_caps_unref (caps);

  caps = gst_caps_new_simple ("audio/x-raw", "rate", G_TYPE_INT, 48000, NULL);
  g_object_set (cf2, "caps", caps, NULL);
  gst_caps_unref (caps);

  /* don't want to sync against the clock, the more throughput the better */
  g_object_set (src, "is-live", FALSE, NULL);
  g_object_set (sink, "sync", FALSE, NULL);

  gst_bin_add_many (GST_BIN (pipeline), src, cf1, ar, cf2, sink, NULL);
  fail_if (!gst_element_link_many (src, cf1, ar, cf2, sink, NULL));

  /* now, wait until pipeline is running and then shut it down again; repeat */
  for (i = 0; i < 20; ++i) {
    gst_element_set_state (pipeline, GST_STATE_PAUSED);
    gst_element_get_state (pipeline, NULL, NULL, -1);
    gst_element_set_state (pipeline, GST_STATE_PLAYING);
    g_usleep (100);
    gst_element_set_state (pipeline, GST_STATE_NULL);
  }

  gst_object_unref (pipeline);
}

GST_END_TEST;

static void
live_switch_push (gint pts, gint rate, GstCaps * caps)
{
  GstBuffer *inbuffer;
  GstCaps *desired;

  desired = gst_caps_copy (caps);
  gst_caps_set_simple (desired, "rate", G_TYPE_INT, rate, NULL);
  gst_pad_set_caps (mysrcpad, desired);

  inbuffer = gst_buffer_new_and_alloc (rate * 4 * 2);
  gst_buffer_memset (inbuffer, 0, 0, rate * 4 * 2);

  GST_BUFFER_DURATION (inbuffer) = GST_SECOND;
  GST_BUFFER_TIMESTAMP (inbuffer) = pts * GST_SECOND;
  GST_BUFFER_OFFSET (inbuffer) = 0;
  GST_BUFFER_OFFSET_END (inbuffer) = rate - 1;

  /* pushing gives away my reference ... */
  fail_unless_equals_int (gst_pad_push (mysrcpad, inbuffer), GST_FLOW_OK);

  /* ... but it ends up being collected on the global buffer list */

  gst_caps_unref (desired);
}

GST_START_TEST (test_live_switch)
{
  GstElement *audioresample;
  GstEvent *newseg;
  GstCaps *caps;
  GstSegment segment;
  GList *l;
  guint i;

  audioresample =
      setup_audioresample (4, 0xf, 48000, 48000, GST_AUDIO_NE (S16));

  caps = gst_pad_get_current_caps (mysrcpad);
  fail_unless (gst_caps_is_fixed (caps));

  fail_unless (gst_element_set_state (audioresample,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  gst_segment_init (&segment, GST_FORMAT_TIME);
  newseg = gst_event_new_segment (&segment);
  fail_unless (gst_pad_push_event (mysrcpad, newseg) != FALSE);

  /* downstream can accept the requested rate */
  live_switch_push (0, 48000, caps);

  /* buffer is directly passed through */
  fail_unless_equals_int (g_list_length (buffers), 1);

  /* Downstream can never accept this rate */
  live_switch_push (1, 40000, caps);

  /* one additional buffer is provided with the new sample rate */
  fail_unless_equals_int (g_list_length (buffers), 2);

  /* Downstream can never accept this rate */
  live_switch_push (2, 50000, caps);

  /* two additional buffers are provided. One is the drained remainder of
   * the previous sample rate, the second is the buffer with the new sample
   * rate */
  fail_unless_equals_int (g_list_length (buffers), 4);

  /* Send EOS to drain the remaining samples */
  fail_unless (gst_pad_push_event (mysrcpad, gst_event_new_eos ()));
  fail_unless_equals_int (g_list_length (buffers), 5);

  /* Now test that each buffer has the expected samples. We simply check this
   * by checking whether the timestamps, durations and sizes are matching */
  for (l = buffers, i = 0; l; l = l->next, i++) {
    GstBuffer *buffer = GST_BUFFER (l->data);

    switch (i) {
      case 0:
        fail_unless_equals_uint64 (GST_BUFFER_PTS (buffer), 0 * GST_SECOND);
        fail_unless_equals_uint64 (GST_BUFFER_DURATION (buffer),
            1 * GST_SECOND);
        fail_unless_equals_int (gst_buffer_get_size (buffer), 48000 * 4 * 2);
        break;
      case 1:
        fail_unless_equals_uint64 (GST_BUFFER_PTS (buffer), 1 * GST_SECOND);
        fail_unless_equals_int (gst_buffer_get_size (buffer), 47961 * 4 * 2);
        break;
      case 2:
        fail_unless (G_APPROX_VALUE (GST_BUFFER_PTS (buffer) +
                GST_BUFFER_DURATION (buffer), 2 * GST_SECOND,
                GST_SECOND / 48000 + 1));
        fail_unless_equals_int (gst_buffer_get_size (buffer), 38 * 4 * 2);
        break;
      case 3:
        fail_unless_equals_uint64 (GST_BUFFER_PTS (buffer), 2 * GST_SECOND);
        fail_unless_equals_int (gst_buffer_get_size (buffer), 47969 * 4 * 2);
        break;
      case 4:
        fail_unless (G_APPROX_VALUE (GST_BUFFER_PTS (buffer) +
                GST_BUFFER_DURATION (buffer), 3 * GST_SECOND,
                GST_SECOND / 48000 + 1));
        fail_unless_equals_int (gst_buffer_get_size (buffer), 30 * 4 * 2);
        break;
      default:
        g_assert_not_reached ();
        break;
    }

    gst_buffer_unref (buffer);
  }

  g_list_free (buffers);
  buffers = NULL;

  cleanup_audioresample (audioresample);
  gst_caps_unref (caps);
}

GST_END_TEST;

static gint current_rate = 0;

static gboolean
live_switch_sink_query (GstPad * pad, GstObject * parent, GstQuery * query)
{
  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_ACCEPT_CAPS:{
      GstCaps *acceptable_caps;
      GstCaps *caps;

      acceptable_caps = gst_pad_get_current_caps (mysrcpad);
      acceptable_caps = gst_caps_make_writable (acceptable_caps);
      gst_caps_set_simple (acceptable_caps, "rate", G_TYPE_INT, current_rate,
          NULL);

      gst_query_parse_accept_caps (query, &caps);

      gst_query_set_accept_caps_result (query, gst_caps_can_intersect (caps,
              acceptable_caps));

      gst_caps_unref (acceptable_caps);

      return TRUE;
    }
    case GST_QUERY_CAPS:{
      GstCaps *acceptable_caps;
      GstCaps *filter;
      GstCaps *caps;

      acceptable_caps = gst_pad_get_current_caps (mysrcpad);
      acceptable_caps = gst_caps_make_writable (acceptable_caps);
      gst_caps_set_simple (acceptable_caps, "rate", G_TYPE_INT, current_rate,
          NULL);

      gst_query_parse_caps (query, &filter);

      if (filter)
        caps =
            gst_caps_intersect_full (filter, acceptable_caps,
            GST_CAPS_INTERSECT_FIRST);
      else
        caps = gst_caps_ref (acceptable_caps);

      gst_query_set_caps_result (query, caps);

      gst_caps_unref (caps);
      gst_caps_unref (acceptable_caps);

      return TRUE;
    }
    default:
      return gst_pad_query_default (pad, parent, query);
  }
}

static void
live_switch_push_downstream (gint pts, gint rate)
{
  GstBuffer *inbuffer;

  current_rate = rate;
  gst_pad_push_event (mysinkpad, gst_event_new_reconfigure ());

  inbuffer = gst_buffer_new_and_alloc (48000 * 4 * 2);
  gst_buffer_memset (inbuffer, 0, 0, 48000 * 4 * 2);

  GST_BUFFER_DURATION (inbuffer) = GST_SECOND;
  GST_BUFFER_TIMESTAMP (inbuffer) = pts * GST_SECOND;
  GST_BUFFER_OFFSET (inbuffer) = 0;
  GST_BUFFER_OFFSET_END (inbuffer) = 47999;

  /* pushing gives away my reference ... */
  fail_unless_equals_int (gst_pad_push (mysrcpad, inbuffer), GST_FLOW_OK);

  /* ... but it ends up being collected on the global buffer list */
}

GST_START_TEST (test_live_switch_downstream)
{
  GstElement *audioresample;
  GstEvent *newseg;
  GstCaps *caps;
  GstSegment segment;
  GList *l;
  guint i;

  audioresample =
      setup_audioresample (4, 0xf, 48000, 48000, GST_AUDIO_NE (S16));

  gst_pad_set_query_function (mysinkpad, live_switch_sink_query);

  caps = gst_pad_get_current_caps (mysrcpad);
  fail_unless (gst_caps_is_fixed (caps));

  fail_unless (gst_element_set_state (audioresample,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  gst_segment_init (&segment, GST_FORMAT_TIME);
  newseg = gst_event_new_segment (&segment);
  fail_unless (gst_pad_push_event (mysrcpad, newseg) != FALSE);

  /* buffer is directly passed through */
  live_switch_push_downstream (0, 48000);
  fail_unless_equals_int (g_list_length (buffers), 1);

  /* Reconfigure downstream to 40000 Hz */
  live_switch_push_downstream (1, 40000);

  /* one additional buffer is provided with the new sample rate */
  fail_unless_equals_int (g_list_length (buffers), 2);

  /* Reconfigure downstream to 50000 Hz */
  live_switch_push_downstream (2, 50000);

  /* two additional buffers are provided. One is the drained remainder of
   * the previous sample rate, the second is the buffer with the new sample
   * rate */
  fail_unless_equals_int (g_list_length (buffers), 4);

  /* Send EOS to drain the remaining samples */
  fail_unless (gst_pad_push_event (mysrcpad, gst_event_new_eos ()));
  fail_unless_equals_int (g_list_length (buffers), 5);

  /* Now test that each buffer has the expected samples. We simply check this
   * by checking whether the timestamps, durations and sizes are matching */
  for (l = buffers, i = 0; l; l = l->next, i++) {
    GstBuffer *buffer = GST_BUFFER (l->data);

    switch (i) {
      case 0:
        fail_unless_equals_uint64 (GST_BUFFER_PTS (buffer), 0 * GST_SECOND);
        fail_unless_equals_uint64 (GST_BUFFER_DURATION (buffer),
            1 * GST_SECOND);
        fail_unless_equals_int (gst_buffer_get_size (buffer), 48000 * 4 * 2);
        break;
      case 1:
        fail_unless_equals_uint64 (GST_BUFFER_PTS (buffer), 1 * GST_SECOND);
        fail_unless_equals_int (gst_buffer_get_size (buffer), 39966 * 4 * 2);
        break;
      case 2:
        fail_unless (G_APPROX_VALUE (GST_BUFFER_PTS (buffer) +
                GST_BUFFER_DURATION (buffer), 2 * GST_SECOND,
                GST_SECOND / 40000 + 1));
        fail_unless_equals_int (gst_buffer_get_size (buffer), 34 * 4 * 2);
        break;
      case 3:
        fail_unless_equals_uint64 (GST_BUFFER_PTS (buffer), 2 * GST_SECOND);
        fail_unless_equals_int (gst_buffer_get_size (buffer), 49966 * 4 * 2);
        break;
      case 4:
        fail_unless (G_APPROX_VALUE (GST_BUFFER_PTS (buffer) +
                GST_BUFFER_DURATION (buffer), 3 * GST_SECOND,
                GST_SECOND / 50000 + 1));
        fail_unless_equals_int (gst_buffer_get_size (buffer), 33 * 4 * 2);
        break;
      default:
        g_assert_not_reached ();
        break;
    }

    gst_buffer_unref (buffer);
  }

  g_list_free (buffers);
  buffers = NULL;

  cleanup_audioresample (audioresample);
  gst_caps_unref (caps);
}

GST_END_TEST;

#ifndef GST_DISABLE_PARSE

static GMainLoop *loop;
static gint messages = 0;

static void
element_message_cb (GstBus * bus, GstMessage * message, gpointer user_data)
{
  gchar *s;

  s = gst_structure_to_string (gst_message_get_structure (message));
  GST_DEBUG ("Received message: %s", s);
  g_free (s);

  messages++;
}

static void
eos_message_cb (GstBus * bus, GstMessage * message, gpointer user_data)
{
  GST_DEBUG ("Received eos");
  g_main_loop_quit (loop);
}

static void
test_pipeline (const gchar * format, gint inrate, gint outrate, gint quality)
{
  GstElement *pipeline;
  GstBus *bus;
  GError *error = NULL;
  gchar *pipe_str;

  pipe_str =
      g_strdup_printf
      ("audiotestsrc num-buffers=10 ! audioconvert ! audio/x-raw,format=%s,rate=%d,channels=2 ! audioresample quality=%d ! audio/x-raw,format=%s,rate=%d ! identity check-imperfect-timestamp=TRUE ! fakesink",
      format, inrate, quality, format, outrate);

  pipeline = gst_parse_launch (pipe_str, &error);
  fail_unless (pipeline != NULL, "Error parsing pipeline: %s",
      error ? error->message : "(invalid error)");
  g_free (pipe_str);

  bus = gst_element_get_bus (pipeline);
  fail_if (bus == NULL);
  gst_bus_add_signal_watch (bus);
  g_signal_connect (bus, "message::element", (GCallback) element_message_cb,
      NULL);
  g_signal_connect (bus, "message::eos", (GCallback) eos_message_cb, NULL);

  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  /* run until we receive EOS */
  loop = g_main_loop_new (NULL, FALSE);

  g_main_loop_run (loop);

  g_main_loop_unref (loop);
  loop = NULL;

  gst_element_set_state (pipeline, GST_STATE_NULL);

  gst_bus_remove_signal_watch (bus);
  gst_object_unref (bus);

  fail_if (messages > 0, "Received imperfect timestamp messages");
  gst_object_unref (pipeline);
}

GST_START_TEST (test_pipelines)
{
  gint quality;

  /* Test qualities 0, 5 and 10 */
  for (quality = 0; quality < 11; quality += 5) {
    GST_DEBUG ("Checking with quality %d", quality);

    test_pipeline ("S8", 44100, 48000, quality);
    test_pipeline ("S8", 48000, 44100, quality);

    test_pipeline (GST_AUDIO_NE (S16), 44100, 48000, quality);
    test_pipeline (GST_AUDIO_NE (S16), 48000, 44100, quality);

    test_pipeline (GST_AUDIO_NE (S24), 44100, 48000, quality);
    test_pipeline (GST_AUDIO_NE (S24), 48000, 44100, quality);

    test_pipeline (GST_AUDIO_NE (S32), 44100, 48000, quality);
    test_pipeline (GST_AUDIO_NE (S32), 48000, 44100, quality);

    test_pipeline (GST_AUDIO_NE (F32), 44100, 48000, quality);
    test_pipeline (GST_AUDIO_NE (F32), 48000, 44100, quality);

    test_pipeline (GST_AUDIO_NE (F64), 44100, 48000, quality);
    test_pipeline (GST_AUDIO_NE (F64), 48000, 44100, quality);
  }
}

GST_END_TEST;

GST_START_TEST (test_preference_passthrough)
{
  GstStateChangeReturn ret;
  GstElement *pipeline, *src;
  GstStructure *s;
  GstMessage *msg;
  GstCaps *caps;
  GstPad *pad;
  GstBus *bus;
  GError *error = NULL;
  gint rate = 0;

  pipeline = gst_parse_launch ("audiotestsrc num-buffers=1 name=src ! "
      "audioresample ! audio/x-raw,format=" GST_AUDIO_NE (S16) ",channels=1,"
      "rate=8000 ! fakesink can-activate-pull=false", &error);
  fail_unless (pipeline != NULL, "Error parsing pipeline: %s",
      error ? error->message : "(invalid error)");

  ret = gst_element_set_state (pipeline, GST_STATE_PLAYING);
  fail_unless_equals_int (ret, GST_STATE_CHANGE_ASYNC);

  /* run until we receive EOS */
  bus = gst_element_get_bus (pipeline);
  fail_if (bus == NULL);
  msg = gst_bus_timed_pop_filtered (bus, -1, GST_MESSAGE_EOS);
  gst_message_unref (msg);
  gst_object_unref (bus);

  src = gst_bin_get_by_name (GST_BIN (pipeline), "src");
  fail_unless (src != NULL);
  pad = gst_element_get_static_pad (src, "src");
  fail_unless (pad != NULL);
  caps = gst_pad_get_current_caps (pad);
  GST_LOG ("current audiotestsrc caps: %" GST_PTR_FORMAT, caps);
  fail_unless (caps != NULL);
  s = gst_caps_get_structure (caps, 0);
  fail_unless (gst_structure_get_int (s, "rate", &rate));
  /* there's no need to resample, audiotestsrc supports any rate, so make
   * sure audioresample provided upstream with the right caps to negotiate
   * this correctly */
  fail_unless_equals_int (rate, 8000);
  gst_caps_unref (caps);
  gst_object_unref (pad);
  gst_object_unref (src);

  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (pipeline);
}

GST_END_TEST;

#endif

static void
_message_cb (GstBus * bus, GstMessage * message, gpointer user_data)
{
  GMainLoop *loop = user_data;

  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_ERROR:
    case GST_MESSAGE_WARNING:
      g_assert_not_reached ();
      break;
    case GST_MESSAGE_EOS:
      g_main_loop_quit (loop);
      break;
    default:
      break;
  }
}

typedef struct
{
  guint64 latency;
  GstClockTime in_ts;

  GstClockTime next_out_ts;
  guint64 next_out_off;

  guint64 in_buffer_count, out_buffer_count;
} TimestampDriftCtx;

static void
fakesink_handoff_cb (GstElement * object, GstBuffer * buffer, GstPad * pad,
    gpointer user_data)
{
  TimestampDriftCtx *ctx = user_data;

  ctx->out_buffer_count++;
  if (ctx->latency == GST_CLOCK_TIME_NONE) {
    ctx->latency = 1000 - gst_buffer_get_size (buffer) / 8;
  }

  /* Check if we have a perfectly timestamped stream */
  if (ctx->next_out_ts != GST_CLOCK_TIME_NONE)
    fail_unless (ctx->next_out_ts == GST_BUFFER_TIMESTAMP (buffer),
        "expected timestamp %" GST_TIME_FORMAT " got timestamp %"
        GST_TIME_FORMAT, GST_TIME_ARGS (ctx->next_out_ts),
        GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buffer)));

  /* Check if we have a perfectly offsetted stream */
  fail_unless (GST_BUFFER_OFFSET_END (buffer) ==
      GST_BUFFER_OFFSET (buffer) + gst_buffer_get_size (buffer) / 8,
      "expected offset end %" G_GUINT64_FORMAT " got offset end %"
      G_GUINT64_FORMAT,
      GST_BUFFER_OFFSET (buffer) + gst_buffer_get_size (buffer) / 8,
      GST_BUFFER_OFFSET_END (buffer));
  if (ctx->next_out_off != GST_BUFFER_OFFSET_NONE) {
    fail_unless (GST_BUFFER_OFFSET (buffer) == ctx->next_out_off,
        "expected offset %" G_GUINT64_FORMAT " got offset %" G_GUINT64_FORMAT,
        ctx->next_out_off, GST_BUFFER_OFFSET (buffer));
  }

  if (ctx->in_buffer_count != ctx->out_buffer_count) {
    GST_INFO ("timestamp %" GST_TIME_FORMAT,
        GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buffer)));
  }

  if (ctx->in_ts != GST_CLOCK_TIME_NONE && ctx->in_buffer_count > 1
      && ctx->in_buffer_count == ctx->out_buffer_count) {
    fail_unless (GST_BUFFER_TIMESTAMP (buffer) ==
        ctx->in_ts - gst_util_uint64_scale_round (ctx->latency, GST_SECOND,
            4096),
        "expected output timestamp %" GST_TIME_FORMAT " (%" G_GUINT64_FORMAT
        ") got output timestamp %" GST_TIME_FORMAT " (%" G_GUINT64_FORMAT ")",
        GST_TIME_ARGS (ctx->in_ts - gst_util_uint64_scale_round (ctx->latency,
                GST_SECOND, 4096)),
        ctx->in_ts - gst_util_uint64_scale_round (ctx->latency, GST_SECOND,
            4096), GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buffer)),
        GST_BUFFER_TIMESTAMP (buffer));
  }

  ctx->next_out_ts =
      GST_BUFFER_TIMESTAMP (buffer) + GST_BUFFER_DURATION (buffer);
  ctx->next_out_off = GST_BUFFER_OFFSET_END (buffer);
}

static void
identity_handoff_cb (GstElement * object, GstBuffer * buffer,
    gpointer user_data)
{
  TimestampDriftCtx *ctx = user_data;

  ctx->in_ts = GST_BUFFER_TIMESTAMP (buffer);
  ctx->in_buffer_count++;
}

GST_START_TEST (test_timestamp_drift)
{
  TimestampDriftCtx ctx =
      { GST_CLOCK_TIME_NONE, GST_CLOCK_TIME_NONE, GST_CLOCK_TIME_NONE,
    GST_BUFFER_OFFSET_NONE, 0, 0
  };
  GstElement *pipeline;
  GstElement *audiotestsrc, *capsfilter1, *identity, *audioresample,
      *capsfilter2, *fakesink;
  GstBus *bus;
  GMainLoop *loop;
  GstCaps *caps;

  pipeline = gst_pipeline_new ("pipeline");
  fail_unless (pipeline != NULL);

  audiotestsrc = gst_element_factory_make ("audiotestsrc", "src");
  fail_unless (audiotestsrc != NULL);
  g_object_set (G_OBJECT (audiotestsrc), "num-buffers", 10000,
      "samplesperbuffer", 4000, NULL);

  capsfilter1 = gst_element_factory_make ("capsfilter", "capsfilter1");
  fail_unless (capsfilter1 != NULL);
  caps = gst_caps_from_string ("audio/x-raw, format=" GST_AUDIO_NE (F64)
      ", channels=1, rate=16384");
  g_object_set (G_OBJECT (capsfilter1), "caps", caps, NULL);
  gst_caps_unref (caps);

  identity = gst_element_factory_make ("identity", "identity");
  fail_unless (identity != NULL);
  g_object_set (G_OBJECT (identity), "sync", FALSE, "signal-handoffs", TRUE,
      NULL);
  g_signal_connect (identity, "handoff", (GCallback) identity_handoff_cb, &ctx);

  audioresample = gst_element_factory_make ("audioresample", "resample");
  fail_unless (audioresample != NULL);
  capsfilter2 = gst_element_factory_make ("capsfilter", "capsfilter2");
  fail_unless (capsfilter2 != NULL);
  caps = gst_caps_from_string ("audio/x-raw, format=" GST_AUDIO_NE (F64)
      ", channels=1, rate=4096");
  g_object_set (G_OBJECT (capsfilter2), "caps", caps, NULL);
  gst_caps_unref (caps);

  fakesink = gst_element_factory_make ("fakesink", "sink");
  fail_unless (fakesink != NULL);
  g_object_set (G_OBJECT (fakesink), "sync", FALSE, "async", FALSE,
      "signal-handoffs", TRUE, NULL);
  g_signal_connect (fakesink, "handoff", (GCallback) fakesink_handoff_cb, &ctx);


  gst_bin_add_many (GST_BIN (pipeline), audiotestsrc, capsfilter1, identity,
      audioresample, capsfilter2, fakesink, NULL);
  fail_unless (gst_element_link_many (audiotestsrc, capsfilter1, identity,
          audioresample, capsfilter2, fakesink, NULL));

  loop = g_main_loop_new (NULL, FALSE);

  bus = gst_element_get_bus (pipeline);
  gst_bus_add_signal_watch (bus);
  g_signal_connect (bus, "message", (GCallback) _message_cb, loop);

  fail_unless (gst_element_set_state (pipeline,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS);
  g_main_loop_run (loop);

  fail_unless (gst_element_set_state (pipeline,
          GST_STATE_NULL) == GST_STATE_CHANGE_SUCCESS);
  g_main_loop_unref (loop);
  gst_bus_remove_signal_watch (bus);
  gst_object_unref (bus);

  gst_object_unref (pipeline);

} GST_END_TEST;

#define FFT_HELPERS(type,ffttag,ffttag2,scale);                                                 \
static gdouble magnitude##ffttag (const GstFFT##ffttag##Complex *c)                             \
{                                                                                               \
  gdouble mag = (gdouble) c->r * (gdouble) c->r;                                                \
  mag += (gdouble) c->i * (gdouble) c->i;                                                       \
  mag /= scale * scale;                                                                         \
  mag = 10.0 * log10 (mag);                                                                     \
  return mag;                                                                                   \
}                                                                                               \
static gdouble find_main_frequency_spot_##ffttag (const GstFFT##ffttag##Complex *v,             \
                                                  int elements)                                 \
{                                                                                               \
  int i;                                                                                        \
  gdouble maxmag = -9999;                                                                       \
  int maxidx = 0;                                                                               \
  for (i=0; i<elements; ++i) {                                                                  \
    gdouble mag = magnitude##ffttag (v+i);                                                      \
    if (mag > maxmag) {                                                                         \
      maxmag = mag;                                                                             \
      maxidx = i;                                                                               \
    }                                                                                           \
  }                                                                                             \
  return maxidx / (gdouble) elements;                                                           \
}                                                                                               \
static gboolean is_zero_except_##ffttag (const GstFFT##ffttag##Complex *v, int elements,        \
                                gdouble spot)                                                   \
{                                                                                               \
  int i;                                                                                        \
  for (i=0; i<elements; ++i) {                                                                  \
    gdouble pos = i / (gdouble) elements;                                                       \
    gdouble mag = magnitude##ffttag (v+i);                                                      \
    if (fabs (pos - spot) > 0.01) {                                                             \
      if (mag > -55.0) {                                                                        \
        return FALSE;                                                                           \
      }                                                                                         \
    }                                                                                           \
  }                                                                                             \
  return TRUE;                                                                                  \
}                                                                                               \
static void compare_ffts_##ffttag (GstBuffer *inbuffer, GstBuffer *outbuffer)                   \
{                                                                                               \
  GstMapInfo inmap, outmap;                                                                     \
  int insamples, outsamples;                                                                    \
  gdouble inspot, outspot;                                                                      \
  GstFFT##ffttag *inctx, *outctx;                                                               \
  GstFFT##ffttag##Complex *in, *out;                                                            \
                                                                                                \
  gst_buffer_map (inbuffer, &inmap, GST_MAP_READ);                                              \
  gst_buffer_map (outbuffer, &outmap, GST_MAP_READWRITE);                                       \
                                                                                                \
  insamples = inmap.size / sizeof(type) & ~1;                                                   \
  outsamples = outmap.size / sizeof(type) & ~1;                                                 \
  inctx = gst_fft_##ffttag2##_new (insamples, FALSE);                                           \
  outctx = gst_fft_##ffttag2##_new (outsamples, FALSE);                                         \
  in = g_new (GstFFT##ffttag##Complex, insamples / 2 + 1);                                      \
  out = g_new (GstFFT##ffttag##Complex, outsamples / 2 + 1);                                    \
                                                                                                \
  gst_fft_##ffttag2##_window (inctx, (type*)inmap.data,                                         \
      GST_FFT_WINDOW_HAMMING);                                                                  \
  gst_fft_##ffttag2##_fft (inctx, (type*)inmap.data, in);                                       \
  gst_fft_##ffttag2##_window (outctx, (type*)outmap.data,                                       \
      GST_FFT_WINDOW_HAMMING);                                                                  \
  gst_fft_##ffttag2##_fft (outctx, (type*)outmap.data, out);                                    \
                                                                                                \
  inspot = find_main_frequency_spot_##ffttag (in, insamples / 2 + 1);                           \
  outspot = find_main_frequency_spot_##ffttag (out, outsamples / 2 + 1);                        \
  GST_LOG ("Spots are %.3f and %.3f", inspot, outspot);                                         \
  fail_unless (fabs (outspot - inspot) < 0.05);                                                 \
  fail_unless (is_zero_except_##ffttag (in, insamples / 2 + 1, inspot));                        \
  fail_unless (is_zero_except_##ffttag (out, outsamples / 2 + 1, outspot));                     \
                                                                                                \
  gst_buffer_unmap (inbuffer, &inmap);                                                          \
  gst_buffer_unmap (outbuffer, &outmap);                                                        \
                                                                                                \
  gst_fft_##ffttag2##_free (inctx);                                                             \
  gst_fft_##ffttag2##_free (outctx);                                                            \
  g_free (in);                                                                                  \
  g_free (out);                                                                                 \
}
FFT_HELPERS (float, F32, f32, 2048.0f);
FFT_HELPERS (double, F64, f64, 2048.0);
FFT_HELPERS (gint16, S16, s16, 32767.0);
FFT_HELPERS (gint32, S32, s32, 2147483647.0);

#define FILL_BUFFER(type, desc, value);                         \
  static void init_##type##_##desc (GstBuffer *buffer)          \
  {                                                             \
    GstMapInfo map;                                             \
    type *ptr;                                                  \
    int i, nsamples;                                            \
    gst_buffer_map (buffer, &map, GST_MAP_WRITE);               \
    ptr = (type *)map.data;                                     \
    nsamples = map.size / sizeof (type);                        \
    for (i = 0; i < nsamples; ++i) {                            \
      *ptr++ = value;                                           \
    }                                                           \
    gst_buffer_unmap (buffer, &map);                            \
  }

FILL_BUFFER (float, silence, 0.0f);
FILL_BUFFER (double, silence, 0.0);
FILL_BUFFER (gint16, silence, 0);
FILL_BUFFER (gint32, silence, 0);
FILL_BUFFER (float, sine, sinf (i * 0.01f));
FILL_BUFFER (float, sine2, sinf (i * 1.8f));
FILL_BUFFER (double, sine, sin (i * 0.01));
FILL_BUFFER (double, sine2, sin (i * 1.8));
FILL_BUFFER (gint16, sine, (gint16) (32767 * sinf (i * 0.01f)));
FILL_BUFFER (gint16, sine2, (gint16) (32767 * sinf (i * 1.8f)));
FILL_BUFFER (gint32, sine, (gint32) (2147483647.0 * sin (i * 0.01)));
FILL_BUFFER (gint32, sine2, (gint32) (2147483647.0 * sin (i * 1.8)));

static void
run_fft_pipeline (int inrate, int outrate, int quality, int width,
    const gchar * format, void (*init) (GstBuffer *),
    void (*compare_ffts) (GstBuffer *, GstBuffer *))
{
  GstElement *audioresample;
  GstBuffer *inbuffer, *outbuffer;
  const int nsamples = 2048;

  audioresample = setup_audioresample (1, 0, inrate, outrate, format);
  fail_unless (audioresample != NULL);
  g_object_set (audioresample, "quality", quality, NULL);

  fail_unless (gst_element_set_state (audioresample,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  inbuffer = gst_buffer_new_and_alloc (nsamples * width / 8);
  GST_BUFFER_DURATION (inbuffer) = GST_FRAMES_TO_CLOCK_TIME (nsamples, inrate);
  GST_BUFFER_TIMESTAMP (inbuffer) = 0;

  (*init) (inbuffer);

  gst_buffer_ref (inbuffer);
  /* pushing gives away my reference ... */
  fail_unless (gst_pad_push (mysrcpad, inbuffer) == GST_FLOW_OK);
  /* ... but it ends up being collected on the global buffer list */
  fail_unless_equals_int (g_list_length (buffers), 1);
  /* retrieve out buffer */
  fail_if ((outbuffer = (GstBuffer *) buffers->data) == NULL);

  fail_unless (gst_element_set_state (audioresample,
          GST_STATE_NULL) == GST_STATE_CHANGE_SUCCESS, "could not set to null");

  if (inbuffer == outbuffer)
    gst_buffer_unref (inbuffer);

  (*compare_ffts) (inbuffer, outbuffer);

  /* cleanup */
  cleanup_audioresample (audioresample);
}

GST_START_TEST (test_fft)
{
  int quality;
  size_t f0, f1;
  static const int frequencies[] =
      { 8000, 16000, 44100, 48000, 128000, 12345, 54321 };

  /* audioresample uses a mixed float/double code path for floats with quality>8, make sure we test it */
  for (quality = 0; quality <= 10; quality += 5) {
    for (f0 = 0; f0 < G_N_ELEMENTS (frequencies); ++f0) {
      for (f1 = 0; f1 < G_N_ELEMENTS (frequencies); ++f1) {
        run_fft_pipeline (frequencies[f0], frequencies[f0], quality, 32,
            GST_AUDIO_NE (F32), &init_float_silence, &compare_ffts_F32);
        run_fft_pipeline (frequencies[f0], frequencies[f0], quality, 32,
            GST_AUDIO_NE (F32), &init_float_sine, &compare_ffts_F32);
        run_fft_pipeline (frequencies[f0], frequencies[f0], quality, 32,
            GST_AUDIO_NE (F32), &init_float_sine2, &compare_ffts_F32);
        run_fft_pipeline (frequencies[f0], frequencies[f0], quality, 64,
            GST_AUDIO_NE (F64), &init_double_silence, &compare_ffts_F64);
        run_fft_pipeline (frequencies[f0], frequencies[f0], quality, 64,
            GST_AUDIO_NE (F64), &init_double_sine, &compare_ffts_F64);
        run_fft_pipeline (frequencies[f0], frequencies[f0], quality, 64,
            GST_AUDIO_NE (F64), &init_double_sine2, &compare_ffts_F64);
        run_fft_pipeline (frequencies[f0], frequencies[f0], quality, 16,
            GST_AUDIO_NE (S16), &init_gint16_silence, &compare_ffts_S16);
        run_fft_pipeline (frequencies[f0], frequencies[f0], quality, 16,
            GST_AUDIO_NE (S16), &init_gint16_sine, &compare_ffts_S16);
        run_fft_pipeline (frequencies[f0], frequencies[f0], quality, 16,
            GST_AUDIO_NE (S16), &init_gint16_sine2, &compare_ffts_S16);
        run_fft_pipeline (frequencies[f0], frequencies[f0], quality, 32,
            GST_AUDIO_NE (S32), &init_gint32_silence, &compare_ffts_S32);
        run_fft_pipeline (frequencies[f0], frequencies[f0], quality, 32,
            GST_AUDIO_NE (S32), &init_gint32_sine, &compare_ffts_S32);
        run_fft_pipeline (frequencies[f0], frequencies[f0], quality, 32,
            GST_AUDIO_NE (S32), &init_gint32_sine2, &compare_ffts_S32);
      }
    }
  }
}

GST_END_TEST;

static Suite *
audioresample_suite (void)
{
  Suite *s = suite_create ("audioresample");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_perfect_stream);
  tcase_add_test (tc_chain, test_discont_stream);
  tcase_add_test (tc_chain, test_reuse);
  tcase_add_test (tc_chain, test_shutdown);
  tcase_add_test (tc_chain, test_live_switch);
  tcase_add_test (tc_chain, test_live_switch_downstream);
  tcase_add_test (tc_chain, test_timestamp_drift);
  tcase_add_test (tc_chain, test_fft);

#ifndef GST_DISABLE_PARSE
  tcase_set_timeout (tc_chain, 360);
  tcase_add_test (tc_chain, test_pipelines);
  tcase_add_test (tc_chain, test_preference_passthrough);
#endif

  return s;
}

GST_CHECK_MAIN (audioresample);
