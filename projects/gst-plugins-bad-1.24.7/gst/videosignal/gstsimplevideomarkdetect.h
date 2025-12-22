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

#ifndef _GST_SIMPLE_VIDEO_MARK_DETECT_H_
#define _GST_SIMPLE_VIDEO_MARK_DETECT_H_

#include <gst/video/video.h>
#include <gst/video/gstvideofilter.h>

G_BEGIN_DECLS

#define GST_TYPE_SIMPLE_VIDEO_MARK_DETECT   (gst_video_detect_get_type())
#define GST_SIMPLE_VIDEO_MARK_DETECT(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_SIMPLE_VIDEO_MARK_DETECT,GstSimpleVideoMarkDetect))
#define GST_SIMPLE_VIDEO_MARK_DETECT_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_SIMPLE_VIDEO_MARK_DETECT,GstSimpleVideoMarkDetectClass))
#define GST_IS_SIMPLE_VIDEO_MARK_DETECT(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_SIMPLE_VIDEO_MARK_DETECT))
#define GST_IS_SIMPLE_VIDEO_MARK_DETECT_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_SIMPLE_VIDEO_MARK_DETECT))

typedef struct _GstSimpleVideoMarkDetect GstSimpleVideoMarkDetect;
typedef struct _GstSimpleVideoMarkDetectClass GstSimpleVideoMarkDetectClass;

struct _GstSimpleVideoMarkDetect
{
  GstVideoFilter base_simplevideomarkdetect;

  gboolean message;
  gint pattern_width;
  gint pattern_height;
  gint pattern_count;
  gint pattern_data_count;
  gdouble pattern_center;
  gdouble pattern_sensitivity;
  gint left_offset;
  gint bottom_offset;

  gboolean in_pattern;
};

struct _GstSimpleVideoMarkDetectClass
{
  GstVideoFilterClass base_simplevideomarkdetect_class;
};

GType gst_video_detect_get_type (void);

GST_ELEMENT_REGISTER_DECLARE (simplevideomarkdetect);

G_END_DECLS

#endif
