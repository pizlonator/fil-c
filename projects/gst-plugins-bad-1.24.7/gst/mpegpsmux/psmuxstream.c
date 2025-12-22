/* MPEG-PS muxer plugin for GStreamer
 * Copyright 2008 Lin YANG <oxcsnicho@gmail.com>
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
/*
 * Unless otherwise indicated, Source Code is licensed under MIT license.
 * See further explanation attached in License Statement (distributed in the file
 * LICENSE).
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "psmuxcommon.h"
#include "psmuxstream.h"
#include "psmux.h"

static guint8 psmux_stream_pes_header_length (PsMuxStream * stream);
static void psmux_stream_write_pes_header (PsMuxStream * stream, guint8 * data);
static void psmux_stream_find_pts_dts_within (PsMuxStream * stream, guint bound,
    gint64 * pts, gint64 * dts);

/**
 * psmux_stream_new:
 * @pid: a PID
 * @stream_type: the stream type
 *
 * Create a new stream with type = @stream_type, assign stream id accordingly
 *
 * Returns: a new #PsMuxStream.
 */
PsMuxStream *
psmux_stream_new (PsMux * mux, PsMuxStreamType stream_type)
{
  PsMuxStream *stream = g_new0 (PsMuxStream, 1);
  PsMuxStreamIdInfo *info = &(mux->id_info);

  stream->stream_type = stream_type;
  stream->is_audio_stream = FALSE;
  stream->is_video_stream = FALSE;
  stream->stream_id = 0;
  stream->max_buffer_size = 0;

  switch (stream_type) {
      /* MPEG AUDIO */
    case PSMUX_ST_AUDIO_MPEG1:
    case PSMUX_ST_AUDIO_MPEG2:
      stream->max_buffer_size = 2484;   /* ISO/IEC 13818 2.5.2.4 */
    case PSMUX_ST_AUDIO_AAC:
      if (info->id_mpga > PSMUX_STREAM_ID_MPGA_MAX)
        break;
      stream->stream_id = info->id_mpga++;
      stream->stream_id_ext = 0;
      stream->is_audio_stream = TRUE;
      break;
      /* MPEG VIDEO */
    case PSMUX_ST_VIDEO_MPEG1:
    case PSMUX_ST_VIDEO_MPEG2:
    case PSMUX_ST_VIDEO_MPEG4:
    case PSMUX_ST_VIDEO_H264:
      if (info->id_mpgv > PSMUX_STREAM_ID_MPGV_MAX)
        break;
      stream->stream_id = info->id_mpgv++;
      stream->stream_id_ext = 0;
      stream->is_video_stream = TRUE;
      break;
      /* AC3 / A52 */
    case PSMUX_ST_PS_AUDIO_AC3:
      if (info->id_ac3 > PSMUX_STREAM_ID_AC3_MAX)
        break;
      stream->stream_id = PSMUX_PRIVATE_STREAM_1;
      stream->stream_id_ext = info->id_ac3++;
      stream->is_audio_stream = TRUE;
      /* AC3 requires data alignment */
      stream->pi.flags |= PSMUX_PACKET_FLAG_PES_DATA_ALIGN;
      break;
      /* TODO: SPU missing */
#if 0
    case spu:
      if (info->id_spu > PSMUX_STREAM_ID_SPU_MAX)
        break;
      return info->id_spu++;
#endif
      /* DTS */
    case PSMUX_ST_PS_AUDIO_DTS:
      if (info->id_dts > PSMUX_STREAM_ID_DTS_MAX)
        break;
      stream->stream_id = PSMUX_PRIVATE_STREAM_1;
      stream->stream_id_ext = info->id_dts++;
      stream->is_audio_stream = TRUE;
      break;
      /* LPCM */
    case PSMUX_ST_PS_AUDIO_LPCM:
      if (info->id_lpcm > PSMUX_STREAM_ID_LPCM_MAX)
        break;
      stream->stream_id = PSMUX_PRIVATE_STREAM_1;
      stream->stream_id_ext = info->id_lpcm++;
      stream->is_audio_stream = TRUE;
      break;
    case PSMUX_ST_VIDEO_DIRAC:
      if (info->id_dirac > PSMUX_STREAM_ID_DIRAC_MAX)
        break;
      stream->stream_id = PSMUX_EXTENDED_STREAM;
      stream->stream_id_ext = info->id_dirac++;
      stream->is_video_stream = TRUE;
      break;
    default:
      g_critical ("Stream type 0x%0x not yet implemented", stream_type);
      break;
  }

  if (stream->stream_id == 0) {
    g_critical ("Number of elementary streams of type %04x exceeds maximum",
        stream->stream_type);
    g_free (stream);
    return NULL;
  }

  /* XXX: Are private streams also using stream_id_ext? */
  if (stream->stream_id == PSMUX_EXTENDED_STREAM)
    stream->pi.flags |= PSMUX_PACKET_FLAG_PES_EXT_STREAMID;

  /* Are these useful at all? */
  if (stream->stream_id == PSMUX_PROGRAM_STREAM_MAP ||
      stream->stream_id == PSMUX_PADDING_STREAM ||
      stream->stream_id == PSMUX_PRIVATE_STREAM_2 ||
      stream->stream_id == PSMUX_ECM ||
      stream->stream_id == PSMUX_EMM ||
      stream->stream_id == PSMUX_PROGRAM_STREAM_DIRECTORY ||
      stream->stream_id == PSMUX_DSMCC_STREAM ||
      stream->stream_id == PSMUX_ITU_T_H222_1_TYPE_E)
    stream->pi.flags &= ~PSMUX_PACKET_FLAG_PES_FULL_HEADER;
  else
    stream->pi.flags |= PSMUX_PACKET_FLAG_PES_FULL_HEADER;

  stream->buffers = NULL;
  stream->bytes_avail = 0;
  stream->cur_buffer = NULL;
  stream->cur_buffer_consumed = 0;

  stream->cur_pes_payload_size = 0;

  stream->pts = -1;
  stream->dts = -1;
  stream->last_pts = -1;

  /* These fields are set by gstreamer */
  stream->audio_sampling = 0;
  stream->audio_channels = 0;
  stream->audio_bitrate = 0;

  if (stream->max_buffer_size == 0) {
    /* XXX: VLC'S VALUE. Better default? */
    if (stream->is_video_stream)
      stream->max_buffer_size = 400 * 1024;
    else if (stream->is_audio_stream)
      stream->max_buffer_size = 4 * 1024;
    else
      g_assert_not_reached ();
  }

  return stream;
}

/**
 * psmux_stream_free:
 * @stream: a #PsMuxStream
 *
 * Free the resources of @stream.
 */
void
psmux_stream_free (PsMuxStream * stream)
{
  g_return_if_fail (stream != NULL);

  if (psmux_stream_bytes_in_buffer (stream)) {
    g_warning ("Freeing stream with data not yet processed");
  }
  g_free (stream);
}

/* Advance the current packet stream position by len bytes.
 * Mustn't consume more than available in the current packet */
static void
psmux_stream_consume (PsMuxStream * stream, guint len)
{
  g_assert (stream->cur_buffer != NULL);
  g_assert (len <= stream->cur_buffer->map.size - stream->cur_buffer_consumed);

  stream->cur_buffer_consumed += len;
  stream->bytes_avail -= len;

  if (stream->cur_buffer_consumed == 0)
    return;

  if (stream->cur_buffer->pts != -1)
    stream->last_pts = stream->cur_buffer->pts;

  if (stream->cur_buffer_consumed == stream->cur_buffer->map.size) {
    /* Current packet is completed, move along */
    stream->buffers = g_list_delete_link (stream->buffers, stream->buffers);

    gst_buffer_unmap (stream->cur_buffer->buf, &stream->cur_buffer->map);
    gst_buffer_unref (stream->cur_buffer->buf);
    g_free (stream->cur_buffer);
    stream->cur_buffer = NULL;
  }
}


/**
 * psmux_stream_bytes_in_buffer:
 * @stream: a #PsMuxStream
 *
 * Calculate how much bytes are in the buffer.
 *
 * Returns: The number of bytes in the buffer.
 */
gint
psmux_stream_bytes_in_buffer (PsMuxStream * stream)
{
  g_return_val_if_fail (stream != NULL, 0);

  return stream->bytes_avail;
}

/**
 * psmux_stream_get_data:
 * @stream: a #PsMuxStream
 * @buf: a buffer to hold the result
 * @len: the length of @buf
 *
 * Write a PES packet to @buf, up to @len bytes
 *
 * Returns: number of bytes having been written, 0 if error
 */
guint
psmux_stream_get_data (PsMuxStream * stream, guint8 * buf, guint len)
{
  guint8 pes_hdr_length;
  guint w;

  g_return_val_if_fail (stream != NULL, FALSE);
  g_return_val_if_fail (buf != NULL, FALSE);
  g_return_val_if_fail (len >= PSMUX_PES_MAX_HDR_LEN, FALSE);

  stream->cur_pes_payload_size =
      MIN (psmux_stream_bytes_in_buffer (stream), len - PSMUX_PES_MAX_HDR_LEN);
  /* Note that we cannot make a better estimation of the header length for the
   * time being; because the header length is dependent on whether we can find a
   * timestamp in the upcoming buffers, which in turn depends on
   * cur_pes_payload_size, which is exactly what we want to decide.
   */

  psmux_stream_find_pts_dts_within (stream, stream->cur_pes_payload_size,
      &stream->pts, &stream->dts);

  /* clear pts/dts flag */
  stream->pi.flags &= ~(PSMUX_PACKET_FLAG_PES_WRITE_PTS_DTS |
      PSMUX_PACKET_FLAG_PES_WRITE_PTS);
  /* update pts/dts flag */
  if (stream->pts != -1 && stream->dts != -1)
    stream->pi.flags |= PSMUX_PACKET_FLAG_PES_WRITE_PTS_DTS;
  else {
    if (stream->pts != -1)
      stream->pi.flags |= PSMUX_PACKET_FLAG_PES_WRITE_PTS;
  }

  pes_hdr_length = psmux_stream_pes_header_length (stream);

  /* write pes header */
  GST_LOG ("Writing PES header of length %u and payload %d",
      pes_hdr_length, stream->cur_pes_payload_size);
  psmux_stream_write_pes_header (stream, buf);

  buf += pes_hdr_length;
  w = stream->cur_pes_payload_size;     /* number of bytes of payload to write */

  while (w > 0) {
    guint32 avail;
    guint8 *cur;

    if (stream->cur_buffer == NULL) {
      /* Start next packet */
      if (stream->buffers == NULL)
        return FALSE;
      stream->cur_buffer = (PsMuxStreamBuffer *) (stream->buffers->data);
      stream->cur_buffer_consumed = 0;
    }

    /* Take as much as we can from the current buffer */
    avail = stream->cur_buffer->map.size - stream->cur_buffer_consumed;
    cur = stream->cur_buffer->map.data + stream->cur_buffer_consumed;
    if (avail < w) {
      memcpy (buf, cur, avail);
      psmux_stream_consume (stream, avail);

      buf += avail;
      w -= avail;
    } else {
      memcpy (buf, cur, w);
      psmux_stream_consume (stream, w);

      w = 0;
    }
  }

  return pes_hdr_length + stream->cur_pes_payload_size;
}

static guint8
psmux_stream_pes_header_length (PsMuxStream * stream)
{
  guint8 packet_len;

  /* Calculate the length of the header for this stream */

  /* start_code prefix + stream_id + pes_packet_length = 6 bytes */
  packet_len = 6;

  if (stream->pi.flags & PSMUX_PACKET_FLAG_PES_FULL_HEADER) {
    /* For a PES 'full header' we have at least 3 more bytes, 
     * and then more based on flags */
    packet_len += 3;
    if (stream->pi.flags & PSMUX_PACKET_FLAG_PES_WRITE_PTS_DTS) {
      packet_len += 10;
    } else if (stream->pi.flags & PSMUX_PACKET_FLAG_PES_WRITE_PTS) {
      packet_len += 5;
    }
    if (stream->pi.flags & PSMUX_PACKET_FLAG_PES_EXT_STREAMID) {
      /* Need basic extension flags (1 byte), plus 2 more bytes for the 
       * length + extended stream id */
      packet_len += 3;
    }
  }

  return packet_len;
}

/* Find a PTS/DTS to write into the pes header within the next bound bytes
 * of the data */
static void
psmux_stream_find_pts_dts_within (PsMuxStream * stream, guint bound,
    gint64 * pts, gint64 * dts)
{
  /* 1. When there are at least one buffer then output the first buffer with
   * pts/dts
   * 2. If the bound is too small to include even one buffer, output the pts/dts
   * of that buffer.
   */
  GList *cur;

  *pts = -1;
  *dts = -1;

  for (cur = g_list_first (stream->buffers); cur != NULL;
      cur = g_list_next (cur)) {
    PsMuxStreamBuffer *curbuf = cur->data;

    /* FIXME: This isn't quite correct - if the 'bound' is within this
     * buffer, we don't know if the timestamp is before or after the split
     * so we shouldn't return it */
    if (bound <= curbuf->map.size) {
      *pts = curbuf->pts;
      *dts = curbuf->dts;
      return;
    }

    /* Have we found a buffer with pts/dts set? */
    if (curbuf->pts != -1 || curbuf->dts != -1) {
      *pts = curbuf->pts;
      *dts = curbuf->dts;
      return;
    }

    bound -= curbuf->map.size;
  }
}

static void
psmux_stream_write_pes_header (PsMuxStream * stream, guint8 * data)
{
  guint16 length_to_write;
  guint8 hdr_len = psmux_stream_pes_header_length (stream);

  /* start_code prefix + stream_id + pes_packet_length = 6 bytes */
  data[0] = 0x00;
  data[1] = 0x00;
  data[2] = 0x01;
  data[3] = stream->stream_id;
  data += 4;

  length_to_write = hdr_len - 6 + stream->cur_pes_payload_size;

  psmux_put16 (&data, length_to_write);

  if (stream->pi.flags & PSMUX_PACKET_FLAG_PES_FULL_HEADER) {
    guint8 flags = 0;

    /* Not scrambled, original, not-copyrighted, data_alignment specified by flag */
    flags = 0x81;
    if (stream->pi.flags & PSMUX_PACKET_FLAG_PES_DATA_ALIGN)
      flags |= 0x04;            /* Enable data_alignment_indicator */
    *data++ = flags;

    /* Flags */
    flags = 0;
    if (stream->pi.flags & PSMUX_PACKET_FLAG_PES_WRITE_PTS_DTS)
      flags |= 0xC0;
    else if (stream->pi.flags & PSMUX_PACKET_FLAG_PES_WRITE_PTS)
      flags |= 0x80;
    if (stream->pi.flags & PSMUX_PACKET_FLAG_PES_EXT_STREAMID)
      flags |= 0x01;            /* Enable PES_extension_flag */
    *data++ = flags;

    /* Header length is the total pes length, 
     * minus the 9 bytes of start codes, flags + hdr_len */
    g_return_if_fail (hdr_len >= 9);
    *data++ = (hdr_len - 9);

    if (stream->pi.flags & PSMUX_PACKET_FLAG_PES_WRITE_PTS_DTS) {
      psmux_put_ts (&data, 0x3, stream->pts);
      psmux_put_ts (&data, 0x1, stream->dts);
    } else if (stream->pi.flags & PSMUX_PACKET_FLAG_PES_WRITE_PTS) {
      psmux_put_ts (&data, 0x2, stream->pts);
    }

    if (stream->pi.flags & PSMUX_PACKET_FLAG_PES_EXT_STREAMID) {
      guint8 ext_len;

      flags = 0x0f;             /* preceding flags all 0 | (reserved bits) | PES_extension_flag_2 */
      *data++ = flags;

      ext_len = 1;              /* Only writing 1 byte into the extended fields */
      *data++ = 0x80 | ext_len; /* marker | PES_extension_field_length */
      *data++ = 0x80 | stream->stream_id_ext;   /* stream_id_extension_flag | extended_stream_id */
    }
  }
}

/**
 * psmux_stream_add_data:
 * @stream: a #PsMuxStream
 * @buffer: (transfer full): buffer with data to add
 * @pts: PTS of access unit in @data
 * @dts: DTS of access unit in @data
 *
 * Submit @len bytes of @data into @stream. @pts and @dts can be set to the
 * timestamp (against a 90Hz clock) of the first access unit in @data. A
 * timestamp of -1 for @pts or @dts means unknown.
 *
 * This function takes ownership of @buffer.
 */
void
psmux_stream_add_data (PsMuxStream * stream, GstBuffer * buffer,
    gint64 pts, gint64 dts, gboolean keyunit)
{
  PsMuxStreamBuffer *packet;

  g_return_if_fail (stream != NULL);

  packet = g_new (PsMuxStreamBuffer, 1);
  packet->buf = buffer;

  if (!gst_buffer_map (packet->buf, &packet->map, GST_MAP_READ)) {
    GST_ERROR ("Failed to map buffer for reading");
    gst_buffer_unref (packet->buf);
    g_free (packet);
    return;
  }

  packet->keyunit = keyunit;
  packet->pts = pts;
  packet->dts = dts;

  if (stream->bytes_avail == 0)
    stream->last_pts = pts;

  stream->bytes_avail += packet->map.size;
  /* FIXME: perhaps use GstQueueArray instead? */
  stream->buffers = g_list_append (stream->buffers, packet);

}

/**
 * psmux_stream_get_es_descrs:
 * @stream: a #PsMuxStream
 * @buf: a buffer to hold the ES descriptor
 * @len: the length used in @buf
 *
 * Write an Elementary Stream Descriptor for @stream into @buf. the number of
 * bytes consumed in @buf will be updated in @len.
 *
 * @buf and @len must be at least #PSMUX_MIN_ES_DESC_LEN.
 */
void
psmux_stream_get_es_descrs (PsMuxStream * stream, guint8 * buf, guint16 * len)
{
  guint8 *pos;

  g_return_if_fail (stream != NULL);

  if (buf == NULL) {
    if (len != NULL)
      *len = 0;
    return;
  }

  /* Based on the stream type, write out any descriptors to go in the 
   * PMT ES_info field */
  pos = buf;

  /* tag (registration_descriptor), length, format_identifier */
  switch (stream->stream_type) {
    case PSMUX_ST_AUDIO_AAC:
      /* FIXME */
      break;
    case PSMUX_ST_VIDEO_MPEG4:
      /* FIXME */
      break;
    case PSMUX_ST_VIDEO_H264:
      *pos++ = 0x05;
      *pos++ = 8;
      *pos++ = 0x48;            /* 'H' */
      *pos++ = 0x44;            /* 'D' */
      *pos++ = 0x4D;            /* 'M' */
      *pos++ = 0x56;            /* 'V' */
      /* FIXME : Not sure about this additional_identification_info */
      *pos++ = 0xFF;
      *pos++ = 0x1B;
      *pos++ = 0x44;
      *pos++ = 0x3F;
      break;
    case PSMUX_ST_VIDEO_DIRAC:
      *pos++ = 0x05;
      *pos++ = 4;
      *pos++ = 0x64;            /* 'd' */
      *pos++ = 0x72;            /* 'r' */
      *pos++ = 0x61;            /* 'a' */
      *pos++ = 0x63;            /* 'c' */
      break;
    case PSMUX_ST_PS_AUDIO_AC3:
    {
      *pos++ = 0x05;
      *pos++ = 4;
      *pos++ = 0x41;            /* 'A' */
      *pos++ = 0x43;            /* 'C' */
      *pos++ = 0x2D;            /* '-' */
      *pos++ = 0x33;            /* '3' */

      /* audio_stream_descriptor () | ATSC A/52-2001 Annex A
       *
       * descriptor_tag       8 uimsbf
       * descriptor_length    8 uimsbf
       * sample_rate_code     3 bslbf
       * bsid                 5 bslbf
       * bit_rate_code        6 bslbf
       * surround_mode        2 bslbf
       * bsmod                3 bslbf
       * num_channels         4 bslbf
       * full_svc             1 bslbf
       * langcod              8 bslbf
       * [...]
       */
      *pos++ = 0x81;
      *pos++ = 0x04;

      /* 3 bits sample_rate_code, 5 bits hardcoded bsid (default ver 8) */
      switch (stream->audio_sampling) {
        case 48000:
          *pos++ = 0x08;
          break;
        case 44100:
          *pos++ = 0x28;
          break;
        case 32000:
          *pos++ = 0x48;
          break;
        default:
          *pos++ = 0xE8;
          break;                /* 48, 44.1 or 32 Khz */
      }

      /* 1 bit bit_rate_limit, 5 bits bit_rate_code, 2 bits suround_mode */
      switch (stream->audio_bitrate) {
        case 32:
          *pos++ = 0x00 << 2;
          break;
        case 40:
          *pos++ = 0x01 << 2;
          break;
        case 48:
          *pos++ = 0x02 << 2;
          break;
        case 56:
          *pos++ = 0x03 << 2;
          break;
        case 64:
          *pos++ = 0x04 << 2;
          break;
        case 80:
          *pos++ = 0x05 << 2;
          break;
        case 96:
          *pos++ = 0x06 << 2;
          break;
        case 112:
          *pos++ = 0x07 << 2;
          break;
        case 128:
          *pos++ = 0x08 << 2;
          break;
        case 160:
          *pos++ = 0x09 << 2;
          break;
        case 192:
          *pos++ = 0x0A << 2;
          break;
        case 224:
          *pos++ = 0x0B << 2;
          break;
        case 256:
          *pos++ = 0x0C << 2;
          break;
        case 320:
          *pos++ = 0x0D << 2;
          break;
        case 384:
          *pos++ = 0x0E << 2;
          break;
        case 448:
          *pos++ = 0x0F << 2;
          break;
        case 512:
          *pos++ = 0x10 << 2;
          break;
        case 576:
          *pos++ = 0x11 << 2;
          break;
        case 640:
          *pos++ = 0x12 << 2;
          break;
        default:
          *pos++ = 0x32 << 2;
          break;                /* 640 Kb/s upper limit */
      }

      /* 3 bits bsmod, 4 bits num_channels, 1 bit full_svc */
      switch (stream->audio_channels) {
        case 1:
          *pos++ = 0x01 << 1;
          break;                /* 1/0 */
        case 2:
          *pos++ = 0x02 << 1;
          break;                /* 2/0 */
        case 3:
          *pos++ = 0x0A << 1;
          break;                /* <= 3 */
        case 4:
          *pos++ = 0x0B << 1;
          break;                /* <= 4 */
        case 5:
          *pos++ = 0x0C << 1;
          break;                /* <= 5 */
        case 6:
        default:
          *pos++ = 0x0D << 1;
          break;                /* <= 6 */
      }

      *pos++ = 0x00;

      break;
    }
    case PSMUX_ST_PS_AUDIO_DTS:
      /* FIXME */
      break;
    case PSMUX_ST_PS_AUDIO_LPCM:
      /* FIXME */
      break;
    default:
      break;
  }

  if (len)
    *len = (pos - buf);
}

/**
 * psmux_stream_get_pts:
 * @stream: a #PsMuxStream
 *
 * Return the PTS of the last buffer that has had bytes written and
 * which _had_ a PTS in @stream.
 *
 * Returns: the PTS of the last buffer in @stream.
 */
guint64
psmux_stream_get_pts (PsMuxStream * stream)
{
  g_return_val_if_fail (stream != NULL, -1);

  return stream->last_pts;
}
