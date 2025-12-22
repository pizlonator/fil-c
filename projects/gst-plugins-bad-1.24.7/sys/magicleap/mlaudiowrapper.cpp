/*
 * Copyright (C) 2019 Collabora Ltd.
 *   Author: Xavier Claessens <xavier.claessens@collabora.com>
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

#include "mlaudiowrapper.h"

#include <ml_audio.h>

#include <lumin/node/AudioNode.h>
#include <lumin/BaseApp.h>
#include <lumin/Prism.h>

GST_DEBUG_CATEGORY_EXTERN (mgl_debug);
#define GST_CAT_DEFAULT mgl_debug

using lumin::BaseApp;
using lumin::AudioNode;
using lumin::AudioBuffer;
using lumin::AudioBufferFormat;
using lumin::AudioSampleFormat;

struct _GstMLAudioWrapper
{
  BaseApp *app;
  AudioNode *node;
  MLHandle handle;
};

AudioBufferFormat
convert_buffer_format(const MLAudioBufferFormat *format)
{
  AudioBufferFormat ret;
  ret.channel_count = format->channel_count;
  ret.samples_per_second = format->samples_per_second;
  ret.bits_per_sample = format->bits_per_sample;
  ret.valid_bits_per_sample = format->valid_bits_per_sample;
  switch (format->sample_format) {
  case MLAudioSampleFormat_Int:
    ret.sample_format = AudioSampleFormat::Integer;
    break;
  case MLAudioSampleFormat_Float:
    ret.sample_format = AudioSampleFormat::Float;
    break;
  default:
    g_warn_if_reached ();
    ret.sample_format = (AudioSampleFormat)format->sample_format;
  };
  ret.reserved = format->reserved;

  return ret;
}

GstMLAudioWrapper *
gst_ml_audio_wrapper_new (gpointer app)
{
  GstMLAudioWrapper *self;

  self = g_new0 (GstMLAudioWrapper, 1);
  self->app = reinterpret_cast<BaseApp *>(app);
  self->node = nullptr;
  self->handle = ML_INVALID_HANDLE;

  return self;
}

void
gst_ml_audio_wrapper_free (GstMLAudioWrapper *self)
{
  if (self->node) {
    self->app->RunOnMainThreadSync ([self] {
      /* Stop playing sound, but user is responsible to destroy the node */
      self->node->stopSound ();
    });
  } else {
    MLAudioDestroySound (self->handle);
  }

  g_free (self);
}

MLResult
gst_ml_audio_wrapper_create_sound (GstMLAudioWrapper *self,
    const MLAudioBufferFormat *format,
    uint32_t buffer_size,
    MLAudioBufferCallback callback,
    gpointer user_data)
{
  if (self->node) {
    auto format2 = convert_buffer_format (format);
    bool success = FALSE;
    success = self->node->createSoundWithOutputStream (&format2,
        buffer_size, callback, user_data);
    if (success)
      self->node->startSound ();
    return success ? MLResult_Ok : MLResult_UnspecifiedFailure;
  }

  MLResult result = MLAudioCreateSoundWithOutputStream (format, buffer_size,
        callback, user_data, &self->handle);
  if (result == MLResult_Ok)
    result = MLAudioStartSound (self->handle);

  return result;
}

MLResult
gst_ml_audio_wrapper_pause_sound (GstMLAudioWrapper *self)
{
  g_return_val_if_fail (self->handle != ML_INVALID_HANDLE,
      MLResult_UnspecifiedFailure);
  return MLAudioPauseSound (self->handle);
}

MLResult
gst_ml_audio_wrapper_resume_sound (GstMLAudioWrapper *self)
{
  g_return_val_if_fail (self->handle != ML_INVALID_HANDLE,
      MLResult_UnspecifiedFailure);
  return MLAudioResumeSound (self->handle);
}

MLResult
gst_ml_audio_wrapper_stop_sound (GstMLAudioWrapper *self)
{
  g_return_val_if_fail (self->handle != ML_INVALID_HANDLE,
      MLResult_UnspecifiedFailure);
  return MLAudioStopSound (self->handle);
}

MLResult
gst_ml_audio_wrapper_get_latency (GstMLAudioWrapper *self,
    float *out_latency_in_msec)
{
  if (self->handle == ML_INVALID_HANDLE) {
    *out_latency_in_msec = 0;
    return MLResult_Ok;
  }

  return MLAudioGetOutputStreamLatency (self->handle, out_latency_in_msec);
}

MLResult
gst_ml_audio_wrapper_get_buffer (GstMLAudioWrapper *self,
    MLAudioBuffer *out_buffer)
{
  return MLAudioGetOutputStreamBuffer (self->handle, out_buffer);
}

MLResult
gst_ml_audio_wrapper_release_buffer (GstMLAudioWrapper *self)
{
  return MLAudioReleaseOutputStreamBuffer (self->handle);
}

void
gst_ml_audio_wrapper_set_handle (GstMLAudioWrapper *self, MLHandle handle)
{
  g_return_if_fail (self->handle == ML_INVALID_HANDLE || self->handle == handle);
  self->handle = handle;
}

void
gst_ml_audio_wrapper_set_node (GstMLAudioWrapper *self,
    gpointer node)
{
  g_return_if_fail (self->node == nullptr);
  self->node = reinterpret_cast<AudioNode *>(node);
}

gboolean
gst_ml_audio_wrapper_invoke_sync (GstMLAudioWrapper *self,
    GstMLAudioWrapperCallback callback, gpointer user_data)
{
  gboolean ret;

  if (self->app) {
    self->app->RunOnMainThreadSync ([self, callback, user_data, &ret] {
      ret = callback (self, user_data);
    });
  } else {
    ret = callback (self, user_data);
  }

  return ret;
}
