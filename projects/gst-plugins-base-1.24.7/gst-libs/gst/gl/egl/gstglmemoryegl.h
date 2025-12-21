/*
 * GStreamer
 * Copyright (C) 2012 Collabora Ltd.
 *   @author: Sebastian Dröge <sebastian.droege@collabora.co.uk>
 * Copyright (C) 2014 Julien Isorce <julien.isorce@gmail.com>
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

#ifndef _GST_GL_MEMORY_EGL_H_
#define _GST_GL_MEMORY_EGL_H_

#include <gst/gl/gstglmemory.h>
#include <gst/gl/egl/gsteglimage.h>
#include <gst/gl/egl/gstgldisplay_egl.h>

G_BEGIN_DECLS

#define GST_TYPE_GL_MEMORY_EGL_ALLOCATOR (gst_gl_memory_egl_allocator_get_type())
GST_GL_API GType gst_gl_memory_egl_allocator_get_type(void);

#define GST_IS_GL_MEMORY_EGL_ALLOCATOR(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_GL_MEMORY_EGL_ALLOCATOR))
#define GST_IS_GL_MEMORY_EGL_ALLOCATOR_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_GL_MEMORY_EGL_ALLOCATOR))
#define GST_GL_MEMORY_EGL_ALLOCATOR_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_GL_MEMORY_EGL_ALLOCATOR, GstGLMemoryEGLAllocatorClass))
#define GST_GL_MEMORY_EGL_ALLOCATOR(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_GL_MEMORY_EGL_ALLOCATOR, GstGLMemoryEGLAllocator))
#define GST_GL_MEMORY_EGL_ALLOCATOR_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_GL_MEMORY_EGL_ALLOCATOR, GstGLMemoryEGLAllocatorClass))
#define GST_GL_MEMORY_EGL_ALLOCATOR_CAST(obj)            ((GstGLMemoryEGLAllocator *)(obj))

typedef struct _GstGLMemoryEGL GstGLMemoryEGL;
typedef struct _GstGLMemoryEGLAllocator GstGLMemoryEGLAllocator;
typedef struct _GstGLMemoryEGLAllocatorClass GstGLMemoryEGLAllocatorClass;

/**
 * GstGLMemoryEGL:
 *
 * Private instance
 */
struct _GstGLMemoryEGL
{
  /*< private >*/
  GstGLMemory mem;

  GstEGLImage *image;

  gpointer _padding[GST_PADDING];
};

/**
 * GST_GL_MEMORY_EGL_ALLOCATOR_NAME:
 *
 * The name of the GL Memory EGL allocator
 */
#define GST_GL_MEMORY_EGL_ALLOCATOR_NAME "GLMemoryEGL"

/**
 * GST_TYPE_GL_MEMORY_EGL:
 *
 * Since: 1.20
 * Deprecated: 1.22: This type has no use.
 */
#define GST_TYPE_GL_MEMORY_EGL (gst_gl_memory_egl_get_type())
GST_GL_DEPRECATED
GType gst_gl_memory_egl_get_type(void);

GST_GL_API
void          gst_gl_memory_egl_init_once               (void);

GST_GL_API
gboolean      gst_is_gl_memory_egl                      (GstMemory * mem);

GST_GL_API
gpointer      gst_gl_memory_egl_get_image               (GstGLMemoryEGL * mem);

GST_GL_API
gpointer      gst_gl_memory_egl_get_display             (GstGLMemoryEGL * mem);

/**
 * GstGLMemoryEGLAllocator:
 *
 * Opaque #GstGLMemoryEGLAllocator struct
 */
struct _GstGLMemoryEGLAllocator
{
  /*< private >*/

  GstGLMemoryAllocator parent;

  gpointer _padding[GST_PADDING];
};

/**
 * GstGLMemoryEGLAllocatorClass:
 *
 * The #GstGLMemoryEGLAllocatorClass only contains private data
 */
struct _GstGLMemoryEGLAllocatorClass
{
  /*< private >*/
  GstGLMemoryAllocatorClass parent_class;

  gpointer _padding[GST_PADDING];
};

G_END_DECLS

#endif /* _GST_GL_MEMORY_EGL_H_ */
