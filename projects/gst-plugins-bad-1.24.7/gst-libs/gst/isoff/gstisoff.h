/*
 * ISO File Format parsing library
 *
 * gstisoff.h
 *
 * Copyright (C) 2015 Samsung Electronics. All rights reserved.
 *   Author: Thiago Santos <thiagoss@osg.samsung.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library (COPYING); if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GST_ISOFF_H__
#define __GST_ISOFF_H__

#include <gst/gst.h>
#include <gst/base/base.h>

G_BEGIN_DECLS

#ifndef GST_ISOFF_API
# ifdef BUILDING_GST_ISOFF
#  define GST_ISOFF_API GST_API_EXPORT         /* from config.h */
# else
#  define GST_ISOFF_API GST_API_IMPORT
# endif
#endif

typedef enum {
  GST_ISOFF_PARSER_OK,
  GST_ISOFF_PARSER_DONE,
  GST_ISOFF_PARSER_UNEXPECTED,
  GST_ISOFF_PARSER_ERROR
} GstIsoffParserResult;

GST_ISOFF_API
gboolean gst_isoff_parse_box_header (GstByteReader * reader, guint32 * type, guint8 extended_type[16], guint * header_size, guint64 * size);

#define GST_ISOFF_FOURCC_UUID GST_MAKE_FOURCC('u','u','i','d')
#define GST_ISOFF_FOURCC_MOOF GST_MAKE_FOURCC('m','o','o','f')
#define GST_ISOFF_FOURCC_MFHD GST_MAKE_FOURCC('m','f','h','d')
#define GST_ISOFF_FOURCC_TFHD GST_MAKE_FOURCC('t','f','h','d')
#define GST_ISOFF_FOURCC_TRUN GST_MAKE_FOURCC('t','r','u','n')
#define GST_ISOFF_FOURCC_TRAF GST_MAKE_FOURCC('t','r','a','f')
#define GST_ISOFF_FOURCC_TFDT GST_MAKE_FOURCC('t','f','d','t')
#define GST_ISOFF_FOURCC_MDAT GST_MAKE_FOURCC('m','d','a','t')
#define GST_ISOFF_FOURCC_MOOV GST_MAKE_FOURCC('m','o','o','v')
#define GST_ISOFF_FOURCC_TRAK GST_MAKE_FOURCC('t','r','a','k')
#define GST_ISOFF_FOURCC_TKHD GST_MAKE_FOURCC('t','k','h','d')
#define GST_ISOFF_FOURCC_MDIA GST_MAKE_FOURCC('m','d','i','a')
#define GST_ISOFF_FOURCC_MDHD GST_MAKE_FOURCC('m','d','h','d')
#define GST_ISOFF_FOURCC_HDLR GST_MAKE_FOURCC('h','d','l','r')
#define GST_ISOFF_FOURCC_SIDX GST_MAKE_FOURCC('s','i','d','x')

/* handler type */
#define GST_ISOFF_FOURCC_SOUN GST_MAKE_FOURCC('s','o','u','n')
#define GST_ISOFF_FOURCC_VIDE GST_MAKE_FOURCC('v','i','d','e')

#define GST_ISOFF_SAMPLE_FLAGS_IS_LEADING(flags)                   (((flags) >> 26) & 0x03)
#define GST_ISOFF_SAMPLE_FLAGS_SAMPLE_DEPENDS_ON(flags)            (((flags) >> 24) & 0x03)
#define GST_ISOFF_SAMPLE_FLAGS_SAMPLE_IS_DEPENDED_ON(flags)        (((flags) >> 22) & 0x03)
#define GST_ISOFF_SAMPLE_FLAGS_SAMPLE_HAS_REDUNDANCY(flags)        (((flags) >> 20) & 0x03)
#define GST_ISOFF_SAMPLE_FLAGS_SAMPLE_PADDING_VALUE(flags)         (((flags) >> 17) & 0x07)
#define GST_ISOFF_SAMPLE_FLAGS_SAMPLE_IS_NON_SYNC_SAMPLE(flags)    (((flags) >> 16) & 0x01)
#define GST_ISOFF_SAMPLE_FLAGS_SAMPLE_DEGRADATION_PRIORITY(flags)  (((flags) >>  0) & 0x0f)

/* Smooth-Streaming specific boxes */
typedef struct _GstTfxdBox
{
  guint8 version;
  guint32 flags;

  guint64 time;
  guint64 duration;
} GstTfxdBox;

typedef struct _GstTfrfBoxEntry
{
  guint64 time;
  guint64 duration;
} GstTfrfBoxEntry;

typedef struct _GstTfrfBox
{
  guint8 version;
  guint32 flags;

  gint entries_count;
  GArray *entries;
} GstTfrfBox;

/* Common boxes */
typedef struct _GstMfhdBox
{
  guint32 sequence_number;
} GstMfhdBox;

typedef enum
{
  GST_TFHD_FLAGS_BASE_DATA_OFFSET_PRESENT         = 0x000001,
  GST_TFHD_FLAGS_SAMPLE_DESCRIPTION_INDEX_PRESENT = 0x000002,
  GST_TFHD_FLAGS_DEFAULT_SAMPLE_DURATION_PRESENT  = 0x000008,
  GST_TFHD_FLAGS_DEFAULT_SAMPLE_SIZE_PRESENT      = 0x000010,
  GST_TFHD_FLAGS_DEFAULT_SAMPLE_FLAGS_PRESENT     = 0x000020,
  GST_TFHD_FLAGS_DURATION_IS_EMPTY                = 0x010000,
  GST_TFHD_FLAGS_DEFAULT_BASE_IS_MOOF             = 0x020000
} GstTfhdFlags;

typedef struct _GstTfhdBox
{
  guint8 version;
  GstTfhdFlags flags;

  guint32 track_id;

  /* optional */
  guint64 base_data_offset;
  guint32 sample_description_index;
  guint32 default_sample_duration;
  guint32 default_sample_size;
  guint32 default_sample_flags;
} GstTfhdBox;

typedef enum
{
  GST_TRUN_FLAGS_DATA_OFFSET_PRESENT                     = 0x000001,
  GST_TRUN_FLAGS_FIRST_SAMPLE_FLAGS_PRESENT              = 0x000004,
  GST_TRUN_FLAGS_SAMPLE_DURATION_PRESENT                 = 0x000100,
  GST_TRUN_FLAGS_SAMPLE_SIZE_PRESENT                     = 0x000200,
  GST_TRUN_FLAGS_SAMPLE_FLAGS_PRESENT                    = 0x000400,
  GST_TRUN_FLAGS_SAMPLE_COMPOSITION_TIME_OFFSETS_PRESENT = 0x000800
} GstTrunFlags;

typedef struct _GstTrunBox
{
  guint8 version;
  GstTrunFlags flags;

  guint32 sample_count;

  /* optional */
  gint32 data_offset;
  guint32 first_sample_flags;
  GArray *samples;
} GstTrunBox;

typedef struct _GstTrunSample
{
  guint32 sample_duration;
  guint32 sample_size;
  guint32 sample_flags;

  union {
    guint32 u; /* version 0 */
    gint32  s; /* others */
  } sample_composition_time_offset;
} GstTrunSample;

typedef struct _GstTdftBox
{
  guint64 decode_time;
} GstTfdtBox;

typedef struct _GstTrafBox
{
  GstTfhdBox tfhd;
  GstTfdtBox tfdt;
  GArray *trun;

  /* smooth-streaming specific */
  GstTfrfBox *tfrf;
  GstTfxdBox *tfxd;
} GstTrafBox;

typedef struct _GstMoofBox
{
  GstMfhdBox mfhd;
  GArray *traf;
} GstMoofBox;

GST_ISOFF_API
GstMoofBox * gst_isoff_moof_box_parse (GstByteReader *reader);

GST_ISOFF_API
void gst_isoff_moof_box_free (GstMoofBox *moof);

typedef struct _GstTkhdBox
{
  guint32 track_id;
} GstTkhdBox;

typedef struct _GstMdhdBox
{
  guint32 timescale;
} GstMdhdBox;

typedef struct _GstHdlrBox
{
  guint32 handler_type;
} GstHdlrBox;

typedef struct _GstMdiaBox
{
  GstMdhdBox mdhd;
  GstHdlrBox hdlr;
} GstMdiaBox;

typedef struct _GstTrakBox
{
  GstTkhdBox tkhd;
  GstMdiaBox mdia;
} GstTrakBox;

typedef struct _GstMoovBox
{
  GArray *trak;
} GstMoovBox;

GST_ISOFF_API
GstMoovBox * gst_isoff_moov_box_parse (GstByteReader *reader);

GST_ISOFF_API
void gst_isoff_moov_box_free (GstMoovBox *moov);

typedef struct _GstSidxBoxEntry
{
  gboolean ref_type;
  guint32 size;
  GstClockTime duration;
  gboolean starts_with_sap;
  guint8 sap_type;
  guint32 sap_delta_time;

  guint64 offset;
  GstClockTime pts;
} GstSidxBoxEntry;

typedef struct _GstSidxBox
{
  guint8 version;
  guint32 flags;

  guint32 ref_id;
  guint32 timescale;
  guint64 earliest_pts;
  guint64 first_offset;

  gint entry_index;
  gint entries_count;

  GstSidxBoxEntry *entries;
} GstSidxBox;

typedef enum _GstSidxParserStatus
{
  GST_ISOFF_SIDX_PARSER_INIT,
  GST_ISOFF_SIDX_PARSER_HEADER,
  GST_ISOFF_SIDX_PARSER_DATA,
  GST_ISOFF_SIDX_PARSER_FINISHED
} GstSidxParserStatus;

typedef struct _GstSidxParser
{
  GstSidxParserStatus status;

  guint64 size;
  guint64 cumulative_entry_size;
  guint64 cumulative_pts;

  GstSidxBox sidx;
} GstSidxParser;

GST_ISOFF_API
void gst_isoff_sidx_parser_init (GstSidxParser * parser);

GST_ISOFF_API
void gst_isoff_sidx_parser_clear (GstSidxParser * parser);

GST_ISOFF_API
GstIsoffParserResult gst_isoff_sidx_parser_parse (GstSidxParser * parser, GstByteReader * reader, guint * consumed);

GST_ISOFF_API
GstIsoffParserResult gst_isoff_sidx_parser_add_buffer (GstSidxParser * parser, GstBuffer * buf, guint * consumed);

G_END_DECLS

#endif /* __GST_ISOFF_H__ */
