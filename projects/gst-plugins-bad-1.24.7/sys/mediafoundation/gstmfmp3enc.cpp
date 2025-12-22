/* GStreamer
 * Copyright (C) 2020 Seungha Yang <seungha@centricular.com>
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
 * SECTION:element-mfmp3enc
 * @title: mfmp3enc
 *
 * This element encodes raw audio into MP3 compressed data.
 *
 * ## Example pipelines
 * |[
 * gst-launch-1.0 -v audiotestsrc ! mfmp3enc ! filesink location=test.mp3
 * ]| This example pipeline will encode a test audio source to MP3 using
 * Media Foundation encoder
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>
#include "gstmfaudioencoder.h"
#include "gstmfmp3enc.h"
#include <wrl.h>
#include <set>
#include <vector>
#include <string>

/* *INDENT-OFF* */
using namespace Microsoft::WRL;
/* *INDENT-ON* */

GST_DEBUG_CATEGORY (gst_mf_mp3_enc_debug);
#define GST_CAT_DEFAULT gst_mf_mp3_enc_debug

enum
{
  PROP_0,
  PROP_BITRATE,
};

#define DEFAULT_BITRATE (0)

typedef struct _GstMFMp3Enc
{
  GstMFAudioEncoder parent;

  /* properties */
  guint bitrate;
} GstMFMp3Enc;

typedef struct _GstMFMp3EncClass
{
  GstMFAudioEncoderClass parent_class;

} GstMFMp3EncClass;

/* *INDENT-OFF* */
typedef struct
{
  GstCaps *sink_caps;
  GstCaps *src_caps;
  gchar *device_name;
  guint32 enum_flags;
  guint device_index;
  std::set<UINT32> bitrate_list;
} GstMFMp3EncClassData;
/* *INDENT-ON* */

static GstElementClass *parent_class = nullptr;

static void gst_mf_mp3_enc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_mf_mp3_enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static gboolean gst_mf_mp3_enc_get_output_type (GstMFAudioEncoder * encoder,
    GstAudioInfo * info, IMFMediaType ** output_type);
static gboolean gst_mf_mp3_enc_get_input_type (GstMFAudioEncoder * encoder,
    GstAudioInfo * info, IMFMediaType ** input_type);
static gboolean gst_mf_mp3_enc_set_src_caps (GstMFAudioEncoder * encoder,
    GstAudioInfo * info);

static void
gst_mf_mp3_enc_class_init (GstMFMp3EncClass * klass, gpointer data)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstMFAudioEncoderClass *encoder_class = GST_MF_AUDIO_ENCODER_CLASS (klass);
  GstMFMp3EncClassData *cdata = (GstMFMp3EncClassData *) data;
  gchar *long_name;
  gchar *classification;
  guint max_bitrate = 0;
  std::string bitrate_blurb;

  parent_class = (GstElementClass *) g_type_class_peek_parent (klass);

  gobject_class->get_property = gst_mf_mp3_enc_get_property;
  gobject_class->set_property = gst_mf_mp3_enc_set_property;

  bitrate_blurb = "Bitrate in bit/sec, (0 = auto), valid values are { 0";

  /* *INDENT-OFF* */
  for (auto iter: cdata->bitrate_list) {
    bitrate_blurb += ", " + std::to_string (iter);
    /* std::set<> stores values in a sorted fashion */
    max_bitrate = iter;
  }
  bitrate_blurb += " }";
  /* *INDENT-ON* */

  g_object_class_install_property (gobject_class, PROP_BITRATE,
      g_param_spec_uint ("bitrate", "Bitrate", bitrate_blurb.c_str (), 0,
          max_bitrate, DEFAULT_BITRATE,
          (GParamFlags) (GST_PARAM_MUTABLE_READY | G_PARAM_READWRITE |
              G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK)));

  long_name = g_strdup_printf ("Media Foundation %s", cdata->device_name);
  classification = g_strdup_printf ("Codec/Encoder/Audio%s",
      (cdata->enum_flags & MFT_ENUM_FLAG_HARDWARE) == MFT_ENUM_FLAG_HARDWARE ?
      "/Hardware" : "");
  gst_element_class_set_metadata (element_class, long_name,
      classification,
      "Microsoft Media Foundation MP3 Encoder",
      "Seungha Yang <seungha@centricular.com>");
  g_free (long_name);
  g_free (classification);

  gst_element_class_add_pad_template (element_class,
      gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
          cdata->sink_caps));
  gst_element_class_add_pad_template (element_class,
      gst_pad_template_new ("src", GST_PAD_SRC, GST_PAD_ALWAYS,
          cdata->src_caps));

  encoder_class->get_output_type =
      GST_DEBUG_FUNCPTR (gst_mf_mp3_enc_get_output_type);
  encoder_class->get_input_type =
      GST_DEBUG_FUNCPTR (gst_mf_mp3_enc_get_input_type);
  encoder_class->set_src_caps = GST_DEBUG_FUNCPTR (gst_mf_mp3_enc_set_src_caps);

  encoder_class->codec_id = MFAudioFormat_MP3;
  encoder_class->enum_flags = cdata->enum_flags;
  encoder_class->device_index = cdata->device_index;
  encoder_class->frame_samples = 1152;

  g_free (cdata->device_name);
  gst_caps_unref (cdata->sink_caps);
  gst_caps_unref (cdata->src_caps);
  delete cdata;
}

static void
gst_mf_mp3_enc_init (GstMFMp3Enc * self)
{
  self->bitrate = DEFAULT_BITRATE;
}

static void
gst_mf_mp3_enc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstMFMp3Enc *self = (GstMFMp3Enc *) (object);

  switch (prop_id) {
    case PROP_BITRATE:
      g_value_set_uint (value, self->bitrate);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_mf_mp3_enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstMFMp3Enc *self = (GstMFMp3Enc *) (object);

  switch (prop_id) {
    case PROP_BITRATE:
      self->bitrate = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_mf_mp3_enc_get_output_type (GstMFAudioEncoder * encoder,
    GstAudioInfo * info, IMFMediaType ** output_type)
{
  GstMFMp3Enc *self = (GstMFMp3Enc *) encoder;
  GstMFTransform *transform = encoder->transform;
  GList *output_list = nullptr;
  GList *iter;
  ComPtr < IMFMediaType > target_output;
  std::vector < ComPtr < IMFMediaType >> filtered_types;
  std::set < UINT32 > bitrate_list;
  UINT32 bitrate;
  UINT32 target_bitrate = 0;
  HRESULT hr;

  if (!gst_mf_transform_get_output_available_types (transform, &output_list)) {
    GST_ERROR_OBJECT (self, "Couldn't get available output type");
    return FALSE;
  }

  /* 1. Filtering based on channels and sample rate */
  for (iter = output_list; iter; iter = g_list_next (iter)) {
    IMFMediaType *type = (IMFMediaType *) iter->data;
    GUID guid = GUID_NULL;
    UINT32 value;

    hr = type->GetGUID (MF_MT_MAJOR_TYPE, &guid);
    if (!gst_mf_result (hr))
      continue;

    if (!IsEqualGUID (guid, MFMediaType_Audio)) {
      GST_WARNING_OBJECT (self, "Major type is not audio");
      continue;
    }

    hr = type->GetGUID (MF_MT_SUBTYPE, &guid);
    if (!gst_mf_result (hr))
      continue;

    if (!IsEqualGUID (guid, MFAudioFormat_MP3)) {
      GST_WARNING_OBJECT (self, "Sub type is not MP3");
      continue;
    }

    hr = type->GetUINT32 (MF_MT_AUDIO_NUM_CHANNELS, &value);
    if (!gst_mf_result (hr))
      continue;

    if (value != GST_AUDIO_INFO_CHANNELS (info))
      continue;

    hr = type->GetUINT32 (MF_MT_AUDIO_SAMPLES_PER_SECOND, &value);
    if (!gst_mf_result (hr))
      continue;

    if (value != GST_AUDIO_INFO_RATE (info))
      continue;

    hr = type->GetUINT32 (MF_MT_AUDIO_AVG_BYTES_PER_SECOND, &value);
    if (!gst_mf_result (hr))
      continue;

    filtered_types.push_back (type);
    /* convert bytes to bit */
    bitrate_list.insert (value * 8);
  }

  g_list_free_full (output_list, (GDestroyNotify) gst_mf_media_type_release);

  if (filtered_types.empty ()) {
    GST_ERROR_OBJECT (self, "Couldn't find target output type");
    return FALSE;
  }

  GST_DEBUG_OBJECT (self, "have %d candidate output", filtered_types.size ());

  /* 2. Find the best matching bitrate */
  bitrate = self->bitrate;

  if (bitrate == 0) {
    /* default of MFT
     * https://docs.microsoft.com/en-us/windows/win32/medfound/mp3-audio-encoder
     */
    if (GST_AUDIO_INFO_CHANNELS (info) == 1) {
      bitrate = 128000;
    } else {
      bitrate = 320000;
    }

    GST_DEBUG_OBJECT (self, "Calculated bitrate %d", bitrate);
  } else {
    GST_DEBUG_OBJECT (self, "Requested bitrate %d", bitrate);
  }

  GST_DEBUG_OBJECT (self, "Available bitrates");

  /* *INDENT-OFF* */
  for (auto it: bitrate_list)
    GST_DEBUG_OBJECT (self, "\t%d", it);

  /* Based on calculated or requested bitrate, find the closest supported
   * bitrate */
  {
    const auto it = bitrate_list.lower_bound (bitrate);
    if (it == bitrate_list.end()) {
      target_bitrate = *std::prev (it);
    } else {
      target_bitrate = *it;
    }
  }

  GST_DEBUG_OBJECT (self, "Selected target bitrate %d", target_bitrate);

  for (auto it: filtered_types) {
    UINT32 value = 0;

    it->GetUINT32 (MF_MT_AUDIO_AVG_BYTES_PER_SECOND, &value);
    if (value * 8 == target_bitrate) {
      target_output = it;
      break;
    }
  }
  /* *INDENT-ON* */

  if (!target_output) {
    GST_ERROR_OBJECT (self, "Failed to decide final output type");
    return FALSE;
  }

  *output_type = target_output.Detach ();

  return TRUE;
}

static gboolean
gst_mf_mp3_enc_get_input_type (GstMFAudioEncoder * encoder, GstAudioInfo * info,
    IMFMediaType ** input_type)
{
  GstMFMp3Enc *self = (GstMFMp3Enc *) encoder;
  GstMFTransform *transform = encoder->transform;
  GList *input_list = nullptr;
  GList *iter;
  ComPtr < IMFMediaType > target_input;
  std::vector < ComPtr < IMFMediaType >> filtered_types;
  std::set < UINT32 > bitrate_list;
  HRESULT hr;

  if (!gst_mf_transform_get_input_available_types (transform, &input_list)) {
    GST_ERROR_OBJECT (self, "Couldn't get available output type");
    return FALSE;
  }

  /* 1. Filtering based on channels and sample rate */
  for (iter = input_list; iter; iter = g_list_next (iter)) {
    IMFMediaType *type = (IMFMediaType *) iter->data;
    GUID guid = GUID_NULL;
    UINT32 value;

    hr = type->GetGUID (MF_MT_MAJOR_TYPE, &guid);
    if (!gst_mf_result (hr))
      continue;

    if (!IsEqualGUID (guid, MFMediaType_Audio)) {
      GST_WARNING_OBJECT (self, "Major type is not audio");
      continue;
    }

    hr = type->GetGUID (MF_MT_SUBTYPE, &guid);
    if (!gst_mf_result (hr))
      continue;

    if (!IsEqualGUID (guid, MFAudioFormat_PCM)) {
      GST_WARNING_OBJECT (self, "Sub type is not PCM");
      continue;
    }

    hr = type->GetUINT32 (MF_MT_AUDIO_NUM_CHANNELS, &value);
    if (!gst_mf_result (hr))
      continue;

    if (value != GST_AUDIO_INFO_CHANNELS (info))
      continue;

    hr = type->GetUINT32 (MF_MT_AUDIO_SAMPLES_PER_SECOND, &value);
    if (!gst_mf_result (hr))
      continue;

    if (value != GST_AUDIO_INFO_RATE (info))
      continue;

    filtered_types.push_back (type);
  }

  g_list_free_full (input_list, (GDestroyNotify) gst_mf_media_type_release);

  if (filtered_types.empty ()) {
    GST_ERROR_OBJECT (self, "Couldn't find target input type");
    return FALSE;
  }

  GST_DEBUG_OBJECT (self, "Total %d input types are available",
      filtered_types.size ());

  /* Just select the first one */
  target_input = *filtered_types.begin ();

  *input_type = target_input.Detach ();

  return TRUE;
}

static gboolean
gst_mf_mp3_enc_set_src_caps (GstMFAudioEncoder * encoder, GstAudioInfo * info)
{
  GstMFMp3Enc *self = (GstMFMp3Enc *) encoder;
  GstCaps *src_caps;
  gboolean ret;
  ComPtr < IMFMediaType > output_type;
  gint version = 1;

  if (!gst_mf_transform_get_output_current_type (encoder->transform,
          &output_type)) {
    GST_ERROR_OBJECT (self, "Couldn't get current output type");
    return FALSE;
  }

  if (GST_AUDIO_INFO_RATE (info) == 32000 ||
      GST_AUDIO_INFO_RATE (info) == 44100 ||
      GST_AUDIO_INFO_RATE (info) == 48000)
    version = 1;
  else
    version = 2;

  src_caps = gst_caps_new_simple ("audio/mpeg",
      "mpegversion", G_TYPE_INT, 1,
      "mpegaudioversion", G_TYPE_INT, version,
      "layer", G_TYPE_INT, 3,
      "channels", G_TYPE_INT, GST_AUDIO_INFO_CHANNELS (info),
      "rate", G_TYPE_INT, GST_AUDIO_INFO_RATE (info), nullptr);

  ret =
      gst_audio_encoder_set_output_format (GST_AUDIO_ENCODER (self), src_caps);
  if (!ret) {
    GST_WARNING_OBJECT (self,
        "Couldn't set output format %" GST_PTR_FORMAT, src_caps);
  }
  gst_caps_unref (src_caps);

  return ret;
}

static void
gst_mf_mp3_enc_register (GstPlugin * plugin, guint rank,
    const gchar * device_name, guint32 enum_flags, guint device_index,
    GstCaps * sink_caps, GstCaps * src_caps,
    const std::set < UINT32 > &bitrate_list)
{
  GType type;
  gchar *type_name;
  gchar *feature_name;
  gint i;
  GstMFMp3EncClassData *cdata;
  gboolean is_default = TRUE;
  GTypeInfo type_info = {
    sizeof (GstMFMp3EncClass),
    nullptr,
    nullptr,
    (GClassInitFunc) gst_mf_mp3_enc_class_init,
    nullptr,
    nullptr,
    sizeof (GstMFMp3Enc),
    0,
    (GInstanceInitFunc) gst_mf_mp3_enc_init,
  };

  cdata = new GstMFMp3EncClassData;
  cdata->sink_caps = sink_caps;
  cdata->src_caps = src_caps;
  cdata->device_name = g_strdup (device_name);
  cdata->enum_flags = enum_flags;
  cdata->device_index = device_index;
  cdata->bitrate_list = bitrate_list;
  type_info.class_data = cdata;

  type_name = g_strdup ("GstMFMp3Enc");
  feature_name = g_strdup ("mfmp3enc");

  i = 1;
  while (g_type_from_name (type_name) != 0) {
    g_free (type_name);
    g_free (feature_name);
    type_name = g_strdup_printf ("GstMFMp3Device%dEnc", i);
    feature_name = g_strdup_printf ("mfmp3device%denc", i);
    is_default = FALSE;
    i++;
  }

  type =
      g_type_register_static (GST_TYPE_MF_AUDIO_ENCODER, type_name, &type_info,
      (GTypeFlags) 0);

  /* make lower rank than default device */
  if (rank > 0 && !is_default)
    rank--;

  if (!gst_element_register (plugin, feature_name, rank, type))
    GST_WARNING ("Failed to register plugin '%s'", type_name);

  g_free (type_name);
  g_free (feature_name);
}

static gboolean
gst_mf_mp3_enc_create_template_caps (const std::set < UINT32 > &rate_list,
    gint channels, GstCaps ** sink_caps, GstCaps ** src_caps)
{
  GstCaps *sink = nullptr;
  GstCaps *src = nullptr;
  GValue rate_value = G_VALUE_INIT;

  if (rate_list.empty ()) {
    GST_WARNING ("No available rate for channels %d", channels);
    return FALSE;
  }

  if (channels != 0) {
    sink =
        gst_caps_from_string ("audio/x-raw, "
        "format = (string) " GST_AUDIO_NE (S16)
        ", layout = (string) interleaved");
    src =
        gst_caps_from_string ("audio/mpeg, mpegversion = (int) 1,"
        "layer = (int) 3");

    gst_caps_set_simple (sink, "channels", G_TYPE_INT, channels, nullptr);
    gst_caps_set_simple (src, "channels", G_TYPE_INT, channels, nullptr);
  } else {
    sink =
        gst_caps_from_string ("audio/x-raw, "
        "format = (string) " GST_AUDIO_NE (S16)
        ", layout = (string) interleaved, channels = (int) [ 1, 2 ]");
    src =
        gst_caps_from_string ("audio/mpeg, mpegversion = (int) 1,"
        "layer = (int) 3,  channels = (int) [ 1, 2 ]");
  }

  g_value_init (&rate_value, GST_TYPE_LIST);

  /* *INDENT-OFF* */
  for (const auto &it: rate_list) {
    GValue rate = G_VALUE_INIT;

    g_value_init (&rate, G_TYPE_INT);
    g_value_set_int (&rate, (gint) it);
    gst_value_list_append_and_take_value (&rate_value, &rate);
  }
  /* *INDENT-ON* */

  gst_caps_set_value (src, "rate", &rate_value);
  gst_caps_set_value (sink, "rate", &rate_value);

  g_value_unset (&rate_value);

  if (*sink_caps == nullptr)
    *sink_caps = sink;
  else
    *sink_caps = gst_caps_merge (*sink_caps, sink);

  if (*src_caps == nullptr)
    *src_caps = src;
  else
    *src_caps = gst_caps_merge (*src_caps, src);

  return TRUE;
}

static void
gst_mf_mp3_enc_plugin_init_internal (GstPlugin * plugin, guint rank,
    GstMFTransform * transform, guint device_index, guint32 enum_flags)
{
  HRESULT hr;
  gint i;
  GstCaps *src_caps = nullptr;
  GstCaps *sink_caps = nullptr;
  gchar *device_name = nullptr;
  GList *output_list = nullptr;
  GList *iter;
  std::set < UINT32 > mono_rate_list;
  std::set < UINT32 > stereo_rate_list;
  std::set < UINT32 > bitrate_list;
  gboolean config_found = FALSE;

  if (!gst_mf_transform_open (transform))
    return;

  g_object_get (transform, "device-name", &device_name, nullptr);
  if (!device_name) {
    GST_WARNING_OBJECT (transform, "Unknown device name");
    return;
  }

  if (!gst_mf_transform_get_output_available_types (transform, &output_list)) {
    GST_WARNING_OBJECT (transform, "Couldn't get output types");
    goto done;
  }

  GST_INFO_OBJECT (transform, "Have %d output type",
      g_list_length (output_list));

  for (iter = output_list, i = 0; iter; iter = g_list_next (iter), i++) {
    UINT32 channels, rate, bitrate;
    GUID guid = GUID_NULL;
    IMFMediaType *type = (IMFMediaType *) iter->data;
#ifndef GST_DISABLE_GST_DEBUG
    gchar *msg = g_strdup_printf ("Output IMFMediaType %d", i);
    gst_mf_dump_attributes ((IMFAttributes *) type, msg, GST_LEVEL_TRACE);
    g_free (msg);
#endif

    hr = type->GetGUID (MF_MT_MAJOR_TYPE, &guid);
    if (!gst_mf_result (hr))
      continue;

    /* shouldn't happen */
    if (!IsEqualGUID (guid, MFMediaType_Audio))
      continue;

    hr = type->GetGUID (MF_MT_SUBTYPE, &guid);
    if (!gst_mf_result (hr))
      continue;

    /* shouldn't happen */
    if (!IsEqualGUID (guid, MFAudioFormat_MP3))
      continue;

    hr = type->GetUINT32 (MF_MT_AUDIO_NUM_CHANNELS, &channels);
    if (!gst_mf_result (hr))
      continue;

    if (channels != 1 && channels != 2) {
      GST_WARNING_OBJECT (transform, "Unknown channels %d", channels);
      continue;
    }

    hr = type->GetUINT32 (MF_MT_AUDIO_SAMPLES_PER_SECOND, &rate);
    if (!gst_mf_result (hr))
      continue;

    hr = type->GetUINT32 (MF_MT_AUDIO_AVG_BYTES_PER_SECOND, &bitrate);
    if (!gst_mf_result (hr))
      continue;

    if (channels == 1)
      mono_rate_list.insert (rate);
    else if (channels == 2)
      stereo_rate_list.insert (rate);
    else
      g_assert_not_reached ();

    /* convert bytes to bit */
    bitrate_list.insert (bitrate * 8);

    config_found = TRUE;
  }

  if (!config_found) {
    GST_WARNING_OBJECT (transform, "Couldn't find available configuration");
    goto done;
  }

  /* MFT might support more rate and channels combination than documented
   * https://docs.microsoft.com/en-us/windows/win32/medfound/mp3-audio-encoder
   *
   * Configure caps per channels if supported rate values are different
   */
  if (!mono_rate_list.empty () && !stereo_rate_list.empty () &&
      mono_rate_list == stereo_rate_list) {
    gst_mf_mp3_enc_create_template_caps (mono_rate_list,
        0, &sink_caps, &src_caps);
  } else {
    if (!mono_rate_list.empty ()) {
      gst_mf_mp3_enc_create_template_caps (mono_rate_list,
          1, &sink_caps, &src_caps);
    }

    if (!stereo_rate_list.empty ()) {
      gst_mf_mp3_enc_create_template_caps (stereo_rate_list,
          2, &sink_caps, &src_caps);
    }
  }

  if (!sink_caps || !src_caps) {
    GST_WARNING_OBJECT (transform, "Failed to configure template caps");
    gst_clear_caps (&sink_caps);
    gst_clear_caps (&src_caps);
    goto done;
  }

  GST_MINI_OBJECT_FLAG_SET (sink_caps, GST_MINI_OBJECT_FLAG_MAY_BE_LEAKED);
  GST_MINI_OBJECT_FLAG_SET (src_caps, GST_MINI_OBJECT_FLAG_MAY_BE_LEAKED);

  gst_mf_mp3_enc_register (plugin, rank, device_name, enum_flags, device_index,
      sink_caps, src_caps, bitrate_list);

done:
  if (output_list)
    g_list_free_full (output_list, (GDestroyNotify) gst_mf_media_type_release);
  g_free (device_name);
}

void
gst_mf_mp3_enc_plugin_init (GstPlugin * plugin, guint rank)
{
  GstMFTransformEnumParams enum_params = { 0, };
  MFT_REGISTER_TYPE_INFO output_type;
  GstMFTransform *transform;
  gint i;
  gboolean do_next;

  GST_DEBUG_CATEGORY_INIT (gst_mf_mp3_enc_debug, "mfmp3enc", 0, "mfmp3enc");

  output_type.guidMajorType = MFMediaType_Audio;
  output_type.guidSubtype = MFAudioFormat_MP3;

  enum_params.category = MFT_CATEGORY_AUDIO_ENCODER;
  enum_params.enum_flags = (MFT_ENUM_FLAG_SYNCMFT |
      MFT_ENUM_FLAG_SORTANDFILTER | MFT_ENUM_FLAG_SORTANDFILTER_APPROVED_ONLY);
  enum_params.output_typeinfo = &output_type;

  i = 0;
  do {
    enum_params.device_index = i++;
    transform = gst_mf_transform_new (&enum_params);
    do_next = TRUE;

    if (!transform) {
      do_next = FALSE;
    } else {
      gst_mf_mp3_enc_plugin_init_internal (plugin, rank, transform,
          enum_params.device_index, enum_params.enum_flags);
      gst_clear_object (&transform);
    }
  } while (do_next);
}
