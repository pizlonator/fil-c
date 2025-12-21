/* GStreamer
 * Copyright (C) 2017 Sebastian Dröge <sebastian@centricular.com>
 *
 * gstaudiostreamalign.h:
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

#include "gstaudiostreamalign.h"

/**
 * SECTION:gstaudiostreamalign
 * @title: GstAudioStreamAlign
 * @short_description: Helper object for tracking audio stream alignment and discontinuities
 *
 * #GstAudioStreamAlign provides a helper object that helps tracking audio
 * stream alignment and discontinuities, and detects discontinuities if
 * possible.
 *
 * See gst_audio_stream_align_new() for a description of its parameters and
 * gst_audio_stream_align_process() for the details of the processing.
 */

G_DEFINE_BOXED_TYPE (GstAudioStreamAlign, gst_audio_stream_align,
    (GBoxedCopyFunc) gst_audio_stream_align_copy,
    (GBoxedFreeFunc) gst_audio_stream_align_free);

struct _GstAudioStreamAlign
{
  gint rate;
  GstClockTime alignment_threshold;
  GstClockTime discont_wait;

  /* counter to keep track of timestamps */
  guint64 next_offset;
  GstClockTime timestamp_at_discont;
  guint64 samples_since_discont;

  /* Last time we noticed a discont */
  GstClockTime discont_time;
};

/**
 * gst_audio_stream_align_new:
 * @rate: a sample rate
 * @alignment_threshold: a alignment threshold in nanoseconds
 * @discont_wait: discont wait in nanoseconds
 *
 * Allocate a new #GstAudioStreamAlign with the given configuration. All
 * processing happens according to sample rate @rate, until
 * gst_audio_stream_align_set_rate() is called with a new @rate.
 * A negative rate can be used for reverse playback.
 *
 * @alignment_threshold gives the tolerance in nanoseconds after which a
 * timestamp difference is considered a discontinuity. Once detected,
 * @discont_wait nanoseconds have to pass without going below the threshold
 * again until the output buffer is marked as a discontinuity. These can later
 * be re-configured with gst_audio_stream_align_set_alignment_threshold() and
 * gst_audio_stream_align_set_discont_wait().
 *
 * Returns: a new #GstAudioStreamAlign. free with gst_audio_stream_align_free().
 *
 * Since: 1.14
 */
GstAudioStreamAlign *
gst_audio_stream_align_new (gint rate, GstClockTime alignment_threshold,
    GstClockTime discont_wait)
{
  GstAudioStreamAlign *align;

  g_return_val_if_fail (rate != 0, NULL);
  g_return_val_if_fail (GST_CLOCK_TIME_IS_VALID (alignment_threshold), NULL);
  g_return_val_if_fail (GST_CLOCK_TIME_IS_VALID (discont_wait), NULL);

  align = g_new0 (GstAudioStreamAlign, 1);
  align->rate = rate;
  align->alignment_threshold = alignment_threshold;
  align->discont_wait = discont_wait;

  align->timestamp_at_discont = GST_CLOCK_TIME_NONE;
  align->samples_since_discont = 0;
  gst_audio_stream_align_mark_discont (align);

  return align;
}

/**
 * gst_audio_stream_align_copy:
 * @align: a #GstAudioStreamAlign
 *
 * Copy a GstAudioStreamAlign structure.
 *
 * Returns: a new #GstAudioStreamAlign. free with gst_audio_stream_align_free.
 *
 * Since: 1.14
 */
GstAudioStreamAlign *
gst_audio_stream_align_copy (const GstAudioStreamAlign * align)
{
  GstAudioStreamAlign *copy;

  g_return_val_if_fail (align != NULL, NULL);

  copy = g_new0 (GstAudioStreamAlign, 1);
  *copy = *align;

  return copy;
}

/**
 * gst_audio_stream_align_free:
 * @align: a #GstAudioStreamAlign
 *
 * Free a GstAudioStreamAlign structure previously allocated with gst_audio_stream_align_new()
 * or gst_audio_stream_align_copy().
 *
 * Since: 1.14
 */
void
gst_audio_stream_align_free (GstAudioStreamAlign * align)
{
  g_return_if_fail (align != NULL);
  g_free (align);
}

/**
 * gst_audio_stream_align_set_rate:
 * @align: a #GstAudioStreamAlign
 * @rate: a new sample rate
 *
 * Sets @rate as new sample rate for the following processing. If the sample
 * rate differs this implicitly marks the next data as discontinuous.
 *
 * Since: 1.14
 */
void
gst_audio_stream_align_set_rate (GstAudioStreamAlign * align, gint rate)
{
  g_return_if_fail (align != NULL);
  g_return_if_fail (rate != 0);

  if (align->rate == rate)
    return;

  align->rate = rate;
  gst_audio_stream_align_mark_discont (align);
}

/**
 * gst_audio_stream_align_get_rate:
 * @align: a #GstAudioStreamAlign
 *
 * Gets the currently configured sample rate.
 *
 * Returns: The currently configured sample rate
 *
 * Since: 1.14
 */
gint
gst_audio_stream_align_get_rate (const GstAudioStreamAlign * align)
{
  g_return_val_if_fail (align != NULL, 0);

  return align->rate;
}

/**
 * gst_audio_stream_align_set_alignment_threshold:
 * @align: a #GstAudioStreamAlign
 * @alignment_threshold: a new alignment threshold
 *
 * Sets @alignment_treshold as new alignment threshold for the following processing.
 *
 * Since: 1.14
 */
void
gst_audio_stream_align_set_alignment_threshold (GstAudioStreamAlign *
    align, GstClockTime alignment_threshold)
{
  g_return_if_fail (align != NULL);
  g_return_if_fail (GST_CLOCK_TIME_IS_VALID (alignment_threshold));

  align->alignment_threshold = alignment_threshold;
}

/**
 * gst_audio_stream_align_get_alignment_threshold:
 * @align: a #GstAudioStreamAlign
 *
 * Gets the currently configured alignment threshold.
 *
 * Returns: The currently configured alignment threshold
 *
 * Since: 1.14
 */
GstClockTime
gst_audio_stream_align_get_alignment_threshold (const GstAudioStreamAlign *
    align)
{
  g_return_val_if_fail (align != NULL, 0);

  return align->alignment_threshold;
}

/**
 * gst_audio_stream_align_set_discont_wait:
 * @align: a #GstAudioStreamAlign
 * @discont_wait: a new discont wait
 *
 * Sets @alignment_treshold as new discont wait for the following processing.
 *
 * Since: 1.14
 */
void
gst_audio_stream_align_set_discont_wait (GstAudioStreamAlign * align,
    GstClockTime discont_wait)
{
  g_return_if_fail (align != NULL);
  g_return_if_fail (GST_CLOCK_TIME_IS_VALID (discont_wait));

  align->discont_wait = discont_wait;
}

/**
 * gst_audio_stream_align_get_discont_wait:
 * @align: a #GstAudioStreamAlign
 *
 * Gets the currently configured discont wait.
 *
 * Returns: The currently configured discont wait
 *
 * Since: 1.14
 */
GstClockTime
gst_audio_stream_align_get_discont_wait (const GstAudioStreamAlign * align)
{
  g_return_val_if_fail (align != NULL, 0);

  return align->discont_wait;
}

/**
 * gst_audio_stream_align_mark_discont:
 * @align: a #GstAudioStreamAlign
 *
 * Marks the next buffer as discontinuous and resets timestamp tracking.
 *
 * Since: 1.14
 */
void
gst_audio_stream_align_mark_discont (GstAudioStreamAlign * align)
{
  g_return_if_fail (align != NULL);

  align->next_offset = -1;
  align->discont_time = GST_CLOCK_TIME_NONE;
}

/**
 * gst_audio_stream_align_get_timestamp_at_discont:
 * @align: a #GstAudioStreamAlign
 *
 * Timestamp that was passed when a discontinuity was detected, i.e. the first
 * timestamp after the discontinuity.
 *
 * Returns: The last timestamp at when a discontinuity was detected
 *
 * Since: 1.14
 */
GstClockTime
gst_audio_stream_align_get_timestamp_at_discont (const GstAudioStreamAlign *
    align)
{
  g_return_val_if_fail (align != NULL, GST_CLOCK_TIME_NONE);

  return align->timestamp_at_discont;
}

/**
 * gst_audio_stream_align_get_samples_since_discont:
 * @align: a #GstAudioStreamAlign
 *
 * Returns the number of samples that were processed since the last
 * discontinuity was detected.
 *
 * Returns: The number of samples processed since the last discontinuity.
 *
 * Since: 1.14
 */
guint64
gst_audio_stream_align_get_samples_since_discont (const GstAudioStreamAlign *
    align)
{
  g_return_val_if_fail (align != NULL, 0);

  return align->samples_since_discont;
}

/**
 * gst_audio_stream_align_process:
 * @align: a #GstAudioStreamAlign
 * @discont: if this data is considered to be discontinuous
 * @timestamp: a #GstClockTime of the start of the data
 * @n_samples: number of samples to process
 * @out_timestamp: (out): output timestamp of the data
 * @out_duration: (out): output duration of the data
 * @out_sample_position: (out): output sample position of the start of the data
 *
 * Processes data with @timestamp and @n_samples, and returns the output
 * timestamp, duration and sample position together with a boolean to signal
 * whether a discontinuity was detected or not. All non-discontinuous data
 * will have perfect timestamps and durations.
 *
 * A discontinuity is detected once the difference between the actual
 * timestamp and the timestamp calculated from the sample count since the last
 * discontinuity differs by more than the alignment threshold for a duration
 * longer than discont wait.
 *
 * Note: In reverse playback, every buffer is considered discontinuous in the
 * context of buffer flags because the last sample of the previous buffer is
 * discontinuous with the first sample of the current one. However for this
 * function they are only considered discontinuous in reverse playback if the
 * first sample of the previous buffer is discontinuous with the last sample
 * of the current one.
 *
 * Returns: %TRUE if a discontinuity was detected, %FALSE otherwise.
 *
 * Since: 1.14
 */
#define ABSDIFF(a, b) ((a) > (b) ? (a) - (b) : (b) - (a))
gboolean
gst_audio_stream_align_process (GstAudioStreamAlign * align,
    gboolean discont, GstClockTime timestamp, guint n_samples,
    GstClockTime * out_timestamp, GstClockTime * out_duration,
    guint64 * out_sample_position)
{
  GstClockTime start_time, end_time, duration;
  guint64 start_offset, end_offset;

  g_return_val_if_fail (align != NULL, FALSE);

  start_time = timestamp;
  start_offset =
      gst_util_uint64_scale (start_time, ABS (align->rate), GST_SECOND);

  end_offset = start_offset + n_samples;
  end_time =
      gst_util_uint64_scale_int (end_offset, GST_SECOND, ABS (align->rate));

  duration = end_time - start_time;

  if (align->next_offset == (guint64) - 1 || discont) {
    discont = TRUE;
  } else {
    guint64 diff, max_sample_diff;
    GstClockTime expected_time;

    /* Check discont */
    if (align->rate > 0) {
      diff = ABSDIFF (start_offset, align->next_offset);
    } else {
      diff = ABSDIFF (end_offset, align->next_offset);
    }

    expected_time =
        gst_util_uint64_scale (align->next_offset, GST_SECOND,
        ABS (align->rate));

    max_sample_diff =
        gst_util_uint64_scale_int (align->alignment_threshold,
        ABS (align->rate), GST_SECOND);

    /* Discont! */
    if (G_UNLIKELY (diff >= max_sample_diff)) {
      if (align->discont_wait > 0) {
        if (align->discont_time == GST_CLOCK_TIME_NONE) {
          if (align->rate > 0
              && ABSDIFF (expected_time, start_time) >= align->discont_wait)
            discont = TRUE;
          else if (align->rate < 0
              && ABSDIFF (expected_time, end_time) >= align->discont_wait)
            discont = TRUE;
          else
            align->discont_time = expected_time;
        } else if ((align->rate > 0
                && ABSDIFF (start_time,
                    align->discont_time) >= align->discont_wait)
            || (align->rate < 0
                && ABSDIFF (end_time,
                    align->discont_time) >= align->discont_wait)) {
          discont = TRUE;
          align->discont_time = GST_CLOCK_TIME_NONE;
        }
      } else {
        discont = TRUE;
      }
    } else if (G_UNLIKELY (align->discont_time != GST_CLOCK_TIME_NONE)) {
      /* we have had a discont, but are now back on track! */
      align->discont_time = GST_CLOCK_TIME_NONE;
    }
  }

  if (discont) {
    /* Have discont, need resync and use the capture timestamps */
    if (align->next_offset != (guint64) - 1)
      GST_INFO ("Have discont. Expected %"
          G_GUINT64_FORMAT ", got %" G_GUINT64_FORMAT,
          align->next_offset, start_offset);
    align->next_offset = align->rate > 0 ? end_offset : start_offset;
    align->timestamp_at_discont = start_time;
    align->samples_since_discont = 0;

    /* Got a discont and adjusted, reset the discont_time marker */
    align->discont_time = GST_CLOCK_TIME_NONE;
  } else {

    /* No discont, just keep counting */
    if (align->rate > 0) {
      timestamp =
          gst_util_uint64_scale (align->next_offset, GST_SECOND,
          ABS (align->rate));

      start_offset = align->next_offset;
      align->next_offset += n_samples;

      duration =
          gst_util_uint64_scale (align->next_offset, GST_SECOND,
          ABS (align->rate)) - timestamp;
    } else {
      guint64 old_offset = align->next_offset;

      if (align->next_offset > n_samples)
        align->next_offset -= n_samples;
      else
        align->next_offset = 0;
      start_offset = align->next_offset;

      timestamp =
          gst_util_uint64_scale (align->next_offset, GST_SECOND,
          ABS (align->rate));

      duration =
          gst_util_uint64_scale (old_offset, GST_SECOND,
          ABS (align->rate)) - timestamp;
    }
  }

  align->samples_since_discont += n_samples;

  if (out_timestamp)
    *out_timestamp = timestamp;
  if (out_duration)
    *out_duration = duration;
  if (out_sample_position)
    *out_sample_position = start_offset;

  return discont;
}

#undef ABSDIFF
