/*  GStreamer LDAC audio encoder
 *  Copyright (C) 2020 Asymptotic <sanchayan@asymptotic.io>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

/**
 * SECTION:element-ldacenc
 * @title: ldacenc
 *
 * This element encodes raw integer PCM audio into a Bluetooth LDAC audio.
 *
 * ## Example pipeline
 * |[
 * gst-launch-1.0 -v audiotestsrc ! ldacenc ! rtpldacpay mtu=679 ! avdtpsink
 * ]| Encode a sine wave into LDAC, RTP payload it and send over bluetooth
 *
 * Since: 1.20
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include "gstldacenc.h"

/*
 * MTU size required for LDAC A2DP streaming. Required for initializing the
 * encoder.
 */
#define GST_LDAC_MTU_REQUIRED    679

GST_DEBUG_CATEGORY_STATIC (ldac_enc_debug);
#define GST_CAT_DEFAULT ldac_enc_debug

#define parent_class gst_ldac_enc_parent_class
G_DEFINE_TYPE (GstLdacEnc, gst_ldac_enc, GST_TYPE_AUDIO_ENCODER);
GST_ELEMENT_REGISTER_DEFINE (ldacenc, "ldacenc", GST_RANK_NONE,
    GST_TYPE_LDAC_ENC);

#define SAMPLE_RATES "44100, 48000, 88200, 96000"

static GstStaticPadTemplate ldac_enc_sink_factory =
GST_STATIC_PAD_TEMPLATE ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
    GST_STATIC_CAPS
    ("audio/x-raw, format=(string) { S16LE, S24LE, S32LE, F32LE }, "
        "rate = (int) { " SAMPLE_RATES " }, channels = (int) [ 1, 2 ] "));

static GstStaticPadTemplate ldac_enc_src_factory =
    GST_STATIC_PAD_TEMPLATE ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-ldac, "
        "rate = (int) { " SAMPLE_RATES " }, "
        "channels = (int) 1, channel-mode = (string)mono; "
        "audio/x-ldac, "
        "rate = (int) { " SAMPLE_RATES " }, "
        "channels = (int) 2, channel-mode = (string) { dual, stereo }"));

enum
{
  PROP_0,
  PROP_EQMID
};

static void gst_ldac_enc_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_ldac_enc_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);

static gboolean gst_ldac_enc_start (GstAudioEncoder * enc);
static gboolean gst_ldac_enc_stop (GstAudioEncoder * enc);
static gboolean gst_ldac_enc_set_format (GstAudioEncoder * enc,
    GstAudioInfo * info);
static gboolean gst_ldac_enc_negotiate (GstAudioEncoder * enc);
static GstFlowReturn gst_ldac_enc_handle_frame (GstAudioEncoder * enc,
    GstBuffer * buffer);
static guint gst_ldac_enc_get_num_frames (guint eqmid, guint channels);
static guint gst_ldac_enc_get_frame_length (guint eqmid, guint channels);
static guint gst_ldac_enc_get_num_samples (guint rate);

#define GST_LDAC_EQMID (gst_ldac_eqmid_get_type ())
static GType
gst_ldac_eqmid_get_type (void)
{
  static GType ldac_eqmid_type = 0;
  static const GEnumValue eqmid_types[] = {
    {GST_LDAC_EQMID_HQ, "HQ", "hq"},
    {GST_LDAC_EQMID_SQ, "SQ", "sq"},
    {GST_LDAC_EQMID_MQ, "MQ", "mq"},
    {0, NULL, NULL}
  };

  if (!ldac_eqmid_type)
    ldac_eqmid_type = g_enum_register_static ("GstLdacEqmid", eqmid_types);

  return ldac_eqmid_type;
}

static void
gst_ldac_enc_class_init (GstLdacEncClass * klass)
{
  GstAudioEncoderClass *encoder_class = GST_AUDIO_ENCODER_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->set_property = gst_ldac_enc_set_property;
  gobject_class->get_property = gst_ldac_enc_get_property;

  encoder_class->start = GST_DEBUG_FUNCPTR (gst_ldac_enc_start);
  encoder_class->stop = GST_DEBUG_FUNCPTR (gst_ldac_enc_stop);
  encoder_class->set_format = GST_DEBUG_FUNCPTR (gst_ldac_enc_set_format);
  encoder_class->handle_frame = GST_DEBUG_FUNCPTR (gst_ldac_enc_handle_frame);
  encoder_class->negotiate = GST_DEBUG_FUNCPTR (gst_ldac_enc_negotiate);

  g_object_class_install_property (gobject_class, PROP_EQMID,
      g_param_spec_enum ("eqmid", "Encode Quality Mode Index",
          "Encode Quality Mode Index. 0: High Quality 1: Standard Quality "
          "2: Mobile Use Quality", GST_LDAC_EQMID,
          GST_LDAC_EQMID_SQ, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_add_static_pad_template (element_class,
      &ldac_enc_sink_factory);
  gst_element_class_add_static_pad_template (element_class,
      &ldac_enc_src_factory);

  gst_element_class_set_static_metadata (element_class,
      "Bluetooth LDAC audio encoder", "Codec/Encoder/Audio",
      "Encode an LDAC audio stream",
      "Sanchayan Maity <sanchayan@asymptotic.io>");

  GST_DEBUG_CATEGORY_INIT (ldac_enc_debug, "ldacenc", 0,
      "LDAC encoding element");
}

static void
gst_ldac_enc_init (GstLdacEnc * self)
{
  GST_PAD_SET_ACCEPT_TEMPLATE (GST_AUDIO_ENCODER_SINK_PAD (self));
  self->eqmid = GST_LDAC_EQMID_SQ;
  self->channel_mode = 0;
  self->init_done = FALSE;
}

static void
gst_ldac_enc_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstLdacEnc *self = GST_LDAC_ENC (object);

  switch (property_id) {
    case PROP_EQMID:
      self->eqmid = g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
gst_ldac_enc_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstLdacEnc *self = GST_LDAC_ENC (object);

  switch (property_id) {
    case PROP_EQMID:
      g_value_set_enum (value, self->eqmid);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static GstCaps *
gst_ldac_enc_do_negotiate (GstAudioEncoder * audio_enc)
{
  GstLdacEnc *enc = GST_LDAC_ENC (audio_enc);
  GstCaps *caps, *filter_caps;
  GstCaps *output_caps = NULL;
  GstStructure *s;

  /* Negotiate output format based on downstream caps restrictions */
  caps = gst_pad_get_allowed_caps (GST_AUDIO_ENCODER_SRC_PAD (enc));

  if (caps == NULL)
    caps = gst_static_pad_template_get_caps (&ldac_enc_src_factory);
  else if (gst_caps_is_empty (caps))
    goto failure;

  /* Fixate output caps */
  filter_caps = gst_caps_new_simple ("audio/x-ldac", "rate", G_TYPE_INT,
      enc->rate, "channels", G_TYPE_INT, enc->channels, NULL);
  output_caps = gst_caps_intersect (caps, filter_caps);
  gst_caps_unref (filter_caps);

  if (output_caps == NULL || gst_caps_is_empty (output_caps)) {
    GST_WARNING_OBJECT (enc, "Couldn't negotiate output caps with input rate "
        "%d and input channels %d and allowed output caps %" GST_PTR_FORMAT,
        enc->rate, enc->channels, caps);
    goto failure;
  }

  gst_clear_caps (&caps);

  GST_DEBUG_OBJECT (enc, "fixating caps %" GST_PTR_FORMAT, output_caps);
  output_caps = gst_caps_truncate (output_caps);
  s = gst_caps_get_structure (output_caps, 0);
  if (enc->channels == 1)
    gst_structure_fixate_field_string (s, "channel-mode", "mono");
  else
    gst_structure_fixate_field_string (s, "channel-mode", "stereo");
  s = NULL;

  /* In case there's anything else left to fixate */
  output_caps = gst_caps_fixate (output_caps);
  gst_caps_set_simple (output_caps, "framed", G_TYPE_BOOLEAN, TRUE, NULL);

  /* Set EQMID in caps to be used downstream by rtpldacpay */
  gst_caps_set_simple (output_caps, "eqmid", G_TYPE_INT, enc->eqmid, NULL);

  GST_INFO_OBJECT (enc, "output caps %" GST_PTR_FORMAT, output_caps);

  if (enc->channels == 1)
    enc->channel_mode = LDACBT_CHANNEL_MODE_MONO;
  else
    enc->channel_mode = LDACBT_CHANNEL_MODE_STEREO;

  return output_caps;

failure:
  if (output_caps)
    gst_caps_unref (output_caps);
  if (caps)
    gst_caps_unref (caps);
  return NULL;
}

static gboolean
gst_ldac_enc_negotiate (GstAudioEncoder * audio_enc)
{
  GstLdacEnc *enc = GST_LDAC_ENC (audio_enc);
  GstCaps *output_caps = NULL;

  output_caps = gst_ldac_enc_do_negotiate (audio_enc);
  if (output_caps == NULL) {
    GST_ERROR_OBJECT (enc, "failed to negotiate");
    return FALSE;
  }

  if (!gst_audio_encoder_set_output_format (audio_enc, output_caps)) {
    GST_ERROR_OBJECT (enc, "failed to configure output caps on src pad");
    gst_caps_unref (output_caps);
    return FALSE;
  }
  gst_caps_unref (output_caps);

  return GST_AUDIO_ENCODER_CLASS (parent_class)->negotiate (audio_enc);
}

static gboolean
gst_ldac_enc_set_format (GstAudioEncoder * audio_enc, GstAudioInfo * info)
{
  GstLdacEnc *enc = GST_LDAC_ENC (audio_enc);
  GstCaps *output_caps = NULL;
  guint num_ldac_frames, num_samples;
  gint ret = 0;

  enc->rate = GST_AUDIO_INFO_RATE (info);
  enc->channels = GST_AUDIO_INFO_CHANNELS (info);

  switch (GST_AUDIO_INFO_FORMAT (info)) {
    case GST_AUDIO_FORMAT_S16:
      enc->ldac_fmt = LDACBT_SMPL_FMT_S16;
      break;
    case GST_AUDIO_FORMAT_S24:
      enc->ldac_fmt = LDACBT_SMPL_FMT_S24;
      break;
    case GST_AUDIO_FORMAT_S32:
      enc->ldac_fmt = LDACBT_SMPL_FMT_S32;
      break;
    case GST_AUDIO_FORMAT_F32:
      enc->ldac_fmt = LDACBT_SMPL_FMT_F32;
      break;
    default:
      GST_ERROR_OBJECT (enc, "Invalid audio format");
      return FALSE;
  }

  output_caps = gst_ldac_enc_do_negotiate (audio_enc);
  if (output_caps == NULL) {
    GST_ERROR_OBJECT (enc, "failed to negotiate");
    return FALSE;
  }

  if (!gst_audio_encoder_set_output_format (audio_enc, output_caps)) {
    GST_ERROR_OBJECT (enc, "failed to configure output caps on src pad");
    gst_caps_unref (output_caps);
    return FALSE;
  }
  gst_caps_unref (output_caps);

  num_samples = gst_ldac_enc_get_num_samples (enc->rate);
  num_ldac_frames = gst_ldac_enc_get_num_frames (enc->eqmid, enc->channels);
  gst_audio_encoder_set_frame_samples_min (audio_enc,
      num_samples * num_ldac_frames);

  /*
   * If initialisation was already done means caps have changed, close the
   * handle. Closed handle can be initialised and used again.
   */
  if (enc->init_done) {
    ldacBT_close_handle (enc->ldac);
    enc->init_done = FALSE;
  }

  /*
   * libldac exposes a bluetooth centric API and emits multiple LDAC frames
   * depending on the MTU. The MTU is required for LDAC A2DP streaming, is
   * inclusive of the RTP header and is required by the encoder. The internal
   * encoder API is not exposed in the public interface.
   */
  ret =
      ldacBT_init_handle_encode (enc->ldac, GST_LDAC_MTU_REQUIRED, enc->eqmid,
      enc->channel_mode, enc->ldac_fmt, enc->rate);
  if (ret != 0) {
    GST_ERROR_OBJECT (enc, "Failed to initialize LDAC handle, ret: %d", ret);
    return FALSE;
  }
  enc->init_done = TRUE;

  return TRUE;
}

static GstFlowReturn
gst_ldac_enc_handle_frame (GstAudioEncoder * audio_enc, GstBuffer * buffer)
{
  GstLdacEnc *enc = GST_LDAC_ENC (audio_enc);
  GstMapInfo in_map, out_map;
  GstAudioInfo *info;
  GstBuffer *outbuf;
  const guint8 *in_data;
  guint8 *out_data;
  gint encoded, to_encode = 0;
  gint samples_consumed = 0;
  guint frames, frame_len;
  guint ldac_enc_read = 0;
  guint frame_count = 0;

  if (buffer == NULL)
    return GST_FLOW_OK;

  if (!gst_buffer_map (buffer, &in_map, GST_MAP_READ)) {
    GST_ELEMENT_ERROR (audio_enc, STREAM, FAILED, (NULL),
        ("Failed to map data from input buffer"));
    return GST_FLOW_ERROR;
  }

  info = gst_audio_encoder_get_audio_info (audio_enc);
  ldac_enc_read = LDACBT_ENC_LSU * info->bpf;
  /*
   * We may produce extra frames at the end of encoding process (See below).
   * Consider some additional frames while allocating output buffer if this
   * happens.
   */
  frames = (in_map.size / ldac_enc_read) + 4;

  frame_len = gst_ldac_enc_get_frame_length (enc->eqmid, info->channels);
  outbuf = gst_audio_encoder_allocate_output_buffer (audio_enc,
      frames * frame_len);
  if (outbuf == NULL)
    goto no_buffer;

  gst_buffer_map (outbuf, &out_map, GST_MAP_WRITE);
  in_data = in_map.data;
  out_data = out_map.data;
  to_encode = in_map.size;

  /*
   * ldacBT_encode does not generate an output frame each time it is called.
   * For each invocation, it consumes number of sample * bpf bytes of data.
   * Depending on the eqmid setting and channels, it will emit multiple frames
   * only after the required number of frames are packed for payloading. Below
   * for loop exists primarily to handle this.
   */
  for (;;) {
    guint8 pcm[LDACBT_MAX_LSU * 4 /* bytes/sample */  * 2 /* ch */ ] = { 0 };
    gint ldac_frame_num, written;
    guint8 *inp_data = NULL;
    gboolean done = FALSE;
    gint ret;

    /*
     * Even with minimum frame samples specified in set_format with EOS,
     * we may get a buffer which is not a multiple of LDACBT_ENC_LSU. LDAC
     * encoder always reads a multiple of this and to handle this scenario
     * we use local PCM array and in the last iteration when buffer bytes
     * < LDACBT_ENC_LSU * bpf, we copy only to_encode bytes to prevent
     * walking off the end of input buffer and the rest of the bytes in
     * PCM buffer would be zero, so should be safe from encoding point of
     * view.
     */
    if (to_encode < 0) {
      /*
       * We got < LDACBT_ENC_LSU * bpf for last iteration. Force the encoder
       * to encode the remaining bytes in buffer by passing NULL to the input
       * PCM buffer argument.
       */
      inp_data = NULL;
      done = TRUE;
    } else if (to_encode >= ldac_enc_read) {
      memcpy (pcm, in_data, ldac_enc_read);
      inp_data = &pcm[0];
    } else if (to_encode > 0 && to_encode < ldac_enc_read) {
      memcpy (pcm, in_data, to_encode);
      inp_data = &pcm[0];
    }

    /*
     * Note that while we do not explicitly pass length of data to library
     * anywhere, based on the initialization considering eqmid and rate, the
     * library will consume a fix number of samples per call. This combined
     * with the previous step ensures that the library does not read outside
     * of in_data and out_data.
     */
    ret = ldacBT_encode (enc->ldac, (void *) inp_data, &encoded,
        (guint8 *) out_data, &written, &ldac_frame_num);
    if (ret < 0) {
      GST_ELEMENT_ERROR (enc, STREAM, ENCODE, (NULL),
          ("encoding error, ret = %d written = %d", ret, ldac_frame_num));
      goto encoding_error;
    } else {
      to_encode -= encoded;
      in_data = in_data + encoded;
      out_data = out_data + written;
      frame_count += ldac_frame_num;

      GST_LOG_OBJECT (enc,
          "To Encode: %d, Encoded: %d, Written: %d, LDAC Frames: %d", to_encode,
          encoded, written, ldac_frame_num);

      if (done || (to_encode == 0 && encoded == ldac_enc_read))
        break;
    }
  }

  gst_buffer_unmap (outbuf, &out_map);

  if (frame_count > 0) {
    samples_consumed = in_map.size / info->bpf;
    gst_buffer_set_size (outbuf, frame_count * frame_len);
  } else {
    samples_consumed = 0;
    gst_buffer_replace (&outbuf, NULL);
  }

  gst_buffer_unmap (buffer, &in_map);

  return gst_audio_encoder_finish_frame (audio_enc, outbuf, samples_consumed);

no_buffer:
  {
    gst_buffer_unmap (buffer, &in_map);

    GST_ELEMENT_ERROR (enc, STREAM, FAILED, (NULL),
        ("could not allocate output buffer"));

    return GST_FLOW_ERROR;
  }
encoding_error:
  {
    gst_buffer_unmap (buffer, &in_map);

    ldacBT_free_handle (enc->ldac);

    enc->ldac = NULL;

    return GST_FLOW_ERROR;
  }
}

static gboolean
gst_ldac_enc_start (GstAudioEncoder * audio_enc)
{
  GstLdacEnc *enc = GST_LDAC_ENC (audio_enc);

  GST_INFO_OBJECT (enc, "Setup LDAC codec");
  /* Note that this only allocates the LDAC handle */
  enc->ldac = ldacBT_get_handle ();
  if (enc->ldac == NULL) {
    GST_ERROR_OBJECT (enc, "Failed to get LDAC handle");
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_ldac_enc_stop (GstAudioEncoder * audio_enc)
{
  GstLdacEnc *enc = GST_LDAC_ENC (audio_enc);

  GST_INFO_OBJECT (enc, "Finish LDAC codec");

  if (enc->ldac) {
    ldacBT_free_handle (enc->ldac);
    enc->ldac = NULL;
  }

  enc->eqmid = GST_LDAC_EQMID_SQ;
  enc->channel_mode = 0;
  enc->init_done = FALSE;

  return TRUE;
}

/**
 * gst_ldac_enc_get_frame_length
 * @eqmid: Encode Quality Mode Index
 * @channels: Number of channels
 *
 * Returns: Frame length.
 */
static guint
gst_ldac_enc_get_frame_length (guint eqmid, guint channels)
{
  g_assert (channels == 1 || channels == 2);

  switch (eqmid) {
      /* Encode setting for High Quality */
    case GST_LDAC_EQMID_HQ:
      return 165 * channels;
      /* Encode setting for Standard Quality */
    case GST_LDAC_EQMID_SQ:
      return 110 * channels;
      /* Encode setting for Mobile use Quality */
    case GST_LDAC_EQMID_MQ:
      return 55 * channels;
    default:
      break;
  }

  g_assert_not_reached ();

  /* If assertion gets compiled out */
  return 110 * channels;
}

/**
 * gst_ldac_enc_get_num_frames
 * @eqmid: Encode Quality Mode Index
 * @channels: Number of channels
 *
 * Returns: Number of LDAC frames per packet.
 */
static guint
gst_ldac_enc_get_num_frames (guint eqmid, guint channels)
{
  g_assert (channels == 1 || channels == 2);

  switch (eqmid) {
      /* Encode setting for High Quality */
    case GST_LDAC_EQMID_HQ:
      return 4 / channels;
      /* Encode setting for Standard Quality */
    case GST_LDAC_EQMID_SQ:
      return 6 / channels;
      /* Encode setting for Mobile use Quality */
    case GST_LDAC_EQMID_MQ:
      return 12 / channels;
    default:
      break;
  }

  g_assert_not_reached ();

  /* If assertion gets compiled out */
  return 6 / channels;
}

/**
 * gst_ldac_enc_get_num_samples
 * @rate: Sampling rate
 *
 * Number of samples in input PCM signal for encoding is fixed to
 * LDACBT_ENC_LSU viz. 128 samples/channel and it is not affected
 * by sampling frequency. However, frame size is 128 samples at 44.1
 * and 48 KHz and 256 at 88.2 and 96 KHz.
 *
 * Returns: Number of samples / channel
 */
static guint
gst_ldac_enc_get_num_samples (guint rate)
{
  switch (rate) {
    case 44100:
    case 48000:
      return 128;
    case 88200:
    case 96000:
      return 256;
    default:
      break;
  }

  g_assert_not_reached ();

  /* If assertion gets compiled out */
  return 128;
}
