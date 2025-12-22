/* GStreamer
 * Copyright (C) 2008-2009 Sebastian Dröge <sebastian.droege@collabora.co.uk>
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

#ifndef __MXF_DEMUX_H__
#define __MXF_DEMUX_H__

#include <gst/gst.h>
#include <gst/base/gstadapter.h>
#include <gst/base/gstflowcombiner.h>
#include <gst/video/video.h>

#include "mxfessence.h"

G_BEGIN_DECLS

#define GST_TYPE_MXF_DEMUX \
  (gst_mxf_demux_get_type())
#define GST_MXF_DEMUX(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_MXF_DEMUX,GstMXFDemux))
#define GST_MXF_DEMUX_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_MXF_DEMUX,GstMXFDemuxClass))
#define GST_IS_MXF_DEMUX(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_MXF_DEMUX))
#define GST_IS_MXF_DEMUX_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_MXF_DEMUX))
typedef struct _GstMXFDemux GstMXFDemux;
typedef struct _GstMXFDemuxClass GstMXFDemuxClass;

#define GST_TYPE_MXF_DEMUX_PAD (gst_mxf_demux_pad_get_type())
#define GST_MXF_DEMUX_PAD(pad) (G_TYPE_CHECK_INSTANCE_CAST((pad),GST_TYPE_MXF_DEMUX_PAD,GstMXFDemuxPad))
#define GST_MXF_DEMUX_PAD_CAST(pad) ((GstMXFDemuxPad *) pad)
#define GST_IS_MXF_DEMUX_PAD(pad) (G_TYPE_CHECK_INSTANCE_TYPE((pad),GST_TYPE_MXF_DEMUX_PAD))
typedef struct _GstMXFDemuxPad GstMXFDemuxPad;
typedef struct _GstMXFDemuxPadClass GstMXFDemuxPadClass;


/*
 * GstMXFKLV is used to pass around information about a KLV.
 *
 * It optionally contains the content of the klv (data field).
 */
typedef struct {
  MXFUL key;
  guint64 offset;               /* absolute offset of K */
  gsize length;                 /* Size of data (i.e. V) */
  guint64 data_offset;          /* relative offset of data (i.e. size of 'KL') */
  GstBuffer *data;              /* Can be NULL in pull-mode. */

  /* For partial reads (ex: clip/custom wrapping essence), the amount of data
   * already consumed within. If 0, all of length+data_offset was consumed */
  guint64 consumed;
} GstMXFKLV;


typedef enum {
  GST_MXF_DEMUX_STATE_UNKNOWN,	/* Still looking for run-in/klv */
  GST_MXF_DEMUX_STATE_KLV,	/* Next read/fetch is a KLV */
  GST_MXF_DEMUX_STATE_ESSENCE	/* Next read/fetch is within a KLV (i.e. non-frame-wrapped) */
} GstMXFDemuxState;

typedef struct _GstMXFDemuxPartition GstMXFDemuxPartition;
typedef struct _GstMXFDemuxEssenceTrack GstMXFDemuxEssenceTrack;

struct _GstMXFDemuxPartition
{
  MXFPartitionPack partition;
  MXFPrimerPack primer;
  gboolean parsed_metadata;

  /* Relative offset at which essence starts within this partition.
   *
   * For Frame wrapping, the position of the first KLV
   * For Clip/Custom wrapping, the position of the first byte of essence in the KLV
   **/
  guint64 essence_container_offset;

  /* If the partition contains a single essence track, point to it */
  GstMXFDemuxEssenceTrack *single_track;

  /* For clip-based wrapping, the essence KLV */
  GstMXFKLV clip_klv;
};

#define MXF_INDEX_DELTA_ID_UNKNOWN -1
#define MXF_INDEX_DELTA_ID_IGNORE -2

struct _GstMXFDemuxEssenceTrack
{
  guint32 body_sid;
  guint32 index_sid;
  guint32 track_number;

  /* delta id, the position of this track in the container package delta table
   * (if the track is in an interleaved essence container)
   *
   * Special values:
   * * -1 Not discovered yet
   * * -2 Ignore delta entry (if index table is not present or not complete)
   */
  gint32 delta_id;

  guint32 track_id;
  MXFUMID source_package_uid;

  /* Position and duration in edit units */
  gint64 position;
  gint64 duration;

  GArray *offsets;

  MXFMetadataSourcePackage *source_package;
  MXFMetadataTimelineTrack *source_track;

  gpointer mapping_data;
  const MXFEssenceElementHandler *handler;
  MXFEssenceElementHandleFunc handle_func;

  GstTagList *tags;

  GstCaps *caps;
  gboolean intra_only;

  MXFEssenceWrapping wrapping;

  /* Minimum number of edit unit to send in one go.
   * Default : 1
   * Used for raw audio track */
  guint min_edit_units;
};

typedef struct
{
  /* absolute byte offset excluding run_in, 0 if uninitialized */
  guint64 offset;

  /* PTS edit unit number or G_MAXUINT64 */
  guint64 pts;

  /* DTS edit unit number if we got here via PTS */
  guint64 dts;

  /* Duration in edit units */
  guint64 duration;

  gboolean keyframe;
  gboolean initialized;

  /* Size, used for non-frame-wrapped content */
  guint64 size;
} GstMXFDemuxIndex;

typedef struct
{
  guint32 body_sid;
  guint32 index_sid;

  /* Array of MXFIndexTableSegment, sorted by DTS
   * Note: Can be empty and can be sparse (i.e. not cover every edit unit) */
  GArray *segments;

  /* Delta entry to which reordering should be applied (-1 == no reordering) */
  gint reordered_delta_entry;

  /* Array of gint8 reverse temporal offsets.
   * Contains the shift to apply to an entry DTS to get the PTS
   *
   * Can be NULL if the content doesn't have temporal shifts (i.e. all present
   * entries have a temporal offset of 0) */
  GArray *reverse_temporal_offsets;

  /* Greatest temporal offset value contained within offsets.
   * Unsigned because the smallest value is 0 (no reordering)  */
  guint max_temporal_offset;
} GstMXFDemuxIndexTable;

struct _GstMXFDemuxPad
{
  GstPad parent;

  guint32 track_id;
  gboolean need_segment;

  GstClockTime position;
  gdouble position_accumulated_error;
  /* Current position in the material track (in edit units) */
  gint64 current_material_track_position;

  gboolean eos, discont;

  GstTagList *tags;

  MXFMetadataGenericPackage *material_package;
  MXFMetadataTimelineTrack *material_track;

  GstVideoTimeCode start_timecode;

  guint current_component_index;
  MXFMetadataSourceClip *current_component;

  /* Position in the material track where this component started */
  gint64 current_component_start_position;

  /* Position/duration in the source track */
  gint64 current_component_start;
  gint64 current_component_duration;

  /* Current essence track and position (in edit units) */
  GstMXFDemuxEssenceTrack *current_essence_track;
  gint64 current_essence_track_position;
};

struct _GstMXFDemuxPadClass
{
  GstPadClass parent;
};

struct _GstMXFDemux
{
  GstElement element;

  GstPad *sinkpad;
  GPtrArray *src;

  /* < private > */
  GstMXFDemuxState state;

  gboolean have_group_id;
  guint group_id;

  GstAdapter *adapter;

  GstFlowCombiner *flowcombiner;

  GstSegment segment;
  guint32 seqnum;

  GstEvent *close_seg_event;

  guint64 offset;

  gboolean random_access;
  gboolean flushing;

  guint64 run_in;

  guint64 header_partition_pack_offset;
  guint64 footer_partition_pack_offset;

  /* MXF file state */
  GList *partitions;
  GstMXFDemuxPartition *current_partition;

  GPtrArray *essence_tracks;

  GList *pending_index_table_segments;
  GList *index_tables; /* one per BodySID / IndexSID */
  gboolean index_table_segments_collected;

  GArray *random_index_pack;

  /* Metadata */
  GRWLock metadata_lock;
  gboolean update_metadata;
  gboolean pull_footer_metadata;

  gboolean metadata_resolved;
  MXFMetadataPreface *preface;
  GHashTable *metadata;

  /* Current Material Package */
  MXFUMID current_package_uid;
  MXFMetadataGenericPackage *current_package;
  gchar *current_package_string;

  GstTagList *tags;

  /* Properties */
  gchar *requested_package_string;
  GstClockTime max_drift;

  /* Quirks */
  gboolean temporal_order_misuse;
};

struct _GstMXFDemuxClass
{
  GstElementClass parent_class;
};

GType gst_mxf_demux_get_type (void);

G_END_DECLS

#endif /* __MXF_DEMUX_H__ */
