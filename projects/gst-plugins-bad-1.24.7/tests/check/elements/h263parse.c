/*
 * GStreamer
 *
 * unit test for h263parse
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

#define SRC_CAPS_TMPL   "video/x-h263, variant=(string)itu, parsed=(boolean)false"
#define SINK_CAPS_TMPL  "video/x-h263, parsed=(boolean)true"

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

#if 0
static guint8 h263_iframe[] = {
  /* keyframes all around */
  0x00, 0x00, 0x80, 0x02, 0x1c, 0x88, 0x01, 0x00,
  0x11, 0xe0, 0x44, 0xc4, 0x04, 0x04, 0x04, 0x3f,
  0xff, 0xe6, 0x20, 0x20, 0x20, 0x21, 0xff, 0xff,
  0x31, 0x01, 0x01, 0x01, 0x0f, 0xff, 0xf9, 0x88,
  0x08, 0x08, 0x08, 0x7f, 0xff, 0x80
};
#endif

static guint8 h263_iframe[] = {
  /* keyframes all around */
  /* actually, this is a truncated keyframe,
   * but don't tell anyone or try this at home */
  0x00, 0x00, 0x80, 0x02, 0x0c, 0x04, 0x26, 0x20,
  0x20, 0x20, 0x21, 0xff, 0xff, 0x31, 0x01, 0x01,
  0x01, 0x0f, 0xff, 0xf9, 0x88, 0x08, 0x08, 0x08,
  0x7f, 0xff, 0xcc, 0x40, 0x40, 0x40, 0x43, 0xff,
  0xfe, 0x62, 0x02, 0x02, 0x02, 0x1f, 0xff, 0xf3,
  0x10, 0x10, 0x10, 0x10, 0xff, 0xff, 0x98, 0x80,
  0x80, 0x80, 0x87, 0xff, 0xfc, 0xc4, 0x04, 0x04,
  0x04, 0x3f, 0xff, 0xe6, 0x20, 0x20, 0x20, 0x21,
  0xff, 0xff, 0x31, 0x01, 0x01, 0x01, 0x0f, 0xff,
  0xf9, 0x88, 0x08, 0x08, 0x08, 0x7f, 0xff, 0xcc,
  0x40, 0x40, 0x40, 0x43, 0xff, 0xfe, 0x62, 0x02,
  0x02, 0x02, 0x1f, 0xff, 0xf3, 0x10, 0x10, 0x10,
  0x10, 0xff, 0xff, 0x98, 0x80, 0x80, 0x80, 0x87,
  0xff, 0xfc, 0xc4, 0x04, 0x04, 0x04, 0x3f, 0xff,
  0xe6, 0x20, 0x20, 0x20, 0x21, 0xff, 0xff, 0x31,
  0x01, 0x01, 0x01, 0x0f, 0xff, 0xf9, 0x88, 0x08
};

GST_START_TEST (test_parse_normal)
{
  gst_parser_test_normal (h263_iframe, sizeof (h263_iframe));
}

GST_END_TEST;


GST_START_TEST (test_parse_drain_single)
{
  gst_parser_test_drain_single (h263_iframe, sizeof (h263_iframe));
}

GST_END_TEST;


GST_START_TEST (test_parse_split)
{
  gst_parser_test_split (h263_iframe, sizeof (h263_iframe));
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

  caps = gst_parser_test_get_output_caps (h263_iframe, sizeof (h263_iframe),
      NULL);
  fail_unless (caps != NULL);

  /* Check that the negotiated caps are as expected */
  /* When codec_data is present, parser assumes that data is version 4 */
  GST_LOG ("mpegvideo output caps: %" GST_PTR_FORMAT, caps);
  s = gst_caps_get_structure (caps, 0);
  fail_unless (gst_structure_has_name (s, "video/x-h263"));
  fail_unless_structure_field_int_equals (s, "width", 352);
  fail_unless_structure_field_int_equals (s, "height", 288);

  gst_caps_unref (caps);
}

GST_END_TEST;


static Suite *
h263parse_suite (void)
{
  Suite *s = suite_create ("h263parse");
  TCase *tc_chain = tcase_create ("general");

  /* init test context */
  ctx_factory = "h263parse";
  ctx_sink_template = &sinktemplate;
  ctx_src_template = &srctemplate;
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
GST_CHECK_MAIN (h263parse);
