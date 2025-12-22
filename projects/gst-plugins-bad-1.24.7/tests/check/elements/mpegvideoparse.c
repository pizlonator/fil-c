/*
 * GStreamer
 *
 * unit test for mpegvideoparse
 *
 * Copyright (C) 2011 Nokia Corporation. All rights reserved.
 *   Contact: Stefan Kost <stefan.kost@nokia.com>
 * Copyright (C) 2018 Tim-Philipp Müller <tim centricular com>
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

#include <gst/check/check.h>
#include <gst/video/video.h>
#include "parser.h"

#define SRC_CAPS_TMPL   "video/mpeg, mpegversion=(int)2, systemstream=(boolean)false, parsed=(boolean)false"
#define SINK_CAPS_TMPL  "video/mpeg, mpegversion=(int){1, 2}, systemstream=(boolean)false, parsed=(boolean)true"

GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (SINK_CAPS_TMPL)
    );

GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (SRC_CAPS_TMPL)
    );

/* some data */

/* actually seq + gop */
static guint8 mpeg2_seq[] = {
  0x00, 0x00, 0x01, 0xb3, 0x02, 0x00, 0x18, 0x15,
  0xff, 0xff, 0xe0, 0x28, 0x00, 0x00, 0x01, 0xb5,
  0x14, 0x8a, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
  0x01, 0xb8, 0x00, 0x08, 0x00, 0x00
};

/* actually seq + gop */
static guint8 mpeg1_seq[] = {
  0x00, 0x00, 0x01, 0xb3, 0x02, 0x00, 0x18, 0x15,
  0xff, 0xff, 0xe0, 0x28, 0x00, 0x00, 0x01, 0xb8,
  0x00, 0x08, 00, 00
};

/* keyframes all around */
static guint8 mpeg2_iframe[] = {
  0x00, 0x00, 0x01, 0x00, 0x00, 0x0f, 0xff, 0xf8,
  0x00, 0x00, 0x01, 0xb5, 0x8f, 0xff, 0xf3, 0x41,
  0x80, 0x00, 0x00, 0x01, 0x01, 0x23, 0xf8, 0x7d,
  0x29, 0x48, 0x8b, 0x94, 0xa5, 0x22, 0x20, 0x00,
  0x00, 0x01, 0x02, 0x23, 0xf8, 0x7d, 0x29, 0x48,
  0x8b, 0x94, 0xa5, 0x22, 0x20
};

static guint8 mpeg1_iframe[] = {
  0x00, 0x00, 0x01, 0x00, 0x00, 0x0f, 0xff, 0xf8,
  0x00, 0x00, 0x01, 0x01, 0x23, 0xf8, 0x7d,
  0x29, 0x48, 0x8b, 0x94, 0xa5, 0x22, 0x20, 0x00,
  0x00, 0x01, 0x02, 0x23, 0xf8, 0x7d, 0x29, 0x48,
  0x8b, 0x94, 0xa5, 0x22, 0x20
};

static gboolean
verify_buffer (buffer_verify_data_s * vdata, GstBuffer * buffer)
{
  GstMapInfo map;

  gst_buffer_map (buffer, &map, GST_MAP_READ);

  /* check initial header special case, otherwise delegate to default */
  if (vdata->discard) {
    /* header is separate */
    fail_unless (map.size == ctx_headers[0].size - 8);
    fail_unless (memcmp (map.data, ctx_headers[0].data, map.size) == 0);
  } else {
    /* header is merged in initial frame */
    if (vdata->buffer_counter == 0) {
      fail_unless (map.size > 4);
      if (GST_READ_UINT32_BE (map.data) == 0x1b3) {
        /* the whole sequence header is included */
        fail_unless (map.size ==
            ctx_headers[0].size + vdata->data_to_verify_size);
        fail_unless (memcmp (map.data, ctx_headers[0].data,
                ctx_headers[0].size) == 0);
        fail_unless (memcmp (map.data + ctx_headers[0].size,
                vdata->data_to_verify, vdata->data_to_verify_size) == 0);
      } else {
        /* sequence was separate, only gop here */
        fail_unless (map.size == 8 + vdata->data_to_verify_size);
        fail_unless (memcmp (map.data,
                ctx_headers[0].data + ctx_headers[0].size - 8, 8) == 0);
        fail_unless (memcmp (map.data + 8,
                vdata->data_to_verify, vdata->data_to_verify_size) == 0);
      }
      gst_buffer_unmap (buffer, &map);
      return TRUE;
    }
  }
  gst_buffer_unmap (buffer, &map);

  return FALSE;
}

#define  GOP_SPLIT           "gop-split"

static GstElement *
setup_element (const gchar * desc)
{
  GstElement *element;

  if (strcmp (desc, GOP_SPLIT) == 0) {
    element = gst_check_setup_element ("mpegvideoparse");
    g_object_set (G_OBJECT (element), "gop-split", TRUE, NULL);
  } else {
    element = gst_check_setup_element ("mpegvideoparse");
  }

  return element;
}

GST_START_TEST (test_parse_normal)
{
  gst_parser_test_normal (mpeg2_iframe, sizeof (mpeg2_iframe));
}

GST_END_TEST;


GST_START_TEST (test_parse_drain_single)
{
  gst_parser_test_drain_single (mpeg2_iframe, sizeof (mpeg2_iframe));
}

GST_END_TEST;


GST_START_TEST (test_parse_split)
{
  gst_parser_test_split (mpeg2_iframe, sizeof (mpeg2_iframe));
}

GST_END_TEST;


#define structure_get_int(s,f) \
    (g_value_get_int(gst_structure_get_value(s,f)))
#define fail_unless_structure_field_int_equals(s,field,num) \
    fail_unless_equals_int (structure_get_int(s,field), num)

static void
mpeg_video_parse_check_caps (guint version, guint8 * seq, gint size)
{
  GstCaps *caps;
  GstStructure *s;
  GstBuffer *buf;
  const GValue *val;
  GstMapInfo map;

  ctx_headers[0].data = seq;
  ctx_headers[0].size = size;
  if (version == 1)
    caps = gst_parser_test_get_output_caps (mpeg1_iframe, sizeof (mpeg1_iframe),
        NULL);
  else
    caps = gst_parser_test_get_output_caps (mpeg2_iframe, sizeof (mpeg2_iframe),
        NULL);
  fail_unless (caps != NULL);

  /* Check that the negotiated caps are as expected */
  /* When codec_data is present, parser assumes that data is version 4 */
  GST_LOG ("mpegvideo output caps: %" GST_PTR_FORMAT, caps);
  s = gst_caps_get_structure (caps, 0);
  fail_unless (gst_structure_has_name (s, "video/mpeg"));
  fail_unless_structure_field_int_equals (s, "mpegversion", version);
  fail_unless_structure_field_int_equals (s, "width", 32);
  fail_unless_structure_field_int_equals (s, "height", 24);
  fail_unless (gst_structure_has_field (s, "codec_data"));

  /* check codec-data in more detail */
  val = gst_structure_get_value (s, "codec_data");
  fail_unless (val != NULL);
  buf = gst_value_get_buffer (val);
  fail_unless (buf != NULL);
  gst_buffer_map (buf, &map, GST_MAP_READ);
  /* codec-data = header - GOP */
  assert_equals_int (map.size, size - 8);
  fail_unless (memcmp (map.data, seq, map.size) == 0);
  gst_buffer_unmap (buf, &map);

  gst_caps_unref (caps);
}

GST_START_TEST (test_parse_detect_stream_mpeg2)
{
  mpeg_video_parse_check_caps (2, mpeg2_seq, sizeof (mpeg2_seq));
}

GST_END_TEST;


GST_START_TEST (test_parse_detect_stream_mpeg1)
{
  mpeg_video_parse_check_caps (1, mpeg1_seq, sizeof (mpeg1_seq));
}

GST_END_TEST;


GST_START_TEST (test_parse_gop_split)
{
  ctx_factory = GOP_SPLIT;
  ctx_discard = 1;
  gst_parser_test_normal (mpeg2_iframe, sizeof (mpeg2_iframe));
  ctx_factory = "mpegvideoparse";
  ctx_discard = 0;
}

GST_END_TEST;

GST_START_TEST (test_parse_cea708_captions)
{
  GstClockTime last_ts = 0;
  GstHarness *h;
  GstBuffer *buf;
  gchar *fn;
  gint i, j;

  h = gst_harness_new_parse ("filesrc name=filesrc ! mpegvideoparse");

  /* Minimal stripped down MPEG-2 video elementary stream with truncated slices.
   * Created via mpegvideoparse ! multifilesink and then truncating all files
   * to 80-100 bytes or such and concatenating them back together. */
  fn = g_build_filename (GST_TEST_FILES_PATH, "mpeg2-es-with-cea708-cc.dat",
      NULL);
  gst_harness_set (h, "filesrc", "location", fn, NULL);
  g_free (fn);

  gst_harness_play (h);

  for (i = 0; i < 50; ++i) {
    GstVideoCaptionMeta *caption_meta;

    buf = gst_harness_pull (h);
    GST_LOG ("pulled buffer %" GST_PTR_FORMAT, buf);
    caption_meta = gst_buffer_get_video_caption_meta (buf);
    fail_unless (caption_meta != NULL);
    fail_unless_equals_int (caption_meta->caption_type,
        GST_VIDEO_CAPTION_TYPE_CEA708_RAW);
    fail_unless_equals_int (caption_meta->size, 60);

    /* iterate over triplets */
    for (j = 0; j < 20; ++j) {
      guint8 cc_type = caption_meta->data[3 * j] & 0x03;

      /* first triplet always CEA-608 line 21 field 2 in our sample */
      if (j == 0)
        fail_unless_equals_int (cc_type, 1);
      /* second triplet always CEA-608 line 21 field 1 in our sample */
      if (j == 1)
        fail_unless_equals_int (cc_type, 0);
      if (j > 2)
        fail_unless (cc_type == 2 || cc_type == 3);
      /* first packet starts with a CCP header */
      if (i == 0 && j == 2)
        fail_unless (cc_type == 3);
    }
    /* buffer sanity check */
    if (i > 0)
      fail_unless (GST_BUFFER_DTS (buf) > last_ts);

    last_ts = GST_BUFFER_DTS (buf);
    gst_buffer_unref (buf);
  }

  gst_harness_teardown (h);
}

GST_END_TEST;

static Suite *
mpegvideoparse_suite (void)
{
  Suite *s = suite_create ("mpegvideoparse");
  TCase *tc_chain = tcase_create ("general");

  /* init test context */
  ctx_factory = "mpegvideoparse";
  ctx_sink_template = &sinktemplate;
  ctx_src_template = &srctemplate;
  ctx_headers[0].data = mpeg2_seq;
  ctx_headers[0].size = sizeof (mpeg2_seq);
  ctx_verify_buffer = verify_buffer;
  ctx_setup = setup_element;


  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_parse_normal);
  tcase_add_test (tc_chain, test_parse_drain_single);
  tcase_add_test (tc_chain, test_parse_split);
  tcase_add_test (tc_chain, test_parse_detect_stream_mpeg1);
  tcase_add_test (tc_chain, test_parse_detect_stream_mpeg2);
  tcase_add_test (tc_chain, test_parse_gop_split);
  tcase_add_test (tc_chain, test_parse_cea708_captions);

  return s;
}


/*
 * TODO:
 *   - Both push- and pull-modes need to be tested
 *      * Pull-mode & EOS
 */
GST_CHECK_MAIN (mpegvideoparse);
