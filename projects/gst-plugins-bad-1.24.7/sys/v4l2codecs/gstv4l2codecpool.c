/* GStreamer
 * Copyright (C) 2020 Collabora Ltd.
 *   Author: Nicolas Dufresne <nicolas.dufresne@collabora.com>
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

#include "gstv4l2codecpool.h"

#include <string.h>

struct _GstV4l2CodecPool
{
  GstBufferPool parent;
  GstAtomicQueue *queue;
  GstV4l2CodecAllocator *allocator;
  /* Used to set GstVideoMeta */
  GstVideoInfo *vinfo;
};

G_DEFINE_TYPE (GstV4l2CodecPool, gst_v4l2_codec_pool, GST_TYPE_BUFFER_POOL);

static GstBuffer *
gst_v4l2_codec_pool_create_empty_buffer (void)
{
  GstVideoMeta *vmeta;
  GstBuffer *buffer = gst_buffer_new ();

  vmeta = gst_buffer_add_video_meta (buffer, 0, GST_VIDEO_FORMAT_NV12, 1, 1);
  GST_META_FLAG_SET (vmeta, GST_META_FLAG_POOLED);

  return buffer;
}

static GstFlowReturn
gst_v4l2_codec_pool_acquire_buffer (GstBufferPool * pool, GstBuffer ** buffer,
    GstBufferPoolAcquireParams * params)
{
  GstV4l2CodecPool *self = GST_V4L2_CODEC_POOL (pool);
  GstBuffer *buf;
  GstVideoMeta *vmeta;

  /* A GstVideoInfo must be set before buffer can be acquired */
  g_return_val_if_fail (self->vinfo, GST_FLOW_ERROR);

  buf = gst_atomic_queue_pop (self->queue);
  if (!buf)
    buf = gst_v4l2_codec_pool_create_empty_buffer ();

  /* First, just try and obtain a buffer. */
  if (!gst_v4l2_codec_allocator_prepare_buffer (self->allocator, buf)) {
    GstFlowReturn flow_ret = GST_FLOW_OK;

    /* If none were available, try and allocate one. */
    if (!gst_v4l2_codec_allocator_create_buffer (self->allocator)) {
      /* Otherwise, wait if this is allowed. */
      if (params && params->flags & GST_BUFFER_POOL_ACQUIRE_FLAG_DONTWAIT) {
        flow_ret = GST_FLOW_EOS;
      } else {
        if (!gst_v4l2_codec_allocator_wait_for_buffer (self->allocator))
          flow_ret = GST_FLOW_FLUSHING;
      }
    }

    if (flow_ret != GST_FLOW_OK) {
      gst_atomic_queue_push (self->queue, buf);
      return flow_ret;
    }

    /* Finally, pop the buffer we created or waited for. */
    gst_v4l2_codec_allocator_prepare_buffer (self->allocator, buf);
  }

  vmeta = gst_buffer_get_video_meta (buf);
  vmeta->format = GST_VIDEO_INFO_FORMAT (self->vinfo);
  vmeta->width = GST_VIDEO_INFO_WIDTH (self->vinfo);
  vmeta->height = GST_VIDEO_INFO_HEIGHT (self->vinfo);
  vmeta->n_planes = GST_VIDEO_INFO_N_PLANES (self->vinfo);
  memcpy (vmeta->offset, self->vinfo->offset, sizeof (vmeta->offset));
  memcpy (vmeta->stride, self->vinfo->stride, sizeof (vmeta->stride));

  *buffer = buf;
  return GST_FLOW_OK;
}

static void
gst_v4l2_codec_pool_reset_buffer (GstBufferPool * pool, GstBuffer * buffer)
{
  GstBufferPoolClass *klass =
      GST_BUFFER_POOL_CLASS (gst_v4l2_codec_pool_parent_class);

  /* Clears all the memories and only pool the GstBuffer objects */
  gst_buffer_remove_all_memory (buffer);
  klass->reset_buffer (pool, buffer);
  GST_BUFFER_FLAGS (buffer) = 0;
}

static void
gst_v4l2_codec_pool_release_buffer (GstBufferPool * pool, GstBuffer * buffer)
{
  GstV4l2CodecPool *self = GST_V4L2_CODEC_POOL (pool);
  gst_atomic_queue_push (self->queue, buffer);
}

static void
gst_v4l2_codec_pool_init (GstV4l2CodecPool * self)
{
  self->queue = gst_atomic_queue_new (4);
}

static void
gst_v4l2_codec_pool_finalize (GObject * object)
{
  GstV4l2CodecPool *self = GST_V4L2_CODEC_POOL (object);
  GstBuffer *buf;

  while ((buf = gst_atomic_queue_pop (self->queue)))
    gst_buffer_unref (buf);

  gst_atomic_queue_unref (self->queue);
  g_object_unref (self->allocator);

  if (self->vinfo)
    gst_video_info_free (self->vinfo);

  G_OBJECT_CLASS (gst_v4l2_codec_pool_parent_class)->finalize (object);
}

static void
gst_v4l2_codec_pool_class_init (GstV4l2CodecPoolClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GstBufferPoolClass *pool_class = GST_BUFFER_POOL_CLASS (klass);

  object_class->finalize = gst_v4l2_codec_pool_finalize;
  pool_class->start = NULL;
  pool_class->acquire_buffer = gst_v4l2_codec_pool_acquire_buffer;
  pool_class->reset_buffer = gst_v4l2_codec_pool_reset_buffer;
  pool_class->release_buffer = gst_v4l2_codec_pool_release_buffer;
}

GstV4l2CodecPool *
gst_v4l2_codec_pool_new (GstV4l2CodecAllocator * allocator,
    const GstVideoInfo * vinfo)
{
  GstV4l2CodecPool *pool = g_object_new (GST_TYPE_V4L2_CODEC_POOL, NULL);
  gsize pool_size;

  pool->allocator = g_object_ref (allocator);
  pool->vinfo = gst_video_info_copy (vinfo);

  pool_size = gst_v4l2_codec_allocator_get_pool_size (allocator);
  for (gsize i = 0; i < pool_size; i++) {
    GstBuffer *buffer = gst_v4l2_codec_pool_create_empty_buffer ();
    gst_atomic_queue_push (pool->queue, buffer);
  }

  return pool;
}

guint32
gst_v4l2_codec_buffer_get_index (GstBuffer * buffer)
{
  GstMemory *mem = gst_buffer_peek_memory (buffer, 0);
  return gst_v4l2_codec_memory_get_index (mem);
}
