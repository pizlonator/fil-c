/*
 * pesparse.h : MPEG PES parsing utility
 * Copyright (C) 2011 Edward Hervey <bilboed@gmail.com>
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

#ifndef __PES_PARSE_H__
#define __PES_PARSE_H__

#include <gst/gst.h>
#include "gstmpegdefs.h"

G_BEGIN_DECLS


/*
 * PES stream_id assignments:
 *
 * 1011 1100                program_stream_map
 * 1011 1101                private_stream_1
 * 1011 1110                padding_stream
 * 1011 1111                private_stream_2
 * 110x xxxx                ISO/IEC 13818-3 or ISO/IEC 11172-3 audio stream number x xxxx
 * 1110 xxxx                ITU-T Rec. H.262 | ISO/IEC 13818-2 or ISO/IEC 11172-2 video stream number xxxx
 * 1111 0000                ECM_stream
 * 1111 0001                EMM_stream
 * 1111 0010                ITU-T Rec. H.222.0 | ISO/IEC 13818-1 Annex A or ISO/IEC 13818-6_DSMCC_stream
 * 1111 0011                ISO/IEC_13522_stream
 * 1111 0100                ITU-T Rec. H.222.1 type A
 * 1111 0101                ITU-T Rec. H.222.1 type B
 * 1111 0110                ITU-T Rec. H.222.1 type C
 * 1111 0111                ITU-T Rec. H.222.1 type D
 * 1111 1000                ITU-T Rec. H.222.1 type E
 * 1111 1001                ancillary_stream
 * 1111 1010                ISO/IEC 14496-1_SL-packetized_stream
 * 1111 1011                ISO/IEC 14496-1_FlexMux_stream
 * 1111 1100                metadata stream
 * 1111 1101                extended_stream_id
 * 1111 1110                reserved data stream
 * 1111 1111                program_stream_directory
 */

#define ID_PS_END_CODE                          0xB9
#define ID_PS_PACK_START_CODE                   0xBA
#define ID_PS_SYSTEM_HEADER_START_CODE          0xBB
#define ID_PS_PROGRAM_STREAM_MAP                0xBC
#define ID_PRIVATE_STREAM_1                     0xBD
#define ID_PADDING_STREAM                       0xBE
#define ID_PRIVATE_STREAM_2                     0xBF
#define ID_ISO_IEC_MPEG12_AUDIO_STREAM_0        0xC0
#define ID_ISO_IEC_MPEG12_AUDIO_STREAM_32       0xDF
#define ID_ISO_IEC_MPEG12_VIDEO_STREAM_0        0xE0
#define ID_ISO_IEC_MPEG12_VIDEO_STREAM_16       0xEF
#define ID_ECM_STREAM                           0xF0
#define ID_EMM_STREAM                           0xF1
#define ID_DSMCC_STREAM                         0xF2
#define ID_ISO_IEC_13522_STREAM                 0xF3
#define ID_ITU_TREC_H222_TYPE_A_STREAM          0xF4
#define ID_ITU_TREC_H222_TYPE_B_STREAM          0xF5
#define ID_ITU_TREC_H222_TYPE_C_STREAM          0xF6
#define ID_ITU_TREC_H222_TYPE_D_STREAM          0xF7
#define ID_ITU_TREC_H222_TYPE_E_STREAM          0xF8
#define ID_ANCILLARY_STREAM                     0xF9
#define ID_14496_1_SL_PACKETIZED_STREAM         0xFA
#define ID_14496_1_SL_FLEXMUX_STREAM            0xFB
#define ID_METADATA_STREAM                      0xFC
#define ID_EXTENDED_STREAM_ID                   0xFD
#define ID_RESERVED_STREAM_3                    0xFE
#define ID_PROGRAM_STREAM_DIRECTORY             0xFF

/*
 * PES stream_id_extension assignments (if stream_id == ID_EXTENDED_STREAM_ID)
 *
 *  000 0000             IPMP Control Information stream
 *  000 0001             IPMP Stream
 *  000 0010 - 001 0001  ISO/IEC 14496-17 text Streams
 *  001 0010 - 010 0001  ISO/IEC 23002-3 auxiliary video data Streams
 *  ... .... - 011 1111  Reserved
 *
 *  PRIVATE STREAM RANGES (But known as used)
 *  101 0101 - 101 1111  VC-1
 *  110 0000 - 110 1111  Dirac (VC-1)
 *
 *  111 0001             AC3 or independent sub-stream 0 of EAC3/DD+
 *                       DTS or core sub-stream
 *  111 0010             dependent sub-stream of EAC3/DD+
 *                       DTS extension sub-stream
 *                       Secondary EAC3/DD+
 *                       Secondary DTS-HD LBR
 *  111 0110             AC3 in MLP/TrueHD
 *  1xx xxxx    private_stream
 */
#define EXT_ID_IPMP_CONTORL_INFORMATION_STREAM  0x00
#define EXT_ID_IPMP_STREAM			0x01

/* VC-1 */
#define EXT_ID_VC1_FIRST			0x55
#define EXT_ID_VC1_LAST 			0x5F

typedef enum {
  PES_FLAG_PRIORITY		= 1 << 3,	/* PES_priority (present: high-priority) */
  PES_FLAG_DATA_ALIGNMENT	= 1 << 2,	/* data_alignment_indicator */
  PES_FLAG_COPYRIGHT		= 1 << 1,	/* copyright */
  PES_FLAG_ORIGINAL_OR_COPY	= 1 << 0	/* original_or_copy */
} PESHeaderFlags;

typedef enum {
  PES_TRICK_MODE_FAST_FORWARD	= 0x000,
  PES_TRICK_MODE_SLOW_MOTION    = 0x001,
  PES_TRICK_MODE_FREEZE_FRAME	= 0x010,
  PES_TRICK_MODE_FAST_REVERSE	= 0x011,
  PES_TRICK_MODE_SLOW_REVERSE	= 0x100,
  /* ... */
  PES_TRICK_MODE_INVALID	= 0xfff	/* Not present or invalid */
} PESTrickModeControl;

typedef enum {
  PES_FIELD_ID_TOP_ONLY		= 0x00, /* Display from top field only */
  PES_FIELD_ID_BOTTOM_ONLY	= 0x01, /* Display from bottom field only */
  PES_FIELD_ID_COMPLETE_FRAME	= 0x10, /* Display complete frame */
  PES_FIELD_ID_INVALID		= 0x11	/* Reserved/Invalid */
} PESFieldID;

typedef enum {
  PES_PARSING_OK	= 0,	/* Header fully parsed and valid */
  PES_PARSING_BAD	= 1,	/* Header invalid (CRC error for ex) */
  PES_PARSING_NEED_MORE = 2	/* Not enough data to parse header */
} PESParsingResult;

typedef struct {
  guint8	stream_id;	/* See ID_* above */
  guint32	packet_length;	/* The size of the PES header and PES data
				 * (if 0 => unbounded packet) */
  guint16	header_size;	/* The complete size of the PES header */

  /* All remaining entries in this structure are optional */
  guint8	scrambling_control; /* 0x00  : Not scrambled/unspecified,
				     * The following are according to ETSI TS 101 154
				     * 0x01  : reserved for future DVB use
				     * 0x10  : PES packet scrambled with Even key
				     * 0x11  : PES packet scrambled with Odd key
				     */
  PESHeaderFlags flags;

  guint64	PTS;		/* PTS (-1 if not present or invalid) */
  guint64	DTS;		/* DTS (-1 if not present or invalid) */
  guint64	ESCR;		/* ESCR (-1 if not present or invalid) */

  guint32	ES_rate;	/* in bytes/seconds (0 if not present or invalid) */
  PESTrickModeControl	trick_mode;
  
  /* Only valid for _FAST_FORWARD, _FAST_REVERSE and _FREEZE_FRAME */
  PESFieldID	field_id;
  /* Only valid for _FAST_FORWARD and _FAST_REVERSE */
  gboolean	intra_slice_refresh;
  guint8	frequency_truncation;
  /* Only valid for _SLOW_FORWARD and _SLOW_REVERSE */
  guint8	rep_cntrl;

  guint8	additional_copy_info; /* Private data */
  guint16	previous_PES_packet_CRC;

  /* Extension fields */
  const guint8*	private_data;			/* PES_private_data, 16 bytes long */
  guint8	pack_header_size;		/* Size of pack_header in bytes */
  const guint8*	pack_header;
  gint8		program_packet_sequence_counter; /* -1 if not present or invalid */
  gboolean	MPEG1_MPEG2_identifier;
  guint8	original_stuff_length;

  guint32	P_STD_buffer_size; /* P-STD buffer size in bytes (0 if invalid
				    * or not present */

  guint8	stream_id_extension; /* Public range (0x00 - 0x3f) only valid if stream_id == ID_EXTENDED_STREAM_ID
				      * Private range (0x40 - 0xff) can be present in any stream type */

  gsize		extension_field_length;   /* Length of remaining extension field data */
  const guint8*	stream_id_extension_data; /* Valid if extension_field_length != 0 */
} PESHeader;

G_GNUC_INTERNAL PESParsingResult mpegts_parse_pes_header (const guint8* data,
							  gsize size,
							  PESHeader *res);
G_GNUC_INTERNAL void init_pes_parser (void);

G_END_DECLS
#endif /* __PES_PARSE_H__ */
