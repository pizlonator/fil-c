/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 * Copyright (C) <2005> Tim-Philipp Müller <tim@centricular.net>
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


#ifndef __GST_TIME_OVERLAY_H__
#define __GST_TIME_OVERLAY_H__

#include "gstbasetextoverlay.h"

G_BEGIN_DECLS

#define GST_TYPE_TIME_OVERLAY \
  (gst_time_overlay_get_type())
#define GST_TIME_OVERLAY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_TIME_OVERLAY,GstTimeOverlay))
#define GST_TIME_OVERLAY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_TIME_OVERLAY,GstTimeOverlayClass))
#define GST_IS_TIME_OVERLAY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_TIME_OVERLAY))
#define GST_IS_TIME_OVERLAY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_TIME_OVERLAY))
#define GST_TIME_OVERLAY_CAST(obj) ((GstTimeOverlay*)(obj))

typedef struct _GstTimeOverlay GstTimeOverlay;
typedef struct _GstTimeOverlayClass GstTimeOverlayClass;

typedef enum {
  GST_TIME_OVERLAY_TIME_LINE_BUFFER_TIME,
  GST_TIME_OVERLAY_TIME_LINE_STREAM_TIME,
  GST_TIME_OVERLAY_TIME_LINE_RUNNING_TIME,
  GST_TIME_OVERLAY_TIME_LINE_TIME_CODE,
  GST_TIME_OVERLAY_TIME_LINE_ELAPSED_RUNNING_TIME,
  GST_TIME_OVERLAY_TIME_LINE_REFERENCE_TIMESTAMP,
  GST_TIME_OVERLAY_TIME_LINE_BUFFER_COUNT,
  GST_TIME_OVERLAY_TIME_LINE_BUFFER_OFFSET,
} GstTimeOverlayTimeLine;

/**
 * GstTimeOverlay:
 *
 * Opaque timeoverlay data structure.
 */
struct _GstTimeOverlay {
  GstBaseTextOverlay textoverlay;

  /*< private >*/
  GstTimeOverlayTimeLine time_line;

  /* For datetime mode */
  gboolean show_times_as_dates;
  guint    buffer_count;
  gboolean show_buffer_count;
  gchar *datetime_format;
  GDateTime *datetime_epoch;

  GstCaps *reference_timestamp_caps;

  /* For GST_TIME_OVERLAY_TIME_LINE_ELAPSED_RUNNING_TIME mode */
  GstClockTime first_running_time;
  GstPadEventFunction orig_video_event;
};

struct _GstTimeOverlayClass {
  GstBaseTextOverlayClass parent_class;
};

GType gst_time_overlay_get_type (void);

G_END_DECLS

#endif /* __GST_TIME_OVERLAY_H__ */

