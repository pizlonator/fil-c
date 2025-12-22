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
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GST_APPLE_CORE_VIDEO_MEMORY_H__
#define __GST_APPLE_CORE_VIDEO_MEMORY_H__

#include <gst/gst.h>

#include "CoreVideo/CoreVideo.h"

G_BEGIN_DECLS

/**
 * GstAppleCoreVideoLockState:
 *
 * Specifies whether the backing CVPixelBuffer is locked for read-only
 * or read-write.
 *
 * Locking for reading only improves performance by preventing
 * Core Video from invalidating existing caches of the buffer’s contents.
 */
typedef enum
{
  GST_APPLE_CORE_VIDEO_MEMORY_UNLOCKED,
  GST_APPLE_CORE_VIDEO_MEMORY_LOCKED_READONLY,
  GST_APPLE_CORE_VIDEO_MEMORY_LOCKED_READ_WRITE
} GstAppleCoreVideoLockState;

/**
 * GstAppleCoreVideoPixelBuffer:
 *
 * This structure wraps CVPixelBuffer, managing its lock states and reference count.
 * It will be referenced by one or more #GstAppleCoreVideoMemory.
 */
typedef struct
{
  guint refcount;
  GMutex mutex;
  CVPixelBufferRef buf;
  /* Allows mem_map to refuse Read-Write locking a buffer that was previously
   * locked for Read-Only. */
  GstAppleCoreVideoLockState lock_state;
  /* Counts the number of times the buffer was locked.
   * Only the first lock affects whether it's just for reading
   * or for reading and writing, as reflected in @lock_state. */
  guint lock_count;
} GstAppleCoreVideoPixelBuffer;

/**
 * GstAppleCoreVideoMemory:
 *
 * Represents a video plane or an entire (non-planar) video image,
 * backed by a CVPixelBuffer.
 *
 * This structure shares a #GstAppleCoreVideoPixelBuffer instance
 * with other instances.
 */
typedef struct
{
  GstMemory mem;

  GstAppleCoreVideoPixelBuffer *gpixbuf;
  size_t plane;
} GstAppleCoreVideoMemory;

void
gst_apple_core_video_memory_init (void);

GstAppleCoreVideoPixelBuffer *
gst_apple_core_video_pixel_buffer_new (CVPixelBufferRef pixbuf);

GstAppleCoreVideoPixelBuffer *
gst_apple_core_video_pixel_buffer_ref (GstAppleCoreVideoPixelBuffer * shared);

void
gst_apple_core_video_pixel_buffer_unref (GstAppleCoreVideoPixelBuffer * shared);

gboolean
gst_is_apple_core_video_memory (GstMemory * mem);

GstAppleCoreVideoMemory *
gst_apple_core_video_memory_new_wrapped (GstAppleCoreVideoPixelBuffer * shared, gsize plane, gsize size);

G_END_DECLS

#endif /* __GST_APPLE_CORE_VIDEO_MEMORY_H__ */
