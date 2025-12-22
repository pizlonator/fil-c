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

#include <components/Component.h>
#include <core/Factory.h>
#include "gstamfencoder.h"

#include <gst/d3d11/gstd3d11.h>
#include <wrl.h>
#include <string.h>
#include <mmsystem.h>
#include <queue>
#include <algorithm>

/* *INDENT-OFF* */
using namespace Microsoft::WRL;
using namespace amf;
/* *INDENT-ON* */

/**
 * GstAmfEncPAActivityType:
 *
 * Determines whether activity analysis is performed on the Luma component
 * only (Y) or on both Luma and Chroma (YUV).
 *
 * Since: 1.24
 */
GType
gst_amf_enc_pa_activity_get_type (void)
{
  static GType pa_activity_type = 0;
  static const GEnumValue activity_types[] = {
    /**
     * GstAmfEncPAActivityType::y:
     *
     * Activity analysis is performed on the Luma component only (Y)
     */
    {AMF_PA_ACTIVITY_Y, "Luma (Y) component only",
        "y"},

    /**
     * GstAmfEncPAActivityType::yuv:
     *
     * Activity analysis is performed on both Luma and Chroma components (YUV)
     */
    {AMF_PA_ACTIVITY_YUV, "Luma and Chroma components (YUV)", "yuv"},
    {0, nullptr, nullptr}
  };

  if (g_once_init_enter (&pa_activity_type)) {
    GType type =
        g_enum_register_static ("GstAmfEncPAActivityType", activity_types);
    g_once_init_leave (&pa_activity_type, type);
  }

  return pa_activity_type;
}

/**
 * GstAmfEncPASceneChangeDetectionSensitivity:
 *
 * Sensitivity of scene change detection. The higher the sensitivity, the more
 * restrictive it is to detect a scene change. This parameter takes effect
 * only when AMF_PA_LOOKAHEAH_BUFFER_DEPTH is set to 0.
 *
 * Since: 1.24
 */
GType
gst_amf_enc_pa_scene_change_detection_sensitivity_get_type (void)
{
  static GType pa_scene_change_detection_sensitivity = 0;
  static const GEnumValue sensitivity_types[] = {
    /**
     * GstAmfEncPASceneChangeDetectionSensitivity::low:
     *
     * Low sensitivity
     */
    {AMF_PA_SCENE_CHANGE_DETECTION_SENSITIVITY_LOW, "Low", "low"},

    /**
     * GstAmfEncPASceneChangeDetectionSensitivity::medium:
     *
     * Medium sensitivity
     */
    {AMF_PA_SCENE_CHANGE_DETECTION_SENSITIVITY_MEDIUM, "Medium", "medium"},

    /**
     * GstAmfEncPASceneChangeDetectionSensitivity::high:
     *
     * High sensitivity
     */
    {AMF_PA_SCENE_CHANGE_DETECTION_SENSITIVITY_HIGH, "High", "high"},
    {0, nullptr, nullptr}
  };

  if (g_once_init_enter (&pa_scene_change_detection_sensitivity)) {
    GType type =
        g_enum_register_static
        ("GstAmfEncPASceneChangeDetectionSensitivity", sensitivity_types);
    g_once_init_leave (&pa_scene_change_detection_sensitivity, type);
  }

  return pa_scene_change_detection_sensitivity;
}

/**
 * GstAmfEncPAStaticSceneDetectionSensitivity:
 *
 * Sensitivity of static scene detection. The higher the sensitivity, the more
 * restrictive it is to detect a static scene.
 *
 * Since: 1.24
 */
GType
gst_amf_enc_pa_static_scene_detection_sensitivity_get_type (void)
{
  static GType pa_static_scene_detection_sensitivity = 0;
  static const GEnumValue sensitivity_types[] = {
    /**
     * GstAmfEncPAStaticSceneDetectionSensitivity::low:
     *
     * Low sensitivity
     */
    {AMF_PA_STATIC_SCENE_DETECTION_SENSITIVITY_LOW, "Low", "low"},

    /**
     * GstAmfEncPAStaticSceneDetectionSensitivity::medium:
     *
     * Medium sensitivity
     */
    {AMF_PA_STATIC_SCENE_DETECTION_SENSITIVITY_MEDIUM, "Medium", "medium"},

    /**
     * GstAmfEncPAStaticSceneDetectionSensitivity::high:
     *
     * High sensitivity
     */
    {AMF_PA_STATIC_SCENE_DETECTION_SENSITIVITY_HIGH, "High", "high"},
    {0, nullptr, nullptr}
  };

  if (g_once_init_enter (&pa_static_scene_detection_sensitivity)) {
    GType type =
        g_enum_register_static
        ("GstAmfEncPAStaticSceneDetectionSensitivity", sensitivity_types);
    g_once_init_leave (&pa_static_scene_detection_sensitivity, type);
  }

  return pa_static_scene_detection_sensitivity;
}

/**
 * GstAmfEncPACAQStrength:
 *
 * Content Adaptive Quantization strength. Stronger CAQ strength means larger
 * variation in block level QP assignment.
 *
 * Since: 1.24
 */
GType
gst_amf_enc_pa_caq_strength_get_type (void)
{
  static GType pa_caq_strength = 0;
  static const GEnumValue strength_types[] = {
    /**
     * GstAmfEncPACAQStrength::low:
     *
     * Low strength
     */
    {AMF_PA_CAQ_STRENGTH_LOW, "Low", "low"},

    /**
     * GstAmfEncPACAQStrength::medium:
     *
     * Medium strength
     */
    {AMF_PA_CAQ_STRENGTH_MEDIUM, "Medium", "medium"},

    /**
     * GstAmfEncPACAQStrength::high:
     *
     * High strength
     */
    {AMF_PA_CAQ_STRENGTH_HIGH, "High", "high"},
    {0, nullptr, nullptr}
  };

  if (g_once_init_enter (&pa_caq_strength)) {
    GType type =
        g_enum_register_static ("GstAmfEncPACAQStrength", strength_types);
    g_once_init_leave (&pa_caq_strength, type);
  }

  return pa_caq_strength;
}

/**
 * GstAmfEncPAPAQMode:
 *
 * Sets the perceptual adaptive quantization mode.
 *
 * Since: 1.24
 */
GType
gst_amf_enc_pa_paq_mode_get_type (void)
{
  static GType pa_paq_mode = 0;
  static const GEnumValue paq_modes[] = {
    /**
     * GstAmfEncPAPAQMode::none:
     *
     * No perceptual adaptive quantization
     */
    {AMF_PA_PAQ_MODE_NONE, "None", "none"},

    /**
     * GstAmfEncPAPAQMode::caq:
     *
     * Content Adaptive Quantization (CAQ) mode
     */
    {AMF_PA_PAQ_MODE_CAQ, "Content Adaptive Quantization (CAQ) mode", "caq"},
    {0, nullptr, nullptr}
  };

  if (g_once_init_enter (&pa_paq_mode)) {
    GType type = g_enum_register_static ("GstAmfEncPAPAQMode", paq_modes);
    g_once_init_leave (&pa_paq_mode, type);
  }

  return pa_paq_mode;
}

/**
 * GstAmfEncPATAQMode:
 *
 * Sets the temporal adaptive quantization mode. MODE_1 is suitable for non-gaming
 * applications whereas MODE_2 is suitable for gaming applications.
 *
 * Since: 1.24
 */
GType
gst_amf_enc_pa_taq_mode_get_type (void)
{
  static GType pa_taq_mode = 0;
  static const GEnumValue taq_modes[] = {
    /**
     * GstAmfEncPATAQMode::none:
     *
     * No temporal adaptive quantization
     */
    {AMF_PA_TAQ_MODE_NONE, "None", "none"},

    /**
     * GstAmfEncPATAQMode::mode1:
     *
     * MODE_1 is suitable for non-gaming applications
     */
    {AMF_PA_TAQ_MODE_1, "Mode_1 is suitable for non-gaming applications",
        "mode1"},

    /**
     * GstAmfEncPATAQMode::mode2:
     *
     * MODE_2 is suitable for for gaming applications
     */
    {AMF_PA_TAQ_MODE_2, "Mode_2 is suitable for gaming applications", "mode2"},
    {0, nullptr, nullptr}
  };

  if (g_once_init_enter (&pa_taq_mode)) {
    GType type = g_enum_register_static ("GstAmfEncPATAQMode", taq_modes);
    g_once_init_leave (&pa_taq_mode, type);
  }

  return pa_taq_mode;
}

/**
 * GstAmfEncPAHQMBMode:
 *
 * Sets the PA high motion quality boost (HQMB) mode to help the encoder in motion search.
 *
 * Since: 1.24
 */
GType
gst_amf_enc_pa_hmbq_mode_get_type (void)
{
  static GType pa_hmbq_mode = 0;
  static const GEnumValue hmbq_modes[] = {
    /**
     * GstAmfEncPAHQMBMode::none:
     *
     * No high motion quality boost
     */
    {AMF_PA_HIGH_MOTION_QUALITY_BOOST_MODE_NONE, "None", "none"},

    /**
     * GstAmfEncPAHQMBMode::auto:
     *
     * Automatic mode
     */
    {AMF_PA_HIGH_MOTION_QUALITY_BOOST_MODE_AUTO, "Auto", "auto"},
    {0, nullptr, nullptr}
  };

  if (g_once_init_enter (&pa_hmbq_mode)) {
    GType type = g_enum_register_static ("GstAmfEncPAHQMBMode", hmbq_modes);
    g_once_init_leave (&pa_hmbq_mode, type);
  }

  return pa_hmbq_mode;
}

GST_DEBUG_CATEGORY_STATIC (gst_amf_encoder_debug);
#define GST_CAT_DEFAULT gst_amf_encoder_debug

static GUID AMFTextureArrayIndexGUID = { 0x28115527, 0xe7c3, 0x4b66, 0x99, 0xd3,
  0x4f, 0x2a, 0xe6, 0xb4, 0x7f, 0xaf
};

#define GST_AMF_BUFFER_PROP L"GstAmfFrameData"

#define GST_AMF_ENCODER_FLOW_TRY_AGAIN GST_FLOW_CUSTOM_SUCCESS_1

typedef struct
{
  GstBuffer *buffer;
  GstMapInfo info;
} GstAmfEncoderFrameData;


/* *INDENT-OFF* */
struct _GstAmfEncoderPrivate
{
  gint64 adapter_luid = 0;
  const wchar_t *codec_id = nullptr;

  GstD3D11Device *device = nullptr;
  GstD3D11Fence *fence = nullptr;
  AMFContext *context = nullptr;
  AMFComponent *comp = nullptr;
  GstBufferPool *internal_pool = nullptr;

  GstVideoCodecState *input_state = nullptr;

  /* High precision clock */
  guint timer_resolution = 0;

  std::queue <GstClockTime> timestamp_queue;
  GstClockTime dts_offset = 0;
  GstClockTime last_dts = GST_CLOCK_TIME_NONE;
};
/* *INDENT-ON* */

#define gst_amf_encoder_parent_class parent_class
G_DEFINE_ABSTRACT_TYPE (GstAmfEncoder, gst_amf_encoder, GST_TYPE_VIDEO_ENCODER);

static void gst_amf_encoder_dispose (GObject * object);
static void gst_amf_encoder_finalize (GObject * object);
static void gst_amf_encoder_set_context (GstElement * element,
    GstContext * context);
static gboolean gst_amf_encoder_open (GstVideoEncoder * encoder);
static gboolean gst_amf_encoder_stop (GstVideoEncoder * encoder);
static gboolean gst_amf_encoder_close (GstVideoEncoder * encoder);
static gboolean gst_amf_encoder_set_format (GstVideoEncoder * encoder,
    GstVideoCodecState * state);
static GstFlowReturn gst_amf_encoder_handle_frame (GstVideoEncoder * encoder,
    GstVideoCodecFrame * frame);
static GstFlowReturn gst_amf_encoder_finish (GstVideoEncoder * encoder);
static gboolean gst_amf_encoder_flush (GstVideoEncoder * encoder);
static gboolean gst_amf_encoder_sink_query (GstVideoEncoder * encoder,
    GstQuery * query);
static gboolean gst_amf_encoder_src_query (GstVideoEncoder * encoder,
    GstQuery * query);
static gboolean gst_amf_encoder_propose_allocation (GstVideoEncoder * encoder,
    GstQuery * query);

static void
gst_amf_encoder_class_init (GstAmfEncoderClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstVideoEncoderClass *videoenc_class = GST_VIDEO_ENCODER_CLASS (klass);

  object_class->dispose = gst_amf_encoder_dispose;
  object_class->finalize = gst_amf_encoder_finalize;

  element_class->set_context = GST_DEBUG_FUNCPTR (gst_amf_encoder_set_context);

  videoenc_class->open = GST_DEBUG_FUNCPTR (gst_amf_encoder_open);
  videoenc_class->stop = GST_DEBUG_FUNCPTR (gst_amf_encoder_stop);
  videoenc_class->close = GST_DEBUG_FUNCPTR (gst_amf_encoder_close);
  videoenc_class->set_format = GST_DEBUG_FUNCPTR (gst_amf_encoder_set_format);
  videoenc_class->handle_frame =
      GST_DEBUG_FUNCPTR (gst_amf_encoder_handle_frame);
  videoenc_class->finish = GST_DEBUG_FUNCPTR (gst_amf_encoder_finish);
  videoenc_class->flush = GST_DEBUG_FUNCPTR (gst_amf_encoder_flush);
  videoenc_class->sink_query = GST_DEBUG_FUNCPTR (gst_amf_encoder_sink_query);
  videoenc_class->src_query = GST_DEBUG_FUNCPTR (gst_amf_encoder_src_query);
  videoenc_class->propose_allocation =
      GST_DEBUG_FUNCPTR (gst_amf_encoder_propose_allocation);

  GST_DEBUG_CATEGORY_INIT (gst_amf_encoder_debug,
      "amfencoder", 0, "amfencoder");

  gst_type_mark_as_plugin_api (GST_TYPE_AMF_ENCODER, (GstPluginAPIFlags) 0);
  gst_type_mark_as_plugin_api (GST_TYPE_AMF_ENC_PA_ACTIVITY_TYPE,
      (GstPluginAPIFlags) 0);
  gst_type_mark_as_plugin_api
      (GST_TYPE_AMF_ENC_PA_SCENE_CHANGE_DETECTION_SENSITIVITY,
      (GstPluginAPIFlags) 0);
  gst_type_mark_as_plugin_api
      (GST_TYPE_AMF_ENC_PA_STATIC_SCENE_DETECTION_SENSITIVITY,
      (GstPluginAPIFlags) 0);
  gst_type_mark_as_plugin_api (GST_TYPE_AMF_ENC_PA_CAQ_STRENGTH,
      (GstPluginAPIFlags) 0);
  gst_type_mark_as_plugin_api (GST_TYPE_AMF_ENC_PA_PAQ_MODE,
      (GstPluginAPIFlags) 0);
  gst_type_mark_as_plugin_api (GST_TYPE_AMF_ENC_PA_TAQ_MODE,
      (GstPluginAPIFlags) 0);
  gst_type_mark_as_plugin_api (GST_TYPE_AMF_ENC_PA_HQMB_MODE,
      (GstPluginAPIFlags) 0);
}

static void
gst_amf_encoder_init (GstAmfEncoder * self)
{
  GstAmfEncoderPrivate *priv;
  TIMECAPS time_caps;

  priv = self->priv = new GstAmfEncoderPrivate ();

  gst_video_encoder_set_min_pts (GST_VIDEO_ENCODER (self),
      GST_SECOND * 60 * 60 * 1000);

  if (timeGetDevCaps (&time_caps, sizeof (TIMECAPS)) == TIMERR_NOERROR) {
    guint resolution;
    MMRESULT ret;

    resolution = MIN (MAX (time_caps.wPeriodMin, 1), time_caps.wPeriodMax);
    ret = timeBeginPeriod (resolution);
    if (ret == TIMERR_NOERROR)
      priv->timer_resolution = resolution;
  }
}

static void
gst_amf_encoder_dispose (GObject * object)
{
  GstAmfEncoder *self = GST_AMF_ENCODER (object);
  GstAmfEncoderPrivate *priv = self->priv;

  gst_clear_object (&priv->device);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_amf_encoder_finalize (GObject * object)
{
  GstAmfEncoder *self = GST_AMF_ENCODER (object);
  GstAmfEncoderPrivate *priv = self->priv;

  if (priv->timer_resolution)
    timeEndPeriod (priv->timer_resolution);

  delete priv;

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_amf_encoder_set_context (GstElement * element, GstContext * context)
{
  GstAmfEncoder *self = GST_AMF_ENCODER (element);
  GstAmfEncoderPrivate *priv = self->priv;

  gst_d3d11_handle_set_context_for_adapter_luid (element,
      context, priv->adapter_luid, &priv->device);

  GST_ELEMENT_CLASS (parent_class)->set_context (element, context);
}

static gboolean
gst_amf_encoder_open (GstVideoEncoder * encoder)
{
  GstAmfEncoder *self = GST_AMF_ENCODER (encoder);
  GstAmfEncoderPrivate *priv = self->priv;
  ComPtr < ID3D10Multithread > multi_thread;
  ID3D11Device *device_handle;
  AMFFactory *factory = (AMFFactory *) gst_amf_get_factory ();
  AMF_RESULT result;
  HRESULT hr;
  D3D_FEATURE_LEVEL feature_level;
  AMF_DX_VERSION dx_ver = AMF_DX11_1;

  if (!gst_d3d11_ensure_element_data_for_adapter_luid (GST_ELEMENT (self),
          priv->adapter_luid, &priv->device)) {
    GST_ERROR_OBJECT (self, "d3d11 device is unavailable");
    return FALSE;
  }

  device_handle = gst_d3d11_device_get_device_handle (priv->device);
  feature_level = device_handle->GetFeatureLevel ();
  if (feature_level >= D3D_FEATURE_LEVEL_11_1)
    dx_ver = AMF_DX11_1;
  else
    dx_ver = AMF_DX11_0;

  hr = device_handle->QueryInterface (IID_PPV_ARGS (&multi_thread));
  if (!gst_d3d11_result (hr, priv->device)) {
    GST_ERROR_OBJECT (self, "ID3D10Multithread interface is unavailable");
    goto error;
  }

  multi_thread->SetMultithreadProtected (TRUE);

  result = factory->CreateContext (&priv->context);
  if (result != AMF_OK) {
    GST_ERROR_OBJECT (self, "Failed to create context");
    goto error;
  }

  result = priv->context->InitDX11 (device_handle, dx_ver);
  if (result != AMF_OK) {
    GST_ERROR_OBJECT (self, "Failed to init context");
    goto error;
  }

  return TRUE;

error:
  gst_clear_object (&priv->device);
  if (priv->context)
    priv->context->Release ();
  priv->context = nullptr;

  return FALSE;
}

static gboolean
gst_amf_encoder_reset (GstAmfEncoder * self)
{
  GstAmfEncoderPrivate *priv = self->priv;

  GST_LOG_OBJECT (self, "Reset");

  if (priv->internal_pool) {
    gst_buffer_pool_set_active (priv->internal_pool, FALSE);
    gst_clear_object (&priv->internal_pool);
  }

  if (priv->comp) {
    priv->comp->Terminate ();
    priv->comp->Release ();
    priv->comp = nullptr;
  }

  std::queue < GstClockTime > empty_queue;
  std::swap (priv->timestamp_queue, empty_queue);

  priv->dts_offset = 0;
  priv->last_dts = GST_CLOCK_TIME_NONE;

  return TRUE;
}

static GstFlowReturn
gst_amf_encoder_process_output (GstAmfEncoder * self, AMFBuffer * buffer)
{
  GstAmfEncoderPrivate *priv = self->priv;
  GstAmfEncoderClass *klass = GST_AMF_ENCODER_GET_CLASS (self);
  GstVideoEncoder *venc = GST_VIDEO_ENCODER_CAST (self);
  AMF_RESULT result;
  GstVideoCodecFrame *frame = nullptr;
  GstBuffer *output_buffer;
  gboolean sync_point = FALSE;

  GST_TRACE_OBJECT (self, "Process output");

  if (buffer->HasProperty (GST_AMF_BUFFER_PROP)) {
    AMFInterfacePtr iface;
    result = buffer->GetProperty (GST_AMF_BUFFER_PROP, &iface);
    if (result != AMF_OK) {
      GST_ERROR_OBJECT (self, "Failed to get prop buffer, result %"
          GST_AMF_RESULT_FORMAT, GST_AMF_RESULT_ARGS (result));
    } else {
      AMFBufferPtr prop_buffer = AMFBufferPtr (iface);
      if (prop_buffer) {
        guint32 system_frame_number = *((guint32 *) prop_buffer->GetNative ());
        frame = gst_video_encoder_get_frame (venc, system_frame_number);
      }
    }
  } else {
    GST_WARNING_OBJECT (self, "AMFData does not hold user data");
  }

  if (!frame) {
    GST_WARNING_OBJECT (self, "Failed to get find associated codec frame");
    frame = gst_video_encoder_get_oldest_frame (venc);
  }

  output_buffer = klass->create_output_buffer (self, buffer, &sync_point);

  if (!output_buffer) {
    GST_WARNING_OBJECT (self, "Empty output buffer");
    return GST_FLOW_OK;
  }

  GST_BUFFER_FLAG_SET (output_buffer, GST_BUFFER_FLAG_MARKER);

  if (frame) {
    GstClockTime dts = GST_CLOCK_TIME_NONE;

    if (GST_CLOCK_TIME_IS_VALID (frame->pts) && !priv->timestamp_queue.empty ()) {
      dts = priv->timestamp_queue.front ();
      priv->timestamp_queue.pop ();

      if (priv->dts_offset > 0)
        dts -= priv->dts_offset;

      if (!GST_CLOCK_TIME_IS_VALID (priv->last_dts)) {
        dts = MIN (dts, frame->pts);
        priv->last_dts = dts;
      } else {
        dts = MAX (priv->last_dts, dts);
        dts = MIN (dts, frame->pts);
        priv->last_dts = dts;
      }
    }

    frame->dts = dts;
    frame->output_buffer = output_buffer;

    if (sync_point)
      GST_VIDEO_CODEC_FRAME_SET_SYNC_POINT (frame);
  } else {
    GstClockTime pts = buffer->GetPts () * 100;

    GST_BUFFER_PTS (output_buffer) = pts;

    if (!sync_point)
      GST_BUFFER_FLAG_SET (output_buffer, GST_BUFFER_FLAG_DELTA_UNIT);

    return gst_pad_push (GST_VIDEO_ENCODER_SRC_PAD (self), output_buffer);
  }

  gst_video_codec_frame_set_user_data (frame, nullptr, nullptr);

  return gst_video_encoder_finish_frame (venc, frame);
}

static AMF_RESULT
gst_amf_encoder_query_output (GstAmfEncoder * self, AMFBuffer ** buffer)
{
  GstAmfEncoderPrivate *priv = self->priv;
  AMF_RESULT result;
  AMFDataPtr data;
  AMFBufferPtr buf;

  result = priv->comp->QueryOutput (&data);
  if (result != AMF_OK)
    return result;

  if (!data) {
    GST_LOG_OBJECT (self, "Empty data");
    return AMF_REPEAT;
  }

  buf = AMFBufferPtr (data);
  if (!buf) {
    GST_ERROR_OBJECT (self, "Failed to convert data to buffer");
    return AMF_NO_INTERFACE;
  }

  *buffer = buf.Detach ();

  return AMF_OK;
}

static GstFlowReturn
gst_amf_encoder_try_output (GstAmfEncoder * self, gboolean do_wait)
{
  GstFlowReturn ret = GST_FLOW_OK;

  do {
    AMFBufferPtr buffer;
    AMF_RESULT result = gst_amf_encoder_query_output (self, &buffer);

    if (buffer) {
      ret = gst_amf_encoder_process_output (self, buffer.GetPtr ());
      if (ret != GST_FLOW_OK) {
        GST_INFO_OBJECT (self, "Process output returned %s",
            gst_flow_get_name (ret));
      }
    } else if (result == AMF_REPEAT || result == AMF_OK) {
      GST_TRACE_OBJECT (self, "Output is not ready, do_wait %d", do_wait);
      if (do_wait) {
        g_usleep (1000);
      } else {
        ret = GST_AMF_ENCODER_FLOW_TRY_AGAIN;
      }
    } else if (result == AMF_EOF) {
      GST_DEBUG_OBJECT (self, "Output queue is drained");
      ret = GST_VIDEO_ENCODER_FLOW_NEED_DATA;
    } else {
      GST_ERROR_OBJECT (self, "query output returned %" GST_AMF_RESULT_FORMAT,
          GST_AMF_RESULT_ARGS (result));
      ret = GST_FLOW_ERROR;
    }
  } while (ret == GST_FLOW_OK);

  return ret;
}

static gboolean
gst_amf_encoder_drain (GstAmfEncoder * self, gboolean flushing)
{
  GstAmfEncoderPrivate *priv = self->priv;
  AMF_RESULT result;

  if (!priv->comp)
    return TRUE;

  GST_DEBUG_OBJECT (self, "%s", flushing ? "Flush" : "Drain");
  if (flushing)
    goto done;

  result = priv->comp->Drain ();
  if (result != AMF_OK) {
    GST_WARNING_OBJECT (self, "Drain returned %" GST_AMF_RESULT_FORMAT,
        GST_AMF_RESULT_ARGS (result));
    goto done;
  }

  gst_amf_encoder_try_output (self, TRUE);

done:
  gst_amf_encoder_reset (self);

  return TRUE;
}

static gboolean
gst_amf_encoder_stop (GstVideoEncoder * encoder)
{
  GstAmfEncoder *self = GST_AMF_ENCODER (encoder);
  GstAmfEncoderPrivate *priv = self->priv;

  GST_DEBUG_OBJECT (self, "Stop");

  gst_amf_encoder_drain (self, TRUE);

  g_clear_pointer (&priv->input_state, gst_video_codec_state_unref);

  return TRUE;
}

static gboolean
gst_amf_encoder_close (GstVideoEncoder * encoder)
{
  GstAmfEncoder *self = GST_AMF_ENCODER (encoder);
  GstAmfEncoderPrivate *priv = self->priv;

  GST_DEBUG_OBJECT (self, "Close");

  if (priv->context) {
    priv->context->Terminate ();
    priv->context->Release ();
    priv->context = nullptr;
  }

  gst_clear_d3d11_fence (&priv->fence);
  gst_clear_object (&priv->device);

  return TRUE;
}

static gboolean
gst_amf_encoder_prepare_internal_pool (GstAmfEncoder * self)
{
  GstAmfEncoderPrivate *priv = self->priv;
  GstVideoInfo *info = &priv->input_state->info;
  GstCaps *caps = priv->input_state->caps;
  GstStructure *config;
  GstD3D11AllocationParams *params;

  if (priv->internal_pool) {
    gst_buffer_pool_set_active (priv->internal_pool, FALSE);
    gst_clear_object (&priv->internal_pool);
  }

  priv->internal_pool = gst_d3d11_buffer_pool_new (priv->device);
  config = gst_buffer_pool_get_config (priv->internal_pool);
  gst_buffer_pool_config_set_params (config, caps,
      GST_VIDEO_INFO_SIZE (info), 0, 0);

  params = gst_d3d11_allocation_params_new (priv->device, info,
      GST_D3D11_ALLOCATION_FLAG_DEFAULT, D3D11_BIND_SHADER_RESOURCE,
      D3D11_RESOURCE_MISC_SHARED);

  gst_buffer_pool_config_set_d3d11_allocation_params (config, params);
  gst_d3d11_allocation_params_free (params);

  if (!gst_buffer_pool_set_config (priv->internal_pool, config)) {
    GST_ERROR_OBJECT (self, "Failed to set config");
    gst_clear_object (&priv->internal_pool);
    return FALSE;
  }

  if (!gst_buffer_pool_set_active (priv->internal_pool, TRUE)) {
    GST_ERROR_OBJECT (self, "Failed to set active");
    gst_clear_object (&priv->internal_pool);
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_amf_encoder_open_component (GstAmfEncoder * self)
{
  GstAmfEncoderClass *klass = GST_AMF_ENCODER_GET_CLASS (self);
  GstAmfEncoderPrivate *priv = self->priv;
  AMFFactory *factory = (AMFFactory *) gst_amf_get_factory ();
  AMFComponentPtr comp;
  AMF_RESULT result;
  guint num_reorder_frames = 0;

  gst_amf_encoder_drain (self, FALSE);

  if (!gst_amf_encoder_prepare_internal_pool (self))
    return FALSE;

  result = factory->CreateComponent (priv->context, priv->codec_id, &comp);
  if (result != AMF_OK) {
    GST_ERROR_OBJECT (self, "Failed to create component, result %"
        GST_AMF_RESULT_FORMAT, GST_AMF_RESULT_ARGS (result));
    return FALSE;
  }

  if (!klass->set_format (self, priv->input_state, comp.GetPtr (),
          &num_reorder_frames)) {
    GST_ERROR_OBJECT (self, "Failed to set format");
    return FALSE;
  }

  if (!klass->set_output_state (self, priv->input_state, comp.GetPtr ())) {
    GST_ERROR_OBJECT (self, "Failed to set output state");
    return FALSE;
  }

  priv->comp = comp.Detach ();

  if (num_reorder_frames > 0) {
    gint fps_n = 25;
    gint fps_d = 1;

    if (priv->input_state->info.fps_n > 0 && priv->input_state->info.fps_d > 0) {
      fps_n = priv->input_state->info.fps_n;
      fps_d = priv->input_state->info.fps_d;
    }

    priv->dts_offset = gst_util_uint64_scale (GST_SECOND, fps_d, fps_n) *
        num_reorder_frames;
  }

  return TRUE;
}

static gboolean
gst_amf_encoder_set_format (GstVideoEncoder * encoder,
    GstVideoCodecState * state)
{
  GstAmfEncoder *self = GST_AMF_ENCODER (encoder);
  GstAmfEncoderPrivate *priv = self->priv;

  g_clear_pointer (&priv->input_state, gst_video_codec_state_unref);
  priv->input_state = gst_video_codec_state_ref (state);

  return gst_amf_encoder_open_component (self);
}

static GstBuffer *
gst_amf_encoder_upload_sysmem (GstAmfEncoder * self, GstBuffer * src_buf,
    const GstVideoInfo * info)
{
  GstAmfEncoderPrivate *priv = self->priv;
  GstVideoFrame src_frame, dst_frame;
  GstBuffer *dst_buf;
  GstFlowReturn ret;

  GST_TRACE_OBJECT (self, "Uploading sysmem buffer");

  ret = gst_buffer_pool_acquire_buffer (priv->internal_pool, &dst_buf, nullptr);
  if (ret != GST_FLOW_OK) {
    GST_ERROR_OBJECT (self, "Failed to acquire buffer");
    return nullptr;
  }

  if (!gst_video_frame_map (&src_frame, info, src_buf, GST_MAP_READ)) {
    GST_WARNING ("Failed to map src frame");
    gst_buffer_unref (dst_buf);
    return nullptr;
  }

  if (!gst_video_frame_map (&dst_frame, info, dst_buf, GST_MAP_WRITE)) {
    GST_WARNING ("Failed to map src frame");
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
gst_amf_encoder_copy_d3d11 (GstAmfEncoder * self, GstBuffer * src_buffer,
    gboolean shared)
{
  GstAmfEncoderPrivate *priv = self->priv;
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

  ret = gst_buffer_pool_acquire_buffer (priv->internal_pool,
      &dst_buffer, nullptr);
  if (ret != GST_FLOW_OK) {
    GST_ERROR_OBJECT (self, "Failed to acquire buffer");
    return nullptr;
  }

  src_mem = gst_buffer_peek_memory (src_buffer, 0);
  dst_mem = gst_buffer_peek_memory (dst_buffer, 0);

  device = GST_D3D11_MEMORY_CAST (src_mem)->device;

  device_handle = gst_d3d11_device_get_device_handle (device);
  device_context = gst_d3d11_device_get_device_context_handle (device);

  if (!gst_memory_map (src_mem, &src_info,
          (GstMapFlags) (GST_MAP_READ | GST_MAP_D3D11))) {
    GST_ERROR_OBJECT (self, "Failed to map src memory");
    gst_buffer_unref (dst_buffer);
    return nullptr;
  }

  if (!gst_memory_map (dst_mem, &dst_info,
          (GstMapFlags) (GST_MAP_WRITE | GST_MAP_D3D11))) {
    GST_ERROR_OBJECT (self, "Failed to map dst memory");
    gst_memory_unmap (src_mem, &src_info);
    gst_buffer_unref (dst_buffer);
    return nullptr;
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

  return nullptr;
}

static GstBuffer *
gst_amf_encoder_upload_buffer (GstAmfEncoder * self, GstBuffer * buffer)
{
  GstAmfEncoderPrivate *priv = self->priv;
  GstVideoInfo *info = &priv->input_state->info;
  GstMemory *mem;
  GstD3D11Memory *dmem;
  D3D11_TEXTURE2D_DESC desc;
  GstBuffer *ret;

  mem = gst_buffer_peek_memory (buffer, 0);
  if (!gst_is_d3d11_memory (mem) || gst_buffer_n_memory (buffer) > 1) {
    /* d3d11 buffer should hold single memory object */
    return gst_amf_encoder_upload_sysmem (self, buffer, info);
  }

  dmem = GST_D3D11_MEMORY_CAST (mem);
  if (dmem->device != priv->device) {
    gint64 adapter_luid;

    g_object_get (dmem->device, "adapter-luid", &adapter_luid, nullptr);
    if (adapter_luid == priv->adapter_luid) {
      GST_LOG_OBJECT (self, "Different device but same GPU, copy d3d11");
      gst_d3d11_device_lock (priv->device);
      ret = gst_amf_encoder_copy_d3d11 (self, buffer, TRUE);
      gst_d3d11_device_unlock (priv->device);

      return ret;
    } else {
      GST_LOG_OBJECT (self, "Different device, system copy");
      return gst_amf_encoder_upload_sysmem (self, buffer, info);
    }
  }

  gst_d3d11_memory_get_texture_desc (dmem, &desc);
  if (desc.Usage != D3D11_USAGE_DEFAULT
      || (desc.BindFlags & D3D11_BIND_SHADER_RESOURCE) == 0) {
    GST_TRACE_OBJECT (self, "Not a default usage texture, d3d11 copy");
    gst_d3d11_device_lock (priv->device);
    ret = gst_amf_encoder_copy_d3d11 (self, buffer, FALSE);
    gst_d3d11_device_unlock (priv->device);

    return ret;
  }

  return gst_buffer_ref (buffer);
}

static void
gst_amf_frame_data_free (GstAmfEncoderFrameData * data)
{
  if (!data)
    return;

  gst_buffer_unmap (data->buffer, &data->info);
  gst_buffer_unref (data->buffer);
  g_free (data);
}

static GstFlowReturn
gst_amf_encoder_submit_input (GstAmfEncoder * self, GstVideoCodecFrame * frame,
    AMFSurface * surface)
{
  GstAmfEncoderPrivate *priv = self->priv;
  AMF_RESULT result;
  GstFlowReturn ret = GST_FLOW_OK;

  do {
    result = priv->comp->SubmitInput (surface);
    if (result == AMF_OK || result == AMF_NEED_MORE_INPUT) {
      GST_TRACE_OBJECT (self, "SubmitInput returned %" GST_AMF_RESULT_FORMAT,
          GST_AMF_RESULT_ARGS (result));
      ret = GST_FLOW_OK;
      if (GST_CLOCK_TIME_IS_VALID (frame->pts))
        priv->timestamp_queue.push (frame->pts);
      break;
    }

    if (result != AMF_INPUT_FULL) {
      GST_ERROR_OBJECT (self, "SubmitInput returned %" GST_AMF_RESULT_FORMAT,
          GST_AMF_RESULT_ARGS (result));
      ret = GST_FLOW_ERROR;
      break;
    }

    /* When submit queue is full, QueryInput() that returns no buffer MUST be
     * followed by another SubmitInput(), otherwise no buffer will ever get
     * returned. Therefore we're passing FALSE as do_wait here. */
    ret = gst_amf_encoder_try_output (self, FALSE);
    if (ret == GST_AMF_ENCODER_FLOW_TRY_AGAIN) {
      g_usleep (1000);
    } else if (ret != GST_FLOW_OK) {
      GST_INFO_OBJECT (self, "Try output returned %s", gst_flow_get_name (ret));
      break;
    }
  } while (TRUE);

  return ret;
}

static GstFlowReturn
gst_amf_encoder_handle_frame (GstVideoEncoder * encoder,
    GstVideoCodecFrame * frame)
{
  GstAmfEncoder *self = GST_AMF_ENCODER (encoder);
  GstAmfEncoderClass *klass = GST_AMF_ENCODER_GET_CLASS (self);
  GstAmfEncoderPrivate *priv = self->priv;
  GstVideoInfo *info = &priv->input_state->info;
  GstBuffer *buffer;
  AMFBufferPtr user_data;
  AMFSurfacePtr surface;
  AMF_RESULT result;
  guint32 *system_frame_number;
  guint subresource_index;
  GstAmfEncoderFrameData *frame_data;
  ID3D11Texture2D *texture;
  gboolean need_reconfigure;
  GstFlowReturn ret;

  if (!priv->comp && !gst_amf_encoder_open_component (self)) {
    GST_ERROR_OBJECT (self, "Encoder object was not configured");
    goto error;
  }

  need_reconfigure = klass->check_reconfigure (self);
  if (need_reconfigure && !gst_amf_encoder_open_component (self)) {
    GST_ERROR_OBJECT (self, "Failed to reconfigure encoder");
    goto error;
  }

  result = priv->context->AllocBuffer (AMF_MEMORY_HOST,
      sizeof (guint32), &user_data);
  if (result != AMF_OK) {
    GST_ERROR_OBJECT (self, "Failed to allocate user data buffer, result %"
        GST_AMF_RESULT_FORMAT, GST_AMF_RESULT_ARGS (result));
    goto error;
  }

  system_frame_number = (guint32 *) user_data->GetNative ();
  *system_frame_number = frame->system_frame_number;

  buffer = gst_amf_encoder_upload_buffer (self, frame->input_buffer);
  if (!buffer)
    goto error;

  frame_data = g_new0 (GstAmfEncoderFrameData, 1);
  frame_data->buffer = buffer;
  gst_buffer_map (frame_data->buffer, &frame_data->info,
      (GstMapFlags) (GST_MAP_READ | GST_MAP_D3D11));
  gst_video_codec_frame_set_user_data (frame, frame_data,
      (GDestroyNotify) gst_amf_frame_data_free);

  subresource_index = GPOINTER_TO_UINT (frame_data->info.user_data[0]);

  gst_d3d11_device_lock (priv->device);
  texture = (ID3D11Texture2D *) frame_data->info.data;
  texture->SetPrivateData (AMFTextureArrayIndexGUID,
      sizeof (guint), &subresource_index);
  result = priv->context->CreateSurfaceFromDX11Native (texture,
      &surface, nullptr);
  gst_d3d11_device_unlock (priv->device);
  if (result != AMF_OK) {
    GST_ERROR_OBJECT (self, "Failed to create surface, result %"
        GST_AMF_RESULT_FORMAT, GST_AMF_RESULT_ARGS (result));
    goto error;
  }

  surface->SetCrop (0, 0, info->width, info->height);
  surface->SetPts (frame->pts / 100);
  if (GST_CLOCK_TIME_IS_VALID (frame->duration))
    surface->SetDuration (frame->duration / 100);

  result = surface->SetProperty (GST_AMF_BUFFER_PROP, user_data);
  if (result != AMF_OK) {
    GST_ERROR_OBJECT (self, "Failed to set user data on AMF surface");
    goto error;
  }

  klass->set_surface_prop (self, frame, surface.GetPtr ());
  gst_video_codec_frame_unref (frame);

  ret = gst_amf_encoder_submit_input (self, frame, surface.GetPtr ());
  if (ret == GST_FLOW_OK)
    ret = gst_amf_encoder_try_output (self, FALSE);
  if (ret == GST_AMF_ENCODER_FLOW_TRY_AGAIN)
    ret = GST_FLOW_OK;

  return ret;

error:
  gst_video_encoder_finish_frame (encoder, frame);

  return GST_FLOW_ERROR;
}

static GstFlowReturn
gst_amf_encoder_finish (GstVideoEncoder * encoder)
{
  GstAmfEncoder *self = GST_AMF_ENCODER (encoder);

  GST_DEBUG_OBJECT (self, "Finish");

  gst_amf_encoder_drain (self, FALSE);

  return GST_FLOW_OK;
}

static gboolean
gst_amf_encoder_flush (GstVideoEncoder * encoder)
{
  GstAmfEncoder *self = GST_AMF_ENCODER (encoder);

  GST_DEBUG_OBJECT (self, "Flush");

  gst_amf_encoder_drain (self, TRUE);

  return TRUE;
}

static gboolean
gst_amf_encoder_handle_context_query (GstAmfEncoder * self, GstQuery * query)
{
  GstAmfEncoderPrivate *priv = self->priv;

  return gst_d3d11_handle_context_query (GST_ELEMENT (self), query,
      priv->device);
}

static gboolean
gst_amf_encoder_sink_query (GstVideoEncoder * encoder, GstQuery * query)
{
  GstAmfEncoder *self = GST_AMF_ENCODER (encoder);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CONTEXT:
      if (gst_amf_encoder_handle_context_query (self, query))
        return TRUE;
      break;
    default:
      break;
  }

  return GST_VIDEO_ENCODER_CLASS (parent_class)->sink_query (encoder, query);
}

static gboolean
gst_amf_encoder_src_query (GstVideoEncoder * encoder, GstQuery * query)
{
  GstAmfEncoder *self = GST_AMF_ENCODER (encoder);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CONTEXT:
      if (gst_amf_encoder_handle_context_query (self, query))
        return TRUE;
      break;
    default:
      break;
  }

  return GST_VIDEO_ENCODER_CLASS (parent_class)->src_query (encoder, query);
}

static gboolean
gst_amf_encoder_propose_allocation (GstVideoEncoder * encoder, GstQuery * query)
{
  GstAmfEncoder *self = GST_AMF_ENCODER (encoder);
  GstAmfEncoderPrivate *priv = self->priv;
  GstD3D11Device *device = GST_D3D11_DEVICE (priv->device);
  GstVideoInfo info;
  GstBufferPool *pool;
  GstCaps *caps;
  guint size;
  GstStructure *config;
  GstCapsFeatures *features;
  gboolean is_d3d11 = FALSE;
  guint min_buffers = 0;
  GstD3D11AllocationParams *params;

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

    /* XXX: AMF API does not provide information about internal queue size,
     * use hardcoded value 16 */
    min_buffers = 16;
  } else {
    pool = gst_video_buffer_pool_new ();
  }

  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_add_option (config, GST_BUFFER_POOL_OPTION_VIDEO_META);
  if (!is_d3d11) {
    gst_buffer_pool_config_add_option (config,
        GST_BUFFER_POOL_OPTION_VIDEO_ALIGNMENT);
  }

  size = GST_VIDEO_INFO_SIZE (&info);
  gst_buffer_pool_config_set_params (config, caps, size, min_buffers, 0);

  params = gst_d3d11_allocation_params_new (device, &info,
      GST_D3D11_ALLOCATION_FLAG_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0);
  gst_buffer_pool_config_set_d3d11_allocation_params (config, params);
  gst_d3d11_allocation_params_free (params);

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

  gst_query_add_allocation_pool (query, pool, size, min_buffers, 0);
  gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, nullptr);
  gst_object_unref (pool);

  return TRUE;
}

/* *INDENT-OFF* */
void
gst_amf_encoder_set_subclass_data (GstAmfEncoder * encoder, gint64 adapter_luid,
    const wchar_t * codec_id)
{
  GstAmfEncoderPrivate *priv;

  g_return_if_fail (GST_IS_AMF_ENCODER (encoder));

  priv = encoder->priv;
  priv->adapter_luid = adapter_luid;
  priv->codec_id = codec_id;
}
/* *INDENT-ON* */

AMF_RESULT
gst_amf_encoder_set_pre_analysis_options (GstAmfEncoder * self,
    AMFComponent * comp, const GstAmfEncoderPreAnalysis * pa,
    GstAmfEncoderPASupportedOptions * pa_supported)
{
  AMF_RESULT result;
  if (pa_supported->activity_type) {
    result = comp->SetProperty (AMF_PA_ACTIVITY_TYPE,
        (AMF_PA_ACTIVITY_TYPE_ENUM) pa->activity_type);
    if (result != AMF_OK) {
      GST_ERROR_OBJECT (self,
          "Failed to set pre-analysis activity type, result %"
          GST_AMF_RESULT_FORMAT, GST_AMF_RESULT_ARGS (result));
      return result;
    }
  }
  if (pa_supported->scene_change_detection) {
    result = comp->SetProperty (AMF_PA_SCENE_CHANGE_DETECTION_ENABLE,
        (amf_bool) pa->scene_change_detection);
    if (result != AMF_OK) {
      GST_ERROR_OBJECT (self,
          "Failed to set pre-analysis scene change detection, result %"
          GST_AMF_RESULT_FORMAT, GST_AMF_RESULT_ARGS (result));
      return result;
    }
  }
  if (pa_supported->scene_change_detection_sensitivity) {
    result = comp->SetProperty (AMF_PA_SCENE_CHANGE_DETECTION_SENSITIVITY,
        (AMF_PA_SCENE_CHANGE_DETECTION_SENSITIVITY_ENUM)
        pa->scene_change_detection_sensitivity);
    if (result != AMF_OK) {
      GST_ERROR_OBJECT (self,
          "Failed to set pre-analysis scene change detection sensitivity, result %"
          GST_AMF_RESULT_FORMAT, GST_AMF_RESULT_ARGS (result));
      return result;
    }
  }
  if (pa_supported->static_scene_detection) {
    result = comp->SetProperty (AMF_PA_STATIC_SCENE_DETECTION_ENABLE,
        (amf_bool) pa->static_scene_detection);
    if (result != AMF_OK) {
      GST_ERROR_OBJECT (self,
          "Failed to set pre-analysis static scene detection, result %"
          GST_AMF_RESULT_FORMAT, GST_AMF_RESULT_ARGS (result));
      return result;
    }
  }
  if (pa_supported->static_scene_detection_sensitivity) {
    result = comp->SetProperty (AMF_PA_STATIC_SCENE_DETECTION_SENSITIVITY,
        (AMF_PA_STATIC_SCENE_DETECTION_SENSITIVITY_ENUM)
        pa->static_scene_detection_sensitivity);
    if (result != AMF_OK) {
      GST_ERROR_OBJECT (self,
          "Failed to set pre-analysis static scene detection sensitivity, result %"
          GST_AMF_RESULT_FORMAT, GST_AMF_RESULT_ARGS (result));
      return result;
    }
  }
  if (pa_supported->initial_qp) {
    result = comp->SetProperty (AMF_PA_INITIAL_QP_AFTER_SCENE_CHANGE,
        (amf_int64) pa->initial_qp);
    if (result != AMF_OK) {
      GST_ERROR_OBJECT (self, "Failed to set pre-analysis initial QP, result %"
          GST_AMF_RESULT_FORMAT, GST_AMF_RESULT_ARGS (result));
      return result;
    }
  }
  if (pa_supported->max_qp) {
    result = comp->SetProperty (AMF_PA_MAX_QP_BEFORE_FORCE_SKIP,
        (amf_int64) pa->max_qp);
    if (result != AMF_OK) {
      GST_ERROR_OBJECT (self, "Failed to set pre-analysis max QP, result %"
          GST_AMF_RESULT_FORMAT, GST_AMF_RESULT_ARGS (result));
      return result;
    }
  }
  if (pa_supported->caq_strength) {
    result = comp->SetProperty (AMF_PA_CAQ_STRENGTH,
        (amf_int64) pa->caq_strength);
    if (result != AMF_OK) {
      GST_ERROR_OBJECT (self,
          "Failed to set pre-analysis CAQ strength, result %"
          GST_AMF_RESULT_FORMAT, GST_AMF_RESULT_ARGS (result));
      return result;
    }
  }
  if (pa_supported->frame_sad) {
    result = comp->SetProperty (AMF_PA_FRAME_SAD_ENABLE,
        (amf_bool) pa->frame_sad);
    if (result != AMF_OK) {
      GST_ERROR_OBJECT (self,
          "Failed to set pre-analysis frame SAD algorithm, result %"
          GST_AMF_RESULT_FORMAT, GST_AMF_RESULT_ARGS (result));
      return result;
    }
  }
  if (pa_supported->ltr) {
    result = comp->SetProperty (AMF_PA_LTR_ENABLE, (amf_bool) pa->ltr);
    if (result != AMF_OK) {
      GST_ERROR_OBJECT (self,
          "Failed to set pre-analysis automatic Long Term Reference frame management, result %"
          GST_AMF_RESULT_FORMAT, GST_AMF_RESULT_ARGS (result));
      return result;
    }
  }
  if (pa_supported->lookahead_buffer_depth) {
    result = comp->SetProperty (AMF_PA_LOOKAHEAD_BUFFER_DEPTH,
        (amf_int64) pa->lookahead_buffer_depth);
    if (result != AMF_OK) {
      GST_ERROR_OBJECT (self,
          "Failed to set pre-analysis lookahead buffer depth, result %"
          GST_AMF_RESULT_FORMAT, GST_AMF_RESULT_ARGS (result));
      return result;
    }
  }
  if (pa_supported->paq_mode) {
    result = comp->SetProperty (AMF_PA_PAQ_MODE, (amf_int64) pa->paq_mode);
    if (result != AMF_OK) {
      GST_ERROR_OBJECT (self, "Failed to set pre-analysis PAQ mode, result %"
          GST_AMF_RESULT_FORMAT, GST_AMF_RESULT_ARGS (result));
      return result;
    }
  }
  if (pa_supported->taq_mode) {
    result = comp->SetProperty (AMF_PA_TAQ_MODE, (amf_int64) pa->taq_mode);
    if (result != AMF_OK) {
      GST_ERROR_OBJECT (self, "Failed to set pre-analysis TAQ mode, result %"
          GST_AMF_RESULT_FORMAT, GST_AMF_RESULT_ARGS (result));
      return result;
    }
  }
  if (pa_supported->hmqb_mode) {
    result = comp->SetProperty (AMF_PA_HIGH_MOTION_QUALITY_BOOST_MODE,
        (amf_int64) pa->hmqb_mode);
    if (result != AMF_OK) {
      GST_ERROR_OBJECT (self,
          "Failed to set pre-analysis high motion quality boost mode, result %"
          GST_AMF_RESULT_FORMAT, GST_AMF_RESULT_ARGS (result));
      return result;
    }
  }
  return AMF_OK;
}

void
gst_amf_encoder_check_pa_supported_options (GstAmfEncoderPASupportedOptions *
    pa_supported, AMFComponent * comp)
{
  if (comp->HasProperty (AMF_PA_ACTIVITY_TYPE))
    pa_supported->activity_type = TRUE;
  if (comp->HasProperty (AMF_PA_SCENE_CHANGE_DETECTION_ENABLE))
    pa_supported->scene_change_detection = TRUE;
  if (comp->HasProperty (AMF_PA_SCENE_CHANGE_DETECTION_SENSITIVITY))
    pa_supported->scene_change_detection_sensitivity = TRUE;
  if (comp->HasProperty (AMF_PA_STATIC_SCENE_DETECTION_ENABLE))
    pa_supported->static_scene_detection = TRUE;
  if (comp->HasProperty (AMF_PA_STATIC_SCENE_DETECTION_SENSITIVITY))
    pa_supported->static_scene_detection_sensitivity = TRUE;
  if (comp->HasProperty (AMF_PA_INITIAL_QP_AFTER_SCENE_CHANGE))
    pa_supported->initial_qp = TRUE;
  if (comp->HasProperty (AMF_PA_MAX_QP_BEFORE_FORCE_SKIP))
    pa_supported->max_qp = TRUE;
  if (comp->HasProperty (AMF_PA_CAQ_STRENGTH))
    pa_supported->caq_strength = TRUE;
  if (comp->HasProperty (AMF_PA_FRAME_SAD_ENABLE))
    pa_supported->frame_sad = TRUE;
  if (comp->HasProperty (AMF_PA_LTR_ENABLE))
    pa_supported->ltr = TRUE;
  if (comp->HasProperty (AMF_PA_LOOKAHEAD_BUFFER_DEPTH))
    pa_supported->lookahead_buffer_depth = TRUE;
  if (comp->HasProperty (AMF_PA_PAQ_MODE))
    pa_supported->paq_mode = TRUE;
  if (comp->HasProperty (AMF_PA_TAQ_MODE))
    pa_supported->taq_mode = TRUE;
  if (comp->HasProperty (AMF_PA_HIGH_MOTION_QUALITY_BOOST_MODE))
    pa_supported->hmqb_mode = TRUE;
}
