/* GStreamer
 *
 * unit tests for audio support library
 *
 * Copyright (C) 2006 Tim-Philipp Müller <tim centricular net>
 * Copyright (C) 2011 Sebastian Dröge <sebastian.droege@collabora.co.uk>
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

#include <gst/audio/audio.h>
#include <string.h>

static GstBuffer *
make_buffer (guint8 ** _data)
{
  GstBuffer *buf = gst_buffer_new ();
  guint8 *data = (guint8 *) g_malloc (1000);

  gst_buffer_append_memory (buf,
      gst_memory_new_wrapped (0, data, 1000, 0, 1000, data, g_free));
  if (_data)
    *_data = data;
  return buf;
}

static void
setup_segment (GstSegment * s, GstFormat fmt, guint64 start, guint64 stop,
    guint64 stream_time)
{
  gst_segment_init (s, fmt);
  s->start = start;
  s->stop = stop;
  s->time = stream_time;
}

GST_START_TEST (test_buffer_clip_unsupported_format)
{
  GstSegment s;
  GstBuffer *buf;
  guint8 *data;

  /* If the format is not TIME or DEFAULT it should assert()
   * FIXME: check if return value is the same as the input buffer.
   *        probably can't be done because the assert() does a SIGABRT.
   */
  buf = make_buffer (&data);
  setup_segment (&s, GST_FORMAT_PERCENT, 0, 10, 0);

  GST_BUFFER_TIMESTAMP (buf) = 0 * GST_SECOND;
  GST_BUFFER_DURATION (buf) = 0;
  GST_BUFFER_OFFSET (buf) = GST_BUFFER_OFFSET_NONE;
  GST_BUFFER_OFFSET_END (buf) = GST_BUFFER_OFFSET_NONE;

  ASSERT_CRITICAL (gst_audio_buffer_clip (buf, &s, 100, 1));

  gst_buffer_unref (buf);
}

GST_END_TEST;

GST_START_TEST (test_buffer_clip_time_start_and_stop)
{
  GstSegment s;
  GstBuffer *buf;
  GstBuffer *ret;
  GstMapInfo map;
  guint8 *data;

  /* Clip start and end */
  buf = make_buffer (&data);
  setup_segment (&s, GST_FORMAT_TIME, 4 * GST_SECOND, 8 * GST_SECOND,
      4 * GST_SECOND);

  GST_BUFFER_TIMESTAMP (buf) = 2 * GST_SECOND;
  GST_BUFFER_DURATION (buf) = 10 * GST_SECOND;
  GST_BUFFER_OFFSET (buf) = 200;
  GST_BUFFER_OFFSET_END (buf) = 1200;

  ret = gst_audio_buffer_clip (buf, &s, 100, 1);
  fail_unless (ret != NULL);

  fail_unless_equals_int64 (GST_BUFFER_TIMESTAMP (ret), 4 * GST_SECOND);
  fail_unless_equals_int64 (GST_BUFFER_DURATION (ret), 4 * GST_SECOND);
  fail_unless_equals_int64 (GST_BUFFER_OFFSET (ret), 400);
  fail_unless_equals_int64 (GST_BUFFER_OFFSET_END (ret), 800);
  gst_buffer_map (ret, &map, GST_MAP_READ);
  fail_unless (map.data == data + 200);
  fail_unless (map.size == 400);
  gst_buffer_unmap (ret, &map);

  gst_buffer_unref (ret);
}

GST_END_TEST;

GST_START_TEST (test_buffer_clip_time_start_and_stop_planar)
{
  GstSegment s;
  GstBuffer *buf;
  GstBuffer *ret;
  GstAudioInfo info;
  GstAudioBuffer abuf;
  guint8 *data;

  /* Clip start and end */
  buf = make_buffer (&data);

  gst_audio_info_init (&info);
  gst_audio_info_set_format (&info, GST_AUDIO_FORMAT_S8, 44100, 1, NULL);
  info.layout = GST_AUDIO_LAYOUT_NON_INTERLEAVED;
  gst_buffer_add_audio_meta (buf, &info, 1000, NULL);

  setup_segment (&s, GST_FORMAT_TIME, 4 * GST_SECOND, 8 * GST_SECOND,
      4 * GST_SECOND);

  GST_BUFFER_TIMESTAMP (buf) = 2 * GST_SECOND;
  GST_BUFFER_DURATION (buf) = 10 * GST_SECOND;
  GST_BUFFER_OFFSET (buf) = 200;
  GST_BUFFER_OFFSET_END (buf) = 1200;

  ret = gst_audio_buffer_clip (buf, &s, 100, 1);
  fail_unless (ret != NULL);

  fail_unless_equals_int64 (GST_BUFFER_TIMESTAMP (ret), 4 * GST_SECOND);
  fail_unless_equals_int64 (GST_BUFFER_DURATION (ret), 4 * GST_SECOND);
  fail_unless_equals_int64 (GST_BUFFER_OFFSET (ret), 400);
  fail_unless_equals_int64 (GST_BUFFER_OFFSET_END (ret), 800);
  gst_audio_buffer_map (&abuf, &info, ret, GST_MAP_READ);
  fail_unless_equals_int (abuf.n_planes, 1);
  fail_unless_equals_int (GST_AUDIO_BUFFER_PLANE_SIZE (&abuf), 400);
  fail_unless_equals_pointer (abuf.planes[0], data + 200);
  gst_audio_buffer_unmap (&abuf);

  gst_buffer_unref (ret);
}

GST_END_TEST;

GST_START_TEST (test_buffer_clip_time_start)
{
  GstSegment s;
  GstBuffer *buf;
  GstBuffer *ret;
  GstMapInfo map;
  guint8 *data;

  /* Clip only start */
  buf = make_buffer (&data);
  setup_segment (&s, GST_FORMAT_TIME, 4 * GST_SECOND, 12 * GST_SECOND,
      4 * GST_SECOND);

  GST_BUFFER_TIMESTAMP (buf) = 2 * GST_SECOND;
  GST_BUFFER_DURATION (buf) = 10 * GST_SECOND;
  GST_BUFFER_OFFSET (buf) = 200;
  GST_BUFFER_OFFSET_END (buf) = 1200;

  ret = gst_audio_buffer_clip (buf, &s, 100, 1);
  fail_unless (ret != NULL);

  fail_unless_equals_int64 (GST_BUFFER_TIMESTAMP (ret), 4 * GST_SECOND);
  fail_unless_equals_int64 (GST_BUFFER_DURATION (ret), 8 * GST_SECOND);
  fail_unless_equals_int64 (GST_BUFFER_OFFSET (ret), 400);
  fail_unless_equals_int64 (GST_BUFFER_OFFSET_END (ret), 1200);
  gst_buffer_map (ret, &map, GST_MAP_READ);
  fail_unless (map.data == data + 200);
  fail_unless (map.size == 800);
  gst_buffer_unmap (ret, &map);

  gst_buffer_unref (ret);
}

GST_END_TEST;

GST_START_TEST (test_buffer_clip_time_start_planar)
{
  GstSegment s;
  GstBuffer *buf;
  GstBuffer *ret;
  GstAudioInfo info;
  GstAudioMeta *meta;
  GstAudioBuffer abuf;
  guint8 *data;

  /* Clip only start */
  buf = make_buffer (&data);

  gst_audio_info_init (&info);
  gst_audio_info_set_format (&info, GST_AUDIO_FORMAT_S8, 44100, 2, NULL);
  info.layout = GST_AUDIO_LAYOUT_NON_INTERLEAVED;
  gst_buffer_add_audio_meta (buf, &info, 500, NULL);

  setup_segment (&s, GST_FORMAT_TIME, 4 * GST_SECOND, 12 * GST_SECOND,
      4 * GST_SECOND);

  GST_BUFFER_TIMESTAMP (buf) = 2 * GST_SECOND;
  GST_BUFFER_DURATION (buf) = 5 * GST_SECOND;
  GST_BUFFER_OFFSET (buf) = 200;
  GST_BUFFER_OFFSET_END (buf) = 700;

  ret = gst_audio_buffer_clip (buf, &s, 100, 2);
  fail_unless (ret != NULL);

  fail_unless_equals_int64 (GST_BUFFER_TIMESTAMP (ret), 4 * GST_SECOND);
  fail_unless_equals_int64 (GST_BUFFER_DURATION (ret), 3 * GST_SECOND);
  fail_unless_equals_int64 (GST_BUFFER_OFFSET (ret), 400);
  fail_unless_equals_int64 (GST_BUFFER_OFFSET_END (ret), 700);

  meta = gst_buffer_get_audio_meta (ret);
  fail_unless (meta);
  fail_unless_equals_int (meta->info.channels, 2);
  fail_unless_equals_int (meta->samples, 300);

  gst_audio_buffer_map (&abuf, &info, ret, GST_MAP_READ);
  fail_unless_equals_int (abuf.n_planes, 2);
  fail_unless_equals_int (GST_AUDIO_BUFFER_PLANE_SIZE (&abuf), 300);
  fail_unless_equals_pointer (abuf.planes[0], data + 200);
  fail_unless_equals_pointer (abuf.planes[1], data + 700);
  gst_audio_buffer_unmap (&abuf);

  gst_buffer_unref (ret);
}

GST_END_TEST;

GST_START_TEST (test_buffer_clip_time_stop)
{
  GstSegment s;
  GstBuffer *buf;
  GstBuffer *ret;
  GstMapInfo map;
  guint8 *data;

  /* Clip only stop */
  buf = make_buffer (&data);
  setup_segment (&s, GST_FORMAT_TIME, 2 * GST_SECOND, 10 * GST_SECOND,
      2 * GST_SECOND);

  GST_BUFFER_TIMESTAMP (buf) = 2 * GST_SECOND;
  GST_BUFFER_DURATION (buf) = 10 * GST_SECOND;
  GST_BUFFER_OFFSET (buf) = 200;
  GST_BUFFER_OFFSET_END (buf) = 1200;

  ret = gst_audio_buffer_clip (buf, &s, 100, 1);
  fail_unless (ret != NULL);

  fail_unless_equals_int64 (GST_BUFFER_TIMESTAMP (ret), 2 * GST_SECOND);
  fail_unless_equals_int64 (GST_BUFFER_DURATION (ret), 8 * GST_SECOND);
  fail_unless_equals_int64 (GST_BUFFER_OFFSET (ret), 200);
  fail_unless_equals_int64 (GST_BUFFER_OFFSET_END (ret), 1000);
  gst_buffer_map (ret, &map, GST_MAP_READ);
  fail_unless (map.data == data);
  fail_unless (map.size == 800);
  gst_buffer_unmap (ret, &map);

  gst_buffer_unref (ret);
}

GST_END_TEST;

GST_START_TEST (test_buffer_clip_time_stop_planar)
{
  GstSegment s;
  GstBuffer *buf;
  GstBuffer *ret;
  GstAudioInfo info;
  GstAudioMeta *meta;
  GstAudioBuffer abuf;
  guint8 *data;

  /* Clip only stop */
  buf = make_buffer (&data);

  gst_audio_info_init (&info);
  gst_audio_info_set_format (&info, GST_AUDIO_FORMAT_S8, 44100, 2, NULL);
  info.layout = GST_AUDIO_LAYOUT_NON_INTERLEAVED;
  gst_buffer_add_audio_meta (buf, &info, 500, NULL);

  setup_segment (&s, GST_FORMAT_TIME, 2 * GST_SECOND, 6 * GST_SECOND,
      2 * GST_SECOND);

  GST_BUFFER_TIMESTAMP (buf) = 2 * GST_SECOND;
  GST_BUFFER_DURATION (buf) = 5 * GST_SECOND;
  GST_BUFFER_OFFSET (buf) = 200;
  GST_BUFFER_OFFSET_END (buf) = 700;

  ret = gst_audio_buffer_clip (buf, &s, 100, 2);
  fail_unless (ret != NULL);

  fail_unless_equals_int64 (GST_BUFFER_TIMESTAMP (ret), 2 * GST_SECOND);
  fail_unless_equals_int64 (GST_BUFFER_DURATION (ret), 4 * GST_SECOND);
  fail_unless_equals_int64 (GST_BUFFER_OFFSET (ret), 200);
  fail_unless_equals_int64 (GST_BUFFER_OFFSET_END (ret), 600);

  meta = gst_buffer_get_audio_meta (ret);
  fail_unless (meta);
  fail_unless_equals_int (meta->info.channels, 2);
  fail_unless_equals_int (meta->samples, 400);

  gst_audio_buffer_map (&abuf, &info, ret, GST_MAP_READ);
  fail_unless_equals_int (abuf.n_planes, 2);
  fail_unless_equals_int (GST_AUDIO_BUFFER_PLANE_SIZE (&abuf), 400);
  fail_unless_equals_pointer (abuf.planes[0], data);
  fail_unless_equals_pointer (abuf.planes[1], data + 500);
  gst_audio_buffer_unmap (&abuf);

  gst_buffer_unref (ret);
}

GST_END_TEST;

GST_START_TEST (test_buffer_clip_time_outside)
{
  GstSegment s;
  GstBuffer *buf;
  GstBuffer *ret;

  /* Buffer outside segment */
  buf = make_buffer (NULL);
  setup_segment (&s, GST_FORMAT_TIME, 12 * GST_SECOND, 20 * GST_SECOND,
      12 * GST_SECOND);

  GST_BUFFER_TIMESTAMP (buf) = 2 * GST_SECOND;
  GST_BUFFER_DURATION (buf) = 10 * GST_SECOND;
  GST_BUFFER_OFFSET (buf) = 200;
  GST_BUFFER_OFFSET_END (buf) = 1200;

  ret = gst_audio_buffer_clip (buf, &s, 100, 1);
  fail_unless (ret == NULL);
}

GST_END_TEST;

GST_START_TEST (test_buffer_clip_time_start_and_stop_no_meta)
{
  GstSegment s;
  GstBuffer *buf;
  GstBuffer *ret;
  GstMapInfo map;
  guint8 *data;

  /* Clip start and end but don't touch duration, offset and offset_end */
  buf = make_buffer (&data);
  setup_segment (&s, GST_FORMAT_TIME, 4 * GST_SECOND, 8 * GST_SECOND,
      4 * GST_SECOND);

  GST_BUFFER_TIMESTAMP (buf) = 2 * GST_SECOND;
  GST_BUFFER_DURATION (buf) = GST_CLOCK_TIME_NONE;
  GST_BUFFER_OFFSET (buf) = GST_BUFFER_OFFSET_NONE;
  GST_BUFFER_OFFSET_END (buf) = GST_BUFFER_OFFSET_NONE;

  ret = gst_audio_buffer_clip (buf, &s, 100, 1);
  fail_unless (ret != NULL);

  fail_unless_equals_int64 (GST_BUFFER_TIMESTAMP (ret), 4 * GST_SECOND);
  fail_unless_equals_int64 (GST_BUFFER_DURATION (ret), GST_CLOCK_TIME_NONE);
  fail_unless_equals_int64 (GST_BUFFER_OFFSET (ret), GST_BUFFER_OFFSET_NONE);
  fail_unless_equals_int64 (GST_BUFFER_OFFSET_END (ret),
      GST_BUFFER_OFFSET_NONE);
  gst_buffer_map (ret, &map, GST_MAP_READ);
  fail_unless (map.data == data + 200);
  fail_unless (map.size == 400);
  gst_buffer_unmap (ret, &map);

  gst_buffer_unref (ret);
}

GST_END_TEST;

GST_START_TEST (test_buffer_clip_time_no_timestamp)
{
  GstSegment s;
  GstBuffer *buf;
  GstBuffer *ret;

  /* If the buffer has no timestamp it should assert()
   * FIXME: check if return value is the same as the input buffer.
   *        probably can't be done because the assert() does a SIGABRT.
   */
  buf = make_buffer (NULL);
  setup_segment (&s, GST_FORMAT_TIME, 0 * GST_SECOND, 10 * GST_SECOND,
      0 * GST_SECOND);

  GST_BUFFER_TIMESTAMP (buf) = GST_CLOCK_TIME_NONE;
  GST_BUFFER_DURATION (buf) = GST_CLOCK_TIME_NONE;
  GST_BUFFER_OFFSET (buf) = GST_BUFFER_OFFSET_NONE;
  GST_BUFFER_OFFSET_END (buf) = GST_BUFFER_OFFSET_NONE;

  ret = gst_audio_buffer_clip (buf, &s, 100, 1);
  fail_unless (ret == buf);

  gst_buffer_unref (buf);
}

GST_END_TEST;

GST_START_TEST (test_buffer_clip_time_handles_rounding)
{
  GstSegment s;
  GstBuffer *buf;
  GstBuffer *ret;
  GstMapInfo map;
  guint8 *data;
  GstClockTime time_per_sample = GST_SECOND / 100;

  /* Clip only stop */
  buf = make_buffer (&data);
  setup_segment (&s, GST_FORMAT_TIME, 2 * GST_SECOND, 10 * GST_SECOND,
      2 * GST_SECOND);

  GST_BUFFER_TIMESTAMP (buf) = 2 * GST_SECOND;
  /* the max duration that still converts back to the same size of samples */
  GST_BUFFER_DURATION (buf) = 10 * GST_SECOND + (time_per_sample - 1);
  GST_BUFFER_OFFSET (buf) = 200;
  GST_BUFFER_OFFSET_END (buf) = 1200;

  ret = gst_audio_buffer_clip (buf, &s, 100, 1);
  fail_unless (ret != NULL);

  fail_unless_equals_int64 (GST_BUFFER_TIMESTAMP (ret), 2 * GST_SECOND);
  fail_unless_equals_int64 (GST_BUFFER_DURATION (ret), 8 * GST_SECOND);
  fail_unless_equals_int64 (GST_BUFFER_OFFSET (ret), 200);
  fail_unless_equals_int64 (GST_BUFFER_OFFSET_END (ret), 1000);
  gst_buffer_map (ret, &map, GST_MAP_READ);
  fail_unless (map.data == data);
  fail_unless (map.size == 800);
  gst_buffer_unmap (ret, &map);

  gst_buffer_unref (ret);
}

GST_END_TEST;

GST_START_TEST (test_buffer_clip_samples_start_and_stop)
{
  GstSegment s;
  GstBuffer *buf;
  GstBuffer *ret;
  GstMapInfo map;
  guint8 *data;

  /* Clip start and end */
  buf = make_buffer (&data);
  setup_segment (&s, GST_FORMAT_DEFAULT, 400, 800, 400);

  GST_BUFFER_TIMESTAMP (buf) = 2 * GST_SECOND;
  GST_BUFFER_DURATION (buf) = 10 * GST_SECOND;
  GST_BUFFER_OFFSET (buf) = 200;
  GST_BUFFER_OFFSET_END (buf) = 1200;

  ret = gst_audio_buffer_clip (buf, &s, 100, 1);
  fail_unless (ret != NULL);

  fail_unless_equals_int64 (GST_BUFFER_TIMESTAMP (ret), 4 * GST_SECOND);
  fail_unless_equals_int64 (GST_BUFFER_DURATION (ret), 4 * GST_SECOND);
  fail_unless_equals_int64 (GST_BUFFER_OFFSET (ret), 400);
  fail_unless_equals_int64 (GST_BUFFER_OFFSET_END (ret), 800);
  gst_buffer_map (ret, &map, GST_MAP_READ);
  fail_unless (map.data == data + 200);
  fail_unless (map.size == 400);
  gst_buffer_unmap (ret, &map);

  gst_buffer_unref (ret);
}

GST_END_TEST;

GST_START_TEST (test_buffer_clip_samples_start_and_stop_planar)
{
  GstSegment s;
  GstBuffer *buf;
  GstBuffer *ret;
  GstAudioInfo info;
  GstAudioMeta *meta;
  GstAudioBuffer abuf;
  guint8 *data;

  /* Clip start and end */
  buf = make_buffer (&data);

  gst_audio_info_init (&info);
  gst_audio_info_set_format (&info, GST_AUDIO_FORMAT_S8, 44100, 2, NULL);
  info.layout = GST_AUDIO_LAYOUT_NON_INTERLEAVED;
  gst_buffer_add_audio_meta (buf, &info, 500, NULL);


  setup_segment (&s, GST_FORMAT_DEFAULT, 400, 600, 400);

  GST_BUFFER_TIMESTAMP (buf) = 2 * GST_SECOND;
  GST_BUFFER_DURATION (buf) = 5 * GST_SECOND;
  GST_BUFFER_OFFSET (buf) = 200;
  GST_BUFFER_OFFSET_END (buf) = 700;

  ret = gst_audio_buffer_clip (buf, &s, 100, 2);
  fail_unless (ret != NULL);

  fail_unless_equals_int64 (GST_BUFFER_TIMESTAMP (ret), 4 * GST_SECOND);
  fail_unless_equals_int64 (GST_BUFFER_DURATION (ret), 2 * GST_SECOND);
  fail_unless_equals_int64 (GST_BUFFER_OFFSET (ret), 400);
  fail_unless_equals_int64 (GST_BUFFER_OFFSET_END (ret), 600);

  meta = gst_buffer_get_audio_meta (ret);
  fail_unless (meta);
  fail_unless_equals_int (meta->info.channels, 2);
  fail_unless_equals_int (meta->samples, 200);

  gst_audio_buffer_map (&abuf, &info, ret, GST_MAP_READ);
  fail_unless_equals_int (abuf.n_planes, 2);
  fail_unless_equals_int (GST_AUDIO_BUFFER_PLANE_SIZE (&abuf), 200);
  fail_unless_equals_pointer (abuf.planes[0], data + 200);
  fail_unless_equals_pointer (abuf.planes[1], data + 700);
  gst_audio_buffer_unmap (&abuf);

  gst_buffer_unref (ret);
}

GST_END_TEST;

GST_START_TEST (test_buffer_clip_samples_start)
{
  GstSegment s;
  GstBuffer *buf;
  GstBuffer *ret;
  GstMapInfo map;
  guint8 *data;

  /* Clip only start */
  buf = make_buffer (&data);
  setup_segment (&s, GST_FORMAT_DEFAULT, 400, 1200, 400);

  GST_BUFFER_TIMESTAMP (buf) = 2 * GST_SECOND;
  GST_BUFFER_DURATION (buf) = 10 * GST_SECOND;
  GST_BUFFER_OFFSET (buf) = 200;
  GST_BUFFER_OFFSET_END (buf) = 1200;

  ret = gst_audio_buffer_clip (buf, &s, 100, 1);
  fail_unless (ret != NULL);

  fail_unless_equals_int64 (GST_BUFFER_TIMESTAMP (ret), 4 * GST_SECOND);
  fail_unless_equals_int64 (GST_BUFFER_DURATION (ret), 8 * GST_SECOND);
  fail_unless_equals_int64 (GST_BUFFER_OFFSET (ret), 400);
  fail_unless_equals_int64 (GST_BUFFER_OFFSET_END (ret), 1200);
  gst_buffer_map (ret, &map, GST_MAP_READ);
  fail_unless (map.data == data + 200);
  fail_unless (map.size == 800);
  gst_buffer_unmap (ret, &map);

  gst_buffer_unref (ret);
}

GST_END_TEST;

GST_START_TEST (test_buffer_clip_samples_stop)
{
  GstSegment s;
  GstBuffer *buf;
  GstBuffer *ret;
  GstMapInfo map;
  guint8 *data;

  /* Clip only stop */
  buf = make_buffer (&data);
  setup_segment (&s, GST_FORMAT_DEFAULT, 200, 1000, 200);

  GST_BUFFER_TIMESTAMP (buf) = 2 * GST_SECOND;
  GST_BUFFER_DURATION (buf) = 10 * GST_SECOND;
  GST_BUFFER_OFFSET (buf) = 200;
  GST_BUFFER_OFFSET_END (buf) = 1200;

  ret = gst_audio_buffer_clip (buf, &s, 100, 1);
  fail_unless (ret != NULL);

  fail_unless_equals_int64 (GST_BUFFER_TIMESTAMP (ret), 2 * GST_SECOND);
  fail_unless_equals_int64 (GST_BUFFER_DURATION (ret), 8 * GST_SECOND);
  fail_unless_equals_int64 (GST_BUFFER_OFFSET (ret), 200);
  fail_unless_equals_int64 (GST_BUFFER_OFFSET_END (ret), 1000);
  gst_buffer_map (ret, &map, GST_MAP_READ);
  fail_unless (map.data == data);
  fail_unless (map.size == 800);
  gst_buffer_unmap (ret, &map);

  gst_buffer_unref (ret);
}

GST_END_TEST;

GST_START_TEST (test_buffer_clip_samples_outside)
{
  GstSegment s;
  GstBuffer *buf;
  GstBuffer *ret;

  /* Buffer outside segment */
  buf = make_buffer (NULL);
  setup_segment (&s, GST_FORMAT_DEFAULT, 1200, 2000, 1200);

  GST_BUFFER_TIMESTAMP (buf) = 2 * GST_SECOND;
  GST_BUFFER_DURATION (buf) = 10 * GST_SECOND;
  GST_BUFFER_OFFSET (buf) = 200;
  GST_BUFFER_OFFSET_END (buf) = 1200;

  ret = gst_audio_buffer_clip (buf, &s, 100, 1);
  fail_unless (ret == NULL);
}

GST_END_TEST;

GST_START_TEST (test_buffer_clip_samples_start_and_stop_no_meta)
{
  GstSegment s;
  GstBuffer *buf;
  GstBuffer *ret;
  GstMapInfo map;
  guint8 *data;

  /* Clip start and end but don't touch duration and offset_end */
  buf = make_buffer (&data);
  setup_segment (&s, GST_FORMAT_DEFAULT, 400, 800, 400);

  GST_BUFFER_TIMESTAMP (buf) = 2 * GST_SECOND;
  GST_BUFFER_DURATION (buf) = GST_CLOCK_TIME_NONE;
  GST_BUFFER_OFFSET (buf) = 200;
  GST_BUFFER_OFFSET_END (buf) = GST_BUFFER_OFFSET_NONE;

  ret = gst_audio_buffer_clip (buf, &s, 100, 1);
  fail_unless (ret != NULL);

  fail_unless_equals_int64 (GST_BUFFER_TIMESTAMP (ret), 4 * GST_SECOND);
  fail_unless_equals_int64 (GST_BUFFER_DURATION (ret), GST_CLOCK_TIME_NONE);
  fail_unless_equals_int64 (GST_BUFFER_OFFSET (ret), 400);
  fail_unless_equals_int64 (GST_BUFFER_OFFSET_END (ret), 800);
  gst_buffer_map (ret, &map, GST_MAP_READ);
  fail_unless (map.data == data + 200);
  fail_unless (map.size == 400);
  gst_buffer_unmap (ret, &map);

  gst_buffer_unref (ret);
}

GST_END_TEST;

GST_START_TEST (test_buffer_clip_samples_no_timestamp)
{
  GstSegment s;
  GstBuffer *buf;

  /* If the buffer has no offset it should assert() in DEFAULT format
   * FIXME: check if return value is the same as the input buffer.
   *        probably can't be done because the assert() does a SIGABRT.
   */
  buf = make_buffer (NULL);
  setup_segment (&s, GST_FORMAT_DEFAULT, 0, 10, 0);

  GST_BUFFER_TIMESTAMP (buf) = 0 * GST_SECOND;
  GST_BUFFER_DURATION (buf) = GST_CLOCK_TIME_NONE;
  GST_BUFFER_OFFSET (buf) = GST_BUFFER_OFFSET_NONE;
  GST_BUFFER_OFFSET_END (buf) = GST_BUFFER_OFFSET_NONE;

  ASSERT_CRITICAL (gst_audio_buffer_clip (buf, &s, 100, 1));

  gst_buffer_unref (buf);
}

GST_END_TEST;

GST_START_TEST (test_buffer_truncate_samples_offset_end)
{
  GstBuffer *buf;
  GstBuffer *ret;
  guint8 *data;

  buf = make_buffer (&data);
  GST_BUFFER_TIMESTAMP (buf) = 2 * GST_SECOND;
  GST_BUFFER_DURATION (buf) = 10 * GST_SECOND;
  GST_BUFFER_OFFSET (buf) = 200;
  GST_BUFFER_OFFSET_END (buf) = 1200;

  ret = gst_audio_buffer_truncate (buf, 4, 100, 100);
  fail_unless (ret != NULL);

  fail_unless_equals_int64 (GST_BUFFER_TIMESTAMP (ret), GST_CLOCK_TIME_NONE);
  fail_unless_equals_int64 (GST_BUFFER_DURATION (ret), GST_CLOCK_TIME_NONE);
  fail_unless_equals_int64 (GST_BUFFER_OFFSET (ret), 300);
  fail_unless_equals_int64 (GST_BUFFER_OFFSET_END (ret), 400);

  gst_buffer_unref (ret);
}

GST_END_TEST;

GST_START_TEST (test_buffer_truncate_samples_no_timestamp)
{
  GstBuffer *buf;
  GstBuffer *ret;
  guint8 *data;

  buf = make_buffer (&data);
  GST_BUFFER_TIMESTAMP (buf) = GST_CLOCK_TIME_NONE;
  GST_BUFFER_DURATION (buf) = GST_CLOCK_TIME_NONE;
  GST_BUFFER_OFFSET (buf) = GST_BUFFER_OFFSET_NONE;
  GST_BUFFER_OFFSET_END (buf) = GST_BUFFER_OFFSET_NONE;

  ret = gst_audio_buffer_truncate (buf, 4, 100, 1);
  fail_unless (ret != NULL);

  fail_unless_equals_int64 (GST_BUFFER_TIMESTAMP (ret), GST_CLOCK_TIME_NONE);
  fail_unless_equals_int64 (GST_BUFFER_DURATION (ret), GST_CLOCK_TIME_NONE);
  fail_unless_equals_int64 (GST_BUFFER_OFFSET (ret), GST_BUFFER_OFFSET_NONE);
  fail_unless_equals_int64 (GST_BUFFER_OFFSET_END (ret),
      GST_BUFFER_OFFSET_NONE);

  gst_buffer_unref (ret);
}

GST_END_TEST;

GST_START_TEST (test_multichannel_checks)
{
  GstAudioChannelPosition pos_2_mixed[2] = {
    GST_AUDIO_CHANNEL_POSITION_MONO,
    GST_AUDIO_CHANNEL_POSITION_NONE
  };
  GstAudioChannelPosition pos_2_none[2] = {
    GST_AUDIO_CHANNEL_POSITION_NONE,
    GST_AUDIO_CHANNEL_POSITION_NONE
  };
  GstAudioChannelPosition pos_2_flr[2] = {
    GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT,
    GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT
  };
  GstAudioChannelPosition pos_2_frl[2] = {
    GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT,
    GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT
  };
  GstAudioChannelPosition pos_2_frr[2] = {
    GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT,
    GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT
  };
  GstAudioChannelPosition pos_3_flrc[3] = {
    GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT,
    GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT,
    GST_AUDIO_CHANNEL_POSITION_FRONT_CENTER
  };
  GstAudioChannelPosition pos_3_frcl[3] = {
    GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT,
    GST_AUDIO_CHANNEL_POSITION_FRONT_CENTER,
    GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT
  };
  GstAudioInfo info, info2;
  GstCaps *caps;

  gst_audio_info_init (&info);
  gst_audio_info_init (&info2);

  gst_audio_info_set_format (&info, GST_AUDIO_FORMAT_F32, 48000, 2, NULL);
  fail_unless (memcmp (&info.position, pos_2_flr, sizeof (pos_2_flr)) == 0);

  gst_audio_info_set_format (&info, GST_AUDIO_FORMAT_F32, 48000, 2, pos_2_flr);
  fail_unless (memcmp (&info.position, pos_2_flr, sizeof (pos_2_flr)) == 0);
  caps = gst_audio_info_to_caps (&info);
  fail_unless (gst_audio_info_from_caps (&info2, caps));
  fail_unless (memcmp (&info, &info2, sizeof (info)) == 0);
  gst_caps_unref (caps);

  gst_audio_info_set_format (&info, GST_AUDIO_FORMAT_F32, 48000, 2, pos_2_none);
  fail_unless (memcmp (&info.position, pos_2_none, sizeof (pos_2_none)) == 0);
  caps = gst_audio_info_to_caps (&info);
  fail_unless (gst_audio_info_from_caps (&info2, caps));
  fail_unless (memcmp (&info, &info2, sizeof (info)) == 0);
  gst_caps_unref (caps);

  gst_audio_info_set_format (&info, GST_AUDIO_FORMAT_F32, 48000, 3, pos_3_flrc);
  fail_unless (memcmp (&info.position, pos_3_flrc, sizeof (pos_3_flrc)) == 0);
  caps = gst_audio_info_to_caps (&info);
  fail_unless (gst_audio_info_from_caps (&info2, caps));
  fail_unless (memcmp (&info, &info2, sizeof (info)) == 0);
  gst_caps_unref (caps);

  ASSERT_WARNING (gst_audio_info_set_format (&info, GST_AUDIO_FORMAT_F32, 48000,
          2, pos_2_frl));
  ASSERT_WARNING (gst_audio_info_set_format (&info, GST_AUDIO_FORMAT_F32, 48000,
          2, pos_2_mixed));
  ASSERT_WARNING (gst_audio_info_set_format (&info, GST_AUDIO_FORMAT_F32, 48000,
          2, pos_2_frr));
  ASSERT_WARNING (gst_audio_info_set_format (&info, GST_AUDIO_FORMAT_F32, 48000,
          3, pos_3_frcl));
}

GST_END_TEST;

typedef struct
{
  gint channels;
  GstAudioChannelPosition from[32], to[32];
  gint32 in[32], out[32];
  gint32 plane_offsets[32];
  gboolean fail;
} MultichannelReorderData;

GST_START_TEST (test_multichannel_reorder)
{
  MultichannelReorderData tests[] = {
    {1,
          {GST_AUDIO_CHANNEL_POSITION_MONO},
          {GST_AUDIO_CHANNEL_POSITION_MONO},
          {0, 1, 2, 3},
          {0, 1, 2, 3},
          {0},
        FALSE},
    {1,
          {GST_AUDIO_CHANNEL_POSITION_MONO},
          {GST_AUDIO_CHANNEL_POSITION_FRONT_CENTER},
          {0, 1, 2, 3},
          {0, 1, 2, 3},
          {0},
        TRUE},
    {2,
          {GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT,
              GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT},
          {GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT,
              GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT},
          {0, 1, 2, 3},
          {0, 1, 2, 3},
          {0, 1},
        FALSE},
    {2,
          {GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT,
              GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT},
          {GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT,
              GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT},
          {0, 1, 2, 3},
          {1, 0, 3, 2},
          {1, 0},
        FALSE},
    {4,
          {GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT,
                GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT,
                GST_AUDIO_CHANNEL_POSITION_FRONT_CENTER,
              GST_AUDIO_CHANNEL_POSITION_REAR_CENTER},
          {GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT,
                GST_AUDIO_CHANNEL_POSITION_FRONT_CENTER,
                GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT,
              GST_AUDIO_CHANNEL_POSITION_REAR_CENTER},
          {0, 1, 2, 3},
          {1, 2, 0, 3},
          {1, 2, 0, 3},
        FALSE},
    {4,
          {GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT,
                GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT,
                GST_AUDIO_CHANNEL_POSITION_FRONT_CENTER,
              GST_AUDIO_CHANNEL_POSITION_REAR_CENTER},
          {GST_AUDIO_CHANNEL_POSITION_REAR_CENTER,
                GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT,
                GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT,
              GST_AUDIO_CHANNEL_POSITION_FRONT_CENTER},
          {0, 1, 2, 3},
          {3, 0, 1, 2},
          {3, 0, 1, 2},
        FALSE},
    {4,
          {GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT,
                GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT,
                GST_AUDIO_CHANNEL_POSITION_FRONT_CENTER,
              GST_AUDIO_CHANNEL_POSITION_REAR_CENTER},
          {GST_AUDIO_CHANNEL_POSITION_REAR_CENTER,
                GST_AUDIO_CHANNEL_POSITION_FRONT_CENTER,
                GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT,
              GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT},
          {0, 1, 2, 3},
          {3, 2, 1, 0},
          {3, 2, 1, 0},
        FALSE},
  };
  gint i, j;
  GstBuffer *buf;
  GstMapInfo map;
  GstAudioInfo info;
  GstAudioBuffer abuf;

  for (i = 0; i < G_N_ELEMENTS (tests); i++) {
    buf =
        gst_buffer_new_wrapped_full (GST_MEMORY_FLAG_READONLY, tests[i].in,
        sizeof (tests[i].in), 0, sizeof (tests[i].in), NULL, NULL);

    if (tests[i].fail) {
      fail_if (gst_audio_buffer_reorder_channels (buf, GST_AUDIO_FORMAT_S32,
              tests[i].channels, tests[i].from, tests[i].to));
    } else {
      /* first interpret as interleaved */

      fail_unless (gst_audio_buffer_reorder_channels (buf, GST_AUDIO_FORMAT_S32,
              tests[i].channels, tests[i].from, tests[i].to));

      gst_buffer_map (buf, &map, GST_MAP_READ);
      fail_unless_equals_int (map.size, sizeof (tests[i].in));
      fail_unless (memcmp (tests[i].out, map.data, map.size) == 0);
      gst_buffer_unmap (buf, &map);

      /* now interpret as planar */

      gst_audio_info_init (&info);
      gst_audio_info_set_format (&info, GST_AUDIO_FORMAT_S32, 44100,
          tests[i].channels, NULL);
      info.layout = GST_AUDIO_LAYOUT_NON_INTERLEAVED;

      gst_buffer_add_audio_meta (buf, &info,
          sizeof (tests[i].in) / (tests[i].channels * sizeof (gint32)), NULL);

      fail_unless (gst_audio_buffer_reorder_channels (buf, GST_AUDIO_FORMAT_S32,
              tests[i].channels, tests[i].from, tests[i].to));

      fail_unless (gst_audio_buffer_map (&abuf, &info, buf, GST_MAP_READ));
      fail_unless_equals_int (abuf.n_planes, tests[i].channels);
      fail_unless_equals_int (GST_AUDIO_BUFFER_PLANE_SIZE (&abuf),
          sizeof (tests[i].in) / tests[i].channels);
      for (j = 0; j < abuf.n_planes; j++) {
        fail_unless_equals_pointer (abuf.planes[j],
            abuf.map_infos[0].data +
            tests[i].plane_offsets[j] * GST_AUDIO_BUFFER_PLANE_SIZE (&abuf));
      }
      gst_audio_buffer_unmap (&abuf);
    }
    gst_buffer_unref (buf);
  }
}

GST_END_TEST;

GST_START_TEST (test_audio_format_s8)
{
  GstAudioFormat fmt;

  fmt = gst_audio_format_build_integer (TRUE, G_BYTE_ORDER, 8, 8);
  fail_unless (fmt == GST_AUDIO_FORMAT_S8);

}

GST_END_TEST;

GST_START_TEST (test_audio_format_u8)
{
  GstAudioFormat fmt;
  fmt = gst_audio_format_build_integer (FALSE, G_BYTE_ORDER, 8, 8);
  fail_unless (fmt == GST_AUDIO_FORMAT_U8);
}

GST_END_TEST;

GST_START_TEST (test_fill_silence)
{
  GstAudioInfo info;
  GstAudioFormat f;
  gint i;
  guint8 test_silence[32];

  for (f = GST_AUDIO_FORMAT_S8; f < GST_AUDIO_FORMAT_F64; f++) {
    gst_audio_info_set_format (&info, f, 48000, 1, NULL);

    gst_audio_format_info_fill_silence (info.finfo, test_silence,
        GST_AUDIO_INFO_BPF (&info) * 4);

    for (i = 0; i < 4; i++)
      fail_unless (memcmp (test_silence + i * GST_AUDIO_INFO_BPF (&info),
              info.finfo->silence, GST_AUDIO_INFO_BPF (&info)) == 0);
  }
}

GST_END_TEST;

GST_START_TEST (test_stream_align)
{
  GstAudioStreamAlign *align;
  guint i;
  GstClockTime timestamp;
  GstClockTime out_timestamp, out_duration;
  gboolean discont;

  align = gst_audio_stream_align_new (1000, 40 * GST_MSECOND, 1 * GST_SECOND);

  for (i = 0; i < 500; i++) {
    timestamp = 10 * GST_MSECOND * i;
    discont = i == 0;

    discont =
        gst_audio_stream_align_process (align, discont, timestamp, 10,
        &out_timestamp, &out_duration, NULL);

    if (i == 0)
      fail_unless (discont);
    else
      fail_unless (!discont);
    fail_unless_equals_uint64 (out_timestamp, 10 * GST_MSECOND * i);
    fail_unless_equals_uint64 (out_duration, 10 * GST_MSECOND);
  }

  /* Drift forwards by 1ms per 10ms buffer for the first 40 buffers.
   * - after 40 buffers we're above alignment threshold
   * - after 40 + 96 buffers we're at discont wait
   */
  for (i = 0; i < 500; i++) {
    timestamp = 10 * GST_MSECOND * i;
    discont = i == 0;
    if (i > 0)
      timestamp += 1 * GST_MSECOND * MIN (i, 40);

    discont =
        gst_audio_stream_align_process (align, discont, timestamp, 10,
        &out_timestamp, &out_duration, NULL);

    if (i < 136) {
      if (i == 0)
        fail_unless (discont);
      else
        fail_unless (!discont);
      fail_unless_equals_uint64 (out_timestamp, 10 * GST_MSECOND * i);
      fail_unless_equals_uint64 (out_duration, 10 * GST_MSECOND);
    } else {
      if (i == 136)
        fail_unless (discont);
      else
        fail_unless (!discont);
      fail_unless_equals_uint64 (out_timestamp,
          10 * GST_MSECOND * i + 40 * GST_MSECOND);
      fail_unless_equals_uint64 (out_duration, 10 * GST_MSECOND);
    }
  }

  /* Drift backwards by 1ms per 10ms buffer for the first 40 buffers.
   * - after 40 buffers we're above alignment threshold
   * - after 40 + 104 buffers we're at discont wait
   */
  for (i = 0; i < 500; i++) {
    timestamp = 10 * GST_MSECOND * i;
    discont = i == 0;
    if (i > 0)
      timestamp -= 1 * GST_MSECOND * MIN (i, 40);

    discont =
        gst_audio_stream_align_process (align, discont, timestamp, 10,
        &out_timestamp, &out_duration, NULL);

    if (i < 144) {
      if (i == 0)
        fail_unless (discont);
      else
        fail_unless (!discont);
      fail_unless_equals_uint64 (out_timestamp, 10 * GST_MSECOND * i);
      fail_unless_equals_uint64 (out_duration, 10 * GST_MSECOND);
    } else {
      if (i == 144)
        fail_unless (discont);
      else
        fail_unless (!discont);

      fail_unless_equals_uint64 (out_timestamp,
          10 * GST_MSECOND * i - 40 * GST_MSECOND);
      fail_unless_equals_uint64 (out_duration, 10 * GST_MSECOND);
    }
  }

  /* Shift all buffers but the first by 40ms
   * - after 1 buffers we're above alignment threshold
   * - after 97 buffers we're at discont wait
   */
  for (i = 0; i < 500; i++) {
    timestamp = 10 * GST_MSECOND * i;
    discont = i == 0;
    if (i > 0)
      timestamp += 40 * GST_MSECOND;

    discont =
        gst_audio_stream_align_process (align, discont, timestamp, 10,
        &out_timestamp, &out_duration, NULL);

    if (i < 97) {
      if (i == 0)
        fail_unless (discont);
      else
        fail_unless (!discont);
      fail_unless_equals_uint64 (out_timestamp, 10 * GST_MSECOND * i);
      fail_unless_equals_uint64 (out_duration, 10 * GST_MSECOND);
    } else {
      if (i == 97)
        fail_unless (discont);
      else
        fail_unless (!discont);
      fail_unless_equals_uint64 (out_timestamp,
          10 * GST_MSECOND * i + 40 * GST_MSECOND);
      fail_unless_equals_uint64 (out_duration, 10 * GST_MSECOND);
    }
  }

  /* Shift every second buffer by 40ms:
   * - never discont!
   */
  for (i = 0; i < 500; i++) {
    timestamp = 10 * GST_MSECOND * i;
    discont = i == 0;

    if (i % 2 == 0 && i > 0)
      timestamp += 40 * GST_MSECOND;

    discont =
        gst_audio_stream_align_process (align, discont, timestamp, 10,
        &out_timestamp, &out_duration, NULL);

    if (i == 0)
      fail_unless (discont);
    else
      fail_unless (!discont);
    fail_unless_equals_uint64 (out_timestamp, 10 * GST_MSECOND * i);
    fail_unless_equals_uint64 (out_duration, 10 * GST_MSECOND);
  }

  /* Shift every buffer after 100 by 2s: discont at buffer 100
   */
  for (i = 0; i < 500; i++) {
    timestamp = 10 * GST_MSECOND * i;
    discont = i == 0;
    if (i >= 100)
      timestamp += 2 * GST_SECOND;

    discont =
        gst_audio_stream_align_process (align, discont, timestamp, 10,
        &out_timestamp, &out_duration, NULL);

    if (i < 100) {
      if (i == 0)
        fail_unless (discont);
      else
        fail_unless (!discont);
      fail_unless_equals_uint64 (out_timestamp, 10 * GST_MSECOND * i);
      fail_unless_equals_uint64 (out_duration, 10 * GST_MSECOND);
    } else {
      if (i == 100)
        fail_unless (discont);
      else
        fail_unless (!discont);
      fail_unless_equals_uint64 (out_timestamp,
          10 * GST_MSECOND * i + 2 * GST_SECOND);
      fail_unless_equals_uint64 (out_duration, 10 * GST_MSECOND);
    }
  }

  gst_audio_stream_align_free (align);
}

GST_END_TEST;

GST_START_TEST (test_stream_align_reverse)
{
  GstAudioStreamAlign *align;
  gint i;
  GstClockTime timestamp;
  GstClockTime out_timestamp, out_duration;
  gboolean discont;

  align = gst_audio_stream_align_new (-1000, 40 * GST_MSECOND, 1 * GST_SECOND);

  for (i = 499; i >= 0; i--) {
    timestamp = 10 * GST_MSECOND * i;
    discont = i == 499;

    discont =
        gst_audio_stream_align_process (align, discont, timestamp, 10,
        &out_timestamp, &out_duration, NULL);

    if (i == 499)
      fail_unless (discont);
    else
      fail_unless (!discont);
    fail_unless_equals_uint64 (out_timestamp, 10 * GST_MSECOND * i);
    fail_unless_equals_uint64 (out_duration, 10 * GST_MSECOND);
  }

  /* Drift forwards by 1ms per 10ms buffer for the first 40 buffers.
   * - after 40 buffers we're above alignment threshold
   * - after 40 + 100 buffers we're at discont wait
   */
  for (i = 499; i >= 0; i--) {
    timestamp = 10 * GST_MSECOND * i;
    discont = i == 499;
    if (i < 499)
      timestamp += 1 * GST_MSECOND * MIN (499 - i, 40);

    discont =
        gst_audio_stream_align_process (align, discont, timestamp, 10,
        &out_timestamp, &out_duration, NULL);

    if (i >= 500 - 144) {
      if (i == 499)
        fail_unless (discont);
      else
        fail_unless (!discont);
      fail_unless_equals_uint64 (out_timestamp, 10 * GST_MSECOND * i);
      fail_unless_equals_uint64 (out_duration, 10 * GST_MSECOND);
    } else {
      if (i == 499 - 144)
        fail_unless (discont);
      else
        fail_unless (!discont);
      fail_unless_equals_uint64 (out_timestamp,
          10 * GST_MSECOND * i + 40 * GST_MSECOND);
      fail_unless_equals_uint64 (out_duration, 10 * GST_MSECOND);
    }
  }

  /* Drift backwards by 1ms per 10ms buffer for the first 40 buffers.
   * - after 40 buffers we're above alignment threshold
   * - after 40 + 100 buffers we're at discont wait
   */
  for (i = 499; i >= 4; i--) {
    timestamp = 10 * GST_MSECOND * i;
    discont = i == 499;
    if (i < 499)
      timestamp -= 1 * GST_MSECOND * MIN (499 - i, 40);

    discont =
        gst_audio_stream_align_process (align, discont, timestamp, 10,
        &out_timestamp, &out_duration, NULL);

    if (i >= 500 - 136) {
      if (i == 499)
        fail_unless (discont);
      else
        fail_unless (!discont);
      fail_unless_equals_uint64 (out_timestamp, 10 * GST_MSECOND * i);
      fail_unless_equals_uint64 (out_duration, 10 * GST_MSECOND);
    } else {
      if (i == 499 - 136)
        fail_unless (discont);
      else
        fail_unless (!discont);

      fail_unless_equals_uint64 (out_timestamp,
          10 * GST_MSECOND * i - 40 * GST_MSECOND);
      fail_unless_equals_uint64 (out_duration, 10 * GST_MSECOND);
    }
  }

  /* Shift all buffers but the first by 40ms
   * - after 1 buffers we're above alignment threshold
   * - after 106 buffers we're at discont wait
   */
  for (i = 499; i >= 0; i--) {
    timestamp = 10 * GST_MSECOND * i;
    discont = i == 499;
    if (i < 499)
      timestamp += 40 * GST_MSECOND;

    discont =
        gst_audio_stream_align_process (align, discont, timestamp, 10,
        &out_timestamp, &out_duration, NULL);

    if (i >= 500 - 105) {
      if (i == 499)
        fail_unless (discont);
      else
        fail_unless (!discont);
      fail_unless_equals_uint64 (out_timestamp, 10 * GST_MSECOND * i);
      fail_unless_equals_uint64 (out_duration, 10 * GST_MSECOND);
    } else {
      if (i == 499 - 105)
        fail_unless (discont);
      else
        fail_unless (!discont);
      fail_unless_equals_uint64 (out_timestamp,
          10 * GST_MSECOND * i + 40 * GST_MSECOND);
      fail_unless_equals_uint64 (out_duration, 10 * GST_MSECOND);
    }
  }

  /* Shift every second buffer by 40ms:
   * - never discont!
   */
  for (i = 499; i >= 0; i--) {
    timestamp = 10 * GST_MSECOND * i;
    discont = i == 499;

    if (i % 2 == 0 && i < 499)
      timestamp += 40 * GST_MSECOND;

    discont =
        gst_audio_stream_align_process (align, discont, timestamp, 10,
        &out_timestamp, &out_duration, NULL);

    if (i == 499)
      fail_unless (discont);
    else
      fail_unless (!discont);
    fail_unless_equals_uint64 (out_timestamp, 10 * GST_MSECOND * i);
    fail_unless_equals_uint64 (out_duration, 10 * GST_MSECOND);
  }

  /* Shift every buffer after 100 by 2s: discont at buffer 100
   */
  for (i = 499; i >= 0; i--) {
    timestamp = 10 * GST_MSECOND * i;
    discont = i == 499;
    if (i < 500 - 100)
      timestamp += 2 * GST_SECOND;

    discont =
        gst_audio_stream_align_process (align, discont, timestamp, 10,
        &out_timestamp, &out_duration, NULL);

    if (i >= 500 - 100) {
      if (i == 499)
        fail_unless (discont);
      else
        fail_unless (!discont);
      fail_unless_equals_uint64 (out_timestamp, 10 * GST_MSECOND * i);
      fail_unless_equals_uint64 (out_duration, 10 * GST_MSECOND);
    } else {
      if (i == 499 - 100)
        fail_unless (discont);
      else
        fail_unless (!discont);
      fail_unless_equals_uint64 (out_timestamp,
          10 * GST_MSECOND * i + 2 * GST_SECOND);
      fail_unless_equals_uint64 (out_duration, 10 * GST_MSECOND);
    }
  }

  gst_audio_stream_align_free (align);
}

GST_END_TEST;

typedef struct
{
  GstAudioFormat format;
  GstAudioLayout layout;
  gint channels;
  gsize samples;
  gsize plane_size;
  gsize offsets[10];
  gboolean add_meta;
  gboolean use_offsets;
} AudioBufferTestData;

GST_START_TEST (test_audio_buffer_and_audio_meta)
{
  AudioBufferTestData td[] = {
    {GST_AUDIO_FORMAT_S24_32, GST_AUDIO_LAYOUT_NON_INTERLEAVED,
        4, 60, 240, {10, 760, 510, 260}, TRUE, TRUE},
    {GST_AUDIO_FORMAT_F32, GST_AUDIO_LAYOUT_NON_INTERLEAVED,
        4, 60, 240, {0, 240, 480, 720}, TRUE, FALSE},
    {GST_AUDIO_FORMAT_S16, GST_AUDIO_LAYOUT_INTERLEAVED,
        4, 125, 1000, {0}, TRUE, FALSE},
    {GST_AUDIO_FORMAT_S16, GST_AUDIO_LAYOUT_INTERLEAVED,
        4, 125, 1000, {0}, FALSE, FALSE},
    {GST_AUDIO_FORMAT_S8, GST_AUDIO_LAYOUT_INTERLEAVED,
        8, 125, 1000, {0}, FALSE, FALSE},
    {GST_AUDIO_FORMAT_U8, GST_AUDIO_LAYOUT_NON_INTERLEAVED,
        8, 125, 125, {0, 125, 250, 375, 500, 625, 750, 875}, TRUE, FALSE},
    {GST_AUDIO_FORMAT_U32, GST_AUDIO_LAYOUT_NON_INTERLEAVED,
          10, 25, 100, {0, 100, 200, 300, 400, 500, 600, 700, 800, 900}, TRUE,
        FALSE},
    {GST_AUDIO_FORMAT_U32, GST_AUDIO_LAYOUT_INTERLEAVED,
        10, 25, 1000, {0}, FALSE, FALSE},
  };
  gint i;

  for (i = 0; i < G_N_ELEMENTS (td); i++) {
    GstBuffer *buf;
    guint8 *data;
    GstAudioInfo info;
    GstAudioMeta *meta;
    GstAudioBuffer buffer;
    gint j;

    gst_audio_info_init (&info);
    gst_audio_info_set_format (&info, td[i].format, 44100, td[i].channels,
        NULL);
    info.layout = td[i].layout;

    buf = make_buffer (&data);
    if (td[i].add_meta) {
      meta = gst_buffer_add_audio_meta (buf, &info, td[i].samples,
          td[i].use_offsets ? td[i].offsets : NULL);

      fail_unless (meta);
      fail_unless (GST_AUDIO_INFO_IS_VALID (&meta->info));
      fail_unless (gst_audio_info_is_equal (&meta->info, &info));
      fail_unless_equals_int (meta->info.finfo->format, td[i].format);
      fail_unless_equals_int (meta->info.layout, td[i].layout);
      fail_unless_equals_int (meta->info.channels, td[i].channels);
      fail_unless_equals_int (meta->samples, td[i].samples);

      if (td[i].layout == GST_AUDIO_LAYOUT_NON_INTERLEAVED) {
        fail_unless (meta->offsets);
        for (j = 0; j < td[i].channels; j++) {
          fail_unless_equals_int (meta->offsets[j], td[i].offsets[j]);
        }
      } else {
        fail_if (meta->offsets);
      }
    }

    fail_unless (gst_audio_buffer_map (&buffer, &info, buf, GST_MAP_READ));
    fail_unless_equals_pointer (buffer.buffer, buf);
    fail_unless (GST_AUDIO_INFO_IS_VALID (&buffer.info));
    fail_unless (gst_audio_info_is_equal (&buffer.info, &info));
    fail_unless_equals_int (buffer.n_samples, td[i].samples);

    if (td[i].layout == GST_AUDIO_LAYOUT_NON_INTERLEAVED) {
      fail_unless_equals_int (buffer.n_planes, td[i].channels);
      for (j = 0; j < td[i].channels; j++) {
        fail_unless_equals_pointer (buffer.planes[j], data + td[i].offsets[j]);
      }
    } else {
      fail_unless_equals_int (buffer.n_planes, 1);
      fail_unless_equals_pointer (buffer.planes[0], data);
    }

    fail_unless_equals_int (GST_AUDIO_BUFFER_PLANE_SIZE (&buffer),
        td[i].plane_size);

    if (buffer.n_planes <= 8) {
      fail_unless_equals_pointer (buffer.map_infos, buffer.priv_map_infos_arr);
      fail_unless_equals_pointer (buffer.planes, buffer.priv_planes_arr);
    } else {
      fail_if (buffer.map_infos == buffer.priv_map_infos_arr);
      fail_if (buffer.planes == buffer.priv_planes_arr);
    }

    gst_audio_buffer_unmap (&buffer);
    gst_buffer_unref (buf);
  }
}

GST_END_TEST;

typedef struct
{
  const gchar *str;
  GstAudioFormat format;
  GstAudioLayout layout;
  gint rate;
  gint channels;
  gboolean ret;
} AudioInfoFromCapsData;

GST_START_TEST (test_audio_info_from_caps)
{
  static const AudioInfoFromCapsData format_list[] = {
    /* raw format, positive */
    {"audio/x-raw, format = (string) S8, layout = (string) non-interleaved, "
          "rate = (int) 44100, channels = (int) 2", GST_AUDIO_FORMAT_S8,
        GST_AUDIO_LAYOUT_NON_INTERLEAVED, 44100, 2, TRUE},
    {"audio/x-raw, format = (string) U8, layout = (string) interleaved, "
          "rate = (int) 44100, channels = (int) 1", GST_AUDIO_FORMAT_U8,
        GST_AUDIO_LAYOUT_INTERLEAVED, 44100, 1, TRUE},

    /* raw format, negative */
    /* unknown format */
    {"audio/x-raw, format = (string) foo, layout = (string) interleaved, "
          "rate = (int) 44100, channels = (int) 2", GST_AUDIO_FORMAT_UNKNOWN,
        GST_AUDIO_LAYOUT_INTERLEAVED, 0, 0, FALSE},
    {"audio/x-raw, layout = (string) non-interleaved, "
          "rate = (int) 44100, channels = (int) 2", GST_AUDIO_FORMAT_UNKNOWN,
        GST_AUDIO_LAYOUT_INTERLEAVED, 0, 0, FALSE},
    /* unknown layout */
    {"audio/x-raw, format = (string) U8, "
          "rate = (int) 44100, channels = (int) 2", GST_AUDIO_FORMAT_UNKNOWN,
        GST_AUDIO_LAYOUT_INTERLEAVED, 0, 0, FALSE},
    /* unknown rate */
    {"audio/x-raw, format = (string) U8, layout = (string) interleaved, "
          "channels = (int) 2", GST_AUDIO_FORMAT_UNKNOWN,
        GST_AUDIO_LAYOUT_INTERLEAVED, 0, 0, FALSE},
    /* unknown channels */
    {"audio/x-raw, format = (string) U8, layout = (string) interleaved, "
          "rate = (int) 44100", GST_AUDIO_FORMAT_UNKNOWN,
        GST_AUDIO_LAYOUT_INTERLEAVED, 0, 0, FALSE},
    /* video caps */
    {"video/x-raw, format = (string) NV12",
        GST_AUDIO_FORMAT_UNKNOWN, GST_AUDIO_LAYOUT_INTERLEAVED, 0, 0, FALSE},

    /* encoded format, it allow empty fields */
    {"audio/x-opus", GST_AUDIO_FORMAT_ENCODED,
        GST_AUDIO_LAYOUT_INTERLEAVED, 0, 0, TRUE},
    /* rate only  */
    {"audio/x-opus, rate = (int) 44100", GST_AUDIO_FORMAT_ENCODED,
        GST_AUDIO_LAYOUT_INTERLEAVED, 44100, 0, TRUE},
    /* channels only  */
    {"audio/x-opus, channels = (int) 2", GST_AUDIO_FORMAT_ENCODED,
        GST_AUDIO_LAYOUT_INTERLEAVED, 0, 2, TRUE},
    /* rate and channels  */
    {"audio/x-opus, rate = (int) 44100, channels = (int) 2",
        GST_AUDIO_FORMAT_ENCODED, GST_AUDIO_LAYOUT_INTERLEAVED, 44100, 2, TRUE},
  };
  gint i;

  for (i = 0; i < G_N_ELEMENTS (format_list); i++) {
    GstAudioInfo info;
    GstCaps *caps;

    GST_LOG ("checking %dth format", i);

    caps = gst_caps_from_string (format_list[i].str);
    fail_unless (caps != NULL);

    gst_audio_info_init (&info);
    fail_unless_equals_int (gst_audio_info_from_caps (&info, caps),
        format_list[i].ret);

    if (format_list[i].ret) {
      fail_unless_equals_int (GST_AUDIO_INFO_FORMAT (&info),
          format_list[i].format);
      fail_unless_equals_int (GST_AUDIO_INFO_LAYOUT (&info),
          format_list[i].layout);
      fail_unless_equals_int (GST_AUDIO_INFO_RATE (&info), format_list[i].rate);
      fail_unless_equals_int (GST_AUDIO_INFO_CHANNELS (&info),
          format_list[i].channels);
    }

    gst_caps_unref (caps);
  }
}

GST_END_TEST;

GST_START_TEST (test_audio_make_raw_caps)
{
  GstCaps *caps, *expected;
  GstAudioFormat f1[] = { GST_AUDIO_FORMAT_U8 };
  GstAudioFormat f2[] = { GST_AUDIO_FORMAT_U8, GST_AUDIO_FORMAT_S8 };

  caps =
      gst_audio_make_raw_caps (f1, G_N_ELEMENTS (f1),
      GST_AUDIO_LAYOUT_INTERLEAVED);
  expected =
      gst_caps_from_string
      ("audio/x-raw, format = (string) U8, rate = (int) [ 1, max ], channels = (int) [ 1, max ], layout = (string) interleaved");
  fail_unless (gst_caps_is_equal (caps, expected));
  gst_caps_unref (caps);
  gst_caps_unref (expected);

  caps =
      gst_audio_make_raw_caps (f2, G_N_ELEMENTS (f2),
      GST_AUDIO_LAYOUT_NON_INTERLEAVED);
  expected =
      gst_caps_from_string
      ("audio/x-raw, format = (string) { U8, S8 }, rate = (int) [ 1, max ], channels = (int) [ 1, max ], layout = (string) non-interleaved");
  fail_unless (gst_caps_is_equal (caps, expected));
  gst_caps_unref (caps);
  gst_caps_unref (expected);

  caps = gst_audio_make_raw_caps (NULL, 0, GST_AUDIO_LAYOUT_INTERLEAVED);
  expected =
      gst_caps_from_string
      ("audio/x-raw, format = (string) { S8, U8, S16LE, S16BE, U16LE, U16BE, S24_32LE, S24_32BE, U24_32LE, U24_32BE, S32LE, S32BE, U32LE, U32BE, S24LE, S24BE, U24LE, U24BE, S20LE, S20BE, U20LE, U20BE, S18LE, S18BE, U18LE, U18BE, F32LE, F32BE, F64LE, F64BE }, rate = (int) [ 1, max ], channels = (int) [ 1, max ], layout = (string) interleaved");
  fail_unless (gst_caps_is_equal (caps, expected));
  gst_caps_unref (caps);
  gst_caps_unref (expected);
}

GST_END_TEST;

GST_START_TEST (test_audio_meta_serialize)
{
  GstBuffer *buf;
  GstAudioInfo info;
  GstAudioMeta *meta;

  GstAudioChannelPosition position[] = { GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT,
    GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT
  };
  gst_audio_info_init (&info);
  gst_audio_info_set_format (&info, GST_AUDIO_FORMAT_S32, 44100, 2, position);
  info.layout = GST_AUDIO_LAYOUT_NON_INTERLEAVED;

  /* Add audio meta */
  gsize samples = 1;
  gsize buf_size = 4 * 2 * samples;
  buf = gst_buffer_new_and_alloc (buf_size);
  gsize offsets[] = { 0, buf_size / 2 };
  meta = gst_buffer_add_audio_meta (buf, &info, samples, offsets);

  /* Serialize */
  GByteArray *data = g_byte_array_new ();
  fail_unless (gst_meta_serialize_simple ((GstMeta *) meta, data));
  gst_buffer_unref (buf);

  /* Create a new buffer */
  buf = gst_buffer_new_and_alloc (buf_size);
  guint32 consumed;
  meta = (GstAudioMeta *) gst_meta_deserialize (buf, data->data, data->len,
      &consumed);
  fail_unless (meta);
  fail_unless (consumed == data->len);
  g_byte_array_unref (data);

  /* Check meta's content */
  g_assert_cmpint (meta->info.finfo->format, ==, GST_AUDIO_FORMAT_S32);
  g_assert_cmpint (meta->info.layout, ==, GST_AUDIO_LAYOUT_NON_INTERLEAVED);
  g_assert_cmpint (meta->info.rate, ==, 44100);
  g_assert_cmpint (meta->info.channels, ==, 2);
  g_assert_cmpint (meta->info.position[0], ==,
      GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT);
  g_assert_cmpint (meta->info.position[1], ==,
      GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT);
  g_assert_cmpuint (meta->samples, ==, samples);
  g_assert_cmpuint (meta->offsets[0], ==, 0);
  g_assert_cmpuint (meta->offsets[1], ==, buf_size / 2);

  gst_buffer_unref (buf);
}

GST_END_TEST;

GST_START_TEST (test_audio_meta_serialize_65_chans)
{
  GstBuffer *buf;
  GstAudioInfo info;
  GstAudioMeta *meta;

  /* With more than 64 channels we cannot have positions */
  gst_audio_info_init (&info);
  gst_audio_info_set_format (&info, GST_AUDIO_FORMAT_S32, 44100, 65, NULL);
  info.layout = GST_AUDIO_LAYOUT_NON_INTERLEAVED;

  /* Add audio meta */
  gsize samples = 1;
  gsize buf_size = 4 * 65 * samples;
  buf = gst_buffer_new_and_alloc (buf_size);
  gsize offsets[65];
  for (int i = 0; i < 65; i++)
    offsets[i] = i * samples * 4;
  meta = gst_buffer_add_audio_meta (buf, &info, samples, offsets);

  /* Serialize */
  GByteArray *data = g_byte_array_new ();
  fail_unless (gst_meta_serialize_simple ((GstMeta *) meta, data));
  gst_buffer_unref (buf);

  /* Create a new buffer */
  buf = gst_buffer_new_and_alloc (buf_size);
  guint32 consumed;
  meta = (GstAudioMeta *) gst_meta_deserialize (buf, data->data, data->len,
      &consumed);
  fail_unless (meta);
  fail_unless (consumed == data->len);
  g_byte_array_unref (data);

  /* Check meta's content */
  g_assert_cmpint (meta->info.finfo->format, ==, GST_AUDIO_FORMAT_S32);
  g_assert_cmpint (meta->info.layout, ==, GST_AUDIO_LAYOUT_NON_INTERLEAVED);
  g_assert_cmpint (meta->info.rate, ==, 44100);
  g_assert_cmpint (meta->info.channels, ==, 65);
  g_assert_cmpuint (meta->samples, ==, samples);
  for (int i = 0; i < 65; i++)
    g_assert_cmpuint (meta->offsets[i], ==, i * samples * 4);

  gst_buffer_unref (buf);
}

GST_END_TEST;

static Suite *
audio_suite (void)
{
  Suite *s = suite_create ("audio support library");

  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_buffer_clip_unsupported_format);
  tcase_add_test (tc_chain, test_buffer_clip_time_start_and_stop);
  tcase_add_test (tc_chain, test_buffer_clip_time_start_and_stop_planar);
  tcase_add_test (tc_chain, test_buffer_clip_time_start);
  tcase_add_test (tc_chain, test_buffer_clip_time_start_planar);
  tcase_add_test (tc_chain, test_buffer_clip_time_stop);
  tcase_add_test (tc_chain, test_buffer_clip_time_stop_planar);
  tcase_add_test (tc_chain, test_buffer_clip_time_outside);
  tcase_add_test (tc_chain, test_buffer_clip_time_start_and_stop_no_meta);
  tcase_add_test (tc_chain, test_buffer_clip_time_no_timestamp);
  tcase_add_test (tc_chain, test_buffer_clip_time_handles_rounding);
  tcase_add_test (tc_chain, test_buffer_clip_samples_start_and_stop);
  tcase_add_test (tc_chain, test_buffer_clip_samples_start_and_stop_planar);
  tcase_add_test (tc_chain, test_buffer_clip_samples_start);
  tcase_add_test (tc_chain, test_buffer_clip_samples_stop);
  tcase_add_test (tc_chain, test_buffer_clip_samples_outside);
  tcase_add_test (tc_chain, test_buffer_clip_samples_start_and_stop_no_meta);
  tcase_add_test (tc_chain, test_buffer_clip_samples_no_timestamp);
  tcase_add_test (tc_chain, test_buffer_truncate_samples_offset_end);
  tcase_add_test (tc_chain, test_buffer_truncate_samples_no_timestamp);
  tcase_add_test (tc_chain, test_multichannel_checks);
  tcase_add_test (tc_chain, test_multichannel_reorder);
  tcase_add_test (tc_chain, test_audio_format_s8);
  tcase_add_test (tc_chain, test_audio_format_u8);
  tcase_add_test (tc_chain, test_fill_silence);
  tcase_add_test (tc_chain, test_stream_align);
  tcase_add_test (tc_chain, test_stream_align_reverse);
  tcase_add_test (tc_chain, test_audio_buffer_and_audio_meta);
  tcase_add_test (tc_chain, test_audio_info_from_caps);
  tcase_add_test (tc_chain, test_audio_make_raw_caps);
  tcase_add_test (tc_chain, test_audio_meta_serialize);
  tcase_add_test (tc_chain, test_audio_meta_serialize_65_chans);

  return s;
}

GST_CHECK_MAIN (audio);
