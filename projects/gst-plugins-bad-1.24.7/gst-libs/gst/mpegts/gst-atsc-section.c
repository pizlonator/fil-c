/*
 * Copyright (C) 2014 Stefan Ringel
 *
 * Authors:
 *   Stefan Ringel <linuxtv@stefanringel.de>
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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <stdlib.h>

#include "mpegts.h"
#include "gstmpegts-private.h"

/**
 * SECTION:gst-atsc-section
 * @title: ATSC variants of MPEG-TS sections
 * @short_description: Sections for the various ATSC specifications
 * @include: gst/mpegts/mpegts.h
 *
 * The list of section types defined and used by the ATSC specifications can be
 * seen in %GstMpegtsSectionATSCTableID.
 *
 * # Supported ATSC MPEG-TS sections
 * These are the sections for which parsing and packetizing code exists.
 *
 * ## Master Guide Table (MGT)
 * See:
 * * gst_mpegts_section_get_atsc_mgt()
 * * %GstMpegtsAtscMGT
 * * %GstMpegtsAtscMGTTable
 * * gst_mpegts_atsc_mgt_new()
 *
 * ## Terrestrial (TVCT) and Cable (CVCT) Virtual Channel Table
 * See:
 * * gst_mpegts_section_get_atsc_tvct()
 * * gst_mpegts_section_get_atsc_cvct()
 * * %GstMpegtsAtscVCT
 * * %GstMpegtsAtscVCTSource
 *
 * ## Rating Region Table (RRT)
 * See:
 * * gst_mpegts_section_get_atsc_rrt()
 * * %GstMpegtsAtscRRT
 * * gst_mpegts_atsc_rrt_new()
 * 
 * ## Event Information Table (EIT)
 * See:
 * * gst_mpegts_section_get_atsc_eit()
 * * %GstMpegtsAtscEIT
 * * %GstMpegtsAtscEITEvent
 *
 * ## Extended Text Table (ETT)
 * See:
 * * gst_mpegts_section_get_atsc_ett()
 * * %GstMpegtsAtscETT
 *
 * ## System Time Table (STT)
 * See:
 * * gst_mpegts_section_get_atsc_stt()
 * * %GstMpegtsAtscSTT
 * * gst_mpegts_atsc_stt_new()
 *
 * # API
 */

/* Terrestrial/Cable Virtual Channel Table TVCT/CVCT */
static GstMpegtsAtscVCTSource *
_gst_mpegts_atsc_vct_source_copy (GstMpegtsAtscVCTSource * source)
{
  GstMpegtsAtscVCTSource *copy;

  copy = g_memdup2 (source, sizeof (GstMpegtsAtscVCTSource));
  copy->descriptors = g_ptr_array_ref (source->descriptors);

  return copy;
}

static void
_gst_mpegts_atsc_vct_source_free (GstMpegtsAtscVCTSource * source)
{
  g_free (source->short_name);
  if (source->descriptors)
    g_ptr_array_unref (source->descriptors);
  g_free (source);
}

G_DEFINE_BOXED_TYPE (GstMpegtsAtscVCTSource, gst_mpegts_atsc_vct_source,
    (GBoxedCopyFunc) _gst_mpegts_atsc_vct_source_copy,
    (GFreeFunc) _gst_mpegts_atsc_vct_source_free);

static GstMpegtsAtscVCT *
_gst_mpegts_atsc_vct_copy (GstMpegtsAtscVCT * vct)
{
  GstMpegtsAtscVCT *copy;

  copy = g_memdup2 (vct, sizeof (GstMpegtsAtscVCT));
  copy->sources = g_ptr_array_ref (vct->sources);
  copy->descriptors = g_ptr_array_ref (vct->descriptors);

  return copy;
}

static void
_gst_mpegts_atsc_vct_free (GstMpegtsAtscVCT * vct)
{
  if (vct->sources)
    g_ptr_array_unref (vct->sources);
  if (vct->descriptors)
    g_ptr_array_unref (vct->descriptors);
  g_free (vct);
}

G_DEFINE_BOXED_TYPE (GstMpegtsAtscVCT, gst_mpegts_atsc_vct,
    (GBoxedCopyFunc) _gst_mpegts_atsc_vct_copy,
    (GFreeFunc) _gst_mpegts_atsc_vct_free);

static gpointer
_parse_atsc_vct (GstMpegtsSection * section)
{
  GstMpegtsAtscVCT *vct = NULL;
  guint8 *data, *end, source_nb;
  guint32 tmp32;
  guint16 descriptors_loop_length, tmp16;
  guint i;
  GError *err = NULL;

  vct = g_new0 (GstMpegtsAtscVCT, 1);

  data = section->data;
  end = data + section->section_length;

  vct->transport_stream_id = section->subtable_extension;

  /* Skip already parsed data */
  data += 8;

  /* minimum size */
  if (end - data < 2 + 2 + 4)
    goto error;

  vct->protocol_version = *data;
  data += 1;

  source_nb = *data;
  data += 1;

  vct->sources = g_ptr_array_new_full (source_nb,
      (GDestroyNotify) _gst_mpegts_atsc_vct_source_free);

  for (i = 0; i < source_nb; i++) {
    GstMpegtsAtscVCTSource *source;

    /* minimum 32 bytes for a entry, 2 bytes second descriptor
       loop-length, 4 bytes crc */
    if (end - data < 32 + 2 + 4)
      goto error;

    source = g_new0 (GstMpegtsAtscVCTSource, 1);
    g_ptr_array_add (vct->sources, source);

    source->short_name =
        g_convert ((gchar *) data, 14, "utf-8", "utf-16be", NULL, NULL, &err);
    if (err) {
      GST_WARNING ("Failed to convert VCT Source short_name to utf-8: %d %s",
          err->code, err->message);
      GST_MEMDUMP ("UTF-16 string", data, 14);
      g_error_free (err);
    }
    data += 14;

    tmp32 = GST_READ_UINT32_BE (data);
    source->major_channel_number = (tmp32 >> 18) & 0x03FF;
    source->minor_channel_number = (tmp32 >> 8) & 0x03FF;
    source->modulation_mode = tmp32 & 0xF;
    data += 4;

    source->carrier_frequency = GST_READ_UINT32_BE (data);
    data += 4;

    source->channel_TSID = GST_READ_UINT16_BE (data);
    data += 2;

    source->program_number = GST_READ_UINT16_BE (data);
    data += 2;

    tmp16 = GST_READ_UINT16_BE (data);
    source->ETM_location = (tmp16 >> 14) & 0x3;
    source->access_controlled = (tmp16 >> 13) & 0x1;
    source->hidden = (tmp16 >> 12) & 0x1;

    /* only used in CVCT */
    source->path_select = (tmp16 >> 11) & 0x1;
    source->out_of_band = (tmp16 >> 10) & 0x1;

    source->hide_guide = (tmp16 >> 9) & 0x1;
    source->service_type = tmp16 & 0x3f;
    data += 2;

    source->source_id = GST_READ_UINT16_BE (data);
    data += 2;

    descriptors_loop_length = GST_READ_UINT16_BE (data) & 0x03FF;
    data += 2;

    if (end - data < descriptors_loop_length + 6)
      goto error;

    source->descriptors =
        gst_mpegts_parse_descriptors (data, descriptors_loop_length);
    if (source->descriptors == NULL)
      goto error;
    data += descriptors_loop_length;
  }

  descriptors_loop_length = GST_READ_UINT16_BE (data) & 0x03FF;
  data += 2;

  if (end - data < descriptors_loop_length + 4)
    goto error;

  vct->descriptors =
      gst_mpegts_parse_descriptors (data, descriptors_loop_length);
  if (vct->descriptors == NULL)
    goto error;

  return (gpointer) vct;

error:
  _gst_mpegts_atsc_vct_free (vct);

  return NULL;
}

/**
 * gst_mpegts_section_get_atsc_tvct:
 * @section: a #GstMpegtsSection of type %GST_MPEGTS_SECTION_ATSC_TVCT
 *
 * Returns the #GstMpegtsAtscVCT contained in the @section
 *
 * Returns: The #GstMpegtsAtscVCT contained in the section, or %NULL if an error
 * happened.
 */
const GstMpegtsAtscVCT *
gst_mpegts_section_get_atsc_tvct (GstMpegtsSection * section)
{
  g_return_val_if_fail (section->section_type == GST_MPEGTS_SECTION_ATSC_TVCT,
      NULL);
  g_return_val_if_fail (section->cached_parsed || section->data, NULL);

  if (!section->cached_parsed)
    section->cached_parsed =
        __common_section_checks (section, 16, _parse_atsc_vct,
        (GDestroyNotify) _gst_mpegts_atsc_vct_free);

  return (const GstMpegtsAtscVCT *) section->cached_parsed;
}

/**
 * gst_mpegts_section_get_atsc_cvct:
 * @section: a #GstMpegtsSection of type %GST_MPEGTS_SECTION_ATSC_CVCT
 *
 * Returns the #GstMpegtsAtscVCT contained in the @section
 *
 * Returns: The #GstMpegtsAtscVCT contained in the section, or %NULL if an error
 * happened.
 */
const GstMpegtsAtscVCT *
gst_mpegts_section_get_atsc_cvct (GstMpegtsSection * section)
{
  g_return_val_if_fail (section->section_type == GST_MPEGTS_SECTION_ATSC_CVCT,
      NULL);
  g_return_val_if_fail (section->cached_parsed || section->data, NULL);

  if (!section->cached_parsed)
    section->cached_parsed =
        __common_section_checks (section, 16, _parse_atsc_vct,
        (GDestroyNotify) _gst_mpegts_atsc_vct_free);

  return (const GstMpegtsAtscVCT *) section->cached_parsed;
}

/* MGT */

static GstMpegtsAtscMGTTable *
_gst_mpegts_atsc_mgt_table_copy (GstMpegtsAtscMGTTable * mgt_table)
{
  GstMpegtsAtscMGTTable *copy;

  copy = g_memdup2 (mgt_table, sizeof (GstMpegtsAtscMGTTable));
  copy->descriptors = g_ptr_array_ref (mgt_table->descriptors);

  return copy;
}

static void
_gst_mpegts_atsc_mgt_table_free (GstMpegtsAtscMGTTable * mgt_table)
{
  g_ptr_array_unref (mgt_table->descriptors);
  g_free (mgt_table);
}

G_DEFINE_BOXED_TYPE (GstMpegtsAtscMGTTable, gst_mpegts_atsc_mgt_table,
    (GBoxedCopyFunc) _gst_mpegts_atsc_mgt_table_copy,
    (GFreeFunc) _gst_mpegts_atsc_mgt_table_free);

static GstMpegtsAtscMGT *
_gst_mpegts_atsc_mgt_copy (GstMpegtsAtscMGT * mgt)
{
  GstMpegtsAtscMGT *copy;

  copy = g_memdup2 (mgt, sizeof (GstMpegtsAtscMGT));
  copy->tables = g_ptr_array_ref (mgt->tables);
  copy->descriptors = g_ptr_array_ref (mgt->descriptors);

  return copy;
}

static void
_gst_mpegts_atsc_mgt_free (GstMpegtsAtscMGT * mgt)
{
  g_ptr_array_unref (mgt->tables);
  g_ptr_array_unref (mgt->descriptors);
  g_free (mgt);
}

G_DEFINE_BOXED_TYPE (GstMpegtsAtscMGT, gst_mpegts_atsc_mgt,
    (GBoxedCopyFunc) _gst_mpegts_atsc_mgt_copy,
    (GFreeFunc) _gst_mpegts_atsc_mgt_free);

static gpointer
_parse_atsc_mgt (GstMpegtsSection * section)
{
  GstMpegtsAtscMGT *mgt = NULL;
  guint i = 0;
  guint8 *data, *end;
  guint16 descriptors_loop_length;

  mgt = g_new0 (GstMpegtsAtscMGT, 1);

  data = section->data;
  end = data + section->section_length;

  /* Skip already parsed data */
  data += 8;

  mgt->protocol_version = GST_READ_UINT8 (data);
  data += 1;
  mgt->tables_defined = GST_READ_UINT16_BE (data);
  data += 2;
  mgt->tables = g_ptr_array_new_full (mgt->tables_defined,
      (GDestroyNotify) _gst_mpegts_atsc_mgt_table_free);
  for (i = 0; i < mgt->tables_defined && data + 11 < end; i++) {
    GstMpegtsAtscMGTTable *mgt_table;

    if (data + 11 >= end) {
      GST_WARNING ("MGT data too short to parse inner table num %d", i);
      goto error;
    }

    mgt_table = g_new0 (GstMpegtsAtscMGTTable, 1);
    g_ptr_array_add (mgt->tables, mgt_table);

    mgt_table->table_type = GST_READ_UINT16_BE (data);
    data += 2;
    mgt_table->pid = GST_READ_UINT16_BE (data) & 0x1FFF;
    data += 2;
    mgt_table->version_number = GST_READ_UINT8 (data) & 0x1F;
    data += 1;
    mgt_table->number_bytes = GST_READ_UINT32_BE (data);
    data += 4;
    descriptors_loop_length = GST_READ_UINT16_BE (data) & 0x0FFF;
    data += 2;

    if (data + descriptors_loop_length >= end) {
      GST_WARNING ("MGT data too short to parse inner table descriptors (table "
          "num %d", i);
      goto error;
    }
    mgt_table->descriptors =
        gst_mpegts_parse_descriptors (data, descriptors_loop_length);
    data += descriptors_loop_length;
  }

  descriptors_loop_length = GST_READ_UINT16_BE (data) & 0xFFF;
  data += 2;
  if (data + descriptors_loop_length >= end) {
    GST_WARNING ("MGT data too short to parse descriptors");
    goto error;
  }
  mgt->descriptors =
      gst_mpegts_parse_descriptors (data, descriptors_loop_length);

  return (gpointer) mgt;

error:
  _gst_mpegts_atsc_mgt_free (mgt);

  return NULL;
}

static gboolean
_packetize_mgt (GstMpegtsSection * section)
{
  const GstMpegtsAtscMGT *mgt;
  guint8 *pos, *data;
  gsize length;
  guint i, j;

  mgt = gst_mpegts_section_get_atsc_mgt (section);

  if (mgt == NULL)
    return FALSE;

  if (mgt->tables_defined != mgt->tables->len)
    return FALSE;

  /* 8 byte common section fields
   * 1 byte protocol version
   * 2 byte tables_defined
   * 2 byte reserved / descriptors_length
   * 4 byte CRC
   */
  length = 17;

  for (i = 0; i < mgt->tables->len; i++) {
    GstMpegtsAtscMGTTable *mgt_table = g_ptr_array_index (mgt->tables, 1);
    /* 2 byte table_type
     * 2 byte reserved / table_type_PID
     * 1 byte reserved / table_type_version_number
     * 4 byte number bytes
     * 2 byte reserved / table_type_descriptors_length
     */
    length += 11;

    if (mgt_table->descriptors) {
      for (j = 0; j < mgt_table->descriptors->len; j++) {
        GstMpegtsDescriptor *descriptor =
            g_ptr_array_index (mgt_table->descriptors, j);
        length += descriptor->length + 2;
      }
    }
  }

  if (mgt->descriptors) {
    for (i = 0; i < mgt->descriptors->len; i++) {
      GstMpegtsDescriptor *descriptor = g_ptr_array_index (mgt->descriptors, i);
      length += descriptor->length + 2;
    }
  }

  _packetize_common_section (section, length);

  data = section->data + 8;

  /* protocol_version - 8 bit */
  GST_WRITE_UINT8 (data, mgt->protocol_version);
  data += 1;

  /* tables_defined - 16 bit uimsbf */
  GST_WRITE_UINT16_BE (data, mgt->tables_defined);
  data += 2;

  for (i = 0; i < mgt->tables_defined; i++) {
    GstMpegtsAtscMGTTable *mgt_table = g_ptr_array_index (mgt->tables, 1);

    /* table_type - 16 bit uimsbf */
    GST_WRITE_UINT16_BE (data, mgt_table->table_type);
    data += 2;

    /* 3 bit reserved, 13 bit table_type_PID uimsbf */
    GST_WRITE_UINT16_BE (data, mgt_table->pid | 0xe000);
    data += 2;

    /* 3 bit reserved, 5 bit table_type_version_number uimsbf */
    GST_WRITE_UINT8 (data, mgt_table->version_number | 0xe0);
    data += 1;

    /* 4 bit reserved, 12 bit table_type_descriptor_length uimsbf */
    pos = data;
    *data++ = 0xF0;
    *data++ = 0x00;

    _packetize_descriptor_array (mgt_table->descriptors, &data);

    /* Go back and update the descriptor length */
    GST_WRITE_UINT16_BE (pos, (data - pos - 2) | 0xF000);
  }

  /* 4 bit reserved, 12 bit descriptor_length uimsbf */
  pos = data;
  *data++ = 0xF0;
  *data++ = 0x00;

  _packetize_descriptor_array (mgt->descriptors, &data);

  /* Go back and update the descriptor length */
  GST_WRITE_UINT16_BE (pos, (data - pos - 2) | 0xF000);

  return TRUE;
}

/**
 * gst_mpegts_section_from_atsc_mgt:
 * @mgt: (transfer full): a #GstMpegtsAtscMGT to create the #GstMpegtsSection from
 *
 * Returns: (transfer full): the #GstMpegtsSection
 * Since: 1.18
 */
GstMpegtsSection *
gst_mpegts_section_from_atsc_mgt (GstMpegtsAtscMGT * mgt)
{
  GstMpegtsSection *section;

  g_return_val_if_fail (mgt != NULL, NULL);

  section = _gst_mpegts_section_init (0x1ffb,
      GST_MTS_TABLE_ID_ATSC_MASTER_GUIDE);

  section->subtable_extension = 0x0000;
  section->cached_parsed = (gpointer) mgt;
  section->packetizer = _packetize_mgt;
  section->destroy_parsed = (GDestroyNotify) _gst_mpegts_atsc_mgt_free;

  return section;
}

/**
 * gst_mpegts_section_get_atsc_mgt:
 * @section: a #GstMpegtsSection of type %GST_MPEGTS_SECTION_ATSC_MGT
 *
 * Returns the #GstMpegtsAtscMGT contained in the @section.
 *
 * Returns: The #GstMpegtsAtscMGT contained in the section, or %NULL if an error
 * happened.
 */
const GstMpegtsAtscMGT *
gst_mpegts_section_get_atsc_mgt (GstMpegtsSection * section)
{
  g_return_val_if_fail (section->section_type == GST_MPEGTS_SECTION_ATSC_MGT,
      NULL);
  g_return_val_if_fail (section->cached_parsed || section->data, NULL);

  if (!section->cached_parsed)
    section->cached_parsed =
        __common_section_checks (section, 17, _parse_atsc_mgt,
        (GDestroyNotify) _gst_mpegts_atsc_mgt_free);

  return (const GstMpegtsAtscMGT *) section->cached_parsed;
}

/**
 * gst_mpegts_section_atsc_mgt_new:
 *
 * Returns: (transfer full): #GstMpegtsAtscMGT
 * Since: 1.18
 */
GstMpegtsAtscMGT *
gst_mpegts_atsc_mgt_new (void)
{
  GstMpegtsAtscMGT *mgt;

  mgt = g_new0 (GstMpegtsAtscMGT, 1);

  mgt->tables = g_ptr_array_new_with_free_func ((GDestroyNotify)
      _gst_mpegts_atsc_mgt_table_free);

  mgt->descriptors = g_ptr_array_new_with_free_func ((GDestroyNotify)
      gst_mpegts_descriptor_free);

  return mgt;
}

/* Multi string structure */

static GstMpegtsAtscStringSegment *
_gst_mpegts_atsc_string_segment_copy (GstMpegtsAtscStringSegment * seg)
{
  GstMpegtsAtscStringSegment *copy;

  copy = g_memdup2 (seg, sizeof (GstMpegtsAtscStringSegment));

  return copy;
}

static void
_gst_mpegts_atsc_string_segment_free (GstMpegtsAtscStringSegment * seg)
{
  g_free (seg->cached_string);
  g_free (seg);
}

static void
_gst_mpegts_atsc_string_segment_decode_string (GstMpegtsAtscStringSegment * seg)
{
  const gchar *from_encoding;

  g_return_if_fail (seg->cached_string == NULL);

  if (seg->compression_type != 0) {
    GST_FIXME ("Compressed strings not yet supported");
    return;
  }

  /* FIXME add more encodings */
  switch (seg->mode) {
    case 0x3F:
      from_encoding = "UTF-16BE";
      break;
    default:
      from_encoding = NULL;
      break;
  }

  if (from_encoding != NULL && seg->compressed_data_size > 0) {
    GError *err = NULL;

    seg->cached_string =
        g_convert ((gchar *) seg->compressed_data,
        (gssize) seg->compressed_data_size, "UTF-8", from_encoding, NULL, NULL,
        &err);

    if (err) {
      GST_WARNING ("Failed to convert input string from codeset %s",
          from_encoding);
      g_error_free (err);
    }
  } else {
    seg->cached_string =
        g_strndup ((gchar *) seg->compressed_data, seg->compressed_data_size);
  }
}

const gchar *
gst_mpegts_atsc_string_segment_get_string (GstMpegtsAtscStringSegment * seg)
{
  if (!seg->cached_string)
    _gst_mpegts_atsc_string_segment_decode_string (seg);

  return seg->cached_string;
}

gboolean
gst_mpegts_atsc_string_segment_set_string (GstMpegtsAtscStringSegment * seg,
    gchar * string, guint8 compression_type, guint8 mode)
{
  const gchar *to_encoding = NULL;
  gboolean ret = FALSE;
  gsize written;
  GError *err = NULL;
  unsigned long len;

  if (compression_type) {
    GST_FIXME ("Compressed strings not yet supported");
    goto done;
  }

  switch (mode) {
    case 0x3f:
      to_encoding = "UTF-16BE";
      break;
    default:
      break;
  }

  if (seg->cached_string)
    g_free (seg->cached_string);

  if (seg->compressed_data)
    g_free (seg->compressed_data);

  seg->cached_string = g_strdup (string);
  seg->compression_type = compression_type;
  seg->mode = mode;

  len = strlen (string);

  if (to_encoding && len) {
    gchar *converted = g_convert (string, len, to_encoding, "UTF-8",
        NULL, &written, &err);

    if (err) {
      GST_WARNING ("Failed to convert input string to codeset %s (%s)",
          to_encoding, err->message);
      g_error_free (err);
      goto done;
    }

    seg->compressed_data = (guint8 *) g_strndup (converted, written);
    seg->compressed_data_size = written;
    g_free (converted);
  } else {
    seg->compressed_data = (guint8 *) g_strndup (string, len);
    seg->compressed_data_size = len;
  }

  ret = TRUE;

done:
  return ret;
}

G_DEFINE_BOXED_TYPE (GstMpegtsAtscStringSegment, gst_mpegts_atsc_string_segment,
    (GBoxedCopyFunc) _gst_mpegts_atsc_string_segment_copy,
    (GFreeFunc) _gst_mpegts_atsc_string_segment_free);

static GstMpegtsAtscMultString *
_gst_mpegts_atsc_mult_string_copy (GstMpegtsAtscMultString * mstring)
{
  GstMpegtsAtscMultString *copy;

  copy = g_memdup2 (mstring, sizeof (GstMpegtsAtscMultString));
  copy->segments = g_ptr_array_ref (mstring->segments);

  return copy;
}

static void
_gst_mpegts_atsc_mult_string_free (GstMpegtsAtscMultString * mstring)
{
  g_ptr_array_unref (mstring->segments);
  g_free (mstring);
}

G_DEFINE_BOXED_TYPE (GstMpegtsAtscMultString, gst_mpegts_atsc_mult_string,
    (GBoxedCopyFunc) _gst_mpegts_atsc_mult_string_copy,
    (GFreeFunc) _gst_mpegts_atsc_mult_string_free);

static GPtrArray *
_parse_atsc_mult_string (guint8 * data, guint datasize)
{
  guint8 num_strings;
  GPtrArray *res = NULL;
  guint8 *end = data + datasize;
  gint i;

  if (datasize > 0) {
    /* 1 is the minimum entry size, so no need to check here */
    num_strings = GST_READ_UINT8 (data);
    data += 1;

    res =
        g_ptr_array_new_full (num_strings,
        (GDestroyNotify) _gst_mpegts_atsc_mult_string_free);

    for (i = 0; i < num_strings; i++) {
      GstMpegtsAtscMultString *mstring;
      guint8 num_segments;
      gint j;

      mstring = g_new0 (GstMpegtsAtscMultString, 1);
      g_ptr_array_add (res, mstring);
      mstring->segments =
          g_ptr_array_new_full (num_strings,
          (GDestroyNotify) _gst_mpegts_atsc_string_segment_free);

      /* each entry needs at least 4 bytes (lang code and segments number) */
      if (end - data < 4) {
        GST_WARNING ("Data too short for multstring parsing %d",
            (gint) (end - data));
        goto error;
      }

      mstring->iso_639_langcode[0] = GST_READ_UINT8 (data);
      data += 1;
      mstring->iso_639_langcode[1] = GST_READ_UINT8 (data);
      data += 1;
      mstring->iso_639_langcode[2] = GST_READ_UINT8 (data);
      data += 1;
      num_segments = GST_READ_UINT8 (data);
      data += 1;

      for (j = 0; j < num_segments; j++) {
        GstMpegtsAtscStringSegment *seg;

        seg = g_new0 (GstMpegtsAtscStringSegment, 1);
        g_ptr_array_add (mstring->segments, seg);

        /* each entry needs at least 3 bytes */
        if (end - data < 3) {
          GST_WARNING ("Data too short for multstring parsing %d", datasize);
          goto error;
        }

        seg->compression_type = GST_READ_UINT8 (data);
        data += 1;
        seg->mode = GST_READ_UINT8 (data);
        data += 1;
        seg->compressed_data_size = GST_READ_UINT8 (data);
        data += 1;

        if (end - data < seg->compressed_data_size) {
          GST_WARNING ("Data too short for multstring parsing %d", datasize);
          goto error;
        }

        if (seg->compressed_data_size)
          seg->compressed_data = data;
        data += seg->compressed_data_size;
      }

    }
  }
  return res;

error:
  if (res)
    g_ptr_array_unref (res);
  return NULL;
}

static void
_packetize_atsc_mult_string (GPtrArray * strings, guint8 ** data)
{
  guint i;

  if (strings == NULL)
    return;

  /* 8 bit number_strings */
  GST_WRITE_UINT8 (*data, strings->len);
  *data += 1;

  for (i = 0; i < strings->len; i++) {
    GstMpegtsAtscMultString *string;
    guint j;

    string = g_ptr_array_index (strings, i);

    /* 24 bit ISO_639_langcode */
    GST_WRITE_UINT8 (*data, string->iso_639_langcode[0]);
    *data += 1;
    GST_WRITE_UINT8 (*data, string->iso_639_langcode[1]);
    *data += 1;
    GST_WRITE_UINT8 (*data, string->iso_639_langcode[2]);
    *data += 1;
    /* 8 bit number_segments */
    GST_WRITE_UINT8 (*data, string->segments->len);
    *data += 1;

    for (j = 0; j < string->segments->len; j++) {
      GstMpegtsAtscStringSegment *seg;

      seg = g_ptr_array_index (string->segments, j);

      /* 8 bit compression_type */
      GST_WRITE_UINT8 (*data, seg->compression_type);
      *data += 1;
      /* 8 bit mode */
      GST_WRITE_UINT8 (*data, seg->mode);
      *data += 1;
      /* 8 bit number_bytes */
      GST_WRITE_UINT8 (*data, seg->compressed_data_size);
      *data += 1;
      /* number_bytes compressed string */
      memcpy (*data, seg->compressed_data, seg->compressed_data_size);
      *data += seg->compressed_data_size;
    }
  }
}

static gsize
_get_atsc_mult_string_packetized_length (GPtrArray * strings)
{
  gsize length = 1;
  guint i;

  for (i = 0; i < strings->len; i++) {
    GstMpegtsAtscMultString *string;
    guint j;

    string = g_ptr_array_index (strings, i);

    length += 4;

    for (j = 0; j < string->segments->len; j++) {
      GstMpegtsAtscStringSegment *seg;

      seg = g_ptr_array_index (string->segments, j);

      length += 3 + seg->compressed_data_size;
    }
  }

  return length;
}

/* EIT */

static GstMpegtsAtscEITEvent *
_gst_mpegts_atsc_eit_event_copy (GstMpegtsAtscEITEvent * event)
{
  GstMpegtsAtscEITEvent *copy;

  copy = g_memdup2 (event, sizeof (GstMpegtsAtscEITEvent));
  copy->titles = g_ptr_array_ref (event->titles);
  copy->descriptors = g_ptr_array_ref (event->descriptors);

  return copy;
}

static void
_gst_mpegts_atsc_eit_event_free (GstMpegtsAtscEITEvent * event)
{
  if (event->titles)
    g_ptr_array_unref (event->titles);
  if (event->descriptors)
    g_ptr_array_unref (event->descriptors);
  g_free (event);
}

G_DEFINE_BOXED_TYPE (GstMpegtsAtscEITEvent, gst_mpegts_atsc_eit_event,
    (GBoxedCopyFunc) _gst_mpegts_atsc_eit_event_copy,
    (GFreeFunc) _gst_mpegts_atsc_eit_event_free);

static GstMpegtsAtscEIT *
_gst_mpegts_atsc_eit_copy (GstMpegtsAtscEIT * eit)
{
  GstMpegtsAtscEIT *copy;

  copy = g_memdup2 (eit, sizeof (GstMpegtsAtscEIT));
  copy->events = g_ptr_array_ref (eit->events);

  return copy;
}

static void
_gst_mpegts_atsc_eit_free (GstMpegtsAtscEIT * eit)
{
  if (eit->events)
    g_ptr_array_unref (eit->events);
  g_free (eit);
}

G_DEFINE_BOXED_TYPE (GstMpegtsAtscEIT, gst_mpegts_atsc_eit,
    (GBoxedCopyFunc) _gst_mpegts_atsc_eit_copy,
    (GFreeFunc) _gst_mpegts_atsc_eit_free);

static gpointer
_parse_atsc_eit (GstMpegtsSection * section)
{
  GstMpegtsAtscEIT *eit = NULL;
  guint i = 0;
  guint8 *data, *end;
  guint8 num_events;

  eit = g_new0 (GstMpegtsAtscEIT, 1);

  data = section->data;
  end = data + section->section_length;

  eit->source_id = section->subtable_extension;

  /* Skip already parsed data */
  data += 8;

  eit->protocol_version = GST_READ_UINT8 (data);
  data += 1;
  num_events = GST_READ_UINT8 (data);
  data += 1;

  eit->events = g_ptr_array_new_with_free_func ((GDestroyNotify)
      _gst_mpegts_atsc_eit_event_free);

  for (i = 0; i < num_events; i++) {
    GstMpegtsAtscEITEvent *event;
    guint32 tmp;
    guint8 text_length;
    guint16 descriptors_loop_length;

    if (end - data < 12) {
      GST_WARNING ("PID %d invalid EIT entry length %d with %u events",
          section->pid, (gint) (end - 4 - data), num_events);
      goto error;
    }

    event = g_new0 (GstMpegtsAtscEITEvent, 1);
    g_ptr_array_add (eit->events, event);

    event->event_id = GST_READ_UINT16_BE (data) & 0x3FFF;
    data += 2;
    event->start_time = GST_READ_UINT32_BE (data);
    data += 4;

    tmp = GST_READ_UINT32_BE (data);
    data += 4;
    event->etm_location = (tmp >> 28) & 0x3;
    event->length_in_seconds = (tmp >> 8) & 0x0FFFFF;
    text_length = tmp & 0xFF;

    if (text_length > end - data - 4 - 2) {
      GST_WARNING ("PID %d invalid EIT entry length %d with %u events",
          section->pid, (gint) (end - 4 - data), num_events);
      goto error;
    }
    event->titles = _parse_atsc_mult_string (data, text_length);
    data += text_length;

    descriptors_loop_length = GST_READ_UINT16_BE (data) & 0x0FFF;
    data += 2;

    if (end - data - 4 < descriptors_loop_length) {
      GST_WARNING ("PID %d invalid EIT entry length %d with %u events",
          section->pid, (gint) (end - 4 - data), num_events);
      goto error;
    }

    event->descriptors =
        gst_mpegts_parse_descriptors (data, descriptors_loop_length);
    data += descriptors_loop_length;
  }

  if (data != end - 4) {
    GST_WARNING ("PID %d invalid EIT parsed %d length %d",
        section->pid, (gint) (data - section->data), section->section_length);
    goto error;
  }

  return (gpointer) eit;

error:
  _gst_mpegts_atsc_eit_free (eit);

  return NULL;

}

/**
 * gst_mpegts_section_get_atsc_eit:
 * @section: a #GstMpegtsSection of type %GST_MPEGTS_SECTION_ATSC_EIT
 *
 * Returns the #GstMpegtsAtscEIT contained in the @section.
 *
 * Returns: The #GstMpegtsAtscEIT contained in the section, or %NULL if an error
 * happened.
 */
const GstMpegtsAtscEIT *
gst_mpegts_section_get_atsc_eit (GstMpegtsSection * section)
{
  g_return_val_if_fail (section->section_type == GST_MPEGTS_SECTION_ATSC_EIT,
      NULL);
  g_return_val_if_fail (section->cached_parsed || section->data, NULL);

  if (!section->cached_parsed)
    section->cached_parsed =
        __common_section_checks (section, 14, _parse_atsc_eit,
        (GDestroyNotify) _gst_mpegts_atsc_eit_free);

  return (const GstMpegtsAtscEIT *) section->cached_parsed;
}


static GstMpegtsAtscETT *
_gst_mpegts_atsc_ett_copy (GstMpegtsAtscETT * ett)
{
  GstMpegtsAtscETT *copy;

  copy = g_memdup2 (ett, sizeof (GstMpegtsAtscETT));
  copy->messages = g_ptr_array_ref (ett->messages);

  return copy;
}

static void
_gst_mpegts_atsc_ett_free (GstMpegtsAtscETT * ett)
{
  if (ett->messages)
    g_ptr_array_unref (ett->messages);
  g_free (ett);
}

G_DEFINE_BOXED_TYPE (GstMpegtsAtscETT, gst_mpegts_atsc_ett,
    (GBoxedCopyFunc) _gst_mpegts_atsc_ett_copy,
    (GFreeFunc) _gst_mpegts_atsc_ett_free);

static gpointer
_parse_ett (GstMpegtsSection * section)
{
  GstMpegtsAtscETT *ett = NULL;
  guint8 *data, *end;

  ett = g_new0 (GstMpegtsAtscETT, 1);

  data = section->data;
  end = data + section->section_length;

  ett->ett_table_id_extension = section->subtable_extension;

  /* Skip already parsed data */
  data += 8;

  ett->protocol_version = GST_READ_UINT8 (data);
  data += 1;
  ett->etm_id = GST_READ_UINT32_BE (data);
  data += 4;

  ett->messages = _parse_atsc_mult_string (data, end - data - 4);
  data += end - data - 4;

  if (data != end - 4) {
    GST_WARNING ("PID %d invalid ETT parsed %d length %d",
        section->pid, (gint) (data - section->data), section->section_length);
    goto error;
  }

  return (gpointer) ett;

error:
  _gst_mpegts_atsc_ett_free (ett);

  return NULL;

}

/**
 * gst_mpegts_section_get_atsc_ett:
 * @section: a #GstMpegtsSection of type %GST_MPEGTS_SECTION_ATSC_ETT
 *
 * Returns the #GstMpegtsAtscETT contained in the @section.
 *
 * Returns: The #GstMpegtsAtscETT contained in the section, or %NULL if an error
 * happened.
 */
const GstMpegtsAtscETT *
gst_mpegts_section_get_atsc_ett (GstMpegtsSection * section)
{
  g_return_val_if_fail (section->section_type == GST_MPEGTS_SECTION_ATSC_ETT,
      NULL);
  g_return_val_if_fail (section->cached_parsed || section->data, NULL);

  if (!section->cached_parsed)
    section->cached_parsed = __common_section_checks (section, 17, _parse_ett,
        (GDestroyNotify) _gst_mpegts_atsc_ett_free);

  return (const GstMpegtsAtscETT *) section->cached_parsed;
}

/* STT */

static GstMpegtsAtscSTT *
_gst_mpegts_atsc_stt_copy (GstMpegtsAtscSTT * stt)
{
  GstMpegtsAtscSTT *copy;

  copy = g_memdup2 (stt, sizeof (GstMpegtsAtscSTT));
  copy->descriptors = g_ptr_array_ref (stt->descriptors);

  return copy;
}

static void
_gst_mpegts_atsc_stt_free (GstMpegtsAtscSTT * stt)
{
  if (stt->descriptors)
    g_ptr_array_unref (stt->descriptors);
  if (stt->utc_datetime)
    gst_date_time_unref (stt->utc_datetime);

  g_free (stt);
}

G_DEFINE_BOXED_TYPE (GstMpegtsAtscSTT, gst_mpegts_atsc_stt,
    (GBoxedCopyFunc) _gst_mpegts_atsc_stt_copy,
    (GFreeFunc) _gst_mpegts_atsc_stt_free);

static gpointer
_parse_atsc_stt (GstMpegtsSection * section)
{
  GstMpegtsAtscSTT *stt = NULL;
  guint8 *data, *end;
  guint16 daylight_saving;

  stt = g_new0 (GstMpegtsAtscSTT, 1);

  data = section->data;
  end = data + section->section_length;

  /* Skip already parsed data */
  data += 8;

  stt->protocol_version = GST_READ_UINT8 (data);
  data += 1;
  stt->system_time = GST_READ_UINT32_BE (data);
  data += 4;
  stt->gps_utc_offset = GST_READ_UINT8 (data);
  data += 1;

  daylight_saving = GST_READ_UINT16_BE (data);
  data += 2;
  stt->ds_status = daylight_saving >> 15;
  stt->ds_dayofmonth = (daylight_saving >> 8) & 0x1F;
  stt->ds_hour = daylight_saving & 0xFF;

  stt->descriptors = gst_mpegts_parse_descriptors (data, end - data - 4);
  if (stt->descriptors == NULL)
    goto error;

  return (gpointer) stt;

error:
  _gst_mpegts_atsc_stt_free (stt);

  return NULL;
}

static gboolean
_packetize_stt (GstMpegtsSection * section)
{
  const GstMpegtsAtscSTT *stt;
  guint8 *data;
  gsize length;
  guint i;

  stt = gst_mpegts_section_get_atsc_stt (section);

  if (stt == NULL)
    return FALSE;

  /* 8 byte common section fields
   * 1 byte protocol version
   * 4 byte system time
   * 1 byte GPS_UTC_offset
   * 2 byte daylight saving
   * 4 byte CRC
   */
  length = 20;

  if (stt->descriptors) {
    for (i = 0; i < stt->descriptors->len; i++) {
      GstMpegtsDescriptor *descriptor = g_ptr_array_index (stt->descriptors, i);
      length += descriptor->length + 2;
    }
  }

  _packetize_common_section (section, length);

  data = section->data + 8;

  /* protocol_version - 8 bit */
  GST_WRITE_UINT8 (data, stt->protocol_version);
  data += 1;
  /* system time - 32 bit uimsbf */
  GST_WRITE_UINT32_BE (data, stt->system_time);
  data += 4;
  /* GPS_UTC_offset - 8 bit */
  GST_WRITE_UINT8 (data, stt->gps_utc_offset);
  data += 1;
  /* daylight_saving - 16 bit uimsbf */
  GST_WRITE_UINT8 (data,
      (stt->ds_status << 7) | 0x60 | (stt->ds_dayofmonth & 0x1f));
  data += 1;
  GST_WRITE_UINT8 (data, stt->ds_hour);
  data += 1;

  _packetize_descriptor_array (stt->descriptors, &data);

  return TRUE;
}

/**
 * gst_mpegts_section_section_from_atsc_stt:
 * @stt: (transfer full): a #GstMpegtsAtscSTT to create the #GstMpegtsSection from
 *
 * Returns: (transfer full): the #GstMpegtsSection
 * Since: 1.18
 */
GstMpegtsSection *
gst_mpegts_section_from_atsc_stt (GstMpegtsAtscSTT * stt)
{
  GstMpegtsSection *section;

  g_return_val_if_fail (stt != NULL, NULL);

  section = _gst_mpegts_section_init (0x1ffb,
      GST_MTS_TABLE_ID_ATSC_SYSTEM_TIME);

  section->subtable_extension = 0x0000;
  section->cached_parsed = (gpointer) stt;
  section->packetizer = _packetize_stt;
  section->destroy_parsed = (GDestroyNotify) _gst_mpegts_atsc_stt_free;

  return section;
}

/**
 * gst_mpegts_section_get_atsc_stt:
 * @section: a #GstMpegtsSection of type %GST_MPEGTS_SECTION_ATSC_STT
 *
 * Returns the #GstMpegtsAtscSTT contained in the @section.
 *
 * Returns: The #GstMpegtsAtscSTT contained in the section, or %NULL if an error
 * happened.
 */
const GstMpegtsAtscSTT *
gst_mpegts_section_get_atsc_stt (GstMpegtsSection * section)
{
  g_return_val_if_fail (section->section_type == GST_MPEGTS_SECTION_ATSC_STT,
      NULL);
  g_return_val_if_fail (section->cached_parsed || section->data, NULL);

  if (!section->cached_parsed)
    section->cached_parsed =
        __common_section_checks (section, 20, _parse_atsc_stt,
        (GDestroyNotify) _gst_mpegts_atsc_stt_free);

  return (const GstMpegtsAtscSTT *) section->cached_parsed;
}

/**
 * gst_mpegts_section_atsc_stt_new:
 *
 * Returns: (transfer full): #GstMpegtsAtscSTT
 * Since: 1.18
 */
GstMpegtsAtscSTT *
gst_mpegts_atsc_stt_new (void)
{
  GstMpegtsAtscSTT *stt;

  stt = g_new0 (GstMpegtsAtscSTT, 1);
  stt->descriptors = g_ptr_array_new_with_free_func ((GDestroyNotify)
      gst_mpegts_descriptor_free);

  return stt;
}

#define GPS_TO_UTC_TICKS G_GINT64_CONSTANT(315964800)
static GstDateTime *
_gst_mpegts_atsc_gps_time_to_datetime (guint32 systemtime, guint8 gps_offset)
{
  return gst_date_time_new_from_unix_epoch_utc (systemtime - gps_offset +
      GPS_TO_UTC_TICKS);
}

GstDateTime *
gst_mpegts_atsc_stt_get_datetime_utc (GstMpegtsAtscSTT * stt)
{
  if (stt->utc_datetime == NULL)
    stt->utc_datetime = _gst_mpegts_atsc_gps_time_to_datetime (stt->system_time,
        stt->gps_utc_offset);

  if (stt->utc_datetime)
    return gst_date_time_ref (stt->utc_datetime);
  return NULL;
}

/* RRT */

static GstMpegtsAtscRRTDimensionValue *
_gst_mpegts_atsc_rrt_dimension_value_copy (GstMpegtsAtscRRTDimensionValue *
    value)
{
  GstMpegtsAtscRRTDimensionValue *copy;

  copy = g_memdup2 (value, sizeof (GstMpegtsAtscRRTDimensionValue));
  copy->abbrev_ratings = g_ptr_array_ref (value->abbrev_ratings);
  copy->ratings = g_ptr_array_ref (value->ratings);

  return copy;
}

static void
_gst_mpegts_atsc_rrt_dimension_value_free (GstMpegtsAtscRRTDimensionValue *
    value)
{
  if (value->abbrev_ratings)
    g_ptr_array_unref (value->abbrev_ratings);
  if (value->ratings)
    g_ptr_array_unref (value->ratings);

  g_free (value);
}

G_DEFINE_BOXED_TYPE (GstMpegtsAtscRRTDimensionValue,
    gst_mpegts_atsc_rrt_dimension_value,
    (GBoxedCopyFunc) _gst_mpegts_atsc_rrt_dimension_value_copy,
    (GFreeFunc) _gst_mpegts_atsc_rrt_dimension_value_free);

static GstMpegtsAtscRRTDimension *
_gst_mpegts_atsc_rrt_dimension_copy (GstMpegtsAtscRRTDimension * dim)
{
  GstMpegtsAtscRRTDimension *copy;

  copy = g_memdup2 (dim, sizeof (GstMpegtsAtscRRTDimension));
  copy->names = g_ptr_array_ref (dim->names);
  copy->values = g_ptr_array_ref (dim->values);

  return copy;
}

static void
_gst_mpegts_atsc_rrt_dimension_free (GstMpegtsAtscRRTDimension * dim)
{
  if (dim->names)
    g_ptr_array_unref (dim->names);
  if (dim->values)
    g_ptr_array_unref (dim->values);

  g_free (dim);
}

G_DEFINE_BOXED_TYPE (GstMpegtsAtscRRTDimension, gst_mpegts_atsc_rrt_dimension,
    (GBoxedCopyFunc) _gst_mpegts_atsc_rrt_dimension_copy,
    (GFreeFunc) _gst_mpegts_atsc_rrt_dimension_free);

static GstMpegtsAtscRRT *
_gst_mpegts_atsc_rrt_copy (GstMpegtsAtscRRT * rrt)
{
  GstMpegtsAtscRRT *copy;

  copy = g_memdup2 (rrt, sizeof (GstMpegtsAtscRRT));
  copy->names = g_ptr_array_ref (rrt->names);
  copy->dimensions = g_ptr_array_ref (rrt->dimensions);
  copy->descriptors = g_ptr_array_ref (rrt->descriptors);

  return copy;
}

static void
_gst_mpegts_atsc_rrt_free (GstMpegtsAtscRRT * rrt)
{
  if (rrt->names)
    g_ptr_array_unref (rrt->names);
  if (rrt->dimensions)
    g_ptr_array_unref (rrt->dimensions);
  if (rrt->descriptors)
    g_ptr_array_unref (rrt->descriptors);

  g_free (rrt);
}

G_DEFINE_BOXED_TYPE (GstMpegtsAtscRRT, gst_mpegts_atsc_rrt,
    (GBoxedCopyFunc) _gst_mpegts_atsc_rrt_copy,
    (GFreeFunc) _gst_mpegts_atsc_rrt_free);

static gpointer
_parse_rrt (GstMpegtsSection * section)
{
  GstMpegtsAtscRRT *rrt = NULL;
  guint i = 0;
  guint8 *data;
  guint16 descriptors_loop_length;
  guint8 text_length;

  rrt = g_new0 (GstMpegtsAtscRRT, 1);

  data = section->data;

  /* Skip already parsed data */
  data += 8;

  rrt->protocol_version = GST_READ_UINT8 (data);
  data += 1;

  text_length = GST_READ_UINT8 (data);
  data += 1;
  rrt->names = _parse_atsc_mult_string (data, text_length);
  data += text_length;

  rrt->dimensions_defined = GST_READ_UINT8 (data);
  data += 1;

  rrt->dimensions = g_ptr_array_new_full (rrt->dimensions_defined,
      (GDestroyNotify) _gst_mpegts_atsc_rrt_dimension_free);

  for (i = 0; i < rrt->dimensions_defined; i++) {
    GstMpegtsAtscRRTDimension *dim;
    guint8 tmp;
    guint j = 0;

    dim = g_new0 (GstMpegtsAtscRRTDimension, 1);
    g_ptr_array_add (rrt->dimensions, dim);

    text_length = GST_READ_UINT8 (data);
    data += 1;
    dim->names = _parse_atsc_mult_string (data, text_length);
    data += text_length;

    tmp = GST_READ_UINT8 (data);
    data += 1;

    dim->graduated_scale = tmp & 0x10;
    dim->values_defined = tmp & 0x0f;

    dim->values = g_ptr_array_new_full (dim->values_defined,
        (GDestroyNotify) _gst_mpegts_atsc_rrt_dimension_value_free);

    for (j = 0; j < dim->values_defined; j++) {
      GstMpegtsAtscRRTDimensionValue *val;

      val = g_new0 (GstMpegtsAtscRRTDimensionValue, 1);
      g_ptr_array_add (dim->values, val);

      text_length = GST_READ_UINT8 (data);
      data += 1;
      val->abbrev_ratings = _parse_atsc_mult_string (data, text_length);
      data += text_length;

      text_length = GST_READ_UINT8 (data);
      data += 1;
      val->ratings = _parse_atsc_mult_string (data, text_length);
      data += text_length;
    }
  }

  descriptors_loop_length = GST_READ_UINT16_BE (data) & 0x3FF;
  data += 2;
  rrt->descriptors =
      gst_mpegts_parse_descriptors (data, descriptors_loop_length);

  return (gpointer) rrt;
}

static gboolean
_packetize_rrt (GstMpegtsSection * section)
{
  const GstMpegtsAtscRRT *rrt;
  guint8 *data, *pos;
  gsize length;
  gsize names_length;
  guint i, j;

  rrt = gst_mpegts_section_get_atsc_rrt (section);

  if (rrt == NULL)
    return FALSE;

  names_length = _get_atsc_mult_string_packetized_length (rrt->names);

  /* 8 byte common section fields
   * 1 byte protocol version
   * 1 byte rating_region_name_length
   * name_length bytes
   * 1 byte dimensions_defined
   * 2 byte reserved / descriptors_length
   * 4 byte CRC
   */
  length = names_length + 17;

  for (i = 0; i < rrt->dimensions->len; i++) {
    GstMpegtsAtscRRTDimension *dim = g_ptr_array_index (rrt->dimensions, i);

    /* 1 byte dimension_name_length
     * 1 byte reserved / graduated_scale / values_defined
     */
    length += 2;
    length += _get_atsc_mult_string_packetized_length (dim->names);
    for (j = 0; j < dim->values->len; j++) {
      GstMpegtsAtscRRTDimensionValue *val = g_ptr_array_index (dim->values, j);

      /* 1 byte abbrev_rating_value_length
       * 1 byte rating_value_length
       */
      length += 2;
      length += _get_atsc_mult_string_packetized_length (val->abbrev_ratings);
      length += _get_atsc_mult_string_packetized_length (val->ratings);
    }
  }

  if (rrt->descriptors) {
    for (i = 0; i < rrt->descriptors->len; i++) {
      GstMpegtsDescriptor *descriptor = g_ptr_array_index (rrt->descriptors, i);
      length += descriptor->length + 2;
    }
  }

  if (length > 1024) {
    GST_WARNING ("RRT size can not exceed 1024");
    return FALSE;
  }

  _packetize_common_section (section, length);

  data = section->data + 8;

  /* protocol_version - 8 bit */
  GST_WRITE_UINT8 (data, rrt->protocol_version);
  data += 1;

  /* rating_region_name_length - 8 bit */
  GST_WRITE_UINT8 (data, names_length);
  data += 1;

  _packetize_atsc_mult_string (rrt->names, &data);

  for (i = 0; i < rrt->dimensions->len; i++) {
    GstMpegtsAtscRRTDimension *dim = g_ptr_array_index (rrt->dimensions, i);

    /* dimension_name_length - 8 bit */
    GST_WRITE_UINT8 (data,
        _get_atsc_mult_string_packetized_length (dim->names));
    data += 1;

    _packetize_atsc_mult_string (rrt->names, &data);

    /* 3 bit reserved / 1 bit graduated_scale / 4 bit values_defined */
    GST_WRITE_UINT8 (data,
        0xe0 | ((dim->graduated_scale ? 1 : 0) << 4) | (dim->
            values_defined & 0x0f));
    data += 1;

    for (j = 0; j < dim->values->len; j++) {
      GstMpegtsAtscRRTDimensionValue *val = g_ptr_array_index (dim->values, j);

      /* abbrev_rating_value_length - 8 bit */
      GST_WRITE_UINT8 (data,
          _get_atsc_mult_string_packetized_length (val->abbrev_ratings));
      data += 1;

      _packetize_atsc_mult_string (val->abbrev_ratings, &data);

      /* rating_value_length - 8 bit */
      GST_WRITE_UINT8 (data,
          _get_atsc_mult_string_packetized_length (val->ratings));
      data += 1;

      _packetize_atsc_mult_string (val->ratings, &data);
    }
  }

  /* 6 bit reserved, 10 bit descriptor_length uimsbf */
  pos = data;
  *data++ = 0xFC;
  *data++ = 0x00;

  _packetize_descriptor_array (rrt->descriptors, &data);

  /* Go back and update the descriptor length */
  GST_WRITE_UINT16_BE (pos, (data - pos - 2) | 0xFC00);

  return TRUE;
}

/**
 * gst_mpegts_section_section_from_atsc_rrt:
 * @rrt: (transfer full): a #GstMpegtsAtscRRT to create the #GstMpegtsSection from
 *
 * Returns: (transfer full): the #GstMpegtsSection
 * Since: 1.18
 */
GstMpegtsSection *
gst_mpegts_section_from_atsc_rrt (GstMpegtsAtscRRT * rrt)
{
  GstMpegtsSection *section;

  g_return_val_if_fail (rrt != NULL, NULL);

  section = _gst_mpegts_section_init (0x1ffb,
      GST_MTS_TABLE_ID_ATSC_RATING_REGION);

  /* FIXME random rating_region, what should be the default? */
  section->subtable_extension = 0xff01;
  section->cached_parsed = (gpointer) rrt;
  section->packetizer = _packetize_rrt;
  section->destroy_parsed = (GDestroyNotify) _gst_mpegts_atsc_rrt_free;

  return section;
}

/**
 * gst_mpegts_section_get_atsc_rrt:
 * @section: a #GstMpegtsSection of type %GST_MPEGTS_SECTION_ATSC_RRT
 *
 * Returns the #GstMpegtsAtscRRT contained in the @section.
 *
 * Returns: The #GstMpegtsAtscRRT contained in the section, or %NULL if an error
 * happened.
 * Since: 1.18
 */
const GstMpegtsAtscRRT *
gst_mpegts_section_get_atsc_rrt (GstMpegtsSection * section)
{
  g_return_val_if_fail (section->section_type == GST_MPEGTS_SECTION_ATSC_RRT,
      NULL);
  g_return_val_if_fail (section->cached_parsed || section->data, NULL);

  if (!section->cached_parsed)
    section->cached_parsed =
        __common_section_checks (section, 17, _parse_rrt,
        (GDestroyNotify) _gst_mpegts_atsc_rrt_free);

  return (const GstMpegtsAtscRRT *) section->cached_parsed;
}

/**
 * gst_mpegts_section_atsc_rrt_dimension_value_new:
 *
 * Returns: (transfer full): #GstMpegtsAtscRRTDimensionValue
 * Since: 1.18
 */
GstMpegtsAtscRRTDimensionValue *
gst_mpegts_atsc_rrt_dimension_value_new (void)
{
  GstMpegtsAtscRRTDimensionValue *val;

  val = g_new0 (GstMpegtsAtscRRTDimensionValue, 1);
  val->abbrev_ratings = g_ptr_array_new_with_free_func ((GDestroyNotify)
      _gst_mpegts_atsc_mult_string_free);
  val->ratings = g_ptr_array_new_with_free_func ((GDestroyNotify)
      _gst_mpegts_atsc_mult_string_free);

  return val;
}

/**
 * gst_mpegts_section_atsc_rrt_dimension_new:
 *
 * Returns: (transfer full): #GstMpegtsAtscRRTDimension
 * Since: 1.18
 */
GstMpegtsAtscRRTDimension *
gst_mpegts_atsc_rrt_dimension_new (void)
{
  GstMpegtsAtscRRTDimension *dim;

  dim = g_new0 (GstMpegtsAtscRRTDimension, 1);
  dim->names = g_ptr_array_new_with_free_func ((GDestroyNotify)
      _gst_mpegts_atsc_mult_string_free);
  dim->values = g_ptr_array_new_with_free_func ((GDestroyNotify)
      _gst_mpegts_atsc_rrt_dimension_value_free);

  return dim;
}

/**
 * gst_mpegts_section_atsc_rrt_new:
 *
 * Returns: (transfer full): #GstMpegtsAtscRRT
 * Since: 1.18
 */
GstMpegtsAtscRRT *
gst_mpegts_atsc_rrt_new (void)
{
  GstMpegtsAtscRRT *rrt;

  rrt = g_new0 (GstMpegtsAtscRRT, 1);
  rrt->names = g_ptr_array_new_with_free_func ((GDestroyNotify)
      _gst_mpegts_atsc_mult_string_free);
  rrt->dimensions = g_ptr_array_new_with_free_func ((GDestroyNotify)
      _gst_mpegts_atsc_rrt_dimension_free);
  rrt->descriptors = g_ptr_array_new_with_free_func ((GDestroyNotify)
      gst_mpegts_descriptor_free);

  return rrt;
}
