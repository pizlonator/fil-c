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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "mpegts.h"
#include "gstmpegts-private.h"

#define GST_CAT_DEFAULT mpegts_debug

static GstMpegtsMetadataDescriptor
    * _gst_mpegts_metadata_descriptor_copy
    (GstMpegtsMetadataDescriptor * source)
{
  GstMpegtsMetadataDescriptor *copy =
      g_memdup2 (source, sizeof (GstMpegtsMetadataDescriptor));
  return copy;
}

static void
_gst_mpegts_metadata_descriptor_free (GstMpegtsMetadataDescriptor * desc)
{
  g_free (desc);
}

G_DEFINE_BOXED_TYPE (GstMpegtsMetadataDescriptor,
    gst_mpegts_metadata_descriptor,
    (GBoxedCopyFunc) _gst_mpegts_metadata_descriptor_copy,
    (GFreeFunc) _gst_mpegts_metadata_descriptor_free);

/**
 * gst_mpegts_descriptor_parse_metadata:
 * @descriptor: a %GST_TYPE_MPEGTS_METADATA_DESCRIPTOR #GstMpegtsDescriptor
 * @res: (out) (transfer full): #GstMpegtsMetadataDescriptor
 *
 * Parses out the metadata descriptor from the @descriptor.
 *
 * See ISO/IEC 13818-1:2018 Section 2.6.60 and 2.6.61 for details.
 * metadata_application_format is provided in Table 2-82. metadata_format is
 * provided in Table 2-85.
 *
 * Returns: %TRUE if the parsing worked correctly, else %FALSE.
 *
 * Since: 1.24
 */
gboolean
gst_mpegts_descriptor_parse_metadata (const GstMpegtsDescriptor * descriptor,
    GstMpegtsMetadataDescriptor ** desc)
{
  guint8 *data;
  guint8 flag;
  GstMpegtsMetadataDescriptor *res;

  g_return_val_if_fail (descriptor != NULL && desc != NULL, FALSE);
  __common_desc_checks (descriptor, GST_MTS_DESC_METADATA, 5, FALSE);

  data = (guint8 *) descriptor->data + 2;

  res = g_new0 (GstMpegtsMetadataDescriptor, 1);

  res->metadata_application_format = GST_READ_UINT16_BE (data);
  data += 2;
  if (res->metadata_application_format == 0xFFFF) {
    // skip over metadata_application_format_identifier if it is provided
    data += 4;
  }
  res->metadata_format = *data;
  data += 1;
  if (res->metadata_format == GST_MPEGTS_METADATA_FORMAT_IDENTIFIER_FIELD) {
    res->metadata_format_identifier = GST_READ_UINT32_BE (data);
    data += 4;
  }
  res->metadata_service_id = *data;
  data += 1;
  flag = *data;
  res->decoder_config_flags = flag >> 5;
  res->dsm_cc_flag = (flag & 0x10);

  // There are more values if the dsm_cc_flag or decoder flags are set.

  *desc = res;

  return TRUE;
}

/**
 * gst_mpegts_descriptor_parse_metadata_std:
 * @descriptor: a %GST_MTS_DESC_METADATA_STD #GstMpegtsDescriptor
 * @metadata_input_leak_rate (out): the input leak rate in units of 400bits/sec.
 * @metadata_buffer_size (out): the buffer size in units of 1024 bytes
 * @metadata_output_leak_rate (out): the output leak rate in units of 400bits/sec.
 *
 * Extracts the metadata STD descriptor from @descriptor.
 *
 * See ISO/IEC 13818-1:2018 Section 2.6.62 and 2.6.63 for details.
 *
 * Returns: %TRUE if parsing succeeded, else %FALSE.
 *
 * Since: 1.24
 */
gboolean
gst_mpegts_descriptor_parse_metadata_std (const GstMpegtsDescriptor *
    descriptor, guint32 * metadata_input_leak_rate,
    guint32 * metadata_buffer_size, guint32 * metadata_output_leak_rate)
{
  guint8 *data;

  g_return_val_if_fail (descriptor != NULL && metadata_input_leak_rate != NULL
      && metadata_buffer_size != NULL
      && metadata_output_leak_rate != NULL, FALSE);
  __common_desc_checks (descriptor, GST_MTS_DESC_METADATA_STD, 9, FALSE);
  data = (guint8 *) descriptor->data + 2;
  *metadata_input_leak_rate = GST_READ_UINT24_BE (data) & 0x3FFFFF;
  data += 3;
  *metadata_buffer_size = GST_READ_UINT24_BE (data) & 0x3FFFFF;
  data += 3;
  *metadata_output_leak_rate = GST_READ_UINT24_BE (data) & 0x3FFFFF;
  return TRUE;
}
