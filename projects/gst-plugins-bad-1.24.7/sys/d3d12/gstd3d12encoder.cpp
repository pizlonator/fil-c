/* GStreamer
 * Copyright (C) 2024 Seungha Yang <seungha@centricular.com>
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

#include "gstd3d12encoder.h"
#include "gstd3d12encoderbufferpool.h"
#include "gstd3d12pluginutils.h"
#include <gst/base/gstqueuearray.h>
#include <directx/d3dx12.h>
#include <wrl.h>
#include <memory>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>

/* *INDENT-OFF* */
using namespace Microsoft::WRL;
/* *INDENT-ON* */

GST_DEBUG_CATEGORY_STATIC (gst_d3d12_encoder_debug);
#define GST_CAT_DEFAULT gst_d3d12_encoder_debug

enum
{
  PROP_0,
  PROP_ADAPTER_LUID,
  PROP_DEVICE_ID,
  PROP_VENDOR_ID,
};

#define ASYNC_DEPTH 4

struct EncoderOutputData
{
  GstVideoCodecFrame *frame = nullptr;
  GstD3D12EncoderBuffer *buffer = nullptr;
  guint64 fence_val = 0;
};

/* *INDENT-OFF* */
struct EncoderSessionData
{
  EncoderSessionData ()
  {
    output_queue = gst_queue_array_new_for_struct (sizeof (EncoderOutputData),
        16);
  }

  ~EncoderSessionData ()
  {
    if (upload_pool)
      gst_buffer_pool_set_active (upload_pool, FALSE);
    gst_clear_object (&upload_pool);
    gst_clear_object (&encoder_pool);
    gst_queue_array_free (output_queue);
  }

  ComPtr<ID3D12VideoEncoder> encoder;
  ComPtr<ID3D12VideoEncoderHeap> heap;

  std::mutex queue_lock;
  std::condition_variable queue_cond;
  GstQueueArray *output_queue;

  GstD3D12EncoderBufferPool *encoder_pool = nullptr;
  GstBufferPool *upload_pool = nullptr;
};

struct EncoderCmdData
{
  EncoderCmdData ()
  {
    event_handle = CreateEventEx (nullptr, nullptr, 0, EVENT_ALL_ACCESS);
  }

  ~EncoderCmdData ()
  {
    if (queue)
      gst_d3d12_command_queue_fence_wait (queue, G_MAXUINT64, event_handle);

    CloseHandle (event_handle);
    gst_clear_object (&ca_pool);
    gst_clear_object (&queue);
  }

  ComPtr<ID3D12VideoDevice3> video_device;
  ComPtr<ID3D12VideoEncodeCommandList2> cl;
  GstD3D12CommandQueue *queue = nullptr;
  GstD3D12CommandAllocatorPool *ca_pool = nullptr;
  HANDLE event_handle;
  guint64 fence_val = 0;
};

struct GstD3D12EncoderPrivate
{
  GstD3D12EncoderPrivate ()
  {
    fence_data_pool = gst_d3d12_fence_data_pool_new ();
  }

  ~GstD3D12EncoderPrivate ()
  {
    gst_clear_object (&fence_data_pool);
    g_clear_pointer (&input_state, gst_video_codec_state_unref);
  }

  GstD3D12EncoderConfig config = { };
  D3D12_VIDEO_ENCODER_DESC encoder_desc = { };
  D3D12_VIDEO_ENCODER_HEAP_DESC heap_desc = { };
  D3D12_FEATURE_DATA_VIDEO_ENCODER_RESOURCE_REQUIREMENTS resource_req = { };

  GstVideoCodecState *input_state = nullptr;

  std::vector<D3D12_VIDEO_ENCODER_FRAME_SUBREGION_METADATA> subregions;

  std::unique_ptr<EncoderCmdData> cmd;
  std::unique_ptr<EncoderSessionData> session;
  GThread *output_thread = nullptr;
  GstD3D12FenceDataPool *fence_data_pool;
  std::atomic<GstFlowReturn> last_flow = { GST_FLOW_OK };
  std::atomic<bool> flushing = { false };
  bool array_of_textures = false;
  D3D12_FEATURE_DATA_FORMAT_INFO format_info = { };
};
/* *INDENT-ON* */

static void gst_d3d12_encoder_finalize (GObject * object);
static void gst_d3d12_encoder_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_d3d12_encoder_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static void gst_d3d12_encoder_set_context (GstElement * element,
    GstContext * context);
static gboolean gst_d3d12_encoder_open (GstVideoEncoder * encoder);
static gboolean gst_d3d12_encoder_start (GstVideoEncoder * encoder);
static gboolean gst_d3d12_encoder_stop (GstVideoEncoder * encoder);
static gboolean gst_d3d12_encoder_close (GstVideoEncoder * encoder);
static gboolean gst_d3d12_encoder_sink_query (GstVideoEncoder * encoder,
    GstQuery * query);
static gboolean gst_d3d12_encoder_src_query (GstVideoEncoder * encoder,
    GstQuery * query);
static gboolean gst_d3d12_encoder_propose_allocation (GstVideoEncoder * encoder,
    GstQuery * query);
static gboolean gst_d3d12_encoder_set_format (GstVideoEncoder * encoder,
    GstVideoCodecState * state);
static GstFlowReturn gst_d3d12_encoder_handle_frame (GstVideoEncoder * encoder,
    GstVideoCodecFrame * frame);
static GstFlowReturn gst_d3d12_encoder_finish (GstVideoEncoder * encoder);
static gboolean gst_d3d12_encoder_flush (GstVideoEncoder * encoder);

#define gst_d3d12_encoder_parent_class parent_class
G_DEFINE_ABSTRACT_TYPE_WITH_CODE (GstD3D12Encoder,
    gst_d3d12_encoder, GST_TYPE_VIDEO_ENCODER,
    GST_DEBUG_CATEGORY_INIT (gst_d3d12_encoder_debug,
        "d3d12encoder", 0, "d3d12encoder"));

static void
gst_d3d12_encoder_class_init (GstD3D12EncoderClass * klass)
{
  auto object_class = G_OBJECT_CLASS (klass);
  auto element_class = GST_ELEMENT_CLASS (klass);
  auto encoder_class = GST_VIDEO_ENCODER_CLASS (klass);
  auto read_only_params = (GParamFlags) (GST_PARAM_DOC_SHOW_DEFAULT |
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  object_class->finalize = gst_d3d12_encoder_finalize;
  object_class->get_property = gst_d3d12_encoder_get_property;

  g_object_class_install_property (object_class, PROP_ADAPTER_LUID,
      g_param_spec_int64 ("adapter-luid", "Adapter LUID",
          "DXGI Adapter LUID (Locally Unique Identifier) of created device",
          G_MININT64, G_MAXINT64, 0, read_only_params));
  g_object_class_install_property (object_class, PROP_DEVICE_ID,
      g_param_spec_uint ("device-id", "Device Id",
          "DXGI Device ID", 0, G_MAXUINT32, 0, read_only_params));
  g_object_class_install_property (object_class, PROP_VENDOR_ID,
      g_param_spec_uint ("vendor-id", "Vendor Id",
          "DXGI Vendor ID", 0, G_MAXUINT32, 0, read_only_params));

  element_class->set_context =
      GST_DEBUG_FUNCPTR (gst_d3d12_encoder_set_context);

  encoder_class->open = GST_DEBUG_FUNCPTR (gst_d3d12_encoder_open);
  encoder_class->start = GST_DEBUG_FUNCPTR (gst_d3d12_encoder_start);
  encoder_class->stop = GST_DEBUG_FUNCPTR (gst_d3d12_encoder_stop);
  encoder_class->close = GST_DEBUG_FUNCPTR (gst_d3d12_encoder_close);
  encoder_class->sink_query = GST_DEBUG_FUNCPTR (gst_d3d12_encoder_sink_query);
  encoder_class->src_query = GST_DEBUG_FUNCPTR (gst_d3d12_encoder_src_query);
  encoder_class->propose_allocation =
      GST_DEBUG_FUNCPTR (gst_d3d12_encoder_propose_allocation);
  encoder_class->set_format = GST_DEBUG_FUNCPTR (gst_d3d12_encoder_set_format);
  encoder_class->handle_frame =
      GST_DEBUG_FUNCPTR (gst_d3d12_encoder_handle_frame);
  encoder_class->finish = GST_DEBUG_FUNCPTR (gst_d3d12_encoder_finish);
  encoder_class->flush = GST_DEBUG_FUNCPTR (gst_d3d12_encoder_flush);

  gst_type_mark_as_plugin_api (GST_TYPE_D3D12_ENCODER, (GstPluginAPIFlags) 0);
  gst_type_mark_as_plugin_api (GST_TYPE_D3D12_ENCODER_RATE_CONTROL,
      (GstPluginAPIFlags) 0);
  gst_type_mark_as_plugin_api (GST_TYPE_D3D12_ENCODER_RATE_CONTROL_SUPPORT,
      (GstPluginAPIFlags) 0);
  gst_type_mark_as_plugin_api (GST_TYPE_D3D12_ENCODER_SUBREGION_LAYOUT,
      (GstPluginAPIFlags) 0);
  gst_type_mark_as_plugin_api (GST_TYPE_D3D12_ENCODER_SUBREGION_LAYOUT_SUPPORT,
      (GstPluginAPIFlags) 0);
  gst_type_mark_as_plugin_api (GST_TYPE_D3D12_ENCODER_SEI_INSERT_MODE,
      (GstPluginAPIFlags) 0);
}

static void
gst_d3d12_encoder_init (GstD3D12Encoder * self)
{
  self->priv = new GstD3D12EncoderPrivate ();
}

static void
gst_d3d12_encoder_finalize (GObject * object)
{
  auto self = GST_D3D12_ENCODER (object);

  delete self->priv;
  gst_clear_object (&self->device);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_d3d12_encoder_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  auto self = GST_D3D12_ENCODER (object);
  auto klass = GST_D3D12_ENCODER_GET_CLASS (self);

  switch (prop_id) {
    case PROP_ADAPTER_LUID:
      g_value_set_int64 (value, klass->adapter_luid);
      break;
    case PROP_DEVICE_ID:
      g_value_set_uint (value, klass->device_id);
      break;
    case PROP_VENDOR_ID:
      g_value_set_uint (value, klass->vendor_id);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_d3d12_encoder_set_context (GstElement * element, GstContext * context)
{
  auto self = GST_D3D12_ENCODER (element);
  auto klass = GST_D3D12_ENCODER_GET_CLASS (self);

  gst_d3d12_handle_set_context_for_adapter_luid (element, context,
      klass->adapter_luid, &self->device);

  GST_ELEMENT_CLASS (parent_class)->set_context (element, context);
}

static gboolean
gst_d3d12_encoder_open (GstVideoEncoder * encoder)
{
  auto self = GST_D3D12_ENCODER (encoder);
  auto priv = self->priv;
  auto klass = GST_D3D12_ENCODER_GET_CLASS (self);
  HRESULT hr;
  ComPtr < ID3D12VideoDevice3 > video_device;

  if (!gst_d3d12_ensure_element_data_for_adapter_luid (GST_ELEMENT_CAST (self),
          klass->adapter_luid, &self->device)) {
    GST_ERROR_OBJECT (self, "Couldn't get device");
    return FALSE;
  }

  auto device = gst_d3d12_device_get_device_handle (self->device);
  hr = device->QueryInterface (IID_PPV_ARGS (&video_device));
  if (!gst_d3d12_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "ID3D12VideoDevice3 interface is unavailable");
    return FALSE;
  }

  D3D12_COMMAND_QUEUE_DESC queue_desc = { };
  queue_desc.Type = D3D12_COMMAND_LIST_TYPE_VIDEO_ENCODE;

  auto cmd = std::make_unique < EncoderCmdData > ();
  cmd->queue = gst_d3d12_command_queue_new (self->device,
      &queue_desc, ASYNC_DEPTH);
  if (!cmd->queue) {
    GST_ERROR_OBJECT (self, "Couldn't create command queue");
    return FALSE;
  }

  cmd->ca_pool = gst_d3d12_command_allocator_pool_new (self->device,
      D3D12_COMMAND_LIST_TYPE_VIDEO_ENCODE);
  cmd->video_device = video_device;

  priv->cmd = std::move (cmd);

  return TRUE;
}

static gboolean
gst_d3d12_encoder_start (GstVideoEncoder * encoder)
{
  auto self = GST_D3D12_ENCODER (encoder);
  auto priv = self->priv;

  GST_DEBUG_OBJECT (self, "Start");

  if (!priv->cmd) {
    GST_ERROR_OBJECT (self, "Command data is not configured");
    return FALSE;
  }

  priv->last_flow = GST_FLOW_OK;

  return TRUE;
}

static void
gst_d3d12_encoder_drain (GstD3D12Encoder * self, gboolean locked)
{
  auto priv = self->priv;

  GST_DEBUG_OBJECT (self, "Draining");

  /* Unlock stream-lock so that output thread can push pending data
   * from output thread */
  if (locked)
    GST_VIDEO_ENCODER_STREAM_UNLOCK (self);

  if (priv->cmd) {
    GST_DEBUG_OBJECT (self, "Waiting for command finish %" G_GUINT64_FORMAT,
        priv->cmd->fence_val);
    gst_d3d12_command_queue_fence_wait (priv->cmd->queue, priv->cmd->fence_val,
        priv->cmd->event_handle);
  }

  if (priv->session && priv->output_thread) {
    GST_DEBUG_OBJECT (self, "Sending empty task");
    auto empty_data = EncoderOutputData ();
    std::lock_guard < std::mutex > lk (priv->session->queue_lock);
    gst_queue_array_push_tail_struct (priv->session->output_queue, &empty_data);
    priv->session->queue_cond.notify_one ();
  }

  g_clear_pointer (&priv->output_thread, g_thread_join);

  if (locked)
    GST_VIDEO_ENCODER_STREAM_LOCK (self);

  GST_DEBUG_OBJECT (self, "Drained");
}

static gboolean
gst_d3d12_encoder_stop (GstVideoEncoder * encoder)
{
  auto self = GST_D3D12_ENCODER (encoder);
  auto priv = self->priv;

  GST_DEBUG_OBJECT (self, "Stop");

  gst_d3d12_encoder_drain (self, FALSE);
  priv->session = nullptr;

  return TRUE;
}

static gboolean
gst_d3d12_encoder_close (GstVideoEncoder * encoder)
{
  auto self = GST_D3D12_ENCODER (encoder);
  auto priv = self->priv;

  GST_DEBUG_OBJECT (self, "Close");

  gst_d3d12_encoder_drain (self, FALSE);
  priv->session = nullptr;
  priv->cmd = nullptr;

  gst_clear_object (&self->device);

  return TRUE;
}

static gboolean
gst_d3d12_encoder_handle_query (GstD3D12Encoder * self, GstQuery * query)
{
  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CONTEXT:
      return gst_d3d12_handle_context_query (GST_ELEMENT (self),
          query, self->device);
    default:
      break;
  }

  return FALSE;
}

static gboolean
gst_d3d12_encoder_sink_query (GstVideoEncoder * encoder, GstQuery * query)
{
  auto self = GST_D3D12_ENCODER (encoder);

  if (gst_d3d12_encoder_handle_query (self, query))
    return TRUE;

  return GST_VIDEO_ENCODER_CLASS (parent_class)->sink_query (encoder, query);
}

static gboolean
gst_d3d12_encoder_src_query (GstVideoEncoder * encoder, GstQuery * query)
{
  auto self = GST_D3D12_ENCODER (encoder);

  if (gst_d3d12_encoder_handle_query (self, query))
    return TRUE;

  return GST_VIDEO_ENCODER_CLASS (parent_class)->src_query (encoder, query);
}

static GstBufferPool *
gst_d3d12_encoder_create_upload_pool (GstD3D12Encoder * self)
{
  auto priv = self->priv;
  GstVideoInfo info;

  gst_video_info_set_format (&info, GST_VIDEO_FORMAT_NV12,
      priv->config.resolution.Width, priv->config.resolution.Height);
  auto caps = gst_video_info_to_caps (&info);
  auto pool = gst_d3d12_buffer_pool_new (self->device);
  auto config = gst_buffer_pool_get_config (pool);

  auto params = gst_d3d12_allocation_params_new (self->device, &info,
      GST_D3D12_ALLOCATION_FLAG_DEFAULT,
      D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS |
      D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
  gst_buffer_pool_config_set_d3d12_allocation_params (config, params);
  gst_d3d12_allocation_params_free (params);
  gst_buffer_pool_config_set_params (config, caps, info.size, 0, 0);
  gst_caps_unref (caps);

  if (!gst_buffer_pool_set_config (pool, config)) {
    GST_ERROR_OBJECT (self, "Set config failed");
    gst_object_unref (pool);
    return nullptr;
  }

  if (!gst_buffer_pool_set_active (pool, TRUE)) {
    GST_ERROR_OBJECT (self, "Set active failed");
    gst_object_unref (pool);
    return nullptr;
  }

  return pool;
}

static gboolean
gst_d3d12_encoder_propose_allocation (GstVideoEncoder * encoder,
    GstQuery * query)
{
  auto self = GST_D3D12_ENCODER (encoder);
  auto priv = self->priv;
  GstCaps *caps;
  GstVideoInfo info;

  gst_query_parse_allocation (query, &caps, nullptr);
  if (!caps) {
    GST_WARNING_OBJECT (self, "null caps in query");
    return FALSE;
  }

  if (!gst_video_info_from_caps (&info, caps)) {
    GST_WARNING_OBJECT (self, "Failed to convert caps into info");
    return FALSE;
  }

  auto features = gst_caps_get_features (caps, 0);
  gboolean is_d3d12 = FALSE;
  GstBufferPool *pool;
  if (gst_caps_features_contains (features,
          GST_CAPS_FEATURE_MEMORY_D3D12_MEMORY)) {
    GST_DEBUG_OBJECT (self, "Upstream supports d3d12 memory");
    pool = gst_d3d12_buffer_pool_new (self->device);
    is_d3d12 = TRUE;
  } else {
    pool = gst_video_buffer_pool_new ();
  }

  if (!pool) {
    GST_ERROR_OBJECT (self, "Couldn't create buffer pool");
    return FALSE;
  }

  auto config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_add_option (config, GST_BUFFER_POOL_OPTION_VIDEO_META);

  if (is_d3d12) {
    GstVideoAlignment align;

    gst_video_alignment_reset (&align);
    align.padding_right = priv->config.resolution.Width - info.width;
    align.padding_bottom = priv->config.resolution.Height - info.height;

    auto params = gst_d3d12_allocation_params_new (self->device, &info,
        GST_D3D12_ALLOCATION_FLAG_DEFAULT,
        D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS |
        D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
    gst_d3d12_allocation_params_alignment (params, &align);
    gst_buffer_pool_config_set_d3d12_allocation_params (config, params);
    gst_d3d12_allocation_params_free (params);
  } else {
    gst_buffer_pool_config_add_option (config,
        GST_BUFFER_POOL_OPTION_VIDEO_ALIGNMENT);
  }

  guint size = GST_VIDEO_INFO_SIZE (&info);
  gst_buffer_pool_config_set_params (config, caps, size, ASYNC_DEPTH, 0);

  if (!gst_buffer_pool_set_config (pool, config)) {
    GST_ERROR_OBJECT (self, "Failed to set pool config");
    gst_object_unref (pool);
    return FALSE;
  }

  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_get_params (config, nullptr, &size, nullptr, nullptr);
  gst_structure_free (config);

  gst_query_add_allocation_pool (query, pool, size, ASYNC_DEPTH, 0);
  gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, nullptr);
  gst_object_unref (pool);

  return TRUE;
}

static gboolean
gst_d3d12_encoder_set_format (GstVideoEncoder * encoder,
    GstVideoCodecState * state)
{
  auto self = GST_D3D12_ENCODER (encoder);
  auto priv = self->priv;
  auto klass = GST_D3D12_ENCODER_GET_CLASS (self);

  gst_d3d12_encoder_drain (self, TRUE);
  priv->session = nullptr;

  g_clear_pointer (&priv->input_state, gst_video_codec_state_unref);
  priv->input_state = gst_video_codec_state_ref (state);

  priv->last_flow = GST_FLOW_OK;
  priv->config = { };

  GST_DEBUG_OBJECT (self, "Set format with caps %" GST_PTR_FORMAT, state->caps);

  if (!klass->new_sequence (self, priv->cmd->video_device.Get (), state,
          &priv->config)) {
    GST_ERROR_OBJECT (self, "Couldn't accept new sequence");
    return FALSE;
  }

  g_assert (priv->config.max_subregions > 0);

  auto & config = priv->config;
  auto flags = config.support_flags;

  GST_DEBUG_OBJECT (self, "Encoder caps, "
      "rate-control-reconfig: %d, resolution-reconfig: %d, "
      "vbv-size: %d, frame-analysis: %d, texture-arrays: %d, delta-qp: %d, "
      "subregion-reconfig: %d, qp-range: %d, initial-qp: %d, "
      "max-frame-size: %d, gop-reconfigure: %d, me-precision-limit: %d",
      CHECK_SUPPORT_FLAG (flags, RATE_CONTROL_RECONFIGURATION_AVAILABLE),
      CHECK_SUPPORT_FLAG (flags, RESOLUTION_RECONFIGURATION_AVAILABLE),
      CHECK_SUPPORT_FLAG (flags, RATE_CONTROL_VBV_SIZE_CONFIG_AVAILABLE),
      CHECK_SUPPORT_FLAG (flags, RATE_CONTROL_FRAME_ANALYSIS_AVAILABLE),
      CHECK_SUPPORT_FLAG (flags, RECONSTRUCTED_FRAMES_REQUIRE_TEXTURE_ARRAYS),
      CHECK_SUPPORT_FLAG (flags, RATE_CONTROL_DELTA_QP_AVAILABLE),
      CHECK_SUPPORT_FLAG (flags, SUBREGION_LAYOUT_RECONFIGURATION_AVAILABLE),
      CHECK_SUPPORT_FLAG (flags, RATE_CONTROL_ADJUSTABLE_QP_RANGE_AVAILABLE),
      CHECK_SUPPORT_FLAG (flags, RATE_CONTROL_INITIAL_QP_AVAILABLE),
      CHECK_SUPPORT_FLAG (flags, RATE_CONTROL_MAX_FRAME_SIZE_AVAILABLE),
      CHECK_SUPPORT_FLAG (flags, SEQUENCE_GOP_RECONFIGURATION_AVAILABLE),
      CHECK_SUPPORT_FLAG (flags,
          MOTION_ESTIMATION_PRECISION_MODE_LIMIT_AVAILABLE));

  auto video_device = priv->cmd->video_device;

  auto & resource_req = priv->resource_req;
  resource_req.Codec = klass->codec;
  resource_req.Profile = config.profile_desc;
  resource_req.InputFormat = DXGI_FORMAT_NV12;
  resource_req.PictureTargetResolution = config.resolution;
  auto hr = video_device->CheckFeatureSupport
      (D3D12_FEATURE_VIDEO_ENCODER_RESOURCE_REQUIREMENTS,
      &resource_req, sizeof (resource_req));
  if (!gst_d3d12_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Couldn't query resource requirement");
    return FALSE;
  }

  if (CHECK_SUPPORT_FLAG (flags, RECONSTRUCTED_FRAMES_REQUIRE_TEXTURE_ARRAYS)) {
    GST_DEBUG_OBJECT (self, "Device requires texture array");
    priv->array_of_textures = false;
  } else {
    GST_DEBUG_OBJECT (self, "Device supports array of textures");
    priv->array_of_textures = true;
  }

  auto device = gst_d3d12_device_get_device_handle (self->device);
  priv->format_info.Format = DXGI_FORMAT_NV12;
  hr = device->CheckFeatureSupport (D3D12_FEATURE_FORMAT_INFO,
      &priv->format_info, sizeof (priv->format_info));
  if (!gst_d3d12_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Couldn't query format info");
    return FALSE;
  }

  auto session = std::make_unique < EncoderSessionData > ();

  auto & desc = priv->encoder_desc;
  desc = { };
  desc.EncodeCodec = klass->codec;
  desc.EncodeProfile = config.profile_desc;
  desc.InputFormat = priv->format_info.Format;
  desc.CodecConfiguration = config.codec_config;
  desc.MaxMotionEstimationPrecision =
      D3D12_VIDEO_ENCODER_MOTION_ESTIMATION_PRECISION_MODE_MAXIMUM;

  hr = video_device->CreateVideoEncoder (&desc,
      IID_PPV_ARGS (&session->encoder));
  if (!gst_d3d12_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Couldn't create encoder");
    return FALSE;
  }

  auto & heap_desc = priv->heap_desc;
  heap_desc = { };
  heap_desc.EncodeCodec = klass->codec;
  heap_desc.EncodeProfile = config.profile_desc;
  heap_desc.EncodeLevel = config.level;
  heap_desc.ResolutionsListCount = 1;
  heap_desc.pResolutionList = &config.resolution;
  hr = video_device->CreateVideoEncoderHeap (&heap_desc,
      IID_PPV_ARGS (&session->heap));
  if (!gst_d3d12_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Couldn't create encoder heap");
    return FALSE;
  }

  guint resolved_metadata_size;
  resolved_metadata_size = sizeof (D3D12_VIDEO_ENCODER_OUTPUT_METADATA)
      + sizeof (D3D12_VIDEO_ENCODER_FRAME_SUBREGION_METADATA) *
      config.max_subregions;
  if (resource_req.EncoderMetadataBufferAccessAlignment > 1) {
    resolved_metadata_size =
        ((resolved_metadata_size +
            resource_req.EncoderMetadataBufferAccessAlignment - 1) /
        resource_req.EncoderMetadataBufferAccessAlignment) *
        resource_req.EncoderMetadataBufferAccessAlignment;
  }

  /* TODO: calculate bitstream buffer size */
  guint bitstream_size = 1024 * 1024 * 8;
  session->encoder_pool = gst_d3d12_encoder_buffer_pool_new (self->device,
      resource_req.MaxEncoderOutputMetadataBufferSize,
      resolved_metadata_size, bitstream_size, ASYNC_DEPTH);

  session->upload_pool = gst_d3d12_encoder_create_upload_pool (self);
  if (!session->upload_pool)
    return FALSE;

  priv->session = std::move (session);

  return TRUE;
}

static GstBuffer *
gst_d3d12_encoder_upload_frame (GstD3D12Encoder * self, GstBuffer * buffer)
{
  auto priv = self->priv;
  const auto info = &priv->input_state->info;
  GstBuffer *upload = nullptr;
  gboolean d3d12_copy = FALSE;

  auto mem = gst_buffer_peek_memory (buffer, 0);
  if (gst_is_d3d12_memory (mem)) {
    auto dmem = GST_D3D12_MEMORY_CAST (mem);
    if (dmem->device == self->device) {
      GstMapInfo map_info;
      if (!gst_memory_map (mem, &map_info,
              (GstMapFlags) (GST_MAP_READ | GST_MAP_D3D12))) {
        GST_ERROR_OBJECT (self, "Couldn't map memory");
        return nullptr;
      }

      gst_memory_unmap (mem, &map_info);

      auto resource = gst_d3d12_memory_get_resource_handle (dmem);
      auto desc = resource->GetDesc ();
      if (desc.Width >= (UINT64) priv->config.resolution.Width &&
          desc.Height >= priv->config.resolution.Height) {
        return gst_buffer_ref (buffer);
      }

      d3d12_copy = TRUE;
    }
  }

  gst_buffer_pool_acquire_buffer (priv->session->upload_pool, &upload, nullptr);
  if (!upload) {
    GST_ERROR_OBJECT (self, "Couldn't acquire buffer");
    return nullptr;
  }

  if (d3d12_copy) {
    std::vector < GstD3D12CopyTextureRegionArgs > copy_args;
    auto dmem = GST_D3D12_MEMORY_CAST (mem);
    auto src_resource = gst_d3d12_memory_get_resource_handle (dmem);
    dmem = (GstD3D12Memory *) gst_buffer_peek_memory (upload, 0);
    auto dst_resource = gst_d3d12_memory_get_resource_handle (dmem);
    D3D12_BOX src_box[2];

    auto desc = src_resource->GetDesc ();

    UINT width = MIN ((UINT) desc.Width, priv->config.resolution.Width);
    UINT height = MIN ((UINT) desc.Height, priv->config.resolution.Height);

    for (guint i = 0; i < 2; i++) {
      GstD3D12CopyTextureRegionArgs args;
      memset (&args, 0, sizeof (args));

      args.src = CD3DX12_TEXTURE_COPY_LOCATION (src_resource, i);
      args.dst = CD3DX12_TEXTURE_COPY_LOCATION (dst_resource, i);

      src_box[i].left = 0;
      src_box[i].top = 0;
      src_box[i].front = 0;
      src_box[i].back = 1;

      if (i == 0) {
        src_box[i].right = width;
        src_box[i].bottom = height;
      } else {
        src_box[i].right = width / 2;
        src_box[i].bottom = height / 2;
      }

      args.src_box = &src_box[i];
      copy_args.push_back (args);
    }

    guint64 fence_val = 0;
    gst_d3d12_device_copy_texture_region (self->device, copy_args.size (),
        copy_args.data (), D3D12_COMMAND_LIST_TYPE_DIRECT, &fence_val);
    gst_d3d12_buffer_after_write (upload, fence_val);
  } else {
    GstVideoFrame src_frame, dst_frame;
    if (!gst_video_frame_map (&src_frame, info, buffer, GST_MAP_READ)) {
      GST_ERROR_OBJECT (self, "Couldn't map frame");
      gst_buffer_unref (upload);
      return nullptr;
    }

    if (!gst_video_frame_map (&dst_frame, info, upload, GST_MAP_WRITE)) {
      GST_ERROR_OBJECT (self, "Couldn't map frame");
      gst_video_frame_unmap (&src_frame);
      gst_buffer_unref (upload);
      return nullptr;
    }

    for (guint i = 0; i < GST_VIDEO_FRAME_N_PLANES (&src_frame); i++) {
      auto src_width_in_bytes = GST_VIDEO_FRAME_COMP_WIDTH (&src_frame, i) *
          GST_VIDEO_FRAME_COMP_PSTRIDE (&src_frame, i);
      auto src_height = GST_VIDEO_FRAME_COMP_HEIGHT (&src_frame, i);
      auto src_stride = GST_VIDEO_FRAME_PLANE_STRIDE (&src_frame, i);

      auto dst_width_in_bytes = GST_VIDEO_FRAME_COMP_WIDTH (&dst_frame, i) *
          GST_VIDEO_FRAME_COMP_PSTRIDE (&dst_frame, i);
      auto dst_height = GST_VIDEO_FRAME_COMP_HEIGHT (&dst_frame, i);
      auto dst_stride = GST_VIDEO_FRAME_PLANE_STRIDE (&dst_frame, i);

      auto width_in_bytes = MIN (src_width_in_bytes, dst_width_in_bytes);
      auto height = MIN (src_height, dst_height);
      auto src_data = (guint8 *) GST_VIDEO_FRAME_PLANE_DATA (&src_frame, i);
      auto dst_data = (guint8 *) GST_VIDEO_FRAME_PLANE_DATA (&dst_frame, i);

      for (guint j = 0; j < height; j++) {
        memcpy (dst_data, src_data, width_in_bytes);
        dst_data += dst_stride;
        src_data += src_stride;
      }
    }

    gst_video_frame_unmap (&dst_frame);
    gst_video_frame_unmap (&src_frame);

    GstMapInfo map_info;
    mem = gst_buffer_peek_memory (upload, 0);
    if (!gst_memory_map (mem, &map_info,
            (GstMapFlags) (GST_MAP_READ | GST_MAP_D3D12))) {
      GST_ERROR_OBJECT (self, "Couldn't map memory");
      gst_buffer_unref (upload);
      return nullptr;
    }

    gst_memory_unmap (mem, &map_info);
  }

  return upload;
}

static void
gst_d3d12_encoder_build_command (GstD3D12Encoder * self,
    const D3D12_VIDEO_ENCODER_ENCODEFRAME_INPUT_ARGUMENTS * in_args,
    const D3D12_VIDEO_ENCODER_ENCODEFRAME_OUTPUT_ARGUMENTS * out_args,
    const D3D12_VIDEO_ENCODER_RESOLVE_METADATA_INPUT_ARGUMENTS * meta_in_args,
    const D3D12_VIDEO_ENCODER_RESOLVE_METADATA_OUTPUT_ARGUMENTS * meta_out_args,
    GstD3D12FenceData * fence_data,
    ID3D12VideoEncodeCommandList2 * command_list)
{
  auto priv = self->priv;
  const auto & format_info = priv->format_info;
  auto array_of_textures = priv->array_of_textures;

  std::vector < D3D12_RESOURCE_BARRIER > pre_enc_barrier;
  std::vector < D3D12_RESOURCE_BARRIER > post_enc_barrier;

  pre_enc_barrier.
      push_back (CD3DX12_RESOURCE_BARRIER::Transition (in_args->pInputFrame,
          D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_VIDEO_ENCODE_READ));
  post_enc_barrier.
      push_back (CD3DX12_RESOURCE_BARRIER::Transition (in_args->pInputFrame,
          D3D12_RESOURCE_STATE_VIDEO_ENCODE_READ, D3D12_RESOURCE_STATE_COMMON));

  /* Reference picture barriers */
  if (in_args->PictureControlDesc.ReferenceFrames.NumTexture2Ds > 0) {
    if (array_of_textures) {
      for (UINT i = 0;
          i < in_args->PictureControlDesc.ReferenceFrames.NumTexture2Ds; i++) {
        auto ref_pic =
            in_args->PictureControlDesc.ReferenceFrames.ppTexture2Ds[i];
        ref_pic->AddRef ();
        gst_d3d12_fence_data_add_notify_com (fence_data, ref_pic);
        pre_enc_barrier.push_back (CD3DX12_RESOURCE_BARRIER::
            Transition (ref_pic, D3D12_RESOURCE_STATE_COMMON,
                D3D12_RESOURCE_STATE_VIDEO_ENCODE_READ));
        post_enc_barrier.push_back (CD3DX12_RESOURCE_BARRIER::
            Transition (ref_pic, D3D12_RESOURCE_STATE_VIDEO_ENCODE_READ,
                D3D12_RESOURCE_STATE_COMMON));
      }
    } else {
      auto ref_pic =
          in_args->PictureControlDesc.ReferenceFrames.ppTexture2Ds[0];
      ref_pic->AddRef ();
      gst_d3d12_fence_data_add_notify_com (fence_data, ref_pic);
      auto ref_pic_desc = ref_pic->GetDesc ();

      for (UINT i = 0;
          i < in_args->PictureControlDesc.ReferenceFrames.NumTexture2Ds; i++) {
        UINT mip_slice, plane_slice, array_slice;
        D3D12DecomposeSubresource (in_args->PictureControlDesc.
            ReferenceFrames.pSubresources[i], ref_pic_desc.MipLevels,
            ref_pic_desc.DepthOrArraySize, mip_slice, array_slice, plane_slice);

        for (UINT plane = 0; plane < format_info.PlaneCount; plane++) {
          UINT subresource = D3D12CalcSubresource (mip_slice, array_slice,
              plane, ref_pic_desc.MipLevels,
              ref_pic_desc.DepthOrArraySize);

          pre_enc_barrier.push_back (CD3DX12_RESOURCE_BARRIER::
              Transition (ref_pic, D3D12_RESOURCE_STATE_COMMON,
                  D3D12_RESOURCE_STATE_VIDEO_ENCODE_READ, subresource));
          post_enc_barrier.push_back (CD3DX12_RESOURCE_BARRIER::
              Transition (ref_pic, D3D12_RESOURCE_STATE_VIDEO_ENCODE_READ,
                  D3D12_RESOURCE_STATE_COMMON, subresource));
        }
      }
    }
  }

  /* Reconstructed picture barries */
  if (out_args->ReconstructedPicture.pReconstructedPicture) {
    out_args->ReconstructedPicture.pReconstructedPicture->AddRef ();
    gst_d3d12_fence_data_add_notify_com (fence_data,
        out_args->ReconstructedPicture.pReconstructedPicture);

    if (array_of_textures) {
      pre_enc_barrier.push_back (CD3DX12_RESOURCE_BARRIER::
          Transition (out_args->ReconstructedPicture.pReconstructedPicture,
              D3D12_RESOURCE_STATE_COMMON,
              D3D12_RESOURCE_STATE_VIDEO_ENCODE_WRITE));

      post_enc_barrier.push_back (CD3DX12_RESOURCE_BARRIER::
          Transition (out_args->ReconstructedPicture.pReconstructedPicture,
              D3D12_RESOURCE_STATE_VIDEO_ENCODE_WRITE,
              D3D12_RESOURCE_STATE_COMMON));
    } else {
      auto recon_pic_desc =
          out_args->ReconstructedPicture.pReconstructedPicture->GetDesc ();
      UINT mip_slice, plane_slice, array_slice;

      D3D12DecomposeSubresource (out_args->
          ReconstructedPicture.ReconstructedPictureSubresource,
          recon_pic_desc.MipLevels, recon_pic_desc.DepthOrArraySize, mip_slice,
          array_slice, plane_slice);

      for (UINT plane = 0; plane < format_info.PlaneCount; plane++) {
        UINT subresource = D3D12CalcSubresource (mip_slice, array_slice,
            plane, recon_pic_desc.MipLevels,
            recon_pic_desc.DepthOrArraySize);

        pre_enc_barrier.push_back (CD3DX12_RESOURCE_BARRIER::
            Transition (out_args->ReconstructedPicture.pReconstructedPicture,
                D3D12_RESOURCE_STATE_COMMON,
                D3D12_RESOURCE_STATE_VIDEO_ENCODE_WRITE, subresource));

        post_enc_barrier.push_back (CD3DX12_RESOURCE_BARRIER::
            Transition (out_args->ReconstructedPicture.pReconstructedPicture,
                D3D12_RESOURCE_STATE_VIDEO_ENCODE_WRITE,
                D3D12_RESOURCE_STATE_COMMON, subresource));
      }
    }
  }

  pre_enc_barrier.
      push_back (CD3DX12_RESOURCE_BARRIER::Transition (out_args->Bitstream.
          pBuffer, D3D12_RESOURCE_STATE_COMMON,
          D3D12_RESOURCE_STATE_VIDEO_ENCODE_WRITE));
  pre_enc_barrier.
      push_back (CD3DX12_RESOURCE_BARRIER::
      Transition (out_args->EncoderOutputMetadata.pBuffer,
          D3D12_RESOURCE_STATE_COMMON,
          D3D12_RESOURCE_STATE_VIDEO_ENCODE_WRITE));

  command_list->ResourceBarrier (pre_enc_barrier.size (),
      pre_enc_barrier.data ());

  auto encoder = priv->session->encoder;
  auto heap = priv->session->heap;

  command_list->EncodeFrame (encoder.Get (), heap.Get (), in_args, out_args);
  gst_d3d12_fence_data_add_notify_com (fence_data, encoder.Detach ());
  gst_d3d12_fence_data_add_notify_com (fence_data, heap.Detach ());

  post_enc_barrier.
      push_back (CD3DX12_RESOURCE_BARRIER::Transition (out_args->Bitstream.
          pBuffer, D3D12_RESOURCE_STATE_VIDEO_ENCODE_WRITE,
          D3D12_RESOURCE_STATE_COMMON));
  post_enc_barrier.
      push_back (CD3DX12_RESOURCE_BARRIER::
      Transition (out_args->EncoderOutputMetadata.pBuffer,
          D3D12_RESOURCE_STATE_VIDEO_ENCODE_WRITE,
          D3D12_RESOURCE_STATE_VIDEO_ENCODE_READ));
  post_enc_barrier.push_back (CD3DX12_RESOURCE_BARRIER::
      Transition (meta_out_args->ResolvedLayoutMetadata.pBuffer,
          D3D12_RESOURCE_STATE_COMMON,
          D3D12_RESOURCE_STATE_VIDEO_ENCODE_WRITE));

  command_list->ResourceBarrier (post_enc_barrier.size (),
      post_enc_barrier.data ());

  command_list->ResolveEncoderOutputMetadata (meta_in_args, meta_out_args);

  std::vector < D3D12_RESOURCE_BARRIER > post_resolve_barrier;
  post_resolve_barrier.push_back (CD3DX12_RESOURCE_BARRIER::
      Transition (out_args->EncoderOutputMetadata.pBuffer,
          D3D12_RESOURCE_STATE_VIDEO_ENCODE_READ, D3D12_RESOURCE_STATE_COMMON));
  post_resolve_barrier.push_back (CD3DX12_RESOURCE_BARRIER::
      Transition (meta_out_args->ResolvedLayoutMetadata.pBuffer,
          D3D12_RESOURCE_STATE_VIDEO_ENCODE_WRITE,
          D3D12_RESOURCE_STATE_COMMON));

  command_list->ResourceBarrier (post_resolve_barrier.size (),
      post_resolve_barrier.data ());
}

static gboolean
gst_d3d12_encoder_resolve_bitstream (GstD3D12Encoder * self,
    ID3D12Resource * resolved_metadata, ID3D12Resource * bitstream,
    GstBuffer * output)
{
  auto priv = self->priv;
  guint8 *map_data;
  HRESULT hr;

  g_assert (output);

  CD3DX12_RANGE zero_range (0, 0);

  hr = resolved_metadata->Map (0, nullptr, (void **) &map_data);
  if (!gst_d3d12_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Couldn't map metadata");
    return FALSE;
  }

  D3D12_VIDEO_ENCODER_OUTPUT_METADATA output_meta =
      *((D3D12_VIDEO_ENCODER_OUTPUT_METADATA *) map_data);
  GST_TRACE_OBJECT (self,
      "EncodeErrorFlags: %" G_GUINT64_FORMAT ", "
      "EncodeStats.AverageQP: %" G_GUINT64_FORMAT ", "
      "EncodeStats.IntraCodingUnitsCount: %" G_GUINT64_FORMAT ", "
      "EncodeStats.InterCodingUnitsCount: %" G_GUINT64_FORMAT ", "
      "EncodeStats.SkipCodingUnitsCount: %" G_GUINT64_FORMAT ", "
      "EncodeStats.AverageMotionEstimationXDirection: %" G_GUINT64_FORMAT ", "
      "EncodeStats.AverageMotionEstimationYDirection: %" G_GUINT64_FORMAT ", "
      "EncodedBitstreamWrittenBytesCount: %" G_GUINT64_FORMAT ", "
      "WrittenSubregionsCount: %" G_GUINT64_FORMAT,
      output_meta.EncodeErrorFlags,
      output_meta.EncodeStats.AverageQP,
      output_meta.EncodeStats.IntraCodingUnitsCount,
      output_meta.EncodeStats.InterCodingUnitsCount,
      output_meta.EncodeStats.SkipCodingUnitsCount,
      output_meta.EncodeStats.AverageMotionEstimationXDirection,
      output_meta.EncodeStats.AverageMotionEstimationYDirection,
      output_meta.EncodedBitstreamWrittenBytesCount,
      output_meta.WrittenSubregionsCount);

  if (output_meta.WrittenSubregionsCount == 0 ||
      output_meta.EncodedBitstreamWrittenBytesCount == 0) {
    GST_ERROR_OBJECT (self, "No written data");
    resolved_metadata->Unmap (0, &zero_range);

    return FALSE;
  }

  if (output_meta.EncodeErrorFlags !=
      D3D12_VIDEO_ENCODER_ENCODE_ERROR_FLAG_NO_ERROR) {
    if ((output_meta.EncodeErrorFlags &
            D3D12_VIDEO_ENCODER_ENCODE_ERROR_FLAG_CODEC_PICTURE_CONTROL_NOT_SUPPORTED)
        != 0) {
      GST_ERROR_OBJECT (self, "Picture control not supported");
    }

    if ((output_meta.EncodeErrorFlags &
            D3D12_VIDEO_ENCODER_ENCODE_ERROR_FLAG_SUBREGION_LAYOUT_CONFIGURATION_NOT_SUPPORTED)
        != 0) {
      GST_ERROR_OBJECT (self, "Subregion layout not supported");
    }

    if ((output_meta.EncodeErrorFlags &
            D3D12_VIDEO_ENCODER_ENCODE_ERROR_FLAG_INVALID_REFERENCE_PICTURES) !=
        0) {
      GST_ERROR_OBJECT (self, "Invalid reference picture");
    }

    if ((output_meta.EncodeErrorFlags &
            D3D12_VIDEO_ENCODER_ENCODE_ERROR_FLAG_RECONFIGURATION_REQUEST_NOT_SUPPORTED)
        != 0) {
      GST_ERROR_OBJECT (self, "Reconfigure not supported");
    }

    if ((output_meta.EncodeErrorFlags &
            D3D12_VIDEO_ENCODER_ENCODE_ERROR_FLAG_INVALID_METADATA_BUFFER_SOURCE)
        != 0) {
      GST_ERROR_OBJECT (self, "Invalid metadata buffer source");
    }
    resolved_metadata->Unmap (0, &zero_range);

    return FALSE;
  }

  map_data += sizeof (D3D12_VIDEO_ENCODER_OUTPUT_METADATA);
  priv->subregions.clear ();
  UINT64 total_subregion_size = 0;
  for (guint i = 0; i < output_meta.WrittenSubregionsCount; i++) {
    auto subregion =
        *((D3D12_VIDEO_ENCODER_FRAME_SUBREGION_METADATA *) map_data);

    GST_TRACE_OBJECT (self, "Subregion %d, bSize: %" G_GUINT64_FORMAT ", "
        "bStartOffset: %" G_GUINT64_FORMAT ", bHeaderSize: %" G_GUINT64_FORMAT,
        i, subregion.bSize, subregion.bStartOffset, subregion.bHeaderSize);

    subregion.bStartOffset += total_subregion_size;

    priv->subregions.push_back (subregion);
    total_subregion_size += subregion.bSize;
    map_data += sizeof (D3D12_VIDEO_ENCODER_FRAME_SUBREGION_METADATA);
  }

  resolved_metadata->Unmap (0, &zero_range);

  hr = bitstream->Map (0, nullptr, (void **) &map_data);
  if (!gst_d3d12_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Couldn't map bitstream");
    return FALSE;
  }

  auto mem = gst_allocator_alloc (nullptr, total_subregion_size, nullptr);
  GstMapInfo map_info;

  gst_memory_map (mem, &map_info, GST_MAP_WRITE);
  auto data = (guint8 *) map_info.data;
  for (size_t i = 0; i < priv->subregions.size (); i++) {
    const auto & subregion = priv->subregions[i];
    memcpy (data, map_data + subregion.bStartOffset, subregion.bSize);
    data += subregion.bSize;
  }
  gst_memory_unmap (mem, &map_info);
  bitstream->Unmap (0, &zero_range);

  gst_buffer_append_memory (output, mem);

  return TRUE;
}

static gpointer
gst_d3d12_encoder_output_loop (GstD3D12Encoder * self)
{
  auto priv = self->priv;
  auto encoder = GST_VIDEO_ENCODER (self);

  GST_DEBUG_OBJECT (self, "Entering output thread");

  HANDLE event_handle = CreateEventEx (nullptr, nullptr, 0, EVENT_ALL_ACCESS);

  while (true) {
    EncoderOutputData output_data;
    {
      GST_LOG_OBJECT (self, "Waiting for output data");
      std::unique_lock < std::mutex > lk (priv->session->queue_lock);
      while (gst_queue_array_is_empty (priv->session->output_queue))
        priv->session->queue_cond.wait (lk);

      output_data = *((EncoderOutputData *)
          gst_queue_array_pop_head_struct (priv->session->output_queue));
    }

    if (!output_data.frame) {
      GST_DEBUG_OBJECT (self, "Got terminate data");
      break;
    }

    GST_LOG_OBJECT (self, "Processing output %" G_GUINT64_FORMAT,
        output_data.fence_val);

    gst_d3d12_command_queue_fence_wait (priv->cmd->queue, output_data.fence_val,
        event_handle);

    if (priv->flushing) {
      GST_DEBUG_OBJECT (self, "We are flushing");
      gst_d3d12_encoder_buffer_unref (output_data.buffer);
      gst_clear_buffer (&output_data.frame->output_buffer);
      gst_video_encoder_finish_frame (encoder, output_data.frame);
    } else if (priv->last_flow == GST_FLOW_OK) {
      ComPtr < ID3D12Resource > resolved_metadata;
      ComPtr < ID3D12Resource > bitstream;

      gst_d3d12_encoder_buffer_get_resolved_metadata (output_data.buffer,
          &resolved_metadata);
      gst_d3d12_encoder_buffer_get_bitstream (output_data.buffer, &bitstream);

      auto frame = output_data.frame;
      if (!frame->output_buffer)
        frame->output_buffer = gst_buffer_new ();

      auto resolve_ret = gst_d3d12_encoder_resolve_bitstream (self,
          resolved_metadata.Get (),
          bitstream.Get (), frame->output_buffer);
      gst_d3d12_encoder_buffer_unref (output_data.buffer);

      if (!resolve_ret) {
        GST_ERROR_OBJECT (self, "Couldn't resolve bitstream buffer");
        priv->last_flow = GST_FLOW_ERROR;
        gst_clear_buffer (&frame->output_buffer);
        gst_video_encoder_finish_frame (encoder, frame);
      } else {
        /* TODO: calculate dts in case of bframe is used */
        frame->dts = frame->pts;

        priv->last_flow = gst_video_encoder_finish_frame (encoder, frame);
        if (priv->last_flow != GST_FLOW_OK) {
          GST_WARNING_OBJECT (self, "Last flow was %s",
              gst_flow_get_name (priv->last_flow));
        }
      }
    } else {
      GST_DEBUG_OBJECT (self, "Dropping framem last flow return was %s",
          gst_flow_get_name (priv->last_flow));
      gst_d3d12_encoder_buffer_unref (output_data.buffer);
      gst_clear_buffer (&output_data.frame->output_buffer);
      gst_video_encoder_finish_frame (encoder, output_data.frame);
    }
  }

  GST_DEBUG_OBJECT (self, "Leaving output thread");

  CloseHandle (event_handle);

  return nullptr;
}

static GstFlowReturn
gst_d3d12_encoder_handle_frame (GstVideoEncoder * encoder,
    GstVideoCodecFrame * frame)
{
  auto self = GST_D3D12_ENCODER (encoder);
  auto priv = self->priv;
  auto & config = priv->config;
  auto klass = GST_D3D12_ENCODER_GET_CLASS (self);

  if (!priv->session) {
    GST_ERROR_OBJECT (self, "Encoding session is not configured");
    return GST_FLOW_ERROR;
  }

  if (!priv->session->encoder || !priv->session->heap) {
    GST_ERROR_OBJECT (self, "Previous reconfigure failed");
    return GST_FLOW_ERROR;
  }

  if (priv->last_flow != GST_FLOW_OK) {
    GST_WARNING_OBJECT (self, "Last flow was %s",
        gst_flow_get_name (priv->last_flow));
    gst_video_encoder_finish_frame (encoder, frame);
    return priv->last_flow;
  }

  auto upload = gst_d3d12_encoder_upload_frame (self, frame->input_buffer);
  if (!upload) {
    gst_video_encoder_finish_frame (encoder, frame);
    return GST_FLOW_ERROR;
  }

  GstD3D12FenceData *fence_data;
  gst_d3d12_fence_data_pool_acquire (priv->fence_data_pool, &fence_data);
  gst_d3d12_fence_data_add_notify_mini_object (fence_data, upload);

  GstD3D12CommandAllocator *gst_ca;
  if (!gst_d3d12_command_allocator_pool_acquire (priv->cmd->ca_pool, &gst_ca)) {
    GST_ERROR_OBJECT (self, "Couldn't acquire command allocator");
    gst_d3d12_fence_data_unref (fence_data);
    gst_video_encoder_finish_frame (encoder, frame);
    return GST_FLOW_ERROR;
  }

  gst_d3d12_fence_data_add_notify_mini_object (fence_data, gst_ca);

  ComPtr < ID3D12CommandAllocator > ca;
  gst_d3d12_command_allocator_get_handle (gst_ca, &ca);
  auto hr = ca->Reset ();
  if (!gst_d3d12_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Couldn't reset command allocator");
    gst_d3d12_fence_data_unref (fence_data);
    gst_video_encoder_finish_frame (encoder, frame);
    return GST_FLOW_ERROR;
  }

  if (!priv->cmd->cl) {
    auto device = gst_d3d12_device_get_device_handle (self->device);
    hr = device->CreateCommandList (0, D3D12_COMMAND_LIST_TYPE_VIDEO_ENCODE,
        ca.Get (), nullptr, IID_PPV_ARGS (&priv->cmd->cl));
  } else {
    hr = priv->cmd->cl->Reset (ca.Get ());
  }

  if (!gst_d3d12_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Couldn't reset command list");
    gst_d3d12_fence_data_unref (fence_data);
    gst_video_encoder_finish_frame (encoder, frame);
    return GST_FLOW_ERROR;
  }

  auto video_device = priv->cmd->video_device.Get ();
  D3D12_VIDEO_ENCODER_ENCODEFRAME_INPUT_ARGUMENTS in_args = { };
  D3D12_VIDEO_ENCODER_ENCODEFRAME_OUTPUT_ARGUMENTS out_args = { };
  D3D12_VIDEO_ENCODER_RESOLVE_METADATA_INPUT_ARGUMENTS meta_in_args = { };
  D3D12_VIDEO_ENCODER_RESOLVE_METADATA_OUTPUT_ARGUMENTS meta_out_args = { };
  auto prev_max_subregions = config.max_subregions;
  gboolean need_new_session = FALSE;
  if (!klass->start_frame (self, video_device, frame,
          &in_args.SequenceControlDesc,
          &in_args.PictureControlDesc, &out_args.ReconstructedPicture, &config,
          &need_new_session)) {
    GST_ERROR_OBJECT (self, "Start frame failed");
    gst_d3d12_fence_data_unref (fence_data);
    gst_video_encoder_finish_frame (encoder, frame);
    return GST_FLOW_ERROR;
  }

  if (need_new_session) {
    GST_DEBUG_OBJECT (self, "Need new encoding session");
    in_args.SequenceControlDesc.Flags =
        D3D12_VIDEO_ENCODER_SEQUENCE_CONTROL_FLAG_NONE;

    priv->session->encoder = nullptr;
    priv->session->heap = nullptr;

    auto & desc = priv->encoder_desc;
    desc.EncodeCodec = klass->codec;
    desc.EncodeProfile = config.profile_desc;
    desc.InputFormat = priv->format_info.Format;
    desc.CodecConfiguration = config.codec_config;
    desc.MaxMotionEstimationPrecision =
        D3D12_VIDEO_ENCODER_MOTION_ESTIMATION_PRECISION_MODE_MAXIMUM;

    hr = video_device->CreateVideoEncoder (&desc,
        IID_PPV_ARGS (&priv->session->encoder));
    if (!gst_d3d12_result (hr, self->device)) {
      GST_ERROR_OBJECT (self, "Couldn't create encoder");
      gst_d3d12_fence_data_unref (fence_data);
      gst_video_encoder_finish_frame (encoder, frame);
      return GST_FLOW_ERROR;
    }

    auto & heap_desc = priv->heap_desc;
    heap_desc.EncodeCodec = klass->codec;
    heap_desc.EncodeProfile = config.profile_desc;
    heap_desc.EncodeLevel = config.level;
    heap_desc.ResolutionsListCount = 1;
    heap_desc.pResolutionList = &config.resolution;
    hr = video_device->CreateVideoEncoderHeap (&heap_desc,
        IID_PPV_ARGS (&priv->session->heap));
    if (!gst_d3d12_result (hr, self->device)) {
      GST_ERROR_OBJECT (self, "Couldn't create encoder heap");
      gst_d3d12_fence_data_unref (fence_data);
      gst_video_encoder_finish_frame (encoder, frame);
      return GST_FLOW_ERROR;
    }
  }

  if (prev_max_subregions != config.max_subregions) {
    gst_clear_object (&priv->session->encoder_pool);
    const auto & resource_req = priv->resource_req;

    GST_DEBUG_OBJECT (self, "Subregion count changed %d -> %d",
        prev_max_subregions, config.max_subregions);

    guint resolved_metadata_size;
    resolved_metadata_size = sizeof (D3D12_VIDEO_ENCODER_OUTPUT_METADATA)
        + sizeof (D3D12_VIDEO_ENCODER_FRAME_SUBREGION_METADATA) *
        config.max_subregions;
    if (priv->resource_req.EncoderMetadataBufferAccessAlignment > 1) {
      resolved_metadata_size =
          ((resolved_metadata_size +
              resource_req.EncoderMetadataBufferAccessAlignment - 1) /
          resource_req.EncoderMetadataBufferAccessAlignment) *
          resource_req.EncoderMetadataBufferAccessAlignment;
    }

    /* TODO: calculate bitstream buffer size */
    guint bitstream_size = 1024 * 1024 * 8;
    priv->session->encoder_pool =
        gst_d3d12_encoder_buffer_pool_new (self->device,
        resource_req.MaxEncoderOutputMetadataBufferSize,
        resolved_metadata_size, bitstream_size, ASYNC_DEPTH);
  }

  GstD3D12EncoderBuffer *encoder_buf;
  GST_VIDEO_ENCODER_STREAM_UNLOCK (self);
  if (!gst_d3d12_encoder_buffer_pool_acquire (priv->session->encoder_pool,
          &encoder_buf)) {
    GST_ERROR_OBJECT (self, "Couldn't acquire bitstream buffer");
    GST_VIDEO_ENCODER_STREAM_LOCK (self);
    klass->end_frame (self);
    gst_clear_buffer (&frame->output_buffer);
    gst_d3d12_fence_data_unref (fence_data);
    gst_video_encoder_finish_frame (encoder, frame);
    return GST_FLOW_ERROR;
  }
  GST_VIDEO_ENCODER_STREAM_LOCK (self);

  gst_d3d12_fence_data_add_notify_mini_object (fence_data, encoder_buf);

  ComPtr < ID3D12Resource > metadata;
  ComPtr < ID3D12Resource > resolved_metadata;
  ComPtr < ID3D12Resource > bitstream;
  gst_d3d12_encoder_buffer_get_metadata (encoder_buf, &metadata);
  gst_d3d12_encoder_buffer_get_resolved_metadata (encoder_buf,
      &resolved_metadata);
  gst_d3d12_encoder_buffer_get_bitstream (encoder_buf, &bitstream);

  auto mem = (GstD3D12Memory *) gst_buffer_peek_memory (upload, 0);
  auto resource = gst_d3d12_memory_get_resource_handle (mem);

  in_args.pInputFrame = resource;
  in_args.InputFrameSubresource = 0;
  in_args.CurrentFrameBitstreamMetadataSize = 0;

  out_args.Bitstream.pBuffer = bitstream.Get ();
  out_args.Bitstream.FrameStartOffset = 0;
  out_args.EncoderOutputMetadata.pBuffer = metadata.Get ();
  out_args.EncoderOutputMetadata.Offset = 0;

  meta_in_args.EncoderCodec = klass->codec;
  meta_in_args.EncoderProfile = config.profile_desc;
  meta_in_args.EncoderInputFormat = DXGI_FORMAT_NV12;
  meta_in_args.EncodedPictureEffectiveResolution = config.resolution;
  meta_in_args.HWLayoutMetadata.pBuffer = metadata.Get ();
  meta_in_args.HWLayoutMetadata.Offset = 0;

  meta_out_args.ResolvedLayoutMetadata.pBuffer = resolved_metadata.Get ();
  meta_out_args.ResolvedLayoutMetadata.Offset = 0;

  auto cl = priv->cmd->cl;
  gst_d3d12_encoder_build_command (self, &in_args, &out_args, &meta_in_args,
      &meta_out_args, fence_data, cl.Get ());
  hr = cl->Close ();

  klass->end_frame (self);

  if (!gst_d3d12_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Couldn't close command list");
    gst_d3d12_fence_data_unref (fence_data);
    gst_clear_buffer (&frame->output_buffer);
    gst_video_encoder_finish_frame (encoder, frame);
    return GST_FLOW_ERROR;
  }

  auto completed = gst_d3d12_device_get_completed_value (self->device,
      D3D12_COMMAND_LIST_TYPE_DIRECT);
  if (completed < mem->fence_value) {
    auto queue = gst_d3d12_device_get_command_queue (self->device,
        D3D12_COMMAND_LIST_TYPE_DIRECT);
    ComPtr < ID3D12Fence > fence;
    gst_d3d12_command_queue_get_fence (queue, &fence);
    gst_d3d12_command_queue_execute_wait (priv->cmd->queue, fence.Get (),
        mem->fence_value);
  }

  ID3D12CommandList *cmd_list[] = { cl.Get () };
  hr = gst_d3d12_command_queue_execute_command_lists (priv->cmd->queue,
      1, cmd_list, &priv->cmd->fence_val);
  if (!gst_d3d12_result (hr, self->device)) {
    GST_ERROR_OBJECT (self, "Couldn't execute command list");
    gst_d3d12_fence_data_unref (fence_data);
    gst_clear_buffer (&frame->output_buffer);
    gst_video_encoder_finish_frame (encoder, frame);
    return GST_FLOW_ERROR;
  }

  gst_d3d12_command_queue_set_notify (priv->cmd->queue, priv->cmd->fence_val,
      fence_data, (GDestroyNotify) gst_d3d12_fence_data_unref);

  auto output_data = EncoderOutputData ();
  output_data.frame = frame;
  output_data.buffer = gst_d3d12_encoder_buffer_ref (encoder_buf);
  output_data.fence_val = priv->cmd->fence_val;

  GST_LOG_OBJECT (self, "Enqueue data %" G_GUINT64_FORMAT,
      priv->cmd->fence_val);

  GST_VIDEO_ENCODER_STREAM_UNLOCK (self);
  {
    std::lock_guard < std::mutex > lk (priv->session->queue_lock);
    gst_queue_array_push_tail_struct (priv->session->output_queue,
        &output_data);
    priv->session->queue_cond.notify_one ();
  }
  GST_VIDEO_ENCODER_STREAM_LOCK (self);

  if (!priv->output_thread) {
    GST_DEBUG_OBJECT (self, "Spawning output thread");
    priv->output_thread = g_thread_new ("GstD3D12H264EncLoop",
        (GThreadFunc) gst_d3d12_encoder_output_loop, self);
  }

  return priv->last_flow;
}

static GstFlowReturn
gst_d3d12_encoder_finish (GstVideoEncoder * encoder)
{
  auto self = GST_D3D12_ENCODER (encoder);

  GST_DEBUG_OBJECT (self, "Finish");

  gst_d3d12_encoder_drain (self, TRUE);

  return GST_FLOW_OK;
}

static gboolean
gst_d3d12_encoder_flush (GstVideoEncoder * encoder)
{
  auto self = GST_D3D12_ENCODER (encoder);
  auto priv = self->priv;

  GST_DEBUG_OBJECT (self, "Finish");

  priv->flushing = true;
  gst_d3d12_encoder_drain (self, TRUE);
  priv->flushing = false;
  priv->last_flow = GST_FLOW_OK;

  return GST_FLOW_OK;
}

GType
gst_d3d12_encoder_rate_control_get_type (void)
{
  static GType type = 0;
  static const GEnumValue methods[] = {
    {D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_CQP, "Constant QP", "cqp"},
    {D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_CBR, "Constant bitrate", "cbr"},
    {D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_VBR, "Variable bitrate", "vbr"},
    {D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_QVBR,
        "Constant quality variable bitrate", "qvbr"},
    {0, nullptr, nullptr},
  };

  GST_D3D12_CALL_ONCE_BEGIN {
    type = g_enum_register_static ("GstD3D12EncoderRateControl", methods);
  } GST_D3D12_CALL_ONCE_END;

  return type;
}

GType
gst_d3d12_encoder_rate_control_support_get_type (void)
{
  static GType type = 0;
  static const GFlagsValue methods[] = {
    {(1 << D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_CQP), "Constant QP", "cqp"},
    {(1 << D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_CBR), "Constant bitrate",
        "cbr"},
    {(1 << D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_VBR), "Variable bitrate",
        "vbr"},
    {(1 << D3D12_VIDEO_ENCODER_RATE_CONTROL_MODE_QVBR),
        "Constant quality variable bitrate", "qvbr"},
    {0, nullptr, nullptr},
  };

  GST_D3D12_CALL_ONCE_BEGIN {
    type = g_flags_register_static ("GstD3D12EncoderRateControlSupport",
        methods);
  } GST_D3D12_CALL_ONCE_END;

  return type;
}

GType
gst_d3d12_encoder_subregion_layout_get_type (void)
{
  static GType type = 0;
  static const GEnumValue methods[] = {
    {D3D12_VIDEO_ENCODER_FRAME_SUBREGION_LAYOUT_MODE_FULL_FRAME,
        "Full frame without partitioning", "full"},
    {D3D12_VIDEO_ENCODER_FRAME_SUBREGION_LAYOUT_MODE_BYTES_PER_SUBREGION,
        "Bytes per subregion", "bytes"},
    {D3D12_VIDEO_ENCODER_FRAME_SUBREGION_LAYOUT_MODE_SQUARE_UNITS_PER_SUBREGION_ROW_UNALIGNED,
        "Coding units (e.g., macroblock) per subregion", "coding-units"},
    {D3D12_VIDEO_ENCODER_FRAME_SUBREGION_LAYOUT_MODE_UNIFORM_PARTITIONING_ROWS_PER_SUBREGION,
        "Uniform rows per subregion", "rows"},
    {D3D12_VIDEO_ENCODER_FRAME_SUBREGION_LAYOUT_MODE_UNIFORM_PARTITIONING_SUBREGIONS_PER_FRAME,
        "Uniform subregions per frame", "subregions"},
    {0, nullptr, nullptr},
  };

  GST_D3D12_CALL_ONCE_BEGIN {
    type = g_enum_register_static ("GstD3D12EncoderSubregionLayout", methods);
  } GST_D3D12_CALL_ONCE_END;

  return type;
}

GType
gst_d3d12_encoder_subregion_layout_support_get_type (void)
{
  static GType type = 0;
  static const GFlagsValue methods[] = {
    {(1 << D3D12_VIDEO_ENCODER_FRAME_SUBREGION_LAYOUT_MODE_FULL_FRAME),
        "Full frame without partitioning", "full"},
    {(1 << D3D12_VIDEO_ENCODER_FRAME_SUBREGION_LAYOUT_MODE_BYTES_PER_SUBREGION),
        "Bytes per subregion", "bytes"},
    {(1 << D3D12_VIDEO_ENCODER_FRAME_SUBREGION_LAYOUT_MODE_SQUARE_UNITS_PER_SUBREGION_ROW_UNALIGNED),
        "Coding units (e.g., macroblock) per subregion", "coding-units"},
    {(1 << D3D12_VIDEO_ENCODER_FRAME_SUBREGION_LAYOUT_MODE_UNIFORM_PARTITIONING_ROWS_PER_SUBREGION),
        "Uniform rows (in coding-unit) per subregion", "rows"},
    {(1 << D3D12_VIDEO_ENCODER_FRAME_SUBREGION_LAYOUT_MODE_UNIFORM_PARTITIONING_SUBREGIONS_PER_FRAME),
        "Uniform subregions per frame", "subregions"},
    {0, nullptr, nullptr},
  };

  GST_D3D12_CALL_ONCE_BEGIN {
    type = g_flags_register_static ("GstD3D12EncoderSubregionLayoutSupport",
        methods);
  } GST_D3D12_CALL_ONCE_END;

  return type;
}

GType
gst_d3d12_encoder_sei_insert_mode_get_type (void)
{
  static GType type = 0;
  static const GEnumValue modes[] = {
    {GST_D3D12_ENCODER_SEI_INSERT, "Insert", "insert"},
    {GST_D3D12_ENCODER_SEI_INSERT_AND_DROP, "Insert and drop",
        "insert-and-drop"},
    {GST_D3D12_ENCODER_SEI_DISABLED, "Disabled", "disabled"},
    {0, nullptr, nullptr}
  };

  GST_D3D12_CALL_ONCE_BEGIN {
    type = g_enum_register_static ("GstD3D12EncoderSeiInsertMode", modes);
  } GST_D3D12_CALL_ONCE_END;

  return type;
}

gboolean
gst_d3d12_encoder_check_needs_new_session (D3D12_VIDEO_ENCODER_SUPPORT_FLAGS
    support_flags, D3D12_VIDEO_ENCODER_SEQUENCE_CONTROL_FLAGS seq_flags)
{
  bool rc_updated = (seq_flags &
      D3D12_VIDEO_ENCODER_SEQUENCE_CONTROL_FLAG_RATE_CONTROL_CHANGE) != 0;
  bool can_rc_update = CHECK_SUPPORT_FLAG (support_flags,
      RATE_CONTROL_RECONFIGURATION_AVAILABLE);
  if (rc_updated && !can_rc_update)
    return TRUE;

  bool layout_updated = (seq_flags &
      D3D12_VIDEO_ENCODER_SEQUENCE_CONTROL_FLAG_SUBREGION_LAYOUT_CHANGE) != 0;
  bool can_layout_update = CHECK_SUPPORT_FLAG (support_flags,
      SUBREGION_LAYOUT_RECONFIGURATION_AVAILABLE);
  if (layout_updated && !can_layout_update)
    return TRUE;

  bool gop_updated = (seq_flags &
      D3D12_VIDEO_ENCODER_SEQUENCE_CONTROL_FLAG_GOP_SEQUENCE_CHANGE) != 0;
  bool can_gop_update = CHECK_SUPPORT_FLAG (support_flags,
      SEQUENCE_GOP_RECONFIGURATION_AVAILABLE);
  if (gop_updated && !can_gop_update)
    return TRUE;

  return FALSE;
}
