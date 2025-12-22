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

#ifndef _GST_SIMPLE_VIDEO_MARK_H_
#define _GST_SIMPLE_VIDEO_MARK_H_

#include <gst/video/video.h>
#include <gst/video/gstvideofilter.h>

G_BEGIN_DECLS

#define GST_TYPE_SIMPLE_VIDEO_MARK   (gst_video_mark_get_type())
#define GST_SIMPLE_VIDEO_MARK(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_SIMPLE_VIDEO_MARK,GstSimpleVideoMark))
#define GST_SIMPLE_VIDEO_MARK_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_SIMPLE_VIDEO_MARK,GstSimpleVideoMarkClass))
#define GST_IS_SIMPLE_VIDEO_MARK(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_SIMPLE_VIDEO_MARK))
#define GST_IS_SIMPLE_VIDEO_MARK_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_SIMPLE_VIDEO_MARK))

typedef struct _GstSimpleVideoMark GstSimpleVideoMark;
typedef struct _GstSimpleVideoMarkClass GstSimpleVideoMarkClass;

struct _GstSimpleVideoMark
{
  GstVideoFilter base_simplevideomark;

  /* properties */
  gint pattern_width;
  gint pattern_height;
  gint pattern_count;
  gint pattern_data_count;
  guint64 pattern_data;
  gboolean enabled;
  gint left_offset;
  gint bottom_offset;
};

struct _GstSimpleVideoMarkClass
{
  GstVideoFilterClass base_simplevideomark_class;
};

GType gst_video_mark_get_type (void);

GST_ELEMENT_REGISTER_DECLARE (simplevideomark);

G_END_DECLS

#endif
