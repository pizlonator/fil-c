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

#include "gstmediasourcetrackbuffer-private.h"
#include "gstmediasourcesamplemap-private.h"
#include "gstmediasource.h"
#include "gstmselogging-private.h"

typedef struct
{
  gboolean enabled;
  GstClockTime group_start;
  GstClockTime group_end;
  GstClockTimeDiff offset;

  GstClockTime last_dts;
  GstClockTime last_duration;
} Timestamps;

struct _GstMediaSourceTrackBuffer
{
  GstObject parent_instance;

  GstMediaSourceSampleMap *samples;
  Timestamps timestamps;
  guint eos;

  guint32 master_cookie;

  GCond new_data_cond;
  GMutex new_data_mutex;
};

#define g_array_new_ranges() \
  (g_array_new (TRUE, FALSE, sizeof (GstMediaSourceRange)))

#define NEW_DATA_LOCK(a) (g_mutex_lock (&a->new_data_mutex))
#define NEW_DATA_UNLOCK(a) (g_mutex_unlock (&a->new_data_mutex))
#define NEW_DATA_SIGNAL(a) (g_cond_signal (&a->new_data_cond))
#define NEW_DATA_WAIT(a) (g_cond_wait (&a->new_data_cond, &a->new_data_mutex))
#define NEW_DATA_WAIT_UNTIL(a, d) \
    (g_cond_wait_until (&a->new_data_cond, &a->new_data_mutex, d))

static void timestamps_init (Timestamps * self, gboolean enabled);
static void timestamps_process (Timestamps * self, GstSample * sample);

G_DEFINE_TYPE (GstMediaSourceTrackBuffer, gst_media_source_track_buffer,
    GST_TYPE_OBJECT);

static void
invalidate_cookie (GstMediaSourceTrackBuffer * self)
{
  self->master_cookie++;
}

GstMediaSourceTrackBuffer *
gst_media_source_track_buffer_new (void)
{
  return gst_object_ref_sink (g_object_new (GST_TYPE_MEDIA_SOURCE_TRACK_BUFFER,
          NULL));
}

static void
gst_media_source_track_buffer_finalize (GObject * object)
{
  GstMediaSourceTrackBuffer *self = GST_MEDIA_SOURCE_TRACK_BUFFER (object);
  gst_object_unref (self->samples);
  g_cond_clear (&self->new_data_cond);
  g_mutex_clear (&self->new_data_mutex);
  G_OBJECT_CLASS (gst_media_source_track_buffer_parent_class)->finalize
      (object);
}

static void
gst_media_source_track_buffer_class_init (GstMediaSourceTrackBufferClass *
    klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);

  oclass->finalize = GST_DEBUG_FUNCPTR (gst_media_source_track_buffer_finalize);
}

static void
gst_media_source_track_buffer_init (GstMediaSourceTrackBuffer * self)
{
  self->samples = gst_media_source_sample_map_new ();
  self->eos = FALSE;
  self->master_cookie = 0;
  timestamps_init (&self->timestamps, FALSE);
  g_cond_init (&self->new_data_cond);
  g_mutex_init (&self->new_data_mutex);
}

void
gst_media_source_track_buffer_process_init_segment (GstMediaSourceTrackBuffer
    * self, gboolean sequence_mode)
{
  NEW_DATA_LOCK (self);

  timestamps_init (&self->timestamps, sequence_mode);

  NEW_DATA_UNLOCK (self);
}

void
gst_media_source_track_buffer_set_group_start (GstMediaSourceTrackBuffer
    * self, GstClockTime group_start)
{
  g_return_if_fail (GST_IS_MEDIA_SOURCE_TRACK_BUFFER (self));
  if (self->timestamps.enabled) {
    self->timestamps.group_start = group_start;
  }
}

void
gst_media_source_track_buffer_add (GstMediaSourceTrackBuffer * self,
    GstSample * sample)
{
  g_return_if_fail (GST_IS_MEDIA_SOURCE_TRACK_BUFFER (self));
  g_return_if_fail (GST_IS_SAMPLE (sample));

  NEW_DATA_LOCK (self);

  timestamps_process (&self->timestamps, sample);
  gst_media_source_sample_map_add (self->samples, sample);
  invalidate_cookie (self);

  NEW_DATA_SIGNAL (self);
  NEW_DATA_UNLOCK (self);
}

void
gst_media_source_track_buffer_remove (GstMediaSourceTrackBuffer * self,
    GstSample * sample)
{
  g_return_if_fail (GST_IS_MEDIA_SOURCE_TRACK_BUFFER (self));
  g_return_if_fail (GST_IS_SAMPLE (sample));

  NEW_DATA_LOCK (self);

  gst_media_source_sample_map_remove (self->samples, sample);
  invalidate_cookie (self);

  NEW_DATA_SIGNAL (self);
  NEW_DATA_UNLOCK (self);
}

gsize
gst_media_source_track_buffer_remove_range (GstMediaSourceTrackBuffer * self,
    GstClockTime earliest, GstClockTime latest)
{
  NEW_DATA_LOCK (self);
  gsize size = gst_media_source_sample_map_remove_range (self->samples,
      earliest, latest);
  invalidate_cookie (self);
  NEW_DATA_SIGNAL (self);
  NEW_DATA_UNLOCK (self);
  return size;
}

void
gst_media_source_track_buffer_clear (GstMediaSourceTrackBuffer * self)
{
  g_return_if_fail (GST_IS_MEDIA_SOURCE_TRACK_BUFFER (self));

  NEW_DATA_LOCK (self);

  g_set_object (&self->samples, gst_media_source_sample_map_new ());

  NEW_DATA_SIGNAL (self);
  NEW_DATA_UNLOCK (self);
}

static gboolean
is_eos (GstMediaSourceTrackBuffer * self)
{
  return g_atomic_int_get (&self->eos);
}

void
gst_media_source_track_buffer_eos (GstMediaSourceTrackBuffer * self)
{
  g_return_if_fail (GST_IS_MEDIA_SOURCE_TRACK_BUFFER (self));
  NEW_DATA_LOCK (self);
  g_atomic_int_set (&self->eos, TRUE);
  NEW_DATA_SIGNAL (self);
  NEW_DATA_UNLOCK (self);
}

gboolean
gst_media_source_track_buffer_is_eos (GstMediaSourceTrackBuffer * self)
{
  g_return_val_if_fail (GST_IS_MEDIA_SOURCE_TRACK_BUFFER (self), FALSE);
  return is_eos (self);
}

gboolean
gst_media_source_track_buffer_await_eos_until (GstMediaSourceTrackBuffer * self,
    gint64 deadline)
{
  NEW_DATA_LOCK (self);
  while (!is_eos (self) && NEW_DATA_WAIT_UNTIL (self, deadline)) {
    /* wait */
  }
  NEW_DATA_UNLOCK (self);
  return is_eos (self);
}

gint
gst_media_source_track_buffer_get_size (GstMediaSourceTrackBuffer * self)
{
  g_return_val_if_fail (GST_IS_MEDIA_SOURCE_TRACK_BUFFER (self), 0);
  return gst_media_source_sample_map_get_size (self->samples);
}

GstClockTime
gst_media_source_track_buffer_get_highest_end_time (GstMediaSourceTrackBuffer
    * self)
{
  g_return_val_if_fail (GST_IS_MEDIA_SOURCE_TRACK_BUFFER (self),
      GST_CLOCK_TIME_NONE);
  return gst_media_source_sample_map_get_highest_end_time (self->samples);
}

typedef struct
{
  GArray *ranges;
  GstMediaSourceRange current_range;
} GetRangesAccumulator;

static gboolean
get_ranges_fold (const GValue * item, GetRangesAccumulator * acc,
    gpointer user_data)
{
  GstSample *sample = gst_value_get_sample (item);
  GstBuffer *buffer = gst_sample_get_buffer (sample);
  GstClockTime start = GST_BUFFER_PTS (buffer);
  GstClockTime end = start + GST_BUFFER_DURATION (buffer);

  GstMediaSourceRange *range = &acc->current_range;

  if (range->end == 0 || start <= (range->end + (GST_SECOND / 100))) {
    range->end = end;
    return TRUE;
  }
  g_array_append_val (acc->ranges, *range);

  range->start = start;
  range->end = end;

  return TRUE;
}

GArray *
gst_media_source_track_buffer_get_ranges (GstMediaSourceTrackBuffer * self)
{
  GetRangesAccumulator acc = {
    .ranges = g_array_new_ranges (),
    .current_range = {.start = 0,.end = 0},
  };

  /* *INDENT-OFF* */
  GstIterator *iter = gst_media_source_sample_map_iter_samples_by_pts (
      self->samples,
      &self->new_data_mutex,
      &self->master_cookie,
      0,
      NULL
  );
  /* *INDENT-ON* */
  while (gst_iterator_fold (iter, (GstIteratorFoldFunction) get_ranges_fold,
          (GValue *) & acc, NULL) == GST_ITERATOR_RESYNC) {
    gst_iterator_resync (iter);
  }
  gst_iterator_free (iter);

  if (acc.current_range.end > 0) {
    g_array_append_val (acc.ranges, acc.current_range);
  }

  return acc.ranges;
}

static void
timestamps_init (Timestamps * self, gboolean enabled)
{
  self->enabled = enabled;
  self->group_start = GST_CLOCK_TIME_NONE;
  self->group_end = GST_CLOCK_TIME_NONE;
  self->offset = 0;
  self->last_dts = 0;
  self->last_duration = 0;
}

static void
timestamps_process (Timestamps * self, GstSample * sample)
{
  if (!self->enabled) {
    return;
  }

  GstBuffer *buffer = gst_sample_get_buffer (sample);
  GstClockTime duration = GST_BUFFER_DURATION (buffer);

  GstClockTime pts = 0;
  GstClockTime dts = 0;

  if (GST_CLOCK_TIME_IS_VALID (self->group_start)) {
    self->offset = self->group_start - pts;
    self->group_end = self->group_start;
    self->group_start = GST_CLOCK_TIME_NONE;
  }

  if (self->offset != 0) {
    pts += self->offset;
    dts += self->offset;
  }

  GstClockTime end_pts = pts + duration;

  self->last_dts = dts;
  self->last_duration = duration;

  if (GST_CLOCK_TIME_IS_VALID (self->group_end)) {
    self->group_end = MAX (self->group_end, end_pts);
  }
  self->offset = end_pts;

  GST_BUFFER_PTS (buffer) = pts;
  GST_BUFFER_DTS (buffer) = dts;
}

gsize
gst_media_source_track_buffer_get_storage_size (GstMediaSourceTrackBuffer *
    self)
{
  g_return_val_if_fail (GST_IS_MEDIA_SOURCE_TRACK_BUFFER (self), 0);
  return gst_media_source_sample_map_get_storage_size (self->samples);
}

GstIterator *
gst_media_source_track_buffer_iter_samples (GstMediaSourceTrackBuffer * self,
    GstClockTime start_dts, GstSample * start_sample)
{
  /* *INDENT-OFF* */
  return gst_media_source_sample_map_iter_samples_by_dts (
      self->samples,
      &self->new_data_mutex,
      &self->master_cookie,
      start_dts,
      start_sample
  );
  /* *INDENT-ON* */
}
