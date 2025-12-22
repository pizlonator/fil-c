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

/**
 * SECTION:element-qsvvp9enc
 * @title: qsvvp9enc
 *
 * Intel Quick Sync VP9 encoder
 *
 * ## Example launch line
 * ```
 * gst-launch-1.0 videotestsrc ! qsvvp9enc ! vp9parse ! matroskamux ! filesink location=out.mkv
 * ```
 *
 * Since: 1.22
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstqsvvp9enc.h"
#include <vector>
#include <string>
#include <set>
#include <string.h>

#ifdef G_OS_WIN32
#include <gst/d3d11/gstd3d11.h>
#else
#include <gst/va/gstva.h>
#endif

GST_DEBUG_CATEGORY_STATIC (gst_qsv_vp9_enc_debug);
#define GST_CAT_DEFAULT gst_qsv_vp9_enc_debug

/**
 * GstQsvVP9EncRateControl:
 *
 * Since: 1.22
 */
#define GST_TYPE_QSV_VP9_ENC_RATE_CONTROL (gst_qsv_vp9_enc_rate_control_get_type ())
static GType
gst_qsv_vp9_enc_rate_control_get_type (void)
{
  static GType rate_control_type = 0;
  static const GEnumValue rate_controls[] = {
    /**
     * GstQsvVP9EncRateControl::cbr:
     *
     * Since: 1.22
     */
    {MFX_RATECONTROL_CBR, "Constant Bitrate", "cbr"},

    /**
     * GstQsvVP9EncRateControl::vbr:
     *
     * Since: 1.22
     */
    {MFX_RATECONTROL_VBR, "Variable Bitrate", "vbr"},

    /**
     * GstQsvVP9EncRateControl::cqp:
     *
     * Since: 1.22
     */
    {MFX_RATECONTROL_CQP, "Constant Quantizer", "cqp"},

    /**
     * GstQsvVP9EncRateControl::icq:
     *
     * Since: 1.22
     */
    {MFX_RATECONTROL_ICQ, "Intelligent CQP", "icq"},
    {0, nullptr, nullptr}
  };

  GST_QSV_CALL_ONCE_BEGIN {
    rate_control_type =
        g_enum_register_static ("GstQsvVP9EncRateControl", rate_controls);
  } GST_QSV_CALL_ONCE_END;

  return rate_control_type;
}

enum
{
  PROP_0,
  PROP_QP_I,
  PROP_QP_P,
  PROP_GOP_SIZE,
  PROP_REF_FRAMES,
  PROP_BITRATE,
  PROP_MAX_BITRATE,
  PROP_RATE_CONTROL,
  PROP_ICQ_QUALITY,
};

#define DEFAULT_QP 0
#define DEFAULT_GOP_SIZE 0
#define DEFAULT_REF_FRAMES 1
#define DEFAULT_BITRATE 2000
#define DEFAULT_MAX_BITRATE 0
#define DEFAULT_RATE_CONTROL MFX_RATECONTROL_VBR
#define DEFAULT_IQC_QUALITY 0

#define DOC_SINK_CAPS_COMM \
    "format = (string) { NV12, P010_10LE, VUYA, Y410 }, " \
    "width = (int) [16, 8192 ], " \
    "height = (int) [16, 8192 ]"

#define DOC_SINK_CAPS \
    "video/x-raw(memory:D3D11Memory), " DOC_SINK_CAPS_COMM "; " \
    "video/x-raw(memory:VAMemory), " DOC_SINK_CAPS_COMM "; " \
    "video/x-raw, " DOC_SINK_CAPS_COMM

#define DOC_SRC_CAPS \
    "video/x-vp9, width = (int) [16, 8192 ], height = (int) [16, 8192 ], " \
    "profile = (string) { 0, 2, 1, 3 }"

typedef struct _GstQsvVP9EncClassData
{
  GstCaps *sink_caps;
  GstCaps *src_caps;
  guint impl_index;
  gint64 adapter_luid;
  gchar *display_path;
  gchar *description;
} GstQsvVP9EncClassData;

typedef struct _GstQsvVP9Enc
{
  GstQsvEncoder parent;

  mfxExtVP9Param vp9_param;

  mfxU16 profile;

  GMutex prop_lock;
  /* protected by prop_lock */
  gboolean bitrate_updated;
  gboolean property_updated;

  /* properties */
  guint qp_i;
  guint qp_p;
  guint gop_size;
  guint ref_frames;
  guint bitrate;
  guint max_bitrate;
  mfxU16 rate_control;
  guint icq_quality;
} GstQsvVP9Enc;

typedef struct _GstQsvVP9EncClass
{
  GstQsvEncoderClass parent_class;
} GstQsvVP9EncClass;

static GstElementClass *parent_class = nullptr;

#define GST_QSV_VP9_ENC(object) ((GstQsvVP9Enc *) (object))
#define GST_QSV_VP9_ENC_GET_CLASS(object) \
    (G_TYPE_INSTANCE_GET_CLASS ((object),G_TYPE_FROM_INSTANCE (object),GstQsvVP9EncClass))

static void gst_qsv_vp9_enc_finalize (GObject * object);
static void gst_qsv_vp9_enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_qsv_vp9_enc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static GstCaps *gst_qsv_vp9_enc_getcaps (GstVideoEncoder * encoder,
    GstCaps * filter);

static gboolean gst_qsv_vp9_enc_set_format (GstQsvEncoder * encoder,
    GstVideoCodecState * state, mfxVideoParam * param,
    GPtrArray * extra_params);
static gboolean gst_qsv_vp9_enc_set_output_state (GstQsvEncoder * encoder,
    GstVideoCodecState * state, mfxSession session);
static GstQsvEncoderReconfigure
gst_qsv_vp9_enc_check_reconfigure (GstQsvEncoder * encoder, mfxSession session,
    mfxVideoParam * param, GPtrArray * extra_params);

static void
gst_qsv_vp9_enc_class_init (GstQsvVP9EncClass * klass, gpointer data)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstVideoEncoderClass *encoder_class = GST_VIDEO_ENCODER_CLASS (klass);
  GstQsvEncoderClass *qsvenc_class = GST_QSV_ENCODER_CLASS (klass);
  GstQsvVP9EncClassData *cdata = (GstQsvVP9EncClassData *) data;
  GstPadTemplate *pad_templ;
  GstCaps *doc_caps;

  qsvenc_class->codec_id = MFX_CODEC_VP9;
  qsvenc_class->impl_index = cdata->impl_index;
  qsvenc_class->adapter_luid = cdata->adapter_luid;
  qsvenc_class->display_path = cdata->display_path;

  object_class->finalize = gst_qsv_vp9_enc_finalize;
  object_class->set_property = gst_qsv_vp9_enc_set_property;
  object_class->get_property = gst_qsv_vp9_enc_get_property;

  g_object_class_install_property (object_class, PROP_QP_I,
      g_param_spec_uint ("qp-i", "QP I",
          "Constant quantizer for I frames (0: default)",
          0, 255, DEFAULT_QP, (GParamFlags)
          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (object_class, PROP_QP_P,
      g_param_spec_uint ("qp-p", "QP P",
          "Constant quantizer for P frames (0: default)",
          0, 255, DEFAULT_QP, (GParamFlags)
          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (object_class, PROP_GOP_SIZE,
      g_param_spec_uint ("gop-size", "GOP Size",
          "Number of pictures within a GOP (0: unspecified)",
          0, G_MAXINT, DEFAULT_GOP_SIZE, (GParamFlags)
          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (object_class, PROP_REF_FRAMES,
      g_param_spec_uint ("ref-frames", "Reference Frames",
          "Number of reference frames (0: unspecified)",
          0, 3, DEFAULT_REF_FRAMES, (GParamFlags)
          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (object_class, PROP_BITRATE,
      g_param_spec_uint ("bitrate", "Bitrate",
          "Target bitrate in kbit/sec, Ignored when selected rate-control mode "
          "is constant QP variants (i.e., \"cqp\" and \"icq\")",
          0, G_MAXUINT16, DEFAULT_BITRATE, (GParamFlags)
          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (object_class, PROP_MAX_BITRATE,
      g_param_spec_uint ("max-bitrate", "Max Bitrate",
          "Maximum bitrate in kbit/sec, Ignored when selected rate-control mode "
          "is constant QP variants (i.e., \"cqp\" and \"icq\")",
          0, G_MAXUINT16, DEFAULT_MAX_BITRATE, (GParamFlags)
          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (object_class, PROP_RATE_CONTROL,
      g_param_spec_enum ("rate-control", "Rate Control",
          "Rate Control Method", GST_TYPE_QSV_VP9_ENC_RATE_CONTROL,
          DEFAULT_RATE_CONTROL,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (object_class, PROP_ICQ_QUALITY,
      g_param_spec_uint ("icq-quality", "ICQ Quality",
          "Intelligent Constant Quality for \"icq\" rate-control (0: default)",
          0, 255, DEFAULT_IQC_QUALITY, (GParamFlags)
          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  parent_class = (GstElementClass *) g_type_class_peek_parent (klass);

#ifdef G_OS_WIN32
  std::string long_name = "Intel Quick Sync Video " +
      std::string (cdata->description) + " VP9 Encoder";

  gst_element_class_set_metadata (element_class, long_name.c_str (),
      "Codec/Encoder/Video/Hardware",
      "Intel Quick Sync Video VP9 Encoder",
      "Seungha Yang <seungha@centricular.com>");
#else
  gst_element_class_set_static_metadata (element_class,
      "Intel Quick Sync Video VP9 Encoder",
      "Codec/Encoder/Video/Hardware",
      "Intel Quick Sync Video VP9 Encoder",
      "Seungha Yang <seungha@centricular.com>");
#endif

  pad_templ = gst_pad_template_new ("sink",
      GST_PAD_SINK, GST_PAD_ALWAYS, cdata->sink_caps);
  doc_caps = gst_caps_from_string (DOC_SINK_CAPS);
  gst_pad_template_set_documentation_caps (pad_templ, doc_caps);
  gst_caps_unref (doc_caps);
  gst_element_class_add_pad_template (element_class, pad_templ);

  pad_templ = gst_pad_template_new ("src",
      GST_PAD_SRC, GST_PAD_ALWAYS, cdata->src_caps);
  doc_caps = gst_caps_from_string (DOC_SRC_CAPS);
  gst_pad_template_set_documentation_caps (pad_templ, doc_caps);
  gst_caps_unref (doc_caps);
  gst_element_class_add_pad_template (element_class, pad_templ);

  encoder_class->getcaps = GST_DEBUG_FUNCPTR (gst_qsv_vp9_enc_getcaps);

  qsvenc_class->set_format = GST_DEBUG_FUNCPTR (gst_qsv_vp9_enc_set_format);
  qsvenc_class->set_output_state =
      GST_DEBUG_FUNCPTR (gst_qsv_vp9_enc_set_output_state);
  qsvenc_class->check_reconfigure =
      GST_DEBUG_FUNCPTR (gst_qsv_vp9_enc_check_reconfigure);

  gst_type_mark_as_plugin_api (GST_TYPE_QSV_VP9_ENC_RATE_CONTROL,
      (GstPluginAPIFlags) 0);

  gst_caps_unref (cdata->sink_caps);
  gst_caps_unref (cdata->src_caps);
  g_free (cdata->description);
  g_free (cdata);
}

static void
gst_qsv_vp9_enc_init (GstQsvVP9Enc * self)
{
  self->qp_i = DEFAULT_QP;
  self->qp_p = DEFAULT_QP;
  self->gop_size = DEFAULT_GOP_SIZE;
  self->ref_frames = DEFAULT_REF_FRAMES;
  self->bitrate = DEFAULT_BITRATE;
  self->max_bitrate = DEFAULT_MAX_BITRATE;
  self->rate_control = DEFAULT_RATE_CONTROL;
  self->icq_quality = DEFAULT_IQC_QUALITY;

  g_mutex_init (&self->prop_lock);
}

static void
gst_qsv_vp9_enc_finalize (GObject * object)
{
  GstQsvVP9Enc *self = GST_QSV_VP9_ENC (object);

  g_mutex_clear (&self->prop_lock);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_qsv_vp9_enc_check_update_uint (GstQsvVP9Enc * self, guint * old_val,
    guint new_val, gboolean is_bitrate_param)
{
  if (*old_val == new_val)
    return;

  *old_val = new_val;
  if (is_bitrate_param)
    self->bitrate_updated = TRUE;
  else
    self->property_updated = TRUE;
}

static void
gst_qsv_vp9_enc_check_update_enum (GstQsvVP9Enc * self, mfxU16 * old_val,
    gint new_val)
{
  if (*old_val == (mfxU16) new_val)
    return;

  *old_val = (mfxU16) new_val;
  self->property_updated = TRUE;
}

static void
gst_qsv_vp9_enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstQsvVP9Enc *self = GST_QSV_VP9_ENC (object);

  g_mutex_lock (&self->prop_lock);
  switch (prop_id) {
    case PROP_QP_I:
      gst_qsv_vp9_enc_check_update_uint (self, &self->qp_i,
          g_value_get_uint (value), TRUE);
      break;
    case PROP_QP_P:
      gst_qsv_vp9_enc_check_update_uint (self, &self->qp_p,
          g_value_get_uint (value), TRUE);
      break;
    case PROP_GOP_SIZE:
      gst_qsv_vp9_enc_check_update_uint (self, &self->gop_size,
          g_value_get_uint (value), FALSE);
      break;
    case PROP_REF_FRAMES:
      gst_qsv_vp9_enc_check_update_uint (self, &self->ref_frames,
          g_value_get_uint (value), FALSE);
      break;
    case PROP_BITRATE:
      gst_qsv_vp9_enc_check_update_uint (self, &self->bitrate,
          g_value_get_uint (value), TRUE);
      break;
    case PROP_MAX_BITRATE:
      gst_qsv_vp9_enc_check_update_uint (self, &self->max_bitrate,
          g_value_get_uint (value), TRUE);
      break;
    case PROP_RATE_CONTROL:
      gst_qsv_vp9_enc_check_update_enum (self, &self->rate_control,
          g_value_get_enum (value));
      break;
    case PROP_ICQ_QUALITY:
      gst_qsv_vp9_enc_check_update_uint (self, &self->icq_quality,
          g_value_get_uint (value), FALSE);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  g_mutex_unlock (&self->prop_lock);
}

static void
gst_qsv_vp9_enc_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstQsvVP9Enc *self = GST_QSV_VP9_ENC (object);

  g_mutex_lock (&self->prop_lock);
  switch (prop_id) {
    case PROP_QP_I:
      g_value_set_uint (value, self->qp_i);
      break;
    case PROP_QP_P:
      g_value_set_uint (value, self->qp_p);
      break;
    case PROP_GOP_SIZE:
      g_value_set_uint (value, self->gop_size);
      break;
    case PROP_REF_FRAMES:
      g_value_set_uint (value, self->ref_frames);
      break;
    case PROP_BITRATE:
      g_value_set_uint (value, self->bitrate);
      break;
    case PROP_MAX_BITRATE:
      g_value_set_uint (value, self->max_bitrate);
      break;
    case PROP_RATE_CONTROL:
      g_value_set_enum (value, self->rate_control);
      break;
    case PROP_ICQ_QUALITY:
      g_value_set_uint (value, self->icq_quality);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  g_mutex_unlock (&self->prop_lock);
}

static GstCaps *
gst_qsv_vp9_enc_getcaps (GstVideoEncoder * encoder, GstCaps * filter)
{
  GstQsvVP9Enc *self = GST_QSV_VP9_ENC (encoder);
  GstCaps *allowed_caps;
  GstCaps *template_caps;
  GstCaps *supported_caps;
  std::set < std::string > downstream_profiles;

  allowed_caps = gst_pad_get_allowed_caps (encoder->srcpad);

  /* Shouldn't be any or empty though, just return template caps in this case */
  if (!allowed_caps || gst_caps_is_empty (allowed_caps) ||
      gst_caps_is_any (allowed_caps)) {
    gst_clear_caps (&allowed_caps);

    return gst_video_encoder_proxy_getcaps (encoder, nullptr, filter);
  }

  /* Check if downstream specified profile explicitly, then filter out
   * incompatible raw video format */
  for (guint i = 0; i < gst_caps_get_size (allowed_caps); i++) {
    const GValue *profile_value;
    const gchar *profile;
    GstStructure *s;

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
      if (g_strcmp0 (profile, "0") == 0 || g_strcmp0 (profile, "1") == 0 ||
          g_strcmp0 (profile, "2") == 0 || g_strcmp0 (profile, "3") == 0) {
        downstream_profiles.insert (profile);
      }
    }
  }

  GST_DEBUG_OBJECT (self, "Downstream specified %" G_GSIZE_FORMAT " profiles",
      downstream_profiles.size ());

  /* Caps returned by gst_pad_get_allowed_caps() should hold profile field
   * already */
  if (downstream_profiles.size () == 0) {
    GST_WARNING_OBJECT (self,
        "Allowed caps holds no profile field %" GST_PTR_FORMAT, allowed_caps);

    gst_clear_caps (&allowed_caps);

    return gst_video_encoder_proxy_getcaps (encoder, nullptr, filter);
  }

  gst_clear_caps (&allowed_caps);

  template_caps = gst_pad_get_pad_template_caps (encoder->sinkpad);
  template_caps = gst_caps_make_writable (template_caps);

  if (downstream_profiles.size () == 1) {
    std::string format;
    const std::string & profile = *downstream_profiles.begin ();

    if (profile == "0") {
      format = "NV12";
    } else if (profile == "1") {
      format = "VUYA";
    } else if (profile == "2") {
      format = "P010_10LE";
    } else if (profile == "3") {
      format = "Y410";
    } else {
      gst_clear_caps (&template_caps);
      g_assert_not_reached ();
      return nullptr;
    }

    gst_caps_set_simple (template_caps, "format", G_TYPE_STRING,
        format.c_str (), nullptr);
  } else {
    GValue formats = G_VALUE_INIT;

    g_value_init (&formats, GST_TYPE_LIST);

    /* *INDENT-OFF* */
    for (const auto & iter : downstream_profiles) {
      GValue val = G_VALUE_INIT;
      g_value_init (&val, G_TYPE_STRING);
      if (iter == "0") {
        g_value_set_static_string (&val, "NV12");
      } else if (iter == "1") {
        g_value_set_static_string (&val, "VUYA");
      } else if (iter == "2") {
        g_value_set_static_string (&val, "P010_10LE");
      } else if (iter == "3") {
        g_value_set_static_string (&val, "Y410");
      } else {
        g_value_unset (&val);
        gst_clear_caps (&template_caps);
        g_assert_not_reached ();
        return nullptr;
      }

      gst_value_list_append_and_take_value (&formats, &val);
    }
    /* *INDENT-ON* */

    gst_caps_set_value (template_caps, "format", &formats);
    g_value_unset (&formats);
  }

  supported_caps = gst_video_encoder_proxy_getcaps (encoder,
      template_caps, filter);
  gst_caps_unref (template_caps);

  GST_DEBUG_OBJECT (self, "Returning %" GST_PTR_FORMAT, supported_caps);

  return supported_caps;
}

typedef struct
{
  mfxU16 profile;
  const gchar *profile_str;
  const gchar *raw_format;
} VP9Profile;

static const VP9Profile profile_map[] = {
  /* preference order */
  {MFX_PROFILE_VP9_0, "0", "NV12"},
  {MFX_PROFILE_VP9_2, "2", "P010_10LE"},
  {MFX_PROFILE_VP9_1, "1", "VUYA"},
  {MFX_PROFILE_VP9_3, "3", "Y410"},
};

static const gchar *
gst_qsv_vp9_profile_to_string (mfxU16 profile)
{
  for (guint i = 0; i < G_N_ELEMENTS (profile_map); i++) {
    if (profile_map[i].profile == profile)
      return profile_map[i].profile_str;
  }

  return nullptr;
}

static void
gst_qsv_vp9_enc_init_vp9_param (mfxExtVP9Param * param)
{
  memset (param, 0, sizeof (mfxExtVP9Param));

  param->Header.BufferId = MFX_EXTBUFF_VP9_PARAM;
  param->Header.BufferSz = sizeof (mfxExtVP9Param);
}

static void
gst_qsv_vp9_enc_set_bitrate (GstQsvVP9Enc * self, mfxVideoParam * param)
{
  switch (param->mfx.RateControlMethod) {
    case MFX_RATECONTROL_CBR:
      param->mfx.TargetKbps = param->mfx.MaxKbps = self->bitrate;
      param->mfx.BRCParamMultiplier = 1;
      break;
    case MFX_RATECONTROL_VBR:
      param->mfx.TargetKbps = self->bitrate;
      param->mfx.MaxKbps = self->max_bitrate;
      param->mfx.BRCParamMultiplier = 1;
      break;
    case MFX_RATECONTROL_CQP:
      param->mfx.QPI = self->qp_i;
      param->mfx.QPP = self->qp_p;
      break;
    case MFX_RATECONTROL_ICQ:
      param->mfx.ICQQuality = self->icq_quality;
      break;
    default:
      GST_WARNING_OBJECT (self,
          "Unhandled rate-control method %d", self->rate_control);
      break;
  }
}

static gboolean
gst_qsv_vp9_enc_set_format (GstQsvEncoder * encoder,
    GstVideoCodecState * state, mfxVideoParam * param, GPtrArray * extra_params)
{
  GstQsvVP9Enc *self = GST_QSV_VP9_ENC (encoder);
  mfxU16 mfx_profile = MFX_PROFILE_UNKNOWN;
  GstVideoInfo *info = &state->info;
  mfxExtVP9Param *vp9_param;
  mfxFrameInfo *frame_info;
  GstVideoFormat format;

  frame_info = &param->mfx.FrameInfo;

  /* QSV expects this resolution, but actual coded frame resolution will be
   * signalled via mfxExtVP9Param */
  frame_info->Width = frame_info->CropW = GST_ROUND_UP_16 (info->width);
  frame_info->Height = frame_info->CropH = GST_ROUND_UP_16 (info->height);

  frame_info->PicStruct = MFX_PICSTRUCT_PROGRESSIVE;

  if (GST_VIDEO_INFO_FPS_N (info) > 0 && GST_VIDEO_INFO_FPS_D (info) > 0) {
    frame_info->FrameRateExtN = GST_VIDEO_INFO_FPS_N (info);
    frame_info->FrameRateExtD = GST_VIDEO_INFO_FPS_D (info);
  } else {
    /* HACK: Same as x264enc */
    frame_info->FrameRateExtN = 25;
    frame_info->FrameRateExtD = 1;
  }

  frame_info->AspectRatioW = GST_VIDEO_INFO_PAR_N (info);
  frame_info->AspectRatioH = GST_VIDEO_INFO_PAR_D (info);

  format = GST_VIDEO_INFO_FORMAT (info);
  switch (format) {
    case GST_VIDEO_FORMAT_NV12:
      mfx_profile = MFX_PROFILE_VP9_0;
      break;
    case GST_VIDEO_FORMAT_VUYA:
      mfx_profile = MFX_PROFILE_VP9_1;
      break;
    case GST_VIDEO_FORMAT_P010_10LE:
      mfx_profile = MFX_PROFILE_VP9_2;
      break;
    case GST_VIDEO_FORMAT_Y410:
      mfx_profile = MFX_PROFILE_VP9_3;
      break;
    default:
      GST_ERROR_OBJECT (self, "Unexpected format %s",
          gst_video_format_to_string (format));
      return FALSE;
  }

  gst_qsv_frame_info_set_format (frame_info, format);

  gst_qsv_vp9_enc_init_vp9_param (&self->vp9_param);
  vp9_param = &self->vp9_param;

  vp9_param->FrameWidth = GST_VIDEO_INFO_WIDTH (info);
  vp9_param->FrameHeight = GST_VIDEO_INFO_HEIGHT (info);

  /* We will always output raw VP9 frames */
  vp9_param->WriteIVFHeaders = MFX_CODINGOPTION_OFF;

  g_mutex_lock (&self->prop_lock);
  param->mfx.CodecId = MFX_CODEC_VP9;
  param->mfx.CodecProfile = mfx_profile;
  param->mfx.GopRefDist = 1;
  param->mfx.GopPicSize = self->gop_size;
  param->mfx.RateControlMethod = self->rate_control;
  param->mfx.NumRefFrame = self->ref_frames;

  gst_qsv_vp9_enc_set_bitrate (self, param);

  g_ptr_array_add (extra_params, vp9_param);

  param->ExtParam = (mfxExtBuffer **) extra_params->pdata;
  param->NumExtParam = extra_params->len;

  self->bitrate_updated = FALSE;
  self->property_updated = FALSE;

  g_mutex_unlock (&self->prop_lock);

  return TRUE;
}

static gboolean
gst_qsv_vp9_enc_set_output_state (GstQsvEncoder * encoder,
    GstVideoCodecState * state, mfxSession session)
{
  GstQsvVP9Enc *self = GST_QSV_VP9_ENC (encoder);
  GstCaps *caps;
  GstTagList *tags;
  GstVideoCodecState *out_state;
  guint bitrate, max_bitrate;
  mfxVideoParam param;
  const gchar *profile_str;
  mfxStatus status;

  memset (&param, 0, sizeof (mfxVideoParam));
  status = MFXVideoENCODE_GetVideoParam (session, &param);
  if (status < MFX_ERR_NONE) {
    GST_ERROR_OBJECT (self, "Failed to get video param %d (%s)",
        QSV_STATUS_ARGS (status));
    return FALSE;
  } else if (status != MFX_ERR_NONE) {
    GST_WARNING_OBJECT (self, "GetVideoParam returned warning %d (%s)",
        QSV_STATUS_ARGS (status));
  }

  caps = gst_caps_from_string ("video/x-vp9");
  profile_str = gst_qsv_vp9_profile_to_string (param.mfx.CodecProfile);
  if (profile_str)
    gst_caps_set_simple (caps, "profile", G_TYPE_STRING, profile_str, nullptr);

  out_state = gst_video_encoder_set_output_state (GST_VIDEO_ENCODER (encoder),
      caps, state);
  gst_video_codec_state_unref (out_state);

  tags = gst_tag_list_new_empty ();
  gst_tag_list_add (tags, GST_TAG_MERGE_REPLACE, GST_TAG_ENCODER, "qsvvp9enc",
      nullptr);

  switch (param.mfx.RateControlMethod) {
    case MFX_RATECONTROL_CQP:
    case MFX_RATECONTROL_ICQ:
      /* We don't know target/max bitrate in this case */
      break;
    default:
      max_bitrate = (guint) param.mfx.MaxKbps;
      bitrate = (guint) param.mfx.TargetKbps;
      if (bitrate > 0) {
        gst_tag_list_add (tags, GST_TAG_MERGE_REPLACE,
            GST_TAG_NOMINAL_BITRATE, bitrate * 1000, nullptr);
      }

      if (max_bitrate > 0) {
        gst_tag_list_add (tags, GST_TAG_MERGE_REPLACE,
            GST_TAG_MAXIMUM_BITRATE, max_bitrate * 1000, nullptr);
      }
      break;
  }

  gst_video_encoder_merge_tags (GST_VIDEO_ENCODER (encoder),
      tags, GST_TAG_MERGE_REPLACE);
  gst_tag_list_unref (tags);

  return TRUE;
}

static GstQsvEncoderReconfigure
gst_qsv_vp9_enc_check_reconfigure (GstQsvEncoder * encoder, mfxSession session,
    mfxVideoParam * param, GPtrArray * extra_params)
{
  GstQsvVP9Enc *self = GST_QSV_VP9_ENC (encoder);
  GstQsvEncoderReconfigure ret = GST_QSV_ENCODER_RECONFIGURE_NONE;

  g_mutex_lock (&self->prop_lock);
  if (self->property_updated) {
    ret = GST_QSV_ENCODER_RECONFIGURE_FULL;
    goto done;
  }

  if (self->bitrate_updated) {
    /* VP9 does not support query with MFX_EXTBUFF_ENCODER_RESET_OPTION
     * Just return GST_QSV_ENCODER_RECONFIGURE_BITRATE here.
     * Baseclass will care error */
    gst_qsv_vp9_enc_set_bitrate (self, param);

    ret = GST_QSV_ENCODER_RECONFIGURE_BITRATE;
  }

done:
  self->property_updated = FALSE;
  self->bitrate_updated = FALSE;
  g_mutex_unlock (&self->prop_lock);

  return ret;
}

void
gst_qsv_vp9_enc_register (GstPlugin * plugin, guint rank, guint impl_index,
    GstObject * device, mfxSession session)
{
  mfxVideoParam param;
  mfxInfoMFX *mfx;
  std::vector < mfxU16 > supported_profiles;
  std::vector < std::string > supported_formats;
  GstQsvResolution max_resolution;
  mfxExtVP9Param vp9_param;
  mfxExtBuffer *ext_bufs[1];

  GST_DEBUG_CATEGORY_INIT (gst_qsv_vp9_enc_debug, "qsvvp9enc", 0, "qsvvp9enc");

  memset (&param, 0, sizeof (mfxVideoParam));
  memset (&max_resolution, 0, sizeof (GstQsvResolution));

  ext_bufs[0] = (mfxExtBuffer *) & vp9_param;

  param.AsyncDepth = 4;
  param.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;

  mfx = &param.mfx;
  mfx->LowPower = MFX_CODINGOPTION_UNKNOWN;
  mfx->CodecId = MFX_CODEC_VP9;

  mfx->FrameInfo.Width = mfx->FrameInfo.CropW = GST_ROUND_UP_16 (320);
  mfx->FrameInfo.Height = mfx->FrameInfo.CropH = GST_ROUND_UP_16 (240);
  mfx->FrameInfo.FrameRateExtN = 30;
  mfx->FrameInfo.FrameRateExtD = 1;
  mfx->FrameInfo.AspectRatioW = 1;
  mfx->FrameInfo.AspectRatioH = 1;
  mfx->FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;

  param.NumExtParam = 1;
  param.ExtParam = ext_bufs;

  for (guint i = 0; i < G_N_ELEMENTS (profile_map); i++) {
    GstVideoFormat format;

    mfx->CodecProfile = profile_map[i].profile;

    gst_qsv_vp9_enc_init_vp9_param (&vp9_param);
    vp9_param.FrameWidth = 320;
    vp9_param.FrameHeight = 240;

    vp9_param.WriteIVFHeaders = MFX_CODINGOPTION_OFF;

    switch (mfx->CodecProfile) {
      case MFX_PROFILE_VP9_0:
        format = GST_VIDEO_FORMAT_NV12;
        break;
      case MFX_PROFILE_VP9_1:
        format = GST_VIDEO_FORMAT_VUYA;
        break;
      case MFX_PROFILE_VP9_2:
        format = GST_VIDEO_FORMAT_P010_10LE;
        break;
      case MFX_PROFILE_VP9_3:
        format = GST_VIDEO_FORMAT_Y410;
        break;
      default:
        g_assert_not_reached ();
        return;
    }

    gst_qsv_frame_info_set_format (&mfx->FrameInfo, format);

    if (MFXVideoENCODE_Query (session, &param, &param) != MFX_ERR_NONE)
      continue;

    supported_profiles.push_back (profile_map[i].profile);
    supported_formats.push_back (profile_map[i].raw_format);
  }

  if (supported_profiles.empty ()) {
    GST_INFO ("Device doesn't support VP9 encoding");
    return;
  }

  mfx->CodecProfile = MFX_PROFILE_VP9_0;
  gst_qsv_frame_info_set_format (&mfx->FrameInfo, GST_VIDEO_FORMAT_NV12);

  /* Check max-resolution */
  for (guint i = 0; i < G_N_ELEMENTS (gst_qsv_resolutions); i++) {
    mfx->FrameInfo.Width = mfx->FrameInfo.CropW =
        GST_ROUND_UP_16 (gst_qsv_resolutions[i].width);
    mfx->FrameInfo.Height = mfx->FrameInfo.CropH =
        GST_ROUND_UP_16 (gst_qsv_resolutions[i].height);

    gst_qsv_vp9_enc_init_vp9_param (&vp9_param);

    vp9_param.FrameWidth = gst_qsv_resolutions[i].width;
    vp9_param.FrameHeight = gst_qsv_resolutions[i].height;

    vp9_param.WriteIVFHeaders = MFX_CODINGOPTION_OFF;

    if (MFXVideoENCODE_Query (session, &param, &param) != MFX_ERR_NONE)
      break;

    max_resolution.width = gst_qsv_resolutions[i].width;
    max_resolution.height = gst_qsv_resolutions[i].height;
  }

  if (max_resolution.width == 0 || max_resolution.height == 0)
    return;

  GST_INFO ("Maximum supported resolution: %dx%d",
      max_resolution.width, max_resolution.height);

  /* TODO: check supported rate-control methods and expose only supported
   * methods, since the device might not be able to support some of them */

  /* To cover both landscape and portrait,
   * select max value (width in this case) */
  guint resolution = MAX (max_resolution.width, max_resolution.height);
  std::string sink_caps_str = "video/x-raw";

  sink_caps_str += ", width=(int) [ 16, " + std::to_string (resolution) + " ]";
  sink_caps_str += ", height=(int) [ 16, " + std::to_string (resolution) + " ]";

  /* *INDENT-OFF* */
  if (supported_formats.size () > 1) {
    sink_caps_str += ", format=(string) { ";
    bool first = true;
    for (const auto &iter: supported_formats) {
      if (!first) {
        sink_caps_str += ", ";
      }

      sink_caps_str += iter;
      first = false;
    }
    sink_caps_str += " }";
  } else {
    sink_caps_str += ", format=(string) " + supported_formats[0];
  }
  /* *INDENT-ON* */

  GstCaps *sink_caps = gst_caps_from_string (sink_caps_str.c_str ());

  /* TODO: Add support for VA */
#ifdef G_OS_WIN32
  GstCaps *d3d11_caps = gst_caps_copy (sink_caps);
  GstCapsFeatures *caps_features =
      gst_caps_features_new (GST_CAPS_FEATURE_MEMORY_D3D11_MEMORY, nullptr);
  gst_caps_set_features_simple (d3d11_caps, caps_features);
  gst_caps_append (d3d11_caps, sink_caps);
  sink_caps = d3d11_caps;
#else
  GstCaps *va_caps = gst_caps_copy (sink_caps);
  GstCapsFeatures *caps_features =
      gst_caps_features_new (GST_CAPS_FEATURE_MEMORY_VA, nullptr);
  gst_caps_set_features_simple (va_caps, caps_features);
  gst_caps_append (va_caps, sink_caps);
  sink_caps = va_caps;
#endif

  std::string src_caps_str = "video/x-vp9";
  src_caps_str += ", width=(int) [ 16, " + std::to_string (resolution) + " ]";
  src_caps_str += ", height=(int) [ 16, " + std::to_string (resolution) + " ]";

  /* *INDENT-OFF* */
  if (supported_profiles.size () > 1) {
    src_caps_str += ", profile=(string) { ";
    bool first = true;
    for (const auto &iter: supported_profiles) {
      if (!first) {
        src_caps_str += ", ";
      }

      src_caps_str += gst_qsv_vp9_profile_to_string (iter);
      first = false;
    }
    src_caps_str += " }";
  } else {
    src_caps_str += ", profile=(string) ";
    src_caps_str += gst_qsv_vp9_profile_to_string (supported_profiles[0]);
  }
  /* *INDENT-ON* */

  GstCaps *src_caps = gst_caps_from_string (src_caps_str.c_str ());

  GST_MINI_OBJECT_FLAG_SET (sink_caps, GST_MINI_OBJECT_FLAG_MAY_BE_LEAKED);
  GST_MINI_OBJECT_FLAG_SET (src_caps, GST_MINI_OBJECT_FLAG_MAY_BE_LEAKED);

  GstQsvVP9EncClassData *cdata = g_new0 (GstQsvVP9EncClassData, 1);
  cdata->sink_caps = sink_caps;
  cdata->src_caps = src_caps;
  cdata->impl_index = impl_index;

#ifdef G_OS_WIN32
  g_object_get (device, "adapter-luid", &cdata->adapter_luid,
      "description", &cdata->description, nullptr);
#else
  g_object_get (device, "path", &cdata->display_path, nullptr);
#endif

  GType type;
  gchar *type_name;
  gchar *feature_name;
  GTypeInfo type_info = {
    sizeof (GstQsvVP9EncClass),
    nullptr,
    nullptr,
    (GClassInitFunc) gst_qsv_vp9_enc_class_init,
    nullptr,
    cdata,
    sizeof (GstQsvVP9Enc),
    0,
    (GInstanceInitFunc) gst_qsv_vp9_enc_init,
  };

  type_name = g_strdup ("GstQsvVP9Enc");
  feature_name = g_strdup ("qsvvp9enc");

  gint index = 0;
  while (g_type_from_name (type_name)) {
    index++;
    g_free (type_name);
    g_free (feature_name);
    type_name = g_strdup_printf ("GstQsvVP9Device%dEnc", index);
    feature_name = g_strdup_printf ("qsvvp9device%denc", index);
  }

  type = g_type_register_static (GST_TYPE_QSV_ENCODER, type_name, &type_info,
      (GTypeFlags) 0);

  if (rank > 0 && index != 0)
    rank--;

  if (index != 0)
    gst_element_type_set_skip_documentation (type);

  if (!gst_element_register (plugin, feature_name, rank, type))
    GST_WARNING ("Failed to register plugin '%s'", type_name);

  g_free (type_name);
  g_free (feature_name);
}
