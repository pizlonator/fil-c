/* GStreamer
 *
 * unit test for clock selection
 *
 * Copyright (C) <2005> Wim Taymans <wim at fluendo dot com>
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

GST_START_TEST (test_add)
{
  GstElement *pipeline;
  GstStateChangeReturn ret;

  pipeline = gst_pipeline_new ("pipeline");
  fail_unless (pipeline != NULL, "could not create pipeline");

  ret = gst_element_set_state (pipeline, GST_STATE_READY);
  fail_unless (ret == GST_STATE_CHANGE_SUCCESS, "could not set to READY");

  /* cleanup */
  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (pipeline);
}

GST_END_TEST;

static Suite *
clocks_suite (void)
{
  Suite *s = suite_create ("clocks");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_add);

  return s;
}

GST_CHECK_MAIN (clocks);
