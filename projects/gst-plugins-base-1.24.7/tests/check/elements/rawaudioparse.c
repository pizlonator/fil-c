/* GStreamer
 *
 * unit test for rawaudioparse
 *
 * Copyright (C) <2016> Carlos Rafael Giani <dv at pseudoterminal dot org>
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

/* FIXME: GValueArray is deprecated, but there is currently no viable alternatives
 * See https://bugzilla.gnome.org/show_bug.cgi?id=667228 */
#define GLIB_DISABLE_DEPRECATION_WARNINGS

#include <gst/check/gstcheck.h>
#include <gst/audio/audio.h>

/* Checks are hardcoded to expect stereo 16-bit data. The sample rate
 * however varies from the default of 40 kHz in some tests to see the
 * differences in calculated buffer durations. */
#define NUM_TEST_SAMPLES 512
#define NUM_TEST_CHANNELS 2
#define TEST_SAMPLE_RATE 40000
#define TEST_SAMPLE_FORMAT GST_AUDIO_FORMAT_S16

/* For ease of programming we use globals to keep refs for our floating
 * src and sink pads we create; otherwise we always have to do get_pad,
 * get_peer, and then remove references in every test function */
static GstPad *mysrcpad, *mysinkpad;

typedef struct
{
  GstElement *rawaudioparse;
  GstAdapter *test_data_adapter;
}
RawAudParseTestCtx;

/* Sets up a rawaudioparse element and a GstAdapter that contains 512 test
 * audio samples. The samples a monotonically increasing set from the values
 * 0 to 511 for the left and 512 to 1023 for the right channel. The result
 * is a GstAdapter that contains the interleaved 16-bit integer values:
 * 0,512,1,513,2,514, ... 511,1023 . This set is used in the checks to see
 * if rawaudioparse's output buffers contain valid data. */
static void
setup_rawaudioparse (RawAudParseTestCtx * testctx, gboolean use_sink_caps,
    gboolean set_properties, GstCaps * incaps, GstFormat format)
{
  GstElement *rawaudioparse;
  GstAdapter *test_data_adapter;
  GstBuffer *buffer;
  guint i;
  guint16 samples[NUM_TEST_SAMPLES * NUM_TEST_CHANNELS];


  /* Setup the rawaudioparse element and the pads */

  static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
      GST_PAD_SINK,
      GST_PAD_ALWAYS,
      GST_STATIC_CAPS (GST_AUDIO_CAPS_MAKE (GST_AUDIO_FORMATS_ALL))
      );
  static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
      GST_PAD_SRC,
      GST_PAD_ALWAYS,
      GST_STATIC_CAPS_ANY);

  rawaudioparse = gst_check_setup_element ("rawaudioparse");

  g_object_set (G_OBJECT (rawaudioparse), "use-sink-caps", use_sink_caps, NULL);
  if (set_properties)
    g_object_set (G_OBJECT (rawaudioparse), "sample-rate", TEST_SAMPLE_RATE,
        "num-channels", NUM_TEST_CHANNELS, "pcm-format", TEST_SAMPLE_FORMAT,
        NULL);

  fail_unless (gst_element_set_state (rawaudioparse,
          GST_STATE_PAUSED) == GST_STATE_CHANGE_SUCCESS,
      "could not set to paused");

  mysrcpad = gst_check_setup_src_pad (rawaudioparse, &srctemplate);
  mysinkpad = gst_check_setup_sink_pad (rawaudioparse, &sinktemplate);

  gst_pad_set_active (mysrcpad, TRUE);
  gst_pad_set_active (mysinkpad, TRUE);

  gst_check_setup_events (mysrcpad, rawaudioparse, incaps, format);
  if (incaps)
    gst_caps_unref (incaps);


  /* Fill the adapter with the interleaved 0..511 and
   * 512..1023 samples */
  for (i = 0; i < NUM_TEST_SAMPLES; ++i) {
    guint c;
    for (c = 0; c < NUM_TEST_CHANNELS; ++c)
      samples[i * NUM_TEST_CHANNELS + c] = c * NUM_TEST_SAMPLES + i;
  }

  test_data_adapter = gst_adapter_new ();
  buffer = gst_buffer_new_allocate (NULL, sizeof (samples), NULL);
  gst_buffer_fill (buffer, 0, samples, sizeof (samples));
  gst_adapter_push (test_data_adapter, buffer);


  testctx->rawaudioparse = rawaudioparse;
  testctx->test_data_adapter = test_data_adapter;
}

static void
cleanup_rawaudioparse (RawAudParseTestCtx * testctx)
{
  int num_buffers, i;

  gst_pad_set_active (mysrcpad, FALSE);
  gst_pad_set_active (mysinkpad, FALSE);
  gst_check_teardown_src_pad (testctx->rawaudioparse);
  gst_check_teardown_sink_pad (testctx->rawaudioparse);
  gst_check_teardown_element (testctx->rawaudioparse);

  g_object_unref (G_OBJECT (testctx->test_data_adapter));

  if (buffers != NULL) {
    num_buffers = g_list_length (buffers);
    for (i = 0; i < num_buffers; ++i) {
      GstBuffer *buf = GST_BUFFER (buffers->data);
      buffers = g_list_remove (buffers, buf);
      gst_buffer_unref (buf);
    }

    g_list_free (buffers);
    buffers = NULL;
  }
}


static void
push_data_and_check_output (RawAudParseTestCtx * testctx, gsize num_in_bytes,
    gsize expected_num_out_bytes, gint64 expected_pts, gint64 expected_dur,
    guint expected_num_buffers_in_list, guint bpf, guint16 channel0_start,
    guint16 channel1_start)
{
  GstBuffer *inbuf, *outbuf;
  guint num_buffers;

  /* Simulate upstream input by taking num_in_bytes bytes from the adapter */
  inbuf = gst_adapter_take_buffer (testctx->test_data_adapter, num_in_bytes);
  fail_unless (inbuf != NULL);

  /* Push the input data and check that the output buffers list grew as
   * expected */
  fail_unless (gst_pad_push (mysrcpad, inbuf) == GST_FLOW_OK);
  num_buffers = g_list_length (buffers);
  fail_unless_equals_int (num_buffers, expected_num_buffers_in_list);

  /* Take the latest output buffer */
  outbuf = g_list_nth_data (buffers, num_buffers - 1);
  fail_unless (outbuf != NULL);

  /* Verify size, PTS, duration of the output buffer */
  fail_unless_equals_uint64 (expected_num_out_bytes,
      gst_buffer_get_size (outbuf));
  fail_unless_equals_uint64 (expected_pts, GST_BUFFER_PTS (outbuf));
  fail_unless_equals_uint64 (expected_dur, GST_BUFFER_DURATION (outbuf));

  /* Go through all of the samples in the output buffer and check that they are
   * valid. The samples are interleaved. The offsets specified by channel0_start
   * and channel1_start are the expected values of the first sample for each
   * channel in the buffer. So, if channel0_start is 512, then sample #0 in the
   * buffer must have value 512, and if channel1_start is 700, then sample #1
   * in the buffer must have value 700 etc. */
  {
    guint i, num_frames;
    guint16 *s;
    GstMapInfo map_info;
    guint channel_starts[2] = { channel0_start, channel1_start };

    gst_buffer_map (outbuf, &map_info, GST_MAP_READ);
    num_frames = map_info.size / bpf;
    s = (guint16 *) (map_info.data);

    for (i = 0; i < num_frames; ++i) {
      guint c;

      for (c = 0; i < NUM_TEST_CHANNELS; ++i) {
        guint16 expected = channel_starts[c] + i;
        guint16 actual = s[i * NUM_TEST_CHANNELS + c];

        fail_unless_equals_int (expected, actual);
      }
    }

    gst_buffer_unmap (outbuf, &map_info);
  }
}


GST_START_TEST (test_push_unaligned_data_properties_config)
{
  RawAudParseTestCtx testctx;

  setup_rawaudioparse (&testctx, FALSE, TRUE, NULL, GST_FORMAT_BYTES);

  /* Send in data buffers that are not aligned to multiples of the
   * frame size (= sample size * num_channels). This tests if rawaudioparse
   * aligns output data properly.
   *
   * The second line sends in 99 bytes, and expects 100 bytes in the
   * output buffer. This is because the first buffer contains 45 bytes,
   * and rawaudioparse is expected to output 44 bytes (which is an integer
   * multiple of the frame size). The leftover 1 byte then gets prepended
   * to the input buffer with 99 bytes, resulting in 100 bytes, which is
   * an integer multiple of the frame size.
   */

  push_data_and_check_output (&testctx, 45, 44, GST_USECOND * 0,
      GST_USECOND * 275, 1, 4, 0, 512);
  push_data_and_check_output (&testctx, 99, 100, GST_USECOND * 275,
      GST_USECOND * 625, 2, 4, 11, 523);
  push_data_and_check_output (&testctx, 18, 16, GST_USECOND * 900,
      GST_USECOND * 100, 3, 4, 36, 548);

  cleanup_rawaudioparse (&testctx);
}

GST_END_TEST;

GST_START_TEST (test_push_unaligned_data_sink_caps_config)
{
  RawAudParseTestCtx testctx;
  GstAudioInfo ainfo;
  GstCaps *caps;

  /* This test is essentially the same as test_push_unaligned_data_properties_config,
   * except that rawaudioparse uses the sink caps config instead of the property config. */

  gst_audio_info_set_format (&ainfo, TEST_SAMPLE_FORMAT, TEST_SAMPLE_RATE,
      NUM_TEST_CHANNELS, NULL);
  caps = gst_audio_info_to_caps (&ainfo);

  setup_rawaudioparse (&testctx, TRUE, FALSE, caps, GST_FORMAT_BYTES);

  push_data_and_check_output (&testctx, 45, 44, GST_USECOND * 0,
      GST_USECOND * 275, 1, 4, 0, 512);
  push_data_and_check_output (&testctx, 99, 100, GST_USECOND * 275,
      GST_USECOND * 625, 2, 4, 11, 523);
  push_data_and_check_output (&testctx, 18, 16, GST_USECOND * 900,
      GST_USECOND * 100, 3, 4, 36, 548);

  cleanup_rawaudioparse (&testctx);
}

GST_END_TEST;

GST_START_TEST (test_push_swapped_channels)
{
  RawAudParseTestCtx testctx;
  GValueArray *valarray;
  GValue val = G_VALUE_INIT;

  /* Send in 40 bytes and use a nonstandard channel order (left and right channels
   * swapped). Expected behavior is for rawaudioparse to reorder the samples inside
   * output buffers to conform to the GStreamer channel order. For this reason,
   * channel0 offset is 512 and channel1 offset is 0 in the check below. */

  setup_rawaudioparse (&testctx, FALSE, TRUE, NULL, GST_FORMAT_BYTES);

  valarray = g_value_array_new (2);
  g_value_init (&val, GST_TYPE_AUDIO_CHANNEL_POSITION);
  g_value_set_enum (&val, GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT);
  g_value_array_insert (valarray, 0, &val);
  g_value_set_enum (&val, GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT);
  g_value_array_insert (valarray, 1, &val);
  g_object_set (G_OBJECT (testctx.rawaudioparse), "channel-positions",
      valarray, NULL);
  g_value_array_free (valarray);
  g_value_unset (&val);

  push_data_and_check_output (&testctx, 40, 40, GST_USECOND * 0,
      GST_USECOND * 250, 1, 4, 512, 0);

  cleanup_rawaudioparse (&testctx);
}

GST_END_TEST;

GST_START_TEST (test_config_switch)
{
  RawAudParseTestCtx testctx;
  GstAudioInfo ainfo;
  GstCaps *caps;

  /* Start processing with the properties config active, then mid-stream switch to
   * the sink caps config. The properties config is altered to have a different
   * sample rate than the sink caps to be able to detect the switch. The net effect
   * is that output buffer durations are altered. For example, 40 bytes equal
   * 10 samples, and this equals 500 us with 20 kHz or 250 us with 40 kHz. */

  gst_audio_info_set_format (&ainfo, TEST_SAMPLE_FORMAT, TEST_SAMPLE_RATE,
      NUM_TEST_CHANNELS, NULL);
  caps = gst_audio_info_to_caps (&ainfo);

  setup_rawaudioparse (&testctx, FALSE, TRUE, caps, GST_FORMAT_BYTES);

  g_object_set (G_OBJECT (testctx.rawaudioparse), "sample-rate", 20000, NULL);

  /* Push in data with properties config active, expecting duration calculations
   * to be based on the 20 kHz sample rate */
  push_data_and_check_output (&testctx, 40, 40, GST_USECOND * 0,
      GST_USECOND * 500, 1, 4, 0, 512);
  push_data_and_check_output (&testctx, 20, 20, GST_USECOND * 500,
      GST_USECOND * 250, 2, 4, 10, 522);

  /* Perform the switch */
  g_object_set (G_OBJECT (testctx.rawaudioparse), "use-sink-caps", TRUE, NULL);

  /* Push in data with sink caps config active, expecting duration calculations
   * to be based on the 40 kHz sample rate */
  push_data_and_check_output (&testctx, 40, 40, GST_USECOND * 750,
      GST_USECOND * 250, 3, 4, 15, 527);

  cleanup_rawaudioparse (&testctx);
}

GST_END_TEST;

GST_START_TEST (test_change_caps)
{
  RawAudParseTestCtx testctx;
  GstAudioInfo ainfo;
  GstCaps *caps;

  /* Start processing with the sink caps config active, using the
   * default channel count and sample format and 20 kHz sample rate
   * for the caps. Push some data, then change caps (20 kHz -> 40 kHz).
   * Check that the changed caps are handled properly. */

  gst_audio_info_set_format (&ainfo, TEST_SAMPLE_FORMAT, 20000,
      NUM_TEST_CHANNELS, NULL);
  caps = gst_audio_info_to_caps (&ainfo);

  setup_rawaudioparse (&testctx, TRUE, FALSE, caps, GST_FORMAT_BYTES);

  /* Push in data with caps sink config active, expecting duration calculations
   * to be based on the 20 kHz sample rate */
  push_data_and_check_output (&testctx, 40, 40, GST_USECOND * 0,
      GST_USECOND * 500, 1, 4, 0, 512);
  push_data_and_check_output (&testctx, 20, 20, GST_USECOND * 500,
      GST_USECOND * 250, 2, 4, 10, 522);

  /* Change caps */
  gst_audio_info_set_format (&ainfo, TEST_SAMPLE_FORMAT, 40000,
      NUM_TEST_CHANNELS, NULL);
  caps = gst_audio_info_to_caps (&ainfo);
  fail_unless (gst_pad_push_event (mysrcpad, gst_event_new_caps (caps)));
  gst_caps_unref (caps);

  /* Push in data with the new caps, expecting duration calculations
   * to be based on the 40 kHz sample rate */
  push_data_and_check_output (&testctx, 40, 40, GST_USECOND * 750,
      GST_USECOND * 250, 3, 4, 15, 527);

  cleanup_rawaudioparse (&testctx);
}

GST_END_TEST;


static Suite *
rawaudioparse_suite (void)
{
  Suite *s = suite_create ("rawaudioparse");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_push_unaligned_data_properties_config);
  tcase_add_test (tc_chain, test_push_unaligned_data_sink_caps_config);
  tcase_add_test (tc_chain, test_push_swapped_channels);
  tcase_add_test (tc_chain, test_config_switch);
  tcase_add_test (tc_chain, test_change_caps);

  return s;
}

GST_CHECK_MAIN (rawaudioparse);
