/* GStreamer
 * Copyright (C) 2019 Seungha Yang <seungha.yang@navercorp.com>
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
#include <gst/check/gstharness.h>
#include <gst/video/video.h>


static void
test_buffer_meta_common (const gchar * in_caps, const gchar * out_caps,
    const gchar * pipeline)
{
  GstHarness *h;
  GstElement *capsfilter;
  GstCaps *caps, *srccaps;
  GstBuffer *in_buf, *out_buf = NULL;
  GstFlowReturn ret;
  GstVideoInfo info;
  GstVideoTimeCodeMeta *meta = NULL;

  h = gst_harness_new_parse (pipeline);
  fail_unless (h != NULL);

  capsfilter = gst_harness_find_element (h, "capsfilter");

  gst_harness_play (h);

  srccaps = gst_caps_from_string (in_caps);
  fail_unless (srccaps != NULL);
  fail_unless (gst_video_info_from_caps (&info, srccaps));

  gst_harness_set_src_caps (h, srccaps);

  /* enforce cuda memory */
  caps = gst_caps_from_string (out_caps);
  g_object_set (capsfilter, "caps", caps, NULL);
  gst_caps_unref (caps);
  gst_object_unref (capsfilter);

  in_buf = gst_buffer_new_and_alloc (GST_VIDEO_INFO_SIZE (&info));
  gst_buffer_memset (in_buf, 0, 0, GST_VIDEO_INFO_SIZE (&info));

  GST_BUFFER_DURATION (in_buf) = GST_SECOND;
  GST_BUFFER_PTS (in_buf) = 0;
  GST_BUFFER_DTS (in_buf) = GST_CLOCK_TIME_NONE;

  gst_buffer_add_video_time_code_meta_full (in_buf, 30, 1,
      NULL, GST_VIDEO_TIME_CODE_FLAGS_NONE, 0, 0, 1, 1, 0);

  ret = gst_harness_push (h, gst_buffer_ref (in_buf));
  fail_unless (ret == GST_FLOW_OK, "GstFlowReturn was %s",
      gst_flow_get_name (ret));

  out_buf = gst_harness_try_pull (h);
  fail_unless (out_buf != NULL, "No output buffer");

  meta = gst_buffer_get_video_time_code_meta (out_buf);
  fail_unless (meta != NULL, "output buffer has no meta");
  fail_unless_equals_int (meta->tc.config.fps_n, 30);
  fail_unless_equals_int (meta->tc.config.fps_d, 1);
  fail_unless_equals_int (meta->tc.seconds, 1);

  gst_buffer_unref (in_buf);
  gst_buffer_unref (out_buf);

  gst_harness_teardown (h);
}

GST_START_TEST (test_buffer_meta)
{
  /* test whether buffer meta would be preserved or not */

  test_buffer_meta_common
      ("video/x-raw,format=(string)NV12,width=340,height=240",
      "video/x-raw(memory:CUDAMemory)", "cudaupload ! capsfilter");
  test_buffer_meta_common
      ("video/x-raw,format=(string)NV12,width=340,height=240", "video/x-raw",
      "cudaupload ! cudadownload ! capsfilter");
  test_buffer_meta_common
      ("video/x-raw,format=(string)NV12,width=340,height=240",
      "video/x-raw,format=(string)I420,width=340,height=240",
      "cudaupload ! cudaconvert ! cudadownload ! capsfilter");
  test_buffer_meta_common
      ("video/x-raw,format=(string)NV12,width=340,height=240",
      "video/x-raw,format=(string)NV12,width=640,height=480",
      "cudaupload ! cudaconvert ! cudascale ! cudaconvert ! cudadownload ! capsfilter");
}

GST_END_TEST;

static gboolean
check_cuda_available (void)
{
  GstElement *elem;

  elem = gst_element_factory_make ("cudaupload", NULL);
  if (!elem) {
    GST_WARNING ("cudaupload is not available, possibly driver load failure");
    return FALSE;
  }
  gst_object_unref (elem);

  elem = gst_element_factory_make ("cudadownload", NULL);
  if (!elem) {
    GST_WARNING ("cudadownload is not available, possibly driver load failure");
    return FALSE;
  }
  gst_object_unref (elem);

  elem = gst_element_factory_make ("cudaconvert", NULL);
  if (!elem) {
    GST_WARNING ("cudaconvert is not available, possibly driver load failure");
    return FALSE;
  }
  gst_object_unref (elem);

  elem = gst_element_factory_make ("cudascale", NULL);
  if (!elem) {
    GST_WARNING ("cudascale is not available, possibly driver load failure");
    return FALSE;
  }
  gst_object_unref (elem);

  return TRUE;
}

static Suite *
cudafilter_suite (void)
{
  Suite *s;
  TCase *tc_chain;

  /* HACK: cuda device init/deinit with fork seems to problematic */
  g_setenv ("CK_FORK", "no", TRUE);

  s = suite_create ("cudafilter");
  tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);

  if (!check_cuda_available ()) {
    GST_DEBUG ("Skip cuda filter test since cannot open device");
    goto end;
  }

  tcase_add_test (tc_chain, test_buffer_meta);

end:
  return s;
}

GST_CHECK_MAIN (cudafilter);
