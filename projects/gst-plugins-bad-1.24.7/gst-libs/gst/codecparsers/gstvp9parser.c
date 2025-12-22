/* gstvp9parser.c
 *
 *  Copyright (C) 2013-2014 Intel Corporation
 *  Copyright (C) 2015 Intel Corporation
 *    Author: XuGuangxin<Guangxin.Xu@intel.com>
 *    Author: Sreerenj Balachandran<sreerenj.balachandran@intel.com>
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
/**
 * SECTION:gstvp9parser
 * @title: GstVp9Parser
 * @short_description: Convenience library for parsing vp9 video bitstream.
 *
 * For more details about the structures, you can refer to the
 * specifications:
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <stdlib.h>
#include <gst/base/gstbitreader.h>
#include "vp9utils.h"
#include "gstvp9parser.h"

#define MIN_TILE_WIDTH_B64 4
#define MAX_TILE_WIDTH_B64 64

/* order of sb64, where sb64 = 64x64 */
#define ALIGN_SB64(w) ((w + 63) >> 6)

GST_DEBUG_CATEGORY_STATIC (gst_vp9_parser_debug);
#define GST_CAT_DEFAULT gst_vp9_parser_debug

static gboolean initialized = FALSE;
#define INITIALIZE_DEBUG_CATEGORY \
  if (!initialized) { \
    GST_DEBUG_CATEGORY_INIT (gst_vp9_parser_debug, "codecparsers_vp9", 0, \
        "vp9 parser library"); \
    initialized = TRUE; \
  }

#define gst_vp9_read_bit(br) gst_bit_reader_get_bits_uint8_unchecked(br, 1)
#define gst_vp9_read_bits(br, bits) gst_bit_reader_get_bits_uint32_unchecked(br, bits)

#define GST_VP9_PARSER_GET_PRIVATE(parser)  ((GstVp9ParserPrivate *)(parser))

typedef struct _ReferenceSize
{
  guint32 width;
  guint32 height;
} ReferenceSize;

typedef struct
{
  /* Public part of the struct */
  GstVp9Parser parser;

  /* Private part of the struct for tracking state variables across frames */

  /* for loop filters */
  gint8 ref_deltas[GST_VP9_MAX_REF_LF_DELTAS];
  gint8 mode_deltas[GST_VP9_MAX_MODE_LF_DELTAS];

  guint8 segmentation_abs_delta;
  GstVp9SegmentationInfoData segmentation[GST_VP9_MAX_SEGMENTS];

  ReferenceSize reference[GST_VP9_REF_FRAMES];
} GstVp9ParserPrivate;

static gint32
gst_vp9_read_signed_bits (GstBitReader * br, int bits)
{
  const gint32 value = gst_vp9_read_bits (br, bits);
  return gst_vp9_read_bit (br) ? -value : value;
}

static gboolean
verify_frame_marker (GstBitReader * br)
{
  guint8 frame_marker = gst_vp9_read_bits (br, 2);
  if (frame_marker != GST_VP9_FRAME_MARKER) {
    GST_ERROR ("Invalid VP9 Frame Marker !");
    return FALSE;
  }
  return TRUE;
}

static gboolean
verify_sync_code (GstBitReader * const br)
{
  return (gst_vp9_read_bits (br, 24) == GST_VP9_SYNC_CODE);
}

static gboolean
verify_superframe_marker (GstBitReader * br)
{
  return gst_vp9_read_bits (br, 3) == GST_VP9_SUPERFRAME_MARKER;
}

static gboolean
parse_bitdepth_colorspace_sampling (GstVp9Parser * parser,
    GstBitReader * const br, GstVp9FrameHdr * frame_hdr)
{
  if (frame_hdr->profile > GST_VP9_PROFILE_1)
    parser->bit_depth =
        gst_vp9_read_bit (br) ? GST_VP9_BIT_DEPTH_12 : GST_VP9_BIT_DEPTH_10;
  else
    parser->bit_depth = GST_VP9_BIT_DEPTH_8;

  parser->color_space = gst_vp9_read_bits (br, 3);
  if (parser->color_space != GST_VP9_CS_SRGB) {
    parser->color_range = gst_vp9_read_bit (br);

    if (frame_hdr->profile == GST_VP9_PROFILE_1
        || frame_hdr->profile == GST_VP9_PROFILE_3) {

      parser->subsampling_x = gst_vp9_read_bit (br);
      parser->subsampling_y = gst_vp9_read_bit (br);

      if (parser->subsampling_x == 1 && parser->subsampling_y == 1) {
        GST_ERROR
            ("4:2:0 subsampling is not supported in profile_1 or profile_3");
        goto error;
      }

      if (gst_vp9_read_bit (br)) {
        GST_ERROR ("Reserved bit set!");
        goto error;
      }
    } else {
      parser->subsampling_y = parser->subsampling_x = 1;
    }
  } else {
    parser->color_range = GST_VP9_CR_FULL;

    if (frame_hdr->profile == GST_VP9_PROFILE_1
        || frame_hdr->profile == GST_VP9_PROFILE_3) {
      if (gst_vp9_read_bit (br)) {
        GST_ERROR ("Reserved bit set!");
        goto error;
      }
    } else {
      GST_ERROR
          ("4:4:4 subsampling is not supported in profile_0 and profile_2");
      goto error;
    }
  }
  return TRUE;

error:
  return FALSE;
}

static guint
parse_profile (GstBitReader * br)
{
  guint8 profile = gst_vp9_read_bit (br);
  profile |= gst_vp9_read_bit (br) << 1;
  if (profile > 2)
    profile += gst_vp9_read_bit (br);
  return profile;
}

static void
parse_frame_size (GstBitReader * br, guint32 * width, guint32 * height)
{
  const guint32 w = gst_vp9_read_bits (br, 16) + 1;
  const guint32 h = gst_vp9_read_bits (br, 16) + 1;
  *width = w;
  *height = h;
}

static void
parse_display_frame_size (GstBitReader * br, GstVp9FrameHdr * frame_hdr)
{
  frame_hdr->display_size_enabled = gst_vp9_read_bit (br);
  if (frame_hdr->display_size_enabled)
    parse_frame_size (br, &frame_hdr->display_width,
        &frame_hdr->display_height);
}

static void
parse_frame_size_from_refs (const GstVp9Parser * parser,
    GstVp9FrameHdr * frame_hdr, GstBitReader * br)
{
  gboolean found = FALSE;
  int i;
  GstVp9ParserPrivate *priv = GST_VP9_PARSER_GET_PRIVATE (parser);

  for (i = 0; i < GST_VP9_REFS_PER_FRAME; i++) {
    found = gst_vp9_read_bit (br);

    if (found) {
      guint8 idx = frame_hdr->ref_frame_indices[i];
      frame_hdr->width = priv->reference[idx].width;
      frame_hdr->height = priv->reference[idx].height;
      break;
    }
  }
  if (!found)
    parse_frame_size (br, &frame_hdr->width, &frame_hdr->height);
}

static GstVp9InterpolationFilter
parse_interp_filter (GstBitReader * br)
{
  static const GstVp9InterpolationFilter filter_map[] = {
    GST_VP9_INTERPOLATION_FILTER_EIGHTTAP_SMOOTH,
    GST_VP9_INTERPOLATION_FILTER_EIGHTTAP,
    GST_VP9_INTERPOLATION_FILTER_EIGHTTAP_SHARP,
    GST_VP9_INTERPOLATION_FILTER_BILINEAR
  };

  return gst_vp9_read_bit (br) ? GST_VP9_INTERPOLATION_FILTER_SWITCHABLE :
      filter_map[gst_vp9_read_bits (br, 2)];
}

static void
parse_loopfilter (GstVp9LoopFilter * lf, GstBitReader * br)
{
  lf->filter_level = gst_vp9_read_bits (br, 6);
  lf->sharpness_level = gst_vp9_read_bits (br, 3);

  lf->mode_ref_delta_update = 0;

  lf->mode_ref_delta_enabled = gst_vp9_read_bit (br);
  if (lf->mode_ref_delta_enabled) {
    lf->mode_ref_delta_update = gst_vp9_read_bit (br);
    if (lf->mode_ref_delta_update) {
      int i;
      for (i = 0; i < GST_VP9_MAX_REF_LF_DELTAS; i++) {
        lf->update_ref_deltas[i] = gst_vp9_read_bit (br);
        if (lf->update_ref_deltas[i])
          lf->ref_deltas[i] = gst_vp9_read_signed_bits (br, 6);
      }

      for (i = 0; i < GST_VP9_MAX_MODE_LF_DELTAS; i++) {
        lf->update_mode_deltas[i] = gst_vp9_read_bit (br);
        if (lf->update_mode_deltas[i])
          lf->mode_deltas[i] = gst_vp9_read_signed_bits (br, 6);
      }
    }
  }
}

static gint8
parse_delta_q (GstBitReader * br)
{
  return gst_vp9_read_bit (br) ? gst_vp9_read_signed_bits (br, 4) : 0;
}

static void
parse_quantization (GstVp9QuantIndices * quant_indices, GstBitReader * br)
{
  quant_indices->y_ac_qi = gst_vp9_read_bits (br, QINDEX_BITS);
  quant_indices->y_dc_delta = parse_delta_q (br);
  quant_indices->uv_dc_delta = parse_delta_q (br);
  quant_indices->uv_ac_delta = parse_delta_q (br);
}

static void
parse_segmentation (GstVp9SegmentationInfo * seg, GstBitReader * br)
{
  int i;

  seg->update_map = FALSE;
  seg->update_data = FALSE;

  seg->enabled = gst_vp9_read_bit (br);
  if (!seg->enabled)
    return;

  /* Segmentation map update */
  seg->update_map = gst_vp9_read_bit (br);
  if (seg->update_map) {
    for (i = 0; i < GST_VP9_SEG_TREE_PROBS; i++) {
      seg->update_tree_probs[i] = gst_vp9_read_bit (br);
      seg->tree_probs[i] = seg->update_tree_probs[i] ?
          gst_vp9_read_bits (br, 8) : GST_VP9_MAX_PROB;
    }

    seg->temporal_update = gst_vp9_read_bit (br);
    if (seg->temporal_update) {
      for (i = 0; i < GST_VP9_PREDICTION_PROBS; i++) {
        seg->update_pred_probs[i] = gst_vp9_read_bit (br);
        seg->pred_probs[i] = seg->update_pred_probs[i] ?
            gst_vp9_read_bits (br, 8) : GST_VP9_MAX_PROB;
      }
    } else {
      for (i = 0; i < GST_VP9_PREDICTION_PROBS; i++)
        seg->pred_probs[i] = GST_VP9_MAX_PROB;
    }
  }

  /* Segmentation data update */
  seg->update_data = gst_vp9_read_bit (br);

  if (seg->update_data) {
    /* clear all features */
    memset (seg->data, 0, sizeof (seg->data));

    seg->abs_delta = gst_vp9_read_bit (br);

    for (i = 0; i < GST_VP9_MAX_SEGMENTS; i++) {
      GstVp9SegmentationInfoData *seg_data = seg->data + i;
      guint8 data;

      /* SEG_LVL_ALT_Q */
      seg_data->alternate_quantizer_enabled = gst_vp9_read_bit (br);
      if (seg_data->alternate_quantizer_enabled) {
        data = gst_vp9_read_bits (br, 8);
        seg_data->alternate_quantizer = gst_vp9_read_bit (br) ? -data : data;
      }

      /* SEG_LVL_ALT_LF */
      seg_data->alternate_loop_filter_enabled = gst_vp9_read_bit (br);
      if (seg_data->alternate_loop_filter_enabled) {
        data = gst_vp9_read_bits (br, 6);
        seg_data->alternate_loop_filter = gst_vp9_read_bit (br) ? -data : data;
      }

      /* SEG_LVL_REF_FRAME */
      seg_data->reference_frame_enabled = gst_vp9_read_bit (br);
      if (seg_data->reference_frame_enabled) {
        seg_data->reference_frame = gst_vp9_read_bits (br, 2);
      }

      seg_data->reference_skip = gst_vp9_read_bit (br);
    }
  }
}

static guint32
get_max_lb_tile_cols (guint32 sb_cols)
{
  gint max_log2 = 1;
  while ((sb_cols >> max_log2) >= MIN_TILE_WIDTH_B64)
    ++max_log2;
  return max_log2 - 1;
}

static guint32
get_min_lb_tile_cols (guint32 sb_cols)
{
  gint min_log2 = 0;
  while ((MAX_TILE_WIDTH_B64 << min_log2) < sb_cols)
    ++min_log2;
  return min_log2;
}

static gboolean
parse_tile_info (GstVp9FrameHdr * frame_hdr, GstBitReader * br)
{
  guint32 max_ones;
  const guint32 sb_cols = ALIGN_SB64 (frame_hdr->width);
  guint32 min_lb_tile_cols = get_min_lb_tile_cols (sb_cols);
  guint32 max_lb_tile_cols = get_max_lb_tile_cols (sb_cols);

  g_assert (min_lb_tile_cols <= max_lb_tile_cols);
  max_ones = max_lb_tile_cols - min_lb_tile_cols;

  /* columns */
  frame_hdr->log2_tile_columns = min_lb_tile_cols;
  while (max_ones-- && gst_vp9_read_bit (br))
    frame_hdr->log2_tile_columns++;

  if (frame_hdr->log2_tile_columns > 6) {
    GST_ERROR ("Invalid number of tile columns..!");
    return FALSE;
  }

  /* row */
  frame_hdr->log2_tile_rows = gst_vp9_read_bit (br);
  if (frame_hdr->log2_tile_rows)
    frame_hdr->log2_tile_rows += gst_vp9_read_bit (br);

  return TRUE;
}

static void
loop_filter_update (GstVp9Parser * parser, const GstVp9LoopFilter * lf)
{
  GstVp9ParserPrivate *priv = GST_VP9_PARSER_GET_PRIVATE (parser);
  int i;

  for (i = 0; i < GST_VP9_MAX_REF_LF_DELTAS; i++) {
    if (lf->update_ref_deltas[i])
      priv->ref_deltas[i] = lf->ref_deltas[i];
  }

  for (i = 0; i < GST_VP9_MAX_MODE_LF_DELTAS; i++) {
    if (lf->update_mode_deltas[i])
      priv->mode_deltas[i] = lf->mode_deltas[i];
  }
}

static guint8
seg_get_base_qindex (const GstVp9Parser * parser,
    const GstVp9FrameHdr * frame_hdr, int segid)
{
  int seg_base = frame_hdr->quant_indices.y_ac_qi;
  GstVp9ParserPrivate *priv = GST_VP9_PARSER_GET_PRIVATE (parser);
  const GstVp9SegmentationInfoData *seg = priv->segmentation + segid;
  /* DEBUG("id = %d, seg_base = %d, seg enable = %d, alt enable = %d, abs = %d, alt= %d\n",segid,
     seg_base, frame_hdr->segmentation.enabled, seg->alternate_quantizer_enabled, priv->segmentation_abs_delta,  seg->alternate_quantizer);
   */
  if (frame_hdr->segmentation.enabled && seg->alternate_quantizer_enabled) {
    if (priv->segmentation_abs_delta)
      seg_base = seg->alternate_quantizer;
    else
      seg_base += seg->alternate_quantizer;
  }
  return CLAMP (seg_base, 0, MAXQ);
}

static guint8
seg_get_filter_level (const GstVp9Parser * parser,
    const GstVp9FrameHdr * frame_hdr, int segid)
{
  int seg_filter = frame_hdr->loopfilter.filter_level;
  GstVp9ParserPrivate *priv = GST_VP9_PARSER_GET_PRIVATE (parser);
  const GstVp9SegmentationInfoData *seg = priv->segmentation + segid;

  if (frame_hdr->segmentation.enabled && seg->alternate_loop_filter_enabled) {
    if (priv->segmentation_abs_delta)
      seg_filter = seg->alternate_loop_filter;
    else
      seg_filter += seg->alternate_loop_filter;
  }
  return CLAMP (seg_filter, 0, GST_VP9_MAX_LOOP_FILTER);
}

/*save segmentation info from frame header to parser*/
static void
segmentation_save (GstVp9Parser * parser, const GstVp9FrameHdr * frame_hdr)
{
  const GstVp9SegmentationInfo *info = &frame_hdr->segmentation;
  if (!info->enabled)
    return;

  if (info->update_map) {
    g_assert (G_N_ELEMENTS (parser->mb_segment_tree_probs) ==
        G_N_ELEMENTS (info->tree_probs));
    g_assert (G_N_ELEMENTS (parser->segment_pred_probs) ==
        G_N_ELEMENTS (info->pred_probs));
    memcpy (parser->mb_segment_tree_probs, info->tree_probs,
        sizeof (info->tree_probs));
    memcpy (parser->segment_pred_probs, info->pred_probs,
        sizeof (info->pred_probs));
  }

  if (info->update_data) {
    GstVp9ParserPrivate *priv = GST_VP9_PARSER_GET_PRIVATE (parser);
    priv->segmentation_abs_delta = info->abs_delta;
    g_assert (G_N_ELEMENTS (priv->segmentation) == G_N_ELEMENTS (info->data));
    memcpy (priv->segmentation, info->data, sizeof (info->data));
  }
}

static void
segmentation_update (GstVp9Parser * parser, const GstVp9FrameHdr * frame_hdr)
{
  int i = 0;
  const GstVp9ParserPrivate *priv = GST_VP9_PARSER_GET_PRIVATE (parser);
  const GstVp9LoopFilter *lf = &frame_hdr->loopfilter;
  const GstVp9QuantIndices *quant_indices = &frame_hdr->quant_indices;
  int default_filter = lf->filter_level;
  const int scale = 1 << (default_filter >> 5);

  segmentation_save (parser, frame_hdr);

  for (i = 0; i < GST_VP9_MAX_SEGMENTS; i++) {
    guint8 q = seg_get_base_qindex (parser, frame_hdr, i);

    GstVp9Segmentation *seg = parser->segmentation + i;
    const GstVp9SegmentationInfoData *info = priv->segmentation + i;

    seg->luma_dc_quant_scale =
        gst_vp9_dc_quant (q, quant_indices->y_dc_delta, parser->bit_depth);
    seg->luma_ac_quant_scale = gst_vp9_ac_quant (q, 0, parser->bit_depth);
    seg->chroma_dc_quant_scale =
        gst_vp9_dc_quant (q, quant_indices->uv_dc_delta, parser->bit_depth);
    seg->chroma_ac_quant_scale =
        gst_vp9_ac_quant (q, quant_indices->uv_ac_delta, parser->bit_depth);

    if (lf->filter_level) {
      guint8 filter = seg_get_filter_level (parser, frame_hdr, i);

      if (!lf->mode_ref_delta_enabled) {
        memset (seg->filter_level, filter, sizeof (seg->filter_level));
      } else {
        int ref, mode;
        const int intra_filter =
            filter + priv->ref_deltas[GST_VP9_REF_FRAME_INTRA] * scale;
        seg->filter_level[GST_VP9_REF_FRAME_INTRA][0] =
            CLAMP (intra_filter, 0, GST_VP9_MAX_LOOP_FILTER);
        for (ref = GST_VP9_REF_FRAME_LAST; ref < GST_VP9_REF_FRAME_MAX; ++ref) {
          for (mode = 0; mode < GST_VP9_MAX_MODE_LF_DELTAS; ++mode) {
            const int inter_filter = filter + priv->ref_deltas[ref] * scale
                + priv->mode_deltas[mode] * scale;
            seg->filter_level[ref][mode] =
                CLAMP (inter_filter, 0, GST_VP9_MAX_LOOP_FILTER);
          }
        }
      }
    }
    seg->reference_frame_enabled = info->reference_frame_enabled;;
    seg->reference_frame = info->reference_frame;
    seg->reference_skip = info->reference_skip;
  }
}

static void
reference_update (GstVp9Parser * parser, const GstVp9FrameHdr * const frame_hdr)
{
  guint8 flag = 1;
  guint8 refresh_frame_flags;
  int i;
  GstVp9ParserPrivate *priv = GST_VP9_PARSER_GET_PRIVATE (parser);
  ReferenceSize *reference = priv->reference;
  if (frame_hdr->frame_type == GST_VP9_KEY_FRAME) {
    refresh_frame_flags = 0xff;
  } else {
    refresh_frame_flags = frame_hdr->refresh_frame_flags;
  }
  for (i = 0; i < GST_VP9_REF_FRAMES; i++) {
    if (refresh_frame_flags & flag) {
      reference[i].width = frame_hdr->width;
      reference[i].height = frame_hdr->height;
    }
    flag <<= 1;
  }
}

static inline int
frame_is_intra_only (const GstVp9FrameHdr * frame_hdr)
{
  return frame_hdr->frame_type == GST_VP9_KEY_FRAME || frame_hdr->intra_only;
}

static void
set_default_lf_deltas (GstVp9Parser * parser)
{
  GstVp9ParserPrivate *priv = GST_VP9_PARSER_GET_PRIVATE (parser);
  priv->ref_deltas[GST_VP9_REF_FRAME_INTRA] = 1;
  priv->ref_deltas[GST_VP9_REF_FRAME_LAST] = 0;
  priv->ref_deltas[GST_VP9_REF_FRAME_GOLDEN] = -1;
  priv->ref_deltas[GST_VP9_REF_FRAME_ALTREF] = -1;

  priv->mode_deltas[0] = 0;
  priv->mode_deltas[1] = 0;
}

static void
set_default_segmentation_info (GstVp9Parser * parser)
{
  GstVp9ParserPrivate *priv = GST_VP9_PARSER_GET_PRIVATE (parser);

  memset (priv->segmentation, 0, sizeof (priv->segmentation));

  priv->segmentation_abs_delta = FALSE;
}

static void
setup_past_independence (GstVp9Parser * parser,
    GstVp9FrameHdr * const frame_hdr)
{
  set_default_lf_deltas (parser);
  set_default_segmentation_info (parser);

  memset (frame_hdr->ref_frame_sign_bias, 0,
      sizeof (frame_hdr->ref_frame_sign_bias));
}

static void
gst_vp9_parser_reset (GstVp9Parser * parser)
{
  GstVp9ParserPrivate *priv = GST_VP9_PARSER_GET_PRIVATE (parser);
  GstVp9Parser saved_vals = *parser;

  /* Reset almost everything in private struct and public struct */
  memset (priv, 0, sizeof (GstVp9ParserPrivate));

  /* Restore the fields we want to keep as-is */
  parser->subsampling_x = saved_vals.subsampling_x;
  parser->subsampling_y = saved_vals.subsampling_y;
  parser->bit_depth = saved_vals.bit_depth;
  parser->color_space = saved_vals.color_space;
  parser->color_range = saved_vals.color_range;
}

static GstVp9ParserResult
gst_vp9_parser_update (GstVp9Parser * parser, GstVp9FrameHdr * const frame_hdr)
{
  if (frame_hdr->frame_type == GST_VP9_KEY_FRAME)
    gst_vp9_parser_reset (parser);

  if (frame_is_intra_only (frame_hdr) || frame_hdr->error_resilient_mode)
    setup_past_independence (parser, frame_hdr);

  loop_filter_update (parser, &frame_hdr->loopfilter);
  segmentation_update (parser, frame_hdr);
  reference_update (parser, frame_hdr);

  return GST_VP9_PARSER_OK;
}


/******** API *************/

/**
 * gst_vp9_parser_new:
 *
 * Creates a new #GstVp9Parser. It should be freed with
 * gst_vp9_parser_free() after use.
 *
 * Returns: a new #GstVp9Parser
 *
 * Since: 1.8
 */
GstVp9Parser *
gst_vp9_parser_new (void)
{
  GstVp9ParserPrivate *priv;
  GstVp9Parser *parser;

  INITIALIZE_DEBUG_CATEGORY;
  GST_DEBUG ("Create VP9 Parser");

  priv = g_new0 (GstVp9ParserPrivate, 1);
  parser = &priv->parser;
  parser->subsampling_x = parser->subsampling_y = -1;

  return parser;
}

/**
 * gst_vp9_parser_free:
 * @parser: the #GstVp9Parser to free
 *
 * Frees @parser.
 *
 * Since: 1.8
 */
void
gst_vp9_parser_free (GstVp9Parser * parser)
{
  if (parser) {
    GstVp9ParserPrivate *priv = GST_VP9_PARSER_GET_PRIVATE (parser);
    g_free (priv);
  }
}

/**
 * gst_vp9_parser_parse_frame_header:
 * @parser: The #GstVp9Parser
 * @frame_hdr: The #GstVp9FrameHdr to fill
 * @data: The data to parse
 * @size: The size of the @data to parse
 *
 * Parses the VP9 bitstream contained in @data, and fills in @frame_hdr
 * with the information. The @size argument represent the whole frame size.
 *
 * Returns: a #GstVp9ParserResult
 *
 * Since: 1.8
 */
GstVp9ParserResult
gst_vp9_parser_parse_frame_header (GstVp9Parser * parser,
    GstVp9FrameHdr * frame_hdr, const guint8 * data, gsize size)
{
  GstBitReader bit_reader;
  GstBitReader *br = &bit_reader;

  gst_bit_reader_init (br, data, size);
  memset (frame_hdr, 0, sizeof (*frame_hdr));

  /* Parsing Uncompressed Data Chunk */

  if (!verify_frame_marker (br))
    goto error;

  frame_hdr->profile = parse_profile (br);
  if (frame_hdr->profile > GST_VP9_PROFILE_UNDEFINED) {
    GST_ERROR ("Stream has undefined VP9  profile !");
    goto error;
  }

  frame_hdr->show_existing_frame = gst_vp9_read_bit (br);
  if (frame_hdr->show_existing_frame) {
    frame_hdr->frame_to_show = gst_vp9_read_bits (br, GST_VP9_REF_FRAMES_LOG2);
    return GST_VP9_PARSER_OK;
  }

  frame_hdr->frame_type = gst_vp9_read_bit (br);
  frame_hdr->show_frame = gst_vp9_read_bit (br);
  frame_hdr->error_resilient_mode = gst_vp9_read_bit (br);

  if (frame_hdr->frame_type == GST_VP9_KEY_FRAME) {

    if (!verify_sync_code (br)) {
      GST_ERROR ("Invalid VP9 Key-frame sync code !");
      goto error;
    }

    if (!parse_bitdepth_colorspace_sampling (parser, br, frame_hdr)) {
      GST_ERROR ("Failed to parse color_space/bit_depth info !");
      goto error;
    }

    parse_frame_size (br, &frame_hdr->width, &frame_hdr->height);

    parse_display_frame_size (br, frame_hdr);

  } else {
    frame_hdr->intra_only = frame_hdr->show_frame ? 0 : gst_vp9_read_bit (br);
    frame_hdr->reset_frame_context = frame_hdr->error_resilient_mode ?
        0 : gst_vp9_read_bits (br, 2);

    if (frame_hdr->intra_only) {

      if (!verify_sync_code (br)) {
        GST_ERROR ("Invalid VP9 sync code in intra-only frame !");
        goto error;
      }

      if (frame_hdr->profile > GST_VP9_PROFILE_0) {
        if (!parse_bitdepth_colorspace_sampling (parser, br, frame_hdr)) {
          GST_ERROR ("Failed to parse color_space/bit_depth info !");
          goto error;
        }
      } else {
        parser->color_space = GST_VP9_CS_BT_601;
        parser->color_range = GST_VP9_CR_LIMITED;
        parser->subsampling_y = parser->subsampling_x = 1;
        parser->bit_depth = GST_VP9_BIT_DEPTH_8;
      }

      frame_hdr->refresh_frame_flags =
          gst_vp9_read_bits (br, GST_VP9_REF_FRAMES);
      parse_frame_size (br, &frame_hdr->width, &frame_hdr->height);
      parse_display_frame_size (br, frame_hdr);

    } else {
      int i;
      frame_hdr->refresh_frame_flags =
          gst_vp9_read_bits (br, GST_VP9_REF_FRAMES);

      for (i = 0; i < GST_VP9_REFS_PER_FRAME; i++) {
        frame_hdr->ref_frame_indices[i] =
            gst_vp9_read_bits (br, GST_VP9_REF_FRAMES_LOG2);
        frame_hdr->ref_frame_sign_bias[i] = gst_vp9_read_bit (br);
      }

      parse_frame_size_from_refs (parser, frame_hdr, br);
      parse_display_frame_size (br, frame_hdr);

      frame_hdr->allow_high_precision_mv = gst_vp9_read_bit (br);
      frame_hdr->mcomp_filter_type = parse_interp_filter (br);
    }
  }

  frame_hdr->refresh_frame_context =
      frame_hdr->error_resilient_mode ? 0 : gst_vp9_read_bit (br);
  frame_hdr->frame_parallel_decoding_mode =
      frame_hdr->error_resilient_mode ? 1 : gst_vp9_read_bit (br);
  frame_hdr->frame_context_idx =
      gst_vp9_read_bits (br, GST_VP9_FRAME_CONTEXTS_LOG2);

  /* loopfilter header  */
  parse_loopfilter (&frame_hdr->loopfilter, br);

  /* quantization header */
  parse_quantization (&frame_hdr->quant_indices, br);
  /* set lossless_flag */
  frame_hdr->lossless_flag = frame_hdr->quant_indices.y_ac_qi == 0 &&
      frame_hdr->quant_indices.y_dc_delta == 0 &&
      frame_hdr->quant_indices.uv_dc_delta == 0
      && frame_hdr->quant_indices.uv_ac_delta == 0;

  /* segmentation header */
  parse_segmentation (&frame_hdr->segmentation, br);

  /* tile header */
  if (!parse_tile_info (frame_hdr, br)) {
    GST_ERROR ("Failed to parse tile info...!");
    goto error;
  }

  /* size of the rest of the header */
  frame_hdr->first_partition_size = gst_vp9_read_bits (br, 16);
  if (!frame_hdr->first_partition_size) {
    GST_ERROR ("Failed to parse the first partition size...!");
    goto error;
  }

  frame_hdr->frame_header_length_in_bytes =
      (gst_bit_reader_get_pos (br) + 7) / 8;
  return gst_vp9_parser_update (parser, frame_hdr);

error:
  return GST_VP9_PARSER_ERROR;
}

/**
 * gst_vp9_parser_parse_superframe_info:
 * @parser: The #GstVp9Parser
 * @superframe_info: The #GstVp9SuperframeInfo to fill
 * @data: The data to parse
 * @size: The size of the @data to parse
 *
 * Parses the VP9 bitstream contained in @data, and fills in @superframe_info
 * with the information. The @size argument represent the whole superframe size.
 * If @data is not superframe but normal frame, the parser returns
 * GST_VP9_PARSER_OK, frame_size[0] is set to @size and frames_in_superframe is
 * set to 1. Also this method does not validate vp9frame header and verifying
 * the frame header is caller's responsibility.
 *
 * Returns: a #GstVp9ParserResult
 *
 * Since: 1.18
 */
GstVp9ParserResult
gst_vp9_parser_parse_superframe_info (GstVp9Parser * parser,
    GstVp9SuperframeInfo * superframe_info, const guint8 * data, gsize size)
{
  GstBitReader header_bit_reader, index_bit_reader;
  GstBitReader *hbr = &header_bit_reader, *ibr = &index_bit_reader;
  guint i, j;

  g_return_val_if_fail (parser != NULL, GST_VP9_PARSER_ERROR);
  g_return_val_if_fail (superframe_info != NULL, GST_VP9_PARSER_ERROR);
  g_return_val_if_fail (data != NULL, GST_VP9_PARSER_ERROR);
  g_return_val_if_fail (size > 0, GST_VP9_PARSER_ERROR);

  gst_bit_reader_init (hbr, data + size - 1, 1);
  memset (superframe_info, 0, sizeof (GstVp9SuperframeInfo));

  /* Parsing Superframe Data Chunk */

  if (!verify_superframe_marker (hbr)) {
    superframe_info->frame_sizes[0] = size;
    superframe_info->frames_in_superframe = 1;
    return GST_VP9_PARSER_OK;
  }

  GST_DEBUG ("Parsing VP9 superframe, size %" G_GSIZE_FORMAT, size);

  superframe_info->bytes_per_framesize = gst_vp9_read_bits (hbr, 2) + 1;
  superframe_info->frames_in_superframe = gst_vp9_read_bits (hbr, 3) + 1;

  if (superframe_info->frames_in_superframe > GST_VP9_MAX_FRAMES_IN_SUPERFRAME)
    goto error;

  superframe_info->superframe_index_size =
      2 +
      superframe_info->frames_in_superframe *
      superframe_info->bytes_per_framesize;

  gst_bit_reader_init (ibr,
      data + size - superframe_info->superframe_index_size,
      superframe_info->superframe_index_size);

  /* Checking that the first byte of the superframe_index matches the final byte */
  if (gst_vp9_read_bits (ibr, 8) != data[size - 1])
    goto error;

  for (i = 0; i < superframe_info->frames_in_superframe; i++) {
    for (j = 0; j < superframe_info->bytes_per_framesize; j++)
      superframe_info->frame_sizes[i] |= gst_vp9_read_bits (ibr, 8) << (j * 8);
  }

  return GST_VP9_PARSER_OK;

error:
  GST_ERROR ("Failed to parse superframe");
  return GST_VP9_PARSER_ERROR;
}
