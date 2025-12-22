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

GST_START_TEST (test_properties)
{
  GstElement *element;
  const gchar *ifname = "enp1s0";
  const gchar *address = "01:AA:BB:CC:DD:EE";
  const gint priority = 3;
  gchar *str;
  gint val;

  element = gst_check_setup_element ("avtpsink");

  g_object_set (G_OBJECT (element), "ifname", ifname, NULL);
  g_object_get (G_OBJECT (element), "ifname", &str, NULL);
  fail_unless_equals_string (str, ifname);
  g_free (str);

  g_object_set (G_OBJECT (element), "address", address, NULL);
  g_object_get (G_OBJECT (element), "address", &str, NULL);
  fail_unless_equals_string (str, address);
  g_free (str);

  g_object_set (G_OBJECT (element), "priority", priority, NULL);
  g_object_get (G_OBJECT (element), "priority", &val, NULL);
  fail_unless (val, priority);

  gst_check_teardown_element (element);
}

GST_END_TEST;

static Suite *
avtpsink_suite (void)
{
  Suite *s = suite_create ("avtpsink");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_properties);

  return s;
}

GST_CHECK_MAIN (avtpsink);
