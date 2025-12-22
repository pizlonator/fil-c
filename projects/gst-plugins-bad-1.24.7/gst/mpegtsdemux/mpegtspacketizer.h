/*
 * mpegtspacketizer.h - 
 * Copyright (C) 2007 Alessandro Decina
 * 
 * Authors:
 *   Alessandro Decina <alessandro@nnva.org>
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

#ifndef GST_MPEGTS_PACKETIZER_H
#define GST_MPEGTS_PACKETIZER_H

#include <gst/gst.h>
#include <gst/base/gstadapter.h>
#include <glib.h>

#include <gst/mpegts/mpegts.h>
#include "gstmpegdefs.h"

#define MPEGTS_NORMAL_PACKETSIZE  188
#define MPEGTS_M2TS_PACKETSIZE    192
#define MPEGTS_DVB_ASI_PACKETSIZE 204
#define MPEGTS_ATSC_PACKETSIZE    208

#define MPEGTS_MIN_PACKETSIZE MPEGTS_NORMAL_PACKETSIZE
#define MPEGTS_MAX_PACKETSIZE MPEGTS_ATSC_PACKETSIZE

#define MPEGTS_AFC_DISCONTINUITY_FLAG           0x80
#define MPEGTS_AFC_RANDOM_ACCESS_FLAG           0x40
#define MPEGTS_AFC_ELEMENTARY_STREAM_PRIORITY   0x20
#define MPEGTS_AFC_PCR_FLAG                     0x10
#define MPEGTS_AFC_OPCR_FLAG                    0x08
#define MPEGTS_AFC_SPLICING_POINT_FLAG          0x04
#define MPEGTS_AFC_TRANSPORT_PRIVATE_DATA_FLAG  0x02
#define MPEGTS_AFC_EXTENSION_FLAG               0x01

#define MAX_WINDOW 512

G_BEGIN_DECLS

#define GST_TYPE_MPEGTS_PACKETIZER \
  (mpegts_packetizer_get_type())
#define GST_MPEGTS_PACKETIZER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_MPEGTS_PACKETIZER,MpegTSPacketizer2))
#define GST_MPEGTS_PACKETIZER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_MPEGTS_PACKETIZER,MpegTSPacketizer2Class))
#define GST_IS_MPEGTS_PACKETIZER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_MPEGTS_PACKETIZER))
#define GST_IS_MPEGTS_PACKETIZER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_MPEGTS_PACKETIZER))

typedef struct _MpegTSPacketizer2 MpegTSPacketizer2;
typedef struct _MpegTSPacketizer2Class MpegTSPacketizer2Class;

typedef struct
{
  guint16 pid;
  guint   continuity_counter;

  /* Section data (always newly allocated) */
  guint8 *section_data;
  /* Current offset in section_data */
  guint16 section_offset;

  /* Values for pending section */
  /* table_id of the pending section_data */
  guint8  table_id;
  guint   section_length;
  guint8  version_number;
  guint16 subtable_extension;
  guint8  section_number;
  guint8  last_section_number;

  GSList *subtables;

  /* Upstream offset of the data contained in the section */
  guint64 offset;
} MpegTSPacketizerStream;

/* Maximum number of MpegTSPcr
 * 256 should be sufficient for most multiplexes */
#define MAX_PCR_OBS_CHANNELS 256

/* PCR/offset structure */
typedef struct _PCROffset
{
  /* PCR value (units: 1/27MHz) */
  guint64 pcr;

  /* The offset (units: bytes) */
  guint64 offset;
} PCROffset;

/* Flags used on groups */
enum
{
  /* Closed groups: There is a contiguous next group */
  PCR_GROUP_FLAG_CLOSED = 1 << 0,
  /* estimated: the pcr_offset has been estimated and is not
   * guaranteed to be 100% accurate */
  PCR_GROUP_FLAG_ESTIMATED = 1 << 1,
  /* reset: there is a pcr reset between the end of this
   * group and the next one.
   * This flag is exclusive with CLOSED. */
  PCR_GROUP_FLAG_RESET = 1 << 2,
  /* reset: there is a pcr wrapover between the end of this
   * group and the next one.
   * This flag is exclusive with CLOSED. */
  PCR_GROUP_FLAG_WRAPOVER = 1 << 3
};



/* PCROffsetGroup: A group of PCR observations.
 * All values in a group have got the same reference pcr and
 * byte offset (first_pcr/first_offset).
 */
#define DEFAULT_ALLOCATED_OFFSET 16
typedef struct _PCROffsetGroup
{
  /* Flags (see PCR_GROUP_FLAG_* above) */
  guint flags;

  /* First raw PCR of this group. Units: 1/27MHz.
   * All values[].pcr are differences against first_pcr */
  guint64 first_pcr;
  /* Offset of this group in bytes.
   * All values[].offset are differences against first_offset */
  guint64 first_offset;

  /* Dynamically allocated table of PCROffset */
  PCROffset *values;
  /* number of PCROffset allocated in values */
  guint nb_allocated;
  /* number of *actual* PCROffset contained in values */
  guint last_value;

  /* Offset since the very first PCR value observed in the whole
   * stream. Units: 1/27MHz.
   * This will take into account gaps/wraparounds/resets/... and is
   * used to determine running times.
   * The value is only guaranteed to be 100% accurate if the group
   * does not have the ESTIMATED flag.
   * If the value is estimated, the pcr_offset shall be recalculated
   * (based on previous groups) whenever it is accessed.
   */
  guint64 pcr_offset;

  /* FIXME : Cache group bitrate ? */
} PCROffsetGroup;

/* Number of PCRs needed before bitrate estimation can start */
/* Note: the reason we use 10 is because PCR should normally be
 * received at least every 100ms so this gives us close to
 * a 1s moving window to calculate bitrate */
#define PCR_BITRATE_NEEDED 10

/* PCROffsetCurrent: The PCR/Offset window iterator
 * This is used to estimate/observe incoming PCR/offset values
 * Points to a group (which it is filling) */
typedef struct _PCROffsetCurrent
{
  /* The PCROffsetGroup we are filling.
   * If NULL, a group needs to be identified */
  PCROffsetGroup *group;

  /* Table of pending values we are iterating over */
  PCROffset pending[PCR_BITRATE_NEEDED];

  /* base offset/pcr from the group */
  guint64 first_pcr;
  guint64 first_offset;

  /* The previous reference PCROffset
   * This corresponds to the last entry of the group we are filling
   * and is used to calculate prev_bitrate */
  PCROffset prev;

  /* The last PCROffset in pending[] */
  PCROffset last_value;

  /* Location of first pending PCR/offset observation in pending */
  guint first;
  /* Location of last pending PCR/offset observation in pending */
  guint last;
  /* Location of next write in pending */
  guint write;

  /* bitrate is always in bytes per second */

  /* cur_bitrate is the bitrate of the pending values: d(last-first) */
  guint64 cur_bitrate;

  /* prev_bitrate is the bitrate between reference PCROffset
   * and the first pending value. Used to detect changes
   * in bitrate */
  guint64 prev_bitrate;
} PCROffsetCurrent;

typedef struct _MpegTSPCR
{
  guint16 pid;

  /* Following variables are only active/used when
   * calculate_skew is TRUE */
  GstClockTime base_time;
  GstClockTime base_pcrtime;
  GstClockTime prev_out_time;
  GstClockTime prev_in_time;
  GstClockTime last_pcrtime;
  gint64 window[MAX_WINDOW];
  guint window_pos;
  guint window_size;
  gboolean window_filling;
  gint64 window_min;
  gint64 skew;
  gint64 prev_send_diff;

  /* Offset to apply to PCR to handle wraparounds */
  guint64 pcroffset;

  /* Used for bitrate calculation */
  /* List of PCR/offset observations */
  GList *groups;

  /* Current PCR/offset observations (used to update pcroffsets) */
  PCROffsetCurrent *current;
} MpegTSPCR;

struct _MpegTSPacketizer2 {
  GObject     parent;

  GMutex group_lock;

  GstAdapter *adapter;
  /* streams hashed by pid */
  /* FIXME : be more memory efficient (see how it's done in mpegtsbase) */
  MpegTSPacketizerStream **streams;
  gboolean    disposed;
  guint16     packet_size;

  /* current offset of the tip of the adapter */
  guint64  offset;
  gboolean empty;

  /* clock skew calculation */
  gboolean       calculate_skew;

  /* offset/bitrate calculator */
  gboolean       calculate_offset;

  /* Shortcuts for adapter usage */
  guint8 *map_data;
  gsize map_offset;
  gsize map_size;
  gboolean need_sync;

  /* Reference offset */
  guint64 refoffset;

  /* Number of seen pcr/offset observations (FIXME : kill later) */
  guint nb_seen_offsets;

  /* Last inputted timestamp */
  GstClockTime last_in_time;

  /* offset to observations table */
  guint8 pcrtablelut[0x2000];
  MpegTSPCR *observations[MAX_PCR_OBS_CHANNELS];
  guint8 lastobsid;
  GstClockTime pcr_discont_threshold;

  /* PTS/DTS of last buffer */
  GstClockTime last_pts;
  GstClockTime last_dts;

  /* Extra time offset to handle values before initial PCR.
   * This will be added to all converted timestamps */
  GstClockTime extra_shift;
};

struct _MpegTSPacketizer2Class {
  GObjectClass object_class;
};

#define FLAGS_SCRAMBLED(f) (f & 0xc0)
#define FLAGS_HAS_AFC(f) (f & 0x20)
#define FLAGS_HAS_PAYLOAD(f) (f & 0x10)
#define FLAGS_CONTINUITY_COUNTER(f) (f & 0x0f)

typedef struct
{
  gint16  pid;
  guint8  payload_unit_start_indicator;
  guint8  scram_afc_cc;
  guint8 *payload;

  guint8 *data_start;
  guint8 *data_end;
  guint8 *data;

  guint8  afc_flags;
  guint64 pcr;
  guint64 offset;
} MpegTSPacketizerPacket;

typedef struct
{
  guint8 table_id;
  /* the spec says sub_table_extension is the fourth and fifth byte of a 
   * section when the section_syntax_indicator is set to a value of "1". If 
   * section_syntax_indicator is 0, sub_table_extension will be set to 0 */
  guint16  subtable_extension;
  guint8   version_number;
  guint8   last_section_number;
  /* table of bits, whether the section was seen or not.
   * Use MPEGTS_BIT_* macros to check */
  /* Size is 32, because there's a maximum of 256 (32*8) section_number */
  guint8   seen_section[32];
} MpegTSPacketizerStreamSubtable;

#define MPEGTS_BIT_SET(field, offs)    ((field)[(offs) >> 3] |=  (1 << ((offs) & 0x7)))
#define MPEGTS_BIT_UNSET(field, offs)  ((field)[(offs) >> 3] &= ~(1 << ((offs) & 0x7)))
#define MPEGTS_BIT_IS_SET(field, offs) ((field)[(offs) >> 3] &   (1 << ((offs) & 0x7)))

typedef enum {
  PACKET_BAD       = FALSE,
  PACKET_OK        = TRUE,
  PACKET_NEED_MORE
} MpegTSPacketizerPacketReturn;

G_GNUC_INTERNAL GType mpegts_packetizer_get_type(void);

G_GNUC_INTERNAL MpegTSPacketizer2 *mpegts_packetizer_new (void);
G_GNUC_INTERNAL void mpegts_packetizer_clear (MpegTSPacketizer2 *packetizer);
G_GNUC_INTERNAL void mpegts_packetizer_flush (MpegTSPacketizer2 *packetizer, gboolean hard);
G_GNUC_INTERNAL void mpegts_packetizer_push (MpegTSPacketizer2 *packetizer, GstBuffer *buffer);
G_GNUC_INTERNAL gboolean mpegts_packetizer_has_packets (MpegTSPacketizer2 *packetizer);
G_GNUC_INTERNAL MpegTSPacketizerPacketReturn mpegts_packetizer_next_packet (MpegTSPacketizer2 *packetizer,
  MpegTSPacketizerPacket *packet);
G_GNUC_INTERNAL MpegTSPacketizerPacketReturn
mpegts_packetizer_process_next_packet(MpegTSPacketizer2 * packetizer);
G_GNUC_INTERNAL void mpegts_packetizer_clear_packet (MpegTSPacketizer2 *packetizer,
				     MpegTSPacketizerPacket *packet);
G_GNUC_INTERNAL void mpegts_packetizer_remove_stream(MpegTSPacketizer2 *packetizer,
  gint16 pid);

G_GNUC_INTERNAL GstMpegtsSection *mpegts_packetizer_push_section (MpegTSPacketizer2 *packetzer,
								  MpegTSPacketizerPacket *packet, GList **remaining);

/* Only valid if calculate_offset is TRUE */
G_GNUC_INTERNAL GstClockTime
mpegts_packetizer_offset_to_ts (MpegTSPacketizer2 * packetizer,
				guint64 offset, guint16 pcr_pid);
G_GNUC_INTERNAL guint64
mpegts_packetizer_ts_to_offset (MpegTSPacketizer2 * packetizer,
				GstClockTime ts, guint16 pcr_pid);
G_GNUC_INTERNAL GstClockTime
mpegts_packetizer_pts_to_ts (MpegTSPacketizer2 * packetizer,
			     GstClockTime pts, guint16 pcr_pid);
G_GNUC_INTERNAL GstClockTime
mpegts_packetizer_pts_to_ts_unchecked (MpegTSPacketizer2 * packetizer,
			     GstClockTime pts, guint16 pcr_pid);
G_GNUC_INTERNAL GstClockTime
mpegts_packetizer_get_current_time (MpegTSPacketizer2 * packetizer,
				    guint16 pcr_pid);
G_GNUC_INTERNAL void
mpegts_packetizer_set_current_pcr_offset (MpegTSPacketizer2 * packetizer,
			  GstClockTime offset, guint16 pcr_pid);
G_GNUC_INTERNAL void
mpegts_packetizer_set_reference_offset (MpegTSPacketizer2 * packetizer,
					guint64 refoffset);
G_GNUC_INTERNAL void
mpegts_packetizer_set_pcr_discont_threshold (MpegTSPacketizer2 * packetizer,
					GstClockTime threshold);
G_END_DECLS

#endif /* GST_MPEGTS_PACKETIZER_H */
