/*
 * GStreamer
 * Copyright (C) 2003 Julien Moutte <julien@moutte.net>
 * Copyright (C) 2005,2006,2007 David A. Schleef <ds@schleef.org>
 * Copyright (C) 2008 Julien Isorce <julien.isorce@gmail.com>
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

#ifndef _GST_GL_SINK_BIN_H_
#define _GST_GL_SINK_BIN_H_

#include <gst/gst.h>
#include <gst/video/gstvideosink.h>
#include <gst/video/video.h>

#include <gst/gl/gl.h>

G_BEGIN_DECLS

GType gst_gl_sink_bin_get_type(void);
#define GST_TYPE_GL_SINK_BIN \
    (gst_gl_sink_bin_get_type())
#define GST_GL_SINK_BIN(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_GL_SINK_BIN,GstGLSinkBin))
#define GST_GL_SINK_BIN_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_GL_SINK_BIN,GstGLSinkBinClass))
#define GST_IS_GL_SINK_BIN(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_GL_SINK_BIN))
#define GST_IS_GL_SINK_BIN_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_GL_SINK_BIN))
#define GST_GL_SINK_BIN_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj) ,GST_TYPE_GL_SINK_BIN,GstGLSinkBinClass))

typedef struct _GstGLSinkBin GstGLSinkBin;
typedef struct _GstGLSinkBinClass GstGLSinkBinClass;

struct _GstGLSinkBin
{
  GstBin parent;

  GstPad *sinkpad;

  GstElement *upload;
  GstElement *convert;
  GstElement *balance;
  GstElement *sink;
};

struct _GstGLSinkBinClass
{
  GstBinClass parent_class;

  GstElement * (*create_element) (void);
};

void gst_gl_sink_bin_finish_init (GstGLSinkBin * self);
void gst_gl_sink_bin_finish_init_with_element (GstGLSinkBin * self,
    GstElement * element);

G_END_DECLS

#endif
