/* GStreamer
 * Copyright (C) 2023 Seungha Yang <seungha@centricular.com>
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

#pragma once

#ifdef G_OS_WIN32
#include <windows.h>
#else
typedef int INT;
typedef unsigned int UINT;
typedef unsigned int UINT32;
typedef unsigned char UCHAR;
typedef unsigned char BYTE;
typedef char CHAR;
typedef unsigned short UINT16;
typedef unsigned short USHORT;
typedef unsigned short WORD;
typedef unsigned long ULONG;
typedef unsigned long DWORD;
typedef short SHORT;
#endif

G_BEGIN_DECLS

#pragma pack(push, BeforeDXVApacking, 1)
typedef struct _DXVA_PicEntry_H264
{
  union
  {
    struct
    {
      UCHAR Index7Bits:7;
      UCHAR AssociatedFlag:1;
    };
    UCHAR bPicEntry;
  };
} DXVA_PicEntry_H264, *LPDXVA_PicEntry_H264;    /* 1 byte */

typedef struct _DXVA_PicParams_H264
{
  USHORT wFrameWidthInMbsMinus1;
  USHORT wFrameHeightInMbsMinus1;
  DXVA_PicEntry_H264 CurrPic;   /* flag is bot field flag */
  UCHAR num_ref_frames;

  union
  {
    struct
    {
      USHORT field_pic_flag:1;
      USHORT MbaffFrameFlag:1;
      USHORT residual_colour_transform_flag:1;
      USHORT sp_for_switch_flag:1;
      USHORT chroma_format_idc:2;
      USHORT RefPicFlag:1;
      USHORT constrained_intra_pred_flag:1;

      USHORT weighted_pred_flag:1;
      USHORT weighted_bipred_idc:2;
      USHORT MbsConsecutiveFlag:1;
      USHORT frame_mbs_only_flag:1;
      USHORT transform_8x8_mode_flag:1;
      USHORT MinLumaBipredSize8x8Flag:1;
      USHORT IntraPicFlag:1;
    };
    USHORT wBitFields;
  };
  UCHAR bit_depth_luma_minus8;
  UCHAR bit_depth_chroma_minus8;

  USHORT Reserved16Bits;
  UINT StatusReportFeedbackNumber;

  DXVA_PicEntry_H264 RefFrameList[16];  /* flag LT */
  INT CurrFieldOrderCnt[2];
  INT FieldOrderCntList[16][2];

  CHAR pic_init_qs_minus26;
  CHAR chroma_qp_index_offset;  /* also used for QScb */
  CHAR second_chroma_qp_index_offset;   /* also for QScr */
  UCHAR ContinuationFlag;

/* remainder for parsing */
  CHAR pic_init_qp_minus26;
  UCHAR num_ref_idx_l0_active_minus1;
  UCHAR num_ref_idx_l1_active_minus1;
  UCHAR Reserved8BitsA;

  USHORT FrameNumList[16];
  UINT UsedForReferenceFlags;
  USHORT NonExistingFrameFlags;
  USHORT frame_num;

  UCHAR log2_max_frame_num_minus4;
  UCHAR pic_order_cnt_type;
  UCHAR log2_max_pic_order_cnt_lsb_minus4;
  UCHAR delta_pic_order_always_zero_flag;

  UCHAR direct_8x8_inference_flag;
  UCHAR entropy_coding_mode_flag;
  UCHAR pic_order_present_flag;
  UCHAR num_slice_groups_minus1;

  UCHAR slice_group_map_type;
  UCHAR deblocking_filter_control_present_flag;
  UCHAR redundant_pic_cnt_present_flag;
  UCHAR Reserved8BitsB;

  USHORT slice_group_change_rate_minus1;

  UCHAR SliceGroupMap[810];     /* 4b/sgmu, Size BT.601 */

} DXVA_PicParams_H264, *LPDXVA_PicParams_H264;

typedef struct _DXVA_Qmatrix_H264
{
  UCHAR bScalingLists4x4[6][16];
  UCHAR bScalingLists8x8[2][64];

} DXVA_Qmatrix_H264, *LPDXVA_Qmatrix_H264;

typedef struct _DXVA_Slice_H264_Short
{
  UINT BSNALunitDataLocation;   /* type 1..5 */
  UINT SliceBytesInBuffer;      /* for off-host parse */
  USHORT wBadSliceChopping;     /* for off-host parse */
} DXVA_Slice_H264_Short, *LPDXVA_Slice_H264_Short;

typedef struct _DXVA_PicEntry_HEVC
{
  union
  {
    struct
    {
      UCHAR Index7Bits:7;
      UCHAR AssociatedFlag:1;
    };
    UCHAR bPicEntry;
  };
} DXVA_PicEntry_HEVC, *LPDXVA_PicEntry_HEVC;

typedef struct _DXVA_PicParams_HEVC
{
  USHORT PicWidthInMinCbsY;
  USHORT PicHeightInMinCbsY;
  union
  {
    struct
    {
      USHORT chroma_format_idc:2;
      USHORT separate_colour_plane_flag:1;
      USHORT bit_depth_luma_minus8:3;
      USHORT bit_depth_chroma_minus8:3;
      USHORT log2_max_pic_order_cnt_lsb_minus4:4;
      USHORT NoPicReorderingFlag:1;
      USHORT NoBiPredFlag:1;
      USHORT ReservedBits1:1;
    };
    USHORT wFormatAndSequenceInfoFlags;
  };
  DXVA_PicEntry_HEVC CurrPic;
  UCHAR sps_max_dec_pic_buffering_minus1;
  UCHAR log2_min_luma_coding_block_size_minus3;
  UCHAR log2_diff_max_min_luma_coding_block_size;
  UCHAR log2_min_transform_block_size_minus2;
  UCHAR log2_diff_max_min_transform_block_size;
  UCHAR max_transform_hierarchy_depth_inter;
  UCHAR max_transform_hierarchy_depth_intra;
  UCHAR num_short_term_ref_pic_sets;
  UCHAR num_long_term_ref_pics_sps;
  UCHAR num_ref_idx_l0_default_active_minus1;
  UCHAR num_ref_idx_l1_default_active_minus1;
  CHAR init_qp_minus26;
  UCHAR ucNumDeltaPocsOfRefRpsIdx;
  USHORT wNumBitsForShortTermRPSInSlice;
  USHORT ReservedBits2;

  union
  {
    struct
    {
      UINT32 scaling_list_enabled_flag:1;
      UINT32 amp_enabled_flag:1;
      UINT32 sample_adaptive_offset_enabled_flag:1;
      UINT32 pcm_enabled_flag:1;
      UINT32 pcm_sample_bit_depth_luma_minus1:4;
      UINT32 pcm_sample_bit_depth_chroma_minus1:4;
      UINT32 log2_min_pcm_luma_coding_block_size_minus3:2;
      UINT32 log2_diff_max_min_pcm_luma_coding_block_size:2;
      UINT32 pcm_loop_filter_disabled_flag:1;
      UINT32 long_term_ref_pics_present_flag:1;
      UINT32 sps_temporal_mvp_enabled_flag:1;
      UINT32 strong_intra_smoothing_enabled_flag:1;
      UINT32 dependent_slice_segments_enabled_flag:1;
      UINT32 output_flag_present_flag:1;
      UINT32 num_extra_slice_header_bits:3;
      UINT32 sign_data_hiding_enabled_flag:1;
      UINT32 cabac_init_present_flag:1;
      UINT32 ReservedBits3:5;
    };
    UINT32 dwCodingParamToolFlags;
  };

  union
  {
    struct
    {
      UINT32 constrained_intra_pred_flag:1;
      UINT32 transform_skip_enabled_flag:1;
      UINT32 cu_qp_delta_enabled_flag:1;
      UINT32 pps_slice_chroma_qp_offsets_present_flag:1;
      UINT32 weighted_pred_flag:1;
      UINT32 weighted_bipred_flag:1;
      UINT32 transquant_bypass_enabled_flag:1;
      UINT32 tiles_enabled_flag:1;
      UINT32 entropy_coding_sync_enabled_flag:1;
      UINT32 uniform_spacing_flag:1;
      UINT32 loop_filter_across_tiles_enabled_flag:1;
      UINT32 pps_loop_filter_across_slices_enabled_flag:1;
      UINT32 deblocking_filter_override_enabled_flag:1;
      UINT32 pps_deblocking_filter_disabled_flag:1;
      UINT32 lists_modification_present_flag:1;
      UINT32 slice_segment_header_extension_present_flag:1;
      UINT32 IrapPicFlag:1;
      UINT32 IdrPicFlag:1;
      UINT32 IntraPicFlag:1;
      UINT32 ReservedBits4:13;
    };
    UINT32 dwCodingSettingPicturePropertyFlags;
  };
  CHAR pps_cb_qp_offset;
  CHAR pps_cr_qp_offset;
  UCHAR num_tile_columns_minus1;
  UCHAR num_tile_rows_minus1;
  USHORT column_width_minus1[19];
  USHORT row_height_minus1[21];
  UCHAR diff_cu_qp_delta_depth;
  CHAR pps_beta_offset_div2;
  CHAR pps_tc_offset_div2;
  UCHAR log2_parallel_merge_level_minus2;
  INT CurrPicOrderCntVal;
  DXVA_PicEntry_HEVC RefPicList[15];
  UCHAR ReservedBits5;
  INT PicOrderCntValList[15];
  UCHAR RefPicSetStCurrBefore[8];
  UCHAR RefPicSetStCurrAfter[8];
  UCHAR RefPicSetLtCurr[8];
  USHORT ReservedBits6;
  USHORT ReservedBits7;
  UINT StatusReportFeedbackNumber;
} DXVA_PicParams_HEVC, *LPDXVA_PicParams_HEVC;

typedef struct _DXVA_Qmatrix_HEVC
{
  UCHAR ucScalingLists0[6][16];
  UCHAR ucScalingLists1[6][64];
  UCHAR ucScalingLists2[6][64];
  UCHAR ucScalingLists3[2][64];
  UCHAR ucScalingListDCCoefSizeID2[6];
  UCHAR ucScalingListDCCoefSizeID3[2];
} DXVA_Qmatrix_HEVC, *LPDXVA_Qmatrix_HEVC;

typedef struct _DXVA_Slice_HEVC_Short
{
  UINT BSNALunitDataLocation;
  UINT SliceBytesInBuffer;
  USHORT wBadSliceChopping;
} DXVA_Slice_HEVC_Short, *LPDXVA_Slice_HEVC_Short;

#define DXVA_USUAL_BLOCK_WIDTH   8
#define DXVA_USUAL_BLOCK_HEIGHT  8
#define DXVA_USUAL_BLOCK_SIZE   (DXVA_USUAL_BLOCK_WIDTH * DXVA_USUAL_BLOCK_HEIGHT)

typedef struct _DXVA_PictureParameters
{

  WORD wDecodedPictureIndex;
  WORD wDeblockedPictureIndex;

  WORD wForwardRefPictureIndex;
  WORD wBackwardRefPictureIndex;

  WORD wPicWidthInMBminus1;
  WORD wPicHeightInMBminus1;

  BYTE bMacroblockWidthMinus1;
  BYTE bMacroblockHeightMinus1;

  BYTE bBlockWidthMinus1;
  BYTE bBlockHeightMinus1;

  BYTE bBPPminus1;

  BYTE bPicStructure;
  BYTE bSecondField;
  BYTE bPicIntra;
  BYTE bPicBackwardPrediction;

  BYTE bBidirectionalAveragingMode;
  BYTE bMVprecisionAndChromaRelation;
  BYTE bChromaFormat;

  BYTE bPicScanFixed;
  BYTE bPicScanMethod;
  BYTE bPicReadbackRequests;

  BYTE bRcontrol;
  BYTE bPicSpatialResid8;
  BYTE bPicOverflowBlocks;
  BYTE bPicExtrapolation;

  BYTE bPicDeblocked;
  BYTE bPicDeblockConfined;
  BYTE bPic4MVallowed;
  BYTE bPicOBMC;
  BYTE bPicBinPB;
  BYTE bMV_RPS;

  BYTE bReservedBits;

  WORD wBitstreamFcodes;
  WORD wBitstreamPCEelements;
  BYTE bBitstreamConcealmentNeed;
  BYTE bBitstreamConcealmentMethod;

} DXVA_PictureParameters, *LPDXVA_PictureParameters;

typedef struct _DXVA_QmatrixData
{
  BYTE bNewQmatrix[4];          /* intra Y, inter Y, intra chroma, inter chroma */
  WORD Qmatrix[4][DXVA_USUAL_BLOCK_WIDTH * DXVA_USUAL_BLOCK_HEIGHT];
} DXVA_QmatrixData, *LPDXVA_QmatrixData;

typedef struct _DXVA_SliceInfo
{
  WORD wHorizontalPosition;
  WORD wVerticalPosition;
  DWORD dwSliceBitsInBuffer;
  DWORD dwSliceDataLocation;
  BYTE bStartCodeBitOffset;
  BYTE bReservedBits;
  WORD wMBbitOffset;
  WORD wNumberMBsInSlice;
  WORD wQuantizerScaleCode;
  WORD wBadSliceChopping;
} DXVA_SliceInfo, *LPDXVA_SliceInfo;

typedef struct _DXVA_PicEntry_VPx
{
  union
  {
    struct
    {
      UCHAR Index7Bits:7;
      UCHAR AssociatedFlag:1;
    };
    UCHAR bPicEntry;
  };
} DXVA_PicEntry_VPx, *LPDXVA_PicEntry_VPx;

typedef struct _segmentation_VP8
{
  union
  {
    struct
    {
      UCHAR segmentation_enabled:1;
      UCHAR update_mb_segmentation_map:1;
      UCHAR update_mb_segmentation_data:1;
      UCHAR mb_segement_abs_delta:1;
      UCHAR ReservedSegmentFlags4Bits:4;
    };
    UCHAR wSegmentFlags;
  };
  CHAR segment_feature_data[2][4];
  UCHAR mb_segment_tree_probs[3];
} DXVA_segmentation_VP8;

/* VP8 picture parameters structure */
typedef struct _DXVA_PicParams_VP8
{
  UINT first_part_size;
  UINT width;
  UINT height;
  DXVA_PicEntry_VPx CurrPic;
  union
  {
    struct
    {
      UCHAR frame_type:1;
      UCHAR version:3;
      UCHAR show_frame:1;
      UCHAR clamp_type:1;
      UCHAR ReservedFrameTag3Bits:2;
    };
    UCHAR wFrameTagFlags;
  };
  DXVA_segmentation_VP8 stVP8Segments;
  UCHAR filter_type;
  UCHAR filter_level;
  UCHAR sharpness_level;
  UCHAR mode_ref_lf_delta_enabled;
  UCHAR mode_ref_lf_delta_update;
  CHAR ref_lf_deltas[4];
  CHAR mode_lf_deltas[4];
  UCHAR log2_nbr_of_dct_partitions;
  UCHAR base_qindex;
  CHAR y1dc_delta_q;
  CHAR y2dc_delta_q;
  CHAR y2ac_delta_q;
  CHAR uvdc_delta_q;
  CHAR uvac_delta_q;
  DXVA_PicEntry_VPx alt_fb_idx;
  DXVA_PicEntry_VPx gld_fb_idx;
  DXVA_PicEntry_VPx lst_fb_idx;
  UCHAR ref_frame_sign_bias_golden;
  UCHAR ref_frame_sign_bias_altref;
  UCHAR refresh_entropy_probs;
  UCHAR vp8_coef_update_probs[4][8][3][11];
  UCHAR mb_no_coeff_skip;
  UCHAR prob_skip_false;
  UCHAR prob_intra;
  UCHAR prob_last;
  UCHAR prob_golden;
  UCHAR intra_16x16_prob[4];
  UCHAR intra_chroma_prob[3];
  UCHAR vp8_mv_update_probs[2][19];
  USHORT ReservedBits1;
  USHORT ReservedBits2;
  USHORT ReservedBits3;
  UINT StatusReportFeedbackNumber;
} DXVA_PicParams_VP8, *LPDXVA_PicParams_VP8;

typedef struct _DXVA_Slice_VPx_Short
{
  UINT BSNALunitDataLocation;
  UINT SliceBytesInBuffer;
  USHORT wBadSliceChopping;
} DXVA_Slice_VPx_Short, *LPDXVA_Slice_VPx_Short;

typedef struct _segmentation_VP9
{
  union
  {
    struct
    {
      UCHAR enabled:1;
      UCHAR update_map:1;
      UCHAR temporal_update:1;
      UCHAR abs_delta:1;
      UCHAR ReservedSegmentFlags4Bits:4;
    };
    UCHAR wSegmentInfoFlags;
  };
  UCHAR tree_probs[7];
  UCHAR pred_probs[3];
  SHORT feature_data[8][4];
  UCHAR feature_mask[8];
} DXVA_segmentation_VP9;

typedef struct _DXVA_PicParams_VP9
{
  DXVA_PicEntry_VPx CurrPic;
  UCHAR profile;
  union
  {
    struct
    {
      USHORT frame_type:1;
      USHORT show_frame:1;
      USHORT error_resilient_mode:1;
      USHORT subsampling_x:1;
      USHORT subsampling_y:1;
      USHORT extra_plane:1;
      USHORT refresh_frame_context:1;
      USHORT frame_parallel_decoding_mode:1;
      USHORT intra_only:1;
      USHORT frame_context_idx:2;
      USHORT reset_frame_context:2;
      USHORT allow_high_precision_mv:1;
      USHORT ReservedFormatInfo2Bits:2;
    };
    USHORT wFormatAndPictureInfoFlags;
  };
  UINT width;
  UINT height;
  UCHAR BitDepthMinus8Luma;
  UCHAR BitDepthMinus8Chroma;
  UCHAR interp_filter;
  UCHAR Reserved8Bits;
  DXVA_PicEntry_VPx ref_frame_map[8];
  UINT ref_frame_coded_width[8];
  UINT ref_frame_coded_height[8];
  DXVA_PicEntry_VPx frame_refs[3];
  CHAR ref_frame_sign_bias[4];
  CHAR filter_level;
  CHAR sharpness_level;
  union
  {
    struct
    {
      UCHAR mode_ref_delta_enabled:1;
      UCHAR mode_ref_delta_update:1;
      UCHAR use_prev_in_find_mv_refs:1;
      UCHAR ReservedControlInfo5Bits:5;
    };
    UCHAR wControlInfoFlags;
  };
  CHAR ref_deltas[4];
  CHAR mode_deltas[2];
  SHORT base_qindex;
  CHAR y_dc_delta_q;
  CHAR uv_dc_delta_q;
  CHAR uv_ac_delta_q;
  DXVA_segmentation_VP9 stVP9Segments;
  UCHAR log2_tile_cols;
  UCHAR log2_tile_rows;
  USHORT uncompressed_header_size_byte_aligned;
  USHORT first_partition_size;
  USHORT Reserved16Bits;
  UINT Reserved32Bits;
  UINT StatusReportFeedbackNumber;
} DXVA_PicParams_VP9, *LPDXVA_PicParams_VP9;

typedef struct _DXVA_PicEntry_AV1
{

  UINT width;
  UINT height;

  // Global motion parameters
  INT wmmat[6];
  union
  {
    struct
    {
      UCHAR wminvalid:1;
      UCHAR wmtype:2;
      UCHAR Reserved:5;
    };
    UCHAR GlobalMotionFlags;
  };

  UCHAR Index;
  UINT16 Reserved16Bits;

} DXVA_PicEntry_AV1, *LPDXVA_PicEntry_AV1;

typedef struct _DXVA_PicParams_AV1
{
  UINT width;
  UINT height;

  UINT max_width;
  UINT max_height;

  UCHAR CurrPicTextureIndex;
  UCHAR superres_denom;
  UCHAR bitdepth;
  UCHAR seq_profile;

  // Tiles:
  struct
  {
    UCHAR cols;
    UCHAR rows;
    USHORT context_update_id;
    USHORT widths[64];
    USHORT heights[64];
  } tiles;

  // Coding Tools
  union
  {
    struct
    {
      UINT use_128x128_superblock:1;
      UINT intra_edge_filter:1;
      UINT interintra_compound:1;
      UINT masked_compound:1;
      UINT warped_motion:1;
      UINT dual_filter:1;
      UINT jnt_comp:1;
      UINT screen_content_tools:1;
      UINT integer_mv:1;
      UINT cdef:1;
      UINT restoration:1;
      UINT film_grain:1;
      UINT intrabc:1;
      UINT high_precision_mv:1;
      UINT switchable_motion_mode:1;
      UINT filter_intra:1;
      UINT disable_frame_end_update_cdf:1;
      UINT disable_cdf_update:1;
      UINT reference_mode:1;
      UINT skip_mode:1;
      UINT reduced_tx_set:1;
      UINT superres:1;
      UINT tx_mode:2;
      UINT use_ref_frame_mvs:1;
      UINT enable_ref_frame_mvs:1;
      UINT reference_frame_update:1;
      UINT Reserved:5;
    };
    UINT32 CodingParamToolFlags;
  } coding;

  // Format & Picture Info flags
  union
  {
    struct
    {
      UCHAR frame_type:2;
      UCHAR show_frame:1;
      UCHAR showable_frame:1;
      UCHAR subsampling_x:1;
      UCHAR subsampling_y:1;
      UCHAR mono_chrome:1;
      UCHAR Reserved:1;
    };
    UCHAR FormatAndPictureInfoFlags;
  } format;

  // References
  UCHAR primary_ref_frame;
  UCHAR order_hint;
  UCHAR order_hint_bits;

  DXVA_PicEntry_AV1 frame_refs[7];
  UCHAR RefFrameMapTextureIndex[8];

  // Loop filter parameters
  struct
  {
    UCHAR filter_level[2];
    UCHAR filter_level_u;
    UCHAR filter_level_v;

    UCHAR sharpness_level;
    union
    {
      struct
      {
        UCHAR mode_ref_delta_enabled:1;
        UCHAR mode_ref_delta_update:1;
        UCHAR delta_lf_multi:1;
        UCHAR delta_lf_present:1;
        UCHAR Reserved:4;
      };
      UCHAR ControlFlags;
    };
    CHAR ref_deltas[8];
    CHAR mode_deltas[2];
    UCHAR delta_lf_res;
    UCHAR frame_restoration_type[3];
    USHORT log2_restoration_unit_size[3];
    UINT16 Reserved16Bits;
  } loop_filter;

  // Quantization
  struct
  {
    union
    {
      struct
      {
        UCHAR delta_q_present:1;
        UCHAR delta_q_res:2;
        UCHAR Reserved:5;
      };
      UCHAR ControlFlags;
    };

    UCHAR base_qindex;
    CHAR y_dc_delta_q;
    CHAR u_dc_delta_q;
    CHAR v_dc_delta_q;
    CHAR u_ac_delta_q;
    CHAR v_ac_delta_q;
    // using_qmatrix:
    UCHAR qm_y;
    UCHAR qm_u;
    UCHAR qm_v;
    UINT16 Reserved16Bits;
  } quantization;

  // Cdef parameters
  struct
  {
    union
    {
      struct
      {
        UCHAR damping:2;
        UCHAR bits:2;
        UCHAR Reserved:4;
      };
      UCHAR ControlFlags;
    };

    union
    {
      struct
      {
        UCHAR primary:6;
        UCHAR secondary:2;
      };
      UCHAR combined;
    } y_strengths[8];

    union
    {
      struct
      {
        UCHAR primary:6;
        UCHAR secondary:2;
      };
      UCHAR combined;
    } uv_strengths[8];

  } cdef;

  UCHAR interp_filter;

  // Segmentation
  struct
  {
    union
    {
      struct
      {
        UCHAR enabled:1;
        UCHAR update_map:1;
        UCHAR update_data:1;
        UCHAR temporal_update:1;
        UCHAR Reserved:4;
      };
      UCHAR ControlFlags;
    };
    UCHAR Reserved24Bits[3];

    union
    {
      struct
      {
        UCHAR alt_q:1;
        UCHAR alt_lf_y_v:1;
        UCHAR alt_lf_y_h:1;
        UCHAR alt_lf_u:1;
        UCHAR alt_lf_v:1;
        UCHAR ref_frame:1;
        UCHAR skip:1;
        UCHAR globalmv:1;
      };
      UCHAR mask;
    } feature_mask[8];

    SHORT feature_data[8][8];

  } segmentation;

  struct
  {
    union
    {
      struct
      {
        USHORT apply_grain:1;
        USHORT scaling_shift_minus8:2;
        USHORT chroma_scaling_from_luma:1;
        USHORT ar_coeff_lag:2;
        USHORT ar_coeff_shift_minus6:2;
        USHORT grain_scale_shift:2;
        USHORT overlap_flag:1;
        USHORT clip_to_restricted_range:1;
        USHORT matrix_coeff_is_identity:1;
        USHORT Reserved:3;
      };
      USHORT ControlFlags;
    };

    USHORT grain_seed;
    UCHAR scaling_points_y[14][2];
    UCHAR num_y_points;
    UCHAR scaling_points_cb[10][2];
    UCHAR num_cb_points;
    UCHAR scaling_points_cr[10][2];
    UCHAR num_cr_points;
    UCHAR ar_coeffs_y[24];
    UCHAR ar_coeffs_cb[25];
    UCHAR ar_coeffs_cr[25];
    UCHAR cb_mult;
    UCHAR cb_luma_mult;
    UCHAR cr_mult;
    UCHAR cr_luma_mult;
    UCHAR Reserved8Bits;
    SHORT cb_offset;
    SHORT cr_offset;
  } film_grain;

  UINT Reserved32Bits;
  UINT StatusReportFeedbackNumber;
} DXVA_PicParams_AV1, *LPDXVA_PicParams_AV1;

typedef struct _DXVA_Tile_AV1
{
  UINT DataOffset;
  UINT DataSize;
  USHORT row;
  USHORT column;
  UINT16 Reserved16Bits;
  UCHAR anchor_frame;
  UCHAR Reserved8Bits;
} DXVA_Tile_AV1, *LPDXVA_Tile_AV1;

#pragma pack(pop, BeforeDXVApacking)

G_END_DECLS
