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

#include "iosurfaceglmemory.h"

GST_DEBUG_CATEGORY_STATIC (GST_CAT_IO_SURFACE_GL_MEMORY);
#define GST_CAT_DEFAULT GST_CAT_IO_SURFACE_GL_MEMORY

G_DEFINE_TYPE (GstIOSurfaceGLMemoryAllocator,
    gst_io_surface_gl_memory_allocator, GST_TYPE_GL_MEMORY_ALLOCATOR);

typedef struct
{
  GstIOSurfaceGLMemory *memory;
  IOSurfaceRef surface;
} ContextThreadData;

static void _io_surface_gl_memory_set_surface (GstIOSurfaceGLMemory * memory,
    IOSurfaceRef surface);

static GstAllocator *_io_surface_gl_memory_allocator;

static gboolean
_io_surface_gl_memory_create (GstGLBaseMemory * bmem, GError ** error)
{
  GstGLMemory *gl_mem = (GstGLMemory *) bmem;
  GstGLContext *context = gl_mem->mem.context;
  const GstGLFuncs *gl = context->gl_vtable;
  GLuint target;

  target = gst_gl_texture_target_to_gl (gl_mem->tex_target);
  gl->GenTextures (1, &gl_mem->tex_id);
  gl->BindTexture (target, gl_mem->tex_id);
  gl->BindTexture (target, 0);

  GST_LOG ("generated texture id:%d", gl_mem->tex_id);

  return TRUE;
}

static void
_io_surface_gl_memory_destroy (GstGLBaseMemory * gl_mem)
{
  GST_GL_BASE_MEMORY_ALLOCATOR_CLASS
      (gst_io_surface_gl_memory_allocator_parent_class)->destroy (gl_mem);
  _io_surface_gl_memory_set_surface ((GstIOSurfaceGLMemory *) gl_mem, NULL);
}

static gpointer
_io_surface_gl_memory_allocator_map (GstGLBaseMemory * bmem,
    GstMapInfo * info, gsize size)
{
  GstGLMemory *gl_mem = (GstGLMemory *) bmem;
  GstIOSurfaceGLMemory *mem = (GstIOSurfaceGLMemory *) gl_mem;

  GST_LOG ("mapping surface %p flags %d gl? %d",
      mem->surface, info->flags, ((info->flags & GST_MAP_GL) != 0));

  if (info->flags & GST_MAP_GL) {
    return &gl_mem->tex_id;
  } else if (!(info->flags & GST_MAP_WRITE)) {
    IOSurfaceLock (mem->surface, kIOSurfaceLockReadOnly, NULL);
    return IOSurfaceGetBaseAddressOfPlane (mem->surface, gl_mem->plane);
  } else {
    GST_ERROR ("couldn't map IOSurface %p flags %d", mem->surface, info->flags);
    return NULL;
  }
}

static void
_io_surface_gl_memory_allocator_unmap (GstGLBaseMemory * bmem,
    GstMapInfo * info)
{
  GstGLMemory *gl_mem = (GstGLMemory *) bmem;
  GstIOSurfaceGLMemory *mem = (GstIOSurfaceGLMemory *) gl_mem;

  GST_LOG ("unmapping surface %p flags %d gl? %d",
      mem->surface, info->flags, ((info->flags & GST_MAP_GL) != 0));

  if (!(info->flags & GST_MAP_GL)) {
    IOSurfaceUnlock (mem->surface, kIOSurfaceLockReadOnly, NULL);
  }
}

static GstMemory *
_mem_alloc (GstAllocator * allocator, gsize size, GstAllocationParams * params)
{
  g_warning ("use gst_io_surface_gl_memory_wrapped () to allocate from this "
      "IOSurface allocator");

  return NULL;
}

static void
    gst_io_surface_gl_memory_allocator_class_init
    (GstIOSurfaceGLMemoryAllocatorClass * klass)
{
  GstAllocatorClass *allocator_class = (GstAllocatorClass *) klass;
  GstGLBaseMemoryAllocatorClass *gl_base_allocator_class =
      (GstGLBaseMemoryAllocatorClass *) klass;

  allocator_class->alloc = _mem_alloc;

  gl_base_allocator_class->create = _io_surface_gl_memory_create;
  gl_base_allocator_class->destroy = _io_surface_gl_memory_destroy;
  gl_base_allocator_class->map = _io_surface_gl_memory_allocator_map;
  gl_base_allocator_class->unmap = _io_surface_gl_memory_allocator_unmap;
}

static void
gst_io_surface_gl_memory_allocator_init (GstIOSurfaceGLMemoryAllocator *
    allocator)
{
  GstAllocator *alloc = GST_ALLOCATOR_CAST (allocator);

  alloc->mem_type = GST_IO_SURFACE_GL_MEMORY_ALLOCATOR_NAME;
  GST_OBJECT_FLAG_SET (allocator, GST_ALLOCATOR_FLAG_CUSTOM_ALLOC);
}

void
gst_ios_surface_gl_memory_init (void)
{
  static gsize _init = 0;

  if (g_once_init_enter (&_init)) {
    GST_DEBUG_CATEGORY_INIT (GST_CAT_IO_SURFACE_GL_MEMORY, "iosurfacegl", 0,
        "IOSurface Buffer");

    _io_surface_gl_memory_allocator =
        g_object_new (GST_TYPE_IO_SURFACE_GL_MEMORY_ALLOCATOR, NULL);
    gst_object_ref_sink (_io_surface_gl_memory_allocator);

    gst_allocator_register (GST_IO_SURFACE_GL_MEMORY_ALLOCATOR_NAME,
        gst_object_ref (_io_surface_gl_memory_allocator));
    g_once_init_leave (&_init, 1);
  }
}

gboolean
gst_is_io_surface_gl_memory (GstMemory * mem)
{
  return mem != NULL && mem->allocator != NULL &&
      g_type_is_a (G_OBJECT_TYPE (mem->allocator),
      GST_TYPE_IO_SURFACE_GL_MEMORY_ALLOCATOR);
}

static GstIOSurfaceGLMemory *
_io_surface_gl_memory_new (GstGLContext * context,
    IOSurfaceRef surface,
    GstGLTextureTarget target,
    GstGLFormat tex_format,
    GstVideoInfo * info,
    guint plane,
    GstVideoAlignment * valign, gpointer user_data, GDestroyNotify notify)
{
  GstIOSurfaceGLMemory *mem;

  g_return_val_if_fail (target == GST_GL_TEXTURE_TARGET_RECTANGLE, NULL);

  mem = g_new0 (GstIOSurfaceGLMemory, 1);
  gst_gl_memory_init (&mem->gl_mem, _io_surface_gl_memory_allocator, NULL,
      context, target, tex_format, NULL, info, plane, valign, user_data,
      notify);

  GST_MINI_OBJECT_FLAG_SET (mem, GST_MEMORY_FLAG_READONLY);

  mem->surface = NULL;
  gst_io_surface_gl_memory_set_surface (mem, surface);

  return mem;
}

GstIOSurfaceGLMemory *
gst_io_surface_gl_memory_wrapped (GstGLContext * context,
    IOSurfaceRef surface,
    GstGLTextureTarget target,
    GstGLFormat tex_format,
    GstVideoInfo * info,
    guint plane,
    GstVideoAlignment * valign, gpointer user_data, GDestroyNotify notify)
{
  return _io_surface_gl_memory_new (context, surface, target, tex_format, info,
      plane, valign, user_data, notify);
}

static void
_io_surface_gl_memory_set_surface (GstIOSurfaceGLMemory * memory,
    IOSurfaceRef surface)
{
  GstGLMemory *gl_mem = (GstGLMemory *) memory;
  GstGLContext *context = ((GstGLBaseMemory *) gl_mem)->context;
  GstGLFuncs *gl = context->gl_vtable;

  if (memory->surface)
    IOSurfaceDecrementUseCount (memory->surface);
  memory->surface = surface;
  if (surface) {
    GLuint tex_id, tex_target, texifmt;
    guint plane;
    CGLError cglError;

    plane = gl_mem->plane;
    tex_id = gl_mem->tex_id;
    tex_target = gst_gl_texture_target_to_gl (gl_mem->tex_target);
    texifmt = gst_gl_format_from_video_info (context, &gl_mem->info, plane);
    gl->BindTexture (tex_target, tex_id);
    cglError = CGLTexImageIOSurface2D ((CGLContextObj)
        gst_gl_context_get_gl_context (context), tex_target, texifmt,
        IOSurfaceGetWidthOfPlane (surface, plane),
        IOSurfaceGetHeightOfPlane (surface, plane), texifmt, GL_UNSIGNED_BYTE,
        surface, plane);
    gl->BindTexture (tex_target, 0);
    IOSurfaceIncrementUseCount (surface);
    GST_DEBUG ("bound surface %p to texture %u: %d", surface, tex_id, cglError);
  }
}

static void
_do_set_surface (GstGLContext * context, ContextThreadData * data)
{
  _io_surface_gl_memory_set_surface (data->memory, data->surface);
}

void
gst_io_surface_gl_memory_set_surface (GstIOSurfaceGLMemory * memory,
    IOSurfaceRef surface)
{
  GstGLContext *context;
  ContextThreadData data = { memory, surface };

  g_return_if_fail (gst_is_io_surface_gl_memory ((GstMemory *) memory));

  context = memory->gl_mem.mem.context;
  gst_gl_context_thread_add (context,
      (GstGLContextThreadFunc) _do_set_surface, &data);
}
