/* GStreamer
 *  Copyright (C) 2022 Intel Corporation
 *     Author: He Junyan <junyan.he@intel.com>
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
 * License along with this library; if not, write to the0
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __GST_AV1_BIT_WRITER_H__
#define __GST_AV1_BIT_WRITER_H__

#include <gst/codecparsers/gstav1parser.h>
#include <gst/codecparsers/codecparsers-prelude.h>

G_BEGIN_DECLS

/**
 * GstAV1BitWriterResult:
 * @GST_AV1_BIT_WRITER_OK: The writing succeeded
 * @GST_AV1_BIT_WRITER_INVALID_DATA: The input data to write is invalid
 * @GST_AV1_BIT_WRITER_NO_MORE_SPACE: The output does not have enough size
 * @GST_AV1_BIT_WRITER_ERROR: An general error occurred when writing
 *
 * The result of writing AV1 data into bit stream.
 *
 * Since: 1.24
 */
typedef enum
{
  GST_AV1_BIT_WRITER_OK,
  GST_AV1_BIT_WRITER_INVALID_DATA,
  GST_AV1_BIT_WRITER_NO_MORE_SPACE,
  GST_AV1_BIT_WRITER_ERROR
} GstAV1BitWriterResult;

GST_CODEC_PARSERS_API
GstAV1BitWriterResult    gst_av1_bit_writer_sequence_header_obu (const GstAV1SequenceHeaderOBU * seq_hdr,
                                                                 gboolean size_field,
                                                                 guint8 * data,
                                                                 guint * size);
GST_CODEC_PARSERS_API
GstAV1BitWriterResult    gst_av1_bit_writer_frame_header_obu    (const GstAV1FrameHeaderOBU * frame_hdr,
                                                                 const GstAV1SequenceHeaderOBU * seq_hdr,
                                                                 guint8 temporal_id,
                                                                 guint8 spatial_id,
                                                                 gboolean size_field,
                                                                 guint8 * data,
                                                                 guint * size);
GST_CODEC_PARSERS_API
GstAV1BitWriterResult    gst_av1_bit_writer_frame_header_obu_with_offsets (const GstAV1FrameHeaderOBU * frame_hdr,
                                                                           const GstAV1SequenceHeaderOBU * seq_hdr,
                                                                           guint8 temporal_id,
                                                                           guint8 spatial_id,
                                                                           gboolean size_field,
                                                                           guint size_field_size,
                                                                           guint * qindex_offset,
                                                                           guint * segmentation_offset,
                                                                           guint * lf_offset,
                                                                           guint * cdef_offset,
                                                                           guint * cdef_size,
                                                                           guint8 * data,
                                                                           guint * size);
GST_CODEC_PARSERS_API
GstAV1BitWriterResult    gst_av1_bit_writer_temporal_delimiter_obu (gboolean size_field,
                                                                    guint8 * data,
                                                                    guint * size);
GST_CODEC_PARSERS_API
GstAV1BitWriterResult    gst_av1_bit_writer_metadata_obu        (const GstAV1MetadataOBU * metadata,
                                                                 guint8 temporal_id,
                                                                 guint8 spatial_id,
                                                                 gboolean size_field,
                                                                 guint8 * data,
                                                                 guint * size);
G_END_DECLS

#endif /* __GST_AV1_BIT_WRITER_H__ */
