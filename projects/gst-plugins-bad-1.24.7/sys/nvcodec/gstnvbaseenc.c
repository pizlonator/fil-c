/* GStreamer NVENC plugin
 * Copyright (C) 2015 Centricular Ltd
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

#include "gstnvbaseenc.h"

#include <gst/pbutils/codec-utils.h>

#include <string.h>

GST_DEBUG_CATEGORY_EXTERN (gst_nvenc_debug);
#define GST_CAT_DEFAULT gst_nvenc_debug

#ifdef HAVE_CUDA_GST_GL
#include <gst/gl/gl.h>
#endif

/* This currently supports both 5.x and 6.x versions of the NvEncodeAPI.h
 * header which are mostly API compatible. */

#define SUPPORTED_GL_APIS GST_GL_API_OPENGL3

/* magic pointer value we can put in the async queue to signal shut down */
#define SHUTDOWN_COOKIE ((gpointer)GINT_TO_POINTER (1))

#define parent_class gst_nv_base_enc_parent_class
G_DEFINE_ABSTRACT_TYPE (GstNvBaseEnc, gst_nv_base_enc, GST_TYPE_VIDEO_ENCODER);

#define GST_TYPE_NV_PRESET (gst_nv_preset_get_type())
static GType
gst_nv_preset_get_type (void)
{
  static GType nv_preset_type = 0;

  static const GEnumValue presets[] = {
    {GST_NV_PRESET_DEFAULT, "Default", "default"},
    {GST_NV_PRESET_HP, "High Performance", "hp"},
    {GST_NV_PRESET_HQ, "High Quality", "hq"},
/*    {GST_NV_PRESET_BD, "BD", "bd"}, */
    {GST_NV_PRESET_LOW_LATENCY_DEFAULT, "Low Latency", "low-latency"},
    {GST_NV_PRESET_LOW_LATENCY_HQ, "Low Latency, High Quality",
        "low-latency-hq"},
    {GST_NV_PRESET_LOW_LATENCY_HP, "Low Latency, High Performance",
        "low-latency-hp"},
    {GST_NV_PRESET_LOSSLESS_DEFAULT, "Lossless", "lossless"},
    {GST_NV_PRESET_LOSSLESS_HP, "Lossless, High Performance", "lossless-hp"},
    {0, NULL, NULL},
  };

  if (!nv_preset_type) {
    nv_preset_type = g_enum_register_static ("GstNvPreset", presets);
  }
  return nv_preset_type;
}

static GUID
_nv_preset_to_guid (GstNvPreset preset)
{
  GUID null = { 0, };

  switch (preset) {
#define CASE(gst,nv) case G_PASTE(GST_NV_PRESET_,gst): return G_PASTE(G_PASTE(NV_ENC_PRESET_,nv),_GUID)
      CASE (DEFAULT, DEFAULT);
      CASE (HP, HP);
      CASE (HQ, HQ);
/*    CASE (BD, BD);*/
      CASE (LOW_LATENCY_DEFAULT, LOW_LATENCY_DEFAULT);
      CASE (LOW_LATENCY_HQ, LOW_LATENCY_HQ);
      CASE (LOW_LATENCY_HP, LOW_LATENCY_HQ);
      CASE (LOSSLESS_DEFAULT, LOSSLESS_DEFAULT);
      CASE (LOSSLESS_HP, LOSSLESS_HP);
#undef CASE
    default:
      return null;
  }
}

#define GST_TYPE_NV_RC_MODE (gst_nv_rc_mode_get_type())
static GType
gst_nv_rc_mode_get_type (void)
{
  static GType nv_rc_mode_type = 0;

  static const GEnumValue modes[] = {
    {GST_NV_RC_MODE_DEFAULT, "Default", "default"},
    {GST_NV_RC_MODE_CONSTQP, "Constant Quantization", "constqp"},
    {GST_NV_RC_MODE_CBR, "Constant Bit Rate", "cbr"},
    {GST_NV_RC_MODE_VBR, "Variable Bit Rate", "vbr"},
    {GST_NV_RC_MODE_VBR_MINQP,
        "Variable Bit Rate "
          "(with minimum quantization parameter, DEPRECATED)", "vbr-minqp"},
    {GST_NV_RC_MODE_CBR_LOWDELAY_HQ,
        "Low-Delay CBR, High Quality", "cbr-ld-hq"},
    {GST_NV_RC_MODE_CBR_HQ, "CBR, High Quality (slower)", "cbr-hq"},
    {GST_NV_RC_MODE_VBR_HQ, "VBR, High Quality (slower)", "vbr-hq"},
    {0, NULL, NULL},
  };

  if (!nv_rc_mode_type) {
    nv_rc_mode_type = g_enum_register_static ("GstNvRCMode", modes);
  }
  return nv_rc_mode_type;
}

static NV_ENC_PARAMS_RC_MODE
_rc_mode_to_nv (GstNvRCMode mode)
{
  switch (mode) {
    case GST_NV_RC_MODE_DEFAULT:
      return NV_ENC_PARAMS_RC_VBR;
#define CASE(gst,nv) case G_PASTE(GST_NV_RC_MODE_,gst): return G_PASTE(NV_ENC_PARAMS_RC_,nv)
      CASE (CONSTQP, CONSTQP);
      CASE (CBR, CBR);
      CASE (VBR, VBR);
      CASE (VBR_MINQP, VBR_MINQP);
      CASE (CBR_LOWDELAY_HQ, CBR_LOWDELAY_HQ);
      CASE (CBR_HQ, CBR_HQ);
      CASE (VBR_HQ, VBR_HQ);
#undef CASE
    default:
      return NV_ENC_PARAMS_RC_VBR;
  }
}

enum
{
  PROP_0,
  PROP_DEVICE_ID,
  PROP_PRESET,
  PROP_BITRATE,
  PROP_RC_MODE,
  PROP_QP_MIN,
  PROP_QP_MAX,
  PROP_QP_CONST,
  PROP_GOP_SIZE,
  PROP_MAX_BITRATE,
  PROP_SPATIAL_AQ,
  PROP_AQ_STRENGTH,
  PROP_NON_REF_P,
  PROP_ZEROLATENCY,
  PROP_STRICT_GOP,
  PROP_CONST_QUALITY,
  PROP_I_ADAPT,
  PROP_QP_MIN_I,
  PROP_QP_MIN_P,
  PROP_QP_MIN_B,
  PROP_QP_MAX_I,
  PROP_QP_MAX_P,
  PROP_QP_MAX_B,
  PROP_QP_CONST_I,
  PROP_QP_CONST_P,
  PROP_QP_CONST_B,
};

#define DEFAULT_PRESET GST_NV_PRESET_DEFAULT
#define DEFAULT_BITRATE 0
#define DEFAULT_RC_MODE GST_NV_RC_MODE_DEFAULT
#define DEFAULT_QP_MIN -1
#define DEFAULT_QP_MAX -1
#define DEFAULT_QP_CONST -1
#define DEFAULT_GOP_SIZE 75
#define DEFAULT_MAX_BITRATE 0
#define DEFAULT_SPATIAL_AQ FALSE
#define DEFAULT_AQ_STRENGTH 0
#define DEFAULT_NON_REF_P FALSE
#define DEFAULT_ZEROLATENCY FALSE
#define DEFAULT_STRICT_GOP FALSE
#define DEFAULT_CONST_QUALITY 0
#define DEFAULT_I_ADAPT FALSE
#define DEFAULT_QP_DETAIL -1

/* This lock is needed to prevent the situation where multiple encoders are
 * initialised at the same time which appears to cause excessive CPU usage over
 * some period of time. */
G_LOCK_DEFINE_STATIC (initialization_lock);

typedef struct
{
  /* Allocated CUDA device memory and registered to NVENC to be used as input
   * buffer regardless of the input memory type (OpenGL or System memory) */
  CUdeviceptr cuda_pointer;

  /* The stride of allocated CUDA device memory (CuMemAllocPitch).
   * This might be different from the stride of GstVideoInfo */
  gsize cuda_stride;

  /* Registered NVENC resource (cuda_pointer is used for this) */
  NV_ENC_REGISTER_RESOURCE nv_resource;

  /* Mapped resource of nv_resource */
  NV_ENC_MAP_INPUT_RESOURCE nv_mapped_resource;

  /* whether nv_mapped_resource was mapped via NvEncMapInputResource()
   * and therefore should unmap via NvEncUnmapInputResource or not */
  gboolean mapped;
} GstNvEncInputResource;

/* The pair of GstNvEncInputResource () and NV_ENC_OUTPUT_PTR.
 * The number of input/output resource are always identical */
typedef struct
{
  GstNvEncInputResource *in_buf;
  NV_ENC_OUTPUT_PTR out_buf;
} GstNvEncFrameState;

static gboolean gst_nv_base_enc_open (GstVideoEncoder * enc);
static gboolean gst_nv_base_enc_close (GstVideoEncoder * enc);
static gboolean gst_nv_base_enc_start (GstVideoEncoder * enc);
static gboolean gst_nv_base_enc_stop (GstVideoEncoder * enc);
static void gst_nv_base_enc_set_context (GstElement * element,
    GstContext * context);
static gboolean gst_nv_base_enc_sink_query (GstVideoEncoder * enc,
    GstQuery * query);
static gboolean gst_nv_base_enc_sink_event (GstVideoEncoder * enc,
    GstEvent * event);
static gboolean gst_nv_base_enc_set_format (GstVideoEncoder * enc,
    GstVideoCodecState * state);
static GstFlowReturn gst_nv_base_enc_handle_frame (GstVideoEncoder * enc,
    GstVideoCodecFrame * frame);
static void gst_nv_base_enc_free_buffers (GstNvBaseEnc * nvenc);
static GstFlowReturn gst_nv_base_enc_finish (GstVideoEncoder * enc);
static void gst_nv_base_enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_nv_base_enc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_nv_base_enc_finalize (GObject * obj);
static GstCaps *gst_nv_base_enc_getcaps (GstVideoEncoder * enc,
    GstCaps * filter);
static gboolean gst_nv_base_enc_stop_bitstream_thread (GstNvBaseEnc * nvenc,
    gboolean force);
static gboolean gst_nv_base_enc_drain_encoder (GstNvBaseEnc * nvenc);
static gboolean gst_nv_base_enc_propose_allocation (GstVideoEncoder * enc,
    GstQuery * query);

static void
gst_nv_base_enc_class_init (GstNvBaseEncClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstVideoEncoderClass *videoenc_class = GST_VIDEO_ENCODER_CLASS (klass);

  gobject_class->set_property = gst_nv_base_enc_set_property;
  gobject_class->get_property = gst_nv_base_enc_get_property;
  gobject_class->finalize = gst_nv_base_enc_finalize;

  element_class->set_context = GST_DEBUG_FUNCPTR (gst_nv_base_enc_set_context);

  videoenc_class->open = GST_DEBUG_FUNCPTR (gst_nv_base_enc_open);
  videoenc_class->close = GST_DEBUG_FUNCPTR (gst_nv_base_enc_close);

  videoenc_class->start = GST_DEBUG_FUNCPTR (gst_nv_base_enc_start);
  videoenc_class->stop = GST_DEBUG_FUNCPTR (gst_nv_base_enc_stop);

  videoenc_class->set_format = GST_DEBUG_FUNCPTR (gst_nv_base_enc_set_format);
  videoenc_class->getcaps = GST_DEBUG_FUNCPTR (gst_nv_base_enc_getcaps);
  videoenc_class->handle_frame =
      GST_DEBUG_FUNCPTR (gst_nv_base_enc_handle_frame);
  videoenc_class->finish = GST_DEBUG_FUNCPTR (gst_nv_base_enc_finish);
  videoenc_class->sink_query = GST_DEBUG_FUNCPTR (gst_nv_base_enc_sink_query);
  videoenc_class->sink_event = GST_DEBUG_FUNCPTR (gst_nv_base_enc_sink_event);
  videoenc_class->propose_allocation =
      GST_DEBUG_FUNCPTR (gst_nv_base_enc_propose_allocation);

  g_object_class_install_property (gobject_class, PROP_DEVICE_ID,
      g_param_spec_uint ("cuda-device-id",
          "Cuda Device ID",
          "Get the GPU device to use for operations",
          0, G_MAXUINT, 0, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_PRESET,
      g_param_spec_enum ("preset", "Encoding Preset",
          "Encoding Preset",
          GST_TYPE_NV_PRESET, DEFAULT_PRESET,
          G_PARAM_READWRITE | GST_PARAM_MUTABLE_PLAYING |
          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_RC_MODE,
      g_param_spec_enum ("rc-mode", "RC Mode", "Rate Control Mode",
          GST_TYPE_NV_RC_MODE, DEFAULT_RC_MODE,
          G_PARAM_READWRITE | GST_PARAM_MUTABLE_PLAYING |
          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_QP_MIN,
      g_param_spec_int ("qp-min", "Minimum Quantizer",
          "Minimum quantizer (-1 = from NVENC preset)", -1, 51, DEFAULT_QP_MIN,
          G_PARAM_READWRITE | GST_PARAM_MUTABLE_PLAYING |
          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_QP_MAX,
      g_param_spec_int ("qp-max", "Maximum Quantizer",
          "Maximum quantizer (-1 = from NVENC preset)", -1, 51, DEFAULT_QP_MAX,
          G_PARAM_READWRITE | GST_PARAM_MUTABLE_PLAYING |
          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_QP_CONST,
      g_param_spec_int ("qp-const", "Constant Quantizer",
          "Constant quantizer (-1 = from NVENC preset)", -1, 51,
          DEFAULT_QP_CONST,
          G_PARAM_READWRITE | GST_PARAM_MUTABLE_PLAYING |
          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_GOP_SIZE,
      g_param_spec_int ("gop-size", "GOP size",
          "Number of frames between intra frames (-1 = infinite)",
          -1, G_MAXINT, DEFAULT_GOP_SIZE,
          (GParamFlags) (G_PARAM_READWRITE | GST_PARAM_MUTABLE_PLAYING |
              G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (gobject_class, PROP_BITRATE,
      g_param_spec_uint ("bitrate", "Bitrate",
          "Bitrate in kbit/sec (0 = from NVENC preset)", 0, 2000 * 1024,
          DEFAULT_BITRATE,
          G_PARAM_READWRITE | GST_PARAM_MUTABLE_PLAYING |
          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_MAX_BITRATE,
      g_param_spec_uint ("max-bitrate", "Max Bitrate",
          "Maximum Bitrate in kbit/sec (ignored for CBR mode)", 0, 2000 * 1024,
          DEFAULT_MAX_BITRATE,
          G_PARAM_READWRITE | GST_PARAM_MUTABLE_PLAYING |
          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_SPATIAL_AQ,
      g_param_spec_boolean ("spatial-aq", "Spatial AQ",
          "Spatial Adaptive Quantization",
          DEFAULT_SPATIAL_AQ,
          G_PARAM_READWRITE | GST_PARAM_MUTABLE_PLAYING |
          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_AQ_STRENGTH,
      g_param_spec_uint ("aq-strength", "AQ Strength",
          "Adaptive Quantization Strength when spatial-aq is enabled"
          " from 1 (low) to 15 (aggressive), (0 = autoselect)",
          0, 15, DEFAULT_AQ_STRENGTH,
          G_PARAM_READWRITE | GST_PARAM_MUTABLE_PLAYING |
          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_NON_REF_P,
      g_param_spec_boolean ("nonref-p", "Nonref P",
          "Automatic insertion of non-reference P-frames", DEFAULT_NON_REF_P,
          G_PARAM_READWRITE | GST_PARAM_MUTABLE_PLAYING |
          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_ZEROLATENCY,
      g_param_spec_boolean ("zerolatency", "Zerolatency",
          "Zero latency operation (no reordering delay)", DEFAULT_ZEROLATENCY,
          G_PARAM_READWRITE | GST_PARAM_MUTABLE_PLAYING |
          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_STRICT_GOP,
      g_param_spec_boolean ("strict-gop", "Strict GOP",
          "Minimize GOP-to-GOP rate fluctuations", DEFAULT_STRICT_GOP,
          G_PARAM_READWRITE | GST_PARAM_MUTABLE_PLAYING |
          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_CONST_QUALITY,
      g_param_spec_double ("const-quality", "Constant Quality",
          "Target Constant Quality level for VBR mode (0 = automatic)",
          0, 51, DEFAULT_CONST_QUALITY,
          G_PARAM_READWRITE | GST_PARAM_MUTABLE_PLAYING |
          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_I_ADAPT,
      g_param_spec_boolean ("i-adapt", "I Adapt",
          "Enable adaptive I-frame insert when lookahead is enabled",
          DEFAULT_I_ADAPT,
          G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_QP_MIN_I,
      g_param_spec_int ("qp-min-i", "QP Min I",
          "Minimum QP value for I frame, When >= 0, \"qp-min-p\" and "
          "\"qp-min-b\" should be also >= 0. Overwritten by \"qp-min\""
          " (-1 = from NVENC preset)", -1, 51,
          DEFAULT_QP_DETAIL,
          G_PARAM_READWRITE | GST_PARAM_MUTABLE_PLAYING |
          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_QP_MIN_P,
      g_param_spec_int ("qp-min-p", "QP Min P",
          "Minimum QP value for P frame, When >= 0, \"qp-min-i\" and "
          "\"qp-min-b\" should be also >= 0. Overwritten by \"qp-min\""
          " (-1 = from NVENC preset)", -1, 51,
          DEFAULT_QP_DETAIL,
          G_PARAM_READWRITE | GST_PARAM_MUTABLE_PLAYING |
          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_QP_MIN_B,
      g_param_spec_int ("qp-min-b", "QP Min B",
          "Minimum QP value for B frame, When >= 0, \"qp-min-i\" and "
          "\"qp-min-p\" should be also >= 0. Overwritten by \"qp-min\""
          " (-1 = from NVENC preset)", -1, 51,
          DEFAULT_QP_DETAIL,
          G_PARAM_READWRITE | GST_PARAM_MUTABLE_PLAYING |
          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_QP_MAX_I,
      g_param_spec_int ("qp-max-i", "QP Max I",
          "Maximum QP value for I frame, When >= 0, \"qp-max-p\" and "
          "\"qp-max-b\" should be also >= 0. Overwritten by \"qp-max\""
          " (-1 = from NVENC preset)", -1, 51,
          DEFAULT_QP_DETAIL,
          G_PARAM_READWRITE | GST_PARAM_MUTABLE_PLAYING |
          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_QP_MAX_P,
      g_param_spec_int ("qp-max-p", "QP Max P",
          "Maximum QP value for P frame, When >= 0, \"qp-max-i\" and "
          "\"qp-max-b\" should be also >= 0. Overwritten by \"qp-max\""
          " (-1 = from NVENC preset)", -1, 51,
          DEFAULT_QP_DETAIL,
          G_PARAM_READWRITE | GST_PARAM_MUTABLE_PLAYING |
          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_QP_MAX_B,
      g_param_spec_int ("qp-max-b", "QP Max B",
          "Maximum QP value for B frame, When >= 0, \"qp-max-i\" and "
          "\"qp-max-p\" should be also >= 0. Overwritten by \"qp-max\""
          " (-1 = from NVENC preset)", -1, 51,
          DEFAULT_QP_DETAIL,
          G_PARAM_READWRITE | GST_PARAM_MUTABLE_PLAYING |
          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_QP_CONST_I,
      g_param_spec_int ("qp-const-i", "QP Const I",
          "Constant QP value for I frame, When >= 0, \"qp-const-p\" and "
          "\"qp-const-b\" should be also >= 0. Overwritten by \"qp-const\""
          " (-1 = from NVENC preset)", -1, 51,
          DEFAULT_QP_DETAIL,
          G_PARAM_READWRITE | GST_PARAM_MUTABLE_PLAYING |
          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_QP_CONST_P,
      g_param_spec_int ("qp-const-p", "QP Const P",
          "Constant QP value for P frame, When >= 0, \"qp-const-i\" and "
          "\"qp-const-b\" should be also >= 0. Overwritten by \"qp-const\""
          " (-1 = from NVENC preset)", -1, 51,
          DEFAULT_QP_DETAIL,
          G_PARAM_READWRITE | GST_PARAM_MUTABLE_PLAYING |
          G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_QP_CONST_B,
      g_param_spec_int ("qp-const-b", "QP Const B",
          "Constant QP value for B frame, When >= 0, \"qp-const-i\" and "
          "\"qp-const-p\" should be also >= 0. Overwritten by \"qp-const\""
          " (-1 = from NVENC preset)", -1, 51,
          DEFAULT_QP_DETAIL,
          G_PARAM_READWRITE | GST_PARAM_MUTABLE_PLAYING |
          G_PARAM_STATIC_STRINGS));

  gst_type_mark_as_plugin_api (GST_TYPE_NV_BASE_ENC, 0);
  gst_type_mark_as_plugin_api (GST_TYPE_NV_PRESET, 0);
  gst_type_mark_as_plugin_api (GST_TYPE_NV_RC_MODE, 0);
}

static gboolean
gst_nv_base_enc_open_encode_session (GstNvBaseEnc * nvenc)
{
  NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS params = { 0, };
  NVENCSTATUS nv_ret;

  params.version = gst_nvenc_get_open_encode_session_ex_params_version ();
  params.apiVersion = gst_nvenc_get_api_version ();
  params.device = gst_cuda_context_get_handle (nvenc->cuda_ctx);
  params.deviceType = NV_ENC_DEVICE_TYPE_CUDA;
  nv_ret = NvEncOpenEncodeSessionEx (&params, &nvenc->encoder);

  if (nv_ret != NV_ENC_SUCCESS) {
    /* Report error to abort if GST_CUDA_CRITICAL_ERRORS is configured */
    gst_cuda_result (CUDA_ERROR_NO_DEVICE);
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_nv_base_enc_open (GstVideoEncoder * enc)
{
  GstNvBaseEnc *nvenc = GST_NV_BASE_ENC (enc);
  GstNvBaseEncClass *klass = GST_NV_BASE_ENC_GET_CLASS (enc);
  GValue *formats = NULL;

  if (!gst_cuda_ensure_element_context (GST_ELEMENT_CAST (enc),
          klass->cuda_device_id, &nvenc->cuda_ctx)) {
    GST_ERROR_OBJECT (nvenc, "failed to create CUDA context");
    return FALSE;
  }

  nvenc->stream = gst_cuda_stream_new (nvenc->cuda_ctx);
  if (!nvenc->stream) {
    GST_WARNING_OBJECT (nvenc,
        "Could not create cuda stream, will use default stream");
  }

  if (!gst_nv_base_enc_open_encode_session (nvenc)) {
    GST_ERROR ("Failed to create NVENC encoder session");
    gst_clear_object (&nvenc->cuda_ctx);
    return FALSE;
  }

  GST_INFO ("created NVENC encoder %p", nvenc->encoder);

  /* query supported input formats */
  if (!gst_nvenc_get_supported_input_formats (nvenc->encoder, klass->codec_id,
          &formats)) {
    GST_WARNING_OBJECT (nvenc, "No supported input formats");
    gst_nv_base_enc_close (enc);
    return FALSE;
  }

  nvenc->input_formats = formats;

  return TRUE;
}

static void
gst_nv_base_enc_set_context (GstElement * element, GstContext * context)
{
  GstNvBaseEnc *nvenc = GST_NV_BASE_ENC (element);
  GstNvBaseEncClass *klass = GST_NV_BASE_ENC_GET_CLASS (nvenc);

  if (gst_cuda_handle_set_context (element, context, klass->cuda_device_id,
          &nvenc->cuda_ctx)) {
    goto done;
  }
#ifdef HAVE_CUDA_GST_GL
  gst_gl_handle_set_context (element, context,
      (GstGLDisplay **) & nvenc->display,
      (GstGLContext **) & nvenc->other_context);
  if (nvenc->display)
    gst_gl_display_filter_gl_api (GST_GL_DISPLAY (nvenc->display),
        SUPPORTED_GL_APIS);
#endif

done:
  GST_ELEMENT_CLASS (parent_class)->set_context (element, context);
}

static gboolean
gst_nv_base_enc_sink_query (GstVideoEncoder * enc, GstQuery * query)
{
  GstNvBaseEnc *nvenc = GST_NV_BASE_ENC (enc);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CONTEXT:{
      if (gst_cuda_handle_context_query (GST_ELEMENT (nvenc),
              query, nvenc->cuda_ctx))
        return TRUE;

#ifdef HAVE_CUDA_GST_GL
      {
        gboolean ret;

        ret = gst_gl_handle_context_query ((GstElement *) nvenc, query,
            (GstGLDisplay *) nvenc->display, NULL,
            (GstGLContext *) nvenc->other_context);
        if (nvenc->display) {
          gst_gl_display_filter_gl_api (GST_GL_DISPLAY (nvenc->display),
              SUPPORTED_GL_APIS);
        }

        if (ret)
          return ret;
      }
#endif
      break;
    }
    default:
      break;
  }

  return GST_VIDEO_ENCODER_CLASS (parent_class)->sink_query (enc, query);
}

#ifdef HAVE_CUDA_GST_GL
static gboolean
gst_nv_base_enc_ensure_gl_context (GstNvBaseEnc * nvenc)
{
  if (!nvenc->display) {
    GST_DEBUG_OBJECT (nvenc, "No available OpenGL display");
    return FALSE;
  }

  if (!gst_gl_query_local_gl_context (GST_ELEMENT (nvenc), GST_PAD_SINK,
          (GstGLContext **) & nvenc->gl_context)) {
    GST_INFO_OBJECT (nvenc, "failed to query local OpenGL context");
    if (nvenc->gl_context)
      gst_object_unref (nvenc->gl_context);
    nvenc->gl_context =
        (GstObject *) gst_gl_display_get_gl_context_for_thread ((GstGLDisplay *)
        nvenc->display, NULL);
    if (!nvenc->gl_context
        || !gst_gl_display_add_context ((GstGLDisplay *) nvenc->display,
            (GstGLContext *) nvenc->gl_context)) {
      if (nvenc->gl_context)
        gst_object_unref (nvenc->gl_context);
      if (!gst_gl_display_create_context ((GstGLDisplay *) nvenc->display,
              (GstGLContext *) nvenc->other_context,
              (GstGLContext **) & nvenc->gl_context, NULL)) {
        GST_ERROR_OBJECT (nvenc, "failed to create OpenGL context");
        return FALSE;
      }
      if (!gst_gl_display_add_context ((GstGLDisplay *) nvenc->display,
              (GstGLContext *) nvenc->gl_context)) {
        GST_ERROR_OBJECT (nvenc,
            "failed to add the OpenGL context to the display");
        return FALSE;
      }
    }
  }

  if (!gst_gl_context_check_gl_version ((GstGLContext *) nvenc->gl_context,
          SUPPORTED_GL_APIS, 3, 0)) {
    GST_WARNING_OBJECT (nvenc, "OpenGL context could not support PBO download");
    return FALSE;
  }

  return TRUE;
}
#endif

static gboolean
gst_nv_base_enc_propose_allocation (GstVideoEncoder * enc, GstQuery * query)
{
  GstNvBaseEnc *nvenc = GST_NV_BASE_ENC (enc);
  GstCaps *caps;
  GstVideoInfo info;
  GstBufferPool *pool;
  GstStructure *config;
  GstCapsFeatures *features;
  guint size;

  GST_DEBUG_OBJECT (nvenc, "propose allocation");

  gst_query_parse_allocation (query, &caps, NULL);

  if (caps == NULL)
    return FALSE;

  if (!gst_video_info_from_caps (&info, caps)) {
    GST_WARNING_OBJECT (nvenc, "failed to get video info");
    return FALSE;
  }

  features = gst_caps_get_features (caps, 0);
#ifdef HAVE_CUDA_GST_GL
  if (features && gst_caps_features_contains (features,
          GST_CAPS_FEATURE_MEMORY_GL_MEMORY)) {
    GST_DEBUG_OBJECT (nvenc, "upsteram support GL memory");
    if (!gst_nv_base_enc_ensure_gl_context (nvenc)) {
      GST_WARNING_OBJECT (nvenc, "Could not get gl context");
      goto done;
    }

    pool = gst_gl_buffer_pool_new ((GstGLContext *) nvenc->gl_context);
  } else
#endif
  if (features && gst_caps_features_contains (features,
          GST_CAPS_FEATURE_MEMORY_CUDA_MEMORY)) {
    GST_DEBUG_OBJECT (nvenc, "upstream support CUDA memory");
    pool = gst_cuda_buffer_pool_new (nvenc->cuda_ctx);
  } else {
    GST_DEBUG_OBJECT (nvenc, "use system memory");
    goto done;
  }

  if (G_UNLIKELY (pool == NULL)) {
    GST_WARNING_OBJECT (nvenc, "cannot create buffer pool");
    goto done;
  }

  size = GST_VIDEO_INFO_SIZE (&info);

  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_set_params (config, caps, size, nvenc->items->len, 0);
  gst_buffer_pool_config_add_option (config, GST_BUFFER_POOL_OPTION_VIDEO_META);

  if (!gst_buffer_pool_set_config (pool, config))
    goto error_pool_config;

  /* Get updated size by cuda buffer pool */
  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_get_params (config, NULL, &size, NULL, NULL);
  gst_structure_free (config);

  gst_query_add_allocation_pool (query, pool, size, nvenc->items->len, 0);
  gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, NULL);

  gst_object_unref (pool);

done:
  return GST_VIDEO_ENCODER_CLASS (parent_class)->propose_allocation (enc,
      query);

error_pool_config:
  {
    if (pool)
      gst_object_unref (pool);
    GST_WARNING_OBJECT (nvenc, "failed to set config");
    return FALSE;
  }
}

static gboolean
gst_nv_base_enc_sink_event (GstVideoEncoder * enc, GstEvent * event)
{
  GstNvBaseEnc *nvenc = GST_NV_BASE_ENC (enc);
  gboolean ret;

  ret = GST_VIDEO_ENCODER_CLASS (parent_class)->sink_event (enc, event);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_STREAM_START:
    case GST_EVENT_FLUSH_STOP:
      nvenc->last_flow = GST_FLOW_OK;
      break;
    default:
      break;
  }

  return ret;
}

static gboolean
gst_nv_base_enc_start (GstVideoEncoder * enc)
{
  GstNvBaseEnc *nvenc = GST_NV_BASE_ENC (enc);

  nvenc->available_queue = g_async_queue_new ();
  nvenc->pending_queue = g_async_queue_new ();
  nvenc->bitstream_queue = g_async_queue_new ();
  nvenc->items = g_array_new (FALSE, TRUE, sizeof (GstNvEncFrameState));

  nvenc->last_flow = GST_FLOW_OK;
  memset (&nvenc->init_params, 0, sizeof (NV_ENC_INITIALIZE_PARAMS));
  memset (&nvenc->config, 0, sizeof (NV_ENC_CONFIG));

#ifdef HAVE_CUDA_GST_GL
  {
    gst_gl_ensure_element_data (GST_ELEMENT (nvenc),
        (GstGLDisplay **) & nvenc->display,
        (GstGLContext **) & nvenc->other_context);
    if (nvenc->display)
      gst_gl_display_filter_gl_api (GST_GL_DISPLAY (nvenc->display),
          SUPPORTED_GL_APIS);
  }
#endif

  /* DTS can be negative if bframe was enabled */
  gst_video_encoder_set_min_pts (enc, GST_SECOND * 60 * 60 * 1000);

  return TRUE;
}

static gboolean
gst_nv_base_enc_stop (GstVideoEncoder * enc)
{
  GstNvBaseEnc *nvenc = GST_NV_BASE_ENC (enc);

  gst_nv_base_enc_stop_bitstream_thread (nvenc, TRUE);

  gst_nv_base_enc_free_buffers (nvenc);

  if (nvenc->input_state) {
    gst_video_codec_state_unref (nvenc->input_state);
    nvenc->input_state = NULL;
  }

  if (nvenc->available_queue) {
    g_async_queue_unref (nvenc->available_queue);
    nvenc->available_queue = NULL;
  }
  if (nvenc->pending_queue) {
    g_async_queue_unref (nvenc->pending_queue);
    nvenc->pending_queue = NULL;
  }
  if (nvenc->bitstream_queue) {
    g_async_queue_unref (nvenc->bitstream_queue);
    nvenc->bitstream_queue = NULL;
  }
  if (nvenc->display) {
    gst_object_unref (nvenc->display);
    nvenc->display = NULL;
  }
  if (nvenc->other_context) {
    gst_object_unref (nvenc->other_context);
    nvenc->other_context = NULL;
  }
  if (nvenc->gl_context) {
    gst_object_unref (nvenc->gl_context);
    nvenc->gl_context = NULL;
  }

  if (nvenc->items) {
    g_array_free (nvenc->items, TRUE);
    nvenc->items = NULL;
  }

  return TRUE;
}

static void
check_formats (const gchar * str, guint * max_chroma, guint * max_bit_minus8)
{
  if (!str)
    return;

  if (g_strrstr (str, "-444") || g_strrstr (str, "-4:4:4"))
    *max_chroma = 2;
  else if ((g_strrstr (str, "-4:2:2") || g_strrstr (str, "-422"))
      && *max_chroma < 1)
    *max_chroma = 1;

  if (g_strrstr (str, "-12"))
    *max_bit_minus8 = 4;
  else if (g_strrstr (str, "-10") && *max_bit_minus8 < 2)
    *max_bit_minus8 = 2;
}

static gboolean
gst_nv_base_enc_set_filtered_input_formats (GstNvBaseEnc * nvenc,
    GstCaps * caps, const GValue * input_formats, guint max_chroma,
    guint max_bit_minus8)
{
  gint i;
  GValue supported_format = G_VALUE_INIT;
  gint num_format = 0;
  const GValue *last_format = NULL;

  g_value_init (&supported_format, GST_TYPE_LIST);

  for (i = 0; i < gst_value_list_get_size (input_formats); i++) {
    const GValue *val;
    GstVideoFormat format;

    val = gst_value_list_get_value (input_formats, i);
    format = gst_video_format_from_string (g_value_get_string (val));

    switch (format) {
      case GST_VIDEO_FORMAT_NV12:
      case GST_VIDEO_FORMAT_YV12:
      case GST_VIDEO_FORMAT_I420:
        /* 8bits 4:2:0 formats are always supported */
      case GST_VIDEO_FORMAT_BGRA:
      case GST_VIDEO_FORMAT_RGBA:
        /* NOTE: RGB formats seems to also supported format, which are
         * encoded to 4:2:0 formats */
        gst_value_list_append_value (&supported_format, val);
        last_format = val;
        num_format++;
        break;
      case GST_VIDEO_FORMAT_Y444:
      case GST_VIDEO_FORMAT_VUYA:
        if (max_chroma >= 2) {
          gst_value_list_append_value (&supported_format, val);
          last_format = val;
          num_format++;
        }
        break;
      case GST_VIDEO_FORMAT_P010_10LE:
      case GST_VIDEO_FORMAT_P010_10BE:
      case GST_VIDEO_FORMAT_BGR10A2_LE:
      case GST_VIDEO_FORMAT_RGB10A2_LE:
      case GST_VIDEO_FORMAT_Y444_16LE:
      case GST_VIDEO_FORMAT_Y444_16BE:
        if (max_bit_minus8 >= 2) {
          gst_value_list_append_value (&supported_format, val);
          last_format = val;
          num_format++;
        }
        break;
      default:
        break;
    }
  }

  if (num_format == 0) {
    g_value_unset (&supported_format);
    GST_WARNING_OBJECT (nvenc, "Cannot find matching input format");
    return FALSE;
  }

  if (num_format > 1)
    gst_caps_set_value (caps, "format", &supported_format);
  else
    gst_caps_set_value (caps, "format", last_format);

  g_value_unset (&supported_format);

  return TRUE;
}

static GstCaps *
gst_nv_base_enc_getcaps (GstVideoEncoder * enc, GstCaps * filter)
{
  GstNvBaseEnc *nvenc = GST_NV_BASE_ENC (enc);
  GstNvBaseEncClass *klass = GST_NV_BASE_ENC_GET_CLASS (enc);
  GstCaps *supported_incaps = NULL;
  GstCaps *template_caps, *caps, *allowed;

  template_caps = gst_pad_get_pad_template_caps (enc->sinkpad);
  allowed = gst_pad_get_allowed_caps (enc->srcpad);

  GST_LOG_OBJECT (enc, "template caps %" GST_PTR_FORMAT, template_caps);
  GST_LOG_OBJECT (enc, "allowed caps %" GST_PTR_FORMAT, allowed);

  if (!allowed) {
    /* no peer */
    supported_incaps = template_caps;
    template_caps = NULL;
    goto done;
  } else if (gst_caps_is_empty (allowed)) {
    /* couldn't be negotiated, just return empty caps */
    gst_caps_unref (template_caps);
    return allowed;
  }

  GST_OBJECT_LOCK (nvenc);

  if (nvenc->input_formats != NULL) {
    gboolean has_profile = FALSE;
    guint max_chroma_index = 0;
    guint max_bit_minus8 = 0;
    gint i, j;

    for (i = 0; i < gst_caps_get_size (allowed); i++) {
      const GstStructure *allowed_s = gst_caps_get_structure (allowed, i);
      const GValue *val;

      if ((val = gst_structure_get_value (allowed_s, "profile"))) {
        if (G_VALUE_HOLDS_STRING (val)) {
          check_formats (g_value_get_string (val), &max_chroma_index,
              &max_bit_minus8);
          has_profile = TRUE;
        } else if (GST_VALUE_HOLDS_LIST (val)) {
          for (j = 0; j < gst_value_list_get_size (val); j++) {
            const GValue *vlist = gst_value_list_get_value (val, j);

            if (G_VALUE_HOLDS_STRING (vlist)) {
              check_formats (g_value_get_string (vlist), &max_chroma_index,
                  &max_bit_minus8);
              has_profile = TRUE;
            }
          }
        }
      }
    }

    GST_LOG_OBJECT (enc,
        "downstream requested profile %d, max bitdepth %d, max chroma %d",
        has_profile, max_bit_minus8 + 8, max_chroma_index);

    supported_incaps = gst_caps_copy (template_caps);
    if (!has_profile ||
        !gst_nv_base_enc_set_filtered_input_formats (nvenc, supported_incaps,
            nvenc->input_formats, max_chroma_index, max_bit_minus8)) {
      gst_caps_set_value (supported_incaps, "format", nvenc->input_formats);
    }

    if (nvenc->encoder) {
      GValue *interlace_mode;

      interlace_mode =
          gst_nvenc_get_interlace_modes (nvenc->encoder, klass->codec_id);
      gst_caps_set_value (supported_incaps, "interlace-mode", interlace_mode);
      g_value_unset (interlace_mode);
      g_free (interlace_mode);
    }

    GST_LOG_OBJECT (enc, "codec input caps %" GST_PTR_FORMAT, supported_incaps);
    GST_LOG_OBJECT (enc, "   template caps %" GST_PTR_FORMAT, template_caps);
    caps = gst_caps_intersect (template_caps, supported_incaps);
    gst_caps_unref (supported_incaps);
    supported_incaps = caps;
    GST_LOG_OBJECT (enc, "  supported caps %" GST_PTR_FORMAT, supported_incaps);
  }

  GST_OBJECT_UNLOCK (nvenc);

done:
  caps = gst_video_encoder_proxy_getcaps (enc, supported_incaps, filter);

  if (supported_incaps)
    gst_caps_unref (supported_incaps);
  gst_clear_caps (&allowed);
  gst_clear_caps (&template_caps);

  GST_DEBUG_OBJECT (nvenc, "  returning caps %" GST_PTR_FORMAT, caps);

  return caps;
}

static gboolean
gst_nv_base_enc_close (GstVideoEncoder * enc)
{
  GstNvBaseEnc *nvenc = GST_NV_BASE_ENC (enc);
  gboolean ret = TRUE;

  if (nvenc->encoder) {
    if (NvEncDestroyEncoder (nvenc->encoder) != NV_ENC_SUCCESS)
      ret = FALSE;

    nvenc->encoder = NULL;
  }

  gst_clear_cuda_stream (&nvenc->stream);
  gst_clear_object (&nvenc->cuda_ctx);

  GST_OBJECT_LOCK (nvenc);
  if (nvenc->input_formats)
    g_value_unset (nvenc->input_formats);
  g_free (nvenc->input_formats);
  nvenc->input_formats = NULL;
  GST_OBJECT_UNLOCK (nvenc);

  if (nvenc->input_state) {
    gst_video_codec_state_unref (nvenc->input_state);
    nvenc->input_state = NULL;
  }

  return ret;
}

static void
gst_nv_base_enc_init (GstNvBaseEnc * nvenc)
{
  GstVideoEncoder *encoder = GST_VIDEO_ENCODER (nvenc);
  GstNvEncQP qp_detail =
      { DEFAULT_QP_DETAIL, DEFAULT_QP_DETAIL, DEFAULT_QP_DETAIL };

  nvenc->preset_enum = DEFAULT_PRESET;
  nvenc->selected_preset = _nv_preset_to_guid (nvenc->preset_enum);
  nvenc->rate_control_mode = DEFAULT_RC_MODE;
  nvenc->qp_min = DEFAULT_QP_MIN;
  nvenc->qp_max = DEFAULT_QP_MAX;
  nvenc->qp_const = DEFAULT_QP_CONST;
  nvenc->bitrate = DEFAULT_BITRATE;
  nvenc->gop_size = DEFAULT_GOP_SIZE;
  nvenc->max_bitrate = DEFAULT_MAX_BITRATE;
  nvenc->spatial_aq = DEFAULT_SPATIAL_AQ;
  nvenc->aq_strength = DEFAULT_AQ_STRENGTH;
  nvenc->non_refp = DEFAULT_NON_REF_P;
  nvenc->zerolatency = DEFAULT_ZEROLATENCY;
  nvenc->strict_gop = DEFAULT_STRICT_GOP;
  nvenc->const_quality = DEFAULT_CONST_QUALITY;
  nvenc->i_adapt = DEFAULT_I_ADAPT;
  nvenc->qp_min_detail = qp_detail;
  nvenc->qp_max_detail = qp_detail;
  nvenc->qp_const_detail = qp_detail;

  GST_VIDEO_ENCODER_STREAM_LOCK (encoder);
  GST_VIDEO_ENCODER_STREAM_UNLOCK (encoder);

  GST_PAD_SET_ACCEPT_INTERSECT (GST_VIDEO_ENCODER_SINK_PAD (encoder));
}

static void
gst_nv_base_enc_finalize (GObject * obj)
{
  G_OBJECT_CLASS (gst_nv_base_enc_parent_class)->finalize (obj);
}

static GstVideoCodecFrame *
_find_frame_with_output_buffer (GstNvBaseEnc * nvenc, NV_ENC_OUTPUT_PTR out_buf)
{
  GList *l, *walk = gst_video_encoder_get_frames (GST_VIDEO_ENCODER (nvenc));
  GstVideoCodecFrame *ret = NULL;

  for (l = walk; l; l = l->next) {
    GstVideoCodecFrame *frame = (GstVideoCodecFrame *) l->data;
    GstNvEncFrameState *state = gst_video_codec_frame_get_user_data (frame);

    if (!state || !state->out_buf)
      continue;

    if (state->out_buf == out_buf) {
      ret = frame;
      break;
    }
  }

  if (ret)
    gst_video_codec_frame_ref (ret);

  g_list_free_full (walk, (GDestroyNotify) gst_video_codec_frame_unref);

  return ret;
}

static const gchar *
picture_type_to_string (NV_ENC_PIC_TYPE type)
{
  switch (type) {
    case NV_ENC_PIC_TYPE_P:
      return "P";
    case NV_ENC_PIC_TYPE_B:
      return "B";
    case NV_ENC_PIC_TYPE_I:
      return "I";
    case NV_ENC_PIC_TYPE_IDR:
      return "IDR";
    case NV_ENC_PIC_TYPE_BI:
      return "BI";
    case NV_ENC_PIC_TYPE_SKIPPED:
      return "SKIPPED";
    case NV_ENC_PIC_TYPE_INTRA_REFRESH:
      return "INTRA-REFRESH";
    case NV_ENC_PIC_TYPE_UNKNOWN:
    default:
      break;
  }

  return "UNKNOWN";
}

static gpointer
gst_nv_base_enc_bitstream_thread (gpointer user_data)
{
  GstVideoEncoder *enc = user_data;
  GstNvBaseEnc *nvenc = user_data;
  GstFlowReturn flow = GST_FLOW_OK;

  /* overview of operation:
   * 1. retrieve the next buffer submitted to the bitstream pool
   * 2. wait for that buffer to be ready from nvenc (LockBitsream)
   * 3. retrieve the GstVideoCodecFrame associated with that buffer
   * 4. for each buffer in the frame
   * 4.1 (step 2): wait for that buffer to be ready from nvenc (LockBitsream)
   * 4.2 create an output GstBuffer from the nvenc buffers
   * 4.3 unlock the nvenc bitstream buffers UnlockBitsream
   * 5. finish_frame()
   * 6. cleanup
   */
  do {
    GstBuffer *buffer = NULL;
    GstNvEncFrameState *state_in_queue = NULL;
    GstNvEncFrameState *state = NULL;
    GstVideoCodecFrame *frame = NULL;
    NVENCSTATUS nv_ret;
    NV_ENC_LOCK_BITSTREAM lock_bs = { 0, };
    NV_ENC_OUTPUT_PTR out_buf;
    GstNvEncInputResource *resource;

    GST_LOG_OBJECT (enc, "wait for bitstream buffer..");

    state_in_queue = g_async_queue_pop (nvenc->bitstream_queue);
    if ((gpointer) state_in_queue == SHUTDOWN_COOKIE)
      goto exit_thread;

    out_buf = state_in_queue->out_buf;
    resource = state_in_queue->in_buf;

    GST_LOG_OBJECT (nvenc, "waiting for output buffer %p to be ready", out_buf);

    lock_bs.version = gst_nvenc_get_lock_bitstream_version ();
    lock_bs.outputBitstream = out_buf;
    lock_bs.doNotWait = 0;

    /* FIXME: this would need to be updated for other slice modes */
    lock_bs.sliceOffsets = NULL;

    if (!gst_cuda_context_push (nvenc->cuda_ctx)) {
      GST_ELEMENT_ERROR (nvenc, LIBRARY, ENCODE, (NULL),
          ("Failed to push current context"));
      goto error_shutdown;
    }

    nv_ret = NvEncLockBitstream (nvenc->encoder, &lock_bs);
    if (nv_ret != NV_ENC_SUCCESS) {
      gst_cuda_context_pop (NULL);

      GST_ELEMENT_ERROR (nvenc, STREAM, ENCODE, (NULL),
          ("Failed to lock bitstream buffer %p, ret %d",
              lock_bs.outputBitstream, nv_ret));
      goto error_shutdown;
    }

    frame = _find_frame_with_output_buffer (nvenc, out_buf);
    state = gst_video_codec_frame_get_user_data (frame);
    g_assert (state->out_buf == out_buf);

    /* copy into output buffer */
    buffer = gst_buffer_new_allocate (NULL, lock_bs.bitstreamSizeInBytes, NULL);
    gst_buffer_fill (buffer, 0, lock_bs.bitstreamBufferPtr,
        lock_bs.bitstreamSizeInBytes);

    if (lock_bs.pictureType == NV_ENC_PIC_TYPE_IDR) {
      GST_DEBUG_OBJECT (nvenc, "This is a keyframe");
      GST_VIDEO_CODEC_FRAME_SET_SYNC_POINT (frame);
    }

    nv_ret = NvEncUnlockBitstream (nvenc->encoder, state->out_buf);

    if (nv_ret != NV_ENC_SUCCESS) {
      gst_cuda_context_pop (NULL);

      GST_ELEMENT_ERROR (nvenc, STREAM, ENCODE, (NULL),
          ("Failed to unlock bitstream buffer %p, ret %d",
              lock_bs.outputBitstream, nv_ret));
      gst_buffer_unref (buffer);
      gst_video_encoder_finish_frame (enc, frame);

      goto error_shutdown;
    }

    frame->dts = frame->pts;
    frame->pts = lock_bs.outputTimeStamp;
    frame->duration = lock_bs.outputDuration;

    GST_LOG_OBJECT (nvenc, "frame index %" G_GUINT32_FORMAT
        ", frame type %s, dts %" GST_TIME_FORMAT
        ", pts %" GST_TIME_FORMAT,
        lock_bs.frameIdx, picture_type_to_string (lock_bs.pictureType),
        GST_TIME_ARGS (frame->dts), GST_TIME_ARGS (frame->pts));

    frame->output_buffer = buffer;

    nv_ret =
        NvEncUnmapInputResource (nvenc->encoder,
        resource->nv_mapped_resource.mappedResource);
    resource->mapped = FALSE;

    if (nv_ret != NV_ENC_SUCCESS) {
      GST_ERROR_OBJECT (nvenc, "Failed to unmap input resource %p, ret %d",
          resource, nv_ret);
    }

    gst_cuda_context_pop (NULL);

    memset (&resource->nv_mapped_resource, 0,
        sizeof (resource->nv_mapped_resource));

    g_async_queue_push (nvenc->available_queue, state_in_queue);

    /* Ugly but no other way to get DTS offset since nvenc dose not adjust
     * dts/pts even if bframe was enabled. So the output PTS can be smaller
     * than DTS. The maximum difference between DTS and PTS can be calculated
     * using the PTS difference between the first frame and the second frame.
     */
    if (nvenc->bframes > 0) {
      if (nvenc->dts_offset == 0) {
        if (!nvenc->first_frame) {
          /* store the first frame to get dts offset */
          nvenc->first_frame = frame;
          continue;
        } else {
          if (nvenc->first_frame->pts >= frame->pts) {
            GstClockTime duration = 0;

            GST_WARNING_OBJECT (enc, "Could not calculate DTS offset");

            if (nvenc->input_info.fps_n > 0 && nvenc->input_info.fps_d > 0) {
              duration =
                  gst_util_uint64_scale (GST_SECOND, nvenc->input_info.fps_d,
                  nvenc->input_info.fps_n);
            } else if (nvenc->first_frame->duration > 0 &&
                GST_CLOCK_TIME_IS_VALID (nvenc->first_frame->duration)) {
              duration = nvenc->first_frame->duration;
            } else {
              GST_WARNING_OBJECT (enc,
                  "No way to get frame duration, assuming 30fps");
              duration = gst_util_uint64_scale (GST_SECOND, 1, 30);
            }

            nvenc->dts_offset = duration * nvenc->bframes;
          } else {
            nvenc->dts_offset = frame->pts - nvenc->first_frame->pts;
          }

          /* + 1 to dts_offset to adjust fraction */
          nvenc->dts_offset++;

          GST_DEBUG_OBJECT (enc,
              "Calculated DTS offset %" GST_TIME_FORMAT,
              GST_TIME_ARGS (nvenc->dts_offset));
        }

        nvenc->first_frame->dts -= nvenc->dts_offset;
        gst_video_encoder_finish_frame (enc, nvenc->first_frame);
        nvenc->first_frame = NULL;
      }

      frame->dts -= nvenc->dts_offset;
    }

    flow = gst_video_encoder_finish_frame (enc, frame);

    if (flow != GST_FLOW_OK) {
      GST_INFO_OBJECT (enc, "got flow %s", gst_flow_get_name (flow));
      g_atomic_int_set (&nvenc->last_flow, flow);
      g_async_queue_push (nvenc->available_queue, SHUTDOWN_COOKIE);
      goto exit_thread;
    }
  }
  while (TRUE);

error_shutdown:
  {
    if (nvenc->first_frame) {
      gst_clear_buffer (&nvenc->first_frame->output_buffer);
      gst_video_encoder_finish_frame (enc, nvenc->first_frame);
      nvenc->first_frame = NULL;
    }
    g_atomic_int_set (&nvenc->last_flow, GST_FLOW_ERROR);
    g_async_queue_push (nvenc->available_queue, SHUTDOWN_COOKIE);

    goto exit_thread;
  }

exit_thread:
  {
    if (nvenc->first_frame) {
      gst_video_encoder_finish_frame (enc, nvenc->first_frame);
      nvenc->first_frame = NULL;
    }

    GST_INFO_OBJECT (nvenc, "exiting thread");

    return NULL;
  }
}

static gboolean
gst_nv_base_enc_start_bitstream_thread (GstNvBaseEnc * nvenc)
{
  gchar *name = g_strdup_printf ("%s-read-bits", GST_OBJECT_NAME (nvenc));

  g_assert (nvenc->bitstream_thread == NULL);

  g_assert (g_async_queue_length (nvenc->bitstream_queue) == 0);

  nvenc->bitstream_thread =
      g_thread_try_new (name, gst_nv_base_enc_bitstream_thread, nvenc, NULL);

  g_free (name);

  if (nvenc->bitstream_thread == NULL)
    return FALSE;

  GST_INFO_OBJECT (nvenc, "started thread to read bitstream");
  return TRUE;
}

static gboolean
gst_nv_base_enc_stop_bitstream_thread (GstNvBaseEnc * nvenc, gboolean force)
{
  GstNvEncFrameState *state;

  if (nvenc->bitstream_thread == NULL)
    return TRUE;

  /* Always send EOS packet to flush GPU. Otherwise, randomly crash happens
   * during NvEncDestroyEncoder especially when rc-lookahead or bframe was
   * enabled */
  gst_nv_base_enc_drain_encoder (nvenc);

  if (force) {
    g_async_queue_lock (nvenc->available_queue);
    g_async_queue_lock (nvenc->pending_queue);
    g_async_queue_lock (nvenc->bitstream_queue);
    while ((state = g_async_queue_try_pop_unlocked (nvenc->bitstream_queue))) {
      GST_INFO_OBJECT (nvenc, "stole bitstream buffer %p from queue", state);
      g_async_queue_push_unlocked (nvenc->available_queue, state);
    }
    g_async_queue_push_unlocked (nvenc->bitstream_queue, SHUTDOWN_COOKIE);
    g_async_queue_unlock (nvenc->available_queue);
    g_async_queue_unlock (nvenc->pending_queue);
    g_async_queue_unlock (nvenc->bitstream_queue);
  } else {
    /* wait for encoder to drain the remaining buffers */
    g_async_queue_push (nvenc->bitstream_queue, SHUTDOWN_COOKIE);
  }

  if (!force) {
    /* temporary unlock during finish, so other thread can find and push frame */
    GST_VIDEO_ENCODER_STREAM_UNLOCK (nvenc);
  }

  g_thread_join (nvenc->bitstream_thread);

  if (!force)
    GST_VIDEO_ENCODER_STREAM_LOCK (nvenc);

  nvenc->bitstream_thread = NULL;
  return TRUE;
}

static void
gst_nv_base_enc_reset_queues (GstNvBaseEnc * nvenc)
{
  gpointer ptr;

  GST_INFO_OBJECT (nvenc, "clearing queues");

  while ((ptr = g_async_queue_try_pop (nvenc->available_queue))) {
    /* do nothing */
  }
  while ((ptr = g_async_queue_try_pop (nvenc->pending_queue))) {
    /* do nothing */
  }
  while ((ptr = g_async_queue_try_pop (nvenc->bitstream_queue))) {
    /* do nothing */
  }
}

static void
gst_nv_base_enc_free_buffers (GstNvBaseEnc * nvenc)
{
  NVENCSTATUS nv_ret;
  CUresult cuda_ret;
  guint i;

  if (nvenc->encoder == NULL)
    return;

  gst_nv_base_enc_reset_queues (nvenc);

  if (!nvenc->items || !nvenc->items->len)
    return;

  gst_cuda_context_push (nvenc->cuda_ctx);
  for (i = 0; i < nvenc->items->len; ++i) {
    NV_ENC_OUTPUT_PTR out_buf =
        g_array_index (nvenc->items, GstNvEncFrameState, i).out_buf;
    GstNvEncInputResource *in_buf =
        g_array_index (nvenc->items, GstNvEncFrameState, i).in_buf;

    if (in_buf->mapped) {
      GST_LOG_OBJECT (nvenc, "Unmap resource %p", in_buf);

      nv_ret =
          NvEncUnmapInputResource (nvenc->encoder,
          in_buf->nv_mapped_resource.mappedResource);

      if (nv_ret != NV_ENC_SUCCESS) {
        GST_ERROR_OBJECT (nvenc, "Failed to unmap input resource %p, ret %d",
            in_buf, nv_ret);
      }
    }

    nv_ret =
        NvEncUnregisterResource (nvenc->encoder,
        in_buf->nv_resource.registeredResource);
    if (nv_ret != NV_ENC_SUCCESS)
      GST_ERROR_OBJECT (nvenc, "Failed to unregister resource %p, ret %d",
          in_buf, nv_ret);

    cuda_ret = CuMemFree (in_buf->cuda_pointer);
    if (!gst_cuda_result (cuda_ret)) {
      GST_ERROR_OBJECT (nvenc, "Failed to free CUDA device memory, ret %d",
          cuda_ret);
    }

    g_free (in_buf);

    GST_DEBUG_OBJECT (nvenc, "Destroying output bitstream buffer %p", out_buf);
    nv_ret = NvEncDestroyBitstreamBuffer (nvenc->encoder, out_buf);
    if (nv_ret != NV_ENC_SUCCESS) {
      GST_ERROR_OBJECT (nvenc, "Failed to destroy output buffer %p, ret %d",
          out_buf, nv_ret);
    }
  }
  gst_cuda_context_pop (NULL);
  g_array_set_size (nvenc->items, 0);
}

static inline guint
_get_plane_width (GstVideoInfo * info, guint plane)
{
  return GST_VIDEO_INFO_COMP_WIDTH (info, plane)
      * GST_VIDEO_INFO_COMP_PSTRIDE (info, plane);
}

static inline guint
_get_plane_height (GstVideoInfo * info, guint plane)
{
  if (GST_VIDEO_INFO_IS_YUV (info))
    /* For now component width and plane width are the same and the
     * plane-component mapping matches
     */
    return GST_VIDEO_INFO_COMP_HEIGHT (info, plane);
  else                          /* RGB, GRAY */
    return GST_VIDEO_INFO_HEIGHT (info);
}

static inline gsize
_get_frame_data_height (GstVideoInfo * info)
{
  gsize ret = 0;
  gint i;

  for (i = 0; i < GST_VIDEO_INFO_N_PLANES (info); i++) {
    ret += _get_plane_height (info, i);
  }

  return ret;
}

static gboolean
qp_has_values (const GstNvEncQP * qp)
{
  return qp->qp_i >= 0 && qp->qp_p >= 0 && qp->qp_b >= 0;
}

static void
gst_nv_base_enc_setup_rate_control (GstNvBaseEnc * nvenc,
    NV_ENC_RC_PARAMS * rc_params)
{
  GstNvRCMode rc_mode = nvenc->rate_control_mode;
  NV_ENC_PARAMS_RC_MODE nv_rcmode;

  if (nvenc->bitrate)
    rc_params->averageBitRate = nvenc->bitrate * 1024;

  if (nvenc->max_bitrate)
    rc_params->maxBitRate = nvenc->max_bitrate * 1024;

  if (nvenc->vbv_buffersize)
    rc_params->vbvBufferSize = nvenc->vbv_buffersize * 1024;

  /* Guess the best matching mode */
  if (rc_mode == GST_NV_RC_MODE_DEFAULT) {
    if (nvenc->qp_const >= 0) {
      /* constQP is used only for RC_CONSTQP mode */
      rc_mode = GST_NV_RC_MODE_CONSTQP;
    }
  }

  if (nvenc->qp_min >= 0) {
    rc_params->enableMinQP = 1;
    rc_params->minQP.qpInterB = nvenc->qp_min;
    rc_params->minQP.qpInterP = nvenc->qp_min;
    rc_params->minQP.qpIntra = nvenc->qp_min;
  } else if (qp_has_values (&nvenc->qp_min_detail)) {
    rc_params->enableMinQP = 1;
    rc_params->minQP.qpInterB = nvenc->qp_min_detail.qp_b;
    rc_params->minQP.qpInterP = nvenc->qp_min_detail.qp_p;
    rc_params->minQP.qpIntra = nvenc->qp_min_detail.qp_i;
  }

  if (nvenc->qp_max >= 0) {
    rc_params->enableMaxQP = 1;
    rc_params->maxQP.qpInterB = nvenc->qp_max;
    rc_params->maxQP.qpInterP = nvenc->qp_max;
    rc_params->maxQP.qpIntra = nvenc->qp_max;
  } else if (qp_has_values (&nvenc->qp_max_detail)) {
    rc_params->enableMaxQP = 1;
    rc_params->maxQP.qpInterB = nvenc->qp_max_detail.qp_b;
    rc_params->maxQP.qpInterP = nvenc->qp_max_detail.qp_p;
    rc_params->maxQP.qpIntra = nvenc->qp_max_detail.qp_i;
  }

  if (nvenc->qp_const >= 0) {
    rc_params->constQP.qpInterB = nvenc->qp_const;
    rc_params->constQP.qpInterP = nvenc->qp_const;
    rc_params->constQP.qpIntra = nvenc->qp_const;
  } else if (qp_has_values (&nvenc->qp_const_detail)) {
    rc_params->constQP.qpInterB = nvenc->qp_const_detail.qp_b;
    rc_params->constQP.qpInterP = nvenc->qp_const_detail.qp_p;
    rc_params->constQP.qpIntra = nvenc->qp_const_detail.qp_i;
  }

  nv_rcmode = _rc_mode_to_nv (rc_mode);
  if (nv_rcmode == NV_ENC_PARAMS_RC_VBR_MINQP && nvenc->qp_min < 0) {
    GST_WARNING_OBJECT (nvenc, "vbr-minqp was requested without qp-min");
    nv_rcmode = NV_ENC_PARAMS_RC_VBR;
  }

  rc_params->rateControlMode = nv_rcmode;

  if (nvenc->spatial_aq) {
    rc_params->enableAQ = 1;
    rc_params->aqStrength = nvenc->aq_strength;
  }

  rc_params->enableTemporalAQ = nvenc->temporal_aq;

  if (nvenc->rc_lookahead) {
    rc_params->enableLookahead = 1;
    rc_params->lookaheadDepth = nvenc->rc_lookahead;
    rc_params->disableIadapt = !nvenc->i_adapt;
    rc_params->disableBadapt = !nvenc->b_adapt;
  }

  rc_params->strictGOPTarget = nvenc->strict_gop;
  rc_params->enableNonRefP = nvenc->non_refp;
  rc_params->zeroReorderDelay = nvenc->zerolatency;

  if (nvenc->const_quality) {
    guint scaled = (gint) (nvenc->const_quality * 256.0);

    rc_params->targetQuality = (guint8) (scaled >> 8);
    rc_params->targetQualityLSB = (guint8) (scaled & 0xff);
  }
}

static guint
gst_nv_base_enc_calculate_num_prealloc_buffers (GstNvBaseEnc * enc,
    NV_ENC_CONFIG * config)
{
  guint num_buffers;

  /* At least 4 surfaces are required as documented by Nvidia Encoder guide */
  num_buffers = 4;

  /* + lookahead depth */
  num_buffers += config->rcParams.lookaheadDepth;

  /* + GOP size */
  num_buffers += config->frameIntervalP;

  /* hardcoded upper bound "48"
   * The worst case
   *   default num buffers: 4
   *   maximum allowed lookahead: 32
   *   max bfraems: 4 -> frameIntervalP: 5
   * "4 + 32 + 5" < "48" so it seems to sufficiently safe upper bound */
  num_buffers = MIN (num_buffers, 48);

  GST_DEBUG_OBJECT (enc, "Calculated num buffers: %d "
      "(lookahead %d, frameIntervalP %d)",
      num_buffers, config->rcParams.lookaheadDepth, config->frameIntervalP);

  return num_buffers;
}

/* GstVideoEncoder::set_format or by nvenc self if new properties were set.
 *
 * NvEncReconfigureEncoder with following conditions are not allowed
 * 1) GOP structure change
 * 2) sync-Async mode change (Async mode is Windows only and we didn't support it)
 * 3) MaxWidth, MaxHeight
 * 4) PTDmode (Picture Type Decision mode)
 *
 * So we will force to re-init the encode session if
 * 1) New resolution is larger than previous config
 * 2) GOP size changed
 * 3) Input pixel format change
 *    pre-allocated CUDA memory could not ensure stride, width and height
 *
 * TODO: bframe also considered as force re-init case
 */
static gboolean
gst_nv_base_enc_set_format (GstVideoEncoder * enc, GstVideoCodecState * state)
{
  GstNvBaseEncClass *nvenc_class = GST_NV_BASE_ENC_GET_CLASS (enc);
  GstNvBaseEnc *nvenc = GST_NV_BASE_ENC (enc);
  GstVideoInfo *info = &state->info;
  GstVideoCodecState *old_state = nvenc->input_state;
  NV_ENC_RECONFIGURE_PARAMS reconfigure_params = { 0, };
  NV_ENC_INITIALIZE_PARAMS *params = &nvenc->init_params;
  NV_ENC_PRESET_CONFIG preset_config = { 0, };
  NVENCSTATUS nv_ret;
  gint dar_n, dar_d;
  gboolean reconfigure = FALSE;

  g_atomic_int_set (&nvenc->reconfig, FALSE);

  if (!nvenc->encoder && !gst_nv_base_enc_open_encode_session (nvenc)) {
    GST_ELEMENT_ERROR (nvenc, LIBRARY, INIT, (NULL),
        ("Failed to open encode session"));
    return FALSE;
  }

  if (old_state) {
    gboolean larger_resolution;
    gboolean format_changed;
    gboolean gop_size_changed;

    larger_resolution =
        (GST_VIDEO_INFO_WIDTH (info) > nvenc->init_params.maxEncodeWidth ||
        GST_VIDEO_INFO_HEIGHT (info) > nvenc->init_params.maxEncodeHeight);
    format_changed =
        GST_VIDEO_INFO_FORMAT (info) !=
        GST_VIDEO_INFO_FORMAT (&old_state->info);

    if (nvenc->config.gopLength == NVENC_INFINITE_GOPLENGTH
        && nvenc->gop_size == -1) {
      gop_size_changed = FALSE;
    } else if (nvenc->config.gopLength != nvenc->gop_size) {
      gop_size_changed = TRUE;
    } else {
      gop_size_changed = FALSE;
    }

    if (larger_resolution || format_changed || gop_size_changed) {
      GST_DEBUG_OBJECT (nvenc,
          "resolution %dx%d -> %dx%d, format %s -> %s, re-init",
          nvenc->init_params.maxEncodeWidth, nvenc->init_params.maxEncodeHeight,
          GST_VIDEO_INFO_WIDTH (info), GST_VIDEO_INFO_HEIGHT (info),
          gst_video_format_to_string (GST_VIDEO_INFO_FORMAT (&old_state->info)),
          gst_video_format_to_string (GST_VIDEO_INFO_FORMAT (info)));

      gst_nv_base_enc_drain_encoder (nvenc);
      gst_nv_base_enc_stop_bitstream_thread (nvenc, FALSE);
      gst_nv_base_enc_free_buffers (nvenc);
      NvEncDestroyEncoder (nvenc->encoder);
      nvenc->encoder = NULL;

      if (!gst_nv_base_enc_open_encode_session (nvenc)) {
        GST_ERROR_OBJECT (nvenc, "Failed to open encode session");
        return FALSE;
      }
    } else {
      reconfigure_params.version = gst_nvenc_get_reconfigure_params_version ();
      /* reset rate control state and start from IDR */
      reconfigure_params.resetEncoder = TRUE;
      reconfigure_params.forceIDR = TRUE;
      reconfigure = TRUE;
    }
  }

  params->version = gst_nvenc_get_initialize_params_version ();
  params->encodeGUID = nvenc_class->codec_id;
  params->encodeWidth = GST_VIDEO_INFO_WIDTH (info);
  params->encodeHeight = GST_VIDEO_INFO_HEIGHT (info);

  {
    guint32 n_presets;
    GUID *presets;
    guint32 i;

    nv_ret =
        NvEncGetEncodePresetCount (nvenc->encoder,
        params->encodeGUID, &n_presets);
    if (nv_ret != NV_ENC_SUCCESS) {
      GST_ELEMENT_ERROR (nvenc, LIBRARY, SETTINGS, (NULL),
          ("Failed to get encoder presets"));
      return FALSE;
    }

    presets = g_new0 (GUID, n_presets);
    nv_ret =
        NvEncGetEncodePresetGUIDs (nvenc->encoder,
        params->encodeGUID, presets, n_presets, &n_presets);
    if (nv_ret != NV_ENC_SUCCESS) {
      GST_ELEMENT_ERROR (nvenc, LIBRARY, SETTINGS, (NULL),
          ("Failed to get encoder presets"));
      g_free (presets);
      return FALSE;
    }

    for (i = 0; i < n_presets; i++) {
      if (gst_nvenc_cmp_guid (presets[i], nvenc->selected_preset))
        break;
    }
    g_free (presets);
    if (i >= n_presets) {
      GST_ELEMENT_ERROR (nvenc, LIBRARY, SETTINGS, (NULL),
          ("Selected preset not supported"));
      return FALSE;
    }

    params->presetGUID = nvenc->selected_preset;
  }

  params->enablePTD = 1;
  if (!reconfigure) {
    /* this sets the required buffer size and the maximum allowed size on
     * subsequent reconfigures */
    params->maxEncodeWidth = GST_VIDEO_INFO_WIDTH (info);
    params->maxEncodeHeight = GST_VIDEO_INFO_HEIGHT (info);
  }

  preset_config.version = gst_nvenc_get_preset_config_version ();
  preset_config.presetCfg.version = gst_nvenc_get_config_version ();

  nv_ret =
      NvEncGetEncodePresetConfig (nvenc->encoder,
      params->encodeGUID, params->presetGUID, &preset_config);
  if (nv_ret != NV_ENC_SUCCESS) {
    GST_ELEMENT_ERROR (nvenc, LIBRARY, SETTINGS, (NULL),
        ("Failed to get encode preset configuration: %d", nv_ret));
    return FALSE;
  }

  params->encodeConfig = &preset_config.presetCfg;

  if (GST_VIDEO_INFO_IS_INTERLACED (info)) {
    if (GST_VIDEO_INFO_INTERLACE_MODE (info) ==
        GST_VIDEO_INTERLACE_MODE_INTERLEAVED
        || GST_VIDEO_INFO_INTERLACE_MODE (info) ==
        GST_VIDEO_INTERLACE_MODE_MIXED) {
      preset_config.presetCfg.frameFieldMode =
          NV_ENC_PARAMS_FRAME_FIELD_MODE_FIELD;
    }
  }

  if (info->fps_d > 0 && info->fps_n > 0) {
    params->frameRateNum = info->fps_n;
    params->frameRateDen = info->fps_d;
  } else {
    params->frameRateNum = 0;
    params->frameRateDen = 1;
  }

  if (gst_util_fraction_multiply (GST_VIDEO_INFO_WIDTH (info),
          GST_VIDEO_INFO_HEIGHT (info), GST_VIDEO_INFO_PAR_N (info),
          GST_VIDEO_INFO_PAR_D (info), &dar_n, &dar_d) && dar_n > 0
      && dar_d > 0) {
    params->darWidth = dar_n;
    params->darHeight = dar_d;
  }

  gst_nv_base_enc_setup_rate_control (nvenc, &params->encodeConfig->rcParams);

  params->enableWeightedPrediction = nvenc->weighted_pred;

  if (nvenc->gop_size < 0) {
    params->encodeConfig->gopLength = NVENC_INFINITE_GOPLENGTH;
    params->encodeConfig->frameIntervalP = 1;
  } else if (nvenc->gop_size > 0) {
    params->encodeConfig->gopLength = nvenc->gop_size;
    /* frameIntervalP
     * 0: All Intra frames
     * 1: I/P only
     * n ( > 1): n - 1 bframes
     */
    params->encodeConfig->frameIntervalP = nvenc->bframes + 1;
  } else {
    /* gop size == 0 means all intra frames */
    params->encodeConfig->gopLength = 1;
    params->encodeConfig->frameIntervalP = 0;
  }

  g_assert (nvenc_class->set_encoder_config);
  if (!nvenc_class->set_encoder_config (nvenc, state, params->encodeConfig)) {
    GST_ERROR_OBJECT (enc, "Subclass failed to set encoder configuration");
    return FALSE;
  }

  /* store the last config to reconfig/re-init decision in the next time */
  nvenc->config = *params->encodeConfig;

  G_LOCK (initialization_lock);
  if (reconfigure) {
    reconfigure_params.reInitEncodeParams = nvenc->init_params;
    nv_ret = NvEncReconfigureEncoder (nvenc->encoder, &reconfigure_params);
  } else {
    nv_ret = NvEncInitializeEncoder (nvenc->encoder, params);
  }
  G_UNLOCK (initialization_lock);

  if (nv_ret != NV_ENC_SUCCESS) {
    GST_ELEMENT_ERROR (nvenc, LIBRARY, SETTINGS, (NULL),
        ("Failed to %sinit encoder: %d- %s", reconfigure ? "re" : "", nv_ret,
            NvEncGetLastErrorString (nvenc->encoder)));
    NvEncDestroyEncoder (nvenc->encoder);
    nvenc->encoder = NULL;
    return FALSE;
  }

  if (!reconfigure) {
    nvenc->input_info = *info;
  }

  if (nvenc->input_state)
    gst_video_codec_state_unref (nvenc->input_state);
  nvenc->input_state = gst_video_codec_state_ref (state);
  GST_INFO_OBJECT (nvenc, "%sconfigured encoder", reconfigure ? "re" : "");

  /* now allocate some buffers only on first configuration */
  if (!reconfigure) {
    GstCapsFeatures *features;
    guint i;
    guint input_width, input_height;
    guint n_bufs;

    input_width = GST_VIDEO_INFO_WIDTH (info);
    input_height = GST_VIDEO_INFO_HEIGHT (info);

    n_bufs =
        gst_nv_base_enc_calculate_num_prealloc_buffers (nvenc,
        params->encodeConfig);

    /* input buffers */
    g_array_set_size (nvenc->items, n_bufs);

    nvenc->mem_type = GST_NVENC_MEM_TYPE_SYSTEM;

    features = gst_caps_get_features (state->caps, 0);
    if (gst_caps_features_contains (features,
            GST_CAPS_FEATURE_MEMORY_CUDA_MEMORY)) {
      nvenc->mem_type = GST_NVENC_MEM_TYPE_CUDA;
    }
#ifdef HAVE_CUDA_GST_GL
    else if (gst_caps_features_contains (features,
            GST_CAPS_FEATURE_MEMORY_GL_MEMORY)) {
      nvenc->mem_type = GST_NVENC_MEM_TYPE_GL;
    }
#endif

    gst_cuda_context_push (nvenc->cuda_ctx);
    for (i = 0; i < nvenc->items->len; ++i) {
      GstNvEncInputResource *resource = g_new0 (GstNvEncInputResource, 1);
      CUresult cu_ret;

      memset (&resource->nv_resource, 0, sizeof (resource->nv_resource));
      memset (&resource->nv_mapped_resource, 0,
          sizeof (resource->nv_mapped_resource));

      /* scratch buffer for non-contiguous planer into a contiguous buffer */
      cu_ret =
          CuMemAllocPitch (&resource->cuda_pointer,
          &resource->cuda_stride, _get_plane_width (info, 0),
          _get_frame_data_height (info), 16);
      if (!gst_cuda_result (cu_ret)) {
        GST_ERROR_OBJECT (nvenc, "failed to allocate cuda scratch buffer "
            "ret %d", cu_ret);
        g_assert_not_reached ();
      }

      resource->nv_resource.version =
          gst_nvenc_get_register_resource_version ();
      resource->nv_resource.resourceType =
          NV_ENC_INPUT_RESOURCE_TYPE_CUDADEVICEPTR;
      resource->nv_resource.width = input_width;
      resource->nv_resource.height = input_height;
      resource->nv_resource.pitch = resource->cuda_stride;
      resource->nv_resource.bufferFormat =
          gst_nvenc_get_nv_buffer_format (GST_VIDEO_INFO_FORMAT (info));
      resource->nv_resource.resourceToRegister =
          (gpointer) resource->cuda_pointer;

      nv_ret = NvEncRegisterResource (nvenc->encoder, &resource->nv_resource);
      if (nv_ret != NV_ENC_SUCCESS)
        GST_ERROR_OBJECT (nvenc, "Failed to register resource %p, ret %d",
            resource, nv_ret);

      g_array_index (nvenc->items, GstNvEncFrameState, i).in_buf = resource;
    }
    gst_cuda_context_pop (NULL);

    /* output buffers */
    for (i = 0; i < nvenc->items->len; ++i) {
      NV_ENC_CREATE_BITSTREAM_BUFFER cout_buf = { 0, };

      cout_buf.version = gst_nvenc_get_create_bitstream_buffer_version ();

      /* 1 MB should be large enough to hold most output frames.
       * NVENC will automatically increase this if it's not enough. */
      cout_buf.size = 1024 * 1024;
      cout_buf.memoryHeap = NV_ENC_MEMORY_HEAP_SYSMEM_CACHED;

      G_LOCK (initialization_lock);
      nv_ret = NvEncCreateBitstreamBuffer (nvenc->encoder, &cout_buf);
      G_UNLOCK (initialization_lock);

      if (nv_ret != NV_ENC_SUCCESS) {
        GST_WARNING_OBJECT (enc, "Failed to allocate input buffer: %d", nv_ret);
        /* FIXME: clean up */
        return FALSE;
      }

      GST_INFO_OBJECT (nvenc, "allocated output buffer %2d: %p", i,
          cout_buf.bitstreamBuffer);

      g_array_index (nvenc->items, GstNvEncFrameState, i).out_buf =
          cout_buf.bitstreamBuffer;

      g_async_queue_push (nvenc->available_queue, &g_array_index (nvenc->items,
              GstNvEncFrameState, i));
    }

#if 0
    /* Get SPS/PPS */
    {
      NV_ENC_SEQUENCE_PARAM_PAYLOAD seq_param = { 0 };
      uint32_t seq_size = 0;

      seq_param.version = gst_nvenc_get_sequence_param_payload_version ();
      seq_param.spsppsBuffer = g_alloca (1024);
      seq_param.inBufferSize = 1024;
      seq_param.outSPSPPSPayloadSize = &seq_size;

      nv_ret = NvEncGetSequenceParams (nvenc->encoder, &seq_param);
      if (nv_ret != NV_ENC_SUCCESS) {
        GST_WARNING_OBJECT (enc, "Failed to retrieve SPS/PPS: %d", nv_ret);
        return FALSE;
      }

      /* FIXME: use SPS/PPS */
      GST_MEMDUMP_OBJECT (enc, "SPS/PPS", seq_param.spsppsBuffer, seq_size);
    }
#endif
  }

  g_assert (nvenc_class->set_src_caps);
  if (!nvenc_class->set_src_caps (nvenc, state)) {
    GST_ERROR_OBJECT (nvenc, "Subclass failed to set output caps");
    /* FIXME: clean up */
    return FALSE;
  }

  return TRUE;
}

static guint
_get_cuda_device_stride (GstVideoInfo * info, guint plane, gsize cuda_stride)
{
  switch (GST_VIDEO_INFO_FORMAT (info)) {
    case GST_VIDEO_FORMAT_NV12:
    case GST_VIDEO_FORMAT_NV21:
    case GST_VIDEO_FORMAT_P010_10LE:
    case GST_VIDEO_FORMAT_P010_10BE:
    case GST_VIDEO_FORMAT_Y444:
    case GST_VIDEO_FORMAT_BGRA:
    case GST_VIDEO_FORMAT_RGBA:
    case GST_VIDEO_FORMAT_BGR10A2_LE:
    case GST_VIDEO_FORMAT_RGB10A2_LE:
    case GST_VIDEO_FORMAT_Y444_16LE:
    case GST_VIDEO_FORMAT_Y444_16BE:
    case GST_VIDEO_FORMAT_VUYA:
      return cuda_stride;
    case GST_VIDEO_FORMAT_I420:
    case GST_VIDEO_FORMAT_YV12:
      return plane == 0 ? cuda_stride : (GST_ROUND_UP_2 (cuda_stride) / 2);
    default:
      g_assert_not_reached ();
      return cuda_stride;
  }
}

#ifdef HAVE_CUDA_GST_GL
typedef struct _GstNvEncRegisterResourceData
{
  GstMemory *mem;
  GstCudaGraphicsResource *resource;
  GstNvBaseEnc *nvenc;
  gboolean ret;
} GstNvEncRegisterResourceData;

static void
register_cuda_resource (GstGLContext * context,
    GstNvEncRegisterResourceData * data)
{
  GstMemory *mem = data->mem;
  GstCudaGraphicsResource *resource = data->resource;
  GstNvBaseEnc *nvenc = data->nvenc;
  GstMapInfo map_info = GST_MAP_INFO_INIT;
  GstGLBuffer *gl_buf_obj;

  data->ret = FALSE;

  if (!gst_cuda_context_push (nvenc->cuda_ctx)) {
    GST_WARNING_OBJECT (nvenc, "failed to push CUDA context");
    return;
  }

  if (gst_memory_map (mem, &map_info, GST_MAP_READ | GST_MAP_GL)) {
    GstGLMemoryPBO *gl_mem = (GstGLMemoryPBO *) data->mem;
    gl_buf_obj = gl_mem->pbo;

    GST_LOG_OBJECT (nvenc,
        "register glbuffer %d to CUDA resource", gl_buf_obj->id);

    if (gst_cuda_graphics_resource_register_gl_buffer (resource,
            gl_buf_obj->id, CU_GRAPHICS_REGISTER_FLAGS_NONE)) {
      data->ret = TRUE;
    } else {
      GST_WARNING_OBJECT (nvenc, "failed to register memory");
    }

    gst_memory_unmap (mem, &map_info);
  } else {
    GST_WARNING_OBJECT (nvenc, "failed to map memory");
  }

  if (!gst_cuda_context_pop (NULL))
    GST_WARNING_OBJECT (nvenc, "failed to unlock CUDA context");
}

static GstCudaGraphicsResource *
ensure_cuda_graphics_resource (GstMemory * mem, GstNvBaseEnc * nvenc)
{
  GQuark quark;
  GstCudaGraphicsResource *cgr_info;
  GstNvEncRegisterResourceData data;

  if (!gst_is_gl_memory_pbo (mem)) {
    GST_WARNING_OBJECT (nvenc, "memory is not GL PBO memory, %s",
        mem->allocator->mem_type);
    return NULL;
  }

  quark = gst_cuda_quark_from_id (GST_CUDA_QUARK_GRAPHICS_RESOURCE);

  cgr_info = gst_mini_object_get_qdata (GST_MINI_OBJECT (mem), quark);
  if (!cgr_info) {
    cgr_info = gst_cuda_graphics_resource_new (nvenc->cuda_ctx,
        GST_OBJECT (GST_GL_BASE_MEMORY_CAST (mem)->context),
        GST_CUDA_GRAPHICS_RESOURCE_GL_BUFFER);
    data.mem = mem;
    data.resource = cgr_info;
    data.nvenc = nvenc;
    gst_gl_context_thread_add ((GstGLContext *) cgr_info->graphics_context,
        (GstGLContextThreadFunc) register_cuda_resource, &data);
    if (!data.ret) {
      GST_WARNING_OBJECT (nvenc, "could not register resource");
      gst_cuda_graphics_resource_free (cgr_info);

      return NULL;
    }

    gst_mini_object_set_qdata (GST_MINI_OBJECT (mem), quark, cgr_info,
        (GDestroyNotify) gst_cuda_graphics_resource_free);
  }

  return cgr_info;
}

typedef struct _GstNvEncGLMapData
{
  GstNvBaseEnc *nvenc;
  GstBuffer *buffer;
  GstVideoInfo *info;
  GstNvEncInputResource *resource;

  gboolean ret;
} GstNvEncGLMapData;

static void
_map_gl_input_buffer (GstGLContext * context, GstNvEncGLMapData * data)
{
  GstNvBaseEnc *nvenc = data->nvenc;
  CUresult cuda_ret;
  CUdeviceptr data_pointer;
  guint i;
  CUDA_MEMCPY2D param;
  GstCudaGraphicsResource **resources;
  guint num_resources;
  CUstream stream = gst_cuda_stream_get_handle (nvenc->stream);

  data->ret = FALSE;

  num_resources = gst_buffer_n_memory (data->buffer);
  resources = g_newa (GstCudaGraphicsResource *, num_resources);

  for (i = 0; i < num_resources; i++) {
    GstMemory *mem;

    mem = gst_buffer_peek_memory (data->buffer, i);
    resources[i] = ensure_cuda_graphics_resource (mem, nvenc);
    if (!resources[i]) {
      GST_ERROR_OBJECT (nvenc, "could not register %dth memory", i);
      return;
    }
  }

  gst_cuda_context_push (nvenc->cuda_ctx);
  data_pointer = data->resource->cuda_pointer;
  for (i = 0; i < GST_VIDEO_INFO_N_PLANES (data->info); i++) {
    GstGLBuffer *gl_buf_obj;
    GstGLMemoryPBO *gl_mem;
    guint src_stride, dest_stride;
    CUgraphicsResource cuda_resource;
    gsize cuda_num_bytes;
    CUdeviceptr cuda_plane_pointer;

    gl_mem = (GstGLMemoryPBO *) gst_buffer_peek_memory (data->buffer, i);
    g_return_if_fail (gst_is_gl_memory_pbo ((GstMemory *) gl_mem));

    gl_buf_obj = (GstGLBuffer *) gl_mem->pbo;
    g_return_if_fail (gl_buf_obj != NULL);

    /* get the texture into the PBO */
    gst_gl_memory_pbo_upload_transfer (gl_mem);
    gst_gl_memory_pbo_download_transfer (gl_mem);

    GST_LOG_OBJECT (nvenc, "attempting to copy texture %u into cuda",
        gl_mem->mem.tex_id);

    cuda_resource =
        gst_cuda_graphics_resource_map (resources[i], stream,
        CU_GRAPHICS_MAP_RESOURCE_FLAGS_READ_ONLY);

    if (!cuda_resource) {
      GST_ERROR_OBJECT (nvenc, "failed to map GL texture %u into cuda",
          gl_mem->mem.tex_id);
      g_assert_not_reached ();
    }

    cuda_ret =
        CuGraphicsResourceGetMappedPointer (&cuda_plane_pointer,
        &cuda_num_bytes, cuda_resource);

    if (!gst_cuda_result (cuda_ret)) {
      GST_ERROR_OBJECT (nvenc, "failed to get mapped pointer of map GL "
          "texture %u in cuda ret :%d", gl_mem->mem.tex_id, cuda_ret);
      g_assert_not_reached ();
    }

    src_stride = GST_VIDEO_INFO_PLANE_STRIDE (data->info, i);
    dest_stride = _get_cuda_device_stride (&nvenc->input_info,
        i, data->resource->cuda_stride);

    /* copy into scratch buffer */
    param.srcXInBytes = 0;
    param.srcY = 0;
    param.srcMemoryType = CU_MEMORYTYPE_DEVICE;
    param.srcDevice = cuda_plane_pointer;
    param.srcPitch = src_stride;

    param.dstXInBytes = 0;
    param.dstY = 0;
    param.dstMemoryType = CU_MEMORYTYPE_DEVICE;
    param.dstDevice = data_pointer;
    param.dstPitch = dest_stride;
    param.WidthInBytes = _get_plane_width (data->info, i);
    param.Height = _get_plane_height (data->info, i);

    cuda_ret = CuMemcpy2DAsync (&param, stream);
    if (!gst_cuda_result (cuda_ret)) {
      GST_ERROR_OBJECT (data->nvenc, "failed to copy GL texture %u into cuda "
          "ret :%d", gl_mem->mem.tex_id, cuda_ret);
      g_assert_not_reached ();
    }

    gst_cuda_graphics_resource_unmap (resources[i], stream);

    data_pointer += dest_stride * _get_plane_height (&nvenc->input_info, i);
  }
  gst_cuda_result (CuStreamSynchronize (stream));
  gst_cuda_context_pop (NULL);

  data->ret = TRUE;
}
#endif

static gboolean
gst_nv_base_enc_upload_frame (GstNvBaseEnc * nvenc, GstVideoFrame * frame,
    GstNvEncInputResource * resource, gboolean use_device_memory,
    GstCudaStream * stream)
{
  gint i;
  CUdeviceptr dst = resource->cuda_pointer;
  GstVideoInfo *info = &frame->info;
  CUresult cuda_ret;
  CUstream stream_handle = gst_cuda_stream_get_handle (stream);

  if (!gst_cuda_context_push (nvenc->cuda_ctx)) {
    GST_ERROR_OBJECT (nvenc, "cannot push context");
    return FALSE;
  }

  for (i = 0; i < GST_VIDEO_FRAME_N_PLANES (frame); i++) {
    CUDA_MEMCPY2D param = { 0, };
    guint dest_stride = _get_cuda_device_stride (&nvenc->input_info, i,
        resource->cuda_stride);

    if (use_device_memory) {
      param.srcMemoryType = CU_MEMORYTYPE_DEVICE;
      param.srcDevice = (CUdeviceptr) GST_VIDEO_FRAME_PLANE_DATA (frame, i);
    } else {
      param.srcMemoryType = CU_MEMORYTYPE_HOST;
      param.srcHost = GST_VIDEO_FRAME_PLANE_DATA (frame, i);
    }
    param.srcPitch = GST_VIDEO_FRAME_PLANE_STRIDE (frame, i);

    param.dstMemoryType = CU_MEMORYTYPE_DEVICE;
    param.dstDevice = dst;
    param.dstPitch = dest_stride;
    param.WidthInBytes = _get_plane_width (info, i);
    param.Height = _get_plane_height (info, i);

    cuda_ret = CuMemcpy2DAsync (&param, stream_handle);
    if (!gst_cuda_result (cuda_ret)) {
      GST_ERROR_OBJECT (nvenc, "cannot copy %dth plane, ret %d", i, cuda_ret);
      gst_cuda_context_pop (NULL);

      return FALSE;
    }

    dst += dest_stride * _get_plane_height (&nvenc->input_info, i);
  }

  gst_cuda_result (CuStreamSynchronize (stream_handle));
  gst_cuda_context_pop (NULL);

  return TRUE;
}

static GstFlowReturn
_acquire_input_buffer (GstNvBaseEnc * nvenc, GstNvEncFrameState ** input)
{
  GST_LOG_OBJECT (nvenc, "acquiring input buffer..");
  GST_VIDEO_ENCODER_STREAM_UNLOCK (nvenc);
  *input = g_async_queue_pop (nvenc->available_queue);
  GST_VIDEO_ENCODER_STREAM_LOCK (nvenc);

  if (*input == SHUTDOWN_COOKIE)
    return g_atomic_int_get (&nvenc->last_flow);

  return GST_FLOW_OK;
}

static GstFlowReturn
_submit_input_buffer (GstNvBaseEnc * nvenc, GstVideoCodecFrame * frame,
    GstVideoFrame * vframe, GstNvEncFrameState * state, void *inputBufferPtr,
    NV_ENC_BUFFER_FORMAT bufferFormat)
{
  GstNvBaseEncClass *nvenc_class = GST_NV_BASE_ENC_GET_CLASS (nvenc);
  NV_ENC_PIC_PARAMS pic_params = { 0, };
  NVENCSTATUS nv_ret;
  gpointer inputBuffer, outputBufferPtr;

  inputBuffer = state->in_buf;
  outputBufferPtr = state->out_buf;

  GST_LOG_OBJECT (nvenc, "%u: input buffer %p, output buffer %p, "
      "pts %" GST_TIME_FORMAT, frame->system_frame_number, inputBuffer,
      outputBufferPtr, GST_TIME_ARGS (frame->pts));

  pic_params.version = gst_nvenc_get_pic_params_version ();
  pic_params.inputBuffer = inputBufferPtr;
  pic_params.bufferFmt = bufferFormat;

  pic_params.inputWidth = GST_VIDEO_FRAME_WIDTH (vframe);
  pic_params.inputHeight = GST_VIDEO_FRAME_HEIGHT (vframe);
  pic_params.outputBitstream = outputBufferPtr;
  pic_params.completionEvent = NULL;
  if (GST_VIDEO_FRAME_IS_INTERLACED (vframe)) {
    if (GST_VIDEO_FRAME_IS_TFF (vframe))
      pic_params.pictureStruct = NV_ENC_PIC_STRUCT_FIELD_TOP_BOTTOM;
    else
      pic_params.pictureStruct = NV_ENC_PIC_STRUCT_FIELD_BOTTOM_TOP;
  } else {
    pic_params.pictureStruct = NV_ENC_PIC_STRUCT_FRAME;
  }
  pic_params.inputTimeStamp = frame->pts;
  pic_params.inputDuration =
      GST_CLOCK_TIME_IS_VALID (frame->duration) ? frame->duration : 0;
  pic_params.frameIdx = frame->system_frame_number;

  if (GST_VIDEO_CODEC_FRAME_IS_FORCE_KEYFRAME (frame))
    pic_params.encodePicFlags = NV_ENC_PIC_FLAG_FORCEIDR;
  else
    pic_params.encodePicFlags = 0;

  if (nvenc_class->set_pic_params
      && !nvenc_class->set_pic_params (nvenc, frame, &pic_params)) {
    GST_ERROR_OBJECT (nvenc, "Subclass failed to submit buffer");
    return GST_FLOW_ERROR;
  }

  if (!gst_cuda_context_push (nvenc->cuda_ctx)) {
    GST_ELEMENT_ERROR (nvenc, LIBRARY, ENCODE, (NULL),
        ("Failed to push current context"));
    return GST_FLOW_ERROR;
  }

  nv_ret = NvEncEncodePicture (nvenc->encoder, &pic_params);

  gst_cuda_context_pop (NULL);

  if (nv_ret == NV_ENC_SUCCESS) {
    GST_LOG_OBJECT (nvenc, "Encoded picture");
  } else if (nv_ret == NV_ENC_ERR_NEED_MORE_INPUT) {
    GST_DEBUG_OBJECT (nvenc, "Encoded picture (encoder needs more input)");
  } else {
    GST_ERROR_OBJECT (nvenc, "Failed to encode picture: %d", nv_ret);
    g_async_queue_push (nvenc->available_queue, state);

    return GST_FLOW_ERROR;
  }

  /* GstNvEncFrameState shouldn't be freed by DestroyNotify */
  gst_video_codec_frame_set_user_data (frame, state, NULL);
  g_async_queue_push (nvenc->pending_queue, state);

  if (nv_ret == NV_ENC_SUCCESS) {
    GstNvEncFrameState *pending_state;
    gint len, i, end;

    /* HACK: NvEncEncodePicture() with returning NV_ENC_SUCCESS means that
     * we can pop encoded bitstream from GPU
     * (via NvEncLockBitstream and copy to memory then NvEncUnlockBitstream).
     * But if we try to pop every buffer from GPU when the rc-lookahead
     * was enabled, NvEncLockBitstream returns error NV_ENC_ERR_INVALID_PARAM
     * randomly (seemingly it's dependent on how fast the encoding thread
     * dequeued the encoded picture).
     * So make "pending_queue" having the number of lookahead pictures always,
     * so that GPU should be able to reference the lookahead pictures.
     *
     * This behavior is not documented by Nvidia. The guess here is that
     * the lookahead pictures are still used for rate-control by Nvidia driver
     * and dequeuing the lookahead picture from GPU seems to be causing the
     * problem.
     */
    end = nvenc->rc_lookahead;

    g_async_queue_lock (nvenc->pending_queue);

    len = g_async_queue_length_unlocked (nvenc->pending_queue);
    for (i = len; i > end; i--) {
      pending_state = g_async_queue_pop_unlocked (nvenc->pending_queue);
      g_async_queue_push (nvenc->bitstream_queue, pending_state);
    }

    g_async_queue_unlock (nvenc->pending_queue);
  }

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_nv_base_enc_handle_frame (GstVideoEncoder * enc, GstVideoCodecFrame * frame)
{
  GstNvBaseEnc *nvenc = GST_NV_BASE_ENC (enc);
  NVENCSTATUS nv_ret;
  GstVideoFrame vframe;
  GstVideoInfo *info = &nvenc->input_state->info;
  GstFlowReturn flow = GST_FLOW_OK;
  GstMapFlags in_map_flags = GST_MAP_READ;
  GstNvEncFrameState *state = NULL;
  GstNvEncInputResource *resource = NULL;
  gboolean use_device_memory = FALSE;
  GstCudaStream *stream = nvenc->stream;

  g_assert (nvenc->encoder != NULL);

  /* check last flow and if it's not OK, just return the last flow,
   * non-OK flow means that encoding thread was terminated */
  flow = g_atomic_int_get (&nvenc->last_flow);
  if (flow != GST_FLOW_OK) {
    GST_DEBUG_OBJECT (nvenc, "last flow was %s", gst_flow_get_name (flow));
    /* just drop this frame */
    gst_video_encoder_finish_frame (enc, frame);

    return flow;
  }

  if (g_atomic_int_compare_and_exchange (&nvenc->reconfig, TRUE, FALSE)) {
    if (!gst_nv_base_enc_set_format (enc, nvenc->input_state)) {
      flow = GST_FLOW_NOT_NEGOTIATED;
      goto drop;
    }

    /* reconfigured encode session should start from keyframe */
    GST_VIDEO_CODEC_FRAME_SET_FORCE_KEYFRAME (frame);
  }
#ifdef HAVE_CUDA_GST_GL
  if (nvenc->mem_type == GST_NVENC_MEM_TYPE_GL)
    in_map_flags |= GST_MAP_GL;
#endif

  if (nvenc->mem_type == GST_NVENC_MEM_TYPE_CUDA) {
    GstMemory *mem;

    if ((mem = gst_buffer_peek_memory (frame->input_buffer, 0)) &&
        gst_is_cuda_memory (mem)) {
      GstCudaMemory *cmem = GST_CUDA_MEMORY_CAST (mem);

      if (cmem->context == nvenc->cuda_ctx ||
          gst_cuda_context_get_handle (cmem->context) ==
          gst_cuda_context_get_handle (nvenc->cuda_ctx) ||
          (gst_cuda_context_can_access_peer (cmem->context, nvenc->cuda_ctx) &&
              gst_cuda_context_can_access_peer (nvenc->cuda_ctx,
                  cmem->context))) {
        GstCudaStream *mem_stream;

        use_device_memory = TRUE;
        in_map_flags |= GST_MAP_CUDA;

        mem_stream = gst_cuda_memory_get_stream (cmem);
        if (mem_stream)
          stream = mem_stream;
      }
    }
  }

  if (!gst_video_frame_map (&vframe, info, frame->input_buffer, in_map_flags)) {
    goto drop;
  }

  /* make sure our thread that waits for output to be ready is started */
  if (nvenc->bitstream_thread == NULL) {
    if (!gst_nv_base_enc_start_bitstream_thread (nvenc)) {
      gst_video_frame_unmap (&vframe);
      goto unmap_and_drop;
    }
  }

  flow = _acquire_input_buffer (nvenc, &state);
  if (flow != GST_FLOW_OK || state == SHUTDOWN_COOKIE || !state)
    goto unmap_and_drop;

  resource = state->in_buf;

#ifdef HAVE_CUDA_GST_GL
  if (nvenc->mem_type == GST_NVENC_MEM_TYPE_GL) {
    GstGLMemory *gl_mem;
    GstNvEncGLMapData data;

    gl_mem = (GstGLMemory *) gst_buffer_peek_memory (frame->input_buffer, 0);
    g_assert (gst_is_gl_memory ((GstMemory *) gl_mem));

    data.nvenc = nvenc;
    data.buffer = frame->input_buffer;
    data.info = &vframe.info;
    data.resource = resource;

    gst_gl_context_thread_add (gl_mem->mem.context,
        (GstGLContextThreadFunc) _map_gl_input_buffer, &data);
    if (!data.ret) {
      flow = GST_FLOW_ERROR;
      goto unmap_and_drop;
    }
  } else
#endif
  if (!gst_nv_base_enc_upload_frame (nvenc,
          &vframe, resource, use_device_memory, stream)) {
    flow = GST_FLOW_ERROR;
    goto unmap_and_drop;
  }

  resource->nv_mapped_resource.version =
      gst_nvenc_get_map_input_resource_version ();
  resource->nv_mapped_resource.registeredResource =
      resource->nv_resource.registeredResource;

  if (!gst_cuda_context_push (nvenc->cuda_ctx)) {
    GST_ELEMENT_ERROR (nvenc, LIBRARY, ENCODE, (NULL),
        ("Failed to push current context"));
    flow = GST_FLOW_ERROR;
    goto unmap_and_drop;
  }

  nv_ret =
      NvEncMapInputResource (nvenc->encoder, &resource->nv_mapped_resource);
  gst_cuda_context_pop (NULL);

  if (nv_ret != NV_ENC_SUCCESS) {
    GST_ERROR_OBJECT (nvenc, "Failed to map input resource %p, ret %d",
        resource, nv_ret);
    flow = GST_FLOW_ERROR;
    goto unmap_and_drop;
  }

  resource->mapped = TRUE;

  flow =
      _submit_input_buffer (nvenc, frame, &vframe, state,
      resource->nv_mapped_resource.mappedResource,
      resource->nv_mapped_resource.mappedBufferFmt);

  if (flow != GST_FLOW_OK) {
    GST_DEBUG_OBJECT (nvenc, "return state to pool");
    g_async_queue_push (nvenc->available_queue, state);
    goto unmap_and_drop;
  }

  flow = g_atomic_int_get (&nvenc->last_flow);

  gst_video_frame_unmap (&vframe);
  /* encoder will keep frame in list internally, we'll look it up again later
   * in the thread where we get the output buffers and finish it there */
  gst_video_codec_frame_unref (frame);

  return flow;

/* ERRORS */
unmap_and_drop:
  {
    gst_video_frame_unmap (&vframe);
    goto drop;
  }
drop:
  {
    gst_video_encoder_finish_frame (enc, frame);
    return flow;
  }
}

static gboolean
gst_nv_base_enc_drain_encoder (GstNvBaseEnc * nvenc)
{
  NV_ENC_PIC_PARAMS pic_params = { 0, };
  NVENCSTATUS nv_ret;
  gboolean ret = TRUE;

  GST_INFO_OBJECT (nvenc, "draining encoder");

  if (nvenc->input_state == NULL) {
    GST_DEBUG_OBJECT (nvenc, "no input state, nothing to do");
    return TRUE;
  }

  if (!nvenc->encoder) {
    GST_DEBUG_OBJECT (nvenc, "no configured encode session");
    return TRUE;
  }

  pic_params.version = gst_nvenc_get_pic_params_version ();
  pic_params.encodePicFlags = NV_ENC_PIC_FLAG_EOS;

  if (!gst_cuda_context_push (nvenc->cuda_ctx)) {
    GST_ERROR_OBJECT (nvenc, "Could not push context");
    return GST_FLOW_ERROR;
  }

  nv_ret = NvEncEncodePicture (nvenc->encoder, &pic_params);

  if (nv_ret != NV_ENC_SUCCESS) {
    GST_LOG_OBJECT (nvenc, "Failed to drain encoder, ret %d", nv_ret);

    ret = FALSE;
  } else {
    GstNvEncFrameState *pending_state;

    g_async_queue_lock (nvenc->pending_queue);
    while ((pending_state =
            g_async_queue_try_pop_unlocked (nvenc->pending_queue))) {
      g_async_queue_push (nvenc->bitstream_queue, pending_state);
    }
    g_async_queue_unlock (nvenc->pending_queue);
  }

  gst_cuda_context_pop (NULL);

  return ret;
}

static GstFlowReturn
gst_nv_base_enc_finish (GstVideoEncoder * enc)
{
  GstNvBaseEnc *nvenc = GST_NV_BASE_ENC (enc);

  gst_nv_base_enc_stop_bitstream_thread (nvenc, FALSE);

  return GST_FLOW_OK;
}

#if 0
static gboolean
gst_nv_base_enc_flush (GstVideoEncoder * enc)
{
  GstNvBaseEnc *nvenc = GST_NV_BASE_ENC (enc);
  GST_INFO_OBJECT (nvenc, "done flushing encoder");
  return TRUE;
}
#endif

void
gst_nv_base_enc_schedule_reconfig (GstNvBaseEnc * nvenc)
{
  g_atomic_int_set (&nvenc->reconfig, TRUE);
}

static void
gst_nv_base_enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstNvBaseEnc *nvenc = GST_NV_BASE_ENC (object);
  GstNvBaseEncClass *klass = GST_NV_BASE_ENC_GET_CLASS (nvenc);
  gboolean reconfig = TRUE;

  switch (prop_id) {
    case PROP_PRESET:
      nvenc->preset_enum = g_value_get_enum (value);
      nvenc->selected_preset = _nv_preset_to_guid (nvenc->preset_enum);
      gst_nv_base_enc_schedule_reconfig (nvenc);
      break;
    case PROP_RC_MODE:
    {
      GstNvRCMode rc_mode = g_value_get_enum (value);
      NV_ENC_PARAMS_RC_MODE nv_rc_mode = _rc_mode_to_nv (rc_mode);

      if ((klass->device_caps.rc_modes & nv_rc_mode) == nv_rc_mode) {
        nvenc->rate_control_mode = rc_mode;
      } else {
        GST_WARNING_OBJECT (nvenc,
            "device does not support requested rate control mode %d", rc_mode);
        reconfig = FALSE;
      }
      break;
    }
    case PROP_QP_MIN:
      nvenc->qp_min = g_value_get_int (value);
      break;
    case PROP_QP_MAX:
      nvenc->qp_max = g_value_get_int (value);
      break;
    case PROP_QP_CONST:
      nvenc->qp_const = g_value_get_int (value);
      break;
    case PROP_BITRATE:
      nvenc->bitrate = g_value_get_uint (value);
      break;
    case PROP_GOP_SIZE:
      nvenc->gop_size = g_value_get_int (value);
      break;
    case PROP_MAX_BITRATE:
      nvenc->max_bitrate = g_value_get_uint (value);
      break;
    case PROP_SPATIAL_AQ:
      nvenc->spatial_aq = g_value_get_boolean (value);
      break;
    case PROP_AQ_STRENGTH:
      nvenc->aq_strength = g_value_get_uint (value);
      break;
    case PROP_NON_REF_P:
      nvenc->non_refp = g_value_get_boolean (value);
      break;
    case PROP_ZEROLATENCY:
      nvenc->zerolatency = g_value_get_boolean (value);
      break;
    case PROP_STRICT_GOP:
      nvenc->strict_gop = g_value_get_boolean (value);
      break;
    case PROP_CONST_QUALITY:
      nvenc->const_quality = g_value_get_double (value);
      break;
    case PROP_I_ADAPT:
      nvenc->i_adapt = g_value_get_boolean (value);
      break;
    case PROP_QP_MIN_I:
      nvenc->qp_min_detail.qp_i = g_value_get_int (value);
      break;
    case PROP_QP_MIN_P:
      nvenc->qp_min_detail.qp_p = g_value_get_int (value);
      break;
    case PROP_QP_MIN_B:
      nvenc->qp_min_detail.qp_b = g_value_get_int (value);
      break;
    case PROP_QP_MAX_I:
      nvenc->qp_max_detail.qp_i = g_value_get_int (value);
      break;
    case PROP_QP_MAX_P:
      nvenc->qp_max_detail.qp_p = g_value_get_int (value);
      break;
    case PROP_QP_MAX_B:
      nvenc->qp_max_detail.qp_b = g_value_get_int (value);
      break;
    case PROP_QP_CONST_I:
      nvenc->qp_const_detail.qp_i = g_value_get_int (value);
      break;
    case PROP_QP_CONST_P:
      nvenc->qp_const_detail.qp_p = g_value_get_int (value);
      break;
    case PROP_QP_CONST_B:
      nvenc->qp_const_detail.qp_b = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      reconfig = FALSE;
      break;
  }

  if (reconfig)
    gst_nv_base_enc_schedule_reconfig (nvenc);
}

static void
gst_nv_base_enc_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstNvBaseEnc *nvenc = GST_NV_BASE_ENC (object);
  GstNvBaseEncClass *nvenc_class = GST_NV_BASE_ENC_GET_CLASS (object);

  switch (prop_id) {
    case PROP_DEVICE_ID:
      g_value_set_uint (value, nvenc_class->cuda_device_id);
      break;
    case PROP_PRESET:
      g_value_set_enum (value, nvenc->preset_enum);
      break;
    case PROP_RC_MODE:
      g_value_set_enum (value, nvenc->rate_control_mode);
      break;
    case PROP_QP_MIN:
      g_value_set_int (value, nvenc->qp_min);
      break;
    case PROP_QP_MAX:
      g_value_set_int (value, nvenc->qp_max);
      break;
    case PROP_QP_CONST:
      g_value_set_int (value, nvenc->qp_const);
      break;
    case PROP_BITRATE:
      g_value_set_uint (value, nvenc->bitrate);
      break;
    case PROP_GOP_SIZE:
      g_value_set_int (value, nvenc->gop_size);
      break;
    case PROP_MAX_BITRATE:
      g_value_set_uint (value, nvenc->max_bitrate);
      break;
    case PROP_SPATIAL_AQ:
      g_value_set_boolean (value, nvenc->spatial_aq);
      break;
    case PROP_AQ_STRENGTH:
      g_value_set_uint (value, nvenc->aq_strength);
      break;
    case PROP_NON_REF_P:
      g_value_set_boolean (value, nvenc->non_refp);
      break;
    case PROP_ZEROLATENCY:
      g_value_set_boolean (value, nvenc->zerolatency);
      break;
    case PROP_STRICT_GOP:
      g_value_set_boolean (value, nvenc->strict_gop);
      break;
    case PROP_CONST_QUALITY:
      g_value_set_double (value, nvenc->const_quality);
      break;
    case PROP_I_ADAPT:
      g_value_set_boolean (value, nvenc->i_adapt);
      break;
    case PROP_QP_MIN_I:
      g_value_set_int (value, nvenc->qp_min_detail.qp_i);
      break;
    case PROP_QP_MIN_P:
      g_value_set_int (value, nvenc->qp_min_detail.qp_p);
      break;
    case PROP_QP_MIN_B:
      g_value_set_int (value, nvenc->qp_min_detail.qp_b);
      break;
    case PROP_QP_MAX_I:
      g_value_set_int (value, nvenc->qp_max_detail.qp_i);
      break;
    case PROP_QP_MAX_P:
      g_value_set_int (value, nvenc->qp_max_detail.qp_p);
      break;
    case PROP_QP_MAX_B:
      g_value_set_int (value, nvenc->qp_max_detail.qp_b);
      break;
    case PROP_QP_CONST_I:
      g_value_set_int (value, nvenc->qp_const_detail.qp_i);
      break;
    case PROP_QP_CONST_P:
      g_value_set_int (value, nvenc->qp_const_detail.qp_p);
      break;
    case PROP_QP_CONST_B:
      g_value_set_int (value, nvenc->qp_const_detail.qp_b);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

typedef struct
{
  guint cuda_device_id;
  GstNvEncDeviceCaps device_caps;
} GstNvEncClassData;

static void
gst_nv_base_enc_subclass_init (gpointer g_class, gpointer data)
{
  GstNvBaseEncClass *nvbaseenc_class = GST_NV_BASE_ENC_CLASS (g_class);
  GstNvEncClassData *cdata = (GstNvEncClassData *) data;

  nvbaseenc_class->cuda_device_id = cdata->cuda_device_id;
  nvbaseenc_class->device_caps = cdata->device_caps;

  g_free (cdata);
}

GType
gst_nv_base_enc_register (const char *codec, guint device_id,
    GstNvEncDeviceCaps * device_caps)
{
  GTypeQuery type_query;
  GTypeInfo type_info = { 0, };
  GType subtype;
  gchar *type_name;
  GstNvEncClassData *cdata;

  type_name = g_strdup_printf ("GstNvDevice%d%sEnc", device_id, codec);
  subtype = g_type_from_name (type_name);

  /* has already registered nvdeviceenc class */
  if (subtype)
    goto done;

  cdata = g_new0 (GstNvEncClassData, 1);
  cdata->cuda_device_id = device_id;
  cdata->device_caps = *device_caps;

  g_type_query (GST_TYPE_NV_BASE_ENC, &type_query);
  memset (&type_info, 0, sizeof (type_info));
  type_info.class_size = type_query.class_size;
  type_info.instance_size = type_query.instance_size;
  type_info.class_init = (GClassInitFunc) gst_nv_base_enc_subclass_init;
  type_info.class_data = cdata;

  subtype = g_type_register_static (GST_TYPE_NV_BASE_ENC,
      type_name, &type_info, 0);

  gst_type_mark_as_plugin_api (subtype, 0);

done:
  g_free (type_name);
  return subtype;
}
