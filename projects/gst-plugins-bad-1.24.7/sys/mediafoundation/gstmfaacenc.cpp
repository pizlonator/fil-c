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
 * SECTION:element-mfaacenc
 * @title: mfaacenc
 *
 * This element encodes raw audio into AAC compressed data.
 *
 * ## Example pipelines
 * |[
 * gst-launch-1.0 -v audiotestsrc ! mfaacenc ! aacparse ! qtmux ! filesink location=audiotestsrc.mp4
 * ]| This example pipeline will encode a test audio source to AAC using
 * Media Foundation encoder, and muxes it in a mp4 container.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>
#include "gstmfaudioencoder.h"
#include "gstmfaacenc.h"
#include <wrl.h>
#include <set>
#include <vector>
#include <string>

/* *INDENT-OFF* */
using namespace Microsoft::WRL;
/* *INDENT-ON* */

GST_DEBUG_CATEGORY (gst_mf_aac_enc_debug);
#define GST_CAT_DEFAULT gst_mf_aac_enc_debug

enum
{
  PROP_0,
  PROP_BITRATE,
};

#define DEFAULT_BITRATE (0)

typedef struct _GstMFAacEnc
{
  GstMFAudioEncoder parent;

  /* properties */
  guint bitrate;
} GstMFAacEnc;

typedef struct _GstMFAacEncClass
{
  GstMFAudioEncoderClass parent_class;

} GstMFAacEncClass;

/* *INDENT-OFF* */
typedef struct
{
  GstCaps *sink_caps;
  GstCaps *src_caps;
  gchar *device_name;
  guint32 enum_flags;
  guint device_index;
  std::set<UINT32> bitrate_list;
} GstMFAacEncClassData;
/* *INDENT-ON* */

static GstElementClass *parent_class = nullptr;

static void gst_mf_aac_enc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_mf_aac_enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static gboolean gst_mf_aac_enc_get_output_type (GstMFAudioEncoder * encoder,
    GstAudioInfo * info, IMFMediaType ** output_type);
static gboolean gst_mf_aac_enc_get_input_type (GstMFAudioEncoder * encoder,
    GstAudioInfo * info, IMFMediaType ** input_type);
static gboolean gst_mf_aac_enc_set_src_caps (GstMFAudioEncoder * encoder,
    GstAudioInfo * info);

static void
gst_mf_aac_enc_class_init (GstMFAacEncClass * klass, gpointer data)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstMFAudioEncoderClass *encoder_class = GST_MF_AUDIO_ENCODER_CLASS (klass);
  GstMFAacEncClassData *cdata = (GstMFAacEncClassData *) data;
  gchar *long_name;
  gchar *classification;
  guint max_bitrate = 0;
  std::string bitrate_blurb;

  parent_class = (GstElementClass *) g_type_class_peek_parent (klass);

  gobject_class->get_property = gst_mf_aac_enc_get_property;
  gobject_class->set_property = gst_mf_aac_enc_set_property;

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
      "Microsoft Media Foundation AAC Encoder",
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
      GST_DEBUG_FUNCPTR (gst_mf_aac_enc_get_output_type);
  encoder_class->get_input_type =
      GST_DEBUG_FUNCPTR (gst_mf_aac_enc_get_input_type);
  encoder_class->set_src_caps = GST_DEBUG_FUNCPTR (gst_mf_aac_enc_set_src_caps);

  encoder_class->codec_id = MFAudioFormat_AAC;
  encoder_class->enum_flags = cdata->enum_flags;
  encoder_class->device_index = cdata->device_index;
  encoder_class->frame_samples = 1024;

  g_free (cdata->device_name);
  gst_caps_unref (cdata->sink_caps);
  gst_caps_unref (cdata->src_caps);
  delete cdata;
}

static void
gst_mf_aac_enc_init (GstMFAacEnc * self)
{
  self->bitrate = DEFAULT_BITRATE;
}

static void
gst_mf_aac_enc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstMFAacEnc *self = (GstMFAacEnc *) (object);

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
gst_mf_aac_enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstMFAacEnc *self = (GstMFAacEnc *) (object);

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
gst_mf_aac_enc_get_output_type (GstMFAudioEncoder * encoder,
    GstAudioInfo * info, IMFMediaType ** output_type)
{
  GstMFAacEnc *self = (GstMFAacEnc *) encoder;
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

    if (!IsEqualGUID (guid, MFAudioFormat_AAC)) {
      GST_WARNING_OBJECT (self, "Sub type is not AAC");
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

  /* Media Foundation AAC encoder supports sample-rate 44100 or 48000 */
  if (bitrate == 0) {
    /* http://wiki.hydrogenaud.io/index.php?title=Fraunhofer_FDK_AAC#Recommended_Sampling_Rate_and_Bitrate_Combinations
     * was referenced but the supported range by MediaFoudation is much limited
     * than it */
    if (GST_AUDIO_INFO_CHANNELS (info) == 1) {
      if (GST_AUDIO_INFO_RATE (info) <= 44100) {
        bitrate = 96000;
      } else {
        bitrate = 160000;
      }
    } else if (GST_AUDIO_INFO_CHANNELS (info) == 2) {
      if (GST_AUDIO_INFO_RATE (info) <= 44100) {
        bitrate = 112000;
      } else {
        bitrate = 320000;
      }
    } else {
      /* 5.1 */
      if (GST_AUDIO_INFO_RATE (info) <= 44100) {
        bitrate = 240000;
      } else {
        bitrate = 320000;
      }
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
gst_mf_aac_enc_get_input_type (GstMFAudioEncoder * encoder, GstAudioInfo * info,
    IMFMediaType ** input_type)
{
  GstMFAacEnc *self = (GstMFAacEnc *) encoder;
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
gst_mf_aac_enc_set_src_caps (GstMFAudioEncoder * encoder, GstAudioInfo * info)
{
  GstMFAacEnc *self = (GstMFAacEnc *) encoder;
  HRESULT hr;
  GstCaps *src_caps;
  GstBuffer *codec_data;
  UINT8 *blob = nullptr;
  UINT32 blob_size = 0;
  gboolean ret;
  ComPtr < IMFMediaType > output_type;
  static const guint config_data_offset = 12;

  if (!gst_mf_transform_get_output_current_type (encoder->transform,
          &output_type)) {
    GST_ERROR_OBJECT (self, "Couldn't get current output type");
    return FALSE;
  }

  /* user data contains the portion of the HEAACWAVEINFO structure that appears
   * after the WAVEFORMATEX structure (that is, after the wfx member).
   * This is followed by the AudioSpecificConfig() data,
   * as defined by ISO/IEC 14496-3.
   * https://docs.microsoft.com/en-us/windows/win32/medfound/aac-encoder
   *
   * The offset AudioSpecificConfig() data is 12 in this case
   */
  hr = output_type->GetBlobSize (MF_MT_USER_DATA, &blob_size);
  if (!gst_mf_result (hr) || blob_size <= config_data_offset) {
    GST_ERROR_OBJECT (self,
        "Couldn't get size of MF_MT_USER_DATA, size %d, %d", blob_size);
    return FALSE;
  }

  hr = output_type->GetAllocatedBlob (MF_MT_USER_DATA, &blob, &blob_size);
  if (!gst_mf_result (hr)) {
    GST_ERROR_OBJECT (self, "Couldn't get user data blob");
    return FALSE;
  }

  codec_data = gst_buffer_new_and_alloc (blob_size - config_data_offset);
  gst_buffer_fill (codec_data, 0, blob + config_data_offset,
      blob_size - config_data_offset);

  src_caps = gst_caps_new_simple ("audio/mpeg",
      "mpegversion", G_TYPE_INT, 4,
      "stream-format", G_TYPE_STRING, "raw",
      "channels", G_TYPE_INT, GST_AUDIO_INFO_CHANNELS (info),
      "rate", G_TYPE_INT, GST_AUDIO_INFO_RATE (info),
      "framed", G_TYPE_BOOLEAN, TRUE,
      "codec_data", GST_TYPE_BUFFER, codec_data, nullptr);
  gst_buffer_unref (codec_data);

  gst_codec_utils_aac_caps_set_level_and_profile (src_caps,
      blob + config_data_offset, blob_size - config_data_offset);
  CoTaskMemFree (blob);

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
gst_mf_aac_enc_register (GstPlugin * plugin, guint rank,
    const gchar * device_name, guint32 enum_flags, guint device_index,
    GstCaps * sink_caps, GstCaps * src_caps,
    const std::set < UINT32 > &bitrate_list)
{
  GType type;
  gchar *type_name;
  gchar *feature_name;
  gint i;
  GstMFAacEncClassData *cdata;
  gboolean is_default = TRUE;
  GTypeInfo type_info = {
    sizeof (GstMFAacEncClass),
    nullptr,
    nullptr,
    (GClassInitFunc) gst_mf_aac_enc_class_init,
    nullptr,
    nullptr,
    sizeof (GstMFAacEnc),
    0,
    (GInstanceInitFunc) gst_mf_aac_enc_init,
  };

  cdata = new GstMFAacEncClassData;
  cdata->sink_caps = sink_caps;
  cdata->src_caps = src_caps;
  cdata->device_name = g_strdup (device_name);
  cdata->enum_flags = enum_flags;
  cdata->device_index = device_index;
  cdata->bitrate_list = bitrate_list;
  type_info.class_data = cdata;

  type_name = g_strdup ("GstMFAacEnc");
  feature_name = g_strdup ("mfaacenc");

  i = 1;
  while (g_type_from_name (type_name) != 0) {
    g_free (type_name);
    g_free (feature_name);
    type_name = g_strdup_printf ("GstMFAacDevice%dEnc", i);
    feature_name = g_strdup_printf ("mfaacdevice%denc", i);
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

static void
gst_mf_aac_enc_plugin_init_internal (GstPlugin * plugin, guint rank,
    GstMFTransform * transform, guint device_index, guint32 enum_flags)
{
  HRESULT hr;
  gint i;
  GstCaps *src_caps = nullptr;
  GstCaps *sink_caps = nullptr;
  gchar *device_name = nullptr;
  GList *output_list = nullptr;
  GList *iter;
  std::set < UINT32 > channels_list;
  std::set < UINT32 > rate_list;
  std::set < UINT32 > bitrate_list;
  gboolean config_found = FALSE;
  GValue channles_value = G_VALUE_INIT;
  GValue rate_value = G_VALUE_INIT;

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
    if (!IsEqualGUID (guid, MFAudioFormat_AAC))
      continue;

    /* Windows 10 channels 6 (5.1) channels so we cannot hard code it */
    hr = type->GetUINT32 (MF_MT_AUDIO_NUM_CHANNELS, &channels);
    if (!gst_mf_result (hr))
      continue;

    hr = type->GetUINT32 (MF_MT_AUDIO_SAMPLES_PER_SECOND, &rate);
    if (!gst_mf_result (hr))
      continue;

    /* NOTE: MFT AAC encoder seems to support more bitrate than it's documented
     * at https://docs.microsoft.com/en-us/windows/win32/medfound/aac-encoder
     * We will pass supported bitrate values to class init
     */
    hr = type->GetUINT32 (MF_MT_AUDIO_AVG_BYTES_PER_SECOND, &bitrate);
    if (!gst_mf_result (hr))
      continue;

    channels_list.insert (channels);
    rate_list.insert (rate);
    /* convert bytes to bit */
    bitrate_list.insert (bitrate * 8);

    config_found = TRUE;
  }

  if (!config_found) {
    GST_WARNING_OBJECT (transform, "Couldn't find available configuration");
    goto done;
  }

  src_caps =
      gst_caps_from_string ("audio/mpeg, mpegversion = (int) 4, "
      "stream-format = (string) raw, framed = (boolean) true, "
      "base-profile = (string) lc");
  sink_caps =
      gst_caps_from_string ("audio/x-raw, layout = (string) interleaved, "
      "format = (string) " GST_AUDIO_NE (S16));

  g_value_init (&channles_value, GST_TYPE_LIST);
  g_value_init (&rate_value, GST_TYPE_LIST);

  /* *INDENT-OFF* */
  for (auto it: channels_list) {
    GValue channles = G_VALUE_INIT;

    g_value_init (&channles, G_TYPE_INT);
    g_value_set_int (&channles, (gint) it);
    gst_value_list_append_and_take_value (&channles_value, &channles);
  }

  for (auto it: rate_list) {
    GValue rate = G_VALUE_INIT;

    g_value_init (&rate, G_TYPE_INT);
    g_value_set_int (&rate, (gint) it);
    gst_value_list_append_and_take_value (&rate_value, &rate);
  }
  /* *INDENT-ON* */

  gst_caps_set_value (src_caps, "channels", &channles_value);
  gst_caps_set_value (sink_caps, "channels", &channles_value);

  gst_caps_set_value (src_caps, "rate", &rate_value);
  gst_caps_set_value (sink_caps, "rate", &rate_value);

  GST_MINI_OBJECT_FLAG_SET (sink_caps, GST_MINI_OBJECT_FLAG_MAY_BE_LEAKED);
  GST_MINI_OBJECT_FLAG_SET (src_caps, GST_MINI_OBJECT_FLAG_MAY_BE_LEAKED);

  gst_mf_aac_enc_register (plugin, rank, device_name, enum_flags, device_index,
      sink_caps, src_caps, bitrate_list);

done:
  if (output_list)
    g_list_free_full (output_list, (GDestroyNotify) gst_mf_media_type_release);
  g_free (device_name);
  g_value_unset (&channles_value);
  g_value_unset (&rate_value);
}

void
gst_mf_aac_enc_plugin_init (GstPlugin * plugin, guint rank)
{
  GstMFTransformEnumParams enum_params = { 0, };
  MFT_REGISTER_TYPE_INFO output_type;
  GstMFTransform *transform;
  gint i;
  gboolean do_next;

  GST_DEBUG_CATEGORY_INIT (gst_mf_aac_enc_debug, "mfaacenc", 0, "mfaacenc");

  output_type.guidMajorType = MFMediaType_Audio;
  output_type.guidSubtype = MFAudioFormat_AAC;

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
      gst_mf_aac_enc_plugin_init_internal (plugin, rank, transform,
          enum_params.device_index, enum_params.enum_flags);
      gst_clear_object (&transform);
    }
  } while (do_next);
}
