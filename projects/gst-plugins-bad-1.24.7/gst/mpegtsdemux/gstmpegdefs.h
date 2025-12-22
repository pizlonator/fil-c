/*
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
 * The Original Code is Fluendo MPEG Demuxer plugin.
 *
 * The Initial Developer of the Original Code is Fluendo, S.L.
 * Portions created by Fluendo, S.L. are Copyright (C) 2005
 * Fluendo, S.L. All Rights Reserved.
 *
 * Contributor(s): Wim Taymans <wim@fluendo.com>
 */

#ifndef __GST_MPEG_DEFS_H__
#define __GST_MPEG_DEFS_H__
#include <glib/gprintf.h>

#define SAFE_FOURCC_FORMAT "02x%02x%02x%02x (%c%c%c%c)"
#define SAFE_CHAR(a) (g_ascii_isalnum((gchar) (a)) ? ((gchar)(a)) : '.')
#define SAFE_FOURCC_ARGS(a)				\
  ((guint8) ((a)>>24)),					\
    ((guint8) ((a) >> 16 & 0xff)),			\
    ((guint8) ((a) >> 8 & 0xff)),			\
    ((guint8) ((a) & 0xff)),				\
    SAFE_CHAR((a)>>24),					\
    SAFE_CHAR((a) >> 16 & 0xff),			\
    SAFE_CHAR((a) >> 8 & 0xff),				\
    SAFE_CHAR(a & 0xff)

/* Stream type assignments */
/* FIXME: Put these in mpegts lib separate stream type enums */
/* Un-official Dirac extension */
#define ST_VIDEO_DIRAC                  0xd1

/* private stream types */
#define ST_PS_AUDIO_AC4                 0x06
#define ST_PS_VIDEO_MPEG2_DCII          0x80
#define ST_PS_AUDIO_AC3                 0x81
#define ST_PS_AUDIO_EAC3                0x87
#define ST_PS_AUDIO_LPCM2               0x83
#define ST_PS_AUDIO_DTS                 0x8a
#define ST_PS_AUDIO_LPCM                0x8b
#define ST_PS_DVD_SUBPICTURE            0xff

/* Blu-ray related (registration: 'HDMV'*/
#define ST_BD_AUDIO_LPCM                0x80
#define ST_BD_AUDIO_AC3                 0x81
#define ST_BD_AUDIO_DTS                 0x82
#define ST_BD_AUDIO_AC3_TRUE_HD         0x83
#define ST_BD_AUDIO_AC3_PLUS            0x84
#define ST_BD_AUDIO_DTS_HD              0x85
#define ST_BD_AUDIO_DTS_HD_MASTER_AUDIO 0x86
#define ST_BD_AUDIO_EAC3                0x87
#define ST_BD_PGS_SUBPICTURE            0x90
#define ST_BD_IGS                       0x91
#define ST_BD_SUBTITLE                  0x92
#define ST_BD_SECONDARY_AC3_PLUS        0xa1
#define ST_BD_SECONDARY_DTS_HD          0xa2

/* defined for VC1 extension in RP227 */
#define ST_PRIVATE_EA                   0xea

/* Following only apply for streams identified as HDV,
 * According to specification 61834-11 the PMT will use
 * a registration descriptor with values TSMV or TSHV */
/* HDV AUX stream mapping
 * 0xA0      ISO/IEC 61834-11
 * 0xA1      ISO/IEC 61834-11
 */
#define ST_HDV_AUX_A                    0xa0
#define ST_HDV_AUX_V                    0xa1

#define CLOCK_BASE 9LL
#define CLOCK_FREQ (CLOCK_BASE * 10000)

/* Numerical values for second/millisecond in PCR units */
#define PCR_SECOND 27000000
#define PCR_MSECOND 27000

/* PCR_TO_GST calculation requires at least 10 extra bits.
 * Since maximum PCR value is coded with 42 bits, we are
 * safe to use direct calculation (10+42 < 63)*/
#define PCRTIME_TO_GSTTIME(t) (((t) * (guint64)1000) / 27)

/* MPEG_TO_GST calculation requires at least 17 extra bits (100000)
 * Since maximum PTS/DTS value is coded with 33bits, we are
 * safe to use direct calculation (17+33 < 63) */
#define MPEGTIME_TO_GSTTIME(t) ((t) * (guint64)100000 / 9)

#define GSTTIME_TO_MPEGTIME(time) (gst_util_uint64_scale ((time), \
            CLOCK_BASE, GST_MSECOND/10))
#define GSTTIME_TO_PCRTIME(time) (gst_util_uint64_scale ((time), \
            300 * CLOCK_BASE, GST_MSECOND/10))

#define MPEG_MUX_RATE_MULT      50

/* sync:4 == 00xx ! pts:3 ! 1 ! pts:15 ! 1 | pts:15 ! 1 */
#define READ_TS(data, target, lost_sync_label)          \
    if ((*data & 0x01) != 0x01) goto lost_sync_label;   \
    target  = ((guint64) (*data++ & 0x0E)) << 29;       \
    target |= ((guint64) (*data++       )) << 22;       \
    if ((*data & 0x01) != 0x01) goto lost_sync_label;   \
    target |= ((guint64) (*data++ & 0xFE)) << 14;       \
    target |= ((guint64) (*data++       )) << 7;        \
    if ((*data & 0x01) != 0x01) goto lost_sync_label;   \
    target |= ((guint64) (*data++ & 0xFE)) >> 1;

#endif /* __GST_MPEG_DEFS_H__ */
