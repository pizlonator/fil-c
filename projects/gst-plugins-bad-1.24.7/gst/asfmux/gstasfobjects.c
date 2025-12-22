/* ASF muxer plugin for GStreamer
 * Copyright (C) 2009 Thiago Santos <thiagoss@embedded.ufcg.edu.br>
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

#include "gstasfobjects.h"
#include <string.h>

/* Guids */
const Guid guids[] = {
  /* asf header object */
  {0x75B22630, 0x668E, 0x11CF, G_GUINT64_CONSTANT (0xA6D900AA0062CE6C)},
  /* asf file properties object */
  {0x8CABDCA1, 0xA947, 0x11CF, G_GUINT64_CONSTANT (0x8EE400C00C205365)},
  /* asf stream properties object */
  {0xB7DC0791, 0xA9B7, 0x11CF, G_GUINT64_CONSTANT (0x8EE600C00C205365)},
  /* asf audio media */
  {0xF8699E40, 0x5B4D, 0x11CF, G_GUINT64_CONSTANT (0xA8FD00805F5C442B)},
  /* asf no error correction */
  {0x20FB5700, 0x5B55, 0x11CF, G_GUINT64_CONSTANT (0xA8FD00805F5C442B)},
  /* asf audio spread */
  {0xBFC3CD50, 0x618F, 0x11CF, G_GUINT64_CONSTANT (0x8BB200AA00B4E220)},
  /* asf header extension object */
  {0x5FBF03B5, 0xA92E, 0x11CF, G_GUINT64_CONSTANT (0x8EE300C00C205365)},
  /* asf reserved 1 */
  {0xABD3D211, 0xA9BA, 0x11CF, G_GUINT64_CONSTANT (0x8EE600C00C205365)},
  /* asf data object */
  {0x75B22636, 0x668E, 0x11CF, G_GUINT64_CONSTANT (0xA6D900AA0062CE6C)},
  /* asf extended stream properties object */
  {0x14E6A5CB, 0xC672, 0x4332, G_GUINT64_CONSTANT (0x8399A96952065B5A)},
  /* asf video media */
  {0xBC19EFC0, 0x5B4D, 0x11CF, G_GUINT64_CONSTANT (0xA8FD00805F5C442B)},
  /* asf simple index object */
  {0x33000890, 0xE5B1, 0x11CF, G_GUINT64_CONSTANT (0x89F400A0C90349CB)},
  /* asf content description */
  {0x75B22633, 0x668E, 0x11CF, G_GUINT64_CONSTANT (0xA6D900AA0062CE6C)},
  /* asf extended content description */
  {0xD2D0A440, 0xE307, 0x11D2, G_GUINT64_CONSTANT (0x97F000A0C95EA850)},
  /* asf metadata object */
  {0xC5F8CBEA, 0x5BAF, 0x4877, G_GUINT64_CONSTANT (0x8467AA8C44FA4CCA)},
  /* asf padding object */
  {0x1806D474, 0xCADF, 0x4509, G_GUINT64_CONSTANT (0xA4BA9AABCB96AAE8)}
};

/**
 * gst_asf_generate_file_id:
 *
 * Generates a random GUID
 *
 * Returns: The generated GUID
 */
void
gst_asf_generate_file_id (Guid * guid)
{
  guint32 aux;

  guid->v1 = g_random_int ();
  aux = g_random_int ();
  guid->v2 = (guint16) (aux & 0x0000FFFF);
  guid->v3 = (guint16) (aux >> 16);
  guid->v4 = (((guint64) g_random_int ()) << 32) | (guint64) g_random_int ();
}

/**
 * gst_byte_reader_get_asf_var_size_field:
 * @reader: A #GstByteReader
 * @field_type: an asf field type
 * @var: pointer to store the result
 *
 * Reads the proper data from the #GstByteReader according to the
 * asf field type and stores it in var
 *
 * Returns: True on success, false otherwise
 */
gboolean
gst_byte_reader_get_asf_var_size_field (GstByteReader * reader,
    guint8 field_type, guint32 * var)
{
  guint8 aux8 = 0;
  guint16 aux16 = 0;
  guint32 aux32 = 0;
  gboolean ret;

  switch (field_type) {
    case ASF_FIELD_TYPE_DWORD:
      ret = gst_byte_reader_get_uint32_le (reader, &aux32);
      *var = aux32;
      break;
    case ASF_FIELD_TYPE_WORD:
      ret = gst_byte_reader_get_uint16_le (reader, &aux16);
      *var = aux16;
      break;
    case ASF_FIELD_TYPE_BYTE:
      ret = gst_byte_reader_get_uint8 (reader, &aux8);
      *var = aux8;
      break;
    case ASF_FIELD_TYPE_NONE:
      ret = TRUE;
      *var = 0;
      break;
    default:
      return FALSE;
  }
  return ret;
}

/**
 * gst_asf_read_var_size_field:
 * @data: pointer to the data to be read
 * @field_type: the asf field type pointed by data
 *
 * Reads and returns the value read from the data, according to the
 * field type given
 *
 * Returns: The value read
 */
guint32
gst_asf_read_var_size_field (guint8 * data, guint8 field_type)
{
  switch (field_type) {
    case ASF_FIELD_TYPE_DWORD:
      return GST_READ_UINT32_LE (data);
    case ASF_FIELD_TYPE_WORD:
      return GST_READ_UINT16_LE (data);
    case ASF_FIELD_TYPE_BYTE:
      return data[0];
    default:
      return 0;
  }
}

/**
 * gst_asf_get_var_size_field_len:
 * @field_type: the asf field type
 *
 * Returns: the size in bytes of a variable of field_type type
 */
guint
gst_asf_get_var_size_field_len (guint8 field_type)
{
  switch (field_type) {
    case ASF_FIELD_TYPE_DWORD:
      return 4;
    case ASF_FIELD_TYPE_WORD:
      return 2;
    case ASF_FIELD_TYPE_BYTE:
      return 1;
    default:
      return 0;
  }
}

/**
 * gst_asf_file_info_new:
 *
 * Creates a new #GstAsfFileInfo
 *
 * Returns: the created struct
 */
GstAsfFileInfo *
gst_asf_file_info_new (void)
{
  return g_new0 (GstAsfFileInfo, 1);
}

/**
 * gst_asf_file_info_reset:
 * @info: the #GstAsfFileInfo to be reset
 *
 * resets the data of a #GstFileInfo
 */
void
gst_asf_file_info_reset (GstAsfFileInfo * info)
{
  info->packet_size = 0;
  info->packets_count = 0;
  info->broadcast = FALSE;
}

/**
 * gst_asf_file_info_free:
 * @info: the #GstAsfFileInfo to be freed
 *
 * Releases memory associated with this #GstAsfFileInfo
 */
void
gst_asf_file_info_free (GstAsfFileInfo * info)
{
  g_free (info);
}

/**
 * gst_asf_payload_get_size:
 * @payload: the payload to get the size from
 *
 * Returns: the size of an asf payload of the data represented by this
 * #AsfPayload
 */
guint32
gst_asf_payload_get_size (AsfPayload * payload)
{
  return ASF_MULTIPLE_PAYLOAD_HEADER_SIZE + gst_buffer_get_size (payload->data);
}

/**
 * gst_asf_payload_free:
 * @payload: the #AsfPayload to be freed
 *
 * Releases the memory associated with this payload
 */
void
gst_asf_payload_free (AsfPayload * payload)
{
  gst_buffer_unref (payload->data);
  g_free (payload);
}

/**
 * gst_asf_get_current_time:
 *
 * Gets system current time in ASF time unit
 * (100-nanoseconds since Jan, 1st 1601)
 *
 * Returns:
 */
guint64
gst_asf_get_current_time (void)
{
  gint64 now;
  guint64 secs;
  guint64 usecs;

  now = g_get_real_time ();

  secs = (guint64) now / G_USEC_PER_SEC;
  usecs = (guint64) now % G_USEC_PER_SEC;
  return secs * G_GUINT64_CONSTANT (10000000) + usecs * 10
      + G_GUINT64_CONSTANT (116444628000000000);
}

/**
 * gst_asf_match_guid:
 * @data: pointer to the guid to be tested
 * @guid: guid to match against data
 *
 * Checks if the guid pointed by data is the same
 * as the guid parameter
 *
 * Returns: True if they are the same, false otherwise
 */
gboolean
gst_asf_match_guid (const guint8 * data, const Guid * guid)
{
  Guid g;
  g.v1 = GST_READ_UINT32_LE (data);
  g.v2 = GST_READ_UINT16_LE (data + 4);
  g.v3 = GST_READ_UINT16_LE (data + 6);
  g.v4 = GST_READ_UINT64_BE (data + 8);

  return g.v1 == guid->v1 &&
      g.v2 == guid->v2 && g.v3 == guid->v3 && g.v4 == guid->v4;
}

/**
 * gst_asf_put_i32:
 * @buf: the memory to write data to
 * @data: the value to be written
 *
 * Writes a 32 bit signed integer to memory
 */
void
gst_asf_put_i32 (guint8 * buf, gint32 data)
{
  GST_WRITE_UINT32_LE (buf, (guint32) data);
}

/**
 * gst_asf_put_time:
 * @buf: pointer to the buffer to write the value to
 * @time: value to be written
 *
 * Writes an asf time value to the buffer
 */
void
gst_asf_put_time (guint8 * buf, guint64 time)
{
  GST_WRITE_UINT64_LE (buf, time);
}

/**
 * gst_asf_put_guid:
 * @buf: the buffer to write the guid to
 * @guid: the guid to be written
 *
 * Writes a GUID to the buffer
 */
void
gst_asf_put_guid (guint8 * buf, Guid guid)
{
  GST_WRITE_UINT32_LE (buf + 0, guid.v1);
  GST_WRITE_UINT16_LE (buf + 4, guid.v2);
  GST_WRITE_UINT16_LE (buf + 6, guid.v3);
  GST_WRITE_UINT64_BE (buf + 8, guid.v4);
}

/**
 * gst_asf_put_payload:
 * @buf: memory to write the payload to
 * @payload: #AsfPayload to be written
 *
 * Writes the asf payload to the buffer. The #AsfPayload
 * packet count is incremented.
 */
void
gst_asf_put_payload (guint8 * buf, AsfPayload * payload)
{
  GST_WRITE_UINT8 (buf, payload->stream_number);
  GST_WRITE_UINT8 (buf + 1, payload->media_obj_num);
  GST_WRITE_UINT32_LE (buf + 2, payload->offset_in_media_obj);
  GST_WRITE_UINT8 (buf + 6, payload->replicated_data_length);
  GST_WRITE_UINT32_LE (buf + 7, payload->media_object_size);
  GST_WRITE_UINT32_LE (buf + 11, payload->presentation_time);
  GST_WRITE_UINT16_LE (buf + 15, (guint16) gst_buffer_get_size (payload->data));
  gst_buffer_extract (payload->data, 0, buf + 17,
      gst_buffer_get_size (payload->data));

  payload->packet_count++;
}

/**
 * gst_asf_put_subpayload:
 * @buf: buffer to write the payload to
 * @payload: the payload to be written
 * @size: maximum size in bytes to write
 *
 * Serializes part of a payload to a buffer.
 * The maximum size is checked against the payload length,
 * the minimum of this size and the payload length is written
 * to the buffer and the written size is returned.
 *
 * It also updates the values of the payload to match the remaining
 * data.
 * In case there is not enough space to write the headers, nothing is done.
 *
 * Returns: The written size in bytes.
 */
guint16
gst_asf_put_subpayload (guint8 * buf, AsfPayload * payload, guint16 size)
{
  guint16 payload_size;
  GstBuffer *newbuf;
  if (size <= ASF_MULTIPLE_PAYLOAD_HEADER_SIZE) {
    return 0;                   /* do nothing if there is not enough space */
  }
  GST_WRITE_UINT8 (buf, payload->stream_number);
  GST_WRITE_UINT8 (buf + 1, payload->media_obj_num);
  GST_WRITE_UINT32_LE (buf + 2, payload->offset_in_media_obj);
  GST_WRITE_UINT8 (buf + 6, payload->replicated_data_length);
  GST_WRITE_UINT32_LE (buf + 7, payload->media_object_size);
  GST_WRITE_UINT32_LE (buf + 11, payload->presentation_time);
  size -= ASF_MULTIPLE_PAYLOAD_HEADER_SIZE;
  payload_size = size < gst_buffer_get_size (payload->data) ?
      size : gst_buffer_get_size (payload->data);
  GST_WRITE_UINT16_LE (buf + 15, payload_size);
  gst_buffer_extract (payload->data, 0, buf + 17, payload_size);

  /* updates the payload to the remaining data */
  payload->offset_in_media_obj += payload_size;
  newbuf = gst_buffer_copy_region (payload->data, GST_BUFFER_COPY_ALL,
      payload_size, gst_buffer_get_size (payload->data) - payload_size);
  GST_BUFFER_TIMESTAMP (newbuf) = GST_BUFFER_TIMESTAMP (payload->data);
  gst_buffer_unref (payload->data);
  payload->data = newbuf;

  payload->packet_count++;

  return payload_size;
}

/**
 * gst_asf_match_and_peek_obj_size:
 * @data: data to be peeked at
 * @guid: pointer to a guid
 *
 * Compares the first bytes of data against the guid parameter and
 * if they match gets the object size (that are right after the guid in
 * asf objects).
 *
 * In case the guids don't match, 0 is returned.
 * If the guid is NULL the match is assumed to be true.
 *
 * Returns: The size of the object in case the guid matches, 0 otherwise
 */
guint64
gst_asf_match_and_peek_obj_size (const guint8 * data, const Guid * guid)
{
  g_assert (data);
  if (guid && !gst_asf_match_guid (data, guid)) {
    /* this is not the expected object */
    return 0;
  }
  /* return the object size */
  return GST_READ_UINT64_LE (data + ASF_GUID_SIZE);
}

/**
 * gst_asf_match_and_peek_obj_size_buf:
 * @buf: buffer to be peeked at
 * @guid: pointer to a guid
 *
 * Compares the first bytes of buf against the guid parameter and
 * if they match gets the object size (that are right after the guid in
 * asf objects).
 *
 * In case the guids don't match, 0 is returned.
 * If the guid is NULL the match is assumed to be true.
 *
 * Returns: The size of the object in case the guid matches, 0 otherwise
 */
guint64
gst_asf_match_and_peek_obj_size_buf (GstBuffer * buf, const Guid * guid)
{
  GstMapInfo map;
  guint64 res;

  gst_buffer_map (buf, &map, GST_MAP_READ);
  res = gst_asf_match_and_peek_obj_size (map.data, guid);
  gst_buffer_unmap (buf, &map);

  return res;
}

/**
 * gst_asf_parse_mult_payload:
 * @reader: a #GstByteReader ready to read the multiple payload data
 * @has_keyframe: pointer to return the result
 *
 * Parses a multiple payload section of an asf data packet
 * to see if any of the paylaods has a a keyframe
 *
 * Notice that the #GstByteReader might not be positioned after
 * this section on this function return. Because this section
 * is the last one in an asf packet and the remaining data
 * is probably uninteresting to the application.
 *
 * Returns: true on success, false if some error occurs
 */
static gboolean
gst_asf_parse_mult_payload (GstByteReader * reader, gboolean * has_keyframe)
{
  guint payloads;
  guint8 payload_len_type;
  guint8 rep_data_len = 0;
  guint32 payload_len;
  guint8 stream_num = 0;
  guint8 aux = 0;
  guint i;

  if (!gst_byte_reader_get_uint8 (reader, &aux))
    return FALSE;

  payloads = (aux & 0x3F);
  payload_len_type = (aux & 0xC0) >> 6;

  *has_keyframe = FALSE;
  for (i = 0; i < payloads; i++) {
    GST_LOG ("Parsing payload %u/%u", i + 1, payloads);
    if (!gst_byte_reader_get_uint8 (reader, &stream_num))
      goto error;
    if ((stream_num & 0x80) != 0) {
      GST_LOG ("Keyframe found, stopping parse of payloads");
      *has_keyframe = TRUE;
      return TRUE;
    }
    /* skip to replicated data length */
    if (!gst_byte_reader_skip (reader, 5))
      goto error;
    if (!gst_byte_reader_get_uint8 (reader, &rep_data_len))
      goto error;
    if (!gst_byte_reader_skip (reader, rep_data_len))
      goto error;
    if (!gst_byte_reader_get_asf_var_size_field (reader, payload_len_type,
            &payload_len))
      goto error;
    if (!gst_byte_reader_skip (reader, payload_len))
      goto error;
  }

  /* we do not skip the rest of the payload bytes as
     this is the last data to be parsed on the buffer */
  return TRUE;
error:
  GST_WARNING ("Error while parsing payloads");
  return FALSE;
}

/**
 * gst_asf_parse_single_payload:
 * @reader: a #GstByteReader ready to read the multiple payload data
 * @has_keyframe: pointer to return the result
 *
 * Parses a single payload section of an asf data packet
 * to see if any of the paylaods has a a keyframe
 *
 * Notice that the #GstByteReader might not be positioned after
 * this section on this function return. Because this section
 * is the last one in an asf packet and the remaining data
 * is probably uninteresting to the application.
 *
 * Returns: true on success, false if some error occurs
 */
static gboolean
gst_asf_parse_single_payload (GstByteReader * reader, gboolean * has_keyframe)
{
  guint8 stream_num = 0;
  if (!gst_byte_reader_get_uint8 (reader, &stream_num))
    return GST_FLOW_ERROR;
  *has_keyframe = (stream_num & 0x80) != 0;

  /* we do not skip the rest of the payload bytes as
     this is the last data to be parsed on the buffer */
  return TRUE;
}

gboolean
gst_asf_parse_packet (GstBuffer * buffer, GstAsfPacketInfo * packet,
    gboolean trust_delta_flag, guint packet_size)
{
  gboolean ret;
  GstMapInfo map;

  gst_buffer_map (buffer, &map, GST_MAP_READ);
  ret = gst_asf_parse_packet_from_data (map.data, map.size, buffer, packet,
      trust_delta_flag, packet_size);
  gst_buffer_unmap (buffer, &map);

  return ret;
}

gboolean
gst_asf_parse_packet_from_data (guint8 * data, gsize size, GstBuffer * buffer,
    GstAsfPacketInfo * packet, gboolean trust_delta_flag, guint packet_size)
{
/* Might be useful in future:
  guint8 rep_data_len_type;
  guint8 mo_number_len_type;
  guint8 mo_offset_type;
*/
  GstByteReader reader;
  gboolean ret = TRUE;
  guint8 first = 0;
  guint8 err_length = 0;        /* length of the error fields */
  guint8 aux = 0;
  guint8 packet_len_type;
  guint8 padding_len_type;
  guint8 seq_len_type;
  gboolean mult_payloads;
  guint32 packet_len;
  guint32 padd_len;
  guint32 send_time = 0;
  guint16 duration = 0;
  gboolean has_keyframe;

  if (packet_size != 0 && size != packet_size) {
    GST_WARNING ("ASF packets should be aligned with buffers");
    return FALSE;
  }

  gst_byte_reader_init (&reader, data, size);

  GST_LOG ("Starting packet parsing, size: %" G_GSIZE_FORMAT, size);
  if (!gst_byte_reader_get_uint8 (&reader, &first))
    goto error;

  if (first & 0x80) {           /* error correction present */
    guint8 err_cor_len;
    err_length += 1;
    GST_DEBUG ("Packet contains error correction");
    if (first & 0x60) {
      GST_ERROR ("Error correction data length should be "
          "set to 0 and is reserved for future use.");
      goto error;
    }
    err_cor_len = (first & 0x0F);
    err_length += err_cor_len;
    GST_DEBUG ("Error correction data length: %d", (gint) err_cor_len);
    if (!gst_byte_reader_skip (&reader, err_cor_len))
      goto error;

    /* put payload parsing info first byte in aux var */
    if (!gst_byte_reader_get_uint8 (&reader, &aux))
      goto error;
  } else {
    aux = first;
  }
  mult_payloads = (aux & 0x1) != 0;

  packet_len_type = (aux >> 5) & 0x3;
  padding_len_type = (aux >> 3) & 0x3;
  seq_len_type = (aux >> 1) & 0x3;
  GST_LOG ("Field sizes: packet length type: %u "
      ", padding length type: %u, sequence length type: %u",
      gst_asf_get_var_size_field_len (packet_len_type),
      gst_asf_get_var_size_field_len (padding_len_type),
      gst_asf_get_var_size_field_len (seq_len_type));

  if (mult_payloads) {
    GST_DEBUG ("Packet contains multiple payloads");
  }

  if (!gst_byte_reader_get_uint8 (&reader, &aux))
    goto error;

/*
  rep_data_len_type = aux & 0x3;
  mo_offset_type = (aux >> 2) & 0x3;
  mo_number_len_type = (aux >> 4) & 0x3;
*/

  /* gets the fields lengths */
  GST_LOG ("Getting packet and padding length");
  if (!gst_byte_reader_get_asf_var_size_field (&reader,
          packet_len_type, &packet_len))
    goto error;
  if (!gst_byte_reader_skip (&reader,
          gst_asf_get_var_size_field_len (seq_len_type)))
    goto error;
  if (!gst_byte_reader_get_asf_var_size_field (&reader,
          padding_len_type, &padd_len))
    goto error;

  /* some packet size validation */
  if (packet_size != 0 && packet_len_type != ASF_FIELD_TYPE_NONE) {
    if (padding_len_type != ASF_FIELD_TYPE_NONE &&
        packet_len + padd_len != packet_size) {
      GST_WARNING ("Packet size (payload=%u + padding=%u) doesn't "
          "match expected size %u", packet_len, padd_len, packet_size);
      ret = FALSE;
    }

    /* Be forgiving if packet_len has the full packet size
     * as the spec isn't really clear on its meaning.
     *
     * I had been taking it as the full packet size (fixed)
     * until bug #607555, that convinced me that it is more likely
     * the actual payloaded data size.
     */
    if (packet_len == packet_size) {
      GST_DEBUG ("This packet's length field represents the full "
          "packet and not the payloaded data length");
      ret = TRUE;
    }

    if (!ret)
      goto end;
  }

  GST_LOG ("Getting send time and duration");
  if (!gst_byte_reader_get_uint32_le (&reader, &send_time))
    goto error;
  if (!gst_byte_reader_get_uint16_le (&reader, &duration))
    goto error;

  has_keyframe = FALSE;
  GST_LOG ("Checking for keyframes");
  if (trust_delta_flag) {
    has_keyframe = GST_BUFFER_FLAG_IS_SET (buffer, GST_BUFFER_FLAG_DELTA_UNIT);
  } else {
    if (mult_payloads) {
      ret = gst_asf_parse_mult_payload (&reader, &has_keyframe);
    } else {
      ret = gst_asf_parse_single_payload (&reader, &has_keyframe);
    }
  }

  if (!ret) {
    GST_WARNING ("Failed to parse payloads");
    goto end;
  }
  GST_DEBUG ("Received packet of length %" G_GUINT32_FORMAT
      ", padding %" G_GUINT32_FORMAT ", send time %" G_GUINT32_FORMAT
      ", duration %" G_GUINT16_FORMAT " and %s keyframe(s)",
      packet_len, padd_len, send_time, duration,
      (has_keyframe) ? "with" : "without");

  packet->packet_size = packet_len;
  packet->padding = padd_len;
  packet->send_time = send_time;
  packet->duration = duration;
  packet->has_keyframe = has_keyframe;
  packet->multiple_payloads = mult_payloads ? TRUE : FALSE;
  packet->padd_field_type = padding_len_type;
  packet->packet_field_type = packet_len_type;
  packet->seq_field_type = seq_len_type;
  packet->err_cor_len = err_length;

  return ret;

error:
  ret = FALSE;
  GST_WARNING ("Error while parsing data packet");
end:
  return ret;
}

static gboolean
gst_asf_parse_file_properties_obj (GstByteReader * reader,
    GstAsfFileInfo * asfinfo)
{
  guint32 min_ps = 0;
  guint32 max_ps = 0;
  guint64 packets = 0;
  guint32 flags = 0;
  GST_DEBUG ("ASF: Parsing file properties object");

  /* skip until data packets count */
  if (!gst_byte_reader_skip (reader, 32))
    return FALSE;
  if (!gst_byte_reader_get_uint64_le (reader, &packets))
    return FALSE;
  asfinfo->packets_count = packets;
  GST_DEBUG ("ASF: packets count %" G_GUINT64_FORMAT, packets);

  /* skip until flags */
  if (!gst_byte_reader_skip (reader, 24))
    return FALSE;

  if (!gst_byte_reader_get_uint32_le (reader, &flags))
    return GST_FLOW_ERROR;
  asfinfo->broadcast = (flags & 0x1) == 1;
  GST_DEBUG ("ASF: broadcast flag: %s", asfinfo->broadcast ? "true" : "false");
  if (!gst_byte_reader_get_uint32_le (reader, &min_ps))
    return GST_FLOW_ERROR;
  if (!gst_byte_reader_get_uint32_le (reader, &max_ps))
    return GST_FLOW_ERROR;

  if (min_ps != max_ps) {
    GST_WARNING ("Minimum and maximum packet size differ "
        "%" G_GUINT32_FORMAT " and %" G_GUINT32_FORMAT ", "
        "ASF spec states they should be the same", min_ps, max_ps);
    return FALSE;
  }

  GST_DEBUG ("ASF: Packet size: %" G_GUINT32_FORMAT, min_ps);
  asfinfo->packet_size = min_ps;
  if (!gst_byte_reader_skip (reader, 4))
    return FALSE;

  return TRUE;
}

gboolean
gst_asf_parse_headers (GstBuffer * buffer, GstAsfFileInfo * file_info)
{
  GstMapInfo map;
  gboolean ret;

  gst_buffer_map (buffer, &map, GST_MAP_READ);
  ret = gst_asf_parse_headers_from_data (map.data, map.size, file_info);
  gst_buffer_unmap (buffer, &map);

  return ret;
}

gboolean
gst_asf_parse_headers_from_data (guint8 * data, guint size,
    GstAsfFileInfo * file_info)
{
  gboolean ret = TRUE;
  guint32 header_objects = 0;
  guint32 i;
  GstByteReader reader;
  guint64 object_size;

  object_size = gst_asf_match_and_peek_obj_size (data,
      &(guids[ASF_HEADER_OBJECT_INDEX]));
  if (object_size == 0) {
    GST_WARNING ("ASF: Cannot parse, header guid not found at the beginning "
        " of data");
    return FALSE;
  }

  gst_byte_reader_init (&reader, data, size);

  if (!gst_byte_reader_skip (&reader, ASF_GUID_OBJSIZE_SIZE))
    goto error;
  if (!gst_byte_reader_get_uint32_le (&reader, &header_objects))
    goto error;
  GST_DEBUG ("ASF: Header has %" G_GUINT32_FORMAT " child"
      " objects", header_objects);
  /* skip reserved bytes */
  if (!gst_byte_reader_skip (&reader, 2))
    goto error;

  /* iterate through childs of header object */
  for (i = 0; i < header_objects; i++) {
    const guint8 *guid = NULL;
    guint64 obj_size = 0;

    if (!gst_byte_reader_get_data (&reader, ASF_GUID_SIZE, &guid))
      goto error;
    if (!gst_byte_reader_get_uint64_le (&reader, &obj_size))
      goto error;

    if (gst_asf_match_guid (guid, &guids[ASF_FILE_PROPERTIES_OBJECT_INDEX])) {
      ret = gst_asf_parse_file_properties_obj (&reader, file_info);
    } else {
      /* we don't know/care about this object */
      if (!gst_byte_reader_skip (&reader, obj_size - ASF_GUID_OBJSIZE_SIZE))
        goto error;
    }

    if (!ret)
      goto end;
  }
  goto end;

error:
  ret = FALSE;
  GST_WARNING ("ASF: Error while parsing headers");
end:
  return ret;
}

#define MAP_GST_TO_ASF_TAG(tag, gst, asf) \
  if (strcmp (tag, gst) == 0) \
    return asf

/**
 * gst_asf_get_asf_tag:
 * @gsttag: a gstreamer tag
 *
 * Maps gstreamer tags to asf tags
 *
 * Returns: The tag corresponding name in asf files or NULL if it is not mapped
 */
const gchar *
gst_asf_get_asf_tag (const gchar * gsttag)
{
  g_return_val_if_fail (gsttag != NULL, NULL);

  MAP_GST_TO_ASF_TAG (gsttag, GST_TAG_TITLE, ASF_TAG_TITLE);
  MAP_GST_TO_ASF_TAG (gsttag, GST_TAG_TITLE_SORTNAME, ASF_TAG_TITLE_SORTNAME);
  MAP_GST_TO_ASF_TAG (gsttag, GST_TAG_ARTIST, ASF_TAG_ARTIST);
  MAP_GST_TO_ASF_TAG (gsttag, GST_TAG_ARTIST_SORTNAME, ASF_TAG_ARTIST_SORTNAME);
  MAP_GST_TO_ASF_TAG (gsttag, GST_TAG_ALBUM, ASF_TAG_ALBUM_TITLE);
  MAP_GST_TO_ASF_TAG (gsttag, GST_TAG_ALBUM_SORTNAME,
      ASF_TAG_ALBUM_TITLE_SORTNAME);
  MAP_GST_TO_ASF_TAG (gsttag, GST_TAG_GENRE, ASF_TAG_GENRE);
  MAP_GST_TO_ASF_TAG (gsttag, GST_TAG_COPYRIGHT, ASF_TAG_COPYRIGHT);
  MAP_GST_TO_ASF_TAG (gsttag, GST_TAG_COMPOSER, ASF_TAG_COMPOSER);
  MAP_GST_TO_ASF_TAG (gsttag, GST_TAG_COMMENT, ASF_TAG_COMMENT);
  MAP_GST_TO_ASF_TAG (gsttag, GST_TAG_TRACK_NUMBER, ASF_TAG_TRACK_NUMBER);

  return NULL;
}

guint
gst_asf_get_tag_field_type (GValue * value)
{
  if (G_VALUE_HOLDS_STRING (value))
    return ASF_TAG_TYPE_UNICODE_STR;
  if (G_VALUE_HOLDS_UINT (value))
    return ASF_TAG_TYPE_DWORD;

  return -1;
}

gboolean
gst_asf_tag_present_in_content_description (const gchar * tag)
{
  return strcmp (tag, GST_TAG_TITLE) == 0 ||
      strcmp (tag, GST_TAG_ARTIST) == 0 ||
      strcmp (tag, GST_TAG_COPYRIGHT) == 0 ||
      strcmp (tag, GST_TAG_DESCRIPTION) == 0;
  /* FIXME we have no tag for rating */
}
