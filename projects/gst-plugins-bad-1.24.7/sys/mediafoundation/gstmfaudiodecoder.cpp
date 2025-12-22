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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include "gstmfaudiodecoder.h"
#include <wrl.h>
#include <string.h>

/* *INDENT-OFF* */
using namespace Microsoft::WRL;
/* *INDENT-ON* */

GST_DEBUG_CATEGORY (gst_mf_audio_decoder_debug);
#define GST_CAT_DEFAULT gst_mf_audio_decoder_debug

/**
 * GstMFAudioDecoder:
 *
 * Base class for MediaFoundation audio decoders
 *
 * Since: 1.22
 */
#define gst_mf_audio_decoder_parent_class parent_class
G_DEFINE_ABSTRACT_TYPE_WITH_CODE (GstMFAudioDecoder, gst_mf_audio_decoder,
    GST_TYPE_AUDIO_DECODER,
    GST_DEBUG_CATEGORY_INIT (gst_mf_audio_decoder_debug, "mfaudiodecoder", 0,
        "mfaudiodecoder"));

static gboolean gst_mf_audio_decoder_open (GstAudioDecoder * dec);
static gboolean gst_mf_audio_decoder_close (GstAudioDecoder * dec);
static gboolean gst_mf_audio_decoder_set_format (GstAudioDecoder * dec,
    GstCaps * caps);
static GstFlowReturn gst_mf_audio_decoder_handle_frame (GstAudioDecoder * dec,
    GstBuffer * buffer);
static GstFlowReturn gst_mf_audio_decoder_drain (GstAudioDecoder * dec);
static void gst_mf_audio_decoder_flush (GstAudioDecoder * dec, gboolean hard);

static void
gst_mf_audio_decoder_class_init (GstMFAudioDecoderClass * klass)
{
  GstAudioDecoderClass *audiodec_class = GST_AUDIO_DECODER_CLASS (klass);

  audiodec_class->open = GST_DEBUG_FUNCPTR (gst_mf_audio_decoder_open);
  audiodec_class->close = GST_DEBUG_FUNCPTR (gst_mf_audio_decoder_close);
  audiodec_class->set_format =
      GST_DEBUG_FUNCPTR (gst_mf_audio_decoder_set_format);
  audiodec_class->handle_frame =
      GST_DEBUG_FUNCPTR (gst_mf_audio_decoder_handle_frame);
  audiodec_class->flush = GST_DEBUG_FUNCPTR (gst_mf_audio_decoder_flush);

  gst_type_mark_as_plugin_api (GST_TYPE_MF_AUDIO_DECODER,
      (GstPluginAPIFlags) 0);
}

static void
gst_mf_audio_decoder_init (GstMFAudioDecoder * self)
{
  gst_audio_decoder_set_drainable (GST_AUDIO_DECODER (self), TRUE);
}

static gboolean
gst_mf_audio_decoder_open (GstAudioDecoder * dec)
{
  GstMFAudioDecoder *self = GST_MF_AUDIO_DECODER (dec);
  GstMFAudioDecoderClass *klass = GST_MF_AUDIO_DECODER_GET_CLASS (dec);
  GstMFTransformEnumParams enum_params = { 0, };
  MFT_REGISTER_TYPE_INFO input_type;

  input_type.guidMajorType = MFMediaType_Audio;
  input_type.guidSubtype = klass->codec_id;

  enum_params.category = MFT_CATEGORY_AUDIO_DECODER;
  enum_params.enum_flags = klass->enum_flags;
  enum_params.input_typeinfo = &input_type;
  enum_params.device_index = klass->device_index;

  GST_DEBUG_OBJECT (self, "Create MFT with enum flags 0x%x, device index %d",
      klass->enum_flags, klass->device_index);

  self->transform = gst_mf_transform_new (&enum_params);
  if (!self->transform) {
    GST_ERROR_OBJECT (self, "Cannot create MFT object");
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_mf_audio_decoder_close (GstAudioDecoder * dec)
{
  GstMFAudioDecoder *self = GST_MF_AUDIO_DECODER (dec);

  gst_clear_object (&self->transform);

  return TRUE;
}

static gboolean
gst_mf_audio_decoder_set_format (GstAudioDecoder * dec, GstCaps * caps)
{
  GstMFAudioDecoder *self = GST_MF_AUDIO_DECODER (dec);
  GstMFAudioDecoderClass *klass = GST_MF_AUDIO_DECODER_GET_CLASS (dec);

  g_assert (klass->set_format != nullptr);

  GST_DEBUG_OBJECT (self, "Set format");

  gst_mf_audio_decoder_drain (dec);

  if (!gst_mf_transform_open (self->transform)) {
    GST_ERROR_OBJECT (self, "Failed to open MFT");
    return FALSE;
  }

  if (!klass->set_format (self, self->transform, caps)) {
    GST_ERROR_OBJECT (self, "Failed to set format");
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_mf_audio_decoder_process_input (GstMFAudioDecoder * self,
    GstBuffer * buffer)
{
  HRESULT hr;
  ComPtr < IMFSample > sample;
  ComPtr < IMFMediaBuffer > media_buffer;
  BYTE *data;
  gboolean res = FALSE;
  GstMapInfo info;

  if (!gst_buffer_map (buffer, &info, GST_MAP_READ)) {
    GST_ELEMENT_ERROR (self,
        RESOURCE, READ, ("Couldn't map input buffer"), (nullptr));
    return FALSE;
  }

  GST_TRACE_OBJECT (self, "Process buffer %" GST_PTR_FORMAT, buffer);

  hr = MFCreateSample (&sample);
  if (!gst_mf_result (hr))
    goto done;

  hr = MFCreateMemoryBuffer (info.size, &media_buffer);
  if (!gst_mf_result (hr))
    goto done;

  hr = media_buffer->Lock (&data, nullptr, nullptr);
  if (!gst_mf_result (hr))
    goto done;

  memcpy (data, info.data, info.size);
  media_buffer->Unlock ();

  hr = media_buffer->SetCurrentLength (info.size);
  if (!gst_mf_result (hr))
    goto done;

  hr = sample->AddBuffer (media_buffer.Get ());
  if (!gst_mf_result (hr))
    goto done;

  if (!gst_mf_transform_process_input (self->transform, sample.Get ())) {
    GST_ERROR_OBJECT (self, "Failed to process input");
    goto done;
  }

  res = TRUE;

done:
  gst_buffer_unmap (buffer, &info);

  return res;
}

static GstFlowReturn
gst_mf_audio_decoder_process_output (GstMFAudioDecoder * self)
{
  HRESULT hr;
  BYTE *data = nullptr;
  ComPtr < IMFMediaBuffer > media_buffer;
  ComPtr < IMFSample > sample;
  GstBuffer *buffer;
  GstFlowReturn res = GST_FLOW_ERROR;
  DWORD buffer_len = 0;

  res = gst_mf_transform_get_output (self->transform, &sample);

  if (res != GST_FLOW_OK)
    return res;

  hr = sample->GetBufferByIndex (0, &media_buffer);
  if (!gst_mf_result (hr))
    return GST_FLOW_ERROR;

  hr = media_buffer->Lock (&data, nullptr, &buffer_len);
  if (!gst_mf_result (hr))
    return GST_FLOW_ERROR;

  /* Can happen while draining */
  if (buffer_len == 0 || !data) {
    GST_DEBUG_OBJECT (self, "Empty media buffer");
    media_buffer->Unlock ();
    return GST_FLOW_OK;
  }

  buffer = gst_audio_decoder_allocate_output_buffer (GST_AUDIO_DECODER (self),
      buffer_len);
  gst_buffer_fill (buffer, 0, data, buffer_len);
  media_buffer->Unlock ();

  return gst_audio_decoder_finish_frame (GST_AUDIO_DECODER (self), buffer, 1);
}

static GstFlowReturn
gst_mf_audio_decoder_handle_frame (GstAudioDecoder * dec, GstBuffer * buffer)
{
  GstMFAudioDecoder *self = GST_MF_AUDIO_DECODER (dec);
  GstFlowReturn ret;

  if (!buffer)
    return gst_mf_audio_decoder_drain (dec);

  if (!gst_mf_audio_decoder_process_input (self, buffer)) {
    GST_ERROR_OBJECT (self, "Failed to process input");
    return GST_FLOW_ERROR;
  }

  do {
    ret = gst_mf_audio_decoder_process_output (self);
  } while (ret == GST_FLOW_OK);

  if (ret == GST_MF_TRANSFORM_FLOW_NEED_DATA)
    ret = GST_FLOW_OK;

  return ret;
}

static GstFlowReturn
gst_mf_audio_decoder_drain (GstAudioDecoder * dec)
{
  GstMFAudioDecoder *self = GST_MF_AUDIO_DECODER (dec);
  GstFlowReturn ret = GST_FLOW_OK;

  if (!self->transform)
    return GST_FLOW_OK;

  gst_mf_transform_drain (self->transform);

  do {
    ret = gst_mf_audio_decoder_process_output (self);
  } while (ret == GST_FLOW_OK);

  if (ret == GST_MF_TRANSFORM_FLOW_NEED_DATA)
    ret = GST_FLOW_OK;

  return ret;
}

static void
gst_mf_audio_decoder_flush (GstAudioDecoder * dec, gboolean hard)
{
  GstMFAudioDecoder *self = GST_MF_AUDIO_DECODER (dec);

  if (!self->transform)
    return;

  gst_mf_transform_flush (self->transform);
}
