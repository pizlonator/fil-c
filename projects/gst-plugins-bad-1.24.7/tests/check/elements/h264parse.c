/*
 * GStreamer
 *
 * unit test for h264parse
 *
 * Copyright (C) 2011 Nokia Corporation. All rights reserved.
 *
 * Contact: Stefan Kost <stefan.kost@nokia.com>
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

#include <gst/check/check.h>
#include <gst/video/video.h>
#include <gst/video/video-sei.h>
#include "gst-libs/gst/codecparsers/gsth264parser.h"
#include "parser.h"

#define SRC_CAPS_TMPL   "video/x-h264, parsed=(boolean)false"
#define SINK_CAPS_TMPL  "video/x-h264, parsed=(boolean)true"

GstStaticPadTemplate sinktemplate_bs_nal = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (SINK_CAPS_TMPL
        ", stream-format = (string) byte-stream, alignment = (string) nal")
    );

GstStaticPadTemplate sinktemplate_bs_au = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (SINK_CAPS_TMPL
        ", stream-format = (string) byte-stream, alignment = (string) au")
    );

GstStaticPadTemplate sinktemplate_avc_au = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (SINK_CAPS_TMPL
        ", stream-format = (string) avc, alignment = (string) au")
    );

GstStaticPadTemplate sinktemplate_avc3_au = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (SINK_CAPS_TMPL
        ", stream-format = (string) avc3, alignment = (string) au")
    );

GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (SRC_CAPS_TMPL)
    );

/* some data */

/* AUD */
static guint8 h264_aud[] = {
  0x00, 0x00, 0x00, 0x01, 0x09, 0xf0
};


/* SPS */
static guint8 h264_sps[] = {
  0x00, 0x00, 0x00, 0x01, 0x67, 0x4d, 0x40, 0x15,
  0xec, 0xa4, 0xbf, 0x2e, 0x02, 0x20, 0x00, 0x00,
  0x03, 0x00, 0x2e, 0xe6, 0xb2, 0x80, 0x01, 0xe2,
  0xc5, 0xb2, 0xc0
};

/* PPS */
static guint8 h264_pps[] = {
  0x00, 0x00, 0x00, 0x01, 0x68, 0xeb, 0xec, 0xb2
};

/* SEI buffering_period() message */
static guint8 h264_sei_buffering_period[] = {
  0x00, 0x00, 0x00, 0x01, 0x06, 0x00, 0x01, 0xc0
};

/* Content light level information SEI message */
static guint8 h264_sei_clli[] = {
  0x00, 0x00, 0x00, 0x01, 0x06, 0x90, 0x04, 0x03, 0xe8, 0x01, 0x90, 0x80
};

/* Mastering display colour volume information SEI message */
static guint8 h264_sei_mdcv[] = {
  0x00, 0x00, 0x00, 0x01, 0x06, 0x89, 0x18, 0x84,
  0xd0, 0x3e, 0x80, 0x33, 0x90, 0x86, 0xc4, 0x1d,
  0x4c, 0x0b, 0xb8, 0x3d, 0x13, 0x40, 0x42, 0x00,
  0x98, 0x96, 0x80, 0x00, 0x00, 0x03, 0x00, 0x01,
  0x80
};

/* combines to this codec-data */
static guint8 h264_avc_codec_data[] = {
  0x01, 0x4d, 0x40, 0x15, 0xff, 0xe1, 0x00, 0x17,
  0x67, 0x4d, 0x40, 0x15, 0xec, 0xa4, 0xbf, 0x2e,
  0x02, 0x20, 0x00, 0x00, 0x03, 0x00, 0x2e, 0xe6,
  0xb2, 0x80, 0x01, 0xe2, 0xc5, 0xb2, 0xc0, 0x01,
  0x00, 0x04, 0x68, 0xeb, 0xec, 0xb2
};

/* codec-data for avc3 where there are no SPS/PPS in the codec_data */
static guint8 h264_avc3_codec_data[] = {
  0x01,                         /* config version, always == 1 */
  0x4d,                         /* profile */
  0x40,                         /* profile compatibility */
  0x15, 0xff,                   /* 6 reserved bits, lengthSizeMinusOne */
  0xe0,                         /* 3 reserved bits, numSPS */
  0x00                          /* numPPS */
};

static guint8 *h264_codec_data = NULL;
static guint8 h264_codec_data_size = 0;


/* keyframes all around */
static guint8 h264_idrframe[] = {
  0x00, 0x00, 0x00, 0x01, 0x65, 0x88, 0x84, 0x00,
  0x10, 0xff, 0xfe, 0xf6, 0xf0, 0xfe, 0x05, 0x36,
  0x56, 0x04, 0x50, 0x96, 0x7b, 0x3f, 0x53, 0xe1
};

/* truncated nal */
static guint8 garbage_frame[] = {
  0x00, 0x00, 0x00, 0x01, 0x05
};

/* context to tweak tests */
static const gchar *ctx_suite;
static gboolean ctx_codec_data;
static gboolean ctx_hdr_sei;

#define SPS_LEN 3
#define SPS_CONSTRAINT_SET_FLAG_0 1 << 7
#define SPS_CONSTRAINT_SET_FLAG_1 (1 << 6)
#define SPS_CONSTRAINT_SET_FLAG_2 (1 << 5)
#define SPS_CONSTRAINT_SET_FLAG_3 (1 << 4)
#define SPS_CONSTRAINT_SET_FLAG_4 (1 << 3)
#define SPS_CONSTRAINT_SET_FLAG_5 (1 << 2)

static void
fill_h264_sps (guint8 * sps, guint8 profile_idc, guint constraint_set_flags,
    guint level_idc)
{
  memset (sps, 0x0, SPS_LEN);
  /*
   * * Bit 0:7   - Profile indication
   * * Bit 8     - constraint_set0_flag
   * * Bit 9     - constraint_set1_flag
   * * Bit 10    - constraint_set2_flag
   * * Bit 11    - constraint_set3_flag
   * * Bit 12    - constraint_set4_flag
   * * Bit 13    - constraint_set5_flag
   * * Bit 14:15 - Reserved
   * * Bit 16:24 - Level indication
   * */
  sps[0] = profile_idc;
  sps[1] |= constraint_set_flags;
  sps[2] = level_idc;
}

static gboolean
verify_buffer (buffer_verify_data_s * vdata, GstBuffer * buffer)
{
  if (vdata->discard) {
    /* check separate header NALs */
    gint i = vdata->buffer_counter;
    gboolean aud;

    /* SEI with start code prefix with 2 0-bytes */
    aud = i == 0;
    fail_unless (i <= 3);

    if (aud) {
      fail_unless (gst_buffer_get_size (buffer) == sizeof (h264_aud));
      fail_unless (gst_buffer_memcmp (buffer, 0, h264_aud,
              gst_buffer_get_size (buffer)) == 0);
      vdata->discard++;
    } else {
      i -= 1;

      fail_unless (gst_buffer_get_size (buffer) == ctx_headers[i].size);
      fail_unless (gst_buffer_memcmp (buffer, 0, ctx_headers[i].data,
              gst_buffer_get_size (buffer)) == 0);
    }
  } else {
    GstMapInfo map;

    gst_buffer_map (buffer, &map, GST_MAP_READ);
    fail_unless (map.size > 4);
    /* only need to check avc and bs-to-nal output case */
    if (GST_READ_UINT24_BE (map.data) == 0x01) {
      /* in bs-to-nal, a leading 0x00 is stripped from output */
      fail_unless (gst_buffer_get_size (buffer) ==
          vdata->data_to_verify_size - 1);
      fail_unless (gst_buffer_memcmp (buffer, 0, vdata->data_to_verify + 1,
              vdata->data_to_verify_size - 1) == 0);
      gst_buffer_unmap (buffer, &map);
      return TRUE;
    } else if (GST_READ_UINT32_BE (map.data) == 0x01) {
      gboolean aud = FALSE;
      aud = vdata->buffer_counter % 2;
      if (aud) {
        fail_unless (gst_buffer_get_size (buffer) == sizeof (h264_aud));
        fail_unless (gst_buffer_memcmp (buffer, 0, h264_aud,
                gst_buffer_get_size (buffer)) == 0);
        gst_buffer_unmap (buffer, &map);
        return TRUE;
      }

      /* this is not avc, use default tests from parser.c */
      gst_buffer_unmap (buffer, &map);
      return FALSE;
    }
    /* header is merged in initial frame */
    if (vdata->buffer_counter == 0) {
      guint8 *data = map.data;

      fail_unless (map.size == vdata->data_to_verify_size +
          ctx_headers[0].size + ctx_headers[1].size + ctx_headers[2].size);
      fail_unless (GST_READ_UINT32_BE (data) == ctx_headers[0].size - 4);
      fail_unless (memcmp (data + 4, ctx_headers[0].data + 4,
              ctx_headers[0].size - 4) == 0);
      data += ctx_headers[0].size;
      fail_unless (GST_READ_UINT32_BE (data) == ctx_headers[1].size - 4);
      fail_unless (memcmp (data + 4, ctx_headers[1].data + 4,
              ctx_headers[1].size - 4) == 0);
      data += ctx_headers[1].size;
      fail_unless (GST_READ_UINT32_BE (data) == ctx_headers[2].size - 4);
      fail_unless (memcmp (data + 4, ctx_headers[2].data + 4,
              ctx_headers[2].size - 4) == 0);
      data += ctx_headers[2].size;
      fail_unless (GST_READ_UINT32_BE (data) == vdata->data_to_verify_size - 4);
      fail_unless (memcmp (data + 4, vdata->data_to_verify + 4,
              vdata->data_to_verify_size - 4) == 0);
    } else {
      fail_unless (GST_READ_UINT32_BE (map.data) == map.size - 4);
      fail_unless (map.size == vdata->data_to_verify_size);
      fail_unless (memcmp (map.data + 4, vdata->data_to_verify + 4,
              map.size - 4) == 0);
    }
    gst_buffer_unmap (buffer, &map);
    return TRUE;
  }

  return FALSE;
}

/* A single access unit comprising of SPS, SEI, PPS and IDR frame */
static gboolean
verify_buffer_bs_au (buffer_verify_data_s * vdata, GstBuffer * buffer)
{
  GstMapInfo map;

  fail_unless (ctx_sink_template == &sinktemplate_bs_au);

  /* Currently the parser can only predict DTS when dealing with raw data.
   * Ensure that this behavior is being checked here. */
  GST_DEBUG ("PTS: %" GST_TIME_FORMAT " DTS: %" GST_TIME_FORMAT,
      GST_TIME_ARGS (GST_BUFFER_PTS (buffer)),
      GST_TIME_ARGS (GST_BUFFER_DTS (buffer)));
  fail_if (GST_BUFFER_PTS_IS_VALID (buffer));
  fail_unless (GST_BUFFER_DTS_IS_VALID (buffer));

  gst_buffer_map (buffer, &map, GST_MAP_READ);
  fail_unless (map.size > 4);

  if (vdata->buffer_counter == 0) {
    guint8 *data = map.data;

    /* AUD, SPS, SEI, PPS */
    fail_unless (map.size == vdata->data_to_verify_size +
        sizeof (h264_aud) + ctx_headers[0].size +
        ctx_headers[1].size + ctx_headers[2].size);
    fail_unless (memcmp (data, h264_aud, sizeof (h264_aud)) == 0);
    data += sizeof (h264_aud);
    fail_unless (memcmp (data, ctx_headers[0].data, ctx_headers[0].size) == 0);
    data += ctx_headers[0].size;
    fail_unless (memcmp (data, ctx_headers[1].data, ctx_headers[1].size) == 0);
    data += ctx_headers[1].size;
    fail_unless (memcmp (data, ctx_headers[2].data, ctx_headers[2].size) == 0);
    data += ctx_headers[2].size;

    /* IDR frame */
    fail_unless (memcmp (data, vdata->data_to_verify,
            vdata->data_to_verify_size) == 0);
  } else {
    /* IDR frame */
    guint aud_size = sizeof (h264_aud);
    fail_unless (map.size == vdata->data_to_verify_size + aud_size);
    fail_unless (memcmp (map.data, h264_aud, aud_size) == 0);
    fail_unless (memcmp (map.data + aud_size, vdata->data_to_verify,
            map.size - aud_size) == 0);
  }

  gst_buffer_unmap (buffer, &map);
  return TRUE;
}

static void
verify_h264parse_compatible_caps (guint profile_idc, guint constraint_set_flags,
    const char *profile)
{
  GstHarness *h;
  GstBuffer *buf;
  gchar *sink_caps_str;
  guint8 *frame_sps;
  guint frame_sps_len;
  GstCaps *caps;

  h = gst_harness_new ("h264parse");

  sink_caps_str = g_strdup_printf ("video/x-h264"
      ", parsed=(boolean)true"
      ", stream-format=(string){ avc, avc3, byte-stream }"
      ", alignment=(string){ au, nal }" ", profile=(string)%s", profile);

  /* create and modify sps to the given profile */
  frame_sps_len = sizeof (h264_sps);
  frame_sps = g_malloc (frame_sps_len);
  memcpy (frame_sps, h264_sps, frame_sps_len);
  fill_h264_sps (&frame_sps[5], profile_idc, constraint_set_flags, 0);

  /* set the peer pad (ie decoder) to the given profile to check the compatibility with the sps */
  gst_harness_set_caps_str (h, "video/x-h264", sink_caps_str);
  g_free (sink_caps_str);


  /* push sps buffer */
  buf = gst_buffer_new_and_alloc (frame_sps_len);
  gst_buffer_fill (buf, 0, frame_sps, frame_sps_len);
  g_free (frame_sps);
  fail_unless_equals_int (gst_harness_push (h, buf), GST_FLOW_OK);
  fail_unless (gst_harness_push_event (h, gst_event_new_eos ()));

  /* check that the caps have been negociated correctly */
  fail_unless (caps = gst_pad_get_current_caps (h->sinkpad));
  gst_caps_unref (caps);

  gst_harness_teardown (h);
}

GST_START_TEST (test_parse_normal)
{
  gst_parser_test_normal (h264_idrframe, sizeof (h264_idrframe));
}

GST_END_TEST;


GST_START_TEST (test_parse_drain_single)
{
  gst_parser_test_drain_single (h264_idrframe, sizeof (h264_idrframe));
}

GST_END_TEST;


GST_START_TEST (test_parse_drain_garbage)
{
  gst_parser_test_drain_garbage (h264_idrframe, sizeof (h264_idrframe),
      garbage_frame, sizeof (garbage_frame));
}

GST_END_TEST;

GST_START_TEST (test_parse_split)
{
  gst_parser_test_split (h264_idrframe, sizeof (h264_idrframe));
}

GST_END_TEST;


GST_START_TEST (test_parse_skip_garbage)
{
  gst_parser_test_skip_garbage (h264_idrframe, sizeof (h264_idrframe),
      garbage_frame, sizeof (garbage_frame));
}

GST_END_TEST;

#define structure_get_int(s,f) \
    (g_value_get_int(gst_structure_get_value(s,f)))
#define fail_unless_structure_field_int_equals(s,field,num) \
    fail_unless_equals_int (structure_get_int(s,field), num)

#define structure_get_string(s,f) \
    (g_value_get_string(gst_structure_get_value(s,f)))
#define fail_unless_structure_field_string_equals(s,field,name) \
    fail_unless_equals_string (structure_get_string(s,field), name)

GST_START_TEST (test_parse_detect_stream)
{
  GstCaps *caps;
  GstStructure *s;
  GstBuffer *buf;
  const GValue *val;

  /* parser does not really care that mpeg1 and mpeg2 frame data
   * should be a bit different */
  caps = gst_parser_test_get_output_caps (h264_idrframe, sizeof (h264_idrframe),
      NULL);
  fail_unless (caps != NULL);

  /* Check that the negotiated caps are as expected */
  /* When codec_data is present, parser assumes that data is version 4 */
  GST_LOG ("h264 output caps: %" GST_PTR_FORMAT, caps);
  s = gst_caps_get_structure (caps, 0);
  fail_unless (gst_structure_has_name (s, "video/x-h264"));
  fail_unless_structure_field_int_equals (s, "width", 32);
  fail_unless_structure_field_int_equals (s, "height", 24);
  fail_unless_structure_field_string_equals (s, "profile", "main");
  fail_unless_structure_field_string_equals (s, "level", "2.1");

  if (ctx_codec_data) {
    fail_unless (gst_structure_has_field (s, "codec_data"));

    /* check codec-data in more detail */
    val = gst_structure_get_value (s, "codec_data");
    fail_unless (val != NULL);
    buf = gst_value_get_buffer (val);
    fail_unless (buf != NULL);
    fail_unless (gst_buffer_get_size (buf) == h264_codec_data_size);
    fail_unless (gst_buffer_memcmp (buf, 0, h264_codec_data,
            gst_buffer_get_size (buf)) == 0);
  }

  gst_caps_unref (caps);
}

GST_END_TEST;

GST_START_TEST (test_parse_detect_stream_with_hdr_sei)
{
  GstCaps *caps;
  GstStructure *s;
  GstBuffer *buf;
  const GValue *val;
  guint8 *h264_idr_plus_sei;
  gsize h264_idr_plus_sei_size;

  h264_idr_plus_sei_size =
      sizeof (h264_sei_clli) + sizeof (h264_sei_mdcv) + sizeof (h264_idrframe);
  h264_idr_plus_sei = malloc (h264_idr_plus_sei_size);

  memcpy (h264_idr_plus_sei, h264_sei_clli, sizeof (h264_sei_clli));
  memcpy (h264_idr_plus_sei + sizeof (h264_sei_clli), h264_sei_mdcv,
      sizeof (h264_sei_mdcv));
  memcpy (h264_idr_plus_sei + sizeof (h264_sei_clli) + sizeof (h264_sei_mdcv),
      h264_idrframe, sizeof (h264_idrframe));

  /* parser does not really care that mpeg1 and mpeg2 frame data
   * should be a bit different */
  caps =
      gst_parser_test_get_output_caps (h264_idr_plus_sei,
      h264_idr_plus_sei_size, NULL);
  fail_unless (caps != NULL);

  /* Check that the negotiated caps are as expected */
  /* When codec_data is present, parser assumes that data is version 4 */
  GST_LOG ("h264 output caps: %" GST_PTR_FORMAT, caps);
  s = gst_caps_get_structure (caps, 0);
  fail_unless (gst_structure_has_name (s, "video/x-h264"));
  fail_unless_structure_field_int_equals (s, "width", 32);
  fail_unless_structure_field_int_equals (s, "height", 24);
  fail_unless_structure_field_string_equals (s, "profile", "main");
  fail_unless_structure_field_string_equals (s, "level", "2.1");

  fail_unless_structure_field_string_equals (s, "mastering-display-info",
      "7500:3000:34000:16000:13200:34500:15635:16450:10000000:1");
  fail_unless_structure_field_string_equals (s, "content-light-level",
      "1000:400");
  if (ctx_codec_data) {
    fail_unless (gst_structure_has_field (s, "codec_data"));

    /* check codec-data in more detail */
    val = gst_structure_get_value (s, "codec_data");
    fail_unless (val != NULL);
    buf = gst_value_get_buffer (val);
    fail_unless (buf != NULL);
    fail_unless (gst_buffer_get_size (buf) == h264_codec_data_size);
    fail_unless (gst_buffer_memcmp (buf, 0, h264_codec_data,
            gst_buffer_get_size (buf)) == 0);
  }
  g_free (h264_idr_plus_sei);
  gst_caps_unref (caps);
}

GST_END_TEST;

static GstStaticPadTemplate srctemplate_avc_au_and_bs_au =
    GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (SRC_CAPS_TMPL
        ", stream-format = (string) avc, alignment = (string) au; "
        SRC_CAPS_TMPL
        ", stream-format = (string) byte-stream, alignment = (string) au")
    );

GST_START_TEST (test_sink_caps_reordering)
{
  /* Upstream can handle avc and byte-stream format (in that preference order)
   * and downstream requires byte-stream.
   * Parser reorder upstream's caps to prefer the format requested downstream
   * and so avoid doing useless conversions. */
  GstElement *parser;
  GstPad *sink, *src;
  GstCaps *src_caps, *sink_caps;
  GstStructure *s;

  parser = gst_check_setup_element ("h264parse");
  fail_unless (parser);

  src = gst_check_setup_src_pad (parser, &srctemplate_avc_au_and_bs_au);
  sink = gst_check_setup_sink_pad (parser, &sinktemplate_bs_au);

  src_caps = gst_pad_get_pad_template_caps (src);
  sink_caps = gst_pad_peer_query_caps (src, src_caps);

  /* Sink pad has both format on its sink caps but prefer to use byte-stream */
  g_assert_cmpuint (gst_caps_get_size (sink_caps), ==, 2);

  s = gst_caps_get_structure (sink_caps, 0);
  g_assert_cmpstr (gst_structure_get_name (s), ==, "video/x-h264");
  g_assert_cmpstr (gst_structure_get_string (s, "alignment"), ==, "au");
  g_assert_cmpstr (gst_structure_get_string (s, "stream-format"), ==,
      "byte-stream");

  s = gst_caps_get_structure (sink_caps, 1);
  g_assert_cmpstr (gst_structure_get_name (s), ==, "video/x-h264");
  g_assert_cmpstr (gst_structure_get_string (s, "alignment"), ==, "au");
  g_assert_cmpstr (gst_structure_get_string (s, "stream-format"), ==, "avc");

  gst_caps_unref (src_caps);
  gst_caps_unref (sink_caps);
  gst_object_unref (src);
  gst_object_unref (sink);
  gst_object_unref (parser);
}

GST_END_TEST;

GST_START_TEST (test_parse_compatible_caps)
{
  verify_h264parse_compatible_caps (GST_H264_PROFILE_BASELINE, 0, "extended");

  verify_h264parse_compatible_caps (GST_H264_PROFILE_BASELINE,
      SPS_CONSTRAINT_SET_FLAG_1, "baseline");
  verify_h264parse_compatible_caps (GST_H264_PROFILE_BASELINE,
      SPS_CONSTRAINT_SET_FLAG_1, "main");
  verify_h264parse_compatible_caps (GST_H264_PROFILE_BASELINE,
      SPS_CONSTRAINT_SET_FLAG_1, "high");
  verify_h264parse_compatible_caps (GST_H264_PROFILE_BASELINE,
      SPS_CONSTRAINT_SET_FLAG_1, "high-10");
  verify_h264parse_compatible_caps (GST_H264_PROFILE_BASELINE,
      SPS_CONSTRAINT_SET_FLAG_1, "high-4:2:2");
  verify_h264parse_compatible_caps (GST_H264_PROFILE_BASELINE,
      SPS_CONSTRAINT_SET_FLAG_1, "high-4:4:4");

  verify_h264parse_compatible_caps (GST_H264_PROFILE_MAIN, 0, "high");
  verify_h264parse_compatible_caps (GST_H264_PROFILE_MAIN, 0, "high-10");
  verify_h264parse_compatible_caps (GST_H264_PROFILE_MAIN, 0, "high-4:2:2");
  verify_h264parse_compatible_caps (GST_H264_PROFILE_MAIN, 0, "high-4:4:4");

  verify_h264parse_compatible_caps (GST_H264_PROFILE_EXTENDED,
      SPS_CONSTRAINT_SET_FLAG_0, "baseline");

  verify_h264parse_compatible_caps (GST_H264_PROFILE_EXTENDED,
      SPS_CONSTRAINT_SET_FLAG_0 | SPS_CONSTRAINT_SET_FLAG_1,
      "constrained-baseline");
  verify_h264parse_compatible_caps (GST_H264_PROFILE_EXTENDED,
      SPS_CONSTRAINT_SET_FLAG_0 | SPS_CONSTRAINT_SET_FLAG_1, "baseline");
  verify_h264parse_compatible_caps (GST_H264_PROFILE_EXTENDED,
      SPS_CONSTRAINT_SET_FLAG_0 | SPS_CONSTRAINT_SET_FLAG_1, "main");
  verify_h264parse_compatible_caps (GST_H264_PROFILE_EXTENDED,
      SPS_CONSTRAINT_SET_FLAG_0 | SPS_CONSTRAINT_SET_FLAG_1, "high");
  verify_h264parse_compatible_caps (GST_H264_PROFILE_EXTENDED,
      SPS_CONSTRAINT_SET_FLAG_0 | SPS_CONSTRAINT_SET_FLAG_1, "high-10");
  verify_h264parse_compatible_caps (GST_H264_PROFILE_EXTENDED,
      SPS_CONSTRAINT_SET_FLAG_0 | SPS_CONSTRAINT_SET_FLAG_1, "high-4:2:2");
  verify_h264parse_compatible_caps (GST_H264_PROFILE_EXTENDED,
      SPS_CONSTRAINT_SET_FLAG_0 | SPS_CONSTRAINT_SET_FLAG_1, "high-4:4:4");

  verify_h264parse_compatible_caps (GST_H264_PROFILE_EXTENDED,
      SPS_CONSTRAINT_SET_FLAG_1, "main");
  verify_h264parse_compatible_caps (GST_H264_PROFILE_EXTENDED,
      SPS_CONSTRAINT_SET_FLAG_1, "high");
  verify_h264parse_compatible_caps (GST_H264_PROFILE_EXTENDED,
      SPS_CONSTRAINT_SET_FLAG_1, "high-10");
  verify_h264parse_compatible_caps (GST_H264_PROFILE_EXTENDED,
      SPS_CONSTRAINT_SET_FLAG_1, "high-4:2:2");
  verify_h264parse_compatible_caps (GST_H264_PROFILE_EXTENDED,
      SPS_CONSTRAINT_SET_FLAG_1, "high-4:4:4");

  verify_h264parse_compatible_caps (GST_H264_PROFILE_HIGH, 0, "high-10");
  verify_h264parse_compatible_caps (GST_H264_PROFILE_HIGH, 0, "high-4:2:2");
  verify_h264parse_compatible_caps (GST_H264_PROFILE_HIGH, 0, "high-4:4:4");

  verify_h264parse_compatible_caps (GST_H264_PROFILE_HIGH,
      SPS_CONSTRAINT_SET_FLAG_1, "main");
  verify_h264parse_compatible_caps (GST_H264_PROFILE_HIGH,
      SPS_CONSTRAINT_SET_FLAG_1, "high-10");
  verify_h264parse_compatible_caps (GST_H264_PROFILE_HIGH,
      SPS_CONSTRAINT_SET_FLAG_1, "high-4:2:2");
  verify_h264parse_compatible_caps (GST_H264_PROFILE_HIGH,
      SPS_CONSTRAINT_SET_FLAG_1, "high-4:4:4");

  verify_h264parse_compatible_caps (GST_H264_PROFILE_HIGH10, 0, "high-4:2:2");
  verify_h264parse_compatible_caps (GST_H264_PROFILE_HIGH10, 0, "high-4:4:4");
  verify_h264parse_compatible_caps (GST_H264_PROFILE_HIGH10,
      SPS_CONSTRAINT_SET_FLAG_1, "main");
  verify_h264parse_compatible_caps (GST_H264_PROFILE_HIGH10,
      SPS_CONSTRAINT_SET_FLAG_1, "high");
  verify_h264parse_compatible_caps (GST_H264_PROFILE_HIGH10,
      SPS_CONSTRAINT_SET_FLAG_1, "high-4:2:2");
  verify_h264parse_compatible_caps (GST_H264_PROFILE_HIGH10,
      SPS_CONSTRAINT_SET_FLAG_1, "high-4:4:4");

  verify_h264parse_compatible_caps (GST_H264_PROFILE_HIGH10,
      SPS_CONSTRAINT_SET_FLAG_3, "high-10");
  verify_h264parse_compatible_caps (GST_H264_PROFILE_HIGH10,
      SPS_CONSTRAINT_SET_FLAG_3, "high-4:2:2");
  verify_h264parse_compatible_caps (GST_H264_PROFILE_HIGH10,
      SPS_CONSTRAINT_SET_FLAG_3, "high-4:4:4");
  verify_h264parse_compatible_caps (GST_H264_PROFILE_HIGH10,
      SPS_CONSTRAINT_SET_FLAG_3, "high-4:2:2-intra");
  verify_h264parse_compatible_caps (GST_H264_PROFILE_HIGH10,
      SPS_CONSTRAINT_SET_FLAG_3, "high-4:4:4-intra");

  verify_h264parse_compatible_caps (GST_H264_PROFILE_HIGH_422, 0, "high-4:2:2");
  verify_h264parse_compatible_caps (GST_H264_PROFILE_HIGH_422, 0, "high-4:4:4");

  verify_h264parse_compatible_caps (GST_H264_PROFILE_HIGH_422,
      SPS_CONSTRAINT_SET_FLAG_1, "main");
  verify_h264parse_compatible_caps (GST_H264_PROFILE_HIGH_422,
      SPS_CONSTRAINT_SET_FLAG_1, "high");
  verify_h264parse_compatible_caps (GST_H264_PROFILE_HIGH_422,
      SPS_CONSTRAINT_SET_FLAG_1, "high-10");
  verify_h264parse_compatible_caps (GST_H264_PROFILE_HIGH_422,
      SPS_CONSTRAINT_SET_FLAG_1, "high-4:4:4");

  verify_h264parse_compatible_caps (GST_H264_PROFILE_HIGH_422,
      SPS_CONSTRAINT_SET_FLAG_3, "high-4:2:2");
  verify_h264parse_compatible_caps (GST_H264_PROFILE_HIGH_422,
      SPS_CONSTRAINT_SET_FLAG_3, "high-4:4:4");
  verify_h264parse_compatible_caps (GST_H264_PROFILE_HIGH_422,
      SPS_CONSTRAINT_SET_FLAG_3, "high-4:2:2-intra");
  verify_h264parse_compatible_caps (GST_H264_PROFILE_HIGH_422,
      SPS_CONSTRAINT_SET_FLAG_3, "high-4:4:4-intra");

  verify_h264parse_compatible_caps (GST_H264_PROFILE_HIGH_444,
      SPS_CONSTRAINT_SET_FLAG_1, "main");
  verify_h264parse_compatible_caps (GST_H264_PROFILE_HIGH_444,
      SPS_CONSTRAINT_SET_FLAG_1, "high");
  verify_h264parse_compatible_caps (GST_H264_PROFILE_HIGH_444,
      SPS_CONSTRAINT_SET_FLAG_1, "high-10");
  verify_h264parse_compatible_caps (GST_H264_PROFILE_HIGH_444,
      SPS_CONSTRAINT_SET_FLAG_1, "high-4:2:2");

  verify_h264parse_compatible_caps (GST_H264_PROFILE_HIGH_444,
      SPS_CONSTRAINT_SET_FLAG_3, "high-4:4:4");
}

GST_END_TEST;

static Suite *
h264parse_suite (void)
{
  Suite *s = suite_create (ctx_suite);
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_parse_normal);
  tcase_add_test (tc_chain, test_parse_drain_single);
  tcase_add_test (tc_chain, test_parse_drain_garbage);
  tcase_add_test (tc_chain, test_parse_split);
  tcase_add_test (tc_chain, test_parse_skip_garbage);
  tcase_add_test (tc_chain, test_parse_detect_stream);
  if (ctx_hdr_sei)
    tcase_add_test (tc_chain, test_parse_detect_stream_with_hdr_sei);
  tcase_add_test (tc_chain, test_sink_caps_reordering);

  return s;
}

static gboolean
verify_buffer_packetized (buffer_verify_data_s * vdata, GstBuffer * buffer)
{
  GstMapInfo map;

  gst_buffer_map (buffer, &map, GST_MAP_READ);

  fail_unless (map.size > 4);
  fail_unless (GST_READ_UINT32_BE (map.data) == 0x01);
  if (vdata->discard) {
    /* check separate header NALs */
    guint8 *data;
    gint size;

    if (vdata->buffer_counter == 0) {
      data = h264_aud;
      size = sizeof (h264_aud);
      vdata->discard++;
    } else if (vdata->buffer_counter == 1) {
      data = h264_sps;
      size = sizeof (h264_sps);
    } else {
      data = h264_pps;
      size = sizeof (h264_pps);
    }

    fail_unless (map.size == size);
    fail_unless (memcmp (map.data + 4, data + 4, size - 4) == 0);
  } else {
    guint8 *data;
    gint size;
    gboolean aud = vdata->buffer_counter % 2;
    if (aud) {
      data = h264_aud;
      size = sizeof (h264_aud);
    } else {
      data = (gpointer) vdata->data_to_verify;
      size = map.size;
    }

    fail_unless (map.size == size);
    fail_unless (memcmp (map.data + 4, data + 4, size - 4) == 0);
  }
  gst_buffer_unmap (buffer, &map);

  return TRUE;
}

GST_START_TEST (test_parse_packetized)
{
  guint8 *frame;
  GstCaps *caps;
  GstBuffer *cdata;
  GstStructure *s;
  gchar *desc;

  /* make AVC frame */
  frame = g_malloc (sizeof (h264_idrframe));
  GST_WRITE_UINT32_BE (frame, sizeof (h264_idrframe) - 4);
  memcpy (frame + 4, h264_idrframe + 4, sizeof (h264_idrframe) - 4);

  /* some caps messing */
  caps = gst_caps_from_string (SRC_CAPS_TMPL);
  cdata =
      gst_buffer_new_wrapped_full (GST_MEMORY_FLAG_READONLY, h264_codec_data,
      h264_codec_data_size, 0, h264_codec_data_size, NULL, NULL);
  gst_caps_set_simple (caps, "codec_data", GST_TYPE_BUFFER, cdata,
      "stream-format", G_TYPE_STRING, "avc", NULL);
  gst_buffer_unref (cdata);
  desc = gst_caps_to_string (caps);
  gst_caps_unref (caps);

  caps = gst_parser_test_get_output_caps (frame, sizeof (h264_idrframe), desc);
  g_free (desc);
  g_free (frame);

  /* minor caps checks */
  GST_LOG ("h264 output caps: %" GST_PTR_FORMAT, caps);
  s = gst_caps_get_structure (caps, 0);
  fail_unless (gst_structure_has_name (s, "video/x-h264"));
  fail_unless_structure_field_int_equals (s, "width", 32);
  fail_unless_structure_field_int_equals (s, "height", 24);

  gst_caps_unref (caps);
}

GST_END_TEST;

static Suite *
h264parse_packetized_suite (void)
{
  Suite *s = suite_create (ctx_suite);
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_parse_packetized);

  return s;
}

/* These were generated using pipeline:
 * gst-launch-1.0 videotestsrc num-buffers=2 pattern=green \
 *     ! video/x-raw,width=128,height=128 \
 *     ! openh264enc num-slices=2 \
 *     ! fakesink dump=1
 */

/* codec-data */
static guint8 h264_slicing_codec_data[] = {
  0x01, 0x42, 0xc0, 0x0b, 0xff, 0xe1, 0x00, 0x0e,
  0x67, 0x42, 0xc0, 0x0b, 0x8c, 0x8d, 0x41, 0x02,
  0x24, 0x03, 0xc2, 0x21, 0x1a, 0x80, 0x01, 0x00,
  0x04, 0x68, 0xce, 0x3c, 0x80
};

/* SPS */
static guint8 h264_slicing_sps[] = {
  0x00, 0x00, 0x00, 0x01, 0x67, 0x42, 0xc0, 0x0b,
  0x8c, 0x8d, 0x41, 0x02, 0x24, 0x03, 0xc2, 0x21,
  0x1a, 0x80
};

/* PPS */
static guint8 h264_slicing_pps[] = {
  0x00, 0x00, 0x00, 0x01, 0x68, 0xce, 0x3c, 0x80
};

/* IDR Slice 1 */
static guint8 h264_idr_slice_1[] = {
  0x00, 0x00, 0x00, 0x01, 0x65, 0xb8, 0x00, 0x04,
  0x00, 0x00, 0x11, 0xff, 0xff, 0xf8, 0x22, 0x8a,
  0x1f, 0x1c, 0x00, 0x04, 0x0a, 0x63, 0x80, 0x00,
  0x81, 0xec, 0x9a, 0x93, 0x93, 0x93, 0x93, 0x93,
  0x93, 0xad, 0x57, 0x5d, 0x75, 0xd7, 0x5d, 0x75,
  0xd7, 0x5d, 0x75, 0xd7, 0x5d, 0x75, 0xd7, 0x5d,
  0x75, 0xd7, 0x5d, 0x78
};

/* IDR Slice 2 */
static guint8 h264_idr_slice_2[] = {
  0x00, 0x00, 0x00, 0x01, 0x65, 0x04, 0x2e, 0x00,
  0x01, 0x00, 0x00, 0x04, 0x7f, 0xff, 0xfe, 0x08,
  0xa2, 0x87, 0xc7, 0x00, 0x01, 0x02, 0x98, 0xe0,
  0x00, 0x20, 0x7b, 0x26, 0xa4, 0xe4, 0xe4, 0xe4,
  0xe4, 0xe4, 0xeb, 0x55, 0xd7, 0x5d, 0x75, 0xd7,
  0x5d, 0x75, 0xd7, 0x5d, 0x75, 0xd7, 0x5d, 0x75,
  0xd7, 0x5d, 0x75, 0xd7, 0x5e
};

/* P Slice 1 */
static guint8 h264_slice_1[] = {
  0x00, 0x00, 0x00, 0x01, 0x61, 0xe0, 0x00, 0x40,
  0x00, 0x9c, 0x82, 0x3c, 0x10, 0xc0
};

/* P Slice 2 */
static guint8 h264_slice_2[] = {
  0x00, 0x00, 0x00, 0x01, 0x61, 0x04, 0x38, 0x00,
  0x10, 0x00, 0x27, 0x20, 0x8f, 0x04, 0x30
};

static inline GstBuffer *
wrap_buffer (const guint8 * buf, gsize size, GstClockTime pts,
    GstBufferFlags flags)
{
  GstBuffer *buffer;

  buffer = gst_buffer_new_wrapped_full (GST_MEMORY_FLAG_READONLY,
      (gpointer) buf, size, 0, size, NULL, NULL);
  GST_BUFFER_PTS (buffer) = pts;
  GST_BUFFER_FLAGS (buffer) |= flags;

  return buffer;
}

static inline GstBuffer *
composite_buffer (GstClockTime pts, GstBufferFlags flags, gint count, ...)
{
  va_list vl;
  gint i;
  const guint8 *data;
  gsize size;
  GstBuffer *buffer;

  va_start (vl, count);

  buffer = gst_buffer_new ();
  for (i = 0; i < count; i++) {
    data = va_arg (vl, guint8 *);
    size = va_arg (vl, gsize);

    buffer = gst_buffer_append (buffer, wrap_buffer (data, size, 0, 0));
  }
  GST_BUFFER_PTS (buffer) = pts;
  GST_BUFFER_FLAGS (buffer) |= flags;

  va_end (vl);

  return buffer;
}

#define pull_and_check_full(h, data, size, pts, flags) \
{ \
  GstBuffer *b = gst_harness_pull (h); \
  gst_check_buffer_data (b, data, size); \
  fail_unless_equals_clocktime (GST_BUFFER_PTS (b), pts); \
  if (flags) \
    fail_unless (GST_BUFFER_FLAG_IS_SET (b, flags)); \
  gst_buffer_unref (b); \
}

#define pull_and_check(h, data, pts, flags) \
  pull_and_check_full (h, data, sizeof (data), pts, flags)

#define pull_and_drop(h) \
  G_STMT_START { \
    GstBuffer *b = gst_harness_pull (h); \
    gst_buffer_unref (b); \
  } G_STMT_END;

GST_START_TEST (test_parse_sliced_nal_nal)
{
  GstHarness *h = gst_harness_new ("h264parse");
  GstBuffer *buf;

  gst_harness_set_caps_str (h,
      "video/x-h264,stream-format=byte-stream,alignment=nal,parsed=false,framerate=30/1",
      "video/x-h264,stream-format=byte-stream,alignment=nal,parsed=true");

  buf = wrap_buffer (h264_slicing_sps, sizeof (h264_slicing_sps), 10, 0);
  fail_unless_equals_int (gst_harness_push (h, buf), GST_FLOW_OK);

  buf = wrap_buffer (h264_slicing_pps, sizeof (h264_slicing_pps), 10, 0);
  fail_unless_equals_int (gst_harness_push (h, buf), GST_FLOW_OK);

  /* parser must have inserted AUD before the headers, with the same PTS */
  pull_and_check (h, h264_aud, 10, 0);

  /* drop the header buffers */
  while ((buf = gst_harness_try_pull (h)))
    gst_buffer_unref (buf);

  /* reported latency must be zero */
  fail_unless_equals_clocktime (gst_harness_query_latency (h), 0);

  /* test some flow with 2 slices.
   * 1st slice gets the input PTS, second gets NONE */
  buf = wrap_buffer (h264_idr_slice_1, sizeof (h264_idr_slice_1), 100, 0);
  fail_unless_equals_int (gst_harness_push (h, buf), GST_FLOW_OK);
  fail_unless_equals_int (gst_harness_buffers_in_queue (h), 1);
  pull_and_check (h, h264_idr_slice_1, 100, 0);

  buf = wrap_buffer (h264_idr_slice_2, sizeof (h264_idr_slice_2), 100, 0);
  fail_unless_equals_int (gst_harness_push (h, buf), GST_FLOW_OK);
  fail_unless_equals_int (gst_harness_buffers_in_queue (h), 1);
  pull_and_check (h, h264_idr_slice_2, -1, 0);

  buf = wrap_buffer (h264_idr_slice_1, sizeof (h264_idr_slice_1), 200, 0);
  fail_unless_equals_int (gst_harness_push (h, buf), GST_FLOW_OK);
  fail_unless_equals_int (gst_harness_buffers_in_queue (h), 2);
  pull_and_check (h, h264_aud, 200, 0);
  pull_and_check (h, h264_idr_slice_1, 200, 0);

  buf = wrap_buffer (h264_idr_slice_2, sizeof (h264_idr_slice_2), 200, 0);
  fail_unless_equals_int (gst_harness_push (h, buf), GST_FLOW_OK);
  fail_unless_equals_int (gst_harness_buffers_in_queue (h), 1);
  pull_and_check (h, h264_idr_slice_2, -1, 0);

  buf = wrap_buffer (h264_idr_slice_1, sizeof (h264_idr_slice_1), 250, 0);
  fail_unless_equals_int (gst_harness_push (h, buf), GST_FLOW_OK);
  fail_unless_equals_int (gst_harness_buffers_in_queue (h), 2);
  pull_and_check (h, h264_aud, 250, 0);
  pull_and_check (h, h264_idr_slice_1, 250, 0);

  /* 1st slice starts a new AU, even though the previous one is incomplete.
   * DISCONT must also be propagated */
  buf = wrap_buffer (h264_idr_slice_1, sizeof (h264_idr_slice_1), 400,
      GST_BUFFER_FLAG_DISCONT);
  fail_unless_equals_int (gst_harness_push (h, buf), GST_FLOW_OK);
  fail_unless_equals_int (gst_harness_buffers_in_queue (h), 2);
  pull_and_check (h, h264_aud, 400, 0);
  pull_and_check (h, h264_idr_slice_1, 400, GST_BUFFER_FLAG_DISCONT);

  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_parse_sliced_au_nal)
{
  GstHarness *h = gst_harness_new ("h264parse");
  GstBuffer *buf;

  gst_harness_set_caps_str (h,
      "video/x-h264,stream-format=byte-stream,alignment=au,parsed=false,framerate=30/1",
      "video/x-h264,stream-format=byte-stream,alignment=nal,parsed=true");

  /* push the whole AU in a single buffer */
  buf = composite_buffer (100, 0, 4,
      h264_slicing_sps, sizeof (h264_slicing_sps),
      h264_slicing_pps, sizeof (h264_slicing_pps),
      h264_idr_slice_1, sizeof (h264_idr_slice_1),
      h264_idr_slice_2, sizeof (h264_idr_slice_2));
  fail_unless_equals_int (gst_harness_push (h, buf), GST_FLOW_OK);

  /* parser must have inserted AUD before the headers, with the same PTS */
  pull_and_check (h, h264_aud, 100, 0);

  /* drop the headers */
  fail_unless (gst_harness_buffers_in_queue (h) > 2);
  while (gst_harness_buffers_in_queue (h) > 2)
    pull_and_drop (h);

  /* reported latency must be zero */
  fail_unless_equals_clocktime (gst_harness_query_latency (h), 0);

  /* 1st slice here doens't have a PTS
   * because it was present in the first header NAL */
  pull_and_check (h, h264_idr_slice_1, -1, 0);
  pull_and_check (h, h264_idr_slice_2, -1, 0);

  /* new AU. we expect AUD to be inserted and 1st slice to have the same PTS */
  buf = composite_buffer (200, 0, 2,
      h264_idr_slice_1, sizeof (h264_idr_slice_1),
      h264_idr_slice_2, sizeof (h264_idr_slice_2));
  fail_unless_equals_int (gst_harness_push (h, buf), GST_FLOW_OK);
  fail_unless_equals_int (gst_harness_buffers_in_queue (h), 3);
  pull_and_check (h, h264_aud, 200, 0);
  pull_and_check (h, h264_idr_slice_1, 200, 0);
  pull_and_check (h, h264_idr_slice_2, -1, 0);

  /* DISCONT must be propagated */
  buf = composite_buffer (400, GST_BUFFER_FLAG_DISCONT, 2,
      h264_idr_slice_1, sizeof (h264_idr_slice_1),
      h264_idr_slice_2, sizeof (h264_idr_slice_2));
  fail_unless_equals_int (gst_harness_push (h, buf), GST_FLOW_OK);
  fail_unless_equals_int (gst_harness_buffers_in_queue (h), 3);
  pull_and_check (h, h264_aud, 400, 0);
  pull_and_check (h, h264_idr_slice_1, 400, GST_BUFFER_FLAG_DISCONT);
  pull_and_check (h, h264_idr_slice_2, -1, 0);

  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_parse_sliced_nal_au)
{
  GstHarness *h = gst_harness_new ("h264parse");
  GstBuffer *buf;

  gst_harness_set_caps_str (h,
      "video/x-h264,stream-format=byte-stream,alignment=nal,parsed=false,framerate=30/1",
      "video/x-h264,stream-format=byte-stream,alignment=au,parsed=true");

  buf = wrap_buffer (h264_slicing_sps, sizeof (h264_slicing_sps), 100, 0);
  fail_unless_equals_int (gst_harness_push (h, buf), GST_FLOW_OK);

  buf = wrap_buffer (h264_slicing_pps, sizeof (h264_slicing_pps), 100, 0);
  fail_unless_equals_int (gst_harness_push (h, buf), GST_FLOW_OK);

  buf = wrap_buffer (h264_idr_slice_1, sizeof (h264_idr_slice_1), 100, 0);
  fail_unless_equals_int (gst_harness_push (h, buf), GST_FLOW_OK);

  buf = wrap_buffer (h264_idr_slice_2, sizeof (h264_idr_slice_2), 100, 0);
  fail_unless_equals_int (gst_harness_push (h, buf), GST_FLOW_OK);

  /* no output yet, it will be pushed as soon as
   * the parser recognizes the new AU */
  fail_unless_equals_int (gst_harness_buffers_in_queue (h), 0);

  buf = wrap_buffer (h264_idr_slice_1, sizeof (h264_idr_slice_1), 200, 0);
  fail_unless_equals_int (gst_harness_push (h, buf), GST_FLOW_OK);

  fail_unless_equals_int (gst_harness_buffers_in_queue (h), 1);

  {
    GstMapInfo info;

    buf = composite_buffer (100, 0, 5,
        h264_aud, sizeof (h264_aud),
        h264_slicing_sps, sizeof (h264_slicing_sps),
        h264_slicing_pps, sizeof (h264_slicing_pps),
        h264_idr_slice_1, sizeof (h264_idr_slice_1),
        h264_idr_slice_2, sizeof (h264_idr_slice_2));
    gst_buffer_map (buf, &info, GST_MAP_READ);

    pull_and_check_full (h, info.data, info.size, 100, 0);

    gst_buffer_unmap (buf, &info);
    gst_buffer_unref (buf);
  }

  /* reported latency must be 1 frame (@ 30fps because of sink pad caps) */
  fail_unless_equals_clocktime (gst_harness_query_latency (h),
      gst_util_uint64_scale (GST_SECOND, 1, 30));

  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_parse_sliced_sps_pps_sps)
{
  GstHarness *h = gst_harness_new ("h264parse");
  GstBuffer *buf;

  gst_harness_set_caps_str (h,
      "video/x-h264,stream-format=byte-stream,alignment=nal,parsed=false,framerate=30/1",
      "video/x-h264,stream-format=byte-stream,alignment=au,parsed=true");

  buf = wrap_buffer (h264_slicing_sps, sizeof (h264_slicing_sps), 100, 0);
  fail_unless_equals_int (gst_harness_push (h, buf), GST_FLOW_OK);

  buf = wrap_buffer (h264_slicing_pps, sizeof (h264_slicing_pps), 100, 0);
  fail_unless_equals_int (gst_harness_push (h, buf), GST_FLOW_OK);

  buf = wrap_buffer (h264_idr_slice_1, sizeof (h264_idr_slice_1), 100, 0);
  fail_unless_equals_int (gst_harness_push (h, buf), GST_FLOW_OK);

  /* no output yet, it will be pushed as soon as
   * the parser recognizes the new AU */
  fail_unless_equals_int (gst_harness_buffers_in_queue (h), 0);

  buf = wrap_buffer (h264_slicing_sps, sizeof (h264_slicing_sps), 200, 0);
  fail_unless_equals_int (gst_harness_push (h, buf), GST_FLOW_OK);

  /* no PP, just a SPS here is valid */

  buf = wrap_buffer (h264_idr_slice_1, sizeof (h264_idr_slice_1), 200, 0);
  fail_unless_equals_int (gst_harness_push (h, buf), GST_FLOW_OK);

  fail_unless_equals_int (gst_harness_buffers_in_queue (h), 1);

  buf = wrap_buffer (h264_idr_slice_1, sizeof (h264_idr_slice_1), 300, 0);
  fail_unless_equals_int (gst_harness_push (h, buf), GST_FLOW_OK);

  fail_unless_equals_int (gst_harness_buffers_in_queue (h), 2);

  {
    GstMapInfo info;

    buf = composite_buffer (100, 0, 4,
        h264_aud, sizeof (h264_aud),
        h264_slicing_sps, sizeof (h264_slicing_sps),
        h264_slicing_pps, sizeof (h264_slicing_pps),
        h264_idr_slice_1, sizeof (h264_idr_slice_1));
    gst_buffer_map (buf, &info, GST_MAP_READ);

    pull_and_check_full (h, info.data, info.size, 100, 0);

    gst_buffer_unmap (buf, &info);
    gst_buffer_unref (buf);

    buf = composite_buffer (200, 0, 3,
        h264_aud, sizeof (h264_aud),
        h264_slicing_sps, sizeof (h264_slicing_sps),
        h264_idr_slice_1, sizeof (h264_idr_slice_1));
    gst_buffer_map (buf, &info, GST_MAP_READ);

    pull_and_check_full (h, info.data, info.size, 200, 0);

    gst_buffer_unmap (buf, &info);
    gst_buffer_unref (buf);
  }

  gst_harness_teardown (h);
}

GST_END_TEST;


static Suite *
h264parse_sliced_suite (void)
{
  Suite *s = suite_create (ctx_suite);
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_parse_sliced_nal_nal);
  tcase_add_test (tc_chain, test_parse_sliced_au_nal);
  tcase_add_test (tc_chain, test_parse_sliced_nal_au);
  tcase_add_test (tc_chain, test_parse_sliced_sps_pps_sps);

  return s;
}


GST_START_TEST (test_parse_sei_closedcaptions)
{
  GstVideoCaptionMeta *cc;
  GstHarness *h;
  GstBuffer *buf;

  const guint8 cc_sei_plus_idr[] = {
    0x00, 0x00, 0x00, 0x4b, 0x06, 0x04, 0x47, 0xb5, 0x00, 0x31, 0x47, 0x41,
    0x39, 0x34, 0x03, 0xd4,
    0xff, 0xfc, 0x80, 0x80, 0xfd, 0x80, 0x80, 0xfa, 0x00, 0x00, 0xfa, 0x00,
    0x00, 0xfa, 0x00, 0x00,
    0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00,
    0xfa, 0x00, 0x00, 0xfa,
    0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa,
    0x00, 0x00, 0xfa, 0x00,
    0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00, 0x00, 0xfa, 0x00,
    0x00, 0xff, 0x80,
    /* IDR frame (doesn't necessarily match caps) */
    0x00, 0x00, 0x00, 0x14, 0x65, 0x88, 0x84, 0x00,
    0x10, 0xff, 0xfe, 0xf6, 0xf0, 0xfe, 0x05, 0x36,
    0x56, 0x04, 0x50, 0x96, 0x7b, 0x3f, 0x53, 0xe1
  };
  const gsize cc_sei_plus_idr_size = sizeof (cc_sei_plus_idr);

  h = gst_harness_new ("h264parse");

  gst_harness_set_src_caps_str (h,
      "video/x-h264, stream-format=(string)avc, alignment=(string)au,"
      " codec_data=(buffer)014d4015ffe10017674d4015eca4bf2e0220000003002ee6b28001e2c5b2c001000468ebecb2,"
      " width=(int)32, height=(int)24, framerate=(fraction)30/1,"
      " pixel-aspect-ratio=(fraction)1/1");

  buf = gst_buffer_new_and_alloc (cc_sei_plus_idr_size);
  gst_buffer_fill (buf, 0, cc_sei_plus_idr, cc_sei_plus_idr_size);
  fail_unless_equals_int (gst_harness_push (h, buf), GST_FLOW_OK);

  buf = gst_harness_pull (h);
  cc = gst_buffer_get_video_caption_meta (buf);
  fail_unless (cc != NULL);
  fail_unless_equals_int (cc->caption_type, GST_VIDEO_CAPTION_TYPE_CEA708_RAW);
  fail_unless_equals_int (cc->size, 60);
  fail_unless_equals_int (cc->data[0], 0xfc);
  fail_unless_equals_int (cc->data[3], 0xfd);
  gst_buffer_unref (buf);

  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_parse_skip_to_4bytes_sc)
{
  GstHarness *h;
  GstBuffer *buf1, *buf2;
  const guint8 initial_bytes[] = { 0x00, 0x00, 0x00, 0x00, 0x01 };
  GstMapInfo map;

  h = gst_harness_new ("h264parse");

  gst_harness_set_caps_str (h, "video/x-h264, stream-format=byte-stream",
      "video/x-h264, stream-format=byte-stream, alignment=nal");

  /* padding bytes, four bytes start code. */
  buf1 = wrap_buffer (initial_bytes, sizeof (initial_bytes), 100, 0);

  /* The second contains the an AUD, starting from NAL identification byte,
   * and is followed by SPS, PPS and IDR */
  buf2 = composite_buffer (100, 0, 4, h264_aud + 4, sizeof (h264_aud) - 4,
      h264_sps, sizeof (h264_sps), h264_pps, sizeof (h264_pps),
      h264_idrframe, sizeof (h264_idrframe));

  fail_unless_equals_int (gst_harness_push (h, buf1), GST_FLOW_OK);
  fail_unless_equals_int (gst_harness_buffers_in_queue (h), 0);

  fail_unless_equals_int (gst_harness_push (h, buf2), GST_FLOW_OK);
  /* The parser will deliver AUD, SPS, PPS as it now have complete caps */
  fail_unless_equals_int (gst_harness_buffers_in_queue (h), 3);

  buf1 = gst_harness_pull (h);
  gst_buffer_map (buf1, &map, GST_MAP_READ);
  fail_unless_equals_int (gst_buffer_get_size (buf1), sizeof (h264_aud));
  gst_buffer_unmap (buf1, &map);
  gst_buffer_unref (buf1);

  gst_harness_teardown (h);
}

GST_END_TEST;

typedef enum
{
  PACKETIZED_AU = 0,
  /* TODO: packetized with nal alignment if we expect that should work? */
  BYTESTREAM_AU,
  BYTESTREAM_NAL,
} H264ParseStreamType;

static const gchar *
stream_type_to_caps_str (H264ParseStreamType type)
{
  switch (type) {
    case PACKETIZED_AU:
      return "video/x-h264,stream-format=avc,alignment=au";
    case BYTESTREAM_AU:
      return "video/x-h264,stream-format=byte-stream,alignment=au";
    case BYTESTREAM_NAL:
      return "video/x-h264,stream-format=byte-stream,alignment=nal";
  }

  g_assert_not_reached ();

  return NULL;
}

static GstMemory *
nalu_to_memory (H264ParseStreamType type, const guint8 * data, gsize size)
{
  gpointer dump = g_memdup2 (data, size);

  if (type == PACKETIZED_AU) {
    guint32 nalu_size;

    nalu_size = size - 4;
    nalu_size = GUINT32_TO_BE (nalu_size);
    memcpy (dump, &nalu_size, sizeof (nalu_size));
  }

  return gst_memory_new_wrapped (0, dump, size, 0, size, dump, g_free);
}

static GList *
create_aud_test_buffers (H264ParseStreamType type, gboolean inband_aud)
{
  GList *list = NULL;
  GstBuffer *buf = NULL;

#define APPEND_NALU_TO_BUFFER(type,nalu,end_of_au) G_STMT_START { \
  if (!buf) { \
    buf = gst_buffer_new (); \
  } \
  gst_buffer_append_memory (buf, nalu_to_memory (type, nalu, \
      sizeof (nalu))); \
  if (type == BYTESTREAM_NAL || end_of_au) { \
    list = g_list_append (list, buf); \
    buf = NULL; \
  } \
} G_STMT_END

  if (inband_aud)
    APPEND_NALU_TO_BUFFER (type, h264_aud, FALSE);

  APPEND_NALU_TO_BUFFER (type, h264_slicing_sps, FALSE);
  APPEND_NALU_TO_BUFFER (type, h264_slicing_pps, FALSE);
  APPEND_NALU_TO_BUFFER (type, h264_idr_slice_1, FALSE);
  APPEND_NALU_TO_BUFFER (type, h264_idr_slice_2, TRUE);

  if (inband_aud)
    APPEND_NALU_TO_BUFFER (type, h264_aud, FALSE);

  APPEND_NALU_TO_BUFFER (type, h264_slice_1, FALSE);
  APPEND_NALU_TO_BUFFER (type, h264_slice_2, TRUE);

#undef APPEND_NALU_TO_BUFFER

  return list;
}

static void
check_aud_insertion (gboolean inband_aud, H264ParseStreamType in_type,
    H264ParseStreamType out_type)
{
  GstHarness *h;
  GList *in_buffers = NULL;
  GList *expected_buffers = NULL;
  GList *result_buffers = NULL;
  GList *iter, *walk;
  GstCaps *in_caps, *out_caps;
  gboolean aud_in_output;
  GstBuffer *buf;

  h = gst_harness_new ("h264parse");

  in_caps = gst_caps_from_string (stream_type_to_caps_str (in_type));
  if (in_type == PACKETIZED_AU) {
    GstBuffer *cdata_buf = gst_buffer_new_memdup (h264_slicing_codec_data,
        sizeof (h264_slicing_codec_data));
    gst_caps_set_simple (in_caps,
        "codec_data", GST_TYPE_BUFFER, cdata_buf, NULL);
    gst_buffer_unref (cdata_buf);
  }

  out_caps = gst_caps_from_string (stream_type_to_caps_str (out_type));

  gst_harness_set_caps (h, in_caps, out_caps);

  in_buffers = create_aud_test_buffers (in_type, inband_aud);

  if (out_type == BYTESTREAM_AU || out_type == BYTESTREAM_NAL) {
    /* In case of byte-stream output, parse will insert AUD always */
    aud_in_output = TRUE;
  } else if (inband_aud) {
    /* Parse will not drop AUD in any case */
    aud_in_output = TRUE;
  } else {
    /* Cases where input bitstream doesn't contain AUD and output format is
     * packetized. In this case parse will not insert AUD */
    aud_in_output = FALSE;
  }

  expected_buffers = create_aud_test_buffers (out_type, aud_in_output);

  for (iter = in_buffers; iter; iter = g_list_next (iter)) {
    buf = (GstBuffer *) iter->data;
    fail_unless_equals_int (gst_harness_push (h, gst_buffer_ref (buf)),
        GST_FLOW_OK);
  }

  /* EOS for pending buffers to be drained if any */
  gst_harness_push_event (h, gst_event_new_eos ());

  while ((buf = gst_harness_try_pull (h)))
    result_buffers = g_list_append (result_buffers, buf);

  fail_unless_equals_int (g_list_length (result_buffers),
      g_list_length (expected_buffers));

  for (iter = expected_buffers, walk = result_buffers; iter && walk;
      iter = g_list_next (iter), walk = g_list_next (walk)) {
    GstBuffer *buf1, *buf2;
    GstMapInfo map1, map2;

    buf1 = (GstBuffer *) iter->data;
    buf2 = (GstBuffer *) walk->data;

    gst_buffer_map (buf1, &map1, GST_MAP_READ);
    gst_buffer_map (buf2, &map2, GST_MAP_READ);

    fail_unless_equals_int (map1.size, map2.size);
    fail_unless (memcmp (map1.data, map2.data, map1.size) == 0);
    gst_buffer_unmap (buf1, &map1);
    gst_buffer_unmap (buf2, &map2);
  }

  g_list_free_full (in_buffers, (GDestroyNotify) gst_buffer_unref);
  g_list_free_full (expected_buffers, (GDestroyNotify) gst_buffer_unref);
  g_list_free_full (result_buffers, (GDestroyNotify) gst_buffer_unref);

  gst_harness_teardown (h);
}

GST_START_TEST (test_parse_aud_insert)
{
  gboolean inband_aud[] = {
    TRUE, FALSE
  };
  H264ParseStreamType stream_types[] = {
    PACKETIZED_AU, BYTESTREAM_AU, BYTESTREAM_NAL
  };
  guint i, j, k;

  for (i = 0; i < G_N_ELEMENTS (inband_aud); i++) {
    for (j = 0; j < G_N_ELEMENTS (stream_types); j++) {
      for (k = 0; k < G_N_ELEMENTS (stream_types); k++) {
        check_aud_insertion (inband_aud[i], stream_types[j], stream_types[k]);
      }
    }
  }
}

GST_END_TEST;

GST_START_TEST (test_parse_sei_userdefinedunregistered)
{
  GstVideoSEIUserDataUnregisteredMeta *meta;
  GstHarness *h;
  GstBuffer *buf;

  const guint8 misb_sei[] = {
    0x00, 0x00, 0x00, 0x20, 0x06, 0x05, 0x1c, 0x4d,
    0x49, 0x53, 0x50, 0x6d, 0x69, 0x63, 0x72, 0x6f,
    0x73, 0x65, 0x63, 0x74, 0x69, 0x6d, 0x65, 0x1f,
    0x00, 0x05, 0xff, 0x21, 0x7e, 0xff, 0x29, 0xb5,
    0xff, 0xdc, 0x13, 0x80,
    /* IDR frame (doesn't match caps) */
    0x00, 0x00, 0x00, 0x14, 0x65, 0x88, 0x84, 0x00,
    0x10, 0xff, 0xfe, 0xf6, 0xf0, 0xfe, 0x05, 0x36,
    0x56, 0x04, 0x50, 0x96, 0x7b, 0x3f, 0x53, 0xe1
  };
  const gsize misb_sei_size = sizeof (misb_sei);

  // Expected result - time status plus padded data
  const guint8 st0604_data[] = {
    0x1f, 0x00, 0x05, 0xff, 0x21, 0x7e, 0xff, 0x29,
    0xb5, 0xff, 0xdc, 0x13
  };

  h = gst_harness_new ("h264parse");

  gst_harness_set_src_caps_str (h,
      "video/x-h264, stream-format=(string)avc,"
      " width=(int)1920, height=(int)1080, framerate=(fraction)25/1,"
      " bit-depth-chroma=(uint)8, parsed=(boolean)true,"
      " alignment=(string)au, profile=(string)high, level=(string)4,"
      " codec_data=(buffer)01640028ffe1001a67640028acb200f0044fcb080000030008000003019478c1924001000568ebccb22c");

  buf = gst_buffer_new_and_alloc (misb_sei_size);
  gst_buffer_fill (buf, 0, misb_sei, misb_sei_size);
  fail_unless_equals_int (gst_harness_push (h, buf), GST_FLOW_OK);

  buf = gst_harness_pull (h);
  meta = gst_buffer_get_video_sei_user_data_unregistered_meta (buf);
  fail_unless (meta != NULL);

  fail_unless (memcmp (meta->uuid, H264_MISP_MICROSECTIME, 16) == 0);
  fail_unless (memcmp (meta->data, st0604_data, 12) == 0);

  gst_buffer_unref (buf);

  gst_harness_teardown (h);
}

GST_END_TEST;

/*
 * TODO:
 *   - Both push- and pull-modes need to be tested
 *      * Pull-mode & EOS
 */

int
main (int argc, char **argv)
{
  int nf = 0;

  Suite *s;
  SRunner *sr;

  gst_check_init (&argc, &argv);

  /* globabl init test context */
  ctx_factory = "h264parse";
  ctx_sink_template = &sinktemplate_bs_nal;
  ctx_src_template = &srctemplate;
  ctx_headers[0].data = h264_sps;
  ctx_headers[0].size = sizeof (h264_sps);
  ctx_headers[1].data = h264_sei_buffering_period;
  ctx_headers[1].size = sizeof (h264_sei_buffering_period);
  ctx_headers[2].data = h264_pps;
  ctx_headers[2].size = sizeof (h264_pps);
  ctx_verify_buffer = verify_buffer;
  ctx_frame_generated = TRUE;
  /* discard initial sps/pps buffers */
  ctx_discard = 3;
  /* no timing info to parse */
  ctx_no_metadata = TRUE;
  ctx_codec_data = FALSE;
  ctx_hdr_sei = FALSE;

  h264_codec_data = h264_avc_codec_data;
  h264_codec_data_size = sizeof (h264_avc_codec_data);

  ctx_suite = "h264parse_to_bs_nal";
  s = h264parse_suite ();
  nf += gst_check_run_suite (s, ctx_suite, __FILE__ "_to_bs_nal.c");

  /* setup and tweak to handle bs au output */
  ctx_suite = "h264parse_to_bs_au";
  ctx_sink_template = &sinktemplate_bs_au;
  ctx_verify_buffer = verify_buffer_bs_au;
  ctx_discard = 0;
  ctx_frame_generated = FALSE;
  ctx_hdr_sei = TRUE;
  s = h264parse_suite ();
  nf += gst_check_run_suite (s, ctx_suite, __FILE__ "_to_bs_au.c");

  /* setup and tweak to handle avc au output */
  ctx_suite = "h264parse_to_avc_au";
  ctx_sink_template = &sinktemplate_avc_au;
  ctx_verify_buffer = verify_buffer;
  ctx_discard = 0;
  ctx_codec_data = TRUE;
  ctx_hdr_sei = FALSE;

  s = h264parse_suite ();
  sr = srunner_create (s);
  srunner_run_all (sr, CK_NORMAL);
  nf += srunner_ntests_failed (sr);
  srunner_free (sr);

  /* setup and tweak to handle avc3 au output */
  h264_codec_data = h264_avc3_codec_data;
  h264_codec_data_size = sizeof (h264_avc3_codec_data);
  ctx_suite = "h264parse_to_avc3_au";
  ctx_sink_template = &sinktemplate_avc3_au;
  ctx_discard = 0;
  ctx_codec_data = TRUE;

  s = h264parse_suite ();
  nf += gst_check_run_suite (s, ctx_suite, __FILE__ "_to_avc3_au.c");

  /* setup and tweak to handle avc packetized input */
  h264_codec_data = h264_avc_codec_data;
  h264_codec_data_size = sizeof (h264_avc_codec_data);
  ctx_suite = "h264parse_packetized";
  /* turn into separate byte stream NALs */
  ctx_sink_template = &sinktemplate_bs_nal;
  /* and ignore inserted codec-data NALs */
  ctx_discard = 2;
  ctx_frame_generated = TRUE;
  /* no more config headers */
  ctx_headers[0].data = NULL;
  ctx_headers[1].data = NULL;
  ctx_headers[2].data = NULL;
  ctx_headers[0].size = 0;
  ctx_headers[1].size = 0;
  ctx_headers[2].size = 0;
  /* and need adapter buffer check */
  ctx_verify_buffer = verify_buffer_packetized;

  s = h264parse_packetized_suite ();
  nf += gst_check_run_suite (s, ctx_suite, __FILE__ "_packetized.c");

  ctx_suite = "h264parse_sliced";
  s = h264parse_sliced_suite ();
  nf += gst_check_run_suite (s, ctx_suite, __FILE__ "_sliced.c");

  {
    TCase *tc_chain = tcase_create ("general");

    s = suite_create ("h264parse");
    suite_add_tcase (s, tc_chain);
    tcase_add_test (tc_chain, test_parse_sei_closedcaptions);
    tcase_add_test (tc_chain, test_parse_compatible_caps);
    tcase_add_test (tc_chain, test_parse_skip_to_4bytes_sc);
    tcase_add_test (tc_chain, test_parse_aud_insert);
    tcase_add_test (tc_chain, test_parse_sei_userdefinedunregistered);
    nf += gst_check_run_suite (s, "h264parse", __FILE__);
  }

  return nf;
}
