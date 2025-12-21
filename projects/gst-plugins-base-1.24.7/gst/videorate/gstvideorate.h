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

#ifndef __GST_VIDEO_RATE_H__
#define __GST_VIDEO_RATE_H__

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>

G_BEGIN_DECLS

#define GST_TYPE_VIDEO_RATE (gst_video_rate_get_type())
G_DECLARE_FINAL_TYPE (GstVideoRate, gst_video_rate, GST, VIDEO_RATE,
    GstBaseTransform)

/**
 * GstVideoRate:
 *
 * Opaque data structure.
 */
struct _GstVideoRate
{
  GstBaseTransform parent;

  /* video state */
  gint from_rate_numerator, from_rate_denominator;
  gint to_rate_numerator, to_rate_denominator;
  guint64 next_ts;              /* Timestamp of next buffer to output */
  GstBuffer *prevbuf;
  guint64 prev_ts;              /* Previous buffer timestamp */
  guint64 out_frame_count;      /* number of frames output since the beginning
                                 * of the segment or the last frame rate caps
                                 * change, whichever was later */
  guint64 base_ts;              /* used in next_ts calculation after a
                                 * frame rate caps change */
  gboolean discont;
  guint64 last_ts;              /* Timestamp of last input buffer */

  guint64 average_period;
  GstClockTimeDiff wanted_diff; /* target average diff */
  GstClockTimeDiff average;     /* moving average period */
  gboolean force_variable_rate;
  gboolean updating_caps;
  guint64 max_duplication_time;
  guint64 max_closing_segment_duplication_duration;

  /* segment handling */
  GstSegment segment;

  /* properties */
  guint64 in, out, dup, drop;
  gboolean silent;
  gdouble new_pref;
  gboolean skip_to_first;
  gboolean drop_only;
  gboolean drop_out_of_segment;
  guint64 average_period_set;

  int max_rate;
  gdouble rate;
  gdouble pending_rate;

  GstCaps *in_caps;
  /* Only set right after caps were set so that we still have a reference to
   * the caps matching the content of `->prevbuf`, this way, if we get an EOS
   * right after a CAPS, we can reset to those caps and close the segment with
   * it */
  GstCaps *prev_caps;
};

GST_ELEMENT_REGISTER_DECLARE (videorate);

G_END_DECLS
#endif /* __GST_VIDEO_RATE_H__ */
