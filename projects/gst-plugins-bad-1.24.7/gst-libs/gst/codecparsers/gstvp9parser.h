/*
 * gstvp9parser.h
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

#ifndef GST_VP9_PARSER_H
#define GST_VP9_PARSER_H

#ifndef GST_USE_UNSTABLE_API
#warning "The VP9 parsing library is unstable API and may change in future."
#warning "You can define GST_USE_UNSTABLE_API to avoid this warning."
#endif

#include <gst/gst.h>
#include <gst/codecparsers/codecparsers-prelude.h>

G_BEGIN_DECLS

#define GST_VP9_FRAME_MARKER 0x02
#define GST_VP9_SYNC_CODE 0x498342
#define GST_VP9_SUPERFRAME_MARKER 0x06

#define GST_VP9_MAX_LOOP_FILTER    63
#define GST_VP9_MAX_PROB           255

#define GST_VP9_REFS_PER_FRAME     3
#define GST_VP9_REF_FRAMES_LOG2    3
#define GST_VP9_REF_FRAMES         (1 << GST_VP9_REF_FRAMES_LOG2)

#define GST_VP9_FRAME_CONTEXTS_LOG2 2

#define GST_VP9_MAX_SHARPNESS      7

#define GST_VP9_MAX_REF_LF_DELTAS  4
#define GST_VP9_MAX_MODE_LF_DELTAS 2

#define GST_VP9_SEGMENT_DELTADATA  0
#define GST_VP9_SEGMENT_ABSDATA    1

#define GST_VP9_MAX_SEGMENTS       8
#define GST_VP9_SEG_TREE_PROBS     (GST_VP9_MAX_SEGMENTS-1)

#define GST_VP9_PREDICTION_PROBS   3

#define GST_VP9_MAX_FRAMES_IN_SUPERFRAME 8

typedef struct _GstVp9Parser               GstVp9Parser;
typedef struct _GstVp9FrameHdr             GstVp9FrameHdr;
typedef struct _GstVp9LoopFilter           GstVp9LoopFilter;
typedef struct _GstVp9QuantIndices         GstVp9QuantIndices;
typedef struct _GstVp9Segmentation         GstVp9Segmentation;
typedef struct _GstVp9SegmentationInfo     GstVp9SegmentationInfo;
typedef struct _GstVp9SegmentationInfoData GstVp9SegmentationInfoData;
typedef struct _GstVp9SuperframeInfo       GstVp9SuperframeInfo;

/**
 * GstVp9ParseResult:
 * @GST_VP9_PARSER_OK: The parsing went well
 * @GST_VP9_PARSER_BROKEN_DATA: The data to parse is broken
 * @GST_VP9_PARSER_NO_PACKET_ERROR: An error occurred during the parsing
 *
 * Result type of any parsing function.
 *
 * Since: 1.8
 */
typedef enum
{
  GST_VP9_PARSER_OK,
  GST_VP9_PARSER_BROKEN_DATA,
  GST_VP9_PARSER_ERROR,
} GstVp9ParserResult;

/**
 * GstVp9Profile: Bitstream profiles indicated by 2-3 bits in the uncompressed header
 * @GST_VP9_PROFILE_0: Profile 0, 8-bit 4:2:0 only.
 * @GST_VP9_PROFILE_1: Profile 1, 8-bit 4:4:4, 4:2:2, and 4:4:0.
 * @GST_VP9_PROFILE_2: Profile 2, 10-bit and 12-bit color only, with 4:2:0 sampling.
 * @GST_VP9_PROFILE_3: Profile 3, 10-bit and 12-bit color only, with 4:2:2/4:4:4/4:4:0 sampling.
 * @GST_VP9_PROFILE_UNDEFINED: Undefined profile
 *
 * VP9 Profiles
 *
 * Since: 1.8
 */
typedef enum {
  GST_VP9_PROFILE_0,
  GST_VP9_PROFILE_1,
  GST_VP9_PROFILE_2,
  GST_VP9_PROFILE_3,
  GST_VP9_PROFILE_UNDEFINED
} GstVP9Profile;

/**
 * GstVp9FrameType:
 * @GST_VP9_KEY_FRAME: Key frame, only have intra blocks
 * @GST_VP9_INTER_FRAME: Inter frame, both intra and inter blocks
 *
 * VP9 frame types
 *
 * Since: 1.8
 */
typedef enum {
  GST_VP9_KEY_FRAME   = 0,
  GST_VP9_INTER_FRAME = 1
} GstVp9FrameType;

/**
 * GstVp9BitDepth:
 * @GST_VP9_BIT_DEPTH_8: Bit depth is 8
 * @GST_VP9_BIT_DEPTH_10 Bit depth is 10
 * @GST_VP9_BIT_DEPTH_12:Bit depth is 12
 *
 * Bit depths of encoded frames
 *
 * Since: 1.8
 */
typedef enum {
  GST_VP9_BIT_DEPTH_8  = 8,
  GST_VP9_BIT_DEPTH_10 = 10,
  GST_VP9_BIT_DEPTH_12 = 12
} GstVp9BitDepth;

/**
 * GstVp9ColorSpace:
 * @GST_VP9_CS_UNKNOWN: Unknown color space
 * @GST_VP9_CS_BT_601: BT.601
 * @GST_VP9_CS_BT_709: BT.709
 * @GST_VP9_CS_SMPTE_170: SMPTE.170
 * @GST_VP9_CS_SMPTE_240: SMPTE.240
 * @GST_VP9_CS_BT_2020: BT.2020
 * @GST_VP9_CS_RESERVED: Reserved
 * @GST_VP9_CS_SRGB: sRGB
 *
 * Supported ColorSpace standards
 *
 * Since: 1.8
 */
typedef enum {
  GST_VP9_CS_UNKNOWN               = 0,
  GST_VP9_CS_BT_601                = 1,
  GST_VP9_CS_BT_709                = 2,
  GST_VP9_CS_SMPTE_170             = 3,
  GST_VP9_CS_SMPTE_240             = 4,
  GST_VP9_CS_BT_2020               = 5,
  GST_VP9_CS_RESERVED_2            = 6,
  GST_VP9_CS_SRGB                  = 7
} GstVp9ColorSpace;

/**
 * GstVp9ColorRange:
 * @GST_VP9_CR_LIMITED: Y range is [16-235], UV range is [16-240]
 * @GST_VP9_CR_FULL: Full range for Y,U and V [0-255]
 *
 * Possible color value ranges
 *
 * Since: 1.8
 */
typedef enum {
  GST_VP9_CR_LIMITED,
  GST_VP9_CR_FULL
} GstVp9ColorRange;

/**
 * GstVp9InterpolationFilter:
 * @GST_VP9_INTERPOLATION_FILTER_EIGHTTAP: EightTap interpolation filter
 * @GST_VP9_INTERPOLATION_FILTER_EIGHTTAP_SMOOTH: Smooth interpolation filter
 * @GST_VP9_INTERPOLATION_FILTER_EIGHTTAP_SHARP: Shart interpolation filter
 * @GST_VP9_INTERPOLATION_FILTER_BILINEAR: Bilinear interpolation filter
 * @GST_VP9_INTERPOLATION_FILTER_SWITCHABLE: Selectable interpolation filter
 *
 * Interpolation Filters Types
 *
 * Since: 1.8
 */
typedef enum {
  GST_VP9_INTERPOLATION_FILTER_EIGHTTAP        = 0,
  GST_VP9_INTERPOLATION_FILTER_EIGHTTAP_SMOOTH = 1,
  GST_VP9_INTERPOLATION_FILTER_EIGHTTAP_SHARP  = 2,
  GST_VP9_INTERPOLATION_FILTER_BILINEAR        = 3,
  GST_VP9_INTERPOLATION_FILTER_SWITCHABLE      = 4
} GstVp9InterpolationFilter;

/**
 * GstVp9RefFrameType:
 * @GST_VP9_REF_FRAME_INTRA: Intra reference frame
 * @GST_VP9_REF_FRAME_LAST: Last Reference frame
 * @GST_VP9_REF_FRAME_GOLDEN: Golden Reference frame
 * @GST_VP9_REF_FRAME_ALTREF: Alternate Reference frame
 * @GST_VP9_REF_FRAME_MAX:
 *
 * Reference Frame types
 *
 * Since: 1.8
 */
typedef enum {
  GST_VP9_REF_FRAME_INTRA  = 0,
  GST_VP9_REF_FRAME_LAST   = 1,
  GST_VP9_REF_FRAME_GOLDEN = 2,
  GST_VP9_REF_FRAME_ALTREF = 3,
  GST_VP9_REF_FRAME_MAX    = 4
} GstVp9RefFrameType;

/**
 * GstVp9QuantIndices:
 * @y_ac_qi: indicates the dequantization table index used for the
 *   luma AC coefficients
 * @y_dc_delta: indicates the delta value that is added to the
 *   baseline index to obtain the luma DC coefficient dequantization
 *   index
 * @uv_dc_delta: indicates the delta value that is added to the
 *   baseline index to obtain the chroma DC coefficient dequantization
 *   index
 * @uv_ac_delta: indicates the delta value that is added to the
 *   baseline index to obtain the chroma AC coefficient dequantization
 *   index
 *
 * Dequantization indices.
 *
 * Since: 1.8
 */
struct _GstVp9QuantIndices
{
  guint8 y_ac_qi;
  gint8 y_dc_delta;
  gint8 uv_dc_delta;
  gint8 uv_ac_delta;
};

/**
 * GstVp9LoopFilter:
 * @filter_level: indicates loop filter level for the current frame
 * @sharpness_level: indicates sharpness level for thecurrent frame
 * @mode_ref_delta_enabled: indicate if filter adjust is on
 * @mode_ref_delta_update: indicates if the delta values used in an
 *   adjustment are updated in the current frame
 * @update_ref_deltas: indicate which ref deltas are updated
 * @ref_deltas:  Loop filter strength adjustments based on
 *  frame type (intra, inter)
 * @update_mode_deltas: indicate with mode deltas are updated
 * @mode_deltas: Loop filter strength adjustments based on
 *   mode (zero, new mv)
 *
 * Loop filter values
 *
 * Since: 1.8
 */
struct _GstVp9LoopFilter {
  gint filter_level;
  gint sharpness_level;

  guint8 mode_ref_delta_enabled;
  guint8 mode_ref_delta_update;
  guint8 update_ref_deltas[GST_VP9_MAX_REF_LF_DELTAS];
  gint8 ref_deltas[GST_VP9_MAX_REF_LF_DELTAS];
  guint8 update_mode_deltas[GST_VP9_MAX_MODE_LF_DELTAS];
  gint8 mode_deltas[GST_VP9_MAX_MODE_LF_DELTAS];
};

/**
 * GstVp9SegmentationInfoData:
 * @alternate_quantizer_enabled: indicate alternate quantizer enabled at segment level
 * @alternate_quantizer: alternate quantizer value
 * @alternate_loop_filter_enabled: indicate alternate loop filter enabled at segment level
 * @alternate_loop_filter: alternate loop filter
 * @reference_frame_enabled: indicate alternate reference frame at segment level
 * @reference_frame: alternate reference frame
 * @reference_skip: a block skip mode that implies both the use of a (0,0)
 *   motion vector and that no residual will be coded.
 *
 * Segmentation info for each segment
 *
 * Since: 1.8
 */
struct _GstVp9SegmentationInfoData {
  /* SEG_LVL_ALT_Q */
  guint8 alternate_quantizer_enabled;
  gint16 alternate_quantizer;

  /* SEG_LVL_ALT_LF */
  guint8 alternate_loop_filter_enabled;
  gint8 alternate_loop_filter;

  /* SEG_LVL_REF_FRAME */
  guint8 reference_frame_enabled;
  gint reference_frame;

  guint8 reference_skip;
};

/**
 * GstVp9SegmentationInfo:
 * @enabled: enables the segmentation feature for the current frame
 * @update_map: determines if segmentation is updated in the current frame
 * @update_tree_probs: determines if tree probabilities updated or not
 * @tree_probs: segment tree probabilities
 * @update_pred_probs:determines if prediction probabilities updated or not
 * @pred_probs: prediction probabilities
 * @abs_delta: interpretation of segment data values
 * @temporal_update: type of map update
 * @update_data: indicates if the segment feature data
 *   is updated in the current frame
 * @data: segment feature data
 *
 * Segmentation info
 *
 * Since: 1.8
 */
struct _GstVp9SegmentationInfo {
  /* enable in setup_segmentation*/
  guint8  enabled;
  /* update_map in setup_segmentation*/
  guint8 update_map;
  /* tree_probs exist or not*/
  guint8 update_tree_probs[GST_VP9_SEG_TREE_PROBS];
  guint8 tree_probs[GST_VP9_SEG_TREE_PROBS];
  /* pred_probs exist or not*/
  guint8 update_pred_probs[GST_VP9_PREDICTION_PROBS];
  guint8 pred_probs[GST_VP9_PREDICTION_PROBS];

  /* abs_delta in setup_segmentation */
  guint8 abs_delta;
  /* temporal_update in setup_segmentation */
  guint8 temporal_update;

  /* update_data in setup_segmentation*/
  guint8 update_data;
  GstVp9SegmentationInfoData data[GST_VP9_MAX_SEGMENTS];
};

/**
 * GstVp9FrameHdr:
 * @profile: encoded profile
 * @show_existing_frame: display already decoded frame instead of doing the decoding
 * @frame_to_show: which frame to show if show_existing_frame is true
 * @frame_type: frame type
 * @show_frame: indicate whether it is a displayable frame or not
 * @error_resilient_mode: error resilent mode
 * @width: frame width
 * @height: frame height
 * @display_size_enabled: display size enabled (cropping)
 * @display_width: display width
 * @display_height: display height
 * @frame_context_idx: frame context index
 * @intra_only: intra only frame
 * @reset_frame_context: reset frame context
 * @refresh_frame_flags: refresh reference frame flags
 * @ref_frame_indices: reference frame index
 * @ref_frame_sign_bias: sign bias for selecting altref,last and golden frames
 * @allow_high_precision_mv: allow hight precision motion vector
 * @mcomp_filter_type: interpolation filter type
 * @refresh_frame_context: refresh frame context indicator
 * @frame_parallel_decoding_mode: enable or disable parallel decoding support.
 * @loopfilter: loopfilter values
 * @quant_indices: quantization indices
 * @segmentation: segmentation info
 * @log2_tile_rows: tile row indicator
 * @log2_tile_columns:  tile column indicator
 * @first_partition_size: first partition size (after the uncompressed header)
 * @lossless_flag: lossless mode decode
 * @frame_header_length_in_bytes: length of uncompressed header
 *
 * Frame header
 *
 * Since: 1.8
 */
struct _GstVp9FrameHdr
{
  guint profile;
  guint8 show_existing_frame;
  gint  frame_to_show;
  guint frame_type;
  guint8 show_frame;
  guint8 error_resilient_mode;
  guint32 width;
  guint32 height;
  guint8 display_size_enabled;
  guint32 display_width;
  guint32 display_height;
  guint frame_context_idx;

  guint8 intra_only;
  gint reset_frame_context;
  gint refresh_frame_flags;

  gint ref_frame_indices[GST_VP9_REFS_PER_FRAME];
  gint ref_frame_sign_bias[GST_VP9_REFS_PER_FRAME];
  gint allow_high_precision_mv;
  guint8 mcomp_filter_type;

  gint refresh_frame_context;
  /* frame_parallel_decoding_mode in vp9 code*/
  gint frame_parallel_decoding_mode;

  GstVp9LoopFilter loopfilter;
  GstVp9QuantIndices quant_indices;
  GstVp9SegmentationInfo segmentation;

  gint log2_tile_rows;
  gint log2_tile_columns;

  guint32 first_partition_size;

  /* calculated values */
  guint lossless_flag;
  guint32 frame_header_length_in_bytes;
};

/**
 * GstVp9SuperframeInfo:
 * @bytes_per_framesize: indicates the number of bytes needed to code each frame size
 * @frames_in_superframe: indicates the number of frames within this superframe
 * @frame_sizes: specifies the size in bytes of frame number i (zero indexed) within this superframe
 * @superframe_index_size: indicates the total size of the superframe_index
 *
 * Superframe info
 *
 * Since: 1.18
 */
struct _GstVp9SuperframeInfo {
  guint32 bytes_per_framesize;
  guint32 frames_in_superframe;
  guint32 frame_sizes[GST_VP9_MAX_FRAMES_IN_SUPERFRAME];
  guint32 superframe_index_size;
};

/**
 * GstVp9Segmentation:
 * @filter_level: loop filter level
 * @luma_ac_quant_scale: AC quant scale for luma(Y) component
 * @luma_dc_quant_scale: DC quant scale for luma(Y) component
 * @chroma_ac_quant_scale AC quant scale for chroma(U/V) component
 * @chroma_dc_quant_scale: DC quant scale for chroma (U/V) component
 * @reference_frame_enabled: alternate reference frame enablement
 * @reference_frame: alternate reference frame
 * @reference_skip:  a block skip mode that implies both the use of a (0,0)
 *   motion vector and that no residual will be coded
 *
 * Segmentation info kept across multiple frames
 *
 * Since: 1.8
 */
struct _GstVp9Segmentation
{
  guint8 filter_level[GST_VP9_MAX_REF_LF_DELTAS][GST_VP9_MAX_MODE_LF_DELTAS];
  gint16 luma_ac_quant_scale;
  gint16 luma_dc_quant_scale;
  gint16 chroma_ac_quant_scale;
  gint16 chroma_dc_quant_scale;

  guint8 reference_frame_enabled;
  gint reference_frame;

  guint8 reference_skip;
};

/**
 * GstVp9Parser:
 * @priv: GstVp9ParserPrivate struct to keep track of state variables
 * @subsampling_x: horizontal subsampling
 * @subsampling_y: vertical subsampling
 * @bit_depth: bit depth of the stream
 * @color_space: color space standard
 * @color_range: color range standard
 * @mb_segment_tree_probs: decoding tree probabilities
 * @segment_pred_probs: segment prediction probabiilties
 * @segmentation: Segmentation info
 *
 * Parser context that needs to be live across frames
 *
 * Since: 1.8
 */
struct _GstVp9Parser
{
#ifndef GST_REMOVE_DEPRECATED
  void *priv;          /* unused, kept around for ABI compatibility */
#endif

  gint subsampling_x;
  gint subsampling_y;
  guint bit_depth;
  guint color_space;
  guint color_range;

  guint8 mb_segment_tree_probs[GST_VP9_SEG_TREE_PROBS];
  guint8 segment_pred_probs[GST_VP9_PREDICTION_PROBS];
  GstVp9Segmentation segmentation[GST_VP9_MAX_SEGMENTS];
};

GST_CODEC_PARSERS_API
GstVp9Parser *     gst_vp9_parser_new (void);

GST_CODEC_PARSERS_API
GstVp9ParserResult gst_vp9_parser_parse_frame_header (GstVp9Parser* parser, GstVp9FrameHdr * frame_hdr, const guint8 * data, gsize size);

GST_CODEC_PARSERS_API
GstVp9ParserResult gst_vp9_parser_parse_superframe_info (GstVp9Parser* parser, GstVp9SuperframeInfo * superframe_info, const guint8 * data, gsize size);

GST_CODEC_PARSERS_API
void               gst_vp9_parser_free (GstVp9Parser * parser);

G_END_DECLS

#endif /* GST_VP9_PARSER_H */
