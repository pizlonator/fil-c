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

#ifndef __GST_ASIO_RING_BUFFER_H__
#define __GST_ASIO_RING_BUFFER_H__

#include <gst/gst.h>
#include <gst/audio/audio.h>
#include "gstasioutils.h"
#include "gstasioobject.h"

G_BEGIN_DECLS

#define GST_TYPE_ASIO_RING_BUFFER (gst_asio_ring_buffer_get_type())
G_DECLARE_FINAL_TYPE (GstAsioRingBuffer, gst_asio_ring_buffer,
    GST, ASIO_RING_BUFFER, GstAudioRingBuffer);

GstAsioRingBuffer * gst_asio_ring_buffer_new (GstAsioObject * object,
                                              GstAsioDeviceClassType type,
                                              const gchar * name);

gboolean            gst_asio_ring_buffer_configure (GstAsioRingBuffer * buf,
                                                    guint * channel_indices,
                                                    guint num_channles,
                                                    guint preferred_buffer_size);

GstCaps *           gst_asio_ring_buffer_get_caps (GstAsioRingBuffer * buf);

G_END_DECLS

#endif /* __GST_ASIO_RING_BUFFER_H__ */