/* GStreamer
 *
 * SPDX-License-Identifier: LGPL-2.1
 *
 * Copyright (C) 2022, 2023 Collabora Ltd.
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

#ifdef HAVE_VALGRIND
# include <valgrind/valgrind.h>
#endif

#include <gst/check/gstcheck.h>

#include <gst/mse/mse.h>
#include <gst/mse/gstappendpipeline-private.h>
#include <gst/mse/gstmediasource-private.h>
#include <gst/mse/gstmediasourcetrack-private.h>
#include <gst/mse/gstmediasourcetrackbuffer-private.h>
#include <gst/mse/gstmediasourcesamplemap-private.h>

static GstCheckLogFilter *
add_log_filter (GLogLevelFlags level, const gchar * regex)
{
  GRegex *gregex = g_regex_new (regex, 0, 0, NULL);
  return gst_check_add_log_filter ("GStreamer-MSE", level, gregex, NULL, NULL,
      NULL);
}

static gchar *
test_mp4_path (void)
{
  return g_build_filename (GST_TEST_FILES_PATH, "mse.mp4", NULL);
}

static gchar *
test_webm_path (void)
{
  return g_build_filename (GST_TEST_FILES_PATH, "mse.webm", NULL);
}

static GstMediaSource *
opened_media_source (void)
{
  GstMediaSource *media_source = gst_media_source_new ();
  media_source->ready_state = GST_MEDIA_SOURCE_READY_STATE_OPEN;
  return media_source;
}

static GstSample *
new_empty_sample_full (GstClockTime dts, GstClockTime pts,
    GstClockTime duration, GstBufferFlags flags, GstCaps * caps,
    GstSegment * segment, GstStructure * info)
{
  GstBuffer *buffer = gst_buffer_new ();
  GST_BUFFER_DTS (buffer) = dts;
  GST_BUFFER_PTS (buffer) = pts;
  GST_BUFFER_DURATION (buffer) = duration;
  GST_BUFFER_FLAGS (buffer) = flags;
  GstSample *sample = gst_sample_new (buffer, caps, segment, info);
  gst_buffer_unref (buffer);
  return sample;
}

static GstSample *
new_empty_sample_with_timing (GstClockTime dts, GstClockTime pts,
    GstClockTime duration)
{
  return new_empty_sample_full (dts, pts, duration, 0, NULL, NULL, NULL);
}

static GstSample *
new_sample_with_bytes_and_timing (GBytes * bytes, GstClockTime dts,
    GstClockTime pts, GstClockTime duration)
{
  GstBuffer *buffer = gst_buffer_new_wrapped_bytes (bytes);
  GST_BUFFER_DTS (buffer) = dts;
  GST_BUFFER_PTS (buffer) = pts;
  GST_BUFFER_DURATION (buffer) = duration;
  GstSample *sample = gst_sample_new (buffer, NULL, NULL, NULL);
  gst_buffer_unref (buffer);
  g_bytes_unref (bytes);
  return sample;
}

GST_START_TEST (test_create_and_free)
{
  GstMediaSource *media_source = gst_media_source_new ();
  fail_unless (GST_IS_MEDIA_SOURCE (media_source));
  gst_check_object_destroyed_on_unref (media_source);
}

GST_END_TEST;

GST_START_TEST (test_create_initial_state)
{
  GstMediaSource *media_source = gst_media_source_new ();

  GstSourceBufferList *buffers =
      gst_media_source_get_source_buffers (media_source);
  GstSourceBufferList *active_buffers =
      gst_media_source_get_active_source_buffers (media_source);

  fail_unless (gst_media_source_get_ready_state (media_source) ==
      GST_MEDIA_SOURCE_READY_STATE_CLOSED);
  fail_unless (gst_source_buffer_list_get_length (buffers) == 0);
  fail_unless (gst_source_buffer_list_get_length (active_buffers) == 0);
  fail_unless (gst_media_source_get_position (media_source) ==
      GST_CLOCK_TIME_NONE);

  gst_object_unref (media_source);
  gst_object_unref (buffers);
  gst_object_unref (active_buffers);
}

GST_END_TEST;

GST_START_TEST (test_add_source_buffer_with_content_type_null)
{
  add_log_filter (G_LOG_LEVEL_CRITICAL,
      "^.*_add_source_buffer: assertion 'type != NULL' failed");

  GstMediaSource *media_source = gst_media_source_new ();

  g_assert_null (gst_media_source_add_source_buffer (media_source, NULL, NULL));

  gst_object_unref (media_source);
}

GST_END_TEST;

GST_START_TEST (test_add_source_buffer_with_content_type_empty)
{
  GError *error = NULL;
  GstMediaSource *media_source = gst_media_source_new ();
  GstSourceBuffer *source_buffer =
      gst_media_source_add_source_buffer (media_source, "", &error);

  g_assert_null (source_buffer);
  g_assert_error (error, GST_MEDIA_SOURCE_ERROR, GST_MEDIA_SOURCE_ERROR_TYPE);

  gst_object_unref (media_source);
  g_clear_error (&error);
}

GST_END_TEST;

GST_START_TEST (test_add_source_buffer_with_content_type_fake)
{
  GError *error = NULL;
  GstMediaSource *media_source = gst_media_source_new ();
  GstSourceBuffer *source_buffer =
      gst_media_source_add_source_buffer (media_source, "fake/type", &error);

  g_assert_null (source_buffer);
  g_assert_error (error, GST_MEDIA_SOURCE_ERROR,
      GST_MEDIA_SOURCE_ERROR_NOT_SUPPORTED);

  gst_object_unref (media_source);
  g_clear_error (&error);
}

GST_END_TEST;

GST_START_TEST (test_add_source_buffer_to_unopened_media_source)
{
  GError *error = NULL;
  GstMediaSource *media_source = gst_media_source_new ();
  GstSourceBuffer *source_buffer =
      gst_media_source_add_source_buffer (media_source, "video/webm", &error);

  g_assert_null (source_buffer);
  g_assert_error (error, GST_MEDIA_SOURCE_ERROR,
      GST_MEDIA_SOURCE_ERROR_INVALID_STATE);

  gst_object_unref (media_source);
  g_clear_error (&error);
}

GST_END_TEST;

GST_START_TEST (test_add_source_buffer_to_opened_media_source)
{
  GError *error = NULL;
  GstMediaSource *media_source = opened_media_source ();
  GstSourceBufferList *buffers =
      gst_media_source_get_source_buffers (media_source);
  guint n_buffers_before = gst_source_buffer_list_get_length (buffers);
  GstSourceBuffer *source_buffer =
      gst_media_source_add_source_buffer (media_source, "video/webm", &error);
  guint n_buffers_after = gst_source_buffer_list_get_length (buffers);

  fail_unless (GST_IS_SOURCE_BUFFER (source_buffer));
  g_assert_no_error (error);
  fail_unless (n_buffers_before < n_buffers_after);

  g_object_unref (media_source);
  g_object_unref (buffers);
  g_object_unref (source_buffer);
  g_clear_error (&error);
}

GST_END_TEST;

GST_START_TEST (test_remove_source_buffer_from_unrelated_media_source)
{
  GError *error = NULL;
  GstMediaSource *a = opened_media_source ();
  GstMediaSource *b = opened_media_source ();
  GstSourceBuffer *buffer_in_b =
      gst_media_source_add_source_buffer (b, "video/webm", &error);

  gst_media_source_remove_source_buffer (a, buffer_in_b, &error);
  g_assert_error (error, GST_MEDIA_SOURCE_ERROR,
      GST_MEDIA_SOURCE_ERROR_NOT_FOUND);

  gst_object_unref (a);
  gst_object_unref (b);
  gst_object_unref (buffer_in_b);
  g_clear_error (&error);
}

GST_END_TEST;

GST_START_TEST (test_remove_source_buffer_from_parent_media_source)
{
  GError *error = NULL;
  GstMediaSource *media_source = opened_media_source ();
  GstSourceBufferList *buffers =
      gst_media_source_get_source_buffers (media_source);
  GstSourceBuffer *buffer =
      gst_media_source_add_source_buffer (media_source, "video/webm", &error);

  guint n_buffers_before = gst_source_buffer_list_get_length (buffers);
  gst_media_source_remove_source_buffer (media_source, buffer, &error);
  guint n_buffers_after = gst_source_buffer_list_get_length (buffers);

  g_assert_no_error (error);
  fail_unless (n_buffers_before > n_buffers_after);

  gst_object_unref (media_source);
  gst_object_unref (buffers);
  gst_object_unref (buffer);
  g_clear_error (&error);
}

GST_END_TEST;

GST_START_TEST (test_set_live_seekable_range_on_unopened_media_source)
{
  GError *error = NULL;
  GstMediaSource *media_source = gst_media_source_new ();

  gst_media_source_set_live_seekable_range (media_source, 0, 1, &error);

  g_assert_error (error, GST_MEDIA_SOURCE_ERROR,
      GST_MEDIA_SOURCE_ERROR_INVALID_STATE);

  gst_object_unref (media_source);
  g_clear_error (&error);
}

GST_END_TEST;

GST_START_TEST (test_set_backwards_live_seekable_range_on_opened_media_source)
{
  GError *error = NULL;
  GstMediaSource *media_source = opened_media_source ();

  gst_media_source_set_live_seekable_range (media_source, 2, 1, &error);

  GstMediaSourceRange range = {
    .start = GST_CLOCK_TIME_NONE,
    .end = GST_CLOCK_TIME_NONE,
  };
  gst_media_source_get_live_seekable_range (media_source, &range);

  g_assert_error (error, GST_MEDIA_SOURCE_ERROR, GST_MEDIA_SOURCE_ERROR_TYPE);
  fail_unless (range.start == 0);
  fail_unless (range.end == 0);

  gst_object_unref (media_source);
  g_clear_error (&error);
}

GST_END_TEST;

GST_START_TEST (test_set_live_seekable_range_on_opened_media_source)
{
  GError *error = NULL;
  GstClockTime start = 1, end = 2;
  GstMediaSource *media_source = opened_media_source ();

  gst_media_source_set_live_seekable_range (media_source, start, end, &error);

  GstMediaSourceRange range = {
    .start = GST_CLOCK_TIME_NONE,
    .end = GST_CLOCK_TIME_NONE,
  };
  gst_media_source_get_live_seekable_range (media_source, &range);

  g_assert_no_error (error);
  fail_unless (range.start == start);
  fail_unless (range.end == end);

  gst_object_unref (media_source);
  g_clear_error (&error);
}

GST_END_TEST;

GST_START_TEST (test_clear_live_seekable_range_on_unopened_media_source)
{
  GError *error = NULL;
  GstMediaSource *media_source = gst_media_source_new ();

  gst_media_source_clear_live_seekable_range (media_source, &error);

  g_assert_error (error, GST_MEDIA_SOURCE_ERROR,
      GST_MEDIA_SOURCE_ERROR_INVALID_STATE);

  gst_object_unref (media_source);
  g_clear_error (&error);
}

GST_END_TEST;

GST_START_TEST (test_clear_live_seekable_range_on_opened_media_source)
{
  GError *error = NULL;
  GstMediaSource *media_source = opened_media_source ();
  gst_media_source_set_live_seekable_range (media_source, 1, 2, NULL);

  gst_media_source_clear_live_seekable_range (media_source, &error);
  GstMediaSourceRange range = {
    .start = GST_CLOCK_TIME_NONE,
    .end = GST_CLOCK_TIME_NONE,
  };
  gst_media_source_get_live_seekable_range (media_source, &range);

  g_assert_no_error (error);
  fail_unless (range.start == 0);
  fail_unless (range.end == 0);

  gst_object_unref (media_source);
  g_clear_error (&error);
}

GST_END_TEST;

GST_START_TEST (test_append_pipeline_create_and_free)
{
  GError *error = NULL;
  GstAppendPipeline *pipeline = gst_append_pipeline_new (NULL, NULL, &error);
  g_assert_no_error (error);
  fail_unless (GST_IS_APPEND_PIPELINE (pipeline));
  gst_check_object_destroyed_on_unref (pipeline);
  g_clear_error (&error);
}

GST_END_TEST;

typedef struct
{
  GMutex mutex;
  GCond eos_cond;
  GCond error_cond;
} AppendPipelineTestContext;

static void
test_append_pipeline_eos (GstAppendPipeline * pipeline,
    GstMediaSourceTrack * track, gpointer user_data)
{
  AppendPipelineTestContext *context = user_data;
  g_mutex_lock (&context->mutex);
  g_cond_signal (&context->eos_cond);
  g_mutex_unlock (&context->mutex);
  GST_DEBUG_OBJECT (pipeline, "signalled eos");
}

static void
test_append_pipeline_error (GstAppendPipeline * pipeline, gpointer user_data)
{
  AppendPipelineTestContext *context = user_data;
  g_mutex_lock (&context->mutex);
  g_cond_signal (&context->error_cond);
  g_mutex_unlock (&context->mutex);
  GST_DEBUG_OBJECT (pipeline, "signalled error");
}

static void
test_append_pipeline_await_eos (GstAppendPipeline * pipeline,
    AppendPipelineTestContext * context)
{
  GST_DEBUG_OBJECT (pipeline, "waiting for eos");
  g_mutex_lock (&context->mutex);
  while (!gst_append_pipeline_get_eos (pipeline)) {
    g_cond_wait (&context->eos_cond, &context->mutex);
  }
  g_mutex_unlock (&context->mutex);
  GST_DEBUG_OBJECT (pipeline, "received eos");
}

static void
test_append_pipeline_await_error (GstAppendPipeline * pipeline,
    AppendPipelineTestContext * context)
{
  GST_DEBUG_OBJECT (pipeline, "waiting for error");
  g_mutex_lock (&context->mutex);
  while (!gst_append_pipeline_get_failed (pipeline)) {
    g_cond_wait (&context->error_cond, &context->mutex);
  }
  g_mutex_unlock (&context->mutex);
  GST_DEBUG_OBJECT (pipeline, "received error");
}

static void
test_append_pipeline (const gchar * filename)
{
  AppendPipelineTestContext context = { 0 };
  GstAppendPipelineCallbacks callbacks = {
    .eos = test_append_pipeline_eos,
  };
  GstAppendPipeline *pipeline =
      gst_append_pipeline_new (&callbacks, &context, NULL);
  GError *error = NULL;

  gchar *data;
  gsize length;

  g_file_get_contents (filename, &data, &length, &error);
  g_assert_no_error (error);

  fail_unless (gst_append_pipeline_append (pipeline,
          gst_buffer_new_wrapped (data, length)) == GST_FLOW_OK);

  gst_append_pipeline_eos (pipeline);

  test_append_pipeline_await_eos (pipeline, &context);

  fail_if (gst_append_pipeline_get_failed (pipeline));

  gst_object_unref (pipeline);
  g_clear_error (&error);
}

GST_START_TEST (test_append_pipeline_mp4)
{
  gchar *filename = test_mp4_path ();
  test_append_pipeline (filename);
  g_free (filename);
}

GST_END_TEST;

GST_START_TEST (test_append_pipeline_webm)
{
  gchar *filename = test_webm_path ();
  test_append_pipeline (filename);
  g_free (filename);
}

GST_END_TEST;

static GstAppendPipeline *
failed_append_pipeline (GstAppendPipelineCallbacks * callbacks,
    AppendPipelineTestContext * context)
{
  GstAppendPipeline *pipeline =
      gst_append_pipeline_new (callbacks, context, NULL);

  gst_append_pipeline_fail (pipeline);

  return pipeline;
}

GST_START_TEST (test_append_pipeline_invalid_data_triggers_error)
{
  AppendPipelineTestContext context = { 0 };
  GstAppendPipelineCallbacks callbacks = {
    .eos = test_append_pipeline_eos,
    .error = test_append_pipeline_error,
  };
  GstAppendPipeline *pipeline = failed_append_pipeline (&callbacks, &context);

  test_append_pipeline_await_error (pipeline, &context);

  gst_object_unref (pipeline);
}

GST_END_TEST;

GST_START_TEST (test_append_pipeline_invalid_data_triggers_eos)
{
  AppendPipelineTestContext context = { 0 };
  GstAppendPipelineCallbacks callbacks = {
    .eos = test_append_pipeline_eos,
    .error = test_append_pipeline_error,
  };
  GstAppendPipeline *pipeline = failed_append_pipeline (&callbacks, &context);

  test_append_pipeline_await_eos (pipeline, &context);

  gst_object_unref (pipeline);
}

GST_END_TEST;

GST_START_TEST (test_append_pipeline_reset_recovery)
{
  AppendPipelineTestContext context = { 0 };
  GstAppendPipelineCallbacks callbacks = {
    .eos = test_append_pipeline_eos,
    .error = test_append_pipeline_error,
  };
  GstAppendPipeline *pipeline = failed_append_pipeline (&callbacks, &context);

  test_append_pipeline_await_error (pipeline, &context);
  fail_unless (gst_append_pipeline_get_failed (pipeline));

  fail_unless (gst_append_pipeline_reset (pipeline));
  fail_if (gst_append_pipeline_get_failed (pipeline));

  gchar *data;
  gsize length;
  {
    GError *error = NULL;
    gchar *filename = test_webm_path ();
    g_file_get_contents (filename, &data, &length, &error);
    g_assert_no_error (error);
    g_clear_error (&error);
    g_free (filename);
  }

  fail_unless (gst_append_pipeline_append (pipeline,
          gst_buffer_new_wrapped (data, length)) == GST_FLOW_OK);

  gst_append_pipeline_eos (pipeline);

  test_append_pipeline_await_eos (pipeline, &context);

  fail_if (gst_append_pipeline_get_failed (pipeline));

  gst_object_unref (pipeline);
}

GST_END_TEST;

GST_START_TEST (test_track_create_and_free)
{
  GstMediaSourceTrack *track =
      gst_media_source_track_new (GST_MEDIA_SOURCE_TRACK_TYPE_OTHER, "");
  fail_unless (GST_IS_MEDIA_SOURCE_TRACK (track));
  gst_check_object_destroyed_on_unref (track);
}

GST_END_TEST;

GST_START_TEST (test_track_create_with_invalid_type)
{
  add_log_filter (G_LOG_LEVEL_CRITICAL,
      "^.*track_new_full: assertion .*type .* failed");

  g_assert_null (gst_media_source_track_new (-1, ""));
  g_assert_null (gst_media_source_track_new (GST_MEDIA_SOURCE_TRACK_TYPE_OTHER +
          1, ""));
}

GST_END_TEST;

GST_START_TEST (test_track_push_with_adequate_space)
{
  GstMediaSourceTrack *track =
      gst_media_source_track_new_with_size (GST_MEDIA_SOURCE_TRACK_TYPE_OTHER,
      "", 1);
  GstBuffer *buffer = gst_buffer_new ();
  GstSample *sample = gst_sample_new (buffer, NULL, NULL, NULL);
  gboolean result = gst_media_source_track_push (track, sample);
  fail_unless (result);
  gst_buffer_unref (buffer);
  gst_object_unref (track);
}

GST_END_TEST;

GST_START_TEST (test_track_push_with_inadequate_space)
{
  GstMediaSourceTrack *track =
      gst_media_source_track_new_with_size (GST_MEDIA_SOURCE_TRACK_TYPE_OTHER,
      "", 0);
  GstBuffer *buffer = gst_buffer_new ();
  GstSample *sample = gst_sample_new (buffer, NULL, NULL, NULL);
  gboolean result = gst_media_source_track_try_push (track, sample);
  fail_if (result);
  gst_sample_unref (sample);
  gst_buffer_unref (buffer);
  gst_object_unref (track);
}

GST_END_TEST;

GST_START_TEST (test_track_buffer_empty)
{
  GstMediaSourceTrackBuffer *buffer = gst_media_source_track_buffer_new ();

  GArray *ranges = gst_media_source_track_buffer_get_ranges (buffer);
  fail_unless_equals_uint64 (ranges->len, 0);

  gst_object_unref (buffer);
  g_array_unref (ranges);
}

GST_END_TEST;

GST_START_TEST (test_track_buffer_single_span)
{
  GstMediaSourceTrackBuffer *buffer = gst_media_source_track_buffer_new ();

  GstSample *sample = new_empty_sample_with_timing (0, 0, 1);
  gst_media_source_track_buffer_add (buffer, sample);

  GArray *ranges = gst_media_source_track_buffer_get_ranges (buffer);
  fail_unless_equals_uint64 (ranges->len, 1);

  GstMediaSourceRange range = g_array_index (ranges, GstMediaSourceRange, 0);
  fail_unless_equals_uint64 (range.start, 0);
  fail_unless_equals_uint64 (range.end, 1);

  gst_sample_unref (sample);
  gst_object_unref (buffer);
  g_array_unref (ranges);
}

GST_END_TEST;

GST_START_TEST (test_track_buffer_continuous_span)
{
  GstMediaSourceTrackBuffer *buffer = gst_media_source_track_buffer_new ();

  GstClockTime a_start = 0;
  GstClockTime a_duration = GST_SECOND;
  GstClockTime b_start = a_start + a_duration;
  GstClockTime b_duration = a_duration;
  GstSample *a = new_empty_sample_with_timing (a_start, a_start, a_duration);
  GstSample *b = new_empty_sample_with_timing (b_start, b_start, b_duration);
  gst_media_source_track_buffer_add (buffer, a);
  gst_media_source_track_buffer_add (buffer, b);

  GArray *ranges = gst_media_source_track_buffer_get_ranges (buffer);
  fail_unless_equals_uint64 (ranges->len, 1);

  GstMediaSourceRange range = g_array_index (ranges, GstMediaSourceRange, 0);
  fail_unless_equals_uint64 (range.start, a_start);
  fail_unless_equals_uint64 (range.end, a_start + a_duration + b_duration);

  gst_sample_unref (a);
  gst_sample_unref (b);
  gst_object_unref (buffer);
  g_array_unref (ranges);
}

GST_END_TEST;

GST_START_TEST (test_track_buffer_discontinuous_span)
{
  GstMediaSourceTrackBuffer *buffer = gst_media_source_track_buffer_new ();

  GstClockTime a_start = 0;
  GstClockTime a_duration = GST_SECOND;
  GstClockTime b_start = a_start + a_duration + GST_SECOND;
  GstClockTime b_duration = a_duration;
  GstSample *a = new_empty_sample_with_timing (a_start, a_start, a_duration);
  GstSample *b = new_empty_sample_with_timing (b_start, b_start, b_duration);
  gst_media_source_track_buffer_add (buffer, a);
  gst_media_source_track_buffer_add (buffer, b);

  GArray *ranges = gst_media_source_track_buffer_get_ranges (buffer);
  fail_unless_equals_uint64 (ranges->len, 2);

  GstMediaSourceRange range_a = g_array_index (ranges, GstMediaSourceRange, 0);
  fail_unless_equals_uint64 (range_a.start, a_start);
  fail_unless_equals_uint64 (range_a.end, a_start + a_duration);

  GstMediaSourceRange range_b = g_array_index (ranges, GstMediaSourceRange, 1);
  fail_unless_equals_uint64 (range_b.start, b_start);
  fail_unless_equals_uint64 (range_b.end, b_start + b_duration);

  gst_sample_unref (a);
  gst_sample_unref (b);
  gst_object_unref (buffer);
  g_array_unref (ranges);
}

GST_END_TEST;

GST_START_TEST (test_source_buffer_generate_timestamps_mp4)
{
  GstMediaSource *media_source = opened_media_source ();
  GstSourceBuffer *source_buffer = gst_media_source_add_source_buffer
      (media_source, "video/mp4", NULL);

  fail_unless_equals_uint64 (gst_source_buffer_get_append_mode (source_buffer),
      GST_SOURCE_BUFFER_APPEND_MODE_SEGMENTS);

  gst_object_unref (source_buffer);
  gst_object_unref (media_source);
}

GST_END_TEST;

GST_START_TEST (test_source_buffer_generate_timestamps_aac)
{
  GstMediaSource *media_source = opened_media_source ();
  GstSourceBuffer *source_buffer = gst_media_source_add_source_buffer
      (media_source, "audio/aac", NULL);

  fail_unless (GST_IS_SOURCE_BUFFER (source_buffer));

  fail_unless_equals_uint64 (gst_source_buffer_get_append_mode (source_buffer),
      GST_SOURCE_BUFFER_APPEND_MODE_SEQUENCE);

  gst_object_unref (source_buffer);
  gst_object_unref (media_source);
}

GST_END_TEST;

GST_START_TEST (test_source_buffer_change_content_type_null)
{
  GstMediaSource *media_source = opened_media_source ();
  GstSourceBuffer *source_buffer = gst_media_source_add_source_buffer
      (media_source, "video/mp4", NULL);

  fail_unless (GST_IS_SOURCE_BUFFER (source_buffer));

  GError *error = NULL;
  gst_source_buffer_change_content_type (source_buffer, NULL, &error);
  g_assert_error (error, GST_MEDIA_SOURCE_ERROR, GST_MEDIA_SOURCE_ERROR_TYPE);

  g_clear_error (&error);
  gst_object_unref (source_buffer);
  gst_object_unref (media_source);
}

GST_END_TEST;

GST_START_TEST (test_source_buffer_change_content_type_empty)
{
  GstMediaSource *media_source = opened_media_source ();
  GstSourceBuffer *source_buffer = gst_media_source_add_source_buffer
      (media_source, "video/mp4", NULL);

  fail_unless (GST_IS_SOURCE_BUFFER (source_buffer));

  GError *error = NULL;
  gst_source_buffer_change_content_type (source_buffer, "", &error);
  g_assert_error (error, GST_MEDIA_SOURCE_ERROR, GST_MEDIA_SOURCE_ERROR_TYPE);

  g_clear_error (&error);
  gst_object_unref (source_buffer);
  gst_object_unref (media_source);
}

GST_END_TEST;

GST_START_TEST (test_source_buffer_change_content_type)
{
  GstMediaSource *media_source = opened_media_source ();
  GstSourceBuffer *source_buffer = gst_media_source_add_source_buffer
      (media_source, "video/mp4", NULL);

  fail_unless (GST_IS_SOURCE_BUFFER (source_buffer));

  GError *error = NULL;
  gst_source_buffer_change_content_type (source_buffer, "video/webm", &error);
  g_assert_error (error, GST_MEDIA_SOURCE_ERROR,
      GST_MEDIA_SOURCE_ERROR_NOT_SUPPORTED);

  gst_object_unref (source_buffer);
  gst_object_unref (media_source);
  g_clear_error (&error);
}

GST_END_TEST;

static const gchar *unsupported_content_types[] = {
  "xxx",
  "text/html",
  "image/jpeg",
};

GST_START_TEST (test_media_source_unsupported_content_type)
{
  const gchar *content_type = unsupported_content_types[__i__];
  ck_assert_msg (!gst_media_source_is_type_supported (content_type),
      "%s should be rejected as an unsupported MIME type", content_type);
}

GST_END_TEST;

static const gchar *valid_mp4_content_types[] = {
  "video/mp4;codecs=\"avc1.4d001e\"",   // H.264 Main Profile level 3.0
  "video/mp4;codecs=\"avc1.42001e\"",   // H.264 Baseline Profile level 3.0
  "audio/mp4;codecs=\"mp4a.40.2\"",     // MPEG4 AAC-LC
  "audio/mp4;codecs=\"mp4a.40.5\"",     // MPEG4 HE-AAC
  "audio/mp4;codecs=\"mp4a.67\"",       // MPEG2 AAC-LC
  "video/mp4;codecs=\"mp4a.40.2\"",
  "video/mp4;codecs=\"avc1.4d001e,mp4a.40.2\"",
  "video/mp4;codecs=\"mp4a.40.2 , avc1.4d001e \"",
  "video/mp4;codecs=\"avc1.4d001e,mp4a.40.5\"",
  "audio/mp4;codecs=\"Opus\"",
  "video/mp4;codecs=\"Opus\"",
  "audio/mp4;codecs=\"fLaC\"",
  "video/mp4;codecs=\"fLaC\"",
};

GST_START_TEST (test_media_source_supported_mp4_content_type)
{
  const gchar *content_type = valid_mp4_content_types[__i__];
  ck_assert_msg (gst_media_source_is_type_supported (content_type),
      "%s must be a supported MP4 content type", content_type);
}

GST_END_TEST;

static const gchar *valid_webm_content_types[] = {
  "video/webm;codecs=\"vp8\"",
  "video/webm;codecs=\"vorbis\"",
  "video/webm;codecs=\"vp8,vorbis\"",
  "video/webm;codecs=\"vorbis, vp8\"",
  "audio/webm;codecs=\"vorbis\"",
  "AUDIO/WEBM;CODECS=\"vorbis\"",
  "audio/webm;codecs=vorbis;test=\"6\"",
  "audio/webm;codecs=\"opus\"",
  "video/webm;codecs=\"opus\"",
};

GST_START_TEST (test_media_source_supported_webm_content_type)
{
  const gchar *content_type = valid_webm_content_types[__i__];
  ck_assert_msg (gst_media_source_is_type_supported (content_type),
      "%s must be a supported WebM content type", content_type);
}

GST_END_TEST;

GST_START_TEST (test_sample_map_create_and_destroy)
{
  GstMediaSourceSampleMap *map = gst_media_source_sample_map_new ();
  gst_check_object_destroyed_on_unref (map);
}

GST_END_TEST;

GST_START_TEST (test_sample_map_add_valid_sample)
{
  GstMediaSourceSampleMap *map = gst_media_source_sample_map_new ();

  GstSample *sample = new_empty_sample_with_timing (0, 0, 0);

  fail_if (gst_media_source_sample_map_contains (map, sample));

  gst_media_source_sample_map_add (map, sample);

  fail_unless (gst_media_source_sample_map_contains (map, sample));

  gst_object_unref (map);
  gst_sample_unref (sample);
}

GST_END_TEST;

GST_START_TEST (test_sample_map_add_invalid_sample)
{
  add_log_filter (G_LOG_LEVEL_CRITICAL,
      "^.*_sample_map_add: assertion .* failed");

  GstMediaSourceSampleMap *map = gst_media_source_sample_map_new ();

  GstSample *sample = new_empty_sample_with_timing (GST_CLOCK_TIME_NONE,
      GST_CLOCK_STIME_NONE, GST_CLOCK_TIME_NONE);

  gst_media_source_sample_map_add (map, sample);

  fail_if (gst_media_source_sample_map_contains (map, sample));

  gst_object_unref (map);
  gst_sample_unref (sample);
}

GST_END_TEST;

GST_START_TEST (test_sample_map_remove_sample)
{
  GstMediaSourceSampleMap *map = gst_media_source_sample_map_new ();

  GstSample *sample = new_empty_sample_with_timing (0, 0, 0);
  gst_media_source_sample_map_add (map, sample);

  gst_media_source_sample_map_remove (map, sample);

  fail_if (gst_media_source_sample_map_contains (map, sample));

  gst_object_unref (map);
  gst_sample_unref (sample);
}

GST_END_TEST;

GST_START_TEST (test_sample_map_remove_range_from_start)
{
  GstMediaSourceSampleMap *map = gst_media_source_sample_map_new ();

  GstSample *samples_to_remove[100] = { NULL };
  for (guint i = 0; i < G_N_ELEMENTS (samples_to_remove); i++) {
    GstClockTime time = i;
    GstSample *sample = new_empty_sample_with_timing (time, time, 1);
    gst_media_source_sample_map_add (map, sample);
    samples_to_remove[i] = sample;
  }
  GstSample *samples_to_preserve[100] = { NULL };
  for (guint i = 0; i < G_N_ELEMENTS (samples_to_preserve); i++) {
    GstClockTime time = i + G_N_ELEMENTS (samples_to_remove);
    GstSample *sample = new_empty_sample_with_timing (time, time, 0);
    gst_media_source_sample_map_add (map, sample);
    samples_to_preserve[i] = sample;
  }

  gst_media_source_sample_map_remove_range_from_start (map, 100);

  for (guint i = 0; i < G_N_ELEMENTS (samples_to_remove); i++) {
    GstSample *sample = samples_to_remove[i];
    fail_if (gst_media_source_sample_map_contains (map, sample));
    gst_sample_unref (sample);
  }
  for (guint i = 0; i < G_N_ELEMENTS (samples_to_preserve); i++) {
    GstSample *sample = samples_to_preserve[i];
    fail_unless (gst_media_source_sample_map_contains (map, sample));
    gst_sample_unref (sample);
  }

  gst_object_unref (map);
}

GST_END_TEST;

GST_START_TEST (test_sample_map_remove_range_from_start_byte_count)
{
  GstMediaSourceSampleMap *map = gst_media_source_sample_map_new ();

  GstSample *samples_to_remove[100] = { NULL };
  const guint8 chunk[1000] = { 0 };
  gsize total_bytes_to_remove = 0;
  for (guint i = 0; i < G_N_ELEMENTS (samples_to_remove); i++) {
    GstClockTime time = i;
    gsize buffer_size = g_random_int_range (0, G_N_ELEMENTS (chunk));
    GBytes *bytes = g_bytes_new_static (chunk, buffer_size);
    total_bytes_to_remove += buffer_size;
    GstSample *sample = new_sample_with_bytes_and_timing (bytes, time, time, 1);
    gst_media_source_sample_map_add (map, sample);
    samples_to_remove[i] = sample;
  }
  GstSample *samples_to_preserve[100] = { NULL };
  for (guint i = 0; i < G_N_ELEMENTS (samples_to_preserve); i++) {
    GstClockTime time = i + G_N_ELEMENTS (samples_to_remove);
    GBytes *bytes = g_bytes_new_static (chunk, 1);
    GstSample *sample = new_sample_with_bytes_and_timing (bytes, time, time, 0);
    gst_media_source_sample_map_add (map, sample);
    samples_to_preserve[i] = sample;
  }

  gsize bytes_removed =
      gst_media_source_sample_map_remove_range_from_start (map,
      G_N_ELEMENTS (samples_to_remove));
  fail_unless_equals_uint64 (bytes_removed, total_bytes_to_remove);

  for (guint i = 0; i < G_N_ELEMENTS (samples_to_remove); i++) {
    gst_sample_unref (samples_to_remove[i]);
  }
  for (guint i = 0; i < G_N_ELEMENTS (samples_to_preserve); i++) {
    gst_sample_unref (samples_to_preserve[i]);
  }

  gst_object_unref (map);
}

GST_END_TEST;

#define DEFAULT_TCASE_TIMEOUT 15

#ifdef HAVE_VALGRIND
#define TCASE_TIMEOUT (RUNNING_ON_VALGRIND ? (5 * 60) : DEFAULT_TCASE_TIMEOUT)
#else
#define TCASE_TIMEOUT DEFAULT_TCASE_TIMEOUT
#endif

static inline TCase *
new_tcase (const gchar * name)
{
  TCase *tcase = tcase_create (name);
  tcase_set_timeout (tcase, TCASE_TIMEOUT);
  return tcase;
}

static Suite *
mse_suite (void)
{
  Suite *s = suite_create ("GstMse");

  TCase *tc_media_source = new_tcase ("GstMediaSource");
  TCase *tc_source_buffer = new_tcase ("GstSourceBuffer");
  TCase *tc_source_buffer_list = new_tcase ("GstSourceBufferList");
  TCase *tc_append_pipeline = new_tcase ("GstAppendPipeline");
  TCase *tc_track = new_tcase ("GstMediaSourceTrack");
  TCase *tc_track_buffer = new_tcase ("GstMediaSourceTrackBuffer");
  TCase *tc_sample_map = new_tcase ("GstMediaSourceSampleMap");

  tcase_add_test (tc_media_source, test_create_and_free);
  tcase_add_test (tc_media_source, test_create_initial_state);
  tcase_add_test (tc_media_source,
      test_add_source_buffer_with_content_type_null);
  tcase_add_test (tc_media_source,
      test_add_source_buffer_with_content_type_empty);
  tcase_add_test (tc_media_source,
      test_add_source_buffer_with_content_type_fake);
  tcase_add_test (tc_media_source,
      test_add_source_buffer_to_unopened_media_source);
  tcase_add_test (tc_media_source,
      test_add_source_buffer_to_opened_media_source);
  tcase_add_test (tc_media_source,
      test_remove_source_buffer_from_unrelated_media_source);
  tcase_add_test (tc_media_source,
      test_remove_source_buffer_from_parent_media_source);
  tcase_add_test (tc_media_source,
      test_set_live_seekable_range_on_unopened_media_source);
  tcase_add_test (tc_media_source,
      test_set_backwards_live_seekable_range_on_opened_media_source);
  tcase_add_test (tc_media_source,
      test_set_live_seekable_range_on_opened_media_source);
  tcase_add_test (tc_media_source,
      test_clear_live_seekable_range_on_unopened_media_source);
  tcase_add_test (tc_media_source,
      test_clear_live_seekable_range_on_opened_media_source);
  tcase_add_loop_test (tc_media_source,
      test_media_source_unsupported_content_type,
      0, G_N_ELEMENTS (unsupported_content_types));
  tcase_add_loop_test (tc_media_source,
      test_media_source_supported_mp4_content_type,
      0, G_N_ELEMENTS (valid_mp4_content_types));
  tcase_add_loop_test (tc_media_source,
      test_media_source_supported_webm_content_type,
      0, G_N_ELEMENTS (valid_webm_content_types));

  tcase_add_test (tc_source_buffer, test_source_buffer_generate_timestamps_mp4);
  tcase_add_test (tc_source_buffer, test_source_buffer_generate_timestamps_aac);

  tcase_add_test (tc_source_buffer,
      test_source_buffer_change_content_type_null);
  tcase_add_test (tc_source_buffer,
      test_source_buffer_change_content_type_empty);
  tcase_add_test (tc_source_buffer, test_source_buffer_change_content_type);

  tcase_add_test (tc_append_pipeline, test_append_pipeline_create_and_free);
  tcase_add_test (tc_append_pipeline, test_append_pipeline_mp4);
  tcase_add_test (tc_append_pipeline, test_append_pipeline_webm);
  tcase_add_test (tc_append_pipeline,
      test_append_pipeline_invalid_data_triggers_eos);
  tcase_add_test (tc_append_pipeline,
      test_append_pipeline_invalid_data_triggers_error);
  tcase_add_test (tc_append_pipeline, test_append_pipeline_reset_recovery);

  tcase_add_test (tc_track, test_track_create_and_free);
  tcase_add_test (tc_track, test_track_create_with_invalid_type);
  tcase_add_test (tc_track, test_track_push_with_adequate_space);
  tcase_add_test (tc_track, test_track_push_with_inadequate_space);

  tcase_add_test (tc_track_buffer, test_track_buffer_empty);
  tcase_add_test (tc_track_buffer, test_track_buffer_single_span);
  tcase_add_test (tc_track_buffer, test_track_buffer_continuous_span);
  tcase_add_test (tc_track_buffer, test_track_buffer_discontinuous_span);

  tcase_add_test (tc_sample_map, test_sample_map_create_and_destroy);
  tcase_add_test (tc_sample_map, test_sample_map_add_valid_sample);
  tcase_add_test (tc_sample_map, test_sample_map_add_invalid_sample);
  tcase_add_test (tc_sample_map, test_sample_map_remove_sample);
  tcase_add_test (tc_sample_map, test_sample_map_remove_range_from_start);
  tcase_add_test (tc_sample_map,
      test_sample_map_remove_range_from_start_byte_count);

  suite_add_tcase (s, tc_media_source);
  suite_add_tcase (s, tc_source_buffer);
  suite_add_tcase (s, tc_source_buffer_list);
  suite_add_tcase (s, tc_append_pipeline);
  suite_add_tcase (s, tc_track);
  suite_add_tcase (s, tc_track_buffer);
  suite_add_tcase (s, tc_sample_map);

  return s;
}

GST_CHECK_MAIN (mse)
