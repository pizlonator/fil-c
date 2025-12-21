/* GStreamer
 *
 * unit test for opus
 *
 * Copyright (C) <2011> Vincent Penquerc'h <vincent.penquerch@collbaora.co.uk>
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
#include <gst/check/gstharness.h>

#if G_BYTE_ORDER == G_BIG_ENDIAN
#define AFORMAT "S16BE"
#else
#define AFORMAT "S16LE"
#endif

#define AUDIO_CAPS_STRING "audio/x-raw, " \
                           "format = (string) " AFORMAT ", "\
                           "layout = (string) interleaved, " \
                           "rate = (int) 48000, " \
                           "channels = (int) 1 "

/* A lot of these taken from the vorbisdec test */

/* For ease of programming we use globals to keep refs for our floating
 * src and sink pads we create; otherwise we always have to do get_pad,
 * get_peer, and then remove references in every test function */
static GstPad *mydecsrcpad, *mydecsinkpad;
static GstPad *myencsrcpad, *myencsinkpad;

static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);
static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

static GstElement *
setup_opusdec (void)
{
  GstElement *opusdec;

  GST_DEBUG ("setup_opusdec");
  opusdec = gst_check_setup_element ("opusdec");
  mydecsrcpad = gst_check_setup_src_pad (opusdec, &srctemplate);
  mydecsinkpad = gst_check_setup_sink_pad (opusdec, &sinktemplate);
  gst_pad_set_active (mydecsrcpad, TRUE);
  gst_pad_set_active (mydecsinkpad, TRUE);

  return opusdec;
}

static void
cleanup_opusdec (GstElement * opusdec)
{
  GST_DEBUG ("cleanup_opusdec");
  gst_element_set_state (opusdec, GST_STATE_NULL);

  gst_pad_set_active (mydecsrcpad, FALSE);
  gst_pad_set_active (mydecsinkpad, FALSE);
  gst_check_teardown_src_pad (opusdec);
  gst_check_teardown_sink_pad (opusdec);
  gst_check_teardown_element (opusdec);
}

static GstElement *
setup_opusenc (void)
{
  GstElement *opusenc;

  GST_DEBUG ("setup_opusenc");
  opusenc = gst_check_setup_element ("opusenc");
  myencsrcpad = gst_check_setup_src_pad (opusenc, &srctemplate);
  myencsinkpad = gst_check_setup_sink_pad (opusenc, &sinktemplate);
  gst_pad_set_active (myencsrcpad, TRUE);
  gst_pad_set_active (myencsinkpad, TRUE);

  return opusenc;
}

static void
cleanup_opusenc (GstElement * opusenc)
{
  GST_DEBUG ("cleanup_opusenc");
  gst_element_set_state (opusenc, GST_STATE_NULL);

  gst_pad_set_active (myencsrcpad, FALSE);
  gst_pad_set_active (myencsinkpad, FALSE);
  gst_check_teardown_src_pad (opusenc);
  gst_check_teardown_sink_pad (opusenc);
  gst_check_teardown_element (opusenc);
}

static void
check_buffers (guint expected)
{
  GstBuffer *outbuffer;
  guint i, num_buffers;

  /* check buffers are the type we expect */
  num_buffers = g_list_length (buffers);
  fail_unless (num_buffers >= expected);
  for (i = 0; i < num_buffers; ++i) {
    outbuffer = GST_BUFFER (buffers->data);
    fail_if (outbuffer == NULL);
    fail_if (gst_buffer_get_size (outbuffer) == 0);

    buffers = g_list_remove (buffers, outbuffer);

    ASSERT_BUFFER_REFCOUNT (outbuffer, "outbuffer", 1);
    gst_buffer_unref (outbuffer);
    outbuffer = NULL;
  }
}

GST_START_TEST (test_opus_encode_nothing)
{
  GstElement *opusenc;

  opusenc = setup_opusenc ();
  fail_unless (gst_element_set_state (opusenc,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  fail_unless (gst_pad_push_event (myencsrcpad, gst_event_new_eos ()) == TRUE);

  fail_unless (gst_element_set_state (opusenc,
          GST_STATE_READY) == GST_STATE_CHANGE_SUCCESS,
      "could not set to ready");

  /* cleanup */
  cleanup_opusenc (opusenc);
}

GST_END_TEST;

GST_START_TEST (test_opus_decode_nothing)
{
  GstElement *opusdec;

  opusdec = setup_opusdec ();
  fail_unless (gst_element_set_state (opusdec,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  fail_unless (gst_pad_push_event (mydecsrcpad, gst_event_new_eos ()) == TRUE);

  fail_unless (gst_element_set_state (opusdec,
          GST_STATE_READY) == GST_STATE_CHANGE_SUCCESS,
      "could not set to ready");

  /* cleanup */
  cleanup_opusdec (opusdec);
}

GST_END_TEST;

GST_START_TEST (test_opus_encode_samples)
{
  const unsigned int nsamples = 4096;
  GstElement *opusenc;
  GstBuffer *inbuffer;
  GstCaps *caps;

  opusenc = setup_opusenc ();

  fail_unless (gst_element_set_state (opusenc,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  inbuffer = gst_buffer_new_and_alloc (nsamples * 2);
  gst_buffer_memset (inbuffer, 0, 0, nsamples * 2);

  GST_BUFFER_TIMESTAMP (inbuffer) = GST_BUFFER_OFFSET (inbuffer) = 0;
  GST_BUFFER_DURATION (inbuffer) = GST_CLOCK_TIME_NONE;
  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);

  caps = gst_caps_from_string (AUDIO_CAPS_STRING);
  fail_unless (caps != NULL);
  gst_check_setup_events (myencsrcpad, opusenc, caps, GST_FORMAT_TIME);
  gst_caps_unref (caps);
  gst_buffer_ref (inbuffer);

  /* pushing gives away my reference ... */
  fail_unless (gst_pad_push (myencsrcpad, inbuffer) == GST_FLOW_OK);
  /* ... and nothing ends up on the global buffer list */
  fail_unless (gst_pad_push_event (myencsrcpad, gst_event_new_eos ()) == TRUE);

  ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
  gst_buffer_unref (inbuffer);

  fail_unless (gst_element_set_state (opusenc,
          GST_STATE_READY) == GST_STATE_CHANGE_SUCCESS,
      "could not set to ready");

  /* default frame size is 20 ms, at 48000 Hz that's 960 samples */
  check_buffers ((nsamples + 959) / 960);

  /* cleanup */
  cleanup_opusenc (opusenc);
  g_list_free (buffers);
}

GST_END_TEST;

GST_START_TEST (test_opus_encode_properties)
{
  const unsigned int nsamples = 4096;
  enum
  { steps = 20 };
  GstElement *opusenc;
  GstBuffer *inbuffer;
  GstCaps *caps;
  unsigned int step;
  static const struct
  {
    const char *param;
    int value;
  } param_changes[steps] = {
    {
        "frame-size", 40}, {
        "inband-fec", 1}, {
        "complexity", 5}, {
        "bandwidth", 1104}, {
        "frame-size", 2}, {
        "max-payload-size", 80}, {
        "frame-size", 60}, {
        "max-payload-size", 900}, {
        "complexity", 1}, {
        "bitrate", 30000}, {
        "frame-size", 10}, {
        "bitrate", 300000}, {
        "inband-fec", 0}, {
        "frame-size", 5}, {
        "bandwidth", 1101}, {
        "frame-size", 10}, {
        "bitrate", 500000}, {
        "frame-size", 5}, {
        "bitrate", 80000}, {
        "complexity", 8},
  };

  opusenc = setup_opusenc ();

  fail_unless (gst_element_set_state (opusenc,
          GST_STATE_PLAYING) == GST_STATE_CHANGE_SUCCESS,
      "could not set to playing");

  caps = gst_caps_from_string (AUDIO_CAPS_STRING);
  fail_unless (caps != NULL);

  gst_check_setup_events (myencsrcpad, opusenc, caps, GST_FORMAT_TIME);

  for (step = 0; step < steps; ++step) {
    GstSegment segment;

    gst_segment_init (&segment, GST_FORMAT_TIME);
    gst_pad_push_event (myencsrcpad, gst_event_new_segment (&segment));

    inbuffer = gst_buffer_new_and_alloc (nsamples * 2);
    gst_buffer_memset (inbuffer, 0, 0, nsamples * 2);

    GST_BUFFER_TIMESTAMP (inbuffer) = GST_BUFFER_OFFSET (inbuffer) = 0;
    GST_BUFFER_DURATION (inbuffer) = GST_CLOCK_TIME_NONE;
    ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);

    gst_buffer_ref (inbuffer);

    /* pushing gives away my reference ... */
    fail_unless (gst_pad_push (myencsrcpad, inbuffer) == GST_FLOW_OK);
    /* ... and nothing ends up on the global buffer list */
    fail_unless (gst_pad_push_event (myencsrcpad,
            gst_event_new_eos ()) == TRUE);

    ASSERT_BUFFER_REFCOUNT (inbuffer, "inbuffer", 1);
    gst_buffer_unref (inbuffer);

    /* change random parameters */
    g_object_set (opusenc, param_changes[step].param, param_changes[step].value,
        NULL);

    check_buffers (1);

    fail_unless (gst_pad_push_event (myencsrcpad,
            gst_event_new_flush_start ()) == TRUE);
    fail_unless (gst_pad_push_event (myencsrcpad,
            gst_event_new_flush_stop (TRUE)) == TRUE);
  }

  gst_caps_unref (caps);

  fail_unless (gst_element_set_state (opusenc,
          GST_STATE_READY) == GST_STATE_CHANGE_SUCCESS,
      "could not set to ready");

  /* cleanup */
  cleanup_opusenc (opusenc);
  g_list_free (buffers);
}

GST_END_TEST;

/* removes fields that do not interest our tests to
 * allow using gst_caps_is_equal for comparison */
static GstCaps *
remove_extra_caps_fields (GstCaps * caps)
{
  gint i;
  for (i = 0; i < gst_caps_get_size (caps); i++) {
    GstStructure *s = gst_caps_get_structure (caps, i);

    gst_structure_remove_field (s, "channel-mapping-family");
    gst_structure_remove_field (s, "coupled-count");
    gst_structure_remove_field (s, "stream-count");
  }
  return gst_caps_simplify (caps);
}

static void
run_getcaps_check (GstCaps * filter, GstCaps * downstream_caps,
    GstCaps * expected_result)
{
  GstElement *opusdec;
  GstElement *capsfilter;
  GstPad *sinkpad;
  GstCaps *result;
  gchar *caps_str;

  opusdec = gst_element_factory_make ("opusdec", NULL);
  capsfilter = gst_element_factory_make ("capsfilter", NULL);
  sinkpad = gst_element_get_static_pad (opusdec, "sink");
  fail_unless (gst_element_link (opusdec, capsfilter));

  if (downstream_caps)
    g_object_set (capsfilter, "caps", downstream_caps, NULL);
  result = gst_pad_query_caps (sinkpad, filter);
  result = remove_extra_caps_fields (result);
  caps_str = gst_caps_to_string (result);
  fail_unless (gst_caps_is_equal (expected_result, result),
      "Unexpected output caps: %s", caps_str);

  if (filter)
    gst_caps_unref (filter);
  gst_caps_unref (result);
  gst_caps_unref (expected_result);
  if (downstream_caps)
    gst_caps_unref (downstream_caps);
  gst_object_unref (sinkpad);
  gst_object_unref (opusdec);
  gst_object_unref (capsfilter);
  g_free (caps_str);
}

static void
run_getcaps_check_from_strings (const gchar * filter,
    const gchar * downstream_caps, const gchar * expected_result)
{
  run_getcaps_check (filter ? gst_caps_from_string (filter) : NULL,
      downstream_caps ? gst_caps_from_string (downstream_caps) : NULL,
      gst_caps_from_string (expected_result));
}

GST_START_TEST (test_opusdec_getcaps)
{
  /* default result */
  run_getcaps_check_from_strings (NULL, NULL,
      "audio/x-opus, rate=(int){48000, 24000, 16000, 12000, 8000}, channels=(int)[1,8]");

  /* A single supported rate downstream - should accept any upstream anyway */
  run_getcaps_check_from_strings (NULL, "audio/x-raw, rate=(int)8000",
      "audio/x-opus, rate=(int){48000, 24000, 16000, 12000, 8000}, channels=(int)[1,8]");

  /* Two supported rates (fields as a array, not as a single int) */
  run_getcaps_check_from_strings (NULL, "audio/x-raw, rate=(int){24000, 8000}",
      "audio/x-opus, rate=(int){48000, 24000, 16000, 12000, 8000}, channels=(int)[1,8]");

  /* One supported and one unsupported rate */
  run_getcaps_check_from_strings (NULL, "audio/x-raw, rate=(int){24000, 1000}",
      "audio/x-opus, rate=(int){48000, 24000, 16000, 12000, 8000}, channels=(int)[1,8]");

  /* Unsupported rate */
  run_getcaps_check_from_strings (NULL, "audio/x-raw, rate=(int)1000", "EMPTY");

  /* same tests for channels */
  run_getcaps_check_from_strings (NULL, "audio/x-raw, channels=(int)2",
      "audio/x-opus, rate=(int){48000, 24000, 16000, 12000, 8000}, channels=(int)[1,2]");
  run_getcaps_check_from_strings (NULL, "audio/x-raw, channels=(int)[1, 2]",
      "audio/x-opus, rate=(int){48000, 24000, 16000, 12000, 8000}, channels=(int)[1,2]");
  run_getcaps_check_from_strings (NULL, "audio/x-raw, channels=(int)5000",
      "EMPTY");

  /* Now add filters */

  /* Formats not acceptable */
  run_getcaps_check_from_strings ("audio/x-opus, rate=(int)1000", NULL,
      "EMPTY");
  run_getcaps_check_from_strings ("audio/x-opus, channels=(int)200", NULL,
      "EMPTY");

  /* Should restrict the result of the caps query to the selected rate/channels */
  run_getcaps_check_from_strings ("audio/x-opus, rate=(int)8000", NULL,
      "audio/x-opus, rate=(int)8000, channels=(int)[1,8]");
  run_getcaps_check_from_strings ("audio/x-opus, channels=(int)2", NULL,
      "audio/x-opus, rate=(int){48000, 24000, 16000, 12000, 8000}, channels=(int)2");
}

GST_END_TEST;

GST_START_TEST (test_opus_decode_plc_timestamps_with_fec)
{
  GstBuffer *buf;
  GstHarness *h =
      gst_harness_new_parse ("opusdec use-inband-fec=TRUE plc=TRUE");
  GstClockTime dur0 = GST_MSECOND * 35 / 10;    /* because of lookahead */
  GstClockTime dur = GST_MSECOND * 10;

  gst_harness_add_src_parse (h,
      "audiotestsrc samplesperbuffer=480 is-live=TRUE ! "
      "opusenc frame-size=10 inband-fec=TRUE", TRUE);

  /* Push first buffer from encoder to decoder. It will not be decoded yet
   * because of the delay introduced by FEC */
  gst_harness_src_crank_and_push_many (h, 1, 1);
  fail_unless_equals_int (0, gst_harness_buffers_received (h));

  /* Drop second buffer from encoder and send a GAP event to decoder
   * instead with 2x duration */
  gst_harness_src_crank_and_push_many (h, 1, 0);
  fail_unless (buf = gst_harness_pull (h->src_harness));
  fail_unless (gst_harness_push_event (h,
          gst_event_new_gap (GST_BUFFER_PTS (buf),
              GST_BUFFER_DURATION (buf) * 2)));
  gst_buffer_unref (buf);

  /* Extract first buffer from decoder and verify timstamps */
  fail_unless (buf = gst_harness_pull (h));
  fail_unless_equals_int64 (0, GST_BUFFER_PTS (buf));
  fail_unless_equals_int64 (dur0, GST_BUFFER_DURATION (buf));
  fail_unless (GST_BUFFER_FLAG_IS_SET (buf, GST_BUFFER_FLAG_DISCONT));
  gst_buffer_unref (buf);

  /* Third buffer is pushed from encoder to decoder with DISCONT set */
  gst_harness_src_crank_and_push_many (h, 1, 0);
  fail_unless (buf = gst_harness_pull (h->src_harness));
  GST_BUFFER_FLAG_SET (buf, GST_BUFFER_FLAG_DISCONT);
  gst_harness_push (h, buf);

  /* Extract second (concealed) buffer from decoder and verify timestamp
     and the 2x duration */
  fail_unless (buf = gst_harness_pull (h));
  fail_unless_equals_int64 (dur0, GST_BUFFER_PTS (buf));
  fail_unless_equals_int64 (dur * 2, GST_BUFFER_DURATION (buf));
  fail_if (GST_BUFFER_FLAG_IS_SET (buf, GST_BUFFER_FLAG_DISCONT));
  gst_buffer_unref (buf);

  /* Push fourth buffer from encoder to decoder as normal */
  gst_harness_src_crank_and_push_many (h, 1, 1);

  /* Extract third buffer from decoder and verify timestamps */
  fail_unless (buf = gst_harness_pull (h));
  fail_unless_equals_int64 (dur0 + 1 * dur, GST_BUFFER_PTS (buf));
  fail_unless_equals_int64 (dur, GST_BUFFER_DURATION (buf));
  fail_if (GST_BUFFER_FLAG_IS_SET (buf, GST_BUFFER_FLAG_DISCONT));
  gst_buffer_unref (buf);

  gst_harness_teardown (h);
}

GST_END_TEST;

static Suite *
opus_suite (void)
{
  Suite *s = suite_create ("opus");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);

  tcase_add_test (tc_chain, test_opus_encode_nothing);
  tcase_add_test (tc_chain, test_opus_decode_nothing);
  tcase_add_test (tc_chain, test_opus_encode_samples);
  tcase_add_test (tc_chain, test_opus_encode_properties);
  tcase_add_test (tc_chain, test_opusdec_getcaps);
  tcase_add_test (tc_chain, test_opus_decode_plc_timestamps_with_fec);

  return s;
}

GST_CHECK_MAIN (opus);
