/* GStreamer
 * Copyright (C) <2005> Philippe Khalaf <burger@speedy.org>
 * Copyright (C) <2006> Wim Taymans <wim@fluendo.com>
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
 * SECTION:gstrtpbuffer
 * @title: GstRTPBuffer
 * @short_description: Helper methods for dealing with RTP buffers
 * @see_also: #GstRTPBasePayload, #GstRTPBaseDepayload, gstrtcpbuffer
 *
 * The GstRTPBuffer helper functions makes it easy to parse and create regular
 * #GstBuffer objects that contain RTP payloads. These buffers are typically of
 * 'application/x-rtp' #GstCaps.
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstrtpbuffer.h"

#include <stdlib.h>
#include <string.h>

#define GST_RTP_HEADER_LEN 12

/* Note: we use bitfields here to make sure the compiler doesn't add padding
 * between fields on certain architectures; can't assume aligned access either
 */
typedef struct _GstRTPHeader
{
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
  unsigned int csrc_count:4;    /* CSRC count */
  unsigned int extension:1;     /* header extension flag */
  unsigned int padding:1;       /* padding flag */
  unsigned int version:2;       /* protocol version */
  unsigned int payload_type:7;  /* payload type */
  unsigned int marker:1;        /* marker bit */
#elif G_BYTE_ORDER == G_BIG_ENDIAN
  unsigned int version:2;       /* protocol version */
  unsigned int padding:1;       /* padding flag */
  unsigned int extension:1;     /* header extension flag */
  unsigned int csrc_count:4;    /* CSRC count */
  unsigned int marker:1;        /* marker bit */
  unsigned int payload_type:7;  /* payload type */
#else
#error "G_BYTE_ORDER should be big or little endian."
#endif
  unsigned int seq:16;          /* sequence number */
  unsigned int timestamp:32;    /* timestamp */
  unsigned int ssrc:32;         /* synchronization source */
  guint8 csrclist[4];           /* optional CSRC list, 32 bits each */
} GstRTPHeader;

#define GST_RTP_HEADER_VERSION(data)      (((GstRTPHeader *)(data))->version)
#define GST_RTP_HEADER_PADDING(data)      (((GstRTPHeader *)(data))->padding)
#define GST_RTP_HEADER_EXTENSION(data)    (((GstRTPHeader *)(data))->extension)
#define GST_RTP_HEADER_CSRC_COUNT(data)   (((GstRTPHeader *)(data))->csrc_count)
#define GST_RTP_HEADER_MARKER(data)       (((GstRTPHeader *)(data))->marker)
#define GST_RTP_HEADER_PAYLOAD_TYPE(data) (((GstRTPHeader *)(data))->payload_type)
#define GST_RTP_HEADER_SEQ(data)          (((GstRTPHeader *)(data))->seq)
#define GST_RTP_HEADER_TIMESTAMP(data)    (((GstRTPHeader *)(data))->timestamp)
#define GST_RTP_HEADER_SSRC(data)         (((GstRTPHeader *)(data))->ssrc)
#define GST_RTP_HEADER_CSRC_LIST_OFFSET(data,i)        \
    data + G_STRUCT_OFFSET(GstRTPHeader, csrclist) +   \
    ((i) * sizeof(guint32))
#define GST_RTP_HEADER_CSRC_SIZE(data)   (GST_RTP_HEADER_CSRC_COUNT(data) * sizeof (guint32))

/**
 * gst_rtp_buffer_allocate_data:
 * @buffer: a #GstBuffer
 * @payload_len: the length of the payload
 * @pad_len: the amount of padding
 * @csrc_count: the number of CSRC entries
 *
 * Allocate enough data in @buffer to hold an RTP packet with @csrc_count CSRCs,
 * a payload length of @payload_len and padding of @pad_len.
 * @buffer must be writable and all previous memory in @buffer will be freed.
 * If @pad_len is >0, the padding bit will be set. All other RTP header fields
 * will be set to 0/FALSE.
 */
void
gst_rtp_buffer_allocate_data (GstBuffer * buffer, guint payload_len,
    guint8 pad_len, guint8 csrc_count)
{
  GstMapInfo map;
  GstMemory *mem;
  gsize hlen;

  g_return_if_fail (csrc_count <= 15);
  g_return_if_fail (GST_IS_BUFFER (buffer));
  g_return_if_fail (gst_buffer_is_writable (buffer));

  gst_buffer_remove_all_memory (buffer);

  hlen = GST_RTP_HEADER_LEN + csrc_count * sizeof (guint32);

  mem = gst_allocator_alloc (NULL, hlen, NULL);

  gst_memory_map (mem, &map, GST_MAP_WRITE);
  /* fill in defaults */
  GST_RTP_HEADER_VERSION (map.data) = GST_RTP_VERSION;
  if (pad_len)
    GST_RTP_HEADER_PADDING (map.data) = TRUE;
  else
    GST_RTP_HEADER_PADDING (map.data) = FALSE;
  GST_RTP_HEADER_EXTENSION (map.data) = FALSE;
  GST_RTP_HEADER_CSRC_COUNT (map.data) = csrc_count;
  memset (GST_RTP_HEADER_CSRC_LIST_OFFSET (map.data, 0), 0,
      csrc_count * sizeof (guint32));
  GST_RTP_HEADER_MARKER (map.data) = FALSE;
  GST_RTP_HEADER_PAYLOAD_TYPE (map.data) = 0;
  GST_RTP_HEADER_SEQ (map.data) = 0;
  GST_RTP_HEADER_TIMESTAMP (map.data) = 0;
  GST_RTP_HEADER_SSRC (map.data) = 0;
  gst_memory_unmap (mem, &map);

  gst_buffer_append_memory (buffer, mem);

  if (payload_len) {
    mem = gst_allocator_alloc (NULL, payload_len, NULL);
    gst_buffer_append_memory (buffer, mem);
  }
  if (pad_len) {
    mem = gst_allocator_alloc (NULL, pad_len, NULL);

    gst_memory_map (mem, &map, GST_MAP_WRITE);
    map.data[pad_len - 1] = pad_len;
    gst_memory_unmap (mem, &map);

    gst_buffer_append_memory (buffer, mem);
  }
}

/**
 * gst_rtp_buffer_new_take_data:
 * @data: (array length=len) (transfer full) (element-type guint8):
 *   data for the new buffer
 * @len: the length of data
 *
 * Create a new buffer and set the data and size of the buffer to @data and @len
 * respectively. @data will be freed when the buffer is unreffed, so this
 * function transfers ownership of @data to the new buffer.
 *
 * Returns: (transfer full): A newly allocated buffer with @data and of size @len.
 */
GstBuffer *
gst_rtp_buffer_new_take_data (gpointer data, gsize len)
{
  g_return_val_if_fail (data != NULL, NULL);
  g_return_val_if_fail (len > 0, NULL);

  return gst_buffer_new_wrapped (data, len);
}

/**
 * gst_rtp_buffer_new_copy_data:
 * @data: (array length=len) (element-type guint8): data for the new
 *   buffer
 * @len: the length of data
 *
 * Create a new buffer and set the data to a copy of @len
 * bytes of @data and the size to @len. The data will be freed when the buffer
 * is freed.
 *
 * Returns: (transfer full): A newly allocated buffer with a copy of @data and of size @len.
 */
GstBuffer *
gst_rtp_buffer_new_copy_data (gconstpointer data, gsize len)
{
  return gst_rtp_buffer_new_take_data (g_memdup2 (data, len), len);
}

/**
 * gst_rtp_buffer_new_allocate:
 * @payload_len: the length of the payload
 * @pad_len: the amount of padding
 * @csrc_count: the number of CSRC entries
 *
 * Allocate a new #GstBuffer with enough data to hold an RTP packet with
 * @csrc_count CSRCs, a payload length of @payload_len and padding of @pad_len.
 * All other RTP header fields will be set to 0/FALSE.
 *
 * Returns: (transfer full): A newly allocated buffer that can hold an RTP packet with given
 * parameters.
 */
GstBuffer *
gst_rtp_buffer_new_allocate (guint payload_len, guint8 pad_len,
    guint8 csrc_count)
{
  GstBuffer *result;

  g_return_val_if_fail (csrc_count <= 15, NULL);

  result = gst_buffer_new ();
  gst_rtp_buffer_allocate_data (result, payload_len, pad_len, csrc_count);

  return result;
}

/**
 * gst_rtp_buffer_new_allocate_len:
 * @packet_len: the total length of the packet
 * @pad_len: the amount of padding
 * @csrc_count: the number of CSRC entries
 *
 * Create a new #GstBuffer that can hold an RTP packet that is exactly
 * @packet_len long. The length of the payload depends on @pad_len and
 * @csrc_count and can be calculated with gst_rtp_buffer_calc_payload_len().
 * All RTP header fields will be set to 0/FALSE.
 *
 * Returns: (transfer full): A newly allocated buffer that can hold an RTP packet of @packet_len.
 */
GstBuffer *
gst_rtp_buffer_new_allocate_len (guint packet_len, guint8 pad_len,
    guint8 csrc_count)
{
  guint len;

  g_return_val_if_fail (csrc_count <= 15, NULL);

  len = gst_rtp_buffer_calc_payload_len (packet_len, pad_len, csrc_count);

  return gst_rtp_buffer_new_allocate (len, pad_len, csrc_count);
}

/**
 * gst_rtp_buffer_calc_header_len:
 * @csrc_count: the number of CSRC entries
 *
 * Calculate the header length of an RTP packet with @csrc_count CSRC entries.
 * An RTP packet can have at most 15 CSRC entries.
 *
 * Returns: The length of an RTP header with @csrc_count CSRC entries.
 */
guint
gst_rtp_buffer_calc_header_len (guint8 csrc_count)
{
  g_return_val_if_fail (csrc_count <= 15, 0);

  return GST_RTP_HEADER_LEN + (csrc_count * sizeof (guint32));
}

/**
 * gst_rtp_buffer_calc_packet_len:
 * @payload_len: the length of the payload
 * @pad_len: the amount of padding
 * @csrc_count: the number of CSRC entries
 *
 * Calculate the total length of an RTP packet with a payload size of @payload_len,
 * a padding of @pad_len and a @csrc_count CSRC entries.
 *
 * Returns: The total length of an RTP header with given parameters.
 */
guint
gst_rtp_buffer_calc_packet_len (guint payload_len, guint8 pad_len,
    guint8 csrc_count)
{
  g_return_val_if_fail (csrc_count <= 15, 0);

  return payload_len + GST_RTP_HEADER_LEN + (csrc_count * sizeof (guint32))
      + pad_len;
}

/**
 * gst_rtp_buffer_calc_payload_len:
 * @packet_len: the length of the total RTP packet
 * @pad_len: the amount of padding
 * @csrc_count: the number of CSRC entries
 *
 * Calculate the length of the payload of an RTP packet with size @packet_len,
 * a padding of @pad_len and a @csrc_count CSRC entries.
 *
 * Returns: The length of the payload of an RTP packet  with given parameters.
 */
guint
gst_rtp_buffer_calc_payload_len (guint packet_len, guint8 pad_len,
    guint8 csrc_count)
{
  g_return_val_if_fail (csrc_count <= 15, 0);

  if (packet_len <
      GST_RTP_HEADER_LEN + (csrc_count * sizeof (guint32)) + pad_len)
    return 0;

  return packet_len - GST_RTP_HEADER_LEN - (csrc_count * sizeof (guint32))
      - pad_len;
}

/**
 * gst_rtp_buffer_map:
 * @buffer: a #GstBuffer
 * @flags: #GstMapFlags
 * @rtp: (out): a #GstRTPBuffer
 *
 * Map the contents of @buffer into @rtp.
 *
 * Returns: %TRUE if @buffer could be mapped.
 */
gboolean
gst_rtp_buffer_map (GstBuffer * buffer, GstMapFlags flags, GstRTPBuffer * rtp)
{
  guint8 padding;
  guint8 csrc_count;
  guint header_len;
  guint8 version, pt;
  guint8 *data;
  guint size;
  gsize bufsize, skip;
  guint idx, length;
  guint n_mem;

  g_return_val_if_fail (GST_IS_BUFFER (buffer), FALSE);
  g_return_val_if_fail (rtp != NULL, FALSE);
  g_return_val_if_fail (rtp->buffer == NULL, FALSE);

  n_mem = gst_buffer_n_memory (buffer);
  if (n_mem < 1)
    goto no_memory;

  /* map first memory, this should be the header */
  if (!gst_buffer_map_range (buffer, 0, 1, &rtp->map[0], flags))
    goto map_failed;

  data = rtp->data[0] = rtp->map[0].data;
  size = rtp->map[0].size;

  /* the header must be completely in the first buffer */
  header_len = GST_RTP_HEADER_LEN;
  if (G_UNLIKELY (size < header_len))
    goto wrong_length;

  /* check version */
  version = (data[0] & 0xc0);
  if (G_UNLIKELY (version != (GST_RTP_VERSION << 6)))
    goto wrong_version;

  /* check reserved PT and marker bit, this is to check for RTCP
   * packets. We do a relaxed check, you can still use 72-76 as long
   * as the marker bit is cleared. */
  pt = data[1];
  if (G_UNLIKELY (pt >= 200 && pt <= 204))
    goto reserved_pt;

  /* calc header length with csrc */
  csrc_count = (data[0] & 0x0f);
  header_len += csrc_count * sizeof (guint32);

  rtp->size[0] = header_len;

  bufsize = gst_buffer_get_size (buffer);

  /* calc extension length when present. */
  if (data[0] & 0x10) {
    guint8 *extdata;
    gsize extlen;

    /* find memory for the extension bits, we find the block for the first 4
     * bytes, all other extension bytes should also be in this block */
    if (!gst_buffer_find_memory (buffer, header_len, 4, &idx, &length, &skip))
      goto wrong_length;

    if (!gst_buffer_map_range (buffer, idx, length, &rtp->map[1], flags))
      goto map_failed;

    extdata = rtp->data[1] = rtp->map[1].data + skip;
    /* skip id */
    extdata += 2;
    /* read length as the number of 32 bits words */
    extlen = GST_READ_UINT16_BE (extdata);
    extlen *= sizeof (guint32);
    /* add id and length */
    extlen += 4;

    /* all extension bytes must be in this block */
    if (G_UNLIKELY (rtp->map[1].size < extlen))
      goto wrong_length;

    rtp->size[1] = extlen;

    header_len += rtp->size[1];
  } else {
    rtp->data[1] = NULL;
    rtp->size[1] = 0;
  }

  /* check for padding unless flags says to skip */
  if ((data[0] & 0x20) != 0 &&
      (flags & GST_RTP_BUFFER_MAP_FLAG_SKIP_PADDING) == 0) {
    /* find memory for the padding bits */
    if (!gst_buffer_find_memory (buffer, bufsize - 1, 1, &idx, &length, &skip))
      goto wrong_length;

    if (!gst_buffer_map_range (buffer, idx, length, &rtp->map[3], flags))
      goto map_failed;

    padding = rtp->map[3].data[skip];
    rtp->data[3] = rtp->map[3].data + skip + 1 - padding;
    rtp->size[3] = padding;

    if (skip + 1 < padding)
      goto wrong_length;
  } else {
    rtp->data[3] = NULL;
    rtp->size[3] = 0;
    padding = 0;
  }

  /* check if padding and header not bigger than packet length */
  if (G_UNLIKELY (bufsize < padding + header_len))
    goto wrong_padding;

  rtp->buffer = buffer;

  if (n_mem == 1) {
    /* we have mapped the buffer already, so might just as well fill in the
     * payload pointer and size and avoid another buffer map/unmap later */
    rtp->data[2] = rtp->map[0].data + header_len;
    rtp->size[2] = bufsize - header_len - padding;
  } else {
    /* we have not yet mapped the payload */
    rtp->data[2] = NULL;
    rtp->size[2] = 0;
  }

  /* rtp->state = 0; *//* unused */

  return TRUE;

  /* ERRORS */
no_memory:
  {
    GST_ERROR ("buffer without memory");
    return FALSE;
  }
map_failed:
  {
    GST_ERROR ("failed to map memory");
    return FALSE;
  }
wrong_length:
  {
    GST_DEBUG ("length check failed");
    goto dump_packet;
  }
wrong_version:
  {
    GST_DEBUG ("version check failed (%d != %d)", version, GST_RTP_VERSION);
    goto dump_packet;
  }
reserved_pt:
  {
    GST_DEBUG ("reserved PT %d found", pt);
    goto dump_packet;
  }
wrong_padding:
  {
    GST_DEBUG ("padding check failed (%" G_GSIZE_FORMAT " - %d < %d)", bufsize,
        header_len, padding);
    goto dump_packet;
  }
dump_packet:
  {
    gint i;

    GST_MEMDUMP ("buffer", data, size);

    for (i = 0; i < G_N_ELEMENTS (rtp->map); ++i) {
      if (rtp->map[i].memory != NULL)
        gst_buffer_unmap (buffer, &rtp->map[i]);
    }
    return FALSE;
  }
}

/**
 * gst_rtp_buffer_unmap:
 * @rtp: a #GstRTPBuffer
 *
 * Unmap @rtp previously mapped with gst_rtp_buffer_map().
 */
void
gst_rtp_buffer_unmap (GstRTPBuffer * rtp)
{
  gint i;

  g_return_if_fail (rtp != NULL);
  g_return_if_fail (rtp->buffer != NULL);

  for (i = 0; i < 4; i++) {
    if (rtp->map[i].memory != NULL) {
      gst_buffer_unmap (rtp->buffer, &rtp->map[i]);
      rtp->map[i].memory = NULL;
    }
    rtp->data[i] = NULL;
    rtp->size[i] = 0;
  }
  rtp->buffer = NULL;
}


/**
 * gst_rtp_buffer_set_packet_len:
 * @rtp: the RTP packet
 * @len: the new packet length
 *
 * Set the total @rtp size to @len. The data in the buffer will be made
 * larger if needed. Any padding will be removed from the packet.
 */
void
gst_rtp_buffer_set_packet_len (GstRTPBuffer * rtp, guint len)
{
  guint8 *data;

  data = rtp->data[0];

  /* FIXME */

  if (rtp->map[0].maxsize <= len) {
    /* FIXME, realloc bigger space */
    g_warning ("not implemented");
  }

  gst_buffer_set_size (rtp->buffer, len);
  rtp->map[0].size = len;

  /* remove any padding */
  GST_RTP_HEADER_PADDING (data) = FALSE;
}

/**
 * gst_rtp_buffer_get_packet_len:
 * @rtp: the RTP packet
 *
 * Return the total length of the packet in @buffer.
 *
 * Returns: The total length of the packet in @buffer.
 */
guint
gst_rtp_buffer_get_packet_len (GstRTPBuffer * rtp)
{
  return gst_buffer_get_size (rtp->buffer);
}

/**
 * gst_rtp_buffer_get_header_len:
 * @rtp: the RTP packet
 *
 * Return the total length of the header in @buffer. This include the length of
 * the fixed header, the CSRC list and the extension header.
 *
 * Returns: The total length of the header in @buffer.
 */
guint
gst_rtp_buffer_get_header_len (GstRTPBuffer * rtp)
{
  return rtp->size[0] + rtp->size[1];
}

/**
 * gst_rtp_buffer_get_version:
 * @rtp: the RTP packet
 *
 * Get the version number of the RTP packet in @buffer.
 *
 * Returns: The version of @buffer.
 */
guint8
gst_rtp_buffer_get_version (GstRTPBuffer * rtp)
{
  return GST_RTP_HEADER_VERSION (rtp->data[0]);
}

/**
 * gst_rtp_buffer_set_version:
 * @rtp: the RTP packet
 * @version: the new version
 *
 * Set the version of the RTP packet in @buffer to @version.
 */
void
gst_rtp_buffer_set_version (GstRTPBuffer * rtp, guint8 version)
{
  g_return_if_fail (version < 0x04);

  GST_RTP_HEADER_VERSION (rtp->data[0]) = version;
}

/**
 * gst_rtp_buffer_get_padding:
 * @rtp: the RTP packet
 *
 * Check if the padding bit is set on the RTP packet in @buffer.
 *
 * Returns: TRUE if @buffer has the padding bit set.
 */
gboolean
gst_rtp_buffer_get_padding (GstRTPBuffer * rtp)
{
  return GST_RTP_HEADER_PADDING (rtp->data[0]);
}

/**
 * gst_rtp_buffer_set_padding:
 * @rtp: the buffer
 * @padding: the new padding
 *
 * Set the padding bit on the RTP packet in @buffer to @padding.
 */
void
gst_rtp_buffer_set_padding (GstRTPBuffer * rtp, gboolean padding)
{
  GST_RTP_HEADER_PADDING (rtp->data[0]) = padding;
}

/**
 * gst_rtp_buffer_pad_to:
 * @rtp: the RTP packet
 * @len: the new amount of padding
 *
 * Set the amount of padding in the RTP packet in @buffer to
 * @len. If @len is 0, the padding is removed.
 *
 * NOTE: This function does not work correctly.
 */
void
gst_rtp_buffer_pad_to (GstRTPBuffer * rtp, guint len)
{
  guint8 *data;

  data = rtp->data[0];

  if (len > 0)
    GST_RTP_HEADER_PADDING (data) = TRUE;
  else
    GST_RTP_HEADER_PADDING (data) = FALSE;

  /* FIXME, set the padding byte at the end of the payload data */
}

/**
 * gst_rtp_buffer_get_extension:
 * @rtp: the RTP packet
 *
 * Check if the extension bit is set on the RTP packet in @buffer.
 *
 * Returns: TRUE if @buffer has the extension bit set.
 */
gboolean
gst_rtp_buffer_get_extension (GstRTPBuffer * rtp)
{
  return GST_RTP_HEADER_EXTENSION (rtp->data[0]);
}

/**
 * gst_rtp_buffer_set_extension:
 * @rtp: the RTP packet
 * @extension: the new extension
 *
 * Set the extension bit on the RTP packet in @buffer to @extension.
 */
void
gst_rtp_buffer_set_extension (GstRTPBuffer * rtp, gboolean extension)
{
  GST_RTP_HEADER_EXTENSION (rtp->data[0]) = extension;
}

/**
 * gst_rtp_buffer_get_extension_data: (skip)
 * @rtp: the RTP packet
 * @bits: (optional) (out): location for result bits
 * @data: (optional) (out) (array) (element-type guint8) (transfer none): location for data
 * @wordlen: (optional) (out): location for length of @data in 32 bits words
 *
 * Get the extension data. @bits will contain the extension 16 bits of custom
 * data. @data will point to the data in the extension and @wordlen will contain
 * the length of @data in 32 bits words.
 *
 * If @buffer did not contain an extension, this function will return %FALSE
 * with @bits, @data and @wordlen unchanged.
 *
 * Returns: TRUE if @buffer had the extension bit set.
 */
gboolean
gst_rtp_buffer_get_extension_data (GstRTPBuffer * rtp, guint16 * bits,
    gpointer * data, guint * wordlen)
{
  guint8 *pdata;

  /* move to the extension */
  pdata = rtp->data[1];
  if (!pdata)
    return FALSE;

  if (bits)
    *bits = GST_READ_UINT16_BE (pdata);
  if (wordlen)
    *wordlen = GST_READ_UINT16_BE (pdata + 2);
  pdata += 4;
  if (data)
    *data = (gpointer *) pdata;

  return TRUE;
}

/**
 * gst_rtp_buffer_get_extension_bytes: (rename-to gst_rtp_buffer_get_extension_data)
 * @rtp: the RTP packet
 * @bits: (out): location for header bits
 *
 * Similar to gst_rtp_buffer_get_extension_data, but more suitable for language
 * bindings usage. @bits will contain the extension 16 bits of custom data and
 * the extension data (not including the extension header) is placed in a new
 * #GBytes structure.
 *
 * If @rtp did not contain an extension, this function will return %NULL, with
 * @bits unchanged. If there is an extension header but no extension data then
 * an empty #GBytes will be returned.
 *
 * Returns: (transfer full) (nullable): A new #GBytes if an extension header was present
 * and %NULL otherwise.
 *
 * Since: 1.2
 */
GBytes *
gst_rtp_buffer_get_extension_bytes (GstRTPBuffer * rtp, guint16 * bits)
{
  gpointer buf_data = NULL;
  guint buf_len;

  g_return_val_if_fail (rtp != NULL, FALSE);

  if (!gst_rtp_buffer_get_extension_data (rtp, bits, &buf_data, &buf_len))
    return NULL;

  if (buf_len == 0) {
    /* if no extension data is present return an empty GBytes */
    buf_data = NULL;
  }

  /* multiply length with 4 to get length in bytes */
  return g_bytes_new (buf_data, 4 * buf_len);
}

static gboolean
gst_rtp_buffer_map_payload (GstRTPBuffer * rtp)
{
  guint hlen, plen;
  guint idx, length;
  gsize skip;

  if (rtp->map[2].memory != NULL)
    return TRUE;

  hlen = gst_rtp_buffer_get_header_len (rtp);
  plen = gst_buffer_get_size (rtp->buffer) - hlen - rtp->size[3];

  if (!gst_buffer_find_memory (rtp->buffer, hlen, plen, &idx, &length, &skip))
    return FALSE;

  if (!gst_buffer_map_range (rtp->buffer, idx, length, &rtp->map[2],
          rtp->map[0].flags))
    return FALSE;

  rtp->data[2] = rtp->map[2].data + skip;
  rtp->size[2] = plen;

  return TRUE;
}

/* ensure header, payload and padding are in separate buffers */
static void
ensure_buffers (GstRTPBuffer * rtp)
{
  guint i, pos;
  gboolean changed = FALSE;

  /* make sure payload is mapped */
  gst_rtp_buffer_map_payload (rtp);

  for (i = 0, pos = 0; i < 4; i++) {
    if (rtp->size[i]) {
      gsize offset = (guint8 *) rtp->data[i] - rtp->map[i].data;

      if (offset != 0 || rtp->map[i].size != rtp->size[i]) {
        GstMemory *mem;

        /* make copy */
        mem = gst_memory_copy (rtp->map[i].memory, offset, rtp->size[i]);

        /* insert new memory */
        gst_buffer_insert_memory (rtp->buffer, pos, mem);

        changed = TRUE;
      }
      pos++;
    }
  }

  if (changed) {
    GstBuffer *buf = rtp->buffer;

    gst_rtp_buffer_unmap (rtp);
    gst_buffer_remove_memory_range (buf, pos, -1);
    gst_rtp_buffer_map (buf, GST_MAP_READWRITE, rtp);
  }
}

/**
 * gst_rtp_buffer_set_extension_data:
 * @rtp: the RTP packet
 * @bits: the bits specific for the extension
 * @length: the length that counts the number of 32-bit words in
 * the extension, excluding the extension header ( therefore zero is a valid length)
 *
 * Set the extension bit of the rtp buffer and fill in the @bits and @length of the
 * extension header. If the existing extension data is not large enough, it will
 * be made larger.
 *
 * Will also shorten the extension data from 1.20.
 *
 * Returns: True if done.
 */
gboolean
gst_rtp_buffer_set_extension_data (GstRTPBuffer * rtp, guint16 bits,
    guint16 length)
{
  guint32 min_size = 0;
  guint8 *data;
  GstMemory *mem = NULL;

  ensure_buffers (rtp);

  /* this is the size of the extension data we need */
  min_size = 4 + length * sizeof (guint32);

  /* we should allocate and map the extension data */
  if (rtp->data[1] == NULL || min_size > rtp->size[1]) {
    GstMapInfo map;

    /* we don't have (enough) extension data, make some */
    mem = gst_allocator_alloc (NULL, min_size, NULL);

    if (rtp->data[1]) {
      /* copy old data & initialize the remainder of the new buffer */
      gst_memory_map (mem, &map, GST_MAP_WRITE);
      memcpy (map.data, rtp->data[1], rtp->size[1]);
      if (min_size > rtp->size[1]) {
        memset (map.data + rtp->size[1], 0, min_size - rtp->size[1]);
      }
      gst_memory_unmap (mem, &map);

      /* unmap old */
      gst_buffer_unmap (rtp->buffer, &rtp->map[1]);
      gst_buffer_replace_memory (rtp->buffer, 1, mem);
    } else {
      /* don't leak data from uninitialized memory via the padding */
      gst_memory_map (mem, &map, GST_MAP_WRITE);
      memset (map.data, 0, map.size);
      gst_memory_unmap (mem, &map);

      /* we didn't have extension data, add */
      gst_buffer_insert_memory (rtp->buffer, 1, mem);
    }

    /* map new */
    gst_memory_map (mem, &rtp->map[1], GST_MAP_READWRITE);
    gst_memory_ref (mem);
    rtp->data[1] = rtp->map[1].data;
    rtp->size[1] = rtp->map[1].size;
  } else if (min_size < rtp->size[1]) {
    GstMemory *mem = rtp->map[1].memory;

    gst_memory_ref (mem);
    gst_buffer_unmap (rtp->buffer, &rtp->map[1]);
    gst_memory_resize (mem, 0, min_size);
    gst_memory_map (mem, &rtp->map[1], GST_MAP_READWRITE);
    rtp->data[1] = rtp->map[1].data;
    rtp->size[1] = rtp->map[1].size;
  }

  /* now we can set the extension bit */
  data = rtp->data[0];
  GST_RTP_HEADER_EXTENSION (data) = TRUE;

  data = rtp->data[1];
  GST_WRITE_UINT16_BE (data, bits);
  GST_WRITE_UINT16_BE (data + 2, length);

  return TRUE;
}

/**
 * gst_rtp_buffer_remove_extension_data:
 * @rtp: the RTP packet
 *
 * Unsets the extension bit of the RTP buffer and removes the extension header
 * and data.
 *
 * If the RTP buffer has no header extension data, the action has no effect.
 * The RTP buffer must be mapped READWRITE only once and the underlying
 * GstBuffer must be writable.
 *
 * Since: 1.20
 */
void
gst_rtp_buffer_remove_extension_data (GstRTPBuffer * rtp)
{
  g_return_if_fail (gst_buffer_is_writable (rtp->buffer));
  g_return_if_fail (rtp->map[0].flags & GST_MAP_WRITE);

  if (rtp->data[1] != NULL) {
    GstBuffer *buf = rtp->buffer;

    ensure_buffers (rtp);

    GST_RTP_HEADER_EXTENSION (rtp->data[0]) = FALSE;
    gst_rtp_buffer_unmap (rtp);
    gst_buffer_remove_memory (buf, 1);
    gst_rtp_buffer_map (buf, GST_MAP_READWRITE, rtp);
  }
}

/**
 * gst_rtp_buffer_get_ssrc:
 * @rtp: the RTP packet
 *
 * Get the SSRC of the RTP packet in @buffer.
 *
 * Returns: the SSRC of @buffer in host order.
 */
guint32
gst_rtp_buffer_get_ssrc (GstRTPBuffer * rtp)
{
  return g_ntohl (GST_RTP_HEADER_SSRC (rtp->data[0]));
}

/**
 * gst_rtp_buffer_set_ssrc:
 * @rtp: the RTP packet
 * @ssrc: the new SSRC
 *
 * Set the SSRC on the RTP packet in @buffer to @ssrc.
 */
void
gst_rtp_buffer_set_ssrc (GstRTPBuffer * rtp, guint32 ssrc)
{
  GST_RTP_HEADER_SSRC (rtp->data[0]) = g_htonl (ssrc);
}

/**
 * gst_rtp_buffer_get_csrc_count:
 * @rtp: the RTP packet
 *
 * Get the CSRC count of the RTP packet in @buffer.
 *
 * Returns: the CSRC count of @buffer.
 */
guint8
gst_rtp_buffer_get_csrc_count (GstRTPBuffer * rtp)
{
  return GST_RTP_HEADER_CSRC_COUNT (rtp->data[0]);
}

/**
 * gst_rtp_buffer_get_csrc:
 * @rtp: the RTP packet
 * @idx: the index of the CSRC to get
 *
 * Get the CSRC at index @idx in @buffer.
 *
 * Returns: the CSRC at index @idx in host order.
 */
guint32
gst_rtp_buffer_get_csrc (GstRTPBuffer * rtp, guint8 idx)
{
  guint8 *data;

  data = rtp->data[0];

  g_return_val_if_fail (idx < GST_RTP_HEADER_CSRC_COUNT (data), 0);

  return GST_READ_UINT32_BE (GST_RTP_HEADER_CSRC_LIST_OFFSET (data, idx));
}

/**
 * gst_rtp_buffer_set_csrc:
 * @rtp: the RTP packet
 * @idx: the CSRC index to set
 * @csrc: the CSRC in host order to set at @idx
 *
 * Modify the CSRC at index @idx in @buffer to @csrc.
 */
void
gst_rtp_buffer_set_csrc (GstRTPBuffer * rtp, guint8 idx, guint32 csrc)
{
  guint8 *data;

  data = rtp->data[0];

  g_return_if_fail (idx < GST_RTP_HEADER_CSRC_COUNT (data));

  GST_WRITE_UINT32_BE (GST_RTP_HEADER_CSRC_LIST_OFFSET (data, idx), csrc);
}

/**
 * gst_rtp_buffer_get_marker:
 * @rtp: the RTP packet
 *
 * Check if the marker bit is set on the RTP packet in @buffer.
 *
 * Returns: TRUE if @buffer has the marker bit set.
 */
gboolean
gst_rtp_buffer_get_marker (GstRTPBuffer * rtp)
{
  return GST_RTP_HEADER_MARKER (rtp->data[0]);
}

/**
 * gst_rtp_buffer_set_marker:
 * @rtp: the RTP packet
 * @marker: the new marker
 *
 * Set the marker bit on the RTP packet in @buffer to @marker.
 */
void
gst_rtp_buffer_set_marker (GstRTPBuffer * rtp, gboolean marker)
{
  GST_RTP_HEADER_MARKER (rtp->data[0]) = marker;
}

/**
 * gst_rtp_buffer_get_payload_type:
 * @rtp: the RTP packet
 *
 * Get the payload type of the RTP packet in @buffer.
 *
 * Returns: The payload type.
 */
guint8
gst_rtp_buffer_get_payload_type (GstRTPBuffer * rtp)
{
  return GST_RTP_HEADER_PAYLOAD_TYPE (rtp->data[0]);
}

/**
 * gst_rtp_buffer_set_payload_type:
 * @rtp: the RTP packet
 * @payload_type: the new type
 *
 * Set the payload type of the RTP packet in @buffer to @payload_type.
 */
void
gst_rtp_buffer_set_payload_type (GstRTPBuffer * rtp, guint8 payload_type)
{
  g_return_if_fail (payload_type < 0x80);

  GST_RTP_HEADER_PAYLOAD_TYPE (rtp->data[0]) = payload_type;
}

/**
 * gst_rtp_buffer_get_seq:
 * @rtp: the RTP packet
 *
 * Get the sequence number of the RTP packet in @buffer.
 *
 * Returns: The sequence number in host order.
 */
guint16
gst_rtp_buffer_get_seq (GstRTPBuffer * rtp)
{
  return g_ntohs (GST_RTP_HEADER_SEQ (rtp->data[0]));
}

/**
 * gst_rtp_buffer_set_seq:
 * @rtp: the RTP packet
 * @seq: the new sequence number
 *
 * Set the sequence number of the RTP packet in @buffer to @seq.
 */
void
gst_rtp_buffer_set_seq (GstRTPBuffer * rtp, guint16 seq)
{
  GST_RTP_HEADER_SEQ (rtp->data[0]) = g_htons (seq);
}

/**
 * gst_rtp_buffer_get_timestamp:
 * @rtp: the RTP packet
 *
 * Get the timestamp of the RTP packet in @buffer.
 *
 * Returns: The timestamp in host order.
 */
guint32
gst_rtp_buffer_get_timestamp (GstRTPBuffer * rtp)
{
  return g_ntohl (GST_RTP_HEADER_TIMESTAMP (rtp->data[0]));
}

/**
 * gst_rtp_buffer_set_timestamp:
 * @rtp: the RTP packet
 * @timestamp: the new timestamp
 *
 * Set the timestamp of the RTP packet in @buffer to @timestamp.
 */
void
gst_rtp_buffer_set_timestamp (GstRTPBuffer * rtp, guint32 timestamp)
{
  GST_RTP_HEADER_TIMESTAMP (rtp->data[0]) = g_htonl (timestamp);
}


/**
 * gst_rtp_buffer_get_payload_subbuffer:
 * @rtp: the RTP packet
 * @offset: the offset in the payload
 * @len: the length in the payload
 *
 * Create a subbuffer of the payload of the RTP packet in @buffer. @offset bytes
 * are skipped in the payload and the subbuffer will be of size @len.
 * If @len is -1 the total payload starting from @offset is subbuffered.
 *
 * Returns: (transfer full): A new buffer with the specified data of the payload.
 */
GstBuffer *
gst_rtp_buffer_get_payload_subbuffer (GstRTPBuffer * rtp, guint offset,
    guint len)
{
  guint poffset, plen;

  plen = gst_rtp_buffer_get_payload_len (rtp);
  /* we can't go past the length */
  if (G_UNLIKELY (offset > plen))
    goto wrong_offset;

  /* apply offset */
  poffset = gst_rtp_buffer_get_header_len (rtp) + offset;
  plen -= offset;

  /* see if we need to shrink the buffer based on @len */
  if (len != -1 && len < plen)
    plen = len;

  return gst_buffer_copy_region (rtp->buffer, GST_BUFFER_COPY_ALL, poffset,
      plen);

  /* ERRORS */
wrong_offset:
  {
    g_warning ("offset=%u should be less than plen=%u", offset, plen);
    return NULL;
  }
}

/**
 * gst_rtp_buffer_get_payload_buffer:
 * @rtp: the RTP packet
 *
 * Create a buffer of the payload of the RTP packet in @buffer. This function
 * will internally create a subbuffer of @buffer so that a memcpy can be
 * avoided.
 *
 * Returns: (transfer full): A new buffer with the data of the payload.
 */
GstBuffer *
gst_rtp_buffer_get_payload_buffer (GstRTPBuffer * rtp)
{
  return gst_rtp_buffer_get_payload_subbuffer (rtp, 0, -1);
}

/**
 * gst_rtp_buffer_get_payload_len:
 * @rtp: the RTP packet
 *
 * Get the length of the payload of the RTP packet in @buffer.
 *
 * Returns: The length of the payload in @buffer.
 */
guint
gst_rtp_buffer_get_payload_len (GstRTPBuffer * rtp)
{
  return gst_buffer_get_size (rtp->buffer) - gst_rtp_buffer_get_header_len (rtp)
      - rtp->size[3];
}

/**
 * gst_rtp_buffer_get_payload: (skip)
 * @rtp: the RTP packet
 *
 * Get a pointer to the payload data in @buffer. This pointer is valid as long
 * as a reference to @buffer is held.
 *
 * Returns: (array) (element-type guint8) (transfer none) (nullable): A pointer
 * to the payload data in @buffer.
 */
gpointer
gst_rtp_buffer_get_payload (GstRTPBuffer * rtp)
{
  if (rtp->data[2])
    return rtp->data[2];

  if (!gst_rtp_buffer_map_payload (rtp))
    return NULL;

  return rtp->data[2];
}

/**
 * gst_rtp_buffer_get_payload_bytes: (rename-to gst_rtp_buffer_get_payload)
 * @rtp: the RTP packet
 *
 * Similar to gst_rtp_buffer_get_payload, but more suitable for language
 * bindings usage. The return value is a pointer to a #GBytes structure
 * containing the payload data in @rtp.
 *
 * Returns: (transfer full) (nullable): A new #GBytes containing the payload data in @rtp.
 *
 * Since: 1.2
 */
GBytes *
gst_rtp_buffer_get_payload_bytes (GstRTPBuffer * rtp)
{
  gpointer data;

  g_return_val_if_fail (rtp != NULL, NULL);

  data = gst_rtp_buffer_get_payload (rtp);
  if (data == NULL)
    return NULL;

  return g_bytes_new (data, gst_rtp_buffer_get_payload_len (rtp));
}

/**
 * gst_rtp_buffer_default_clock_rate:
 * @payload_type: the static payload type
 *
 * Get the default clock-rate for the static payload type @payload_type.
 *
 * Returns: the default clock rate or -1 if the payload type is not static or
 * the clock-rate is undefined.
 */
guint32
gst_rtp_buffer_default_clock_rate (guint8 payload_type)
{
  const GstRTPPayloadInfo *info;
  guint32 res;

  info = gst_rtp_payload_info_for_pt (payload_type);
  if (!info)
    return -1;

  res = info->clock_rate;
  /* 0 means unknown so we have to return -1 from this function */
  if (res == 0)
    res = -1;

  return res;
}

/**
 * gst_rtp_buffer_compare_seqnum:
 * @seqnum1: a sequence number
 * @seqnum2: a sequence number
 *
 * Compare two sequence numbers, taking care of wraparounds. This function
 * returns the difference between @seqnum1 and @seqnum2.
 *
 * Returns: a negative value if @seqnum1 is bigger than @seqnum2, 0 if they
 * are equal or a positive value if @seqnum1 is smaller than @segnum2.
 */
gint
gst_rtp_buffer_compare_seqnum (guint16 seqnum1, guint16 seqnum2)
{
  /* See http://en.wikipedia.org/wiki/Serial_number_arithmetic
   * for an explanation why this does the right thing even for
   * wraparounds, under the assumption that the difference is
   * never bigger than 2**15 sequence numbers
   */
  return (gint16) (seqnum2 - seqnum1);
}

/**
 * gst_rtp_buffer_ext_timestamp:
 * @exttimestamp: (inout): a previous extended timestamp
 * @timestamp: a new timestamp
 *
 * Update the @exttimestamp field with the extended timestamp of @timestamp
 * For the first call of the method, @exttimestamp should point to a location
 * with a value of -1.
 *
 * This function is able to handle both forward and backward timestamps taking
 * into account:
 *   - timestamp wraparound making sure that the returned value is properly increased.
 *   - timestamp unwraparound making sure that the returned value is properly decreased.
 *
 * Returns: The extended timestamp of @timestamp or 0 if the result can't go anywhere backwards.
 */
guint64
gst_rtp_buffer_ext_timestamp (guint64 * exttimestamp, guint32 timestamp)
{
  guint64 result, ext;

  g_return_val_if_fail (exttimestamp != NULL, -1);

  ext = *exttimestamp;

  if (ext == -1) {
    result = (G_GUINT64_CONSTANT (1) << 32) + timestamp;
  } else {
    /* pick wraparound counter from previous timestamp and add to new timestamp */
    result = timestamp + (ext & ~(G_GUINT64_CONSTANT (0xffffffff)));

    /* check for timestamp wraparound */
    if (result < ext) {
      guint64 diff = ext - result;

      if (diff > G_MAXINT32) {
        /* timestamp went backwards more than allowed, we wrap around and get
         * updated extended timestamp. */
        result += (G_GUINT64_CONSTANT (1) << 32);
      }
    } else {
      guint64 diff = result - ext;

      if (diff > G_MAXINT32) {
        if (result < (G_GUINT64_CONSTANT (1) << 32)) {
          GST_WARNING
              ("Cannot unwrap, any wrapping took place yet. Returning 0 without updating extended timestamp.");
          return 0;
        } else {
          /* timestamp went forwards more than allowed, we unwrap around and get
           * updated extended timestamp. */
          result -= (G_GUINT64_CONSTANT (1) << 32);
          /* We don't want the extended timestamp storage to go back, ever */
          return result;
        }
      }
    }
  }

  *exttimestamp = result;

  return result;
}


static gboolean
_get_extension_onebyte_header (const guint8 * pdata, guint len,
    guint16 bit_pattern, guint8 id, guint nth, gpointer * data, guint * size)
{
  gulong offset = 0;
  guint count = 0;

  g_return_val_if_fail (id > 0 && id < 15, FALSE);

  if (bit_pattern != 0xBEDE)
    return FALSE;

  for (;;) {
    guint8 read_id, read_len;

    if (offset + 1 >= len)
      break;

    read_id = GST_READ_UINT8 (pdata + offset) >> 4;
    read_len = (GST_READ_UINT8 (pdata + offset) & 0x0F) + 1;
    offset += 1;

    /* ID 0 means its padding, skip */
    if (read_id == 0)
      continue;

    /* ID 15 is special and means we should stop parsing */
    if (read_id == 15)
      break;

    /* Ignore extension headers where the size does not fit */
    if (offset + read_len > len)
      break;

    /* If we have the right one */
    if (id == read_id) {
      if (nth == count) {
        if (data)
          *data = (gpointer) & pdata[offset];
        if (size)
          *size = read_len;

        return TRUE;
      }

      count++;
    }
    offset += read_len;

    if (offset >= len)
      break;
  }

  return FALSE;
}


/**
 * gst_rtp_buffer_get_extension_onebyte_header_from_bytes:
 * @bytes: #GBytes
 * @bit_pattern: The bit-pattern. Anything but 0xBEDE is rejected.
 * @id: The ID of the header extension to be read (between 1 and 14).
 * @nth: Read the nth extension packet with the requested ID
 * @data: (out) (array length=size) (element-type guint8) (transfer none):
 *   location for data
 * @size: (out): the size of the data in bytes
 *
 * Similar to gst_rtp_buffer_get_extension_onebyte_header, but working
 * on the #GBytes you get from gst_rtp_buffer_get_extension_bytes.
 * Parses RFC 5285 style header extensions with a one byte header. It will
 * return the nth extension with the requested id.
 *
 * Returns: TRUE if @bytes had the requested header extension
 *
 * Since: 1.18
 */
gboolean
gst_rtp_buffer_get_extension_onebyte_header_from_bytes (GBytes * bytes,
    guint16 bit_pattern, guint8 id, guint nth, gpointer * data, guint * size)
{
  const guint8 *pdata = g_bytes_get_data (bytes, NULL);
  gsize len = g_bytes_get_size (bytes);
  return _get_extension_onebyte_header (pdata, len, bit_pattern, id, nth, data,
      size);
}


/**
 * gst_rtp_buffer_get_extension_onebyte_header:
 * @rtp: the RTP packet
 * @id: The ID of the header extension to be read (between 1 and 14).
 * @nth: Read the nth extension packet with the requested ID
 * @data: (optional) (out) (array length=size) (element-type guint8) (transfer none):
 *   location for data
 * @size: (optional) (out): the size of the data in bytes
 *
 * Parses RFC 5285 style header extensions with a one byte header. It will
 * return the nth extension with the requested id.
 *
 * Returns: TRUE if @buffer had the requested header extension
 */

gboolean
gst_rtp_buffer_get_extension_onebyte_header (GstRTPBuffer * rtp, guint8 id,
    guint nth, gpointer * data, guint * size)
{
  guint16 bit_pattern;
  guint8 *pdata;
  guint wordlen;

  if (!gst_rtp_buffer_get_extension_data (rtp, &bit_pattern, (gpointer) & pdata,
          &wordlen))
    return FALSE;

  return _get_extension_onebyte_header (pdata, wordlen * 4, bit_pattern, id,
      nth, data, size);
}


/**
 * gst_rtp_buffer_get_extension_twobytes_header:
 * @rtp: the RTP packet
 * @appbits: (optional) (out): Application specific bits
 * @id: The ID of the header extension to be read (between 1 and 14).
 * @nth: Read the nth extension packet with the requested ID
 * @data: (optional) (out) (array length=size) (element-type guint8) (transfer none):
 *   location for data
 * @size: (optional) (out): the size of the data in bytes
 *
 * Parses RFC 5285 style header extensions with a two bytes header. It will
 * return the nth extension with the requested id.
 *
 * Returns: TRUE if @buffer had the requested header extension
 */

gboolean
gst_rtp_buffer_get_extension_twobytes_header (GstRTPBuffer * rtp,
    guint8 * appbits, guint8 id, guint nth, gpointer * data, guint * size)
{
  guint16 bits;
  guint8 *pdata = NULL;
  guint wordlen;
  guint bytelen;
  gulong offset = 0;
  guint count = 0;

  if (!gst_rtp_buffer_get_extension_data (rtp, &bits, (gpointer *) & pdata,
          &wordlen))
    return FALSE;

  if (bits >> 4 != 0x100)
    return FALSE;

  bytelen = wordlen * 4;

  for (;;) {
    guint8 read_id, read_len;

    if (offset + 2 >= bytelen)
      break;

    read_id = GST_READ_UINT8 (pdata + offset);
    offset += 1;

    if (read_id == 0)
      continue;

    read_len = GST_READ_UINT8 (pdata + offset);
    offset += 1;

    /* Ignore extension headers where the size does not fit */
    if (offset + read_len > bytelen)
      break;

    /* If we have the right one, return it */
    if (id == read_id) {
      if (nth == count) {
        if (data)
          *data = pdata + offset;
        if (size)
          *size = read_len;
        if (appbits)
          *appbits = bits;

        return TRUE;
      }

      count++;
    }
    offset += read_len;
  }

  return FALSE;
}

static gboolean
get_onebyte_header_end_offset (guint8 * pdata, guint wordlen, guint * offset)
{
  guint bytelen = wordlen * 4;
  guint paddingcount = 0;

  *offset = 0;

  while (*offset + 1 < bytelen) {
    guint8 read_id, read_len;

    read_id = GST_READ_UINT8 (pdata + *offset) >> 4;
    read_len = (GST_READ_UINT8 (pdata + *offset) & 0x0F) + 1;
    *offset += 1;

    /* ID 0 means its padding, skip */
    if (read_id == 0) {
      paddingcount++;
      continue;
    }

    paddingcount = 0;

    /* ID 15 is special and means we should stop parsing */
    /* It also means we can't add an extra packet */
    if (read_id == 15) {
      return FALSE;
    }

    /* Ignore extension headers where the size does not fit */
    if (*offset + read_len > bytelen) {
      return FALSE;
    }

    *offset += read_len;
  }

  *offset -= paddingcount;

  return TRUE;
}

/**
 * gst_rtp_buffer_add_extension_onebyte_header:
 * @rtp: the RTP packet
 * @id: The ID of the header extension (between 1 and 14).
 * @data: (array length=size) (element-type guint8): location for data
 * @size: the size of the data in bytes
 *
 * Adds a RFC 5285 header extension with a one byte header to the end of the
 * RTP header. If there is already a RFC 5285 header extension with a one byte
 * header, the new extension will be appended.
 * It will not work if there is already a header extension that does not follow
 * the mechanism described in RFC 5285 or if there is a header extension with
 * a two bytes header as described in RFC 5285. In that case, use
 * gst_rtp_buffer_add_extension_twobytes_header()
 *
 * Returns: %TRUE if header extension could be added
 */

gboolean
gst_rtp_buffer_add_extension_onebyte_header (GstRTPBuffer * rtp, guint8 id,
    gconstpointer data, guint size)
{
  guint16 bits;
  guint8 *pdata = 0;
  guint wordlen = 0;
  guint wordlen_new;
  gboolean has_bit;
  guint extlen, offset = 0;

  g_return_val_if_fail (id > 0 && id < 15, FALSE);
  g_return_val_if_fail (size >= 1 && size <= 16, FALSE);
  g_return_val_if_fail (gst_buffer_is_writable (rtp->buffer), FALSE);

  has_bit = gst_rtp_buffer_get_extension_data (rtp, &bits,
      (gpointer) & pdata, &wordlen);

  if (has_bit) {
    if (bits != 0xBEDE)
      return FALSE;

    if (!get_onebyte_header_end_offset (pdata, wordlen, &offset))
      return FALSE;
  }

  /* the required size of the new extension data */
  extlen = offset + size + 1;
  /* calculate amount of words */
  wordlen_new = extlen / 4 + ((extlen % 4) ? 1 : 0);
  wordlen_new = MAX (wordlen_new, wordlen);

  gst_rtp_buffer_set_extension_data (rtp, 0xBEDE, wordlen_new);
  gst_rtp_buffer_get_extension_data (rtp, &bits, (gpointer) & pdata, &wordlen);

  pdata += offset;

  pdata[0] = (id << 4) | (0x0F & (size - 1));
  memcpy (pdata + 1, data, size);

  if (extlen % 4)
    memset (pdata + 1 + size, 0, 4 - (extlen % 4));

  return TRUE;
}


static guint
get_twobytes_header_end_offset (const guint8 * pdata, guint wordlen)
{
  guint offset = 0;
  guint bytelen = wordlen * 4;
  guint paddingcount = 0;

  while (offset + 2 < bytelen) {
    guint8 read_id, read_len;

    read_id = GST_READ_UINT8 (pdata + offset);
    offset += 1;

    /* ID 0 means its padding, skip */
    if (read_id == 0) {
      paddingcount++;
      continue;
    }

    paddingcount = 0;

    read_len = GST_READ_UINT8 (pdata + offset);
    offset += 1;

    /* Ignore extension headers where the size does not fit */
    if (offset + read_len > bytelen)
      return 0;

    offset += read_len;
  }

  return offset - paddingcount;
}

/**
 * gst_rtp_buffer_add_extension_twobytes_header:
 * @rtp: the RTP packet
 * @appbits: Application specific bits
 * @id: The ID of the header extension
 * @data: (array length=size) (element-type guint8): location for data
 * @size: the size of the data in bytes
 *
 * Adds a RFC 5285 header extension with a two bytes header to the end of the
 * RTP header. If there is already a RFC 5285 header extension with a two bytes
 * header, the new extension will be appended.
 * It will not work if there is already a header extension that does not follow
 * the mechanism described in RFC 5285 or if there is a header extension with
 * a one byte header as described in RFC 5285. In that case, use
 * gst_rtp_buffer_add_extension_onebyte_header()
 *
 * Returns: %TRUE if header extension could be added
 */

gboolean
gst_rtp_buffer_add_extension_twobytes_header (GstRTPBuffer * rtp,
    guint8 appbits, guint8 id, gconstpointer data, guint size)
{
  guint16 bits;
  guint8 *pdata = 0;
  guint wordlen;
  gboolean has_bit;
  gulong offset = 0;
  guint extlen;

  g_return_val_if_fail ((appbits & 0xF0) == 0, FALSE);
  g_return_val_if_fail (size < 256, FALSE);
  g_return_val_if_fail (gst_buffer_is_writable (rtp->buffer), FALSE);

  has_bit = gst_rtp_buffer_get_extension_data (rtp, &bits,
      (gpointer) & pdata, &wordlen);

  if (has_bit) {
    if (bits != ((0x100 << 4) | (appbits & 0x0f)))
      return FALSE;

    offset = get_twobytes_header_end_offset (pdata, wordlen);
    if (offset == 0)
      return FALSE;
  }

  /* the required size of the new extension data */
  extlen = offset + size + 2;
  /* calculate amount of words */
  wordlen = extlen / 4 + ((extlen % 4) ? 1 : 0);

  gst_rtp_buffer_set_extension_data (rtp, (0x100 << 4) | (appbits & 0x0F),
      wordlen);
  gst_rtp_buffer_get_extension_data (rtp, &bits, (gpointer) & pdata, &wordlen);

  pdata += offset;

  pdata[0] = id;
  pdata[1] = size;
  memcpy (pdata + 2, data, size);
  if (extlen % 4)
    memset (pdata + 2 + size, 0, 4 - (extlen % 4));

  return TRUE;
}
