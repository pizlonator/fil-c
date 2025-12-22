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
#include <gst/gst.h>

#include "mpegpsmux.h"
#include "psmuxcommon.h"
#include "psmuxstream.h"
#include "psmux.h"
#include "crc.h"

static gboolean psmux_packet_out (PsMux * mux);
static gboolean psmux_write_pack_header (PsMux * mux);
static gboolean psmux_write_system_header (PsMux * mux);
static gboolean psmux_write_program_stream_map (PsMux * mux);

/**
 * psmux_new:
 *
 * Create a new muxer session.
 *
 * Returns: A new #PsMux object.
 */
PsMux *
psmux_new (void)
{
  PsMux *mux;

  mux = g_new0 (PsMux, 1);

  mux->pts = -1;                /* uninitialized values */
  mux->pack_hdr_pts = -1;
  mux->sys_hdr_pts = -1;
  mux->psm_pts = -1;

  mux->bit_pts = 0;

  mux->pes_max_payload = PSMUX_PES_MAX_PAYLOAD;
  mux->bit_rate = 400 * 1024;   /* XXX: better default values? */
  mux->rate_bound = 2 * 1024;   /* 2* bit_rate / (8*50). XXX: any better default? */

  mux->pack_hdr_freq = PSMUX_PACK_HDR_FREQ;
  mux->sys_hdr_freq = PSMUX_SYS_HDR_FREQ;
  mux->psm_freq = PSMUX_PSM_FREQ;

  psmux_stream_id_info_init (&mux->id_info);

  return mux;
}

/**
 * psmux_set_write_func:
 * @mux: a #PsMux
 * @func: a user callback function
 * @user_data: user data passed to @func
 *
 * Set the callback function and user data to be called when @mux has output to
 * produce. @user_data will be passed as user data in @func.
 */
void
psmux_set_write_func (PsMux * mux, PsMuxWriteFunc func, void *user_data)
{
  g_return_if_fail (mux != NULL);

  mux->write_func = func;
  mux->write_func_data = user_data;
}

gboolean
psmux_write_end_code (PsMux * mux)
{
  guint8 end_code[4] = { 0, 0, 1, PSMUX_PROGRAM_END };
  return mux->write_func (end_code, 4, mux->write_func_data);
}


/**
 * psmux_free:
 * @mux: a #PsMux
 *
 * Free all resources associated with @mux. After calling this function @mux can
 * not be used anymore.
 */
void
psmux_free (PsMux * mux)
{
  GList *cur;

  g_return_if_fail (mux != NULL);

  /* Free all streams */
  for (cur = g_list_first (mux->streams); cur != NULL; cur = g_list_next (cur)) {
    PsMuxStream *stream = (PsMuxStream *) cur->data;

    psmux_stream_free (stream);
  }
  g_list_free (mux->streams);

  if (mux->sys_header != NULL)
    gst_buffer_unref (mux->sys_header);

  if (mux->psm != NULL)
    gst_buffer_unref (mux->psm);

  g_free (mux);
}

/**
 * psmux_create_stream:
 * @mux: a #PsMux
 * @stream_type: a #PsMuxStreamType
 *
 * Create a new stream of @stream_type in the muxer session @mux.
 *
 * Returns: a new #PsMuxStream.
 */
PsMuxStream *
psmux_create_stream (PsMux * mux, PsMuxStreamType stream_type)
{
  PsMuxStream *stream;
//  guint16 new_pid;

  g_return_val_if_fail (mux != NULL, NULL);

#if 0
  if (pid == PSMUX_PID_AUTO) {
    new_pid = psmux_get_new_pid (mux);
  } else {
    new_pid = pid & 0x1FFF;
  }

  /* Ensure we're not creating a PID collision */
  if (psmux_find_stream (mux, new_pid))
    return NULL;
#endif

  stream = psmux_stream_new (mux, stream_type);

  mux->streams = g_list_prepend (mux->streams, stream);
  if (stream->stream_id_ext) {
    if (!mux->nb_private_streams)
      mux->nb_streams++;
    mux->nb_private_streams++;
  } else
    mux->nb_streams++;

  if (stream->is_video_stream) {
    mux->video_bound++;
    if (mux->video_bound > 32)
      g_critical ("Number of video es exceeds upper limit");
  } else if (stream->is_audio_stream) {
    mux->audio_bound++;
    if (mux->audio_bound > 64)
      g_critical ("Number of audio es exceeds upper limit");
  }

  return stream;
}

static gboolean
psmux_packet_out (PsMux * mux)
{
  gboolean res;
  if (G_UNLIKELY (mux->write_func == NULL))
    return TRUE;

  res = mux->write_func (mux->packet_buf, mux->packet_bytes_written,
      mux->write_func_data);

  if (res) {
    mux->bit_size += mux->packet_bytes_written;
  }
  mux->packet_bytes_written = 0;
  return res;
}

/**
 * psmux_write_stream_packet:
 * @mux: a #PsMux
 * @stream: a #PsMuxStream
 *
 * Write a packet of @stream.
 *
 * Returns: TRUE if the packet could be written.
 */
gboolean
psmux_write_stream_packet (PsMux * mux, PsMuxStream * stream)
{
  gboolean res;

  g_return_val_if_fail (mux != NULL, FALSE);
  g_return_val_if_fail (stream != NULL, FALSE);


  {
    guint64 ts = psmux_stream_get_pts (stream);
    if (ts != -1)
      mux->pts = ts;
  }

  if (mux->pts - mux->pack_hdr_pts > PSMUX_PACK_HDR_INTERVAL
      || mux->pes_cnt % mux->pack_hdr_freq == 0) {
    /* Time to write pack header */
    /* FIXME: currently we write the mux rate of the PREVIOUS pack into the
     * pack header, because of the incapability to calculate the mux_rate
     * before outputting the pack. To calculate the mux_rate for the current
     * pack, we need to put the whole pack into buffer, calculate the
     * mux_rate, and then output the whole trunck.
     */
    if (mux->pts != -1 && mux->pts > mux->bit_pts
        && mux->pts - mux->bit_pts > PSMUX_BITRATE_CALC_INTERVAL) {
      /* XXX: smoothing the rate? */
      mux->bit_rate =
          gst_util_uint64_scale (mux->bit_size, 8 * CLOCKBASE,
          (mux->pts - mux->bit_pts));

      mux->bit_size = 0;
      mux->bit_pts = mux->pts;
    }

    psmux_write_pack_header (mux);
    mux->pack_hdr_pts = mux->pts;
  }

  if (mux->pes_cnt % mux->sys_hdr_freq == 0) {
    /* Time to write system header */
    psmux_write_system_header (mux);
    mux->sys_hdr_pts = mux->pts;
  }

  if (mux->pes_cnt % mux->psm_freq == 0) {
    /* Time to write program stream map (PSM) */
    psmux_write_program_stream_map (mux);
    mux->psm_pts = mux->pts;
  }

  /* Write the packet */
  if (!(mux->packet_bytes_written =
          psmux_stream_get_data (stream, mux->packet_buf,
              mux->pes_max_payload + PSMUX_PES_MAX_HDR_LEN))) {
    return FALSE;
  }

  res = psmux_packet_out (mux);
  if (!res) {
    GST_DEBUG_OBJECT (mux, "packet write false");
    return FALSE;
  }

  mux->pes_cnt += 1;

  return res;
}

static gboolean
psmux_write_pack_header (PsMux * mux)
{
  bits_buffer_t bw;
  guint64 scr = mux->pts;       /* XXX: is this correct? necessary to put any offset? */
  if (mux->pts == -1)
    scr = 0;

  /* pack_start_code */
  bits_initwrite (&bw, 14, mux->packet_buf);
  bits_write (&bw, 24, PSMUX_START_CODE_PREFIX);
  bits_write (&bw, 8, PSMUX_PACK_HEADER);

  /* scr */
  bits_write (&bw, 2, 0x1);
  bits_write (&bw, 3, (scr >> 30) & 0x07);
  bits_write (&bw, 1, 1);
  bits_write (&bw, 15, (scr >> 15) & 0x7fff);
  bits_write (&bw, 1, 1);
  bits_write (&bw, 15, scr & 0x7fff);
  bits_write (&bw, 1, 1);
  bits_write (&bw, 9, 0);       /* system_clock_reference_extension: set to 0 (like what VLC does) */
  bits_write (&bw, 1, 1);

  {
    /* Scale to get the mux_rate, rounding up */
    guint mux_rate =
        gst_util_uint64_scale (mux->bit_rate + 8 * 50 - 1, 1, 8 * 50);
    if (mux_rate > mux->rate_bound / 2)
      mux->rate_bound = mux_rate * 2;
    bits_write (&bw, 22, mux_rate);     /* program_mux_rate */
    bits_write (&bw, 2, 3);
  }

  bits_write (&bw, 5, 0x1f);
  bits_write (&bw, 3, 0);       /* pack_stuffing_length */

  mux->packet_bytes_written = 14;
  return psmux_packet_out (mux);
}

static void
psmux_ensure_system_header (PsMux * mux)
{
  bits_buffer_t bw;
  guint len = 12 + (mux->nb_streams +
      (mux->nb_private_streams > 1 ? mux->nb_private_streams - 1 : 0)) * 3;
  GList *cur;
  gboolean private_hit = FALSE;
  guint8 *data;

  if (mux->sys_header != NULL)
    return;

  data = g_malloc (len);

  bits_initwrite (&bw, len, data);

  /* system_header start code */
  bits_write (&bw, 24, PSMUX_START_CODE_PREFIX);
  bits_write (&bw, 8, PSMUX_SYSTEM_HEADER);

  bits_write (&bw, 16, len - 6);        /* header_length (bytes after this field) */
  bits_write (&bw, 1, 1);       /* marker */
  bits_write (&bw, 22, mux->rate_bound);        /* rate_bound */
  bits_write (&bw, 1, 1);       /* marker */
  bits_write (&bw, 6, mux->audio_bound);        /* audio_bound */
  bits_write (&bw, 1, 0);       /* fixed_flag */
  bits_write (&bw, 1, 0);       /* CSPS_flag */
  bits_write (&bw, 1, 0);       /* system_audio_lock_flag */
  bits_write (&bw, 1, 0);       /* system_video_lock_flag */
  bits_write (&bw, 1, 1);       /* marker */
  bits_write (&bw, 5, mux->video_bound);        /* video_bound */
  bits_write (&bw, 1, 0);       /* packet_rate_restriction_flag */
  bits_write (&bw, 7, 0x7f);    /* reserved_bits */

  for (cur = mux->streams, private_hit = FALSE; cur != NULL; cur = cur->next) {
    PsMuxStream *stream = (PsMuxStream *) cur->data;

    if (private_hit && stream->stream_id == PSMUX_EXTENDED_STREAM)
      continue;

    bits_write (&bw, 8, stream->stream_id);     /* stream_id */
    bits_write (&bw, 2, 0x3);   /* reserved */
    bits_write (&bw, 1, stream->is_video_stream);       /* buffer_bound_scale */
    bits_write (&bw, 13, stream->max_buffer_size / (stream->is_video_stream ? 1024 : 128));     /* buffer_size_bound */

    if (stream->stream_id == PSMUX_EXTENDED_STREAM)
      private_hit = TRUE;
  }

  GST_MEMDUMP ("System Header", data, len);

  mux->sys_header = gst_buffer_new_wrapped (data, len);
}

static gboolean
psmux_write_system_header (PsMux * mux)
{
  GstMapInfo map;

  psmux_ensure_system_header (mux);

  gst_buffer_map (mux->sys_header, &map, GST_MAP_READ);
  memcpy (mux->packet_buf, map.data, map.size);
  mux->packet_bytes_written = map.size;
  gst_buffer_unmap (mux->sys_header, &map);

  return psmux_packet_out (mux);
}

static void
psmux_ensure_program_stream_map (PsMux * mux)
{
  gint psm_size = 16, es_map_size = 0;
  bits_buffer_t bw;
  GList *cur;
  guint16 len;
  guint8 *pos;
  guint8 *data;

  if (mux->psm != NULL)
    return;

  /* pre-write the descriptor loop */
  pos = mux->es_info_buf;
  for (cur = mux->streams; cur != NULL; cur = cur->next) {
    PsMuxStream *stream = (PsMuxStream *) cur->data;
    len = 0;

    *pos++ = stream->stream_type;
    *pos++ = stream->stream_id;

    psmux_stream_get_es_descrs (stream, pos + 2, &len);
    psmux_put16 (&pos, len);

    es_map_size += len + 4;
    pos += len;
#if 0
    if (stream->lang[0] != 0)
      es_map_size += 6;
#endif
  }

  psm_size += es_map_size;

  data = g_malloc (psm_size);

  bits_initwrite (&bw, psm_size, data);

  /* psm start code */
  bits_write (&bw, 24, PSMUX_START_CODE_PREFIX);
  bits_write (&bw, 8, PSMUX_PROGRAM_STREAM_MAP);

  bits_write (&bw, 16, psm_size - 6);   /* psm_length */
  bits_write (&bw, 1, 1);       /* current_next_indicator */
  bits_write (&bw, 2, 0xF);     /* reserved */
  bits_write (&bw, 5, 0x1);     /* psm_version = 1 */
  bits_write (&bw, 7, 0xFF);    /* reserved */
  bits_write (&bw, 1, 1);       /* marker */

  bits_write (&bw, 16, 0);      /* program_stream_info_length */
  /* program_stream_info empty */

  bits_write (&bw, 16, es_map_size);    /* elementary_stream_map_length */

  memcpy (bw.p_data + bw.i_data, mux->es_info_buf, es_map_size);

  /* CRC32 */
  {
    guint32 crc = calc_crc32 (bw.p_data, psm_size - 4);
    guint8 *pos = bw.p_data + psm_size - 4;
    psmux_put32 (&pos, crc);
  }

  GST_MEMDUMP ("Program Stream Map", data, psm_size);

  mux->psm = gst_buffer_new_wrapped (data, psm_size);
}

static gboolean
psmux_write_program_stream_map (PsMux * mux)
{
  GstMapInfo map;

  psmux_ensure_program_stream_map (mux);

  gst_buffer_map (mux->psm, &map, GST_MAP_READ);
  memcpy (mux->packet_buf, map.data, map.size);
  mux->packet_bytes_written = map.size;
  gst_buffer_unmap (mux->psm, &map);

  return psmux_packet_out (mux);
}

GList *
psmux_get_stream_headers (PsMux * mux)
{
  GList *list;

  psmux_ensure_system_header (mux);
  psmux_ensure_program_stream_map (mux);

  list = g_list_append (NULL, gst_buffer_ref (mux->sys_header));
  list = g_list_append (list, gst_buffer_ref (mux->psm));

  return list;
}
