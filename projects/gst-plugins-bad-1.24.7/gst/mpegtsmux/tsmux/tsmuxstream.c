/*
 * Copyright 2006 BBC and Fluendo S.A.
 *
 * This library is licensed under 3 different licenses and you
 * can choose to use it under the terms of any one of them. The
 * three licenses are the MPL 1.1, the LGPL, and the MIT license.
 *
 * MPL:
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/.
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * LGPL:
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
 *
 * MIT:
 *
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
 *
 * SPDX-License-Identifier: MPL-1.1 OR MIT OR LGPL-2.0-or-later
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include <gst/mpegts/mpegts.h>
#include <gst/base/gstbytewriter.h>

#include "tsmuxcommon.h"
#include "tsmuxstream.h"

#define GST_CAT_DEFAULT gst_base_ts_mux_debug

static guint8 tsmux_stream_pes_header_length (TsMuxStream * stream);
static void tsmux_stream_write_pes_header (TsMuxStream * stream, guint8 * data);
static void tsmux_stream_find_pts_dts_within (TsMuxStream * stream, guint bound,
    gint64 * pts, gint64 * dts);

struct TsMuxStreamBuffer
{
  guint8 *data;
  guint32 size;

  /* PTS & DTS associated with the contents of this buffer */
  gint64 pts;
  gint64 dts;

  /* data represents random access point */
  gboolean random_access;

  /* user_data for release function */
  void *user_data;
};

static void
tsmux_stream_get_es_descrs_default (TsMuxStream * stream,
    GstMpegtsPMTStream * pmt_stream, gpointer user_data)
{
  tsmux_stream_default_get_es_descrs (stream, pmt_stream);
}

/**
 * tsmux_stream_new:
 * @pid: a PID
 * @stream_type: the stream type
 * @stream_number: stream number
 *
 * Create a new stream with PID of @pid and @stream_type,
 * with stream number @stream_number.
 *
 * Returns: a new #TsMuxStream.
 */
TsMuxStream *
tsmux_stream_new (guint16 pid, guint stream_type, guint stream_number)
{
  TsMuxStream *stream = g_new0 (TsMuxStream, 1);
  gboolean supports_user_specified_stream_number = FALSE;

  stream->state = TSMUX_STREAM_STATE_HEADER;
  stream->pi.pid = pid;
  stream->stream_type = stream_type;

  stream->pes_payload_size = 0;
  stream->cur_pes_payload_size = 0;
  stream->pes_bytes_written = 0;
  stream->pmt_index = -1;

  switch (stream_type) {
    case TSMUX_ST_VIDEO_MPEG1:
    case TSMUX_ST_VIDEO_MPEG2:
    case TSMUX_ST_VIDEO_MPEG4:
    case TSMUX_ST_VIDEO_H264:
    case TSMUX_ST_VIDEO_HEVC:
      if (stream_number > 0xF) {
        GST_WARNING
            ("video stream number %d is greater than 0xF. Setting to 0.",
            stream_number);
        stream_number = 0;
      }
      stream->id = 0xE0 | stream_number;
      stream->pi.flags |= TSMUX_PACKET_FLAG_PES_FULL_HEADER;
      stream->is_video_stream = TRUE;
      supports_user_specified_stream_number = TRUE;
      break;
    case TSMUX_ST_VIDEO_JP2K:
      stream->id = 0xBD;
      stream->pi.flags |= TSMUX_PACKET_FLAG_PES_FULL_HEADER;
      stream->is_video_stream = TRUE;
      break;
    case TSMUX_ST_AUDIO_AAC:
    case TSMUX_ST_AUDIO_MPEG1:
    case TSMUX_ST_AUDIO_MPEG2:
      if (stream_number > 0x1F) {
        GST_WARNING
            ("audio stream number %d is greater than 0x1F. Setting to 0.",
            stream_number);
        stream_number = 0;
      }
      stream->is_audio = TRUE;
      stream->id = 0xC0 | stream_number;
      stream->pi.flags |= TSMUX_PACKET_FLAG_PES_FULL_HEADER;
      supports_user_specified_stream_number = TRUE;
      break;
    case TSMUX_ST_VIDEO_DIRAC:
    case TSMUX_ST_PS_AUDIO_LPCM:
    case TSMUX_ST_PS_AUDIO_AC3:
    case TSMUX_ST_PS_AUDIO_DTS:
      stream->id = 0xFD;
      /* FIXME: assign sequential extended IDs? */
      switch (stream_type) {
        case TSMUX_ST_VIDEO_DIRAC:
          stream->id_extended = 0x60;
          stream->is_video_stream = TRUE;
          break;
        case TSMUX_ST_PS_AUDIO_LPCM:
          stream->is_audio = TRUE;
          stream->id_extended = 0x80;
          break;
        case TSMUX_ST_PS_AUDIO_AC3:
          stream->is_audio = TRUE;
          stream->id_extended = 0x71;
          break;
        case TSMUX_ST_PS_AUDIO_DTS:
          stream->is_audio = TRUE;
          stream->id_extended = 0x82;
          break;
        default:
          break;
      }
      stream->pi.flags |=
          TSMUX_PACKET_FLAG_PES_FULL_HEADER |
          TSMUX_PACKET_FLAG_PES_EXT_STREAMID;
      break;
    case TSMUX_ST_PS_TELETEXT:
      /* needs fixes PES header length */
      stream->pi.pes_header_length = 36;
      /* fall through */
    case TSMUX_ST_PS_DVB_SUBPICTURE:
      /* private stream 1 */
      stream->id = 0xBD;
      stream->is_dvb_sub = TRUE;
      stream->stream_type = TSMUX_ST_PRIVATE_DATA;
      stream->pi.flags |=
          TSMUX_PACKET_FLAG_PES_FULL_HEADER |
          TSMUX_PACKET_FLAG_PES_DATA_ALIGNMENT;

      break;
    case TSMUX_ST_PS_KLV:
      /* FIXME: assign sequential extended IDs? */
      stream->id = 0xBD;
      stream->stream_type = TSMUX_ST_PRIVATE_DATA;
      stream->is_meta = TRUE;
      stream->pi.flags |=
          TSMUX_PACKET_FLAG_PES_FULL_HEADER |
          TSMUX_PACKET_FLAG_PES_DATA_ALIGNMENT;
      break;
    case TSMUX_ST_PS_OPUS:
      /* FIXME: assign sequential extended IDs? */
      stream->id = 0xBD;
      stream->is_audio = TRUE;
      stream->stream_type = TSMUX_ST_PRIVATE_DATA;
      stream->is_opus = TRUE;
      stream->pi.flags |= TSMUX_PACKET_FLAG_PES_FULL_HEADER;
      break;
    default:
      /* Might be a custom stream type implemented by a subclass */
      break;
  }

  if (!supports_user_specified_stream_number && stream_number != 0) {
    GST_WARNING
        ("Attempt to set stream number %d for unsupported stream type %d",
        stream_number, stream_type);
  }

  stream->first_ts = GST_CLOCK_STIME_NONE;

  stream->last_pts = GST_CLOCK_STIME_NONE;
  stream->last_dts = GST_CLOCK_STIME_NONE;

  stream->pcr_ref = 0;
  stream->next_pcr = -1;

  stream->get_es_descrs = tsmux_stream_get_es_descrs_default;
  stream->get_es_descrs_data = NULL;

  return stream;
}

/**
 * tsmux_stream_get_pid:
 * @stream: a #TsMuxStream
 *
 * Get the PID of @stream.
 *
 * Returns: The PID of @stream. 0xffff on error.
 */
guint16
tsmux_stream_get_pid (TsMuxStream * stream)
{
  g_return_val_if_fail (stream != NULL, G_MAXUINT16);

  return stream->pi.pid;
}

/**
 * tsmux_stream_free:
 * @stream: a #TsMuxStream
 *
 * Free the resources of @stream.
 */
void
tsmux_stream_free (TsMuxStream * stream)
{
  GList *cur;

  g_return_if_fail (stream != NULL);

  /* free buffers */
  for (cur = stream->buffers; cur; cur = cur->next) {
    TsMuxStreamBuffer *tmbuf = (TsMuxStreamBuffer *) cur->data;

    if (stream->buffer_release)
      stream->buffer_release (tmbuf->data, tmbuf->user_data);
    g_free (tmbuf);
  }
  g_list_free (stream->buffers);

  g_free (stream);
}

/**
 * tsmux_stream_set_buffer_release_func:
 * @stream: a #TsMuxStream
 * @func: the new #TsMuxStreamBufferReleaseFunc
 *
 * Set the function that will be called when a a piece of data fed to @stream
 * with tsmux_stream_add_data() can be freed. @func will be called with user
 * data as provided with the call to tsmux_stream_add_data().
 */
void
tsmux_stream_set_buffer_release_func (TsMuxStream * stream,
    TsMuxStreamBufferReleaseFunc func)
{
  g_return_if_fail (stream != NULL);

  stream->buffer_release = func;
}

/**
 * tsmux_stream_set_get_es_descriptors_func:
 * @stream: a #TsMuxStream
 * @func: a user callback function
 * @user_data: user data passed to @func
 *
 * Set the callback function and user data to be called when @stream has
 * to create Elementary Stream Descriptors.
 */
void
tsmux_stream_set_get_es_descriptors_func (TsMuxStream * stream,
    TsMuxStreamGetESDescriptorsFunc func, void *user_data)
{
  g_return_if_fail (stream != NULL);

  stream->get_es_descrs = func;
  stream->get_es_descrs_data = user_data;
}

/* Advance the current packet stream position by len bytes.
 * Mustn't consume more than available in the current packet */
static void
tsmux_stream_consume (TsMuxStream * stream, guint len)
{
  g_assert (stream->cur_buffer != NULL);
  g_assert (len <= stream->cur_buffer->size - stream->cur_buffer_consumed);

  stream->cur_buffer_consumed += len;
  stream->bytes_avail -= len;

  if (stream->cur_buffer_consumed == 0 && stream->cur_buffer->size != 0)
    return;

  if (GST_CLOCK_STIME_IS_VALID (stream->cur_buffer->pts))
    stream->last_pts = stream->cur_buffer->pts;
  if (GST_CLOCK_STIME_IS_VALID (stream->cur_buffer->dts))
    stream->last_dts = stream->cur_buffer->dts;

  if (stream->cur_buffer_consumed == stream->cur_buffer->size) {
    /* Current packet is completed, move along */
    stream->buffers = g_list_delete_link (stream->buffers, stream->buffers);

    if (stream->buffer_release) {
      stream->buffer_release (stream->cur_buffer->data,
          stream->cur_buffer->user_data);
    }

    g_free (stream->cur_buffer);
    stream->cur_buffer = NULL;
    /* FIXME: As a hack, for unbounded streams, start a new PES packet for each
     * incoming packet we receive. This assumes that incoming data is
     * packetised sensibly - ie, every video frame */
    if (stream->cur_pes_payload_size == 0) {
      stream->state = TSMUX_STREAM_STATE_HEADER;
      stream->pes_bytes_written = 0;
    }
  }
}

/**
 * tsmux_stream_at_pes_start:
 * @stream: a #TsMuxStream
 *
 * Check if @stream is at the start of a PES packet.
 *
 * Returns: TRUE if @stream is at a PES header packet.
 */
gboolean
tsmux_stream_at_pes_start (TsMuxStream * stream)
{
  g_return_val_if_fail (stream != NULL, FALSE);

  return stream->state == TSMUX_STREAM_STATE_HEADER;
}

/**
 * tsmux_stream_bytes_avail:
 * @stream: a #TsMuxStream
 *
 * Calculate how much bytes are available.
 *
 * Returns: The number of bytes available.
 */
static inline gint
_tsmux_stream_bytes_avail (TsMuxStream * stream)
{
  gint bytes_avail;

  g_return_val_if_fail (stream != NULL, 0);

  if (stream->cur_pes_payload_size != 0)
    bytes_avail = stream->cur_pes_payload_size - stream->pes_bytes_written;
  else
    bytes_avail = stream->bytes_avail;

  bytes_avail = MIN (bytes_avail, stream->bytes_avail);

  /* Calculate the number of bytes available in the current PES */
  if (stream->state == TSMUX_STREAM_STATE_HEADER)
    bytes_avail += tsmux_stream_pes_header_length (stream);

  return bytes_avail;
}

gint
tsmux_stream_bytes_avail (TsMuxStream * stream)
{
  g_return_val_if_fail (stream != NULL, 0);

  return _tsmux_stream_bytes_avail (stream);
}

/**
 * tsmux_stream_bytes_in_buffer:
 * @stream: a #TsMuxStream
 *
 * Calculate how much bytes are in the buffer.
 *
 * Returns: The number of bytes in the buffer.
 */
gint
tsmux_stream_bytes_in_buffer (TsMuxStream * stream)
{
  g_return_val_if_fail (stream != NULL, 0);

  return stream->bytes_avail;
}

/**
 * tsmux_stream_initialize_pes_packet:
 * @stream: a #TsMuxStream
 *
 * Initializes the PES packet.
 *
 * Returns: TRUE if we the packet was initialized.
 */
gboolean
tsmux_stream_initialize_pes_packet (TsMuxStream * stream)
{
  if (stream->state != TSMUX_STREAM_STATE_HEADER)
    return TRUE;

  if (stream->pes_payload_size != 0) {
    /* Use prescribed fixed PES payload size */
    stream->cur_pes_payload_size = stream->pes_payload_size;
    tsmux_stream_find_pts_dts_within (stream, stream->cur_pes_payload_size,
        &stream->pts, &stream->dts);
  } else {
    /* Output a PES packet of all currently available bytes otherwise */
    stream->cur_pes_payload_size = stream->bytes_avail;
    tsmux_stream_find_pts_dts_within (stream, stream->cur_pes_payload_size,
        &stream->pts, &stream->dts);
  }

  stream->pi.flags &= ~(TSMUX_PACKET_FLAG_PES_WRITE_PTS_DTS |
      TSMUX_PACKET_FLAG_PES_WRITE_PTS);

  if (GST_CLOCK_STIME_IS_VALID (stream->pts)
      && GST_CLOCK_STIME_IS_VALID (stream->dts)
      && stream->pts != stream->dts)
    stream->pi.flags |= TSMUX_PACKET_FLAG_PES_WRITE_PTS_DTS;
  else {
    if (GST_CLOCK_STIME_IS_VALID (stream->pts))
      stream->pi.flags |= TSMUX_PACKET_FLAG_PES_WRITE_PTS;
  }

  if (stream->buffers) {
    TsMuxStreamBuffer *buf = (TsMuxStreamBuffer *) (stream->buffers->data);
    if (buf->random_access) {
      stream->pi.flags |= TSMUX_PACKET_FLAG_RANDOM_ACCESS;
      stream->pi.flags |= TSMUX_PACKET_FLAG_ADAPTATION;
    }
  }

  if (stream->is_video_stream) {
    guint8 hdr_len;

    hdr_len = tsmux_stream_pes_header_length (stream);

    /* Unbounded for video streams if pes packet length is over 16 bit */
    if ((stream->cur_pes_payload_size + hdr_len - 6) > G_MAXUINT16)
      stream->cur_pes_payload_size = 0;
  }

  return TRUE;
}

/**
 * tsmux_stream_get_data:
 * @stream: a #TsMuxStream
 * @buf: a buffer to hold the result
 * @len: the length of @buf
 *
 * Copy up to @len available data in @stream into the buffer @buf.
 *
 * Returns: TRUE if @len bytes could be retrieved.
 */
gboolean
tsmux_stream_get_data (TsMuxStream * stream, guint8 * buf, guint len)
{
  g_return_val_if_fail (stream != NULL, FALSE);
  g_return_val_if_fail (buf != NULL, FALSE);

  if (stream->state == TSMUX_STREAM_STATE_HEADER) {
    guint8 pes_hdr_length;

    pes_hdr_length = tsmux_stream_pes_header_length (stream);

    /* Submitted buffer must be at least as large as the PES header */
    if (len < pes_hdr_length)
      return FALSE;

    TS_DEBUG ("Writing PES header of length %u and payload %d",
        pes_hdr_length, stream->cur_pes_payload_size);
    tsmux_stream_write_pes_header (stream, buf);

    len -= pes_hdr_length;
    buf += pes_hdr_length;

    stream->state = TSMUX_STREAM_STATE_PACKET;
  }

  if (len > (guint) _tsmux_stream_bytes_avail (stream))
    return FALSE;

  stream->pes_bytes_written += len;

  if (stream->cur_pes_payload_size != 0 &&
      stream->pes_bytes_written == stream->cur_pes_payload_size) {
    TS_DEBUG ("Finished PES packet");
    stream->state = TSMUX_STREAM_STATE_HEADER;
    stream->pes_bytes_written = 0;
  }

  while (len > 0) {
    guint32 avail;
    guint8 *cur;

    if (stream->cur_buffer == NULL) {
      /* Start next packet */
      if (stream->buffers == NULL)
        return FALSE;
      stream->cur_buffer = (TsMuxStreamBuffer *) (stream->buffers->data);
      stream->cur_buffer_consumed = 0;
    }

    /* Take as much as we can from the current buffer */
    avail = stream->cur_buffer->size - stream->cur_buffer_consumed;
    cur = stream->cur_buffer->data + stream->cur_buffer_consumed;
    if (avail < len) {
      memcpy (buf, cur, avail);
      tsmux_stream_consume (stream, avail);

      buf += avail;
      len -= avail;
    } else {
      memcpy (buf, cur, len);
      tsmux_stream_consume (stream, len);

      len = 0;
    }
  }

  return TRUE;
}

static guint8
tsmux_stream_pes_header_length (TsMuxStream * stream)
{
  guint8 packet_len;

  /* Calculate the length of the header for this stream */

  /* start_code prefix + stream_id + pes_packet_length = 6 bytes */
  packet_len = 6;

  if (stream->pi.flags & TSMUX_PACKET_FLAG_PES_FULL_HEADER) {
    /* For a PES 'full header' we have at least 3 more bytes,
     * and then more based on flags */
    packet_len += 3;
    if (stream->pi.flags & TSMUX_PACKET_FLAG_PES_WRITE_PTS_DTS) {
      packet_len += 10;
    } else if (stream->pi.flags & TSMUX_PACKET_FLAG_PES_WRITE_PTS) {
      packet_len += 5;
    }
    if (stream->pi.flags & TSMUX_PACKET_FLAG_PES_EXT_STREAMID) {
      /* Need basic extension flags (1 byte), plus 2 more bytes for the
       * length + extended stream id */
      packet_len += 3;
    }
    if (stream->pi.pes_header_length) {
      /* check for consistency, then we can add stuffing */
      g_assert (packet_len <= stream->pi.pes_header_length + 6 + 3);
      packet_len = stream->pi.pes_header_length + 6 + 3;
    }
  }

  return packet_len;
}

/* Find a PTS/DTS to write into the pes header within the next bound bytes
 * of the data */
static void
tsmux_stream_find_pts_dts_within (TsMuxStream * stream, guint bound,
    gint64 * pts, gint64 * dts)
{
  GList *cur;

  *pts = GST_CLOCK_STIME_NONE;
  *dts = GST_CLOCK_STIME_NONE;

  for (cur = stream->buffers; cur; cur = cur->next) {
    TsMuxStreamBuffer *curbuf = cur->data;

    /* FIXME: This isn't quite correct - if the 'bound' is within this
     * buffer, we don't know if the timestamp is before or after the split
     * so we shouldn't return it */
    if (bound <= curbuf->size) {
      *pts = curbuf->pts;
      *dts = curbuf->dts;
      return;
    }

    /* Have we found a buffer with pts/dts set? */
    if (GST_CLOCK_STIME_IS_VALID (curbuf->pts)
        || GST_CLOCK_STIME_IS_VALID (curbuf->dts)) {
      *pts = curbuf->pts;
      *dts = curbuf->dts;
      return;
    }

    bound -= curbuf->size;
  }
}

static void
tsmux_stream_write_pes_header (TsMuxStream * stream, guint8 * data)
{
  guint16 length_to_write;
  guint8 hdr_len = tsmux_stream_pes_header_length (stream);
  guint8 *orig_data = data;

  /* start_code prefix + stream_id + pes_packet_length = 6 bytes */
  data[0] = 0x00;
  data[1] = 0x00;
  data[2] = 0x01;
  data[3] = stream->id;
  data += 4;

  /* Write 2 byte PES packet length here. 0 (unbounded) is only
   * valid for video packets */
  if (stream->cur_pes_payload_size != 0) {
    length_to_write = hdr_len + stream->cur_pes_payload_size - 6;
  } else {
    length_to_write = 0;
  }

  tsmux_put16 (&data, length_to_write);

  if (stream->pi.flags & TSMUX_PACKET_FLAG_PES_FULL_HEADER) {
    guint8 flags = 0;

    /* Not scrambled, original, not-copyrighted, data_alignment not specified */
    flags = 0x81;
    if (stream->pi.flags & TSMUX_PACKET_FLAG_PES_DATA_ALIGNMENT)
      flags |= 0x4;
    *data++ = flags;
    flags = 0;

    /* Flags */
    if (stream->pi.flags & TSMUX_PACKET_FLAG_PES_WRITE_PTS_DTS)
      flags |= 0xC0;
    else if (stream->pi.flags & TSMUX_PACKET_FLAG_PES_WRITE_PTS)
      flags |= 0x80;
    if (stream->pi.flags & TSMUX_PACKET_FLAG_PES_EXT_STREAMID)
      flags |= 0x01;            /* Enable PES_extension_flag */
    *data++ = flags;

    /* Header length is the total pes length,
     * minus the 9 bytes of start codes, flags + hdr_len */
    g_return_if_fail (hdr_len >= 9);
    *data++ = (hdr_len - 9);

    if (stream->pi.flags & TSMUX_PACKET_FLAG_PES_WRITE_PTS_DTS) {
      tsmux_put_ts (&data, 0x3, stream->pts);
      tsmux_put_ts (&data, 0x1, stream->dts);
    } else if (stream->pi.flags & TSMUX_PACKET_FLAG_PES_WRITE_PTS) {
      tsmux_put_ts (&data, 0x2, stream->pts);
    }
    if (stream->pi.flags & TSMUX_PACKET_FLAG_PES_EXT_STREAMID) {
      guint8 ext_len;

      flags = 0x0f;             /* (reserved bits) | PES_extension_flag_2 */
      *data++ = flags;

      ext_len = 1;              /* Only writing 1 byte into the extended fields */
      *data++ = 0x80 | ext_len;
      /* Write the extended streamID */
      *data++ = stream->id_extended;
    }
    /* write stuffing bytes if fixed PES header length requested */
    if (stream->pi.pes_header_length)
      while (data < orig_data + stream->pi.pes_header_length + 9)
        *data++ = 0xff;
  }
}

/**
 * tsmux_stream_add_data:
 * @stream: a #TsMuxStream
 * @data: data to add
 * @len: length of @data
 * @user_data: user data to pass to release func
 * @pts: PTS of access unit in @data
 * @dts: DTS of access unit in @data
 * @random_access: TRUE if random access point (keyframe)
 *
 * Submit @len bytes of @data into @stream. @pts and @dts can be set to the
 * timestamp (against a 90Hz clock) of the first access unit in @data. A
 * timestamp of GST_CLOCK_STIME_NONE for @pts or @dts means unknown.
 *
 * @user_data will be passed to the release function as set with
 * tsmux_stream_set_buffer_release_func() when @data can be freed.
 */
void
tsmux_stream_add_data (TsMuxStream * stream, guint8 * data, guint len,
    void *user_data, gint64 pts, gint64 dts, gboolean random_access)
{
  TsMuxStreamBuffer *packet;

  g_return_if_fail (stream != NULL);

  packet = g_new (TsMuxStreamBuffer, 1);
  packet->data = data;
  packet->size = len;
  packet->user_data = user_data;
  packet->random_access = random_access;

  packet->pts = pts;
  packet->dts = dts;

  if (stream->bytes_avail == 0) {
    stream->last_pts = pts;
    stream->last_dts = dts;
  }

  stream->bytes_avail += len;
  stream->buffers = g_list_append (stream->buffers, packet);
}

/**
 * tsmux_stream_default_get_es_descrs:
 * @stream: a #TsMuxStream
 * @buf: a buffer to hold the ES descriptor
 * @len: the length used in @buf
 *
 * Write an Elementary Stream Descriptor for @stream into @buf. the number of
 * bytes consumed in @buf will be updated in @len.
 *
 * @buf and @len must be at least #TSMUX_MIN_ES_DESC_LEN.
 */
void
tsmux_stream_default_get_es_descrs (TsMuxStream * stream,
    GstMpegtsPMTStream * pmt_stream)
{
  GstMpegtsDescriptor *descriptor;

  g_return_if_fail (stream != NULL);
  g_return_if_fail (pmt_stream != NULL);

  if (stream->is_audio && stream->language[0] != '\0') {
    descriptor = gst_mpegts_descriptor_from_iso_639_language (stream->language);
    g_ptr_array_add (pmt_stream->descriptors, descriptor);
    descriptor = NULL;
  }

  /* Based on the stream type, write out any descriptors to go in the
   * PMT ES_info field */
  /* tag (registration_descriptor), length, format_identifier */
  switch (stream->stream_type) {
    case TSMUX_ST_AUDIO_AAC:
      /* FIXME */
      break;
    case TSMUX_ST_VIDEO_MPEG4:
      /* FIXME */
      break;
    case TSMUX_ST_VIDEO_H264:
    {
      /* FIXME : Not sure about this additional_identification_info */
      guint8 add_info[] = { 0xFF, 0x1B, 0x44, 0x3F };

      descriptor = gst_mpegts_descriptor_from_registration ("HDMV",
          add_info, 4);

      g_ptr_array_add (pmt_stream->descriptors, descriptor);
      break;
    }
    case TSMUX_ST_VIDEO_DIRAC:
      descriptor = gst_mpegts_descriptor_from_registration ("drac", NULL, 0);
      g_ptr_array_add (pmt_stream->descriptors, descriptor);
      break;
    case TSMUX_ST_VIDEO_JP2K:
    {
      /* J2K video descriptor
       * descriptor_tag             8 uimsbf
       * descriptor_length          8 uimsbf
       * profile_and_level         16 uimsbf
       * horizontal_size           32 uimsbf
       * vertical_size             32 uimsbf
       * max_bit_rate              32 uimsbf
       * max_buffer_size           32 uimsbf
       * DEN_frame_rate            16 uimsbf
       * NUM_frame_rate            16 uimsbf
       * color_specification        8 bslbf
       * still_mode                 1 bslbf
       * interlace_video            1 bslbf
       * reserved                   6 bslbf
       * private_data_byte          8 bslbf
       */
      gint8 still_interlace_reserved = 0x00;
      int wr_size = 0;
      guint8 *add_info = NULL;
      guint8 level = stream->profile_and_level & 0xF;
      guint32 max_buffer_size = 0;
      GstByteWriter writer;
      gst_byte_writer_init_with_size (&writer, 32, FALSE);

      switch (level) {
        case 1:
        case 2:
        case 3:
          max_buffer_size = 1250000;
          break;
        case 4:
          max_buffer_size = 2500000;
          break;
        case 5:
          max_buffer_size = 5000000;
          break;
        case 6:
          max_buffer_size = 10000000;
          break;
        default:
          break;
      }

      gst_byte_writer_put_uint16_be (&writer, stream->profile_and_level);
      gst_byte_writer_put_uint32_be (&writer, stream->horizontal_size);
      gst_byte_writer_put_uint32_be (&writer, stream->vertical_size);
      gst_byte_writer_put_uint32_be (&writer, max_buffer_size);
      gst_byte_writer_put_uint32_be (&writer, stream->max_bitrate);
      gst_byte_writer_put_uint16_be (&writer, stream->den);
      gst_byte_writer_put_uint16_be (&writer, stream->num);
      gst_byte_writer_put_uint8 (&writer, stream->color_spec);

      if (stream->interlace_mode)
        still_interlace_reserved |= 0x40;

      gst_byte_writer_put_uint8 (&writer, still_interlace_reserved);
      gst_byte_writer_put_uint8 (&writer, 0x00);        /* private data byte */

      wr_size = gst_byte_writer_get_size (&writer);
      add_info = gst_byte_writer_reset_and_get_data (&writer);

      descriptor =
          gst_mpegts_descriptor_from_custom (GST_MTS_DESC_J2K_VIDEO, add_info,
          wr_size);
      g_ptr_array_add (pmt_stream->descriptors, descriptor);
    }
      break;
    case TSMUX_ST_PS_AUDIO_AC3:
    {
      /* This is only called for DVB, ATSC ignores this case in favour of its
       * special handling for the 0x81 descriptor */
      /* We choose to provide no optional information */
      guint8 add_info[1] = { 0x00 };

      descriptor = gst_mpegts_descriptor_from_registration ("AC-3", NULL, 0);
      g_ptr_array_add (pmt_stream->descriptors, descriptor);

      descriptor =
          gst_mpegts_descriptor_from_custom (GST_MTS_DESC_DVB_AC3, add_info, 1);
      g_ptr_array_add (pmt_stream->descriptors, descriptor);
      break;
    }
    case TSMUX_ST_PS_AUDIO_DTS:
      /* FIXME */
      break;
    case TSMUX_ST_PS_AUDIO_LPCM:
      /* FIXME */
      break;
    case TSMUX_ST_PS_TELETEXT:
      /* FIXME empty descriptor for now;
       * should be provided by upstream in event or so ? */
      descriptor =
          gst_mpegts_descriptor_from_custom (GST_MTS_DESC_DVB_TELETEXT, 0, 1);

      g_ptr_array_add (pmt_stream->descriptors, descriptor);
      break;
    case TSMUX_ST_PS_DVB_SUBPICTURE:
      /* fallthrough ...
       * that should never happen anyway as
       * dvb subtitles are private data */
    case TSMUX_ST_PRIVATE_DATA:
      if (stream->is_dvb_sub) {
        GST_DEBUG ("Stream language %s", stream->language);
        /* Simple DVB subtitles with no monitor aspect ratio critical
           FIXME, how do we make it settable? */
        /* Default composition page ID */
        /* Default ancillary_page_id */
        descriptor =
            gst_mpegts_descriptor_from_dvb_subtitling (stream->language, 0x10,
            0x0001, 0x0152);

        g_ptr_array_add (pmt_stream->descriptors, descriptor);
        break;
      }
      if (stream->is_opus) {
        descriptor = gst_mpegts_descriptor_from_registration ("Opus", NULL, 0);
        g_ptr_array_add (pmt_stream->descriptors, descriptor);

        descriptor =
            gst_mpegts_descriptor_from_custom_with_extension
            (GST_MTS_DESC_DVB_EXTENSION, 0x80,
            stream->opus_channel_config, stream->opus_channel_config_len);

        g_ptr_array_add (pmt_stream->descriptors, descriptor);
      }
      if (stream->is_meta) {
        descriptor = gst_mpegts_descriptor_from_registration ("KLVA", NULL, 0);
        GST_DEBUG ("adding KLVA registration descriptor");
        g_ptr_array_add (pmt_stream->descriptors, descriptor);
      }
    default:
      break;
  }
}

/**
 * tsmux_stream_get_es_descrs:
 * @stream: a #TsMuxStream
 * @buf: a buffer to hold the ES descriptor
 * @len: the length used in @buf
 *
 * Write an Elementary Stream Descriptor for @stream into @buf. the number of
 * bytes consumed in @buf will be updated in @len.
 *
 * @buf and @len must be at least #TSMUX_MIN_ES_DESC_LEN.
 */
void
tsmux_stream_get_es_descrs (TsMuxStream * stream,
    GstMpegtsPMTStream * pmt_stream)
{
  g_return_if_fail (stream->get_es_descrs != NULL);

  stream->get_es_descrs (stream, pmt_stream, stream->get_es_descrs_data);
}

/**
 * tsmux_stream_pcr_ref:
 * @stream: a #TsMuxStream
 *
 * Mark the stream as being used as the PCR for some program.
 */
void
tsmux_stream_pcr_ref (TsMuxStream * stream)
{
  g_return_if_fail (stream != NULL);

  stream->pcr_ref++;
}

/**
 * tsmux_stream_pcr_unref:
 * @stream: a #TsMuxStream
 *
 * Mark the stream as no longer being used as the PCR for some program.
 */
void
tsmux_stream_pcr_unref (TsMuxStream * stream)
{
  g_return_if_fail (stream != NULL);

  stream->pcr_ref--;
}

/**
 * tsmux_stream_is_pcr:
 * @stream: a #TsMuxStream
 *
 * Check if @stream is used as the PCR for some program.
 *
 * Returns: TRUE if the stream is in use as the PCR for some program.
 */
gboolean
tsmux_stream_is_pcr (TsMuxStream * stream)
{
  return stream->pcr_ref != 0;
}

/**
 * tsmux_stream_get_pts:
 * @stream: a #TsMuxStream
 *
 * Return the PTS of the last buffer that has had bytes written and
 * which _had_ a PTS in @stream.
 *
 * Returns: the PTS of the last buffer in @stream.
 */
gint64
tsmux_stream_get_pts (TsMuxStream * stream)
{
  g_return_val_if_fail (stream != NULL, GST_CLOCK_STIME_NONE);

  return stream->last_pts;
}

/**
 * tsmux_stream_get_dts:
 * @stream: a #TsMuxStream
 *
 * Return the DTS of the last buffer that has had bytes written and
 * which _had_ a DTS in @stream.
 *
 * Returns: the DTS of the last buffer in @stream.
 */
gint64
tsmux_stream_get_dts (TsMuxStream * stream)
{
  g_return_val_if_fail (stream != NULL, GST_CLOCK_STIME_NONE);

  return stream->last_dts;
}
