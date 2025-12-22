/*
 * GStreamer
 *
 * unit test for h265parse
 *
 * Copyright (C) 2019 Stéphane Cerveau <scerveau@collabora.com>
 * Copyright (C) 2019 Collabora Ltd.
 *   @author George Kiagiadakis <george.kiagiadakis@collabora.com>
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
#include <gst/video/video-sei.h>
#include "parser.h"

#define SRC_CAPS_TMPL   "video/x-h265, parsed=(boolean)false"
#define SINK_CAPS_TMPL  "video/x-h265, parsed=(boolean)true"

GstStaticPadTemplate sinktemplate_bs_au = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (SINK_CAPS_TMPL
        ", stream-format = (string) byte-stream, alignment = (string) au")
    );

GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (SRC_CAPS_TMPL)
    );

/* Data generated with:
 *
 * gst-launch-1.0 videotestsrc num-buffers=1 ! video/x-raw,width=16,height=16 ! x265enc option-string="max-cll=1000,400:master-display=G(13250,34500)B(7500,3000)R(34000,16000)WP(15635,16450)L(10000000,1)" ! h265parse ! fakesink
 *
 * x265enc SEI has been dropped.
 *
 */

static const guint8 h265_vps[] = {
  0x00, 0x00, 0x00, 0x01, 0x40, 0x01, 0x0c, 0x01, 0xff, 0xff, 0x01, 0x60, 0x00,
  0x00, 0x03, 0x00, 0x90, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x3f, 0x95,
  0x98, 0x09
};

static const guint8 h265_sps[] = {
  0x00, 0x00, 0x00, 0x01, 0x42, 0x01, 0x01, 0x01, 0x60, 0x00, 0x00, 0x03, 0x00,
  0x90, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x3f, 0xa0, 0x88, 0x45, 0x96,
  0x56, 0x6a, 0xbc, 0xaf, 0xff, 0x00, 0x01, 0x00, 0x01, 0x6a, 0x0c, 0x02, 0x0c,
  0x08, 0x00, 0x00, 0x03, 0x00, 0x08, 0x00, 0x00, 0x03, 0x00, 0xf0, 0x40
};

static const guint8 h265_pps[] = {
  0x00, 0x00, 0x00, 0x01, 0x44, 0x01, 0xc1, 0x73, 0xd0, 0x89
};

static const guint8 h265_idr[] = {
  0x00, 0x00, 0x00, 0x01, 0x26, 0x01, 0xaf, 0x06, 0xb8, 0xcf, 0xbc, 0x65, 0x85,
  0x3b, 0x49, 0xff, 0xd0, 0x2c, 0xff, 0x3b, 0x61, 0x6d, 0x1b, 0xae, 0xf1, 0xf4,
  0x96, 0x15, 0xef, 0x3e, 0xc6, 0x67, 0x3c, 0x0a, 0xd0, 0x6a, 0xb9, 0xfb, 0xf8,
  0xb4, 0xb8, 0x4a, 0x4c, 0x4e, 0xe2, 0xf6, 0xb0, 0x29, 0x41, 0x4e, 0x14, 0xe8,
  0x1f, 0x41, 0x58, 0xcb, 0x7a, 0x94, 0xdc, 0xba, 0x3d, 0x2e, 0xe0, 0x83, 0x4d,
  0x3c, 0x3d, 0x2d, 0x70, 0xd1, 0xc4, 0x3d, 0x65, 0xf8, 0x3a, 0xe3, 0xdf, 0xb1,
  0xf1, 0x1c, 0x48, 0x45, 0x63, 0x5b, 0x55, 0x0e, 0x0d, 0xef, 0xfc, 0x07, 0xd3,
  0xce, 0x14, 0xc2, 0xac, 0x79, 0xd6, 0x1c, 0x44, 0x2c, 0xbd, 0x00, 0xff, 0xe5,
  0x0c, 0x09, 0x3a, 0x3b, 0x53, 0xa8, 0x58, 0xb5, 0xb0, 0x29, 0xe6, 0x64, 0x14,
  0x3a, 0xec, 0x8c, 0x7d, 0xd9, 0x19, 0xb4, 0xc2, 0x75, 0x37, 0xa2, 0x64, 0xa3,
  0x1f, 0x26, 0x78, 0xe0, 0xa4, 0xde, 0xed, 0xb1, 0x52, 0x67, 0x90, 0xf1, 0x8e,
  0xf9, 0x99, 0xa8, 0x9e, 0xfa, 0x55, 0xfc, 0x92, 0x3d, 0xd1, 0x03, 0xff, 0xff,
  0xf7, 0x79, 0xaf, 0xa5, 0x90, 0x72, 0x35, 0x4e, 0x64, 0x16, 0x48, 0xa8, 0x28,
  0xc4, 0xcf, 0x51, 0x83, 0x78, 0x6d, 0x90, 0x3a, 0xdf, 0xff, 0xb1, 0x1b, 0xb4,
  0x3e, 0xa5, 0xd3, 0xc9, 0x2b, 0x75, 0x16, 0x01, 0x16, 0xa6, 0xc5, 0x1d, 0x1e,
  0xd6, 0x63, 0x0c, 0xba, 0x2f, 0x77, 0x58, 0x5a, 0x4c, 0xb6, 0x49, 0x63, 0xb4,
  0xa5, 0xb3, 0x25, 0x1b, 0xfd, 0xea, 0x13, 0x8b, 0xb3, 0x8f, 0x42, 0x81, 0xa1,
  0x89, 0xe1, 0x36, 0x80, 0x11, 0x3c, 0x88, 0x84, 0x29, 0x51, 0x59, 0x2c, 0xb2,
  0x9c, 0x90, 0xa5, 0x12, 0x80, 0x2d, 0x16, 0x61, 0x8e, 0xf1, 0x28, 0xba, 0x0f,
  0x71, 0xdf, 0x7b, 0xdb, 0xd7, 0xb0, 0x3d, 0xa1, 0xbe, 0x4f, 0x7c, 0xcf, 0x09,
  0x73, 0xe1, 0x10, 0xea, 0x64, 0x96, 0x89, 0x5d, 0x7e, 0x7f, 0x26, 0x18, 0x43,
  0xbb, 0x0d, 0x2c, 0x95, 0xaa, 0xec, 0x03, 0x9d, 0x55, 0x56, 0xdf, 0xd3, 0x7e,
  0x4f, 0xf7, 0x47, 0x60, 0x89, 0x35, 0x6e, 0x08, 0x9a, 0xcf, 0x11, 0x26, 0xc3,
  0xec, 0x31, 0x23, 0xca, 0x51, 0x10, 0x80
};

/* Content light level information SEI message */
static const guint8 h265_sei_clli[] = {
  0x00, 0x00, 0x00, 0x01, 0x4e, 0x01, 0x90, 0x04, 0x03, 0xe8, 0x01, 0x90, 0x80
};

/* Mastering display colour volume information SEI message */
static const guint8 h265_sei_mdcv[] = {
  0x00, 0x00, 0x00, 0x01, 0x4e, 0x01, 0x89, 0x18, 0x33, 0xc2, 0x86, 0xc4, 0x1d,
  0x4c, 0x0b, 0xb8, 0x84, 0xd0, 0x3e, 0x80, 0x3d, 0x13, 0x40, 0x42, 0x00, 0x98,
  0x96, 0x80, 0x00, 0x00, 0x03, 0x00, 0x01, 0x80
};


/* single-sliced data, generated with:
 * gst-launch-1.0 videotestsrc num-buffers=1 pattern=green \
 *    ! video/x-raw,width=128,height=128 \
 *    ! x265enc
 *    ! fakesink dump=1
 */

static const guint8 h265_128x128_vps[] = {
  0x00, 0x00, 0x00, 0x01, 0x40, 0x01, 0x0c, 0x01,
  0xff, 0xff, 0x01, 0x60, 0x00, 0x00, 0x03, 0x00,
  0x90, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00,
  0x3f, 0x95, 0x98, 0x09
};

static const guint8 h265_128x128_sps[] = {
  0x00, 0x00, 0x00, 0x01, 0x42, 0x01, 0x01, 0x01,
  0x60, 0x00, 0x00, 0x03, 0x00, 0x90, 0x00, 0x00,
  0x03, 0x00, 0x00, 0x03, 0x00, 0x3f, 0xa0, 0x10,
  0x20, 0x20, 0x59, 0x65, 0x66, 0x92, 0x4c, 0xaf,
  0xff, 0x00, 0x01, 0x00, 0x01, 0x01, 0x00, 0x00,
  0x03, 0x00, 0x01, 0x00, 0x00, 0x03, 0x00, 0x1e,
  0x08
};

static const guint8 h265_128x128_pps[] = {
  0x00, 0x00, 0x00, 0x01, 0x44, 0x01, 0xc1, 0x72,
  0xb4, 0x22, 0x40
};

static const guint8 h265_128x128_slice_idr_n_lp[] = {
  0x00, 0x00, 0x00, 0x01, 0x28, 0x01, 0xaf, 0x0e,
  0xe0, 0x34, 0x82, 0x15, 0x84, 0xf4, 0x70, 0x4f,
  0xff, 0xed, 0x41, 0x3f, 0xff, 0xe4, 0xcd, 0xc4,
  0x7c, 0x03, 0x0c, 0xc2, 0xbb, 0xb0, 0x74, 0xe5,
  0xef, 0x4f, 0xe1, 0xa3, 0xd4, 0x00, 0x02, 0xc2
};

/* multi-sliced data, generated on zynqultrascaleplus with:
 * gst-launch-1.0 videotestsrc num-buffers=1 pattern=green \
 *    ! video/x-raw,width=128,height=128 \
 *    ! omxh265enc num-slices=2 \
 *    ! fakesink dump=1
 */

static const guint8 h265_128x128_sliced_vps[] = {
  0x00, 0x00, 0x00, 0x01, 0x40, 0x01, 0x0c, 0x01,
  0xff, 0xff, 0x01, 0x40, 0x00, 0x00, 0x03, 0x00,
  0x90, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00,
  0x1e, 0x25, 0x02, 0x40
};

static const guint8 h265_128x128_sliced_sps[] = {
  0x00, 0x00, 0x00, 0x01, 0x42, 0x01, 0x01, 0x01,
  0x40, 0x00, 0x00, 0x03, 0x00, 0x90, 0x00, 0x00,
  0x03, 0x00, 0x00, 0x03, 0x00, 0x1e, 0xa0, 0x10,
  0x20, 0x20, 0x59, 0xe9, 0x6e, 0x44, 0xa1, 0x73,
  0x50, 0x60, 0x20, 0x2e, 0x10, 0x00, 0x00, 0x03,
  0x00, 0x10, 0x00, 0x00, 0x03, 0x01, 0xe5, 0x1a,
  0xff, 0xff, 0x10, 0x3e, 0x80, 0x5d, 0xf7, 0xc2,
  0x01, 0x04
};

static const guint8 h265_128x128_sliced_pps[] = {
  0x00, 0x00, 0x00, 0x01, 0x44, 0x01, 0xc0, 0x71,
  0x81, 0x8d, 0xb2
};

static const guint8 h265_128x128_slice_1_idr_n_lp[] = {
  0x00, 0x00, 0x00, 0x01, 0x28, 0x01, 0xac, 0x46,
  0x13, 0xb6, 0x45, 0x43, 0xaf, 0xee, 0x3d, 0x3f,
  0x76, 0xe5, 0x73, 0x2f, 0xee, 0xd2, 0xeb, 0xbf,
  0x80
};

static const guint8 h265_128x128_slice_2_idr_n_lp[] = {
  0x00, 0x00, 0x00, 0x01, 0x28, 0x01, 0x30, 0xc4,
  0x60, 0x13, 0xb6, 0x45, 0x43, 0xaf, 0xee, 0x3d,
  0x3f, 0x76, 0xe5, 0x73, 0x2f, 0xee, 0xd2, 0xeb,
  0xbf, 0x80
};

static const gchar *ctx_suite;
static gboolean ctx_codec_data;

/* A single access unit comprising of VPS, SPS, PPS and IDR frame */
static gboolean
verify_buffer_bs_au (buffer_verify_data_s * vdata, GstBuffer * buffer)
{
  GstMapInfo map;

  fail_unless (ctx_sink_template == &sinktemplate_bs_au);

  gst_buffer_map (buffer, &map, GST_MAP_READ);
  fail_unless (map.size > 4);

  if (vdata->buffer_counter == 0) {
    guint8 *data = map.data;

    /* VPS, SPS, PPS */
    fail_unless (map.size == vdata->data_to_verify_size +
        ctx_headers[0].size + ctx_headers[1].size + ctx_headers[2].size);

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
    fail_unless (map.size == vdata->data_to_verify_size);

    fail_unless (memcmp (map.data, vdata->data_to_verify, map.size) == 0);
  }

  gst_buffer_unmap (buffer, &map);
  return TRUE;
}

GST_START_TEST (test_parse_normal)
{
  gst_parser_test_normal (h265_idr, sizeof (h265_idr));
}

GST_END_TEST;


GST_START_TEST (test_parse_drain_single)
{
  gst_parser_test_drain_single (h265_idr, sizeof (h265_idr));
}

GST_END_TEST;


GST_START_TEST (test_parse_split)
{
  gst_parser_test_split (h265_idr, sizeof (h265_idr));
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

  caps = gst_parser_test_get_output_caps (h265_idr, sizeof (h265_idr), NULL);
  fail_unless (caps != NULL);

  /* Check that the negotiated caps are as expected */
  GST_DEBUG ("output caps: %" GST_PTR_FORMAT, caps);
  s = gst_caps_get_structure (caps, 0);
  fail_unless (gst_structure_has_name (s, "video/x-h265"));
  fail_unless_structure_field_int_equals (s, "width", 16);
  fail_unless_structure_field_int_equals (s, "height", 16);
  fail_unless_structure_field_string_equals (s, "stream-format", "byte-stream");
  fail_unless_structure_field_string_equals (s, "alignment", "au");
  fail_unless_structure_field_string_equals (s, "profile", "main");
  fail_unless_structure_field_string_equals (s, "tier", "main");
  fail_unless_structure_field_string_equals (s, "level", "2.1");

  gst_caps_unref (caps);
}

GST_END_TEST;

GST_START_TEST (test_parse_detect_stream_with_hdr_sei)
{
  GstCaps *caps;
  GstStructure *s;
  guint8 *h265_idr_plus_sei;
  gsize h265_idr_plus_sei_size =
      sizeof (h265_sei_clli) + sizeof (h265_sei_mdcv) + sizeof (h265_idr);

  h265_idr_plus_sei = malloc (h265_idr_plus_sei_size);

  memcpy (h265_idr_plus_sei, h265_sei_clli, sizeof (h265_sei_clli));
  memcpy (h265_idr_plus_sei + sizeof (h265_sei_clli), h265_sei_mdcv,
      sizeof (h265_sei_mdcv));
  memcpy (h265_idr_plus_sei + sizeof (h265_sei_clli) + sizeof (h265_sei_mdcv),
      h265_idr, sizeof (h265_idr));

  caps =
      gst_parser_test_get_output_caps (h265_idr_plus_sei,
      h265_idr_plus_sei_size, NULL);
  fail_unless (caps != NULL);

  /* Check that the negotiated caps are as expected */
  GST_DEBUG ("output caps: %" GST_PTR_FORMAT, caps);
  s = gst_caps_get_structure (caps, 0);
  fail_unless (gst_structure_has_name (s, "video/x-h265"));
  fail_unless_structure_field_int_equals (s, "width", 16);
  fail_unless_structure_field_int_equals (s, "height", 16);
  fail_unless_structure_field_string_equals (s, "stream-format", "byte-stream");
  fail_unless_structure_field_string_equals (s, "alignment", "au");
  fail_unless_structure_field_string_equals (s, "profile", "main");
  fail_unless_structure_field_string_equals (s, "tier", "main");
  fail_unless_structure_field_string_equals (s, "level", "2.1");
  fail_unless_structure_field_string_equals (s, "mastering-display-info",
      "34000:16000:13250:34500:7500:3000:15635:16450:10000000:1");
  fail_unless_structure_field_string_equals (s, "content-light-level",
      "1000:400");

  g_free (h265_idr_plus_sei);
  gst_caps_unref (caps);
}

GST_END_TEST;

/* 8bits 4:4:4 encoded stream, and profile-level-tier is not spec compliant.
 * extracted from the file reported at
 * https://gitlab.freedesktop.org/gstreamer/gstreamer/-/issues/1009
 */
static const guint8 broken_profile_codec_data[] = {
  0x01, 0x24, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x99, 0xf0, 0x00, 0xfc, 0xff, 0xf8, 0xf8, 0x00, 0x00, 0x0f, 0x03, 0x20,
  0x00, 0x01, 0x00, 0x18, 0x40, 0x01, 0x0c, 0x01, 0xff, 0xff, 0x24, 0x08,
  0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03,
  0x00, 0x99, 0xac, 0x09, 0x21, 0x00, 0x01, 0x00, 0x2c, 0x42, 0x01, 0x01,
  0x24, 0x08, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00,
  0x00, 0x03, 0x00, 0x99, 0x90, 0x00, 0x3c, 0x04, 0x00, 0x44, 0x0f, 0x84,
  0x72, 0xd6, 0x94, 0x84, 0xb2, 0x5c, 0x40, 0x20, 0x00, 0x00, 0x03, 0x00,
  0x20, 0x00, 0x00, 0x07, 0x81, 0x22, 0x00, 0x01, 0x00, 0x08, 0x44, 0x01,
  0xc0, 0xf7, 0x18, 0x30, 0x0c, 0xc9
};

GST_START_TEST (test_parse_fallback_profile)
{
  GstHarness *h = gst_harness_new ("h265parse");
  GstCaps *caps;
  GstBuffer *codec_data;
  GstEvent *event;

  codec_data = gst_buffer_new_memdup (broken_profile_codec_data,
      sizeof (broken_profile_codec_data));

  caps = gst_caps_from_string ("video/x-h265, stream-format=(string)hvc1, "
      "alignment=(string)au");
  gst_caps_set_simple (caps, "codec_data", GST_TYPE_BUFFER, codec_data, NULL);
  gst_buffer_unref (codec_data);

  gst_harness_set_src_caps (h, caps);
  while ((event = gst_harness_pull_event (h)) != NULL) {
    GstStructure *s;
    const gchar *profile;

    if (GST_EVENT_TYPE (event) != GST_EVENT_CAPS) {
      gst_event_unref (event);
      continue;
    }

    gst_event_parse_caps (event, &caps);
    s = gst_caps_get_structure (caps, 0);
    profile = gst_structure_get_string (s, "profile");

    /* h265parse must provide profile */
    fail_unless (profile);

    /* must not be main profile at least.
     * main-444 is expected but we might update the profile parsing
     * logic later. At least it should not be main profile
     */
    fail_if (g_strcmp0 (profile, "main") == 0);

    gst_event_unref (event);
    break;
  }

  gst_harness_teardown (h);
}

GST_END_TEST;

static Suite *
h265parse_suite (void)
{
  Suite *s = suite_create (ctx_suite);
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_parse_normal);
  tcase_add_test (tc_chain, test_parse_drain_single);
  tcase_add_test (tc_chain, test_parse_split);
  tcase_add_test (tc_chain, test_parse_detect_stream);
  tcase_add_test (tc_chain, test_parse_detect_stream_with_hdr_sei);
  tcase_add_test (tc_chain, test_parse_fallback_profile);

  return s;
}


/* helper methods for GstHasness based tests */

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

static inline void
pull_and_check_full (GstHarness * h, const guint8 * data, gsize size,
    GstClockTime pts, GstBufferFlags flags)
{
  GstBuffer *b = gst_harness_pull (h);
  gst_check_buffer_data (b, data, size);
  fail_unless_equals_clocktime (GST_BUFFER_PTS (b), pts);
  if (flags)
    fail_unless (GST_BUFFER_FLAG_IS_SET (b, flags));
  gst_buffer_unref (b);
}

#define pull_and_check(h, data, pts, flags) \
  pull_and_check_full (h, data, sizeof (data), pts, flags)

#define pull_and_check_composite(h, pts, flags, ...) \
  G_STMT_START { \
    GstMapInfo info; \
    GstBuffer *cb; \
    \
    cb = composite_buffer (0, 0, __VA_ARGS__); \
    gst_buffer_map (cb, &info, GST_MAP_READ); \
    \
    pull_and_check_full (h, info.data, info.size, pts, flags); \
    \
    gst_buffer_unmap (cb, &info); \
    gst_buffer_unref (cb); \
  } G_STMT_END

#define pull_and_drop(h) \
  G_STMT_START { \
    GstBuffer *b = gst_harness_pull (h); \
    gst_buffer_unref (b); \
  } G_STMT_END

#define HEADER_DATA \
  const guint8 * const vps = sliced ? h265_128x128_sliced_vps : h265_128x128_vps; \
  const guint8 * const sps = sliced ? h265_128x128_sliced_sps : h265_128x128_sps; \
  const guint8 * const pps = sliced ? h265_128x128_sliced_pps : h265_128x128_pps; \
  const gsize vps_size = sliced ? sizeof (h265_128x128_sliced_vps) : sizeof (h265_128x128_vps); \
  const gsize sps_size = sliced ? sizeof (h265_128x128_sliced_sps) : sizeof (h265_128x128_sps); \
  const gsize pps_size = sliced ? sizeof (h265_128x128_sliced_pps) : sizeof (h265_128x128_pps)

#define SLICE_DATA \
  const guint8 * const slice_1 = sliced ? h265_128x128_slice_1_idr_n_lp : h265_128x128_slice_idr_n_lp; \
  const guint8 * const slice_2 = sliced ? h265_128x128_slice_2_idr_n_lp : NULL; \
  const gsize slice_1_size = sliced ? sizeof (h265_128x128_slice_1_idr_n_lp) : sizeof (h265_128x128_slice_idr_n_lp); \
  const gsize slice_2_size = sliced ? sizeof (h265_128x128_slice_2_idr_n_lp) : 0

#define bytestream_set_caps(h, in_align, out_align) \
  gst_harness_set_caps_str (h, \
      "video/x-h265, parsed=(boolean)false, stream-format=byte-stream, alignment=" in_align ", framerate=30/1", \
      "video/x-h265, parsed=(boolean)true, stream-format=byte-stream, alignment=" out_align)

static inline void
bytestream_push_first_au_inalign_nal (GstHarness * h, gboolean sliced)
{
  HEADER_DATA;
  SLICE_DATA;
  GstBuffer *buf;

  buf = wrap_buffer (vps, vps_size, 10, 0);
  fail_unless_equals_int (gst_harness_push (h, buf), GST_FLOW_OK);

  buf = wrap_buffer (sps, sps_size, 10, 0);
  fail_unless_equals_int (gst_harness_push (h, buf), GST_FLOW_OK);

  buf = wrap_buffer (pps, pps_size, 10, 0);
  fail_unless_equals_int (gst_harness_push (h, buf), GST_FLOW_OK);

  buf = wrap_buffer (slice_1, slice_1_size, 10, 0);
  fail_unless_equals_int (gst_harness_push (h, buf), GST_FLOW_OK);

  if (sliced) {
    buf = wrap_buffer (slice_2, slice_2_size, 10, 0);
    fail_unless_equals_int (gst_harness_push (h, buf), GST_FLOW_OK);
  }
}

static inline void
bytestream_push_first_au_inalign_au (GstHarness * h, gboolean sliced)
{
  HEADER_DATA;
  SLICE_DATA;
  GstBuffer *buf;

  buf = composite_buffer (10, 0, sliced ? 5 : 4,
      vps, vps_size, sps, sps_size, pps, pps_size,
      slice_1, slice_1_size, slice_2, slice_2_size);
  fail_unless_equals_int (gst_harness_push (h, buf), GST_FLOW_OK);
}

/* tests */

static void
test_flow_outalign_nal (GstHarness * h)
{
  GstBuffer *buf;

  /* drop the first AU - tested separately */
  fail_unless (gst_harness_buffers_in_queue (h) > 0);
  while (gst_harness_buffers_in_queue (h) > 0)
    pull_and_drop (h);

  buf = wrap_buffer (h265_128x128_slice_idr_n_lp,
      sizeof (h265_128x128_slice_idr_n_lp), 100, 0);
  fail_unless_equals_int (gst_harness_push (h, buf), GST_FLOW_OK);
  fail_unless_equals_int (gst_harness_buffers_in_queue (h), 1);
  pull_and_check (h, h265_128x128_slice_idr_n_lp, 100, 0);

  buf = wrap_buffer (h265_128x128_slice_idr_n_lp,
      sizeof (h265_128x128_slice_idr_n_lp), 200, 0);
  fail_unless_equals_int (gst_harness_push (h, buf), GST_FLOW_OK);
  fail_unless_equals_int (gst_harness_buffers_in_queue (h), 1);
  pull_and_check (h, h265_128x128_slice_idr_n_lp, 200, 0);
}

static void
test_flow_outalign_au (GstHarness * h)
{
  GstBuffer *buf;

  /* drop the first AU - tested separately */
  fail_unless_equals_int (gst_harness_buffers_in_queue (h), 1);
  pull_and_drop (h);

  buf = wrap_buffer (h265_128x128_slice_idr_n_lp,
      sizeof (h265_128x128_slice_idr_n_lp), 100, 0);
  fail_unless_equals_int (gst_harness_push (h, buf), GST_FLOW_OK);
  fail_unless_equals_int (gst_harness_buffers_in_queue (h), 1);
  pull_and_check (h, h265_128x128_slice_idr_n_lp, 100, 0);

  buf = wrap_buffer (h265_128x128_slice_idr_n_lp,
      sizeof (h265_128x128_slice_idr_n_lp), 200, 0);
  fail_unless_equals_int (gst_harness_push (h, buf), GST_FLOW_OK);
  fail_unless_equals_int (gst_harness_buffers_in_queue (h), 1);
  pull_and_check (h, h265_128x128_slice_idr_n_lp, 200, 0);
}

GST_START_TEST (test_flow_nal_nal)
{
  GstHarness *h = gst_harness_new ("h265parse");

  bytestream_set_caps (h, "nal", "nal");
  bytestream_push_first_au_inalign_nal (h, FALSE);
  test_flow_outalign_nal (h);

  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_flow_au_nal)
{
  GstHarness *h = gst_harness_new ("h265parse");

  bytestream_set_caps (h, "au", "nal");
  bytestream_push_first_au_inalign_au (h, FALSE);
  test_flow_outalign_nal (h);

  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_flow_nal_au)
{
  GstHarness *h = gst_harness_new ("h265parse");
  GstBuffer *buf;

  bytestream_set_caps (h, "nal", "au");
  bytestream_push_first_au_inalign_nal (h, FALSE);

  /* special case because we have latency */

  fail_unless_equals_int (gst_harness_buffers_in_queue (h), 0);

  buf = wrap_buffer (h265_128x128_slice_idr_n_lp,
      sizeof (h265_128x128_slice_idr_n_lp), 100, 0);
  fail_unless_equals_int (gst_harness_push (h, buf), GST_FLOW_OK);

  /* drop the first AU - tested separately */
  fail_unless_equals_int (gst_harness_buffers_in_queue (h), 1);
  pull_and_drop (h);

  buf = wrap_buffer (h265_128x128_slice_idr_n_lp,
      sizeof (h265_128x128_slice_idr_n_lp), 200, 0);
  fail_unless_equals_int (gst_harness_push (h, buf), GST_FLOW_OK);

  fail_unless_equals_int (gst_harness_buffers_in_queue (h), 1);
  pull_and_check (h, h265_128x128_slice_idr_n_lp, 100, 0);

  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_flow_au_au)
{
  GstHarness *h = gst_harness_new ("h265parse");

  bytestream_set_caps (h, "au", "au");
  bytestream_push_first_au_inalign_au (h, FALSE);
  test_flow_outalign_au (h);

  gst_harness_teardown (h);
}

GST_END_TEST;

static void
test_headers_outalign_nal (GstHarness * h)
{
  /* 5 -> AUD + VPS + SPS + PPS + slice */
  fail_unless_equals_int (gst_harness_buffers_in_queue (h), 4);

  /* parser must have inserted AUD before the headers, with the same PTS */
  pull_and_check (h, h265_128x128_vps, 10, 0);
  pull_and_check (h, h265_128x128_sps, 10, 0);
  pull_and_check (h, h265_128x128_pps, 10, 0);

  /* FIXME The timestamp should be 10 really, but base parse refuse to repeat
   * the same TS for two consecutive calls to _finish_frame(), see [0] for
   * more details. It's not a huge issue, the decoder can fix it for now.
   *
   * [0] https://gitlab.freedesktop.org/gstreamer/gstreamer/-/merge_requests/287
   */
  pull_and_check (h, h265_128x128_slice_idr_n_lp, -1, 0);
}

static void
test_headers_outalign_au (GstHarness * h)
{
  fail_unless_equals_int (gst_harness_buffers_in_queue (h), 1);
  pull_and_check_composite (h, 10, 0, 4,
      h265_128x128_vps, sizeof (h265_128x128_vps),
      h265_128x128_sps, sizeof (h265_128x128_sps),
      h265_128x128_pps, sizeof (h265_128x128_pps),
      h265_128x128_slice_idr_n_lp, sizeof (h265_128x128_slice_idr_n_lp));
}

GST_START_TEST (test_headers_nal_nal)
{
  GstHarness *h = gst_harness_new ("h265parse");

  bytestream_set_caps (h, "nal", "nal");
  bytestream_push_first_au_inalign_nal (h, FALSE);
  test_headers_outalign_nal (h);

  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_headers_au_nal)
{
  GstHarness *h = gst_harness_new ("h265parse");

  bytestream_set_caps (h, "au", "nal");
  bytestream_push_first_au_inalign_au (h, FALSE);
  test_headers_outalign_nal (h);

  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_headers_au_au)
{
  GstHarness *h = gst_harness_new ("h265parse");

  bytestream_set_caps (h, "au", "au");
  bytestream_push_first_au_inalign_au (h, FALSE);
  test_headers_outalign_au (h);

  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_latency_nal_nal)
{
  GstHarness *h = gst_harness_new ("h265parse");

  bytestream_set_caps (h, "nal", "nal");
  bytestream_push_first_au_inalign_nal (h, FALSE);

  fail_unless_equals_clocktime (gst_harness_query_latency (h), 0);

  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_latency_au_nal)
{
  GstHarness *h = gst_harness_new ("h265parse");

  bytestream_set_caps (h, "au", "nal");
  bytestream_push_first_au_inalign_au (h, FALSE);

  fail_unless_equals_clocktime (gst_harness_query_latency (h), 0);

  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_latency_nal_au)
{
  GstHarness *h = gst_harness_new ("h265parse");
  GstBuffer *buf;

  bytestream_set_caps (h, "nal", "au");
  bytestream_push_first_au_inalign_nal (h, FALSE);

  /* special case because we have latency;
   * the first buffer needs to be pushed out
   * before we can correctly query the latency */
  fail_unless_equals_int (gst_harness_buffers_in_queue (h), 0);
  buf = wrap_buffer (h265_128x128_slice_idr_n_lp,
      sizeof (h265_128x128_slice_idr_n_lp), 100, 0);
  fail_unless_equals_int (gst_harness_push (h, buf), GST_FLOW_OK);

  /* our input caps declare framerate=30fps, so the latency must be 1/30 sec */
  fail_unless_equals_clocktime (gst_harness_query_latency (h),
      gst_util_uint64_scale (GST_SECOND, 1, 30));

  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_latency_au_au)
{
  GstHarness *h = gst_harness_new ("h265parse");

  bytestream_set_caps (h, "au", "au");
  bytestream_push_first_au_inalign_au (h, FALSE);

  fail_unless_equals_clocktime (gst_harness_query_latency (h), 0);

  gst_harness_teardown (h);
}

GST_END_TEST;

static void
test_discont_outalign_nal (GstHarness * h)
{
  GstBuffer *buf;

  /* drop the first AU - tested separately */
  fail_unless (gst_harness_buffers_in_queue (h) > 0);
  while (gst_harness_buffers_in_queue (h) > 0)
    pull_and_drop (h);

  /* FIXME: I think the AUD ought to have DISCONT */
  buf = wrap_buffer (h265_128x128_slice_idr_n_lp,
      sizeof (h265_128x128_slice_idr_n_lp), 1000, GST_BUFFER_FLAG_DISCONT);
  fail_unless_equals_int (gst_harness_push (h, buf), GST_FLOW_OK);
  fail_unless_equals_int (gst_harness_buffers_in_queue (h), 1);
  pull_and_check (h, h265_128x128_slice_idr_n_lp, 1000,
      GST_BUFFER_FLAG_DISCONT);
}

static void
test_discont_outalign_au (GstHarness * h)
{
  GstBuffer *buf;

  /* drop the first AU - tested separately */
  fail_unless_equals_int (gst_harness_buffers_in_queue (h), 1);
  pull_and_drop (h);

  buf = wrap_buffer (h265_128x128_slice_idr_n_lp,
      sizeof (h265_128x128_slice_idr_n_lp), 1000, GST_BUFFER_FLAG_DISCONT);
  fail_unless_equals_int (gst_harness_push (h, buf), GST_FLOW_OK);
  fail_unless_equals_int (gst_harness_buffers_in_queue (h), 1);
  pull_and_check (h, h265_128x128_slice_idr_n_lp, 1000,
      GST_BUFFER_FLAG_DISCONT);
}

GST_START_TEST (test_discont_nal_nal)
{
  GstHarness *h = gst_harness_new ("h265parse");

  bytestream_set_caps (h, "nal", "nal");
  bytestream_push_first_au_inalign_nal (h, FALSE);
  test_discont_outalign_nal (h);

  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_discont_au_nal)
{
  GstHarness *h = gst_harness_new ("h265parse");

  bytestream_set_caps (h, "au", "nal");
  bytestream_push_first_au_inalign_au (h, FALSE);
  test_discont_outalign_nal (h);

  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_discont_au_au)
{
  GstHarness *h = gst_harness_new ("h265parse");

  bytestream_set_caps (h, "au", "au");
  bytestream_push_first_au_inalign_au (h, FALSE);
  test_discont_outalign_au (h);

  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_sliced_nal_nal)
{
  GstHarness *h = gst_harness_new ("h265parse");
  GstBuffer *buf;

  bytestream_set_caps (h, "nal", "nal");
  bytestream_push_first_au_inalign_nal (h, TRUE);

  /* drop the header buffers */
  fail_unless (gst_harness_buffers_in_queue (h) > 2);
  while (gst_harness_buffers_in_queue (h) > 2)
    pull_and_drop (h);

  /* but expect 2 slices */
  pull_and_check (h, h265_128x128_slice_1_idr_n_lp, -1, 0);
  pull_and_check (h, h265_128x128_slice_2_idr_n_lp, -1, 0);

  /* push some more */
  buf = wrap_buffer (h265_128x128_slice_1_idr_n_lp,
      sizeof (h265_128x128_slice_1_idr_n_lp), 100, 0);
  fail_unless_equals_int (gst_harness_push (h, buf), GST_FLOW_OK);
  fail_unless_equals_int (gst_harness_buffers_in_queue (h), 1);
  pull_and_check (h, h265_128x128_slice_1_idr_n_lp, 100, 0);

  buf = wrap_buffer (h265_128x128_slice_2_idr_n_lp,
      sizeof (h265_128x128_slice_2_idr_n_lp), 100, 0);
  fail_unless_equals_int (gst_harness_push (h, buf), GST_FLOW_OK);
  fail_unless_equals_int (gst_harness_buffers_in_queue (h), 1);
  pull_and_check (h, h265_128x128_slice_2_idr_n_lp, -1, 0);

  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_sliced_au_nal)
{
  GstHarness *h = gst_harness_new ("h265parse");
  GstBuffer *buf;

  bytestream_set_caps (h, "au", "nal");
  bytestream_push_first_au_inalign_au (h, TRUE);

  /* drop the header buffers */
  fail_unless (gst_harness_buffers_in_queue (h) > 2);
  while (gst_harness_buffers_in_queue (h) > 2)
    pull_and_drop (h);

  /* but expect 2 slices */
  pull_and_check (h, h265_128x128_slice_1_idr_n_lp, -1, 0);
  pull_and_check (h, h265_128x128_slice_2_idr_n_lp, -1, 0);

  /* push some more */
  buf = composite_buffer (100, 0, 2,
      h265_128x128_slice_1_idr_n_lp, sizeof (h265_128x128_slice_1_idr_n_lp),
      h265_128x128_slice_2_idr_n_lp, sizeof (h265_128x128_slice_2_idr_n_lp));
  fail_unless_equals_int (gst_harness_push (h, buf), GST_FLOW_OK);
  fail_unless_equals_int (gst_harness_buffers_in_queue (h), 2);
  pull_and_check (h, h265_128x128_slice_1_idr_n_lp, 100, 0);
  pull_and_check (h, h265_128x128_slice_2_idr_n_lp, -1, 0);

  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_sliced_nal_au)
{
  GstHarness *h = gst_harness_new ("h265parse");
  GstBuffer *buf;

  bytestream_set_caps (h, "nal", "au");
  bytestream_push_first_au_inalign_nal (h, TRUE);

  /* nal -> au has latency; we need to start the next AU to get output */
  fail_unless_equals_int (gst_harness_buffers_in_queue (h), 0);


  /* push some more */
  buf = wrap_buffer (h265_128x128_slice_1_idr_n_lp,
      sizeof (h265_128x128_slice_1_idr_n_lp), 100, 0);
  fail_unless_equals_int (gst_harness_push (h, buf), GST_FLOW_OK);

  /* now we can see the initial AU on the output */
  fail_unless_equals_int (gst_harness_buffers_in_queue (h), 1);
  pull_and_check_composite (h, 10, 0, 5,
      h265_128x128_sliced_vps, sizeof (h265_128x128_sliced_vps),
      h265_128x128_sliced_sps, sizeof (h265_128x128_sliced_sps),
      h265_128x128_sliced_pps, sizeof (h265_128x128_sliced_pps),
      h265_128x128_slice_1_idr_n_lp, sizeof (h265_128x128_slice_1_idr_n_lp),
      h265_128x128_slice_2_idr_n_lp, sizeof (h265_128x128_slice_2_idr_n_lp));

  buf = wrap_buffer (h265_128x128_slice_2_idr_n_lp,
      sizeof (h265_128x128_slice_2_idr_n_lp), 100, 0);
  fail_unless_equals_int (gst_harness_push (h, buf), GST_FLOW_OK);
  fail_unless_equals_int (gst_harness_buffers_in_queue (h), 0);

  buf = wrap_buffer (h265_128x128_slice_1_idr_n_lp,
      sizeof (h265_128x128_slice_1_idr_n_lp), 200, 0);
  fail_unless_equals_int (gst_harness_push (h, buf), GST_FLOW_OK);

  fail_unless_equals_int (gst_harness_buffers_in_queue (h), 1);
  pull_and_check_composite (h, 100, 0, 2,
      h265_128x128_slice_1_idr_n_lp, sizeof (h265_128x128_slice_1_idr_n_lp),
      h265_128x128_slice_2_idr_n_lp, sizeof (h265_128x128_slice_2_idr_n_lp));

  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_sliced_au_au)
{
  GstHarness *h = gst_harness_new ("h265parse");
  GstBuffer *buf;

  bytestream_set_caps (h, "au", "au");
  bytestream_push_first_au_inalign_au (h, TRUE);

  fail_unless_equals_int (gst_harness_buffers_in_queue (h), 1);
  pull_and_check_composite (h, 10, 0, 5,
      h265_128x128_sliced_vps, sizeof (h265_128x128_sliced_vps),
      h265_128x128_sliced_sps, sizeof (h265_128x128_sliced_sps),
      h265_128x128_sliced_pps, sizeof (h265_128x128_sliced_pps),
      h265_128x128_slice_1_idr_n_lp, sizeof (h265_128x128_slice_1_idr_n_lp),
      h265_128x128_slice_2_idr_n_lp, sizeof (h265_128x128_slice_2_idr_n_lp));

  /* push some more */
  buf = composite_buffer (100, 0, 2,
      h265_128x128_slice_1_idr_n_lp, sizeof (h265_128x128_slice_1_idr_n_lp),
      h265_128x128_slice_2_idr_n_lp, sizeof (h265_128x128_slice_2_idr_n_lp));
  fail_unless_equals_int (gst_harness_push (h, buf), GST_FLOW_OK);
  fail_unless_equals_int (gst_harness_buffers_in_queue (h), 1);
  pull_and_check_composite (h, 100, 0, 2,
      h265_128x128_slice_1_idr_n_lp, sizeof (h265_128x128_slice_1_idr_n_lp),
      h265_128x128_slice_2_idr_n_lp, sizeof (h265_128x128_slice_2_idr_n_lp));

  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_parse_skip_to_4bytes_sc)
{
  GstHarness *h;
  GstBuffer *buf1, *buf2;
  const guint8 initial_bytes[] = { 0x00, 0x00, 0x00, 0x00, 0x01, h265_vps[4] };
  GstMapInfo map;

  h = gst_harness_new ("h265parse");

  gst_harness_set_caps_str (h, "video/x-h265, stream-format=byte-stream",
      "video/x-h265, stream-format=byte-stream, alignment=nal");

  /* padding bytes, four bytes start code and 1 of the two identification
   * bytes. */
  buf1 = wrap_buffer (initial_bytes, sizeof (initial_bytes), 100, 0);

  /* The second contains the an VPS, starting from second NAL identification
   * byte and is followed by an SPS, IDR to ensure that the NAL end can be
   * found */
  buf2 = composite_buffer (100, 0, 4, h265_vps + 5, sizeof (h265_vps) - 5,
      h265_sps, sizeof (h265_sps), h265_pps, sizeof (h265_pps),
      h265_idr, sizeof (h265_idr));

  fail_unless_equals_int (gst_harness_push (h, buf1), GST_FLOW_OK);
  fail_unless_equals_int (gst_harness_buffers_in_queue (h), 0);

  fail_unless_equals_int (gst_harness_push (h, buf2), GST_FLOW_OK);
  /* The parser will deliver VPS, SPS, PPS as it now have complete cpas */
  fail_unless_equals_int (gst_harness_buffers_in_queue (h), 3);

  buf1 = gst_harness_pull (h);
  gst_buffer_map (buf1, &map, GST_MAP_READ);
  fail_unless_equals_int (gst_buffer_get_size (buf1), sizeof (h265_vps));
  gst_buffer_unmap (buf1, &map);
  gst_buffer_unref (buf1);

  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_parse_sc_with_half_header)
{
  GstHarness *h;
  GstBuffer *buf1, *buf2;
  GstMapInfo map;

  h = gst_harness_new ("h265parse");

  gst_harness_set_caps_str (h, "video/x-h265, stream-format=byte-stream",
      "video/x-h265, stream-format=byte-stream, alignment=nal");

  buf1 = composite_buffer (100, 0, 4, h265_vps, sizeof (h265_vps),
      h265_sps, sizeof (h265_sps), h265_pps, sizeof (h265_pps), h265_idr, 5);
  buf2 = wrap_buffer (h265_idr + 5, sizeof (h265_idr) - 5, 100, 0);

  fail_unless_equals_int (gst_harness_push (h, buf1), GST_FLOW_OK);
  fail_unless_equals_int (gst_harness_buffers_in_queue (h), 0);

  fail_unless_equals_int (gst_harness_push (h, buf2), GST_FLOW_OK);
  /* The parser will deliver VPS, SPS, PPS as it now have complete cpas */
  fail_unless_equals_int (gst_harness_buffers_in_queue (h), 3);

  buf1 = gst_harness_pull (h);
  gst_buffer_map (buf1, &map, GST_MAP_READ);
  fail_unless_equals_int (gst_buffer_get_size (buf1), sizeof (h265_vps));
  gst_buffer_unmap (buf1, &map);
  gst_buffer_unref (buf1);

  gst_harness_teardown (h);
}

GST_END_TEST;



/* nal->au has latency, but EOS should force the last AU out */
GST_START_TEST (test_drain)
{
  GstHarness *h = gst_harness_new ("h265parse");

  bytestream_set_caps (h, "nal", "au");
  bytestream_push_first_au_inalign_nal (h, FALSE);

  fail_unless_equals_int (gst_harness_buffers_in_queue (h), 0);

  gst_harness_push_event (h, gst_event_new_eos ());

  fail_unless_equals_int (gst_harness_buffers_in_queue (h), 1);
  pull_and_drop (h);

  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_parse_sei_userdefinedunregistered)
{
  GstVideoSEIUserDataUnregisteredMeta *meta;
  GstHarness *h;
  GstBuffer *buf;

  const guint8 bytestream[] = {
    0x00, 0x00, 0x00, 0x01, 0x42, 0x01, 0x01, 0x04, 0x08, 0x00, 0x00, 0x03,
    0x00, 0x9e, 0x08, 0x00, 0x00, 0x03, 0x00, 0x00, 0x1e, 0x90, 0x11, 0x08,
    0xb2, 0xca, 0xcd, 0x57, 0x95, 0xcd, 0xc0, 0x80, 0x80, 0x01, 0x00, 0x00,
    0x03, 0x00, 0x01, 0x00, 0x00, 0x03, 0x00, 0x19, 0x08, 0x00, 0x00, 0x00,
    0x01, 0x44, 0x01, 0xc1, 0x73, 0x18, 0x31, 0x08, 0x90,
    // SEI
    0x00, 0x00, 0x01, 0x4e, 0x01,
    0x05,                       // SEI Type.
    0x18,                       // SEI Payload size (16 UUID size + 8 payload size = 24).
    // SEI User Data Unregistered UUID.
    0xee, 0x2c, 0xa2, 0xde, 0x09, 0xb5, 0x17, 0x47, 0xdb, 0xbb, 0x55, 0xa4,
    0xfe, 0x7f, 0xc2, 0xfc,
    // SEI User Data Unregistered Payload.
    0x4e, 0x78, 0x32, 0x36, 0x35, 0x20, 0x28, 0x62,
  };
  const gsize bytestream_size = sizeof (bytestream);
  const guint8 uuid[] = {
    0xee, 0x2c, 0xa2, 0xde, 0x09, 0xb5, 0x17, 0x47, 0xdb, 0xbb, 0x55, 0xa4,
    0xfe, 0x7f, 0xc2, 0xfc
  };
  const guint8 payload[] = { 0x4e, 0x78, 0x32, 0x36, 0x35, 0x20, 0x28, 0x62 };

  h = gst_harness_new ("h265parse");
  gst_harness_set_src_caps_str (h, "video/x-h265, stream-format=byte-stream");

  buf = gst_buffer_new_and_alloc (bytestream_size);
  gst_buffer_fill (buf, 0, bytestream, bytestream_size);
  fail_unless_equals_int (gst_harness_push (h, buf), GST_FLOW_OK);

  gst_harness_push_event (h, gst_event_new_eos ());

  buf = gst_harness_pull (h);
  meta = gst_buffer_get_video_sei_user_data_unregistered_meta (buf);
  fail_unless (meta != NULL);

  fail_unless (memcmp (meta->uuid, uuid, 16) == 0);
  fail_unless_equals_int (meta->size, G_N_ELEMENTS (payload));
  fail_unless (memcmp (meta->data, payload, meta->size) == 0);

  gst_buffer_unref (buf);

  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_invalid_sei_in_hvcc)
{
  GstHarness *h;
  GstCaps *caps;
  GstBuffer *codec_data;
  /* Consists of 4 arrays (VPS, SPS, PPS, SEI -> broken) and each array contains
   * single nalu
   * Captured from the log at
   * https://gitlab.freedesktop.org/gstreamer/gstreamer/-/issues/2905
   */
  static const guint8 hvcc_data[] = {
    0x01, 0x01, 0x01, 0x01, 0x60, 0x00, 0x90, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0xf0, 0x00, 0xfc, 0xfd, 0xf8, 0xf8, 0x00, 0x00, 0x0f, 0x04, 0x20,
    0x00, 0x01, 0x00, 0x17, 0x40, 0x01, 0x0c, 0x01, 0xff, 0xff, 0x01, 0x60,
    0x00, 0x00, 0x03, 0x00, 0x80, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00,
    0x7b, 0xac, 0x09, 0x21, 0x00, 0x01, 0x00, 0x42, 0x42, 0x01, 0x01, 0x01,
    0x60, 0x00, 0x00, 0x03, 0x00, 0x80, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03,
    0x00, 0x7b, 0xa0, 0x02, 0x80, 0x80, 0x2d, 0x1f, 0xe3, 0x6b, 0xbb, 0x53,
    0x77, 0x72, 0x5d, 0x60, 0x2d, 0xc0, 0x40, 0x40, 0x41, 0x00, 0x00, 0x03,
    0x03, 0xe8, 0x00, 0x00, 0x4e, 0x20, 0x72, 0x1d, 0xee, 0x51, 0x00, 0x05,
    0xdc, 0x00, 0x00, 0x1a, 0x5e, 0x00, 0x00, 0x2e, 0xe0, 0x00, 0x00, 0xd2,
    0xf0, 0x08, 0x22, 0x00, 0x01, 0x00, 0x0b, 0x44, 0x01, 0xc1, 0x72, 0xb0,
    0x9c, 0x38, 0x77, 0x06, 0x0c, 0x24, 0x27, 0x00, 0x01, 0x00, 0x00, 0x00
  };

  caps = gst_caps_new_simple ("video/x-h265", "stream-format", G_TYPE_STRING,
      "hvc1", "alignment", G_TYPE_STRING, "au", NULL);
  codec_data = gst_buffer_new_memdup (hvcc_data, sizeof (hvcc_data));
  gst_caps_set_simple (caps, "codec_data", GST_TYPE_BUFFER, codec_data, NULL);
  gst_buffer_unref (codec_data);

  h = gst_harness_new ("h265parse");
  gst_harness_set_src_caps (h, caps);
  gst_harness_push_event (h, gst_event_new_eos ());

  while (TRUE) {
    GstEvent *event = gst_harness_pull_event (h);
    fail_unless (event);

    if (GST_EVENT_TYPE (event) == GST_EVENT_CAPS) {
      GstStructure *s;
      gint width, height;

      gst_event_parse_caps (event, &caps);
      s = gst_caps_get_structure (caps, 0);

      fail_unless (gst_structure_get_int (s, "width", &width));
      fail_unless_equals_int (width, 1280);
      fail_unless (gst_structure_get_int (s, "height", &height));
      fail_unless_equals_int (height, 720);

      gst_event_unref (event);
      break;
    }

    gst_event_unref (event);
  }

  gst_harness_teardown (h);
}

GST_END_TEST;

static Suite *
h265parse_harnessed_suite (void)
{
  Suite *s = suite_create ("h265parse");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_flow_nal_nal);
  tcase_add_test (tc_chain, test_flow_au_nal);
  tcase_add_test (tc_chain, test_flow_nal_au);
  tcase_add_test (tc_chain, test_flow_au_au);

  tcase_add_test (tc_chain, test_headers_nal_nal);
  tcase_add_test (tc_chain, test_headers_au_nal);
  tcase_add_test (tc_chain, test_headers_au_au);

  tcase_add_test (tc_chain, test_latency_nal_nal);
  tcase_add_test (tc_chain, test_latency_au_nal);
  tcase_add_test (tc_chain, test_latency_nal_au);
  tcase_add_test (tc_chain, test_latency_au_au);

  tcase_add_test (tc_chain, test_discont_nal_nal);
  tcase_add_test (tc_chain, test_discont_au_nal);
  tcase_add_test (tc_chain, test_discont_au_au);

  tcase_add_test (tc_chain, test_sliced_nal_nal);
  tcase_add_test (tc_chain, test_sliced_au_nal);
  tcase_add_test (tc_chain, test_sliced_nal_au);
  tcase_add_test (tc_chain, test_sliced_au_au);

  tcase_add_test (tc_chain, test_parse_skip_to_4bytes_sc);
  tcase_add_test (tc_chain, test_parse_sc_with_half_header);

  tcase_add_test (tc_chain, test_drain);

  tcase_add_test (tc_chain, test_parse_sei_userdefinedunregistered);
  tcase_add_test (tc_chain, test_invalid_sei_in_hvcc);

  return s;
}

int
main (int argc, char **argv)
{
  int nf = 0;

  Suite *s;

  gst_check_init (&argc, &argv);

  /* init test context */
  ctx_factory = "h265parse";
  ctx_sink_template = &sinktemplate_bs_au;
  ctx_src_template = &srctemplate;
  /* no timing info to parse */
  ctx_headers[0].data = h265_vps;
  ctx_headers[0].size = sizeof (h265_vps);
  ctx_headers[1].data = h265_sps;
  ctx_headers[1].size = sizeof (h265_sps);
  ctx_headers[2].data = h265_pps;
  ctx_headers[2].size = sizeof (h265_pps);
  ctx_verify_buffer = verify_buffer_bs_au;

  /* discard initial vps/sps/pps buffers */
  ctx_discard = 0;
  /* no timing info to parse */
  ctx_no_metadata = TRUE;
  ctx_codec_data = FALSE;

  ctx_suite = "h265parse_to_bs_au";
  s = h265parse_suite ();
  nf += gst_check_run_suite (s, ctx_suite, __FILE__ "_to_bs_au.c");

  ctx_suite = "h265parse_harnessed";
  s = h265parse_harnessed_suite ();
  nf += gst_check_run_suite (s, ctx_suite, __FILE__ "_harnessed.c");

  return nf;
}
