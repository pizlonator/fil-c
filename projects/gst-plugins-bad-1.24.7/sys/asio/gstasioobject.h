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
 * License aglong with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __GST_ASIO_OBJECT_H__
#define __GST_ASIO_OBJECT_H__

#include <gst/gst.h>
#include <windows.h>
#include "gstasioutils.h"

G_BEGIN_DECLS

#define GST_TYPE_ASIO_OBJECT (gst_asio_object_get_type())
G_DECLARE_FINAL_TYPE (GstAsioObject, gst_asio_object,
    GST, ASIO_OBJECT, GstObject);

typedef struct {
  gboolean (*buffer_switch) (GstAsioObject * obj,
                             glong index,
                             ASIOBufferInfo * infos,
                             guint num_infos,
                             ASIOChannelInfo * input_channel_infos,
                             ASIOChannelInfo * output_channel_infos,
                             ASIOSampleRate sample_rate,
                             glong buffer_size,
                             ASIOTime * time_info,
                             gpointer user_data);

  gpointer user_data;
} GstAsioObjectCallbacks;

typedef enum
{
  GST_ASIO_DEVICE_CLASS_CAPTURE,
  GST_ASIO_DEVICE_CLASS_RENDER,
  GST_ASIO_DEVICE_CLASS_LOOPBACK_CAPTURE,
} GstAsioDeviceClassType;

GstAsioObject * gst_asio_object_new (const GstAsioDeviceInfo * info,
                                     gboolean occupy_all_channels);

GstCaps       * gst_asio_object_get_caps         (GstAsioObject * obj,
                                                  GstAsioDeviceClassType type,
                                                  guint num_min_channels,
                                                  guint num_max_channels);

gboolean        gst_asio_object_create_buffers (GstAsioObject * obj,
                                                GstAsioDeviceClassType type,
                                                guint * channel_indices,
                                                guint num_channels,
                                                guint * buffer_size);

gboolean        gst_asio_object_start           (GstAsioObject * obj);

gboolean        gst_asio_object_install_callback (GstAsioObject * obj,
                                                  GstAsioDeviceClassType type,
                                                  GstAsioObjectCallbacks * callbacks,
                                                  guint64 * callback_id);

void            gst_asio_object_uninstall_callback (GstAsioObject * obj,
                                                    guint64 callback_id);

gboolean        gst_asio_object_get_max_num_channels (GstAsioObject * obj,
                                                      glong * num_input_ch,
                                                      glong * num_output_ch);

gboolean        gst_asio_object_get_buffer_size (GstAsioObject * obj,
                                                 glong * min_size,
                                                 glong * max_size,
                                                 glong * preferred_size,
                                                 glong * granularity);

gboolean        gst_asio_object_get_latencies (GstAsioObject * obj,
                                               glong * input_latency,
                                               glong * output_latency);

gboolean        gst_asio_object_can_sample_rate (GstAsioObject * obj,
                                                 ASIOSampleRate sample_rate);

gboolean        gst_asio_object_get_sample_rate (GstAsioObject * obj,
                                                 ASIOSampleRate * sample_rate);

gboolean        gst_asio_object_set_sample_rate (GstAsioObject * obj,
                                                 ASIOSampleRate sample_rate);

G_END_DECLS

#endif /* __GST_ASIO_OBJECT_H__ */