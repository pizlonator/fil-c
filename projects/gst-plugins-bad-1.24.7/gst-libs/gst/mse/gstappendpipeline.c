/* GStreamer
 *
 * SPDX-License-Identifier: LGPL-2.1
 *
 * Copyright (C) 2016, 2017 Metrological Group B.V.
 * Copyright (C) 2016, 2017 Igalia S.L
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

#include "gstappendpipeline-private.h"

#include "mse.h"

#include "gstmselogging-private.h"
#include "gstmsemediatype-private.h"
#include "gstmediasourcetrack-private.h"

#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <gst/app/gstappsink.h>

typedef enum
{
  PARSE_ERROR,
  IGNORED,
  ADDED,
} AddTrackResult;

typedef struct
{
  GstAppendPipeline *pipeline;
  GstTask *task;
  GRecMutex mutex;
  GstBus *bus;
} BackgroundTask;

typedef struct
{
  GstAppendPipelineCallbacks callbacks;
  gpointer user_data;
} Callbacks;

typedef struct
{
  GstAppendPipeline *parent;
  GstPad *src_pad;
  GstAppSink *sink;
  GstMediaSourceTrack *mse_track;
  GstStream *stream;
  GstClockTime previous_pts;
} Track;

typedef struct
{
  GstClockTime duration;
  GPtrArray *video_tracks;
  GPtrArray *audio_tracks;
  GPtrArray *text_tracks;
} InitSegment;

struct _GstAppendPipeline
{
  GstObject parent_instance;

  GstPipeline *pipeline;
  GstAppSrc *src;
  GstElement *parsebin;
  GstBus *bus;

  GstStreamCollection *streams;
  GArray *tracks;

  gboolean received_init_segment;
  gboolean have_outstanding_samples;
  InitSegment init_segment;

  gboolean encountered_error;

  BackgroundTask *task;

  Callbacks callbacks;
};

G_DEFINE_TYPE (GstAppendPipeline, gst_append_pipeline, GST_TYPE_OBJECT);

#define END_OF_APPEND "end-of-append"
#define ABORT "abort"
#define SHUTDOWN "shutdown"

static void process_init_segment (GstAppendPipeline *);

static gboolean
send_abort (GstAppendPipeline * self)
{
  return gst_bus_post (self->bus, gst_message_new_application (NULL,
          gst_structure_new_empty (ABORT)));
}

static gboolean
send_shutdown (GstAppendPipeline * self)
{
  return gst_bus_post (self->bus, gst_message_new_application (NULL,
          gst_structure_new_empty (SHUTDOWN)));
}

static GstEvent *
new_end_of_append_event (void)
{
  GstStructure *structure = gst_structure_new_empty (END_OF_APPEND);
  return gst_event_new_custom (GST_EVENT_CUSTOM_DOWNSTREAM, structure);
}

static gboolean
is_end_of_append_event (GstEvent * event)
{
  return GST_EVENT_TYPE (event) == GST_EVENT_CUSTOM_DOWNSTREAM
      && gst_event_has_name (event, END_OF_APPEND);
}

static inline guint
n_tracks (GstAppendPipeline * self)
{
  return self->tracks->len;
}

static inline Track *
index_track (GstAppendPipeline * self, guint i)
{
  return &g_array_index (self->tracks, Track, i);
}

static inline void
call_parse_error (GstAppendPipeline * self)
{
  GstAppendPipelineCallbacks *callbacks = &self->callbacks.callbacks;
  gpointer user_data = self->callbacks.user_data;

  if (callbacks->error) {
    callbacks->error (self, user_data);
    GST_TRACE_OBJECT (self, "done");
  } else {
    GST_TRACE_OBJECT (self, "dropping");
  }
}

static inline void
call_received_init_segment (GstAppendPipeline * self)
{
  GstAppendPipelineCallbacks *callbacks = &self->callbacks.callbacks;
  gpointer user_data = self->callbacks.user_data;

  if (callbacks->received_init_segment) {
    callbacks->received_init_segment (self, user_data);
    GST_TRACE_OBJECT (self, "done");
  } else {
    GST_TRACE_OBJECT (self, "dropping");
  }
}

static inline void
call_new_sample (GstAppendPipeline * self, GstMediaSourceTrack * track,
    GstSample * sample)
{
  GstAppendPipelineCallbacks *callbacks = &self->callbacks.callbacks;
  gpointer user_data = self->callbacks.user_data;

  if (callbacks->new_sample) {
    callbacks->new_sample (self, track, sample, user_data);
    GST_TRACE_OBJECT (self, "done");
  } else {
    GST_TRACE_OBJECT (self, "dropping");
  }
}

static inline void
call_duration_changed (GstAppendPipeline * self)
{
  GstAppendPipelineCallbacks *callbacks = &self->callbacks.callbacks;
  gpointer user_data = self->callbacks.user_data;

  if (callbacks->duration_changed) {
    callbacks->duration_changed (self, user_data);
    GST_TRACE_OBJECT (self, "done");
  } else {
    GST_TRACE_OBJECT (self, "dropping");
  }
}

static inline void
call_eos (GstAppendPipeline * self, GstMediaSourceTrack * track)
{
  GstAppendPipelineCallbacks *callbacks = &self->callbacks.callbacks;
  gpointer user_data = self->callbacks.user_data;

  if (callbacks->eos) {
    callbacks->eos (self, track, user_data);
    GST_TRACE_OBJECT (self, "done");
  } else {
    GST_TRACE_OBJECT (self, "dropping");
  }
}

static inline GstSample *
patch_missing_duration (GstAppendPipeline * self, GstSample * sample)
{
  GstBuffer *buffer = gst_sample_get_buffer (sample);
  if (!GST_BUFFER_DURATION_IS_VALID (buffer)) {
    GST_BUFFER_DURATION (buffer) = GST_SECOND / 60;
    GST_TRACE_OBJECT (self, "sample is missing duration, patched to %"
        GST_TIMEP_FORMAT, &buffer->duration);
  }
  return sample;
}

static inline GstSample *
patch_missing_pts (GstAppendPipeline * self, GstSample * sample, GstClockTime
    fallback)
{
  GstBuffer *buffer = gst_sample_get_buffer (sample);
  if (!GST_BUFFER_PTS_IS_VALID (buffer) && GST_CLOCK_TIME_IS_VALID (fallback)) {
    GST_TRACE_OBJECT (self, "sample is missing pts, patching with %"
        GST_TIMEP_FORMAT, &fallback);
    GST_BUFFER_PTS (buffer) = fallback;
  }
  return sample;
}

static inline GstSample *
patch_missing_dts (GstAppendPipeline * self, GstSample * sample)
{
  GstBuffer *buffer = gst_sample_get_buffer (sample);
  if (!GST_BUFFER_DTS_IS_VALID (buffer) && GST_BUFFER_PTS_IS_VALID (buffer)) {
    GST_TRACE_OBJECT (self, "sample is missing dts, patching with pts %"
        GST_TIMEP_FORMAT, &buffer->pts);
    GST_BUFFER_DTS (buffer) = GST_BUFFER_PTS (buffer);
  }
  return sample;
}

static gboolean
consume_sample_from_track (GstAppendPipeline * self, Track * track)
{
  GstSample *sample = gst_app_sink_try_pull_sample (track->sink, 0);
  if (sample == NULL) {
    return FALSE;
  }
  GstBuffer *buffer = gst_sample_get_buffer (sample);
  if (!GST_IS_BUFFER (buffer)) {
    GST_WARNING_OBJECT (self, "got null buffer in sample");
    goto done;
  }
  sample = patch_missing_pts (self, sample, track->previous_pts);
  sample = patch_missing_duration (self, sample);
  sample = patch_missing_dts (self, sample);
  track->previous_pts = GST_BUFFER_PTS (buffer);
  call_new_sample (self, track->mse_track, sample);

done:
  gst_clear_sample (&sample);
  return TRUE;
}

static void
consume_all_samples (GstAppendPipeline * self)
{
  if (!self->received_init_segment) {
    GST_DEBUG_OBJECT (self, "not all tracks are available, delaying");
    self->have_outstanding_samples = TRUE;
    return;
  }
  guint track_count = n_tracks (self);
  while (TRUE) {
    gboolean sample_consumed = FALSE;
    for (guint i = 0; i < track_count; i++) {
      Track *track = index_track (self, i);
      sample_consumed |= consume_sample_from_track (self, track);
    }
    if (!sample_consumed) {
      break;
    }
  }
  call_duration_changed (self);
  self->have_outstanding_samples = FALSE;
}

static void
handle_shutdown (BackgroundTask * task)
{
  gst_task_stop (task->task);
  GstAppendPipeline *self = task->pipeline;
  guint track_count = n_tracks (self);
  for (guint i = 0; i < track_count; i++) {
    Track *track = index_track (self, i);
    call_eos (self, track->mse_track);
  }
  call_eos (self, NULL);
}

static void
handle_abort (BackgroundTask * task)
{
  gst_task_stop (task->task);
}

static void
task_function (gpointer user_data)
{
  BackgroundTask *task = (BackgroundTask *) user_data;
  GstAppendPipeline *self = task->pipeline;
  GstMessage *message = gst_bus_timed_pop (task->bus, GST_CLOCK_TIME_NONE);
  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_APPLICATION:{
      if (gst_message_has_name (message, END_OF_APPEND)) {
        GST_TRACE_OBJECT (self, "end of append");
        consume_all_samples (self);
        goto done;
      }
      if (gst_message_has_name (message, SHUTDOWN)) {
        GST_DEBUG_OBJECT (self, "shutdown");
        handle_shutdown (task);
        goto done;
      }
      if (gst_message_has_name (message, ABORT)) {
        GST_DEBUG_OBJECT (self, "abort");
        handle_abort (task);
        goto done;
      }
      g_error ("received unsupported application message");
    }
    case GST_MESSAGE_STREAM_COLLECTION:{
      GST_DEBUG_OBJECT (self, "stream collection");
      GstStreamCollection *streams;
      gst_message_parse_stream_collection (message, &streams);
      gst_clear_object (&self->streams);
      self->streams = streams;
      process_init_segment (self);
      goto done;
    }
    case GST_MESSAGE_EOS:
      GST_DEBUG_OBJECT (self, "end of stream");
      if (self->have_outstanding_samples) {
        GST_DEBUG_OBJECT (self, "consuming remaining samples before EOS");
        consume_all_samples (self);
      }
      handle_shutdown (task);
      goto done;
    case GST_MESSAGE_ERROR:
      GST_DEBUG_OBJECT (self, "error: %" GST_PTR_FORMAT, message);
      self->encountered_error = TRUE;
      call_parse_error (self);
      handle_shutdown (task);
      goto done;
    default:
      GST_TRACE_OBJECT (self, "ignoring message %" GST_PTR_FORMAT, message);
      goto done;
  }
done:
  gst_message_unref (message);
}

static inline GstAppSink *
new_appsink (GstAppendPipeline * self, GstStreamType type)
{
  const gchar *type_name = gst_stream_type_get_name (type);
  gchar *name = g_strdup_printf ("%s-%u", type_name, n_tracks (self));
  GstAppSink *appsink =
      GST_APP_SINK (gst_element_factory_make ("appsink", name));
  gst_base_sink_set_sync (GST_BASE_SINK (appsink), FALSE);
  gst_base_sink_set_async_enabled (GST_BASE_SINK (appsink), FALSE);
  gst_base_sink_set_drop_out_of_segment (GST_BASE_SINK (appsink), FALSE);
  gst_base_sink_set_last_sample_enabled (GST_BASE_SINK (appsink), FALSE);
  g_free (name);
  return appsink;
}

static GstPadProbeReturn
black_hole_probe (GstPad * pad, GstPadProbeInfo * info, gpointer user_data)
{
  g_return_val_if_fail (GST_PAD_PROBE_INFO_TYPE (info) &
      GST_PAD_PROBE_TYPE_BUFFER, GST_PAD_PROBE_DROP);
  return GST_PAD_PROBE_DROP;
}

static GstPadProbeReturn
event_probe (GstPad * pad, GstPadProbeInfo * info, gpointer user_data)
{
  GstAppendPipeline *self = GST_APPEND_PIPELINE (user_data);
  GstEvent *event = GST_PAD_PROBE_INFO_EVENT (info);

  if (is_end_of_append_event (event)) {
    GST_TRACE_OBJECT (self, "end of append event");
    if (gst_bus_post (self->bus, gst_message_new_application (NULL,
                gst_structure_new_empty (END_OF_APPEND)))) {
      return GST_PAD_PROBE_DROP;
    } else {
      GST_ERROR_OBJECT (self, "failed to post end of append");
      goto error;
    }
  }

  if (GST_EVENT_TYPE (event) == GST_EVENT_EOS) {
    GST_DEBUG_OBJECT (self, "eos event");
    if (send_shutdown (self)) {
      return GST_PAD_PROBE_OK;
    } else {
      GST_ERROR_OBJECT (self, "failed to post shutdown");
      goto error;
    }
  }

  return GST_PAD_PROBE_OK;

error:
  GST_PAD_PROBE_INFO_FLOW_RETURN (info) = GST_FLOW_ERROR;
  gst_event_unref (event);
  return GST_PAD_PROBE_HANDLED;
}

static AddTrackResult
add_track (GstAppendPipeline * self, GstPad * pad, GstStream * stream,
    GstCaps * caps, Track * added_track)
{
  GstStreamType type = gst_stream_get_stream_type (stream);
  GstMediaSourceTrackType track_type =
      gst_media_source_track_type_from_stream_type (type);

  switch (type) {
    case GST_STREAM_TYPE_AUDIO:
    case GST_STREAM_TYPE_TEXT:
    case GST_STREAM_TYPE_VIDEO:
      break;
    default:{
      GST_DEBUG_OBJECT (self, "unexpected caps %" GST_PTR_FORMAT
          ", using black hole probe", caps);
      gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_BUFFER, black_hole_probe, self,
          NULL);
      return IGNORED;
    }
  }

  if (type != GST_STREAM_TYPE_TEXT &&
      !gst_media_source_media_type_is_caps_supported (caps)) {
    GST_ERROR_OBJECT (self, "unsupported caps: %" GST_PTR_FORMAT, caps);
    return PARSE_ERROR;
  }

  GstAppSink *appsink = new_appsink (self, type);
  gst_bin_add (GST_BIN (self->pipeline), GST_ELEMENT (appsink));
  gst_element_sync_state_with_parent (GST_ELEMENT (appsink));

  GstPad *appsink_pad =
      gst_element_get_static_pad (GST_ELEMENT (appsink), "sink");
  GstPadLinkReturn link_result = gst_pad_link (pad, appsink_pad);
  gst_clear_object (&appsink_pad);
  if (GST_PAD_LINK_FAILED (link_result)) {
    g_error ("failed to link parser to appsink: %s",
        gst_pad_link_get_name (link_result));
  }

  Track track_template = {
    .parent = self,
    .sink = gst_object_ref (appsink),
    .src_pad = gst_object_ref (pad),
    .stream = gst_object_ref (stream),
    .mse_track = gst_media_source_track_new_with_initial_caps (track_type,
        GST_OBJECT_NAME (appsink), caps),
    .previous_pts = GST_CLOCK_TIME_NONE,
  };
  g_array_append_val (self->tracks, track_template);

  GST_TRACE_OBJECT (self, "added appsink %s to pad %s",
      GST_OBJECT_NAME (appsink), GST_OBJECT_NAME (pad));

  *added_track = track_template;

  return ADDED;
}

static void
clear_track (Track * track)
{
  gst_clear_object (&track->sink);
  gst_clear_object (&track->src_pad);
  gst_clear_object (&track->mse_track);
  gst_clear_object (&track->stream);
}

static inline GPtrArray *
init_segment_tracks_for (InitSegment * self, GstStreamType type)
{
  switch (type) {
    case GST_STREAM_TYPE_AUDIO:
      return self->audio_tracks;
    case GST_STREAM_TYPE_TEXT:
      return self->text_tracks;
    case GST_STREAM_TYPE_VIDEO:
      return self->video_tracks;
    default:
      g_assert_not_reached ();
      return NULL;
  }
}

static void
process_init_segment_track (GstPad * pad, GstAppendPipeline * self)
{
  GST_OBJECT_LOCK (self);
  InitSegment *init_segment = &self->init_segment;
  GstStream *stream = gst_pad_get_stream (pad);
  GstCaps *caps = gst_stream_get_caps (stream);
  GST_DEBUG_OBJECT (self, "%" GST_PTR_FORMAT " got caps %" GST_PTR_FORMAT, pad,
      caps);

  if (gst_pad_is_linked (pad)) {
    GST_TRACE_OBJECT (self, "%" GST_PTR_FORMAT " is already linked, skipping",
        pad);
    goto done;
  }

  if (!GST_IS_CAPS (caps)) {
    GST_ERROR_OBJECT (self, "no caps on %" GST_PTR_FORMAT
        " after stream collection", pad);
    call_parse_error (self);
    goto done;
  }

  Track track;
  AddTrackResult result = add_track (self, pad, stream, caps, &track);
  GstMediaSourceTrack *mse_track = track.mse_track;
  switch (result) {
    case ADDED:{
      GstStreamType type = gst_stream_get_stream_type (stream);
      GPtrArray *tracks = init_segment_tracks_for (init_segment, type);
      if (tracks->len < 1) {
        gst_media_source_track_set_active (mse_track, TRUE);
      }
      g_ptr_array_add (tracks, gst_object_ref (mse_track));
      break;
    }
    case IGNORED:
      break;
    case PARSE_ERROR:
      call_parse_error (self);
      break;
  }

done:
  gst_clear_object (&stream);
  gst_clear_caps (&caps);
  GST_OBJECT_UNLOCK (self);
}

static void
on_pad_added (GstElement * parsebin, GstPad * pad, gpointer user_data)
{
  GstAppendPipeline *self = GST_APPEND_PIPELINE (user_data);
  process_init_segment_track (pad, self);
  process_init_segment (self);
}

static gboolean
has_track_for_stream (GstAppendPipeline * self, GstStream * stream)
{
  guint track_count = n_tracks (self);
  for (guint i = 0; i < track_count; i++) {
    Track *track = index_track (self, i);
    if (track->stream == stream) {
      return TRUE;
    }
  }
  return FALSE;
}

static gboolean
has_all_tracks (GstAppendPipeline * self)
{
  if (!GST_IS_STREAM_COLLECTION (self->streams)) {
    return FALSE;
  }
  for (guint i = 0; i < gst_stream_collection_get_size (self->streams); i++) {
    GstStream *stream = gst_stream_collection_get_stream (self->streams, i);
    switch (gst_stream_get_stream_type (stream)) {
      case GST_STREAM_TYPE_AUDIO:
      case GST_STREAM_TYPE_VIDEO:
      case GST_STREAM_TYPE_TEXT:
        break;
      default:
        continue;
    }
    if (!has_track_for_stream (self, stream)) {
      return FALSE;
    }
  }
  return TRUE;
}

static void
process_init_segment (GstAppendPipeline * self)
{
  gint64 duration;
  InitSegment *init_segment = &self->init_segment;

  GST_OBJECT_LOCK (self);

  if (!has_all_tracks (self)) {
    goto done;
  }

  if (gst_element_query_duration (self->parsebin, GST_FORMAT_TIME, &duration)) {
    init_segment->duration = MAX (0, duration);
  } else {
    init_segment->duration = GST_CLOCK_TIME_NONE;
  }

  GST_DEBUG_OBJECT (self, "init segment says duration=%" GST_TIME_FORMAT,
      GST_TIME_ARGS ((GstClockTime) duration));

  self->received_init_segment = TRUE;

  call_received_init_segment (self);

done:
  GST_OBJECT_UNLOCK (self);
}

static inline void
init_segment_init (InitSegment * self)
{
  self->audio_tracks = g_ptr_array_new_with_free_func (gst_object_unref);
  self->text_tracks = g_ptr_array_new_with_free_func (gst_object_unref);
  self->video_tracks = g_ptr_array_new_with_free_func (gst_object_unref);
  self->duration = GST_CLOCK_TIME_NONE;
}

static inline void
init_segment_finalize (InitSegment * self)
{
  g_ptr_array_free (self->audio_tracks, TRUE);
  g_ptr_array_free (self->text_tracks, TRUE);
  g_ptr_array_free (self->video_tracks, TRUE);
}

static GArray *
new_tracks_array (void)
{
  GArray *tracks = g_array_new (TRUE, TRUE, sizeof (Track));
  g_array_set_clear_func (tracks, (GDestroyNotify) clear_track);
  return tracks;
}

static BackgroundTask *
background_task_new (GstAppendPipeline * pipeline)
{
  BackgroundTask *task = g_new0 (BackgroundTask, 1);
  g_rec_mutex_init (&task->mutex);
  task->task = gst_task_new (task_function, task, NULL);
  task->pipeline = pipeline;
  task->bus = gst_object_ref (pipeline->bus);
  gst_task_set_lock (task->task, &task->mutex);
  return task;
}

static gboolean
background_task_start (BackgroundTask * task)
{
  gst_bus_set_flushing (task->bus, FALSE);
  return gst_task_start (task->task);
}

static gboolean
background_task_stop (BackgroundTask * task)
{
  send_abort (task->pipeline);
  gst_task_join (task->task);
  gst_bus_set_flushing (task->bus, TRUE);
  return TRUE;
}

static void
background_task_cleanup (gpointer ptr)
{
  BackgroundTask *task = (BackgroundTask *) ptr;
  background_task_stop (task);
  task->pipeline = NULL;
  gst_clear_object (&task->task);
  gst_clear_object (&task->bus);
  g_rec_mutex_clear (&task->mutex);
  g_free (task);
}

static void
gst_append_pipeline_init (GstAppendPipeline * self)
{
  GstElement *appsrc = GST_ELEMENT (gst_element_factory_make ("appsrc", "src"));
  GstElement *parsebin =
      GST_ELEMENT (gst_element_factory_make ("parsebin", "parse"));
  GstElement *pipeline = gst_pipeline_new ("append-pipeline");

  GstPad *appsrc_pad = GST_PAD (gst_element_get_static_pad (appsrc, "src"));
  gst_pad_add_probe (appsrc_pad, GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM,
      event_probe, self, NULL);
  gst_object_unref (appsrc_pad);

  gst_bin_add_many (GST_BIN (pipeline), appsrc, parsebin, NULL);
  if (!gst_element_link (appsrc, parsebin)) {
    g_error ("failed to link appsrc to parsebin");
  }

  self->bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  self->pipeline = gst_object_ref_sink (GST_PIPELINE (pipeline));
  self->src = GST_APP_SRC (appsrc);
  self->parsebin = parsebin;

  self->task = background_task_new (self);

  g_signal_connect_object (parsebin, "pad-added", G_CALLBACK (on_pad_added),
      self, 0);

  self->received_init_segment = FALSE;
  self->encountered_error = FALSE;
  self->tracks = new_tracks_array ();
  init_segment_init (&self->init_segment);
}

static void
gst_append_pipeline_dispose (GObject * object)
{
  GstAppendPipeline *self = (GstAppendPipeline *) object;

  send_shutdown (self);
  g_clear_pointer (&self->task, background_task_cleanup);

  gst_element_set_state (GST_ELEMENT (self->pipeline), GST_STATE_NULL);

  G_OBJECT_CLASS (gst_append_pipeline_parent_class)->dispose (object);
}

static void
gst_append_pipeline_finalize (GObject * object)
{
  GstAppendPipeline *self = (GstAppendPipeline *) object;

  gst_clear_object (&self->pipeline);
  gst_clear_object (&self->bus);

  init_segment_finalize (&self->init_segment);

  g_array_free (self->tracks, TRUE);
  gst_clear_object (&self->streams);

  G_OBJECT_CLASS (gst_append_pipeline_parent_class)->finalize (object);
}

static void
gst_append_pipeline_class_init (GstAppendPipelineClass * klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);

  oclass->dispose = GST_DEBUG_FUNCPTR (gst_append_pipeline_dispose);
  oclass->finalize = GST_DEBUG_FUNCPTR (gst_append_pipeline_finalize);
}

GstAppendPipeline *
gst_append_pipeline_new (GstAppendPipelineCallbacks * callbacks,
    gpointer user_data, GError ** error)
{
  gst_mse_init_logging ();
  GstAppendPipeline *self = g_object_new (GST_TYPE_APPEND_PIPELINE, NULL);
  GstStateChangeReturn started =
      gst_element_set_state (GST_ELEMENT (self->pipeline), GST_STATE_PLAYING);
  if (started != GST_STATE_CHANGE_SUCCESS) {
    GST_ERROR_OBJECT (self, "failed to start: %s",
        gst_element_state_change_return_get_name (started));
    g_set_error (error,
        GST_MEDIA_SOURCE_ERROR, GST_MEDIA_SOURCE_ERROR_INVALID_STATE,
        "failed to start append pipeline");
    goto error;
  }
  if (callbacks) {
    self->callbacks.callbacks = *callbacks;
    self->callbacks.user_data = user_data;
  }
  if (!background_task_start (self->task)) {
    GST_ERROR_OBJECT (self, "failed to start background task");
    g_set_error (error,
        GST_MEDIA_SOURCE_ERROR, GST_MEDIA_SOURCE_ERROR_INVALID_STATE,
        "failed to start append pipeline's background task");
    goto error;
  }
  return gst_object_ref_sink (self);
error:
  gst_clear_object (&self);
  return NULL;
}

GstFlowReturn
gst_append_pipeline_append (GstAppendPipeline * self, GstBuffer * buffer)
{
  g_return_val_if_fail (GST_IS_APPEND_PIPELINE (self), GST_FLOW_ERROR);
  GstFlowReturn push_result = gst_app_src_push_buffer (self->src, buffer);
  if (push_result != GST_FLOW_OK)
    return push_result;

  if (!gst_element_send_event (GST_ELEMENT_CAST (self->src),
          new_end_of_append_event ())) {
    GST_ERROR_OBJECT (self, "failed to push end-of-append event");
    return GST_FLOW_ERROR;
  }
  return GST_FLOW_OK;
}

GstFlowReturn
gst_append_pipeline_eos (GstAppendPipeline * self)
{
  g_return_val_if_fail (GST_IS_APPEND_PIPELINE (self), GST_FLOW_ERROR);
  return gst_app_src_end_of_stream (self->src);
}

gboolean
gst_append_pipeline_stop (GstAppendPipeline * self)
{
  g_return_val_if_fail (GST_IS_APPEND_PIPELINE (self), FALSE);

  GstElement *pipeline = GST_ELEMENT (self->pipeline);

  GstStateChangeReturn stopped =
      gst_element_set_state (pipeline, GST_STATE_NULL);
  if (stopped != GST_STATE_CHANGE_SUCCESS) {
    GST_ERROR_OBJECT (self, "failed to stop: %s",
        gst_element_state_change_return_get_name (stopped));
    return FALSE;
  }
  self->received_init_segment = FALSE;
  self->encountered_error = FALSE;

  return TRUE;
}

gboolean
gst_append_pipeline_reset (GstAppendPipeline * self)
{
  g_return_val_if_fail (GST_IS_APPEND_PIPELINE (self), FALSE);

  GstElement *pipeline = GST_ELEMENT (self->pipeline);

  GstStateChangeReturn stopped =
      gst_element_set_state (pipeline, GST_STATE_READY);
  if (stopped != GST_STATE_CHANGE_SUCCESS) {
    GST_ERROR_OBJECT (self, "failed to stop: %s",
        gst_element_state_change_return_get_name (stopped));
    return FALSE;
  }

  background_task_stop (self->task);

  init_segment_finalize (&self->init_segment);
  gst_clear_object (&self->streams);
  g_array_free (self->tracks, TRUE);

  self->received_init_segment = FALSE;
  self->have_outstanding_samples = FALSE;
  self->encountered_error = FALSE;
  self->tracks = new_tracks_array ();
  init_segment_init (&self->init_segment);

  if (!background_task_start (self->task)) {
    GST_ERROR_OBJECT (self, "failed to start background task");
    return FALSE;
  }

  GstStateChangeReturn started =
      gst_element_set_state (pipeline, GST_STATE_PLAYING);
  if (started == GST_STATE_CHANGE_SUCCESS) {
    return TRUE;
  } else {
    GST_ERROR_OBJECT (self, "failed to start: %s",
        gst_element_state_change_return_get_name (started));
    return FALSE;
  }
}

gsize
gst_append_pipeline_n_tracks (GstAppendPipeline * self)
{
  g_return_val_if_fail (GST_IS_APPEND_PIPELINE (self), 0);
  return n_tracks (self);
}

gboolean
gst_append_pipeline_has_init_segment (GstAppendPipeline * self)
{
  g_return_val_if_fail (GST_IS_APPEND_PIPELINE (self), FALSE);
  return self->received_init_segment;
}

GstClockTime
gst_append_pipeline_get_duration (GstAppendPipeline * self)
{
  g_return_val_if_fail (GST_IS_APPEND_PIPELINE (self), GST_CLOCK_TIME_NONE);
  if (self->received_init_segment) {
    return self->init_segment.duration;
  } else {
    return GST_CLOCK_TIME_NONE;
  }
}

GPtrArray *
gst_append_pipeline_get_audio_tracks (GstAppendPipeline * self)
{
  g_return_val_if_fail (GST_IS_APPEND_PIPELINE (self), NULL);
  if (self->received_init_segment) {
    return self->init_segment.audio_tracks;
  } else {
    return NULL;
  }
}

GPtrArray *
gst_append_pipeline_get_text_tracks (GstAppendPipeline * self)
{
  g_return_val_if_fail (GST_IS_APPEND_PIPELINE (self), NULL);
  if (self->received_init_segment) {
    return self->init_segment.text_tracks;
  } else {
    return NULL;
  }
}

GPtrArray *
gst_append_pipeline_get_video_tracks (GstAppendPipeline * self)
{
  g_return_val_if_fail (GST_IS_APPEND_PIPELINE (self), NULL);
  if (self->received_init_segment) {
    return self->init_segment.video_tracks;
  } else {
    return NULL;
  }
}

gboolean
gst_append_pipeline_get_eos (GstAppendPipeline * self)
{
  g_return_val_if_fail (GST_IS_APPEND_PIPELINE (self), FALSE);
  return gst_task_get_state (self->task->task) != GST_TASK_STARTED;
}

void
gst_append_pipeline_fail (GstAppendPipeline * self)
{
  g_return_if_fail (GST_IS_APPEND_PIPELINE (self));
  gst_bus_post (self->bus, gst_message_new_error (NULL, NULL, NULL));
}

gboolean
gst_append_pipeline_get_failed (GstAppendPipeline * self)
{
  g_return_val_if_fail (GST_IS_APPEND_PIPELINE (self), FALSE);
  return self->encountered_error;
}
