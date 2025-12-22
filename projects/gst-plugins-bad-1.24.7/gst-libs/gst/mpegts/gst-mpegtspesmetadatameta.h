/* Gstreamer
 * Copyright 2021,2023 Brad Hards <bradh@frogmouth.net>
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GST_MPEGTS_PES_METADATA_META_H__
#define __GST_MPEGTS_PES_METADATA_META_H__

#include <gst/gst.h>
#include <gst/mpegts/mpegts-prelude.h>

G_BEGIN_DECLS

typedef struct _GstMpegtsPESMetadataMeta GstMpegtsPESMetadataMeta;

/**
 * gst_mpegts_pes_metadata_meta_api_get_type
 *
 * Return the #GType associated with #GstMpegtsPESMetadataMeta
 *
 * Returns: a #GType
 *
 * Since: 1.24
 */
GST_MPEGTS_API
GType gst_mpegts_pes_metadata_meta_api_get_type (void);

/**
 * GST_MPEGTS_PES_METADATA_META_API_TYPE:
 *
 * The #GType associated with #GstMpegtsPESMetadataMeta.
 *
 * Since: 1.24
 */
#define GST_MPEGTS_PES_METADATA_META_API_TYPE  (gst_mpegts_pes_metadata_meta_api_get_type())

/**
 * GST_MPEGTS_PES_METADATA_META_INFO:
 *
 * The #GstMetaInfo associated with #GstMpegtsPESMetadataMeta.
 *
 * Since: 1.24
 */
#define GST_MPEGTS_PES_METADATA_META_INFO  (gst_mpegts_pes_metadata_meta_get_info())

/**
 * gst_mpegts_pes_metadata_meta_get_info:
 *
 * Gets the global #GstMetaInfo describing the #GstMpegtsPESMetadataMeta meta.
 *
 * Returns: (transfer none): The #GstMetaInfo
 *
 * Since: 1.24
 */
GST_MPEGTS_API
const GstMetaInfo * gst_mpegts_pes_metadata_meta_get_info (void);

/**
 * GstMpegtsPESMetadataMeta:
 * @meta: parent #GstMeta
 * @metadata_service_id: metadata service identifier
 * @flags: bit flags, see spec for details
 *
 * Extra buffer metadata describing the PES Metadata context.
 * This is based on the Metadata AU cell header in
 * ISO/IEC 13818-1:2018 Section 2.12.4.
 *
 * AU_cell_data_length is not provided, since it matches the length of the buffer
 *
 * Since: 1.24
 */
struct _GstMpegtsPESMetadataMeta {
  GstMeta            meta;
  guint8             metadata_service_id;
  guint8             flags;
};

/**
 * gst_buffer_add_mpegts_pes_metadata_meta:
 * @buffer: a #GstBuffer
 *
 * Creates and adds a #GstMpegtsPESMetadataMeta to a @buffer.
 *
 * Returns: (transfer none): a newly created #GstMpegtsPESMetadataMeta
 *
 * Since: 1.24
 */
GST_MPEGTS_API
GstMpegtsPESMetadataMeta *
gst_buffer_add_mpegts_pes_metadata_meta (GstBuffer * buffer);

G_END_DECLS

#endif
