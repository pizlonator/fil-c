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

/**
 * SECTION:element-mxfdemux
 * @title: mxfdemux
 *
 * mxfdemux demuxes an MXF file into the different contained streams.
 *
 * ## Example launch line
 * |[
 * gst-launch-1.0 -v filesrc location=/path/to/mxf ! mxfdemux ! audioconvert ! autoaudiosink
 * ]| This pipeline demuxes an MXF file and outputs one of the contained raw audio streams.
 *
 */

/* TODO:
 *   - Handle timecode tracks correctly (where is this documented?)
 *   - Handle drop-frame field of timecode tracks
 *   - Handle Generic container system items
 *   - Post structural metadata and descriptive metadata trees as a message on the bus
 *     and send them downstream as event.
 *   - Multichannel audio needs channel layouts, define them (SMPTE S320M?).
 *   - Correctly handle the different rectangles and aspect-ratio for video
 *   - Add more support for non-standard MXF used by Avid (bug #561922).
 *   - Fix frame layout stuff, i.e. interlaced/progressive
 *   - In pull mode first find the first buffer for every pad before pushing
 *     to prevent jumpy playback in the beginning due to resynchronization.
 *
 *   - Implement SMPTE D11 essence and the digital cinema/MXF specs
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstmxfelements.h"
#include "mxfdemux.h"
#include "mxfessence.h"

#include <string.h>

static GstStaticPadTemplate mxf_sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/mxf")
    );

static GstStaticPadTemplate mxf_src_template =
GST_STATIC_PAD_TEMPLATE ("track_%u",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS_ANY);

GST_DEBUG_CATEGORY_STATIC (mxfdemux_debug);
#define GST_CAT_DEFAULT mxfdemux_debug

/* Fill klv for the given offset, does not download the data */
static GstFlowReturn
gst_mxf_demux_peek_klv_packet (GstMXFDemux * demux, guint64 offset,
    GstMXFKLV * klv);

/* Ensures the klv data is present. Pulls it if needed */
static GstFlowReturn
gst_mxf_demux_fill_klv (GstMXFDemux * demux, GstMXFKLV * klv);

/* Call when done with a klv. Will release the buffer (if any) and will update
 * the demuxer offset position */
static void gst_mxf_demux_consume_klv (GstMXFDemux * demux, GstMXFKLV * klv);

static GstFlowReturn
gst_mxf_demux_handle_index_table_segment (GstMXFDemux * demux, GstMXFKLV * klv);

static void collect_index_table_segments (GstMXFDemux * demux);
static gboolean find_entry_for_offset (GstMXFDemux * demux,
    GstMXFDemuxEssenceTrack * etrack, guint64 offset,
    GstMXFDemuxIndex * retentry);

GType gst_mxf_demux_pad_get_type (void);
G_DEFINE_TYPE (GstMXFDemuxPad, gst_mxf_demux_pad, GST_TYPE_PAD);

static void
gst_mxf_demux_pad_finalize (GObject * object)
{
  GstMXFDemuxPad *pad = GST_MXF_DEMUX_PAD (object);

  if (pad->tags) {
    gst_tag_list_unref (pad->tags);
    pad->tags = NULL;
  }

  G_OBJECT_CLASS (gst_mxf_demux_pad_parent_class)->finalize (object);
}

static void
gst_mxf_demux_pad_class_init (GstMXFDemuxPadClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->finalize = gst_mxf_demux_pad_finalize;
}

static void
gst_mxf_demux_pad_init (GstMXFDemuxPad * pad)
{
  pad->position = 0;
  pad->current_material_track_position = 0;
}

#define DEFAULT_MAX_DRIFT 100 * GST_MSECOND

enum
{
  PROP_0,
  PROP_PACKAGE,
  PROP_MAX_DRIFT,
  PROP_STRUCTURE
};

static gboolean gst_mxf_demux_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event);
static gboolean gst_mxf_demux_src_event (GstPad * pad, GstObject * parent,
    GstEvent * event);
static gboolean gst_mxf_demux_src_query (GstPad * pad, GstObject * parent,
    GstQuery * query);

#define gst_mxf_demux_parent_class parent_class
G_DEFINE_TYPE (GstMXFDemux, gst_mxf_demux, GST_TYPE_ELEMENT);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (mxfdemux, "mxfdemux", GST_RANK_PRIMARY,
    GST_TYPE_MXF_DEMUX, mxf_element_init (plugin));

static void
gst_mxf_demux_remove_pad (GstMXFDemuxPad * pad, GstMXFDemux * demux)
{
  gst_flow_combiner_remove_pad (demux->flowcombiner, GST_PAD_CAST (pad));
  gst_element_remove_pad (GST_ELEMENT (demux), GST_PAD_CAST (pad));
}

static void
gst_mxf_demux_remove_pads (GstMXFDemux * demux)
{
  g_ptr_array_foreach (demux->src, (GFunc) gst_mxf_demux_remove_pad, demux);
  g_ptr_array_foreach (demux->src, (GFunc) gst_object_unref, NULL);
  g_ptr_array_set_size (demux->src, 0);
}

static void
gst_mxf_demux_partition_free (GstMXFDemuxPartition * partition)
{
  mxf_partition_pack_reset (&partition->partition);
  mxf_primer_pack_reset (&partition->primer);

  g_free (partition);
}

static void
gst_mxf_demux_essence_track_free (GstMXFDemuxEssenceTrack * t)
{
  if (t->offsets)
    g_array_free (t->offsets, TRUE);

  g_free (t->mapping_data);

  if (t->tags)
    gst_tag_list_unref (t->tags);

  if (t->caps)
    gst_caps_unref (t->caps);

  g_free (t);
}

static void
gst_mxf_demux_reset_mxf_state (GstMXFDemux * demux)
{
  GST_DEBUG_OBJECT (demux, "Resetting MXF state");

  g_list_foreach (demux->partitions, (GFunc) gst_mxf_demux_partition_free,
      NULL);
  g_list_free (demux->partitions);
  demux->partitions = NULL;

  demux->current_partition = NULL;
  g_ptr_array_set_size (demux->essence_tracks, 0);
}

static void
gst_mxf_demux_reset_linked_metadata (GstMXFDemux * demux)
{
  guint i;

  for (i = 0; i < demux->src->len; i++) {
    GstMXFDemuxPad *pad = g_ptr_array_index (demux->src, i);

    pad->material_track = NULL;
    pad->material_package = NULL;
    pad->current_component = NULL;
  }

  for (i = 0; i < demux->essence_tracks->len; i++) {
    GstMXFDemuxEssenceTrack *track =
        g_ptr_array_index (demux->essence_tracks, i);

    track->source_package = NULL;
    track->delta_id = -1;
    track->source_track = NULL;
  }

  demux->current_package = NULL;
}

static void
gst_mxf_demux_reset_metadata (GstMXFDemux * demux)
{
  GST_DEBUG_OBJECT (demux, "Resetting metadata");

  g_rw_lock_writer_lock (&demux->metadata_lock);

  demux->update_metadata = TRUE;
  demux->metadata_resolved = FALSE;

  gst_mxf_demux_reset_linked_metadata (demux);

  demux->preface = NULL;

  if (demux->metadata) {
    g_hash_table_destroy (demux->metadata);
  }
  demux->metadata = mxf_metadata_hash_table_new ();

  if (demux->tags) {
    gst_tag_list_unref (demux->tags);
    demux->tags = NULL;
  }

  g_rw_lock_writer_unlock (&demux->metadata_lock);
}

static void
gst_mxf_demux_reset (GstMXFDemux * demux)
{
  GST_DEBUG_OBJECT (demux, "cleaning up MXF demuxer");

  demux->flushing = FALSE;

  demux->state = GST_MXF_DEMUX_STATE_UNKNOWN;

  demux->footer_partition_pack_offset = 0;
  demux->offset = 0;

  demux->pull_footer_metadata = TRUE;

  demux->run_in = -1;

  memset (&demux->current_package_uid, 0, sizeof (MXFUMID));

  gst_segment_init (&demux->segment, GST_FORMAT_TIME);

  if (demux->close_seg_event) {
    gst_event_unref (demux->close_seg_event);
    demux->close_seg_event = NULL;
  }

  gst_adapter_clear (demux->adapter);

  gst_mxf_demux_remove_pads (demux);

  if (demux->random_index_pack) {
    g_array_free (demux->random_index_pack, TRUE);
    demux->random_index_pack = NULL;
  }

  if (demux->pending_index_table_segments) {
    GList *l;

    for (l = demux->pending_index_table_segments; l; l = l->next) {
      MXFIndexTableSegment *s = l->data;
      mxf_index_table_segment_reset (s);
      g_free (s);
    }
    g_list_free (demux->pending_index_table_segments);
    demux->pending_index_table_segments = NULL;
  }

  if (demux->index_tables) {
    GList *l;

    for (l = demux->index_tables; l; l = l->next) {
      GstMXFDemuxIndexTable *t = l->data;
      g_array_free (t->segments, TRUE);
      g_array_free (t->reverse_temporal_offsets, TRUE);
      g_free (t);
    }
    g_list_free (demux->index_tables);
    demux->index_tables = NULL;
  }

  demux->index_table_segments_collected = FALSE;

  gst_mxf_demux_reset_mxf_state (demux);
  gst_mxf_demux_reset_metadata (demux);

  demux->have_group_id = FALSE;
  demux->group_id = G_MAXUINT;
}

static GstFlowReturn
gst_mxf_demux_pull_range (GstMXFDemux * demux, guint64 offset,
    guint size, GstBuffer ** buffer)
{
  GstFlowReturn ret;

  ret = gst_pad_pull_range (demux->sinkpad, offset, size, buffer);
  if (G_UNLIKELY (ret != GST_FLOW_OK)) {
    GST_WARNING_OBJECT (demux,
        "failed when pulling %u bytes from offset %" G_GUINT64_FORMAT ": %s",
        size, offset, gst_flow_get_name (ret));
    *buffer = NULL;
    return ret;
  }

  if (G_UNLIKELY (*buffer && gst_buffer_get_size (*buffer) != size)) {
    GST_WARNING_OBJECT (demux,
        "partial pull got %" G_GSIZE_FORMAT " when expecting %u from offset %"
        G_GUINT64_FORMAT, gst_buffer_get_size (*buffer), size, offset);
    gst_buffer_unref (*buffer);
    ret = GST_FLOW_EOS;
    *buffer = NULL;
    return ret;
  }

  return ret;
}

static gboolean
gst_mxf_demux_push_src_event (GstMXFDemux * demux, GstEvent * event)
{
  gboolean ret = TRUE;
  guint i;

  GST_DEBUG_OBJECT (demux, "Pushing '%s' event downstream",
      GST_EVENT_TYPE_NAME (event));

  for (i = 0; i < demux->src->len; i++) {
    GstMXFDemuxPad *pad = GST_MXF_DEMUX_PAD (g_ptr_array_index (demux->src, i));

    if (pad->eos && GST_EVENT_TYPE (event) == GST_EVENT_EOS)
      continue;

    ret |= gst_pad_push_event (GST_PAD_CAST (pad), gst_event_ref (event));
  }

  gst_event_unref (event);

  return ret;
}

static GstMXFDemuxPad *
gst_mxf_demux_get_earliest_pad (GstMXFDemux * demux)
{
  guint i;
  GstClockTime earliest = GST_CLOCK_TIME_NONE;
  GstMXFDemuxPad *pad = NULL;

  for (i = 0; i < demux->src->len; i++) {
    GstMXFDemuxPad *p = g_ptr_array_index (demux->src, i);

    if (!p->eos && p->position < earliest) {
      earliest = p->position;
      pad = p;
    }
  }

  return pad;
}

static gint
gst_mxf_demux_partition_compare (GstMXFDemuxPartition * a,
    GstMXFDemuxPartition * b)
{
  if (a->partition.this_partition < b->partition.this_partition)
    return -1;
  else if (a->partition.this_partition > b->partition.this_partition)
    return 1;
  else
    return 0;
}

/* Final checks and variable calculation for tracks and partition. This function
 * can be called repeatedly without any side-effect.
 */
static void
gst_mxf_demux_partition_postcheck (GstMXFDemux * demux,
    GstMXFDemuxPartition * partition)
{
  guint i;
  GstMXFDemuxPartition *old_partition = demux->current_partition;

  /* If we already handled this partition or it doesn't contain any essence, skip */
  if (partition->single_track || !partition->partition.body_sid)
    return;

  for (i = 0; i < demux->essence_tracks->len; i++) {
    GstMXFDemuxEssenceTrack *cand =
        g_ptr_array_index (demux->essence_tracks, i);

    if (cand->body_sid != partition->partition.body_sid)
      continue;

    if (!cand->source_package->is_interleaved) {
      GST_DEBUG_OBJECT (demux,
          "Assigning single track %d (0x%08x) to partition at offset %"
          G_GUINT64_FORMAT, cand->track_id, cand->track_number,
          partition->partition.this_partition);

      partition->single_track = cand;

      if (partition->essence_container_offset != 0
          && cand->wrapping != MXF_ESSENCE_WRAPPING_FRAME_WRAPPING) {
        GstMXFKLV essence_klv;
        GstMXFDemuxIndex entry;
        /* Update the essence container offset for the fact that the index
         * stream offset is relative to the essence position and not to the
         * KLV position */
        if (gst_mxf_demux_peek_klv_packet (demux,
                partition->partition.this_partition +
                partition->essence_container_offset,
                &essence_klv) == GST_FLOW_OK) {
          partition->essence_container_offset += essence_klv.data_offset;
          /* And keep a copy of the clip/custom klv for this partition */
          partition->clip_klv = essence_klv;
          GST_DEBUG_OBJECT (demux,
              "Non-frame wrapping, updated essence_container_offset to %"
              G_GUINT64_FORMAT, partition->essence_container_offset);

          /* And match it against index table, this will also update the track delta_id (if needed) */
          demux->current_partition = partition;
          find_entry_for_offset (demux, cand,
              essence_klv.offset + essence_klv.data_offset, &entry);
          demux->current_partition = old_partition;
        }
      }

      break;
    }
  }
}

static GstFlowReturn
gst_mxf_demux_handle_partition_pack (GstMXFDemux * demux, GstMXFKLV * klv)
{
  MXFPartitionPack partition;
  GList *l;
  GstMXFDemuxPartition *p = NULL;
  GstMapInfo map;
  gboolean ret;
  GstFlowReturn flowret;

  GST_DEBUG_OBJECT (demux,
      "Handling partition pack of size %" G_GSIZE_FORMAT " at offset %"
      G_GUINT64_FORMAT, klv->length, klv->offset);

  for (l = demux->partitions; l; l = l->next) {
    GstMXFDemuxPartition *tmp = l->data;

    if (tmp->partition.this_partition + demux->run_in == demux->offset &&
        tmp->partition.major_version == 0x0001) {
      GST_DEBUG_OBJECT (demux, "Partition already parsed");
      p = tmp;
      goto out;
    }
  }

  flowret = gst_mxf_demux_fill_klv (demux, klv);
  if (flowret != GST_FLOW_OK)
    return flowret;

  gst_buffer_map (klv->data, &map, GST_MAP_READ);
  ret = mxf_partition_pack_parse (&klv->key, &partition, map.data, map.size);
  gst_buffer_unmap (klv->data, &map);
  if (!ret) {
    GST_ERROR_OBJECT (demux, "Parsing partition pack failed");
    return GST_FLOW_ERROR;
  }

  if (partition.this_partition != demux->offset + demux->run_in) {
    GST_WARNING_OBJECT (demux,
        "Partition with incorrect offset (this %" G_GUINT64_FORMAT
        " demux offset %" G_GUINT64_FORMAT " run_in:%" G_GUINT64_FORMAT ")",
        partition.this_partition, demux->offset, demux->run_in);
    partition.this_partition = demux->offset + demux->run_in;
  }

  if (partition.type == MXF_PARTITION_PACK_HEADER)
    demux->footer_partition_pack_offset = partition.footer_partition;

  for (l = demux->partitions; l; l = l->next) {
    GstMXFDemuxPartition *tmp = l->data;

    if (tmp->partition.this_partition + demux->run_in == demux->offset) {
      p = tmp;
      break;
    }
  }

  if (p) {
    mxf_partition_pack_reset (&p->partition);
    memcpy (&p->partition, &partition, sizeof (MXFPartitionPack));
  } else {
    p = g_new0 (GstMXFDemuxPartition, 1);
    memcpy (&p->partition, &partition, sizeof (MXFPartitionPack));
    demux->partitions =
        g_list_insert_sorted (demux->partitions, p,
        (GCompareFunc) gst_mxf_demux_partition_compare);
  }

  gst_mxf_demux_partition_postcheck (demux, p);

  for (l = demux->partitions; l; l = l->next) {
    GstMXFDemuxPartition *a, *b;

    if (l->next == NULL)
      break;

    a = l->data;
    b = l->next->data;

    b->partition.prev_partition = a->partition.this_partition;
  }

out:
  GST_DEBUG_OBJECT (demux,
      "Current partition now %p (body_sid:%d index_sid:%d this_partition:%"
      G_GUINT64_FORMAT ")", p, p->partition.body_sid, p->partition.index_sid,
      p->partition.this_partition);
  demux->current_partition = p;

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_mxf_demux_handle_primer_pack (GstMXFDemux * demux, GstMXFKLV * klv)
{
  GstMapInfo map;
  gboolean ret;
  GstFlowReturn flowret;

  GST_DEBUG_OBJECT (demux,
      "Handling primer pack of size %" G_GSIZE_FORMAT " at offset %"
      G_GUINT64_FORMAT, klv->length, klv->offset);

  if (G_UNLIKELY (!demux->current_partition)) {
    GST_ERROR_OBJECT (demux, "Primer pack before partition pack");
    return GST_FLOW_ERROR;
  }

  if (G_UNLIKELY (demux->current_partition->primer.mappings)) {
    GST_DEBUG_OBJECT (demux, "Primer pack already exists");
    return GST_FLOW_OK;
  }

  flowret = gst_mxf_demux_fill_klv (demux, klv);
  if (flowret != GST_FLOW_OK)
    return flowret;

  gst_buffer_map (klv->data, &map, GST_MAP_READ);
  ret = mxf_primer_pack_parse (&klv->key, &demux->current_partition->primer,
      map.data, map.size);
  gst_buffer_unmap (klv->data, &map);
  if (!ret) {
    GST_ERROR_OBJECT (demux, "Parsing primer pack failed");
    return GST_FLOW_ERROR;
  }

  demux->current_partition->primer.offset = demux->offset;

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_mxf_demux_resolve_references (GstMXFDemux * demux)
{
  GstFlowReturn ret = GST_FLOW_OK;
  GHashTableIter iter;
  MXFMetadataBase *m = NULL;
  GstStructure *structure;
  guint i;

  g_rw_lock_writer_lock (&demux->metadata_lock);

  GST_DEBUG_OBJECT (demux, "Resolve metadata references");
  demux->update_metadata = FALSE;

  if (!demux->metadata) {
    GST_ERROR_OBJECT (demux, "No metadata yet");
    g_rw_lock_writer_unlock (&demux->metadata_lock);
    return GST_FLOW_ERROR;
  }

  g_hash_table_iter_init (&iter, demux->metadata);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer) & m)) {
    m->resolved = MXF_METADATA_BASE_RESOLVE_STATE_NONE;
  }

  g_hash_table_iter_init (&iter, demux->metadata);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer) & m)) {
    gboolean resolved;

    resolved = mxf_metadata_base_resolve (m, demux->metadata);

    /* Resolving can fail for anything but the preface, as the preface
     * will resolve everything required */
    if (!resolved && MXF_IS_METADATA_PREFACE (m)) {
      ret = GST_FLOW_ERROR;
      goto error;
    }
  }

  demux->metadata_resolved = TRUE;

  structure =
      mxf_metadata_base_to_structure (MXF_METADATA_BASE (demux->preface));
  if (!demux->tags)
    demux->tags = gst_tag_list_new_empty ();

  gst_tag_list_add (demux->tags, GST_TAG_MERGE_REPLACE, GST_TAG_MXF_STRUCTURE,
      structure, NULL);

  gst_structure_free (structure);

  /* Check for quirks */
  for (i = 0; i < demux->preface->n_identifications; i++) {
    MXFMetadataIdentification *identification =
        demux->preface->identifications[i];

    GST_DEBUG_OBJECT (demux, "product:'%s' company:'%s'",
        identification->product_name, identification->company_name);
    if (!g_strcmp0 (identification->product_name, "MXFTk Advanced") &&
        !g_strcmp0 (identification->company_name, "OpenCube") &&
        identification->product_version.major <= 2 &&
        identification->product_version.minor <= 0) {
      GST_WARNING_OBJECT (demux,
          "Setting up quirk for misuse of temporal_order field");
      demux->temporal_order_misuse = TRUE;
    }
  }

  g_rw_lock_writer_unlock (&demux->metadata_lock);

  return ret;

error:
  demux->metadata_resolved = FALSE;
  g_rw_lock_writer_unlock (&demux->metadata_lock);

  return ret;
}

static MXFMetadataGenericPackage *
gst_mxf_demux_find_package (GstMXFDemux * demux, const MXFUMID * umid)
{
  MXFMetadataGenericPackage *ret = NULL;
  guint i;

  if (demux->preface->content_storage
      && demux->preface->content_storage->packages) {
    for (i = 0; i < demux->preface->content_storage->n_packages; i++) {
      MXFMetadataGenericPackage *p =
          demux->preface->content_storage->packages[i];

      if (!p)
        continue;

      if (mxf_umid_is_equal (&p->package_uid, umid)) {
        ret = p;
        break;
      }
    }
  }

  return ret;
}

static MXFMetadataGenericPackage *
gst_mxf_demux_choose_package (GstMXFDemux * demux)
{
  MXFMetadataGenericPackage *ret = NULL;
  guint i;

  if (demux->requested_package_string) {
    MXFUMID umid = { {0,}
    };

    if (!mxf_umid_from_string (demux->requested_package_string, &umid)) {
      GST_ERROR_OBJECT (demux, "Invalid requested package");
    }
    g_free (demux->requested_package_string);
    demux->requested_package_string = NULL;

    ret = gst_mxf_demux_find_package (demux, &umid);
  }

  if (!ret && !mxf_umid_is_zero (&demux->current_package_uid))
    ret = gst_mxf_demux_find_package (demux, &demux->current_package_uid);

  if (ret && (MXF_IS_METADATA_MATERIAL_PACKAGE (ret)
          || (MXF_IS_METADATA_SOURCE_PACKAGE (ret)
              && MXF_METADATA_SOURCE_PACKAGE (ret)->top_level)))
    goto done;
  else if (ret)
    GST_WARNING_OBJECT (demux,
        "Current package is not a material package or top-level source package, choosing the first best");
  else if (!mxf_umid_is_zero (&demux->current_package_uid))
    GST_WARNING_OBJECT (demux,
        "Current package not found, choosing the first best");

  ret = demux->preface->primary_package;
  if (ret && (MXF_IS_METADATA_MATERIAL_PACKAGE (ret)
          || (MXF_IS_METADATA_SOURCE_PACKAGE (ret)
              && MXF_METADATA_SOURCE_PACKAGE (ret)->top_level)))
    goto done;
  ret = NULL;

  for (i = 0; i < demux->preface->content_storage->n_packages; i++) {
    if (demux->preface->content_storage->packages[i] &&
        MXF_IS_METADATA_MATERIAL_PACKAGE (demux->preface->content_storage->
            packages[i])) {
      ret =
          MXF_METADATA_GENERIC_PACKAGE (demux->preface->content_storage->
          packages[i]);
      break;
    }
  }

  if (!ret) {
    GST_ERROR_OBJECT (demux, "No material package");
    return NULL;
  }

done:
  if (mxf_umid_is_equal (&ret->package_uid, &demux->current_package_uid)) {
    gchar current_package_string[96];

    gst_mxf_demux_remove_pads (demux);
    memcpy (&demux->current_package_uid, &ret->package_uid, 32);

    mxf_umid_to_string (&ret->package_uid, current_package_string);
    demux->current_package_string = g_strdup (current_package_string);
    g_object_notify (G_OBJECT (demux), "package");

    if (!demux->tags)
      demux->tags = gst_tag_list_new_empty ();
    gst_tag_list_add (demux->tags, GST_TAG_MERGE_REPLACE, GST_TAG_MXF_UMID,
        demux->current_package_string, NULL);
  }
  demux->current_package = ret;

  return ret;
}

static GstFlowReturn
gst_mxf_demux_update_essence_tracks (GstMXFDemux * demux)
{
  guint i, j, k;

  g_return_val_if_fail (demux->preface->content_storage, GST_FLOW_ERROR);
  g_return_val_if_fail (demux->preface->content_storage->essence_container_data,
      GST_FLOW_ERROR);

  for (i = 0; i < demux->preface->content_storage->n_essence_container_data;
      i++) {
    MXFMetadataEssenceContainerData *edata;
    MXFMetadataSourcePackage *package;
    MXFFraction common_rate = { 0, 0 };

    if (demux->preface->content_storage->essence_container_data[i] == NULL)
      continue;

    edata = demux->preface->content_storage->essence_container_data[i];

    if (!edata->linked_package) {
      GST_WARNING_OBJECT (demux, "Linked package not resolved");
      continue;
    }

    package = edata->linked_package;

    if (!package->parent.tracks) {
      GST_WARNING_OBJECT (demux, "Linked package with no resolved tracks");
      continue;
    }

    for (j = 0; j < package->parent.n_tracks; j++) {
      MXFMetadataTimelineTrack *track;
      GstMXFDemuxEssenceTrack *etrack = NULL;
      GstCaps *caps = NULL;
      gboolean new = FALSE;

      if (!package->parent.tracks[j]
          || !MXF_IS_METADATA_TIMELINE_TRACK (package->parent.tracks[j]))
        continue;

      track = MXF_METADATA_TIMELINE_TRACK (package->parent.tracks[j]);
      if ((track->parent.type & 0xf0) != 0x30) {
        GST_DEBUG_OBJECT (demux,
            "Skipping track of type 0x%02x (id:%d number:0x%08x)",
            track->parent.type, track->parent.track_id,
            track->parent.track_number);
        continue;
      }

      if (track->edit_rate.n <= 0 || track->edit_rate.d <= 0) {
        GST_WARNING_OBJECT (demux, "Invalid edit rate");
        continue;
      }

      if (package->is_interleaved) {
        /*
         * S377-1:2019 "9.4.2 The MXF timing model"
         *
         * The value of Edit Rate shall be identical for every timeline Essence
         * Track of the Top-Level File Package.
         *
         * The value of Edit Rate of the timeline Essence Tracks of one
         * Top-Level File Package need not match the Edit Rate of the Essence
         * Tracks of the other Top-Level File Packages.
         *
         * S377-1:2019 "9.5.5 Top-Level File Packages"
         *
         *12. All Essence Tracks of a Top-Level File Package **shall** have the
         *    same value of Edit Rate. All other Tracks of a Top-Level File
         *    Package **should** have the same value of Edit Rate as the
         *    Essence Tracks.
         */
        if (common_rate.n == 0 && common_rate.d == 0) {
          common_rate = track->edit_rate;
        } else if (common_rate.n * track->edit_rate.d !=
            common_rate.d * track->edit_rate.n) {
          GST_ELEMENT_ERROR (demux, STREAM, WRONG_TYPE, (NULL),
              ("Interleaved File Package doesn't have identical edit rate on all tracks."));
          return GST_FLOW_ERROR;
        }
      }

      for (k = 0; k < demux->essence_tracks->len; k++) {
        GstMXFDemuxEssenceTrack *tmp =
            g_ptr_array_index (demux->essence_tracks, k);

        if (tmp->track_number == track->parent.track_number &&
            tmp->body_sid == edata->body_sid) {
          if (tmp->track_id != track->parent.track_id ||
              !mxf_umid_is_equal (&tmp->source_package_uid,
                  &package->parent.package_uid)) {
            GST_ERROR_OBJECT (demux, "There already exists a different track "
                "with this track number and body sid but a different source "
                "or source track id -- ignoring");
            continue;
          }
          etrack = tmp;
          break;
        }
      }

      if (!etrack) {
        GstMXFDemuxEssenceTrack *tmp = g_new0 (GstMXFDemuxEssenceTrack, 1);

        tmp->body_sid = edata->body_sid;
        tmp->index_sid = edata->index_sid;
        tmp->track_number = track->parent.track_number;
        tmp->track_id = track->parent.track_id;
        memcpy (&tmp->source_package_uid, &package->parent.package_uid, 32);

        if (demux->current_partition->partition.body_sid == edata->body_sid &&
            demux->current_partition->partition.body_offset == 0)
          tmp->position = 0;
        else
          tmp->position = -1;

        g_ptr_array_add (demux->essence_tracks, tmp);
        etrack =
            g_ptr_array_index (demux->essence_tracks,
            demux->essence_tracks->len - 1);
        new = TRUE;
      }

      etrack->source_package = NULL;
      etrack->source_track = NULL;
      etrack->delta_id = -1;

      if (!track->parent.sequence) {
        GST_WARNING_OBJECT (demux, "Source track has no sequence");
        goto next;
      }

      if (track->parent.n_descriptor == 0) {
        GST_WARNING_OBJECT (demux, "Source track has no descriptors");
        goto next;
      }

      if (track->parent.sequence->duration > etrack->duration)
        etrack->duration = track->parent.sequence->duration;

      g_free (etrack->mapping_data);
      etrack->mapping_data = NULL;
      etrack->handler = NULL;
      etrack->handle_func = NULL;
      if (etrack->tags)
        gst_tag_list_unref (etrack->tags);
      etrack->tags = NULL;

      etrack->handler = mxf_essence_element_handler_find (track);
      if (!etrack->handler) {
        gchar essence_container[48];
        gchar essence_compression[48];
        gchar *name;

        GST_WARNING_OBJECT (demux,
            "No essence element handler for track %u found", i);

        mxf_ul_to_string (&track->parent.descriptor[0]->essence_container,
            essence_container);

        if (track->parent.type == MXF_METADATA_TRACK_PICTURE_ESSENCE) {
          if (MXF_IS_METADATA_GENERIC_PICTURE_ESSENCE_DESCRIPTOR (track->parent.
                  descriptor[0]))
            mxf_ul_to_string (&MXF_METADATA_GENERIC_PICTURE_ESSENCE_DESCRIPTOR
                (track->parent.descriptor[0])->picture_essence_coding,
                essence_compression);

          name =
              g_strdup_printf ("video/x-mxf-%s-%s", essence_container,
              essence_compression);
        } else if (track->parent.type == MXF_METADATA_TRACK_SOUND_ESSENCE) {
          if (MXF_IS_METADATA_GENERIC_SOUND_ESSENCE_DESCRIPTOR (track->parent.
                  descriptor[0]))
            mxf_ul_to_string (&MXF_METADATA_GENERIC_SOUND_ESSENCE_DESCRIPTOR
                (track->parent.descriptor[0])->sound_essence_compression,
                essence_compression);

          name =
              g_strdup_printf ("audio/x-mxf-%s-%s", essence_container,
              essence_compression);
        } else if (track->parent.type == MXF_METADATA_TRACK_DATA_ESSENCE) {
          if (MXF_IS_METADATA_GENERIC_DATA_ESSENCE_DESCRIPTOR (track->parent.
                  descriptor[0]))
            mxf_ul_to_string (&MXF_METADATA_GENERIC_DATA_ESSENCE_DESCRIPTOR
                (track->parent.descriptor[0])->data_essence_coding,
                essence_compression);

          name =
              g_strdup_printf ("application/x-mxf-%s-%s", essence_container,
              essence_compression);
        } else {
          name = NULL;
          g_assert_not_reached ();
        }

        caps = gst_caps_new_empty_simple (name);
        g_free (name);
        etrack->intra_only = FALSE;
      } else {
        caps =
            etrack->handler->create_caps (track, &etrack->tags,
            &etrack->intra_only, &etrack->handle_func, &etrack->mapping_data);
      }

      GST_DEBUG_OBJECT (demux, "Created caps %" GST_PTR_FORMAT, caps);

      if (!caps && new) {
        GST_WARNING_OBJECT (demux, "No caps created, ignoring stream");
        g_free (etrack->mapping_data);
        etrack->mapping_data = NULL;
        if (etrack->tags)
          gst_tag_list_unref (etrack->tags);
        etrack->tags = NULL;
        goto next;
      } else if (!caps) {
        GST_WARNING_OBJECT (demux, "Couldn't create updated caps for stream");
      } else if (!etrack->caps || !gst_caps_is_equal (etrack->caps, caps)) {
        if (etrack->caps)
          gst_caps_unref (etrack->caps);
        etrack->caps = caps;
      } else {
        gst_caps_unref (caps);
        caps = NULL;
      }

      etrack->min_edit_units = 1;
      /* Ensure we don't output one buffer per sample for audio */
      if (gst_util_uint64_scale (GST_SECOND, track->edit_rate.d,
              track->edit_rate.n) < 10 * GST_MSECOND) {
        GstStructure *s = gst_caps_get_structure (etrack->caps, 0);
        const gchar *name = gst_structure_get_name (s);
        if (g_str_has_prefix (name, "audio/x-raw")) {
          etrack->min_edit_units =
              gst_util_uint64_scale (25 * GST_MSECOND, track->edit_rate.n,
              track->edit_rate.d * GST_SECOND);
          GST_DEBUG_OBJECT (demux, "Seting miminum number of edit units to %u",
              etrack->min_edit_units);
        }
      }

      /* FIXME : We really should just abort/ignore the stream completely if we
       * don't have a handler for it */
      if (etrack->handler != NULL)
        etrack->wrapping = etrack->handler->get_track_wrapping (track);
      else
        etrack->wrapping = MXF_ESSENCE_WRAPPING_UNKNOWN_WRAPPING;

      if (package->is_interleaved) {
        GST_DEBUG_OBJECT (demux,
            "track comes from interleaved source package with %d track(s), setting delta_id to -1",
            package->parent.n_tracks);
        if (etrack->wrapping != MXF_ESSENCE_WRAPPING_FRAME_WRAPPING) {
          GST_ELEMENT_ERROR (demux, STREAM, WRONG_TYPE, (NULL),
              ("Non-frame-wrapping is not allowed in interleaved File Package."));
          return GST_FLOW_ERROR;
        }
        etrack->delta_id = MXF_INDEX_DELTA_ID_UNKNOWN;
      } else {
        etrack->delta_id = MXF_INDEX_DELTA_ID_UNKNOWN;
      }
      etrack->source_package = package;
      etrack->source_track = track;
      continue;

    next:
      if (new) {
        g_ptr_array_remove_index (demux->essence_tracks,
            demux->essence_tracks->len - 1);
      }
    }
  }

  if (demux->essence_tracks->len == 0) {
    GST_ERROR_OBJECT (demux, "No valid essence tracks in this file");
    return GST_FLOW_ERROR;
  }

  for (i = 0; i < demux->essence_tracks->len; i++) {
    GstMXFDemuxEssenceTrack *etrack =
        g_ptr_array_index (demux->essence_tracks, i);

    if (!etrack->source_package || !etrack->source_track || !etrack->caps) {
      GST_ERROR_OBJECT (demux, "Failed to update essence track %u", i);
      return GST_FLOW_ERROR;
    }

  }

  return GST_FLOW_OK;
}

static MXFMetadataEssenceContainerData *
essence_container_for_source_package (MXFMetadataContentStorage * storage,
    MXFMetadataSourcePackage * package)
{
  guint i;

  for (i = 0; i < storage->n_essence_container_data; i++) {
    MXFMetadataEssenceContainerData *cont = storage->essence_container_data[i];
    if (cont && cont->linked_package == package)
      return cont;
  }

  return NULL;
}

static void
gst_mxf_demux_show_topology (GstMXFDemux * demux)
{
  GList *material_packages = NULL;
  GList *file_packages = NULL;
  GList *tmp;
  MXFMetadataContentStorage *storage = demux->preface->content_storage;
  guint i;
  gchar str[96];

  /* Show the topology starting from the preface */
  GST_DEBUG_OBJECT (demux, "Topology");

  for (i = 0; i < storage->n_packages; i++) {
    MXFMetadataGenericPackage *pack = storage->packages[i];
    if (MXF_IS_METADATA_MATERIAL_PACKAGE (pack))
      material_packages = g_list_append (material_packages, pack);
    else if (MXF_IS_METADATA_SOURCE_PACKAGE (pack))
      file_packages = g_list_append (file_packages, pack);
    else
      GST_DEBUG_OBJECT (demux, "Unknown package type");
  }

  GST_DEBUG_OBJECT (demux, "Number of Material Package (i.e. output) : %d",
      g_list_length (material_packages));
  for (tmp = material_packages; tmp; tmp = tmp->next) {
    MXFMetadataMaterialPackage *pack = (MXFMetadataMaterialPackage *) tmp->data;
    GST_DEBUG_OBJECT (demux, "  Package with %d tracks , UID:%s",
        pack->n_tracks, mxf_umid_to_string (&pack->package_uid, str));
    for (i = 0; i < pack->n_tracks; i++) {
      MXFMetadataTrack *track = pack->tracks[i];
      if (track == NULL) {
        GST_DEBUG_OBJECT (demux, "    Unknown/Unhandled track UUID %s",
            mxf_uuid_to_string (&pack->tracks_uids[i], str));
      } else if (MXF_IS_METADATA_TIMELINE_TRACK (track)) {
        MXFMetadataTimelineTrack *mtrack = (MXFMetadataTimelineTrack *) track;
        GST_DEBUG_OBJECT (demux,
            "    Timeline Track id:%d number:0x%08x name:`%s` edit_rate:%d/%d origin:%"
            G_GINT64_FORMAT, track->track_id, track->track_number,
            track->track_name, mtrack->edit_rate.n, mtrack->edit_rate.d,
            mtrack->origin);
      } else {
        GST_DEBUG_OBJECT (demux,
            "    Non-Timeline-Track id:%d number:0x%08x name:`%s`",
            track->track_id, track->track_number, track->track_name);
      }
      if (track) {
        MXFMetadataSequence *sequence = track->sequence;
        guint si;
        GST_DEBUG_OBJECT (demux,
            "      Sequence duration:%" G_GINT64_FORMAT
            " n_structural_components:%d", sequence->duration,
            sequence->n_structural_components);
        for (si = 0; si < sequence->n_structural_components; si++) {
          MXFMetadataStructuralComponent *comp =
              sequence->structural_components[si];
          GST_DEBUG_OBJECT (demux,
              "        Component #%d duration:%" G_GINT64_FORMAT, si,
              comp->duration);
          if (MXF_IS_METADATA_SOURCE_CLIP (comp)) {
            MXFMetadataSourceClip *clip = (MXFMetadataSourceClip *) comp;
            GST_DEBUG_OBJECT (demux,
                "          Clip start_position:%" G_GINT64_FORMAT
                " source_track_id:%d source_package_id:%s",
                clip->start_position, clip->source_track_id,
                mxf_umid_to_string (&clip->source_package_id, str));
          }
        }

      }
    }
  }

  GST_DEBUG_OBJECT (demux, "Number of File Packages (i.e. input) : %d",
      g_list_length (file_packages));
  for (tmp = file_packages; tmp; tmp = tmp->next) {
    MXFMetadataMaterialPackage *pack = (MXFMetadataMaterialPackage *) tmp->data;
    MXFMetadataSourcePackage *src = (MXFMetadataSourcePackage *) pack;
#ifndef GST_DISABLE_GST_DEBUG
    MXFMetadataEssenceContainerData *econt =
        essence_container_for_source_package (storage, src);
    GST_DEBUG_OBJECT (demux,
        "  Package (body_sid:%d index_sid:%d top_level:%d) with %d tracks , UID:%s",
        econt ? econt->body_sid : 0, econt ? econt->index_sid : 0,
        src->top_level, pack->n_tracks, mxf_umid_to_string (&pack->package_uid,
            str));
#endif
    GST_DEBUG_OBJECT (demux, "    Package descriptor : %s",
        g_type_name (G_OBJECT_TYPE (src->descriptor)));
    for (i = 0; i < pack->n_tracks; i++) {
      MXFMetadataTrack *track = pack->tracks[i];
      if (!track)
        continue;
      MXFMetadataSequence *sequence = track->sequence;
      guint di, si;
      if (MXF_IS_METADATA_TIMELINE_TRACK (track)) {
        MXFMetadataTimelineTrack *mtrack = (MXFMetadataTimelineTrack *) track;
        GST_DEBUG_OBJECT (demux,
            "    Timeline Track id:%d number:0x%08x name:`%s` edit_rate:%d/%d origin:%"
            G_GINT64_FORMAT, track->track_id, track->track_number,
            track->track_name, mtrack->edit_rate.n, mtrack->edit_rate.d,
            mtrack->origin);
      } else {
        GST_DEBUG_OBJECT (demux,
            "    Non-Timeline-Track id:%d number:0x%08x name:`%s` type:0x%x",
            track->track_id, track->track_number, track->track_name,
            track->type);
      }
      for (di = 0; di < track->n_descriptor; di++) {
        MXFMetadataFileDescriptor *desc = track->descriptor[di];
        MXFMetadataGenericDescriptor *generic =
            (MXFMetadataGenericDescriptor *) desc;
        guint subdi;

        GST_DEBUG_OBJECT (demux, "      Descriptor %s %s",
            g_type_name (G_OBJECT_TYPE (desc)),
            mxf_ul_to_string (&desc->essence_container, str));
        for (subdi = 0; subdi < generic->n_sub_descriptors; subdi++) {
          MXFMetadataGenericDescriptor *subdesc =
              generic->sub_descriptors[subdi];
          if (subdesc) {
            GST_DEBUG_OBJECT (demux, "        Sub-Descriptor %s",
                g_type_name (G_OBJECT_TYPE (subdesc)));
          }
        }
      }
      GST_DEBUG_OBJECT (demux,
          "      Sequence duration:%" G_GINT64_FORMAT
          " n_structural_components:%d", sequence->duration,
          sequence->n_structural_components);
      for (si = 0; si < sequence->n_structural_components; si++) {
        MXFMetadataStructuralComponent *comp =
            sequence->structural_components[si];
        GST_DEBUG_OBJECT (demux,
            "        Component #%d duration:%" G_GINT64_FORMAT, si,
            comp->duration);
      }
    }
  }

  g_list_free (material_packages);
  g_list_free (file_packages);
}

static GstFlowReturn
gst_mxf_demux_update_tracks (GstMXFDemux * demux)
{
  MXFMetadataGenericPackage *current_package = NULL;
  guint i, j, k;
  gboolean first_run;
  guint component_index;
  GstFlowReturn ret;
  GList *pads = NULL, *l;
  GstVideoTimeCode start_timecode = GST_VIDEO_TIME_CODE_INIT;

  g_rw_lock_writer_lock (&demux->metadata_lock);
  GST_DEBUG_OBJECT (demux, "Updating tracks");

  gst_mxf_demux_show_topology (demux);

  if ((ret = gst_mxf_demux_update_essence_tracks (demux)) != GST_FLOW_OK) {
    goto error;
  }

  current_package = gst_mxf_demux_choose_package (demux);

  if (!current_package) {
    GST_ERROR_OBJECT (demux, "Unable to find current package");
    ret = GST_FLOW_ERROR;
    goto error;
  } else if (!current_package->tracks) {
    GST_ERROR_OBJECT (demux, "Current package has no (resolved) tracks");
    ret = GST_FLOW_ERROR;
    goto error;
  } else if (!current_package->n_essence_tracks) {
    GST_ERROR_OBJECT (demux, "Current package has no essence tracks");
    ret = GST_FLOW_ERROR;
    goto error;
  }

  first_run = (demux->src->len == 0);

  /* For material packages, there must be one timecode track with one
   * continuous timecode. For source packages there might be multiple,
   * discontinuous timecode components.
   * TODO: Support multiple timecode components
   */
  for (i = 0; i < current_package->n_tracks; i++) {
    MXFMetadataTimelineTrack *track = NULL;
    MXFMetadataSequence *sequence = NULL;
    MXFMetadataTimecodeComponent *component = NULL;

    if (!current_package->tracks[i]) {
      GST_WARNING_OBJECT (demux, "Unresolved track");
      continue;
    }

    if (!MXF_IS_METADATA_TIMELINE_TRACK (current_package->tracks[i])) {
      GST_DEBUG_OBJECT (demux, "Skipping Non-timeline track");
      continue;
    }


    track = MXF_METADATA_TIMELINE_TRACK (current_package->tracks[i]);

    if (!track->parent.sequence)
      continue;
    sequence = track->parent.sequence;
    if (sequence->n_structural_components != 1 ||
        !sequence->structural_components[0]
        ||
        !MXF_IS_METADATA_TIMECODE_COMPONENT (sequence->structural_components
            [0]))
      continue;

    component =
        MXF_METADATA_TIMECODE_COMPONENT (sequence->structural_components[0]);

    /* Not a timecode track */
    if (track->parent.type && (track->parent.type & 0xf0) != 0x10)
      continue;

    /* Main timecode track must have id 1, all others must be 0 */
    if (track->parent.track_id != 1)
      continue;

    gst_video_time_code_init (&start_timecode, track->edit_rate.n,
        track->edit_rate.d, NULL, (component->drop_frame
            ?
            GST_VIDEO_TIME_CODE_FLAGS_DROP_FRAME
            : GST_VIDEO_TIME_CODE_FLAGS_NONE), 0, 0, 0, 0, 0);
    gst_video_time_code_add_frames (&start_timecode, track->origin);
    gst_video_time_code_add_frames (&start_timecode, component->start_timecode);
    break;
  }

  for (i = 0; i < current_package->n_tracks; i++) {
    MXFMetadataTimelineTrack *track = NULL;
    MXFMetadataSequence *sequence;
    MXFMetadataSourceClip *component = NULL;
    MXFMetadataSourcePackage *source_package = NULL;
    MXFMetadataTimelineTrack *source_track = NULL;
    GstMXFDemuxEssenceTrack *etrack = NULL;
    GstMXFDemuxPad *pad = NULL;
    GstCaps *pad_caps;

    GST_DEBUG_OBJECT (demux, "Handling track %u", i);

    if (!current_package->tracks[i]) {
      GST_WARNING_OBJECT (demux, "Unresolved track");
      continue;
    }

    if (!MXF_IS_METADATA_TIMELINE_TRACK (current_package->tracks[i])) {
      GST_DEBUG_OBJECT (demux, "No timeline track");
      continue;
    }

    track = MXF_METADATA_TIMELINE_TRACK (current_package->tracks[i]);

    if (!first_run) {
      /* Find pad from track_id */
      for (j = 0; j < demux->src->len; j++) {
        GstMXFDemuxPad *tmp = g_ptr_array_index (demux->src, j);

        if (tmp->track_id == track->parent.track_id) {
          pad = tmp;
          break;
        }
      }
    }

    if (pad)
      component_index = pad->current_component_index;
    else
      component_index = 0;

    if (!track->parent.sequence) {
      GST_WARNING_OBJECT (demux, "Track with no sequence");
      if (!pad) {
        continue;
      } else {
        ret = GST_FLOW_ERROR;
        goto error;
      }
    }

    sequence = track->parent.sequence;

    if (MXF_IS_METADATA_SOURCE_PACKAGE (current_package)) {
      GST_DEBUG_OBJECT (demux, "Playing source package");

      component = NULL;
      source_package = MXF_METADATA_SOURCE_PACKAGE (current_package);
      source_track = track;
    } else if (sequence->structural_components
        &&
        MXF_IS_METADATA_SOURCE_CLIP (sequence->structural_components
            [component_index])) {
      GST_DEBUG_OBJECT (demux, "Playing material package");

      component =
          MXF_METADATA_SOURCE_CLIP (sequence->structural_components
          [component_index]);
      if (!component) {
        GST_WARNING_OBJECT (demux, "NULL component in non-source package");
        if (!pad) {
          continue;
        } else {
          ret = GST_FLOW_ERROR;
          goto error;
        }
      }

      if (component->source_package && component->source_package->top_level &&
          MXF_METADATA_GENERIC_PACKAGE (component->source_package)->tracks) {
        MXFMetadataGenericPackage *tmp_pkg =
            MXF_METADATA_GENERIC_PACKAGE (component->source_package);

        source_package = component->source_package;

        for (k = 0; k < tmp_pkg->n_tracks; k++) {
          MXFMetadataTrack *tmp = tmp_pkg->tracks[k];

          if (tmp->track_id == component->source_track_id) {
            source_track = MXF_METADATA_TIMELINE_TRACK (tmp);
            break;
          }
        }
      }
    }

    if (track->parent.type && (track->parent.type & 0xf0) != 0x30) {
      GST_DEBUG_OBJECT (demux,
          "No essence track. type:0x%02x track_id:%d track_number:0x%08x",
          track->parent.type, track->parent.track_id,
          track->parent.track_number);
      if (!pad) {
        continue;
      } else {
        ret = GST_FLOW_ERROR;
        goto error;
      }
    }

    if (!source_package || track->parent.type == MXF_METADATA_TRACK_UNKNOWN
        || !source_track) {
      GST_WARNING_OBJECT (demux,
          "No source package or track type for track found");
      if (!pad) {
        continue;
      } else {
        ret = GST_FLOW_ERROR;
        goto error;
      }
    }

    for (k = 0; k < demux->essence_tracks->len; k++) {
      GstMXFDemuxEssenceTrack *tmp =
          g_ptr_array_index (demux->essence_tracks, k);

      if (tmp->source_package == source_package &&
          tmp->source_track == source_track) {
        etrack = tmp;
        break;
      }
    }

    if (!etrack) {
      GST_WARNING_OBJECT (demux, "No essence track for this track found");
      if (!pad) {
        continue;
      } else {
        ret = GST_FLOW_ERROR;
        goto error;
      }
    }

    if (track->edit_rate.n <= 0 || track->edit_rate.d <= 0 ||
        source_track->edit_rate.n <= 0 || source_track->edit_rate.d <= 0) {
      GST_WARNING_OBJECT (demux, "Track has an invalid edit rate");
      if (!pad) {
        continue;
      } else {
        ret = GST_FLOW_ERROR;
        goto error;
      }
    }

    if (MXF_IS_METADATA_MATERIAL_PACKAGE (current_package) && !component) {
      GST_WARNING_OBJECT (demux,
          "Playing material package but found no component for track");
      if (!pad) {
        continue;
      } else {
        ret = GST_FLOW_ERROR;
        goto error;
      }
    }

    if (!source_package->descriptor) {
      GST_WARNING_OBJECT (demux, "Source package has no descriptors");
      if (!pad) {
        continue;
      } else {
        ret = GST_FLOW_ERROR;
        goto error;
      }
    }

    if (!source_track->parent.descriptor) {
      GST_WARNING_OBJECT (demux, "No descriptor found for track");
      if (!pad) {
        continue;
      } else {
        ret = GST_FLOW_ERROR;
        goto error;
      }
    }

    if (!pad && first_run) {
      GstPadTemplate *templ;
      gchar *pad_name;

      templ =
          gst_element_class_get_pad_template (GST_ELEMENT_GET_CLASS (demux),
          "track_%u");
      pad_name = g_strdup_printf ("track_%u", track->parent.track_id);

      g_assert (templ != NULL);

      /* Create pad */
      pad = (GstMXFDemuxPad *) g_object_new (GST_TYPE_MXF_DEMUX_PAD,
          "name", pad_name, "direction", GST_PAD_SRC, "template", templ, NULL);
      pad->need_segment = TRUE;
      pad->eos = FALSE;
      g_free (pad_name);

      if (demux->tags)
        pad->tags = gst_tag_list_copy (demux->tags);
    }

    if (!pad) {
      GST_WARNING_OBJECT (demux,
          "Not the first pad addition run, ignoring new track");
      continue;
    }

    /* Update pad */
    pad->track_id = track->parent.track_id;

    pad->material_package = current_package;
    pad->material_track = track;

    pad->start_timecode = start_timecode;

    /* If we just added the pad initialize for the current component */
    if (first_run && MXF_IS_METADATA_MATERIAL_PACKAGE (current_package)) {
      pad->current_component_index = 0;
      pad->current_component_start = source_track->origin;
      pad->current_component_start_position = 0;

      if (component->parent.duration >= -1)
        pad->current_component_duration = component->parent.duration;
      else
        pad->current_component_duration = -1;

      if (track->edit_rate.n != source_track->edit_rate.n ||
          track->edit_rate.d != source_track->edit_rate.d) {
        pad->current_component_start +=
            gst_util_uint64_scale (component->start_position,
            source_track->edit_rate.n * track->edit_rate.d,
            source_track->edit_rate.d * track->edit_rate.n);

        if (pad->current_component_duration != -1)
          pad->current_component_duration =
              gst_util_uint64_scale (pad->current_component_duration,
              source_track->edit_rate.n * track->edit_rate.d,
              source_track->edit_rate.d * track->edit_rate.n);
      } else {
        pad->current_component_start += component->start_position;
      }
      pad->current_essence_track_position = pad->current_component_start;
    }

    /* NULL iff playing a source package */
    pad->current_component = component;

    pad->current_essence_track = etrack;

    if (etrack->tags) {
      if (pad->tags)
        gst_tag_list_insert (pad->tags, etrack->tags, GST_TAG_MERGE_REPLACE);
      else
        pad->tags = gst_tag_list_copy (etrack->tags);
    }

    pad_caps = gst_pad_get_current_caps (GST_PAD_CAST (pad));
    if (pad_caps && !gst_caps_is_equal (pad_caps, etrack->caps)) {
      gst_pad_set_caps (GST_PAD_CAST (pad), etrack->caps);
    } else if (!pad_caps) {
      GstEvent *event;
      gchar *stream_id;

      gst_pad_set_event_function (GST_PAD_CAST (pad),
          GST_DEBUG_FUNCPTR (gst_mxf_demux_src_event));

      gst_pad_set_query_function (GST_PAD_CAST (pad),
          GST_DEBUG_FUNCPTR (gst_mxf_demux_src_query));

      gst_pad_use_fixed_caps (GST_PAD_CAST (pad));
      gst_pad_set_active (GST_PAD_CAST (pad), TRUE);

      stream_id =
          gst_pad_create_stream_id_printf (GST_PAD_CAST (pad),
          GST_ELEMENT_CAST (demux), "%03u", pad->track_id);

      event =
          gst_pad_get_sticky_event (demux->sinkpad, GST_EVENT_STREAM_START, 0);
      if (event) {
        if (gst_event_parse_group_id (event, &demux->group_id))
          demux->have_group_id = TRUE;
        else
          demux->have_group_id = FALSE;
        gst_event_unref (event);
      } else if (!demux->have_group_id) {
        demux->have_group_id = TRUE;
        demux->group_id = gst_util_group_id_next ();
      }
      event = gst_event_new_stream_start (stream_id);
      if (demux->have_group_id)
        gst_event_set_group_id (event, demux->group_id);

      gst_pad_push_event (GST_PAD_CAST (pad), event);
      g_free (stream_id);

      gst_pad_set_caps (GST_PAD_CAST (pad), etrack->caps);

      pads = g_list_prepend (pads, gst_object_ref (pad));

      g_ptr_array_add (demux->src, pad);
      pad->discont = TRUE;
    }
    if (pad_caps)
      gst_caps_unref (pad_caps);
  }

  if (demux->src->len > 0) {
    for (i = 0; i < demux->src->len; i++) {
      GstMXFDemuxPad *pad = g_ptr_array_index (demux->src, i);

      if (!pad->material_track || !pad->material_package) {
        GST_ERROR_OBJECT (demux, "Unable to update existing pad");
        ret = GST_FLOW_ERROR;
        goto error;
      }
    }
  } else {
    GST_ERROR_OBJECT (demux, "Couldn't create any streams");
    ret = GST_FLOW_ERROR;
    goto error;
  }

  g_rw_lock_writer_unlock (&demux->metadata_lock);

  for (l = pads; l; l = l->next) {
    gst_flow_combiner_add_pad (demux->flowcombiner, l->data);
    gst_element_add_pad (GST_ELEMENT_CAST (demux), l->data);
  }
  g_list_free (pads);

  if (first_run)
    gst_element_no_more_pads (GST_ELEMENT_CAST (demux));

  /* Re-check all existing partitions for source package linking in case the
   * header partition contains data (allowed in early MXF versions) */
  for (l = demux->partitions; l; l = l->next)
    gst_mxf_demux_partition_postcheck (demux, (GstMXFDemuxPartition *) l->data);

  return GST_FLOW_OK;

error:
  g_rw_lock_writer_unlock (&demux->metadata_lock);
  return ret;
}

static GstFlowReturn
gst_mxf_demux_handle_metadata (GstMXFDemux * demux, GstMXFKLV * klv)
{
  guint16 type;
  MXFMetadata *metadata = NULL, *old = NULL;
  GstMapInfo map;
  GstFlowReturn ret = GST_FLOW_OK;

  type = GST_READ_UINT16_BE (&klv->key.u[13]);

  GST_DEBUG_OBJECT (demux,
      "Handling metadata of size %" G_GSIZE_FORMAT " at offset %"
      G_GUINT64_FORMAT " of type 0x%04x", klv->length, klv->offset, type);

  if (G_UNLIKELY (!demux->current_partition)) {
    GST_ERROR_OBJECT (demux, "Partition pack doesn't exist");
    return GST_FLOW_ERROR;
  }

  if (G_UNLIKELY (!demux->current_partition->primer.mappings)) {
    GST_ERROR_OBJECT (demux, "Primer pack doesn't exists");
    return GST_FLOW_ERROR;
  }

  if (demux->current_partition->parsed_metadata) {
    GST_DEBUG_OBJECT (demux, "Metadata of this partition was already parsed");
    return GST_FLOW_OK;
  }

  if (klv->length == 0)
    return GST_FLOW_OK;
  ret = gst_mxf_demux_fill_klv (demux, klv);
  if (ret != GST_FLOW_OK)
    return ret;

  gst_buffer_map (klv->data, &map, GST_MAP_READ);
  metadata =
      mxf_metadata_new (type, &demux->current_partition->primer, demux->offset,
      map.data, map.size);
  gst_buffer_unmap (klv->data, &map);

  if (!metadata) {
    GST_WARNING_OBJECT (demux,
        "Unknown or unhandled metadata of type 0x%04x", type);
    return GST_FLOW_OK;
  }

  old =
      g_hash_table_lookup (demux->metadata,
      &MXF_METADATA_BASE (metadata)->instance_uid);

  if (old && G_TYPE_FROM_INSTANCE (old) != G_TYPE_FROM_INSTANCE (metadata)) {
#ifndef GST_DISABLE_GST_DEBUG
    gchar str[48];
#endif

    GST_DEBUG_OBJECT (demux,
        "Metadata with instance uid %s already exists and has different type '%s',"
        " expected '%s'",
        mxf_uuid_to_string (&MXF_METADATA_BASE (metadata)->instance_uid, str),
        g_type_name (G_TYPE_FROM_INSTANCE (old)),
        g_type_name (G_TYPE_FROM_INSTANCE (metadata)));
    g_object_unref (metadata);
    return GST_FLOW_ERROR;
  } else if (old
      && MXF_METADATA_BASE (old)->offset >=
      MXF_METADATA_BASE (metadata)->offset) {
#ifndef GST_DISABLE_GST_DEBUG
    gchar str[48];
#endif

    GST_DEBUG_OBJECT (demux,
        "Metadata with instance uid %s already exists and is newer",
        mxf_uuid_to_string (&MXF_METADATA_BASE (metadata)->instance_uid, str));
    g_object_unref (metadata);
    return GST_FLOW_OK;
  }

  g_rw_lock_writer_lock (&demux->metadata_lock);
  demux->update_metadata = TRUE;

  if (MXF_IS_METADATA_PREFACE (metadata)) {
    demux->preface = MXF_METADATA_PREFACE (metadata);
  }

  gst_mxf_demux_reset_linked_metadata (demux);

  g_hash_table_replace (demux->metadata,
      &MXF_METADATA_BASE (metadata)->instance_uid, metadata);
  g_rw_lock_writer_unlock (&demux->metadata_lock);

  return ret;
}

static GstFlowReturn
gst_mxf_demux_handle_descriptive_metadata (GstMXFDemux * demux, GstMXFKLV * klv)
{
  guint32 type;
  guint8 scheme;
  GstMapInfo map;
  GstFlowReturn ret = GST_FLOW_OK;
  MXFDescriptiveMetadata *m = NULL, *old = NULL;

  scheme = GST_READ_UINT8 (&klv->key.u[12]);
  type = GST_READ_UINT24_BE (&klv->key.u[13]);

  GST_DEBUG_OBJECT (demux,
      "Handling descriptive metadata of size %" G_GSIZE_FORMAT " at offset %"
      G_GUINT64_FORMAT " with scheme 0x%02x and type 0x%06x",
      klv->length, klv->offset, scheme, type);

  if (G_UNLIKELY (!demux->current_partition)) {
    GST_ERROR_OBJECT (demux, "Partition pack doesn't exist");
    return GST_FLOW_ERROR;
  }

  if (G_UNLIKELY (!demux->current_partition->primer.mappings)) {
    GST_ERROR_OBJECT (demux, "Primer pack doesn't exists");
    return GST_FLOW_ERROR;
  }

  if (demux->current_partition->parsed_metadata) {
    GST_DEBUG_OBJECT (demux, "Metadata of this partition was already parsed");
    return GST_FLOW_OK;
  }

  ret = gst_mxf_demux_fill_klv (demux, klv);
  if (ret != GST_FLOW_OK)
    return ret;

  gst_buffer_map (klv->data, &map, GST_MAP_READ);
  m = mxf_descriptive_metadata_new (scheme, type,
      &demux->current_partition->primer, demux->offset, map.data, map.size);
  gst_buffer_unmap (klv->data, &map);

  if (!m) {
    GST_WARNING_OBJECT (demux,
        "Unknown or unhandled descriptive metadata of scheme 0x%02x and type 0x%06x",
        scheme, type);
    return GST_FLOW_OK;
  }

  old =
      g_hash_table_lookup (demux->metadata,
      &MXF_METADATA_BASE (m)->instance_uid);

  if (old && G_TYPE_FROM_INSTANCE (old) != G_TYPE_FROM_INSTANCE (m)) {
#ifndef GST_DISABLE_GST_DEBUG
    gchar str[48];
#endif

    GST_DEBUG_OBJECT (demux,
        "Metadata with instance uid %s already exists and has different type '%s',"
        " expected '%s'",
        mxf_uuid_to_string (&MXF_METADATA_BASE (m)->instance_uid, str),
        g_type_name (G_TYPE_FROM_INSTANCE (old)),
        g_type_name (G_TYPE_FROM_INSTANCE (m)));
    g_object_unref (m);
    return GST_FLOW_ERROR;
  } else if (old
      && MXF_METADATA_BASE (old)->offset >= MXF_METADATA_BASE (m)->offset) {
#ifndef GST_DISABLE_GST_DEBUG
    gchar str[48];
#endif

    GST_DEBUG_OBJECT (demux,
        "Metadata with instance uid %s already exists and is newer",
        mxf_uuid_to_string (&MXF_METADATA_BASE (m)->instance_uid, str));
    g_object_unref (m);
    return GST_FLOW_OK;
  }

  g_rw_lock_writer_lock (&demux->metadata_lock);

  demux->update_metadata = TRUE;
  gst_mxf_demux_reset_linked_metadata (demux);

  g_hash_table_replace (demux->metadata, &MXF_METADATA_BASE (m)->instance_uid,
      m);

  g_rw_lock_writer_unlock (&demux->metadata_lock);

  return ret;
}

static GstFlowReturn
gst_mxf_demux_handle_generic_container_system_item (GstMXFDemux * demux,
    GstMXFKLV * klv)
{
  GST_DEBUG_OBJECT (demux,
      "Handling generic container system item of size %" G_GSIZE_FORMAT
      " at offset %" G_GUINT64_FORMAT, klv->length, klv->offset);

  if (demux->current_partition->essence_container_offset == 0)
    demux->current_partition->essence_container_offset =
        demux->offset - demux->current_partition->partition.this_partition -
        demux->run_in;

  /* TODO: parse this */
  return GST_FLOW_OK;
}

static GstFlowReturn
gst_mxf_demux_pad_set_component (GstMXFDemux * demux, GstMXFDemuxPad * pad,
    guint i)
{
  GstFlowReturn ret = GST_FLOW_OK;
  GstCaps *pad_caps;
  MXFMetadataSequence *sequence;
  guint k;
  MXFMetadataSourcePackage *source_package = NULL;
  MXFMetadataTimelineTrack *source_track = NULL;
  gboolean update = (pad->current_component_index != i);

  pad->current_component_index = i;

  sequence = pad->material_track->parent.sequence;

  if (pad->current_component_index >= sequence->n_structural_components) {
    GST_DEBUG_OBJECT (demux, "After last structural component");
    pad->current_component_index = sequence->n_structural_components - 1;
    ret = GST_FLOW_EOS;
  }

  GST_DEBUG_OBJECT (demux, "Switching to component %u",
      pad->current_component_index);

  pad->current_component =
      MXF_METADATA_SOURCE_CLIP (sequence->structural_components[pad->
          current_component_index]);
  if (pad->current_component == NULL) {
    GST_ERROR_OBJECT (demux, "No such structural component");
    return GST_FLOW_ERROR;
  }

  if (!pad->current_component->source_package
      || !pad->current_component->source_package->top_level
      || !MXF_METADATA_GENERIC_PACKAGE (pad->current_component->
          source_package)->tracks) {
    GST_ERROR_OBJECT (demux, "Invalid component");
    return GST_FLOW_ERROR;
  }

  source_package = pad->current_component->source_package;

  for (k = 0; k < source_package->parent.n_tracks; k++) {
    MXFMetadataTrack *tmp = source_package->parent.tracks[k];

    if (tmp->track_id == pad->current_component->source_track_id) {
      source_track = MXF_METADATA_TIMELINE_TRACK (tmp);
      break;
    }
  }

  if (!source_track) {
    GST_ERROR_OBJECT (demux, "No source track found");
    return GST_FLOW_ERROR;
  }

  pad->current_essence_track = NULL;

  for (k = 0; k < demux->essence_tracks->len; k++) {
    GstMXFDemuxEssenceTrack *tmp = g_ptr_array_index (demux->essence_tracks, k);

    if (tmp->source_package == source_package &&
        tmp->source_track == source_track) {
      pad->current_essence_track = tmp;
      break;
    }
  }

  if (!pad->current_essence_track) {
    GST_ERROR_OBJECT (demux, "No corresponding essence track found");
    return GST_FLOW_ERROR;
  }

  if (!source_package->descriptor) {
    GST_ERROR_OBJECT (demux, "Source package has no descriptors");
    return GST_FLOW_ERROR;
  }

  if (!source_track->parent.descriptor) {
    GST_ERROR_OBJECT (demux, "No descriptor found for track");
    return GST_FLOW_ERROR;
  }

  if (source_track->edit_rate.n <= 0 || source_track->edit_rate.d <= 0) {
    GST_ERROR_OBJECT (demux, "Source track has invalid edit rate");
    return GST_FLOW_ERROR;
  }

  pad->current_component_start_position = 0;
  for (k = 0; k < i; k++) {
    pad->current_component_start_position +=
        MXF_METADATA_SOURCE_CLIP (sequence->structural_components[k])->
        parent.duration;
  }

  if (pad->current_component->parent.duration >= -1)
    pad->current_component_duration = pad->current_component->parent.duration;
  else
    pad->current_component_duration = -1;

  if (pad->material_track->edit_rate.n != source_track->edit_rate.n ||
      pad->material_track->edit_rate.d != source_track->edit_rate.d) {
    pad->current_component_start +=
        gst_util_uint64_scale (pad->current_component->start_position,
        source_track->edit_rate.n * pad->material_track->edit_rate.d,
        source_track->edit_rate.d * pad->material_track->edit_rate.n);

    if (pad->current_component_duration != -1)
      pad->current_component_duration =
          gst_util_uint64_scale (pad->current_component_duration,
          source_track->edit_rate.n * pad->material_track->edit_rate.d,
          source_track->edit_rate.d * pad->material_track->edit_rate.n);
  } else {
    pad->current_component_start += pad->current_component->start_position;
  }
  pad->current_essence_track_position = pad->current_component_start;

  pad_caps = gst_pad_get_current_caps (GST_PAD_CAST (pad));
  if (!pad_caps
      || !gst_caps_is_equal (pad_caps, pad->current_essence_track->caps)) {
    gst_pad_set_caps (GST_PAD_CAST (pad), pad->current_essence_track->caps);
  }
  if (pad_caps)
    gst_caps_unref (pad_caps);

  if (update) {
    if (pad->tags) {
      if (pad->current_essence_track->tags)
        gst_tag_list_insert (pad->tags, pad->current_essence_track->tags,
            GST_TAG_MERGE_REPLACE);
    } else {
      if (pad->current_essence_track->tags)
        pad->tags = gst_tag_list_copy (pad->current_essence_track->tags);
    }
  }

  if (ret == GST_FLOW_EOS) {
    pad->current_essence_track_position += pad->current_component_duration;
  }

  return ret;
}

/*
 * Find the partition containing the stream offset of the given track
 * */
static GstMXFDemuxPartition *
get_partition_for_stream_offset (GstMXFDemux * demux,
    GstMXFDemuxEssenceTrack * etrack, guint64 stream_offset)
{
  GList *tmp;
  GstMXFDemuxPartition *offset_partition = NULL, *next_partition = NULL;

  for (tmp = demux->partitions; tmp; tmp = tmp->next) {
    GstMXFDemuxPartition *partition = tmp->data;

    if (!next_partition && offset_partition)
      next_partition = partition;

    if (partition->partition.body_sid != etrack->body_sid)
      continue;
    if (partition->partition.body_offset > stream_offset)
      break;

    offset_partition = partition;
    next_partition = NULL;
  }

  if (offset_partition
      && stream_offset < offset_partition->partition.body_offset)
    return NULL;

  GST_DEBUG_OBJECT (demux,
      "Found this_partition:%" G_GUINT64_FORMAT " body_offset:%"
      G_GUINT64_FORMAT, offset_partition->partition.this_partition,
      offset_partition->partition.body_offset);

  /* Are we overriding into the next partition ? */
  if (next_partition) {
    guint64 partition_essence_size =
        next_partition->partition.this_partition -
        offset_partition->partition.this_partition +
        offset_partition->essence_container_offset;
    guint64 in_partition =
        stream_offset - offset_partition->partition.body_offset;
    GST_DEBUG_OBJECT (demux,
        "Followed by this_partition:%" G_GUINT64_FORMAT " body_offset:%"
        G_GUINT64_FORMAT, next_partition->partition.this_partition,
        next_partition->partition.body_offset);

    if (in_partition >= partition_essence_size) {
      GST_WARNING_OBJECT (demux,
          "stream_offset %" G_GUINT64_FORMAT
          " in track body_sid:% index_sid:%d leaks into next unrelated partition (body_sid:%d / index_sid:%d)",
          stream_offset, etrack->body_sid, etrack->index_sid,
          next_partition->partition.body_sid,
          next_partition->partition.index_sid);
      return NULL;
    }
  }
  return offset_partition;
}

static GstMXFDemuxIndexTable *
get_track_index_table (GstMXFDemux * demux, GstMXFDemuxEssenceTrack * etrack)
{
  GList *l;

  /* Look in the indextables */
  for (l = demux->index_tables; l; l = l->next) {
    GstMXFDemuxIndexTable *tmp = l->data;

    if (tmp->body_sid == etrack->body_sid
        && tmp->index_sid == etrack->index_sid) {
      return tmp;
    }
  }

  return NULL;
}

static guint32
get_track_max_temporal_offset (GstMXFDemux * demux,
    GstMXFDemuxEssenceTrack * etrack)
{
  GstMXFDemuxIndexTable *table;

  if (etrack->intra_only)
    return 0;

  table = get_track_index_table (demux, etrack);

  if (table)
    return table->max_temporal_offset;
  return 0;
}

static guint64
find_offset (GArray * offsets, gint64 * position, gboolean keyframe)
{
  GstMXFDemuxIndex *idx;
  guint64 current_offset = -1;
  gint64 current_position = *position;

  if (!offsets || offsets->len <= *position)
    return -1;

  idx = &g_array_index (offsets, GstMXFDemuxIndex, *position);
  if (idx->offset != 0 && (!keyframe || idx->keyframe)) {
    current_offset = idx->offset;
  } else if (idx->offset != 0) {
    current_position--;
    while (current_position >= 0) {
      GST_LOG ("current_position %" G_GINT64_FORMAT, current_position);
      idx = &g_array_index (offsets, GstMXFDemuxIndex, current_position);
      if (idx->offset == 0) {
        GST_LOG ("breaking offset 0");
        break;
      } else if (!idx->keyframe) {
        current_position--;
        continue;
      } else {
        GST_LOG ("Breaking found offset");
        current_offset = idx->offset;
        break;
      }
    }
  }

  if (current_offset == -1)
    return -1;

  *position = current_position;
  return current_offset;
}

/**
 * find_edit_entry:
 * @demux: The demuxer
 * @etrack: The target essence track
 * @position: An edit unit position
 * @keyframe: if TRUE search for supporting keyframe
 * @entry: (out): Will be filled with the matching entry information
 *
 * Finds the edit entry of @etrack for the given edit unit @position and fill
 * @entry with the information about that edit entry. If @keyframe is TRUE, the
 * supporting entry (i.e. keyframe) for the given position will be searched for.
 *
 * For frame-wrapped contents, the returned offset will be the position of the
 * KLV of the content. For clip-wrapped content, the returned offset will be the
 * position of the essence (i.e. without KLV header) and the entry will specify
 * the size (in bytes).
 *
 * The returned entry will also specify the duration (in edit units) of the
 * content, which can be different from 1 for special cases (such as raw audio
 * where multiple samples could be aggregated).
 *
 * Returns: TRUE if the entry was found and @entry was properly filled, else
 * FALSE.
 */
static gboolean
find_edit_entry (GstMXFDemux * demux, GstMXFDemuxEssenceTrack * etrack,
    gint64 position, gboolean keyframe, GstMXFDemuxIndex * entry)
{
  GstMXFDemuxIndexTable *index_table = NULL;
  guint i;
  MXFIndexTableSegment *segment = NULL;
  GstMXFDemuxPartition *offset_partition = NULL;
  guint64 stream_offset = G_MAXUINT64, absolute_offset;

  GST_DEBUG_OBJECT (demux,
      "track %d body_sid:%d index_sid:%d delta_id:%d position:%" G_GINT64_FORMAT
      " keyframe:%d", etrack->track_id, etrack->body_sid,
      etrack->index_sid, etrack->delta_id, position, keyframe);

  /* Default values */
  entry->duration = 1;
  /* By default every entry is a keyframe unless specified otherwise */
  entry->keyframe = TRUE;

  /* Look in the track offsets */
  if (etrack->offsets && etrack->offsets->len > position) {
    if (find_offset (etrack->offsets, &position, keyframe) != -1) {
      *entry = g_array_index (etrack->offsets, GstMXFDemuxIndex, position);
      GST_LOG_OBJECT (demux, "Found entry in track offsets");
      return TRUE;
    } else
      GST_LOG_OBJECT (demux, "Didn't find entry in track offsets");
  }

  /* Look in the indextables */
  index_table = get_track_index_table (demux, etrack);

  if (!index_table) {
    GST_DEBUG_OBJECT (demux,
        "Couldn't find index table for body_sid:%d index_sid:%d",
        etrack->body_sid, etrack->index_sid);
    return FALSE;
  }

  GST_DEBUG_OBJECT (demux,
      "Looking for position %" G_GINT64_FORMAT
      " in index table (max temporal offset %u)",
      etrack->position, index_table->max_temporal_offset);

  /* Searching for a position in index tables works in 3 steps:
   *
   * 1. Figure out the table segment containing that position
   * 2. Figure out the "stream offset" (and additional flags/timing) of that
   *    position from the table segment.
   * 3. Figure out the "absolute offset" of that "stream offset" using partitions
   */

search_in_segment:

  /* Find matching index segment */
  GST_DEBUG_OBJECT (demux, "Look for entry in %d segments",
      index_table->segments->len);
  for (i = 0; i < index_table->segments->len; i++) {
    MXFIndexTableSegment *cand =
        &g_array_index (index_table->segments, MXFIndexTableSegment, i);
    if (position >= cand->index_start_position && (cand->index_duration == 0
            || position <
            (cand->index_start_position + cand->index_duration))) {
      GST_DEBUG_OBJECT (demux,
          "Entry is in Segment #%d , start: %" G_GINT64_FORMAT " , duration: %"
          G_GINT64_FORMAT, i, cand->index_start_position, cand->index_duration);
      segment = cand;
      break;
    }
  }
  if (!segment) {
    GST_DEBUG_OBJECT (demux,
        "Didn't find index table segment for position %" G_GINT64_FORMAT,
        position);
    return FALSE;
  }

  /* Were we asked for a keyframe ? */
  if (keyframe) {
    if (segment->edit_unit_byte_count && !segment->n_index_entries) {
      GST_LOG_OBJECT (demux,
          "Index table without entries, directly using requested position for keyframe search");
    } else {
      gint64 candidate;
      GST_LOG_OBJECT (demux, "keyframe search");
      /* Search backwards for keyframe */
      for (candidate = position; candidate >= segment->index_start_position;
          candidate--) {
        MXFIndexEntry *segment_index_entry =
            &segment->index_entries[candidate - segment->index_start_position];

        /* Match */
        if (segment_index_entry->flags & 0x80) {
          GST_LOG_OBJECT (demux, "Found keyframe at position %" G_GINT64_FORMAT,
              candidate);
          position = candidate;
          break;
        }

        /* If a keyframe offset is specified and valid, use that */
        if (segment_index_entry->key_frame_offset
            && !(segment_index_entry->flags & 0x08)) {
          GST_DEBUG_OBJECT (demux, "Using keyframe offset %d",
              segment_index_entry->key_frame_offset);
          position = candidate + segment_index_entry->key_frame_offset;
          if (position < segment->index_start_position) {
            GST_DEBUG_OBJECT (demux, "keyframe info is in previous segment");
            goto search_in_segment;
          }
          break;
        }

        /* If we reached the beginning, use that */
        if (candidate == 0) {
          GST_LOG_OBJECT (demux,
              "Reached position 0 while searching for keyframe");
          position = 0;
          break;
        }

        /* If we looped past the beginning of this segment, go to the previous one */
        if (candidate == segment->index_start_position) {
          position = candidate - 1;
          GST_LOG_OBJECT (demux, "Looping with new position %" G_GINT64_FORMAT,
              position);
          goto search_in_segment;
        }

        /* loop back to check previous entry */
      }
    }
  }

  /* Figure out the stream offset (also called "body offset" in specification) */
  if (segment->edit_unit_byte_count && !segment->n_index_entries) {
    /* Constant entry table. */
    stream_offset = position * segment->edit_unit_byte_count;
    if (etrack->delta_id >= 0) {
      MXFDeltaEntry *delta_entry = &segment->delta_entries[etrack->delta_id];
      GST_LOG_OBJECT (demux,
          "Using delta %d pos_table_index:%d slice:%u element_delta:%u",
          etrack->delta_id, delta_entry->pos_table_index, delta_entry->slice,
          delta_entry->element_delta);
      stream_offset += delta_entry->element_delta;
    } else if (etrack->min_edit_units != 1) {
      GST_LOG_OBJECT (demux, "Handling minimum edit unit %u",
          etrack->min_edit_units);
      entry->duration =
          MIN (etrack->min_edit_units,
          (segment->index_start_position + segment->index_duration) - position);
      entry->size = segment->edit_unit_byte_count * entry->duration;
    } else {
      entry->size = segment->edit_unit_byte_count;
    }
  } else if (segment->n_index_entries) {
    MXFIndexEntry *segment_index_entry;
    MXFDeltaEntry *delta_entry = NULL;
    g_assert (position <=
        segment->index_start_position + segment->n_index_entries);
    segment_index_entry =
        &segment->index_entries[position - segment->index_start_position];
    stream_offset = segment_index_entry->stream_offset;

    if (segment->n_delta_entries > 0)
      delta_entry = &segment->delta_entries[etrack->delta_id];

    if (delta_entry) {
      GST_LOG_OBJECT (demux,
          "Using delta %d pos_table_index:%d slice:%u element_delta:%u",
          etrack->delta_id, delta_entry->pos_table_index, delta_entry->slice,
          delta_entry->element_delta);

      /* Apply offset from slice/delta if needed */
      if (delta_entry->slice)
        stream_offset +=
            segment_index_entry->slice_offset[delta_entry->slice - 1];
      stream_offset += delta_entry->element_delta;
      if (delta_entry->pos_table_index == -1) {
        entry->keyframe = (segment_index_entry->flags & 0x80) == 0x80;
      }
      /* FIXME : Handle fractional offset position (delta_entry->pos_table_offset > 0) */
    }

    /* Apply reverse temporal reordering if present */
    if (index_table->reordered_delta_entry == etrack->delta_id) {
      if (position >= index_table->reverse_temporal_offsets->len) {
        GST_WARNING_OBJECT (demux,
            "Can't apply temporal offset for position %" G_GINT64_FORMAT
            " (max:%d)", position, index_table->reverse_temporal_offsets->len);
      }
      if (demux->temporal_order_misuse) {
        GST_DEBUG_OBJECT (demux, "Handling temporal order misuse");
        entry->pts = position + segment_index_entry->temporal_offset;
      } else {
        entry->pts =
            position + g_array_index (index_table->reverse_temporal_offsets,
            gint8, position);
        GST_LOG_OBJECT (demux,
            "Applied temporal offset. dts:%" G_GINT64_FORMAT " pts:%"
            G_GINT64_FORMAT, position, entry->pts);
      }
    } else
      entry->pts = position;
  } else {
    /* Note : This should have been handled in the parser */
    GST_WARNING_OBJECT (demux,
        "Can't handle index tables without entries nor constant edit unit byte count");
    return FALSE;
  }

  /* Find the partition containing the stream offset for this track */
  offset_partition =
      get_partition_for_stream_offset (demux, etrack, stream_offset);

  if (!offset_partition) {
    GST_WARNING_OBJECT (demux,
        "Couldn't find matching partition for stream offset %" G_GUINT64_FORMAT,
        stream_offset);
    return FALSE;
  } else {
    GST_DEBUG_OBJECT (demux, "Entry is in partition %" G_GUINT64_FORMAT,
        offset_partition->partition.this_partition);
  }

  /* Convert stream offset to absolute offset using matching partition */
  absolute_offset =
      offset_partition->partition.this_partition +
      offset_partition->essence_container_offset + (stream_offset -
      offset_partition->partition.body_offset);

  GST_LOG_OBJECT (demux,
      "track %d position:%" G_GINT64_FORMAT " stream_offset %" G_GUINT64_FORMAT
      " matches to absolute offset %" G_GUINT64_FORMAT, etrack->track_id,
      position, stream_offset, absolute_offset);
  entry->initialized = TRUE;
  entry->offset = absolute_offset;
  entry->dts = position;

  return TRUE;
}

/**
 * find_entry_for_offset:
 * @demux: The demuxer
 * @etrack: The target essence track
 * @offset: An absolute byte offset (excluding run_in)
 * @entry: (out): Will be filled with the matching entry information
 *
 * Find the entry located at the given absolute byte offset.
 *
 * Note: the offset requested should be in the current partition !
 *
 * Returns: TRUE if the entry was found and @entry was properly filled, else
 * FALSE.
 */
static gboolean
find_entry_for_offset (GstMXFDemux * demux, GstMXFDemuxEssenceTrack * etrack,
    guint64 offset, GstMXFDemuxIndex * retentry)
{
  GstMXFDemuxIndexTable *index_table = get_track_index_table (demux, etrack);
  guint i;
  MXFIndexTableSegment *index_segment = NULL;
  GstMXFDemuxPartition *partition = demux->current_partition;
  guint64 original_offset = offset;
  guint64 cp_offset = 0;        /* Offset in Content Package */
  MXFIndexEntry *index_entry = NULL;
  MXFDeltaEntry *delta_entry = NULL;
  gint64 position = 0;

  GST_DEBUG_OBJECT (demux,
      "track %d body_sid:%d index_sid:%d offset:%" G_GUINT64_FORMAT,
      etrack->track_id, etrack->body_sid, etrack->index_sid, offset);

  /* Default value */
  retentry->duration = 1;
  retentry->keyframe = TRUE;

  /* Index-less search */
  if (etrack->offsets) {
    for (i = 0; i < etrack->offsets->len; i++) {
      GstMXFDemuxIndex *idx =
          &g_array_index (etrack->offsets, GstMXFDemuxIndex, i);

      if (idx->initialized && idx->offset != 0 && idx->offset == offset) {
        *retentry = *idx;
        GST_DEBUG_OBJECT (demux,
            "Found in track index. Position:%" G_GINT64_FORMAT, idx->dts);
        return TRUE;
      }
    }
  }

  /* Actual index search */
  if (!index_table || !index_table->segments->len) {
    GST_WARNING_OBJECT (demux, "No index table or entries to search in");
    return FALSE;
  }

  if (!partition) {
    GST_WARNING_OBJECT (demux, "No current partition for search");
    return FALSE;
  }

  /* Searching for a stream position from an absolute offset works in 3 steps:
   *
   * 1. Convert the absolute offset to a "stream offset" based on the partition
   *    information.
   * 2. Find the segment for that "stream offset"
   * 3. Match the entry within that segment
   */

  /* Convert to stream offset */
  GST_LOG_OBJECT (demux,
      "offset %" G_GUINT64_FORMAT " this_partition:%" G_GUINT64_FORMAT
      " essence_container_offset:%" G_GINT64_FORMAT " partition body offset %"
      G_GINT64_FORMAT, offset, partition->partition.this_partition,
      partition->essence_container_offset, partition->partition.body_offset);
  offset =
      offset - partition->partition.this_partition -
      partition->essence_container_offset + partition->partition.body_offset;

  GST_LOG_OBJECT (demux, "stream offset %" G_GUINT64_FORMAT, offset);

  /* Find the segment that covers the given stream offset (the highest one that
   * covers that offset) */
  for (i = index_table->segments->len - 1; i >= 0; i--) {
    index_segment =
        &g_array_index (index_table->segments, MXFIndexTableSegment, i);
    GST_DEBUG_OBJECT (demux,
        "Checking segment #%d (essence_offset %" G_GUINT64_FORMAT ")", i,
        index_segment->segment_start_offset);
    /* Not in the right segment yet */
    if (offset >= index_segment->segment_start_offset) {
      GST_LOG_OBJECT (demux, "Found");
      break;
    }
  }
  if (!index_segment) {
    GST_WARNING_OBJECT (demux,
        "Couldn't find index table segment for given offset");
    return FALSE;
  }

  /* In the right segment, figure out:
   * * the offset in the content package,
   * * the position in edit units
   * * the matching entry (if the table has entries)
   */
  if (index_segment->edit_unit_byte_count) {
    cp_offset = offset % index_segment->edit_unit_byte_count;
    position = offset / index_segment->edit_unit_byte_count;
    /* Boundary check */
    if ((position < index_segment->index_start_position)
        || (index_segment->index_duration
            && position >
            (index_segment->index_start_position +
                index_segment->index_duration))) {
      GST_WARNING_OBJECT (demux,
          "Invalid offset, exceeds table segment limits");
      return FALSE;
    }
    if (etrack->min_edit_units != 1) {
      retentry->duration = MIN (etrack->min_edit_units,
          (index_segment->index_start_position +
              index_segment->index_duration) - position);
      retentry->size = index_segment->edit_unit_byte_count * retentry->duration;
    } else {
      retentry->size = index_segment->edit_unit_byte_count;
    }
  } else {
    /* Find the content package entry containing this offset */
    guint cpidx;
    for (cpidx = 0; cpidx < index_segment->n_index_entries; cpidx++) {
      index_entry = &index_segment->index_entries[cpidx];
      GST_DEBUG_OBJECT (demux,
          "entry #%u offset:%" G_GUINT64_FORMAT " stream_offset:%"
          G_GUINT64_FORMAT, cpidx, offset, index_entry->stream_offset);
      if (index_entry->stream_offset == offset) {
        index_entry = &index_segment->index_entries[cpidx];
        /* exactly on the entry */
        cp_offset = offset - index_entry->stream_offset;
        position = index_segment->index_start_position + cpidx;
        break;
      }
      if (index_entry->stream_offset > offset && cpidx > 0) {
        index_entry = &index_segment->index_entries[cpidx - 1];
        /* One too far, result is in previous entry */
        cp_offset = offset - index_entry->stream_offset;
        position = index_segment->index_start_position + cpidx - 1;
        break;
      }
    }
    if (cpidx == index_segment->n_index_entries) {
      GST_WARNING_OBJECT (demux,
          "offset exceeds maximum number of entries in table segment");
      return FALSE;
    }
  }

  /* If the track comes from an interleaved essence container and doesn't have a
   * delta_id set, figure it out now */
  if (G_UNLIKELY (etrack->delta_id == MXF_INDEX_DELTA_ID_UNKNOWN)) {
    guint delta;
    GST_DEBUG_OBJECT (demux,
        "Unknown delta_id for track. Attempting to resolve it");

    if (index_segment->n_delta_entries == 0) {
      /* No delta entries, nothing we can do about this */
      GST_DEBUG_OBJECT (demux, "Index table has no delta entries, ignoring");
      etrack->delta_id = MXF_INDEX_DELTA_ID_IGNORE;
    } else if (!index_entry) {
      for (delta = 0; delta < index_segment->n_delta_entries; delta++) {
        /* No entry, therefore no slices */
        GST_LOG_OBJECT (demux,
            "delta #%d offset %" G_GUINT64_FORMAT " cp_offs:%" G_GUINT64_FORMAT
            " element_delta:%u", delta, offset, cp_offset,
            index_segment->delta_entries[delta].element_delta);
        if (cp_offset == index_segment->delta_entries[delta].element_delta) {
          GST_DEBUG_OBJECT (demux, "Matched to delta %d", delta);
          etrack->delta_id = delta;
          delta_entry = &index_segment->delta_entries[delta];
          break;
        }
      }
    } else {
      for (delta = 0; delta < index_segment->n_delta_entries; delta++) {
        guint64 delta_offs = 0;
        /* If we are not in the first slice, take that offset into account */
        if (index_segment->delta_entries[delta].slice)
          delta_offs =
              index_entry->slice_offset[index_segment->
              delta_entries[delta].slice - 1];
        /* Add the offset for this delta */
        delta_offs += index_segment->delta_entries[delta].element_delta;
        if (cp_offset == delta_offs) {
          GST_DEBUG_OBJECT (demux, "Matched to delta %d", delta);
          etrack->delta_id = delta;
          delta_entry = &index_segment->delta_entries[delta];
          break;
        }
      }

    }
    /* If we didn't managed to match, ignore it from now on */
    if (etrack->delta_id == MXF_INDEX_DELTA_ID_UNKNOWN) {
      GST_WARNING_OBJECT (demux,
          "Couldn't match delta id, ignoring it from now on");
      etrack->delta_id = MXF_INDEX_DELTA_ID_IGNORE;
    }
  } else if (index_segment->n_delta_entries > 0) {
    delta_entry = &index_segment->delta_entries[etrack->delta_id];
  }

  if (index_entry && delta_entry && delta_entry->pos_table_index == -1) {
    retentry->keyframe = (index_entry->flags & 0x80) == 0x80;
    if (!demux->temporal_order_misuse)
      retentry->pts =
          position + g_array_index (index_table->reverse_temporal_offsets,
          gint8, position);
    else
      retentry->pts = position + index_entry->temporal_offset;
    GST_LOG_OBJECT (demux,
        "Applied temporal offset. dts:%" G_GINT64_FORMAT " pts:%"
        G_GINT64_FORMAT, position, retentry->pts);
  } else
    retentry->pts = position;

  /* FIXME : check if position and cp_offs matches the table */
  GST_LOG_OBJECT (demux, "Found in index table. position:%" G_GINT64_FORMAT,
      position);
  retentry->initialized = TRUE;
  retentry->offset = original_offset;
  retentry->dts = position;

  return TRUE;
}

static GstFlowReturn
gst_mxf_demux_handle_generic_container_essence_element (GstMXFDemux * demux,
    GstMXFKLV * klv, gboolean peek)
{
  GstFlowReturn ret = GST_FLOW_OK;
  guint32 track_number;
  guint i;
  GstBuffer *inbuf = NULL;
  GstBuffer *outbuf = NULL;
  GstMXFDemuxEssenceTrack *etrack = NULL;
  /* As in GstMXFDemuxIndex */
  guint64 pts = G_MAXUINT64;
  gint32 max_temporal_offset = 0;
  GstMXFDemuxIndex index_entry = { 0, };
  guint64 offset;

  GST_DEBUG_OBJECT (demux,
      "Handling generic container essence element of size %" G_GSIZE_FORMAT
      " at offset %" G_GUINT64_FORMAT, klv->length,
      klv->offset + klv->consumed);

  GST_DEBUG_OBJECT (demux, "  type = 0x%02x", klv->key.u[12]);
  GST_DEBUG_OBJECT (demux, "  essence element count = 0x%02x", klv->key.u[13]);
  GST_DEBUG_OBJECT (demux, "  essence element type = 0x%02x", klv->key.u[14]);
  GST_DEBUG_OBJECT (demux, "  essence element number = 0x%02x", klv->key.u[15]);

  if (demux->current_partition->essence_container_offset == 0) {
    demux->current_partition->essence_container_offset =
        demux->offset - demux->current_partition->partition.this_partition -
        demux->run_in;
    if (demux->current_partition->single_track
        && demux->current_partition->single_track->wrapping !=
        MXF_ESSENCE_WRAPPING_FRAME_WRAPPING) {
      demux->current_partition->essence_container_offset += klv->data_offset;
      demux->current_partition->clip_klv = *klv;
      /* "consume" the initial bytes of the KLV */
      klv->consumed = klv->data_offset;
      GST_DEBUG_OBJECT (demux,
          "Non-frame wrapping, updated essence_container_offset to %"
          G_GUINT64_FORMAT, demux->current_partition->essence_container_offset);
    }
  }

  if (!demux->current_package) {
    GST_ERROR_OBJECT (demux, "No package selected yet");
    return GST_FLOW_ERROR;
  }

  if (demux->src->len == 0) {
    GST_ERROR_OBJECT (demux, "No streams created yet");
    return GST_FLOW_ERROR;
  }

  if (demux->essence_tracks->len == 0) {
    GST_ERROR_OBJECT (demux, "No essence streams found in the metadata");
    return GST_FLOW_ERROR;
  }

  /* Identify and fetch the essence track */
  track_number = GST_READ_UINT32_BE (&klv->key.u[12]);

  etrack = demux->current_partition->single_track;
  if (!etrack) {
    for (i = 0; i < demux->essence_tracks->len; i++) {
      GstMXFDemuxEssenceTrack *tmp =
          g_ptr_array_index (demux->essence_tracks, i);

      if (tmp->body_sid == demux->current_partition->partition.body_sid &&
          (tmp->track_number == track_number || tmp->track_number == 0)) {
        etrack = tmp;
        break;
      }
    }

    if (!etrack) {
      GST_DEBUG_OBJECT (demux,
          "No essence track for this essence element found");
      return GST_FLOW_OK;
    }
  }

  GST_DEBUG_OBJECT (demux,
      "Handling generic container essence (track %d , position:%"
      G_GINT64_FORMAT ", number: 0x%08x , frame-wrapped:%d)", etrack->track_id,
      etrack->position, track_number,
      etrack->wrapping == MXF_ESSENCE_WRAPPING_FRAME_WRAPPING);

  /* Fetch the current entry.
   *
   * 1. If we don't have a current position, use find_entry_for_offset()
   * 2. If we do have a position, use find_edit_entry()
   *
   * 3. If we are dealing with frame-wrapped content, pull the corresponding
   *    data from upstream (because it wasn't provided). If we didn't find an
   *    entry, error out because we can't deal with a frame-wrapped stream
   *    without index.
   */

  offset = klv->offset + klv->consumed;

  /* Update the track position (in case of resyncs) */
  if (etrack->position == -1) {
    GST_DEBUG_OBJECT (demux,
        "Unknown essence track position, looking into index");
    if (!find_entry_for_offset (demux, etrack, offset - demux->run_in,
            &index_entry)) {
      GST_WARNING_OBJECT (demux, "Essence track position not in index");
      return GST_FLOW_OK;
    }
    /* Update track position */
    etrack->position = index_entry.dts;
  } else if (etrack->delta_id == MXF_INDEX_DELTA_ID_UNKNOWN) {
    GST_DEBUG_OBJECT (demux,
        "Unknown essence track delta_id, looking into index");
    if (!find_entry_for_offset (demux, etrack, offset - demux->run_in,
            &index_entry)) {
      /* Non-fatal, fallback to legacy mode */
      GST_WARNING_OBJECT (demux, "Essence track position not in index");
    } else if (etrack->position != index_entry.dts) {
      GST_ERROR_OBJECT (demux,
          "track position doesn't match %" G_GINT64_FORMAT " entry dts %"
          G_GINT64_FORMAT, etrack->position, index_entry.dts);
      return GST_FLOW_ERROR;
    }
  } else {
    if (!find_edit_entry (demux, etrack, etrack->position, FALSE, &index_entry)) {
      GST_DEBUG_OBJECT (demux, "Couldn't find entry");
    } else if (etrack->wrapping == MXF_ESSENCE_WRAPPING_FRAME_WRAPPING) {
      if (etrack->delta_id != MXF_INDEX_DELTA_ID_IGNORE
          && index_entry.offset != offset) {
        GST_ERROR_OBJECT (demux,
            "demux offset doesn't match %" G_GINT64_FORMAT " entry offset %"
            G_GUINT64_FORMAT, offset, index_entry.offset);
        return GST_FLOW_ERROR;
      }
    } else if (index_entry.offset != klv->offset + klv->consumed &&
        index_entry.offset != klv->offset + klv->data_offset) {
      GST_ERROR_OBJECT (demux,
          "KLV offset doesn't match %" G_GINT64_FORMAT " entry offset %"
          G_GUINT64_FORMAT, klv->offset + klv->consumed, index_entry.offset);
      return GST_FLOW_ERROR;
    }
  }

  if (etrack->wrapping != MXF_ESSENCE_WRAPPING_FRAME_WRAPPING) {
    /* We need entry information to deal with non-frame-wrapped content */
    if (!index_entry.initialized) {
      GST_ELEMENT_ERROR (demux, STREAM, WRONG_TYPE, (NULL),
          ("Essence with non-frame-wrapping require an index table to be present"));
      return GST_FLOW_ERROR;
    }
    /* We cannot deal with non-frame-wrapping in push mode for now */
    if (!demux->random_access) {
      GST_ELEMENT_ERROR (demux, STREAM, WRONG_TYPE, (NULL),
          ("Non-frame-wrapping is not support in push mode"));
      return GST_FLOW_ERROR;
    }
  }

  /* FIXME : If we're peeking and don't need to actually parse the data, we
   * should avoid pulling the content from upstream */
  if (etrack->wrapping != MXF_ESSENCE_WRAPPING_FRAME_WRAPPING) {
    g_assert (index_entry.size);
    GST_DEBUG_OBJECT (demux, "Should only grab %" G_GUINT64_FORMAT " bytes",
        index_entry.size);
    ret =
        gst_mxf_demux_pull_range (demux, index_entry.offset, index_entry.size,
        &inbuf);
    if (ret != GST_FLOW_OK)
      return ret;
    if (klv->consumed == 0)
      klv->consumed = klv->data_offset + index_entry.size;
    else
      klv->consumed += index_entry.size;
    if (klv != &demux->current_partition->clip_klv)
      demux->current_partition->clip_klv = *klv;
    GST_LOG_OBJECT (demux,
        "klv data_offset:%" G_GUINT64_FORMAT " length:%" G_GSIZE_FORMAT
        " consumed:%" G_GUINT64_FORMAT, klv->data_offset, klv->length,
        klv->consumed);
    /* Switch back to KLV mode if we're done with this one */
    if (klv->length + klv->data_offset == klv->consumed)
      demux->state = GST_MXF_DEMUX_STATE_KLV;
    else
      demux->state = GST_MXF_DEMUX_STATE_ESSENCE;
  } else {

    ret = gst_mxf_demux_fill_klv (demux, klv);
    if (ret != GST_FLOW_OK)
      return ret;

    /* Create subbuffer to be able to change metadata */
    inbuf =
        gst_buffer_copy_region (klv->data, GST_BUFFER_COPY_ALL, 0,
        gst_buffer_get_size (klv->data));

  }

  if (index_entry.initialized) {
    GST_DEBUG_OBJECT (demux, "Got entry dts:%" G_GINT64_FORMAT " keyframe:%d",
        index_entry.dts, index_entry.keyframe);
  }
  if (index_entry.initialized && !index_entry.keyframe)
    GST_BUFFER_FLAG_SET (inbuf, GST_BUFFER_FLAG_DELTA_UNIT);

  if (etrack->handle_func) {
    /* Takes ownership of inbuf */
    ret =
        etrack->handle_func (&klv->key, inbuf, etrack->caps,
        etrack->source_track, etrack->mapping_data, &outbuf);
    inbuf = NULL;
  } else {
    outbuf = inbuf;
    inbuf = NULL;
    ret = GST_FLOW_OK;
  }

  if (ret != GST_FLOW_OK) {
    GST_ERROR_OBJECT (demux, "Failed to handle essence element");
    if (outbuf) {
      gst_buffer_unref (outbuf);
      outbuf = NULL;
    }
    return ret;
  }

  if (!index_entry.initialized) {
    /* This can happen when doing scanning without entry tables */
    index_entry.duration = 1;
    index_entry.offset = demux->offset - demux->run_in;
    index_entry.dts = etrack->position;
    index_entry.pts = etrack->intra_only ? etrack->position : G_MAXUINT64;
    index_entry.keyframe =
        !GST_BUFFER_FLAG_IS_SET (outbuf, GST_BUFFER_FLAG_DELTA_UNIT);
    index_entry.initialized = TRUE;
    GST_DEBUG_OBJECT (demux,
        "Storing newly discovered information on track %d. dts: %"
        G_GINT64_FORMAT " offset:%" G_GUINT64_FORMAT " keyframe:%d",
        etrack->track_id, index_entry.dts, index_entry.offset,
        index_entry.keyframe);

    if (!etrack->offsets)
      etrack->offsets = g_array_new (FALSE, TRUE, sizeof (GstMXFDemuxIndex));

    /* We only ever append to the track offset entry. */
    g_assert (etrack->position <= etrack->offsets->len);
    g_array_insert_val (etrack->offsets, etrack->position, index_entry);
  }

  if (peek)
    goto out;

  if (!outbuf) {
    GST_DEBUG_OBJECT (demux, "No output buffer created");
    goto out;
  }

  inbuf = outbuf;
  outbuf = NULL;

  max_temporal_offset = get_track_max_temporal_offset (demux, etrack);

  for (i = 0; i < demux->src->len; i++) {
    GstMXFDemuxPad *pad = g_ptr_array_index (demux->src, i);

    if (pad->current_essence_track != etrack)
      continue;

    if (pad->eos) {
      GST_DEBUG_OBJECT (pad, "Pad is already EOS");
      continue;
    }

    if (etrack->position < pad->current_essence_track_position) {
      GST_DEBUG_OBJECT (pad,
          "Not at current component's position (track:%" G_GINT64_FORMAT
          " essence:%" G_GINT64_FORMAT ")", etrack->position,
          pad->current_essence_track_position);
      continue;
    }

    {
      GstMXFDemuxPad *earliest = gst_mxf_demux_get_earliest_pad (demux);

      if (earliest && earliest != pad && earliest->position < pad->position &&
          pad->position - earliest->position > demux->max_drift) {
        GST_DEBUG_OBJECT (earliest,
            "Pad is too far ahead of time (%" GST_TIME_FORMAT " vs earliest:%"
            GST_TIME_FORMAT ")", GST_TIME_ARGS (earliest->position),
            GST_TIME_ARGS (pad->position));
        continue;
      }
    }

    /* Create another subbuffer to have writable metadata */
    outbuf =
        gst_buffer_copy_region (inbuf, GST_BUFFER_COPY_ALL, 0,
        gst_buffer_get_size (inbuf));

    pts = index_entry.pts;

    GST_BUFFER_DTS (outbuf) = pad->position;
    if (etrack->intra_only) {
      GST_BUFFER_PTS (outbuf) = pad->position;
    } else if (pts != G_MAXUINT64) {
      GST_BUFFER_PTS (outbuf) = gst_util_uint64_scale (pts * GST_SECOND,
          pad->current_essence_track->source_track->edit_rate.d,
          pad->current_essence_track->source_track->edit_rate.n);
      GST_BUFFER_PTS (outbuf) +=
          gst_util_uint64_scale (pad->current_component_start_position *
          GST_SECOND, pad->material_track->edit_rate.d,
          pad->material_track->edit_rate.n);
      /* We are dealing with reordered data, the PTS is shifted forward by the
       * maximum temporal reordering (the DTS remain as-is). */
      if (max_temporal_offset > 0)
        GST_BUFFER_PTS (outbuf) +=
            gst_util_uint64_scale (max_temporal_offset * GST_SECOND,
            pad->current_essence_track->source_track->edit_rate.d,
            pad->current_essence_track->source_track->edit_rate.n);

    } else {
      GST_BUFFER_PTS (outbuf) = GST_CLOCK_TIME_NONE;
    }

    GST_BUFFER_DURATION (outbuf) =
        gst_util_uint64_scale (GST_SECOND,
        index_entry.duration *
        pad->current_essence_track->source_track->edit_rate.d,
        pad->current_essence_track->source_track->edit_rate.n);
    GST_BUFFER_OFFSET (outbuf) = GST_BUFFER_OFFSET_NONE;
    GST_BUFFER_OFFSET_END (outbuf) = GST_BUFFER_OFFSET_NONE;

    if (pad->material_track->parent.type == MXF_METADATA_TRACK_PICTURE_ESSENCE
        && pad->start_timecode.config.fps_n != 0
        && pad->start_timecode.config.fps_d != 0) {
      if (etrack->intra_only) {
        GstVideoTimeCode timecode = pad->start_timecode;

        gst_video_time_code_add_frames (&timecode,
            pad->current_material_track_position);
        gst_buffer_add_video_time_code_meta (outbuf, &timecode);
      } else if (pts != G_MAXUINT64) {
        GstVideoTimeCode timecode = pad->start_timecode;

        gst_video_time_code_add_frames (&timecode,
            pad->current_component_start_position);
        gst_video_time_code_add_frames (&timecode,
            gst_util_uint64_scale (pts,
                pad->material_track->edit_rate.n *
                pad->current_essence_track->source_track->edit_rate.d,
                pad->material_track->edit_rate.d *
                pad->current_essence_track->source_track->edit_rate.n));
        gst_buffer_add_video_time_code_meta (outbuf, &timecode);
      }

    }

    /* Update accumulated error and compensate */
    {
      guint64 abs_error =
          (GST_SECOND * pad->current_essence_track->source_track->edit_rate.d) %
          pad->current_essence_track->source_track->edit_rate.n;
      pad->position_accumulated_error +=
          ((gdouble) abs_error) /
          ((gdouble) pad->current_essence_track->source_track->edit_rate.n);
    }
    if (pad->position_accumulated_error >= 1.0) {
      GST_BUFFER_DURATION (outbuf) += 1;
      pad->position_accumulated_error -= 1.0;
    }

    if (pad->need_segment) {
      GstEvent *e;

      if (demux->close_seg_event)
        gst_pad_push_event (GST_PAD_CAST (pad),
            gst_event_ref (demux->close_seg_event));

      if (max_temporal_offset > 0) {
        GstSegment shift_segment;
        /* Handle maximum temporal offset. We are shifting all output PTS for
         * this stream by the greatest temporal reordering that can occur. In
         * order not to change the stream/running time we shift the segment
         * start and stop values accordingly */
        gst_segment_copy_into (&demux->segment, &shift_segment);
        if (GST_CLOCK_TIME_IS_VALID (shift_segment.start))
          shift_segment.start +=
              gst_util_uint64_scale (max_temporal_offset * GST_SECOND,
              pad->current_essence_track->source_track->edit_rate.d,
              pad->current_essence_track->source_track->edit_rate.n);
        if (GST_CLOCK_TIME_IS_VALID (shift_segment.stop))
          shift_segment.stop +=
              gst_util_uint64_scale (max_temporal_offset * GST_SECOND,
              pad->current_essence_track->source_track->edit_rate.d,
              pad->current_essence_track->source_track->edit_rate.n);
        e = gst_event_new_segment (&shift_segment);
      } else
        e = gst_event_new_segment (&demux->segment);
      GST_DEBUG_OBJECT (pad, "Sending segment %" GST_PTR_FORMAT, e);
      gst_event_set_seqnum (e, demux->seqnum);
      gst_pad_push_event (GST_PAD_CAST (pad), e);
      pad->need_segment = FALSE;
    }

    if (pad->tags) {
      gst_pad_push_event (GST_PAD_CAST (pad), gst_event_new_tag (pad->tags));
      pad->tags = NULL;
    }

    pad->position += GST_BUFFER_DURATION (outbuf);
    pad->current_material_track_position += index_entry.duration;

    if (pad->discont) {
      GST_BUFFER_FLAG_SET (outbuf, GST_BUFFER_FLAG_DISCONT);
      pad->discont = FALSE;
    }

    /* Handlers can provide empty GAP buffers to indicate that the parsed
     * content was valid but that nothing meaningful needs to be outputted. In
     * such cases we send out a GAP event instead */
    if (GST_BUFFER_FLAG_IS_SET (outbuf, GST_BUFFER_FLAG_GAP) &&
        gst_buffer_get_size (outbuf) == 0) {
      GstEvent *gap = gst_event_new_gap (GST_BUFFER_DTS (outbuf),
          GST_BUFFER_DURATION (outbuf));
      gst_buffer_unref (outbuf);
      GST_DEBUG_OBJECT (pad,
          "Replacing empty gap buffer with gap event %" GST_PTR_FORMAT, gap);
      gst_pad_push_event (GST_PAD_CAST (pad), gap);
    } else {
      GST_DEBUG_OBJECT (pad,
          "Pushing buffer of size %" G_GSIZE_FORMAT " for track %u: pts %"
          GST_TIME_FORMAT " dts %" GST_TIME_FORMAT " duration %" GST_TIME_FORMAT
          " position %" G_GUINT64_FORMAT, gst_buffer_get_size (outbuf),
          pad->material_track->parent.track_id,
          GST_TIME_ARGS (GST_BUFFER_PTS (outbuf)),
          GST_TIME_ARGS (GST_BUFFER_DTS (outbuf)),
          GST_TIME_ARGS (GST_BUFFER_DURATION (outbuf)),
          pad->current_essence_track_position);

      ret = gst_pad_push (GST_PAD_CAST (pad), outbuf);
    }
    outbuf = NULL;
    ret = gst_flow_combiner_update_flow (demux->flowcombiner, ret);
    GST_LOG_OBJECT (pad, "combined return %s", gst_flow_get_name (ret));

    if (pad->position > demux->segment.position)
      demux->segment.position = pad->position;

    if (ret != GST_FLOW_OK)
      goto out;

    pad->current_essence_track_position += index_entry.duration;

    if (pad->current_component) {
      if (pad->current_component_duration > 0 &&
          pad->current_essence_track_position - pad->current_component_start
          >= pad->current_component_duration) {
        GST_DEBUG_OBJECT (demux, "Switching to next component");

        ret =
            gst_mxf_demux_pad_set_component (demux, pad,
            pad->current_component_index + 1);
        if (ret == GST_FLOW_OK) {
          pad->current_essence_track->position =
              pad->current_essence_track_position;
        } else if (ret != GST_FLOW_EOS) {
          GST_ERROR_OBJECT (demux, "Switching component failed");
        }
      } else if (etrack->duration > 0
          && pad->current_essence_track_position >= etrack->duration) {
        GST_DEBUG_OBJECT (demux,
            "Current component position after end of essence track");
        ret = GST_FLOW_EOS;
      }
    } else if (etrack->duration > 0
        && pad->current_essence_track_position == etrack->duration) {
      GST_DEBUG_OBJECT (demux, "At the end of the essence track");
      ret = GST_FLOW_EOS;
    }

    if (ret == GST_FLOW_EOS) {
      GstEvent *e;

      GST_DEBUG_OBJECT (pad, "EOS for track");
      pad->eos = TRUE;
      e = gst_event_new_eos ();
      gst_event_set_seqnum (e, demux->seqnum);
      gst_pad_push_event (GST_PAD_CAST (pad), e);
      ret = GST_FLOW_OK;
    }

    if (ret != GST_FLOW_OK)
      goto out;
  }

out:
  if (inbuf)
    gst_buffer_unref (inbuf);

  if (outbuf)
    gst_buffer_unref (outbuf);

  etrack->position += index_entry.duration;

  return ret;
}

/*
 * Called when analyzing the (RIP) Random Index Pack.
 *
 * FIXME : If a file doesn't have a RIP, we should iterate the partition headers
 * to collect as much information as possible.
 *
 * This function collects as much information as possible from the partition headers:
 * * Store partition information in the list of partitions
 * * Handle any index table segment present
 */
static void
read_partition_header (GstMXFDemux * demux)
{
  GstMXFKLV klv;

  if (gst_mxf_demux_peek_klv_packet (demux, demux->offset, &klv) != GST_FLOW_OK
      || !mxf_is_partition_pack (&klv.key)) {
    return;
  }

  if (gst_mxf_demux_handle_partition_pack (demux, &klv) != GST_FLOW_OK) {
    if (klv.data)
      gst_buffer_unref (klv.data);
    return;
  }
  gst_mxf_demux_consume_klv (demux, &klv);

  if (gst_mxf_demux_peek_klv_packet (demux, demux->offset, &klv) != GST_FLOW_OK)
    return;

  while (mxf_is_fill (&klv.key)) {
    gst_mxf_demux_consume_klv (demux, &klv);
    if (gst_mxf_demux_peek_klv_packet (demux, demux->offset,
            &klv) != GST_FLOW_OK)
      return;
  }

  if (!mxf_is_index_table_segment (&klv.key)
      && demux->current_partition->partition.header_byte_count) {
    demux->offset += demux->current_partition->partition.header_byte_count;
    if (gst_mxf_demux_peek_klv_packet (demux, demux->offset,
            &klv) != GST_FLOW_OK)
      return;
  }

  while (mxf_is_fill (&klv.key)) {
    gst_mxf_demux_consume_klv (demux, &klv);
    if (gst_mxf_demux_peek_klv_packet (demux, demux->offset,
            &klv) != GST_FLOW_OK)
      return;
  }

  if (demux->current_partition->partition.index_byte_count
      && mxf_is_index_table_segment (&klv.key)) {
    guint64 index_end_offset =
        demux->offset + demux->current_partition->partition.index_byte_count;

    while (demux->offset < index_end_offset) {
      if (mxf_is_index_table_segment (&klv.key))
        gst_mxf_demux_handle_index_table_segment (demux, &klv);
      gst_mxf_demux_consume_klv (demux, &klv);

      if (gst_mxf_demux_peek_klv_packet (demux, demux->offset,
              &klv) != GST_FLOW_OK)
        return;
    }
  }

  while (mxf_is_fill (&klv.key)) {
    gst_mxf_demux_consume_klv (demux, &klv);
    if (gst_mxf_demux_peek_klv_packet (demux, demux->offset,
            &klv) != GST_FLOW_OK)
      return;
  }

  if (mxf_is_generic_container_system_item (&klv.key) ||
      mxf_is_generic_container_essence_element (&klv.key) ||
      mxf_is_avid_essence_container_essence_element (&klv.key)) {
    if (demux->current_partition->essence_container_offset == 0)
      demux->current_partition->essence_container_offset =
          demux->offset - demux->current_partition->partition.this_partition -
          demux->run_in;
  }
}

static GstFlowReturn
gst_mxf_demux_handle_random_index_pack (GstMXFDemux * demux, GstMXFKLV * klv)
{
  guint i;
  GList *l;
  GstMapInfo map;
  gboolean ret;
  GstFlowReturn flowret;

  GST_DEBUG_OBJECT (demux,
      "Handling random index pack of size %" G_GSIZE_FORMAT " at offset %"
      G_GUINT64_FORMAT, klv->length, klv->offset);

  if (demux->random_index_pack) {
    GST_DEBUG_OBJECT (demux, "Already parsed random index pack");
    return GST_FLOW_OK;
  }

  flowret = gst_mxf_demux_fill_klv (demux, klv);
  if (flowret != GST_FLOW_OK)
    return flowret;

  gst_buffer_map (klv->data, &map, GST_MAP_READ);
  ret =
      mxf_random_index_pack_parse (&klv->key, map.data, map.size,
      &demux->random_index_pack);
  gst_buffer_unmap (klv->data, &map);

  if (!ret) {
    GST_ERROR_OBJECT (demux, "Parsing random index pack failed");
    return GST_FLOW_ERROR;
  }

  for (i = 0; i < demux->random_index_pack->len; i++) {
    GstMXFDemuxPartition *p = NULL;
    MXFRandomIndexPackEntry *e =
        &g_array_index (demux->random_index_pack, MXFRandomIndexPackEntry, i);

    if (e->offset < demux->run_in) {
      GST_ERROR_OBJECT (demux, "Invalid random index pack entry");
      return GST_FLOW_ERROR;
    }

    for (l = demux->partitions; l; l = l->next) {
      GstMXFDemuxPartition *tmp = l->data;

      if (tmp->partition.this_partition + demux->run_in == e->offset) {
        p = tmp;
        break;
      }
    }

    if (!p) {
      p = g_new0 (GstMXFDemuxPartition, 1);
      p->partition.this_partition = e->offset - demux->run_in;
      p->partition.body_sid = e->body_sid;
      demux->partitions =
          g_list_insert_sorted (demux->partitions, p,
          (GCompareFunc) gst_mxf_demux_partition_compare);
    }
  }

  for (l = demux->partitions; l; l = l->next) {
    GstMXFDemuxPartition *a, *b;

    if (l->next == NULL)
      break;

    a = l->data;
    b = l->next->data;

    b->partition.prev_partition = a->partition.this_partition;
  }

  return GST_FLOW_OK;
}

static gint
compare_index_table_segment (MXFIndexTableSegment * sa,
    MXFIndexTableSegment * sb)
{
  if (sa->body_sid != sb->body_sid)
    return (sa->body_sid < sb->body_sid) ? -1 : 1;
  if (sa->index_sid != sb->index_sid)
    return (sa->index_sid < sb->index_sid) ? -1 : 1;
  if (sa->index_start_position != sb->index_start_position)
    return (sa->index_start_position < sb->index_start_position) ? -1 : 1;

  /* If all the above are equal ... the index table segments are only equal if
   * their instance ID are equal. Until March 2022 the FFmpeg MXF muxer would
   * write the same instance id for the various (different) index table
   * segments, we therefore only check instance ID *after* all the above
   * properties to make sure they are really different. */
  if (mxf_uuid_is_equal (&sa->instance_id, &sb->instance_id))
    return 0;

  return 1;
}

static GstFlowReturn
gst_mxf_demux_handle_index_table_segment (GstMXFDemux * demux, GstMXFKLV * klv)
{
  MXFIndexTableSegment *segment;
  GstMapInfo map;
  gboolean ret;
  GList *tmp;
  GstFlowReturn flowret;

  flowret = gst_mxf_demux_fill_klv (demux, klv);
  if (flowret != GST_FLOW_OK)
    return flowret;

  GST_DEBUG_OBJECT (demux,
      "Handling index table segment of size %" G_GSIZE_FORMAT " at offset %"
      G_GUINT64_FORMAT, klv->length, klv->offset);

  segment = g_new0 (MXFIndexTableSegment, 1);

  gst_buffer_map (klv->data, &map, GST_MAP_READ);
  ret = mxf_index_table_segment_parse (&klv->key, segment, map.data, map.size);
  gst_buffer_unmap (klv->data, &map);

  if (!ret) {
    GST_ERROR_OBJECT (demux, "Parsing index table segment failed");
    g_free (segment);
    return GST_FLOW_ERROR;
  }

  /* Drop it if we already saw it. Ideally we should be able to do this before
     parsing (by checking instance UID) */
  if (g_list_find_custom (demux->pending_index_table_segments, segment,
          (GCompareFunc) compare_index_table_segment)) {
    GST_DEBUG_OBJECT (demux, "Already in pending list");
    mxf_index_table_segment_reset (segment);
    g_free (segment);
    return GST_FLOW_OK;
  }
  for (tmp = demux->index_tables; tmp; tmp = tmp->next) {
    GstMXFDemuxIndexTable *table = (GstMXFDemuxIndexTable *) tmp->data;
    if (g_array_binary_search (table->segments, segment,
            (GCompareFunc) compare_index_table_segment, NULL)) {
      GST_DEBUG_OBJECT (demux, "Already handled");
      mxf_index_table_segment_reset (segment);
      g_free (segment);
      return GST_FLOW_OK;
    }
  }

  demux->pending_index_table_segments =
      g_list_insert_sorted (demux->pending_index_table_segments, segment,
      (GCompareFunc) compare_index_table_segment);

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_mxf_demux_peek_klv_packet (GstMXFDemux * demux, guint64 offset,
    GstMXFKLV * klv)
{
  GstBuffer *buffer = NULL;
  const guint8 *data;
  GstFlowReturn ret = GST_FLOW_OK;
  GstMapInfo map;
#ifndef GST_DISABLE_GST_DEBUG
  gchar str[48];
#endif

  memset (klv, 0, sizeof (GstMXFKLV));
  klv->offset = offset;

  /* Pull 16 byte key and first byte of BER encoded length */
  if ((ret =
          gst_mxf_demux_pull_range (demux, offset, 17, &buffer)) != GST_FLOW_OK)
    goto beach;

  gst_buffer_map (buffer, &map, GST_MAP_READ);

  memcpy (&klv->key, map.data, 16);

  /* Decode BER encoded packet length */
  if ((map.data[16] & 0x80) == 0) {
    klv->length = map.data[16];
    klv->data_offset = 17;
  } else {
    guint slen = map.data[16] & 0x7f;

    klv->data_offset = 16 + 1 + slen;

    gst_buffer_unmap (buffer, &map);
    gst_buffer_unref (buffer);
    buffer = NULL;

    /* Must be at most 8 according to SMPTE-379M 5.3.4 */
    if (slen > 8) {
      GST_ERROR_OBJECT (demux, "Invalid KLV packet length: %u", slen);
      ret = GST_FLOW_ERROR;
      goto beach;
    }

    /* Now pull the length of the packet */
    if ((ret = gst_mxf_demux_pull_range (demux, offset + 17, slen,
                &buffer)) != GST_FLOW_OK)
      goto beach;

    gst_buffer_map (buffer, &map, GST_MAP_READ);

    data = map.data;
    klv->length = 0;
    while (slen) {
      klv->length = (klv->length << 8) | *data;
      data++;
      slen--;
    }
  }

  gst_buffer_unmap (buffer, &map);
  gst_buffer_unref (buffer);
  buffer = NULL;

  /* GStreamer's buffer sizes are stored in a guint so we
   * limit ourself to G_MAXUINT large buffers */
  if (klv->length > G_MAXUINT) {
    GST_ERROR_OBJECT (demux,
        "Unsupported KLV packet length: %" G_GSIZE_FORMAT, klv->length);
    ret = GST_FLOW_ERROR;
    goto beach;
  }

  GST_DEBUG_OBJECT (demux,
      "Found KLV packet at offset %" G_GUINT64_FORMAT " with key %s and length "
      "%" G_GSIZE_FORMAT, offset, mxf_ul_to_string (&klv->key, str),
      klv->length);

beach:
  if (buffer)
    gst_buffer_unref (buffer);

  return ret;
}

static GstFlowReturn
gst_mxf_demux_fill_klv (GstMXFDemux * demux, GstMXFKLV * klv)
{
  if (klv->data)
    return GST_FLOW_OK;
  GST_DEBUG_OBJECT (demux,
      "Pulling %" G_GSIZE_FORMAT " bytes from offset %" G_GUINT64_FORMAT,
      klv->length, klv->offset + klv->data_offset);
  return gst_mxf_demux_pull_range (demux, klv->offset + klv->data_offset,
      klv->length, &klv->data);
}

/* Call when done with a klv. Will release the buffer (if any) and will update
 * the demuxer offset position. Do *NOT* call if you do not want the demuxer
 * offset to be updated */
static void
gst_mxf_demux_consume_klv (GstMXFDemux * demux, GstMXFKLV * klv)
{
  if (klv->data) {
    gst_buffer_unref (klv->data);
    klv->data = NULL;
  }
  GST_DEBUG_OBJECT (demux,
      "Consuming KLV offset:%" G_GUINT64_FORMAT " data_offset:%"
      G_GUINT64_FORMAT " length:%" G_GSIZE_FORMAT " consumed:%"
      G_GUINT64_FORMAT, klv->offset, klv->data_offset, klv->length,
      klv->consumed);
  if (klv->consumed)
    demux->offset = klv->offset + klv->consumed;
  else
    demux->offset += klv->data_offset + klv->length;
}

static void
gst_mxf_demux_pull_random_index_pack (GstMXFDemux * demux)
{
  GstBuffer *buffer;
  gint64 filesize = -1;
  GstFormat fmt = GST_FORMAT_BYTES;
  guint32 pack_size;
  guint64 old_offset = demux->offset;
  GstMapInfo map;
  GstFlowReturn flow_ret;
  GstMXFKLV klv;

  if (!gst_pad_peer_query_duration (demux->sinkpad, fmt, &filesize) ||
      fmt != GST_FORMAT_BYTES || filesize == -1) {
    GST_DEBUG_OBJECT (demux, "Can't query upstream size");
    return;
  }

  g_assert (filesize > 4);

  buffer = NULL;
  if (gst_mxf_demux_pull_range (demux, filesize - 4, 4, &buffer) != GST_FLOW_OK) {
    GST_DEBUG_OBJECT (demux, "Failed pulling last 4 bytes");
    return;
  }

  gst_buffer_map (buffer, &map, GST_MAP_READ);
  pack_size = GST_READ_UINT32_BE (map.data);
  gst_buffer_unmap (buffer, &map);

  gst_buffer_unref (buffer);

  if (pack_size < 20) {
    GST_DEBUG_OBJECT (demux, "Too small pack size (%u bytes)", pack_size);
    return;
  } else if (pack_size > filesize - 20) {
    GST_DEBUG_OBJECT (demux, "Too large pack size (%u bytes)", pack_size);
    return;
  }

  /* Peek for klv at filesize - pack_size */
  if (gst_mxf_demux_peek_klv_packet (demux, filesize - pack_size,
          &klv) != GST_FLOW_OK) {
    GST_DEBUG_OBJECT (demux, "Failed pulling random index pack key");
    return;
  }

  if (!mxf_is_random_index_pack (&klv.key)) {
    GST_DEBUG_OBJECT (demux, "No random index pack");
    return;
  }

  demux->offset = filesize - pack_size;
  flow_ret = gst_mxf_demux_handle_random_index_pack (demux, &klv);
  if (klv.data)
    gst_buffer_unref (klv.data);
  demux->offset = old_offset;

  if (flow_ret == GST_FLOW_OK && !demux->index_table_segments_collected) {
    collect_index_table_segments (demux);
    demux->index_table_segments_collected = TRUE;
  }
}

static void
gst_mxf_demux_parse_footer_metadata (GstMXFDemux * demux)
{
  guint64 old_offset = demux->offset;
  GstMXFKLV klv;
  GstFlowReturn flow = GST_FLOW_OK;
  GstMXFDemuxPartition *old_partition = demux->current_partition;

  GST_DEBUG_OBJECT (demux, "Parsing footer metadata");

  demux->current_partition = NULL;

  gst_mxf_demux_reset_metadata (demux);

  if (demux->footer_partition_pack_offset != 0) {
    demux->offset = demux->run_in + demux->footer_partition_pack_offset;
  } else {
    MXFRandomIndexPackEntry *entry =
        &g_array_index (demux->random_index_pack, MXFRandomIndexPackEntry,
        demux->random_index_pack->len - 1);
    demux->offset = entry->offset;
  }

next_try:
  GST_LOG_OBJECT (demux, "Peeking partition pack at offset %" G_GUINT64_FORMAT,
      demux->offset);

  /* Process Partition Pack */
  flow = gst_mxf_demux_peek_klv_packet (demux, demux->offset, &klv);
  if (G_UNLIKELY (flow != GST_FLOW_OK))
    goto out;

  if (!mxf_is_partition_pack (&klv.key))
    goto out;

  if (gst_mxf_demux_handle_partition_pack (demux, &klv) != GST_FLOW_OK) {
    if (klv.data)
      gst_buffer_unref (klv.data);
    goto out;
  }

  gst_mxf_demux_consume_klv (demux, &klv);

  /* If there's no Header Metadata in this partition, jump to the previous
   * one */
  if (demux->current_partition->partition.header_byte_count == 0) {
    /* Reached the first partition, bail out */
    if (demux->current_partition->partition.this_partition == 0)
      goto out;

    demux->offset =
        demux->run_in + demux->current_partition->partition.prev_partition;
    goto next_try;
  }

  /* Next up should be an optional fill pack followed by a primer pack */
  while (TRUE) {
    flow = gst_mxf_demux_peek_klv_packet (demux, demux->offset, &klv);
    if (G_UNLIKELY (flow != GST_FLOW_OK)) {
      /* If ever we can't get the next KLV, jump to the previous partition */
      if (!demux->current_partition->partition.prev_partition)
        goto out;
      demux->offset =
          demux->run_in + demux->current_partition->partition.prev_partition;
      goto next_try;
    }

    if (mxf_is_fill (&klv.key)) {
      gst_mxf_demux_consume_klv (demux, &klv);
    } else if (mxf_is_primer_pack (&klv.key)) {
      /* Update primer mapping if present (jump to previous if it failed) */
      if (!demux->current_partition->primer.mappings) {
        if (gst_mxf_demux_handle_primer_pack (demux, &klv) != GST_FLOW_OK) {
          gst_mxf_demux_consume_klv (demux, &klv);
          if (!demux->current_partition->partition.prev_partition)
            goto out;
          demux->offset =
              demux->run_in +
              demux->current_partition->partition.prev_partition;
          goto next_try;
        }
      }
      gst_mxf_demux_consume_klv (demux, &klv);
      break;
    } else {
      if (!demux->current_partition->partition.prev_partition)
        goto out;
      demux->offset =
          demux->run_in + demux->current_partition->partition.prev_partition;
      goto next_try;
    }
  }

  /* parse metadata for this partition */
  while (demux->offset <
      demux->run_in + demux->current_partition->primer.offset +
      demux->current_partition->partition.header_byte_count) {
    flow = gst_mxf_demux_peek_klv_packet (demux, demux->offset, &klv);
    if (G_UNLIKELY (flow != GST_FLOW_OK)) {
      if (!demux->current_partition->partition.prev_partition)
        goto out;
      demux->offset =
          demux->run_in + demux->current_partition->partition.prev_partition;
      goto next_try;
    }

    if (mxf_is_metadata (&klv.key)) {
      flow = gst_mxf_demux_handle_metadata (demux, &klv);
      gst_mxf_demux_consume_klv (demux, &klv);

      if (G_UNLIKELY (flow != GST_FLOW_OK)) {
        gst_mxf_demux_reset_metadata (demux);
        if (!demux->current_partition->partition.prev_partition)
          goto out;
        demux->offset =
            demux->run_in + demux->current_partition->partition.prev_partition;
        goto next_try;
      }
    } else if (mxf_is_descriptive_metadata (&klv.key)) {
      gst_mxf_demux_handle_descriptive_metadata (demux, &klv);
      gst_mxf_demux_consume_klv (demux, &klv);
    } else {
      gst_mxf_demux_consume_klv (demux, &klv);
    }
  }

  /* resolve references etc */
  if (!demux->preface || gst_mxf_demux_resolve_references (demux) !=
      GST_FLOW_OK || gst_mxf_demux_update_tracks (demux) != GST_FLOW_OK) {
    /* Don't attempt to parse metadata from this partition again */
    demux->current_partition->parsed_metadata = TRUE;
    /* Skip to previous partition or bail out */
    if (!demux->current_partition->partition.prev_partition)
      goto out;
    demux->offset =
        demux->run_in + demux->current_partition->partition.prev_partition;
    goto next_try;
  }

out:
  demux->offset = old_offset;
  demux->current_partition = old_partition;
}

static GstFlowReturn
gst_mxf_demux_handle_klv_packet (GstMXFDemux * demux, GstMXFKLV * klv,
    gboolean peek)
{
  MXFUL *key = &klv->key;
#ifndef GST_DISABLE_GST_DEBUG
  gchar key_str[48];
#endif
  GstFlowReturn ret = GST_FLOW_OK;

  if (demux->update_metadata
      && demux->preface
      && (demux->offset >=
          demux->run_in + demux->current_partition->primer.offset +
          demux->current_partition->partition.header_byte_count ||
          mxf_is_generic_container_system_item (key) ||
          mxf_is_generic_container_essence_element (key) ||
          mxf_is_avid_essence_container_essence_element (key))) {
    demux->current_partition->parsed_metadata = TRUE;
    if ((ret = gst_mxf_demux_resolve_references (demux)) != GST_FLOW_OK ||
        (ret = gst_mxf_demux_update_tracks (demux)) != GST_FLOW_OK) {
      goto beach;
    }
  } else if (demux->metadata_resolved && demux->requested_package_string) {
    if ((ret = gst_mxf_demux_update_tracks (demux)) != GST_FLOW_OK) {
      goto beach;
    }
  }

  if (!mxf_is_mxf_packet (key)) {
    GST_WARNING_OBJECT (demux,
        "Skipping non-MXF packet of size %" G_GSIZE_FORMAT " at offset %"
        G_GUINT64_FORMAT ", key: %s", klv->length,
        demux->offset, mxf_ul_to_string (key, key_str));
  } else if (mxf_is_partition_pack (key)) {
    ret = gst_mxf_demux_handle_partition_pack (demux, klv);
  } else if (mxf_is_primer_pack (key)) {
    ret = gst_mxf_demux_handle_primer_pack (demux, klv);
  } else if (mxf_is_metadata (key)) {
    ret = gst_mxf_demux_handle_metadata (demux, klv);
  } else if (mxf_is_descriptive_metadata (key)) {
    ret = gst_mxf_demux_handle_descriptive_metadata (demux, klv);
  } else if (mxf_is_generic_container_system_item (key)) {
    if (demux->pending_index_table_segments)
      collect_index_table_segments (demux);
    ret = gst_mxf_demux_handle_generic_container_system_item (demux, klv);
  } else if (mxf_is_generic_container_essence_element (key) ||
      mxf_is_avid_essence_container_essence_element (key)) {
    if (demux->pending_index_table_segments)
      collect_index_table_segments (demux);
    ret =
        gst_mxf_demux_handle_generic_container_essence_element (demux, klv,
        peek);
  } else if (mxf_is_random_index_pack (key)) {
    ret = gst_mxf_demux_handle_random_index_pack (demux, klv);

    if (ret == GST_FLOW_OK && demux->random_access
        && !demux->index_table_segments_collected) {
      collect_index_table_segments (demux);
      demux->index_table_segments_collected = TRUE;
    }
  } else if (mxf_is_index_table_segment (key)) {
    ret = gst_mxf_demux_handle_index_table_segment (demux, klv);
  } else if (mxf_is_fill (key)) {
    GST_DEBUG_OBJECT (demux,
        "Skipping filler packet of size %" G_GSIZE_FORMAT " at offset %"
        G_GUINT64_FORMAT, klv->length, demux->offset);
  } else {
    GST_DEBUG_OBJECT (demux,
        "Skipping unknown packet of size %" G_GSIZE_FORMAT " at offset %"
        G_GUINT64_FORMAT ", key: %s", klv->length,
        demux->offset, mxf_ul_to_string (key, key_str));
  }

beach:
  return ret;
}

static void
gst_mxf_demux_set_partition_for_offset (GstMXFDemux * demux, guint64 offset)
{
  GList *l;

  GST_LOG_OBJECT (demux, "offset %" G_GUINT64_FORMAT, offset);

  /* This partition will already be parsed, otherwise
   * the position wouldn't be in the index */
  for (l = demux->partitions; l; l = l->next) {
    GstMXFDemuxPartition *p = l->data;

    if (p->partition.this_partition + demux->run_in <= offset)
      demux->current_partition = p;
  }
  if (demux->current_partition)
    GST_DEBUG_OBJECT (demux,
        "Current partition now %p (body_sid:%d index_sid:%d this_partition:%"
        G_GUINT64_FORMAT ")", demux->current_partition,
        demux->current_partition->partition.body_sid,
        demux->current_partition->partition.index_sid,
        demux->current_partition->partition.this_partition);
  else
    GST_DEBUG_OBJECT (demux, "Haven't found partition for offset yet");
}

static guint64
find_closest_offset (GArray * offsets, gint64 * position, gboolean keyframe)
{
  GstMXFDemuxIndex *idx;
  gint64 current_position = *position;

  if (!offsets || offsets->len == 0)
    return -1;

  current_position = MIN (current_position, offsets->len - 1);

  idx = &g_array_index (offsets, GstMXFDemuxIndex, current_position);
  while (idx->offset == 0 || (keyframe && !idx->keyframe)) {
    current_position--;
    if (current_position < 0)
      break;
    idx = &g_array_index (offsets, GstMXFDemuxIndex, current_position);
  }

  if (idx->offset != 0 && (!keyframe || idx->keyframe)) {
    *position = current_position;
    return idx->offset;
  }

  return -1;
}

static guint64
gst_mxf_demux_find_essence_element (GstMXFDemux * demux,
    GstMXFDemuxEssenceTrack * etrack, gint64 * position, gboolean keyframe)
{
  GstFlowReturn ret = GST_FLOW_OK;
  guint64 old_offset = demux->offset;
  GstMXFDemuxPartition *old_partition = demux->current_partition;
  gint i;
  guint64 offset;
  gint64 requested_position = *position, index_start_position;
  GstMXFDemuxIndex index_entry = { 0, };

  GST_DEBUG_OBJECT (demux, "Trying to find essence element %" G_GINT64_FORMAT
      " of track 0x%08x with body_sid %u (keyframe %d)", *position,
      etrack->track_number, etrack->body_sid, keyframe);

  /* Get entry from index table if present */
  if (find_edit_entry (demux, etrack, *position, keyframe, &index_entry)) {
    GST_DEBUG_OBJECT (demux,
        "Got position %" G_GINT64_FORMAT " at offset %" G_GUINT64_FORMAT,
        index_entry.dts, index_entry.offset);
    *position = index_entry.dts;
    return index_entry.offset;
  }

  GST_DEBUG_OBJECT (demux, "Not found in index table");

  /* Fallback to track offsets */

  if (!demux->random_access) {
    /* Best effort for push mode */
    offset = find_closest_offset (etrack->offsets, position, keyframe);
    if (offset != -1)
      GST_DEBUG_OBJECT (demux,
          "Starting with edit unit %" G_GINT64_FORMAT " for %" G_GINT64_FORMAT
          " in generated index at offset %" G_GUINT64_FORMAT, *position,
          requested_position, offset);
    return offset;
  }

  if (etrack->duration > 0 && *position >= etrack->duration) {
    GST_WARNING_OBJECT (demux, "Position after end of essence track");
    return -1;
  }

from_track_offset:

  index_start_position = *position;

  demux->offset = demux->run_in;

  offset = find_closest_offset (etrack->offsets, &index_start_position, FALSE);
  if (offset != -1) {
    demux->offset = offset + demux->run_in;
    GST_DEBUG_OBJECT (demux,
        "Starting with edit unit %" G_GINT64_FORMAT " for %" G_GINT64_FORMAT
        " in generated index at offset %" G_GUINT64_FORMAT,
        index_start_position, requested_position, offset);
  } else {
    index_start_position = -1;
  }

  gst_mxf_demux_set_partition_for_offset (demux, demux->offset);

  for (i = 0; i < demux->essence_tracks->len; i++) {
    GstMXFDemuxEssenceTrack *t = g_ptr_array_index (demux->essence_tracks, i);

    if (index_start_position != -1 && t == etrack)
      t->position = index_start_position;
    else
      t->position = (demux->offset == demux->run_in) ? 0 : -1;
    GST_LOG_OBJECT (demux, "Setting track %d position to %" G_GINT64_FORMAT,
        t->track_id, t->position);
  }

  /* Else peek at all essence elements and complete our
   * index until we find the requested element
   */
  while (ret == GST_FLOW_OK) {
    GstMXFKLV klv;

    GST_LOG_OBJECT (demux, "Pulling from offset %" G_GINT64_FORMAT,
        demux->offset);
    ret = gst_mxf_demux_peek_klv_packet (demux, demux->offset, &klv);

    if (ret == GST_FLOW_EOS) {
      /* Handle EOS */
      for (i = 0; i < demux->essence_tracks->len; i++) {
        GstMXFDemuxEssenceTrack *t =
            g_ptr_array_index (demux->essence_tracks, i);

        if (t->position > 0)
          t->duration = t->position;
      }
      /* For the searched track this is really our position */
      etrack->duration = etrack->position;

      for (i = 0; i < demux->src->len; i++) {
        GstMXFDemuxPad *p = g_ptr_array_index (demux->src, i);

        if (!p->eos
            && p->current_essence_track_position >=
            p->current_essence_track->duration) {
          GstEvent *e;

          p->eos = TRUE;
          e = gst_event_new_eos ();
          gst_event_set_seqnum (e, demux->seqnum);
          gst_pad_push_event (GST_PAD_CAST (p), e);
        }
      }
    }

    GST_LOG_OBJECT (demux,
        "pulling gave flow:%s track->position:%" G_GINT64_FORMAT,
        gst_flow_get_name (ret), etrack->position);
    if (G_UNLIKELY (ret != GST_FLOW_OK) && etrack->position <= *position) {
      demux->offset = old_offset;
      demux->current_partition = old_partition;
      break;
    } else if (G_UNLIKELY (ret == GST_FLOW_OK)) {
      ret = gst_mxf_demux_handle_klv_packet (demux, &klv, TRUE);
      gst_mxf_demux_consume_klv (demux, &klv);
    }

    GST_LOG_OBJECT (demux,
        "Handling gave flow:%s track->position:%" G_GINT64_FORMAT
        " looking for %" G_GINT64_FORMAT, gst_flow_get_name (ret),
        etrack->position, *position);

    /* If we found the position read it from the index again */
    if (((ret == GST_FLOW_OK && etrack->position == *position + 1) ||
            (ret == GST_FLOW_EOS && etrack->position == *position + 1))
        && etrack->offsets && etrack->offsets->len > *position
        && g_array_index (etrack->offsets, GstMXFDemuxIndex,
            *position).offset != 0) {
      GST_DEBUG_OBJECT (demux, "Found at offset %" G_GUINT64_FORMAT,
          demux->offset);
      demux->offset = old_offset;
      demux->current_partition = old_partition;
      if (find_edit_entry (demux, etrack, *position, keyframe, &index_entry)) {
        GST_DEBUG_OBJECT (demux,
            "Got position %" G_GINT64_FORMAT " at offset %" G_GUINT64_FORMAT,
            index_entry.dts, index_entry.offset);
        *position = index_entry.dts;
        return index_entry.offset;
      }
      goto from_track_offset;
    }
  }
  demux->offset = old_offset;
  demux->current_partition = old_partition;

  GST_DEBUG_OBJECT (demux, "Not found in this file");

  return -1;
}

static GstFlowReturn
gst_mxf_demux_pull_and_handle_klv_packet (GstMXFDemux * demux)
{
  GstMXFKLV klv;
  GstFlowReturn ret = GST_FLOW_OK;
  gboolean force_switch = FALSE;

  if (demux->src->len > 0) {
    if (!gst_mxf_demux_get_earliest_pad (demux)) {
      ret = GST_FLOW_EOS;
      GST_DEBUG_OBJECT (demux, "All tracks are EOS");
      goto beach;
    }
  }

  if (demux->state == GST_MXF_DEMUX_STATE_ESSENCE) {
    g_assert (demux->current_partition->single_track
        && demux->current_partition->single_track->wrapping !=
        MXF_ESSENCE_WRAPPING_FRAME_WRAPPING);
    /* Feeding essence directly (i.e. in the middle of a custom/clip KLV) */
    ret =
        gst_mxf_demux_handle_generic_container_essence_element (demux,
        &demux->current_partition->clip_klv, FALSE);
    gst_mxf_demux_consume_klv (demux, &demux->current_partition->clip_klv);
    if (ret == GST_FLOW_OK
        && demux->current_partition->single_track->position >=
        demux->current_partition->single_track->duration) {
      /* We are done with the contents of this clip/custom wrapping, force the
       * switch to the next non-EOS track */
      GST_DEBUG_OBJECT (demux, "Single track EOS, switch");
      force_switch = TRUE;
    }

  } else {

    ret = gst_mxf_demux_peek_klv_packet (demux, demux->offset, &klv);

    /* FIXME
     *
     * Move this EOS handling to a separate function
     */
    if (ret == GST_FLOW_EOS && demux->src->len > 0) {
      guint i;
      GstMXFDemuxPad *p = NULL;

      GST_DEBUG_OBJECT (demux, "EOS HANDLING");

      for (i = 0; i < demux->src->len; i++) {
        GstMXFDemuxPad *p = g_ptr_array_index (demux->src, i);

        GST_DEBUG_OBJECT (p,
            "eos:%d current_essence_track_position:%" G_GINT64_FORMAT
            " position:%" G_GINT64_FORMAT " duration:%" G_GINT64_FORMAT, p->eos,
            p->current_essence_track_position,
            p->current_essence_track->position,
            p->current_essence_track->duration);
        if (!p->eos
            && p->current_essence_track->position >=
            p->current_essence_track->duration) {
          GstEvent *e;

          p->eos = TRUE;
          e = gst_event_new_eos ();
          gst_event_set_seqnum (e, demux->seqnum);
          gst_pad_push_event (GST_PAD_CAST (p), e);
        }
      }

      while ((p = gst_mxf_demux_get_earliest_pad (demux))) {
        guint64 offset;
        gint64 position;

        GST_DEBUG_OBJECT (p, "Trying on earliest");

        position = p->current_essence_track_position;

        offset =
            gst_mxf_demux_find_essence_element (demux, p->current_essence_track,
            &position, FALSE);
        if (offset == -1) {
          GstEvent *e;

          GST_ERROR_OBJECT (demux, "Failed to find offset for essence track");
          p->eos = TRUE;
          e = gst_event_new_eos ();
          gst_event_set_seqnum (e, demux->seqnum);
          gst_pad_push_event (GST_PAD_CAST (p), e);
          continue;
        }

        demux->offset = offset + demux->run_in;
        gst_mxf_demux_set_partition_for_offset (demux, demux->offset);
        if (p->current_essence_track->wrapping !=
            MXF_ESSENCE_WRAPPING_FRAME_WRAPPING) {
          demux->state = GST_MXF_DEMUX_STATE_ESSENCE;
          demux->current_partition->clip_klv.consumed =
              offset - demux->current_partition->clip_klv.offset;
        } else
          demux->state = GST_MXF_DEMUX_STATE_KLV;
        p->current_essence_track->position = position;

        ret = GST_FLOW_OK;
        goto beach;
      }
    }
    if (G_UNLIKELY (ret != GST_FLOW_OK))
      goto beach;

    ret = gst_mxf_demux_handle_klv_packet (demux, &klv, FALSE);
    gst_mxf_demux_consume_klv (demux, &klv);

    /* We entered a new partition */
    if (ret == GST_FLOW_OK && mxf_is_partition_pack (&klv.key)) {
      GstMXFDemuxPartition *partition = demux->current_partition;
      gboolean partition_done = FALSE;

      /* Grab footer metadata if needed */
      if (demux->pull_footer_metadata
          && partition->partition.type == MXF_PARTITION_PACK_HEADER
          && (!partition->partition.closed || !partition->partition.complete)
          && (demux->footer_partition_pack_offset != 0
              || demux->random_index_pack)) {
        GST_DEBUG_OBJECT (demux,
            "Open or incomplete header partition, trying to get final metadata from the last partitions");
        gst_mxf_demux_parse_footer_metadata (demux);
        demux->pull_footer_metadata = FALSE;
      }

      /* If the partition has some content, do post-checks */
      if (partition->partition.body_sid != 0) {
        guint64 lowest_offset = G_MAXUINT64;
        GST_DEBUG_OBJECT (demux,
            "Entered partition (body_sid:%d index_sid:%d body_offset:%"
            G_GUINT64_FORMAT "), checking positions",
            partition->partition.body_sid, partition->partition.index_sid,
            partition->partition.body_offset);

        if (partition->single_track) {
          /* Fast-path for single track partition */
          if (partition->single_track->position == -1
              && partition->partition.body_offset == 0) {
            GST_DEBUG_OBJECT (demux,
                "First time in partition, setting track position to 0");
            partition->single_track->position = 0;
          } else if (partition->single_track->position == -1) {
            GST_ERROR_OBJECT (demux,
                "Unknown track position, consuming data from first partition entry");
            lowest_offset =
                partition->partition.this_partition +
                partition->essence_container_offset;
            partition->clip_klv.consumed = 0;
          } else if (partition->single_track->position != 0) {
            GstMXFDemuxIndex entry;
            GST_DEBUG_OBJECT (demux,
                "Track already at another position : %" G_GINT64_FORMAT,
                partition->single_track->position);
            if (find_edit_entry (demux, partition->single_track,
                    partition->single_track->position, FALSE, &entry)) {
              lowest_offset = entry.offset;
            } else if (partition->single_track->position >=
                partition->single_track->duration) {
              GST_DEBUG_OBJECT (demux, "Track fully consumed, partition done");
              partition_done = TRUE;
            }
          }
        } else {
          guint i;
          for (i = 0; i < demux->essence_tracks->len; i++) {
            GstMXFDemuxEssenceTrack *etrack =
                g_ptr_array_index (demux->essence_tracks, i);

            if (etrack->body_sid != partition->partition.body_sid)
              continue;
            if (etrack->position == -1 && partition->partition.body_offset == 0) {
              GST_DEBUG_OBJECT (demux, "Resetting track %d to position 0",
                  etrack->track_id);

              etrack->position = 0;
            } else if (etrack->position != 0) {
              GstMXFDemuxIndex entry;
              if (find_edit_entry (demux, etrack,
                      etrack->position, FALSE, &entry)) {
                if (lowest_offset == G_MAXUINT64
                    || entry.offset < lowest_offset)
                  lowest_offset = entry.offset;
              }
            }
          }
        }

        if (partition_done || lowest_offset != G_MAXUINT64) {
          GstMXFDemuxPartition *next_partition = NULL;
          GList *cur_part = g_list_find (demux->partitions, partition);
          if (cur_part && cur_part->next)
            next_partition = (GstMXFDemuxPartition *) cur_part->next->data;

          /* If we have completely processed this partition, skip to next partition */
          if (partition_done
              || lowest_offset > next_partition->partition.this_partition) {
            GST_DEBUG_OBJECT (demux,
                "Partition entirely processed, skipping to next one");
            demux->offset = next_partition->partition.this_partition;
          } else {
            GST_DEBUG_OBJECT (demux,
                "Skipping to demuxer offset %" G_GUINT64_FORMAT " (from %"
                G_GUINT64_FORMAT ")", lowest_offset, demux->offset);
            demux->offset = lowest_offset;
            if (partition->single_track
                && partition->single_track->wrapping !=
                MXF_ESSENCE_WRAPPING_FRAME_WRAPPING) {
              demux->state = GST_MXF_DEMUX_STATE_ESSENCE;
              demux->current_partition->clip_klv.consumed =
                  demux->offset - demux->current_partition->clip_klv.offset;
            }
          }
        }
      }
    }
  }

  if (ret == GST_FLOW_OK && demux->src->len > 0
      && demux->essence_tracks->len > 0) {
    GstMXFDemuxPad *earliest = NULL;
    /* We allow time drifts of at most 500ms */
    while ((earliest = gst_mxf_demux_get_earliest_pad (demux)) && (force_switch
            || demux->segment.position - earliest->position >
            demux->max_drift)) {
      guint64 offset;
      gint64 position;

      GST_DEBUG_OBJECT (demux,
          "Found synchronization issue -- trying to solve");

      position = earliest->current_essence_track_position;

      /* FIXME: This can probably be improved by using the
       * offset of position-1 if it's in the same partition
       * or the start of the position otherwise.
       * This way we won't skip elements from the same essence
       * container as etrack->position
       */
      offset =
          gst_mxf_demux_find_essence_element (demux,
          earliest->current_essence_track, &position, FALSE);
      if (offset == -1) {
        GstEvent *e;

        GST_WARNING_OBJECT (demux,
            "Failed to find offset for late essence track");
        earliest->eos = TRUE;
        e = gst_event_new_eos ();
        gst_event_set_seqnum (e, demux->seqnum);
        gst_pad_push_event (GST_PAD_CAST (earliest), e);
        continue;
      }

      demux->offset = offset + demux->run_in;
      gst_mxf_demux_set_partition_for_offset (demux, demux->offset);
      GST_DEBUG_OBJECT (demux,
          "Switching to offset %" G_GUINT64_FORMAT " for position %"
          G_GINT64_FORMAT " on track %d (body_sid:%d index_sid:%d)",
          demux->offset, position, earliest->current_essence_track->track_id,
          earliest->current_essence_track->body_sid,
          earliest->current_essence_track->index_sid);
      if (demux->current_partition->single_track
          && demux->current_partition->single_track->wrapping !=
          MXF_ESSENCE_WRAPPING_FRAME_WRAPPING) {
        demux->state = GST_MXF_DEMUX_STATE_ESSENCE;
        demux->current_partition->clip_klv.consumed =
            offset - demux->current_partition->clip_klv.offset;
      } else
        demux->state = GST_MXF_DEMUX_STATE_KLV;

      earliest->current_essence_track->position = position;
      GST_DEBUG_OBJECT (earliest, "Switching to this pad");
      break;
    }
  }

beach:
  return ret;
}

static void
gst_mxf_demux_loop (GstPad * pad)
{
  GstMXFDemux *demux = NULL;
  GstFlowReturn flow = GST_FLOW_OK;

  demux = GST_MXF_DEMUX (gst_pad_get_parent (pad));

  if (demux->state == GST_MXF_DEMUX_STATE_UNKNOWN) {
    GstMXFKLV klv;

    /* Skip run-in, which is at most 64K and is finished
     * by a header partition pack */
    while (demux->offset < 64 * 1024) {
      if ((flow =
              gst_mxf_demux_peek_klv_packet (demux, demux->offset,
                  &klv)) != GST_FLOW_OK)
        goto pause;

      if (mxf_is_header_partition_pack (&klv.key)) {
        GST_DEBUG_OBJECT (demux,
            "Found header partition pack at offset %" G_GUINT64_FORMAT,
            demux->offset);
        demux->state = GST_MXF_DEMUX_STATE_KLV;
        demux->run_in = demux->offset;
        break;
      }
      demux->offset++;
    }

    if (G_UNLIKELY (demux->run_in == -1)) {
      GST_ERROR_OBJECT (demux, "No valid header partition pack found");
      flow = GST_FLOW_ERROR;
      goto pause;
    }

    /* Grab the RIP at the end of the file (if present) */
    gst_mxf_demux_pull_random_index_pack (demux);
  }

  /* Now actually do something */
  flow = gst_mxf_demux_pull_and_handle_klv_packet (demux);

  /* pause if something went wrong */
  if (G_UNLIKELY (flow != GST_FLOW_OK))
    goto pause;

  /* check EOS condition */
  if ((demux->segment.stop != -1) &&
      (demux->segment.position >= demux->segment.stop)) {
    guint i;
    gboolean eos = TRUE;

    for (i = 0; i < demux->src->len; i++) {
      GstMXFDemuxPad *p = g_ptr_array_index (demux->src, i);

      if (!p->eos && p->position < demux->segment.stop) {
        eos = FALSE;
        break;
      }
    }

    if (eos) {
      flow = GST_FLOW_EOS;
      goto pause;
    }
  }

  gst_object_unref (demux);

  return;

pause:
  {
    const gchar *reason = gst_flow_get_name (flow);

    GST_LOG_OBJECT (demux, "pausing task, reason %s", reason);
    gst_pad_pause_task (pad);

    if (flow == GST_FLOW_EOS) {
      /* perform EOS logic */
      if (demux->src->len == 0) {
        GST_ELEMENT_ERROR (demux, STREAM, WRONG_TYPE,
            ("This stream contains no data."),
            ("got eos and didn't find any streams"));
      } else if (demux->segment.flags & GST_SEEK_FLAG_SEGMENT) {
        gint64 stop;
        GstMessage *m;
        GstEvent *e;

        /* for segment playback we need to post when (in stream time)
         * we stopped, this is either stop (when set) or the duration. */
        if ((stop = demux->segment.stop) == -1)
          stop = demux->segment.duration;

        GST_LOG_OBJECT (demux, "Sending segment done, at end of segment");
        m = gst_message_new_segment_done (GST_OBJECT_CAST (demux),
            GST_FORMAT_TIME, stop);
        gst_message_set_seqnum (m, demux->seqnum);
        gst_element_post_message (GST_ELEMENT_CAST (demux), m);
        e = gst_event_new_segment_done (GST_FORMAT_TIME, stop);
        gst_event_set_seqnum (e, demux->seqnum);
        gst_mxf_demux_push_src_event (demux, e);
      } else {
        GstEvent *e;

        /* normal playback, send EOS to all linked pads */
        GST_LOG_OBJECT (demux, "Sending EOS, at end of stream");
        e = gst_event_new_eos ();
        gst_event_set_seqnum (e, demux->seqnum);
        if (!gst_mxf_demux_push_src_event (demux, e)) {
          GST_WARNING_OBJECT (demux, "failed pushing EOS on streams");
        }
      }
    } else if (flow == GST_FLOW_NOT_LINKED || flow < GST_FLOW_EOS) {
      GstEvent *e;

      GST_ELEMENT_FLOW_ERROR (demux, flow);
      e = gst_event_new_eos ();
      gst_event_set_seqnum (e, demux->seqnum);
      gst_mxf_demux_push_src_event (demux, e);
    }
    gst_object_unref (demux);
    return;
  }
}

static GstFlowReturn
gst_mxf_demux_chain (GstPad * pad, GstObject * parent, GstBuffer * inbuf)
{
  GstFlowReturn ret = GST_FLOW_OK;
  GstMXFDemux *demux = NULL;
  const guint8 *data = NULL;
  gboolean res;
  GstMXFKLV klv;
#ifndef GST_DISABLE_GST_DEBUG
  gchar str[48];
#endif

  demux = GST_MXF_DEMUX (parent);

  GST_LOG_OBJECT (demux,
      "received buffer of %" G_GSIZE_FORMAT " bytes at offset %"
      G_GUINT64_FORMAT, gst_buffer_get_size (inbuf), GST_BUFFER_OFFSET (inbuf));

  if (demux->src->len > 0) {
    if (!gst_mxf_demux_get_earliest_pad (demux)) {
      ret = GST_FLOW_EOS;
      GST_DEBUG_OBJECT (demux, "All tracks are EOS");
      return ret;
    }
  }

  if (G_UNLIKELY (GST_BUFFER_OFFSET (inbuf) == 0)) {
    GST_DEBUG_OBJECT (demux, "beginning of file, expect header");
    demux->run_in = -1;
    demux->offset = 0;
    demux->state = GST_MXF_DEMUX_STATE_UNKNOWN;
  }

  if (G_UNLIKELY (demux->offset == 0 && GST_BUFFER_OFFSET (inbuf) != 0)) {
    GST_DEBUG_OBJECT (demux, "offset was zero, synchronizing with buffer's");
    if (GST_BUFFER_OFFSET_IS_VALID (inbuf))
      demux->offset = GST_BUFFER_OFFSET (inbuf);
    gst_mxf_demux_set_partition_for_offset (demux, demux->offset);
  } else if (demux->current_partition == NULL) {
    gst_mxf_demux_set_partition_for_offset (demux, demux->offset);
  }

  gst_adapter_push (demux->adapter, inbuf);
  inbuf = NULL;

  while (ret == GST_FLOW_OK) {
    if (G_UNLIKELY (demux->flushing)) {
      GST_DEBUG_OBJECT (demux, "we are now flushing, exiting parser loop");
      ret = GST_FLOW_FLUSHING;
      break;
    }

    if (gst_adapter_available (demux->adapter) < 16)
      break;

    if (demux->state == GST_MXF_DEMUX_STATE_UNKNOWN) {
      /* Skip run-in, which is at most 64K and is finished
       * by a header partition pack */

      while (demux->offset < 64 * 1024
          && gst_adapter_available (demux->adapter) >= 16) {
        data = gst_adapter_map (demux->adapter, 16);
        res = mxf_is_header_partition_pack ((const MXFUL *) data);
        gst_adapter_unmap (demux->adapter);

        if (res) {
          GST_DEBUG_OBJECT (demux,
              "Found header partition pack at offset %" G_GUINT64_FORMAT,
              demux->offset);
          demux->run_in = demux->offset;
          demux->state = GST_MXF_DEMUX_STATE_KLV;
          break;
        }
        gst_adapter_flush (demux->adapter, 1);
        demux->offset++;
      }
    } else if (demux->offset < demux->run_in) {
      guint64 flush = MIN (gst_adapter_available (demux->adapter),
          demux->run_in - demux->offset);
      gst_adapter_flush (demux->adapter, flush);
      demux->offset += flush;
      continue;
    }

    if (demux->state == GST_MXF_DEMUX_STATE_UNKNOWN) {
      /* Need more data */
      if (demux->offset < 64 * 1024)
        break;

      GST_ERROR_OBJECT (demux, "No valid header partition pack found");
      ret = GST_FLOW_ERROR;
      break;
    }

    if (gst_adapter_available (demux->adapter) < 17)
      break;

    /* FIXME : Handle non-klv state */
    g_assert (demux->state == GST_MXF_DEMUX_STATE_KLV);

    /* Now actually do something */
    memset (&klv, 0, sizeof (GstMXFKLV));

    /* Pull 16 byte key and first byte of BER encoded length */
    data = gst_adapter_map (demux->adapter, 17);

    memcpy (&klv.key, data, 16);

    GST_DEBUG_OBJECT (demux, "Got KLV packet with key %s",
        mxf_ul_to_string (&klv.key, str));

    /* Decode BER encoded packet length */
    if ((data[16] & 0x80) == 0) {
      klv.length = data[16];
      klv.data_offset = 17;
    } else {
      guint slen = data[16] & 0x7f;

      klv.data_offset = 16 + 1 + slen;

      gst_adapter_unmap (demux->adapter);

      /* Must be at most 8 according to SMPTE-379M 5.3.4 and
       * GStreamer buffers can only have a 4 bytes length */
      if (slen > 8) {
        GST_ERROR_OBJECT (demux, "Invalid KLV packet length: %u", slen);
        ret = GST_FLOW_ERROR;
        break;
      }

      if (gst_adapter_available (demux->adapter) < 17 + slen)
        break;

      data = gst_adapter_map (demux->adapter, 17 + slen);
      data += 17;

      klv.length = 0;
      while (slen) {
        klv.length = (klv.length << 8) | *data;
        data++;
        slen--;
      }
    }

    gst_adapter_unmap (demux->adapter);

    /* GStreamer's buffer sizes are stored in a guint so we
     * limit ourself to G_MAXUINT large buffers */
    if (klv.length > G_MAXUINT) {
      GST_ERROR_OBJECT (demux,
          "Unsupported KLV packet length: %" G_GSIZE_FORMAT, klv.length);
      ret = GST_FLOW_ERROR;
      break;
    }

    GST_DEBUG_OBJECT (demux, "KLV packet with key %s has length "
        "%" G_GSIZE_FORMAT, mxf_ul_to_string (&klv.key, str), klv.length);

    if (gst_adapter_available (demux->adapter) < klv.data_offset + klv.length)
      break;

    gst_adapter_flush (demux->adapter, klv.data_offset);

    if (klv.length > 0) {
      klv.data = gst_adapter_take_buffer (demux->adapter, klv.length);

      ret = gst_mxf_demux_handle_klv_packet (demux, &klv, FALSE);
    }
    gst_mxf_demux_consume_klv (demux, &klv);
  }

  return ret;
}

/* Given a stream time for an output pad, figure out:
 * * The Essence track for that stream time
 * * The position on that track
 */
static gboolean
gst_mxf_demux_pad_to_track_and_position (GstMXFDemux * demux,
    GstMXFDemuxPad * pad, GstClockTime streamtime,
    GstMXFDemuxEssenceTrack ** etrack, gint64 * position)
{
  gint64 material_position;
  guint64 sum = 0;
  guint i;
  MXFMetadataSourceClip *clip = NULL;
  gchar str[96];

  /* Convert to material position */
  material_position =
      gst_util_uint64_scale (streamtime, pad->material_track->edit_rate.n,
      pad->material_track->edit_rate.d * GST_SECOND);

  GST_DEBUG_OBJECT (pad,
      "streamtime %" GST_TIME_FORMAT " position %" G_GINT64_FORMAT,
      GST_TIME_ARGS (streamtime), material_position);


  /* Find sequence component covering that position */
  for (i = 0; i < pad->material_track->parent.sequence->n_structural_components;
      i++) {
    clip =
        MXF_METADATA_SOURCE_CLIP (pad->material_track->parent.sequence->
        structural_components[i]);
    GST_LOG_OBJECT (pad,
        "clip %d start_position:%" G_GINT64_FORMAT " duration %"
        G_GINT64_FORMAT, clip->source_track_id, clip->start_position,
        clip->parent.duration);
    if (clip->parent.duration <= 0)
      break;
    if ((sum + clip->parent.duration) > material_position)
      break;
    sum += clip->parent.duration;
  }

  if (i == pad->material_track->parent.sequence->n_structural_components) {
    GST_WARNING_OBJECT (pad, "Requested position beyond the last clip");
    /* Outside of current components. Setting to the end of the last clip */
    material_position = sum;
    sum -= clip->parent.duration;
  }

  GST_DEBUG_OBJECT (pad, "Looking for essence track for track_id:%d umid:%s",
      clip->source_track_id, mxf_umid_to_string (&clip->source_package_id,
          str));

  /* Get the corresponding essence track for the given source package and stream id */
  for (i = 0; i < demux->essence_tracks->len; i++) {
    GstMXFDemuxEssenceTrack *track =
        g_ptr_array_index (demux->essence_tracks, i);
    GST_LOG_OBJECT (pad, "Looking at essence track body_sid:%d index_sid:%d",
        track->body_sid, track->index_sid);
    if (clip->source_track_id == 0 || (track->track_id == clip->source_track_id
            && mxf_umid_is_equal (&clip->source_package_id,
                &track->source_package_uid))) {
      GST_DEBUG_OBJECT (pad,
          "Found matching essence track body_sid:%d index_sid:%d",
          track->body_sid, track->index_sid);
      *etrack = track;
      *position = material_position - sum;
      return TRUE;
    }
  }

  return FALSE;
}

/* Given a track+position for a given pad, figure out the resulting stream time */
static gboolean
gst_mxf_demux_pad_get_stream_time (GstMXFDemux * demux,
    GstMXFDemuxPad * pad, GstMXFDemuxEssenceTrack * etrack,
    gint64 position, GstClockTime * stream_time)
{
  guint i;
  guint64 sum = 0;
  MXFMetadataSourceClip *clip = NULL;

  /* Find the component for that */
  /* Find sequence component covering that position */
  for (i = 0; i < pad->material_track->parent.sequence->n_structural_components;
      i++) {
    clip =
        MXF_METADATA_SOURCE_CLIP (pad->material_track->parent.sequence->
        structural_components[i]);
    GST_LOG_OBJECT (pad,
        "clip %d start_position:%" G_GINT64_FORMAT " duration %"
        G_GINT64_FORMAT, clip->source_track_id, clip->start_position,
        clip->parent.duration);
    if (etrack->track_id == clip->source_track_id
        && mxf_umid_is_equal (&clip->source_package_id,
            &etrack->source_package_uid)) {
      /* This is the clip */
      break;
    }
    /* Fetch in the next one */
    sum += clip->parent.duration;
  }

  /* Theoretically impossible */
  if (i == pad->material_track->parent.sequence->n_structural_components) {
    /* Outside of current components ?? */
    return FALSE;
  }

  *stream_time =
      gst_util_uint64_scale (position + sum,
      pad->material_track->edit_rate.d * GST_SECOND,
      pad->material_track->edit_rate.n);

  return TRUE;
}

static void
gst_mxf_demux_pad_set_position (GstMXFDemux * demux, GstMXFDemuxPad * p,
    GstClockTime start)
{
  guint i;
  guint64 sum = 0;
  MXFMetadataSourceClip *clip = NULL;

  if (!p->current_component) {
    p->current_essence_track_position =
        gst_util_uint64_scale (start, p->material_track->edit_rate.n,
        p->material_track->edit_rate.d * GST_SECOND);

    if (p->current_essence_track_position >= p->current_essence_track->duration
        && p->current_essence_track->duration > 0) {
      p->current_essence_track_position = p->current_essence_track->duration;
      p->position =
          gst_util_uint64_scale (p->current_essence_track->duration,
          p->material_track->edit_rate.d * GST_SECOND,
          p->material_track->edit_rate.n);
    } else {
      p->position = start;
    }
    p->position_accumulated_error = 0.0;
    p->current_material_track_position = p->current_essence_track_position;

    return;
  }

  for (i = 0; i < p->material_track->parent.sequence->n_structural_components;
      i++) {
    clip =
        MXF_METADATA_SOURCE_CLIP (p->material_track->parent.sequence->
        structural_components[i]);

    if (clip->parent.duration <= 0)
      break;

    sum += clip->parent.duration;

    if (gst_util_uint64_scale (sum, p->material_track->edit_rate.d * GST_SECOND,
            p->material_track->edit_rate.n) > start)
      break;
  }

  if (i == p->material_track->parent.sequence->n_structural_components) {
    p->position =
        gst_util_uint64_scale (sum, p->material_track->edit_rate.d * GST_SECOND,
        p->material_track->edit_rate.n);
    p->position_accumulated_error = 0.0;
    p->current_material_track_position = sum;

    gst_mxf_demux_pad_set_component (demux, p, i);
    return;
  }

  if (clip->parent.duration > 0)
    sum -= clip->parent.duration;

  start -=
      gst_util_uint64_scale (sum, p->material_track->edit_rate.d * GST_SECOND,
      p->material_track->edit_rate.n);

  gst_mxf_demux_pad_set_component (demux, p, i);

  {
    gint64 essence_offset = gst_util_uint64_scale (start,
        p->current_essence_track->source_track->edit_rate.n,
        p->current_essence_track->source_track->edit_rate.d * GST_SECOND);

    p->current_essence_track_position += essence_offset;

    p->position = gst_util_uint64_scale (sum,
        GST_SECOND * p->material_track->edit_rate.d,
        p->material_track->edit_rate.n) + gst_util_uint64_scale (essence_offset,
        GST_SECOND * p->current_essence_track->source_track->edit_rate.d,
        p->current_essence_track->source_track->edit_rate.n);
    p->position_accumulated_error = 0.0;
    p->current_material_track_position = sum + essence_offset;
  }

  if (p->current_essence_track_position >= p->current_essence_track->duration
      && p->current_essence_track->duration > 0) {
    p->current_essence_track_position = p->current_essence_track->duration;
    p->position =
        gst_util_uint64_scale (sum + p->current_component->parent.duration,
        p->material_track->edit_rate.d * GST_SECOND,
        p->material_track->edit_rate.n);
    p->position_accumulated_error = 0.0;
    p->current_material_track_position =
        sum + p->current_component->parent.duration;
  }
}

static gboolean
gst_mxf_demux_seek_push (GstMXFDemux * demux, GstEvent * event)
{
  GstFormat format;
  GstSeekFlags flags;
  GstSeekType start_type, stop_type;
  gint64 start, stop;
  gdouble rate;
  gboolean update, flush, keyframe;
  GstSegment seeksegment;
  guint i;
  guint32 seqnum;

  gst_event_parse_seek (event, &rate, &format, &flags,
      &start_type, &start, &stop_type, &stop);
  seqnum = gst_event_get_seqnum (event);

  if (rate <= 0.0)
    goto wrong_rate;

  if (format != GST_FORMAT_TIME)
    goto wrong_format;

  flush = !!(flags & GST_SEEK_FLAG_FLUSH);
  keyframe = !!(flags & GST_SEEK_FLAG_KEY_UNIT);

  /* Work on a copy until we are sure the seek succeeded. */
  memcpy (&seeksegment, &demux->segment, sizeof (GstSegment));

  GST_DEBUG_OBJECT (demux, "segment before configure %" GST_SEGMENT_FORMAT,
      &demux->segment);

  /* Apply the seek to our segment */
  gst_segment_do_seek (&seeksegment, rate, format, flags,
      start_type, start, stop_type, stop, &update);

  GST_DEBUG_OBJECT (demux, "segment configured %" GST_SEGMENT_FORMAT,
      &seeksegment);

  if (flush || seeksegment.position != demux->segment.position) {
    gboolean ret;
    guint64 new_offset = -1;
    GstEvent *e;

    if (!demux->metadata_resolved || demux->update_metadata) {
      if (gst_mxf_demux_resolve_references (demux) != GST_FLOW_OK ||
          gst_mxf_demux_update_tracks (demux) != GST_FLOW_OK) {
        goto unresolved_metadata;
      }
    }

    /* Do the actual seeking */
    for (i = 0; i < demux->src->len; i++) {
      GstMXFDemuxPad *p = g_ptr_array_index (demux->src, i);
      gint64 position;
      guint64 off;

      /* Reset EOS flag on all pads */
      p->eos = FALSE;
      gst_mxf_demux_pad_set_position (demux, p, start);

      position = p->current_essence_track_position;
      off = gst_mxf_demux_find_essence_element (demux, p->current_essence_track,
          &position, keyframe);
      new_offset = MIN (off, new_offset);
      p->discont = TRUE;
    }

    if (new_offset == -1)
      goto no_new_offset;

    new_offset += demux->run_in;

    GST_DEBUG_OBJECT (demux, "generating an upstream seek at position %"
        G_GUINT64_FORMAT, new_offset);
    e = gst_event_new_seek (seeksegment.rate, GST_FORMAT_BYTES,
        seeksegment.flags | GST_SEEK_FLAG_ACCURATE, GST_SEEK_TYPE_SET,
        new_offset, GST_SEEK_TYPE_NONE, 0);
    gst_event_set_seqnum (e, seqnum);
    ret = gst_pad_push_event (demux->sinkpad, e);

    if (G_UNLIKELY (!ret)) {
      goto seek_failed;
    }
  }

  /* Tell all the stream a new segment is needed */
  for (i = 0; i < demux->src->len; i++) {
    GstMXFDemuxPad *p = g_ptr_array_index (demux->src, i);
    p->need_segment = TRUE;
  }

  for (i = 0; i < demux->essence_tracks->len; i++) {
    GstMXFDemuxEssenceTrack *t = g_ptr_array_index (demux->essence_tracks, i);
    t->position = -1;
  }

  /* Ok seek succeeded, take the newly configured segment */
  memcpy (&demux->segment, &seeksegment, sizeof (GstSegment));

  return TRUE;

/* ERRORS */
wrong_format:
  {
    GST_WARNING_OBJECT (demux, "seeking only supported in TIME format");
    return gst_pad_push_event (demux->sinkpad, gst_event_ref (event));
  }
wrong_rate:
  {
    GST_WARNING_OBJECT (demux, "only rates > 0.0 are allowed");
    return FALSE;
  }
unresolved_metadata:
  {
    GST_WARNING_OBJECT (demux, "metadata can't be resolved");
    return gst_pad_push_event (demux->sinkpad, gst_event_ref (event));
  }
seek_failed:
  {
    GST_WARNING_OBJECT (demux, "upstream seek failed");
    return gst_pad_push_event (demux->sinkpad, gst_event_ref (event));
  }
no_new_offset:
  {
    GST_WARNING_OBJECT (demux, "can't find new offset");
    return gst_pad_push_event (demux->sinkpad, gst_event_ref (event));
  }
}

static void
collect_index_table_segments (GstMXFDemux * demux)
{
  GList *l;
  guint i;
  guint64 old_offset = demux->offset;
  GstMXFDemuxPartition *old_partition = demux->current_partition;

  /* This function can also be called when a RIP is not present. This can happen
   * if index table segments were discovered while scanning the file */
  if (demux->random_index_pack) {
    for (i = 0; i < demux->random_index_pack->len; i++) {
      MXFRandomIndexPackEntry *e =
          &g_array_index (demux->random_index_pack, MXFRandomIndexPackEntry, i);

      if (e->offset < demux->run_in) {
        GST_ERROR_OBJECT (demux, "Invalid random index pack entry");
        return;
      }

      demux->offset = e->offset;
      read_partition_header (demux);
    }

    demux->offset = old_offset;
    demux->current_partition = old_partition;
  }

  if (demux->pending_index_table_segments == NULL) {
    GST_DEBUG_OBJECT (demux, "No pending index table segments to collect");
    return;
  }

  GST_LOG_OBJECT (demux, "Collecting pending index table segments");

  for (l = demux->pending_index_table_segments; l; l = l->next) {
    MXFIndexTableSegment *segment = l->data;
    GstMXFDemuxIndexTable *t = NULL;
    GList *k;
    guint didx;
#ifndef GST_DISABLE_GST_DEBUG
    gchar str[48];
#endif

    GST_LOG_OBJECT (demux,
        "Collecting from segment bodySID:%d indexSID:%d instance_id: %s",
        segment->body_sid, segment->index_sid,
        mxf_uuid_to_string (&segment->instance_id, str));

    for (k = demux->index_tables; k; k = k->next) {
      GstMXFDemuxIndexTable *tmp = k->data;

      if (tmp->body_sid == segment->body_sid
          && tmp->index_sid == segment->index_sid) {
        t = tmp;
        break;
      }
    }

    if (!t) {
      t = g_new0 (GstMXFDemuxIndexTable, 1);
      t->body_sid = segment->body_sid;
      t->index_sid = segment->index_sid;
      t->max_temporal_offset = 0;
      t->segments = g_array_new (FALSE, TRUE, sizeof (MXFIndexTableSegment));
      g_array_set_clear_func (t->segments,
          (GDestroyNotify) mxf_index_table_segment_reset);
      t->reordered_delta_entry = -1;
      t->reverse_temporal_offsets = g_array_new (FALSE, TRUE, 1);
      demux->index_tables = g_list_prepend (demux->index_tables, t);
    }

    /* Store index segment */
    g_array_append_val (t->segments, *segment);

    /* Check if temporal reordering tables should be pre-calculated */
    for (didx = 0; didx < segment->n_delta_entries; didx++) {
      MXFDeltaEntry *delta = &segment->delta_entries[didx];
      if (delta->pos_table_index == -1) {
        if (t->reordered_delta_entry != -1 && didx != t->reordered_delta_entry)
          GST_WARNING_OBJECT (demux,
              "Index Table specifies more than one stream using temporal reordering (%d and %d)",
              didx, t->reordered_delta_entry);
        else
          t->reordered_delta_entry = didx;
      } else if (delta->pos_table_index > 0)
        GST_WARNING_OBJECT (delta,
            "Index Table uses fractional offset, please file a bug");
    }

  }

  /* Handle temporal offset if present and needed */
  for (l = demux->index_tables; l; l = l->next) {
    GstMXFDemuxIndexTable *table = l->data;
    guint segidx;

    /* No reordered entries, skip */
    if (table->reordered_delta_entry == -1)
      continue;

    GST_DEBUG_OBJECT (demux,
        "bodySID:%d indexSID:%d Calculating reverse temporal offset table",
        table->body_sid, table->index_sid);

    for (segidx = 0; segidx < table->segments->len; segidx++) {
      MXFIndexTableSegment *s =
          &g_array_index (table->segments, MXFIndexTableSegment, segidx);
      guint start = s->index_start_position;
      guint stop =
          s->index_duration ? start + s->index_duration : start +
          s->n_index_entries;
      guint entidx = 0;

      if (stop > table->reverse_temporal_offsets->len)
        g_array_set_size (table->reverse_temporal_offsets, stop);

      for (entidx = 0; entidx < s->n_index_entries; entidx++) {
        MXFIndexEntry *entry = &s->index_entries[entidx];
        gint8 offs = -entry->temporal_offset;
        /* Check we don't exceed boundaries */
        if ((start + entidx + entry->temporal_offset) < 0 ||
            (start + entidx + entry->temporal_offset) >
            table->reverse_temporal_offsets->len) {
          GST_ERROR_OBJECT (demux,
              "Temporal offset exceeds boundaries. entry:%d offset:%d max:%d",
              start + entidx, entry->temporal_offset,
              table->reverse_temporal_offsets->len);
        } else {
          /* Applying the temporal offset gives us the entry that should contain this PTS.
           * We store the reverse temporal offset on that entry, i.e. the value it should apply
           * to go from DTS to PTS. (i.e. entry.pts = entry.dts + rto[idx]) */
          g_array_index (table->reverse_temporal_offsets, gint8,
              start + entidx + entry->temporal_offset) = offs;
          if (entry->temporal_offset > (gint) table->max_temporal_offset) {
            GST_LOG_OBJECT (demux,
                "Updating max temporal offset to %d (was %d)",
                entry->temporal_offset, table->max_temporal_offset);
            table->max_temporal_offset = entry->temporal_offset;
          }
        }
      }
    }
  }

  g_list_free_full (demux->pending_index_table_segments, g_free);
  demux->pending_index_table_segments = NULL;

  GST_DEBUG_OBJECT (demux, "Done collecting segments");
}

static gboolean
gst_mxf_demux_seek_pull (GstMXFDemux * demux, GstEvent * event)
{
  GstClockTime keyunit_ts;
  GstFormat format;
  GstSeekFlags flags;
  GstSeekType start_type, stop_type;
  gint64 start, stop;
  gdouble rate;
  gboolean update, flush, keyframe;
  GstSegment seeksegment;
  guint i;
  gboolean ret = TRUE;
  guint32 seqnum;

  gst_event_parse_seek (event, &rate, &format, &flags,
      &start_type, &start, &stop_type, &stop);
  seqnum = gst_event_get_seqnum (event);

  if (seqnum == demux->seqnum) {
    GST_DEBUG_OBJECT (demux, "Already handled requested seek");
    return TRUE;
  }

  GST_DEBUG_OBJECT (demux, "Seek %" GST_PTR_FORMAT, event);

  if (format != GST_FORMAT_TIME)
    goto wrong_format;

  if (rate <= 0.0)
    goto wrong_rate;

  flush = !!(flags & GST_SEEK_FLAG_FLUSH);
  keyframe = !!(flags & GST_SEEK_FLAG_KEY_UNIT);

  if (!demux->index_table_segments_collected) {
    collect_index_table_segments (demux);
    demux->index_table_segments_collected = TRUE;
  }

  if (flush) {
    GstEvent *e;

    /* Flush start up and downstream to make sure data flow and loops are
       idle */
    e = gst_event_new_flush_start ();
    gst_event_set_seqnum (e, seqnum);
    gst_mxf_demux_push_src_event (demux, gst_event_ref (e));
    gst_pad_push_event (demux->sinkpad, e);
  } else {
    /* Pause the pulling task */
    gst_pad_pause_task (demux->sinkpad);
  }

  /* Take the stream lock */
  GST_PAD_STREAM_LOCK (demux->sinkpad);

  if (flush) {
    GstEvent *e;

    /* Stop flushing upstream we need to pull */
    e = gst_event_new_flush_stop (TRUE);
    gst_event_set_seqnum (e, seqnum);
    gst_pad_push_event (demux->sinkpad, e);
  }

  /* Work on a copy until we are sure the seek succeeded. */
  memcpy (&seeksegment, &demux->segment, sizeof (GstSegment));

  GST_DEBUG_OBJECT (demux, "segment before configure %" GST_SEGMENT_FORMAT,
      &demux->segment);

  /* Apply the seek to our segment */
  gst_segment_do_seek (&seeksegment, rate, format, flags,
      start_type, start, stop_type, stop, &update);

  GST_DEBUG_OBJECT (demux,
      "segment initially configured to %" GST_SEGMENT_FORMAT, &seeksegment);

  /* Initialize and reset ourselves if needed */
  if (flush || seeksegment.position != demux->segment.position) {
    GList *tmp;
    if (!demux->metadata_resolved || demux->update_metadata) {
      if (gst_mxf_demux_resolve_references (demux) != GST_FLOW_OK ||
          gst_mxf_demux_update_tracks (demux) != GST_FLOW_OK) {
        goto unresolved_metadata;
      }
    }

    /* Reset all single-track KLV tracking */
    for (tmp = demux->partitions; tmp; tmp = tmp->next) {
      GstMXFDemuxPartition *partition = (GstMXFDemuxPartition *) tmp->data;
      if (partition->single_track) {
        partition->clip_klv.consumed = 0;
      }
    }
  }

  keyunit_ts = seeksegment.position;

  /* Do a first round without changing positions. This is needed to figure out
   * the supporting keyframe position (if any) */
  for (i = 0; i < demux->src->len; i++) {
    GstMXFDemuxPad *p = g_ptr_array_index (demux->src, i);
    GstMXFDemuxEssenceTrack *etrack;
    gint64 track_pos, seeked_pos;

    /* Get track and track position for requested time, handles out of bound internally */
    if (!gst_mxf_demux_pad_to_track_and_position (demux, p,
            seeksegment.position, &etrack, &track_pos))
      goto invalid_position;

    GST_LOG_OBJECT (p,
        "track %d (body_sid:%d index_sid:%d), position %" G_GINT64_FORMAT,
        etrack->track_id, etrack->body_sid, etrack->index_sid, track_pos);

    /* Find supporting keyframe entry */
    seeked_pos = track_pos;
    if (gst_mxf_demux_find_essence_element (demux, etrack, &seeked_pos,
            TRUE) == -1) {
      /* Couldn't find entry, ignore */
      break;
    }

    GST_LOG_OBJECT (p,
        "track %d (body_sid:%d index_sid:%d), position %" G_GINT64_FORMAT
        " entry position %" G_GINT64_FORMAT, etrack->track_id, etrack->body_sid,
        etrack->index_sid, track_pos, seeked_pos);

    if (seeked_pos != track_pos) {
      GstClockTime stream_time;
      if (!gst_mxf_demux_pad_get_stream_time (demux, p, etrack, seeked_pos,
              &stream_time))
        goto invalid_position;
      GST_LOG_OBJECT (p, "Need to seek to stream time %" GST_TIME_FORMAT,
          GST_TIME_ARGS (stream_time));
      keyunit_ts = MIN (seeksegment.position, stream_time);
    }
  }

  if (keyframe && keyunit_ts != seeksegment.position) {
    GST_INFO_OBJECT (demux, "key unit seek, adjusting segment start to "
        "%" GST_TIME_FORMAT, GST_TIME_ARGS (keyunit_ts));
    gst_segment_do_seek (&seeksegment, rate, format, flags,
        start_type, keyunit_ts, stop_type, stop, &update);
  }

  /* Finally set the position to the calculated position */
  if (flush || keyunit_ts != demux->segment.position) {
    guint64 new_offset = -1;

    /* Do the actual seeking */
    for (i = 0; i < demux->src->len; i++) {
      GstMXFDemuxPad *p = g_ptr_array_index (demux->src, i);
      gint64 position;
      guint64 off;

      /* Reset EOS flag on all pads */
      p->eos = FALSE;
      gst_mxf_demux_pad_set_position (demux, p, seeksegment.position);

      /* we always want to send data starting with a key unit */
      position = p->current_essence_track_position;
      off =
          gst_mxf_demux_find_essence_element (demux, p->current_essence_track,
          &position, TRUE);
      if (off == -1) {
        GST_DEBUG_OBJECT (demux, "Unable to find offset for pad %s",
            GST_PAD_NAME (p));
        p->current_essence_track_position = p->current_essence_track->duration;
      } else {
        new_offset = MIN (off, new_offset);
        if (position != p->current_essence_track_position) {
          p->position -=
              gst_util_uint64_scale (p->current_essence_track_position -
              position,
              GST_SECOND * p->current_essence_track->source_track->edit_rate.d,
              p->current_essence_track->source_track->edit_rate.n);
          p->position_accumulated_error = 0.0;
          p->current_material_track_position -=
              gst_util_uint64_scale (p->current_essence_track_position -
              position,
              p->material_track->edit_rate.n *
              p->current_essence_track->source_track->edit_rate.d,
              p->material_track->edit_rate.d *
              p->current_essence_track->source_track->edit_rate.n);
        }
        p->current_essence_track_position = position;
      }
      p->current_essence_track->position = p->current_essence_track_position;
      p->discont = TRUE;
    }
    gst_flow_combiner_reset (demux->flowcombiner);
    if (new_offset == -1) {
      GST_WARNING_OBJECT (demux, "No new offset found");
      ret = FALSE;
    } else {
      demux->offset = new_offset + demux->run_in;
    }
    gst_mxf_demux_set_partition_for_offset (demux, demux->offset);
    /* Reset the state accordingly */
    if (demux->current_partition->single_track
        && demux->current_partition->single_track->wrapping !=
        MXF_ESSENCE_WRAPPING_FRAME_WRAPPING)
      demux->state = GST_MXF_DEMUX_STATE_ESSENCE;
    else
      demux->state = GST_MXF_DEMUX_STATE_KLV;
  }

  if (G_UNLIKELY (demux->close_seg_event)) {
    gst_event_unref (demux->close_seg_event);
    demux->close_seg_event = NULL;
  }

  if (flush) {
    GstEvent *e;

    /* Stop flushing, the sinks are at time 0 now */
    e = gst_event_new_flush_stop (TRUE);
    gst_event_set_seqnum (e, seqnum);
    gst_mxf_demux_push_src_event (demux, e);
  } else {
    GST_DEBUG_OBJECT (demux, "closing running segment %" GST_SEGMENT_FORMAT,
        &demux->segment);

    /* Close the current segment for a linear playback */
    demux->close_seg_event = gst_event_new_segment (&demux->segment);
    gst_event_set_seqnum (demux->close_seg_event, demux->seqnum);
  }

  /* Ok seek succeeded, take the newly configured segment */
  memcpy (&demux->segment, &seeksegment, sizeof (GstSegment));

  /* Notify about the start of a new segment */
  if (demux->segment.flags & GST_SEEK_FLAG_SEGMENT) {
    GstMessage *m;

    m = gst_message_new_segment_start (GST_OBJECT (demux),
        demux->segment.format, demux->segment.position);
    gst_message_set_seqnum (m, seqnum);
    gst_element_post_message (GST_ELEMENT (demux), m);
  }

  /* Tell all the stream a new segment is needed */
  for (i = 0; i < demux->src->len; i++) {
    GstMXFDemuxPad *p = g_ptr_array_index (demux->src, i);
    p->need_segment = TRUE;
  }

  for (i = 0; i < demux->essence_tracks->len; i++) {
    GstMXFDemuxEssenceTrack *t = g_ptr_array_index (demux->essence_tracks, i);
    t->position = -1;
  }

  demux->seqnum = seqnum;

  gst_pad_start_task (demux->sinkpad,
      (GstTaskFunction) gst_mxf_demux_loop, demux->sinkpad, NULL);

  GST_PAD_STREAM_UNLOCK (demux->sinkpad);

  return ret;

  /* ERRORS */
wrong_format:
  {
    GST_WARNING_OBJECT (demux, "seeking only supported in TIME format");
    return FALSE;
  }
wrong_rate:
  {
    GST_WARNING_OBJECT (demux, "only rates > 0.0 are allowed");
    return FALSE;
  }
unresolved_metadata:
  {
    gst_pad_start_task (demux->sinkpad,
        (GstTaskFunction) gst_mxf_demux_loop, demux->sinkpad, NULL);
    GST_PAD_STREAM_UNLOCK (demux->sinkpad);
    GST_WARNING_OBJECT (demux, "metadata can't be resolved");
    return FALSE;
  }

invalid_position:
  {
    if (flush) {
      GstEvent *e;

      /* Stop flushing, the sinks are at time 0 now */
      e = gst_event_new_flush_stop (TRUE);
      gst_event_set_seqnum (e, seqnum);
      gst_mxf_demux_push_src_event (demux, e);
    }
    gst_pad_start_task (demux->sinkpad,
        (GstTaskFunction) gst_mxf_demux_loop, demux->sinkpad, NULL);
    GST_PAD_STREAM_UNLOCK (demux->sinkpad);
    GST_WARNING_OBJECT (demux, "Requested seek position is not valid");
    return FALSE;
  }
}

static gboolean
gst_mxf_demux_src_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstMXFDemux *demux = GST_MXF_DEMUX (parent);
  gboolean ret;

  GST_DEBUG_OBJECT (pad, "handling event %s", GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_SEEK:
      if (demux->random_access)
        ret = gst_mxf_demux_seek_pull (demux, event);
      else
        ret = gst_mxf_demux_seek_push (demux, event);
      gst_event_unref (event);
      break;
    default:
      ret = gst_pad_push_event (demux->sinkpad, event);
      break;
  }

  return ret;
}

static gboolean
gst_mxf_demux_src_query (GstPad * pad, GstObject * parent, GstQuery * query)
{
  GstMXFDemux *demux = GST_MXF_DEMUX (parent);
  gboolean ret = FALSE;
  GstMXFDemuxPad *mxfpad = GST_MXF_DEMUX_PAD (pad);

  GST_DEBUG_OBJECT (pad, "handling query %s",
      gst_query_type_get_name (GST_QUERY_TYPE (query)));

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_POSITION:
    {
      GstFormat format;
      gint64 pos;

      gst_query_parse_position (query, &format, NULL);
      if (format != GST_FORMAT_TIME && format != GST_FORMAT_DEFAULT)
        goto error;

      pos =
          format ==
          GST_FORMAT_DEFAULT ? mxfpad->current_material_track_position :
          mxfpad->position;

      GST_DEBUG_OBJECT (pad,
          "Returning position %" G_GINT64_FORMAT " in format %s", pos,
          gst_format_get_name (format));

      gst_query_set_position (query, format, pos);
      ret = TRUE;

      break;
    }
    case GST_QUERY_DURATION:{
      gint64 duration;
      GstFormat format;

      gst_query_parse_duration (query, &format, NULL);
      if (format != GST_FORMAT_TIME && format != GST_FORMAT_DEFAULT)
        goto error;

      g_rw_lock_reader_lock (&demux->metadata_lock);
      if (!mxfpad->material_track || !mxfpad->material_track->parent.sequence) {
        g_rw_lock_reader_unlock (&demux->metadata_lock);
        goto error;
      }

      duration = mxfpad->material_track->parent.sequence->duration;
      if (duration <= -1)
        duration = -1;

      if (duration != -1 && format == GST_FORMAT_TIME) {
        if (mxfpad->material_track->edit_rate.n == 0 ||
            mxfpad->material_track->edit_rate.d == 0) {
          g_rw_lock_reader_unlock (&demux->metadata_lock);
          goto error;
        }

        duration =
            gst_util_uint64_scale (duration,
            GST_SECOND * mxfpad->material_track->edit_rate.d,
            mxfpad->material_track->edit_rate.n);
      }
      g_rw_lock_reader_unlock (&demux->metadata_lock);

      GST_DEBUG_OBJECT (pad,
          "Returning duration %" G_GINT64_FORMAT " in format %s", duration,
          gst_format_get_name (format));

      gst_query_set_duration (query, format, duration);
      ret = TRUE;
      break;
    }
    case GST_QUERY_SEEKING:{
      GstFormat fmt;
      gint64 duration;

      ret = TRUE;
      gst_query_parse_seeking (query, &fmt, NULL, NULL, NULL);
      if (fmt != GST_FORMAT_TIME) {
        gst_query_set_seeking (query, fmt, FALSE, -1, -1);
        goto done;
      }

      if (!gst_pad_query_duration (pad, GST_FORMAT_TIME, &duration)) {
        gst_query_set_seeking (query, fmt, FALSE, -1, -1);
        goto done;
      }

      if (demux->random_access) {
        gst_query_set_seeking (query, GST_FORMAT_TIME, TRUE, 0, duration);
      } else {
        GstQuery *peerquery = gst_query_new_seeking (GST_FORMAT_BYTES);
        gboolean seekable;

        seekable = gst_pad_peer_query (demux->sinkpad, peerquery);
        if (seekable)
          gst_query_parse_seeking (peerquery, NULL, &seekable, NULL, NULL);
        if (seekable)
          gst_query_set_seeking (query, GST_FORMAT_TIME, TRUE, 0, duration);
        else
          gst_query_set_seeking (query, GST_FORMAT_TIME, FALSE, -1, -1);

        gst_query_unref (peerquery);
      }

      break;
    }
    case GST_QUERY_SEGMENT:{
      GstFormat format;
      gint64 start, stop;

      format = demux->segment.format;

      start =
          gst_segment_to_stream_time (&demux->segment, format,
          demux->segment.start);
      if ((stop = demux->segment.stop) == -1)
        stop = demux->segment.duration;
      else
        stop = gst_segment_to_stream_time (&demux->segment, format, stop);

      gst_query_set_segment (query, demux->segment.rate, format, start, stop);
      ret = TRUE;
      break;
    }
    default:
      ret = gst_pad_query_default (pad, parent, query);
      break;
  }

done:
  return ret;

  /* ERRORS */
error:
  {
    GST_DEBUG_OBJECT (pad, "query failed");
    goto done;
  }
}

static gboolean
gst_mxf_demux_sink_activate (GstPad * sinkpad, GstObject * parent)
{
  GstQuery *query;
  GstPadMode mode = GST_PAD_MODE_PUSH;

  query = gst_query_new_scheduling ();

  if (gst_pad_peer_query (sinkpad, query)) {
    if (gst_query_has_scheduling_mode_with_flags (query,
            GST_PAD_MODE_PULL, GST_SCHEDULING_FLAG_SEEKABLE)) {
      GstSchedulingFlags flags;
      gst_query_parse_scheduling (query, &flags, NULL, NULL, NULL);
      if (!(flags & GST_SCHEDULING_FLAG_SEQUENTIAL))
        mode = GST_PAD_MODE_PULL;
    }
  }
  gst_query_unref (query);

  return gst_pad_activate_mode (sinkpad, mode, TRUE);
}

static gboolean
gst_mxf_demux_sink_activate_mode (GstPad * sinkpad, GstObject * parent,
    GstPadMode mode, gboolean active)
{
  GstMXFDemux *demux;

  demux = GST_MXF_DEMUX (parent);

  if (mode == GST_PAD_MODE_PUSH) {
    demux->random_access = FALSE;
  } else {
    if (active) {
      demux->random_access = TRUE;
      return gst_pad_start_task (sinkpad, (GstTaskFunction) gst_mxf_demux_loop,
          sinkpad, NULL);
    } else {
      demux->random_access = FALSE;
      return gst_pad_stop_task (sinkpad);
    }
  }

  return TRUE;
}

static gboolean
gst_mxf_demux_sink_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstMXFDemux *demux;
  gboolean ret = FALSE;

  demux = GST_MXF_DEMUX (parent);

  GST_DEBUG_OBJECT (pad, "handling event %s", GST_EVENT_TYPE_NAME (event));

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_FLUSH_START:
      demux->flushing = TRUE;
      ret = gst_pad_event_default (pad, parent, event);
      break;
    case GST_EVENT_FLUSH_STOP:
      GST_DEBUG_OBJECT (demux, "flushing queued data in the MXF demuxer");

      gst_adapter_clear (demux->adapter);
      demux->flushing = FALSE;
      demux->offset = 0;
      ret = gst_pad_event_default (pad, parent, event);
      break;
    case GST_EVENT_EOS:{
      GstMXFDemuxPad *p = NULL;
      guint i;

      if (demux->src->len == 0) {
        GST_ELEMENT_ERROR (demux, STREAM, WRONG_TYPE,
            ("This stream contains no data."),
            ("got eos and didn't find any streams"));
      }

      for (i = 0; i < demux->essence_tracks->len; i++) {
        GstMXFDemuxEssenceTrack *t =
            g_ptr_array_index (demux->essence_tracks, i);

        if (t->position > 0)
          t->duration = t->position;
      }

      for (i = 0; i < demux->src->len; i++) {
        GstMXFDemuxPad *p = g_ptr_array_index (demux->src, i);

        if (!p->eos
            && p->current_essence_track_position >=
            p->current_essence_track->duration) {
          p->eos = TRUE;
          gst_pad_push_event (GST_PAD_CAST (p), gst_event_new_eos ());
        }
      }

      while ((p = gst_mxf_demux_get_earliest_pad (demux))) {
        guint64 offset;
        gint64 position;

        position = p->current_essence_track_position;

        offset =
            gst_mxf_demux_find_essence_element (demux, p->current_essence_track,
            &position, FALSE);
        if (offset == -1) {
          GST_ERROR_OBJECT (demux, "Failed to find offset for essence track");
          p->eos = TRUE;
          gst_pad_push_event (GST_PAD_CAST (p), gst_event_new_eos ());
          continue;
        }

        if (gst_pad_push_event (demux->sinkpad,
                gst_event_new_seek (demux->segment.rate, GST_FORMAT_BYTES,
                    demux->segment.flags | GST_SEEK_FLAG_ACCURATE,
                    GST_SEEK_TYPE_SET, offset + demux->run_in,
                    GST_SEEK_TYPE_NONE, 0))) {

          for (i = 0; i < demux->essence_tracks->len; i++) {
            GstMXFDemuxEssenceTrack *etrack =
                g_ptr_array_index (demux->essence_tracks, i);
            etrack->position = -1;
          }
          ret = TRUE;
          goto out;
        } else {
          GST_WARNING_OBJECT (demux,
              "Seek to remaining part of the file failed");
          p->eos = TRUE;
          gst_pad_push_event (GST_PAD_CAST (p), gst_event_new_eos ());
          continue;
        }
      }

      /* and one more time for good measure apparently? */
      gst_pad_event_default (pad, parent, event);
      ret = (demux->src->len > 0);
      break;
    }
    case GST_EVENT_SEGMENT:{
      guint i;

      for (i = 0; i < demux->essence_tracks->len; i++) {
        GstMXFDemuxEssenceTrack *t =
            g_ptr_array_index (demux->essence_tracks, i);
        t->position = -1;
      }
      demux->current_partition = NULL;
      demux->seqnum = gst_event_get_seqnum (event);
      gst_event_unref (event);
      ret = TRUE;
      break;
    }
    default:
      ret = gst_pad_event_default (pad, parent, event);
      break;
  }

out:

  return ret;
}

static gboolean
gst_mxf_demux_query (GstElement * element, GstQuery * query)
{
  GstMXFDemux *demux = GST_MXF_DEMUX (element);
  gboolean ret = FALSE;

  GST_DEBUG_OBJECT (demux, "handling query %s",
      gst_query_type_get_name (GST_QUERY_TYPE (query)));

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_POSITION:
    {
      GstFormat format;
      gint64 pos;

      gst_query_parse_position (query, &format, NULL);
      if (format != GST_FORMAT_TIME)
        goto error;

      pos = demux->segment.position;

      GST_DEBUG_OBJECT (demux,
          "Returning position %" G_GINT64_FORMAT " in format %s", pos,
          gst_format_get_name (format));

      gst_query_set_position (query, format, pos);
      ret = TRUE;

      break;
    }
    case GST_QUERY_DURATION:{
      gint64 duration = -1;
      GstFormat format;
      guint i;

      gst_query_parse_duration (query, &format, NULL);
      if (format != GST_FORMAT_TIME)
        goto error;

      if (demux->src->len == 0)
        goto done;

      g_rw_lock_reader_lock (&demux->metadata_lock);
      for (i = 0; i < demux->src->len; i++) {
        GstMXFDemuxPad *pad = g_ptr_array_index (demux->src, i);
        gint64 pdur = -1;

        if (!pad->material_track || !pad->material_track->parent.sequence)
          continue;

        pdur = pad->material_track->parent.sequence->duration;
        if (pad->material_track->edit_rate.n == 0 ||
            pad->material_track->edit_rate.d == 0 || pdur <= -1)
          continue;

        pdur =
            gst_util_uint64_scale (pdur,
            GST_SECOND * pad->material_track->edit_rate.d,
            pad->material_track->edit_rate.n);
        duration = MAX (duration, pdur);
      }
      g_rw_lock_reader_unlock (&demux->metadata_lock);

      if (duration == -1) {
        GST_DEBUG_OBJECT (demux, "No duration known (yet)");
        goto done;
      }

      GST_DEBUG_OBJECT (demux,
          "Returning duration %" G_GINT64_FORMAT " in format %s", duration,
          gst_format_get_name (format));

      gst_query_set_duration (query, format, duration);
      ret = TRUE;
      break;
    }
    case GST_QUERY_SEEKING:{
      GstFormat fmt;

      ret = TRUE;
      gst_query_parse_seeking (query, &fmt, NULL, NULL, NULL);
      if (fmt != GST_FORMAT_TIME) {
        gst_query_set_seeking (query, fmt, FALSE, -1, -1);
        goto done;
      }

      if (demux->random_access) {
        gst_query_set_seeking (query, GST_FORMAT_TIME, TRUE, 0, -1);
      } else {
        GstQuery *peerquery = gst_query_new_seeking (GST_FORMAT_BYTES);
        gboolean seekable;

        seekable = gst_pad_peer_query (demux->sinkpad, peerquery);
        if (seekable)
          gst_query_parse_seeking (peerquery, NULL, &seekable, NULL, NULL);
        if (seekable)
          gst_query_set_seeking (query, GST_FORMAT_TIME, TRUE, 0, -1);
        else
          gst_query_set_seeking (query, GST_FORMAT_TIME, FALSE, -1, -1);
      }

      break;
    }
    case GST_QUERY_SEGMENT:{
      GstFormat format;
      gint64 start, stop;

      format = demux->segment.format;

      start =
          gst_segment_to_stream_time (&demux->segment, format,
          demux->segment.start);
      if ((stop = demux->segment.stop) == -1)
        stop = demux->segment.duration;
      else
        stop = gst_segment_to_stream_time (&demux->segment, format, stop);

      gst_query_set_segment (query, demux->segment.rate, format, start, stop);
      ret = TRUE;
      break;
    }
    default:
      /* else forward upstream */
      ret = gst_pad_peer_query (demux->sinkpad, query);
      break;
  }

done:
  return ret;

  /* ERRORS */
error:
  {
    GST_DEBUG_OBJECT (demux, "query failed");
    goto done;
  }
}

static GstStateChangeReturn
gst_mxf_demux_change_state (GstElement * element, GstStateChange transition)
{
  GstMXFDemux *demux = GST_MXF_DEMUX (element);
  GstStateChangeReturn ret;

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      demux->seqnum = gst_util_seqnum_next ();
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      gst_mxf_demux_reset (demux);
      break;
    default:
      break;
  }

  return ret;
}

static void
gst_mxf_demux_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstMXFDemux *demux = GST_MXF_DEMUX (object);

  switch (prop_id) {
    case PROP_PACKAGE:
      g_free (demux->requested_package_string);
      demux->requested_package_string = g_value_dup_string (value);
      break;
    case PROP_MAX_DRIFT:
      demux->max_drift = g_value_get_uint64 (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_mxf_demux_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstMXFDemux *demux = GST_MXF_DEMUX (object);

  switch (prop_id) {
    case PROP_PACKAGE:
      g_value_set_string (value, demux->current_package_string);
      break;
    case PROP_MAX_DRIFT:
      g_value_set_uint64 (value, demux->max_drift);
      break;
    case PROP_STRUCTURE:{
      GstStructure *s;

      g_rw_lock_reader_lock (&demux->metadata_lock);
      if (demux->preface &&
          MXF_METADATA_BASE (demux->preface)->resolved ==
          MXF_METADATA_BASE_RESOLVE_STATE_SUCCESS)
        s = mxf_metadata_base_to_structure (MXF_METADATA_BASE (demux->preface));
      else
        s = NULL;

      gst_value_set_structure (value, s);

      if (s)
        gst_structure_free (s);

      g_rw_lock_reader_unlock (&demux->metadata_lock);
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_mxf_demux_finalize (GObject * object)
{
  GstMXFDemux *demux = GST_MXF_DEMUX (object);

  gst_mxf_demux_reset (demux);

  if (demux->adapter) {
    g_object_unref (demux->adapter);
    demux->adapter = NULL;
  }

  if (demux->flowcombiner) {
    gst_flow_combiner_free (demux->flowcombiner);
    demux->flowcombiner = NULL;
  }

  if (demux->close_seg_event) {
    gst_event_unref (demux->close_seg_event);
    demux->close_seg_event = NULL;
  }

  g_free (demux->current_package_string);
  demux->current_package_string = NULL;
  g_free (demux->requested_package_string);
  demux->requested_package_string = NULL;

  g_ptr_array_free (demux->src, TRUE);
  demux->src = NULL;
  g_ptr_array_free (demux->essence_tracks, TRUE);
  demux->essence_tracks = NULL;

  g_hash_table_destroy (demux->metadata);

  g_rw_lock_clear (&demux->metadata_lock);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_mxf_demux_class_init (GstMXFDemuxClass * klass)
{
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  GST_DEBUG_CATEGORY_INIT (mxfdemux_debug, "mxfdemux", 0, "MXF demuxer");

  parent_class = g_type_class_peek_parent (klass);

  gobject_class->finalize = gst_mxf_demux_finalize;
  gobject_class->set_property = gst_mxf_demux_set_property;
  gobject_class->get_property = gst_mxf_demux_get_property;

  g_object_class_install_property (gobject_class, PROP_PACKAGE,
      g_param_spec_string ("package", "Package",
          "Material or Source package to use for playback", NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_MAX_DRIFT,
      g_param_spec_uint64 ("max-drift", "Maximum drift",
          "Maximum number of nanoseconds by which tracks can differ",
          100 * GST_MSECOND, G_MAXUINT64, DEFAULT_MAX_DRIFT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_STRUCTURE,
      g_param_spec_boxed ("structure", "Structure",
          "Structural metadata of the MXF file",
          GST_TYPE_STRUCTURE, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_mxf_demux_change_state);
  gstelement_class->query = GST_DEBUG_FUNCPTR (gst_mxf_demux_query);

  gst_element_class_add_static_pad_template (gstelement_class,
      &mxf_sink_template);
  gst_element_class_add_static_pad_template (gstelement_class,
      &mxf_src_template);
  gst_element_class_set_static_metadata (gstelement_class, "MXF Demuxer",
      "Codec/Demuxer", "Demux MXF files",
      "Sebastian Dröge <sebastian.droege@collabora.co.uk>");
}

static void
gst_mxf_demux_init (GstMXFDemux * demux)
{
  demux->sinkpad =
      gst_pad_new_from_static_template (&mxf_sink_template, "sink");

  gst_pad_set_event_function (demux->sinkpad,
      GST_DEBUG_FUNCPTR (gst_mxf_demux_sink_event));
  gst_pad_set_chain_function (demux->sinkpad,
      GST_DEBUG_FUNCPTR (gst_mxf_demux_chain));
  gst_pad_set_activate_function (demux->sinkpad,
      GST_DEBUG_FUNCPTR (gst_mxf_demux_sink_activate));
  gst_pad_set_activatemode_function (demux->sinkpad,
      GST_DEBUG_FUNCPTR (gst_mxf_demux_sink_activate_mode));

  gst_element_add_pad (GST_ELEMENT (demux), demux->sinkpad);

  demux->max_drift = DEFAULT_MAX_DRIFT;

  demux->adapter = gst_adapter_new ();
  demux->flowcombiner = gst_flow_combiner_new ();
  g_rw_lock_init (&demux->metadata_lock);

  demux->src = g_ptr_array_new ();
  demux->essence_tracks = g_ptr_array_new_with_free_func ((GDestroyNotify)
      gst_mxf_demux_essence_track_free);

  gst_segment_init (&demux->segment, GST_FORMAT_TIME);

  gst_mxf_demux_reset (demux);
}
