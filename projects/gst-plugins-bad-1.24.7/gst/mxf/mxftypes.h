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

/* Definitions of the basic MXF types, excluding structural metadata */

#ifndef __MXF_TYPES_H__
#define __MXF_TYPES_H__

#include <gst/gst.h>

#include "mxful.h"

typedef struct {
  guint8 u[16];
} MXFUUID;

/* SMPTE 377M 3.2 */
typedef struct {
  guint8 u[32];
} MXFUMID;

/* SMPTE 377M 3.3 */
typedef struct {
  gint16 year;
  guint8 month;
  guint8 day;
  guint8 hour;
  guint8 minute;
  guint8 second;
  guint16 msecond;
} MXFTimestamp;

/* SMPTE 377M 3.3 */
typedef struct {
  guint16 major;
  guint16 minor;
  guint16 patch;
  guint16 build;
  guint16 release;
} MXFProductVersion;

/* SMPTE 377M 3.3 */
typedef struct {
  gint32 n;
  gint32 d;
} MXFFraction;

/* SMPTE 377M 8.3 */
typedef struct {
  MXFUL ul;
  guint16 size;
  guint8 *data;
} MXFLocalTag;

/* SMPTE 377M 11.1 */
typedef struct {
  guint32 body_sid;
  guint64 offset;
} MXFRandomIndexPackEntry;

typedef enum {
  MXF_OP_UNKNOWN = 0,
  MXF_OP_ATOM,
  MXF_OP_1a,
  MXF_OP_1b,
  MXF_OP_1c,
  MXF_OP_2a,
  MXF_OP_2b,
  MXF_OP_2c,
  MXF_OP_3a,
  MXF_OP_3b,
  MXF_OP_3c,
} MXFOperationalPattern;

typedef enum {
  MXF_PARTITION_PACK_HEADER,
  MXF_PARTITION_PACK_BODY,
  MXF_PARTITION_PACK_FOOTER
} MXFPartitionPackType;

/* SMPTE 377M 6.1, Table 1 and 2 */
typedef struct {
  MXFPartitionPackType type;

  gboolean closed;
  gboolean complete;

  guint16 major_version;
  guint16 minor_version;

  guint32 kag_size;

  guint64 this_partition;
  guint64 prev_partition;
  guint64 footer_partition;

  guint64 header_byte_count;
  guint64 index_byte_count;

  guint32 index_sid;

  guint64 body_offset;

  guint32 body_sid;

  MXFUL operational_pattern;

  guint32 n_essence_containers;
  MXFUL *essence_containers;
} MXFPartitionPack;

/* SMPTE 377M 8.1 */
typedef struct {
  guint64 offset;
  GHashTable *mappings;
  GHashTable *reverse_mappings;
  guint16 next_free_tag;
} MXFPrimerPack;

/* SMPTE 377M 10.2.3 */
typedef struct {
  gint8 pos_table_index;
  guint8 slice;
  guint32 element_delta;
} MXFDeltaEntry;

typedef struct {
  gint8 temporal_offset;
  gint8 key_frame_offset;

  guint8 flags;
  guint64 stream_offset;

  guint32 *slice_offset;
  MXFFraction *pos_table;
} MXFIndexEntry;

typedef struct {
  MXFUUID instance_id;
  MXFFraction index_edit_rate;
  gint64 index_start_position;
  gint64 index_duration;
  guint32 edit_unit_byte_count;
  guint32 index_sid;
  guint32 body_sid;
  guint8 slice_count;
  guint8 pos_table_count;

  guint32 n_delta_entries;
  MXFDeltaEntry *delta_entries;

  guint32 n_index_entries;
  MXFIndexEntry *index_entries;

  /* Computed fields (i.e. not present in file) */

  /* Initial essence offset covered by this segment */
  guint64 segment_start_offset;
} MXFIndexTableSegment;

#define GST_TAG_MXF_UMID "mxf-umid"
#define GST_TAG_MXF_STRUCTURE "mxf-structure"
#define GST_TAG_MXF_DESCRIPTIVE_METADATA_FRAMEWORK "mxf-descriptive-metadata-framework"

void mxf_uuid_init (MXFUUID *uuid, GHashTable *hashtable);
gboolean mxf_uuid_is_equal (const MXFUUID *a, const MXFUUID *b);
gboolean mxf_uuid_is_zero (const MXFUUID *uuid);
guint mxf_uuid_hash (const MXFUUID *uuid);
gchar * mxf_uuid_to_string (const MXFUUID *uuid, gchar str[48]);
MXFUUID * mxf_uuid_from_string (const gchar *str, MXFUUID *uuid);
gboolean mxf_uuid_array_parse (MXFUUID ** array, guint32 * count, const guint8 * data, guint size);

gchar *mxf_umid_to_string (const MXFUMID * umid, gchar str[96]);
MXFUMID *mxf_umid_from_string (const gchar *str, MXFUMID * umid);
gboolean mxf_umid_is_equal (const MXFUMID *a, const MXFUMID *b);
gboolean mxf_umid_is_zero (const MXFUMID *umid);
void mxf_umid_init (MXFUMID *umid);

gboolean mxf_is_mxf_packet (const MXFUL *ul);

gboolean mxf_is_partition_pack (const MXFUL *ul);
gboolean mxf_is_header_partition_pack (const MXFUL *ul);
gboolean mxf_is_body_partition_pack (const MXFUL *ul);
gboolean mxf_is_footer_partition_pack (const MXFUL *ul);

gboolean mxf_is_primer_pack (const MXFUL *ul);

gboolean mxf_is_metadata (const MXFUL *ul);
gboolean mxf_is_descriptive_metadata (const MXFUL *ul);

gboolean mxf_is_random_index_pack (const MXFUL *ul);
gboolean mxf_is_index_table_segment (const MXFUL *ul);

gboolean mxf_is_generic_container_system_item (const MXFUL *ul);
gboolean mxf_is_generic_container_essence_element (const MXFUL *ul);
gboolean mxf_is_avid_essence_container_essence_element (const MXFUL * key);

gboolean mxf_is_generic_container_essence_container_label (const MXFUL *ul);
gboolean mxf_is_avid_essence_container_label (const MXFUL *ul);

gboolean mxf_is_fill (const MXFUL *ul);

guint mxf_ber_encode_size (guint size, guint8 ber[9]);

gchar * mxf_utf16_to_utf8 (const guint8 * data, guint size);
guint8 * mxf_utf8_to_utf16 (const gchar *str, guint16 *size);

gboolean mxf_product_version_parse (MXFProductVersion * product_version,
    const guint8 * data, guint size);
gboolean mxf_product_version_is_valid (const MXFProductVersion *version);
void mxf_product_version_write (const MXFProductVersion *version, guint8 *data);


gboolean mxf_fraction_parse (MXFFraction *fraction, const guint8 *data, guint size);
gdouble mxf_fraction_to_double (const MXFFraction *fraction);

gboolean mxf_timestamp_parse (MXFTimestamp * timestamp, const guint8 * data, guint size);
gboolean mxf_timestamp_is_unknown (const MXFTimestamp *a);
gint mxf_timestamp_compare (const MXFTimestamp *a, const MXFTimestamp *b);
gchar *mxf_timestamp_to_string (const MXFTimestamp *t, gchar str[32]);
void mxf_timestamp_set_now (MXFTimestamp *timestamp);
void mxf_timestamp_write (const MXFTimestamp *timestamp, guint8 *data);

void mxf_op_set_atom (MXFUL *ul, gboolean single_sourceclip, gboolean single_essence_track);
void mxf_op_set_generalized (MXFUL *ul, MXFOperationalPattern pattern, gboolean internal_essence, gboolean streamable, gboolean single_track);

GstBuffer * mxf_fill_to_buffer (guint size);

gboolean mxf_partition_pack_parse (const MXFUL *ul, MXFPartitionPack *pack, const guint8 *data, guint size);
void mxf_partition_pack_reset (MXFPartitionPack *pack);
GstBuffer * mxf_partition_pack_to_buffer (const MXFPartitionPack *pack);

gboolean mxf_primer_pack_parse (const MXFUL *ul, MXFPrimerPack *pack, const guint8 *data, guint size);
void mxf_primer_pack_reset (MXFPrimerPack *pack);
guint16 mxf_primer_pack_add_mapping (MXFPrimerPack *primer, guint16 local_tag, const MXFUL *ul);
GstBuffer * mxf_primer_pack_to_buffer (const MXFPrimerPack *pack);

gboolean mxf_random_index_pack_parse (const MXFUL *ul, const guint8 *data, guint size, GArray **array);
GstBuffer * mxf_random_index_pack_to_buffer (const GArray *array);

gboolean mxf_index_table_segment_parse (const MXFUL *ul, MXFIndexTableSegment *segment, const guint8 *data, guint size);
void mxf_index_table_segment_reset (MXFIndexTableSegment *segment);
GstBuffer * mxf_index_table_segment_to_buffer (const MXFIndexTableSegment *segment);

gboolean mxf_local_tag_parse (const guint8 * data, guint size, guint16 * tag,
    guint16 * tag_size, const guint8 ** tag_data);
void mxf_local_tag_free (MXFLocalTag *tag);

gboolean mxf_local_tag_add_to_hash_table (const MXFPrimerPack *primer,
  guint16 tag, const guint8 *tag_data, guint16 tag_size,
  GHashTable **hash_table);
MXFUL *mxf_primer_tag_to_ul (const MXFPrimerPack *primer, guint16 tag);
gboolean mxf_local_tag_insert (MXFLocalTag *tag, GHashTable **hash_table);

#endif /* __MXF_TYPES_H__ */
