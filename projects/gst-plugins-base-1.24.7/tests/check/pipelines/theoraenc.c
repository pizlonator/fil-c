/* GStreamer
 *
 * unit test for theoraenc
 *
 * Copyright (C) 2006 Andy Wingo <wingo at pobox.com>
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
#include <gst/check/gstbufferstraw.h>

#include <theora/theora.h>

#ifndef GST_DISABLE_PARSE

#define TIMESTAMP_OFFSET G_GINT64_CONSTANT(3249870963)
#define FRAMERATE 10

/* I know all of these have a shift of 6 bits */
#define GRANULEPOS_SHIFT 6


#define check_buffer_is_header(buffer,is_header) \
  fail_unless (GST_BUFFER_FLAG_IS_SET (buffer,   \
          GST_BUFFER_FLAG_HEADER) == is_header, \
      "GST_BUFFER_IN_CAPS is set to %d but expected %d", \
      GST_BUFFER_FLAG_IS_SET (buffer, GST_BUFFER_FLAG_HEADER), is_header)

#define check_buffer_timestamp(buffer,timestamp) \
  fail_unless (GST_BUFFER_TIMESTAMP (buffer) == timestamp, \
      "expected timestamp %" GST_TIME_FORMAT \
      ", but got timestamp %" GST_TIME_FORMAT, \
      GST_TIME_ARGS (timestamp), GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buffer)))

#define check_buffer_duration(buffer,duration) \
  fail_unless (GST_BUFFER_DURATION (buffer) == duration, \
      "expected duration %" GST_TIME_FORMAT \
      ", but got duration %" GST_TIME_FORMAT, \
      GST_TIME_ARGS (duration), GST_TIME_ARGS (GST_BUFFER_DURATION (buffer)))

static gboolean old_libtheora;

static void
check_libtheora (void)
{
  old_libtheora = (theora_version_number () <= 0x00030200);
}

static void
check_buffer_granulepos (GstBuffer * buffer, gint64 granulepos)
{
  GstClockTime clocktime;
  int framecount;

  /* With old versions of libtheora, the granulepos represented the
   * start time, not end time. Adapt for that. */
  if (old_libtheora) {
    if (granulepos >> GRANULEPOS_SHIFT)
      granulepos -= 1 << GRANULEPOS_SHIFT;
    else if (granulepos)
      granulepos -= 1;
  }

  fail_unless (GST_BUFFER_OFFSET_END (buffer) == granulepos,
      "expected granulepos %" G_GUINT64_FORMAT
      ", but got granulepos %" G_GUINT64_FORMAT,
      granulepos, GST_BUFFER_OFFSET_END (buffer));

  /* contrary to what we record as TIMESTAMP, we can use OFFSET to check
   * the granulepos correctly here */
  framecount = GST_BUFFER_OFFSET_END (buffer);
  framecount = granulepos >> GRANULEPOS_SHIFT;
  framecount += granulepos & ((1 << GRANULEPOS_SHIFT) - 1);
  clocktime = gst_util_uint64_scale (framecount, GST_SECOND, FRAMERATE);

  fail_unless (clocktime == GST_BUFFER_OFFSET (buffer),
      "expected OFFSET set to clocktime %" GST_TIME_FORMAT
      ", but got %" GST_TIME_FORMAT,
      GST_TIME_ARGS (clocktime), GST_TIME_ARGS (GST_BUFFER_OFFSET (buffer)));
}

/* this check is here to check that the granulepos we derive from the
   timestamp is about correct. This is "about correct" because you can't
   precisely go from timestamp to granulepos due to the downward-rounding
   characteristics of gst_util_uint64_scale, so you check if granulepos is
   equal to the number, or the number plus one. */
/* should be from_endtime, but theora's granulepos mapping is "special" */
static void
check_buffer_granulepos_from_starttime (GstBuffer * buffer,
    GstClockTime starttime)
{
  gint64 granulepos, expected, framecount;

  granulepos = GST_BUFFER_OFFSET_END (buffer);
  /* Now convert to 'granulepos for start time', depending on libtheora 
   * version */
  if (!old_libtheora) {
    if (granulepos & ((1 << GRANULEPOS_SHIFT) - 1))
      granulepos -= 1;
    else if (granulepos)
      granulepos -= 1 << GRANULEPOS_SHIFT;
  }

  framecount = granulepos >> GRANULEPOS_SHIFT;
  framecount += granulepos & ((1 << GRANULEPOS_SHIFT) - 1);
  expected = gst_util_uint64_scale (starttime, FRAMERATE, GST_SECOND);

  fail_unless (framecount == expected || framecount == expected + 1,
      "expected frame count %" G_GUINT64_FORMAT
      " or %" G_GUINT64_FORMAT
      ", but got frame count %" G_GUINT64_FORMAT,
      expected, expected + 1, framecount);
}

GST_START_TEST (test_granulepos_offset)
{
  GstElement *bin;
  GstPad *pad;
  gchar *pipe_str;
  GstBuffer *buffer;
  GError *error = NULL;

  pipe_str = g_strdup_printf ("videotestsrc timestamp-offset=%" G_GUINT64_FORMAT
      " num-buffers=10 ! video/x-raw,format=(string)I420,framerate=10/1"
      " ! theoraenc ! fakesink name=fs0", TIMESTAMP_OFFSET);

  bin = gst_parse_launch (pipe_str, &error);
  fail_unless (bin != NULL, "Error parsing pipeline: %s",
      error ? error->message : "(invalid error)");
  g_free (pipe_str);

  /* get the pad */
  {
    GstElement *sink = gst_bin_get_by_name (GST_BIN (bin), "fs0");

    fail_unless (sink != NULL, "Could not get fakesink out of bin");
    pad = gst_element_get_static_pad (sink, "sink");
    fail_unless (pad != NULL, "Could not get pad out of fakesink");
    gst_object_unref (sink);
  }

  gst_buffer_straw_start_pipeline (bin, pad);

  /* header packets should have timestamp == NONE, granulepos 0, IN_CAPS */
  buffer = gst_buffer_straw_get_buffer (bin, pad);
  check_buffer_timestamp (buffer, GST_CLOCK_TIME_NONE);
  check_buffer_duration (buffer, GST_CLOCK_TIME_NONE);
  check_buffer_granulepos (buffer, 0);
  check_buffer_is_header (buffer, TRUE);
  gst_buffer_unref (buffer);

  buffer = gst_buffer_straw_get_buffer (bin, pad);
  check_buffer_timestamp (buffer, GST_CLOCK_TIME_NONE);
  check_buffer_duration (buffer, GST_CLOCK_TIME_NONE);
  check_buffer_granulepos (buffer, 0);
  check_buffer_is_header (buffer, TRUE);
  gst_buffer_unref (buffer);

  buffer = gst_buffer_straw_get_buffer (bin, pad);
  check_buffer_timestamp (buffer, GST_CLOCK_TIME_NONE);
  check_buffer_duration (buffer, GST_CLOCK_TIME_NONE);
  check_buffer_granulepos (buffer, 0);
  check_buffer_is_header (buffer, TRUE);
  gst_buffer_unref (buffer);

  {
    GstClockTime next_timestamp;
    gint64 last_granulepos;

    /* first buffer should have timestamp of TIMESTAMP_OFFSET, granulepos to
     * match the timestamp of the end of the last sample in the output buffer.
     * Note that one cannot go timestamp->granulepos->timestamp and get the
     * same value due to loss of precision with granulepos. theoraenc does
     * take care to timestamp correctly based on the offset of the input data
     * however, so it does do sub-granulepos timestamping. */
    buffer = gst_buffer_straw_get_buffer (bin, pad);
    last_granulepos = GST_BUFFER_OFFSET_END (buffer);
    check_buffer_timestamp (buffer, TIMESTAMP_OFFSET);
    /* don't really have a good way of checking duration... */
    check_buffer_granulepos_from_starttime (buffer, TIMESTAMP_OFFSET);
    check_buffer_is_header (buffer, FALSE);

    next_timestamp = TIMESTAMP_OFFSET + GST_BUFFER_DURATION (buffer);

    gst_buffer_unref (buffer);

    /* check continuity with the next buffer */
    buffer = gst_buffer_straw_get_buffer (bin, pad);
    check_buffer_timestamp (buffer, next_timestamp);
    check_buffer_duration (buffer,
        gst_util_uint64_scale (GST_BUFFER_OFFSET_END (buffer), GST_SECOND,
            FRAMERATE)
        - gst_util_uint64_scale (last_granulepos, GST_SECOND, FRAMERATE));
    check_buffer_granulepos_from_starttime (buffer, next_timestamp);
    check_buffer_is_header (buffer, FALSE);

    gst_buffer_unref (buffer);
  }

  gst_buffer_straw_stop_pipeline (bin, pad);

  gst_object_unref (pad);
  gst_object_unref (bin);
}

GST_END_TEST;

GST_START_TEST (test_continuity)
{
  GstElement *bin;
  GstPad *pad;
  gchar *pipe_str;
  GstBuffer *buffer;
  GError *error = NULL;

  pipe_str = g_strdup_printf ("videotestsrc num-buffers=10"
      " ! video/x-raw,format=(string)I420,framerate=10/1"
      " ! theoraenc ! fakesink name=fs0");

  bin = gst_parse_launch (pipe_str, &error);
  fail_unless (bin != NULL, "Error parsing pipeline: %s",
      error ? error->message : "(invalid error)");
  g_free (pipe_str);

  /* get the pad */
  {
    GstElement *sink = gst_bin_get_by_name (GST_BIN (bin), "fs0");

    fail_unless (sink != NULL, "Could not get fakesink out of bin");
    pad = gst_element_get_static_pad (sink, "sink");
    fail_unless (pad != NULL, "Could not get pad out of fakesink");
    gst_object_unref (sink);
  }

  gst_buffer_straw_start_pipeline (bin, pad);

  /* header packets should have timestamp == NONE, granulepos 0 */
  buffer = gst_buffer_straw_get_buffer (bin, pad);
  check_buffer_timestamp (buffer, GST_CLOCK_TIME_NONE);
  check_buffer_duration (buffer, GST_CLOCK_TIME_NONE);
  check_buffer_granulepos (buffer, 0);
  check_buffer_is_header (buffer, TRUE);
  gst_buffer_unref (buffer);

  buffer = gst_buffer_straw_get_buffer (bin, pad);
  check_buffer_timestamp (buffer, GST_CLOCK_TIME_NONE);
  check_buffer_duration (buffer, GST_CLOCK_TIME_NONE);
  check_buffer_granulepos (buffer, 0);
  check_buffer_is_header (buffer, TRUE);
  gst_buffer_unref (buffer);

  buffer = gst_buffer_straw_get_buffer (bin, pad);
  check_buffer_timestamp (buffer, GST_CLOCK_TIME_NONE);
  check_buffer_duration (buffer, GST_CLOCK_TIME_NONE);
  check_buffer_granulepos (buffer, 0);
  check_buffer_is_header (buffer, TRUE);
  gst_buffer_unref (buffer);

  {
    GstClockTime next_timestamp;

    /* first buffer should have timestamp of TIMESTAMP_OFFSET, granulepos to
     * match the timestamp of the end of the last sample in the output buffer.
     * Note that one cannot go timestamp->granulepos->timestamp and get the
     * same value due to loss of precision with granulepos. theoraenc does
     * take care to timestamp correctly based on the offset of the input data
     * however, so it does do sub-granulepos timestamping. */
    buffer = gst_buffer_straw_get_buffer (bin, pad);
    check_buffer_timestamp (buffer, 0);
    /* plain division because I know the answer is exact */
    check_buffer_duration (buffer, GST_SECOND / 10);
    check_buffer_granulepos (buffer, 1 << GRANULEPOS_SHIFT);
    check_buffer_is_header (buffer, FALSE);

    next_timestamp = GST_BUFFER_DURATION (buffer);

    gst_buffer_unref (buffer);

    /* check continuity with the next buffer */
    buffer = gst_buffer_straw_get_buffer (bin, pad);
    check_buffer_timestamp (buffer, next_timestamp);
    check_buffer_duration (buffer, GST_SECOND / 10);
    check_buffer_granulepos (buffer, (1 << GRANULEPOS_SHIFT) | 1);
    check_buffer_is_header (buffer, FALSE);

    gst_buffer_unref (buffer);
  }

  gst_buffer_straw_stop_pipeline (bin, pad);

  gst_object_unref (pad);
  gst_object_unref (bin);
}

GST_END_TEST;

#endif /* #ifndef GST_DISABLE_PARSE */

static Suite *
theoraenc_suite (void)
{
  Suite *s = suite_create ("theoraenc");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);

  check_libtheora ();

#ifndef GST_DISABLE_PARSE
  tcase_add_test (tc_chain, test_granulepos_offset);
  tcase_add_test (tc_chain, test_continuity);
#endif

  return s;
}

GST_CHECK_MAIN (theoraenc);
