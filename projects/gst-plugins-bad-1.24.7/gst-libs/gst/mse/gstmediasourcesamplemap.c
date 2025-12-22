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

#include "gstmediasourcesamplemap-private.h"
#include "gstmselogging-private.h"

struct _GstMediaSourceSampleMap
{
  GstObject parent_instance;

  GHashTable *samples;
  GSequence *samples_by_dts;
  GSequence *samples_by_pts;

  gsize storage_size;
};

G_DEFINE_TYPE (GstMediaSourceSampleMap, gst_media_source_sample_map,
    GST_TYPE_OBJECT);

static gint compare_pts (GstSample * a, GstSample * b, gpointer user_data);
static GSequenceIter *find_sample_containing_dts (GstMediaSourceSampleMap *
    self, GstClockTime dts);
static GSequenceIter *find_sample_containing_pts (GstMediaSourceSampleMap *
    self, GstClockTime pts);
static GSequenceIter *find_sequentially (GSequence * sequence, gpointer item);

static inline GstClockTime
sample_dts (GstSample * sample)
{
  return GST_BUFFER_DTS (gst_sample_get_buffer (sample));
}

static inline GstClockTime
sample_pts (GstSample * sample)
{
  return GST_BUFFER_PTS (gst_sample_get_buffer (sample));
}

static gsize
sample_buffer_size (GstSample * sample)
{
  return gst_buffer_get_size (gst_sample_get_buffer (sample));
}

static gboolean
iter_is_delta_unit (GSequenceIter * iter)
{
  if (g_sequence_iter_is_end (iter)) {
    return TRUE;
  }
  GstSample *sample = g_sequence_get (iter);
  GstBuffer *buffer = gst_sample_get_buffer (sample);
  return GST_BUFFER_FLAG_IS_SET (buffer, GST_BUFFER_FLAG_DELTA_UNIT);
}

static gint
compare_dts (GstSample * a, GstSample * b, gpointer user_data)
{
  GstClockTime a_dts = sample_dts (a);
  GstClockTime b_dts = sample_dts (b);
  if (a_dts == b_dts) {
    return compare_pts (a, b, user_data);
  }
  return a_dts > b_dts ? +1 : -1;
}

static gint
compare_pts (GstSample * a, GstSample * b, gpointer user_data)
{
  GstClockTime a_pts = sample_pts (a);
  GstClockTime b_pts = sample_pts (b);
  if (a_pts == b_pts) {
    return 0;
  }
  return a_pts > b_pts ? +1 : -1;
}

static inline void
remove_sample (GSequence * sequence, GCompareDataFunc compare,
    GstSample * sample, GstMediaSourceSampleMap * self)
{
  GSequenceIter *match = g_sequence_lookup (sequence, sample, compare, self);
  if (match == NULL) {
    return;
  }
  g_sequence_remove (match);
}

GstMediaSourceSampleMap *
gst_media_source_sample_map_new (void)
{
  gst_mse_init_logging ();
  return gst_object_ref_sink (g_object_new (GST_TYPE_MEDIA_SOURCE_SAMPLE_MAP,
          NULL));
}

static void
gst_media_source_sample_map_finalize (GObject * object)
{
  GstMediaSourceSampleMap *self = GST_MEDIA_SOURCE_SAMPLE_MAP (object);
  g_sequence_free (self->samples_by_dts);
  g_sequence_free (self->samples_by_pts);
  g_hash_table_unref (self->samples);
  G_OBJECT_CLASS (gst_media_source_sample_map_parent_class)->finalize (object);
}

static void
gst_media_source_sample_map_class_init (GstMediaSourceSampleMapClass * klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);

  oclass->finalize = GST_DEBUG_FUNCPTR (gst_media_source_sample_map_finalize);
}

static void
gst_media_source_sample_map_init (GstMediaSourceSampleMap * self)
{
  self->samples = g_hash_table_new_full (g_direct_hash, g_direct_equal,
      (GDestroyNotify) gst_sample_unref, NULL);
  self->samples_by_dts = g_sequence_new ((GDestroyNotify) gst_sample_unref);
  self->samples_by_pts = g_sequence_new ((GDestroyNotify) gst_sample_unref);
}

void
gst_media_source_sample_map_add (GstMediaSourceSampleMap * self,
    GstSample * sample)
{
  g_return_if_fail (GST_IS_MEDIA_SOURCE_SAMPLE_MAP (self));
  g_return_if_fail (GST_IS_SAMPLE (sample));
  GstBuffer *buffer = gst_sample_get_buffer (sample);
  g_return_if_fail (GST_BUFFER_DTS_IS_VALID (buffer));
  g_return_if_fail (GST_BUFFER_PTS_IS_VALID (buffer));
  g_return_if_fail (GST_BUFFER_DURATION_IS_VALID (buffer));

  if (g_hash_table_contains (self->samples, sample)) {
    return;
  }

  g_hash_table_add (self->samples, gst_sample_ref (sample));
  g_sequence_insert_sorted (self->samples_by_dts, gst_sample_ref (sample),
      (GCompareDataFunc) compare_dts, self);
  g_sequence_insert_sorted (self->samples_by_pts, gst_sample_ref (sample),
      (GCompareDataFunc) compare_pts, self);
  self->storage_size += gst_buffer_get_size (buffer);
  GST_TRACE_OBJECT (self, "new storage size=%" G_GSIZE_FORMAT,
      self->storage_size);
}

void
gst_media_source_sample_map_remove (GstMediaSourceSampleMap * self,
    GstSample * sample)
{
  g_return_if_fail (GST_IS_MEDIA_SOURCE_SAMPLE_MAP (self));
  g_return_if_fail (GST_IS_SAMPLE (sample));

  if (!g_hash_table_contains (self->samples, sample)) {
    return;
  }

  gsize buffer_size = sample_buffer_size (sample);
  remove_sample (self->samples_by_dts, (GCompareDataFunc) compare_dts, sample,
      self);
  remove_sample (self->samples_by_pts, (GCompareDataFunc) compare_pts, sample,
      self);
  g_hash_table_remove (self->samples, sample);
  self->storage_size -= MIN (buffer_size, self->storage_size);
}

gboolean
gst_media_source_sample_map_contains (GstMediaSourceSampleMap * self,
    GstSample * sample)
{
  g_return_val_if_fail (GST_IS_MEDIA_SOURCE_SAMPLE_MAP (self), FALSE);
  return g_hash_table_contains (self->samples, sample);
}

gsize
gst_media_source_sample_map_remove_range (GstMediaSourceSampleMap * self,
    GstClockTime earliest, GstClockTime latest)
{
  g_return_val_if_fail (GST_IS_MEDIA_SOURCE_SAMPLE_MAP (self), 0);
  g_return_val_if_fail (earliest <= latest, 0);

  GSequenceIter *start_by_dts = find_sample_containing_dts (self, earliest);
  GSequenceIter *end_by_dts = find_sample_containing_dts (self, latest);

  GstSample *start = g_sequence_get (start_by_dts);
  GstSample *end = g_sequence_get (end_by_dts);

  GstClockTime start_time =
      start == NULL ? GST_CLOCK_TIME_NONE : sample_dts (start);
  GstClockTime end_time = end == NULL ? start_time : sample_dts (end);

  GST_TRACE_OBJECT (self, "remove range [%" GST_TIMEP_FORMAT ",%"
      GST_TIMEP_FORMAT ")", &start_time, &end_time);

  GList *to_remove = NULL;
  while (g_sequence_iter_compare (start_by_dts, end_by_dts) < 1) {
    GstSample *sample = g_sequence_get (start_by_dts);
    to_remove = g_list_prepend (to_remove, sample);
    start_by_dts = g_sequence_iter_next (start_by_dts);
  }
  gsize bytes_removed = 0;
  for (GList * iter = to_remove; iter != NULL; iter = g_list_next (iter)) {
    GstSample *sample = iter->data;
    bytes_removed += sample_buffer_size (sample);
    gst_media_source_sample_map_remove (self, sample);
  }

  g_list_free (to_remove);

  GST_TRACE_OBJECT (self, "removed=%" G_GSIZE_FORMAT "B, latest=%"
      GST_TIMEP_FORMAT, bytes_removed, &latest);
  return bytes_removed;
}

gsize
gst_media_source_sample_map_remove_range_from_start (GstMediaSourceSampleMap
    * self, GstClockTime latest_dts)
{
  return gst_media_source_sample_map_remove_range (self, 0, latest_dts);
}

gsize
gst_media_source_sample_map_remove_range_from_end (GstMediaSourceSampleMap
    * self, GstClockTime earliest_dts)
{
  return gst_media_source_sample_map_remove_range (self, earliest_dts,
      GST_CLOCK_TIME_NONE);
}

GstClockTime
gst_media_source_sample_map_get_highest_end_time (GstMediaSourceSampleMap *
    self)
{
  g_return_val_if_fail (GST_IS_MEDIA_SOURCE_SAMPLE_MAP (self),
      GST_CLOCK_TIME_NONE);
  GSequenceIter *iter = g_sequence_get_end_iter (self->samples_by_pts);
  iter = g_sequence_iter_prev (iter);
  if (g_sequence_iter_is_begin (iter)) {
    return GST_CLOCK_TIME_NONE;
  }
  GstSample *sample = g_sequence_get (iter);
  GstBuffer *buffer = gst_sample_get_buffer (sample);
  g_return_val_if_fail (GST_BUFFER_PTS_IS_VALID (buffer), GST_CLOCK_TIME_NONE);
  g_return_val_if_fail (GST_BUFFER_DURATION_IS_VALID (buffer),
      GST_CLOCK_TIME_NONE);
  return GST_BUFFER_PTS (buffer) + GST_BUFFER_DURATION (buffer);
}

guint
gst_media_source_sample_map_get_size (GstMediaSourceSampleMap * self)
{
  g_return_val_if_fail (GST_IS_MEDIA_SOURCE_SAMPLE_MAP (self), 0);
  return g_hash_table_size (self->samples);
}

gsize
gst_media_source_sample_map_get_storage_size (GstMediaSourceSampleMap * self)
{
  g_return_val_if_fail (GST_IS_MEDIA_SOURCE_SAMPLE_MAP (self), 0);
  return self->storage_size;
}

static inline gboolean
sample_contains_dts (GstSample * sample, GstClockTime dts)
{
  GstBuffer *buffer = gst_sample_get_buffer (sample);
  g_return_val_if_fail (GST_BUFFER_DURATION_IS_VALID (buffer), FALSE);
  g_return_val_if_fail (GST_BUFFER_DTS_IS_VALID (buffer), FALSE);
  g_return_val_if_fail (GST_CLOCK_TIME_IS_VALID (dts), FALSE);
  GstClockTime end = GST_BUFFER_DTS (buffer) + GST_BUFFER_DURATION (buffer);
  return dts <= end;
}

static inline gboolean
sample_contains_pts (GstSample * sample, GstClockTime pts)
{
  GstBuffer *buffer = gst_sample_get_buffer (sample);
  g_return_val_if_fail (GST_BUFFER_DURATION_IS_VALID (buffer), FALSE);
  g_return_val_if_fail (GST_BUFFER_PTS_IS_VALID (buffer), FALSE);
  g_return_val_if_fail (GST_CLOCK_TIME_IS_VALID (pts), FALSE);
  GstClockTime end = GST_BUFFER_PTS (buffer) + GST_BUFFER_DURATION (buffer);
  return pts <= end;
}

static GSequenceIter *
find_sequentially (GSequence * sequence, gpointer item)
{
  GSequenceIter *iter = g_sequence_get_begin_iter (sequence);
  for (; !g_sequence_iter_is_end (iter); iter = g_sequence_iter_next (iter)) {
    gpointer current = g_sequence_get (iter);
    if (current == item) {
      return iter;
    }
  }
  return iter;
}

static GSequenceIter *
find_sample_containing_dts (GstMediaSourceSampleMap * self, GstClockTime dts)
{
  if (dts == 0) {
    return g_sequence_get_begin_iter (self->samples_by_dts);
  }
  if (dts == GST_CLOCK_TIME_NONE) {
    return g_sequence_get_end_iter (self->samples_by_dts);
  }
  GSequenceIter *iter = g_sequence_get_begin_iter (self->samples_by_dts);
  while (!g_sequence_iter_is_end (iter)) {
    GstSample *sample = g_sequence_get (iter);
    if (sample_contains_dts (sample, dts)) {
      return iter;
    }
    iter = g_sequence_iter_next (iter);
  }
  return iter;
}

static GSequenceIter *
find_sample_containing_pts (GstMediaSourceSampleMap * self, GstClockTime pts)
{
  if (pts == 0) {
    return g_sequence_get_begin_iter (self->samples_by_pts);
  }
  if (pts == GST_CLOCK_TIME_NONE) {
    return g_sequence_get_end_iter (self->samples_by_pts);
  }
  GSequenceIter *iter = g_sequence_get_begin_iter (self->samples_by_pts);
  while (!g_sequence_iter_is_end (iter)) {
    GstSample *sample = g_sequence_get (iter);
    if (sample_contains_pts (sample, pts)) {
      return iter;
    }
    iter = g_sequence_iter_next (iter);
  }
  return iter;
}

static GSequenceIter *
find_previous_non_delta_unit (GstMediaSourceSampleMap * self,
    GSequenceIter * iter)
{
  while (!g_sequence_iter_is_begin (iter)) {
    if (!iter_is_delta_unit (iter)) {
      GST_TRACE_OBJECT (self, "found valid sample");
      return iter;
    }
    iter = g_sequence_iter_prev (iter);
  }
  GST_TRACE_OBJECT (self, "rolled back to the first sample");
  return iter;
}

static GSequenceIter *
gst_media_source_sample_map_iter_starting_dts (GstMediaSourceSampleMap * self,
    GstClockTime start_dts)
{
  g_return_val_if_fail (GST_IS_MEDIA_SOURCE_SAMPLE_MAP (self), NULL);
  g_return_val_if_fail (GST_CLOCK_TIME_IS_VALID (start_dts), NULL);
  GSequenceIter *iter = find_sample_containing_dts (self, start_dts);
  return find_previous_non_delta_unit (self, iter);
}

static GSequenceIter *
gst_media_source_sample_map_iter_starting_pts (GstMediaSourceSampleMap
    * self, GstClockTime start_pts)
{
  g_return_val_if_fail (GST_IS_MEDIA_SOURCE_SAMPLE_MAP (self), NULL);
  g_return_val_if_fail (GST_CLOCK_TIME_IS_VALID (start_pts), NULL);
  GSequenceIter *iter = find_sample_containing_pts (self, start_pts);
  return find_previous_non_delta_unit (self, iter);
}

typedef struct _SampleMapIterator SampleMapIterator;

struct _SampleMapIterator
{
  GstIterator iterator;
  GstMediaSourceSampleMap *map;
/* *INDENT-OFF* */
  GstClockTime   (*timestamp_func)      (GstSample *);
  GSequenceIter *(*resync_locator_func) (SampleMapIterator *);
/* *INDENT-ON* */

  GstClockTime start_time;
  GstClockTime current_time;
  GSequenceIter *current_iter;
  GstSample *current_sample;
};

static void
iter_copy (const SampleMapIterator * it, SampleMapIterator * copy)
{
  copy->map = gst_object_ref (it->map);
  copy->timestamp_func = it->timestamp_func;
  copy->resync_locator_func = it->resync_locator_func;

  copy->current_time = it->current_time;
  copy->start_time = it->start_time;
  copy->current_iter = it->current_iter;
  copy->current_sample = gst_sample_ref (it->current_sample);
}

static GSequenceIter *
iter_find_resync_point_dts (SampleMapIterator * it)
{
  if (it->current_sample) {
    GSequenceIter *iter =
        find_sequentially (it->map->samples_by_dts, it->current_sample);
    if (!g_sequence_iter_is_end (iter)) {
      return g_sequence_iter_next (iter);
    }
  }

  return gst_media_source_sample_map_iter_starting_dts (it->map,
      it->current_time);
}

static GSequenceIter *
iter_find_resync_point_pts (SampleMapIterator * it)
{
  if (it->current_sample) {
    GSequenceIter *iter =
        find_sequentially (it->map->samples_by_pts, it->current_sample);
    if (!g_sequence_iter_is_end (iter)) {
      return g_sequence_iter_next (iter);
    }
  }

  return gst_media_source_sample_map_iter_starting_pts (it->map,
      it->current_time);
}

static GstIteratorResult
iter_next (SampleMapIterator * it, GValue * result)
{

  if (g_sequence_iter_is_end (it->current_iter)) {
    return GST_ITERATOR_DONE;
  }

  GstSample *sample = g_sequence_get (it->current_iter);
  gst_clear_sample (&it->current_sample);
  it->current_sample = gst_sample_ref (sample);

  it->current_time = it->timestamp_func (sample);
  it->current_iter = g_sequence_iter_next (it->current_iter);

  gst_value_set_sample (result, sample);

  return GST_ITERATOR_OK;
}

static void
iter_resync (SampleMapIterator * it)
{
  GST_TRACE_OBJECT (it->map, "resync");
  it->current_time = it->start_time;
  it->current_iter = it->resync_locator_func (it);
}

static void
iter_free (SampleMapIterator * it)
{
  gst_clear_object (&it->map);
  gst_clear_sample (&it->current_sample);
}

GstIterator *
gst_media_source_sample_map_iter_samples_by_dts (GstMediaSourceSampleMap * map,
    GMutex * lock, guint32 * master_cookie, GstClockTime start_dts,
    GstSample * start_sample)
{
/* *INDENT-OFF* */
  SampleMapIterator *it = (SampleMapIterator *) gst_iterator_new (
      sizeof (SampleMapIterator),
      GST_TYPE_SAMPLE,
      lock,
      master_cookie,
      (GstIteratorCopyFunction) iter_copy,
      (GstIteratorNextFunction) iter_next,
      (GstIteratorItemFunction) NULL,
      (GstIteratorResyncFunction) iter_resync,
      (GstIteratorFreeFunction) iter_free
  );
/* *INDENT-ON* */

  it->map = gst_object_ref (map);
  it->timestamp_func = sample_dts;
  it->resync_locator_func = iter_find_resync_point_dts;
  it->start_time = start_dts;
  it->current_time = start_dts;
  it->current_sample = start_sample ? gst_sample_ref (start_sample) : NULL;
  it->current_iter = iter_find_resync_point_dts (it);

  return GST_ITERATOR (it);
}

GstIterator *
gst_media_source_sample_map_iter_samples_by_pts (GstMediaSourceSampleMap * map,
    GMutex * lock, guint32 * master_cookie, GstClockTime start_pts,
    GstSample * start_sample)
{
/* *INDENT-OFF* */
  SampleMapIterator *it = (SampleMapIterator *) gst_iterator_new (
      sizeof (SampleMapIterator),
      GST_TYPE_SAMPLE,
      lock,
      master_cookie,
      (GstIteratorCopyFunction) iter_copy,
      (GstIteratorNextFunction) iter_next,
      (GstIteratorItemFunction) NULL,
      (GstIteratorResyncFunction) iter_resync,
      (GstIteratorFreeFunction) iter_free
  );
/* *INDENT-ON* */

  it->map = gst_object_ref (map);
  it->timestamp_func = sample_pts;
  it->resync_locator_func = iter_find_resync_point_pts;
  it->start_time = start_pts;
  it->current_time = start_pts;
  it->current_sample = start_sample ? gst_sample_ref (start_sample) : NULL;
  it->current_iter = iter_find_resync_point_pts (it);

  return GST_ITERATOR (it);
}
