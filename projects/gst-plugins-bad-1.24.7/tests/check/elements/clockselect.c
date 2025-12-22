/*
 * GStreamer AVTP Plugin
 * Copyright (C) 2019 Intel Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later
 * version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA
 */

#include <gst/check/gstcheck.h>
#include <gst/check/gstharness.h>

GST_START_TEST (test_clock_select_tai_clock)
{
  GstHarness *h;
  GstElement *element;
  GstClock *clock;
  guint clock_type;

  h = gst_harness_new_parse ("clockselect clock-id=tai");

  /* Check if element provides right clock */
  element = gst_harness_find_element (h, "clockselect");
  clock = gst_element_provide_clock (element);

  fail_unless (GST_IS_SYSTEM_CLOCK (clock));
  g_object_get (G_OBJECT (clock), "clock-type", &clock_type, NULL);
  fail_unless_equals_uint64 (clock_type, GST_CLOCK_TYPE_TAI);

  gst_object_unref (element);
  gst_object_unref (clock);
  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_clock_select_realtime_clock)
{
  GstHarness *h;
  GstElement *element;
  GstClock *clock;
  guint clock_type;

  h = gst_harness_new_parse ("clockselect clock-id=realtime");

  /* Check if element provides right clock */
  element = gst_harness_find_element (h, "clockselect");
  clock = gst_element_provide_clock (element);

  fail_unless (GST_IS_SYSTEM_CLOCK (clock));
  g_object_get (G_OBJECT (clock), "clock-type", &clock_type, NULL);
  fail_unless_equals_uint64 (clock_type, GST_CLOCK_TYPE_REALTIME);

  gst_object_unref (element);
  gst_object_unref (clock);
  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_clock_select_monotonic_clock)
{
  GstHarness *h;
  GstElement *element;
  GstClock *clock;
  guint clock_type;

  h = gst_harness_new_parse ("clockselect clock-id=monotonic");

  /* Check if element provides right clock */
  element = gst_harness_find_element (h, "clockselect");
  clock = gst_element_provide_clock (element);

  fail_unless (GST_IS_SYSTEM_CLOCK (clock));
  g_object_get (G_OBJECT (clock), "clock-type", &clock_type, NULL);
  fail_unless_equals_uint64 (clock_type, GST_CLOCK_TYPE_MONOTONIC);

  gst_object_unref (element);
  gst_object_unref (clock);
  gst_harness_teardown (h);
}

GST_END_TEST;

GST_START_TEST (test_clock_select_properties)
{
  GstHarness *h;
  GstElement *element;
  guint clock_id, domain;

  h = gst_harness_new_parse ("clockselect clock-id=ptp ptp-domain=2");

  /* Check if all properties were properly set up */
  element = gst_harness_find_element (h, "clockselect");
  g_object_get (G_OBJECT (element), "clock-id", &clock_id, NULL);
  fail_unless_equals_uint64 (clock_id, 3);

  g_object_get (G_OBJECT (element), "ptp-domain", &domain, NULL);
  fail_unless_equals_uint64 (domain, 2);

  gst_object_unref (element);
  gst_harness_teardown (h);
}

GST_END_TEST;

static Suite *
clock_select_suite (void)
{
  Suite *s = suite_create ("clockselect");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_clock_select_properties);
  tcase_add_test (tc_chain, test_clock_select_monotonic_clock);
  tcase_add_test (tc_chain, test_clock_select_realtime_clock);
  tcase_add_test (tc_chain, test_clock_select_tai_clock);

  return s;
}

GST_CHECK_MAIN (clock_select);
