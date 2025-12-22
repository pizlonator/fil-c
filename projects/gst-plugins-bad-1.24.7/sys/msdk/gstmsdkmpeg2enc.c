/* GStreamer Intel MSDK plugin
 * Copyright (c) 2016, Oblong Industries, Inc.
 * All rights reserved.
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
 * SECTION:element-msdkmpeg2enc
 * @title: msdkmpeg2enc
 * @short_description: Intel MSDK MPEG2 encoder
 *
 * MPEG2 video encoder based on Intel MFX
 *
 * ## Example launch line
 * ```
 * gst-launch-1.0 videotestsrc num-buffers=90 ! msdkmpeg2enc ! mpegvideoparse ! filesink location=output.mpg
 * ```
 *
 * Since: 1.12
 *
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "gstmsdkmpeg2enc.h"

GST_DEBUG_CATEGORY_EXTERN (gst_msdkmpeg2enc_debug);
#define GST_CAT_DEFAULT gst_msdkmpeg2enc_debug

#define GST_MSDKMPEG2ENC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), G_TYPE_FROM_INSTANCE (obj), GstMsdkMPEG2Enc))
#define GST_MSDKMPEG2ENC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), G_TYPE_FROM_CLASS (klass), GstMsdkMPEG2EncClass))
#define GST_IS_MSDKMPEG2ENC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), G_TYPE_FROM_INSTANCE (obj)))
#define GST_IS_MSDKMPEG2ENC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), G_TYPE_FROM_CLASS (klass)))

/* *INDENT-OFF* */
static const gchar *doc_sink_caps_str =
    GST_VIDEO_CAPS_MAKE ("{ NV12 }") " ;"
    GST_VIDEO_CAPS_MAKE_WITH_FEATURES ("memory:DMABuf", "{ NV12 }") " ;"
    GST_VIDEO_CAPS_MAKE_WITH_FEATURES ("memory:VAMemory", "{ NV12 }") " ;"
    GST_VIDEO_CAPS_MAKE_WITH_FEATURES ("memory:D3D11Memory", "{ NV12 }");
/* *INDENT-ON* */

static const gchar *doc_src_caps_str = "video/mpeg";

static GstElementClass *parent_class = NULL;

static gboolean
gst_msdkmpeg2enc_set_format (GstMsdkEnc * encoder)
{
  GstMsdkMPEG2Enc *thiz = GST_MSDKMPEG2ENC (encoder);
  GstPad *srcpad;
  GstCaps *template_caps;
  GstCaps *allowed_caps = NULL;

  thiz->profile = 0;

  srcpad = GST_VIDEO_ENCODER_SRC_PAD (encoder);
  template_caps = gst_pad_get_pad_template_caps (srcpad);
  allowed_caps = gst_pad_get_allowed_caps (srcpad);

  /* If downstream has ANY caps let encoder decide profile and level */
  if (allowed_caps == template_caps) {
    GST_INFO_OBJECT (thiz,
        "downstream has ANY caps, profile/level set to auto");
  } else if (allowed_caps) {
    GstStructure *s;
    const gchar *profile;

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
      if (!g_strcmp0 (profile, "high")) {
        thiz->profile = MFX_PROFILE_MPEG2_HIGH;
      } else if (!g_strcmp0 (profile, "main")) {
        thiz->profile = MFX_PROFILE_MPEG2_MAIN;
      } else if (!g_strcmp0 (profile, "simple")) {
        thiz->profile = MFX_PROFILE_MPEG2_SIMPLE;
      } else {
        g_assert_not_reached ();
      }
    }

    gst_caps_unref (allowed_caps);
  }

  gst_caps_unref (template_caps);

  return TRUE;
}

static gboolean
gst_msdkmpeg2enc_configure (GstMsdkEnc * encoder)
{
  GstMsdkMPEG2Enc *thiz = GST_MSDKMPEG2ENC (encoder);

  encoder->param.mfx.CodecId = MFX_CODEC_MPEG2;
  encoder->param.mfx.CodecProfile = thiz->profile;
  encoder->param.mfx.CodecLevel = 0;

  /* Enable Extended Coding options */
  gst_msdkenc_ensure_extended_coding_options (encoder);

  return TRUE;
}

static inline const gchar *
profile_to_string (gint profile)
{
  switch (profile) {
    case MFX_PROFILE_MPEG2_HIGH:
      return "high";
    case MFX_PROFILE_MPEG2_MAIN:
      return "main";
    case MFX_PROFILE_MPEG2_SIMPLE:
      return "simple";
    default:
      break;
  }

  return NULL;
}

static GstCaps *
gst_msdkmpeg2enc_set_src_caps (GstMsdkEnc * encoder)
{
  GstCaps *caps;
  GstStructure *structure;
  const gchar *profile;

  caps = gst_caps_from_string ("video/mpeg, mpegversion=2, systemstream=false");
  structure = gst_caps_get_structure (caps, 0);

  profile = profile_to_string (encoder->param.mfx.CodecProfile);
  if (profile)
    gst_structure_set (structure, "profile", G_TYPE_STRING, profile, NULL);

  return caps;
}

static void
gst_msdkmpeg2enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstMsdkMPEG2Enc *thiz = GST_MSDKMPEG2ENC (object);

  if (!gst_msdkenc_set_common_property (object, prop_id, value, pspec))
    GST_WARNING_OBJECT (thiz, "Failed to set common encode property");
}

static void
gst_msdkmpeg2enc_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstMsdkMPEG2Enc *thiz = GST_MSDKMPEG2ENC (object);

  if (!gst_msdkenc_get_common_property (object, prop_id, value, pspec))
    GST_WARNING_OBJECT (thiz, "Failed to get common encode property");
}

static void
gst_msdkmpeg2enc_class_init (gpointer klass, gpointer data)
{
  GObjectClass *gobject_class;
  GstElementClass *element_class;
  GstMsdkEncClass *encoder_class;
  MsdkEncCData *cdata = data;

  parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  element_class = GST_ELEMENT_CLASS (klass);
  encoder_class = GST_MSDKENC_CLASS (klass);

  gobject_class->set_property = gst_msdkmpeg2enc_set_property;
  gobject_class->get_property = gst_msdkmpeg2enc_get_property;

  encoder_class->set_format = gst_msdkmpeg2enc_set_format;
  encoder_class->configure = gst_msdkmpeg2enc_configure;
  encoder_class->set_src_caps = gst_msdkmpeg2enc_set_src_caps;

  gst_msdkenc_install_common_properties (encoder_class);

  gst_element_class_set_static_metadata (element_class,
      "Intel MSDK MPEG2 encoder",
      "Codec/Encoder/Video/Hardware",
      "MPEG2 video encoder based on " MFX_API_SDK,
      "Josep Torra <jtorra@oblong.com>");

  gst_msdkcaps_pad_template_init (element_class,
      cdata->sink_caps, cdata->src_caps, doc_sink_caps_str, doc_src_caps_str);

  gst_caps_unref (cdata->sink_caps);
  gst_caps_unref (cdata->src_caps);
  g_free (cdata);
}

static void
gst_msdkmpeg2enc_init (GTypeInstance * instance, gpointer g_class)
{
}

gboolean
gst_msdkmpeg2enc_register (GstPlugin * plugin,
    GstMsdkContext * context, GstCaps * sink_caps,
    GstCaps * src_caps, guint rank)
{
  GType type;
  MsdkEncCData *cdata;
  gchar *type_name, *feature_name;
  gboolean ret = FALSE;

  GTypeInfo type_info = {
    .class_size = sizeof (GstMsdkMPEG2EncClass),
    .class_init = gst_msdkmpeg2enc_class_init,
    .instance_size = sizeof (GstMsdkMPEG2Enc),
    .instance_init = gst_msdkmpeg2enc_init
  };

  cdata = g_new (MsdkEncCData, 1);
  cdata->sink_caps = gst_caps_ref (sink_caps);
  cdata->src_caps = gst_caps_copy (src_caps);

  gst_caps_set_simple (cdata->src_caps,
      "mpegversion", G_TYPE_INT, 2,
      "systemstream", G_TYPE_BOOLEAN, FALSE, NULL);

  GST_MINI_OBJECT_FLAG_SET (cdata->sink_caps,
      GST_MINI_OBJECT_FLAG_MAY_BE_LEAKED);
  GST_MINI_OBJECT_FLAG_SET (cdata->src_caps,
      GST_MINI_OBJECT_FLAG_MAY_BE_LEAKED);

  type_info.class_data = cdata;

  type_name = g_strdup ("GstMsdkMPEG2Enc");
  feature_name = g_strdup ("msdkmpeg2enc");

  type = g_type_register_static (GST_TYPE_MSDKENC, type_name, &type_info, 0);
  if (type)
    ret = gst_element_register (plugin, feature_name, rank, type);

  g_free (type_name);
  g_free (feature_name);

  return ret;
}
