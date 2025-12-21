/*
 * GStreamer
 * Copyright (C) 2015 Jan Schmidt <jan@centricular.com>
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

#ifndef __GST_GL_STEREOSPLIT_H__
#define __GST_GL_STEREOSPLIT_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/gl/gl.h>

G_BEGIN_DECLS

#define GST_TYPE_GL_STEREOSPLIT            (gst_gl_stereosplit_get_type())
#define GST_GL_STEREOSPLIT(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_GL_STEREOSPLIT,GstGLStereoSplit))
#define GST_IS_GL_STEREOSPLIT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_GL_STEREOSPLIT))
#define GST_GL_STEREOSPLIT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass) ,GST_TYPE_GL_STEREOSPLIT,GstGLStereoSplitClass))
#define GST_IS_GL_STEREOSPLIT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass) ,GST_TYPE_GL_STEREOSPLIT))
#define GST_GL_STEREOSPLIT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj) ,GST_TYPE_GL_STEREOSPLIT,GstGLStereoSplitClass))

typedef struct _GstGLStereoSplit GstGLStereoSplit;
typedef struct _GstGLStereoSplitClass GstGLStereoSplitClass;

struct _GstGLStereoSplit
{
  GstElement parent;

  GstPad *sink_pad;
  GstPad *left_pad;
  GstPad *right_pad;

  GstGLDisplay      *display;
  GstGLContext      *context;
  GstGLContext      *other_context;

  GstGLViewConvert    *viewconvert;

  GRecMutex            context_lock;
};

struct _GstGLStereoSplitClass
{
  GstElementClass parent_class;
};

GType gst_gl_stereosplit_get_type (void);

G_END_DECLS

#endif
