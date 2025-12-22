/* GStreamer
 * Copyright (C) 2015 Centricular Ltd.
 * Author: Arun Raghavan <arun@centricular.com>
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

#ifndef __OPENSLESCOMMON_H__
#define __OPENSLESCOMMON_H__

#include <gst/gst.h>

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
/* This is needed explicitly for API level < 14 */
#include <SLES/OpenSLES_AndroidConfiguration.h>

G_BEGIN_DECLS

typedef enum {
  GST_OPENSLES_RECORDING_PRESET_NONE,
  GST_OPENSLES_RECORDING_PRESET_GENERIC,
  GST_OPENSLES_RECORDING_PRESET_CAMCORDER,
  GST_OPENSLES_RECORDING_PRESET_VOICE_RECOGNITION,
  GST_OPENSLES_RECORDING_PRESET_VOICE_COMMUNICATION,
} GstOpenSLESRecordingPreset;

#define GST_TYPE_OPENSLES_RECORDING_PRESET \
  (gst_opensles_recording_preset_get_type())

GType gst_opensles_recording_preset_get_type (void);

SLint32 gst_to_opensles_recording_preset (GstOpenSLESRecordingPreset preset);

typedef enum {
  GST_OPENSLES_STREAM_TYPE_VOICE,
  GST_OPENSLES_STREAM_TYPE_SYSTEM,
  GST_OPENSLES_STREAM_TYPE_RING,
  GST_OPENSLES_STREAM_TYPE_MEDIA,
  GST_OPENSLES_STREAM_TYPE_ALARM,
  GST_OPENSLES_STREAM_TYPE_NOTIFICATION,
  GST_OPENSLES_STREAM_TYPE_NONE = -1, /* If we don't want to set a type */
} GstOpenSLESStreamType;

#define GST_TYPE_OPENSLES_STREAM_TYPE \
  (gst_opensles_stream_type_get_type())

GType gst_opensles_stream_type_get_type (void);

SLint32 gst_to_opensles_stream_type (GstOpenSLESStreamType stream_type);

G_END_DECLS

#endif /* __OPENSLESCOMMON_H__ */
