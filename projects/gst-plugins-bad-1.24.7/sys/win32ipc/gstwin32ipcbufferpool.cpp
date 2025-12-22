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

#include "gstwin32ipcbufferpool.h"
#include "gstwin32ipcmemory.h"
#include <gst/video/video.h>

GST_DEBUG_CATEGORY_STATIC (gst_win32_ipc_buffer_pool_debug);
#define GST_CAT_DEFAULT gst_win32_ipc_buffer_pool_debug

struct _GstWin32IpcBufferPool
{
  GstBufferPool parent;

  GstWin32IpcAllocator *alloc;
  GstVideoInfo info;
  gboolean add_videometa;
};

#define gst_win32_ipc_buffer_pool_parent_class parent_class
G_DEFINE_TYPE (GstWin32IpcBufferPool,
    gst_win32_ipc_buffer_pool, GST_TYPE_BUFFER_POOL);

static void gst_win32_ipc_buffer_pool_dispose (GObject * object);
static const gchar **gst_win32_ipc_buffer_pool_get_options (GstBufferPool *
    pool);
static gboolean gst_win32_ipc_buffer_pool_set_config (GstBufferPool * pool,
    GstStructure * config);
static GstFlowReturn gst_win32_ipc_buffer_pool_alloc_buffer (GstBufferPool *
    pool, GstBuffer ** buffer, GstBufferPoolAcquireParams * params);
static gboolean gst_win32_ipc_buffer_pool_start (GstBufferPool * pool);
static gboolean gst_win32_ipc_buffer_pool_stop (GstBufferPool * pool);

static void
gst_win32_ipc_buffer_pool_class_init (GstWin32IpcBufferPoolClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstBufferPoolClass *bufferpool_class = GST_BUFFER_POOL_CLASS (klass);

  gobject_class->dispose = gst_win32_ipc_buffer_pool_dispose;

  bufferpool_class->get_options = gst_win32_ipc_buffer_pool_get_options;
  bufferpool_class->set_config = gst_win32_ipc_buffer_pool_set_config;
  bufferpool_class->alloc_buffer = gst_win32_ipc_buffer_pool_alloc_buffer;
  bufferpool_class->start = gst_win32_ipc_buffer_pool_start;
  bufferpool_class->stop = gst_win32_ipc_buffer_pool_stop;

  GST_DEBUG_CATEGORY_INIT (gst_win32_ipc_buffer_pool_debug,
      "win32_ipcbufferpool", 0, "win32_ipcbufferpool object");
}

static void
gst_win32_ipc_buffer_pool_init (GstWin32IpcBufferPool * self)
{
}

static void
gst_win32_ipc_buffer_pool_dispose (GObject * object)
{
  GstWin32IpcBufferPool *self = GST_WIN32_IPC_BUFFER_POOL (object);

  if (self->alloc) {
    gst_win32_ipc_allocator_set_active (self->alloc, FALSE);
    gst_clear_object (&self->alloc);
  }

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static const gchar **
gst_win32_ipc_buffer_pool_get_options (GstBufferPool * pool)
{
  static const gchar *options[] = { GST_BUFFER_POOL_OPTION_VIDEO_META,
    nullptr
  };

  return options;
}

static gboolean
gst_win32_ipc_buffer_pool_set_config (GstBufferPool * pool,
    GstStructure * config)
{
  GstWin32IpcBufferPool *self = GST_WIN32_IPC_BUFFER_POOL (pool);
  GstVideoInfo info;
  GstCaps *caps = nullptr;
  gboolean ret = TRUE;
  guint size, min_buffers, max_buffers;

  if (!gst_buffer_pool_config_get_params (config, &caps, &size, &min_buffers,
          &max_buffers)) {
    GST_WARNING_OBJECT (pool, "Invalid config");
    return FALSE;
  }

  if (!caps) {
    GST_WARNING_OBJECT (pool, "No caps");
    return FALSE;
  }

  /* now parse the caps from the config */
  if (!gst_video_info_from_caps (&info, caps)) {
    GST_WARNING_OBJECT (self, "Couldn't get video info from caps");
    return FALSE;
  }

  if (size < info.size) {
    GST_WARNING_OBJECT (self, "Size is smaller for the caps");
    return FALSE;
  }

  info.size = MAX (size, info.size);
  self->info = info;

  GST_LOG_OBJECT (pool, "%dx%d, caps %" GST_PTR_FORMAT, info.width, info.height,
      caps);

  if (self->alloc) {
    gst_win32_ipc_allocator_set_active (self->alloc, FALSE);
    gst_clear_object (&self->alloc);
  }

  self->alloc = gst_win32_ipc_allocator_new (size);
  if (!self->alloc) {
    GST_ERROR_OBJECT (self, "Couldn't create allocator");
    return FALSE;
  }

  self->add_videometa = gst_buffer_pool_config_has_option (config,
      GST_BUFFER_POOL_OPTION_VIDEO_META);

  gst_buffer_pool_config_set_params (config,
      caps, info.size, min_buffers, max_buffers);

  return GST_BUFFER_POOL_CLASS (parent_class)->set_config (pool, config) && ret;
}

static GstFlowReturn
gst_win32_ipc_buffer_pool_alloc_buffer (GstBufferPool * pool,
    GstBuffer ** buffer, GstBufferPoolAcquireParams * params)
{
  GstWin32IpcBufferPool *self = GST_WIN32_IPC_BUFFER_POOL (pool);
  GstFlowReturn ret = GST_FLOW_OK;
  GstBuffer *buf;
  GstMemory *mem = nullptr;
  GstVideoInfo *info = &self->info;

  ret = gst_win32_ipc_allocator_acquire_memory (self->alloc, &mem);
  if (ret != GST_FLOW_OK) {
    GST_WARNING_OBJECT (self, "Couldn't acquire memory");
    return ret;
  }

  buf = gst_buffer_new ();
  gst_buffer_append_memory (buf, mem);

  if (self->add_videometa) {
    gst_buffer_add_video_meta_full (buf, GST_VIDEO_FRAME_FLAG_NONE,
        GST_VIDEO_INFO_FORMAT (info), GST_VIDEO_INFO_WIDTH (info),
        GST_VIDEO_INFO_HEIGHT (info), GST_VIDEO_INFO_N_PLANES (info),
        info->offset, info->stride);
  }

  *buffer = buf;

  return GST_FLOW_OK;
}

static gboolean
gst_win32_ipc_buffer_pool_start (GstBufferPool * pool)
{
  GstWin32IpcBufferPool *self = GST_WIN32_IPC_BUFFER_POOL (pool);
  gboolean ret;

  GST_DEBUG_OBJECT (self, "Start");

  if (!self->alloc) {
    GST_ERROR_OBJECT (self, "No allocator");
    return FALSE;
  }

  if (!gst_win32_ipc_allocator_set_active (self->alloc, TRUE)) {
    GST_ERROR_OBJECT (self, "Failed to activate allocator");
    return FALSE;
  }

  ret = GST_BUFFER_POOL_CLASS (parent_class)->start (pool);
  if (!ret) {
    GST_ERROR_OBJECT (self, "Failed to start");
    gst_win32_ipc_allocator_set_active (self->alloc, FALSE);

    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_win32_ipc_buffer_pool_stop (GstBufferPool * pool)
{
  GstWin32IpcBufferPool *self = GST_WIN32_IPC_BUFFER_POOL (pool);

  GST_DEBUG_OBJECT (self, "Stop");

  if (self->alloc)
    gst_win32_ipc_allocator_set_active (self->alloc, FALSE);

  return GST_BUFFER_POOL_CLASS (parent_class)->stop (pool);
}

GstBufferPool *
gst_win32_ipc_buffer_pool_new (void)
{
  GstWin32IpcBufferPool *self;

  self = (GstWin32IpcBufferPool *)
      g_object_new (GST_TYPE_WIN32_IPC_BUFFER_POOL, nullptr);
  gst_object_ref_sink (self);

  return GST_BUFFER_POOL_CAST (self);
}
