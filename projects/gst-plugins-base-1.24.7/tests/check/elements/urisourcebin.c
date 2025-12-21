/* GStreamer unit tests for playsink
 * Copyright (C) 2015 Matthew Waters <matthew@centricular.com>
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
# include <config.h>
#endif

#include <gst/check/gstcheck.h>


GST_START_TEST (test_initial_statistics)
{
  GstElement *urisrc;
  GstStructure *stats = NULL;
  guint min_bytes, max_bytes, avg_bytes;
  guint64 min_time, max_time, avg_time;

  urisrc = gst_element_factory_make ("urisourcebin", NULL);
  fail_unless (urisrc != NULL);

  g_object_get (urisrc, "statistics", &stats, NULL);

  fail_unless (stats != NULL);
  fail_unless (g_strcmp0 (gst_structure_get_name (stats),
          "application/x-urisourcebin-stats") == 0);
  fail_unless_equals_int (6, gst_structure_n_fields (stats));

  fail_unless_equals_int (TRUE, gst_structure_get_uint (stats,
          "minimum-byte-level", &min_bytes));
  fail_unless_equals_int (0, min_bytes);
  fail_unless_equals_int (TRUE, gst_structure_get_uint (stats,
          "maximum-byte-level", &max_bytes));
  fail_unless_equals_int (0, max_bytes);
  fail_unless_equals_int (TRUE, gst_structure_get_uint (stats,
          "average-byte-level", &avg_bytes));
  fail_unless_equals_int (0, avg_bytes);

  fail_unless_equals_int (TRUE, gst_structure_get_uint64 (stats,
          "minimum-time-level", &min_time));
  fail_unless_equals_int (0, min_time);
  fail_unless_equals_int (TRUE, gst_structure_get_uint64 (stats,
          "maximum-time-level", &max_time));
  fail_unless_equals_int (0, max_time);
  fail_unless_equals_int (TRUE, gst_structure_get_uint64 (stats,
          "average-time-level", &avg_time));
  fail_unless_equals_int (0, avg_time);

  gst_structure_free (stats);
  gst_object_unref (urisrc);
}

GST_END_TEST;

GST_START_TEST (test_get_set_watermark)
{
  GstElement *urisrc;
  gdouble watermark;

  urisrc = gst_element_factory_make ("urisourcebin", NULL);
  fail_unless (urisrc != NULL);

  g_object_set (urisrc, "low-watermark", 0.2, "high-watermark", 0.8, NULL);
  g_object_get (urisrc, "low-watermark", &watermark, NULL);
  fail_unless_equals_float (watermark, 0.2);
  g_object_get (urisrc, "high-watermark", &watermark, NULL);
  fail_unless_equals_float (watermark, 0.8);

  gst_object_unref (urisrc);
}

GST_END_TEST;

static Suite *
urisourcebin_suite (void)
{
  Suite *s = suite_create ("urisourcebin");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);

  tcase_add_test (tc_chain, test_initial_statistics);
  tcase_add_test (tc_chain, test_get_set_watermark);

  return s;
}

GST_CHECK_MAIN (urisourcebin);
