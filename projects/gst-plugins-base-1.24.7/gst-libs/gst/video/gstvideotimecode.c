/* GStreamer
 * Copyright (C) <2016> Vivia Nikolaidou <vivia@toolsonair.com>
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

#include <stdio.h>
#include "gstvideotimecode.h"

static void
gst_video_time_code_gvalue_to_string (const GValue * tc_val, GValue * str_val);
static void
gst_video_time_code_gvalue_from_string (const GValue * str_val,
    GValue * tc_val);
static gboolean gst_video_time_code_deserialize (GValue * dest,
    const gchar * tc_str);
static gchar *gst_video_time_code_serialize (const GValue * val);

static void
_init (GType type)
{
  static GstValueTable table =
      { 0, (GstValueCompareFunc) gst_video_time_code_compare,
    (GstValueSerializeFunc) gst_video_time_code_serialize,
    (GstValueDeserializeFunc) gst_video_time_code_deserialize
  };

  table.type = type;
  gst_value_register (&table);
  g_value_register_transform_func (type, G_TYPE_STRING,
      (GValueTransform) gst_video_time_code_gvalue_to_string);
  g_value_register_transform_func (G_TYPE_STRING, type,
      (GValueTransform) gst_video_time_code_gvalue_from_string);
}

G_DEFINE_BOXED_TYPE_WITH_CODE (GstVideoTimeCode, gst_video_time_code,
    (GBoxedCopyFunc) gst_video_time_code_copy,
    (GBoxedFreeFunc) gst_video_time_code_free, _init (g_define_type_id));

/**
 * gst_video_time_code_is_valid:
 * @tc: #GstVideoTimeCode to check
 *
 * Returns: whether @tc is a valid timecode (supported frame rate,
 * hours/minutes/seconds/frames not overflowing)
 *
 * Since: 1.10
 */
gboolean
gst_video_time_code_is_valid (const GstVideoTimeCode * tc)
{
  guint fr;

  g_return_val_if_fail (tc != NULL, FALSE);

  if (tc->config.fps_n == 0 || tc->config.fps_d == 0)
    return FALSE;

  if (tc->hours >= 24)
    return FALSE;
  if (tc->minutes >= 60)
    return FALSE;
  if (tc->seconds >= 60)
    return FALSE;

  /* We can't have more frames than rounded up frames per second */
  fr = (tc->config.fps_n + (tc->config.fps_d >> 1)) / tc->config.fps_d;
  if (tc->config.fps_d > tc->config.fps_n) {
    guint64 s;

    if (tc->frames > 0)
      return FALSE;
    /* For less than 1 fps only certain second values are allowed */
    s = tc->seconds + (60 * (tc->minutes + (60 * tc->hours)));
    if ((s * tc->config.fps_n) % tc->config.fps_d != 0)
      return FALSE;
  } else {
    if (tc->frames >= fr && (tc->config.fps_n != 0 || tc->config.fps_d != 1))
      return FALSE;
  }

  /* We need either a specific X/1001 framerate, or less than 1 FPS,
   * otherwise an integer framerate. */
  if (tc->config.fps_d == 1001) {
    if (tc->config.fps_n != 30000 && tc->config.fps_n != 60000 &&
        tc->config.fps_n != 24000 && tc->config.fps_n != 120000)
      return FALSE;
  } else if (tc->config.fps_n >= tc->config.fps_d
      && tc->config.fps_n % tc->config.fps_d != 0) {
    return FALSE;
  }

  /* We support only 30000/1001, 60000/1001, and 120000/1001 (see above) as
   * drop-frame framerates. 24000/1001 is *not* a drop-frame framerate! */
  if (tc->config.flags & GST_VIDEO_TIME_CODE_FLAGS_DROP_FRAME) {
    if (tc->config.fps_d != 1001 || tc->config.fps_n == 24000)
      return FALSE;
  }

  /* Drop-frame framerates require skipping over the first two
   * timecodes every minute except for every tenth minute in case
   * of 30000/1001, the first four timecodes for 60000/1001,
   * and the first eight timecodes for 120000/1001. */
  if ((tc->config.flags & GST_VIDEO_TIME_CODE_FLAGS_DROP_FRAME) &&
      tc->minutes % 10 && tc->seconds == 0 && tc->frames < fr / 15) {
    return FALSE;
  }

  return TRUE;
}

/**
 * gst_video_time_code_to_string:
 * @tc: A #GstVideoTimeCode to convert
 *
 * Returns: the SMPTE ST 2059-1:2015 string representation of @tc. That will
 * take the form hh:mm:ss:ff. The last separator (between seconds and frames)
 * may vary:
 *
 * ';' for drop-frame, non-interlaced content and for drop-frame interlaced
 * field 2
 * ',' for drop-frame interlaced field 1
 * ':' for non-drop-frame, non-interlaced content and for non-drop-frame
 * interlaced field 2
 * '.' for non-drop-frame interlaced field 1
 *
 * Since: 1.10
 */
gchar *
gst_video_time_code_to_string (const GstVideoTimeCode * tc)
{
  gchar *ret;
  gboolean top_dot_present;
  gchar sep;

  /* Top dot is present for non-interlaced content, and for field 2 in
   * interlaced content */
  top_dot_present =
      !((tc->config.flags & GST_VIDEO_TIME_CODE_FLAGS_INTERLACED) != 0
      && tc->field_count == 1);

  if (tc->config.flags & GST_VIDEO_TIME_CODE_FLAGS_DROP_FRAME)
    sep = top_dot_present ? ';' : ',';
  else
    sep = top_dot_present ? ':' : '.';

  ret =
      g_strdup_printf ("%02d:%02d:%02d%c%02d", tc->hours, tc->minutes,
      tc->seconds, sep, tc->frames);

  return ret;
}

/**
 * gst_video_time_code_to_date_time:
 * @tc: A valid #GstVideoTimeCode to convert
 *
 * The @tc.config->latest_daily_jam is required to be non-NULL.
 *
 * Returns: (nullable): the #GDateTime representation of @tc or %NULL if @tc
 *   has no daily jam.
 *
 * Since: 1.10
 */
GDateTime *
gst_video_time_code_to_date_time (const GstVideoTimeCode * tc)
{
  GDateTime *ret;
  GDateTime *ret2;
  gdouble add_us;

  g_return_val_if_fail (gst_video_time_code_is_valid (tc), NULL);

  if (tc->config.latest_daily_jam == NULL) {
    gchar *tc_str = gst_video_time_code_to_string (tc);
    GST_WARNING
        ("Asked to convert time code %s to GDateTime, but its latest daily jam is NULL",
        tc_str);
    g_free (tc_str);
    return NULL;
  }

  ret = g_date_time_ref (tc->config.latest_daily_jam);

  gst_util_fraction_to_double (tc->frames * tc->config.fps_d, tc->config.fps_n,
      &add_us);
  if ((tc->config.flags & GST_VIDEO_TIME_CODE_FLAGS_INTERLACED)
      && tc->field_count == 1) {
    gdouble sub_us;

    gst_util_fraction_to_double (tc->config.fps_d, 2 * tc->config.fps_n,
        &sub_us);
    add_us -= sub_us;
  }

  ret2 = g_date_time_add_seconds (ret, add_us + tc->seconds);
  g_date_time_unref (ret);
  ret = g_date_time_add_minutes (ret2, tc->minutes);
  g_date_time_unref (ret2);
  ret2 = g_date_time_add_hours (ret, tc->hours);
  g_date_time_unref (ret);

  return ret2;
}

/**
 * gst_video_time_code_init_from_date_time:
 * @tc: an uninitialized #GstVideoTimeCode
 * @fps_n: Numerator of the frame rate
 * @fps_d: Denominator of the frame rate
 * @dt: #GDateTime to convert
 * @flags: #GstVideoTimeCodeFlags
 * @field_count: Interlaced video field count
 *
 * The resulting config->latest_daily_jam is set to midnight, and timecode is
 * set to the given time.
 *
 * Will assert on invalid parameters, use gst_video_time_code_init_from_date_time_full()
 * for being able to handle invalid parameters.
 *
 * Since: 1.12
 */
void
gst_video_time_code_init_from_date_time (GstVideoTimeCode * tc,
    guint fps_n, guint fps_d,
    GDateTime * dt, GstVideoTimeCodeFlags flags, guint field_count)
{
  if (!gst_video_time_code_init_from_date_time_full (tc, fps_n, fps_d, dt,
          flags, field_count))
    g_return_if_fail (gst_video_time_code_is_valid (tc));
}

/**
 * gst_video_time_code_init_from_date_time_full:
 * @tc: a #GstVideoTimeCode
 * @fps_n: Numerator of the frame rate
 * @fps_d: Denominator of the frame rate
 * @dt: #GDateTime to convert
 * @flags: #GstVideoTimeCodeFlags
 * @field_count: Interlaced video field count
 *
 * The resulting config->latest_daily_jam is set to
 * midnight, and timecode is set to the given time.
 *
 * Returns: %TRUE if @tc could be correctly initialized to a valid timecode
 *
 * Since: 1.16
 */
gboolean
gst_video_time_code_init_from_date_time_full (GstVideoTimeCode * tc,
    guint fps_n, guint fps_d,
    GDateTime * dt, GstVideoTimeCodeFlags flags, guint field_count)
{
  GDateTime *jam;

  g_return_val_if_fail (tc != NULL, FALSE);
  g_return_val_if_fail (dt != NULL, FALSE);
  g_return_val_if_fail (fps_n != 0 && fps_d != 0, FALSE);

  gst_video_time_code_clear (tc);

  jam = g_date_time_new_local (g_date_time_get_year (dt),
      g_date_time_get_month (dt), g_date_time_get_day_of_month (dt), 0, 0, 0.0);

  if (fps_d > fps_n) {
    guint64 hour, min, sec;

    sec =
        g_date_time_get_second (dt) + (60 * (g_date_time_get_minute (dt) +
            (60 * g_date_time_get_hour (dt))));
    sec -= (sec * fps_n) % fps_d;

    min = sec / 60;
    sec = sec % 60;
    hour = min / 60;
    min = min % 60;

    gst_video_time_code_init (tc, fps_n, fps_d, jam, flags,
        hour, min, sec, 0, field_count);
  } else {
    guint64 frames;
    gboolean add_a_frame = FALSE;

    /* Note: This might be inaccurate for 1 frame
     * in case we have a drop frame timecode */
    frames =
        gst_util_uint64_scale_round (g_date_time_get_microsecond (dt) *
        G_GINT64_CONSTANT (1000), fps_n, fps_d * GST_SECOND);
    if (G_UNLIKELY (((frames == fps_n) && (fps_d == 1)) ||
            ((frames == fps_n / 1000) && (fps_d == 1001)))) {
      /* Avoid invalid timecodes */
      frames--;
      add_a_frame = TRUE;
    }

    gst_video_time_code_init (tc, fps_n, fps_d, jam, flags,
        g_date_time_get_hour (dt), g_date_time_get_minute (dt),
        g_date_time_get_second (dt), frames, field_count);

    if (tc->config.flags & GST_VIDEO_TIME_CODE_FLAGS_DROP_FRAME) {
      guint df = (tc->config.fps_n + (tc->config.fps_d >> 1)) /
          (15 * tc->config.fps_d);
      if (tc->minutes % 10 && tc->seconds == 0 && tc->frames < df) {
        tc->frames = df;
      }
    }
    if (add_a_frame)
      gst_video_time_code_increment_frame (tc);
  }

  g_date_time_unref (jam);

  return gst_video_time_code_is_valid (tc);
}

/**
 * gst_video_time_code_nsec_since_daily_jam:
 * @tc: a valid #GstVideoTimeCode
 *
 * Returns: how many nsec have passed since the daily jam of @tc.
 *
 * Since: 1.10
 */
guint64
gst_video_time_code_nsec_since_daily_jam (const GstVideoTimeCode * tc)
{
  guint64 frames, nsec;

  g_return_val_if_fail (gst_video_time_code_is_valid (tc), -1);

  frames = gst_video_time_code_frames_since_daily_jam (tc);
  nsec =
      gst_util_uint64_scale (frames, GST_SECOND * tc->config.fps_d,
      tc->config.fps_n);

  return nsec;
}

/**
 * gst_video_time_code_frames_since_daily_jam:
 * @tc: a valid #GstVideoTimeCode
 *
 * Returns: how many frames have passed since the daily jam of @tc.
 *
 * Since: 1.10
 */
guint64
gst_video_time_code_frames_since_daily_jam (const GstVideoTimeCode * tc)
{
  guint ff_nom;
  gdouble ff;

  g_return_val_if_fail (gst_video_time_code_is_valid (tc), -1);

  gst_util_fraction_to_double (tc->config.fps_n, tc->config.fps_d, &ff);
  if (tc->config.fps_d == 1001) {
    ff_nom = tc->config.fps_n / 1000;
  } else {
    ff_nom = ff;
  }
  if (tc->config.flags & GST_VIDEO_TIME_CODE_FLAGS_DROP_FRAME) {
    /* these need to be truncated to integer: side effect, code looks cleaner
     * */
    guint ff_minutes = 60 * ff;
    guint ff_hours = 3600 * ff;
    /* for 30000/1001 we drop the first 2 frames per minute, for 60000/1001 we
     * drop the first 4 : so we use this number */
    guint dropframe_multiplier;

    if (tc->config.fps_n == 30000) {
      dropframe_multiplier = 2;
    } else if (tc->config.fps_n == 60000) {
      dropframe_multiplier = 4;
    } else {
      /* already checked by gst_video_time_code_is_valid() */
      g_assert_not_reached ();
    }

    return tc->frames + (ff_nom * tc->seconds) +
        (ff_minutes * tc->minutes) +
        dropframe_multiplier * ((gint) (tc->minutes / 10)) +
        (ff_hours * tc->hours);
  } else if (tc->config.fps_d > tc->config.fps_n) {
    return gst_util_uint64_scale (tc->seconds + (60 * (tc->minutes +
                (60 * tc->hours))), tc->config.fps_n, tc->config.fps_d);
  } else {
    return tc->frames + (ff_nom * (tc->seconds + (60 * (tc->minutes +
                    (60 * tc->hours)))));
  }

}

/**
 * gst_video_time_code_increment_frame:
 * @tc: a valid #GstVideoTimeCode
 *
 * Adds one frame to @tc.
 *
 * Since: 1.10
 */
void
gst_video_time_code_increment_frame (GstVideoTimeCode * tc)
{
  gst_video_time_code_add_frames (tc, 1);
}

/**
 * gst_video_time_code_add_frames:
 * @tc: a valid #GstVideoTimeCode
 * @frames: How many frames to add or subtract
 *
 * Adds or subtracts @frames amount of frames to @tc. tc needs to
 * contain valid data, as verified by gst_video_time_code_is_valid().
 *
 * Since: 1.10
 */
void
gst_video_time_code_add_frames (GstVideoTimeCode * tc, gint64 frames)
{
  guint64 framecount;
  guint64 h_notmod24;
  guint64 h_new, min_new, sec_new, frames_new;
  gdouble ff;
  guint ff_nom;
  /* This allows for better readability than putting G_GUINT64_CONSTANT(60)
   * into a long calculation line */
  const guint64 sixty = 60;
  /* formulas found in SMPTE ST 2059-1:2015 section 9.4.3
   * and adapted for 60/1.001 as well as 30/1.001 */

  g_return_if_fail (gst_video_time_code_is_valid (tc));

  gst_util_fraction_to_double (tc->config.fps_n, tc->config.fps_d, &ff);
  if (tc->config.fps_d == 1001) {
    ff_nom = tc->config.fps_n / 1000;
  } else {
    ff_nom = ff;
  }

  if (tc->config.flags & GST_VIDEO_TIME_CODE_FLAGS_DROP_FRAME) {
    /* these need to be truncated to integer: side effect, code looks cleaner
     * */
    guint ff_minutes = 60 * ff;
    guint ff_hours = 3600 * ff;
    /* a bunch of intermediate variables, to avoid monster code with possible
     * integer overflows */
    guint64 min_new_tmp1, min_new_tmp2, min_new_tmp3, min_new_denom;
    /* for 30000/1001 we drop the first 2 frames per minute, for 60000/1001 we
     * drop the first 4 : so we use this number */
    guint dropframe_multiplier;

    if (tc->config.fps_n == 30000) {
      dropframe_multiplier = 2;
    } else if (tc->config.fps_n == 60000) {
      dropframe_multiplier = 4;
    } else {
      /* already checked by gst_video_time_code_is_valid() */
      g_assert_not_reached ();
    }

    framecount =
        frames + tc->frames + (ff_nom * tc->seconds) +
        (ff_minutes * tc->minutes) +
        dropframe_multiplier * ((gint) (tc->minutes / 10)) +
        (ff_hours * tc->hours);
    h_notmod24 = gst_util_uint64_scale_int (framecount, 1, ff_hours);

    min_new_denom = sixty * ff_nom;
    min_new_tmp1 = (framecount - (h_notmod24 * ff_hours)) / min_new_denom;
    min_new_tmp2 = framecount + dropframe_multiplier * min_new_tmp1;
    min_new_tmp1 =
        (framecount - (h_notmod24 * ff_hours)) / (sixty * 10 * ff_nom);
    min_new_tmp3 =
        dropframe_multiplier * min_new_tmp1 + (h_notmod24 * ff_hours);
    min_new =
        gst_util_uint64_scale_int (min_new_tmp2 - min_new_tmp3, 1,
        min_new_denom);

    sec_new =
        (guint64) ((framecount - (ff_minutes * min_new) -
            dropframe_multiplier * ((gint) (min_new / 10)) -
            (ff_hours * h_notmod24)) / ff_nom);

    frames_new =
        framecount - (ff_nom * sec_new) - (ff_minutes * min_new) -
        (dropframe_multiplier * ((gint) (min_new / 10))) -
        (ff_hours * h_notmod24);
  } else if (tc->config.fps_d > tc->config.fps_n) {
    frames_new =
        frames + gst_util_uint64_scale (tc->seconds + (60 * (tc->minutes +
                (60 * tc->hours))), tc->config.fps_n, tc->config.fps_d);
    sec_new =
        gst_util_uint64_scale (frames_new, tc->config.fps_d, tc->config.fps_n);
    frames_new = 0;
    min_new = sec_new / 60;
    sec_new = sec_new % 60;
    h_notmod24 = min_new / 60;
    min_new = min_new % 60;
  } else {
    framecount =
        frames + tc->frames + (ff_nom * (tc->seconds + (sixty * (tc->minutes +
                    (sixty * tc->hours)))));
    h_notmod24 =
        gst_util_uint64_scale_int (framecount, 1, ff_nom * sixty * sixty);
    min_new =
        gst_util_uint64_scale_int ((framecount -
            (ff_nom * sixty * sixty * h_notmod24)), 1, (ff_nom * sixty));
    sec_new =
        gst_util_uint64_scale_int ((framecount - (ff_nom * sixty * (min_new +
                    (sixty * h_notmod24)))), 1, ff_nom);
    frames_new =
        framecount - (ff_nom * (sec_new + sixty * (min_new +
                (sixty * h_notmod24))));
    if (frames_new > ff_nom)
      frames_new = 0;
  }

  h_new = h_notmod24 % 24;

  /* The calculations above should always give correct results */
  g_assert (min_new < 60);
  g_assert (sec_new < 60);
  g_assert (frames_new < ff_nom || (ff_nom == 0 && frames_new == 0));

  tc->hours = h_new;
  tc->minutes = min_new;
  tc->seconds = sec_new;
  tc->frames = frames_new;
}

/**
 * gst_video_time_code_compare:
 * @tc1: a valid #GstVideoTimeCode
 * @tc2: another valid #GstVideoTimeCode
 *
 * Compares @tc1 and @tc2. If both have latest daily jam information, it is
 * taken into account. Otherwise, it is assumed that the daily jam of both
 * @tc1 and @tc2 was at the same time. Both time codes must be valid.
 *
 * Returns: 1 if @tc1 is after @tc2, -1 if @tc1 is before @tc2, 0 otherwise.
 *
 * Since: 1.10
 */
gint
gst_video_time_code_compare (const GstVideoTimeCode * tc1,
    const GstVideoTimeCode * tc2)
{
  g_return_val_if_fail (gst_video_time_code_is_valid (tc1), -1);
  g_return_val_if_fail (gst_video_time_code_is_valid (tc2), -1);

  if (tc1->config.latest_daily_jam == NULL
      || tc2->config.latest_daily_jam == NULL) {
    guint64 nsec1, nsec2;
#ifndef GST_DISABLE_GST_DEBUG
    gchar *str1, *str2;

    str1 = gst_video_time_code_to_string (tc1);
    str2 = gst_video_time_code_to_string (tc2);
    GST_INFO
        ("Comparing time codes %s and %s, but at least one of them has no "
        "latest daily jam information. Assuming they started together",
        str1, str2);
    g_free (str1);
    g_free (str2);
#endif
    if (tc1->hours > tc2->hours) {
      return 1;
    } else if (tc1->hours < tc2->hours) {
      return -1;
    }
    if (tc1->minutes > tc2->minutes) {
      return 1;
    } else if (tc1->minutes < tc2->minutes) {
      return -1;
    }
    if (tc1->seconds > tc2->seconds) {
      return 1;
    } else if (tc1->seconds < tc2->seconds) {
      return -1;
    }

    nsec1 =
        gst_util_uint64_scale (GST_SECOND,
        tc1->frames * tc1->config.fps_n, tc1->config.fps_d);
    nsec2 =
        gst_util_uint64_scale (GST_SECOND,
        tc2->frames * tc2->config.fps_n, tc2->config.fps_d);
    if (nsec1 > nsec2) {
      return 1;
    } else if (nsec1 < nsec2) {
      return -1;
    }
    if (tc1->config.flags & GST_VIDEO_TIME_CODE_FLAGS_INTERLACED) {
      if (tc1->field_count > tc2->field_count)
        return 1;
      else if (tc1->field_count < tc2->field_count)
        return -1;
    }
    return 0;
  } else {
    GDateTime *dt1, *dt2;
    gint ret;

    dt1 = gst_video_time_code_to_date_time (tc1);
    dt2 = gst_video_time_code_to_date_time (tc2);

    ret = g_date_time_compare (dt1, dt2);

    g_date_time_unref (dt1);
    g_date_time_unref (dt2);

    return ret;
  }
}

/**
 * gst_video_time_code_new:
 * @fps_n: Numerator of the frame rate
 * @fps_d: Denominator of the frame rate
 * @latest_daily_jam: The latest daily jam of the #GstVideoTimeCode
 * @flags: #GstVideoTimeCodeFlags
 * @hours: the hours field of #GstVideoTimeCode
 * @minutes: the minutes field of #GstVideoTimeCode
 * @seconds: the seconds field of #GstVideoTimeCode
 * @frames: the frames field of #GstVideoTimeCode
 * @field_count: Interlaced video field count
 *
 * @field_count is 0 for progressive, 1 or 2 for interlaced.
 * @latest_daiy_jam reference is stolen from caller.
 *
 * Returns: a new #GstVideoTimeCode with the given values.
 * The values are not checked for being in a valid range. To see if your
 * timecode actually has valid content, use gst_video_time_code_is_valid().
 *
 * Since: 1.10
 */
GstVideoTimeCode *
gst_video_time_code_new (guint fps_n, guint fps_d, GDateTime * latest_daily_jam,
    GstVideoTimeCodeFlags flags, guint hours, guint minutes, guint seconds,
    guint frames, guint field_count)
{
  GstVideoTimeCode *tc;

  tc = g_new0 (GstVideoTimeCode, 1);
  gst_video_time_code_init (tc, fps_n, fps_d, latest_daily_jam, flags, hours,
      minutes, seconds, frames, field_count);
  return tc;
}

/**
 * gst_video_time_code_new_empty:
 *
 * Returns: a new empty, invalid #GstVideoTimeCode
 *
 * Since: 1.10
 */
GstVideoTimeCode *
gst_video_time_code_new_empty (void)
{
  GstVideoTimeCode *tc;

  tc = g_new0 (GstVideoTimeCode, 1);
  gst_video_time_code_clear (tc);
  return tc;
}

static void
gst_video_time_code_gvalue_from_string (const GValue * str_val, GValue * tc_val)
{
  const gchar *tc_str = g_value_get_string (str_val);
  GstVideoTimeCode *tc;

  tc = gst_video_time_code_new_from_string (tc_str);
  g_value_take_boxed (tc_val, tc);
}

static void
gst_video_time_code_gvalue_to_string (const GValue * tc_val, GValue * str_val)
{
  const GstVideoTimeCode *tc = g_value_get_boxed (tc_val);
  gchar *tc_str;

  tc_str = gst_video_time_code_to_string (tc);
  g_value_take_string (str_val, tc_str);
}

static gchar *
gst_video_time_code_serialize (const GValue * val)
{
  GstVideoTimeCode *tc = g_value_get_boxed (val);
  return gst_video_time_code_to_string (tc);
}

static gboolean
gst_video_time_code_deserialize (GValue * dest, const gchar * tc_str)
{
  GstVideoTimeCode *tc = gst_video_time_code_new_from_string (tc_str);

  if (tc == NULL) {
    return FALSE;
  }

  g_value_take_boxed (dest, tc);
  return TRUE;
}

/**
 * gst_video_time_code_new_from_string:
 * @tc_str: The string that represents the #GstVideoTimeCode
 *
 * Returns: (nullable): a new #GstVideoTimeCode from the given string or %NULL
 *   if the string could not be passed.
 *
 * Since: 1.12
 */
GstVideoTimeCode *
gst_video_time_code_new_from_string (const gchar * tc_str)
{
  GstVideoTimeCode *tc;
  guint hours, minutes, seconds, frames;

  if (sscanf (tc_str, "%02u:%02u:%02u:%02u", &hours, &minutes, &seconds,
          &frames)
      == 4
      || sscanf (tc_str, "%02u:%02u:%02u.%02u", &hours, &minutes, &seconds,
          &frames)
      == 4) {
    tc = gst_video_time_code_new (0, 1, NULL, GST_VIDEO_TIME_CODE_FLAGS_NONE,
        hours, minutes, seconds, frames, 0);

    return tc;
  } else if (sscanf (tc_str, "%02u:%02u:%02u;%02u", &hours, &minutes, &seconds,
          &frames)
      == 4 || sscanf (tc_str, "%02u:%02u:%02u,%02u", &hours, &minutes, &seconds,
          &frames)
      == 4) {
    tc = gst_video_time_code_new (0, 1, NULL,
        GST_VIDEO_TIME_CODE_FLAGS_DROP_FRAME, hours, minutes, seconds, frames,
        0);

    return tc;
  } else {
    GST_ERROR ("Warning: Could not parse timecode %s. "
        "Please input a timecode in the form 00:00:00:00", tc_str);
    return NULL;
  }
}

/**
 * gst_video_time_code_new_from_date_time:
 * @fps_n: Numerator of the frame rate
 * @fps_d: Denominator of the frame rate
 * @dt: #GDateTime to convert
 * @flags: #GstVideoTimeCodeFlags
 * @field_count: Interlaced video field count
 *
 * The resulting config->latest_daily_jam is set to
 * midnight, and timecode is set to the given time.
 *
 * This might return a completely invalid timecode, use
 * gst_video_time_code_new_from_date_time_full() to ensure
 * that you would get %NULL instead in that case.
 *
 * Returns: the #GstVideoTimeCode representation of @dt.
 *
 * Since: 1.12
 */
GstVideoTimeCode *
gst_video_time_code_new_from_date_time (guint fps_n, guint fps_d,
    GDateTime * dt, GstVideoTimeCodeFlags flags, guint field_count)
{
  GstVideoTimeCode *tc;
  tc = gst_video_time_code_new_empty ();
  gst_video_time_code_init_from_date_time_full (tc, fps_n, fps_d, dt, flags,
      field_count);
  return tc;
}

/**
 * gst_video_time_code_new_from_date_time_full:
 * @fps_n: Numerator of the frame rate
 * @fps_d: Denominator of the frame rate
 * @dt: #GDateTime to convert
 * @flags: #GstVideoTimeCodeFlags
 * @field_count: Interlaced video field count
 *
 * The resulting config->latest_daily_jam is set to
 * midnight, and timecode is set to the given time.
 *
 * Returns: (nullable): the #GstVideoTimeCode representation of @dt, or %NULL if
 *   no valid timecode could be created.
 *
 * Since: 1.16
 */
GstVideoTimeCode *
gst_video_time_code_new_from_date_time_full (guint fps_n, guint fps_d,
    GDateTime * dt, GstVideoTimeCodeFlags flags, guint field_count)
{
  GstVideoTimeCode *tc;
  tc = gst_video_time_code_new_empty ();
  if (!gst_video_time_code_init_from_date_time_full (tc, fps_n, fps_d, dt,
          flags, field_count)) {
    gst_video_time_code_free (tc);
    return NULL;
  }
  return tc;
}

/**
 * gst_video_time_code_init:
 * @tc: a #GstVideoTimeCode
 * @fps_n: Numerator of the frame rate
 * @fps_d: Denominator of the frame rate
 * @latest_daily_jam: (allow-none): The latest daily jam of the #GstVideoTimeCode
 * @flags: #GstVideoTimeCodeFlags
 * @hours: the hours field of #GstVideoTimeCode
 * @minutes: the minutes field of #GstVideoTimeCode
 * @seconds: the seconds field of #GstVideoTimeCode
 * @frames: the frames field of #GstVideoTimeCode
 * @field_count: Interlaced video field count
 *
 * @field_count is 0 for progressive, 1 or 2 for interlaced.
 * @latest_daiy_jam reference is stolen from caller.
 *
 * Initializes @tc with the given values.
 * The values are not checked for being in a valid range. To see if your
 * timecode actually has valid content, use gst_video_time_code_is_valid().
 *
 * Since: 1.10
 */
void
gst_video_time_code_init (GstVideoTimeCode * tc, guint fps_n, guint fps_d,
    GDateTime * latest_daily_jam, GstVideoTimeCodeFlags flags, guint hours,
    guint minutes, guint seconds, guint frames, guint field_count)
{
  tc->hours = hours;
  tc->minutes = minutes;
  tc->seconds = seconds;
  tc->frames = frames;
  tc->field_count = field_count;
  tc->config.fps_n = fps_n;
  tc->config.fps_d = fps_d;
  if (latest_daily_jam != NULL)
    tc->config.latest_daily_jam = g_date_time_ref (latest_daily_jam);
  else
    tc->config.latest_daily_jam = NULL;
  tc->config.flags = flags;
}

/**
 * gst_video_time_code_clear:
 * @tc: a #GstVideoTimeCode
 *
 * Initializes @tc with empty/zero/NULL values and frees any memory
 * it might currently use.
 *
 * Since: 1.10
 */
void
gst_video_time_code_clear (GstVideoTimeCode * tc)
{
  tc->hours = 0;
  tc->minutes = 0;
  tc->seconds = 0;
  tc->frames = 0;
  tc->field_count = 0;
  tc->config.fps_n = 0;
  tc->config.fps_d = 1;
  if (tc->config.latest_daily_jam != NULL)
    g_date_time_unref (tc->config.latest_daily_jam);
  tc->config.latest_daily_jam = NULL;
  tc->config.flags = 0;
}

/**
 * gst_video_time_code_copy:
 * @tc: a #GstVideoTimeCode
 *
 * Returns: a new #GstVideoTimeCode with the same values as @tc.
 *
 * Since: 1.10
 */
GstVideoTimeCode *
gst_video_time_code_copy (const GstVideoTimeCode * tc)
{
  return gst_video_time_code_new (tc->config.fps_n, tc->config.fps_d,
      tc->config.latest_daily_jam, tc->config.flags, tc->hours, tc->minutes,
      tc->seconds, tc->frames, tc->field_count);
}

/**
 * gst_video_time_code_free:
 * @tc: a #GstVideoTimeCode
 *
 * Frees @tc.
 *
 * Since: 1.10
 */
void
gst_video_time_code_free (GstVideoTimeCode * tc)
{
  if (tc->config.latest_daily_jam != NULL)
    g_date_time_unref (tc->config.latest_daily_jam);

  g_free (tc);
}

/**
 * gst_video_time_code_add_interval:
 * @tc: The #GstVideoTimeCode where the diff should be added. This
 * must contain valid timecode values.
 * @tc_inter: The #GstVideoTimeCodeInterval to add to @tc.
 * The interval must contain valid values, except that for drop-frame
 * timecode, it may also contain timecodes which would normally
 * be dropped. These are then corrected to the next reasonable timecode.
 *
 * This makes a component-wise addition of @tc_inter to @tc. For example,
 * adding ("01:02:03:04", "00:01:00:00") will return "01:03:03:04".
 * When it comes to drop-frame timecodes,
 * adding ("00:00:00;00", "00:01:00:00") will return "00:01:00;02"
 * because of drop-frame oddities. However,
 * adding ("00:09:00;02", "00:01:00:00") will return "00:10:00;00"
 * because this time we can have an exact minute.
 *
 * Returns: (nullable): A new #GstVideoTimeCode with @tc_inter added or %NULL
 *   if the interval can't be added.
 *
 * Since: 1.12
 */
GstVideoTimeCode *
gst_video_time_code_add_interval (const GstVideoTimeCode * tc,
    const GstVideoTimeCodeInterval * tc_inter)
{
  GstVideoTimeCode *ret;
  guint frames_to_add;
  guint df;
  gboolean needs_correction;

  g_return_val_if_fail (gst_video_time_code_is_valid (tc), NULL);

  ret = gst_video_time_code_new (tc->config.fps_n, tc->config.fps_d,
      tc->config.latest_daily_jam, tc->config.flags, tc_inter->hours,
      tc_inter->minutes, tc_inter->seconds, tc_inter->frames, 0);

  df = (tc->config.fps_n + (tc->config.fps_d >> 1)) / (tc->config.fps_d * 15);

  /* Drop-frame compensation: Create a valid timecode from the
   * interval */
  needs_correction = (tc->config.flags & GST_VIDEO_TIME_CODE_FLAGS_DROP_FRAME)
      && ret->minutes % 10 && ret->seconds == 0 && ret->frames < df;
  if (needs_correction) {
    ret->minutes--;
    ret->seconds = 59;
    ret->frames = df * 14;
  }

  if (!gst_video_time_code_is_valid (ret)) {
    GST_ERROR ("Unsupported time code interval");
    gst_video_time_code_free (ret);
    return NULL;
  }

  frames_to_add = gst_video_time_code_frames_since_daily_jam (tc);

  /* Drop-frame compensation: 00:01:00;00 is falsely interpreted as
   * 00:00:59;28 */
  if (needs_correction) {
    /* User wants us to split at invalid timecodes */
    if (tc->minutes % 10 == 0 && tc->frames <= df) {
      /* Apply compensation every 10th minute: before adding the frames,
       * but only if we are before the "invalid frame" mark */
      frames_to_add += df;
      needs_correction = FALSE;
    }
  }
  gst_video_time_code_add_frames (ret, frames_to_add);
  if (needs_correction && ret->minutes % 10 == 0 && tc->frames > df) {
    gst_video_time_code_add_frames (ret, df);
  }

  return ret;
}

G_DEFINE_BOXED_TYPE (GstVideoTimeCodeInterval, gst_video_time_code_interval,
    (GBoxedCopyFunc) gst_video_time_code_interval_copy,
    (GBoxedFreeFunc) gst_video_time_code_interval_free);

/**
 * gst_video_time_code_interval_new:
 * @hours: the hours field of #GstVideoTimeCodeInterval
 * @minutes: the minutes field of #GstVideoTimeCodeInterval
 * @seconds: the seconds field of #GstVideoTimeCodeInterval
 * @frames: the frames field of #GstVideoTimeCodeInterval
 *
 * Returns: a new #GstVideoTimeCodeInterval with the given values.
 *
 * Since: 1.12
 */
GstVideoTimeCodeInterval *
gst_video_time_code_interval_new (guint hours, guint minutes, guint seconds,
    guint frames)
{
  GstVideoTimeCodeInterval *tc;

  tc = g_new0 (GstVideoTimeCodeInterval, 1);
  gst_video_time_code_interval_init (tc, hours, minutes, seconds, frames);
  return tc;
}

/**
 * gst_video_time_code_interval_new_from_string:
 * @tc_inter_str: The string that represents the #GstVideoTimeCodeInterval
 *
 * @tc_inter_str must only have ":" as separators.
 *
 * Returns: (nullable): a new #GstVideoTimeCodeInterval from the given string
 *   or %NULL if the string could not be passed.
 *
 * Since: 1.12
 */
GstVideoTimeCodeInterval *
gst_video_time_code_interval_new_from_string (const gchar * tc_inter_str)
{
  GstVideoTimeCodeInterval *tc;
  guint hours, minutes, seconds, frames;

  if (sscanf (tc_inter_str, "%02u:%02u:%02u:%02u", &hours, &minutes, &seconds,
          &frames)
      == 4
      || sscanf (tc_inter_str, "%02u:%02u:%02u;%02u", &hours, &minutes,
          &seconds, &frames)
      == 4
      || sscanf (tc_inter_str, "%02u:%02u:%02u.%02u", &hours, &minutes,
          &seconds, &frames)
      == 4
      || sscanf (tc_inter_str, "%02u:%02u:%02u,%02u", &hours, &minutes,
          &seconds, &frames)
      == 4) {
    tc = gst_video_time_code_interval_new (hours, minutes, seconds, frames);

    return tc;
  } else {
    GST_ERROR ("Warning: Could not parse timecode %s. "
        "Please input a timecode in the form 00:00:00:00", tc_inter_str);
    return NULL;
  }

}

/**
 * gst_video_time_code_interval_init:
 * @tc: a #GstVideoTimeCodeInterval
 * @hours: the hours field of #GstVideoTimeCodeInterval
 * @minutes: the minutes field of #GstVideoTimeCodeInterval
 * @seconds: the seconds field of #GstVideoTimeCodeInterval
 * @frames: the frames field of #GstVideoTimeCodeInterval
 *
 * Initializes @tc with the given values.
 *
 * Since: 1.12
 */
void
gst_video_time_code_interval_init (GstVideoTimeCodeInterval * tc, guint hours,
    guint minutes, guint seconds, guint frames)
{
  tc->hours = hours;
  tc->minutes = minutes;
  tc->seconds = seconds;
  tc->frames = frames;
}

/**
 * gst_video_time_code_interval_clear:
 * @tc: a #GstVideoTimeCodeInterval
 *
 * Initializes @tc with empty/zero/NULL values.
 *
 * Since: 1.12
 */
void
gst_video_time_code_interval_clear (GstVideoTimeCodeInterval * tc)
{
  tc->hours = 0;
  tc->minutes = 0;
  tc->seconds = 0;
  tc->frames = 0;
}

/**
 * gst_video_time_code_interval_copy:
 * @tc: a #GstVideoTimeCodeInterval
 *
 * Returns: a new #GstVideoTimeCodeInterval with the same values as @tc.
 *
 * Since: 1.12
 */
GstVideoTimeCodeInterval *
gst_video_time_code_interval_copy (const GstVideoTimeCodeInterval * tc)
{
  return gst_video_time_code_interval_new (tc->hours, tc->minutes,
      tc->seconds, tc->frames);
}

/**
 * gst_video_time_code_interval_free:
 * @tc: a #GstVideoTimeCodeInterval
 *
 * Frees @tc.
 *
 * Since: 1.12
 */
void
gst_video_time_code_interval_free (GstVideoTimeCodeInterval * tc)
{
  g_free (tc);
}
