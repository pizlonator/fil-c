/* GStreamer
 * Copyright (C) 2015 Centricular Ltd.,
 *                    Arun Raghavan <mail@arunraghavan.net>
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

#include "openslescommon.h"

#ifndef SL_ANDROID_RECORDING_PRESET_VOICE_COMMUNICATION
/* This was added in Android API level 14 */
#define SL_ANDROID_RECORDING_PRESET_VOICE_COMMUNICATION ((SLuint32) 0x00000004)
#endif

GType
gst_opensles_recording_preset_get_type (void)
{
  static const GEnumValue values[] = {
    {GST_OPENSLES_RECORDING_PRESET_NONE,
        "GST_OPENSLES_RECORDING_PRESET_NONE", "none"},
    {GST_OPENSLES_RECORDING_PRESET_GENERIC,
        "GST_OPENSLES_RECORDING_PRESET_GENERIC", "generic"},
    {GST_OPENSLES_RECORDING_PRESET_CAMCORDER,
        "GST_OPENSLES_RECORDING_PRESET_CAMCORDER", "camcorder"},
    {GST_OPENSLES_RECORDING_PRESET_VOICE_RECOGNITION,
        "GST_OPENSLES_RECORDING_PRESET_VOICE_RECOGNITION", "voice-recognition"},
    {GST_OPENSLES_RECORDING_PRESET_VOICE_COMMUNICATION,
          "GST_OPENSLES_RECORDING_PRESET_VOICE_COMMUNICATION",
        "voice-communication"},
    {0, NULL, NULL}
  };
  static GType id = 0;

  if (g_once_init_enter ((gsize *) & id)) {
    GType _id;

    _id = g_enum_register_static ("GstOpenSLESRecordingPreset", values);

    g_once_init_leave ((gsize *) & id, _id);
  }

  return id;
}

SLint32
gst_to_opensles_recording_preset (GstOpenSLESRecordingPreset preset)
{
  switch (preset) {
    case GST_OPENSLES_RECORDING_PRESET_NONE:
      return SL_ANDROID_RECORDING_PRESET_NONE;

    case GST_OPENSLES_RECORDING_PRESET_GENERIC:
      return SL_ANDROID_RECORDING_PRESET_GENERIC;

    case GST_OPENSLES_RECORDING_PRESET_CAMCORDER:
      return SL_ANDROID_RECORDING_PRESET_CAMCORDER;

    case GST_OPENSLES_RECORDING_PRESET_VOICE_RECOGNITION:
      return SL_ANDROID_RECORDING_PRESET_VOICE_RECOGNITION;

    case GST_OPENSLES_RECORDING_PRESET_VOICE_COMMUNICATION:
      return SL_ANDROID_RECORDING_PRESET_VOICE_COMMUNICATION;

    default:
      GST_ERROR ("Unsupported preset: %d", (int) preset);
      return SL_ANDROID_RECORDING_PRESET_NONE;
  }
}

GType
gst_opensles_stream_type_get_type (void)
{
  static const GEnumValue values[] = {
    {GST_OPENSLES_STREAM_TYPE_VOICE,
        "GST_OPENSLES_STREAM_TYPE_VOICE", "voice"},
    {GST_OPENSLES_STREAM_TYPE_SYSTEM,
        "GST_OPENSLES_STREAM_TYPE_SYSTEM", "system"},
    {GST_OPENSLES_STREAM_TYPE_RING,
        "GST_OPENSLES_STREAM_TYPE_RING", "ring"},
    {GST_OPENSLES_STREAM_TYPE_MEDIA,
        "GST_OPENSLES_STREAM_TYPE_MEDIA", "media"},
    {GST_OPENSLES_STREAM_TYPE_ALARM,
        "GST_OPENSLES_STREAM_TYPE_ALARM", "alarm"},
    {GST_OPENSLES_STREAM_TYPE_NOTIFICATION,
        "GST_OPENSLES_STREAM_TYPE_NOTIFICATION", "notification"},
    {GST_OPENSLES_STREAM_TYPE_NONE,
        "GST_OPENSLES_STREAM_TYPE_NONE", "none"},
    {0, NULL, NULL}
  };
  static GType id = 0;

  if (g_once_init_enter ((gsize *) & id)) {
    GType _id;

    _id = g_enum_register_static ("GstOpenSLESStreamType", values);

    g_once_init_leave ((gsize *) & id, _id);
  }

  return id;
}


SLint32
gst_to_opensles_stream_type (GstOpenSLESStreamType stream_type)
{
  switch (stream_type) {
    case GST_OPENSLES_STREAM_TYPE_VOICE:
      return SL_ANDROID_STREAM_VOICE;

    case GST_OPENSLES_STREAM_TYPE_SYSTEM:
      return SL_ANDROID_STREAM_SYSTEM;

    case GST_OPENSLES_STREAM_TYPE_RING:
      return SL_ANDROID_STREAM_RING;

    case GST_OPENSLES_STREAM_TYPE_MEDIA:
      return SL_ANDROID_STREAM_MEDIA;

    case GST_OPENSLES_STREAM_TYPE_ALARM:
      return SL_ANDROID_STREAM_ALARM;

    case GST_OPENSLES_STREAM_TYPE_NOTIFICATION:
      return SL_ANDROID_STREAM_NOTIFICATION;

    default:
      GST_ERROR ("Unsupported stream type: %d", (int) stream_type);
      return SL_ANDROID_STREAM_MEDIA;
  }
}
