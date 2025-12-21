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

#include "gstglbasememory.h"

#include "gstglcontext.h"
#include "gstglcontext_private.h"
#include "gstglquery.h"

/**
 * SECTION:gstglbasememory
 * @title: GstGLBaseMemory
 * @short_description: memory subclass for GL buffers
 * @see_also: #GstMemory, #GstAllocator
 *
 * GstGLBaseMemory is a #GstMemory subclass providing the basis of support
 * for the mapping of GL buffers.
 *
 * Data is uploaded or downloaded from the GPU as is necessary.
 */

#define USING_OPENGL(context) (gst_gl_context_check_gl_version (context, GST_GL_API_OPENGL, 1, 0))
#define USING_OPENGL3(context) (gst_gl_context_check_gl_version (context, GST_GL_API_OPENGL3, 3, 1))
#define USING_GLES(context) (gst_gl_context_check_gl_version (context, GST_GL_API_GLES, 1, 0))
#define USING_GLES2(context) (gst_gl_context_check_gl_version (context, GST_GL_API_GLES2, 2, 0))
#define USING_GLES3(context) (gst_gl_context_check_gl_version (context, GST_GL_API_GLES2, 3, 0))

GST_DEBUG_CATEGORY_STATIC (GST_CAT_GL_BASE_MEMORY);
#define GST_CAT_DEFUALT GST_CAT_GL_BASE_MEMORY

#ifndef GST_REMOVE_DEPRECATED
GST_DEFINE_MINI_OBJECT_TYPE (GstGLBaseMemory, gst_gl_base_memory);
#endif

/**
 * gst_gl_base_memory_error_quark:
 *
 * Returns: the quark used for #GstGLBaseMemory in #GError's
 */
GQuark
gst_gl_base_memory_error_quark (void)
{
  return g_quark_from_static_string ("gst-gl-base-memory-error-quark");
}

static gboolean
_default_create (GstGLBaseMemory * mem, GError ** error)
{
  g_set_error (error, GST_GL_BASE_MEMORY_ERROR, GST_GL_BASE_MEMORY_ERROR_FAILED,
      "subclass should define create() vfunc");

  g_critical ("subclass should override "
      "GstGLBaseMemoryAllocatorClass::create() function");

  return FALSE;
}

struct create_data
{
  GstGLBaseMemory *mem;
  gboolean result;
};

static void
_mem_create_gl (GstGLContext * context, struct create_data *transfer)
{
  GstGLBaseMemoryAllocatorClass *alloc_class;
  GError *error = NULL;

  GST_CAT_TRACE (GST_CAT_GL_BASE_MEMORY, "Create memory %p", transfer->mem);

  alloc_class =
      GST_GL_BASE_MEMORY_ALLOCATOR_GET_CLASS (transfer->mem->mem.allocator);

  g_return_if_fail (alloc_class->create != NULL);

  /* Don't do expensive queries when debugging is disabled */
  transfer->mem->query = NULL;
  if (_gst_gl_context_debug_is_enabled (context))
    transfer->mem->query =
        gst_gl_query_new (context, GST_GL_QUERY_TIME_ELAPSED);

  if ((transfer->result = alloc_class->create (transfer->mem, &error)))
    return;

  g_assert (error != NULL);

  GST_CAT_ERROR (GST_CAT_GL_BASE_MEMORY, "Failed to create GL buffer: %s",
      error->message);
  g_clear_error (&error);
}

/**
 * gst_gl_base_memory_init:
 * @mem: the #GstGLBaseMemory to initialize
 * @allocator: the #GstAllocator to initialize with
 * @parent: (allow-none): the parent #GstMemory to initialize with
 * @context: the #GstGLContext to initialize with
 * @params: (allow-none): the @GstAllocationParams to initialize with
 * @size: the number of bytes to be allocated
 * @user_data: (allow-none): user data to call @notify with
 * @notify: (allow-none): a #GDestroyNotify
 *
 * Initializes @mem with the required parameters
 *
 * Since: 1.8
 */
void
gst_gl_base_memory_init (GstGLBaseMemory * mem, GstAllocator * allocator,
    GstMemory * parent, GstGLContext * context,
    const GstAllocationParams * params, gsize size, gpointer user_data,
    GDestroyNotify notify)
{
  gsize align = gst_memory_alignment, offset = 0, maxsize;
  GstMemoryFlags flags = 0;
  struct create_data data;

  /* A note on sizes.
   * gl_mem->alloc_size: the size to allocate when we control the allocation.
   *                     Size of the unaligned allocation.
   * mem->maxsize: the size that is used by GstMemory for mapping, to map the
   *               entire memory. The size of the aligned allocation
   * mem->size: represents the size of the valid data. Can be reduced with
   *            gst_memory_resize()
   *
   * It holds that:
   * mem->size + mem->offset <= mem->maxsize
   * and
   * mem->maxsize + alignment offset <= gl_mem->alloc_size
   *
   * We need to add the alignment mask to the allocated size in order to have
   * the freedom to align the gl_mem->data pointer correctly which may be offset
   * by at most align bytes in the alloc_data pointer.
   *
   * maxsize is not suitable for this as it is used by GstMemory as the size
   * to map with.
   */
  mem->alloc_size = maxsize = size;
  if (params) {
    flags = params->flags;
    align |= params->align;
    offset = params->prefix;
    maxsize += params->prefix + params->padding;

    /* deals with any alignment */
    mem->alloc_size = maxsize + align;
  }

  gst_memory_init (GST_MEMORY_CAST (mem), flags, allocator, parent, maxsize,
      align, offset, size);

  mem->context = gst_object_ref (context);
  mem->notify = notify;
  mem->user_data = user_data;

  g_mutex_init (&mem->lock);

  data.mem = mem;

  gst_gl_context_thread_add (context,
      (GstGLContextThreadFunc) _mem_create_gl, &data);
  if (!data.result) {
    GST_CAT_ERROR (GST_CAT_GL_BASE_MEMORY,
        "Could not create GL buffer with context:%p", context);
  }

  GST_CAT_DEBUG (GST_CAT_GL_BASE_MEMORY, "new GL buffer memory:%p size:%"
      G_GSIZE_FORMAT, mem, maxsize);
}

static gpointer
_align_data (gpointer data, gsize align)
{
  guint8 *ret = data;
  gsize aoffset;

  /* do alignment, data must have enough padding at the end to move at most
   * align bytes */
  if ((aoffset = ((guintptr) ret & align))) {
    aoffset = (align + 1) - aoffset;
    ret += aoffset;
  }

  return ret;
}

/* subclass usage only */
/**
 * gst_gl_base_memory_alloc_data:
 * @gl_mem: a #GstGLBaseMemory
 *
 * Note: only intended for subclass usage to allocate the system memory buffer
 * on demand.  If there is already a non-NULL data pointer in @gl_mem->data,
 * then this function imply returns TRUE.
 *
 * Returns: whether the system memory could be allocated
 */
gboolean
gst_gl_base_memory_alloc_data (GstGLBaseMemory * gl_mem)
{
  GstMemory *mem = (GstMemory *) gl_mem;

  if (gl_mem->data)
    return TRUE;

  GST_CAT_LOG (GST_CAT_GL_BASE_MEMORY, "%p attempting allocation of data "
      "pointer of size %" G_GSIZE_FORMAT, gl_mem, gl_mem->alloc_size);
  gl_mem->alloc_data = g_try_malloc (gl_mem->alloc_size);

  if (gl_mem->alloc_data == NULL)
    return FALSE;

  gl_mem->data = _align_data (gl_mem->alloc_data, mem->align);

  GST_CAT_DEBUG (GST_CAT_GL_BASE_MEMORY, "%p allocated data pointer alloc %p, "
      "data %p", gl_mem, gl_mem->alloc_data, gl_mem->data);

  return TRUE;
}

struct map_data
{
  GstGLBaseMemory *mem;
  GstMapInfo *info;
  gsize size;
  gpointer data;
};

static void
_map_data_gl (GstGLContext * context, struct map_data *transfer)
{
  GstGLBaseMemoryAllocatorClass *alloc_class;
  GstGLBaseMemory *mem = transfer->mem;
  GstMapInfo *info = transfer->info;
  guint prev_map_flags;
  guint prev_gl_map_count;

  alloc_class =
      GST_GL_BASE_MEMORY_ALLOCATOR_GET_CLASS (transfer->mem->mem.allocator);

  g_return_if_fail (alloc_class->map != NULL);

  g_mutex_lock (&mem->lock);

  prev_map_flags = mem->map_flags;
  prev_gl_map_count = mem->gl_map_count;

  GST_CAT_LOG (GST_CAT_GL_BASE_MEMORY, "mapping mem %p flags %04x", mem,
      info->flags);

  /* FIXME: validate map flags based on the memory domain */
  if (mem->map_count++ == 0)
    mem->map_flags = info->flags;
  else {
    /* assert that the flags are a subset of the first map flags */
    g_assert ((((GST_MAP_GL - 1) & info->flags) & mem->map_flags) != 0);
    GST_CAT_LOG (GST_CAT_GL_BASE_MEMORY, "multiple map no %d flags %04x "
        "all flags %04x", mem->map_count, info->flags, mem->map_flags);
  }

  if ((info->flags & GST_MAP_GL) != (mem->map_flags & GST_MAP_GL))
    mem->map_flags |= GST_MAP_GL;

  if (info->flags & GST_MAP_GL)
    mem->gl_map_count++;

  transfer->data = alloc_class->map (transfer->mem, transfer->info,
      transfer->size);

  if (transfer->data) {
    if (info->flags & GST_MAP_GL) {
      if (info->flags & GST_MAP_WRITE)
        GST_MINI_OBJECT_FLAG_SET (mem,
            GST_GL_BASE_MEMORY_TRANSFER_NEED_DOWNLOAD);
      GST_MEMORY_FLAG_UNSET (mem, GST_GL_BASE_MEMORY_TRANSFER_NEED_UPLOAD);
    } else {
      if (info->flags & GST_MAP_WRITE)
        GST_MINI_OBJECT_FLAG_SET (mem, GST_GL_BASE_MEMORY_TRANSFER_NEED_UPLOAD);
      GST_MEMORY_FLAG_UNSET (mem, GST_GL_BASE_MEMORY_TRANSFER_NEED_DOWNLOAD);
    }
  } else {
    /* undo state tracking on error */
    mem->map_flags = prev_map_flags;
    mem->gl_map_count = prev_gl_map_count;
    mem->map_count--;
  }

  g_mutex_unlock (&mem->lock);
}

static gpointer
_mem_map_full (GstGLBaseMemory * mem, GstMapInfo * info, gsize size)
{
  struct map_data transfer;

  transfer.mem = mem;
  transfer.info = info;
  transfer.size = size;
  transfer.data = NULL;

  gst_gl_context_thread_add (mem->context,
      (GstGLContextThreadFunc) _map_data_gl, &transfer);

  return transfer.data;
}

struct unmap_data
{
  GstGLBaseMemory *mem;
  GstMapInfo *info;
};

static void
_unmap_data_gl (GstGLContext * context, struct unmap_data *transfer)
{
  GstGLBaseMemoryAllocatorClass *alloc_class;
  GstGLBaseMemory *mem = transfer->mem;
  GstMapInfo *info = transfer->info;

  alloc_class =
      GST_GL_BASE_MEMORY_ALLOCATOR_GET_CLASS (transfer->mem->mem.allocator);

  g_return_if_fail (alloc_class->unmap != NULL);

  g_mutex_lock (&mem->lock);

  GST_CAT_LOG (GST_CAT_GL_BASE_MEMORY, "unmapping mem %p flags %04x", mem,
      info->flags);

  alloc_class->unmap (transfer->mem, transfer->info);

  if (info->flags & GST_MAP_GL && --mem->gl_map_count)
    /* unset the gl flag */
    mem->map_flags &= ~GST_MAP_GL;

  if (--mem->map_count <= 0) {
    mem->map_flags = 0;
  }

  if (info->flags & GST_MAP_GL) {
    if (info->flags & GST_MAP_WRITE)
      GST_MINI_OBJECT_FLAG_SET (mem, GST_GL_BASE_MEMORY_TRANSFER_NEED_DOWNLOAD);
  } else {
    if (info->flags & GST_MAP_WRITE)
      GST_MINI_OBJECT_FLAG_SET (mem, GST_GL_BASE_MEMORY_TRANSFER_NEED_UPLOAD);
  }

  g_mutex_unlock (&mem->lock);
}

static void
_mem_unmap_full (GstGLBaseMemory * mem, GstMapInfo * info)
{
  struct unmap_data transfer;

  transfer.mem = mem;
  transfer.info = info;

  gst_gl_context_thread_add (mem->context,
      (GstGLContextThreadFunc) _unmap_data_gl, &transfer);
}

static GstGLBaseMemory *
_default_copy (GstGLBaseMemory * src, gssize offset, gssize size)
{
  return NULL;
}

struct copy_params
{
  GstGLBaseMemory *src;
  GstGLBaseMemory *dest;
  gssize offset;
  gssize size;
  gboolean result;
};

static void
_mem_copy_gl (GstGLContext * context, struct copy_params *transfer)
{
  GstGLBaseMemoryAllocatorClass *alloc_class;

  alloc_class =
      GST_GL_BASE_MEMORY_ALLOCATOR_GET_CLASS (transfer->src->mem.allocator);

  g_return_if_fail (alloc_class->copy != NULL);

  transfer->dest =
      alloc_class->copy (transfer->src, transfer->offset, transfer->size);
}

static GstMemory *
_mem_copy (GstGLBaseMemory * src, gssize offset, gssize size)
{
  struct copy_params transfer;

  transfer.dest = NULL;
  transfer.src = src;
  transfer.offset = offset;
  transfer.size = size;
  if (size == -1 || size > 0)
    gst_gl_context_thread_add (src->context,
        (GstGLContextThreadFunc) _mem_copy_gl, &transfer);

  return (GstMemory *) transfer.dest;
}

static GstMemory *
_mem_share (GstGLBaseMemory * mem, gssize offset, gssize size)
{
  return NULL;
}

static gboolean
_mem_is_span (GstGLBaseMemory * mem1, GstGLBaseMemory * mem2, gsize * offset)
{
  return FALSE;
}

static GstMemory *
_mem_alloc (GstAllocator * allocator, gsize size, GstAllocationParams * params)
{
  g_critical ("Subclass should override GstAllocatorClass::alloc() function");

  return NULL;
}

static void
_default_destroy (GstGLBaseMemory * mem)
{
}

static void
_destroy_gl_objects (GstGLContext * context, GstGLBaseMemory * mem)
{
  GstGLBaseMemoryAllocatorClass *alloc_class;

  alloc_class = GST_GL_BASE_MEMORY_ALLOCATOR_GET_CLASS (mem->mem.allocator);

  g_return_if_fail (alloc_class->destroy != NULL);

  alloc_class->destroy (mem);

  if (mem->query)
    gst_gl_query_free (mem->query);
}

static void
_mem_free (GstAllocator * allocator, GstMemory * memory)
{
  GstGLBaseMemory *mem = (GstGLBaseMemory *) memory;

  GST_CAT_TRACE (GST_CAT_GL_BASE_MEMORY, "freeing buffer memory:%p", mem);

  gst_gl_context_thread_add (mem->context,
      (GstGLContextThreadFunc) _destroy_gl_objects, mem);

  g_mutex_clear (&mem->lock);

  if (mem->alloc_data) {
    g_free (mem->alloc_data);
    mem->alloc_data = NULL;
  }
  mem->data = NULL;

  if (mem->notify)
    mem->notify (mem->user_data);

  gst_object_unref (mem->context);

  g_free (memory);
}

/**
 * gst_gl_base_memory_init_once:
 *
 * Initializes the GL Base Memory allocator. It is safe to call this function
 * multiple times.  This must be called before any other GstGLBaseMemory operation.
 *
 * Since: 1.8
 */
void
gst_gl_base_memory_init_once (void)
{
  static gsize _init = 0;

  if (g_once_init_enter (&_init)) {
    GST_DEBUG_CATEGORY_INIT (GST_CAT_GL_BASE_MEMORY, "glbasememory", 0,
        "OpenGL BaseMemory");

    g_once_init_leave (&_init, 1);
  }
}

G_DEFINE_ABSTRACT_TYPE (GstGLBaseMemoryAllocator, gst_gl_base_memory_allocator,
    GST_TYPE_ALLOCATOR);

static void
gst_gl_base_memory_allocator_class_init (GstGLBaseMemoryAllocatorClass * klass)
{
  GstAllocatorClass *allocator_class = (GstAllocatorClass *) klass;

  allocator_class->alloc = _mem_alloc;
  allocator_class->free = _mem_free;

  klass->create = _default_create;
  klass->copy = _default_copy;
  klass->destroy = _default_destroy;
}

static void
gst_gl_base_memory_allocator_init (GstGLBaseMemoryAllocator * allocator)
{
  GstAllocator *alloc = GST_ALLOCATOR_CAST (allocator);

  /* Keep the fallback copy function around, we will need it when copying with
   * at an offset or smaller size */
  allocator->fallback_mem_copy = alloc->mem_copy;

  alloc->mem_map_full = (GstMemoryMapFullFunction) _mem_map_full;
  alloc->mem_unmap_full = (GstMemoryUnmapFullFunction) _mem_unmap_full;
  alloc->mem_copy = (GstMemoryCopyFunction) _mem_copy;
  alloc->mem_share = (GstMemoryShareFunction) _mem_share;
  alloc->mem_is_span = (GstMemoryIsSpanFunction) _mem_is_span;
}

/**
 * gst_is_gl_base_memory:
 * @mem:a #GstMemory
 *
 * Returns: whether the memory at @mem is a #GstGLBaseMemory
 *
 * Since: 1.8
 */
gboolean
gst_is_gl_base_memory (GstMemory * mem)
{
  return mem != NULL && mem->allocator != NULL &&
      g_type_is_a (G_OBJECT_TYPE (mem->allocator),
      GST_TYPE_GL_BASE_MEMORY_ALLOCATOR);
}

/**
 * gst_gl_base_memory_memcpy:
 * @src: the source #GstGLBaseMemory
 * @dest: the destination #GstGLBaseMemory
 * @offset: the offset to start at
 * @size: the number of bytes to copy
 *
 * Returns: whether the copy succeeded.
 *
 * Since: 1.8
 */
gboolean
gst_gl_base_memory_memcpy (GstGLBaseMemory * src, GstGLBaseMemory * dest,
    gssize offset, gssize size)
{
  GstMapInfo sinfo, dinfo;

  if (!gst_gl_base_memory_alloc_data (GST_GL_BASE_MEMORY_CAST (dest)))
    return FALSE;

  if (!gst_memory_map ((GstMemory *) src, &sinfo, GST_MAP_READ)) {
    GST_CAT_WARNING (GST_CAT_GL_BASE_MEMORY,
        "could not read map source memory %p", src);
    return FALSE;
  }

  if (!gst_memory_map ((GstMemory *) dest, &dinfo, GST_MAP_WRITE)) {
    GST_CAT_WARNING (GST_CAT_GL_BASE_MEMORY,
        "could not write map dest memory %p", dest);
    gst_memory_unmap ((GstMemory *) src, &sinfo);
    return FALSE;
  }

  if (size == -1)
    size = sinfo.size > offset ? sinfo.size - offset : 0;

  GST_CAT_DEBUG (GST_CAT_GL_BASE_MEMORY,
      "memcpy %" G_GSSIZE_FORMAT " memory %p -> %p", size, src, dest);
  memcpy (dinfo.data, sinfo.data + offset, size);
  gst_memory_unmap ((GstMemory *) dest, &dinfo);
  gst_memory_unmap ((GstMemory *) src, &sinfo);

  return TRUE;
}

/**
 * gst_gl_allocation_params_init: (skip)
 * @params: the #GstGLAllocationParams to initialize
 * @struct_size: the struct size of the implementation
 * @alloc_flags: some alloc flags
 * @copy: a copy function
 * @free: a free function
 * @context: (transfer none): a #GstGLContext
 * @alloc_size: the number of bytes to allocate.
 * @alloc_params: (transfer none) (allow-none): a #GstAllocationParams to apply
 * @wrapped_data: (transfer none) (allow-none): a sysmem data pointer to initialize the allocation with
 * @gl_handle: (transfer none): a GL handle to initialize the allocation with
 * @user_data: (transfer none) (allow-none): user data to call @notify with
 * @notify: (allow-none): a #GDestroyNotify
 *
 * @notify will be called once for each allocated memory using these @params
 * when freeing the memory.
 *
 * Returns: whether the parameters could be initialized
 *
 * Since: 1.8
 */
gboolean
gst_gl_allocation_params_init (GstGLAllocationParams * params,
    gsize struct_size, guint alloc_flags, GstGLAllocationParamsCopyFunc copy,
    GstGLAllocationParamsFreeFunc free, GstGLContext * context,
    gsize alloc_size, const GstAllocationParams * alloc_params,
    gpointer wrapped_data, gpointer gl_handle, gpointer user_data,
    GDestroyNotify notify)
{
  memset (params, 0, sizeof (*params));

  g_return_val_if_fail (struct_size > 0, FALSE);
  g_return_val_if_fail (copy != NULL, FALSE);
  g_return_val_if_fail (free != NULL, FALSE);
  g_return_val_if_fail (GST_IS_GL_CONTEXT (context), FALSE);

  params->struct_size = struct_size;
  params->alloc_size = alloc_size;
  params->copy = copy;
  params->free = free;
  params->alloc_flags = alloc_flags;
  params->context = gst_object_ref (context);
  if (alloc_params)
    params->alloc_params = gst_allocation_params_copy (alloc_params);
  params->notify = notify;
  params->user_data = user_data;
  params->wrapped_data = wrapped_data;
  params->gl_handle = gl_handle;

  return TRUE;
}

/**
 * gst_gl_allocation_params_copy:
 * @src: the #GstGLAllocationParams to initialize
 *
 * Returns: (transfer full): a copy of the #GstGLAllocationParams specified by
 *          @src
 *
 * Since: 1.8
 */
GstGLAllocationParams *
gst_gl_allocation_params_copy (GstGLAllocationParams * src)
{
  GstGLAllocationParams *dest;

  g_return_val_if_fail (src != NULL, NULL);

  dest = g_malloc0 (src->struct_size);

  if (src->copy)
    src->copy (src, dest);

  return dest;
}

/**
 * gst_gl_allocation_params_free:
 * @params: the #GstGLAllocationParams to initialize
 *
 * Frees the #GstGLAllocationParams and all associated data.
 *
 * Since: 1.8
 */
void
gst_gl_allocation_params_free (GstGLAllocationParams * params)
{
  if (params->free)
    params->free (params);

  g_free (params);
}

/**
 * gst_gl_allocation_params_free_data:
 * @params: the source #GstGLAllocationParams
 *
 * Frees the dynamically allocated data in @params.  Direct subclasses
 * should call this function in their own overridden free function.
 *
 * Since: 1.8
 */
void
gst_gl_allocation_params_free_data (GstGLAllocationParams * params)
{
  if (params->context)
    gst_object_unref (params->context);
  if (params->alloc_params)
    gst_allocation_params_free (params->alloc_params);
}

/**
 * gst_gl_allocation_params_copy_data:
 * @src: the source #GstGLAllocationParams
 * @dest: the destination #GstGLAllocationParams
 *
 * Copies the dynamically allocated data from @src to @dest.  Direct subclasses
 * should call this function in their own overridden copy function.
 *
 * Since: 1.8
 */
void
gst_gl_allocation_params_copy_data (GstGLAllocationParams * src,
    GstGLAllocationParams * dest)
{
  gst_gl_allocation_params_init (dest, src->struct_size, src->alloc_flags,
      src->copy, src->free, src->context, src->alloc_size, NULL,
      src->wrapped_data, src->gl_handle, src->user_data, src->notify);

  if (src->alloc_params)
    dest->alloc_params = gst_allocation_params_copy (src->alloc_params);
}

G_DEFINE_BOXED_TYPE (GstGLAllocationParams, gst_gl_allocation_params,
    (GBoxedCopyFunc) gst_gl_allocation_params_copy,
    (GBoxedFreeFunc) gst_gl_allocation_params_free);

/**
 * gst_gl_base_memory_alloc:
 * @allocator: a #GstGLBaseMemoryAllocator
 * @params: the #GstGLAllocationParams to allocate the memory with
 *
 * Returns: (transfer full) (nullable): a new #GstGLBaseMemory from @allocator with the requested @params.
 *
 * Since: 1.8
 */
GstGLBaseMemory *
gst_gl_base_memory_alloc (GstGLBaseMemoryAllocator * allocator,
    GstGLAllocationParams * params)
{
  GstGLBaseMemoryAllocatorClass *alloc_class;

  g_return_val_if_fail (GST_IS_GL_BASE_MEMORY_ALLOCATOR (allocator), NULL);

  alloc_class = GST_GL_BASE_MEMORY_ALLOCATOR_GET_CLASS (allocator);

  g_return_val_if_fail (alloc_class != NULL, NULL);
  g_return_val_if_fail (alloc_class->alloc != NULL, NULL);

  return alloc_class->alloc (allocator, params);
}
