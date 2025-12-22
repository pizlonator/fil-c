/* GStreamer
 *
 * unit test for VA allocators
 *
 * Copyright (C) 2021 Igalia, S.L.
 *     Author: Víctor Jáquez <vjaquez@igalia.com>
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
#  include "config.h"
#endif

#include <gst/gst.h>
#include <gst/check/gstcheck.h>
#include <gst/check/gstharness.h>
#include <gst/video/video.h>

GST_START_TEST (raw_copy)
{
  GstHarness *h;
  GstBuffer *buf, *buf_copy;
  gboolean ret;

  h = gst_harness_new_parse ("videotestsrc num-buffers=1 ! "
      "video/x-raw, width=(int)1024, height=(int)768 ! vapostproc");
  ck_assert (h);

  gst_harness_set_sink_caps_str (h,
      "video/x-raw, format=(string)NV12, width=(int)3840, height=(int)2160");

  gst_harness_add_propose_allocation_meta (h, GST_VIDEO_META_API_TYPE, NULL);
  gst_harness_play (h);

  buf = gst_harness_pull (h);
  ck_assert (buf);

  buf_copy = gst_buffer_new ();
  ret = gst_buffer_copy_into (buf_copy, buf,
      GST_BUFFER_COPY_MEMORY | GST_BUFFER_COPY_DEEP, 0, -1);
  ck_assert (ret);

  gst_clear_buffer (&buf);
  gst_clear_buffer (&buf_copy);

  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (dmabuf_copy)
{
  GstHarness *h;
  GstBuffer *buf, *buf_copy;
  gboolean ret;

  h = gst_harness_new_parse ("videotestsrc num-buffers=1 ! "
      "video/x-raw, width=(int)1024, height=(int)768 ! vapostproc");
  ck_assert (h);

  gst_harness_set_sink_caps_str (h,
      "video/x-raw(memory:DMABuf), format=(string)NV12, width=(int)3840, height=(int)2160");

  gst_harness_add_propose_allocation_meta (h, GST_VIDEO_META_API_TYPE, NULL);
  gst_harness_play (h);

  buf = gst_harness_pull (h);
  ck_assert (buf);

  buf_copy = gst_buffer_new ();
  ret = gst_buffer_copy_into (buf_copy, buf,
      GST_BUFFER_COPY_MEMORY | GST_BUFFER_COPY_DEEP, 0, -1);

  if (gst_buffer_n_memory (buf_copy) == 1)
    ck_assert (ret == TRUE);
  /* else it will depend on the drm modifier */

  gst_clear_buffer (&buf);
  gst_clear_buffer (&buf_copy);

  gst_harness_teardown (h);
}

GST_END_TEST;

int
main (int argc, char **argv)
{
  GstElement *vpp;
  Suite *s;
  TCase *tc_chain;

  gst_check_init (&argc, &argv);

  vpp = gst_element_factory_make ("vapostproc", NULL);
  if (!vpp)
    return EXIT_SUCCESS;        /* not available vapostproc */
  gst_object_unref (vpp);

  s = suite_create ("va");
  tc_chain = tcase_create ("copy");

  suite_add_tcase (s, tc_chain);

  tcase_add_test (tc_chain, raw_copy);
  tcase_add_test (tc_chain, dmabuf_copy);

  return gst_check_run_suite (s, "va", __FILE__);
}
