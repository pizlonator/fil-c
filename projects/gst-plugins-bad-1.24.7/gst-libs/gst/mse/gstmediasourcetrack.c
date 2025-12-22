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

#include <gst/mse/mse-enumtypes.h>
#include <gst/mse/mse-enumtypes-private.h>
#include "gstmediasourcetrack-private.h"

#include "gstmselogging-private.h"

#include <gst/base/gstdataqueue.h>

struct _GstMediaSourceTrack
{
  GstObject parent_instance;

  GstMediaSourceTrackType track_type;
  gchar *track_id;
  gboolean active;
  GstCaps *initial_caps;

  gsize queue_size;
  GstDataQueue *samples;
};

G_DEFINE_TYPE (GstMediaSourceTrack, gst_media_source_track, GST_TYPE_OBJECT);

#define DEFAULT_QUEUE_SIZE (1 << 10)

enum
{
  PROP_0,

  PROP_TRACK_TYPE,
  PROP_TRACK_ID,
  PROP_ACTIVE,
  PROP_INITIAL_CAPS,
  PROP_QUEUE_SIZE,

  N_PROPS,
};

enum
{
  ON_NOT_EMPTY,

  N_SIGNALS,
};

static GParamSpec *properties[N_PROPS];
static guint signals[N_SIGNALS];

static GstMediaSourceTrack *
_gst_media_source_track_new_full (GstMediaSourceTrackType type,
    const gchar * track_id, gsize size, GstCaps * initial_caps)
{
  g_return_val_if_fail (type >= 0, NULL);
  g_return_val_if_fail (type <= GST_MEDIA_SOURCE_TRACK_TYPE_OTHER, NULL);

  gst_mse_init_logging ();

  GstMediaSourceTrack *self = g_object_new (GST_TYPE_MEDIA_SOURCE_TRACK,
      "track-type", type, "track-id", track_id, "queue-size", size,
      "initial-caps", initial_caps, NULL);

  return gst_object_ref_sink (self);
}

GstMediaSourceTrack *
gst_media_source_track_new_with_size (GstMediaSourceTrackType type,
    const gchar * track_id, gsize size)
{
  return _gst_media_source_track_new_full (type, track_id, size, NULL);
}

GstMediaSourceTrack *
gst_media_source_track_new (GstMediaSourceTrackType type,
    const gchar * track_id)
{
  return _gst_media_source_track_new_full (type, track_id, DEFAULT_QUEUE_SIZE,
      NULL);
}

GstMediaSourceTrack *
gst_media_source_track_new_with_initial_caps (GstMediaSourceTrackType type,
    const gchar * track_id, GstCaps * initial_caps)
{
  g_return_val_if_fail (GST_IS_CAPS (initial_caps), NULL);
  return _gst_media_source_track_new_full (type, track_id, DEFAULT_QUEUE_SIZE,
      initial_caps);
}

static void
gst_media_source_track_finalize (GObject * object)
{
  GstMediaSourceTrack *self = (GstMediaSourceTrack *) object;

  g_free (self->track_id);
  gst_clear_caps (&self->initial_caps);
  g_object_unref (self->samples);

  G_OBJECT_CLASS (gst_media_source_track_parent_class)->finalize (object);
}

static void
gst_media_source_track_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstMediaSourceTrack *self = GST_MEDIA_SOURCE_TRACK (object);

  switch (prop_id) {
    case PROP_TRACK_TYPE:
      g_value_set_enum (value, gst_media_source_track_get_track_type (self));
      break;
    case PROP_TRACK_ID:
      g_value_set_static_string (value, gst_media_source_track_get_id (self));
      break;
    case PROP_ACTIVE:
      g_value_set_boolean (value, gst_media_source_track_get_active (self));
      break;
    case PROP_INITIAL_CAPS:
      g_value_take_boxed (value,
          gst_media_source_track_get_initial_caps (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
gst_media_source_track_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstMediaSourceTrack *self = GST_MEDIA_SOURCE_TRACK (object);

  switch (prop_id) {
    case PROP_TRACK_TYPE:
      self->track_type = g_value_get_enum (value);
      break;
    case PROP_TRACK_ID:
      self->track_id = g_value_dup_string (value);
      break;
    case PROP_ACTIVE:
      gst_media_source_track_set_active (self, g_value_get_boolean (value));
      break;
    case PROP_INITIAL_CAPS:{
      const GstCaps *caps = gst_value_get_caps (value);
      self->initial_caps = caps == NULL ? NULL : gst_caps_copy (caps);
      break;
    }
    case PROP_QUEUE_SIZE:{
      self->queue_size = g_value_get_ulong (value);
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
gst_media_source_track_class_init (GstMediaSourceTrackClass * klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);

  oclass->finalize = GST_DEBUG_FUNCPTR (gst_media_source_track_finalize);
  oclass->get_property =
      GST_DEBUG_FUNCPTR (gst_media_source_track_get_property);
  oclass->set_property =
      GST_DEBUG_FUNCPTR (gst_media_source_track_set_property);

  signals[ON_NOT_EMPTY] = g_signal_new ("on-not-empty",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);

  properties[PROP_TRACK_TYPE] =
      g_param_spec_enum ("track-type", "Track Type",
      "Type of media in this Track, either Audio, Video, Text, or Other.",
      GST_TYPE_MEDIA_SOURCE_TRACK_TYPE, GST_MEDIA_SOURCE_TRACK_TYPE_OTHER,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  properties[PROP_TRACK_ID] =
      g_param_spec_string ("track-id", "Track ID",
      "Identifier for this Track that must be unique within a Source Buffer.",
      "", G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  properties[PROP_ACTIVE] =
      g_param_spec_boolean ("active", "Active",
      "Whether this Track requires its parent Source Buffer to be in its "
      "parent Media Source's Active Source Buffers list", FALSE,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  properties[PROP_INITIAL_CAPS] =
      g_param_spec_boxed ("initial-caps", "Initial Caps",
      "GstCaps discovered in the first Initialization Segment",
      GST_TYPE_CAPS, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
      G_PARAM_STATIC_STRINGS);

  properties[PROP_QUEUE_SIZE] =
      g_param_spec_ulong ("queue-size", "Queue Size",
      "Maximum Track Queue size",
      0, G_MAXULONG, DEFAULT_QUEUE_SIZE,
      G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (oclass, N_PROPS, properties);
}

static gboolean
check_queue_depth (GstDataQueue * queue, guint visible, guint bytes,
    guint64 time, gpointer checkdata)
{
  GstMediaSourceTrack *self = GST_MEDIA_SOURCE_TRACK (checkdata);
  return visible >= self->queue_size;
}

static void
gst_media_source_track_init (GstMediaSourceTrack * self)
{
  self->samples = gst_data_queue_new (check_queue_depth, NULL, NULL, self);
}

GstMediaSourceTrackType
gst_media_source_track_get_track_type (GstMediaSourceTrack * self)
{
  g_return_val_if_fail (GST_IS_MEDIA_SOURCE_TRACK (self),
      GST_MEDIA_SOURCE_TRACK_TYPE_OTHER);
  return self->track_type;
}

GstStreamType
gst_media_source_track_get_stream_type (GstMediaSourceTrack * self)
{
  g_return_val_if_fail (GST_IS_MEDIA_SOURCE_TRACK (self),
      GST_STREAM_TYPE_UNKNOWN);
  return gst_media_source_track_type_to_stream_type (self->track_type);
}

const gchar *
gst_media_source_track_get_id (GstMediaSourceTrack * self)
{
  g_return_val_if_fail (GST_IS_MEDIA_SOURCE_TRACK (self), NULL);
  return self->track_id;
}

GstCaps *
gst_media_source_track_get_initial_caps (GstMediaSourceTrack * self)
{
  g_return_val_if_fail (GST_IS_MEDIA_SOURCE_TRACK (self), NULL);
  if (GST_IS_CAPS (self->initial_caps)) {
    return self->initial_caps;
  } else {
    return NULL;
  }
}

gboolean
gst_media_source_track_get_active (GstMediaSourceTrack * self)
{
  g_return_val_if_fail (GST_IS_MEDIA_SOURCE_TRACK (self), FALSE);
  return self->active;
}

void
gst_media_source_track_set_active (GstMediaSourceTrack * self, gboolean active)
{
  g_return_if_fail (GST_IS_MEDIA_SOURCE_TRACK (self));
  self->active = active;
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ACTIVE]);
}

GstMiniObject *
gst_media_source_track_pop (GstMediaSourceTrack * self)
{
  g_return_val_if_fail (GST_IS_MEDIA_SOURCE_TRACK (self), NULL);
  GstDataQueueItem *item = NULL;
  if (!gst_data_queue_pop (self->samples, &item)) {
    return NULL;
  }
  GstMiniObject *object = item->object;
  item->object = NULL;
  item->destroy (item);
  return object;
}

static inline gsize
sample_size (GstSample * sample)
{
  GstBuffer *buffer = gst_sample_get_buffer (sample);
  return gst_buffer_get_size (buffer);
}

static inline GstClockTime
sample_duration (GstSample * sample)
{
  GstBuffer *buffer = gst_sample_get_buffer (sample);
  return GST_BUFFER_DURATION (buffer);
}

static void
destroy_item (GstDataQueueItem * item)
{
  gst_clear_mini_object (&item->object);
  g_free (item);
}

static inline GstDataQueueItem *
wrap_sample (GstSample * sample)
{
  GstDataQueueItem item = {
    .object = GST_MINI_OBJECT (sample),
    .size = sample_size (sample),
    .duration = sample_duration (sample),
    .destroy = (GDestroyNotify) destroy_item,
    .visible = TRUE,
  };
  return g_memdup2 (&item, sizeof (GstDataQueueItem));
}

gboolean
gst_media_source_track_push (GstMediaSourceTrack * self, GstSample * sample)
{
  g_return_val_if_fail (GST_IS_MEDIA_SOURCE_TRACK (self), FALSE);
  g_return_val_if_fail (GST_IS_SAMPLE (sample), FALSE);

  gboolean was_empty = gst_media_source_track_is_empty (self);

  GstDataQueueItem *item = wrap_sample (sample);

  gboolean result = gst_data_queue_push (self->samples, item);

  if (result) {
    if (was_empty) {
      g_signal_emit (self, signals[ON_NOT_EMPTY], 0);
    }
    return TRUE;
  } else {
    item->destroy (item);
    return FALSE;
  }
}

static inline GstDataQueueItem *
wrap_eos (void)
{
  GstEvent *event = gst_event_ref (gst_event_new_eos ());

  GstDataQueueItem item = {
    .object = GST_MINI_OBJECT (event),
    .size = 0,
    .duration = 0,
    .destroy = (GDestroyNotify) destroy_item,
    .visible = TRUE,
  };
  return g_memdup2 (&item, sizeof (GstDataQueueItem));
}

gboolean
gst_media_source_track_push_eos (GstMediaSourceTrack * self)
{
  g_return_val_if_fail (GST_IS_MEDIA_SOURCE_TRACK (self), FALSE);

  GstDataQueueItem *item = wrap_eos ();

  gboolean result = gst_data_queue_push (self->samples, item);

  if (result) {
    return TRUE;
  } else {
    item->destroy (item);
    return FALSE;
  }
}

void
gst_media_source_track_flush (GstMediaSourceTrack * self)
{
  g_return_if_fail (GST_IS_MEDIA_SOURCE_TRACK (self));
  gst_data_queue_set_flushing (self->samples, TRUE);
  gst_data_queue_flush (self->samples);
}

void
gst_media_source_track_resume (GstMediaSourceTrack * self)
{
  g_return_if_fail (GST_IS_MEDIA_SOURCE_TRACK (self));
  gst_data_queue_set_flushing (self->samples, FALSE);
}

gboolean
gst_media_source_track_try_push (GstMediaSourceTrack * self, GstSample * sample)
{
  g_return_val_if_fail (GST_IS_MEDIA_SOURCE_TRACK (self), FALSE);
  g_return_val_if_fail (GST_IS_SAMPLE (sample), FALSE);

  if (gst_data_queue_is_full (self->samples)) {
    return FALSE;
  }

  return gst_media_source_track_push (self, sample);
}

gboolean
gst_media_source_track_is_empty (GstMediaSourceTrack * self)
{
  g_return_val_if_fail (GST_IS_MEDIA_SOURCE_TRACK (self), FALSE);
  return gst_data_queue_is_empty (self->samples);
}

GstStreamType
gst_media_source_track_type_to_stream_type (GstMediaSourceTrackType type)
{
  switch (type) {
    case GST_MEDIA_SOURCE_TRACK_TYPE_AUDIO:
      return GST_STREAM_TYPE_AUDIO;
    case GST_MEDIA_SOURCE_TRACK_TYPE_TEXT:
      return GST_STREAM_TYPE_TEXT;
    case GST_MEDIA_SOURCE_TRACK_TYPE_VIDEO:
      return GST_STREAM_TYPE_VIDEO;
    default:
      return GST_STREAM_TYPE_UNKNOWN;
  }
}

GstMediaSourceTrackType
gst_media_source_track_type_from_stream_type (GstStreamType type)
{
  switch (type) {
    case GST_STREAM_TYPE_AUDIO:
      return GST_MEDIA_SOURCE_TRACK_TYPE_AUDIO;
    case GST_STREAM_TYPE_TEXT:
      return GST_MEDIA_SOURCE_TRACK_TYPE_TEXT;
    case GST_STREAM_TYPE_VIDEO:
      return GST_MEDIA_SOURCE_TRACK_TYPE_VIDEO;
    default:
      return GST_MEDIA_SOURCE_TRACK_TYPE_OTHER;
  }
}
