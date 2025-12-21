/* GStreamer
 * Copyright (C) <2005,2006> Wim Taymans <wim@fluendo.com>
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
/*
 * Unless otherwise indicated, Source Code is licensed under MIT license.
 * See further explanation attached in License Statement (distributed in the file
 * LICENSE).
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/**
 * SECTION:gstrtsprange
 * @title: GstRTSPTimeRange
 * @short_description: dealing with time ranges
 *
 * Provides helper functions to deal with time ranges.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "gstrtsprange.h"

static gdouble
gst_strtod (const gchar * dstr)
{
  gchar s[G_ASCII_DTOSTR_BUF_SIZE] = { 0, };

  /* canonicalise floating point string so we can handle float strings
   * in the form "24.930" or "24,930" irrespective of the current locale.
   * We should always be getting floats in 24.930 format with a floating point,
   * but let's accept malformed ones as well, easy mistake to make after all */
  g_strlcpy (s, dstr, sizeof (s));
  g_strdelimit (s, ",", '.');
  return g_ascii_strtod (s, NULL);
}

/* npt-time     =   "now" | npt-sec | npt-hhmmss
 * npt-sec      =   1*DIGIT [ "." *DIGIT ]
 * npt-hhmmss   =   npt-hh ":" npt-mm ":" npt-ss [ "." *DIGIT ]
 * npt-hh       =   1*DIGIT     ; any positive number
 * npt-mm       =   1*2DIGIT    ; 0-59
 * npt-ss       =   1*2DIGIT    ; 0-59
 */
static GstRTSPResult
parse_npt_time (const gchar * str, GstRTSPTime * time)
{
  if (strncmp (str, "now", 3) == 0) {
    time->type = GST_RTSP_TIME_NOW;
  } else if (str[0] == '\0' || str[0] == '-') {
    time->type = GST_RTSP_TIME_END;
  } else if (strstr (str, ":")) {
    gint hours, mins;

    if (sscanf (str, "%2d:%2d:", &hours, &mins) != 2)
      return GST_RTSP_EINVAL;

    str = strchr (str, ':');
    str = strchr (str + 1, ':');
    if (str == NULL)
      return GST_RTSP_EINVAL;

    time->type = GST_RTSP_TIME_SECONDS;
    time->seconds = ((hours * 60) + mins) * 60 + gst_strtod (str + 1);
  } else {
    time->type = GST_RTSP_TIME_SECONDS;
    time->seconds = gst_strtod (str);
  }
  return GST_RTSP_OK;
}

/* npt-range    =   ( npt-time "-" [ npt-time ] ) | ( "-" npt-time )
 */
static GstRTSPResult
parse_npt_range (const gchar * str, GstRTSPTimeRange * range)
{
  GstRTSPResult res;
  gchar *p;

  range->unit = GST_RTSP_RANGE_NPT;

  /* find '-' separator */
  p = strstr (str, "-");
  if (p == NULL)
    return GST_RTSP_EINVAL;

  if ((res = parse_npt_time (str, &range->min)) != GST_RTSP_OK)
    goto done;

  res = parse_npt_time (p + 1, &range->max);

  /* a single - is not allowed */
  if (range->min.type == GST_RTSP_TIME_END
      && range->max.type == GST_RTSP_TIME_END)
    return GST_RTSP_EINVAL;

done:
  return res;
}

/*   utc-time     =   utc-date "T" utc-time "Z"
 *   utc-date     =   8DIGIT                    ; < YYYYMMDD >
 *   utc-time     =   6DIGIT [ "." fraction ]   ; < HHMMSS.fraction >
 *
 *   Example for November 8, 1996 at 14h37 and 20 and a quarter seconds
 *   UTC:
 *
 *   19961108T143720.25Z
 */
static GstRTSPResult
parse_utc_time (const gchar * str, GstRTSPTime * time, GstRTSPTime2 * time2,
    const gchar * limit)
{

  if (str[0] == '\0') {
    time->type = GST_RTSP_TIME_END;
    return GST_RTSP_OK;
  } else {
    gint year, month, day;
    gint hours, mins;
    gdouble secs;
    gchar *T, *Z;

    T = strchr (str, 'T');
    if (T == NULL || T != str + 8)
      return GST_RTSP_EINVAL;

    Z = strchr (T + 1, 'Z');
    if (Z == NULL)
      return GST_RTSP_EINVAL;

    time->type = GST_RTSP_TIME_UTC;

    if (sscanf (str, "%4d%2d%2dT%2d%2d%lfZ", &year, &month, &day, &hours,
            &mins, &secs) != 6)
      return GST_RTSP_EINVAL;

    time2->year = year;
    time2->month = month;
    time2->day = day;
    time->seconds = ((hours * 60) + mins) * 60 + secs;
  }
  return GST_RTSP_OK;
}

/*   utc-range    =   "clock" "=" utc-time "-" [ utc-time ]
 */
static GstRTSPResult
parse_utc_range (const gchar * str, GstRTSPTimeRange * range)
{
  GstRTSPResult res;
  gchar *p;

  range->unit = GST_RTSP_RANGE_CLOCK;

  /* find '-' separator, can't have a single - */
  p = strstr (str, "-");
  if (p == NULL || p == str)
    return GST_RTSP_EINVAL;

  if ((res = parse_utc_time (str, &range->min, &range->min2, p)) != GST_RTSP_OK)
    goto done;

  res = parse_utc_time (p + 1, &range->max, &range->max2, NULL);

done:
  return res;
}

/* smpte-time   =   1*2DIGIT ":" 1*2DIGIT ":" 1*2DIGIT [ ":" 1*2DIGIT ]
 *                     [ "." 1*2DIGIT ]
 *  hours:minutes:seconds:frames.subframes
*/
static GstRTSPResult
parse_smpte_time (const gchar * str, GstRTSPTime * time, GstRTSPTime2 * time2,
    const gchar * limit)
{
  gint hours, mins, secs;

  if (str[0] == '\0') {
    time->type = GST_RTSP_TIME_END;
    return GST_RTSP_OK;
  } else {
    if (sscanf (str, "%2d:%2d:%2d", &hours, &mins, &secs) != 3)
      return GST_RTSP_EINVAL;

    time->type = GST_RTSP_TIME_FRAMES;
    time->seconds = ((hours * 60) + mins) * 60 + secs;
    str = strchr (str, ':');
    str = strchr (str + 1, ':');
    str = strchr (str + 1, ':');
    if (str && (limit == NULL || str < limit))
      time2->frames = gst_strtod (str + 1);
  }
  return GST_RTSP_OK;
}

/* smpte-range  =   smpte-type "=" smpte-time "-" [ smpte-time ]
 */
static GstRTSPResult
parse_smpte_range (const gchar * str, GstRTSPTimeRange * range)
{
  GstRTSPResult res;
  gchar *p;

  /* find '-' separator, can't have a single - */
  p = strstr (str, "-");
  if (p == NULL || p == str)
    return GST_RTSP_EINVAL;

  if ((res =
          parse_smpte_time (str, &range->min, &range->min2, p)) != GST_RTSP_OK)
    goto done;

  res = parse_smpte_time (p + 1, &range->max, &range->max2, NULL);

done:
  return res;
}

/**
 * gst_rtsp_range_parse:
 * @rangestr: a range string to parse
 * @range: (out): location to hold the #GstRTSPTimeRange result
 *
 * Parse @rangestr to a #GstRTSPTimeRange.
 *
 * Returns: #GST_RTSP_OK on success.
 */
GstRTSPResult
gst_rtsp_range_parse (const gchar * rangestr, GstRTSPTimeRange ** range)
{
  GstRTSPResult ret;
  GstRTSPTimeRange *res;
  gchar *p;

  g_return_val_if_fail (rangestr != NULL, GST_RTSP_EINVAL);
  g_return_val_if_fail (range != NULL, GST_RTSP_EINVAL);

  res = g_new0 (GstRTSPTimeRange, 1);

  p = (gchar *) rangestr;
  /* first figure out the units of the range */
  if (g_str_has_prefix (p, "npt=")) {
    ret = parse_npt_range (p + 4, res);
  } else if (g_str_has_prefix (p, "clock=")) {
    ret = parse_utc_range (p + 6, res);
  } else if (g_str_has_prefix (p, "smpte=")) {
    res->unit = GST_RTSP_RANGE_SMPTE;
    ret = parse_smpte_range (p + 6, res);
  } else if (g_str_has_prefix (p, "smpte-30-drop=")) {
    res->unit = GST_RTSP_RANGE_SMPTE_30_DROP;
    ret = parse_smpte_range (p + 14, res);
  } else if (g_str_has_prefix (p, "smpte-25=")) {
    res->unit = GST_RTSP_RANGE_SMPTE_25;
    ret = parse_smpte_range (p + 9, res);
  } else
    goto invalid;

  if (ret != GST_RTSP_OK)
    goto invalid;

  *range = res;
  return ret;

  /* ERRORS */
invalid:
  {
    gst_rtsp_range_free (res);
    return GST_RTSP_EINVAL;
  }
}

static void
string_append_dtostr (GString * string, gdouble value, guint precision)
{
  gchar dstrbuf[G_ASCII_DTOSTR_BUF_SIZE] = { 0, };
  gchar *dot;
  guint len;

  precision++;

  if (value != 0.0)
    value += 4.9 * pow (10.0, precision * -1.0);

  g_ascii_dtostr (dstrbuf, G_ASCII_DTOSTR_BUF_SIZE, value);

  dot = strchr (dstrbuf, '.');

  if (dot == NULL)
    goto done;

  for (; *dot != '.' && *dot != '0'; dot++);

  if ((dot - dstrbuf) + precision < G_ASCII_DTOSTR_BUF_SIZE)
    dot[precision] = 0;

  len = strlen (dstrbuf);
  while (dstrbuf[len - 1] == '0')
    dstrbuf[--len] = 0;
  if (dstrbuf[len - 1] == '.')
    dstrbuf[--len] = 0;

done:

  g_string_append (string, dstrbuf);
}

static gboolean
time_to_string (const GstRTSPTime * t1, const GstRTSPTime2 * t2,
    GString * string)
{
  gboolean res = TRUE;

  switch (t1->type) {
    case GST_RTSP_TIME_SECONDS:
      /* need to format floating point value strings as in C locale */
      string_append_dtostr (string, t1->seconds +
          (t1->seconds ? 0.00000000005 : 0), 9);
      break;
    case GST_RTSP_TIME_NOW:
      g_string_append (string, "now");
      break;
    case GST_RTSP_TIME_END:
      break;
    case GST_RTSP_TIME_FRAMES:
    {
      gint64 sec = t1->seconds;

      /* need to format floating point value strings as in C locale */
      g_string_append_printf (string, "%d:%02d:%02d", (gint) sec / (60 * 60),
          (gint) (sec % (60 * 60)) / 60, (gint) sec % 60);

      if (t2->frames > 0.0) {
        g_string_append_printf (string, ":%s", t2->frames < 10 ? "0" : "");
        string_append_dtostr (string, t2->frames + 0.005, 2);
      }
      break;
    }
    case GST_RTSP_TIME_UTC:
    {
      gint64 sec = t1->seconds;
      gint hours, minutes;
      gdouble seconds;

      hours = sec / (60 * 60);
      sec -= hours * 60 * 60;
      minutes = sec / 60;
      sec = ((hours * 60) + minutes) * 60;
      seconds = t1->seconds - sec;
      if (seconds)
        seconds += 0.00000000005;

      g_string_append_printf (string, "%04d%02d%02dT%02d%02d%s",
          t2->year, t2->month, t2->day, hours, minutes,
          seconds < 10 ? "0" : "");
      string_append_dtostr (string, seconds, 9);
      g_string_append (string, "Z");
      break;
    }
    default:
      res = FALSE;
      break;
  }
  return res;
}

static gboolean
range_to_string (const GstRTSPTimeRange * range, GString * string)
{
  gboolean res;

  if (!(res = time_to_string (&range->min, &range->min2, string)))
    goto done;

  g_string_append (string, "-");

  if (!(res = time_to_string (&range->max, &range->max2, string)))
    goto done;

done:
  return res;
}

/**
 * gst_rtsp_range_to_string:
 * @range: a #GstRTSPTimeRange
 *
 * Convert @range into a string representation.
 *
 * Returns: The string representation of @range. g_free() after usage.
 */
gchar *
gst_rtsp_range_to_string (const GstRTSPTimeRange * range)
{
  GString *string;

  g_return_val_if_fail (range != NULL, NULL);

  switch (range->unit) {
    case GST_RTSP_RANGE_NPT:
      string = g_string_new ("npt=");
      break;
    case GST_RTSP_RANGE_SMPTE:
    case GST_RTSP_RANGE_SMPTE_30_DROP:
      string = g_string_new ("smpte=");
      break;
    case GST_RTSP_RANGE_SMPTE_25:
      string = g_string_new ("smpte-25=");
      break;
    case GST_RTSP_RANGE_CLOCK:
      string = g_string_new ("clock=");
      break;
    default:
      goto not_implemented;
  }

  if (!range_to_string (range, string))
    goto format_failed;

  return g_string_free (string, FALSE);

  /* ERRORS */
not_implemented:
  {
    g_warning ("time range unit not yet implemented");
    return NULL;
  }
format_failed:
  {
    g_string_free (string, TRUE);
    return NULL;
  }
}

/**
 * gst_rtsp_range_free:
 * @range: a #GstRTSPTimeRange
 *
 * Free the memory allocated by @range.
 */
void
gst_rtsp_range_free (GstRTSPTimeRange * range)
{
  g_return_if_fail (range != NULL);

  g_free (range);
}

static GstClockTime
get_seconds (const GstRTSPTime * t)
{
  if (t->seconds < G_MAXINT) {
    gint num, denom;
    /* Don't do direct multiply with GST_SECOND to avoid rounding
     * errors.
     * This only works for "small" numbers, because num is limited to 32-bit
     */
    gst_util_double_to_fraction (t->seconds, &num, &denom);
    return gst_util_uint64_scale_int (GST_SECOND, num, denom);
  } else {
    return gst_util_gdouble_to_guint64 (t->seconds * GST_SECOND);
  }
}

static GstClockTime
get_frames (const GstRTSPTime2 * t, GstRTSPRangeUnit unit)
{
  gint num, denom;

  gst_util_double_to_fraction (t->frames, &num, &denom);

  switch (unit) {
    case GST_RTSP_RANGE_SMPTE_25:
      denom *= 25;
      break;
    case GST_RTSP_RANGE_SMPTE:
    case GST_RTSP_RANGE_SMPTE_30_DROP:
    default:
      num *= 1001;
      denom *= 30003;
      break;
  }
  return gst_util_uint64_scale_int (GST_SECOND, num, denom);
}

static GstClockTime
get_time (GstRTSPRangeUnit unit, const GstRTSPTime * t1,
    const GstRTSPTime2 * t2)
{
  GstClockTime res;

  switch (t1->type) {
    case GST_RTSP_TIME_SECONDS:
    {
      res = get_seconds (t1);
      break;
    }
    case GST_RTSP_TIME_UTC:
    {
      GDateTime *dt, *bt;
      GTimeSpan span;

      /* make time base, we use 1900 */
      bt = g_date_time_new_utc (1900, 1, 1, 0, 0, 0.0);
      /* convert to GDateTime without the seconds */
      dt = g_date_time_new_utc (t2->year, t2->month, t2->day, 0, 0, 0.0);
      /* get amount of microseconds */
      span = g_date_time_difference (dt, bt);
      g_date_time_unref (bt);
      g_date_time_unref (dt);
      /* add seconds */
      res = get_seconds (t1) + (span * 1000);
      break;
    }
    case GST_RTSP_TIME_FRAMES:
      res = get_seconds (t1);
      res += get_frames (t2, unit);
      break;
    default:
    case GST_RTSP_TIME_NOW:
    case GST_RTSP_TIME_END:
      res = GST_CLOCK_TIME_NONE;
      break;
  }
  return res;
}

/**
 * gst_rtsp_range_get_times:
 * @range: a #GstRTSPTimeRange
 * @min: (out): result minimum #GstClockTime
 * @max: (out): result maximum #GstClockTime
 *
 * Retrieve the minimum and maximum values from @range converted to
 * #GstClockTime in @min and @max.
 *
 * A value of %GST_CLOCK_TIME_NONE will be used to signal #GST_RTSP_TIME_NOW
 * and #GST_RTSP_TIME_END for @min and @max respectively.
 *
 * UTC times will be converted to nanoseconds since 1900.
 *
 * Returns: %TRUE on success.
 *
 * Since: 1.2
 */
gboolean
gst_rtsp_range_get_times (const GstRTSPTimeRange * range,
    GstClockTime * min, GstClockTime * max)
{
  g_return_val_if_fail (range != NULL, FALSE);

  if (min)
    *min = get_time (range->unit, &range->min, &range->min2);
  if (max)
    *max = get_time (range->unit, &range->max, &range->max2);

  return TRUE;
}

static void
set_time (GstRTSPTime * time, GstRTSPTime2 * time2, GstRTSPRangeUnit unit,
    GstClockTime clock_time)
{
  memset (time, 0, sizeof (GstRTSPTime));
  memset (time2, 0, sizeof (GstRTSPTime2));

  if (clock_time == GST_CLOCK_TIME_NONE) {
    time->type = GST_RTSP_TIME_END;
    return;
  }

  switch (unit) {
    case GST_RTSP_RANGE_SMPTE:
    case GST_RTSP_RANGE_SMPTE_30_DROP:
    {
      time->seconds = (guint64) (clock_time / GST_SECOND);
      time2->frames = 30003 * (clock_time % GST_SECOND) /
          (gdouble) (1001 * GST_SECOND);
      time->type = GST_RTSP_TIME_FRAMES;
      g_assert (time2->frames < 30);
      break;
    }
    case GST_RTSP_RANGE_SMPTE_25:
    {
      time->seconds = (guint64) (clock_time / GST_SECOND);
      time2->frames = (25 * (clock_time % GST_SECOND)) / (gdouble) GST_SECOND;
      time->type = GST_RTSP_TIME_FRAMES;
      g_assert (time2->frames < 25);
      break;
    }
    case GST_RTSP_RANGE_NPT:
    {
      time->seconds = (gdouble) clock_time / (gdouble) GST_SECOND;
      time->type = GST_RTSP_TIME_SECONDS;
      break;
    }
    case GST_RTSP_RANGE_CLOCK:
    {
      GDateTime *bt, *datetime;
      GstClockTime subsecond = clock_time % GST_SECOND;

      bt = g_date_time_new_utc (1900, 1, 1, 0, 0, 0.0);
      datetime = g_date_time_add_seconds (bt, clock_time / GST_SECOND);

      time2->year = g_date_time_get_year (datetime);
      time2->month = g_date_time_get_month (datetime);
      time2->day = g_date_time_get_day_of_month (datetime);

      time->seconds = g_date_time_get_hour (datetime) * 60 * 60;
      time->seconds += g_date_time_get_minute (datetime) * 60;
      time->seconds += g_date_time_get_seconds (datetime);
      time->seconds += (gdouble) subsecond / (gdouble) GST_SECOND;
      time->type = GST_RTSP_TIME_UTC;

      g_date_time_unref (bt);
      g_date_time_unref (datetime);
      break;
    }
  }

  if (time->seconds < 0.000000001)
    time->seconds = 0;
  if (time2->frames < 0.000000001)
    time2->frames = 0;
}

/**
 * gst_rtsp_range_convert_units:
 * @range: a #GstRTSPTimeRange
 * @unit: the unit to convert the range into
 *
 * Converts the range in-place between different types of units.
 * Ranges containing the special value #GST_RTSP_TIME_NOW can not be
 * converted as these are only valid for #GST_RTSP_RANGE_NPT.
 *
 * Returns: %TRUE if the range could be converted
 */

gboolean
gst_rtsp_range_convert_units (GstRTSPTimeRange * range, GstRTSPRangeUnit unit)
{
  if (range->unit == unit)
    return TRUE;

  if (range->min.type == GST_RTSP_TIME_NOW ||
      range->max.type == GST_RTSP_TIME_NOW)
    return FALSE;

  set_time (&range->min, &range->min2, unit,
      get_time (range->unit, &range->min, &range->min2));
  set_time (&range->max, &range->max2, unit,
      get_time (range->unit, &range->max, &range->max2));

  range->unit = unit;

  return TRUE;
}
