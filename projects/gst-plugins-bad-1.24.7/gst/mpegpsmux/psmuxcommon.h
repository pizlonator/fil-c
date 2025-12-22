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



#ifndef __PSMUX_COMMON_H__
#define __PSMUX_COMMON_H__

#include <glib.h>
#include <gst/gst.h>
#include "bits.h" /* from VLC */

G_BEGIN_DECLS

#define PSMUX_PACK_HDR_FREQ	30
#define PSMUX_SYS_HDR_FREQ	300
#define PSMUX_PSM_FREQ		300

#define PSMUX_PES_MAX_PAYLOAD 65500 /* from VLC */
#define PSMUX_PES_MAX_HDR_LEN 30
#define PSMUX_MAX_PACKET_LEN (PSMUX_PES_MAX_PAYLOAD + PSMUX_PES_MAX_HDR_LEN)

#define CLOCKBASE 90000
#define PSMUX_PACK_HDR_INTERVAL		( 0.7 * CLOCKBASE) /* interval to update pack header. 0.7 sec */
#define PSMUX_BITRATE_CALC_INTERVAL	CLOCKBASE /* interval to update bitrate in pack header. 1 sec */

#define PSMUX_PES_BITRATE_DEFAULT 1000 /* Default bit_rate to write in the first pack header */

#define PSMUX_START_CODE_PREFIX         0x01

/* stream_id */ 
#define PSMUX_PACK_HEADER               0xba
#define PSMUX_SYSTEM_HEADER             0xbb
#define PSMUX_PROGRAM_STREAM_MAP        0xbc
#define PSMUX_PRIVATE_STREAM_1          0xbd
#define PSMUX_PADDING_STREAM            0xbe
#define PSMUX_PRIVATE_STREAM_2          0xbf
#define PSMUX_ECM                       0xb0
#define PSMUX_EMM                       0xb1
#define PSMUX_PROGRAM_STREAM_DIRECTORY  0xff
#define PSMUX_DSMCC_STREAM              0xf2
#define PSMUX_ITU_T_H222_1_TYPE_E       0xf8
#define PSMUX_EXTENDED_STREAM           0xfd
#define PSMUX_PROGRAM_END               0xb9

#define PSMUX_MIN_ES_DESC_LEN 8

/* Frequency for PCR representation */
#define PSMUX_SYS_CLOCK_FREQ (27000000L)
/* Frequency for PTS values */
#define PSMUX_CLOCK_FREQ (PSMUX_SYS_CLOCK_FREQ / 300)

/* TODO: flags? looks that we don't need these */
#define PSMUX_PACKET_FLAG_NONE            (0)
#define PSMUX_PACKET_FLAG_ADAPTATION      (1 << 0)
#define PSMUX_PACKET_FLAG_DISCONT         (1 << 1)
#define PSMUX_PACKET_FLAG_RANDOM_ACCESS   (1 << 2)
#define PSMUX_PACKET_FLAG_PRIORITY        (1 << 3)
#define PSMUX_PACKET_FLAG_WRITE_PCR       (1 << 4)
#define PSMUX_PACKET_FLAG_WRITE_OPCR      (1 << 5)
#define PSMUX_PACKET_FLAG_WRITE_SPLICE    (1 << 6)
#define PSMUX_PACKET_FLAG_WRITE_ADAPT_EXT (1 << 7)

/* PES stream specific flags */
#define PSMUX_PACKET_FLAG_PES_FULL_HEADER   (1 << 8)
#define PSMUX_PACKET_FLAG_PES_WRITE_PTS     (1 << 9)
#define PSMUX_PACKET_FLAG_PES_WRITE_PTS_DTS (1 << 10)
#define PSMUX_PACKET_FLAG_PES_WRITE_ESCR    (1 << 11)
#define PSMUX_PACKET_FLAG_PES_EXT_STREAMID  (1 << 12)
#define PSMUX_PACKET_FLAG_PES_DATA_ALIGN    (1 << 13)

typedef struct PsMuxPacketInfo PsMuxPacketInfo;
typedef struct PsMuxProgram PsMuxProgram;
typedef struct PsMuxStream PsMuxStream;
typedef struct PsMuxStreamIdInfo PsMuxStreamIdInfo;
typedef struct PsMux PsMux;
typedef enum PsMuxStreamType PsMuxStreamType;
typedef struct PsMuxStreamBuffer PsMuxStreamBuffer;

/* clearup and see if we need this anymore */
struct PsMuxPacketInfo {
  guint32 flags;
};

/* bitstream writers */
static inline void
psmux_put16 (guint8 **pos, guint16 val)
{
  *(*pos)++ = (val >> 8) & 0xff;
  *(*pos)++ = val & 0xff;
}

static inline void
psmux_put32 (guint8 **pos, guint32 val)
{
  *(*pos)++ = (val >> 24) & 0xff;
  *(*pos)++ = (val >> 16) & 0xff;
  *(*pos)++ = (val >> 8) & 0xff;
  *(*pos)++ = val & 0xff;
}

static inline void
psmux_put_ts (guint8 **pos, guint8 id, gint64 ts)
{
  /* 1: 4 bit id value | TS [32..30] | marker_bit */
  *(*pos)++ = ((id << 4) | ((ts >> 29) & 0x0E) | 0x01) & 0xff;
  /* 2, 3: TS[29..15] | marker_bit */
  psmux_put16 (pos, ((ts >> 14) & 0xfffe) | 0x01);
  /* 4, 5: TS[14..0] | marker_bit */
  psmux_put16 (pos, ((ts << 1) & 0xfffe) | 0x01);
}

G_END_DECLS

#endif
