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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstav1bitwriter.h"
#include <gst/base/gstbitwriter.h>

#define WRITE_BITS_UNCHECK(bw, val, nbits)                                    \
  (nbits <= 8 ? gst_bit_writer_put_bits_uint8 (bw, val, nbits) :              \
   (nbits <= 16 ? gst_bit_writer_put_bits_uint16 (bw, val, nbits) :           \
    (nbits <= 32 ? gst_bit_writer_put_bits_uint32 (bw, val, nbits) :          \
     FALSE)))

#define WRITE_BITS(bw, val, nbits)                                            \
  if (!WRITE_BITS_UNCHECK (bw, val, nbits)) {                                 \
    g_warning ("Unsupported bit size: %u", nbits);                            \
    have_space = FALSE;                                                       \
    goto error;                                                               \
  }

static guint
_av1_uleb_size_in_bytes (guint64 value)
{
  guint size = 0;

  do {
    ++size;
  } while ((value >>= 7) != 0);
  return size;
}

static gboolean
_av1_encode_uleb (guint64 value, guint available,
    guint8 * coded_value, guint coded_size)
{
  const guint kMaximumLeb128Size = 8;
  const guint leb_size = _av1_uleb_size_in_bytes (value);
  const guint64 kMaximumLeb128Value = 0xffffffff /* 4294967295U */ ;
  guint i;

  g_assert (coded_size >= leb_size);

  if (value > kMaximumLeb128Value || coded_size > kMaximumLeb128Size ||
      coded_size > available || !coded_value) {
    return FALSE;
  }

  if (leb_size == coded_size) {
    for (i = 0; i < leb_size; i++) {
      guint8 byte = value & 0x7f;
      value >>= 7;

      if (value != 0)
        byte |= 0x80;           // Signal that more bytes follow.

      *(coded_value + i) = byte;
    }
  } else {
    memset (coded_value, 0, coded_size);

    i = 0;
    do {
      *(coded_value + i) = value & 0x7fU;
      value >>= 7;
      i++;
    } while (value);

    for (i = 0; i < coded_size - 1; i++)
      *(coded_value + i) |= 0x80;
  }

  return TRUE;
}

/* 4.10.3
 *
 * Variable length unsigned n-bit number appearing directly in the
 * bitstream */
static gboolean
_av1_write_uvlc (GstBitWriter * bw, guint32 value, gboolean * space)
{
  gboolean have_space = TRUE;
  gint64 shift_val = value;
  gint32 leading_zeroes = 1;

  value++;

  g_assert (shift_val > 0);

  while (shift_val >>= 1)
    leading_zeroes += 2;

  WRITE_BITS (bw, 0, leading_zeroes >> 1);
  WRITE_BITS (bw, value, (leading_zeroes + 1) >> 1);

  *space = TRUE;
  return TRUE;

error:
  *space = have_space;
  return FALSE;
}

static gint
_av1_helper_msb (guint n)
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

static gboolean
_av1_write_uniform (GstBitWriter * bw, guint32 max_value, guint32 value,
    gboolean * space)
{
  gboolean have_space = TRUE;
  const gint l = max_value ? _av1_helper_msb (max_value) + 1 : 0;
  const gint m = (1 << l) - max_value;

  if (l == 0)
    goto success;

  if (value < m) {
    WRITE_BITS (bw, value, l - 1);
  } else {
    WRITE_BITS (bw, m + ((value - m) >> 1), l - 1);
    WRITE_BITS (bw, (value - m) & 1, 1);
  }

success:
  *space = TRUE;
  return TRUE;

error:
  *space = have_space;
  return FALSE;
}

/* 5.9.13
 *
 * Delta quantizer */
static gboolean
_av1_write_delta_q (GstBitWriter * bw, gint32 delta_q, gboolean * space)
{
  gboolean have_space = TRUE;

  if (delta_q != 0) {
    WRITE_BITS (bw, 1, 1);
    WRITE_BITS (bw, delta_q, 7);
  } else {
    WRITE_BITS (bw, 0, 1);
  }

  *space = TRUE;
  return TRUE;

error:
  *space = have_space;
  return FALSE;
}

/* 4.10.6
 *
 * su(n) */
static gboolean
_av1_write_su (GstBitWriter * bw, gint32 val, guint n, gboolean * space)
{
  gboolean have_space = TRUE;

  g_assert (n < 31);
  WRITE_BITS (bw, val, n + 1);

  *space = TRUE;
  return TRUE;

error:
  *space = have_space;
  return FALSE;
}

/* 5.9.16 Tile size calculation
 *
 * returns the smallest value for k such that blkSize << k is greater
 * than or equal to target */
static gint
_av1_helper_tile_log2 (gint blkSize, gint target)
{
  gint k;

  for (k = 0; (blkSize << k) < target; k++);

  return k;
}

static gboolean
_av1_write_primitive_quniform (GstBitWriter * bw, guint16 n, guint16 v,
    gboolean * space)
{
  gboolean have_space = TRUE;
  const gint l = _av1_helper_msb (n) + 1;
  const gint m = (1 << l) - n;

  if (n <= 1)
    goto success;

  if (v < m) {
    WRITE_BITS (bw, v, l - 1);
  } else {
    WRITE_BITS (bw, m + ((v - m) >> 1), l - 1);
    WRITE_BITS (bw, (v - m) & 1, 1);
  }

success:
  *space = TRUE;
  return TRUE;

error:
  *space = have_space;
  return FALSE;
}

static gboolean
_av1_write_primitive_subexpfin (GstBitWriter * bw, guint16 n, guint16 k,
    guint16 v, gboolean * space)
{
  gboolean have_space = TRUE;
  gint i = 0;
  gint mk = 0;

  while (1) {
    gint b = (i ? k + i - 1 : k);
    gint a = (1 << b);

    if (n <= mk + 3 * a) {
      if (!_av1_write_primitive_quniform (bw, n - mk, v - mk, space))
        goto error;

      break;
    } else {
      gint t = (v >= mk + a);
      WRITE_BITS (bw, t, 1);
      if (t) {
        i = i + 1;
        mk += a;
      } else {
        WRITE_BITS (bw, v - mk, b);
        break;
      }
    }
  }

  *space = TRUE;
  return TRUE;

error:
  *space = have_space;
  return FALSE;
}

/* Recenters a non-negative literal v around a reference r */
static guint16
_av1_helper_recenter_nonneg (guint16 r, guint16 v)
{
  if (v > (r << 1))
    return v;
  else if (v >= r)
    return ((v - r) << 1);
  else
    return ((r - v) << 1) - 1;
}

/* Recenters a non-negative literal v in [0, n-1] around
   a reference r also in [0, n-1] */
static guint16
_av1_helper_recenter_finite_nonneg (guint16 n, guint16 r, guint16 v)
{
  if ((r << 1) <= n) {
    return _av1_helper_recenter_nonneg (r, v);
  } else {
    return _av1_helper_recenter_nonneg (n - 1 - r, n - 1 - v);
  }
}

static gboolean
_av1_write_primitive_refsubexpfin (GstBitWriter * bw,
    guint16 n, guint16 k, guint16 ref, guint16 v, gboolean * space)
{
  gboolean have_space = TRUE;

  if (!_av1_write_primitive_subexpfin (bw, n, k,
          _av1_helper_recenter_finite_nonneg (n, ref, v), space))
    goto error;

  *space = TRUE;
  return TRUE;

error:
  *space = have_space;
  return FALSE;
}

static gboolean
_av1_write_signed_primitive_refsubexpfin (GstBitWriter * bw,
    guint16 n, guint16 k, gint16 ref, gint16 v, gboolean * space)
{
  gboolean have_space = TRUE;
  const guint16 scaled_n = (n << 1) - 1;
  ref += n - 1;
  v += n - 1;

  if (!_av1_write_primitive_refsubexpfin (bw, scaled_n, k, ref, v, space))
    goto error;

  *space = TRUE;
  return TRUE;

error:
  *space = have_space;
  return FALSE;
}

static gboolean
_av1_seq_level_idx_is_valid (GstAV1SeqLevels seq_level_idx)
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

static gboolean
_av1_bit_writer_timing_info (const GstAV1TimingInfo * timing_info,
    GstBitWriter * bw, gboolean * space)
{
  gboolean have_space = TRUE;

  GST_DEBUG ("writing timing info");

  if (timing_info->num_units_in_display_tick == 0 ||
      timing_info->time_scale == 0)
    goto error;

  WRITE_BITS (bw, timing_info->num_units_in_display_tick, 32);
  WRITE_BITS (bw, timing_info->time_scale, 32);

  WRITE_BITS (bw, timing_info->equal_picture_interval, 1);
  if (timing_info->equal_picture_interval) {
    if (timing_info->num_ticks_per_picture_minus_1 == G_MAXUINT)
      goto error;

    if (!_av1_write_uvlc (bw, timing_info->num_ticks_per_picture_minus_1,
            &have_space))
      goto error;
  }

  *space = TRUE;
  return TRUE;

error:
  GST_WARNING ("failed to write timing info");
  *space = have_space;
  return FALSE;
}

/* 5.5.4 */
static gboolean
_av1_bit_writer_decoder_model_info (const GstAV1DecoderModelInfo *
    decoder_model_info, GstBitWriter * bw, gboolean * space)
{
  gboolean have_space = TRUE;

  GST_DEBUG ("writing decoder model info");

  WRITE_BITS (bw, decoder_model_info->buffer_delay_length_minus_1, 5);
  WRITE_BITS (bw, decoder_model_info->num_units_in_decoding_tick, 32);
  WRITE_BITS (bw, decoder_model_info->buffer_removal_time_length_minus_1, 5);
  WRITE_BITS (bw, decoder_model_info->frame_presentation_time_length_minus_1,
      5);

  *space = TRUE;
  return TRUE;
error:
  GST_WARNING ("failed to write decoder model info");
  *space = have_space;
  return FALSE;
}

/* 5.5.5 */
static gboolean
_av1_bit_writer_operating_parameters_info (const GstAV1SequenceHeaderOBU *
    seq_header, const GstAV1OperatingPoint * op_point, GstBitWriter * bw,
    gboolean * space)
{
  gboolean have_space = TRUE;
  guint32 n = seq_header->decoder_model_info.buffer_delay_length_minus_1 + 1;

  GST_DEBUG ("writing operating parameters info");

  if (n > 32)
    goto error;

  WRITE_BITS (bw, op_point->decoder_buffer_delay, n);
  WRITE_BITS (bw, op_point->encoder_buffer_delay, n);
  WRITE_BITS (bw, op_point->low_delay_mode_flag, 1);

  *space = TRUE;
  return TRUE;

error:
  GST_WARNING ("failed to write operating parameters info");
  *space = have_space;
  return FALSE;
}

/* 5.5.2 */
static gboolean
_av1_bit_writer_color_config (const GstAV1SequenceHeaderOBU * seq_header,
    const GstAV1ColorConfig * color_config, GstBitWriter * bw, gboolean * space)
{
  gboolean have_space = TRUE;

  GST_DEBUG ("writing color config");

  WRITE_BITS (bw, color_config->high_bitdepth, 1);

  if (seq_header->seq_profile == GST_AV1_PROFILE_2
      && color_config->high_bitdepth) {
    WRITE_BITS (bw, color_config->twelve_bit, 1);
  } else if (seq_header->seq_profile > GST_AV1_PROFILE_2) {
    GST_WARNING ("Unsupported profile/bit-depth combination");
    goto error;
  }

  if (seq_header->seq_profile != GST_AV1_PROFILE_1)
    WRITE_BITS (bw, color_config->mono_chrome, 1);

  if (seq_header->num_planes != 1 && seq_header->num_planes != 3) {
    GST_WARNING ("num_planes is not correct");
    goto error;
  }
  if (!color_config->mono_chrome && seq_header->num_planes != 3) {
    GST_WARNING ("num_planes is not correct");
    goto error;
  }

  WRITE_BITS (bw, color_config->color_description_present_flag, 1);

  if (color_config->color_description_present_flag) {
    WRITE_BITS (bw, color_config->color_primaries, 8);
    WRITE_BITS (bw, color_config->transfer_characteristics, 8);
    WRITE_BITS (bw, color_config->matrix_coefficients, 8);
  }

  if (color_config->mono_chrome) {
    if (color_config->subsampling_x != 1 || color_config->subsampling_y != 1) {
      GST_WARNING ("set subsampling_x or subsampling_y wrong value");
      goto error;
    }

    WRITE_BITS (bw, color_config->color_range, 1);
    goto success;
  } else if (color_config->color_primaries == GST_AV1_CP_BT_709 &&
      color_config->transfer_characteristics == GST_AV1_TC_SRGB &&
      color_config->matrix_coefficients == GST_AV1_MC_IDENTITY) {

    if (color_config->color_range != 1) {
      GST_WARNING ("set color_range wrong value");
      goto error;
    }

    if (color_config->subsampling_x != 0 || color_config->subsampling_y != 0) {
      GST_WARNING ("set subsampling_x or subsampling_y wrong value");
      goto error;
    }

    if (!(seq_header->seq_profile == GST_AV1_PROFILE_1 ||
            (seq_header->seq_profile == GST_AV1_PROFILE_2
                && seq_header->bit_depth == 12))) {
      GST_WARNING ("sRGB colorspace not compatible with specified profile");
      goto error;
    }
  } else {
    WRITE_BITS (bw, color_config->color_range, 1);

    if (seq_header->seq_profile == GST_AV1_PROFILE_0) {
      if (color_config->subsampling_x != 1 || color_config->subsampling_y != 1) {
        GST_WARNING ("set subsampling_x or subsampling_y wrong value");
        goto error;
      }
    } else if (seq_header->seq_profile == GST_AV1_PROFILE_1) {
      if (color_config->subsampling_x != 0 || color_config->subsampling_y != 0) {
        GST_WARNING ("set subsampling_x or subsampling_y wrong value");
        goto error;
      }
    } else if (seq_header->seq_profile == GST_AV1_PROFILE_2) {
      if (seq_header->bit_depth == 12) {
        WRITE_BITS (bw, color_config->subsampling_x, 1);

        if (color_config->subsampling_x) {
          /* 422 or 420 */
          WRITE_BITS (bw, color_config->subsampling_y, 1);
        }
      }
    }

    if (color_config->subsampling_x && color_config->subsampling_y)
      WRITE_BITS (bw, color_config->chroma_sample_position, 2);
  }

  WRITE_BITS (bw, color_config->separate_uv_delta_q, 1);

  if (!(color_config->subsampling_x == 0 && color_config->subsampling_y == 0) &&
      !(color_config->subsampling_x == 1 && color_config->subsampling_y == 1) &&
      !(color_config->subsampling_x == 1 && color_config->subsampling_y == 0)) {
    GST_WARNING ("Only 4:4:4, 4:2:2 and 4:2:0 are currently supported, "
        "%d %d subsampling is not supported.\n",
        color_config->subsampling_x, color_config->subsampling_y);
    goto error;
  }

success:
  *space = TRUE;
  return TRUE;

error:
  GST_WARNING ("failed to write color config");
  *space = have_space;
  return FALSE;
}

static gboolean
_av1_bit_writer_sequence_header (const GstAV1SequenceHeaderOBU * seq_header,
    GstBitWriter * bw, gboolean * space)
{
  gboolean have_space = TRUE;
  gint i;

  GST_DEBUG ("writing sequence header");

  if (seq_header->seq_profile > GST_AV1_PROFILE_2) {
    GST_WARNING ("Unsupported profile %d", seq_header->seq_profile);
    goto error;
  }
  WRITE_BITS (bw, seq_header->seq_profile, 3);

  WRITE_BITS (bw, seq_header->still_picture, 1);

  if (!seq_header->still_picture && seq_header->reduced_still_picture_header) {
    GST_WARNING (" If reduced_still_picture_header is equal to 1, it is a"
        " requirement of bitstream conformance that still_picture is equal"
        " to 1. ");
    goto error;
  }
  WRITE_BITS (bw, seq_header->reduced_still_picture_header, 1);

  if (seq_header->reduced_still_picture_header) {
    if (!_av1_seq_level_idx_is_valid
        (seq_header->operating_points[0].seq_level_idx)) {
      GST_WARNING ("The seq_level_idx is unsupported");
      goto error;
    }
    WRITE_BITS (bw, seq_header->operating_points[0].seq_level_idx, 5);
  } else {
    WRITE_BITS (bw, seq_header->timing_info_present_flag, 1);
    if (seq_header->timing_info_present_flag) {
      if (!_av1_bit_writer_timing_info (&seq_header->timing_info, bw,
              &have_space))
        goto error;

      WRITE_BITS (bw, seq_header->decoder_model_info_present_flag, 1);
      if (seq_header->decoder_model_info_present_flag) {
        if (!_av1_bit_writer_decoder_model_info
            (&seq_header->decoder_model_info, bw, &have_space))
          goto error;
      }
    }

    WRITE_BITS (bw, seq_header->initial_display_delay_present_flag, 1);

    if (seq_header->operating_points_cnt_minus_1 + 1 >
        GST_AV1_MAX_OPERATING_POINTS) {
      GST_WARNING ("The operating points number %d is too big",
          seq_header->operating_points_cnt_minus_1 + 1);
      goto error;
    }
    WRITE_BITS (bw, seq_header->operating_points_cnt_minus_1, 5);

    for (i = 0; i < seq_header->operating_points_cnt_minus_1 + 1; i++) {
      const GstAV1OperatingPoint *op = &seq_header->operating_points[i];

      WRITE_BITS (bw, op->idc, 12);

      if (!_av1_seq_level_idx_is_valid (op->seq_level_idx)) {
        GST_WARNING ("The seq_level_idx is unsupported");
        goto error;
      }
      WRITE_BITS (bw, op->seq_level_idx, 5);

      if (op->seq_level_idx > GST_AV1_SEQ_LEVEL_3_3)
        WRITE_BITS (bw, op->seq_tier, 1);

      if (seq_header->decoder_model_info_present_flag) {
        WRITE_BITS (bw, op->decoder_model_present_for_this_op, 1);
        if (op->decoder_model_present_for_this_op) {
          if (!_av1_bit_writer_operating_parameters_info (seq_header,
                  op, bw, &have_space))
            goto error;
        }
      }

      if (seq_header->initial_display_delay_present_flag) {
        WRITE_BITS (bw, op->initial_display_delay_present_for_this_op, 1);

        if (op->initial_display_delay_present_for_this_op) {
          if (op->initial_display_delay_minus_1 + 1 > 10) {
            GST_INFO ("AV1 does not support more than 10 decoded frames delay");
            goto error;
          }

          WRITE_BITS (bw, op->initial_display_delay_minus_1, 4);
        }
      }
    }
  }

  WRITE_BITS (bw, seq_header->frame_width_bits_minus_1, 4);
  WRITE_BITS (bw, seq_header->frame_height_bits_minus_1, 4);
  WRITE_BITS (bw, seq_header->max_frame_width_minus_1,
      seq_header->frame_width_bits_minus_1 + 1);
  WRITE_BITS (bw, seq_header->max_frame_height_minus_1,
      seq_header->frame_height_bits_minus_1 + 1);

  if (!seq_header->reduced_still_picture_header)
    WRITE_BITS (bw, seq_header->frame_id_numbers_present_flag, 1);

  if (seq_header->frame_id_numbers_present_flag) {
    if (seq_header->additional_frame_id_length_minus_1 + 1 +
        seq_header->delta_frame_id_length_minus_2 + 2 > 16) {
      GST_WARNING ("Invalid frame_id_length");
      goto error;
    }
    WRITE_BITS (bw, seq_header->delta_frame_id_length_minus_2, 4);
    WRITE_BITS (bw, seq_header->additional_frame_id_length_minus_1, 3);
  }

  WRITE_BITS (bw, seq_header->use_128x128_superblock, 1);
  WRITE_BITS (bw, seq_header->enable_filter_intra, 1);
  WRITE_BITS (bw, seq_header->enable_intra_edge_filter, 1);

  if (!seq_header->reduced_still_picture_header) {
    WRITE_BITS (bw, seq_header->enable_interintra_compound, 1);
    WRITE_BITS (bw, seq_header->enable_masked_compound, 1);
    WRITE_BITS (bw, seq_header->enable_warped_motion, 1);
    WRITE_BITS (bw, seq_header->enable_dual_filter, 1);
    WRITE_BITS (bw, seq_header->enable_order_hint, 1);
    if (seq_header->enable_order_hint) {
      WRITE_BITS (bw, seq_header->enable_jnt_comp, 1);
      WRITE_BITS (bw, seq_header->enable_ref_frame_mvs, 1);
    }

    WRITE_BITS (bw, seq_header->seq_choose_screen_content_tools, 1);
    if (!seq_header->seq_choose_screen_content_tools)
      WRITE_BITS (bw, seq_header->seq_force_screen_content_tools, 1);

    if (seq_header->seq_force_screen_content_tools > 0) {
      WRITE_BITS (bw, seq_header->seq_choose_integer_mv, 1);
      if (!seq_header->seq_choose_integer_mv)
        WRITE_BITS (bw, seq_header->seq_force_integer_mv, 1);
    }

    if (seq_header->enable_order_hint)
      WRITE_BITS (bw, seq_header->order_hint_bits_minus_1, 3);
  }

  WRITE_BITS (bw, seq_header->enable_superres, 1);
  WRITE_BITS (bw, seq_header->enable_cdef, 1);
  WRITE_BITS (bw, seq_header->enable_restoration, 1);

  if (!_av1_bit_writer_color_config (seq_header, &seq_header->color_config,
          bw, &have_space))
    goto error;

  WRITE_BITS (bw, seq_header->film_grain_params_present, 1);

  *space = TRUE;
  return TRUE;

error:
  GST_WARNING ("failed to write sequence header");
  *space = have_space;
  return FALSE;
}

/* Return the actual size field size. */
static guint
_av1_bit_writer_add_size_field (guint8 * data, guint * size, guint header_size,
    guint payload_size, guint size_field_size, gboolean * space)
{
  guint size_field_sz;

  size_field_sz = _av1_uleb_size_in_bytes (payload_size);
  g_assert (size_field_sz > 0);

  if (size_field_size > 0) {
    if (size_field_sz > size_field_size) {
      GST_WARNING ("the fixed size field size is too small");
      goto error;
    }

    size_field_sz = size_field_size;
  }

  /* Move and write the data size field */
  if (header_size + payload_size + size_field_sz > *size) {
    *space = FALSE;
    goto error;
  }
  *space = TRUE;

  memmove (data + header_size + size_field_sz,
      data + header_size, payload_size);

  if (!_av1_encode_uleb (payload_size, sizeof (payload_size),
          data + header_size, size_field_sz))
    goto error;

  *size = header_size + payload_size + size_field_sz;
  return size_field_sz;

error:
  GST_WARNING ("failed to write the size field");
  return 0;
}

/**
 * gst_av1_bit_writer_sequence_header_obu:
 * @seq_hdr: the sequence header of #GstAV1SequenceHeaderOBU to write
 * @size_field: whether the header contain size feild
 * @data: (out): the bit stream generated by the sequence header
 * @size: (inout): the size in bytes of the input and output
 *
 * Generating the according AV1 bit stream OBU by providing the sequence header.
 *
 * Returns: a #GstAV1BitWriterResult
 *
 * Since: 1.22
 **/
GstAV1BitWriterResult
gst_av1_bit_writer_sequence_header_obu (const GstAV1SequenceHeaderOBU *
    seq_hdr, gboolean size_field, guint8 * data, guint * size)
{
  gboolean have_space = TRUE;
  GstBitWriter bw;
  guint header_size;
  guint payload_size;

  g_return_val_if_fail (seq_hdr != NULL, GST_AV1_BIT_WRITER_ERROR);
  g_return_val_if_fail (data != NULL, GST_AV1_BIT_WRITER_ERROR);
  g_return_val_if_fail (size != NULL, GST_AV1_BIT_WRITER_ERROR);
  g_return_val_if_fail (*size > 0, GST_AV1_BIT_WRITER_ERROR);

  gst_bit_writer_init_with_data (&bw, data, *size, FALSE);

  /* obu_forbidden_bit */
  WRITE_BITS (&bw, 0, 1);
  /* obu_type */
  WRITE_BITS (&bw, GST_AV1_OBU_SEQUENCE_HEADER, 4);
  /* obu_extention_flag */
  WRITE_BITS (&bw, 0, 1);
  /* obu_has_size_field */
  WRITE_BITS (&bw, 1, 1);
  /* obu_reserved_1bit */
  WRITE_BITS (&bw, 0, 1);

  header_size = gst_bit_writer_get_size (&bw);
  g_assert (header_size % 8 == 0);
  header_size /= 8;

  if (!_av1_bit_writer_sequence_header (seq_hdr, &bw, &have_space))
    goto error;

  /* Add trailings. */
  WRITE_BITS (&bw, 1, 1);
  if (!gst_bit_writer_align_bytes (&bw, 0)) {
    have_space = FALSE;
    goto error;
  }

  payload_size = gst_bit_writer_get_size (&bw);
  g_assert (payload_size % 8 == 0);
  payload_size /= 8;
  payload_size -= header_size;

  gst_bit_writer_reset (&bw);

  if (size_field) {
    if (!_av1_bit_writer_add_size_field (data, size,
            header_size, payload_size, 0, &have_space))
      goto error;
  } else {
    *size = header_size + payload_size;
  }

  return GST_AV1_BIT_WRITER_OK;

error:
  gst_bit_writer_reset (&bw);
  *size = 0;
  return have_space ? GST_AV1_BIT_WRITER_INVALID_DATA :
      GST_AV1_BIT_WRITER_NO_MORE_SPACE;
}

/* 5.9.5 */
static gboolean
_av1_bit_writer_superres_params (const GstAV1FrameHeaderOBU * frame_header,
    const GstAV1SequenceHeaderOBU * seq_header, GstBitWriter * bw,
    gboolean * space)
{
  gboolean have_space = TRUE;

  GST_DEBUG ("writing superres param");

  if (seq_header->enable_superres)
    WRITE_BITS (bw, frame_header->use_superres, 1);

  if (frame_header->use_superres) {
    guint8 coded_denom;

    if (frame_header->superres_denom < GST_AV1_SUPERRES_DENOM_MIN)
      goto error;

    coded_denom = frame_header->superres_denom - GST_AV1_SUPERRES_DENOM_MIN;
    if (coded_denom > (1 << GST_AV1_SUPERRES_DENOM_BITS) - 1)
      goto error;

    WRITE_BITS (bw, coded_denom, GST_AV1_SUPERRES_DENOM_BITS);
  }

  *space = TRUE;
  return TRUE;

error:
  GST_WARNING ("failed to write superres param");
  *space = have_space;
  return FALSE;
}

/* 5.9.5 */
static gboolean
_av1_bit_writer_frame_size (const GstAV1FrameHeaderOBU * frame_header,
    const GstAV1SequenceHeaderOBU * seq_header, GstBitWriter * bw,
    gboolean * space)
{
  gboolean have_space = TRUE;

  GST_DEBUG ("writing frame size");

  if (frame_header->frame_size_override_flag) {
    guint16 frame_width_minus_1;
    guint16 frame_height_minus_1;

    frame_width_minus_1 = frame_header->frame_width - 1;
    WRITE_BITS (bw, frame_width_minus_1,
        seq_header->frame_width_bits_minus_1 + 1);
    frame_height_minus_1 = frame_header->frame_height - 1;
    WRITE_BITS (bw, frame_height_minus_1,
        seq_header->frame_height_bits_minus_1 + 1);
  }

  if (!_av1_bit_writer_superres_params (frame_header, seq_header, bw, space))
    goto error;

  *space = TRUE;
  return TRUE;

error:
  GST_WARNING ("failed to write frame size");
  *space = have_space;
  return FALSE;
}

/* 5.9.6 */
static gboolean
_av1_bit_writer_render_size (const GstAV1FrameHeaderOBU * frame_header,
    GstBitWriter * bw, gboolean * space)
{
  gboolean have_space = TRUE;

  GST_DEBUG ("writing render size");

  WRITE_BITS (bw, frame_header->render_and_frame_size_different, 1);

  if (frame_header->render_and_frame_size_different) {
    guint16 render_width_minus_1 = frame_header->render_width - 1;
    guint16 render_height_minus_1 = frame_header->render_height - 1;

    WRITE_BITS (bw, render_width_minus_1, 16);
    WRITE_BITS (bw, render_height_minus_1, 16);
  }

  *space = TRUE;
  return TRUE;

error:
  GST_WARNING ("failed to write render size");
  *space = have_space;
  return FALSE;
}

/* 5.9.15 */
static gboolean
_av1_bit_writer_tile_info (const GstAV1FrameHeaderOBU * frame_header,
    const GstAV1SequenceHeaderOBU * seq_header, GstBitWriter * bw,
    gboolean * space)
{
  gboolean have_space = TRUE;
  const GstAV1TileInfo *tile_info;
  guint32 mi_cols /* MiCols */ ;
  guint32 mi_rows /* MiRows */ ;
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
  gint size_sb /* sizeSb */ ;
  gint widest_tile_sb /* widestTileSb */ ;

  tile_info = &frame_header->tile_info;

  GST_DEBUG ("writing tile info");

  /* User must specify the frame resolution. */
  if (frame_header->frame_width == 0 || frame_header->frame_height == 0) {
    GST_WARNING ("unknown frame_width or frame_height");
    goto error;
  }

  mi_cols = 2 * ((frame_header->frame_width + 7) >> 3);
  mi_rows = 2 * ((frame_header->frame_height + 7) >> 3);

  sb_cols = seq_header->use_128x128_superblock ?
      ((mi_cols + 31) >> 5) : ((mi_cols + 15) >> 4);
  sb_rows = seq_header->use_128x128_superblock ?
      ((mi_rows + 31) >> 5) : ((mi_rows + 15) >> 4);
  sb_shift = seq_header->use_128x128_superblock ? 5 : 4;
  sb_size = sb_shift + 2;

  max_tile_width_sb = GST_AV1_MAX_TILE_WIDTH >> sb_size;
  max_tile_area_sb = GST_AV1_MAX_TILE_AREA >> (2 * sb_size);
  min_log2_tile_cols = _av1_helper_tile_log2 (max_tile_width_sb, sb_cols);
  max_log2_tile_cols = _av1_helper_tile_log2 (1, MIN (sb_cols,
          GST_AV1_MAX_TILE_COLS));
  max_log2_tile_rows = _av1_helper_tile_log2 (1, MIN (sb_rows,
          GST_AV1_MAX_TILE_ROWS));
  min_log2_tiles = MAX (min_log2_tile_cols,
      _av1_helper_tile_log2 (max_tile_area_sb, sb_rows * sb_cols));

  WRITE_BITS (bw, tile_info->uniform_tile_spacing_flag, 1);
  if (tile_info->uniform_tile_spacing_flag) {
    /* columns */
    gint ones = tile_info->tile_cols_log2 - min_log2_tile_cols;
    if (ones < 0)
      goto error;

    while (ones--)
      WRITE_BITS (bw, 1, 1);
    if (tile_info->tile_cols_log2 < max_log2_tile_cols)
      WRITE_BITS (bw, 0, 1);

    /* rows */
    min_log2_tile_rows = MAX (min_log2_tiles - tile_info->tile_cols_log2, 0);
    ones = tile_info->tile_rows_log2 - min_log2_tile_rows;
    if (ones < 0)
      goto error;

    while (ones--)
      WRITE_BITS (bw, 1, 1);
    if (tile_info->tile_rows_log2 < max_log2_tile_rows)
      WRITE_BITS (bw, 0, 1);
  } else {
    /* Explicit tiles with configurable tile widths and heights */
    guint i;
    guint width_sb;
    guint height_sb;

    widest_tile_sb = 0;

    /* columns */
    width_sb = sb_cols;
    for (i = 0; i < tile_info->tile_cols; i++) {
      size_sb = tile_info->mi_col_starts[i + 1] - tile_info->mi_col_starts[i];
      size_sb = size_sb >> sb_shift;
      widest_tile_sb = MAX (size_sb, widest_tile_sb);
      if (_av1_write_uniform (bw, MIN (width_sb, max_tile_width_sb),
              size_sb - 1, &have_space))
        goto error;

      width_sb -= size_sb;
    }

    if (width_sb != 0)
      goto error;

    // rows
    if (min_log2_tiles > 0) {
      max_tile_area_sb = (sb_rows * sb_cols) >> (min_log2_tiles + 1);
    } else {
      max_tile_area_sb = sb_rows * sb_cols;
    }

    max_tile_height_sb = MAX (max_tile_area_sb / widest_tile_sb, 1);

    height_sb = sb_rows;
    for (i = 0; i < tile_info->tile_rows; i++) {
      size_sb = tile_info->mi_row_starts[i + 1] - tile_info->mi_row_starts[i];
      size_sb = size_sb >> sb_shift;
      if (_av1_write_uniform (bw, MIN (height_sb, max_tile_height_sb),
              size_sb - 1, &have_space))
        goto error;

      height_sb -= size_sb;
    }

    if (height_sb != 0)
      goto error;
  }

  if (tile_info->tile_cols_log2 > 0 || tile_info->tile_rows_log2 > 0) {
    WRITE_BITS (bw, tile_info->context_update_tile_id,
        tile_info->tile_cols_log2 + tile_info->tile_rows_log2);

    WRITE_BITS (bw, tile_info->tile_size_bytes_minus_1, 2);
  }

  *space = TRUE;
  return TRUE;

error:
  GST_WARNING ("failed to write tile info");
  *space = have_space;
  return FALSE;
}

/* 5.9.12 */
static gboolean
_av1_bit_writer_quantization_params (const GstAV1FrameHeaderOBU * frame_header,
    const GstAV1SequenceHeaderOBU * seq_header, guint * qindex_offset,
    GstBitWriter * bw, gboolean * space)
{
  gboolean have_space = TRUE;
  const GstAV1QuantizationParams *quant_params =
      &(frame_header->quantization_params);

  GST_DEBUG ("writing quantization params");

  if (qindex_offset)
    *qindex_offset = gst_bit_writer_get_size (bw);

  WRITE_BITS (bw, quant_params->base_q_idx, 8);

  if (!_av1_write_delta_q (bw,
          frame_header->quantization_params.delta_q_y_dc, &have_space))
    goto error;

  if (seq_header->num_planes > 1) {
    if (seq_header->color_config.separate_uv_delta_q)
      WRITE_BITS (bw, quant_params->diff_uv_delta, 1);

    if (!_av1_write_delta_q (bw,
            frame_header->quantization_params.delta_q_u_dc, &have_space))
      goto error;

    if (!_av1_write_delta_q (bw,
            frame_header->quantization_params.delta_q_u_ac, &have_space))
      goto error;

    if (quant_params->diff_uv_delta) {
      if (!_av1_write_delta_q (bw,
              frame_header->quantization_params.delta_q_v_dc, &have_space))
        goto error;

      if (!_av1_write_delta_q (bw,
              frame_header->quantization_params.delta_q_v_ac, &have_space))
        goto error;
    }
  }

  WRITE_BITS (bw, quant_params->using_qmatrix, 1);
  if (quant_params->using_qmatrix) {
    WRITE_BITS (bw, quant_params->qm_y, 4);
    WRITE_BITS (bw, quant_params->qm_u, 4);

    if (seq_header->color_config.separate_uv_delta_q)
      WRITE_BITS (bw, quant_params->qm_v, 4);
  }

  *space = TRUE;
  return TRUE;

error:
  GST_WARNING ("failed to write quantization params");
  *space = have_space;
  return FALSE;
}

/* 5.9.14 */
static gboolean
_av1_bit_writer_segmentation_params (const GstAV1FrameHeaderOBU * frame_header,
    const GstAV1SequenceHeaderOBU * seq_header, guint * segmentation_offset,
    GstBitWriter * bw, gboolean * space)
{
  gboolean have_space = TRUE;

  GST_DEBUG ("writing segmentation params");

  if (segmentation_offset)
    *segmentation_offset = gst_bit_writer_get_size (bw);

  /* TODO: segmentation support. */
  if (frame_header->segmentation_params.segmentation_enabled) {
    GST_WARNING ("segmentation is not supported now");
    goto error;
  }

  WRITE_BITS (bw, frame_header->segmentation_params.segmentation_enabled, 1);

  *space = TRUE;
  return TRUE;

error:
  GST_WARNING ("failed to write segmentation params");
  *space = have_space;
  return FALSE;
}

/* 5.9.17 */
static gboolean
_av1_bit_writer_delta_q_params (const GstAV1FrameHeaderOBU * frame_header,
    const GstAV1SequenceHeaderOBU * seq_header, GstBitWriter * bw,
    gboolean * space)
{
  gboolean have_space = TRUE;

  GST_DEBUG ("writing delta q params");

  if (frame_header->quantization_params.base_q_idx > 0)
    WRITE_BITS (bw, frame_header->quantization_params.delta_q_present, 1);

  if (frame_header->quantization_params.delta_q_present)
    WRITE_BITS (bw, frame_header->quantization_params.delta_q_res, 2);

  *space = TRUE;
  return TRUE;

error:
  GST_WARNING ("failed to write delta q params");
  *space = have_space;
  return FALSE;
}

/* 5.9.18 */
static gboolean
_av1_bit_writer_delta_lf_params (const GstAV1FrameHeaderOBU * frame_header,
    const GstAV1SequenceHeaderOBU * seq_header, GstBitWriter * bw,
    gboolean * space)
{
  gboolean have_space = TRUE;

  GST_DEBUG ("writing delta lf params");

  if (frame_header->quantization_params.delta_q_present) {
    if (!frame_header->allow_intrabc)
      WRITE_BITS (bw, frame_header->loop_filter_params.delta_lf_present, 1);

    if (frame_header->loop_filter_params.delta_lf_present) {
      WRITE_BITS (bw, frame_header->loop_filter_params.delta_lf_res, 2);
      WRITE_BITS (bw, frame_header->loop_filter_params.delta_lf_multi, 1);
    }
  }

  *space = TRUE;
  return TRUE;

error:
  GST_WARNING ("failed to write delta lf params");
  *space = have_space;
  return FALSE;
}

static gboolean
_av1_bit_writer_loop_filter_params (const GstAV1FrameHeaderOBU * frame_header,
    const GstAV1SequenceHeaderOBU * seq_header, guint * loopfilter_offset,
    GstBitWriter * bw, gboolean * space)
{
  gboolean have_space = TRUE;
  const GstAV1LoopFilterParams *lf_params = &frame_header->loop_filter_params;

  GST_DEBUG ("writing loop filter params");

  if (frame_header->coded_lossless || frame_header->allow_intrabc)
    goto success;

  if (loopfilter_offset)
    *loopfilter_offset = gst_bit_writer_get_size (bw);

  WRITE_BITS (bw, lf_params->loop_filter_level[0], 6);
  WRITE_BITS (bw, lf_params->loop_filter_level[1], 6);
  if (seq_header->num_planes > 1) {
    if (lf_params->loop_filter_level[0] || lf_params->loop_filter_level[1]) {
      WRITE_BITS (bw, lf_params->loop_filter_level[2], 6);
      WRITE_BITS (bw, lf_params->loop_filter_level[3], 6);
    }
  }

  WRITE_BITS (bw, lf_params->loop_filter_sharpness, 3);

  WRITE_BITS (bw, lf_params->loop_filter_delta_enabled, 1);
  if (lf_params->loop_filter_delta_enabled) {
    guint i;

    WRITE_BITS (bw, lf_params->loop_filter_delta_update, 1);

    if (lf_params->loop_filter_delta_update) {
      const gint8 default_loop_filter_ref_deltas[] =
          { 1, 0, 0, 0, -1, 0, -1, -1 };
      gboolean update_ref_deltas;

      for (i = 0; i < GST_AV1_TOTAL_REFS_PER_FRAME; i++) {
        /* If loop_filter_ref_deltas[i] is different from default
           value, we update it. */
        update_ref_deltas = (lf_params->loop_filter_ref_deltas[i] !=
            default_loop_filter_ref_deltas[i]);

        WRITE_BITS (bw, update_ref_deltas, 1);

        if (update_ref_deltas) {
          if (!_av1_write_su (bw, lf_params->loop_filter_ref_deltas[i],
                  6, space))
            goto error;
        }
      }

      for (i = 0; i < 2; i++) {
        /* If loop_filter_mode_deltas[i] is different from default
           value, we update it. */
        update_ref_deltas = (lf_params->loop_filter_mode_deltas[i] != 0);

        WRITE_BITS (bw, update_ref_deltas, 1);

        if (update_ref_deltas) {
          if (!_av1_write_su (bw, lf_params->loop_filter_mode_deltas[i],
                  6, space))
            goto error;
        }
      }
    }
  }

success:
  *space = TRUE;
  return TRUE;

error:
  GST_WARNING ("failed to write loop filter params");
  *space = have_space;
  return FALSE;
}

static gboolean
_av1_bit_writer_cdef_params (const GstAV1FrameHeaderOBU * frame_header,
    const GstAV1SequenceHeaderOBU * seq_header, guint * cdef_offset,
    guint * cdef_size, GstBitWriter * bw, gboolean * space)
{
  gboolean have_space = TRUE;
  const GstAV1CDEFParams *cdef_params = &frame_header->cdef_params;
  guint cdef_start;
  guint i;

  GST_DEBUG ("writing cdef params");

  if (frame_header->coded_lossless || frame_header->allow_intrabc
      || !seq_header->enable_cdef)
    goto success;

  cdef_start = gst_bit_writer_get_size (bw);
  if (cdef_offset)
    *cdef_offset = cdef_start;

  WRITE_BITS (bw, cdef_params->cdef_damping - 3, 2);
  WRITE_BITS (bw, cdef_params->cdef_bits, 2);

  for (i = 0; i < (1 << cdef_params->cdef_bits); i++) {
    guint8 cdef_y_sec_strength;

    WRITE_BITS (bw, cdef_params->cdef_y_pri_strength[i], 4);

    cdef_y_sec_strength = cdef_params->cdef_y_sec_strength[i];

    if (cdef_y_sec_strength >= 4) {
      GST_WARNING ("cdef_y_sec_strength is not valid");
      goto error;
    }
    WRITE_BITS (bw, cdef_y_sec_strength, 2);

    if (seq_header->num_planes > 1) {
      guint8 cdef_uv_sec_strength;

      WRITE_BITS (bw, cdef_params->cdef_uv_pri_strength[i], 4);

      cdef_uv_sec_strength = cdef_params->cdef_uv_sec_strength[i];

      if (cdef_uv_sec_strength >= 4) {
        GST_WARNING ("cdef_uv_sec_strength is not valid");
        goto error;
      }
      WRITE_BITS (bw, cdef_uv_sec_strength, 2);
    }
  }

  if (cdef_size)
    *cdef_size = gst_bit_writer_get_size (bw) - cdef_start;

success:
  *space = TRUE;
  return TRUE;

error:
  GST_WARNING ("failed to write cdef params");
  *space = have_space;
  return FALSE;
}

static gboolean
_av1_bit_writer_loop_restoration_params (const GstAV1FrameHeaderOBU *
    frame_header, const GstAV1SequenceHeaderOBU * seq_header,
    GstBitWriter * bw, gboolean * space)
{
  gboolean have_space = TRUE;
  guint i, j;
  guint8 use_chroma_lr = 0 /* useChromaLr */ ;
  const GstAV1LoopRestorationParams *lr_params =
      &frame_header->loop_restoration_params;
  const GstAV1FrameRestorationType remap_lr_type /* Remap_Lr_Type */ [4] = {
    GST_AV1_FRAME_RESTORE_NONE,
    GST_AV1_FRAME_RESTORE_SWITCHABLE,
    GST_AV1_FRAME_RESTORE_WIENER, GST_AV1_FRAME_RESTORE_SGRPROJ
  };

  GST_DEBUG ("writing loop restoration params");

  if (frame_header->all_lossless || frame_header->allow_intrabc
      || !seq_header->enable_restoration)
    goto success;

  for (i = 0; i < seq_header->num_planes; i++) {
    for (j = 0; j < 4; j++) {
      if (lr_params->frame_restoration_type[i] == remap_lr_type[j])
        break;
    }
    if (j == 4)
      goto error;

    if (lr_params->frame_restoration_type[i] != GST_AV1_FRAME_RESTORE_NONE) {
      if (!lr_params->uses_lr) {
        GST_WARNING ("uses_lr set to wrong value");
        goto error;
      }

      if (i > 1)
        use_chroma_lr = 1;
    }

    WRITE_BITS (bw, j, 2);
  }

  if (lr_params->uses_lr) {
    if (lr_params->lr_unit_shift > 2)
      goto error;

    if (seq_header->use_128x128_superblock) {
      WRITE_BITS (bw, lr_params->lr_unit_shift - 1, 1);
    } else {
      WRITE_BITS (bw, 1, 1);

      if (lr_params->lr_unit_shift > 1)
        /* lr_unit_extra_shift */
        WRITE_BITS (bw, 1, 1);
    }

    if (seq_header->color_config.subsampling_x
        && seq_header->color_config.subsampling_y && use_chroma_lr)
      WRITE_BITS (bw, lr_params->lr_uv_shift, 1);
  }

success:
  *space = TRUE;
  return TRUE;

error:
  GST_WARNING ("failed to write loop restoration params");
  *space = have_space;
  return FALSE;
}

/* 5.9.22 */
static gboolean
_av1_bit_writer_skip_mode_params (const GstAV1FrameHeaderOBU * frame_header,
    const GstAV1SequenceHeaderOBU * seq_header, GstBitWriter * bw,
    gboolean * space)
{
  gboolean have_space = TRUE;

  GST_DEBUG ("writing skip mode params");

  /* if skipModeAllowed is true */
  if (frame_header->skip_mode_frame[0] > 0
      || frame_header->skip_mode_frame[1] > 0)
    WRITE_BITS (bw, frame_header->skip_mode_present, 1);

  *space = TRUE;
  return TRUE;

error:
  GST_WARNING ("failed to write skip mode params");
  *space = have_space;
  return FALSE;
}

/* 5.9.24 */
static gboolean
_av1_bit_writer_global_motion_params (const GstAV1FrameHeaderOBU * frame_header,
    const GstAV1SequenceHeaderOBU * seq_header, GstBitWriter * bw,
    gboolean * space)
{
  gboolean have_space = TRUE;
  gint ref, i;
  const GstAV1GlobalMotionParams *gm_params =
      &(frame_header->global_motion_params);
  gint32 prev_gm_params[GST_AV1_NUM_REF_FRAMES][6] /* PrevGmParams */ ;

  GST_DEBUG ("writing global motion params");

  if (frame_header->frame_is_intra)
    goto success;

  if (frame_header->primary_ref_frame != GST_AV1_PRIMARY_REF_NONE) {
    memcpy (prev_gm_params, &frame_header->ref_global_motion_params,
        sizeof (gint32) * GST_AV1_NUM_REF_FRAMES * 6);
  } else {
    for (ref = GST_AV1_REF_INTRA_FRAME; ref < GST_AV1_NUM_REF_FRAMES; ref++)
      for (i = 0; i < 6; i++)
        prev_gm_params[ref][i] =
            ((i % 3 == 2) ? 1 << GST_AV1_WARPEDMODEL_PREC_BITS : 0);
  }

  for (ref = GST_AV1_REF_LAST_FRAME; ref <= GST_AV1_REF_ALTREF_FRAME; ref++) {
    WRITE_BITS (bw, gm_params->gm_type[ref] != GST_AV1_WARP_MODEL_IDENTITY, 1);

    if (gm_params->gm_type[ref] != GST_AV1_WARP_MODEL_IDENTITY) {
      WRITE_BITS (bw, gm_params->gm_type[ref] == GST_AV1_WARP_MODEL_ROTZOOM, 1);

      if (gm_params->gm_type[ref] != GST_AV1_WARP_MODEL_ROTZOOM)
        WRITE_BITS (bw,
            gm_params->gm_type[ref] == GST_AV1_WARP_MODEL_TRANSLATION, 1);
    }

    if (gm_params->gm_type[ref] >= GST_AV1_WARP_MODEL_ROTZOOM) {
      if (!_av1_write_signed_primitive_refsubexpfin (bw,
              (1 << GST_AV1_GM_ABS_ALPHA_BITS) + 1, 3,
              (prev_gm_params[ref][2] >> (GST_AV1_WARPEDMODEL_PREC_BITS -
                      GST_AV1_GM_ALPHA_PREC_BITS)) -
              (1 << GST_AV1_GM_ALPHA_PREC_BITS),
              (gm_params->gm_params[ref][2] >> (GST_AV1_WARPEDMODEL_PREC_BITS -
                      GST_AV1_GM_ALPHA_PREC_BITS)) -
              (1 << GST_AV1_GM_ALPHA_PREC_BITS), space))
        goto error;

      if (!_av1_write_signed_primitive_refsubexpfin (bw,
              (1 << GST_AV1_GM_ABS_ALPHA_BITS) + 1, 3,
              (prev_gm_params[ref][3] >> (GST_AV1_WARPEDMODEL_PREC_BITS -
                      GST_AV1_GM_ALPHA_PREC_BITS)),
              (gm_params->gm_params[ref][3] >> (GST_AV1_WARPEDMODEL_PREC_BITS -
                      GST_AV1_GM_ALPHA_PREC_BITS)), space))
        goto error;
    }

    if (gm_params->gm_type[ref] >= GST_AV1_WARP_MODEL_AFFINE) {
      if (!_av1_write_signed_primitive_refsubexpfin (bw,
              (1 << GST_AV1_GM_ABS_ALPHA_BITS) + 1, 3,
              (prev_gm_params[ref][4] >> (GST_AV1_WARPEDMODEL_PREC_BITS -
                      GST_AV1_GM_ALPHA_PREC_BITS)),
              (gm_params->gm_params[ref][4] >> (GST_AV1_WARPEDMODEL_PREC_BITS -
                      GST_AV1_GM_ALPHA_PREC_BITS)), space))
        goto error;

      if (!_av1_write_signed_primitive_refsubexpfin (bw,
              (1 << GST_AV1_GM_ABS_ALPHA_BITS) + 1, 3,
              (prev_gm_params[ref][5] >> (GST_AV1_WARPEDMODEL_PREC_BITS -
                      GST_AV1_GM_ALPHA_PREC_BITS)) -
              (1 << GST_AV1_GM_ALPHA_PREC_BITS),
              (gm_params->gm_params[ref][5] >> (GST_AV1_WARPEDMODEL_PREC_BITS -
                      GST_AV1_GM_ALPHA_PREC_BITS)) -
              (1 << GST_AV1_GM_ALPHA_PREC_BITS), space))
        goto error;
    }

    if (gm_params->gm_type[ref] >= GST_AV1_WARP_MODEL_TRANSLATION) {
      const gint trans_bits =
          (gm_params->gm_type[ref] == GST_AV1_WARP_MODEL_TRANSLATION) ?
          GST_AV1_GM_ABS_TRANS_ONLY_BITS -
          !frame_header->allow_high_precision_mv : GST_AV1_GM_ABS_TRANS_BITS;
      const gint trans_prec_diff =
          (gm_params->gm_type[ref] == GST_AV1_WARP_MODEL_TRANSLATION) ?
          GST_AV1_WARPEDMODEL_PREC_BITS - 3 +
          !frame_header->allow_high_precision_mv :
          (GST_AV1_WARPEDMODEL_PREC_BITS - GST_AV1_GM_TRANS_PREC_BITS);

      if (!_av1_write_signed_primitive_refsubexpfin (bw, (1 << trans_bits) + 1,
              3, (prev_gm_params[ref][0] >> trans_prec_diff),
              (gm_params->gm_params[ref][0] >> trans_prec_diff), space))
        goto error;

      if (!_av1_write_signed_primitive_refsubexpfin (bw, (1 << trans_bits) + 1,
              3, (prev_gm_params[ref][1] >> trans_prec_diff),
              (gm_params->gm_params[ref][1] >> trans_prec_diff), space))
        goto error;
    }
  }

success:
  *space = TRUE;
  return TRUE;

error:
  GST_WARNING ("failed to write global motion params");
  *space = have_space;
  return FALSE;
}

/* 5.9.30 */
static gboolean
_av1_bit_writer_film_grain_params (const GstAV1FrameHeaderOBU * frame_header,
    const GstAV1SequenceHeaderOBU * seq_header, GstBitWriter * bw,
    gboolean * space)
{
  gboolean have_space = TRUE;
  guint i;
  gint num_pos_chroma /* numPosChroma */ , num_pos_luma /* numPosLuma */ ;
  const GstAV1FilmGrainParams *fg_params = &frame_header->film_grain_params;

  GST_DEBUG ("writing film grain params");

  fg_params = &frame_header->film_grain_params;
  if (!seq_header->film_grain_params_present || (!frame_header->show_frame
          && !frame_header->showable_frame))
    goto success;

  WRITE_BITS (bw, fg_params->apply_grain, 1);
  if (!fg_params->apply_grain)
    goto success;

  WRITE_BITS (bw, fg_params->grain_seed, 16);

  if (frame_header->frame_type == GST_AV1_INTER_FRAME)
    WRITE_BITS (bw, fg_params->update_grain, 1);

  if (!fg_params->update_grain) {
    WRITE_BITS (bw, fg_params->film_grain_params_ref_idx, 3);
    goto success;
  }

  WRITE_BITS (bw, fg_params->num_y_points, 4);

  for (i = 0; i < fg_params->num_y_points; i++) {
    WRITE_BITS (bw, fg_params->point_y_value[i], 8);
    WRITE_BITS (bw, fg_params->point_y_scaling[i], 8);
  }

  if (!seq_header->color_config.mono_chrome)
    WRITE_BITS (bw, fg_params->chroma_scaling_from_luma, 1);

  if (!(seq_header->color_config.mono_chrome
          || fg_params->chroma_scaling_from_luma
          || (seq_header->color_config.subsampling_x == 1
              && seq_header->color_config.subsampling_y == 1
              && fg_params->num_y_points == 0))) {
    WRITE_BITS (bw, fg_params->num_cb_points, 4);

    for (i = 0; i < fg_params->num_cb_points; i++) {
      WRITE_BITS (bw, fg_params->point_cb_value[i], 8);
      WRITE_BITS (bw, fg_params->point_cb_scaling[i], 8);
    }

    WRITE_BITS (bw, fg_params->num_cr_points, 4);
    for (i = 0; i < fg_params->num_cr_points; i++) {
      WRITE_BITS (bw, fg_params->point_cr_value[i], 8);
      WRITE_BITS (bw, fg_params->point_cr_scaling[i], 8);
    }
  }

  WRITE_BITS (bw, fg_params->grain_scaling_minus_8, 2);

  WRITE_BITS (bw, fg_params->ar_coeff_lag, 2);

  num_pos_luma = 2 * fg_params->ar_coeff_lag * (fg_params->ar_coeff_lag + 1);
  if (fg_params->num_y_points) {
    num_pos_chroma = num_pos_luma + 1;
    for (i = 0; i < num_pos_luma; i++)
      WRITE_BITS (bw, fg_params->ar_coeffs_y_plus_128[i], 8);
  } else {
    num_pos_chroma = num_pos_luma;
  }

  if (fg_params->chroma_scaling_from_luma || fg_params->num_cb_points) {
    for (i = 0; i < num_pos_chroma; i++)
      WRITE_BITS (bw, fg_params->ar_coeffs_cb_plus_128[i], 8);
  }

  if (fg_params->chroma_scaling_from_luma || fg_params->num_cr_points) {
    for (i = 0; i < num_pos_chroma; i++)
      WRITE_BITS (bw, fg_params->ar_coeffs_cr_plus_128[i], 8);
  }

  WRITE_BITS (bw, fg_params->ar_coeff_shift_minus_6, 2);

  WRITE_BITS (bw, fg_params->grain_scale_shift, 2);

  if (fg_params->num_cb_points) {
    WRITE_BITS (bw, fg_params->cb_mult, 8);
    WRITE_BITS (bw, fg_params->cb_luma_mult, 8);
    WRITE_BITS (bw, fg_params->cb_offset, 9);
  }

  if (fg_params->num_cr_points) {
    WRITE_BITS (bw, fg_params->cr_mult, 8);
    WRITE_BITS (bw, fg_params->cr_luma_mult, 8);
    WRITE_BITS (bw, fg_params->cr_offset, 9);
  }

  WRITE_BITS (bw, fg_params->overlap_flag, 1);
  WRITE_BITS (bw, fg_params->clip_to_restricted_range, 1);

success:
  *space = TRUE;
  return TRUE;

error:
  GST_WARNING ("failed to write film grain params");
  *space = have_space;
  return FALSE;
}

static gboolean
_av1_bit_writer_uncompressed_frame_header (const GstAV1FrameHeaderOBU *
    frame_header, const GstAV1SequenceHeaderOBU * seq_header,
    guint8 temporal_id, guint8 spatial_id, guint * qindex_offset,
    guint * segmentation_offset, guint * loopfilter_offset,
    guint * cdef_offset, guint * cdef_size, GstBitWriter * bw, gboolean * space)
{
  gboolean have_space = TRUE;
  guint i;
  gint id_len /* idLen */  = 0;
  const GstAV1DecoderModelInfo *cmi = &seq_header->decoder_model_info;

  GST_DEBUG ("writing frame header");

  if (seq_header->frame_id_numbers_present_flag)
    id_len = seq_header->additional_frame_id_length_minus_1 + 1 +
        seq_header->delta_frame_id_length_minus_2 + 2;

  if (!seq_header->reduced_still_picture_header) {
    WRITE_BITS (bw, frame_header->show_existing_frame, 1);
    if (frame_header->show_existing_frame) {
      WRITE_BITS (bw, frame_header->frame_to_show_map_idx, 3);

      if (seq_header->decoder_model_info_present_flag
          && !seq_header->timing_info.equal_picture_interval)
        WRITE_BITS (bw, frame_header->frame_presentation_time,
            cmi->frame_presentation_time_length_minus_1 + 1);

      if (seq_header->frame_id_numbers_present_flag) {
        if (id_len <= 0)
          goto error;

        WRITE_BITS (bw, frame_header->display_frame_id, id_len);
      }

      goto success;
    }

    if (seq_header->still_picture &&
        (frame_header->frame_type != GST_AV1_KEY_FRAME
            || !frame_header->show_frame)) {
      GST_INFO ("Still pictures must be coded as shown keyframes");
      goto error;
    }
    WRITE_BITS (bw, frame_header->frame_type, 2);
    WRITE_BITS (bw, frame_header->show_frame, 1);

    if (frame_header->show_frame
        && seq_header->decoder_model_info_present_flag
        && !seq_header->timing_info.equal_picture_interval)
      WRITE_BITS (bw, frame_header->frame_presentation_time,
          cmi->frame_presentation_time_length_minus_1 + 1);

    if (!frame_header->show_frame)
      WRITE_BITS (bw, frame_header->showable_frame, 1);

    if (!(frame_header->frame_type == GST_AV1_SWITCH_FRAME
            || (frame_header->frame_type == GST_AV1_KEY_FRAME
                && frame_header->show_frame)))
      WRITE_BITS (bw, frame_header->error_resilient_mode, 1);
  }

  WRITE_BITS (bw, frame_header->disable_cdf_update, 1);

  if (seq_header->seq_force_screen_content_tools ==
      GST_AV1_SELECT_SCREEN_CONTENT_TOOLS)
    WRITE_BITS (bw, frame_header->allow_screen_content_tools, 1);

  if (frame_header->allow_screen_content_tools) {
    if (seq_header->seq_force_integer_mv == GST_AV1_SELECT_INTEGER_MV)
      WRITE_BITS (bw, frame_header->force_integer_mv, 1);
  }

  if (seq_header->frame_id_numbers_present_flag) {
    if (id_len <= 0)
      goto error;

    WRITE_BITS (bw, frame_header->current_frame_id, id_len);
  }

  if (frame_header->frame_type != GST_AV1_SWITCH_FRAME
      && !seq_header->reduced_still_picture_header)
    WRITE_BITS (bw, frame_header->frame_size_override_flag, 1);

  WRITE_BITS (bw, frame_header->order_hint,
      seq_header->order_hint_bits_minus_1 + 1);

  if (frame_header->frame_is_intra || frame_header->error_resilient_mode) {
    if (frame_header->primary_ref_frame != GST_AV1_PRIMARY_REF_NONE) {
      GST_WARNING ("primary_ref_frame is not none.");
      goto error;
    }
  } else {
    WRITE_BITS (bw, frame_header->primary_ref_frame, 3);
  }

  if (seq_header->decoder_model_info_present_flag) {
    if (frame_header->buffer_removal_time_present_flag) {
      guint op_num;
      const GstAV1OperatingPoint *operating_points;

      for (op_num = 0; op_num <= seq_header->operating_points_cnt_minus_1;
          op_num++) {
        operating_points = &seq_header->operating_points[op_num];

        if (operating_points->decoder_model_present_for_this_op) {
          gint op_pt_idc = operating_points->idc;
          gint in_temporal_layer = (op_pt_idc >> temporal_id) & 1;
          gint in_spatial_layer = (op_pt_idc >> (spatial_id + 8)) & 1;

          if (op_pt_idc == 0 || (in_temporal_layer && in_spatial_layer))
            WRITE_BITS (bw, frame_header->buffer_removal_time[op_num],
                cmi->buffer_removal_time_length_minus_1 + 1);
        }
      }
    }
  }

  if (frame_header->frame_type == GST_AV1_INTRA_ONLY_FRAME) {
    if (frame_header->refresh_frame_flags == 0xFF) {
      GST_INFO ("Intra only frames cannot have refresh flags 0xFF");
      goto error;
    }
  }
  if (!(frame_header->frame_type == GST_AV1_SWITCH_FRAME ||
          (frame_header->frame_type == GST_AV1_KEY_FRAME
              && frame_header->show_frame)))
    WRITE_BITS (bw, frame_header->refresh_frame_flags, 8);

  if (!frame_header->frame_is_intra
      || frame_header->refresh_frame_flags !=
      (1 << GST_AV1_NUM_REF_FRAMES) - 1) {
    if (frame_header->error_resilient_mode && seq_header->enable_order_hint) {
      for (i = 0; i < GST_AV1_NUM_REF_FRAMES; i++)
        WRITE_BITS (bw, frame_header->ref_order_hint[i],
            seq_header->order_hint_bits_minus_1 + 1);
    }
  }

  if (frame_header->frame_is_intra) {
    if (!_av1_bit_writer_frame_size (frame_header, seq_header, bw, space))
      goto error;

    if (!_av1_bit_writer_render_size (frame_header, bw, space))
      goto error;

    if (frame_header->allow_screen_content_tools
        && frame_header->upscaled_width == frame_header->frame_width)
      WRITE_BITS (bw, frame_header->allow_intrabc, 1);
  } else {
    if (seq_header->enable_order_hint) {
      WRITE_BITS (bw, frame_header->frame_refs_short_signaling, 1);

      if (frame_header->frame_refs_short_signaling) {
        WRITE_BITS (bw, frame_header->last_frame_idx, 3);
        WRITE_BITS (bw, frame_header->gold_frame_idx, 3);
      }
    }

    for (i = 0; i < GST_AV1_REFS_PER_FRAME; i++) {
      if (!frame_header->frame_refs_short_signaling)
        WRITE_BITS (bw, frame_header->ref_frame_idx[i], 3);

      if (seq_header->frame_id_numbers_present_flag) {
        gint32 delta_frame_id /* DeltaFrameId */ ;

        if (id_len <= 0)
          goto error;

        delta_frame_id =
            frame_header->current_frame_id - frame_header->expected_frame_id[i];
        delta_frame_id = delta_frame_id + (1 << id_len);
        delta_frame_id = delta_frame_id % (1 << id_len);
        WRITE_BITS (bw, delta_frame_id - 1,
            seq_header->delta_frame_id_length_minus_2 + 2);
      }
    }

    if (frame_header->frame_size_override_flag
        && !frame_header->error_resilient_mode) {
      /* 5.9.7 */
      /* TODO: reuse reference frame width/height. Just disable now. */
      for (i = 0; i < GST_AV1_REFS_PER_FRAME; i++)
        WRITE_BITS (bw, 0, 1);
    }

    if (!_av1_bit_writer_frame_size (frame_header, seq_header, bw, space))
      goto error;

    if (!_av1_bit_writer_render_size (frame_header, bw, space))
      goto error;

    if (!frame_header->force_integer_mv)
      WRITE_BITS (bw, frame_header->allow_high_precision_mv, 1);

    WRITE_BITS (bw, frame_header->is_filter_switchable, 1);
    if (!frame_header->is_filter_switchable)
      WRITE_BITS (bw, frame_header->interpolation_filter, 2);

    WRITE_BITS (bw, frame_header->is_motion_mode_switchable, 1);

    if (!(frame_header->error_resilient_mode
            || !seq_header->enable_ref_frame_mvs))
      WRITE_BITS (bw, frame_header->use_ref_frame_mvs, 1);
  }

  if (!(seq_header->reduced_still_picture_header
          || frame_header->disable_cdf_update))
    WRITE_BITS (bw, frame_header->disable_frame_end_update_cdf, 1);

  if (!_av1_bit_writer_tile_info (frame_header, seq_header, bw, space))
    goto error;

  if (!_av1_bit_writer_quantization_params (frame_header, seq_header,
          qindex_offset, bw, space))
    goto error;

  if (!_av1_bit_writer_segmentation_params (frame_header, seq_header,
          segmentation_offset, bw, space))
    goto error;

  if (!_av1_bit_writer_delta_q_params (frame_header, seq_header, bw, space))
    goto error;

  if (!_av1_bit_writer_delta_lf_params (frame_header, seq_header, bw, space))
    goto error;

  if (!_av1_bit_writer_loop_filter_params (frame_header, seq_header,
          loopfilter_offset, bw, space))
    goto error;

  if (!_av1_bit_writer_cdef_params (frame_header, seq_header,
          cdef_offset, cdef_size, bw, space))
    goto error;

  if (!_av1_bit_writer_loop_restoration_params (frame_header, seq_header,
          bw, space))
    goto error;

  /* 5.9.21 tx_mode() */
  if (frame_header->coded_lossless != 1) {
    /* Write the tx_mode_select bit. */
    if (frame_header->tx_mode == GST_AV1_TX_MODE_SELECT) {
      WRITE_BITS (bw, 1, 1);
    } else {
      WRITE_BITS (bw, 0, 1);
    }
  }

  /* 5.9.23 inlined frame_reference_mode () */
  if (!frame_header->frame_is_intra)
    WRITE_BITS (bw, frame_header->reference_select, 1);

  if (!_av1_bit_writer_skip_mode_params (frame_header, seq_header, bw, space))
    goto error;

  if (!(frame_header->frame_is_intra || frame_header->error_resilient_mode ||
          !seq_header->enable_warped_motion))
    WRITE_BITS (bw, frame_header->allow_warped_motion, 1);

  WRITE_BITS (bw, frame_header->reduced_tx_set, 1);

  if (!_av1_bit_writer_global_motion_params (frame_header,
          seq_header, bw, space))
    goto error;

  if (!_av1_bit_writer_film_grain_params (frame_header, seq_header, bw, space))
    goto error;

success:
  *space = TRUE;
  return TRUE;

error:
  GST_WARNING ("failed to write frame header");
  *space = have_space;
  return FALSE;
}

/**
 * gst_av1_bit_writer_frame_header_obu:
 * @frame_hdr: the frame header of #GstAV1FrameHeaderOBU to write
 * @seq_hdr: the sequence header of #GstAV1SequenceHeaderOBU to refer
 * @temporal_id: specifies the temporal level of the data contained in the OBU.
 * @spatial_id: specifies the spatial level of the data contained in the OBU.
 * @size_field: whether the OBU header contains the OBU size.
 * @data: (out): the bit stream generated by the frame header
 * @size: (inout): the size in bytes of the input and output
 *
 * Generating the according AV1 bit stream OBU by providing the frame header.
 *
 * Returns: a #GstAV1BitWriterResult
 *
 * Since: 1.22
 **/
GstAV1BitWriterResult
gst_av1_bit_writer_frame_header_obu (const GstAV1FrameHeaderOBU * frame_hdr,
    const GstAV1SequenceHeaderOBU * seq_hdr, guint8 temporal_id,
    guint8 spatial_id, gboolean size_field, guint8 * data, guint * size)
{
  return gst_av1_bit_writer_frame_header_obu_with_offsets (frame_hdr, seq_hdr,
      temporal_id, spatial_id, size_field, 0, NULL, NULL, NULL, NULL, NULL,
      data, size);
}

/**
 * gst_av1_bit_writer_frame_header_obu_with_offsets:
 * @frame_hdr: the frame header of #GstAV1FrameHeaderOBU to write
 * @seq_hdr: the sequence header of #GstAV1SequenceHeaderOBU to refer
 * @temporal_id: specifies the temporal level of the data contained in the OBU.
 * @spatial_id: specifies the spatial level of the data contained in the OBU.
 * @size_field: whether the OBU header contains the OBU size.
 * @size_field_size: >0 means a fixed OBU header size field.
 * @qindex_offset: (out): return the qindex fields offset in bits.
 * @segmentation_offset: (out): return the segmentation fields offset in bits.
 * @lf_offset: (out): return the loopfilter fields offset in bits.
 * @cdef_offset: (out): return the cdef fields offset in bits.
 * @cdef_size: (out): return the cdef fields size in bits.
 * @data: (out): the bit stream generated by the frame header
 * @size: (inout): the size in bytes of the input and output
 *
 * While Generating the according AV1 bit stream OBU by providing the frame
 * header, this function also return bit offsets of qindex, segmentation and
 * cdef, etc. These offsets can help to change the content of these fields
 * later. This function is useful if the encoder may change the content of
 * the frame header after generating it. For example, some HW needs user to
 * provide a frame header before the real encoding job, and it will change
 * the according fields in the frame header during the real encoding job.
 *
 * Returns: a #GstAV1BitWriterResult
 *
 * Since: 1.22
 **/
GstAV1BitWriterResult
gst_av1_bit_writer_frame_header_obu_with_offsets (const GstAV1FrameHeaderOBU *
    frame_hdr, const GstAV1SequenceHeaderOBU * seq_hdr, guint8 temporal_id,
    guint8 spatial_id, gboolean size_field, guint size_field_size,
    guint * qindex_offset, guint * segmentation_offset, guint * lf_offset,
    guint * cdef_offset, guint * cdef_size, guint8 * data, guint * size)
{
  gboolean have_space = TRUE;
  GstBitWriter bw;
  guint payload_size;
  guint header_size;

  g_return_val_if_fail (frame_hdr != NULL, GST_AV1_BIT_WRITER_ERROR);
  g_return_val_if_fail (seq_hdr != NULL, GST_AV1_BIT_WRITER_ERROR);
  g_return_val_if_fail (temporal_id < GST_AV1_MAX_NUM_TEMPORAL_LAYERS,
      GST_AV1_BIT_WRITER_ERROR);
  g_return_val_if_fail (spatial_id < GST_AV1_MAX_NUM_SPATIAL_LAYERS,
      GST_AV1_BIT_WRITER_ERROR);
  g_return_val_if_fail (data != NULL, GST_AV1_BIT_WRITER_ERROR);
  g_return_val_if_fail (size != NULL, GST_AV1_BIT_WRITER_ERROR);
  g_return_val_if_fail (*size > 0, GST_AV1_BIT_WRITER_ERROR);

  gst_bit_writer_init_with_data (&bw, data, *size, FALSE);

  WRITE_BITS (&bw, 0, 1);
  /* obu_type */
  WRITE_BITS (&bw, GST_AV1_OBU_FRAME_HEADER, 4);
  /* obu_extention_flag */
  if (temporal_id > 0 || spatial_id > 0) {
    WRITE_BITS (&bw, 1, 1);
  } else {
    WRITE_BITS (&bw, 0, 1);
  }

  /* obu_has_size_field */
  if (size_field > 0) {
    WRITE_BITS (&bw, 1, 1);
  } else {
    WRITE_BITS (&bw, 0, 1);
  }
  /* obu_reserved_1bit */
  WRITE_BITS (&bw, 0, 1);

  header_size = 1;

  /* 5.3.3 OBU extension header */
  if (temporal_id > 0 || spatial_id > 0) {
    WRITE_BITS (&bw, temporal_id, 3);
    WRITE_BITS (&bw, spatial_id, 2);
    /* obu_extension_header_reserved_3bits */
    WRITE_BITS (&bw, 0, 3);
    header_size = 2;
  }

  /* Write size field later */

  if (!_av1_bit_writer_uncompressed_frame_header (frame_hdr, seq_hdr,
          temporal_id, spatial_id, qindex_offset, segmentation_offset,
          lf_offset, cdef_offset, cdef_size, &bw, &have_space))
    goto error;

  /* Add trailings. */
  WRITE_BITS (&bw, 1, 1);
  if (!gst_bit_writer_align_bytes (&bw, 0)) {
    have_space = FALSE;
    goto error;
  }

  payload_size = gst_bit_writer_get_size (&bw);
  g_assert (payload_size % 8 == 0);
  payload_size /= 8;
  payload_size -= header_size;

  gst_bit_writer_reset (&bw);

  if (size_field) {
    size_field_size = _av1_bit_writer_add_size_field (data, size,
        header_size, payload_size, size_field_size, &have_space);
    if (!size_field_size)
      goto error;
  } else {
    *size = header_size + payload_size;
    size_field_size = 0;
  }

  if (qindex_offset)
    *qindex_offset = *qindex_offset + size_field_size * 8;
  if (segmentation_offset)
    *segmentation_offset = *segmentation_offset + size_field_size * 8;
  if (lf_offset)
    *lf_offset = *lf_offset + size_field_size * 8;
  if (cdef_offset)
    *cdef_offset = *cdef_offset + size_field_size * 8;

  return GST_AV1_BIT_WRITER_OK;

error:
  gst_bit_writer_reset (&bw);
  *size = 0;
  return have_space ? GST_AV1_BIT_WRITER_INVALID_DATA :
      GST_AV1_BIT_WRITER_NO_MORE_SPACE;
}

/**
 * gst_av1_bit_writer_temporal_delimiter_obu:
 * @size_field: whether the header contain size feild
 * @data: (out): the bit stream generated
 * @size: (inout): the size in bytes of the input and output
 *
 * Generating the according temporal delimiter AV1 bit stream OBU.
 *
 * Returns: a #GstAV1BitWriterResult
 *
 * Since: 1.22
 **/
GstAV1BitWriterResult
gst_av1_bit_writer_temporal_delimiter_obu (gboolean size_field,
    guint8 * data, guint * size)
{
  gboolean have_space = TRUE;
  GstBitWriter bw;

  g_return_val_if_fail (data != NULL, GST_AV1_BIT_WRITER_ERROR);
  g_return_val_if_fail (size != NULL, GST_AV1_BIT_WRITER_ERROR);
  g_return_val_if_fail (*size > 0, GST_AV1_BIT_WRITER_ERROR);

  gst_bit_writer_init_with_data (&bw, data, *size, FALSE);

  /* obu_forbidden_bit */
  WRITE_BITS (&bw, 0, 1);
  /* obu_type */
  WRITE_BITS (&bw, GST_AV1_OBU_TEMPORAL_DELIMITER, 4);
  /* obu_extention_flag */
  WRITE_BITS (&bw, 0, 1);
  /* obu_has_size_field */
  if (size_field) {
    WRITE_BITS (&bw, 1, 1);
  } else {
    WRITE_BITS (&bw, 0, 1);
  }
  /* obu_reserved_1bit */
  WRITE_BITS (&bw, 0, 1);

  /* No need to add trailings */

  /* header_size is 1 and payload_size is 0 */
  if (size_field) {
    if (!_av1_bit_writer_add_size_field (data, size, 1, 0, 0, &have_space))
      goto error;
  } else {
    *size = 1 + 0;
  }

  return GST_AV1_BIT_WRITER_OK;

error:
  gst_bit_writer_reset (&bw);
  *size = 0;
  return have_space ? GST_AV1_BIT_WRITER_INVALID_DATA :
      GST_AV1_BIT_WRITER_NO_MORE_SPACE;
}

/* 5.8.2 */
static gboolean
_av1_bit_writer_metadata_itut_t35 (const GstAV1MetadataITUT_T35 * itut_t35,
    GstBitWriter * bw, gboolean * space)
{
  gboolean have_space = TRUE;

  GST_DEBUG ("writing metadata itut t35");

  WRITE_BITS (bw, itut_t35->itu_t_t35_country_code, 8);

  if (itut_t35->itu_t_t35_country_code == 0xFF)
    WRITE_BITS (bw, itut_t35->itu_t_t35_country_code_extention_byte, 8);

  *space = TRUE;
  return TRUE;

error:
  GST_WARNING ("failed to write metadata itut t35");
  *space = have_space;
  return FALSE;
}

/* 5.8.3 */
static gboolean
_av1_bit_writer_metadata_hdr_cll (const GstAV1MetadataHdrCll * hdr_cll,
    GstBitWriter * bw, gboolean * space)
{
  gboolean have_space = TRUE;

  GST_DEBUG ("writing metadata hdr cll");

  WRITE_BITS (bw, hdr_cll->max_cll, 16);
  WRITE_BITS (bw, hdr_cll->max_fall, 16);

  *space = TRUE;
  return TRUE;

error:
  GST_WARNING ("failed to write metadata hdr cll");
  *space = have_space;
  return FALSE;
}

/* 5.8.4 */
static gboolean
_av1_bit_writer_metadata_hdr_mdcv (const GstAV1MetadataHdrMdcv * hdr_mdcv,
    GstBitWriter * bw, gboolean * space)
{
  gboolean have_space = TRUE;
  gint i;

  GST_DEBUG ("writing metadata hdr mdcv");

  for (i = 0; i < 3; i++) {
    WRITE_BITS (bw, hdr_mdcv->primary_chromaticity_x[i], 16);
    WRITE_BITS (bw, hdr_mdcv->primary_chromaticity_y[i], 16);
  }

  WRITE_BITS (bw, hdr_mdcv->white_point_chromaticity_x, 16);
  WRITE_BITS (bw, hdr_mdcv->white_point_chromaticity_y, 16);

  WRITE_BITS (bw, hdr_mdcv->luminance_max, 32);
  WRITE_BITS (bw, hdr_mdcv->luminance_min, 32);

  *space = TRUE;
  return TRUE;

error:
  GST_WARNING ("failed to write metadata hdr mdcv");
  *space = have_space;
  return FALSE;
}

/* 5.8.5 */
static gboolean
_av1_bit_writer_metadata_scalability (const GstAV1MetadataScalability *
    scalability, GstBitWriter * bw, gboolean * space)
{
  gboolean have_space = TRUE;
  guint i, j;

  GST_DEBUG ("writing metadata scalability");

  WRITE_BITS (bw, scalability->scalability_mode_idc, 8);

  if (scalability->scalability_mode_idc != GST_AV1_SCALABILITY_SS)
    goto success;

  /* 5.8.6 */
  WRITE_BITS (bw, scalability->spatial_layers_cnt_minus_1, 2);
  WRITE_BITS (bw, scalability->spatial_layer_dimensions_present_flag, 1);
  WRITE_BITS (bw, scalability->spatial_layer_description_present_flag, 1);
  WRITE_BITS (bw, scalability->temporal_group_description_present_flag, 1);
  /* scalability_structure_reserved_3bits */
  WRITE_BITS (bw, 0, 3);

  if (scalability->spatial_layer_dimensions_present_flag) {
    for (i = 0; i <= scalability->spatial_layers_cnt_minus_1; i++) {
      WRITE_BITS (bw, scalability->spatial_layer_max_width[i], 16);
      WRITE_BITS (bw, scalability->spatial_layer_max_height[i], 16);
    }
  }

  if (scalability->spatial_layer_description_present_flag) {
    for (i = 0; i <= scalability->spatial_layers_cnt_minus_1; i++)
      WRITE_BITS (bw, scalability->spatial_layer_ref_id[i], 8);
  }

  if (scalability->temporal_group_description_present_flag) {
    WRITE_BITS (bw, scalability->temporal_group_size, 8);

    for (i = 0; i < scalability->temporal_group_size; i++) {
      WRITE_BITS (bw, scalability->temporal_group_temporal_id[i], 3);
      WRITE_BITS (bw,
          scalability->temporal_group_temporal_switching_up_point_flag[i], 1);
      WRITE_BITS (bw,
          scalability->temporal_group_spatial_switching_up_point_flag[i], 1);
      WRITE_BITS (bw, scalability->temporal_group_ref_cnt[i], 3);
      for (j = 0; j < scalability->temporal_group_ref_cnt[i]; j++)
        WRITE_BITS (bw, scalability->temporal_group_ref_pic_diff[i][j], 8);
    }
  }

success:
  *space = TRUE;
  return TRUE;

error:
  GST_WARNING ("failed to write metadata scalability");
  *space = have_space;
  return FALSE;
}

/**
 * gst_av1_bit_writer_metadata_obu:
 * @metadata: the meta data of #GstAV1MetadataOBU to write
 * @temporal_id: specifies the temporal level of the data contained in the OBU.
 * @spatial_id: specifies the spatial level of the data contained in the OBU.
 * @size_field: whether the header contain size feild
 * @data: (out): the bit stream generated by the meta data
 * @size: (inout): the size in bytes of the input and output
 *
 * Generating the according AV1 bit stream OBU by providing the meta data.
 *
 * Returns: a #GstAV1BitWriterResult
 *
 * Since: 1.22
 **/
GstAV1BitWriterResult
gst_av1_bit_writer_metadata_obu (const GstAV1MetadataOBU * metadata,
    guint8 temporal_id, guint8 spatial_id, gboolean size_field,
    guint8 * data, guint * size)
{
  gboolean have_space = TRUE;
  GstBitWriter bw;
  guint header_size;
  guint payload_size;
  guint8 metadata_type_data[4];
  guint metadata_size;
  guint i;

  g_return_val_if_fail (metadata != NULL, GST_AV1_BIT_WRITER_ERROR);
  g_return_val_if_fail (temporal_id < GST_AV1_MAX_NUM_TEMPORAL_LAYERS,
      GST_AV1_BIT_WRITER_ERROR);
  g_return_val_if_fail (spatial_id < GST_AV1_MAX_NUM_SPATIAL_LAYERS,
      GST_AV1_BIT_WRITER_ERROR);
  g_return_val_if_fail (data != NULL, GST_AV1_BIT_WRITER_ERROR);
  g_return_val_if_fail (size != NULL, GST_AV1_BIT_WRITER_ERROR);
  g_return_val_if_fail (*size > 0, GST_AV1_BIT_WRITER_ERROR);

  gst_bit_writer_init_with_data (&bw, data, *size, FALSE);

  /* obu_forbidden_bit */
  WRITE_BITS (&bw, 0, 1);
  /* obu_type */
  WRITE_BITS (&bw, GST_AV1_OBU_METADATA, 4);
  /* obu_extention_flag */
  if (temporal_id > 0 || spatial_id > 0) {
    WRITE_BITS (&bw, 1, 1);
  } else {
    WRITE_BITS (&bw, 0, 1);
  }
  /* obu_has_size_field */
  if (size_field) {
    WRITE_BITS (&bw, 1, 1);
  } else {
    WRITE_BITS (&bw, 0, 1);
  }
  /* obu_reserved_1bit */
  WRITE_BITS (&bw, 0, 1);

  header_size = 1;
  /* 5.3.3 OBU extension header */
  if (temporal_id > 0 || spatial_id > 0) {
    WRITE_BITS (&bw, temporal_id, 3);
    WRITE_BITS (&bw, spatial_id, 2);
    /* obu_extension_header_reserved_3bits */
    WRITE_BITS (&bw, 0, 3);
    header_size = 2;
  }

  /* Generate the metadata_type first */
  metadata_size = _av1_uleb_size_in_bytes (metadata->metadata_type);
  if (metadata_size > 4) {
    GST_WARNING ("Invalid metadata_type");
    goto error;
  }
  if (!_av1_encode_uleb (metadata->metadata_type,
          sizeof (metadata->metadata_type),
          metadata_type_data, metadata_size)) {
    GST_WARNING ("Failed to write metadata_type");
    goto error;
  }
  for (i = 0; i < metadata_size; i++)
    WRITE_BITS (&bw, metadata_type_data[i], 8);

  switch (metadata->metadata_type) {
    case GST_AV1_METADATA_TYPE_ITUT_T35:
      if (!_av1_bit_writer_metadata_itut_t35 (&metadata->itut_t35,
              &bw, &have_space))
        goto error;
      break;
    case GST_AV1_METADATA_TYPE_HDR_CLL:
      if (!_av1_bit_writer_metadata_hdr_cll (&metadata->hdr_cll,
              &bw, &have_space))
        goto error;
      break;
    case GST_AV1_METADATA_TYPE_HDR_MDCV:
      if (!_av1_bit_writer_metadata_hdr_mdcv (&metadata->hdr_mdcv,
              &bw, &have_space))
        goto error;
      break;
    case GST_AV1_METADATA_TYPE_SCALABILITY:
      if (!_av1_bit_writer_metadata_scalability (&metadata->scalability,
              &bw, &have_space))
        goto error;
      break;
    default:
      GST_WARNING ("Unsupported metadata_type");
      goto error;
  }

  /* Add trailings. */
  WRITE_BITS (&bw, 1, 1);
  if (!gst_bit_writer_align_bytes (&bw, 0)) {
    have_space = FALSE;
    goto error;
  }

  payload_size = gst_bit_writer_get_size (&bw);
  g_assert (payload_size % 8 == 0);
  payload_size /= 8;
  payload_size -= header_size;

  gst_bit_writer_reset (&bw);

  if (size_field) {
    if (!_av1_bit_writer_add_size_field (data, size,
            header_size, payload_size, 0, &have_space))
      goto error;
  } else {
    *size = header_size + payload_size;
  }

  return GST_AV1_BIT_WRITER_OK;

error:
  gst_bit_writer_reset (&bw);
  *size = 0;
  return have_space ? GST_AV1_BIT_WRITER_INVALID_DATA :
      GST_AV1_BIT_WRITER_NO_MORE_SPACE;
}
