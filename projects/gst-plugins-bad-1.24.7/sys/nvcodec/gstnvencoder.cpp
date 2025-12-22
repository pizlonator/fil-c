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

#include "gstnvencoder.h"

#include <gst/cuda/gstcudautils.h>
#include <gst/cuda/gstcudamemory.h>
#include <gst/cuda/gstcudabufferpool.h>
#include <gst/cuda/gstcudastream.h>
#include <gst/cuda/gstcuda-private.h>
#include <gst/base/gstbytewriter.h>
#include <string.h>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <memory>
#include <atomic>
#include "gstnvencobject.h"

#ifdef G_OS_WIN32
#include <wrl.h>

/* *INDENT-OFF* */
using namespace Microsoft::WRL;
/* *INDENT-ON* */
#endif

#ifdef HAVE_CUDA_GST_GL
#define SUPPORTED_GL_APIS GST_GL_API_OPENGL3
#endif

GST_DEBUG_CATEGORY (gst_nv_encoder_debug);
#define GST_CAT_DEFAULT gst_nv_encoder_debug

#define GST_NVENC_STATUS_FORMAT "s (%d)"
#define GST_NVENC_STATUS_ARGS(s) nvenc_status_to_string (s), s

enum
{
  PROP_0,
  PROP_CC_INSERT,
};

#define DEFAULT_CC_INSERT GST_NV_ENCODER_SEI_INSERT

struct _GstNvEncoderPrivate
{
  _GstNvEncoderPrivate ()
  {
    memset (&init_params, 0, sizeof (NV_ENC_INITIALIZE_PARAMS));
    memset (&config, 0, sizeof (NV_ENC_CONFIG));
  }

  GstCudaContext *context = nullptr;
  GstCudaStream *stream = nullptr;

#ifdef G_OS_WIN32
  GstD3D11Device *device = nullptr;
  GstD3D11Fence *fence = nullptr;
#endif

#ifdef HAVE_CUDA_GST_GL
  GstGLDisplay *gl_display = nullptr;
  GstGLContext *gl_context = nullptr;
  GstGLContext *other_gl_context = nullptr;

  gboolean gl_interop = FALSE;
#endif

  std::shared_ptr < GstNvEncObject > object;

  GstNvEncoderDeviceMode subclass_device_mode;
  GstNvEncoderDeviceMode selected_device_mode;
  gint64 dxgi_adapter_luid = 0;
  guint cuda_device_id = 0;

  NV_ENC_INITIALIZE_PARAMS init_params;
  NV_ENC_CONFIG config;

  GstVideoCodecState *input_state = nullptr;

  GstBufferPool *internal_pool = nullptr;

  GstClockTime dts_offset = 0;

  std::mutex lock;
  std::condition_variable cond;

  std::recursive_mutex context_lock;

  std::unique_ptr < std::thread > encoding_thread;

  std::atomic < GstFlowReturn > last_flow;

  /* properties */
  GstNvEncoderSeiInsertMode cc_insert = DEFAULT_CC_INSERT;
};

/**
 * GstNvEncoder:
 *
 * Since: 1.22
 */
#define gst_nv_encoder_parent_class parent_class
G_DEFINE_ABSTRACT_TYPE (GstNvEncoder, gst_nv_encoder, GST_TYPE_VIDEO_ENCODER);

static void gst_nv_encoder_finalize (GObject * object);
static void gst_nv_encoder_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_nv_encoder_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_nv_encoder_set_context (GstElement * element,
    GstContext * context);
static gboolean gst_nv_encoder_open (GstVideoEncoder * encoder);
static gboolean gst_nv_encoder_close (GstVideoEncoder * encoder);
static gboolean gst_nv_encoder_stop (GstVideoEncoder * encoder);
static gboolean gst_nv_encoder_sink_event (GstVideoEncoder * encoder,
    GstEvent * event);
static gboolean gst_nv_encoder_sink_query (GstVideoEncoder * encoder,
    GstQuery * query);
static gboolean gst_nv_encoder_src_query (GstVideoEncoder * encoder,
    GstQuery * query);
static gboolean gst_nv_encoder_propose_allocation (GstVideoEncoder *
    encoder, GstQuery * query);
static gboolean gst_nv_encoder_set_format (GstVideoEncoder * encoder,
    GstVideoCodecState * state);
static GstFlowReturn gst_nv_encoder_handle_frame (GstVideoEncoder *
    encoder, GstVideoCodecFrame * frame);
static GstFlowReturn gst_nv_encoder_finish (GstVideoEncoder * encoder);
static gboolean gst_nv_encoder_flush (GstVideoEncoder * encoder);
static gboolean gst_nv_encoder_transform_meta (GstVideoEncoder * encoder,
    GstVideoCodecFrame * frame, GstMeta * meta);

static void
gst_nv_encoder_class_init (GstNvEncoderClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstVideoEncoderClass *videoenc_class = GST_VIDEO_ENCODER_CLASS (klass);

  object_class->finalize = gst_nv_encoder_finalize;
  object_class->set_property = gst_nv_encoder_set_property;
  object_class->get_property = gst_nv_encoder_get_property;

  /**
   * GstNvEncoder:cc-insert:
   *
   * Closed Caption insert mode
   *
   * Since: 1.24
   */
  g_object_class_install_property (object_class, PROP_CC_INSERT,
      g_param_spec_enum ("cc-insert", "Closed Caption Insert",
          "Closed Caption Insert mode",
          GST_TYPE_NV_ENCODER_SEI_INSERT_MODE, DEFAULT_CC_INSERT,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  element_class->set_context = GST_DEBUG_FUNCPTR (gst_nv_encoder_set_context);

  videoenc_class->open = GST_DEBUG_FUNCPTR (gst_nv_encoder_open);
  videoenc_class->close = GST_DEBUG_FUNCPTR (gst_nv_encoder_close);
  videoenc_class->stop = GST_DEBUG_FUNCPTR (gst_nv_encoder_stop);
  videoenc_class->sink_event = GST_DEBUG_FUNCPTR (gst_nv_encoder_sink_event);
  videoenc_class->sink_query = GST_DEBUG_FUNCPTR (gst_nv_encoder_sink_query);
  videoenc_class->src_query = GST_DEBUG_FUNCPTR (gst_nv_encoder_src_query);
  videoenc_class->propose_allocation =
      GST_DEBUG_FUNCPTR (gst_nv_encoder_propose_allocation);
  videoenc_class->set_format = GST_DEBUG_FUNCPTR (gst_nv_encoder_set_format);
  videoenc_class->handle_frame =
      GST_DEBUG_FUNCPTR (gst_nv_encoder_handle_frame);
  videoenc_class->finish = GST_DEBUG_FUNCPTR (gst_nv_encoder_finish);
  videoenc_class->flush = GST_DEBUG_FUNCPTR (gst_nv_encoder_flush);
  videoenc_class->transform_meta =
      GST_DEBUG_FUNCPTR (gst_nv_encoder_transform_meta);

  GST_DEBUG_CATEGORY_INIT (gst_nv_encoder_debug, "nvencoder", 0, "nvencoder");

  gst_type_mark_as_plugin_api (GST_TYPE_NV_ENCODER, (GstPluginAPIFlags) 0);
  gst_type_mark_as_plugin_api (GST_TYPE_NV_ENCODER_PRESET,
      (GstPluginAPIFlags) 0);
  gst_type_mark_as_plugin_api (GST_TYPE_NV_ENCODER_RC_MODE,
      (GstPluginAPIFlags) 0);
  gst_type_mark_as_plugin_api (GST_TYPE_NV_ENCODER_SEI_INSERT_MODE,
      (GstPluginAPIFlags) 0);
  gst_type_mark_as_plugin_api (GST_TYPE_NV_ENCODER_MULTI_PASS,
      (GstPluginAPIFlags) 0);
  gst_type_mark_as_plugin_api (GST_TYPE_NV_ENCODER_TUNE, (GstPluginAPIFlags) 0);
}

static void
gst_nv_encoder_init (GstNvEncoder * self)
{
  self->priv = new GstNvEncoderPrivate ();

  gst_video_encoder_set_min_pts (GST_VIDEO_ENCODER (self),
      GST_SECOND * 60 * 60 * 1000);
  GST_PAD_SET_ACCEPT_INTERSECT (GST_VIDEO_ENCODER_SINK_PAD (self));
}

static void
gst_nv_encoder_finalize (GObject * object)
{
  GstNvEncoder *self = GST_NV_ENCODER (object);

  delete self->priv;

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_nv_encoder_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstNvEncoder *self = GST_NV_ENCODER (object);
  GstNvEncoderPrivate *priv = self->priv;

  switch (prop_id) {
    case PROP_CC_INSERT:
      priv->cc_insert = (GstNvEncoderSeiInsertMode) g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_nv_encoder_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstNvEncoder *self = GST_NV_ENCODER (object);
  GstNvEncoderPrivate *priv = self->priv;

  switch (prop_id) {
    case PROP_CC_INSERT:
      g_value_set_enum (value, priv->cc_insert);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_nv_encoder_set_context (GstElement * element, GstContext * context)
{
  GstNvEncoder *self = GST_NV_ENCODER (element);
  GstNvEncoderPrivate *priv = self->priv;

  std::unique_lock < std::recursive_mutex > lk (priv->context_lock);

  switch (priv->selected_device_mode) {
#ifdef G_OS_WIN32
    case GST_NV_ENCODER_DEVICE_D3D11:
      gst_d3d11_handle_set_context_for_adapter_luid (element,
          context, priv->dxgi_adapter_luid, &priv->device);
      break;
#endif
    case GST_NV_ENCODER_DEVICE_CUDA:
      gst_cuda_handle_set_context (element, context, priv->cuda_device_id,
          &priv->context);
#ifdef HAVE_CUDA_GST_GL
      if (gst_gl_handle_set_context (element, context, &priv->gl_display,
              &priv->other_gl_context)) {
        if (priv->gl_display)
          gst_gl_display_filter_gl_api (priv->gl_display, SUPPORTED_GL_APIS);
      }
#endif
      break;
    default:
      break;
  }

  lk.unlock ();

  GST_ELEMENT_CLASS (parent_class)->set_context (element, context);
}

static gboolean
gst_nv_encoder_reset (GstNvEncoder * self)
{
  GstNvEncoderPrivate *priv = self->priv;

  GST_LOG_OBJECT (self, "Reset");

  if (priv->internal_pool) {
    gst_buffer_pool_set_active (priv->internal_pool, FALSE);
    gst_clear_object (&priv->internal_pool);
  }

  if (priv->encoding_thread) {
    priv->encoding_thread->join ();
    priv->encoding_thread = nullptr;
  }

  priv->object = nullptr;
  priv->last_flow = GST_FLOW_OK;

  return TRUE;
}

static gboolean
gst_nv_encoder_device_lock (GstNvEncoder * self)
{
  GstNvEncoderPrivate *priv = self->priv;
  gboolean ret = TRUE;

  switch (priv->selected_device_mode) {
#ifdef G_OS_WIN32
    case GST_NV_ENCODER_DEVICE_D3D11:
      gst_d3d11_device_lock (priv->device);
      break;
#endif
    case GST_NV_ENCODER_DEVICE_CUDA:
      ret = gst_cuda_context_push (priv->context);
      break;
    default:
      break;
  }

  return ret;
}

static gboolean
gst_nv_encoder_device_unlock (GstNvEncoder * self)
{
  GstNvEncoderPrivate *priv = self->priv;
  gboolean ret = TRUE;

  switch (priv->selected_device_mode) {
#ifdef G_OS_WIN32
    case GST_NV_ENCODER_DEVICE_D3D11:
      gst_d3d11_device_unlock (priv->device);
      break;
#endif
    case GST_NV_ENCODER_DEVICE_CUDA:
      ret = gst_cuda_context_pop (nullptr);
      break;
    default:
      break;
  }

  return ret;
}

static gboolean
gst_nv_encoder_drain (GstNvEncoder * self, gboolean locked)
{
  GstNvEncoderPrivate *priv = self->priv;
  NVENCSTATUS status;
  GstNvEncTask *task = nullptr;

  if (!priv->object || !priv->encoding_thread)
    return TRUE;

  GST_DEBUG_OBJECT (self, "Drain");

  if (locked)
    GST_VIDEO_ENCODER_STREAM_UNLOCK (self);

  priv->object->AcquireTask (&task, true);

  status = priv->object->Drain (task);
  gst_nv_enc_result (status, self);

  priv->encoding_thread->join ();
  priv->encoding_thread = nullptr;

  gst_nv_encoder_reset (self);

  if (locked)
    GST_VIDEO_ENCODER_STREAM_LOCK (self);

  return TRUE;
}

#ifdef G_OS_WIN32
static gboolean
gst_nv_encoder_open_d3d11_device (GstNvEncoder * self)
{
  GstNvEncoderPrivate *priv = self->priv;
  ComPtr < ID3D10Multithread > multi_thread;
  ID3D11Device *device_handle;
  HRESULT hr;

  if (!gst_d3d11_ensure_element_data_for_adapter_luid (GST_ELEMENT (self),
          priv->dxgi_adapter_luid, &priv->device)) {
    GST_ERROR_OBJECT (self, "Cannot create d3d11device");
    return FALSE;
  }

  device_handle = gst_d3d11_device_get_device_handle (priv->device);
  hr = device_handle->QueryInterface (IID_PPV_ARGS (&multi_thread));
  if (!gst_d3d11_result (hr, priv->device)) {
    GST_ERROR_OBJECT (self, "ID3D10Multithread interface is unavailable");
    gst_clear_object (&priv->device);

    return FALSE;
  }

  multi_thread->SetMultithreadProtected (TRUE);

  return TRUE;
}
#endif

static gboolean
gst_nv_encoder_open (GstVideoEncoder * encoder)
{
  GstNvEncoder *self = GST_NV_ENCODER (encoder);
  GstNvEncoderPrivate *priv = self->priv;

  switch (priv->selected_device_mode) {
    case GST_NV_ENCODER_DEVICE_AUTO_SELECT:
      /* Will open GPU later */
      return TRUE;
#ifdef G_OS_WIN32
    case GST_NV_ENCODER_DEVICE_D3D11:
      return gst_nv_encoder_open_d3d11_device (self);
#endif
    case GST_NV_ENCODER_DEVICE_CUDA:
      if (!gst_cuda_ensure_element_context (GST_ELEMENT_CAST (encoder),
              priv->cuda_device_id, &priv->context)) {
        GST_ERROR_OBJECT (self, "failed to create CUDA context");
        return FALSE;
      }
      if (!priv->stream && gst_nvenc_have_set_io_cuda_streams ())
        priv->stream = gst_cuda_stream_new (priv->context);
      break;
    default:
      g_assert_not_reached ();
      return FALSE;
  }

  return TRUE;
}

static gboolean
gst_nv_encoder_close (GstVideoEncoder * encoder)
{
  GstNvEncoder *self = GST_NV_ENCODER (encoder);
  GstNvEncoderPrivate *priv = self->priv;

  gst_clear_cuda_stream (&priv->stream);
  gst_clear_object (&priv->context);
#ifdef G_OS_WIN32
  gst_clear_d3d11_fence (&priv->fence);
  gst_clear_object (&priv->device);
#endif
#ifdef HAVE_CUDA_GST_GL
  gst_clear_object (&priv->gl_display);
  gst_clear_object (&priv->gl_context);
  gst_clear_object (&priv->other_gl_context);
#endif

  return TRUE;
}

static gboolean
gst_nv_encoder_stop (GstVideoEncoder * encoder)
{
  GstNvEncoder *self = GST_NV_ENCODER (encoder);
  GstNvEncoderPrivate *priv = self->priv;

  GST_DEBUG_OBJECT (self, "Stop");

  gst_nv_encoder_drain (self, FALSE);

  if (priv->subclass_device_mode == GST_NV_ENCODER_DEVICE_AUTO_SELECT) {
    gst_clear_cuda_stream (&priv->stream);
    gst_clear_object (&priv->context);
#ifdef G_OS_WIN32
    gst_clear_object (&priv->device);
#endif
    priv->selected_device_mode = GST_NV_ENCODER_DEVICE_AUTO_SELECT;
  }

  g_clear_pointer (&priv->input_state, gst_video_codec_state_unref);

  return TRUE;
}

static gboolean
gst_nv_encoder_sink_event (GstVideoEncoder * encoder, GstEvent * event)
{
  GstNvEncoder *self = GST_NV_ENCODER (encoder);
  GstNvEncoderPrivate *priv = self->priv;

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_FLUSH_START:
      if (priv->object)
        priv->object->SetFlushing (TRUE);
      break;
    default:
      break;
  }

  return GST_VIDEO_ENCODER_CLASS (parent_class)->sink_event (encoder, event);
}

#ifdef HAVE_CUDA_GST_GL
static void
gst_nv_encoder_check_cuda_device_from_gl_context (GstGLContext * context,
    gboolean * ret)
{
  guint device_count = 0;
  CUdevice device_list[1] = { 0, };
  CUresult cuda_ret;

  *ret = FALSE;

  cuda_ret = CuGLGetDevices (&device_count,
      device_list, 1, CU_GL_DEVICE_LIST_ALL);

  if (!gst_cuda_result (cuda_ret) || device_count == 0)
    return;

  *ret = TRUE;
}

static gboolean
gst_nv_encoder_ensure_gl_context (GstNvEncoder * self)
{
  GstNvEncoderPrivate *priv = self->priv;
  gboolean ret = FALSE;

  std::unique_lock < std::recursive_mutex > lk (priv->context_lock);

  if (!gst_gl_ensure_element_data (GST_ELEMENT (self), &priv->gl_display,
          &priv->other_gl_context)) {
    GST_DEBUG_OBJECT (self, "Couldn't get GL display");
    return FALSE;
  }

  gst_gl_display_filter_gl_api (priv->gl_display, SUPPORTED_GL_APIS);

  if (!gst_gl_display_ensure_context (priv->gl_display, priv->other_gl_context,
          &priv->gl_context, nullptr)) {
    GST_DEBUG_OBJECT (self, "Couldn't get GL context");
    return FALSE;
  }

  gst_gl_context_thread_add (priv->gl_context,
      (GstGLContextThreadFunc) gst_nv_encoder_check_cuda_device_from_gl_context,
      &ret);

  return ret;
}
#endif

static gboolean
gst_nv_encoder_handle_context_query (GstNvEncoder * self, GstQuery * query)
{
  GstNvEncoderPrivate *priv = self->priv;
  gboolean ret = FALSE;

  std::unique_lock < std::recursive_mutex > lk (priv->context_lock);

  switch (priv->selected_device_mode) {
#ifdef G_OS_WIN32
    case GST_NV_ENCODER_DEVICE_D3D11:
      ret = gst_d3d11_handle_context_query (GST_ELEMENT (self),
          query, priv->device);
      break;
#endif
    case GST_NV_ENCODER_DEVICE_CUDA:
#ifdef HAVE_CUDA_GST_GL
    {
      GstGLDisplay *display = nullptr;
      GstGLContext *other = nullptr;
      GstGLContext *local = nullptr;

      if (priv->gl_display)
        display = (GstGLDisplay *) gst_object_ref (priv->gl_display);
      if (priv->gl_context)
        local = (GstGLContext *) gst_object_ref (priv->gl_context);
      if (priv->other_gl_context)
        other = (GstGLContext *) gst_object_ref (priv->other_gl_context);

      lk.unlock ();
      ret = gst_gl_handle_context_query (GST_ELEMENT (self), query,
          display, local, other);
      lk.lock ();

      gst_clear_object (&display);
      gst_clear_object (&other);
      gst_clear_object (&local);

      if (ret)
        return ret;
    }
#endif
      ret = gst_cuda_handle_context_query (GST_ELEMENT (self),
          query, priv->context);
      break;
    default:
      break;
  }

  return ret;
}

static gboolean
gst_nv_encoder_sink_query (GstVideoEncoder * encoder, GstQuery * query)
{
  GstNvEncoder *self = GST_NV_ENCODER (encoder);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CONTEXT:
      if (gst_nv_encoder_handle_context_query (self, query))
        return TRUE;
      break;
    default:
      break;
  }

  return GST_VIDEO_ENCODER_CLASS (parent_class)->sink_query (encoder, query);
}

static gboolean
gst_nv_encoder_src_query (GstVideoEncoder * encoder, GstQuery * query)
{
  GstNvEncoder *self = GST_NV_ENCODER (encoder);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CONTEXT:
      if (gst_nv_encoder_handle_context_query (self, query))
        return TRUE;
      break;
    default:
      break;
  }

  return GST_VIDEO_ENCODER_CLASS (parent_class)->src_query (encoder, query);
}

static guint
gst_nv_encoder_get_task_size (GstNvEncoder * self)
{
  std::shared_ptr < GstNvEncObject > object = self->priv->object;

  if (!object)
    return 0;

  return object->GetTaskSize ();
}

static gboolean
gst_nv_encoder_propose_allocation (GstVideoEncoder * encoder, GstQuery * query)
{
  GstNvEncoder *self = GST_NV_ENCODER (encoder);
  GstNvEncoderPrivate *priv = self->priv;
  GstVideoInfo info;
  GstBufferPool *pool = NULL;
  GstCaps *caps;
  guint size;
  GstStructure *config;
  GstCapsFeatures *features;
  guint min_buffers;
  gboolean use_cuda_pool = FALSE;

  gst_query_parse_allocation (query, &caps, NULL);
  if (!caps) {
    GST_WARNING_OBJECT (self, "null caps in query");
    return FALSE;
  }

  if (!gst_video_info_from_caps (&info, caps)) {
    GST_WARNING_OBJECT (self, "Failed to convert caps into info");
    return FALSE;
  }

  features = gst_caps_get_features (caps, 0);
  min_buffers = gst_nv_encoder_get_task_size (self);
  if (min_buffers == 0) {
    GstNvEncoderClass *klass = GST_NV_ENCODER_GET_CLASS (self);

    min_buffers = klass->calculate_min_buffers (self);
  }

  switch (priv->subclass_device_mode) {
    case GST_NV_ENCODER_DEVICE_AUTO_SELECT:
      /* Use upstream pool in case of auto select mode. We don't know which
       * GPU to use at this moment */
      gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, nullptr);
      gst_query_add_allocation_pool (query, nullptr, info.size, min_buffers, 0);
      return TRUE;
#ifdef G_OS_WIN32
    case GST_NV_ENCODER_DEVICE_D3D11:
      if (features && gst_caps_features_contains (features,
              GST_CAPS_FEATURE_MEMORY_D3D11_MEMORY)) {
        GST_DEBUG_OBJECT (self, "upstream support d3d11 memory");
        pool = gst_d3d11_buffer_pool_new (priv->device);
      }
      break;
#endif
    case GST_NV_ENCODER_DEVICE_CUDA:
#ifdef HAVE_CUDA_GST_GL
      if (features && gst_caps_features_contains (features,
              GST_CAPS_FEATURE_MEMORY_GL_MEMORY)) {
        GST_DEBUG_OBJECT (self, "upstream support GL memory");
        if (!gst_nv_encoder_ensure_gl_context (self)) {
          GST_WARNING_OBJECT (self, "Couldn't get GL context");
          priv->gl_interop = FALSE;
          gst_query_add_allocation_meta (query,
              GST_VIDEO_META_API_TYPE, nullptr);
          gst_query_add_allocation_pool (query,
              nullptr, info.size, min_buffers, 0);
          return TRUE;
        }

        pool = gst_gl_buffer_pool_new (priv->gl_context);
        break;
      }
#endif

      if (features && gst_caps_features_contains (features,
              GST_CAPS_FEATURE_MEMORY_CUDA_MEMORY)) {
        GST_DEBUG_OBJECT (self, "upstream support CUDA memory");
        pool = gst_cuda_buffer_pool_new (priv->context);
        use_cuda_pool = TRUE;
      }
      break;
    default:
      g_assert_not_reached ();
      return FALSE;
  }

  if (!pool)
    pool = gst_video_buffer_pool_new ();

  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_add_option (config, GST_BUFFER_POOL_OPTION_VIDEO_META);

  size = GST_VIDEO_INFO_SIZE (&info);
  gst_buffer_pool_config_set_params (config, caps, size, min_buffers, 0);
  if (use_cuda_pool && priv->stream) {
    /* Set our stream on buffer pool config so that CUstream can be shared */
    gst_buffer_pool_config_set_cuda_stream (config, priv->stream);
  }

  if (!gst_buffer_pool_set_config (pool, config)) {
    GST_WARNING_OBJECT (self, "Failed to set pool config");
    gst_object_unref (pool);
    return FALSE;
  }

  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_get_params (config, NULL, &size, NULL, NULL);
  gst_structure_free (config);

  gst_query_add_allocation_pool (query, pool, size, min_buffers, 0);
  gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, NULL);
  gst_object_unref (pool);

  return TRUE;
}

static NV_ENC_PIC_STRUCT
gst_nv_encoder_get_pic_struct (GstNvEncoder * self, GstBuffer * buffer)
{
  GstNvEncoderPrivate *priv = self->priv;
  GstVideoInfo *info = &priv->input_state->info;

  if (!GST_VIDEO_INFO_IS_INTERLACED (info))
    return NV_ENC_PIC_STRUCT_FRAME;

  if (GST_VIDEO_INFO_INTERLACE_MODE (info) == GST_VIDEO_INTERLACE_MODE_MIXED) {
    if (!GST_BUFFER_FLAG_IS_SET (buffer, GST_VIDEO_BUFFER_FLAG_INTERLACED)) {
      return NV_ENC_PIC_STRUCT_FRAME;
    }

    if (GST_BUFFER_FLAG_IS_SET (buffer, GST_VIDEO_BUFFER_FLAG_TFF))
      return NV_ENC_PIC_STRUCT_FIELD_TOP_BOTTOM;

    return NV_ENC_PIC_STRUCT_FIELD_BOTTOM_TOP;
  }

  switch (GST_VIDEO_INFO_FIELD_ORDER (info)) {
    case GST_VIDEO_FIELD_ORDER_TOP_FIELD_FIRST:
      return NV_ENC_PIC_STRUCT_FIELD_TOP_BOTTOM;
      break;
    case GST_VIDEO_FIELD_ORDER_BOTTOM_FIELD_FIRST:
      return NV_ENC_PIC_STRUCT_FIELD_BOTTOM_TOP;
      break;
    default:
      break;
  }

  if (GST_BUFFER_FLAG_IS_SET (buffer, GST_VIDEO_BUFFER_FLAG_TFF))
    return NV_ENC_PIC_STRUCT_FIELD_TOP_BOTTOM;

  return NV_ENC_PIC_STRUCT_FIELD_BOTTOM_TOP;
}

static GstVideoCodecFrame *
gst_nv_encoder_find_output_frame (GstVideoEncoder * self, GstNvEncTask * task)
{
  GList *frames, *iter;
  GstVideoCodecFrame *ret = NULL;

  frames = gst_video_encoder_get_frames (self);

  for (iter = frames; iter; iter = g_list_next (iter)) {
    GstVideoCodecFrame *frame = (GstVideoCodecFrame *) iter->data;
    GstNvEncTask *other = (GstNvEncTask *)
        gst_video_codec_frame_get_user_data (frame);

    if (!other)
      continue;

    if (other == task) {
      ret = frame;
      break;
    }
  }

  if (ret)
    gst_video_codec_frame_ref (ret);

  if (frames)
    g_list_free_full (frames, (GDestroyNotify) gst_video_codec_frame_unref);

  return ret;
}

static void
gst_nv_encoder_thread_func (GstNvEncoder * self)
{
  GstVideoEncoder *encoder = GST_VIDEO_ENCODER (self);
  GstNvEncoderClass *klass = GST_NV_ENCODER_GET_CLASS (self);
  GstNvEncoderPrivate *priv = self->priv;
  std::shared_ptr < GstNvEncObject > object = priv->object;

  GST_INFO_OBJECT (self, "Entering encoding loop");

  do {
    GstFlowReturn ret;
    GstNvEncTask *task = nullptr;
    GstVideoCodecFrame *frame;
    NV_ENC_LOCK_BITSTREAM bitstream;
    NVENCSTATUS status;

    ret = object->GetOutput (&task);
    if (ret == GST_FLOW_EOS) {
      g_assert (!task);
      GST_INFO_OBJECT (self, "Got EOS task");
      break;
    }

    frame = gst_nv_encoder_find_output_frame (encoder, task);
    if (!frame) {
      gst_nv_enc_task_unref (task);
      GST_ELEMENT_ERROR (self, STREAM, ENCODE, (NULL),
          ("Failed to find associated codec frame"));
      priv->last_flow = GST_FLOW_ERROR;
      continue;
    }

    status = gst_nv_enc_task_lock_bitstream (task, &bitstream);
    if (status != NV_ENC_SUCCESS) {
      gst_nv_enc_task_unref (task);
      gst_video_encoder_finish_frame (encoder, frame);
      GST_ELEMENT_ERROR (self, STREAM, ENCODE, (NULL),
          ("Failed to lock bitstream, status: %" GST_NVENC_STATUS_FORMAT,
              GST_NVENC_STATUS_ARGS (status)));
      priv->last_flow = GST_FLOW_ERROR;
      continue;
    }

    if (priv->last_flow != GST_FLOW_OK) {
      gst_nv_enc_task_unlock_bitstream (task);
      gst_nv_enc_task_unref (task);
      continue;
    }

    if (klass->create_output_buffer) {
      frame->output_buffer = klass->create_output_buffer (self, &bitstream);
    } else {
      frame->output_buffer =
          gst_buffer_new_memdup (bitstream.bitstreamBufferPtr,
          bitstream.bitstreamSizeInBytes);
    }

    GST_BUFFER_FLAG_SET (frame->output_buffer, GST_BUFFER_FLAG_MARKER);

    if (bitstream.pictureType == NV_ENC_PIC_TYPE_IDR)
      GST_VIDEO_CODEC_FRAME_SET_SYNC_POINT (frame);

    frame->dts = frame->pts - priv->dts_offset;
    frame->pts = bitstream.outputTimeStamp;
    frame->duration = bitstream.outputDuration;

    gst_nv_enc_task_unlock_bitstream (task);
    gst_nv_enc_task_unref (task);

    priv->last_flow = gst_video_encoder_finish_frame (encoder, frame);
    if (priv->last_flow != GST_FLOW_OK) {
      GST_INFO_OBJECT (self,
          "Finish frame returned %s", gst_flow_get_name (priv->last_flow));
    }
  } while (true);

  GST_INFO_OBJECT (self, "Exiting thread");
}

static guint
gst_nv_encoder_calculate_task_pool_size (GstNvEncoder * self,
    NV_ENC_CONFIG * config)
{
  guint num_tasks;

  /* At least 4 surfaces are required as documented by Nvidia Encoder guide */
  num_tasks = 4;

  /* lookahead depth */
  num_tasks += config->rcParams.lookaheadDepth;

  /* B frames + 1 */
  num_tasks += MAX (0, config->frameIntervalP - 1) + 1;

  GST_DEBUG_OBJECT (self, "Calculated task pool size: %d "
      "(lookahead %d, frameIntervalP %d)",
      num_tasks, config->rcParams.lookaheadDepth, config->frameIntervalP);

  return num_tasks;
}

static gboolean
gst_nv_encoder_open_encode_session (GstNvEncoder * self)
{
  GstNvEncoderPrivate *priv = self->priv;
  NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS session_params = { 0, };
  session_params.version =
      gst_nvenc_get_open_encode_session_ex_params_version ();
  session_params.apiVersion = gst_nvenc_get_api_version ();
  GstObject *device = (GstObject *) priv->context;

  switch (priv->selected_device_mode) {
#ifdef G_OS_WIN32
    case GST_NV_ENCODER_DEVICE_D3D11:
      session_params.deviceType = NV_ENC_DEVICE_TYPE_DIRECTX;
      session_params.device = gst_d3d11_device_get_device_handle (priv->device);
      device = (GstObject *) priv->device;
      break;
#endif
    case GST_NV_ENCODER_DEVICE_CUDA:
      session_params.deviceType = NV_ENC_DEVICE_TYPE_CUDA;
      session_params.device = gst_cuda_context_get_handle (priv->context);
      break;
    default:
      g_assert_not_reached ();
      return FALSE;
  }

  priv->object = GstNvEncObject::CreateInstance (GST_ELEMENT_CAST (self),
      device, &session_params);

  if (!priv->object) {
    GST_ERROR_OBJECT (self, "Couldn't create encoder session");
    return FALSE;
  }

  return TRUE;
}

#ifdef G_OS_WIN32
static GstBufferPool *
gst_nv_encoder_create_d3d11_pool (GstNvEncoder * self,
    GstVideoCodecState * state)
{
  GstNvEncoderPrivate *priv = self->priv;
  GstStructure *config;
  GstBufferPool *pool = NULL;
  GstD3D11AllocationParams *params;

  params = gst_d3d11_allocation_params_new (priv->device, &state->info,
      GST_D3D11_ALLOCATION_FLAG_DEFAULT, 0, D3D11_RESOURCE_MISC_SHARED);

  pool = gst_d3d11_buffer_pool_new (priv->device);

  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_set_d3d11_allocation_params (config, params);
  gst_d3d11_allocation_params_free (params);

  gst_buffer_pool_config_set_params (config, state->caps,
      GST_VIDEO_INFO_SIZE (&state->info), 0, 0);
  if (!gst_buffer_pool_set_config (pool, config)) {
    GST_ERROR_OBJECT (self, "Failed to set pool config");
    gst_object_unref (pool);

    return NULL;
  }

  if (!gst_buffer_pool_set_active (pool, TRUE)) {
    GST_ERROR_OBJECT (self, "Failed to set active");
    gst_object_unref (pool);
    return NULL;
  }

  return pool;
}
#endif

static GstBufferPool *
gst_nv_encoder_create_pool (GstNvEncoder * self, GstVideoCodecState * state)
{
  GstNvEncoderPrivate *priv = self->priv;
  GstStructure *config;
  GstBufferPool *pool = NULL;

  /* At this moment device type must be selected already */
  switch (priv->selected_device_mode) {
#ifdef G_OS_WIN32
    case GST_NV_ENCODER_DEVICE_D3D11:
      return gst_nv_encoder_create_d3d11_pool (self, state);
#endif
    case GST_NV_ENCODER_DEVICE_CUDA:
      pool = gst_cuda_buffer_pool_new (priv->context);
      break;
    default:
      g_assert_not_reached ();
      return FALSE;
  }

  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_set_params (config, state->caps,
      GST_VIDEO_INFO_SIZE (&state->info), 0, 0);
  if (priv->selected_device_mode == GST_NV_ENCODER_DEVICE_CUDA && priv->stream)
    gst_buffer_pool_config_set_cuda_stream (config, priv->stream);

  if (!gst_buffer_pool_set_config (pool, config)) {
    GST_ERROR_OBJECT (self, "Failed to set pool config");
    gst_object_unref (pool);

    return NULL;
  }

  if (!gst_buffer_pool_set_active (pool, TRUE)) {
    GST_ERROR_OBJECT (self, "Failed to set active");
    gst_object_unref (pool);
    return NULL;
  }

  return pool;
}

static gboolean
gst_nv_encoder_init_session (GstNvEncoder * self, GstBuffer * in_buf)
{
  GstNvEncoderPrivate *priv = self->priv;
  GstNvEncoderClass *klass = GST_NV_ENCODER_GET_CLASS (self);
  GstVideoCodecState *state = priv->input_state;
  GstVideoInfo *info = &state->info;
  NVENCSTATUS status;
  guint task_pool_size;
  gint fps_n, fps_d;
  GstClockTime frame_duration, min_latency, max_latency;

  gst_nv_encoder_reset (self);

  memset (&priv->init_params, 0, sizeof (NV_ENC_INITIALIZE_PARAMS));
  memset (&priv->config, 0, sizeof (NV_ENC_CONFIG));

  if (priv->selected_device_mode == GST_NV_ENCODER_DEVICE_AUTO_SELECT) {
    GstNvEncoderDeviceData data;
    gboolean ret;

    if (!in_buf) {
      GST_DEBUG_OBJECT (self, "Unknown device mode, open session later");
      return TRUE;
    }

    if (!klass->select_device (self, info, in_buf, &data)) {
      GST_ELEMENT_ERROR (self, STREAM, ENCODE, (NULL),
          ("Failed to select device mode"));
      return FALSE;
    }

    GST_DEBUG_OBJECT (self,
        "Selected device mode: %d, cuda-device-id: %d, adapter-luid %"
        G_GINT64_FORMAT, data.device_mode, data.cuda_device_id,
        data.adapter_luid);

    g_assert (data.device_mode == GST_NV_ENCODER_DEVICE_CUDA ||
        data.device_mode == GST_NV_ENCODER_DEVICE_D3D11);

    std::lock_guard < std::recursive_mutex > clk (priv->context_lock);
    priv->selected_device_mode = data.device_mode;
    priv->cuda_device_id = data.cuda_device_id;
    priv->dxgi_adapter_luid = data.adapter_luid;
    gst_clear_object (&priv->context);
    if (data.device_mode == GST_NV_ENCODER_DEVICE_CUDA) {
      GstMemory *mem = gst_buffer_peek_memory (in_buf, 0);

      priv->context = (GstCudaContext *) data.device;
      gst_clear_cuda_stream (&priv->stream);

      if (gst_nvenc_have_set_io_cuda_streams ()) {
        if (gst_is_cuda_memory (mem)) {
          /* Use upstream CUDA stream */
          priv->stream =
              gst_cuda_memory_get_stream (GST_CUDA_MEMORY_CAST (mem));
          if (priv->stream)
            gst_cuda_stream_ref (priv->stream);
        }
      }
    }
#ifdef G_OS_WIN32
    gst_clear_object (&priv->device);
    if (data.device_mode == GST_NV_ENCODER_DEVICE_D3D11)
      priv->device = (GstD3D11Device *) data.device;
#endif

    ret = gst_nv_encoder_open (GST_VIDEO_ENCODER (self));

    if (!ret) {
      GST_ELEMENT_ERROR (self, STREAM, ENCODE, (NULL),
          ("Failed to open device"));
      return FALSE;
    }
  }

  priv->internal_pool = gst_nv_encoder_create_pool (self, state);
  if (!priv->internal_pool) {
    GST_ELEMENT_ERROR (self, STREAM, ENCODE, (NULL),
        ("Failed to create internal pool"));
    return FALSE;
  }

  if (!gst_nv_encoder_device_lock (self)) {
    GST_ELEMENT_ERROR (self, STREAM, ENCODE, (NULL), ("Failed to lock device"));
    gst_nv_encoder_reset (self);
    return FALSE;
  }

  if (!gst_nv_encoder_open_encode_session (self)) {
    GST_ELEMENT_ERROR (self, STREAM, ENCODE, (NULL),
        ("Failed to open session"));
    goto error;
  }

  if (!klass->set_format (self, state, priv->object->GetHandle (),
          &priv->init_params, &priv->config)) {
    GST_ELEMENT_ERROR (self, STREAM, ENCODE, (NULL), ("Failed to set format"));
    goto error;
  }

  task_pool_size = gst_nv_encoder_calculate_task_pool_size (self,
      &priv->config);

  priv->init_params.encodeConfig = &priv->config;
  status = priv->object->InitSession (&priv->init_params,
      priv->stream, &priv->input_state->info, task_pool_size);
  if (!gst_nv_enc_result (status, self)) {
    GST_ELEMENT_ERROR (self, STREAM, ENCODE, (NULL),
        ("Failed to init encoder, status: %"
            GST_NVENC_STATUS_FORMAT, GST_NVENC_STATUS_ARGS (status)));
    goto error;
  }

  gst_nv_encoder_device_unlock (self);

  if (!klass->set_output_state (self, priv->input_state,
          priv->object->GetHandle ())) {
    GST_ELEMENT_ERROR (self, STREAM, ENCODE, (NULL),
        ("Failed to set output state"));
    gst_nv_encoder_reset (self);
    return FALSE;
  }

  priv->encoding_thread = std::make_unique < std::thread >
      (gst_nv_encoder_thread_func, self);

  if (info->fps_n > 0 && info->fps_d > 0) {
    fps_n = info->fps_n;
    fps_d = info->fps_d;
  } else {
    fps_n = 25;
    fps_d = 1;
  }

  frame_duration = gst_util_uint64_scale (GST_SECOND, fps_d, fps_n);

  priv->dts_offset = 0;
  /* Calculate DTS offset for B frame. NVENC does not provide DTS */
  if (priv->config.frameIntervalP > 1)
    priv->dts_offset = frame_duration * (priv->config.frameIntervalP - 1);

  min_latency = priv->dts_offset +
      priv->config.rcParams.lookaheadDepth * frame_duration;
  max_latency = frame_duration * task_pool_size;
  gst_video_encoder_set_latency (GST_VIDEO_ENCODER (self),
      min_latency, max_latency);

  return TRUE;

error:
  gst_nv_encoder_device_unlock (self);

  gst_nv_encoder_reset (self);

  return FALSE;
}

static gboolean
gst_nv_encoder_reconfigure_session (GstNvEncoder * self)
{
  GstNvEncoderPrivate *priv = self->priv;
  NV_ENC_RECONFIGURE_PARAMS params = { 0, };
  NVENCSTATUS status;

  if (!priv->object) {
    GST_WARNING_OBJECT (self,
        "Encoding session was not configured, open session");
    gst_nv_encoder_drain (self, TRUE);

    return gst_nv_encoder_init_session (self, nullptr);
  }

  params.version = gst_nvenc_get_reconfigure_params_version ();
  params.reInitEncodeParams = priv->init_params;
  params.reInitEncodeParams.encodeConfig = &priv->config;

  status = priv->object->Reconfigure (&params);
  if (!gst_nv_enc_result (status, self)) {
    gst_nv_encoder_drain (self, TRUE);

    return gst_nv_encoder_init_session (self, nullptr);
  }

  return TRUE;
}

static gboolean
gst_nv_encoder_set_format (GstVideoEncoder * encoder,
    GstVideoCodecState * state)
{
  GstNvEncoder *self = GST_NV_ENCODER (encoder);
  GstNvEncoderPrivate *priv = self->priv;

  gst_nv_encoder_drain (self, TRUE);

  g_clear_pointer (&priv->input_state, gst_video_codec_state_unref);
  priv->input_state = gst_video_codec_state_ref (state);
  priv->last_flow = GST_FLOW_OK;

#ifdef HAVE_CUDA_GST_GL
  {
    GstCapsFeatures *features = gst_caps_get_features (state->caps, 0);
    if (gst_caps_features_contains (features,
            GST_CAPS_FEATURE_MEMORY_GL_MEMORY)) {
      priv->gl_interop = TRUE;
    } else {
      priv->gl_interop = FALSE;
    }
  }
#endif

  /* select device again on next buffer */
  if (priv->subclass_device_mode == GST_NV_ENCODER_DEVICE_AUTO_SELECT)
    priv->selected_device_mode = GST_NV_ENCODER_DEVICE_AUTO_SELECT;

  return gst_nv_encoder_init_session (self, nullptr);
}

static GstFlowReturn
gst_nv_encoder_copy_system (GstNvEncoder * self, const GstVideoInfo * info,
    GstBuffer * buffer, GstNvEncTask * task)
{
  std::shared_ptr < GstNvEncObject > object = self->priv->object;
  NVENCSTATUS status;
  GstVideoFrame frame;
  guint8 *dst_data;
  guint32 pitch;
  GstNvEncBuffer *inbuf = nullptr;

  if (!gst_video_frame_map (&frame, info, buffer, GST_MAP_READ)) {
    GST_ERROR_OBJECT (self, "Failed to map buffer");
    return GST_FLOW_ERROR;
  }

  status = object->AcquireBuffer (&inbuf);
  if (!gst_nv_enc_result (status, self)) {
    gst_video_frame_unmap (&frame);
    return GST_FLOW_ERROR;
  }

  status = gst_nv_enc_buffer_lock (inbuf, (gpointer *) & dst_data, &pitch);
  if (!gst_nv_enc_result (status, self)) {
    gst_video_frame_unmap (&frame);
    gst_nv_enc_buffer_unref (inbuf);
    return GST_FLOW_ERROR;
  }

  for (guint i = 0; i < GST_VIDEO_FRAME_N_PLANES (&frame); i++) {
    guint8 *src_data = (guint8 *) GST_VIDEO_FRAME_PLANE_DATA (&frame, i);
    guint width_in_bytes = GST_VIDEO_FRAME_COMP_WIDTH (&frame, i) *
        GST_VIDEO_FRAME_COMP_PSTRIDE (&frame, i);
    guint stride = GST_VIDEO_FRAME_PLANE_STRIDE (&frame, i);
    guint height = GST_VIDEO_FRAME_COMP_HEIGHT (&frame, i);

    for (guint j = 0; j < height; j++) {
      memcpy (dst_data, src_data, width_in_bytes);
      dst_data += pitch;
      src_data += stride;
    }
  }

  gst_nv_enc_buffer_unlock (inbuf);
  gst_video_frame_unmap (&frame);

  gst_nv_enc_task_set_buffer (task, inbuf);

  return GST_FLOW_OK;
}

#ifdef HAVE_CUDA_GST_GL
static GstCudaGraphicsResource *
gst_nv_encoder_ensure_gl_cuda_resource (GstNvEncoder * self, GstMemory * mem)
{
  GQuark quark;
  GstNvEncoderPrivate *priv = self->priv;
  GstCudaGraphicsResource *resource;

  if (!gst_is_gl_memory_pbo (mem)) {
    GST_WARNING_OBJECT (self, "memory is not GL PBO memory, %s",
        mem->allocator->mem_type);
    return nullptr;
  }

  quark = gst_cuda_quark_from_id (GST_CUDA_QUARK_GRAPHICS_RESOURCE);
  resource = (GstCudaGraphicsResource *)
      gst_mini_object_get_qdata (GST_MINI_OBJECT (mem), quark);

  if (!resource) {
    GstMapInfo map_info;
    GstGLMemoryPBO *pbo = (GstGLMemoryPBO *) mem;
    GstGLBuffer *gl_buf = pbo->pbo;
    gboolean ret;

    if (!gst_memory_map (mem, &map_info,
            (GstMapFlags) (GST_MAP_READ | GST_MAP_GL))) {
      GST_ERROR_OBJECT (self, "Couldn't map gl memory");
      return nullptr;
    }

    resource = gst_cuda_graphics_resource_new (priv->context,
        GST_OBJECT (GST_GL_BASE_MEMORY_CAST (mem)->context),
        GST_CUDA_GRAPHICS_RESOURCE_GL_BUFFER);

    GST_LOG_OBJECT (self, "registering gl buffer %d to CUDA", gl_buf->id);
    ret = gst_cuda_graphics_resource_register_gl_buffer (resource, gl_buf->id,
        CU_GRAPHICS_REGISTER_FLAGS_NONE);
    gst_memory_unmap (mem, &map_info);

    if (!ret) {
      GST_ERROR_OBJECT (self, "Couldn't register gl buffer %d", gl_buf->id);
      gst_cuda_graphics_resource_free (resource);
      return nullptr;
    }

    gst_mini_object_set_qdata (GST_MINI_OBJECT (mem), quark, resource,
        (GDestroyNotify) gst_cuda_graphics_resource_free);
  }

  return resource;
}

struct GstNvEncGLInteropData
{
  GstNvEncoder *self = nullptr;
  GstBuffer *in_buf = nullptr;
  GstBuffer *out_buf = nullptr;
};

static void
gst_nv_encoder_upload_gl (GstGLContext * context, GstNvEncGLInteropData * data)
{
  GstNvEncoder *self = data->self;
  GstNvEncoderPrivate *priv = self->priv;
  CUDA_MEMCPY2D copy_param;
  GstCudaGraphicsResource *gst_res[GST_VIDEO_MAX_PLANES] = { nullptr, };
  CUgraphicsResource cuda_res[GST_VIDEO_MAX_PLANES] = { nullptr, };
  CUdeviceptr src_devptr[GST_VIDEO_MAX_PLANES] = { 0, };
  const GstVideoInfo *info = &priv->input_state->info;
  CUstream stream = gst_cuda_stream_get_handle (priv->stream);
  GstCudaMemory *cmem;
  GstMapInfo map_info;
  CUresult cuda_ret;
  gboolean ret = FALSE;

  gst_cuda_context_push (priv->context);

  for (guint i = 0; i < GST_VIDEO_INFO_N_PLANES (info); i++) {
    GstMemory *mem = gst_buffer_peek_memory (data->in_buf, i);
    GstGLMemoryPBO *pbo = (GstGLMemoryPBO *) mem;
    gsize src_size;

    if (!gst_is_gl_memory_pbo (mem)) {
      GST_ERROR_OBJECT (self, "Not a GL PBO memory");
      goto out;
    }

    gst_res[i] = gst_nv_encoder_ensure_gl_cuda_resource (self, mem);
    if (!gst_res[i]) {
      GST_ERROR_OBJECT (self, "Couldn't get resource %d", i);
      goto out;
    }

    gst_gl_memory_pbo_upload_transfer (pbo);
    gst_gl_memory_pbo_download_transfer (pbo);

    cuda_res[i] = gst_cuda_graphics_resource_map (gst_res[i], stream,
        CU_GRAPHICS_MAP_RESOURCE_FLAGS_READ_ONLY);
    if (!cuda_res[i]) {
      GST_ERROR_OBJECT (self, "Couldn't map resource");
      goto out;
    }

    cuda_ret = CuGraphicsResourceGetMappedPointer (&src_devptr[i],
        &src_size, cuda_res[i]);
    if (!gst_cuda_result (cuda_ret)) {
      GST_ERROR_OBJECT (self, "Couldn't get mapped device pointer");
      goto out;
    }
  }

  if (gst_buffer_pool_acquire_buffer (priv->internal_pool,
          &data->out_buf, nullptr) != GST_FLOW_OK) {
    GST_ERROR_OBJECT (self, "Couldn't acquire fallback buffer");
    goto out;
  }

  cmem = (GstCudaMemory *) gst_buffer_peek_memory (data->out_buf, 0);
  if (!gst_memory_map (GST_MEMORY_CAST (cmem), &map_info,
          (GstMapFlags) (GST_MAP_WRITE | GST_MAP_CUDA))) {
    GST_ERROR_OBJECT (self, "Couldn't map fallback memory");
    goto out;
  }

  memset (&copy_param, 0, sizeof (CUDA_MEMCPY2D));

  for (guint i = 0; i < GST_VIDEO_INFO_N_PLANES (info); i++) {
    copy_param.srcMemoryType = CU_MEMORYTYPE_DEVICE;
    copy_param.srcDevice = src_devptr[i];
    copy_param.srcPitch = GST_VIDEO_INFO_PLANE_STRIDE (info, i);

    copy_param.dstMemoryType = CU_MEMORYTYPE_DEVICE;
    copy_param.dstDevice = ((CUdeviceptr) map_info.data) + cmem->info.offset[i];
    copy_param.dstPitch = cmem->info.stride[0];

    copy_param.WidthInBytes = GST_VIDEO_INFO_COMP_WIDTH (info, i) *
        GST_VIDEO_INFO_COMP_PSTRIDE (info, i);
    copy_param.Height = GST_VIDEO_INFO_COMP_HEIGHT (info, i);

    if (!gst_cuda_result (CuMemcpy2DAsync (&copy_param, stream))) {
      gst_memory_unmap (GST_MEMORY_CAST (cmem), &map_info);
      GST_ERROR_OBJECT (self, "Couldn't copy plane %d", i);
      goto out;
    }
  }

  gst_memory_unmap (GST_MEMORY_CAST (cmem), &map_info);

  ret = TRUE;

out:
  for (guint i = 0; i < gst_buffer_n_memory (data->in_buf); i++) {
    if (!gst_res[i])
      break;

    gst_cuda_graphics_resource_unmap (gst_res[i], stream);
  }

  CuStreamSynchronize (stream);
  gst_cuda_context_pop (nullptr);
  if (!ret)
    gst_clear_buffer (&data->out_buf);
}
#endif /* HAVE_CUDA_GST_GL */

static GstFlowReturn
gst_nv_encoder_prepare_task_input_cuda (GstNvEncoder * self,
    GstBuffer * buffer, GstNvEncTask * task)
{
  GstNvEncoderPrivate *priv = self->priv;
  std::shared_ptr < GstNvEncObject > object = priv->object;
  GstMemory *mem;
  GstCudaMemory *cmem;
  NVENCSTATUS status;
  GstCudaStream *stream;
  GstNvEncResource *resource = nullptr;
  const GstVideoInfo *info = &priv->input_state->info;

  mem = gst_buffer_peek_memory (buffer, 0);

#ifdef HAVE_CUDA_GST_GL
  if (priv->gl_interop) {
    if (gst_is_gl_memory (mem) && gst_buffer_n_memory (buffer) ==
        GST_VIDEO_INFO_N_PLANES (info)) {
      GstNvEncGLInteropData gl_data;
      GstGLMemory *gl_mem = (GstGLMemory *) mem;
      gl_data.self = self;
      gl_data.in_buf = buffer;
      gl_data.out_buf = nullptr;
      gst_gl_context_thread_add (gl_mem->mem.context,
          (GstGLContextThreadFunc) gst_nv_encoder_upload_gl, &gl_data);
      if (gl_data.out_buf) {
        mem = gst_buffer_peek_memory (gl_data.out_buf, 0);
        status = object->AcquireResource (mem, &resource);

        if (status != NV_ENC_SUCCESS) {
          gst_buffer_unref (gl_data.out_buf);
          GST_ERROR_OBJECT (self, "Failed to get resource, status %"
              GST_NVENC_STATUS_FORMAT, GST_NVENC_STATUS_ARGS (status));
          return GST_FLOW_ERROR;
        }

        gst_nv_enc_task_set_resource (task, gl_data.out_buf, resource);

        return GST_FLOW_OK;
      } else {
        GST_WARNING_OBJECT (self, "GL interop failed");
        priv->gl_interop = FALSE;
      }
    }
  }
#endif

  if (!gst_is_cuda_memory (mem)) {
    GST_LOG_OBJECT (self, "Not a CUDA buffer, system copy");
    return gst_nv_encoder_copy_system (self, info, buffer, task);
  }

  cmem = GST_CUDA_MEMORY_CAST (mem);
  if (cmem->context != priv->context) {
    GST_LOG_OBJECT (self, "Different context, system copy");
    return gst_nv_encoder_copy_system (self, info, buffer, task);
  }

  status = object->AcquireResource (mem, &resource);

  if (status != NV_ENC_SUCCESS) {
    GST_ERROR_OBJECT (self, "Failed to get resource, status %"
        GST_NVENC_STATUS_FORMAT, GST_NVENC_STATUS_ARGS (status));

    return GST_FLOW_ERROR;
  }

  stream = gst_cuda_memory_get_stream (cmem);
  if (stream != priv->stream) {
    /* different stream, needs sync */
    gst_cuda_memory_sync (cmem);
  }

  gst_nv_enc_task_set_resource (task, gst_buffer_ref (buffer), resource);

  return GST_FLOW_OK;
}

#ifdef G_OS_WIN32
static GstBuffer *
gst_nv_encoder_copy_d3d11 (GstNvEncoder * self,
    GstBuffer * src_buffer, GstBufferPool * pool, gboolean shared)
{
  GstNvEncoderPrivate *priv = self->priv;
  D3D11_TEXTURE2D_DESC src_desc, dst_desc;
  D3D11_BOX src_box;
  guint subresource_idx;
  GstMemory *src_mem, *dst_mem;
  GstMapInfo src_info, dst_info;
  ID3D11Texture2D *src_tex, *dst_tex;
  ID3D11Device *device_handle;
  ID3D11DeviceContext *device_context;
  GstBuffer *dst_buffer;
  GstFlowReturn ret;
  ComPtr < IDXGIResource > dxgi_resource;
  ComPtr < ID3D11Texture2D > shared_texture;
  HANDLE shared_handle;
  GstD3D11Device *device;
  HRESULT hr;

  ret = gst_buffer_pool_acquire_buffer (pool, &dst_buffer, NULL);
  if (ret != GST_FLOW_OK) {
    GST_ERROR_OBJECT (self, "Failed to acquire buffer");
    return NULL;
  }

  src_mem = gst_buffer_peek_memory (src_buffer, 0);
  dst_mem = gst_buffer_peek_memory (dst_buffer, 0);

  device = GST_D3D11_MEMORY_CAST (src_mem)->device;

  device_handle = gst_d3d11_device_get_device_handle (device);
  device_context = gst_d3d11_device_get_device_context_handle (device);

  if (!gst_memory_map (src_mem, &src_info,
          (GstMapFlags) (GST_MAP_READ | GST_MAP_D3D11))) {
    GST_WARNING ("Failed to map src memory");
    gst_buffer_unref (dst_buffer);
    return NULL;
  }

  if (!gst_memory_map (dst_mem, &dst_info,
          (GstMapFlags) (GST_MAP_WRITE | GST_MAP_D3D11))) {
    GST_WARNING ("Failed to map dst memory");
    gst_memory_unmap (src_mem, &src_info);
    gst_buffer_unref (dst_buffer);
    return NULL;
  }

  src_tex = (ID3D11Texture2D *) src_info.data;
  dst_tex = (ID3D11Texture2D *) dst_info.data;

  gst_d3d11_memory_get_texture_desc (GST_D3D11_MEMORY_CAST (src_mem),
      &src_desc);
  gst_d3d11_memory_get_texture_desc (GST_D3D11_MEMORY_CAST (dst_mem),
      &dst_desc);
  subresource_idx =
      gst_d3d11_memory_get_subresource_index (GST_D3D11_MEMORY_CAST (src_mem));

  if (shared) {
    hr = dst_tex->QueryInterface (IID_PPV_ARGS (&dxgi_resource));
    if (!gst_d3d11_result (hr, priv->device)) {
      GST_ERROR_OBJECT (self,
          "IDXGIResource interface is not available, hr: 0x%x", (guint) hr);
      goto error;
    }

    hr = dxgi_resource->GetSharedHandle (&shared_handle);
    if (!gst_d3d11_result (hr, priv->device)) {
      GST_ERROR_OBJECT (self, "Failed to get shared handle, hr: 0x%x",
          (guint) hr);
      goto error;
    }

    hr = device_handle->OpenSharedResource (shared_handle,
        IID_PPV_ARGS (&shared_texture));

    if (!gst_d3d11_result (hr, device)) {
      GST_ERROR_OBJECT (self, "Failed to get shared texture, hr: 0x%x",
          (guint) hr);
      goto error;
    }

    dst_tex = shared_texture.Get ();
  }

  src_box.left = 0;
  src_box.top = 0;
  src_box.front = 0;
  src_box.back = 1;
  src_box.right = MIN (src_desc.Width, dst_desc.Width);
  src_box.bottom = MIN (src_desc.Height, dst_desc.Height);

  if (shared) {
    if (priv->fence && priv->fence->device != device)
      gst_clear_d3d11_fence (&priv->fence);

    if (!priv->fence)
      priv->fence = gst_d3d11_device_create_fence (device);

    if (!priv->fence) {
      GST_ERROR_OBJECT (self, "Couldn't crete fence");
      goto error;
    }

    gst_d3d11_device_lock (device);
  }

  device_context->CopySubresourceRegion (dst_tex, 0,
      0, 0, 0, src_tex, subresource_idx, &src_box);

  if (shared) {
    if (!gst_d3d11_fence_signal (priv->fence) ||
        !gst_d3d11_fence_wait (priv->fence)) {
      GST_ERROR_OBJECT (self, "Couldn't sync GPU operation");
      gst_d3d11_device_unlock (device);
      gst_clear_d3d11_fence (&priv->fence);
      goto error;
    }

    gst_d3d11_device_unlock (device);
  }

  gst_memory_unmap (dst_mem, &dst_info);
  gst_memory_unmap (src_mem, &src_info);

  return dst_buffer;

error:
  gst_memory_unmap (dst_mem, &dst_info);
  gst_memory_unmap (src_mem, &src_info);
  gst_buffer_unref (dst_buffer);

  return NULL;
}

static GstBuffer *
gst_nv_encoder_upload_d3d11_frame (GstNvEncoder * self,
    const GstVideoInfo * info, GstBuffer * buffer, GstBufferPool * pool)
{
  GstD3D11Memory *dmem;
  D3D11_TEXTURE2D_DESC desc;

  dmem = (GstD3D11Memory *) gst_buffer_peek_memory (buffer, 0);

  gst_d3d11_memory_get_texture_desc (dmem, &desc);
  if (desc.Usage != D3D11_USAGE_DEFAULT) {
    GST_TRACE_OBJECT (self, "Not a default usage texture, d3d11 copy");
    return gst_nv_encoder_copy_d3d11 (self, buffer, pool, FALSE);
  }

  GST_TRACE_OBJECT (self, "Use input buffer without copy");

  return gst_buffer_ref (buffer);
}

static GstFlowReturn
gst_nv_encoder_prepare_task_input_d3d11 (GstNvEncoder * self,
    GstBuffer * buffer, GstNvEncTask * task)
{
  GstNvEncoderPrivate *priv = self->priv;
  std::shared_ptr < GstNvEncObject > object = priv->object;
  GstMemory *mem;
  GstD3D11Memory *dmem;
  NVENCSTATUS status;
  GstBuffer *upload_buffer = nullptr;
  GstNvEncResource *resource = nullptr;
  const GstVideoInfo *info = &priv->input_state->info;
  GstBufferPool *pool = priv->internal_pool;

  if (gst_buffer_n_memory (buffer) > 1) {
    GST_LOG_OBJECT (self, "Not a native DXGI format, system copy");
    return gst_nv_encoder_copy_system (self, info, buffer, task);
  }

  mem = gst_buffer_peek_memory (buffer, 0);
  if (!gst_is_d3d11_memory (mem)) {
    GST_LOG_OBJECT (self, "Not a D3D11 buffer, system copy");
    return gst_nv_encoder_copy_system (self, info, buffer, task);
  }

  dmem = GST_D3D11_MEMORY_CAST (mem);
  if (dmem->device != priv->device) {
    gint64 adapter_luid;

    g_object_get (dmem->device, "adapter-luid", &adapter_luid, NULL);
    if (adapter_luid == priv->dxgi_adapter_luid) {
      GST_LOG_OBJECT (self, "Different device but same GPU, copy d3d11");
      upload_buffer = gst_nv_encoder_copy_d3d11 (self, buffer, pool, TRUE);
    } else {
      GST_LOG_OBJECT (self, "Different device, system copy");
      return gst_nv_encoder_copy_system (self, info, buffer, task);
    }
  }

  if (!upload_buffer)
    upload_buffer =
        gst_nv_encoder_upload_d3d11_frame (self, info, buffer, pool);

  if (!upload_buffer) {
    GST_ERROR_OBJECT (self, "Failed to upload buffer");
    return GST_FLOW_ERROR;
  }

  status = object->AcquireResource (mem, &resource);

  if (status != NV_ENC_SUCCESS) {
    GST_ERROR_OBJECT (self, "Failed to get resource, status %"
        GST_NVENC_STATUS_FORMAT, GST_NVENC_STATUS_ARGS (status));
    gst_buffer_unref (upload_buffer);

    return GST_FLOW_ERROR;
  }

  gst_nv_enc_task_set_resource (task, upload_buffer, resource);

  return GST_FLOW_OK;
}
#endif

static GstFlowReturn
gst_nv_encoder_prepare_task_input (GstNvEncoder * self,
    GstBuffer * buffer, GstNvEncTask * task)
{
  GstNvEncoderPrivate *priv = self->priv;
  GstFlowReturn ret = GST_FLOW_ERROR;

  switch (priv->selected_device_mode) {
#ifdef G_OS_WIN32
    case GST_NV_ENCODER_DEVICE_D3D11:
      ret = gst_nv_encoder_prepare_task_input_d3d11 (self, buffer, task);
      break;
#endif
    case GST_NV_ENCODER_DEVICE_CUDA:
      ret = gst_nv_encoder_prepare_task_input_cuda (self, buffer, task);
      break;
    default:
      g_assert_not_reached ();
      break;
  }

  return ret;
}

static gboolean
gst_nv_encoder_foreach_caption_meta (GstBuffer * buffer, GstMeta ** meta,
    GArray * payload)
{
  GstVideoCaptionMeta *cc_meta;
  GstByteWriter br;
  guint payload_size;
  NV_ENC_SEI_PAYLOAD sei_payload;

  if ((*meta)->info->api != GST_VIDEO_CAPTION_META_API_TYPE)
    return TRUE;

  cc_meta = (GstVideoCaptionMeta *) (*meta);

  if (cc_meta->caption_type != GST_VIDEO_CAPTION_TYPE_CEA708_RAW)
    return TRUE;

  /* 1 byte contry_code + 10 bytes CEA-708 specific data + caption data */
  payload_size = 11 + cc_meta->size;

  gst_byte_writer_init_with_size (&br, payload_size, FALSE);

  /* 8-bits itu_t_t35_country_code */
  gst_byte_writer_put_uint8 (&br, 181);

  /* 16-bits itu_t_t35_provider_code */
  gst_byte_writer_put_uint8 (&br, 0);
  gst_byte_writer_put_uint8 (&br, 49);

  /* 32-bits ATSC_user_identifier */
  gst_byte_writer_put_uint8 (&br, 'G');
  gst_byte_writer_put_uint8 (&br, 'A');
  gst_byte_writer_put_uint8 (&br, '9');
  gst_byte_writer_put_uint8 (&br, '4');

  /* 8-bits ATSC1_data_user_data_type_code */
  gst_byte_writer_put_uint8 (&br, 3);

  /* 8-bits:
   * 1 bit process_em_data_flag (0)
   * 1 bit process_cc_data_flag (1)
   * 1 bit additional_data_flag (0)
   * 5-bits cc_count
   */
  gst_byte_writer_put_uint8 (&br, ((cc_meta->size / 3) & 0x1f) | 0x40);

  /* 8 bits em_data, unused */
  gst_byte_writer_put_uint8 (&br, 255);

  gst_byte_writer_put_data (&br, cc_meta->data, cc_meta->size);

  /* 8 marker bits */
  gst_byte_writer_put_uint8 (&br, 255);

  sei_payload.payloadSize = gst_byte_writer_get_pos (&br);
  sei_payload.payloadType = 4;
  sei_payload.payload = gst_byte_writer_reset_and_get_data (&br);

  g_array_append_val (payload, sei_payload);

  return TRUE;
}

static GstFlowReturn
gst_nv_encoder_handle_frame (GstVideoEncoder * encoder,
    GstVideoCodecFrame * frame)
{
  GstNvEncoder *self = GST_NV_ENCODER (encoder);
  GstNvEncoderPrivate *priv = self->priv;
  GstNvEncoderClass *klass = GST_NV_ENCODER_GET_CLASS (self);
  GstFlowReturn ret = GST_FLOW_ERROR;
  GstNvEncTask *task = nullptr;
  GstNvEncoderReconfigure reconfig;
  GstBuffer *in_buf = frame->input_buffer;
  NVENCSTATUS status;

  if (priv->last_flow != GST_FLOW_OK) {
    GST_INFO_OBJECT (self, "Last flow was %s",
        gst_flow_get_name (priv->last_flow));
    gst_video_encoder_finish_frame (encoder, frame);

    return priv->last_flow;
  }

  if (!priv->object && !gst_nv_encoder_init_session (self, in_buf)) {
    GST_ERROR_OBJECT (self, "Encoder object was not configured");
    gst_video_encoder_finish_frame (encoder, frame);

    return GST_FLOW_NOT_NEGOTIATED;
  }

  reconfig = klass->check_reconfigure (self, &priv->config);
  switch (reconfig) {
    case GST_NV_ENCODER_RECONFIGURE_BITRATE:
      if (!gst_nv_encoder_reconfigure_session (self)) {
        gst_video_encoder_finish_frame (encoder, frame);
        return GST_FLOW_NOT_NEGOTIATED;
      }
      break;
    case GST_NV_ENCODER_RECONFIGURE_FULL:
    {
      gst_nv_encoder_drain (self, TRUE);
      if (!gst_nv_encoder_init_session (self, nullptr)) {
        gst_video_encoder_finish_frame (encoder, frame);
        return GST_FLOW_NOT_NEGOTIATED;
      }
      break;
    }
    default:
      break;
  }

  /* Release stream lock temporarily for encoding thread to be able to
   * push encoded data */
  GST_VIDEO_ENCODER_STREAM_UNLOCK (self);
  GST_TRACE_OBJECT (self, "Waiting for new task");
  ret = priv->object->AcquireTask (&task, false);
  GST_VIDEO_ENCODER_STREAM_LOCK (self);

  if (priv->last_flow != GST_FLOW_OK) {
    GST_INFO_OBJECT (self, "Last flow was %s",
        gst_flow_get_name (priv->last_flow));
    gst_video_encoder_finish_frame (encoder, frame);

    return priv->last_flow;
  }

  if (ret != GST_FLOW_OK) {
    GST_DEBUG_OBJECT (self, "AcquireTask returned %s", gst_flow_get_name (ret));
    gst_video_encoder_finish_frame (encoder, frame);
    return ret;
  }

  gst_nv_encoder_device_lock (self);
  ret = gst_nv_encoder_prepare_task_input (self, in_buf, task);
  gst_nv_encoder_device_unlock (self);

  if (ret != GST_FLOW_OK) {
    GST_ERROR_OBJECT (self, "Failed to upload frame");
    gst_nv_enc_task_unref (task);
    gst_video_encoder_finish_frame (encoder, frame);

    return ret;
  }

  if (priv->cc_insert != GST_NV_ENCODER_SEI_DISABLED) {
    gst_buffer_foreach_meta (in_buf,
        (GstBufferForeachMetaFunc) gst_nv_encoder_foreach_caption_meta,
        gst_nv_enc_task_get_sei_payload (task));
  }

  status = priv->object->Encode (frame,
      gst_nv_encoder_get_pic_struct (self, in_buf), task);
  if (status != NV_ENC_SUCCESS) {
    GST_ERROR_OBJECT (self, "Failed to encode frame");
    gst_video_encoder_finish_frame (encoder, frame);

    return GST_FLOW_ERROR;
  }

  gst_video_codec_frame_unref (frame);

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_nv_encoder_finish (GstVideoEncoder * encoder)
{
  GstNvEncoder *self = GST_NV_ENCODER (encoder);

  GST_DEBUG_OBJECT (self, "Finish");

  gst_nv_encoder_drain (self, TRUE);

  return GST_FLOW_OK;
}

static gboolean
gst_nv_encoder_flush (GstVideoEncoder * encoder)
{
  GstNvEncoder *self = GST_NV_ENCODER (encoder);
  GstNvEncoderPrivate *priv = self->priv;

  GST_DEBUG_OBJECT (self, "Flush");

  gst_nv_encoder_drain (self, TRUE);

  priv->last_flow = GST_FLOW_OK;

  return TRUE;
}

static gboolean
gst_nv_encoder_transform_meta (GstVideoEncoder * encoder,
    GstVideoCodecFrame * frame, GstMeta * meta)
{
  GstNvEncoder *self = GST_NV_ENCODER (encoder);
  GstNvEncoderPrivate *priv = self->priv;
  GstVideoCaptionMeta *cc_meta;

  /* We need to handle only case CC meta should be dropped */
  if (priv->cc_insert != GST_NV_ENCODER_SEI_INSERT_AND_DROP)
    goto out;

  if (meta->info->api != GST_VIDEO_CAPTION_META_API_TYPE)
    goto out;

  cc_meta = (GstVideoCaptionMeta *) meta;
  if (cc_meta->caption_type != GST_VIDEO_CAPTION_TYPE_CEA708_RAW)
    goto out;

  /* Don't copy this meta into output buffer */
  return FALSE;

out:
  return GST_VIDEO_ENCODER_CLASS (parent_class)->transform_meta (encoder,
      frame, meta);
}

void
gst_nv_encoder_set_device_mode (GstNvEncoder * encoder,
    GstNvEncoderDeviceMode mode, guint cuda_device_id, gint64 adapter_luid)
{
  GstNvEncoderPrivate *priv = encoder->priv;

  priv->subclass_device_mode = mode;
  priv->selected_device_mode = mode;
  priv->cuda_device_id = cuda_device_id;
  priv->dxgi_adapter_luid = adapter_luid;
}

/**
 * GstNvEncoderPreset:
 *
 * Since: 1.22
 */
GType
gst_nv_encoder_preset_get_type (void)
{
  static GType preset_type = 0;
  static const GEnumValue presets[] = {
    /**
     * GstNvEncoderPreset::default:
     *
     * Since: 1.22
     */
    {GST_NV_ENCODER_PRESET_DEFAULT, "Default (deprecated, use p1~7 with tune)",
        "default"},

    /**
     * GstNvEncoderPreset::hp:
     *
     * Since: 1.22
     */
    {GST_NV_ENCODER_PRESET_HP,
        "High Performance (deprecated, use p1~7 with tune)", "hp"},

    /**
     * GstNvEncoderPreset::hq:
     *
     * Since: 1.22
     */
    {GST_NV_ENCODER_PRESET_HQ, "High Quality (deprecated, use p1~7 with tune)",
        "hq"},

    /**
     * GstNvEncoderPreset::low-latency:
     *
     * Since: 1.22
     */
    {GST_NV_ENCODER_PRESET_LOW_LATENCY_DEFAULT,
        "Low Latency (deprecated, use p1~7 with tune)", "low-latency"},

    /**
     * GstNvEncoderPreset::low-latency-hq:
     *
     * Since: 1.22
     */
    {GST_NV_ENCODER_PRESET_LOW_LATENCY_HQ,
          "Low Latency (deprecated, use p1~7 with tune), High Quality",
        "low-latency-hq"},

    /**
     * GstNvEncoderPreset::low-latency-hq:
     *
     * Since: 1.22
     */
    {GST_NV_ENCODER_PRESET_LOW_LATENCY_HP,
          "Low Latency (deprecated, use p1~7 with tune), High Performance",
        "low-latency-hp"},

    /**
     * GstNvEncoderPreset::lossless:
     *
     * Since: 1.22
     */
    {GST_NV_ENCODER_PRESET_LOSSLESS_DEFAULT,
        "Lossless (deprecated, use p1~7 with tune)", "lossless"},

    /**
     * GstNvEncoderPreset::lossless-hp:
     *
     * Since: 1.22
     */
    {GST_NV_ENCODER_PRESET_LOSSLESS_HP,
          "Lossless (deprecated, use p1~7 with tune), High Performance",
        "lossless-hp"},

    /**
     * GstNvEncoderPreset::p1:
     *
     * Since: 1.24
     */
    {GST_NV_ENCODER_PRESET_P1, "P1, fastest", "p1"},

    /**
     * GstNvEncoderPreset::p2:
     *
     * Since: 1.24
     */
    {GST_NV_ENCODER_PRESET_P2, "P2, faster", "p2"},

    /**
     * GstNvEncoderPreset::p3:
     *
     * Since: 1.24
     */
    {GST_NV_ENCODER_PRESET_P3, "P3, fast", "p3"},

    /**
     * GstNvEncoderPreset::p4:
     *
     * Since: 1.24
     */
    {GST_NV_ENCODER_PRESET_P4, "P4, medium", "p4"},

    /**
     * GstNvEncoderPreset::p5:
     *
     * Since: 1.24
     */
    {GST_NV_ENCODER_PRESET_P5, "P5, slow", "p5"},

    /**
     * GstNvEncoderPreset::p6:
     *
     * Since: 1.24
     */
    {GST_NV_ENCODER_PRESET_P6, "P6, slower", "p6"},

    /**
     * GstNvEncoderPreset::p7:
     *
     * Since: 1.24
     */
    {GST_NV_ENCODER_PRESET_P7, "P7, slowest", "p7"},
    {0, NULL, NULL},
  };

  if (g_once_init_enter (&preset_type)) {
    GType type = g_enum_register_static ("GstNvEncoderPreset", presets);

    g_once_init_leave (&preset_type, type);
  }

  return preset_type;
}

/**
 * GstNvEncoderRCMode:
 *
 * Since: 1.22
 */
GType
gst_nv_encoder_rc_mode_get_type (void)
{
  static GType rc_mode_type = 0;
  static const GEnumValue rc_modes[] = {
    {GST_NV_ENCODER_RC_MODE_CONSTQP, "Constant Quantization", "cqp"},
    {GST_NV_ENCODER_RC_MODE_VBR, "Variable Bit Rate", "vbr"},
    {GST_NV_ENCODER_RC_MODE_CBR, "Constant Bit Rate", "cbr"},
    {GST_NV_ENCODER_RC_MODE_CBR_LOWDELAY_HQ,
        "Low-Delay CBR, High Quality "
          "(deprecated, use cbr with tune and multipass)", "cbr-ld-hq"},
    {GST_NV_ENCODER_RC_MODE_CBR_HQ, "CBR, High Quality "
          "(deprecated, use cbr with tune and multipass)", "cbr-hq"},
    {GST_NV_ENCODER_RC_MODE_VBR_HQ, "VBR, High Quality "
          "(deprecated, use vbr with tune and multipass)", "vbr-hq"},
    {0, NULL, NULL},
  };

  if (g_once_init_enter (&rc_mode_type)) {
    GType type = g_enum_register_static ("GstNvEncoderRCMode", rc_modes);

    g_once_init_leave (&rc_mode_type, type);
  }

  return rc_mode_type;
}

void
gst_nv_encoder_preset_to_native (GstNvEncoderPreset preset,
    GstNvEncoderTune tune, GUID * preset_guid, NV_ENC_TUNING_INFO * tune_info)
{
  gboolean is_low_latency = FALSE;
  gboolean is_lossless = FALSE;
  switch (preset) {
    case GST_NV_ENCODER_PRESET_DEFAULT:
      *preset_guid = NV_ENC_PRESET_P4_GUID;
      break;
    case GST_NV_ENCODER_PRESET_HP:
      *preset_guid = NV_ENC_PRESET_P1_GUID;
      break;
    case GST_NV_ENCODER_PRESET_HQ:
      *preset_guid = NV_ENC_PRESET_P7_GUID;
      break;
    case GST_NV_ENCODER_PRESET_LOW_LATENCY_DEFAULT:
      is_low_latency = TRUE;
      *preset_guid = NV_ENC_PRESET_P4_GUID;
      break;
    case GST_NV_ENCODER_PRESET_LOW_LATENCY_HQ:
      is_low_latency = TRUE;
      *preset_guid = NV_ENC_PRESET_P7_GUID;
      break;
    case GST_NV_ENCODER_PRESET_LOW_LATENCY_HP:
      is_low_latency = TRUE;
      *preset_guid = NV_ENC_PRESET_P1_GUID;
      break;
    case GST_NV_ENCODER_PRESET_LOSSLESS_DEFAULT:
      is_lossless = TRUE;
      *preset_guid = NV_ENC_PRESET_P4_GUID;
      break;
    case GST_NV_ENCODER_PRESET_LOSSLESS_HP:
      is_lossless = TRUE;
      *preset_guid = NV_ENC_PRESET_P1_GUID;
      break;
    case GST_NV_ENCODER_PRESET_P1:
      *preset_guid = NV_ENC_PRESET_P1_GUID;
      break;
    case GST_NV_ENCODER_PRESET_P2:
      *preset_guid = NV_ENC_PRESET_P2_GUID;
      break;
    case GST_NV_ENCODER_PRESET_P3:
      *preset_guid = NV_ENC_PRESET_P3_GUID;
      break;
    case GST_NV_ENCODER_PRESET_P4:
      *preset_guid = NV_ENC_PRESET_P4_GUID;
      break;
    case GST_NV_ENCODER_PRESET_P5:
      *preset_guid = NV_ENC_PRESET_P5_GUID;
      break;
    case GST_NV_ENCODER_PRESET_P6:
      *preset_guid = NV_ENC_PRESET_P6_GUID;
      break;
    case GST_NV_ENCODER_PRESET_P7:
      *preset_guid = NV_ENC_PRESET_P7_GUID;
      break;
    default:
      *preset_guid = NV_ENC_PRESET_P4_GUID;
      break;
  }

  switch (tune) {
    case GST_NV_ENCODER_TUNE_DEFAULT:
      if (is_low_latency)
        *tune_info = NV_ENC_TUNING_INFO_LOW_LATENCY;
      else if (is_lossless)
        *tune_info = NV_ENC_TUNING_INFO_LOSSLESS;
      else
        *tune_info = NV_ENC_TUNING_INFO_HIGH_QUALITY;
      break;
    case GST_NV_ENCODER_TUNE_HIGH_QUALITY:
      *tune_info = NV_ENC_TUNING_INFO_HIGH_QUALITY;
      break;
    case GST_NV_ENCODER_TUNE_LOW_LATENCY:
      *tune_info = NV_ENC_TUNING_INFO_LOW_LATENCY;
      break;
    case GST_NV_ENCODER_TUNE_ULTRA_LOW_LATENCY:
      *tune_info = NV_ENC_TUNING_INFO_ULTRA_LOW_LATENCY;
      break;
    case GST_NV_ENCODER_TUNE_LOSSLESS:
      *tune_info = NV_ENC_TUNING_INFO_LOSSLESS;
      break;
    default:
      *tune_info = NV_ENC_TUNING_INFO_HIGH_QUALITY;
      break;
  }
}

void
gst_nv_encoder_rc_mode_to_native (GstNvEncoderRCMode rc_mode,
    GstNvEncoderMultiPass multipass, NV_ENC_PARAMS_RC_MODE * rc_mode_native,
    NV_ENC_MULTI_PASS * multipass_native)
{
  gboolean is_hq = FALSE;
  switch (rc_mode) {
    case GST_NV_ENCODER_RC_MODE_CONSTQP:
      *rc_mode_native = NV_ENC_PARAMS_RC_CONSTQP;
      break;
    case GST_NV_ENCODER_RC_MODE_VBR:
      *rc_mode_native = NV_ENC_PARAMS_RC_VBR;
      break;
    case GST_NV_ENCODER_RC_MODE_CBR:
      *rc_mode_native = NV_ENC_PARAMS_RC_CBR;
      break;
    case GST_NV_ENCODER_RC_MODE_CBR_LOWDELAY_HQ:
      is_hq = TRUE;
      *rc_mode_native = NV_ENC_PARAMS_RC_CBR;
      break;
    case GST_NV_ENCODER_RC_MODE_CBR_HQ:
      is_hq = TRUE;
      *rc_mode_native = NV_ENC_PARAMS_RC_CBR;
      break;
    case GST_NV_ENCODER_RC_MODE_VBR_HQ:
      is_hq = TRUE;
      *rc_mode_native = NV_ENC_PARAMS_RC_VBR;
      break;
    default:
      *rc_mode_native = NV_ENC_PARAMS_RC_VBR;
      break;
  }

  switch (multipass) {
    case GST_NV_ENCODER_MULTI_PASS_DEFAULT:
      if (is_hq)
        *multipass_native = NV_ENC_TWO_PASS_QUARTER_RESOLUTION;
      else
        *multipass_native = NV_ENC_MULTI_PASS_DISABLED;
      break;
    case GST_NV_ENCODER_MULTI_PASS_DISABLED:
      *multipass_native = NV_ENC_MULTI_PASS_DISABLED;
      break;
    case GST_NV_ENCODER_TWO_PASS_QUARTER_RESOLUTION:
      *multipass_native = NV_ENC_TWO_PASS_QUARTER_RESOLUTION;
      break;
    case GST_NV_ENCODER_TWO_PASS_FULL_RESOLUTION:
      *multipass_native = NV_ENC_TWO_PASS_FULL_RESOLUTION;
      break;
    default:
      *multipass_native = NV_ENC_MULTI_PASS_DISABLED;
      break;
  }
}

/**
 * GstNvEncoderSeiInsertMode:
 *
 * Since: 1.24
 */
GType
gst_nv_encoder_sei_insert_mode_get_type (void)
{
  static GType type = 0;
  static const GEnumValue insert_modes[] = {
    /**
     * GstNvEncoderSeiInsertMode::insert:
     *
     * Since: 1.24
     */
    {GST_NV_ENCODER_SEI_INSERT, "Insert SEI", "insert"},

    /**
     * GstNvEncoderSeiInsertMode::insert-and-drop:
     *
     * Since: 1.24
     */
    {GST_NV_ENCODER_SEI_INSERT_AND_DROP,
          "Insert SEI and remove corresponding meta from output buffer",
        "insert-and-drop"},

    /**
     * GstNvEncoderSeiInsertMode::disabled:
     *
     * Since: 1.24
     */
    {GST_NV_ENCODER_SEI_DISABLED, "Disable SEI insertion", "disabled"},
    {0, nullptr, nullptr}
  };

  GST_CUDA_CALL_ONCE_BEGIN {
    type = g_enum_register_static ("GstNvEncoderSeiInsertMode", insert_modes);
  } GST_CUDA_CALL_ONCE_END;

  return type;
}

/**
 * GstNvEncoderMultiPass:
 *
 * Since: 1.24
 */
GType
gst_nv_encoder_multi_pass_get_type (void)
{
  static GType type = 0;
  static const GEnumValue modes[] = {
    /**
     * GstNvEncoderMultiPass::disabled:
     *
     * Since: 1.24
     */
    {GST_NV_ENCODER_MULTI_PASS_DEFAULT,
        "Disable multi-pass when cqp, vbr or cbr is used. "
          "Otherwise encoder will select it based on rc-mode", "default"},

    /**
     * GstNvEncoderMultiPass::disabled:
     *
     * Since: 1.24
     */
    {GST_NV_ENCODER_MULTI_PASS_DISABLED, "Disabled", "disabled"},

    /**
     * GstNvEncoderMultiPass::two-pass-quarter:
     *
     * Since: 1.24
     */
    {GST_NV_ENCODER_TWO_PASS_QUARTER_RESOLUTION,
          "Two pass with quarter resolution encoding in first pass",
        "two-pass-quarter"},

    /**
     * GstNvEncoderMultiPass::two-pass:
     *
     * Since: 1.24
     */
    {GST_NV_ENCODER_TWO_PASS_FULL_RESOLUTION, "Two pass", "two-pass"},
    {0, nullptr, nullptr}
  };

  GST_CUDA_CALL_ONCE_BEGIN {
    type = g_enum_register_static ("GstNvEncoderMultiPass", modes);
  } GST_CUDA_CALL_ONCE_END;

  return type;
}

/**
 * GstNvEncoderTune:
 *
 * Since: 1.24
 */
GType
gst_nv_encoder_tune_get_type (void)
{
  static GType type = 0;
  static const GEnumValue modes[] = {
    /**
     * GstNvEncoderTune::default:
     *
     * Since: 1.24
     */
    {GST_NV_ENCODER_TUNE_DEFAULT, "High quality when p1~7 preset is used. "
          "Otherwise encoder will select it based on preset", "default"},

    /**
     * GstNvEncoderTune::high-quality:
     *
     * Since: 1.24
     */
    {GST_NV_ENCODER_TUNE_HIGH_QUALITY, "High quality", "high-quality"},

    /**
     * GstNvEncoderMultiPass::low-latency:
     *
     * Since: 1.24
     */
    {GST_NV_ENCODER_TUNE_LOW_LATENCY, "Low latency", "low-latency"},

    /**
     * GstNvEncoderMultiPass::ultra-low-latency:
     *
     * Since: 1.24
     */
    {GST_NV_ENCODER_TUNE_ULTRA_LOW_LATENCY, "Ultra low latency",
        "ultra-low-latency"},

    /**
     * GstNvEncoderMultiPass::lossless:
     *
     * Since: 1.24
     */
    {GST_NV_ENCODER_TUNE_LOSSLESS, "Lossless", "lossless"},
    {0, nullptr, nullptr}
  };

  GST_CUDA_CALL_ONCE_BEGIN {
    type = g_enum_register_static ("GstNvEncoderTune", modes);
  } GST_CUDA_CALL_ONCE_END;

  return type;
}

GstNvEncoderClassData *
gst_nv_encoder_class_data_new (void)
{
  GstNvEncoderClassData *data = g_new0 (GstNvEncoderClassData, 1);
  data->ref_count = 1;

  return data;
}

GstNvEncoderClassData *
gst_nv_encoder_class_data_ref (GstNvEncoderClassData * cdata)
{
  g_atomic_int_add (&cdata->ref_count, 1);

  return cdata;
}

void
gst_nv_encoder_class_data_unref (GstNvEncoderClassData * cdata)
{
  if (g_atomic_int_dec_and_test (&cdata->ref_count)) {
    gst_clear_caps (&cdata->sink_caps);
    gst_clear_caps (&cdata->src_caps);
    if (cdata->formats)
      g_list_free_full (cdata->formats, (GDestroyNotify) g_free);
    if (cdata->profiles)
      g_list_free_full (cdata->profiles, (GDestroyNotify) g_free);
    g_free (cdata);
  }
}

void
gst_nv_encoder_get_encoder_caps (gpointer session, const GUID * encode_guid,
    GstNvEncoderDeviceCaps * device_caps)
{
  GstNvEncoderDeviceCaps dev_caps = { 0, };
  NV_ENC_CAPS_PARAM caps_param = { 0, };
  NVENCSTATUS status;
  GUID guid = *encode_guid;

  GST_DEBUG_CATEGORY_INIT (gst_nv_encoder_debug, "nvencoder", 0, "nvencoder");

  caps_param.version = gst_nvenc_get_caps_param_version ();

#define CHECK_CAPS(to_query,val,default_val) G_STMT_START { \
  gint _val; \
  caps_param.capsToQuery = to_query; \
  status = NvEncGetEncodeCaps (session, guid, &caps_param, \
      &_val); \
  if (status != NV_ENC_SUCCESS) { \
    GST_WARNING ("Unable to query %s, status: %" \
        GST_NVENC_STATUS_FORMAT, G_STRINGIFY (to_query), \
        GST_NVENC_STATUS_ARGS (status)); \
    val = default_val; \
  } else { \
    GST_DEBUG ("%s: %d", G_STRINGIFY (to_query), _val); \
    val = _val; \
  } \
} G_STMT_END

  CHECK_CAPS (NV_ENC_CAPS_NUM_MAX_BFRAMES, dev_caps.max_bframes, 0);
  CHECK_CAPS (NV_ENC_CAPS_SUPPORTED_RATECONTROL_MODES,
      dev_caps.ratecontrol_modes, NV_ENC_PARAMS_RC_VBR);
  CHECK_CAPS (NV_ENC_CAPS_SUPPORT_FIELD_ENCODING, dev_caps.field_encoding, 0);
  CHECK_CAPS (NV_ENC_CAPS_SUPPORT_MONOCHROME, dev_caps.monochrome, 0);
  CHECK_CAPS (NV_ENC_CAPS_SUPPORT_FMO, dev_caps.fmo, 0);
  CHECK_CAPS (NV_ENC_CAPS_SUPPORT_QPELMV, dev_caps.qpelmv, 0);
  CHECK_CAPS (NV_ENC_CAPS_SUPPORT_BDIRECT_MODE, dev_caps.bdirect_mode, 0);
  CHECK_CAPS (NV_ENC_CAPS_SUPPORT_CABAC, dev_caps.cabac, 0);
  CHECK_CAPS (NV_ENC_CAPS_SUPPORT_ADAPTIVE_TRANSFORM,
      dev_caps.adaptive_transform, 0);
  CHECK_CAPS (NV_ENC_CAPS_SUPPORT_STEREO_MVC, dev_caps.stereo_mvc, 0);
  CHECK_CAPS (NV_ENC_CAPS_NUM_MAX_TEMPORAL_LAYERS, dev_caps.temoral_layers, 0);
  CHECK_CAPS (NV_ENC_CAPS_SUPPORT_HIERARCHICAL_PFRAMES,
      dev_caps.hierarchical_pframes, 0);
  CHECK_CAPS (NV_ENC_CAPS_SUPPORT_HIERARCHICAL_BFRAMES,
      dev_caps.hierarchical_bframes, 0);
  CHECK_CAPS (NV_ENC_CAPS_LEVEL_MAX, dev_caps.level_max, 0);
  CHECK_CAPS (NV_ENC_CAPS_LEVEL_MIN, dev_caps.level_min, 0);
  CHECK_CAPS (NV_ENC_CAPS_SEPARATE_COLOUR_PLANE,
      dev_caps.separate_colour_plane, 0);
  CHECK_CAPS (NV_ENC_CAPS_WIDTH_MAX, dev_caps.width_max, 4096);
  CHECK_CAPS (NV_ENC_CAPS_HEIGHT_MAX, dev_caps.height_max, 4096);
  CHECK_CAPS (NV_ENC_CAPS_SUPPORT_TEMPORAL_SVC, dev_caps.temporal_svc, 0);
  CHECK_CAPS (NV_ENC_CAPS_SUPPORT_DYN_RES_CHANGE, dev_caps.dyn_res_change, 0);
  CHECK_CAPS (NV_ENC_CAPS_SUPPORT_DYN_BITRATE_CHANGE,
      dev_caps.dyn_bitrate_change, 0);
  CHECK_CAPS (NV_ENC_CAPS_SUPPORT_DYN_FORCE_CONSTQP,
      dev_caps.dyn_force_constqp, 0);
  CHECK_CAPS (NV_ENC_CAPS_SUPPORT_DYN_RCMODE_CHANGE,
      dev_caps.dyn_rcmode_change, 0);
  CHECK_CAPS (NV_ENC_CAPS_SUPPORT_SUBFRAME_READBACK,
      dev_caps.subframe_readback, 0);
  CHECK_CAPS (NV_ENC_CAPS_SUPPORT_CONSTRAINED_ENCODING,
      dev_caps.constrained_encoding, 0);
  CHECK_CAPS (NV_ENC_CAPS_SUPPORT_INTRA_REFRESH, dev_caps.intra_refresh, 0);
  CHECK_CAPS (NV_ENC_CAPS_SUPPORT_CUSTOM_VBV_BUF_SIZE,
      dev_caps.custom_vbv_buf_size, 0);
  CHECK_CAPS (NV_ENC_CAPS_SUPPORT_DYNAMIC_SLICE_MODE,
      dev_caps.dynamic_slice_mode, 0);
  CHECK_CAPS (NV_ENC_CAPS_SUPPORT_REF_PIC_INVALIDATION,
      dev_caps.ref_pic_invalidation, 0);
  CHECK_CAPS (NV_ENC_CAPS_PREPROC_SUPPORT, dev_caps.preproc_support, 0);
  /* NOTE: Async is Windows only */
#ifdef G_OS_WIN32
  CHECK_CAPS (NV_ENC_CAPS_ASYNC_ENCODE_SUPPORT,
      dev_caps.async_encoding_support, 0);
#endif
  CHECK_CAPS (NV_ENC_CAPS_MB_NUM_MAX, dev_caps.mb_num_max, 0);
  CHECK_CAPS (NV_ENC_CAPS_MB_PER_SEC_MAX, dev_caps.mb_per_sec_max, 0);
  CHECK_CAPS (NV_ENC_CAPS_SUPPORT_YUV444_ENCODE, dev_caps.yuv444_encode, 0);
  CHECK_CAPS (NV_ENC_CAPS_SUPPORT_LOSSLESS_ENCODE, dev_caps.lossless_encode, 0);
  CHECK_CAPS (NV_ENC_CAPS_SUPPORT_SAO, dev_caps.sao, 0);
  CHECK_CAPS (NV_ENC_CAPS_SUPPORT_MEONLY_MODE, dev_caps.meonly_mode, 0);
  CHECK_CAPS (NV_ENC_CAPS_SUPPORT_LOOKAHEAD, dev_caps.lookahead, 0);
  CHECK_CAPS (NV_ENC_CAPS_SUPPORT_TEMPORAL_AQ, dev_caps.temporal_aq, 0);
  CHECK_CAPS (NV_ENC_CAPS_SUPPORT_10BIT_ENCODE,
      dev_caps.supports_10bit_encode, 0);
  CHECK_CAPS (NV_ENC_CAPS_NUM_MAX_LTR_FRAMES, dev_caps.num_max_ltr_frames, 0);
  CHECK_CAPS (NV_ENC_CAPS_SUPPORT_WEIGHTED_PREDICTION,
      dev_caps.weighted_prediction, 0);
  CHECK_CAPS (NV_ENC_CAPS_SUPPORT_BFRAME_REF_MODE, dev_caps.bframe_ref_mode, 0);
  CHECK_CAPS (NV_ENC_CAPS_SUPPORT_EMPHASIS_LEVEL_MAP,
      dev_caps.emphasis_level_map, 0);
  CHECK_CAPS (NV_ENC_CAPS_WIDTH_MIN, dev_caps.width_min, 16);
  CHECK_CAPS (NV_ENC_CAPS_HEIGHT_MIN, dev_caps.height_min, 16);
  CHECK_CAPS (NV_ENC_CAPS_SUPPORT_MULTIPLE_REF_FRAMES,
      dev_caps.multiple_ref_frames, 0);
#undef CHECK_CAPS

  *device_caps = dev_caps;
}

void
gst_nv_encoder_merge_device_caps (const GstNvEncoderDeviceCaps * a,
    const GstNvEncoderDeviceCaps * b, GstNvEncoderDeviceCaps * merged)
{
  GstNvEncoderDeviceCaps caps;

#define SELECT_MAX(value) G_STMT_START { \
  caps.value = MAX (a->value, b->value); \
} G_STMT_END

#define SELECT_MIN(value) G_STMT_START { \
  caps.value = MAX (MIN (a->value, b->value), 1); \
} G_STMT_END

  SELECT_MAX (max_bframes);
  SELECT_MAX (ratecontrol_modes);
  SELECT_MAX (field_encoding);
  SELECT_MAX (monochrome);
  SELECT_MAX (fmo);
  SELECT_MAX (qpelmv);
  SELECT_MAX (bdirect_mode);
  SELECT_MAX (cabac);
  SELECT_MAX (adaptive_transform);
  SELECT_MAX (stereo_mvc);
  SELECT_MAX (temoral_layers);
  SELECT_MAX (hierarchical_pframes);
  SELECT_MAX (hierarchical_bframes);
  SELECT_MAX (level_max);
  SELECT_MAX (level_min);
  SELECT_MAX (separate_colour_plane);
  SELECT_MAX (width_max);
  SELECT_MAX (height_max);
  SELECT_MAX (temporal_svc);
  SELECT_MAX (dyn_res_change);
  SELECT_MAX (dyn_bitrate_change);
  SELECT_MAX (dyn_force_constqp);
  SELECT_MAX (dyn_rcmode_change);
  SELECT_MAX (subframe_readback);
  SELECT_MAX (constrained_encoding);
  SELECT_MAX (intra_refresh);
  SELECT_MAX (custom_vbv_buf_size);
  SELECT_MAX (dynamic_slice_mode);
  SELECT_MAX (ref_pic_invalidation);
  SELECT_MAX (preproc_support);
  SELECT_MAX (async_encoding_support);
  SELECT_MAX (mb_num_max);
  SELECT_MAX (mb_per_sec_max);
  SELECT_MAX (yuv444_encode);
  SELECT_MAX (lossless_encode);
  SELECT_MAX (sao);
  SELECT_MAX (meonly_mode);
  SELECT_MAX (lookahead);
  SELECT_MAX (temporal_aq);
  SELECT_MAX (supports_10bit_encode);
  SELECT_MAX (num_max_ltr_frames);
  SELECT_MAX (weighted_prediction);
  SELECT_MAX (bframe_ref_mode);
  SELECT_MAX (emphasis_level_map);
  SELECT_MIN (width_min);
  SELECT_MIN (height_min);
  SELECT_MAX (multiple_ref_frames);

#undef SELECT_MAX
#undef SELECT_MIN

  *merged = caps;
}

gboolean
_gst_nv_enc_result (NVENCSTATUS status, GObject * self, const gchar * file,
    const gchar * function, gint line)
{
  if (status == NV_ENC_SUCCESS)
    return TRUE;

#ifndef GST_DISABLE_GST_DEBUG
  const gchar *status_str = nvenc_status_to_string (status);

  gst_debug_log (GST_CAT_DEFAULT, GST_LEVEL_ERROR, file, function,
      line, self, "NvEnc API call failed: 0x%x, %s",
      (guint) status, status_str);
#endif

  return FALSE;
}
