/* GStreamer H265 encoder plugin
 * Copyright (C) 2005 Michal Benes <michal.benes@itonis.tv>
 * Copyright (C) 2005 Josef Zlomek <josef.zlomek@itonis.tv>
 * Copyright (C) 2008 Mark Nauwelaerts <mnauw@users.sf.net>
 * Copyright (C) 2014 Thijs Vermeir <thijs.vermeir@barco.com>
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
 * SECTION:element-x265enc
 * @title: x265enc
 *
 * This element encodes raw video into H265 compressed data.
 *
 **/

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "gstx265enc.h"

#include <gst/pbutils/pbutils.h>
#include <gst/video/video.h>
#include <gst/video/gstvideometa.h>
#include <gst/video/gstvideopool.h>

#include <string.h>
#include <stdlib.h>

GST_DEBUG_CATEGORY_STATIC (x265_enc_debug);
#define GST_CAT_DEFAULT x265_enc_debug

static x265_api default_vtable;

static const x265_api *vtable_8bit = NULL;
static const x265_api *vtable_10bit = NULL;
static const x265_api *vtable_12bit = NULL;

enum
{
  PROP_0,
  PROP_BITRATE,
  PROP_QP,
  PROP_OPTION_STRING,
  PROP_X265_LOG_LEVEL,
  PROP_SPEED_PRESET,
  PROP_TUNE,
  PROP_KEY_INT_MAX
};

#define PROP_BITRATE_DEFAULT            (2 * 1024)
#define PROP_QP_DEFAULT                 -1
#define PROP_OPTION_STRING_DEFAULT      ""
#define PROP_LOG_LEVEL_DEFAULT           -1     /* None   */
#define PROP_SPEED_PRESET_DEFAULT        6      /* Medium */
#define PROP_TUNE_DEFAULT                2      /* SSIM   */
#define PROP_KEY_INT_MAX_DEFAULT         0      /* x265 lib default */

#define GST_X265_ENC_LOG_LEVEL_TYPE (gst_x265_enc_log_level_get_type())
static GType
gst_x265_enc_log_level_get_type (void)
{
  static GType log_level = 0;

  static const GEnumValue log_levels[] = {
    {X265_LOG_NONE, "No logging", "none"},
    {X265_LOG_ERROR, "Error", "error"},
    {X265_LOG_WARNING, "Warning", "warning"},
    {X265_LOG_INFO, "Info", "info"},
    {X265_LOG_DEBUG, "Debug", "debug"},
    {X265_LOG_FULL, "Full", "full"},
    {0, NULL, NULL}
  };

  if (!log_level) {
    log_level = g_enum_register_static ("GstX265LogLevel", log_levels);
  }
  return log_level;
}

#define GST_X265_ENC_SPEED_PRESET_TYPE (gst_x265_enc_speed_preset_get_type())
static GType
gst_x265_enc_speed_preset_get_type (void)
{
  static GType speed_preset = 0;
  static GEnumValue *speed_presets;
  int n, i;

  if (speed_preset != 0)
    return speed_preset;

  n = 0;
  while (x265_preset_names[n] != NULL)
    n++;

  speed_presets = g_new0 (GEnumValue, n + 2);

  speed_presets[0].value = 0;
  speed_presets[0].value_name = "No preset";
  speed_presets[0].value_nick = "No preset";

  for (i = 0; i < n; i++) {
    speed_presets[i + 1].value = i + 1;
    speed_presets[i + 1].value_name = x265_preset_names[i];
    speed_presets[i + 1].value_nick = x265_preset_names[i];
  }

  speed_preset = g_enum_register_static ("GstX265SpeedPreset", speed_presets);

  return speed_preset;
}

#define GST_X265_ENC_TUNE_TYPE (gst_x265_enc_tune_get_type())
static GType
gst_x265_enc_tune_get_type (void)
{
  static GType tune = 0;
  static GEnumValue *tune_values;
  int n, i;

  if (tune != 0)
    return tune;

  n = 0;
  while (x265_tune_names[n] != NULL)
    n++;

  tune_values = g_new0 (GEnumValue, n + 2);

  tune_values[0].value = 0;
  tune_values[0].value_name = "No tunning";
  tune_values[0].value_nick = "No tunning";

  for (i = 0; i < n; i++) {
    tune_values[i + 1].value = i + 1;
    tune_values[i + 1].value_name = x265_tune_names[i];
    tune_values[i + 1].value_nick = x265_tune_names[i];
  }

  tune = g_enum_register_static ("GstX265Tune", tune_values);

  return tune;
}

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-h265, "
        "framerate = (fraction) [0/1, MAX], "
        "width = (int) [ 16, MAX ], " "height = (int) [ 16, MAX ], "
        "stream-format = (string) byte-stream, "
        "alignment = (string) au, "
        "profile = (string) { main, main-still-picture, main-intra, main-444,"
        " main-444-intra, main-444-still-picture,"
        " main-10, main-10-intra, main-422-10, main-422-10-intra,"
        " main-444-10, main-444-10-intra,"
        " main-12, main-12-intra, main-422-12, main-422-12-intra,"
        " main-444-12, main-444-12-intra }")
    );

static void gst_x265_enc_finalize (GObject * object);
static gboolean gst_x265_enc_start (GstVideoEncoder * encoder);
static gboolean gst_x265_enc_stop (GstVideoEncoder * encoder);
static gboolean gst_x265_enc_flush (GstVideoEncoder * encoder);

static gboolean gst_x265_enc_init_encoder (GstX265Enc * encoder);
static void gst_x265_enc_close_encoder (GstX265Enc * encoder);

static GstFlowReturn gst_x265_enc_finish (GstVideoEncoder * encoder);
static GstFlowReturn gst_x265_enc_handle_frame (GstVideoEncoder * encoder,
    GstVideoCodecFrame * frame);
static void gst_x265_enc_flush_frames (GstX265Enc * encoder, gboolean send);
static GstFlowReturn gst_x265_enc_encode_frame (GstX265Enc * encoder,
    x265_picture * pic_in, GstVideoCodecFrame * input_frame, guint32 * i_nal,
    gboolean send);
static gboolean gst_x265_enc_set_format (GstVideoEncoder * video_enc,
    GstVideoCodecState * state);
static gboolean gst_x265_enc_propose_allocation (GstVideoEncoder * encoder,
    GstQuery * query);

static void gst_x265_enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_x265_enc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static gboolean x265enc_element_init (GstPlugin * plugin);

#define gst_x265_enc_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstX265Enc, gst_x265_enc, GST_TYPE_VIDEO_ENCODER,
    G_IMPLEMENT_INTERFACE (GST_TYPE_PRESET, NULL));
GST_ELEMENT_REGISTER_DEFINE_CUSTOM (x265enc, x265enc_element_init);

static gboolean
gst_x265_enc_add_x265_chroma_format (GstStructure * s,
    gboolean allow_420, gboolean allow_422, gboolean allow_444,
    gboolean allow_8bit, gboolean allow_10bit, gboolean allow_12bit)
{
  GValue fmts = G_VALUE_INIT;
  GValue fmt = G_VALUE_INIT;
  gboolean ret = FALSE;

  g_value_init (&fmts, GST_TYPE_LIST);
  g_value_init (&fmt, G_TYPE_STRING);

  if (allow_8bit) {
    if (allow_444) {
      g_value_set_string (&fmt, "Y444");
      gst_value_list_append_value (&fmts, &fmt);
    }

    if (allow_422) {
      g_value_set_string (&fmt, "Y42B");
      gst_value_list_append_value (&fmts, &fmt);
    }

    if (allow_420) {
      g_value_set_string (&fmt, "I420");
      gst_value_list_append_value (&fmts, &fmt);
    }
  }

  if (allow_10bit) {
    if (allow_444) {
      if (G_BYTE_ORDER == G_LITTLE_ENDIAN)
        g_value_set_string (&fmt, "Y444_10LE");
      else
        g_value_set_string (&fmt, "Y444_10BE");

      gst_value_list_append_value (&fmts, &fmt);
    }

    if (allow_422) {
      if (G_BYTE_ORDER == G_LITTLE_ENDIAN)
        g_value_set_string (&fmt, "I422_10LE");
      else
        g_value_set_string (&fmt, "I422_10BE");

      gst_value_list_append_value (&fmts, &fmt);
    }

    if (allow_420) {
      if (G_BYTE_ORDER == G_LITTLE_ENDIAN)
        g_value_set_string (&fmt, "I420_10LE");
      else
        g_value_set_string (&fmt, "I420_10BE");

      gst_value_list_append_value (&fmts, &fmt);
    }
  }

  if (allow_12bit) {
    if (allow_444) {
      if (G_BYTE_ORDER == G_LITTLE_ENDIAN)
        g_value_set_string (&fmt, "Y444_12LE");
      else
        g_value_set_string (&fmt, "Y444_12BE");

      gst_value_list_append_value (&fmts, &fmt);
    }

    if (allow_422) {
      if (G_BYTE_ORDER == G_LITTLE_ENDIAN)
        g_value_set_string (&fmt, "I422_12LE");
      else
        g_value_set_string (&fmt, "I422_12BE");

      gst_value_list_append_value (&fmts, &fmt);
    }

    if (allow_420) {
      if (G_BYTE_ORDER == G_LITTLE_ENDIAN)
        g_value_set_string (&fmt, "I420_12LE");
      else
        g_value_set_string (&fmt, "I420_12BE");

      gst_value_list_append_value (&fmts, &fmt);
    }
  }

  if (gst_value_list_get_size (&fmts) != 0) {
    gst_structure_take_value (s, "format", &fmts);
    ret = TRUE;
  } else {
    g_value_unset (&fmts);
  }

  g_value_unset (&fmt);

  return ret;
}

static gboolean
gst_x265_enc_sink_query (GstVideoEncoder * enc, GstQuery * query)
{
  GstPad *pad = GST_VIDEO_ENCODER_SINK_PAD (enc);
  gboolean res;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_ACCEPT_CAPS:{
      GstCaps *acceptable, *caps;

      acceptable = gst_pad_get_pad_template_caps (pad);

      gst_query_parse_accept_caps (query, &caps);

      gst_query_set_accept_caps_result (query,
          gst_caps_is_subset (caps, acceptable));
      gst_caps_unref (acceptable);
      res = TRUE;
    }
      break;
    default:
      res = GST_VIDEO_ENCODER_CLASS (parent_class)->sink_query (enc, query);
      break;
  }

  return res;
}

static void
check_formats (const gchar * str, guint * max_chroma, guint * max_bit_minus_8)
{
  if (!str)
    return;

  if (g_strrstr (str, "-444"))
    *max_chroma = 2;
  else if (g_strrstr (str, "-422") && *max_chroma < 1)
    *max_chroma = 1;

  if (g_strrstr (str, "-12"))
    *max_bit_minus_8 = 4;
  else if (g_strrstr (str, "-10") && *max_bit_minus_8 < 2)
    *max_bit_minus_8 = 2;
}

static GstCaps *
gst_x265_enc_sink_getcaps (GstVideoEncoder * enc, GstCaps * filter)
{
  GstCaps *templ_caps, *supported_incaps;
  GstCaps *allowed;
  GstCaps *fcaps;
  gint i, j;
  gboolean has_profile = FALSE;
  guint max_chroma_index = 0;
  guint max_bit_minus_8 = 0;

  templ_caps = gst_pad_get_pad_template_caps (GST_VIDEO_ENCODER_SINK_PAD (enc));
  allowed = gst_pad_get_allowed_caps (enc->srcpad);

  GST_LOG_OBJECT (enc, "template caps %" GST_PTR_FORMAT, templ_caps);
  GST_LOG_OBJECT (enc, "allowed caps %" GST_PTR_FORMAT, allowed);

  if (!allowed) {
    /* no peer */
    supported_incaps = templ_caps;
    goto done;
  } else if (gst_caps_is_empty (allowed)) {
    /* cannot negotiate, return empty caps */
    gst_caps_unref (templ_caps);
    return allowed;
  }

  /* fill format based on requested profile */
  for (i = 0; i < gst_caps_get_size (allowed); i++) {
    const GstStructure *allowed_s = gst_caps_get_structure (allowed, i);
    const GValue *val;

    if ((val = gst_structure_get_value (allowed_s, "profile"))) {
      if (G_VALUE_HOLDS_STRING (val)) {
        check_formats (g_value_get_string (val), &max_chroma_index,
            &max_bit_minus_8);
        has_profile = TRUE;
      } else if (GST_VALUE_HOLDS_LIST (val)) {
        for (j = 0; j < gst_value_list_get_size (val); j++) {
          const GValue *vlist = gst_value_list_get_value (val, j);

          if (G_VALUE_HOLDS_STRING (vlist)) {
            check_formats (g_value_get_string (vlist), &max_chroma_index,
                &max_bit_minus_8);
            has_profile = TRUE;
          }
        }
      }
    }
  }

  if (!has_profile) {
    /* downstream did not request profile */
    supported_incaps = templ_caps;
  } else {
    GstStructure *s;
    gboolean has_12bit = FALSE;
    gboolean has_10bit = FALSE;
    gboolean has_8bit = TRUE;
    gboolean has_444 = FALSE;
    gboolean has_422 = FALSE;
    gboolean has_420 = TRUE;

    supported_incaps = gst_caps_new_simple ("video/x-raw",
        "framerate", GST_TYPE_FRACTION_RANGE, 0, 1, G_MAXINT, 1,
        "width", GST_TYPE_INT_RANGE, 16, G_MAXINT,
        "height", GST_TYPE_INT_RANGE, 16, G_MAXINT, NULL);

    /* NOTE: 12bits profiles can accept 8bits and 10bits format */
    if (max_bit_minus_8 >= 4)
      has_12bit = TRUE;
    if (max_bit_minus_8 >= 2)
      has_10bit = TRUE;

    has_8bit &= !!vtable_8bit;
    has_10bit &= !!vtable_10bit;
    has_12bit &= !!vtable_12bit;

    /* 4:4:4 profiles can handle 4:2:2 and 4:2:0 */
    if (max_chroma_index >= 2)
      has_444 = TRUE;
    if (max_chroma_index >= 1)
      has_422 = TRUE;

    s = gst_caps_get_structure (supported_incaps, 0);
    gst_x265_enc_add_x265_chroma_format (s, has_420, has_422, has_444,
        has_8bit, has_10bit, has_12bit);

    gst_caps_unref (templ_caps);
  }

done:
  GST_LOG_OBJECT (enc, "supported caps %" GST_PTR_FORMAT, supported_incaps);
  fcaps = gst_video_encoder_proxy_getcaps (enc, supported_incaps, filter);
  gst_clear_caps (&supported_incaps);
  gst_clear_caps (&allowed);

  GST_LOG_OBJECT (enc, "proxy caps %" GST_PTR_FORMAT, fcaps);

  return fcaps;
}

static void
gst_x265_enc_class_init (GstX265EncClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *element_class;
  GstVideoEncoderClass *gstencoder_class;
  GstPadTemplate *sink_templ;
  GstCaps *supported_sinkcaps;

  gobject_class = G_OBJECT_CLASS (klass);
  element_class = GST_ELEMENT_CLASS (klass);
  gstencoder_class = GST_VIDEO_ENCODER_CLASS (klass);

  gobject_class->set_property = gst_x265_enc_set_property;
  gobject_class->get_property = gst_x265_enc_get_property;
  gobject_class->finalize = gst_x265_enc_finalize;

  gstencoder_class->set_format = GST_DEBUG_FUNCPTR (gst_x265_enc_set_format);
  gstencoder_class->handle_frame =
      GST_DEBUG_FUNCPTR (gst_x265_enc_handle_frame);
  gstencoder_class->start = GST_DEBUG_FUNCPTR (gst_x265_enc_start);
  gstencoder_class->stop = GST_DEBUG_FUNCPTR (gst_x265_enc_stop);
  gstencoder_class->flush = GST_DEBUG_FUNCPTR (gst_x265_enc_flush);
  gstencoder_class->finish = GST_DEBUG_FUNCPTR (gst_x265_enc_finish);
  gstencoder_class->getcaps = GST_DEBUG_FUNCPTR (gst_x265_enc_sink_getcaps);
  gstencoder_class->sink_query = GST_DEBUG_FUNCPTR (gst_x265_enc_sink_query);
  gstencoder_class->propose_allocation =
      GST_DEBUG_FUNCPTR (gst_x265_enc_propose_allocation);

  g_object_class_install_property (gobject_class, PROP_BITRATE,
      g_param_spec_uint ("bitrate", "Bitrate", "Bitrate in kbit/sec", 1,
          100 * 1024, PROP_BITRATE_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_PLAYING));

  g_object_class_install_property (gobject_class, PROP_QP,
      g_param_spec_int ("qp", "Quantization parameter",
          "QP for P slices in (implied) CQP mode (-1 = disabled)", -1,
          51, PROP_QP_DEFAULT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_OPTION_STRING,
      g_param_spec_string ("option-string", "Option string",
          "String of x265 options (overridden by element properties)"
          " in the format \"key1=value1:key2=value2\".",
          PROP_OPTION_STRING_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_X265_LOG_LEVEL,
      g_param_spec_enum ("log-level", "(internal) x265 log level",
          "x265 log level", GST_X265_ENC_LOG_LEVEL_TYPE,
          PROP_LOG_LEVEL_DEFAULT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_SPEED_PRESET,
      g_param_spec_enum ("speed-preset", "Speed preset",
          "Preset name for speed/quality tradeoff options",
          GST_X265_ENC_SPEED_PRESET_TYPE, PROP_SPEED_PRESET_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_TUNE,
      g_param_spec_enum ("tune", "Tune options",
          "Preset name for tuning options", GST_X265_ENC_TUNE_TYPE,
          PROP_TUNE_DEFAULT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  /**
   * GstX265Enc::key-int-max:
   *
   * Controls maximum number of frames since the last keyframe
   *
   * Since: 1.16
   */
  g_object_class_install_property (gobject_class, PROP_KEY_INT_MAX,
      g_param_spec_int ("key-int-max", "Max key frame",
          "Maximal distance between two key-frames (0 = x265 default / 250)",
          0, G_MAXINT32, PROP_KEY_INT_MAX_DEFAULT, G_PARAM_READWRITE));

  gst_element_class_set_static_metadata (element_class,
      "x265enc", "Codec/Encoder/Video", "H265 Encoder",
      "Thijs Vermeir <thijs.vermeir@barco.com>");

  supported_sinkcaps = gst_caps_new_simple ("video/x-raw",
      "framerate", GST_TYPE_FRACTION_RANGE, 0, 1, G_MAXINT, 1,
      "width", GST_TYPE_INT_RANGE, 16, G_MAXINT,
      "height", GST_TYPE_INT_RANGE, 16, G_MAXINT, NULL);

  gst_x265_enc_add_x265_chroma_format (gst_caps_get_structure
      (supported_sinkcaps, 0), TRUE, TRUE, TRUE, !!vtable_8bit,
      !!vtable_10bit, !!vtable_12bit);

  sink_templ = gst_pad_template_new ("sink",
      GST_PAD_SINK, GST_PAD_ALWAYS, supported_sinkcaps);

  gst_caps_unref (supported_sinkcaps);

  gst_element_class_add_pad_template (element_class, sink_templ);
  gst_element_class_add_static_pad_template (element_class, &src_factory);

  gst_type_mark_as_plugin_api (GST_X265_ENC_LOG_LEVEL_TYPE, 0);
  gst_type_mark_as_plugin_api (GST_X265_ENC_SPEED_PRESET_TYPE, 0);
  gst_type_mark_as_plugin_api (GST_X265_ENC_TUNE_TYPE,
      GST_PLUGIN_API_FLAG_IGNORE_ENUM_MEMBERS);
}

/* initialize the new element
 * instantiate pads and add them to element
 * set functions
 * initialize structure
 */
static void
gst_x265_enc_init (GstX265Enc * encoder)
{
  encoder->push_header = TRUE;

  encoder->bitrate = PROP_BITRATE_DEFAULT;
  encoder->qp = PROP_QP_DEFAULT;
  encoder->option_string_prop = g_string_new (PROP_OPTION_STRING_DEFAULT);
  encoder->log_level = PROP_LOG_LEVEL_DEFAULT;
  encoder->speed_preset = PROP_SPEED_PRESET_DEFAULT;
  encoder->tune = PROP_TUNE_DEFAULT;
  encoder->keyintmax = PROP_KEY_INT_MAX_DEFAULT;
  encoder->api = &default_vtable;

  encoder->api->param_default (&encoder->x265param);

  encoder->peer_profiles = g_ptr_array_new ();
}

typedef struct
{
  GstVideoCodecFrame *frame;
  GstVideoFrame vframe;
} FrameData;

static FrameData *
gst_x265_enc_queue_frame (GstX265Enc * enc, GstVideoCodecFrame * frame,
    GstVideoInfo * info)
{
  GstVideoFrame vframe;
  FrameData *fdata;

  if (!gst_video_frame_map (&vframe, info, frame->input_buffer, GST_MAP_READ))
    return NULL;

  fdata = g_new (FrameData, 1);
  fdata->frame = gst_video_codec_frame_ref (frame);
  fdata->vframe = vframe;

  enc->pending_frames = g_list_prepend (enc->pending_frames, fdata);

  return fdata;
}

static void
gst_x265_enc_dequeue_frame (GstX265Enc * enc, GstVideoCodecFrame * frame)
{
  GList *l;

  for (l = enc->pending_frames; l; l = l->next) {
    FrameData *fdata = l->data;

    if (fdata->frame != frame)
      continue;

    gst_video_frame_unmap (&fdata->vframe);
    gst_video_codec_frame_unref (fdata->frame);
    g_free (fdata);

    enc->pending_frames = g_list_delete_link (enc->pending_frames, l);
    return;
  }
}

static void
gst_x265_enc_dequeue_all_frames (GstX265Enc * enc)
{
  GList *l;

  for (l = enc->pending_frames; l; l = l->next) {
    FrameData *fdata = l->data;

    gst_video_frame_unmap (&fdata->vframe);
    gst_video_codec_frame_unref (fdata->frame);
    g_free (fdata);
  }
  g_list_free (enc->pending_frames);
  enc->pending_frames = NULL;
}

static gboolean
gst_x265_enc_start (GstVideoEncoder * encoder)
{
  GstX265Enc *x265enc = GST_X265_ENC (encoder);

  g_ptr_array_set_size (x265enc->peer_profiles, 0);

  /* make sure that we have enough time for first DTS,
     this is probably overkill for most streams */
  gst_video_encoder_set_min_pts (encoder, GST_SECOND * 60 * 60 * 1000);

  return TRUE;
}

static gboolean
gst_x265_enc_stop (GstVideoEncoder * encoder)
{
  GstX265Enc *x265enc = GST_X265_ENC (encoder);

  GST_DEBUG_OBJECT (encoder, "stop encoder");

  gst_x265_enc_flush_frames (x265enc, FALSE);
  gst_x265_enc_close_encoder (x265enc);
  gst_x265_enc_dequeue_all_frames (x265enc);

  if (x265enc->input_state)
    gst_video_codec_state_unref (x265enc->input_state);
  x265enc->input_state = NULL;

  g_ptr_array_set_size (x265enc->peer_profiles, 0);

  return TRUE;
}


static gboolean
gst_x265_enc_flush (GstVideoEncoder * encoder)
{
  GstX265Enc *x265enc = GST_X265_ENC (encoder);

  GST_DEBUG_OBJECT (encoder, "flushing encoder");

  gst_x265_enc_flush_frames (x265enc, FALSE);
  gst_x265_enc_close_encoder (x265enc);
  gst_x265_enc_dequeue_all_frames (x265enc);

  gst_x265_enc_init_encoder (x265enc);

  return TRUE;
}

static void
gst_x265_enc_finalize (GObject * object)
{
  GstX265Enc *encoder = GST_X265_ENC (object);

  if (encoder->input_state)
    gst_video_codec_state_unref (encoder->input_state);
  encoder->input_state = NULL;

  gst_x265_enc_close_encoder (encoder);

  g_string_free (encoder->option_string_prop, TRUE);

  if (encoder->peer_profiles)
    g_ptr_array_free (encoder->peer_profiles, FALSE);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gint
gst_x265_enc_gst_to_x265_video_format (GstVideoFormat format, gint * nplanes)
{
  switch (format) {
    case GST_VIDEO_FORMAT_I420:
    case GST_VIDEO_FORMAT_YV12:
    case GST_VIDEO_FORMAT_I420_10LE:
    case GST_VIDEO_FORMAT_I420_10BE:
    case GST_VIDEO_FORMAT_I420_12LE:
    case GST_VIDEO_FORMAT_I420_12BE:
      if (nplanes)
        *nplanes = 3;
      return X265_CSP_I420;
    case GST_VIDEO_FORMAT_Y444:
    case GST_VIDEO_FORMAT_Y444_10LE:
    case GST_VIDEO_FORMAT_Y444_10BE:
    case GST_VIDEO_FORMAT_Y444_12LE:
    case GST_VIDEO_FORMAT_Y444_12BE:
      if (nplanes)
        *nplanes = 3;
      return X265_CSP_I444;
    case GST_VIDEO_FORMAT_Y42B:
    case GST_VIDEO_FORMAT_I422_10LE:
    case GST_VIDEO_FORMAT_I422_10BE:
    case GST_VIDEO_FORMAT_I422_12LE:
    case GST_VIDEO_FORMAT_I422_12BE:
      if (nplanes)
        *nplanes = 3;
      return X265_CSP_I422;
    default:
      g_return_val_if_reached (GST_VIDEO_FORMAT_UNKNOWN);
  }
}

/*
 * gst_x265_enc_parse_options
 * @encoder: Encoder to which options are assigned
 * @str: Option string
 *
 * Parse option string and assign to x265 parameters
 *
 */
static gboolean
gst_x265_enc_parse_options (GstX265Enc * encoder, const gchar * str)
{
  GStrv kvpairs;
  guint npairs, i;
  gint parse_result = 0, ret = 0;
  gchar *options = (gchar *) str;
  const x265_api *api = encoder->api;

  g_assert (api != NULL);

  while (*options == ':')
    options++;

  kvpairs = g_strsplit (options, ":", 0);
  npairs = g_strv_length (kvpairs);

  for (i = 0; i < npairs; i++) {
    GStrv key_val = g_strsplit (kvpairs[i], "=", 2);

    parse_result =
        api->param_parse (&encoder->x265param, key_val[0], key_val[1]);

    if (parse_result == X265_PARAM_BAD_NAME) {
      GST_ERROR_OBJECT (encoder, "Bad name for option %s=%s",
          key_val[0] ? key_val[0] : "", key_val[1] ? key_val[1] : "");
    }
    if (parse_result == X265_PARAM_BAD_VALUE) {
      GST_ERROR_OBJECT (encoder,
          "Bad value for option %s=%s (Note: a NULL value for a non-boolean triggers this)",
          key_val[0] ? key_val[0] : "", key_val[1] ? key_val[1] : "");
    }

    g_strfreev (key_val);

    if (parse_result)
      ret++;
  }

  g_strfreev (kvpairs);
  return !ret;
}

static gboolean
gst_x265_enc_init_encoder_locked (GstX265Enc * encoder)
{
  GstVideoInfo *info;
  guint bitdepth;
  gboolean peer_intra = FALSE;

  if (!encoder->input_state) {
    GST_DEBUG_OBJECT (encoder, "Have no input state yet");
    return FALSE;
  }

  info = &encoder->input_state->info;

  /* make sure that the encoder is closed */
  gst_x265_enc_close_encoder (encoder);

  bitdepth = GST_VIDEO_INFO_COMP_DEPTH (info, 0);
  encoder->api = NULL;

  switch (bitdepth) {
    case 8:
      if (vtable_8bit)
        encoder->api = vtable_8bit;
      else if (vtable_10bit)
        encoder->api = vtable_10bit;
      else
        encoder->api = vtable_12bit;
      break;
    case 10:
      if (vtable_10bit)
        encoder->api = vtable_10bit;
      else
        encoder->api = vtable_12bit;
      break;
    case 12:
      encoder->api = vtable_12bit;
      break;
    default:
      break;
  }

  if (!encoder->api) {
    GST_ERROR_OBJECT (encoder, "no %d bitdepth vtable available", bitdepth);
    return FALSE;
  }

  if (encoder->api->param_default_preset (&encoder->x265param,
          x265_preset_names[encoder->speed_preset - 1],
          x265_tune_names[encoder->tune - 1]) < 0) {
    GST_DEBUG_OBJECT (encoder, "preset or tune unrecognized");
    return FALSE;
  }

  /* set up encoder parameters */
  encoder->x265param.logLevel = encoder->log_level;
  encoder->x265param.internalCsp =
      gst_x265_enc_gst_to_x265_video_format (info->finfo->format, NULL);
  if (info->fps_d == 0 || info->fps_n == 0) {
  } else {
    encoder->x265param.fpsNum = info->fps_n;
    encoder->x265param.fpsDenom = info->fps_d;
  }
  encoder->x265param.sourceWidth = info->width;
  encoder->x265param.sourceHeight = info->height;

  /* x265 does not allow user to configure a picture size smaller than
   * at least one CU size, and maxCUSize must be 16, 32, or 64.
   * Therefore, we should be set the CU size according to the input resolution.
   */
  if (encoder->x265param.sourceWidth < 64
      || encoder->x265param.sourceHeight < 64)
    encoder->x265param.maxCUSize = 32;
  if (encoder->x265param.sourceWidth < 32
      || encoder->x265param.sourceHeight < 32)
    encoder->x265param.maxCUSize = 16;

  if (info->par_d > 0) {
    encoder->x265param.vui.aspectRatioIdc = X265_EXTENDED_SAR;
    encoder->x265param.vui.sarWidth = info->par_n;
    encoder->x265param.vui.sarHeight = info->par_d;
  }

  encoder->x265param.vui.bEnableVideoSignalTypePresentFlag = 1;
  /* Unspecified video format (5) */
  encoder->x265param.vui.videoFormat = 5;
  if (info->colorimetry.range == GST_VIDEO_COLOR_RANGE_0_255) {
    encoder->x265param.vui.bEnableVideoFullRangeFlag = 1;
  } else {
    encoder->x265param.vui.bEnableVideoFullRangeFlag = 0;
  }

  encoder->x265param.vui.bEnableColorDescriptionPresentFlag = 1;
  encoder->x265param.vui.matrixCoeffs =
      gst_video_color_matrix_to_iso (info->colorimetry.matrix);
  encoder->x265param.vui.colorPrimaries =
      gst_video_color_primaries_to_iso (info->colorimetry.primaries);
  encoder->x265param.vui.transferCharacteristics =
      gst_video_transfer_function_to_iso (info->colorimetry.transfer);

  if (encoder->qp != -1) {
    /* CQP */
    encoder->x265param.rc.qp = encoder->qp;
    encoder->x265param.rc.rateControlMode = X265_RC_CQP;
  } else {
    /* ABR */
    encoder->x265param.rc.bitrate = encoder->bitrate;
    encoder->x265param.rc.rateControlMode = X265_RC_ABR;
  }

  if (encoder->peer_profiles->len > 0) {
    gint i;

    for (i = 0; i < encoder->peer_profiles->len; i++) {
      const gchar *profile = g_ptr_array_index (encoder->peer_profiles, i);

      GST_DEBUG_OBJECT (encoder, "Apply peer profile %s", profile);
      if (encoder->api->param_apply_profile (&encoder->x265param, profile) < 0) {
        GST_WARNING_OBJECT (encoder, "Failed to apply profile %s", profile);
      } else {
        /* libx265 chooses still-picture profile only if x265_param::totalFrames
         * equals to one (otherwise, -intra profile will be chosen) */
        if (g_strrstr (profile, "stillpicture"))
          encoder->x265param.totalFrames = 1;

        if (g_str_has_suffix (profile, "-intra"))
          peer_intra = TRUE;

        break;
      }
    }

    if (i == encoder->peer_profiles->len) {
      GST_ERROR_OBJECT (encoder, "Couldn't apply peer profile");

      return FALSE;
    }
  }

  if (peer_intra) {
    encoder->x265param.keyframeMax = 1;
  } else if (encoder->keyintmax > 0) {
    encoder->x265param.keyframeMax = encoder->keyintmax;
  }
#if (X265_BUILD >= 79)
  {
    GstVideoMasteringDisplayInfo minfo;
    GstVideoContentLightLevel cll;

    if (gst_video_mastering_display_info_from_caps (&minfo,
            encoder->input_state->caps)) {
      GST_DEBUG_OBJECT (encoder, "Apply mastering display info");

      /* GstVideoMasteringDisplayInfo::display_primaries is rgb order but
       * HEVC uses gbr order
       * See spec D.3.28 display_primaries_x and display_primaries_y
       */
      encoder->x265param.masteringDisplayColorVolume =
          g_strdup_printf ("G(%hu,%hu)B(%hu,%hu)R(%hu,%hu)WP(%hu,%hu)L(%u,%u)",
          minfo.display_primaries[1].x, minfo.display_primaries[1].y,
          minfo.display_primaries[2].x, minfo.display_primaries[2].y,
          minfo.display_primaries[0].x, minfo.display_primaries[0].y,
          minfo.white_point.x, minfo.white_point.y,
          minfo.max_display_mastering_luminance,
          minfo.min_display_mastering_luminance);
    }

    if (gst_video_content_light_level_from_caps (&cll,
            encoder->input_state->caps)) {
      GST_DEBUG_OBJECT (encoder, "Apply content light level");

      encoder->x265param.maxCLL = cll.max_content_light_level;
      encoder->x265param.maxFALL = cll.max_frame_average_light_level;
    }
  }
#endif

  /* apply option-string property */
  if (encoder->option_string_prop && encoder->option_string_prop->len) {
    GST_DEBUG_OBJECT (encoder, "Applying option-string: %s",
        encoder->option_string_prop->str);
    if (gst_x265_enc_parse_options (encoder,
            encoder->option_string_prop->str) == FALSE) {
      GST_DEBUG_OBJECT (encoder, "Your option-string contains errors.");
      return FALSE;
    }
  }

  encoder->reconfig = FALSE;

  /* good start, will be corrected if needed */
  encoder->dts_offset = 0;

  encoder->x265enc = encoder->api->encoder_open (&encoder->x265param);
  if (!encoder->x265enc) {
    GST_ERROR_OBJECT (encoder, "Can not open x265 encoder.");
    return FALSE;
  }

  encoder->push_header = TRUE;

  return TRUE;
}

/*
 * gst_x265_enc_init_encoder
 * @encoder:  Encoder which should be initialized.
 *
 * Initialize x265 encoder.
 *
 */
static gboolean
gst_x265_enc_init_encoder (GstX265Enc * encoder)
{
  gboolean result;

  GST_OBJECT_LOCK (encoder);
  result = gst_x265_enc_init_encoder_locked (encoder);
  GST_OBJECT_UNLOCK (encoder);

  if (!result)
    GST_ELEMENT_ERROR (encoder, STREAM, ENCODE,
        ("Can not initialize x265 encoder."), (NULL));

  return result;
}

/* gst_x265_enc_close_encoder
 * @encoder:  Encoder which should close.
 *
 * Close x265 encoder.
 */
static void
gst_x265_enc_close_encoder (GstX265Enc * encoder)
{
  if (encoder->x265enc != NULL) {
    g_assert (encoder->api != NULL);

    encoder->api->encoder_close (encoder->x265enc);
    encoder->x265enc = NULL;
  }
}

static x265_nal *
gst_x265_enc_bytestream_to_nal (x265_nal * input)
{
  x265_nal *output;
  int i, j, zeros;

  output = g_malloc (sizeof (x265_nal));
  output->payload = g_malloc (input->sizeBytes - 4);
  output->sizeBytes = input->sizeBytes - 4;
  output->type = input->type;

  zeros = 0;
  for (i = 4, j = 0; i < input->sizeBytes; (i++, j++)) {
    if (input->payload[i] == 0x00) {
      zeros++;
    } else if (input->payload[i] == 0x03 && zeros == 2) {
      zeros = 0;
      j--;
      output->sizeBytes--;
      continue;
    } else {
      zeros = 0;
    }
    output->payload[j] = input->payload[i];
  }

  return output;
}

static void
x265_nal_free (x265_nal * nal)
{
  g_free (nal->payload);
  g_free (nal);
}

static gboolean
gst_x265_enc_set_level_tier_and_profile (GstX265Enc * encoder, GstCaps * caps)
{
  x265_nal *nal, *vps_nal;
  guint32 i_nal;
  int header_return;
  const x265_api *api = encoder->api;
  GstStructure *s;
  const gchar *profile;
  GstCaps *allowed_caps;
  GstStructure *s2;
  const gchar *allowed_profile;

  GST_DEBUG_OBJECT (encoder, "set profile, level and tier");

  g_assert (api != NULL);

  header_return = api->encoder_headers (encoder->x265enc, &nal, &i_nal);
  if (header_return < 0) {
    GST_ELEMENT_ERROR (encoder, STREAM, ENCODE, ("Encode x265 header failed."),
        ("x265_encoder_headers return code=%d", header_return));
    return FALSE;
  }

  GST_DEBUG_OBJECT (encoder, "%d nal units in header", i_nal);

  g_assert (nal[0].type == NAL_UNIT_VPS);
  vps_nal = gst_x265_enc_bytestream_to_nal (&nal[0]);

  GST_MEMDUMP ("VPS", vps_nal->payload, vps_nal->sizeBytes);

  gst_codec_utils_h265_caps_set_level_tier_and_profile (caps,
      vps_nal->payload + 6, vps_nal->sizeBytes - 6);
  x265_nal_free (vps_nal);

  /* relaxing the profile condition since libx265 can select lower profile than
   * requested one via param_apply_profile()
   */
  s = gst_caps_get_structure (caps, 0);
  profile = gst_structure_get_string (s, "profile");

  allowed_caps = gst_pad_get_allowed_caps (GST_VIDEO_ENCODER_SRC_PAD (encoder));

  if (allowed_caps == NULL)
    goto no_peer;

  if (!gst_caps_can_intersect (allowed_caps, caps)) {
    guint peer_bitdepth = 0;
    guint peer_chroma_format = 0;
    guint bitdepth = 0;
    guint chroma_format = 0;

    allowed_caps = gst_caps_make_writable (allowed_caps);
    allowed_caps = gst_caps_truncate (allowed_caps);
    s2 = gst_caps_get_structure (allowed_caps, 0);
    gst_structure_fixate_field_string (s2, "profile", profile);
    allowed_profile = gst_structure_get_string (s2, "profile");

    check_formats (allowed_profile, &peer_chroma_format, &peer_bitdepth);
    check_formats (profile, &chroma_format, &bitdepth);

    if (chroma_format <= peer_chroma_format && bitdepth <= peer_bitdepth) {
      GST_INFO_OBJECT (encoder, "downstream requested %s profile, but "
          "encoder will now output %s profile (which is a subset), due "
          "to how it's been configured", allowed_profile, profile);
      gst_structure_set (s, "profile", G_TYPE_STRING, allowed_profile, NULL);
    }
  }
  gst_caps_unref (allowed_caps);

no_peer:

  return TRUE;
}

static GstBuffer *
gst_x265_enc_get_header_buffer (GstX265Enc * encoder)
{
  x265_nal *nal;
  guint32 i_nal, i, offset;
  gint32 vps_idx, sps_idx, pps_idx;
  int header_return;
  GstBuffer *buf;
  gsize header_size = 0;
  const x265_api *api = encoder->api;

  g_assert (api != NULL);

  header_return = api->encoder_headers (encoder->x265enc, &nal, &i_nal);
  if (header_return < 0) {
    GST_ELEMENT_ERROR (encoder, STREAM, ENCODE, ("Encode x265 header failed."),
        ("x265_encoder_headers return code=%d", header_return));
    return FALSE;
  }

  GST_DEBUG_OBJECT (encoder, "%d nal units in header", i_nal);

  /* x265 returns also non header nal units with the call x265_encoder_headers.
   * The useful headers are sequential (VPS, SPS and PPS), so we look for this
   * nal units and only copy these tree nal units as the header */

  vps_idx = sps_idx = pps_idx = -1;
  for (i = 0; i < i_nal; i++) {
    if (nal[i].type == NAL_UNIT_VPS) {
      vps_idx = i;
      header_size += nal[i].sizeBytes;
    } else if (nal[i].type == NAL_UNIT_SPS) {
      sps_idx = i;
      header_size += nal[i].sizeBytes;
    } else if (nal[i].type == NAL_UNIT_PPS) {
      pps_idx = i;
      header_size += nal[i].sizeBytes;
    } else if (nal[i].type == NAL_UNIT_PREFIX_SEI) {
      header_size += nal[i].sizeBytes;
    }
  }

  if (vps_idx == -1 || sps_idx == -1 || pps_idx == -1) {
    GST_ELEMENT_ERROR (encoder, STREAM, ENCODE, ("Encode x265 header failed."),
        ("x265_encoder_headers did not return VPS, SPS and PPS"));
    return FALSE;
  }

  offset = 0;
  buf = gst_buffer_new_allocate (NULL, header_size, NULL);
  gst_buffer_fill (buf, offset, nal[vps_idx].payload, nal[vps_idx].sizeBytes);
  offset += nal[vps_idx].sizeBytes;
  gst_buffer_fill (buf, offset, nal[sps_idx].payload, nal[sps_idx].sizeBytes);
  offset += nal[sps_idx].sizeBytes;
  gst_buffer_fill (buf, offset, nal[pps_idx].payload, nal[pps_idx].sizeBytes);
  offset += nal[pps_idx].sizeBytes;

  for (i = 0; i < i_nal; i++) {
    if (nal[i].type == NAL_UNIT_PREFIX_SEI) {
      gst_buffer_fill (buf, offset, nal[i].payload, nal[i].sizeBytes);
      offset += nal[i].sizeBytes;
    }
  }

  return buf;
}

/* gst_x265_enc_set_src_caps
 * Returns: TRUE on success.
 */
static gboolean
gst_x265_enc_set_src_caps (GstX265Enc * encoder, GstCaps * caps)
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

  if (!gst_x265_enc_set_level_tier_and_profile (encoder, outcaps)) {
    gst_caps_unref (outcaps);
    return FALSE;
  }

  state = gst_video_encoder_set_output_state (GST_VIDEO_ENCODER (encoder),
      outcaps, encoder->input_state);
  GST_DEBUG_OBJECT (encoder, "output caps: %" GST_PTR_FORMAT, state->caps);
  gst_video_codec_state_unref (state);

  tags = gst_tag_list_new_empty ();
  gst_tag_list_add (tags, GST_TAG_MERGE_REPLACE, GST_TAG_ENCODER, "x265",
      GST_TAG_ENCODER_VERSION, x265_version_str, NULL);
  gst_video_encoder_merge_tags (GST_VIDEO_ENCODER (encoder), tags,
      GST_TAG_MERGE_REPLACE);
  gst_tag_list_unref (tags);

  return TRUE;
}

static void
gst_x265_enc_set_latency (GstX265Enc * encoder)
{
  GstVideoInfo *info = &encoder->input_state->info;
  gint max_delayed_frames;
  GstClockTime latency;

  /* FIXME get a real value from the encoder, this is currently not exposed */
  if (encoder->tune > 0 && encoder->tune <= G_N_ELEMENTS (x265_tune_names) &&
      strcmp (x265_tune_names[encoder->tune - 1], "zerolatency") == 0)
    max_delayed_frames = 0;
  else
    max_delayed_frames = 5;

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

typedef struct
{
  const gchar *gst_profile;
  const gchar *x265_profile;
} GstX265EncProfileTable;

static const gchar *
gst_x265_enc_profile_from_gst (const gchar * profile)
{
  gint i;
  static const GstX265EncProfileTable profile_table[] = {
    /* 8 bits */
    {"main", "main"},
    {"main-still-picture", "mainstillpicture"},
    {"main-intra", "main-intra"},
    {"main-444", "main444-8"},
    {"main-444-intra", "main444-intra"},
    {"main-444-still-picture", "main444-stillpicture"},
    /* 10 bits */
    {"main-10", "main10"},
    {"main-10-intra", "main10-intra"},
    {"main-422-10", "main422-10"},
    {"main-422-10-intra", "main422-10-intra"},
    {"main-444-10", "main444-10"},
    {"main-444-10-intra", "main444-10-intra"},
    /* 12 bits */
    {"main-12", "main12"},
    {"main-12-intra", "main12-intra"},
    {"main-422-12", "main422-12"},
    {"main-422-12-intra", "main422-12-intra"},
    {"main-444-12", "main444-12"},
    {"main-444-12-intra", "main444-12-intra"},
  };

  if (!profile)
    return NULL;

  for (i = 0; i < G_N_ELEMENTS (profile_table); i++) {
    if (!strcmp (profile, profile_table[i].gst_profile))
      return profile_table[i].x265_profile;
  }

  return NULL;
}

static gboolean
gst_x265_enc_set_format (GstVideoEncoder * video_enc,
    GstVideoCodecState * state)
{
  GstX265Enc *encoder = GST_X265_ENC (video_enc);
  GstVideoInfo *info = &state->info;
  GstCaps *template_caps;
  GstCaps *allowed_caps = NULL;

  /* If the encoder is initialized, do not reinitialize it again if not
   * necessary */
  if (encoder->x265enc) {
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
    gst_x265_enc_flush_frames (encoder, TRUE);
  }

  if (encoder->input_state)
    gst_video_codec_state_unref (encoder->input_state);
  encoder->input_state = gst_video_codec_state_ref (state);

  g_ptr_array_set_size (encoder->peer_profiles, 0);

  template_caps = gst_static_pad_template_get_caps (&src_factory);
  allowed_caps = gst_pad_get_allowed_caps (GST_VIDEO_ENCODER_SRC_PAD (encoder));

  GST_DEBUG_OBJECT (encoder, "allowed caps %" GST_PTR_FORMAT, allowed_caps);

  /* allowed != template is meaning that downstream has some restriction
   * so we need check whether there is requested profile or not */
  if (allowed_caps && !gst_caps_is_equal (allowed_caps, template_caps)) {
    gint i, j;

    if (gst_caps_is_empty (allowed_caps)) {
      gst_caps_unref (allowed_caps);
      gst_caps_unref (template_caps);
      return FALSE;
    }

    for (i = 0; i < gst_caps_get_size (allowed_caps); i++) {
      GstStructure *s;
      const GValue *val;
      const gchar *profile;
      const gchar *x265_profile;

      s = gst_caps_get_structure (allowed_caps, i);

      if ((val = gst_structure_get_value (s, "profile"))) {
        if (G_VALUE_HOLDS_STRING (val)) {
          profile = g_value_get_string (val);
          x265_profile = gst_x265_enc_profile_from_gst (profile);

          if (x265_profile) {
            GST_DEBUG_OBJECT (encoder,
                "Add profile %s to peer profile list", x265_profile);

            g_ptr_array_add (encoder->peer_profiles, (gpointer) x265_profile);
          }
        } else if (GST_VALUE_HOLDS_LIST (val)) {
          for (j = 0; j < gst_value_list_get_size (val); j++) {
            const GValue *vlist = gst_value_list_get_value (val, j);
            profile = g_value_get_string (vlist);
            x265_profile = gst_x265_enc_profile_from_gst (profile);

            if (x265_profile) {
              GST_DEBUG_OBJECT (encoder,
                  "Add profile %s to peer profile list", x265_profile);

              g_ptr_array_add (encoder->peer_profiles, (gpointer) x265_profile);
            }
          }
        }
      }
    }
  }

  gst_clear_caps (&allowed_caps);
  gst_caps_unref (template_caps);


  if (!gst_x265_enc_init_encoder (encoder))
    return FALSE;

  if (!gst_x265_enc_set_src_caps (encoder, state->caps)) {
    gst_x265_enc_close_encoder (encoder);
    return FALSE;
  }

  gst_x265_enc_set_latency (encoder);

  return TRUE;
}

static GstFlowReturn
gst_x265_enc_finish (GstVideoEncoder * encoder)
{
  GST_DEBUG_OBJECT (encoder, "finish encoder");

  gst_x265_enc_flush_frames (GST_X265_ENC (encoder), TRUE);
  gst_x265_enc_flush_frames (GST_X265_ENC (encoder), TRUE);
  return GST_FLOW_OK;
}

static gboolean
gst_x265_enc_propose_allocation (GstVideoEncoder * encoder, GstQuery * query)
{
  gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, NULL);

  return GST_VIDEO_ENCODER_CLASS (parent_class)->propose_allocation (encoder,
      query);
}

/* chain function
 * this function does the actual processing
 */
static GstFlowReturn
gst_x265_enc_handle_frame (GstVideoEncoder * video_enc,
    GstVideoCodecFrame * frame)
{
  GstX265Enc *encoder = GST_X265_ENC (video_enc);
  GstVideoInfo *info = &encoder->input_state->info;
  GstFlowReturn ret;
  x265_picture pic_in;
  guint32 i_nal, i;
  FrameData *fdata;
  gint nplanes = 0;
  const x265_api *api = encoder->api;

  g_assert (api != NULL);

  if (G_UNLIKELY (encoder->x265enc == NULL))
    goto not_inited;

  /* set up input picture */
  api->picture_init (&encoder->x265param, &pic_in);

  fdata = gst_x265_enc_queue_frame (encoder, frame, info);
  if (!fdata)
    goto invalid_frame;

  pic_in.colorSpace =
      gst_x265_enc_gst_to_x265_video_format (info->finfo->format, &nplanes);
  for (i = 0; i < nplanes; i++) {
    pic_in.planes[i] = GST_VIDEO_FRAME_PLANE_DATA (&fdata->vframe, i);
    pic_in.stride[i] = GST_VIDEO_FRAME_COMP_STRIDE (&fdata->vframe, i);
  }

  pic_in.sliceType = X265_TYPE_AUTO;
  pic_in.pts = frame->pts;
  pic_in.dts = frame->dts;
  pic_in.bitDepth = info->finfo->depth[0];
  pic_in.userData = GINT_TO_POINTER (frame->system_frame_number);

  ret = gst_x265_enc_encode_frame (encoder, &pic_in, frame, &i_nal, TRUE);

  /* input buffer is released later on */
  return ret;

/* ERRORS */
not_inited:
  {
    GST_WARNING_OBJECT (encoder, "Got buffer before set_caps was called");
    return GST_FLOW_NOT_NEGOTIATED;
  }
invalid_frame:
  {
    GST_ERROR_OBJECT (encoder, "Failed to map frame");
    return GST_FLOW_ERROR;
  }
}

static GstFlowReturn
gst_x265_enc_encode_frame (GstX265Enc * encoder, x265_picture * pic_in,
    GstVideoCodecFrame * input_frame, guint32 * i_nal, gboolean send)
{
  GstVideoCodecFrame *frame = NULL;
  GstBuffer *out_buf = NULL;
  x265_picture pic_out;
  x265_nal *nal;
  int i_size, i, offset;
  int encoder_return;
  GstFlowReturn ret = GST_FLOW_OK;
  gboolean update_latency = FALSE;
  const x265_api *api;

  if (G_UNLIKELY (encoder->x265enc == NULL)) {
    if (input_frame)
      gst_video_codec_frame_unref (input_frame);
    return GST_FLOW_NOT_NEGOTIATED;
  }

  api = encoder->api;
  g_assert (api != NULL);

  GST_OBJECT_LOCK (encoder);
  if (encoder->reconfig) {
    /* x265_encoder_reconfig is not yet implemented thus we shut down and re-create encoder */
    gst_x265_enc_init_encoder_locked (encoder);
    update_latency = TRUE;
  }

  if (pic_in && input_frame) {
    if (GST_VIDEO_CODEC_FRAME_IS_FORCE_KEYFRAME (input_frame)) {
      GST_INFO_OBJECT (encoder, "Forcing key frame");
      pic_in->sliceType = X265_TYPE_IDR;
    }
  }
  GST_OBJECT_UNLOCK (encoder);

  if (G_UNLIKELY (update_latency))
    gst_x265_enc_set_latency (encoder);

  encoder_return = api->encoder_encode (encoder->x265enc,
      &nal, i_nal, pic_in, &pic_out);

  GST_DEBUG_OBJECT (encoder, "encoder result (%d) with %u nal units",
      encoder_return, *i_nal);

  if (encoder_return < 0) {
    GST_ELEMENT_ERROR (encoder, STREAM, ENCODE, ("Encode x265 frame failed."),
        ("x265_encoder_encode return code=%d", encoder_return));
    ret = GST_FLOW_ERROR;
    /* Make sure we finish this frame */
    frame = input_frame;
    goto out;
  }

  /* Input frame is now queued */
  if (input_frame)
    gst_video_codec_frame_unref (input_frame);

  if (!*i_nal) {
    ret = GST_FLOW_OK;
    GST_LOG_OBJECT (encoder, "no output yet");
    goto out;
  }

  frame = gst_video_encoder_get_frame (GST_VIDEO_ENCODER (encoder),
      GPOINTER_TO_INT (pic_out.userData));
  g_assert (frame || !send);

  GST_DEBUG_OBJECT (encoder,
      "output picture ready POC=%d system=%d frame found %d", pic_out.poc,
      GPOINTER_TO_INT (pic_out.userData), frame != NULL);

  if (!send || !frame) {
    GST_LOG_OBJECT (encoder, "not sending (%d) or frame not found (%d)", send,
        frame != NULL);
    ret = GST_FLOW_OK;
    goto out;
  }

  i_size = 0;
  offset = 0;
  for (i = 0; i < *i_nal; i++)
    i_size += nal[i].sizeBytes;
  out_buf = gst_buffer_new_allocate (NULL, i_size, NULL);
  for (i = 0; i < *i_nal; i++) {
    gst_buffer_fill (out_buf, offset, nal[i].payload, nal[i].sizeBytes);
    offset += nal[i].sizeBytes;
  }

  if (pic_out.sliceType == X265_TYPE_IDR || pic_out.sliceType == X265_TYPE_I) {
    GST_VIDEO_CODEC_FRAME_SET_SYNC_POINT (frame);
  }

  frame->output_buffer = out_buf;

  if (encoder->push_header) {
    GstBuffer *header;

    header = gst_x265_enc_get_header_buffer (encoder);
    frame->output_buffer = gst_buffer_append (header, frame->output_buffer);
    encoder->push_header = FALSE;
  }

  GST_LOG_OBJECT (encoder,
      "output: dts %" G_GINT64_FORMAT " pts %" G_GINT64_FORMAT,
      (gint64) pic_out.dts, (gint64) pic_out.pts);

  frame->dts = pic_out.dts + encoder->dts_offset;

out:
  if (frame) {
    gst_x265_enc_dequeue_frame (encoder, frame);
    ret = gst_video_encoder_finish_frame (GST_VIDEO_ENCODER (encoder), frame);
  }

  return ret;
}

static void
gst_x265_enc_flush_frames (GstX265Enc * encoder, gboolean send)
{
  GstFlowReturn flow_ret;
  guint32 i_nal;

  /* first send the remaining frames */
  if (encoder->x265enc)
    do {
      flow_ret = gst_x265_enc_encode_frame (encoder, NULL, NULL, &i_nal, send);
    } while (flow_ret == GST_FLOW_OK && i_nal > 0);
}

static void
gst_x265_enc_reconfig (GstX265Enc * encoder)
{
  encoder->x265param.rc.bitrate = encoder->bitrate;
  encoder->reconfig = TRUE;
}

static void
gst_x265_enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstX265Enc *encoder;
  GstState state;

  encoder = GST_X265_ENC (object);

  GST_OBJECT_LOCK (encoder);

  state = GST_STATE (encoder);
  if ((state != GST_STATE_READY && state != GST_STATE_NULL) &&
      !(pspec->flags & GST_PARAM_MUTABLE_PLAYING))
    goto wrong_state;

  switch (prop_id) {
    case PROP_BITRATE:
      encoder->bitrate = g_value_get_uint (value);
      break;
    case PROP_QP:
      encoder->qp = g_value_get_int (value);
      break;
    case PROP_OPTION_STRING:
      g_string_assign (encoder->option_string_prop, g_value_get_string (value));
      break;
    case PROP_X265_LOG_LEVEL:
      encoder->log_level = g_value_get_enum (value);
      break;
    case PROP_SPEED_PRESET:
      encoder->speed_preset = g_value_get_enum (value);
      break;
    case PROP_TUNE:
      encoder->tune = g_value_get_enum (value);
      break;
    case PROP_KEY_INT_MAX:
      encoder->keyintmax = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  gst_x265_enc_reconfig (encoder);
  GST_OBJECT_UNLOCK (encoder);
  return;

wrong_state:
  {
    GST_WARNING_OBJECT (encoder, "setting property in wrong state");
    GST_OBJECT_UNLOCK (encoder);
  }
}

static void
gst_x265_enc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstX265Enc *encoder;

  encoder = GST_X265_ENC (object);

  GST_OBJECT_LOCK (encoder);
  switch (prop_id) {
    case PROP_BITRATE:
      g_value_set_uint (value, encoder->bitrate);
      break;
    case PROP_QP:
      g_value_set_int (value, encoder->qp);
      break;
    case PROP_OPTION_STRING:
      g_value_set_string (value, encoder->option_string_prop->str);
      break;
    case PROP_X265_LOG_LEVEL:
      g_value_set_enum (value, encoder->log_level);
      break;
    case PROP_SPEED_PRESET:
      g_value_set_enum (value, encoder->speed_preset);
      break;
    case PROP_TUNE:
      g_value_set_enum (value, encoder->tune);
      break;
    case PROP_KEY_INT_MAX:
      g_value_set_int (value, encoder->keyintmax);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (encoder);
}

static gboolean
x265enc_element_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (x265_enc_debug, "x265enc", 0,
      "h265 encoding element");

  GST_INFO ("x265 build: %u", X265_BUILD);

  default_vtable = *x265_api_get (0);

  GST_INFO ("x265 default bitdepth: %u", default_vtable.bit_depth);

  switch (default_vtable.bit_depth) {
    case 8:
      vtable_8bit = &default_vtable;
      break;
    case 10:
      vtable_10bit = &default_vtable;
      break;
    case 12:
      vtable_12bit = &default_vtable;
      break;
    default:
      GST_WARNING ("Unknown default bitdepth %d", default_vtable.bit_depth);
      break;
  }

  if (!vtable_8bit && (vtable_8bit = x265_api_get (8)))
    GST_INFO ("x265 8bit api available");

  if (!vtable_10bit && (vtable_10bit = x265_api_get (10)))
    GST_INFO ("x265 10bit api available");

#if (X265_BUILD >= 68)
  if (!vtable_12bit && (vtable_12bit = x265_api_get (12)))
    GST_INFO ("x265 12bit api available");
#endif

  return gst_element_register (plugin, "x265enc",
      GST_RANK_PRIMARY, GST_TYPE_X265_ENC);
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  return GST_ELEMENT_REGISTER (x265enc, plugin);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    x265,
    "x265-based H265 plugins",
    plugin_init, VERSION, "GPL", GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
