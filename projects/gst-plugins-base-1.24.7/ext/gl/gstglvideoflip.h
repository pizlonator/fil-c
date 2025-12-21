/*
 * GStreamer
 * Copyright (C) 2016 Matthew Waters <matthew@centricular.com>
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

#ifndef _GST_GL_VIDEO_FLIP_H_
#define _GST_GL_VIDEO_FLIP_H_

#include <gst/gl/gl.h>
#include <gst/video/video.h>

G_BEGIN_DECLS

#define GST_TYPE_GL_VIDEO_FLIP            (gst_gl_video_flip_get_type())
#define GST_GL_VIDEO_FLIP(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_GL_VIDEO_FLIP,GstGLVideoFlip))
#define GST_IS_GL_VIDEO_FLIP(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_GL_VIDEO_FLIP))
#define GST_GL_VIDEO_FLIP_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass) ,GST_TYPE_GL_VIDEO_FLIP,GstGLVideoFlipClass))
#define GST_IS_GL_VIDEO_FLIP_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass) ,GST_TYPE_GL_VIDEO_FLIP))
#define GST_GL_VIDEO_FLIP_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj) ,GST_TYPE_GL_VIDEO_FLIP,GstGLVideoFlipClass))


typedef struct _GstGLVideoFlip GstGLVideoFlip;
typedef struct _GstGLVideoFlipClass GstGLVideoFlipClass;

struct _GstGLVideoFlip
{
  GstBin        bin;

  GstPad       *srcpad;
  GstPad       *sinkpad;

  GstElement   *input_capsfilter;
  GstElement   *transformation;
  GstElement   *output_capsfilter;

  gulong        sink_probe;
  gulong        src_probe;

  GstCaps      *input_caps;

  /* properties */
  GstVideoOrientationMethod method;
  GstVideoOrientationMethod tag_method;
  GstVideoOrientationMethod active_method;

  gfloat aspect;
};

struct _GstGLVideoFlipClass
{
  GstBinClass filter_class;
};

GType gst_gl_video_flip_get_type (void);

G_END_DECLS

#endif /* _GST_GL_VIDEO_FLIP_H_ */
