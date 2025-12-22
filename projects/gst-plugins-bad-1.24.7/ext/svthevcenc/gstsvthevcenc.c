/* GStreamer H265 encoder plugin
 * Copyright (C) 2019 Yeongjin Jeong <yeongjin.jeong@navercorp.com>
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
 * SECTION:element-svthevcenc
 * @title: svthevcenc
 *
 * This element encodes raw video into H265 compressed data.
 *
 **/

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "gstsvthevcenc.h"

#include <gst/pbutils/pbutils.h>
#include <gst/video/video.h>
#include <gst/video/gstvideometa.h>
#include <gst/video/gstvideopool.h>
#include <gst/base/gstbitreader.h>
#include <gst/codecparsers/gsth265parser.h>

#include <string.h>
#include <stdlib.h>

GST_DEBUG_CATEGORY_STATIC (svthevc_enc_debug);
#define GST_CAT_DEFAULT svthevc_enc_debug

enum
{
  PROP_0,
  PROP_INSERT_VUI,
  PROP_AUD,
  PROP_HIERARCHICAL_LEVEL,
  PROP_LOOKAHEAD_DISTANCE,
  PROP_ENCODER_MODE,
  PROP_RC_MODE,
  PROP_QP_I,
  PROP_QP_MAX,
  PROP_QP_MIN,
  PROP_SCENE_CHANGE_DETECTION,
  PROP_TUNE,
  PROP_BASE_LAYER_SWITCH_MODE,
  PROP_BITRATE,
  PROP_KEY_INT_MAX,
  PROP_ENABLE_OPEN_GOP,
  PROP_CONFIG_INTERVAL,
  PROP_CORES,
  PROP_SOCKET,
  PROP_TILE_ROW,
  PROP_TILE_COL,
  PROP_PRED_STRUCTURE,
  PROP_VBV_MAX_RATE,
  PROP_VBV_BUFFER_SIZE,
};

#define PROP_INSERT_VUI_DEFAULT             FALSE
#define PROP_AUD_DEFAULT                    FALSE
#define PROP_HIERARCHICAL_LEVEL_DEFAULT     GST_SVTHEVC_ENC_B_PYRAMID_4LEVEL_HIERARCHY
#define PROP_LOOKAHEAD_DISTANCE_DEFAULT     40
#define PROP_ENCODER_MODE_DEFAULT           7
#define PROP_RC_MODE_DEFAULT                GST_SVTHEVC_ENC_RC_CQP
#define PROP_QP_I_DEFAULT                   25
#define PROP_QP_MAX_DEFAULT                 48
#define PROP_QP_MIN_DEFAULT                 10
#define PROP_SCENE_CHANGE_DETECTION_DEFAULT TRUE
#define PROP_TUNE_DEFAULT                   GST_SVTHEVC_ENC_TUNE_OQ
#define PROP_BASE_LAYER_SWITCH_MODE_DEFAULT GST_SVTHEVC_ENC_BASE_LAYER_MODE_BFRAME
#define PROP_BITRATE_DEFAULT                (7 * 1000)
#define PROP_KEY_INT_MAX_DEFAULT            -2
#define PROP_ENABLE_OPEN_GOP_DEFAULT        TRUE
#define PROP_CONFIG_INTERVAL_DEFAULT        0
#define PROP_CORES_DEFAULT                  0
#define PROP_SOCKET_DEFAULT                 -1
#define PROP_TILE_ROW_DEFAULT               1
#define PROP_TILE_COL_DEFAULT               1
#define PROP_PRED_STRUCTURE_DEFAULT         GST_SVTHEVC_ENC_PRED_STRUCT_RANDOM_ACCESS
#define PROP_VBV_MAX_RATE_DEFAULT           0
#define PROP_VBV_BUFFER_SIZE_DEFAULT        0

#define PROFILE_DEFAULT                     2
#define LEVEL_DEFAULT                       0
#define TIER_DEFAULT                        0

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
#define FORMATS "I420, Y42B, Y444, I420_10LE, I422_10LE, Y444_10LE"
#else
#define FORMATS "I420, Y42B, Y444, I420_10BE, I422_10BE, Y444_10BE"
#endif

#define GST_SVTHEVC_ENC_B_PYRAMID_TYPE (gst_svthevc_enc_b_pyramid_get_type())
static GType
gst_svthevc_enc_b_pyramid_get_type (void)
{
  static GType b_pyramid_type = 0;

  static const GEnumValue b_pyramid_types[] = {
    {GST_SVTHEVC_ENC_B_PYRAMID_FLAT, "Flat", "flat"},
    {GST_SVTHEVC_ENC_B_PYRAMID_2LEVEL_HIERARCHY, "2-Level Hierarchy",
        "2-level-hierarchy"},
    {GST_SVTHEVC_ENC_B_PYRAMID_3LEVEL_HIERARCHY, "3-Level Hierarchy",
        "3-level-hierarchy"},
    {GST_SVTHEVC_ENC_B_PYRAMID_4LEVEL_HIERARCHY, "4-Level Hierarchy",
        "4-level-hierarchy"},
    {0, NULL, NULL}
  };

  if (!b_pyramid_type) {
    b_pyramid_type =
        g_enum_register_static ("GstSvtHevcEncBPyramid", b_pyramid_types);
  }
  return b_pyramid_type;
}

#define GST_SVTHEVC_ENC_BASE_LAYER_MODE_TYPE (gst_svthevc_enc_base_layer_mode_get_type())
static GType
gst_svthevc_enc_base_layer_mode_get_type (void)
{
  static GType base_layer_mode_type = 0;

  static const GEnumValue base_layer_mode_types[] = {
    {GST_SVTHEVC_ENC_BASE_LAYER_MODE_BFRAME,
          "Use B-frames in the base layer pointing to the same past picture",
        "B-frame"},
    {GST_SVTHEVC_ENC_BASE_LAYER_MODE_PFRAME, "Use P-frames in the base layer",
        "P-frame"},
    {0, NULL, NULL}
  };

  if (!base_layer_mode_type) {
    base_layer_mode_type =
        g_enum_register_static ("GstSvtHevcEncBaseLayerMode",
        base_layer_mode_types);
  }
  return base_layer_mode_type;
}

#define GST_SVTHEVC_ENC_RC_TYPE (gst_svthevc_enc_rc_get_type())
static GType
gst_svthevc_enc_rc_get_type (void)
{
  static GType rc_type = 0;

  static const GEnumValue rc_types[] = {
    {GST_SVTHEVC_ENC_RC_CQP, "Constant QP Control", "cqp"},
    {GST_SVTHEVC_ENC_RC_VBR, "Variable Bitrate Contorol", "vbr"},
    {0, NULL, NULL}
  };

  if (!rc_type) {
    rc_type = g_enum_register_static ("GstSvtHevcEncRC", rc_types);
  }
  return rc_type;
}

#define GST_SVTHEVC_ENC_TUNE_TYPE (gst_svthevc_enc_tune_get_type())
static GType
gst_svthevc_enc_tune_get_type (void)
{
  static GType tune_type = 0;

  static const GEnumValue tune_types[] = {
    {GST_SVTHEVC_ENC_TUNE_SQ, "Visually Optimized Mode", "sq"},
    {GST_SVTHEVC_ENC_TUNE_OQ, "PSNR/SSIM Optimized Mode", "oq"},
    {GST_SVTHEVC_ENC_TUNE_VMAF, "VMAF Optimized Mode", "vmaf"},
    {0, NULL, NULL}
  };

  if (!tune_type) {
    tune_type = g_enum_register_static ("GstSvtHevcEncTune", tune_types);
  }
  return tune_type;
}

#define GST_SVTHEVC_ENC_PRED_STRUCT_TYPE (gst_svthevc_enc_pred_struct_get_type())
static GType
gst_svthevc_enc_pred_struct_get_type (void)
{
  static GType pred_struct_type = 0;

  static const GEnumValue pred_struct_types[] = {
    {GST_SVTHEVC_ENC_PRED_STRUCT_LOW_DELAY_P,
        "Low Delay Prediction Structure with P/p pictures", "low-delay-P"},
    {GST_SVTHEVC_ENC_PRED_STRUCT_LOW_DELAY_B,
        "Low Delay Prediction Structure with B/b pictures", "low-delay-B"},
    {GST_SVTHEVC_ENC_PRED_STRUCT_RANDOM_ACCESS,
        "Random Access Prediction Structure", "random-access"},
    {0, NULL, NULL}
  };

  if (!pred_struct_type) {
    pred_struct_type =
        g_enum_register_static ("GstSvtHevcEncPredStruct", pred_struct_types);
  }
  return pred_struct_type;
}

static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw, "
        "format = (string) { " FORMATS " }, "
        "framerate = (fraction) [0, MAX], "
        "width = (int) [ 64, 8192 ], " "height = (int) [ 64, 4320 ]")
    );

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-h265, "
        "framerate = (fraction) [0/1, MAX], "
        "width = (int) [ 64, 8192 ], " "height = (int) [ 64, 4320 ], "
        "stream-format = (string) byte-stream, "
        "alignment = (string) au, "
        "profile = (string) { main, main-10, main-422-10, main-444, main-444-10 }")
    );

static void gst_svthevc_enc_finalize (GObject * object);
static gboolean gst_svthevc_enc_start (GstVideoEncoder * encoder);
static gboolean gst_svthevc_enc_stop (GstVideoEncoder * encoder);
static gboolean gst_svthevc_enc_flush (GstVideoEncoder * encoder);

static gboolean gst_svthevc_enc_init_encoder (GstSvtHevcEnc * encoder);
static void gst_svthevc_enc_close_encoder (GstSvtHevcEnc * encoder);

static GstFlowReturn gst_svthevc_enc_finish (GstVideoEncoder * encoder);
static GstFlowReturn gst_svthevc_enc_handle_frame (GstVideoEncoder * encoder,
    GstVideoCodecFrame * frame);
static GstFlowReturn gst_svthevc_enc_drain_encoder (GstSvtHevcEnc * encoder,
    gboolean send);
static GstFlowReturn gst_svthevc_enc_send_frame (GstSvtHevcEnc * encoder,
    GstVideoCodecFrame * frame);
static GstFlowReturn gst_svthevc_enc_receive_frame (GstSvtHevcEnc * encoder,
    gboolean * got_packet, gboolean send);
static gboolean gst_svthevc_enc_set_format (GstVideoEncoder * video_enc,
    GstVideoCodecState * state);
static gboolean gst_svthevc_enc_propose_allocation (GstVideoEncoder * encoder,
    GstQuery * query);

static void gst_svthevc_enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_svthevc_enc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

#define gst_svthevc_enc_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstSvtHevcEnc, gst_svthevc_enc, GST_TYPE_VIDEO_ENCODER,
    G_IMPLEMENT_INTERFACE (GST_TYPE_PRESET, NULL));

#define MAX_FORMAT_COUNT 6
typedef struct
{
  const GstH265Profile gst_profile;
  const guint svt_profile;
  const GstVideoFormat formats[MAX_FORMAT_COUNT];
} GstSvtHevcEncProfileTable;

static const GstSvtHevcEncProfileTable profile_table[] = {
  {GST_H265_PROFILE_MAIN, 1, {GST_VIDEO_FORMAT_I420,}},
  {GST_H265_PROFILE_MAIN_444, 4, {GST_VIDEO_FORMAT_I420, GST_VIDEO_FORMAT_Y42B,
          GST_VIDEO_FORMAT_Y444,}},
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
  {GST_H265_PROFILE_MAIN_10, 2, {GST_VIDEO_FORMAT_I420,
          GST_VIDEO_FORMAT_I420_10LE,}},
  {GST_H265_PROFILE_MAIN_422_10, 4, {GST_VIDEO_FORMAT_I420,
              GST_VIDEO_FORMAT_Y42B, GST_VIDEO_FORMAT_I420_10LE,
          GST_VIDEO_FORMAT_I422_10LE,}},
  {GST_H265_PROFILE_MAIN_444_10, 4, {GST_VIDEO_FORMAT_I420,
              GST_VIDEO_FORMAT_Y42B, GST_VIDEO_FORMAT_Y444,
              GST_VIDEO_FORMAT_I420_10LE, GST_VIDEO_FORMAT_I422_10LE,
          GST_VIDEO_FORMAT_Y444_10LE}}
#else
  {GST_H265_PROFILE_MAIN_10, 2, {GST_VIDEO_FORMAT_I420,
          GST_VIDEO_FORMAT_I420_10BE,}},
  {GST_H265_PROFILE_MAIN_422_10, 4, {GST_VIDEO_FORMAT_I420,
              GST_VIDEO_FORMAT_Y42B, GST_VIDEO_FORMAT_I420_10BE,
          GST_VIDEO_FORMAT_I422_10BE,}},
  {GST_H265_PROFILE_MAIN_444_10, 4, {GST_VIDEO_FORMAT_I420,
              GST_VIDEO_FORMAT_Y42B, GST_VIDEO_FORMAT_Y444,
              GST_VIDEO_FORMAT_I420_10BE, GST_VIDEO_FORMAT_I422_10BE,
          GST_VIDEO_FORMAT_Y444_10BE}}
#endif
};

static void
set_array_val (GArray * arr, guint index, guint val)
{
  if (!arr)
    return;

  if (index >= arr->len)
    g_array_set_size (arr, index + 1);
  arr->data[index] = val;
}

static void
get_support_format_from_profile (GArray * formats, const gchar * profile_str)
{
  GstH265Profile profile = gst_h265_profile_from_string (profile_str);
  guint i, j;

  if (!formats)
    return;

  for (i = 0; i < G_N_ELEMENTS (profile_table); i++) {
    if (profile_table[i].gst_profile == profile) {
      for (j = 0; j < MAX_FORMAT_COUNT; j++) {
        if (profile_table[i].formats[j] > GST_VIDEO_FORMAT_UNKNOWN)
          set_array_val (formats, profile_table[i].formats[j], 1);
      }
      break;
    }
  }
}

static void
get_compatible_profile_from_format (GArray * profiles,
    const GstVideoFormat format)
{
  guint i, j;

  if (!profiles)
    return;

  for (i = 0; i < G_N_ELEMENTS (profile_table); i++) {
    for (j = 0; j < MAX_FORMAT_COUNT; j++) {
      if (profile_table[i].formats[j] == format) {
        set_array_val (profiles, profile_table[i].gst_profile, 1);
      }
    }
  }
}

static GstCaps *
gst_svthevc_enc_sink_getcaps (GstVideoEncoder * enc, GstCaps * filter)
{
  GstCaps *supported_incaps;
  GstCaps *allowed_caps;
  GstCaps *filter_caps, *fcaps;
  gint i, j, k;

  supported_incaps = gst_static_pad_template_get_caps (&sink_factory);

  allowed_caps = gst_pad_get_allowed_caps (GST_VIDEO_ENCODER_SRC_PAD (enc));

  if (!allowed_caps || gst_caps_is_empty (allowed_caps)
      || gst_caps_is_any (allowed_caps)) {
    fcaps = supported_incaps;
    goto done;
  }

  GST_LOG_OBJECT (enc, "template caps %" GST_PTR_FORMAT, supported_incaps);
  GST_LOG_OBJECT (enc, "allowed caps %" GST_PTR_FORMAT, allowed_caps);

  filter_caps = gst_caps_new_empty ();

  for (i = 0; i < gst_caps_get_size (supported_incaps); i++) {
    GQuark q_name =
        gst_structure_get_name_id (gst_caps_get_structure (supported_incaps,
            i));

    for (j = 0; j < gst_caps_get_size (allowed_caps); j++) {
      const GstStructure *allowed_s = gst_caps_get_structure (allowed_caps, j);
      const GValue *val;
      GstStructure *s;

      s = gst_structure_new_id_empty (q_name);
      if ((val = gst_structure_get_value (allowed_s, "width")))
        gst_structure_set_value (s, "width", val);
      if ((val = gst_structure_get_value (allowed_s, "height")))
        gst_structure_set_value (s, "height", val);

      if ((val = gst_structure_get_value (allowed_s, "profile"))) {
        GArray *formats = g_array_new (FALSE, TRUE, sizeof (guint));
        GValue fmts = G_VALUE_INIT;
        GValue fmt = G_VALUE_INIT;
        guint i;

        g_value_init (&fmts, GST_TYPE_LIST);
        g_value_init (&fmt, G_TYPE_STRING);

        if (G_VALUE_HOLDS_STRING (val)) {
          get_support_format_from_profile (formats, g_value_get_string (val));
        } else if (GST_VALUE_HOLDS_LIST (val)) {
          for (k = 0; k < gst_value_list_get_size (val); k++) {
            const GValue *vlist = gst_value_list_get_value (val, k);

            if (G_VALUE_HOLDS_STRING (vlist))
              get_support_format_from_profile (formats,
                  g_value_get_string (vlist));
          }
        }

        for (i = 0; i < formats->len; i++) {
          if (formats->data[i]) {
            g_value_set_string (&fmt,
                gst_video_format_to_string ((GstVideoFormat) i));
            gst_value_list_append_value (&fmts, &fmt);
          }
        }

        g_array_free (formats, TRUE);

        if (gst_value_list_get_size (&fmts) != 0)
          gst_structure_take_value (s, "format", &fmts);
        else
          g_value_unset (&fmts);

        g_value_unset (&fmt);
      }

      filter_caps = gst_caps_merge_structure (filter_caps, s);
    }
  }

  fcaps = gst_caps_intersect (filter_caps, supported_incaps);
  gst_caps_unref (filter_caps);
  gst_caps_unref (supported_incaps);

  if (filter) {
    GST_LOG_OBJECT (enc, "intersecting with %" GST_PTR_FORMAT, filter);
    filter_caps = gst_caps_intersect (fcaps, filter);
    gst_caps_unref (fcaps);
    fcaps = filter_caps;
  }

done:
  if (allowed_caps)
    gst_caps_unref (allowed_caps);

  GST_LOG_OBJECT (enc, "proxy caps %" GST_PTR_FORMAT, fcaps);

  return fcaps;
}

static void
gst_svthevc_enc_class_init (GstSvtHevcEncClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *element_class;
  GstVideoEncoderClass *gstencoder_class;

  gobject_class = G_OBJECT_CLASS (klass);
  element_class = GST_ELEMENT_CLASS (klass);
  gstencoder_class = GST_VIDEO_ENCODER_CLASS (klass);

  gobject_class->set_property = gst_svthevc_enc_set_property;
  gobject_class->get_property = gst_svthevc_enc_get_property;
  gobject_class->finalize = gst_svthevc_enc_finalize;

  gstencoder_class->set_format = GST_DEBUG_FUNCPTR (gst_svthevc_enc_set_format);
  gstencoder_class->handle_frame =
      GST_DEBUG_FUNCPTR (gst_svthevc_enc_handle_frame);
  gstencoder_class->start = GST_DEBUG_FUNCPTR (gst_svthevc_enc_start);
  gstencoder_class->stop = GST_DEBUG_FUNCPTR (gst_svthevc_enc_stop);
  gstencoder_class->flush = GST_DEBUG_FUNCPTR (gst_svthevc_enc_flush);
  gstencoder_class->finish = GST_DEBUG_FUNCPTR (gst_svthevc_enc_finish);
  gstencoder_class->getcaps = GST_DEBUG_FUNCPTR (gst_svthevc_enc_sink_getcaps);
  gstencoder_class->propose_allocation =
      GST_DEBUG_FUNCPTR (gst_svthevc_enc_propose_allocation);

  g_object_class_install_property (gobject_class, PROP_INSERT_VUI,
      g_param_spec_boolean ("insert-vui", "Insert VUI",
          "Insert VUI NAL in stream",
          PROP_INSERT_VUI_DEFAULT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_AUD,
      g_param_spec_boolean ("aud", "AUD",
          "Use AU (Access Unit) delimiter", PROP_AUD_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_HIERARCHICAL_LEVEL,
      g_param_spec_enum ("b-pyramid", "B Pyramid (Hierarchical Levels)",
          "Number of hierarchical layers used to construct GOP",
          GST_SVTHEVC_ENC_B_PYRAMID_TYPE, PROP_HIERARCHICAL_LEVEL_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_LOOKAHEAD_DISTANCE,
      g_param_spec_uint ("lookahead", "Lookahead Depth",
          "Look ahead distance",
          0, 250, PROP_LOOKAHEAD_DISTANCE_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_ENCODER_MODE,
      g_param_spec_uint ("speed", "speed (Encoder Mode)",
          "Encoding preset [0, 11] (e.g. 0 is the highest quality mode, 11 is the highest), [0, 11] (for >= 4k resolution), [0, 10] (for >= 1080p resolution), [0, 9] (for all resolution)",
          0, 11, PROP_ENCODER_MODE_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_RC_MODE,
      g_param_spec_enum ("rc", "Ratecontrol Mode",
          "Bitrate control mode",
          GST_SVTHEVC_ENC_RC_TYPE, PROP_RC_MODE_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_QP_I,
      g_param_spec_uint ("qp-i", "QP I",
          "QP value for intra frames in CQP mode",
          0, 51, PROP_QP_I_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_QP_MAX,
      g_param_spec_uint ("qp-max", "QP Max",
          "Maximum QP value allowed for rate control use",
          0, 51, PROP_QP_MAX_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_QP_MIN,
      g_param_spec_uint ("qp-min", "QP Min",
          "Minimum QP value allowed for rate control use",
          0, 50, PROP_QP_MIN_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_SCENE_CHANGE_DETECTION,
      g_param_spec_boolean ("enable-scd", "Scene Change Detection",
          "Use the scene change detection algorithm",
          PROP_SCENE_CHANGE_DETECTION_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_TUNE,
      g_param_spec_enum ("tune", "Tune",
          "Quality tuning mode",
          GST_SVTHEVC_ENC_TUNE_TYPE, PROP_TUNE_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_DEPRECATED));

  g_object_class_install_property (gobject_class, PROP_BASE_LAYER_SWITCH_MODE,
      g_param_spec_enum ("baselayer-mode", "Base Layer Switch Mode",
          "Random Access Prediction Structure type setting",
          GST_SVTHEVC_ENC_BASE_LAYER_MODE_TYPE,
          PROP_BASE_LAYER_SWITCH_MODE_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_BITRATE,
      g_param_spec_uint ("bitrate", "Bitrate",
          "Bitrate in kbit/sec",
          1, G_MAXINT, PROP_BITRATE_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_KEY_INT_MAX,
      g_param_spec_int ("key-int-max", "Key-frame maximal interval",
          "Distance Between Intra Frame inserted: -1=no intra update. -2=auto",
          -2, 255, PROP_KEY_INT_MAX_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_ENABLE_OPEN_GOP,
      g_param_spec_boolean ("enable-open-gop", "Enable Open GOP",
          "Allow intra-refresh using the CRA, not IDR",
          PROP_ENABLE_OPEN_GOP_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_CONFIG_INTERVAL,
      g_param_spec_uint ("config-interval", "VPS SPS PPS Send Interval",
          "Send VPS, SPS and PPS Insertion Interval per every few IDR. 0: disabled",
          0, UINT_MAX, PROP_CONFIG_INTERVAL_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_CORES,
      g_param_spec_uint ("cores", "Number of logical cores",
          "Number of logical cores to be used. 0: auto",
          0, UINT_MAX, PROP_CORES_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_SOCKET,
      g_param_spec_int ("socket", "Target socket",
          "Target socket to run on. -1: all available",
          -1, 1, PROP_SOCKET_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_TILE_ROW,
      g_param_spec_uint ("tile-row", "Tile Row Count",
          "Tile count in the Row",
          1, 16, PROP_TILE_ROW_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_TILE_COL,
      g_param_spec_uint ("tile-col", "Tile Column Count",
          "Tile count in the Column",
          1, 16, PROP_TILE_COL_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_PRED_STRUCTURE,
      g_param_spec_enum ("pred-struct", "Prediction Structure",
          "Prediction Structure used to construct GOP",
          GST_SVTHEVC_ENC_PRED_STRUCT_TYPE, PROP_PRED_STRUCTURE_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_VBV_MAX_RATE,
      g_param_spec_uint ("vbv-max-rate", "VBV Maxrate",
          "VBV maxrate in kbit/sec for VBR mode",
          0, G_MAXINT, PROP_VBV_MAX_RATE_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_VBV_BUFFER_SIZE,
      g_param_spec_uint ("vbv-buffer-size", "VBV Buffer Size",
          "VBV buffer size in kbits for VBR mode",
          0, G_MAXINT, PROP_VBV_BUFFER_SIZE_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));


  gst_element_class_set_static_metadata (element_class,
      "svthevcenc", "Codec/Encoder/Video",
      "Scalable Video Technology for HEVC Encoder (SVT-HEVC Encoder)",
      "Yeongjin Jeong <yeongjin.jeong@navercorp.com>");

  gst_element_class_add_static_pad_template (element_class, &sink_factory);
  gst_element_class_add_static_pad_template (element_class, &src_factory);
}

static void
gst_svthevc_enc_init (GstSvtHevcEnc * encoder)
{
  EB_H265_ENC_INPUT *in_data;

  encoder->in_buf = g_new0 (EB_BUFFERHEADERTYPE, 1);
  in_data = g_new0 (EB_H265_ENC_INPUT, 1);
  encoder->in_buf->pBuffer = (unsigned char *) in_data;
  encoder->in_buf->nSize = sizeof (*encoder->in_buf);
  encoder->in_buf->pAppPrivate = NULL;

  encoder->insert_vui = PROP_INSERT_VUI_DEFAULT;
  encoder->aud = PROP_AUD_DEFAULT;
  encoder->hierarchical_level = PROP_HIERARCHICAL_LEVEL_DEFAULT;
  encoder->la_depth = PROP_LOOKAHEAD_DISTANCE_DEFAULT;
  encoder->enc_mode = PROP_ENCODER_MODE_DEFAULT;
  encoder->rc_mode = PROP_RC_MODE_DEFAULT;
  encoder->qp_i = PROP_QP_I_DEFAULT;
  encoder->qp_max = PROP_QP_MAX_DEFAULT;
  encoder->qp_min = PROP_QP_MIN_DEFAULT;
  encoder->scene_change_detection = PROP_SCENE_CHANGE_DETECTION_DEFAULT;
  encoder->tune = PROP_TUNE_DEFAULT;
  encoder->base_layer_switch_mode = PROP_BASE_LAYER_SWITCH_MODE_DEFAULT;
  encoder->bitrate = PROP_BITRATE_DEFAULT;
  encoder->keyintmax = PROP_KEY_INT_MAX_DEFAULT;
  encoder->enable_open_gop = PROP_ENABLE_OPEN_GOP_DEFAULT;
  encoder->config_interval = PROP_CONFIG_INTERVAL_DEFAULT;
  encoder->cores = PROP_CORES_DEFAULT;
  encoder->socket = PROP_SOCKET_DEFAULT;
  encoder->tile_row = PROP_TILE_ROW_DEFAULT;
  encoder->tile_col = PROP_TILE_COL_DEFAULT;
  encoder->pred_structure = PROP_PRED_STRUCTURE_DEFAULT;
  encoder->vbv_maxrate = PROP_VBV_MAX_RATE_DEFAULT;
  encoder->vbv_bufsize = PROP_VBV_BUFFER_SIZE_DEFAULT;

  encoder->profile = PROFILE_DEFAULT;
  encoder->tier = TIER_DEFAULT;
  encoder->level = LEVEL_DEFAULT;

  encoder->svthevc_version =
      g_strdup_printf ("%d.%d.%d", SVT_VERSION_MAJOR, SVT_VERSION_MINOR,
      SVT_VERSION_PATCHLEVEL);
  encoder->push_header = TRUE;
  encoder->first_buffer = TRUE;
  encoder->update_latency = TRUE;

  encoder->internal_pool = NULL;
  encoder->aligned_info = NULL;

  GST_PAD_SET_ACCEPT_TEMPLATE (GST_VIDEO_ENCODER_SINK_PAD (encoder));
}

static gboolean
gst_svthevc_enc_start (GstVideoEncoder * encoder)
{
  GstSvtHevcEnc *svthevcenc = GST_SVTHEVC_ENC (encoder);

  GST_INFO_OBJECT (svthevcenc, "start encoder");

  /* make sure that we have enough time for first DTS,
     this is probably overkill for most streams */
  gst_video_encoder_set_min_pts (encoder, GST_SECOND * 60 * 60 * 1000);

  return TRUE;
}

static gboolean
gst_svthevc_enc_stop (GstVideoEncoder * encoder)
{
  GstSvtHevcEnc *svthevcenc = GST_SVTHEVC_ENC (encoder);

  GST_INFO_OBJECT (encoder, "stop encoder");

  /* Always drain SVT-HEVC encoder before releasing SVT-HEVC.
   * Otherwise, randomly block happens when releasing SVT-HEVC. */
  gst_svthevc_enc_drain_encoder (svthevcenc, FALSE);
  gst_svthevc_enc_close_encoder (svthevcenc);

  if (svthevcenc->input_state)
    gst_video_codec_state_unref (svthevcenc->input_state);
  svthevcenc->input_state = NULL;

  if (svthevcenc->internal_pool)
    gst_object_unref (svthevcenc->internal_pool);
  svthevcenc->internal_pool = NULL;

  if (svthevcenc->aligned_info)
    gst_video_info_free (svthevcenc->aligned_info);
  svthevcenc->aligned_info = NULL;

  return TRUE;
}


static gboolean
gst_svthevc_enc_flush (GstVideoEncoder * encoder)
{
  GstSvtHevcEnc *svthevcenc = GST_SVTHEVC_ENC (encoder);

  GST_INFO_OBJECT (encoder, "flushing encoder");

  /* Always drain SVT-HEVC encoder before releasing SVT-HEVC.
   * Otherwise, randomly block happens when releasing SVT-HEVC. */
  gst_svthevc_enc_drain_encoder (svthevcenc, FALSE);
  gst_svthevc_enc_close_encoder (svthevcenc);

  GST_OBJECT_LOCK (encoder);
  if (!gst_svthevc_enc_init_encoder (svthevcenc)) {
    GST_OBJECT_UNLOCK (encoder);
    return FALSE;
  }
  GST_OBJECT_UNLOCK (encoder);

  return TRUE;
}

static void
gst_svthevc_enc_finalize (GObject * object)
{
  GstSvtHevcEnc *encoder = GST_SVTHEVC_ENC (object);

  if (encoder->in_buf) {
    EB_H265_ENC_INPUT *in_data = (EB_H265_ENC_INPUT *) encoder->in_buf->pBuffer;
    if (in_data)
      g_free (in_data);
    g_free (encoder->in_buf);
  }

  g_free ((gpointer) encoder->svthevc_version);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gint
gst_svthevc_enc_gst_to_svthevc_video_format (GstVideoFormat format,
    gint * nplanes)
{
  switch (format) {
    case GST_VIDEO_FORMAT_I420:
    case GST_VIDEO_FORMAT_YV12:
    case GST_VIDEO_FORMAT_I420_10LE:
    case GST_VIDEO_FORMAT_I420_10BE:
      if (nplanes)
        *nplanes = 3;
      return EB_YUV420;
    case GST_VIDEO_FORMAT_Y42B:
    case GST_VIDEO_FORMAT_I422_10LE:
    case GST_VIDEO_FORMAT_I422_10BE:
      if (nplanes)
        *nplanes = 3;
      return EB_YUV422;
    case GST_VIDEO_FORMAT_Y444:
    case GST_VIDEO_FORMAT_Y444_10LE:
    case GST_VIDEO_FORMAT_Y444_10BE:
      if (nplanes)
        *nplanes = 3;
      return EB_YUV444;
    default:
      g_return_val_if_reached (GST_VIDEO_FORMAT_UNKNOWN);
  }
}

static void
config_enc_params (GstSvtHevcEnc * encoder, EB_H265_ENC_CONFIGURATION * param)
{
  GstVideoInfo *info;

  info = &encoder->input_state->info;

  param->sourceWidth = info->width;
  param->sourceHeight = info->height;

  if (GST_VIDEO_INFO_COMP_DEPTH (info, 0) == 10) {
    GST_DEBUG_OBJECT (encoder, "Encoder 10 bits depth input");
    /* Disable Compressed 10-bit format default.
     * SVT-HEVC support a compressed 10-bit format allowing the
     * software to achieve a higher speed and channel density levels.
     * The conversion between the 10-bit I420 and the compressed
     * 10-bit format is a lossless operation.
     */
    param->compressedTenBitFormat = 0;
    param->encoderBitDepth = 10;
  }
  /* Update param from options */
  param->hierarchicalLevels = encoder->hierarchical_level;
  param->encMode = encoder->enc_mode;
  param->profile = encoder->profile;
  param->tier = encoder->tier;
  param->level = encoder->level;
  param->rateControlMode = encoder->rc_mode;
  param->sceneChangeDetection = encoder->scene_change_detection;
  param->tune = encoder->tune;
  param->latencyMode = 0;
  param->baseLayerSwitchMode = encoder->base_layer_switch_mode;
  param->qp = encoder->qp_i;
  param->accessUnitDelimiter = encoder->aud;

  param->targetBitRate = encoder->bitrate * 1000;
  param->intraPeriodLength =
      encoder->keyintmax > 0 ? encoder->keyintmax - 1 : encoder->keyintmax;

  if (info->fps_d == 0 || info->fps_n == 0) {
    param->frameRateNumerator = 0;
    param->frameRateDenominator = 1;
  } else {
    param->frameRateNumerator = info->fps_n;
    param->frameRateDenominator = info->fps_d;
  }

  if (param->rateControlMode) {
    param->maxQpAllowed = encoder->qp_max;
    param->minQpAllowed = encoder->qp_min;
  }

  if (encoder->enable_open_gop)
    param->intraRefreshType = -1;
  else
    param->intraRefreshType = encoder->config_interval;

  param->logicalProcessors = encoder->cores;
  param->targetSocket = encoder->socket;

  param->tileRowCount = encoder->tile_row;
  param->tileColumnCount = encoder->tile_col;

  param->predStructure = encoder->pred_structure;

  if (encoder->vbv_maxrate)
    param->vbvMaxrate = encoder->vbv_maxrate * 1000;

  if (encoder->vbv_bufsize)
    param->vbvBufsize = encoder->vbv_bufsize * 1000;

  /*
   * NOTE: codeVpsSpsPps flag allows the VPS, SPS and PPS Insertion and
   * sending in first IDR frame. But in the SVT-HEVC specific version,
   * If codeVpsSpsPps enabled and using the EbH265EncStreamHeader API
   * before receiving encoded packets, It cause bug which encoded packets
   * are not output.
   */
  if (SVT_CHECK_VERSION (1, 4, 1))
    param->codeVpsSpsPps = 1;
  else
    param->codeVpsSpsPps = 0;

  param->codeEosNal = 1;

  if (encoder->insert_vui)
    param->videoUsabilityInfo = encoder->insert_vui;

  if (encoder->la_depth != -1)
    param->lookAheadDistance = encoder->la_depth;

  param->encoderColorFormat =
      gst_svthevc_enc_gst_to_svthevc_video_format (info->finfo->format, NULL);
}

static void
read_in_data (EB_H265_ENC_CONFIGURATION * config,
    GstVideoFrame * vframe, EB_BUFFERHEADERTYPE * headerPtr)
{
  EB_H265_ENC_INPUT *in_data = (EB_H265_ENC_INPUT *) headerPtr->pBuffer;

  in_data->luma = GST_VIDEO_FRAME_PLANE_DATA (vframe, 0);
  in_data->cb = GST_VIDEO_FRAME_PLANE_DATA (vframe, 1);
  in_data->cr = GST_VIDEO_FRAME_PLANE_DATA (vframe, 2);

  in_data->yStride =
      GST_VIDEO_FRAME_COMP_STRIDE (vframe,
      0) / GST_VIDEO_FRAME_COMP_PSTRIDE (vframe, 0);
  in_data->cbStride =
      GST_VIDEO_FRAME_COMP_STRIDE (vframe,
      1) / GST_VIDEO_FRAME_COMP_PSTRIDE (vframe, 1);
  in_data->crStride =
      GST_VIDEO_FRAME_COMP_STRIDE (vframe,
      2) / GST_VIDEO_FRAME_COMP_PSTRIDE (vframe, 2);

  headerPtr->nAllocLen = headerPtr->nFilledLen = GST_VIDEO_FRAME_SIZE (vframe);
}

/*
 * gst_svthevc_enc_init_encoder
 * @encoder:  Encoder which should be initialized.
 *
 * Initialize svthevc encoder.
 *
 */
static gboolean
gst_svthevc_enc_init_encoder (GstSvtHevcEnc * encoder)
{
  EB_ERRORTYPE svt_ret;

  if (!encoder->input_state) {
    GST_DEBUG_OBJECT (encoder, "Have no input state yet");
    return FALSE;
  }

  /* make sure that the encoder is closed */
  gst_svthevc_enc_close_encoder (encoder);

  encoder->svt_eos_flag = EOS_NOT_REACHED;

  /* set up encoder parameters */
  svt_ret = EbInitHandle (&encoder->svt_handle, encoder, &encoder->enc_params);
  if (svt_ret != EB_ErrorNone) {
    GST_DEBUG_OBJECT (encoder, "Error init encoder handle");
    goto failed;
  }

  config_enc_params (encoder, &encoder->enc_params);

  svt_ret = EbH265EncSetParameter (encoder->svt_handle, &encoder->enc_params);
  if (svt_ret != EB_ErrorNone) {
    GST_DEBUG_OBJECT (encoder, "Error setting encoder parameters");
    goto failed_init_handle;
  }

  svt_ret = EbInitEncoder (encoder->svt_handle);
  if (svt_ret != EB_ErrorNone) {
    GST_DEBUG_OBJECT (encoder, "Error init encoder");
    goto failed_init_handle;
  }

  encoder->push_header = TRUE;
  encoder->first_buffer = TRUE;
  encoder->update_latency = TRUE;
  encoder->reconfig = FALSE;

  /* good start, will be corrected if needed */
  encoder->dts_offset = 0;
  encoder->first_frame = NULL;

  return TRUE;

failed_init_handle:
  EbDeinitHandle (encoder->svt_handle);
failed:
  encoder->svt_handle = NULL;

  return FALSE;
}

/* gst_svthevc_enc_close_encoder
 * @encoder:  Encoder which should close.
 *
 * Close svthevc encoder.
 */
static void
gst_svthevc_enc_close_encoder (GstSvtHevcEnc * encoder)
{
  if (encoder->svt_handle != NULL) {
    EbDeinitEncoder (encoder->svt_handle);
    EbDeinitHandle (encoder->svt_handle);
    encoder->svt_handle = NULL;
  }
}

static EB_BUFFERHEADERTYPE *
gst_svthevc_enc_bytestream_to_nal (GstSvtHevcEnc * encoder,
    EB_BUFFERHEADERTYPE * input)
{
  EB_BUFFERHEADERTYPE *output;
  int i, j, zeros;
  int offset = 4;

  output = g_malloc (sizeof (EB_BUFFERHEADERTYPE));

  /* skip access unit delimiter */
  if (encoder->aud)
    offset += 7;

  output->pBuffer = g_malloc (input->nFilledLen - offset);
  output->nFilledLen = input->nFilledLen - offset;

  zeros = 0;
  for (i = offset, j = 0; i < input->nFilledLen; (i++, j++)) {
    if (input->pBuffer[i] == 0x00) {
      zeros++;
    } else if (input->pBuffer[i] == 0x03 && zeros == 2) {
      zeros = 0;
      j--;
      output->nFilledLen--;
      continue;
    } else {
      zeros = 0;
    }
    output->pBuffer[j] = input->pBuffer[i];
  }

  return output;
}

static void
svthevc_nal_free (EB_BUFFERHEADERTYPE * nal)
{
  g_free (nal->pBuffer);
  g_free (nal);
}

static gboolean
gst_svthevc_enc_set_level_tier_and_profile (GstSvtHevcEnc * encoder,
    GstCaps * caps)
{
  EB_BUFFERHEADERTYPE *headerPtr = NULL, *nal = NULL;
  EB_ERRORTYPE svt_ret;
  const gchar *level, *tier, *profile;
  GstStructure *s;
  GstCaps *allowed_caps;
  GstStructure *s2;
  const gchar *allowed_profile;

  GST_DEBUG_OBJECT (encoder, "set profile, level and tier");

  svt_ret = EbH265EncStreamHeader (encoder->svt_handle, &headerPtr);
  if (svt_ret != EB_ErrorNone) {
    GST_ELEMENT_ERROR (encoder, STREAM, ENCODE,
        ("Encode svthevc header failed."),
        ("svthevc_encoder_headers return code=%d", svt_ret));
    return FALSE;
  }

  GST_MEMDUMP ("ENCODER_HEADER", headerPtr->pBuffer, headerPtr->nFilledLen);

  nal = gst_svthevc_enc_bytestream_to_nal (encoder, headerPtr);

  gst_codec_utils_h265_caps_set_level_tier_and_profile (caps,
      nal->pBuffer + 6, nal->nFilledLen - 6);

  svthevc_nal_free (nal);

  s = gst_caps_get_structure (caps, 0);
  profile = gst_structure_get_string (s, "profile");
  tier = gst_structure_get_string (s, "tier");
  level = gst_structure_get_string (s, "level");

  GST_DEBUG_OBJECT (encoder, "profile : %s", (profile) ? profile : "---");
  GST_DEBUG_OBJECT (encoder, "tier    : %s", (tier) ? tier : "---");
  GST_DEBUG_OBJECT (encoder, "level   : %s", (level) ? level : "---");

  /* Relaxing the profile condition since libSvtHevcEnc can generate
   * wrong bitstream indication for conformance to profile than requested one.
   * See : https://github.com/OpenVisualCloud/SVT-HEVC/pull/320
   */
  allowed_caps = gst_pad_get_allowed_caps (GST_VIDEO_ENCODER_SRC_PAD (encoder));

  if (allowed_caps == NULL)
    goto no_peer;

  if (!gst_caps_can_intersect (allowed_caps, caps)) {
    GArray *peer_formats = g_array_new (FALSE, TRUE, sizeof (guint));
    GArray *enc_formats = g_array_new (FALSE, TRUE, sizeof (guint));
    gboolean is_subset = TRUE;
    guint i, j;

    allowed_caps = gst_caps_make_writable (allowed_caps);
    allowed_caps = gst_caps_truncate (allowed_caps);
    s2 = gst_caps_get_structure (allowed_caps, 0);
    gst_structure_fixate_field_string (s2, "profile", profile);
    allowed_profile = gst_structure_get_string (s2, "profile");

    get_support_format_from_profile (peer_formats, allowed_profile);
    get_support_format_from_profile (enc_formats, profile);

    for (i = 0; i < enc_formats->len; i++) {
      if (enc_formats->data[i]) {
        gboolean is_support = FALSE;
        for (j = 0; j < peer_formats->len; j++) {
          if (peer_formats->data[j] && (i == j))
            is_support = TRUE;
        }
        if (!is_support) {
          is_subset = FALSE;
          break;
        }
      }
    }

    GST_INFO_OBJECT (encoder, "downstream requested %s profile but "
        "encoder will now output %s profile (which is a %s), so relaxing the "
        "profile condition for negotiation",
        allowed_profile, profile, is_subset ? "subset" : "not subset");

    gst_structure_set (s, "profile", G_TYPE_STRING, allowed_profile, NULL);

    g_array_free (peer_formats, TRUE);
    g_array_free (enc_formats, TRUE);
  }
  gst_caps_unref (allowed_caps);

no_peer:
  return TRUE;
}

static GstBuffer *
gst_svthevc_enc_get_header_buffer (GstSvtHevcEnc * encoder)
{
  EB_BUFFERHEADERTYPE *headerPtr = NULL;
  EB_ERRORTYPE svt_ret;
  GstBuffer *buf;

  svt_ret = EbH265EncStreamHeader (encoder->svt_handle, &headerPtr);
  if (svt_ret != EB_ErrorNone) {
    GST_ELEMENT_ERROR (encoder, STREAM, ENCODE,
        ("Encode svthevc header failed."),
        ("svthevc_encoder_headers return code=%d", svt_ret));
    return FALSE;
  }

  buf = gst_buffer_new_allocate (NULL, headerPtr->nFilledLen, NULL);
  gst_buffer_fill (buf, 0, headerPtr->pBuffer, headerPtr->nFilledLen);

  return buf;
}

/* gst_svthevc_enc_set_src_caps
 * Returns: TRUE on success.
 */
static gboolean
gst_svthevc_enc_set_src_caps (GstSvtHevcEnc * encoder, GstCaps * caps)
{
  GstCaps *outcaps;
  GstStructure *structure;
  GstVideoCodecState *state;
  GstTagList *tags;

  outcaps = gst_caps_new_empty_simple ("video/x-h265");
  structure = gst_caps_get_structure (outcaps, 0);

  gst_structure_set (structure, "stream-format", G_TYPE_STRING, "byte-stream",
      NULL);
  gst_structure_set (structure, "alignment", G_TYPE_STRING, "au", NULL);

  if (!gst_svthevc_enc_set_level_tier_and_profile (encoder, outcaps)) {
    gst_caps_unref (outcaps);
    return FALSE;
  }

  state = gst_video_encoder_set_output_state (GST_VIDEO_ENCODER (encoder),
      outcaps, encoder->input_state);
  GST_LOG_OBJECT (encoder, "output caps: %" GST_PTR_FORMAT, state->caps);
  gst_video_codec_state_unref (state);

  tags = gst_tag_list_new_empty ();
  gst_tag_list_add (tags, GST_TAG_MERGE_REPLACE, GST_TAG_ENCODER, "svthevc",
      GST_TAG_ENCODER_VERSION, encoder->svthevc_version, NULL);
  gst_video_encoder_merge_tags (GST_VIDEO_ENCODER (encoder), tags,
      GST_TAG_MERGE_REPLACE);
  gst_tag_list_unref (tags);

  return TRUE;
}

static void
gst_svthevc_enc_set_latency (GstSvtHevcEnc * encoder)
{
  GstVideoInfo *info = &encoder->input_state->info;
  guint max_delayed_frames;
  GstClockTime latency;

  if (encoder->first_buffer) {
    /* FIXME get a real value from the encoder, this is currently not exposed */
    max_delayed_frames = 5;
  } else {
    GList *frames = gst_video_encoder_get_frames (GST_VIDEO_ENCODER (encoder));
    max_delayed_frames = g_list_length (frames);
    g_list_free_full (frames, (GDestroyNotify) gst_video_codec_frame_unref);
  }

  if (info->fps_n) {
    latency = gst_util_uint64_scale_ceil (GST_SECOND * info->fps_d,
        max_delayed_frames, info->fps_n);
  } else {
    /* FIXME: Assume 25fps. This is better than reporting no latency at
     * all and then later failing in live pipelines
     */
    latency = gst_util_uint64_scale_ceil (GST_SECOND * 1,
        max_delayed_frames, 25);
  }

  GST_INFO_OBJECT (encoder,
      "Updating latency to %" GST_TIME_FORMAT " (%d frames)",
      GST_TIME_ARGS (latency), max_delayed_frames);

  gst_video_encoder_set_latency (GST_VIDEO_ENCODER (encoder), latency, latency);
}

static const guint
gst_svthevc_enc_profile_from_gst (const GstH265Profile profile)
{
  gint i;

  for (i = 0; i < G_N_ELEMENTS (profile_table); i++) {
    if (profile == profile_table[i].gst_profile)
      return profile_table[i].svt_profile;
  }

  GST_WARNING ("Unsupported profile string '%s'",
      gst_h265_profile_to_string (profile));
  return 0;
}

static guint
gst_svthevc_enc_level_from_gst (const gchar * level)
{
  if (g_str_equal (level, "1"))
    return 10;
  else if (g_str_equal (level, "2"))
    return 20;
  else if (g_str_equal (level, "2.1"))
    return 21;
  else if (g_str_equal (level, "3"))
    return 30;
  else if (g_str_equal (level, "3.1"))
    return 31;
  else if (g_str_equal (level, "4"))
    return 40;
  else if (g_str_equal (level, "4.1"))
    return 41;
  else if (g_str_equal (level, "5"))
    return 50;
  else if (g_str_equal (level, "5.1"))
    return 51;
  else if (g_str_equal (level, "5.2"))
    return 52;
  else if (g_str_equal (level, "6"))
    return 60;
  else if (g_str_equal (level, "6.1"))
    return 61;
  else if (g_str_equal (level, "6.2"))
    return 62;

  GST_WARNING ("Unsupported level string '%s'", level);
  return LEVEL_DEFAULT;
}

static guint
gst_svthevc_enc_tier_from_gst (const gchar * level)
{
  if (g_str_equal (level, "main"))
    return 0;
  else if (g_str_equal (level, "high"))
    return 1;

  GST_WARNING ("Unsupported tier string '%s'", level);
  return TIER_DEFAULT;
}

static gboolean
gst_svthevc_enc_set_format (GstVideoEncoder * video_enc,
    GstVideoCodecState * state)
{
  GstSvtHevcEnc *encoder = GST_SVTHEVC_ENC (video_enc);
  GstVideoInfo *info = &state->info;
  GstCaps *template_caps;
  GstCaps *allowed_caps;

  /* If the encoder is initialized, do not reinitialize it again if not
   * necessary */
  if (encoder->svt_handle) {
    GstVideoInfo *old = &encoder->input_state->info;

    if (info->finfo->format == old->finfo->format
        && info->width == old->width && info->height == old->height
        && info->fps_n == old->fps_n && info->fps_d == old->fps_d
        && info->par_n == old->par_n && info->par_d == old->par_d) {
      gst_video_codec_state_unref (encoder->input_state);
      encoder->input_state = gst_video_codec_state_ref (state);
      return TRUE;
    }

    /* clear out pending frames */
    gst_svthevc_enc_drain_encoder (encoder, TRUE);
  }

  if (encoder->input_state)
    gst_video_codec_state_unref (encoder->input_state);
  encoder->input_state = gst_video_codec_state_ref (state);

  template_caps = gst_static_pad_template_get_caps (&src_factory);
  allowed_caps = gst_pad_get_allowed_caps (GST_VIDEO_ENCODER_SRC_PAD (encoder));

  if (allowed_caps == template_caps) {
    GST_INFO_OBJECT (encoder, "downstream has ANY caps");

    /* SVT-HEVC encoder does not yet support auto profile selecting.
     * So we should be set the profile from input format */
    encoder->profile = GST_VIDEO_INFO_COMP_DEPTH (info, 0) == 8 ? 1 : 2;
    switch (GST_VIDEO_INFO_FORMAT (info)) {
      case GST_VIDEO_FORMAT_Y42B:
      case GST_VIDEO_FORMAT_I422_10LE:
      case GST_VIDEO_FORMAT_I422_10BE:
      case GST_VIDEO_FORMAT_Y444:
      case GST_VIDEO_FORMAT_Y444_10LE:
      case GST_VIDEO_FORMAT_Y444_10BE:
        encoder->profile = 4;
      default:
        break;
    }

    gst_caps_unref (allowed_caps);
  } else if (allowed_caps) {
    GstStructure *s;
    const gchar *profile;
    const gchar *level;
    const gchar *tier;

    GST_LOG_OBJECT (encoder, "allowed caps %" GST_PTR_FORMAT, allowed_caps);

    if (gst_caps_is_empty (allowed_caps)) {
      gst_caps_unref (template_caps);
      gst_caps_unref (allowed_caps);
      return FALSE;
    }

    s = gst_caps_get_structure (allowed_caps, 0);

    if (gst_structure_has_field (s, "profile")) {
      const GValue *v = gst_structure_get_value (s, "profile");
      GArray *profiles = g_array_new (FALSE, TRUE, sizeof (guint));
      GstH265Profile gst_profile;
      guint svt_profile = 0;

      get_compatible_profile_from_format (profiles,
          GST_VIDEO_INFO_FORMAT (info));

      if (GST_VALUE_HOLDS_LIST (v)) {
        const gint list_size = gst_value_list_get_size (v);
        gint i, j;

        for (i = 0; i < list_size; i++) {
          const GValue *list_val = gst_value_list_get_value (v, i);
          profile = g_value_get_string (list_val);

          if (profile) {
            gst_profile =
                gst_h265_profile_from_string (g_value_get_string (list_val));

            for (j = 0; j < profiles->len; j++) {
              if (profiles->data[j] && (j == gst_profile)) {
                svt_profile = gst_svthevc_enc_profile_from_gst (j);
                break;
              }
            }
          }

          if (svt_profile != 0)
            break;
        }
      } else if (G_VALUE_HOLDS_STRING (v)) {
        gint i;
        profile = g_value_get_string (v);

        if (profile) {
          gst_profile = gst_h265_profile_from_string (g_value_get_string (v));

          for (i = 0; i < profiles->len; i++) {
            if (profiles->data[i] && (i == gst_profile)) {
              svt_profile = gst_svthevc_enc_profile_from_gst (i);
              break;
            }
          }
        }
      }

      g_array_free (profiles, TRUE);

      if (svt_profile == 0) {
        GST_ERROR_OBJECT (encoder, "Could't apply peer profile");
        gst_caps_unref (template_caps);
        gst_caps_unref (allowed_caps);
        return FALSE;
      }

      encoder->profile = svt_profile;
    }

    level = gst_structure_get_string (s, "level");
    if (level)
      encoder->level = gst_svthevc_enc_level_from_gst (level);

    tier = gst_structure_get_string (s, "tier");
    if (tier)
      encoder->tier = gst_svthevc_enc_tier_from_gst (tier);

    gst_caps_unref (allowed_caps);
  }
  gst_caps_unref (template_caps);

  GST_INFO_OBJECT (encoder, "Using profile %d, tier %d, level %d",
      encoder->profile, encoder->tier, encoder->level);

  GST_OBJECT_LOCK (encoder);
  if (!gst_svthevc_enc_init_encoder (encoder)) {
    GST_OBJECT_UNLOCK (encoder);
    return FALSE;
  }
  GST_OBJECT_UNLOCK (encoder);

  if (!gst_svthevc_enc_set_src_caps (encoder, state->caps)) {
    gst_svthevc_enc_close_encoder (encoder);
    return FALSE;
  }

  {
    /* The SVT-HEVC uses stride in pixel, not in bytes, while upstream can
     * provide aligned stride in bytes. So there is no guaranty
     * that a stride is multiple of PSTRIDE, we should ensure internal pool
     * to use when converting frames. */
    GstVideoAlignment video_align;
    GstAllocationParams params = { 0, 15, 0, 0 };
    GstCaps *caps;
    GstBufferPool *pool;
    GstStructure *config;
    guint i, size;

    if (encoder->internal_pool)
      gst_object_unref (encoder->internal_pool);
    encoder->internal_pool = NULL;

    if (encoder->aligned_info)
      gst_video_info_free (encoder->aligned_info);
    encoder->aligned_info = gst_video_info_copy (info);

    caps = gst_video_info_to_caps (info);
    pool = gst_video_buffer_pool_new ();

    size = GST_VIDEO_INFO_SIZE (info);
    GST_INFO_OBJECT (encoder,
        "create internal buffer pool size %u, caps %" GST_PTR_FORMAT, size,
        caps);

    config = gst_buffer_pool_get_config (pool);
    gst_buffer_pool_config_set_params (config, caps, size, 0, 0);
    gst_buffer_pool_config_set_allocator (config, NULL, &params);

    gst_caps_unref (caps);

    /* set stride align */
    gst_video_alignment_reset (&video_align);
    for (i = 0; i < GST_VIDEO_INFO_N_PLANES (info); i++)
      video_align.stride_align[i] = GST_VIDEO_INFO_COMP_PSTRIDE (info, i) - 1;
    gst_video_info_align (encoder->aligned_info, &video_align);

    gst_buffer_pool_config_add_option (config,
        GST_BUFFER_POOL_OPTION_VIDEO_ALIGNMENT);
    gst_buffer_pool_config_set_video_alignment (config, &video_align);

    if (!gst_buffer_pool_set_config (pool, config)) {
      if (pool)
        gst_object_unref (pool);
      pool = NULL;
    }
    gst_buffer_pool_set_active (pool, TRUE);

    encoder->internal_pool = pool;
  }

  gst_svthevc_enc_set_latency (encoder);

  return TRUE;
}

static GstFlowReturn
gst_svthevc_enc_finish (GstVideoEncoder * encoder)
{
  GST_INFO_OBJECT (encoder, "finish encoder");

  gst_svthevc_enc_drain_encoder (GST_SVTHEVC_ENC (encoder), TRUE);
  return GST_FLOW_OK;
}

static gboolean
gst_svthevc_enc_propose_allocation (GstVideoEncoder * encoder, GstQuery * query)
{
  GstSvtHevcEnc *svthevcenc = GST_SVTHEVC_ENC (encoder);
  GstCaps *caps;
  GstVideoInfo info;
  GstVideoAlignment video_align;
  GstBufferPool *pool = NULL;
  GstStructure *config;
  guint i, size, min, max;

  GST_INFO_OBJECT (svthevcenc, "propose allocation");

  gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, NULL);

  gst_query_parse_allocation (query, &caps, NULL);

  if (caps == NULL)
    goto done;

  if (!gst_video_info_from_caps (&info, caps))
    goto done;

  /* We should propose to specify required stride alignments. */
  gst_video_alignment_reset (&video_align);
  for (i = 0; i < GST_VIDEO_INFO_N_PLANES (&info); i++)
    video_align.stride_align[i] = GST_VIDEO_INFO_COMP_PSTRIDE (&info, i) - 1;
  gst_video_info_align (&info, &video_align);

  if (gst_query_get_n_allocation_pools (query) > 0) {
    gst_query_parse_nth_allocation_pool (query, 0, &pool, &size, &min, &max);
    config = gst_buffer_pool_get_config (pool);

    /* set stride align */
    gst_buffer_pool_config_add_option (config,
        GST_BUFFER_POOL_OPTION_VIDEO_ALIGNMENT);
    gst_buffer_pool_config_set_video_alignment (config, &video_align);

    gst_buffer_pool_set_config (pool, config);
    gst_query_set_nth_allocation_pool (query, 0, pool, size, min, max);
  } else {
    GstAllocator *allocator = NULL;
    GstAllocationParams params = { 0, 15, 0, 0 };

    size = GST_VIDEO_INFO_SIZE (&info);
    GST_INFO_OBJECT (svthevcenc,
        "create buffer pool size %u, caps %" GST_PTR_FORMAT, size, caps);

    if (gst_query_get_n_allocation_params (query) > 0)
      gst_query_parse_nth_allocation_param (query, 0, &allocator, &params);
    else
      gst_query_add_allocation_param (query, allocator, &params);

    pool = gst_video_buffer_pool_new ();

    config = gst_buffer_pool_get_config (pool);
    gst_buffer_pool_config_set_params (config, caps, size, 0, 0);
    gst_buffer_pool_config_set_allocator (config, allocator, &params);

    /* set stride align */
    gst_buffer_pool_config_add_option (config,
        GST_BUFFER_POOL_OPTION_VIDEO_ALIGNMENT);
    gst_buffer_pool_config_set_video_alignment (config, &video_align);

    if (allocator)
      gst_object_unref (allocator);

    if (!gst_buffer_pool_set_config (pool, config))
      goto done;

    gst_query_add_allocation_pool (query, pool, size, 0, 0);
  }

done:
  if (pool)
    gst_object_unref (pool);

  return GST_VIDEO_ENCODER_CLASS (parent_class)->propose_allocation (encoder,
      query);
}

/* chain function
 * this function does the actual processing
 */
static GstFlowReturn
gst_svthevc_enc_handle_frame (GstVideoEncoder * video_enc,
    GstVideoCodecFrame * frame)
{
  GstSvtHevcEnc *encoder = GST_SVTHEVC_ENC (video_enc);
  GstFlowReturn ret = GST_FLOW_OK;
  gboolean got_packet;

  if (G_UNLIKELY (encoder->svt_handle == NULL))
    goto not_inited;

  ret = gst_svthevc_enc_send_frame (encoder, frame);

  if (ret != GST_FLOW_OK)
    goto encode_fail;

  do {
    ret = gst_svthevc_enc_receive_frame (encoder, &got_packet, TRUE);
    GST_LOG_OBJECT (encoder, "ret %d, got_packet %d", ret, got_packet);
    if (ret != GST_FLOW_OK)
      break;
  } while (got_packet);

done:
  return ret;

/* ERRORS */
not_inited:
  {
    GST_WARNING_OBJECT (encoder, "Got buffer before set_caps was called");
    return GST_FLOW_NOT_NEGOTIATED;
  }
encode_fail:
  {
    /* avoid frame (and ts etc) piling up */
    if (frame)
      ret = gst_video_encoder_finish_frame (GST_VIDEO_ENCODER (encoder), frame);
    goto done;
  }
}

static gboolean
gst_svthevc_enc_convert_frame (GstSvtHevcEnc * encoder,
    GstVideoCodecFrame * frame)
{
  GstVideoInfo *info = &encoder->input_state->info;
  GstVideoFrame src_frame, aligned_frame;
  GstBuffer *aligned_buffer;

  if (encoder->internal_pool == NULL)
    return FALSE;

  if (gst_buffer_pool_acquire_buffer (encoder->internal_pool, &aligned_buffer,
          NULL) != GST_FLOW_OK) {
    GST_ERROR_OBJECT (encoder, "Failed to acquire a buffer from pool");
    return FALSE;
  }

  if (!gst_video_frame_map (&src_frame, info, frame->input_buffer,
          GST_MAP_READ)) {
    GST_ERROR_OBJECT (encoder, "Failed to map the frame for aligned buffer");
    goto error;
  }

  /* FIXME: need to adjust video info align?? */
  if (!gst_video_frame_map (&aligned_frame, encoder->aligned_info,
          aligned_buffer, GST_MAP_WRITE)) {
    GST_ERROR_OBJECT (encoder, "Failed to map the frame for aligned buffer");
    gst_video_frame_unmap (&src_frame);
    goto error;
  }

  if (!gst_video_frame_copy (&aligned_frame, &src_frame)) {
    GST_ERROR_OBJECT (encoder, "Failed to copy frame");
    gst_video_frame_unmap (&src_frame);
    gst_video_frame_unmap (&aligned_frame);
    goto error;
  }

  gst_video_frame_unmap (&src_frame);
  gst_video_frame_unmap (&aligned_frame);

  gst_buffer_replace (&frame->input_buffer, aligned_buffer);
  gst_buffer_unref (aligned_buffer);

  return TRUE;

error:
  if (aligned_buffer)
    gst_buffer_unref (aligned_buffer);
  return FALSE;
}

static GstFlowReturn
gst_svthevc_enc_send_frame (GstSvtHevcEnc * encoder, GstVideoCodecFrame * frame)
{
  GstFlowReturn ret = GST_FLOW_OK;
  EB_BUFFERHEADERTYPE *headerPtr = NULL;
  GstVideoInfo *info = &encoder->input_state->info;
  GstVideoFrame vframe;
  EB_ERRORTYPE svt_ret;
  guint i;

  if (encoder->svt_eos_flag == EOS_REACHED) {
    if (frame)
      gst_video_codec_frame_unref (frame);
    return GST_FLOW_OK;
  }

  if (encoder->svt_eos_flag == EOS_TOTRIGGER) {
    if (frame)
      gst_video_codec_frame_unref (frame);
    return GST_FLOW_EOS;
  }

  if (!frame)
    goto out;

  headerPtr = encoder->in_buf;

  /* Check that stride is a multiple of pstride, otherwise convert to
   * desired stride from SVT-HEVC.*/
  for (i = 0; i < 3; i++) {
    if (GST_VIDEO_INFO_COMP_STRIDE (info,
            i) % GST_VIDEO_INFO_COMP_PSTRIDE (info, i)) {
      GST_LOG_OBJECT (encoder, "need to convert frame");
      if (!gst_svthevc_enc_convert_frame (encoder, frame)) {
        if (frame)
          gst_video_codec_frame_unref (frame);
      }
      break;
    }
  }

  if (!gst_video_frame_map (&vframe, info, frame->input_buffer, GST_MAP_READ)) {
    GST_ERROR_OBJECT (encoder, "Failed to map frame");
    if (frame)
      gst_video_codec_frame_unref (frame);
    return GST_FLOW_ERROR;
  }

  read_in_data (&encoder->enc_params, &vframe, headerPtr);

  headerPtr->nFlags = 0;
  headerPtr->sliceType = EB_INVALID_PICTURE;
  headerPtr->pAppPrivate = NULL;
  headerPtr->pts = frame->pts;

  if (encoder->reconfig && frame) {
    /* svthevc_encoder_reconfig is not yet implemented thus we shut down and re-create encoder */
    GST_INFO_OBJECT (encoder, "reconfigure encoder");
    gst_svthevc_enc_drain_encoder (encoder, TRUE);
    GST_OBJECT_LOCK (encoder);
    if (!gst_svthevc_enc_init_encoder (encoder)) {
      GST_OBJECT_UNLOCK (encoder);
      return GST_FLOW_ERROR;
    }
    GST_OBJECT_UNLOCK (encoder);
  }

  if (headerPtr && frame) {
    if (GST_VIDEO_CODEC_FRAME_IS_FORCE_KEYFRAME (frame)) {
      GST_INFO_OBJECT (encoder, "Forcing key frame");
      headerPtr->sliceType = EB_IDR_PICTURE;
    }
  }

out:
  if (!headerPtr) {
    EB_BUFFERHEADERTYPE headerPtrLast;

    if (encoder->first_buffer) {
      GST_DEBUG_OBJECT (encoder, "No need to send eos buffer");
      encoder->svt_eos_flag = EOS_TOTRIGGER;
      return GST_FLOW_OK;
    }

    headerPtrLast.nAllocLen = 0;
    headerPtrLast.nFilledLen = 0;
    headerPtrLast.nTickCount = 0;
    headerPtrLast.pAppPrivate = NULL;
    headerPtrLast.pBuffer = NULL;
    headerPtrLast.nFlags = EB_BUFFERFLAG_EOS;

    GST_DEBUG_OBJECT (encoder, "drain frame");
    svt_ret = EbH265EncSendPicture (encoder->svt_handle, &headerPtrLast);
    encoder->svt_eos_flag = EOS_REACHED;
  } else {
    GST_LOG_OBJECT (encoder, "encode frame");
    svt_ret = EbH265EncSendPicture (encoder->svt_handle, headerPtr);
    encoder->first_buffer = FALSE;
  }

  GST_LOG_OBJECT (encoder, "encoder result (%d)", svt_ret);

  if (svt_ret != EB_ErrorNone) {
    GST_ELEMENT_ERROR (encoder, STREAM, ENCODE,
        ("Encode svthevc frame failed."),
        ("svthevc_encoder_encode return code=%d", svt_ret));
    ret = GST_FLOW_ERROR;
  }

  /* Input frame is now queued */
  if (frame) {
    gst_video_frame_unmap (&vframe);
    gst_video_codec_frame_unref (frame);
  }

  return ret;
}

static GstVideoCodecFrame *
gst_svthevc_encoder_get_frame (GstVideoEncoder * encoder, GstClockTime ts)
{
  GList *g;
  GList *frames;
  GstVideoCodecFrame *frame = NULL;

  GST_LOG_OBJECT (encoder, "timestamp : %" GST_TIME_FORMAT, GST_TIME_ARGS (ts));

  frames = gst_video_encoder_get_frames (GST_VIDEO_ENCODER (encoder));

  for (g = frames; g; g = g->next) {
    GstVideoCodecFrame *tmp = g->data;

    if (tmp->pts == ts) {
      frame = gst_video_codec_frame_ref (tmp);
      break;
    }
  }

  g_list_free_full (frames, (GDestroyNotify) gst_video_codec_frame_unref);

  return frame;
}

static GstClockTime
gst_svthevc_encoder_get_oldest_pts (GstVideoEncoder * encoder)
{
  GList *g;
  GList *frames;
  GstClockTime min_ts = GST_CLOCK_TIME_NONE;
  gboolean seen_none = FALSE;

  frames = gst_video_encoder_get_frames (GST_VIDEO_ENCODER (encoder));

  /* find the lowest unsent PTS */
  for (g = frames; g; g = g->next) {
    GstVideoCodecFrame *tmp = g->data;

    if (!GST_CLOCK_TIME_IS_VALID (tmp->abidata.ABI.ts)) {
      seen_none = TRUE;
      continue;
    }

    if (!GST_CLOCK_TIME_IS_VALID (min_ts) || tmp->abidata.ABI.ts < min_ts) {
      if (!seen_none)
        min_ts = tmp->abidata.ABI.ts;
    }
  }

  g_list_free_full (frames, (GDestroyNotify) gst_video_codec_frame_unref);

  return min_ts;
}

static GstFlowReturn
gst_svthevc_enc_receive_frame (GstSvtHevcEnc * encoder,
    gboolean * got_packet, gboolean send)
{
  GstVideoCodecFrame *frame = NULL;
  GstBuffer *out_buf = NULL;
  GstFlowReturn ret = GST_FLOW_OK;
  EB_BUFFERHEADERTYPE *output_buffer = NULL;
  EB_ERRORTYPE svt_ret;

  *got_packet = FALSE;

  if (encoder->svt_eos_flag == EOS_TOTRIGGER)
    return GST_FLOW_EOS;

  svt_ret =
      EbH265GetPacket (encoder->svt_handle, &output_buffer,
      encoder->svt_eos_flag);

  if (svt_ret == EB_NoErrorEmptyQueue) {
    GST_DEBUG_OBJECT (encoder, "no output yet");
    return GST_FLOW_OK;
  }

  if (svt_ret != EB_ErrorNone || !output_buffer) {
    GST_ELEMENT_ERROR (encoder, STREAM, ENCODE,
        ("Encode svthevc frame failed."),
        ("EbH265GetPacket return code=%d", svt_ret));
    return GST_FLOW_ERROR;
  }

  GST_LOG_OBJECT (encoder, "got %d from svt", output_buffer->nFlags);

  *got_packet = TRUE;

  frame =
      gst_svthevc_encoder_get_frame (GST_VIDEO_ENCODER (encoder),
      output_buffer->pts);

  if (!frame && send) {
    GST_ELEMENT_ERROR (encoder, STREAM, ENCODE,
        ("Encode svthevc frame failed."), ("Frame not found."));
    ret = GST_FLOW_ERROR;
    goto out;
  }

  if (!send || !frame) {
    GST_DEBUG_OBJECT (encoder, "not sending (%d) or frame not found (%d)", send,
        frame != NULL);
    ret = GST_FLOW_OK;
    goto out;
  }

  GST_LOG_OBJECT (encoder,
      "output picture ready system=%d frame found %d",
      frame->system_frame_number, frame != NULL);

  if (encoder->update_latency) {
    gst_svthevc_enc_set_latency (encoder);
    encoder->update_latency = FALSE;
  }

  out_buf = gst_buffer_new_allocate (NULL, output_buffer->nFilledLen, NULL);
  gst_buffer_fill (out_buf, 0, output_buffer->pBuffer,
      output_buffer->nFilledLen);

  frame->output_buffer = out_buf;

  if (output_buffer->sliceType == EB_IDR_PICTURE)
    GST_VIDEO_CODEC_FRAME_SET_SYNC_POINT (frame);
  else
    GST_VIDEO_CODEC_FRAME_UNSET_SYNC_POINT (frame);

  if (encoder->push_header) {
    GstBuffer *header;

    header = gst_svthevc_enc_get_header_buffer (encoder);
    frame->output_buffer = gst_buffer_append (header, frame->output_buffer);
    encoder->push_header = FALSE;
  }

  frame->pts = output_buffer->pts;

  if (encoder->pred_structure) {
    /* Since the SVT-HEVC does not support adjust dts when bframe was enabled,
     * output pts can be smaller than dts. The maximum difference between DTS and PTS can be calculated
     * using the PTS difference between the first frame and the second frame.
     */
    if (encoder->dts_offset == 0) {
      if (encoder->first_frame) {
        if (frame->pts > encoder->first_frame->pts) {
          encoder->dts_offset = frame->pts - encoder->first_frame->pts;
        } else {
          GstVideoInfo *info = &encoder->input_state->info;
          GstClockTime duration;
          gdouble framerate;

          GST_WARNING_OBJECT (encoder, "Could not calculate DTS offset");

          /* No way to get maximum bframe count since SVT-HEVC does not support it,
           * so using keyframe interval instead.
           */
          if (info->fps_d == 0 || info->fps_n == 0) {
            /* No way to get duration, assume 60fps. */
            duration = gst_util_uint64_scale (1, GST_SECOND, 60);
            framerate = 60;
          } else {
            duration =
                gst_util_uint64_scale (info->fps_d, GST_SECOND, info->fps_n);
            gst_util_fraction_to_double (info->fps_n, info->fps_d, &framerate);
          }

          if (encoder->keyintmax > 0) {
            encoder->dts_offset = duration * encoder->keyintmax;
          } else {
            /* The SVT-HEVC sets the default gop-size the closest possible to
             * 1 second without breaking the minigop.
             */
            gint mini_gop = (1 << (encoder->hierarchical_level));
            gint keyintmin = ((int) ((framerate) / mini_gop) * (mini_gop));
            gint keyintmax =
                ((int) ((framerate + mini_gop) / mini_gop) * (mini_gop));
            gint keyint =
                (ABS ((framerate - keyintmax)) >
                ABS ((framerate - keyintmin))) ? keyintmin : keyintmax;

            if (encoder->enable_open_gop)
              keyint -= 1;

            encoder->dts_offset = duration * keyint;
          }
        }

        GST_INFO_OBJECT (encoder, "Calculated DTS offset %" GST_TIME_FORMAT,
            GST_TIME_ARGS (encoder->dts_offset));

        encoder->first_frame->dts =
            gst_svthevc_encoder_get_oldest_pts (GST_VIDEO_ENCODER (encoder));
        if (GST_CLOCK_TIME_IS_VALID (encoder->first_frame->dts))
          encoder->first_frame->dts -= encoder->dts_offset;

        GST_LOG_OBJECT (encoder,
            "output: frame dts %" GST_TIME_FORMAT " pts %" GST_TIME_FORMAT,
            GST_TIME_ARGS (encoder->first_frame->dts),
            GST_TIME_ARGS (encoder->first_frame->pts));

        ret =
            gst_video_encoder_finish_frame (GST_VIDEO_ENCODER (encoder),
            encoder->first_frame);
        encoder->first_frame = NULL;
      } else {
        encoder->first_frame = frame;
        frame = NULL;
        goto out;
      }
    }

    frame->dts =
        gst_svthevc_encoder_get_oldest_pts (GST_VIDEO_ENCODER (encoder));
    if (GST_CLOCK_TIME_IS_VALID (frame->dts))
      frame->dts -= encoder->dts_offset;
  }

  GST_LOG_OBJECT (encoder,
      "output: frame dts %" GST_TIME_FORMAT " pts %" GST_TIME_FORMAT,
      GST_TIME_ARGS (frame->dts), GST_TIME_ARGS (frame->pts));

out:
  if (output_buffer->nFlags == EB_BUFFERFLAG_EOS)
    encoder->svt_eos_flag = EOS_TOTRIGGER;

  if (output_buffer)
    EbH265ReleaseOutBuffer (&output_buffer);

  if (frame)
    ret = gst_video_encoder_finish_frame (GST_VIDEO_ENCODER (encoder), frame);

  return ret;
}

static GstFlowReturn
gst_svthevc_enc_drain_encoder (GstSvtHevcEnc * encoder, gboolean send)
{
  GstFlowReturn ret = GST_FLOW_OK;
  gboolean got_packet;

  /* first send the remaining frames */

  if (G_UNLIKELY (encoder->svt_handle == NULL) ||
      G_UNLIKELY (encoder->svt_eos_flag == EOS_TOTRIGGER))
    goto done;

  ret = gst_svthevc_enc_send_frame (encoder, NULL);

  if (ret != GST_FLOW_OK)
    goto done;

  do {
    ret = gst_svthevc_enc_receive_frame (encoder, &got_packet, send);
    GST_LOG_OBJECT (encoder, "ret %d, got_packet %d", ret, got_packet);
    if (ret != GST_FLOW_OK)
      break;
  } while (got_packet);

done:
  if (encoder->first_frame) {
    GST_LOG_OBJECT (encoder,
        "output: frame dts %" GST_TIME_FORMAT " pts %" GST_TIME_FORMAT,
        GST_TIME_ARGS (encoder->first_frame->dts),
        GST_TIME_ARGS (encoder->first_frame->pts));
    gst_video_encoder_finish_frame (GST_VIDEO_ENCODER (encoder),
        encoder->first_frame);
    encoder->first_frame = NULL;
  }

  return ret;
}

static void
gst_svthevc_enc_reconfig (GstSvtHevcEnc * encoder)
{
  encoder->reconfig = TRUE;
}

static void
gst_svthevc_enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstSvtHevcEnc *encoder;
  GstState state;

  encoder = GST_SVTHEVC_ENC (object);

  GST_OBJECT_LOCK (encoder);

  state = GST_STATE (encoder);
  if ((state != GST_STATE_READY && state != GST_STATE_NULL) &&
      !(pspec->flags & GST_PARAM_MUTABLE_PLAYING))
    goto wrong_state;

  switch (prop_id) {
    case PROP_INSERT_VUI:
      encoder->insert_vui = g_value_get_boolean (value);
      break;
    case PROP_AUD:
      encoder->aud = g_value_get_boolean (value);
      break;
    case PROP_HIERARCHICAL_LEVEL:
      encoder->hierarchical_level = g_value_get_enum (value);
      break;
    case PROP_LOOKAHEAD_DISTANCE:
      encoder->la_depth = g_value_get_uint (value);
      break;
    case PROP_ENCODER_MODE:
      encoder->enc_mode = g_value_get_uint (value);
      break;
    case PROP_RC_MODE:
      encoder->rc_mode = g_value_get_enum (value);
      break;
    case PROP_QP_I:
      encoder->qp_i = g_value_get_uint (value);
      break;
    case PROP_QP_MAX:
      encoder->qp_max = g_value_get_uint (value);
      break;
    case PROP_QP_MIN:
      encoder->qp_min = g_value_get_uint (value);
      break;
    case PROP_SCENE_CHANGE_DETECTION:
      encoder->scene_change_detection = g_value_get_boolean (value);
      break;
    case PROP_TUNE:
      encoder->tune = g_value_get_enum (value);
      break;
    case PROP_BASE_LAYER_SWITCH_MODE:
      encoder->base_layer_switch_mode = g_value_get_enum (value);
      break;
    case PROP_BITRATE:
      encoder->bitrate = g_value_get_uint (value);
      break;
    case PROP_KEY_INT_MAX:
      encoder->keyintmax = g_value_get_int (value);
      break;
    case PROP_ENABLE_OPEN_GOP:
      encoder->enable_open_gop = g_value_get_boolean (value);
      break;
    case PROP_CONFIG_INTERVAL:
      encoder->config_interval = g_value_get_uint (value);
      break;
    case PROP_CORES:
      encoder->cores = g_value_get_uint (value);
      break;
    case PROP_SOCKET:
      encoder->socket = g_value_get_int (value);
      break;
    case PROP_TILE_ROW:
      encoder->tile_row = g_value_get_uint (value);
      break;
    case PROP_TILE_COL:
      encoder->tile_col = g_value_get_uint (value);
      break;
    case PROP_PRED_STRUCTURE:
      encoder->pred_structure = g_value_get_enum (value);
      break;
    case PROP_VBV_MAX_RATE:
      encoder->vbv_maxrate = g_value_get_uint (value);
      break;
    case PROP_VBV_BUFFER_SIZE:
      encoder->vbv_bufsize = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  gst_svthevc_enc_reconfig (encoder);
  GST_OBJECT_UNLOCK (encoder);
  return;

wrong_state:
  {
    GST_WARNING_OBJECT (encoder, "setting property in wrong state");
    GST_OBJECT_UNLOCK (encoder);
  }
}

static void
gst_svthevc_enc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstSvtHevcEnc *encoder;

  encoder = GST_SVTHEVC_ENC (object);

  GST_OBJECT_LOCK (encoder);
  switch (prop_id) {
    case PROP_INSERT_VUI:
      g_value_set_boolean (value, encoder->insert_vui);
      break;
    case PROP_AUD:
      g_value_set_boolean (value, encoder->aud);
      break;
    case PROP_HIERARCHICAL_LEVEL:
      g_value_set_enum (value, encoder->hierarchical_level);
      break;
    case PROP_LOOKAHEAD_DISTANCE:
      g_value_set_uint (value, encoder->la_depth);
      break;
    case PROP_ENCODER_MODE:
      g_value_set_uint (value, encoder->enc_mode);
      break;
    case PROP_RC_MODE:
      g_value_set_enum (value, encoder->rc_mode);
      break;
    case PROP_QP_I:
      g_value_set_uint (value, encoder->qp_i);
      break;
    case PROP_QP_MAX:
      g_value_set_uint (value, encoder->qp_max);
      break;
    case PROP_QP_MIN:
      g_value_set_uint (value, encoder->qp_min);
      break;
    case PROP_SCENE_CHANGE_DETECTION:
      g_value_set_boolean (value, encoder->scene_change_detection);
      break;
    case PROP_TUNE:
      g_value_set_enum (value, encoder->tune);
      break;
    case PROP_BASE_LAYER_SWITCH_MODE:
      g_value_set_enum (value, encoder->base_layer_switch_mode);
      break;
    case PROP_BITRATE:
      g_value_set_uint (value, encoder->bitrate);
      break;
    case PROP_KEY_INT_MAX:
      g_value_set_int (value, encoder->keyintmax);
      break;
    case PROP_ENABLE_OPEN_GOP:
      g_value_set_boolean (value, encoder->enable_open_gop);
      break;
    case PROP_CONFIG_INTERVAL:
      g_value_set_uint (value, encoder->config_interval);
      break;
    case PROP_CORES:
      g_value_set_uint (value, encoder->cores);
      break;
    case PROP_SOCKET:
      g_value_set_int (value, encoder->socket);
      break;
    case PROP_TILE_ROW:
      g_value_set_uint (value, encoder->tile_row);
      break;
    case PROP_TILE_COL:
      g_value_set_uint (value, encoder->tile_col);
      break;
    case PROP_PRED_STRUCTURE:
      g_value_set_enum (value, encoder->pred_structure);
      break;
    case PROP_VBV_MAX_RATE:
      g_value_set_uint (value, encoder->vbv_maxrate);
      break;
    case PROP_VBV_BUFFER_SIZE:
      g_value_set_uint (value, encoder->vbv_bufsize);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (encoder);
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (svthevc_enc_debug, "svthevcenc", 0,
      "h265 encoding element");

  return gst_element_register (plugin, "svthevcenc",
      GST_RANK_PRIMARY, GST_TYPE_SVTHEVC_ENC);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    svthevcenc,
    "svt-hevc encoder based H265 plugins",
    plugin_init, VERSION, "GPL", GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
