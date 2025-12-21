/*
 * GStreamer
 * Copyright (C) 2014 Jan Schmidt <jan@centricular.com>
 * Copyright (C) 2015 Matthew Waters <matthew@centricular.com>
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

#ifndef _GST_GL_VIEW_CONVERT_ELEMENT_H_
#define _GST_GL_VIEW_CONVERT_ELEMENT_H_

#include <gst/gl/gstglfilter.h>

G_BEGIN_DECLS
#define GST_TYPE_GL_VIEW_CONVERT_ELEMENT            (gst_gl_view_convert_element_get_type())
#define GST_GL_VIEW_CONVERT_ELEMENT(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_GL_VIEW_CONVERT_ELEMENT,GstGLViewConvertElement))
#define GST_IS_GL_VIEW_CONVERT_ELEMENT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_GL_VIEW_CONVERT_ELEMENT))
#define GST_GL_VIEW_CONVERT_ELEMENT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass) ,GST_TYPE_GL_VIEW_CONVERT_ELEMENT,GstGLViewConvertElementClass))
#define GST_IS_GL_VIEW_CONVERT_ELEMENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass) ,GST_TYPE_GL_VIEW_CONVERT_ELEMENT))
#define GST_GL_VIEW_CONVERT_ELEMENT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj) ,GST_TYPE_GL_VIEW_CONVERT_ELEMENT,GstGLViewConvertElementClass))

typedef struct _GstGLViewConvertElement GstGLViewConvertElement;
typedef struct _GstGLViewConvertElementClass GstGLViewConvertElementClass;

struct _GstGLViewConvertElement
{
  GstGLFilter filter;

  GstGLViewConvert *viewconvert;
};

struct _GstGLViewConvertElementClass
{
  GstGLFilterClass filter_class;
};

GType gst_gl_view_convert_element_get_type (void);

G_END_DECLS
#endif /* _GST_GL_VIEW_CONVERT_H_ */
