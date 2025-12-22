/* GStreamer
 * Copyright (C) 2021 Seungha Yang <seungha@centricular.com>
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

#ifndef __GST_ASIO_DEVICE_ENUM_H__
#define __GST_ASIO_DEVICE_ENUM_H__

#include <gst/gst.h>
#include <gst/audio/audio.h>
#include <windows.h>
#include <asiosys.h>
#include <asio.h>

G_BEGIN_DECLS

#define GST_ASIO_STATIC_CAPS "audio/x-raw, " \
        "format = (string) " GST_AUDIO_FORMATS_ALL ", " \
        "layout = (string) interleaved, " \
        "rate = " GST_AUDIO_RATE_RANGE ", " \
        "channels = " GST_AUDIO_CHANNELS_RANGE

typedef struct
{
  CLSID clsid;
  gboolean sta_model;
  gchar *driver_name;
  gchar *driver_desc;
} GstAsioDeviceInfo;

guint               gst_asio_enum (GList ** infos);

GstAsioDeviceInfo * gst_asio_device_info_copy (const GstAsioDeviceInfo * info);

void                gst_asio_device_info_free (GstAsioDeviceInfo * info);

GstAudioFormat      gst_asio_sample_type_to_gst (ASIOSampleType type);

G_END_DECLS

#endif /* __GST_ASIO_DEVICE_ENUM_H__ */