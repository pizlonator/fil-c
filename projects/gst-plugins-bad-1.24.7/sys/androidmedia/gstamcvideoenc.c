/*
 * Initially based on gst-plugins-bad/sys/androidmedia/gstamcvideodec.c
 *
 * Copyright (C) 2011, Hewlett-Packard Development Company, L.P.
 *   Author: Sebastian Dröge <sebastian.droege@collabora.co.uk>, Collabora Ltd.
 *
 * Copyright (C) 2012, Collabora Ltd.
 *   Author: Sebastian Dröge <sebastian.droege@collabora.co.uk>
 *
 * Copyright (C) 2013, Lemote Ltd.
 *   Author: Chen Jie <chenj@lemote.com>
 *
 * Copyright (C) 2015, Sebastian Dröge <sebastian@centricular.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <string.h>

#ifdef HAVE_ORC
#include <orc/orc.h>
#else
#define orc_memcpy memcpy
#endif

#ifdef HAVE_JNI_H
#include "gstjniutils.h"
#endif

#include "gstamcvideoenc.h"
#include "gstamc-constants.h"

GST_DEBUG_CATEGORY_STATIC (gst_amc_video_enc_debug_category);
#define GST_CAT_DEFAULT gst_amc_video_enc_debug_category

typedef struct _BufferIdentification BufferIdentification;
struct _BufferIdentification
{
  guint64 timestamp;
};

static BufferIdentification *
buffer_identification_new (GstClockTime timestamp)
{
  BufferIdentification *id = g_slice_new (BufferIdentification);

  id->timestamp = timestamp;

  return id;
}

static void
buffer_identification_free (BufferIdentification * id)
{
  g_slice_free (BufferIdentification, id);
}

/* prototypes */
static void gst_amc_video_enc_finalize (GObject * object);

static GstStateChangeReturn
gst_amc_video_enc_change_state (GstElement * element,
    GstStateChange transition);

static gboolean gst_amc_video_enc_open (GstVideoEncoder * encoder);
static gboolean gst_amc_video_enc_close (GstVideoEncoder * encoder);
static gboolean gst_amc_video_enc_start (GstVideoEncoder * encoder);
static gboolean gst_amc_video_enc_stop (GstVideoEncoder * encoder);
static gboolean gst_amc_video_enc_set_format (GstVideoEncoder * encoder,
    GstVideoCodecState * state);
static gboolean gst_amc_video_enc_flush (GstVideoEncoder * encoder);
static GstFlowReturn gst_amc_video_enc_handle_frame (GstVideoEncoder * encoder,
    GstVideoCodecFrame * frame);
static GstFlowReturn gst_amc_video_enc_finish (GstVideoEncoder * encoder);

static GstFlowReturn gst_amc_video_enc_drain (GstAmcVideoEnc * self);

#define BIT_RATE_DEFAULT (2 * 1024 * 1024)
#define I_FRAME_INTERVAL_DEFAULT 0
enum
{
  PROP_0,
  PROP_BIT_RATE,
  PROP_I_FRAME_INTERVAL,
  PROP_I_FRAME_INTERVAL_FLOAT
};

/* class initialization */

static void gst_amc_video_enc_class_init (GstAmcVideoEncClass * klass);
static void gst_amc_video_enc_init (GstAmcVideoEnc * self);
static void gst_amc_video_enc_base_init (gpointer g_class);

static GstVideoEncoderClass *parent_class = NULL;

GType
gst_amc_video_enc_get_type (void)
{
  static gsize type = 0;

  if (g_once_init_enter (&type)) {
    GType _type;
    static const GTypeInfo info = {
      sizeof (GstAmcVideoEncClass),
      gst_amc_video_enc_base_init,
      NULL,
      (GClassInitFunc) gst_amc_video_enc_class_init,
      NULL,
      NULL,
      sizeof (GstAmcVideoEnc),
      0,
      (GInstanceInitFunc) gst_amc_video_enc_init,
      NULL
    };

    _type = g_type_register_static (GST_TYPE_VIDEO_ENCODER, "GstAmcVideoEnc",
        &info, 0);

    GST_DEBUG_CATEGORY_INIT (gst_amc_video_enc_debug_category, "amcvideoenc", 0,
        "Android MediaCodec video encoder");

    g_once_init_leave (&type, _type);
  }
  return type;
}

static GstAmcFormat *
create_amc_format (GstAmcVideoEnc * encoder, GstVideoCodecState * input_state,
    GstCaps * src_caps)
{
  GstAmcVideoEncClass *klass;
  GstStructure *s;
  const gchar *name;
  const gchar *mime = NULL;
  const gchar *profile_string = NULL;
  const gchar *level_string = NULL;
  struct
  {
    const gchar *key;
    gint id;
  } amc_profile = {
    NULL, -1
  };
  struct
  {
    const gchar *key;
    gint id;
  } amc_level = {
    NULL, -1
  };
  gint color_format;
  gint stride, slice_height;
  GstAmcFormat *format = NULL;
  GstVideoInfo *info = &input_state->info;
  GError *err = NULL;

  klass = GST_AMC_VIDEO_ENC_GET_CLASS (encoder);
  s = gst_caps_get_structure (src_caps, 0);
  if (!s)
    return NULL;

  name = gst_structure_get_name (s);
  profile_string = gst_structure_get_string (s, "profile");
  level_string = gst_structure_get_string (s, "level");

  if (strcmp (name, "video/mpeg") == 0) {
    gint mpegversion;

    if (!gst_structure_get_int (s, "mpegversion", &mpegversion))
      return NULL;

    if (mpegversion == 4) {
      mime = "video/mp4v-es";

      if (profile_string) {
        amc_profile.key = "profile";    /* named profile ? */
        amc_profile.id = gst_amc_mpeg4_profile_from_string (profile_string);
      }

      if (level_string) {
        amc_level.key = "level";        /* named level ? */
        amc_level.id = gst_amc_mpeg4_level_from_string (level_string);
      }
    } else if ( /* mpegversion == 1 || */ mpegversion == 2)
      mime = "video/mpeg2";
  } else if (strcmp (name, "video/x-h263") == 0) {
    mime = "video/3gpp";
  } else if (strcmp (name, "video/x-h264") == 0) {
    mime = "video/avc";

    if (profile_string) {
      amc_profile.key = "profile";      /* named profile ? */
      amc_profile.id = gst_amc_avc_profile_from_string (profile_string);
    }

    if (level_string) {
      amc_level.key = "level";  /* named level ? */
      amc_level.id = gst_amc_avc_level_from_string (level_string);
    }
  } else if (strcmp (name, "video/x-h265") == 0) {
    const gchar *tier_string = gst_structure_get_string (s, "tier");

    mime = "video/hevc";

    if (profile_string) {
      amc_profile.key = "profile";      /* named profile ? */
      amc_profile.id = gst_amc_hevc_profile_from_string (profile_string);
    }

    if (level_string && tier_string) {
      amc_level.key = "level";  /* named level ? */
      amc_level.id =
          gst_amc_hevc_tier_level_from_string (tier_string, level_string);
    }
  } else if (strcmp (name, "video/x-vp8") == 0) {
    mime = "video/x-vnd.on2.vp8";
  } else if (strcmp (name, "video/x-vp9") == 0) {
    mime = "video/x-vnd.on2.vp9";
  } else if (strcmp (name, "video/x-av1") == 0) {
    mime = "video/av01";
  } else {
    GST_ERROR_OBJECT (encoder, "Failed to convert caps(%s/...) to any mime",
        name);
    return NULL;
  }

  format = gst_amc_format_new_video (mime, info->width, info->height, &err);
  if (!format) {
    GST_ERROR_OBJECT (encoder, "Failed to create a \"%s,%dx%d\" MediaFormat",
        mime, info->width, info->height);
    GST_ELEMENT_ERROR_FROM_ERROR (encoder, err);
    return NULL;
  }

  color_format =
      gst_amc_video_format_to_color_format (klass->codec_info,
      mime, info->finfo->format);
  if (color_format == -1)
    goto video_format_failed_to_convert;

  gst_amc_format_set_int (format, "bitrate", encoder->bitrate, &err);
  if (err)
    GST_ELEMENT_WARNING_FROM_ERROR (encoder, err);
  gst_amc_format_set_int (format, "color-format", color_format, &err);
  if (err)
    GST_ELEMENT_WARNING_FROM_ERROR (encoder, err);
  stride = GST_ROUND_UP_4 (info->width);        /* safe (?) */
  gst_amc_format_set_int (format, "stride", stride, &err);
  if (err)
    GST_ELEMENT_WARNING_FROM_ERROR (encoder, err);
  slice_height = info->height;
  gst_amc_format_set_int (format, "slice-height", slice_height, &err);
  if (err)
    GST_ELEMENT_WARNING_FROM_ERROR (encoder, err);

  if (profile_string) {
    if (amc_profile.id == -1)
      goto unsupported_profile;

    /* FIXME: Set to any value in AVCProfile* leads to
     * codec configuration fail */
    /* gst_amc_format_set_int (format, amc_profile.key, 0x40); */
  }

  if (level_string) {
    if (amc_level.id == -1)
      goto unsupported_level;

    /* gst_amc_format_set_int (format, amc_level.key, amc_level.id); */
  }

  /* On Android N_MR1 and higher, i-frame-interval can be a float value */
#ifdef HAVE_JNI_H
  if (gst_amc_jni_get_android_level () >= 25) {
    GST_LOG_OBJECT (encoder, "Setting i-frame-interval to %f",
        encoder->i_frame_int);
    gst_amc_format_set_float (format, "i-frame-interval", encoder->i_frame_int,
        &err);
  } else
#endif
  {
    int i_frame_int = encoder->i_frame_int;
    /* Round a fractional interval to 1 per sec on older Android */
    if (encoder->i_frame_int > 0 && encoder->i_frame_int < 1.0)
      i_frame_int = 1;
    gst_amc_format_set_int (format, "i-frame-interval", i_frame_int, &err);
  }
  if (err)
    GST_ELEMENT_WARNING_FROM_ERROR (encoder, err);

  if (info->fps_d)
    gst_amc_format_set_float (format, "frame-rate",
        ((gfloat) info->fps_n) / info->fps_d, &err);
  if (err)
    GST_ELEMENT_WARNING_FROM_ERROR (encoder, err);

  encoder->format = info->finfo->format;
  if (!gst_amc_color_format_info_set (&encoder->color_format_info,
          klass->codec_info, mime, color_format, info->width, info->height,
          stride, slice_height, 0, 0, 0, 0))
    goto color_format_info_failed_to_set;

  GST_DEBUG_OBJECT (encoder,
      "Color format info: {color_format=%d, width=%d, height=%d, "
      "stride=%d, slice-height=%d, crop-left=%d, crop-top=%d, "
      "crop-right=%d, crop-bottom=%d, frame-size=%d}",
      encoder->color_format_info.color_format, encoder->color_format_info.width,
      encoder->color_format_info.height, encoder->color_format_info.stride,
      encoder->color_format_info.slice_height,
      encoder->color_format_info.crop_left, encoder->color_format_info.crop_top,
      encoder->color_format_info.crop_right,
      encoder->color_format_info.crop_bottom,
      encoder->color_format_info.frame_size);

  return format;

video_format_failed_to_convert:
  GST_ERROR_OBJECT (encoder, "Failed to convert video format");
  gst_amc_format_free (format);
  return NULL;

color_format_info_failed_to_set:
  GST_ERROR_OBJECT (encoder, "Failed to set up GstAmcColorFormatInfo");
  gst_amc_format_free (format);
  return NULL;

unsupported_profile:
  GST_ERROR_OBJECT (encoder, "Unsupported profile '%s'", profile_string);
  gst_amc_format_free (format);
  return NULL;

unsupported_level:
  GST_ERROR_OBJECT (encoder, "Unsupported level '%s'", level_string);
  gst_amc_format_free (format);
  return NULL;
}

static GstCaps *
caps_from_amc_format (GstAmcFormat * amc_format)
{
  GstCaps *caps = NULL;
  gchar *mime = NULL;
  gint width, height;
  gint amc_profile, amc_level;
  gfloat frame_rate = 0.0;
  gint fraction_n, fraction_d;
  GError *err = NULL;

  if (!gst_amc_format_get_string (amc_format, "mime", &mime, &err)) {
    GST_ERROR ("Failed to get 'mime': %s", err->message);
    g_clear_error (&err);
    return NULL;
  }

  if (!gst_amc_format_get_int (amc_format, "width", &width, &err) ||
      !gst_amc_format_get_int (amc_format, "height", &height, &err)) {
    GST_ERROR ("Failed to get size: %s", err->message);
    g_clear_error (&err);

    g_free (mime);
    return NULL;
  }

  gst_amc_format_get_float (amc_format, "frame-rate", &frame_rate, NULL);
  gst_util_double_to_fraction (frame_rate, &fraction_n, &fraction_d);

  if (strcmp (mime, "video/mp4v-es") == 0) {
    const gchar *profile_string, *level_string;

    caps =
        gst_caps_new_simple ("video/mpeg", "mpegversion", G_TYPE_INT, 4,
        "systemstream", G_TYPE_BOOLEAN, FALSE, NULL);

    if (gst_amc_format_get_int (amc_format, "profile", &amc_profile, NULL)) {
      profile_string = gst_amc_mpeg4_profile_to_string (amc_profile);
      if (!profile_string)
        goto unsupported_profile;

      gst_caps_set_simple (caps, "profile", G_TYPE_STRING, profile_string,
          NULL);
    }

    if (gst_amc_format_get_int (amc_format, "level", &amc_level, NULL)) {
      level_string = gst_amc_mpeg4_level_to_string (amc_profile);
      if (!level_string)
        goto unsupported_level;

      gst_caps_set_simple (caps, "level", G_TYPE_STRING, level_string, NULL);
    }

  } else if (strcmp (mime, "video/mpeg2") == 0) {
    caps = gst_caps_new_simple ("video/mpeg", "mpegversion", 2, NULL);
  } else if (strcmp (mime, "video/3gpp") == 0) {
    caps = gst_caps_new_empty_simple ("video/x-h263");
  } else if (strcmp (mime, "video/avc") == 0) {
    const gchar *profile_string, *level_string;

    caps =
        gst_caps_new_simple ("video/x-h264",
        "stream-format", G_TYPE_STRING, "byte-stream", NULL);

    if (gst_amc_format_get_int (amc_format, "profile", &amc_profile, NULL)) {
      profile_string = gst_amc_avc_profile_to_string (amc_profile, NULL);
      if (!profile_string)
        goto unsupported_profile;

      gst_caps_set_simple (caps, "profile", G_TYPE_STRING, profile_string,
          NULL);
    }

    if (gst_amc_format_get_int (amc_format, "level", &amc_level, NULL)) {
      level_string = gst_amc_avc_level_to_string (amc_profile);
      if (!level_string)
        goto unsupported_level;

      gst_caps_set_simple (caps, "level", G_TYPE_STRING, level_string, NULL);
    }
  } else if (strcmp (mime, "video/hevc") == 0) {
    const gchar *profile_string, *level_string, *tier_string;

    caps =
        gst_caps_new_simple ("video/x-h265",
        "stream-format", G_TYPE_STRING, "byte-stream", NULL);

    if (gst_amc_format_get_int (amc_format, "profile", &amc_profile, NULL)) {
      profile_string = gst_amc_avc_profile_to_string (amc_profile, NULL);
      if (!profile_string)
        goto unsupported_profile;

      gst_caps_set_simple (caps, "profile", G_TYPE_STRING, profile_string,
          NULL);
    }

    if (gst_amc_format_get_int (amc_format, "level", &amc_level, NULL)) {
      level_string =
          gst_amc_hevc_tier_level_to_string (amc_profile, &tier_string);
      if (!level_string || !tier_string)
        goto unsupported_level;

      gst_caps_set_simple (caps,
          "level", G_TYPE_STRING, level_string,
          "tier", G_TYPE_STRING, tier_string, NULL);
    }
  } else if (strcmp (mime, "video/x-vnd.on2.vp8") == 0) {
    caps = gst_caps_new_empty_simple ("video/x-vp8");
  } else if (strcmp (mime, "video/x-vnd.on2.vp9") == 0) {
    caps = gst_caps_new_empty_simple ("video/x-vp9");
  } else if (strcmp (mime, "video/av01") == 0) {
    caps = gst_caps_new_simple ("video/x-av1",
        "stream-format", G_TYPE_STRING, "obu-stream",
        "alignment", G_TYPE_STRING, "tu", NULL);
  }

  gst_caps_set_simple (caps, "width", G_TYPE_INT, width,
      "height", G_TYPE_INT, height,
      "framerate", GST_TYPE_FRACTION, fraction_n, fraction_d, NULL);

  g_free (mime);
  return caps;

unsupported_profile:
  GST_ERROR ("Unsupported amc profile id %d", amc_profile);
  g_free (mime);
  gst_caps_unref (caps);

  return NULL;

unsupported_level:
  GST_ERROR ("Unsupported amc level id %d", amc_level);
  g_free (mime);
  gst_caps_unref (caps);

  return NULL;
}

static void
gst_amc_video_enc_base_init (gpointer g_class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);
  GstAmcVideoEncClass *videoenc_class = GST_AMC_VIDEO_ENC_CLASS (g_class);
  const GstAmcCodecInfo *codec_info;
  GstPadTemplate *templ;
  GstCaps *sink_caps, *src_caps;
  gchar *longname;

  codec_info =
      g_type_get_qdata (G_TYPE_FROM_CLASS (g_class), gst_amc_codec_info_quark);
  /* This happens for the base class and abstract subclasses */
  if (!codec_info)
    return;

  videoenc_class->codec_info = codec_info;

  gst_amc_codec_info_to_caps (codec_info, &sink_caps, &src_caps);
  /* Add pad templates */
  templ =
      gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS, sink_caps);
  gst_element_class_add_pad_template (element_class, templ);
  gst_caps_unref (sink_caps);

  templ = gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS, src_caps);
  gst_element_class_add_pad_template (element_class, templ);
  gst_caps_unref (src_caps);

  longname = g_strdup_printf ("Android MediaCodec %s", codec_info->name);
  gst_element_class_set_metadata (element_class,
      codec_info->name,
      "Codec/Encoder/Video/Hardware",
      longname, "Sebastian Dröge <sebastian.droege@collabora.co.uk>");
  g_free (longname);
}

static void
gst_amc_video_enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstAmcVideoEnc *encoder;
  GstState state;
  gboolean codec_active;
  GError *err = NULL;

  encoder = GST_AMC_VIDEO_ENC (object);

  GST_OBJECT_LOCK (encoder);

  state = GST_STATE (encoder);
  codec_active = (encoder->codec && state != GST_STATE_READY
      && state != GST_STATE_NULL);

  switch (prop_id) {
    case PROP_BIT_RATE:
      encoder->bitrate = g_value_get_uint (value);

      g_mutex_lock (&encoder->codec_lock);
      if (encoder->codec) {
        if (!gst_amc_codec_set_dynamic_bitrate (encoder->codec, &err,
                encoder->bitrate)) {
          g_mutex_unlock (&encoder->codec_lock);
          goto wrong_state;
        }
      }
      g_mutex_unlock (&encoder->codec_lock);
      if (err) {
        GST_ELEMENT_WARNING_FROM_ERROR (encoder, err);
        g_clear_error (&err);
      }

      break;
    case PROP_I_FRAME_INTERVAL:
      encoder->i_frame_int = g_value_get_uint (value);
      if (codec_active)
        goto wrong_state;
      break;
    case PROP_I_FRAME_INTERVAL_FLOAT:
      encoder->i_frame_int = g_value_get_float (value);
      if (codec_active)
        goto wrong_state;
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (encoder);
  return;

  /* ERROR */
wrong_state:
  {
    GST_WARNING_OBJECT (encoder, "setting property in wrong state");
    GST_OBJECT_UNLOCK (encoder);
  }
}

static void
gst_amc_video_enc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstAmcVideoEnc *encoder;

  encoder = GST_AMC_VIDEO_ENC (object);

  GST_OBJECT_LOCK (encoder);
  switch (prop_id) {
    case PROP_BIT_RATE:
      g_value_set_uint (value, encoder->bitrate);
      break;
    case PROP_I_FRAME_INTERVAL:
      g_value_set_uint (value, encoder->i_frame_int);
      break;
    case PROP_I_FRAME_INTERVAL_FLOAT:
      g_value_set_float (value, encoder->i_frame_int);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (encoder);
}


static void
gst_amc_video_enc_class_init (GstAmcVideoEncClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstVideoEncoderClass *videoenc_class = GST_VIDEO_ENCODER_CLASS (klass);
  GParamFlags dynamic_flag = 0;

  parent_class = g_type_class_peek_parent (klass);

  gobject_class->set_property = gst_amc_video_enc_set_property;
  gobject_class->get_property = gst_amc_video_enc_get_property;
  gobject_class->finalize = gst_amc_video_enc_finalize;

  element_class->change_state =
      GST_DEBUG_FUNCPTR (gst_amc_video_enc_change_state);

  videoenc_class->start = GST_DEBUG_FUNCPTR (gst_amc_video_enc_start);
  videoenc_class->stop = GST_DEBUG_FUNCPTR (gst_amc_video_enc_stop);
  videoenc_class->open = GST_DEBUG_FUNCPTR (gst_amc_video_enc_open);
  videoenc_class->close = GST_DEBUG_FUNCPTR (gst_amc_video_enc_close);
  videoenc_class->flush = GST_DEBUG_FUNCPTR (gst_amc_video_enc_flush);
  videoenc_class->set_format = GST_DEBUG_FUNCPTR (gst_amc_video_enc_set_format);
  videoenc_class->handle_frame =
      GST_DEBUG_FUNCPTR (gst_amc_video_enc_handle_frame);
  videoenc_class->finish = GST_DEBUG_FUNCPTR (gst_amc_video_enc_finish);

  // On Android >= 19, we can set bitrate dynamically
  // so add the flag so apps can detect it.
  if (gst_amc_codec_have_dynamic_bitrate ())
    dynamic_flag = GST_PARAM_MUTABLE_PLAYING;

  g_object_class_install_property (gobject_class, PROP_BIT_RATE,
      g_param_spec_uint ("bitrate", "Bitrate", "Bitrate in bit/sec", 1,
          G_MAXINT, BIT_RATE_DEFAULT,
          dynamic_flag | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_I_FRAME_INTERVAL,
      g_param_spec_uint ("i-frame-interval", "I-frame interval",
          "The frequency of I frames expressed in seconds between I frames (0 for automatic)",
          0, G_MAXINT, I_FRAME_INTERVAL_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_I_FRAME_INTERVAL_FLOAT,
      g_param_spec_float ("i-frame-interval-float", "I-frame interval",
          "The frequency of I frames expressed in seconds between I frames (0 for automatic). "
          "Fractional intervals work on Android >= 25",
          0, G_MAXFLOAT, I_FRAME_INTERVAL_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
gst_amc_video_enc_init (GstAmcVideoEnc * self)
{
  g_mutex_init (&self->codec_lock);
  g_mutex_init (&self->drain_lock);
  g_cond_init (&self->drain_cond);

  self->bitrate = BIT_RATE_DEFAULT;
  self->i_frame_int = I_FRAME_INTERVAL_DEFAULT;
}

static gboolean
gst_amc_video_enc_open (GstVideoEncoder * encoder)
{
  GstAmcVideoEnc *self = GST_AMC_VIDEO_ENC (encoder);
  GstAmcVideoEncClass *klass = GST_AMC_VIDEO_ENC_GET_CLASS (self);
  GError *err = NULL;

  GST_DEBUG_OBJECT (self, "Opening encoder");

  g_mutex_lock (&self->codec_lock);
  self->codec = gst_amc_codec_new (klass->codec_info->name, TRUE, &err);
  if (!self->codec) {
    g_mutex_unlock (&self->codec_lock);
    GST_ELEMENT_ERROR_FROM_ERROR (self, err);
    return FALSE;
  }
  g_mutex_unlock (&self->codec_lock);
  self->started = FALSE;
  self->flushing = TRUE;

  GST_DEBUG_OBJECT (self, "Opened encoder");

  return TRUE;
}

static gboolean
gst_amc_video_enc_close (GstVideoEncoder * encoder)
{
  GstAmcVideoEnc *self = GST_AMC_VIDEO_ENC (encoder);

  GST_DEBUG_OBJECT (self, "Closing encoder");

  g_mutex_lock (&self->codec_lock);
  if (self->codec) {
    GError *err = NULL;

    gst_amc_codec_release (self->codec, &err);
    if (err)
      GST_ELEMENT_WARNING_FROM_ERROR (self, err);

    gst_amc_codec_free (self->codec);
  }
  self->codec = NULL;
  g_mutex_unlock (&self->codec_lock);

  self->started = FALSE;
  self->flushing = TRUE;

  GST_DEBUG_OBJECT (self, "Closed encoder");

  return TRUE;
}

static void
gst_amc_video_enc_finalize (GObject * object)
{
  GstAmcVideoEnc *self = GST_AMC_VIDEO_ENC (object);

  g_mutex_clear (&self->codec_lock);
  g_mutex_clear (&self->drain_lock);
  g_cond_clear (&self->drain_cond);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static GstStateChangeReturn
gst_amc_video_enc_change_state (GstElement * element, GstStateChange transition)
{
  GstAmcVideoEnc *self;
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  GError *err = NULL;

  g_return_val_if_fail (GST_IS_AMC_VIDEO_ENC (element),
      GST_STATE_CHANGE_FAILURE);
  self = GST_AMC_VIDEO_ENC (element);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      self->downstream_flow_ret = GST_FLOW_OK;
      self->draining = FALSE;
      self->started = FALSE;
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      self->flushing = TRUE;
      gst_amc_codec_flush (self->codec, &err);
      if (err)
        GST_ELEMENT_WARNING_FROM_ERROR (self, err);
      g_mutex_lock (&self->drain_lock);
      self->draining = FALSE;
      g_cond_broadcast (&self->drain_cond);
      g_mutex_unlock (&self->drain_lock);
      break;
    default:
      break;
  }

  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      self->downstream_flow_ret = GST_FLOW_FLUSHING;
      self->started = FALSE;
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      break;
    default:
      break;
  }

  return ret;
}

#define MAX_FRAME_DIST_TIME  (5 * GST_SECOND)
#define MAX_FRAME_DIST_FRAMES (100)

static GstVideoCodecFrame *
_find_nearest_frame (GstAmcVideoEnc * self, GstClockTime reference_timestamp)
{
  GList *l, *best_l = NULL;
  GList *finish_frames = NULL;
  GstVideoCodecFrame *best = NULL;
  guint64 best_timestamp = 0;
  guint64 best_diff = G_MAXUINT64;
  BufferIdentification *best_id = NULL;
  GList *frames;

  frames = gst_video_encoder_get_frames (GST_VIDEO_ENCODER (self));

  for (l = frames; l; l = l->next) {
    GstVideoCodecFrame *tmp = l->data;
    BufferIdentification *id = gst_video_codec_frame_get_user_data (tmp);
    guint64 timestamp, diff;

    /* This happens for frames that were just added but
     * which were not passed to the component yet. Ignore
     * them here!
     */
    if (!id)
      continue;

    timestamp = id->timestamp;

    if (timestamp > reference_timestamp)
      diff = timestamp - reference_timestamp;
    else
      diff = reference_timestamp - timestamp;

    if (best == NULL || diff < best_diff) {
      best = tmp;
      best_timestamp = timestamp;
      best_diff = diff;
      best_l = l;
      best_id = id;

      /* For frames without timestamp we simply take the first frame */
      if ((reference_timestamp == 0 && !GST_CLOCK_TIME_IS_VALID (timestamp))
          || diff == 0)
        break;
    }
  }

  if (best_id) {
    for (l = frames; l && l != best_l; l = l->next) {
      GstVideoCodecFrame *tmp = l->data;
      BufferIdentification *id = gst_video_codec_frame_get_user_data (tmp);
      guint64 diff_time, diff_frames;

      if (id->timestamp > best_timestamp)
        break;

      if (id->timestamp == 0 || best_timestamp == 0)
        diff_time = 0;
      else
        diff_time = best_timestamp - id->timestamp;
      diff_frames = best->system_frame_number - tmp->system_frame_number;

      if (diff_time > MAX_FRAME_DIST_TIME
          || diff_frames > MAX_FRAME_DIST_FRAMES) {
        finish_frames =
            g_list_prepend (finish_frames, gst_video_codec_frame_ref (tmp));
      }
    }
  }

  if (finish_frames) {
    g_warning ("%s: Too old frames, bug in encoder -- please file a bug",
        GST_ELEMENT_NAME (self));
    for (l = finish_frames; l; l = l->next) {
      gst_video_encoder_finish_frame (GST_VIDEO_ENCODER (self), l->data);
    }
  }

  if (best)
    gst_video_codec_frame_ref (best);

  GST_DEBUG_OBJECT (self, "found best %p from %u frames", best,
      g_list_length (frames));
  if (best) {
    GST_LOG_OBJECT (self, "best %p (input pts %" GST_TIME_FORMAT " dts %"
        GST_TIME_FORMAT " frame no %" G_GUINT32_FORMAT " buffer %"
        GST_PTR_FORMAT, best, GST_TIME_ARGS (best->pts),
        GST_TIME_ARGS (best->dts), best->system_frame_number,
        best->input_buffer);
  }

  g_list_foreach (frames, (GFunc) gst_video_codec_frame_unref, NULL);
  g_list_free (frames);

  return best;
}

static gboolean
gst_amc_video_enc_set_src_caps (GstAmcVideoEnc * self, GstAmcFormat * format)
{
  GstCaps *caps;
  GstVideoCodecState *output_state;
  GstStructure *s;

  caps = caps_from_amc_format (format);
  if (!caps) {
    GST_ERROR_OBJECT (self, "Failed to create output caps");
    return FALSE;
  }

  /* It may not be proper to reference self->input_state here,
   * because MediaCodec is an async model -- input_state may change multiple times,
   * the passed-in MediaFormat may not be the one matched to the current input_state.
   *
   * Though, currently, the final src caps only calculate
   * width/height/pixel-aspect-ratio/framerate/codec_data from self->input_state.
   *
   * If input width/height/codec_data change(is_format_change), it will restart
   * MediaCodec, which means in these cases, self->input_state is matched.
   */
  output_state = gst_video_encoder_set_output_state (GST_VIDEO_ENCODER (self),
      caps, self->input_state);
  gst_video_codec_state_unref (output_state);

  if (!gst_video_encoder_negotiate (GST_VIDEO_ENCODER (self)))
    return FALSE;

  output_state = gst_video_encoder_get_output_state (GST_VIDEO_ENCODER (self));
  s = gst_caps_get_structure (output_state->caps, 0);

  if (!strcmp (gst_structure_get_name (s), "video/x-h264") ||
      !strcmp (gst_structure_get_name (s), "video/x-h265")) {
    self->codec_data_in_bytestream = TRUE;
  } else {
    self->codec_data_in_bytestream = FALSE;
  }
  gst_video_codec_state_unref (output_state);

  return TRUE;
}

/* The weird handling of cropping, alignment and everything is taken from
 * platform/frameworks/media/libstagefright/colorconversion/ColorConversion.cpp
 */
static gboolean
gst_amc_video_enc_fill_buffer (GstAmcVideoEnc * self, GstBuffer * inbuf,
    GstAmcBuffer * outbuf, const GstAmcBufferInfo * buffer_info)
{
  GstVideoCodecState *input_state = self->input_state;
  /* The fill_buffer runs in the same thread as set_format?
   * then we can use state->info safely */
  GstVideoInfo *info = &input_state->info;

  if (buffer_info->size < self->color_format_info.frame_size)
    return FALSE;

  return gst_amc_color_format_copy (&self->color_format_info, outbuf,
      buffer_info, info, inbuf, COLOR_FORMAT_COPY_IN);
}

static GstFlowReturn
gst_amc_video_enc_handle_output_frame (GstAmcVideoEnc * self,
    GstAmcBuffer * buf, const GstAmcBufferInfo * buffer_info,
    GstVideoCodecFrame * frame)
{
  GstFlowReturn flow_ret = GST_FLOW_OK;
  GstVideoEncoder *encoder = GST_VIDEO_ENCODER_CAST (self);

  if (buffer_info->size > 0) {
    GstBuffer *out_buf;
    GstPad *srcpad;

    if (buffer_info->flags & BUFFER_FLAG_PARTIAL_FRAME) {
      GST_FIXME_OBJECT (self, "partial frames are currently not handled");
    }

    srcpad = GST_VIDEO_ENCODER_SRC_PAD (encoder);
    out_buf =
        gst_video_encoder_allocate_output_buffer (encoder, buffer_info->size);
    gst_buffer_fill (out_buf, 0, buf->data + buffer_info->offset,
        buffer_info->size);

    GST_BUFFER_PTS (out_buf) =
        gst_util_uint64_scale (buffer_info->presentation_time_us, GST_USECOND,
        1);

    if (frame) {
      frame->output_buffer = out_buf;
      flow_ret = gst_video_encoder_finish_frame (encoder, frame);
    } else {
      /* This sometimes happens at EOS or if the input is not properly framed,
       * let's handle it gracefully by allocating a new buffer for the current
       * caps and filling it
       */

      GST_ERROR_OBJECT (self, "No corresponding frame found: buffer pts: %"
          GST_TIME_FORMAT " presentation_time_us %" G_GUINT64_FORMAT,
          GST_TIME_ARGS (GST_BUFFER_PTS (out_buf)),
          (guint64) buffer_info->presentation_time_us);
      flow_ret = gst_pad_push (srcpad, out_buf);
    }
  } else if (frame) {
    flow_ret = gst_video_encoder_finish_frame (encoder, frame);
  }

  return flow_ret;
}

static void
gst_amc_video_enc_loop (GstAmcVideoEnc * self)
{
  GstVideoEncoder *encoder = GST_VIDEO_ENCODER_CAST (self);
  GstVideoCodecFrame *frame;
  GstFlowReturn flow_ret = GST_FLOW_OK;
  gboolean is_eos, is_codec_data;
  GstAmcBufferInfo buffer_info;
  GstAmcBuffer *buf;
  gint idx;
  GError *err = NULL;

  GST_VIDEO_ENCODER_STREAM_LOCK (self);

retry:
  GST_DEBUG_OBJECT (self, "Waiting for available output buffer");
  GST_VIDEO_ENCODER_STREAM_UNLOCK (self);
  /* Wait at most 100ms here, some codecs don't fail dequeueing if
   * the codec is flushing, causing deadlocks during shutdown */
  idx =
      gst_amc_codec_dequeue_output_buffer (self->codec, &buffer_info, 100000,
      &err);
  GST_VIDEO_ENCODER_STREAM_LOCK (self);
  /*} */

  if (idx < 0 || self->amc_format) {
    if (self->flushing) {
      g_clear_error (&err);
      goto flushing;
    }

    /* The comments from https://android.googlesource.com/platform/cts/+/android-4.3_r3.1/tests/tests/media/src/android/media/cts/EncodeDecodeTest.java
     * line 539 says INFO_OUTPUT_FORMAT_CHANGED is not expected for an encoder
     */
    if (self->amc_format || idx == INFO_OUTPUT_FORMAT_CHANGED) {
      GstAmcFormat *format;
      gchar *format_string;

      GST_DEBUG_OBJECT (self, "Output format has changed");

      format = (idx == INFO_OUTPUT_FORMAT_CHANGED) ?
          gst_amc_codec_get_output_format (self->codec,
          &err) : self->amc_format;
      if (err) {
        format = self->amc_format;
        GST_ELEMENT_WARNING_FROM_ERROR (self, err);
      }

      if (self->amc_format) {
        if (format != self->amc_format)
          gst_amc_format_free (self->amc_format);
        self->amc_format = NULL;
      }

      if (!format)
        goto format_error;

      format_string = gst_amc_format_to_string (format, &err);
      if (err) {
        gst_amc_format_free (format);
        goto format_error;
      }
      GST_DEBUG_OBJECT (self, "Got new output format: %s", format_string);
      g_free (format_string);

      if (!gst_amc_video_enc_set_src_caps (self, format)) {
        gst_amc_format_free (format);
        goto format_error;
      }

      gst_amc_format_free (format);

      if (idx >= 0)
        goto process_buffer;

      goto retry;
    }

    switch (idx) {
      case INFO_OUTPUT_BUFFERS_CHANGED:
        /* Handled internally */
        g_assert_not_reached ();
        break;
      case INFO_TRY_AGAIN_LATER:
        GST_DEBUG_OBJECT (self, "Dequeueing output buffer timed out");
        goto retry;
        break;
      case G_MININT:
        GST_ERROR_OBJECT (self, "Failure dequeueing input buffer");
        goto dequeue_error;
        break;
      default:
        g_assert_not_reached ();
        break;
    }

    goto retry;
  }

process_buffer:
  GST_DEBUG_OBJECT (self,
      "Got output buffer at index %d: size %d time %" G_GINT64_FORMAT
      " flags 0x%08x", idx, buffer_info.size, buffer_info.presentation_time_us,
      buffer_info.flags);

  buf = gst_amc_codec_get_output_buffer (self->codec, idx, &err);
  if (err) {
    if (self->flushing) {
      g_clear_error (&err);
      goto flushing;
    }
    goto failed_to_get_output_buffer;
  } else if (!buf) {
    goto got_null_output_buffer;
  }

  is_codec_data = FALSE;
  /* The BUFFER_FLAG_CODEC_CONFIG logic is borrowed from
   * gst-omx. see *_handle_output_frame in
   * gstomxvideoenc.c and gstomxh264enc.c */
  if ((buffer_info.flags & BUFFER_FLAG_CODEC_CONFIG)
      && buffer_info.size > 0) {

    if (self->codec_data_in_bytestream) {
      if (buffer_info.size > 4 &&
          GST_READ_UINT32_BE (buf->data + buffer_info.offset) == 0x00000001) {
        GList *l = NULL;
        GstBuffer *hdrs;

        GST_DEBUG_OBJECT (self, "got codecconfig in byte-stream format");

        hdrs = gst_buffer_new_and_alloc (buffer_info.size);
        gst_buffer_fill (hdrs, 0, buf->data + buffer_info.offset,
            buffer_info.size);
        GST_BUFFER_PTS (hdrs) =
            gst_util_uint64_scale (buffer_info.presentation_time_us,
            GST_USECOND, 1);

        l = g_list_append (l, hdrs);
        gst_video_encoder_set_headers (encoder, l);
        is_codec_data = TRUE;
      }
    } else {
      GstBuffer *codec_data;
      GstVideoCodecState *output_state =
          gst_video_encoder_get_output_state (GST_VIDEO_ENCODER (self));

      GST_DEBUG_OBJECT (self, "Handling codec data");

      codec_data = gst_buffer_new_and_alloc (buffer_info.size);
      gst_buffer_fill (codec_data, 0, buf->data + buffer_info.offset,
          buffer_info.size);
      output_state->codec_data = codec_data;
      gst_video_codec_state_unref (output_state);
      is_codec_data = TRUE;

      if (!gst_video_encoder_negotiate (encoder))
        flow_ret = GST_FLOW_NOT_NEGOTIATED;
    }
  }

  is_eos = !!(buffer_info.flags & BUFFER_FLAG_END_OF_STREAM);

  if (flow_ret == GST_FLOW_OK && !is_codec_data) {
    frame =
        _find_nearest_frame (self,
        gst_util_uint64_scale (buffer_info.presentation_time_us, GST_USECOND,
            1));

    flow_ret =
        gst_amc_video_enc_handle_output_frame (self, buf, &buffer_info, frame);
  }

  gst_amc_buffer_free (buf);
  buf = NULL;

  if (!gst_amc_codec_release_output_buffer (self->codec, idx, FALSE, &err)) {
    if (self->flushing) {
      g_clear_error (&err);
      goto flushing;
    }
    goto failed_release;
  }

  if (is_eos || flow_ret == GST_FLOW_EOS) {
    GST_VIDEO_ENCODER_STREAM_UNLOCK (self);
    g_mutex_lock (&self->drain_lock);
    if (self->draining) {
      GST_DEBUG_OBJECT (self, "Drained");
      self->draining = FALSE;
      g_cond_broadcast (&self->drain_cond);
    } else if (flow_ret == GST_FLOW_OK) {
      GST_DEBUG_OBJECT (self, "Component signalled EOS");
      flow_ret = GST_FLOW_EOS;
    }
    g_mutex_unlock (&self->drain_lock);
    GST_VIDEO_ENCODER_STREAM_LOCK (self);
  } else {
    GST_DEBUG_OBJECT (self, "Finished frame: %s", gst_flow_get_name (flow_ret));
  }

  self->downstream_flow_ret = flow_ret;

  if (flow_ret != GST_FLOW_OK)
    goto flow_error;

  GST_VIDEO_ENCODER_STREAM_UNLOCK (self);

  return;

dequeue_error:
  {
    GST_ELEMENT_ERROR_FROM_ERROR (self, err);
    gst_pad_push_event (GST_VIDEO_ENCODER_SRC_PAD (self), gst_event_new_eos ());
    gst_pad_pause_task (GST_VIDEO_ENCODER_SRC_PAD (self));
    self->downstream_flow_ret = GST_FLOW_ERROR;
    GST_VIDEO_ENCODER_STREAM_UNLOCK (self);
    g_mutex_lock (&self->drain_lock);
    self->draining = FALSE;
    g_cond_broadcast (&self->drain_cond);
    g_mutex_unlock (&self->drain_lock);
    return;
  }

format_error:
  {
    if (err)
      GST_ELEMENT_ERROR_FROM_ERROR (self, err);
    else
      GST_ELEMENT_ERROR (self, LIBRARY, FAILED, (NULL),
          ("Failed to handle format"));
    gst_pad_push_event (GST_VIDEO_ENCODER_SRC_PAD (self), gst_event_new_eos ());
    gst_pad_pause_task (GST_VIDEO_ENCODER_SRC_PAD (self));
    self->downstream_flow_ret = GST_FLOW_ERROR;
    GST_VIDEO_ENCODER_STREAM_UNLOCK (self);
    g_mutex_lock (&self->drain_lock);
    self->draining = FALSE;
    g_cond_broadcast (&self->drain_cond);
    g_mutex_unlock (&self->drain_lock);
    return;
  }
failed_release:
  {
    GST_ELEMENT_ERROR_FROM_ERROR (self, err);
    gst_pad_push_event (GST_VIDEO_ENCODER_SRC_PAD (self), gst_event_new_eos ());
    gst_pad_pause_task (GST_VIDEO_ENCODER_SRC_PAD (self));
    self->downstream_flow_ret = GST_FLOW_ERROR;
    GST_VIDEO_ENCODER_STREAM_UNLOCK (self);
    g_mutex_lock (&self->drain_lock);
    self->draining = FALSE;
    g_cond_broadcast (&self->drain_cond);
    g_mutex_unlock (&self->drain_lock);
    return;
  }
flushing:
  {
    GST_DEBUG_OBJECT (self, "Flushing -- stopping task");
    gst_pad_pause_task (GST_VIDEO_ENCODER_SRC_PAD (self));
    self->downstream_flow_ret = GST_FLOW_FLUSHING;
    GST_VIDEO_ENCODER_STREAM_UNLOCK (self);
    return;
  }

flow_error:
  {
    if (flow_ret == GST_FLOW_EOS) {
      GST_DEBUG_OBJECT (self, "EOS");
      gst_pad_push_event (GST_VIDEO_ENCODER_SRC_PAD (self),
          gst_event_new_eos ());
      gst_pad_pause_task (GST_VIDEO_ENCODER_SRC_PAD (self));
    } else if (flow_ret == GST_FLOW_NOT_LINKED || flow_ret < GST_FLOW_EOS) {
      GST_ELEMENT_FLOW_ERROR (self, flow_ret);
      gst_pad_push_event (GST_VIDEO_ENCODER_SRC_PAD (self),
          gst_event_new_eos ());
      gst_pad_pause_task (GST_VIDEO_ENCODER_SRC_PAD (self));
    }
    GST_VIDEO_ENCODER_STREAM_UNLOCK (self);
    g_mutex_lock (&self->drain_lock);
    self->draining = FALSE;
    g_cond_broadcast (&self->drain_cond);
    g_mutex_unlock (&self->drain_lock);
    return;
  }

failed_to_get_output_buffer:
  {
    GST_ELEMENT_ERROR_FROM_ERROR (self, err);
    gst_pad_push_event (GST_VIDEO_ENCODER_SRC_PAD (self), gst_event_new_eos ());
    gst_pad_pause_task (GST_VIDEO_ENCODER_SRC_PAD (self));
    self->downstream_flow_ret = GST_FLOW_ERROR;
    GST_VIDEO_ENCODER_STREAM_UNLOCK (self);
    g_mutex_lock (&self->drain_lock);
    self->draining = FALSE;
    g_cond_broadcast (&self->drain_cond);
    g_mutex_unlock (&self->drain_lock);
    return;
  }

got_null_output_buffer:
  {
    GST_ELEMENT_ERROR (self, LIBRARY, SETTINGS, (NULL),
        ("Got no output buffer"));
    gst_pad_push_event (GST_VIDEO_ENCODER_SRC_PAD (self), gst_event_new_eos ());
    gst_pad_pause_task (GST_VIDEO_ENCODER_SRC_PAD (self));
    self->downstream_flow_ret = GST_FLOW_ERROR;
    GST_VIDEO_ENCODER_STREAM_UNLOCK (self);
    g_mutex_lock (&self->drain_lock);
    self->draining = FALSE;
    g_cond_broadcast (&self->drain_cond);
    g_mutex_unlock (&self->drain_lock);
    return;
  }
}

static gboolean
gst_amc_video_enc_start (GstVideoEncoder * encoder)
{
  GstAmcVideoEnc *self;

  self = GST_AMC_VIDEO_ENC (encoder);
  self->last_upstream_ts = 0;
  self->drained = TRUE;
  self->downstream_flow_ret = GST_FLOW_OK;
  self->started = FALSE;
  self->flushing = TRUE;

  return TRUE;
}

static gboolean
gst_amc_video_enc_stop (GstVideoEncoder * encoder)
{
  GstAmcVideoEnc *self;
  GError *err = NULL;

  self = GST_AMC_VIDEO_ENC (encoder);
  GST_DEBUG_OBJECT (self, "Stopping encoder");
  self->flushing = TRUE;
  if (self->started) {
    gst_amc_codec_flush (self->codec, &err);
    if (err)
      GST_ELEMENT_WARNING_FROM_ERROR (self, err);
    gst_amc_codec_stop (self->codec, &err);
    if (err)
      GST_ELEMENT_WARNING_FROM_ERROR (self, err);
    self->started = FALSE;
  }
  gst_pad_stop_task (GST_VIDEO_ENCODER_SRC_PAD (encoder));

  self->downstream_flow_ret = GST_FLOW_FLUSHING;
  self->drained = TRUE;
  g_mutex_lock (&self->drain_lock);
  self->draining = FALSE;
  g_cond_broadcast (&self->drain_cond);
  g_mutex_unlock (&self->drain_lock);
  if (self->input_state)
    gst_video_codec_state_unref (self->input_state);
  self->input_state = NULL;

  if (self->amc_format) {
    gst_amc_format_free (self->amc_format);
    self->amc_format = NULL;
  }

  GST_DEBUG_OBJECT (self, "Stopped encoder");
  return TRUE;
}

static gboolean
gst_amc_video_enc_set_format (GstVideoEncoder * encoder,
    GstVideoCodecState * state)
{
  GstAmcVideoEnc *self;
  GstAmcFormat *format = NULL;
  GstCaps *allowed_caps = NULL;
  gboolean is_format_change = FALSE;
  gboolean needs_disable = FALSE;
  gchar *format_string;
  gboolean r = FALSE;
  GError *err = NULL;

  self = GST_AMC_VIDEO_ENC (encoder);

  GST_DEBUG_OBJECT (self, "Setting new caps %" GST_PTR_FORMAT, state->caps);

  /* Check if the caps change is a real format change or if only irrelevant
   * parts of the caps have changed or nothing at all.
   */
  is_format_change |= self->color_format_info.width != state->info.width;
  is_format_change |= self->color_format_info.height != state->info.height;
  needs_disable = self->started;

  /* If the component is not started and a real format change happens
   * we have to restart the component. If no real format change
   * happened we can just exit here.
   */
  if (needs_disable && !is_format_change) {

    /* Framerate or something minor changed */
    if (self->input_state)
      gst_video_codec_state_unref (self->input_state);
    self->input_state = gst_video_codec_state_ref (state);
    GST_DEBUG_OBJECT (self,
        "Already running and caps did not change the format");
    return TRUE;
  }

  if (needs_disable && is_format_change) {
    gst_amc_video_enc_drain (self);
    GST_VIDEO_ENCODER_STREAM_UNLOCK (self);
    gst_amc_video_enc_stop (GST_VIDEO_ENCODER (self));
    GST_VIDEO_ENCODER_STREAM_LOCK (self);
    gst_amc_video_enc_close (GST_VIDEO_ENCODER (self));
    if (!gst_amc_video_enc_open (GST_VIDEO_ENCODER (self))) {
      GST_ERROR_OBJECT (self, "Failed to open codec again");
      return FALSE;
    }

    if (!gst_amc_video_enc_start (GST_VIDEO_ENCODER (self))) {
      GST_ERROR_OBJECT (self, "Failed to start codec again");
    }
  }
  /* srcpad task is not running at this point */
  if (self->input_state)
    gst_video_codec_state_unref (self->input_state);
  self->input_state = NULL;

  GST_DEBUG_OBJECT (self, "picking an output format ...");
  allowed_caps = gst_pad_get_allowed_caps (GST_VIDEO_ENCODER_SRC_PAD (encoder));
  if (!allowed_caps) {
    GST_DEBUG_OBJECT (self, "... but no peer, using template caps");
    allowed_caps =
        gst_pad_get_pad_template_caps (GST_VIDEO_ENCODER_SRC_PAD (encoder));
  }
  GST_DEBUG_OBJECT (self, "chose caps %" GST_PTR_FORMAT, allowed_caps);
  allowed_caps = gst_caps_truncate (allowed_caps);

  format = create_amc_format (self, state, allowed_caps);
  if (!format)
    goto quit;

  format_string = gst_amc_format_to_string (format, &err);
  if (err)
    GST_ELEMENT_WARNING_FROM_ERROR (self, err);
  GST_DEBUG_OBJECT (self, "Configuring codec with format: %s",
      GST_STR_NULL (format_string));
  g_free (format_string);

  if (!gst_amc_codec_configure (self->codec, format, NULL, &err)) {
    GST_ERROR_OBJECT (self, "Failed to configure codec");
    GST_ELEMENT_ERROR_FROM_ERROR (self, err);
    goto quit;
  }

  if (!gst_amc_codec_start (self->codec, &err)) {
    GST_ERROR_OBJECT (self, "Failed to start codec");
    GST_ELEMENT_ERROR_FROM_ERROR (self, err);
    goto quit;
  }

  self->amc_format = format;
  format = NULL;

  self->input_state = gst_video_codec_state_ref (state);

  self->started = TRUE;

  /* Start the srcpad loop again */
  self->flushing = FALSE;
  self->downstream_flow_ret = GST_FLOW_OK;
  gst_pad_start_task (GST_VIDEO_ENCODER_SRC_PAD (self),
      (GstTaskFunction) gst_amc_video_enc_loop, encoder, NULL);

  r = TRUE;

quit:
  if (allowed_caps)
    gst_caps_unref (allowed_caps);

  if (format)
    gst_amc_format_free (format);

  return r;
}

static gboolean
gst_amc_video_enc_flush (GstVideoEncoder * encoder)
{
  GstAmcVideoEnc *self;
  GError *err = NULL;

  self = GST_AMC_VIDEO_ENC (encoder);

  GST_DEBUG_OBJECT (self, "Flushing encoder");

  if (!self->started) {
    GST_DEBUG_OBJECT (self, "Codec not started yet");
    return TRUE;
  }

  self->flushing = TRUE;
  gst_amc_codec_flush (self->codec, &err);
  if (err)
    GST_ELEMENT_WARNING_FROM_ERROR (self, err);

  /* Wait until the srcpad loop is finished,
   * unlock GST_VIDEO_ENCODER_STREAM_LOCK to prevent deadlocks
   * caused by using this lock from inside the loop function */
  GST_VIDEO_ENCODER_STREAM_UNLOCK (self);
  GST_PAD_STREAM_LOCK (GST_VIDEO_ENCODER_SRC_PAD (self));
  GST_PAD_STREAM_UNLOCK (GST_VIDEO_ENCODER_SRC_PAD (self));
  GST_VIDEO_ENCODER_STREAM_LOCK (self);
  self->flushing = FALSE;

  /* Start the srcpad loop again */
  self->last_upstream_ts = 0;
  self->drained = TRUE;
  self->downstream_flow_ret = GST_FLOW_OK;
  gst_pad_start_task (GST_VIDEO_ENCODER_SRC_PAD (self),
      (GstTaskFunction) gst_amc_video_enc_loop, encoder, NULL);

  GST_DEBUG_OBJECT (self, "Flush encoder");

  return TRUE;
}

static GstFlowReturn
gst_amc_video_enc_handle_frame (GstVideoEncoder * encoder,
    GstVideoCodecFrame * frame)
{
  GstAmcVideoEnc *self;
  gint idx;
  GstAmcBuffer *buf;
  GstAmcBufferInfo buffer_info;
  GstClockTime timestamp, duration, timestamp_offset = 0;
  BufferIdentification *id;
  GError *err = NULL;

  self = GST_AMC_VIDEO_ENC (encoder);

  GST_DEBUG_OBJECT (self, "Handling frame");

  if (!self->started) {
    GST_ERROR_OBJECT (self, "Codec not started yet");
    gst_video_codec_frame_unref (frame);
    return GST_FLOW_NOT_NEGOTIATED;
  }

  if (self->flushing)
    goto flushing;

  if (self->downstream_flow_ret != GST_FLOW_OK)
    goto downstream_error;

  timestamp = frame->pts;
  duration = frame->duration;

  if (GST_VIDEO_CODEC_FRAME_IS_FORCE_KEYFRAME (frame)) {
    if (gst_amc_codec_request_key_frame (self->codec, &err)) {
      GST_DEBUG_OBJECT (self, "Passed keyframe request to MediaCodec");
    }
    if (err) {
      GST_ELEMENT_WARNING_FROM_ERROR (self, err);
      g_clear_error (&err);
    }
  }

again:
  /* Make sure to release the base class stream lock, otherwise
   * _loop() can't call _finish_frame() and we might block forever
   * because no input buffers are released */
  GST_VIDEO_ENCODER_STREAM_UNLOCK (self);
  /* Wait at most 100ms here, some codecs don't fail dequeueing if
   * the codec is flushing, causing deadlocks during shutdown */
  idx = gst_amc_codec_dequeue_input_buffer (self->codec, 100000, &err);
  GST_VIDEO_ENCODER_STREAM_LOCK (self);

  if (idx < 0) {
    if (self->flushing || self->downstream_flow_ret == GST_FLOW_FLUSHING) {
      g_clear_error (&err);
      goto flushing;
    }

    switch (idx) {
      case INFO_TRY_AGAIN_LATER:
        GST_DEBUG_OBJECT (self, "Dequeueing input buffer timed out");
        goto again;             /* next try */
        break;
      case G_MININT:
        GST_ERROR_OBJECT (self, "Failed to dequeue input buffer");
        goto dequeue_error;
      default:
        g_assert_not_reached ();
        break;
    }

    goto again;
  }

  if (self->flushing) {
    memset (&buffer_info, 0, sizeof (buffer_info));
    gst_amc_codec_queue_input_buffer (self->codec, idx, &buffer_info, NULL);
    goto flushing;
  }

  if (self->downstream_flow_ret != GST_FLOW_OK) {
    memset (&buffer_info, 0, sizeof (buffer_info));
    gst_amc_codec_queue_input_buffer (self->codec, idx, &buffer_info, &err);
    if (err && !self->flushing)
      GST_ELEMENT_WARNING_FROM_ERROR (self, err);
    g_clear_error (&err);
    goto downstream_error;
  }

  /* Now handle the frame */

  /* Copy the buffer content in chunks of size as requested
   * by the port */
  buf = gst_amc_codec_get_input_buffer (self->codec, idx, &err);
  if (err)
    goto failed_to_get_input_buffer;
  else if (!buf)
    goto got_null_input_buffer;

  memset (&buffer_info, 0, sizeof (buffer_info));
  buffer_info.offset = 0;
  buffer_info.size = MIN (self->color_format_info.frame_size, buf->size);
  gst_amc_buffer_set_position_and_limit (buf, NULL, buffer_info.offset,
      buffer_info.size);

  if (!gst_amc_video_enc_fill_buffer (self, frame->input_buffer, buf,
          &buffer_info)) {
    memset (&buffer_info, 0, sizeof (buffer_info));
    gst_amc_codec_queue_input_buffer (self->codec, idx, &buffer_info, &err);
    if (err && !self->flushing)
      GST_ELEMENT_WARNING_FROM_ERROR (self, err);
    g_clear_error (&err);
    gst_amc_buffer_free (buf);
    buf = NULL;
    goto buffer_fill_error;
  }

  gst_amc_buffer_free (buf);
  buf = NULL;

  if (timestamp != GST_CLOCK_TIME_NONE) {
    buffer_info.presentation_time_us =
        gst_util_uint64_scale (timestamp + timestamp_offset, 1, GST_USECOND);
    self->last_upstream_ts = timestamp + timestamp_offset;
  }
  if (duration != GST_CLOCK_TIME_NONE)
    self->last_upstream_ts += duration;

  id = buffer_identification_new (timestamp + timestamp_offset);
  if (GST_VIDEO_CODEC_FRAME_IS_SYNC_POINT (frame))
    buffer_info.flags |= BUFFER_FLAG_SYNC_FRAME;
  gst_video_codec_frame_set_user_data (frame, id,
      (GDestroyNotify) buffer_identification_free);

  GST_DEBUG_OBJECT (self,
      "Queueing buffer %d: size %d time %" G_GINT64_FORMAT " flags 0x%08x",
      idx, buffer_info.size, buffer_info.presentation_time_us,
      buffer_info.flags);
  if (!gst_amc_codec_queue_input_buffer (self->codec, idx, &buffer_info, &err)) {
    if (self->flushing) {
      g_clear_error (&err);
      goto flushing;
    }
    goto queue_error;
  }

  self->drained = FALSE;

  gst_video_codec_frame_unref (frame);

  return self->downstream_flow_ret;

downstream_error:
  {
    GST_ERROR_OBJECT (self, "Downstream returned %s",
        gst_flow_get_name (self->downstream_flow_ret));

    gst_video_codec_frame_unref (frame);
    return self->downstream_flow_ret;
  }
failed_to_get_input_buffer:
  {
    GST_ELEMENT_ERROR_FROM_ERROR (self, err);
    gst_video_codec_frame_unref (frame);
    return GST_FLOW_ERROR;
  }
got_null_input_buffer:
  {
    GST_ELEMENT_ERROR (self, LIBRARY, SETTINGS, (NULL),
        ("Got no input buffer"));
    gst_video_codec_frame_unref (frame);
    return GST_FLOW_ERROR;
  }
buffer_fill_error:
  {
    GST_ELEMENT_ERROR (self, RESOURCE, WRITE, (NULL),
        ("Failed to write input into the amc buffer(write %dB to a %"
            G_GSIZE_FORMAT "B buffer)", self->color_format_info.frame_size,
            buf->size));
    gst_video_codec_frame_unref (frame);
    return GST_FLOW_ERROR;
  }
dequeue_error:
  {
    GST_ELEMENT_ERROR_FROM_ERROR (self, err);
    gst_video_codec_frame_unref (frame);
    return GST_FLOW_ERROR;
  }
queue_error:
  {
    GST_ELEMENT_ERROR_FROM_ERROR (self, err);
    gst_video_codec_frame_unref (frame);
    return GST_FLOW_ERROR;
  }
flushing:
  {
    GST_DEBUG_OBJECT (self, "Flushing -- returning FLUSHING");
    gst_video_codec_frame_unref (frame);
    return GST_FLOW_FLUSHING;
  }
}

static GstFlowReturn
gst_amc_video_enc_finish (GstVideoEncoder * encoder)
{
  GstAmcVideoEnc *self;

  self = GST_AMC_VIDEO_ENC (encoder);

  return gst_amc_video_enc_drain (self);
}

static GstFlowReturn
gst_amc_video_enc_drain (GstAmcVideoEnc * self)
{
  GstFlowReturn ret;
  gint idx;
  GError *err = NULL;

  GST_DEBUG_OBJECT (self, "Draining codec");
  if (!self->started) {
    GST_DEBUG_OBJECT (self, "Codec not started yet");
    return GST_FLOW_OK;
  }

  /* Don't send drain buffer twice, this doesn't work */
  if (self->drained) {
    GST_DEBUG_OBJECT (self, "Codec is drained already");
    return GST_FLOW_OK;
  }

  /* Make sure to release the base class stream lock, otherwise
   * _loop() can't call _finish_frame() and we might block forever
   * because no input buffers are released */
  GST_VIDEO_ENCODER_STREAM_UNLOCK (self);
  /* Send an EOS buffer to the component and let the base
   * class drop the EOS event. We will send it later when
   * the EOS buffer arrives on the output port.
   * Wait at most 0.5s here. */
  idx = gst_amc_codec_dequeue_input_buffer (self->codec, 500000, &err);
  GST_VIDEO_ENCODER_STREAM_LOCK (self);

  if (idx >= 0) {
    GstAmcBuffer *buf;
    GstAmcBufferInfo buffer_info;

    buf = gst_amc_codec_get_input_buffer (self->codec, idx, &err);
    if (buf) {
      GST_VIDEO_ENCODER_STREAM_UNLOCK (self);
      g_mutex_lock (&self->drain_lock);
      self->draining = TRUE;

      memset (&buffer_info, 0, sizeof (buffer_info));
      buffer_info.size = 0;
      buffer_info.presentation_time_us =
          gst_util_uint64_scale (self->last_upstream_ts, 1, GST_USECOND);
      buffer_info.flags |= BUFFER_FLAG_END_OF_STREAM;

      gst_amc_buffer_set_position_and_limit (buf, NULL, 0, 0);
      gst_amc_buffer_free (buf);
      buf = NULL;

      if (gst_amc_codec_queue_input_buffer (self->codec, idx, &buffer_info,
              &err)) {
        GST_DEBUG_OBJECT (self, "Waiting until codec is drained");
        g_cond_wait (&self->drain_cond, &self->drain_lock);
        GST_DEBUG_OBJECT (self, "Drained codec");
        ret = GST_FLOW_OK;
      } else {
        GST_ERROR_OBJECT (self, "Failed to queue input buffer");
        if (self->flushing) {
          g_clear_error (&err);
          ret = GST_FLOW_FLUSHING;
        } else {
          GST_ELEMENT_WARNING_FROM_ERROR (self, err);
          ret = GST_FLOW_ERROR;
        }
      }

      self->drained = TRUE;
      self->draining = FALSE;
      g_mutex_unlock (&self->drain_lock);
      GST_VIDEO_ENCODER_STREAM_LOCK (self);
    } else {
      GST_ERROR_OBJECT (self, "Failed to get buffer for EOS: %d", idx);
      if (err)
        GST_ELEMENT_WARNING_FROM_ERROR (self, err);
      ret = GST_FLOW_ERROR;
    }
  } else {
    GST_ERROR_OBJECT (self, "Failed to acquire buffer for EOS: %d", idx);
    if (err)
      GST_ELEMENT_WARNING_FROM_ERROR (self, err);
    ret = GST_FLOW_ERROR;
  }

  return ret;
}
