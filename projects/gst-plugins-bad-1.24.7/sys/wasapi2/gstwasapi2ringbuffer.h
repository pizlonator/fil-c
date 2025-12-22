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

#ifndef __GST_WASAPI2_RING_BUFFER_H__
#define __GST_WASAPI2_RING_BUFFER_H__

#include <gst/gst.h>
#include <gst/audio/audio.h>
#include "gstwasapi2client.h"

G_BEGIN_DECLS

#define GST_TYPE_WASAPI2_RING_BUFFER (gst_wasapi2_ring_buffer_get_type())
G_DECLARE_FINAL_TYPE (GstWasapi2RingBuffer, gst_wasapi2_ring_buffer,
    GST, WASAPI2_RING_BUFFER, GstAudioRingBuffer);

GstAudioRingBuffer *   gst_wasapi2_ring_buffer_new (GstWasapi2ClientDeviceClass device_class,
                                                    gboolean low_latency,
                                                    const gchar *device_id,
                                                    gpointer dispatcher,
                                                    const gchar * name,
                                                    guint loopback_target_pid);

GstCaps *              gst_wasapi2_ring_buffer_get_caps (GstWasapi2RingBuffer * buf);

HRESULT                gst_wasapi2_ring_buffer_set_mute  (GstWasapi2RingBuffer * buf,
                                                          gboolean mute);

HRESULT                gst_wasapi2_ring_buffer_get_mute  (GstWasapi2RingBuffer * buf,
                                                          gboolean * mute);

HRESULT                gst_wasapi2_ring_buffer_set_volume (GstWasapi2RingBuffer * buf,
                                                           gfloat volume);

HRESULT                gst_wasapi2_ring_buffer_get_volume (GstWasapi2RingBuffer * buf,
                                                           gfloat * volume);

void                   gst_wasapi2_ring_buffer_set_device_mute_monitoring (GstWasapi2RingBuffer * buf,
                                                                           gboolean value);

G_END_DECLS

#endif /* __GST_WASAPI2_RING_BUFFER_H__ */
