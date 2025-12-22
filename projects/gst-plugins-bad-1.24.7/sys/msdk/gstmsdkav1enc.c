/*
 * GStreamer Intel MSDK plugin
 * Copyright (c) 2022 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * SECTION: element-msdkav1enc
 * @title: msdkav1enc
 * @short_description: Intel MSDK AV1 encoder
 *
 * AV1 video encoder based on Intel MFX
 *
 * ## Example launch line
 * ```
 * gst-launch-1.0 videotestsrc num-buffers=90 ! msdkav1enc ! av1parse ! matroskamux ! filesink location=output.webm
 * ```
 *
 * Since: 1.21
 *
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/allocators/gstdmabuf.h>

#include "gstmsdkav1enc.h"

GST_DEBUG_CATEGORY_EXTERN (gst_msdkav1enc_debug);
#define GST_CAT_DEFAULT gst_msdkav1enc_debug

#define GST_MSDKAV1ENC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), G_TYPE_FROM_INSTANCE (obj), GstMsdkAV1Enc))
#define GST_MSDKAV1ENC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), G_TYPE_FROM_CLASS (klass), GstMsdkAV1EncClass))
#define GST_IS_MSDKAV1ENC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), G_TYPE_FROM_INSTANCE (obj)))
#define GST_IS_MSDKAV1ENC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), G_TYPE_FROM_CLASS (klass)))

enum
{
  PROP_TILE_ROW = GST_MSDKENC_PROP_MAX,
  PROP_TILE_COL,
  PROP_B_PYRAMID,
  PROP_P_PYRAMID,
};

#define PROP_TILE_ROW_DEFAULT           1
#define PROP_TILE_COL_DEFAULT           1
#define PROP_B_PYRAMID_DEFAULT          MFX_B_REF_UNKNOWN
#define PROP_P_PYRAMID_DEFAULT          MFX_P_REF_DEFAULT

/* *INDENT-OFF* */
static const gchar *doc_sink_caps_str =
    GST_VIDEO_CAPS_MAKE ("{ NV12, P010_10LE }") " ;"
    GST_VIDEO_CAPS_MAKE_WITH_FEATURES ("memory:DMABuf",
        "{ NV12, P010_10LE }") " ;"
    GST_VIDEO_CAPS_MAKE_WITH_FEATURES ("memory:VAMemory", "{ NV12 }") " ;"
    GST_VIDEO_CAPS_MAKE_WITH_FEATURES ("memory:D3D11Memory", "{ NV12 }");
/* *INDENT-ON* */

static const gchar *doc_src_caps_str = "video/x-av1";

static GstElementClass *parent_class = NULL;

static gboolean
gst_msdkav1enc_set_format (GstMsdkEnc * encoder)
{
  GstMsdkAV1Enc *thiz = GST_MSDKAV1ENC (encoder);
  GstPad *srcpad;
  GstCaps *template_caps;
  GstCaps *allowed_caps = NULL;

  thiz->profile = MFX_PROFILE_AV1_MAIN;

  srcpad = GST_VIDEO_ENCODER_SRC_PAD (encoder);

  allowed_caps = gst_pad_get_allowed_caps (srcpad);
  if (!allowed_caps || gst_caps_is_empty (allowed_caps)) {
    if (allowed_caps)
      gst_caps_unref (allowed_caps);
    return FALSE;
  }

  template_caps = gst_pad_get_pad_template_caps (srcpad);

  /* If downstream has ANY caps let encoder decide profile and level */
  if (gst_caps_is_equal (allowed_caps, template_caps)) {
    GST_INFO_OBJECT (thiz,
        "downstream has ANY caps, profile/level set to auto");
  } else {
    GstStructure *s;
    const gchar *profile;

    allowed_caps = gst_caps_make_writable (allowed_caps);
    allowed_caps = gst_caps_fixate (allowed_caps);
    s = gst_caps_get_structure (allowed_caps, 0);
    profile = gst_structure_get_string (s, "profile");

    if (profile) {
      if (!g_strcmp0 (profile, "main"))
        thiz->profile = MFX_PROFILE_AV1_MAIN;
      else
        g_assert_not_reached ();
    }
  }

  gst_caps_unref (allowed_caps);
  gst_caps_unref (template_caps);

  return TRUE;
}

static gboolean
gst_msdkav1enc_configure (GstMsdkEnc * encoder)
{
  GstMsdkAV1Enc *av1enc = GST_MSDKAV1ENC (encoder);

  encoder->num_extra_frames = encoder->async_depth - 1;
  encoder->param.mfx.CodecId = MFX_CODEC_AV1;
  encoder->param.mfx.CodecLevel = 0;

  switch (encoder->param.mfx.FrameInfo.FourCC) {
    case MFX_FOURCC_NV12:
    case MFX_FOURCC_P010:
      encoder->param.mfx.CodecProfile = MFX_PROFILE_AV1_MAIN;
      break;

    default:
      g_assert_not_reached ();
      break;
  }

  /* Always turn on this flag for AV1 */
  /* encoder->param.mfx.LowPower = MFX_CODINGOPTION_ON; */

  /* Enable Extended coding options */
  if (av1enc->b_pyramid)
    encoder->option2.BRefType = MFX_B_REF_PYRAMID;

  if (av1enc->p_pyramid) {
    encoder->option3.PRefType = MFX_P_REF_PYRAMID;
    /* MFX_P_REF_PYRAMID is available for GopRefDist = 1 */
    encoder->param.mfx.GopRefDist = 1;
    /* SDK decides the DPB size for P pyramid */
    encoder->param.mfx.NumRefFrame = 0;
  }

  encoder->option3.GPB = MFX_CODINGOPTION_OFF;
  encoder->enable_extopt3 = TRUE;

  gst_msdkenc_ensure_extended_coding_options (encoder);

  memset (&av1enc->ext_av1_bs_param, 0, sizeof (av1enc->ext_av1_bs_param));
  av1enc->ext_av1_bs_param.Header.BufferId = MFX_EXTBUFF_AV1_BITSTREAM_PARAM;
  av1enc->ext_av1_bs_param.Header.BufferSz = sizeof (av1enc->ext_av1_bs_param);
  av1enc->ext_av1_bs_param.WriteIVFHeaders = MFX_CODINGOPTION_OFF;
  gst_msdkenc_add_extra_param (encoder,
      (mfxExtBuffer *) & av1enc->ext_av1_bs_param);

  memset (&av1enc->ext_av1_res_param, 0, sizeof (av1enc->ext_av1_res_param));
  av1enc->ext_av1_res_param.Header.BufferId = MFX_EXTBUFF_AV1_RESOLUTION_PARAM;
  av1enc->ext_av1_res_param.Header.BufferSz =
      sizeof (av1enc->ext_av1_res_param);
  av1enc->ext_av1_res_param.FrameWidth = encoder->param.mfx.FrameInfo.CropW;
  av1enc->ext_av1_res_param.FrameHeight = encoder->param.mfx.FrameInfo.CropH;
  gst_msdkenc_add_extra_param (encoder,
      (mfxExtBuffer *) & av1enc->ext_av1_res_param);

  memset (&av1enc->ext_av1_tile_param, 0, sizeof (av1enc->ext_av1_tile_param));
  av1enc->ext_av1_tile_param.Header.BufferId = MFX_EXTBUFF_AV1_TILE_PARAM;
  av1enc->ext_av1_tile_param.Header.BufferSz =
      sizeof (av1enc->ext_av1_tile_param);
  av1enc->ext_av1_tile_param.NumTileRows = av1enc->num_tile_rows;
  av1enc->ext_av1_tile_param.NumTileColumns = av1enc->num_tile_cols;
  gst_msdkenc_add_extra_param (encoder,
      (mfxExtBuffer *) & av1enc->ext_av1_tile_param);

  return TRUE;
}

static inline const gchar *
profile_to_string (gint profile)
{
  switch (profile) {
    case MFX_PROFILE_AV1_MAIN:
      return "main";
    default:
      break;
  }

  return NULL;
}

static GstCaps *
gst_msdkav1enc_set_src_caps (GstMsdkEnc * encoder)
{
  GstCaps *caps;
  GstStructure *structure;
  const gchar *profile;

  caps = gst_caps_new_empty_simple ("video/x-av1");
  structure = gst_caps_get_structure (caps, 0);

  profile = profile_to_string (encoder->param.mfx.CodecProfile);
  if (profile)
    gst_structure_set (structure, "profile", G_TYPE_STRING, profile, NULL);

  return caps;
}

static gboolean
gst_msdkav1enc_is_format_supported (GstMsdkEnc * encoder, GstVideoFormat format)
{
  if (format == GST_VIDEO_FORMAT_NV12 || format == GST_VIDEO_FORMAT_P010_10LE)
    return TRUE;

  return FALSE;
}

static void
gst_msdkav1enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstMsdkAV1Enc *thiz = GST_MSDKAV1ENC (object);

  if (gst_msdkenc_set_common_property (object, prop_id, value, pspec))
    return;

  GST_OBJECT_LOCK (thiz);

  switch (prop_id) {
    case PROP_TILE_ROW:
      thiz->num_tile_rows = g_value_get_uint (value);
      break;

    case PROP_TILE_COL:
      thiz->num_tile_cols = g_value_get_uint (value);
      break;

    case PROP_B_PYRAMID:
      thiz->b_pyramid = g_value_get_boolean (value);
      break;

    case PROP_P_PYRAMID:
      thiz->p_pyramid = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (thiz);
}

static void
gst_msdkav1enc_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstMsdkAV1Enc *thiz = GST_MSDKAV1ENC (object);

  if (gst_msdkenc_get_common_property (object, prop_id, value, pspec))
    return;

  GST_OBJECT_LOCK (thiz);

  switch (prop_id) {
    case PROP_TILE_ROW:
      g_value_set_uint (value, thiz->num_tile_rows);
      break;

    case PROP_TILE_COL:
      g_value_set_uint (value, thiz->num_tile_cols);
      break;

    case PROP_B_PYRAMID:
      g_value_set_boolean (value, thiz->b_pyramid);
      break;

    case PROP_P_PYRAMID:
      g_value_set_boolean (value, thiz->p_pyramid);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (thiz);
}

static void
gst_msdkav1enc_finalize (GObject * object)
{
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
_msdkav1enc_install_properties (GObjectClass * gobject_class,
    GstMsdkEncClass * encoder_class)
{
  gst_msdkenc_install_common_properties (encoder_class);

  g_object_class_install_property (gobject_class, PROP_TILE_ROW,
      g_param_spec_uint ("num-tile-rows",
          "number of rows for tiled encoding",
          "number of rows for tiled encoding", 1, 64,
          PROP_TILE_ROW_DEFAULT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_TILE_COL,
      g_param_spec_uint ("num-tile-cols",
          "number of columns for tiled encoding",
          "number of columns for tiled encoding", 1, 64,
          PROP_TILE_COL_DEFAULT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_B_PYRAMID,
      g_param_spec_boolean ("b-pyramid", "B-pyramid",
          "Enable B-Pyramid Reference structure", PROP_B_PYRAMID_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_P_PYRAMID,
      g_param_spec_boolean ("p-pyramid", "P-pyramid",
          "Enable P-Pyramid Reference structure", PROP_P_PYRAMID_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
gst_msdkav1enc_class_init (gpointer klass, gpointer data)
{
  GObjectClass *gobject_class;
  GstElementClass *element_class;
  GstMsdkEncClass *encoder_class;
  MsdkEncCData *cdata = data;

  parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  element_class = GST_ELEMENT_CLASS (klass);
  encoder_class = GST_MSDKENC_CLASS (klass);

  gobject_class->finalize = gst_msdkav1enc_finalize;
  gobject_class->set_property = gst_msdkav1enc_set_property;
  gobject_class->get_property = gst_msdkav1enc_get_property;

  encoder_class->set_format = gst_msdkav1enc_set_format;
  encoder_class->configure = gst_msdkav1enc_configure;
  encoder_class->set_src_caps = gst_msdkav1enc_set_src_caps;
  encoder_class->is_format_supported = gst_msdkav1enc_is_format_supported;
  encoder_class->qp_max = 255;
  encoder_class->qp_min = 0;

  _msdkav1enc_install_properties (gobject_class, encoder_class);

  gst_element_class_set_static_metadata (element_class,
      "Intel MSDK AV1 encoder",
      "Codec/Encoder/Video/Hardware",
      "AV1 video encoder based on Intel Media SDK",
      "Haihao Xiang <haihao.xiang@intel.com>, "
      "Mengkejiergeli Ba <mengkejiergeli.ba@intel.com>");

  gst_msdkcaps_pad_template_init (element_class,
      cdata->sink_caps, cdata->src_caps, doc_sink_caps_str, doc_src_caps_str);

  gst_caps_unref (cdata->sink_caps);
  gst_caps_unref (cdata->src_caps);
  g_free (cdata);
}

static void
gst_msdkav1enc_init (GTypeInstance * instance, gpointer g_class)
{
  GstMsdkAV1Enc *thiz = GST_MSDKAV1ENC (instance);
  thiz->num_tile_rows = PROP_TILE_ROW_DEFAULT;
  thiz->num_tile_cols = PROP_TILE_COL_DEFAULT;
  thiz->b_pyramid = PROP_B_PYRAMID_DEFAULT;
  thiz->p_pyramid = PROP_P_PYRAMID_DEFAULT;
}

gboolean
gst_msdkav1enc_register (GstPlugin * plugin,
    GstMsdkContext * context, GstCaps * sink_caps,
    GstCaps * src_caps, guint rank)
{
  GType type;
  MsdkEncCData *cdata;
  gchar *type_name, *feature_name;
  gboolean ret = FALSE;

  GTypeInfo type_info = {
    .class_size = sizeof (GstMsdkAV1EncClass),
    .class_init = gst_msdkav1enc_class_init,
    .instance_size = sizeof (GstMsdkAV1Enc),
    .instance_init = gst_msdkav1enc_init
  };

  cdata = g_new (MsdkEncCData, 1);
  cdata->sink_caps = gst_caps_ref (sink_caps);
  cdata->src_caps = gst_caps_ref (src_caps);

  GST_MINI_OBJECT_FLAG_SET (cdata->sink_caps,
      GST_MINI_OBJECT_FLAG_MAY_BE_LEAKED);
  GST_MINI_OBJECT_FLAG_SET (cdata->src_caps,
      GST_MINI_OBJECT_FLAG_MAY_BE_LEAKED);

  type_info.class_data = cdata;

  type_name = g_strdup ("GstMsdkAV1Enc");
  feature_name = g_strdup ("msdkav1enc");

  type = g_type_register_static (GST_TYPE_MSDKENC, type_name, &type_info, 0);
  if (type)
    ret = gst_element_register (plugin, feature_name, rank, type);

  g_free (type_name);
  g_free (feature_name);

  return ret;
}
