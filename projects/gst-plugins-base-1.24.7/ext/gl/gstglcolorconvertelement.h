/*
 * GStreamer
 * Copyright (C) 2012 Matthew Waters <ystree00@gmail.com>
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

#ifndef __GST_GL_COLOR_CONVERT_ELEMENT_H__
#define __GST_GL_COLOR_CONVERT_ELEMENT_H__

#include <gst/video/video.h>
#include <gst/gstmemory.h>

#include <gst/gl/gstgl_fwd.h>

G_BEGIN_DECLS

GType gst_gl_color_convert_element_get_type (void);
#define GST_TYPE_GL_COLOR_CONVERT_ELEMENT (gst_gl_color_convert_element_get_type())
#define GST_GL_COLOR_CONVERT_ELEMENT(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_GL_COLOR_CONVERT_ELEMENT,GstGLColorConvertElement))
#define GST_GL_COLOR_CONVERT_ELEMENT_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_GL_DISPLAY,GstGLColorConvertElementClass))
#define GST_IS_GL_COLOR_CONVERT_ELEMENT(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_GL_COLOR_CONVERT_ELEMENT))
#define GST_IS_GL_COLOR_CONVERT_ELEMENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_GL_COLOR_CONVERT_ELEMENT))
#define GST_GL_COLOR_CONVERT_ELEMENT_CAST(obj) ((GstGLColorConvertElement*)(obj))

typedef struct _GstGLColorConvertElement GstGLColorConvertElement;
typedef struct _GstGLColorConvertElementClass GstGLColorConvertElementClass;
typedef struct _GstGLColorConvertElementPrivate GstGLColorConvertElementPrivate;

struct _GstGLColorConvertElement
{
  GstGLBaseFilter        parent;

  GstGLColorConvert *convert;
};

struct _GstGLColorConvertElementClass
{
  GstGLBaseFilterClass object_class;
};

G_END_DECLS

#endif /* __GST_GL_COLOR_CONVERT_ELEMENT_H__ */
