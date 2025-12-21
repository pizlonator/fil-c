/* GStreamer unit tests for the RTSP support library
 * Copyright (C) 2010 Andy Wingo <wingo@oblong.com>
 * Copyright (C) 2015 Tim-Philipp Müller <tim@centricular.com>
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

#include <gst/rtsp/rtsp.h>
#include <string.h>

GST_START_TEST (test_rtsp_url_basic)
{
  GstRTSPUrl *url = NULL;
  GstRTSPResult res;

  res = gst_rtsp_url_parse ("rtsp://localhost/foo/bar", &url);
  fail_unless (res == GST_RTSP_OK);
  fail_unless (url != NULL);
  fail_unless (url->transports & GST_RTSP_LOWER_TRANS_TCP);
  fail_unless (url->transports & GST_RTSP_LOWER_TRANS_UDP);
  fail_unless (url->transports & GST_RTSP_LOWER_TRANS_UDP_MCAST);
  fail_unless (url->family == GST_RTSP_FAM_INET);
  fail_unless (!url->user);
  fail_unless (!url->passwd);
  fail_unless_equals_string (url->host, "localhost");
  /* fail_unless (url->port == GST_RTSP_DEFAULT_PORT); */
  fail_unless_equals_string (url->abspath, "/foo/bar");
  fail_unless (!url->query);

  gst_rtsp_url_free (url);
}

GST_END_TEST;

GST_START_TEST (test_rtsp_url_query)
{
  GstRTSPUrl *url = NULL;
  GstRTSPResult res;
  gchar *uri;
  const gchar *original_uri = "rtsp://localhost/foo/bar/?baz=fooo";
  const gchar *original_uri_with_control =
      "rtsp://localhost/foo/bar/video/stream1";

  res = gst_rtsp_url_parse (original_uri, &url);
  fail_unless (res == GST_RTSP_OK);
  fail_unless (url != NULL);
  fail_unless (url->transports & GST_RTSP_LOWER_TRANS_TCP);
  fail_unless (url->transports & GST_RTSP_LOWER_TRANS_UDP);
  fail_unless (url->transports & GST_RTSP_LOWER_TRANS_UDP_MCAST);
  fail_unless (url->family == GST_RTSP_FAM_INET);
  fail_unless (!url->user);
  fail_unless (!url->passwd);
  fail_unless_equals_string (url->host, "localhost");
  fail_unless_equals_string (url->abspath, "/foo/bar/");
  fail_unless_equals_string (url->query, "baz=fooo");
  uri = gst_rtsp_url_get_request_uri (url);
  fail_unless_equals_string (uri, original_uri);
  g_free (uri);
  uri = gst_rtsp_url_get_request_uri_with_control (url, "video/stream1");
  fail_unless_equals_string (uri, original_uri_with_control);
  g_free (uri);

  gst_rtsp_url_free (url);
}

GST_END_TEST;

GST_START_TEST (test_rtsp_url_components_1)
{
  GstRTSPUrl *url = NULL;
  GstRTSPResult res;
  gchar **comps = NULL;

  res = gst_rtsp_url_parse ("rtsp://localhost/foo/bar", &url);
  fail_unless (res == GST_RTSP_OK);
  fail_unless (url != NULL);

  comps = gst_rtsp_url_decode_path_components (url);
  fail_unless (comps != NULL);
  fail_unless (g_strv_length (comps) == 3);
  fail_unless_equals_string (comps[0], "");
  fail_unless_equals_string (comps[1], "foo");
  fail_unless_equals_string (comps[2], "bar");

  g_strfreev (comps);
  gst_rtsp_url_free (url);
}

GST_END_TEST;

GST_START_TEST (test_rtsp_url_components_2)
{
  GstRTSPUrl *url = NULL;
  GstRTSPResult res;
  gchar **comps = NULL;

  res = gst_rtsp_url_parse ("rtsp://localhost/foo%2Fbar/qux%20baz", &url);
  fail_unless (res == GST_RTSP_OK);
  fail_unless (url != NULL);

  comps = gst_rtsp_url_decode_path_components (url);
  fail_unless (comps != NULL);
  fail_unless (g_strv_length (comps) == 3);
  fail_unless_equals_string (comps[0], "");
  fail_unless_equals_string (comps[1], "foo/bar");
  fail_unless_equals_string (comps[2], "qux baz");

  g_strfreev (comps);
  gst_rtsp_url_free (url);
}

GST_END_TEST;

GST_START_TEST (test_rtsp_url_components_3)
{
  GstRTSPUrl *url = NULL;
  GstRTSPResult res;
  gchar **comps = NULL;

  res = gst_rtsp_url_parse ("rtsp://localhost/foo%00bar/qux%20baz", &url);
  fail_unless (res == GST_RTSP_OK);
  fail_unless (url != NULL);

  comps = gst_rtsp_url_decode_path_components (url);
  fail_unless (comps != NULL);
  fail_unless (g_strv_length (comps) == 3);
  fail_unless_equals_string (comps[0], "");
  fail_unless_equals_string (comps[1], "foo%00bar");
  fail_unless_equals_string (comps[2], "qux baz");

  g_strfreev (comps);
  gst_rtsp_url_free (url);
}

GST_END_TEST;

GST_START_TEST (test_rtsp_range_npt)
{
  GstRTSPTimeRange *range;
  GstClockTime min, max;
  gchar *str;

  fail_unless (gst_rtsp_range_parse ("npt=", &range) == GST_RTSP_EINVAL);
  fail_unless (gst_rtsp_range_parse ("npt=0", &range) == GST_RTSP_EINVAL);
  fail_unless (gst_rtsp_range_parse ("npt=-", &range) == GST_RTSP_EINVAL);
  fail_unless (gst_rtsp_range_parse ("npt=now", &range) == GST_RTSP_EINVAL);

  fail_unless (gst_rtsp_range_parse ("npt=-now", &range) == GST_RTSP_OK);
  fail_unless (range->unit == GST_RTSP_RANGE_NPT);
  fail_unless (range->min.type == GST_RTSP_TIME_END);
  fail_unless (range->max.type == GST_RTSP_TIME_NOW);
  fail_unless (gst_rtsp_range_get_times (range, &min, &max));
  fail_unless (min == GST_CLOCK_TIME_NONE);
  fail_unless (max == GST_CLOCK_TIME_NONE);
  str = gst_rtsp_range_to_string (range);
  fail_unless_equals_string ("npt=-now", str);
  GST_DEBUG ("%s", str);
  g_free (str);
  gst_rtsp_range_free (range);

  fail_unless (gst_rtsp_range_parse ("npt=now-now", &range) == GST_RTSP_OK);
  fail_unless (range->unit == GST_RTSP_RANGE_NPT);
  fail_unless (range->min.type == GST_RTSP_TIME_NOW);
  fail_unless (range->max.type == GST_RTSP_TIME_NOW);
  str = gst_rtsp_range_to_string (range);
  fail_unless_equals_string ("npt=now-now", str);
  GST_DEBUG ("%s", str);
  g_free (str);
  gst_rtsp_range_free (range);

  fail_unless (gst_rtsp_range_parse ("npt=now-", &range) == GST_RTSP_OK);
  fail_unless (range->unit == GST_RTSP_RANGE_NPT);
  fail_unless (range->min.type == GST_RTSP_TIME_NOW);
  fail_unless (range->max.type == GST_RTSP_TIME_END);
  str = gst_rtsp_range_to_string (range);
  fail_unless_equals_string ("npt=now-", str);
  GST_DEBUG ("%s", str);
  g_free (str);
  gst_rtsp_range_free (range);

  fail_unless (gst_rtsp_range_parse ("npt=now-34.12", &range) == GST_RTSP_OK);
  fail_unless (range->unit == GST_RTSP_RANGE_NPT);
  fail_unless (range->min.type == GST_RTSP_TIME_NOW);
  fail_unless (range->max.type == GST_RTSP_TIME_SECONDS);
  fail_unless (range->max.seconds == 34.12);
  fail_unless (gst_rtsp_range_get_times (range, &min, &max));
  fail_unless (min == GST_CLOCK_TIME_NONE);
  fail_unless (max == 34120000000);
  str = gst_rtsp_range_to_string (range);
  fail_unless_equals_string ("npt=now-34.12", str);
  GST_DEBUG ("%s", str);
  g_free (str);
  gst_rtsp_range_free (range);

  fail_unless (gst_rtsp_range_parse ("npt=23,89-now", &range) == GST_RTSP_OK);
  fail_unless (range->unit == GST_RTSP_RANGE_NPT);
  fail_unless (range->min.type == GST_RTSP_TIME_SECONDS);
  fail_unless (range->min.seconds == 23.89);
  fail_unless (range->max.type == GST_RTSP_TIME_NOW);
  fail_unless (gst_rtsp_range_get_times (range, &min, &max));
  fail_unless (min == 23890000000);
  fail_unless (max == GST_CLOCK_TIME_NONE);
  str = gst_rtsp_range_to_string (range);
  fail_unless_equals_string ("npt=23.89-now", str);
  GST_DEBUG ("%s", str);
  g_free (str);
  gst_rtsp_range_free (range);

  fail_unless (gst_rtsp_range_parse ("npt=-12.09", &range) == GST_RTSP_OK);
  fail_unless (range->unit == GST_RTSP_RANGE_NPT);
  fail_unless (range->min.type == GST_RTSP_TIME_END);
  fail_unless (range->max.type == GST_RTSP_TIME_SECONDS);
  fail_unless (range->max.seconds == 12.09);
  fail_unless (gst_rtsp_range_get_times (range, &min, &max));
  fail_unless (min == GST_CLOCK_TIME_NONE);
  fail_unless (max == 12090000000);
  str = gst_rtsp_range_to_string (range);
  fail_unless_equals_string ("npt=-12.09", str);
  GST_DEBUG ("%s", str);
  g_free (str);
  gst_rtsp_range_free (range);

  fail_unless (gst_rtsp_range_parse ("npt=0-", &range) == GST_RTSP_OK);
  fail_unless (range->unit == GST_RTSP_RANGE_NPT);
  fail_unless (range->min.type == GST_RTSP_TIME_SECONDS);
  fail_unless (range->min.seconds == 0.0);
  fail_unless (range->max.type == GST_RTSP_TIME_END);
  fail_unless (gst_rtsp_range_get_times (range, &min, &max));
  fail_unless (min == 0);
  fail_unless (max == GST_CLOCK_TIME_NONE);
  str = gst_rtsp_range_to_string (range);
  fail_unless_equals_string ("npt=0-", str);
  GST_DEBUG ("%s", str);
  g_free (str);
  gst_rtsp_range_free (range);


  fail_unless (gst_rtsp_range_parse ("npt=1.123-", &range) == GST_RTSP_OK);
  fail_unless (range->unit == GST_RTSP_RANGE_NPT);
  fail_unless (range->min.type == GST_RTSP_TIME_SECONDS);
  fail_unless (range->min.seconds == 1.123);
  fail_unless (range->max.type == GST_RTSP_TIME_END);
  fail_unless (gst_rtsp_range_get_times (range, &min, &max));
  fail_unless (min == 1123000000);
  fail_unless (max == GST_CLOCK_TIME_NONE);
  str = gst_rtsp_range_to_string (range);
  fail_unless_equals_string ("npt=1.123-", str);
  GST_DEBUG ("%s", str);
  g_free (str);
  gst_rtsp_range_free (range);

  fail_unless (gst_rtsp_range_parse ("npt=10,20-20.10", &range) == GST_RTSP_OK);
  fail_unless (range->unit == GST_RTSP_RANGE_NPT);
  fail_unless (range->min.type == GST_RTSP_TIME_SECONDS);
  fail_unless (range->min.seconds == 10.20);
  fail_unless (range->max.type == GST_RTSP_TIME_SECONDS);
  fail_unless (range->max.seconds == 20.10);
  fail_unless (gst_rtsp_range_get_times (range, &min, &max));
  fail_unless (min == 10200000000);
  fail_unless (max == 20100000000);
  str = gst_rtsp_range_to_string (range);
  fail_unless_equals_string ("npt=10.2-20.1", str);
  GST_DEBUG ("%s", str);
  g_free (str);
  gst_rtsp_range_free (range);

  fail_unless (gst_rtsp_range_parse ("npt=500-15.001", &range) == GST_RTSP_OK);
  fail_unless (range->unit == GST_RTSP_RANGE_NPT);
  fail_unless (range->min.type == GST_RTSP_TIME_SECONDS);
  fail_unless (range->min.seconds == 500);
  fail_unless (range->max.type == GST_RTSP_TIME_SECONDS);
  fail_unless (range->max.seconds == 15.001);
  fail_unless (gst_rtsp_range_get_times (range, &min, &max));
  fail_unless (min == 500000000000);
  fail_unless (max == 15001000000);
  str = gst_rtsp_range_to_string (range);
  fail_unless_equals_string ("npt=500-15.001", str);
  GST_DEBUG ("%s", str);
  g_free (str);
  gst_rtsp_range_free (range);

  fail_unless (gst_rtsp_range_parse ("npt=20:34.23-",
          &range) == GST_RTSP_EINVAL);
  fail_unless (gst_rtsp_range_parse ("npt=10:20;34.23-",
          &range) == GST_RTSP_EINVAL);
  fail_unless (gst_rtsp_range_parse ("npt=0:4.23-", &range) == GST_RTSP_EINVAL);

  fail_unless (gst_rtsp_range_parse ("npt=20:12:34.23-21:45:00.01",
          &range) == GST_RTSP_OK);
  fail_unless (range->unit == GST_RTSP_RANGE_NPT);
  fail_unless (range->min.type == GST_RTSP_TIME_SECONDS);
  fail_unless (range->min.seconds == 72754.23);
  fail_unless (range->max.type == GST_RTSP_TIME_SECONDS);
  fail_unless (range->max.seconds == 78300.01);
  fail_unless (gst_rtsp_range_get_times (range, &min, &max));
  fail_unless (min == 72754230000000);
  fail_unless (max == 78300010000000);
  str = gst_rtsp_range_to_string (range);
  fail_unless_equals_string ("npt=72754.23-78300.01", str);
  GST_DEBUG ("%s", str);
  g_free (str);
  gst_rtsp_range_free (range);
}

GST_END_TEST;

GST_START_TEST (test_rtsp_range_smpte)
{
  GstClockTime min, max;
  GstRTSPTimeRange *range;
  gchar *str;

  fail_unless (gst_rtsp_range_parse ("smpte=", &range) == GST_RTSP_EINVAL);
  fail_unless (gst_rtsp_range_parse ("smpte=10:34:23",
          &range) == GST_RTSP_EINVAL);
  fail_unless (gst_rtsp_range_parse ("smpte=-", &range) == GST_RTSP_EINVAL);
  fail_unless (gst_rtsp_range_parse ("smpte=-12:09:34",
          &range) == GST_RTSP_EINVAL);
  fail_unless (gst_rtsp_range_parse ("smpte=12:09:34",
          &range) == GST_RTSP_EINVAL);

  fail_unless (gst_rtsp_range_parse ("smpte=00:00:00-", &range) == GST_RTSP_OK);
  fail_unless (range->unit == GST_RTSP_RANGE_SMPTE);
  fail_unless (range->min.type == GST_RTSP_TIME_FRAMES);
  fail_unless (range->min.seconds == 0.0);
  fail_unless (range->min2.frames == 0.0);
  fail_unless (range->max.type == GST_RTSP_TIME_END);
  fail_unless (gst_rtsp_range_get_times (range, &min, &max));
  fail_unless (min == 0);
  fail_unless (max == GST_CLOCK_TIME_NONE);
  str = gst_rtsp_range_to_string (range);
  fail_unless_equals_string ("smpte=0:00:00-", str);
  GST_DEBUG ("%s", str);
  g_free (str);
  gst_rtsp_range_free (range);

  fail_unless (gst_rtsp_range_parse ("smpte=10:34:23-20:12:09:20.89",
          &range) == GST_RTSP_OK);
  fail_unless (range->unit == GST_RTSP_RANGE_SMPTE);
  fail_unless (range->min.type == GST_RTSP_TIME_FRAMES);
  fail_unless (range->min.seconds == 38063.0);
  fail_unless (range->min2.frames == 0.0);
  fail_unless (range->max.type == GST_RTSP_TIME_FRAMES);
  fail_unless (range->max.seconds == 72729.0);
  fail_unless (range->max2.frames == 20.89);
  fail_unless (gst_rtsp_range_get_times (range, &min, &max));
  fail_unless (min == 38063000000000);
  /* 20.89 * GST_SECOND * 1001 / 30003 */
  fail_unless (max == 72729000000000 + 696959970);
  str = gst_rtsp_range_to_string (range);
  fail_unless_equals_string ("smpte=10:34:23-20:12:09:20.89", str);
  GST_DEBUG ("%s", str);
  g_free (str);
  gst_rtsp_range_free (range);

  fail_unless (gst_rtsp_range_parse ("smpte-25=10:34:23-20:12:09:20.89",
          &range) == GST_RTSP_OK);
  fail_unless (range->unit == GST_RTSP_RANGE_SMPTE_25);
  fail_unless (range->min.type == GST_RTSP_TIME_FRAMES);
  fail_unless (range->min.seconds == 38063.0);
  fail_unless (range->min2.frames == 0.0);
  fail_unless (range->max.type == GST_RTSP_TIME_FRAMES);
  fail_unless (range->max.seconds == 72729.0);
  fail_unless (range->max2.frames == 20.89);
  fail_unless (gst_rtsp_range_get_times (range, &min, &max));
  fail_unless (min == 38063000000000);
  GST_DEBUG ("%" GST_TIME_FORMAT, GST_TIME_ARGS (max));
  /* 20.89 * GST_SECOND * 1 / 25 */
  fail_unless (max == 72729000000000 + 835600000);
  str = gst_rtsp_range_to_string (range);
  GST_DEBUG ("%s", str);
  fail_unless_equals_string ("smpte-25=10:34:23-20:12:09:20.89", str);
  g_free (str);
  gst_rtsp_range_free (range);

  fail_unless (gst_rtsp_range_parse ("smpte-25=0:00:00:00.01-9:59:59:24.99",
          &range) == GST_RTSP_OK);
  fail_unless (range->unit == GST_RTSP_RANGE_SMPTE_25);
  fail_unless (range->min.type == GST_RTSP_TIME_FRAMES);
  fail_unless (range->min.seconds == 0);
  fail_unless (range->min2.frames == 0.01);
  fail_unless (range->max.type == GST_RTSP_TIME_FRAMES);
  fail_unless (range->max.seconds == 35999);
  fail_unless (range->max2.frames == 24.99);
  fail_unless (gst_rtsp_range_get_times (range, &min, &max));
  fail_unless (min == 400000);
  GST_DEBUG ("%" GST_TIME_FORMAT, GST_TIME_ARGS (max));
  /* 35999 + (24.99/25) */
  fail_unless (max == 35999999600000);
  str = gst_rtsp_range_to_string (range);
  GST_DEBUG ("%s", str);
  fail_unless_equals_string ("smpte-25=0:00:00:00.01-9:59:59:24.99", str);
  g_free (str);
  gst_rtsp_range_free (range);
}

GST_END_TEST;

GST_START_TEST (test_rtsp_range_clock)
{
  GstRTSPTimeRange *range;
  gchar *str;

  fail_unless (gst_rtsp_range_parse ("clock=", &range) == GST_RTSP_EINVAL);
  fail_unless (gst_rtsp_range_parse ("clock=20001010T120023Z",
          &range) == GST_RTSP_EINVAL);
  fail_unless (gst_rtsp_range_parse ("clock=-", &range) == GST_RTSP_EINVAL);
  fail_unless (gst_rtsp_range_parse ("clock=-20001010T120934Z",
          &range) == GST_RTSP_EINVAL);

  fail_unless (gst_rtsp_range_parse ("clock=20001010T122345Z-",
          &range) == GST_RTSP_OK);
  fail_unless (range->unit == GST_RTSP_RANGE_CLOCK);
  fail_unless (range->min.type == GST_RTSP_TIME_UTC);
  fail_unless (range->min2.year == 2000);
  fail_unless (range->min2.month == 10);
  fail_unless (range->min2.day == 10);
  fail_unless (range->min.seconds == 44625.0);
  fail_unless (range->max.type == GST_RTSP_TIME_END);
  str = gst_rtsp_range_to_string (range);
  fail_unless_equals_string ("clock=20001010T122345Z-", str);
  GST_DEBUG ("%s", str);
  g_free (str);
  gst_rtsp_range_free (range);

  fail_unless (gst_rtsp_range_parse
      ("clock=19700101T103423Z-30001230T201209.89Z", &range) == GST_RTSP_OK);
  fail_unless (range->unit == GST_RTSP_RANGE_CLOCK);
  fail_unless (range->min.type == GST_RTSP_TIME_UTC);
  fail_unless (range->min2.year == 1970);
  fail_unless (range->min2.month == 1);
  fail_unless (range->min2.day == 1);
  fail_unless (range->min.seconds == 38063.0);
  fail_unless (range->max.type == GST_RTSP_TIME_UTC);
  fail_unless (range->max2.year == 3000);
  fail_unless (range->max2.month == 12);
  fail_unless (range->max2.day == 30);
  fail_unless (range->max.seconds == 72729.89);
  str = gst_rtsp_range_to_string (range);
  fail_unless_equals_string ("clock=19700101T103423Z-30001230T201209.89Z", str);
  GST_DEBUG ("%s", str);
  g_free (str);
  gst_rtsp_range_free (range);
}

GST_END_TEST;


GST_START_TEST (test_rtsp_range_convert)
{
  GstRTSPTimeRange *range;
  gchar *str;

  fail_unless (gst_rtsp_range_parse ("npt=now-100", &range) == GST_RTSP_OK);
  fail_unless (gst_rtsp_range_convert_units (range, GST_RTSP_RANGE_NPT));
  fail_unless (!gst_rtsp_range_convert_units (range, GST_RTSP_RANGE_CLOCK));
  fail_unless (!gst_rtsp_range_convert_units (range, GST_RTSP_RANGE_SMPTE));
  str = gst_rtsp_range_to_string (range);
  fail_unless_equals_string (str, "npt=now-100");
  g_free (str);
  gst_rtsp_range_free (range);

  fail_unless (gst_rtsp_range_parse ("npt=0-100", &range) == GST_RTSP_OK);
  fail_unless (gst_rtsp_range_convert_units (range, GST_RTSP_RANGE_SMPTE));
  fail_unless (gst_rtsp_range_convert_units (range, GST_RTSP_RANGE_NPT));
  str = gst_rtsp_range_to_string (range);
  fail_unless_equals_string (str, "npt=0-100");
  g_free (str);
  gst_rtsp_range_free (range);

  fail_unless (gst_rtsp_range_parse ("npt=0-100", &range) == GST_RTSP_OK);
  fail_unless (gst_rtsp_range_convert_units (range, GST_RTSP_RANGE_SMPTE_25));
  fail_unless (gst_rtsp_range_convert_units (range, GST_RTSP_RANGE_NPT));
  str = gst_rtsp_range_to_string (range);
  fail_unless_equals_string (str, "npt=0-100");
  g_free (str);
  gst_rtsp_range_free (range);

  fail_unless (gst_rtsp_range_parse ("npt=0-100", &range) == GST_RTSP_OK);
  fail_unless (gst_rtsp_range_convert_units (range, GST_RTSP_RANGE_CLOCK));
  fail_unless (gst_rtsp_range_convert_units (range, GST_RTSP_RANGE_NPT));
  str = gst_rtsp_range_to_string (range);
  fail_unless_equals_string (str, "npt=0-100");
  g_free (str);
  gst_rtsp_range_free (range);

  fail_unless (gst_rtsp_range_parse ("smpte-25=10:07:00-10:07:33:05.01", &range)
      == GST_RTSP_OK);
  fail_unless (gst_rtsp_range_convert_units (range, GST_RTSP_RANGE_NPT));
  fail_unless (gst_rtsp_range_convert_units (range, GST_RTSP_RANGE_SMPTE_25));
  str = gst_rtsp_range_to_string (range);
  fail_unless_equals_string (str, "smpte-25=10:07:00-10:07:33:05.01");
  g_free (str);
  gst_rtsp_range_free (range);

  fail_unless (gst_rtsp_range_parse ("smpte=77:07:59-", &range)
      == GST_RTSP_OK);
  fail_unless (gst_rtsp_range_convert_units (range, GST_RTSP_RANGE_NPT));
  fail_unless (gst_rtsp_range_convert_units (range, GST_RTSP_RANGE_SMPTE));
  str = gst_rtsp_range_to_string (range);
  fail_unless_equals_string (str, "smpte=77:07:59-");
  g_free (str);
  gst_rtsp_range_free (range);

  fail_unless (gst_rtsp_range_parse ("smpte=10:07:00-10:07:33:05.01", &range)
      == GST_RTSP_OK);
  fail_unless (gst_rtsp_range_convert_units (range, GST_RTSP_RANGE_NPT));
  fail_unless (gst_rtsp_range_convert_units (range, GST_RTSP_RANGE_SMPTE));
  str = gst_rtsp_range_to_string (range);
  fail_unless_equals_string (str, "smpte=10:07:00-10:07:33:05.01");
  g_free (str);
  gst_rtsp_range_free (range);

  fail_unless (gst_rtsp_range_parse ("smpte-25=10:07:00-10:07:33:05.01", &range)
      == GST_RTSP_OK);
  fail_unless (gst_rtsp_range_convert_units (range, GST_RTSP_RANGE_CLOCK));
  fail_unless (gst_rtsp_range_convert_units (range, GST_RTSP_RANGE_SMPTE_25));
  str = gst_rtsp_range_to_string (range);
  fail_unless_equals_string (str, "smpte-25=10:07:00-10:07:33:05.01");
  g_free (str);
  gst_rtsp_range_free (range);

  fail_unless (gst_rtsp_range_parse ("smpte=10:07:00-10:07:33:05.01", &range)
      == GST_RTSP_OK);
  fail_unless (gst_rtsp_range_convert_units (range, GST_RTSP_RANGE_CLOCK));
  fail_unless (gst_rtsp_range_convert_units (range, GST_RTSP_RANGE_SMPTE));
  str = gst_rtsp_range_to_string (range);
  fail_unless_equals_string (str, "smpte=10:07:00-10:07:33:05.01");
  g_free (str);
  gst_rtsp_range_free (range);

  fail_unless (gst_rtsp_range_parse
      ("clock=20001010T120023Z-20320518T152245.12Z", &range)
      == GST_RTSP_OK);
  fail_unless (gst_rtsp_range_convert_units (range, GST_RTSP_RANGE_NPT));
  fail_unless (gst_rtsp_range_convert_units (range, GST_RTSP_RANGE_CLOCK));
  str = gst_rtsp_range_to_string (range);
  fail_unless_equals_string (str, "clock=20001010T120023Z-20320518T152245.12Z");
  g_free (str);
  gst_rtsp_range_free (range);

  fail_unless (gst_rtsp_range_parse
      ("clock=20001010T120023Z-20320518T152245.12Z", &range)
      == GST_RTSP_OK);
  fail_unless (gst_rtsp_range_convert_units (range, GST_RTSP_RANGE_SMPTE));
  fail_unless (gst_rtsp_range_convert_units (range, GST_RTSP_RANGE_CLOCK));
  str = gst_rtsp_range_to_string (range);
  fail_unless_equals_string (str, "clock=20001010T120023Z-20320518T152245.12Z");
  g_free (str);
  gst_rtsp_range_free (range);
}

GST_END_TEST;

GST_START_TEST (test_rtsp_message)
{
  GstRTSPMessage *msg;
  GstRTSPResult res;
  gchar *val = NULL;

  res = gst_rtsp_message_new_request (&msg, GST_RTSP_PLAY,
      "rtsp://foo.bar:8554/test");
  fail_unless_equals_int (res, GST_RTSP_OK);

  res = gst_rtsp_message_add_header (msg, GST_RTSP_HDR_CSEQ, "3");
  fail_unless_equals_int (res, GST_RTSP_OK);
  res = gst_rtsp_message_add_header (msg, GST_RTSP_HDR_SERVER, "GStreamer");
  fail_unless_equals_int (res, GST_RTSP_OK);
  res = gst_rtsp_message_add_header (msg, GST_RTSP_HDR_TRANSPORT,
      "RTP/AVP/TCP;unicast;interleaved=0-1");
  fail_unless_equals_int (res, GST_RTSP_OK);
  res = gst_rtsp_message_add_header (msg, GST_RTSP_HDR_SESSION, "xnb_NpaKEc");
  fail_unless_equals_int (res, GST_RTSP_OK);

  res = gst_rtsp_message_add_header_by_name (msg, "FOO99-Version", "bar.0");
  fail_unless_equals_int (res, GST_RTSP_OK);
  res = gst_rtsp_message_add_header_by_name (msg, "Custom", "value");
  fail_unless_equals_int (res, GST_RTSP_OK);
  res = gst_rtsp_message_add_header_by_name (msg, "FOO99-Version", "bar.1");
  fail_unless_equals_int (res, GST_RTSP_OK);
  res = gst_rtsp_message_add_header_by_name (msg, "FOO99-Version", "bar.2");
  fail_unless_equals_int (res, GST_RTSP_OK);

  /* make sure fields added via enum work as well */
  res = gst_rtsp_message_get_header_by_name (msg, "CSeq", &val, 0);
  fail_unless_equals_int (res, GST_RTSP_OK);
  fail_unless_equals_string (val, "3");
  res = gst_rtsp_message_get_header_by_name (msg, "CSeq", &val, 1);
  fail_unless_equals_int (res, GST_RTSP_ENOTIMPL);

  res = gst_rtsp_message_get_header (msg, GST_RTSP_HDR_CSEQ, &val, 0);
  fail_unless_equals_int (res, GST_RTSP_OK);
  fail_unless_equals_string (val, "3");
  res = gst_rtsp_message_get_header (msg, GST_RTSP_HDR_CSEQ, &val, 1);
  fail_unless_equals_int (res, GST_RTSP_ENOTIMPL);

  res = gst_rtsp_message_get_header_by_name (msg, "DoesNotExist", &val, 0);
  fail_unless_equals_int (res, GST_RTSP_ENOTIMPL);

  res = gst_rtsp_message_get_header_by_name (msg, "Custom", &val, 1);
  fail_unless_equals_int (res, GST_RTSP_ENOTIMPL);
  res = gst_rtsp_message_get_header_by_name (msg, "Custom", &val, 0);
  fail_unless_equals_int (res, GST_RTSP_OK);
  fail_unless_equals_string (val, "value");

  res = gst_rtsp_message_get_header_by_name (msg, "FOO99-Version", &val, 3);
  fail_unless_equals_int (res, GST_RTSP_ENOTIMPL);
  res = gst_rtsp_message_get_header_by_name (msg, "FOO99-Version", &val, 1);
  fail_unless_equals_int (res, GST_RTSP_OK);
  fail_unless_equals_string (val, "bar.1");
  res = gst_rtsp_message_get_header_by_name (msg, "FOO99-Version", &val, 2);
  fail_unless_equals_int (res, GST_RTSP_OK);
  fail_unless_equals_string (val, "bar.2");
  res = gst_rtsp_message_get_header_by_name (msg, "FOO99-Version", &val, 0);
  fail_unless_equals_int (res, GST_RTSP_OK);
  fail_unless_equals_string (val, "bar.0");

  res = gst_rtsp_message_remove_header_by_name (msg, "FOO99-Version", 3);
  fail_unless_equals_int (res, GST_RTSP_ENOTIMPL);
  res = gst_rtsp_message_remove_header_by_name (msg, "FOO99-Version", 1);
  fail_unless_equals_int (res, GST_RTSP_OK);

  res = gst_rtsp_message_get_header_by_name (msg, "FOO99-Version", &val, 2);
  fail_unless_equals_int (res, GST_RTSP_ENOTIMPL);

  /* 2 shifted to position 1 */
  res = gst_rtsp_message_get_header_by_name (msg, "FOO99-Version", &val, 1);
  fail_unless_equals_int (res, GST_RTSP_OK);
  fail_unless_equals_string (val, "bar.2");
  res = gst_rtsp_message_get_header_by_name (msg, "FOO99-Version", &val, 0);
  fail_unless_equals_int (res, GST_RTSP_OK);
  fail_unless_equals_string (val, "bar.0");

  /* remove all headers for a name */
  res = gst_rtsp_message_remove_header_by_name (msg, "FOO99-Version", -1);
  fail_unless_equals_int (res, GST_RTSP_OK);
  res = gst_rtsp_message_get_header_by_name (msg, "FOO99-Version", &val, 0);
  fail_unless_equals_int (res, GST_RTSP_ENOTIMPL);

  /* gst_rtsp_message_dump (msg); */

  res = gst_rtsp_message_free (msg);
  fail_unless_equals_int (res, GST_RTSP_OK);

  /* === */

  res = gst_rtsp_message_new_request (&msg, GST_RTSP_PLAY,
      "rtsp://foo.bar:8554/test");
  fail_unless_equals_int (res, GST_RTSP_OK);

  res = gst_rtsp_message_add_header_by_name (msg, "CSeq", "3");
  fail_unless_equals_int (res, GST_RTSP_OK);

  res = gst_rtsp_message_get_header (msg, GST_RTSP_HDR_CSEQ, &val, 0);
  fail_unless_equals_int (res, GST_RTSP_OK);
  fail_unless_equals_string (val, "3");

  val = NULL;
  res = gst_rtsp_message_get_header_by_name (msg, "cseq", &val, 0);
  fail_unless_equals_int (res, GST_RTSP_OK);
  fail_unless_equals_string (val, "3");

  res = gst_rtsp_message_free (msg);
  fail_unless_equals_int (res, GST_RTSP_OK);
}

GST_END_TEST;

GST_START_TEST (test_rtsp_message_auth_credentials)
{
  GstRTSPMessage *msg;
  GstRTSPResult res;
  GstRTSPAuthCredential **credentials;
  GstRTSPAuthCredential **credential;
  GstRTSPAuthParam **param;

  /* Simple basic auth, no params */
  res = gst_rtsp_message_new_request (&msg, GST_RTSP_PLAY,
      "rtsp://foo.bar:8554/test");
  fail_unless_equals_int (res, GST_RTSP_OK);
  res =
      gst_rtsp_message_add_header (msg, GST_RTSP_HDR_WWW_AUTHENTICATE, "Basic");
  credentials =
      gst_rtsp_message_parse_auth_credentials (msg,
      GST_RTSP_HDR_WWW_AUTHENTICATE);
  fail_unless (credentials != NULL);

  credential = credentials;
  fail_unless_equals_int ((*credential)->scheme, GST_RTSP_AUTH_BASIC);
  param = (*credential)->params;
  fail_unless (param == NULL);
  credential++;
  fail_unless (*credential == NULL);

  gst_rtsp_auth_credentials_free (credentials);
  res = gst_rtsp_message_free (msg);
  fail_unless_equals_int (res, GST_RTSP_OK);

  /* Simple basic auth, digest auth, no params */
  res = gst_rtsp_message_new_request (&msg, GST_RTSP_PLAY,
      "rtsp://foo.bar:8554/test");
  fail_unless_equals_int (res, GST_RTSP_OK);
  res =
      gst_rtsp_message_add_header (msg, GST_RTSP_HDR_WWW_AUTHENTICATE,
      "Basic Digest");
  credentials =
      gst_rtsp_message_parse_auth_credentials (msg,
      GST_RTSP_HDR_WWW_AUTHENTICATE);
  fail_unless (credentials != NULL);

  credential = credentials;
  fail_unless_equals_int ((*credential)->scheme, GST_RTSP_AUTH_BASIC);
  param = (*credential)->params;
  fail_unless (param == NULL);
  credential++;
  fail_unless_equals_int ((*credential)->scheme, GST_RTSP_AUTH_DIGEST);
  param = (*credential)->params;
  fail_unless (param == NULL);
  credential++;
  fail_unless (*credential == NULL);

  gst_rtsp_auth_credentials_free (credentials);
  res = gst_rtsp_message_free (msg);
  fail_unless_equals_int (res, GST_RTSP_OK);

  /* Simple basic auth */
  res = gst_rtsp_message_new_request (&msg, GST_RTSP_PLAY,
      "rtsp://foo.bar:8554/test");
  fail_unless_equals_int (res, GST_RTSP_OK);
  res =
      gst_rtsp_message_add_header (msg, GST_RTSP_HDR_WWW_AUTHENTICATE,
      "Basic foo=\"bar\", baz=foo");
  credentials =
      gst_rtsp_message_parse_auth_credentials (msg,
      GST_RTSP_HDR_WWW_AUTHENTICATE);
  fail_unless (credentials != NULL);

  credential = credentials;
  fail_unless_equals_int ((*credential)->scheme, GST_RTSP_AUTH_BASIC);
  param = (*credential)->params;
  fail_unless (param != NULL);
  fail_unless (*param != NULL);
  fail_unless_equals_string ((*param)->name, "foo");
  fail_unless_equals_string ((*param)->value, "bar");
  param++;
  fail_unless (*param != NULL);
  fail_unless_equals_string ((*param)->name, "baz");
  fail_unless_equals_string ((*param)->value, "foo");
  param++;
  fail_unless (*param == NULL);
  credential++;
  fail_unless (*credential == NULL);

  gst_rtsp_auth_credentials_free (credentials);
  res = gst_rtsp_message_free (msg);
  fail_unless_equals_int (res, GST_RTSP_OK);

  /* Two simple basic auth headers */
  res = gst_rtsp_message_new_request (&msg, GST_RTSP_PLAY,
      "rtsp://foo.bar:8554/test");
  fail_unless_equals_int (res, GST_RTSP_OK);
  res =
      gst_rtsp_message_add_header (msg, GST_RTSP_HDR_WWW_AUTHENTICATE,
      "Basic foo=\"bar\", baz=foo");
  res =
      gst_rtsp_message_add_header (msg, GST_RTSP_HDR_WWW_AUTHENTICATE,
      "Basic foo1=\"bar\", baz1=foo");
  credentials =
      gst_rtsp_message_parse_auth_credentials (msg,
      GST_RTSP_HDR_WWW_AUTHENTICATE);
  fail_unless (credentials != NULL);

  credential = credentials;
  fail_unless_equals_int ((*credential)->scheme, GST_RTSP_AUTH_BASIC);
  param = (*credential)->params;
  fail_unless (param != NULL);
  fail_unless (*param != NULL);
  fail_unless_equals_string ((*param)->name, "foo");
  fail_unless_equals_string ((*param)->value, "bar");
  param++;
  fail_unless (*param != NULL);
  fail_unless_equals_string ((*param)->name, "baz");
  fail_unless_equals_string ((*param)->value, "foo");
  param++;
  fail_unless (*param == NULL);
  credential++;
  fail_unless_equals_int ((*credential)->scheme, GST_RTSP_AUTH_BASIC);
  param = (*credential)->params;
  fail_unless (param != NULL);
  fail_unless (*param != NULL);
  fail_unless_equals_string ((*param)->name, "foo1");
  fail_unless_equals_string ((*param)->value, "bar");
  param++;
  fail_unless (*param != NULL);
  fail_unless_equals_string ((*param)->name, "baz1");
  fail_unless_equals_string ((*param)->value, "foo");
  param++;
  fail_unless (*param == NULL);
  credential++;
  fail_unless (*credential == NULL);

  gst_rtsp_auth_credentials_free (credentials);
  res = gst_rtsp_message_free (msg);
  fail_unless_equals_int (res, GST_RTSP_OK);

  /* Simple basic auth, digest auth in one header */
  res = gst_rtsp_message_new_request (&msg, GST_RTSP_PLAY,
      "rtsp://foo.bar:8554/test");
  fail_unless_equals_int (res, GST_RTSP_OK);
  res =
      gst_rtsp_message_add_header (msg, GST_RTSP_HDR_WWW_AUTHENTICATE,
      "Basic foo=\"bar\", baz=foo Digest foo1=\"bar\", baz1=foo");
  credentials =
      gst_rtsp_message_parse_auth_credentials (msg,
      GST_RTSP_HDR_WWW_AUTHENTICATE);
  fail_unless (credentials != NULL);

  credential = credentials;
  fail_unless_equals_int ((*credential)->scheme, GST_RTSP_AUTH_BASIC);
  param = (*credential)->params;
  fail_unless (param != NULL);
  fail_unless (*param != NULL);
  fail_unless_equals_string ((*param)->name, "foo");
  fail_unless_equals_string ((*param)->value, "bar");
  param++;
  fail_unless (*param != NULL);
  fail_unless_equals_string ((*param)->name, "baz");
  fail_unless_equals_string ((*param)->value, "foo");
  param++;
  fail_unless (*param == NULL);
  credential++;
  fail_unless_equals_int ((*credential)->scheme, GST_RTSP_AUTH_DIGEST);
  param = (*credential)->params;
  fail_unless (param != NULL);
  fail_unless (*param != NULL);
  fail_unless_equals_string ((*param)->name, "foo1");
  fail_unless_equals_string ((*param)->value, "bar");
  param++;
  fail_unless (*param != NULL);
  fail_unless_equals_string ((*param)->name, "baz1");
  fail_unless_equals_string ((*param)->value, "foo");
  param++;
  fail_unless (*param == NULL);
  credential++;
  fail_unless (*credential == NULL);

  gst_rtsp_auth_credentials_free (credentials);
  res = gst_rtsp_message_free (msg);
  fail_unless_equals_int (res, GST_RTSP_OK);

  /* Simple basic auth, digest auth in one header, with random commas and spaces */
  res = gst_rtsp_message_new_request (&msg, GST_RTSP_PLAY,
      "rtsp://foo.bar:8554/test");
  fail_unless_equals_int (res, GST_RTSP_OK);
  res =
      gst_rtsp_message_add_header (msg, GST_RTSP_HDR_WWW_AUTHENTICATE,
      "Basic     foo=\"bar\",, , baz=foo, Digest , foo1=\"bar\",, baz1=foo");
  credentials =
      gst_rtsp_message_parse_auth_credentials (msg,
      GST_RTSP_HDR_WWW_AUTHENTICATE);
  fail_unless (credentials != NULL);

  credential = credentials;
  fail_unless_equals_int ((*credential)->scheme, GST_RTSP_AUTH_BASIC);
  param = (*credential)->params;
  fail_unless (param != NULL);
  fail_unless (*param != NULL);
  fail_unless_equals_string ((*param)->name, "foo");
  fail_unless_equals_string ((*param)->value, "bar");
  param++;
  fail_unless (*param != NULL);
  fail_unless_equals_string ((*param)->name, "baz");
  fail_unless_equals_string ((*param)->value, "foo");
  param++;
  fail_unless (*param == NULL);
  credential++;
  fail_unless_equals_int ((*credential)->scheme, GST_RTSP_AUTH_DIGEST);
  param = (*credential)->params;
  fail_unless (param != NULL);
  fail_unless (*param != NULL);
  fail_unless_equals_string ((*param)->name, "foo1");
  fail_unless_equals_string ((*param)->value, "bar");
  param++;
  fail_unless (*param != NULL);
  fail_unless_equals_string ((*param)->name, "baz1");
  fail_unless_equals_string ((*param)->value, "foo");
  param++;
  fail_unless (*param == NULL);
  credential++;
  fail_unless (*credential == NULL);

  gst_rtsp_auth_credentials_free (credentials);
  res = gst_rtsp_message_free (msg);
  fail_unless_equals_int (res, GST_RTSP_OK);

  /* Simple basic auth */
  res = gst_rtsp_message_new_request (&msg, GST_RTSP_PLAY,
      "rtsp://foo.bar:8554/test");
  fail_unless_equals_int (res, GST_RTSP_OK);
  res =
      gst_rtsp_message_add_header (msg, GST_RTSP_HDR_AUTHORIZATION,
      "Basic foobarbaz");
  credentials =
      gst_rtsp_message_parse_auth_credentials (msg, GST_RTSP_HDR_AUTHORIZATION);
  fail_unless (credentials != NULL);

  credential = credentials;
  fail_unless_equals_int ((*credential)->scheme, GST_RTSP_AUTH_BASIC);
  param = (*credential)->params;
  fail_unless (param == NULL);
  fail_unless_equals_string ((*credential)->authorization, "foobarbaz");
  credential++;
  fail_unless (*credential == NULL);

  gst_rtsp_auth_credentials_free (credentials);
  res = gst_rtsp_message_free (msg);
  fail_unless_equals_int (res, GST_RTSP_OK);

  /* Simple digest auth */
  res = gst_rtsp_message_new_request (&msg, GST_RTSP_PLAY,
      "rtsp://foo.bar:8554/test");
  fail_unless_equals_int (res, GST_RTSP_OK);
  res =
      gst_rtsp_message_add_header (msg, GST_RTSP_HDR_AUTHORIZATION,
      "Digest foo=\"bar\" baz=foo");
  credentials =
      gst_rtsp_message_parse_auth_credentials (msg, GST_RTSP_HDR_AUTHORIZATION);
  fail_unless (credentials != NULL);

  credential = credentials;
  fail_unless_equals_int ((*credential)->scheme, GST_RTSP_AUTH_DIGEST);
  param = (*credential)->params;
  fail_unless (param != NULL);
  fail_unless (*param != NULL);
  fail_unless_equals_string ((*param)->name, "foo");
  fail_unless_equals_string ((*param)->value, "bar");
  param++;
  fail_unless (*param != NULL);
  fail_unless_equals_string ((*param)->name, "baz");
  fail_unless_equals_string ((*param)->value, "foo");
  param++;
  fail_unless (*param == NULL);
  credential++;
  fail_unless (*credential == NULL);

  gst_rtsp_auth_credentials_free (credentials);
  res = gst_rtsp_message_free (msg);
  fail_unless_equals_int (res, GST_RTSP_OK);
}

GST_END_TEST;

GST_START_TEST (test_rtsp_message_auth_credentials_boxed)
{
  GstRTSPAuthCredential **credentials, *credentials2;
  GstRTSPAuthParam *param2;
  GstRTSPMessage *msg;
  GstRTSPResult res;

  res = gst_rtsp_message_new_request (&msg, GST_RTSP_PLAY,
      "rtsp://foo.bar:8554/test");
  fail_unless_equals_int (res, GST_RTSP_OK);
  res =
      gst_rtsp_message_add_header (msg, GST_RTSP_HDR_WWW_AUTHENTICATE,
      "Basic foo=\"bar\", baz=foo");
  res =
      gst_rtsp_message_add_header (msg, GST_RTSP_HDR_WWW_AUTHENTICATE,
      "Basic foo1=\"bar\", baz1=foo");
  credentials =
      gst_rtsp_message_parse_auth_credentials (msg,
      GST_RTSP_HDR_WWW_AUTHENTICATE);

  credentials2 = g_boxed_copy (GST_TYPE_RTSP_AUTH_CREDENTIAL, credentials[0]);
  gst_rtsp_auth_credentials_free (credentials);
  gst_rtsp_message_free (msg);

  param2 = g_boxed_copy (GST_TYPE_RTSP_AUTH_PARAM, credentials2->params[0]);
  g_boxed_free (GST_TYPE_RTSP_AUTH_CREDENTIAL, credentials2);
  g_boxed_free (GST_TYPE_RTSP_AUTH_PARAM, param2);
}

GST_END_TEST;

static Suite *
rtsp_suite (void)
{
  Suite *s = suite_create ("rtsp support library");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_rtsp_url_basic);
  tcase_add_test (tc_chain, test_rtsp_url_query);
  tcase_add_test (tc_chain, test_rtsp_url_components_1);
  tcase_add_test (tc_chain, test_rtsp_url_components_2);
  tcase_add_test (tc_chain, test_rtsp_url_components_3);
  tcase_add_test (tc_chain, test_rtsp_range_npt);
  tcase_add_test (tc_chain, test_rtsp_range_smpte);
  tcase_add_test (tc_chain, test_rtsp_range_clock);
  tcase_add_test (tc_chain, test_rtsp_range_convert);
  tcase_add_test (tc_chain, test_rtsp_message);
  tcase_add_test (tc_chain, test_rtsp_message_auth_credentials);
  tcase_add_test (tc_chain, test_rtsp_message_auth_credentials_boxed);

  return s;
}

GST_CHECK_MAIN (rtsp);
