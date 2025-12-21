/*
 * glshader gstreamer plugin
 * Copyright (C) 2018 Matthew Waters <matthew@centricular.com>
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

#ifndef _GST_GL_OVERLAY_COMPOSITOR_ELEMENT_H_
#define _GST_GL_OVERLAY_COMPOSITOR_ELEMENT_H_

#include <gst/gl/gstglfilter.h>

#define GST_TYPE_GL_OVERLAY_COMPOSITOR_ELEMENT            (gst_gl_overlay_compositor_element_get_type())
#define GST_GL_OVERLAY_COMPOSITOR_ELEMENT(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_GL_OVERLAY_COMPOSITOR_ELEMENT,GstGLOverlayCompositorElement))
#define GST_IS_GL_OVERLAY_COMPOSITOR_ELEMENT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_GL_OVERLAY_COMPOSITOR_ELEMENT))
#define GST_GL_OVERLAY_COMPOSITOR_ELEMENT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass) ,GST_TYPE_GL_OVERLAY_COMPOSITOR_ELEMENT,GstGLOverlayCompositorElementClass))
#define GST_IS_GL_OVERLAY_COMPOSITOR_ELEMENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass) ,GST_TYPE_GL_OVERLAY_COMPOSITOR_ELEMENT))
#define GST_GL_OVERLAY_COMPOSITOR_ELEMENT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj) ,GST_TYPE_GL_OVERLAY_COMPOSITOR_ELEMENT,GstGLOverlayCompositorElementClass))

typedef struct _GstGLOverlayCompositorElement GstGLOverlayCompositorElement;
typedef struct _GstGLOverlayCompositorElementClass GstGLOverlayCompositorElementClass;

struct _GstGLOverlayCompositorElement
{
  GstGLFilter filter;

  GstGLShader *shader;
  GstGLOverlayCompositor *overlay_compositor;
};

struct _GstGLOverlayCompositorElementClass
{
  GstGLFilterClass filter_class;
};

GType gst_gl_overlay_compositor_element_get_type (void);

#endif /* _GST_GL_OVERLAY_COMPOSITOR_ELEMENT_H_ */
