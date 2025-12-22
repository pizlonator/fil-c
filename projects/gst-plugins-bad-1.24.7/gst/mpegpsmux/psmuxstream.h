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



#ifndef __PSMUXSTREAM_H__
#define __PSMUXSTREAM_H__

#include <gst/gst.h>

#include "psmuxcommon.h"

G_BEGIN_DECLS


typedef void (*PsMuxStreamBufferReleaseFunc) (guint8 *data, void *user_data);

enum PsMuxStreamType { /* Table 2-29 in spec */
  PSMUX_ST_RESERVED                   = 0x00,
  PSMUX_ST_VIDEO_MPEG1                = 0x01,
  PSMUX_ST_VIDEO_MPEG2                = 0x02,
  PSMUX_ST_AUDIO_MPEG1                = 0x03,
  PSMUX_ST_AUDIO_MPEG2                = 0x04,
  PSMUX_ST_PRIVATE_SECTIONS           = 0x05,
  PSMUX_ST_PRIVATE_DATA               = 0x06,
  PSMUX_ST_MHEG                       = 0x07,
  PSMUX_ST_DSMCC                      = 0x08,
  PSMUX_ST_H222_1                     = 0x09,

  /* later extensions */
  PSMUX_ST_AUDIO_AAC                  = 0x0f,
  PSMUX_ST_VIDEO_MPEG4                = 0x10,
  PSMUX_ST_VIDEO_H264                 = 0x1b,

  /* private stream types */
  PSMUX_ST_PS_AUDIO_AC3               = 0x81,
  PSMUX_ST_PS_AUDIO_DTS               = 0x8a,
  PSMUX_ST_PS_AUDIO_LPCM              = 0x8b,
  PSMUX_ST_PS_DVD_SUBPICTURE          = 0xff,

  /* Non-standard definitions */
  PSMUX_ST_VIDEO_DIRAC                = 0xD1
};

struct PsMuxStreamBuffer
{
  gboolean keyunit;

  /* PTS & DTS associated with the contents of this buffer */
  GstClockTime pts;
  GstClockTime dts;

  GstBuffer *buf;
  GstMapInfo map;
};

/* PsMuxStream receives elementary streams for parsing.
 * Via the write_bytes() method, it can output a PES stream piecemeal */
struct PsMuxStream{
  PsMuxPacketInfo pi;

  PsMuxStreamType stream_type;
  guint8 stream_id;
  guint8 stream_id_ext; /* extended stream id (13818-1 Amdt 2) */

  /* List of data buffers available for writing out */
  GList *buffers;
  guint32 bytes_avail;

  /* Current data buffer being consumed */
  PsMuxStreamBuffer *cur_buffer;
  guint32 cur_buffer_consumed;

  /* PES payload */
  guint16 cur_pes_payload_size;
  guint16 pes_bytes_written; /* delete*/

  /* PTS/DTS to write if the flags in the packet info are set */
  gint64 pts; /* TODO: cur_buffer->pts?*/
  gint64 dts; /* TODO: cur_buffer->dts?*/
  gint64 last_pts;

  /* stream type */
  gboolean is_video_stream;
  gboolean is_audio_stream;

  /* for writing descriptors */
  gint audio_sampling;
  gint audio_channels;
  gint audio_bitrate;

  /* for writing buffer size in system header */
  guint max_buffer_size;
};

/* stream management */
PsMuxStream*    psmux_stream_new                (PsMux * mux, PsMuxStreamType stream_type);
void 		psmux_stream_free 		(PsMuxStream *stream);

/* Add a new buffer to the pool of available bytes. If pts or dts are not -1, they
 * indicate the PTS or DTS of the first access unit within this packet */
void 		psmux_stream_add_data 		(PsMuxStream *stream,
						 GstBuffer * buffer,
						 gint64 pts, gint64 dts,
						 gboolean keyunit);

/* total bytes in buffer */
gint 		psmux_stream_bytes_in_buffer 	(PsMuxStream *stream);
/* number of bytes of raw data available for writing */
gint 		psmux_stream_bytes_avail 	(PsMuxStream *stream);

/* write PES data */
guint	 	psmux_stream_get_data 		(PsMuxStream *stream, guint8 *buf, guint len);

/* write corresponding descriptors of the stream */
void 		psmux_stream_get_es_descrs 	(PsMuxStream *stream, guint8 *buf, guint16 *len);

/* get the pts of stream */
guint64 	psmux_stream_get_pts 		(PsMuxStream *stream);

/* stream_id assignment */
#define PSMUX_STREAM_ID_MPGA_INIT       0xc0
#define PSMUX_STREAM_ID_MPGA_MAX        0xcf

#define PSMUX_STREAM_ID_MPGV_INIT       0xe0
#define PSMUX_STREAM_ID_MPGV_MAX        0xef

#define PSMUX_STREAM_ID_AC3_INIT        0x80
#define PSMUX_STREAM_ID_AC3_MAX         0x87

#define PSMUX_STREAM_ID_SPU_INIT        0x20
#define PSMUX_STREAM_ID_SPU_MAX        	0x3f

#define PSMUX_STREAM_ID_DTS_INIT        0x88
#define PSMUX_STREAM_ID_DTS_MAX         0x8f

#define PSMUX_STREAM_ID_LPCM_INIT       0xa0
#define PSMUX_STREAM_ID_LPCM_MAX        0xaf

#define PSMUX_STREAM_ID_DIRAC_INIT      0x60
#define PSMUX_STREAM_ID_DIRAC_MAX       0x6f

struct PsMuxStreamIdInfo {
    guint8 id_mpga;
    guint8 id_mpgv;
    guint8 id_ac3;
    guint8 id_spu;
    guint8 id_dts;
    guint8 id_lpcm;
    guint8 id_dirac;
};

static inline void
psmux_stream_id_info_init (PsMuxStreamIdInfo * info)
{
    g_return_if_fail (info != NULL);
    info->id_mpga = PSMUX_STREAM_ID_MPGA_INIT;
    info->id_mpgv = PSMUX_STREAM_ID_MPGV_INIT;
    info->id_ac3  = PSMUX_STREAM_ID_AC3_INIT;
    info->id_spu  = PSMUX_STREAM_ID_SPU_INIT;
    info->id_dts  = PSMUX_STREAM_ID_DTS_INIT;
    info->id_lpcm = PSMUX_STREAM_ID_LPCM_INIT;
    info->id_dirac= PSMUX_STREAM_ID_DIRAC_INIT;
}

G_END_DECLS

#endif
