/*
 * GStreamer
 * Copyright (C) 2015 Julien Isorce <julien.isorce@gmail.com>
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

#ifndef __GST_GL_DISPLAY_COCOA_H__
#define __GST_GL_DISPLAY_COCOA_H__

#include <gst/gst.h>

#include <gst/gl/gstgl_fwd.h>
#include <gst/gl/gstgldisplay.h>

G_BEGIN_DECLS

GType gst_gl_display_cocoa_get_type (void);

#define GST_TYPE_GL_DISPLAY_COCOA             (gst_gl_display_cocoa_get_type())
#define GST_GL_DISPLAY_COCOA(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_GL_DISPLAY_COCOA,GstGLDisplayCocoa))
#define GST_GL_DISPLAY_COCOA_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_GL_DISPLAY_COCOA,GstGLDisplayCocoaClass))
#define GST_IS_GL_DISPLAY_COCOA(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_GL_DISPLAY_COCOA))
#define GST_IS_GL_DISPLAY_COCOA_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_GL_DISPLAY_COCOA))
#define GST_GL_DISPLAY_COCOA_CAST(obj)        ((GstGLDisplayCocoa*)(obj))

typedef struct _GstGLDisplayCocoa GstGLDisplayCocoa;
typedef struct _GstGLDisplayCocoaClass GstGLDisplayCocoaClass;

/**
 * GstGLDisplayCocoa:
 *
 * Initialized NSApp if the application has not done it.
 */
struct _GstGLDisplayCocoa
{
  GstGLDisplay          parent;
};

struct _GstGLDisplayCocoaClass
{
  GstGLDisplayClass object_class;
};

GstGLDisplayCocoa *gst_gl_display_cocoa_new (void);

G_END_DECLS

#endif /* __GST_GL_DISPLAY_COCOA_H__ */
