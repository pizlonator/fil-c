/* 
 * GStreamer
 * Copyright (C) 2009 Julien Isorce <julien.isorce@gmail.com>
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

#ifndef _GST_GL_DEINTERLACE_H_
#define _GST_GL_DEINTERLACE_H_

#include <gst/gl/gstglfilter.h>

G_BEGIN_DECLS

#define GST_TYPE_GL_DEINTERLACE            (gst_gl_deinterlace_get_type())
#define GST_GL_DEINTERLACE(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_GL_DEINTERLACE,GstGLDeinterlace))
#define GST_IS_GL_DEINTERLACE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_GL_DEINTERLACE))
#define GST_GL_DEINTERLACE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass) ,GST_TYPE_GL_DEINTERLACE,GstGLDeinterlaceClass))
#define GST_IS_GL_DEINTERLACE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass) ,GST_TYPE_GL_DEINTERLACE))
#define GST_GL_DEINTERLACE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj) ,GST_TYPE_GL_DEINTERLACE,GstGLDeinterlaceClass))

#define USING_OPENGL(context) (gst_gl_context_check_gl_version (context, GST_GL_API_OPENGL, 1, 0))

typedef struct _GstGLDeinterlace GstGLDeinterlace;
typedef struct _GstGLDeinterlaceClass GstGLDeinterlaceClass;

struct _GstGLDeinterlace
{
  GstGLFilter               filter;

  GstGLFilterRenderFunc     deinterlacefunc; 
  GHashTable               *shaderstable;
  GstBuffer                *prev_buffer;
  GstGLMemory *             prev_tex;

  gint	                    current_method;
};

struct _GstGLDeinterlaceClass
{
  GstGLFilterClass filter_class;
};

GType gst_gl_deinterlace_get_type (void);

G_END_DECLS

#endif /* _GST_GLFILTERLAPLACIAN_H_ */
