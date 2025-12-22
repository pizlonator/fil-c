/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 * Copyright (C) <2004> Thomas Vander Stichele <thomas at apestaart dot org>
 *
 * dp-private.h: private defines/macros for dataprotocol implementation
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

#ifndef __GST_DP_PRIVATE_H__
#define __GST_DP_PRIVATE_H__

#include <gst/gstbuffer.h>
#include <gst/gstevent.h>
#include <gst/gstcaps.h>

G_BEGIN_DECLS

/* FIXME: please make the dataprotocol format typefindable in new versions */

/* accessor defines */
#define GST_DP_HEADER_MAJOR_VERSION(x)	((x)[0])
#define GST_DP_HEADER_MINOR_VERSION(x)  ((x)[1])
#define GST_DP_HEADER_FLAGS(x)          ((x)[2])
/* free byte here to align */
#define GST_DP_HEADER_PAYLOAD_TYPE(x)   GST_READ_UINT16_BE (x + 4)
#define GST_DP_HEADER_PAYLOAD_LENGTH(x) GST_READ_UINT32_BE (x + 6)
#define GST_DP_HEADER_TIMESTAMP(x)      GST_READ_UINT64_BE (x + 10)
#define GST_DP_HEADER_DURATION(x)       GST_READ_UINT64_BE (x + 18)
#define GST_DP_HEADER_OFFSET(x)         GST_READ_UINT64_BE (x + 26)
#define GST_DP_HEADER_OFFSET_END(x)     GST_READ_UINT64_BE (x + 34)
#define GST_DP_HEADER_BUFFER_FLAGS(x)   GST_READ_UINT16_BE (x + 42)
#define GST_DP_HEADER_DTS(x)            GST_READ_UINT64_BE (x + 44)
#define GST_DP_HEADER_CRC_HEADER(x)     GST_READ_UINT16_BE (x + 58)
#define GST_DP_HEADER_CRC_PAYLOAD(x)    GST_READ_UINT16_BE (x + 60)

void gst_dp_dump_byte_array (guint8 *array, guint length);

G_END_DECLS

#endif /* __GST_DP_PRIVATE_H__ */
