/* GStreamer
 * Copyright (C) 2022 Seungha Yang <seungha@centricular.com>
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
 * SECTION:element-mfaacdec
 * @title: mfaacdec
 *
 * This element decodes AAC compressed data into RAW audio data.
 *
 * Since: 1.22
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>
#include "gstmfaudiodecoder.h"
#include "gstmfaacdec.h"
#include <wrl.h>
#include <string.h>

/* *INDENT-OFF* */
using namespace Microsoft::WRL;
/* *INDENT-ON* */

GST_DEBUG_CATEGORY (gst_mf_aac_dec_debug);
#define GST_CAT_DEFAULT gst_mf_aac_dec_debug

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/mpeg, "
        "mpegversion = (int) {2, 4}, "
        "stream-format = (string) raw, framed = (boolean) true, "
        "channels = (int) [1, 6], rate = (int) [8000, 48000]")
    );

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw, "
        "format = (string) " GST_AUDIO_NE (S16) ", "
        "layout = (string) interleaved, "
        "channels = (int) [1, 6], rate = (int) [8000, 48000]")
    );

typedef struct _GstMFAacDec
{
  GstMFAudioDecoder parent;
} GstMFAacDec;

typedef struct _GstMFAacDecClass
{
  GstMFAudioDecoderClass parent_class;
} GstMFAacDecClass;

static GTypeClass *parent_class = nullptr;

static gboolean gst_mf_aac_dec_set_format (GstMFAudioDecoder * decoder,
    GstMFTransform * transform, GstCaps * caps);

static void
gst_mf_aac_dec_class_init (GstMFAacDecClass * klass, gpointer data)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstMFAudioDecoderClass *decoder_class = GST_MF_AUDIO_DECODER_CLASS (klass);
  GstMFAudioDecoderClassData *cdata = (GstMFAudioDecoderClassData *) data;
  gchar *long_name;

  parent_class = (GTypeClass *) g_type_class_peek_parent (klass);

  long_name = g_strdup_printf ("Media Foundation %s", cdata->device_name);
  gst_element_class_set_metadata (element_class, long_name,
      "Codec/Decoder/Audio",
      "Microsoft Media Foundation AAC Decoder",
      "Seungha Yang <seungha@centricular.com>");
  g_free (long_name);

  gst_element_class_add_static_pad_template (element_class, &sink_template);
  gst_element_class_add_static_pad_template (element_class, &src_template);

  decoder_class->set_format = GST_DEBUG_FUNCPTR (gst_mf_aac_dec_set_format);

  decoder_class->codec_id = MFAudioFormat_AAC;
  decoder_class->enum_flags = cdata->enum_flags;
  decoder_class->device_index = cdata->device_index;

  g_free (cdata->device_name);
  g_free (cdata);
}

static void
gst_mf_aac_dec_init (GstMFAacDec * self)
{
}

/* Portion of HEAACWAVEINFO struct after wfx field
 * plus 2 bytes AudioSpecificConfig() */
typedef struct
{
  WORD wPayloadType;
  WORD wAudioProfileLevelIndication;
  WORD wStructType;
  WORD wReserved1;
  DWORD dwReserved2;

  WORD AudioSpecificConfig;
} AACWaveInfo;

static gboolean
gst_mf_aac_dec_set_format (GstMFAudioDecoder * decoder,
    GstMFTransform * transform, GstCaps * caps)
{
  GstMFAacDec *self = (GstMFAacDec *) decoder;
  HRESULT hr;
  const GValue *value;
  GstStructure *structure;
  GstBuffer *codec_data;
  ComPtr < IMFMediaType > in_type;
  ComPtr < IMFMediaType > out_type;
  AACWaveInfo wave_info;
  GstMapInfo map_info;
  guint channels, rate;
  const guint8 *data;
  GstAudioInfo in_audio_info, out_audio_info;
  GList *output_list, *iter;
  GstCaps *out_caps;

  G_STATIC_ASSERT (sizeof (AACWaveInfo) >= 12);

  if (!gst_audio_info_from_caps (&in_audio_info, caps)) {
    GST_ERROR_OBJECT (self, "Failed to get audio info from caps");
    return FALSE;
  }

  structure = gst_caps_get_structure (caps, 0);
  value = gst_structure_get_value (structure, "codec_data");
  if (!value) {
    GST_ERROR_OBJECT (self, "Missing codec_data");
    return FALSE;
  }

  codec_data = gst_value_get_buffer (value);
  if (!codec_data || gst_buffer_get_size (codec_data) < 2) {
    GST_ERROR_OBJECT (self, "Invalid codec_data");
    return FALSE;
  }

  if (!gst_buffer_map (codec_data, &map_info, GST_MAP_READ)) {
    GST_ERROR_OBJECT (self, "Invalid codec_data buffer");
    return FALSE;
  }

  data = (guint8 *) map_info.data;
  channels = gst_codec_utils_aac_get_channels (data, map_info.size);
  rate = gst_codec_utils_aac_get_sample_rate (data, map_info.size);

  /* Fallback to channels/rate values specified in caps */
  if (channels == 0)
    channels = in_audio_info.channels;

  if (rate == 0)
    rate = in_audio_info.rate;

  memset (&wave_info, 0, sizeof (AACWaveInfo));
  wave_info.wAudioProfileLevelIndication = 0xfe;
  memcpy (&wave_info.AudioSpecificConfig, data, 2);

  hr = MFCreateMediaType (&in_type);
  if (!gst_mf_result (hr))
    return FALSE;

  hr = in_type->SetGUID (MF_MT_MAJOR_TYPE, MFMediaType_Audio);
  if (!gst_mf_result (hr))
    return FALSE;

  hr = in_type->SetGUID (MF_MT_SUBTYPE, MFAudioFormat_AAC);
  if (!gst_mf_result (hr))
    return FALSE;

  hr = in_type->SetUINT32 (MF_MT_AAC_PAYLOAD_TYPE, 0);
  if (!gst_mf_result (hr))
    return FALSE;

  hr = in_type->SetUINT32 (MF_MT_AUDIO_NUM_CHANNELS, channels);
  if (!gst_mf_result (hr))
    return FALSE;

  hr = in_type->SetUINT32 (MF_MT_AUDIO_SAMPLES_PER_SECOND, rate);
  if (!gst_mf_result (hr))
    return FALSE;

  /* FIXME: should parse this somehow? */
  hr = in_type->SetUINT32 (MF_MT_AAC_AUDIO_PROFILE_LEVEL_INDICATION, 0xfe);
  if (!gst_mf_result (hr))
    return FALSE;

  hr = in_type->SetBlob (MF_MT_USER_DATA, (UINT8 *) & wave_info, 12);
  if (!gst_mf_result (hr))
    return FALSE;

  if (!gst_mf_transform_set_input_type (transform, in_type.Get ())) {
    GST_ERROR_OBJECT (self, "Failed to set format");
    return FALSE;
  }

  if (!gst_mf_transform_get_output_available_types (transform, &output_list)) {
    GST_ERROR_OBJECT (self, "Failed to get output types");
    return FALSE;
  }

  for (iter = output_list; iter; iter = g_list_next (iter)) {
    GUID guid;
    IMFMediaType *type = (IMFMediaType *) iter->data;
    UINT32 bps;

    hr = type->GetGUID (MF_MT_MAJOR_TYPE, &guid);
    if (!gst_mf_result (hr))
      continue;

    if (!IsEqualGUID (guid, MFMediaType_Audio))
      continue;

    hr = type->GetGUID (MF_MT_SUBTYPE, &guid);
    if (!gst_mf_result (hr))
      continue;

    if (!IsEqualGUID (guid, MFAudioFormat_PCM))
      continue;

    hr = type->GetUINT32 (MF_MT_AUDIO_BITS_PER_SAMPLE, &bps);
    if (!gst_mf_result (hr))
      continue;

    if (bps != 16)
      continue;

    out_type = type;
    break;
  }

  g_list_free_full (output_list, (GDestroyNotify) gst_mf_media_type_release);

  if (!out_type) {
    GST_ERROR_OBJECT (self, "Failed to select output type");
    return FALSE;
  }

  if (!gst_mf_transform_set_output_type (transform, out_type.Get ())) {
    GST_ERROR_OBJECT (self, "Failed to select output type");
    return FALSE;
  }

  out_caps = gst_mf_media_type_to_caps (out_type.Get ());
  if (!out_caps) {
    GST_ERROR_OBJECT (self, "Failed to get output caps");
    return FALSE;
  }

  GST_DEBUG_OBJECT (self, "Output caps %" GST_PTR_FORMAT, out_caps);

  if (!gst_audio_info_from_caps (&out_audio_info, out_caps)) {
    GST_ERROR_OBJECT (self,
        "Failed to convert caps to audio info %" GST_PTR_FORMAT, out_caps);
    gst_caps_unref (out_caps);
  }

  gst_caps_unref (out_caps);

  return gst_audio_decoder_set_output_format (GST_AUDIO_DECODER (self),
      &out_audio_info);
}

static void
gst_mf_aac_dec_register (GstPlugin * plugin, guint rank,
    const gchar * device_name, guint32 enum_flags, guint device_index)
{
  GType type;
  GstMFAudioDecoderClassData *cdata;
  GTypeInfo type_info = {
    sizeof (GstMFAacDecClass),
    nullptr,
    nullptr,
    (GClassInitFunc) gst_mf_aac_dec_class_init,
    nullptr,
    nullptr,
    sizeof (GstMFAacDec),
    0,
    (GInstanceInitFunc) gst_mf_aac_dec_init,
  };

  cdata = g_new0 (GstMFAudioDecoderClassData, 1);
  cdata->device_name = g_strdup (device_name);
  cdata->enum_flags = enum_flags;
  cdata->device_index = device_index;
  type_info.class_data = cdata;

  type = g_type_register_static (GST_TYPE_MF_AUDIO_DECODER, "GstMFAacDec",
      &type_info, (GTypeFlags) 0);

  if (!gst_element_register (plugin, "mfaacdec", rank, type))
    GST_WARNING ("Failed to register plugin");
}

static gboolean
gst_mf_aac_dec_plugin_init_internal (GstPlugin * plugin, guint rank,
    GstMFTransform * transform, guint device_index, guint32 enum_flags)
{
  gchar *device_name = nullptr;

  if (!gst_mf_transform_open (transform))
    return FALSE;

  g_object_get (transform, "device-name", &device_name, nullptr);
  if (!device_name) {
    GST_WARNING_OBJECT (transform, "Unknown device name");
    return FALSE;
  }

  gst_mf_aac_dec_register (plugin, rank, device_name, enum_flags, device_index);
  g_free (device_name);

  return TRUE;
}

void
gst_mf_aac_dec_plugin_init (GstPlugin * plugin, guint rank)
{
  GstMFTransformEnumParams enum_params = { 0, };
  MFT_REGISTER_TYPE_INFO input_type;
  GstMFTransform *transform;
  gint i;
  gboolean do_next;

  GST_DEBUG_CATEGORY_INIT (gst_mf_aac_dec_debug, "mfaacdec", 0, "mfaacdec");

  input_type.guidMajorType = MFMediaType_Audio;
  input_type.guidSubtype = MFAudioFormat_AAC;

  enum_params.category = MFT_CATEGORY_AUDIO_DECODER;
  enum_params.enum_flags = (MFT_ENUM_FLAG_SYNCMFT |
      MFT_ENUM_FLAG_SORTANDFILTER | MFT_ENUM_FLAG_SORTANDFILTER_APPROVED_ONLY);
  enum_params.input_typeinfo = &input_type;

  i = 0;
  do {
    enum_params.device_index = i++;
    transform = gst_mf_transform_new (&enum_params);
    do_next = TRUE;

    if (!transform) {
      do_next = FALSE;
    } else {
      if (gst_mf_aac_dec_plugin_init_internal (plugin, rank, transform,
              enum_params.device_index, enum_params.enum_flags)) {
        do_next = FALSE;
      }
      gst_clear_object (&transform);
    }
  } while (do_next);
}
