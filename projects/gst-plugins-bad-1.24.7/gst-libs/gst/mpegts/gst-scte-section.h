/*
 * gst-scte-section.h -
 * Copyright (C) 2013, CableLabs, Louisville, CO 80027
 *           (c) 2019, Centricular ltd
 *
 * Authors:
 *   RUIH Team <ruih@cablelabs.com>
 *   Edward Hervey <edward@centricular.com>
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

#ifndef GST_SCTE_SECTION_H
#define GST_SCTE_SECTION_H

#include <gst/gst.h>
#include <gst/mpegts/gstmpegtssection.h>
#include <gst/mpegts/gstmpegtsdescriptor.h>

G_BEGIN_DECLS

/**
 * GstMpegtsScteStreamType:
 * @GST_MPEGTS_STREAM_TYPE_SCTE_SUBTITLING:  SCTE-27 Subtitling
 * @GST_MPEGTS_STREAM_TYPE_SCTE_ISOCH_DATA:  SCTE-19 Isochronous data
 * @GST_MPEGTS_STREAM_TYPE_SCTE_SIT:         SCTE-35 Splice Information Table
 * @GST_MPEGTS_STREAM_TYPE_SCTE_DST_NRT:     SCTE-07 Data Service or
 * Network Resource Table
 * @GST_MPEGTS_STREAM_TYPE_SCTE_DSMCC_DCB:   Type B - DSM-CC Data Carousel
 * [IEC 13818-6])
 * @GST_MPEGTS_STREAM_TYPE_SCTE_SIGNALING:   Enhanced Television Application
 * Signaling (OC-SP-ETV-AM1.0.1-120614)
 * @GST_MPEGTS_STREAM_TYPE_SCTE_SYNC_DATA:   SCTE-07 Synchronous data
 * @GST_MPEGTS_STREAM_TYPE_SCTE_ASYNC_DATA:  SCTE-53 Asynchronous data
 *
 * Type of mpeg-ts streams for SCTE. Most users would want to use the
 * #GstMpegtsATSCStreamType instead since it also covers these stream types
 *
 */
typedef enum {

  /* 0x01 - 0x7f : defined in other specs */
  GST_MPEGTS_STREAM_TYPE_SCTE_SUBTITLING = 0x82,   /* Subtitling data */
  GST_MPEGTS_STREAM_TYPE_SCTE_ISOCH_DATA = 0x83,   /* Isochronous data */
  /* 0x84 - 0x85 : defined in other specs */
  GST_MPEGTS_STREAM_TYPE_SCTE_SIT        = 0x86,   /* Splice Information Table */
  /* 0x87 - 0x94 : defined in other specs */
  GST_MPEGTS_STREAM_TYPE_SCTE_DST_NRT    = 0x95,   /* DST / NRT data */
  /* 0x96 - 0xaf : defined in other specs */
  GST_MPEGTS_STREAM_TYPE_SCTE_DSMCC_DCB  = 0xb0,   /* Data Carousel Type B */
  /* 0xb1 - 0xbf : User Private (or defined in other specs) */
  GST_MPEGTS_STREAM_TYPE_SCTE_SIGNALING  = 0xc0,   /* EBIF Signaling */
  GST_MPEGTS_STREAM_TYPE_SCTE_SYNC_DATA  = 0xc2,   /* Synchronous data */
  GST_MPEGTS_STREAM_TYPE_SCTE_ASYNC_DATA = 0xc3,   /* Asynchronous data */
  /* 0xc4 - 0xff : User Private (or defined in other specs) */

} GstMpegtsScteStreamType;


/**
 * GstMpegtsSectionSCTETableID:
 * @GST_MTS_TABLE_ID_SCTE_EAS:    SCTE-18 Emergency Alert System
 * @GST_MTS_TABLE_ID_SCTE_EBIF:   CL-SP-ETV-AM 1.0.1 EBIF message
 * @GST_MTS_TABLE_ID_SCTE_EISS:   CL-SP-ETV-AM 1.0.1 EBIF Int. Signaling Sect.
 * @GST_MTS_TABLE_ID_SCTE_DII:    CL-SP-ETV-AM 1.0.1 DSMCC DII message
 * @GST_MTS_TABLE_ID_SCTE_DDB:    CL-SP-ETV-AM 1.0.1 DSMCC Data Download Block
 * @GST_MTS_TABLE_ID_SCTE_SPLICE: SCTE-35 splice information is carried in a
 * section stream on a separate PID in the program’s Map Table (PMT) allowing
 * Splice Event notifications to remain associated with the program and pass
 * through multiplexers.
 *
 * Values for a #GstMpegtsSection table_id.
 *
 * These are the registered SCTE table_id variants.
 *
 * see also: #GstMpegtsSectionTableID
 */
typedef enum {

  /* 0x01 - 0xd7 : defined in other specs */
  GST_MTS_TABLE_ID_SCTE_EAS      = 0xd8,     /* emergency alert information */
  /* 0xd8 - 0xdf : defined in other specs */
  GST_MTS_TABLE_ID_SCTE_EBIF     = 0xE0,     /* EBIF message */
  GST_MTS_TABLE_ID_SCTE_RESERVED = 0xE1,
  GST_MTS_TABLE_ID_SCTE_EISS     = 0xE2,     /* EBIF Int. Signaling Sect. */
  GST_MTS_TABLE_ID_SCTE_DII      = 0xE3,     /* DSMCC DII message */
  GST_MTS_TABLE_ID_SCTE_DDB      = 0xE4,     /* DSMCC Data Download Block */
  /* 0xe5 - 0xfb : defined in other specs */
  GST_MTS_TABLE_ID_SCTE_SPLICE   = 0xfc,     /* splice information table */

} GstMpegtsSectionSCTETableID;

#define GST_MPEGTS_TYPE_SCTE_SPLICE_COMPONENT (gst_mpegts_scte_splice_component_get_type())
typedef struct _GstMpegtsSCTESpliceComponent GstMpegtsSCTESpliceComponent;

/**
 * GstMpegtsSCTESpliceComponent:
 * @tag: the elementary PID stream containing the Splice Point
 * @splice_time_specified: Whether @splice_time was specified
 * @splice_time: the presentation time of the signaled splice event
 * @utc_splice_time: The UTC time of the signaled splice event
 *
 * Per-PID splice information.
 *
 * Since: 1.20
 */
struct _GstMpegtsSCTESpliceComponent {
  guint8 tag;

  gboolean splice_time_specified; /* Only valid for insert_event */
  guint64 splice_time; /* Only valid for insert_event */

  guint32 utc_splice_time; /* Only valid for !insert_event (schedule) */
};

/* Splice Information Table (SIT) */
#define GST_TYPE_MPEGTS_SCTE_SPLICE_EVENT (gst_mpegts_scte_splice_event_get_type())
typedef struct _GstMpegtsSCTESpliceEvent GstMpegtsSCTESpliceEvent;

struct _GstMpegtsSCTESpliceEvent {
  /* TRUE if from/to an insert event (else belongs to a schedule event) */
  gboolean insert_event;

  guint32 splice_event_id;
  gboolean splice_event_cancel_indicator;

  /* If splice_event_cancel_indicator == 0 */
  gboolean out_of_network_indicator;
  gboolean program_splice_flag;
  gboolean duration_flag;

  gboolean splice_immediate_flag; /* Only valid for insert_event */

  gboolean program_splice_time_specified; /* Only valid for insert_event && program_splice */
  guint64 program_splice_time; /* Only valid for insert_event && program_splice */

  /**
   * GstMpegtsSCTESpliceEvent.utc_splice_time:
   *
   * The UTC time of the signaled splice event
   *
   * Since: 1.20
   */
  guint32 utc_splice_time; /* Only valid for !insert_event (schedule) && program_splice */

  /**
   * GstMpegtsSCTESpliceEvent.components:
   *
   * Per-PID splice time information
   *
   * Since: 1.20
   */
  GPtrArray *components; /* Only valid for !program_splice */

  gboolean break_duration_auto_return;
  guint64 break_duration;

  guint16 unique_program_id;
  guint8 avail_num;
  guint8 avails_expected;

};

/*
 * Types of descriptors
 *
 * Note: These are only for the descriptors *WITHIN* a SIT */
typedef enum {
  GST_MTS_SCTE_DESC_AVAIL        = 0x00,
  GST_MTS_SCTE_DESC_DTMF         = 0x01,
  GST_MTS_SCTE_DESC_SEGMENTATION = 0x02,
  GST_MTS_SCTE_DESC_TIME         = 0x03,
  GST_MTS_SCTE_DESC_AUDIO        = 0x04
} GstMpegtsSCTESpliceDescriptor;

typedef enum {
  GST_MTS_SCTE_SPLICE_COMMAND_NULL      = 0x00,
  GST_MTS_SCTE_SPLICE_COMMAND_SCHEDULE  = 0x04,
  GST_MTS_SCTE_SPLICE_COMMAND_INSERT    = 0x05,
  GST_MTS_SCTE_SPLICE_COMMAND_TIME      = 0x06,
  GST_MTS_SCTE_SPLICE_COMMAND_BANDWIDTH = 0x07,
  GST_MTS_SCTE_SPLICE_COMMAND_PRIVATE   = 0xff
} GstMpegtsSCTESpliceCommandType;

#define GST_TYPE_MPEGTS_SCTE_SIT (gst_mpegts_scte_sit_get_type())

typedef struct _GstMpegtsSCTESIT GstMpegtsSCTESIT;

struct _GstMpegtsSCTESIT
{
  gboolean encrypted_packet;
  guint8   encryption_algorithm;

  guint64  pts_adjustment;
  guint8   cw_index;
  guint16  tier;

  guint16  splice_command_length;

  GstMpegtsSCTESpliceCommandType splice_command_type;

  /* For time_signal commands */
  gboolean splice_time_specified;
  guint64  splice_time;

  GPtrArray *splices;

  GPtrArray *descriptors;

  /**
   * GstMpegtsSCTESIT.fully_parsed:
   *
   * When encrypted, or when encountering an unknown command type,
   * we may still want to pass the sit through.
   *
   * Since: 1.20
   */
  gboolean fully_parsed;

  /**
   * GstMpegtsSCTESIT.is_running_time:
   *
   * When the SIT was constructed by the application, splice times
   * are in running_time and must be translated before packetizing.
   *
   * Since: 1.20
   */
  gboolean is_running_time;
};

GST_MPEGTS_API
GType gst_mpegts_scte_sit_get_type (void);

GST_MPEGTS_API
GstMpegtsSCTESIT *gst_mpegts_scte_sit_new (void);

GST_MPEGTS_API
GstMpegtsSCTESIT *gst_mpegts_scte_null_new (void);

GST_MPEGTS_API
GstMpegtsSCTESIT *gst_mpegts_scte_cancel_new (guint32 event_id);

GST_MPEGTS_API
GstMpegtsSCTESIT *gst_mpegts_scte_splice_in_new (guint32 event_id,
						 GstClockTime splice_time);

GST_MPEGTS_API
GstMpegtsSCTESIT *gst_mpegts_scte_splice_out_new (guint32 event_id,
						  GstClockTime splice_time,
						  GstClockTime duration);


GST_MPEGTS_API
GType gst_mpegts_scte_splice_event_get_type (void);

GST_MPEGTS_API
GstMpegtsSCTESpliceEvent *gst_mpegts_scte_splice_event_new (void);

GST_MPEGTS_API
const GstMpegtsSCTESIT *gst_mpegts_section_get_scte_sit (GstMpegtsSection *section);

GST_MPEGTS_API
GstMpegtsSection *gst_mpegts_section_from_scte_sit (GstMpegtsSCTESIT * sit, guint16 pid);

GST_MPEGTS_API
GType gst_mpegts_scte_splice_component_get_type (void);

GST_MPEGTS_API
GstMpegtsSCTESpliceComponent *gst_mpegts_scte_splice_component_new (guint8 tag);


G_END_DECLS

#endif  /* GST_SCTE_SECTION_H */
