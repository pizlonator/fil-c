/* GStreamer
 * Copyright (C) <2018> Marc Leeman <marc.leeman@gmail.com>
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

GST_START_TEST (test_uri_to_properties)
{
  GstElement *rtpsrc;
  guint latency, ttl, ttl_mc;

  rtpsrc = gst_element_factory_make ("rtpsrc", NULL);

  /* Sets properties to non-default values (make sure this stays in sync) */
  g_object_set (rtpsrc, "uri", "rtp://1.230.1.2:1234?"
      "latency=300" "&ttl=8" "&ttl-mc=9" "&multicast-iface=dummy", NULL);

  g_object_get (rtpsrc,
      "latency", &latency, "ttl-mc", &ttl_mc, "ttl", &ttl, NULL);

  /* Make sure these values are in sync with the one from the URI. */
  g_assert_cmpuint (latency, ==, 300);
  g_assert_cmpint (ttl, ==, 8);
  g_assert_cmpint (ttl_mc, ==, 9);

  gst_object_unref (rtpsrc);
}

GST_END_TEST;

static Suite *
rtpsrc_suite (void)
{
  Suite *s = suite_create ("rtpsrc");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_uri_to_properties);

  return s;
}

GST_CHECK_MAIN (rtpsrc);
