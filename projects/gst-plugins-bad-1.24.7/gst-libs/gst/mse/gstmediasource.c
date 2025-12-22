/* GStreamer
 *
 * SPDX-License-Identifier: LGPL-2.1
 *
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 2013 Orange
 * Copyright (C) 2013-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2014 Sebastian Dröge <sebastian@centricular.com>
 * Copyright (C) 2015, 2016 Igalia, S.L
 * Copyright (C) 2015, 2016 Metrological Group B.V.
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

/**
 * SECTION:gstmediasource
 * @title: GstMediaSource
 * @short_description: Media Source
 * @symbols:
 * - GstMediaSource
 *
 * #GstMediaSource is the entry point into the W3C Media Source API. It offers
 * functionality similar to #GstAppSrc for client-side web or JavaScript
 * applications decoupling the source of media from its processing and playback.
 *
 * To interact with a Media Source, connect it to a #GstMseSrc that is in some
 * #GstPipeline using gst_media_source_attach(). Then create at least one
 * #GstSourceBuffer using gst_media_source_add_source_buffer(). Finally, feed
 * some media data to the Source Buffer(s) using
 * gst_source_buffer_append_buffer() and play the pipeline.
 *
 * Since: 1.24
 */

/**
 * GstMediaSource:
 * Since: 1.24
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/mse/mse-enumtypes.h>
#include "gstmediasource.h"
#include "gstmediasource-private.h"

#include "gstmselogging-private.h"
#include "gstmsemediatype-private.h"
#include "gstsourcebuffer-private.h"
#include "gstsourcebufferlist-private.h"

#include "gstmsesrc.h"
#include "gstmsesrc-private.h"

G_DEFINE_TYPE (GstMediaSource, gst_media_source, GST_TYPE_OBJECT);
G_DEFINE_QUARK (gst_media_source_error_quark, gst_media_source_error);

enum
{
  PROP_0,

  PROP_SOURCE_BUFFERS,
  PROP_ACTIVE_SOURCE_BUFFERS,
  PROP_READY_STATE,
  PROP_POSITION,
  PROP_DURATION,

  N_PROPS,
};

typedef enum
{
  ON_SOURCE_OPEN,
  ON_SOURCE_ENDED,
  ON_SOURCE_CLOSE,

  N_SIGNALS,
} MediaSourceEvent;

typedef struct
{
  GstDataQueueItem item;
  MediaSourceEvent event;
} MediaSourceEventItem;

static GParamSpec *properties[N_PROPS];
static guint signals[N_SIGNALS];

#define DEFAULT_READY_STATE GST_MEDIA_SOURCE_READY_STATE_CLOSED
#define DEFAULT_POSITION    GST_CLOCK_TIME_NONE
#define DEFAULT_DURATION    GST_CLOCK_TIME_NONE

static void rebuild_active_source_buffers (GstMediaSource * self);

/**
 * gst_media_source_is_type_supported:
 * @type: (transfer none): A MIME type value
 *
 * Determines whether the current Media Source configuration can process media
 * of the supplied @type.
 *
 * Returns: `TRUE` when supported, `FALSE` otherwise
 *
 * Since: 1.24
 */
gboolean
gst_media_source_is_type_supported (const gchar * type)
{
  gst_mse_init_logging ();
  g_return_val_if_fail (type != NULL, FALSE);

  if (g_strcmp0 (type, "") == 0) {
    return FALSE;
  }

  GstMediaSourceMediaType media_type = GST_MEDIA_SOURCE_MEDIA_TYPE_INIT;
  if (!gst_media_source_media_type_parse (&media_type, type)) {
    return FALSE;
  }

  gboolean supported = gst_media_source_media_type_is_supported (&media_type);

  gst_media_source_media_type_reset (&media_type);

  return supported;
}

/**
 * gst_media_source_new:
 *
 * Creates a new #GstMediaSource instance. The instance is in the
 * %GST_MEDIA_SOURCE_READY_STATE_CLOSED state and is not associated with any
 * media player.
 *
 * [Specification](https://www.w3.org/TR/media-source-2/#dom-mediasource-constructor)
 *
 * Returns: (transfer full): a new #GstMediaSource instance
 * Since: 1.24
 */
GstMediaSource *
gst_media_source_new (void)
{
  gst_mse_init_logging ();
  return g_object_ref_sink (g_object_new (GST_TYPE_MEDIA_SOURCE, NULL));
}

static inline void
empty_buffers (GstMediaSource * self)
{
  for (guint i = 0;; i++) {
    GstSourceBuffer *buf = gst_source_buffer_list_index (self->buffers, i);
    if (buf == NULL) {
      break;
    }
    gst_object_unparent (GST_OBJECT_CAST (buf));
    gst_object_unref (buf);
  }
  gst_source_buffer_list_remove_all (self->buffers);
}

static void
gst_media_source_dispose (GObject * object)
{
  GstMediaSource *self = (GstMediaSource *) object;

  gst_media_source_detach (self);

  g_clear_object (&self->active_buffers);

  if (self->buffers) {
    empty_buffers (self);
  }
  gst_clear_object (&self->buffers);

  gst_clear_object (&self->event_queue);

  G_OBJECT_CLASS (gst_media_source_parent_class)->dispose (object);
}

static void
gst_media_source_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstMediaSource *self = GST_MEDIA_SOURCE (object);

  switch (prop_id) {
    case PROP_SOURCE_BUFFERS:
      g_value_take_object (value, gst_media_source_get_source_buffers (self));
      break;
    case PROP_ACTIVE_SOURCE_BUFFERS:
      g_value_take_object (value,
          gst_media_source_get_active_source_buffers (self));
      break;
    case PROP_READY_STATE:
      g_value_set_enum (value, gst_media_source_get_ready_state (self));
      break;
    case PROP_POSITION:
      g_value_set_uint64 (value, gst_media_source_get_position (self));
      break;
    case PROP_DURATION:
      g_value_set_uint64 (value, gst_media_source_get_duration (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
gst_media_source_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstMediaSource *self = GST_MEDIA_SOURCE (object);

  switch (prop_id) {
    case PROP_DURATION:{
      GstClockTime duration = (GstClockTime) g_value_get_uint64 (value);
      gst_media_source_set_duration (self, duration, NULL);
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static void
gst_media_source_class_init (GstMediaSourceClass * klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);

  oclass->dispose = GST_DEBUG_FUNCPTR (gst_media_source_dispose);
  oclass->get_property = GST_DEBUG_FUNCPTR (gst_media_source_get_property);
  oclass->set_property = GST_DEBUG_FUNCPTR (gst_media_source_set_property);

  /**
   * GstMediaSource:source-buffers:
   *
   * A #GstSourceBufferList of every #GstSourceBuffer in this Media Source
   *
   * [Specification](https://www.w3.org/TR/media-source-2/#dom-mediasource-sourcebuffers)
   *
   * Since: 1.24
   */
  properties[PROP_SOURCE_BUFFERS] = g_param_spec_object ("source-buffers",
      "Source Buffers",
      "A SourceBufferList of all SourceBuffers in this Media Source",
      GST_TYPE_SOURCE_BUFFER_LIST, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * GstMediaSource:active-source-buffers:
   *
   * A #GstSourceBufferList of every #GstSourceBuffer in this Media Source that
   * is considered active
   *
   * [Specification](https://www.w3.org/TR/media-source-2/#dom-mediasource-activesourcebuffers)
   *
   * Since: 1.24
   */
  properties[PROP_ACTIVE_SOURCE_BUFFERS] =
      g_param_spec_object ("active-source-buffers", "Active Source Buffers",
      "A SourceBufferList of all SourceBuffers that are active in this Media Source",
      GST_TYPE_SOURCE_BUFFER_LIST, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * GstMediaSource:ready-state:
   *
   * The Ready State of the Media Source
   *
   * [Specification](https://www.w3.org/TR/media-source-2/#dom-mediasource-readystate)
   *
   * Since: 1.24
   */
  properties[PROP_READY_STATE] = g_param_spec_enum ("ready-state",
      "Ready State",
      "The Ready State of the Media Source",
      GST_TYPE_MEDIA_SOURCE_READY_STATE, DEFAULT_READY_STATE, G_PARAM_READABLE |
      G_PARAM_STATIC_STRINGS);

  /**
   * GstMediaSource:position:
   *
   * The position of the player consuming from the Media Source
   *
   * Since: 1.24
   */
  properties[PROP_POSITION] = g_param_spec_uint64 ("position",
      "Position",
      "The Position of the Media Source as a GstClockTime",
      GST_CLOCK_TIME_NONE, G_MAXUINT64, DEFAULT_DURATION, G_PARAM_READWRITE |
      G_PARAM_STATIC_STRINGS);

  /**
   * GstMediaSource:duration:
   *
   * The Duration of the Media Source as a #GstClockTime
   *
   * [Specification](https://www.w3.org/TR/media-source-2/#dom-mediasource-duration)
   *
   * Since: 1.24
   */
  properties[PROP_DURATION] = g_param_spec_uint64 ("duration",
      "Duration",
      "The Duration of the Media Source as a GstClockTime",
      GST_CLOCK_TIME_NONE, G_MAXUINT64, DEFAULT_DURATION, G_PARAM_READWRITE |
      G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (oclass, N_PROPS, properties);

  /**
   * GstMediaSource::on-source-open:
   * @self: The #GstMediaSource that has just opened
   *
   * Emitted when @self has been opened.
   *
   * [Specification](https://www.w3.org/TR/media-source-2/#dom-mediasource-onsourceopen)
   *
   * Since: 1.24
   */
  signals[ON_SOURCE_OPEN] = g_signal_new ("on-source-open",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);

  /**
   * GstMediaSource::on-source-ended:
   * @self: The #GstMediaSource that has just ended
   *
   * Emitted when @self has ended, normally through
   * gst_media_source_end_of_stream().
   *
   * [Specification](https://www.w3.org/TR/media-source-2/#dom-mediasource-onsourceended)
   *
   * Since: 1.24
   */
  signals[ON_SOURCE_ENDED] = g_signal_new ("on-source-ended",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);

  /**
   * GstMediaSource::on-source-closed:
   * @self: The #GstMediaSource that has just closed
   *
   * Emitted when @self has closed, normally when detached from a #GstMseSrc.
   *
   * [Specification](https://www.w3.org/TR/media-source-2/#dom-mediasource-onsourceclose)
   *
   * Since: 1.24
   */
  signals[ON_SOURCE_CLOSE] = g_signal_new ("on-source-close",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);

}

static inline void
reset_live_seekable_range (GstMediaSource * self)
{
  self->live_seekable_range.start = 0;
  self->live_seekable_range.end = 0;
}

static inline gboolean
is_updating (GstMediaSource * self)
{
  for (guint i = 0;; i++) {
    GstSourceBuffer *buf = gst_source_buffer_list_index (self->buffers, i);
    if (buf == NULL)
      break;
    gboolean updating = gst_source_buffer_get_updating (buf);
    gst_object_unref (buf);
    if (updating) {
      return TRUE;
    }
  }
  return FALSE;
}

static inline gboolean
is_attached (GstMediaSource * self)
{
  return GST_IS_MSE_SRC (self->element);
}

static inline void
network_error (GstMediaSource * self)
{
  if (is_attached (self)) {
    gst_mse_src_network_error (self->element);
  }
}

static inline void
decode_error (GstMediaSource * self)
{
  if (is_attached (self)) {
    gst_mse_src_decode_error (self->element);
  }
}

static inline void
update_duration (GstMediaSource * self)
{
  if (is_attached (self)) {
    gst_mse_src_set_duration (self->element, self->duration);
  }
}

static void
schedule_event (GstMediaSource * self, MediaSourceEvent event)
{
  MediaSourceEventItem item = {
    .item = {.destroy = g_free,.visible = TRUE,.size = 1,.object = NULL},
    .event = event,
  };

  gst_mse_event_queue_push (self->event_queue, g_memdup2 (&item,
          sizeof (MediaSourceEventItem)));
}

static void
dispatch_event (MediaSourceEventItem * item, GstMediaSource * self)
{
  g_signal_emit (self, signals[item->event], 0);
}

static void
gst_media_source_init (GstMediaSource * self)
{
  self->buffers = gst_source_buffer_list_new ();
  self->active_buffers = gst_source_buffer_list_new ();
  self->ready_state = DEFAULT_READY_STATE;
  self->duration = DEFAULT_DURATION;
  reset_live_seekable_range (self);
  self->element = NULL;
  self->event_queue =
      gst_mse_event_queue_new ((GstMseEventQueueCallback) dispatch_event, self);
}

/**
 * gst_media_source_attach:
 * @self: #GstMediaSource instance
 * @element: (transfer none): #GstMseSrc source Element
 *
 * Associates @self with @element.
 * Normally, the Element will be part of a #GstPipeline that plays back the data
 * submitted to the Media Source's Source Buffers.
 *
 * #GstMseSrc is a special source element that is designed to consume media from
 * a #GstMediaSource.
 *
 * [Specification](https://www.w3.org/TR/media-source-2/#dfn-attaching-to-a-media-element)
 *
 * Since: 1.24
 */
void
gst_media_source_attach (GstMediaSource * self, GstMseSrc * element)
{
  g_return_if_fail (GST_IS_MEDIA_SOURCE (self));
  g_return_if_fail (GST_IS_MSE_SRC (element));

  if (is_attached (self))
    gst_media_source_detach (self);

  self->element = gst_object_ref_sink (element);
  gst_mse_src_attach (element, self);

  self->ready_state = GST_MEDIA_SOURCE_READY_STATE_OPEN;
  schedule_event (self, ON_SOURCE_OPEN);
}

/**
 * gst_media_source_detach:
 * @self: #GstMediaSource instance
 *
 * Detaches @self from any #GstMseSrc element that it may be associated with.
 *
 * Since: 1.24
 */
void
gst_media_source_detach (GstMediaSource * self)
{
  g_return_if_fail (GST_IS_MEDIA_SOURCE (self));

  self->ready_state = GST_MEDIA_SOURCE_READY_STATE_CLOSED;
  gst_media_source_set_duration (self, GST_CLOCK_TIME_NONE, NULL);

  gst_source_buffer_list_remove_all (self->active_buffers);
  empty_buffers (self);

  if (is_attached (self)) {
    gst_mse_src_detach (self->element);
    gst_clear_object (&self->element);
  }

  schedule_event (self, ON_SOURCE_CLOSE);
}

/**
 * gst_media_source_get_source_element:
 * @self: #GstMediaSource instance
 *
 * Gets the #GstMseSrc currently attached to @self or `NULL` if there is none.
 *
 * Returns: (transfer full) (nullable): a #GstMseSrc instance or `NULL`
 */
GstMseSrc *
gst_media_source_get_source_element (GstMediaSource * self)
{
  g_return_val_if_fail (GST_IS_MEDIA_SOURCE (self), NULL);
  GST_OBJECT_LOCK (self);
  GstMseSrc *element = self->element == NULL ? NULL
      : gst_object_ref (self->element);
  GST_OBJECT_UNLOCK (self);
  return element;
}

void
gst_media_source_open (GstMediaSource * self)
{
  g_return_if_fail (GST_IS_MEDIA_SOURCE (self));
  if (self->ready_state != GST_MEDIA_SOURCE_READY_STATE_OPEN) {
    self->ready_state = GST_MEDIA_SOURCE_READY_STATE_OPEN;
    schedule_event (self, ON_SOURCE_OPEN);
  }
}

/**
 * gst_media_source_get_source_buffers:
 * @self: #GstMediaSource instance
 *
 * Gets a #GstSourceBufferList containing all the Source Buffers currently
 * associated with this Media Source. This object will reflect any future
 * changes to the parent Media Source as well.
 *
 * [Specification](https://www.w3.org/TR/media-source-2/#dom-mediasource-sourcebuffers)
 *
 * Returns: (transfer full): a #GstSourceBufferList instance
 * Since: 1.24
 */
GstSourceBufferList *
gst_media_source_get_source_buffers (GstMediaSource * self)
{
  g_return_val_if_fail (GST_IS_MEDIA_SOURCE (self), NULL);
  return g_object_ref (self->buffers);
}

/**
 * gst_media_source_get_active_source_buffers:
 * @self: #GstMediaSource instance
 *
 * Gets a #GstSourceBufferList containing all the Source Buffers currently
 * associated with this Media Source that are considered "active."
 * For a Source Buffer to be considered active, either its video track is
 * selected, its audio track is enabled, or its text track is visible or hidden.
 * This object will reflect any future changes to the parent Media Source as
 * well.
 *
 * [Specification](https://www.w3.org/TR/media-source-2/#dom-mediasource-activesourcebuffers)
 *
 * Returns: (transfer full): a new #GstSourceBufferList instance
 * Since: 1.24
 */
GstSourceBufferList *
gst_media_source_get_active_source_buffers (GstMediaSource * self)
{
  g_return_val_if_fail (GST_IS_MEDIA_SOURCE (self), NULL);
  return g_object_ref (self->active_buffers);
}

/**
 * gst_media_source_get_ready_state:
 * @self: #GstMediaSource instance
 *
 * Gets the current Ready State of the Media Source.
 *
 * [Specification](https://www.w3.org/TR/media-source-2/#dom-mediasource-readystate)
 *
 * Returns: the current #GstMediaSourceReadyState value
 * Since: 1.24
 */
GstMediaSourceReadyState
gst_media_source_get_ready_state (GstMediaSource * self)
{
  g_return_val_if_fail (GST_IS_MEDIA_SOURCE (self), DEFAULT_READY_STATE);
  return self->ready_state;
}

/**
 * gst_media_source_get_position:
 * @self: #GstMediaSource instance
 *
 * Gets the current playback position of the Media Source.
 *
 * Returns: the current playback position as a #GstClockTime
 * Since: 1.24
 */
GstClockTime
gst_media_source_get_position (GstMediaSource * self)
{
  g_return_val_if_fail (GST_IS_MEDIA_SOURCE (self), DEFAULT_POSITION);
  if (is_attached (self))
    return gst_mse_src_get_position (self->element);
  return DEFAULT_POSITION;
}

/**
 * gst_media_source_get_duration:
 * @self: #GstMediaSource instance
 *
 * Gets the current duration of @self.
 *
 * [Specification](https://www.w3.org/TR/media-source-2/#dom-mediasource-duration)
 *
 * Returns: the current duration as a #GstClockTime
 * Since: 1.24
 */
GstClockTime
gst_media_source_get_duration (GstMediaSource * self)
{
  g_return_val_if_fail (GST_IS_MEDIA_SOURCE (self), DEFAULT_DURATION);
  if (self->ready_state == GST_MEDIA_SOURCE_READY_STATE_CLOSED)
    return GST_CLOCK_TIME_NONE;
  return self->duration;
}

/**
 * gst_media_source_set_duration:
 * @self: #GstMediaSource instance
 * @duration: The new duration to apply to @self.
 * @error: (out) (optional) (nullable) (transfer full): the resulting error or `NULL`
 *
 * Sets the duration of @self.
 *
 * [Specification](https://www.w3.org/TR/media-source-2/#dom-mediasource-duration)
 *
 * Returns: `TRUE` on success, `FALSE` otherwise
 * Since: 1.24
 */
gboolean
gst_media_source_set_duration (GstMediaSource * self, GstClockTime duration,
    GError ** error)
{
  g_return_val_if_fail (GST_IS_MEDIA_SOURCE (self), FALSE);
  self->duration = duration;
  update_duration (self);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_DURATION]);
  return TRUE;
}

static void
on_received_init_segment (G_GNUC_UNUSED GstSourceBuffer * source_buffer,
    gpointer user_data)
{
  GstMediaSource *self = GST_MEDIA_SOURCE (user_data);
  if (!is_attached (self)) {
    GST_DEBUG_OBJECT (self, "received init segment while detached, ignoring");
    return;
  }

  GPtrArray *all_tracks = g_ptr_array_new ();

  for (guint i = 0;; i++) {
    GstSourceBuffer *buf = gst_source_buffer_list_index (self->buffers, i);
    if (buf == NULL) {
      break;
    }
    GPtrArray *tracks = gst_source_buffer_get_all_tracks (buf);
    g_ptr_array_extend (all_tracks, tracks, NULL, NULL);
    g_ptr_array_unref (tracks);
    gst_object_unref (buf);
  }

  gst_mse_src_emit_streams (self->element,
      (GstMediaSourceTrack **) all_tracks->pdata, all_tracks->len);

  g_ptr_array_unref (all_tracks);
}

static void
on_duration_changed (G_GNUC_UNUSED GstSourceBuffer * source_buffer,
    gpointer user_data)
{
  GstMediaSource *self = GST_MEDIA_SOURCE (user_data);
  GstClockTime current = self->duration;
  GstClockTime max = 0;
  for (guint i = 0;; i++) {
    GstSourceBuffer *buf = gst_source_buffer_list_index (self->buffers, i);
    if (buf == NULL) {
      break;
    }
    GstClockTime duration = gst_source_buffer_get_duration (buf);
    if (GST_CLOCK_TIME_IS_VALID (duration)) {
      max = MAX (max, duration);
    }
    gst_object_unref (buf);
  }
  if (current == max) {
    return;
  }
  GST_DEBUG_OBJECT (self, "updating %" GST_TIMEP_FORMAT "=>%" GST_TIMEP_FORMAT,
      &current, &max);
  gst_media_source_set_duration (self, max, NULL);
}

static GHashTable *
source_buffer_list_as_set (GstSourceBufferList * list)
{
  GHashTable *buffers = g_hash_table_new_full (g_direct_hash, g_direct_equal,
      gst_object_unref, NULL);
  for (guint i = 0;; i++) {
    GstSourceBuffer *buf = gst_source_buffer_list_index (list, i);
    if (buf == NULL) {
      break;
    }
    g_hash_table_add (buffers, buf);
  }
  return buffers;
}

static void
rebuild_active_source_buffers (GstMediaSource * self)
{
  // TODO: Lock the source buffer lists
  GST_DEBUG_OBJECT (self, "rebuilding active source buffers");
  GHashTable *previously_active =
      source_buffer_list_as_set (self->active_buffers);

  gst_source_buffer_list_notify_freeze (self->active_buffers);
  gst_source_buffer_list_remove_all (self->active_buffers);

  gboolean added = FALSE;
  gboolean removed = FALSE;

  for (guint i = 0;; i++) {
    GstSourceBuffer *buf = gst_source_buffer_list_index (self->buffers, i);
    if (buf == NULL) {
      break;
    }
    if (gst_source_buffer_get_active (buf)) {
      gst_source_buffer_list_append (self->active_buffers, buf);
      added |= !g_hash_table_contains (previously_active, buf);
    } else {
      gst_source_buffer_list_append (self->active_buffers, buf);
      removed |= g_hash_table_contains (previously_active, buf);
    }
    gst_object_unref (buf);
  }
  g_hash_table_unref (previously_active);

  gst_source_buffer_list_notify_cancel (self->active_buffers);
  gst_source_buffer_list_notify_thaw (self->active_buffers);

  if (added) {
    GST_DEBUG_OBJECT (self, "notifying active source buffer added");
    gst_source_buffer_list_notify_added (self->active_buffers);
  }
  if (removed) {
    GST_DEBUG_OBJECT (self, "notifying active source buffer removed");
    gst_source_buffer_list_notify_removed (self->active_buffers);
  }
}

static void
on_active_state_changed (GstSourceBuffer * source_buffer, gpointer user_data)
{
  GstMediaSource *self = GST_MEDIA_SOURCE (user_data);
  rebuild_active_source_buffers (self);
}

/**
 * gst_media_source_add_source_buffer:
 * @self: #GstMediaSource instance
 * @type: (transfer none): A MIME type describing the format of the incoming media
 * @error: (out) (optional) (nullable) (transfer full): the resulting error or `NULL`
 *
 * Add a #GstSourceBuffer to this #GstMediaSource of the specified media type.
 * The Media Source must be in the #GstMediaSourceReadyState %GST_MEDIA_SOURCE_READY_STATE_OPEN.
 *
 * [Specification](https://www.w3.org/TR/media-source-2/#dom-mediasource-addsourcebuffer)
 *
 * Returns: (transfer full): a new #GstSourceBuffer instance on success, otherwise `NULL`
 * Since: 1.24
 */
GstSourceBuffer *
gst_media_source_add_source_buffer (GstMediaSource * self, const gchar * type,
    GError ** error)
{
  g_return_val_if_fail (GST_IS_MEDIA_SOURCE (self), NULL);
  g_return_val_if_fail (type != NULL, NULL);

  if (g_strcmp0 (type, "") == 0) {
    g_set_error (error,
        GST_MEDIA_SOURCE_ERROR, GST_MEDIA_SOURCE_ERROR_TYPE,
        "supplied content type is empty");
    return NULL;
  }

  if (!gst_media_source_is_type_supported (type)) {
    g_set_error (error,
        GST_MEDIA_SOURCE_ERROR, GST_MEDIA_SOURCE_ERROR_NOT_SUPPORTED,
        "unsupported content type");
    return NULL;
  }

  if (self->ready_state != GST_MEDIA_SOURCE_READY_STATE_OPEN) {
    g_set_error (error,
        GST_MEDIA_SOURCE_ERROR, GST_MEDIA_SOURCE_ERROR_INVALID_STATE,
        "media source is not open");
    return NULL;
  }

  GstSourceBufferCallbacks callbacks = {
    .duration_changed = on_duration_changed,
    .received_init_segment = on_received_init_segment,
    .active_state_changed = on_active_state_changed,
  };

  GError *source_buffer_error = NULL;
  GstSourceBuffer *buf = gst_source_buffer_new_with_callbacks (type,
      GST_OBJECT (self), &callbacks, self, &source_buffer_error);
  if (source_buffer_error) {
    g_propagate_prefixed_error (error, source_buffer_error,
        "failed to create source buffer");
    gst_clear_object (&buf);
    return NULL;
  }

  gst_source_buffer_list_append (self->buffers, buf);

  return buf;
}

/**
 * gst_media_source_remove_source_buffer:
 * @self: #GstMediaSource instance
 * @buffer: (transfer none): #GstSourceBuffer instance
 * @error: (out) (optional) (nullable) (transfer full): the resulting error or `NULL`
 *
 * Remove @buffer from @self.
 *
 * @buffer must have been created as a child of @self and @self must be in the
 * #GstMediaSourceReadyState %GST_MEDIA_SOURCE_READY_STATE_OPEN.
 *
 * [Specification](https://www.w3.org/TR/media-source-2/#dom-mediasource-removesourcebuffer)
 *
 * Returns: `TRUE` on success, `FALSE` otherwise
 * Since: 1.24
 */
gboolean
gst_media_source_remove_source_buffer (GstMediaSource * self,
    GstSourceBuffer * buffer, GError ** error)
{
  g_return_val_if_fail (GST_IS_MEDIA_SOURCE (self), FALSE);
  g_return_val_if_fail (GST_IS_SOURCE_BUFFER (buffer), FALSE);

  if (!gst_source_buffer_list_contains (self->buffers, buffer)) {
    g_set_error (error,
        GST_MEDIA_SOURCE_ERROR, GST_MEDIA_SOURCE_ERROR_NOT_FOUND,
        "the supplied source buffer was not found in this media source");
    return FALSE;
  }

  if (gst_source_buffer_get_updating (buffer))
    gst_source_buffer_teardown (buffer);

  gst_source_buffer_list_remove (self->active_buffers, buffer);

  gst_object_unparent (GST_OBJECT (buffer));
  gst_source_buffer_list_remove (self->buffers, buffer);

  return TRUE;
}

static void
abort_all_source_buffers (GstMediaSource * self)
{
  for (guint i = 0;; i++) {
    GstSourceBuffer *buf = gst_source_buffer_list_index (self->buffers, i);
    if (buf == NULL) {
      return;
    }
    GST_LOG_OBJECT (self, "shutting down %" GST_PTR_FORMAT, buf);
    gst_source_buffer_abort (buf, NULL);
    gst_object_unref (buf);
  }
}

/**
 * gst_media_source_end_of_stream:
 * @self: #GstMediaSource instance
 * @eos_error: The error type, if any
 * @error: (out) (optional) (nullable) (transfer full): the resulting error or `NULL`
 *
 * Mark @self as reaching the end of stream, disallowing new data inputs.
 *
 * [Specification](https://www.w3.org/TR/media-source-2/#dom-mediasource-endofstream)
 *
 * Returns: `TRUE` on success, `FALSE` otherwise
 * Since: 1.24
 */
gboolean
gst_media_source_end_of_stream (GstMediaSource * self,
    GstMediaSourceEOSError eos_error, GError ** error)
{
  g_return_val_if_fail (GST_IS_MEDIA_SOURCE (self), FALSE);

  if (self->ready_state != GST_MEDIA_SOURCE_READY_STATE_OPEN) {
    g_set_error (error,
        GST_MEDIA_SOURCE_ERROR, GST_MEDIA_SOURCE_ERROR_INVALID_STATE,
        "media source is not open");
    return FALSE;
  }

  if (is_updating (self)) {
    g_set_error (error,
        GST_MEDIA_SOURCE_ERROR, GST_MEDIA_SOURCE_ERROR_INVALID_STATE,
        "some buffers are still updating");
    return FALSE;
  }

  self->ready_state = GST_MEDIA_SOURCE_READY_STATE_ENDED;
  schedule_event (self, ON_SOURCE_ENDED);

  switch (eos_error) {
    case GST_MEDIA_SOURCE_EOS_ERROR_NETWORK:
      network_error (self);
      break;
    case GST_MEDIA_SOURCE_EOS_ERROR_DECODE:
      decode_error (self);
      break;
    default:
      update_duration (self);
      abort_all_source_buffers (self);
      break;
  }

  return TRUE;
}

/**
 * gst_media_source_set_live_seekable_range:
 * @self: #GstMediaSource instance
 * @start: The earliest point in the stream considered seekable
 * @end: The latest point in the stream considered seekable
 * @error: (out) (optional) (nullable) (transfer full): the resulting error or `NULL`
 *
 * Set the live seekable range for @self. This range informs the component
 * playing this Media Source what it can allow the user to seek through.
 *
 * If the ready state is not %GST_MEDIA_SOURCE_READY_STATE_OPEN, or the supplied
 * @start time is later than @end it will fail and set an error.
 *
 * [Specification](https://www.w3.org/TR/media-source-2/#dom-mediasource-setliveseekablerange)
 *
 * Returns: `TRUE` on success, `FALSE` otherwise
 * Since: 1.24
 */
gboolean
gst_media_source_set_live_seekable_range (GstMediaSource * self,
    GstClockTime start, GstClockTime end, GError ** error)
{
  g_return_val_if_fail (GST_IS_MEDIA_SOURCE (self), FALSE);
  if (self->ready_state != GST_MEDIA_SOURCE_READY_STATE_OPEN) {
    g_set_error (error,
        GST_MEDIA_SOURCE_ERROR, GST_MEDIA_SOURCE_ERROR_INVALID_STATE,
        "media source is not open");
    return FALSE;
  }

  if (start > end) {
    g_set_error (error,
        GST_MEDIA_SOURCE_ERROR, GST_MEDIA_SOURCE_ERROR_TYPE,
        "bad time range: start must be earlier than end");
    return FALSE;
  }

  self->live_seekable_range.start = start;
  self->live_seekable_range.end = end;

  return TRUE;
}

/**
 * gst_media_source_clear_live_seekable_range:
 * @self: #GstMediaSource instance
 * @error: (out) (optional) (nullable) (transfer full): the resulting error or `NULL`
 *
 * Clear the live seekable range for @self. This will inform the component
 * playing this Media Source that there is no seekable time range.
 *
 * If the ready state is not %GST_MEDIA_SOURCE_READY_STATE_OPEN, it will fail
 * and set an error.
 *
 * [Specification](https://www.w3.org/TR/media-source-2/#dom-mediasource-clearliveseekablerange)
 *
 * Returns: `TRUE` on success, `FALSE` otherwise
 * Since: 1.24
 */
gboolean
gst_media_source_clear_live_seekable_range (GstMediaSource * self,
    GError ** error)
{
  g_return_val_if_fail (GST_IS_MEDIA_SOURCE (self), FALSE);

  if (self->ready_state != GST_MEDIA_SOURCE_READY_STATE_OPEN) {
    g_set_error (error,
        GST_MEDIA_SOURCE_ERROR, GST_MEDIA_SOURCE_ERROR_INVALID_STATE,
        "media source is not open");
    return FALSE;
  }

  reset_live_seekable_range (self);

  return TRUE;
}

/**
 * gst_media_source_get_live_seekable_range:
 * @self: #GstMediaSource instance
 * @range: (out) (transfer none): time range
 *
 * Get the live seekable range of @self. Will fill in the supplied @range with
 * the current live seekable range.
 *
 * Since: 1.24
 */
void
gst_media_source_get_live_seekable_range (GstMediaSource * self,
    GstMediaSourceRange * range)
{
  g_return_if_fail (GST_IS_MEDIA_SOURCE (self));
  g_return_if_fail (range != NULL);

  range->start = self->live_seekable_range.start;
  range->end = self->live_seekable_range.end;
}

void
gst_media_source_seek (GstMediaSource * self, GstClockTime time)
{
  g_return_if_fail (GST_IS_MEDIA_SOURCE (self));
  for (guint i = 0;; i++) {
    GstSourceBuffer *buf = gst_source_buffer_list_index (self->buffers, i);
    if (buf == NULL) {
      return;
    }
    gst_source_buffer_seek (buf, time);
    gst_object_unref (buf);
  }
}
