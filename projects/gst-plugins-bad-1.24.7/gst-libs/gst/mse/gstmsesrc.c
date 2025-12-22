/* GStreamer
 *
 * SPDX-License-Identifier: LGPL-2.1
 *
 * Copyright (C) 2009, 2010 Sebastian Dröge <sebastian.droege@collabora.co.uk>
 * Copyright (C) 2013, 2022, 2023 Collabora Ltd.
 * Copyright (C) 2013 Orange
 * Copyright (C) 2014, 2015 Sebastian Dröge <sebastian@centricular.com>
 * Copyright (C) 2015, 2016, 2018, 2019, 2020, 2021 Igalia, S.L
 * Copyright (C) 2015, 2016, 2018, 2019, 2020, 2021 Metrological Group B.V.
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

/**
 * SECTION:gstmsesrc
 * @title: GstMseSrc
 * @short_description: Source Element for Media Source playback
 *
 * #GstMseSrc is a source Element that interacts with a #GstMediaSource to
 * consume #GstSample<!-- -->s processed by the Media Source and supplies them
 * to the containing #GstPipeline. In the perspective of the Media Source API,
 * this element fulfills the basis of the Media Element's role relating to
 * working with a Media Source. The remaining responsibilities are meant to be
 * fulfilled by the application and #GstPlay can be used to satisfy many of
 * them.
 *
 * Once added to a Pipeline, this element should be attached to a Media Source
 * using gst_media_source_attach().
 *
 * Since: 1.24
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/base/base.h>

#include <gst/mse/mse-enumtypes.h>
#include "gstmsesrc.h"
#include "gstmsesrc-private.h"

#include "gstmselogging-private.h"

#include "gstmediasource.h"
#include "gstmediasource-private.h"
#include "gstmediasourcetrack-private.h"
#include "gstsourcebuffer.h"
#include "gstsourcebuffer-private.h"

#define DEFAULT_POSITION GST_CLOCK_TIME_NONE
#define DEFAULT_DURATION GST_CLOCK_TIME_NONE
#define DEFAULT_READY_STATE GST_MSE_SRC_READY_STATE_HAVE_NOTHING
#define DECODE_ERROR "decode error"
#define NETWORK_ERROR "network error"

enum
{
  PROP_0,

  PROP_POSITION,
  PROP_DURATION,
  PROP_READY_STATE,

  PROP_N_AUDIO,
  PROP_N_TEXT,
  PROP_N_VIDEO,

  N_PROPS,
};

enum
{
  THRESHOLD_FUTURE_DATA = GST_SECOND * 5,
  THRESHOLD_ENOUGH_DATA = GST_SECOND * 50,
};

static GParamSpec *properties[N_PROPS];

static GstStaticPadTemplate gst_mse_src_template =
GST_STATIC_PAD_TEMPLATE ("src_%s", GST_PAD_SRC, GST_PAD_SOMETIMES,
    GST_STATIC_CAPS_ANY);

/**
 * GstMseSrcPad:
 *
 * Since: 1.24
 */
struct _GstMseSrcPad
{
  GstPad base;

  GstStream *stream;
  GstMediaSourceTrack *track;
  GstCaps *most_recent_caps;
  GstSegment segment;

  GstClockTime position;

  gboolean sent_stream_collection;
  gboolean sent_stream_start;
  gboolean sent_initial_caps;
  gboolean does_need_segment;

  GCond linked_or_flushing_cond;
  GMutex linked_or_flushing_lock;
  gboolean flushing;
  gboolean eos;
};

#define STREAMS_LOCK(a) (g_mutex_lock (&a->streams_lock))
#define STREAMS_UNLOCK(a) (g_mutex_unlock (&a->streams_lock))

#define LINKED_OR_FLUSHING_LOCK(a) (g_mutex_lock (&a->linked_or_flushing_lock))
#define LINKED_OR_FLUSHING_UNLOCK(a) (g_mutex_unlock (&a->linked_or_flushing_lock))
#define LINKED_OR_FLUSHING_SIGNAL(a) (g_cond_signal (&a->linked_or_flushing_cond))
#define LINKED_OR_FLUSHING_WAIT(a) \
  (g_cond_wait (&a->linked_or_flushing_cond, &a->linked_or_flushing_lock))

#define FLOW_COMBINER_LOCK(a) (g_mutex_lock (&a->flow_combiner_lock))
#define FLOW_COMBINER_UNLOCK(a) (g_mutex_unlock (&a->flow_combiner_lock))

G_DEFINE_TYPE (GstMseSrcPad, gst_mse_src_pad, GST_TYPE_PAD);

static gboolean pad_activate_mode (GstMseSrcPad * pad, GstObject * parent,
    GstPadMode mode, gboolean active);

static GstPadLinkReturn pad_linked (GstMseSrcPad * pad, GstMseSrc * parent,
    GstPad * sink);
static gboolean pad_event (GstMseSrcPad * pad, GstMseSrc * parent,
    GstEvent * event);
static gboolean pad_query (GstMseSrcPad * pad, GstObject * parent,
    GstQuery * query);
static void pad_task (GstMseSrcPad * pad);

static GstPad *
gst_mse_src_pad_new (GstMediaSourceTrack * track, GstStream * stream,
    guint id, GstClockTime start, gdouble rate)
{
  gchar *name = g_strdup_printf ("src_%u", id);
  GstMseSrcPad *self = g_object_new (GST_TYPE_MSE_SRC_PAD, "name", name,
      "direction", GST_PAD_SRC, NULL);
  g_free (name);
  self->stream = stream;
  self->track = track;
  self->segment.start = start;
  self->segment.rate = rate;

  return GST_PAD (self);
}

static void
gst_mse_src_pad_init (GstMseSrcPad * self)
{
  gst_segment_init (&self->segment, GST_FORMAT_TIME);
  self->sent_stream_collection = FALSE;
  self->sent_stream_start = FALSE;
  self->sent_initial_caps = FALSE;
  self->does_need_segment = TRUE;
  self->position = DEFAULT_POSITION;
  self->flushing = FALSE;
  self->eos = FALSE;
  g_mutex_init (&self->linked_or_flushing_lock);
  g_cond_init (&self->linked_or_flushing_cond);

  gst_pad_set_activatemode_function (GST_PAD (self),
      (GstPadActivateModeFunction) pad_activate_mode);
  gst_pad_set_link_function (GST_PAD (self), (GstPadLinkFunction) pad_linked);
  gst_pad_set_event_function (GST_PAD (self), (GstPadEventFunction) pad_event);
  gst_pad_set_query_function (GST_PAD (self), (GstPadQueryFunction) pad_query);
}

static void
gst_mse_src_pad_finalize (GObject * object)
{
  GstMseSrcPad *self = GST_MSE_SRC_PAD (object);

  gst_clear_caps (&self->most_recent_caps);
  g_mutex_clear (&self->linked_or_flushing_lock);
  g_cond_clear (&self->linked_or_flushing_cond);

  G_OBJECT_CLASS (gst_mse_src_pad_parent_class)->finalize (object);
}

static void
gst_mse_src_pad_class_init (GstMseSrcPadClass * klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);
  oclass->finalize = GST_DEBUG_FUNCPTR (gst_mse_src_pad_finalize);
}

// TODO: Check if this struct is even necessary.
// The custom pad should be able to keep track of information for each track.
typedef struct
{
  GstMediaSourceTrack *track;
  GstPad *pad;
  GstStream *info;
} Stream;

/**
 * GstMseSrc:
 *
 * Since: 1.24
 */
struct _GstMseSrc
{
  GstElement base;

  GstMediaSource *media_source;

  guint group_id;
  GstStreamCollection *collection;
  GHashTable *streams;
  GMutex streams_lock;

  GstClockTime duration;
  GstClockTime start_time;
  gdouble rate;
  GstMseSrcReadyState ready_state;

  GstFlowCombiner *flow_combiner;
  GMutex flow_combiner_lock;

  GCond eos_cond;
  GMutex eos_lock;

  gchar *uri;
};

static void gst_mse_src_uri_handler_init (gpointer g_iface,
    gpointer iface_data);
static GstStateChangeReturn gst_mse_src_change_state (GstElement * element,
    GstStateChange transition);
static gboolean gst_mse_src_send_event (GstElement * element, GstEvent * event);
static void update_ready_state_for_init_segment (GstMseSrc * self);
static void update_ready_state_for_sample (GstMseSrc * self);

G_DEFINE_TYPE_WITH_CODE (GstMseSrc, gst_mse_src, GST_TYPE_ELEMENT,
    G_IMPLEMENT_INTERFACE (GST_TYPE_URI_HANDLER, gst_mse_src_uri_handler_init));

static void
gst_mse_src_constructed (GObject * object)
{
  GstMseSrc *self = GST_MSE_SRC (object);
  GST_OBJECT_FLAG_SET (self, GST_ELEMENT_FLAG_SOURCE);
}

static void
gst_mse_src_dispose (GObject * object)
{
  GstMseSrc *self = GST_MSE_SRC (object);
  gst_clear_object (&self->collection);
  g_clear_pointer (&self->streams, g_hash_table_unref);
  g_mutex_clear (&self->streams_lock);
  g_clear_pointer (&self->flow_combiner, gst_flow_combiner_free);
  g_mutex_clear (&self->flow_combiner_lock);
  g_cond_clear (&self->eos_cond);
  g_mutex_clear (&self->eos_lock);
  G_OBJECT_CLASS (gst_mse_src_parent_class)->dispose (object);
}

static void
gst_mse_src_finalize (GObject * object)
{
  GstMseSrc *self = GST_MSE_SRC (object);

  g_clear_pointer (&self->uri, g_free);

  G_OBJECT_CLASS (gst_mse_src_parent_class)->finalize (object);
}

static void
gst_mse_src_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstMseSrc *self = GST_MSE_SRC (object);

  switch (prop_id) {
    case PROP_DURATION:
      g_value_set_uint64 (value, gst_mse_src_get_duration (self));
      break;
    case PROP_POSITION:
      g_value_set_uint64 (value, gst_mse_src_get_position (self));
      break;
    case PROP_READY_STATE:
      g_value_set_enum (value, gst_mse_src_get_ready_state (self));
      break;
    case PROP_N_AUDIO:
      g_value_set_uint (value, gst_mse_src_get_n_audio (self));
      break;
    case PROP_N_TEXT:
      g_value_set_uint (value, gst_mse_src_get_n_text (self));
      break;
    case PROP_N_VIDEO:
      g_value_set_uint (value, gst_mse_src_get_n_video (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_mse_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstMseSrc *self = GST_MSE_SRC (object);

  switch (prop_id) {
    case PROP_DURATION:
      gst_mse_src_set_duration (self, g_value_get_uint64 (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_mse_src_class_init (GstMseSrcClass * klass)
{
  GObjectClass *oclass = (GObjectClass *) klass;
  GstElementClass *eclass = (GstElementClass *) klass;

  oclass->constructed = GST_DEBUG_FUNCPTR (gst_mse_src_constructed);
  oclass->finalize = GST_DEBUG_FUNCPTR (gst_mse_src_finalize);
  oclass->dispose = GST_DEBUG_FUNCPTR (gst_mse_src_dispose);
  oclass->get_property = GST_DEBUG_FUNCPTR (gst_mse_src_get_property);
  oclass->set_property = GST_DEBUG_FUNCPTR (gst_mse_src_set_property);

  eclass->change_state = GST_DEBUG_FUNCPTR (gst_mse_src_change_state);
  eclass->send_event = GST_DEBUG_FUNCPTR (gst_mse_src_send_event);

  /**
   * GstMseSrc:position:
   *
   * The playback position as a #GstClockTime
   *
   * [Specification](https://html.spec.whatwg.org/multipage/media.html#current-playback-position)
   *
   * Since: 1.24
   */
  properties[PROP_POSITION] = g_param_spec_uint64 ("position",
      "Position",
      "The playback position as a GstClockTime",
      0, G_MAXUINT64, DEFAULT_POSITION, G_PARAM_READABLE |
      G_PARAM_STATIC_STRINGS);

  /**
   * GstMseSrc:duration:
   *
   * The duration of the stream as a #GstClockTime
   *
   * [Specification](https://html.spec.whatwg.org/multipage/media.html#dom-media-duration)
   *
   * Since: 1.24
   */
  properties[PROP_DURATION] = g_param_spec_uint64 ("duration",
      "Duration",
      "The duration of the stream as a GstClockTime",
      0, G_MAXUINT64, DEFAULT_DURATION, G_PARAM_READWRITE |
      G_PARAM_STATIC_STRINGS);

  /**
   * GstMseSrc:ready-state:
   *
   * The Ready State of this element, describing to what level it can supply
   * content for the current #GstMseSrc:position. This is a separate concept
   * from #GstMediaSource:ready-state: and corresponds to the HTML Media
   * Element's Ready State.
   *
   * [Specification](https://html.spec.whatwg.org/multipage/media.html#ready-states)
   *
   * Since: 1.24
   */
  properties[PROP_READY_STATE] = g_param_spec_enum ("ready-state",
      "Ready State",
      "The Ready State of this Element",
      GST_TYPE_MSE_SRC_READY_STATE,
      DEFAULT_READY_STATE, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * GstMseSrc:n-audio:
   *
   * The number of audio tracks in the Media Source
   *
   * Since: 1.24
   */
  properties[PROP_N_AUDIO] = g_param_spec_uint ("n-audio",
      "Number of Audio Tracks",
      "The number of audio tracks in the Media Source",
      0, G_MAXINT, 0, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * GstMseSrc:n-text:
   *
   * The number of text tracks in the Media Source
   *
   * Since: 1.24
   */
  properties[PROP_N_TEXT] = g_param_spec_uint ("n-text",
      "Number of Text Tracks",
      "The number of text tracks in the Media Source",
      0, G_MAXINT, 0, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * GstMseSrc:n-video:
   *
   * The number of video tracks in the Media Source
   *
   * Since: 1.24
   */
  properties[PROP_N_VIDEO] = g_param_spec_uint ("n-video",
      "Number of Video Tracks",
      "The number of video tracks in the Media Source",
      0, G_MAXINT, 0, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (oclass, N_PROPS, properties);

  gst_element_class_set_static_metadata (eclass, "MseSrc",
      "Generic/Source",
      "Implements a GStreamer Source for the gstreamer-mse API", "Collabora");
  gst_element_class_add_static_pad_template (eclass, &gst_mse_src_template);

  gst_mse_init_logging ();
}

static void
clear_stream (Stream * stream)
{
  gst_clear_object (&stream->track);
  gst_clear_object (&stream->info);
  g_free (stream);
}

static GHashTable *
streams_init (const GstMseSrc * self)
{
  return g_hash_table_new_full (g_direct_hash, g_direct_equal,
      NULL, (GDestroyNotify) clear_stream);
}

static GstStreamCollection *
collection_init (const GstMseSrc * self)
{
  return gst_stream_collection_new (G_OBJECT_TYPE_NAME (self));
}

static void
gst_mse_src_init (GstMseSrc * self)
{
  self->group_id = gst_util_group_id_next ();
  self->streams = streams_init (self);
  self->collection = collection_init (self);
  self->uri = NULL;
  self->start_time = 0;
  self->rate = 1;
  g_mutex_init (&self->streams_lock);
  self->flow_combiner = gst_flow_combiner_new ();
  g_mutex_init (&self->flow_combiner_lock);
  g_cond_init (&self->eos_cond);
  g_mutex_init (&self->eos_lock);
}

/**
 * gst_mse_src_get_position:
 * @self: #GstMseSrc instance
 *
 * Gets the current playback position of @self.
 *
 * [Specification](https://html.spec.whatwg.org/multipage/media.html#current-playback-position)
 *
 * Returns: The playback position of this Element as a #GstClockTime
 * Since: 1.24
 */
GstClockTime
gst_mse_src_get_position (GstMseSrc * self)
{
  g_return_val_if_fail (GST_IS_MSE_SRC (self), GST_CLOCK_TIME_NONE);
  gint64 position;
  gboolean success = gst_element_query_position (GST_ELEMENT (self),
      GST_FORMAT_TIME, &position);
  if (success)
    return (GstClockTime) position;
  else
    return DEFAULT_POSITION;
}

static void
update_pad_duration (GstMseSrc * self, GstMseSrcPad * pad)
{
  pad->segment.duration = self->duration;
  pad->does_need_segment = TRUE;
}

void
gst_mse_src_set_duration (GstMseSrc * self, GstClockTime duration)
{
  g_return_if_fail (GST_IS_MSE_SRC (self));

  self->duration = duration;

  gst_element_foreach_src_pad (GST_ELEMENT (self),
      (GstElementForeachPadFunc) update_pad_duration, NULL);

  gst_element_post_message (GST_ELEMENT (self),
      gst_message_new_duration_changed (GST_OBJECT (self)));
}

/**
 * gst_mse_src_get_duration:
 * @self: #GstMseSrc instance
 *
 * Gets the duration of @self.
 *
 * [Specification](https://html.spec.whatwg.org/multipage/media.html#dom-media-duration)
 *
 * Returns: The duration of this stream as a #GstClockTime
 * Since: 1.24
 */
GstClockTime
gst_mse_src_get_duration (GstMseSrc * self)
{
  g_return_val_if_fail (GST_IS_MSE_SRC (self), DEFAULT_DURATION);
  return self->duration;
}

/**
 * gst_mse_src_get_ready_state:
 * @self: #GstMseSrc instance
 *
 * The Ready State of @self, describing to what level it can supply content for
 * the current #GstMseSrc:position. This is a separate concept from
 * #GstMediaSource:ready-state: and corresponds to the HTML Media Element's
 * Ready State.
 *
 * [Specification](https://html.spec.whatwg.org/multipage/media.html#ready-states)
 *
 * Returns: the current #GstMseSrcReadyState
 * Since: 1.24
 */
GstMseSrcReadyState
gst_mse_src_get_ready_state (GstMseSrc * self)
{
  g_return_val_if_fail (GST_IS_MSE_SRC (self), DEFAULT_READY_STATE);
  return self->ready_state;
}

static guint
n_streams_by_type (GstMseSrc * self, GstMediaSourceTrackType type)
{
  guint count = 0;
  GHashTableIter iter;
  g_hash_table_iter_init (&iter, self->streams);
  for (gpointer key; g_hash_table_iter_next (&iter, &key, NULL);) {
    GstMediaSourceTrack *track = GST_MEDIA_SOURCE_TRACK (key);
    GstMediaSourceTrackType stream_type =
        gst_media_source_track_get_track_type (track);
    if (type == stream_type) {
      count++;
    }
  }
  return count;
}

/**
 * gst_mse_src_get_n_audio:
 * @self: #GstMseSrc instance
 *
 * Returns: the number of audio tracks available from this source
 * Since: 1.24
 */
guint
gst_mse_src_get_n_audio (GstMseSrc * self)
{
  g_return_val_if_fail (GST_IS_MSE_SRC (self), 0);
  return n_streams_by_type (self, GST_MEDIA_SOURCE_TRACK_TYPE_AUDIO);
}

/**
 * gst_mse_src_get_n_text:
 * @self: #GstMseSrc instance
 *
 * Returns: the number of text tracks available from this source
 * Since: 1.24
 */
guint
gst_mse_src_get_n_text (GstMseSrc * self)
{
  g_return_val_if_fail (GST_IS_MSE_SRC (self), 0);
  return n_streams_by_type (self, GST_MEDIA_SOURCE_TRACK_TYPE_TEXT);
}

/**
 * gst_mse_src_get_n_video:
 * @self: #GstMseSrc instance
 *
 * Returns: the number of video tracks available from this source
 * Since: 1.24
 */
guint
gst_mse_src_get_n_video (GstMseSrc * self)
{
  g_return_val_if_fail (GST_IS_MSE_SRC (self), 0);
  return n_streams_by_type (self, GST_MEDIA_SOURCE_TRACK_TYPE_VIDEO);
}

void
gst_mse_src_decode_error (GstMseSrc * self)
{
  g_return_if_fail (GST_IS_MSE_SRC (self));
  GstMseSrcReadyState ready_state = g_atomic_int_get (&self->ready_state);
  if (ready_state == GST_MSE_SRC_READY_STATE_HAVE_NOTHING) {
    GST_ELEMENT_ERROR (self, STREAM, DECODE, (DECODE_ERROR),
        ("the necessary decoder may be missing from this installation"));
  } else {
    GST_ELEMENT_ERROR (self, STREAM, DECODE, (DECODE_ERROR),
        ("the stream may be corrupt"));
  }
}

void
gst_mse_src_network_error (GstMseSrc * self)
{
  g_return_if_fail (GST_IS_MSE_SRC (self));
  GstMseSrcReadyState ready_state = g_atomic_int_get (&self->ready_state);
  if (ready_state == GST_MSE_SRC_READY_STATE_HAVE_NOTHING) {
    GST_ELEMENT_ERROR (self, RESOURCE, OPEN_READ, (NETWORK_ERROR),
        ("an error occurred before any media was read"));
  } else {
    GST_ELEMENT_ERROR (self, RESOURCE, READ, (NETWORK_ERROR),
        ("an error occurred while reading media"));
  }
}

static inline gboolean
is_streamable (GstMediaSourceTrack * track)
{
  switch (gst_media_source_track_get_track_type (track)) {
    case GST_MEDIA_SOURCE_TRACK_TYPE_AUDIO:
    case GST_MEDIA_SOURCE_TRACK_TYPE_TEXT:
    case GST_MEDIA_SOURCE_TRACK_TYPE_VIDEO:
      return TRUE;
    default:
      return FALSE;
  }
}

static GstStream *
create_gst_stream (GstMediaSourceTrack * track)
{
  gchar *stream_id = g_strdup_printf ("%s-%s",
      GST_OBJECT_NAME (track), gst_media_source_track_get_id (track));
  GstStream *stream = gst_stream_new (stream_id,
      gst_media_source_track_get_initial_caps (track),
      gst_media_source_track_get_stream_type (track), GST_STREAM_FLAG_SELECT);
  g_free (stream_id);
  return stream;
}

static void
set_flushing_and_signal (GstMseSrcPad * pad)
{
  GST_TRACE_OBJECT (pad, "locking");
  LINKED_OR_FLUSHING_LOCK (pad);
  g_atomic_int_set (&pad->flushing, TRUE);
  LINKED_OR_FLUSHING_SIGNAL (pad);
  LINKED_OR_FLUSHING_UNLOCK (pad);
  GST_TRACE_OBJECT (pad, "done");
}

static void
clear_flushing (GstMseSrcPad * pad)
{
  GST_TRACE_OBJECT (pad, "locking");
  LINKED_OR_FLUSHING_LOCK (pad);
  g_atomic_int_set (&pad->flushing, FALSE);
  LINKED_OR_FLUSHING_UNLOCK (pad);
  GST_TRACE_OBJECT (pad, "done");
}

static void
flush_stream (GstMseSrc * self, Stream * stream, gboolean is_seek)
{
  GstMseSrcPad *pad = GST_MSE_SRC_PAD (stream->pad);
  gst_pad_push_event (GST_PAD (pad), gst_event_new_flush_start ());

  if (is_seek) {
    GST_DEBUG_OBJECT (pad, "flushing for seek to %" GST_TIMEP_FORMAT,
        &self->start_time);
    set_flushing_and_signal (pad);
    gst_media_source_track_flush (stream->track);
    gst_pad_stop_task (GST_PAD (pad));
    GST_DEBUG_OBJECT (pad, "stopped task");
    GstSegment *segment = &(pad->segment);
    segment->base = 0;
    segment->start = self->start_time;
    segment->time = self->start_time;
    segment->rate = self->rate;
  }

  gst_media_source_track_flush (stream->track);
  g_atomic_int_set (&pad->does_need_segment, TRUE);

  gst_pad_push_event (GST_PAD (pad), gst_event_new_flush_stop (is_seek));
}

static void
flush_all_streams (GstMseSrc * self, gboolean is_seek)
{
  GHashTableIter iter;
  g_hash_table_iter_init (&iter, self->streams);
  for (gpointer value; g_hash_table_iter_next (&iter, NULL, &value);) {
    flush_stream (self, (Stream *) value, is_seek);
  }
}

static void
resume_all_streams (GstMseSrc * self)
{
  GstState state;
  gst_element_get_state (GST_ELEMENT (self), &state, NULL, 0);
  gboolean active = state > GST_STATE_READY;

  GHashTableIter iter;
  g_hash_table_iter_init (&iter, self->streams);
  for (gpointer value; g_hash_table_iter_next (&iter, NULL, &value);) {
    Stream *stream = value;
    GstPad *pad = GST_PAD (stream->pad);
    if (active) {
      clear_flushing (GST_MSE_SRC_PAD (pad));
      gst_pad_start_task (pad, (GstTaskFunction) pad_task, pad, NULL);
    }
  }
}

static void
tear_down_stream (GstMseSrc * self, Stream * stream)
{
  GST_DEBUG_OBJECT (self, "tearing down stream %s",
      gst_media_source_track_get_id (stream->track));

  flush_stream (self, stream, FALSE);
  gst_pad_set_active (stream->pad, FALSE);

  if (gst_stream_collection_get_size (self->collection) > 0) {
    gst_element_remove_pad (GST_ELEMENT (self), stream->pad);
    FLOW_COMBINER_LOCK (self);
    gst_flow_combiner_remove_pad (self->flow_combiner, stream->pad);
    FLOW_COMBINER_UNLOCK (self);
  }
}


static void
tear_down_all_streams (GstMseSrc * self)
{
  GHashTableIter iter;
  g_hash_table_iter_init (&iter, self->streams);
  for (gpointer value; g_hash_table_iter_next (&iter, NULL, &value);) {
    tear_down_stream (self, (Stream *) value);
    g_hash_table_iter_remove (&iter);
  }
}

static GstStateChangeReturn
gst_mse_src_change_state (GstElement * element, GstStateChange transition)
{
  GstMseSrc *self = GST_MSE_SRC (element);
  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      tear_down_all_streams (self);
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      gst_mse_src_detach (self);
      break;
    default:
      break;
  }
  return GST_ELEMENT_CLASS (gst_mse_src_parent_class)->change_state (element,
      transition);
}

static void
gst_mse_src_seek (GstMseSrc * self, GstClockTime start_time, gdouble rate)
{
  self->start_time = start_time;
  self->rate = rate;

  flush_all_streams (self, TRUE);
  if (self->media_source) {
    GST_DEBUG_OBJECT (self, "seeking on media source %" GST_PTR_FORMAT,
        self->media_source);
    gst_media_source_seek (self->media_source, start_time);
  } else {
    GST_DEBUG_OBJECT (self, "detached, not seeking on media source");
  }
  resume_all_streams (self);
}

static gboolean
gst_mse_src_send_event (GstElement * element, GstEvent * event)
{
  if (GST_EVENT_TYPE (event) != GST_EVENT_SEEK) {
    return GST_ELEMENT_CLASS (gst_mse_src_parent_class)->send_event (element,
        event);
  }

  GstMseSrc *self = GST_MSE_SRC (element);

  gdouble rate;
  GstFormat format;
  GstSeekType seek_type;
  gint64 start;
  gst_event_parse_seek (event, &rate, &format, NULL, &seek_type, &start, NULL,
      NULL);

  gst_event_unref (event);

  if (format != GST_FORMAT_TIME || seek_type != GST_SEEK_TYPE_SET) {
    GST_ERROR_OBJECT (self,
        "Rejecting unsupported seek event: %" GST_PTR_FORMAT, event);
    return FALSE;
  }

  GST_DEBUG_OBJECT (self, "handling %" GST_PTR_FORMAT, event);
  gst_mse_src_seek (self, start, rate);
  return TRUE;
}

static inline gboolean
is_flushing (GstMseSrcPad * pad)
{
  return g_atomic_int_get (&pad->flushing) || GST_PAD_IS_FLUSHING (pad);
}

static void
await_pad_linked_or_flushing (GstMseSrcPad * pad)
{
  GST_TRACE_OBJECT (pad, "waiting for link");
  LINKED_OR_FLUSHING_LOCK (pad);
  while (!gst_pad_is_linked (GST_PAD_CAST (pad)) && !is_flushing (pad)) {
    LINKED_OR_FLUSHING_WAIT (pad);
  }
  LINKED_OR_FLUSHING_UNLOCK (pad);
  GST_TRACE_OBJECT (pad, "linked");
}

static gboolean
all_pads_eos_fold (const GValue * item, gboolean * all_eos, gpointer user_data)
{
  GstMseSrcPad *pad = g_value_get_object (item);
  if (pad->eos) {
    return TRUE;
  } else {
    *all_eos = FALSE;
    return FALSE;
  }
}

static gboolean
all_pads_eos (GstMseSrc * self)
{
  GstIterator *iter = gst_element_iterate_src_pads (GST_ELEMENT_CAST (self));
  gboolean all_eos = TRUE;
  while (gst_iterator_fold (iter,
          (GstIteratorFoldFunction) all_pads_eos_fold, (GValue *) & all_eos,
          NULL) == GST_ITERATOR_RESYNC) {
    gst_iterator_resync (iter);
  }
  gst_iterator_free (iter);
  return all_eos;
}

static void
pad_task (GstMseSrcPad * pad)
{
  await_pad_linked_or_flushing (pad);

  if (is_flushing (pad)) {
    GST_TRACE_OBJECT (pad, "pad is flushing");
    goto pause;
  }

  GstMseSrc *self = GST_MSE_SRC (gst_pad_get_parent_element (GST_PAD (pad)));

  GstMediaSourceTrack *track = pad->track;

  GstMiniObject *object = gst_media_source_track_pop (track);

  if (object == NULL) {
    GST_DEBUG_OBJECT (pad, "nothing was popped from track, must be flushing");
    gst_media_source_track_flush (track);
    goto pause;
  }

  if (!g_atomic_int_get (&pad->sent_stream_start)) {
    const gchar *track_id = gst_media_source_track_get_id (track);
    GstEvent *event = gst_event_new_stream_start (track_id);
    gst_event_set_group_id (event, self->group_id);
    gst_event_set_stream (event, pad->stream);
    if (!gst_pad_push_event (GST_PAD (pad), event)) {
      GST_ERROR_OBJECT (pad, "failed to push stream start");
      goto pause;
    }
    GST_TRACE_OBJECT (pad, "stream start");
    g_atomic_int_set (&pad->sent_stream_start, TRUE);
  }

  GstCaps *caps = gst_media_source_track_get_initial_caps (track);
  if (!g_atomic_int_get (&pad->sent_initial_caps) && GST_IS_CAPS (caps)) {
    GST_DEBUG_OBJECT (pad, "sending initial caps");
    gst_caps_replace (&pad->most_recent_caps, caps);
    GstEvent *event = gst_event_new_caps (caps);
    if (!gst_pad_push_event (GST_PAD (pad), event)) {
      GST_ERROR_OBJECT (pad, "failed to push caps update");
      goto pause;
    }
    GST_TRACE_OBJECT (pad, "initial caps %" GST_PTR_FORMAT, caps);
    g_atomic_int_set (&pad->sent_initial_caps, TRUE);
  }

  if (g_atomic_int_get (&pad->does_need_segment)) {
    GST_DEBUG_OBJECT (pad, "sending new segment starting@%" GST_TIMEP_FORMAT,
        &pad->segment.time);
    GstEvent *event = gst_event_new_segment (&pad->segment);
    if (!gst_pad_push_event (GST_PAD (pad), event)) {
      GST_ERROR_OBJECT (pad, "failed to push new segment");
      goto pause;
    }
    GST_TRACE_OBJECT (pad, "segment");
    g_atomic_int_set (&pad->does_need_segment, FALSE);
  }

  if (!g_atomic_int_get (&pad->sent_stream_collection)) {
    GstEvent *event = gst_event_new_stream_collection (self->collection);
    if (!gst_pad_push_event (GST_PAD (pad), event)) {
      GST_ERROR_OBJECT (pad, "failed to push stream collection");
      goto pause;
    }
    GST_TRACE_OBJECT (pad, "stream collection");
    g_atomic_int_set (&pad->sent_stream_collection, TRUE);
  }

  if (GST_IS_SAMPLE (object)) {
    GstSample *sample = GST_SAMPLE (object);
    GstCaps *sample_caps = gst_sample_get_caps (sample);

    if (!gst_caps_is_equal (pad->most_recent_caps, sample_caps)) {
      gst_caps_replace (&pad->most_recent_caps, sample_caps);
      GstEvent *event = gst_event_new_caps (gst_caps_ref (sample_caps));
      if (!gst_pad_push_event (GST_PAD (pad), event)) {
        GST_ERROR_OBJECT (pad, "failed to push new caps");
        goto pause;
      }
      GST_TRACE_OBJECT (pad, "new caps %" GST_PTR_FORMAT, sample_caps);
    }

    GstBuffer *buffer = gst_buffer_copy (gst_sample_get_buffer (sample));
    if (GST_BUFFER_DTS_IS_VALID (buffer)) {
      pad->position = GST_BUFFER_DTS (buffer);
    }

    GstFlowReturn push_result = gst_pad_push (GST_PAD (pad), buffer);

    FLOW_COMBINER_LOCK (self);
    GstFlowReturn combined_result =
        gst_flow_combiner_update_pad_flow (self->flow_combiner,
        GST_PAD_CAST (pad), push_result);
    FLOW_COMBINER_UNLOCK (self);

    if (combined_result != GST_FLOW_OK) {
      GST_DEBUG_OBJECT (pad, "push result: %s, combined result: %s",
          gst_flow_get_name (push_result), gst_flow_get_name (combined_result));
      goto pause;
    }
  } else if (GST_IS_EVENT (object)) {
    if (GST_EVENT_TYPE (object) == GST_EVENT_EOS) {
      g_mutex_lock (&self->eos_lock);
      pad->eos = TRUE;
      g_cond_broadcast (&self->eos_cond);
      g_mutex_unlock (&self->eos_lock);
      g_mutex_lock (&self->eos_lock);
      while (!all_pads_eos (self)) {
        GST_DEBUG_OBJECT (pad, "waiting for eos on all tracks");
        g_cond_wait (&self->eos_cond, &self->eos_lock);
      }
      g_mutex_unlock (&self->eos_lock);
      GST_DEBUG_OBJECT (pad, "have eos on all tracks");
    }
    if (!gst_pad_push_event (GST_PAD (pad), GST_EVENT (object))) {
      GST_ERROR_OBJECT (self, "failed to push enqueued event");
      goto pause;
    }
  } else {
    GST_ERROR_OBJECT (self, "unexpected object on track queue"
        ", only samples and events are supported");
    g_assert_not_reached ();
  }

  return;

pause:
  if (!g_atomic_int_get (&pad->flushing)) {
    gst_pad_pause_task (GST_PAD (pad));
  }
}

static gboolean
pad_activate_mode (GstMseSrcPad * pad, GstObject * parent, GstPadMode mode,
    gboolean active)
{
  if (mode != GST_PAD_MODE_PUSH) {
    GST_ERROR_OBJECT (parent, "msesrc only supports push mode");
    return FALSE;
  }

  if (active) {
    gst_pad_start_task (GST_PAD (pad), (GstTaskFunction) pad_task, pad, NULL);
  } else {
    set_flushing_and_signal (pad);
    gst_media_source_track_flush (pad->track);
    gst_pad_stop_task (GST_PAD (pad));
    clear_flushing (pad);
  }

  return TRUE;
}

static GstPadLinkReturn
pad_linked (GstMseSrcPad * pad, GstMseSrc * parent, GstPad * sink)
{
  GST_DEBUG_OBJECT (pad, "pad is linked to %" GST_PTR_FORMAT ", resuming task",
      sink);
  LINKED_OR_FLUSHING_LOCK (pad);
  LINKED_OR_FLUSHING_SIGNAL (pad);
  LINKED_OR_FLUSHING_UNLOCK (pad);
  return GST_PAD_LINK_OK;
}

static gboolean
pad_event (GstMseSrcPad * pad, GstMseSrc * parent, GstEvent * event)
{
  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_SEEK:
      return gst_element_send_event (GST_ELEMENT (parent), event);
    default:
      return gst_pad_event_default (GST_PAD (pad), GST_OBJECT (parent), event);
  }
}

static gboolean
pad_query (GstMseSrcPad * pad, GstObject * parent, GstQuery * query)
{
  GstMseSrc *self = GST_MSE_SRC (parent);
  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_POSITION:{
      GstClockTime position = pad->position;
      GstFormat fmt;
      gst_query_parse_position (query, &fmt, NULL);
      if (fmt == GST_FORMAT_TIME && GST_CLOCK_TIME_IS_VALID (position)) {
        GST_TRACE_OBJECT (pad, "position query returning %" GST_TIMEP_FORMAT,
            &position);
        gst_query_set_position (query, GST_FORMAT_TIME, position);
        return TRUE;
      }
      break;
    }
    case GST_QUERY_DURATION:{
      GstFormat fmt;
      gst_query_parse_duration (query, &fmt, NULL);
      if (fmt == GST_FORMAT_TIME) {
        gst_query_set_duration (query, GST_FORMAT_TIME, self->duration);
        return TRUE;
      } else {
        return FALSE;
      }
    }
    case GST_QUERY_SEEKING:{
      GstFormat fmt;
      gst_query_parse_seeking (query, &fmt, NULL, NULL, NULL);
      if (fmt != GST_FORMAT_TIME) {
        return FALSE;
      }
      gst_query_set_seeking (query, GST_FORMAT_TIME, TRUE, 0, self->duration);
      return TRUE;
    }
    default:
      break;
  }
  return gst_pad_query_default (GST_PAD (pad), parent, query);
}

static void
append_stream (GstMseSrc * self, GstMediaSourceTrack * track)
{
  if (g_hash_table_contains (self->streams, track)) {
    GST_DEBUG_OBJECT (self, "skipping processed %" GST_PTR_FORMAT, track);
    return;
  }
  GST_DEBUG_OBJECT (self, "creating stream for %" GST_PTR_FORMAT, track);
  guint pad_index = g_hash_table_size (self->streams);
  GstStream *info = create_gst_stream (track);
  Stream stream = {
    .info = gst_object_ref (info),
    .track = gst_object_ref (track),
    .pad = gst_mse_src_pad_new (track, info, pad_index,
        self->start_time, self->rate),
  };
  g_hash_table_insert (self->streams, track,
      g_memdup2 (&stream, sizeof (Stream)));
  gst_stream_collection_add_stream (self->collection, stream.info);
}

void
gst_mse_src_emit_streams (GstMseSrc * self, GstMediaSourceTrack ** tracks,
    gsize n_tracks)
{
  g_return_if_fail (GST_IS_MSE_SRC (self));

  GstElement *element = GST_ELEMENT (self);
  GstObject *object = GST_OBJECT (self);

  update_ready_state_for_init_segment (self);

  STREAMS_LOCK (self);

  for (gsize i = 0; i < n_tracks; i++) {
    GstMediaSourceTrack *track = tracks[i];
    if (!is_streamable (track)) {
      continue;
    }
    append_stream (self, track);
  }

  GstState state;
  gst_element_get_state (element, &state, NULL, 0);
  gboolean active = state > GST_STATE_READY;

  GHashTableIter iter;
  g_hash_table_iter_init (&iter, self->streams);
  for (gpointer value; g_hash_table_iter_next (&iter, NULL, &value);) {
    Stream *stream = value;
    GstPad *pad = stream->pad;
    if (active) {
      gst_pad_set_active (pad, TRUE);
    }
    GstElement *parent = gst_pad_get_parent_element (pad);
    if (parent) {
      GST_DEBUG_OBJECT (self, "skipping parented pad %" GST_PTR_FORMAT, pad);
      gst_object_unref (parent);
      continue;
    }
    gst_element_add_pad (element, pad);
    FLOW_COMBINER_LOCK (self);
    gst_flow_combiner_add_pad (self->flow_combiner, pad);
    FLOW_COMBINER_UNLOCK (self);
  }
  STREAMS_UNLOCK (self);

  gst_element_no_more_pads (element);
  gst_element_post_message (element, gst_message_new_stream_collection (object,
          self->collection));
}

void
gst_mse_src_update_ready_state (GstMseSrc * self)
{
  g_return_if_fail (GST_IS_MSE_SRC (self));
  update_ready_state_for_sample (self);
}

static GstURIType
gst_mse_src_uri_get_type (GType type)
{
  return GST_URI_SRC;
}

static const gchar *const *
gst_mse_src_uri_get_protocols (GType type)
{
  static const gchar *protocols[] = { "mse", NULL };
  return protocols;
}

static gchar *
gst_mse_src_uri_get_uri (GstURIHandler * handler)
{
  GstMseSrc *self = GST_MSE_SRC (handler);
  return g_strdup (self->uri);
}

static gboolean
gst_mse_src_uri_set_uri (GstURIHandler * handler, const gchar * uri,
    GError ** error)
{
  GstMseSrc *self = GST_MSE_SRC (handler);
  g_free (self->uri);
  self->uri = g_strdup (uri);
  return TRUE;
}

static void
gst_mse_src_uri_handler_init (gpointer g_iface, gpointer iface_data)
{
  GstURIHandlerInterface *iface = (GstURIHandlerInterface *) g_iface;
  iface->get_type = gst_mse_src_uri_get_type;
  iface->get_protocols = gst_mse_src_uri_get_protocols;
  iface->get_uri = gst_mse_src_uri_get_uri;
  iface->set_uri = gst_mse_src_uri_set_uri;
}

void
gst_mse_src_attach (GstMseSrc * self, GstMediaSource * media_source)
{
  g_return_if_fail (GST_IS_MSE_SRC (self));
  g_return_if_fail (GST_IS_MEDIA_SOURCE (media_source));
  g_set_object (&self->media_source, media_source);
}

void
gst_mse_src_detach (GstMseSrc * self)
{
  g_return_if_fail (GST_IS_MSE_SRC (self));
  gst_clear_object (&self->media_source);
}

static void
set_ready_state (GstMseSrc * self, GstMseSrcReadyState ready_state)
{
  if (ready_state == self->ready_state) {
    return;
  }
  GST_DEBUG_OBJECT (self, "ready state %d=>%d", self->ready_state, ready_state);
  self->ready_state = ready_state;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_READY_STATE]);
}

static void
update_ready_state_for_init_segment (GstMseSrc * self)
{
  g_return_if_fail (GST_IS_MEDIA_SOURCE (self->media_source));
  if (self->ready_state != GST_MSE_SRC_READY_STATE_HAVE_NOTHING) {
    return;
  }
  GstSourceBufferList *buffers = gst_media_source_get_source_buffers
      (self->media_source);
  gboolean all_received_init_segment = TRUE;
  for (guint i = 0; all_received_init_segment; i++) {
    GstSourceBuffer *buf = gst_source_buffer_list_index (buffers, i);
    if (buf == NULL) {
      break;
    }
    all_received_init_segment &= gst_source_buffer_has_init_segment (buf);
    gst_object_unref (buf);
  }
  if (!all_received_init_segment) {
    return;
  }
  set_ready_state (self, GST_MSE_SRC_READY_STATE_HAVE_METADATA);
}

static gboolean
has_current_data (GstMseSrc * self)
{
  GstClockTime position = gst_mse_src_get_position (self);
  if (!GST_CLOCK_TIME_IS_VALID (position)) {
    return FALSE;
  }
  GstSourceBufferList *active =
      gst_media_source_get_active_source_buffers (self->media_source);
  gboolean has_data = TRUE;
  for (guint i = 0; has_data; i++) {
    GstSourceBuffer *buf = gst_source_buffer_list_index (active, i);
    if (buf == NULL) {
      if (i == 0) {
        has_data = FALSE;
        GST_DEBUG_OBJECT (self,
            "no active source buffers, nothing is buffered");
      }
      break;
    }
    has_data = gst_source_buffer_is_buffered (buf, position);
    gst_object_unref (buf);
  }
  g_object_unref (active);
  return has_data;
}

static gboolean
has_future_data (GstMseSrc * self)
{
  GstClockTime position = gst_mse_src_get_position (self);
  GstClockTime duration = self->duration;
  if (!GST_CLOCK_TIME_IS_VALID (position)
      || !GST_CLOCK_TIME_IS_VALID (duration)) {
    return FALSE;
  }
  GstClockTime target_position = MIN (position + THRESHOLD_FUTURE_DATA,
      duration);
  GstSourceBufferList *active =
      gst_media_source_get_active_source_buffers (self->media_source);
  gboolean has_data = TRUE;
  for (guint i = 0; has_data; i++) {
    GstSourceBuffer *buf = gst_source_buffer_list_index (active, i);
    if (buf == NULL) {
      if (i == 0) {
        has_data = FALSE;
        GST_DEBUG_OBJECT (self,
            "no active source buffers, nothing is buffered");
      }
      break;
    }
    has_data = gst_source_buffer_is_range_buffered (buf, position,
        target_position);
    gst_object_unref (buf);
  }
  g_object_unref (active);
  return has_data;
}

static gboolean
has_enough_data (GstMseSrc * self)
{
  GstClockTime position = gst_mse_src_get_position (self);
  GstClockTime duration = self->duration;
  if (!GST_CLOCK_TIME_IS_VALID (position)
      || !GST_CLOCK_TIME_IS_VALID (duration)) {
    return FALSE;
  }
  GstClockTime target_position = MIN (position + THRESHOLD_ENOUGH_DATA,
      duration);
  GstSourceBufferList *active =
      gst_media_source_get_active_source_buffers (self->media_source);
  gboolean has_data = TRUE;
  for (guint i = 0; has_data; i++) {
    GstSourceBuffer *buf = gst_source_buffer_list_index (active, i);
    if (buf == NULL) {
      if (i == 0) {
        has_data = FALSE;
        GST_DEBUG_OBJECT (self,
            "no active source buffers, nothing is buffered");
      }
      break;
    }
    has_data = gst_source_buffer_is_range_buffered (buf, position,
        target_position);
    gst_object_unref (buf);
  }
  g_object_unref (active);
  return has_data;
}

static void
update_ready_state_for_sample (GstMseSrc * self)
{
  g_return_if_fail (GST_IS_MEDIA_SOURCE (self->media_source));
  g_return_if_fail (self->ready_state >= GST_MSE_SRC_READY_STATE_HAVE_METADATA);

  if (has_enough_data (self)) {
    set_ready_state (self, GST_MSE_SRC_READY_STATE_HAVE_ENOUGH_DATA);
  } else if (has_future_data (self)) {
    set_ready_state (self, GST_MSE_SRC_READY_STATE_HAVE_FUTURE_DATA);
  } else if (has_current_data (self)) {
    set_ready_state (self, GST_MSE_SRC_READY_STATE_HAVE_CURRENT_DATA);
  } else {
    set_ready_state (self, GST_MSE_SRC_READY_STATE_HAVE_METADATA);
  }
}
