/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
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
 * SECTION:gstaudio
 * @title: GstAudio
 * @short_description: Support library for audio elements
 *
 * This library contains some helper functions for audio elements.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <string.h>

#include "audio.h"
#include "audio-enumtypes.h"

#ifndef GST_DISABLE_GST_DEBUG
#define GST_CAT_DEFAULT ensure_debug_category()
static GstDebugCategory *
ensure_debug_category (void)
{
  static gsize cat_gonce = 0;

  if (g_once_init_enter (&cat_gonce)) {
    gsize cat_done;

    cat_done = (gsize) _gst_debug_category_new ("audio", 0, "audio library");

    g_once_init_leave (&cat_gonce, cat_done);
  }

  return (GstDebugCategory *) cat_gonce;
}
#else
#define ensure_debug_category() /* NOOP */
#endif /* GST_DISABLE_GST_DEBUG */


/**
 * gst_audio_buffer_clip:
 * @buffer: (transfer full): The buffer to clip.
 * @segment: Segment in %GST_FORMAT_TIME or %GST_FORMAT_DEFAULT to which
 *           the buffer should be clipped.
 * @rate: sample rate.
 * @bpf: size of one audio frame in bytes. This is the size of one sample *
 * number of channels.
 *
 * Clip the buffer to the given %GstSegment.
 *
 * After calling this function the caller does not own a reference to
 * @buffer anymore.
 *
 * Returns: (transfer full) (nullable): %NULL if the buffer is completely outside the configured segment,
 * otherwise the clipped buffer is returned.
 *
 * If the buffer has no timestamp, it is assumed to be inside the segment and
 * is not clipped
 */
GstBuffer *
gst_audio_buffer_clip (GstBuffer * buffer, const GstSegment * segment,
    gint rate, gint bpf)
{
  GstBuffer *ret;
  GstAudioMeta *meta;
  GstClockTime timestamp = GST_CLOCK_TIME_NONE, duration = GST_CLOCK_TIME_NONE;
  guint64 offset = GST_BUFFER_OFFSET_NONE, offset_end = GST_BUFFER_OFFSET_NONE;
  gsize trim, size, osize;
  gboolean change_duration = TRUE, change_offset = TRUE, change_offset_end =
      TRUE;

  g_return_val_if_fail (segment->format == GST_FORMAT_TIME ||
      segment->format == GST_FORMAT_DEFAULT, buffer);
  g_return_val_if_fail (GST_IS_BUFFER (buffer), NULL);

  if (!GST_BUFFER_PTS_IS_VALID (buffer))
    /* No timestamp - assume the buffer is completely in the segment */
    return buffer;

  /* Get copies of the buffer metadata to change later.
   * Calculate the missing values for the calculations,
   * they won't be changed later though. */

  meta = gst_buffer_get_audio_meta (buffer);

  /* these variables measure samples */
  trim = 0;
  osize = size = meta ? meta->samples : (gst_buffer_get_size (buffer) / bpf);

  /* no data, nothing to clip */
  if (!size)
    return buffer;

  timestamp = GST_BUFFER_PTS (buffer);
  GST_DEBUG ("timestamp %" GST_TIME_FORMAT, GST_TIME_ARGS (timestamp));
  if (GST_BUFFER_DURATION_IS_VALID (buffer)) {
    duration = GST_BUFFER_DURATION (buffer);
  } else {
    change_duration = FALSE;
    duration = gst_util_uint64_scale (size, GST_SECOND, rate);
  }

  if (GST_BUFFER_OFFSET_IS_VALID (buffer)) {
    offset = GST_BUFFER_OFFSET (buffer);
  } else {
    change_offset = FALSE;
    offset = 0;
  }

  if (GST_BUFFER_OFFSET_END_IS_VALID (buffer)) {
    offset_end = GST_BUFFER_OFFSET_END (buffer);
  } else {
    change_offset_end = FALSE;
    offset_end = offset + size;
  }

  if (segment->format == GST_FORMAT_TIME) {
    /* Handle clipping for GST_FORMAT_TIME */

    guint64 start, stop, cstart, cstop, diff;

    start = timestamp;
    stop = timestamp + duration;

    if (gst_segment_clip (segment, GST_FORMAT_TIME,
            start, stop, &cstart, &cstop)) {

      diff = cstart - start;
      if (diff > 0) {
        timestamp = cstart;

        if (change_duration)
          duration -= diff;

        diff = gst_util_uint64_scale (diff, rate, GST_SECOND);
        if (change_offset)
          offset += diff;
        trim += diff;
        size -= diff;
      }

      diff = stop - cstop;
      if (diff > 0) {
        /* duration is always valid if stop is valid */
        duration -= diff;

        diff = gst_util_uint64_scale (diff, rate, GST_SECOND);
        if (change_offset_end)
          offset_end -= diff;
        size -= diff;
      }
    } else {
      gst_buffer_unref (buffer);
      return NULL;
    }
  } else {
    /* Handle clipping for GST_FORMAT_DEFAULT */
    guint64 start, stop, cstart, cstop, diff;

    g_return_val_if_fail (GST_BUFFER_OFFSET_IS_VALID (buffer), buffer);

    start = offset;
    stop = offset_end;

    if (gst_segment_clip (segment, GST_FORMAT_DEFAULT,
            start, stop, &cstart, &cstop)) {

      diff = cstart - start;
      if (diff > 0) {
        offset = cstart;

        timestamp = gst_util_uint64_scale (cstart, GST_SECOND, rate);

        if (change_duration)
          duration -= gst_util_uint64_scale (diff, GST_SECOND, rate);

        trim += diff;
        size -= diff;
      }

      diff = stop - cstop;
      if (diff > 0) {
        offset_end = cstop;

        if (change_duration)
          duration -= gst_util_uint64_scale (diff, GST_SECOND, rate);

        size -= diff;
      }
    } else {
      gst_buffer_unref (buffer);
      return NULL;
    }
  }

  if (trim == 0 && size == osize) {
    ret = buffer;

    if (GST_BUFFER_PTS (ret) != timestamp) {
      ret = gst_buffer_make_writable (ret);
      GST_BUFFER_PTS (ret) = timestamp;
    }
    if (GST_BUFFER_DURATION (ret) != duration) {
      ret = gst_buffer_make_writable (ret);
      GST_BUFFER_DURATION (ret) = duration;
    }
  } else {
    /* cut out all the samples that are no longer relevant */
    GST_DEBUG ("trim %" G_GSIZE_FORMAT " size %" G_GSIZE_FORMAT, trim, size);
    ret = gst_audio_buffer_truncate (buffer, bpf, trim, size);

    GST_DEBUG ("timestamp %" GST_TIME_FORMAT, GST_TIME_ARGS (timestamp));
    if (ret) {
      GST_BUFFER_PTS (ret) = timestamp;

      if (change_duration)
        GST_BUFFER_DURATION (ret) = duration;
      if (change_offset)
        GST_BUFFER_OFFSET (ret) = offset;
      if (change_offset_end)
        GST_BUFFER_OFFSET_END (ret) = offset_end;
    } else {
      GST_ERROR ("gst_audio_buffer_truncate failed");
    }
  }
  return ret;
}

/**
 * gst_audio_buffer_truncate:
 * @buffer: (transfer full): The buffer to truncate.
 * @bpf: size of one audio frame in bytes. This is the size of one sample *
 * number of channels.
 * @trim: the number of samples to remove from the beginning of the buffer
 * @samples: the final number of samples that should exist in this buffer or -1
 * to use all the remaining samples if you are only removing samples from the
 * beginning.
 *
 * Truncate the buffer to finally have @samples number of samples, removing
 * the necessary amount of samples from the end and @trim number of samples
 * from the beginning.
 *
 * This function does not know the audio rate, therefore the caller is
 * responsible for re-setting the correct timestamp and duration to the
 * buffer. However, timestamp will be preserved if trim == 0, and duration
 * will also be preserved if there is no trimming to be done. Offset and
 * offset end will be preserved / updated.
 *
 * After calling this function the caller does not own a reference to
 * @buffer anymore.
 *
 * Returns: (transfer full): the truncated buffer
 *
 * Since: 1.16
 */
GstBuffer *
gst_audio_buffer_truncate (GstBuffer * buffer, gint bpf, gsize trim,
    gsize samples)
{
  GstAudioMeta *meta = NULL;
  GstBuffer *ret = NULL;
  gsize orig_samples;
  gint i;
  GstClockTime orig_ts, orig_offset;

  g_return_val_if_fail (GST_IS_BUFFER (buffer), NULL);

  meta = gst_buffer_get_audio_meta (buffer);
  orig_samples = meta ? meta->samples : gst_buffer_get_size (buffer) / bpf;
  orig_ts = GST_BUFFER_PTS (buffer);
  orig_offset = GST_BUFFER_OFFSET (buffer);

  g_return_val_if_fail (trim < orig_samples, NULL);
  g_return_val_if_fail (samples == -1 || trim + samples <= orig_samples, NULL);

  if (samples == -1)
    samples = orig_samples - trim;

  /* nothing to truncate */
  if (samples == orig_samples)
    return buffer;

  GST_DEBUG ("Truncating %" G_GSIZE_FORMAT " to %" G_GSIZE_FORMAT
      " (trim start %" G_GSIZE_FORMAT ", end %" G_GSIZE_FORMAT ")",
      orig_samples, samples, trim, orig_samples - trim - samples);

  if (!meta || meta->info.layout == GST_AUDIO_LAYOUT_INTERLEAVED) {
    /* interleaved */
    ret = gst_buffer_copy_region (buffer, GST_BUFFER_COPY_ALL, trim * bpf,
        samples * bpf);
    gst_buffer_unref (buffer);

    if ((meta = gst_buffer_get_audio_meta (ret)))
      meta->samples = samples;
  } else {
    /* non-interleaved */
    ret = gst_buffer_make_writable (buffer);
    meta = gst_buffer_get_audio_meta (ret);
    meta->samples = samples;
    for (i = 0; i < meta->info.channels; i++) {
      meta->offsets[i] += trim * bpf / meta->info.channels;
    }
  }

  GST_BUFFER_DTS (ret) = GST_CLOCK_TIME_NONE;
  if (GST_CLOCK_TIME_IS_VALID (orig_ts) && trim == 0) {
    GST_BUFFER_PTS (ret) = orig_ts;
  } else {
    GST_BUFFER_PTS (ret) = GST_CLOCK_TIME_NONE;
  }
  /* If duration was the same, it would have meant there's no trimming to be
   * done, so we have an early return further up */
  GST_BUFFER_DURATION (ret) = GST_CLOCK_TIME_NONE;
  if (orig_offset != GST_BUFFER_OFFSET_NONE) {
    GST_BUFFER_OFFSET (ret) = orig_offset + trim;
    GST_BUFFER_OFFSET_END (ret) = GST_BUFFER_OFFSET (ret) + samples;
  } else {
    GST_BUFFER_OFFSET (ret) = GST_BUFFER_OFFSET_NONE;
    GST_BUFFER_OFFSET_END (ret) = GST_BUFFER_OFFSET_NONE;
  }

  return ret;
}
