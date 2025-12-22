/*
 * GStreamer
 * Copyright (C) 2013 Miguel Casas-Sanchez <miguelecasassanchez@gmail.com>
 *
 * This library is free software; you can
 * redistribute it and/or modify it under the terms of the GNU Library
 * General Public License as published by the Free Software Foundation;
 * either version 2 of the License, or (at your option) any later version.
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Library
 * General Public License for more details. You should have received a copy
 * of the GNU Library General Public License along with this library; if
 * not, write to the Free Software Foundation, Inc., 51 Franklin St,
 * Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef __GST_OPENNI2_SRC_H__
#define __GST_OPENNI2_SRC_H__

#include <gst/gst.h>
#include <stdio.h>
#include <OpenNI.h>

#include <gst/base/gstbasesrc.h>
#include <gst/base/gstpushsrc.h>
#include <gst/video/video.h>

G_BEGIN_DECLS
#define GST_TYPE_OPENNI2_SRC \
  (gst_openni2_src_get_type())
#define GST_OPENNI2_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_OPENNI2_SRC,GstOpenni2Src))
#define GST_OPENNI2_SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_OPENNI2_SRC,GstOpenni2SrcClass))
#define GST_IS_OPENNI2_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_OPENNI2_SRC))
#define GST_IS_OPENNI2_SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_OPENNI2_SRC))
typedef struct _GstOpenni2Src GstOpenni2Src;
typedef struct _GstOpenni2SrcClass GstOpenni2SrcClass;

typedef enum
{
  GST_OPENNI2_SRC_FILE_TRANSFER,
  GST_OPENNI2_SRC_NEXT_PROGRAM_CHAIN,
  GST_OPENNI2_SRC_INVALID_DATA
} GstOpenni2State;

struct _GstOpenni2Src
{
  GstPushSrc element;

  GstOpenni2State state;
  gchar *uri_name;
  gint sourcetype;
  GstVideoInfo info;
  GstCaps *gst_caps;

  /* Timestamp of the first frame */
  GstClockTime oni_start_ts;

  /* OpenNI2 variables */
  openni::Device *device;
  openni::VideoStream *depth, *color;
  openni::VideoMode depthVideoMode, colorVideoMode;
  openni::PixelFormat depthpixfmt, colorpixfmt;
  int width, height, fps;
  openni::VideoFrameRef *depthFrame, *colorFrame;
};

struct _GstOpenni2SrcClass
{
  GstPushSrcClass parent_class;
};

GType gst_openni2_src_get_type (void);

GST_ELEMENT_REGISTER_DECLARE (openni2src);

G_END_DECLS
#endif /* __GST_OPENNI2_SRC_H__ */
