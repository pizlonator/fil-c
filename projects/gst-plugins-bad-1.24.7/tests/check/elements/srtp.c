/* GStreamer unit tests for the srtp elements
 * Copyright (C) 2007 Tim-Philipp Müller <tim centricular net>
 * Copyright (C) 2008 Sebastian Dröge <slomo@circular-chaos.org>
 * Copyright (C) 2016 Collabora Ltd <vincent.penquerch@collabora.co.uk>
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

#ifdef HAVE_VALGRIND
#include <valgrind/valgrind.h>
#endif

#include <gst/check/gstcheck.h>

#include <gst/check/gstharness.h>

GST_START_TEST (test_create_and_unref)
{
  GstElement *e;

  e = gst_element_factory_make ("srtpenc", NULL);
  fail_unless (e != NULL);
  gst_element_set_state (e, GST_STATE_NULL);
  gst_object_unref (e);

  e = gst_element_factory_make ("srtpdec", NULL);
  fail_unless (e != NULL);
  gst_element_set_state (e, GST_STATE_NULL);
  gst_object_unref (e);
}

GST_END_TEST;

static void
check_play (const gchar * encode_key, const gchar * decode_key,
    guint buffer_count, guint expected_recv_count,
    guint expected_recv_drop_count, guint port)
{
  GstElement *source_pipeline, *sink_pipeline;
  GstBus *source_bus;
  GstMessage *msg;
  GstStructure *stats;
  guint recv_count = 0;
  guint drop_count = 0;
  GstElement *srtp_dec;

  gchar *source_pipeline_desc = g_strdup_printf ("audiotestsrc num-buffers=%d \
        ! alawenc ! rtppcmapay ! application/x-rtp, payload=(int)8, ssrc=(uint)1356955624 \
        ! srtpenc name=enc key=%s ! udpsink port=%d sync=false host=127.0.0.1", buffer_count, encode_key, port);

  gchar *sink_pipeline_desc =
      g_strdup_printf ("udpsrc port=%d caps=\"application/x-srtp, \
      payload=(int)8, ssrc=(uint)1356955624, srtp-key=(buffer)%s, srtp-cipher=(string)aes-128-icm, \
      srtp-auth=(string)hmac-sha1-80, srtcp-cipher=(string)aes-128-icm, srtcp-auth=(string)hmac-sha1-80\" \
      ! srtpdec name=dec ! rtppcmadepay ! alawdec ! fakesink", port, decode_key);

  source_pipeline = gst_parse_launch (source_pipeline_desc, NULL);
  sink_pipeline = gst_parse_launch (sink_pipeline_desc, NULL);

  g_free (source_pipeline_desc);
  g_free (sink_pipeline_desc);

  fail_unless (gst_element_set_state (source_pipeline,
          GST_STATE_PLAYING) != GST_STATE_CHANGE_FAILURE);
  fail_unless (gst_element_set_state (sink_pipeline,
          GST_STATE_PLAYING) != GST_STATE_CHANGE_FAILURE);

  source_bus = gst_pipeline_get_bus (GST_PIPELINE (source_pipeline));

  msg =
      gst_bus_timed_pop_filtered (source_bus, GST_CLOCK_TIME_NONE,
      GST_MESSAGE_ERROR | GST_MESSAGE_EOS);
  fail_unless (GST_MESSAGE_TYPE (msg) == GST_MESSAGE_EOS);
  gst_message_unref (msg);

  // Wait 3s that all the buffers reached the sink pipeline entirely
  g_usleep (G_USEC_PER_SEC * 3);

  srtp_dec = gst_bin_get_by_name (GST_BIN (sink_pipeline), "dec");
  g_object_get (srtp_dec, "stats", &stats, NULL);
  gst_structure_get_uint (stats, "recv-count", &recv_count);
  fail_unless (recv_count == expected_recv_count);
  gst_structure_get_uint (stats, "recv-drop-count", &drop_count);
  fail_unless (drop_count == expected_recv_drop_count);
  gst_object_unref (srtp_dec);
  gst_structure_free (stats);
  gst_object_unref (source_bus);

  gst_element_set_state (source_pipeline, GST_STATE_NULL);
  gst_element_set_state (sink_pipeline, GST_STATE_NULL);

  gst_object_unref (source_pipeline);
  gst_object_unref (sink_pipeline);
}

GST_START_TEST (test_play)
{
  check_play ("012345678901234567890123456789012345678901234567890123456789",
      "012345678901234567890123456789012345678901234567890123456789", 50, 50,
      0, 5064);
}

GST_END_TEST;

GST_START_TEST (test_play_key_error)
{
  check_play ("012345678901234567890123456789012345678901234567890123456789",
      "000000000000000000000000000000000000000000000000000000000000", 50, 50,
      50, 5074);
}

GST_END_TEST;
typedef struct
{
  guint counter;
  guint start_roc;
} roc_check_data;

static guint
get_roc (GstElement * e)
{
  GstStructure *stats;
  const GstStructure *ss;
  const GValue *v;
  guint roc = 0;

  g_object_get (e, "stats", &stats, NULL);
  v = gst_structure_get_value (stats, "streams");
  fail_unless (v);
  v = gst_value_array_get_value (v, 0);
  ss = gst_value_get_structure (v);
  gst_structure_get_uint (ss, "roc", &roc);
  gst_structure_free (stats);
  return roc;
}

static GstPadProbeReturn
roc_check_probe (GstPad * pad, GstPadProbeInfo * info, gpointer user_data)
{
  roc_check_data *data = user_data;
  GstElement *e = GST_PAD_PARENT (pad);

  if (G_UNLIKELY (data->counter % 8192 == 0))
    GST_DEBUG_OBJECT (pad, "counter at %d", data->counter);

  /* record first roc, then wait for 2^16 packets to pass */
  if (data->counter == 0) {
    data->start_roc = get_roc (e);
  } else if (data->counter == 65536) {
    /* get roc and check it's one more than what we started with */
    fail_unless ((get_roc (e) & 0xffff) == ((data->start_roc + 1) & 0xffff));
  }
  data->counter++;
  return GST_PAD_PROBE_OK;
}

static GstCaps *
request_key (void)
{
  GstCaps *caps;

  caps =
      gst_caps_from_string
      ("application/x-srtp, payload=(int)8, ssrc=(uint)1356955624, srtp-key=(buffer)012345678901234567890123456789012345678901234567890123456789, srtp-cipher=(string)aes-128-icm, srtp-auth=(string)hmac-sha1-80, srtcp-cipher=(string)aes-128-icm, srtcp-auth=(string)hmac-sha1-80");
  return caps;
}

GST_START_TEST (test_roc)
{
  GstElement *source_pipeline, *sink_pipeline;
  GstElement *srtpenc, *srtpdec;
  GstBus *source_bus, *sink_bus;
  GstMessage *msg;
  GstPad *pad;
  roc_check_data source_roc_check_data, sink_roc_check_data;

  source_pipeline =
      gst_parse_launch
      ("audiotestsrc num-buffers=65555 ! alawenc ! rtppcmapay ! application/x-rtp, payload=(int)8, ssrc=(uint)1356955624 ! srtpenc name=enc key=012345678901234567890123456789012345678901234567890123456789 ! udpsink port=5004 sync=false host=127.0.0.1",
      NULL);
  sink_pipeline =
      gst_parse_launch
      ("udpsrc port=5004 caps=\"application/x-srtp, payload=(int)8, ssrc=(uint)1356955624, srtp-key=(buffer)012345678901234567890123456789012345678901234567890123456789, srtp-cipher=(string)aes-128-icm, srtp-auth=(string)hmac-sha1-80, srtcp-cipher=(string)aes-128-icm, srtcp-auth=(string)hmac-sha1-80\" ! srtpdec name=dec ! rtppcmadepay ! alawdec ! fakesink",
      NULL);

  fail_unless (gst_element_set_state (source_pipeline,
          GST_STATE_PLAYING) != GST_STATE_CHANGE_FAILURE);
  fail_unless (gst_element_set_state (sink_pipeline,
          GST_STATE_PLAYING) != GST_STATE_CHANGE_FAILURE);

  source_bus = gst_pipeline_get_bus (GST_PIPELINE (source_pipeline));
  sink_bus = gst_pipeline_get_bus (GST_PIPELINE (sink_pipeline));

  /* install a pad probe on the srtp elements' source pads */
  srtpenc = gst_bin_get_by_name (GST_BIN (source_pipeline), "enc");
  fail_unless (srtpenc != NULL);
  pad = gst_element_get_static_pad (srtpenc, "rtp_src_0");
  fail_unless (pad != NULL);
  source_roc_check_data.counter = 0;
  gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_BUFFER, roc_check_probe,
      &source_roc_check_data, NULL);
  gst_object_unref (pad);
  gst_object_unref (srtpenc);

  srtpdec = gst_bin_get_by_name (GST_BIN (sink_pipeline), "dec");
  fail_unless (srtpdec != NULL);
  g_signal_connect (srtpdec, "request_key", G_CALLBACK (request_key),
      GINT_TO_POINTER (0));
  pad = gst_element_get_static_pad (srtpdec, "rtp_src");
  sink_roc_check_data.counter = 0;
  gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_BUFFER, roc_check_probe,
      &sink_roc_check_data, NULL);
  fail_unless (pad != NULL);
  gst_object_unref (pad);
  gst_object_unref (srtpdec);

  msg =
      gst_bus_timed_pop_filtered (source_bus, GST_CLOCK_TIME_NONE,
      GST_MESSAGE_ERROR | GST_MESSAGE_EOS);
  fail_unless (GST_MESSAGE_TYPE (msg) == GST_MESSAGE_EOS);
  gst_message_unref (msg);

  gst_object_unref (source_bus);
  gst_object_unref (sink_bus);

  gst_element_set_state (source_pipeline, GST_STATE_NULL);
  gst_element_set_state (sink_pipeline, GST_STATE_NULL);

  gst_object_unref (source_pipeline);
  gst_object_unref (sink_pipeline);
}

GST_END_TEST;

#ifdef HAVE_SRTP2

GST_START_TEST (test_simple_mki)
{
  GstElement *pipeline;
  GstElement *dec;
  GstPad *pad;
  GstBus *bus;
  GstMessage *msg;
  GstCaps *caps1, *caps2;

  pipeline =
      gst_parse_launch
      ("audiotestsrc num-buffers=50 ! alawenc ! rtppcmapay ! application/x-rtp, payload=(int)8, ssrc=(uint)1356955624 ! srtpenc name=enc key=012345678901234567890123456789012345678901234567890123456789 mki=1234 ! srtpdec name=dec ! rtppcmadepay ! alawdec ! fakesink",
      NULL);

  fail_unless (gst_element_set_state (pipeline,
          GST_STATE_PLAYING) != GST_STATE_CHANGE_FAILURE);

  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));

  msg =
      gst_bus_timed_pop_filtered (bus, GST_CLOCK_TIME_NONE,
      GST_MESSAGE_ERROR | GST_MESSAGE_EOS);
  fail_unless (GST_MESSAGE_TYPE (msg) == GST_MESSAGE_EOS);
  gst_message_unref (msg);

  gst_object_unref (bus);

  dec = gst_bin_get_by_name (GST_BIN (pipeline), "dec");
  fail_unless (dec);
  pad = gst_element_get_static_pad (dec, "rtp_sink");
  fail_unless (pad);
  g_object_get (pad, "caps", &caps1, NULL);
  fail_unless (caps1);
  caps2 =
      gst_caps_from_string
      ("application/x-srtp, srtp-key=(buffer)012345678901234567890123456789012345678901234567890123456789, mki=(buffer)1234");
  fail_unless (gst_caps_can_intersect (caps1, caps2));
  gst_caps_unref (caps1);
  gst_caps_unref (caps2);
  gst_object_unref (pad);
  gst_object_unref (dec);

  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (pipeline);
}

GST_END_TEST;

GST_START_TEST (test_srtpdec_multiple_mki)
{
  static const char CAPS_RTP[] =
      "application/x-rtp, media=(string)audio, clock-rate=(int)8000, encoding-name=(string)PCMA, payload=(int)8, ssrc=(uint)2648728855";
  static const char CAPS_SRTP[] =
      "application/x-srtp, media=(string)audio, clock-rate=(int)8000, encoding-name=(string)PCMA, payload=(int)8, ssrc=(uint)2648728855, srtp-key=(buffer)012345678901234567890123456789012345678901234567890123456789, mki=(buffer)01, srtp-cipher=(string)aes-128-icm, srtp-auth=(string)hmac-sha1-80, srtcp-cipher=(string)aes-128-icm, srtcp-auth=(string)hmac-sha1-80, srtp-key2=(buffer)678901234567890123456789012345678901234567890123456780123456, mki2=(buffer)02";

  unsigned char DECRYPTED_1_PKT[] = {
    0x80, 0x88, 0x13, 0xe1, 0x87, 0x76, 0xda, 0x98, 0x9d, 0xe0, 0x65, 0x17,
    0xb4, 0xa5, 0xa3, 0xac, 0xac, 0xa3, 0xa5, 0xb7, 0xfc, 0x0a
  };
  unsigned int DECRYPTED_1_PKT_LEN = 22;
  unsigned char DECRYPTED_2_PKT[] = {
    0x80, 0x08, 0x13, 0xe2, 0x87, 0x76, 0xda, 0xa2, 0x9d, 0xe0, 0x65, 0x17,
    0x3a, 0x20, 0x2d, 0x2c, 0x23, 0x24, 0x31, 0x6c, 0x89, 0xbb
  };
  unsigned int DECRYPTED_2_PKT_LEN = 22;
  unsigned char DECRYPTED_3_PKT[] = {
    0x80, 0x08, 0x13, 0xe3, 0x87, 0x76, 0xda, 0xac, 0x9d, 0xe0, 0x65, 0x17,
    0xa0, 0xad, 0xac, 0xa2, 0xa7, 0xb0, 0x96, 0x0c, 0x39, 0x21
  };
  unsigned int DECRYPTED_3_PKT_LEN = 22;
  unsigned char MKI_1_01_PKT[] = {
    0x80, 0x88, 0x13, 0xe1, 0x87, 0x76, 0xda, 0x98, 0x9d, 0xe0, 0x65, 0x17,
    0xd7, 0x16, 0xac, 0x3e, 0x60, 0x08, 0x04, 0xd6, 0xfb, 0x0e, 0x01, 0x77,
    0x93, 0x20, 0x3f, 0x45, 0x2c, 0xb3, 0x74, 0xd1, 0x20
  };
  unsigned int MKI_1_01_PKT_LEN = 33;
  unsigned char MKI_2_02_PKT[] = {
    0x80, 0x08, 0x13, 0xe2, 0x87, 0x76, 0xda, 0xa2, 0x9d, 0xe0, 0x65, 0x17,
    0xc4, 0x69, 0x8c, 0xb3, 0xf8, 0x64, 0x66, 0x78, 0x7f, 0x1d, 0x02, 0x8f,
    0x50, 0x57, 0xff, 0xa4, 0x80, 0xe6, 0x68, 0x74, 0x21
  };
  unsigned int MKI_2_02_PKT_LEN = 33;
  unsigned char MKI_3_01_PKT[] = {
    0x80, 0x08, 0x13, 0xe3, 0x87, 0x76, 0xda, 0xac, 0x9d, 0xe0, 0x65, 0x17,
    0xa6, 0xdf, 0x77, 0x4c, 0xb0, 0xe9, 0x3c, 0x1a, 0x54, 0x6f, 0x01, 0x9d,
    0xc3, 0x4b, 0x1d, 0x29, 0x67, 0xa0, 0x4d, 0xde, 0xec
  };
  unsigned int MKI_3_01_PKT_LEN = 33;


  GstHarness *h =
      gst_harness_new_with_padnames ("srtpdec", "rtp_sink", "rtp_src");
  GstBuffer *buf;

  gst_harness_set_caps_str (h, CAPS_SRTP, CAPS_RTP);

  buf = gst_harness_push_and_pull (h,
      gst_buffer_new_wrapped_full (GST_MEMORY_FLAG_READONLY,
          (char *) MKI_1_01_PKT, MKI_1_01_PKT_LEN, 0, MKI_1_01_PKT_LEN, NULL,
          NULL));
  fail_unless (buf);
  fail_unless_equals_int (gst_buffer_get_size (buf), DECRYPTED_1_PKT_LEN);
  fail_unless (!gst_buffer_memcmp (buf, 0, DECRYPTED_1_PKT,
          DECRYPTED_1_PKT_LEN));
  gst_buffer_unref (buf);

  buf = gst_harness_push_and_pull (h,
      gst_buffer_new_wrapped_full (GST_MEMORY_FLAG_READONLY,
          (char *) MKI_2_02_PKT, MKI_2_02_PKT_LEN, 0, MKI_2_02_PKT_LEN, NULL,
          NULL));
  fail_unless_equals_int (gst_buffer_get_size (buf), DECRYPTED_2_PKT_LEN);
  fail_unless (!gst_buffer_memcmp (buf, 0, DECRYPTED_2_PKT,
          DECRYPTED_2_PKT_LEN));
  gst_buffer_unref (buf);

  buf = gst_harness_push_and_pull (h,
      gst_buffer_new_wrapped_full (GST_MEMORY_FLAG_READONLY,
          (char *) MKI_3_01_PKT, MKI_3_01_PKT_LEN, 0, MKI_3_01_PKT_LEN, NULL,
          NULL));
  fail_unless_equals_int (gst_buffer_get_size (buf), DECRYPTED_3_PKT_LEN);
  fail_unless (!gst_buffer_memcmp (buf, 0, DECRYPTED_3_PKT,
          DECRYPTED_3_PKT_LEN));
  gst_buffer_unref (buf);

  gst_harness_teardown (h);
}

GST_END_TEST;


#endif

static Suite *
srtp_suite (void)
{
  Suite *s = suite_create ("srtp");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_set_timeout (tc_chain, 180);
  tcase_add_test (tc_chain, test_create_and_unref);
  tcase_add_test (tc_chain, test_play);
  tcase_add_test (tc_chain, test_roc);
  tcase_add_test (tc_chain, test_play_key_error);
#ifdef HAVE_SRTP2
  tcase_add_test (tc_chain, test_simple_mki);
  tcase_add_test (tc_chain, test_srtpdec_multiple_mki);
#endif

  return s;
}

GST_CHECK_MAIN (srtp);
