/* GStreamer
 *
 * unit test for rawvideoparse
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

#include <gst/check/gstcheck.h>
#include <gst/video/video.h>

/* The checks use as test data an 8x8 Y444 image, with 25 Hz framerate. In the
 * sink caps configuration, the stride is 8 bytes, and the frames are tightly
 * packed together. In the properties configuration, the stride is 10 bytes, the
 * planes aren't tightly packed (there are 20 bytes between the planes), and the
 * frames overall have padding between them (the overall frame size is
 * stride (10) * height (8) * num-planes (3) + bytes-between-planes (20) * 2
 * = 280 bytes, and the frame stride is 500 bytes, so there are 220 bytes of
 * extra padding between frames).
 *
 * In the test 8x8 frame, the pixels are all set to #000000, except for two
 * pixels: (xofs+1 yofs+0) is set to #8899AA, (xofs+0 yofs+1) is set to #112233.
 * The first frame uses the offsets xofs=0 yofs=0. The second frame uses
 * xofs=1 yofs=0 etc. For each configuration, there is a separate set of frames,
 * each stored in the GstAdapter in the Context struct.
 *
 * During the tests, as part of the checks, the pixels are verified to have the
 * right values. The pattern of the pixels was chosen to easily detect stride
 * errors, incorrect plane offsets etc.
 */

#define TEST_WIDTH 8
#define TEST_HEIGHT 8
#define TEST_FRAMERATE_N 25
#define TEST_FRAMERATE_D 1
#define TEST_FRAME_FORMAT GST_VIDEO_FORMAT_Y444
#define NUM_TEST_PLANES 3

#define PROP_CTX_PLANE_STRIDE 10
#define PROP_CTX_FRAME_SIZE 500
#define PROP_CTX_PLANE_PADDING 20
#define PROP_CTX_PLANE_SIZE (PROP_CTX_PLANE_STRIDE * TEST_HEIGHT + PROP_CTX_PLANE_PADDING)

GstElement *rawvideoparse;

/* For ease of programming we use globals to keep refs for our floating
 * src and sink pads we create; otherwise we always have to do get_pad,
 * get_peer, and then remove references in every test function */
static GstPad *mysrcpad, *mysinkpad;

typedef struct
{
  GstAdapter *data;
  guint plane_stride;
  guint plane_size;
}
Context;

static Context properties_ctx, sinkcaps_ctx;

static void
set_pixel (Context const *ctx, guint8 * pixels, guint x, guint y, guint32 color)
{
  guint i;
  guint ofs = y * ctx->plane_stride + x;
  for (i = 0; i < NUM_TEST_PLANES; ++i)
    pixels[ctx->plane_size * i + ofs] =
        (color >> ((NUM_TEST_PLANES - 1 - i) * 8)) & 0xFF;
}

static guint32
get_pixel (Context const *ctx, const guint8 * pixels, guint x, guint y)
{
  guint i;
  guint ofs = y * ctx->plane_stride + x;
  guint32 color = 0;
  for (i = 0; i < NUM_TEST_PLANES; ++i)
    color |=
        ((guint32) (pixels[ctx->plane_size * i + ofs])) << ((NUM_TEST_PLANES -
            1 - i) * 8);
  return color;
}

static void
fill_test_pattern (Context const *ctx, GstBuffer * buffer, guint xofs,
    guint yofs)
{
  guint8 *pixels;
  GstMapInfo map_info;

  gst_buffer_map (buffer, &map_info, GST_MAP_WRITE);
  pixels = map_info.data;

  memset (pixels, 0, ctx->plane_size * NUM_TEST_PLANES);
  set_pixel (ctx, pixels, 1 + xofs, 0 + yofs, 0x8899AA);
  set_pixel (ctx, pixels, 0 + xofs, 1 + yofs, 0x112233);

  gst_buffer_unmap (buffer, &map_info);
}

static void
check_test_pattern (Context const *ctx, GstBuffer * buffer, guint xofs,
    guint yofs)
{
  guint x, y;
  guint8 *pixels;
  GstMapInfo map_info;

  gst_buffer_map (buffer, &map_info, GST_MAP_READ);
  pixels = map_info.data;

  fail_unless_equals_uint64_hex (get_pixel (ctx, pixels, 1 + xofs, 0 + yofs),
      0x8899AA);
  fail_unless_equals_uint64_hex (get_pixel (ctx, pixels, 0 + xofs, 1 + yofs),
      0x112233);

  for (y = 0; y < TEST_HEIGHT; ++y) {
    for (x = 0; x < TEST_WIDTH; ++x) {
      if ((x == (1 + xofs) && y == (0 + yofs)) || (x == (0 + xofs)
              && y == (1 + yofs)))
        continue;

      fail_unless_equals_uint64_hex (get_pixel (ctx, pixels, x, y), 0x000000);
    }
  }

  gst_buffer_unmap (buffer, &map_info);
}


static void
setup_rawvideoparse (gboolean use_sink_caps,
    gboolean set_properties, GstCaps * incaps, GstFormat format)
{
  guint i;


  /* Setup the rawvideoparse element and the pads */

  static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
      GST_PAD_SINK,
      GST_PAD_ALWAYS,
      GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE (GST_VIDEO_FORMATS_ALL))
      );
  static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
      GST_PAD_SRC,
      GST_PAD_ALWAYS,
      GST_STATIC_CAPS_ANY);

  rawvideoparse = gst_check_setup_element ("rawvideoparse");

  properties_ctx.plane_stride = PROP_CTX_PLANE_STRIDE;
  properties_ctx.plane_size = PROP_CTX_PLANE_SIZE;
  properties_ctx.data = gst_adapter_new ();

  sinkcaps_ctx.plane_stride = TEST_WIDTH;
  sinkcaps_ctx.plane_size = TEST_WIDTH * TEST_HEIGHT;
  sinkcaps_ctx.data = gst_adapter_new ();

  g_object_set (G_OBJECT (rawvideoparse), "use-sink-caps", use_sink_caps, NULL);
  if (set_properties) {
    GValue plane_offsets = G_VALUE_INIT;
    GValue plane_strides = G_VALUE_INIT;
    GValue val = G_VALUE_INIT;

    g_value_init (&val, G_TYPE_INT);
    g_value_init (&plane_offsets, GST_TYPE_ARRAY);
    g_value_init (&plane_strides, GST_TYPE_ARRAY);

    for (i = 0; i < NUM_TEST_PLANES; ++i) {
      g_value_set_int (&val, properties_ctx.plane_size * i);
      gst_value_array_append_value (&plane_offsets, &val);
    }

    for (i = 0; i < NUM_TEST_PLANES; ++i) {
      g_value_set_int (&val, properties_ctx.plane_stride);
      gst_value_array_append_value (&plane_strides, &val);
    }

    g_value_unset (&val);

    g_object_set (G_OBJECT (rawvideoparse), "width", TEST_WIDTH, "height",
        TEST_HEIGHT, "frame-size", PROP_CTX_FRAME_SIZE, "framerate",
        TEST_FRAMERATE_N, TEST_FRAMERATE_D, "format", TEST_FRAME_FORMAT, NULL);
    g_object_set_property (G_OBJECT (rawvideoparse), "plane-offsets",
        &plane_offsets);
    g_object_set_property (G_OBJECT (rawvideoparse), "plane-strides",
        &plane_strides);

    g_value_unset (&plane_offsets);
    g_value_unset (&plane_strides);
  }

  /* Check that the plane stride/offset values are correct */
  {
    GValue plane_offsets_array = G_VALUE_INIT;
    GValue plane_strides_array = G_VALUE_INIT;

    /* By default, 320x240 i420 is used as format */
    guint plane_offsets[3] = { 0, 76800, 96000 };
    guint plane_strides[3] = { 320, 160, 160 };

    g_value_init (&plane_offsets_array, GST_TYPE_ARRAY);
    g_value_init (&plane_strides_array, GST_TYPE_ARRAY);

    if (set_properties) {
      /* When properties are explicitly set, we use Y444 as video format,
       * so in that case, plane stride values are all the same */
      plane_offsets[0] = properties_ctx.plane_size * 0;
      plane_offsets[1] = properties_ctx.plane_size * 1;
      plane_offsets[2] = properties_ctx.plane_size * 2;
      plane_strides[0] = plane_strides[1] = plane_strides[2] =
          properties_ctx.plane_stride;
    }

    g_object_get_property (G_OBJECT (rawvideoparse), "plane-offsets",
        &plane_offsets_array);
    g_object_get_property (G_OBJECT (rawvideoparse), "plane-strides",
        &plane_strides_array);

    fail_unless (gst_value_array_get_size (&plane_offsets_array) ==
        gst_value_array_get_size (&plane_strides_array));

    for (i = 0; i < gst_value_array_get_size (&plane_offsets_array); ++i) {
      const GValue *gvalue;

      gvalue = gst_value_array_get_value (&plane_offsets_array, i);
      fail_unless (gvalue != NULL);
      fail_unless_equals_uint64 (plane_offsets[i], g_value_get_int (gvalue));

      gvalue = gst_value_array_get_value (&plane_strides_array, i);
      fail_unless (gvalue != NULL);
      fail_unless_equals_uint64 (plane_strides[i], g_value_get_int (gvalue));
    }

    g_value_unset (&plane_offsets_array);
    g_value_unset (&plane_strides_array);
  }

  fail_unless (gst_element_set_state (rawvideoparse,
          GST_STATE_PAUSED) == GST_STATE_CHANGE_SUCCESS,
      "could not set to paused");

  mysrcpad = gst_check_setup_src_pad (rawvideoparse, &srctemplate);
  mysinkpad = gst_check_setup_sink_pad (rawvideoparse, &sinktemplate);

  gst_pad_set_active (mysrcpad, TRUE);
  gst_pad_set_active (mysinkpad, TRUE);

  gst_check_setup_events (mysrcpad, rawvideoparse, incaps, format);
  if (incaps)
    gst_caps_unref (incaps);


  /* Fill the adapters with test frames */

  for (i = 0; i < 10; ++i) {
    GstBuffer *buffer =
        gst_buffer_new_allocate (NULL, PROP_CTX_FRAME_SIZE, NULL);
    gst_buffer_memset (buffer, 0, 0xCC, gst_buffer_get_size (buffer));
    fill_test_pattern (&properties_ctx, buffer, i, 0);
    gst_adapter_push (properties_ctx.data, buffer);
  }

  for (i = 0; i < 10; ++i) {
    GstBuffer *buffer =
        gst_buffer_new_allocate (NULL, sinkcaps_ctx.plane_size * 3, NULL);
    gst_buffer_memset (buffer, 0, 0xCC, gst_buffer_get_size (buffer));
    fill_test_pattern (&sinkcaps_ctx, buffer, i, 0);
    gst_adapter_push (sinkcaps_ctx.data, buffer);
  }
}

static void
cleanup_rawvideoparse (void)
{
  int num_buffers, i;

  gst_pad_set_active (mysrcpad, FALSE);
  gst_pad_set_active (mysinkpad, FALSE);
  gst_check_teardown_src_pad (rawvideoparse);
  gst_check_teardown_sink_pad (rawvideoparse);
  gst_check_teardown_element (rawvideoparse);

  g_object_unref (G_OBJECT (properties_ctx.data));
  g_object_unref (G_OBJECT (sinkcaps_ctx.data));

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
push_data_and_check_output (Context * ctx, gsize num_in_bytes,
    gsize expected_num_out_bytes, gint64 expected_pts, gint64 expected_dur,
    guint expected_num_buffers_in_list, guint buf_idx, guint xofs, guint yofs)
{
  GstBuffer *inbuf, *outbuf;
  guint num_buffers;

  /* Simulate upstream input by taking num_in_bytes bytes from the adapter */
  inbuf = gst_adapter_take_buffer (ctx->data, num_in_bytes);
  fail_unless (inbuf != NULL);

  /* Push the input data and check that the output buffers list grew as
   * expected */
  fail_unless (gst_pad_push (mysrcpad, inbuf) == GST_FLOW_OK);
  num_buffers = g_list_length (buffers);
  fail_unless_equals_int (num_buffers, expected_num_buffers_in_list);

  /* Take the output buffer */
  outbuf = g_list_nth_data (buffers, buf_idx);
  fail_unless (outbuf != NULL);

  /* Verify size, PTS, duration of the output buffer */
  fail_unless_equals_uint64 (expected_num_out_bytes,
      gst_buffer_get_size (outbuf));
  fail_unless_equals_uint64 (expected_pts, GST_BUFFER_PTS (outbuf));
  fail_unless_equals_uint64 (expected_dur, GST_BUFFER_DURATION (outbuf));

  /* Check that the pixels have the correct values */
  check_test_pattern (ctx, outbuf, xofs, yofs);
}


GST_START_TEST (test_push_unaligned_data_properties_config)
{
  setup_rawvideoparse (FALSE, TRUE, NULL, GST_FORMAT_BYTES);

  /* Send in data buffers that are not aligned to multiples of the
   * frame size (= sample size * num_channels). This tests if rawvideoparse
   * aligns output data properly.
   *
   * The second line sends a buffer with multiple frames inside.
   * rawvideoparse will then parse this buffer repeatedly (and prepend
   * leftover data from the earlier parse iteration), explaining why
   * all of a sudden there are 4 output buffers, compared to just one
   * earlier. The output data is expected to be 280 bytes large, since this
   * is the size of the actual frame, without extra padding at the end.
   */
  push_data_and_check_output (&properties_ctx, 511, 280, GST_MSECOND * 0,
      GST_MSECOND * 40, 1, 0, 0, 0);
  push_data_and_check_output (&properties_ctx, 1940, 280, GST_MSECOND * 40,
      GST_MSECOND * 40, 4, 1, 1, 0);
  push_data_and_check_output (&properties_ctx, 10, 280, GST_MSECOND * 80,
      GST_MSECOND * 40, 4, 2, 2, 0);

  cleanup_rawvideoparse ();
}

GST_END_TEST;

GST_START_TEST (test_push_unaligned_data_sink_caps_config)
{
  GstVideoInfo vinfo;
  GstCaps *caps;

  /* This test is essentially the same as test_push_unaligned_data_properties_config,
   * except that rawvideoparse uses the sink caps config instead of the property config.
   * Also, the input sizes are different, since the sink caps config does not use extra
   * padding between planes and does use a stride that directly corresponds to the width,
   * resulting in smaller frame size (192 bytes vs 280 bytes). */

  gst_video_info_set_format (&vinfo, TEST_FRAME_FORMAT, TEST_WIDTH,
      TEST_HEIGHT);
  GST_VIDEO_INFO_FPS_N (&vinfo) = 25;
  GST_VIDEO_INFO_FPS_D (&vinfo) = 1;
  caps = gst_video_info_to_caps (&vinfo);

  setup_rawvideoparse (TRUE, FALSE, caps, GST_FORMAT_BYTES);

  push_data_and_check_output (&sinkcaps_ctx, 250, 192, GST_MSECOND * 0,
      GST_MSECOND * 40, 1, 0, 0, 0);
  push_data_and_check_output (&sinkcaps_ctx, 811, 192, GST_MSECOND * 40,
      GST_MSECOND * 40, 5, 1, 1, 0);
  push_data_and_check_output (&sinkcaps_ctx, 10, 192, GST_MSECOND * 80,
      GST_MSECOND * 40, 5, 2, 2, 0);

  cleanup_rawvideoparse ();
}

GST_END_TEST;

GST_START_TEST (test_config_switch)
{
  GstVideoInfo vinfo;
  GstCaps *caps;

  /* Start processing with the properties config active, then mid-stream switch to
   * the sink caps config. Since the sink caps config does not use padding, its
   * frame size is smaller. The buffer duration stays the same (since it only depends
   * on the framerate), but the expected output buffer size is different). */

  gst_video_info_set_format (&vinfo, TEST_FRAME_FORMAT, TEST_WIDTH,
      TEST_HEIGHT);
  GST_VIDEO_INFO_FPS_N (&vinfo) = 25;
  GST_VIDEO_INFO_FPS_D (&vinfo) = 1;
  caps = gst_video_info_to_caps (&vinfo);

  setup_rawvideoparse (FALSE, TRUE, caps, GST_FORMAT_BYTES);

  /* Push in data with properties config active */
  push_data_and_check_output (&properties_ctx, 500, 280, GST_MSECOND * 0,
      GST_MSECOND * 40, 1, 0, 0, 0);
  push_data_and_check_output (&properties_ctx, 500, 280, GST_MSECOND * 40,
      GST_MSECOND * 40, 2, 1, 1, 0);

  /* Perform the switch */
  g_object_set (G_OBJECT (rawvideoparse), "use-sink-caps", TRUE, NULL);

  /* Push in data with sink caps config active, expecting a different frame size */
  push_data_and_check_output (&sinkcaps_ctx, 192, 192, GST_MSECOND * 80,
      GST_MSECOND * 40, 3, 2, 0, 0);

  cleanup_rawvideoparse ();
}

GST_END_TEST;

GST_START_TEST (test_push_with_no_framerate)
{
  /* Test the special case when no framerate is set. The parser is expected to
   * still work then, but without setting duration or PTS/DTS (it cannot do that,
   * because these require a nonzero framerate). The output buffers have PTS 0,
   * all subsequent ones have no set PTS. */

  setup_rawvideoparse (FALSE, TRUE, NULL, GST_FORMAT_BYTES);
  g_object_set (G_OBJECT (rawvideoparse), "framerate", 0, 1, NULL);

  push_data_and_check_output (&properties_ctx, 500, 280, 0, GST_CLOCK_TIME_NONE,
      1, 0, 0, 0);
  push_data_and_check_output (&properties_ctx, 500, 280, GST_CLOCK_TIME_NONE,
      GST_CLOCK_TIME_NONE, 2, 1, 1, 0);

  cleanup_rawvideoparse ();
}

GST_END_TEST;

GST_START_TEST (test_computed_plane_strides)
{
  /* Test how plane strides & offsets are (re)computed if custom offsets/strides
   * are disabled, and how they are preserved if they are enabled. */

  GValue plane_offsets_array = G_VALUE_INIT;
  GValue plane_strides_array = G_VALUE_INIT;
  guint i;
  guint const expected_comp_psize = TEST_WIDTH * TEST_HEIGHT;

  g_value_init (&plane_offsets_array, GST_TYPE_ARRAY);
  g_value_init (&plane_strides_array, GST_TYPE_ARRAY);

  setup_rawvideoparse (FALSE, TRUE, NULL, GST_FORMAT_BYTES);

  /* The setup set a custom set of plane offsets and strides together with
   * width=TEST_WIDTH and height=TEST_HEIGHT. Check that the offsets & strides
   * are preserved even after setting new, different width & height values. */

  g_object_set (G_OBJECT (rawvideoparse), "width", TEST_WIDTH * 2,
      "height", TEST_HEIGHT * 2, NULL);

  g_object_get_property (G_OBJECT (rawvideoparse), "plane-offsets",
      &plane_offsets_array);
  g_object_get_property (G_OBJECT (rawvideoparse), "plane-strides",
      &plane_strides_array);

  for (i = 0; i < gst_value_array_get_size (&plane_offsets_array); ++i) {
    const GValue *gvalue;

    /* See setup_rawvideoparse() for how the offsets & strides are defined
     * there. Offsets are set to plane_size*plane_index, and strides are
     * set to the properties_ctx.plane_stride value. */

    gvalue = gst_value_array_get_value (&plane_offsets_array, i);
    fail_unless (gvalue != NULL);
    fail_unless_equals_uint64 (properties_ctx.plane_size * i,
        g_value_get_int (gvalue));

    gvalue = gst_value_array_get_value (&plane_strides_array, i);
    fail_unless (gvalue != NULL);
    fail_unless_equals_uint64 (properties_ctx.plane_stride,
        g_value_get_int (gvalue));
  }

  /* Discard the custom planes&offsets, re-enabling computed values. */
  g_value_reset (&plane_offsets_array);
  g_value_reset (&plane_strides_array);
  g_object_set_property (G_OBJECT (rawvideoparse), "plane-offsets",
      &plane_offsets_array);
  g_object_set_property (G_OBJECT (rawvideoparse), "plane-strides",
      &plane_strides_array);


  /* The strides & offsets should have been recomputed by now. Since the Y444
   * format is used, all strides are the same, and should equal the frame width
   * (which was set to TEST_WIDTH*2 earlier). Plane offsets should be
   * plane_size*plane_index, with plane_size set to (TEST_WIDTH*2 * TEST_HEIGHT*2),
   * or TEST_WIDTH*TEST_HEIGHT*4 (-> expected_comp_psize*4). */

  g_object_get_property (G_OBJECT (rawvideoparse), "plane-offsets",
      &plane_offsets_array);
  g_object_get_property (G_OBJECT (rawvideoparse), "plane-strides",
      &plane_strides_array);

  for (i = 0; i < gst_value_array_get_size (&plane_offsets_array); ++i) {
    const GValue *gvalue;

    gvalue = gst_value_array_get_value (&plane_offsets_array, i);
    fail_unless (gvalue != NULL);
    fail_unless_equals_uint64 (expected_comp_psize * 4 * i,
        g_value_get_int (gvalue));

    gvalue = gst_value_array_get_value (&plane_strides_array, i);
    fail_unless (gvalue != NULL);
    fail_unless_equals_uint64 (TEST_WIDTH * 2, g_value_get_int (gvalue));
  }

  g_value_reset (&plane_offsets_array);
  g_value_reset (&plane_strides_array);


  /* Again change the width & height values. width=TEST_WIDTH, height=TEST_HEIGHT.
   * However, this time, offsets&strides are computed; the current values should
   * not be preserved. Expected plane stride and offset values are similar to
   * above, expect that no multiplications by 2 are present (since the TEST_WIDTH
   * and TEST_HEIGHT values were passed without multiplying them). */

  g_object_set (G_OBJECT (rawvideoparse), "width", TEST_WIDTH,
      "height", TEST_HEIGHT, NULL);


  g_object_get_property (G_OBJECT (rawvideoparse), "plane-offsets",
      &plane_offsets_array);
  g_object_get_property (G_OBJECT (rawvideoparse), "plane-strides",
      &plane_strides_array);

  for (i = 0; i < gst_value_array_get_size (&plane_offsets_array); ++i) {
    const GValue *gvalue;

    gvalue = gst_value_array_get_value (&plane_offsets_array, i);
    fail_unless (gvalue != NULL);
    fail_unless_equals_uint64 (expected_comp_psize * i,
        g_value_get_int (gvalue));

    gvalue = gst_value_array_get_value (&plane_strides_array, i);
    fail_unless (gvalue != NULL);
    fail_unless_equals_uint64 (TEST_WIDTH, g_value_get_int (gvalue));
  }

  g_value_unset (&plane_offsets_array);
  g_value_unset (&plane_strides_array);

  cleanup_rawvideoparse ();
}

GST_END_TEST;

GST_START_TEST (test_change_caps)
{
  GstVideoInfo vinfo;
  GstCaps *caps;

  /* Start processing with the sink caps config active, using the
   * default width/height/format and 25 Hz frame rate for the caps.
   * Push some data, then change caps (25 Hz -> 50 Hz).
   * Check that the changed caps are handled properly. */

  gst_video_info_set_format (&vinfo, TEST_FRAME_FORMAT, TEST_WIDTH,
      TEST_HEIGHT);
  GST_VIDEO_INFO_FPS_N (&vinfo) = 25;
  GST_VIDEO_INFO_FPS_D (&vinfo) = 1;
  caps = gst_video_info_to_caps (&vinfo);

  setup_rawvideoparse (TRUE, FALSE, caps, GST_FORMAT_BYTES);

  /* Push in data with sink config active, expecting duration calculations
   * to be based on the 25 Hz frame rate */
  push_data_and_check_output (&sinkcaps_ctx, 192, 192, GST_MSECOND * 0,
      GST_MSECOND * 40, 1, 0, 0, 0);
  push_data_and_check_output (&sinkcaps_ctx, 192, 192, GST_MSECOND * 40,
      GST_MSECOND * 40, 2, 1, 1, 0);

  /* Change caps */
  GST_VIDEO_INFO_FPS_N (&vinfo) = 50;
  GST_VIDEO_INFO_FPS_D (&vinfo) = 1;
  caps = gst_video_info_to_caps (&vinfo);
  fail_unless (gst_pad_push_event (mysrcpad, gst_event_new_caps (caps)));
  gst_caps_unref (caps);

  /* Push in data with sink config active, expecting duration calculations
   * to be based on the 50 Hz frame rate */
  push_data_and_check_output (&sinkcaps_ctx, 192, 192, GST_MSECOND * 80,
      GST_MSECOND * 20, 3, 2, 2, 0);

  cleanup_rawvideoparse ();
}

GST_END_TEST;

GST_START_TEST (test_incomplete_last_buffer)
{
  GstVideoInfo vinfo;
  GstCaps *caps;

  /* Start processing with the sink caps config active, using the
   * default width/height/format and 25 Hz frame rate for the caps.
   * Push some data, then change caps (25 Hz -> 50 Hz).
   * Check that the changed caps are handled properly. */

  gst_video_info_set_format (&vinfo, TEST_FRAME_FORMAT, TEST_WIDTH,
      TEST_HEIGHT);
  GST_VIDEO_INFO_FPS_N (&vinfo) = 25;
  GST_VIDEO_INFO_FPS_D (&vinfo) = 1;
  caps = gst_video_info_to_caps (&vinfo);

  setup_rawvideoparse (TRUE, FALSE, caps, GST_FORMAT_BYTES);

  push_data_and_check_output (&sinkcaps_ctx, 192, 192, GST_MSECOND * 0,
      GST_MSECOND * 40, 1, 0, 0, 0);
  push_data_and_check_output (&sinkcaps_ctx, 192, 192, GST_MSECOND * 40,
      GST_MSECOND * 40, 2, 1, 1, 0);
  push_data_and_check_output (&sinkcaps_ctx, 100, 192, GST_MSECOND * 40,
      GST_MSECOND * 40, 2, 1, 1, 0);
  gst_pad_push_event (mysrcpad, gst_event_new_eos ());
  fail_unless_equals_int (g_list_length (buffers), 2);

  cleanup_rawvideoparse ();
}

GST_END_TEST;

static Suite *
rawvideoparse_suite (void)
{
  Suite *s = suite_create ("rawvideoparse");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_push_unaligned_data_properties_config);
  tcase_add_test (tc_chain, test_push_unaligned_data_sink_caps_config);
  tcase_add_test (tc_chain, test_config_switch);
  tcase_add_test (tc_chain, test_push_with_no_framerate);
  tcase_add_test (tc_chain, test_computed_plane_strides);
  tcase_add_test (tc_chain, test_change_caps);
  tcase_add_test (tc_chain, test_incomplete_last_buffer);

  return s;
}

GST_CHECK_MAIN (rawvideoparse);
