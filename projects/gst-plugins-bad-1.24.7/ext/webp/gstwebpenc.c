/* GStreamer
 * Copyright (C) <2014> Sreerenj Balachandran <sreerenjb@gnome.org>
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

#include <string.h>

#include <gst/video/video.h>
#include <gst/video/gstvideometa.h>

#include "gstwebpenc.h"

#define GST_CAT_DEFAULT webpenc_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

enum
{
  PROP_0,
  PROP_LOSSLESS,
  PROP_QUALITY,
  PROP_SPEED,
  PROP_PRESET,
  PROP_ANIMATED,
  PROP_ANIMATION_LOOPS,
  PROP_ANIMATION_BACKGROUND_COLOR
};

#define DEFAULT_LOSSLESS FALSE
#define DEFAULT_QUALITY 90
#define DEFAULT_SPEED 4
#define DEFAULT_PRESET WEBP_PRESET_PHOTO
#define DEFAULT_ANIMATED FALSE
#define DEFAULT_ANIMATION_LOOPS 0
#define DEFAULT_ANIMATION_BACKGROUND_COLOR 0

static void gst_webp_enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_webp_enc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static gboolean gst_webp_enc_start (GstVideoEncoder * encoder);
static gboolean gst_webp_enc_stop (GstVideoEncoder * encoder);
static gboolean gst_webp_enc_set_format (GstVideoEncoder * encoder,
    GstVideoCodecState * state);
static GstFlowReturn gst_webp_enc_handle_frame (GstVideoEncoder * encoder,
    GstVideoCodecFrame * frame);
static gboolean gst_webp_enc_propose_allocation (GstVideoEncoder * encoder,
    GstQuery * query);
static GstFlowReturn gst_webp_enc_finish (GstVideoEncoder * encoder);

static GstStaticPadTemplate webp_enc_sink_factory =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE ("{ I420, YV12, RGB, RGBA}"))
    );
static GstStaticPadTemplate webp_enc_src_factory =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("image/webp, "
        "framerate = (fraction) [0/1, MAX], "
        "width = (int) [ 16, 16383 ], " "height = (int) [ 16, 16383 ]")
    );

enum
{
  GST_WEBP_PRESET_DEFAULT,
  GST_WEBP_PRESET_PICTURE,
  GST_WEBP_PRESET_PHOTO,
  GST_WEBP_PRESET_DRAWING,
  GST_WEBP_PRESET_ICON,
  GST_WEBP_PREET_TEXT
};

static const GEnumValue preset_values[] = {
  {GST_WEBP_PRESET_DEFAULT, "Default", "none"},
  {GST_WEBP_PRESET_PICTURE, "Digital picture,inner shot", "picture"},
  {GST_WEBP_PRESET_PHOTO, "Outdoor photo, natural lighting", "photo"},
  {GST_WEBP_PRESET_DRAWING, "Hand or Line drawing", "drawing"},
  {GST_WEBP_PRESET_ICON, "Small-sized colorful images", "icon"},
  {GST_WEBP_PREET_TEXT, "text-like", "text"},
  {0, NULL, NULL},
};

#define GST_WEBP_ENC_PRESET_TYPE (gst_webp_enc_preset_get_type())
static GType
gst_webp_enc_preset_get_type (void)
{
  static GType preset_type = 0;

  if (!preset_type) {
    preset_type = g_enum_register_static ("GstWebpEncPreset", preset_values);
  }
  return preset_type;
}

#define gst_webp_enc_parent_class parent_class
G_DEFINE_TYPE (GstWebpEnc, gst_webp_enc, GST_TYPE_VIDEO_ENCODER);
GST_ELEMENT_REGISTER_DEFINE (webpenc, "webpenc",
    GST_RANK_PRIMARY, GST_TYPE_WEBP_ENC);

static void
gst_webp_enc_class_init (GstWebpEncClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *element_class;
  GstVideoEncoderClass *venc_class;

  gobject_class = (GObjectClass *) klass;
  element_class = (GstElementClass *) klass;
  venc_class = (GstVideoEncoderClass *) klass;

  parent_class = g_type_class_peek_parent (klass);

  gobject_class->set_property = gst_webp_enc_set_property;
  gobject_class->get_property = gst_webp_enc_get_property;
  gst_element_class_add_static_pad_template (element_class,
      &webp_enc_sink_factory);
  gst_element_class_add_static_pad_template (element_class,
      &webp_enc_src_factory);
  gst_element_class_set_static_metadata (element_class, "WEBP image encoder",
      "Codec/Encoder/Image", "Encode images in WEBP format",
      "Sreerenj Balachandran <sreerenjb@gnome.org>");

  venc_class->start = gst_webp_enc_start;
  venc_class->finish = gst_webp_enc_finish;
  venc_class->stop = gst_webp_enc_stop;
  venc_class->set_format = gst_webp_enc_set_format;
  venc_class->handle_frame = gst_webp_enc_handle_frame;
  venc_class->propose_allocation = gst_webp_enc_propose_allocation;

  g_object_class_install_property (gobject_class, PROP_LOSSLESS,
      g_param_spec_boolean ("lossless", "Lossless",
          "Enable lossless encoding",
          DEFAULT_LOSSLESS, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_QUALITY,
      g_param_spec_float ("quality", "quality-level",
          "quality level, between 0 (smallest file) and 100 (biggest)",
          0, 100, DEFAULT_QUALITY, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_SPEED,
      g_param_spec_uint ("speed", "Compression Method",
          "quality/speed trade-off (0=fast, 6=slower-better)",
          0, 6, DEFAULT_SPEED, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_PRESET,
      g_param_spec_enum ("preset", "preset tuning",
          "Preset name for visual tuning",
          GST_WEBP_ENC_PRESET_TYPE, DEFAULT_PRESET,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstWebpEnc:animated:
   *
   * Encode an animated webp, instead of several pictures.
   *
   * Since: 1.24
   */
  g_object_class_install_property (gobject_class, PROP_ANIMATED,
      g_param_spec_boolean ("animated", "Animated",
          "Encode an animated webp, instead of several pictures",
          DEFAULT_ANIMATED, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstWebpEnc:animation-loops:
   *
   * The number of animation loops for the animated mode.
   * If set to 0, the animation will loop forever.
   *
   * Since: 1.24
   */
  g_object_class_install_property (gobject_class, PROP_ANIMATION_LOOPS,
      g_param_spec_uint ("animation-loops", "Animation Loops",
          "The number of animation loops for the animated mode. "
          "If set to 0, the animation will loop forever.", 0, G_MAXUINT,
          DEFAULT_ANIMATION_LOOPS, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstWebpEnc:animation-background-color:
   *
   * The animation background color in ARGB order (1 byte per component).
   *
   * Since: 1.24
   */
  g_object_class_install_property (gobject_class,
      PROP_ANIMATION_BACKGROUND_COLOR,
      g_param_spec_uint ("animation-background-color",
          "Animation Background Color",
          "The animation background color in ARGB order (1 byte per component).",
          0, G_MAXUINT32, DEFAULT_ANIMATION_BACKGROUND_COLOR,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  GST_DEBUG_CATEGORY_INIT (webpenc_debug, "webpenc", 0,
      "WEBP encoding element");

  gst_type_mark_as_plugin_api (GST_WEBP_ENC_PRESET_TYPE, 0);
}

static void
gst_webp_enc_init (GstWebpEnc * webpenc)
{
  GST_PAD_SET_ACCEPT_TEMPLATE (GST_VIDEO_ENCODER_SINK_PAD (webpenc));

  webpenc->lossless = DEFAULT_LOSSLESS;
  webpenc->quality = DEFAULT_QUALITY;
  webpenc->speed = DEFAULT_SPEED;
  webpenc->preset = DEFAULT_PRESET;
  webpenc->animated = DEFAULT_ANIMATED;
  webpenc->animation_loops = DEFAULT_ANIMATION_LOOPS;
  webpenc->animation_background_color = DEFAULT_ANIMATION_BACKGROUND_COLOR;

  webpenc->use_argb = FALSE;
  webpenc->rgb_format = GST_VIDEO_FORMAT_UNKNOWN;
}

static gboolean
gst_webp_enc_set_format (GstVideoEncoder * encoder, GstVideoCodecState * state)
{
  GstWebpEnc *enc = GST_WEBP_ENC (encoder);
  GstVideoCodecState *output_state;
  GstVideoInfo *info = &state->info;
  GstVideoFormat format = GST_VIDEO_INFO_FORMAT (info);
  gint width = GST_VIDEO_INFO_WIDTH (info);
  gint height = GST_VIDEO_INFO_HEIGHT (info);

  if (GST_VIDEO_INFO_IS_YUV (info)) {
    switch (format) {
      case GST_VIDEO_FORMAT_I420:
      case GST_VIDEO_FORMAT_YV12:
        enc->webp_color_space = WEBP_YUV420;
        break;
      default:
        GST_ERROR_OBJECT (enc, "Invalid color format");
        return FALSE;
    }
  } else if (GST_VIDEO_INFO_IS_RGB (info)) {
    enc->rgb_format = format;
    enc->use_argb = 1;
  } else {
    GST_ERROR_OBJECT (enc, "Invalid color format");
    return FALSE;
  }

  if (enc->input_state) {
    if (enc->anim_enc) {
      gint prev_width = GST_VIDEO_INFO_WIDTH (&enc->input_state->info);
      gint prev_height = GST_VIDEO_INFO_HEIGHT (&enc->input_state->info);

      if (prev_width != width || prev_height != height) {
        GST_ERROR_OBJECT (enc, "Image size is changing in animation mode");
        return FALSE;
      }
    }

    gst_video_codec_state_unref (enc->input_state);
  }
  enc->input_state = gst_video_codec_state_ref (state);

  output_state =
      gst_video_encoder_set_output_state (GST_VIDEO_ENCODER (enc),
      gst_caps_new_empty_simple ("image/webp"), enc->input_state);
  gst_video_codec_state_unref (output_state);

  if (enc->animated && !enc->anim_enc) {
    WebPAnimEncoderOptions enc_options = { 0 };
    if (!WebPAnimEncoderOptionsInit (&enc_options)) {
      GST_ERROR_OBJECT (enc, "Failed to initialize animation encoder options");
      return FALSE;
    }

    enc_options.anim_params.bgcolor = enc->animation_background_color;
    enc_options.anim_params.loop_count = enc->animation_loops;

    enc->anim_enc = WebPAnimEncoderNew (width, height, &enc_options);
    if (!enc->anim_enc) {
      GST_ERROR_OBJECT (enc, "Failed to create the animation encoder");
      return FALSE;
    }
  }

  return TRUE;
}

static gboolean
gst_webp_enc_init_picture (GstWebpEnc * enc)
{
  GstVideoInfo *info = &enc->input_state->info;

  if (!WebPPictureInit (&enc->webp_picture)) {
    GST_ERROR_OBJECT (enc, "Failed to Initialize WebPPicture !");
    return FALSE;
  }

  enc->webp_picture.use_argb = enc->use_argb;
  if (!enc->use_argb)
    enc->webp_picture.colorspace = enc->webp_color_space;

  enc->webp_picture.width = GST_VIDEO_INFO_WIDTH (info);
  enc->webp_picture.height = GST_VIDEO_INFO_HEIGHT (info);

  WebPMemoryWriterInit (&enc->webp_writer);
  enc->webp_picture.writer = WebPMemoryWrite;
  enc->webp_picture.custom_ptr = &enc->webp_writer;

  return TRUE;
}

static void
gst_webp_enc_clear_picture (GstWebpEnc * enc)
{
  WebPMemoryWriterClear (&enc->webp_writer);
  WebPPictureFree (&enc->webp_picture);
}

static GstFlowReturn
gst_webp_enc_handle_frame (GstVideoEncoder * encoder,
    GstVideoCodecFrame * frame)
{
  GstWebpEnc *enc = GST_WEBP_ENC (encoder);
  GstVideoFrame vframe;

  GST_LOG_OBJECT (enc, "got new frame");

  if (!gst_webp_enc_init_picture (enc))
    return GST_FLOW_ERROR;

  if (!gst_video_frame_map (&vframe, &enc->input_state->info,
          frame->input_buffer, GST_MAP_READ)) {
    gst_webp_enc_clear_picture (enc);
    return GST_FLOW_ERROR;
  }

  if (!enc->use_argb) {
    enc->webp_picture.y = GST_VIDEO_FRAME_COMP_DATA (&vframe, 0);
    enc->webp_picture.u = GST_VIDEO_FRAME_COMP_DATA (&vframe, 1);
    enc->webp_picture.v = GST_VIDEO_FRAME_COMP_DATA (&vframe, 2);

    enc->webp_picture.y_stride = GST_VIDEO_FRAME_COMP_STRIDE (&vframe, 0);
    enc->webp_picture.uv_stride = GST_VIDEO_FRAME_COMP_STRIDE (&vframe, 1);

  } else {
    switch (enc->rgb_format) {
      case GST_VIDEO_FORMAT_RGB:
        WebPPictureImportRGB (&enc->webp_picture,
            GST_VIDEO_FRAME_COMP_DATA (&vframe, 0),
            GST_VIDEO_FRAME_COMP_STRIDE (&vframe, 0));
        break;
      case GST_VIDEO_FORMAT_RGBA:
        WebPPictureImportRGBA (&enc->webp_picture,
            GST_VIDEO_FRAME_COMP_DATA (&vframe, 0),
            GST_VIDEO_FRAME_COMP_STRIDE (&vframe, 0));
        break;
      default:
        break;
    }
  }

  if (enc->anim_enc) {
    /* Webp timestamps are in milliseconds */
    int timestamp = frame->pts / 1000000;
    enc->next_timestamp = (frame->pts + frame->duration) / 1000000;

    if (!WebPAnimEncoderAdd (enc->anim_enc, &enc->webp_picture,
            timestamp, &enc->webp_config)) {
      GST_ERROR_OBJECT (enc, "Failed to add WebPPicture: %d (%s)",
          enc->webp_picture.error_code,
          WebPAnimEncoderGetError (enc->anim_enc));
      goto error;
    }
  } else {
    GstBuffer *out_buffer;

    if (!WebPEncode (&enc->webp_config, &enc->webp_picture)) {
      GST_ERROR_OBJECT (enc, "Failed to encode WebPPicture");
      goto error;
    }

    out_buffer = gst_buffer_new_allocate (NULL, enc->webp_writer.size, NULL);
    if (!out_buffer) {
      GST_ERROR_OBJECT (enc, "Failed to create output buffer");
      goto error;
    }

    gst_buffer_fill (out_buffer, 0, enc->webp_writer.mem,
        enc->webp_writer.size);
    frame->output_buffer = out_buffer;
  }

  gst_video_frame_unmap (&vframe);
  gst_webp_enc_clear_picture (enc);
  return gst_video_encoder_finish_frame (encoder, frame);

error:
  gst_video_frame_unmap (&vframe);
  gst_webp_enc_clear_picture (enc);
  return GST_FLOW_ERROR;
}

static gboolean
gst_webp_enc_propose_allocation (GstVideoEncoder * encoder, GstQuery * query)
{
  gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, NULL);
  return
      GST_VIDEO_ENCODER_CLASS (parent_class)->propose_allocation (encoder,
      query);
}

static void
gst_webp_enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstWebpEnc *webpenc = GST_WEBP_ENC (object);

  switch (prop_id) {
    case PROP_LOSSLESS:
      webpenc->lossless = g_value_get_boolean (value);
      break;
    case PROP_QUALITY:
      webpenc->quality = g_value_get_float (value);
      break;
    case PROP_SPEED:
      webpenc->speed = g_value_get_uint (value);
      break;
    case PROP_PRESET:
      webpenc->preset = g_value_get_enum (value);
      break;
    case PROP_ANIMATED:
      webpenc->animated = g_value_get_boolean (value);
      break;
    case PROP_ANIMATION_LOOPS:
      webpenc->animation_loops = g_value_get_uint (value);
      break;
    case PROP_ANIMATION_BACKGROUND_COLOR:
      webpenc->animation_background_color = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_webp_enc_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstWebpEnc *webpenc = GST_WEBP_ENC (object);

  switch (prop_id) {
    case PROP_LOSSLESS:
      g_value_set_boolean (value, webpenc->lossless);
      break;
    case PROP_QUALITY:
      g_value_set_float (value, webpenc->quality);
      break;
    case PROP_SPEED:
      g_value_set_uint (value, webpenc->speed);
      break;
    case PROP_PRESET:
      g_value_set_enum (value, webpenc->preset);
      break;
    case PROP_ANIMATED:
      g_value_set_boolean (value, webpenc->animated);
      break;
    case PROP_ANIMATION_LOOPS:
      g_value_set_uint (value, webpenc->animation_loops);
      break;
    case PROP_ANIMATION_BACKGROUND_COLOR:
      g_value_set_uint (value, webpenc->animation_background_color);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_webp_enc_start (GstVideoEncoder * encoder)
{
  GstWebpEnc *enc = (GstWebpEnc *) encoder;

  if (!WebPConfigPreset (&enc->webp_config, enc->preset, enc->quality)) {
    GST_ERROR_OBJECT (enc, "Failed to Initialize WebPConfig ");
    return FALSE;
  }

  enc->webp_config.lossless = enc->lossless;
  enc->webp_config.method = enc->speed;
  if (!WebPValidateConfig (&enc->webp_config)) {
    GST_ERROR_OBJECT (enc, "Failed to Validate the WebPConfig");
    return FALSE;
  }

  enc->next_timestamp = 0;

  return TRUE;
}

static GstFlowReturn
gst_webp_enc_finish (GstVideoEncoder * encoder)
{
  GstWebpEnc *enc = GST_WEBP_ENC (encoder);
  WebPData data = { 0 };
  GstBuffer *out;
  GstFlowReturn ret = GST_FLOW_OK;

  if (!enc->anim_enc)
    return ret;

  if (!WebPAnimEncoderAdd (enc->anim_enc, NULL, enc->next_timestamp,
          &enc->webp_config)) {
    GST_ERROR_OBJECT (enc, "Failed to flush animation encoder");
    ret = GST_FLOW_ERROR;
    goto done;
  }

  if (!WebPAnimEncoderAssemble (enc->anim_enc, &data)) {
    GST_ERROR_OBJECT (enc, "Failed to assemble output animation");
    ret = GST_FLOW_ERROR;
    goto done;
  }

  out = gst_buffer_new_allocate (NULL, data.size, NULL);
  gst_buffer_fill (out, 0, data.bytes, data.size);
  WebPDataClear (&data);
  ret = gst_pad_push (encoder->srcpad, out);

done:
  WebPAnimEncoderDelete (enc->anim_enc);
  enc->anim_enc = NULL;

  return ret;
}

static gboolean
gst_webp_enc_stop (GstVideoEncoder * encoder)
{
  GstWebpEnc *enc = GST_WEBP_ENC (encoder);
  if (enc->input_state) {
    gst_video_codec_state_unref (enc->input_state);
    enc->input_state = NULL;
  }

  if (enc->anim_enc) {
    WebPAnimEncoderDelete (enc->anim_enc);
    enc->anim_enc = NULL;
  }

  return TRUE;
}
