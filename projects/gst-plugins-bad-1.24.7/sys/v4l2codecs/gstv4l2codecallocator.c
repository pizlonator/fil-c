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

#include "gstv4l2codecallocator.h"

#include <gst/video/video.h>
#include <sys/types.h>

#define GST_CAT_DEFAULT allocator_debug
GST_DEBUG_CATEGORY_STATIC (allocator_debug);

typedef struct _GstV4l2CodecBuffer GstV4l2CodecBuffer;
struct _GstV4l2CodecBuffer
{
  gint index;

  GstMemory *mem[GST_VIDEO_MAX_PLANES];
  guint num_mems;

  guint outstanding_mems;
};

struct _GstV4l2CodecAllocator
{
  GstDmaBufAllocator parent;

  GQueue pool;
  gint pool_size;
  gboolean detached;

  GCond buffer_cond;
  gboolean flushing;

  GstV4l2Decoder *decoder;
  GstPadDirection direction;
};

G_DEFINE_TYPE_WITH_CODE (GstV4l2CodecAllocator, gst_v4l2_codec_allocator,
    GST_TYPE_DMABUF_ALLOCATOR,
    GST_DEBUG_CATEGORY_INIT (allocator_debug, "v4l2codecs-allocator", 0,
        "V4L2 Codecs Allocator"));

static gboolean gst_v4l2_codec_allocator_release (GstMiniObject * mini_object);

static GQuark
gst_v4l2_codec_buffer_quark (void)
{
  static gsize buffer_quark = 0;

  if (g_once_init_enter (&buffer_quark)) {
    GQuark quark = g_quark_from_string ("GstV4l2CodecBuffer");
    g_once_init_leave (&buffer_quark, quark);
  }

  return buffer_quark;
}

static GstV4l2CodecBuffer *
gst_v4l2_codec_buffer_new (GstAllocator * allocator, GstV4l2Decoder * decoder,
    GstPadDirection direction, gint index)
{
  GstV4l2CodecBuffer *buf;
  guint i, num_mems;
  gint fds[GST_VIDEO_MAX_PLANES];
  gsize sizes[GST_VIDEO_MAX_PLANES];
  gsize offsets[GST_VIDEO_MAX_PLANES];

  if (!gst_v4l2_decoder_export_buffer (decoder, direction, index, fds, sizes,
          offsets, &num_mems))
    return NULL;

  buf = g_new0 (GstV4l2CodecBuffer, 1);
  buf->index = index;
  buf->num_mems = num_mems;
  for (i = 0; i < buf->num_mems; i++) {
    GstMemory *mem = gst_fd_allocator_alloc (allocator, fds[i], sizes[i],
        GST_FD_MEMORY_FLAG_KEEP_MAPPED);
    gst_memory_resize (mem, offsets[i], sizes[i] - offsets[i]);

    GST_MINI_OBJECT (mem)->dispose = gst_v4l2_codec_allocator_release;
    gst_mini_object_set_qdata (GST_MINI_OBJECT (mem),
        gst_v4l2_codec_buffer_quark (), buf, NULL);

    /* On outstanding memory keeps a reference on the allocator, this is
     * needed to break the cycle. */
    gst_object_unref (mem->allocator);
    buf->mem[i] = mem;
  }

  GST_DEBUG_OBJECT (allocator, "Create buffer %i with %i memory fds",
      buf->index, buf->num_mems);

  return buf;
}

static void
gst_v4l2_codec_buffer_free (GstV4l2CodecBuffer * buf)
{
  guint i;

  g_warn_if_fail (buf->outstanding_mems == 0);

  GST_DEBUG_OBJECT (buf->mem[0]->allocator, "Freeing buffer %i", buf->index);

  for (i = 0; i < buf->num_mems; i++) {
    GstMemory *mem = buf->mem[i];
    GST_MINI_OBJECT (mem)->dispose = NULL;
    g_object_ref (mem->allocator);
    gst_memory_unref (mem);
  }

  g_free (buf);
}

static void
gst_v4l2_codec_buffer_acquire (GstV4l2CodecBuffer * buf)
{
  buf->outstanding_mems += buf->num_mems;
}

static gboolean
gst_v4l2_codec_buffer_release_mem (GstV4l2CodecBuffer * buf)
{
  return (--buf->outstanding_mems == 0);
}

static gboolean
gst_v4l2_codec_allocator_release (GstMiniObject * mini_object)
{
  GstMemory *mem = GST_MEMORY_CAST (mini_object);
  GstV4l2CodecAllocator *self = GST_V4L2_CODEC_ALLOCATOR (mem->allocator);
  GstV4l2CodecBuffer *buf;

  GST_OBJECT_LOCK (self);

  buf = gst_mini_object_get_qdata (mini_object, gst_v4l2_codec_buffer_quark ());
  gst_memory_ref (mem);

  if (gst_v4l2_codec_buffer_release_mem (buf)) {
    GST_DEBUG_OBJECT (self, "Placing back buffer %i into pool", buf->index);
    g_queue_push_tail (&self->pool, buf);
    g_cond_signal (&self->buffer_cond);
  }

  GST_OBJECT_UNLOCK (self);

  /* Keep last in case we are holding on the last allocator ref */
  g_object_unref (mem->allocator);

  /* Returns FALSE so that our mini object isn't freed */
  return FALSE;
}

static gboolean
gst_v4l2_codec_allocator_prepare (GstV4l2CodecAllocator * self)
{
  GstV4l2Decoder *decoder = self->decoder;
  GstPadDirection direction = self->direction;
  gint ret;
  guint i;

  ret = gst_v4l2_decoder_request_buffers (decoder, direction, self->pool_size);
  if (ret < self->pool_size) {
    if (ret >= 0)
      GST_ERROR_OBJECT (self,
          "%i buffer was needed, but only %i could be allocated",
          self->pool_size, ret);
    goto failed;
  }

  for (i = 0; i < self->pool_size; i++) {
    GstV4l2CodecBuffer *buf = gst_v4l2_codec_buffer_new (GST_ALLOCATOR (self),
        decoder, direction, i);
    g_queue_push_tail (&self->pool, buf);
  }

  return TRUE;

failed:
  gst_v4l2_decoder_request_buffers (decoder, direction, 0);
  return FALSE;
}

static void
gst_v4l2_codec_allocator_init (GstV4l2CodecAllocator * self)
{
  g_cond_init (&self->buffer_cond);
}

static void
gst_v4l2_codec_allocator_dispose (GObject * object)
{
  GstV4l2CodecAllocator *self = GST_V4L2_CODEC_ALLOCATOR (object);
  GstV4l2CodecBuffer *buf;

  while ((buf = g_queue_pop_head (&self->pool)))
    gst_v4l2_codec_buffer_free (buf);

  if (self->decoder) {
    gst_v4l2_codec_allocator_detach (self);
    gst_clear_object (&self->decoder);
  }

  G_OBJECT_CLASS (gst_v4l2_codec_allocator_parent_class)->dispose (object);
}

static void
gst_v4l2_codec_allocator_finalize (GObject * object)
{
  GstV4l2CodecAllocator *self = GST_V4L2_CODEC_ALLOCATOR (object);

  g_cond_clear (&self->buffer_cond);

  G_OBJECT_CLASS (gst_v4l2_codec_allocator_parent_class)->finalize (object);
}

static void
gst_v4l2_codec_allocator_class_init (GstV4l2CodecAllocatorClass * klass)
{
  GstAllocatorClass *allocator_class = GST_ALLOCATOR_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gst_v4l2_codec_allocator_dispose;
  object_class->finalize = gst_v4l2_codec_allocator_finalize;
  allocator_class->alloc = NULL;
}

GstV4l2CodecAllocator *
gst_v4l2_codec_allocator_new (GstV4l2Decoder * decoder,
    GstPadDirection direction, guint num_buffers)
{
  GstV4l2CodecAllocator *self =
      g_object_new (GST_TYPE_V4L2_CODEC_ALLOCATOR, NULL);

  self->decoder = g_object_ref (decoder);
  self->direction = direction;
  self->pool_size = num_buffers;

  if (!gst_v4l2_codec_allocator_prepare (self)) {
    g_object_unref (self);
    return NULL;
  }

  return self;
}

GstMemory *
gst_v4l2_codec_allocator_alloc (GstV4l2CodecAllocator * self)
{
  GstV4l2CodecBuffer *buf;
  GstMemory *mem = NULL;

  GST_OBJECT_LOCK (self);
  buf = g_queue_pop_head (&self->pool);
  if (buf) {
    GST_DEBUG_OBJECT (self, "Allocated buffer %i", buf->index);
    g_warn_if_fail (buf->num_mems == 1);
    mem = buf->mem[0];
    g_object_ref (mem->allocator);
    buf->outstanding_mems++;
  }
  GST_OBJECT_UNLOCK (self);

  return mem;
}

gboolean
gst_v4l2_codec_allocator_create_buffer (GstV4l2CodecAllocator * self)
{
  /* TODO implement */
  return FALSE;
}

gboolean
gst_v4l2_codec_allocator_wait_for_buffer (GstV4l2CodecAllocator * self)
{
  gboolean ret;

  GST_OBJECT_LOCK (self);
  while (self->pool.length == 0 && !self->flushing)
    g_cond_wait (&self->buffer_cond, GST_OBJECT_GET_LOCK (self));
  ret = !self->flushing;
  GST_OBJECT_UNLOCK (self);

  return ret;
}

gboolean
gst_v4l2_codec_allocator_prepare_buffer (GstV4l2CodecAllocator * self,
    GstBuffer * gstbuf)
{
  GstV4l2CodecBuffer *buf;
  guint i;

  GST_OBJECT_LOCK (self);

  buf = g_queue_pop_head (&self->pool);
  if (!buf) {
    GST_OBJECT_UNLOCK (self);
    return FALSE;
  }

  GST_DEBUG_OBJECT (self, "Allocated buffer %i", buf->index);

  gst_v4l2_codec_buffer_acquire (buf);
  for (i = 0; i < buf->num_mems; i++) {
    gst_buffer_append_memory (gstbuf, buf->mem[i]);
    g_object_ref (buf->mem[i]->allocator);
  }

  GST_OBJECT_UNLOCK (self);

  return TRUE;
}

guint
gst_v4l2_codec_allocator_get_pool_size (GstV4l2CodecAllocator * self)
{
  guint size;

  GST_OBJECT_LOCK (self);
  size = self->pool_size;
  GST_OBJECT_UNLOCK (self);

  return size;
}

void
gst_v4l2_codec_allocator_detach (GstV4l2CodecAllocator * self)
{
  GST_OBJECT_LOCK (self);
  if (!self->detached) {
    self->detached = TRUE;
    gst_v4l2_decoder_request_buffers (self->decoder, self->direction, 0);
  }
  GST_OBJECT_UNLOCK (self);
}

void
gst_v4l2_codec_allocator_set_flushing (GstV4l2CodecAllocator * self,
    gboolean flushing)
{
  GST_OBJECT_LOCK (self);
  self->flushing = flushing;
  if (flushing)
    g_cond_broadcast (&self->buffer_cond);
  GST_OBJECT_UNLOCK (self);
}

guint32
gst_v4l2_codec_memory_get_index (GstMemory * mem)
{
  GstV4l2CodecBuffer *buf;

  buf = gst_mini_object_get_qdata (GST_MINI_OBJECT (mem),
      gst_v4l2_codec_buffer_quark ());
  g_return_val_if_fail (buf, G_MAXUINT32);

  return buf->index;
}
