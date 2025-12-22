/* GStreamer AAC encoder plugin
 * Copyright (C) 2011 Kan Hu <kan.hu@linaro.org>
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
 * SECTION:element-voaacenc
 * @title: voaacenc
 *
 * AAC audio encoder based on vo-aacenc library.
 *
 * [vo-aacenc library source file](http://sourceforge.net/projects/opencore-amr/files/vo-aacenc/)
 *
 * ## Example launch line
 * |[
 * gst-launch-1.0 filesrc location=abc.wav ! wavparse ! audioresample ! audioconvert ! voaacenc ! filesink location=abc.aac
 * ]|
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include <gst/pbutils/codec-utils.h>

#include "gstvoaacenc.h"

#define VOAAC_ENC_DEFAULT_BITRATE (128000)
#define VOAAC_ENC_DEFAULT_OUTPUTFORMAT (0)      /* RAW */
#define VOAAC_ENC_MPEGVERSION (4)
#define VOAAC_ENC_CODECDATA_LEN (2)
#define VOAAC_ENC_BITS_PER_SAMPLE (16)

enum
{
  PROP_0,
  PROP_BITRATE
};

#define SAMPLE_RATES " 8000, " \
                    "11025, " \
                    "12000, " \
                    "16000, " \
                    "22050, " \
                    "24000, " \
                    "32000, " \
                    "44100, " \
                    "48000, " \
                    "64000, " \
                    "88200, " \
                    "96000"

/* voaacenc only supports 1 or 2 channels */
static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw, "
        "format = (string) " GST_AUDIO_NE (S16) ", "
        "layout = (string) interleaved, "
        "rate = (int) { " SAMPLE_RATES " }, " "channels = (int) 1;"
        "audio/x-raw, "
        "format = (string) " GST_AUDIO_NE (S16) ", "
        "layout = (string) interleaved, "
        "rate = (int) { " SAMPLE_RATES " }, " "channels = (int) 2, "
        "channel-mask=(bitmask)0x3")
    );

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/mpeg, "
        "mpegversion = (int) 4, "
        "rate = (int) { " SAMPLE_RATES " }, "
        "channels = (int) [1, 2], "
        "stream-format = (string) { adts, raw }, " "base-profile = (string) lc")
    );

GST_DEBUG_CATEGORY_STATIC (gst_voaacenc_debug);
#define GST_CAT_DEFAULT gst_voaacenc_debug

static gboolean voaacenc_core_init (GstVoAacEnc * voaacenc);
static gboolean voaacenc_core_set_parameter (GstVoAacEnc * voaacenc);
static void voaacenc_core_uninit (GstVoAacEnc * voaacenc);

static gboolean gst_voaacenc_start (GstAudioEncoder * enc);
static gboolean gst_voaacenc_stop (GstAudioEncoder * enc);
static gboolean gst_voaacenc_set_format (GstAudioEncoder * enc,
    GstAudioInfo * info);
static GstFlowReturn gst_voaacenc_handle_frame (GstAudioEncoder * enc,
    GstBuffer * in_buf);

G_DEFINE_TYPE (GstVoAacEnc, gst_voaacenc, GST_TYPE_AUDIO_ENCODER);
GST_ELEMENT_REGISTER_DEFINE (voaacenc, "voaacenc",
    GST_RANK_SECONDARY, GST_TYPE_VOAACENC);

static void
gst_voaacenc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstVoAacEnc *self = GST_VOAACENC (object);

  switch (prop_id) {
    case PROP_BITRATE:
      self->bitrate = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  return;
}

static void
gst_voaacenc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstVoAacEnc *self = GST_VOAACENC (object);

  switch (prop_id) {
    case PROP_BITRATE:
      g_value_set_int (value, self->bitrate);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  return;
}

static void
gst_voaacenc_class_init (GstVoAacEncClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstAudioEncoderClass *base_class = GST_AUDIO_ENCODER_CLASS (klass);

  object_class->set_property = GST_DEBUG_FUNCPTR (gst_voaacenc_set_property);
  object_class->get_property = GST_DEBUG_FUNCPTR (gst_voaacenc_get_property);

  base_class->start = GST_DEBUG_FUNCPTR (gst_voaacenc_start);
  base_class->stop = GST_DEBUG_FUNCPTR (gst_voaacenc_stop);
  base_class->set_format = GST_DEBUG_FUNCPTR (gst_voaacenc_set_format);
  base_class->handle_frame = GST_DEBUG_FUNCPTR (gst_voaacenc_handle_frame);

  g_object_class_install_property (object_class, PROP_BITRATE,
      g_param_spec_int ("bitrate",
          "Bitrate",
          "Target Audio Bitrate (bits per second)",
          0, 320000, VOAAC_ENC_DEFAULT_BITRATE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_add_static_pad_template (element_class, &sink_template);
  gst_element_class_add_static_pad_template (element_class, &src_template);

  gst_element_class_set_static_metadata (element_class, "AAC audio encoder",
      "Codec/Encoder/Audio", "AAC audio encoder", "Kan Hu <kan.hu@linaro.org>");

  GST_DEBUG_CATEGORY_INIT (gst_voaacenc_debug, "voaacenc", 0, "voaac encoder");
}

static void
gst_voaacenc_init (GstVoAacEnc * voaacenc)
{
  GST_PAD_SET_ACCEPT_TEMPLATE (GST_AUDIO_ENCODER_SINK_PAD (voaacenc));
  voaacenc->bitrate = VOAAC_ENC_DEFAULT_BITRATE;
  voaacenc->output_format = VOAAC_ENC_DEFAULT_OUTPUTFORMAT;

  /* init rest */
  voaacenc->handle = NULL;
}

static gboolean
gst_voaacenc_start (GstAudioEncoder * enc)
{
  GstVoAacEnc *voaacenc = GST_VOAACENC (enc);

  GST_DEBUG_OBJECT (enc, "start");

  if (voaacenc_core_init (voaacenc) == FALSE)
    return FALSE;

  voaacenc->rate = 0;
  voaacenc->channels = 0;

  return TRUE;
}

static gboolean
gst_voaacenc_stop (GstAudioEncoder * enc)
{
  GstVoAacEnc *voaacenc = GST_VOAACENC (enc);

  GST_DEBUG_OBJECT (enc, "stop");
  voaacenc_core_uninit (voaacenc);

  return TRUE;
}

#define VOAAC_ENC_MAX_CHANNELS 6
/* describe the channels position */
static const GstAudioChannelPosition
    aac_channel_positions[][VOAAC_ENC_MAX_CHANNELS] = {
  {                             /* 1 ch: Mono */
      GST_AUDIO_CHANNEL_POSITION_MONO},
  {                             /* 2 ch: front left + front right (front stereo) */
        GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT,
      GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT},
  {                             /* 3 ch: front center + front stereo */
        GST_AUDIO_CHANNEL_POSITION_FRONT_CENTER,
        GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT,
      GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT},
  {                             /* 4 ch: front center + front stereo + back center */
        GST_AUDIO_CHANNEL_POSITION_FRONT_CENTER,
        GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT,
        GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT,
      GST_AUDIO_CHANNEL_POSITION_REAR_CENTER},
  {                             /* 5 ch: front center + front stereo + back stereo */
        GST_AUDIO_CHANNEL_POSITION_FRONT_CENTER,
        GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT,
        GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT,
        GST_AUDIO_CHANNEL_POSITION_REAR_LEFT,
      GST_AUDIO_CHANNEL_POSITION_REAR_RIGHT},
  {                             /* 6ch: front center + front stereo + back stereo + LFE */
        GST_AUDIO_CHANNEL_POSITION_FRONT_CENTER,
        GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT,
        GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT,
        GST_AUDIO_CHANNEL_POSITION_REAR_LEFT,
        GST_AUDIO_CHANNEL_POSITION_REAR_RIGHT,
      GST_AUDIO_CHANNEL_POSITION_LFE1}
};

/* check downstream caps to configure format */
static void
gst_voaacenc_negotiate (GstVoAacEnc * voaacenc)
{
  GstCaps *caps;

  caps = gst_pad_get_allowed_caps (GST_AUDIO_ENCODER_SRC_PAD (voaacenc));

  GST_DEBUG_OBJECT (voaacenc, "allowed caps: %" GST_PTR_FORMAT, caps);

  if (caps && gst_caps_get_size (caps) > 0) {
    GstStructure *s = gst_caps_get_structure (caps, 0);
    const gchar *str = NULL;

    if ((str = gst_structure_get_string (s, "stream-format"))) {
      if (strcmp (str, "adts") == 0) {
        GST_DEBUG_OBJECT (voaacenc, "use ADTS format for output");
        voaacenc->output_format = 1;
      } else if (strcmp (str, "raw") == 0) {
        GST_DEBUG_OBJECT (voaacenc, "use RAW format for output");
        voaacenc->output_format = 0;
      } else {
        GST_DEBUG_OBJECT (voaacenc, "unknown stream-format: %s", str);
        voaacenc->output_format = VOAAC_ENC_DEFAULT_OUTPUTFORMAT;
      }
    }
  }

  if (caps)
    gst_caps_unref (caps);
}

static gint
gst_voaacenc_get_rate_index (gint rate)
{
  static const gint rate_table[] = {
    96000, 88200, 64000, 48000, 44100, 32000,
    24000, 22050, 16000, 12000, 11025, 8000
  };
  gint i;
  for (i = 0; i < G_N_ELEMENTS (rate_table); ++i) {
    if (rate == rate_table[i]) {
      return i;
    }
  }
  return -1;
}

static GstCaps *
gst_voaacenc_create_source_pad_caps (GstVoAacEnc * voaacenc)
{
  GstCaps *caps = NULL;
  gint index;
  GstBuffer *codec_data;
  GstMapInfo map;

  if ((index = gst_voaacenc_get_rate_index (voaacenc->rate)) >= 0) {
    codec_data = gst_buffer_new_and_alloc (VOAAC_ENC_CODECDATA_LEN);
    gst_buffer_map (codec_data, &map, GST_MAP_WRITE);
    /* LC profile only */
    map.data[0] = ((0x02 << 3) | (index >> 1));
    map.data[1] = ((index & 0x01) << 7) | (voaacenc->channels << 3);

    caps = gst_caps_new_simple ("audio/mpeg",
        "mpegversion", G_TYPE_INT, VOAAC_ENC_MPEGVERSION,
        "channels", G_TYPE_INT, voaacenc->channels,
        "rate", G_TYPE_INT, voaacenc->rate, NULL);

    gst_codec_utils_aac_caps_set_level_and_profile (caps, map.data,
        VOAAC_ENC_CODECDATA_LEN);
    gst_buffer_unmap (codec_data, &map);

    if (!voaacenc->output_format) {
      gst_caps_set_simple (caps,
          "stream-format", G_TYPE_STRING, "raw",
          "codec_data", GST_TYPE_BUFFER, codec_data, NULL);
    } else {
      gst_caps_set_simple (caps,
          "stream-format", G_TYPE_STRING, "adts",
          "framed", G_TYPE_BOOLEAN, TRUE, NULL);
    }
    gst_buffer_unref (codec_data);
  }

  return caps;
}

static gboolean
gst_voaacenc_set_format (GstAudioEncoder * benc, GstAudioInfo * info)
{
  gboolean ret = FALSE;
  GstVoAacEnc *voaacenc;
  GstCaps *src_caps;

  voaacenc = GST_VOAACENC (benc);

  /* get channel count */
  voaacenc->channels = GST_AUDIO_INFO_CHANNELS (info);
  voaacenc->rate = GST_AUDIO_INFO_RATE (info);

  /* precalc buffer size as it's constant now */
  voaacenc->inbuf_size = voaacenc->channels * 2 * 1024;

  gst_voaacenc_negotiate (voaacenc);

  /* create reverse caps */
  src_caps = gst_voaacenc_create_source_pad_caps (voaacenc);

  if (src_caps) {
    gst_audio_encoder_set_output_format (GST_AUDIO_ENCODER (voaacenc),
        src_caps);
    gst_caps_unref (src_caps);
    ret = voaacenc_core_set_parameter (voaacenc);
  }

  /* report needs to base class */
  gst_audio_encoder_set_frame_samples_min (benc, 1024);
  gst_audio_encoder_set_frame_samples_max (benc, 1024);
  gst_audio_encoder_set_frame_max (benc, 1);

  return ret;
}

static GstFlowReturn
gst_voaacenc_handle_frame (GstAudioEncoder * benc, GstBuffer * buf)
{
  GstVoAacEnc *voaacenc;
  GstFlowReturn ret = GST_FLOW_OK;
  GstBuffer *out;
  VO_AUDIO_OUTPUTINFO output_info = { {0} };
  VO_CODECBUFFER input = { 0 };
  VO_CODECBUFFER output = { 0 };
  GstMapInfo map, omap;
  GstAudioInfo *info = gst_audio_encoder_get_audio_info (benc);

  voaacenc = GST_VOAACENC (benc);

  g_return_val_if_fail (voaacenc->handle, GST_FLOW_NOT_NEGOTIATED);

  /* we don't deal with squeezing remnants, so simply discard those */
  if (G_UNLIKELY (buf == NULL)) {
    GST_DEBUG_OBJECT (benc, "no data");
    goto exit;
  }

  if (memcmp (info->position, aac_channel_positions[info->channels - 1],
          sizeof (GstAudioChannelPosition) * info->channels) != 0) {
    buf = gst_buffer_make_writable (buf);
    gst_audio_buffer_reorder_channels (buf, info->finfo->format,
        info->channels, info->position,
        aac_channel_positions[info->channels - 1]);
  }

  gst_buffer_map (buf, &map, GST_MAP_READ);

  if (G_UNLIKELY (map.size < voaacenc->inbuf_size)) {
    gst_buffer_unmap (buf, &map);
    GST_DEBUG_OBJECT (voaacenc, "discarding trailing data %d", (gint) map.size);
    ret = gst_audio_encoder_finish_frame (benc, NULL, -1);
    goto exit;
  }

  /* max size */
  out = gst_buffer_new_and_alloc (voaacenc->inbuf_size);
  gst_buffer_map (out, &omap, GST_MAP_WRITE);

  output.Buffer = omap.data;
  output.Length = voaacenc->inbuf_size;

  g_assert (map.size == voaacenc->inbuf_size);
  input.Buffer = map.data;
  input.Length = voaacenc->inbuf_size;
  voaacenc->codec_api.SetInputData (voaacenc->handle, &input);

  /* encode */
  if (voaacenc->codec_api.GetOutputData (voaacenc->handle, &output,
          &output_info) != VO_ERR_NONE) {
    gst_buffer_unmap (buf, &map);
    gst_buffer_unmap (out, &omap);
    gst_buffer_unref (out);
    goto encode_failed;
  }

  GST_LOG_OBJECT (voaacenc, "encoded to %lu bytes", output.Length);
  gst_buffer_unmap (buf, &map);
  gst_buffer_unmap (out, &omap);
  gst_buffer_resize (out, 0, output.Length);

  ret = gst_audio_encoder_finish_frame (benc, out, 1024);

exit:
  return ret;

  /* ERRORS */
encode_failed:
  {
    GST_ELEMENT_ERROR (voaacenc, STREAM, ENCODE, (NULL), ("encode failed"));
    ret = GST_FLOW_ERROR;
    goto exit;
  }
}

static VO_U32
voaacenc_core_mem_alloc (VO_S32 uID, VO_MEM_INFO * pMemInfo)
{
  if (!pMemInfo)
    return VO_ERR_INVALID_ARG;

  pMemInfo->VBuffer = g_malloc (pMemInfo->Size);
  return 0;
}

static VO_U32
voaacenc_core_mem_free (VO_S32 uID, VO_PTR pMem)
{
  g_free (pMem);
  return 0;
}

static VO_U32
voaacenc_core_mem_set (VO_S32 uID, VO_PTR pBuff, VO_U8 uValue, VO_U32 uSize)
{
  memset (pBuff, uValue, uSize);
  return 0;
}

static VO_U32
voaacenc_core_mem_copy (VO_S32 uID, VO_PTR pDest, VO_PTR pSource, VO_U32 uSize)
{
  memcpy (pDest, pSource, uSize);
  return 0;
}

static VO_U32
voaacenc_core_mem_check (VO_S32 uID, VO_PTR pBuffer, VO_U32 uSize)
{
  return 0;
}

static gboolean
voaacenc_core_init (GstVoAacEnc * voaacenc)
{
  VO_CODEC_INIT_USERDATA user_data = { 0 };
  voGetAACEncAPI (&voaacenc->codec_api);

  voaacenc->mem_operator.Alloc = voaacenc_core_mem_alloc;
  voaacenc->mem_operator.Copy = voaacenc_core_mem_copy;
  voaacenc->mem_operator.Free = voaacenc_core_mem_free;
  voaacenc->mem_operator.Set = voaacenc_core_mem_set;
  voaacenc->mem_operator.Check = voaacenc_core_mem_check;
  user_data.memflag = VO_IMF_USERMEMOPERATOR;
  user_data.memData = &voaacenc->mem_operator;
  voaacenc->codec_api.Init (&voaacenc->handle, VO_AUDIO_CodingAAC, &user_data);

  if (voaacenc->handle == NULL) {
    return FALSE;
  }
  return TRUE;

}

static gboolean
voaacenc_core_set_parameter (GstVoAacEnc * voaacenc)
{
  AACENC_PARAM params = { 0 };
  guint32 ret;

  params.sampleRate = voaacenc->rate;
  params.bitRate = voaacenc->bitrate;
  params.nChannels = voaacenc->channels;
  if (voaacenc->output_format) {
    params.adtsUsed = 1;
  } else {
    params.adtsUsed = 0;
  }

  ret =
      voaacenc->codec_api.SetParam (voaacenc->handle, VO_PID_AAC_ENCPARAM,
      &params);
  if (ret != VO_ERR_NONE) {
    GST_ERROR_OBJECT (voaacenc, "Failed to set encoder parameters");
    return FALSE;
  }
  return TRUE;
}

static void
voaacenc_core_uninit (GstVoAacEnc * voaacenc)
{
  if (voaacenc->handle) {
    voaacenc->codec_api.Uninit (voaacenc->handle);
    voaacenc->handle = NULL;
  }
}
