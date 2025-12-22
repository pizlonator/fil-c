/* GStreamer
 * Copyright (C) 2021 Seungha Yang <seungha@centricular.com>
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

#include "gstqsvallocator_va.h"

GST_DEBUG_CATEGORY_EXTERN (gst_qsv_allocator_debug);
#define GST_CAT_DEFAULT gst_qsv_allocator_debug

struct _GstQsvVaAllocator
{
  GstQsvAllocator parent;

  GstVaDisplay *display;
};

#define gst_qsv_va_allocator_parent_class parent_class
G_DEFINE_TYPE (GstQsvVaAllocator, gst_qsv_va_allocator, GST_TYPE_QSV_ALLOCATOR);

static void gst_qsv_va_allocator_dispose (GObject * object);
static mfxStatus gst_qsv_va_allocator_alloc (GstQsvAllocator * allocator,
    gboolean dummy_alloc, mfxFrameAllocRequest * request,
    mfxFrameAllocResponse * response);
static GstBuffer *gst_qsv_va_allocator_upload (GstQsvAllocator * allocator,
    const GstVideoInfo * info, GstBuffer * buffer, GstBufferPool * pool);
static GstBuffer *gst_qsv_va_allocator_download (GstQsvAllocator * allocator,
    const GstVideoInfo * info, gboolean force_copy, GstQsvFrame * frame,
    GstBufferPool * pool);

static void
gst_qsv_va_allocator_class_init (GstQsvVaAllocatorClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GstQsvAllocatorClass *alloc_class = GST_QSV_ALLOCATOR_CLASS (klass);

  object_class->dispose = gst_qsv_va_allocator_dispose;

  alloc_class->alloc = GST_DEBUG_FUNCPTR (gst_qsv_va_allocator_alloc);
  alloc_class->upload = GST_DEBUG_FUNCPTR (gst_qsv_va_allocator_upload);
  alloc_class->download = GST_DEBUG_FUNCPTR (gst_qsv_va_allocator_download);
}

static void
gst_qsv_va_allocator_init (GstQsvVaAllocator * self)
{
}

static void
gst_qsv_va_allocator_dispose (GObject * object)
{
  GstQsvVaAllocator *self = GST_QSV_VA_ALLOCATOR (object);

  gst_clear_object (&self->display);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static mfxStatus
gst_qsv_va_allocator_alloc (GstQsvAllocator * allocator, gboolean dummy_alloc,
    mfxFrameAllocRequest * request, mfxFrameAllocResponse * response)
{
  GST_ERROR_OBJECT (allocator, "Not implemented");

  return MFX_ERR_UNSUPPORTED;
}

static GstBuffer *
gst_qsv_va_allocator_upload (GstQsvAllocator * allocator,
    const GstVideoInfo * info, GstBuffer * buffer, GstBufferPool * pool)
{
  GstQsvVaAllocator *self = GST_QSV_VA_ALLOCATOR (allocator);
  GstVideoFrame src_frame, dst_frame;
  VASurfaceID surface;
  GstBuffer *dst_buf;
  GstFlowReturn ret;

  surface = gst_va_buffer_get_surface (buffer);
  if (surface != VA_INVALID_ID && gst_va_buffer_peek_display (buffer) ==
      self->display) {
    return gst_buffer_ref (buffer);
  }

  ret = gst_buffer_pool_acquire_buffer (pool, &dst_buf, nullptr);
  if (ret != GST_FLOW_OK) {
    GST_WARNING_OBJECT (allocator, "Failed to acquire buffer");
    return nullptr;
  }

  if (!gst_video_frame_map (&src_frame, info, buffer, GST_MAP_READ)) {
    GST_WARNING_OBJECT (allocator, "Failed to map src frame");
    gst_buffer_unref (dst_buf);
    return nullptr;
  }

  if (!gst_video_frame_map (&dst_frame, info, dst_buf, GST_MAP_WRITE)) {
    GST_WARNING_OBJECT (allocator, "Failed to map src frame");
    gst_video_frame_unmap (&src_frame);
    gst_buffer_unref (dst_buf);
    return nullptr;
  }

  for (guint i = 0; i < GST_VIDEO_FRAME_N_PLANES (&src_frame); i++) {
    guint src_width_in_bytes, src_height;
    guint dst_width_in_bytes, dst_height;
    guint width_in_bytes, height;
    guint src_stride, dst_stride;
    guint8 *src_data, *dst_data;

    src_width_in_bytes = GST_VIDEO_FRAME_COMP_WIDTH (&src_frame, i) *
        GST_VIDEO_FRAME_COMP_PSTRIDE (&src_frame, i);
    src_height = GST_VIDEO_FRAME_COMP_HEIGHT (&src_frame, i);
    src_stride = GST_VIDEO_FRAME_COMP_STRIDE (&src_frame, i);

    dst_width_in_bytes = GST_VIDEO_FRAME_COMP_WIDTH (&dst_frame, i) *
        GST_VIDEO_FRAME_COMP_PSTRIDE (&src_frame, i);
    dst_height = GST_VIDEO_FRAME_COMP_HEIGHT (&src_frame, i);
    dst_stride = GST_VIDEO_FRAME_COMP_STRIDE (&dst_frame, i);

    width_in_bytes = MIN (src_width_in_bytes, dst_width_in_bytes);
    height = MIN (src_height, dst_height);

    src_data = (guint8 *) GST_VIDEO_FRAME_PLANE_DATA (&src_frame, i);
    dst_data = (guint8 *) GST_VIDEO_FRAME_PLANE_DATA (&dst_frame, i);

    for (guint j = 0; j < height; j++) {
      memcpy (dst_data, src_data, width_in_bytes);
      dst_data += dst_stride;
      src_data += src_stride;
    }
  }

  gst_video_frame_unmap (&dst_frame);
  gst_video_frame_unmap (&src_frame);

  return dst_buf;
}

static GstBuffer *
gst_qsv_va_allocator_download (GstQsvAllocator * allocator,
    const GstVideoInfo * info, gboolean force_copy, GstQsvFrame * frame,
    GstBufferPool * pool)
{
  GST_ERROR_OBJECT (allocator, "Not implemented");

  return nullptr;
}

GstQsvAllocator *
gst_qsv_va_allocator_new (GstVaDisplay * display)
{
  GstQsvVaAllocator *self;

  g_return_val_if_fail (GST_IS_VA_DISPLAY (display), nullptr);

  self = (GstQsvVaAllocator *)
      g_object_new (GST_TYPE_QSV_VA_ALLOCATOR, nullptr);
  self->display = (GstVaDisplay *) gst_object_ref (display);

  gst_object_ref_sink (self);

  return GST_QSV_ALLOCATOR (self);
}
