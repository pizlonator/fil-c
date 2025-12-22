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

/**
 * element-nvcudah264enc:
 *
 * NVIDIA CUDA mode H.264 encoder
 *
 * Since: 1.22
 */

/**
 * element-nvd3d11h264enc:
 *
 * NVIDIA Direct3D11 mode H.264 encoder
 *
 * Since: 1.22
 */

/**
 * element-nvautogpuh264enc:
 *
 * NVIDIA auto GPU select mode H.264 encoder
 *
 * Since: 1.22
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstnvh264encoder.h"
#include <gst/codecparsers/gsth264parser.h>
#include <gst/pbutils/codec-utils.h>
#include <string>
#include <set>
#include <string.h>
#include <vector>

GST_DEBUG_CATEGORY_STATIC (gst_nv_h264_encoder_debug);
#define GST_CAT_DEFAULT gst_nv_h264_encoder_debug

static GTypeClass *parent_class = nullptr;

enum
{
  PROP_0,
  PROP_ADAPTER_LUID,
  PROP_CUDA_DEVICE_ID,

  /* init params */
  PROP_PRESET,
  PROP_TUNE,
  PROP_MULTI_PASS,
  PROP_WEIGHTED_PRED,

  /* encoding config */
  PROP_GOP_SIZE,
  PROP_B_FRAMES,

  /* rate-control params */
  PROP_RATE_CONTROL,

  PROP_QP_I,
  PROP_QP_P,
  PROP_QP_B,

  PROP_BITRATE,
  PROP_MAX_BITRATE,
  PROP_VBV_BUFFER_SIZE,

  PROP_RC_LOOKAHEAD,
  PROP_I_ADAPT,
  PROP_B_ADAPT,
  PROP_SPATIAL_AQ,
  PROP_TEMPORAL_AQ,
  PROP_ZERO_REORDER_DELAY,
  PROP_NON_REF_P,
  PROP_STRICT_GOP,
  PROP_AQ_STRENGTH,

  PROP_MIN_QP_I,
  PROP_MIN_QP_P,
  PROP_MIN_QP_B,

  PROP_MAX_QP_I,
  PROP_MAX_QP_P,
  PROP_MAX_QP_B,

  PROP_CONST_QUALITY,

  /* h264 specific */
  PROP_AUD,
  PROP_CABAC,
  PROP_REPEAT_SEQUENCE_HEADER,
};

#define DEFAULT_PRESET            GST_NV_ENCODER_PRESET_P4
#define DEFAULT_TUNE              GST_NV_ENCODER_TUNE_DEFAULT
#define DEFAULT_MULTI_PASS        GST_NV_ENCODER_MULTI_PASS_DEFAULT
#define DEFAULT_WEIGHTED_PRED     FALSE
#define DEFAULT_GOP_SIZE          30
#define DEFAULT_B_FRAMES          0
#define DEFAULT_RATE_CONTROL      GST_NV_ENCODER_RC_MODE_VBR
#define DEFAULT_QP                -1
#define DEFAULT_BITRATE           0
#define DEFAULT_MAX_BITRATE       0
#define DEFAULT_VBV_BUFFER_SIZE   0
#define DEFAULT_RC_LOOKAHEAD      0
#define DEFAULT_I_ADAPT           FALSE
#define DEFAULT_B_ADAPT           FALSE
#define DEFAULT_SPATIAL_AQ        FALSE
#define DEFAULT_TEMPORAL_AQ       FALSE
#define DEFAULT_ZERO_REORDER_DELAY FALSE
#define DEFAULT_NON_REF_P         FALSE
#define DEFAULT_STRICT_GOP        FALSE
#define DEFAULT_AQ_STRENGTH       FALSE
#define DEFAULT_CONST_QUALITY     0
#define DEFAULT_AUD               TRUE
#define DEFAULT_REPEAT_SEQUENCE_HEADER FALSE

typedef struct _GstNvH264Encoder
{
  GstNvEncoder parent;
  GMutex prop_lock;

  gboolean init_param_updated;
  gboolean rc_param_updated;
  gboolean bitrate_updated;

  gboolean packetized;
  GstH264NalParser *parser;
  GstMemory *sei;
  GArray *sei_array;

  GstNvEncoderDeviceMode selected_device_mode;

  /* Properties */
  guint cuda_device_id;
  gint64 adapter_luid;

  GstNvEncoderPreset preset;
  GstNvEncoderMultiPass multipass;
  GstNvEncoderTune tune;
  gboolean weighted_pred;

  gint gop_size;
  guint bframes;

  GstNvEncoderRCMode rc_mode;
  gint qp_i;
  gint qp_p;
  gint qp_b;
  guint bitrate;
  guint max_bitrate;
  guint vbv_buffer_size;
  guint rc_lookahead;
  gboolean i_adapt;
  gboolean b_adapt;
  gboolean spatial_aq;
  gboolean temporal_aq;
  gboolean zero_reorder_delay;
  gboolean non_ref_p;
  gboolean strict_gop;
  guint aq_strength;
  gint min_qp_i;
  gint min_qp_p;
  gint min_qp_b;
  gint max_qp_i;
  gint max_qp_p;
  gint max_qp_b;
  gdouble const_quality;

  gboolean aud;
  gboolean cabac;
  gboolean repeat_sequence_header;
} GstNvH264Encoder;

typedef struct _GstNvH264EncoderClass
{
  GstNvEncoderClass parent_class;

  guint cuda_device_id;
  gint64 adapter_luid;

  GstNvEncoderDeviceMode device_mode;

  /* representative device caps */
  GstNvEncoderDeviceCaps device_caps;

  /* auto gpu select mode */
  guint cuda_device_id_size;
  guint cuda_device_id_list[8];

  guint adapter_luid_size;
  gint64 adapter_luid_list[8];
} GstNvH264EncoderClass;

#define GST_NV_H264_ENCODER(object) ((GstNvH264Encoder *) (object))
#define GST_NV_H264_ENCODER_GET_CLASS(object) \
    (G_TYPE_INSTANCE_GET_CLASS ((object),G_TYPE_FROM_INSTANCE (object),GstNvH264EncoderClass))

static void gst_nv_h264_encoder_finalize (GObject * object);
static void gst_nv_h264_encoder_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_nv_h264_encoder_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static GstCaps *gst_nv_h264_encoder_getcaps (GstVideoEncoder * encoder,
    GstCaps * filter);
static gboolean gst_nv_h264_encoder_stop (GstVideoEncoder * encoder);
static gboolean gst_nv_h264_encoder_set_format (GstNvEncoder * encoder,
    GstVideoCodecState * state, gpointer session,
    NV_ENC_INITIALIZE_PARAMS * init_params, NV_ENC_CONFIG * config);
static gboolean gst_nv_h264_encoder_set_output_state (GstNvEncoder * encoder,
    GstVideoCodecState * state, gpointer session);
static GstBuffer *gst_nv_h264_encoder_create_output_buffer (GstNvEncoder *
    encoder, NV_ENC_LOCK_BITSTREAM * bitstream);
static GstNvEncoderReconfigure
gst_nv_h264_encoder_check_reconfigure (GstNvEncoder * encoder,
    NV_ENC_CONFIG * config);
static gboolean gst_nv_h264_encoder_select_device (GstNvEncoder * encoder,
    const GstVideoInfo * info, GstBuffer * buffer,
    GstNvEncoderDeviceData * data);
static guint gst_nv_h264_encoder_calculate_min_buffers (GstNvEncoder * encoder);

static void
gst_nv_h264_encoder_class_init (GstNvH264EncoderClass * klass, gpointer data)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstVideoEncoderClass *videoenc_class = GST_VIDEO_ENCODER_CLASS (klass);
  GstNvEncoderClass *nvenc_class = GST_NV_ENCODER_CLASS (klass);
  GstNvEncoderClassData *cdata = (GstNvEncoderClassData *) data;
  GstNvEncoderDeviceCaps *dev_caps = &cdata->device_caps;
  GParamFlags param_flags = (GParamFlags) (G_PARAM_READWRITE |
      GST_PARAM_MUTABLE_PLAYING | G_PARAM_STATIC_STRINGS);
  GParamFlags conditional_param_flags = (GParamFlags) (G_PARAM_READWRITE |
      GST_PARAM_CONDITIONALLY_AVAILABLE | GST_PARAM_MUTABLE_PLAYING |
      G_PARAM_STATIC_STRINGS);

  parent_class = (GTypeClass *) g_type_class_peek_parent (klass);

  object_class->finalize = gst_nv_h264_encoder_finalize;
  object_class->set_property = gst_nv_h264_encoder_set_property;
  object_class->get_property = gst_nv_h264_encoder_get_property;

  switch (cdata->device_mode) {
    case GST_NV_ENCODER_DEVICE_CUDA:
      g_object_class_install_property (object_class, PROP_CUDA_DEVICE_ID,
          g_param_spec_uint ("cuda-device-id", "CUDA Device ID",
              "CUDA device ID of associated GPU",
              0, G_MAXINT, 0,
              (GParamFlags) (GST_PARAM_DOC_SHOW_DEFAULT |
                  G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)));
      break;
    case GST_NV_ENCODER_DEVICE_D3D11:
      g_object_class_install_property (object_class, PROP_ADAPTER_LUID,
          g_param_spec_int64 ("adapter-luid", "Adapter LUID",
              "DXGI Adapter LUID (Locally Unique Identifier) of associated GPU",
              G_MININT64, G_MAXINT64, 0,
              (GParamFlags) (GST_PARAM_DOC_SHOW_DEFAULT |
                  G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)));
      break;
    case GST_NV_ENCODER_DEVICE_AUTO_SELECT:
      if (cdata->cuda_device_id_size > 0) {
        g_object_class_install_property (object_class, PROP_CUDA_DEVICE_ID,
            g_param_spec_uint ("cuda-device-id", "CUDA Device ID",
                "CUDA device ID to use",
                0, G_MAXINT, 0,
                (GParamFlags) (conditional_param_flags |
                    GST_PARAM_DOC_SHOW_DEFAULT)));
      }
      if (cdata->adapter_luid_size > 0) {
        g_object_class_install_property (object_class, PROP_ADAPTER_LUID,
            g_param_spec_int64 ("adapter-luid", "Adapter LUID",
                "DXGI Adapter LUID (Locally Unique Identifier) to use",
                G_MININT64, G_MAXINT64, 0,
                (GParamFlags) (conditional_param_flags |
                    GST_PARAM_DOC_SHOW_DEFAULT)));
      }
      break;
    default:
      g_assert_not_reached ();
      break;
  }

  g_object_class_install_property (object_class, PROP_PRESET,
      g_param_spec_enum ("preset", "Encoding Preset",
          "Encoding Preset", GST_TYPE_NV_ENCODER_PRESET,
          DEFAULT_PRESET, param_flags));

  /**
   * GstNvCudaH264Enc:tune:
   *
   * Since: 1.24
   */

  /**
   * GstNvD3D11H264Enc:tune:
   *
   * Since: 1.24
   */

  /**
   * GstNvAutoGpuH264Enc:tune:
   *
   * Since: 1.24
   */
  g_object_class_install_property (object_class, PROP_TUNE,
      g_param_spec_enum ("tune", "Tune",
          "Encoding tune", GST_TYPE_NV_ENCODER_TUNE,
          DEFAULT_TUNE, param_flags));

  /**
   * GstNvCudaH264Enc:multi-pass:
   *
   * Since: 1.24
   */

  /**
   * GstNvD3D11H264Enc:multi-pass:
   *
   * Since: 1.24
   */

  /**
   * GstNvAutoGpuH264Enc:multi-pass:
   *
   * Since: 1.24
   */
  g_object_class_install_property (object_class, PROP_MULTI_PASS,
      g_param_spec_enum ("multi-pass", "Multi Pass",
          "Multi pass encoding", GST_TYPE_NV_ENCODER_MULTI_PASS,
          DEFAULT_MULTI_PASS, param_flags));
  if (dev_caps->weighted_prediction) {
    g_object_class_install_property (object_class, PROP_WEIGHTED_PRED,
        g_param_spec_boolean ("weighted-pred", "Weighted Pred",
            "Enables Weighted Prediction", DEFAULT_WEIGHTED_PRED,
            conditional_param_flags));
  }
  g_object_class_install_property (object_class, PROP_GOP_SIZE,
      g_param_spec_int ("gop-size", "GOP size",
          "Number of frames between intra frames (-1 = infinite)",
          -1, G_MAXINT, DEFAULT_GOP_SIZE, param_flags));
  if (dev_caps->max_bframes > 0) {
    g_object_class_install_property (object_class, PROP_B_FRAMES,
        g_param_spec_uint ("b-frames", "B-Frames",
            "Number of B-frames between I and P", 0, dev_caps->max_bframes,
            DEFAULT_B_FRAMES, conditional_param_flags));
  }
  g_object_class_install_property (object_class, PROP_RATE_CONTROL,
      g_param_spec_enum ("rate-control", "Rate Control", "Rate Control Method",
          GST_TYPE_NV_ENCODER_RC_MODE, DEFAULT_RATE_CONTROL, param_flags));
  g_object_class_install_property (object_class, PROP_QP_I,
      g_param_spec_int ("qp-i", "QP I",
          "Constant QP value for I frame (-1 = default)", -1, 51,
          DEFAULT_QP, param_flags));
  g_object_class_install_property (object_class, PROP_QP_P,
      g_param_spec_int ("qp-p", "QP P",
          "Constant QP value for P frame (-1 = default)", -1, 51,
          DEFAULT_QP, param_flags));
  g_object_class_install_property (object_class, PROP_QP_B,
      g_param_spec_int ("qp-b", "QP B",
          "Constant QP value for B frame (-1 = default)", -1, 51,
          DEFAULT_QP, param_flags));
  g_object_class_install_property (object_class, PROP_BITRATE,
      g_param_spec_uint ("bitrate", "Bitrate",
          "Bitrate in kbit/sec (0 = automatic)", 0, 2000 * 1024,
          DEFAULT_BITRATE, param_flags));
  g_object_class_install_property (object_class, PROP_MAX_BITRATE,
      g_param_spec_uint ("max-bitrate", "Max Bitrate",
          "Maximum Bitrate in kbit/sec (ignored in CBR mode)", 0, 2000 * 1024,
          DEFAULT_MAX_BITRATE, param_flags));
  if (dev_caps->custom_vbv_buf_size) {
    g_object_class_install_property (object_class,
        PROP_VBV_BUFFER_SIZE,
        g_param_spec_uint ("vbv-buffer-size", "VBV Buffer Size",
            "VBV(HRD) Buffer Size in kbits (0 = NVENC default)",
            0, G_MAXUINT, DEFAULT_VBV_BUFFER_SIZE, conditional_param_flags));
  }
  if (dev_caps->lookahead) {
    g_object_class_install_property (object_class, PROP_RC_LOOKAHEAD,
        g_param_spec_uint ("rc-lookahead", "Rate Control Lookahead",
            "Number of frames for frame type lookahead",
            0, 32, DEFAULT_RC_LOOKAHEAD, conditional_param_flags));
    g_object_class_install_property (object_class, PROP_I_ADAPT,
        g_param_spec_boolean ("i-adapt", "I Adapt",
            "Enable adaptive I-frame insert when lookahead is enabled",
            DEFAULT_I_ADAPT, conditional_param_flags));
    if (dev_caps->max_bframes > 0) {
      g_object_class_install_property (object_class, PROP_B_ADAPT,
          g_param_spec_boolean ("b-adapt", "B Adapt",
              "Enable adaptive B-frame insert when lookahead is enabled",
              DEFAULT_B_ADAPT, conditional_param_flags));
    }
  }
  g_object_class_install_property (object_class, PROP_SPATIAL_AQ,
      g_param_spec_boolean ("spatial-aq", "Spatial AQ",
          "Spatial Adaptive Quantization", DEFAULT_SPATIAL_AQ, param_flags));
  if (dev_caps->temporal_aq) {
    g_object_class_install_property (object_class, PROP_TEMPORAL_AQ,
        g_param_spec_boolean ("temporal-aq", "Temporal AQ",
            "Temporal Adaptive Quantization", DEFAULT_TEMPORAL_AQ,
            conditional_param_flags));
  }
  g_object_class_install_property (object_class, PROP_ZERO_REORDER_DELAY,
      g_param_spec_boolean ("zero-reorder-delay", "Zero Reorder Delay",
          "Zero latency operation (i.e., num_reorder_frames = 0)",
          DEFAULT_ZERO_REORDER_DELAY, param_flags));
  g_object_class_install_property (object_class, PROP_NON_REF_P,
      g_param_spec_boolean ("nonref-p", "Nonref P",
          "Automatic insertion of non-reference P-frames", DEFAULT_NON_REF_P,
          param_flags));
  g_object_class_install_property (object_class, PROP_STRICT_GOP,
      g_param_spec_boolean ("strict-gop", "Strict GOP",
          "Minimize GOP-to-GOP rate fluctuations", DEFAULT_STRICT_GOP,
          param_flags));
  g_object_class_install_property (object_class, PROP_AQ_STRENGTH,
      g_param_spec_uint ("aq-strength", "AQ Strength",
          "Adaptive Quantization Strength when spatial-aq is enabled"
          " from 1 (low) to 15 (aggressive), (0 = autoselect)",
          0, 15, DEFAULT_AQ_STRENGTH, param_flags));
  g_object_class_install_property (object_class, PROP_MIN_QP_I,
      g_param_spec_int ("min-qp-i", "Min QP I",
          "Minimum QP value for I frame, (-1 = disabled)", -1, 51,
          DEFAULT_QP, param_flags));
  g_object_class_install_property (object_class, PROP_MIN_QP_P,
      g_param_spec_int ("min-qp-p", "Min QP P",
          "Minimum QP value for P frame, (-1 = automatic)", -1, 51,
          DEFAULT_QP, param_flags));
  g_object_class_install_property (object_class, PROP_MIN_QP_B,
      g_param_spec_int ("min-qp-b", "Min QP B",
          "Minimum QP value for B frame, (-1 = automatic)", -1, 51,
          DEFAULT_QP, param_flags));
  g_object_class_install_property (object_class, PROP_MAX_QP_I,
      g_param_spec_int ("max-qp-i", "Max QP I",
          "Maximum QP value for I frame, (-1 = disabled)", -1, 51,
          DEFAULT_QP, param_flags));
  g_object_class_install_property (object_class, PROP_MAX_QP_P,
      g_param_spec_int ("max-qp-p", "Max QP P",
          "Maximum QP value for P frame, (-1 = automatic)", -1, 51,
          DEFAULT_QP, param_flags));
  g_object_class_install_property (object_class, PROP_MAX_QP_B,
      g_param_spec_int ("max-qp-b", "Max QP B",
          "Maximum QP value for B frame, (-1 = automatic)", -1, 51,
          DEFAULT_QP, param_flags));
  g_object_class_install_property (object_class, PROP_CONST_QUALITY,
      g_param_spec_double ("const-quality", "Constant Quality",
          "Target Constant Quality level for VBR mode (0 = automatic)",
          0, 51, DEFAULT_CONST_QUALITY, param_flags));
  g_object_class_install_property (object_class, PROP_AUD,
      g_param_spec_boolean ("aud", "AUD",
          "Use AU (Access Unit) delimiter", DEFAULT_AUD, param_flags));
  if (dev_caps->cabac) {
    g_object_class_install_property (object_class, PROP_CABAC,
        g_param_spec_boolean ("cabac", "CABAC",
            "Enable CABAC entropy coding", TRUE, conditional_param_flags));
  }
  g_object_class_install_property (object_class, PROP_REPEAT_SEQUENCE_HEADER,
      g_param_spec_boolean ("repeat-sequence-header", "Repeat Sequence Header",
          "Insert sequence headers (SPS/PPS) per IDR",
          DEFAULT_REPEAT_SEQUENCE_HEADER, param_flags));

  switch (cdata->device_mode) {
    case GST_NV_ENCODER_DEVICE_CUDA:
      gst_element_class_set_static_metadata (element_class,
          "NVENC H.264 Video Encoder CUDA Mode",
          "Codec/Encoder/Video/Hardware",
          "Encode H.264 video streams using NVCODEC API CUDA Mode",
          "Seungha Yang <seungha@centricular.com>");
      break;
    case GST_NV_ENCODER_DEVICE_D3D11:
      gst_element_class_set_static_metadata (element_class,
          "NVENC H.264 Video Encoder Direct3D11 Mode",
          "Codec/Encoder/Video/Hardware",
          "Encode H.264 video streams using NVCODEC API Direct3D11 Mode",
          "Seungha Yang <seungha@centricular.com>");
      break;
    case GST_NV_ENCODER_DEVICE_AUTO_SELECT:
      gst_element_class_set_static_metadata (element_class,
          "NVENC H.264 Video Encoder Auto GPU select Mode",
          "Codec/Encoder/Video/Hardware",
          "Encode H.264 video streams using NVCODEC API auto GPU select Mode",
          "Seungha Yang <seungha@centricular.com>");
      break;
    default:
      g_assert_not_reached ();
      break;
  }

  gst_element_class_add_pad_template (element_class,
      gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
          cdata->sink_caps));
  gst_element_class_add_pad_template (element_class,
      gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
          cdata->src_caps));

  videoenc_class->getcaps = GST_DEBUG_FUNCPTR (gst_nv_h264_encoder_getcaps);
  videoenc_class->stop = GST_DEBUG_FUNCPTR (gst_nv_h264_encoder_stop);

  nvenc_class->set_format = GST_DEBUG_FUNCPTR (gst_nv_h264_encoder_set_format);
  nvenc_class->set_output_state =
      GST_DEBUG_FUNCPTR (gst_nv_h264_encoder_set_output_state);
  nvenc_class->create_output_buffer =
      GST_DEBUG_FUNCPTR (gst_nv_h264_encoder_create_output_buffer);
  nvenc_class->check_reconfigure =
      GST_DEBUG_FUNCPTR (gst_nv_h264_encoder_check_reconfigure);
  nvenc_class->select_device =
      GST_DEBUG_FUNCPTR (gst_nv_h264_encoder_select_device);
  nvenc_class->calculate_min_buffers =
      GST_DEBUG_FUNCPTR (gst_nv_h264_encoder_calculate_min_buffers);

  klass->device_caps = cdata->device_caps;
  klass->cuda_device_id = cdata->cuda_device_id;
  klass->adapter_luid = cdata->adapter_luid;
  klass->device_mode = cdata->device_mode;
  klass->cuda_device_id_size = cdata->cuda_device_id_size;
  klass->adapter_luid_size = cdata->adapter_luid_size;
  memcpy (klass->cuda_device_id_list, cdata->cuda_device_id_list,
      sizeof (klass->cuda_device_id_list));
  memcpy (klass->adapter_luid_list, cdata->adapter_luid_list,
      sizeof (klass->adapter_luid_list));

  gst_nv_encoder_class_data_unref (cdata);
}

static void
gst_nv_h264_encoder_init (GstNvH264Encoder * self)
{
  GstNvH264EncoderClass *klass = GST_NV_H264_ENCODER_GET_CLASS (self);

  g_mutex_init (&self->prop_lock);

  self->selected_device_mode = klass->device_mode;
  self->cuda_device_id = klass->cuda_device_id;
  self->adapter_luid = klass->adapter_luid;
  self->preset = DEFAULT_PRESET;
  self->tune = DEFAULT_TUNE;
  self->multipass = DEFAULT_MULTI_PASS;
  self->weighted_pred = DEFAULT_WEIGHTED_PRED;
  self->gop_size = DEFAULT_GOP_SIZE;
  self->bframes = DEFAULT_B_FRAMES;
  self->rc_mode = DEFAULT_RATE_CONTROL;
  self->qp_i = DEFAULT_QP;
  self->qp_p = DEFAULT_QP;
  self->qp_b = DEFAULT_QP;
  self->bitrate = DEFAULT_BITRATE;
  self->max_bitrate = DEFAULT_MAX_BITRATE;
  self->vbv_buffer_size = DEFAULT_VBV_BUFFER_SIZE;
  self->rc_lookahead = DEFAULT_RC_LOOKAHEAD;
  self->i_adapt = DEFAULT_I_ADAPT;
  self->b_adapt = DEFAULT_B_ADAPT;
  self->spatial_aq = DEFAULT_SPATIAL_AQ;
  self->temporal_aq = DEFAULT_TEMPORAL_AQ;
  self->zero_reorder_delay = DEFAULT_ZERO_REORDER_DELAY;
  self->non_ref_p = DEFAULT_NON_REF_P;
  self->strict_gop = DEFAULT_STRICT_GOP;
  self->aq_strength = DEFAULT_AQ_STRENGTH;
  self->min_qp_i = DEFAULT_QP;
  self->min_qp_p = DEFAULT_QP;
  self->min_qp_b = DEFAULT_QP;
  self->max_qp_i = DEFAULT_QP;
  self->max_qp_p = DEFAULT_QP;
  self->max_qp_b = DEFAULT_QP;
  self->const_quality = DEFAULT_CONST_QUALITY;
  self->aud = DEFAULT_AUD;
  if (klass->device_caps.cabac)
    self->cabac = TRUE;
  self->repeat_sequence_header = DEFAULT_REPEAT_SEQUENCE_HEADER;

  self->parser = gst_h264_nal_parser_new ();
  self->sei_array = g_array_new (FALSE, FALSE, sizeof (GstH264SEIMessage));

  gst_nv_encoder_set_device_mode (GST_NV_ENCODER (self), klass->device_mode,
      klass->cuda_device_id, klass->adapter_luid);
}

static void
gst_nv_h264_encoder_finalize (GObject * object)
{
  GstNvH264Encoder *self = GST_NV_H264_ENCODER (object);

  g_mutex_clear (&self->prop_lock);
  gst_h264_nal_parser_free (self->parser);
  g_array_unref (self->sei_array);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

typedef enum
{
  UPDATE_INIT_PARAM,
  UPDATE_RC_PARAM,
  UPDATE_BITRATE,
} PropUpdateLevel;

static void
update_boolean (GstNvH264Encoder * self, gboolean * old_val,
    const GValue * new_val, PropUpdateLevel level)
{
  gboolean val = g_value_get_boolean (new_val);

  if (*old_val == val)
    return;

  *old_val = val;
  switch (level) {
    case UPDATE_INIT_PARAM:
      self->init_param_updated = TRUE;
      break;
    case UPDATE_RC_PARAM:
      self->rc_param_updated = TRUE;
      break;
    case UPDATE_BITRATE:
      self->bitrate_updated = TRUE;
      break;
  }
}

static void
update_int (GstNvH264Encoder * self, gint * old_val,
    const GValue * new_val, PropUpdateLevel level)
{
  gint val = g_value_get_int (new_val);

  if (*old_val == val)
    return;

  *old_val = val;
  switch (level) {
    case UPDATE_INIT_PARAM:
      self->init_param_updated = TRUE;
      break;
    case UPDATE_RC_PARAM:
      self->rc_param_updated = TRUE;
      break;
    case UPDATE_BITRATE:
      self->bitrate_updated = TRUE;
      break;
  }
}

static void
update_uint (GstNvH264Encoder * self, guint * old_val,
    const GValue * new_val, PropUpdateLevel level)
{
  guint val = g_value_get_uint (new_val);

  if (*old_val == val)
    return;

  *old_val = val;
  switch (level) {
    case UPDATE_INIT_PARAM:
      self->init_param_updated = TRUE;
      break;
    case UPDATE_RC_PARAM:
      self->rc_param_updated = TRUE;
      break;
    case UPDATE_BITRATE:
      self->bitrate_updated = TRUE;
      break;
  }
}

static void
update_double (GstNvH264Encoder * self, gdouble * old_val,
    const GValue * new_val, PropUpdateLevel level)
{
  gdouble val = g_value_get_double (new_val);

  if (*old_val == val)
    return;

  *old_val = val;
  switch (level) {
    case UPDATE_INIT_PARAM:
      self->init_param_updated = TRUE;
      break;
    case UPDATE_RC_PARAM:
      self->rc_param_updated = TRUE;
      break;
    case UPDATE_BITRATE:
      self->bitrate_updated = TRUE;
      break;
  }
}

static void
gst_nv_h264_encoder_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstNvH264Encoder *self = GST_NV_H264_ENCODER (object);
  GstNvH264EncoderClass *klass = GST_NV_H264_ENCODER_GET_CLASS (self);

  g_mutex_lock (&self->prop_lock);
  switch (prop_id) {
    case PROP_ADAPTER_LUID:{
      gint64 adapter_luid = g_value_get_int64 (value);
      gboolean is_valid = FALSE;

      for (guint i = 0; i < klass->adapter_luid_size; i++) {
        if (klass->adapter_luid_list[i] == adapter_luid) {
          self->adapter_luid = adapter_luid;
          is_valid = TRUE;
          break;
        }
      }

      if (!is_valid)
        g_warning ("%" G_GINT64_FORMAT " is not a valid adapter luid",
            adapter_luid);
      break;
    }
    case PROP_CUDA_DEVICE_ID:{
      guint cuda_device_id = g_value_get_uint (value);
      gboolean is_valid = FALSE;

      for (guint i = 0; i < klass->cuda_device_id_size; i++) {
        if (klass->cuda_device_id_list[i] == cuda_device_id) {
          self->cuda_device_id = cuda_device_id;
          is_valid = TRUE;
          break;
        }
      }

      if (!is_valid)
        g_warning ("%d is not a valid cuda device id", cuda_device_id);
      break;
    }
    case PROP_PRESET:{
      GstNvEncoderPreset preset = (GstNvEncoderPreset) g_value_get_enum (value);
      if (preset != self->preset) {
        self->preset = preset;
        self->init_param_updated = TRUE;
      }
      break;
    }
    case PROP_TUNE:{
      GstNvEncoderTune tune = (GstNvEncoderTune) g_value_get_enum (value);
      if (tune != self->tune) {
        self->tune = tune;
        self->init_param_updated = TRUE;
      }
      break;
    }
    case PROP_MULTI_PASS:{
      GstNvEncoderMultiPass multipass =
          (GstNvEncoderMultiPass) g_value_get_enum (value);
      if (multipass != self->multipass) {
        self->multipass = multipass;
        self->init_param_updated = TRUE;
      }
      break;
    }
    case PROP_WEIGHTED_PRED:
      update_boolean (self, &self->weighted_pred, value, UPDATE_INIT_PARAM);
      break;
    case PROP_GOP_SIZE:
      update_int (self, &self->gop_size, value, UPDATE_INIT_PARAM);
      break;
    case PROP_B_FRAMES:
      update_uint (self, &self->bframes, value, UPDATE_INIT_PARAM);
      break;
    case PROP_RATE_CONTROL:{
      GstNvEncoderRCMode mode = (GstNvEncoderRCMode) g_value_get_enum (value);
      if (mode != self->rc_mode) {
        self->rc_mode = mode;
        self->rc_param_updated = TRUE;
      }
      break;
    }
    case PROP_QP_I:
      update_int (self, &self->qp_i, value, UPDATE_RC_PARAM);
      break;
    case PROP_QP_P:
      update_int (self, &self->qp_p, value, UPDATE_RC_PARAM);
      break;
    case PROP_QP_B:
      update_int (self, &self->qp_b, value, UPDATE_RC_PARAM);
      break;
    case PROP_BITRATE:
      update_uint (self, &self->bitrate, value, UPDATE_BITRATE);
      break;
    case PROP_MAX_BITRATE:
      update_uint (self, &self->max_bitrate, value, UPDATE_BITRATE);
      break;
    case PROP_VBV_BUFFER_SIZE:
      update_uint (self, &self->vbv_buffer_size, value, UPDATE_RC_PARAM);
      break;
    case PROP_RC_LOOKAHEAD:
      /* rc-lookahead update requires pool size change */
      update_uint (self, &self->rc_lookahead, value, UPDATE_INIT_PARAM);
      break;
    case PROP_I_ADAPT:
      update_boolean (self, &self->i_adapt, value, UPDATE_RC_PARAM);
      break;
    case PROP_B_ADAPT:
      update_boolean (self, &self->b_adapt, value, UPDATE_RC_PARAM);
      break;
    case PROP_SPATIAL_AQ:
      update_boolean (self, &self->spatial_aq, value, UPDATE_RC_PARAM);
      break;
    case PROP_TEMPORAL_AQ:
      update_boolean (self, &self->temporal_aq, value, UPDATE_RC_PARAM);
      break;
    case PROP_ZERO_REORDER_DELAY:
      update_boolean (self, &self->zero_reorder_delay, value, UPDATE_RC_PARAM);
      break;
    case PROP_NON_REF_P:
      update_boolean (self, &self->non_ref_p, value, UPDATE_RC_PARAM);
      break;
    case PROP_STRICT_GOP:
      update_boolean (self, &self->strict_gop, value, UPDATE_RC_PARAM);
      break;
    case PROP_AQ_STRENGTH:
      update_uint (self, &self->aq_strength, value, UPDATE_RC_PARAM);
      break;
    case PROP_MIN_QP_I:
      update_int (self, &self->min_qp_i, value, UPDATE_RC_PARAM);
      break;
    case PROP_MIN_QP_P:
      update_int (self, &self->min_qp_p, value, UPDATE_RC_PARAM);
      break;
    case PROP_MIN_QP_B:
      update_int (self, &self->min_qp_b, value, UPDATE_RC_PARAM);
      break;
    case PROP_MAX_QP_I:
      update_int (self, &self->max_qp_i, value, UPDATE_RC_PARAM);
      break;
    case PROP_MAX_QP_P:
      update_int (self, &self->max_qp_p, value, UPDATE_RC_PARAM);
      break;
    case PROP_MAX_QP_B:
      update_int (self, &self->max_qp_b, value, UPDATE_RC_PARAM);
      break;
    case PROP_CONST_QUALITY:
      update_double (self, &self->const_quality, value, UPDATE_RC_PARAM);
      break;
    case PROP_AUD:
      update_boolean (self, &self->aud, value, UPDATE_INIT_PARAM);
      break;
    case PROP_CABAC:
      update_boolean (self, &self->cabac, value, UPDATE_INIT_PARAM);
      break;
    case PROP_REPEAT_SEQUENCE_HEADER:
      update_boolean (self,
          &self->repeat_sequence_header, value, UPDATE_INIT_PARAM);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  g_mutex_unlock (&self->prop_lock);
}

static void
gst_nv_h264_encoder_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstNvH264Encoder *self = GST_NV_H264_ENCODER (object);

  switch (prop_id) {
    case PROP_ADAPTER_LUID:
      g_value_set_int64 (value, self->adapter_luid);
      break;
    case PROP_CUDA_DEVICE_ID:
      g_value_set_uint (value, self->cuda_device_id);
      break;
    case PROP_PRESET:
      g_value_set_enum (value, self->preset);
      break;
    case PROP_TUNE:
      g_value_set_enum (value, self->tune);
      break;
    case PROP_MULTI_PASS:
      g_value_set_enum (value, self->multipass);
      break;
    case PROP_WEIGHTED_PRED:
      g_value_set_boolean (value, self->weighted_pred);
      break;
    case PROP_GOP_SIZE:
      g_value_set_int (value, self->gop_size);
      break;
    case PROP_B_FRAMES:
      g_value_set_uint (value, self->bframes);
      break;
    case PROP_RATE_CONTROL:
      g_value_set_enum (value, self->rc_mode);
      break;
    case PROP_QP_I:
      g_value_set_int (value, self->qp_i);
      break;
    case PROP_QP_P:
      g_value_set_int (value, self->qp_p);
      break;
    case PROP_QP_B:
      g_value_set_int (value, self->qp_b);
      break;
    case PROP_BITRATE:
      g_value_set_uint (value, self->bitrate);
      break;
    case PROP_MAX_BITRATE:
      g_value_set_uint (value, self->max_bitrate);
      break;
    case PROP_VBV_BUFFER_SIZE:
      g_value_set_uint (value, self->vbv_buffer_size);
      break;
    case PROP_RC_LOOKAHEAD:
      g_value_set_uint (value, self->rc_lookahead);
      break;
    case PROP_I_ADAPT:
      g_value_set_boolean (value, self->i_adapt);
      break;
    case PROP_B_ADAPT:
      g_value_set_boolean (value, self->b_adapt);
      break;
    case PROP_SPATIAL_AQ:
      g_value_set_boolean (value, self->spatial_aq);
      break;
    case PROP_TEMPORAL_AQ:
      g_value_set_boolean (value, self->temporal_aq);
      break;
    case PROP_ZERO_REORDER_DELAY:
      g_value_set_boolean (value, self->zero_reorder_delay);
      break;
    case PROP_NON_REF_P:
      g_value_set_boolean (value, self->non_ref_p);
      break;
    case PROP_STRICT_GOP:
      g_value_set_boolean (value, self->strict_gop);
      break;
    case PROP_AQ_STRENGTH:
      g_value_set_uint (value, self->aq_strength);
      break;
    case PROP_MIN_QP_I:
      g_value_set_int (value, self->min_qp_i);
      break;
    case PROP_MIN_QP_P:
      g_value_set_int (value, self->min_qp_p);
      break;
    case PROP_MIN_QP_B:
      g_value_set_int (value, self->min_qp_b);
      break;
    case PROP_MAX_QP_I:
      g_value_set_int (value, self->max_qp_i);
      break;
    case PROP_MAX_QP_P:
      g_value_set_int (value, self->max_qp_p);
      break;
    case PROP_MAX_QP_B:
      g_value_set_int (value, self->max_qp_b);
      break;
    case PROP_CONST_QUALITY:
      g_value_set_double (value, self->const_quality);
      break;
    case PROP_AUD:
      g_value_set_boolean (value, self->aud);
      break;
    case PROP_CABAC:
      g_value_set_boolean (value, self->cabac);
      break;
    case PROP_REPEAT_SEQUENCE_HEADER:
      g_value_set_boolean (value, self->repeat_sequence_header);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_nv_h264_encoder_get_downstream_profiles_and_format (GstNvH264Encoder * self,
    std::set < std::string > &downstream_profiles, gboolean * packetized)
{
  GstCaps *allowed_caps;
  GstStructure *s;
  const gchar *stream_format;

  allowed_caps = gst_pad_get_allowed_caps (GST_VIDEO_ENCODER_SRC_PAD (self));

  if (!allowed_caps || gst_caps_is_empty (allowed_caps) ||
      gst_caps_is_any (allowed_caps)) {
    gst_clear_caps (&allowed_caps);

    return;
  }

  for (guint i = 0; i < gst_caps_get_size (allowed_caps); i++) {
    const GValue *profile_value;
    const gchar *profile;

    s = gst_caps_get_structure (allowed_caps, i);
    profile_value = gst_structure_get_value (s, "profile");
    if (!profile_value)
      continue;

    if (GST_VALUE_HOLDS_LIST (profile_value)) {
      for (guint j = 0; j < gst_value_list_get_size (profile_value); j++) {
        const GValue *p = gst_value_list_get_value (profile_value, j);

        if (!G_VALUE_HOLDS_STRING (p))
          continue;

        profile = g_value_get_string (p);
        if (profile)
          downstream_profiles.insert (profile);
      }

    } else if (G_VALUE_HOLDS_STRING (profile_value)) {
      profile = g_value_get_string (profile_value);
      if (profile)
        downstream_profiles.insert (profile);
    }
  }

  if (packetized) {
    *packetized = FALSE;
    allowed_caps = gst_caps_fixate (allowed_caps);
    s = gst_caps_get_structure (allowed_caps, 0);
    stream_format = gst_structure_get_string (s, "stream-format");
    if (g_strcmp0 (stream_format, "avc") == 0)
      *packetized = TRUE;
  }

  gst_caps_unref (allowed_caps);
}

static GstCaps *
gst_nv_h264_encoder_getcaps (GstVideoEncoder * encoder, GstCaps * filter)
{
  GstNvH264Encoder *self = GST_NV_H264_ENCODER (encoder);
  GstNvH264EncoderClass *klass = GST_NV_H264_ENCODER_GET_CLASS (self);
  GstCaps *allowed_caps;
  GstCaps *template_caps;
  GstCaps *filtered_caps;
  GstCaps *supported_caps;
  std::set < std::string > downstream_profiles;
  std::set < std::string > allowed_formats;
  gboolean profile_support_interlaced = FALSE;

  gst_nv_h264_encoder_get_downstream_profiles_and_format (self,
      downstream_profiles, nullptr);

  GST_DEBUG_OBJECT (self, "Downstream specified %" G_GSIZE_FORMAT " profiles",
      downstream_profiles.size ());

  if (downstream_profiles.size () == 0)
    return gst_video_encoder_proxy_getcaps (encoder, nullptr, filter);

  /* *INDENT-OFF* */
  for (const auto &iter: downstream_profiles) {
    if (iter == "high" || iter == "main") {
      profile_support_interlaced = TRUE;
    }

    if (iter == "high-4:4:4") {
      profile_support_interlaced = TRUE;
      allowed_formats.insert("Y444");
    } else {
      allowed_formats.insert("NV12");
    }
  }
  /* *INDENT-ON* */

  GST_DEBUG_OBJECT (self, "Downstream %s support interlaced format",
      profile_support_interlaced ? "can" : "cannot");

  template_caps = gst_pad_get_pad_template_caps (encoder->sinkpad);
  allowed_caps = gst_caps_copy (template_caps);

  if (klass->device_caps.field_encoding == 0 || !profile_support_interlaced) {
    gst_caps_set_simple (allowed_caps, "interlace-mode", G_TYPE_STRING,
        "progressive", nullptr);
  }

  GValue formats = G_VALUE_INIT;

  g_value_init (&formats, GST_TYPE_LIST);
  /* *INDENT-OFF* */
  for (const auto &iter: allowed_formats) {
    GValue val = G_VALUE_INIT;
    g_value_init (&val, G_TYPE_STRING);

    g_value_set_string (&val, iter.c_str());
    gst_value_list_append_and_take_value (&formats, &val);
  }
  /* *INDENT-ON* */

  gst_caps_set_value (allowed_caps, "format", &formats);
  g_value_unset (&formats);

  filtered_caps = gst_caps_intersect_full (template_caps, allowed_caps,
      GST_CAPS_INTERSECT_FIRST);

  supported_caps = gst_video_encoder_proxy_getcaps (encoder,
      filtered_caps, filter);
  gst_caps_unref (filtered_caps);
  gst_caps_unref (allowed_caps);
  gst_caps_unref (template_caps);

  GST_DEBUG_OBJECT (self, "Returning %" GST_PTR_FORMAT, supported_caps);

  return supported_caps;
}

static gboolean
gst_nv_h264_encoder_stop (GstVideoEncoder * encoder)
{
  GstNvH264Encoder *self = GST_NV_H264_ENCODER (encoder);

  if (self->sei) {
    gst_memory_unref (self->sei);
    self->sei = nullptr;
  }

  g_array_set_size (self->sei_array, 0);

  return GST_VIDEO_ENCODER_CLASS (parent_class)->stop (encoder);
}

static gboolean
gst_nv_h264_encoder_set_format (GstNvEncoder * encoder,
    GstVideoCodecState * state, gpointer session,
    NV_ENC_INITIALIZE_PARAMS * init_params, NV_ENC_CONFIG * config)
{
  GstNvH264Encoder *self = GST_NV_H264_ENCODER (encoder);
  GstNvH264EncoderClass *klass = GST_NV_H264_ENCODER_GET_CLASS (self);
  GstNvEncoderDeviceCaps *dev_caps = &klass->device_caps;
  NV_ENC_RC_PARAMS *rc_params;
  GstVideoInfo *info = &state->info;
  NVENCSTATUS status;
  NV_ENC_PRESET_CONFIG preset_config = { 0, };
  gint dar_n, dar_d;
  NV_ENC_CONFIG_H264 *h264_config;
  NV_ENC_CONFIG_H264_VUI_PARAMETERS *vui;
  std::set < std::string > downstream_profiles;
  GUID selected_profile = NV_ENC_CODEC_PROFILE_AUTOSELECT_GUID;
  gboolean downstream_supports_bframe = FALSE;
  gboolean bframe_aborted = FALSE;
  gboolean weight_pred_aborted = FALSE;
  gboolean vbv_buffer_size_aborted = FALSE;
  gboolean lookahead_aborted = FALSE;
  gboolean temporal_aq_aborted = FALSE;

  self->packetized = FALSE;

  gst_nv_h264_encoder_get_downstream_profiles_and_format (self,
      downstream_profiles, &self->packetized);

  if (downstream_profiles.empty ()) {
    GST_ERROR_OBJECT (self, "Unable to get downstream profile");
    return FALSE;
  }

  if (GST_VIDEO_INFO_IS_INTERLACED (info)) {
    downstream_profiles.erase ("progressive-high");
    downstream_profiles.erase ("constrained-high");
    downstream_profiles.erase ("constrained-baseline");
    downstream_profiles.erase ("baseline");

    if (downstream_profiles.empty ()) {
      GST_ERROR_OBJECT (self,
          "None of downstream profile supports interlaced encoding");
      return FALSE;
    }
  }

  if (GST_VIDEO_INFO_FORMAT (info) == GST_VIDEO_FORMAT_Y444) {
    if (downstream_profiles.find ("high-4:4:4") == downstream_profiles.end ()) {
      GST_ERROR_OBJECT (self, "Downstream does not support 4:4:4 profile");
      return FALSE;
    } else {
      selected_profile = NV_ENC_H264_PROFILE_HIGH_444_GUID;
      downstream_supports_bframe = TRUE;
    }
  } else {
    /* *INDENT-OFF* */
    for (const auto &iter: downstream_profiles) {
      if (iter == "high" || iter == "main" || iter == "progressive-high") {
        downstream_supports_bframe = TRUE;
      }
    }
    /* *INDENT-ON* */
  }

  g_mutex_lock (&self->prop_lock);

  if (klass->device_mode == GST_NV_ENCODER_DEVICE_AUTO_SELECT) {
    GstNvEncoderDeviceCaps dev_caps;

    gst_nv_encoder_get_encoder_caps (session,
        &NV_ENC_CODEC_H264_GUID, &dev_caps);

    if (self->bframes > 0 && !dev_caps.max_bframes) {
      self->bframes = 0;
      bframe_aborted = TRUE;

      GST_INFO_OBJECT (self, "B-frame was enabled but not support by device");
    }

    if (self->weighted_pred && !dev_caps.weighted_prediction) {
      self->weighted_pred = FALSE;
      weight_pred_aborted = TRUE;

      GST_INFO_OBJECT (self,
          "Weighted prediction was enabled but not support by device");
    }

    if (self->vbv_buffer_size && !dev_caps.custom_vbv_buf_size) {
      self->vbv_buffer_size = 0;
      vbv_buffer_size_aborted = TRUE;

      GST_INFO_OBJECT (self,
          "VBV buffer size was specified but not supported by device");
    }

    if (self->rc_lookahead && !dev_caps.lookahead) {
      self->rc_lookahead = 0;
      lookahead_aborted = TRUE;

      GST_INFO_OBJECT (self,
          "VBV buffer size was specified but not supported by device");
    }

    if (self->temporal_aq && !dev_caps.temporal_aq) {
      self->temporal_aq = FALSE;
      temporal_aq_aborted = TRUE;

      GST_INFO_OBJECT (self,
          "temporal-aq was enabled but not supported by device");
    }
  }

  init_params->version = gst_nvenc_get_initialize_params_version ();
  init_params->encodeGUID = NV_ENC_CODEC_H264_GUID;

  init_params->encodeWidth = GST_VIDEO_INFO_WIDTH (info);
  init_params->maxEncodeWidth = GST_VIDEO_INFO_WIDTH (info);
  init_params->encodeHeight = GST_VIDEO_INFO_HEIGHT (info);
  init_params->maxEncodeHeight = GST_VIDEO_INFO_HEIGHT (info);
  init_params->enablePTD = TRUE;
  if (dev_caps->async_encoding_support)
    init_params->enableEncodeAsync = 1;
  if (info->fps_d > 0 && info->fps_n > 0) {
    init_params->frameRateNum = info->fps_n;
    init_params->frameRateDen = info->fps_d;
  } else {
    init_params->frameRateNum = 0;
    init_params->frameRateDen = 1;
  }

  init_params->enableWeightedPrediction = self->weighted_pred;

  if (gst_util_fraction_multiply (GST_VIDEO_INFO_WIDTH (info),
          GST_VIDEO_INFO_HEIGHT (info), GST_VIDEO_INFO_PAR_N (info),
          GST_VIDEO_INFO_PAR_D (info), &dar_n, &dar_d) && dar_n > 0
      && dar_d > 0) {
    init_params->darWidth = dar_n;
    init_params->darHeight = dar_d;
  }

  if (GST_VIDEO_INFO_IS_INTERLACED (info) && dev_caps->field_encoding > 0) {
    switch (GST_VIDEO_INFO_INTERLACE_MODE (info)) {
      case GST_VIDEO_INTERLACE_MODE_INTERLEAVED:
      case GST_VIDEO_INTERLACE_MODE_MIXED:
        config->frameFieldMode = NV_ENC_PARAMS_FRAME_FIELD_MODE_FIELD;
        preset_config.presetCfg.frameFieldMode =
            NV_ENC_PARAMS_FRAME_FIELD_MODE_FIELD;
        break;
      default:
        break;
    }
  }

  gst_nv_encoder_preset_to_native (self->preset, self->tune,
      &init_params->presetGUID, &init_params->tuningInfo);

  preset_config.version = gst_nvenc_get_preset_config_version ();
  preset_config.presetCfg.version = gst_nvenc_get_config_version ();

  status = NvEncGetEncodePresetConfigEx (session, NV_ENC_CODEC_H264_GUID,
      init_params->presetGUID, init_params->tuningInfo, &preset_config);
  if (!gst_nv_enc_result (status, self)) {
    GST_ERROR_OBJECT (self, "Failed to get preset config");
    g_mutex_unlock (&self->prop_lock);
    return FALSE;
  }

  *config = preset_config.presetCfg;
  if (self->gop_size < 0) {
    config->gopLength = NVENC_INFINITE_GOPLENGTH;
    config->frameIntervalP = 1;
  } else if (self->gop_size > 0) {
    config->gopLength = self->gop_size;
    /* frameIntervalP
     * 0: All Intra frames
     * 1: I/P only
     * 2: IBP
     * 3: IBBP
     */
    config->frameIntervalP = 1;
    if (self->bframes > 0 && !downstream_supports_bframe) {
      GST_WARNING_OBJECT (self,
          "B-frame was enabled but downstream profile does not support it");
      bframe_aborted = TRUE;
      self->bframes = 0;
    }

    config->frameIntervalP = self->bframes + 1;
  } else {
    /* gop size == 0 means all intra frames */
    config->gopLength = 1;
    config->frameIntervalP = 0;
  }

  rc_params = &config->rcParams;

  if (self->bitrate)
    rc_params->averageBitRate = self->bitrate * 1024;
  if (self->max_bitrate)
    rc_params->maxBitRate = self->max_bitrate * 1024;
  if (self->vbv_buffer_size)
    rc_params->vbvBufferSize = self->vbv_buffer_size * 1024;

  if (self->min_qp_i >= 0) {
    rc_params->enableMinQP = TRUE;
    rc_params->minQP.qpIntra = self->min_qp_i;
    if (self->min_qp_p >= 0) {
      rc_params->minQP.qpInterP = self->min_qp_p;
    } else {
      rc_params->minQP.qpInterP = rc_params->minQP.qpIntra;
    }
    if (self->min_qp_b >= 0) {
      rc_params->minQP.qpInterB = self->min_qp_b;
    } else {
      rc_params->minQP.qpInterB = rc_params->minQP.qpInterP;
    }
  }

  if (self->max_qp_i >= 0) {
    rc_params->enableMaxQP = TRUE;
    rc_params->maxQP.qpIntra = self->max_qp_i;
    if (self->max_qp_p >= 0) {
      rc_params->maxQP.qpInterP = self->max_qp_p;
    } else {
      rc_params->maxQP.qpInterP = rc_params->maxQP.qpIntra;
    }
    if (self->max_qp_b >= 0) {
      rc_params->maxQP.qpInterB = self->max_qp_b;
    } else {
      rc_params->maxQP.qpInterB = rc_params->maxQP.qpInterP;
    }
  }

  gst_nv_encoder_rc_mode_to_native (self->rc_mode, self->multipass,
      &rc_params->rateControlMode, &rc_params->multiPass);

  if (rc_params->rateControlMode == NV_ENC_PARAMS_RC_CONSTQP) {
    if (self->qp_i >= 0)
      rc_params->constQP.qpIntra = self->qp_i;
    if (self->qp_p >= 0)
      rc_params->constQP.qpInterP = self->qp_p;
    if (self->qp_b >= 0)
      rc_params->constQP.qpInterB = self->qp_b;
  }

  if (self->spatial_aq) {
    rc_params->enableAQ = TRUE;
    rc_params->aqStrength = self->aq_strength;
  }

  rc_params->enableTemporalAQ = self->temporal_aq;

  if (self->rc_lookahead) {
    rc_params->enableLookahead = 1;
    rc_params->lookaheadDepth = self->rc_lookahead;
    rc_params->disableIadapt = !self->i_adapt;
    rc_params->disableBadapt = !self->b_adapt;
  }

  rc_params->strictGOPTarget = self->strict_gop;
  rc_params->enableNonRefP = self->non_ref_p;
  rc_params->zeroReorderDelay = self->zero_reorder_delay;

  if (self->const_quality) {
    guint scaled = (gint) (self->const_quality * 256.0);

    rc_params->targetQuality = (guint8) (scaled >> 8);
    rc_params->targetQualityLSB = (guint8) (scaled & 0xff);
  }
  self->init_param_updated = FALSE;
  self->bitrate_updated = FALSE;
  self->rc_param_updated = FALSE;

  if (selected_profile == NV_ENC_CODEC_PROFILE_AUTOSELECT_GUID &&
      config->frameIntervalP > 1) {
    if (downstream_profiles.find ("main") != downstream_profiles.end ()) {
      selected_profile = NV_ENC_H264_PROFILE_MAIN_GUID;
    } else if (downstream_profiles.find ("high") != downstream_profiles.end ()) {
      selected_profile = NV_ENC_H264_PROFILE_HIGH_GUID;
    } else if (downstream_profiles.find ("progressive-high") !=
        downstream_profiles.end ()) {
      selected_profile = NV_ENC_H264_PROFILE_PROGRESSIVE_HIGH_GUID;
    }
  }

  /* Pick the first profile */
  if (selected_profile == NV_ENC_CODEC_PROFILE_AUTOSELECT_GUID) {
    if (*downstream_profiles.begin () == "baseline" ||
        *downstream_profiles.begin () == "constrained-baseline") {
      selected_profile = NV_ENC_H264_PROFILE_BASELINE_GUID;
    } else if (*downstream_profiles.begin () == "main") {
      selected_profile = NV_ENC_H264_PROFILE_MAIN_GUID;
    } else if (*downstream_profiles.begin () == "progressive-high") {
      selected_profile = NV_ENC_H264_PROFILE_PROGRESSIVE_HIGH_GUID;
    } else if (*downstream_profiles.begin () == "constrained-high") {
      selected_profile = NV_ENC_H264_PROFILE_CONSTRAINED_HIGH_GUID;
    }
  }

  config->profileGUID = selected_profile;

  h264_config = &config->encodeCodecConfig.h264Config;
  vui = &h264_config->h264VUIParameters;

  h264_config->level = NV_ENC_LEVEL_AUTOSELECT;
  h264_config->chromaFormatIDC = 1;
  if (selected_profile == NV_ENC_H264_PROFILE_HIGH_444_GUID)
    h264_config->chromaFormatIDC = 3;
  h264_config->idrPeriod = config->gopLength;
  h264_config->outputAUD = self->aud;
  if (self->repeat_sequence_header) {
    h264_config->disableSPSPPS = 0;
    h264_config->repeatSPSPPS = 1;
  } else {
    if (self->packetized)
      h264_config->disableSPSPPS = 1;
    else
      h264_config->disableSPSPPS = 0;
  }

  if (dev_caps->cabac && selected_profile != NV_ENC_H264_PROFILE_BASELINE_GUID) {
    if (self->cabac)
      h264_config->entropyCodingMode = NV_ENC_H264_ENTROPY_CODING_MODE_CABAC;
    else
      h264_config->entropyCodingMode = NV_ENC_H264_ENTROPY_CODING_MODE_CAVLC;
  } else {
    h264_config->entropyCodingMode = NV_ENC_H264_ENTROPY_CODING_MODE_AUTOSELECT;
  }

  vui->videoSignalTypePresentFlag = 1;
  /* Unspecified */
  vui->videoFormat = 5;
  if (info->colorimetry.range == GST_VIDEO_COLOR_RANGE_0_255) {
    vui->videoFullRangeFlag = 1;
  } else {
    vui->videoFullRangeFlag = 0;
  }

  vui->colourDescriptionPresentFlag = 1;
  vui->colourMatrix = gst_video_color_matrix_to_iso (info->colorimetry.matrix);
  vui->colourPrimaries =
      gst_video_color_primaries_to_iso (info->colorimetry.primaries);
  vui->transferCharacteristics =
      gst_video_transfer_function_to_iso (info->colorimetry.transfer);

  g_mutex_unlock (&self->prop_lock);

  if (bframe_aborted)
    g_object_notify (G_OBJECT (self), "b-frames");
  if (weight_pred_aborted)
    g_object_notify (G_OBJECT (self), "weighted-pred");
  if (vbv_buffer_size_aborted)
    g_object_notify (G_OBJECT (self), "vbv-buffer-size");
  if (lookahead_aborted)
    g_object_notify (G_OBJECT (self), "rc-lookahead");
  if (temporal_aq_aborted)
    g_object_notify (G_OBJECT (self), "temporal-aq");

  if (state->mastering_display_info) {
    GstH264SEIMessage sei;
    GstH264MasteringDisplayColourVolume *mdcv;

    memset (&sei, 0, sizeof (GstH264SEIMessage));

    sei.payloadType = GST_H264_SEI_MASTERING_DISPLAY_COLOUR_VOLUME;
    mdcv = &sei.payload.mastering_display_colour_volume;

    /* AVC uses GBR order */
    mdcv->display_primaries_x[0] =
        state->mastering_display_info->display_primaries[1].x;
    mdcv->display_primaries_y[0] =
        state->mastering_display_info->display_primaries[1].y;
    mdcv->display_primaries_x[1] =
        state->mastering_display_info->display_primaries[2].x;
    mdcv->display_primaries_y[1] =
        state->mastering_display_info->display_primaries[2].y;
    mdcv->display_primaries_x[2] =
        state->mastering_display_info->display_primaries[0].x;
    mdcv->display_primaries_y[2] =
        state->mastering_display_info->display_primaries[0].y;

    mdcv->white_point_x = state->mastering_display_info->white_point.x;
    mdcv->white_point_y = state->mastering_display_info->white_point.y;
    mdcv->max_display_mastering_luminance =
        state->mastering_display_info->max_display_mastering_luminance;
    mdcv->min_display_mastering_luminance =
        state->mastering_display_info->min_display_mastering_luminance;

    g_array_append_val (self->sei_array, sei);
  }

  if (state->content_light_level) {
    GstH264SEIMessage sei;
    GstH264ContentLightLevel *cll;

    memset (&sei, 0, sizeof (GstH264SEIMessage));

    sei.payloadType = GST_H264_SEI_CONTENT_LIGHT_LEVEL;
    cll = &sei.payload.content_light_level;

    cll->max_content_light_level =
        state->content_light_level->max_content_light_level;
    cll->max_pic_average_light_level =
        state->content_light_level->max_frame_average_light_level;

    g_array_append_val (self->sei_array, sei);
  }

  if (self->sei_array->len > 0) {
    if (!self->packetized) {
      self->sei = gst_h264_create_sei_memory (4, self->sei_array);
    } else {
      self->sei = gst_h264_create_sei_memory_avc (4, self->sei_array);
    }
  }

  return TRUE;
}

static gboolean
gst_nv_h264_encoder_set_output_state (GstNvEncoder * encoder,
    GstVideoCodecState * state, gpointer session)
{
  GstNvH264Encoder *self = GST_NV_H264_ENCODER (encoder);
  GstVideoCodecState *output_state;
  NV_ENC_SEQUENCE_PARAM_PAYLOAD seq_params = { 0, };
  guint8 spspps[1024];
  guint32 seq_size;
  GstCaps *caps;
  const gchar *profile_from_sps;
  NVENCSTATUS status;
  std::set < std::string > downstream_profiles;
  std::string caps_str;
  GstTagList *tags;
  GstBuffer *codec_data = nullptr;
  GstH264NalUnit sps_nalu, pps_nalu;
  GstH264ParserResult rst;

  caps_str = "video/x-h264, alignment = (string) au";

  gst_nv_h264_encoder_get_downstream_profiles_and_format (self,
      downstream_profiles, nullptr);

  seq_params.version = gst_nvenc_get_sequence_param_payload_version ();
  seq_params.inBufferSize = sizeof (spspps);
  seq_params.spsppsBuffer = &spspps;
  seq_params.outSPSPPSPayloadSize = &seq_size;
  status = NvEncGetSequenceParams (session, &seq_params);
  if (!gst_nv_enc_result (status, self)) {
    GST_ERROR_OBJECT (self, "Failed to get sequence header");
    return FALSE;
  }

  rst = gst_h264_parser_identify_nalu (self->parser,
      spspps, 0, seq_size, &sps_nalu);
  if (rst != GST_H264_PARSER_OK) {
    GST_ERROR_OBJECT (self, "Failed to identify SPS nal");
    return FALSE;
  }

  if (sps_nalu.size < 4) {
    GST_ERROR_OBJECT (self, "Too small sps nal size %d", sps_nalu.size);
    return FALSE;
  }

  rst = gst_h264_parser_identify_nalu_unchecked (self->parser,
      spspps, sps_nalu.offset + sps_nalu.size, seq_size, &pps_nalu);
  if (rst != GST_H264_PARSER_OK && self->packetized) {
    GST_ERROR_OBJECT (self, "Failed to identify PPS nal, %d", rst);
    return FALSE;
  }

  if (self->packetized) {
    GstMapInfo info;
    guint8 *data;
    guint8 profile_idc, profile_comp, level_idc;
    const guint nal_length_size = 4;
    const guint num_sps = 1;
    const guint num_pps = 1;

    data = sps_nalu.data + sps_nalu.offset + sps_nalu.header_bytes;
    profile_idc = data[0];
    profile_comp = data[1];
    level_idc = data[2];

    /* 5: configuration version, profile, compatibility, level, nal length
     * 1: num sps
     * 2: sps size bytes
     * sizeof (sps)
     * 1: num pps
     * 2: pps size bytes
     * sizeof (pps)
     *
     * -> 11 + sps_size + pps_size
     */
    codec_data = gst_buffer_new_and_alloc (11 + sps_nalu.size + pps_nalu.size);

    gst_buffer_map (codec_data, &info, GST_MAP_WRITE);

    data = (guint8 *) info.data;
    data[0] = 1;
    data[1] = profile_idc;
    data[2] = profile_comp;
    data[3] = level_idc;
    data[4] = 0xfc | (nal_length_size - 1);
    data[5] = 0xe0 | num_sps;
    data += 6;
    GST_WRITE_UINT16_BE (data, sps_nalu.size);
    data += 2;
    memcpy (data, sps_nalu.data + sps_nalu.offset, sps_nalu.size);
    data += sps_nalu.size;

    data[0] = num_pps;
    data++;

    GST_WRITE_UINT16_BE (data, pps_nalu.size);
    data += 2;
    memcpy (data, pps_nalu.data + pps_nalu.offset, pps_nalu.size);

    gst_buffer_unmap (codec_data, &info);
  }

  profile_from_sps =
      gst_codec_utils_h264_get_profile (sps_nalu.data + sps_nalu.offset +
      sps_nalu.header_bytes, 3);

  if (!profile_from_sps) {
    GST_WARNING_OBJECT (self, "Failed to parse profile from SPS");
  } else if (!downstream_profiles.empty ()) {
    if (downstream_profiles.find (profile_from_sps) !=
        downstream_profiles.end ()) {
      caps_str += ", profile = (string) " + std::string (profile_from_sps);
    } else if (downstream_profiles.find ("baseline") !=
        downstream_profiles.end () &&
        strcmp (profile_from_sps, "constrained-baseline") == 0) {
      caps_str += ", profile = (string) baseline";
    } else if (downstream_profiles.find ("constrained-baseline") !=
        downstream_profiles.end () &&
        strcmp (profile_from_sps, "constrained-baseline") == 0) {
      caps_str += ", profile = (string) constrained-baseline";
    }
  } else {
    caps_str += ", profile = (string) " + std::string (profile_from_sps);
  }

  if (self->packetized) {
    caps_str += ", stream-format = (string) avc";
  } else {
    caps_str += ", stream-format = (string) byte-stream";
  }

  caps = gst_caps_from_string (caps_str.c_str ());

  if (self->packetized) {
    gst_caps_set_simple (caps, "codec_data", GST_TYPE_BUFFER, codec_data,
        nullptr);
    gst_buffer_unref (codec_data);
  }

  output_state = gst_video_encoder_set_output_state (GST_VIDEO_ENCODER (self),
      caps, state);

  GST_INFO_OBJECT (self, "Output caps: %" GST_PTR_FORMAT, output_state->caps);
  gst_video_codec_state_unref (output_state);

  tags = gst_tag_list_new_empty ();
  gst_tag_list_add (tags, GST_TAG_MERGE_REPLACE, GST_TAG_ENCODER,
      "nvh264encoder", nullptr);

  gst_video_encoder_merge_tags (GST_VIDEO_ENCODER (encoder),
      tags, GST_TAG_MERGE_REPLACE);
  gst_tag_list_unref (tags);

  return TRUE;
}

static GstBuffer *
gst_nv_h264_encoder_create_output_buffer (GstNvEncoder * encoder,
    NV_ENC_LOCK_BITSTREAM * bitstream)
{
  GstNvH264Encoder *self = GST_NV_H264_ENCODER (encoder);
  GstBuffer *buffer = nullptr;
  GstH264ParserResult rst;
  GstH264NalUnit nalu;

  if (!self->packetized) {
    buffer = gst_buffer_new_memdup (bitstream->bitstreamBufferPtr,
        bitstream->bitstreamSizeInBytes);
  } else {
    std::vector < GstH264NalUnit > nalu_list;
    gsize total_size = 0;
    GstMapInfo info;
    guint8 *data;

    rst = gst_h264_parser_identify_nalu (self->parser,
        (guint8 *) bitstream->bitstreamBufferPtr, 0,
        bitstream->bitstreamSizeInBytes, &nalu);

    if (rst == GST_H264_PARSER_NO_NAL_END)
      rst = GST_H264_PARSER_OK;

    while (rst == GST_H264_PARSER_OK) {
      nalu_list.push_back (nalu);
      total_size += nalu.size + 4;

      rst = gst_h264_parser_identify_nalu (self->parser,
          (guint8 *) bitstream->bitstreamBufferPtr, nalu.offset + nalu.size,
          bitstream->bitstreamSizeInBytes, &nalu);

      if (rst == GST_H264_PARSER_NO_NAL_END)
        rst = GST_H264_PARSER_OK;
    }

    buffer = gst_buffer_new_and_alloc (total_size);
    gst_buffer_map (buffer, &info, GST_MAP_WRITE);
    data = (guint8 *) info.data;
    /* *INDENT-OFF* */
    for (const auto & it : nalu_list) {
      GST_WRITE_UINT32_BE (data, it.size);
      data += 4;
      memcpy (data, it.data + it.offset, it.size);
      data += it.size;
    }
    /* *INDENT-ON* */

    gst_buffer_unmap (buffer, &info);
  }

  if (bitstream->pictureType == NV_ENC_PIC_TYPE_IDR && self->sei) {
    GstBuffer *new_buf = nullptr;

    if (!self->packetized) {
      new_buf = gst_h264_parser_insert_sei (self->parser, buffer, self->sei);
    } else {
      new_buf = gst_h264_parser_insert_sei_avc (self->parser, 4, buffer,
          self->sei);
    }

    if (new_buf) {
      gst_buffer_unref (buffer);
      buffer = new_buf;
    } else {
      GST_WARNING_OBJECT (self, "Couldn't insert SEI memory");
    }
  }

  return buffer;
}

static GstNvEncoderReconfigure
gst_nv_h264_encoder_check_reconfigure (GstNvEncoder * encoder,
    NV_ENC_CONFIG * config)
{
  GstNvH264Encoder *self = GST_NV_H264_ENCODER (encoder);
  GstNvEncoderReconfigure reconfig = GST_NV_ENCODER_RECONFIGURE_NONE;

  /* Dynamic RC param update is not tested, do soft-reconfigure only for
   * bitrate update */
  g_mutex_lock (&self->prop_lock);
  if (self->init_param_updated || self->rc_param_updated) {
    reconfig = GST_NV_ENCODER_RECONFIGURE_FULL;
    goto done;
  }

  if (self->bitrate_updated) {
    GstNvH264EncoderClass *klass = GST_NV_H264_ENCODER_GET_CLASS (self);
    if (klass->device_caps.dyn_bitrate_change > 0) {
      config->rcParams.averageBitRate = self->bitrate * 1024;
      config->rcParams.maxBitRate = self->max_bitrate * 1024;
      reconfig = GST_NV_ENCODER_RECONFIGURE_BITRATE;
    } else {
      reconfig = GST_NV_ENCODER_RECONFIGURE_FULL;
    }
  }

done:
  self->init_param_updated = FALSE;
  self->rc_param_updated = FALSE;
  self->bitrate_updated = FALSE;
  g_mutex_unlock (&self->prop_lock);

  return reconfig;
}

static gboolean
gst_nv_h264_encoder_select_device (GstNvEncoder * encoder,
    const GstVideoInfo * info, GstBuffer * buffer,
    GstNvEncoderDeviceData * data)
{
  GstNvH264Encoder *self = GST_NV_H264_ENCODER (encoder);
  GstNvH264EncoderClass *klass = GST_NV_H264_ENCODER_GET_CLASS (self);
  GstMemory *mem;

  memset (data, 0, sizeof (GstNvEncoderDeviceData));

  g_assert (klass->device_mode == GST_NV_ENCODER_DEVICE_AUTO_SELECT);

  mem = gst_buffer_peek_memory (buffer, 0);
  if (klass->cuda_device_id_size > 0 && gst_is_cuda_memory (mem)) {
    GstCudaMemory *cmem = GST_CUDA_MEMORY_CAST (mem);
    GstCudaContext *context = cmem->context;
    guint device_id;
    gboolean found = FALSE;

    g_object_get (context, "cuda-device-id", &device_id, nullptr);

    data->device_mode = GST_NV_ENCODER_DEVICE_CUDA;
    self->selected_device_mode = GST_NV_ENCODER_DEVICE_CUDA;

    for (guint i = 0; i < klass->cuda_device_id_size; i++) {
      if (klass->cuda_device_id_list[i] == device_id) {
        data->cuda_device_id = device_id;
        found = TRUE;
        break;
      }
    }

    if (!found) {
      GST_INFO_OBJECT (self,
          "Upstream CUDA device is not in supported device list");
      data->cuda_device_id = self->cuda_device_id;
    } else {
      data->device = (GstObject *) gst_object_ref (context);
    }

    if (data->cuda_device_id != self->cuda_device_id) {
      self->cuda_device_id = data->cuda_device_id;
      g_object_notify (G_OBJECT (self), "cuda-device-id");
    }

    return TRUE;
  }
#ifdef G_OS_WIN32
  if (klass->adapter_luid_size > 0 && gst_is_d3d11_memory (mem)) {
    GstD3D11Memory *dmem = GST_D3D11_MEMORY_CAST (mem);
    GstD3D11Device *device = dmem->device;
    gint64 adapter_luid;
    gboolean found = FALSE;

    g_object_get (device, "adapter-luid", &adapter_luid, nullptr);

    data->device_mode = GST_NV_ENCODER_DEVICE_D3D11;
    self->selected_device_mode = GST_NV_ENCODER_DEVICE_D3D11;

    for (guint i = 0; i < klass->cuda_device_id_size; i++) {
      if (klass->adapter_luid_list[i] == adapter_luid) {
        data->adapter_luid = adapter_luid;
        found = TRUE;
        break;
      }
    }

    if (!found) {
      GST_INFO_OBJECT (self,
          "Upstream D3D11 device is not in supported device list");
      data->adapter_luid = self->adapter_luid;
    } else {
      data->device = (GstObject *) gst_object_ref (device);
    }

    if (data->adapter_luid != self->adapter_luid) {
      self->adapter_luid = data->adapter_luid;
      g_object_notify (G_OBJECT (self), "adapter-luid");
    }

    return TRUE;
  }
#endif

  if (klass->cuda_device_id_size > 0 &&
      (self->selected_device_mode != GST_NV_ENCODER_DEVICE_D3D11)) {
    GST_INFO_OBJECT (self, "Upstream is system memory, use CUDA mode");
    data->device_mode = GST_NV_ENCODER_DEVICE_CUDA;
    data->cuda_device_id = self->cuda_device_id;
  } else {
    GST_INFO_OBJECT (self, "Upstream is system memory, use D3D11 mode");
    data->device_mode = GST_NV_ENCODER_DEVICE_D3D11;
    data->adapter_luid = klass->adapter_luid;
  }

  self->selected_device_mode = data->device_mode;

  return TRUE;
}

static guint
gst_nv_h264_encoder_calculate_min_buffers (GstNvEncoder * encoder)
{
  GstNvH264Encoder *self = GST_NV_H264_ENCODER (encoder);
  guint num_buffers;

  /* At least 4 surfaces are required as documented by Nvidia Encoder guide */
  num_buffers = 4;

  /* lookahead depth */
  num_buffers += self->rc_lookahead;

  /* B frames + 1 */
  num_buffers += self->bframes + 1;

  return num_buffers;
}

static GstNvEncoderClassData *
gst_nv_h264_encoder_create_class_data (GstObject * device, gpointer session,
    GstNvEncoderDeviceMode device_mode)
{
  NVENCSTATUS status;
  GstNvEncoderDeviceCaps dev_caps = { 0, };
  GUID profile_guids[16];
  NV_ENC_BUFFER_FORMAT input_formats[16];
  guint32 profile_guid_count = 0;
  guint32 input_format_count = 0;
  std::string sink_caps_str;
  std::string src_caps_str;
  std::string format_str;
  std::set < std::string > formats;
  std::set < std::string > profiles;
  std::string profile_str;
  std::string resolution_str;
  GstNvEncoderClassData *cdata;
  GstCaps *sink_caps;
  GstCaps *system_caps;
  NV_ENC_PRESET_CONFIG preset_config = { 0, };

  preset_config.version = gst_nvenc_get_preset_config_version ();
  preset_config.presetCfg.version = gst_nvenc_get_config_version ();

  status = NvEncGetEncodePresetConfigEx (session, NV_ENC_CODEC_H264_GUID,
      NV_ENC_PRESET_P4_GUID, NV_ENC_TUNING_INFO_HIGH_QUALITY, &preset_config);
  if (status != NV_ENC_SUCCESS) {
    GST_WARNING_OBJECT (device, "New preset is not supported");
    return nullptr;
  }

  status = NvEncGetEncodeProfileGUIDs (session, NV_ENC_CODEC_H264_GUID,
      profile_guids, G_N_ELEMENTS (profile_guids), &profile_guid_count);
  if (status != NV_ENC_SUCCESS || profile_guid_count == 0) {
    GST_WARNING_OBJECT (device, "Unable to get supported profiles");
    return nullptr;
  }

  status = NvEncGetInputFormats (session, NV_ENC_CODEC_H264_GUID, input_formats,
      G_N_ELEMENTS (input_formats), &input_format_count);
  if (status != NV_ENC_SUCCESS || input_format_count == 0) {
    GST_WARNING_OBJECT (device, "Unable to get supported input formats");
    return nullptr;
  }

  gst_nv_encoder_get_encoder_caps (session, &NV_ENC_CODEC_H264_GUID, &dev_caps);

  for (guint32 i = 0; i < input_format_count; i++) {
    switch (input_formats[i]) {
      case NV_ENC_BUFFER_FORMAT_NV12:
        formats.insert ("NV12");
        break;
      case NV_ENC_BUFFER_FORMAT_YUV444:
        if (dev_caps.yuv444_encode)
          formats.insert ("Y444");
        break;
      default:
        break;
    }
  }

  if (formats.empty ()) {
    GST_WARNING_OBJECT (device, "Empty supported input format");
    return nullptr;
  }
#define APPEND_STRING(dst,set,str) G_STMT_START { \
  if (set.find(str) != set.end()) { \
    if (!first) \
      dst += ", "; \
    dst += str; \
    first = false; \
  } \
} G_STMT_END

  if (formats.size () == 1) {
    format_str = "format = (string) " + *(formats.begin ());
  } else {
    bool first = true;

    format_str = "format = (string) { ";
    APPEND_STRING (format_str, formats, "NV12");
    APPEND_STRING (format_str, formats, "Y444");
    format_str += " }";
  }

  for (guint32 i = 0; i < profile_guid_count; i++) {
    if (profile_guids[i] == NV_ENC_H264_PROFILE_BASELINE_GUID) {
      profiles.insert ("baseline");
      profiles.insert ("constrained-baseline");
    } else if (profile_guids[i] == NV_ENC_H264_PROFILE_MAIN_GUID) {
      profiles.insert ("main");
    } else if (profile_guids[i] == NV_ENC_H264_PROFILE_HIGH_GUID) {
      profiles.insert ("high");
    } else if (profile_guids[i] == NV_ENC_H264_PROFILE_HIGH_444_GUID) {
      profiles.insert ("high-4:4:4");
    } else if (profile_guids[i] == NV_ENC_H264_PROFILE_PROGRESSIVE_HIGH_GUID) {
      profiles.insert ("progressive-high");
    } else if (profile_guids[i] == NV_ENC_H264_PROFILE_CONSTRAINED_HIGH_GUID) {
      profiles.insert ("constrained-high");
    }
  }

  if (profiles.empty ()) {
    GST_WARNING_OBJECT (device, "Empty supported h264 profile");
    return nullptr;
  }

  if (profiles.size () == 1) {
    profile_str = "profile = (string) " + *(profiles.begin ());
  } else {
    bool first = true;

    profile_str = "profile = (string) { ";
    APPEND_STRING (profile_str, profiles, "main");
    APPEND_STRING (profile_str, profiles, "high");
    APPEND_STRING (profile_str, profiles, "progressive-high");
    APPEND_STRING (profile_str, profiles, "constrained-high");
    APPEND_STRING (profile_str, profiles, "constrained-baseline");
    APPEND_STRING (profile_str, profiles, "baseline");
    APPEND_STRING (profile_str, profiles, "high-4:4:4");
    profile_str += " }";
  }
#undef APPEND_STRING

  resolution_str = "width = (int) [ " +
      std::to_string (GST_ROUND_UP_16 (dev_caps.width_min))
      + ", " + std::to_string (dev_caps.width_max) + " ]";
  resolution_str += ", height = (int) [ " +
      std::to_string (GST_ROUND_UP_16 (dev_caps.height_min))
      + ", " + std::to_string (dev_caps.height_max) + " ]";

  sink_caps_str = "video/x-raw, " + format_str + ", " + resolution_str;

  if (dev_caps.field_encoding > 0) {
    sink_caps_str +=
        ", interlace-mode = (string) { progressive, interleaved, mixed }";
  } else {
    sink_caps_str += ", interlace-mode = (string) progressive";
  }

  src_caps_str = "video/x-h264, " + resolution_str + ", " + profile_str +
      ", stream-format = (string) { avc, byte-stream }, alignment = (string) au";

  system_caps = gst_caps_from_string (sink_caps_str.c_str ());
  sink_caps = gst_caps_copy (system_caps);
#ifdef G_OS_WIN32
  if (device_mode == GST_NV_ENCODER_DEVICE_D3D11) {
    gst_caps_set_features (sink_caps, 0,
        gst_caps_features_new (GST_CAPS_FEATURE_MEMORY_D3D11_MEMORY, nullptr));
  }
#endif

  if (device_mode == GST_NV_ENCODER_DEVICE_CUDA) {
    gst_caps_set_features (sink_caps, 0,
        gst_caps_features_new (GST_CAPS_FEATURE_MEMORY_CUDA_MEMORY, nullptr));
#ifdef HAVE_CUDA_GST_GL
    GstCaps *gl_caps = gst_caps_copy (system_caps);
    gst_caps_set_features (gl_caps, 0,
        gst_caps_features_new (GST_CAPS_FEATURE_MEMORY_GL_MEMORY, nullptr));
    gst_caps_append (sink_caps, gl_caps);
#endif
  }

  gst_caps_append (sink_caps, system_caps);

  cdata = gst_nv_encoder_class_data_new ();
  cdata->sink_caps = sink_caps;
  cdata->src_caps = gst_caps_from_string (src_caps_str.c_str ());
  cdata->device_caps = dev_caps;
  cdata->device_mode = device_mode;

  /* *INDENT-OFF* */
  for (const auto &iter: formats)
    cdata->formats = g_list_append (cdata->formats, g_strdup (iter.c_str()));

  for (const auto &iter: profiles)
    cdata->profiles = g_list_append (cdata->profiles, g_strdup (iter.c_str()));
  /* *INDENT-ON* */

  if (device_mode == GST_NV_ENCODER_DEVICE_D3D11)
    g_object_get (device, "adapter-luid", &cdata->adapter_luid, nullptr);

  if (device_mode == GST_NV_ENCODER_DEVICE_CUDA)
    g_object_get (device, "cuda-device-id", &cdata->cuda_device_id, nullptr);

  /* class data will be leaked if the element never gets instantiated */
  GST_MINI_OBJECT_FLAG_SET (cdata->sink_caps,
      GST_MINI_OBJECT_FLAG_MAY_BE_LEAKED);
  GST_MINI_OBJECT_FLAG_SET (cdata->src_caps,
      GST_MINI_OBJECT_FLAG_MAY_BE_LEAKED);

  return cdata;
}

GstNvEncoderClassData *
gst_nv_h264_encoder_register_cuda (GstPlugin * plugin, GstCudaContext * context,
    guint rank)
{
  NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS session_params = { 0, };
  gpointer session;
  NVENCSTATUS status;
  GstNvEncoderClassData *cdata;

  GST_DEBUG_CATEGORY_INIT (gst_nv_h264_encoder_debug, "nvh264encoder", 0,
      "nvh264encoder");

  session_params.version =
      gst_nvenc_get_open_encode_session_ex_params_version ();
  session_params.deviceType = NV_ENC_DEVICE_TYPE_CUDA;
  session_params.device = gst_cuda_context_get_handle (context);
  session_params.apiVersion = gst_nvenc_get_api_version ();

  status = NvEncOpenEncodeSessionEx (&session_params, &session);
  if (status != NV_ENC_SUCCESS) {
    GST_WARNING_OBJECT (context, "Failed to open session");
    return nullptr;
  }

  cdata = gst_nv_h264_encoder_create_class_data (GST_OBJECT (context), session,
      GST_NV_ENCODER_DEVICE_CUDA);
  NvEncDestroyEncoder (session);

  if (!cdata)
    return nullptr;

  gst_nv_encoder_class_data_ref (cdata);

  GType type;
  gchar *type_name;
  gchar *feature_name;
  GTypeInfo type_info = {
    sizeof (GstNvH264EncoderClass),
    nullptr,
    nullptr,
    (GClassInitFunc) gst_nv_h264_encoder_class_init,
    nullptr,
    cdata,
    sizeof (GstNvH264Encoder),
    0,
    (GInstanceInitFunc) gst_nv_h264_encoder_init,
  };

  type_name = g_strdup ("GstNvCudaH264Enc");
  feature_name = g_strdup ("nvcudah264enc");

  gint index = 0;
  while (g_type_from_name (type_name)) {
    index++;
    g_free (type_name);
    g_free (feature_name);
    type_name = g_strdup_printf ("GstNvCudaH264Device%dEnc", index);
    feature_name = g_strdup_printf ("nvcudah264device%denc", index);
  }

  type = g_type_register_static (GST_TYPE_NV_ENCODER, type_name,
      &type_info, (GTypeFlags) 0);

  if (rank > 0 && index != 0)
    rank--;

  if (index != 0)
    gst_element_type_set_skip_documentation (type);

  if (!gst_element_register (plugin, feature_name, rank, type))
    GST_WARNING ("Failed to register plugin '%s'", type_name);

  g_free (type_name);
  g_free (feature_name);

  return cdata;
}

#ifdef G_OS_WIN32
GstNvEncoderClassData *
gst_nv_h264_encoder_register_d3d11 (GstPlugin * plugin, GstD3D11Device * device,
    guint rank)
{
  NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS session_params = { 0, };
  gpointer session;
  NVENCSTATUS status;
  GstNvEncoderClassData *cdata;

  GST_DEBUG_CATEGORY_INIT (gst_nv_h264_encoder_debug, "nvh264encoder", 0,
      "nvh264encoder");

  session_params.version =
      gst_nvenc_get_open_encode_session_ex_params_version ();
  session_params.deviceType = NV_ENC_DEVICE_TYPE_DIRECTX;
  session_params.device = gst_d3d11_device_get_device_handle (device);
  session_params.apiVersion = gst_nvenc_get_api_version ();

  status = NvEncOpenEncodeSessionEx (&session_params, &session);
  if (status != NV_ENC_SUCCESS) {
    GST_WARNING_OBJECT (device, "Failed to open session");
    return nullptr;
  }

  cdata = gst_nv_h264_encoder_create_class_data (GST_OBJECT (device), session,
      GST_NV_ENCODER_DEVICE_D3D11);
  NvEncDestroyEncoder (session);

  if (!cdata)
    return nullptr;

  gst_nv_encoder_class_data_ref (cdata);

  GType type;
  gchar *type_name;
  gchar *feature_name;
  GTypeInfo type_info = {
    sizeof (GstNvH264EncoderClass),
    nullptr,
    nullptr,
    (GClassInitFunc) gst_nv_h264_encoder_class_init,
    nullptr,
    cdata,
    sizeof (GstNvH264Encoder),
    0,
    (GInstanceInitFunc) gst_nv_h264_encoder_init,
  };

  type_name = g_strdup ("GstNvD3D11H264Enc");
  feature_name = g_strdup ("nvd3d11h264enc");

  gint index = 0;
  while (g_type_from_name (type_name)) {
    index++;
    g_free (type_name);
    g_free (feature_name);
    type_name = g_strdup_printf ("GstNvD3D11H264Device%dEnc", index);
    feature_name = g_strdup_printf ("nvd3d11h264device%denc", index);
  }

  type = g_type_register_static (GST_TYPE_NV_ENCODER, type_name,
      &type_info, (GTypeFlags) 0);

  if (rank > 0 && index != 0)
    rank--;

  if (index != 0)
    gst_element_type_set_skip_documentation (type);

  if (!gst_element_register (plugin, feature_name, rank, type))
    GST_WARNING ("Failed to register plugin '%s'", type_name);

  g_free (type_name);
  g_free (feature_name);

  return cdata;
}
#endif

void
gst_nv_h264_encoder_register_auto_select (GstPlugin * plugin,
    GList * device_caps_list, guint rank)
{
  std::set < std::string > formats;
  std::set < std::string > profiles;
  std::string sink_caps_str;
  std::string src_caps_str;
  std::string format_str;
  std::string profile_str;
  std::string resolution_str;
  GList *iter;
  guint adapter_luid_size = 0;
  gint64 adapter_luid_list[8] = { 0, };
  guint cuda_device_id_size = 0;
  guint cuda_device_id_list[8] = { 0, };
  GstNvEncoderDeviceCaps dev_caps;
  GstNvEncoderClassData *cdata;
  GstCaps *sink_caps = nullptr;
  GstCaps *system_caps;

  GST_DEBUG_CATEGORY_INIT (gst_nv_h264_encoder_debug, "nvh264encoder", 0,
      "nvh264encoder");

  for (iter = device_caps_list; iter; iter = g_list_next (iter)) {
    GstNvEncoderClassData *cdata = (GstNvEncoderClassData *) iter->data;
    GList *walk;

    for (walk = cdata->formats; walk; walk = g_list_next (walk))
      formats.insert ((gchar *) walk->data);

    for (walk = cdata->profiles; walk; walk = g_list_next (walk))
      profiles.insert ((gchar *) walk->data);

    if (cdata->device_mode == GST_NV_ENCODER_DEVICE_D3D11 &&
        adapter_luid_size <= G_N_ELEMENTS (adapter_luid_list) - 1) {
      adapter_luid_list[adapter_luid_size] = cdata->adapter_luid;
      adapter_luid_size++;
    }

    if (cdata->device_mode == GST_NV_ENCODER_DEVICE_CUDA &&
        cuda_device_id_size <= G_N_ELEMENTS (cuda_device_id_list) - 1) {
      cuda_device_id_list[cuda_device_id_size] = cdata->cuda_device_id;
      cuda_device_id_size++;
    }

    if (iter == device_caps_list) {
      dev_caps = cdata->device_caps;
    } else {
      gst_nv_encoder_merge_device_caps (&dev_caps, &cdata->device_caps,
          &dev_caps);
    }
  }

  g_list_free_full (device_caps_list,
      (GDestroyNotify) gst_nv_encoder_class_data_unref);
  if (formats.empty () || profiles.empty ())
    return;

#define APPEND_STRING(dst,set,str) G_STMT_START { \
  if (set.find(str) != set.end()) { \
    if (!first) \
      dst += ", "; \
    dst += str; \
    first = false; \
  } \
} G_STMT_END

  if (formats.size () == 1) {
    format_str = "format = (string) " + *(formats.begin ());
  } else {
    bool first = true;

    format_str = "format = (string) { ";
    APPEND_STRING (format_str, formats, "NV12");
    APPEND_STRING (format_str, formats, "Y444");
    format_str += " }";
  }

  if (profiles.size () == 1) {
    profile_str = "profile = (string) " + *(profiles.begin ());
  } else {
    bool first = true;

    profile_str = "profile = (string) { ";
    APPEND_STRING (profile_str, profiles, "main");
    APPEND_STRING (profile_str, profiles, "high");
    APPEND_STRING (profile_str, profiles, "progressive-high");
    APPEND_STRING (profile_str, profiles, "constrained-high");
    APPEND_STRING (profile_str, profiles, "constrained-baseline");
    APPEND_STRING (profile_str, profiles, "baseline");
    APPEND_STRING (profile_str, profiles, "high-4:4:4");
    profile_str += " }";
  }
#undef APPEND_STRING

  resolution_str = "width = (int) [ " +
      std::to_string (GST_ROUND_UP_16 (dev_caps.width_min))
      + ", " + std::to_string (dev_caps.width_max) + " ]";
  resolution_str += ", height = (int) [ " +
      std::to_string (GST_ROUND_UP_16 (dev_caps.height_min))
      + ", " + std::to_string (dev_caps.height_max) + " ]";

  sink_caps_str = "video/x-raw, " + format_str + ", " + resolution_str;

  if (dev_caps.field_encoding > 0) {
    sink_caps_str +=
        ", interlace-mode = (string) { progressive, interleaved, mixed }";
  } else {
    sink_caps_str += ", interlace-mode = (string) progressive";
  }

  src_caps_str = "video/x-h264, " + resolution_str + ", " + profile_str +
      ", stream-format = (string) { avc, byte-stream }, alignment = (string) au";

  system_caps = gst_caps_from_string (sink_caps_str.c_str ());
  sink_caps = gst_caps_new_empty ();

  if (cuda_device_id_size > 0) {
    GstCaps *cuda_caps = gst_caps_copy (system_caps);
    gst_caps_set_features (cuda_caps, 0,
        gst_caps_features_new (GST_CAPS_FEATURE_MEMORY_CUDA_MEMORY, nullptr));
    gst_caps_append (sink_caps, cuda_caps);
  }
#ifdef G_OS_WIN32
  if (adapter_luid_size > 0) {
    GstCaps *d3d11_caps = gst_caps_copy (system_caps);
    gst_caps_set_features (d3d11_caps, 0,
        gst_caps_features_new (GST_CAPS_FEATURE_MEMORY_D3D11_MEMORY, nullptr));
    gst_caps_append (sink_caps, d3d11_caps);
  }
#endif

#ifdef HAVE_CUDA_GST_GL
  GstCaps *gl_caps = gst_caps_copy (system_caps);
  gst_caps_set_features (gl_caps, 0,
      gst_caps_features_new (GST_CAPS_FEATURE_MEMORY_GL_MEMORY, nullptr));
  gst_caps_append (sink_caps, gl_caps);
#endif

  gst_caps_append (sink_caps, system_caps);

  cdata = gst_nv_encoder_class_data_new ();
  cdata->sink_caps = sink_caps;
  cdata->src_caps = gst_caps_from_string (src_caps_str.c_str ());
  cdata->device_caps = dev_caps;
  cdata->device_mode = GST_NV_ENCODER_DEVICE_AUTO_SELECT;
  cdata->adapter_luid = adapter_luid_list[0];
  cdata->adapter_luid_size = adapter_luid_size;
  memcpy (&cdata->adapter_luid_list,
      adapter_luid_list, sizeof (adapter_luid_list));
  cdata->cuda_device_id = cuda_device_id_list[0];
  cdata->cuda_device_id_size = cuda_device_id_size;
  memcpy (&cdata->cuda_device_id_list,
      cuda_device_id_list, sizeof (cuda_device_id_list));

  /* class data will be leaked if the element never gets instantiated */
  GST_MINI_OBJECT_FLAG_SET (cdata->sink_caps,
      GST_MINI_OBJECT_FLAG_MAY_BE_LEAKED);
  GST_MINI_OBJECT_FLAG_SET (cdata->src_caps,
      GST_MINI_OBJECT_FLAG_MAY_BE_LEAKED);

  GType type;
  GTypeInfo type_info = {
    sizeof (GstNvH264EncoderClass),
    nullptr,
    nullptr,
    (GClassInitFunc) gst_nv_h264_encoder_class_init,
    nullptr,
    cdata,
    sizeof (GstNvH264Encoder),
    0,
    (GInstanceInitFunc) gst_nv_h264_encoder_init,
  };

  type = g_type_register_static (GST_TYPE_NV_ENCODER, "GstNvAutoGpuH264Enc",
      &type_info, (GTypeFlags) 0);

  if (!gst_element_register (plugin, "nvautogpuh264enc", rank, type))
    GST_WARNING ("Failed to register plugin 'GstNvAutoGpuH264Enc'");
}
