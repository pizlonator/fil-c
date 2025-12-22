/* GStreamer
 * Copyright (C) <2017> Sean DuBois <sean@siobud.com>
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
 * SECTION:element-av1enc
 *
 * AV1 Encoder.
 *
 * ## Example launch line
 *
 * |[
 * gst-launch-1.0 videotestsrc num-buffers=50 ! av1enc ! webmmux ! filesink location=av1.webm
 * ]|
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstav1enc.h"
#include "gstav1utils.h"
#include <gst/video/video.h>
#include <gst/video/gstvideometa.h>
#include <gst/base/base.h>

#define GST_AV1_ENC_APPLY_CODEC_CONTROL(av1enc, flag, value)             \
  if (av1enc->encoder_inited) {                                        \
    if (aom_codec_control (&av1enc->encoder, flag,                     \
            value) != AOM_CODEC_OK) {                                  \
      gst_av1_codec_error (&av1enc->encoder, "Failed to set " #flag);  \
    }                                                                  \
  }

GST_DEBUG_CATEGORY_STATIC (av1_enc_debug);
#define GST_CAT_DEFAULT av1_enc_debug

#define GST_TYPE_RESIZE_MODE (gst_resize_mode_get_type())
static GType
gst_resize_mode_get_type (void)
{
  static GType resize_mode_type = 0;
  static const GEnumValue resize_mode[] = {
    {GST_AV1_ENC_RESIZE_NONE, "No frame resizing allowed", "none"},
    {GST_AV1_ENC_RESIZE_FIXED, "All frames are coded at the specified scale",
        "fixed"},
    {GST_AV1_ENC_RESIZE_RANDOM, "All frames are coded at a random scale",
        "random"},
    {0, NULL, NULL},
  };

  if (!resize_mode_type) {
    resize_mode_type =
        g_enum_register_static ("GstAV1EncResizeMode", resize_mode);
  }
  return resize_mode_type;
}

#define GST_TYPE_SUPERRES_MODE (gst_superres_mode_get_type())
static GType
gst_superres_mode_get_type (void)
{
  static GType superres_mode_type = 0;
  static const GEnumValue superres_mode[] = {
    {GST_AV1_ENC_SUPERRES_NONE, "No frame superres allowed", "none"},
    {GST_AV1_ENC_SUPERRES_FIXED,
          "All frames are coded at the specified scale and super-resolved",
        "fixed"},
    {GST_AV1_ENC_SUPERRES_RANDOM,
          "All frames are coded at a random scale and super-resolved",
        "random"},
    {GST_AV1_ENC_SUPERRES_QTHRESH,
          "Superres scale for a frame is determined based on q_index",
        "qthresh"},
    {0, NULL, NULL},
  };

  if (!superres_mode_type) {
    superres_mode_type =
        g_enum_register_static ("GstAV1EncSuperresMode", superres_mode);
  }
  return superres_mode_type;
}

#define GST_TYPE_END_USAGE_MODE (gst_end_usage_mode_get_type())
static GType
gst_end_usage_mode_get_type (void)
{
  static GType end_usage_mode_type = 0;
  static const GEnumValue end_usage_mode[] = {
    {GST_AV1_ENC_END_USAGE_VBR, "Variable Bit Rate Mode", "vbr"},
    {GST_AV1_ENC_END_USAGE_CBR, "Constant Bit Rate Mode", "cbr"},
    {GST_AV1_ENC_END_USAGE_CQ, "Constrained Quality Mode", "cq"},
    {GST_AV1_ENC_END_USAGE_Q, "Constant Quality Mode", "q"},
    {0, NULL, NULL},
  };

  if (!end_usage_mode_type) {
    end_usage_mode_type =
        g_enum_register_static ("GstAV1EncEndUsageMode", end_usage_mode);
  }
  return end_usage_mode_type;
}

#define GST_TYPE_KF_MODE (gst_kf_mode_get_type())
static GType
gst_kf_mode_get_type (void)
{
  static GType kf_mode_type = 0;
  static const GEnumValue kf_mode[] = {
    {GST_AV1_ENC_KF_AUTO,
          "Encoder determines optimal keyframe placement automatically",
        "auto"},
    {GST_AV1_ENC_KF_DISABLED, "Encoder does not place keyframes", "disabled"},
    {0, NULL, NULL},
  };

  if (!kf_mode_type) {
    kf_mode_type = g_enum_register_static ("GstAV1EncKFMode", kf_mode);
  }
  return kf_mode_type;
}

#define GST_TYPE_ENC_PASS (gst_enc_pass_get_type())
static GType
gst_enc_pass_get_type (void)
{
  static GType enc_pass_type = 0;
  static const GEnumValue enc_pass[] = {
    {GST_AV1_ENC_ONE_PASS, "Single pass mode", "one-pass"},
    {GST_AV1_ENC_FIRST_PASS, "First pass of multi-pass mode", "first-pass"},
    {GST_AV1_ENC_SECOND_PASS, "Second pass of multi-pass mode", "second-pass"},
    {GST_AV1_ENC_THIRD_PASS, "Third pass of multi-pass mode", "third-pass"},
    {0, NULL, NULL},
  };

  if (!enc_pass_type) {
    enc_pass_type = g_enum_register_static ("GstAV1EncEncPass", enc_pass);
  }
  return enc_pass_type;
}

#define GST_TYPE_USAGE_PROFILE (gst_usage_profile_get_type())
static GType
gst_usage_profile_get_type (void)
{
  static GType usage_profile_type = 0;
  static const GEnumValue usage_profile[] = {
    {GST_AV1_ENC_USAGE_GOOD_QUALITY, "Good Quality profile", "good-quality"},
    {GST_AV1_ENC_USAGE_REALTIME, "Realtime profile", "realtime"},
    {GST_AV1_ENC_USAGE_ALL_INTRA, "All Intra profile", "all-intra"},
    {0, NULL, NULL},
  };

  if (!usage_profile_type) {
    usage_profile_type =
        g_enum_register_static ("GstAV1EncUsageProfile", usage_profile);
  }
  return usage_profile_type;
}

enum
{
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_CPU_USED,
  PROP_DROP_FRAME,
  PROP_RESIZE_MODE,
  PROP_RESIZE_DENOMINATOR,
  PROP_RESIZE_KF_DENOMINATOR,
  PROP_SUPERRES_MODE,
  PROP_SUPERRES_DENOMINATOR,
  PROP_SUPERRES_KF_DENOMINATOR,
  PROP_SUPERRES_QTHRESH,
  PROP_SUPERRES_KF_QTHRESH,
  PROP_END_USAGE,
  PROP_TARGET_BITRATE,
  PROP_MIN_QUANTIZER,
  PROP_MAX_QUANTIZER,
  PROP_UNDERSHOOT_PCT,
  PROP_OVERSHOOT_PCT,
  PROP_BUF_SZ,
  PROP_BUF_INITIAL_SZ,
  PROP_BUF_OPTIMAL_SZ,
  PROP_THREADS,
  PROP_ROW_MT,
  PROP_TILE_COLUMNS,
  PROP_TILE_ROWS,
  PROP_KF_MODE,
  PROP_ENC_PASS,
  PROP_USAGE_PROFILE,
  PROP_LAG_IN_FRAMES,
  PROP_KEYFRAME_MAX_DIST
};

/* From av1/av1_cx_iface.c */
#define DEFAULT_PROFILE                                         0
#define DEFAULT_CPU_USED                                        0
#define DEFAULT_DROP_FRAME                                      0
#define DEFAULT_RESIZE_MODE               GST_AV1_ENC_RESIZE_NONE
#define DEFAULT_RESIZE_DENOMINATOR                              8
#define DEFAULT_RESIZE_KF_DENOMINATOR                           8
#define DEFAULT_SUPERRES_MODE           GST_AV1_ENC_SUPERRES_NONE
#define DEFAULT_SUPERRES_DENOMINATOR                            8
#define DEFAULT_SUPERRES_KF_DENOMINATOR                         8
#define DEFAULT_SUPERRES_QTHRESH                               63
#define DEFAULT_SUPERRES_KF_QTHRESH                            63
#define DEFAULT_END_USAGE               GST_AV1_ENC_END_USAGE_VBR
#define DEFAULT_TARGET_BITRATE                                256
#define DEFAULT_MIN_QUANTIZER                                   0
#define DEFAULT_MAX_QUANTIZER                                   0
#define DEFAULT_UNDERSHOOT_PCT                                 25
#define DEFAULT_OVERSHOOT_PCT                                  25
#define DEFAULT_BUF_SZ                                       6000
#define DEFAULT_BUF_INITIAL_SZ                               4000
#define DEFAULT_BUF_OPTIMAL_SZ                               5000
#define DEFAULT_TIMEBASE_N                                      1
#define DEFAULT_TIMEBASE_D                                  90000
#define DEFAULT_BIT_DEPTH                              AOM_BITS_8
#define DEFAULT_THREADS                                         0
#define DEFAULT_ROW_MT                                       TRUE
#define DEFAULT_TILE_COLUMNS                                    0
#define DEFAULT_TILE_ROWS                                       0
#define DEFAULT_KF_MODE                       GST_AV1_ENC_KF_AUTO
#define DEFAULT_ENC_PASS                     GST_AV1_ENC_ONE_PASS
#define DEFAULT_USAGE_PROFILE      GST_AV1_ENC_USAGE_GOOD_QUALITY
#define DEFAULT_LAG_IN_FRAMES                                   0
#define DEFAULT_KEYFRAME_MAX_DIST                              30

static void gst_av1_enc_finalize (GObject * object);
static void gst_av1_enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_av1_enc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_av1_enc_start (GstVideoEncoder * encoder);
static gboolean gst_av1_enc_stop (GstVideoEncoder * encoder);
static gboolean gst_av1_enc_set_format (GstVideoEncoder * encoder,
    GstVideoCodecState * state);
static GstFlowReturn gst_av1_enc_handle_frame (GstVideoEncoder * encoder,
    GstVideoCodecFrame * frame);
static GstFlowReturn gst_av1_enc_finish (GstVideoEncoder * encoder);
static gboolean gst_av1_enc_propose_allocation (GstVideoEncoder * encoder,
    GstQuery * query);

static void gst_av1_enc_destroy_encoder (GstAV1Enc * av1enc);

#define gst_av1_enc_parent_class parent_class
G_DEFINE_TYPE (GstAV1Enc, gst_av1_enc, GST_TYPE_VIDEO_ENCODER);
GST_ELEMENT_REGISTER_DEFINE (av1enc, "av1enc", GST_RANK_PRIMARY,
    GST_TYPE_AV1_ENC);

/* *INDENT-OFF* */
static GstStaticPadTemplate gst_av1_enc_sink_pad_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
        GST_STATIC_CAPS ("video/x-raw, "
        "format = (string) { I420, Y42B, Y444, YV12 }, "
        "framerate = (fraction) [0, MAX], "
        "width = (int) [ 4, 65536 ], "
        "height = (int) [ 4, 65536 ]")
    );
/* *INDENT-ON* */

static GstStaticPadTemplate gst_av1_enc_src_pad_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-av1, "
        "stream-format = (string) obu-stream, " "alignment = (string) tu")
    );

static void
gst_av1_enc_class_init (GstAV1EncClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *element_class;
  GstVideoEncoderClass *venc_class;

  gobject_class = (GObjectClass *) klass;
  element_class = (GstElementClass *) klass;
  venc_class = (GstVideoEncoderClass *) klass;

  parent_class = g_type_class_peek_parent (klass);

  gobject_class->finalize = gst_av1_enc_finalize;
  gobject_class->set_property = gst_av1_enc_set_property;
  gobject_class->get_property = gst_av1_enc_get_property;

  gst_element_class_add_static_pad_template (element_class,
      &gst_av1_enc_sink_pad_template);
  gst_element_class_add_static_pad_template (element_class,
      &gst_av1_enc_src_pad_template);
  gst_element_class_set_static_metadata (element_class, "AV1 Encoder",
      "Codec/Encoder/Video", "Encode AV1 video streams",
      "Sean DuBois <sean@siobud.com>");

  venc_class->start = gst_av1_enc_start;
  venc_class->stop = gst_av1_enc_stop;
  venc_class->set_format = gst_av1_enc_set_format;
  venc_class->handle_frame = gst_av1_enc_handle_frame;
  venc_class->finish = gst_av1_enc_finish;
  venc_class->propose_allocation = gst_av1_enc_propose_allocation;

  klass->codec_algo = &aom_codec_av1_cx_algo;
  GST_DEBUG_CATEGORY_INIT (av1_enc_debug, "av1enc", 0, "AV1 encoding element");

  g_object_class_install_property (gobject_class, PROP_CPU_USED,
      g_param_spec_int ("cpu-used", "CPU Used",
          "CPU Used. A Value greater than 0 will increase encoder speed at the expense of quality.",
          0, 10, DEFAULT_CPU_USED, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /* Rate control configurations */
  g_object_class_install_property (gobject_class, PROP_DROP_FRAME,
      g_param_spec_uint ("drop-frame", "Drop frame",
          "Temporal resampling configuration, drop frames as a strategy to meet "
          "its target data rate Set to zero (0) to disable this feature.",
          0, G_MAXUINT, DEFAULT_DROP_FRAME,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_RESIZE_MODE,
      g_param_spec_enum ("resize-mode", "Resize mode",
          "Frame resize mode", GST_TYPE_RESIZE_MODE,
          DEFAULT_RESIZE_MODE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_RESIZE_DENOMINATOR,
      g_param_spec_uint ("resize-denominator", "Resize denominator",
          "Frame resize denominator, assuming 8 as the numerator",
          8, 16, DEFAULT_RESIZE_DENOMINATOR,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_RESIZE_KF_DENOMINATOR,
      g_param_spec_uint ("resize-kf-denominator", "Resize keyframe denominator",
          "Frame resize keyframe denominator, assuming 8 as the numerator",
          8, 16, DEFAULT_RESIZE_KF_DENOMINATOR,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_SUPERRES_MODE,
      g_param_spec_enum ("superres-mode", "Super-resolution scaling mode",
          "It integrates upscaling after the encode/decode process",
          GST_TYPE_SUPERRES_MODE,
          DEFAULT_SUPERRES_MODE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_SUPERRES_DENOMINATOR,
      g_param_spec_uint ("superres-denominator", "Super-resolution denominator",
          "Frame super-resolution denominator, used only by SUPERRES_FIXED mode",
          8, 16, DEFAULT_SUPERRES_DENOMINATOR,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_SUPERRES_KF_DENOMINATOR,
      g_param_spec_uint ("superres-kf-denominator",
          "Keyframe super-resolution denominator",
          "Keyframe super-resolution denominator",
          8, 16, DEFAULT_SUPERRES_KF_DENOMINATOR,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_SUPERRES_QTHRESH,
      g_param_spec_uint ("superres-qthresh",
          "Frame super-resolution qindex threshold",
          "Frame super-resolution qindex threshold, used only by SUPERRES_QTHRESH mode",
          1, 63, DEFAULT_SUPERRES_QTHRESH,
          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_SUPERRES_KF_QTHRESH,
      g_param_spec_uint ("superres-kf-qthresh",
          "Keyframe super-resolution qindex threshold",
          "Keyframe super-resolution qindex threshold, used only by SUPERRES_QTHRESH mode",
          1, 63, DEFAULT_SUPERRES_KF_QTHRESH,
          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_END_USAGE,
      g_param_spec_enum ("end-usage", "Rate control mode",
          "Rate control algorithm to use, indicates the end usage of this stream",
          GST_TYPE_END_USAGE_MODE, DEFAULT_END_USAGE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_TARGET_BITRATE,
      g_param_spec_uint ("target-bitrate", "Target bitrate",
          "Target bitrate, in kilobits per second",
          1, G_MAXUINT, DEFAULT_TARGET_BITRATE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_MIN_QUANTIZER,
      g_param_spec_uint ("min-quantizer", "Minimum (best quality) quantizer",
          "Minimum (best quality) quantizer",
          0, G_MAXUINT, DEFAULT_MIN_QUANTIZER,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_MAX_QUANTIZER,
      g_param_spec_uint ("max-quantizer", "Maximum (worst quality) quantizer",
          "Maximum (worst quality) quantizer",
          0, G_MAXUINT, DEFAULT_MAX_QUANTIZER,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_UNDERSHOOT_PCT,
      g_param_spec_uint ("undershoot-pct", "Datarate undershoot (min) target",
          "Rate control adaptation undershoot control",
          0, 1000, DEFAULT_UNDERSHOOT_PCT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_OVERSHOOT_PCT,
      g_param_spec_uint ("overshoot-pct", "Datarate overshoot (max) target",
          "Rate control adaptation overshoot control",
          0, 1000, DEFAULT_OVERSHOOT_PCT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_BUF_SZ,
      g_param_spec_uint ("buf-sz", "Decoder buffer size",
          "Decoder buffer size, expressed in units of time (milliseconds)",
          0, G_MAXUINT, DEFAULT_BUF_SZ,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_BUF_INITIAL_SZ,
      g_param_spec_uint ("buf-initial-sz", "Decoder buffer initial size",
          "Decoder buffer initial size, expressed in units of time (milliseconds)",
          0, G_MAXUINT, DEFAULT_BUF_INITIAL_SZ,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_BUF_OPTIMAL_SZ,
      g_param_spec_uint ("buf-optimal-sz", "Decoder buffer optimal size",
          "Decoder buffer optimal size, expressed in units of time (milliseconds)",
          0, G_MAXUINT, DEFAULT_BUF_OPTIMAL_SZ,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_THREADS,
      g_param_spec_uint ("threads", "Max number of threads to use",
          "Max number of threads to use encoding, set to 0 determine the "
          "approximate number of threads that the system schedule",
          0, G_MAXUINT, DEFAULT_THREADS,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

#ifdef AOM_CTRL_AV1E_SET_ROW_MT
  g_object_class_install_property (gobject_class, PROP_ROW_MT,
      g_param_spec_boolean ("row-mt", "Row based multi-threading",
          "Enable row based multi-threading",
          DEFAULT_ROW_MT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
#endif

  g_object_class_install_property (gobject_class, PROP_TILE_COLUMNS,
      g_param_spec_uint ("tile-columns", "Number of tile columns",
          "Partition into separate vertical tile columns from image frame which "
          "can enable parallel encoding",
          0, 6, DEFAULT_TILE_COLUMNS,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_TILE_ROWS,
      g_param_spec_uint ("tile-rows", "Number of tile rows",
          "Partition into separate horizontal tile rows from image frame which "
          "can enable parallel encoding",
          0, 6, DEFAULT_TILE_ROWS, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * av1enc:keyframe-mode:
   *
   * Since: 1.22
   */
  g_object_class_install_property (gobject_class, PROP_KF_MODE,
      g_param_spec_enum ("keyframe-mode", "Keyframe placement mode",
          "Determines whether keyframes are placed automatically by the encoder",
          GST_TYPE_KF_MODE, DEFAULT_KF_MODE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * av1enc:enc-pass:
   *
   * Since: 1.22
   */
  g_object_class_install_property (gobject_class, PROP_ENC_PASS,
      g_param_spec_enum ("enc-pass", "Multi-pass Encoding Pass",
          "Current phase for multi-pass encoding or @GST_AV1_ENC_ONE_PASS for single pass",
          GST_TYPE_ENC_PASS, DEFAULT_ENC_PASS,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * av1enc:usage-profile:
   *
   * Since: 1.22
   */
  g_object_class_install_property (gobject_class, PROP_USAGE_PROFILE,
      g_param_spec_enum ("usage-profile", "Usage value",
          "Usage profile is used to guide the default config for the encoder",
          GST_TYPE_USAGE_PROFILE, DEFAULT_USAGE_PROFILE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * av1enc:lag-in-frames:
   *
   * Since: 1.22
   */
  g_object_class_install_property (gobject_class, PROP_LAG_IN_FRAMES,
      g_param_spec_uint ("lag-in-frames", "Allow lagged encoding",
          "Maximum number of future frames the encoder is allowed to consume "
          "before producing the current output frame. "
          "Set value to 0 for disabling lagged encoding.",
          0, G_MAXUINT, DEFAULT_LAG_IN_FRAMES,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * av1enc:keyframe-max-dist:
   *
   * Since: 1.22
   */
  g_object_class_install_property (gobject_class, PROP_KEYFRAME_MAX_DIST,
      g_param_spec_int ("keyframe-max-dist", "Keyframe max distance",
          "Maximum distance between keyframes (number of frames)",
          0, G_MAXINT, DEFAULT_KEYFRAME_MAX_DIST,
          (GParamFlags) (G_PARAM_READWRITE |
              G_PARAM_STATIC_STRINGS | GST_PARAM_DOC_SHOW_DEFAULT)));

  gst_type_mark_as_plugin_api (GST_TYPE_END_USAGE_MODE, 0);
  gst_type_mark_as_plugin_api (GST_TYPE_RESIZE_MODE, 0);
  gst_type_mark_as_plugin_api (GST_TYPE_SUPERRES_MODE, 0);
  gst_type_mark_as_plugin_api (GST_TYPE_KF_MODE, 0);
  gst_type_mark_as_plugin_api (GST_TYPE_ENC_PASS, 0);
  gst_type_mark_as_plugin_api (GST_TYPE_USAGE_PROFILE, 0);
}

static void
gst_av1_codec_error (aom_codec_ctx_t * ctx, const char *s)
{
  const char *detail = aom_codec_error_detail (ctx);

  GST_ERROR ("%s: %s %s", s, aom_codec_error (ctx), detail ? detail : "");
}

static void
gst_av1_enc_init (GstAV1Enc * av1enc)
{
  GST_PAD_SET_ACCEPT_TEMPLATE (GST_VIDEO_ENCODER_SINK_PAD (av1enc));

  av1enc->encoder_inited = FALSE;

  av1enc->cpu_used = DEFAULT_CPU_USED;
  av1enc->format = AOM_IMG_FMT_I420;
  av1enc->threads = DEFAULT_THREADS;
  av1enc->row_mt = DEFAULT_ROW_MT;
  av1enc->tile_columns = DEFAULT_TILE_COLUMNS;
  av1enc->tile_rows = DEFAULT_TILE_ROWS;

#ifdef FIXED_QP_OFFSET_COUNT
  av1enc->aom_cfg.fixed_qp_offsets[0] = -1;
  av1enc->aom_cfg.fixed_qp_offsets[1] = -1;
  av1enc->aom_cfg.fixed_qp_offsets[2] = -1;
  av1enc->aom_cfg.fixed_qp_offsets[3] = -1;
  av1enc->aom_cfg.fixed_qp_offsets[4] = -1;
#endif
  av1enc->aom_cfg.kf_max_dist = DEFAULT_KEYFRAME_MAX_DIST;
  av1enc->aom_cfg.rc_dropframe_thresh = DEFAULT_DROP_FRAME;
  av1enc->aom_cfg.rc_resize_mode = DEFAULT_RESIZE_MODE;
  av1enc->aom_cfg.rc_resize_denominator = DEFAULT_RESIZE_DENOMINATOR;
  av1enc->aom_cfg.rc_resize_kf_denominator = DEFAULT_RESIZE_KF_DENOMINATOR;
#ifdef HAVE_LIBAOM_3
  av1enc->aom_cfg.rc_superres_mode = (aom_superres_mode) DEFAULT_SUPERRES_MODE;
#else
  av1enc->aom_cfg.rc_superres_mode = DEFAULT_SUPERRES_MODE;
#endif
  av1enc->aom_cfg.rc_superres_denominator = DEFAULT_SUPERRES_DENOMINATOR;
  av1enc->aom_cfg.rc_superres_kf_denominator = DEFAULT_SUPERRES_KF_DENOMINATOR;
  av1enc->aom_cfg.rc_superres_qthresh = DEFAULT_SUPERRES_QTHRESH;
  av1enc->aom_cfg.rc_superres_kf_qthresh = DEFAULT_SUPERRES_KF_QTHRESH;
  av1enc->aom_cfg.rc_end_usage = (enum aom_rc_mode) DEFAULT_END_USAGE;
  av1enc->aom_cfg.rc_target_bitrate = DEFAULT_TARGET_BITRATE;
  av1enc->aom_cfg.rc_min_quantizer = DEFAULT_MIN_QUANTIZER;
  av1enc->aom_cfg.rc_max_quantizer = DEFAULT_MAX_QUANTIZER;
  av1enc->aom_cfg.rc_undershoot_pct = DEFAULT_UNDERSHOOT_PCT;
  av1enc->aom_cfg.rc_overshoot_pct = DEFAULT_OVERSHOOT_PCT;
  av1enc->aom_cfg.rc_buf_sz = DEFAULT_BUF_SZ;
  av1enc->aom_cfg.rc_buf_initial_sz = DEFAULT_BUF_INITIAL_SZ;
  av1enc->aom_cfg.rc_buf_optimal_sz = DEFAULT_BUF_OPTIMAL_SZ;
  av1enc->aom_cfg.g_timebase.num = DEFAULT_TIMEBASE_N;
  av1enc->aom_cfg.g_timebase.den = DEFAULT_TIMEBASE_D;
  av1enc->aom_cfg.g_bit_depth = DEFAULT_BIT_DEPTH;
  av1enc->aom_cfg.g_input_bit_depth = (unsigned int) DEFAULT_BIT_DEPTH;
  av1enc->aom_cfg.kf_mode = (enum aom_kf_mode) DEFAULT_KF_MODE;
  av1enc->aom_cfg.g_pass = (enum aom_enc_pass) DEFAULT_ENC_PASS;
  av1enc->aom_cfg.g_usage = (unsigned int) DEFAULT_USAGE_PROFILE;
  av1enc->aom_cfg.g_lag_in_frames = DEFAULT_LAG_IN_FRAMES;

  g_mutex_init (&av1enc->encoder_lock);
}

static void
gst_av1_enc_finalize (GObject * object)
{
  GstAV1Enc *av1enc = GST_AV1_ENC (object);

  if (av1enc->input_state) {
    gst_video_codec_state_unref (av1enc->input_state);
  }
  av1enc->input_state = NULL;

  gst_av1_enc_destroy_encoder (av1enc);
  g_mutex_clear (&av1enc->encoder_lock);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_av1_enc_set_latency (GstAV1Enc * av1enc)
{
  GstClockTime latency;
  gint fps_n, fps_d;

  if (av1enc->input_state->info.fps_n && av1enc->input_state->info.fps_d) {
    fps_n = av1enc->input_state->info.fps_n;
    fps_d = av1enc->input_state->info.fps_d;
  } else {
    fps_n = 25;
    fps_d = 1;
  }

  latency =
      gst_util_uint64_scale (av1enc->aom_cfg.g_lag_in_frames * GST_SECOND,
      fps_d, fps_n);
  gst_video_encoder_set_latency (GST_VIDEO_ENCODER (av1enc), latency, latency);

  GST_DEBUG_OBJECT (av1enc, "Latency set to %" GST_TIME_FORMAT
      " = %d frames at %d/%d fps ", GST_TIME_ARGS (latency),
      av1enc->aom_cfg.g_lag_in_frames, fps_n, fps_d);
}

static const gchar *
gst_av1_enc_get_aom_rc_mode_name (enum aom_rc_mode rc_mode)
{
  switch (rc_mode) {
    case AOM_VBR:
      return "VBR (Variable Bit Rate)";
    case AOM_CBR:
      return "CBR (Constant Bit Rate)";
    case AOM_CQ:
      return "CQ (Constrained Quality)";
    case AOM_Q:
      return "Q (Constant Quality)";
    default:
      return "<UNKNOWN>";
  }
}

static void
gst_av1_enc_debug_encoder_cfg (struct aom_codec_enc_cfg *cfg)
{
  GST_DEBUG ("g_usage : %u", cfg->g_usage);
  GST_DEBUG ("g_threads : %u", cfg->g_threads);
  GST_DEBUG ("g_profile : %u", cfg->g_profile);
  GST_DEBUG ("g_w x g_h : %u x %u", cfg->g_w, cfg->g_h);
  GST_DEBUG ("g_bit_depth : %d", cfg->g_bit_depth);
  GST_DEBUG ("g_input_bit_depth : %u", cfg->g_input_bit_depth);
  GST_DEBUG ("g_timebase : %d / %d", cfg->g_timebase.num, cfg->g_timebase.den);
  GST_DEBUG ("g_error_resilient : 0x%x", cfg->g_error_resilient);
  GST_DEBUG ("g_pass : %d", cfg->g_pass);
  GST_DEBUG ("g_lag_in_frames : %u", cfg->g_lag_in_frames);
  GST_DEBUG ("rc_dropframe_thresh : %u", cfg->rc_dropframe_thresh);
  GST_DEBUG ("rc_resize_mode : %u", cfg->rc_resize_mode);
  GST_DEBUG ("rc_resize_denominator : %u", cfg->rc_resize_denominator);
  GST_DEBUG ("rc_resize_kf_denominator : %u", cfg->rc_resize_kf_denominator);
  GST_DEBUG ("rc_superres_mode : %u", cfg->rc_superres_mode);
  GST_DEBUG ("rc_superres_denominator : %u", cfg->rc_superres_denominator);
  GST_DEBUG ("rc_superres_kf_denominator : %u",
      cfg->rc_superres_kf_denominator);
  GST_DEBUG ("rc_superres_qthresh : %u", cfg->rc_superres_qthresh);
  GST_DEBUG ("rc_superres_kf_qthresh : %u", cfg->rc_superres_kf_qthresh);
  GST_DEBUG ("rc_end_usage : %s",
      gst_av1_enc_get_aom_rc_mode_name (cfg->rc_end_usage));
  /* rc_twopass_stats_in */
  /* rc_firstpass_mb_stats_in */
  GST_DEBUG ("rc_target_bitrate : %u (kbps)", cfg->rc_target_bitrate);
  GST_DEBUG ("rc_min_quantizer : %u", cfg->rc_min_quantizer);
  GST_DEBUG ("rc_max_quantizer : %u", cfg->rc_max_quantizer);
  GST_DEBUG ("rc_undershoot_pct : %u", cfg->rc_undershoot_pct);
  GST_DEBUG ("rc_overshoot_pct : %u", cfg->rc_overshoot_pct);
  GST_DEBUG ("rc_buf_sz : %u (ms)", cfg->rc_buf_sz);
  GST_DEBUG ("rc_buf_initial_sz : %u (ms)", cfg->rc_buf_initial_sz);
  GST_DEBUG ("rc_buf_optimal_sz : %u (ms)", cfg->rc_buf_optimal_sz);
  GST_DEBUG ("rc_2pass_vbr_bias_pct : %u (%%)", cfg->rc_2pass_vbr_bias_pct);
  GST_DEBUG ("rc_2pass_vbr_minsection_pct : %u (%%)",
      cfg->rc_2pass_vbr_minsection_pct);
  GST_DEBUG ("rc_2pass_vbr_maxsection_pct : %u (%%)",
      cfg->rc_2pass_vbr_maxsection_pct);
  GST_DEBUG ("kf_mode : %u", cfg->kf_mode);
  GST_DEBUG ("kf_min_dist : %u", cfg->kf_min_dist);
  GST_DEBUG ("kf_max_dist : %u", cfg->kf_max_dist);
  GST_DEBUG ("large_scale_tile : %u", cfg->large_scale_tile);
  /* Tile-related values */
}

static gint
gst_av1_enc_get_downstream_profile (GstAV1Enc * av1enc)
{
  GstCaps *allowed;
  GstStructure *s;
  gint profile = DEFAULT_PROFILE;

  allowed = gst_pad_get_allowed_caps (GST_VIDEO_ENCODER_SRC_PAD (av1enc));
  if (allowed) {
    allowed = gst_caps_truncate (allowed);
    s = gst_caps_get_structure (allowed, 0);
    if (gst_structure_has_field (s, "profile")) {
      const GValue *v = gst_structure_get_value (s, "profile");
      const gchar *profile_str = NULL;

      if (GST_VALUE_HOLDS_LIST (v) && gst_value_list_get_size (v) > 0) {
        profile_str = g_value_get_string (gst_value_list_get_value (v, 0));
      } else if (G_VALUE_HOLDS_STRING (v)) {
        profile_str = g_value_get_string (v);
      }

      if (profile_str) {
        gchar *endptr = NULL;

        if (g_strcmp0 (profile_str, "main") == 0) {
          GST_DEBUG_OBJECT (av1enc, "Downstream profile is \"main\"");
          profile = 0;
        } else if (g_strcmp0 (profile_str, "high") == 0) {
          profile = 1;
          GST_DEBUG_OBJECT (av1enc, "Downstream profile is \"high\"");
        } else if (g_strcmp0 (profile_str, "professional") == 0) {
          profile = 2;
          GST_DEBUG_OBJECT (av1enc, "Downstream profile is \"professional\"");
        } else {
          profile = g_ascii_strtoull (profile_str, &endptr, 10);
          if (*endptr != '\0' || profile < 0 || profile > 3) {
            GST_ERROR_OBJECT (av1enc, "Invalid profile '%s'", profile_str);
            profile = DEFAULT_PROFILE;
          } else {
            GST_DEBUG_OBJECT (av1enc,
                "Downstream profile is \"%s\"", profile_str);
          }
        }
      }
    }
    gst_caps_unref (allowed);
  }

  GST_DEBUG_OBJECT (av1enc, "Using profile %d", profile);

  return profile;
}

static void
gst_av1_enc_adjust_profile (GstAV1Enc * av1enc, GstVideoFormat format)
{
  guint depth = av1enc->aom_cfg.g_bit_depth;
  guint profile = av1enc->aom_cfg.g_profile;
  gboolean update = FALSE;

  switch (profile) {
    case 0:
      if (depth < 12 && format == GST_VIDEO_FORMAT_Y444) {
        profile = 1;
        update = TRUE;
      } else if (depth == 12 || format == GST_VIDEO_FORMAT_Y42B) {
        profile = 2;
        update = TRUE;
      }
      break;
    case 1:
      if (depth == 12 || format == GST_VIDEO_FORMAT_Y42B) {
        profile = 2;
        update = TRUE;
      } else if (depth < 12 && format == GST_VIDEO_FORMAT_I420) {
        profile = 0;
        update = TRUE;
      }
      break;
    case 2:
      if (depth < 12) {
        if (format == GST_VIDEO_FORMAT_Y444) {
          profile = 1;
          update = TRUE;
        } else if (format == GST_VIDEO_FORMAT_I420) {
          profile = 0;
          update = TRUE;
        }
      }
      break;
    default:
      break;
  }

  if (update) {
    GST_INFO_OBJECT (av1enc, "profile updated to %d from %d",
        profile, av1enc->aom_cfg.g_profile);
    av1enc->aom_cfg.g_profile = profile;
  }
}

static gboolean
gst_av1_enc_set_format (GstVideoEncoder * encoder, GstVideoCodecState * state)
{
  GstVideoCodecState *output_state;
  GstAV1Enc *av1enc = GST_AV1_ENC_CAST (encoder);
  GstAV1EncClass *av1enc_class = GST_AV1_ENC_GET_CLASS (av1enc);
  GstVideoInfo *info = &state->info;

  output_state =
      gst_video_encoder_set_output_state (encoder,
      gst_pad_get_pad_template_caps (GST_VIDEO_ENCODER_SRC_PAD (encoder)),
      state);
  gst_video_codec_state_unref (output_state);

  if (av1enc->input_state) {
    gst_video_codec_state_unref (av1enc->input_state);
  }
  av1enc->input_state = gst_video_codec_state_ref (state);

  g_mutex_lock (&av1enc->encoder_lock);
  gst_av1_enc_set_latency (av1enc);

  av1enc->aom_cfg.g_profile = gst_av1_enc_get_downstream_profile (av1enc);

  /* Scale default bitrate to our size */
  if (!av1enc->target_bitrate_set)
    av1enc->aom_cfg.rc_target_bitrate =
        gst_util_uint64_scale (DEFAULT_TARGET_BITRATE,
        GST_VIDEO_INFO_WIDTH (info) * GST_VIDEO_INFO_HEIGHT (info), 320 * 240);

  av1enc->aom_cfg.g_w = GST_VIDEO_INFO_WIDTH (info);
  av1enc->aom_cfg.g_h = GST_VIDEO_INFO_HEIGHT (info);
  /* Zero framerate and max-framerate but still need to setup the timebase to avoid
   * a divide by zero error. Presuming the lowest common denominator will be RTP -
   * VP8 payload draft states clock rate of 90000 which should work for anyone where
   * FPS < 90000 (shouldn't be too many cases where it's higher) though wouldn't be optimal. RTP specification
   * http://tools.ietf.org/html/draft-ietf-payload-vp8-01 section 6.3.1 */
  av1enc->aom_cfg.g_timebase.num = 1;
  av1enc->aom_cfg.g_timebase.den = 90000;
  av1enc->aom_cfg.g_error_resilient = AOM_ERROR_RESILIENT_DEFAULT;

  if (av1enc->threads == DEFAULT_THREADS)
    av1enc->aom_cfg.g_threads = g_get_num_processors ();
  else
    av1enc->aom_cfg.g_threads = av1enc->threads;
  /* TODO: do more configuration including bit_depth config */

  av1enc->format =
      gst_video_format_to_av1_img_format (GST_VIDEO_INFO_FORMAT (info));

  if (av1enc->aom_cfg.g_bit_depth != DEFAULT_BIT_DEPTH) {
    av1enc->aom_cfg.g_input_bit_depth = av1enc->aom_cfg.g_bit_depth;
    if (av1enc->aom_cfg.g_bit_depth > 8)
      av1enc->format |= AOM_IMG_FMT_HIGHBITDEPTH;
  }

  /* Adjust profile according to format and bit-depth */
  gst_av1_enc_adjust_profile (av1enc, GST_VIDEO_INFO_FORMAT (info));

  GST_DEBUG_OBJECT (av1enc, "Calling encoder init with config:");
  gst_av1_enc_debug_encoder_cfg (&av1enc->aom_cfg);

  if (aom_codec_enc_init (&av1enc->encoder, av1enc_class->codec_algo,
          &av1enc->aom_cfg, 0)) {
    gst_av1_codec_error (&av1enc->encoder, "Failed to initialize encoder");
    g_mutex_unlock (&av1enc->encoder_lock);
    return FALSE;
  }
  av1enc->encoder_inited = TRUE;
  av1enc->last_pts = GST_CLOCK_TIME_NONE;
  av1enc->last_input_duration = GST_CLOCK_TIME_NONE;

  GST_AV1_ENC_APPLY_CODEC_CONTROL (av1enc, AOME_SET_CPUUSED, av1enc->cpu_used);
#ifdef AOM_CTRL_AV1E_SET_ROW_MT
  GST_AV1_ENC_APPLY_CODEC_CONTROL (av1enc, AV1E_SET_ROW_MT,
      (av1enc->row_mt ? 1 : 0));
#endif
  GST_AV1_ENC_APPLY_CODEC_CONTROL (av1enc, AV1E_SET_TILE_COLUMNS,
      av1enc->tile_columns);
  GST_AV1_ENC_APPLY_CODEC_CONTROL (av1enc, AV1E_SET_TILE_ROWS,
      av1enc->tile_rows);
  g_mutex_unlock (&av1enc->encoder_lock);

  return TRUE;
}

static GstFlowReturn
gst_av1_enc_process (GstAV1Enc * encoder)
{
  aom_codec_iter_t iter = NULL;
  const aom_codec_cx_pkt_t *pkt;
  GstVideoCodecFrame *frame;
  GstVideoEncoder *video_encoder;
  GstFlowReturn ret = GST_FLOW_CUSTOM_SUCCESS;

  video_encoder = GST_VIDEO_ENCODER (encoder);

  while ((pkt = aom_codec_get_cx_data (&encoder->encoder, &iter)) != NULL) {
    if (pkt->kind == AOM_CODEC_STATS_PKT) {
      GST_WARNING_OBJECT (encoder, "Unhandled stats packet");
    } else if (pkt->kind == AOM_CODEC_FPMB_STATS_PKT) {
      GST_WARNING_OBJECT (encoder, "Unhandled FPMB pkt");
    } else if (pkt->kind == AOM_CODEC_PSNR_PKT) {
      GST_WARNING_OBJECT (encoder, "Unhandled PSNR packet");
    } else if (pkt->kind == AOM_CODEC_CX_FRAME_PKT) {
      frame = gst_video_encoder_get_oldest_frame (video_encoder);
      g_assert (frame != NULL);
      if ((pkt->data.frame.flags & AOM_FRAME_IS_KEY) != 0) {
        GST_VIDEO_CODEC_FRAME_SET_SYNC_POINT (frame);
      } else {
        GST_VIDEO_CODEC_FRAME_UNSET_SYNC_POINT (frame);
      }

      frame->output_buffer =
          gst_buffer_new_memdup (pkt->data.frame.buf, pkt->data.frame.sz);

      if ((pkt->data.frame.flags & AOM_FRAME_IS_DROPPABLE) != 0)
        GST_BUFFER_FLAG_SET (frame->output_buffer, GST_BUFFER_FLAG_DROPPABLE);

      ret = gst_video_encoder_finish_frame (video_encoder, frame);
      if (ret != GST_FLOW_OK)
        break;
    }
  }

  return ret;
}

static void
gst_av1_enc_fill_image (GstAV1Enc * enc, GstVideoFrame * frame,
    aom_image_t * image)
{
  image->planes[AOM_PLANE_Y] = GST_VIDEO_FRAME_COMP_DATA (frame, 0);
  image->planes[AOM_PLANE_U] = GST_VIDEO_FRAME_COMP_DATA (frame, 1);
  image->planes[AOM_PLANE_V] = GST_VIDEO_FRAME_COMP_DATA (frame, 2);

  image->stride[AOM_PLANE_Y] = GST_VIDEO_FRAME_COMP_STRIDE (frame, 0);
  image->stride[AOM_PLANE_U] = GST_VIDEO_FRAME_COMP_STRIDE (frame, 1);
  image->stride[AOM_PLANE_V] = GST_VIDEO_FRAME_COMP_STRIDE (frame, 2);
}

static GstFlowReturn
gst_av1_enc_handle_frame (GstVideoEncoder * encoder, GstVideoCodecFrame * frame)
{
  GstAV1Enc *av1enc = GST_AV1_ENC_CAST (encoder);
  aom_image_t raw;
  int flags = 0;
  GstFlowReturn ret = GST_FLOW_OK;
  GstVideoFrame vframe;
  aom_codec_pts_t scaled_pts;
  GstClockTime pts_rt;
  unsigned long duration;

  if (!aom_img_alloc (&raw, av1enc->format, av1enc->aom_cfg.g_w,
          av1enc->aom_cfg.g_h, 1)) {
    GST_ERROR_OBJECT (encoder, "Failed to initialize encoder");
    return FALSE;
  }

  gst_video_frame_map (&vframe, &av1enc->input_state->info,
      frame->input_buffer, GST_MAP_READ);
  gst_av1_enc_fill_image (av1enc, &vframe, &raw);
  gst_video_frame_unmap (&vframe);

  // aom_codec_encode requires pts to be strictly increasing
  pts_rt =
      gst_segment_to_running_time (&encoder->input_segment,
      GST_FORMAT_TIME, frame->pts);

  if (GST_CLOCK_TIME_IS_VALID (av1enc->last_pts)
      && pts_rt <= av1enc->last_pts) {
    GST_WARNING_OBJECT (av1enc,
        "decreasing pts %" GST_TIME_FORMAT " previous buffer was %"
        GST_TIME_FORMAT " enforce increasing pts", GST_TIME_ARGS (pts_rt),
        GST_TIME_ARGS (av1enc->last_pts));
    pts_rt = av1enc->last_pts + 1;
  }

  av1enc->last_pts = pts_rt;

  // Convert the pts from nanoseconds to timebase units
  scaled_pts =
      gst_util_uint64_scale (pts_rt,
      av1enc->aom_cfg.g_timebase.den,
      av1enc->aom_cfg.g_timebase.num * (GstClockTime) GST_SECOND);

  if (frame->duration != GST_CLOCK_TIME_NONE) {
    duration =
        gst_util_uint64_scale_round (frame->duration,
        av1enc->aom_cfg.g_timebase.den,
        av1enc->aom_cfg.g_timebase.num * (GstClockTime) GST_SECOND);

    if (duration > 0) {
      av1enc->last_input_duration = frame->duration;
    } else {
      /* We force the path ignoring the duration if we end up with a zero
       * value for duration after scaling (e.g. duration value too small) */
      GST_WARNING_OBJECT (av1enc,
          "Ignoring too small frame duration %" GST_TIME_FORMAT,
          GST_TIME_ARGS (frame->duration));
      duration = 1;
    }
  } else {
    duration = 1;
  }

  if (GST_VIDEO_CODEC_FRAME_IS_FORCE_KEYFRAME (frame)) {
    GST_DEBUG_OBJECT (av1enc, "Forcing keyframe for frame %u",
        frame->system_frame_number);
    flags |= AOM_EFLAG_FORCE_KF;
  }

  if (aom_codec_encode (&av1enc->encoder, &raw, scaled_pts, duration, flags)
      != AOM_CODEC_OK) {
    gst_av1_codec_error (&av1enc->encoder, "Failed to encode frame");
    ret = GST_FLOW_ERROR;
  }

  aom_img_free (&raw);
  gst_video_codec_frame_unref (frame);

  if (ret == GST_FLOW_ERROR)
    return ret;

  ret = gst_av1_enc_process (av1enc);

  if (ret == GST_FLOW_CUSTOM_SUCCESS)
    ret = GST_FLOW_OK;

  return ret;
}

static GstFlowReturn
gst_av1_enc_finish (GstVideoEncoder * encoder)
{
  GstFlowReturn ret = GST_FLOW_OK;
  GstAV1Enc *av1enc = GST_AV1_ENC_CAST (encoder);
  aom_codec_pts_t scaled_pts;
  GstClockTime pts = 0;

  while (ret == GST_FLOW_OK) {
    GST_DEBUG_OBJECT (encoder, "Calling finish");
    g_mutex_lock (&av1enc->encoder_lock);

    if (GST_CLOCK_TIME_IS_VALID (av1enc->last_pts))
      pts = av1enc->last_pts;
    if (GST_CLOCK_TIME_IS_VALID (av1enc->last_input_duration))
      pts += av1enc->last_input_duration;

    scaled_pts =
        gst_util_uint64_scale (pts,
        av1enc->aom_cfg.g_timebase.den,
        av1enc->aom_cfg.g_timebase.num * (GstClockTime) GST_SECOND);

    if (aom_codec_encode (&av1enc->encoder, NULL, scaled_pts, 1, 0)
        != AOM_CODEC_OK) {
      gst_av1_codec_error (&av1enc->encoder, "Failed to encode frame");
      ret = GST_FLOW_ERROR;
    }
    g_mutex_unlock (&av1enc->encoder_lock);

    ret = gst_av1_enc_process (av1enc);
  }


  if (ret == GST_FLOW_CUSTOM_SUCCESS)
    ret = GST_FLOW_OK;

  return ret;
}

static void
gst_av1_enc_destroy_encoder (GstAV1Enc * av1enc)
{
  g_mutex_lock (&av1enc->encoder_lock);
  if (av1enc->encoder_inited) {
    aom_codec_destroy (&av1enc->encoder);
    av1enc->encoder_inited = FALSE;
  }

  av1enc->last_pts = GST_CLOCK_TIME_NONE;
  av1enc->last_input_duration = GST_CLOCK_TIME_NONE;

  g_mutex_unlock (&av1enc->encoder_lock);
}

static gboolean
gst_av1_enc_propose_allocation (GstVideoEncoder * encoder, GstQuery * query)
{
  gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, NULL);

  return GST_VIDEO_ENCODER_CLASS (parent_class)->propose_allocation (encoder,
      query);
}

static void
gst_av1_enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstAV1Enc *av1enc = GST_AV1_ENC_CAST (object);
  gboolean global = FALSE;
  aom_codec_err_t status;

  GST_OBJECT_LOCK (av1enc);

  g_mutex_lock (&av1enc->encoder_lock);
  switch (prop_id) {
    case PROP_CPU_USED:
      av1enc->cpu_used = g_value_get_int (value);
      GST_AV1_ENC_APPLY_CODEC_CONTROL (av1enc, AOME_SET_CPUUSED,
          av1enc->cpu_used);
      break;
    case PROP_DROP_FRAME:
      av1enc->aom_cfg.rc_dropframe_thresh = g_value_get_uint (value);
      global = TRUE;
      break;
    case PROP_RESIZE_MODE:
      av1enc->aom_cfg.rc_resize_mode = g_value_get_enum (value);
      global = TRUE;
      break;
    case PROP_RESIZE_DENOMINATOR:
      av1enc->aom_cfg.rc_resize_denominator = g_value_get_uint (value);
      global = TRUE;
      break;
    case PROP_RESIZE_KF_DENOMINATOR:
      av1enc->aom_cfg.rc_resize_kf_denominator = g_value_get_uint (value);
      global = TRUE;
      break;
    case PROP_SUPERRES_MODE:
      av1enc->aom_cfg.rc_superres_mode = g_value_get_enum (value);
      global = TRUE;
      break;
    case PROP_SUPERRES_DENOMINATOR:
      av1enc->aom_cfg.rc_superres_denominator = g_value_get_uint (value);
      global = TRUE;
      break;
    case PROP_SUPERRES_KF_DENOMINATOR:
      av1enc->aom_cfg.rc_superres_kf_denominator = g_value_get_uint (value);
      global = TRUE;
      break;
    case PROP_SUPERRES_QTHRESH:
      av1enc->aom_cfg.rc_superres_qthresh = g_value_get_uint (value);
      global = TRUE;
      break;
    case PROP_SUPERRES_KF_QTHRESH:
      av1enc->aom_cfg.rc_superres_kf_qthresh = g_value_get_uint (value);
      global = TRUE;
      break;
    case PROP_END_USAGE:
      av1enc->aom_cfg.rc_end_usage = g_value_get_enum (value);
      global = TRUE;
      break;
    case PROP_TARGET_BITRATE:
      av1enc->aom_cfg.rc_target_bitrate = g_value_get_uint (value);
      av1enc->target_bitrate_set = TRUE;
      global = TRUE;
      break;
    case PROP_MIN_QUANTIZER:
      av1enc->aom_cfg.rc_min_quantizer = g_value_get_uint (value);
      global = TRUE;
      break;
    case PROP_MAX_QUANTIZER:
      av1enc->aom_cfg.rc_max_quantizer = g_value_get_uint (value);
      global = TRUE;
      break;
    case PROP_UNDERSHOOT_PCT:
      av1enc->aom_cfg.rc_undershoot_pct = g_value_get_uint (value);
      global = TRUE;
      break;
    case PROP_OVERSHOOT_PCT:
      av1enc->aom_cfg.rc_overshoot_pct = g_value_get_uint (value);
      global = TRUE;
      break;
    case PROP_BUF_SZ:
      av1enc->aom_cfg.rc_buf_sz = g_value_get_uint (value);
      global = TRUE;
      break;
    case PROP_BUF_INITIAL_SZ:
      av1enc->aom_cfg.rc_buf_initial_sz = g_value_get_uint (value);
      global = TRUE;
      break;
    case PROP_BUF_OPTIMAL_SZ:
      av1enc->aom_cfg.rc_buf_optimal_sz = g_value_get_uint (value);
      global = TRUE;
      break;
    case PROP_THREADS:
      av1enc->threads = g_value_get_uint (value);
      global = TRUE;
      break;
#ifdef AOM_CTRL_AV1E_SET_ROW_MT
    case PROP_ROW_MT:
      av1enc->row_mt = g_value_get_boolean (value);
      GST_AV1_ENC_APPLY_CODEC_CONTROL (av1enc, AV1E_SET_ROW_MT,
          (av1enc->row_mt ? 1 : 0));
      break;
#endif
    case PROP_TILE_COLUMNS:
      av1enc->tile_columns = g_value_get_uint (value);
      GST_AV1_ENC_APPLY_CODEC_CONTROL (av1enc, AV1E_SET_TILE_COLUMNS,
          av1enc->tile_columns);
      break;
    case PROP_TILE_ROWS:
      av1enc->tile_rows = g_value_get_uint (value);
      GST_AV1_ENC_APPLY_CODEC_CONTROL (av1enc, AV1E_SET_TILE_ROWS,
          av1enc->tile_rows);
      break;
    case PROP_KF_MODE:
      av1enc->aom_cfg.kf_mode = g_value_get_enum (value);
      global = TRUE;
      break;
    case PROP_ENC_PASS:
      av1enc->aom_cfg.g_pass = g_value_get_enum (value);
      global = TRUE;
      break;
    case PROP_USAGE_PROFILE:
      av1enc->aom_cfg.g_usage = g_value_get_enum (value);
      global = TRUE;
      break;
    case PROP_LAG_IN_FRAMES:
      av1enc->aom_cfg.g_lag_in_frames = g_value_get_uint (value);
      global = TRUE;
      break;
    case PROP_KEYFRAME_MAX_DIST:
      av1enc->aom_cfg.kf_max_dist = g_value_get_int (value);
      global = TRUE;
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  if (global &&av1enc->encoder_inited) {
    status = aom_codec_enc_config_set (&av1enc->encoder, &av1enc->aom_cfg);
    GST_DEBUG_OBJECT (av1enc, "Set %s encoder configuration, ret = %s",
        pspec->name, gst_av1_get_error_name (status));
  }

  g_mutex_unlock (&av1enc->encoder_lock);
  GST_OBJECT_UNLOCK (av1enc);
}

static void
gst_av1_enc_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstAV1Enc *av1enc = GST_AV1_ENC_CAST (object);

  GST_OBJECT_LOCK (av1enc);

  switch (prop_id) {
    case PROP_CPU_USED:
      g_value_set_int (value, av1enc->cpu_used);
      break;
    case PROP_DROP_FRAME:
      g_value_set_uint (value, av1enc->aom_cfg.rc_dropframe_thresh);
      break;
    case PROP_RESIZE_MODE:
      g_value_set_enum (value, av1enc->aom_cfg.rc_resize_mode);
      break;
    case PROP_RESIZE_DENOMINATOR:
      g_value_set_uint (value, av1enc->aom_cfg.rc_resize_denominator);
      break;
    case PROP_RESIZE_KF_DENOMINATOR:
      g_value_set_uint (value, av1enc->aom_cfg.rc_resize_kf_denominator);
      break;
    case PROP_SUPERRES_MODE:
      g_value_set_enum (value, av1enc->aom_cfg.rc_superres_mode);
      break;
    case PROP_SUPERRES_DENOMINATOR:
      g_value_set_uint (value, av1enc->aom_cfg.rc_superres_denominator);
      break;
    case PROP_SUPERRES_KF_DENOMINATOR:
      g_value_set_uint (value, av1enc->aom_cfg.rc_superres_kf_denominator);
      break;
    case PROP_SUPERRES_QTHRESH:
      g_value_set_uint (value, av1enc->aom_cfg.rc_superres_qthresh);
      break;
    case PROP_SUPERRES_KF_QTHRESH:
      g_value_set_uint (value, av1enc->aom_cfg.rc_superres_kf_qthresh);
      break;
    case PROP_END_USAGE:
      g_value_set_enum (value, av1enc->aom_cfg.rc_end_usage);
      break;
    case PROP_TARGET_BITRATE:
      g_value_set_uint (value, av1enc->aom_cfg.rc_target_bitrate);
      break;
    case PROP_MIN_QUANTIZER:
      g_value_set_uint (value, av1enc->aom_cfg.rc_min_quantizer);
      break;
    case PROP_MAX_QUANTIZER:
      g_value_set_uint (value, av1enc->aom_cfg.rc_max_quantizer);
      break;
    case PROP_UNDERSHOOT_PCT:
      g_value_set_uint (value, av1enc->aom_cfg.rc_undershoot_pct);
      break;
    case PROP_OVERSHOOT_PCT:
      g_value_set_uint (value, av1enc->aom_cfg.rc_overshoot_pct);
      break;
    case PROP_BUF_SZ:
      g_value_set_uint (value, av1enc->aom_cfg.rc_buf_sz);
      break;
    case PROP_BUF_INITIAL_SZ:
      g_value_set_uint (value, av1enc->aom_cfg.rc_buf_initial_sz);
      break;
    case PROP_BUF_OPTIMAL_SZ:
      g_value_set_uint (value, av1enc->aom_cfg.rc_buf_optimal_sz);
      break;
    case PROP_THREADS:
      g_value_set_uint (value, av1enc->threads);
      break;
#ifdef AOM_CTRL_AV1E_SET_ROW_MT
    case PROP_ROW_MT:
      g_value_set_boolean (value, av1enc->row_mt);
      break;
#endif
    case PROP_TILE_COLUMNS:
      g_value_set_uint (value, av1enc->tile_columns);
      break;
    case PROP_TILE_ROWS:
      g_value_set_uint (value, av1enc->tile_rows);
      break;
    case PROP_KF_MODE:
      g_value_set_enum (value, av1enc->aom_cfg.kf_mode);
      break;
    case PROP_ENC_PASS:
      g_value_set_enum (value, av1enc->aom_cfg.g_pass);
      break;
    case PROP_USAGE_PROFILE:
      g_value_set_enum (value, av1enc->aom_cfg.g_usage);
      break;
    case PROP_LAG_IN_FRAMES:
      g_value_set_uint (value, av1enc->aom_cfg.g_lag_in_frames);
      break;
    case PROP_KEYFRAME_MAX_DIST:
      g_value_set_int (value, av1enc->aom_cfg.kf_max_dist);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  GST_OBJECT_UNLOCK (av1enc);
}

static gboolean
gst_av1_enc_start (GstVideoEncoder * encoder)
{
  return TRUE;
}

static gboolean
gst_av1_enc_stop (GstVideoEncoder * encoder)
{
  GstAV1Enc *av1enc = GST_AV1_ENC_CAST (encoder);

  if (av1enc->input_state) {
    gst_video_codec_state_unref (av1enc->input_state);
  }
  av1enc->input_state = NULL;

  gst_av1_enc_destroy_encoder (av1enc);

  return TRUE;
}
