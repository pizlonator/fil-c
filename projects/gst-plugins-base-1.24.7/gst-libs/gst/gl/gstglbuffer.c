/*
 * GStreamer
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "gstglbuffer.h"

#include "gstglcontext.h"
#include "gstglfuncs.h"
#include "gstglutils.h"

/**
 * SECTION:gstglbuffer
 * @title: GstGLBuffer
 * @short_description: memory subclass for GL buffers
 * @see_also: #GstMemory, #GstAllocator
 *
 * GstGLBuffer is a #GstMemory subclass providing support for the mapping of
 * GL buffers.
 *
 * Data is uploaded or downloaded from the GPU as is necessary.
 */

/* Implementation notes:
 *
 * Currently does not take into account GLES2 differences (no mapbuffer)
 */

#define USING_OPENGL(context) (gst_gl_context_check_gl_version (context, GST_GL_API_OPENGL, 1, 0))
#define USING_OPENGL3(context) (gst_gl_context_check_gl_version (context, GST_GL_API_OPENGL3, 3, 1))
#define USING_GLES(context) (gst_gl_context_check_gl_version (context, GST_GL_API_GLES, 1, 0))
#define USING_GLES2(context) (gst_gl_context_check_gl_version (context, GST_GL_API_GLES2, 2, 0))
#define USING_GLES3(context) (gst_gl_context_check_gl_version (context, GST_GL_API_GLES2, 3, 0))

#define HAVE_BUFFER_STORAGE(context) (context->gl_vtable->BufferStorage != NULL)

/* compatibility definitions... */
#ifndef GL_MAP_READ_BIT
#define GL_MAP_READ_BIT 0x0001
#endif
#ifndef GL_MAP_WRITE_BIT
#define GL_MAP_WRITE_BIT 0x0002
#endif
#ifndef GL_MAP_FLUSH_EXPLICIT_BIT
#define GL_MAP_FLUSH_EXPLICIT_BIT 0x0010
#endif
#ifndef GL_MAP_PERSISTENT_BIT
#define GL_MAP_PERSISTENT_BIT 0x0040
#endif
#ifndef GL_DYNAMIC_STORAGE_BIT
#define GL_DYNAMIC_STORAGE_BIT 0x0100
#endif
#ifndef GL_CLIENT_STORAGE_BIT
#define GL_CLIENT_STORAGE_BIT 0x0200
#endif
#ifndef GL_COPY_READ_BUFFER
#define GL_COPY_READ_BUFFER 0x8F36
#endif
#ifndef GL_COPY_WRITE_BUFFER
#define GL_COPY_WRITE_BUFFER 0x8F37
#endif

GST_DEBUG_CATEGORY_STATIC (GST_CAT_GL_BUFFER);
#define GST_CAT_DEFUALT GST_CAT_GL_BUFFER

#ifndef GST_REMOVE_DEPRECATED
GST_DEFINE_MINI_OBJECT_TYPE (GstGLBuffer, gst_gl_buffer);
#endif

static GstAllocator *_gl_buffer_allocator;

static gboolean
_gl_buffer_create (GstGLBuffer * gl_mem, GError ** error)
{
  const GstGLFuncs *gl = gl_mem->mem.context->gl_vtable;

  gl->GenBuffers (1, &gl_mem->id);
  gl->BindBuffer (gl_mem->target, gl_mem->id);
  if (HAVE_BUFFER_STORAGE (gl_mem->mem.context)) {
    GLenum flags = 0;

    flags |= GL_MAP_READ_BIT | GL_MAP_WRITE_BIT;
    /* allow access to GPU-side data while there are outstanding mappings */
    flags |= GL_MAP_PERSISTENT_BIT;
    /* match the glBufferData() below and  make this buffer mutable */
    flags |= GL_DYNAMIC_STORAGE_BIT;
    /* hint that the data should be kept CPU-side.  Fixes atrocious
     * performance when e.g. libav decoders are direct-rendering into our
     * data pointer */
    /* XXX: make this an fine-grained option. The current assumption here is
     * that users signal GL_STREAM_DRAW for regularly updating video frames as
     * is done by gstglmemorypbo */
    if (gl_mem->usage_hints == GL_STREAM_DRAW) {
      GST_CAT_DEBUG (GST_CAT_GL_BUFFER, "using client-side storage for "
          "buffer %p %u", gl_mem, gl_mem->id);
      flags |= GL_CLIENT_STORAGE_BIT;
    }

    gl->BufferStorage (gl_mem->target, gl_mem->mem.mem.maxsize, NULL, flags);
  } else {
    gl->BufferData (gl_mem->target, gl_mem->mem.mem.maxsize, NULL,
        gl_mem->usage_hints);
  }
  gl->BindBuffer (gl_mem->target, 0);

  return TRUE;
}

struct create_data
{
  GstGLBuffer *mem;
  gboolean result;
};

static void
_gl_buffer_init (GstGLBuffer * mem, GstAllocator * allocator,
    GstMemory * parent, GstGLContext * context, guint gl_target, guint gl_usage,
    const GstAllocationParams * params, gsize size)
{
  mem->target = gl_target;
  mem->usage_hints = gl_usage;

  gst_gl_base_memory_init ((GstGLBaseMemory *) mem, allocator, parent, context,
      params, size, NULL, NULL);

  GST_CAT_DEBUG (GST_CAT_GL_BUFFER, "new GL buffer memory:%p size:%"
      G_GSIZE_FORMAT, mem, mem->mem.mem.maxsize);
}

static GstGLBuffer *
_gl_buffer_new (GstAllocator * allocator, GstMemory * parent,
    GstGLContext * context, guint gl_target, guint gl_usage,
    const GstAllocationParams * params, gsize size)
{
  GstGLBuffer *ret = g_new0 (GstGLBuffer, 1);
  _gl_buffer_init (ret, allocator, parent, context, gl_target, gl_usage,
      params, size);

  return ret;
}

static GLenum
gst_map_flags_to_gl (GstMapFlags flags)
{
  GLenum ret = 0;

  if (flags & GST_MAP_READ)
    ret |= GL_MAP_READ_BIT;

  if (flags & GST_MAP_WRITE)
    ret |= GL_MAP_WRITE_BIT;

  return ret;
}

static gpointer
gst_gl_buffer_cpu_access (GstGLBuffer * mem, GstMapInfo * info, gsize size)
{
  const GstGLFuncs *gl = mem->mem.context->gl_vtable;
  gpointer data, ret;

  GST_CAT_LOG (GST_CAT_GL_BUFFER, "mapping %p id %d size %" G_GSIZE_FORMAT,
      mem, mem->id, size);

  if (HAVE_BUFFER_STORAGE (mem->mem.context)) {
    GLenum gl_map_flags = gst_map_flags_to_gl (info->flags);

    gl_map_flags |= GL_MAP_PERSISTENT_BIT;
    if (info->flags & GST_MAP_WRITE)
      gl_map_flags |= GL_MAP_FLUSH_EXPLICIT_BIT;

    if (mem->mem.map_count == (mem->mem.gl_map_count + 1)) {
      gl->BindBuffer (mem->target, mem->id);
      mem->mem.data = gl->MapBufferRange (mem->target, 0, size, gl_map_flags);
      gl->BindBuffer (mem->target, 0);
    }

    return mem->mem.data;
  }

  if (!gst_gl_base_memory_alloc_data (GST_GL_BASE_MEMORY_CAST (mem)))
    return NULL;

  ret = mem->mem.data;

  /* The extra data pointer indirection/memcpy is needed for coherent across
   * concurrent map()'s in both GL and CPU */
  if (GST_MEMORY_FLAG_IS_SET (mem, GST_GL_BASE_MEMORY_TRANSFER_NEED_DOWNLOAD)
      && (info->flags & GST_MAP_GL) == 0 && (info->flags & GST_MAP_READ) != 0) {
    gl->BindBuffer (mem->target, mem->id);

    if (gl->MapBufferRange) {
      /* FIXME: optionally remove this with a flag and return the
       * glMapBufferRange pointer (requires
       * GL_ARB_buffer_storage/GL4/GL_COHERENT_BIT) */
      guint gl_map_flags = GL_MAP_READ_BIT;

      data = gl->MapBufferRange (mem->target, 0, size, gl_map_flags);

      if (data)
        memcpy (mem->mem.data, data, size);

      gl->UnmapBuffer (mem->target);
      ret = mem->mem.data;
    } else if (gl->GetBufferSubData) {
      gl->GetBufferSubData (mem->target, 0, size, mem->mem.data);
      ret = mem->mem.data;
    } else {
      ret = NULL;
    }
    gl->BindBuffer (mem->target, 0);
  }

  return ret;
}

static void
gst_gl_buffer_upload_cpu_write (GstGLBuffer * mem, GstMapInfo * info,
    gsize size)
{
  const GstGLFuncs *gl = mem->mem.context->gl_vtable;
  gpointer data;

  if (!mem->mem.data)
    /* no data pointer has been written */
    return;

  GST_CAT_LOG (GST_CAT_GL_BUFFER, "mapping %p id %d size %" G_GSIZE_FORMAT,
      mem, mem->id, size);

  if (HAVE_BUFFER_STORAGE (mem->mem.context)) {
    /* flushing is done on unmap already */
    return;
  }

  /* The extra data pointer indirection/memcpy is needed for coherent across
   * concurrent map()'s in both GL and CPU */
  /* FIXME: uploading potentially half-written data for libav pushing READWRITE
   * mapped buffers */
  if (GST_MEMORY_FLAG_IS_SET (mem, GST_GL_BASE_MEMORY_TRANSFER_NEED_UPLOAD)
      || (mem->mem.map_flags & GST_MAP_WRITE) != 0) {
    gl->BindBuffer (mem->target, mem->id);

    if (gl->MapBufferRange) {
      /* FIXME: optionally remove this with a flag and return the
       * glMapBufferRange pointer (requires
       * GL_ARB_buffer_storage/GL4/GL_COHERENT_BIT) */
      guint gl_map_flags = GL_MAP_WRITE_BIT;

      data = gl->MapBufferRange (mem->target, 0, size, gl_map_flags);

      if (data)
        memcpy (data, mem->mem.data, size);

      gl->UnmapBuffer (mem->target);
    } else if (gl->BufferSubData) {
      gl->BufferSubData (mem->target, 0, size, mem->mem.data);
    }
    gl->BindBuffer (mem->target, 0);
  }
}

static gpointer
_gl_buffer_map (GstGLBuffer * mem, GstMapInfo * info, gsize size)
{
  const GstGLFuncs *gl = mem->mem.context->gl_vtable;

  if ((info->flags & GST_MAP_GL) != 0) {
    if (info->flags & GST_MAP_READ) {
      gst_gl_buffer_upload_cpu_write (mem, info, size);
    }
    gl->BindBuffer (mem->target, mem->id);
    return &mem->id;
  } else {
    return gst_gl_buffer_cpu_access (mem, info, size);
  }

  return NULL;
}

static void
_gl_buffer_unmap (GstGLBuffer * mem, GstMapInfo * info)
{
  const GstGLFuncs *gl = mem->mem.context->gl_vtable;

  GST_CAT_LOG (GST_CAT_GL_BUFFER, "unmapping %p id %d size %" G_GSIZE_FORMAT,
      mem, mem->id, info->size);

  if (HAVE_BUFFER_STORAGE (mem->mem.context) && (info->flags & GST_MAP_GL) == 0) {
    gl->BindBuffer (mem->target, mem->id);

    if (info->flags & GST_MAP_WRITE)
      gl->FlushMappedBufferRange (mem->target, 0, info->maxsize);

    if (mem->mem.map_count == (mem->mem.gl_map_count + 1))
      /* if this is our last cpu-mapping, unmap the GL buffer */
      gl->UnmapBuffer (mem->target);

    gl->BindBuffer (mem->target, 0);
  }
  if ((info->flags & GST_MAP_GL) != 0) {
    gl->BindBuffer (mem->target, 0);
  }
  /* XXX: optimistically transfer data */
}

/**
 * gst_gl_buffer_copy_buffer_sub_data:
 * @src: the source #GstGLBuffer
 * @dest: the destination #GstGLBuffer
 * @offset: the offset to copy from @src
 * @size: the size to copy from @src
 *
 * Copies @src into @dest using glCopyBufferSubData().
 *
 * Returns: whether the copy operation succeeded
 *
 * Since: 1.8
 */
static gboolean
gst_gl_buffer_copy_buffer_sub_data (GstGLBuffer * src,
    GstGLBuffer * dest, gssize offset, gssize size)
{
  const GstGLFuncs *gl = src->mem.context->gl_vtable;
  GstMapInfo sinfo, dinfo;

  if (!gl->CopyBufferSubData)
    /* This is GL(ES) 3.0+ only */
    return FALSE;

  if (!gst_memory_map ((GstMemory *) src, &sinfo, GST_MAP_READ | GST_MAP_GL)) {
    GST_CAT_WARNING (GST_CAT_GL_BUFFER,
        "failed to read map source memory %p", src);
    return FALSE;
  }

  if (!gst_memory_map ((GstMemory *) dest, &dinfo, GST_MAP_WRITE | GST_MAP_GL)) {
    GST_CAT_WARNING (GST_CAT_GL_BUFFER,
        "failed to write map destination memory %p", dest);
    gst_memory_unmap ((GstMemory *) src, &sinfo);
    return FALSE;
  }

  gl->BindBuffer (GL_COPY_READ_BUFFER, src->id);
  gl->BindBuffer (GL_COPY_WRITE_BUFFER, dest->id);
  gl->CopyBufferSubData (GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER,
      offset, 0, size);

  gst_memory_unmap ((GstMemory *) src, &sinfo);
  gst_memory_unmap ((GstMemory *) dest, &dinfo);

  return TRUE;
}

static GstGLBuffer *
_gl_buffer_copy (GstGLBuffer * src, gssize offset, gssize size)
{
  GstAllocator *allocator = src->mem.mem.allocator;
  GstAllocationParams params = { 0, src->mem.mem.align, 0, 0 };
  GstGLBuffer *dest = NULL;

  dest = _gl_buffer_new (allocator, NULL, src->mem.context,
      src->target, src->usage_hints, &params, src->mem.mem.maxsize);

  /* If not doing a full copy, then copy to sysmem, the 2D represention of the
   * texture would become wrong */
  if (GST_MEMORY_FLAG_IS_SET (src, GST_GL_BASE_MEMORY_TRANSFER_NEED_UPLOAD)) {
    if (!gst_gl_base_memory_memcpy (GST_GL_BASE_MEMORY_CAST (src),
            GST_GL_BASE_MEMORY_CAST (dest), offset, size)) {
      GST_CAT_WARNING (GST_CAT_GL_BUFFER, "Could not copy GL Buffer");
      gst_memory_unref (GST_MEMORY_CAST (dest));
      dest = NULL;
    }
  } else {
    if (!gst_gl_buffer_copy_buffer_sub_data (src, dest, offset, size)) {
      if (!gst_gl_base_memory_memcpy (GST_GL_BASE_MEMORY_CAST (src),
              GST_GL_BASE_MEMORY_CAST (dest), offset, size)) {
        GST_CAT_WARNING (GST_CAT_GL_BUFFER, "Could not copy GL Buffer");
        gst_memory_unref (GST_MEMORY_CAST (dest));
        dest = NULL;
      }
    }
  }

  return dest;
}

static GstMemory *
_gl_buffer_alloc (GstAllocator * allocator, gsize size,
    GstAllocationParams * params)
{
  g_critical ("Need to use gst_gl_base_memory_alloc() to allocate from "
      "this allocator");

  return NULL;
}

static void
_gl_buffer_destroy (GstGLBuffer * mem)
{
  const GstGLFuncs *gl = mem->mem.context->gl_vtable;

  gl->DeleteBuffers (1, &mem->id);
}

static void
_gst_gl_buffer_allocation_params_copy_data (GstGLBufferAllocationParams * src,
    GstGLBufferAllocationParams * dest)
{
  memset (dest, 0, sizeof (*dest));

  gst_gl_allocation_params_copy_data (&src->parent, &dest->parent);

  dest->gl_target = src->gl_target;
  dest->gl_usage = src->gl_usage;
}

static void
_gst_gl_buffer_allocation_params_free_data (GstGLBufferAllocationParams *
    params)
{
  gst_gl_allocation_params_free_data (&params->parent);
}

G_DEFINE_BOXED_TYPE (GstGLBufferAllocationParams,
    gst_gl_buffer_allocation_params,
    (GBoxedCopyFunc) gst_gl_allocation_params_copy,
    (GBoxedFreeFunc) gst_gl_allocation_params_free);

/**
 * gst_gl_buffer_allocation_params_new:
 * @context: a #GstGLContext
 * @alloc_size: the size in bytes to allocate
 * @alloc_params: (allow-none): the #GstAllocationParams for @tex_id
 * @gl_target: the OpenGL target to allocate
 * @gl_usage: the OpenGL usage hint to allocate with
 *
 * Returns: a new #GstGLBufferAllocationParams for allocating OpenGL buffer
 *          objects
 *
 * Since: 1.8
 */
GstGLBufferAllocationParams *
gst_gl_buffer_allocation_params_new (GstGLContext * context, gsize alloc_size,
    const GstAllocationParams * alloc_params, guint gl_target, guint gl_usage)
{
  GstGLBufferAllocationParams *params;

  g_return_val_if_fail (GST_IS_GL_CONTEXT (context), NULL);
  g_return_val_if_fail (alloc_size > 0, NULL);

  params = g_new0 (GstGLBufferAllocationParams, 1);

  if (!gst_gl_allocation_params_init (&params->parent, sizeof (*params),
          GST_GL_ALLOCATION_PARAMS_ALLOC_FLAG_BUFFER |
          GST_GL_ALLOCATION_PARAMS_ALLOC_FLAG_ALLOC,
          (GstGLAllocationParamsCopyFunc)
          _gst_gl_buffer_allocation_params_copy_data,
          (GstGLAllocationParamsFreeFunc)
          _gst_gl_buffer_allocation_params_free_data, context, alloc_size,
          alloc_params, NULL, 0, NULL, NULL)) {
    g_free (params);
    return NULL;
  }

  params->gl_target = gl_target;
  params->gl_usage = gl_usage;

  return params;
}

static GstGLBuffer *
_gl_buffer_alloc_mem (GstGLBufferAllocator * allocator,
    GstGLBufferAllocationParams * params)
{
  guint alloc_flags = params->parent.alloc_flags;

  g_return_val_if_fail (alloc_flags &
      GST_GL_ALLOCATION_PARAMS_ALLOC_FLAG_BUFFER, NULL);
  g_return_val_if_fail (alloc_flags & GST_GL_ALLOCATION_PARAMS_ALLOC_FLAG_ALLOC,
      NULL);

  return _gl_buffer_new (GST_ALLOCATOR (allocator), NULL,
      params->parent.context, params->gl_target, params->gl_usage,
      params->parent.alloc_params, params->parent.alloc_size);
}

G_DEFINE_TYPE (GstGLBufferAllocator, gst_gl_buffer_allocator,
    GST_TYPE_GL_BASE_MEMORY_ALLOCATOR);

static void
gst_gl_buffer_allocator_class_init (GstGLBufferAllocatorClass * klass)
{
  GstAllocatorClass *allocator_class = (GstAllocatorClass *) klass;
  GstGLBaseMemoryAllocatorClass *gl_base;

  gl_base = (GstGLBaseMemoryAllocatorClass *) klass;

  gl_base->alloc = (GstGLBaseMemoryAllocatorAllocFunction) _gl_buffer_alloc_mem;
  gl_base->create = (GstGLBaseMemoryAllocatorCreateFunction) _gl_buffer_create;
  gl_base->map = (GstGLBaseMemoryAllocatorMapFunction) _gl_buffer_map;
  gl_base->unmap = (GstGLBaseMemoryAllocatorUnmapFunction) _gl_buffer_unmap;
  gl_base->copy = (GstGLBaseMemoryAllocatorCopyFunction) _gl_buffer_copy;
  gl_base->destroy =
      (GstGLBaseMemoryAllocatorDestroyFunction) _gl_buffer_destroy;

  allocator_class->alloc = _gl_buffer_alloc;
}

static void
gst_gl_buffer_allocator_init (GstGLBufferAllocator * allocator)
{
  GstAllocator *alloc = GST_ALLOCATOR_CAST (allocator);

  alloc->mem_type = GST_GL_BUFFER_ALLOCATOR_NAME;

  GST_OBJECT_FLAG_SET (allocator, GST_ALLOCATOR_FLAG_CUSTOM_ALLOC);
}

/**
 * gst_gl_buffer_init_once:
 *
 * Initializes the GL Buffer allocator. It is safe to call this function
 * multiple times.  This must be called before any other #GstGLBuffer operation.
 *
 * Since: 1.8
 */
void
gst_gl_buffer_init_once (void)
{
  static gsize _init = 0;

  if (g_once_init_enter (&_init)) {
    gst_gl_base_memory_init_once ();

    GST_DEBUG_CATEGORY_INIT (GST_CAT_GL_BUFFER, "glbuffer", 0, "OpenGL Buffer");

    _gl_buffer_allocator =
        g_object_new (gst_gl_buffer_allocator_get_type (), NULL);
    gst_object_ref_sink (_gl_buffer_allocator);

    /* The allocator is never unreffed */
    GST_OBJECT_FLAG_SET (_gl_buffer_allocator, GST_OBJECT_FLAG_MAY_BE_LEAKED);

    gst_allocator_register (GST_GL_BUFFER_ALLOCATOR_NAME,
        gst_object_ref (_gl_buffer_allocator));
    g_once_init_leave (&_init, 1);
  }
}

/**
 * gst_is_gl_buffer:
 * @mem:a #GstMemory
 *
 * Returns: whether the memory at @mem is a #GstGLBuffer
 *
 * Since: 1.8
 */
gboolean
gst_is_gl_buffer (GstMemory * mem)
{
  return mem != NULL && mem->allocator != NULL &&
      g_type_is_a (G_OBJECT_TYPE (mem->allocator),
      GST_TYPE_GL_BUFFER_ALLOCATOR);
}
