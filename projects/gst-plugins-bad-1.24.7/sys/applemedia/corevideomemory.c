/* GStreamer Apple Core Video memory
 * Copyright (C) 2015 Ilya Konstantinov
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

#include "corevideomemory.h"

GST_DEBUG_CATEGORY_STATIC (GST_CAT_APPLE_CORE_VIDEO_MEMORY);
#define GST_CAT_DEFAULT GST_CAT_APPLE_CORE_VIDEO_MEMORY

static const char *_lock_state_names[] = {
  "Unlocked", "Locked Read-Only", "Locked Read-Write"
};

/**
 * gst_apple_core_video_pixel_buffer_new:
 * @buf: an unlocked CVPixelBuffer
 *
 * Initializes a wrapper to manage locking state for a CVPixelBuffer.
 * This function expects to receive unlocked CVPixelBuffer, and further assumes
 * that no one else will lock it (as long as the wrapper exists).
 *
 * This function retains @buf.
 *
 * Returns: The wrapped @buf.
 */
GstAppleCoreVideoPixelBuffer *
gst_apple_core_video_pixel_buffer_new (CVPixelBufferRef buf)
{
  GstAppleCoreVideoPixelBuffer *gpixbuf =
      g_slice_new (GstAppleCoreVideoPixelBuffer);
  gpixbuf->refcount = 1;
  g_mutex_init (&gpixbuf->mutex);
  gpixbuf->buf = CVPixelBufferRetain (buf);
  gpixbuf->lock_state = GST_APPLE_CORE_VIDEO_MEMORY_UNLOCKED;
  gpixbuf->lock_count = 0;
  return gpixbuf;
}

GstAppleCoreVideoPixelBuffer *
gst_apple_core_video_pixel_buffer_ref (GstAppleCoreVideoPixelBuffer * gpixbuf)
{
  g_atomic_int_inc (&gpixbuf->refcount);
  return gpixbuf;
}

void
gst_apple_core_video_pixel_buffer_unref (GstAppleCoreVideoPixelBuffer * gpixbuf)
{
  if (g_atomic_int_dec_and_test (&gpixbuf->refcount)) {
    if (gpixbuf->lock_state != GST_APPLE_CORE_VIDEO_MEMORY_UNLOCKED) {
      GST_ERROR
          ("%p: CVPixelBuffer memory still locked (lock_count = %d), likely forgot to unmap GstAppleCoreVideoMemory",
          gpixbuf, gpixbuf->lock_count);
    }
    CVPixelBufferRelease (gpixbuf->buf);
    g_mutex_clear (&gpixbuf->mutex);
    g_slice_free (GstAppleCoreVideoPixelBuffer, gpixbuf);
  }
}

/**
 * gst_apple_core_video_pixel_buffer_lock:
 * @gpixbuf: the wrapped CVPixelBuffer
 * @flags: mapping flags for either read-only or read-write locking
 *
 * Locks the pixel buffer into CPU memory for reading only, or
 * reading and writing. The desired lock mode is deduced from @flags.
 *
 * For planar buffers, each plane's #GstAppleCoreVideoMemory will reference
 * the same #GstAppleCoreVideoPixelBuffer; therefore this function will be
 * called multiple times for the same @gpixbuf. Each call to this function
 * should be matched by a call to gst_apple_core_video_pixel_buffer_unlock().
 *
 * Notes:
 *
 * - Read-only locking improves performance by preventing Core Video
 *   from invalidating existing caches of the buffer’s contents.
 *
 * - Only the first call actually locks; subsequent calls succeed
 *   as long as their requested flags are compatible with how the buffer
 *   is already locked.
 *
 *   For example, the following code will succeed:
 *   |[<!-- language="C" -->
 *   gst_memory_map(plane1, GST_MAP_READWRITE);
 *   gst_memory_map(plane2, GST_MAP_READ);
 *   ]|
 *   while the ƒollowing code will fail:
 *   |[<!-- language="C" -->
 *   gst_memory_map(plane1, GST_MAP_READ);
 *   gst_memory_map(plane2, GST_MAP_READWRITE); /<!-- -->* ERROR: already locked for read-only *<!-- -->/
 *   ]|
 *
 * Returns: %TRUE if the buffer was locked as requested
 */
static gboolean
gst_apple_core_video_pixel_buffer_lock (GstAppleCoreVideoPixelBuffer * gpixbuf,
    GstMapFlags flags)
{
  CVReturn cvret;
  CVOptionFlags lockFlags;

  g_mutex_lock (&gpixbuf->mutex);

  switch (gpixbuf->lock_state) {
    case GST_APPLE_CORE_VIDEO_MEMORY_UNLOCKED:
      lockFlags = (flags & GST_MAP_WRITE) ? 0 : kCVPixelBufferLock_ReadOnly;
      cvret = CVPixelBufferLockBaseAddress (gpixbuf->buf, lockFlags);
      if (cvret != kCVReturnSuccess) {
        g_mutex_unlock (&gpixbuf->mutex);
        /* TODO: Map kCVReturnError etc. into strings */
        GST_ERROR ("%p: unable to lock base address for pixbuf %p: %d", gpixbuf,
            gpixbuf->buf, cvret);
        return FALSE;
      }
      gpixbuf->lock_state =
          (flags & GST_MAP_WRITE) ?
          GST_APPLE_CORE_VIDEO_MEMORY_LOCKED_READ_WRITE :
          GST_APPLE_CORE_VIDEO_MEMORY_LOCKED_READONLY;
      break;

    case GST_APPLE_CORE_VIDEO_MEMORY_LOCKED_READONLY:
      if (flags & GST_MAP_WRITE) {
        g_mutex_unlock (&gpixbuf->mutex);
        GST_ERROR ("%p: pixel buffer %p already locked for read-only access",
            gpixbuf, gpixbuf->buf);
        return FALSE;
      }
      break;

    case GST_APPLE_CORE_VIDEO_MEMORY_LOCKED_READ_WRITE:
      break;                    /* nothing to do, already most permissive mapping */
  }

  g_atomic_int_inc (&gpixbuf->lock_count);

  g_mutex_unlock (&gpixbuf->mutex);

  GST_DEBUG ("%p: pixbuf %p, %s (%d times)",
      gpixbuf,
      gpixbuf->buf,
      _lock_state_names[gpixbuf->lock_state], gpixbuf->lock_count);

  return TRUE;
}

/**
 * gst_apple_core_video_pixel_buffer_unlock:
 * @gpixbuf: the wrapped CVPixelBuffer
 *
 * Unlocks the pixel buffer from CPU memory. Should be called
 * for every gst_apple_core_video_pixel_buffer_lock() call.
 */
static gboolean
gst_apple_core_video_pixel_buffer_unlock (GstAppleCoreVideoPixelBuffer *
    gpixbuf)
{
  CVOptionFlags lockFlags;
  CVReturn cvret;

  if (gpixbuf->lock_state == GST_APPLE_CORE_VIDEO_MEMORY_UNLOCKED) {
    GST_ERROR ("%p: pixel buffer %p not locked", gpixbuf, gpixbuf->buf);
    return FALSE;
  }

  if (!g_atomic_int_dec_and_test (&gpixbuf->lock_count)) {
    return TRUE;                /* still locked, by current and/or other callers */
  }

  g_mutex_lock (&gpixbuf->mutex);

  lockFlags =
      (gpixbuf->lock_state ==
      GST_APPLE_CORE_VIDEO_MEMORY_LOCKED_READONLY) ? kCVPixelBufferLock_ReadOnly
      : 0;
  cvret = CVPixelBufferUnlockBaseAddress (gpixbuf->buf, lockFlags);
  if (cvret != kCVReturnSuccess) {
    g_mutex_unlock (&gpixbuf->mutex);
    g_atomic_int_inc (&gpixbuf->lock_count);
    /* TODO: Map kCVReturnError etc. into strings */
    GST_ERROR ("%p: unable to unlock base address for pixbuf %p: %d", gpixbuf,
        gpixbuf->buf, cvret);
    return FALSE;
  }

  gpixbuf->lock_state = GST_APPLE_CORE_VIDEO_MEMORY_UNLOCKED;

  g_mutex_unlock (&gpixbuf->mutex);

  GST_DEBUG ("%p: pixbuf %p, %s (%d locks remaining)",
      gpixbuf,
      gpixbuf->buf,
      _lock_state_names[gpixbuf->lock_state], gpixbuf->lock_count);

  return TRUE;
}

/*
 * GstAppleCoreVideoAllocator
 */

struct _GstAppleCoreVideoAllocatorClass
{
  GstAllocatorClass parent_class;
};

typedef struct _GstAppleCoreVideoAllocatorClass GstAppleCoreVideoAllocatorClass;

struct _GstAppleCoreVideoAllocator
{
  GstAllocator parent_instance;
};

typedef struct _GstAppleCoreVideoAllocator GstAppleCoreVideoAllocator;

/* GType for GstAppleCoreVideoAllocator */
GType gst_apple_core_video_allocator_get_type (void);
#define GST_TYPE_APPLE_CORE_VIDEO_ALLOCATOR             (gst_apple_core_video_allocator_get_type())
#define GST_IS_APPLE_CORE_VIDEO_ALLOCATOR(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_APPLE_CORE_VIDEO_ALLOCATOR))
#define GST_IS_APPLE_CORE_VIDEO_ALLOCATOR_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_APPLE_CORE_VIDEO_ALLOCATOR))
#define GST_APPLE_CORE_VIDEO_ALLOCATOR_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_APPLE_CORE_VIDEO_ALLOCATOR, GstAppleCoreVideoAllocatorClass))
#define GST_APPLE_CORE_VIDEO_ALLOCATOR(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_APPLE_CORE_VIDEO_ALLOCATOR, GstAppleCoreVideoAllocator))
#define GST_APPLE_CORE_VIDEO_ALLOCATOR_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_APPLE_CORE_VIDEO_ALLOCATOR, GstAppleCoreVideoAllocatorClass))

G_DEFINE_TYPE (GstAppleCoreVideoAllocator, gst_apple_core_video_allocator,
    GST_TYPE_ALLOCATOR);

/* Name for allocator registration */
#define GST_APPLE_CORE_VIDEO_ALLOCATOR_NAME "AppleCoreVideoMemory"

/* Singleton instance of GstAppleCoreVideoAllocator */
static GstAppleCoreVideoAllocator *_apple_core_video_allocator;

/**
 * gst_apple_core_video_memory_init:
 *
 * Initializes the Core Video Memory allocator. This function must be called
 * before #GstAppleCoreVideoMemory can be created.
 *
 * It is safe to call this function multiple times.
 */
void
gst_apple_core_video_memory_init (void)
{
  static gsize _init = 0;

  if (g_once_init_enter (&_init)) {
    GST_DEBUG_CATEGORY_INIT (GST_CAT_APPLE_CORE_VIDEO_MEMORY, "corevideomemory",
        0, "Apple Core Video Memory");

    _apple_core_video_allocator =
        g_object_new (GST_TYPE_APPLE_CORE_VIDEO_ALLOCATOR, NULL);
    gst_object_ref_sink (_apple_core_video_allocator);

    gst_allocator_register (GST_APPLE_CORE_VIDEO_ALLOCATOR_NAME,
        gst_object_ref (_apple_core_video_allocator));
    g_once_init_leave (&_init, 1);
  }
}

/**
 * gst_is_apple_core_video_memory:
 * @mem: #GstMemory
 *
 * Checks whether @mem is backed by a CVPixelBuffer.
 * This has limited use since #GstAppleCoreVideoMemory is transparently
 * mapped into CPU memory on request.
 *
 * Returns: %TRUE when @mem is backed by a CVPixelBuffer
 */
gboolean
gst_is_apple_core_video_memory (GstMemory * mem)
{
  g_return_val_if_fail (mem != NULL, FALSE);

  return GST_IS_APPLE_CORE_VIDEO_ALLOCATOR (mem->allocator);
}

/**
 * gst_apple_core_video_memory_new:
 *
 * Helper function for gst_apple_core_video_mem_share().
 * Users should call gst_apple_core_video_memory_new_wrapped() instead.
 */
static GstAppleCoreVideoMemory *
gst_apple_core_video_memory_new (GstMemoryFlags flags, GstMemory * parent,
    GstAppleCoreVideoPixelBuffer * gpixbuf, gsize plane, gsize maxsize,
    gsize align, gsize offset, gsize size)
{
  GstAppleCoreVideoMemory *mem;

  g_return_val_if_fail (gpixbuf != NULL, NULL);

  mem = g_slice_new0 (GstAppleCoreVideoMemory);
  gst_memory_init (GST_MEMORY_CAST (mem), flags,
      GST_ALLOCATOR_CAST (_apple_core_video_allocator), parent, maxsize, align,
      offset, size);

  mem->gpixbuf = gst_apple_core_video_pixel_buffer_ref (gpixbuf);
  mem->plane = plane;

  GST_DEBUG ("%p: gpixbuf %p, plane: %" G_GSSIZE_FORMAT ", size %"
      G_GSIZE_FORMAT, mem, mem->gpixbuf, mem->plane, mem->mem.size);

  return mem;
}

/**
 * gst_apple_core_video_memory_new_wrapped:
 * @gpixbuf: the backing #GstAppleCoreVideoPixelBuffer
 * @plane: the plane this memory will represent, or 0 for non-planar buffer
 * @size: the size of the buffer or specific plane
 *
 * Returns: a newly allocated #GstAppleCoreVideoMemory
 */
GstAppleCoreVideoMemory *
gst_apple_core_video_memory_new_wrapped (GstAppleCoreVideoPixelBuffer * gpixbuf,
    gsize plane, gsize size)
{
  return gst_apple_core_video_memory_new (0, NULL, gpixbuf, plane, size, 0, 0,
      size);
}

static gpointer
gst_apple_core_video_mem_map (GstMemory * gmem, gsize maxsize,
    GstMapFlags flags)
{
  GstAppleCoreVideoMemory *mem = (GstAppleCoreVideoMemory *) gmem;
  gpointer ret;

  if (!gst_apple_core_video_pixel_buffer_lock (mem->gpixbuf, flags))
    return NULL;

  if (CVPixelBufferIsPlanar (mem->gpixbuf->buf)) {
    ret = CVPixelBufferGetBaseAddressOfPlane (mem->gpixbuf->buf, mem->plane);

    if (ret != NULL)
      GST_DEBUG ("%p: pixbuf %p plane %" G_GSIZE_FORMAT
          " flags %08x: mapped %p", mem, mem->gpixbuf->buf, mem->plane, flags,
          ret);
    else
      GST_ERROR ("%p: invalid plane base address (NULL) for pixbuf %p plane %"
          G_GSIZE_FORMAT, mem, mem->gpixbuf->buf, mem->plane);
  } else {
    ret = CVPixelBufferGetBaseAddress (mem->gpixbuf->buf);

    if (ret != NULL)
      GST_DEBUG ("%p: pixbuf %p flags %08x: mapped %p", mem, mem->gpixbuf->buf,
          flags, ret);
    else
      GST_ERROR ("%p: invalid base address (NULL) for pixbuf %p"
          G_GSIZE_FORMAT, mem, mem->gpixbuf->buf);
  }

  return ret;
}

static void
gst_apple_core_video_mem_unmap (GstMemory * gmem)
{
  GstAppleCoreVideoMemory *mem = (GstAppleCoreVideoMemory *) gmem;
  (void) gst_apple_core_video_pixel_buffer_unlock (mem->gpixbuf);
  GST_DEBUG ("%p: pixbuf %p plane %" G_GSIZE_FORMAT, mem,
      mem->gpixbuf->buf, mem->plane);
}

static GstMemory *
gst_apple_core_video_mem_share (GstMemory * gmem, gssize offset, gssize size)
{
  GstAppleCoreVideoMemory *mem;
  GstMemory *parent, *sub;

  mem = (GstAppleCoreVideoMemory *) gmem;

  /* find the real parent */
  parent = gmem->parent;
  if (parent == NULL)
    parent = gmem;

  if (size == -1)
    size = gmem->size - offset;

  /* the shared memory is always readonly */
  sub =
      GST_MEMORY_CAST (gst_apple_core_video_memory_new (GST_MINI_OBJECT_FLAGS
          (parent) | GST_MINI_OBJECT_FLAG_LOCK_READONLY, parent, mem->gpixbuf,
          mem->plane, gmem->maxsize, gmem->align, gmem->offset + offset, size));

  return sub;
}

static gboolean
gst_apple_core_video_mem_is_span (GstMemory * mem1, GstMemory * mem2,
    gsize * offset)
{
  /* We may only return FALSE since:
   * 1) Core Video gives no guarantees about planes being consecutive.
   *    We may only know this after mapping.
   * 2) GstAppleCoreVideoMemory instances for planes do not share a common
   *    parent -- i.e. they're not offsets into the same parent
   *    memory instance.
   *
   * It's not unlikely that planes will be stored in consecutive memory
   * but it should be checked by the user after mapping.
   */
  return FALSE;
}

static void
gst_apple_core_video_mem_free (GstAllocator * allocator, GstMemory * gmem)
{
  GstAppleCoreVideoMemory *mem = (GstAppleCoreVideoMemory *) gmem;

  gst_apple_core_video_pixel_buffer_unref (mem->gpixbuf);

  g_slice_free (GstAppleCoreVideoMemory, mem);
}

static void
gst_apple_core_video_allocator_class_init (GstAppleCoreVideoAllocatorClass *
    klass)
{
  GstAllocatorClass *allocator_class;

  allocator_class = (GstAllocatorClass *) klass;

  /* we don't do allocations, only wrap existing pixel buffers */
  allocator_class->alloc = NULL;
  allocator_class->free = gst_apple_core_video_mem_free;
}

static void
gst_apple_core_video_allocator_init (GstAppleCoreVideoAllocator * allocator)
{
  GstAllocator *alloc = GST_ALLOCATOR_CAST (allocator);

  alloc->mem_type = GST_APPLE_CORE_VIDEO_ALLOCATOR_NAME;
  alloc->mem_map = gst_apple_core_video_mem_map;
  alloc->mem_unmap = gst_apple_core_video_mem_unmap;
  alloc->mem_share = gst_apple_core_video_mem_share;
  alloc->mem_is_span = gst_apple_core_video_mem_is_span;

  GST_OBJECT_FLAG_SET (allocator, GST_ALLOCATOR_FLAG_CUSTOM_ALLOC);
}
