/* GStreamer
 * Copyright (C) 1999 Erik Walthinsen <omega@cse.ogi.edu>
 * Copyright (C) 2004,2006 Thomas Vander Stichele <thomas at apestaart dot org>
 *
 * dataprotocol.h: Functions implementing the GStreamer Data Protocol
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

#ifndef __GST_DATA_PROTOCOL_H__
#define __GST_DATA_PROTOCOL_H__

#include <gst/gstbuffer.h>
#include <gst/gstevent.h>
#include <gst/gstcaps.h>

G_BEGIN_DECLS

/**
 * GST_DP_HEADER_LENGTH:
 *
 * The header size in bytes.
 */
#define GST_DP_HEADER_LENGTH 62

/**
 * GstDPHeaderFlag:
 * @GST_DP_HEADER_FLAG_NONE: No flag present.
 * @GST_DP_HEADER_FLAG_CRC_HEADER: a header CRC field is present.
 * @GST_DP_HEADER_FLAG_CRC_PAYLOAD: a payload CRC field is present.
 * @GST_DP_HEADER_FLAG_CRC: a CRC for header and payload is present.
 *
 * header flags for the dataprotocol.
 */
typedef enum {
  GST_DP_HEADER_FLAG_NONE        = 0,
  GST_DP_HEADER_FLAG_CRC_HEADER  = (1 << 0),
  GST_DP_HEADER_FLAG_CRC_PAYLOAD = (1 << 1),
  GST_DP_HEADER_FLAG_CRC         = (1 << 1) | (1 << 0),
} GstDPHeaderFlag;

/**
 * GstDPPayloadType:
 * @GST_DP_PAYLOAD_NONE: Invalid payload type.
 * @GST_DP_PAYLOAD_BUFFER: #GstBuffer payload packet.
 * @GST_DP_PAYLOAD_CAPS: #GstCaps payload packet.
 * @GST_DP_PAYLOAD_EVENT_NONE: First value of #GstEvent payload packets.
 *
 * The GDP payload types. a #GstEvent payload type is encoded with the
 * event type number starting from @GST_DP_PAYLOAD_EVENT_NONE.
 */
typedef enum {
  GST_DP_PAYLOAD_NONE            = 0,
  GST_DP_PAYLOAD_BUFFER,
  GST_DP_PAYLOAD_CAPS,
  GST_DP_PAYLOAD_EVENT_NONE      = 64,
} GstDPPayloadType;

void            gst_dp_init                     (void);

/* payload information from header */
guint32         gst_dp_header_payload_length    (const guint8 * header);
GstDPPayloadType
                gst_dp_header_payload_type      (const guint8 * header);

/* converting to GstBuffer/GstEvent/GstCaps */
GstBuffer *     gst_dp_buffer_from_header       (guint header_length,
                                                const guint8 * header,
                                                GstAllocator * allocator,
                                                GstAllocationParams * allocation_params);
GstCaps *       gst_dp_caps_from_packet         (guint header_length,
                                                const guint8 * header,
                                                const guint8 * payload);
GstEvent *      gst_dp_event_from_packet        (guint header_length,
                                                const guint8 * header,
                                                const guint8 * payload);

/* payloading GstBuffer/GstEvent/GstCaps */
GstBuffer *     gst_dp_payload_buffer           (GstBuffer      * buffer,
                                                 GstDPHeaderFlag  flags);

GstBuffer *     gst_dp_payload_caps             (const GstCaps  * caps,
                                                 GstDPHeaderFlag  flags);

GstBuffer *     gst_dp_payload_event            (const GstEvent * event,
                                                 GstDPHeaderFlag  flags);

/* validation */
gboolean        gst_dp_validate_header          (guint header_length,
                                                const guint8 * header);
gboolean        gst_dp_validate_payload         (guint header_length,
                                                const guint8 * header,
                                                const guint8 * payload);
gboolean        gst_dp_validate_packet          (guint header_length,
                                                const guint8 * header,
                                                const guint8 * payload);

G_END_DECLS

#endif /* __GST_DATA_PROTOCOL_H__ */
