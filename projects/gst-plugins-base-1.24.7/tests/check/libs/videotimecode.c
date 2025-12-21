/* GStreamer
 *
 * Copyright (C) 2016 Vivia Nikolaidou <vivia@toolsonair.com>
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
#include <gst/video/video.h>


GST_START_TEST (videotimecode_compare_equal)
{
  GstVideoTimeCode *tc1, *tc2;

  tc2 = gst_video_time_code_new (50, 1, NULL, 0, 10, 10, 10, 10, 0);
  tc1 = gst_video_time_code_new (50, 1, NULL, 0, 10, 10, 10, 10, 0);
  fail_unless (gst_video_time_code_compare (tc1, tc2) == 0);
  gst_video_time_code_free (tc1);
  gst_video_time_code_free (tc2);
}

GST_END_TEST;

GST_START_TEST (videotimecode_compare_fps_n)
{
  GstVideoTimeCode *tc1, *tc2;

  tc1 = gst_video_time_code_new (50, 1, NULL, 0, 10, 10, 10, 10, 0);
  tc2 = gst_video_time_code_new (25, 1, NULL, 0, 10, 10, 10, 10, 0);
  fail_unless (gst_video_time_code_compare (tc1, tc2) == 1);
  fail_unless (gst_video_time_code_compare (tc2, tc1) == -1);
  gst_video_time_code_free (tc1);
  gst_video_time_code_free (tc2);
}

GST_END_TEST;

GST_START_TEST (videotimecode_compare_fps_d)
{
  GstVideoTimeCode *tc1, *tc2;

  tc1 = gst_video_time_code_new (50, 1, NULL, 0, 10, 10, 10, 10, 0);
  tc2 = gst_video_time_code_new (50, 2, NULL, 0, 10, 10, 10, 10, 0);
  fail_unless (gst_video_time_code_compare (tc1, tc2) == 1);
  fail_unless (gst_video_time_code_compare (tc2, tc1) == -1);
  gst_video_time_code_free (tc1);
  gst_video_time_code_free (tc2);
}

GST_END_TEST;

GST_START_TEST (videotimecode_compare_frames)
{
  GstVideoTimeCode *tc1, *tc2;

  tc1 = gst_video_time_code_new (50, 1, NULL, 0, 10, 10, 10, 10, 0);
  tc2 = gst_video_time_code_new (50, 1, NULL, 0, 10, 10, 10, 9, 0);
  fail_unless (gst_video_time_code_compare (tc1, tc2) == 1);
  fail_unless (gst_video_time_code_compare (tc2, tc1) == -1);
  gst_video_time_code_free (tc1);
  gst_video_time_code_free (tc2);
}

GST_END_TEST;

GST_START_TEST (videotimecode_compare_seconds)
{
  GstVideoTimeCode *tc1, *tc2;

  tc1 = gst_video_time_code_new (50, 1, NULL, 0, 10, 10, 10, 10, 0);
  tc2 = gst_video_time_code_new (50, 1, NULL, 0, 10, 10, 9, 10, 0);
  fail_unless (gst_video_time_code_compare (tc1, tc2) == 1);
  fail_unless (gst_video_time_code_compare (tc2, tc1) == -1);
  gst_video_time_code_free (tc1);
  gst_video_time_code_free (tc2);
}

GST_END_TEST;

GST_START_TEST (videotimecode_compare_minutes)
{
  GstVideoTimeCode *tc1, *tc2;

  tc1 = gst_video_time_code_new (50, 1, NULL, 0, 10, 10, 10, 10, 0);
  tc2 = gst_video_time_code_new (50, 1, NULL, 0, 10, 9, 10, 10, 0);
  fail_unless (gst_video_time_code_compare (tc1, tc2) == 1);
  fail_unless (gst_video_time_code_compare (tc2, tc1) == -1);
  gst_video_time_code_free (tc1);
  gst_video_time_code_free (tc2);
}

GST_END_TEST;

GST_START_TEST (videotimecode_compare_hours)
{
  GstVideoTimeCode *tc1, *tc2;

  tc1 = gst_video_time_code_new (50, 1, NULL, 0, 10, 10, 10, 10, 0);
  tc2 = gst_video_time_code_new (50, 1, NULL, 0, 9, 10, 10, 10, 0);
  fail_unless (gst_video_time_code_compare (tc1, tc2) == 1);
  fail_unless (gst_video_time_code_compare (tc2, tc1) == -1);
  gst_video_time_code_free (tc1);
  gst_video_time_code_free (tc2);
}

GST_END_TEST;

GST_START_TEST (videotimecode_compare_fieldcounts)
{
  GstVideoTimeCode *tc1, *tc2;

  tc1 =
      gst_video_time_code_new (50, 1, NULL,
      GST_VIDEO_TIME_CODE_FLAGS_INTERLACED, 10, 10, 10, 10, 2);
  tc2 =
      gst_video_time_code_new (50, 1, NULL,
      GST_VIDEO_TIME_CODE_FLAGS_INTERLACED, 10, 10, 10, 10, 1);
  fail_unless (gst_video_time_code_compare (tc1, tc2) == 1);
  fail_unless (gst_video_time_code_compare (tc2, tc1) == -1);
  gst_video_time_code_free (tc1);
  gst_video_time_code_free (tc2);
}

GST_END_TEST;

GST_START_TEST (videotimecode_addframe_10)
{
  GstVideoTimeCode *tc1;

  tc1 = gst_video_time_code_new (50, 1, NULL, 0, 10, 10, 10, 10, 0);
  gst_video_time_code_increment_frame (tc1);
  fail_unless (tc1->hours == 10);
  fail_unless (tc1->minutes == 10);
  fail_unless (tc1->seconds == 10);
  fail_unless (tc1->frames == 11);
  gst_video_time_code_free (tc1);
}

GST_END_TEST;

GST_START_TEST (videotimecode_addframe_0)
{
  GstVideoTimeCode *tc1;

  tc1 = gst_video_time_code_new (50, 1, NULL, 0, 0, 0, 0, 0, 0);
  gst_video_time_code_increment_frame (tc1);
  fail_unless (tc1->hours == 0);
  fail_unless (tc1->minutes == 0);
  fail_unless (tc1->seconds == 0);
  fail_unless (tc1->frames == 1);
  gst_video_time_code_free (tc1);
}

GST_END_TEST;

GST_START_TEST (videotimecode_addframe_high)
{
  GstVideoTimeCode *tc1;
  /* Make sure nothing overflows */

  tc1 = gst_video_time_code_new (60, 1, NULL, 0, 23, 59, 59, 58, 0);
  gst_video_time_code_increment_frame (tc1);
  fail_unless (tc1->hours == 23);
  fail_unless (tc1->minutes == 59);
  fail_unless (tc1->seconds == 59);
  fail_unless (tc1->frames == 59);
  gst_video_time_code_free (tc1);
}

GST_END_TEST;

GST_START_TEST (videotimecode_addframe_dropframe)
{
  GstVideoTimeCode *tc1;

  tc1 =
      gst_video_time_code_new (30000, 1001, NULL,
      GST_VIDEO_TIME_CODE_FLAGS_DROP_FRAME, 10, 10, 10, 10, 0);
  gst_video_time_code_increment_frame (tc1);
  fail_unless (tc1->hours == 10);
  fail_unless (tc1->minutes == 10);
  fail_unless (tc1->seconds == 10);
  fail_unless (tc1->frames == 11);
  gst_video_time_code_free (tc1);
}

GST_END_TEST;

GST_START_TEST (videotimecode_addframe_framedropped)
{
  GstVideoTimeCode *tc1;

  tc1 =
      gst_video_time_code_new (30000, 1001, NULL,
      GST_VIDEO_TIME_CODE_FLAGS_DROP_FRAME, 10, 10, 59, 29, 0);
  gst_video_time_code_increment_frame (tc1);
  fail_unless (tc1->hours == 10);
  fail_unless (tc1->minutes == 11);
  fail_unless (tc1->seconds == 0);
  fail_unless (tc1->frames == 2);
  gst_video_time_code_free (tc1);
}

GST_END_TEST;

GST_START_TEST (videotimecode_addframe_wrapover)
{
  GstVideoTimeCode *tc1;

  tc1 =
      gst_video_time_code_new (30000, 1001, NULL,
      GST_VIDEO_TIME_CODE_FLAGS_DROP_FRAME, 23, 59, 59, 29, 0);
  gst_video_time_code_increment_frame (tc1);
  fail_unless (tc1->hours == 0);
  fail_unless (tc1->minutes == 0);
  fail_unless (tc1->seconds == 0);
  fail_unless (tc1->frames == 0);
  gst_video_time_code_free (tc1);
}

GST_END_TEST;

GST_START_TEST (videotimecode_addframe_60drop_dropframe)
{
  GstVideoTimeCode *tc1;

  tc1 =
      gst_video_time_code_new (60000, 1001, NULL,
      GST_VIDEO_TIME_CODE_FLAGS_DROP_FRAME, 10, 10, 10, 10, 0);
  gst_video_time_code_increment_frame (tc1);
  fail_unless (tc1->hours == 10);
  fail_unless (tc1->minutes == 10);
  fail_unless (tc1->seconds == 10);
  fail_unless (tc1->frames == 11);
  gst_video_time_code_free (tc1);
}

GST_END_TEST;

GST_START_TEST (videotimecode_addframe_60drop_framedropped)
{
  GstVideoTimeCode *tc1;

  tc1 =
      gst_video_time_code_new (60000, 1001, NULL,
      GST_VIDEO_TIME_CODE_FLAGS_DROP_FRAME, 10, 10, 59, 59, 0);
  gst_video_time_code_increment_frame (tc1);
  fail_unless (tc1->hours == 10);
  fail_unless (tc1->minutes == 11);
  fail_unless (tc1->seconds == 0);
  fail_unless (tc1->frames == 4);
  gst_video_time_code_free (tc1);
}

GST_END_TEST;

GST_START_TEST (videotimecode_addframe_60drop_wrapover)
{
  GstVideoTimeCode *tc1;
  /* Make sure nothing overflows here either */

  tc1 =
      gst_video_time_code_new (60000, 1001, NULL,
      GST_VIDEO_TIME_CODE_FLAGS_DROP_FRAME, 23, 59, 59, 59, 0);
  gst_video_time_code_increment_frame (tc1);
  fail_unless (tc1->hours == 0);
  fail_unless (tc1->minutes == 0);
  fail_unless (tc1->seconds == 0);
  fail_unless (tc1->frames == 0);
  gst_video_time_code_free (tc1);
}

GST_END_TEST;

GST_START_TEST (videotimecode_addframe_loop)
{
  GstVideoTimeCode *tc1;
  guint64 i;
  /* Loop for an hour and a bit, make sure no assertions explode */

  tc1 =
      gst_video_time_code_new (60000, 1001, NULL,
      GST_VIDEO_TIME_CODE_FLAGS_DROP_FRAME, 12, 12, 12, 12, 0);
  for (i = 0; i < 220000; i++)
    gst_video_time_code_increment_frame (tc1);
  gst_video_time_code_init (tc1, 60, 1, NULL, 0, 12, 12, 12, 12, 0);
  for (i = 0; i < 220000; i++)
    gst_video_time_code_increment_frame (tc1);
  gst_video_time_code_free (tc1);
}

GST_END_TEST;

GST_START_TEST (videotimecode_dailyjam_todatetime)
{
  GstVideoTimeCode *tc1;
  GDateTime *dt1, *dt2;

  dt1 = g_date_time_new_utc (2016, 7, 29, 10, 32, 50);

  tc1 =
      gst_video_time_code_new (50, 1, dt1,
      GST_VIDEO_TIME_CODE_FLAGS_NONE, 0, 0, 0, 0, 0);
  /* 1 hour, 4 minutes, 3 seconds, and 2 frames */
  gst_video_time_code_add_frames (tc1, 192152);
  fail_unless (tc1->hours == 1);
  fail_unless (tc1->minutes == 4);
  fail_unless (tc1->seconds == 3);
  fail_unless (tc1->frames == 2);

  dt2 = gst_video_time_code_to_date_time (tc1);
  fail_unless (g_date_time_get_year (dt2) == 2016);
  fail_unless (g_date_time_get_month (dt2) == 7);
  fail_unless (g_date_time_get_day_of_month (dt2) == 29);
  fail_unless (g_date_time_get_hour (dt2) == 11);
  fail_unless (g_date_time_get_minute (dt2) == 36);
  fail_unless_equals_float (g_date_time_get_seconds (dt2), 53.04);

  gst_video_time_code_free (tc1);
  g_date_time_unref (dt2);
  g_date_time_unref (dt1);
}

GST_END_TEST;

GST_START_TEST (videotimecode_dailyjam_compare)
{
  GstVideoTimeCode *tc1, *tc2;
  GDateTime *dt1;

  dt1 = g_date_time_new_utc (2016, 7, 29, 10, 32, 50);

  tc1 =
      gst_video_time_code_new (50, 1, dt1,
      GST_VIDEO_TIME_CODE_FLAGS_NONE, 0, 0, 0, 0, 0);
  tc2 = gst_video_time_code_copy (tc1);
  fail_unless (gst_video_time_code_compare (tc1, tc2) == 0);
  gst_video_time_code_increment_frame (tc1);
  fail_unless (gst_video_time_code_compare (tc1, tc2) == 1);
  gst_video_time_code_add_frames (tc2, 2);
  fail_unless (gst_video_time_code_compare (tc1, tc2) == -1);

  gst_video_time_code_free (tc1);
  gst_video_time_code_free (tc2);
  g_date_time_unref (dt1);
}

GST_END_TEST;

GST_START_TEST (videotimecode_dailyjam_distance)
{
  GstVideoTimeCode *tc;
  GDateTime *dt;

  dt = g_date_time_new_utc (2016, 7, 29, 10, 32, 50);

  tc = gst_video_time_code_new (50, 1, dt,
      GST_VIDEO_TIME_CODE_FLAGS_NONE, 0, 0, 0, 0, 0);

  fail_unless_equals_uint64 (gst_video_time_code_nsec_since_daily_jam (tc), 0);
  fail_unless_equals_uint64 (gst_video_time_code_frames_since_daily_jam (tc),
      0);

  gst_video_time_code_add_frames (tc, 10);
  fail_unless_equals_uint64 (gst_video_time_code_nsec_since_daily_jam (tc),
      200 * GST_MSECOND);
  fail_unless_equals_uint64 (gst_video_time_code_frames_since_daily_jam (tc),
      10);

  gst_video_time_code_add_frames (tc, 40);
  fail_unless_equals_uint64 (gst_video_time_code_nsec_since_daily_jam (tc),
      1 * GST_SECOND);
  fail_unless_equals_uint64 (gst_video_time_code_frames_since_daily_jam (tc),
      50);

  gst_video_time_code_add_frames (tc, 50);
  fail_unless_equals_uint64 (gst_video_time_code_nsec_since_daily_jam (tc),
      2 * GST_SECOND);
  fail_unless_equals_uint64 (gst_video_time_code_frames_since_daily_jam (tc),
      100);

  gst_video_time_code_add_frames (tc, 58 * 50);
  fail_unless_equals_uint64 (gst_video_time_code_nsec_since_daily_jam (tc),
      60 * GST_SECOND);
  fail_unless_equals_uint64 (gst_video_time_code_frames_since_daily_jam (tc),
      60 * 50);

  gst_video_time_code_add_frames (tc, 9 * 60 * 50);
  fail_unless_equals_uint64 (gst_video_time_code_nsec_since_daily_jam (tc),
      10 * 60 * GST_SECOND);
  fail_unless_equals_uint64 (gst_video_time_code_frames_since_daily_jam (tc),
      10 * 60 * 50);

  gst_video_time_code_add_frames (tc, 20 * 60 * 50);
  fail_unless_equals_uint64 (gst_video_time_code_nsec_since_daily_jam (tc),
      30 * 60 * GST_SECOND);
  fail_unless_equals_uint64 (gst_video_time_code_frames_since_daily_jam (tc),
      30 * 60 * 50);

  gst_video_time_code_add_frames (tc, 30 * 60 * 50);
  fail_unless_equals_uint64 (gst_video_time_code_nsec_since_daily_jam (tc),
      60 * 60 * GST_SECOND);
  fail_unless_equals_uint64 (gst_video_time_code_frames_since_daily_jam (tc),
      60 * 60 * 50);

  gst_video_time_code_add_frames (tc, 9 * 60 * 60 * 50);
  fail_unless_equals_uint64 (gst_video_time_code_nsec_since_daily_jam (tc),
      10 * 60 * 60 * GST_SECOND);
  fail_unless_equals_uint64 (gst_video_time_code_frames_since_daily_jam (tc),
      10 * 60 * 60 * 50);

  gst_video_time_code_free (tc);

  /* Now test with drop-frame: while it is called "drop-frame", not actual
   * frames are dropped but instead every once in a while timecodes are
   * skipped. As such, every frame still has the same distance to its next
   * frame. */
  tc = gst_video_time_code_new (60000, 1001, dt,
      GST_VIDEO_TIME_CODE_FLAGS_DROP_FRAME, 0, 0, 0, 0, 0);

  fail_unless_equals_uint64 (gst_video_time_code_nsec_since_daily_jam (tc), 0);
  fail_unless_equals_uint64 (gst_video_time_code_frames_since_daily_jam (tc),
      0);

  gst_video_time_code_add_frames (tc, 10);
  fail_unless_equals_uint64 (gst_video_time_code_nsec_since_daily_jam (tc),
      gst_util_uint64_scale (10, G_GUINT64_CONSTANT (1001) * GST_SECOND,
          60000));
  fail_unless_equals_uint64 (gst_video_time_code_frames_since_daily_jam (tc),
      10);

  gst_video_time_code_add_frames (tc, 50);
  fail_unless_equals_uint64 (gst_video_time_code_nsec_since_daily_jam (tc),
      gst_util_uint64_scale (60, G_GUINT64_CONSTANT (1001) * GST_SECOND,
          60000));
  fail_unless_equals_uint64 (gst_video_time_code_frames_since_daily_jam (tc),
      60);

  gst_video_time_code_add_frames (tc, 60);
  fail_unless_equals_uint64 (gst_video_time_code_nsec_since_daily_jam (tc),
      gst_util_uint64_scale (120, G_GUINT64_CONSTANT (1001) * GST_SECOND,
          60000));
  fail_unless_equals_uint64 (gst_video_time_code_frames_since_daily_jam (tc),
      120);

  gst_video_time_code_add_frames (tc, 58 * 60);
  fail_unless_equals_uint64 (gst_video_time_code_nsec_since_daily_jam (tc),
      gst_util_uint64_scale (60 * 60, G_GUINT64_CONSTANT (1001) * GST_SECOND,
          60000));
  fail_unless_equals_uint64 (gst_video_time_code_frames_since_daily_jam (tc),
      60 * 60);

  gst_video_time_code_add_frames (tc, 9 * 60 * 60);
  fail_unless_equals_uint64 (gst_video_time_code_nsec_since_daily_jam (tc),
      gst_util_uint64_scale (10 * 60 * 60,
          G_GUINT64_CONSTANT (1001) * GST_SECOND, 60000));
  fail_unless_equals_uint64 (gst_video_time_code_frames_since_daily_jam (tc),
      10 * 60 * 60);

  gst_video_time_code_add_frames (tc, 20 * 60 * 60);
  fail_unless_equals_uint64 (gst_video_time_code_nsec_since_daily_jam (tc),
      gst_util_uint64_scale (30 * 60 * 60,
          G_GUINT64_CONSTANT (1001) * GST_SECOND, 60000));
  fail_unless_equals_uint64 (gst_video_time_code_frames_since_daily_jam (tc),
      30 * 60 * 60);

  gst_video_time_code_add_frames (tc, 30 * 60 * 60);
  fail_unless_equals_uint64 (gst_video_time_code_nsec_since_daily_jam (tc),
      gst_util_uint64_scale (60 * 60 * 60,
          G_GUINT64_CONSTANT (1001) * GST_SECOND, 60000));
  fail_unless_equals_uint64 (gst_video_time_code_frames_since_daily_jam (tc),
      60 * 60 * 60);

  gst_video_time_code_add_frames (tc, 9 * 60 * 60 * 60);
  fail_unless_equals_uint64 (gst_video_time_code_nsec_since_daily_jam (tc),
      gst_util_uint64_scale (10 * 60 * 60 * 60,
          G_GUINT64_CONSTANT (1001) * GST_SECOND, 60000));
  fail_unless_equals_uint64 (gst_video_time_code_frames_since_daily_jam (tc),
      10 * 60 * 60 * 60);

  gst_video_time_code_free (tc);

  g_date_time_unref (dt);
}

GST_END_TEST;

GST_START_TEST (videotimecode_serialize_deserialize)
{
  const gchar *str = "01:02:03:04";
  gchar *str2;
  GstVideoTimeCode *tc;
  GValue v = G_VALUE_INIT;
  GValue v2 = G_VALUE_INIT;

  g_value_init (&v, G_TYPE_STRING);
  g_value_init (&v2, GST_TYPE_VIDEO_TIME_CODE);

  fail_unless (gst_value_deserialize (&v2, str));
  tc = g_value_get_boxed (&v2);
  str2 = gst_video_time_code_to_string (tc);
  fail_unless_equals_string (str, str2);
  g_free (str2);

  g_value_set_string (&v, str);

  g_value_transform (&v, &v2);
  str2 = gst_value_serialize (&v2);
  fail_unless_equals_string (str, str2);
  g_free (str2);

  tc = g_value_get_boxed (&v2);
  str2 = gst_video_time_code_to_string (tc);
  fail_unless_equals_string (str, str2);
  g_free (str2);

  g_value_transform (&v2, &v);
  str2 = (gchar *) g_value_get_string (&v);
  fail_unless_equals_string (str, str2);
  g_value_unset (&v2);
  g_value_unset (&v);
}

GST_END_TEST;

GST_START_TEST (videotimecode_interval)
{
  GstVideoTimeCode *tc, *tc2;
  GstVideoTimeCodeInterval *tc_diff;
  int i;

  tc = gst_video_time_code_new (25, 1, NULL, 0, 1, 2, 3, 4, 0);
  tc_diff = gst_video_time_code_interval_new (1, 1, 1, 1);
  tc2 = gst_video_time_code_add_interval (tc, tc_diff);
  fail_unless_equals_int (tc2->hours, 2);
  fail_unless_equals_int (tc2->minutes, 3);
  fail_unless_equals_int (tc2->seconds, 4);
  fail_unless_equals_int (tc2->frames, 5);
  fail_unless_equals_int (tc2->config.fps_n, tc->config.fps_n);
  fail_unless_equals_int (tc2->config.fps_d, tc->config.fps_d);
  gst_video_time_code_free (tc2);
  gst_video_time_code_interval_free (tc_diff);

  gst_video_time_code_init (tc, 30000, 1001, NULL,
      GST_VIDEO_TIME_CODE_FLAGS_DROP_FRAME, 0, 0, 0, 0, 0);
  tc_diff = gst_video_time_code_interval_new (0, 1, 0, 0);
  for (i = 1; i <= 9; i++) {
    tc2 = gst_video_time_code_add_interval (tc, tc_diff);
    fail_unless_equals_int (tc2->hours, 0);
    fail_unless_equals_int (tc2->minutes, i);
    fail_unless_equals_int (tc2->seconds, 0);
    fail_unless_equals_int (tc2->frames, 2);
    gst_video_time_code_free (tc);
    tc = gst_video_time_code_copy (tc2);
    gst_video_time_code_free (tc2);
  }
  tc2 = gst_video_time_code_add_interval (tc, tc_diff);
  fail_unless_equals_int (tc2->hours, 0);
  fail_unless_equals_int (tc2->minutes, 10);
  fail_unless_equals_int (tc2->seconds, 0);
  fail_unless_equals_int (tc2->frames, 0);
  gst_video_time_code_free (tc2);
  gst_video_time_code_free (tc);
  gst_video_time_code_interval_free (tc_diff);
}

GST_END_TEST;

GST_START_TEST (videotimecode_validation)
{
#define CHECK_TC(fps_n, fps_d, drop, hours, minutes, seconds, frames, valid)    \
    G_STMT_START { GstVideoTimeCode *tc =                                 \
        gst_video_time_code_new (fps_n, fps_d, /*latest_daily_jam=*/ NULL,\
          drop ? GST_VIDEO_TIME_CODE_FLAGS_DROP_FRAME                     \
               : GST_VIDEO_TIME_CODE_FLAGS_NONE,                          \
          hours, minutes, seconds, frames, /*field_count=*/ 0);           \
      if (valid)  /* not '...(tc) == valid' for nicer error messages */   \
        fail_unless (gst_video_time_code_is_valid (tc));                  \
      else                                                                \
        fail_if (gst_video_time_code_is_valid (tc));                      \
      gst_video_time_code_free (tc);                                      \
    } G_STMT_END

  /* plain vanilla valid */
  CHECK_TC (25, 1, FALSE, 10, 11, 12, 13, TRUE);

  /* disallowed invalid frame rate */
  CHECK_TC (25, 0, FALSE, 0, 0, 0, 0, FALSE);
  /* disallowed unknown frame rate */
  CHECK_TC (0, 1, FALSE, 0, 0, 0, 0, FALSE);
  /* disallowed fractional frame rate */
  CHECK_TC (90000, 1001, FALSE, 0, 0, 0, 0, FALSE);
  /* allowed fractional frame rate */
  CHECK_TC (24000, 1001, FALSE, 0, 0, 0, 0, TRUE);
  /* allowed frame rate less than 1 FPS */
  CHECK_TC (900, 1000, FALSE, 0, 0, 0, 0, TRUE);
  /* allowed integer frame rate */
  CHECK_TC (9000, 100, FALSE, 0, 0, 0, 0, TRUE);
  /* TODO: CHECK_TC (60060, 1001, FALSE, 0, 0, 0, 0, TRUE);
   * https://gitlab.freedesktop.org/gstreamer/gstreamer/-/issues/2823
   */

  /* 'hours' >= 24 */
  CHECK_TC (60, 1, FALSE, 28, 1, 2, 3, FALSE);
  /* 'minutes' >= 60 */
  CHECK_TC (30, 1, FALSE, 1, 67, 4, 5, FALSE);
  /* 'seconds' >= 60 */
  CHECK_TC (25, 1, FALSE, 0, 1, 234, 5, FALSE);
  /* 'frames' >= FPS */
  CHECK_TC (24, 1, FALSE, 0, 1, 2, 34, FALSE);
  /* TODO Add tests for dis-/allowed 'seconds' when FPS<1.0 */

  /* 23.976 is not a drop-frame frame rate */
  CHECK_TC (24000, 1001, TRUE, 0, 0, 0, 11, FALSE);
  /* non-dropped frame at 29.97 FPS */
  CHECK_TC (30000, 1001, TRUE, 0, 20, 0, 0, TRUE);
  /* dropped frame at 29.97 FPS */
  CHECK_TC (30000, 1001, TRUE, 0, 25, 0, 1, FALSE);
  /* non-dropped frame at 59.94 FPS */
  CHECK_TC (60000, 1001, TRUE, 1, 30, 0, 2, TRUE);
  /* dropped frame at 59.94 FPS */
  CHECK_TC (60000, 1001, TRUE, 1, 36, 0, 3, FALSE);
  /* non-dropped frame at 119.88 FPS */
  CHECK_TC (120000, 1001, TRUE, 12, 40, 0, 6, TRUE);
  /* dropped frame at 119.88 FPS */
  CHECK_TC (120000, 1001, TRUE, 12, 49, 0, 7, FALSE);
#undef CHECK_TC
}

GST_END_TEST;

GST_START_TEST (videotimecode_from_date_time_1s)
{
  GstVideoTimeCode *tc;
  GDateTime *dt;

  dt = g_date_time_new_local (2017, 2, 16, 0, 0, 1);
  tc = gst_video_time_code_new_from_date_time_full (30000, 1001, dt,
      GST_VIDEO_TIME_CODE_FLAGS_DROP_FRAME, 0);

  fail_unless_equals_int (tc->hours, 0);
  fail_unless_equals_int (tc->minutes, 0);
  fail_unless_equals_int (tc->seconds, 1);
  fail_unless_equals_int (tc->frames, 0);

  gst_video_time_code_free (tc);
  g_date_time_unref (dt);
}

GST_END_TEST;

GST_START_TEST (videotimecode_from_date_time_halfsecond)
{
  GstVideoTimeCode *tc;
  GDateTime *dt, *dt2;

  dt = g_date_time_new_local (2017, 2, 17, 14, 13, 0);
  dt2 = g_date_time_add (dt, 500000);
  g_date_time_unref (dt);
  tc = gst_video_time_code_new_from_date_time_full (30000, 1001, dt2,
      GST_VIDEO_TIME_CODE_FLAGS_DROP_FRAME, 0);

  fail_unless_equals_int (tc->hours, 14);
  fail_unless_equals_int (tc->minutes, 13);
  fail_unless_equals_int (tc->seconds, 0);
  fail_unless_equals_int (tc->frames, 15);

  gst_video_time_code_free (tc);
  g_date_time_unref (dt2);
}

GST_END_TEST;

GST_START_TEST (videotimecode_from_date_time)
{
  GstVideoTimeCode *tc;
  GDateTime *dt;

  dt = g_date_time_new_local (2017, 2, 17, 14, 13, 30);
  tc = gst_video_time_code_new_from_date_time_full (30000, 1001, dt,
      GST_VIDEO_TIME_CODE_FLAGS_DROP_FRAME, 0);

  fail_unless_equals_int (tc->hours, 14);
  fail_unless_equals_int (tc->minutes, 13);
  fail_unless_equals_int (tc->seconds, 30);
  fail_unless_equals_int (tc->frames, 0);

  gst_video_time_code_free (tc);
  g_date_time_unref (dt);
}

GST_END_TEST;

static void
test_timecode_from_string (const gchar * str, gboolean success, guint hours,
    guint minutes, guint seconds, guint frames, gboolean drop_frame)
{
  GstVideoTimeCode *tc;
  gchar *s;

  tc = gst_video_time_code_new_from_string (str);

  if (success) {
    fail_unless (tc);
  } else {
    fail_if (tc);
    return;
  }

  fail_unless_equals_int (tc->hours, hours);
  fail_unless_equals_int (tc->minutes, minutes);
  fail_unless_equals_int (tc->seconds, seconds);
  fail_unless_equals_int (tc->frames, frames);

  if (drop_frame)
    fail_unless (tc->config.flags & GST_VIDEO_TIME_CODE_FLAGS_DROP_FRAME);
  else
    fail_if (tc->config.flags & GST_VIDEO_TIME_CODE_FLAGS_DROP_FRAME);

  s = gst_video_time_code_to_string (tc);
  fail_unless_equals_string (s, str);

  gst_video_time_code_free (tc);
  g_free (s);
}

GST_START_TEST (videotimecode_from_to_string)
{
  test_timecode_from_string ("11:12:13:14", TRUE, 11, 12, 13, 14, FALSE);
  test_timecode_from_string ("11:12:13;14", TRUE, 11, 12, 13, 14, TRUE);
  test_timecode_from_string ("11:12:13:", FALSE, 0, 0, 0, 0, FALSE);
  test_timecode_from_string ("11:12:13:ab", FALSE, 0, 0, 0, 0, FALSE);
  test_timecode_from_string ("a 11:12:13:14", FALSE, 0, 0, 0, 0, FALSE);
}

GST_END_TEST;

GST_START_TEST (videotimecode_half_fps)
{
  GstVideoTimeCode *tc;
  GDateTime *dt;

  dt = g_date_time_new_utc (2016, 7, 29, 10, 32, 50);

  tc = gst_video_time_code_new (1, 2, dt,
      GST_VIDEO_TIME_CODE_FLAGS_NONE, 0, 0, 0, 0, 0);

  fail_unless (gst_video_time_code_is_valid (tc));
  fail_unless_equals_uint64 (gst_video_time_code_nsec_since_daily_jam (tc), 0);
  fail_unless_equals_uint64 (gst_video_time_code_frames_since_daily_jam (tc),
      0);
  fail_unless_equals_int (tc->frames, 0);
  fail_unless_equals_int (tc->seconds, 0);
  fail_unless_equals_int (tc->minutes, 0);
  fail_unless_equals_int (tc->hours, 0);

  gst_video_time_code_add_frames (tc, 10);
  fail_unless (gst_video_time_code_is_valid (tc));
  fail_unless_equals_uint64 (gst_video_time_code_nsec_since_daily_jam (tc),
      20 * GST_SECOND);
  fail_unless_equals_uint64 (gst_video_time_code_frames_since_daily_jam (tc),
      10);
  fail_unless_equals_int (tc->frames, 0);
  fail_unless_equals_int (tc->seconds, 20);
  fail_unless_equals_int (tc->minutes, 0);
  fail_unless_equals_int (tc->hours, 0);

  gst_video_time_code_add_frames (tc, 40);
  fail_unless (gst_video_time_code_is_valid (tc));
  fail_unless_equals_uint64 (gst_video_time_code_nsec_since_daily_jam (tc),
      100 * GST_SECOND);
  fail_unless_equals_uint64 (gst_video_time_code_frames_since_daily_jam (tc),
      50);
  fail_unless_equals_int (tc->frames, 0);
  fail_unless_equals_int (tc->seconds, 40);
  fail_unless_equals_int (tc->minutes, 1);
  fail_unless_equals_int (tc->hours, 0);

  tc->seconds += 1;
  fail_if (gst_video_time_code_is_valid (tc));

  gst_video_time_code_free (tc);
  g_date_time_unref (dt);
}

GST_END_TEST;

static Suite *
gst_videotimecode_suite (void)
{
  Suite *s = suite_create ("GstVideoTimeCode");
  TCase *tc = tcase_create ("general");

  suite_add_tcase (s, tc);

  tcase_add_test (tc, videotimecode_compare_equal);
  tcase_add_test (tc, videotimecode_compare_fps_n);
  tcase_add_test (tc, videotimecode_compare_fps_d);
  tcase_add_test (tc, videotimecode_compare_frames);
  tcase_add_test (tc, videotimecode_compare_seconds);
  tcase_add_test (tc, videotimecode_compare_minutes);
  tcase_add_test (tc, videotimecode_compare_hours);
  tcase_add_test (tc, videotimecode_compare_fieldcounts);

  tcase_add_test (tc, videotimecode_addframe_10);
  tcase_add_test (tc, videotimecode_addframe_0);
  tcase_add_test (tc, videotimecode_addframe_high);
  tcase_add_test (tc, videotimecode_addframe_dropframe);
  tcase_add_test (tc, videotimecode_addframe_framedropped);
  tcase_add_test (tc, videotimecode_addframe_wrapover);
  tcase_add_test (tc, videotimecode_addframe_60drop_dropframe);
  tcase_add_test (tc, videotimecode_addframe_60drop_framedropped);
  tcase_add_test (tc, videotimecode_addframe_60drop_wrapover);
  tcase_add_test (tc, videotimecode_addframe_loop);

  tcase_add_test (tc, videotimecode_dailyjam_todatetime);
  tcase_add_test (tc, videotimecode_dailyjam_compare);
  tcase_add_test (tc, videotimecode_dailyjam_distance);
  tcase_add_test (tc, videotimecode_serialize_deserialize);
  tcase_add_test (tc, videotimecode_interval);
  tcase_add_test (tc, videotimecode_validation);

  tcase_add_test (tc, videotimecode_from_date_time_1s);
  tcase_add_test (tc, videotimecode_from_date_time_halfsecond);
  tcase_add_test (tc, videotimecode_from_date_time);

  tcase_add_test (tc, videotimecode_from_to_string);

  tcase_add_test (tc, videotimecode_half_fps);

  return s;
}

GST_CHECK_MAIN (gst_videotimecode);
