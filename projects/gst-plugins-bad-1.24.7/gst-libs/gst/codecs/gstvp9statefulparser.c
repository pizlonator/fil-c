/* GStreamer
 * Copyright (C) 2021 Seungha Yang <seungha@centricular.com>
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

/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *
 *   * Neither the name of Google, nor the WebM Project, nor the names
 *     of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written
 *     permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. *
 */

/**
 * SECTION:gstvp9statefulparser
 * @title: GstVp9StatefulParser
 * @short_description: Convenience library for parsing vp9 video bitstream.
 *
 * This object is used to parse VP9 bitstream header.
 *
 * Since: 1.20
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/base/gstbitreader.h>
#include "gstvp9statefulparser.h"
#include <string.h>

#ifndef GST_DISABLE_GST_DEBUG
#define GST_CAT_DEFAULT ensure_debug_category()
static GstDebugCategory *
ensure_debug_category (void)
{
  static gsize cat_gonce = 0;

  if (g_once_init_enter (&cat_gonce)) {
    gsize cat_done;

    cat_done = (gsize) _gst_debug_category_new ("codecparsers_vp9stateful", 0,
        "VP9 parser library");

    g_once_init_leave (&cat_gonce, cat_done);
  }

  return (GstDebugCategory *) cat_gonce;
}
#else
#define ensure_debug_category()
#endif /* GST_DISABLE_GST_DEBUG */

#define VP9_READ_UINT8(val,nbits) G_STMT_START { \
  if (!gst_bit_reader_get_bits_uint8 (br, &val, nbits)) { \
    GST_ERROR ("failed to read uint8 for '" G_STRINGIFY (val) "', nbits: %d", nbits); \
    return GST_VP9_PARSER_BROKEN_DATA; \
  } \
} G_STMT_END

#define VP9_READ_UINT16(val,nbits) G_STMT_START { \
  if (!gst_bit_reader_get_bits_uint16 (br, &val, nbits)) { \
    GST_ERROR ("failed to read uint16 for '" G_STRINGIFY (val) "', nbits: %d", nbits); \
    return GST_VP9_PARSER_BROKEN_DATA; \
  } \
} G_STMT_END

#define VP9_READ_UINT32(val,nbits) G_STMT_START { \
  if (!gst_bit_reader_get_bits_uint32 (br, &val, nbits)) { \
    GST_ERROR ("failed to read uint32 for '" G_STRINGIFY (val) "', nbits: %d", nbits); \
    return GST_VP9_PARSER_BROKEN_DATA; \
  } \
} G_STMT_END

#define VP9_READ_BIT(val) VP9_READ_UINT8(val, 1)

#define VP9_READ_SIGNED_8(val,nbits) G_STMT_START { \
  guint8 _value; \
  guint8 _negative; \
  VP9_READ_UINT8(_value, nbits); \
  VP9_READ_BIT(_negative); \
  if (_negative) { \
    val = (gint8) _value * -1; \
  } else { \
    val = _value; \
  } \
} G_STMT_END

#define VP9_READ_SIGNED_16(val,nbits) G_STMT_START { \
  guint16 _value; \
  guint8 _negative; \
  VP9_READ_UINT16(_value, nbits); \
  VP9_READ_BIT(_negative); \
  if (_negative) { \
    val = (gint16) _value * -1; \
  } else { \
    val = _value; \
  } \
} G_STMT_END

#define CHECK_ALLOWED_WITH_DEBUG(dbg, val, min, max) { \
  if (val < min || val > max) { \
    GST_WARNING ("value for '" dbg "' not in allowed range. value: %d, range %d-%d", \
                     val, min, max); \
    return GST_VP9_PARSER_ERROR; \
  } \
}

#define CHECK_ALLOWED(val, min, max) \
  CHECK_ALLOWED_WITH_DEBUG (G_STRINGIFY (val), val, min, max)

typedef struct _Vp9BoolDecoder
{
  guint64 value;
  guint32 range;
  guint32 bits_left;
  gint count_to_fill;
  GstBitReader *bit_reader;
  gboolean out_of_bits;
} Vp9BoolDecoder;

/* how much to shift to get range > 128 */
const static guint8 bool_shift_table[256] = {
  0, 7, 6, 6, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4, 3, 3, 3, 3, 3, 3, 3, 3,
  3, 3, 3, 3, 3, 3, 3, 3, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

static const guint8 inv_map_table[255] = {
  7, 20, 33, 46, 59, 72, 85, 98, 111, 124, 137, 150, 163, 176,
  189, 202, 215, 228, 241, 254, 1, 2, 3, 4, 5, 6, 8, 9,
  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 21, 22, 23, 24,
  25, 26, 27, 28, 29, 30, 31, 32, 34, 35, 36, 37, 38, 39,
  40, 41, 42, 43, 44, 45, 47, 48, 49, 50, 51, 52, 53, 54,
  55, 56, 57, 58, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69,
  70, 71, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84,
  86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 99, 100,
  101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 112, 113, 114, 115,
  116, 117, 118, 119, 120, 121, 122, 123, 125, 126, 127, 128, 129, 130,
  131, 132, 133, 134, 135, 136, 138, 139, 140, 141, 142, 143, 144, 145,
  146, 147, 148, 149, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160,
  161, 162, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175,
  177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 190, 191,
  192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 203, 204, 205, 206,
  207, 208, 209, 210, 211, 212, 213, 214, 216, 217, 218, 219, 220, 221,
  222, 223, 224, 225, 226, 227, 229, 230, 231, 232, 233, 234, 235, 236,
  237, 238, 239, 240, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251,
  252, 253, 253,
};

static void
fill_bool (Vp9BoolDecoder * bd)
{
  guint max_bits_to_read;
  guint bits_to_read;
  guint64 data;

  if (G_UNLIKELY (bd->bits_left < bd->count_to_fill)) {
    GST_ERROR
        ("Invalid VP9 bitstream: the boolean decoder ran out of bits to read");
    bd->out_of_bits = TRUE;
    return;
  }

  max_bits_to_read =
      8 * (sizeof (bd->value) - sizeof (guint8)) + bd->count_to_fill;
  bits_to_read = MIN (max_bits_to_read, bd->bits_left);

  data =
      gst_bit_reader_get_bits_uint64_unchecked (bd->bit_reader, bits_to_read);

  bd->value |= data << (max_bits_to_read - bits_to_read);
  bd->count_to_fill -= bits_to_read;
  bd->bits_left -= bits_to_read;
}

static gboolean
read_bool (Vp9BoolDecoder * bd, guint8 probability)
{
  guint64 split;
  guint64 big_split;
  guint count;
  gboolean bit;

  if (bd->count_to_fill > 0)
    fill_bool (bd);

  split = 1 + (((bd->range - 1) * probability) >> 8);
  big_split = split << 8 * (sizeof (bd->value) - sizeof (guint8));

  if (bd->value < big_split) {
    bd->range = split;
    bit = FALSE;
  } else {
    bd->range -= split;
    bd->value -= big_split;
    bit = TRUE;
  }

  count = bool_shift_table[bd->range];
  bd->range <<= count;
  bd->value <<= count;
  bd->count_to_fill += count;

  return bit;
}

static guint
read_literal (Vp9BoolDecoder * bd, guint n)
{
  guint ret = 0;
  guint i;

  for (i = 0; G_UNLIKELY (!bd->out_of_bits) && i < n; i++) {
    ret = 2 * ret + read_bool (bd, 128);
  }

  return ret;
}

static GstVp9ParserResult
init_bool (Vp9BoolDecoder * bd, GstBitReader * br, guint size_in_bytes)
{
  gboolean marker_bit;

  if (size_in_bytes < 1)
    GST_ERROR ("VP9 Boolean Decoder has no bits to read");

  if ((gst_bit_reader_get_pos (br) % 8) != 0)
    GST_ERROR ("VP9 Boolean Decoder was passed an unaligned buffer");

  bd->value = 0;
  bd->range = 255;
  bd->bits_left = 8 * size_in_bytes;
  bd->bit_reader = br;
  bd->count_to_fill = 8;
  bd->out_of_bits = FALSE;

  marker_bit = read_literal (bd, 1);
  if (marker_bit != 0) {
    GST_ERROR ("Marker bit should be zero was %d", marker_bit);
    return GST_VP9_PARSER_BROKEN_DATA;
  }

  return GST_VP9_PARSER_OK;
}

static GstVp9ParserResult
exit_bool (Vp9BoolDecoder * bd)
{
  guint8 padding;
  guint8 bits = bd->bits_left;
  guint8 n;

  while (bits) {
    n = MIN (bits, 8);
    padding = gst_bit_reader_get_bits_uint32_unchecked (bd->bit_reader, n);
    if (padding != 0 || (n < 8 && (padding & 0xe0) == 0xc0)) {
      GST_ERROR
          ("Invalid padding at end of frame. Total padding bits is %d and the wrong byte is: %x",
          bd->bits_left, padding);
      return GST_VP9_PARSER_BROKEN_DATA;
    }
    bits -= n;
  }

  return GST_VP9_PARSER_OK;
}

static guint
decode_term_subexp (Vp9BoolDecoder * bd)
{
  guint8 bit;
  guint v;
  /* only coded if update_prob is set */
  gboolean prob_is_coded_in_bitstream;
  guint delta;

  prob_is_coded_in_bitstream = read_bool (bd, 252);
  if (!prob_is_coded_in_bitstream)
    return 0;

  bit = read_literal (bd, 1);
  if (bit == 0) {
    delta = read_literal (bd, 4);
    goto end;
  }

  bit = read_literal (bd, 1);
  if (bit == 0) {
    delta = read_literal (bd, 4) + 16;
    goto end;
  }

  bit = read_literal (bd, 1);
  if (bit == 0) {
    delta = read_literal (bd, 5) + 32;
    goto end;
  }

  v = read_literal (bd, 7);
  if (v < 65) {
    delta = v + 64;
    goto end;
  }

  bit = read_literal (bd, 1);
  delta = (v << 1) - 1 + bit;
end:
  return inv_map_table[delta];
}

static guint8
read_mv_prob (Vp9BoolDecoder * bd)
{
  gboolean update_mv_prob;
  guint8 mv_prob;
  guint8 prob = 0;

  update_mv_prob = read_bool (bd, 252);
  if (update_mv_prob) {
    mv_prob = read_literal (bd, 7);
    prob = (mv_prob << 1) | 1;
  }

  return prob;
}

static GstVp9ParserResult
parse_mv_probs (GstVp9FrameHeader * hdr, Vp9BoolDecoder * bd)
{
  guint i, j, k;

  for (j = 0; j < GST_VP9_MV_JOINTS - 1; j++)
    hdr->delta_probabilities.mv.joint[j] = read_mv_prob (bd);

  for (i = 0; i < 2; i++) {
    hdr->delta_probabilities.mv.sign[i] = read_mv_prob (bd);

    for (j = 0; j < GST_VP9_MV_CLASSES - 1; j++)
      hdr->delta_probabilities.mv.klass[i][j] = read_mv_prob (bd);

    hdr->delta_probabilities.mv.class0_bit[i] = read_mv_prob (bd);

    for (j = 0; j < GST_VP9_MV_OFFSET_BITS; j++)
      hdr->delta_probabilities.mv.bits[i][j] = read_mv_prob (bd);
  }

  for (i = 0; i < 2; i++) {
    for (j = 0; j < GST_VP9_CLASS0_SIZE; j++)
      for (k = 0; k < GST_VP9_MV_FR_SIZE - 1; k++)
        hdr->delta_probabilities.mv.class0_fr[i][j][k] = read_mv_prob (bd);

    for (k = 0; k < GST_VP9_MV_FR_SIZE - 1; k++)
      hdr->delta_probabilities.mv.fr[i][k] = read_mv_prob (bd);
  }

  if (hdr->allow_high_precision_mv) {
    for (i = 0; i < 2; i++) {
      hdr->delta_probabilities.mv.class0_hp[i] = read_mv_prob (bd);
      hdr->delta_probabilities.mv.hp[i] = read_mv_prob (bd);
    }

  }

  return GST_VP9_PARSER_OK;
}

static GstVp9ParserResult
parse_partition_probs (GstVp9FrameHeader * hdr, Vp9BoolDecoder * bd)
{
  guint i, j;

  for (i = 0; i < GST_VP9_PARTITION_CONTEXTS; i++)
    for (j = 0; j < GST_VP9_PARTITION_TYPES - 1; j++)
      hdr->delta_probabilities.partition[i][j] = decode_term_subexp (bd);

  return GST_VP9_PARSER_OK;
}

static GstVp9ParserResult
parse_y_mode_probs (GstVp9FrameHeader * hdr, Vp9BoolDecoder * bd)
{
  guint i, j;

  for (i = 0; i < GST_VP9_BLOCK_SIZE_GROUPS; i++)
    for (j = 0; j < GST_VP9_INTRA_MODES - 1; j++)
      hdr->delta_probabilities.y_mode[i][j] = decode_term_subexp (bd);

  return GST_VP9_PARSER_OK;
}

static GstVp9ParserResult
parse_frame_reference_mode_probs (GstVp9FrameHeader * hdr, Vp9BoolDecoder * bd)
{
  guint i;

  if (hdr->reference_mode == GST_VP9_REFERENCE_MODE_SELECT)
    for (i = 0; i < GST_VP9_COMP_MODE_CONTEXTS; i++)
      hdr->delta_probabilities.comp_mode[i] = decode_term_subexp (bd);

  if (hdr->reference_mode != GST_VP9_REFERENCE_MODE_COMPOUND_REFERENCE)
    for (i = 0; i < GST_VP9_REF_CONTEXTS; i++) {
      hdr->delta_probabilities.single_ref[i][0] = decode_term_subexp (bd);
      hdr->delta_probabilities.single_ref[i][1] = decode_term_subexp (bd);
    }

  if (hdr->reference_mode != GST_VP9_REFERENCE_MODE_SINGLE_REFERENCE)
    for (i = 0; i < GST_VP9_REF_CONTEXTS; i++)
      hdr->delta_probabilities.comp_ref[i] = decode_term_subexp (bd);

  return GST_VP9_PARSER_OK;
}

static GstVp9ParserResult
parse_frame_reference (GstVp9FrameHeader * hdr, Vp9BoolDecoder * bd)
{
  gboolean compound_ref_allowed = FALSE;
  guint8 non_single_reference;
  guint8 reference_select;
  guint i;

  for (i = GST_VP9_REF_FRAME_LAST; i < GST_VP9_REFS_PER_FRAME; i++)
    if (hdr->ref_frame_sign_bias[i + 1] !=
        hdr->ref_frame_sign_bias[GST_VP9_REF_FRAME_LAST])
      compound_ref_allowed = TRUE;

  if (compound_ref_allowed) {
    non_single_reference = read_literal (bd, 1);
    if (!non_single_reference)
      hdr->reference_mode = GST_VP9_REFERENCE_MODE_SINGLE_REFERENCE;
    else {
      reference_select = read_literal (bd, 1);
      if (!reference_select)
        hdr->reference_mode = GST_VP9_REFERENCE_MODE_COMPOUND_REFERENCE;
      else
        hdr->reference_mode = GST_VP9_REFERENCE_MODE_SELECT;
    }
  } else
    hdr->reference_mode = GST_VP9_REFERENCE_MODE_SINGLE_REFERENCE;

  return GST_VP9_PARSER_OK;
}

static GstVp9ParserResult
parse_is_inter_probs (GstVp9FrameHeader * hdr, Vp9BoolDecoder * bd)
{
  guint i;

  for (i = 0; i < GST_VP9_IS_INTER_CONTEXTS; i++)
    hdr->delta_probabilities.is_inter[i] = decode_term_subexp (bd);

  return GST_VP9_PARSER_OK;
}

static GstVp9ParserResult
parse_interp_filter_probs (GstVp9FrameHeader * hdr, Vp9BoolDecoder * bd)
{
  guint i, j;

  for (i = 0; i < GST_VP9_INTERP_FILTER_CONTEXTS; i++)
    for (j = 0; j < GST_VP9_SWITCHABLE_FILTERS - 1; j++)
      hdr->delta_probabilities.interp_filter[i][j] = decode_term_subexp (bd);

  return GST_VP9_PARSER_OK;
}

static GstVp9ParserResult
parse_inter_mode_probs (GstVp9FrameHeader * hdr, Vp9BoolDecoder * bd)
{
  guint i, j;

  for (i = 0; i < GST_VP9_INTER_MODE_CONTEXTS; i++)
    for (j = 0; j < GST_VP9_INTER_MODES - 1; j++)
      hdr->delta_probabilities.inter_mode[i][j] = decode_term_subexp (bd);

  return GST_VP9_PARSER_OK;
}

static GstVp9ParserResult
parse_skip_probs (GstVp9FrameHeader * hdr, Vp9BoolDecoder * bd)
{
  guint i;

  for (i = 0; i < GST_VP9_SKIP_CONTEXTS; i++)
    hdr->delta_probabilities.skip[i] = decode_term_subexp (bd);

  return GST_VP9_PARSER_OK;
}

static GstVp9ParserResult
parse_coef_probs (GstVp9FrameHeader * hdr, Vp9BoolDecoder * bd)
{
  GstVp9TxSize tx_size, max_tx_size;
  guint8 i, j, k, l, m;
  guint8 update_probs;

  static const guint8 tx_mode_to_biggest_tx_size[GST_VP9_TX_MODES] = {
    GST_VP9_TX_4x4,
    GST_VP9_TX_8x8,
    GST_VP9_TX_16x16,
    GST_VP9_TX_32x32,
    GST_VP9_TX_32x32,
  };

  max_tx_size = tx_mode_to_biggest_tx_size[hdr->tx_mode];
  for (tx_size = GST_VP9_TX_4x4; tx_size <= max_tx_size; tx_size++) {
    update_probs = read_literal (bd, 1);
    if (update_probs) {
      for (i = 0; i < 2; i++)
        for (j = 0; j < 2; j++)
          for (k = 0; k < 6; k++)
            for (l = 0; l < ((k == 0) ? 3 : 6); l++)
              for (m = 0; m < 3; m++)
                hdr->delta_probabilities.coef[tx_size][i][j][k][l][m] =
                    decode_term_subexp (bd);
    }
  }

  return GST_VP9_PARSER_OK;
}

static GstVp9ParserResult
parse_tx_mode_probs (GstVp9FrameHeader * hdr, Vp9BoolDecoder * bd)
{
  guint i, j;

  for (i = 0; i < GST_VP9_TX_SIZE_CONTEXTS; i++)
    for (j = 0; j < GST_VP9_TX_SIZES - 3; j++)
      hdr->delta_probabilities.tx_probs_8x8[i][j] = decode_term_subexp (bd);

  for (i = 0; i < GST_VP9_TX_SIZE_CONTEXTS; i++)
    for (j = 0; j < GST_VP9_TX_SIZES - 2; j++)
      hdr->delta_probabilities.tx_probs_16x16[i][j] = decode_term_subexp (bd);

  for (i = 0; i < GST_VP9_TX_SIZE_CONTEXTS; i++)
    for (j = 0; j < GST_VP9_TX_SIZES - 1; j++)
      hdr->delta_probabilities.tx_probs_32x32[i][j] = decode_term_subexp (bd);

  return GST_VP9_PARSER_OK;
}

static GstVp9ParserResult
parse_tx_mode (GstVp9FrameHeader * hdr, Vp9BoolDecoder * bd)
{
  guint8 tx_mode;
  guint8 tx_mode_select;

  if (hdr->lossless_flag) {
    hdr->tx_mode = GST_VP9_TX_MODE_ONLY_4x4;
    return GST_VP9_PARSER_OK;
  }

  tx_mode = read_literal (bd, 2);
  if (tx_mode == GST_VP9_TX_MODE_ALLOW_32x32) {
    tx_mode_select = read_literal (bd, 1);
    tx_mode += tx_mode_select;
  }

  hdr->tx_mode = tx_mode;
  return GST_VP9_PARSER_OK;
}

static GstVp9ParserResult
parse_compressed_header (GstVp9StatefulParser * self, GstVp9FrameHeader * hdr,
    GstBitReader * br)
{
  GstVp9ParserResult rst;
  gboolean frame_is_intra_only;
  Vp9BoolDecoder bd;

  /* consume trailing bits */
  while (gst_bit_reader_get_pos (br) & 0x7)
    gst_bit_reader_get_bits_uint8_unchecked (br, 1);

  rst = init_bool (&bd, br, hdr->header_size_in_bytes);
  if (rst != GST_VP9_PARSER_OK) {
    GST_ERROR ("Failed to init the boolean decoder.");
    return rst;
  }

  rst = parse_tx_mode (hdr, &bd);
  if (rst != GST_VP9_PARSER_OK)
    return rst;

  if (hdr->tx_mode == GST_VP9_TX_MODE_SELECT) {
    rst = parse_tx_mode_probs (hdr, &bd);
    if (rst != GST_VP9_PARSER_OK)
      return rst;
  }

  rst = parse_coef_probs (hdr, &bd);
  if (rst != GST_VP9_PARSER_OK)
    return rst;

  rst = parse_skip_probs (hdr, &bd);
  if (rst != GST_VP9_PARSER_OK)
    return rst;

  frame_is_intra_only = (hdr->frame_type == GST_VP9_KEY_FRAME
      || hdr->intra_only);

  if (!frame_is_intra_only) {
    rst = parse_inter_mode_probs (hdr, &bd);
    if (rst != GST_VP9_PARSER_OK)
      return rst;

    if (hdr->interpolation_filter == GST_VP9_INTERPOLATION_FILTER_SWITCHABLE) {
      rst = parse_interp_filter_probs (hdr, &bd);
      if (rst != GST_VP9_PARSER_OK)
        return rst;
    }

    rst = parse_is_inter_probs (hdr, &bd);
    if (rst != GST_VP9_PARSER_OK)
      return rst;

    rst = parse_frame_reference (hdr, &bd);
    if (rst != GST_VP9_PARSER_OK)
      return rst;

    rst = parse_frame_reference_mode_probs (hdr, &bd);
    if (rst != GST_VP9_PARSER_OK)
      return rst;

    rst = parse_y_mode_probs (hdr, &bd);
    if (rst != GST_VP9_PARSER_OK)
      return rst;

    rst = parse_partition_probs (hdr, &bd);
    if (rst != GST_VP9_PARSER_OK)
      return rst;

    rst = parse_mv_probs (hdr, &bd);
    if (rst != GST_VP9_PARSER_OK)
      return rst;
  }

  rst = exit_bool (&bd);
  if (rst != GST_VP9_PARSER_OK) {
    GST_ERROR ("The boolean decoder did not exit cleanly.");
    return rst;
  }

  return GST_VP9_PARSER_OK;
}

static const gint16 dc_qlookup[256] = {
  4, 8, 8, 9, 10, 11, 12, 12,
  13, 14, 15, 16, 17, 18, 19, 19,
  20, 21, 22, 23, 24, 25, 26, 26,
  27, 28, 29, 30, 31, 32, 32, 33,
  34, 35, 36, 37, 38, 38, 39, 40,
  41, 42, 43, 43, 44, 45, 46, 47,
  48, 48, 49, 50, 51, 52, 53, 53,
  54, 55, 56, 57, 57, 58, 59, 60,
  61, 62, 62, 63, 64, 65, 66, 66,
  67, 68, 69, 70, 70, 71, 72, 73,
  74, 74, 75, 76, 77, 78, 78, 79,
  80, 81, 81, 82, 83, 84, 85, 85,
  87, 88, 90, 92, 93, 95, 96, 98,
  99, 101, 102, 104, 105, 107, 108, 110,
  111, 113, 114, 116, 117, 118, 120, 121,
  123, 125, 127, 129, 131, 134, 136, 138,
  140, 142, 144, 146, 148, 150, 152, 154,
  156, 158, 161, 164, 166, 169, 172, 174,
  177, 180, 182, 185, 187, 190, 192, 195,
  199, 202, 205, 208, 211, 214, 217, 220,
  223, 226, 230, 233, 237, 240, 243, 247,
  250, 253, 257, 261, 265, 269, 272, 276,
  280, 284, 288, 292, 296, 300, 304, 309,
  313, 317, 322, 326, 330, 335, 340, 344,
  349, 354, 359, 364, 369, 374, 379, 384,
  389, 395, 400, 406, 411, 417, 423, 429,
  435, 441, 447, 454, 461, 467, 475, 482,
  489, 497, 505, 513, 522, 530, 539, 549,
  559, 569, 579, 590, 602, 614, 626, 640,
  654, 668, 684, 700, 717, 736, 755, 775,
  796, 819, 843, 869, 896, 925, 955, 988,
  1022, 1058, 1098, 1139, 1184, 1232, 1282, 1336,
};

static const gint16 dc_qlookup_10[256] = {
  4, 9, 10, 13, 15, 17, 20, 22,
  25, 28, 31, 34, 37, 40, 43, 47,
  50, 53, 57, 60, 64, 68, 71, 75,
  78, 82, 86, 90, 93, 97, 101, 105,
  109, 113, 116, 120, 124, 128, 132, 136,
  140, 143, 147, 151, 155, 159, 163, 166,
  170, 174, 178, 182, 185, 189, 193, 197,
  200, 204, 208, 212, 215, 219, 223, 226,
  230, 233, 237, 241, 244, 248, 251, 255,
  259, 262, 266, 269, 273, 276, 280, 283,
  287, 290, 293, 297, 300, 304, 307, 310,
  314, 317, 321, 324, 327, 331, 334, 337,
  343, 350, 356, 362, 369, 375, 381, 387,
  394, 400, 406, 412, 418, 424, 430, 436,
  442, 448, 454, 460, 466, 472, 478, 484,
  490, 499, 507, 516, 525, 533, 542, 550,
  559, 567, 576, 584, 592, 601, 609, 617,
  625, 634, 644, 655, 666, 676, 687, 698,
  708, 718, 729, 739, 749, 759, 770, 782,
  795, 807, 819, 831, 844, 856, 868, 880,
  891, 906, 920, 933, 947, 961, 975, 988,
  1001, 1015, 1030, 1045, 1061, 1076, 1090, 1105,
  1120, 1137, 1153, 1170, 1186, 1202, 1218, 1236,
  1253, 1271, 1288, 1306, 1323, 1342, 1361, 1379,
  1398, 1416, 1436, 1456, 1476, 1496, 1516, 1537,
  1559, 1580, 1601, 1624, 1647, 1670, 1692, 1717,
  1741, 1766, 1791, 1817, 1844, 1871, 1900, 1929,
  1958, 1990, 2021, 2054, 2088, 2123, 2159, 2197,
  2236, 2276, 2319, 2363, 2410, 2458, 2508, 2561,
  2616, 2675, 2737, 2802, 2871, 2944, 3020, 3102,
  3188, 3280, 3375, 3478, 3586, 3702, 3823, 3953,
  4089, 4236, 4394, 4559, 4737, 4929, 5130, 5347,
};

static const gint16 dc_qlookup_12[256] = {
  4, 12, 18, 25, 33, 41, 50, 60,
  70, 80, 91, 103, 115, 127, 140, 153,
  166, 180, 194, 208, 222, 237, 251, 266,
  281, 296, 312, 327, 343, 358, 374, 390,
  405, 421, 437, 453, 469, 484, 500, 516,
  532, 548, 564, 580, 596, 611, 627, 643,
  659, 674, 690, 706, 721, 737, 752, 768,
  783, 798, 814, 829, 844, 859, 874, 889,
  904, 919, 934, 949, 964, 978, 993, 1008,
  1022, 1037, 1051, 1065, 1080, 1094, 1108, 1122,
  1136, 1151, 1165, 1179, 1192, 1206, 1220, 1234,
  1248, 1261, 1275, 1288, 1302, 1315, 1329, 1342,
  1368, 1393, 1419, 1444, 1469, 1494, 1519, 1544,
  1569, 1594, 1618, 1643, 1668, 1692, 1717, 1741,
  1765, 1789, 1814, 1838, 1862, 1885, 1909, 1933,
  1957, 1992, 2027, 2061, 2096, 2130, 2165, 2199,
  2233, 2267, 2300, 2334, 2367, 2400, 2434, 2467,
  2499, 2532, 2575, 2618, 2661, 2704, 2746, 2788,
  2830, 2872, 2913, 2954, 2995, 3036, 3076, 3127,
  3177, 3226, 3275, 3324, 3373, 3421, 3469, 3517,
  3565, 3621, 3677, 3733, 3788, 3843, 3897, 3951,
  4005, 4058, 4119, 4181, 4241, 4301, 4361, 4420,
  4479, 4546, 4612, 4677, 4742, 4807, 4871, 4942,
  5013, 5083, 5153, 5222, 5291, 5367, 5442, 5517,
  5591, 5665, 5745, 5825, 5905, 5984, 6063, 6149,
  6234, 6319, 6404, 6495, 6587, 6678, 6769, 6867,
  6966, 7064, 7163, 7269, 7376, 7483, 7599, 7715,
  7832, 7958, 8085, 8214, 8352, 8492, 8635, 8788,
  8945, 9104, 9275, 9450, 9639, 9832, 10031, 10245,
  10465, 10702, 10946, 11210, 11482, 11776, 12081, 12409,
  12750, 13118, 13501, 13913, 14343, 14807, 15290, 15812,
  16356, 16943, 17575, 18237, 18949, 19718, 20521, 21387,
};

static const gint16 ac_qlookup[256] = {
  4, 8, 9, 10, 11, 12, 13, 14,
  15, 16, 17, 18, 19, 20, 21, 22,
  23, 24, 25, 26, 27, 28, 29, 30,
  31, 32, 33, 34, 35, 36, 37, 38,
  39, 40, 41, 42, 43, 44, 45, 46,
  47, 48, 49, 50, 51, 52, 53, 54,
  55, 56, 57, 58, 59, 60, 61, 62,
  63, 64, 65, 66, 67, 68, 69, 70,
  71, 72, 73, 74, 75, 76, 77, 78,
  79, 80, 81, 82, 83, 84, 85, 86,
  87, 88, 89, 90, 91, 92, 93, 94,
  95, 96, 97, 98, 99, 100, 101, 102,
  104, 106, 108, 110, 112, 114, 116, 118,
  120, 122, 124, 126, 128, 130, 132, 134,
  136, 138, 140, 142, 144, 146, 148, 150,
  152, 155, 158, 161, 164, 167, 170, 173,
  176, 179, 182, 185, 188, 191, 194, 197,
  200, 203, 207, 211, 215, 219, 223, 227,
  231, 235, 239, 243, 247, 251, 255, 260,
  265, 270, 275, 280, 285, 290, 295, 300,
  305, 311, 317, 323, 329, 335, 341, 347,
  353, 359, 366, 373, 380, 387, 394, 401,
  408, 416, 424, 432, 440, 448, 456, 465,
  474, 483, 492, 501, 510, 520, 530, 540,
  550, 560, 571, 582, 593, 604, 615, 627,
  639, 651, 663, 676, 689, 702, 715, 729,
  743, 757, 771, 786, 801, 816, 832, 848,
  864, 881, 898, 915, 933, 951, 969, 988,
  1007, 1026, 1046, 1066, 1087, 1108, 1129, 1151,
  1173, 1196, 1219, 1243, 1267, 1292, 1317, 1343,
  1369, 1396, 1423, 1451, 1479, 1508, 1537, 1567,
  1597, 1628, 1660, 1692, 1725, 1759, 1793, 1828,
};

static const gint16 ac_qlookup_10[256] = {
  4, 9, 11, 13, 16, 18, 21, 24,
  27, 30, 33, 37, 40, 44, 48, 51,
  55, 59, 63, 67, 71, 75, 79, 83,
  88, 92, 96, 100, 105, 109, 114, 118,
  122, 127, 131, 136, 140, 145, 149, 154,
  158, 163, 168, 172, 177, 181, 186, 190,
  195, 199, 204, 208, 213, 217, 222, 226,
  231, 235, 240, 244, 249, 253, 258, 262,
  267, 271, 275, 280, 284, 289, 293, 297,
  302, 306, 311, 315, 319, 324, 328, 332,
  337, 341, 345, 349, 354, 358, 362, 367,
  371, 375, 379, 384, 388, 392, 396, 401,
  409, 417, 425, 433, 441, 449, 458, 466,
  474, 482, 490, 498, 506, 514, 523, 531,
  539, 547, 555, 563, 571, 579, 588, 596,
  604, 616, 628, 640, 652, 664, 676, 688,
  700, 713, 725, 737, 749, 761, 773, 785,
  797, 809, 825, 841, 857, 873, 889, 905,
  922, 938, 954, 970, 986, 1002, 1018, 1038,
  1058, 1078, 1098, 1118, 1138, 1158, 1178, 1198,
  1218, 1242, 1266, 1290, 1314, 1338, 1362, 1386,
  1411, 1435, 1463, 1491, 1519, 1547, 1575, 1603,
  1631, 1663, 1695, 1727, 1759, 1791, 1823, 1859,
  1895, 1931, 1967, 2003, 2039, 2079, 2119, 2159,
  2199, 2239, 2283, 2327, 2371, 2415, 2459, 2507,
  2555, 2603, 2651, 2703, 2755, 2807, 2859, 2915,
  2971, 3027, 3083, 3143, 3203, 3263, 3327, 3391,
  3455, 3523, 3591, 3659, 3731, 3803, 3876, 3952,
  4028, 4104, 4184, 4264, 4348, 4432, 4516, 4604,
  4692, 4784, 4876, 4972, 5068, 5168, 5268, 5372,
  5476, 5584, 5692, 5804, 5916, 6032, 6148, 6268,
  6388, 6512, 6640, 6768, 6900, 7036, 7172, 7312,
};

static const gint16 ac_qlookup_12[256] = {
  4, 13, 19, 27, 35, 44, 54, 64,
  75, 87, 99, 112, 126, 139, 154, 168,
  183, 199, 214, 230, 247, 263, 280, 297,
  314, 331, 349, 366, 384, 402, 420, 438,
  456, 475, 493, 511, 530, 548, 567, 586,
  604, 623, 642, 660, 679, 698, 716, 735,
  753, 772, 791, 809, 828, 846, 865, 884,
  902, 920, 939, 957, 976, 994, 1012, 1030,
  1049, 1067, 1085, 1103, 1121, 1139, 1157, 1175,
  1193, 1211, 1229, 1246, 1264, 1282, 1299, 1317,
  1335, 1352, 1370, 1387, 1405, 1422, 1440, 1457,
  1474, 1491, 1509, 1526, 1543, 1560, 1577, 1595,
  1627, 1660, 1693, 1725, 1758, 1791, 1824, 1856,
  1889, 1922, 1954, 1987, 2020, 2052, 2085, 2118,
  2150, 2183, 2216, 2248, 2281, 2313, 2346, 2378,
  2411, 2459, 2508, 2556, 2605, 2653, 2701, 2750,
  2798, 2847, 2895, 2943, 2992, 3040, 3088, 3137,
  3185, 3234, 3298, 3362, 3426, 3491, 3555, 3619,
  3684, 3748, 3812, 3876, 3941, 4005, 4069, 4149,
  4230, 4310, 4390, 4470, 4550, 4631, 4711, 4791,
  4871, 4967, 5064, 5160, 5256, 5352, 5448, 5544,
  5641, 5737, 5849, 5961, 6073, 6185, 6297, 6410,
  6522, 6650, 6778, 6906, 7034, 7162, 7290, 7435,
  7579, 7723, 7867, 8011, 8155, 8315, 8475, 8635,
  8795, 8956, 9132, 9308, 9484, 9660, 9836, 10028,
  10220, 10412, 10604, 10812, 11020, 11228, 11437, 11661,
  11885, 12109, 12333, 12573, 12813, 13053, 13309, 13565,
  13821, 14093, 14365, 14637, 14925, 15213, 15502, 15806,
  16110, 16414, 16734, 17054, 17390, 17726, 18062, 18414,
  18766, 19134, 19502, 19886, 20270, 20670, 21070, 21486,
  21902, 22334, 22766, 23214, 23662, 24126, 24590, 25070,
  25551, 26047, 26559, 27071, 27599, 28143, 28687, 29247,
};

static GstVp9ParserResult
parse_frame_marker (GstBitReader * br)
{
  guint8 frame_marker;

  VP9_READ_UINT8 (frame_marker, 2);

  if (frame_marker != GST_VP9_FRAME_MARKER) {
    GST_ERROR ("Invalid VP9 Frame Marker");
    return GST_VP9_PARSER_ERROR;
  }

  return GST_VP9_PARSER_OK;
}

static GstVp9ParserResult
parse_frame_sync_code (GstBitReader * br)
{
  guint32 code;

  VP9_READ_UINT32 (code, 24);
  if (code != GST_VP9_SYNC_CODE) {
    GST_ERROR ("%d is not VP9 sync code", code);
    return GST_VP9_PARSER_ERROR;
  }

  return GST_VP9_PARSER_OK;
}

/* 6.2.2 Color config syntax */
static GstVp9ParserResult
parse_color_config (GstVp9StatefulParser * self, GstBitReader * br,
    GstVp9FrameHeader * header)
{
  guint8 bit = 0;

  if (header->profile >= GST_VP9_PROFILE_2) {
    VP9_READ_BIT (bit);
    if (bit) {
      header->bit_depth = GST_VP9_BIT_DEPTH_12;
    } else {
      header->bit_depth = GST_VP9_BIT_DEPTH_10;
    }
  } else {
    header->bit_depth = GST_VP9_BIT_DEPTH_8;
  }

  VP9_READ_UINT8 (header->color_space, 3);
  if (header->color_space != GST_VP9_CS_SRGB) {
    VP9_READ_BIT (header->color_range);

    if (header->profile == GST_VP9_PROFILE_1
        || header->profile == GST_VP9_PROFILE_3) {
      VP9_READ_BIT (header->subsampling_x);
      VP9_READ_BIT (header->subsampling_y);

      if (header->subsampling_x == 1 && header->subsampling_y == 1) {
        GST_ERROR
            ("4:2:0 subsampling is not supported in profile_1 or profile_3");
        return GST_VP9_PARSER_ERROR;
      }

      /* reserved bit */
      VP9_READ_BIT (bit);
    } else {
      header->subsampling_y = header->subsampling_x = 1;
    }
  } else {
    header->color_range = GST_VP9_CR_FULL;
    if (header->profile == GST_VP9_PROFILE_1
        || header->profile == GST_VP9_PROFILE_3) {
      /* reserved bit */
      VP9_READ_BIT (bit);
    } else {
      GST_ERROR
          ("4:4:4 subsampling is not supported in profile_0 and profile_2");
      return GST_VP9_PARSER_ERROR;
    }
  }

  self->bit_depth = header->bit_depth;
  self->color_space = header->color_space;
  self->subsampling_x = header->subsampling_x;
  self->subsampling_y = header->subsampling_y;
  self->color_range = header->color_range;

  return GST_VP9_PARSER_OK;
}

/* 6.2 Uncompressed header syntax */
static GstVp9ParserResult
parse_profile (GstBitReader * br, guint8 * profile)
{
  guint8 profile_low_bit, profile_high_bit, ret, bit;

  VP9_READ_BIT (profile_low_bit);
  VP9_READ_BIT (profile_high_bit);

  ret = (profile_high_bit << 1) | profile_low_bit;
  if (ret == 3) {
    /* reserved bit */
    VP9_READ_BIT (bit);
  }

  *profile = ret;

  return GST_VP9_PARSER_OK;
}

/* 6.2.6 Compute image size syntax */
static void
compute_image_size (GstVp9StatefulParser * self, guint32 width, guint32 height)
{
  self->mi_cols = (width + 7) >> 3;
  self->mi_rows = (height + 7) >> 3;
  self->sb64_cols = (self->mi_cols + 7) >> 3;
  self->sb64_rows = (self->mi_rows + 7) >> 3;
}

static GstVp9ParserResult
parse_frame_or_render_size (GstBitReader * br,
    guint32 * width, guint32 * height)
{
  guint32 width_minus_1;
  guint32 height_minus_1;

  VP9_READ_UINT32 (width_minus_1, 16);
  VP9_READ_UINT32 (height_minus_1, 16);

  *width = width_minus_1 + 1;
  *height = height_minus_1 + 1;

  return GST_VP9_PARSER_OK;
}

/* 6.2.3 Frame size syntax */
static GstVp9ParserResult
parse_frame_size (GstVp9StatefulParser * self, GstBitReader * br,
    guint32 * width, guint32 * height)
{
  GstVp9ParserResult rst;

  rst = parse_frame_or_render_size (br, width, height);
  if (rst != GST_VP9_PARSER_OK) {
    GST_ERROR ("Failed to parse frame size");
    return rst;
  }

  compute_image_size (self, *width, *height);

  return GST_VP9_PARSER_OK;
}

/* 6.2.4 Render size syntax */
static GstVp9ParserResult
parse_render_size (GstBitReader * br, GstVp9FrameHeader * header)
{
  VP9_READ_BIT (header->render_and_frame_size_different);
  if (header->render_and_frame_size_different) {
    return parse_frame_or_render_size (br,
        &header->render_width, &header->render_height);
  } else {
    header->render_width = header->width;
    header->render_height = header->height;
  }

  return GST_VP9_PARSER_OK;
}

/* 6.2.5 Frame size with refs syntax */
static GstVp9ParserResult
parse_frame_size_with_refs (GstVp9StatefulParser * self, GstBitReader * br,
    GstVp9FrameHeader * header)
{
  guint8 found_ref = 0;
  guint i;

  for (i = 0; i < GST_VP9_REFS_PER_FRAME; i++) {
    VP9_READ_BIT (found_ref);

    if (found_ref) {
      guint8 idx = header->ref_frame_idx[i];

      header->width = self->reference[idx].width;
      header->height = self->reference[idx].height;
      break;
    }
  }

  if (found_ref == 0) {
    GstVp9ParserResult rst;

    rst = parse_frame_size (self, br, &header->width, &header->height);
    if (rst != GST_VP9_PARSER_OK) {
      GST_ERROR ("Failed to parse frame size without refs");
      return rst;
    }
  } else {
    compute_image_size (self, header->width, header->height);
  }

  return parse_render_size (br, header);
}

/* 6.2.7 Interpolation filter syntax */
static GstVp9ParserResult
read_interpolation_filter (GstBitReader * br, GstVp9FrameHeader * header)
{
  static const GstVp9InterpolationFilter filter_map[] = {
    GST_VP9_INTERPOLATION_FILTER_EIGHTTAP_SMOOTH,
    GST_VP9_INTERPOLATION_FILTER_EIGHTTAP,
    GST_VP9_INTERPOLATION_FILTER_EIGHTTAP_SHARP,
    GST_VP9_INTERPOLATION_FILTER_BILINEAR
  };
  guint8 is_filter_switchable;

  VP9_READ_BIT (is_filter_switchable);
  if (is_filter_switchable) {
    header->interpolation_filter = GST_VP9_INTERPOLATION_FILTER_SWITCHABLE;
  } else {
    guint8 map_val;

    VP9_READ_UINT8 (map_val, 2);
    header->interpolation_filter = filter_map[map_val];
  }

  return GST_VP9_PARSER_OK;
}

/* 6.2.8 Loop filter params syntax */
static GstVp9ParserResult
parse_loop_filter_params (GstBitReader * br, GstVp9LoopFilterParams * params)
{
  VP9_READ_UINT8 (params->loop_filter_level, 6);
  VP9_READ_UINT8 (params->loop_filter_sharpness, 3);
  VP9_READ_BIT (params->loop_filter_delta_enabled);

  if (params->loop_filter_delta_enabled) {
    VP9_READ_BIT (params->loop_filter_delta_update);
    if (params->loop_filter_delta_update) {
      guint i;

      for (i = 0; i < GST_VP9_MAX_REF_LF_DELTAS; i++) {
        VP9_READ_BIT (params->update_ref_delta[i]);
        if (params->update_ref_delta[i]) {
          VP9_READ_SIGNED_8 (params->loop_filter_ref_deltas[i], 6);
        }
      }

      for (i = 0; i < GST_VP9_MAX_MODE_LF_DELTAS; i++) {
        VP9_READ_BIT (params->update_mode_delta[i]);
        if (params->update_mode_delta[i])
          VP9_READ_SIGNED_8 (params->loop_filter_mode_deltas[i], 6);
      }
    }
  }

  return GST_VP9_PARSER_OK;
}

/* 6.2.10 Delta quantizer syntax */
static inline GstVp9ParserResult
parse_delta_q (GstBitReader * br, gint8 * value)
{
  guint8 read_signed;
  gint8 delta_q;

  VP9_READ_BIT (read_signed);
  if (!read_signed) {
    *value = 0;
    return GST_VP9_PARSER_OK;
  }

  VP9_READ_SIGNED_8 (delta_q, 4);
  *value = delta_q;

  return GST_VP9_PARSER_OK;
}

/* 6.2.9 Quantization params syntax */
static GstVp9ParserResult
parse_quantization_params (GstBitReader * br, GstVp9FrameHeader * header)
{
  GstVp9QuantizationParams *params = &header->quantization_params;
  GstVp9ParserResult rst;

  VP9_READ_UINT8 (params->base_q_idx, 8);
  rst = parse_delta_q (br, &params->delta_q_y_dc);
  if (rst != GST_VP9_PARSER_OK)
    return rst;

  rst = parse_delta_q (br, &params->delta_q_uv_dc);
  if (rst != GST_VP9_PARSER_OK)
    return rst;

  rst = parse_delta_q (br, &params->delta_q_uv_ac);
  if (rst != GST_VP9_PARSER_OK)
    return rst;

  header->lossless_flag = params->base_q_idx == 0 && params->delta_q_y_dc == 0
      && params->delta_q_uv_dc == 0 && params->delta_q_uv_ac == 0;

  return GST_VP9_PARSER_OK;
}

/* 6.2.12 Probability syntax */
static GstVp9ParserResult
read_prob (GstBitReader * br, guint8 * val)
{
  guint8 prob = GST_VP9_MAX_PROB;
  guint8 prob_coded;

  VP9_READ_BIT (prob_coded);

  if (prob_coded)
    VP9_READ_UINT8 (prob, 8);

  *val = prob;

  return GST_VP9_PARSER_OK;
}

/* 6.2.11 Segmentation params syntax */
static GstVp9ParserResult
parse_segmentation_params (GstBitReader * br, GstVp9SegmentationParams * params)
{
  guint i;
  GstVp9ParserResult rst;

  params->segmentation_update_map = 0;
  params->segmentation_update_data = 0;
  params->segmentation_temporal_update = 0;

  VP9_READ_BIT (params->segmentation_enabled);
  if (!params->segmentation_enabled)
    return GST_VP9_PARSER_OK;

  VP9_READ_BIT (params->segmentation_update_map);
  if (params->segmentation_update_map) {
    for (i = 0; i < GST_VP9_SEG_TREE_PROBS; i++) {
      rst = read_prob (br, &params->segmentation_tree_probs[i]);
      if (rst != GST_VP9_PARSER_OK) {
        GST_ERROR ("Failed to read segmentation_tree_probs[%d]", i);
        return rst;
      }
    }

    VP9_READ_BIT (params->segmentation_temporal_update);
    if (params->segmentation_temporal_update) {
      for (i = 0; i < GST_VP9_PREDICTION_PROBS; i++) {
        rst = read_prob (br, &params->segmentation_pred_prob[i]);
        if (rst != GST_VP9_PARSER_OK) {
          GST_ERROR ("Failed to read segmentation_pred_prob[%d]", i);
          return rst;
        }
      }
    } else {
      for (i = 0; i < GST_VP9_PREDICTION_PROBS; i++)
        params->segmentation_pred_prob[i] = GST_VP9_MAX_PROB;
    }
  }

  VP9_READ_BIT (params->segmentation_update_data);
  if (params->segmentation_update_data) {
    VP9_READ_BIT (params->segmentation_abs_or_delta_update);

    for (i = 0; i < GST_VP9_MAX_SEGMENTS; i++) {
      VP9_READ_BIT (params->feature_enabled[i][GST_VP9_SEG_LVL_ALT_Q]);
      if (params->feature_enabled[i][GST_VP9_SEG_LVL_ALT_Q]) {
        VP9_READ_SIGNED_16 (params->feature_data[i][GST_VP9_SEG_LVL_ALT_Q], 8);
      } else {
        params->feature_data[i][GST_VP9_SEG_LVL_ALT_Q] = 0;
      }

      VP9_READ_BIT (params->feature_enabled[i][GST_VP9_SEG_LVL_ALT_L]);
      if (params->feature_enabled[i][GST_VP9_SEG_LVL_ALT_L]) {
        VP9_READ_SIGNED_8 (params->feature_data[i][GST_VP9_SEG_LVL_ALT_L], 6);
      } else {
        params->feature_data[i][GST_VP9_SEG_LVL_ALT_L] = 0;
      }

      VP9_READ_BIT (params->feature_enabled[i][GST_VP9_SEG_LVL_REF_FRAME]);
      if (params->feature_enabled[i][GST_VP9_SEG_LVL_REF_FRAME]) {
        guint8 val;

        VP9_READ_UINT8 (val, 2);
        params->feature_data[i][GST_VP9_SEG_LVL_REF_FRAME] = val;
      } else {
        params->feature_data[i][GST_VP9_SEG_LVL_REF_FRAME] = 0;
      }

      VP9_READ_BIT (params->feature_enabled[i][GST_VP9_SEG_SEG_LVL_SKIP]);
    }
  }
  return GST_VP9_PARSER_OK;
}

/* 6.2.14 Tile size calculation */
static guint
calc_min_log2_tile_cols (guint32 sb64_cols)
{
  guint minLog2 = 0;
  static const guint MAX_TILE_WIDTH_B64 = 64;

  while ((MAX_TILE_WIDTH_B64 << minLog2) < sb64_cols)
    minLog2++;

  return minLog2;
}

static guint
calc_max_log2_tile_cols (guint32 sb64_cols)
{
  guint maxLog2 = 1;
  static const guint MIN_TILE_WIDTH_B64 = 4;

  while ((sb64_cols >> maxLog2) >= MIN_TILE_WIDTH_B64)
    maxLog2++;

  return maxLog2 - 1;
}

/* 6.2.13 Tile info syntax */
static GstVp9ParserResult
parse_tile_info (GstVp9StatefulParser * self, GstBitReader * br,
    GstVp9FrameHeader * header)
{
  guint32 minLog2TileCols = calc_min_log2_tile_cols (self->sb64_cols);
  guint32 maxLog2TileCols = calc_max_log2_tile_cols (self->sb64_cols);

  header->tile_cols_log2 = minLog2TileCols;

  while (header->tile_cols_log2 < maxLog2TileCols) {
    guint8 increment_tile_cols_log2;

    VP9_READ_BIT (increment_tile_cols_log2);
    if (increment_tile_cols_log2)
      header->tile_cols_log2++;
    else
      break;
  }

  if (header->tile_cols_log2 > 6) {
    GST_ERROR ("Invalid number of tile columns");
    return GST_VP9_PARSER_ERROR;
  }

  VP9_READ_BIT (header->tile_rows_log2);
  if (header->tile_rows_log2) {
    guint8 increment_tile_rows_log2;

    VP9_READ_BIT (increment_tile_rows_log2);
    header->tile_rows_log2 += increment_tile_rows_log2;
  }

  return GST_VP9_PARSER_OK;
}

/* 7.2 Uncompressed header semantics */
static void
setup_past_independence (GstVp9StatefulParser * self,
    GstVp9FrameHeader * header)
{
  memset (self->segmentation_params.feature_enabled,
      0, sizeof (self->segmentation_params.feature_enabled));
  memset (self->segmentation_params.feature_data,
      0, sizeof (self->segmentation_params.feature_data));

  self->segmentation_params.segmentation_abs_or_delta_update = 0;

  self->loop_filter_params.loop_filter_delta_enabled = 1;
  self->loop_filter_params.loop_filter_ref_deltas[GST_VP9_REF_FRAME_INTRA] = 1;
  self->loop_filter_params.loop_filter_ref_deltas[GST_VP9_REF_FRAME_LAST] = 0;
  self->loop_filter_params.loop_filter_ref_deltas[GST_VP9_REF_FRAME_GOLDEN] =
      -1;
  self->loop_filter_params.loop_filter_ref_deltas[GST_VP9_REF_FRAME_ALTREF] =
      -1;

  memset (self->loop_filter_params.loop_filter_mode_deltas, 0,
      sizeof (self->loop_filter_params.loop_filter_mode_deltas));
  memset (header->ref_frame_sign_bias, 0, sizeof (header->ref_frame_sign_bias));
}

/**
 * gst_vp9_stateful_parser_new:
 *
 * Creates a new #GstVp9StatefulParser. It should be freed with
 * gst_vp9_stateful_parser_free() after use.
 *
 * Returns: a new #GstVp9StatefulParser
 *
 * Since: 1.20
 */
GstVp9StatefulParser *
gst_vp9_stateful_parser_new (void)
{
  GstVp9StatefulParser *parser;

  parser = g_new0 (GstVp9StatefulParser, 1);

  return parser;
}

/**
 * gst_vp9_stateful_parser_free:
 * @parser: the #GstVp9StatefulParser to free
 *
 * Frees @parser.
 *
 * Since: 1.20
 */
void
gst_vp9_stateful_parser_free (GstVp9StatefulParser * parser)
{
  g_free (parser);
}

/**
 * gst_vp9_stateful_parser_parse_compressed_frame_header:
 * @parser: The #GstVp9StatefulParser
 * @header: The #GstVp9FrameHeader to fill
 * @data: The data to parse
 * @size: The size of the @data to parse
 *
 * Parses the compressed information in the VP9 bitstream contained in @data,
 * and fills in @header with the parsed values.
 * The @size argument represent the whole frame size.
 *
 * Returns: a #GstVp9ParserResult
 *
 * Since: 1.20
 */

GstVp9ParserResult
gst_vp9_stateful_parser_parse_compressed_frame_header (GstVp9StatefulParser *
    parser, GstVp9FrameHeader * header, const guint8 * data, gsize size)
{
  GstVp9ParserResult rst = GST_VP9_PARSER_OK;
  GstBitReader bit_reader;
  GstBitReader *br = &bit_reader;

  gst_bit_reader_init (br, data, size);

  rst = parse_compressed_header (parser, header, br);
  if (rst != GST_VP9_PARSER_OK) {
    GST_ERROR ("Failed to parse the compressed header");
    return GST_VP9_PARSER_ERROR;
  }

  return rst;
}

/**
 * gst_vp9_stateful_parser_parse_uncompressed_frame_header:
 * @parser: The #GstVp9StatefulParser
 * @header: The #GstVp9FrameHeader to fill
 * @data: The data to parse
 * @size: The size of the @data to parse
 *
 * Parses the VP9 bitstream contained in @data, and fills in @header
 * with the information. The @size argument represent the whole frame size.
 *
 * Returns: a #GstVp9ParserResult
 *
 * Since: 1.20
 */
GstVp9ParserResult
gst_vp9_stateful_parser_parse_uncompressed_frame_header (GstVp9StatefulParser *
    parser, GstVp9FrameHeader * header, const guint8 * data, gsize size)
{
  GstBitReader bit_reader;
  GstBitReader *br = &bit_reader;
  gboolean frame_is_intra = FALSE;
  GstVp9ParserResult rst = GST_VP9_PARSER_OK;
  guint i;

  g_return_val_if_fail (parser, GST_VP9_PARSER_ERROR);
  g_return_val_if_fail (header, GST_VP9_PARSER_ERROR);
  g_return_val_if_fail (data, GST_VP9_PARSER_ERROR);
  g_return_val_if_fail (size, GST_VP9_PARSER_ERROR);

  gst_bit_reader_init (br, data, size);
  memset (header, 0, sizeof (GstVp9FrameHeader));

  /* Parsing Uncompressed Data Chunk */
  rst = parse_frame_marker (br);
  if (rst != GST_VP9_PARSER_OK)
    return rst;

  rst = parse_profile (br, &header->profile);
  if (rst != GST_VP9_PARSER_OK)
    return rst;

  CHECK_ALLOWED (header->profile, GST_VP9_PROFILE_0, GST_VP9_PROFILE_3);

  VP9_READ_BIT (header->show_existing_frame);
  if (header->show_existing_frame) {
    VP9_READ_UINT8 (header->frame_to_show_map_idx, 3);
    return GST_VP9_PARSER_OK;
  }

  VP9_READ_BIT (header->frame_type);
  VP9_READ_BIT (header->show_frame);
  VP9_READ_BIT (header->error_resilient_mode);

  if (header->frame_type == GST_VP9_KEY_FRAME) {
    rst = parse_frame_sync_code (br);
    if (rst != GST_VP9_PARSER_OK) {
      GST_ERROR ("Invalid VP9 sync code in keyframe");
      return rst;
    }

    rst = parse_color_config (parser, br, header);
    if (rst != GST_VP9_PARSER_OK) {
      GST_ERROR ("Failed to parse color config of keyframe");
      return rst;
    }

    rst = parse_frame_size (parser, br, &header->width, &header->height);
    if (rst != GST_VP9_PARSER_OK) {
      GST_ERROR ("Failed to parse frame size of keyframe");
      return rst;
    }

    rst = parse_render_size (br, header);
    if (rst != GST_VP9_PARSER_OK) {
      GST_ERROR ("Failed to parse render size of keyframe");
      return rst;
    }

    header->refresh_frame_flags = 0xff;
    frame_is_intra = TRUE;
  } else {
    if (header->show_frame == 0)
      VP9_READ_BIT (header->intra_only);

    frame_is_intra = header->intra_only;
    if (header->error_resilient_mode == 0)
      VP9_READ_UINT8 (header->reset_frame_context, 2);

    if (header->intra_only) {
      rst = parse_frame_sync_code (br);
      if (rst != GST_VP9_PARSER_OK) {
        GST_ERROR ("Invalid VP9 sync code in intra-only frame");
        return rst;
      }

      if (header->profile > GST_VP9_PROFILE_0) {
        rst = parse_color_config (parser, br, header);
        if (rst != GST_VP9_PARSER_OK) {
          GST_ERROR ("Failed to parse color config of intra-only frame");
          return rst;
        }
      } else {
        parser->color_space = header->color_space = GST_VP9_CS_BT_601;
        parser->color_range = header->color_range = GST_VP9_CR_LIMITED;
        parser->subsampling_x = parser->subsampling_y =
            header->subsampling_x = header->subsampling_y = 1;
        parser->bit_depth = header->bit_depth = GST_VP9_BIT_DEPTH_8;
      }

      VP9_READ_UINT8 (header->refresh_frame_flags, 8);
      rst = parse_frame_size (parser, br, &header->width, &header->height);
      if (rst != GST_VP9_PARSER_OK) {
        GST_ERROR ("Failed to pase frame size of intra-only frame");
        return rst;
      }

      rst = parse_render_size (br, header);
      if (rst != GST_VP9_PARSER_OK) {
        GST_ERROR ("Failed to parse render size of intra-only frame");
        return rst;
      }
    } else {
      /* copy color_config from previously parsed one */
      header->color_space = parser->color_space;
      header->color_range = parser->color_range;
      header->subsampling_x = parser->subsampling_x;
      header->subsampling_y = parser->subsampling_y;
      header->bit_depth = parser->bit_depth;

      VP9_READ_UINT8 (header->refresh_frame_flags, 8);
      for (i = 0; i < GST_VP9_REFS_PER_FRAME; i++) {
        VP9_READ_UINT8 (header->ref_frame_idx[i], 3);
        VP9_READ_BIT (header->ref_frame_sign_bias[GST_VP9_REF_FRAME_LAST + i]);
      }

      rst = parse_frame_size_with_refs (parser, br, header);
      if (rst != GST_VP9_PARSER_OK) {
        GST_ERROR ("Failed to parse frame size with refs");
        return rst;
      }

      VP9_READ_BIT (header->allow_high_precision_mv);
      rst = read_interpolation_filter (br, header);
      if (rst != GST_VP9_PARSER_OK) {
        GST_ERROR ("Failed to read interpolation filter information");
        return rst;
      }
    }
  }

  if (!header->error_resilient_mode) {
    VP9_READ_BIT (header->refresh_frame_context);
    VP9_READ_BIT (header->frame_parallel_decoding_mode);
  } else {
    header->refresh_frame_context = 0;
    header->frame_parallel_decoding_mode = 1;
  }

  VP9_READ_UINT8 (header->frame_context_idx, 2);

  if (frame_is_intra || header->error_resilient_mode)
    setup_past_independence (parser, header);

  /* First update our own table, and we will copy to frame header later */
  rst = parse_loop_filter_params (br, &parser->loop_filter_params);
  if (rst != GST_VP9_PARSER_OK) {
    GST_ERROR ("Failed to parse loop filter params");
    return rst;
  }

  rst = parse_quantization_params (br, header);
  if (rst != GST_VP9_PARSER_OK) {
    GST_ERROR ("Failed to parse quantization params");
    return rst;
  }

  /* Also update our own table, then it will be copied later */
  rst = parse_segmentation_params (br, &parser->segmentation_params);
  if (rst != GST_VP9_PARSER_OK) {
    GST_ERROR ("Failed to parse segmentation params");
    return rst;
  }

  rst = parse_tile_info (parser, br, header);
  if (rst != GST_VP9_PARSER_OK) {
    GST_ERROR ("Failed to parse tile info");
    return rst;
  }

  VP9_READ_UINT16 (header->header_size_in_bytes, 16);
  if (!header->header_size_in_bytes) {
    GST_ERROR ("Failed to parse header size in bytes");
    return GST_VP9_PARSER_ERROR;
  }

  /* copy our values to header */
  memcpy (&header->loop_filter_params, &parser->loop_filter_params,
      sizeof (GstVp9LoopFilterParams));
  memcpy (&header->segmentation_params, &parser->segmentation_params,
      sizeof (GstVp9SegmentationParams));

  /* And update reference frames */
  for (i = 0; i < GST_VP9_REF_FRAMES; i++) {
    guint8 flag = (1 << i);
    if ((header->refresh_frame_flags & flag) != 0) {
      parser->reference[i].width = header->width;
      parser->reference[i].height = header->height;
    }
  }

  header->frame_header_length_in_bytes = (gst_bit_reader_get_pos (br) + 7) / 8;


  return GST_VP9_PARSER_OK;
}

/**
 * gst_vp9_seg_feature_active:
 * @params: a #GstVp9SegmentationParams
 * @segment_id: a segment id
 * @feature: a segmentation feature
 *
 * An implementation of "seg_feature_active" function specified in
 * "6.4.9 Segmentation feature active syntax"
 *
 * Returns: %TRUE if feature is active
 *
 * Since: 1.20
 */
gboolean
gst_vp9_seg_feature_active (const GstVp9SegmentationParams * params,
    guint8 segment_id, guint8 feature)
{
  g_return_val_if_fail (params != NULL, FALSE);
  g_return_val_if_fail (segment_id < GST_VP9_MAX_SEGMENTS, FALSE);
  g_return_val_if_fail (feature < GST_VP9_SEG_LVL_MAX, FALSE);

  return params->segmentation_enabled &&
      params->feature_enabled[segment_id][feature];
}

/**
 * gst_vp9_get_qindex:
 * @segmentation_params: a #GstVp9SegmentationParams
 * @quantization_params: a #GstVp9QuantizationParams
 * @segment_id: a segment id
 *
 * An implementation of "get_qindex" function specfied in
 * "8.6.1 Dequantization functions"
 *
 * Returns: the quantizer index
 *
 * Since: 1.20
 */
guint8
gst_vp9_get_qindex (const GstVp9SegmentationParams * segmentation_params,
    const GstVp9QuantizationParams * quantization_params, guint8 segment_id)
{
  guint8 base_q_index;

  g_return_val_if_fail (segmentation_params != NULL, 0);
  g_return_val_if_fail (quantization_params != NULL, 0);
  g_return_val_if_fail (segment_id < GST_VP9_MAX_SEGMENTS, 0);

  base_q_index = quantization_params->base_q_idx;

  if (gst_vp9_seg_feature_active (segmentation_params, segment_id,
          GST_VP9_SEG_LVL_ALT_Q)) {
    gint data =
        segmentation_params->feature_data[segment_id][GST_VP9_SEG_LVL_ALT_Q];

    if (!segmentation_params->segmentation_abs_or_delta_update)
      data += base_q_index;

    return CLAMP (data, 0, 255);
  }

  return base_q_index;
}

/**
 * gst_vp9_get_dc_quant:
 * @qindex: the quantizer index
 * @delta_q_dc: a delta_q_dc value
 * @bit_depth: coded bit depth
 *
 * An implementation of "dc_q" function specified in
 * "8.6.1 Dequantization functions"
 *
 * Returns: the quantizer value for the dc coefficient
 *
 * Since: 1.20
 */
gint16
gst_vp9_get_dc_quant (guint8 qindex, gint8 delta_q_dc, guint8 bit_depth)
{
  guint8 q_table_idx = CLAMP (qindex + delta_q_dc, 0, 255);

  switch (bit_depth) {
    case 8:
      return dc_qlookup[q_table_idx];
    case 10:
      return dc_qlookup_10[q_table_idx];
    case 12:
      return dc_qlookup_12[q_table_idx];
    default:
      GST_WARNING ("Unhandled bitdepth %d", bit_depth);
      break;
  }

  return -1;
}

/**
 * gst_vp9_get_ac_quant:
 * @qindex: the quantizer index
 * @delta_q_ac: a delta_q_ac value
 * @bit_depth: coded bit depth
 *
 * An implementation of "ac_q" function specified in
 * "8.6.1 Dequantization functions"
 *
 * Returns: the quantizer value for the ac coefficient
 *
 * Since: 1.20
 */
gint16
gst_vp9_get_ac_quant (guint8 qindex, gint8 delta_q_ac, guint8 bit_depth)
{
  guint8 q_table_idx = CLAMP (qindex + delta_q_ac, 0, 255);

  switch (bit_depth) {
    case 8:
      return ac_qlookup[q_table_idx];
    case 10:
      return ac_qlookup_10[q_table_idx];
    case 12:
      return ac_qlookup_12[q_table_idx];
    default:
      GST_WARNING ("Unhandled bitdepth %d", bit_depth);
      break;
  }

  return -1;
}
