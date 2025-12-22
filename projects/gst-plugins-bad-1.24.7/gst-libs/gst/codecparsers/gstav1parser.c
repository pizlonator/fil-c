/* gstav1parser.c
 *
 *  Copyright (C) 2018 Georg Ottinger
 *  Copyright (C) 2019-2020 Intel Corporation
 *    Author: Georg Ottinger <g.ottinger@gmx.at>
 *    Author: Junyan He <junyan.he@hotmail.com>
 *    Author: Victor Jaquez <vjaquez@igalia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */
/**
 * SECTION:gstav1parser
 * @title: GstAV1Parser
 * @short_description: Convenience library for parsing AV1 video bitstream.
 *
 * For more details about the structures, you can refer to the AV1 Bitstream &
 * Decoding Process Specification V1.0.0
 * [specification](https://aomediacodec.github.io/av1-spec/av1-spec.pdf)
 *
 * It offers you bitstream parsing of low overhead bistream format (Section 5)
 * or Annex B according to the setting of the parser. By calling the function of
 * gst_av1_parser_reset(), user can switch between bistream mode and Annex B mode.
 *
 * To retrieve OBUs and parse its headers, you should firstly call the function of
 * gst_av1_parser_identify_one_obu() to get the OBU type if succeeds or just discard
 * the data if fails.
 *
 * Then, depending on the #GstAV1OBUType of the newly parsed #GstAV1OBU,
 * you should call the differents functions to parse the structure details:
 *
 *   * #GST_AV1_OBU_SEQUENCE_HEADER: gst_av1_parser_parse_sequence_header_obu()
 *
 *   * #GST_AV1_OBU_TEMPORAL_DELIMITER: gst_av1_parser_parse_temporal_delimiter_obu()
 *
 *   * #GST_AV1_OBU_FRAME: gst_av1_parser_parse_frame_obu()
 *
 *   * #GST_AV1_OBU_FRAME_HEADER: gst_av1_parser_parse_frame_header_obu()
 *
 *   * #GST_AV1_OBU_TILE_GROUP: gst_av1_parser_parse_tile_group_obu()
 *
 *   * #GST_AV1_OBU_METADATA: gst_av1_parser_parse_metadata_obu()
 *
 *   * #GST_AV1_OBU_REDUNDANT_FRAME_HEADER: gst_av1_parser_parse_frame_header_obu()
 *
 *   * #GST_AV1_OBU_TILE_LIST: gst_av1_parser_parse_tile_list_obu()
 *
 * Note: Some parser functions are dependent on information provided in the sequence
 * header and reference frame's information. It maintains a state inside itself, which
 * contains all global vars and reference information during the whole parsing process.
 * Calling gst_av1_parser_reset() or a new sequence's arriving can clear and reset this
 * inside state.
 *
 * After successfully handled a frame(for example, decode a frame successfully), you
 * should call gst_av1_parser_reference_frame_update() to update the parser's inside
 * state(such as reference information, global segmentation information, etc).
 *
 * @since: 1.18.00
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstav1parser.h"

#include <gst/base/gstbitreader.h>

#include <string.h>
#include <stdlib.h>

#ifndef GST_DISABLE_GST_DEBUG
#define GST_CAT_DEFAULT gst_av1_debug_category_get ()
static GstDebugCategory *
gst_av1_debug_category_get (void)
{
  static gsize cat_gonce = 0;

  if (g_once_init_enter (&cat_gonce)) {
    GstDebugCategory *cat = NULL;

    GST_DEBUG_CATEGORY_INIT (cat, "codecparsers_av1", 0, "av1 parse library");

    g_once_init_leave (&cat_gonce, (gsize) cat);
  }

  return (GstDebugCategory *) cat_gonce;
}
#endif /* GST_DISABLE_GST_DEBUG */

#define AV1_READ_BIT(br)    ((guint8) gst_bit_reader_get_bits_uint32_unchecked (br, 1))
#define AV1_READ_UINT8(br)  ((guint8) gst_bit_reader_get_bits_uint32_unchecked (br, 8))
#define AV1_READ_UINT16(br) ((guint16) gst_bit_reader_get_bits_uint32_unchecked (br, 16))
#define AV1_READ_UINT32(br) gst_bit_reader_get_bits_uint32_unchecked (br, 32)
#define AV1_READ_UINT64(br) gst_bit_reader_get_bits_uint64_unchecked (br, 64)
#define AV1_READ_BITS(br, nbits)                                        \
  ((nbits <= 32) ? (gst_bit_reader_get_bits_uint32_unchecked (br, nbits)) : \
   (gst_bit_reader_get_bits_uint64_unchecked (br, nbits)))

static guint64
av1_read_bits_checked (GstBitReader * br, guint nbits,
    GstAV1ParserResult * retval, const char *func_name, gint line)
{
  guint64 read_bits = 0;
  gboolean result;

  if (nbits <= 64)
    result = gst_bit_reader_get_bits_uint64 (br, &read_bits, nbits);
  else
    result = FALSE;

  if (result == TRUE) {
    *retval = GST_AV1_PARSER_OK;
    return read_bits;
  } else {
    *retval = GST_AV1_PARSER_NO_MORE_DATA;
    GST_WARNING ("Read %d bits failed in func: %s, line %d", nbits, func_name,
        line);
    return 0;
  }
}

#define AV1_READ_BIT_CHECKED(br, ret)                                   \
  ((guint8) av1_read_bits_checked (br, 1, ret, __func__, __LINE__))
#define AV1_READ_UINT8_CHECKED(br, ret)                                 \
  ((guint8) av1_read_bits_checked (br, 8, ret, __func__, __LINE__))
#define AV1_READ_UINT16_CHECKED(br, ret)                                \
  ((guint16) av1_read_bits_checked (br, 16, ret, __func__, __LINE__))
#define AV1_READ_UINT32_CHECKED(br, ret)                                \
  ((guint32) av1_read_bits_checked (br, 32, ret, __func__, __LINE__))
#define AV1_READ_BITS_CHECKED(br, nbits, ret)                   \
  av1_read_bits_checked (br, nbits, ret, __func__, __LINE__)

#define AV1_REMAINING_BYTES(br) (gst_bit_reader_get_remaining (br) / 8)
#define AV1_REMAINING_BITS(br)  (gst_bit_reader_get_remaining (br))

/*************************************
 *                                   *
 * Helperfunctions                   *
 *                                   *
 *************************************/

/* 4.7
 *
 * floor of the base 2 logarithm of the input x */
static gint
av1_helpers_floor_log2 (guint32 x)
{
  gint s = 0;

  while (x != 0) {
    x = x >> 1;
    s++;
  }
  return s - 1;
}

/* 5.9.16 Tile size calculation
 *
 * returns the smallest value for k such that blkSize << k is greater
 * than or equal to target */
static gint
av1_helper_tile_log2 (gint blkSize, gint target)
{
  gint k;
  for (k = 0; (blkSize << k) < target; k++);
  return k;
}

/* 5.9.29 */
static gint
av1_helper_inverse_recenter (gint r, gint v)
{
  if (v > 2 * r)
    return v;
  else if (v & 1)
    return r - ((v + 1) >> 1);
  else
    return r + (v >> 1);
}

/* Shift down with rounding for use when n >= 0, value >= 0 */
static guint64
av1_helper_round_power_of_two (guint64 value, guint16 n)
{
  return (value + (((guint64) (1) << n) >> 1)) >> n;
}

 /* Shift down with rounding for signed integers, for use when n >= 0 */
static gint64
av1_helper_round_power_of_two_signed (gint64 value, guint16 n)
{
  return (value < 0) ? -((gint64) (av1_helper_round_power_of_two (-value, n)))
      : (gint64) av1_helper_round_power_of_two (value, n);
}

static gint
av1_helper_msb (guint n)
{
  int log = 0;
  guint value = n;
  int i;

  g_assert (n != 0);

  for (i = 4; i >= 0; --i) {
    const gint shift = (1 << i);
    const guint x = value >> shift;
    if (x != 0) {
      value = x;
      log += shift;
    }
  }
  return log;
}

static const guint16 div_lut[GST_AV1_DIV_LUT_NUM + 1] = {
  16384, 16320, 16257, 16194, 16132, 16070, 16009, 15948, 15888, 15828, 15768,
  15709, 15650, 15592, 15534, 15477, 15420, 15364, 15308, 15252, 15197, 15142,
  15087, 15033, 14980, 14926, 14873, 14821, 14769, 14717, 14665, 14614, 14564,
  14513, 14463, 14413, 14364, 14315, 14266, 14218, 14170, 14122, 14075, 14028,
  13981, 13935, 13888, 13843, 13797, 13752, 13707, 13662, 13618, 13574, 13530,
  13487, 13443, 13400, 13358, 13315, 13273, 13231, 13190, 13148, 13107, 13066,
  13026, 12985, 12945, 12906, 12866, 12827, 12788, 12749, 12710, 12672, 12633,
  12596, 12558, 12520, 12483, 12446, 12409, 12373, 12336, 12300, 12264, 12228,
  12193, 12157, 12122, 12087, 12053, 12018, 11984, 11950, 11916, 11882, 11848,
  11815, 11782, 11749, 11716, 11683, 11651, 11619, 11586, 11555, 11523, 11491,
  11460, 11429, 11398, 11367, 11336, 11305, 11275, 11245, 11215, 11185, 11155,
  11125, 11096, 11067, 11038, 11009, 10980, 10951, 10923, 10894, 10866, 10838,
  10810, 10782, 10755, 10727, 10700, 10673, 10645, 10618, 10592, 10565, 10538,
  10512, 10486, 10460, 10434, 10408, 10382, 10356, 10331, 10305, 10280, 10255,
  10230, 10205, 10180, 10156, 10131, 10107, 10082, 10058, 10034, 10010, 9986,
  9963, 9939, 9916, 9892, 9869, 9846, 9823, 9800, 9777, 9754, 9732,
  9709, 9687, 9664, 9642, 9620, 9598, 9576, 9554, 9533, 9511, 9489,
  9468, 9447, 9425, 9404, 9383, 9362, 9341, 9321, 9300, 9279, 9259,
  9239, 9218, 9198, 9178, 9158, 9138, 9118, 9098, 9079, 9059, 9039,
  9020, 9001, 8981, 8962, 8943, 8924, 8905, 8886, 8867, 8849, 8830,
  8812, 8793, 8775, 8756, 8738, 8720, 8702, 8684, 8666, 8648, 8630,
  8613, 8595, 8577, 8560, 8542, 8525, 8508, 8490, 8473, 8456, 8439,
  8422, 8405, 8389, 8372, 8355, 8339, 8322, 8306, 8289, 8273, 8257,
  8240, 8224, 8208, 8192,
};

static gint16
av1_helper_resolve_divisor_32 (guint32 D, gint16 * shift)
{
  gint32 f;
  gint32 e;

  *shift = av1_helper_msb (D);
  // e is obtained from D after resetting the most significant 1 bit.
  e = D - ((guint32) 1 << *shift);
  // Get the most significant DIV_LUT_BITS (8) bits of e into f
  if (*shift > GST_AV1_DIV_LUT_BITS)
    f = av1_helper_round_power_of_two (e, *shift - GST_AV1_DIV_LUT_BITS);
  else
    f = e << (GST_AV1_DIV_LUT_BITS - *shift);
  g_assert (f <= GST_AV1_DIV_LUT_NUM);
  *shift += GST_AV1_DIV_LUT_PREC_BITS;
  // Use f as lookup into the precomputed table of multipliers
  return div_lut[f];
}

/*************************************
 *                                   *
 * Bitstream Functions               *
 *                                   *
 *************************************/
/* 4.10.5
 *
 * Unsigned integer represented by a variable number of little-endian
 * bytes. */
static guint32
av1_bitstreamfn_leb128 (GstBitReader * br, GstAV1ParserResult * retval)
{
  guint8 leb128_byte = 0;
  guint64 value = 0;
  gint i;

  for (i = 0; i < 8; i++) {
    leb128_byte = AV1_READ_UINT8_CHECKED (br, retval);
    if (*retval != GST_AV1_PARSER_OK)
      return 0;

    value |= (((gint) leb128_byte & 0x7f) << (i * 7));
    if (!(leb128_byte & 0x80))
      break;
  }

  /* check for bitstream conformance see chapter4.10.5 */
  if (value < G_MAXUINT32) {
    return (guint32) value;
  } else {
    GST_WARNING ("invalid leb128");
    *retval = GST_AV1_PARSER_BITSTREAM_ERROR;
    return 0;
  }
}

/* 4.10.3
 *
 * Variable length unsigned n-bit number appearing directly in the
 * bitstream */
static guint32
av1_bitstreamfn_uvlc (GstBitReader * br, GstAV1ParserResult * retval)
{
  guint8 leadingZero = 0;
  guint32 readv;
  guint32 value;
  gboolean done;

  while (1) {
    done = AV1_READ_BIT_CHECKED (br, retval);
    if (*retval != GST_AV1_PARSER_OK) {
      GST_WARNING ("invalid uvlc");
      return 0;
    }

    if (done)
      break;
    leadingZero++;
  }

  if (leadingZero >= 32) {
    value = G_MAXUINT32;
    return value;
  }
  readv = AV1_READ_BITS_CHECKED (br, leadingZero, retval);
  if (*retval != GST_AV1_PARSER_OK) {
    GST_WARNING ("invalid uvlc");
    return 0;
  }

  value = readv + (1 << leadingZero) - 1;

  return value;
}

/* 4.10.6
 *
 * Signed integer converted from an n bits unsigned integer in the
 * bitstream */
static guint32
av1_bitstreamfn_su (GstBitReader * br, guint8 n, GstAV1ParserResult * retval)
{
  guint32 v, signMask;

  v = AV1_READ_BITS_CHECKED (br, n, retval);
  if (*retval != GST_AV1_PARSER_OK)
    return 0;

  signMask = 1 << (n - 1);
  if (v & signMask)
    return v - 2 * signMask;
  else
    return v;
}

/* 4.10.7
 *
 * Unsigned encoded integer with maximum number of values n */
static guint32
av1_bitstreamfn_ns (GstBitReader * br, guint32 n, GstAV1ParserResult * retval)
{
  gint w, m, v;
  gint extra_bit;

  w = av1_helpers_floor_log2 (n) + 1;
  m = (1 << w) - n;
  v = AV1_READ_BITS_CHECKED (br, w - 1, retval);
  if (*retval != GST_AV1_PARSER_OK)
    return 0;

  if (v < m)
    return v;
  extra_bit = AV1_READ_BITS_CHECKED (br, 1, retval);
  if (*retval != GST_AV1_PARSER_OK)
    return 0;

  return (v << 1) - m + extra_bit;
}

/* 4.10.4
 *
 * Unsigned little-endian n-byte number appearing directly in the
 * bitstream */
static guint
av1_bitstreamfn_le (GstBitReader * br, guint8 n, GstAV1ParserResult * retval)
{
  guint t = 0;
  guint8 byte;
  gint i;

  for (i = 0; i < n; i++) {
    byte = AV1_READ_BITS_CHECKED (br, 8, retval);
    if (*retval != GST_AV1_PARSER_OK)
      return 0;

    t += (byte << (i * 8));
  }
  return t;
}

/* 5.9.13
 *
 * Delta quantizer */
static gint8
av1_bitstreamfn_delta_q (GstBitReader * br, GstAV1ParserResult * retval)
{
  guint8 delta_coded;

  delta_coded = AV1_READ_BIT_CHECKED (br, retval);
  if (*retval != GST_AV1_PARSER_OK)
    return 0;

  if (delta_coded) {
    gint delta_q = av1_bitstreamfn_su (br, 7, retval);
    if (*retval != GST_AV1_PARSER_OK)
      return 0;
    return delta_q;
  } else {
    return 0;
  }

  return 0;
}

/* 5.3.4 */
static GstAV1ParserResult
av1_bitstreamfn_trailing_bits (GstBitReader * br, guint32 nbBits)
{
  guint8 trailing_one_bit, trailing_zero_bit;

  g_assert (nbBits);

  trailing_one_bit = AV1_READ_BIT (br);
  if (trailing_one_bit != 1) {
    return GST_AV1_PARSER_BITSTREAM_ERROR;
  }

  nbBits--;
  while (nbBits > 0) {
    trailing_zero_bit = AV1_READ_BIT (br);
    if (trailing_zero_bit != 0)
      return GST_AV1_PARSER_BITSTREAM_ERROR;
    nbBits--;
  }

  return GST_AV1_PARSER_OK;
}

static GstAV1ParserResult
av1_skip_trailing_bits (GstAV1Parser * parser, GstBitReader * br,
    GstAV1OBU * obu)
{
  guint32 payloadBits = gst_bit_reader_get_pos (br);
  GstAV1ParserResult ret;

  if (obu->obu_size > 0
      && obu->obu_type != GST_AV1_OBU_TILE_GROUP
      && obu->obu_type != GST_AV1_OBU_TILE_LIST
      && obu->obu_type != GST_AV1_OBU_FRAME) {
    if (payloadBits >= obu->obu_size * 8)
      return GST_AV1_PARSER_NO_MORE_DATA;

    ret = av1_bitstreamfn_trailing_bits (br, obu->obu_size * 8 - payloadBits);
    if (ret != GST_AV1_PARSER_OK)
      return ret;
  }
  return GST_AV1_PARSER_OK;
}

static gboolean
av1_seq_level_idx_is_valid (GstAV1SeqLevels seq_level_idx)
{
  return seq_level_idx == GST_AV1_SEQ_LEVEL_MAX
      || (seq_level_idx < GST_AV1_SEQ_LEVELS
      /* The following levels are currently undefined. */
      && seq_level_idx != GST_AV1_SEQ_LEVEL_2_2
      && seq_level_idx != GST_AV1_SEQ_LEVEL_2_3
      && seq_level_idx != GST_AV1_SEQ_LEVEL_3_2
      && seq_level_idx != GST_AV1_SEQ_LEVEL_3_3
      && seq_level_idx != GST_AV1_SEQ_LEVEL_4_2
      && seq_level_idx != GST_AV1_SEQ_LEVEL_4_3
      && seq_level_idx != GST_AV1_SEQ_LEVEL_7_0
      && seq_level_idx != GST_AV1_SEQ_LEVEL_7_1
      && seq_level_idx != GST_AV1_SEQ_LEVEL_7_2
      && seq_level_idx != GST_AV1_SEQ_LEVEL_7_3);
}

static void
av1_parser_init_sequence_header (GstAV1SequenceHeaderOBU * seq_header)
{
  memset (seq_header, 0, sizeof (*seq_header));
  seq_header->bit_depth = 8;
  seq_header->num_planes = 1;
}

/*************************************
 *                                   *
 * Parser Functions                  *
 *                                   *
 *************************************/
static void
gst_av1_parse_reset_state (GstAV1Parser * parser, gboolean free_sps)
{
  parser->state.begin_first_frame = FALSE;

  parser->state.prev_frame_id = 0;
  parser->state.current_frame_id = 0;
  memset (&parser->state.ref_info, 0, sizeof (parser->state.ref_info));
  parser->state.frame_width = 0;
  parser->state.frame_height = 0;
  parser->state.upscaled_width = 0;
  parser->state.mi_cols = 0;
  parser->state.mi_rows = 0;
  parser->state.render_width = 0;
  parser->state.render_height = 0;

  memset (parser->state.mi_col_starts, 0, sizeof (parser->state.mi_col_starts));
  memset (parser->state.mi_row_starts, 0, sizeof (parser->state.mi_row_starts));

  parser->state.tile_cols_log2 = 0;
  parser->state.tile_cols = 0;
  parser->state.tile_rows_log2 = 0;
  parser->state.tile_rows = 0;
  parser->state.tile_size_bytes = 0;

  parser->state.seen_frame_header = 0;

  if (free_sps) {
    parser->state.sequence_changed = FALSE;

    if (parser->seq_header) {
      g_free (parser->seq_header);
      parser->seq_header = NULL;
    }
  }
}

/**
 * gst_av1_parser_reset:
 * @parser: the #GstAV1Parser
 * @annex_b: indicate whether conforms to annex b
 *
 * Reset the current #GstAV1Parser's state totally.
 *
 * Since: 1.18
 */
void
gst_av1_parser_reset (GstAV1Parser * parser, gboolean annex_b)
{
  g_return_if_fail (parser != NULL);

  parser->annex_b = annex_b;
  if (parser->annex_b)
    gst_av1_parser_reset_annex_b (parser);

  gst_av1_parse_reset_state (parser, TRUE);
}

/**
 * gst_av1_parser_reset_annex_b:
 * @parser: the #GstAV1Parser
 *
 * Only reset the current #GstAV1Parser's annex b context.
 * The other part of the state is kept.
 *
 * Since: 1.20
 */
void
gst_av1_parser_reset_annex_b (GstAV1Parser * parser)
{
  g_return_if_fail (parser != NULL);
  g_return_if_fail (parser->annex_b);

  if (parser->temporal_unit_consumed < parser->temporal_unit_size)
    GST_DEBUG ("temporal_unit_consumed: %d, temporal_unit_size:%d, "
        "discard the left %d bytes for a temporal_unit.",
        parser->temporal_unit_consumed, parser->temporal_unit_size,
        parser->temporal_unit_size - parser->temporal_unit_consumed);

  if (parser->frame_unit_consumed < parser->frame_unit_size)
    GST_DEBUG (" frame_unit_consumed %d, frame_unit_size: %d "
        "discard the left %d bytes for a frame_unit.",
        parser->frame_unit_consumed, parser->frame_unit_size,
        parser->frame_unit_size - parser->frame_unit_consumed);

  parser->temporal_unit_consumed = 0;
  parser->temporal_unit_size = 0;
  parser->frame_unit_consumed = 0;
  parser->frame_unit_size = 0;
}

/* 5.3.2 */
static GstAV1ParserResult
gst_av1_parse_obu_header (GstAV1Parser * parser, GstBitReader * br,
    GstAV1OBUHeader * obu_header)
{
  guint8 obu_forbidden_bit;
  guint8 obu_reserved_1bit;
  guint8 obu_extension_header_reserved_3bits;
  GstAV1ParserResult ret = GST_AV1_PARSER_OK;

  if (AV1_REMAINING_BYTES (br) < 1) {
    ret = GST_AV1_PARSER_NO_MORE_DATA;
    goto error;
  }

  obu_forbidden_bit = AV1_READ_BIT (br);
  if (obu_forbidden_bit != 0) {
    ret = GST_AV1_PARSER_BITSTREAM_ERROR;
    goto error;
  }

  obu_header->obu_type = AV1_READ_BITS (br, 4);
  obu_header->obu_extention_flag = AV1_READ_BIT (br);
  obu_header->obu_has_size_field = AV1_READ_BIT (br);
  obu_reserved_1bit = AV1_READ_BIT (br);
  if (obu_reserved_1bit != 0) {
    ret = GST_AV1_PARSER_BITSTREAM_ERROR;
    goto error;
  }

  if (obu_header->obu_extention_flag) {
    if (AV1_REMAINING_BYTES (br) < 1) {
      ret = GST_AV1_PARSER_NO_MORE_DATA;
      goto error;
    }

    /* 5.3.3 OBU extension header */
    obu_header->obu_temporal_id = AV1_READ_BITS (br, 3);
    obu_header->obu_spatial_id = AV1_READ_BITS (br, 2);
    obu_extension_header_reserved_3bits = AV1_READ_BITS (br, 3);
    if (obu_extension_header_reserved_3bits != 0) {
      ret = GST_AV1_PARSER_BITSTREAM_ERROR;
      goto error;
    }
  }

  return GST_AV1_PARSER_OK;

error:
  GST_WARNING ("parse OBU header error %d", ret);
  return ret;
}

/**
 * gst_av1_parser_identify_one_obu:
 * @parser: the #GstAV1Parser
 * @data: the data to parse
 * @size: the size of @data
 * @obu: a #GstAV1OBU to store the identified result
 * @consumed: (out): the consumed data size
 *
 * Identify one @obu's type from the incoming data stream. This function
 * should be called first to know the type of @obu before other parse APIs.
 *
 * Returns: The #GstAV1ParserResult.
 *
 * Since: 1.18
 */
GstAV1ParserResult
gst_av1_parser_identify_one_obu (GstAV1Parser * parser, const guint8 * data,
    guint32 size, GstAV1OBU * obu, guint32 * consumed)
{
  GstAV1ParserResult ret = GST_AV1_PARSER_OK;
  GstBitReader br;
  guint obu_length = 0;
  guint32 used;

  g_return_val_if_fail (parser != NULL, GST_AV1_PARSER_INVALID_OPERATION);
  g_return_val_if_fail (obu != NULL, GST_AV1_PARSER_INVALID_OPERATION);
  g_return_val_if_fail (data != NULL, GST_AV1_PARSER_INVALID_OPERATION);
  g_return_val_if_fail (consumed != NULL, GST_AV1_PARSER_INVALID_OPERATION);

  *consumed = 0;
  memset (obu, 0, sizeof (*obu));

  if (parser->annex_b) {
    GST_LOG ("temporal_unit_consumed: %d, temporal_unit_size:%d,"
        " frame_unit_consumed %d, frame_unit_size: %d",
        parser->temporal_unit_consumed, parser->temporal_unit_size,
        parser->frame_unit_consumed, parser->frame_unit_size);
  }

  if (!size) {
    ret = GST_AV1_PARSER_NO_MORE_DATA;
    goto error;
  }

  /* parse the data size if annex_b */
  if (parser->annex_b) {
    guint last_pos;
  annex_b_again:
    last_pos = 0;

    if (*consumed > size)
      goto error;
    if (*consumed == size) {
      ret = GST_AV1_PARSER_NO_MORE_DATA;
      goto error;
    }
    gst_bit_reader_init (&br, data + *consumed, size - *consumed);

    if (parser->temporal_unit_consumed > parser->temporal_unit_size)
      goto error;

    if (parser->temporal_unit_consumed &&
        parser->temporal_unit_consumed == parser->temporal_unit_size) {
      GST_LOG ("Complete a temporal unit of size %d",
          parser->temporal_unit_size);
      parser->temporal_unit_consumed = parser->temporal_unit_size = 0;
    }

    if (parser->temporal_unit_size == 0) {
      parser->temporal_unit_size = av1_bitstreamfn_leb128 (&br, &ret);
      if (ret != GST_AV1_PARSER_OK)
        goto error;

      g_assert (gst_bit_reader_get_pos (&br) % 8 == 0);
      used = (gst_bit_reader_get_pos (&br) / 8 - last_pos);
      last_pos = gst_bit_reader_get_pos (&br) / 8;
      *consumed += used;

      if (parser->temporal_unit_consumed == parser->temporal_unit_size) {
        /* Some extreme case like a temporal unit just
           hold a temporal_unit_size = 0 */
        goto annex_b_again;
      }
    }

    if (parser->frame_unit_consumed > parser->frame_unit_size)
      goto error;

    if (parser->frame_unit_consumed &&
        parser->frame_unit_consumed == parser->frame_unit_size) {
      GST_LOG ("Complete a frame unit of size %d", parser->frame_unit_size);
      parser->frame_unit_size = parser->frame_unit_consumed = 0;
    }

    if (parser->frame_unit_size == 0) {
      parser->frame_unit_size = av1_bitstreamfn_leb128 (&br, &ret);
      if (ret != GST_AV1_PARSER_OK)
        goto error;

      g_assert (gst_bit_reader_get_pos (&br) % 8 == 0);
      used = gst_bit_reader_get_pos (&br) / 8 - last_pos;
      last_pos = gst_bit_reader_get_pos (&br) / 8;
      *consumed += used;
      parser->temporal_unit_consumed += used;

      if (parser->frame_unit_size >
          parser->temporal_unit_size - parser->temporal_unit_consumed) {
        GST_INFO ("Error stream, frame unit size %d, bigger than the left"
            "temporal unit size %d", parser->frame_unit_size,
            parser->temporal_unit_size - parser->temporal_unit_consumed);
        ret = GST_AV1_PARSER_BITSTREAM_ERROR;
        goto error;
      }

      if (parser->temporal_unit_consumed == parser->temporal_unit_size ||
          parser->frame_unit_consumed == parser->frame_unit_size) {
        /* Some extreme case like a temporal unit just hold a frame_unit_size,
           or a frame unit just hold frame_unit_size = 0 */
        goto annex_b_again;
      }
    }

    obu_length = av1_bitstreamfn_leb128 (&br, &ret);
    if (ret != GST_AV1_PARSER_OK)
      goto error;

    if (obu_length > parser->frame_unit_size - parser->frame_unit_consumed) {
      GST_INFO ("Error stream, obu_length is %d, bigger than the left"
          "frame unit size %d", obu_length,
          parser->frame_unit_size - parser->frame_unit_consumed);
      ret = GST_AV1_PARSER_BITSTREAM_ERROR;
      goto error;
    }
    /* update the consumed */
    used = gst_bit_reader_get_pos (&br) / 8 - last_pos;
    last_pos = gst_bit_reader_get_pos (&br) / 8;
    *consumed += used;
    parser->temporal_unit_consumed += used;
    parser->frame_unit_consumed += used;

    if (obu_length == 0) {
      /* An empty obu? let continue to the next */
      return GST_AV1_PARSER_DROP;
    }
  }

  if (*consumed > size)
    goto error;
  if (*consumed == size) {
    ret = GST_AV1_PARSER_NO_MORE_DATA;
    goto error;
  }
  gst_bit_reader_init (&br, data + *consumed, size - *consumed);

  ret = gst_av1_parse_obu_header (parser, &br, &obu->header);
  if (ret != GST_AV1_PARSER_OK)
    goto error;

  obu->obu_type = obu->header.obu_type;
  GST_LOG ("identify obu type is %d", obu->obu_type);

  if (obu->header.obu_has_size_field) {
    guint size_sz = gst_bit_reader_get_pos (&br) / 8;

    obu->obu_size = av1_bitstreamfn_leb128 (&br, &ret);
    if (ret != GST_AV1_PARSER_OK)
      goto error;

    size_sz = gst_bit_reader_get_pos (&br) / 8 - size_sz;
    if (obu_length
        && obu_length - 1 - obu->header.obu_extention_flag - size_sz !=
        obu->obu_size) {
      /* If obu_size and obu_length are both present, but inconsistent,
         then the packed bitstream is deemed invalid. */
      ret = GST_AV1_PARSER_BITSTREAM_ERROR;
      goto error;
    }

    if (AV1_REMAINING_BYTES (&br) < obu->obu_size) {
      ret = GST_AV1_PARSER_NO_MORE_DATA;
      goto error;
    }
  } else {
    if (obu_length == 0) {
      ret = GST_AV1_PARSER_BITSTREAM_ERROR;
      goto error;
    }

    obu->obu_size = obu_length - 1 - obu->header.obu_extention_flag;
  }

  g_assert (gst_bit_reader_get_pos (&br) % 8 == 0);
  used = gst_bit_reader_get_pos (&br) / 8;
  /* fail if not a complete obu */
  if (size - *consumed - used < obu->obu_size) {
    ret = GST_AV1_PARSER_NO_MORE_DATA;
    goto error;
  }

  /* update the consumed */
  *consumed += used;
  if (parser->annex_b) {
    parser->temporal_unit_consumed += used;
    parser->frame_unit_consumed += used;
  }

  obu->data = (guint8 *) (data + *consumed);

  *consumed += obu->obu_size;
  if (parser->annex_b) {
    parser->temporal_unit_consumed += obu->obu_size;
    parser->frame_unit_consumed += obu->obu_size;
  }

  if (obu->obu_type != GST_AV1_OBU_SEQUENCE_HEADER
      && obu->obu_type != GST_AV1_OBU_TEMPORAL_DELIMITER
      && parser->state.operating_point_idc && obu->header.obu_extention_flag) {
    guint32 inTemporalLayer =
        (parser->state.operating_point_idc >> obu->header.obu_temporal_id) & 1;
    guint32 inSpatialLayer =
        (parser->state.operating_point_idc >> (obu->header.obu_spatial_id +
            8)) & 1;
    if (!inTemporalLayer || !inSpatialLayer) {
      return GST_AV1_PARSER_DROP;
    }
  }

  return GST_AV1_PARSER_OK;

error:
  GST_WARNING ("can not identify obu, error %d", ret);
  return ret;
}

/* 5.5.2 */
static GstAV1ParserResult
gst_av1_parse_color_config (GstAV1Parser * parser, GstBitReader * br,
    GstAV1SequenceHeaderOBU * seq_header, GstAV1ColorConfig * color_config)
{
  GstAV1ParserResult ret = GST_AV1_PARSER_OK;

  color_config->high_bitdepth = AV1_READ_BIT_CHECKED (br, &ret);
  if (ret != GST_AV1_PARSER_OK)
    goto error;

  if (seq_header->seq_profile == GST_AV1_PROFILE_2
      && color_config->high_bitdepth) {
    color_config->twelve_bit = AV1_READ_BIT_CHECKED (br, &ret);
    if (ret != GST_AV1_PARSER_OK)
      goto error;

    seq_header->bit_depth = color_config->twelve_bit ? 12 : 10;
  } else if (seq_header->seq_profile <= GST_AV1_PROFILE_2) {
    seq_header->bit_depth = color_config->high_bitdepth ? 10 : 8;
  } else {
    GST_INFO ("Unsupported profile/bit-depth combination");
    ret = GST_AV1_PARSER_BITSTREAM_ERROR;
    goto error;
  }

  if (seq_header->seq_profile == GST_AV1_PROFILE_1)
    color_config->mono_chrome = 0;
  else {
    color_config->mono_chrome = AV1_READ_BIT_CHECKED (br, &ret);
    if (ret != GST_AV1_PARSER_OK)
      goto error;
  }
  seq_header->num_planes = color_config->mono_chrome ? 1 : 3;

  color_config->color_description_present_flag =
      AV1_READ_BIT_CHECKED (br, &ret);
  if (ret != GST_AV1_PARSER_OK)
    goto error;

  if (color_config->color_description_present_flag) {
    if (AV1_REMAINING_BITS (br) < 8 + 8 + 8) {
      ret = GST_AV1_PARSER_NO_MORE_DATA;
      goto error;
    }
    color_config->color_primaries = AV1_READ_BITS (br, 8);
    color_config->transfer_characteristics = AV1_READ_BITS (br, 8);
    color_config->matrix_coefficients = AV1_READ_BITS (br, 8);
  } else {
    color_config->color_primaries = GST_AV1_CP_UNSPECIFIED;
    color_config->transfer_characteristics = GST_AV1_TC_UNSPECIFIED;
    color_config->matrix_coefficients = GST_AV1_MC_UNSPECIFIED;
  }

  if (color_config->mono_chrome) {
    color_config->color_range = AV1_READ_BIT_CHECKED (br, &ret);
    if (ret != GST_AV1_PARSER_OK)
      goto error;

    color_config->subsampling_x = 1;
    color_config->subsampling_y = 1;
    color_config->chroma_sample_position = GST_AV1_CSP_UNKNOWN;
    color_config->separate_uv_delta_q = 0;
    goto success;
  } else if (color_config->color_primaries == GST_AV1_CP_BT_709 &&
      color_config->transfer_characteristics == GST_AV1_TC_SRGB &&
      color_config->matrix_coefficients == GST_AV1_MC_IDENTITY) {
    color_config->color_range = 1;
    color_config->subsampling_x = 0;
    color_config->subsampling_y = 0;
    if (!(seq_header->seq_profile == GST_AV1_PROFILE_1 ||
            (seq_header->seq_profile == GST_AV1_PROFILE_2
                && seq_header->bit_depth == 12))) {
      GST_INFO ("sRGB colorspace not compatible with specified profile");
      ret = GST_AV1_PARSER_BITSTREAM_ERROR;
      goto error;
    }
  } else {
    color_config->color_range = AV1_READ_BIT_CHECKED (br, &ret);
    if (ret != GST_AV1_PARSER_OK)
      goto error;

    if (seq_header->seq_profile == GST_AV1_PROFILE_0) {
      /* 420 only */
      color_config->subsampling_x = 1;
      color_config->subsampling_y = 1;
    } else if (seq_header->seq_profile == GST_AV1_PROFILE_1) {
      /* 444 only */
      color_config->subsampling_x = 0;
      color_config->subsampling_y = 0;
    } else {
      g_assert (seq_header->seq_profile == GST_AV1_PROFILE_2);
      if (seq_header->bit_depth == 12) {
        color_config->subsampling_x = AV1_READ_BIT_CHECKED (br, &ret);
        if (ret != GST_AV1_PARSER_OK)
          goto error;

        if (color_config->subsampling_x) {
          /* 422 or 420 */
          color_config->subsampling_y = AV1_READ_BIT_CHECKED (br, &ret);
          if (ret != GST_AV1_PARSER_OK)
            goto error;
        } else
          /* 444 */
          color_config->subsampling_y = 0;
      } else {
        /* 422 */
        color_config->subsampling_x = 1;
        color_config->subsampling_y = 0;
      }
    }

    if (color_config->matrix_coefficients == GST_AV1_MC_IDENTITY &&
        (color_config->subsampling_x || color_config->subsampling_y)) {
      GST_INFO ("Identity CICP Matrix incompatible with"
          " non 4:4:4 color sampling");
      ret = GST_AV1_PARSER_BITSTREAM_ERROR;
      goto error;
    }

    if (color_config->subsampling_x && color_config->subsampling_y) {
      color_config->chroma_sample_position =
          AV1_READ_BITS_CHECKED (br, 2, &ret);
      if (ret != GST_AV1_PARSER_OK)
        goto error;
    }
  }

  color_config->separate_uv_delta_q = AV1_READ_BIT_CHECKED (br, &ret);
  if (ret != GST_AV1_PARSER_OK)
    goto error;

  if (!(color_config->subsampling_x == 0 && color_config->subsampling_y == 0) &&
      !(color_config->subsampling_x == 1 && color_config->subsampling_y == 1) &&
      !(color_config->subsampling_x == 1 && color_config->subsampling_y == 0)) {
    GST_INFO ("Only 4:4:4, 4:2:2 and 4:2:0 are currently supported, "
        "%d %d subsampling is not supported.\n",
        color_config->subsampling_x, color_config->subsampling_y);
    ret = GST_AV1_PARSER_BITSTREAM_ERROR;
    goto error;
  }

success:
  return GST_AV1_PARSER_OK;

error:
  GST_WARNING ("parse color config error %d", ret);
  return ret;
}

/* 5.5.3 */
static GstAV1ParserResult
gst_av1_parse_timing_info (GstAV1Parser * parser, GstBitReader * br,
    GstAV1TimingInfo * timing_info)
{
  GstAV1ParserResult ret = GST_AV1_PARSER_OK;

  if (AV1_REMAINING_BITS (br) < 32 + 32 + 1) {
    ret = GST_AV1_PARSER_NO_MORE_DATA;
    goto error;
  }

  timing_info->num_units_in_display_tick = AV1_READ_UINT32 (br);
  timing_info->time_scale = AV1_READ_UINT32 (br);
  if (timing_info->num_units_in_display_tick == 0 ||
      timing_info->time_scale == 0) {
    ret = GST_AV1_PARSER_BITSTREAM_ERROR;
    goto error;
  }

  timing_info->equal_picture_interval = AV1_READ_BIT (br);
  if (timing_info->equal_picture_interval) {
    timing_info->num_ticks_per_picture_minus_1 =
        av1_bitstreamfn_uvlc (br, &ret);
    if (ret != GST_AV1_PARSER_OK)
      goto error;

    if (timing_info->num_ticks_per_picture_minus_1 == G_MAXUINT) {
      ret = GST_AV1_PARSER_BITSTREAM_ERROR;
      goto error;
    }
  }

  return GST_AV1_PARSER_OK;

error:
  GST_WARNING ("parse timing info error %d", ret);
  return ret;
}

/* 5.5.4 */
static GstAV1ParserResult
gst_av1_parse_decoder_model_info (GstAV1Parser * parser, GstBitReader * br,
    GstAV1DecoderModelInfo * decoder_model_info)
{
  if (AV1_REMAINING_BITS (br) < 5 + 32 + 5 + 5)
    return GST_AV1_PARSER_NO_MORE_DATA;

  decoder_model_info->buffer_delay_length_minus_1 = AV1_READ_BITS (br, 5);
  decoder_model_info->num_units_in_decoding_tick = AV1_READ_BITS (br, 32);
  decoder_model_info->buffer_removal_time_length_minus_1 =
      AV1_READ_BITS (br, 5);
  decoder_model_info->frame_presentation_time_length_minus_1 =
      AV1_READ_BITS (br, 5);

  return GST_AV1_PARSER_OK;
}

/* 5.5.5 */
static GstAV1ParserResult
gst_av1_parse_operating_parameters_info (GstAV1Parser * parser,
    GstBitReader * br, GstAV1SequenceHeaderOBU * seq_header,
    GstAV1OperatingPoint * op_point)
{
  guint32 n = seq_header->decoder_model_info.buffer_delay_length_minus_1 + 1;

  if (AV1_REMAINING_BITS (br) < n + n + 1)
    return GST_AV1_PARSER_NO_MORE_DATA;

  op_point->decoder_buffer_delay = AV1_READ_BITS (br, n);
  op_point->encoder_buffer_delay = AV1_READ_BITS (br, n);
  op_point->low_delay_mode_flag = AV1_READ_BIT (br);
  return GST_AV1_PARSER_OK;
}

/* 5.5.1 General sequence header OBU */
/**
 * gst_av1_parser_parse_sequence_header_obu:
 * @parser: the #GstAV1Parser
 * @obu: a #GstAV1OBU to be parsed
 * @seq_header: a #GstAV1SequenceHeaderOBU to store the parsed result.
 *
 * Parse one sequence header @obu based on the @parser context, store the
 * result in the @seq_header.
 *
 * Returns: The #GstAV1ParserResult.
 *
 * Since: 1.18
 */
GstAV1ParserResult
gst_av1_parser_parse_sequence_header_obu (GstAV1Parser * parser,
    GstAV1OBU * obu, GstAV1SequenceHeaderOBU * seq_header)
{
  GstAV1ParserResult retval = GST_AV1_PARSER_OK;
  gint i;
  GstBitReader bit_reader;
  GstBitReader *br = &bit_reader;

  g_return_val_if_fail (parser != NULL, GST_AV1_PARSER_INVALID_OPERATION);
  g_return_val_if_fail (obu != NULL, GST_AV1_PARSER_INVALID_OPERATION);
  g_return_val_if_fail (obu->obu_type == GST_AV1_OBU_SEQUENCE_HEADER,
      GST_AV1_PARSER_INVALID_OPERATION);
  g_return_val_if_fail (seq_header != NULL, GST_AV1_PARSER_INVALID_OPERATION);

  av1_parser_init_sequence_header (seq_header);
  gst_bit_reader_init (br, obu->data, obu->obu_size);

  if (AV1_REMAINING_BITS (br) < 8) {
    retval = GST_AV1_PARSER_NO_MORE_DATA;
    goto error;
  }

  seq_header->seq_profile = AV1_READ_BITS (br, 3);
  if (seq_header->seq_profile > GST_AV1_PROFILE_2) {
    GST_INFO ("Unsupported profile %d", seq_header->seq_profile);
    retval = GST_AV1_PARSER_BITSTREAM_ERROR;
    goto error;
  }

  seq_header->still_picture = AV1_READ_BIT (br);
  seq_header->reduced_still_picture_header = AV1_READ_BIT (br);
  if (!seq_header->still_picture && seq_header->reduced_still_picture_header) {
    GST_INFO (" If reduced_still_picture_header is equal to 1, it is a"
        " requirement of bitstream conformance that still_picture is equal"
        " to 1. ");
    retval = GST_AV1_PARSER_BITSTREAM_ERROR;
    goto error;
  }

  if (seq_header->reduced_still_picture_header) {
    seq_header->timing_info_present_flag = 0;
    seq_header->decoder_model_info_present_flag = 0;
    seq_header->initial_display_delay_present_flag = 0;
    seq_header->operating_points_cnt_minus_1 = 0;
    seq_header->operating_points[0].idc = 0;
    seq_header->operating_points[0].seq_level_idx = AV1_READ_BITS (br, 5);
    if (!av1_seq_level_idx_is_valid
        (seq_header->operating_points[0].seq_level_idx)) {
      GST_INFO ("The seq_level_idx is unsupported");
      retval = GST_AV1_PARSER_BITSTREAM_ERROR;
      goto error;
    }
    seq_header->operating_points[0].seq_tier = 0;
    seq_header->operating_points[0].decoder_model_present_for_this_op = 0;
    seq_header->operating_points[0].initial_display_delay_present_for_this_op =
        0;
  } else {
    seq_header->timing_info_present_flag = AV1_READ_BIT (br);

    if (seq_header->timing_info_present_flag) {
      retval =
          gst_av1_parse_timing_info (parser, br, &(seq_header->timing_info));
      if (retval != GST_AV1_PARSER_OK)
        goto error;

      seq_header->decoder_model_info_present_flag =
          AV1_READ_BIT_CHECKED (br, &retval);
      if (retval != GST_AV1_PARSER_OK)
        goto error;

      if (seq_header->decoder_model_info_present_flag) {
        retval = gst_av1_parse_decoder_model_info (parser, br,
            &(seq_header->decoder_model_info));
        if (retval != GST_AV1_PARSER_OK)
          goto error;
      }
    } else {
      seq_header->decoder_model_info_present_flag = 0;
    }

    if (AV1_REMAINING_BITS (br) < 6) {
      retval = GST_AV1_PARSER_NO_MORE_DATA;
      goto error;
    }
    seq_header->initial_display_delay_present_flag = AV1_READ_BIT (br);
    seq_header->operating_points_cnt_minus_1 = AV1_READ_BITS (br, 5);
    if (seq_header->operating_points_cnt_minus_1 + 1 >
        GST_AV1_MAX_OPERATING_POINTS) {
      GST_INFO ("The operating points number %d is too big",
          seq_header->operating_points_cnt_minus_1 + 1);
      retval = GST_AV1_PARSER_BITSTREAM_ERROR;
      goto error;
    }

    for (i = 0; i < seq_header->operating_points_cnt_minus_1 + 1; i++) {
      if (AV1_REMAINING_BITS (br) < 17) {
        retval = GST_AV1_PARSER_NO_MORE_DATA;
        goto error;
      }
      seq_header->operating_points[i].idc = AV1_READ_BITS (br, 12);
      seq_header->operating_points[i].seq_level_idx = AV1_READ_BITS (br, 5);
      if (!av1_seq_level_idx_is_valid
          (seq_header->operating_points[i].seq_level_idx)) {
        GST_INFO ("The seq_level_idx is unsupported");
        retval = GST_AV1_PARSER_BITSTREAM_ERROR;
        goto error;
      }
      if (seq_header->operating_points[i].seq_level_idx > GST_AV1_SEQ_LEVEL_3_3) {
        seq_header->operating_points[i].seq_tier = AV1_READ_BIT (br);
      } else {
        seq_header->operating_points[i].seq_tier = 0;
      }
      if (seq_header->decoder_model_info_present_flag) {
        seq_header->operating_points[i].decoder_model_present_for_this_op =
            AV1_READ_BIT (br);
        if (seq_header->operating_points[i].decoder_model_present_for_this_op)
          retval =
              gst_av1_parse_operating_parameters_info (parser, br, seq_header,
              &(seq_header->operating_points[i]));
        if (retval != GST_AV1_PARSER_OK)
          goto error;
      } else {
        seq_header->operating_points[i].decoder_model_present_for_this_op = 0;
      }

      if (seq_header->initial_display_delay_present_flag) {
        seq_header->
            operating_points[i].initial_display_delay_present_for_this_op =
            AV1_READ_BIT_CHECKED (br, &retval);
        if (retval != GST_AV1_PARSER_OK)
          goto error;

        if (seq_header->
            operating_points[i].initial_display_delay_present_for_this_op) {
          seq_header->operating_points[i].initial_display_delay_minus_1 =
              AV1_READ_BITS_CHECKED (br, 4, &retval);
          if (retval != GST_AV1_PARSER_OK)
            goto error;

          if (seq_header->operating_points[i].initial_display_delay_minus_1 +
              1 > 10) {
            GST_INFO ("AV1 does not support more than 10 decoded frames delay");
            retval = GST_AV1_PARSER_BITSTREAM_ERROR;
            goto error;
          }
        } else {
          seq_header->operating_points[i].initial_display_delay_minus_1 = 9;
        }
      } else {
        seq_header->
            operating_points[i].initial_display_delay_present_for_this_op = 0;
        seq_header->operating_points[i].initial_display_delay_minus_1 = 9;
      }
    }
  }

  /* Let user decide the operatingPoint,
     implemented by calling gst_av1_parser_set_operating_point()
     operatingPoint = choose_operating_point( )
     operating_point_idc = operating_point_idc[ operatingPoint ] */

  if (AV1_REMAINING_BITS (br) < 4 + 4 +
      (seq_header->frame_width_bits_minus_1 + 1) +
      (seq_header->frame_height_bits_minus_1 + 1)) {
    retval = GST_AV1_PARSER_NO_MORE_DATA;
    goto error;
  }

  seq_header->frame_width_bits_minus_1 = AV1_READ_BITS (br, 4);
  seq_header->frame_height_bits_minus_1 = AV1_READ_BITS (br, 4);
  seq_header->max_frame_width_minus_1 =
      AV1_READ_BITS (br, seq_header->frame_width_bits_minus_1 + 1);
  seq_header->max_frame_height_minus_1 =
      AV1_READ_BITS (br, seq_header->frame_height_bits_minus_1 + 1);

  if (seq_header->reduced_still_picture_header)
    seq_header->frame_id_numbers_present_flag = 0;
  else {
    seq_header->frame_id_numbers_present_flag =
        AV1_READ_BIT_CHECKED (br, &retval);
    if (retval != GST_AV1_PARSER_OK)
      goto error;
  }

  if (seq_header->frame_id_numbers_present_flag) {
    if (AV1_REMAINING_BITS (br) < 4 + 3) {
      retval = GST_AV1_PARSER_NO_MORE_DATA;
      goto error;
    }
    seq_header->delta_frame_id_length_minus_2 = AV1_READ_BITS (br, 4);
    seq_header->additional_frame_id_length_minus_1 = AV1_READ_BITS (br, 3);

    if (seq_header->additional_frame_id_length_minus_1 + 1 +
        seq_header->delta_frame_id_length_minus_2 + 2 > 16) {
      GST_INFO ("Invalid frame_id_length");
      retval = GST_AV1_PARSER_BITSTREAM_ERROR;
      goto error;
    }
  }

  if (AV1_REMAINING_BITS (br) < 3) {
    retval = GST_AV1_PARSER_NO_MORE_DATA;
    goto error;
  }
  seq_header->use_128x128_superblock = AV1_READ_BIT (br);
  seq_header->enable_filter_intra = AV1_READ_BIT (br);
  seq_header->enable_intra_edge_filter = AV1_READ_BIT (br);

  if (seq_header->reduced_still_picture_header) {
    seq_header->enable_interintra_compound = 0;
    seq_header->enable_masked_compound = 0;
    seq_header->enable_warped_motion = 0;
    seq_header->enable_dual_filter = 0;
    seq_header->enable_order_hint = 0;
    seq_header->enable_jnt_comp = 0;
    seq_header->enable_ref_frame_mvs = 0;
    seq_header->seq_force_screen_content_tools =
        GST_AV1_SELECT_SCREEN_CONTENT_TOOLS;
    seq_header->seq_force_integer_mv = GST_AV1_SELECT_INTEGER_MV;
    seq_header->order_hint_bits_minus_1 = -1;
    seq_header->order_hint_bits = 0;
  } else {
    if (AV1_REMAINING_BITS (br) < 5) {
      retval = GST_AV1_PARSER_NO_MORE_DATA;
      goto error;
    }
    seq_header->enable_interintra_compound = AV1_READ_BIT (br);
    seq_header->enable_masked_compound = AV1_READ_BIT (br);
    seq_header->enable_warped_motion = AV1_READ_BIT (br);
    seq_header->enable_dual_filter = AV1_READ_BIT (br);
    seq_header->enable_order_hint = AV1_READ_BIT (br);
    if (seq_header->enable_order_hint) {
      if (AV1_REMAINING_BITS (br) < 2) {
        retval = GST_AV1_PARSER_NO_MORE_DATA;
        goto error;
      }
      seq_header->enable_jnt_comp = AV1_READ_BIT (br);
      seq_header->enable_ref_frame_mvs = AV1_READ_BIT (br);
    } else {
      seq_header->enable_jnt_comp = 0;
      seq_header->enable_ref_frame_mvs = 0;
    }

    seq_header->seq_choose_screen_content_tools =
        AV1_READ_BIT_CHECKED (br, &retval);
    if (retval != GST_AV1_PARSER_OK)
      goto error;
    if (seq_header->seq_choose_screen_content_tools)
      seq_header->seq_force_screen_content_tools =
          GST_AV1_SELECT_SCREEN_CONTENT_TOOLS;
    else {
      seq_header->seq_force_screen_content_tools =
          AV1_READ_BIT_CHECKED (br, &retval);
      if (retval != GST_AV1_PARSER_OK)
        goto error;
    }

    if (seq_header->seq_force_screen_content_tools > 0) {
      seq_header->seq_choose_integer_mv = AV1_READ_BIT_CHECKED (br, &retval);
      if (retval != GST_AV1_PARSER_OK)
        goto error;
      if (seq_header->seq_choose_integer_mv)
        seq_header->seq_force_integer_mv = GST_AV1_SELECT_INTEGER_MV;
      else {
        seq_header->seq_force_integer_mv = AV1_READ_BIT_CHECKED (br, &retval);
        if (retval != GST_AV1_PARSER_OK)
          goto error;
      }
    } else {
      seq_header->seq_force_integer_mv = GST_AV1_SELECT_INTEGER_MV;
    }
    if (seq_header->enable_order_hint) {
      seq_header->order_hint_bits_minus_1 =
          AV1_READ_BITS_CHECKED (br, 3, &retval);
      if (retval != GST_AV1_PARSER_OK)
        goto error;
      seq_header->order_hint_bits = seq_header->order_hint_bits_minus_1 + 1;
    } else {
      seq_header->order_hint_bits_minus_1 = -1;
      seq_header->order_hint_bits = 0;
    }
  }

  if (AV1_REMAINING_BITS (br) < 3) {
    retval = GST_AV1_PARSER_NO_MORE_DATA;
    goto error;
  }
  seq_header->enable_superres = AV1_READ_BIT (br);
  seq_header->enable_cdef = AV1_READ_BIT (br);
  seq_header->enable_restoration = AV1_READ_BIT (br);

  retval = gst_av1_parse_color_config (parser, br, seq_header,
      &seq_header->color_config);
  if (retval != GST_AV1_PARSER_OK)
    goto error;

  seq_header->film_grain_params_present = AV1_READ_BIT_CHECKED (br, &retval);
  if (retval != GST_AV1_PARSER_OK)
    goto error;

  retval = av1_skip_trailing_bits (parser, br, obu);
  if (retval != GST_AV1_PARSER_OK)
    goto error;

  if (parser->seq_header) {
    if (!memcmp (parser->seq_header, seq_header,
            sizeof (GstAV1SequenceHeaderOBU)))
      goto success;

    g_free (parser->seq_header);
  }

  parser->seq_header = g_memdup2 (seq_header, sizeof (GstAV1SequenceHeaderOBU));
  gst_av1_parse_reset_state (parser, FALSE);

  /* choose_operating_point() set the operating_point */
  if (parser->state.operating_point > seq_header->operating_points_cnt_minus_1) {
    GST_WARNING ("Invalid operating_point %d set by user, just use 0",
        parser->state.operating_point);
    parser->state.operating_point_idc = seq_header->operating_points[0].idc;
  } else {
    parser->state.operating_point_idc =
        seq_header->operating_points[parser->state.operating_point].idc;
  }

  parser->state.sequence_changed = TRUE;

success:
  return GST_AV1_PARSER_OK;

error:
  GST_WARNING ("parse sequence header error %d", retval);
  return retval;
}

/* 5.6 */
/**
 * gst_av1_parser_parse_temporal_delimiter_obu:
 * @parser: the #GstAV1Parser
 * @obu: a #GstAV1OBU to be parsed
 *
 * Parse one temporal delimiter @obu based on the @parser context.
 * The temporal delimiter is just delimiter and contains no content.
 *
 * Returns: The #GstAV1ParserResult.
 *
 * Since: 1.18
 */
GstAV1ParserResult
gst_av1_parser_parse_temporal_delimiter_obu (GstAV1Parser * parser,
    GstAV1OBU * obu)
{
  GstBitReader bit_reader;
  GstAV1ParserResult ret;

  g_return_val_if_fail (parser != NULL, GST_AV1_PARSER_INVALID_OPERATION);
  g_return_val_if_fail (obu != NULL, GST_AV1_PARSER_INVALID_OPERATION);
  g_return_val_if_fail (obu->obu_type == GST_AV1_OBU_TEMPORAL_DELIMITER,
      GST_AV1_PARSER_INVALID_OPERATION);

  gst_bit_reader_init (&bit_reader, obu->data, obu->obu_size);

  parser->state.seen_frame_header = 0;

  ret = av1_skip_trailing_bits (parser, &bit_reader, obu);
  if (ret != GST_AV1_PARSER_OK)
    GST_WARNING ("parse temporal delimiter error %d", ret);

  return ret;
}

/* 5.8.2 */
static GstAV1ParserResult
gst_av1_parse_metadata_itut_t35 (GstAV1Parser * parser, GstBitReader * br,
    GstAV1MetadataITUT_T35 * itut_t35)
{
  GstAV1ParserResult ret;

  itut_t35->itu_t_t35_country_code = AV1_READ_BITS_CHECKED (br, 8, &ret);
  if (ret != GST_AV1_PARSER_OK)
    return ret;

  if (itut_t35->itu_t_t35_country_code == 0xFF) {
    itut_t35->itu_t_t35_country_code_extention_byte =
        AV1_READ_BITS_CHECKED (br, 8, &ret);
    if (ret != GST_AV1_PARSER_OK)
      return ret;
  }
  /* itu_t_t35_payload_bytes is not defined in specification.
     Just skip this part. */

  return GST_AV1_PARSER_OK;
}

/* 5.8.3 */
static GstAV1ParserResult
gst_av1_parse_metadata_hdr_cll (GstAV1Parser * parser, GstBitReader * br,
    GstAV1MetadataHdrCll * hdr_cll)
{
  if (AV1_REMAINING_BITS (br) < 32)
    return GST_AV1_PARSER_NO_MORE_DATA;

  hdr_cll->max_cll = AV1_READ_UINT16 (br);
  hdr_cll->max_fall = AV1_READ_UINT16 (br);

  return GST_AV1_PARSER_OK;
}

/* 5.8.4 */
static GstAV1ParserResult
gst_av1_parse_metadata_hdr_mdcv (GstAV1Parser * parser, GstBitReader * br,
    GstAV1MetadataHdrMdcv * hdr_mdcv)
{
  gint i;

  if (AV1_REMAINING_BITS (br) < 3 * (16 + 16) + 16 + 16 + 32 + 32)
    return GST_AV1_PARSER_NO_MORE_DATA;

  for (i = 0; i < 3; i++) {
    hdr_mdcv->primary_chromaticity_x[i] = AV1_READ_UINT16 (br);
    hdr_mdcv->primary_chromaticity_y[i] = AV1_READ_UINT16 (br);
  }

  hdr_mdcv->white_point_chromaticity_x = AV1_READ_UINT16 (br);
  hdr_mdcv->white_point_chromaticity_y = AV1_READ_UINT16 (br);

  hdr_mdcv->luminance_max = AV1_READ_UINT32 (br);
  hdr_mdcv->luminance_min = AV1_READ_UINT32 (br);

  return GST_AV1_PARSER_OK;
}

/* 5.8.5 */
static GstAV1ParserResult
gst_av1_parse_metadata_scalability (GstAV1Parser * parser,
    GstBitReader * br, GstAV1MetadataScalability * scalability)
{
  gint i, j;
  GstAV1ParserResult ret = GST_AV1_PARSER_OK;
  guint8 scalability_structure_reserved_3bits;

  scalability->scalability_mode_idc = AV1_READ_UINT8_CHECKED (br, &ret);
  if (ret != GST_AV1_PARSER_OK)
    goto error;

  if (scalability->scalability_mode_idc != GST_AV1_SCALABILITY_SS)
    goto success;

  if (AV1_REMAINING_BITS (br) < 8) {
    ret = GST_AV1_PARSER_NO_MORE_DATA;
    goto error;
  }

  /* 5.8.6 */
  scalability->spatial_layers_cnt_minus_1 = AV1_READ_BITS (br, 2);
  scalability->spatial_layer_dimensions_present_flag = AV1_READ_BIT (br);
  scalability->spatial_layer_description_present_flag = AV1_READ_BIT (br);
  scalability->temporal_group_description_present_flag = AV1_READ_BIT (br);
  scalability_structure_reserved_3bits = AV1_READ_BITS (br, 3);
  /* scalability_structure_reserved_3bits: must be set to zero and
     be ignored by decoders. */
  if (scalability_structure_reserved_3bits) {
    ret = GST_AV1_PARSER_BITSTREAM_ERROR;
    goto error;
  }

  if (scalability->spatial_layer_dimensions_present_flag) {
    for (i = 0; i <= scalability->spatial_layers_cnt_minus_1; i++) {
      if (AV1_REMAINING_BITS (br) < 16 * 2) {
        ret = GST_AV1_PARSER_NO_MORE_DATA;
        goto error;
      }

      scalability->spatial_layer_max_width[i] = AV1_READ_UINT16 (br);
      scalability->spatial_layer_max_height[i] = AV1_READ_UINT16 (br);
    }
  }

  if (scalability->spatial_layer_description_present_flag) {
    for (i = 0; i <= scalability->spatial_layers_cnt_minus_1; i++) {
      scalability->spatial_layer_ref_id[i] = AV1_READ_UINT8_CHECKED (br, &ret);
      if (ret != GST_AV1_PARSER_OK)
        goto error;
    }
  }

  if (scalability->temporal_group_description_present_flag) {
    scalability->temporal_group_size = AV1_READ_UINT8_CHECKED (br, &ret);
    if (ret != GST_AV1_PARSER_OK)
      goto error;

    for (i = 0; i < scalability->temporal_group_size; i++) {
      if (AV1_REMAINING_BITS (br) < 8) {
        ret = GST_AV1_PARSER_NO_MORE_DATA;
        goto error;
      }

      scalability->temporal_group_temporal_id[i] = AV1_READ_BITS (br, 3);
      scalability->temporal_group_temporal_switching_up_point_flag[i] =
          AV1_READ_BIT (br);
      scalability->temporal_group_spatial_switching_up_point_flag[i] =
          AV1_READ_BIT (br);
      scalability->temporal_group_ref_cnt[i] = AV1_READ_BITS (br, 3);
      for (j = 0; j < scalability->temporal_group_ref_cnt[i]; j++) {
        scalability->temporal_group_ref_pic_diff[i][j] =
            AV1_READ_UINT8_CHECKED (br, &ret);
        if (ret != GST_AV1_PARSER_OK)
          goto error;
      }
    }
  }

success:
  return GST_AV1_PARSER_OK;

error:
  GST_WARNING ("parse metadata scalability error %d", ret);
  return ret;
}

/* 5.8.7 */
static GstAV1ParserResult
gst_av1_parse_metadata_timecode (GstAV1Parser * parser, GstBitReader * br,
    GstAV1MetadataTimecode * timecode)
{
  GstAV1ParserResult ret = GST_AV1_PARSER_OK;

  if (AV1_REMAINING_BITS (br) < 17) {
    ret = GST_AV1_PARSER_NO_MORE_DATA;
    goto error;
  }

  timecode->counting_type = AV1_READ_BITS (br, 5);
  timecode->full_timestamp_flag = AV1_READ_BIT (br);
  timecode->discontinuity_flag = AV1_READ_BIT (br);
  timecode->cnt_dropped_flag = AV1_READ_BIT (br);
  timecode->n_frames = AV1_READ_BITS (br, 9);

  if (timecode->full_timestamp_flag) {
    if (AV1_REMAINING_BITS (br) < 17) {
      ret = GST_AV1_PARSER_NO_MORE_DATA;
      goto error;
    }
    timecode->seconds_value = AV1_READ_BITS (br, 6);
    timecode->minutes_value = AV1_READ_BITS (br, 6);
    timecode->hours_value = AV1_READ_BITS (br, 5);
  } else {
    timecode->seconds_flag = AV1_READ_BIT_CHECKED (br, &ret);
    if (ret != GST_AV1_PARSER_OK)
      goto error;

    if (timecode->seconds_flag) {
      if (AV1_REMAINING_BITS (br) < 7) {
        ret = GST_AV1_PARSER_NO_MORE_DATA;
        goto error;
      }
      timecode->seconds_value = AV1_READ_BITS (br, 6);
      timecode->minutes_flag = AV1_READ_BIT (br);

      if (timecode->minutes_flag) {
        if (AV1_REMAINING_BITS (br) < 7) {
          ret = GST_AV1_PARSER_NO_MORE_DATA;
          goto error;
        }
        timecode->minutes_value = AV1_READ_BITS (br, 6);
        timecode->hours_flag = AV1_READ_BIT (br);

        if (timecode->hours_flag) {
          timecode->hours_value = AV1_READ_BITS_CHECKED (br, 6, &ret);
          if (ret != GST_AV1_PARSER_OK)
            goto error;
        }
      }
    }
  }

  timecode->time_offset_length = AV1_READ_BITS_CHECKED (br, 5, &ret);
  if (ret != GST_AV1_PARSER_OK)
    goto error;

  if (timecode->time_offset_length > 0) {
    timecode->time_offset_value =
        AV1_READ_BITS_CHECKED (br, timecode->time_offset_length, &ret);
    if (ret != GST_AV1_PARSER_OK)
      goto error;
  }

  return GST_AV1_PARSER_OK;

error:
  GST_WARNING ("parse metadata timecode error %d", ret);
  return ret;
}

/* 5.8.1 */
/**
 * gst_av1_parser_parse_metadata_obu:
 * @parser: the #GstAV1Parser
 * @obu: a #GstAV1OBU to be parsed
 * @metadata: a #GstAV1MetadataOBU to store the parsed result.
 *
 * Parse one meta data @obu based on the @parser context.
 *
 * Returns: The #GstAV1ParserResult.
 *
 * Since: 1.18
 */
GstAV1ParserResult
gst_av1_parser_parse_metadata_obu (GstAV1Parser * parser, GstAV1OBU * obu,
    GstAV1MetadataOBU * metadata)
{
  GstAV1ParserResult retval = GST_AV1_PARSER_OK;
  GstBitReader bit_reader;

  g_return_val_if_fail (parser != NULL, GST_AV1_PARSER_INVALID_OPERATION);
  g_return_val_if_fail (obu != NULL, GST_AV1_PARSER_INVALID_OPERATION);
  g_return_val_if_fail (obu->obu_type == GST_AV1_OBU_METADATA,
      GST_AV1_PARSER_INVALID_OPERATION);
  g_return_val_if_fail (metadata != NULL, GST_AV1_PARSER_INVALID_OPERATION);

  gst_bit_reader_init (&bit_reader, obu->data, obu->obu_size);

  memset (metadata, 0, sizeof (*metadata));

  metadata->metadata_type = av1_bitstreamfn_leb128 (&bit_reader, &retval);
  if (retval != GST_AV1_PARSER_OK)
    goto error;

  switch (metadata->metadata_type) {
    case GST_AV1_METADATA_TYPE_ITUT_T35:
      retval = gst_av1_parse_metadata_itut_t35 (parser,
          &bit_reader, &(metadata->itut_t35));
      break;
    case GST_AV1_METADATA_TYPE_HDR_CLL:
      retval = gst_av1_parse_metadata_hdr_cll (parser,
          &bit_reader, &(metadata->hdr_cll));
      break;
    case GST_AV1_METADATA_TYPE_HDR_MDCV:
      retval = gst_av1_parse_metadata_hdr_mdcv (parser,
          &bit_reader, &(metadata->hdr_mdcv));
      break;
    case GST_AV1_METADATA_TYPE_SCALABILITY:
      retval = gst_av1_parse_metadata_scalability (parser,
          &bit_reader, &(metadata->scalability));
      break;
    case GST_AV1_METADATA_TYPE_TIMECODE:
      retval = gst_av1_parse_metadata_timecode (parser,
          &bit_reader, &(metadata->timecode));
      break;
    default:
      GST_WARNING ("Unknown metadata type %u", metadata->metadata_type);
      return GST_AV1_PARSER_OK;
  }

  if (retval != GST_AV1_PARSER_OK)
    goto error;

  retval = av1_skip_trailing_bits (parser, &bit_reader, obu);
  if (retval != GST_AV1_PARSER_OK) {
    GST_WARNING ("Metadata type %d may have wrong trailings.",
        metadata->metadata_type);
    retval = GST_AV1_PARSER_OK;
  }

  return retval;

error:
  GST_WARNING ("parse metadata error %d", retval);
  return retval;
}

/* 5.9.8 */
static GstAV1ParserResult
gst_av1_parse_superres_params_compute_image_size (GstAV1Parser * parser,
    GstBitReader * br, GstAV1FrameHeaderOBU * frame_header)
{
  GstAV1ParserResult ret;
  GstAV1SequenceHeaderOBU *seq_header;

  g_assert (parser->seq_header);
  seq_header = parser->seq_header;

  if (seq_header->enable_superres) {
    frame_header->use_superres = AV1_READ_BIT_CHECKED (br, &ret);
    if (ret != GST_AV1_PARSER_OK)
      return ret;
  } else {
    frame_header->use_superres = 0;
  }

  if (frame_header->use_superres) {
    guint8 coded_denom;
    coded_denom = AV1_READ_BITS_CHECKED (br, GST_AV1_SUPERRES_DENOM_BITS, &ret);
    if (ret != GST_AV1_PARSER_OK)
      return ret;

    frame_header->superres_denom = coded_denom + GST_AV1_SUPERRES_DENOM_MIN;
  } else {
    frame_header->superres_denom = GST_AV1_SUPERRES_NUM;
  }
  parser->state.upscaled_width = parser->state.frame_width;
  parser->state.frame_width =
      (parser->state.upscaled_width * GST_AV1_SUPERRES_NUM +
      (frame_header->superres_denom / 2)) / frame_header->superres_denom;

  /* 5.9.9 compute_image_size */
  parser->state.mi_cols = 2 * ((parser->state.frame_width + 7) >> 3);
  parser->state.mi_rows = 2 * ((parser->state.frame_height + 7) >> 3);

  return GST_AV1_PARSER_OK;
}

/* 5.9.5 */
static GstAV1ParserResult
gst_av1_parse_frame_size (GstAV1Parser * parser, GstBitReader * br,
    GstAV1FrameHeaderOBU * frame_header)
{
  GstAV1ParserResult retval;
  GstAV1SequenceHeaderOBU *seq_header;

  g_assert (parser->seq_header);
  seq_header = parser->seq_header;

  if (frame_header->frame_size_override_flag) {
    guint16 frame_width_minus_1;
    guint16 frame_height_minus_1;

    if (AV1_REMAINING_BITS (br) <
        seq_header->frame_width_bits_minus_1 + 1 +
        seq_header->frame_height_bits_minus_1 + 1)
      return GST_AV1_PARSER_NO_MORE_DATA;

    frame_width_minus_1 =
        AV1_READ_BITS (br, seq_header->frame_width_bits_minus_1 + 1);
    frame_height_minus_1 =
        AV1_READ_BITS (br, seq_header->frame_height_bits_minus_1 + 1);
    parser->state.frame_width = frame_width_minus_1 + 1;
    parser->state.frame_height = frame_height_minus_1 + 1;
  } else {
    parser->state.frame_width = seq_header->max_frame_width_minus_1 + 1;
    parser->state.frame_height = seq_header->max_frame_height_minus_1 + 1;
  }

  retval = gst_av1_parse_superres_params_compute_image_size (parser, br,
      frame_header);
  return retval;
}

/* 5.9.6 */
static GstAV1ParserResult
gst_av1_parse_render_size (GstAV1Parser * parser, GstBitReader * br,
    GstAV1FrameHeaderOBU * frame_header)
{
  GstAV1ParserResult retval;

  frame_header->render_and_frame_size_different =
      AV1_READ_BIT_CHECKED (br, &retval);
  if (retval != GST_AV1_PARSER_OK)
    return retval;

  if (frame_header->render_and_frame_size_different) {
    guint16 render_width_minus_1;
    guint16 render_height_minus_1;

    if (AV1_REMAINING_BITS (br) < 16 + 16)
      return GST_AV1_PARSER_NO_MORE_DATA;

    render_width_minus_1 = AV1_READ_UINT16 (br);
    render_height_minus_1 = AV1_READ_UINT16 (br);
    parser->state.render_width = render_width_minus_1 + 1;
    parser->state.render_height = render_height_minus_1 + 1;
  } else {
    parser->state.render_width = parser->state.upscaled_width;
    parser->state.render_height = parser->state.frame_height;
  }

  return GST_AV1_PARSER_OK;
}

/* 5.9.7 */
static GstAV1ParserResult
gst_av1_parse_frame_size_with_refs (GstAV1Parser * parser,
    GstBitReader * br, GstAV1FrameHeaderOBU * frame_header)
{
  GstAV1ParserResult retval;
  GstAV1ReferenceFrameInfo *ref_info;
  gboolean found_ref = FALSE;
  gint i;

  ref_info = &(parser->state.ref_info);

  for (i = 0; i < GST_AV1_REFS_PER_FRAME; i++) {
    found_ref = AV1_READ_BIT_CHECKED (br, &retval);
    if (retval != GST_AV1_PARSER_OK)
      return retval;

    if (found_ref == 1) {
      gint ref_idx = frame_header->ref_frame_idx[i];
      parser->state.upscaled_width =
          ref_info->entry[ref_idx].ref_upscaled_width;
      parser->state.frame_width = parser->state.upscaled_width;
      parser->state.frame_height = ref_info->entry[ref_idx].ref_frame_height;
      parser->state.render_width = ref_info->entry[ref_idx].ref_render_width;
      parser->state.render_height = ref_info->entry[ref_idx].ref_render_height;
      break;
    }
  }
  if (found_ref == 0) {
    retval = gst_av1_parse_frame_size (parser, br, frame_header);
    if (retval != GST_AV1_PARSER_OK)
      return retval;

    retval = gst_av1_parse_render_size (parser, br, frame_header);
    if (retval != GST_AV1_PARSER_OK)
      return retval;
  } else {
    retval = gst_av1_parse_superres_params_compute_image_size (parser, br,
        frame_header);
    if (retval != GST_AV1_PARSER_OK)
      return retval;
  }

  return GST_AV1_PARSER_OK;
}

/* 5.9.12 */
static GstAV1ParserResult
gst_av1_parse_quantization_params (GstAV1Parser * parser, GstBitReader * br,
    GstAV1FrameHeaderOBU * frame_header)
{
  GstAV1ParserResult retval = GST_AV1_PARSER_OK;
  GstAV1ColorConfig *color_config;
  GstAV1QuantizationParams *quant_params = &(frame_header->quantization_params);

  g_assert (parser->seq_header);

  color_config = &(parser->seq_header->color_config);

  quant_params->base_q_idx = AV1_READ_UINT8_CHECKED (br, &retval);
  if (retval != GST_AV1_PARSER_OK)
    goto error;

  frame_header->quantization_params.delta_q_y_dc =
      av1_bitstreamfn_delta_q (br, &retval);
  if (retval != GST_AV1_PARSER_OK)
    goto error;

  if (parser->seq_header->num_planes > 1) {
    if (color_config->separate_uv_delta_q) {
      quant_params->diff_uv_delta = AV1_READ_BIT_CHECKED (br, &retval);
      if (retval != GST_AV1_PARSER_OK)
        goto error;
    } else {
      quant_params->diff_uv_delta = 0;
    }
    frame_header->quantization_params.delta_q_u_dc =
        av1_bitstreamfn_delta_q (br, &retval);
    if (retval != GST_AV1_PARSER_OK)
      goto error;

    frame_header->quantization_params.delta_q_u_ac =
        av1_bitstreamfn_delta_q (br, &retval);
    if (retval != GST_AV1_PARSER_OK)
      goto error;

    if (quant_params->diff_uv_delta) {
      frame_header->quantization_params.delta_q_v_dc =
          av1_bitstreamfn_delta_q (br, &retval);
      if (retval != GST_AV1_PARSER_OK)
        goto error;

      frame_header->quantization_params.delta_q_v_ac =
          av1_bitstreamfn_delta_q (br, &retval);
      if (retval != GST_AV1_PARSER_OK)
        goto error;
    } else {
      frame_header->quantization_params.delta_q_v_dc =
          frame_header->quantization_params.delta_q_u_dc;
      frame_header->quantization_params.delta_q_v_ac =
          frame_header->quantization_params.delta_q_u_ac;
    }
  } else {
    frame_header->quantization_params.delta_q_u_dc = 0;
    frame_header->quantization_params.delta_q_u_ac = 0;
    frame_header->quantization_params.delta_q_v_dc = 0;
    frame_header->quantization_params.delta_q_v_ac = 0;
  }

  quant_params->using_qmatrix = AV1_READ_BIT_CHECKED (br, &retval);
  if (retval != GST_AV1_PARSER_OK)
    goto error;

  if (quant_params->using_qmatrix) {
    if (AV1_REMAINING_BITS (br) < 4 + 4) {
      retval = GST_AV1_PARSER_NO_MORE_DATA;
      goto error;
    }

    quant_params->qm_y = AV1_READ_BITS (br, 4);
    quant_params->qm_u = AV1_READ_BITS (br, 4);

    if (!color_config->separate_uv_delta_q) {
      quant_params->qm_v = quant_params->qm_u;
    } else {
      quant_params->qm_v = AV1_READ_BITS_CHECKED (br, 4, &retval);
      if (retval != GST_AV1_PARSER_OK)
        goto error;
    }
  }

  return GST_AV1_PARSER_OK;

error:
  GST_WARNING ("parse quantization params error %d", retval);
  return retval;
}

/* 5.9.14 */
static GstAV1ParserResult
gst_av1_parse_segmentation_params (GstAV1Parser * parser, GstBitReader * br,
    GstAV1FrameHeaderOBU * frame_header)
{
  gint i, j;
  GstAV1ParserResult retval = GST_AV1_PARSER_OK;
  gint clipped_value /* clippedValue */ ;
  GstAV1SegmenationParams *seg_params;
  gint feature_value = 0;

  const guint8 segmentation_feature_bits[GST_AV1_SEG_LVL_MAX] = {
    8, 6, 6, 6, 6, 3, 0, 0
  };
  const guint8 segmentation_feature_signed[GST_AV1_SEG_LVL_MAX] = {
    1, 1, 1, 1, 1, 0, 0, 0
  };
  const guint8 segmentation_feature_max[GST_AV1_SEG_LVL_MAX] = {
    255, GST_AV1_MAX_LOOP_FILTER, GST_AV1_MAX_LOOP_FILTER,
    GST_AV1_MAX_LOOP_FILTER, GST_AV1_MAX_LOOP_FILTER, 7, 0, 0
  };

  seg_params = &frame_header->segmentation_params;

  seg_params->segmentation_enabled = AV1_READ_BIT_CHECKED (br, &retval);
  if (retval != GST_AV1_PARSER_OK)
    goto error;

  if (seg_params->segmentation_enabled) {
    if (frame_header->primary_ref_frame == GST_AV1_PRIMARY_REF_NONE) {
      seg_params->segmentation_update_map = 1;
      seg_params->segmentation_temporal_update = 0;
      seg_params->segmentation_update_data = 1;
    } else {
      seg_params->segmentation_update_map = AV1_READ_BIT_CHECKED (br, &retval);
      if (retval != GST_AV1_PARSER_OK)
        goto error;

      if (seg_params->segmentation_update_map) {
        seg_params->segmentation_temporal_update =
            AV1_READ_BIT_CHECKED (br, &retval);
        if (retval != GST_AV1_PARSER_OK)
          goto error;
      }
      seg_params->segmentation_update_data = AV1_READ_BIT_CHECKED (br, &retval);
      if (retval != GST_AV1_PARSER_OK)
        goto error;
    }

    if (seg_params->segmentation_update_data) {
      for (i = 0; i < GST_AV1_MAX_SEGMENTS; i++) {
        for (j = 0; j < GST_AV1_SEG_LVL_MAX; j++) {
          seg_params->feature_enabled[i][j] =
              AV1_READ_BIT_CHECKED (br, &retval);
          if (retval != GST_AV1_PARSER_OK)
            goto error;

          clipped_value = 0;
          feature_value = 0;
          if (seg_params->feature_enabled[i][j]) {
            gint bits_to_read = segmentation_feature_bits[j];
            gint limit = segmentation_feature_max[j];
            if (segmentation_feature_signed[j]) {
              feature_value =
                  av1_bitstreamfn_su (br, 1 + bits_to_read, &retval);
              if (retval != GST_AV1_PARSER_OK)
                goto error;

              clipped_value = CLAMP (feature_value, limit * (-1), limit);
            } else {
              feature_value = AV1_READ_BITS_CHECKED (br, bits_to_read, &retval);
              if (retval != GST_AV1_PARSER_OK)
                goto error;

              clipped_value = CLAMP (feature_value, 0, limit);
            }
          }
          seg_params->feature_data[i][j] = clipped_value;
        }
      }
    } else {
      gint8 ref_idx;
      GstAV1SegmenationParams *ref_seg_params;

      /* Copy it from prime_ref */
      if (frame_header->primary_ref_frame >= GST_AV1_PRIMARY_REF_NONE) {
        GST_WARNING ("Invalid primary_ref_frame %d",
            frame_header->primary_ref_frame);
        return GST_AV1_PARSER_BITSTREAM_ERROR;
      }

      ref_idx = frame_header->ref_frame_idx[frame_header->primary_ref_frame];
      if (ref_idx >= GST_AV1_NUM_REF_FRAMES || ref_idx < 0) {
        GST_WARNING ("Invalid ref_frame_idx %d", ref_idx);
        return GST_AV1_PARSER_BITSTREAM_ERROR;
      }

      if (!parser->state.ref_info.entry[ref_idx].ref_valid) {
        GST_WARNING ("Reference frame at index %d is unavailable", ref_idx);
        return GST_AV1_PARSER_BITSTREAM_ERROR;
      }

      ref_seg_params =
          &parser->state.ref_info.entry[ref_idx].ref_segmentation_params;

      for (i = 0; i < GST_AV1_MAX_SEGMENTS; i++) {
        for (j = 0; j < GST_AV1_SEG_LVL_MAX; j++) {
          seg_params->feature_enabled[i][j] =
              ref_seg_params->feature_enabled[i][j];
          seg_params->feature_data[i][j] = ref_seg_params->feature_data[i][j];
        }
      }
    }
  } else {
    seg_params->segmentation_update_map = 0;
    seg_params->segmentation_temporal_update = 0;
    seg_params->segmentation_update_data = 0;
    for (i = 0; i < GST_AV1_MAX_SEGMENTS; i++) {
      for (j = 0; j < GST_AV1_SEG_LVL_MAX; j++) {
        seg_params->feature_enabled[i][j] = 0;
        seg_params->feature_data[i][j] = 0;
      }
    }
  }

  seg_params->seg_id_pre_skip = 0;
  seg_params->last_active_seg_id = 0;
  for (i = 0; i < GST_AV1_MAX_SEGMENTS; i++) {
    for (j = 0; j < GST_AV1_SEG_LVL_MAX; j++) {
      if (seg_params->feature_enabled[i][j]) {
        seg_params->last_active_seg_id = i;
        if (j >= GST_AV1_SEG_LVL_REF_FRAME) {
          seg_params->seg_id_pre_skip = 1;
        }
      }
    }
  }

  return GST_AV1_PARSER_OK;

error:
  GST_WARNING ("parse segmentation params error %d", retval);
  return retval;
}

/* 5.9.15 */
static GstAV1ParserResult
gst_av1_parse_tile_info (GstAV1Parser * parser, GstBitReader * br,
    GstAV1FrameHeaderOBU * frame_header)
{
  GstAV1ParserResult retval = GST_AV1_PARSER_OK;
  GstAV1SequenceHeaderOBU *seq_header;
  GstAV1TileInfo *tile_info;
  gint i;
  gint start_sb /* startSb */ ;
  gint sb_cols /* sbCols */ ;
  gint sb_rows /* sbRows */ ;
  gint sb_shift /*sbShift */ ;
  gint sb_size /* sbSize */ ;
  gint max_tile_width_sb /* maxTileWidthSb */ ;
  gint max_tile_height_sb /* maxTileHeightSb */ ;
  gint max_tile_area_sb /* maxTileAreaSb */ ;
  gint min_log2_tile_cols /* minLog2TileCols */ ;
  gint max_log2_tile_cols /* maxLog2TileCols */ ;
  gint min_log2_tile_rows /* minLog2TileRows */ ;
  gint max_log2_tile_rows /* maxLog2TileRows */ ;
  gint min_log2_tiles /* minLog2Tiles */ ;
  gint tile_width_sb /* tileWidthSb */ ;
  gint tile_height_sb /* tileHeightSb */ ;
  gint max_width /* maxWidth */ , max_height /* maxHeight */ ;
  gint size_sb /* sizeSb */ ;
  gint widest_tile_sb /* widestTileSb */ ;

  g_assert (parser->seq_header);
  seq_header = parser->seq_header;
  tile_info = &frame_header->tile_info;

  sb_cols = seq_header->use_128x128_superblock ?
      ((parser->state.mi_cols + 31) >> 5) : ((parser->state.mi_cols + 15) >> 4);
  sb_rows = seq_header->use_128x128_superblock ? ((parser->state.mi_rows +
          31) >> 5) : ((parser->state.mi_rows + 15) >> 4);
  sb_shift = seq_header->use_128x128_superblock ? 5 : 4;
  sb_size = sb_shift + 2;
  max_tile_width_sb = GST_AV1_MAX_TILE_WIDTH >> sb_size;
  max_tile_area_sb = GST_AV1_MAX_TILE_AREA >> (2 * sb_size);
  min_log2_tile_cols = av1_helper_tile_log2 (max_tile_width_sb, sb_cols);
  max_log2_tile_cols = av1_helper_tile_log2 (1, MIN (sb_cols,
          GST_AV1_MAX_TILE_COLS));
  max_log2_tile_rows = av1_helper_tile_log2 (1, MIN (sb_rows,
          GST_AV1_MAX_TILE_ROWS));
  min_log2_tiles = MAX (min_log2_tile_cols,
      av1_helper_tile_log2 (max_tile_area_sb, sb_rows * sb_cols));

  tile_info->uniform_tile_spacing_flag = AV1_READ_BIT_CHECKED (br, &retval);
  if (retval != GST_AV1_PARSER_OK)
    goto error;

  if (tile_info->uniform_tile_spacing_flag) {
    parser->state.tile_cols_log2 = min_log2_tile_cols;
    while (parser->state.tile_cols_log2 < max_log2_tile_cols) {
      gint increment_tile_cols_log2 = AV1_READ_BIT_CHECKED (br, &retval);
      if (retval != GST_AV1_PARSER_OK)
        goto error;

      if (increment_tile_cols_log2 == 1)
        parser->state.tile_cols_log2++;
      else
        break;
    }
    tile_width_sb = (sb_cols + (1 << parser->state.tile_cols_log2) -
        1) >> parser->state.tile_cols_log2;
    i = 0;
    /* Fill mi_col_starts[] and make sure to not exceed array range */
    for (start_sb = 0; start_sb < sb_cols && i < GST_AV1_MAX_TILE_COLS;
        start_sb += tile_width_sb) {
      parser->state.mi_col_starts[i] = start_sb << sb_shift;
      i += 1;
    }
    parser->state.mi_col_starts[i] = parser->state.mi_cols;
    parser->state.tile_cols = i;

    while (i >= 1) {
      tile_info->width_in_sbs_minus_1[i - 1] =
          ((parser->state.mi_col_starts[i] - parser->state.mi_col_starts[i - 1]
              + ((1 << sb_shift) - 1)) >> sb_shift) - 1;
      i--;
    }

    min_log2_tile_rows = MAX (min_log2_tiles - parser->state.tile_cols_log2, 0);
    parser->state.tile_rows_log2 = min_log2_tile_rows;
    while (parser->state.tile_rows_log2 < max_log2_tile_rows) {
      tile_info->increment_tile_rows_log2 = AV1_READ_BIT_CHECKED (br, &retval);
      if (retval != GST_AV1_PARSER_OK)
        goto error;

      if (tile_info->increment_tile_rows_log2 == 1)
        parser->state.tile_rows_log2++;
      else
        break;
    }
    tile_height_sb = (sb_rows + (1 << parser->state.tile_rows_log2) -
        1) >> parser->state.tile_rows_log2;
    i = 0;
    /* Fill mi_row_starts[] and make sure to not exceed array range */
    for (start_sb = 0; start_sb < sb_rows && i < GST_AV1_MAX_TILE_ROWS;
        start_sb += tile_height_sb) {
      parser->state.mi_row_starts[i] = start_sb << sb_shift;
      i += 1;
    }
    parser->state.mi_row_starts[i] = parser->state.mi_rows;
    parser->state.tile_rows = i;
    while (i >= 1) {
      tile_info->height_in_sbs_minus_1[i - 1] =
          ((parser->state.mi_row_starts[i] - parser->state.mi_row_starts[i - 1]
              + ((1 << sb_shift) - 1)) >> sb_shift) - 1;
      i--;
    }
  } else {
    widest_tile_sb = 0;
    start_sb = 0;
    /* Fill mi_col_starts[] and make sure to not exceed array range */
    for (i = 0; start_sb < sb_cols && i < GST_AV1_MAX_TILE_COLS; i++) {
      parser->state.mi_col_starts[i] = start_sb << sb_shift;
      max_width = MIN (sb_cols - start_sb, max_tile_width_sb);
      tile_info->width_in_sbs_minus_1[i] =
          av1_bitstreamfn_ns (br, max_width, &retval);
      if (retval != GST_AV1_PARSER_OK)
        goto error;

      size_sb = tile_info->width_in_sbs_minus_1[i] + 1;
      widest_tile_sb = MAX (size_sb, widest_tile_sb);
      start_sb += size_sb;
    }
    parser->state.mi_col_starts[i] = parser->state.mi_cols;
    parser->state.tile_cols = i;
    parser->state.tile_cols_log2 =
        av1_helper_tile_log2 (1, parser->state.tile_cols);

    if (min_log2_tiles > 0)
      max_tile_area_sb = (sb_rows * sb_cols) >> (min_log2_tiles + 1);
    else
      max_tile_area_sb = sb_rows * sb_cols;

    max_tile_height_sb = MAX (max_tile_area_sb / widest_tile_sb, 1);

    start_sb = 0;
    /* Fill mi_row_starts[] and make sure to not exceed array range */
    for (i = 0; start_sb < sb_rows && i < GST_AV1_MAX_TILE_ROWS; i++) {
      parser->state.mi_row_starts[i] = start_sb << sb_shift;
      max_height = MIN (sb_rows - start_sb, max_tile_height_sb);
      tile_info->height_in_sbs_minus_1[i] =
          av1_bitstreamfn_ns (br, max_height, &retval);
      if (retval != GST_AV1_PARSER_OK)
        goto error;

      size_sb = tile_info->height_in_sbs_minus_1[i] + 1;
      start_sb += size_sb;
    }

    parser->state.mi_row_starts[i] = parser->state.mi_rows;
    parser->state.tile_rows = i;
    parser->state.tile_rows_log2 =
        av1_helper_tile_log2 (1, parser->state.tile_rows);
  }

  if (parser->state.tile_cols_log2 > 0 || parser->state.tile_rows_log2 > 0) {
    tile_info->context_update_tile_id =
        AV1_READ_BITS_CHECKED (br,
        parser->state.tile_cols_log2 + parser->state.tile_rows_log2, &retval);
    if (retval != GST_AV1_PARSER_OK)
      goto error;

    tile_info->tile_size_bytes_minus_1 = AV1_READ_BITS_CHECKED (br, 2, &retval);
    if (retval != GST_AV1_PARSER_OK)
      goto error;

    parser->state.tile_size_bytes = tile_info->tile_size_bytes_minus_1 + 1;
  } else {
    tile_info->context_update_tile_id = 0;
  }

  memcpy (tile_info->mi_col_starts, parser->state.mi_col_starts,
      sizeof (guint32) * (GST_AV1_MAX_TILE_COLS + 1));
  memcpy (tile_info->mi_row_starts, parser->state.mi_row_starts,
      sizeof (guint32) * (GST_AV1_MAX_TILE_ROWS + 1));
  tile_info->tile_cols_log2 = parser->state.tile_cols_log2;
  tile_info->tile_cols = parser->state.tile_cols;
  tile_info->tile_rows_log2 = parser->state.tile_rows_log2;
  tile_info->tile_rows = parser->state.tile_rows;
  tile_info->tile_size_bytes = parser->state.tile_size_bytes;

  return GST_AV1_PARSER_OK;

error:
  GST_WARNING ("parse tile info error %d", retval);
  return retval;
}

static GstAV1ParserResult
gst_av1_parse_loop_filter_params (GstAV1Parser * parser,
    GstBitReader * br, GstAV1FrameHeaderOBU * frame_header)
{
  GstAV1ParserResult retval = GST_AV1_PARSER_OK;
  GstAV1LoopFilterParams *lf_params;
  gint i;
  guint8 update_ref_deltas = 0;
  guint8 update_mode_deltas = 0;

  g_assert (parser->seq_header);

  lf_params = &frame_header->loop_filter_params;

  if (frame_header->coded_lossless || frame_header->allow_intrabc) {
    lf_params->loop_filter_level[0] = 0;
    lf_params->loop_filter_level[1] = 0;
    lf_params->loop_filter_ref_deltas[GST_AV1_REF_INTRA_FRAME] = 1;
    lf_params->loop_filter_ref_deltas[GST_AV1_REF_LAST_FRAME] = 0;
    lf_params->loop_filter_ref_deltas[GST_AV1_REF_LAST2_FRAME] = 0;
    lf_params->loop_filter_ref_deltas[GST_AV1_REF_LAST3_FRAME] = 0;
    lf_params->loop_filter_ref_deltas[GST_AV1_REF_BWDREF_FRAME] = 0;
    lf_params->loop_filter_ref_deltas[GST_AV1_REF_GOLDEN_FRAME] = -1;
    lf_params->loop_filter_ref_deltas[GST_AV1_REF_ALTREF_FRAME] = -1;
    lf_params->loop_filter_ref_deltas[GST_AV1_REF_ALTREF2_FRAME] = -1;
    for (i = 0; i < 2; i++)
      lf_params->loop_filter_mode_deltas[i] = 0;

    goto success;
  }

  if (AV1_REMAINING_BITS (br) < 6 + 6) {
    retval = GST_AV1_PARSER_NO_MORE_DATA;
    goto error;
  }

  lf_params->loop_filter_level[0] = AV1_READ_BITS (br, 6);
  lf_params->loop_filter_level[1] = AV1_READ_BITS (br, 6);
  if (parser->seq_header->num_planes > 1) {
    if (lf_params->loop_filter_level[0] || lf_params->loop_filter_level[1]) {
      if (AV1_REMAINING_BITS (br) < 6 + 6) {
        retval = GST_AV1_PARSER_NO_MORE_DATA;
        goto error;
      }

      lf_params->loop_filter_level[2] = AV1_READ_BITS (br, 6);
      lf_params->loop_filter_level[3] = AV1_READ_BITS (br, 6);
    }
  }

  if (AV1_REMAINING_BITS (br) < 3 + 1) {
    retval = GST_AV1_PARSER_NO_MORE_DATA;
    goto error;
  }

  lf_params->loop_filter_sharpness = AV1_READ_BITS (br, 3);

  lf_params->loop_filter_delta_enabled = AV1_READ_BIT (br);
  if (lf_params->loop_filter_delta_enabled) {
    lf_params->loop_filter_delta_update = AV1_READ_BIT_CHECKED (br, &retval);
    if (retval != GST_AV1_PARSER_OK)
      goto error;

    if (lf_params->loop_filter_delta_update) {
      for (i = 0; i < GST_AV1_TOTAL_REFS_PER_FRAME; i++) {
        update_ref_deltas = AV1_READ_BIT_CHECKED (br, &retval);
        if (retval != GST_AV1_PARSER_OK)
          goto error;

        if (update_ref_deltas) {
          lf_params->loop_filter_ref_deltas[i] =
              av1_bitstreamfn_su (br, 7, &retval);
          if (retval != GST_AV1_PARSER_OK)
            goto error;
        }
      }
      for (i = 0; i < 2; i++) {
        update_mode_deltas = AV1_READ_BIT_CHECKED (br, &retval);
        if (retval != GST_AV1_PARSER_OK)
          goto error;

        if (update_mode_deltas) {
          lf_params->loop_filter_mode_deltas[i] =
              av1_bitstreamfn_su (br, 7, &retval);
          if (retval != GST_AV1_PARSER_OK)
            goto error;
        }
      }
    }
  }

success:
  return GST_AV1_PARSER_OK;

error:
  GST_WARNING ("parse loop filter params error %d", retval);
  return retval;
}

/* 5.9.17 */
static GstAV1ParserResult
gst_av1_parse_delta_q_params (GstAV1Parser * parser,
    GstBitReader * br, GstAV1QuantizationParams * quant_params)
{
  GstAV1ParserResult retval;

  quant_params->delta_q_res = 0;
  quant_params->delta_q_present = 0;
  if (quant_params->base_q_idx > 0) {
    quant_params->delta_q_present = AV1_READ_BIT_CHECKED (br, &retval);
    if (retval != GST_AV1_PARSER_OK)
      return retval;
  }

  if (quant_params->delta_q_present) {
    quant_params->delta_q_res = AV1_READ_BITS_CHECKED (br, 2, &retval);
    if (retval != GST_AV1_PARSER_OK)
      return retval;
  }

  return GST_AV1_PARSER_OK;
}

/* 5.9.18 */
static GstAV1ParserResult
gst_av1_parse_delta_lf_params (GstAV1Parser * parser,
    GstBitReader * br, GstAV1FrameHeaderOBU * frame_header)
{
  GstAV1ParserResult retval;
  GstAV1LoopFilterParams *lf_params;

  lf_params = &frame_header->loop_filter_params;
  lf_params->delta_lf_present = 0;
  lf_params->delta_lf_res = 0;
  lf_params->delta_lf_multi = 0;

  if (frame_header->quantization_params.delta_q_present) {
    if (!frame_header->allow_intrabc) {
      lf_params->delta_lf_present = AV1_READ_BIT_CHECKED (br, &retval);
      if (retval != GST_AV1_PARSER_OK)
        return retval;
    }
    if (lf_params->delta_lf_present) {
      if (AV1_REMAINING_BITS (br) < 2 + 1)
        return GST_AV1_PARSER_NO_MORE_DATA;
      lf_params->delta_lf_res = AV1_READ_BITS (br, 2);
      lf_params->delta_lf_multi = AV1_READ_BIT (br);
    }
  }

  return GST_AV1_PARSER_OK;
}

/* 5.9.19 */
static GstAV1ParserResult
gst_av1_parse_cdef_params (GstAV1Parser * parser, GstBitReader * br,
    GstAV1FrameHeaderOBU * frame_header)
{
  GstAV1SequenceHeaderOBU *seq_header;
  GstAV1CDEFParams *cdef_params;
  guint8 cdef_damping_minus_3;
  gint i;

  g_assert (parser->seq_header);

  cdef_params = &frame_header->cdef_params;
  seq_header = parser->seq_header;

  if (frame_header->coded_lossless || frame_header->allow_intrabc
      || !seq_header->enable_cdef) {
    cdef_params->cdef_bits = 0;
    cdef_params->cdef_y_pri_strength[0] = 0;
    cdef_params->cdef_y_sec_strength[0] = 0;
    cdef_params->cdef_uv_pri_strength[0] = 0;
    cdef_params->cdef_uv_sec_strength[0] = 0;
    cdef_params->cdef_damping = 3;
    return GST_AV1_PARSER_OK;
  }

  if (AV1_REMAINING_BITS (br) < 2 + 2)
    return GST_AV1_PARSER_NO_MORE_DATA;

  cdef_damping_minus_3 = AV1_READ_BITS (br, 2);
  cdef_params->cdef_damping = cdef_damping_minus_3 + 3;
  cdef_params->cdef_bits = AV1_READ_BITS (br, 2);
  for (i = 0; i < (1 << cdef_params->cdef_bits); i++) {
    if (AV1_REMAINING_BITS (br) < 4 + 2)
      return GST_AV1_PARSER_NO_MORE_DATA;

    cdef_params->cdef_y_pri_strength[i] = AV1_READ_BITS (br, 4);
    cdef_params->cdef_y_sec_strength[i] = AV1_READ_BITS (br, 2);
    if (cdef_params->cdef_y_sec_strength[i] == 3)
      cdef_params->cdef_y_sec_strength[i] += 1;

    if (parser->seq_header->num_planes > 1) {
      if (AV1_REMAINING_BITS (br) < 4 + 2)
        return GST_AV1_PARSER_NO_MORE_DATA;

      cdef_params->cdef_uv_pri_strength[i] = AV1_READ_BITS (br, 4);
      cdef_params->cdef_uv_sec_strength[i] = AV1_READ_BITS (br, 2);
      if (cdef_params->cdef_uv_sec_strength[i] == 3)
        cdef_params->cdef_uv_sec_strength[i] += 1;
    }
  }

  return GST_AV1_PARSER_OK;
}

/* 5.9.20 */
static GstAV1ParserResult
gst_av1_parse_loop_restoration_params (GstAV1Parser * parser,
    GstBitReader * br, GstAV1FrameHeaderOBU * frame_header)
{
  GstAV1LoopRestorationParams *lr_params;
  GstAV1SequenceHeaderOBU *seq_header;
  GstAV1ParserResult retval = GST_AV1_PARSER_OK;
  guint8 lr_type;
  gint i;
  guint8 use_chroma_lr /* useChromaLr */ ;
  const GstAV1FrameRestorationType remap_lr_type /* Remap_Lr_Type */ [4] = {
    GST_AV1_FRAME_RESTORE_NONE,
    GST_AV1_FRAME_RESTORE_SWITCHABLE,
    GST_AV1_FRAME_RESTORE_WIENER, GST_AV1_FRAME_RESTORE_SGRPROJ
  };

  g_assert (parser->seq_header);

  lr_params = &frame_header->loop_restoration_params;
  seq_header = parser->seq_header;

  if (frame_header->all_lossless || frame_header->allow_intrabc
      || !seq_header->enable_restoration) {
    for (i = 0; i < GST_AV1_MAX_NUM_PLANES; i++)
      lr_params->frame_restoration_type[i] = GST_AV1_FRAME_RESTORE_NONE;

    lr_params->uses_lr = 0;
    goto success;
  }

  lr_params->uses_lr = 0;
  use_chroma_lr = 0;
  for (i = 0; i < seq_header->num_planes; i++) {
    lr_type = AV1_READ_BITS_CHECKED (br, 2, &retval);
    if (retval != GST_AV1_PARSER_OK)
      goto error;

    lr_params->frame_restoration_type[i] = remap_lr_type[lr_type];
    if (lr_params->frame_restoration_type[i] != GST_AV1_FRAME_RESTORE_NONE) {
      lr_params->uses_lr = 1;
      if (i > 0) {
        use_chroma_lr = 1;
      }
    }
  }

  if (lr_params->uses_lr) {
    if (seq_header->use_128x128_superblock) {
      lr_params->lr_unit_shift = AV1_READ_BIT_CHECKED (br, &retval);
      if (retval != GST_AV1_PARSER_OK)
        goto error;

      lr_params->lr_unit_shift++;
    } else {
      guint8 lr_unit_extra_shift;

      lr_params->lr_unit_shift = AV1_READ_BIT_CHECKED (br, &retval);
      if (retval != GST_AV1_PARSER_OK)
        goto error;

      if (lr_params->lr_unit_shift) {
        lr_unit_extra_shift = AV1_READ_BIT_CHECKED (br, &retval);
        if (retval != GST_AV1_PARSER_OK)
          goto error;

        lr_params->lr_unit_shift += lr_unit_extra_shift;
      }
    }

    lr_params->loop_restoration_size[0] =
        GST_AV1_RESTORATION_TILESIZE_MAX >> (2 - lr_params->lr_unit_shift);
    if (seq_header->color_config.subsampling_x
        && seq_header->color_config.subsampling_y && use_chroma_lr) {
      lr_params->lr_uv_shift = AV1_READ_BIT_CHECKED (br, &retval);
      if (retval != GST_AV1_PARSER_OK)
        goto error;
    } else {
      lr_params->lr_uv_shift = 0;
    }

    lr_params->loop_restoration_size[1] =
        lr_params->loop_restoration_size[0] >> lr_params->lr_uv_shift;
    lr_params->loop_restoration_size[2] =
        lr_params->loop_restoration_size[0] >> lr_params->lr_uv_shift;
  }

success:
  return GST_AV1_PARSER_OK;

error:
  GST_WARNING ("parse loop restoration params error %d", retval);
  return retval;
}

/* 5.9.21 */
static GstAV1ParserResult
gst_av1_parse_tx_mode (GstAV1Parser * parser, GstBitReader * br,
    GstAV1FrameHeaderOBU * frame_header)
{
  GstAV1ParserResult retval;

  if (frame_header->coded_lossless == 1) {
    frame_header->tx_mode = GST_AV1_TX_MODE_ONLY_4x4;
  } else {
    frame_header->tx_mode_select = AV1_READ_BIT_CHECKED (br, &retval);
    if (retval != GST_AV1_PARSER_OK)
      return retval;

    if (frame_header->tx_mode_select) {
      frame_header->tx_mode = GST_AV1_TX_MODE_SELECT;
    } else {
      frame_header->tx_mode = GST_AV1_TX_MODE_LARGEST;
    }
  }

  return GST_AV1_PARSER_OK;
}

/* 5.9.3 */
static gint
gst_av1_get_relative_dist (GstAV1SequenceHeaderOBU * seq_header, gint a, gint b)
{
  gint m, diff;
  if (!seq_header->enable_order_hint)
    return 0;
  diff = a - b;
  m = 1 << seq_header->order_hint_bits_minus_1;
  diff = (diff & (m - 1)) - (diff & m);
  return diff;
}

/* 5.9.22 */
static GstAV1ParserResult
gst_av1_parse_skip_mode_params (GstAV1Parser * parser, GstBitReader * br,
    GstAV1FrameHeaderOBU * frame_header)
{
  GstAV1ReferenceFrameInfo *ref_info;
  GstAV1SequenceHeaderOBU *seq_header;
  gint i;
  gint skip_mode_allowed /* skipModeAllowed */ ;
  GstAV1ParserResult retval;

  g_assert (parser->seq_header);

  seq_header = parser->seq_header;
  ref_info = &(parser->state.ref_info);
  skip_mode_allowed = 0;
  if (frame_header->frame_is_intra || !frame_header->reference_select
      || !seq_header->enable_order_hint) {
    skip_mode_allowed = 0;
  } else {
    gint forward_idx = -1 /* forwardIdx */ ;
    gint forward_hint = 0 /* forwardHint */ ;
    gint backward_idx = -1 /* backwardIdx */ ;
    gint backward_hint = 0 /* backwardHint */ ;
    gint ref_hint = 0 /* refHint */ ;

    for (i = 0; i < GST_AV1_REFS_PER_FRAME; i++) {
      ref_hint = ref_info->entry[frame_header->ref_frame_idx[i]].ref_order_hint;
      if (gst_av1_get_relative_dist (parser->seq_header, ref_hint,
              frame_header->order_hint) < 0) {
        if (forward_idx < 0
            || gst_av1_get_relative_dist (parser->seq_header, ref_hint,
                forward_hint) > 0) {
          forward_idx = i;
          forward_hint = ref_hint;
        }
      } else
          if (gst_av1_get_relative_dist (parser->seq_header, ref_hint,
              frame_header->order_hint) > 0) {
        if (backward_idx < 0
            || gst_av1_get_relative_dist (parser->seq_header, ref_hint,
                backward_hint) < 0) {
          backward_idx = i;
          backward_hint = ref_hint;
        }
      }
    }

    if (forward_idx < 0) {
      skip_mode_allowed = 0;
    } else if (backward_idx >= 0) {
      skip_mode_allowed = 1;
      frame_header->skip_mode_frame[0] =
          GST_AV1_REF_LAST_FRAME + MIN (forward_idx, backward_idx);
      frame_header->skip_mode_frame[1] =
          GST_AV1_REF_LAST_FRAME + MAX (forward_idx, backward_idx);
    } else {
      gint second_forward_idx = -1 /* secondForwardIdx */ ;
      gint second_forward_hint = 0 /* secondForwardHint */ ;
      for (i = 0; i < GST_AV1_REFS_PER_FRAME; i++) {
        ref_hint =
            ref_info->entry[frame_header->ref_frame_idx[i]].ref_order_hint;
        if (gst_av1_get_relative_dist (parser->seq_header, ref_hint,
                forward_hint) < 0) {
          if (second_forward_idx < 0
              || gst_av1_get_relative_dist (parser->seq_header, ref_hint,
                  second_forward_hint) > 0) {
            second_forward_idx = i;
            second_forward_hint = ref_hint;
          }
        }
      }

      if (second_forward_idx < 0) {
        skip_mode_allowed = 0;
      } else {
        skip_mode_allowed = 1;
        frame_header->skip_mode_frame[0] =
            GST_AV1_REF_LAST_FRAME + MIN (forward_idx, second_forward_idx);
        frame_header->skip_mode_frame[1] =
            GST_AV1_REF_LAST_FRAME + MAX (forward_idx, second_forward_idx);
      }
    }
  }

  if (skip_mode_allowed) {
    frame_header->skip_mode_present = AV1_READ_BIT_CHECKED (br, &retval);
    if (retval != GST_AV1_PARSER_OK)
      return retval;
  } else {
    frame_header->skip_mode_present = 0;
  }

  return GST_AV1_PARSER_OK;
}

/* 5.9.28 */
static gint
gst_av1_decode_subexp (GstBitReader * br, gint numSyms,
    GstAV1ParserResult * retval)
{
  gint i = 0;
  gint mk = 0;
  gint k = 3;
  gint subexp_final_bits = 0;
  gint subexp_more_bits = 0;
  gint subexp_bits = 0;

  while (1) {
    gint b2 = i ? k + i - 1 : k;
    gint a = 1 << b2;
    if (numSyms <= mk + 3 * a) {
      subexp_final_bits = av1_bitstreamfn_ns (br, numSyms - mk, retval);
      if (*retval != GST_AV1_PARSER_OK)
        return 0;
      return subexp_final_bits + mk;
    } else {
      subexp_more_bits = AV1_READ_BITS_CHECKED (br, 1, retval);
      if (*retval != GST_AV1_PARSER_OK)
        return 0;
      if (subexp_more_bits) {
        i++;
        mk += a;
      } else {
        subexp_bits = AV1_READ_BITS_CHECKED (br, b2, retval);
        if (*retval != GST_AV1_PARSER_OK)
          return 0;
        return subexp_bits + mk;
      }
    }
  }
}

/* 5.9.27 */
static gint
gst_av1_decode_unsigned_subexp_with_ref (GstBitReader * br, gint mx,
    gint r, GstAV1ParserResult * retval)
{
  gint v;

  v = gst_av1_decode_subexp (br, mx, retval);
  if ((r << 1) <= mx) {
    return av1_helper_inverse_recenter (r, v);
  } else {
    return mx - 1 - av1_helper_inverse_recenter (mx - 1 - r, v);
  }
}

/* 5.9.26 */
static gint
gst_av1_decode_signed_subexp_with_ref (GstBitReader * br, gint low,
    gint high, gint r, GstAV1ParserResult * retval)
{
  return gst_av1_decode_unsigned_subexp_with_ref (br,
      high - low, r - low, retval) + low;
}

/* 5.9.25 */
static GstAV1ParserResult
gst_av1_parse_global_param (GstAV1Parser * parser,
    GstAV1FrameHeaderOBU * frame_header, GstBitReader * br,
    GstAV1GlobalMotionParams * gm_params, GstAV1WarpModelType type,
    gint32 prev_gm_params[GST_AV1_NUM_REF_FRAMES][6], gint ref, gint idx)
{
  GstAV1ParserResult retval;
  gint prec_diff /* precDiff */ , wm_round, mx, r;
  gint abs_bits /* absBits */  = GST_AV1_GM_ABS_ALPHA_BITS;
  gint prec_bits /* precBits */  = GST_AV1_GM_ALPHA_PREC_BITS;
  gint sub;

  if (idx < 2) {
    if (type == GST_AV1_WARP_MODEL_TRANSLATION) {
      abs_bits =
          GST_AV1_GM_ABS_TRANS_ONLY_BITS -
          (frame_header->allow_high_precision_mv ? 0 : 1);
      prec_bits =
          GST_AV1_GM_TRANS_ONLY_PREC_BITS -
          (frame_header->allow_high_precision_mv ? 0 : 1);
    } else {
      abs_bits = GST_AV1_GM_ABS_TRANS_BITS;
      prec_bits = GST_AV1_GM_TRANS_PREC_BITS;
    }
  }

  prec_diff = GST_AV1_WARPEDMODEL_PREC_BITS - prec_bits;
  wm_round = (idx % 3) == 2 ? (1 << GST_AV1_WARPEDMODEL_PREC_BITS) : 0;
  sub = (idx % 3) == 2 ? (1 << prec_bits) : 0;
  mx = (1 << abs_bits);
  r = (prev_gm_params[ref][idx] >> prec_diff) - sub;
  gm_params->gm_params[ref][idx] =
      (gst_av1_decode_signed_subexp_with_ref (br, -mx, mx + 1, r,
          &retval) << prec_diff) + wm_round;
  if (retval != GST_AV1_PARSER_OK)
    return retval;
  return GST_AV1_PARSER_OK;
}

static gboolean
gst_av1_parser_is_shear_params_valid (gint32 gm_params[6])
{
  const gint32 *mat = gm_params;
  gint16 alpha, beta, gamma, delta;
  gint16 shift;
  gint16 y;
  gint16 v;
  guint i;
  gboolean default_warp_params;

  if (!(mat[2] > 0))
    return FALSE;

  default_warp_params = TRUE;
  for (i = 0; i < 6; i++) {
    if (gm_params[i] != ((i % 3 == 2) ? 1 << GST_AV1_WARPEDMODEL_PREC_BITS : 0)) {
      default_warp_params = FALSE;
      break;
    }
  }
  if (default_warp_params)
    return TRUE;

  alpha = CLAMP (mat[2] - (1 << GST_AV1_WARPEDMODEL_PREC_BITS),
      G_MININT16, G_MAXINT16);
  beta = CLAMP (mat[3], G_MININT16, G_MAXINT16);
  y = av1_helper_resolve_divisor_32 (ABS (mat[2]), &shift)
      * (mat[2] < 0 ? -1 : 1);
  v = ((gint64) mat[4] * (1 << GST_AV1_WARPEDMODEL_PREC_BITS)) * y;
  gamma =
      CLAMP ((gint) av1_helper_round_power_of_two_signed (v, shift), G_MININT16,
      G_MAXINT16);
  v = ((gint64) mat[3] * mat[4]) * y;
  delta =
      CLAMP (mat[5] - (gint) av1_helper_round_power_of_two_signed (v,
          shift) - (1 << GST_AV1_WARPEDMODEL_PREC_BITS), G_MININT16,
      G_MAXINT16);

  alpha =
      av1_helper_round_power_of_two_signed (alpha,
      GST_AV1_WARP_PARAM_REDUCE_BITS) * (1 << GST_AV1_WARP_PARAM_REDUCE_BITS);
  beta =
      av1_helper_round_power_of_two_signed (beta,
      GST_AV1_WARP_PARAM_REDUCE_BITS) * (1 << GST_AV1_WARP_PARAM_REDUCE_BITS);
  gamma =
      av1_helper_round_power_of_two_signed (gamma,
      GST_AV1_WARP_PARAM_REDUCE_BITS) * (1 << GST_AV1_WARP_PARAM_REDUCE_BITS);
  delta =
      av1_helper_round_power_of_two_signed (delta,
      GST_AV1_WARP_PARAM_REDUCE_BITS) * (1 << GST_AV1_WARP_PARAM_REDUCE_BITS);

  if ((4 * ABS (alpha) + 7 * ABS (beta) >= (1 << GST_AV1_WARPEDMODEL_PREC_BITS))
      || (4 * ABS (gamma) + 4 * ABS (delta) >=
          (1 << GST_AV1_WARPEDMODEL_PREC_BITS)))
    return FALSE;

  return TRUE;
}

/* 5.9.24 */
static GstAV1ParserResult
gst_av1_parse_global_motion_params (GstAV1Parser * parser,
    GstBitReader * br, GstAV1FrameHeaderOBU * frame_header)
{
  GstAV1WarpModelType type;
  GstAV1ParserResult retval = GST_AV1_PARSER_OK;
  gint i, ref;
  GstAV1GlobalMotionParams *gm_params = &(frame_header->global_motion_params);
  gint32 prev_gm_params[GST_AV1_NUM_REF_FRAMES][6] /* PrevGmParams */ ;

  /* init value */
  gm_params->gm_type[GST_AV1_REF_INTRA_FRAME] = GST_AV1_WARP_MODEL_IDENTITY;
  for (ref = GST_AV1_REF_LAST_FRAME; ref <= GST_AV1_REF_ALTREF_FRAME; ref++) {
    gm_params->invalid[ref] = 0;
    gm_params->gm_type[ref] = GST_AV1_WARP_MODEL_IDENTITY;
    for (i = 0; i < 6; i++) {
      gm_params->gm_params[ref][i] =
          ((i % 3 == 2) ? 1 << GST_AV1_WARPEDMODEL_PREC_BITS : 0);
    }
  }

  if (frame_header->frame_is_intra)
    goto success;

  if (frame_header->primary_ref_frame != GST_AV1_PRIMARY_REF_NONE) {
    GstAV1GlobalMotionParams *ref_global_motion_params =
        &parser->state.ref_info.entry[frame_header->
        ref_frame_idx[frame_header->primary_ref_frame]].
        ref_global_motion_params;
    memcpy (prev_gm_params, ref_global_motion_params->gm_params,
        sizeof (gint32) * GST_AV1_NUM_REF_FRAMES * 6);
  } else {
    for (ref = GST_AV1_REF_INTRA_FRAME; ref < GST_AV1_NUM_REF_FRAMES; ref++)
      for (i = 0; i < 6; i++)
        prev_gm_params[ref][i] =
            ((i % 3 == 2) ? 1 << GST_AV1_WARPEDMODEL_PREC_BITS : 0);
  }

  for (ref = GST_AV1_REF_LAST_FRAME; ref <= GST_AV1_REF_ALTREF_FRAME; ref++) {
    gm_params->is_global[ref] = AV1_READ_BIT_CHECKED (br, &retval);
    if (retval != GST_AV1_PARSER_OK)
      goto error;

    if (gm_params->is_global[ref]) {
      gm_params->is_rot_zoom[ref] = AV1_READ_BIT_CHECKED (br, &retval);
      if (retval != GST_AV1_PARSER_OK)
        goto error;

      if (gm_params->is_rot_zoom[ref]) {
        type = GST_AV1_WARP_MODEL_ROTZOOM;
      } else {
        gm_params->is_translation[ref] = AV1_READ_BIT_CHECKED (br, &retval);
        if (retval != GST_AV1_PARSER_OK)
          goto error;
        type =
            gm_params->is_translation[ref] ? GST_AV1_WARP_MODEL_TRANSLATION :
            GST_AV1_WARP_MODEL_AFFINE;
      }
    } else {
      type = GST_AV1_WARP_MODEL_IDENTITY;
    }
    gm_params->gm_type[ref] = type;

    if (type >= GST_AV1_WARP_MODEL_ROTZOOM) {
      retval =
          gst_av1_parse_global_param (parser, frame_header, br, gm_params, type,
          prev_gm_params, ref, 2);
      if (retval != GST_AV1_PARSER_OK)
        goto error;

      retval =
          gst_av1_parse_global_param (parser, frame_header, br, gm_params, type,
          prev_gm_params, ref, 3);
      if (retval != GST_AV1_PARSER_OK)
        goto error;

      if (type == GST_AV1_WARP_MODEL_AFFINE) {
        retval =
            gst_av1_parse_global_param (parser, frame_header, br, gm_params,
            type, prev_gm_params, ref, 4);
        if (retval != GST_AV1_PARSER_OK)
          goto error;

        retval =
            gst_av1_parse_global_param (parser, frame_header, br, gm_params,
            type, prev_gm_params, ref, 5);
        if (retval != GST_AV1_PARSER_OK)
          goto error;
      } else {
        gm_params->gm_params[ref][4] = gm_params->gm_params[ref][3] * (-1);
        gm_params->gm_params[ref][5] = gm_params->gm_params[ref][2];
      }
    }
    if (type >= GST_AV1_WARP_MODEL_TRANSLATION) {
      retval =
          gst_av1_parse_global_param (parser, frame_header, br, gm_params, type,
          prev_gm_params, ref, 0);
      if (retval != GST_AV1_PARSER_OK)
        goto error;
      retval =
          gst_av1_parse_global_param (parser, frame_header, br, gm_params, type,
          prev_gm_params, ref, 1);
      if (retval != GST_AV1_PARSER_OK)
        goto error;
    }

    if (type <= GST_AV1_WARP_MODEL_AFFINE)
      gm_params->invalid[ref] =
          !gst_av1_parser_is_shear_params_valid (gm_params->gm_params[ref]);
  }

success:
  return GST_AV1_PARSER_OK;

error:
  GST_WARNING ("parse global motion params error %d", retval);
  return retval;
}

/* 5.9.30 */
static GstAV1ParserResult
gst_av1_parse_film_grain_params (GstAV1Parser * parser, GstBitReader * br,
    GstAV1FrameHeaderOBU * frame_header)
{
  GstAV1FilmGrainParams *fg_params;
  GstAV1SequenceHeaderOBU *seq_header;
  gint i;
  gint num_pos_chroma /* numPosChroma */ , num_pos_luma /* numPosLuma */ ;
  GstAV1ParserResult ret = GST_AV1_PARSER_OK;

  g_assert (parser->seq_header);

  fg_params = &frame_header->film_grain_params;
  seq_header = parser->seq_header;
  if (!seq_header->film_grain_params_present || (!frame_header->show_frame
          && !frame_header->showable_frame)) {
    /* reset_grain_params() is a function call that indicates that all the
       syntax elements read in film_grain_params should be set equal to 0. */
    memset (fg_params, 0, sizeof (*fg_params));
    goto success;
  }

  fg_params->apply_grain = AV1_READ_BIT_CHECKED (br, &ret);
  if (ret != GST_AV1_PARSER_OK)
    goto error;
  if (!fg_params->apply_grain) {
    /* reset_grain_params() */
    memset (fg_params, 0, sizeof (*fg_params));
    goto success;
  }

  fg_params->grain_seed = AV1_READ_UINT16_CHECKED (br, &ret);
  if (ret != GST_AV1_PARSER_OK)
    goto error;

  if (frame_header->frame_type == GST_AV1_INTER_FRAME) {
    fg_params->update_grain = AV1_READ_BIT_CHECKED (br, &ret);
    if (ret != GST_AV1_PARSER_OK)
      goto error;
  } else {
    fg_params->update_grain = 1;
  }

  if (!fg_params->update_grain) {
    guint16 temp_grain_seed /* tempGrainSeed */ ;
    gint j;
    gboolean found = FALSE;

    fg_params->film_grain_params_ref_idx = AV1_READ_BITS_CHECKED (br, 3, &ret);
    if (ret != GST_AV1_PARSER_OK)
      goto error;

    for (j = 0; j < GST_AV1_REFS_PER_FRAME; j++) {
      if (frame_header->ref_frame_idx[j] ==
          fg_params->film_grain_params_ref_idx) {
        found = TRUE;
        break;
      }
    }

    if (!found) {
      GST_INFO ("Invalid film grain reference idx %d.",
          fg_params->film_grain_params_ref_idx);
      ret = GST_AV1_PARSER_BITSTREAM_ERROR;
      goto error;
    }

    if (!parser->state.ref_info.entry[fg_params->film_grain_params_ref_idx].
        ref_valid) {
      GST_INFO ("Invalid ref info of film grain idx %d.",
          fg_params->film_grain_params_ref_idx);
      ret = GST_AV1_PARSER_BITSTREAM_ERROR;
      goto error;
    }

    temp_grain_seed = fg_params->grain_seed;
    memcpy (fg_params,
        &parser->state.ref_info.entry[fg_params->film_grain_params_ref_idx].
        ref_film_grain_params, sizeof (GstAV1FilmGrainParams));
    fg_params->grain_seed = temp_grain_seed;

    goto success;
  }

  fg_params->num_y_points = AV1_READ_BITS_CHECKED (br, 4, &ret);
  if (ret != GST_AV1_PARSER_OK)
    goto error;

  for (i = 0; i < fg_params->num_y_points; i++) {
    if (AV1_REMAINING_BITS (br) < 8 + 8) {
      ret = GST_AV1_PARSER_NO_MORE_DATA;
      goto error;
    }
    fg_params->point_y_value[i] = AV1_READ_UINT8 (br);
    fg_params->point_y_scaling[i] = AV1_READ_UINT8 (br);
  }

  if (seq_header->color_config.mono_chrome) {
    fg_params->chroma_scaling_from_luma = 0;
  } else {
    fg_params->chroma_scaling_from_luma = AV1_READ_BIT_CHECKED (br, &ret);
    if (ret != GST_AV1_PARSER_OK)
      goto error;
  }

  if (seq_header->color_config.mono_chrome
      || fg_params->chroma_scaling_from_luma
      || (seq_header->color_config.subsampling_x == 1
          && seq_header->color_config.subsampling_y == 1
          && fg_params->num_y_points == 0)) {
    fg_params->num_cb_points = 0;
    fg_params->num_cr_points = 0;
  } else {
    fg_params->num_cb_points = AV1_READ_BITS_CHECKED (br, 4, &ret);
    if (ret != GST_AV1_PARSER_OK)
      goto error;
    for (i = 0; i < fg_params->num_cb_points; i++) {
      if (AV1_REMAINING_BITS (br) < 8 + 8) {
        ret = GST_AV1_PARSER_NO_MORE_DATA;
        goto error;
      }
      fg_params->point_cb_value[i] = AV1_READ_UINT8 (br);
      fg_params->point_cb_scaling[i] = AV1_READ_UINT8 (br);
    }

    fg_params->num_cr_points = AV1_READ_BITS_CHECKED (br, 4, &ret);
    if (ret != GST_AV1_PARSER_OK)
      goto error;
    for (i = 0; i < fg_params->num_cr_points; i++) {
      if (AV1_REMAINING_BITS (br) < 8 + 8) {
        ret = GST_AV1_PARSER_NO_MORE_DATA;
        goto error;
      }
      fg_params->point_cr_value[i] = AV1_READ_UINT8 (br);
      fg_params->point_cr_scaling[i] = AV1_READ_UINT8 (br);
    }
  }

  fg_params->grain_scaling_minus_8 = AV1_READ_BITS_CHECKED (br, 2, &ret);
  if (ret != GST_AV1_PARSER_OK)
    goto error;

  fg_params->ar_coeff_lag = AV1_READ_BITS_CHECKED (br, 2, &ret);
  if (ret != GST_AV1_PARSER_OK)
    goto error;

  num_pos_luma = 2 * fg_params->ar_coeff_lag * (fg_params->ar_coeff_lag + 1);
  if (fg_params->num_y_points) {
    num_pos_chroma = num_pos_luma + 1;
    for (i = 0; i < num_pos_luma; i++) {
      fg_params->ar_coeffs_y_plus_128[i] = AV1_READ_UINT8_CHECKED (br, &ret);
      if (ret != GST_AV1_PARSER_OK)
        goto error;
    }
  } else {
    num_pos_chroma = num_pos_luma;
  }

  if (fg_params->chroma_scaling_from_luma || fg_params->num_cb_points) {
    for (i = 0; i < num_pos_chroma; i++) {
      fg_params->ar_coeffs_cb_plus_128[i] = AV1_READ_UINT8_CHECKED (br, &ret);
      if (ret != GST_AV1_PARSER_OK)
        goto error;
    }
  }

  if (fg_params->chroma_scaling_from_luma || fg_params->num_cr_points) {
    for (i = 0; i < num_pos_chroma; i++) {
      fg_params->ar_coeffs_cr_plus_128[i] = AV1_READ_UINT8_CHECKED (br, &ret);
      if (ret != GST_AV1_PARSER_OK)
        goto error;
    }
  }

  if (AV1_REMAINING_BITS (br) < 2 + 2) {
    ret = GST_AV1_PARSER_NO_MORE_DATA;
    goto error;
  }
  fg_params->ar_coeff_shift_minus_6 = AV1_READ_BITS (br, 2);
  fg_params->grain_scale_shift = AV1_READ_BITS (br, 2);

  if (fg_params->num_cb_points) {
    if (AV1_REMAINING_BITS (br) < 8 + 8 + 9) {
      ret = GST_AV1_PARSER_NO_MORE_DATA;
      goto error;
    }
    fg_params->cb_mult = AV1_READ_BITS (br, 8);
    fg_params->cb_luma_mult = AV1_READ_BITS (br, 8);
    fg_params->cb_offset = AV1_READ_BITS (br, 9);
  }

  if (fg_params->num_cr_points) {
    if (AV1_REMAINING_BITS (br) < 8 + 8 + 9) {
      ret = GST_AV1_PARSER_NO_MORE_DATA;
      goto error;
    }
    fg_params->cr_mult = AV1_READ_BITS (br, 8);
    fg_params->cr_luma_mult = AV1_READ_BITS (br, 8);
    fg_params->cr_offset = AV1_READ_BITS (br, 9);
  }

  if (AV1_REMAINING_BITS (br) < 2) {
    ret = GST_AV1_PARSER_NO_MORE_DATA;
    goto error;
  }
  fg_params->overlap_flag = AV1_READ_BIT (br);
  fg_params->clip_to_restricted_range = AV1_READ_BIT (br);

success:
  return GST_AV1_PARSER_OK;

error:
  GST_WARNING ("parse film grain params error %d", ret);
  return ret;
}

/* 5.9.4 */
static void
gst_av1_mark_ref_frames (GstAV1Parser * parser, GstBitReader * br, gint idLen)
{
  GstAV1ReferenceFrameInfo *ref_info;
  GstAV1SequenceHeaderOBU *seq_header;
  gint i, diff_len /* diffLen */ ;

  seq_header = parser->seq_header;
  ref_info = &(parser->state.ref_info);
  diff_len = seq_header->delta_frame_id_length_minus_2 + 2;

  for (i = 0; i < GST_AV1_NUM_REF_FRAMES; i++) {
    if (parser->state.current_frame_id > (1 << diff_len)) {
      if (ref_info->entry[i].ref_frame_id > parser->state.current_frame_id
          || ref_info->entry[i].ref_frame_id <
          (parser->state.current_frame_id - (1 << diff_len)))
        ref_info->entry[i].ref_valid = 0;
    } else {
      if (ref_info->entry[i].ref_frame_id > parser->state.current_frame_id
          && ref_info->entry[i].ref_frame_id <
          ((1 << idLen) + parser->state.current_frame_id - (1 << diff_len)))
        ref_info->entry[i].ref_valid = 0;
    }
  }
}

/* 5.11.14 */
static gboolean
gst_av1_seg_feature_active_idx (GstAV1Parser * parser,
    GstAV1FrameHeaderOBU * frame_header, gint idx, gint feature)
{
  return frame_header->segmentation_params.segmentation_enabled
      && frame_header->segmentation_params.feature_enabled[idx][feature];
}

/* 7.12.2 */
static gint
gst_av1_get_qindex (GstAV1Parser * parser,
    GstAV1FrameHeaderOBU * frame_header, gboolean ignoreDeltaQ, gint segmentId)
{
  gint qindex;
  if (gst_av1_seg_feature_active_idx (parser, frame_header, segmentId,
          GST_AV1_SEG_LVL_ALT_Q)) {
    gint data =
        frame_header->
        segmentation_params.feature_data[segmentId][GST_AV1_SEG_LVL_ALT_Q];
    qindex = frame_header->quantization_params.base_q_idx + data;
    if (ignoreDeltaQ == 0 && frame_header->quantization_params.delta_q_present)
      qindex = qindex + frame_header->quantization_params.delta_q_res;
    return CLAMP (qindex, 0, 255);
  } else
    return frame_header->quantization_params.base_q_idx;
}

/* 7.8 */
static void
gst_av1_set_frame_refs (GstAV1Parser * parser,
    GstAV1SequenceHeaderOBU * seq_header, GstAV1FrameHeaderOBU * frame_header)
{
  const GstAV1ReferenceFrame ref_frame_list[GST_AV1_REFS_PER_FRAME - 2] = {
    GST_AV1_REF_LAST2_FRAME,
    GST_AV1_REF_LAST3_FRAME,
    GST_AV1_REF_BWDREF_FRAME,
    GST_AV1_REF_ALTREF2_FRAME,
    GST_AV1_REF_ALTREF_FRAME
  };
  gboolean used_frame[GST_AV1_NUM_REF_FRAMES];
  gint shifted_order_hints[GST_AV1_NUM_REF_FRAMES];
  gint cur_frame_hint = 1 << (seq_header->order_hint_bits - 1);
  gint last_order_hint, earliest_order_hint;
  gint ref, hint;
  gint i, j;

  g_assert (seq_header->enable_order_hint);
  g_assert (seq_header->order_hint_bits_minus_1 >= 0);

  for (i = 0; i < GST_AV1_REFS_PER_FRAME; i++)
    frame_header->ref_frame_idx[i] = -1;
  frame_header->ref_frame_idx[GST_AV1_REF_LAST_FRAME -
      GST_AV1_REF_LAST_FRAME] = frame_header->last_frame_idx;
  frame_header->ref_frame_idx[GST_AV1_REF_GOLDEN_FRAME -
      GST_AV1_REF_LAST_FRAME] = frame_header->gold_frame_idx;

  for (i = 0; i < GST_AV1_NUM_REF_FRAMES; i++)
    used_frame[i] = 0;
  used_frame[frame_header->last_frame_idx] = 1;
  used_frame[frame_header->gold_frame_idx] = 1;

  for (i = 0; i < GST_AV1_NUM_REF_FRAMES; i++)
    shifted_order_hints[i] = cur_frame_hint +
        gst_av1_get_relative_dist (seq_header,
        parser->state.ref_info.entry[i].ref_order_hint,
        frame_header->order_hint);

  last_order_hint = shifted_order_hints[frame_header->last_frame_idx];
  earliest_order_hint = shifted_order_hints[frame_header->gold_frame_idx];

  /* === Backward Reference Frames === */
  /* The ALTREF_FRAME reference is set to be a backward
     reference to the frame with highest output order. */
  ref = -1;
  for (i = 0; i < GST_AV1_NUM_REF_FRAMES; i++) {
    hint = shifted_order_hints[i];
    if (!used_frame[i] && hint >= cur_frame_hint
        && (ref < 0 || hint >= last_order_hint)) {
      ref = i;
      last_order_hint = hint;
    }
  }
  if (ref >= 0) {
    frame_header->ref_frame_idx[GST_AV1_REF_ALTREF_FRAME -
        GST_AV1_REF_LAST_FRAME] = ref;
    used_frame[ref] = 1;
  }

  /* The BWDREF_FRAME reference is set to be a backward reference
     to the closest frame. */
  ref = -1;
  for (i = 0; i < GST_AV1_NUM_REF_FRAMES; i++) {
    hint = shifted_order_hints[i];
    if (!used_frame[i] && hint >= cur_frame_hint
        && (ref < 0 || hint < earliest_order_hint)) {
      ref = i;
      earliest_order_hint = hint;
    }
  }
  if (ref >= 0) {
    frame_header->ref_frame_idx[GST_AV1_REF_BWDREF_FRAME -
        GST_AV1_REF_LAST_FRAME] = ref;
    used_frame[ref] = 1;
  }

  /* The ALTREF2_FRAME reference is set to the next closest
     backward reference. */
  ref = -1;
  for (i = 0; i < GST_AV1_NUM_REF_FRAMES; i++) {
    hint = shifted_order_hints[i];
    if (!used_frame[i] && hint >= cur_frame_hint
        && (ref < 0 || hint < earliest_order_hint)) {
      ref = i;
      earliest_order_hint = hint;
    }
  }
  if (ref >= 0) {
    frame_header->ref_frame_idx[GST_AV1_REF_ALTREF2_FRAME -
        GST_AV1_REF_LAST_FRAME] = ref;
    used_frame[ref] = 1;
  }

  /* === Forward Reference Frames === */

  /* The remaining references are set to be forward references
     in anti-chronological order. */
  for (i = 0; i < GST_AV1_REFS_PER_FRAME - 2; i++) {
    GstAV1ReferenceFrame ref_frame = ref_frame_list[i];
    if (frame_header->ref_frame_idx[ref_frame - GST_AV1_REF_LAST_FRAME] < 0) {
      ref = -1;
      for (j = 0; j < GST_AV1_NUM_REF_FRAMES; j++) {
        hint = shifted_order_hints[j];
        if (!used_frame[j] && hint < cur_frame_hint &&
            (ref < 0 || hint >= last_order_hint)) {
          ref = j;
          last_order_hint = hint;
        }
      }

      if (ref >= 0) {
        frame_header->ref_frame_idx[ref_frame - GST_AV1_REF_LAST_FRAME] = ref;
        used_frame[ref] = 1;
      }
    }
  }

  /* Finally, any remaining references are set to the reference frame
     with smallest output order. */
  ref = -1;
  for (i = 0; i < GST_AV1_NUM_REF_FRAMES; i++) {
    hint = shifted_order_hints[i];
    if (ref < 0 || hint < earliest_order_hint) {
      ref = i;
      earliest_order_hint = hint;
    }
  }
  for (i = 0; i < GST_AV1_REFS_PER_FRAME; i++)
    if (frame_header->ref_frame_idx[i] < 0)
      frame_header->ref_frame_idx[i] = ref;
}

/* 7.21 */
static void
gst_av1_parser_reference_frame_loading (GstAV1Parser * parser,
    GstAV1FrameHeaderOBU * frame_header)
{
  GstAV1ReferenceFrameInfo *ref_info = &(parser->state.ref_info);
  gint idx = frame_header->frame_to_show_map_idx;
  GstAV1TileInfo *ref_tile_info = &ref_info->entry[idx].ref_tile_info;
  const gint all_frames = (1 << GST_AV1_NUM_REF_FRAMES) - 1;

  /* copy the relevant frame information as these will be needed by
   * all subclasses. */
  frame_header->frame_type = ref_info->entry[idx].ref_frame_type;
  frame_header->upscaled_width = ref_info->entry[idx].ref_upscaled_width;
  frame_header->frame_width = ref_info->entry[idx].ref_frame_width;
  frame_header->frame_height = ref_info->entry[idx].ref_frame_height;
  frame_header->render_width = ref_info->entry[idx].ref_render_width;
  frame_header->render_height = ref_info->entry[idx].ref_render_height;

  if (parser->seq_header->film_grain_params_present)
    frame_header->film_grain_params =
        ref_info->entry[idx].ref_film_grain_params;

  /* the remaining is only relevant to ensure proper state update and only
   * keyframe updates the state. */
  if (frame_header->frame_type != GST_AV1_KEY_FRAME)
    return;

  frame_header->refresh_frame_flags = all_frames;
  frame_header->current_frame_id = ref_info->entry[idx].ref_frame_id;
  frame_header->order_hint = ref_info->entry[idx].ref_order_hint;
  frame_header->segmentation_params =
      ref_info->entry[idx].ref_segmentation_params;
  frame_header->global_motion_params =
      ref_info->entry[idx].ref_global_motion_params;
  frame_header->loop_filter_params = ref_info->entry[idx].ref_lf_params;
  frame_header->tile_info = *ref_tile_info;

  parser->state.current_frame_id = ref_info->entry[idx].ref_frame_id;
  parser->state.upscaled_width = ref_info->entry[idx].ref_upscaled_width;
  parser->state.frame_width = ref_info->entry[idx].ref_frame_width;
  parser->state.frame_height = ref_info->entry[idx].ref_frame_height;
  parser->state.render_width = ref_info->entry[idx].ref_render_width;
  parser->state.render_height = ref_info->entry[idx].ref_render_height;
  parser->state.mi_cols = ref_info->entry[idx].ref_mi_cols;
  parser->state.mi_rows = ref_info->entry[idx].ref_mi_rows;

  memcpy (parser->state.mi_col_starts, ref_tile_info->mi_col_starts,
      sizeof (guint32) * (GST_AV1_MAX_TILE_COLS + 1));
  memcpy (parser->state.mi_row_starts, ref_tile_info->mi_row_starts,
      sizeof (guint32) * (GST_AV1_MAX_TILE_ROWS + 1));
  parser->state.tile_cols_log2 = ref_tile_info->tile_cols_log2;
  parser->state.tile_cols = ref_tile_info->tile_cols;
  parser->state.tile_rows_log2 = ref_tile_info->tile_rows_log2;
  parser->state.tile_rows = ref_tile_info->tile_rows;
  parser->state.tile_size_bytes = ref_tile_info->tile_size_bytes;
}

/* 5.9.2 */
static GstAV1ParserResult
gst_av1_parse_uncompressed_frame_header (GstAV1Parser * parser, GstAV1OBU * obu,
    GstBitReader * br, GstAV1FrameHeaderOBU * frame_header)
{
  GstAV1ParserResult retval = GST_AV1_PARSER_OK;
  GstAV1ReferenceFrameInfo *ref_info;
  GstAV1SequenceHeaderOBU *seq_header;
  gint i, op_num /* opNum */ ;
  gint segment_id /* segmentId */ , all_frames /* allFrames */ ;
  gint id_len /* idLen */  = 0;

  if (!parser->seq_header) {
    GST_WARNING ("Missing OBU Reference: seq_header");
    retval = GST_AV1_PARSER_MISSING_OBU_REFERENCE;
    goto error;
  }

  seq_header = parser->seq_header;
  ref_info = &(parser->state.ref_info);
  if (seq_header->frame_id_numbers_present_flag)
    id_len = seq_header->additional_frame_id_length_minus_1 + 1 +
        seq_header->delta_frame_id_length_minus_2 + 2;
  all_frames = (1 << GST_AV1_NUM_REF_FRAMES) - 1;

  if (seq_header->reduced_still_picture_header) {
    frame_header->show_existing_frame = 0;
    frame_header->frame_type = GST_AV1_KEY_FRAME;
    frame_header->frame_is_intra = 1;
    frame_header->show_frame = 1;
    frame_header->showable_frame = 0;
    if (parser->state.sequence_changed) {
      /* This is the start of a new coded video sequence. */
      parser->state.sequence_changed = 0;
      parser->state.begin_first_frame = 1;
    }
  } else {
    frame_header->show_existing_frame = AV1_READ_BIT_CHECKED (br, &retval);
    if (retval != GST_AV1_PARSER_OK)
      goto error;

    if (frame_header->show_existing_frame) {
      if (parser->state.sequence_changed) {
        GST_INFO ("New sequence header starts with a show_existing_frame.");
        retval = GST_AV1_PARSER_BITSTREAM_ERROR;
        goto error;
      }

      frame_header->frame_to_show_map_idx =
          AV1_READ_BITS_CHECKED (br, 3, &retval);
      if (retval != GST_AV1_PARSER_OK)
        goto error;

      if (!ref_info->entry[frame_header->frame_to_show_map_idx].ref_valid) {
        GST_INFO ("The frame_to_show %d is invalid.",
            frame_header->frame_to_show_map_idx);
        retval = GST_AV1_PARSER_BITSTREAM_ERROR;
        goto error;
      }

      if (seq_header->decoder_model_info_present_flag
          && !seq_header->timing_info.equal_picture_interval)
        frame_header->frame_presentation_time =
            AV1_READ_BITS_CHECKED (br,
            seq_header->
            decoder_model_info.frame_presentation_time_length_minus_1 + 1,
            &retval);
      if (retval != GST_AV1_PARSER_OK)
        goto error;

      frame_header->refresh_frame_flags = 0;
      if (seq_header->frame_id_numbers_present_flag) {
        g_assert (id_len > 0);
        frame_header->display_frame_id =
            AV1_READ_BITS_CHECKED (br, id_len, &retval);
        if (retval != GST_AV1_PARSER_OK)
          goto error;
        if (frame_header->display_frame_id !=
            ref_info->entry[frame_header->frame_to_show_map_idx].ref_frame_id) {
          GST_INFO ("Reference frame ID mismatch");
          retval = GST_AV1_PARSER_BITSTREAM_ERROR;
          goto error;
        }
      }

      gst_av1_parser_reference_frame_loading (parser, frame_header);
      goto success;
    }

    frame_header->frame_type = AV1_READ_BITS_CHECKED (br, 2, &retval);
    if (retval != GST_AV1_PARSER_OK)
      goto error;

    if (parser->state.sequence_changed) {
      if (frame_header->frame_type == GST_AV1_KEY_FRAME) {
        /* This is the start of a new coded video sequence. */
        parser->state.sequence_changed = FALSE;
        parser->state.begin_first_frame = TRUE;
      } else {
        GST_INFO ("Sequence header has changed without a keyframe.");
        retval = GST_AV1_PARSER_BITSTREAM_ERROR;
        goto error;
      }
    }

    frame_header->frame_is_intra =
        (frame_header->frame_type == GST_AV1_INTRA_ONLY_FRAME
        || frame_header->frame_type == GST_AV1_KEY_FRAME);

    frame_header->show_frame = AV1_READ_BIT_CHECKED (br, &retval);
    if (retval != GST_AV1_PARSER_OK)
      goto error;

    if (seq_header->still_picture &&
        (frame_header->frame_type != GST_AV1_KEY_FRAME
            || !frame_header->show_frame)) {
      GST_INFO ("Still pictures must be coded as shown keyframes");
      retval = GST_AV1_PARSER_BITSTREAM_ERROR;
      goto error;
    }

    if (frame_header->show_frame
        && seq_header->decoder_model_info_present_flag
        && !seq_header->timing_info.equal_picture_interval) {
      frame_header->frame_presentation_time =
          AV1_READ_BITS_CHECKED (br,
          seq_header->decoder_model_info.
          frame_presentation_time_length_minus_1 + 1, &retval);
      if (retval != GST_AV1_PARSER_OK)
        goto error;
    }

    if (frame_header->show_frame) {
      frame_header->showable_frame =
          (frame_header->frame_type != GST_AV1_KEY_FRAME);
    } else {
      frame_header->showable_frame = AV1_READ_BIT_CHECKED (br, &retval);
      if (retval != GST_AV1_PARSER_OK)
        goto error;
    }

    if (frame_header->frame_type == GST_AV1_SWITCH_FRAME
        || (frame_header->frame_type == GST_AV1_KEY_FRAME
            && frame_header->show_frame))
      frame_header->error_resilient_mode = 1;
    else {
      frame_header->error_resilient_mode = AV1_READ_BIT_CHECKED (br, &retval);
      if (retval != GST_AV1_PARSER_OK)
        goto error;
    }
  }

  if (frame_header->frame_type == GST_AV1_KEY_FRAME && frame_header->show_frame) {
    for (i = 0; i < GST_AV1_NUM_REF_FRAMES; i++) {
      ref_info->entry[i].ref_valid = 0;
      ref_info->entry[i].ref_order_hint = 0;
    }
    for (i = 0; i < GST_AV1_REFS_PER_FRAME; i++) {
      frame_header->order_hints[GST_AV1_REF_LAST_FRAME + i] = 0;
    }
  }

  frame_header->disable_cdf_update = AV1_READ_BIT_CHECKED (br, &retval);
  if (retval != GST_AV1_PARSER_OK)
    goto error;

  if (seq_header->seq_force_screen_content_tools ==
      GST_AV1_SELECT_SCREEN_CONTENT_TOOLS) {
    frame_header->allow_screen_content_tools =
        AV1_READ_BIT_CHECKED (br, &retval);
    if (retval != GST_AV1_PARSER_OK)
      goto error;
  } else {
    frame_header->allow_screen_content_tools =
        seq_header->seq_force_screen_content_tools;
  }

  if (frame_header->allow_screen_content_tools) {
    if (seq_header->seq_force_integer_mv == GST_AV1_SELECT_INTEGER_MV) {
      frame_header->force_integer_mv = AV1_READ_BIT_CHECKED (br, &retval);
      if (retval != GST_AV1_PARSER_OK)
        goto error;
    } else {
      frame_header->force_integer_mv = seq_header->seq_force_integer_mv;
    }
  } else {
    frame_header->force_integer_mv = 0;
  }

  if (frame_header->frame_is_intra) {
    frame_header->force_integer_mv = 1;
  }

  if (seq_header->frame_id_numbers_present_flag) {
    gboolean have_prev_frame_id =
        !parser->state.begin_first_frame &&
        (!(frame_header->frame_type == GST_AV1_KEY_FRAME
            && frame_header->show_frame));
    if (have_prev_frame_id)
      parser->state.prev_frame_id = parser->state.current_frame_id;

    g_assert (id_len > 0);
    frame_header->current_frame_id =
        AV1_READ_BITS_CHECKED (br, id_len, &retval);
    if (retval != GST_AV1_PARSER_OK)
      goto error;

    parser->state.current_frame_id = frame_header->current_frame_id;
    /* Check whether the id and id diff is valid */
    if (have_prev_frame_id) {
      gint32 diff_frame_id;
      if (parser->state.current_frame_id > parser->state.prev_frame_id) {
        diff_frame_id =
            parser->state.current_frame_id - parser->state.prev_frame_id;
      } else {
        diff_frame_id = (1 << id_len) +
            parser->state.current_frame_id - parser->state.prev_frame_id;
      }
      if (parser->state.current_frame_id == parser->state.prev_frame_id ||
          diff_frame_id >= (1 << (id_len - 1))) {
        GST_INFO ("Invalid value of current_frame_id");
        retval = GST_AV1_PARSER_BITSTREAM_ERROR;
        goto error;
      }
    }

    gst_av1_mark_ref_frames (parser, br, id_len);
  } else {
    frame_header->current_frame_id = 0;
    parser->state.prev_frame_id = parser->state.current_frame_id;
    parser->state.current_frame_id = frame_header->current_frame_id;
  }

  if (frame_header->frame_type == GST_AV1_SWITCH_FRAME) {
    frame_header->frame_size_override_flag = 1;
  } else if (seq_header->reduced_still_picture_header) {
    frame_header->frame_size_override_flag = 0;
  } else {
    frame_header->frame_size_override_flag = AV1_READ_BIT_CHECKED (br, &retval);
    if (retval != GST_AV1_PARSER_OK)
      goto error;
  }

  frame_header->order_hint =
      AV1_READ_BITS_CHECKED (br, seq_header->order_hint_bits_minus_1 + 1,
      &retval);
  if (retval != GST_AV1_PARSER_OK)
    goto error;

  if (frame_header->frame_is_intra || frame_header->error_resilient_mode) {
    frame_header->primary_ref_frame = GST_AV1_PRIMARY_REF_NONE;
  } else {
    frame_header->primary_ref_frame = AV1_READ_BITS_CHECKED (br, 3, &retval);
    if (retval != GST_AV1_PARSER_OK)
      goto error;
  }

  if (seq_header->decoder_model_info_present_flag) {
    frame_header->buffer_removal_time_present_flag =
        AV1_READ_BIT_CHECKED (br, &retval);
    if (retval != GST_AV1_PARSER_OK)
      goto error;

    if (frame_header->buffer_removal_time_present_flag) {
      for (op_num = 0; op_num <= seq_header->operating_points_cnt_minus_1;
          op_num++) {
        if (seq_header->
            operating_points[op_num].decoder_model_present_for_this_op) {
          gint op_pt_idc = seq_header->operating_points[op_num].idc;
          gint in_temporal_layer =
              (op_pt_idc >> obu->header.obu_temporal_id) & 1;
          gint in_spatial_layer =
              (op_pt_idc >> (obu->header.obu_spatial_id + 8)) & 1;
          if (op_pt_idc == 0 || (in_temporal_layer && in_spatial_layer)) {
            frame_header->buffer_removal_time[op_num] =
                AV1_READ_BITS_CHECKED (br,
                seq_header->decoder_model_info.
                buffer_removal_time_length_minus_1 + 1, &retval);
            if (retval != GST_AV1_PARSER_OK)
              goto error;
          } else {
            frame_header->buffer_removal_time[op_num] = 0;
          }
        } else {
          frame_header->buffer_removal_time[op_num] = 0;
        }
      }
    }
  }

  frame_header->allow_high_precision_mv = 0;
  frame_header->use_ref_frame_mvs = 0;
  frame_header->allow_intrabc = 0;
  if (frame_header->frame_type == GST_AV1_SWITCH_FRAME ||
      (frame_header->frame_type == GST_AV1_KEY_FRAME
          && frame_header->show_frame)) {
    frame_header->refresh_frame_flags = all_frames;
  } else {
    frame_header->refresh_frame_flags = AV1_READ_UINT8_CHECKED (br, &retval);
    if (retval != GST_AV1_PARSER_OK)
      goto error;
  }
  if (frame_header->frame_type == GST_AV1_INTRA_ONLY_FRAME) {
    if (frame_header->refresh_frame_flags == 0xFF) {
      GST_INFO ("Intra only frames cannot have refresh flags 0xFF");
      retval = GST_AV1_PARSER_BITSTREAM_ERROR;
      goto error;
    }
  }

  if (!frame_header->frame_is_intra
      || frame_header->refresh_frame_flags != all_frames) {
    if (frame_header->error_resilient_mode && seq_header->enable_order_hint) {
      for (i = 0; i < GST_AV1_NUM_REF_FRAMES; i++) {
        frame_header->ref_order_hint[i] = AV1_READ_BITS_CHECKED (br,
            seq_header->order_hint_bits_minus_1 + 1, &retval);
        if (retval != GST_AV1_PARSER_OK)
          goto error;

        if (frame_header->ref_order_hint[i] !=
            ref_info->entry[i].ref_order_hint)
          ref_info->entry[i].ref_valid = 0;
      }
    }
  }

  if (frame_header->frame_is_intra) {
    retval = gst_av1_parse_frame_size (parser, br, frame_header);
    if (retval != GST_AV1_PARSER_OK)
      goto error;
    retval = gst_av1_parse_render_size (parser, br, frame_header);
    if (retval != GST_AV1_PARSER_OK)
      goto error;
    if (frame_header->allow_screen_content_tools
        && parser->state.upscaled_width == parser->state.frame_width) {
      frame_header->allow_intrabc = AV1_READ_BIT_CHECKED (br, &retval);
      if (retval != GST_AV1_PARSER_OK)
        goto error;
    }

    frame_header->upscaled_width = parser->state.upscaled_width;
    frame_header->frame_width = parser->state.frame_width;
    frame_header->frame_height = parser->state.frame_height;
    frame_header->render_width = parser->state.render_width;
    frame_header->render_height = parser->state.render_height;
  } else {
    if (!seq_header->enable_order_hint) {
      frame_header->frame_refs_short_signaling = 0;
    } else {
      frame_header->frame_refs_short_signaling =
          AV1_READ_BIT_CHECKED (br, &retval);
      if (retval != GST_AV1_PARSER_OK)
        goto error;

      if (frame_header->frame_refs_short_signaling) {
        if (AV1_REMAINING_BITS (br) < 3 + 3) {
          retval = GST_AV1_PARSER_NO_MORE_DATA;
          goto error;
        }
        frame_header->last_frame_idx = AV1_READ_BITS (br, 3);
        frame_header->gold_frame_idx = AV1_READ_BITS (br, 3);
        gst_av1_set_frame_refs (parser, seq_header, frame_header);
      }
    }

    for (i = 0; i < GST_AV1_REFS_PER_FRAME; i++) {
      if (!frame_header->frame_refs_short_signaling) {
        frame_header->ref_frame_idx[i] = AV1_READ_BITS_CHECKED (br, 3, &retval);
        if (retval != GST_AV1_PARSER_OK)
          goto error;
      }

      if (seq_header->frame_id_numbers_present_flag) {
        gint32 delta_frame_id /* DeltaFrameId */ ;
        gint32 expected_frame_id;
        guint32 delta_frame_id_minus_1;

        g_assert (id_len > 0);

        delta_frame_id_minus_1 = AV1_READ_BITS_CHECKED (br,
            seq_header->delta_frame_id_length_minus_2 + 2, &retval);
        if (retval != GST_AV1_PARSER_OK)
          goto error;

        delta_frame_id = delta_frame_id_minus_1 + 1;
        expected_frame_id = (frame_header->current_frame_id + (1 << id_len) -
            delta_frame_id) % (1 << id_len);
        if (expected_frame_id !=
            parser->state.ref_info.entry[frame_header->
                ref_frame_idx[i]].ref_frame_id) {
          GST_DEBUG ("Reference buffer frame ID mismatch, expectedFrameId"
              " is %d while ref frame id is %d", expected_frame_id,
              parser->state.ref_info.entry[frame_header->
                  ref_frame_idx[i]].ref_frame_id);
        }
      }
    }

    if (frame_header->frame_size_override_flag
        && !frame_header->error_resilient_mode) {
      retval = gst_av1_parse_frame_size_with_refs (parser, br, frame_header);
      if (retval != GST_AV1_PARSER_OK)
        goto error;
    } else {
      retval = gst_av1_parse_frame_size (parser, br, frame_header);
      if (retval != GST_AV1_PARSER_OK)
        goto error;
      retval = gst_av1_parse_render_size (parser, br, frame_header);
      if (retval != GST_AV1_PARSER_OK)
        goto error;
    }
    frame_header->upscaled_width = parser->state.upscaled_width;
    frame_header->frame_width = parser->state.frame_width;
    frame_header->frame_height = parser->state.frame_height;
    frame_header->render_width = parser->state.render_width;
    frame_header->render_height = parser->state.render_height;

    if (frame_header->force_integer_mv) {
      frame_header->allow_high_precision_mv = 0;
    } else {
      frame_header->allow_high_precision_mv =
          AV1_READ_BIT_CHECKED (br, &retval);
      if (retval != GST_AV1_PARSER_OK)
        goto error;
    }

    /* read_interpolation_filter() expand */
    frame_header->is_filter_switchable = AV1_READ_BIT_CHECKED (br, &retval);
    if (retval != GST_AV1_PARSER_OK)
      goto error;

    if (frame_header->is_filter_switchable) {
      frame_header->interpolation_filter =
          GST_AV1_INTERPOLATION_FILTER_SWITCHABLE;
    } else {
      frame_header->interpolation_filter =
          AV1_READ_BITS_CHECKED (br, 2, &retval);
      if (retval != GST_AV1_PARSER_OK)
        goto error;
    }

    frame_header->is_motion_mode_switchable =
        AV1_READ_BIT_CHECKED (br, &retval);
    if (retval != GST_AV1_PARSER_OK)
      goto error;

    if (frame_header->error_resilient_mode || !seq_header->enable_ref_frame_mvs) {
      frame_header->use_ref_frame_mvs = 0;
    } else {
      frame_header->use_ref_frame_mvs = AV1_READ_BIT_CHECKED (br, &retval);
      if (retval != GST_AV1_PARSER_OK)
        goto error;
    }
  }

  if (!frame_header->frame_is_intra) {
    for (i = 0; i < GST_AV1_REFS_PER_FRAME; i++) {
      gint refFrame = GST_AV1_REF_LAST_FRAME + i;
      gint hint =
          ref_info->entry[frame_header->ref_frame_idx[i]].ref_order_hint;
      frame_header->order_hints[refFrame] = hint;
      if (!seq_header->enable_order_hint) {
        frame_header->ref_frame_sign_bias[refFrame] = 0;
      } else {
        frame_header->ref_frame_sign_bias[refFrame] =
            (gst_av1_get_relative_dist (parser->seq_header, hint,
                frame_header->order_hint) > 0);
      }
    }
  }

  if (seq_header->reduced_still_picture_header
      || frame_header->disable_cdf_update)
    frame_header->disable_frame_end_update_cdf = 1;
  else {
    frame_header->disable_frame_end_update_cdf =
        AV1_READ_BIT_CHECKED (br, &retval);
    if (retval != GST_AV1_PARSER_OK)
      goto error;
  }

  if (frame_header->primary_ref_frame != GST_AV1_PRIMARY_REF_NONE &&
      !ref_info->entry[frame_header->ref_frame_idx[frame_header->
              primary_ref_frame]].ref_valid) {
    GST_INFO ("Primary ref point to an invalid frame");
    retval = GST_AV1_PARSER_BITSTREAM_ERROR;
    goto error;
  }

  if (frame_header->primary_ref_frame == GST_AV1_PRIMARY_REF_NONE) {
    /* do something in setup_past_independence() of parser level */
    gint8 *loop_filter_ref_deltas =
        frame_header->loop_filter_params.loop_filter_ref_deltas;

    frame_header->loop_filter_params.loop_filter_delta_enabled = 1;
    loop_filter_ref_deltas[GST_AV1_REF_INTRA_FRAME] = 1;
    loop_filter_ref_deltas[GST_AV1_REF_LAST_FRAME] = 0;
    loop_filter_ref_deltas[GST_AV1_REF_LAST2_FRAME] = 0;
    loop_filter_ref_deltas[GST_AV1_REF_LAST3_FRAME] = 0;
    loop_filter_ref_deltas[GST_AV1_REF_BWDREF_FRAME] = 0;
    loop_filter_ref_deltas[GST_AV1_REF_GOLDEN_FRAME] = -1;
    loop_filter_ref_deltas[GST_AV1_REF_ALTREF_FRAME] = -1;
    loop_filter_ref_deltas[GST_AV1_REF_ALTREF2_FRAME] = -1;
    frame_header->loop_filter_params.loop_filter_mode_deltas[0] = 0;
    frame_header->loop_filter_params.loop_filter_mode_deltas[1] = 0;
  } else {
    /* do something in load_previous() of parser level */
    /*   load_loop_filter_params() */
    GstAV1LoopFilterParams *ref_lf_params =
        &parser->state.ref_info.entry[frame_header->
        ref_frame_idx[frame_header->primary_ref_frame]].ref_lf_params;
    gint8 *loop_filter_ref_deltas =
        frame_header->loop_filter_params.loop_filter_ref_deltas;

    /* Copy all from prime_ref */
    g_assert (parser->state.ref_info.
        entry[frame_header->ref_frame_idx[frame_header->primary_ref_frame]].
        ref_valid);
    loop_filter_ref_deltas[GST_AV1_REF_INTRA_FRAME] =
        ref_lf_params->loop_filter_ref_deltas[GST_AV1_REF_INTRA_FRAME];
    loop_filter_ref_deltas[GST_AV1_REF_LAST_FRAME] =
        ref_lf_params->loop_filter_ref_deltas[GST_AV1_REF_LAST_FRAME];
    loop_filter_ref_deltas[GST_AV1_REF_LAST2_FRAME] =
        ref_lf_params->loop_filter_ref_deltas[GST_AV1_REF_LAST2_FRAME];
    loop_filter_ref_deltas[GST_AV1_REF_LAST3_FRAME] =
        ref_lf_params->loop_filter_ref_deltas[GST_AV1_REF_LAST3_FRAME];
    loop_filter_ref_deltas[GST_AV1_REF_BWDREF_FRAME] =
        ref_lf_params->loop_filter_ref_deltas[GST_AV1_REF_BWDREF_FRAME];
    loop_filter_ref_deltas[GST_AV1_REF_GOLDEN_FRAME] =
        ref_lf_params->loop_filter_ref_deltas[GST_AV1_REF_GOLDEN_FRAME];
    loop_filter_ref_deltas[GST_AV1_REF_ALTREF2_FRAME] =
        ref_lf_params->loop_filter_ref_deltas[GST_AV1_REF_ALTREF2_FRAME];
    loop_filter_ref_deltas[GST_AV1_REF_ALTREF_FRAME] =
        ref_lf_params->loop_filter_ref_deltas[GST_AV1_REF_ALTREF_FRAME];
    for (i = 0; i < 2; i++)
      frame_header->loop_filter_params.loop_filter_mode_deltas[i] =
          ref_lf_params->loop_filter_mode_deltas[i];
  }

  /* @TODO:
     if ( primary_ref_frame == PRIMARY_REF_NONE ) {
     init_non_coeff_cdfs( )
     } else {
     load_cdfs( ref_frame_idx[primary_ref_frame] )
     }
   */
  /* @TODO:
     if ( use_ref_frame_mvs == 1 )
     motion_field_estimation( )
   */

  retval = gst_av1_parse_tile_info (parser, br, frame_header);
  if (retval != GST_AV1_PARSER_OK)
    goto error;

  retval = gst_av1_parse_quantization_params (parser, br, frame_header);
  if (retval != GST_AV1_PARSER_OK)
    goto error;

  retval = gst_av1_parse_segmentation_params (parser, br, frame_header);
  if (retval != GST_AV1_PARSER_OK)
    goto error;

  retval = gst_av1_parse_delta_q_params (parser, br,
      &(frame_header->quantization_params));
  if (retval != GST_AV1_PARSER_OK)
    goto error;

  retval = gst_av1_parse_delta_lf_params (parser, br, frame_header);
  if (retval != GST_AV1_PARSER_OK)
    goto error;

  /* @TODO:
     if ( primary_ref_frame == PRIMARY_REF_NONE ) {
     init_coeff_cdfs( )
     } else {
     load_previous_segment_ids( )
     }
   */

  frame_header->coded_lossless = 1;
  for (segment_id = 0; segment_id < GST_AV1_MAX_SEGMENTS; segment_id++) {
    gint qindex = gst_av1_get_qindex (parser, frame_header, 1, segment_id);
    frame_header->lossless_array[segment_id] = (qindex == 0)
        && (frame_header->quantization_params.delta_q_y_dc == 0)
        && (frame_header->quantization_params.delta_q_u_ac == 0)
        && (frame_header->quantization_params.delta_q_u_dc == 0)
        && (frame_header->quantization_params.delta_q_v_ac == 0)
        && (frame_header->quantization_params.delta_q_v_dc == 0);
    if (!frame_header->lossless_array[segment_id])
      frame_header->coded_lossless = 0;
    if (frame_header->quantization_params.using_qmatrix) {
      if (frame_header->lossless_array[segment_id]) {
        frame_header->seg_qm_Level[0][segment_id] = 15;
        frame_header->seg_qm_Level[1][segment_id] = 15;
        frame_header->seg_qm_Level[2][segment_id] = 15;
      } else {
        frame_header->seg_qm_Level[0][segment_id] =
            frame_header->quantization_params.qm_y;
        frame_header->seg_qm_Level[1][segment_id] =
            frame_header->quantization_params.qm_u;
        frame_header->seg_qm_Level[2][segment_id] =
            frame_header->quantization_params.qm_v;
      }
    }
  }
  frame_header->all_lossless = frame_header->coded_lossless
      && (parser->state.frame_width == parser->state.upscaled_width);

  retval = gst_av1_parse_loop_filter_params (parser, br, frame_header);
  if (retval != GST_AV1_PARSER_OK)
    goto error;

  retval = gst_av1_parse_cdef_params (parser, br, frame_header);
  if (retval != GST_AV1_PARSER_OK)
    goto error;

  retval = gst_av1_parse_loop_restoration_params (parser, br, frame_header);
  if (retval != GST_AV1_PARSER_OK)
    goto error;

  retval = gst_av1_parse_tx_mode (parser, br, frame_header);
  if (retval != GST_AV1_PARSER_OK)
    goto error;

  /* 5.9.23 inlined frame_reference_mode () */
  if (frame_header->frame_is_intra) {
    frame_header->reference_select = 0;
  } else {
    frame_header->reference_select = AV1_READ_BIT_CHECKED (br, &retval);
    if (retval != GST_AV1_PARSER_OK)
      goto error;
  }

  retval = gst_av1_parse_skip_mode_params (parser, br, frame_header);
  if (retval != GST_AV1_PARSER_OK)
    goto error;

  if (frame_header->frame_is_intra ||
      frame_header->error_resilient_mode || !seq_header->enable_warped_motion)
    frame_header->allow_warped_motion = 0;
  else {
    frame_header->allow_warped_motion = AV1_READ_BIT_CHECKED (br, &retval);
    if (retval != GST_AV1_PARSER_OK)
      goto error;
  }

  frame_header->reduced_tx_set = AV1_READ_BIT_CHECKED (br, &retval);
  if (retval != GST_AV1_PARSER_OK)
    goto error;

  retval = gst_av1_parse_global_motion_params (parser, br, frame_header);
  if (retval != GST_AV1_PARSER_OK)
    goto error;

  retval = gst_av1_parse_film_grain_params (parser, br, frame_header);
  if (retval != GST_AV1_PARSER_OK)
    goto error;

success:
  return GST_AV1_PARSER_OK;

error:
  GST_WARNING ("parse uncompressed frame header error %d", retval);
  return retval;
}

/**
 * gst_av1_parser_reference_frame_update:
 * @parser: the #GstAV1Parser
 * @frame_header: a #GstAV1FrameHeaderOBU to load
 *
 * Update the context of @frame_header to parser's state. This function is
 * used when we finish one frame's decoding/showing, and need to update info
 * such as reference, global parameters.
 *
 * Returns: The #GstAV1ParserResult.
 *
 * Since: 1.18
 */
GstAV1ParserResult
gst_av1_parser_reference_frame_update (GstAV1Parser * parser,
    GstAV1FrameHeaderOBU * frame_header)
{
  gint i;
  GstAV1SequenceHeaderOBU *seq_header;
  GstAV1ReferenceFrameInfo *ref_info;

  g_return_val_if_fail (parser != NULL, GST_AV1_PARSER_INVALID_OPERATION);
  g_return_val_if_fail (frame_header != NULL, GST_AV1_PARSER_INVALID_OPERATION);

  if (!parser->seq_header) {
    GST_WARNING ("Missing OBU Reference: seq_header");
    return GST_AV1_PARSER_MISSING_OBU_REFERENCE;
  }

  seq_header = parser->seq_header;
  ref_info = &(parser->state.ref_info);
  if (frame_header->frame_type == GST_AV1_INTRA_ONLY_FRAME
      && frame_header->refresh_frame_flags == 0xff)
    return GST_AV1_PARSER_BITSTREAM_ERROR;

  for (i = 0; i < GST_AV1_NUM_REF_FRAMES; i++) {
    if ((frame_header->refresh_frame_flags >> i) & 1) {
      ref_info->entry[i].ref_valid = 1;
      ref_info->entry[i].ref_frame_id = frame_header->current_frame_id;
      ref_info->entry[i].ref_frame_type = frame_header->frame_type;
      ref_info->entry[i].ref_upscaled_width = frame_header->upscaled_width;
      ref_info->entry[i].ref_frame_width = frame_header->frame_width;
      ref_info->entry[i].ref_frame_height = frame_header->frame_height;
      ref_info->entry[i].ref_render_width = frame_header->render_width;
      ref_info->entry[i].ref_render_height = frame_header->render_height;
      ref_info->entry[i].ref_order_hint = frame_header->order_hint;
      ref_info->entry[i].ref_mi_cols = parser->state.mi_cols;
      ref_info->entry[i].ref_mi_rows = parser->state.mi_rows;
      ref_info->entry[i].ref_subsampling_x =
          seq_header->color_config.subsampling_x;
      ref_info->entry[i].ref_subsampling_y =
          seq_header->color_config.subsampling_y;
      ref_info->entry[i].ref_bit_depth = seq_header->bit_depth;
      ref_info->entry[i].ref_segmentation_params =
          frame_header->segmentation_params;
      ref_info->entry[i].ref_global_motion_params =
          frame_header->global_motion_params;
      ref_info->entry[i].ref_lf_params = frame_header->loop_filter_params;
      ref_info->entry[i].ref_tile_info = frame_header->tile_info;
      if (seq_header->film_grain_params_present)
        ref_info->entry[i].ref_film_grain_params =
            frame_header->film_grain_params;
    }
  }

  return GST_AV1_PARSER_OK;
}

/* 5.12.1 */
/**
 * gst_av1_parser_parse_tile_list_obu:
 * @parser: the #GstAV1Parser
 * @obu: a #GstAV1OBU to be parsed
 * @tile_list: a #GstAV1TileListOBU to store the parsed result.
 *
 * Parse one tile list @obu based on the @parser context, store the result
 * in the @tile_list. It is for large scale tile coding mode.
 *
 * Returns: The #GstAV1ParserResult.
 *
 * Since: 1.18
 */
GstAV1ParserResult
gst_av1_parser_parse_tile_list_obu (GstAV1Parser * parser,
    GstAV1OBU * obu, GstAV1TileListOBU * tile_list)
{
  GstAV1ParserResult retval = GST_AV1_PARSER_OK;
  GstBitReader *br;
  GstBitReader bitreader;
  gint tile;

  g_return_val_if_fail (parser != NULL, GST_AV1_PARSER_INVALID_OPERATION);
  g_return_val_if_fail (obu != NULL, GST_AV1_PARSER_INVALID_OPERATION);
  g_return_val_if_fail (obu->obu_type == GST_AV1_OBU_TILE_LIST,
      GST_AV1_PARSER_INVALID_OPERATION);
  g_return_val_if_fail (tile_list != NULL, GST_AV1_PARSER_INVALID_OPERATION);

  br = &bitreader;
  memset (tile_list, 0, sizeof (*tile_list));
  gst_bit_reader_init (br, obu->data, obu->obu_size);
  if (AV1_REMAINING_BITS (br) < 8 + 8 + 16) {
    retval = GST_AV1_PARSER_NO_MORE_DATA;
    goto error;
  }

  tile_list->output_frame_width_in_tiles_minus_1 = AV1_READ_BITS (br, 8);
  tile_list->output_frame_height_in_tiles_minus_1 = AV1_READ_BITS (br, 8);
  tile_list->tile_count_minus_1 = AV1_READ_BITS (br, 16);
  if (tile_list->tile_count_minus_1 + 1 > GST_AV1_MAX_TILE_COUNT) {
    GST_WARNING ("Invalid tile_count_minus_1 %d",
        tile_list->tile_count_minus_1);
    retval = GST_AV1_PARSER_BITSTREAM_ERROR;
    goto error;
  }

  for (tile = 0; tile <= tile_list->tile_count_minus_1; tile++) {
    if (AV1_REMAINING_BITS (br) < 8 + 8 + 8 + 16) {
      retval = GST_AV1_PARSER_NO_MORE_DATA;
      goto error;
    }
    tile_list->entry[tile].anchor_frame_idx = AV1_READ_BITS (br, 8);
    tile_list->entry[tile].anchor_tile_row = AV1_READ_BITS (br, 8);
    tile_list->entry[tile].anchor_tile_col = AV1_READ_BITS (br, 8);
    tile_list->entry[tile].tile_data_size_minus_1 = AV1_READ_BITS (br, 16);

    g_assert (gst_bit_reader_get_pos (br) % 8 == 0);

    tile_list->entry[tile].coded_tile_data =
        obu->data + gst_bit_reader_get_pos (br) / 8;
    /* skip the coded_tile_data */
    if (!gst_bit_reader_skip (br,
            tile_list->entry[tile].tile_data_size_minus_1 + 1)) {
      retval = GST_AV1_PARSER_NO_MORE_DATA;
      goto error;
    }
  }

  retval = av1_skip_trailing_bits (parser, br, obu);
  if (retval != GST_AV1_PARSER_OK)
    goto error;

  return GST_AV1_PARSER_OK;

error:
  GST_WARNING ("parse tile list error %d", retval);
  return retval;
}

/* 5.11.1 */
static GstAV1ParserResult
gst_av1_parse_tile_group (GstAV1Parser * parser, GstBitReader * br,
    GstAV1TileGroupOBU * tile_group)
{
  GstAV1ParserResult retval = GST_AV1_PARSER_OK;
  gint tile_num /* TileNum */ , end_bit_pos /* endBitPos */ ;
  gint header_bytes /* headerBytes */ , start_bitpos /* startBitPos */ ;
  guint32 sz = AV1_REMAINING_BYTES (br);
  guint32 tile_row /* tileRow */ ;
  guint32 tile_col /* tileCol */ ;
  guint32 tile_size /* tileSize */ ;

  memset (tile_group, 0, sizeof (*tile_group));
  tile_group->num_tiles = parser->state.tile_cols * parser->state.tile_rows;
  start_bitpos = gst_bit_reader_get_pos (br);
  tile_group->tile_start_and_end_present_flag = 0;

  if (tile_group->num_tiles > 1) {
    tile_group->tile_start_and_end_present_flag =
        AV1_READ_BIT_CHECKED (br, &retval);
    if (retval != GST_AV1_PARSER_OK)
      goto error;
  }
  if (tile_group->num_tiles == 1
      || !tile_group->tile_start_and_end_present_flag) {
    tile_group->tg_start = 0;
    tile_group->tg_end = tile_group->num_tiles - 1;
  } else {
    gint tileBits = parser->state.tile_cols_log2 + parser->state.tile_rows_log2;
    tile_group->tg_start = AV1_READ_BITS_CHECKED (br, tileBits, &retval);
    if (retval != GST_AV1_PARSER_OK)
      goto error;

    tile_group->tg_end = AV1_READ_BITS_CHECKED (br, tileBits, &retval);
    if (retval != GST_AV1_PARSER_OK)
      goto error;
  }

  if (tile_group->tg_end < tile_group->tg_start) {
    retval = GST_AV1_PARSER_NO_MORE_DATA;
    goto error;
  }

  if (!gst_bit_reader_skip_to_byte (br)) {
    retval = GST_AV1_PARSER_NO_MORE_DATA;
    goto error;
  }

  end_bit_pos = gst_bit_reader_get_pos (br);
  header_bytes = (end_bit_pos - start_bitpos) / 8;
  sz -= header_bytes;

  for (tile_num = tile_group->tg_start; tile_num <= tile_group->tg_end;
      tile_num++) {
    tile_row = tile_num / parser->state.tile_cols;
    tile_col = tile_num % parser->state.tile_cols;
    /* if last tile */
    if (tile_num == tile_group->tg_end) {
      tile_size = sz;
    } else {
      gint tile_size_minus_1 = av1_bitstreamfn_le (br,
          parser->state.tile_size_bytes, &retval);
      if (retval != GST_AV1_PARSER_OK)
        goto error;
      tile_size = tile_size_minus_1 + 1;
      sz -= (tile_size + parser->state.tile_size_bytes);
    }

    tile_group->entry[tile_num].tile_size = tile_size;
    tile_group->entry[tile_num].tile_offset = gst_bit_reader_get_pos (br) / 8;
    tile_group->entry[tile_num].tile_row = tile_row;
    tile_group->entry[tile_num].tile_col = tile_col;

    tile_group->entry[tile_num].mi_row_start =
        parser->state.mi_row_starts[tile_row];
    tile_group->entry[tile_num].mi_row_end =
        parser->state.mi_row_starts[tile_row + 1];
    tile_group->entry[tile_num].mi_col_start =
        parser->state.mi_col_starts[tile_col];
    tile_group->entry[tile_num].mi_col_end =
        parser->state.mi_col_starts[tile_col + 1];
    /* Not implement here, the real decoder process
       init_symbol( tileSize )
       decode_tile( )
       exit_symbol( )
     */

    /* Skip the real data to the next one */
    if (tile_num < tile_group->tg_end &&
        !gst_bit_reader_skip (br, tile_size * 8)) {
      retval = GST_AV1_PARSER_NO_MORE_DATA;
      goto error;
    }
  }

  if (tile_group->tg_end == tile_group->num_tiles - 1) {
    /* Not implement here, the real decoder process
       if ( !disable_frame_end_update_cdf ) {
       frame_end_update_cdf( )
       }
       decode_frame_wrapup( )
     */
    parser->state.seen_frame_header = 0;
  }

  return GST_AV1_PARSER_OK;

error:
  GST_WARNING ("parse tile group error %d", retval);
  return retval;
}

/**
 * gst_av1_parser_parse_tile_group_obu:
 * @parser: the #GstAV1Parser
 * @obu: a #GstAV1OBU to be parsed
 * @tile_group: a #GstAV1TileGroupOBU to store the parsed result.
 *
 * Parse one tile group @obu based on the @parser context, store the result
 * in the @tile_group.
 *
 * Returns: The #GstAV1ParserResult.
 *
 * Since: 1.18
 */
GstAV1ParserResult
gst_av1_parser_parse_tile_group_obu (GstAV1Parser * parser, GstAV1OBU * obu,
    GstAV1TileGroupOBU * tile_group)
{
  GstAV1ParserResult ret;
  GstBitReader bit_reader;

  g_return_val_if_fail (parser != NULL, GST_AV1_PARSER_INVALID_OPERATION);
  g_return_val_if_fail (obu != NULL, GST_AV1_PARSER_INVALID_OPERATION);
  g_return_val_if_fail (obu->obu_type == GST_AV1_OBU_TILE_GROUP,
      GST_AV1_PARSER_INVALID_OPERATION);
  g_return_val_if_fail (tile_group != NULL, GST_AV1_PARSER_INVALID_OPERATION);

  if (!parser->state.seen_frame_header) {
    GST_WARNING ("Missing OBU Reference: frame_header");
    return GST_AV1_PARSER_MISSING_OBU_REFERENCE;
  }

  gst_bit_reader_init (&bit_reader, obu->data, obu->obu_size);
  ret = gst_av1_parse_tile_group (parser, &bit_reader, tile_group);
  return ret;
}

static GstAV1ParserResult
gst_av1_parse_frame_header (GstAV1Parser * parser, GstAV1OBU * obu,
    GstBitReader * bit_reader, GstAV1FrameHeaderOBU * frame_header)
{
  GstAV1ParserResult ret;
  guint i;

  memset (frame_header, 0, sizeof (*frame_header));
  frame_header->frame_is_intra = 1;
  frame_header->last_frame_idx = -1;
  frame_header->gold_frame_idx = -1;
  for (i = 0; i < GST_AV1_REFS_PER_FRAME; i++)
    frame_header->ref_frame_idx[i] = -1;

  ret = gst_av1_parse_uncompressed_frame_header (parser, obu, bit_reader,
      frame_header);
  if (ret != GST_AV1_PARSER_OK)
    return ret;

  if (frame_header->show_existing_frame) {
    parser->state.seen_frame_header = 0;
  } else {
    parser->state.seen_frame_header = 1;
  }

  return GST_AV1_PARSER_OK;
}

/**
 * gst_av1_parser_parse_frame_header_obu:
 * @parser: the #GstAV1Parser
 * @obu: a #GstAV1OBU to be parsed
 * @frame_header: a #GstAV1FrameHeaderOBU to store the parsed result.
 *
 * Parse one frame header @obu based on the @parser context, store the result
 * in the @frame.
 *
 * Returns: The #GstAV1ParserResult.
 *
 * Since: 1.18
 */
GstAV1ParserResult
gst_av1_parser_parse_frame_header_obu (GstAV1Parser * parser,
    GstAV1OBU * obu, GstAV1FrameHeaderOBU * frame_header)
{
  GstAV1ParserResult ret;
  GstBitReader bit_reader;

  g_return_val_if_fail (parser != NULL, GST_AV1_PARSER_INVALID_OPERATION);
  g_return_val_if_fail (obu != NULL, GST_AV1_PARSER_INVALID_OPERATION);
  g_return_val_if_fail (obu->obu_type == GST_AV1_OBU_FRAME_HEADER ||
      obu->obu_type == GST_AV1_OBU_REDUNDANT_FRAME_HEADER,
      GST_AV1_PARSER_INVALID_OPERATION);
  g_return_val_if_fail (frame_header != NULL, GST_AV1_PARSER_INVALID_OPERATION);

  if (obu->obu_type == GST_AV1_OBU_REDUNDANT_FRAME_HEADER
      && !parser->state.seen_frame_header) {
    GST_WARNING ("no seen of frame header while get redundant frame header");
    return GST_AV1_PARSER_BITSTREAM_ERROR;
  }

  if (obu->obu_type == GST_AV1_OBU_FRAME_HEADER
      && parser->state.seen_frame_header) {
    GST_WARNING ("already seen a frame header");
    return GST_AV1_PARSER_BITSTREAM_ERROR;
  }

  gst_bit_reader_init (&bit_reader, obu->data, obu->obu_size);
  ret = gst_av1_parse_frame_header (parser, obu, &bit_reader, frame_header);
  if (ret != GST_AV1_PARSER_OK)
    return ret;

  ret = av1_skip_trailing_bits (parser, &bit_reader, obu);
  return ret;
}

/**
 * gst_av1_parser_parse_frame_obu:
 * @parser: the #GstAV1Parser
 * @obu: a #GstAV1OBU to be parsed
 * @frame: a #GstAV1FrameOBU to store the parsed result.
 *
 * Parse one frame @obu based on the @parser context, store the result
 * in the @frame.
 *
 * Returns: The #GstAV1ParserResult.
 *
 * Since: 1.18
 */
GstAV1ParserResult
gst_av1_parser_parse_frame_obu (GstAV1Parser * parser, GstAV1OBU * obu,
    GstAV1FrameOBU * frame)
{
  GstAV1ParserResult retval;
  GstBitReader bit_reader;

  g_return_val_if_fail (parser != NULL, GST_AV1_PARSER_INVALID_OPERATION);
  g_return_val_if_fail (obu != NULL, GST_AV1_PARSER_INVALID_OPERATION);
  g_return_val_if_fail (obu->obu_type == GST_AV1_OBU_FRAME,
      GST_AV1_PARSER_INVALID_OPERATION);
  g_return_val_if_fail (frame != NULL, GST_AV1_PARSER_INVALID_OPERATION);

  if (parser->state.seen_frame_header) {
    GST_WARNING ("already seen a frame header");
    return GST_AV1_PARSER_BITSTREAM_ERROR;
  }

  gst_bit_reader_init (&bit_reader, obu->data, obu->obu_size);
  retval = gst_av1_parse_frame_header (parser, obu, &bit_reader,
      &(frame->frame_header));
  if (retval != GST_AV1_PARSER_OK)
    return retval;

  if (!gst_bit_reader_skip_to_byte (&bit_reader))
    return GST_AV1_PARSER_NO_MORE_DATA;

  retval = gst_av1_parse_tile_group (parser, &bit_reader, &(frame->tile_group));
  return retval;
}

/**
 * gst_av1_parser_set_operating_point:
 * @parser: the #GstAV1Parser
 * @operating_point: the operating point to set
 *
 * Set the operating point to filter OBUs.
 *
 * Returns: The #GstAV1ParserResult.
 *
 * Since: 1.20
 */
GstAV1ParserResult
gst_av1_parser_set_operating_point (GstAV1Parser * parser,
    gint32 operating_point)
{
  g_return_val_if_fail (parser != NULL, GST_AV1_PARSER_INVALID_OPERATION);
  g_return_val_if_fail (operating_point >= 0, GST_AV1_PARSER_INVALID_OPERATION);

  if (parser->seq_header &&
      operating_point > parser->seq_header->operating_points_cnt_minus_1)
    return GST_AV1_PARSER_INVALID_OPERATION;

  /* Decide whether it is valid when sequence comes. */
  parser->state.operating_point = operating_point;
  return GST_AV1_PARSER_OK;
}

/**
 * gst_av1_parser_new:
 *
 * Allocates a new #GstAV1Parser,
 *
 * Returns: (transfer full): a newly-allocated  #GstAV1Parser
 *
 * Since: 1.18
 */
GstAV1Parser *
gst_av1_parser_new (void)
{
  return g_new0 (GstAV1Parser, 1);
}

/**
 * gst_av1_parser_free:
 * @parser: the #GstAV1Parser to free
 *
 * If parser is not %NULL, frees its allocated memory.
 *
 * It cannot be used afterwards.
 *
 * Since: 1.18
 */
void
gst_av1_parser_free (GstAV1Parser * parser)
{
  g_return_if_fail (parser != NULL);

  if (parser->seq_header)
    g_free (parser->seq_header);
  g_free (parser);
}
