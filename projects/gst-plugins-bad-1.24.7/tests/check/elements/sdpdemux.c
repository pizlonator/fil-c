/*
 * GStreamer
 * Copyright (C) 2023 Seungha Yang <seungha@centricular.com>
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

#include <gst/gst.h>
#include "../../../gst/sdp/gstsdpdemux.h"
#include "../../../gst/sdp/gstsdpdemux.c"
#undef GST_CAT_DEFAULT

#include <gst/check/gstcheck.h>

static void
test_source_filter (const gchar * str, guint size, const gchar * result)
{
  GstSDPMessage sdp = { 0 };
  GstSDPDemux *demux = g_object_new (GST_TYPE_SDP_DEMUX, NULL);
  GstSDPStream *stream;

  gst_sdp_message_init (&sdp);
  fail_if (gst_sdp_message_parse_buffer ((const guint8 *) str, size,
          &sdp) != GST_SDP_OK);

  stream = gst_sdp_demux_create_stream (demux, &sdp, 0);
  fail_if (stream == NULL);

  if (result) {
    fail_unless_equals_string (stream->src_list, result);
  } else {
    fail_if (stream->src_list);
  }

  gst_sdp_demux_cleanup (demux);
  gst_object_unref (demux);
  gst_sdp_message_uninit (&sdp);
}

GST_START_TEST (test_parse_source_filter_incl)
{
  static const gchar sdp[] =
      "v=0\r\n"
      "o=- 18 0 IN IP4 127.0.0.1\r\n"
      "s=TestSdp\r\n"
      "t=0 0\r\n"
      "m=audio 5004 RTP/AVP 98\r\n"
      "c=IN IP4 224.0.0.0\r\n"
      "a=recvonly\r\n"
      "a=source-filter: incl IN IP4 224.0.0.0 127.0.0.1\r\n"
      "a=rtpmap:98 L24/48000/2\r\n" "a=framecount:48\r\n" "a=recvonly\r\n";

  test_source_filter (sdp, sizeof (sdp), "+127.0.0.1");
}

GST_END_TEST;

GST_START_TEST (test_parse_source_filter_incl_multi_list)
{
  static const gchar sdp[] =
      "v=0\r\n"
      "o=- 18 0 IN IP4 127.0.0.1\r\n"
      "s=TestSdp\r\n"
      "t=0 0\r\n"
      "m=audio 5004 RTP/AVP 98\r\n"
      "c=IN IP4 224.0.0.0\r\n"
      "a=recvonly\r\n"
      "a=source-filter: incl IN IP4 224.0.0.0 127.0.0.1 127.0.0.2\r\n"
      "a=rtpmap:98 L24/48000/2\r\n" "a=framecount:48\r\n" "a=recvonly\r\n";

  test_source_filter (sdp, sizeof (sdp), "+127.0.0.1+127.0.0.2");
}

GST_END_TEST;

GST_START_TEST (test_parse_source_filter_excl)
{
  static const gchar sdp[] =
      "v=0\r\n"
      "o=- 18 0 IN IP4 127.0.0.1\r\n"
      "s=TestSdp\r\n"
      "t=0 0\r\n"
      "m=audio 5004 RTP/AVP 98\r\n"
      "c=IN IP4 224.0.0.0\r\n"
      "a=recvonly\r\n"
      "a=source-filter: excl IN IP4 224.0.0.0 127.0.0.2\r\n"
      "a=rtpmap:98 L24/48000/2\r\n" "a=framecount:48\r\n" "a=recvonly\r\n";

  test_source_filter (sdp, sizeof (sdp), "-127.0.0.2");
}

GST_END_TEST;

GST_START_TEST (test_parse_source_filter_incl_excl)
{
  static const gchar sdp[] =
      "v=0\r\n"
      "o=- 18 0 IN IP4 127.0.0.1\r\n"
      "s=TestSdp\r\n"
      "t=0 0\r\n"
      "m=audio 5004 RTP/AVP 98\r\n"
      "c=IN IP4 224.0.0.0\r\n"
      "a=recvonly\r\n"
      "a=source-filter: incl IN IP4 224.0.0.0 127.0.0.1\r\n"
      "a=source-filter: excl IN IP4 224.0.0.0 127.0.0.2\r\n"
      "a=rtpmap:98 L24/48000/2\r\n" "a=framecount:48\r\n" "a=recvonly\r\n";

  test_source_filter (sdp, sizeof (sdp), "+127.0.0.1-127.0.0.2");
}

GST_END_TEST;

GST_START_TEST (test_parse_source_filter_with_trailing_space)
{
  static const gchar sdp[] =
      "v=0\r\n"
      "o=- 18 0 IN IP4 127.0.0.1\r\n"
      "s=TestSdp\r\n"
      "t=0 0\r\n"
      "m=audio 5004 RTP/AVP 98\r\n"
      "c=IN IP4 224.0.0.0\r\n"
      "a=recvonly\r\n"
      "a=source-filter: incl  IN   IP4  224.0.0.0   127.0.0.1    \r\n"
      "a=rtpmap:98 L24/48000/2\r\n" "a=framecount:48\r\n" "a=recvonly\r\n";

  test_source_filter (sdp, sizeof (sdp), "+127.0.0.1");
}

GST_END_TEST;

GST_START_TEST (test_parse_source_filter_missing_list)
{
  static const gchar sdp[] =
      "v=0\r\n"
      "o=- 18 0 IN IP4 127.0.0.1\r\n"
      "s=TestSdp\r\n"
      "t=0 0\r\n"
      "m=audio 5004 RTP/AVP 98\r\n"
      "c=IN IP4 224.0.0.0\r\n"
      "a=recvonly\r\n"
      "a=source-filter: incl IN IP4 224.0.0.0 \r\n"
      "a=rtpmap:98 L24/48000/2\r\n" "a=framecount:48\r\n" "a=recvonly\r\n";

  test_source_filter (sdp, sizeof (sdp), NULL);
}

GST_END_TEST;

static Suite *
sdpdemux_suite (void)
{
  Suite *s;
  TCase *tc_chain;

  s = suite_create ("sdpdemux");
  tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_parse_source_filter_incl);
  tcase_add_test (tc_chain, test_parse_source_filter_incl_multi_list);
  tcase_add_test (tc_chain, test_parse_source_filter_excl);
  tcase_add_test (tc_chain, test_parse_source_filter_incl_excl);
  tcase_add_test (tc_chain, test_parse_source_filter_with_trailing_space);
  tcase_add_test (tc_chain, test_parse_source_filter_missing_list);

  return s;
}

GST_CHECK_MAIN (sdpdemux);
