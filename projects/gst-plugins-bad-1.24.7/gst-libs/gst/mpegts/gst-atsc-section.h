/*
 * mpegtspacketizer.h -
 * Copyright (C) 2013 Edward Hervey
 *
 * Authors:
 *   Edward Hervey <edward@collabora.com>
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

#ifndef GST_ATSC_SECTION_H
#define GST_ATSC_SECTION_H

#include <gst/gst.h>
#include <gst/mpegts/gstmpegtssection.h>
#include <gst/mpegts/gstmpegtsdescriptor.h>

G_BEGIN_DECLS

/**
 * GstMpegtsSectionATSCTableID:
 * @GST_MTS_TABLE_ID_ATSC_MASTER_GUIDE: Master Guide Table (MGT)
 * @GST_MTS_TABLE_ID_ATSC_TERRESTRIAL_VIRTUAL_CHANNEL: Terrestrial Virtual Channel Table (TVCT)
 * @GST_MTS_TABLE_ID_ATSC_CABLE_VIRTUAL_CHANNEL: Cable Virtual Channel Table (CVCT)
 * @GST_MTS_TABLE_ID_ATSC_RATING_REGION: Rating Region Table (RRT)
 * @GST_MTS_TABLE_ID_ATSC_EVENT_INFORMATION: Event Information Table (EIT)
 * @GST_MTS_TABLE_ID_ATSC_CHANNEL_OR_EVENT_EXTENDED_TEXT: Extended Text Table (ETT)
 * @GST_MTS_TABLE_ID_ATSC_SYSTEM_TIME: System Time Table (STT)
 * @GST_MTS_TABLE_ID_ATSC_DATA_EVENT: A/90: Data Event Table (DET)
 * @GST_MTS_TABLE_ID_ATSC_DATA_SERVICE: A/90: Data Service Table (DST)
 * @GST_MTS_TABLE_ID_ATSC_NETWORK_RESOURCE: A/90: Network Resources Table (NRT)
 * @GST_MTS_TABLE_ID_ATSC_LONG_TERM_SERVICE: A/90: Long Term Service Table (LTST)
 * @GST_MTS_TABLE_ID_ATSC_DIRECTED_CHANNEL_CHANGE: Directed Channel Change Table (DCCT)
 * @GST_MTS_TABLE_ID_ATSC_DIRECTED_CHANNEL_CHANGE_SECTION_CODE: Directed Channel Change Selection Code Table (DCCSCT)
 * @GST_MTS_TABLE_ID_ATSC_SATELLITE_VIRTUAL_CHANNEL: A/81: Satellite Virtual Channel Table
 *
 * Values for a #GstMpegtsSection table_id.
 *
 * These are the registered ATSC section `table_id` variants. Unless specified
 * otherwise, they are defined in the "ATSC A/65" specification.
 *
 * see also: #GstMpegtsSectionTableID and other variants.
 */
typedef enum {

  /* ATSC (A/65) */
  GST_MTS_TABLE_ID_ATSC_MASTER_GUIDE                    = 0xC7,
  GST_MTS_TABLE_ID_ATSC_TERRESTRIAL_VIRTUAL_CHANNEL     = 0xC8,
  GST_MTS_TABLE_ID_ATSC_CABLE_VIRTUAL_CHANNEL           = 0xC9,
  GST_MTS_TABLE_ID_ATSC_RATING_REGION                   = 0xCA,
  GST_MTS_TABLE_ID_ATSC_EVENT_INFORMATION               = 0xCB,
  GST_MTS_TABLE_ID_ATSC_CHANNEL_OR_EVENT_EXTENDED_TEXT  = 0xCC,
  GST_MTS_TABLE_ID_ATSC_SYSTEM_TIME                     = 0xCD,
  /* ATSC (A/90) */
  GST_MTS_TABLE_ID_ATSC_DATA_EVENT                      = 0xCE,
  GST_MTS_TABLE_ID_ATSC_DATA_SERVICE                    = 0xCF,

  /* ATSC (A/57B) */
  /**
   * GST_MTS_TABLE_ID_ATSC_PROGRAM_IDENTIFIER:
   *
   * A/57B: Program Identifier Table.
   *
   * Since: 1.20
   */
  GST_MTS_TABLE_ID_ATSC_PROGRAM_IDENTIFIER		= 0xD0,
  /* ATSC (A/90) */
  GST_MTS_TABLE_ID_ATSC_NETWORK_RESOURCE                = 0xD1,
  GST_MTS_TABLE_ID_ATSC_LONG_TERM_SERVICE               = 0xD2,
  /* ATSC (A/65) */
  GST_MTS_TABLE_ID_ATSC_DIRECTED_CHANNEL_CHANGE         = 0xD3,
  GST_MTS_TABLE_ID_ATSC_DIRECTED_CHANNEL_CHANGE_SECTION_CODE = 0xD4,
  /* 0xD5-0xD9 covered in CEA/SCTE */
  GST_MTS_TABLE_ID_ATSC_AGGREGATE_EVENT_INFORMATION     = 0xD6,
  GST_MTS_TABLE_ID_ATSC_AGGREGATE_EXTENDED_TEXT         = 0xD7,
  GST_MTS_TABLE_ID_ATSC_AGGREGATE_DATA_EVENT            = 0xD9,
  /*  */
  GST_MTS_TABLE_ID_ATSC_SATELLITE_VIRTUAL_CHANNEL       = 0xDA,
} GstMpegtsSectionATSCTableID;

/**
 * GstMpegtsATSCStreamType:
 * @GST_MPEGTS_STREAM_TYPE_ATSC_DCII_VIDEO:  DigiCipher II video | Identical to ITU-T Rec. H.262 | ISO/IEC 13818-2 Video
 * @GST_MPEGTS_STREAM_TYPE_ATSC_AUDIO_AC3:   ATSC A/53 Audio | AC-3
 * @GST_MPEGTS_STREAM_TYPE_ATSC_SUBTITLING:  SCTE-27 Subtitling
 * @GST_MPEGTS_STREAM_TYPE_ATSC_ISOCH_DATA:  SCTE-19 Isochronous data | Reserved
 * @GST_MPEGTS_STREAM_TYPE_ATSC_SIT:         SCTE-35 Splice Information Table
 * @GST_MPEGTS_STREAM_TYPE_ATSC_AUDIO_EAC3:  E-AC-3 A/52:2018
 * @GST_MPEGTS_STREAM_TYPE_ATSC_AUDIO_DTS_HD:  E-AC-3 A/107 (ATSC 2.0)
 *
 * Type of mpeg-ts streams for ATSC, as defined by the ATSC Code Points
 * Registry. For convenience, some stream types from %GstMpegtsScteStreamType
 * are also included.
 *
 * Since: 1.20
 */
typedef enum {
  GST_MPEGTS_STREAM_TYPE_ATSC_DCII_VIDEO = 0x80,
  GST_MPEGTS_STREAM_TYPE_ATSC_AUDIO_AC3  = 0x81,
  GST_MPEGTS_STREAM_TYPE_ATSC_SUBTITLING = 0x82,
  GST_MPEGTS_STREAM_TYPE_ATSC_ISOCH_DATA = 0x83,
  /* 0x84-0x85 : RESERVED */
  GST_MPEGTS_STREAM_TYPE_ATSC_SIT        = 0x86,
  GST_MPEGTS_STREAM_TYPE_ATSC_AUDIO_EAC3 = 0x87,
  GST_MPEGTS_STREAM_TYPE_ATSC_AUDIO_DTS_HD = 0x88,
} GstMpegtsATSCStreamType;

/* TVCT/CVCT */
#define GST_TYPE_MPEGTS_ATSC_VCT (gst_mpegts_atsc_vct_get_type ())
#define GST_TYPE_MPEGTS_ATSC_VCT_SOURCE (gst_mpegts_atsc_vct_source_get_type ())

typedef struct _GstMpegtsAtscVCTSource GstMpegtsAtscVCTSource;
typedef struct _GstMpegtsAtscVCT GstMpegtsAtscVCT;

/**
 * GstMpegtsAtscVCTSource:
 * @short_name: The short name of a source
 * @major_channel_number: The major channel number
 * @minor_channel_number: The minor channel number
 * @modulation_mode: The modulation mode
 * @carrier_frequency: The carrier frequency
 * @channel_TSID: The transport stream ID
 * @program_number: The program number (see #GstMpegtsPatProgram)
 * @ETM_location: The ETM location
 * @access_controlled: is access controlled
 * @hidden: is hidden
 * @path_select: is path select, CVCT only
 * @out_of_band: is out of band, CVCT only
 * @hide_guide: is hide guide
 * @service_type: The service type
 * @source_id: The source id
 * @descriptors: (element-type GstMpegtsDescriptor): an array of #GstMpegtsDescriptor
 *
 * Source from a %GstMpegtsAtscVCT, can be used both for TVCT and CVCT tables
 */
struct _GstMpegtsAtscVCTSource
{
  gchar    *short_name;
  guint16   major_channel_number;
  guint16   minor_channel_number;
  guint8    modulation_mode;
  guint32   carrier_frequency;
  guint16   channel_TSID;
  guint16   program_number;
  /* FIXME: */
  guint8    ETM_location;
  gboolean  access_controlled;
  gboolean  hidden;
  gboolean  path_select; /* CVCT only - reserved bit in TVCT */
  gboolean  out_of_band; /* CVCT only - reserved bit in TVCT */
  gboolean  hide_guide;
  /* FIXME: */
  guint8    service_type;
  guint16   source_id;
  GPtrArray *descriptors;
};

/**
 * GstMpegtsAtscVCT:
 * @transport_stream_id: The transport stream
 * @protocol_version: The protocol version
 * @sources: (element-type GstMpegtsAtscVCTSource): sources
 * @descriptors: (element-type GstMpegtsDescriptor): descriptors
 *
 * Represents both:
 *   Terrestrial Virtual Channel Table (A65)
 *   Cable Virtual Channel Table (A65)
 *
 */
struct _GstMpegtsAtscVCT
{
  guint16   transport_stream_id;
  guint8    protocol_version;
  GPtrArray *sources;
  GPtrArray *descriptors;
};

GST_MPEGTS_API
GType gst_mpegts_atsc_vct_get_type (void);

GST_MPEGTS_API
GType gst_mpegts_atsc_vct_source_get_type (void);

GST_MPEGTS_API
const GstMpegtsAtscVCT * gst_mpegts_section_get_atsc_tvct (GstMpegtsSection * section);

GST_MPEGTS_API
const GstMpegtsAtscVCT * gst_mpegts_section_get_atsc_cvct (GstMpegtsSection * section);

/* MGT */
#define GST_TYPE_MPEGTS_ATSC_MGT (gst_mpegts_atsc_mgt_get_type ())
#define GST_TYPE_MPEGTS_ATSC_MGT_TABLE (gst_mpegts_atsc_mgt_table_get_type ())

typedef struct _GstMpegtsAtscMGTTable GstMpegtsAtscMGTTable;
typedef struct _GstMpegtsAtscMGT GstMpegtsAtscMGT;

typedef enum {
  GST_MPEGTS_ATSC_MGT_TABLE_TYPE_EIT0 = 0x0100,
  GST_MPEGTS_ATSC_MGT_TABLE_TYPE_EIT127 = 0x017F,
  GST_MPEGTS_ATSC_MGT_TABLE_TYPE_ETT0 = 0x0200,
  GST_MPEGTS_ATSC_MGT_TABLE_TYPE_ETT127 = 0x027F
} GstMpegtsAtscMGTTableType;

/**
 * GstMpegtsAtscMGTTable:
 * @table_type: #GstMpegtsAtscMGTTableType
 * @pid: The packet ID
 * @version_number: The version number
 * @number_bytes:
 * @descriptors: (element-type GstMpegtsDescriptor): descriptors
 *
 * Source from a @GstMpegtsAtscMGT
 */
struct _GstMpegtsAtscMGTTable
{
  guint16 table_type;
  guint16 pid;
  guint8  version_number;
  guint32 number_bytes;
  GPtrArray *descriptors;
};

/**
 * GstMpegtsAtscMGT:
 * @protocol_version: The protocol version
 * @tables_defined: The numbers of subtables
 * @tables: (element-type GstMpegtsAtscMGTTable): the tables
 * @descriptors: (element-type GstMpegtsDescriptor): descriptors
 *
 * Master Guide Table (A65)
 *
 */
struct _GstMpegtsAtscMGT
{
  guint8  protocol_version;
  guint16 tables_defined;
  GPtrArray *tables;
  GPtrArray *descriptors;
};

GST_MPEGTS_API
GType gst_mpegts_atsc_mgt_get_type (void);

GST_MPEGTS_API
GType gst_mpegts_atsc_mgt_table_get_type (void);

GST_MPEGTS_API
const GstMpegtsAtscMGT * gst_mpegts_section_get_atsc_mgt (GstMpegtsSection * section);

GST_MPEGTS_API
GstMpegtsSection * gst_mpegts_section_from_atsc_mgt (GstMpegtsAtscMGT * mgt);

GST_MPEGTS_API
GstMpegtsAtscMGT * gst_mpegts_atsc_mgt_new (void);

/* Multiple string structure (used in ETT and EIT) */

#define GST_TYPE_MPEGTS_ATSC_STRING_SEGMENT (gst_mpegts_atsc_string_segment_get_type())
#define GST_TYPE_MPEGTS_ATSC_MULT_STRING (gst_mpegts_atsc_mult_string_get_type())

typedef struct _GstMpegtsAtscStringSegment GstMpegtsAtscStringSegment;
typedef struct _GstMpegtsAtscMultString GstMpegtsAtscMultString;

/**
 * GstMpegtsAtscStringSegment:
 * @compression_type: The compression type
 * @mode: The mode
 * @compressed_data_size: The size of compressed data
 * @compressed_data: The compressed data
 * @cached_string:
 *
 * A string segment
 */
struct _GstMpegtsAtscStringSegment {
  guint8 compression_type;
  guint8 mode;
  guint8 compressed_data_size;
  guint8 *compressed_data;

  gchar *cached_string;
};

GST_MPEGTS_API
const gchar * gst_mpegts_atsc_string_segment_get_string (GstMpegtsAtscStringSegment * seg);

GST_MPEGTS_API
gboolean
gst_mpegts_atsc_string_segment_set_string (GstMpegtsAtscStringSegment * seg,
                                           gchar *string,
                                           guint8 compression_type,
                                           guint8 mode);

/**
 * GstMpegtsAtscMultString:
 * @iso_639_langcode: The ISO639 language code
 * @segments: (element-type GstMpegtsAtscStringSegment)
 *
 */
struct _GstMpegtsAtscMultString {
  gchar      iso_639_langcode[4];
  GPtrArray *segments;
};

GST_MPEGTS_API
GType gst_mpegts_atsc_string_segment_get_type (void);

GST_MPEGTS_API
GType gst_mpegts_atsc_mult_string_get_type (void);

/* EIT */

#define GST_TYPE_MPEGTS_ATSC_EIT_EVENT (gst_mpegts_atsc_eit_event_get_type())
#define GST_TYPE_MPEGTS_ATSC_EIT (gst_mpegts_atsc_eit_get_type())

typedef struct _GstMpegtsAtscEITEvent GstMpegtsAtscEITEvent;
typedef struct _GstMpegtsAtscEIT GstMpegtsAtscEIT;

/**
 * GstMpegtsAtscEITEvent:
 * @event_id: The event id
 * @start_time: The start time
 * @etm_location: The etm location
 * @length_in_seconds: The length in seconds
 * @titles: (element-type GstMpegtsAtscMultString): the titles
 * @descriptors: (element-type GstMpegtsDescriptor): descriptors
 *
 * An ATSC EIT Event
 */
struct _GstMpegtsAtscEITEvent {
  guint16        event_id;
  guint32        start_time;
  guint8         etm_location;
  guint32        length_in_seconds;
  GPtrArray     *titles;

  GPtrArray *descriptors;
};

/**
 * GstMpegtsAtscEIT:
 * @source_id: The source id
 * @protocol_version: The protocol version
 * @events: (element-type GstMpegtsAtscEITEvent): Events
 *
 * Event Information Table (ATSC)
 *
 */
struct _GstMpegtsAtscEIT
{
  guint16        source_id;
  guint8         protocol_version;

  GPtrArray     *events;
};

GST_MPEGTS_API
GType gst_mpegts_atsc_eit_event_get_type (void);

GST_MPEGTS_API
GType gst_mpegts_atsc_eit_get_type (void);

GST_MPEGTS_API
const GstMpegtsAtscEIT *gst_mpegts_section_get_atsc_eit (GstMpegtsSection *section);

/* ETT */

#define GST_TYPE_MPEGTS_ATSC_ETT (gst_mpegts_atsc_ett_get_type())

typedef struct _GstMpegtsAtscETT GstMpegtsAtscETT;

/**
 * GstMpegtsAtscETT:
 * @ett_table_id_extension:
 * @protocol_version: The protocol version
 * @etm_id: The etm id
 * @messages: (element-type GstMpegtsAtscMultString): List of texts
 *
 * Extended Text Table (ATSC)
 *
 */
struct _GstMpegtsAtscETT
{
  guint16        ett_table_id_extension;
  guint16        protocol_version;
  guint32        etm_id;

  GPtrArray     *messages;
};

GST_MPEGTS_API
GType gst_mpegts_atsc_ett_get_type (void);

GST_MPEGTS_API
const GstMpegtsAtscETT *gst_mpegts_section_get_atsc_ett (GstMpegtsSection *section);

/* STT */
#define GST_TYPE_MPEGTS_ATSC_STT (gst_mpegts_atsc_stt_get_type ())

typedef struct _GstMpegtsAtscSTT GstMpegtsAtscSTT;

/**
 * GstMpegtsAtscSTT:
 * @protocol_version: The protocol version
 * @system_time: The system time
 * @gps_utc_offset: The GPS to UTC offset
 * @ds_status:
 * @ds_dayofmonth: The day of month
 * @ds_hour: The hour
 * @descriptors: (element-type GstMpegtsDescriptor): descriptors
 * @utc_datetime: The UTC date and time
 *
 * System Time Table (A65)
 *
 */
struct _GstMpegtsAtscSTT
{
  guint8     protocol_version;
  guint32    system_time;
  guint8     gps_utc_offset;
  gboolean   ds_status;
  guint8     ds_dayofmonth;
  guint8     ds_hour;
  GPtrArray *descriptors;

  GstDateTime *utc_datetime;
};

GST_MPEGTS_API
GType gst_mpegts_atsc_stt_get_type (void);

GST_MPEGTS_API
const GstMpegtsAtscSTT * gst_mpegts_section_get_atsc_stt (GstMpegtsSection * section);
/* FIXME receive a non-const parameter but we only provide a const getter */

GST_MPEGTS_API
GstDateTime * gst_mpegts_atsc_stt_get_datetime_utc (GstMpegtsAtscSTT * stt);

GST_MPEGTS_API
GstMpegtsSection * gst_mpegts_section_from_atsc_stt (GstMpegtsAtscSTT * stt);

GST_MPEGTS_API
GstMpegtsAtscSTT * gst_mpegts_atsc_stt_new (void);

/* RRT */
#define GST_TYPE_MPEGTS_ATSC_RRT (gst_mpegts_atsc_rrt_get_type ())
#define GST_TYPE_MPEGTS_ATSC_RRT_DIMENSION (gst_mpegts_atsc_rrt_dimension_get_type ())
#define GST_TYPE_MPEGTS_ATSC_RRT_DIMENSION_VALUE (gst_mpegts_atsc_rrt_dimension_value_get_type ())

typedef struct _GstMpegtsAtscRRT GstMpegtsAtscRRT;
typedef struct _GstMpegtsAtscRRTDimension GstMpegtsAtscRRTDimension;
typedef struct _GstMpegtsAtscRRTDimensionValue GstMpegtsAtscRRTDimensionValue;

/**
 * GstMpegtsAtscRRTDimensionValue:
 * @abbrev_ratings: (element-type GstMpegtsAtscMultString): the abbreviated ratings
 * @ratings: (element-type GstMpegtsAtscMultString): the ratings
 *
 * Since: 1.18
 */
struct _GstMpegtsAtscRRTDimensionValue
{
  GPtrArray *abbrev_ratings;
  GPtrArray *ratings;
};

/**
 * GstMpegtsAtscRRTDimension:
 * @names: (element-type GstMpegtsAtscMultString): the names
 * @graduated_scale: whether the ratings represent a graduated scale
 * @values_defined: the number of values defined for this dimension
 * @values: (element-type GstMpegtsAtscRRTDimensionValue): set of values
 *
 * Since: 1.18
 */
struct _GstMpegtsAtscRRTDimension
{
  GPtrArray * names;
  gboolean    graduated_scale;
  guint8      values_defined;
  GPtrArray * values;
};

/**
 * GstMpegtsAtscRRT:
 * @protocol_version: The protocol version
 * @names: (element-type GstMpegtsAtscMultString): the names
 * @dimensions_defined: the number of dimensions defined for this rating table
 * @dimensions: (element-type GstMpegtsAtscRRTDimension): A set of dimensions
 * @descriptors: descriptors
 *
 * Region Rating Table (A65)
 *
 * Since: 1.18
 */
struct _GstMpegtsAtscRRT
{
  guint8      protocol_version;
  GPtrArray * names;
  guint8      dimensions_defined;
  GPtrArray * dimensions;
  GPtrArray * descriptors;
};

GST_MPEGTS_API
GType gst_mpegts_atsc_rrt_get_type (void);

GST_MPEGTS_API
GType gst_mpegts_atsc_rrt_dimension_get_type (void);

GST_MPEGTS_API
GType gst_mpegts_atsc_rrt_dimension_value_get_type (void);

GST_MPEGTS_API
const GstMpegtsAtscRRT * gst_mpegts_section_get_atsc_rrt (GstMpegtsSection * section);

GST_MPEGTS_API
GstMpegtsSection * gst_mpegts_section_from_atsc_rrt (GstMpegtsAtscRRT * rrt);

GST_MPEGTS_API
GstMpegtsAtscRRT * gst_mpegts_atsc_rrt_new (void);

GST_MPEGTS_API
GstMpegtsAtscRRTDimension * gst_mpegts_atsc_rrt_dimension_new (void);

GST_MPEGTS_API
GstMpegtsAtscRRTDimensionValue * gst_mpegts_atsc_rrt_dimension_value_new (void);

G_END_DECLS

#endif				/* GST_MPEGTS_SECTION_H */
