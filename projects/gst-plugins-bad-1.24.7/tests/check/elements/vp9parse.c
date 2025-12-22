/* GStreamer
 * Copyright (C) 2020 Seungha Yang <seungha@centricular.com>
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
#include "vp9parse.h"
#include <string.h>

typedef struct
{
  const guint8 *data;
  guint len;
  gboolean superframe;
  guint subframe_len[2];
} GstVp9ParseTestFrameData;

static void
run_split_superframe_with_caps (const gchar * in_caps)
{
  GstHarness *h;
  GstBuffer *in_buf, *out_buf = NULL;
  GstMapInfo map;
  GstFlowReturn ret;
  gint i = 0;
  GstVp9ParseTestFrameData frames[] = {
    {profile_0_frame0, profile_0_frame0_len, FALSE, {profile_0_frame0_len, 0}},
    {profile_0_frame1, profile_0_frame1_len, TRUE, {profile_0_frame1_first_len,
            profile_0_frame1_last_len}},
    {profile_0_frame2, profile_0_frame2_len, FALSE, {profile_0_frame2_len, 0}},
  };

  h = gst_harness_new_parse ("vp9parse");
  fail_unless (h != NULL);

  gst_harness_set_sink_caps_str (h, "video/x-vp9,alignment=(string)frame");

  /* default alignment is super-frame */
  gst_harness_set_src_caps_str (h, in_caps);

  gst_harness_play (h);
  for (i = 0; i < G_N_ELEMENTS (frames); i++) {
    in_buf = gst_buffer_new_and_alloc (frames[i].len);
    gst_buffer_map (in_buf, &map, GST_MAP_WRITE);
    memcpy (map.data, frames[i].data, frames[i].len);
    gst_buffer_unmap (in_buf, &map);

    ret = gst_harness_push (h, in_buf);
    fail_unless (ret == GST_FLOW_OK, "GstFlowReturn was %s",
        gst_flow_get_name (ret));
    out_buf = gst_harness_try_pull (h);
    fail_unless (out_buf);
    fail_unless_equals_int (gst_buffer_get_size (out_buf),
        frames[i].subframe_len[0]);

    if (i == 0) {
      GstEvent *event;
      GstCaps *caps = NULL;
      GstStructure *s;
      const gchar *profile;
      gint width, height;

      fail_if (GST_BUFFER_FLAG_IS_SET (out_buf, GST_BUFFER_FLAG_DELTA_UNIT));

      while ((event = gst_harness_try_pull_event (h))) {
        GstCaps *event_caps;
        if (GST_EVENT_TYPE (event) != GST_EVENT_CAPS) {
          gst_event_unref (event);
          continue;
        }

        gst_event_parse_caps (event, &event_caps);
        gst_caps_replace (&caps, event_caps);
        gst_event_unref (event);
      }

      fail_unless (caps != NULL);
      s = gst_caps_get_structure (caps, 0);
      fail_unless (gst_structure_get_int (s, "width", &width));
      fail_unless (gst_structure_get_int (s, "height", &height));
      fail_unless ((profile = gst_structure_get_string (s, "profile")));

      fail_unless_equals_int (width, 256);
      fail_unless_equals_int (height, 144);
      fail_unless_equals_string (profile, "0");
      gst_caps_unref (caps);
    } else {
      fail_unless (GST_BUFFER_FLAG_IS_SET (out_buf,
              GST_BUFFER_FLAG_DELTA_UNIT));
    }

    if (frames[i].superframe) {
      /* this is decoding only frame */
      fail_unless (GST_BUFFER_FLAG_IS_SET (out_buf,
              GST_BUFFER_FLAG_DECODE_ONLY));
      fail_unless (GST_BUFFER_FLAG_IS_SET (out_buf,
              GST_BUFFER_FLAG_DELTA_UNIT));
      gst_clear_buffer (&out_buf);

      out_buf = gst_harness_try_pull (h);
      fail_unless (out_buf);
      fail_unless_equals_int (gst_buffer_get_size (out_buf),
          frames[i].subframe_len[1]);
      fail_unless (GST_BUFFER_FLAG_IS_SET (out_buf,
              GST_BUFFER_FLAG_DELTA_UNIT));
    }

    gst_clear_buffer (&out_buf);
  }

  gst_harness_teardown (h);
}

GST_START_TEST (test_split_superframe)
{
  /* vp9parse will split frame if downstream alignment is frame
   * whatever the upstream alignment was specified */
  run_split_superframe_with_caps ("video/x-vp9");
  run_split_superframe_with_caps ("video/x-vp9,alignment=(string)super-frame");
  run_split_superframe_with_caps ("video/x-vp9,alignment=(string)frame");
}

GST_END_TEST;

static Suite *
vp9parse_suite (void)
{
  Suite *s;
  TCase *tc_chain;

  s = suite_create ("vp9parse");
  tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);

  tcase_add_test (tc_chain, test_split_superframe);

  return s;
}

GST_CHECK_MAIN (vp9parse);
