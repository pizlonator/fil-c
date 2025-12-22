/* GStreamer
 * Copyright (C) 2023 Seungha Yang <seungha@centricular.com>
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

#include "gstdwritebitmappool.h"

GST_DEBUG_CATEGORY_STATIC (dwrite_bitmap_pool_debug);
#define GST_CAT_DEFAULT dwrite_bitmap_pool_debug

struct _GstDWriteBitmapPool
{
  GstBufferPool parent;

  GstDWriteBitmapAllocator *alloc;
  GstVideoInfo info;
};

#define gst_dwrite_bitmap_pool_parent_class parent_class
G_DEFINE_TYPE (GstDWriteBitmapPool,
    gst_dwrite_bitmap_pool, GST_TYPE_BUFFER_POOL);

static void gst_dwrite_bitmap_pool_finalize (GObject * object);
static const gchar **gst_dwrite_bitmap_pool_get_options (GstBufferPool * pool);
static gboolean gst_dwrite_bitmap_pool_set_config (GstBufferPool * pool,
    GstStructure * config);
static GstFlowReturn gst_dwrite_bitmap_pool_alloc_buffer (GstBufferPool *
    pool, GstBuffer ** buffer, GstBufferPoolAcquireParams * params);

static void
gst_dwrite_bitmap_pool_class_init (GstDWriteBitmapPoolClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GstBufferPoolClass *pool_class = GST_BUFFER_POOL_CLASS (klass);

  object_class->finalize = gst_dwrite_bitmap_pool_finalize;

  pool_class->get_options =
      GST_DEBUG_FUNCPTR (gst_dwrite_bitmap_pool_get_options);
  pool_class->set_config =
      GST_DEBUG_FUNCPTR (gst_dwrite_bitmap_pool_set_config);
  pool_class->alloc_buffer =
      GST_DEBUG_FUNCPTR (gst_dwrite_bitmap_pool_alloc_buffer);

  GST_DEBUG_CATEGORY_INIT (dwrite_bitmap_pool_debug,
      "dwritebitmappool", 0, "dwritebitmappool");
}

static void
gst_dwrite_bitmap_pool_init (GstDWriteBitmapPool * self)
{
}

static void
gst_dwrite_bitmap_pool_finalize (GObject * object)
{
  GstDWriteBitmapPool *self = GST_DWRITE_BITMAP_POOL (object);

  gst_clear_object (&self->alloc);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static const gchar **
gst_dwrite_bitmap_pool_get_options (GstBufferPool * pool)
{
  static const gchar *options[] = { GST_BUFFER_POOL_OPTION_VIDEO_META,
    nullptr
  };

  return options;
}

static gboolean
gst_dwrite_bitmap_pool_set_config (GstBufferPool * pool, GstStructure * config)
{
  GstDWriteBitmapPool *self = GST_DWRITE_BITMAP_POOL (pool);
  GstCaps *caps;
  guint size, min_buffers, max_buffers;
  GstDWriteBitmapMemory *dmem;
  GstMemory *mem;

  if (!gst_buffer_pool_config_get_params (config,
          &caps, &size, &min_buffers, &max_buffers)) {
    GST_WARNING_OBJECT (self, "Invalid config");
    return FALSE;
  }

  if (!caps) {
    GST_WARNING_OBJECT (self, "No caps");
    return FALSE;
  }

  if (!gst_video_info_from_caps (&self->info, caps)) {
    GST_WARNING_OBJECT (self, "Invalid caps");
    return FALSE;
  }

  if (GST_VIDEO_INFO_FORMAT (&self->info) != GST_VIDEO_FORMAT_BGRA) {
    GST_WARNING_OBJECT (self, "Unsupported format");
    return FALSE;
  }

  if (!self->alloc)
    self->alloc = gst_dwrite_bitmap_allocator_new ();

  if (!self->alloc) {
    GST_WARNING_OBJECT (self, "Couldn't create allocator");
    return FALSE;
  }

  mem = gst_dwrite_bitmap_allocator_alloc (self->alloc,
      self->info.width, self->info.height);
  if (!mem) {
    GST_WARNING_OBJECT (self, "Couldn't allocate memory");
    return FALSE;
  }

  dmem = (GstDWriteBitmapMemory *) mem;
  size = dmem->info.size;
  gst_memory_unref (mem);

  gst_buffer_pool_config_set_params (config, caps, size, min_buffers,
      max_buffers);

  return GST_BUFFER_POOL_CLASS (parent_class)->set_config (pool, config);
}

static GstFlowReturn
gst_dwrite_bitmap_pool_alloc_buffer (GstBufferPool * pool,
    GstBuffer ** buffer, GstBufferPoolAcquireParams * params)
{
  GstDWriteBitmapPool *self = GST_DWRITE_BITMAP_POOL (pool);
  GstBuffer *buf;
  GstMemory *mem;
  GstDWriteBitmapMemory *dmem;

  *buffer = nullptr;

  if (!self->alloc) {
    GST_ERROR_OBJECT (self, "Allocator was not configured");
    return GST_FLOW_ERROR;
  }

  mem = gst_dwrite_bitmap_allocator_alloc (self->alloc,
      self->info.width, self->info.height);
  if (!mem) {
    GST_ERROR_OBJECT (self, "Couldn't allocate memory");
    return GST_FLOW_ERROR;
  }

  buf = gst_buffer_new ();
  gst_buffer_append_memory (buf, mem);

  dmem = (GstDWriteBitmapMemory *) mem;

  gst_buffer_add_video_meta_full (buf, GST_VIDEO_FRAME_FLAG_NONE,
      GST_VIDEO_INFO_FORMAT (&dmem->info), GST_VIDEO_INFO_WIDTH (&dmem->info),
      GST_VIDEO_INFO_HEIGHT (&dmem->info),
      GST_VIDEO_INFO_N_PLANES (&dmem->info), dmem->info.offset,
      dmem->info.stride);

  *buffer = buf;

  return GST_FLOW_OK;
}

GstBufferPool *
gst_dwrite_bitmap_pool_new (void)
{
  GstBufferPool *pool = (GstBufferPool *)
      g_object_new (GST_TYPE_DWRITE_BITMAP_POOL, nullptr);

  gst_object_ref_sink (pool);

  return pool;
}
