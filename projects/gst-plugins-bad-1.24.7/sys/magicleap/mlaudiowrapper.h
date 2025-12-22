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

#pragma once

#include <gst/gst.h>
#include <gst/audio/gstaudiosink.h>

#include <ml_audio.h>

G_BEGIN_DECLS

typedef struct _GstMLAudioWrapper GstMLAudioWrapper;

GstMLAudioWrapper * gst_ml_audio_wrapper_new (gpointer app);
void gst_ml_audio_wrapper_free (GstMLAudioWrapper *self);

MLResult gst_ml_audio_wrapper_create_sound (GstMLAudioWrapper *self,
    const MLAudioBufferFormat *format,
    uint32_t buffer_size,
    MLAudioBufferCallback callback,
    gpointer user_data);
MLResult gst_ml_audio_wrapper_pause_sound (GstMLAudioWrapper *self);
MLResult gst_ml_audio_wrapper_resume_sound (GstMLAudioWrapper *self);
MLResult gst_ml_audio_wrapper_stop_sound (GstMLAudioWrapper *self);
MLResult gst_ml_audio_wrapper_get_latency (GstMLAudioWrapper *self,
    float *out_latency_in_msec);
MLResult gst_ml_audio_wrapper_get_buffer (GstMLAudioWrapper *self,
    MLAudioBuffer *out_buffer);
MLResult gst_ml_audio_wrapper_release_buffer (GstMLAudioWrapper *self);

void gst_ml_audio_wrapper_set_handle (GstMLAudioWrapper *self, MLHandle handle);
void gst_ml_audio_wrapper_set_node (GstMLAudioWrapper *self, gpointer
    audio_node);

typedef gboolean (*GstMLAudioWrapperCallback) (GstMLAudioWrapper *self,
    gpointer user_data);
gboolean gst_ml_audio_wrapper_invoke_sync (GstMLAudioWrapper *self,
    GstMLAudioWrapperCallback callback, gpointer user_data);

G_END_DECLS
