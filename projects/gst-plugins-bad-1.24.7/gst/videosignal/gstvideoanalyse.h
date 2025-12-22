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

#ifndef _GST_VIDEO_ANALYSE_H_
#define _GST_VIDEO_ANALYSE_H_

#include <gst/video/video.h>
#include <gst/video/gstvideofilter.h>

G_BEGIN_DECLS

#define GST_TYPE_VIDEO_ANALYSE   (gst_video_analyse_get_type())
#define GST_VIDEO_ANALYSE(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_VIDEO_ANALYSE,GstVideoAnalyse))
#define GST_VIDEO_ANALYSE_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_VIDEO_ANALYSE,GstVideoAnalyseClass))
#define GST_IS_VIDEO_ANALYSE(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_VIDEO_ANALYSE))
#define GST_IS_VIDEO_ANALYSE_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_VIDEO_ANALYSE))

typedef struct _GstVideoAnalyse GstVideoAnalyse;
typedef struct _GstVideoAnalyseClass GstVideoAnalyseClass;

struct _GstVideoAnalyse
{
  GstVideoFilter base_videoanalyse;

  /* properties */
  gboolean message;
  guint64 interval;
  gdouble luma_average;
  gdouble luma_variance;
};

struct _GstVideoAnalyseClass
{
  GstVideoFilterClass base_videoanalyse_class;
};

GType gst_video_analyse_get_type (void);

GST_ELEMENT_REGISTER_DECLARE (videoanalyse);

G_END_DECLS

#endif
