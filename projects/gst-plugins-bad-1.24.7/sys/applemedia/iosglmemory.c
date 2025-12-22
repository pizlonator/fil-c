/*
 * GStreamer
 * Copyright (C) 2015 Alessandro Decina <twi@centricular.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "iosglmemory.h"

GST_DEBUG_CATEGORY_STATIC (GST_CAT_IOS_GL_MEMORY);
#define GST_CAT_DEFAULT GST_CAT_IOS_GL_MEMORY

G_DEFINE_TYPE (GstIOSGLMemoryAllocator, gst_ios_gl_memory_allocator,
    GST_TYPE_GL_MEMORY_ALLOCATOR);

typedef struct
{
  GstIOSGLMemory *memory;
} ContextThreadData;

static GstAllocator *_ios_gl_memory_allocator;

static void
_ios_gl_memory_destroy (GstGLBaseMemory * gl_mem)
{
  GstIOSGLMemory *mem = (GstIOSGLMemory *) gl_mem;

  mem->gl_notify (mem->gl_data);
  gst_memory_unref (GST_MEMORY_CAST (mem->cv_mem));
  GST_GL_BASE_MEMORY_ALLOCATOR_CLASS
      (gst_ios_gl_memory_allocator_parent_class)->destroy (gl_mem);
}

static gpointer
_ios_gl_memory_allocator_map (GstGLBaseMemory * bmem,
    GstMapInfo * info, gsize size)
{
  GstGLMemory *gl_mem = (GstGLMemory *) bmem;
  GstIOSGLMemory *mem = (GstIOSGLMemory *) gl_mem;

  if (info->flags & GST_MAP_GL)
    return &gl_mem->tex_id;
  return GST_MEMORY_CAST (mem->cv_mem)->allocator->
      mem_map (GST_MEMORY_CAST (mem->cv_mem), size, info->flags);
}

static void
_ios_gl_memory_allocator_unmap (GstGLBaseMemory * bmem, GstMapInfo * info)
{
  GstIOSGLMemory *mem = (GstIOSGLMemory *) bmem;

  if (!(info->flags & GST_MAP_GL))
    GST_MEMORY_CAST (mem->cv_mem)->allocator->
        mem_unmap (GST_MEMORY_CAST (mem->cv_mem));
}

static GstMemory *
_mem_alloc (GstAllocator * allocator, gsize size, GstAllocationParams * params)
{
  g_warning ("use gst_ios_gl_memory_new_wrapped () to allocate from this "
      "IOSGL allocator");

  return NULL;
}

static void
gst_ios_gl_memory_allocator_class_init (GstIOSGLMemoryAllocatorClass * klass)
{
  GstAllocatorClass *allocator_class = (GstAllocatorClass *) klass;
  GstGLBaseMemoryAllocatorClass *gl_base_allocator_class =
      (GstGLBaseMemoryAllocatorClass *) klass;

  allocator_class->alloc = _mem_alloc;
  gl_base_allocator_class->destroy = _ios_gl_memory_destroy;
  gl_base_allocator_class->map = _ios_gl_memory_allocator_map;
  gl_base_allocator_class->unmap = _ios_gl_memory_allocator_unmap;
}

static void
gst_ios_gl_memory_allocator_init (GstIOSGLMemoryAllocator * allocator)
{
  GstAllocator *alloc = GST_ALLOCATOR_CAST (allocator);

  alloc->mem_type = GST_IOS_GL_MEMORY_ALLOCATOR_NAME;
  GST_OBJECT_FLAG_SET (allocator, GST_ALLOCATOR_FLAG_CUSTOM_ALLOC);
}

void
gst_ios_gl_memory_init (void)
{
  static gsize _init = 0;

  if (g_once_init_enter (&_init)) {
    GST_DEBUG_CATEGORY_INIT (GST_CAT_IOS_GL_MEMORY, "iosurface", 0,
        "IOSGL Buffer");

    _ios_gl_memory_allocator =
        g_object_new (GST_TYPE_IOS_GL_MEMORY_ALLOCATOR, NULL);
    gst_object_ref_sink (_ios_gl_memory_allocator);

    gst_allocator_register (GST_IOS_GL_MEMORY_ALLOCATOR_NAME,
        gst_object_ref (_ios_gl_memory_allocator));
    g_once_init_leave (&_init, 1);
  }
}

gboolean
gst_is_ios_gl_memory (GstMemory * mem)
{
  return mem != NULL && mem->allocator != NULL &&
      g_type_is_a (G_OBJECT_TYPE (mem->allocator),
      GST_TYPE_IOS_GL_MEMORY_ALLOCATOR);
}

static GstIOSGLMemory *
_ios_gl_memory_new (GstGLContext * context,
    GstAppleCoreVideoMemory * cv_mem,
    GstGLTextureTarget target,
    GstGLFormat tex_format,
    guint tex_id,
    GstVideoInfo * info,
    guint plane,
    GstVideoAlignment * valign, gpointer gl_data, GDestroyNotify gl_notify)
{
  GstIOSGLMemory *mem;

  mem = g_new0 (GstIOSGLMemory, 1);
  mem->gl_mem.tex_id = tex_id;
  mem->gl_mem.texture_wrapped = TRUE;
  gst_gl_memory_init (&mem->gl_mem, _ios_gl_memory_allocator, NULL, context,
      target, tex_format, NULL, info, plane, valign, NULL, NULL);
  mem->cv_mem = cv_mem;
  mem->gl_data = gl_data;
  mem->gl_notify = gl_notify;

  GST_MINI_OBJECT_FLAG_SET (mem, GST_MEMORY_FLAG_READONLY);

  return mem;
}

GstIOSGLMemory *
gst_ios_gl_memory_new_wrapped (GstGLContext * context,
    GstAppleCoreVideoMemory * cv_mem,
    GstGLTextureTarget target,
    GstGLFormat tex_format,
    guint tex_id,
    GstVideoInfo * info,
    guint plane,
    GstVideoAlignment * valign, gpointer gl_data, GDestroyNotify gl_notify)
{
  return _ios_gl_memory_new (context, cv_mem, target, tex_format, tex_id, info,
      plane, valign, gl_data, gl_notify);
}
