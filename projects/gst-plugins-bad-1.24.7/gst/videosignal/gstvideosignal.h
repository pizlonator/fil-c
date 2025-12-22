/* GStreamer
 * Copyright (C) <2006> Wim Taymans <wim@fluendo.com>
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

#ifndef __GST_VIDEO_SIGNAL_H__
#define __GST_VIDEO_SIGNAL_H__

#include <gst/video/gstvideofilter.h>

G_BEGIN_DECLS

#define GST_TYPE_VIDEO_SIGNAL \
  (gst_video_signal_get_type())
#define GST_VIDEO_SIGNAL(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_VIDEO_SIGNAL,GstVideoSignal))
#define GST_VIDEO_SIGNAL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_VIDEO_SIGNAL,GstVideoSignalClass))
#define GST_IS_VIDEO_SIGNAL(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_VIDEO_SIGNAL))
#define GST_IS_VIDEO_SIGNAL_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_VIDEO_SIGNAL))

typedef struct _GstVideoSignal GstVideoSignal;
typedef struct _GstVideoSignalClass GstVideoSignalClass;

/**
 * GstVideoSignal:
 *
 * Opaque datastructure.
 */
struct _GstVideoSignal {
  GstVideoFilter videofilter;
  
  gint width, height;

  gdouble brightness;
  gdouble brightness_var;

  guint64 interval;
};

struct _GstVideoSignalClass {
  GstVideoFilterClass parent_class;
};

GType gst_video_signal_get_type (void);

G_END_DECLS

#endif /* __GST_VIDEO_SIGNAL_H__ */
