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

#include "gstnvh264enc.h"

#include <gst/pbutils/codec-utils.h>

#include <string.h>

typedef struct
{
  GstCaps *sink_caps;
  GstCaps *src_caps;
  gboolean is_default;
} GstNvH264EncClassData;

GST_DEBUG_CATEGORY_STATIC (gst_nv_h264_enc_debug);
#define GST_CAT_DEFAULT gst_nv_h264_enc_debug

static GstElementClass *parent_class = NULL;

enum
{
  PROP_0,
  PROP_AUD,
  PROP_WEIGHTED_PRED,
  PROP_VBV_BUFFER_SIZE,
  PROP_RC_LOOKAHEAD,
  PROP_TEMPORAL_AQ,
  PROP_BFRAMES,
  PROP_B_ADAPT,
};

#define DEFAULT_AUD TRUE
#define DEFAULT_WEIGHTED_PRED FALSE
#define DEFAULT_VBV_BUFFER_SIZE 0
#define DEFAULT_RC_LOOKAHEAD 0
#define DEFAULT_TEMPORAL_AQ FALSE
#define DEFAULT_BFRAMES 0
#define DEFAULT_B_ADAPT FALSE

/* captured using RTX 2080 */
#define DOCUMENTATION_SINK_CAPS_COMM \
    "format = (string) { NV12, YV12, I420, BGRA, RGBA, Y444, VUYA }, " \
    "width = (int) [ 145, 4096 ], " \
    "height = (int) [ 49, 4096 ], " \
    "framerate = " GST_VIDEO_FPS_RANGE ", " \
    "interlace-mode = (string) { progressive } "

#define DOCUMENTATION_SINK_CAPS \
    "video/x-raw, " DOCUMENTATION_SINK_CAPS_COMM "; " \
    "video/x-raw(memory:GLMemory), " DOCUMENTATION_SINK_CAPS_COMM "; " \
    "video/x-raw(memory:CUDAMemory), " DOCUMENTATION_SINK_CAPS_COMM

#define DOCUMENTATION_SRC_CAPS \
    "video/x-h264, "  \
    "width = (int) [ 145, 4096 ], " \
    "height = (int) [ 49, 4096 ], " \
    "framerate = " GST_VIDEO_FPS_RANGE ", " \
    "stream-format = (string) byte-stream, " \
    "alignment = (string) au, " \
    "profile = (string) { main, high, high-4:4:4, baseline, constrained-baseline }"

static gboolean gst_nv_h264_enc_open (GstVideoEncoder * enc);
static gboolean gst_nv_h264_enc_close (GstVideoEncoder * enc);
static gboolean gst_nv_h264_enc_set_src_caps (GstNvBaseEnc * nvenc,
    GstVideoCodecState * state);
static gboolean gst_nv_h264_enc_set_encoder_config (GstNvBaseEnc * nvenc,
    GstVideoCodecState * state, NV_ENC_CONFIG * config);
static gboolean gst_nv_h264_enc_set_pic_params (GstNvBaseEnc * nvenc,
    GstVideoCodecFrame * frame, NV_ENC_PIC_PARAMS * pic_params);
static void gst_nv_h264_enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_nv_h264_enc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_nv_h264_enc_finalize (GObject * obj);

static void
gst_nv_h264_enc_class_init (GstNvH264EncClass * klass, gpointer data)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstVideoEncoderClass *videoenc_class = GST_VIDEO_ENCODER_CLASS (klass);
  GstNvBaseEncClass *nvenc_class = GST_NV_BASE_ENC_CLASS (klass);
  GstNvEncDeviceCaps *device_caps = &nvenc_class->device_caps;
  GstNvH264EncClassData *cdata = (GstNvH264EncClassData *) data;
  gchar *long_name;
  GstPadTemplate *pad_templ;
  GstCaps *doc_caps;

  parent_class = g_type_class_peek_parent (klass);

  gobject_class->set_property = gst_nv_h264_enc_set_property;
  gobject_class->get_property = gst_nv_h264_enc_get_property;
  gobject_class->finalize = gst_nv_h264_enc_finalize;

  videoenc_class->open = GST_DEBUG_FUNCPTR (gst_nv_h264_enc_open);
  videoenc_class->close = GST_DEBUG_FUNCPTR (gst_nv_h264_enc_close);

  nvenc_class->codec_id = NV_ENC_CODEC_H264_GUID;
  nvenc_class->set_encoder_config = gst_nv_h264_enc_set_encoder_config;
  nvenc_class->set_src_caps = gst_nv_h264_enc_set_src_caps;
  nvenc_class->set_pic_params = gst_nv_h264_enc_set_pic_params;

  /**
   * GstNvH264Enc:aud:
   *
   * Use AU (Access Unit) delimiter
   *
   * Since: 1.18
   */
  g_object_class_install_property (gobject_class, PROP_AUD,
      g_param_spec_boolean ("aud", "AUD",
          "Use AU (Access Unit) delimiter", DEFAULT_AUD,
          G_PARAM_READWRITE | GST_PARAM_MUTABLE_PLAYING |
          G_PARAM_STATIC_STRINGS));

  if (device_caps->weighted_prediction) {
    /**
     * GstNvH264Enc:weighted-pred:
     *
     * Weighted Prediction
     *
     * Since: 1.18
     */
    g_object_class_install_property (gobject_class, PROP_WEIGHTED_PRED,
        g_param_spec_boolean ("weighted-pred", "Weighted Pred",
            "Weighted Prediction", DEFAULT_WEIGHTED_PRED,
            G_PARAM_READWRITE | GST_PARAM_MUTABLE_PLAYING |
            GST_PARAM_CONDITIONALLY_AVAILABLE | G_PARAM_STATIC_STRINGS));
  }

  if (device_caps->custom_vbv_bufsize) {
    /**
     * GstNvH264Enc:vbv-buffer-size:
     *
     * VBV(HRD) Buffer Size in kbits (0 = NVENC default)
     *
     * Since: 1.18
     */
    g_object_class_install_property (gobject_class,
        PROP_VBV_BUFFER_SIZE,
        g_param_spec_uint ("vbv-buffer-size", "VBV Buffer Size",
            "VBV(HRD) Buffer Size in kbits (0 = NVENC default)",
            0, G_MAXUINT, DEFAULT_VBV_BUFFER_SIZE,
            G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
            GST_PARAM_CONDITIONALLY_AVAILABLE | G_PARAM_STATIC_STRINGS));
  }

  if (device_caps->lookahead) {
    /**
     * GstNvH264Enc:rc-lookahead:
     *
     * Number of frames for frame type lookahead
     *
     * Since: 1.18
     */
    g_object_class_install_property (gobject_class, PROP_RC_LOOKAHEAD,
        g_param_spec_uint ("rc-lookahead", "Rate Control Lookahead",
            "Number of frames for frame type lookahead",
            0, 32, DEFAULT_RC_LOOKAHEAD,
            G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
            GST_PARAM_CONDITIONALLY_AVAILABLE | G_PARAM_STATIC_STRINGS));
  }

  if (device_caps->temporal_aq) {
    /**
     * GstNvH264Enc:temporal-aq:
     *
     * Temporal Adaptive Quantization
     *
     * Since: 1.18
     */
    g_object_class_install_property (gobject_class, PROP_TEMPORAL_AQ,
        g_param_spec_boolean ("temporal-aq", "Temporal AQ",
            "Temporal Adaptive Quantization", DEFAULT_TEMPORAL_AQ,
            G_PARAM_READWRITE | GST_PARAM_MUTABLE_PLAYING |
            GST_PARAM_CONDITIONALLY_AVAILABLE | G_PARAM_STATIC_STRINGS));
  }

  if (device_caps->bframes > 0) {
    /**
     * GstNvH264Enc:bframes:
     *
     * Number of B-frames between I and P
     *
     * Since: 1.18
     */
    g_object_class_install_property (gobject_class, PROP_BFRAMES,
        g_param_spec_uint ("bframes", "B-Frames",
            "Number of B-frames between I and P", 0, device_caps->bframes,
            DEFAULT_BFRAMES,
            G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
            GST_PARAM_CONDITIONALLY_AVAILABLE | G_PARAM_STATIC_STRINGS));

    /**
     * GstNvH264Enc:b-adapt:
     *
     * Enable adaptive B-frame insert when lookahead is enabled
     *
     * Since: 1.18
     */
    g_object_class_install_property (gobject_class, PROP_B_ADAPT,
        g_param_spec_boolean ("b-adapt", "B Adapt",
            "Enable adaptive B-frame insert when lookahead is enabled",
            DEFAULT_B_ADAPT,
            G_PARAM_READWRITE | GST_PARAM_MUTABLE_READY |
            GST_PARAM_CONDITIONALLY_AVAILABLE | G_PARAM_STATIC_STRINGS));
  }

  if (cdata->is_default)
    long_name = g_strdup ("NVENC H.264 Video Encoder");
  else
    long_name = g_strdup_printf ("NVENC H.264 Video Encoder with device %d",
        nvenc_class->cuda_device_id);

  gst_element_class_set_metadata (element_class, long_name,
      "Codec/Encoder/Video/Hardware",
      "Encode H.264 video streams using NVIDIA's hardware-accelerated NVENC encoder API",
      "Tim-Philipp Müller <tim@centricular.com>, "
      "Matthew Waters <matthew@centricular.com>, "
      "Seungha Yang <seungha.yang@navercorp.com>");
  g_free (long_name);

  GST_DEBUG_CATEGORY_INIT (gst_nv_h264_enc_debug,
      "nvh264enc", 0, "Nvidia H.264 encoder");

  pad_templ = gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
      cdata->sink_caps);
  doc_caps = gst_caps_from_string (DOCUMENTATION_SINK_CAPS);
  gst_pad_template_set_documentation_caps (pad_templ, doc_caps);
  gst_caps_unref (doc_caps);
  gst_element_class_add_pad_template (element_class, pad_templ);

  pad_templ = gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
      cdata->src_caps);
  doc_caps = gst_caps_from_string (DOCUMENTATION_SRC_CAPS);
  gst_pad_template_set_documentation_caps (pad_templ, doc_caps);
  gst_caps_unref (doc_caps);
  gst_element_class_add_pad_template (element_class, pad_templ);

  gst_caps_unref (cdata->sink_caps);
  gst_caps_unref (cdata->src_caps);
  g_free (cdata);
}

static void
gst_nv_h264_enc_init (GstNvH264Enc * nvenc)
{
  GstNvBaseEnc *baseenc = GST_NV_BASE_ENC (nvenc);

  nvenc->aud = DEFAULT_AUD;

  /* device capability dependent properties */
  baseenc->weighted_pred = DEFAULT_WEIGHTED_PRED;
  baseenc->vbv_buffersize = DEFAULT_VBV_BUFFER_SIZE;
  baseenc->rc_lookahead = DEFAULT_RC_LOOKAHEAD;
  baseenc->temporal_aq = DEFAULT_TEMPORAL_AQ;
  baseenc->bframes = DEFAULT_BFRAMES;
  baseenc->b_adapt = DEFAULT_B_ADAPT;
}

static void
gst_nv_h264_enc_finalize (GObject * obj)
{
  G_OBJECT_CLASS (parent_class)->finalize (obj);
}

static gboolean
gst_nv_h264_enc_open (GstVideoEncoder * enc)
{
  GstNvBaseEnc *base = GST_NV_BASE_ENC (enc);

  if (!GST_VIDEO_ENCODER_CLASS (parent_class)->open (enc))
    return FALSE;

  /* Check if H.264 is supported */
  {
    uint32_t i, num = 0;
    GUID guids[16];

    NvEncGetEncodeGUIDs (base->encoder, guids, G_N_ELEMENTS (guids), &num);

    for (i = 0; i < num; ++i) {
      if (gst_nvenc_cmp_guid (guids[i], NV_ENC_CODEC_H264_GUID))
        break;
    }
    GST_INFO_OBJECT (enc, "H.264 encoding %ssupported", (i == num) ? "un" : "");
    if (i == num) {
      gst_nv_h264_enc_close (enc);
      return FALSE;
    }
  }

  return TRUE;
}

static gboolean
gst_nv_h264_enc_close (GstVideoEncoder * enc)
{
  return GST_VIDEO_ENCODER_CLASS (parent_class)->close (enc);
}

static gboolean
gst_nv_h264_enc_set_profile_and_level (GstNvH264Enc * nvenc, GstCaps * caps)
{
#define N_BYTES_SPS 128
  guint8 sps[N_BYTES_SPS];
  NV_ENC_SEQUENCE_PARAM_PAYLOAD spp = { 0, };
  GstStructure *s;
  const gchar *profile;
  GstCaps *allowed_caps;
  GstStructure *s2;
  const gchar *allowed_profile;
  NVENCSTATUS nv_ret;
  guint32 seq_size;

  spp.version = gst_nvenc_get_sequence_param_payload_version ();
  spp.inBufferSize = N_BYTES_SPS;
  spp.spsId = 0;
  spp.ppsId = 0;
  spp.spsppsBuffer = &sps;
  spp.outSPSPPSPayloadSize = &seq_size;
  nv_ret = NvEncGetSequenceParams (GST_NV_BASE_ENC (nvenc)->encoder, &spp);
  if (nv_ret != NV_ENC_SUCCESS) {
    GST_ELEMENT_ERROR (nvenc, STREAM, ENCODE, ("Encode header failed."),
        ("NvEncGetSequenceParams return code=%d", nv_ret));
    return FALSE;
  }

  if (seq_size < 8) {
    GST_ELEMENT_ERROR (nvenc, STREAM, ENCODE, ("Encode header failed."),
        ("NvEncGetSequenceParams returned incomplete data"));
    return FALSE;
  }

  /* skip nal header and identifier */
  gst_codec_utils_h264_caps_set_level_and_profile (caps, &sps[5], 3);

  /* Constrained baseline is a strict subset of baseline. If downstream
   * wanted baseline and we produced constrained baseline, we can just
   * set the profile to baseline in the caps to make negotiation happy.
   * Same goes for baseline as subset of main profile and main as a subset
   * of high profile.
   */
  s = gst_caps_get_structure (caps, 0);
  profile = gst_structure_get_string (s, "profile");

  allowed_caps = gst_pad_get_allowed_caps (GST_VIDEO_ENCODER_SRC_PAD (nvenc));

  if (allowed_caps == NULL)
    goto no_peer;

  if (!gst_caps_can_intersect (allowed_caps, caps)) {
    allowed_caps = gst_caps_make_writable (allowed_caps);
    allowed_caps = gst_caps_truncate (allowed_caps);
    s2 = gst_caps_get_structure (allowed_caps, 0);
    gst_structure_fixate_field_string (s2, "profile", profile);
    allowed_profile = gst_structure_get_string (s2, "profile");
    if (!strcmp (allowed_profile, "high")) {
      if (!strcmp (profile, "constrained-baseline")
          || !strcmp (profile, "baseline") || !strcmp (profile, "main")) {
        gst_structure_set (s, "profile", G_TYPE_STRING, "high", NULL);
        GST_INFO_OBJECT (nvenc, "downstream requested high profile, but "
            "encoder will now output %s profile (which is a subset), due "
            "to how it's been configured", profile);
      }
    } else if (!strcmp (allowed_profile, "main")) {
      if (!strcmp (profile, "constrained-baseline")
          || !strcmp (profile, "baseline")) {
        gst_structure_set (s, "profile", G_TYPE_STRING, "main", NULL);
        GST_INFO_OBJECT (nvenc, "downstream requested main profile, but "
            "encoder will now output %s profile (which is a subset), due "
            "to how it's been configured", profile);
      }
    } else if (!strcmp (allowed_profile, "baseline")) {
      if (!strcmp (profile, "constrained-baseline"))
        gst_structure_set (s, "profile", G_TYPE_STRING, "baseline", NULL);
    }
  }
  gst_caps_unref (allowed_caps);

no_peer:

  return TRUE;

#undef N_BYTES_SPS
}

static gboolean
gst_nv_h264_enc_set_src_caps (GstNvBaseEnc * nvenc, GstVideoCodecState * state)
{
  GstNvH264Enc *h264enc = (GstNvH264Enc *) nvenc;
  GstVideoCodecState *out_state;
  GstStructure *s;
  GstCaps *out_caps;

  out_caps = gst_caps_new_empty_simple ("video/x-h264");
  s = gst_caps_get_structure (out_caps, 0);

  /* TODO: add support for avc format as well */
  gst_structure_set (s, "stream-format", G_TYPE_STRING, "byte-stream",
      "alignment", G_TYPE_STRING, "au", NULL);

  if (!gst_nv_h264_enc_set_profile_and_level (h264enc, out_caps)) {
    gst_caps_unref (out_caps);
    return FALSE;
  }

  out_state = gst_video_encoder_set_output_state (GST_VIDEO_ENCODER (nvenc),
      out_caps, state);

  GST_INFO_OBJECT (nvenc, "output caps: %" GST_PTR_FORMAT, out_state->caps);

  /* encoder will keep it around for us */
  gst_video_codec_state_unref (out_state);

  /* TODO: would be nice to also send some tags with the codec name */
  return TRUE;
}

static gboolean
gst_nv_h264_enc_set_encoder_config (GstNvBaseEnc * nvenc,
    GstVideoCodecState * state, NV_ENC_CONFIG * config)
{
  GstNvH264Enc *h264enc = (GstNvH264Enc *) nvenc;
  GstCaps *allowed_caps, *template_caps;
  GUID selected_profile = NV_ENC_CODEC_PROFILE_AUTOSELECT_GUID;
  int level_idc = NV_ENC_LEVEL_AUTOSELECT;
  GstVideoInfo *info = &state->info;
  NV_ENC_CONFIG_H264 *h264_config = &config->encodeCodecConfig.h264Config;
  NV_ENC_CONFIG_H264_VUI_PARAMETERS *vui = &h264_config->h264VUIParameters;

  template_caps =
      gst_pad_get_pad_template_caps (GST_VIDEO_ENCODER_SRC_PAD (h264enc));
  allowed_caps = gst_pad_get_allowed_caps (GST_VIDEO_ENCODER_SRC_PAD (h264enc));

  if (template_caps == allowed_caps) {
    GST_INFO_OBJECT (h264enc, "downstream has ANY caps");
  } else if (allowed_caps) {
    GstStructure *s;
    const gchar *profile;
    const gchar *level;

    if (gst_caps_is_empty (allowed_caps)) {
      gst_caps_unref (allowed_caps);
      gst_caps_unref (template_caps);
      return FALSE;
    }

    allowed_caps = gst_caps_make_writable (allowed_caps);
    allowed_caps = gst_caps_fixate (allowed_caps);
    s = gst_caps_get_structure (allowed_caps, 0);

    profile = gst_structure_get_string (s, "profile");
    if (profile) {
      if (!strcmp (profile, "baseline")
          || !strcmp (profile, "constrained-baseline")) {
        selected_profile = NV_ENC_H264_PROFILE_BASELINE_GUID;
      } else if (g_str_has_prefix (profile, "high-4:4:4")) {
        selected_profile = NV_ENC_H264_PROFILE_HIGH_444_GUID;
      } else if (g_str_has_prefix (profile, "high-10")) {
        g_assert_not_reached ();
      } else if (g_str_has_prefix (profile, "high-4:2:2")) {
        g_assert_not_reached ();
      } else if (g_str_has_prefix (profile, "high")) {
        selected_profile = NV_ENC_H264_PROFILE_HIGH_GUID;
      } else if (g_str_has_prefix (profile, "main")) {
        selected_profile = NV_ENC_H264_PROFILE_MAIN_GUID;
      } else {
        g_assert_not_reached ();
      }
    }

    level = gst_structure_get_string (s, "level");
    if (level)
      /* matches values stored in NV_ENC_LEVEL */
      level_idc = gst_codec_utils_h264_get_level_idc (level);

    gst_caps_unref (allowed_caps);
  }
  gst_caps_unref (template_caps);

  /* override some defaults */
  GST_LOG_OBJECT (h264enc, "setting parameters");
  config->profileGUID = selected_profile;
  h264_config->level = level_idc;
  h264_config->chromaFormatIDC = 1;
  if (GST_VIDEO_INFO_FORMAT (info) == GST_VIDEO_FORMAT_Y444 ||
      GST_VIDEO_INFO_FORMAT (info) == GST_VIDEO_FORMAT_VUYA) {
    GST_DEBUG_OBJECT (h264enc, "have Y444 input, setting config accordingly");
    config->profileGUID = NV_ENC_H264_PROFILE_HIGH_444_GUID;
    h264_config->chromaFormatIDC = 3;
  }

  h264_config->idrPeriod = config->gopLength;
  h264_config->outputAUD = h264enc->aud;

  vui->videoSignalTypePresentFlag = 1;
  /* NOTE: vui::video_format represents the video format before
   * being encoded such as PAL, NTSC, SECAM, and MAC. That's not much informal
   * and can be inferred with resolution and framerate by any application.
   */
  /* Unspecified video format (5) */
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

  return TRUE;
}

static gboolean
gst_nv_h264_enc_set_pic_params (GstNvBaseEnc * enc, GstVideoCodecFrame * frame,
    NV_ENC_PIC_PARAMS * pic_params)
{
  /* encode whole picture in one single slice */
  pic_params->codecPicParams.h264PicParams.sliceMode = 0;
  pic_params->codecPicParams.h264PicParams.sliceModeData = 0;

  return TRUE;
}

static void
gst_nv_h264_enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstNvH264Enc *self = (GstNvH264Enc *) object;
  GstNvBaseEnc *nvenc = GST_NV_BASE_ENC (object);
  GstNvBaseEncClass *klass = GST_NV_BASE_ENC_GET_CLASS (object);
  GstNvEncDeviceCaps *device_caps = &klass->device_caps;
  gboolean reconfig = FALSE;

  switch (prop_id) {
    case PROP_AUD:
    {
      gboolean aud;

      aud = g_value_get_boolean (value);
      if (aud != self->aud) {
        self->aud = aud;
        reconfig = TRUE;
      }
      break;
    }
    case PROP_WEIGHTED_PRED:
      if (!device_caps->weighted_prediction) {
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      } else {
        nvenc->weighted_pred = g_value_get_boolean (value);
        reconfig = TRUE;
      }
      break;
    case PROP_VBV_BUFFER_SIZE:
      if (!device_caps->custom_vbv_bufsize) {
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      } else {
        nvenc->vbv_buffersize = g_value_get_uint (value);
        reconfig = TRUE;
      }
      break;
    case PROP_RC_LOOKAHEAD:
      if (!device_caps->lookahead) {
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      } else {
        nvenc->rc_lookahead = g_value_get_uint (value);
        reconfig = TRUE;
      }
      break;
    case PROP_TEMPORAL_AQ:
      if (!device_caps->temporal_aq) {
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      } else {
        nvenc->temporal_aq = g_value_get_boolean (value);
        reconfig = TRUE;
      }
      break;
    case PROP_BFRAMES:
      if (!device_caps->bframes) {
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      } else {
        nvenc->bframes = g_value_get_uint (value);
        reconfig = TRUE;
      }
      break;
    case PROP_B_ADAPT:
      if (!device_caps->bframes) {
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      } else {
        nvenc->b_adapt = g_value_get_boolean (value);
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  if (reconfig)
    gst_nv_base_enc_schedule_reconfig (GST_NV_BASE_ENC (self));
}

static void
gst_nv_h264_enc_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstNvH264Enc *self = (GstNvH264Enc *) object;
  GstNvBaseEnc *nvenc = GST_NV_BASE_ENC (object);
  GstNvBaseEncClass *klass = GST_NV_BASE_ENC_GET_CLASS (object);
  GstNvEncDeviceCaps *device_caps = &klass->device_caps;

  switch (prop_id) {
    case PROP_AUD:
      g_value_set_boolean (value, self->aud);
      break;
    case PROP_WEIGHTED_PRED:
      if (!device_caps->weighted_prediction) {
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      } else {
        g_value_set_boolean (value, nvenc->weighted_pred);
      }
      break;
    case PROP_VBV_BUFFER_SIZE:
      if (!device_caps->custom_vbv_bufsize) {
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      } else {
        g_value_set_uint (value, nvenc->vbv_buffersize);
      }
      break;
    case PROP_RC_LOOKAHEAD:
      if (!device_caps->lookahead) {
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      } else {
        g_value_set_uint (value, nvenc->rc_lookahead);
      }
      break;
    case PROP_TEMPORAL_AQ:
      if (!device_caps->temporal_aq) {
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      } else {
        g_value_set_boolean (value, nvenc->temporal_aq);
      }
      break;
    case PROP_BFRAMES:
      if (!device_caps->bframes) {
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      } else {
        g_value_set_uint (value, nvenc->bframes);
      }
      break;
    case PROP_B_ADAPT:
      if (!device_caps->bframes) {
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      } else {
        g_value_set_boolean (value, nvenc->b_adapt);
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

void
gst_nv_h264_enc_register (GstPlugin * plugin, guint device_id, guint rank,
    GstCaps * sink_caps, GstCaps * src_caps, GstNvEncDeviceCaps * device_caps)
{
  GType parent_type;
  GType type;
  gchar *type_name;
  gchar *feature_name;
  GstNvH264EncClassData *cdata;
  gboolean is_default = TRUE;
  GTypeInfo type_info = {
    sizeof (GstNvH264EncClass),
    NULL,
    NULL,
    (GClassInitFunc) gst_nv_h264_enc_class_init,
    NULL,
    NULL,
    sizeof (GstNvH264Enc),
    0,
    (GInstanceInitFunc) gst_nv_h264_enc_init,
  };

  parent_type = gst_nv_base_enc_register ("H264", device_id, device_caps);

  cdata = g_new0 (GstNvH264EncClassData, 1);
  cdata->sink_caps = gst_caps_ref (sink_caps);
  cdata->src_caps = gst_caps_ref (src_caps);
  type_info.class_data = cdata;
  /* class data will be leaked if the element never gets instantiated */
  GST_MINI_OBJECT_FLAG_SET (sink_caps, GST_MINI_OBJECT_FLAG_MAY_BE_LEAKED);
  GST_MINI_OBJECT_FLAG_SET (src_caps, GST_MINI_OBJECT_FLAG_MAY_BE_LEAKED);

  type_name = g_strdup ("GstNvH264Enc");
  feature_name = g_strdup ("nvh264enc");

  if (g_type_from_name (type_name) != 0) {
    g_free (type_name);
    g_free (feature_name);
    type_name = g_strdup_printf ("GstNvH264Device%dEnc", device_id);
    feature_name = g_strdup_printf ("nvh264device%denc", device_id);
    is_default = FALSE;
  }

  cdata->is_default = is_default;
  type = g_type_register_static (parent_type, type_name, &type_info, 0);

  /* make lower rank than default device */
  if (rank > 0 && !is_default)
    rank--;

  if (!gst_element_register (plugin, feature_name, rank, type))
    GST_WARNING ("Failed to register plugin '%s'", type_name);

  g_free (type_name);
  g_free (feature_name);
}
