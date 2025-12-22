/* GStreamer
 *
 * Unit tests for lc3enc and lc3dec
 *
 * Copyright (C) <2023> Asymptotic Inc. <taruntej@asymptotic.io>
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

#include <string.h>
#include <unistd.h>

#include <gst/gst.h>
#include <gst/check/gstcheck.h>
#include <gst/check/gstharness.h>
#include <gst/audio/audio.h>

/* For ease of programming we use globals to keep refs for our floating
 * src and sink pads we create; otherwise we always have to do get_pad,
 * get_peer, and then remove references in every test function
 * `srcpad` is the peer pad of lc3enc sinkpad upstream
 * `sinkpad` is the peer pad of lc3enc srcpad downstream
 */
static GstPad *srcpad, *sinkpad;

#define SAMPLE_RATES "8000, 16000, 24000, 32000, 48000"
#define FORMATS "S16LE, S24LE, S32LE, F32LE"

#define FRAME_DURATION_10000US 10000
#define FRAME_DURATION_7500US 7500
#define FRAME_BYTES_MIN 20
#define FRAME_BYTES_MAX 400

#define FRAME_DURATIONS G_STRINGIFY(FRAME_DURATION_10000US) ", " G_STRINGIFY(FRAME_DURATION_7500US)
#define FRAME_BYTES_RANGE  G_STRINGIFY(FRAME_BYTES_MIN) ", " G_STRINGIFY(FRAME_BYTES_MAX)

#define RAW_AUDIO_CAPS_STRING "audio/x-raw, " \
        "format = { " FORMATS "  }, " \
        "rate = (int) { " SAMPLE_RATES  " }, " "channels = (int) [1, MAX]" \
        ", layout=(string)interleaved"

#define LC3_AUDIO_CAPS_STRING "audio/x-lc3, " \
        "rate = (int) { " SAMPLE_RATES " }, " \
        "channels = (int) [1, MAX], " \
        "frame-bytes = (int) [" FRAME_BYTES_RANGE "], " \
        "frame-duration-us = (int) { " FRAME_DURATIONS "}, " \
        "framed=(boolean) true"

#define LC3_AUDIO_CAPS_STRING_10000US "audio/x-lc3, " \
        "rate = (int) { " SAMPLE_RATES " }, " \
        "channels = (int) [1, MAX], " \
        "frame-bytes = (int) [" FRAME_BYTES_RANGE "], " \
        "frame-duration-us = (int) 10000, " \
        "framed=(boolean) true"

#define LC3_AUDIO_CAPS_STRING_7500US "audio/x-lc3, " \
        "rate = (int) { " SAMPLE_RATES " }, " \
        "channels = (int) [1, MAX], " \
        "frame-bytes = (int) [" FRAME_BYTES_RANGE "], " \
        "frame-duration-us = (int)  7500, " \
        "framed=(boolean) true"

static GstStaticPadTemplate sinktemplate_10000us =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (LC3_AUDIO_CAPS_STRING_10000US));

static GstStaticPadTemplate sinktemplate_7500us =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (LC3_AUDIO_CAPS_STRING_7500US));

static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (RAW_AUDIO_CAPS_STRING));

static GstStaticPadTemplate dec_srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (LC3_AUDIO_CAPS_STRING));

static GstStaticPadTemplate dec_sinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (RAW_AUDIO_CAPS_STRING));

static GstElement *
setup_lc3enc (GstStaticPadTemplate * srctmpl, GstStaticPadTemplate * sinktmpl)
{
  GstElement *lc3enc;

  GST_DEBUG ("setup_lc3enc");
  lc3enc = gst_check_setup_element ("lc3enc");

  srcpad = gst_check_setup_src_pad (lc3enc, srctmpl);
  sinkpad = gst_check_setup_sink_pad (lc3enc, sinktmpl);

  gst_pad_set_active (srcpad, TRUE);
  gst_pad_set_active (sinkpad, TRUE);

  return lc3enc;
}


static void
cleanup_lc3enc (GstElement * lc3enc)
{
  GST_DEBUG ("cleanup_lc3enc");
  gst_element_set_state (lc3enc, GST_STATE_NULL);

  gst_pad_set_active (srcpad, FALSE);
  gst_pad_set_active (sinkpad, FALSE);

  gst_check_teardown_src_pad (lc3enc);
  gst_check_teardown_sink_pad (lc3enc);
  gst_check_teardown_element (lc3enc);
}

static int
get_delay_samples (gint rate, gint frame_duration_us)
{
  /* delay is
     - 2.5ms for 10ms
     - 4ms for 7.5ms
   */
  int frame_samples = (rate * frame_duration_us) / GST_MSECOND;

  if (frame_duration_us == 10000) {
    return (2.5 * frame_samples) / 10;
  } else if (frame_duration_us == 7500) {
    return (4 * frame_samples) / 7.5;
  }
  return 0;
}

static void
enc_buffer_test (gint rate, gint channels, gint nbuffers, int frame_dur_us)
{
  GstElement *lc3enc;
  GstBuffer *inbuffer, *outbuffer;
  GstCaps *caps;
  guint i, num_buffers;
  GstStructure *s;
  gint frame_duration_us;
  gint frame_bytes;
  guint outbuffers;
  guint64 chmask;
  int num;
  GstStaticPadTemplate *sinktmpl =
      frame_dur_us == 7500 ? &sinktemplate_7500us : &sinktemplate_10000us;

  /* cleanup any stale buffers from previous run */
  g_list_free (buffers);
  buffers = NULL;

  lc3enc = setup_lc3enc (&srctemplate, sinktmpl);
  fail_unless (gst_element_set_state (lc3enc,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  /* corresponds to audio buffer mentioned in the caps */
  inbuffer = gst_buffer_new_and_alloc (1024 * nbuffers * 2 * channels);
  /* makes valgrind's memcheck happier */
  gst_buffer_memset (inbuffer, 0, '1', 1024 * nbuffers * 2 * channels);

  chmask = gst_audio_channel_get_fallback_mask (channels);

  caps =
      gst_caps_new_simple ("audio/x-raw", "rate", G_TYPE_INT, rate, "channels",
      G_TYPE_INT, channels, "format", G_TYPE_STRING, "S16LE", "layout",
      G_TYPE_STRING, "interleaved", "channel-mask", GST_TYPE_BITMASK, chmask,
      NULL);

  gst_check_setup_events (srcpad, lc3enc, caps, GST_FORMAT_TIME);
  gst_caps_unref (caps);

  GST_BUFFER_TIMESTAMP (inbuffer) = 0;
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  fail_unless (gst_pad_push (srcpad, inbuffer) == GST_FLOW_OK);

  /* send eos to have all flushed if needed */
  fail_unless (gst_pad_push_event (srcpad, gst_event_new_eos ()) == TRUE);

  /* get the caps on the srcpad of lc3enc */
  fail_unless (gst_pad_has_current_caps (sinkpad));
  caps = gst_pad_get_current_caps (sinkpad);
  fail_unless (gst_caps_is_fixed (caps));
  s = gst_caps_get_structure (caps, 0);
  fail_if (s == NULL);

  gst_structure_get_int (s, "frame-duration-us", &frame_duration_us);
  fail_unless (frame_duration_us == frame_dur_us);
  gst_structure_get_int (s, "frame-bytes", &frame_bytes);
  fail_if (frame_bytes == 0);

  gst_caps_unref (caps);
  num = get_delay_samples (rate, frame_duration_us) + (1024 * nbuffers);
  outbuffers =
      gst_util_uint64_scale_int_ceil (1, num,
      ((rate * frame_duration_us) / GST_MSECOND));
  num_buffers = g_list_length (buffers);
  /* check the num of outbuffers is as expected */
  fail_unless_equals_int (num_buffers, outbuffers);

  /* clean up buffers */
  for (i = 0; i < num_buffers; ++i) {
    GstMapInfo map;
    gsize size;

    outbuffer = GST_BUFFER (buffers->data);
    fail_if (outbuffer == NULL);

    gst_buffer_map (outbuffer, &map, GST_MAP_READ);
    size = map.size;

    /* check the size of each out buffer per channel
     * is same as frame_bytes
     */
    fail_unless_equals_int (size / channels, frame_bytes);

    gst_buffer_unmap (outbuffer, &map);

    buffers = g_list_remove (buffers, outbuffer);

    ASSERT_BUFFER_REFCOUNT (outbuffer, "outbuffer", 1);
    gst_buffer_unref (outbuffer);
    outbuffer = NULL;
  }

  cleanup_lc3enc (lc3enc);
  g_list_free (buffers);
  buffers = NULL;
}

static GstBuffer *
create_test_buffer (guint64 num, guint64 size)
{
  GstBuffer *buffer;
  guint64 *data = g_malloc (sizeof (guint64));

  *data = num;

  buffer = gst_buffer_new_wrapped (data, size);

  GST_BUFFER_PTS (buffer) = num * 10 * GST_MSECOND;
  GST_BUFFER_DURATION (buffer) = 10 * GST_MSECOND;

  return buffer;
}

static void
dec_plc_test (void)
{
  GstElement *dec;
  GstClockTime pts;
  GstClockTime dur;
  GstBuffer *buf;
  GstHarness *h;
  GstCaps *caps;

  dec = gst_check_setup_element ("lc3dec");
  g_object_set (dec, "plc", TRUE, NULL);

  dur = GST_MSECOND * 10;
  h = gst_harness_new_full (dec, &dec_srctemplate, "sink", &dec_sinktemplate,
      "src");
  caps =
      gst_caps_from_string
      ("audio/x-lc3,channels=2,frame-bytes=100,frame-duration-us=10000,framed=true,rate=48000");

  gst_check_setup_events (h->srcpad, dec, caps, GST_FORMAT_TIME);
  gst_caps_unref (caps);

  pts = 0;
  gst_harness_push (h, create_test_buffer (0, 2 * 100));
  buf = gst_harness_pull (h);
  fail_unless_equals_int (pts, GST_BUFFER_PTS (buf));
  fail_unless_equals_int (dur, GST_BUFFER_DURATION (buf));
  /* first buffer is marked with discont in the gstaudiodecoder */
  fail_unless (GST_BUFFER_FLAG_IS_SET (buf, GST_BUFFER_FLAG_DISCONT));
  gst_buffer_unref (buf);

  pts = 10 * GST_MSECOND;
  fail_unless (gst_harness_push_event (h, gst_event_new_gap (pts, dur)));
  buf = gst_harness_try_pull (h);
  fail_unless (buf);
  fail_unless_equals_int (pts, GST_BUFFER_PTS (buf));
  fail_unless_equals_int (dur, GST_BUFFER_DURATION (buf));
  fail_unless (!GST_BUFFER_FLAG_IS_SET (buf, GST_BUFFER_FLAG_DISCONT));
  gst_buffer_unref (buf);

  pts = 2 * 10 * GST_MSECOND;
  buf = create_test_buffer (2, 2 * 100);
  GST_BUFFER_FLAG_SET (buf, GST_BUFFER_FLAG_DISCONT);
  gst_harness_push (h, buf);
  buf = gst_harness_pull (h);
  fail_unless_equals_int (pts, GST_BUFFER_PTS (buf));
  fail_unless_equals_int (dur, GST_BUFFER_DURATION (buf));
  fail_unless (!GST_BUFFER_FLAG_IS_SET (buf, GST_BUFFER_FLAG_DISCONT));

  gst_buffer_unref (buf);
  gst_harness_teardown (h);
}

GST_START_TEST (test_48k_8ch_10000us)
{
  enc_buffer_test (48000, 8, 100, 10000);
}

GST_END_TEST;

GST_START_TEST (test_48k_8ch_7500us)
{
  enc_buffer_test (48000, 8, 100, 7500);
}

GST_END_TEST;

GST_START_TEST (test_24k_4ch_10000us)
{
  enc_buffer_test (24000, 4, 150, 10000);
}

GST_END_TEST;

GST_START_TEST (test_dec_plc)
{
  dec_plc_test ();
}

GST_END_TEST;

static Suite *
lc3_suite (void)
{
  Suite *s = suite_create ("lc3");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_48k_8ch_10000us);
  tcase_add_test (tc_chain, test_48k_8ch_7500us);
  tcase_add_test (tc_chain, test_24k_4ch_10000us);
  tcase_add_test (tc_chain, test_dec_plc);

  return s;
}

GST_CHECK_MAIN (lc3);
