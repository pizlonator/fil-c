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

#include "gstqsvallocator.h"

GST_DEBUG_CATEGORY_EXTERN (gst_qsv_allocator_debug);
#define GST_CAT_DEFAULT gst_qsv_allocator_debug

/* Both d3d11 and va use (GST_MAP_FLAG_LAST << 1) value
 * for GPU access */
#define GST_MAP_QSV (GST_MAP_FLAG_LAST << 1)

struct _GstQsvFrame
{
  GstMiniObject parent;

  GstQsvAllocator *allocator;

  GMutex lock;

  guint map_count;
  GstBuffer *buffer;
  GstVideoInfo info;
  GstVideoFrame frame;
  GstQsvMemoryType mem_type;
  GstMapFlags map_flags;
};

GST_DEFINE_MINI_OBJECT_TYPE (GstQsvFrame, gst_qsv_frame);

static void
_gst_qsv_frame_free (GstQsvFrame * frame)
{
  g_mutex_clear (&frame->lock);
  gst_clear_buffer (&frame->buffer);
  gst_clear_object (&frame->allocator);
  g_free (frame);
}

static GstQsvFrame *
gst_qsv_frame_new (void)
{
  GstQsvFrame *self;

  self = g_new0 (GstQsvFrame, 1);
  g_mutex_init (&self->lock);

  gst_mini_object_init (GST_MINI_OBJECT_CAST (self), 0,
      GST_TYPE_QSV_FRAME, nullptr, nullptr,
      (GstMiniObjectFreeFunction) _gst_qsv_frame_free);

  return self;
}

GstBuffer *
gst_qsv_frame_peek_buffer (GstQsvFrame * frame)
{
  g_return_val_if_fail (GST_IS_QSV_FRAME (frame), nullptr);

  return frame->buffer;
}

gboolean
gst_qsv_frame_set_buffer (GstQsvFrame * frame, GstBuffer * buffer)
{
  g_return_val_if_fail (GST_IS_QSV_FRAME (frame), FALSE);

  g_mutex_lock (&frame->lock);
  if (frame->buffer == buffer) {
    g_mutex_unlock (&frame->lock);
    return TRUE;
  }

  if (frame->map_count > 0) {
    GST_ERROR ("frame is locked");
    g_mutex_unlock (&frame->lock);

    return FALSE;
  }

  gst_clear_buffer (&frame->buffer);
  frame->buffer = buffer;
  g_mutex_unlock (&frame->lock);

  return TRUE;
}

struct _GstQsvAllocatorPrivate
{
  GstAtomicQueue *queue;

  mfxFrameAllocator allocator;
  mfxFrameAllocResponse response;
  guint16 extra_alloc_size;
  gboolean dummy_alloc;
};

#define gst_qsv_allocator_parent_class parent_class
G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GstQsvAllocator,
    gst_qsv_allocator, GST_TYPE_OBJECT);

static void gst_qsv_allocator_finalize (GObject * object);
static mfxStatus gst_qsv_allocator_alloc (mfxHDL pthis,
    mfxFrameAllocRequest * request, mfxFrameAllocResponse * response);
static mfxStatus gst_qsv_allocator_lock (mfxHDL pthis, mfxMemId mid,
    mfxFrameData * ptr);
static mfxStatus gst_qsv_allocator_unlock (mfxHDL pthis, mfxMemId mid,
    mfxFrameData * ptr);
static mfxStatus gst_qsv_allocator_get_hdl (mfxHDL pthis, mfxMemId mid,
    mfxHDL * handle);
static mfxStatus gst_qsv_allocator_free (mfxHDL pthis,
    mfxFrameAllocResponse * response);
static GstBuffer *gst_qsv_allocator_download_default (GstQsvAllocator * self,
    const GstVideoInfo * info, gboolean force_copy, GstQsvFrame * frame,
    GstBufferPool * pool);

static void
gst_qsv_allocator_class_init (GstQsvAllocatorClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gst_qsv_allocator_finalize;

  klass->download = GST_DEBUG_FUNCPTR (gst_qsv_allocator_download_default);
}

static void
gst_qsv_allocator_init (GstQsvAllocator * self)
{
  GstQsvAllocatorPrivate *priv;

  self->is_gbr = FALSE;

  priv = self->priv = (GstQsvAllocatorPrivate *)
      gst_qsv_allocator_get_instance_private (self);

  priv->queue = gst_atomic_queue_new (16);

  priv->allocator.pthis = self;
  priv->allocator.Alloc = gst_qsv_allocator_alloc;
  priv->allocator.Lock = gst_qsv_allocator_lock;
  priv->allocator.Unlock = gst_qsv_allocator_unlock;
  priv->allocator.GetHDL = gst_qsv_allocator_get_hdl;
  priv->allocator.Free = gst_qsv_allocator_free;
}

static void
gst_qsv_allocator_finalize (GObject * object)
{
  GstQsvAllocator *self = GST_QSV_ALLOCATOR (object);
  GstQsvAllocatorPrivate *priv = self->priv;
  GstQsvFrame *frame;

  GST_DEBUG_OBJECT (object, "finalize");

  while ((frame = (GstQsvFrame *) gst_atomic_queue_pop (priv->queue)))
    gst_qsv_frame_unref (frame);

  gst_atomic_queue_unref (priv->queue);
  gst_qsv_allocator_free ((mfxHDL) self, &priv->response);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static mfxStatus
gst_qsv_allocator_alloc_default (GstQsvAllocator * self, gboolean dummy_alloc,
    mfxFrameAllocRequest * request, mfxFrameAllocResponse * response)
{
  GstQsvFrame **mids = nullptr;
  GstVideoInfo info;
  GstVideoAlignment align;
  GstVideoFormat format = GST_VIDEO_FORMAT_UNKNOWN;
  GstBufferPool *pool;
  GstCaps *caps;
  GstStructure *config;

  /* Something unexpected and went wrong */
  if ((request->Type & MFX_MEMTYPE_SYSTEM_MEMORY) == 0) {
    GST_ERROR_OBJECT (self,
        "MFX is requesting system memory, type 0x%x", request->Type);
    return MFX_ERR_UNSUPPORTED;
  }

  format = gst_qsv_frame_info_format_to_gst (&request->Info, self->is_gbr);
  if (format == GST_VIDEO_FORMAT_UNKNOWN) {
    GST_ERROR_OBJECT (self, "Unknown MFX format fourcc %" GST_FOURCC_FORMAT,
        GST_FOURCC_ARGS (request->Info.FourCC));

    return MFX_ERR_UNSUPPORTED;
  }

  mids = g_new0 (GstQsvFrame *, request->NumFrameSuggested);
  response->NumFrameActual = request->NumFrameSuggested;

  gst_video_info_set_format (&info,
      format, request->Info.CropW, request->Info.CropH);

  if (dummy_alloc) {
    for (guint i = 0; i < request->NumFrameSuggested; i++) {
      mids[i] = gst_qsv_allocator_acquire_frame (self,
          GST_QSV_SYSTEM_MEMORY, &info, nullptr, nullptr);
    }

    response->mids = (mfxMemId *) mids;

    return MFX_ERR_NONE;
  }

  caps = gst_video_info_to_caps (&info);
  if (!caps) {
    GST_ERROR_OBJECT (self, "Failed to convert video-info to caps");
    return MFX_ERR_UNSUPPORTED;
  }

  gst_video_alignment_reset (&align);
  align.padding_right = request->Info.Width - request->Info.CropW;
  align.padding_bottom = request->Info.Height - request->Info.CropH;

  pool = gst_video_buffer_pool_new ();
  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_add_option (config, GST_BUFFER_POOL_OPTION_VIDEO_META);
  gst_buffer_pool_config_add_option (config,
      GST_BUFFER_POOL_OPTION_VIDEO_ALIGNMENT);
  gst_buffer_pool_config_set_video_alignment (config, &align);
  gst_buffer_pool_config_set_params (config, caps, GST_VIDEO_INFO_SIZE (&info),
      0, 0);
  gst_caps_unref (caps);
  gst_buffer_pool_set_config (pool, config);
  gst_buffer_pool_set_active (pool, TRUE);

  for (guint i = 0; i < request->NumFrameSuggested; i++) {
    GstBuffer *buffer;

    if (gst_buffer_pool_acquire_buffer (pool, &buffer, nullptr) != GST_FLOW_OK) {
      GST_ERROR_OBJECT (self, "Failed to allocate texture buffer");
      gst_buffer_pool_set_active (pool, FALSE);
      gst_object_unref (pool);
      goto error;
    }

    mids[i] = gst_qsv_allocator_acquire_frame (self,
        GST_QSV_SYSTEM_MEMORY, &info, buffer, nullptr);
  }

  gst_buffer_pool_set_active (pool, FALSE);
  gst_object_unref (pool);

  response->mids = (mfxMemId *) mids;

  return MFX_ERR_NONE;

error:
  if (mids) {
    for (guint i = 0; i < response->NumFrameActual; i++)
      gst_clear_qsv_frame (&mids[i]);

    g_free (mids);
  }

  response->NumFrameActual = 0;

  return MFX_ERR_MEMORY_ALLOC;
}

static gboolean
gst_qsv_allocator_copy_cached_response (GstQsvAllocator * self,
    mfxFrameAllocResponse * dst, mfxFrameAllocResponse * src)
{
  GstQsvFrame **mids;

  if (src->NumFrameActual == 0)
    return FALSE;

  mids = g_new0 (GstQsvFrame *, src->NumFrameActual);

  for (guint i = 0; i < src->NumFrameActual; i++) {
    GstQsvFrame *frame = (GstQsvFrame *) src->mids[i];

    mids[i] = gst_qsv_frame_ref (frame);
  }

  dst->NumFrameActual = src->NumFrameActual;
  dst->mids = (mfxMemId *) mids;

  return TRUE;
}

static mfxStatus
gst_qsv_allocator_alloc (mfxHDL pthis,
    mfxFrameAllocRequest * request, mfxFrameAllocResponse * response)
{
  GstQsvAllocator *self = GST_QSV_ALLOCATOR (pthis);
  GstQsvAllocatorPrivate *priv = self->priv;
  GstQsvAllocatorClass *klass;
  mfxStatus status;
  mfxFrameAllocRequest req = *request;
  gboolean dummy_alloc = priv->dummy_alloc;

  GST_INFO_OBJECT (self, "Alloc, Request Type: 0x%x, %dx%d (%dx%d)",
      req.Type, req.Info.Width, req.Info.Height,
      req.Info.CropW, req.Info.CropH);

  /* Apply extra_alloc_size only for GST internal use case */
  if ((request->Type & MFX_MEMTYPE_EXTERNAL_FRAME) != 0)
    req.NumFrameSuggested += priv->extra_alloc_size;

  if (req.Info.CropW == 0 || req.Info.CropH == 0) {
    req.Info.CropW = req.Info.Width;
    req.Info.CropH = req.Info.Height;
  }

  if (request->Info.FourCC == MFX_FOURCC_P8 ||
      (request->Type & MFX_MEMTYPE_EXTERNAL_FRAME) == 0) {
    dummy_alloc = FALSE;
  }

  GST_INFO_OBJECT (self, "Dummy alloc %d", dummy_alloc);

  if ((request->Type & MFX_MEMTYPE_SYSTEM_MEMORY) != 0) {
    status = gst_qsv_allocator_alloc_default (self,
        dummy_alloc, &req, response);
  } else {
    klass = GST_QSV_ALLOCATOR_GET_CLASS (self);
    g_assert (klass->alloc);

    status = klass->alloc (self, dummy_alloc, &req, response);
  }

  if (status != MFX_ERR_NONE)
    return status;

  /* Cache this respons so that this can be accessible from GST side */
  if (dummy_alloc) {
    gst_qsv_allocator_free ((mfxHDL) self, &priv->response);
    gst_qsv_allocator_copy_cached_response (self, &priv->response, response);
  }

  return MFX_ERR_NONE;
}

static mfxStatus
gst_qsv_allocator_lock (mfxHDL pthis, mfxMemId mid, mfxFrameData * ptr)
{
  GstQsvAllocator *self = GST_QSV_ALLOCATOR (pthis);
  GstQsvFrame *frame = (GstQsvFrame *) mid;
  guint stride;

  GST_TRACE_OBJECT (self, "Lock mfxMemId %p", mid);

  g_mutex_lock (&frame->lock);
  if (!frame->buffer) {
    GST_ERROR_OBJECT (self, "MemId %p doesn't hold buffer", mid);
    g_mutex_unlock (&frame->lock);
    return MFX_ERR_LOCK_MEMORY;
  }

  if (frame->map_count == 0) {
    guint map_flags = (guint) frame->map_flags;
    map_flags &= ~((guint) GST_MAP_QSV);

    gst_video_frame_map (&frame->frame, &frame->info, frame->buffer,
        (GstMapFlags) map_flags);
  }

  frame->map_count++;
  stride = GST_VIDEO_FRAME_PLANE_STRIDE (&frame->frame, 0);
  ptr->PitchHigh = (mfxU16) (stride / (1 << 16));
  ptr->PitchLow = (mfxU16) (stride % (1 << 16));

  /* FIXME: check and handle other formats */
  switch (GST_VIDEO_INFO_FORMAT (&frame->info)) {
    case GST_VIDEO_FORMAT_NV12:
    case GST_VIDEO_FORMAT_P010_10LE:
    case GST_VIDEO_FORMAT_P012_LE:
    case GST_VIDEO_FORMAT_P016_LE:
      ptr->Y = (mfxU8 *) GST_VIDEO_FRAME_PLANE_DATA (&frame->frame, 0);
      ptr->UV = (mfxU8 *) GST_VIDEO_FRAME_PLANE_DATA (&frame->frame, 1);
      break;
    case GST_VIDEO_FORMAT_YUY2:
      ptr->Y = (mfxU8 *) GST_VIDEO_FRAME_PLANE_DATA (&frame->frame, 0);
      ptr->U = ptr->Y + 1;
      ptr->V = ptr->Y + 3;
      break;
    case GST_VIDEO_FORMAT_Y210:
    case GST_VIDEO_FORMAT_Y212_LE:
      ptr->Y16 = (mfxU16 *) GST_VIDEO_FRAME_PLANE_DATA (&frame->frame, 0);
      ptr->U16 = ptr->Y16 + 1;
      ptr->V16 = ptr->Y16 + 3;
      break;
    case GST_VIDEO_FORMAT_VUYA:
    case GST_VIDEO_FORMAT_RBGA:
      ptr->V = (mfxU8 *) GST_VIDEO_FRAME_PLANE_DATA (&frame->frame, 0);
      ptr->U = ptr->V + 1;
      ptr->Y = ptr->V + 2;
      ptr->A = ptr->V + 3;
      break;
    case GST_VIDEO_FORMAT_Y410:
    case GST_VIDEO_FORMAT_BGR10A2_LE:
      ptr->Y410 = (mfxY410 *) GST_VIDEO_FRAME_PLANE_DATA (&frame->frame, 0);
      break;
    case GST_VIDEO_FORMAT_Y412_LE:
    case GST_VIDEO_FORMAT_BGRA64_LE:
      ptr->U = (mfxU8 *) GST_VIDEO_FRAME_PLANE_DATA (&frame->frame, 0);
      ptr->Y = ptr->Y + 2;
      ptr->V = ptr->Y + 4;
      ptr->A = ptr->Y + 6;
      break;
    case GST_VIDEO_FORMAT_BGRA:
      ptr->B = (mfxU8 *) GST_VIDEO_FRAME_PLANE_DATA (&frame->frame, 0);
      ptr->G = ptr->B + 1;
      ptr->R = ptr->B + 2;
      ptr->A = ptr->B + 3;
      break;
    case GST_VIDEO_FORMAT_RGBA:
      ptr->R = (mfxU8 *) GST_VIDEO_FRAME_PLANE_DATA (&frame->frame, 0);
      ptr->G = ptr->R + 1;
      ptr->B = ptr->R + 2;
      ptr->A = ptr->R + 3;
      break;
    default:
      break;
  }

  g_mutex_unlock (&frame->lock);

  return MFX_ERR_NONE;
}

static mfxStatus
gst_qsv_allocator_unlock (mfxHDL pthis, mfxMemId mid, mfxFrameData * ptr)
{
  GstQsvAllocator *self = GST_QSV_ALLOCATOR (pthis);
  GstQsvFrame *frame = (GstQsvFrame *) mid;

  GST_TRACE_OBJECT (self, "Unlock mfxMemId %p", mid);

  g_mutex_lock (&frame->lock);

  if (frame->map_count > 0) {
    frame->map_count--;

    if (frame->map_count == 0)
      gst_video_frame_unmap (&frame->frame);
  } else {
    GST_WARNING_OBJECT (self, "Unlock request for non-locked memory");
  }

  g_mutex_unlock (&frame->lock);

  /* SDK will not re-lock unless we clear data pointer here. It happens
   * on Linux with BGRA JPEG encoding */
  ptr->R = nullptr;
  ptr->G = nullptr;
  ptr->B = nullptr;
  ptr->A = nullptr;

  return MFX_ERR_NONE;
}

static mfxStatus
gst_qsv_allocator_get_hdl (mfxHDL pthis, mfxMemId mid, mfxHDL * handle)
{
  GstQsvAllocator *self = GST_QSV_ALLOCATOR (pthis);
  GstQsvFrame *frame = GST_QSV_FRAME_CAST (mid);
  GstMapInfo map_info;

  if (!GST_QSV_MEM_TYPE_IS_VIDEO (frame->mem_type))
    return MFX_ERR_UNSUPPORTED;

  g_mutex_lock (&frame->lock);
  if (!frame->buffer) {
    GST_ERROR_OBJECT (self, "MemId %p doesn't hold buffer", mid);
    g_mutex_unlock (&frame->lock);

    return MFX_ERR_UNSUPPORTED;
  }

  g_assert ((frame->map_flags & GST_MAP_QSV) != 0);
  if (!gst_buffer_map (frame->buffer, &map_info, frame->map_flags)) {
    GST_ERROR_OBJECT (self, "Failed to map buffer");
    g_mutex_unlock (&frame->lock);

    return MFX_ERR_UNSUPPORTED;
  }

  GST_TRACE_OBJECT (self, "Get handle for mfxMemId %p", mid);

#ifdef G_OS_WIN32
  mfxHDLPair *pair = (mfxHDLPair *) handle;
  pair->first = (mfxHDL) map_info.data;

  /* GstD3D11 will fill user_data[0] with subresource index */
  pair->second = (mfxHDL) map_info.user_data[0];
#else
  *handle = (mfxHDL) map_info.data;
#endif

  /* XXX: Ideally we should unmap only when this surface is unlocked... */
  gst_buffer_unmap (frame->buffer, &map_info);
  g_mutex_unlock (&frame->lock);

  return MFX_ERR_NONE;
}

static mfxStatus
gst_qsv_allocator_free (mfxHDL pthis, mfxFrameAllocResponse * response)
{
  GstQsvFrame **frames = (GstQsvFrame **) response->mids;

  for (guint i = 0; i < response->NumFrameActual; i++)
    gst_clear_qsv_frame (&frames[i]);

  g_clear_pointer (&response->mids, g_free);
  response->NumFrameActual = 0;

  return MFX_ERR_NONE;
}

static void
gst_qsv_frame_release (GstQsvFrame * frame)
{
  GstQsvAllocator *allocator = frame->allocator;

  g_mutex_lock (&frame->lock);
  if (frame->map_count > 0) {
    GST_WARNING_OBJECT (allocator, "Releasing mapped frame %p", frame);
    gst_video_frame_unmap (&frame->frame);
  }
  frame->map_count = 0;
  gst_clear_buffer (&frame->buffer);
  g_mutex_unlock (&frame->lock);

  GST_MINI_OBJECT_CAST (frame)->dispose = nullptr;
  frame->allocator = nullptr;

  GST_TRACE_OBJECT (allocator, "Moving frame %p back to pool", frame);

  gst_atomic_queue_push (allocator->priv->queue, frame);
  gst_object_unref (allocator);
}

static gboolean
gst_qsv_frame_dispose (GstQsvFrame * frame)
{
  g_assert (frame->allocator);

  gst_qsv_frame_ref (frame);
  gst_qsv_frame_release (frame);

  return FALSE;
}

static GstBuffer *
gst_qsv_allocator_upload_default (GstQsvAllocator * allocator,
    const GstVideoInfo * info, GstBuffer * buffer, GstBufferPool * pool)
{
  GstBuffer *dst_buf;
  GstFlowReturn flow_ret;
  GstVideoFrame src_frame, dst_frame;

  flow_ret = gst_buffer_pool_acquire_buffer (pool, &dst_buf, nullptr);
  if (flow_ret != GST_FLOW_OK) {
    GST_WARNING ("Failed to acquire buffer from pool, return %s",
        gst_flow_get_name (flow_ret));
    return nullptr;
  }

  gst_video_frame_map (&src_frame, info, buffer, GST_MAP_READ);
  gst_video_frame_map (&dst_frame, info, dst_buf, GST_MAP_WRITE);

  if (GST_VIDEO_FRAME_WIDTH (&src_frame) == GST_VIDEO_FRAME_WIDTH (&dst_frame)
      && GST_VIDEO_FRAME_HEIGHT (&src_frame) ==
      GST_VIDEO_FRAME_HEIGHT (&dst_frame)) {
    gst_video_frame_unmap (&src_frame);
    gst_video_frame_unmap (&dst_frame);

    gst_buffer_unref (dst_buf);
    return gst_buffer_ref (buffer);
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
        GST_VIDEO_FRAME_COMP_PSTRIDE (&dst_frame, i);
    dst_height = GST_VIDEO_FRAME_COMP_HEIGHT (&dst_frame, i);
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

/**
 * gst_qsv_allocator_acquire_frame:
 * @allocator: a #GstQsvAllocator
 * @mem_type: a memory type
 * @info: a #GstVideoInfo
 * @buffer: (nullable) (transfer full): a #GstBuffer
 * @pool: (nullable): a #GstBufferPool
 *
 * Uploads @buffer to video memory if required, and wraps GstBuffer using
 * #GstQsvFrame object so that QSV API can access native memory handle
 * via mfxFrameAllocator interface.
 *
 * Returns: a #GstQsvFrame object
 */
GstQsvFrame *
gst_qsv_allocator_acquire_frame (GstQsvAllocator * allocator,
    GstQsvMemoryType mem_type, const GstVideoInfo * info, GstBuffer * buffer,
    GstBufferPool * pool)
{
  GstQsvAllocatorPrivate *priv;
  GstQsvFrame *frame;
  guint32 map_flags = 0;

  g_return_val_if_fail (GST_IS_QSV_ALLOCATOR (allocator), nullptr);

  if (GST_QSV_MEM_TYPE_IS_SYSTEM (mem_type) &&
      GST_QSV_MEM_TYPE_IS_VIDEO (mem_type)) {
    GST_ERROR_OBJECT (allocator, "Invalid memory type");
    return nullptr;
  }

  if (GST_QSV_MEM_TYPE_IS_VIDEO (mem_type)) {
    map_flags = GST_MAP_QSV;

    if ((mem_type & GST_QSV_ENCODER_IN_MEMORY) != 0) {
      map_flags |= GST_MAP_READ;
    } else if ((mem_type & GST_QSV_DECODER_OUT_MEMORY) != 0 ||
        (mem_type & GST_QSV_PROCESS_TARGET) != 0) {
      map_flags |= GST_MAP_WRITE;
    } else {
      GST_ERROR_OBJECT (allocator,
          "Unknown read/write access for video memory");
      return nullptr;
    }
  } else if ((mem_type & GST_QSV_ENCODER_IN_MEMORY) != 0) {
    map_flags = GST_MAP_READ;
  } else {
    map_flags = GST_MAP_READWRITE;
  }

  priv = allocator->priv;
  frame = (GstQsvFrame *) gst_atomic_queue_pop (priv->queue);

  if (!frame)
    frame = gst_qsv_frame_new ();

  frame->mem_type = mem_type;
  frame->map_flags = (GstMapFlags) map_flags;
  frame->info = *info;

  if (!pool) {
    frame->buffer = buffer;
  } else if (buffer) {
    GstBuffer *upload_buf;

    frame->allocator = (GstQsvAllocator *) gst_object_ref (allocator);
    GST_MINI_OBJECT_CAST (frame)->dispose =
        (GstMiniObjectDisposeFunction) gst_qsv_frame_dispose;

    if (GST_QSV_MEM_TYPE_IS_SYSTEM (mem_type)) {
      upload_buf = gst_qsv_allocator_upload_default (allocator, info, buffer,
          pool);
    } else {
      GstQsvAllocatorClass *klass;

      klass = GST_QSV_ALLOCATOR_GET_CLASS (allocator);
      g_assert (klass->upload);

      upload_buf = klass->upload (allocator, info, buffer, pool);
    }

    gst_buffer_unref (buffer);

    if (!upload_buf) {
      GST_WARNING_OBJECT (allocator, "Failed to upload buffer");
      gst_qsv_frame_unref (frame);

      return nullptr;
    }

    frame->buffer = upload_buf;
  }

  return frame;
}

static GstBuffer *
gst_qsv_allocator_download_default (GstQsvAllocator * self,
    const GstVideoInfo * info, gboolean force_copy, GstQsvFrame * frame,
    GstBufferPool * pool)
{
  GstBuffer *buffer = nullptr;
  GstFlowReturn ret;
  GstVideoFrame dst_frame;
  mfxStatus status;
  mfxFrameData dummy;
  gboolean copy_ret;

  GST_TRACE_OBJECT (self, "Download");

  if (!force_copy)
    return gst_buffer_ref (frame->buffer);

  ret = gst_buffer_pool_acquire_buffer (pool, &buffer, nullptr);
  if (ret != GST_FLOW_OK) {
    GST_WARNING_OBJECT (self, "Failed to acquire buffer");
    return nullptr;
  }

  /* Use gst_qsv_allocator_lock() instead of gst_video_frame_map() to avoid
   * redundant map if it's already locked by driver, already locked by driver
   * sounds unsafe situaltion though */
  status = gst_qsv_allocator_lock ((mfxHDL) self, (mfxMemId) frame, &dummy);
  if (status != MFX_ERR_NONE) {
    gst_buffer_unref (buffer);
    GST_ERROR_OBJECT (self, "Failed to lock frame");
    return nullptr;
  }

  if (!gst_video_frame_map (&dst_frame, info, buffer, GST_MAP_WRITE)) {
    gst_qsv_allocator_unlock ((mfxHDL) self, (mfxMemId) frame, &dummy);
    gst_buffer_unref (buffer);
    GST_ERROR_OBJECT (self, "Failed to map output buffer");
    return nullptr;
  }

  copy_ret = gst_video_frame_copy (&dst_frame, &frame->frame);
  gst_qsv_allocator_unlock ((mfxHDL) self, (mfxMemId) frame, &dummy);
  gst_video_frame_unmap (&dst_frame);

  if (!copy_ret) {
    GST_ERROR_OBJECT (self, "Failed to copy frame");
    gst_buffer_unref (buffer);
    return nullptr;
  }

  return buffer;
}

GstBuffer *
gst_qsv_allocator_download_frame (GstQsvAllocator * allocator,
    gboolean force_copy, GstQsvFrame * frame, const GstVideoInfo * pool_info,
    GstBufferPool * pool)
{
  GstQsvAllocatorClass *klass;

  g_return_val_if_fail (GST_IS_QSV_ALLOCATOR (allocator), nullptr);
  g_return_val_if_fail (GST_IS_QSV_FRAME (frame), nullptr);
  g_return_val_if_fail (GST_IS_BUFFER_POOL (pool), nullptr);

  if (GST_QSV_MEM_TYPE_IS_SYSTEM (frame->mem_type)) {
    return gst_qsv_allocator_download_default (allocator, pool_info,
        force_copy, frame, pool);
  }

  klass = GST_QSV_ALLOCATOR_GET_CLASS (allocator);
  g_assert (klass->download);

  return klass->download (allocator, pool_info, force_copy, frame, pool);
}

mfxFrameAllocator *
gst_qsv_allocator_get_allocator_handle (GstQsvAllocator * allocator)
{
  g_return_val_if_fail (GST_IS_QSV_ALLOCATOR (allocator), nullptr);

  return &allocator->priv->allocator;
}

gboolean
gst_qsv_allocator_get_cached_response (GstQsvAllocator * allocator,
    mfxFrameAllocResponse * response)
{
  g_return_val_if_fail (GST_IS_QSV_ALLOCATOR (allocator), FALSE);

  return gst_qsv_allocator_copy_cached_response (allocator,
      response, &allocator->priv->response);
}

void
gst_qsv_allocator_set_options (GstQsvAllocator * allocator,
    guint16 extra_alloc_size, gboolean dummy_alloc)
{
  g_return_if_fail (GST_IS_QSV_ALLOCATOR (allocator));

  allocator->priv->extra_alloc_size = extra_alloc_size;
  allocator->priv->dummy_alloc = dummy_alloc;
}
