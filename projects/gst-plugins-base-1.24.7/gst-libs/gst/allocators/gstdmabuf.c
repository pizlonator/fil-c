/* GStreamer dmabuf allocator
 * Copyright (C) 2013 Linaro SA
 * Author: Benjamin Gaignard <benjamin.gaignard@linaro.org> for Linaro.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for mordetails.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstfdmemory.h"
#include "gstdmabuf.h"

/**
 * SECTION:gstdmabuf
 * @title: GstDmaBufAllocator
 * @short_description: Memory wrapper for Linux dmabuf memory
 * @see_also: #GstMemory
 *
 * Since: 1.2
 */

#ifdef HAVE_LINUX_DMA_BUF_H
#include <sys/ioctl.h>
#include <linux/dma-buf.h>
#endif

GST_DEBUG_CATEGORY_STATIC (dmabuf_debug);
#define GST_CAT_DEFAULT dmabuf_debug

#define _do_init                                        \
    GST_DEBUG_CATEGORY_INIT (dmabuf_debug,              \
    "dmabuf", 0, "dmabuf memory");

G_DEFINE_TYPE_WITH_CODE (GstDmaBufAllocator, gst_dmabuf_allocator,
    GST_TYPE_FD_ALLOCATOR, _do_init);

static gpointer
gst_dmabuf_mem_map (GstMemory * gmem, GstMapInfo * info, gsize maxsize)
{
  GstAllocator *allocator = gmem->allocator;
  gpointer ret;

#ifdef HAVE_LINUX_DMA_BUF_H
  struct dma_buf_sync sync = { DMA_BUF_SYNC_START };

  if (info->flags & GST_MAP_READ)
    sync.flags |= DMA_BUF_SYNC_READ;

  if (info->flags & GST_MAP_WRITE)
    sync.flags |= DMA_BUF_SYNC_WRITE;
#endif

  ret = allocator->mem_map (gmem, maxsize, info->flags);

#ifdef HAVE_LINUX_DMA_BUF_H
  if (ret) {
    if (ioctl (gst_fd_memory_get_fd (gmem), DMA_BUF_IOCTL_SYNC, &sync) < 0)
      GST_WARNING_OBJECT (allocator, "Failed to synchronize DMABuf: %s (%i)",
          g_strerror (errno), errno);
  }
#endif

  return ret;
}

static void
gst_dmabuf_mem_unmap (GstMemory * gmem, GstMapInfo * info)
{
  GstAllocator *allocator = gmem->allocator;
#ifdef HAVE_LINUX_DMA_BUF_H
  struct dma_buf_sync sync = { DMA_BUF_SYNC_END };

  if (info->flags & GST_MAP_READ)
    sync.flags |= DMA_BUF_SYNC_READ;

  if (info->flags & GST_MAP_WRITE)
    sync.flags |= DMA_BUF_SYNC_WRITE;

  if (ioctl (gst_fd_memory_get_fd (gmem), DMA_BUF_IOCTL_SYNC, &sync) < 0)
    GST_WARNING_OBJECT (allocator, "Failed to synchronize DMABuf: %s (%i)",
        g_strerror (errno), errno);
#else
  GST_WARNING_OBJECT (allocator, "Using DMABuf without synchronization.");
#endif

  allocator->mem_unmap (gmem);
}

static void
gst_dmabuf_allocator_class_init (GstDmaBufAllocatorClass * klass)
{
}

static void
gst_dmabuf_allocator_init (GstDmaBufAllocator * allocator)
{
  GstAllocator *alloc = GST_ALLOCATOR_CAST (allocator);

  alloc->mem_type = GST_ALLOCATOR_DMABUF;
  alloc->mem_map_full = gst_dmabuf_mem_map;
  alloc->mem_unmap_full = gst_dmabuf_mem_unmap;
}

/**
 * gst_dmabuf_allocator_new:
 *
 * Return a new dmabuf allocator.
 *
 * Returns: (transfer full): a new dmabuf allocator. Use gst_object_unref() to
 * release the allocator after usage
 *
 * Since: 1.2
 */
GstAllocator *
gst_dmabuf_allocator_new (void)
{
  GstAllocator *alloc;

  alloc = g_object_new (GST_TYPE_DMABUF_ALLOCATOR, NULL);
  gst_object_ref_sink (alloc);

  return alloc;
}

/**
 * gst_dmabuf_allocator_alloc:
 * @allocator: allocator to be used for this memory
 * @fd: dmabuf file descriptor
 * @size: memory size
 *
 * Return a %GstMemory that wraps a dmabuf file descriptor.
 *
 * Returns: (transfer full) (nullable): a GstMemory based on @allocator.
 * When the buffer will be released dmabuf allocator will close the @fd.
 * The memory is only mmapped on gst_buffer_map() request.
 *
 * Since: 1.2
 */
GstMemory *
gst_dmabuf_allocator_alloc (GstAllocator * allocator, gint fd, gsize size)
{
  g_return_val_if_fail (GST_IS_DMABUF_ALLOCATOR (allocator), NULL);

  return gst_fd_allocator_alloc (allocator, fd, size, GST_FD_MEMORY_FLAG_NONE);
}

/**
 * gst_dmabuf_allocator_alloc_with_flags:
 * @allocator: allocator to be used for this memory
 * @fd: dmabuf file descriptor
 * @size: memory size
 * @flags: extra #GstFdMemoryFlags
 *
 * Return a %GstMemory that wraps a dmabuf file descriptor.
 *
 * Returns: (transfer full) (nullable): a GstMemory based on @allocator.
 *
 * When the buffer will be released the allocator will close the @fd unless
 * the %GST_FD_MEMORY_FLAG_DONT_CLOSE flag is specified.
 * The memory is only mmapped on gst_buffer_mmap() request.
 *
 * Since: 1.16
 */
GstMemory *
gst_dmabuf_allocator_alloc_with_flags (GstAllocator * allocator, gint fd,
    gsize size, GstFdMemoryFlags flags)
{
  g_return_val_if_fail (GST_IS_DMABUF_ALLOCATOR (allocator), NULL);

  return gst_fd_allocator_alloc (allocator, fd, size, flags);
}

/**
 * gst_dmabuf_memory_get_fd:
 * @mem: the memory to get the file descriptor
 *
 * Return the file descriptor associated with @mem.
 *
 * Returns: the file descriptor associated with the memory, or -1.  The file
 *     descriptor is still owned by the GstMemory.  Use dup to take a copy
 *     if you intend to use it beyond the lifetime of this GstMemory.
 *
 * Since: 1.2
 */
gint
gst_dmabuf_memory_get_fd (GstMemory * mem)
{
  g_return_val_if_fail (gst_is_dmabuf_memory (mem), -1);

  return gst_fd_memory_get_fd (mem);
}

/**
 * gst_is_dmabuf_memory:
 * @mem: the memory to be check
 *
 * Check if @mem is dmabuf memory.
 *
 * Returns: %TRUE if @mem is dmabuf memory, otherwise %FALSE
 *
 * Since: 1.2
 */
gboolean
gst_is_dmabuf_memory (GstMemory * mem)
{
  g_return_val_if_fail (mem != NULL, FALSE);

  return GST_IS_DMABUF_ALLOCATOR (mem->allocator);
}
