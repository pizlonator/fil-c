/* GStreamer
 * Copyright (C) 2018 Collabora Ltd
 *   @author George Kiagiadakis <george.kiagiadakis@collabora.com>
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
 * SECTION:gstplanaraudioadapter
 * @title: GstPlanarAudioAdapter
 * @short_description: adapts incoming audio data on a sink pad into chunks of N samples
 *
 * This class is similar to GstAdapter, but it is made to work with
 * non-interleaved (planar) audio buffers. Before using, an audio format
 * must be configured with gst_planar_audio_adapter_configure()
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstplanaraudioadapter.h"

GST_DEBUG_CATEGORY_STATIC (gst_planar_audio_adapter_debug);
#define GST_CAT_DEFAULT gst_planar_audio_adapter_debug

struct _GstPlanarAudioAdapter
{
  GObject object;

  GstAudioInfo info;
  GSList *buflist;
  GSList *buflist_end;
  gsize samples;
  gsize skip;
  guint count;

  GstClockTime pts;
  guint64 pts_distance;
  GstClockTime dts;
  guint64 dts_distance;
  guint64 offset;
  guint64 offset_distance;

  GstClockTime pts_at_discont;
  GstClockTime dts_at_discont;
  guint64 offset_at_discont;

  guint64 distance_from_discont;
};

struct _GstPlanarAudioAdapterClass
{
  GObjectClass parent_class;
};

#define _do_init \
  GST_DEBUG_CATEGORY_INIT (gst_planar_audio_adapter_debug, "planaraudioadapter", \
      0, "object to splice and merge audio buffers to desired size")
#define gst_planar_audio_adapter_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstPlanarAudioAdapter, gst_planar_audio_adapter,
    G_TYPE_OBJECT, _do_init);

static void gst_planar_audio_adapter_dispose (GObject * object);

static void
gst_planar_audio_adapter_class_init (GstPlanarAudioAdapterClass * klass)
{
  GObjectClass *object = G_OBJECT_CLASS (klass);

  object->dispose = gst_planar_audio_adapter_dispose;
}

static void
gst_planar_audio_adapter_init (GstPlanarAudioAdapter * adapter)
{
  adapter->pts = GST_CLOCK_TIME_NONE;
  adapter->pts_distance = 0;
  adapter->dts = GST_CLOCK_TIME_NONE;
  adapter->dts_distance = 0;
  adapter->offset = GST_BUFFER_OFFSET_NONE;
  adapter->offset_distance = 0;
  adapter->pts_at_discont = GST_CLOCK_TIME_NONE;
  adapter->dts_at_discont = GST_CLOCK_TIME_NONE;
  adapter->offset_at_discont = GST_BUFFER_OFFSET_NONE;
  adapter->distance_from_discont = 0;
}

static void
gst_planar_audio_adapter_dispose (GObject * object)
{
  GstPlanarAudioAdapter *adapter = GST_PLANAR_AUDIO_ADAPTER (object);

  gst_planar_audio_adapter_clear (adapter);

  GST_CALL_PARENT (G_OBJECT_CLASS, dispose, (object));
}

/**
 * gst_planar_audio_adapter_new:
 *
 * Creates a new #GstPlanarAudioAdapter. Free with g_object_unref().
 *
 * Returns: (transfer full): a new #GstPlanarAudioAdapter
 */
GstPlanarAudioAdapter *
gst_planar_audio_adapter_new (void)
{
  return g_object_new (GST_TYPE_PLANAR_AUDIO_ADAPTER, NULL);
}

/**
 * gst_planar_audio_adapter_configure:
 * @adapter: a #GstPlanarAudioAdapter
 * @info: a #GstAudioInfo describing the format of the audio data
 *
 * Sets up the @adapter to handle audio data of the specified audio format.
 * Note that this will internally clear the adapter and re-initialize it.
 */
void
gst_planar_audio_adapter_configure (GstPlanarAudioAdapter * adapter,
    const GstAudioInfo * info)
{
  g_return_if_fail (GST_IS_PLANAR_AUDIO_ADAPTER (adapter));
  g_return_if_fail (info != NULL);
  g_return_if_fail (GST_AUDIO_INFO_IS_VALID (info));
  g_return_if_fail (info->layout == GST_AUDIO_LAYOUT_NON_INTERLEAVED);

  gst_planar_audio_adapter_clear (adapter);
  adapter->info = *info;
}

/**
 * gst_planar_audio_adapter_clear:
 * @adapter: a #GstPlanarAudioAdapter
 *
 * Removes all buffers from @adapter.
 */
void
gst_planar_audio_adapter_clear (GstPlanarAudioAdapter * adapter)
{
  g_return_if_fail (GST_IS_PLANAR_AUDIO_ADAPTER (adapter));

  g_slist_foreach (adapter->buflist, (GFunc) gst_mini_object_unref, NULL);
  g_slist_free (adapter->buflist);
  adapter->buflist = NULL;
  adapter->buflist_end = NULL;
  adapter->count = 0;
  adapter->samples = 0;
  adapter->skip = 0;

  adapter->pts = GST_CLOCK_TIME_NONE;
  adapter->pts_distance = 0;
  adapter->dts = GST_CLOCK_TIME_NONE;
  adapter->dts_distance = 0;
  adapter->offset = GST_BUFFER_OFFSET_NONE;
  adapter->offset_distance = 0;
  adapter->pts_at_discont = GST_CLOCK_TIME_NONE;
  adapter->dts_at_discont = GST_CLOCK_TIME_NONE;
  adapter->offset_at_discont = GST_BUFFER_OFFSET_NONE;
  adapter->distance_from_discont = 0;
}

static inline void
update_timestamps_and_offset (GstPlanarAudioAdapter * adapter, GstBuffer * buf)
{
  GstClockTime pts, dts;
  guint64 offset;

  pts = GST_BUFFER_PTS (buf);
  if (GST_CLOCK_TIME_IS_VALID (pts)) {
    GST_LOG_OBJECT (adapter, "new pts %" GST_TIME_FORMAT, GST_TIME_ARGS (pts));
    adapter->pts = pts;
    adapter->pts_distance = 0;
  }
  dts = GST_BUFFER_DTS (buf);
  if (GST_CLOCK_TIME_IS_VALID (dts)) {
    GST_LOG_OBJECT (adapter, "new dts %" GST_TIME_FORMAT, GST_TIME_ARGS (dts));
    adapter->dts = dts;
    adapter->dts_distance = 0;
  }
  offset = GST_BUFFER_OFFSET (buf);
  if (offset != GST_BUFFER_OFFSET_NONE) {
    GST_LOG_OBJECT (adapter, "new offset %" G_GUINT64_FORMAT, offset);
    adapter->offset = offset;
    adapter->offset_distance = 0;
  }

  if (GST_BUFFER_IS_DISCONT (buf)) {
    /* Take values as-is (might be NONE) */
    adapter->pts_at_discont = pts;
    adapter->dts_at_discont = dts;
    adapter->offset_at_discont = offset;
    adapter->distance_from_discont = 0;
  }
}

/**
 * gst_planar_audio_adapter_push:
 * @adapter: a #GstPlanarAudioAdapter
 * @buf: (transfer full): a #GstBuffer to queue in the adapter
 *
 * Adds the data from @buf to the data stored inside @adapter and takes
 * ownership of the buffer.
 */
void
gst_planar_audio_adapter_push (GstPlanarAudioAdapter * adapter, GstBuffer * buf)
{
  GstAudioMeta *meta;
  gsize samples;

  g_return_if_fail (GST_IS_PLANAR_AUDIO_ADAPTER (adapter));
  g_return_if_fail (GST_AUDIO_INFO_IS_VALID (&adapter->info));
  g_return_if_fail (GST_IS_BUFFER (buf));

  meta = gst_buffer_get_audio_meta (buf);
  g_return_if_fail (meta != NULL);
  g_return_if_fail (gst_audio_info_is_equal (&meta->info, &adapter->info));

  samples = meta->samples;
  adapter->samples += samples;

  if (G_UNLIKELY (adapter->buflist == NULL)) {
    GST_LOG_OBJECT (adapter, "pushing %p first %" G_GSIZE_FORMAT " samples",
        buf, samples);
    adapter->buflist = adapter->buflist_end = g_slist_append (NULL, buf);
    update_timestamps_and_offset (adapter, buf);
  } else {
    /* Otherwise append to the end, and advance our end pointer */
    GST_LOG_OBJECT (adapter, "pushing %p %" G_GSIZE_FORMAT " samples at end, "
        "samples now %" G_GSIZE_FORMAT, buf, samples, adapter->samples);
    adapter->buflist_end = g_slist_append (adapter->buflist_end, buf);
    adapter->buflist_end = g_slist_next (adapter->buflist_end);
  }
  ++adapter->count;
}

static void
gst_planar_audio_adapter_flush_unchecked (GstPlanarAudioAdapter * adapter,
    gsize to_flush)
{
  GSList *g = adapter->buflist;
  gsize cur_samples;

  /* clear state */
  adapter->samples -= to_flush;

  /* take skip into account */
  to_flush += adapter->skip;
  /* distance is always at least the amount of skipped samples */
  adapter->pts_distance -= adapter->skip;
  adapter->dts_distance -= adapter->skip;
  adapter->offset_distance -= adapter->skip;
  adapter->distance_from_discont -= adapter->skip;

  g = adapter->buflist;
  cur_samples = gst_buffer_get_audio_meta (g->data)->samples;
  while (to_flush >= cur_samples) {
    /* can skip whole buffer */
    GST_LOG_OBJECT (adapter, "flushing out head buffer");
    adapter->pts_distance += cur_samples;
    adapter->dts_distance += cur_samples;
    adapter->offset_distance += cur_samples;
    adapter->distance_from_discont += cur_samples;
    to_flush -= cur_samples;

    gst_buffer_unref (g->data);
    g = g_slist_delete_link (g, g);
    --adapter->count;

    if (G_UNLIKELY (g == NULL)) {
      GST_LOG_OBJECT (adapter, "adapter empty now");
      adapter->buflist_end = NULL;
      break;
    }
    /* there is a new head buffer, update the timestamps */
    update_timestamps_and_offset (adapter, g->data);
    cur_samples = gst_buffer_get_audio_meta (g->data)->samples;
  }
  adapter->buflist = g;
  /* account for the remaining bytes */
  adapter->skip = to_flush;
  adapter->pts_distance += to_flush;
  adapter->dts_distance += to_flush;
  adapter->offset_distance += to_flush;
  adapter->distance_from_discont += to_flush;
}

/**
 * gst_planar_audio_adapter_flush:
 * @adapter: a #GstPlanarAudioAdapter
 * @to_flush: the number of samples to flush
 *
 * Flushes the first @to_flush samples in the @adapter. The caller must ensure
 * that at least this many samples are available.
 */
void
gst_planar_audio_adapter_flush (GstPlanarAudioAdapter * adapter, gsize to_flush)
{
  g_return_if_fail (GST_IS_PLANAR_AUDIO_ADAPTER (adapter));
  g_return_if_fail (to_flush <= adapter->samples);

  /* flushing out 0 bytes will do nothing */
  if (G_UNLIKELY (to_flush == 0))
    return;

  gst_planar_audio_adapter_flush_unchecked (adapter, to_flush);
}

/**
 * gst_planar_audio_adapter_get_buffer:
 * @adapter: a #GstPlanarAudioAdapter
 * @nsamples: the number of samples to get
 * @flags: hint the intended use of the returned buffer
 *
 * Returns a #GstBuffer containing the first @nsamples of the @adapter, but
 * does not flush them from the adapter.
 * Use gst_planar_audio_adapter_take_buffer() for flushing at the same time.
 *
 * The map @flags can be used to give an optimization hint to this function.
 * When the requested buffer is meant to be mapped only for reading, it might
 * be possible to avoid copying memory in some cases.
 *
 * Caller owns a reference to the returned buffer. gst_buffer_unref() after
 * usage.
 *
 * Free-function: gst_buffer_unref
 *
 * Returns: (transfer full) (nullable): a #GstBuffer containing the first
 *     @nsamples of the adapter, or %NULL if @nsamples samples are not
 *     available. gst_buffer_unref() when no longer needed.
 */
GstBuffer *
gst_planar_audio_adapter_get_buffer (GstPlanarAudioAdapter * adapter,
    gsize nsamples, GstMapFlags flags)
{
  GstBuffer *buffer = NULL;
  GstBuffer *cur;
  gsize hsamples, skip;

  g_return_val_if_fail (GST_IS_PLANAR_AUDIO_ADAPTER (adapter), NULL);
  g_return_val_if_fail (GST_AUDIO_INFO_IS_VALID (&adapter->info), NULL);
  g_return_val_if_fail (nsamples > 0, NULL);

  GST_LOG_OBJECT (adapter, "getting buffer of %" G_GSIZE_FORMAT " samples",
      nsamples);

  /* we don't have enough data, return NULL. This is unlikely
   * as one usually does an _available() first instead of grabbing a
   * random size. */
  if (G_UNLIKELY (nsamples > adapter->samples))
    return NULL;

  cur = adapter->buflist->data;
  skip = adapter->skip;
  hsamples = gst_buffer_get_audio_meta (cur)->samples;


  if (skip == 0 && hsamples == nsamples) {
    /* our head buffer fits exactly the requirements */
    GST_LOG_OBJECT (adapter, "providing buffer of %" G_GSIZE_FORMAT " samples"
        " as head buffer", nsamples);

    buffer = gst_buffer_ref (cur);

  } else if (hsamples >= nsamples + skip && !(flags & GST_MAP_WRITE)) {
    /* return a buffer with the same data as our head buffer but with
     * a modified GstAudioMeta that maps only the parts of the planes
     * that should be made available to the caller. This is more efficient
     * for reading (no mem copy), but will hit performance if the caller
     * decides to map for writing or otherwise do a deep copy */
    GST_LOG_OBJECT (adapter, "providing buffer of %" G_GSIZE_FORMAT " samples"
        " via copy region", nsamples);

    buffer = gst_buffer_copy_region (cur, GST_BUFFER_COPY_ALL, 0, -1);
    gst_audio_buffer_truncate (buffer, adapter->info.bpf, skip, nsamples);

  } else {
    gint c, bps;
    GstAudioMeta *meta;

    /* construct a buffer with concatenated memory chunks from the appropriate
     * places. These memories will be copied into a single memory chunk
     * as soon as the buffer is mapped */
    GST_LOG_OBJECT (adapter, "providing buffer of %" G_GSIZE_FORMAT " samples"
        " via memory concatenation", nsamples);

    bps = adapter->info.finfo->width / 8;

    for (c = 0; c < adapter->info.channels; c++) {
      gsize need = nsamples;
      gsize cur_skip = skip;
      gsize take_from_cur;
      GSList *cur_node = adapter->buflist;
      while (cur_node && need > 0) {
        cur = cur_node->data;
        meta = gst_buffer_get_audio_meta (cur);
        take_from_cur = need > (meta->samples - cur_skip) ?
            meta->samples - cur_skip : need;

        cur = gst_buffer_copy_region (cur, GST_BUFFER_COPY_MEMORY,
            meta->offsets[c] + cur_skip * bps, take_from_cur * bps);

        if (!buffer)
          buffer = cur;
        else
          gst_buffer_append (buffer, cur);

        need -= take_from_cur;
        cur_skip = 0;
        cur_node = g_slist_next (cur_node);
      }
    }

    gst_buffer_add_audio_meta (buffer, &adapter->info, nsamples, NULL);
  }

  return buffer;
}

/**
 * gst_planar_audio_adapter_take_buffer:
 * @adapter: a #GstPlanarAudioAdapter
 * @nsamples: the number of samples to take
 * @flags: hint the intended use of the returned buffer
 *
 * Returns a #GstBuffer containing the first @nsamples bytes of the
 * @adapter. The returned bytes will be flushed from the adapter.
 *
 * See gst_planar_audio_adapter_get_buffer() for more details.
 *
 * Caller owns a reference to the returned buffer. gst_buffer_unref() after
 * usage.
 *
 * Free-function: gst_buffer_unref
 *
 * Returns: (transfer full) (nullable): a #GstBuffer containing the first
 *     @nsamples of the adapter, or %NULL if @nsamples samples are not
 *     available. gst_buffer_unref() when no longer needed.
 */
GstBuffer *
gst_planar_audio_adapter_take_buffer (GstPlanarAudioAdapter * adapter,
    gsize nsamples, GstMapFlags flags)
{
  GstBuffer *buffer;

  buffer = gst_planar_audio_adapter_get_buffer (adapter, nsamples, flags);
  if (buffer)
    gst_planar_audio_adapter_flush_unchecked (adapter, nsamples);

  return buffer;
}

/**
 * gst_planar_audio_adapter_available:
 * @adapter: a #GstPlanarAudioAdapter
 *
 * Gets the maximum amount of samples available, that is it returns the maximum
 * value that can be supplied to gst_planar_audio_adapter_get_buffer() without
 * that function returning %NULL.
 *
 * Returns: number of samples available in @adapter
 */
gsize
gst_planar_audio_adapter_available (GstPlanarAudioAdapter * adapter)
{
  g_return_val_if_fail (GST_IS_PLANAR_AUDIO_ADAPTER (adapter), 0);

  return adapter->samples;
}

/**
 * gst_planar_audio_adapter_get_distance_from_discont:
 * @adapter: a #GstPlanarAudioAdapter
 *
 * Get the distance in samples since the last buffer with the
 * %GST_BUFFER_FLAG_DISCONT flag.
 *
 * The distance will be reset to 0 for all buffers with
 * %GST_BUFFER_FLAG_DISCONT on them, and then calculated for all other
 * following buffers based on their size.
 *
 * Returns: The offset. Can be %GST_BUFFER_OFFSET_NONE.
 */
guint64
gst_planar_audio_adapter_distance_from_discont (GstPlanarAudioAdapter * adapter)
{
  return adapter->distance_from_discont;
}

/**
 * gst_planar_audio_adapter_offset_at_discont:
 * @adapter: a #GstPlanarAudioAdapter
 *
 * Get the offset that was on the last buffer with the GST_BUFFER_FLAG_DISCONT
 * flag, or GST_BUFFER_OFFSET_NONE.
 *
 * Returns: The offset at the last discont or GST_BUFFER_OFFSET_NONE.
 */
guint64
gst_planar_audio_adapter_offset_at_discont (GstPlanarAudioAdapter * adapter)
{
  g_return_val_if_fail (GST_IS_PLANAR_AUDIO_ADAPTER (adapter),
      GST_BUFFER_OFFSET_NONE);

  return adapter->offset_at_discont;
}

/**
 * gst_planar_audio_adapter_pts_at_discont:
 * @adapter: a #GstPlanarAudioAdapter
 *
 * Get the PTS that was on the last buffer with the GST_BUFFER_FLAG_DISCONT
 * flag, or GST_CLOCK_TIME_NONE.
 *
 * Returns: The PTS at the last discont or GST_CLOCK_TIME_NONE.
 */
GstClockTime
gst_planar_audio_adapter_pts_at_discont (GstPlanarAudioAdapter * adapter)
{
  g_return_val_if_fail (GST_IS_PLANAR_AUDIO_ADAPTER (adapter),
      GST_CLOCK_TIME_NONE);

  return adapter->pts_at_discont;
}

/**
 * gst_planar_audio_adapter_dts_at_discont:
 * @adapter: a #GstPlanarAudioAdapter
 *
 * Get the DTS that was on the last buffer with the GST_BUFFER_FLAG_DISCONT
 * flag, or GST_CLOCK_TIME_NONE.
 *
 * Returns: The DTS at the last discont or GST_CLOCK_TIME_NONE.
 */
GstClockTime
gst_planar_audio_adapter_dts_at_discont (GstPlanarAudioAdapter * adapter)
{
  g_return_val_if_fail (GST_IS_PLANAR_AUDIO_ADAPTER (adapter),
      GST_CLOCK_TIME_NONE);

  return adapter->dts_at_discont;
}

/**
 * gst_planar_audio_adapter_prev_offset:
 * @adapter: a #GstPlanarAudioAdapter
 * @distance: (out) (allow-none): pointer to a location for distance, or %NULL
 *
 * Get the offset that was before the current sample in the adapter. When
 * @distance is given, the amount of samples between the offset and the current
 * position is returned.
 *
 * The offset is reset to GST_BUFFER_OFFSET_NONE and the distance is set to 0
 * when the adapter is first created or when it is cleared. This also means that
 * before the first sample with an offset is removed from the adapter, the
 * offset and distance returned are GST_BUFFER_OFFSET_NONE and 0 respectively.
 *
 * Returns: The previous seen offset.
 */
guint64
gst_planar_audio_adapter_prev_offset (GstPlanarAudioAdapter * adapter,
    guint64 * distance)
{
  g_return_val_if_fail (GST_IS_PLANAR_AUDIO_ADAPTER (adapter),
      GST_BUFFER_OFFSET_NONE);

  if (distance)
    *distance = adapter->offset_distance;

  return adapter->offset;
}

/**
 * gst_planar_audio_adapter_prev_pts:
 * @adapter: a #GstPlanarAudioAdapter
 * @distance: (out) (allow-none): pointer to location for distance, or %NULL
 *
 * Get the pts that was before the current sample in the adapter. When
 * @distance is given, the amount of samples between the pts and the current
 * position is returned.
 *
 * The pts is reset to GST_CLOCK_TIME_NONE and the distance is set to 0 when
 * the adapter is first created or when it is cleared. This also means that before
 * the first sample with a pts is removed from the adapter, the pts
 * and distance returned are GST_CLOCK_TIME_NONE and 0 respectively.
 *
 * Returns: The previously seen pts.
 */
GstClockTime
gst_planar_audio_adapter_prev_pts (GstPlanarAudioAdapter * adapter,
    guint64 * distance)
{
  g_return_val_if_fail (GST_IS_PLANAR_AUDIO_ADAPTER (adapter),
      GST_CLOCK_TIME_NONE);

  if (distance)
    *distance = adapter->pts_distance;

  return adapter->pts;
}

/**
 * gst_planar_audio_adapter_prev_dts:
 * @adapter: a #GstPlanarAudioAdapter
 * @distance: (out) (allow-none): pointer to location for distance, or %NULL
 *
 * Get the dts that was before the current sample in the adapter. When
 * @distance is given, the amount of bytes between the dts and the current
 * position is returned.
 *
 * The dts is reset to GST_CLOCK_TIME_NONE and the distance is set to 0 when
 * the adapter is first created or when it is cleared. This also means that
 * before the first sample with a dts is removed from the adapter, the dts
 * and distance returned are GST_CLOCK_TIME_NONE and 0 respectively.
 *
 * Returns: The previously seen dts.
 */
GstClockTime
gst_planar_audio_adapter_prev_dts (GstPlanarAudioAdapter * adapter,
    guint64 * distance)
{
  g_return_val_if_fail (GST_IS_PLANAR_AUDIO_ADAPTER (adapter),
      GST_CLOCK_TIME_NONE);

  if (distance)
    *distance = adapter->dts_distance;

  return adapter->dts;
}
