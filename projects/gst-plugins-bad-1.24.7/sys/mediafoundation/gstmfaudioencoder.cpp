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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include "gstmfaudioencoder.h"
#include <wrl.h>
#include <string.h>

/* *INDENT-OFF* */
using namespace Microsoft::WRL;
/* *INDENT-ON* */

GST_DEBUG_CATEGORY (gst_mf_audio_encoder_debug);
#define GST_CAT_DEFAULT gst_mf_audio_encoder_debug

/**
 * GstMFAudioEncoder:
 *
 * Base class for MediaFoundation audio encoders
 *
 * Since: 1.22
 */
#define gst_mf_audio_encoder_parent_class parent_class
G_DEFINE_ABSTRACT_TYPE_WITH_CODE (GstMFAudioEncoder, gst_mf_audio_encoder,
    GST_TYPE_AUDIO_ENCODER,
    GST_DEBUG_CATEGORY_INIT (gst_mf_audio_encoder_debug, "mfaudioencoder", 0,
        "mfaudioencoder"));

static gboolean gst_mf_audio_encoder_open (GstAudioEncoder * enc);
static gboolean gst_mf_audio_encoder_close (GstAudioEncoder * enc);
static gboolean gst_mf_audio_encoder_set_format (GstAudioEncoder * enc,
    GstAudioInfo * info);
static GstFlowReturn gst_mf_audio_encoder_handle_frame (GstAudioEncoder * enc,
    GstBuffer * buffer);
static GstFlowReturn gst_mf_audio_encoder_drain (GstAudioEncoder * enc);
static void gst_mf_audio_encoder_flush (GstAudioEncoder * enc);

static void
gst_mf_audio_encoder_class_init (GstMFAudioEncoderClass * klass)
{
  GstAudioEncoderClass *audioenc_class = GST_AUDIO_ENCODER_CLASS (klass);

  audioenc_class->open = GST_DEBUG_FUNCPTR (gst_mf_audio_encoder_open);
  audioenc_class->close = GST_DEBUG_FUNCPTR (gst_mf_audio_encoder_close);
  audioenc_class->set_format =
      GST_DEBUG_FUNCPTR (gst_mf_audio_encoder_set_format);
  audioenc_class->handle_frame =
      GST_DEBUG_FUNCPTR (gst_mf_audio_encoder_handle_frame);
  audioenc_class->flush = GST_DEBUG_FUNCPTR (gst_mf_audio_encoder_flush);

  gst_type_mark_as_plugin_api (GST_TYPE_MF_AUDIO_ENCODER,
      (GstPluginAPIFlags) 0);
}

static void
gst_mf_audio_encoder_init (GstMFAudioEncoder * self)
{
  gst_audio_encoder_set_drainable (GST_AUDIO_ENCODER (self), TRUE);
}

static gboolean
gst_mf_audio_encoder_open (GstAudioEncoder * enc)
{
  GstMFAudioEncoder *self = GST_MF_AUDIO_ENCODER (enc);
  GstMFAudioEncoderClass *klass = GST_MF_AUDIO_ENCODER_GET_CLASS (enc);
  GstMFTransformEnumParams enum_params = { 0, };
  MFT_REGISTER_TYPE_INFO output_type;

  output_type.guidMajorType = MFMediaType_Audio;
  output_type.guidSubtype = klass->codec_id;

  enum_params.category = MFT_CATEGORY_AUDIO_ENCODER;
  enum_params.enum_flags = klass->enum_flags;
  enum_params.output_typeinfo = &output_type;
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
gst_mf_audio_encoder_close (GstAudioEncoder * enc)
{
  GstMFAudioEncoder *self = GST_MF_AUDIO_ENCODER (enc);

  gst_clear_object (&self->transform);

  return TRUE;
}

static gboolean
gst_mf_audio_encoder_set_format (GstAudioEncoder * enc, GstAudioInfo * info)
{
  GstMFAudioEncoder *self = GST_MF_AUDIO_ENCODER (enc);
  GstMFAudioEncoderClass *klass = GST_MF_AUDIO_ENCODER_GET_CLASS (enc);
  ComPtr < IMFMediaType > in_type;
  ComPtr < IMFMediaType > out_type;

  GST_DEBUG_OBJECT (self, "Set format");

  gst_mf_audio_encoder_drain (enc);

  if (!gst_mf_transform_open (self->transform)) {
    GST_ERROR_OBJECT (self, "Failed to open MFT");
    return FALSE;
  }

  g_assert (klass->get_output_type != nullptr);
  if (!klass->get_output_type (self, info, &out_type)) {
    GST_ERROR_OBJECT (self, "subclass failed to set output type");
    return FALSE;
  }

  gst_mf_dump_attributes (out_type.Get (), "Set output type", GST_LEVEL_DEBUG);

  if (!gst_mf_transform_set_output_type (self->transform, out_type.Get ())) {
    GST_ERROR_OBJECT (self, "Couldn't set output type");
    return FALSE;
  }

  g_assert (klass->get_input_type != nullptr);
  if (!klass->get_input_type (self, info, &in_type)) {
    GST_ERROR_OBJECT (self, "subclass didn't provide input type");
    return FALSE;
  }

  gst_mf_dump_attributes (in_type.Get (), "Set input type", GST_LEVEL_DEBUG);

  if (!gst_mf_transform_set_input_type (self->transform, in_type.Get ())) {
    GST_ERROR_OBJECT (self, "Couldn't set input media type");
    return FALSE;
  }

  g_assert (klass->set_src_caps != nullptr);
  if (!klass->set_src_caps (self, info))
    return FALSE;

  g_assert (klass->frame_samples > 0);
  gst_audio_encoder_set_frame_samples_min (enc, klass->frame_samples);
  gst_audio_encoder_set_frame_samples_max (enc, klass->frame_samples);
  gst_audio_encoder_set_frame_max (enc, 1);

  /* mediafoundation encoder needs timestamp and duration */
  self->sample_count = 0;
  self->sample_duration_in_mf = gst_util_uint64_scale (klass->frame_samples,
      10000000, GST_AUDIO_INFO_RATE (info));

  GST_DEBUG_OBJECT (self,
      "Calculated sample duration %" GST_TIME_FORMAT,
      GST_TIME_ARGS (self->sample_duration_in_mf * 100));

  return TRUE;
}

static gboolean
gst_mf_audio_encoder_process_input (GstMFAudioEncoder * self,
    GstBuffer * buffer)
{
  HRESULT hr;
  ComPtr < IMFSample > sample;
  ComPtr < IMFMediaBuffer > media_buffer;
  BYTE *data;
  gboolean res = FALSE;
  GstMapInfo info;
  guint64 timestamp;

  if (!gst_buffer_map (buffer, &info, GST_MAP_READ)) {
    GST_ELEMENT_ERROR (self,
        RESOURCE, READ, ("Couldn't map input buffer"), (nullptr));
    return FALSE;
  }

  GST_TRACE_OBJECT (self, "Process buffer %" GST_PTR_FORMAT, buffer);

  timestamp = self->sample_count * self->sample_duration_in_mf;

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

  hr = sample->SetSampleTime (timestamp);
  if (!gst_mf_result (hr))
    goto done;

  hr = sample->SetSampleDuration (self->sample_duration_in_mf);
  if (!gst_mf_result (hr))
    goto done;

  if (!gst_mf_transform_process_input (self->transform, sample.Get ())) {
    GST_ERROR_OBJECT (self, "Failed to process input");
    goto done;
  }

  self->sample_count++;

  res = TRUE;

done:
  gst_buffer_unmap (buffer, &info);

  return res;
}

static GstFlowReturn
gst_mf_audio_encoder_process_output (GstMFAudioEncoder * self)
{
  GstMFAudioEncoderClass *klass = GST_MF_AUDIO_ENCODER_GET_CLASS (self);
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

  buffer = gst_audio_encoder_allocate_output_buffer (GST_AUDIO_ENCODER (self),
      buffer_len);
  gst_buffer_fill (buffer, 0, data, buffer_len);
  media_buffer->Unlock ();

  return gst_audio_encoder_finish_frame (GST_AUDIO_ENCODER (self), buffer,
      klass->frame_samples);
}

static GstFlowReturn
gst_mf_audio_encoder_handle_frame (GstAudioEncoder * enc, GstBuffer * buffer)
{
  GstMFAudioEncoder *self = GST_MF_AUDIO_ENCODER (enc);
  GstFlowReturn ret;

  if (!buffer)
    return gst_mf_audio_encoder_drain (enc);

  if (!gst_mf_audio_encoder_process_input (self, buffer)) {
    GST_ERROR_OBJECT (self, "Failed to process input");
    return GST_FLOW_ERROR;
  }

  do {
    ret = gst_mf_audio_encoder_process_output (self);
  } while (ret == GST_FLOW_OK);

  if (ret == GST_MF_TRANSFORM_FLOW_NEED_DATA)
    ret = GST_FLOW_OK;

  return ret;
}

static GstFlowReturn
gst_mf_audio_encoder_drain (GstAudioEncoder * enc)
{
  GstMFAudioEncoder *self = GST_MF_AUDIO_ENCODER (enc);
  GstFlowReturn ret = GST_FLOW_OK;

  if (!self->transform)
    return GST_FLOW_OK;

  gst_mf_transform_drain (self->transform);

  do {
    ret = gst_mf_audio_encoder_process_output (self);
  } while (ret == GST_FLOW_OK);

  if (ret == GST_MF_TRANSFORM_FLOW_NEED_DATA)
    ret = GST_FLOW_OK;

  return ret;
}

static void
gst_mf_audio_encoder_flush (GstAudioEncoder * enc)
{
  GstMFAudioEncoder *self = GST_MF_AUDIO_ENCODER (enc);

  if (!self->transform)
    return;

  gst_mf_transform_flush (self->transform);
}
