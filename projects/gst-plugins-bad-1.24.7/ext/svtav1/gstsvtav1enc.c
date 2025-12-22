/*
* Copyright(c) 2019 Intel Corporation
*     Authors: Jun Tian <jun.tian@intel.com> Xavier Hallade <xavier.hallade@intel.com>
* SPDX - License - Identifier: LGPL-2.1-or-later
*/

/**
 * SECTION:element-gstsvtav1enc
 *
 * The svtav1enc element does AV1 encoding using Scalable
 * Video Technology for AV1 Encoder (SVT-AV1 Encoder).
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch-1.0 -e videotestsrc ! video/x-raw ! svtav1enc ! matroskamux ! filesink location=out.mkv
 * ]|
 * Encodes test input into AV1 compressed data which is then packaged in out.mkv
 * </refsect2>
 */

#include "config.h"

#include "gstsvtav1enc.h"
#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideoencoder.h>

#if !SVT_AV1_CHECK_VERSION(1,2,1)
#define SVT_AV1_RC_MODE_CQP_OR_CRF 0
#define SVT_AV1_RC_MODE_VBR 1
#define SVT_AV1_RC_MODE_CBR 2
#endif

GST_DEBUG_CATEGORY_STATIC (gst_svtav1enc_debug_category);
#define GST_CAT_DEFAULT gst_svtav1enc_debug_category

#define GST_SVTAV1ENC_TYPE_INTRA_REFRESH_TYPE (gst_svtav1enc_intra_refresh_type_get_type())
static GType
gst_svtav1enc_intra_refresh_type_get_type (void)
{
  static GType intra_refresh_type = 0;
  static const GEnumValue intra_refresh[] = {
    {SVT_AV1_FWDKF_REFRESH, "Open GOP", "CRA"},
    {SVT_AV1_KF_REFRESH, "Closed GOP", "IDR"},
    {0, NULL, NULL},
  };

  if (!intra_refresh_type) {
    intra_refresh_type =
        g_enum_register_static ("GstSvtAv1EncIntraRefreshType", intra_refresh);
  }
  return intra_refresh_type;
}

typedef struct _GstSvtAv1Enc
{
  GstVideoEncoder video_encoder;

  /* SVT-AV1 Encoder Handle */
  EbComponentType *svt_encoder;

  /* GStreamer Codec state */
  GstVideoCodecState *state;

  /* SVT-AV1 configuration */
  EbSvtAv1EncConfiguration *svt_config;
  /* Property values */
  guint preset;
  guint target_bitrate;
  guint max_bitrate;
  guint max_qp_allowed;
  guint min_qp_allowed;
  gint cqp, crf;
  guint maximum_buffer_size;
  gint intra_period_length;
  gint intra_refresh_type;
  gint logical_processors;
  gint target_socket;
  gchar *parameters_string;

  EbBufferHeaderType *input_buf;
} GstSvtAv1Enc;


/* prototypes */
static void gst_svtav1enc_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec);
static void gst_svtav1enc_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec);
static void gst_svtav1enc_finalize (GObject * object);

static void gst_svtav1enc_allocate_svt_buffers (GstSvtAv1Enc * svtav1enc);
static void gst_svtav1enc_deallocate_svt_buffers (GstSvtAv1Enc * svtav1enc);
static gboolean gst_svtav1enc_configure_svt (GstSvtAv1Enc * svtav1enc);
static GstFlowReturn gst_svtav1enc_encode (GstSvtAv1Enc * svtav1enc,
    GstVideoCodecFrame * frame);
static gboolean gst_svtav1enc_send_eos (GstSvtAv1Enc * svtav1enc);
static GstFlowReturn gst_svtav1enc_dequeue_encoded_frames (GstSvtAv1Enc *
    svtav1enc, gboolean closing_encoder, gboolean output_frames);

static gboolean gst_svtav1enc_open (GstVideoEncoder * encoder);
static gboolean gst_svtav1enc_close (GstVideoEncoder * encoder);
static gboolean gst_svtav1enc_start (GstVideoEncoder * encoder);
static gboolean gst_svtav1enc_stop (GstVideoEncoder * encoder);
static gboolean gst_svtav1enc_set_format (GstVideoEncoder * encoder,
    GstVideoCodecState * state);
static GstFlowReturn gst_svtav1enc_handle_frame (GstVideoEncoder * encoder,
    GstVideoCodecFrame * frame);
static GstFlowReturn gst_svtav1enc_finish (GstVideoEncoder * encoder);
static gboolean gst_svtav1enc_propose_allocation (GstVideoEncoder * encoder,
    GstQuery * query);
static gboolean gst_svtav1enc_flush (GstVideoEncoder * encoder);

static void gst_svtav1enc_parse_parameters_string (GstSvtAv1Enc * svtav1enc);

enum
{
  PROP_0,
  PROP_PRESET,
  PROP_TARGET_BITRATE,
  PROP_MAX_BITRATE,
  PROP_MAX_QP_ALLOWED,
  PROP_MIN_QP_ALLOWED,
  PROP_CQP,
  PROP_CRF,
  PROP_MAXIMUM_BUFFER_SIZE,
  PROP_INTRA_PERIOD_LENGTH,
  PROP_INTRA_REFRESH_TYPE,
  PROP_LOGICAL_PROCESSORS,
  PROP_TARGET_SOCKET,
  PROP_PARAMETERS_STRING,
};

#define PROP_PRESET_DEFAULT 10
#define PROP_TARGET_BITRATE_DEFAULT 0
#define PROP_MAX_BITRATE_DEFAULT 0
#define PROP_QP_MAX_QP_ALLOWED_DEFAULT 63
#define PROP_QP_MIN_QP_ALLOWED_DEFAULT 1
#define PROP_CQP_DEFAULT -1
#define PROP_CRF_DEFAULT 35
#define PROP_MAXIMUM_BUFFER_SIZE_DEFAULT 1000
#define PROP_INTRA_PERIOD_LENGTH_DEFAULT -2
#define PROP_INTRA_REFRESH_TYPE_DEFAULT SVT_AV1_KF_REFRESH
#define PROP_LOGICAL_PROCESSORS_DEFAULT 0
#define PROP_TARGET_SOCKET_DEFAULT -1
#define PROP_PARAMETERS_STRING_DEFAULT NULL

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
#define FORMAT_I420_10 "I420_10LE"
#else
#define FORMAT_I420_10 "I420_10BE"
#endif

/* pad templates */
static GstStaticPadTemplate gst_svtav1enc_sink_pad_template =
GST_STATIC_PAD_TEMPLATE ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw, " "format = (string) {I420, " FORMAT_I420_10
        "}, " "width = (int) [64, 16384], " "height = (int) [64, 8704], "
        "framerate = (fraction) [0, MAX]"));

static GstStaticPadTemplate gst_svtav1enc_src_pad_template =
GST_STATIC_PAD_TEMPLATE ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-av1, " "stream-format = (string) obu-stream, "
        "alignment = (string) tu, " "width = (int) [64, 16384], "
        "height = (int) [64, 8704], " "framerate = (fraction) [0, MAX]"));

/* class initialization */
G_DEFINE_TYPE_WITH_CODE (GstSvtAv1Enc, gst_svtav1enc, GST_TYPE_VIDEO_ENCODER,
    GST_DEBUG_CATEGORY_INIT (gst_svtav1enc_debug_category, "svtav1enc", 0,
        "SVT-AV1 encoder element"));

GST_ELEMENT_REGISTER_DEFINE (svtav1enc, "svtav1enc", GST_RANK_SECONDARY,
    gst_svtav1enc_get_type ());

/* this mutex is required to avoid race conditions in SVT-AV1 memory allocations, which aren't thread-safe */
G_LOCK_DEFINE_STATIC (init_mutex);

static void
gst_svtav1enc_class_init (GstSvtAv1EncClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstVideoEncoderClass *video_encoder_class = GST_VIDEO_ENCODER_CLASS (klass);

  gst_element_class_add_static_pad_template (GST_ELEMENT_CLASS (klass),
      &gst_svtav1enc_src_pad_template);

  gst_element_class_add_static_pad_template (GST_ELEMENT_CLASS (klass),
      &gst_svtav1enc_sink_pad_template);

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS (klass),
      "SvtAv1Enc",
      "Codec/Encoder/Video",
      "Scalable Video Technology for AV1 Encoder (SVT-AV1 Encoder)",
      "Jun Tian <jun.tian@intel.com> Xavier Hallade <xavier.hallade@intel.com>");

  gobject_class->set_property = gst_svtav1enc_set_property;
  gobject_class->get_property = gst_svtav1enc_get_property;
  gobject_class->finalize = gst_svtav1enc_finalize;
  video_encoder_class->open = GST_DEBUG_FUNCPTR (gst_svtav1enc_open);
  video_encoder_class->close = GST_DEBUG_FUNCPTR (gst_svtav1enc_close);
  video_encoder_class->start = GST_DEBUG_FUNCPTR (gst_svtav1enc_start);
  video_encoder_class->stop = GST_DEBUG_FUNCPTR (gst_svtav1enc_stop);
  video_encoder_class->set_format =
      GST_DEBUG_FUNCPTR (gst_svtav1enc_set_format);
  video_encoder_class->handle_frame =
      GST_DEBUG_FUNCPTR (gst_svtav1enc_handle_frame);
  video_encoder_class->finish = GST_DEBUG_FUNCPTR (gst_svtav1enc_finish);
  video_encoder_class->propose_allocation =
      GST_DEBUG_FUNCPTR (gst_svtav1enc_propose_allocation);
  video_encoder_class->flush = GST_DEBUG_FUNCPTR (gst_svtav1enc_flush);

  g_object_class_install_property (gobject_class,
      PROP_PRESET,
      g_param_spec_uint ("preset",
          "Preset",
          "Quality vs density tradeoff point"
          " that the encoding is to be performed at"
          " (0 is the highest quality, 13 is the highest speed) ",
          0,
          13, PROP_PRESET_DEFAULT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_TARGET_BITRATE,
      g_param_spec_uint ("target-bitrate",
          "Target bitrate",
          "Target bitrate in kbits/sec. Enables CBR or VBR mode",
          0,
          100000,
          PROP_TARGET_BITRATE_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_MAX_BITRATE,
      g_param_spec_uint ("max-bitrate",
          "Maximum bitrate",
          "Maximum bitrate in kbits/sec. Enables VBR mode if a different "
          "target-bitrate is provided",
          0,
          100000,
          PROP_MAX_BITRATE_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_PLAYING));

  g_object_class_install_property (gobject_class,
      PROP_MAX_QP_ALLOWED,
      g_param_spec_uint ("max-qp-allowed",
          "Max Quantization parameter",
          "Maximum QP value allowed for rate control use"
          " Only used in CBR and VBR mode.",
          0,
          63, PROP_MAX_QP_ALLOWED, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_MIN_QP_ALLOWED,
      g_param_spec_uint ("min-qp-allowed",
          "Min Quantization parameter",
          "Minimum QP value allowed for rate control use"
          " Only used in CBR and VBR mode.",
          0,
          63, PROP_MIN_QP_ALLOWED, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_CQP,
      g_param_spec_int ("cqp",
          "Quantization parameter",
          "Quantization parameter used in CQP mode (-1 is disabled)",
          -1,
          63, PROP_CQP_DEFAULT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_CRF,
      g_param_spec_int ("crf",
          "Constant Rate Factor",
          "Quantization parameter used in CRF mode (-1 is disabled)",
          -1,
          63, PROP_CRF_DEFAULT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_MAXIMUM_BUFFER_SIZE,
      g_param_spec_uint ("maximum-buffer-size",
          "Maximum Buffer Size",
          "Maximum buffer size in milliseconds."
          " Only used in CBR mode.",
          20,
          10000,
          PROP_MAXIMUM_BUFFER_SIZE_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_INTRA_PERIOD_LENGTH,
      g_param_spec_int ("intra-period-length",
          "Intra Period Length",
          "Period of Intra Frames insertion (-2 is auto, -1 no updates)",
          -2,
          G_MAXINT,
          PROP_INTRA_PERIOD_LENGTH_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_INTRA_REFRESH_TYPE,
      g_param_spec_enum ("intra-refresh-type",
          "Intra refresh type",
          "CRA (open GOP)"
          "or IDR frames (closed GOP)",
          GST_SVTAV1ENC_TYPE_INTRA_REFRESH_TYPE,
          PROP_INTRA_REFRESH_TYPE_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_LOGICAL_PROCESSORS,
      g_param_spec_uint ("logical-processors",
          "Logical Processors",
          "Number of logical CPU cores to be used. 0: auto",
          0,
          G_MAXUINT,
          PROP_LOGICAL_PROCESSORS_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_TARGET_SOCKET,
      g_param_spec_int ("target-socket",
          "Target socket",
          "Target CPU socket to run on. -1: all available",
          -1,
          15,
          PROP_TARGET_SOCKET_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
      PROP_PARAMETERS_STRING,
      g_param_spec_string ("parameters-string",
          "Parameters String",
          "Colon-delimited list of key=value pairs of additional parameters to set",
          PROP_PARAMETERS_STRING_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
gst_svtav1enc_init (GstSvtAv1Enc * svtav1enc)
{
  svtav1enc->svt_config = g_new0 (EbSvtAv1EncConfiguration, 1);
  svtav1enc->preset = PROP_PRESET_DEFAULT;
  svtav1enc->target_bitrate = PROP_TARGET_BITRATE_DEFAULT;
  svtav1enc->max_bitrate = PROP_MAX_BITRATE_DEFAULT;
  svtav1enc->max_qp_allowed = PROP_QP_MAX_QP_ALLOWED_DEFAULT;
  svtav1enc->min_qp_allowed = PROP_QP_MIN_QP_ALLOWED_DEFAULT;
  svtav1enc->cqp = PROP_CQP_DEFAULT;
  svtav1enc->crf = PROP_CRF_DEFAULT;
  svtav1enc->maximum_buffer_size = PROP_MAXIMUM_BUFFER_SIZE_DEFAULT;
  svtav1enc->intra_period_length = PROP_INTRA_PERIOD_LENGTH_DEFAULT;
  svtav1enc->intra_refresh_type = PROP_INTRA_REFRESH_TYPE_DEFAULT;
  svtav1enc->logical_processors = PROP_LOGICAL_PROCESSORS_DEFAULT;
  svtav1enc->target_socket = PROP_TARGET_SOCKET_DEFAULT;
  svtav1enc->parameters_string = PROP_PARAMETERS_STRING_DEFAULT;
}

static void
gst_svtav1enc_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstSvtAv1Enc *svtav1enc = GST_SVTAV1ENC (object);

  /* TODO: support reconfiguring on the fly when possible */
  if (svtav1enc->state) {
    GST_ERROR_OBJECT (svtav1enc,
        "encoder state has been set before properties, this isn't supported yet.");
    return;
  }

  GST_LOG_OBJECT (svtav1enc, "setting property %u", property_id);

  switch (property_id) {
    case PROP_PRESET:
      svtav1enc->preset = g_value_get_uint (value);
      break;
    case PROP_TARGET_BITRATE:
      svtav1enc->target_bitrate = g_value_get_uint (value) * 1000;
      break;
    case PROP_MAX_BITRATE:
      svtav1enc->max_bitrate = g_value_get_uint (value) * 1000;
      break;
    case PROP_MAX_QP_ALLOWED:
      svtav1enc->max_qp_allowed = g_value_get_uint (value);
      break;
    case PROP_MIN_QP_ALLOWED:
      svtav1enc->min_qp_allowed = g_value_get_uint (value);
      break;
    case PROP_CQP:
      svtav1enc->cqp = g_value_get_int (value);
      break;
    case PROP_CRF:
      svtav1enc->crf = g_value_get_int (value);
      break;
    case PROP_MAXIMUM_BUFFER_SIZE:
      svtav1enc->maximum_buffer_size = g_value_get_uint (value);
      break;
    case PROP_INTRA_PERIOD_LENGTH:
      svtav1enc->intra_period_length = g_value_get_int (value);
      break;
    case PROP_INTRA_REFRESH_TYPE:
      svtav1enc->intra_refresh_type = g_value_get_enum (value);
      break;
    case PROP_LOGICAL_PROCESSORS:
      svtav1enc->logical_processors = g_value_get_uint (value);
      break;
    case PROP_TARGET_SOCKET:
      svtav1enc->target_socket = g_value_get_int (value);
      break;
    case PROP_PARAMETERS_STRING:{
      g_free (svtav1enc->parameters_string);
      svtav1enc->parameters_string = g_value_dup_string (value);
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
gst_svtav1enc_get_property (GObject * object, guint property_id, GValue * value,
    GParamSpec * pspec)
{
  GstSvtAv1Enc *svtav1enc = GST_SVTAV1ENC (object);

  GST_LOG_OBJECT (svtav1enc, "getting property %u", property_id);

  switch (property_id) {
    case PROP_PRESET:
      g_value_set_uint (value, svtav1enc->preset);
      break;
    case PROP_TARGET_BITRATE:
      g_value_set_uint (value, svtav1enc->target_bitrate / 1000);
      break;
    case PROP_MAX_BITRATE:
      g_value_set_uint (value, svtav1enc->max_bitrate / 1000);
      break;
    case PROP_MAX_QP_ALLOWED:
      g_value_set_uint (value, svtav1enc->max_qp_allowed);
      break;
    case PROP_MIN_QP_ALLOWED:
      g_value_set_uint (value, svtav1enc->min_qp_allowed);
      break;
    case PROP_CQP:
      g_value_set_int (value, svtav1enc->cqp);
      break;
    case PROP_CRF:
      g_value_set_int (value, svtav1enc->crf);
      break;
    case PROP_MAXIMUM_BUFFER_SIZE:
      g_value_set_uint (value, svtav1enc->maximum_buffer_size);
      break;
    case PROP_INTRA_PERIOD_LENGTH:
      g_value_set_int (value, svtav1enc->intra_period_length);
      break;
    case PROP_INTRA_REFRESH_TYPE:
      g_value_set_enum (value, svtav1enc->intra_refresh_type);
      break;
    case PROP_LOGICAL_PROCESSORS:
      g_value_set_uint (value, svtav1enc->logical_processors);
      break;
    case PROP_TARGET_SOCKET:
      g_value_set_int (value, svtav1enc->target_socket);
      break;
    case PROP_PARAMETERS_STRING:
      g_value_set_string (value, svtav1enc->parameters_string);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
gst_svtav1enc_finalize (GObject * object)
{
  GstSvtAv1Enc *svtav1enc = GST_SVTAV1ENC (object);

  GST_DEBUG_OBJECT (svtav1enc, "finalizing svtav1enc");

  g_free (svtav1enc->svt_config);
  g_free (svtav1enc->parameters_string);

  G_OBJECT_CLASS (gst_svtav1enc_parent_class)->finalize (object);
}

static void
gst_svtav1enc_allocate_svt_buffers (GstSvtAv1Enc * svtav1enc)
{
  svtav1enc->input_buf = g_new0 (EbBufferHeaderType, 1);
  svtav1enc->input_buf->p_buffer = (guint8 *) g_new0 (EbSvtIOFormat, 1);
  svtav1enc->input_buf->size = sizeof (EbBufferHeaderType);
  svtav1enc->input_buf->p_app_private = NULL;
  svtav1enc->input_buf->pic_type = EB_AV1_INVALID_PICTURE;
  svtav1enc->input_buf->metadata = NULL;
}

static void
gst_svtav1enc_deallocate_svt_buffers (GstSvtAv1Enc * svtav1enc)
{
  if (svtav1enc->input_buf) {
    g_free (svtav1enc->input_buf->p_buffer);
    svtav1enc->input_buf->p_buffer = NULL;
    g_free (svtav1enc->input_buf);
    svtav1enc->input_buf = NULL;
  }
}

static gboolean
gst_svtav1enc_configure_svt (GstSvtAv1Enc * svtav1enc)
{
  if (!svtav1enc->state) {
    GST_WARNING_OBJECT (svtav1enc, "no state, can't configure encoder yet");
    return FALSE;
  }

  /* set object properties */
  svtav1enc->svt_config->enc_mode = svtav1enc->preset;
  if (svtav1enc->target_bitrate != 0) {
    svtav1enc->svt_config->target_bit_rate = svtav1enc->target_bitrate;
    if (svtav1enc->target_bitrate != svtav1enc->max_bitrate) {
      GST_DEBUG_OBJECT (svtav1enc,
          "Enabling VBR mode (br %u max-br %u max-qp %u min-qp %u)",
          svtav1enc->target_bitrate,
          svtav1enc->max_bitrate,
          svtav1enc->max_qp_allowed, svtav1enc->min_qp_allowed);
      svtav1enc->svt_config->max_bit_rate = svtav1enc->max_bitrate;
      svtav1enc->svt_config->rate_control_mode = SVT_AV1_RC_MODE_VBR;
    } else {
      GST_DEBUG_OBJECT (svtav1enc,
          "Enabling CBR mode (br %u max-bs %u)",
          svtav1enc->target_bitrate, svtav1enc->maximum_buffer_size);
      svtav1enc->svt_config->rate_control_mode = SVT_AV1_RC_MODE_CBR;
      svtav1enc->svt_config->maximum_buffer_size_ms =
          svtav1enc->maximum_buffer_size;
    }
    svtav1enc->svt_config->max_qp_allowed = svtav1enc->max_qp_allowed;
    svtav1enc->svt_config->min_qp_allowed = svtav1enc->min_qp_allowed;
    svtav1enc->svt_config->force_key_frames = FALSE;
  } else if (svtav1enc->crf > 0) {
    GST_DEBUG_OBJECT (svtav1enc, "Enabling CRF mode (qp %u)", svtav1enc->crf);
    svtav1enc->svt_config->qp = svtav1enc->crf;
    svtav1enc->svt_config->rate_control_mode = SVT_AV1_RC_MODE_CQP_OR_CRF;
    svtav1enc->svt_config->force_key_frames = TRUE;
  } else if (svtav1enc->cqp > 0) {
    GST_DEBUG_OBJECT (svtav1enc, "Enabling CQP mode (qp %u)", svtav1enc->cqp);
    svtav1enc->svt_config->qp = svtav1enc->cqp;
    svtav1enc->svt_config->rate_control_mode = SVT_AV1_RC_MODE_CQP_OR_CRF;
    svtav1enc->svt_config->enable_adaptive_quantization = FALSE;
    svtav1enc->svt_config->force_key_frames = TRUE;
  } else {
    GST_DEBUG_OBJECT (svtav1enc, "Using default rate control settings");
  }
  svtav1enc->svt_config->intra_period_length = svtav1enc->intra_period_length;
  svtav1enc->svt_config->intra_refresh_type = svtav1enc->intra_refresh_type;
  svtav1enc->svt_config->logical_processors = svtav1enc->logical_processors;
  svtav1enc->svt_config->target_socket = svtav1enc->target_socket;
  gst_svtav1enc_parse_parameters_string (svtav1enc);

  /* set properties out of GstVideoInfo */
  const GstVideoInfo *info = &svtav1enc->state->info;
  svtav1enc->svt_config->encoder_bit_depth =
      GST_VIDEO_INFO_COMP_DEPTH (info, 0);
  svtav1enc->svt_config->source_width = GST_VIDEO_INFO_WIDTH (info);
  svtav1enc->svt_config->source_height = GST_VIDEO_INFO_HEIGHT (info);
  svtav1enc->svt_config->frame_rate_numerator = GST_VIDEO_INFO_FPS_N (info) > 0
      ? GST_VIDEO_INFO_FPS_N (info)
      : 1;
  svtav1enc->svt_config->frame_rate_denominator =
      GST_VIDEO_INFO_FPS_D (info) > 0 ? GST_VIDEO_INFO_FPS_D (info)
      : 1;
  GST_LOG_OBJECT (svtav1enc,
      "width %d, height %d, framerate %d/%d",
      svtav1enc->svt_config->source_width,
      svtav1enc->svt_config->source_height,
      svtav1enc->svt_config->frame_rate_numerator,
      svtav1enc->svt_config->frame_rate_denominator);

  switch (GST_VIDEO_INFO_COLORIMETRY (info).primaries) {
    case GST_VIDEO_COLOR_PRIMARIES_BT709:
      svtav1enc->svt_config->color_primaries = EB_CICP_CP_BT_709;
      break;
    case GST_VIDEO_COLOR_PRIMARIES_BT470M:
      svtav1enc->svt_config->color_primaries = EB_CICP_CP_BT_470_M;
      break;
    case GST_VIDEO_COLOR_PRIMARIES_BT470BG:
      svtav1enc->svt_config->color_primaries = EB_CICP_CP_BT_470_B_G;
      break;
    case GST_VIDEO_COLOR_PRIMARIES_SMPTE170M:
      svtav1enc->svt_config->color_primaries = EB_CICP_CP_BT_601;
      break;
    case GST_VIDEO_COLOR_PRIMARIES_SMPTE240M:
      svtav1enc->svt_config->color_primaries = EB_CICP_CP_SMPTE_240;
      break;
    case GST_VIDEO_COLOR_PRIMARIES_FILM:
      svtav1enc->svt_config->color_primaries = EB_CICP_CP_GENERIC_FILM;
      break;
    case GST_VIDEO_COLOR_PRIMARIES_BT2020:
      svtav1enc->svt_config->color_primaries = EB_CICP_CP_BT_2020;
      break;
    case GST_VIDEO_COLOR_PRIMARIES_SMPTERP431:
      svtav1enc->svt_config->color_primaries = EB_CICP_CP_SMPTE_431;
      break;
    case GST_VIDEO_COLOR_PRIMARIES_SMPTEEG432:
      svtav1enc->svt_config->color_primaries = EB_CICP_CP_SMPTE_432;
      break;
    case GST_VIDEO_COLOR_PRIMARIES_EBU3213:
      svtav1enc->svt_config->color_primaries = EB_CICP_CP_EBU_3213;
      break;
    default:
      svtav1enc->svt_config->color_primaries = EB_CICP_CP_UNSPECIFIED;
      break;
  }

  switch (GST_VIDEO_INFO_COLORIMETRY (info).transfer) {
    case GST_VIDEO_TRANSFER_BT709:
      svtav1enc->svt_config->transfer_characteristics = EB_CICP_TC_BT_709;
      break;
    case GST_VIDEO_TRANSFER_GAMMA28:
      svtav1enc->svt_config->transfer_characteristics = EB_CICP_TC_BT_470_B_G;
      break;
    case GST_VIDEO_TRANSFER_BT601:
      svtav1enc->svt_config->transfer_characteristics = EB_CICP_TC_BT_601;
      break;
    case GST_VIDEO_TRANSFER_SMPTE240M:
      svtav1enc->svt_config->transfer_characteristics = EB_CICP_TC_SMPTE_240;
      break;
    case GST_VIDEO_TRANSFER_GAMMA10:
      svtav1enc->svt_config->transfer_characteristics = EB_CICP_TC_LINEAR;
      break;
    case GST_VIDEO_TRANSFER_LOG100:
      svtav1enc->svt_config->transfer_characteristics = EB_CICP_TC_LOG_100;
      break;
    case GST_VIDEO_TRANSFER_LOG316:
      svtav1enc->svt_config->transfer_characteristics =
          EB_CICP_TC_LOG_100_SQRT10;
      break;
    case GST_VIDEO_TRANSFER_SRGB:
      svtav1enc->svt_config->transfer_characteristics = EB_CICP_TC_SRGB;
      break;
    case GST_VIDEO_TRANSFER_BT2020_10:
      svtav1enc->svt_config->transfer_characteristics =
          EB_CICP_TC_BT_2020_10_BIT;
      break;
    case GST_VIDEO_TRANSFER_BT2020_12:
      svtav1enc->svt_config->transfer_characteristics =
          EB_CICP_TC_BT_2020_12_BIT;
      break;
    case GST_VIDEO_TRANSFER_SMPTE2084:
      svtav1enc->svt_config->transfer_characteristics = EB_CICP_TC_SMPTE_2084;
      break;
    case GST_VIDEO_TRANSFER_ARIB_STD_B67:
      svtav1enc->svt_config->transfer_characteristics = EB_CICP_TC_HLG;
      break;
    default:
      svtav1enc->svt_config->transfer_characteristics = EB_CICP_TC_UNSPECIFIED;
      break;
  }

  switch (GST_VIDEO_INFO_COLORIMETRY (info).matrix) {
    case GST_VIDEO_COLOR_MATRIX_RGB:
      svtav1enc->svt_config->matrix_coefficients = EB_CICP_MC_IDENTITY;
      break;
    case GST_VIDEO_COLOR_MATRIX_BT709:
      svtav1enc->svt_config->matrix_coefficients = EB_CICP_MC_BT_709;
      break;
    case GST_VIDEO_COLOR_MATRIX_FCC:
      svtav1enc->svt_config->matrix_coefficients = EB_CICP_MC_FCC;
      break;
    case GST_VIDEO_COLOR_MATRIX_BT601:
      svtav1enc->svt_config->matrix_coefficients = EB_CICP_MC_BT_601;
      break;
    case GST_VIDEO_COLOR_MATRIX_SMPTE240M:
      svtav1enc->svt_config->matrix_coefficients = EB_CICP_MC_SMPTE_240;
      break;
    case GST_VIDEO_COLOR_MATRIX_BT2020:
      svtav1enc->svt_config->matrix_coefficients = EB_CICP_MC_BT_2020_NCL;
      break;

    default:
      svtav1enc->svt_config->matrix_coefficients = EB_CICP_MC_UNSPECIFIED;
      break;
  }

  if (GST_VIDEO_INFO_COLORIMETRY (info).range == GST_VIDEO_COLOR_RANGE_0_255) {
    svtav1enc->svt_config->color_range = EB_CR_FULL_RANGE;
  } else {
    svtav1enc->svt_config->color_range = EB_CR_STUDIO_RANGE;
  }

  switch (GST_VIDEO_INFO_CHROMA_SITE (info)) {
    case GST_VIDEO_CHROMA_SITE_V_COSITED:
      svtav1enc->svt_config->chroma_sample_position = EB_CSP_VERTICAL;
      break;
    case GST_VIDEO_CHROMA_SITE_COSITED:
      svtav1enc->svt_config->chroma_sample_position = EB_CSP_COLOCATED;
      break;
    default:
      svtav1enc->svt_config->chroma_sample_position = EB_CSP_UNKNOWN;
  }

  GstVideoMasteringDisplayInfo master_display_info;
  if (gst_video_mastering_display_info_from_caps (&master_display_info,
          svtav1enc->state->caps)) {
    svtav1enc->svt_config->mastering_display.r.x =
        master_display_info.display_primaries[0].x;
    svtav1enc->svt_config->mastering_display.r.y =
        master_display_info.display_primaries[0].y;
    svtav1enc->svt_config->mastering_display.g.x =
        master_display_info.display_primaries[1].x;
    svtav1enc->svt_config->mastering_display.g.y =
        master_display_info.display_primaries[1].y;
    svtav1enc->svt_config->mastering_display.b.x =
        master_display_info.display_primaries[2].x;
    svtav1enc->svt_config->mastering_display.b.y =
        master_display_info.display_primaries[2].y;
    svtav1enc->svt_config->mastering_display.white_point.x =
        master_display_info.white_point.x;
    svtav1enc->svt_config->mastering_display.white_point.y =
        master_display_info.white_point.y;
    svtav1enc->svt_config->mastering_display.max_luma =
        master_display_info.max_display_mastering_luminance;
    svtav1enc->svt_config->mastering_display.min_luma =
        master_display_info.min_display_mastering_luminance;
    svtav1enc->svt_config->high_dynamic_range_input = TRUE;
  } else {
    memset (&svtav1enc->svt_config->mastering_display,
        0, sizeof (svtav1enc->svt_config->mastering_display));
    svtav1enc->svt_config->high_dynamic_range_input = FALSE;
  }

  GstVideoContentLightLevel content_light_level;
  if (gst_video_content_light_level_from_caps (&content_light_level,
          svtav1enc->state->caps)) {
    svtav1enc->svt_config->content_light_level.max_cll =
        content_light_level.max_content_light_level;
    svtav1enc->svt_config->content_light_level.max_fall =
        content_light_level.max_frame_average_light_level;
  } else {
    memset (&svtav1enc->svt_config->content_light_level,
        0, sizeof (svtav1enc->svt_config->content_light_level));
  }

  EbErrorType res =
      svt_av1_enc_set_parameter (svtav1enc->svt_encoder, svtav1enc->svt_config);
  if (res != EB_ErrorNone) {
    GST_ELEMENT_ERROR (svtav1enc,
        LIBRARY,
        INIT, (NULL), ("svt_av1_enc_set_parameter failed with error %d", res));
    return FALSE;
  }
  return TRUE;
}

static gboolean
gst_svtav1enc_start_svt (GstSvtAv1Enc * svtav1enc)
{
  G_LOCK (init_mutex);
  EbErrorType res = svt_av1_enc_init (svtav1enc->svt_encoder);
  G_UNLOCK (init_mutex);

  if (res != EB_ErrorNone) {
    GST_ELEMENT_ERROR (svtav1enc, LIBRARY, INIT, (NULL),
        ("svt_av1_enc_init failed with error %d", res));
    return FALSE;
  }
  return TRUE;
}

static GstFlowReturn
gst_svtav1enc_encode (GstSvtAv1Enc * svtav1enc, GstVideoCodecFrame * frame)
{
  GstFlowReturn ret = GST_FLOW_OK;
  EbErrorType res = EB_ErrorNone;
  EbBufferHeaderType *input_buffer = svtav1enc->input_buf;
  EbSvtIOFormat *input_picture_buffer =
      (EbSvtIOFormat *) svtav1enc->input_buf->p_buffer;
  GstVideoFrame video_frame;

  if (!gst_video_frame_map (&video_frame, &svtav1enc->state->info,
          frame->input_buffer, GST_MAP_READ)) {
    GST_ELEMENT_ERROR (svtav1enc, LIBRARY, ENCODE, (NULL),
        ("couldn't map input frame"));
    return GST_FLOW_ERROR;
  }

  input_picture_buffer->y_stride =
      GST_VIDEO_FRAME_COMP_STRIDE (&video_frame,
      0) / GST_VIDEO_FRAME_COMP_PSTRIDE (&video_frame, 0);
  input_picture_buffer->cb_stride =
      GST_VIDEO_FRAME_COMP_STRIDE (&video_frame,
      1) / GST_VIDEO_FRAME_COMP_PSTRIDE (&video_frame, 1);
  input_picture_buffer->cr_stride =
      GST_VIDEO_FRAME_COMP_STRIDE (&video_frame,
      2) / GST_VIDEO_FRAME_COMP_PSTRIDE (&video_frame, 2);

  input_picture_buffer->luma = GST_VIDEO_FRAME_PLANE_DATA (&video_frame, 0);
  input_picture_buffer->cb = GST_VIDEO_FRAME_PLANE_DATA (&video_frame, 1);
  input_picture_buffer->cr = GST_VIDEO_FRAME_PLANE_DATA (&video_frame, 2);

  input_buffer->n_filled_len = GST_VIDEO_FRAME_SIZE (&video_frame);

  /* Fill in Buffers Header control data */
  input_buffer->flags = 0;
  input_buffer->p_app_private = NULL;
  input_buffer->pts = frame->pts;
  input_buffer->pic_type = EB_AV1_INVALID_PICTURE;

  if (GST_VIDEO_CODEC_FRAME_IS_FORCE_KEYFRAME (frame)) {
    input_buffer->pic_type = EB_AV1_KEY_PICTURE;
  }

  input_buffer->metadata = NULL;

  res = svt_av1_enc_send_picture (svtav1enc->svt_encoder, input_buffer);
  if (res != EB_ErrorNone) {
    GST_ELEMENT_ERROR (svtav1enc, LIBRARY, ENCODE, (NULL),
        ("error in sending picture to encoder"));
    ret = GST_FLOW_ERROR;
  }
  gst_video_frame_unmap (&video_frame);

  return ret;
}

static gboolean
gst_svtav1enc_send_eos (GstSvtAv1Enc * svtav1enc)
{
  EbErrorType ret = EB_ErrorNone;

  EbBufferHeaderType input_buffer;
  input_buffer.n_alloc_len = 0;
  input_buffer.n_filled_len = 0;
  input_buffer.n_tick_count = 0;
  input_buffer.p_app_private = NULL;
  input_buffer.flags = EB_BUFFERFLAG_EOS;
  input_buffer.p_buffer = NULL;
  input_buffer.metadata = NULL;

  GST_DEBUG_OBJECT (svtav1enc, "send eos");

  ret = svt_av1_enc_send_picture (svtav1enc->svt_encoder, &input_buffer);

  if (ret != EB_ErrorNone) {
    GST_ELEMENT_ERROR (svtav1enc, LIBRARY, ENCODE, (NULL),
        ("couldn't send EOS frame."));
    return FALSE;
  }

  return (ret == EB_ErrorNone);
}

static gboolean
gst_svtav1enc_flush (GstVideoEncoder * encoder)
{
  GstFlowReturn ret =
      gst_svtav1enc_dequeue_encoded_frames (GST_SVTAV1ENC (encoder), TRUE,
      FALSE);

  return (ret != GST_FLOW_ERROR);
}

static GstFlowReturn
gst_svtav1enc_dequeue_encoded_frames (GstSvtAv1Enc * svtav1enc,
    gboolean done_sending_pics, gboolean output_frames)
{
  GstFlowReturn ret = GST_FLOW_OK;
  EbErrorType res = EB_ErrorNone;
  gboolean encode_at_eos = FALSE;

  GST_DEBUG_OBJECT (svtav1enc, "dequeue encoded frames");

  do {
    GstVideoCodecFrame *frame = NULL;
    EbBufferHeaderType *output_buf = NULL;

    res =
        svt_av1_enc_get_packet (svtav1enc->svt_encoder, &output_buf,
        done_sending_pics);

    if (output_buf != NULL)
      encode_at_eos =
          ((output_buf->flags & EB_BUFFERFLAG_EOS) == EB_BUFFERFLAG_EOS);

    if (res == EB_ErrorMax) {
      GST_ELEMENT_ERROR (svtav1enc, LIBRARY, ENCODE, (NULL), ("encode failed"));
      return GST_FLOW_ERROR;
    } else if (res != EB_NoErrorEmptyQueue && output_frames && output_buf) {
      // AV1 has no frame re-ordering so always get the oldest frame
      if (!(frame =
              gst_video_encoder_get_oldest_frame (GST_VIDEO_ENCODER
                  (svtav1enc)))) {
        svt_av1_enc_release_out_buffer (&output_buf);
        break;
      }

      if (output_buf->pic_type == EB_AV1_KEY_PICTURE
          || output_buf->pic_type == EB_AV1_INTRA_ONLY_PICTURE) {
        GST_VIDEO_CODEC_FRAME_SET_SYNC_POINT (frame);
      }

      if ((ret =
              gst_video_encoder_allocate_output_frame (GST_VIDEO_ENCODER
                  (svtav1enc), frame,
                  output_buf->n_filled_len)) != GST_FLOW_OK) {
        svt_av1_enc_release_out_buffer (&output_buf);
        gst_video_codec_frame_unref (frame);
        return ret;
      }
      gst_buffer_fill (frame->output_buffer, 0, output_buf->p_buffer,
          output_buf->n_filled_len);

      frame->pts = frame->output_buffer->pts = output_buf->pts;

      GST_LOG_OBJECT (svtav1enc,
          "#frame:%u pts:%" G_GINT64_FORMAT " SliceType:%d\n",
          frame->system_frame_number, (frame->pts), output_buf->pic_type);

      svt_av1_enc_release_out_buffer (&output_buf);
      output_buf = NULL;

      ret =
          gst_video_encoder_finish_frame (GST_VIDEO_ENCODER (svtav1enc), frame);
    }

  } while (res == EB_ErrorNone && !encode_at_eos && ret == GST_FLOW_OK);

  return ret;
}

static gboolean
gst_svtav1enc_open (GstVideoEncoder * encoder)
{
  GstSvtAv1Enc *svtav1enc = GST_SVTAV1ENC (encoder);

  GST_DEBUG_OBJECT (svtav1enc, "open");

  EbErrorType res = svt_av1_enc_init_handle (&svtav1enc->svt_encoder, NULL,
      svtav1enc->svt_config);
  if (res != EB_ErrorNone) {
    GST_ELEMENT_ERROR (svtav1enc,
        LIBRARY,
        INIT, (NULL), ("svt_av1_enc_init_handle failed with error %d", res));
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_svtav1enc_close (GstVideoEncoder * encoder)
{
  GstSvtAv1Enc *svtav1enc = GST_SVTAV1ENC (encoder);

  GST_DEBUG_OBJECT (svtav1enc, "close");

  svt_av1_enc_deinit_handle (svtav1enc->svt_encoder);
  svtav1enc->svt_encoder = NULL;
  return TRUE;
}

static gboolean
gst_svtav1enc_start (GstVideoEncoder * encoder)
{
  GstSvtAv1Enc *svtav1enc = GST_SVTAV1ENC (encoder);

  GST_DEBUG_OBJECT (svtav1enc, "start");

  gst_svtav1enc_allocate_svt_buffers (svtav1enc);
  return TRUE;
}

static gboolean
gst_svtav1enc_stop (GstVideoEncoder * encoder)
{
  GstSvtAv1Enc *svtav1enc = GST_SVTAV1ENC (encoder);

  GST_DEBUG_OBJECT (svtav1enc, "stop");

  if (svtav1enc->state)
    gst_video_codec_state_unref (svtav1enc->state);
  svtav1enc->state = NULL;

  svt_av1_enc_deinit (svtav1enc->svt_encoder);
  gst_svtav1enc_deallocate_svt_buffers (svtav1enc);

  return TRUE;
}

static gboolean
gst_svtav1enc_set_format (GstVideoEncoder * encoder, GstVideoCodecState * state)
{
  GstSvtAv1Enc *svtav1enc = GST_SVTAV1ENC (encoder);
  GstClockTime min_latency_frames = 0;
  GstCaps *src_caps = NULL;
  GstVideoCodecState *output_state;
  GST_DEBUG_OBJECT (svtav1enc, "set_format");

  if (svtav1enc->state
      && !gst_video_info_is_equal (&svtav1enc->state->info, &state->info)) {
    gst_svtav1enc_finish (encoder);
    gst_svtav1enc_stop (encoder);
    gst_svtav1enc_close (encoder);
    gst_svtav1enc_open (encoder);
    gst_svtav1enc_start (encoder);
  }
  svtav1enc->state = gst_video_codec_state_ref (state);

  if (!gst_svtav1enc_configure_svt (svtav1enc))
    return FALSE;
  if (!gst_svtav1enc_start_svt (svtav1enc))
    return FALSE;

  guint32 fps = svtav1enc->svt_config->frame_rate_numerator /
      svtav1enc->svt_config->frame_rate_denominator;
  fps = fps > 120 ? 120 : fps;
  fps = fps < 24 ? 24 : fps;

  min_latency_frames = ((fps * 5) >> 2);

  gst_video_encoder_set_latency (encoder,
      min_latency_frames * GST_SECOND /
      (svtav1enc->svt_config->frame_rate_numerator /
          svtav1enc->svt_config->frame_rate_denominator), -1);

  src_caps = gst_static_pad_template_get_caps (&gst_svtav1enc_src_pad_template);
  output_state =
      gst_video_encoder_set_output_state (GST_VIDEO_ENCODER (encoder), src_caps,
      svtav1enc->state);
  gst_video_codec_state_unref (output_state);

  GST_DEBUG_OBJECT (svtav1enc, "output caps: %" GST_PTR_FORMAT,
      svtav1enc->state->caps);

  return gst_video_encoder_negotiate (encoder);
}

static GstFlowReturn
gst_svtav1enc_handle_frame (GstVideoEncoder * encoder,
    GstVideoCodecFrame * frame)
{
  GstSvtAv1Enc *svtav1enc = GST_SVTAV1ENC (encoder);
  GstFlowReturn ret = GST_FLOW_OK;

  GST_DEBUG_OBJECT (svtav1enc, "handle_frame");

  ret = gst_svtav1enc_encode (svtav1enc, frame);
  gst_video_codec_frame_unref (frame);
  if (ret != GST_FLOW_OK) {
    GST_DEBUG_OBJECT (svtav1enc, "gst_svtav1enc_encode returned %d", ret);
    return ret;
  }

  return gst_svtav1enc_dequeue_encoded_frames (svtav1enc, FALSE, TRUE);
}

static GstFlowReturn
gst_svtav1enc_finish (GstVideoEncoder * encoder)
{
  GstFlowReturn ret = GST_FLOW_OK;
  GstSvtAv1Enc *svtav1enc = GST_SVTAV1ENC (encoder);

  GST_DEBUG_OBJECT (svtav1enc, "finish");

  if (svtav1enc->state) {
    gst_svtav1enc_send_eos (svtav1enc);
    ret = gst_svtav1enc_dequeue_encoded_frames (svtav1enc, TRUE, TRUE);
  }

  return ret;
}

static gboolean
gst_svtav1enc_propose_allocation (GstVideoEncoder * encoder, GstQuery * query)
{
  GstSvtAv1Enc *svtav1enc = GST_SVTAV1ENC (encoder);

  GST_DEBUG_OBJECT (svtav1enc, "propose_allocation");

  gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, NULL);

  return
      GST_VIDEO_ENCODER_CLASS (gst_svtav1enc_parent_class)->propose_allocation
      (encoder, query);
}

static void
gst_svtav1enc_parse_parameters_string (GstSvtAv1Enc * svtav1enc)
{
  gchar **key_values, **p;

  if (!svtav1enc->parameters_string)
    return;

  p = key_values = g_strsplit (svtav1enc->parameters_string, ":", -1);
  while (p && *p) {
    gchar *equals;
    EbErrorType res;

    equals = strchr (*p, '=');
    if (!equals) {
      p++;
      continue;
    }

    *equals = '\0';
    equals++;

    GST_DEBUG_OBJECT (svtav1enc, "Setting parameter %s=%s", *p, equals);

    res = svt_av1_enc_parse_parameter (svtav1enc->svt_config, *p, equals);
    if (res != EB_ErrorNone) {
      GST_WARNING_OBJECT (svtav1enc, "Failed to set parameter %s=%s: %d", *p,
          equals, res);
    }

    p++;
  }

  g_strfreev (key_values);
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  return GST_ELEMENT_REGISTER (svtav1enc, plugin);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR, GST_VERSION_MINOR, svtav1,
    "Scalable Video Technology for AV1 (SVT-AV1)", plugin_init,
    VERSION, "LGPL", PACKAGE_NAME, GST_PACKAGE_ORIGIN)
