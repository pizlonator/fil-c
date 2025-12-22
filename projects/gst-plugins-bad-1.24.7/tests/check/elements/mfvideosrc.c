/* GStreamer
 *
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

#include <gst/gst.h>
#include <gst/check/gstcheck.h>

GST_START_TEST (test_mf_video_src_reuse)
{
  GstElement *pipeline;
  GstStateChangeReturn ret;
  GstBus *bus;
  GstMessage *msg;

  pipeline = gst_parse_launch ("mfvideosrc ! fakevideosink name=sink", NULL);
  fail_unless (pipeline != NULL);

  bus = gst_element_get_bus (GST_ELEMENT (pipeline));
  fail_unless (bus != NULL);

  GST_INFO ("Set state playing");
  ret = gst_element_set_state (pipeline, GST_STATE_PLAYING);
  msg = gst_bus_poll (bus, GST_MESSAGE_ASYNC_DONE | GST_MESSAGE_ERROR, -1);
  fail_unless (GST_MESSAGE_TYPE (msg) == GST_MESSAGE_ASYNC_DONE);
  gst_message_unref (msg);

  GST_INFO ("Set state ready");
  ret = gst_element_set_state (pipeline, GST_STATE_READY);
  fail_unless (ret == GST_STATE_CHANGE_SUCCESS);

  GST_INFO ("Set state playing again");
  ret = gst_element_set_state (pipeline, GST_STATE_PLAYING);
  msg = gst_bus_poll (bus, GST_MESSAGE_ASYNC_DONE | GST_MESSAGE_ERROR, -1);
  fail_unless (GST_MESSAGE_TYPE (msg) == GST_MESSAGE_ASYNC_DONE);
  gst_message_unref (msg);

  ret = gst_element_set_state (pipeline, GST_STATE_NULL);
  fail_unless (ret == GST_STATE_CHANGE_SUCCESS);

  gst_object_unref (bus);
  gst_object_unref (pipeline);
}

GST_END_TEST;

static gboolean
check_mf_available (void)
{
  gboolean ret = TRUE;
  GstElement *mfvideosrc;

  mfvideosrc = gst_element_factory_make ("mfvideosrc", NULL);
  if (!mfvideosrc) {
    GST_INFO ("nvh264dec is not available");
    return FALSE;
  }

  /* GST_STATE_READY is meaning that camera is available */
  if (gst_element_set_state (mfvideosrc,
          GST_STATE_READY) != GST_STATE_CHANGE_SUCCESS) {
    GST_INFO ("cannot open device");
    ret = FALSE;
  }

  gst_element_set_state (mfvideosrc, GST_STATE_NULL);
  gst_object_unref (mfvideosrc);

  return ret;
}

static Suite *
mfvideosrc_suite (void)
{
  Suite *s = suite_create ("mfvideosrc");
  TCase *tc_basic = tcase_create ("general");
  gboolean have_mf = FALSE;

  suite_add_tcase (s, tc_basic);

  have_mf = check_mf_available ();

  if (have_mf) {
    tcase_add_test (tc_basic, test_mf_video_src_reuse);
  } else {
    GST_INFO ("Skipping tests, media foundation plugin is unavailable");
  }

  return s;
}

GST_CHECK_MAIN (mfvideosrc);
