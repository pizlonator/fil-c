/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 * Copyright (C) 2004,2006 Thomas Vander Stichele <thomas at apestaart dot org>
 * Copyright (C) 2014 Tim-Philipp Müller <tim centricular com>
 *
 * dataprotocol.c: Functions implementing the GStreamer Data Protocol
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

/**
 * SECTION:gstdataprotocol
 * @title: GstDataProtocol
 * @short_description: Serialization of caps, buffers and events.
 * @see_also: #GstCaps, #GstEvent, #GstBuffer
 *
 * This helper library provides serialization of GstBuffer, GstCaps and
 * GstEvent structures.
 *
 * This serialization is useful when GStreamer needs to interface with
 * the outside world to transport data between distinct GStreamer pipelines.
 * The connections with the outside world generally don't have mechanisms
 * to transport properties of these structures.
 *
 * For example, transporting buffers across named pipes or network connections
 * doesn't maintain the buffer size and separation.
 *
 * This data protocol assumes a reliable connection-oriented transport, such as
 * TCP, a pipe, or a file.  The protocol does not serialize the caps for
 * each buffer; instead, it transport the caps only when they change in the
 * stream.  This implies that there will always be a caps packet before any
 * buffer packets.
 *
 * The versioning of the protocol is independent of GStreamer's version.
 * The major number gets incremented, and the minor reset, for incompatible
 * changes.  The minor number gets incremented for compatible changes that
 * allow clients who do not completely understand the newer protocol version
 * to still decode what they do understand.
 *
 * Version 0.2 serializes only a small subset of all events, with a custom
 * payload for each type.  Also, all GDP streams start with the initial caps
 * packet.
 *
 * Version 1.0 serializes all events by taking the string representation of
 * the event as the payload.  In addition, GDP streams can now start with
 * events as well, as required by the new data stream model in GStreamer 0.10.
 *
 * Converting buffers, caps and events to GDP buffers is done using the
 * appropriate functions.
 *
 * For reference, this image shows the byte layout of the GDP header:
 *
 * <inlinegraphic format="PNG" fileref="gdp-header.png"></inlinegraphic>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include "dataprotocol.h"
#include <glib/gprintf.h>       /* g_sprintf */
#include <string.h>             /* strlen */
#include "dp-private.h"

/* debug category */
GST_DEBUG_CATEGORY_STATIC (data_protocol_debug);
#ifndef GST_CAT_DEFAULT
#define GST_CAT_DEFAULT data_protocol_debug
#endif

/* The version of the GDP protocol being used */
typedef enum
{
  GST_DP_VERSION_0_2 = 1,
  GST_DP_VERSION_1_0,
} GstDPVersion;

/* helper macros */

/* write first 6 bytes of header */
#define GST_DP_INIT_HEADER(h, version, flags, type)		\
G_STMT_START {							\
  gint maj = 0, min = 0;					\
  switch (version) {						\
    case GST_DP_VERSION_0_2: maj = 0; min = 2; break;		\
    case GST_DP_VERSION_1_0: maj = 1; min = 0; break;		\
  }								\
  h[0] = (guint8) maj;						\
  h[1] = (guint8) min;						\
  h[2] = (guint8) flags;					\
  h[3] = 0; /* padding byte */					\
  GST_WRITE_UINT16_BE (h + 4, type);				\
} G_STMT_END

#define GST_DP_SET_CRC(h, flags, payload, length);		\
G_STMT_START {							\
  guint16 crc = 0;						\
  if (flags & GST_DP_HEADER_FLAG_CRC_HEADER)			\
    /* we don't crc the last four bytes since they are crc's */ \
    crc = gst_dp_crc (h, 58);					\
  GST_WRITE_UINT16_BE (h + 58, crc);				\
								\
  crc = 0;							\
  if (length && (flags & GST_DP_HEADER_FLAG_CRC_PAYLOAD))	\
    crc = gst_dp_crc (payload, length);				\
  GST_WRITE_UINT16_BE (h + 60, crc);				\
} G_STMT_END

/* calculate a CCITT 16 bit CRC check value for a given byte array */
/*
 * this code snippet is adapted from a web page I found
 * it is identical except for cleanups, and a final XOR with 0xffff
 * as outlined in the uecp spec
 *
 * XMODEM    x^16 + x^12 + x^5 + 1
 */

#define POLY       0x1021
#define CRC_INIT   0xFFFF

static guint16 gst_dp_crc (const guint8 * buffer, guint length);
static guint16 gst_dp_crc_from_memory_maps (const GstMapInfo * maps,
    guint n_maps);

/* payloading functions */

GstBuffer *
gst_dp_payload_buffer (GstBuffer * buffer, GstDPHeaderFlag flags)
{
  GstBuffer *ret_buf;
  GstMapInfo map;
  GstMemory *mem;
  guint8 *h;
  guint16 flags_mask;
  guint16 header_crc = 0, crc = 0;
  gsize buffer_size;

  mem = gst_allocator_alloc (NULL, GST_DP_HEADER_LENGTH, NULL);
  gst_memory_map (mem, &map, GST_MAP_READWRITE);
  h = memset (map.data, 0, map.size);

  /* version, flags, type */
  GST_DP_INIT_HEADER (h, GST_DP_VERSION_1_0, flags, GST_DP_PAYLOAD_BUFFER);

  if ((flags & GST_DP_HEADER_FLAG_CRC_PAYLOAD)) {
    GstMapInfo *maps;
    guint n_maps, i;

    buffer_size = 0;

    n_maps = gst_buffer_n_memory (buffer);
    if (n_maps > 0) {
      maps = g_newa (GstMapInfo, n_maps);

      for (i = 0; i < n_maps; ++i) {
        GstMemory *mem;

        mem = gst_buffer_peek_memory (buffer, i);
        gst_memory_map (mem, &maps[i], GST_MAP_READ);
        buffer_size += maps[i].size;
      }

      crc = gst_dp_crc_from_memory_maps (maps, n_maps);

      for (i = 0; i < n_maps; ++i)
        gst_memory_unmap (maps[i].memory, &maps[i]);
    }
  } else {
    buffer_size = gst_buffer_get_size (buffer);
  }

  /* buffer properties */
  GST_WRITE_UINT32_BE (h + 6, buffer_size);
  GST_WRITE_UINT64_BE (h + 10, GST_BUFFER_TIMESTAMP (buffer));
  GST_WRITE_UINT64_BE (h + 18, GST_BUFFER_DURATION (buffer));
  GST_WRITE_UINT64_BE (h + 26, GST_BUFFER_OFFSET (buffer));
  GST_WRITE_UINT64_BE (h + 34, GST_BUFFER_OFFSET_END (buffer));

  /* data flags; eats two bytes from the ABI area */
  /* we copy everything but the read-only flags */
  flags_mask = GST_BUFFER_FLAG_LIVE | GST_BUFFER_FLAG_DISCONT |
      GST_BUFFER_FLAG_HEADER | GST_BUFFER_FLAG_GAP | GST_BUFFER_FLAG_DELTA_UNIT;

  GST_WRITE_UINT16_BE (h + 42, GST_BUFFER_FLAGS (buffer) & flags_mask);

  /* from gstreamer 1.x, buffers also have the DTS */
  GST_WRITE_UINT64_BE (h + 44, GST_BUFFER_DTS (buffer));

  /* header CRC */
  if ((flags & GST_DP_HEADER_FLAG_CRC_HEADER))
    /* we don't crc the last four bytes since they are crc's */
    header_crc = gst_dp_crc (h, 58);
  else
    header_crc = 0;

  GST_WRITE_UINT16_BE (h + 58, header_crc);

  /* payload CRC */
  GST_WRITE_UINT16_BE (h + 60, crc);

  GST_MEMDUMP ("payload header for buffer", h, GST_DP_HEADER_LENGTH);
  gst_memory_unmap (mem, &map);

  ret_buf = gst_buffer_new ();

  /* header */
  gst_buffer_append_memory (ret_buf, mem);

  /* buffer data */
  return gst_buffer_append (ret_buf, gst_buffer_ref (buffer));
}

GstBuffer *
gst_dp_payload_caps (const GstCaps * caps, GstDPHeaderFlag flags)
{
  GstBuffer *buf;
  GstMapInfo map;
  GstMemory *mem;
  guint8 *h;
  guchar *string;
  guint payload_length;

  g_assert (GST_IS_CAPS (caps));

  buf = gst_buffer_new ();

  mem = gst_allocator_alloc (NULL, GST_DP_HEADER_LENGTH, NULL);
  gst_memory_map (mem, &map, GST_MAP_READWRITE);
  h = memset (map.data, 0, map.size);

  string = (guchar *) gst_caps_to_string (caps);
  payload_length = strlen ((gchar *) string) + 1;       /* include trailing 0 */

  /* version, flags, type */
  GST_DP_INIT_HEADER (h, GST_DP_VERSION_1_0, flags, GST_DP_PAYLOAD_CAPS);

  /* buffer properties */
  GST_WRITE_UINT32_BE (h + 6, payload_length);
  GST_WRITE_UINT64_BE (h + 10, (guint64) 0);
  GST_WRITE_UINT64_BE (h + 18, (guint64) 0);
  GST_WRITE_UINT64_BE (h + 26, (guint64) 0);
  GST_WRITE_UINT64_BE (h + 34, (guint64) 0);

  GST_DP_SET_CRC (h, flags, string, payload_length);

  GST_MEMDUMP ("payload header for caps", h, GST_DP_HEADER_LENGTH);
  gst_memory_unmap (mem, &map);

  /* header */
  gst_buffer_append_memory (buf, mem);

  /* caps string */
  gst_buffer_append_memory (buf,
      gst_memory_new_wrapped (0, string, payload_length, 0, payload_length,
          string, g_free));

  return buf;
}

GstBuffer *
gst_dp_payload_event (const GstEvent * event, GstDPHeaderFlag flags)
{
  GstBuffer *buf;
  GstMapInfo map;
  GstMemory *mem;
  guint8 *h;
  guint32 pl_length;            /* length of payload */
  guchar *string = NULL;
  const GstStructure *structure;

  g_assert (GST_IS_EVENT (event));

  buf = gst_buffer_new ();

  mem = gst_allocator_alloc (NULL, GST_DP_HEADER_LENGTH, NULL);
  gst_memory_map (mem, &map, GST_MAP_READWRITE);
  h = memset (map.data, 0, map.size);

  structure = gst_event_get_structure ((GstEvent *) event);
  if (structure) {
    string = (guchar *) gst_structure_to_string (structure);
    GST_LOG ("event %p has structure, string %s", event, string);
    pl_length = strlen ((gchar *) string) + 1;  /* include trailing 0 */
  } else {
    GST_LOG ("event %p has no structure", event);
    pl_length = 0;
  }

  /* version, flags, type */
  GST_DP_INIT_HEADER (h, GST_DP_VERSION_1_0, flags,
      GST_DP_PAYLOAD_EVENT_NONE + GST_EVENT_TYPE (event));

  /* length */
  GST_WRITE_UINT32_BE (h + 6, pl_length);
  /* timestamp */
  /* NOTE: timestamp field will be removed from GstEvent in 2.0 API */
  GST_WRITE_UINT64_BE (h + 10, GST_CLOCK_TIME_NONE);

  GST_DP_SET_CRC (h, flags, string, pl_length);

  GST_MEMDUMP ("payload header for event", h, GST_DP_HEADER_LENGTH);
  gst_memory_unmap (mem, &map);

  /* header */
  gst_buffer_append_memory (buf, mem);

  /* event string */
  if (pl_length > 0) {
    gst_buffer_append_memory (buf,
        gst_memory_new_wrapped (0, string, pl_length, 0, pl_length,
            string, g_free));
  }

  return buf;
}

/*** PUBLIC FUNCTIONS ***/

static const guint16 gst_dp_crc_table[256] = {
  0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
  0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef,
  0x1231, 0x0210, 0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6,
  0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de,
  0x2462, 0x3443, 0x0420, 0x1401, 0x64e6, 0x74c7, 0x44a4, 0x5485,
  0xa56a, 0xb54b, 0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d,
  0x3653, 0x2672, 0x1611, 0x0630, 0x76d7, 0x66f6, 0x5695, 0x46b4,
  0xb75b, 0xa77a, 0x9719, 0x8738, 0xf7df, 0xe7fe, 0xd79d, 0xc7bc,
  0x48c4, 0x58e5, 0x6886, 0x78a7, 0x0840, 0x1861, 0x2802, 0x3823,
  0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969, 0xa90a, 0xb92b,
  0x5af5, 0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0x0a50, 0x3a33, 0x2a12,
  0xdbfd, 0xcbdc, 0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a,
  0x6ca6, 0x7c87, 0x4ce4, 0x5cc5, 0x2c22, 0x3c03, 0x0c60, 0x1c41,
  0xedae, 0xfd8f, 0xcdec, 0xddcd, 0xad2a, 0xbd0b, 0x8d68, 0x9d49,
  0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0x0e70,
  0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a, 0x9f59, 0x8f78,
  0x9188, 0x81a9, 0xb1ca, 0xa1eb, 0xd10c, 0xc12d, 0xf14e, 0xe16f,
  0x1080, 0x00a1, 0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067,
  0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e,
  0x02b1, 0x1290, 0x22f3, 0x32d2, 0x4235, 0x5214, 0x6277, 0x7256,
  0xb5ea, 0xa5cb, 0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d,
  0x34e2, 0x24c3, 0x14a0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
  0xa7db, 0xb7fa, 0x8799, 0x97b8, 0xe75f, 0xf77e, 0xc71d, 0xd73c,
  0x26d3, 0x36f2, 0x0691, 0x16b0, 0x6657, 0x7676, 0x4615, 0x5634,
  0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9, 0xb98a, 0xa9ab,
  0x5844, 0x4865, 0x7806, 0x6827, 0x18c0, 0x08e1, 0x3882, 0x28a3,
  0xcb7d, 0xdb5c, 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a,
  0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1, 0x1ad0, 0x2ab3, 0x3a92,
  0xfd2e, 0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b, 0x9de8, 0x8dc9,
  0x7c26, 0x6c07, 0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0x0cc1,
  0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8,
  0x6e17, 0x7e36, 0x4e55, 0x5e74, 0x2e93, 0x3eb2, 0x0ed1, 0x1ef0
};

/**
 * gst_dp_crc:
 * @buffer: array of bytes
 * @length: the length of @buffer
 *
 * Calculate a CRC for the given buffer over the given number of bytes.
 * This is only provided for verification purposes; typical GDP users
 * will not need this function.
 *
 * Returns: a two-byte CRC checksum.
 */
static guint16
gst_dp_crc (const guint8 * buffer, guint length)
{
  guint16 crc_register = CRC_INIT;

  if (length == 0)
    return 0;

  g_assert (buffer != NULL);

  /* calc CRC */
  for (; length--;) {
    crc_register = (guint16) ((crc_register << 8) ^
        gst_dp_crc_table[((crc_register >> 8) & 0x00ff) ^ *buffer++]);
  }
  return (0xffff ^ crc_register);
}

static guint16
gst_dp_crc_from_memory_maps (const GstMapInfo * maps, guint n_maps)
{
  guint16 crc_register = CRC_INIT;
  gsize total_length = 0;

  if (n_maps == 0)
    return 0;

  g_assert (maps != NULL);

  /* calc CRC */
  while (n_maps > 0) {
    guint8 *buffer = maps->data;
    gsize length = maps->size;

    total_length += length;

    while (length-- > 0) {
      crc_register = (guint16) ((crc_register << 8) ^
          gst_dp_crc_table[((crc_register >> 8) & 0x00ff) ^ *buffer++]);
    }
    --n_maps;
    ++maps;
  }

  if (G_UNLIKELY (total_length == 0))
    return 0;

  return (0xffff ^ crc_register);
}

/**
 * gst_dp_init:
 *
 * Initialize GStreamer Data Protocol library.
 *
 * Should be called before using these functions from source linking
 * to this source file.
 */
void
gst_dp_init (void)
{
  GST_DEBUG_CATEGORY_INIT (data_protocol_debug, "gdp", 0,
      "GStreamer Data Protocol");
}

/**
 * gst_dp_header_payload_length:
 * @header: the byte header of the packet array
 *
 * Get the length of the payload described by @header.
 *
 * Returns: the length of the payload this header describes.
 */
guint32
gst_dp_header_payload_length (const guint8 * header)
{
  g_return_val_if_fail (header != NULL, 0);

  return GST_DP_HEADER_PAYLOAD_LENGTH (header);
}

/**
 * gst_dp_header_payload_type:
 * @header: the byte header of the packet array
 *
 * Get the type of the payload described by @header.
 *
 * Returns: the #GstDPPayloadType the payload this header describes.
 */
GstDPPayloadType
gst_dp_header_payload_type (const guint8 * header)
{
  g_return_val_if_fail (header != NULL, GST_DP_PAYLOAD_NONE);

  return GST_DP_HEADER_PAYLOAD_TYPE (header);
}

/*** DEPACKETIZING FUNCTIONS ***/

/**
 * gst_dp_buffer_from_header:
 * @header_length: the length of the packet header
 * @header: the byte array of the packet header
 * @allocator: the allocator used to allocate the new #GstBuffer
 * @allocation_params: the allocations parameters used to allocate the new #GstBuffer
 *
 * Creates a newly allocated #GstBuffer from the given header.
 * The buffer data needs to be copied into it before validating.
 *
 * Use this function if you want to pre-allocate a buffer based on the
 * packet header to read the packet payload in to.
 *
 * This function does not check the header passed to it, use
 * gst_dp_validate_header() first if the header data is unchecked.
 *
 * Returns: A #GstBuffer if the buffer was successfully created, or NULL.
 */
GstBuffer *
gst_dp_buffer_from_header (guint header_length, const guint8 * header,
    GstAllocator * allocator, GstAllocationParams * allocation_params)
{
  GstBuffer *buffer;

  g_return_val_if_fail (header != NULL, NULL);
  g_return_val_if_fail (header_length >= GST_DP_HEADER_LENGTH, NULL);
  g_return_val_if_fail (GST_DP_HEADER_PAYLOAD_TYPE (header) ==
      GST_DP_PAYLOAD_BUFFER, NULL);

  buffer =
      gst_buffer_new_allocate (allocator,
      (guint) GST_DP_HEADER_PAYLOAD_LENGTH (header), allocation_params);

  GST_BUFFER_TIMESTAMP (buffer) = GST_DP_HEADER_TIMESTAMP (header);
  GST_BUFFER_DTS (buffer) = GST_DP_HEADER_DTS (header);
  GST_BUFFER_DURATION (buffer) = GST_DP_HEADER_DURATION (header);
  GST_BUFFER_OFFSET (buffer) = GST_DP_HEADER_OFFSET (header);
  GST_BUFFER_OFFSET_END (buffer) = GST_DP_HEADER_OFFSET_END (header);
  GST_BUFFER_FLAGS (buffer) = GST_DP_HEADER_BUFFER_FLAGS (header);

  return buffer;
}

/**
 * gst_dp_caps_from_packet:
 * @header_length: the length of the packet header
 * @header: the byte array of the packet header
 * @payload: the byte array of the packet payload
 *
 * Creates a newly allocated #GstCaps from the given packet.
 *
 * This function does not check the arguments passed to it, use
 * gst_dp_validate_packet() first if the header and payload data are
 * unchecked.
 *
 * Returns: A #GstCaps containing the caps represented in the packet,
 *          or NULL if the packet could not be converted.
 */
GstCaps *
gst_dp_caps_from_packet (guint header_length, const guint8 * header,
    const guint8 * payload)
{
  GstCaps *caps;
  gchar *string;

  g_return_val_if_fail (header, NULL);
  g_return_val_if_fail (header_length >= GST_DP_HEADER_LENGTH, NULL);
  g_return_val_if_fail (GST_DP_HEADER_PAYLOAD_TYPE (header) ==
      GST_DP_PAYLOAD_CAPS, NULL);
  g_return_val_if_fail (payload, NULL);

  /* 0 sized payload length will work create NULL string */
  string = g_strndup ((gchar *) payload, GST_DP_HEADER_PAYLOAD_LENGTH (header));
  caps = gst_caps_from_string (string);
  g_free (string);

  return caps;
}

static GstEvent *
gst_dp_event_from_packet_0_2 (guint header_length, const guint8 * header,
    const guint8 * payload)
{
  GstEvent *event = NULL;
  GstEventType type;

  type = GST_DP_HEADER_PAYLOAD_TYPE (header) - GST_DP_PAYLOAD_EVENT_NONE;
  switch (type) {
    case GST_EVENT_UNKNOWN:
      GST_WARNING ("Unknown event, ignoring");
      return NULL;
    case GST_EVENT_EOS:
    case GST_EVENT_FLUSH_START:
    case GST_EVENT_FLUSH_STOP:
    case GST_EVENT_SEGMENT:
      event = gst_event_new_custom (type, NULL);
      break;
    case GST_EVENT_SEEK:
    {
      gdouble rate;
      GstFormat format;
      GstSeekFlags flags;
      GstSeekType start_type, stop_type;
      gint64 start, stop;

      g_return_val_if_fail (payload != NULL, NULL);

      /* FIXME, read rate */
      rate = 1.0;
      format = (GstFormat) GST_READ_UINT32_BE (payload);
      flags = (GstSeekFlags) GST_READ_UINT32_BE (payload + 4);
      start_type = (GstSeekType) GST_READ_UINT32_BE (payload + 8);
      start = (gint64) GST_READ_UINT64_BE (payload + 12);
      stop_type = (GstSeekType) GST_READ_UINT32_BE (payload + 20);
      stop = (gint64) GST_READ_UINT64_BE (payload + 24);

      event = gst_event_new_seek (rate, format, flags, start_type, start,
          stop_type, stop);
      GST_EVENT_TIMESTAMP (event) = GST_DP_HEADER_TIMESTAMP (header);
      break;
    }
    case GST_EVENT_QOS:
    case GST_EVENT_NAVIGATION:
    case GST_EVENT_TAG:
      GST_WARNING ("Unhandled event type %d, ignoring", type);
      return NULL;
    default:
      GST_WARNING ("Unknown event type %d, ignoring", type);
      return NULL;
  }

  return event;
}

static GstEvent *
gst_dp_event_from_packet_1_0 (guint header_length, const guint8 * header,
    const guint8 * payload)
{
  GstEvent *event = NULL;
  GstEventType type;
  gchar *string = NULL;
  GstStructure *s = NULL;

  type = GST_DP_HEADER_PAYLOAD_TYPE (header) - GST_DP_PAYLOAD_EVENT_NONE;
  if (payload) {
    string =
        g_strndup ((gchar *) payload, GST_DP_HEADER_PAYLOAD_LENGTH (header));
    s = gst_structure_from_string (string, NULL);
    if (s == NULL) {
      GST_WARNING ("Could not parse payload string: %s", string);
      g_free (string);
      return NULL;
    }

    g_free (string);
  }
  GST_LOG ("Creating event of type 0x%x with structure '%" GST_PTR_FORMAT "'",
      type, s);
  event = gst_event_new_custom (type, s);
  return event;
}


/**
 * gst_dp_event_from_packet:
 * @header_length: the length of the packet header
 * @header: the byte array of the packet header
 * @payload: the byte array of the packet payload
 *
 * Creates a newly allocated #GstEvent from the given packet.
 *
 * This function does not check the arguments passed to it, use
 * gst_dp_validate_packet() first if the header and payload data are
 * unchecked.
 *
 * Returns: A #GstEvent if the event was successfully created,
 *          or NULL if an event could not be read from the payload.
 */
GstEvent *
gst_dp_event_from_packet (guint header_length, const guint8 * header,
    const guint8 * payload)
{
  guint8 major, minor;

  g_return_val_if_fail (header, NULL);
  g_return_val_if_fail (header_length >= GST_DP_HEADER_LENGTH, NULL);

  major = GST_DP_HEADER_MAJOR_VERSION (header);
  minor = GST_DP_HEADER_MINOR_VERSION (header);

  if (major == 0 && minor == 2)
    return gst_dp_event_from_packet_0_2 (header_length, header, payload);
  else if (major == 1 && minor == 0)
    return gst_dp_event_from_packet_1_0 (header_length, header, payload);
  else {
    GST_ERROR ("Unknown GDP version %d.%d", major, minor);
    return NULL;
  }
}

/**
 * gst_dp_validate_header:
 * @header_length: the length of the packet header
 * @header: the byte array of the packet header
 *
 * Validates the given packet header by checking the CRC checksum.
 *
 * Returns: %TRUE if the CRC matches, or no CRC checksum is present.
 */
gboolean
gst_dp_validate_header (guint header_length, const guint8 * header)
{
  guint16 crc_read, crc_calculated;

  g_return_val_if_fail (header != NULL, FALSE);
  g_return_val_if_fail (header_length >= GST_DP_HEADER_LENGTH, FALSE);

  if (!(GST_DP_HEADER_FLAGS (header) & GST_DP_HEADER_FLAG_CRC_HEADER))
    return TRUE;

  crc_read = GST_DP_HEADER_CRC_HEADER (header);

  /* don't include the last two crc fields for the crc check */
  crc_calculated = gst_dp_crc (header, header_length - 4);
  if (crc_read != crc_calculated)
    goto crc_error;

  GST_LOG ("header crc validation: %02x", crc_read);
  return TRUE;

  /* ERRORS */
crc_error:
  {
    GST_WARNING ("header crc mismatch: read %02x, calculated %02x", crc_read,
        crc_calculated);
    return FALSE;
  }
}

/**
 * gst_dp_validate_payload:
 * @header_length: the length of the packet header
 * @header: the byte array of the packet header
 * @payload: the byte array of the packet payload
 *
 * Validates the given packet payload using the given packet header
 * by checking the CRC checksum.
 *
 * Returns: %TRUE if the CRC matches, or no CRC checksum is present.
 */
gboolean
gst_dp_validate_payload (guint header_length, const guint8 * header,
    const guint8 * payload)
{
  guint16 crc_read, crc_calculated;

  g_return_val_if_fail (header != NULL, FALSE);
  g_return_val_if_fail (header_length >= GST_DP_HEADER_LENGTH, FALSE);

  if (!(GST_DP_HEADER_FLAGS (header) & GST_DP_HEADER_FLAG_CRC_PAYLOAD))
    return TRUE;

  crc_read = GST_DP_HEADER_CRC_PAYLOAD (header);
  crc_calculated = gst_dp_crc (payload, GST_DP_HEADER_PAYLOAD_LENGTH (header));
  if (crc_read != crc_calculated)
    goto crc_error;

  GST_LOG ("payload crc validation: %02x", crc_read);
  return TRUE;

  /* ERRORS */
crc_error:
  {
    GST_WARNING ("payload crc mismatch: read %02x, calculated %02x", crc_read,
        crc_calculated);
    return FALSE;
  }
}

/**
 * gst_dp_validate_packet:
 * @header_length: the length of the packet header
 * @header: the byte array of the packet header
 * @payload: the byte array of the packet payload
 *
 * Validates the given packet by checking version information and checksums.
 *
 * Returns: %TRUE if the packet validates.
 */
gboolean
gst_dp_validate_packet (guint header_length, const guint8 * header,
    const guint8 * payload)
{
  if (!gst_dp_validate_header (header_length, header))
    return FALSE;
  if (!gst_dp_validate_payload (header_length, header, payload))
    return FALSE;

  return TRUE;
}
