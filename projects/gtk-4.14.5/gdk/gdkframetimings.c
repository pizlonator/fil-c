/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2012 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <string.h>

#include "gdkframeclockprivate.h"

/**
 * GdkFrameTimings:
 *
 * A `GdkFrameTimings` object holds timing information for a single frame
 * of the application’s displays.
 *
 * To retrieve `GdkFrameTimings` objects, use [method@Gdk.FrameClock.get_timings]
 * or [method@Gdk.FrameClock.get_current_timings]. The information in
 * `GdkFrameTimings` is useful for precise synchronization of video with
 * the event or audio streams, and for measuring quality metrics for the
 * application’s display, such as latency and jitter.
 */

G_DEFINE_BOXED_TYPE (GdkFrameTimings, gdk_frame_timings,
                     gdk_frame_timings_ref,
                     gdk_frame_timings_unref)

GdkFrameTimings *
_gdk_frame_timings_new (gint64 frame_counter)
{
  GdkFrameTimings *timings;

  timings = g_new0 (GdkFrameTimings, 1);
  timings->ref_count = 1;
  timings->frame_counter = frame_counter;

  return timings;
}

gboolean
_gdk_frame_timings_steal (GdkFrameTimings *timings,
                          gint64           frame_counter)
{
  if (timings->ref_count == 1)
    {
      memset (timings, 0, sizeof *timings);
      timings->ref_count = 1;
      timings->frame_counter = frame_counter;
      return TRUE;
    }

  return FALSE;
}

/**
 * gdk_frame_timings_ref:
 * @timings: a `GdkFrameTimings`
 *
 * Increases the reference count of @timings.
 *
 * Returns: @timings
 */
GdkFrameTimings *
gdk_frame_timings_ref (GdkFrameTimings *timings)
{
  g_return_val_if_fail (timings != NULL, NULL);

  timings->ref_count++;

  return timings;
}

/**
 * gdk_frame_timings_unref:
 * @timings: a `GdkFrameTimings`
 *
 * Decreases the reference count of @timings.
 *
 * If @timings is no longer referenced, it will be freed.
 */
void
gdk_frame_timings_unref (GdkFrameTimings *timings)
{
  g_return_if_fail (timings != NULL);
  g_return_if_fail (timings->ref_count > 0);

  timings->ref_count--;
  if (timings->ref_count == 0)
    g_free (timings);
}

/**
 * gdk_frame_timings_get_frame_counter:
 * @timings: a `GdkFrameTimings`
 *
 * Gets the frame counter value of the `GdkFrameClock` when
 * this frame was drawn.
 *
 * Returns: the frame counter value for this frame
 */
gint64
gdk_frame_timings_get_frame_counter (GdkFrameTimings *timings)
{
  return timings->frame_counter;
}

/**
 * gdk_frame_timings_get_complete:
 * @timings: a `GdkFrameTimings`
 *
 * Returns whether @timings are complete.
 *
 * The timing information in a `GdkFrameTimings` is filled in
 * incrementally as the frame as drawn and passed off to the
 * window system for processing and display to the user. The
 * accessor functions for `GdkFrameTimings` can return 0 to
 * indicate an unavailable value for two reasons: either because
 * the information is not yet available, or because it isn't
 * available at all.
 *
 * Once this function returns %TRUE for a frame, you can be
 * certain that no further values will become available and be
 * stored in the `GdkFrameTimings`.
 *
 * Returns: %TRUE if all information that will be available
 *   for the frame has been filled in.
 */
gboolean
gdk_frame_timings_get_complete (GdkFrameTimings *timings)
{
  g_return_val_if_fail (timings != NULL, FALSE);

  return timings->complete;
}

/**
 * gdk_frame_timings_get_frame_time:
 * @timings: A `GdkFrameTimings`
 *
 * Returns the frame time for the frame.
 *
 * This is the time value that is typically used to time
 * animations for the frame. See [method@Gdk.FrameClock.get_frame_time].
 *
 * Returns: the frame time for the frame, in the timescale
 *  of g_get_monotonic_time()
 */
gint64
gdk_frame_timings_get_frame_time (GdkFrameTimings *timings)
{
  g_return_val_if_fail (timings != NULL, 0);

  return timings->frame_time;
}

/**
 * gdk_frame_timings_get_presentation_time:
 * @timings: a `GdkFrameTimings`
 *
 * Reurns the presentation time.
 *
 * This is the time at which the frame became visible to the user.
 *
 * Returns: the time the frame was displayed to the user, in the
 *   timescale of g_get_monotonic_time(), or 0 if no presentation
 *   time is available. See [method@Gdk.FrameTimings.get_complete]
 */
gint64
gdk_frame_timings_get_presentation_time (GdkFrameTimings *timings)
{
  g_return_val_if_fail (timings != NULL, 0);

  return timings->presentation_time;
}

/**
 * gdk_frame_timings_get_predicted_presentation_time:
 * @timings: a `GdkFrameTimings`
 *
 * Gets the predicted time at which this frame will be displayed.
 *
 * Although no predicted time may be available, if one is available,
 * it will be available while the frame is being generated, in contrast
 * to [method@Gdk.FrameTimings.get_presentation_time], which is only
 * available after the frame has been presented.
 *
 * In general, if you are simply animating, you should use
 * [method@Gdk.FrameClock.get_frame_time] rather than this function,
 * but this function is useful for applications that want exact control
 * over latency. For example, a movie player may want this information
 * for Audio/Video synchronization.
 *
 * Returns: The predicted time at which the frame will be presented,
 *   in the timescale of g_get_monotonic_time(), or 0 if no predicted
 *   presentation time is available.
 */
gint64
gdk_frame_timings_get_predicted_presentation_time (GdkFrameTimings *timings)
{
  g_return_val_if_fail (timings != NULL, 0);

  return timings->predicted_presentation_time;
}

/**
 * gdk_frame_timings_get_refresh_interval:
 * @timings: a `GdkFrameTimings`
 *
 * Gets the natural interval between presentation times for
 * the display that this frame was displayed on.
 *
 * Frame presentation usually happens during the “vertical
 * blanking interval”.
 *
 * Returns: the refresh interval of the display, in microseconds,
 *   or 0 if the refresh interval is not available.
 *   See [method@Gdk.FrameTimings.get_complete].
 */
gint64
gdk_frame_timings_get_refresh_interval (GdkFrameTimings *timings)
{
  g_return_val_if_fail (timings != NULL, 0);

  return timings->refresh_interval;
}
