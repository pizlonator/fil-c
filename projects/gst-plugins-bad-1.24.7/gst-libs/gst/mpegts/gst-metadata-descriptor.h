/* Gstreamer
 * Copyright 2023 Brad Hards <bradh@frogmouth.net>
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

#ifndef __GST_MPEGTS_METADATA_DESCRIPTOR_H__
#define __GST_MPEGTS_METADATA_DESCRIPTOR_H__

#include <gst/gst.h>
#include <gst/mpegts/mpegts-prelude.h>

G_BEGIN_DECLS

/**
 * GstMpegtsMetadataFormat:
 *
 * metadata_descriptor metadata_format valid values. See ISO/IEC 13818-1:2018(E) Table 2-85.
 *
 * Since: 1.24
 */
typedef enum {
  /**
   * GST_MPEGTS_METADATA_FORMAT_TEM:
   *
   * ISO/IEC 15938-1 TeM.
   *
   * Since: 1.24
   */
  GST_MPEGTS_METADATA_FORMAT_TEM = 0x10,
  /**
   * GST_MPEGTS_METADATA_FORMAT_BIM:
   *
   * ISO/IEC 15938-1 BiM.
   *
   * Since: 1.24
   */
  GST_MPEGTS_METADATA_FORMAT_BIM = 0x11,
  /**
   * GST_MPEGTS_METADATA_FORMAT_APPLICATION_FORMAT:
   *
   * Defined by metadata application format.
   *
   * Since: 1.24
   */
  GST_MPEGTS_METADATA_FORMAT_APPLICATION_FORMAT = 0x3f,
  /**
   * GST_MPEGTS_METADATA_FORMAT_IDENTIFIER_FIELD:
   *
   * Defined by metadata_format_identifier field.
   *
   * Since: 1.24
   */
  GST_MPEGTS_METADATA_FORMAT_IDENTIFIER_FIELD = 0xff
} GstMpegtsMetadataFormat;


/* MPEG-TS Metadata Descriptor (0x26) */
typedef struct _GstMpegtsMetadataDescriptor GstMpegtsMetadataDescriptor;

/**
 * GstMpegtsMetadataDescriptor:
 * @metadata_application_format: specifies the application responsible for defining usage, syntax and semantics
 * @metadata_format: indicates the format and coding of the metadata
 * @metadata_format_identifier: format identifier (equivalent to registration descriptor), for example 0x4B4C4641 ('KLVA') to indicate SMPTE 336 KLV.
 * @metadata_service_id:  metadata service to which this metadata descriptor applies, typically 0x00
 * @decoder_config_flags: decoder flags, see ISO/IEC 13818-1:2018 Table 2-88.
 * @dsm_cc_flag: true if stream associated with this descriptor is in an ISO/IEC 13818-6 data or object carousel.
 *
 * The metadata descriptor specifies parameters of a metadata service carried in an MPEG-2 Transport Stream (or Program Stream). The descriptor is included in the PMT in the descriptor loop for the elementary stream that carries the
metadata service. The descriptor specifies the format of the associated metadata, and contains the value of the
metadata_service_id to identify the metadata service to which the metadata descriptor applies.
 *
 * Note that this structure does not include all of the metadata_descriptor items, and will need extension to support DSM-CC and private data.
 * See ISO/IEC 13818-1:2018 Section 2.6.60 and Section 2.6.61 for more information.
 *
 * Since: 1.24
 */
struct _GstMpegtsMetadataDescriptor
{
  guint16 metadata_application_format;
  GstMpegtsMetadataFormat metadata_format;
  guint32 metadata_format_identifier;
  guint8 metadata_service_id;
  guint8 decoder_config_flags;
  gboolean dsm_cc_flag;
};

/**
 * GST_TYPE_MPEGTS_METADATA_DESCRIPTOR:
 *
 * metadata_descriptor type
 *
 * Since: 1.24
 */
#define GST_TYPE_MPEGTS_METADATA_DESCRIPTOR (gst_mpegts_metadata_descriptor_get_type ())

GST_MPEGTS_API
GType gst_mpegts_metadata_descriptor_get_type (void);

G_END_DECLS

#endif
