/* GStreamer shared memory allocator
 *
 * Copyright (C) 2012 Intel Corporation
 * Copyright (C) 2012 Sreerenj Balachandran <sreerenj.balachandran@intel.com>
 * Copyright (C) 2023 Netflix Inc.
 *  Author: Xavier Claessens <xavier.claessens@collabora.com>
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

/**
 * SECTION:gstshmallocator
 * @title: GstShmAllocator
 * @short_description: Allocator for file-descriptor backed shared memory
 * @see_also: #GstMemory and #GstFdAllocator
 *
 * This is a subclass of #GstFdAllocator that implements the
 * gst_allocator_alloc() method using `memfd_create()` when available, POSIX
 * `shm_open()` otherwise. Platforms not supporting any of those (Windows) will
 * always return %NULL.
 *
 * Note that allocating new shared memories has a significant performance cost,
 * it is thus recommended to keep a pool of pre-allocated #GstMemory, using
 * #GstBufferPool. For that reason, this allocator has the
 * %GST_ALLOCATOR_FLAG_NO_COPY flag set.
 *
 * Since: 1.24
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/stat.h>
#include "gstshmallocator.h"

#ifdef HAVE_MMAP
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#endif

struct _GstShmAllocator
{
  GstFdAllocator parent;
};

#define GST_CAT_DEFAULT gst_shm_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

G_DEFINE_TYPE_WITH_CODE (GstShmAllocator, gst_shm_allocator,
    GST_TYPE_FD_ALLOCATOR,
    GST_DEBUG_CATEGORY_INIT (gst_shm_debug, "shmallocator", 0,
        "Shared memory allocator");
    );

static GstMemory *
gst_shm_allocator_alloc (GstAllocator * allocator, gsize size,
    GstAllocationParams * params)
{
#if defined(HAVE_MEMFD_CREATE) || defined(HAVE_SHM_OPEN)
  GstShmAllocator *self = GST_SHM_ALLOCATOR (allocator);
  int fd;
  GstMemory *mem;
  GstMapInfo info;

  /* See _sysmem_new_block() for details */
  gsize maxsize = size + params->prefix + params->padding;
  gsize align = params->align;
  align |= gst_memory_alignment;
  maxsize += align;

#ifdef HAVE_MEMFD_CREATE
  fd = memfd_create ("gst-shm", MFD_CLOEXEC | MFD_ALLOW_SEALING);
  if (fd < 0) {
    GST_ERROR_OBJECT (self, "memfd_create() failed: %s", strerror (errno));
    return NULL;
  }
#else
  {
    char filename[1024];
    static int init = 0;
    int flags = O_RDWR | O_CREAT | O_EXCL;
    int perms = S_IRUSR | S_IWUSR | S_IRGRP;

    snprintf (filename, 1024, "/gst-shm.%d.%d", getpid (), init++);
    fd = shm_open (filename, flags, perms);
    if (fd < 0) {
      GST_ERROR_OBJECT (self, "shm_open() failed: %s", strerror (errno));
      return NULL;
    }

    shm_unlink (filename);
  }
#endif

  if (ftruncate (fd, maxsize) < 0) {
    GST_ERROR_OBJECT (self, "ftruncate failed: %s", strerror (errno));
    close (fd);
    return NULL;
  }

  mem = gst_fd_allocator_alloc (allocator, fd, maxsize,
      GST_FD_MEMORY_FLAG_KEEP_MAPPED);
  if (G_UNLIKELY (!mem)) {
    GST_ERROR_OBJECT (self, "GstFdMemory allocation failed");
    close (fd);
    return NULL;
  }

  /* We use GST_FD_MEMORY_FLAG_KEEP_MAPPED, so make sure the first map is RW. */
  if (!gst_memory_map (mem, &info, GST_MAP_READWRITE)) {
    GST_ERROR_OBJECT (self, "GstFdMemory map failed");
    gst_memory_unref (mem);
    return NULL;
  }

  /* See _sysmem_new_block() for details */
  guint8 *data = info.data;
  gsize aoffset;
  gsize asize = maxsize;
  if ((aoffset = ((guintptr) data & align))) {
    aoffset = (align + 1) - aoffset;
    data += aoffset;
    asize = maxsize - aoffset;
  }

  if (params->prefix && (params->flags & GST_MEMORY_FLAG_ZERO_PREFIXED))
    memset (data, 0, params->prefix);

  gsize padding = asize - (params->prefix + size);
  if (padding && (params->flags & GST_MEMORY_FLAG_ZERO_PADDED))
    memset (data + params->prefix + size, 0, padding);

  mem->align = align;
  mem->size = size;
  mem->offset = params->prefix + aoffset;

  gst_memory_unmap (mem, &info);

#ifdef HAVE_MEMFD_CREATE
  fcntl (fd, F_ADD_SEALS, F_SEAL_SHRINK);
#endif

  return mem;
#else
  return NULL;
#endif
}

static void
gst_shm_allocator_class_init (GstShmAllocatorClass * klass)
{
  GstAllocatorClass *alloc_class = (GstAllocatorClass *) klass;

  alloc_class->alloc = GST_DEBUG_FUNCPTR (gst_shm_allocator_alloc);
}

static void
gst_shm_allocator_init (GstShmAllocator * self)
{
  GstAllocator *alloc = GST_ALLOCATOR_CAST (self);

  alloc->mem_type = GST_ALLOCATOR_SHM;

  GST_OBJECT_FLAG_SET (self, GST_ALLOCATOR_FLAG_NO_COPY);
}

/**
 * gst_shm_allocator_init_once:
 *
 * Register a #GstShmAllocator using gst_allocator_register() with the name
 * %GST_ALLOCATOR_SHM. This is no-op after the first call.
 *
 * Since: 1.24
 */
void
gst_shm_allocator_init_once (void)
{
  static gsize _init = 0;

  if (g_once_init_enter (&_init)) {
    GstAllocator *alloc;

    alloc = (GstAllocator *) g_object_new (GST_TYPE_SHM_ALLOCATOR, NULL);
    gst_object_ref_sink (alloc);
    gst_allocator_register (GST_ALLOCATOR_SHM, alloc);

    g_once_init_leave (&_init, 1);
  }
}

/**
 * gst_shm_allocator_get:
 *
 * Get the #GstShmAllocator singleton previously registered with
 * gst_shm_allocator_init_once().
 *
 * Returns: (transfer full) (nullable): a #GstAllocator or %NULL if
 * gst_shm_allocator_init_once() has not been previously called.
 *
 * Since: 1.24
 */
GstAllocator *
gst_shm_allocator_get (void)
{
  return gst_allocator_find (GST_ALLOCATOR_SHM);
}
