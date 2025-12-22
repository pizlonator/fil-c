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
 * SECTION:element-qsvh265enc
 * @title: qsvh265enc
 *
 * Intel Quick Sync H.265 encoder
 *
 * ## Example launch line
 * ```
 * gst-launch-1.0 videotestsrc ! qsvh265enc ! h265parse ! matroskamux ! filesink location=out.mkv
 * ```
 *
 * Since: 1.22
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstqsvh265enc.h"
#include <gst/base/gstbytewriter.h>
#include <vector>
#include <string>
#include <set>
#include <string.h>

#ifdef G_OS_WIN32
#include <gst/d3d11/gstd3d11.h>
#else
#include <gst/va/gstva.h>
#endif

GST_DEBUG_CATEGORY_STATIC (gst_qsv_h265_enc_debug);
#define GST_CAT_DEFAULT gst_qsv_h265_enc_debug

typedef enum
{
  GST_QSV_H265_ENC_SEI_INSERT,
  GST_QSV_H265_ENC_SEI_INSERT_AND_DROP,
  GST_QSV_H265_ENC_SEI_DISABLED,
} GstQsvH265EncSeiInsertMode;

/**
 * GstQsvH265EncSeiInsertMode:
 *
 * Since: 1.22
 */
#define GST_TYPE_QSV_H265_ENC_SEI_INSERT_MODE (gst_qsv_h265_enc_sei_insert_mode_get_type ())
static GType
gst_qsv_h265_enc_sei_insert_mode_get_type (void)
{
  static GType sei_insert_mode_type = 0;
  static const GEnumValue insert_modes[] = {
    /**
     * GstQsvH265EncSeiInsertMode::insert:
     *
     * Since: 1.22
     */
    {GST_QSV_H265_ENC_SEI_INSERT, "Insert SEI", "insert"},

    /**
     * GstQsvH265EncSeiInsertMode::insert-and-drop:
     *
     * Since: 1.22
     */
    {GST_QSV_H265_ENC_SEI_INSERT_AND_DROP,
          "Insert SEI and remove corresponding meta from output buffer",
        "insert-and-drop"},

    /**
     * GstQsvH265EncSeiInsertMode::disabled:
     *
     * Since: 1.22
     */
    {GST_QSV_H265_ENC_SEI_DISABLED, "Disable SEI insertion", "disabled"},
    {0, nullptr, nullptr}
  };

  GST_QSV_CALL_ONCE_BEGIN {
    sei_insert_mode_type =
        g_enum_register_static ("GstQsvH265EncSeiInsertMode", insert_modes);
  } GST_QSV_CALL_ONCE_END;

  return sei_insert_mode_type;
}

/**
 * GstQsvH265EncRateControl:
 *
 * Since: 1.22
 */
#define GST_TYPE_QSV_H265_ENC_RATE_CONTROL (gst_qsv_h265_enc_rate_control_get_type ())
static GType
gst_qsv_h265_enc_rate_control_get_type (void)
{
  static GType rate_control_type = 0;
  static const GEnumValue rate_controls[] = {
    /**
     * GstQsvH265EncSeiInsertMode::cbr:
     *
     * Since: 1.22
     */
    {MFX_RATECONTROL_CBR, "Constant Bitrate", "cbr"},

    /**
     * GstQsvH265EncSeiInsertMode::vbr:
     *
     * Since: 1.22
     */
    {MFX_RATECONTROL_VBR, "Variable Bitrate", "vbr"},

    /**
     * GstQsvH265EncSeiInsertMode::cqp:
     *
     * Since: 1.22
     */
    {MFX_RATECONTROL_CQP, "Constant Quantizer", "cqp"},

    /**
     * GstQsvH265EncSeiInsertMode::icq:
     *
     * Since: 1.22
     */
    {MFX_RATECONTROL_ICQ, "Intelligent CQP", "icq"},

    /**
     * GstQsvH265EncSeiInsertMode::vcm:
     *
     * Since: 1.22
     */
    {MFX_RATECONTROL_VCM, "Video Conferencing Mode (Non HRD compliant)", "vcm"},

    /**
     * GstQsvH265EncSeiInsertMode::qvbr:
     *
     * Since: 1.22
     */
    {MFX_RATECONTROL_QVBR, "VBR with CQP", "qvbr"},
    {0, nullptr, nullptr}
  };

  GST_QSV_CALL_ONCE_BEGIN {
    rate_control_type =
        g_enum_register_static ("GstQsvH265EncRateControl", rate_controls);
  } GST_QSV_CALL_ONCE_END;

  return rate_control_type;
}

enum
{
  PROP_0,
  PROP_MIN_QP_I,
  PROP_MIN_QP_P,
  PROP_MIN_QP_B,
  PROP_MAX_QP_I,
  PROP_MAX_QP_P,
  PROP_MAX_QP_B,
  PROP_QP_I,
  PROP_QP_P,
  PROP_QP_B,
  PROP_GOP_SIZE,
  PROP_IDR_INTERVAL,
  PROP_B_FRAMES,
  PROP_REF_FRAMES,
  PROP_BITRATE,
  PROP_MAX_BITRATE,
  PROP_RATE_CONTROL,
  PROP_ICQ_QUALITY,
  PROP_QVBR_QUALITY,
  PROP_DISABLE_HRD_CONFORMANCE,
  PROP_CC_INSERT,
};

#define DEFAULT_QP 0
#define DEFAULT_GOP_SIZE 30
#define DEFAULT_IDR_INTERVAL 1
#define DEFAULT_B_FRAMES 0
#define DEFAULT_REF_FRAMES 2
#define DEFAULT_BITRATE 2000
#define DEFAULT_MAX_BITRATE 0
#define DEFAULT_RATE_CONTROL MFX_RATECONTROL_VBR
#define DEFAULT_IQC_QUALITY 0
#define DEFAULT_QVBR_QUALITY 0
#define DEFAULT_DISABLE_HRD_CONFORMANCE FALSE
#define DEFAULT_CC_INSERT GST_QSV_H265_ENC_SEI_INSERT

#define DOC_SINK_CAPS_COMM \
    "format = (string) { NV12, P010_10LE }, " \
    "width = (int) [ 16, 8192 ], height = (int) [ 16, 8192 ]"

#define DOC_SINK_CAPS \
    "video/x-raw(memory:D3D11Memory), " DOC_SINK_CAPS_COMM "; " \
    "video/x-raw(memory:VAMemory), " DOC_SINK_CAPS_COMM "; " \
    "video/x-raw, " DOC_SINK_CAPS_COMM

#define DOC_SRC_CAPS \
    "video/x-h265, width = (int) [ 16, 8192 ], height = (int) [ 16, 8192 ], " \
    "stream-format = (string) byte-stream, alignment = (string) au, " \
    "profile = (string) { main, main-10 }"

typedef struct _GstQsvH265EncClassData
{
  GstCaps *sink_caps;
  GstCaps *src_caps;
  guint impl_index;
  gint64 adapter_luid;
  gchar *display_path;
  gchar *description;
  gboolean hdr10_aware;
} GstQsvH265EncClassData;

typedef struct _GstQsvH265Enc
{
  GstQsvEncoder parent;

  mfxExtVideoSignalInfo signal_info;
  mfxExtCodingOption option;
  mfxExtCodingOption2 option2;
  mfxExtCodingOption3 option3;
  mfxExtContentLightLevelInfo cll;
  mfxExtMasteringDisplayColourVolume mdcv;

  mfxU16 profile;

  GMutex prop_lock;
  /* protected by prop_lock */
  gboolean bitrate_updated;
  gboolean property_updated;

  /* properties */
  guint min_qp_i;
  guint min_qp_p;
  guint min_qp_b;
  guint max_qp_i;
  guint max_qp_p;
  guint max_qp_b;
  guint qp_i;
  guint qp_p;
  guint qp_b;
  guint gop_size;
  guint idr_interval;
  guint bframes;
  guint ref_frames;
  guint bitrate;
  guint max_bitrate;
  mfxU16 rate_control;
  guint icq_quality;
  guint qvbr_quality;
  gboolean disable_hrd_conformance;
  GstQsvH265EncSeiInsertMode cc_insert;
} GstQsvH265Enc;

typedef struct _GstQsvH265EncClass
{
  GstQsvEncoderClass parent_class;

  gboolean hdr10_aware;
} GstQsvH265EncClass;

static GstElementClass *parent_class = nullptr;

#define GST_QSV_H265_ENC(object) ((GstQsvH265Enc *) (object))
#define GST_QSV_H265_ENC_GET_CLASS(object) \
    (G_TYPE_INSTANCE_GET_CLASS ((object),G_TYPE_FROM_INSTANCE (object),GstQsvH265EncClass))

static void gst_qsv_h265_enc_finalize (GObject * object);
static void gst_qsv_h265_enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_qsv_h265_enc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_qsv_h265_enc_start (GstVideoEncoder * encoder);
static gboolean gst_qsv_h265_enc_transform_meta (GstVideoEncoder * encoder,
    GstVideoCodecFrame * frame, GstMeta * meta);
static GstCaps *gst_qsv_h265_enc_getcaps (GstVideoEncoder * encoder,
    GstCaps * filter);

static gboolean gst_qsv_h265_enc_set_format (GstQsvEncoder * encoder,
    GstVideoCodecState * state, mfxVideoParam * param,
    GPtrArray * extra_params);
static gboolean gst_qsv_h265_enc_set_output_state (GstQsvEncoder * encoder,
    GstVideoCodecState * state, mfxSession session);
static gboolean gst_qsv_h265_enc_attach_payload (GstQsvEncoder * encoder,
    GstVideoCodecFrame * frame, GPtrArray * payload);
static GstBuffer *gst_qsv_h265_enc_create_output_buffer (GstQsvEncoder *
    encoder, mfxBitstream * bitstream);
static GstQsvEncoderReconfigure
gst_qsv_h265_enc_check_reconfigure (GstQsvEncoder * encoder, mfxSession session,
    mfxVideoParam * param, GPtrArray * extra_params);

static void
gst_qsv_h265_enc_class_init (GstQsvH265EncClass * klass, gpointer data)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstVideoEncoderClass *encoder_class = GST_VIDEO_ENCODER_CLASS (klass);
  GstQsvEncoderClass *qsvenc_class = GST_QSV_ENCODER_CLASS (klass);
  GstQsvH265EncClassData *cdata = (GstQsvH265EncClassData *) data;
  GstPadTemplate *pad_templ;
  GstCaps *doc_caps;

  qsvenc_class->codec_id = MFX_CODEC_AVC;
  qsvenc_class->impl_index = cdata->impl_index;
  qsvenc_class->adapter_luid = cdata->adapter_luid;
  qsvenc_class->display_path = cdata->display_path;

  object_class->finalize = gst_qsv_h265_enc_finalize;
  object_class->set_property = gst_qsv_h265_enc_set_property;
  object_class->get_property = gst_qsv_h265_enc_get_property;

  g_object_class_install_property (object_class, PROP_MIN_QP_I,
      g_param_spec_uint ("min-qp-i", "Min QP I",
          "Minimum allowed QP value for I-frame types (0: default)",
          0, 51, DEFAULT_QP, (GParamFlags)
          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (object_class, PROP_MIN_QP_P,
      g_param_spec_uint ("min-qp-p", "Min QP P",
          "Minimum allowed QP value for P-frame types (0: default)",
          0, 51, DEFAULT_QP, (GParamFlags)
          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (object_class, PROP_MIN_QP_B,
      g_param_spec_uint ("min-qp-b", "Min QP B",
          "Minimum allowed QP value for B-frame types (0: default)",
          0, 51, DEFAULT_QP, (GParamFlags)
          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (object_class, PROP_MAX_QP_I,
      g_param_spec_uint ("max-qp-i", "Max QP I",
          "Maximum allowed QP value for I-frame types (0: default)",
          0, 51, DEFAULT_QP, (GParamFlags)
          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (object_class, PROP_MAX_QP_P,
      g_param_spec_uint ("max-qp-p", "Max QP P",
          "Maximum allowed QP value for P-frame types (0: default)",
          0, 51, DEFAULT_QP, (GParamFlags)
          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (object_class, PROP_MAX_QP_B,
      g_param_spec_uint ("max-qp-b", "Max QP B",
          "Maximum allowed QP value for B-frame types (0: default)",
          0, 51, DEFAULT_QP, (GParamFlags)
          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (object_class, PROP_QP_I,
      g_param_spec_uint ("qp-i", "QP I",
          "Constant quantizer for I frames (0: default)",
          0, 51, DEFAULT_QP, (GParamFlags)
          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (object_class, PROP_QP_P,
      g_param_spec_uint ("qp-p", "QP P",
          "Constant quantizer for P frames (0: default)",
          0, 51, DEFAULT_QP, (GParamFlags)
          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (object_class, PROP_QP_B,
      g_param_spec_uint ("qp-b", "QP B",
          "Constant quantizer for B frames (0: default)",
          0, 51, DEFAULT_QP, (GParamFlags)
          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (object_class, PROP_GOP_SIZE,
      g_param_spec_uint ("gop-size", "GOP Size",
          "Number of pictures within a GOP (0: unspecified)",
          0, G_MAXUSHORT, DEFAULT_GOP_SIZE, (GParamFlags)
          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (object_class, PROP_IDR_INTERVAL,
      g_param_spec_uint ("idr-interval", "IDR interval",
          "IDR-frame interval in terms of I-frames. "
          "0: only first I-frame is is an IDR frame, "
          "1: every I-frame is an IDR frame, "
          "N: \"N - 1\" I-frames are inserted between IDR-frames",
          0, G_MAXUSHORT, DEFAULT_IDR_INTERVAL, (GParamFlags)
          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (object_class, PROP_B_FRAMES,
      g_param_spec_uint ("b-frames", "B Frames",
          "Number of B frames between I and P frames",
          0, G_MAXUSHORT, DEFAULT_B_FRAMES, (GParamFlags)
          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (object_class, PROP_REF_FRAMES,
      g_param_spec_uint ("ref-frames", "Reference Frames",
          "Number of reference frames (0: unspecified)",
          0, 16, DEFAULT_REF_FRAMES, (GParamFlags)
          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (object_class, PROP_BITRATE,
      g_param_spec_uint ("bitrate", "Bitrate",
          "Target bitrate in kbit/sec, Ignored when selected rate-control mode "
          "is constant QP variants (i.e., \"cqp\" and \"icq\")",
          0, G_MAXINT, DEFAULT_BITRATE, (GParamFlags)
          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (object_class, PROP_MAX_BITRATE,
      g_param_spec_uint ("max-bitrate", "Max Bitrate",
          "Maximum bitrate in kbit/sec, Ignored when selected rate-control mode "
          "is constant QP variants (i.e., \"cqp\" and \"icq\")",
          0, G_MAXINT, DEFAULT_MAX_BITRATE, (GParamFlags)
          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (object_class, PROP_RATE_CONTROL,
      g_param_spec_enum ("rate-control", "Rate Control",
          "Rate Control Method", GST_TYPE_QSV_H265_ENC_RATE_CONTROL,
          DEFAULT_RATE_CONTROL,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (object_class, PROP_ICQ_QUALITY,
      g_param_spec_uint ("icq-quality", "ICQ Quality",
          "Intelligent Constant Quality for \"icq\" rate-control (0: default)",
          0, 51, DEFAULT_IQC_QUALITY, (GParamFlags)
          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (object_class, PROP_QVBR_QUALITY,
      g_param_spec_uint ("qvbr-quality", "QVBR Quality",
          "Quality level used for \"qvbr\" rate-control mode (0: default)",
          0, 51, DEFAULT_QVBR_QUALITY, (GParamFlags)
          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (object_class, PROP_DISABLE_HRD_CONFORMANCE,
      g_param_spec_boolean ("disable-hrd-conformance",
          "Disable HRD Conformance", "Allow NAL HRD non-conformant stream",
          DEFAULT_DISABLE_HRD_CONFORMANCE,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
  g_object_class_install_property (object_class, PROP_CC_INSERT,
      g_param_spec_enum ("cc-insert", "Closed Caption Insert",
          "Closed Caption Insert mode. "
          "Only CEA-708 RAW format is supported for now",
          GST_TYPE_QSV_H265_ENC_SEI_INSERT_MODE, DEFAULT_CC_INSERT,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  parent_class = (GstElementClass *) g_type_class_peek_parent (klass);

#ifdef G_OS_WIN32
  std::string long_name = "Intel Quick Sync Video " +
      std::string (cdata->description) + " H.265 Encoder";

  gst_element_class_set_metadata (element_class, long_name.c_str (),
      "Codec/Encoder/Video/Hardware",
      "Intel Quick Sync Video H.265 Encoder",
      "Seungha Yang <seungha@centricular.com>");
#else
  gst_element_class_set_static_metadata (element_class,
      "Intel Quick Sync Video H.265 Encoder",
      "Codec/Encoder/Video/Hardware",
      "Intel Quick Sync Video H.265 Encoder",
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

  encoder_class->start = GST_DEBUG_FUNCPTR (gst_qsv_h265_enc_start);
  encoder_class->transform_meta =
      GST_DEBUG_FUNCPTR (gst_qsv_h265_enc_transform_meta);
  encoder_class->getcaps = GST_DEBUG_FUNCPTR (gst_qsv_h265_enc_getcaps);

  qsvenc_class->set_format = GST_DEBUG_FUNCPTR (gst_qsv_h265_enc_set_format);
  qsvenc_class->set_output_state =
      GST_DEBUG_FUNCPTR (gst_qsv_h265_enc_set_output_state);
  qsvenc_class->attach_payload =
      GST_DEBUG_FUNCPTR (gst_qsv_h265_enc_attach_payload);
  qsvenc_class->create_output_buffer =
      GST_DEBUG_FUNCPTR (gst_qsv_h265_enc_create_output_buffer);
  qsvenc_class->check_reconfigure =
      GST_DEBUG_FUNCPTR (gst_qsv_h265_enc_check_reconfigure);

  klass->hdr10_aware = cdata->hdr10_aware;

  gst_type_mark_as_plugin_api (GST_TYPE_QSV_H265_ENC_SEI_INSERT_MODE,
      (GstPluginAPIFlags) 0);
  gst_type_mark_as_plugin_api (GST_TYPE_QSV_H265_ENC_RATE_CONTROL,
      (GstPluginAPIFlags) 0);

  gst_caps_unref (cdata->sink_caps);
  gst_caps_unref (cdata->src_caps);
  g_free (cdata->description);
  g_free (cdata);
}

static void
gst_qsv_h265_enc_init (GstQsvH265Enc * self)
{
  self->min_qp_i = DEFAULT_QP;
  self->min_qp_p = DEFAULT_QP;
  self->min_qp_b = DEFAULT_QP;
  self->max_qp_i = DEFAULT_QP;
  self->max_qp_p = DEFAULT_QP;
  self->max_qp_p = DEFAULT_QP;
  self->qp_i = DEFAULT_QP;
  self->qp_p = DEFAULT_QP;
  self->qp_b = DEFAULT_QP;
  self->gop_size = DEFAULT_GOP_SIZE;
  self->idr_interval = DEFAULT_IDR_INTERVAL;
  self->bframes = DEFAULT_B_FRAMES;
  self->ref_frames = DEFAULT_REF_FRAMES;
  self->bitrate = DEFAULT_BITRATE;
  self->max_bitrate = DEFAULT_MAX_BITRATE;
  self->rate_control = DEFAULT_RATE_CONTROL;
  self->icq_quality = DEFAULT_IQC_QUALITY;
  self->qvbr_quality = DEFAULT_QVBR_QUALITY;
  self->disable_hrd_conformance = DEFAULT_DISABLE_HRD_CONFORMANCE;
  self->cc_insert = DEFAULT_CC_INSERT;

  g_mutex_init (&self->prop_lock);
}

static void
gst_qsv_h265_enc_finalize (GObject * object)
{
  GstQsvH265Enc *self = GST_QSV_H265_ENC (object);

  g_mutex_clear (&self->prop_lock);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_qsv_h265_enc_check_update_uint (GstQsvH265Enc * self, guint * old_val,
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
gst_qsv_h265_enc_check_update_enum (GstQsvH265Enc * self, mfxU16 * old_val,
    gint new_val)
{
  if (*old_val == (mfxU16) new_val)
    return;

  *old_val = (mfxU16) new_val;
  self->property_updated = TRUE;
}

static void
gst_qsv_h265_enc_check_update_boolean (GstQsvH265Enc * self, gboolean * old_val,
    gboolean new_val)
{
  if (*old_val == new_val)
    return;

  *old_val = new_val;
  self->property_updated = TRUE;
}

static void
gst_qsv_h265_enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstQsvH265Enc *self = GST_QSV_H265_ENC (object);

  g_mutex_lock (&self->prop_lock);
  switch (prop_id) {
    case PROP_MIN_QP_I:
      gst_qsv_h265_enc_check_update_uint (self, &self->min_qp_i,
          g_value_get_uint (value), FALSE);
      break;
    case PROP_MIN_QP_P:
      gst_qsv_h265_enc_check_update_uint (self, &self->min_qp_p,
          g_value_get_uint (value), FALSE);
      break;
    case PROP_MIN_QP_B:
      gst_qsv_h265_enc_check_update_uint (self, &self->min_qp_b,
          g_value_get_uint (value), FALSE);
      break;
    case PROP_MAX_QP_I:
      gst_qsv_h265_enc_check_update_uint (self, &self->max_qp_i,
          g_value_get_uint (value), FALSE);
      break;
    case PROP_MAX_QP_P:
      gst_qsv_h265_enc_check_update_uint (self, &self->max_qp_p,
          g_value_get_uint (value), FALSE);
      break;
    case PROP_MAX_QP_B:
      gst_qsv_h265_enc_check_update_uint (self, &self->max_qp_b,
          g_value_get_uint (value), FALSE);
      break;
    case PROP_QP_I:
      gst_qsv_h265_enc_check_update_uint (self, &self->qp_i,
          g_value_get_uint (value), TRUE);
      break;
    case PROP_QP_P:
      gst_qsv_h265_enc_check_update_uint (self, &self->qp_p,
          g_value_get_uint (value), TRUE);
      break;
    case PROP_QP_B:
      gst_qsv_h265_enc_check_update_uint (self, &self->qp_b,
          g_value_get_uint (value), TRUE);
      break;
    case PROP_GOP_SIZE:
      gst_qsv_h265_enc_check_update_uint (self, &self->gop_size,
          g_value_get_uint (value), FALSE);
      break;
    case PROP_IDR_INTERVAL:
      gst_qsv_h265_enc_check_update_uint (self, &self->idr_interval,
          g_value_get_uint (value), FALSE);
      break;
    case PROP_B_FRAMES:
      gst_qsv_h265_enc_check_update_uint (self, &self->bframes,
          g_value_get_uint (value), FALSE);
      break;
    case PROP_REF_FRAMES:
      gst_qsv_h265_enc_check_update_uint (self, &self->ref_frames,
          g_value_get_uint (value), FALSE);
      break;
    case PROP_BITRATE:
      gst_qsv_h265_enc_check_update_uint (self, &self->bitrate,
          g_value_get_uint (value), TRUE);
      break;
    case PROP_MAX_BITRATE:
      gst_qsv_h265_enc_check_update_uint (self, &self->max_bitrate,
          g_value_get_uint (value), TRUE);
      break;
    case PROP_RATE_CONTROL:
      gst_qsv_h265_enc_check_update_enum (self, &self->rate_control,
          g_value_get_enum (value));
      break;
    case PROP_ICQ_QUALITY:
      gst_qsv_h265_enc_check_update_uint (self, &self->icq_quality,
          g_value_get_uint (value), FALSE);
      break;
    case PROP_QVBR_QUALITY:
      gst_qsv_h265_enc_check_update_uint (self, &self->qvbr_quality,
          g_value_get_uint (value), FALSE);
      break;
    case PROP_DISABLE_HRD_CONFORMANCE:
      gst_qsv_h265_enc_check_update_boolean (self,
          &self->disable_hrd_conformance, g_value_get_boolean (value));
      break;
    case PROP_CC_INSERT:
      /* This property is unrelated to encoder-reset */
      self->cc_insert = (GstQsvH265EncSeiInsertMode) g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  g_mutex_unlock (&self->prop_lock);
}

static void
gst_qsv_h265_enc_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstQsvH265Enc *self = GST_QSV_H265_ENC (object);

  g_mutex_lock (&self->prop_lock);
  switch (prop_id) {
    case PROP_MIN_QP_I:
      g_value_set_uint (value, self->min_qp_i);
      break;
    case PROP_MIN_QP_P:
      g_value_set_uint (value, self->min_qp_p);
      break;
    case PROP_MIN_QP_B:
      g_value_set_uint (value, self->min_qp_b);
      break;
    case PROP_MAX_QP_I:
      g_value_set_uint (value, self->max_qp_i);
      break;
    case PROP_MAX_QP_P:
      g_value_set_uint (value, self->max_qp_p);
      break;
    case PROP_MAX_QP_B:
      g_value_set_uint (value, self->max_qp_b);
      break;
    case PROP_QP_I:
      g_value_set_uint (value, self->qp_i);
      break;
    case PROP_QP_P:
      g_value_set_uint (value, self->qp_p);
      break;
    case PROP_QP_B:
      g_value_set_uint (value, self->qp_b);
      break;
    case PROP_GOP_SIZE:
      g_value_set_uint (value, self->gop_size);
      break;
    case PROP_IDR_INTERVAL:
      g_value_set_uint (value, self->idr_interval);
      break;
    case PROP_B_FRAMES:
      g_value_set_uint (value, self->bframes);
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
    case PROP_QVBR_QUALITY:
      g_value_set_uint (value, self->qvbr_quality);
      break;
    case PROP_CC_INSERT:
      g_value_set_enum (value, self->cc_insert);
      break;
    case PROP_DISABLE_HRD_CONFORMANCE:
      g_value_set_boolean (value, self->disable_hrd_conformance);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  g_mutex_unlock (&self->prop_lock);
}

static gboolean
gst_qsv_h265_enc_start (GstVideoEncoder * encoder)
{
  /* To avoid negative DTS when B frame is enabled */
  gst_video_encoder_set_min_pts (encoder, GST_SECOND * 60 * 60 * 1000);

  return TRUE;
}

static gboolean
gst_qsv_h265_enc_transform_meta (GstVideoEncoder * encoder,
    GstVideoCodecFrame * frame, GstMeta * meta)
{
  GstQsvH265Enc *self = GST_QSV_H265_ENC (encoder);
  GstVideoCaptionMeta *cc_meta;

  /* We need to handle only case CC meta should be dropped */
  if (self->cc_insert != GST_QSV_H265_ENC_SEI_INSERT_AND_DROP)
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

static GstCaps *
gst_qsv_h265_enc_getcaps (GstVideoEncoder * encoder, GstCaps * filter)
{
  GstQsvH265Enc *self = GST_QSV_H265_ENC (encoder);
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
      if (g_strcmp0 (profile, "main") == 0 ||
          g_strcmp0 (profile, "main-10") == 0) {
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

    if (profile == "main") {
      format = "NV12";
    } else if (profile == "main-10") {
      format = "P010_10LE";
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
      if (iter == "main") {
        g_value_set_static_string (&val, "NV12");
      } else if (iter == "main-10") {
        g_value_set_static_string (&val, "P010_10LE");
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
} H265Profile;

static const H265Profile profile_map[] = {
  {MFX_PROFILE_HEVC_MAIN, "main", "NV12"},
  {MFX_PROFILE_HEVC_MAIN10, "main-10", "P010_10LE"},
};

static const gchar *
gst_qsv_h265_profile_to_string (mfxU16 profile)
{
  for (guint i = 0; i < G_N_ELEMENTS (profile_map); i++) {
    if (profile_map[i].profile == profile)
      return profile_map[i].profile_str;
  }

  return nullptr;
}

static void
gst_qsv_h265_enc_init_extra_params (GstQsvH265Enc * self)
{
  memset (&self->signal_info, 0, sizeof (mfxExtVideoSignalInfo));
  memset (&self->option, 0, sizeof (mfxExtCodingOption));
  memset (&self->option2, 0, sizeof (mfxExtCodingOption2));
  memset (&self->option3, 0, sizeof (mfxExtCodingOption3));
  memset (&self->cll, 0, sizeof (mfxExtContentLightLevelInfo));
  memset (&self->mdcv, 0, sizeof (mfxExtMasteringDisplayColourVolume));

  self->signal_info.Header.BufferId = MFX_EXTBUFF_VIDEO_SIGNAL_INFO;
  self->signal_info.Header.BufferSz = sizeof (mfxExtVideoSignalInfo);

  self->option.Header.BufferId = MFX_EXTBUFF_CODING_OPTION;
  self->option.Header.BufferSz = sizeof (mfxExtCodingOption);

  self->option2.Header.BufferId = MFX_EXTBUFF_CODING_OPTION2;
  self->option2.Header.BufferSz = sizeof (mfxExtCodingOption2);

  self->option3.Header.BufferId = MFX_EXTBUFF_CODING_OPTION3;
  self->option3.Header.BufferSz = sizeof (mfxExtCodingOption3);

  self->cll.Header.BufferId = MFX_EXTBUFF_CONTENT_LIGHT_LEVEL_INFO;
  self->cll.Header.BufferSz = sizeof (mfxExtContentLightLevelInfo);
  self->cll.InsertPayloadToggle = MFX_PAYLOAD_IDR;

  self->mdcv.Header.BufferId = MFX_EXTBUFF_MASTERING_DISPLAY_COLOUR_VOLUME;
  self->mdcv.Header.BufferSz = sizeof (mfxExtMasteringDisplayColourVolume);
  self->mdcv.InsertPayloadToggle = MFX_PAYLOAD_IDR;
}

static void
gst_qsv_h265_enc_set_bitrate (GstQsvH265Enc * self, mfxVideoParam * param)
{
  guint max_val;
  guint multiplier;

  switch (param->mfx.RateControlMethod) {
    case MFX_RATECONTROL_CBR:
      multiplier = (self->bitrate + 0x10000) / 0x10000;
      param->mfx.TargetKbps = param->mfx.MaxKbps = self->bitrate / multiplier;
      param->mfx.BRCParamMultiplier = (mfxU16) multiplier;
      break;
    case MFX_RATECONTROL_VBR:
    case MFX_RATECONTROL_VCM:
    case MFX_RATECONTROL_QVBR:
      max_val = MAX (self->bitrate, self->max_bitrate);
      multiplier = (max_val + 0x10000) / 0x10000;
      param->mfx.TargetKbps = self->bitrate / multiplier;
      param->mfx.MaxKbps = self->max_bitrate / multiplier;
      param->mfx.BRCParamMultiplier = (mfxU16) multiplier;
      break;
    case MFX_RATECONTROL_CQP:
      param->mfx.QPI = self->qp_i;
      param->mfx.QPP = self->qp_p;
      param->mfx.QPB = self->qp_b;
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
gst_qsv_h265_enc_set_format (GstQsvEncoder * encoder,
    GstVideoCodecState * state, mfxVideoParam * param, GPtrArray * extra_params)
{
  GstQsvH265Enc *self = GST_QSV_H265_ENC (encoder);
  GstQsvH265EncClass *klass = GST_QSV_H265_ENC_GET_CLASS (self);
  mfxU16 mfx_profile = MFX_PROFILE_UNKNOWN;
  GstVideoInfo *info = &state->info;
  mfxExtVideoSignalInfo *signal_info = nullptr;
  mfxExtCodingOption *option;
  mfxExtCodingOption2 *option2;
  mfxExtCodingOption3 *option3;
  mfxExtContentLightLevelInfo *cll;
  mfxExtMasteringDisplayColourVolume *mdcv;
  gboolean have_cll = FALSE;
  gboolean have_mdcv = FALSE;

  mfxFrameInfo *frame_info;

  frame_info = &param->mfx.FrameInfo;

  frame_info->Width = GST_ROUND_UP_16 (info->width);
  frame_info->Height = GST_ROUND_UP_16 (info->height);
  frame_info->PicStruct = MFX_PICSTRUCT_PROGRESSIVE;

  /* QSV wouldn't be happy with this size, increase */
  if (frame_info->Width == 16)
    frame_info->Width = 32;

  if (frame_info->Height == 16)
    frame_info->Height = 32;

  frame_info->CropW = info->width;
  frame_info->CropH = info->height;
  if (GST_VIDEO_INFO_FPS_N (info) > 0 && GST_VIDEO_INFO_FPS_D (info) > 0) {
    frame_info->FrameRateExtN = GST_VIDEO_INFO_FPS_N (info);
    frame_info->FrameRateExtD = GST_VIDEO_INFO_FPS_D (info);
  } else {
    /* HACK: Same as x265enc */
    frame_info->FrameRateExtN = 25;
    frame_info->FrameRateExtD = 1;
  }

  frame_info->AspectRatioW = GST_VIDEO_INFO_PAR_N (info);
  frame_info->AspectRatioH = GST_VIDEO_INFO_PAR_D (info);

  switch (GST_VIDEO_INFO_FORMAT (info)) {
    case GST_VIDEO_FORMAT_NV12:
      mfx_profile = MFX_PROFILE_HEVC_MAIN;
      frame_info->ChromaFormat = MFX_CHROMAFORMAT_YUV420;
      frame_info->FourCC = MFX_FOURCC_NV12;
      frame_info->BitDepthLuma = 8;
      frame_info->BitDepthChroma = 8;
      frame_info->Shift = 0;
      break;
    case GST_VIDEO_FORMAT_P010_10LE:
      mfx_profile = MFX_PROFILE_HEVC_MAIN10;
      frame_info->ChromaFormat = MFX_CHROMAFORMAT_YUV420;
      frame_info->FourCC = MFX_FOURCC_P010;
      frame_info->BitDepthLuma = 10;
      frame_info->BitDepthChroma = 10;
      frame_info->Shift = 1;
      break;
    default:
      GST_ERROR_OBJECT (self, "Unexpected format %s",
          gst_video_format_to_string (GST_VIDEO_INFO_FORMAT (info)));
      return FALSE;
  }

  gst_qsv_h265_enc_init_extra_params (self);
  option = &self->option;
  option2 = &self->option2;
  option3 = &self->option3;
  cll = &self->cll;
  mdcv = &self->mdcv;

  if (klass->hdr10_aware) {
    GstVideoMasteringDisplayInfo mdcv_info;
    GstVideoContentLightLevel cll_info;

    if (gst_video_content_light_level_from_caps (&cll_info, state->caps)) {
      cll->MaxContentLightLevel = cll_info.max_content_light_level;
      cll->MaxPicAverageLightLevel = cll_info.max_frame_average_light_level;
      have_cll = TRUE;
    }

    if (gst_video_mastering_display_info_from_caps (&mdcv_info, state->caps)) {
      /* GBR order is used in HEVC */
      mdcv->DisplayPrimariesX[0] = mdcv_info.display_primaries[1].x;
      mdcv->DisplayPrimariesY[0] = mdcv_info.display_primaries[1].y;
      mdcv->DisplayPrimariesX[1] = mdcv_info.display_primaries[2].x;
      mdcv->DisplayPrimariesY[1] = mdcv_info.display_primaries[2].y;
      mdcv->DisplayPrimariesX[2] = mdcv_info.display_primaries[0].x;
      mdcv->DisplayPrimariesY[2] = mdcv_info.display_primaries[0].y;

      mdcv->WhitePointX = mdcv_info.white_point.x;
      mdcv->WhitePointY = mdcv_info.white_point.y;
      mdcv->MaxDisplayMasteringLuminance =
          mdcv_info.max_display_mastering_luminance;
      mdcv->MinDisplayMasteringLuminance =
          mdcv_info.min_display_mastering_luminance;
      have_mdcv = TRUE;
    }
  }

  g_mutex_lock (&self->prop_lock);
  param->mfx.CodecId = MFX_CODEC_HEVC;
  param->mfx.CodecProfile = mfx_profile;
  param->mfx.GopRefDist = self->bframes + 1;
  param->mfx.GopPicSize = self->gop_size;
  param->mfx.IdrInterval = self->idr_interval;
  param->mfx.RateControlMethod = self->rate_control;
  param->mfx.NumRefFrame = self->ref_frames;

  gst_qsv_h265_enc_set_bitrate (self, param);

  /* Write signal info only when upstream caps contains valid colorimetry,
   * because derived default colorimetry in gst_video_info_from_caps() tends to
   * very wrong in various cases, and it's even worse than "unknown" */
  if (state->caps) {
    GstStructure *s = gst_caps_get_structure (state->caps, 0);
    GstVideoColorimetry cinfo;
    const gchar *str;

    str = gst_structure_get_string (s, "colorimetry");
    if (str && gst_video_colorimetry_from_string (&cinfo, str)) {
      signal_info = &self->signal_info;

      /* 0: Component, 1: PAL, 2: NTSC, 3: SECAM, 4: MAC, 5: Unspecified */
      signal_info->VideoFormat = 5;
      if (cinfo.range == GST_VIDEO_COLOR_RANGE_0_255)
        signal_info->VideoFullRange = 1;
      else
        signal_info->VideoFullRange = 0;
      signal_info->ColourDescriptionPresent = 1;
      signal_info->ColourPrimaries =
          gst_video_color_primaries_to_iso (cinfo.primaries);
      signal_info->TransferCharacteristics =
          gst_video_transfer_function_to_iso (cinfo.transfer);
      signal_info->MatrixCoefficients =
          gst_video_color_matrix_to_iso (cinfo.matrix);
    }
  }

  /* TODO: property ? */
  option->AUDelimiter = MFX_CODINGOPTION_ON;

  if (self->disable_hrd_conformance) {
    option->NalHrdConformance = MFX_CODINGOPTION_OFF;
    option->VuiVclHrdParameters = MFX_CODINGOPTION_OFF;
  }

  /* Enables PicTiming SEI by default */
  option->PicTimingSEI = MFX_CODINGOPTION_ON;

  /* VUI is useful in various cases, so we don't want to disable it */
  option2->DisableVUI = MFX_CODINGOPTION_OFF;

  /* Do not repeat PPS */
  option2->RepeatPPS = MFX_CODINGOPTION_OFF;

  option2->MinQPI = self->min_qp_i;
  option2->MinQPP = self->min_qp_p;
  option2->MinQPB = self->min_qp_b;
  option2->MaxQPI = self->max_qp_i;
  option2->MaxQPP = self->max_qp_p;
  option2->MaxQPB = self->max_qp_b;

  /* QSV wants MFX_B_REF_PYRAMID when more than 1 b-frame is enabled */
  if (param->mfx.GopRefDist > 2)
    option2->BRefType = MFX_B_REF_PYRAMID;

  /* Upstream specified framerate, we will believe it's fixed framerate */
  if (GST_VIDEO_INFO_FPS_N (info) > 0 && GST_VIDEO_INFO_FPS_D (info) > 0) {
    option2->FixedFrameRate = MFX_CODINGOPTION_ON;
    option3->TimingInfoPresent = MFX_CODINGOPTION_ON;
  }

  if (param->mfx.RateControlMethod == MFX_RATECONTROL_QVBR)
    option3->QVBRQuality = self->qvbr_quality;

  if (signal_info)
    g_ptr_array_add (extra_params, signal_info);
  g_ptr_array_add (extra_params, option);
  g_ptr_array_add (extra_params, option2);
  g_ptr_array_add (extra_params, option3);
  if (have_cll)
    g_ptr_array_add (extra_params, cll);
  if (have_mdcv)
    g_ptr_array_add (extra_params, mdcv);

  param->ExtParam = (mfxExtBuffer **) extra_params->pdata;
  param->NumExtParam = extra_params->len;

  self->bitrate_updated = FALSE;
  self->property_updated = FALSE;

  g_mutex_unlock (&self->prop_lock);

  return TRUE;
}

static gboolean
gst_qsv_h265_enc_set_output_state (GstQsvEncoder * encoder,
    GstVideoCodecState * state, mfxSession session)
{
  GstQsvH265Enc *self = GST_QSV_H265_ENC (encoder);
  GstCaps *caps;
  GstTagList *tags;
  GstVideoCodecState *out_state;
  guint bitrate, max_bitrate;
  guint multiplier = 1;
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

  caps = gst_caps_from_string ("video/x-h265, alignment = (string) au, "
      "stream-format = (string) byte-stream");
  profile_str = gst_qsv_h265_profile_to_string (param.mfx.CodecProfile);
  if (profile_str)
    gst_caps_set_simple (caps, "profile", G_TYPE_STRING, profile_str, nullptr);

  out_state = gst_video_encoder_set_output_state (GST_VIDEO_ENCODER (encoder),
      caps, state);
  gst_video_codec_state_unref (out_state);

  tags = gst_tag_list_new_empty ();
  gst_tag_list_add (tags, GST_TAG_MERGE_REPLACE, GST_TAG_ENCODER, "qsvh265enc",
      nullptr);

  if (param.mfx.BRCParamMultiplier > 0)
    multiplier = param.mfx.BRCParamMultiplier;

  switch (param.mfx.RateControlMethod) {
    case MFX_RATECONTROL_CQP:
    case MFX_RATECONTROL_ICQ:
      /* We don't know target/max bitrate in this case */
      break;
    default:
      max_bitrate = (guint) param.mfx.MaxKbps * multiplier;
      bitrate = (guint) param.mfx.TargetKbps * multiplier;
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

static gboolean
gst_qsv_h265_enc_foreach_caption_meta (GstBuffer * buffer, GstMeta ** meta,
    GPtrArray * payload)
{
  GstVideoCaptionMeta *cc_meta;
  GstByteWriter br;
  guint payload_size;
  guint extra_size;
  mfxPayload *p;

  if ((*meta)->info->api != GST_VIDEO_CAPTION_META_API_TYPE)
    return TRUE;

  cc_meta = (GstVideoCaptionMeta *) (*meta);

  if (cc_meta->caption_type != GST_VIDEO_CAPTION_TYPE_CEA708_RAW)
    return TRUE;

  /* QSV requires full sei_message() structure */
  /* 1 byte contry_code + 10 bytes CEA-708 specific data + caption data */
  payload_size = 11 + cc_meta->size;
  extra_size = payload_size / 255;

  /* 1 byte SEI type + 1 byte SEI payload size (+ extra) + payload data */
  gst_byte_writer_init_with_size (&br, 2 + extra_size + payload_size, FALSE);

  /* SEI type */
  gst_byte_writer_put_uint8 (&br, 4);

  /* SEI payload size */
  while (payload_size >= 0xff) {
    gst_byte_writer_put_uint8 (&br, 0xff);
    payload_size -= 0xff;
  }
  gst_byte_writer_put_uint8 (&br, payload_size);

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

  p = g_new0 (mfxPayload, 1);
  p->BufSize = gst_byte_writer_get_pos (&br);
  p->NumBit = p->BufSize * 8;
  p->Type = 4;
  p->Data = gst_byte_writer_reset_and_get_data (&br);

  g_ptr_array_add (payload, p);

  return TRUE;
}

static gboolean
gst_qsv_h265_enc_attach_payload (GstQsvEncoder * encoder,
    GstVideoCodecFrame * frame, GPtrArray * payload)
{
  GstQsvH265Enc *self = GST_QSV_H265_ENC (encoder);

  if (self->cc_insert == GST_QSV_H265_ENC_SEI_DISABLED)
    return TRUE;

  gst_buffer_foreach_meta (frame->input_buffer,
      (GstBufferForeachMetaFunc) gst_qsv_h265_enc_foreach_caption_meta,
      payload);

  return TRUE;
}

static GstBuffer *
gst_qsv_h265_enc_create_output_buffer (GstQsvEncoder * encoder,
    mfxBitstream * bitstream)
{
  GstBuffer *buf;

  buf = gst_buffer_new_memdup (bitstream->Data + bitstream->DataOffset,
      bitstream->DataLength);

  /* This buffer must be the end of a frame boundary */
  GST_BUFFER_FLAG_SET (buf, GST_BUFFER_FLAG_MARKER);

  return buf;
}

static GstQsvEncoderReconfigure
gst_qsv_h265_enc_check_reconfigure (GstQsvEncoder * encoder, mfxSession session,
    mfxVideoParam * param, GPtrArray * extra_params)
{
  GstQsvH265Enc *self = GST_QSV_H265_ENC (encoder);
  GstQsvEncoderReconfigure ret = GST_QSV_ENCODER_RECONFIGURE_NONE;

  g_mutex_lock (&self->prop_lock);
  if (self->property_updated) {
    ret = GST_QSV_ENCODER_RECONFIGURE_FULL;
    goto done;
  }

  if (self->bitrate_updated) {
    mfxStatus status;
    mfxExtEncoderResetOption reset_opt;
    reset_opt.Header.BufferId = MFX_EXTBUFF_ENCODER_RESET_OPTION;
    reset_opt.Header.BufferSz = sizeof (mfxExtEncoderResetOption);
    reset_opt.StartNewSequence = MFX_CODINGOPTION_UNKNOWN;

    gst_qsv_h265_enc_set_bitrate (self, param);

    g_ptr_array_add (extra_params, &reset_opt);
    param->ExtParam = (mfxExtBuffer **) extra_params->pdata;
    param->NumExtParam = extra_params->len;

    status = MFXVideoENCODE_Query (session, param, param);
    g_ptr_array_remove_index (extra_params, extra_params->len - 1);
    param->NumExtParam = extra_params->len;

    if (status != MFX_ERR_NONE) {
      GST_WARNING_OBJECT (self, "MFXVideoENCODE_Query returned %d (%s)",
          QSV_STATUS_ARGS (status));
      ret = GST_QSV_ENCODER_RECONFIGURE_FULL;
    } else {
      if (reset_opt.StartNewSequence == MFX_CODINGOPTION_OFF) {
        GST_DEBUG_OBJECT (self, "Can update without new sequence");
        ret = GST_QSV_ENCODER_RECONFIGURE_BITRATE;
      } else {
        GST_DEBUG_OBJECT (self, "Need new sequence");
        ret = GST_QSV_ENCODER_RECONFIGURE_FULL;
      }
    }
  }

done:
  self->property_updated = FALSE;
  self->bitrate_updated = FALSE;
  g_mutex_unlock (&self->prop_lock);

  return ret;
}

void
gst_qsv_h265_enc_register (GstPlugin * plugin, guint rank, guint impl_index,
    GstObject * device, mfxSession session)
{
  mfxVideoParam param;
  mfxInfoMFX *mfx;
  std::vector < mfxU16 > supported_profiles;
  std::vector < std::string > supported_formats;
  GstQsvResolution max_resolution;
  mfxExtContentLightLevelInfo cll;
  mfxExtMasteringDisplayColourVolume mdcv;
  mfxExtBuffer *ext_buffers[2];
  gboolean hdr10_aware = FALSE;

  GST_DEBUG_CATEGORY_INIT (gst_qsv_h265_enc_debug,
      "qsvh265enc", 0, "qsvh265enc");

  memset (&param, 0, sizeof (mfxVideoParam));
  memset (&max_resolution, 0, sizeof (GstQsvResolution));
  memset (&cll, 0, sizeof (mfxExtContentLightLevelInfo));
  memset (&mdcv, 0, sizeof (mfxExtMasteringDisplayColourVolume));

  param.AsyncDepth = 4;
  param.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;

  mfx = &param.mfx;
  mfx->CodecId = MFX_CODEC_HEVC;

  mfx->FrameInfo.Width = GST_ROUND_UP_16 (320);
  mfx->FrameInfo.Height = GST_ROUND_UP_16 (240);
  mfx->FrameInfo.CropW = 320;
  mfx->FrameInfo.CropH = 240;
  mfx->FrameInfo.FrameRateExtN = 30;
  mfx->FrameInfo.FrameRateExtD = 1;
  mfx->FrameInfo.AspectRatioW = 1;
  mfx->FrameInfo.AspectRatioH = 1;
  mfx->FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;

  /* Check supported profiles */
  for (guint i = 0; i < G_N_ELEMENTS (profile_map); i++) {
    GstVideoFormat format = GST_VIDEO_FORMAT_UNKNOWN;
    mfx->CodecProfile = profile_map[i].profile;
    mfx->CodecLevel = MFX_LEVEL_UNKNOWN;

    switch (mfx->CodecProfile) {
      case MFX_PROFILE_HEVC_MAIN:
        format = GST_VIDEO_FORMAT_NV12;
        break;
      case MFX_PROFILE_HEVC_MAIN10:
        format = GST_VIDEO_FORMAT_P010_10LE;
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
    GST_INFO ("Device doesn't support H.265 encoding");
    return;
  }

  gst_qsv_frame_info_set_format (&mfx->FrameInfo, GST_VIDEO_FORMAT_NV12);
  mfx->CodecProfile = MFX_PROFILE_HEVC_MAIN;

  /* check hdr10 metadata SEI support */
  cll.Header.BufferId = MFX_EXTBUFF_CONTENT_LIGHT_LEVEL_INFO;
  cll.Header.BufferSz = sizeof (mfxExtContentLightLevelInfo);
  cll.InsertPayloadToggle = MFX_PAYLOAD_IDR;
  cll.MaxContentLightLevel = 1;
  cll.MaxPicAverageLightLevel = 1;

  mdcv.Header.BufferId = MFX_EXTBUFF_MASTERING_DISPLAY_COLOUR_VOLUME;
  mdcv.Header.BufferSz = sizeof (mfxExtMasteringDisplayColourVolume);
  mdcv.InsertPayloadToggle = MFX_PAYLOAD_IDR;
  mdcv.MaxDisplayMasteringLuminance = 1;
  mdcv.MinDisplayMasteringLuminance = 1;

  ext_buffers[0] = (mfxExtBuffer *) & cll;
  ext_buffers[1] = (mfxExtBuffer *) & mdcv;

  param.NumExtParam = 2;
  param.ExtParam = ext_buffers;
  if (MFXVideoENCODE_Query (session, &param, &param) == MFX_ERR_NONE) {
    GST_INFO ("HDR10 metadata SEI is supported");
    hdr10_aware = TRUE;
  }

  param.NumExtParam = 0;
  param.ExtParam = nullptr;

  /* Check max-resolution */
  for (guint i = 0; i < G_N_ELEMENTS (gst_qsv_resolutions); i++) {
    mfx->FrameInfo.Width = GST_ROUND_UP_16 (gst_qsv_resolutions[i].width);
    mfx->FrameInfo.Height = GST_ROUND_UP_16 (gst_qsv_resolutions[i].height);
    mfx->FrameInfo.CropW = gst_qsv_resolutions[i].width;
    mfx->FrameInfo.CropH = gst_qsv_resolutions[i].height;

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

  std::string src_caps_str = "video/x-h265";
  src_caps_str += ", width=(int) [ 16, " + std::to_string (resolution) + " ]";
  src_caps_str += ", height=(int) [ 16, " + std::to_string (resolution) + " ]";

  src_caps_str += ", stream-format= (string) byte-stream";
  src_caps_str += ", alignment=(string) au";
  /* *INDENT-OFF* */
  if (supported_profiles.size () > 1) {
    src_caps_str += ", profile=(string) { ";
    bool first = true;
    for (const auto &iter: supported_profiles) {
      if (!first) {
        src_caps_str += ", ";
      }

      src_caps_str += gst_qsv_h265_profile_to_string (iter);
      first = false;
    }
    src_caps_str += " }";
  } else {
    src_caps_str += ", profile=(string) ";
    src_caps_str += gst_qsv_h265_profile_to_string (supported_profiles[0]);
  }
  /* *INDENT-ON* */

  GstCaps *src_caps = gst_caps_from_string (src_caps_str.c_str ());

  GST_MINI_OBJECT_FLAG_SET (sink_caps, GST_MINI_OBJECT_FLAG_MAY_BE_LEAKED);
  GST_MINI_OBJECT_FLAG_SET (src_caps, GST_MINI_OBJECT_FLAG_MAY_BE_LEAKED);

  GstQsvH265EncClassData *cdata = g_new0 (GstQsvH265EncClassData, 1);
  cdata->sink_caps = sink_caps;
  cdata->src_caps = src_caps;
  cdata->impl_index = impl_index;
  cdata->hdr10_aware = hdr10_aware;

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
    sizeof (GstQsvH265EncClass),
    nullptr,
    nullptr,
    (GClassInitFunc) gst_qsv_h265_enc_class_init,
    nullptr,
    cdata,
    sizeof (GstQsvH265Enc),
    0,
    (GInstanceInitFunc) gst_qsv_h265_enc_init,
  };

  type_name = g_strdup ("GstQsvH265Enc");
  feature_name = g_strdup ("qsvh265enc");

  gint index = 0;
  while (g_type_from_name (type_name)) {
    index++;
    g_free (type_name);
    g_free (feature_name);
    type_name = g_strdup_printf ("GstQsvH265Device%dEnc", index);
    feature_name = g_strdup_printf ("qsvh265device%denc", index);
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
