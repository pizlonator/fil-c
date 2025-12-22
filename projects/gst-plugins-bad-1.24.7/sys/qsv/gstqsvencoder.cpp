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

#include "gstqsvencoder.h"
#include <mfxvideo++.h>
#include <string.h>
#include <string>

#ifdef G_OS_WIN32
#include "gstqsvallocator_d3d11.h"

#include <wrl.h>

/* *INDENT-OFF* */
using namespace Microsoft::WRL;
/* *INDENT-ON* */
#else
#include "gstqsvallocator_va.h"
#endif /* G_OS_WIN32 */

GST_DEBUG_CATEGORY_STATIC (gst_qsv_encoder_debug);
#define GST_CAT_DEFAULT gst_qsv_encoder_debug

/**
 * GstQsvCodingOption:
 *
 * Since: 1.22
 */
GType
gst_qsv_coding_option_get_type (void)
{
  static GType coding_opt_type = 0;
  static const GEnumValue coding_opts[] = {
    /**
     * GstQsvCodingOption::unknown:
     *
     * Since: 1.22
     */
    {MFX_CODINGOPTION_UNKNOWN, "Unknown", "unknown"},

    /**
     * GstQsvCodingOption::on:
     *
     * Since: 1.22
     */
    {MFX_CODINGOPTION_ON, "On", "on"},

    /**
     * GstQsvCodingOption::off:
     *
     * Since: 1.22
     */
    {MFX_CODINGOPTION_OFF, "Off", "off"},
    {0, nullptr, nullptr}
  };

  GST_QSV_CALL_ONCE_BEGIN {
    coding_opt_type = g_enum_register_static ("GstQsvCodingOption",
        coding_opts);
  } GST_QSV_CALL_ONCE_END;

  return coding_opt_type;
}

enum
{
  PROP_0,
  PROP_ADAPTER_LUID,
  PROP_DEVICE_PATH,
  PROP_TARGET_USAGE,
  PROP_LOW_LATENCY,
};

#define DEFAULT_TARGET_USAGE MFX_TARGETUSAGE_BALANCED
#define DEFAULT_LOW_LATENCY  FALSE

typedef struct _GstQsvEncoderSurface
{
  mfxFrameSurface1 surface;
  mfxEncodeCtrl encode_control;

  /* array of mfxPayload (e.g., SEI data) associated with this surface */
  GPtrArray *payload;

  /* holds ownership */
  GstQsvFrame *qsv_frame;
} GstQsvEncoderSurface;

typedef struct _GstQsvEncoderTask
{
  mfxSyncPoint sync_point;
  mfxBitstream bitstream;
} GstQsvEncoderTask;

struct _GstQsvEncoderPrivate
{
  GstObject *device;

  GstVideoCodecState *input_state;
  GstQsvAllocator *allocator;

  /* API specific alignment requirement (multiple of 16 or 32) */
  GstVideoInfo aligned_info;

  mfxSession session;
  mfxVideoParam video_param;

  /* List of mfxExtBuffer configured by subclass, subclass will hold
   * allocated memory for each mfxExtBuffer */
  GPtrArray *extra_params;

  MFXVideoENCODE *encoder;
  GstQsvMemoryType mem_type;

  /* Internal buffer pool used to allocate fallback buffer when input buffer
   * is not compatible with expected format/type/resolution etc */
  GstBufferPool *internal_pool;

  /* Array of GstQsvEncoderSurface, holding ownership */
  GArray *surface_pool;
  guint next_surface_index;

  /* Array of GstQsvEncoderTask, holding ownership */
  GArray *task_pool;

  GQueue free_tasks;
  GQueue pending_tasks;

  /* Properties */
  guint target_usage;
  gboolean low_latency;
};

/**
 * GstQsvEncoder:
 *
 * Base class for Intel Quick Sync video encoders
 *
 * Since: 1.22
 */
#define gst_qsv_encoder_parent_class parent_class
G_DEFINE_ABSTRACT_TYPE_WITH_CODE (GstQsvEncoder, gst_qsv_encoder,
    GST_TYPE_VIDEO_ENCODER, G_ADD_PRIVATE (GstQsvEncoder);
    GST_DEBUG_CATEGORY_INIT (gst_qsv_encoder_debug,
        "qsvencoder", 0, "qsvencoder"));

static void gst_qsv_encoder_dispose (GObject * object);
static void gst_qsv_encoder_finalize (GObject * object);
static void gst_qsv_encoder_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_qsv_encoder_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static void gst_qsv_encoder_set_context (GstElement * element,
    GstContext * context);

static gboolean gst_qsv_encoder_open (GstVideoEncoder * encoder);
static gboolean gst_qsv_encoder_stop (GstVideoEncoder * encoder);
static gboolean gst_qsv_encoder_close (GstVideoEncoder * encoder);
static gboolean gst_qsv_encoder_set_format (GstVideoEncoder * encoder,
    GstVideoCodecState * state);
static GstFlowReturn gst_qsv_encoder_handle_frame (GstVideoEncoder * encoder,
    GstVideoCodecFrame * frame);
static GstFlowReturn gst_qsv_encoder_finish (GstVideoEncoder * encoder);
static gboolean gst_qsv_encoder_flush (GstVideoEncoder * encoder);
static gboolean gst_qsv_encoder_sink_query (GstVideoEncoder * encoder,
    GstQuery * query);
static gboolean gst_qsv_encoder_src_query (GstVideoEncoder * encoder,
    GstQuery * query);
static gboolean gst_qsv_encoder_propose_allocation (GstVideoEncoder * encoder,
    GstQuery * query);

static void gst_qsv_encoder_surface_clear (GstQsvEncoderSurface * task);
static void gst_qsv_encoder_task_clear (GstQsvEncoderTask * task);

static void
gst_qsv_encoder_class_init (GstQsvEncoderClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstVideoEncoderClass *videoenc_class = GST_VIDEO_ENCODER_CLASS (klass);
  GParamFlags param_flags = (GParamFlags) (GST_PARAM_DOC_SHOW_DEFAULT |
      GST_PARAM_CONDITIONALLY_AVAILABLE | G_PARAM_READABLE |
      G_PARAM_STATIC_STRINGS);

  object_class->dispose = gst_qsv_encoder_dispose;
  object_class->finalize = gst_qsv_encoder_finalize;
  object_class->set_property = gst_qsv_encoder_set_property;
  object_class->get_property = gst_qsv_encoder_get_property;

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

  g_object_class_install_property (object_class, PROP_TARGET_USAGE,
      g_param_spec_uint ("target-usage", "Target Usage",
          "1: Best quality, 4: Balanced, 7: Best speed",
          1, 7, DEFAULT_TARGET_USAGE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (object_class, PROP_LOW_LATENCY,
      g_param_spec_boolean ("low-latency", "Low Latency",
          "Enables low-latency encoding", DEFAULT_LOW_LATENCY,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  element_class->set_context = GST_DEBUG_FUNCPTR (gst_qsv_encoder_set_context);

  videoenc_class->open = GST_DEBUG_FUNCPTR (gst_qsv_encoder_open);
  videoenc_class->stop = GST_DEBUG_FUNCPTR (gst_qsv_encoder_stop);
  videoenc_class->close = GST_DEBUG_FUNCPTR (gst_qsv_encoder_close);
  videoenc_class->set_format = GST_DEBUG_FUNCPTR (gst_qsv_encoder_set_format);
  videoenc_class->handle_frame =
      GST_DEBUG_FUNCPTR (gst_qsv_encoder_handle_frame);
  videoenc_class->finish = GST_DEBUG_FUNCPTR (gst_qsv_encoder_finish);
  videoenc_class->flush = GST_DEBUG_FUNCPTR (gst_qsv_encoder_flush);
  videoenc_class->sink_query = GST_DEBUG_FUNCPTR (gst_qsv_encoder_sink_query);
  videoenc_class->src_query = GST_DEBUG_FUNCPTR (gst_qsv_encoder_src_query);
  videoenc_class->propose_allocation =
      GST_DEBUG_FUNCPTR (gst_qsv_encoder_propose_allocation);

  gst_type_mark_as_plugin_api (GST_TYPE_QSV_ENCODER, (GstPluginAPIFlags) 0);
  gst_type_mark_as_plugin_api (GST_TYPE_QSV_CODING_OPTION,
      (GstPluginAPIFlags) 0);
}

static void
gst_qsv_encoder_init (GstQsvEncoder * self)
{
  GstQsvEncoderPrivate *priv;

  priv = self->priv =
      (GstQsvEncoderPrivate *) gst_qsv_encoder_get_instance_private (self);

  priv->extra_params = g_ptr_array_sized_new (8);

  priv->surface_pool = g_array_new (FALSE, TRUE, sizeof (GstQsvEncoderSurface));
  g_array_set_clear_func (priv->surface_pool,
      (GDestroyNotify) gst_qsv_encoder_surface_clear);

  priv->task_pool = g_array_new (FALSE, TRUE, sizeof (GstQsvEncoderTask));
  g_array_set_clear_func (priv->task_pool,
      (GDestroyNotify) gst_qsv_encoder_task_clear);

  g_queue_init (&priv->free_tasks);
  g_queue_init (&priv->pending_tasks);

  priv->target_usage = DEFAULT_TARGET_USAGE;
  priv->low_latency = DEFAULT_LOW_LATENCY;
}

static void
gst_qsv_encoder_dispose (GObject * object)
{
  GstQsvEncoder *self = GST_QSV_ENCODER (object);
  GstQsvEncoderPrivate *priv = self->priv;

  gst_clear_object (&priv->device);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_qsv_encoder_finalize (GObject * object)
{
  GstQsvEncoder *self = GST_QSV_ENCODER (object);
  GstQsvEncoderPrivate *priv = self->priv;

  g_ptr_array_unref (priv->extra_params);
  g_array_unref (priv->task_pool);
  g_array_unref (priv->surface_pool);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_qsv_encoder_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstQsvEncoder *self = GST_QSV_ENCODER (object);
  GstQsvEncoderPrivate *priv = self->priv;

  switch (prop_id) {
    case PROP_TARGET_USAGE:
      priv->target_usage = g_value_get_uint (value);
      break;
    case PROP_LOW_LATENCY:
      priv->low_latency = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_qsv_encoder_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstQsvEncoder *self = GST_QSV_ENCODER (object);
  GstQsvEncoderPrivate *priv = self->priv;
  GstQsvEncoderClass *klass = GST_QSV_ENCODER_GET_CLASS (self);

  switch (prop_id) {
    case PROP_ADAPTER_LUID:
      g_value_set_int64 (value, klass->adapter_luid);
      break;
    case PROP_DEVICE_PATH:
      g_value_set_string (value, klass->display_path);
      break;
    case PROP_TARGET_USAGE:
      g_value_set_uint (value, priv->target_usage);
      break;
    case PROP_LOW_LATENCY:
      g_value_set_boolean (value, priv->low_latency);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_qsv_encoder_set_context (GstElement * element, GstContext * context)
{
  GstQsvEncoder *self = GST_QSV_ENCODER (element);
  GstQsvEncoderClass *klass = GST_QSV_ENCODER_GET_CLASS (element);
  GstQsvEncoderPrivate *priv = self->priv;

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
gst_qsv_encoder_open_platform_device (GstQsvEncoder * self)
{
  GstQsvEncoderPrivate *priv = self->priv;
  GstQsvEncoderClass *klass = GST_QSV_ENCODER_GET_CLASS (self);
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
   * If so, we should create encoding session dedicated d3d11 device and
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

  /* NOTE: We never use this mfxFrameAllocator to allocate memory from our side,
   * but required for QSV because:
   * 1) QSV may request memory allocation for encoder's internal usage,
   *   MFX_FOURCC_P8 for example
   * 2) Our mfxFrameAllocator provides bridge layer for
   *   gst_video_frame_{map,unmap} and mfxFrameAllocator::{Lock,Unlock},
   *   including mfxFrameAllocator::GetHDL.
   * 3) GstQsvAllocator provides GstQsvFrame pool, and therefore allocated
   *   GstQsvFrame struct can be re-used without per-frame malloc/free
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
gst_qsv_encoder_open_platform_device (GstQsvEncoder * self)
{
  GstQsvEncoderPrivate *priv = self->priv;
  GstQsvEncoderClass *klass = GST_QSV_ENCODER_GET_CLASS (self);
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
gst_qsv_encoder_open (GstVideoEncoder * encoder)
{
  GstQsvEncoder *self = GST_QSV_ENCODER (encoder);
  GstQsvEncoderPrivate *priv = self->priv;
  GstQsvEncoderClass *klass = GST_QSV_ENCODER_GET_CLASS (self);
  mfxStatus status;

  status = MFXCreateSession (gst_qsv_get_loader (), klass->impl_index,
      &priv->session);
  if (status != MFX_ERR_NONE) {
    GST_ERROR_OBJECT (self, "Failed to create session");
    return FALSE;
  }

  if (!gst_qsv_encoder_open_platform_device (self)) {
    g_clear_pointer (&priv->session, MFXClose);
    gst_clear_object (&priv->allocator);
    gst_clear_object (&priv->device);

    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_qsv_encoder_reset (GstQsvEncoder * self)
{
  GstQsvEncoderPrivate *priv = self->priv;

  if (priv->encoder) {
    delete priv->encoder;
    priv->encoder = nullptr;
  }

  if (priv->internal_pool) {
    gst_buffer_pool_set_active (priv->internal_pool, FALSE);
    gst_clear_object (&priv->internal_pool);
  }

  g_array_set_size (priv->surface_pool, 0);
  g_array_set_size (priv->task_pool, 0);
  g_queue_clear (&priv->free_tasks);
  g_queue_clear (&priv->pending_tasks);

  return TRUE;
}

static gboolean
gst_qsv_encoder_stop (GstVideoEncoder * encoder)
{
  GstQsvEncoder *self = GST_QSV_ENCODER (encoder);
  GstQsvEncoderPrivate *priv = self->priv;

  gst_qsv_encoder_reset (self);
  g_clear_pointer (&priv->input_state, gst_video_codec_state_unref);

  return TRUE;
}

static gboolean
gst_qsv_encoder_close (GstVideoEncoder * encoder)
{
  GstQsvEncoder *self = GST_QSV_ENCODER (encoder);
  GstQsvEncoderPrivate *priv = self->priv;

  g_clear_pointer (&priv->session, MFXClose);
  gst_clear_object (&priv->allocator);
  gst_clear_object (&priv->device);

  return TRUE;
}

static void
gst_qsv_encoder_payload_clear (mfxPayload * payload)
{
  if (!payload)
    return;

  g_free (payload->Data);
  g_free (payload);
}

static void
gst_qsv_encoder_surface_reset (GstQsvEncoderSurface * surface)
{
  if (!surface)
    return;

  gst_clear_qsv_frame (&surface->qsv_frame);
  g_ptr_array_set_size (surface->payload, 0);
  memset (&surface->encode_control, 0, sizeof (mfxEncodeCtrl));
}

static void
gst_qsv_encoder_surface_clear (GstQsvEncoderSurface * surface)
{
  if (!surface)
    return;

  gst_qsv_encoder_surface_reset (surface);
  g_clear_pointer (&surface->payload, g_ptr_array_unref);
  memset (&surface->surface, 0, sizeof (mfxFrameSurface1));
}

static void
gst_qsv_encoder_task_reset (GstQsvEncoder * self, GstQsvEncoderTask * task)
{
  GstQsvEncoderPrivate *priv = self->priv;

  if (!task)
    return;

  task->sync_point = nullptr;
  task->bitstream.DataLength = 0;
  g_queue_push_head (&priv->free_tasks, task);
}

static void
gst_qsv_encoder_task_clear (GstQsvEncoderTask * task)
{
  if (!task)
    return;

  g_clear_pointer (&task->bitstream.Data, g_free);
  memset (&task->bitstream, 0, sizeof (mfxBitstream));
}

static GstQsvEncoderSurface *
gst_qsv_encoder_get_next_surface (GstQsvEncoder * self)
{
  GstQsvEncoderPrivate *priv = self->priv;
  GstQsvEncoderSurface *surface = nullptr;

  for (guint i = priv->next_surface_index; i < priv->surface_pool->len; i++) {
    GstQsvEncoderSurface *iter =
        &g_array_index (priv->surface_pool, GstQsvEncoderSurface, i);

    /* This means surface is still being used by QSV */
    if (iter->surface.Data.Locked > 0)
      continue;

    surface = iter;
    priv->next_surface_index = i;
    goto out;
  }

  for (guint i = 0; i < priv->next_surface_index; i++) {
    GstQsvEncoderSurface *iter =
        &g_array_index (priv->surface_pool, GstQsvEncoderSurface, i);

    /* This means surface is still being used by QSV */
    if (iter->surface.Data.Locked > 0)
      continue;

    surface = iter;
    priv->next_surface_index = i;
    goto out;
  }

  /* Magic number to avoid too large pool size */
  if (priv->surface_pool->len > 64) {
    GST_ERROR_OBJECT (self,
        "No availble surface but pool size is too large already");
    return nullptr;
  }

  /* Something went wrong, increase surface pool size */
  GST_INFO_OBJECT (self, "No useable surfaces, increasing pool size to %d",
      priv->surface_pool->len + 1);

  g_array_set_size (priv->surface_pool, priv->surface_pool->len + 1);
  surface = &g_array_index (priv->surface_pool, GstQsvEncoderSurface,
      priv->surface_pool->len - 1);

  memset (surface, 0, sizeof (GstQsvEncoderSurface));
  surface->surface.Info =
      g_array_index (priv->surface_pool, GstQsvEncoderSurface, 0).surface.Info;
  surface->payload = g_ptr_array_new_with_free_func ((GDestroyNotify)
      gst_qsv_encoder_payload_clear);

out:
  priv->next_surface_index++;
  priv->next_surface_index %= priv->surface_pool->len;

  gst_qsv_encoder_surface_reset (surface);
  return surface;
}

static mfxStatus
gst_qsv_encoder_encode_frame (GstQsvEncoder * self,
    GstQsvEncoderSurface * surface, GstQsvEncoderTask * task, mfxU64 timestamp)
{
  mfxFrameSurface1 *s;
  GstQsvEncoderPrivate *priv = self->priv;
  mfxStatus status;
  guint retry_count = 0;
  /* magic number */
  const guint retry_threshold = 100;
  mfxEncodeCtrl *encode_ctrl;

  if (surface) {
    s = &surface->surface;
    s->Data.MemId = (mfxMemId) surface->qsv_frame;
    s->Data.TimeStamp = timestamp;
    encode_ctrl = &surface->encode_control;
  } else {
    /* draining */
    s = nullptr;
    encode_ctrl = nullptr;
  }

  do {
    status = priv->encoder->EncodeFrameAsync (encode_ctrl,
        s, &task->bitstream, &task->sync_point);

    /* XXX: probably we should try to drain pending tasks if any in this case
     * as documented? */
    if (status == MFX_WRN_DEVICE_BUSY && retry_count < retry_threshold) {
      GST_INFO_OBJECT (self, "GPU is busy, retry count (%d/%d)",
          retry_count, retry_threshold);
      retry_count++;

      /* Magic number 10ms */
      g_usleep (10000);
      continue;
    }

    break;
  } while (TRUE);

  return status;
}

static GstVideoCodecFrame *
gst_qsv_encoder_find_output_frame (GstQsvEncoder * self, GstClockTime pts)
{
  GList *frames, *iter;
  GstVideoCodecFrame *ret = nullptr;
  GstVideoCodecFrame *closest = nullptr;
  guint64 min_pts_abs_diff = 0;

  /* give up, just returns the oldest frame */
  if (!GST_CLOCK_TIME_IS_VALID (pts))
    return gst_video_encoder_get_oldest_frame (GST_VIDEO_ENCODER (self));

  frames = gst_video_encoder_get_frames (GST_VIDEO_ENCODER (self));

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
  } else {
    ret = gst_video_encoder_get_oldest_frame (GST_VIDEO_ENCODER (self));
  }

  if (frames)
    g_list_free_full (frames, (GDestroyNotify) gst_video_codec_frame_unref);

  return ret;
}

static GstFlowReturn
gst_qsv_encoder_finish_frame (GstQsvEncoder * self, GstQsvEncoderTask * task,
    gboolean discard)
{
  GstQsvEncoderPrivate *priv = self->priv;
  GstQsvEncoderClass *klass = GST_QSV_ENCODER_GET_CLASS (self);
  mfxStatus status;
  mfxBitstream *bs;
  GstVideoCodecFrame *frame;
  GstClockTime qsv_pts = GST_CLOCK_TIME_NONE;
  GstClockTime qsv_dts = GST_CLOCK_TIME_NONE;
  GstBuffer *buffer;
  gboolean keyframe = FALSE;
  guint retry_count = 0;
  /* magic number */
  const guint retry_threshold = 100;

  status = MFX_ERR_NONE;
  do {
    /* magic number 100 ms */
    status = MFXVideoCORE_SyncOperation (priv->session, task->sync_point, 100);

    /* Retry up to 10 sec (100 ms x 100 times), that should be enough time for
     * encoding a frame using hardware */
    if (status == MFX_WRN_IN_EXECUTION && retry_count < retry_threshold) {
      GST_DEBUG_OBJECT (self,
          "Operation is still in execution, retry count (%d/%d)",
          retry_count, retry_threshold);
      retry_count++;
      continue;
    }

    break;
  } while (TRUE);

  if (discard) {
    gst_qsv_encoder_task_reset (self, task);
    return GST_FLOW_OK;
  }

  if (status != MFX_ERR_NONE && status != MFX_ERR_NONE_PARTIAL_OUTPUT) {
    gst_qsv_encoder_task_reset (self, task);

    if (status == MFX_ERR_ABORTED) {
      GST_INFO_OBJECT (self, "Operation was aborted");
      return GST_FLOW_FLUSHING;
    }

    GST_WARNING_OBJECT (self, "SyncOperation returned %d (%s)",
        QSV_STATUS_ARGS (status));

    return GST_FLOW_ERROR;
  }

  bs = &task->bitstream;
  qsv_pts = gst_qsv_timestamp_to_gst (bs->TimeStamp);

  /* SDK runtime seems to report zero DTS for all fraems in case of VP9.
   * It sounds SDK bug, but we can workaround it safely because VP9 B-frame is
   * not supported in this implementation.
   *
   * Also we perfer our nanoseconds timestamp instead of QSV's timescale.
   * So let' ignore QSV's timescale for non-{h264,h265} cases.
   *
   * TODO: We may need to use DTS for MPEG2 (not implemented yet)
   */
  if (klass->codec_id == MFX_CODEC_AVC || klass->codec_id == MFX_CODEC_HEVC)
    qsv_dts = gst_qsv_timestamp_to_gst ((mfxU64) bs->DecodeTimeStamp);

  if ((bs->FrameType & MFX_FRAMETYPE_IDR) != 0)
    keyframe = TRUE;

  if (klass->create_output_buffer) {
    buffer = klass->create_output_buffer (self, bs);
  } else {
    buffer = gst_buffer_new_memdup (bs->Data + bs->DataOffset, bs->DataLength);
  }
  gst_qsv_encoder_task_reset (self, task);

  if (!buffer) {
    GST_ERROR_OBJECT (self, "No output buffer");
    return GST_FLOW_ERROR;
  }

  frame = gst_qsv_encoder_find_output_frame (self, qsv_pts);
  if (frame) {
    if (GST_CLOCK_TIME_IS_VALID (qsv_dts)) {
      frame->pts = qsv_pts;
      frame->dts = qsv_dts;
    } else {
      frame->dts = frame->pts;
    }

    frame->output_buffer = buffer;

    if (keyframe)
      GST_VIDEO_CODEC_FRAME_SET_SYNC_POINT (frame);

    return gst_video_encoder_finish_frame (GST_VIDEO_ENCODER (self), frame);
  }

  /* Empty available frame, something went wrong but we can just push this
   * buffer */
  GST_WARNING_OBJECT (self, "Failed to find corresponding frame");
  GST_BUFFER_PTS (buffer) = qsv_pts;
  if (GST_CLOCK_TIME_IS_VALID (qsv_dts))
    GST_BUFFER_DTS (buffer) = qsv_dts;
  else
    GST_BUFFER_DTS (buffer) = qsv_pts;

  if (!keyframe)
    GST_BUFFER_FLAG_SET (buffer, GST_BUFFER_FLAG_DELTA_UNIT);

  return gst_pad_push (GST_VIDEO_ENCODER_SRC_PAD (self), buffer);
}

static GstFlowReturn
gst_qsv_encoder_drain (GstQsvEncoder * self, gboolean discard)
{
  GstQsvEncoderPrivate *priv = self->priv;
  mfxStatus status = MFX_ERR_NONE;
  GstFlowReturn ret = GST_FLOW_OK;
  GstQsvEncoderTask *task;

  if (!priv->session || !priv->encoder)
    return GST_FLOW_OK;

  GST_DEBUG_OBJECT (self, "Drain");

  /* Drain pending tasks first if any */
  while (g_queue_get_length (&priv->pending_tasks) > 0) {
    task = (GstQsvEncoderTask *) g_queue_pop_tail (&priv->pending_tasks);
    ret = gst_qsv_encoder_finish_frame (self, task, discard);
  }

  while (status == MFX_ERR_NONE) {
    task = (GstQsvEncoderTask *) g_queue_pop_tail (&priv->free_tasks);
    status = gst_qsv_encoder_encode_frame (self,
        nullptr, task, MFX_TIMESTAMP_UNKNOWN);

    /* once it's fully drained, then driver will return more data */
    if (status == MFX_ERR_NONE && task->sync_point) {
      ret = gst_qsv_encoder_finish_frame (self, task, discard);
      continue;
    }

    if (status != MFX_ERR_MORE_DATA)
      GST_WARNING_OBJECT (self, "Unexpected status return %d (%s)",
          QSV_STATUS_ARGS (status));

    g_queue_push_head (&priv->free_tasks, task);
  }

  /* Release GstQsvFrame objects */
  for (guint i = 0; i < priv->surface_pool->len; i++) {
    GstQsvEncoderSurface *iter =
        &g_array_index (priv->surface_pool, GstQsvEncoderSurface, i);

    if (iter->surface.Data.Locked > 0) {
      GST_WARNING_OBJECT (self,
          "Encoder was drained but QSV is holding surface %d", i);
      continue;
    }

    gst_qsv_encoder_surface_reset (iter);
  }

  return ret;
}

#ifdef G_OS_WIN32
static gboolean
gst_qsv_encoder_prepare_d3d11_pool (GstQsvEncoder * self,
    GstCaps * caps, GstVideoInfo * aligned_info)
{
  GstQsvEncoderPrivate *priv = self->priv;
  GstStructure *config;
  GstD3D11AllocationParams *params;
  GstD3D11Device *device = GST_D3D11_DEVICE_CAST (priv->device);
  guint bind_flags = 0;
  GstD3D11Format device_format;

  gst_d3d11_device_get_format (device, GST_VIDEO_INFO_FORMAT (aligned_info),
      &device_format);
  if ((device_format.format_support[0] & D3D11_FORMAT_SUPPORT_RENDER_TARGET) ==
      D3D11_FORMAT_SUPPORT_RENDER_TARGET) {
    /* XXX: workaround for greenish artifacts
     * https://gitlab.freedesktop.org/gstreamer/gstreamer/-/issues/1238
     * bind to render target so that d3d11 memory allocator can clear texture
     * with black color */
    bind_flags = D3D11_BIND_RENDER_TARGET;
  }

  priv->internal_pool = gst_d3d11_buffer_pool_new (device);
  config = gst_buffer_pool_get_config (priv->internal_pool);
  params = gst_d3d11_allocation_params_new (device, aligned_info,
      GST_D3D11_ALLOCATION_FLAG_DEFAULT, bind_flags,
      D3D11_RESOURCE_MISC_SHARED);

  gst_buffer_pool_config_set_d3d11_allocation_params (config, params);
  gst_d3d11_allocation_params_free (params);
  gst_buffer_pool_config_set_params (config, caps,
      GST_VIDEO_INFO_SIZE (aligned_info), 0, 0);
  gst_buffer_pool_set_config (priv->internal_pool, config);
  gst_buffer_pool_set_active (priv->internal_pool, TRUE);

  return TRUE;
}
#else
static gboolean
gst_qsv_encoder_prepare_va_pool (GstQsvEncoder * self,
    GstCaps * caps, GstVideoInfo * aligned_info)
{
  GstQsvEncoderPrivate *priv = self->priv;
  GstAllocator *allocator;
  GstStructure *config;
  GArray *formats;
  GstAllocationParams params;
  GstVaDisplay *display = GST_VA_DISPLAY (priv->device);

  formats = g_array_new (FALSE, FALSE, sizeof (GstVideoFormat));
  g_array_append_val (formats, GST_VIDEO_INFO_FORMAT (aligned_info));

  allocator = gst_va_allocator_new (display, formats);
  if (!allocator) {
    GST_ERROR_OBJECT (self, "Failed to create allocator");
    return FALSE;
  }

  gst_allocation_params_init (&params);

  priv->internal_pool = gst_va_pool_new_with_config (caps, 0, 0,
      VA_SURFACE_ATTRIB_USAGE_HINT_GENERIC, GST_VA_FEATURE_AUTO,
      allocator, &params);
  gst_object_unref (allocator);


  if (!priv->internal_pool) {
    GST_ERROR_OBJECT (self, "Failed to create va pool");
    return FALSE;
  }

  config = gst_buffer_pool_get_config (priv->internal_pool);
  gst_buffer_pool_config_add_option (config, GST_BUFFER_POOL_OPTION_VIDEO_META);
  gst_buffer_pool_config_set_params (config, caps,
      GST_VIDEO_INFO_SIZE (aligned_info), 0, 0);
  gst_buffer_pool_set_config (priv->internal_pool, config);
  gst_buffer_pool_set_active (priv->internal_pool, TRUE);

  return TRUE;
}
#endif

/* Prepare internal pool, which is used to allocate fallback buffer
 * when upstream buffer is not directly accessible by QSV */
static gboolean
gst_qsv_encoder_prepare_pool (GstQsvEncoder * self, GstCaps * caps,
    GstVideoInfo * aligned_info)
{
  GstQsvEncoderPrivate *priv = self->priv;
  gboolean ret = FALSE;
  GstCaps *aligned_caps;

  if (priv->internal_pool) {
    gst_buffer_pool_set_active (priv->internal_pool, FALSE);
    gst_clear_object (&priv->internal_pool);
  }

  aligned_caps = gst_video_info_to_caps (aligned_info);

#ifdef G_OS_WIN32
  ret = gst_qsv_encoder_prepare_d3d11_pool (self, aligned_caps, aligned_info);
#else
  ret = gst_qsv_encoder_prepare_va_pool (self, aligned_caps, aligned_info);
#endif

  gst_caps_unref (aligned_caps);

  return ret;
}

static gboolean
gst_qsv_encoder_init_encode_session (GstQsvEncoder * self)
{
  GstQsvEncoderPrivate *priv = self->priv;
  GstQsvEncoderClass *klass = GST_QSV_ENCODER_GET_CLASS (self);
  GstVideoInfo *info = &priv->input_state->info;
  GstCaps *caps = priv->input_state->caps;
  mfxVideoParam param;
  mfxFrameInfo *frame_info;
  mfxFrameAllocRequest alloc_request;
  mfxStatus status;
  MFXVideoENCODE *encoder_handle = nullptr;
  guint bitstream_size;
  gboolean ret;
  guint64 min_delay_frames, max_delay_frames;
  GstClockTime min_latency, max_latency;

  gst_qsv_encoder_drain (self, FALSE);
  gst_qsv_encoder_reset (self);

  encoder_handle = new MFXVideoENCODE (priv->session);

  memset (&param, 0, sizeof (mfxVideoParam));

  g_ptr_array_set_size (priv->extra_params, 0);
  g_assert (klass->set_format);
  if (!klass->set_format (self, priv->input_state, &param, priv->extra_params)) {
    GST_ERROR_OBJECT (self, "Subclass failed to set format");
    goto error;
  }

  /* LowPower mode supports smaller set of features, don't enable it for now */
  param.mfx.LowPower = MFX_CODINGOPTION_OFF;
  if (priv->low_latency)
    param.AsyncDepth = 1;
  else
    param.AsyncDepth = 4;

  param.mfx.TargetUsage = priv->target_usage;

  frame_info = &param.mfx.FrameInfo;

  gst_video_info_set_interlaced_format (&priv->aligned_info,
      GST_VIDEO_INFO_FORMAT (info), GST_VIDEO_INFO_INTERLACE_MODE (info),
      frame_info->Width, frame_info->Height);

  /* Always video memory, even when upstream is non-hardware element */
  priv->mem_type = GST_QSV_VIDEO_MEMORY | GST_QSV_ENCODER_IN_MEMORY;
  param.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;
  if (!gst_qsv_encoder_prepare_pool (self, caps, &priv->aligned_info)) {
    GST_ERROR_OBJECT (self, "Failed to prepare pool");
    goto error;
  }

  status = encoder_handle->Query (&param, &param);
  /* If device is unhappy with LowPower = OFF, try again with unknown */
  if (status < MFX_ERR_NONE) {
    GST_INFO_OBJECT (self, "LowPower - OFF returned %d (%s)",
        QSV_STATUS_ARGS (status));
    param.mfx.LowPower = MFX_CODINGOPTION_UNKNOWN;
    status = encoder_handle->Query (&param, &param);
  }
  QSV_CHECK_STATUS (self, status, MFXVideoENCODE::Query);

  status = encoder_handle->QueryIOSurf (&param, &alloc_request);
  QSV_CHECK_STATUS (self, status, MFXVideoENCODE::QueryIOSurf);

  status = encoder_handle->Init (&param);
  QSV_CHECK_STATUS (self, status, MFXVideoENCODE::Init);

  status = encoder_handle->GetVideoParam (&param);
  QSV_CHECK_STATUS (self, status, MFXVideoENCODE::GetVideoParam);

  GST_DEBUG_OBJECT (self, "NumFrameSuggested: %d, AsyncDepth %d",
      alloc_request.NumFrameSuggested, param.AsyncDepth);

  g_assert (klass->set_output_state);
  ret = klass->set_output_state (self, priv->input_state, priv->session);
  if (!ret) {
    GST_ERROR_OBJECT (self, "Subclass failed to set output state");
    goto error;
  }

  /* Prepare surface pool with size NumFrameSuggested, then if it's not
   * sufficient while encoding, we can increse the pool size dynamically
   * if needed */
  g_array_set_size (priv->surface_pool, alloc_request.NumFrameSuggested);
  for (guint i = 0; i < priv->surface_pool->len; i++) {
    GstQsvEncoderSurface *surface = &g_array_index (priv->surface_pool,
        GstQsvEncoderSurface, i);

    surface->surface.Info = param.mfx.FrameInfo;
    surface->payload = g_ptr_array_new_with_free_func ((GDestroyNotify)
        gst_qsv_encoder_payload_clear);
  }
  priv->next_surface_index = 0;

  g_array_set_size (priv->task_pool, param.AsyncDepth);
  if (klass->codec_id == MFX_CODEC_JPEG) {
    gdouble factor = 4.0;

    /* jpeg zero returns buffer size */
    switch (GST_VIDEO_INFO_FORMAT (info)) {
      case GST_VIDEO_FORMAT_NV12:
        factor = 1.5;
        break;
      case GST_VIDEO_FORMAT_YUY2:
        factor = 2.0;
        break;
      default:
        break;
    }
    bitstream_size = (guint)
        (factor * GST_VIDEO_INFO_WIDTH (info) * GST_VIDEO_INFO_HEIGHT (info));
  } else {
    bitstream_size =
        (guint) param.mfx.BufferSizeInKB * param.mfx.BRCParamMultiplier * 1024;
  }

  for (guint i = 0; i < priv->task_pool->len; i++) {
    GstQsvEncoderTask *task = &g_array_index (priv->task_pool,
        GstQsvEncoderTask, i);

    task->bitstream.Data = (mfxU8 *) g_malloc (bitstream_size);
    task->bitstream.MaxLength = bitstream_size;

    g_queue_push_head (&priv->free_tasks, task);
  }

  min_delay_frames = priv->task_pool->len;
  /* takes the number of bframes into account */
  if (param.mfx.GopRefDist > 1)
    min_delay_frames += (param.mfx.GopRefDist - 1);
  max_delay_frames = priv->surface_pool->len + priv->task_pool->len;

  min_latency = gst_util_uint64_scale (min_delay_frames * GST_SECOND,
      param.mfx.FrameInfo.FrameRateExtD, param.mfx.FrameInfo.FrameRateExtN);
  max_latency = gst_util_uint64_scale (max_delay_frames * GST_SECOND,
      param.mfx.FrameInfo.FrameRateExtD, param.mfx.FrameInfo.FrameRateExtN);
  gst_video_encoder_set_latency (GST_VIDEO_ENCODER (self),
      min_latency, max_latency);

  priv->video_param = param;
  priv->encoder = encoder_handle;

  return TRUE;

error:
  if (encoder_handle)
    delete encoder_handle;

  gst_qsv_encoder_reset (self);

  return FALSE;
}

static gboolean
gst_qsv_encoder_reset_encode_session (GstQsvEncoder * self)
{
  GstQsvEncoderPrivate *priv = self->priv;
  GPtrArray *extra_params = priv->extra_params;
  mfxStatus status;
  mfxExtEncoderResetOption reset_opt;

  if (!priv->encoder) {
    GST_WARNING_OBJECT (self, "Encoder was not configured");
    return gst_qsv_encoder_init_encode_session (self);
  }

  reset_opt.Header.BufferId = MFX_EXTBUFF_ENCODER_RESET_OPTION;
  reset_opt.Header.BufferSz = sizeof (mfxExtEncoderResetOption);
  reset_opt.StartNewSequence = MFX_CODINGOPTION_OFF;

  gst_qsv_encoder_drain (self, FALSE);

  g_ptr_array_add (extra_params, &reset_opt);
  priv->video_param.ExtParam = (mfxExtBuffer **) extra_params->pdata;
  priv->video_param.NumExtParam = extra_params->len;

  status = priv->encoder->Reset (&priv->video_param);
  g_ptr_array_remove_index (extra_params, extra_params->len - 1);
  priv->video_param.NumExtParam = extra_params->len;

  if (status != MFX_ERR_NONE) {
    GST_WARNING_OBJECT (self, "MFXVideoENCODE_Reset returned %d (%s)",
        QSV_STATUS_ARGS (status));
    return gst_qsv_encoder_init_encode_session (self);
  }

  GST_DEBUG_OBJECT (self, "Encode session reset done");

  return TRUE;
}

static gboolean
gst_qsv_encoder_set_format (GstVideoEncoder * encoder,
    GstVideoCodecState * state)
{
  GstQsvEncoder *self = GST_QSV_ENCODER (encoder);
  GstQsvEncoderPrivate *priv = self->priv;

  g_clear_pointer (&priv->input_state, gst_video_codec_state_unref);
  priv->input_state = gst_video_codec_state_ref (state);

  return gst_qsv_encoder_init_encode_session (self);
}

static mfxU16
gst_qsv_encoder_get_pic_struct (GstQsvEncoder * self,
    GstVideoCodecFrame * frame)
{
  GstQsvEncoderClass *klass = GST_QSV_ENCODER_GET_CLASS (self);
  GstQsvEncoderPrivate *priv = self->priv;
  GstVideoInfo *info = &priv->input_state->info;

  if (klass->codec_id != MFX_CODEC_AVC)
    return MFX_PICSTRUCT_PROGRESSIVE;

  if (!GST_VIDEO_INFO_IS_INTERLACED (info))
    return MFX_PICSTRUCT_PROGRESSIVE;

  if (GST_VIDEO_INFO_INTERLACE_MODE (info) == GST_VIDEO_INTERLACE_MODE_MIXED) {
    if (!GST_BUFFER_FLAG_IS_SET (frame->input_buffer,
            GST_VIDEO_BUFFER_FLAG_INTERLACED)) {
      return MFX_PICSTRUCT_PROGRESSIVE;
    }

    if (GST_BUFFER_FLAG_IS_SET (frame->input_buffer, GST_VIDEO_BUFFER_FLAG_TFF))
      return MFX_PICSTRUCT_FIELD_TFF;

    return MFX_PICSTRUCT_FIELD_BFF;
  }

  switch (GST_VIDEO_INFO_FIELD_ORDER (info)) {
    case GST_VIDEO_FIELD_ORDER_TOP_FIELD_FIRST:
      return MFX_PICSTRUCT_FIELD_TFF;
      break;
    case GST_VIDEO_FIELD_ORDER_BOTTOM_FIELD_FIRST:
      return MFX_PICSTRUCT_FIELD_BFF;
      break;
    default:
      break;
  }

  if (GST_BUFFER_FLAG_IS_SET (frame->input_buffer, GST_VIDEO_BUFFER_FLAG_TFF))
    return MFX_PICSTRUCT_FIELD_TFF;

  return MFX_PICSTRUCT_FIELD_BFF;
}

static GstFlowReturn
gst_qsv_encoder_handle_frame (GstVideoEncoder * encoder,
    GstVideoCodecFrame * frame)
{
  GstQsvEncoder *self = GST_QSV_ENCODER (encoder);
  GstQsvEncoderPrivate *priv = self->priv;
  GstQsvEncoderClass *klass = GST_QSV_ENCODER_GET_CLASS (self);
  GstFlowReturn ret = GST_FLOW_ERROR;
  GstQsvEncoderSurface *surface;
  GstQsvEncoderTask *task;
  mfxU64 timestamp;
  mfxStatus status;

  if (klass->check_reconfigure && priv->encoder) {
    GstQsvEncoderReconfigure reconfigure;

    reconfigure = klass->check_reconfigure (self, priv->session,
        &priv->video_param, priv->extra_params);

    switch (reconfigure) {
      case GST_QSV_ENCODER_RECONFIGURE_BITRATE:
        if (!gst_qsv_encoder_reset_encode_session (self)) {
          GST_ERROR_OBJECT (self, "Failed to reset session");
          gst_video_encoder_finish_frame (encoder, frame);

          return GST_FLOW_ERROR;
        }
        break;
      case GST_QSV_ENCODER_RECONFIGURE_FULL:
        if (!gst_qsv_encoder_init_encode_session (self)) {
          GST_ERROR_OBJECT (self, "Failed to init session");
          gst_video_encoder_finish_frame (encoder, frame);

          return GST_FLOW_ERROR;
        }
        break;
      default:
        break;
    }
  }

  if (!priv->encoder) {
    GST_ERROR_OBJECT (self, "Encoder object was not configured");
    gst_video_encoder_finish_frame (encoder, frame);

    return GST_FLOW_NOT_NEGOTIATED;
  }

  surface = gst_qsv_encoder_get_next_surface (self);
  if (!surface) {
    GST_ERROR_OBJECT (self, "No available surface");
    goto out;
  }

  task = (GstQsvEncoderTask *) g_queue_pop_tail (&priv->free_tasks);
  g_assert (task);

  surface->qsv_frame =
      gst_qsv_allocator_acquire_frame (priv->allocator, priv->mem_type,
      &priv->input_state->info, gst_buffer_ref (frame->input_buffer),
      priv->internal_pool);

  if (!surface->qsv_frame) {
    GST_ERROR_OBJECT (self, "Failed to wrap buffer with qsv frame");
    gst_qsv_encoder_task_reset (self, task);
    goto out;
  }

  surface->surface.Info.PicStruct =
      gst_qsv_encoder_get_pic_struct (self, frame);

  if (GST_VIDEO_CODEC_FRAME_IS_FORCE_KEYFRAME (frame)) {
    surface->encode_control.FrameType =
        MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF;
  } else {
    surface->encode_control.FrameType = MFX_FRAMETYPE_UNKNOWN;
  }

  if (klass->attach_payload) {
    klass->attach_payload (self, frame, surface->payload);
    if (surface->payload->len > 0) {
      surface->encode_control.NumPayload = surface->payload->len;
      surface->encode_control.Payload = (mfxPayload **) surface->payload->pdata;
    }
  }

  timestamp = gst_qsv_timestamp_from_gst (frame->pts);
  status = gst_qsv_encoder_encode_frame (self, surface, task, timestamp);
  if (status != MFX_ERR_NONE && status != MFX_ERR_MORE_DATA) {
    GST_ERROR_OBJECT (self, "Failed to encode frame, ret %d (%s)",
        QSV_STATUS_ARGS (status));
    gst_qsv_encoder_task_reset (self, task);
    goto out;
  }

  if (status == MFX_ERR_NONE && task->sync_point) {
    g_queue_push_head (&priv->pending_tasks, task);
  } else {
    gst_qsv_encoder_task_reset (self, task);
  }

  ret = GST_FLOW_OK;
  /* Do not sync immediately, but record tasks which have output buffer here
   * to improve throughput.
   * In this way, hardware may be able to run encoding job from its background
   * threads (if any). We will do sync only when there's no more free task item
   */
  while (g_queue_get_length (&priv->pending_tasks) >= priv->task_pool->len) {
    GstQsvEncoderTask *task =
        (GstQsvEncoderTask *) g_queue_pop_tail (&priv->pending_tasks);
    ret = gst_qsv_encoder_finish_frame (self, task, FALSE);
  }

out:
  gst_video_codec_frame_unref (frame);

  return ret;
}

static GstFlowReturn
gst_qsv_encoder_finish (GstVideoEncoder * encoder)
{
  GstQsvEncoder *self = GST_QSV_ENCODER (encoder);

  return gst_qsv_encoder_drain (self, FALSE);
}

static gboolean
gst_qsv_encoder_flush (GstVideoEncoder * encoder)
{
  GstQsvEncoder *self = GST_QSV_ENCODER (encoder);

  gst_qsv_encoder_drain (self, TRUE);

  return TRUE;
}

static gboolean
gst_qsv_encoder_handle_context_query (GstQsvEncoder * self, GstQuery * query)
{
  GstQsvEncoderPrivate *priv = self->priv;

#ifdef G_OS_WIN32
  return gst_d3d11_handle_context_query (GST_ELEMENT (self), query,
      (GstD3D11Device *) priv->device);
#else
  return gst_va_handle_context_query (GST_ELEMENT (self), query,
      (GstVaDisplay *) priv->device);
#endif
}

static gboolean
gst_qsv_encoder_sink_query (GstVideoEncoder * encoder, GstQuery * query)
{
  GstQsvEncoder *self = GST_QSV_ENCODER (encoder);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CONTEXT:
      if (gst_qsv_encoder_handle_context_query (self, query))
        return TRUE;
      break;
    default:
      break;
  }

  return GST_VIDEO_ENCODER_CLASS (parent_class)->sink_query (encoder, query);
}

static gboolean
gst_qsv_encoder_src_query (GstVideoEncoder * encoder, GstQuery * query)
{
  GstQsvEncoder *self = GST_QSV_ENCODER (encoder);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CONTEXT:
      if (gst_qsv_encoder_handle_context_query (self, query))
        return TRUE;
      break;
    default:
      break;
  }

  return GST_VIDEO_ENCODER_CLASS (parent_class)->src_query (encoder, query);
}

#ifdef G_OS_WIN32
static gboolean
gst_qsv_encoder_propose_allocation (GstVideoEncoder * encoder, GstQuery * query)
{
  GstQsvEncoder *self = GST_QSV_ENCODER (encoder);
  GstQsvEncoderPrivate *priv = self->priv;
  GstD3D11Device *device = GST_D3D11_DEVICE (priv->device);
  GstVideoInfo info;
  GstBufferPool *pool;
  GstCaps *caps;
  guint size;
  GstStructure *config;
  GstCapsFeatures *features;
  gboolean is_d3d11 = FALSE;

  gst_query_parse_allocation (query, &caps, nullptr);
  if (!caps) {
    GST_WARNING_OBJECT (self, "null caps in query");
    return FALSE;
  }

  if (!gst_video_info_from_caps (&info, caps)) {
    GST_WARNING_OBJECT (self, "Failed to convert caps into info");
    return FALSE;
  }

  features = gst_caps_get_features (caps, 0);
  if (features && gst_caps_features_contains (features,
          GST_CAPS_FEATURE_MEMORY_D3D11_MEMORY)) {
    GST_DEBUG_OBJECT (self, "upstream support d3d11 memory");
    pool = gst_d3d11_buffer_pool_new (device);
    is_d3d11 = TRUE;
  } else {
    pool = gst_video_buffer_pool_new ();
  }

  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_add_option (config, GST_BUFFER_POOL_OPTION_VIDEO_META);

  if (is_d3d11) {
    GstD3D11AllocationParams *d3d11_params;
    GstVideoAlignment align;

    /* d3d11 buffer pool doesn't support generic video alignment
     * because memory layout of CPU accessible staging texture is uncontrollable.
     * Do D3D11 specific handling */
    gst_video_alignment_reset (&align);

    align.padding_right = GST_VIDEO_INFO_WIDTH (&priv->aligned_info) -
        GST_VIDEO_INFO_WIDTH (&info);
    align.padding_bottom = GST_VIDEO_INFO_HEIGHT (&priv->aligned_info) -
        GST_VIDEO_INFO_HEIGHT (&info);

    d3d11_params = gst_d3d11_allocation_params_new (device, &info,
        GST_D3D11_ALLOCATION_FLAG_DEFAULT, 0, 0);

    gst_d3d11_allocation_params_alignment (d3d11_params, &align);
    gst_buffer_pool_config_set_d3d11_allocation_params (config, d3d11_params);
    gst_d3d11_allocation_params_free (d3d11_params);
  } else {
    gst_buffer_pool_config_add_option (config,
        GST_BUFFER_POOL_OPTION_VIDEO_ALIGNMENT);
  }

  size = GST_VIDEO_INFO_SIZE (&info);
  gst_buffer_pool_config_set_params (config,
      caps, size, priv->surface_pool->len, 0);

  if (!gst_buffer_pool_set_config (pool, config)) {
    GST_WARNING_OBJECT (self, "Failed to set pool config");
    gst_object_unref (pool);
    return FALSE;
  }

  /* d3d11 buffer pool will update actual CPU accessible buffer size based on
   * allocated staging texture per gst_buffer_pool_set_config() call,
   * need query again to get the size */
  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_get_params (config, nullptr, &size, nullptr, nullptr);
  gst_structure_free (config);

  gst_query_add_allocation_pool (query, pool, size, priv->surface_pool->len, 0);
  gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, nullptr);
  gst_object_unref (pool);

  return TRUE;
}
#else
static gboolean
gst_qsv_encoder_propose_allocation (GstVideoEncoder * encoder, GstQuery * query)
{
  GstQsvEncoder *self = GST_QSV_ENCODER (encoder);
  GstQsvEncoderPrivate *priv = self->priv;
  GstVideoInfo info;
  GstAllocator *allocator = nullptr;
  GstBufferPool *pool;
  GstCaps *caps;
  guint size;
  GstStructure *config;
  GstVideoAlignment align;
  GstAllocationParams params;
  GArray *formats;

  gst_query_parse_allocation (query, &caps, nullptr);
  if (!caps) {
    GST_WARNING_OBJECT (self, "null caps in query");
    return FALSE;
  }

  if (!gst_video_info_from_caps (&info, caps)) {
    GST_WARNING_OBJECT (self, "Failed to convert caps into info");
    return FALSE;
  }

  gst_allocation_params_init (&params);

  formats = g_array_new (FALSE, FALSE, sizeof (GstVideoFormat));
  g_array_append_val (formats, GST_VIDEO_INFO_FORMAT (&info));

  allocator = gst_va_allocator_new (GST_VA_DISPLAY (priv->device), formats);
  if (!allocator) {
    GST_ERROR_OBJECT (self, "Failed to create allocator");
    return FALSE;
  }

  pool = gst_va_pool_new_with_config (caps, priv->surface_pool->len, 0,
      VA_SURFACE_ATTRIB_USAGE_HINT_GENERIC, GST_VA_FEATURE_AUTO,
      allocator, &params);

  if (!pool) {
    GST_ERROR_OBJECT (self, "Failed to create va pool");
    gst_object_unref (allocator);

    return FALSE;
  }

  gst_video_alignment_reset (&align);
  align.padding_right = GST_VIDEO_INFO_WIDTH (&priv->aligned_info) -
      GST_VIDEO_INFO_WIDTH (&info);
  align.padding_bottom = GST_VIDEO_INFO_HEIGHT (&priv->aligned_info) -
      GST_VIDEO_INFO_HEIGHT (&info);

  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_add_option (config, GST_BUFFER_POOL_OPTION_VIDEO_META);
  gst_buffer_pool_config_add_option (config,
      GST_BUFFER_POOL_OPTION_VIDEO_ALIGNMENT);
  gst_buffer_pool_config_set_video_alignment (config, &align);

  gst_buffer_pool_config_set_params (config,
      caps, GST_VIDEO_INFO_SIZE (&info), priv->surface_pool->len, 0);

  if (!gst_buffer_pool_set_config (pool, config)) {
    GST_ERROR_OBJECT (self, "Failed to set pool config");
    gst_clear_object (&allocator);
    gst_object_unref (pool);
    return FALSE;
  }

  if (allocator)
    gst_query_add_allocation_param (query, allocator, &params);

  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_get_params (config, nullptr, &size, nullptr, nullptr);
  gst_structure_free (config);

  gst_query_add_allocation_pool (query, pool, size, priv->surface_pool->len, 0);
  gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, nullptr);

  gst_clear_object (&allocator);
  gst_object_unref (pool);

  return TRUE;
}
#endif
