/* GStreamer
 * Copyright (C) 2022 Seungha Yang <seungha@centricular.com>
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

#include "gstqsvdecoder.h"
#include <mfxvideo++.h>
#include <string.h>

#ifdef G_OS_WIN32
#include "gstqsvallocator_d3d11.h"

#include <wrl.h>

/* *INDENT-OFF* */
using namespace Microsoft::WRL;
/* *INDENT-ON* */
#else
#include "gstqsvallocator_va.h"
#endif /* G_OS_WIN32 */

#include <queue>

GST_DEBUG_CATEGORY_STATIC (gst_qsv_decoder_debug);
#define GST_CAT_DEFAULT gst_qsv_decoder_debug

#define GST_QSV_DECODER_FLOW_NEW_SEQUENCE GST_FLOW_CUSTOM_SUCCESS_1

enum
{
  PROP_0,
  PROP_ADAPTER_LUID,
  PROP_DEVICE_PATH,
};

typedef struct _GstQsvDecoderSurface
{
  mfxFrameSurface1 surface;

  /* mfxFrameSurface1:Data:MemId */
  GstQsvFrame *frame;
  gboolean need_output;
} GstQsvDecoderSurface;

typedef struct _GstQsvDecoderTask
{
  mfxSyncPoint sync_point;

  /* without ownership */
  GstQsvDecoderSurface *surface;
} GstQsvDecoderTask;

struct _GstQsvDecoderPrivate
{
  GstObject *device;

  GstVideoCodecState *input_state;
  GstVideoCodecState *output_state;
  GstQsvAllocator *allocator;

  GstBufferPool *internal_pool;

  GstVideoInfo info;
  GstVideoInfo aligned_info;

  mfxSession session;
  mfxVideoParam video_param;
  mfxExtVideoSignalInfo signal_info;
  mfxExtBuffer *video_param_ext[1];

  /* holding allocated GstQsvFrame, should be cleared via
   * mfxFrameAllocator::Free() */
  mfxFrameAllocResponse response;

  MFXVideoDECODE *decoder;
  GstQsvMemoryType mem_type;
  gboolean use_video_memory;
  gboolean have_video_meta;

  gboolean is_live;

  /* Array of GstQsvDecoderSurface */
  GArray *surface_pool;

  /* Array of GstQsvDecoderTask */
  GArray *task_pool;
  guint next_task_index;
};

/**
 * GstQsvDecoder:
 *
 * Base class for Intel Quick Sync video decoders
 *
 * Since: 1.22
 */
#define gst_qsv_decoder_parent_class parent_class
G_DEFINE_ABSTRACT_TYPE_WITH_CODE (GstQsvDecoder, gst_qsv_decoder,
    GST_TYPE_VIDEO_DECODER, G_ADD_PRIVATE (GstQsvDecoder);
    GST_DEBUG_CATEGORY_INIT (gst_qsv_decoder_debug,
        "qsvdecoder", 0, "qsvdecoder"));

static void gst_qsv_decoder_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_qsv_decoder_dispose (GObject * object);
static void gst_qsv_decoder_finalize (GObject * object);

static void gst_qsv_decoder_set_context (GstElement * element,
    GstContext * context);

static gboolean gst_qsv_decoder_open (GstVideoDecoder * decoder);
static gboolean gst_qsv_decoder_stop (GstVideoDecoder * decoder);
static gboolean gst_qsv_decoder_close (GstVideoDecoder * decoder);
static gboolean gst_qsv_decoder_set_format (GstVideoDecoder * decoder,
    GstVideoCodecState * state);
static gboolean gst_qsv_decoder_negotiate (GstVideoDecoder * decoder);
static gboolean gst_qsv_decoder_decide_allocation (GstVideoDecoder * decoder,
    GstQuery * query);
static gboolean gst_qsv_decoder_sink_query (GstVideoDecoder * decoder,
    GstQuery * query);
static gboolean gst_qsv_decoder_src_query (GstVideoDecoder * decoder,
    GstQuery * query);
static GstFlowReturn gst_qsv_decoder_handle_frame (GstVideoDecoder * decoder,
    GstVideoCodecFrame * frame);
static gboolean gst_qsv_decoder_flush (GstVideoDecoder * decoder);
static GstFlowReturn gst_qsv_decoder_finish (GstVideoDecoder * decoder);
static GstFlowReturn gst_qsv_decoder_drain (GstVideoDecoder * decoder);

static void gst_qsv_decoder_surface_clear (GstQsvDecoderSurface * surface);
static void gst_qsv_decoder_task_clear (GstQsvDecoderTask * task);
static gboolean gst_qsv_decoder_negotiate_internal (GstVideoDecoder * decoder,
    const mfxFrameInfo * frame_info);

static void
gst_qsv_decoder_class_init (GstQsvDecoderClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstVideoDecoderClass *videodec_class = GST_VIDEO_DECODER_CLASS (klass);
  GParamFlags param_flags = (GParamFlags) (GST_PARAM_DOC_SHOW_DEFAULT |
      GST_PARAM_CONDITIONALLY_AVAILABLE | G_PARAM_READABLE |
      G_PARAM_STATIC_STRINGS);

  object_class->get_property = gst_qsv_decoder_get_property;
  object_class->dispose = gst_qsv_decoder_dispose;
  object_class->finalize = gst_qsv_decoder_finalize;

#ifdef G_OS_WIN32
  g_object_class_install_property (object_class, PROP_ADAPTER_LUID,
      g_param_spec_int64 ("adapter-luid", "Adapter LUID",
          "DXGI Adapter LUID (Locally Unique Identifier) of created device",
          G_MININT64, G_MAXINT64, 0, param_flags));
#else
  g_object_class_install_property (object_class, PROP_DEVICE_PATH,
      g_param_spec_string ("device-path", "Device Path",
          "DRM device path", nullptr, param_flags));
#endif

  element_class->set_context = GST_DEBUG_FUNCPTR (gst_qsv_decoder_set_context);

  videodec_class->open = GST_DEBUG_FUNCPTR (gst_qsv_decoder_open);
  videodec_class->stop = GST_DEBUG_FUNCPTR (gst_qsv_decoder_stop);
  videodec_class->close = GST_DEBUG_FUNCPTR (gst_qsv_decoder_close);
  videodec_class->negotiate = GST_DEBUG_FUNCPTR (gst_qsv_decoder_negotiate);
  videodec_class->decide_allocation =
      GST_DEBUG_FUNCPTR (gst_qsv_decoder_decide_allocation);
  videodec_class->sink_query = GST_DEBUG_FUNCPTR (gst_qsv_decoder_sink_query);
  videodec_class->src_query = GST_DEBUG_FUNCPTR (gst_qsv_decoder_src_query);
  videodec_class->set_format = GST_DEBUG_FUNCPTR (gst_qsv_decoder_set_format);
  videodec_class->handle_frame =
      GST_DEBUG_FUNCPTR (gst_qsv_decoder_handle_frame);
  videodec_class->drain = GST_DEBUG_FUNCPTR (gst_qsv_decoder_drain);
  videodec_class->finish = GST_DEBUG_FUNCPTR (gst_qsv_decoder_finish);
  videodec_class->flush = GST_DEBUG_FUNCPTR (gst_qsv_decoder_flush);

  gst_type_mark_as_plugin_api (GST_TYPE_QSV_DECODER, (GstPluginAPIFlags) 0);
}

static void
gst_qsv_decoder_init (GstQsvDecoder * self)
{
  GstQsvDecoderPrivate *priv;

  priv = self->priv =
      (GstQsvDecoderPrivate *) gst_qsv_decoder_get_instance_private (self);

  priv->surface_pool = g_array_new (FALSE, TRUE, sizeof (GstQsvDecoderSurface));
  g_array_set_clear_func (priv->surface_pool,
      (GDestroyNotify) gst_qsv_decoder_surface_clear);

  priv->task_pool = g_array_new (FALSE, TRUE, sizeof (GstQsvDecoderTask));
  g_array_set_clear_func (priv->task_pool,
      (GDestroyNotify) gst_qsv_decoder_task_clear);

  gst_video_decoder_set_packetized (GST_VIDEO_DECODER (self), TRUE);
}

static void
gst_qsv_decoder_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstQsvDecoderClass *klass = GST_QSV_DECODER_GET_CLASS (object);

  switch (prop_id) {
    case PROP_ADAPTER_LUID:
      g_value_set_int64 (value, klass->adapter_luid);
      break;
    case PROP_DEVICE_PATH:
      g_value_set_string (value, klass->display_path);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_qsv_decoder_dispose (GObject * object)
{
  GstQsvDecoder *self = GST_QSV_DECODER (object);
  GstQsvDecoderPrivate *priv = self->priv;

  gst_clear_object (&priv->device);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_qsv_decoder_finalize (GObject * object)
{
  GstQsvDecoder *self = GST_QSV_DECODER (object);
  GstQsvDecoderPrivate *priv = self->priv;

  g_array_unref (priv->task_pool);
  g_array_unref (priv->surface_pool);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_qsv_decoder_set_context (GstElement * element, GstContext * context)
{
  GstQsvDecoder *self = GST_QSV_DECODER (element);
  GstQsvDecoderClass *klass = GST_QSV_DECODER_GET_CLASS (element);
  GstQsvDecoderPrivate *priv = self->priv;

#ifdef G_OS_WIN32
  gst_d3d11_handle_set_context_for_adapter_luid (element,
      context, klass->adapter_luid, (GstD3D11Device **) & priv->device);
#else
  gst_va_handle_set_context (element, context, klass->display_path,
      (GstVaDisplay **) & priv->device);
#endif

  GST_ELEMENT_CLASS (parent_class)->set_context (element, context);
}

#ifdef G_OS_WIN32
static gboolean
gst_qsv_decoder_open_platform_device (GstQsvDecoder * self)
{
  GstQsvDecoderPrivate *priv = self->priv;
  GstQsvDecoderClass *klass = GST_QSV_DECODER_GET_CLASS (self);
  ComPtr < ID3D10Multithread > multi_thread;
  HRESULT hr;
  ID3D11Device *device_handle;
  mfxStatus status;
  GstD3D11Device *device;

  if (!gst_d3d11_ensure_element_data_for_adapter_luid (GST_ELEMENT (self),
          klass->adapter_luid, (GstD3D11Device **) & priv->device)) {
    GST_ERROR_OBJECT (self, "d3d11 device is unavailable");
    return FALSE;
  }

  device = GST_D3D11_DEVICE_CAST (priv->device);
  priv->allocator = gst_qsv_d3d11_allocator_new (device);

  /* For D3D11 device handle to be used by QSV, multithread protection layer
   * must be enabled before the MFXVideoCORE_SetHandle() call.
   *
   * TODO: Need to check performance impact by this mutithread protection layer,
   * since it may have a negative impact on overall pipeline performance.
   * If so, we should create decoding session dedicated d3d11 device and
   * make use of shared resource */
  device_handle = gst_d3d11_device_get_device_handle (device);
  hr = device_handle->QueryInterface (IID_PPV_ARGS (&multi_thread));
  if (!gst_d3d11_result (hr, device)) {
    GST_ERROR_OBJECT (self, "ID3D10Multithread interface is unavailable");
    return FALSE;
  }

  multi_thread->SetMultithreadProtected (TRUE);
  status = MFXVideoCORE_SetHandle (priv->session, MFX_HANDLE_D3D11_DEVICE,
      device_handle);
  if (status != MFX_ERR_NONE) {
    GST_ERROR_OBJECT (self, "Failed to set d3d11 device handle");
    return FALSE;
  }

  /* Similar to the QSV encoder, we don't use this allocator for actual
   * D3D11 texture allocation. But still required because of QSV API design.
   */
  status = MFXVideoCORE_SetFrameAllocator (priv->session,
      gst_qsv_allocator_get_allocator_handle (priv->allocator));
  if (status != MFX_ERR_NONE) {
    GST_ERROR_OBJECT (self, "Failed to set frame allocator %d", status);
    return FALSE;
  }

  return TRUE;
}
#else
static gboolean
gst_qsv_decoder_open_platform_device (GstQsvDecoder * self)
{
  GstQsvDecoderPrivate *priv = self->priv;
  GstQsvDecoderClass *klass = GST_QSV_DECODER_GET_CLASS (self);
  mfxStatus status;
  GstVaDisplay *display;

  if (!gst_va_ensure_element_data (GST_ELEMENT (self), klass->display_path,
          (GstVaDisplay **) & priv->device)) {
    GST_ERROR_OBJECT (self, "VA display is unavailable");
    return FALSE;
  }

  display = GST_VA_DISPLAY (priv->device);

  priv->allocator = gst_qsv_va_allocator_new (display);

  status = MFXVideoCORE_SetHandle (priv->session, MFX_HANDLE_VA_DISPLAY,
      gst_va_display_get_va_dpy (display));
  if (status != MFX_ERR_NONE) {
    GST_ERROR_OBJECT (self, "Failed to set VA display handle");
    return FALSE;
  }

  status = MFXVideoCORE_SetFrameAllocator (priv->session,
      gst_qsv_allocator_get_allocator_handle (priv->allocator));
  if (status != MFX_ERR_NONE) {
    GST_ERROR_OBJECT (self, "Failed to set frame allocator %d", status);
    return FALSE;
  }

  return TRUE;
}
#endif

static gboolean
gst_qsv_decoder_open (GstVideoDecoder * decoder)
{
  GstQsvDecoder *self = GST_QSV_DECODER (decoder);
  GstQsvDecoderPrivate *priv = self->priv;
  GstQsvDecoderClass *klass = GST_QSV_DECODER_GET_CLASS (self);
  mfxStatus status;

  status = MFXCreateSession (gst_qsv_get_loader (), klass->impl_index,
      &priv->session);
  if (status != MFX_ERR_NONE) {
    GST_ERROR_OBJECT (self, "Failed to create session");
    return FALSE;
  }

  if (!gst_qsv_decoder_open_platform_device (self)) {
    g_clear_pointer (&priv->session, MFXClose);
    gst_clear_object (&priv->allocator);
    gst_clear_object (&priv->device);

    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_qsv_decoder_reset (GstQsvDecoder * self)
{
  GstQsvDecoderPrivate *priv = self->priv;

  GST_DEBUG_OBJECT (self, "Reset");

  if (priv->decoder) {
    delete priv->decoder;
    priv->decoder = nullptr;
  }

  if (priv->internal_pool) {
    gst_buffer_pool_set_active (priv->internal_pool, FALSE);
    gst_clear_object (&priv->internal_pool);
  }

  if (priv->allocator) {
    mfxFrameAllocator *alloc =
        gst_qsv_allocator_get_allocator_handle (priv->allocator);
    alloc->Free ((mfxHDL) priv->allocator, &priv->response);
  }
  memset (&priv->response, 0, sizeof (mfxFrameAllocResponse));

  g_array_set_size (priv->surface_pool, 0);
  g_array_set_size (priv->task_pool, 0);

  return TRUE;
}

static gboolean
gst_qsv_decoder_stop (GstVideoDecoder * decoder)
{
  GstQsvDecoder *self = GST_QSV_DECODER (decoder);
  GstQsvDecoderPrivate *priv = self->priv;

  g_clear_pointer (&priv->input_state, gst_video_codec_state_unref);
  g_clear_pointer (&priv->output_state, gst_video_codec_state_unref);

  return gst_qsv_decoder_reset (self);
}

static gboolean
gst_qsv_decoder_close (GstVideoDecoder * decoder)
{
  GstQsvDecoder *self = GST_QSV_DECODER (decoder);
  GstQsvDecoderPrivate *priv = self->priv;

  g_clear_pointer (&priv->session, MFXClose);
  gst_clear_object (&priv->allocator);
  gst_clear_object (&priv->device);

  return TRUE;
}

static void
gst_qsv_decoder_surface_clear (GstQsvDecoderSurface * surface)
{
  if (!surface)
    return;

  memset (surface, 0, sizeof (GstQsvDecoderSurface));
}

static void
gst_qsv_decoder_task_clear (GstQsvDecoderTask * task)
{
  if (!task)
    return;

  if (task->surface) {
    task->surface->need_output = FALSE;
    if (task->surface->frame && task->surface->surface.Data.Locked == 0)
      gst_qsv_frame_set_buffer (task->surface->frame, nullptr);
  }

  task->surface = nullptr;
  task->sync_point = nullptr;
}

static GstQsvDecoderSurface *
gst_qsv_decoder_get_next_surface (GstQsvDecoder * self)
{
  GstQsvDecoderPrivate *priv = self->priv;
  GstQsvDecoderSurface *surface = nullptr;
  GstBuffer *buffer;
  GstFlowReturn ret;

  /* Clear unlocked frames as well */
  for (guint i = 0; i < priv->surface_pool->len; i++) {
    GstQsvDecoderSurface *iter =
        &g_array_index (priv->surface_pool, GstQsvDecoderSurface, i);

    if (iter->surface.Data.Locked > 0 || iter->need_output)
      continue;

    gst_qsv_frame_set_buffer (iter->frame, nullptr);

    if (!surface)
      surface = iter;
  }

  if (!surface) {
    GST_ERROR_OBJECT (self, "Failed to find unlocked surface");
    return nullptr;
  }

  ret = gst_buffer_pool_acquire_buffer (priv->internal_pool, &buffer, nullptr);
  if (ret != GST_FLOW_OK) {
    GST_ERROR_OBJECT (self, "Failed to allocate buffer");
    return nullptr;
  }

  gst_qsv_frame_set_buffer (surface->frame, buffer);

  return surface;
}

static GstQsvDecoderTask *
gst_qsv_decoder_get_next_task (GstQsvDecoder * self)
{
  GstQsvDecoderPrivate *priv = self->priv;
  GstQsvDecoderTask *task;

  task = &g_array_index (priv->task_pool,
      GstQsvDecoderTask, priv->next_task_index);
  priv->next_task_index++;
  priv->next_task_index %= priv->task_pool->len;

  return task;
}

static GstVideoCodecFrame *
gst_qsv_decoder_find_output_frame (GstQsvDecoder * self, GstClockTime pts)
{
  auto videodec = GST_VIDEO_DECODER (self);
  GList *frames, *iter;
  GstVideoCodecFrame *ret = nullptr;
  GstVideoCodecFrame *closest = nullptr;
  guint64 min_pts_abs_diff = 0;

  /* give up, just returns the oldest frame */
  if (!GST_CLOCK_TIME_IS_VALID (pts))
    return gst_video_decoder_get_oldest_frame (videodec);

  frames = gst_video_decoder_get_frames (videodec);

  for (iter = frames; iter; iter = g_list_next (iter)) {
    GstVideoCodecFrame *frame = (GstVideoCodecFrame *) iter->data;
    guint64 abs_diff;

    if (!GST_CLOCK_TIME_IS_VALID (frame->pts))
      continue;

    if (pts == frame->pts) {
      ret = frame;
      break;
    }

    if (pts >= frame->pts)
      abs_diff = pts - frame->pts;
    else
      abs_diff = frame->pts - pts;

    if (!closest || abs_diff < min_pts_abs_diff) {
      closest = frame;
      min_pts_abs_diff = abs_diff;
    }
  }

  if (!ret && closest)
    ret = closest;

  if (ret) {
    gst_video_codec_frame_ref (ret);

    /* Do garbage collection */
    std::queue < GstVideoCodecFrame * >old_frames;

    /* Release older frames, it can happen if input buffer holds only single
     * field in case of H264 or incomplete AU */
    for (iter = frames; iter; iter = g_list_next (iter)) {
      GstVideoCodecFrame *frame = (GstVideoCodecFrame *) iter->data;

      if (frame == ret)
        break;

      old_frames.push (gst_video_codec_frame_ref (frame));
    }

    GST_LOG_OBJECT (self, "%u frames are queued before the current output",
        (guint) old_frames.size ());

    while (old_frames.size () > 16) {
      auto front = old_frames.front ();
      old_frames.pop ();
      gst_video_decoder_release_frame (videodec, front);
    }

    while (!old_frames.empty ()) {
      auto front = old_frames.front ();
      old_frames.pop ();
      gst_video_codec_frame_unref (front);
    }
  } else {
    ret = gst_video_decoder_get_oldest_frame (GST_VIDEO_DECODER (self));
  }

  if (frames)
    g_list_free_full (frames, (GDestroyNotify) gst_video_codec_frame_unref);

  return ret;
}

static GstFlowReturn
gst_qsv_decoder_finish_frame (GstQsvDecoder * self, GstQsvDecoderTask * task,
    gboolean flushing)
{
  GstVideoDecoder *vdec = GST_VIDEO_DECODER (self);
  GstQsvDecoderPrivate *priv = self->priv;
  GstQsvDecoderClass *klass = GST_QSV_DECODER_GET_CLASS (self);
  mfxStatus status;
  GstVideoCodecFrame *frame;
  GstClockTime pts = GST_CLOCK_TIME_NONE;
  GstBuffer *buffer;
  guint retry_count = 0;
  /* magic number */
  const guint retry_threshold = 100;
  GstQsvDecoderSurface *surface = task->surface;
  GstBufferPool *pool;
  gboolean force_copy = FALSE;

  g_assert (surface != nullptr);
  g_assert (task->sync_point != nullptr);

  status = MFX_ERR_NONE;
  do {
    /* magic number 100 ms */
    status = MFXVideoCORE_SyncOperation (priv->session, task->sync_point, 100);

    /* Retry up to 10 sec (100 ms x 100 times), that should be enough time for
     * decoding a frame using hardware */
    if (status == MFX_WRN_IN_EXECUTION && retry_count < retry_threshold) {
      GST_DEBUG_OBJECT (self,
          "Operation is still in execution, retry count (%d/%d)",
          retry_count, retry_threshold);
      retry_count++;
      continue;
    }

    break;
  } while (TRUE);

  if (flushing) {
    gst_qsv_decoder_task_clear (task);
    return GST_FLOW_OK;
  }

  if (status != MFX_ERR_NONE) {
    gst_qsv_decoder_task_clear (task);

    if (status == MFX_ERR_ABORTED) {
      GST_INFO_OBJECT (self, "Operation was aborted");
      return GST_FLOW_FLUSHING;
    }

    GST_WARNING_OBJECT (self, "SyncOperation returned %d (%s)",
        QSV_STATUS_ARGS (status));

    return GST_FLOW_ERROR;
  }

  /* Handle non-keyframe resolution change */
  if (klass->codec_id == MFX_CODEC_VP9) {
    guint width, height;

    if (surface->surface.Info.CropW > 0 && surface->surface.Info.CropH > 0) {
      width = surface->surface.Info.CropW;
      height = surface->surface.Info.CropH;
    } else {
      width = surface->surface.Info.Width;
      height = surface->surface.Info.Height;
    }

    if (width != (guint) priv->output_state->info.width ||
        height != (guint) priv->output_state->info.height) {
      GST_DEBUG_OBJECT (self,
          "VP9 resolution change %dx%d -> %dx%d, negotiate again",
          priv->output_state->info.width, priv->output_state->info.height,
          width, height);
      if (!gst_qsv_decoder_negotiate_internal (vdec, &surface->surface.Info)) {
        GST_ERROR_OBJECT (self, "Could not negotiate with downstream");
        return GST_FLOW_NOT_NEGOTIATED;
      }
    }

    /* TODO: Use crop meta if supported by downstream.
     * Most d3d11 elements supports crop meta */
    if (width != (guint) priv->info.width
        || height != (guint) priv->info.height)
      force_copy = TRUE;
  }

  pts = gst_qsv_timestamp_to_gst (surface->surface.Data.TimeStamp);
  pool = gst_video_decoder_get_buffer_pool (vdec);
  if (!pool) {
    GST_ERROR_OBJECT (self, "Decoder doesn't hold buffer pool");
    gst_qsv_decoder_task_clear (task);
    return GST_FLOW_ERROR;
  }

  if (priv->use_video_memory) {
    /* Copy decoded frame in case of reverse playback, too many bound frame to
     * decoder may cause driver unhappy */
    if (vdec->input_segment.rate < 0.0)
      force_copy = TRUE;
  } else if (!priv->have_video_meta) {
    /* downstream does not support video meta, need copy */
    force_copy = TRUE;
  }

  /* TODO: Handle non-zero crop-{x,y} position via crop meta or similar */
  buffer = gst_qsv_allocator_download_frame (priv->allocator, force_copy,
      surface->frame, &priv->output_state->info, pool);
  gst_object_unref (pool);
  gst_qsv_decoder_task_clear (task);

  if (!buffer) {
    GST_ERROR_OBJECT (self, "No output buffer");
    return GST_FLOW_ERROR;
  }

  if (priv->aligned_info.interlace_mode == GST_VIDEO_INTERLACE_MODE_MIXED) {
    if ((surface->surface.Info.PicStruct & MFX_PICSTRUCT_FIELD_TFF) != 0) {
      GST_BUFFER_FLAG_SET (buffer,
          GST_VIDEO_BUFFER_FLAG_TFF | GST_VIDEO_BUFFER_FLAG_INTERLACED);
    } else if ((surface->surface.Info.PicStruct & MFX_PICSTRUCT_FIELD_BFF) != 0) {
      GST_BUFFER_FLAG_SET (buffer, GST_VIDEO_BUFFER_FLAG_INTERLACED);
      GST_BUFFER_FLAG_UNSET (buffer, GST_VIDEO_BUFFER_FLAG_TFF);
    }
  }

  frame = gst_qsv_decoder_find_output_frame (self, pts);
  if (frame) {
    frame->pts = pts;
    frame->output_buffer = buffer;

    return gst_video_decoder_finish_frame (vdec, frame);
  }

  /* Empty available frame, something went wrong but we can just push this
   * buffer */
  GST_WARNING_OBJECT (self, "Failed to find corresponding frame");
  GST_BUFFER_PTS (buffer) = pts;

  return gst_pad_push (GST_VIDEO_DECODER_SRC_PAD (self), buffer);
}

static GstFlowReturn
gst_qsv_decoder_decode_frame (GstQsvDecoder * self, mfxBitstream * bitstream,
    gboolean flushing)
{
  GstQsvDecoderPrivate *priv = self->priv;
  mfxStatus status;
  guint retry_count = 0;
  /* magic number */
  const guint retry_threshold = 1000;
  GstQsvDecoderSurface *surface = nullptr;
  GstFlowReturn ret;

  do {
    mfxFrameSurface1 *out_surface = nullptr;
    GstQsvDecoderTask *task = gst_qsv_decoder_get_next_task (self);
    if (task->sync_point) {
      ret = gst_qsv_decoder_finish_frame (self, task, flushing);

      if (ret != GST_FLOW_OK)
        return ret;
    }

    if (!surface)
      surface = gst_qsv_decoder_get_next_surface (self);

    if (!surface) {
      GST_ERROR_OBJECT (self, "No available surface");
      return GST_FLOW_ERROR;
    }

    status = priv->decoder->DecodeFrameAsync (bitstream, &surface->surface,
        &out_surface, &task->sync_point);

    if (status != MFX_ERR_NONE) {
      GST_LOG_OBJECT (self, "DecodeFrameAsync returned %d (%s)",
          QSV_STATUS_ARGS (status));
    }

    if (out_surface) {
      g_assert (task->sync_point != nullptr);

      for (guint i = 0; i < priv->surface_pool->len; i++) {
        GstQsvDecoderSurface *iter =
            &g_array_index (priv->surface_pool, GstQsvDecoderSurface, i);

        if (iter->surface.Data.MemId == out_surface->Data.MemId) {
          task->surface = iter;
          break;
        }
      }

      if (!task->surface) {
        GST_ERROR_OBJECT (self, "Failed to find surface");
        gst_qsv_decoder_task_clear (task);
        return GST_FLOW_ERROR;
      }

      /* Make need-output to hold underlying GstBuffer until output happens  */
      task->surface->need_output = TRUE;
    }

    switch (status) {
      case MFX_ERR_NONE:
      case MFX_WRN_VIDEO_PARAM_CHANGED:
        if (surface->surface.Data.Locked > 0)
          surface = nullptr;

        if (bitstream && bitstream->DataLength == 0)
          return GST_FLOW_OK;
        break;
      case MFX_ERR_MORE_SURFACE:
        return GST_FLOW_OK;
      case MFX_ERR_INCOMPATIBLE_VIDEO_PARAM:
        GST_DEBUG_OBJECT (self, "Found new sequence");
        return GST_QSV_DECODER_FLOW_NEW_SEQUENCE;
      case MFX_ERR_MORE_DATA:
        return GST_VIDEO_DECODER_FLOW_NEED_DATA;
      case MFX_WRN_DEVICE_BUSY:
        GST_LOG_OBJECT (self, "GPU is busy, retry count (%d/%d)",
            retry_count, retry_threshold);

        if (retry_count > retry_threshold) {
          GST_ERROR_OBJECT (self, "Give up");
          return GST_FLOW_ERROR;
        }

        retry_count++;

        /* Magic number 1ms */
        g_usleep (1000);
        break;
      default:
        if (status < MFX_ERR_NONE) {
          GST_ERROR_OBJECT (self, "Got error %d (%s)",
              QSV_STATUS_ARGS (status));
          return GST_FLOW_ERROR;
        }
        break;
    }
  } while (TRUE);

  return GST_FLOW_ERROR;
}

static GstFlowReturn
gst_qsv_decoder_drain_internal (GstQsvDecoder * self, gboolean flushing)
{
  GstQsvDecoderPrivate *priv = self->priv;
  GstFlowReturn ret = GST_FLOW_OK;

  if (!priv->session || !priv->decoder)
    return GST_FLOW_OK;

  do {
    ret = gst_qsv_decoder_decode_frame (self, nullptr, flushing);
  } while (ret != GST_VIDEO_DECODER_FLOW_NEED_DATA && ret >= GST_FLOW_OK);

  for (guint i = 0; i < priv->task_pool->len; i++) {
    GstQsvDecoderTask *task = gst_qsv_decoder_get_next_task (self);

    if (!task->sync_point)
      continue;

    ret = gst_qsv_decoder_finish_frame (self, task, flushing);
  }

  switch (ret) {
    case GST_VIDEO_DECODER_FLOW_NEED_DATA:
    case GST_QSV_DECODER_FLOW_NEW_SEQUENCE:
      return GST_FLOW_OK;
    default:
      break;
  }

  return ret;
}

static gboolean
gst_qsv_decoder_set_format (GstVideoDecoder * decoder,
    GstVideoCodecState * state)
{
  GstQsvDecoder *self = GST_QSV_DECODER (decoder);
  GstQsvDecoderPrivate *priv = self->priv;
  GstQsvDecoderClass *klass = GST_QSV_DECODER_GET_CLASS (self);
  GstQuery *query;

  GST_DEBUG_OBJECT (self, "Set format with %" GST_PTR_FORMAT, state->caps);

  gst_qsv_decoder_drain_internal (self, FALSE);

  g_clear_pointer (&priv->input_state, gst_video_codec_state_unref);

  priv->input_state = gst_video_codec_state_ref (state);

  memset (&priv->video_param, 0, sizeof (mfxVideoParam));
  priv->video_param.mfx.CodecId = klass->codec_id;
  priv->signal_info.Header.BufferId = MFX_EXTBUFF_VIDEO_SIGNAL_INFO;
  priv->signal_info.Header.BufferSz = sizeof (mfxExtVideoSignalInfo);
  priv->video_param_ext[0] = (mfxExtBuffer *) & priv->signal_info;
  priv->video_param.ExtParam = priv->video_param_ext;

  /* If upstream is live, we will use single async-depth for low-latency
   * decoding */
  query = gst_query_new_latency ();
  if (gst_pad_peer_query (GST_VIDEO_DECODER_SINK_PAD (self), query))
    gst_query_parse_latency (query, &priv->is_live, nullptr, nullptr);
  gst_query_unref (query);

  /* We will open decoder later once sequence header is parsed */
  if (klass->set_format)
    return klass->set_format (self, state);

  return TRUE;
}

#ifdef G_OS_WIN32
static gboolean
gst_qsv_decoder_prepare_d3d11_pool (GstQsvDecoder * self,
    GstCaps * caps, GstVideoInfo * info, GstVideoAlignment * align)
{
  GstQsvDecoderPrivate *priv = self->priv;
  GstStructure *config;
  GstD3D11AllocationParams *params;
  GstD3D11Device *device = GST_D3D11_DEVICE_CAST (priv->device);
  guint bind_flags = 0;
  GstD3D11Format d3d11_format;

  GST_DEBUG_OBJECT (self, "Use d3d11 memory pool");

  priv->internal_pool = gst_d3d11_buffer_pool_new (device);
  config = gst_buffer_pool_get_config (priv->internal_pool);

  gst_d3d11_device_get_format (device, GST_VIDEO_INFO_FORMAT (info),
      &d3d11_format);

  /* May not support DOV, specifically RGB output case */
  if ((d3d11_format.format_support[0] &
          (guint) D3D11_FORMAT_SUPPORT_DECODER_OUTPUT) != 0) {
    bind_flags |= D3D11_BIND_DECODER;
  } else if ((d3d11_format.format_support[0] &
          (guint) D3D11_FORMAT_SUPPORT_RENDER_TARGET) != 0) {
    bind_flags |= D3D11_BIND_RENDER_TARGET;
  }
  /* Bind to shader resource as well for this texture can be used
   * in generic pixel shader */
  if ((d3d11_format.format_support[0] & (guint)
          D3D11_FORMAT_SUPPORT_SHADER_SAMPLE) != 0)
    bind_flags |= D3D11_BIND_SHADER_RESOURCE;

  params = gst_d3d11_allocation_params_new (device, info,
      GST_D3D11_ALLOCATION_FLAG_DEFAULT, bind_flags, 0);
  gst_d3d11_allocation_params_alignment (params, align);
  gst_buffer_pool_config_set_d3d11_allocation_params (config, params);
  gst_d3d11_allocation_params_free (params);
  gst_buffer_pool_config_set_params (config, caps,
      GST_VIDEO_INFO_SIZE (info), 0, 0);
  gst_buffer_pool_set_config (priv->internal_pool, config);
  gst_buffer_pool_set_active (priv->internal_pool, TRUE);

  return TRUE;
}
#endif

static gboolean
gst_qsv_decoder_prepare_system_pool (GstQsvDecoder * self,
    GstCaps * caps, GstVideoInfo * info, GstVideoAlignment * align)
{
  GstQsvDecoderPrivate *priv = self->priv;
  GstStructure *config;

  GST_DEBUG_OBJECT (self, "Use system memory pool");

  priv->internal_pool = gst_video_buffer_pool_new ();
  config = gst_buffer_pool_get_config (priv->internal_pool);
  gst_buffer_pool_config_add_option (config, GST_BUFFER_POOL_OPTION_VIDEO_META);
  gst_buffer_pool_config_add_option (config,
      GST_BUFFER_POOL_OPTION_VIDEO_ALIGNMENT);
  gst_buffer_pool_config_set_video_alignment (config, align);
  gst_buffer_pool_config_set_params (config,
      caps, GST_VIDEO_INFO_SIZE (info), 0, 0);

  gst_buffer_pool_set_config (priv->internal_pool, config);
  gst_buffer_pool_set_active (priv->internal_pool, TRUE);

  return TRUE;
}

static gboolean
gst_qsv_decoder_prepare_pool (GstQsvDecoder * self, mfxU16 * io_pattern)
{
  GstQsvDecoderPrivate *priv = self->priv;
  gboolean ret = FALSE;
  GstCaps *caps;
  GstVideoAlignment align;

  if (priv->internal_pool) {
    gst_buffer_pool_set_active (priv->internal_pool, FALSE);
    gst_clear_object (&priv->internal_pool);
  }

  caps = gst_video_info_to_caps (&priv->info);
  if (!caps) {
    GST_ERROR_OBJECT (self, "Failed to convet video-info to caps");
    return FALSE;
  }

  gst_video_alignment_reset (&align);
  align.padding_right = priv->aligned_info.width - priv->info.width;
  align.padding_bottom = priv->aligned_info.height - priv->info.height;

  /* TODO: Add Linux video memory (VA/DMABuf) support */
#ifdef G_OS_WIN32
  if (priv->use_video_memory) {
    priv->mem_type = GST_QSV_VIDEO_MEMORY | GST_QSV_DECODER_OUT_MEMORY;
    *io_pattern = MFX_IOPATTERN_OUT_VIDEO_MEMORY;

    ret = gst_qsv_decoder_prepare_d3d11_pool (self, caps, &priv->info, &align);
  }
#endif

  if (!ret) {
    priv->mem_type = GST_QSV_SYSTEM_MEMORY | GST_QSV_DECODER_OUT_MEMORY;
    *io_pattern = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;

    ret = gst_qsv_decoder_prepare_system_pool (self, caps, &priv->info, &align);
  }
  gst_caps_unref (caps);

  return ret;
}

static gboolean
gst_qsv_decoder_init_session (GstQsvDecoder * self)
{
  GstQsvDecoderPrivate *priv = self->priv;
  mfxVideoParam *param = &priv->video_param;
  mfxFrameInfo *frame_info = &param->mfx.FrameInfo;
  MFXVideoDECODE *decoder_handle = nullptr;
  mfxFrameAllocRequest request;
  mfxStatus status;

  GST_DEBUG_OBJECT (self, "Init session");

  memset (&request, 0, sizeof (mfxFrameAllocRequest));

  gst_qsv_decoder_reset (self);

  if (!gst_qsv_decoder_prepare_pool (self, &param->IOPattern)) {
    GST_ERROR_OBJECT (self, "Failed to prepare pool");
    goto error;
  }

  param->AsyncDepth = priv->is_live ? 1 : 4;

  decoder_handle = new MFXVideoDECODE (priv->session);

  /* Additional 4 frames for margin. Actually large pool size would be fine
   * because we don't pre-allocate underlying output memory objects */
  gst_qsv_allocator_set_options (priv->allocator, param->AsyncDepth + 4, TRUE);

  status = decoder_handle->Query (param, param);
  QSV_CHECK_STATUS (self, status, MFXVideoDECODE::Query);

  status = decoder_handle->QueryIOSurf (param, &request);
  QSV_CHECK_STATUS (self, status, MFXVideoDECODE::QueryIOSurf);

  status = decoder_handle->Init (param);
  QSV_CHECK_STATUS (self, status, MFXVideoDECODE::Init);

  status = decoder_handle->GetVideoParam (param);
  QSV_CHECK_STATUS (self, status, MFXVideoDECODE::GetVideoParam);

  /* In case that we use video memory, MFXVideoDECODE::Init() will invoke
   * mfxFrameAllocator::Alloc(). Pull the pre-allocated dummy GstQsvFrame
   * objects here and fill with GstBuffer later when needed */
  if (GST_QSV_MEM_TYPE_IS_SYSTEM (priv->mem_type)) {
    mfxFrameAllocator *alloc_handle =
        gst_qsv_allocator_get_allocator_handle (priv->allocator);

    request.Type |= MFX_MEMTYPE_EXTERNAL_FRAME;
    status = alloc_handle->Alloc ((mfxHDL) priv->allocator, &request,
        &priv->response);
    if (status != MFX_ERR_NONE) {
      GST_ERROR_OBJECT (self, "Failed to allocate system memory frames");
      goto error;
    }
  } else if (!gst_qsv_allocator_get_cached_response (priv->allocator,
          &priv->response)) {
    GST_ERROR_OBJECT (self, "Failed to get cached response");
    goto error;
  }

  g_array_set_size (priv->surface_pool, priv->response.NumFrameActual);
  for (guint i = 0; i < priv->surface_pool->len; i++) {
    GstQsvDecoderSurface *surface = &g_array_index (priv->surface_pool,
        GstQsvDecoderSurface, i);
    GstBuffer *buf;

    gst_qsv_decoder_surface_clear (surface);
    surface->surface.Info = *frame_info;
    surface->surface.Data.MemId = priv->response.mids[i];

    /* holds casted object without ref, to make code cleaner */
    surface->frame = (GstQsvFrame *) surface->surface.Data.MemId;

    /* This frame must not hold buffer at this moment */
    buf = gst_qsv_frame_peek_buffer (surface->frame);
    g_assert (buf == nullptr);
  }

  g_array_set_size (priv->task_pool, param->AsyncDepth);
  for (guint i = 0; i < priv->task_pool->len; i++) {
    GstQsvDecoderTask *task = &g_array_index (priv->task_pool,
        GstQsvDecoderTask, i);

    gst_qsv_decoder_task_clear (task);
  }
  priv->next_task_index = 0;

  priv->decoder = decoder_handle;

  return TRUE;

error:
  if (decoder_handle)
    delete decoder_handle;

  gst_qsv_decoder_reset (self);

  return FALSE;
}

static gboolean
gst_qsv_decoder_negotiate_internal (GstVideoDecoder * decoder,
    const mfxFrameInfo * frame_info)
{
  GstQsvDecoder *self = GST_QSV_DECODER (decoder);
  GstQsvDecoderPrivate *priv = self->priv;
  guint width, height;

  width = frame_info->Width;
  height = frame_info->Height;

  if (frame_info->CropW > 0 && frame_info->CropH > 0) {
    width = frame_info->CropW;
    height = frame_info->CropH;
  }

  g_clear_pointer (&priv->output_state, gst_video_codec_state_unref);
  priv->output_state =
      gst_video_decoder_set_interlaced_output_state (GST_VIDEO_DECODER (self),
      GST_VIDEO_INFO_FORMAT (&priv->info),
      GST_VIDEO_INFO_INTERLACE_MODE (&priv->info),
      width, height, priv->input_state);

  priv->output_state->caps = gst_video_info_to_caps (&priv->output_state->info);
  priv->use_video_memory = FALSE;

#ifdef G_OS_WIN32
  GstCaps *peer_caps =
      gst_pad_get_allowed_caps (GST_VIDEO_DECODER_SRC_PAD (self));
  GST_DEBUG_OBJECT (self, "Allowed caps %" GST_PTR_FORMAT, peer_caps);

  if (!peer_caps || gst_caps_is_any (peer_caps)) {
    GST_DEBUG_OBJECT (self,
        "cannot determine output format, use system memory");
  } else {
    GstCapsFeatures *features;
    guint size = gst_caps_get_size (peer_caps);

    for (guint i = 0; i < size; i++) {
      features = gst_caps_get_features (peer_caps, i);

      if (!features)
        continue;

      if (gst_caps_features_contains (features,
              GST_CAPS_FEATURE_MEMORY_D3D11_MEMORY)) {
        priv->use_video_memory = TRUE;
        break;
      }
    }
  }
  gst_clear_caps (&peer_caps);

  if (priv->use_video_memory) {
    GST_DEBUG_OBJECT (self, "Downstream supports D3D11 memory");
    gst_caps_set_features (priv->output_state->caps, 0,
        gst_caps_features_new (GST_CAPS_FEATURE_MEMORY_D3D11_MEMORY, nullptr));
  }
#endif

  GST_DEBUG_OBJECT (self,
      "Negotiating with %" GST_PTR_FORMAT, priv->output_state->caps);

  return GST_VIDEO_DECODER_CLASS (parent_class)->negotiate (decoder);
}

static gboolean
gst_qsv_decoder_negotiate (GstVideoDecoder * decoder)
{
  GstQsvDecoder *self = GST_QSV_DECODER (decoder);
  GstQsvDecoderPrivate *priv = self->priv;
  GstQsvDecoderClass *klass = GST_QSV_DECODER_GET_CLASS (self);
  guint width, height;
  guint coded_width, coded_height;
  guint aligned_width, aligned_height;
  mfxVideoParam *param = &priv->video_param;
  mfxFrameInfo *frame_info = &param->mfx.FrameInfo;
  GstVideoFormat format = GST_VIDEO_FORMAT_UNKNOWN;
  GstVideoInterlaceMode interlace_mode = GST_VIDEO_INTERLACE_MODE_PROGRESSIVE;
  gboolean is_gbr = FALSE;

  width = coded_width = frame_info->Width;
  height = coded_height = frame_info->Height;

  if (frame_info->CropW > 0 && frame_info->CropH > 0) {
    width = frame_info->CropW;
    height = frame_info->CropH;
  }

  if (klass->codec_id == MFX_CODEC_HEVC &&
      priv->signal_info.ColourDescriptionPresent &&
      gst_video_color_matrix_from_iso (priv->signal_info.MatrixCoefficients) ==
      GST_VIDEO_COLOR_MATRIX_RGB) {
    is_gbr = TRUE;
  }

  if (priv->allocator)
    priv->allocator->is_gbr = is_gbr;

  if (klass->codec_id == MFX_CODEC_JPEG) {
    if (param->mfx.JPEGChromaFormat == MFX_CHROMAFORMAT_YUV422) {
      format = GST_VIDEO_FORMAT_YUY2;
      frame_info->FourCC = MFX_FOURCC_YUY2;
      frame_info->ChromaFormat = MFX_CHROMAFORMAT_YUV422;
    } else if (param->mfx.JPEGColorFormat == MFX_JPEG_COLORFORMAT_RGB) {
      format = GST_VIDEO_FORMAT_BGRA;
      frame_info->FourCC = MFX_FOURCC_RGB4;
      frame_info->ChromaFormat = MFX_CHROMAFORMAT_YUV444;
    }
  } else {
    format = gst_qsv_frame_info_format_to_gst (frame_info, is_gbr);
  }

  if (format == GST_VIDEO_FORMAT_UNKNOWN) {
    GST_ERROR_OBJECT (self, "Unknown video format");
    return FALSE;
  }

  aligned_width = GST_ROUND_UP_16 (coded_width);
  if (klass->codec_id == MFX_CODEC_AVC) {
    if (frame_info->PicStruct == MFX_PICSTRUCT_PROGRESSIVE) {
      aligned_height = GST_ROUND_UP_16 (coded_height);
    } else {
      aligned_height = GST_ROUND_UP_32 (coded_height);
      /* In theory, tff/bff can be altered in a sequence */
      interlace_mode = GST_VIDEO_INTERLACE_MODE_MIXED;
    }
  } else {
    aligned_height = GST_ROUND_UP_16 (coded_height);
  }

  frame_info->Width = aligned_width;
  frame_info->Height = aligned_height;

  gst_video_info_set_interlaced_format (&priv->info, format,
      interlace_mode, width, height);
  gst_video_info_set_interlaced_format (&priv->aligned_info, format,
      interlace_mode, aligned_width, aligned_height);

  return gst_qsv_decoder_negotiate_internal (decoder,
      &priv->video_param.mfx.FrameInfo);
}

#ifdef G_OS_WIN32
static gboolean
gst_qsv_decoder_decide_allocation (GstVideoDecoder * decoder, GstQuery * query)
{
  GstQsvDecoder *self = GST_QSV_DECODER (decoder);
  GstQsvDecoderPrivate *priv = self->priv;
  GstCaps *outcaps;
  GstBufferPool *pool = nullptr;
  guint n, size, min = 0, max = 0;
  GstVideoInfo vinfo;
  GstStructure *config;
  GstD3D11AllocationParams *d3d11_params;
  gboolean use_d3d11_pool;
  GstD3D11Device *device = GST_D3D11_DEVICE (priv->device);

  gst_query_parse_allocation (query, &outcaps, nullptr);

  if (!outcaps) {
    GST_DEBUG_OBJECT (decoder, "No output caps");
    return FALSE;
  }

  priv->have_video_meta = gst_query_find_allocation_meta (query,
      GST_VIDEO_META_API_TYPE, nullptr);
  use_d3d11_pool = priv->use_video_memory;

  gst_video_info_from_caps (&vinfo, outcaps);
  n = gst_query_get_n_allocation_pools (query);
  if (n > 0)
    gst_query_parse_nth_allocation_pool (query, 0, &pool, &size, &min, &max);

  if (pool && use_d3d11_pool) {
    if (!GST_IS_D3D11_BUFFER_POOL (pool)) {
      GST_DEBUG_OBJECT (self,
          "Downstream pool is not d3d11, will create new one");
      gst_clear_object (&pool);
    } else {
      GstD3D11BufferPool *dpool = GST_D3D11_BUFFER_POOL (pool);
      if (dpool->device != device) {
        GST_DEBUG_OBJECT (self, "Different device, will create new one");
        gst_clear_object (&pool);
      }
    }
  }

  if (!pool) {
    if (use_d3d11_pool)
      pool = gst_d3d11_buffer_pool_new (device);
    else
      pool = gst_video_buffer_pool_new ();

    size = (guint) vinfo.size;
  }

  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_set_params (config, outcaps, size, min, max);
  gst_buffer_pool_config_add_option (config, GST_BUFFER_POOL_OPTION_VIDEO_META);

  /* Decoder will use internal pool to output but this pool is required for
   * copying in case of reverse playback */
  if (use_d3d11_pool) {
    guint bind_flags = 0;
    GstD3D11Format d3d11_format;

    d3d11_params = gst_buffer_pool_config_get_d3d11_allocation_params (config);
    if (!d3d11_params) {
      d3d11_params = gst_d3d11_allocation_params_new (device, &vinfo,
          GST_D3D11_ALLOCATION_FLAG_DEFAULT, 0, 0);
    }

    gst_d3d11_device_get_format (device, GST_VIDEO_INFO_FORMAT (&vinfo),
        &d3d11_format);

    /* Use both render target (for videoprocessor) and shader resource
     * for (pixel shader) bind flags for downstream to be able to use consistent
     * conversion path even when we copy textures */
    if ((d3d11_format.format_support[0] &
            (guint) D3D11_FORMAT_SUPPORT_RENDER_TARGET) != 0) {
      bind_flags |= D3D11_BIND_RENDER_TARGET;
    }

    if ((d3d11_format.format_support[0] & (guint)
            D3D11_FORMAT_SUPPORT_SHADER_SAMPLE) != 0)
      bind_flags |= D3D11_BIND_SHADER_RESOURCE;

    gst_d3d11_allocation_params_set_bind_flags (d3d11_params, bind_flags);
    gst_buffer_pool_config_set_d3d11_allocation_params (config, d3d11_params);
    gst_d3d11_allocation_params_free (d3d11_params);
  }

  gst_buffer_pool_set_config (pool, config);
  /* d3d11 buffer pool will update buffer size based on allocated texture,
   * get size from config again */
  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_get_params (config, nullptr, &size, nullptr, nullptr);
  gst_structure_free (config);

  if (n > 0)
    gst_query_set_nth_allocation_pool (query, 0, pool, size, min, max);
  else
    gst_query_add_allocation_pool (query, pool, size, min, max);
  gst_object_unref (pool);

  return GST_VIDEO_DECODER_CLASS (parent_class)->decide_allocation (decoder,
      query);
}
#else
static gboolean
gst_qsv_decoder_decide_allocation (GstVideoDecoder * decoder, GstQuery * query)
{
  /* TODO: add VA support */
  return GST_VIDEO_DECODER_CLASS (parent_class)->decide_allocation (decoder,
      query);
}
#endif /* G_OS_WIN32 */

static gboolean
gst_qsv_decoder_handle_context_query (GstQsvDecoder * self, GstQuery * query)
{
  GstQsvDecoderPrivate *priv = self->priv;

#ifdef G_OS_WIN32
  return gst_d3d11_handle_context_query (GST_ELEMENT (self), query,
      (GstD3D11Device *) priv->device);
#else
  return gst_va_handle_context_query (GST_ELEMENT (self), query,
      (GstVaDisplay *) priv->device);
#endif
}

static gboolean
gst_qsv_decoder_sink_query (GstVideoDecoder * decoder, GstQuery * query)
{
  GstQsvDecoder *self = GST_QSV_DECODER (decoder);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CONTEXT:
      if (gst_qsv_decoder_handle_context_query (self, query))
        return TRUE;
      break;
    default:
      break;
  }

  return GST_VIDEO_DECODER_CLASS (parent_class)->sink_query (decoder, query);
}

static gboolean
gst_qsv_decoder_src_query (GstVideoDecoder * decoder, GstQuery * query)
{
  GstQsvDecoder *self = GST_QSV_DECODER (decoder);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CONTEXT:
      if (gst_qsv_decoder_handle_context_query (self, query))
        return TRUE;
      break;
    default:
      break;
  }

  return GST_VIDEO_DECODER_CLASS (parent_class)->src_query (decoder, query);
}

static GstFlowReturn
gst_qsv_decoder_handle_frame (GstVideoDecoder * decoder,
    GstVideoCodecFrame * frame)
{
  GstQsvDecoder *self = GST_QSV_DECODER (decoder);
  GstQsvDecoderClass *klass = GST_QSV_DECODER_GET_CLASS (self);
  GstQsvDecoderPrivate *priv = self->priv;
  mfxBitstream bs;
  GstMapInfo info;
  mfxStatus status;
  GstFlowReturn ret = GST_FLOW_ERROR;
  gboolean was_reconfigured = FALSE;
  GstBuffer *input_buf = nullptr;

  if (klass->process_input) {
    input_buf = klass->process_input (self, priv->decoder ? FALSE : TRUE,
        frame->input_buffer);
  } else {
    input_buf = gst_buffer_ref (frame->input_buffer);
  }

  if (!input_buf || !gst_buffer_map (input_buf, &info, GST_MAP_READ)) {
    GST_ERROR_OBJECT (self, "Failed to map input buffer");
    gst_clear_buffer (&input_buf);
    gst_video_decoder_release_frame (decoder, frame);
    return GST_FLOW_ERROR;
  }

  memset (&bs, 0, sizeof (mfxBitstream));

  bs.Data = (mfxU8 *) info.data;
  bs.DataLength = bs.MaxLength = (mfxU32) info.size;
  bs.TimeStamp = gst_qsv_timestamp_from_gst (frame->pts);

new_sequence:
  if (!priv->decoder) {
    if (klass->codec_id == MFX_CODEC_HEVC)
      priv->video_param.NumExtParam = 1;
    status = MFXVideoDECODE_DecodeHeader (priv->session,
        &bs, &priv->video_param);
    priv->video_param.NumExtParam = 0;

    if (status != MFX_ERR_NONE) {
      if (status == MFX_ERR_MORE_DATA) {
        GST_WARNING_OBJECT (self, "Need more date to parse header");
        ret = GST_FLOW_OK;
      } else {
        GST_ERROR_OBJECT (self, "Failed to parse header %d (%s)",
            QSV_STATUS_ARGS (status));
      }

      goto unmap_and_error;
    }

    if (!gst_video_decoder_negotiate (decoder)) {
      GST_ERROR_OBJECT (self, "Failed to negotiate");
      ret = GST_FLOW_NOT_NEGOTIATED;
      goto unmap_and_error;
    }

    if (!gst_qsv_decoder_init_session (self)) {
      GST_ERROR_OBJECT (self, "Failed to init session");
      return GST_FLOW_ERROR;
    }
  }

  if (!priv->decoder) {
    GST_ERROR_OBJECT (self, "Decoder object was not configured");
    ret = GST_FLOW_NOT_NEGOTIATED;
    goto unmap_and_error;
  }

  ret = gst_qsv_decoder_decode_frame (self, &bs, FALSE);

  switch (ret) {
    case GST_QSV_DECODER_FLOW_NEW_SEQUENCE:
      if (!was_reconfigured) {
        gst_qsv_decoder_drain_internal (self, FALSE);
        gst_qsv_decoder_reset (self);
        was_reconfigured = TRUE;

        goto new_sequence;
      }

      ret = GST_FLOW_ERROR;
      break;
    case GST_VIDEO_DECODER_FLOW_NEED_DATA:
      ret = GST_FLOW_OK;
      break;
    default:
      break;
  }

  gst_buffer_unmap (input_buf, &info);
  gst_buffer_unref (input_buf);
  gst_video_codec_frame_unref (frame);

  return ret;

unmap_and_error:
  gst_buffer_unmap (input_buf, &info);
  gst_buffer_unref (input_buf);
  gst_video_decoder_release_frame (decoder, frame);

  return ret;
}

static gboolean
gst_qsv_decoder_flush (GstVideoDecoder * decoder)
{
  GstQsvDecoder *self = GST_QSV_DECODER (decoder);

  GST_DEBUG_OBJECT (self, "Flush");

  gst_qsv_decoder_drain_internal (self, TRUE);

  return TRUE;
}

static GstFlowReturn
gst_qsv_decoder_finish (GstVideoDecoder * decoder)
{
  GstQsvDecoder *self = GST_QSV_DECODER (decoder);

  GST_DEBUG_OBJECT (self, "Finish");

  return gst_qsv_decoder_drain_internal (self, FALSE);
}

static GstFlowReturn
gst_qsv_decoder_drain (GstVideoDecoder * decoder)
{
  GstQsvDecoder *self = GST_QSV_DECODER (decoder);

  GST_DEBUG_OBJECT (self, "Drain");

  return gst_qsv_decoder_drain_internal (self, FALSE);
}
