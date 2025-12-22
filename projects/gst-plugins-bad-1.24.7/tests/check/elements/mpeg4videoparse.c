/*
 * GStreamer
 *
 * unit test for mpeg4videoparse
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

#include <gst/check/gstcheck.h>
#include "parser.h"

#define SRC_CAPS_TMPL   "video/mpeg, mpegversion=(int)4, systemstream=(boolean)false, parsed=(boolean)false"
#define SINK_CAPS_TMPL  "video/mpeg, mpegversion=(int)4, systemstream=(boolean)false, parsed=(boolean)true"

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

/* codec data; VOS up to and including GOP */
static guint8 mpeg4_config[] = {
  0x00, 0x00, 0x01, 0xb0, 0x01, 0x00, 0x00, 0x01,
  0xb5, 0x89, 0x13, 0x00, 0x00, 0x01, 0x00, 0x00,
  0x00, 0x01, 0x20, 0x00, 0xc4, 0x8d, 0x88, 0x00,
  0xf5, 0x01, 0x04, 0x03, 0x14, 0x63, 0x00, 0x00,
  0x01, 0xb3, 0x00, 0x10, 0x07
};

/* keyframes all around */
static guint8 mpeg4_iframe[] = {
  0x00, 0x00, 0x01, 0xb6, 0x10, 0x60, 0x91, 0x82,
  0x3d, 0xb7, 0xf1, 0xb6, 0xdf, 0xc6, 0xdb, 0x7f,
  0x1b, 0x6d, 0xfb
};

static gboolean
verify_buffer (buffer_verify_data_s * vdata, GstBuffer * buffer)
{
  GstMapInfo map;

  gst_buffer_map (buffer, &map, GST_MAP_READ);

  /* header is merged in initial frame */
  if (vdata->buffer_counter == 0) {
    /* the whole sequence header is included */
    fail_unless (map.size == ctx_headers[0].size + vdata->data_to_verify_size);
    fail_unless (memcmp (map.data, ctx_headers[0].data,
            ctx_headers[0].size) == 0);
    fail_unless (memcmp (map.data + ctx_headers[0].size,
            vdata->data_to_verify, vdata->data_to_verify_size) == 0);
    gst_buffer_unmap (buffer, &map);
    return TRUE;
  }
  gst_buffer_unmap (buffer, &map);

  return FALSE;
}

GST_START_TEST (test_parse_normal)
{
  gst_parser_test_normal (mpeg4_iframe, sizeof (mpeg4_iframe));
}

GST_END_TEST;


GST_START_TEST (test_parse_drain_single)
{
  gst_parser_test_drain_single (mpeg4_iframe, sizeof (mpeg4_iframe));
}

GST_END_TEST;


GST_START_TEST (test_parse_split)
{
  gst_parser_test_split (mpeg4_iframe, sizeof (mpeg4_iframe));
}

GST_END_TEST;


#define structure_get_int(s,f) \
    (g_value_get_int(gst_structure_get_value(s,f)))
#define fail_unless_structure_field_int_equals(s,field,num) \
    fail_unless_equals_int (structure_get_int(s,field), num)

GST_START_TEST (test_parse_detect_stream)
{
  GstCaps *caps;
  GstStructure *s;
  GstBuffer *buf;
  const GValue *val;
  GstMapInfo map;

  caps = gst_parser_test_get_output_caps (mpeg4_iframe, sizeof (mpeg4_iframe),
      NULL);
  fail_unless (caps != NULL);

  /* Check that the negotiated caps are as expected */
  /* When codec_data is present, parser assumes that data is version 4 */
  GST_LOG ("mpeg4video output caps: %" GST_PTR_FORMAT, caps);
  s = gst_caps_get_structure (caps, 0);
  fail_unless (gst_structure_has_name (s, "video/mpeg"));
  fail_unless_structure_field_int_equals (s, "mpegversion", 4);
  fail_unless_structure_field_int_equals (s, "width", 32);
  fail_unless_structure_field_int_equals (s, "height", 24);
  fail_unless (gst_structure_has_field (s, "codec_data"));

  /* check codec-data in more detail */
  val = gst_structure_get_value (s, "codec_data");
  fail_unless (val != NULL);
  buf = gst_value_get_buffer (val);
  fail_unless (buf != NULL);
  /* codec-data == config header - GOP */
  gst_buffer_map (buf, &map, GST_MAP_READ);
  fail_unless (map.size == sizeof (mpeg4_config) - 7);
  fail_unless (memcmp (map.data, mpeg4_config, map.size) == 0);
  gst_buffer_unmap (buf, &map);

  gst_caps_unref (caps);
}

GST_END_TEST;


static Suite *
mpeg4videoparse_suite (void)
{
  Suite *s = suite_create ("mpeg4videoparse");
  TCase *tc_chain = tcase_create ("general");

  /* init test context */
  ctx_factory = "mpeg4videoparse";
  ctx_sink_template = &sinktemplate;
  ctx_src_template = &srctemplate;
  ctx_headers[0].data = mpeg4_config;
  ctx_headers[0].size = sizeof (mpeg4_config);
  ctx_verify_buffer = verify_buffer;
  /* no timing info to parse */
  ctx_no_metadata = TRUE;

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_parse_normal);
  tcase_add_test (tc_chain, test_parse_drain_single);
  tcase_add_test (tc_chain, test_parse_split);
  tcase_add_test (tc_chain, test_parse_detect_stream);

  return s;
}


/*
 * TODO:
 *   - Both push- and pull-modes need to be tested
 *      * Pull-mode & EOS
 */
GST_CHECK_MAIN (mpeg4videoparse);
