/*  GStreamer JPEG parser
 *  Copyright (C) 2011-2012 Intel Corporation
 *  Copyright (C) 2015 Tim-Philipp Müller <tim@centricular.com>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License
 *  as published by the Free Software Foundation; either version 2.1
 *  of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301 USA
 */

/**
 * SECTION:gstjpegparser
 * @title: GstJpegParser
 * @short_description: Convenience library for JPEG bitstream parsing.
 *
 * Provides useful functions for parsing JPEG images
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <stdlib.h>
#include <gst/base/gstbytereader.h>
#include "gstjpegparser.h"

#ifndef GST_DISABLE_GST_DEBUG

#define GST_CAT_DEFAULT ensure_debug_category()

static GstDebugCategory *
ensure_debug_category (void)
{
  static gsize cat_gonce = 0;

  if (g_once_init_enter (&cat_gonce)) {
    gsize cat_done;

    cat_done = (gsize) _gst_debug_category_new ("codecparsers_jpeg", 0,
        "GstJpegCodecParser");

    g_once_init_leave (&cat_gonce, cat_done);
  }

  return (GstDebugCategory *) cat_gonce;
}
#else

#define ensure_debug_category() /* NOOP */

#endif /* GST_DISABLE_GST_DEBUG */

#define DEBUG_PRINT_COMMENT 0

#define READ_UINT8(reader, val) G_STMT_START {                  \
    if (!gst_byte_reader_get_uint8 ((reader), &(val))) {        \
      GST_WARNING ("failed to read uint8");                     \
      goto failed;                                              \
    }                                                           \
  } G_STMT_END

#define READ_UINT16(reader, val) G_STMT_START {                 \
    if (!gst_byte_reader_get_uint16_be ((reader), &(val))) {    \
      GST_WARNING ("failed to read uint16");                    \
      goto failed;                                              \
    }                                                           \
  } G_STMT_END

#define READ_BYTES(reader, buf, length) G_STMT_START {          \
    const guint8 *vals;                                         \
    if (!gst_byte_reader_get_data (reader, length, &vals)) {    \
      GST_WARNING ("failed to read bytes, size:%d", length);    \
      goto failed;                                              \
    }                                                           \
    memcpy (buf, vals, length);                                 \
  } G_STMT_END

#define U_READ_UINT8(reader, val) G_STMT_START {                \
    (val) = gst_byte_reader_get_uint8_unchecked(reader);        \
  } G_STMT_END

#define U_READ_UINT16(reader, val) G_STMT_START {               \
    (val) = gst_byte_reader_get_uint16_be_unchecked(reader);    \
  } G_STMT_END


/* Table used to address an 8x8 matrix in zig-zag order */
/* *INDENT-OFF* */
static const guint8 zigzag_index[64] = {
  0,   1,  8, 16,  9,  2,  3, 10,
  17, 24, 32, 25, 18, 11,  4,  5,
  12, 19, 26, 33, 40, 48, 41, 34,
  27, 20, 13,  6,  7, 14, 21, 28,
  35, 42, 49, 56, 57, 50, 43, 36,
  29, 22, 15, 23, 30, 37, 44, 51,
  58, 59, 52, 45, 38, 31, 39, 46,
  53, 60, 61, 54, 47, 55, 62, 63
};
/* *INDENT-ON* */

/* Table K.1 - Luminance quantization table */
/* *INDENT-OFF* */
static const guint8 default_luminance_quant_table[64] = {
  16,  11,  10,  16,  24,  40,  51,  61,
  12,  12,  14,  19,  26,  58,  60,  55,
  14,  13,  16,  24,  40,  57,  69,  56,
  14,  17,  22,  29,  51,  87,  80,  62,
  18,  22,  37,  56,  68, 109, 103,  77,
  24,  35,  55,  64,  81, 104, 113,  92,
  49,  64,  78,  87, 103, 121, 120, 101,
  72,  92,  95,  98, 112, 100, 103,  99
};
/* *INDENT-ON* */

/* Table K.2 - Chrominance quantization table */
/* *INDENT-OFF* */
static const guint8 default_chrominance_quant_table[64] = {
  17,  18,  24,  47,  99,  99,  99,  99,
  18,  21,  26,  66,  99,  99,  99,  99,
  24,  26,  56,  99,  99,  99,  99,  99,
  47,  66,  99,  99,  99,  99,  99,  99,
  99,  99,  99,  99,  99,  99,  99,  99,
  99,  99,  99,  99,  99,  99,  99,  99,
  99,  99,  99,  99,  99,  99,  99,  99,
  99,  99,  99,  99,  99,  99,  99,  99
};
/* *INDENT-ON* */

typedef struct _GstJpegHuffmanTableEntry GstJpegHuffmanTableEntry;
struct _GstJpegHuffmanTableEntry
{
  guint8 value;                 /* category */
  guint8 length;                /* code length in bits */
};

/* Table K.3 - Table for luminance DC coefficient differences */
static const GstJpegHuffmanTableEntry default_luminance_dc_table[] = {
  {0x00, 2}, {0x01, 3}, {0x02, 3}, {0x03, 3}, {0x04, 3}, {0x05, 3},
  {0x06, 4}, {0x07, 5}, {0x08, 6}, {0x09, 7}, {0x0a, 8}, {0x0b, 9}
};

/* Table K.4 - Table for chrominance DC coefficient differences */
static const GstJpegHuffmanTableEntry default_chrominance_dc_table[] = {
  {0x00, 2}, {0x01, 2}, {0x02, 2}, {0x03, 3}, {0x04, 4}, {0x05, 5},
  {0x06, 6}, {0x07, 7}, {0x08, 8}, {0x09, 9}, {0x0a, 10}, {0x0b, 11}
};

/* Table K.5 - Table for luminance AC coefficients */
/* *INDENT-OFF* */
static const GstJpegHuffmanTableEntry default_luminance_ac_table[] = {
  {0x00,  4}, {0x01,  2}, {0x02,  2}, {0x03,  3}, {0x04,  4}, {0x05,  5},
  {0x06,  7}, {0x07,  8}, {0x08, 10}, {0x09, 16}, {0x0a, 16}, {0x11,  4},
  {0x12,  5}, {0x13,  7}, {0x14,  9}, {0x15, 11}, {0x16, 16}, {0x17, 16},
  {0x18, 16}, {0x19, 16}, {0x1a, 16}, {0x21,  5}, {0x22,  8}, {0x23, 10},
  {0x24, 12}, {0x25, 16}, {0x26, 16}, {0x27, 16}, {0x28, 16}, {0x29, 16},
  {0x2a, 16}, {0x31,  6}, {0x32,  9}, {0x33, 12}, {0x34, 16}, {0x35, 16},
  {0x36, 16}, {0x37, 16}, {0x38, 16}, {0x39, 16}, {0x3a, 16}, {0x41,  6},
  {0x42, 10}, {0x43, 16}, {0x44, 16}, {0x45, 16}, {0x46, 16}, {0x47, 16},
  {0x48, 16}, {0x49, 16}, {0x4a, 16}, {0x51,  7}, {0x52, 11}, {0x53, 16},
  {0x54, 16}, {0x55, 16}, {0x56, 16}, {0x57, 16}, {0x58, 16}, {0x59, 16},
  {0x5a, 16}, {0x61,  7}, {0x62, 12}, {0x63, 16}, {0x64, 16}, {0x65, 16},
  {0x66, 16}, {0x67, 16}, {0x68, 16}, {0x69, 16}, {0x6a, 16}, {0x71,  8},
  {0x72, 12}, {0x73, 16}, {0x74, 16}, {0x75, 16}, {0x76, 16}, {0x77, 16},
  {0x78, 16}, {0x79, 16}, {0x7a, 16}, {0x81,  9}, {0x82, 15}, {0x83, 16},
  {0x84, 16}, {0x85, 16}, {0x86, 16}, {0x87, 16}, {0x88, 16}, {0x89, 16},
  {0x8a, 16}, {0x91,  9}, {0x92, 16}, {0x93, 16}, {0x94, 16}, {0x95, 16},
  {0x96, 16}, {0x97, 16}, {0x98, 16}, {0x99, 16}, {0x9a, 16}, {0xa1,  9},
  {0xa2, 16}, {0xa3, 16}, {0xa4, 16}, {0xa5, 16}, {0xa6, 16}, {0xa7, 16},
  {0xa8, 16}, {0xa9, 16}, {0xaa, 16}, {0xb1, 10}, {0xb2, 16}, {0xb3, 16},
  {0xb4, 16}, {0xb5, 16}, {0xb6, 16}, {0xb7, 16}, {0xb8, 16}, {0xb9, 16},
  {0xba, 16}, {0xc1, 10}, {0xc2, 16}, {0xc3, 16}, {0xc4, 16}, {0xc5, 16},
  {0xc6, 16}, {0xc7, 16}, {0xc8, 16}, {0xc9, 16}, {0xca, 16}, {0xd1, 11},
  {0xd2, 16}, {0xd3, 16}, {0xd4, 16}, {0xd5, 16}, {0xd6, 16}, {0xd7, 16},
  {0xd8, 16}, {0xd9, 16}, {0xda, 16}, {0xe1, 16}, {0xe2, 16}, {0xe3, 16},
  {0xe4, 16}, {0xe5, 16}, {0xe6, 16}, {0xe7, 16}, {0xe8, 16}, {0xe9, 16},
  {0xea, 16}, {0xf0, 11}, {0xf1, 16}, {0xf2, 16}, {0xf3, 16}, {0xf4, 16},
  {0xf5, 16}, {0xf6, 16}, {0xf7, 16}, {0xf8, 16}, {0xf9, 16}, {0xfa, 16}
};
/* *INDENT-ON* */

/* Table K.6 - Table for chrominance AC coefficients */
/* *INDENT-OFF* */
static const GstJpegHuffmanTableEntry default_chrominance_ac_table[] = {
  {0x00,  2}, {0x01,  2}, {0x02,  3}, {0x03,  4}, {0x04,  5}, {0x05,  5},
  {0x06,  6}, {0x07,  7}, {0x08,  9}, {0x09, 10}, {0x0a, 12}, {0x11,  4},
  {0x12,  6}, {0x13,  8}, {0x14,  9}, {0x15, 11}, {0x16, 12}, {0x17, 16},
  {0x18, 16}, {0x19, 16}, {0x1a, 16}, {0x21,  5}, {0x22,  8}, {0x23, 10},
  {0x24, 12}, {0x25, 15}, {0x26, 16}, {0x27, 16}, {0x28, 16}, {0x29, 16},
  {0x2a, 16}, {0x31,  5}, {0x32,  8}, {0x33, 10}, {0x34, 12}, {0x35, 16},
  {0x36, 16}, {0x37, 16}, {0x38, 16}, {0x39, 16}, {0x3a, 16}, {0x41,  6},
  {0x42,  9}, {0x43, 16}, {0x44, 16}, {0x45, 16}, {0x46, 16}, {0x47, 16},
  {0x48, 16}, {0x49, 16}, {0x4a, 16}, {0x51,  6}, {0x52, 10}, {0x53, 16},
  {0x54, 16}, {0x55, 16}, {0x56, 16}, {0x57, 16}, {0x58, 16}, {0x59, 16},
  {0x5a, 16}, {0x61,  7}, {0x62, 11}, {0x63, 16}, {0x64, 16}, {0x65, 16},
  {0x66, 16}, {0x67, 16}, {0x68, 16}, {0x69, 16}, {0x6a, 16}, {0x71,  7},
  {0x72, 11}, {0x73, 16}, {0x74, 16}, {0x75, 16}, {0x76, 16}, {0x77, 16},
  {0x78, 16}, {0x79, 16}, {0x7a, 16}, {0x81,  8}, {0x82, 16}, {0x83, 16},
  {0x84, 16}, {0x85, 16}, {0x86, 16}, {0x87, 16}, {0x88, 16}, {0x89, 16},
  {0x8a, 16}, {0x91,  9}, {0x92, 16}, {0x93, 16}, {0x94, 16}, {0x95, 16},
  {0x96, 16}, {0x97, 16}, {0x98, 16}, {0x99, 16}, {0x9a, 16}, {0xa1,  9},
  {0xa2, 16}, {0xa3, 16}, {0xa4, 16}, {0xa5, 16}, {0xa6, 16}, {0xa7, 16},
  {0xa8, 16}, {0xa9, 16}, {0xaa, 16}, {0xb1,  9}, {0xb2, 16}, {0xb3, 16},
  {0xb4, 16}, {0xb5, 16}, {0xb6, 16}, {0xb7, 16}, {0xb8, 16}, {0xb9, 16},
  {0xba, 16}, {0xc1,  9}, {0xc2, 16}, {0xc3, 16}, {0xc4, 16}, {0xc5, 16},
  {0xc6, 16}, {0xc7, 16}, {0xc8, 16}, {0xc9, 16}, {0xca, 16}, {0xd1, 11},
  {0xd2, 16}, {0xd3, 16}, {0xd4, 16}, {0xd5, 16}, {0xd6, 16}, {0xd7, 16},
  {0xd8, 16}, {0xd9, 16}, {0xda, 16}, {0xe1, 14}, {0xe2, 16}, {0xe3, 16},
  {0xe4, 16}, {0xe5, 16}, {0xe6, 16}, {0xe7, 16}, {0xe8, 16}, {0xe9, 16},
  {0xea, 16}, {0xf0, 10}, {0xf1, 15}, {0xf2, 16}, {0xf3, 16}, {0xf4, 16},
  {0xf5, 16}, {0xf6, 16}, {0xf7, 16}, {0xf8, 16}, {0xf9, 16}, {0xfa, 16}
};
/* *INDENT-ON* */

static gint gst_jpeg_scan_for_marker_code (const guint8 * data, gsize size,
    guint offset);

static inline gboolean
jpeg_parse_to_next_marker (GstByteReader * br, GstJpegMarker * marker)
{
  gint ofs;

  ofs = gst_jpeg_scan_for_marker_code (br->data, br->size, br->byte);
  if (ofs < 0)
    return FALSE;

  if (marker)
    *marker = (GstJpegMarker) br->data[ofs + 1];

  gst_byte_reader_skip_unchecked (br, ofs - br->byte);
  return TRUE;
}

/* gst_jpeg_scan_for_marker_code:
 * @data: The data to parse
 * @size: The size of @data
 * @offset: The offset from which to start parsing
 *
 * Scans the JPEG bitstream contained in @data for the next marker
 * code. If found, the function returns an offset to the marker code,
 * including the 0xff prefix code but excluding any extra fill bytes.
 *
 * Returns: offset to the marker code if found, or -1 if not found.
 */
static gint
gst_jpeg_scan_for_marker_code (const guint8 * data, gsize size, guint offset)
{
  guint i;

  i = offset + 1;
  while (i < size) {
    const guint8 v = data[i];
    if (v < 0xc0)
      i += 2;
    else if (v < 0xff && data[i - 1] == 0xff)
      return i - 1;
    else
      i++;
  }
  return -1;
}

/**
 * gst_jpeg_segment_parse_frame_header:
 * @segment: the JPEG segment
 * @frame_hdr: (out): The #GstJpegFrameHdr structure to fill in
 *
 * Parses the @frame_hdr JPEG frame header structure members from @segment.
 *
 * The caller must make sure there is enough data for the whole segment
 * available.
 *
 * Returns: TRUE if the frame header was correctly parsed.
 *
 * Since: 1.6
 */
gboolean
gst_jpeg_segment_parse_frame_header (const GstJpegSegment * segment,
    GstJpegFrameHdr * frame_hdr)
{
  GstByteReader br;
  guint8 val;
  guint i;

  g_return_val_if_fail (segment != NULL, FALSE);
  g_return_val_if_fail (frame_hdr != NULL, FALSE);

  if (segment->size < 8)
    return FALSE;

  gst_byte_reader_init (&br, segment->data + segment->offset, segment->size);
  gst_byte_reader_skip_unchecked (&br, 2);

  U_READ_UINT8 (&br, frame_hdr->sample_precision);
  U_READ_UINT16 (&br, frame_hdr->height);
  U_READ_UINT16 (&br, frame_hdr->width);
  U_READ_UINT8 (&br, frame_hdr->num_components);

  if (frame_hdr->num_components > GST_JPEG_MAX_SCAN_COMPONENTS)
    return FALSE;

  if (gst_byte_reader_get_remaining (&br) < 3 * frame_hdr->num_components)
    return FALSE;

  for (i = 0; i < frame_hdr->num_components; i++) {
    U_READ_UINT8 (&br, frame_hdr->components[i].identifier);
    U_READ_UINT8 (&br, val);
    frame_hdr->components[i].horizontal_factor = (val >> 4) & 0x0F;
    frame_hdr->components[i].vertical_factor = (val & 0x0F);
    U_READ_UINT8 (&br, frame_hdr->components[i].quant_table_selector);
    if (frame_hdr->components[i].horizontal_factor > 4
        || frame_hdr->components[i].vertical_factor > 4
        || frame_hdr->components[i].quant_table_selector >= 4)
      return FALSE;
  }

  if (gst_byte_reader_get_remaining (&br) > 0)
    GST_DEBUG ("data left at end of frame header segment");

  return TRUE;
}

/**
 * gst_jpeg_segment_parse_scan_header:
 * @segment: the JPEG segment
 * @scan_hdr: (out): The #GstJpegScanHdr structure to fill in
 *
 * Parses the @scan_hdr JPEG scan header structure members from @segment.
 *
 * The caller must make sure there is enough data for the whole segment
 * available.
 *
 * Returns: TRUE if the scan header was correctly parsed
 *
 * Since: 1.6
 */
gboolean
gst_jpeg_segment_parse_scan_header (const GstJpegSegment * segment,
    GstJpegScanHdr * scan_hdr)
{
  GstByteReader br;
  guint8 val;
  guint i;

  g_return_val_if_fail (segment != NULL, FALSE);
  g_return_val_if_fail (scan_hdr != NULL, FALSE);

  gst_byte_reader_init (&br, segment->data + segment->offset, segment->size);

  if (segment->size < 3)
    return FALSE;

  gst_byte_reader_skip_unchecked (&br, 2);

  U_READ_UINT8 (&br, scan_hdr->num_components);

  if (scan_hdr->num_components > GST_JPEG_MAX_SCAN_COMPONENTS)
    return FALSE;

  if (gst_byte_reader_get_remaining (&br) < 2 * scan_hdr->num_components)
    return FALSE;

  for (i = 0; i < scan_hdr->num_components; i++) {
    U_READ_UINT8 (&br, scan_hdr->components[i].component_selector);
    U_READ_UINT8 (&br, val);
    scan_hdr->components[i].dc_selector = (val >> 4) & 0x0F;
    scan_hdr->components[i].ac_selector = val & 0x0F;
    if (scan_hdr->components[i].dc_selector >= 4
        || scan_hdr->components[i].ac_selector >= 4)
      return FALSE;
  }

  if (gst_byte_reader_get_remaining (&br) < 3)
    return FALSE;

  /* FIXME: Ss, Se, Ah, Al */
  gst_byte_reader_skip_unchecked (&br, 3);

  if (gst_byte_reader_get_remaining (&br) > 0)
    GST_DEBUG ("data left at end of scan header segment");

  return TRUE;
}

/**
 * gst_jpeg_segment_parse_huffman_table:
 * @segment: the JPEG segment
 * @huff_tables: (out): The #GstJpegHuffmanTables structure to fill in
 *
 * Parses the JPEG Huffman table structure members from @segment.
 *
 * The caller must make sure there is enough data for the whole segment
 * available.
 *
 * Note: @huf_tables represents the complete set of possible Huffman
 * tables. However, the parser will only write to the Huffman table
 * specified by the table destination identifier (Th). While doing so,
 * the @valid flag of the specified Huffman table will also be set to
 * %TRUE;
 *
 * Returns: TRUE if the Huffman table was correctly parsed.
 *
 * Since: 1.6
 */
gboolean
gst_jpeg_segment_parse_huffman_table (const GstJpegSegment * segment,
    GstJpegHuffmanTables * huff_tables)
{
  GstByteReader br;
  GstJpegHuffmanTable *huf_table;
  guint8 val, table_class, table_index;
  guint32 value_count;
  guint i;

  g_return_val_if_fail (segment != NULL, FALSE);
  g_return_val_if_fail (huff_tables != NULL, FALSE);

  if (segment->size < 2)
    return FALSE;

  gst_byte_reader_init (&br, segment->data + segment->offset, segment->size);

  gst_byte_reader_skip_unchecked (&br, 2);

  while (gst_byte_reader_get_remaining (&br) > 0) {
    U_READ_UINT8 (&br, val);
    table_class = ((val >> 4) & 0x0F);
    table_index = (val & 0x0F);
    if (table_index >= GST_JPEG_MAX_SCAN_COMPONENTS)
      return FALSE;
    if (table_class == 0) {
      huf_table = &huff_tables->dc_tables[table_index];
    } else {
      huf_table = &huff_tables->ac_tables[table_index];
    }
    READ_BYTES (&br, huf_table->huf_bits, 16);
    value_count = 0;
    for (i = 0; i < 16; i++)
      value_count += huf_table->huf_bits[i];
    READ_BYTES (&br, huf_table->huf_values, value_count);
    huf_table->valid = TRUE;
  }
  return TRUE;

failed:
  return FALSE;
}

/**
 * gst_jpeg_segment_parse_quantization_table:
 * @segment: the JPEG segment
 * @quant_tables: (out): The #GstJpegQuantTables structure to fill in
 *
 * Parses the JPEG quantization table structure members from @segment.
 *
 * The caller must make sure there is enough data for the whole segment
 * available.
 *
 * Note: @quant_tables represents the complete set of possible
 * quantization tables. However, the parser will only write to the
 * quantization table specified by the table destination identifier
 * (Tq). While doing so, the @valid flag of the specified quantization
 * table will also be set to %TRUE.
 *
 * Returns: TRUE if the quantization table was correctly parsed.
 *
 * Since: 1.6
 */
gboolean
gst_jpeg_segment_parse_quantization_table (const GstJpegSegment * segment,
    GstJpegQuantTables * quant_tables)
{
  GstByteReader br;
  GstJpegQuantTable *quant_table;
  guint8 val, table_index;
  guint i;

  g_return_val_if_fail (segment != NULL, FALSE);
  g_return_val_if_fail (quant_tables != NULL, FALSE);

  if (segment->size < 2)
    return FALSE;

  gst_byte_reader_init (&br, segment->data + segment->offset, segment->size);

  gst_byte_reader_skip_unchecked (&br, 2);

  while (gst_byte_reader_get_remaining (&br) > 0) {
    guint8 element_size;

    U_READ_UINT8 (&br, val);
    table_index = (val & 0x0f);
    if (table_index >= GST_JPEG_MAX_SCAN_COMPONENTS)
      return FALSE;
    quant_table = &quant_tables->quant_tables[table_index];
    quant_table->quant_precision = ((val >> 4) & 0x0f);

    element_size = (quant_table->quant_precision == 0) ? 1 : 2;
    if (gst_byte_reader_get_remaining (&br) <
        GST_JPEG_MAX_QUANT_ELEMENTS * element_size)
      return FALSE;
    for (i = 0; i < GST_JPEG_MAX_QUANT_ELEMENTS; i++) {
      if (!quant_table->quant_precision) {      /* 8-bit values */
        U_READ_UINT8 (&br, val);
        quant_table->quant_table[i] = val;
      } else {                  /* 16-bit values */
        U_READ_UINT16 (&br, quant_table->quant_table[i]);
      }
    }
    quant_table->valid = TRUE;
  }
  return TRUE;
}

/**
 * gst_jpeg_segment_parse_restart_interval:
 * @segment: the JPEG segment
 * @interval: (out): The parsed restart interval value
 *
 * The caller must make sure there is enough data for the whole segment
 * available.
 *
 * Returns: TRUE if the restart interval value was correctly parsed.
 *
 * Since: 1.6
 */
gboolean
gst_jpeg_segment_parse_restart_interval (const GstJpegSegment * segment,
    guint * interval)
{
  GstByteReader br;
  guint16 val;

  g_return_val_if_fail (segment != NULL, FALSE);
  g_return_val_if_fail (interval != NULL, FALSE);

  if (segment->size < 4)
    return FALSE;

  gst_byte_reader_init (&br, segment->data + segment->offset, segment->size);
  gst_byte_reader_skip_unchecked (&br, 2);

  U_READ_UINT16 (&br, val);
  *interval = val;
  return TRUE;
}

static int
compare_huffman_table_entry (const void *a, const void *b)
{
  const GstJpegHuffmanTableEntry *const e1 = *(GstJpegHuffmanTableEntry **) a;
  const GstJpegHuffmanTableEntry *const e2 = *(GstJpegHuffmanTableEntry **) b;

  if (e1->length == e2->length)
    return (gint) e1->value - (gint) e2->value;
  return (gint) e1->length - (gint) e2->length;
}

static void
build_huffman_table (GstJpegHuffmanTable * huf_table,
    const GstJpegHuffmanTableEntry * entries, guint num_entries)
{
  const GstJpegHuffmanTableEntry *sorted_entries[256];
  guint i, j, n;

  g_assert (num_entries <= G_N_ELEMENTS (sorted_entries));

  for (i = 0; i < num_entries; i++)
    sorted_entries[i] = &entries[i];
  qsort ((void *) sorted_entries, num_entries, sizeof (sorted_entries[0]),
      compare_huffman_table_entry);

  for (i = 0, j = 1, n = 0; i < num_entries; i++) {
    const GstJpegHuffmanTableEntry *const e = sorted_entries[i];
    if (e->length != j) {
      huf_table->huf_bits[j++ - 1] = n;
      for (; j < e->length; j++)
        huf_table->huf_bits[j - 1] = 0;
      n = 0;
    }
    huf_table->huf_values[i] = e->value;
    n++;
  }
  huf_table->huf_bits[j - 1] = n;

  for (; j < G_N_ELEMENTS (huf_table->huf_bits); j++)
    huf_table->huf_bits[j] = 0;
  for (; i < G_N_ELEMENTS (huf_table->huf_values); i++)
    huf_table->huf_values[i] = 0;
  huf_table->valid = TRUE;
}

/**
 * gst_jpeg_get_default_huffman_tables:
 * @huf_tables: (out): The default DC/AC Huffman tables to fill in
 *
 * Fills in @huf_tables with the default AC/DC Huffman tables, as
 * specified by the JPEG standard.
 *
 * Since: 1.6
 */
void
gst_jpeg_get_default_huffman_tables (GstJpegHuffmanTables * huf_tables)
{
  g_return_if_fail (huf_tables != NULL);

  /* Build DC tables */
  build_huffman_table (&huf_tables->dc_tables[0], default_luminance_dc_table,
      G_N_ELEMENTS (default_luminance_dc_table));
  build_huffman_table (&huf_tables->dc_tables[1], default_chrominance_dc_table,
      G_N_ELEMENTS (default_chrominance_dc_table));
  memcpy (&huf_tables->dc_tables[2], &huf_tables->dc_tables[1],
      sizeof (huf_tables->dc_tables[2]));

  /* Build AC tables */
  build_huffman_table (&huf_tables->ac_tables[0], default_luminance_ac_table,
      G_N_ELEMENTS (default_luminance_ac_table));
  build_huffman_table (&huf_tables->ac_tables[1], default_chrominance_ac_table,
      G_N_ELEMENTS (default_chrominance_ac_table));
  memcpy (&huf_tables->ac_tables[2], &huf_tables->ac_tables[1],
      sizeof (huf_tables->ac_tables[2]));
}

static void
build_quant_table (GstJpegQuantTable * quant_table, const guint8 values[64])
{
  guint i;

  for (i = 0; i < 64; i++)
    quant_table->quant_table[i] = values[zigzag_index[i]];
  quant_table->quant_precision = 0;     /* Pq = 0 (8-bit precision) */
  quant_table->valid = TRUE;
}

/**
 * gst_jpeg_get_default_quantization_table:
 * @quant_tables: (out): The default luma/chroma quant-tables in zigzag mode
 *
 * Fills in @quant_tables with the default quantization tables, as
 * specified by the JPEG standard.
 *
 * Since: 1.6
 */
void
gst_jpeg_get_default_quantization_tables (GstJpegQuantTables * quant_tables)
{
  g_return_if_fail (quant_tables != NULL);

  build_quant_table (&quant_tables->quant_tables[0],
      default_luminance_quant_table);
  build_quant_table (&quant_tables->quant_tables[1],
      default_chrominance_quant_table);
  build_quant_table (&quant_tables->quant_tables[2],
      default_chrominance_quant_table);
}

/**
 * gst_jpeg_parse:
 * @segment: (out): pointer to a #GstJpegSegment structure to fill in
 * @data: The data to parse
 * @size: The size of @data
 * @offset: The offset from which to start parsing
 *
 * Parses the JPEG bitstream contained in @data, and returns the
 * detected segment as a #GstJpegSegment.
 *
 * Note that a valid segment may be returned with a length that exceeds
 * the available data. It is up to the caller to make sure there's enough
 * data available when parsing the segment.
 *
 * Returns: TRUE if a packet start code was found.
 *
 * Since: 1.6
 */
gboolean
gst_jpeg_parse (GstJpegSegment * segment,
    const guint8 * data, gsize size, guint offset)
{
  GstJpegSegment *seg = segment;
  GstByteReader br;
  guint16 length;

  g_return_val_if_fail (seg != NULL, FALSE);
  g_return_val_if_fail (data != NULL, FALSE);

  if (size <= offset) {
    GST_DEBUG ("failed to parse from offset %u, buffer is too small", offset);
    return FALSE;
  }

  size -= offset;
  gst_byte_reader_init (&br, &data[offset], size);

  if (!jpeg_parse_to_next_marker (&br, &seg->marker)) {
    GST_DEBUG ("failed to find marker code");
    return FALSE;
  }

  gst_byte_reader_skip_unchecked (&br, 2);
  seg->offset = offset + gst_byte_reader_get_pos (&br);
  seg->size = -1;

  /* Try to find end of segment */
  switch (seg->marker) {
    case GST_JPEG_MARKER_SOI:
    case GST_JPEG_MARKER_EOI:
    fixed_size_segment:
      seg->size = 0;
      break;

    case (GST_JPEG_MARKER_SOF_MIN + 0):        /* Lf */
    case (GST_JPEG_MARKER_SOF_MIN + 1):        /* Lf */
    case (GST_JPEG_MARKER_SOF_MIN + 2):        /* Lf */
    case (GST_JPEG_MARKER_SOF_MIN + 3):        /* Lf */
    case (GST_JPEG_MARKER_SOF_MIN + 9):        /* Lf */
    case (GST_JPEG_MARKER_SOF_MIN + 10):       /* Lf */
    case (GST_JPEG_MARKER_SOF_MIN + 11):       /* Lf */
    case GST_JPEG_MARKER_SOS:  /* Ls */
    case GST_JPEG_MARKER_DQT:  /* Lq */
    case GST_JPEG_MARKER_DHT:  /* Lh */
    case GST_JPEG_MARKER_DAC:  /* La */
    case GST_JPEG_MARKER_DRI:  /* Lr */
    case GST_JPEG_MARKER_COM:  /* Lc */
    case GST_JPEG_MARKER_DNL:  /* Ld */
    variable_size_segment:
      READ_UINT16 (&br, length);
      seg->size = length;
      break;

    default:
      /* Application data segment length (Lp) */
      if (seg->marker >= GST_JPEG_MARKER_APP_MIN &&
          seg->marker <= GST_JPEG_MARKER_APP_MAX)
        goto variable_size_segment;

      /* Restart markers (fixed size, two bytes only) */
      if (seg->marker >= GST_JPEG_MARKER_RST_MIN &&
          seg->marker <= GST_JPEG_MARKER_RST_MAX)
        goto fixed_size_segment;

      /* Fallback: scan for next marker */
      if (!jpeg_parse_to_next_marker (&br, NULL))
        goto failed;
      seg->size = gst_byte_reader_get_pos (&br) - seg->offset;
      break;
  }

  seg->data = data;
  return TRUE;

failed:
  return FALSE;
}
