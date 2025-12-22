/*
 * GStreamer
 * Copyright (C) 2016 Alessandro Decina <twi@centricular.com>
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

#ifndef _GST_IOS_GL_MEMORY_H_
#define _GST_IOS_GL_MEMORY_H_

#include <gst/gst.h>
#include <gst/gstallocator.h>
#include <gst/video/video.h>
#include <gst/gl/gl.h>
#include <gst/gl/gstglfuncs.h>
#include "corevideomemory.h"

G_BEGIN_DECLS

#define GST_TYPE_IOS_GL_MEMORY_ALLOCATOR (gst_ios_gl_memory_allocator_get_type())
GType gst_ios_gl_memory_allocator_get_type(void);

#define GST_IS_IOS_GL_MEMORY_ALLOCATOR(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_IOS_GL_MEMORY_ALLOCATOR))
#define GST_IS_IOS_GL_MEMORY_ALLOCATOR_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_IOS_GL_MEMORY_ALLOCATOR))
#define GST_IOS_GL_MEMORY_ALLOCATOR_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_IOS_GL_MEMORY_ALLOCATOR, GstIOSGLMemoryAllocatorClass))
#define GST_IOS_GL_MEMORY_ALLOCATOR(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_IOS_GL_MEMORY_ALLOCATOR, GstIOSGLMemoryAllocator))
#define GST_IOS_GL_MEMORY_ALLOCATOR_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_IOS_GL_MEMORY_ALLOCATOR, GstIOSGLMemoryAllocatorClass))
#define GST_IOS_GL_MEMORY_ALLOCATOR_CAST(obj)            ((GstIOSGLMemoryAllocator *)(obj))

typedef struct _GstIOSGLMemory
{
  GstGLMemory gl_mem;
  GstAppleCoreVideoMemory *cv_mem;
  gpointer gl_data;
  GDestroyNotify gl_notify;
} GstIOSGLMemory;

#define GST_IOS_GL_MEMORY_ALLOCATOR_NAME   "IOSGLMemory"

void gst_ios_gl_memory_init (void);

GstIOSGLMemory *
gst_ios_gl_memory_new_wrapped (GstGLContext * context,
    GstAppleCoreVideoMemory *cv_mem,
    GstGLTextureTarget target,
    GstGLFormat tex_format,
    guint tex_id,
    GstVideoInfo * info,
    guint plane,
    GstVideoAlignment *valign,
    gpointer gl_data,
    GDestroyNotify gl_notify);

gboolean gst_is_ios_gl_memory (GstMemory * mem);

typedef struct _GstIOSGLMemoryAllocator
{
  GstGLMemoryAllocator allocator;
} GstIOSGLMemoryAllocator;

typedef struct _GstIOSGLMemoryAllocatorClass
{
  GstGLMemoryAllocatorClass parent_class;
} GstIOSGLMemoryAllocatorClass;

G_END_DECLS

#endif /* _GST_IOS_GL_MEMORY_H_ */
